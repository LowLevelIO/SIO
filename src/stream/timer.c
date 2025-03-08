/**
* @file src/stream/timer.c
* @brief Implementation of timer stream functionality
*
* This file provides the implementation of timer operations for the SIO library.
* Timer streams provide a way to wait for time intervals and can be used for
* timeout management and other time-based operations.
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
  #include <sys/timerfd.h>
  #include <unistd.h>
  #include <time.h>
  #include <errno.h>
  #include <fcntl.h>
#endif

/* Forward declarations of timer stream operations */
static sio_error_t timer_close(sio_stream_t *stream);
static sio_error_t timer_read(sio_stream_t *stream, void *buffer, size_t size, size_t *bytes_read, int flags);
static sio_error_t timer_write(sio_stream_t *stream, const void *buffer, size_t size, size_t *bytes_written, int flags);
static sio_error_t timer_get_option(sio_stream_t *stream, sio_stream_option_t option, void *value, size_t *size);
static sio_error_t timer_set_option(sio_stream_t *stream, sio_stream_option_t option, const void *value, size_t size);

/* Timer stream operations vtable */
static const sio_stream_ops_t timer_ops = {
  .close = timer_close,
  .read = timer_read,
  .write = timer_write,
  .readv = NULL, /* Will use fallback in stream.c */
  .writev = NULL, /* Will use fallback in stream.c */
  .flush = NULL, /* No flush needed for timers */
  .get_option = timer_get_option,
  .set_option = timer_set_option,
  .seek = NULL, /* Timers are not seekable */
  .tell = NULL, /* Timers don't have a position */
  .truncate = NULL, /* Timers can't be truncated */
  .get_size = NULL /* Timers don't have a size */
};

/**
* @brief Create a timer stream
* 
* @param stream Pointer to stream structure to initialize
* @param interval_ms Timer interval in milliseconds
* @param is_oneshot Whether timer fires once (1) or repeatedly (0)
* @param opt Combination of SIO_STREAM_* flags
* @return sio_error_t SIO_SUCCESS or error code
*/
sio_error_t sio_stream_open_timer(sio_stream_t *stream, uint64_t interval_ms, int is_oneshot, sio_stream_flags_t opt) {
  if (!stream) {
    return SIO_ERROR_PARAM;
  }
  
  /* Initialize the stream structure */
  memset(stream, 0, sizeof(sio_stream_t));
  stream->type = SIO_STREAM_TIMER;
  stream->flags = opt;
  stream->ops = &timer_ops;
  
#if defined(SIO_OS_WINDOWS)
  /* Windows implementation using waitable timer */
  HANDLE timer = CreateWaitableTimer(NULL, is_oneshot, NULL);
  if (timer == NULL) {
    return sio_get_last_error();
  }
  
  /* Convert interval to 100-nanosecond units */
  LARGE_INTEGER due_time;
  due_time.QuadPart = -(LONGLONG)(interval_ms * 10000);
  
  /* Set up timer */
  if (!SetWaitableTimer(timer, &due_time, is_oneshot ? 0 : (LONG)interval_ms, NULL, NULL, FALSE)) {
    CloseHandle(timer);
    return sio_get_last_error();
  }
  
  /* Store the timer */
  stream->data.timer.timer = timer;
#else
  /* POSIX implementation using timerfd */
  int fd = timerfd_create(CLOCK_MONOTONIC, TFD_NONBLOCK | TFD_CLOEXEC);
  if (fd < 0) {
    return sio_get_last_error();
  }
  
  /* Set up timer */
  struct itimerspec its;
  memset(&its, 0, sizeof(its));
  
  /* Convert milliseconds to seconds and nanoseconds */
  its.it_value.tv_sec = interval_ms / 1000;
  its.it_value.tv_nsec = (interval_ms % 1000) * 1000000;
  
  if (!is_oneshot) {
    its.it_interval.tv_sec = interval_ms / 1000;
    its.it_interval.tv_nsec = (interval_ms % 1000) * 1000000;
  }
  
  if (timerfd_settime(fd, 0, &its, NULL) < 0) {
    close(fd);
    return sio_get_last_error();
  }
  
  /* Store the file descriptor and timer ID */
  stream->data.timer.fd = fd;
#endif
  
  return SIO_SUCCESS;
}

/**
* @brief Open a timer stream from an existing handle
*/
sio_error_t sio_stream_open_timer_from_handle(sio_stream_t *stream, void *handle, sio_stream_flags_t opt) {
  if (!stream || !handle) {
    return SIO_ERROR_PARAM;
  }
  
  /* Initialize the stream structure */
  memset(stream, 0, sizeof(sio_stream_t));
  stream->type = SIO_STREAM_TIMER;
  stream->flags = opt;
  stream->ops = &timer_ops;
  
#if defined(SIO_OS_WINDOWS)
  stream->data.timer.timer = (HANDLE)handle;
#else
  stream->data.timer.fd = (int)(intptr_t)handle;
#endif
  
  return SIO_SUCCESS;
}

/**
* @brief Close a timer stream
*/
static sio_error_t timer_close(sio_stream_t *stream) {
  assert(stream && stream->type == SIO_STREAM_TIMER);
  
#if defined(SIO_OS_WINDOWS)
  /* Close the timer handle */
  if (stream->data.timer.timer && stream->data.timer.timer != INVALID_HANDLE_VALUE) {
    CancelWaitableTimer(stream->data.timer.timer);
    if (!CloseHandle(stream->data.timer.timer)) {
      return sio_get_last_error();
    }
    stream->data.timer.timer = INVALID_HANDLE_VALUE;
  }
#else
  /* Close the timer file descriptor */
  if (stream->data.timer.fd >= 0) {
    if (close(stream->data.timer.fd) < 0) {
      return sio_get_last_error();
    }
    stream->data.timer.fd = -1;
  }
#endif
  
  return SIO_SUCCESS;
}

/**
* @brief Read from a timer stream (wait for timer expiration)
*/
static sio_error_t timer_read(sio_stream_t *stream, void *buffer, size_t size, size_t *bytes_read, int flags) {
  assert(stream && stream->type == SIO_STREAM_TIMER);
  
  /* Initialize bytes_read if provided */
  if (bytes_read) {
    *bytes_read = 0;
  }
  
  /* Check if stream is readable */
  if (!(stream->flags & SIO_STREAM_READ)) {
    return SIO_ERROR_PERM;
  }
  
#if defined(SIO_OS_WINDOWS)
  /* Wait for the timer */
  DWORD wait_ms;
  
  if (flags & SIO_MSG_DONTWAIT) {
    wait_ms = 0; /* Don't wait */
  } else {
    wait_ms = INFINITE; /* Wait indefinitely */
  }
  
  DWORD result = WaitForSingleObject(stream->data.timer.timer, wait_ms);
  
  if (result == WAIT_OBJECT_0) {
    /* Timer expired */
    if (buffer && size >= sizeof(uint64_t)) {
      /* Return the number of expirations (always 1 for Windows) */
      *((uint64_t*)buffer) = 1;
      
      if (bytes_read) {
        *bytes_read = sizeof(uint64_t);
      }
    }
    
    return SIO_SUCCESS;
  } else if (result == WAIT_TIMEOUT) {
    /* Timer not expired yet */
    return SIO_ERROR_WOULDBLOCK;
  } else {
    /* Error */
    return sio_get_last_error();
  }
#else
  /* Read the timer expiration count */
  uint64_t expirations;
  
  int recv_flags = 0;
  /* Convert SIO flags to native flags */
  if (flags & SIO_MSG_DONTWAIT) {
    recv_flags |= MSG_DONTWAIT;
  }
  
  ssize_t result;
  
  if (flags & SIO_MSG_DONTWAIT) {
    /* Non-blocking read */
    result = read(stream->data.timer.fd, &expirations, sizeof(expirations));
    
    if (result < 0) {
      if (errno == EAGAIN || errno == EWOULDBLOCK) {
        /* Timer not expired yet */
        return SIO_ERROR_WOULDBLOCK;
      }
      return sio_get_last_error();
    }
  } else {
    /* Blocking read - need to poll or select since timerfd might be in non-blocking mode */
    fd_set readfds;
    int maxfd = stream->data.timer.fd;
    
    while (1) {
      FD_ZERO(&readfds);
      FD_SET(stream->data.timer.fd, &readfds);
      
      /* Wait for the timer fd to become readable */
      int select_result = select(maxfd + 1, &readfds, NULL, NULL, NULL);
      
      if (select_result < 0) {
        if (errno == EINTR) {
          /* Interrupted, try again */
          continue;
        }
        return sio_get_last_error();
      }
      
      if (FD_ISSET(stream->data.timer.fd, &readfds)) {
        /* Timer fd is readable, try to read from it */
        result = read(stream->data.timer.fd, &expirations, sizeof(expirations));
        
        if (result < 0) {
          if (errno == EAGAIN || errno == EWOULDBLOCK) {
            /* Spurious wakeup, try again */
            continue;
          }
          return sio_get_last_error();
        }
        
        /* Successfully read timer expiration count */
        break;
      }
    }
  }
  
  /* Return the expiration count if buffer is provided */
  if (buffer && size >= sizeof(uint64_t)) {
    *((uint64_t*)buffer) = expirations;
  }
  
  if (bytes_read) {
    *bytes_read = sizeof(uint64_t);
  }
  
  return SIO_SUCCESS;
#endif
}

/**
* @brief Write to a timer stream (set or reset timer)
*/
static sio_error_t timer_write(sio_stream_t *stream, const void *buffer, size_t size, size_t *bytes_written, int flags) {
  assert(stream && stream->type == SIO_STREAM_TIMER);
  
  /* Initialize bytes_written if provided */
  if (bytes_written) {
    *bytes_written = 0;
  }
  
  /* Check if stream is writable */
  if (!(stream->flags & SIO_STREAM_WRITE)) {
    return SIO_ERROR_PERM;
  }
  
  /* Writing to a timer resets or modifies it */
  if (!buffer || size < sizeof(uint64_t)) {
    return SIO_ERROR_PARAM;
  }
  
  /* Get the new interval in milliseconds */
  uint64_t interval_ms = *((const uint64_t*)buffer);
  
#if defined(SIO_OS_WINDOWS)
  /* Convert interval to 100-nanosecond units */
  LARGE_INTEGER due_time;
  due_time.QuadPart = -(LONGLONG)(interval_ms * 10000);
  
  /* Determine if timer is oneshot or periodic */
  HANDLE timer = stream->data.timer.timer;
  BOOL is_oneshot = (interval_ms == 0); /* If interval is 0, it's one-shot */
  
  /* Reset the timer */
  if (!SetWaitableTimer(timer, &due_time, is_oneshot ? 0 : (LONG)interval_ms, NULL, NULL, FALSE)) {
    return sio_get_last_error();
  }
  
  if (bytes_written) {
    *bytes_written = sizeof(uint64_t);
  }
#else
  /* Convert milliseconds to seconds and nanoseconds */
  struct itimerspec its;
  memset(&its, 0, sizeof(its));
  
  its.it_value.tv_sec = interval_ms / 1000;
  its.it_value.tv_nsec = (interval_ms % 1000) * 1000000;
  
  /* If this is a repeated timer and second uint64_t is provided */
  uint64_t interval_period = 0;
  if (size >= 2 * sizeof(uint64_t)) {
    interval_period = ((const uint64_t*)buffer)[1];
    its.it_interval.tv_sec = interval_period / 1000;
    its.it_interval.tv_nsec = (interval_period % 1000) * 1000000;
  }
  
  /* Reset the timer */
  if (timerfd_settime(stream->data.timer.fd, 0, &its, NULL) < 0) {
    return sio_get_last_error();
  }
  
  if (bytes_written) {
    *bytes_written = sizeof(uint64_t) * (interval_period > 0 ? 2 : 1);
  }
#endif
  
  return SIO_SUCCESS;
}

/**
* @brief Get timer stream options
*/
static sio_error_t timer_get_option(sio_stream_t *stream, sio_stream_option_t option, void *value, size_t *size) {
  assert(stream && stream->type == SIO_STREAM_TIMER);
  
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
      *((int*)value) = 0; /* Timers are not seekable */
      *size = sizeof(int);
      break;
      
    case SIO_INFO_HANDLE:
#if defined(SIO_OS_WINDOWS)
      if (*size < sizeof(HANDLE)) {
        return SIO_ERROR_BUFFER_TOO_SMALL;
      }
      *((HANDLE*)value) = stream->data.timer.timer;
      *size = sizeof(HANDLE);
#else
      if (*size < sizeof(int)) {
        return SIO_ERROR_BUFFER_TOO_SMALL;
      }
      *((int*)value) = stream->data.timer.fd;
      *size = sizeof(int);
#endif
      break;
      
    case SIO_OPT_TIMER_INTERVAL: {
      if (*size < sizeof(int32_t)) {
        return SIO_ERROR_BUFFER_TOO_SMALL;
      }
      
      int32_t interval = 0;
      
#if defined(SIO_OS_WINDOWS)
      /* Not easily accessible in Windows API */
      /* Return 0 as a fallback */
#else
      struct itimerspec its;
      if (timerfd_gettime(stream->data.timer.fd, &its) < 0) {
        return sio_get_last_error();
      }
      
      interval = (int32_t)(its.it_interval.tv_sec * 1000 + its.it_interval.tv_nsec / 1000000);
#endif
      
      *((int32_t*)value) = interval;
      *size = sizeof(int32_t);
      break;
    }
      
    case SIO_OPT_TIMER_ONESHOT: {
      if (*size < sizeof(int)) {
        return SIO_ERROR_BUFFER_TOO_SMALL;
      }
      
      int oneshot = 0;
      
#if defined(SIO_OS_WINDOWS)
      /* Not easily accessible in Windows API */
      /* Return 0 as a fallback */
#else
      struct itimerspec its;
      if (timerfd_gettime(stream->data.timer.fd, &its) < 0) {
        return sio_get_last_error();
      }
      
      oneshot = (its.it_interval.tv_sec == 0 && its.it_interval.tv_nsec == 0) ? 1 : 0;
#endif
      
      *((int*)value) = oneshot;
      *size = sizeof(int);
      break;
    }
      
    default:
      return SIO_ERROR_UNSUPPORTED;
  }
  
  return SIO_SUCCESS;
}

/**
* @brief Set timer stream options
*/
static sio_error_t timer_set_option(sio_stream_t *stream, sio_stream_option_t option, const void *value, size_t size) {
  assert(stream && stream->type == SIO_STREAM_TIMER);
  
  if (!value) {
    return SIO_ERROR_PARAM;
  }
  
  switch (option) {
    case SIO_OPT_TIMER_INTERVAL: {
      if (size < sizeof(int32_t)) {
        return SIO_ERROR_PARAM;
      }
      
      int32_t interval = *((const int32_t*)value);
      
#if defined(SIO_OS_WINDOWS)
      /* Get current one-shot state */
      int oneshot = 0;
      
      /* Convert interval to 100-nanosecond units */
      LARGE_INTEGER due_time;
      due_time.QuadPart = -(LONGLONG)(interval * 10000);
      
      /* Set the timer */
      if (!SetWaitableTimer(stream->data.timer.timer, &due_time, 
                          oneshot ? 0 : interval, NULL, NULL, FALSE)) {
        return sio_get_last_error();
      }
#else
      /* Get current timer state */
      struct itimerspec its;
      if (timerfd_gettime(stream->data.timer.fd, &its) < 0) {
        return sio_get_last_error();
      }
      
      /* Only update the interval */
      its.it_interval.tv_sec = interval / 1000;
      its.it_interval.tv_nsec = (interval % 1000) * 1000000;
      
      /* Update the value too if timer is active */
      if (its.it_value.tv_sec != 0 || its.it_value.tv_nsec != 0) {
        its.it_value.tv_sec = interval / 1000;
        its.it_value.tv_nsec = (interval % 1000) * 1000000;
      }
      
      /* Set the timer */
      if (timerfd_settime(stream->data.timer.fd, 0, &its, NULL) < 0) {
        return sio_get_last_error();
      }
#endif
      
      break;
    }
      
    case SIO_OPT_TIMER_ONESHOT: {
      if (size < sizeof(int)) {
        return SIO_ERROR_PARAM;
      }
      
      int oneshot = *((const int*)value);
      
#if defined(SIO_OS_WINDOWS)
      /* Get current interval */
      int32_t interval = 0; /* Default to 0 since we can't easily retrieve it */
      
      /* Convert interval to 100-nanosecond units */
      LARGE_INTEGER due_time;
      due_time.QuadPart = -(LONGLONG)(interval * 10000);
      
      /* Set the timer */
      if (!SetWaitableTimer(stream->data.timer.timer, &due_time, 
                          oneshot ? 0 : interval, NULL, NULL, FALSE)) {
        return sio_get_last_error();
      }
#else
      /* Get current timer state */
      struct itimerspec its;
      if (timerfd_gettime(stream->data.timer.fd, &its) < 0) {
        return sio_get_last_error();
      }
      
      /* Calculate current interval in milliseconds */
      int32_t interval = (int32_t)(its.it_interval.tv_sec * 1000 + its.it_interval.tv_nsec / 1000000);
      
      /* Update the interval based on one-shot setting */
      if (oneshot) {
        its.it_interval.tv_sec = 0;
        its.it_interval.tv_nsec = 0;
      } else {
        its.it_interval.tv_sec = interval / 1000;
        its.it_interval.tv_nsec = (interval % 1000) * 1000000;
      }
      
      /* Set the timer */
      if (timerfd_settime(stream->data.timer.fd, 0, &its, NULL) < 0) {
        return sio_get_last_error();
      }
#endif
      
      break;
    }
      
    default:
      return SIO_ERROR_UNSUPPORTED;
  }
  
  return SIO_SUCCESS;
}