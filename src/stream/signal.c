/**
* @file src/stream/signal.c
* @brief Implementation of signal stream functionality
*
* This file provides the implementation of signal handling operations for the SIO library.
* Signal streams provide a way to wait for and handle signals in a stream-based API,
* allowing for easier integration with other I/O operations.
*
* @author zczxy
* @version 0.1.0
*/

#include <sio/stream.h>
#include <sio/err.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>

#if defined(SIO_OS_WINDOWS)
  #include <windows.h>
#else
  #include <signal.h>
  #include <unistd.h>
  #include <fcntl.h>
  #include <errno.h>
  #include <sys/signalfd.h>
#endif

/* Forward declarations of signal stream operations */
static sio_error_t signal_close(sio_stream_t *stream);
static sio_error_t signal_read(sio_stream_t *stream, void *buffer, size_t size, size_t *bytes_read, int flags);
static sio_error_t signal_write(sio_stream_t *stream, const void *buffer, size_t size, size_t *bytes_written, int flags);
static sio_error_t signal_get_option(sio_stream_t *stream, sio_stream_option_t option, void *value, size_t *size);
static sio_error_t signal_set_option(sio_stream_t *stream, sio_stream_option_t option, const void *value, size_t size);

/* Signal stream operations vtable */
static const sio_stream_ops_t signal_ops = {
  .close = signal_close,
  .read = signal_read,
  .write = signal_write,
  .readv = NULL, /* Will use fallback in stream.c */
  .writev = NULL, /* Will use fallback in stream.c */
  .flush = NULL, /* No flush needed for signals */
  .get_option = signal_get_option,
  .set_option = signal_set_option,
  .seek = NULL, /* Signals are not seekable */
  .tell = NULL, /* Signals don't have a position */
  .truncate = NULL, /* Signals can't be truncated */
  .get_size = NULL /* Signals don't have a size */
};

#if defined(SIO_OS_WINDOWS)
/* Windows-specific signal event mapping */
typedef struct {
  int signal;
  HANDLE event;
} win_signal_event_t;

/* Map of signal numbers to event handles */
static win_signal_event_t *g_win_signal_events = NULL;
static size_t g_win_signal_event_count = 0;

/* Signal handler for Windows */
static BOOL WINAPI win_signal_handler(DWORD signal_type) {
  /* Find the corresponding event */
  for (size_t i = 0; i < g_win_signal_event_count; i++) {
    if (g_win_signal_events[i].signal == (int)signal_type) {
      SetEvent(g_win_signal_events[i].event);
      return TRUE;
    }
  }
  
  /* Not handled, let Windows process it */
  return FALSE;
}

/* Initialize Windows signal handling */
static sio_error_t win_init_signals(const int *signals, size_t signal_count, HANDLE *event_handle) {
  /* Allocate signal event map if not already done */
  if (!g_win_signal_events) {
    g_win_signal_events = (win_signal_event_t*)malloc(sizeof(win_signal_event_t) * signal_count);
    if (!g_win_signal_events) {
      return SIO_ERROR_MEM;
    }
    g_win_signal_event_count = 0;
  } else {
    /* Resize the map */
    win_signal_event_t *new_map = (win_signal_event_t*)realloc(g_win_signal_events, 
                                                            sizeof(win_signal_event_t) * (g_win_signal_event_count + signal_count));
    if (!new_map) {
      return SIO_ERROR_MEM;
    }
    g_win_signal_events = new_map;
  }
  
  /* Create an event for all signals */
  HANDLE event = CreateEvent(NULL, TRUE, FALSE, NULL);
  if (event == NULL) {
    return sio_get_last_error();
  }
  
  *event_handle = event;
  
  /* Register handlers for each signal */
  for (size_t i = 0; i < signal_count; i++) {
    int sig = signals[i];
    
    /* Map signal number to Windows control handler */
    DWORD signal_type;
    switch (sig) {
      case SIGINT:
        signal_type = CTRL_C_EVENT;
        break;
      case SIGBREAK:
        signal_type = CTRL_BREAK_EVENT;
        break;
      case SIGTERM:
        signal_type = CTRL_CLOSE_EVENT;
        break;
      default:
        /* Unsupported signal */
        CloseHandle(event);
        return SIO_ERROR_UNSUPPORTED;
    }
    
    /* Register the handler if not already done */
    static BOOL handler_registered = FALSE;
    if (!handler_registered) {
      if (!SetConsoleCtrlHandler(win_signal_handler, TRUE)) {
        CloseHandle(event);
        return sio_get_last_error();
      }
      handler_registered = TRUE;
    }
    
    /* Add to map */
    g_win_signal_events[g_win_signal_event_count].signal = sig;
    g_win_signal_events[g_win_signal_event_count].event = event;
    g_win_signal_event_count++;
  }
  
  return SIO_SUCCESS;
}
#endif

/**
* @brief Create a signal stream
* 
* @param stream Pointer to stream structure to initialize
* @param signals Array of signal numbers
* @param signal_count Number of signals in the array
* @param opt Combination of SIO_STREAM_* flags
* @return sio_error_t SIO_SUCCESS or error code
*/
sio_error_t sio_stream_open_signal(sio_stream_t *stream, const int *signals, size_t signal_count, sio_stream_flags_t opt) {
  if (!stream || !signals || signal_count == 0) {
    return SIO_ERROR_PARAM;
  }
  
  /* Initialize the stream structure */
  memset(stream, 0, sizeof(sio_stream_t));
  stream->type = SIO_STREAM_SIGNAL;
  stream->flags = opt;
  stream->ops = &signal_ops;
  
#if defined(SIO_OS_WINDOWS)
  /* Windows implementation using events */
  HANDLE event;
  sio_error_t err = win_init_signals(signals, signal_count, &event);
  if (err != SIO_SUCCESS) {
    return err;
  }
  
  /* Store the event handle */
  stream->data.signal.event = event;
#else
  /* POSIX implementation using signalfd */
  
  /* Initialize the signal mask */
  sigset_t mask;
  sigemptyset(&mask);
  
  for (size_t i = 0; i < signal_count; i++) {
    sigaddset(&mask, signals[i]);
  }
  
  /* Block the signals so they will be delivered to signalfd */
  if (sigprocmask(SIG_BLOCK, &mask, NULL) < 0) {
    return sio_get_last_error();
  }
  
  /* Create the signalfd */
  int fd = signalfd(-1, &mask, SFD_NONBLOCK | SFD_CLOEXEC);
  if (fd < 0) {
    sigprocmask(SIG_UNBLOCK, &mask, NULL); /* Unblock the signals */
    return sio_get_last_error();
  }
  
  /* Store the file descriptor and signal mask */
  stream->data.signal.fd = fd;
  stream->data.signal.mask = mask;
#endif
  
  return SIO_SUCCESS;
}

/**
* @brief Open a signal stream from an existing handle
*/
sio_error_t sio_stream_open_signal_from_handle(sio_stream_t *stream, void *handle, sio_stream_flags_t opt) {
  if (!stream || !handle) {
    return SIO_ERROR_PARAM;
  }
  
  /* Initialize the stream structure */
  memset(stream, 0, sizeof(sio_stream_t));
  stream->type = SIO_STREAM_SIGNAL;
  stream->flags = opt;
  stream->ops = &signal_ops;
  
#if defined(SIO_OS_WINDOWS)
  stream->data.signal.event = (HANDLE)handle;
#else
  stream->data.signal.fd = (int)(intptr_t)handle;
  /* We don't know the signal mask in this case */
  sigemptyset(&stream->data.signal.mask);
#endif
  
  return SIO_SUCCESS;
}

/**
* @brief Close a signal stream
*/
static sio_error_t signal_close(sio_stream_t *stream) {
  assert(stream && stream->type == SIO_STREAM_SIGNAL);
  
#if defined(SIO_OS_WINDOWS)
  /* Close the event handle */
  if (stream->data.signal.event && stream->data.signal.event != INVALID_HANDLE_VALUE) {
    /* Remove from the global map */
    for (size_t i = 0; i < g_win_signal_event_count; i++) {
      if (g_win_signal_events[i].event == stream->data.signal.event) {
        /* Move last element to this position */
        if (i < g_win_signal_event_count - 1) {
          g_win_signal_events[i] = g_win_signal_events[g_win_signal_event_count - 1];
        }
        g_win_signal_event_count--;
        break;
      }
    }
    
    /* Close the handle */
    if (!CloseHandle(stream->data.signal.event)) {
      return sio_get_last_error();
    }
    stream->data.signal.event = INVALID_HANDLE_VALUE;
  }
#else
  /* Close the signal file descriptor */
  if (stream->data.signal.fd >= 0) {
    /* Unblock the signals */
    sigprocmask(SIG_UNBLOCK, &stream->data.signal.mask, NULL);
    
    /* Close the file descriptor */
    if (close(stream->data.signal.fd) < 0) {
      return sio_get_last_error();
    }
    stream->data.signal.fd = -1;
  }
#endif
  
  return SIO_SUCCESS;
}

/**
* @brief Read from a signal stream (wait for signals)
*/
static sio_error_t signal_read(sio_stream_t *stream, void *buffer, size_t size, size_t *bytes_read, int flags) {
  assert(stream && stream->type == SIO_STREAM_SIGNAL);
  
  /* Initialize bytes_read if provided */
  if (bytes_read) {
    *bytes_read = 0;
  }
  
  /* Check if stream is readable */
  if (!(stream->flags & SIO_STREAM_READ)) {
    return SIO_ERROR_PERM;
  }
  
#if defined(SIO_OS_WINDOWS)
  /* Wait for the event */
  DWORD wait_ms;
  
  if (flags & SIO_MSG_DONTWAIT) {
    wait_ms = 0; /* Don't wait */
  } else {
    wait_ms = INFINITE; /* Wait indefinitely */
  }
  
  DWORD result = WaitForSingleObject(stream->data.signal.event, wait_ms);
  
  if (result == WAIT_OBJECT_0) {
    /* Signal received */
    if (buffer && size >= sizeof(int)) {
      /* The signal number isn't available in Windows */
      *((int*)buffer) = 0;
      
      if (bytes_read) {
        *bytes_read = sizeof(int);
      }
    }
    
    /* Reset the event for next time */
    ResetEvent(stream->data.signal.event);
    
    return SIO_SUCCESS;
  } else if (result == WAIT_TIMEOUT) {
    /* No signal yet */
    return SIO_ERROR_WOULDBLOCK;
  } else {
    /* Error */
    return sio_get_last_error();
  }
#else
  /* Read the signal info */
  struct signalfd_siginfo siginfo;
  
  int recv_flags = 0;
  /* Convert SIO flags to native flags */
  if (flags & SIO_MSG_DONTWAIT) {
    recv_flags |= MSG_DONTWAIT;
  }
  
  ssize_t result;
  
  if (flags & SIO_MSG_DONTWAIT) {
    /* Non-blocking read */
    result = read(stream->data.signal.fd, &siginfo, sizeof(siginfo));
    
    if (result < 0) {
      if (errno == EAGAIN || errno == EWOULDBLOCK) {
        /* No signal yet */
        return SIO_ERROR_WOULDBLOCK;
      }
      return sio_get_last_error();
    }
  } else {
    /* Blocking read - need to poll or select since signalfd might be in non-blocking mode */
    fd_set readfds;
    int maxfd = stream->data.signal.fd;
    
    while (1) {
      FD_ZERO(&readfds);
      FD_SET(stream->data.signal.fd, &readfds);
      
      /* Wait for the signal fd to become readable */
      int select_result = select(maxfd + 1, &readfds, NULL, NULL, NULL);
      
      if (select_result < 0) {
        if (errno == EINTR) {
          /* Interrupted, try again */
          continue;
        }
        return sio_get_last_error();
      }
      
      if (FD_ISSET(stream->data.signal.fd, &readfds)) {
        /* Signal fd is readable, try to read from it */
        result = read(stream->data.signal.fd, &siginfo, sizeof(siginfo));
        
        if (result < 0) {
          if (errno == EAGAIN || errno == EWOULDBLOCK) {
            /* Spurious wakeup, try again */
            continue;
          }
          return sio_get_last_error();
        }
        
        /* Successfully read a signal */
        break;
      }
    }
  }
  
  /* Return the signal info if buffer is provided */
  if (buffer) {
    if (size >= sizeof(struct signalfd_siginfo)) {
      memcpy(buffer, &siginfo, sizeof(struct signalfd_siginfo));
      
      if (bytes_read) {
        *bytes_read = sizeof(struct signalfd_siginfo);
      }
    } else if (size >= sizeof(int)) {
      /* Just return the signal number */
      *((int*)buffer) = siginfo.ssi_signo;
      
      if (bytes_read) {
        *bytes_read = sizeof(int);
      }
    }
  }
  
  return SIO_SUCCESS;
#endif
}

/**
* @brief Write to a signal stream (send signals)
*/
static sio_error_t signal_write(sio_stream_t *stream, const void *buffer, size_t size, size_t *bytes_written, int flags) {
  assert(stream && stream->type == SIO_STREAM_SIGNAL);
  
  /* Initialize bytes_written if provided */
  if (bytes_written) {
    *bytes_written = 0;
  }
  
  /* Check if stream is writable */
  if (!(stream->flags & SIO_STREAM_WRITE)) {
    return SIO_ERROR_PERM;
  }
  
  /* Writing to a signal stream sends signals */
  if (!buffer || size < sizeof(int)) {
    return SIO_ERROR_PARAM;
  }
  
  /* Get the signal number */
  int signum = *((const int*)buffer);
  
#if defined(SIO_OS_WINDOWS)
  /* Convert signal number to Windows signal */
  DWORD signal_type;
  switch (signum) {
    case SIGINT:
      signal_type = CTRL_C_EVENT;
      break;
    case SIGBREAK:
      signal_type = CTRL_BREAK_EVENT;
      break;
    default:
      /* Unsupported signal */
      return SIO_ERROR_UNSUPPORTED;
  }
  
  /* Send the signal */
  if (!GenerateConsoleCtrlEvent(signal_type, 0)) {
    return sio_get_last_error();
  }
  
  if (bytes_written) {
    *bytes_written = sizeof(int);
  }
#else
  /* Get target process ID (default to current process) */
  pid_t pid = 0;
  if (size >= 2 * sizeof(int)) {
    pid = ((const int*)buffer)[1];
  }
  
  /* Send the signal */
  if (kill(pid != 0 ? pid : getpid(), signum) < 0) {
    return sio_get_last_error();
  }
  
  if (bytes_written) {
    *bytes_written = sizeof(int) + (pid != 0 ? sizeof(int) : 0);
  }
#endif
  
  return SIO_SUCCESS;
}

/**
* @brief Get signal stream options
*/
static sio_error_t signal_get_option(sio_stream_t *stream, sio_stream_option_t option, void *value, size_t *size) {
  assert(stream && stream->type == SIO_STREAM_SIGNAL);
  
  if (!value || !size || *size == 0) {
    return SIO_ERROR_PARAM;
  }
  
  switch (option) {
    case SIO_INFO_TYPE:
      if (*size < sizeof(sio_stream_type_t)) {
        return SIO_ERROR_BUFFER_TOO_SMALL;
      }
      *((sio_stream_type_t*)value) = stream->type;
      *size = sizeof(sio_stream_type_t);
      break;
      
    case SIO_INFO_FLAGS:
      if (*size < sizeof(int)) {
        return SIO_ERROR_BUFFER_TOO_SMALL;
      }
      *((int*)value) = stream->flags;
      *size = sizeof(int);
      break;
      
    case SIO_INFO_READABLE:
      if (*size < sizeof(int)) {
        return SIO_ERROR_BUFFER_TOO_SMALL;
      }
      *((int*)value) = (stream->flags & SIO_STREAM_READ) ? 1 : 0;
      *size = sizeof(int);
      break;
      
    case SIO_INFO_WRITABLE:
      if (*size < sizeof(int)) {
        return SIO_ERROR_BUFFER_TOO_SMALL;
      }
      *((int*)value) = (stream->flags & SIO_STREAM_WRITE) ? 1 : 0;
      *size = sizeof(int);
      break;
      
    case SIO_INFO_SEEKABLE:
      if (*size < sizeof(int)) {
        return SIO_ERROR_BUFFER_TOO_SMALL;
      }
      *((int*)value) = 0; /* Signals are not seekable */
      *size = sizeof(int);
      break;
      
    case SIO_INFO_HANDLE:
#if defined(SIO_OS_WINDOWS)
      if (*size < sizeof(HANDLE)) {
        return SIO_ERROR_BUFFER_TOO_SMALL;
      }
      *((HANDLE*)value) = stream->data.signal.event;
      *size = sizeof(HANDLE);
#else
      if (*size < sizeof(int)) {
        return SIO_ERROR_BUFFER_TOO_SMALL;
      }
      *((int*)value) = stream->data.signal.fd;
      *size = sizeof(int);
#endif
      break;
      
    default:
      return SIO_ERROR_UNSUPPORTED;
  }
  
  return SIO_SUCCESS;
}

/**
* @brief Set signal stream options
*/
static sio_error_t signal_set_option(sio_stream_t *stream, sio_stream_option_t option, const void *value, size_t size) {
  assert(stream && stream->type == SIO_STREAM_SIGNAL);
  
  /* Signal streams have no settable options */
  return SIO_ERROR_UNSUPPORTED;
}