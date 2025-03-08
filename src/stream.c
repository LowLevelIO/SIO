/**
* @file src/stream.c
* @brief Implementation of core stream functionality
*
* Common stream operations and utilities that are not specific to particular stream types.
* This file provides the main interface for operations on streams by dispatching to the
* appropriate implementation via the operations vtable.
*
* @author zczxy
* @version 0.1.0
*/

#include <sio/stream.h>
#include <sio/err.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

/* Static function declarations */
static sio_error_t check_stream_valid(sio_stream_t *stream);
static sio_error_t check_stream_operation(sio_stream_t *stream, void *op_func);

/* Standard streams */
static sio_stream_t g_stdin = {0};
static sio_stream_t g_stdout = {0};
static sio_stream_t g_stderr = {0};
static int g_std_streams_initialized = 0;

/**
* @brief Initialize the standard streams if not already done
* 
* @return sio_error_t SIO_SUCCESS or error code
*/
static sio_error_t initialize_std_streams(void) {
  if (g_std_streams_initialized) {
    return SIO_SUCCESS;
  }

  /* Initialize the stream structures */
  memset(&g_stdin, 0, sizeof(g_stdin));
  memset(&g_stdout, 0, sizeof(g_stdout));
  memset(&g_stderr, 0, sizeof(g_stderr));
  
  sio_error_t err;
  
#if defined(SIO_OS_WINDOWS)
  /* Windows standard handles */
  err = sio_stream_open_file_from_handle(&g_stdin, GetStdHandle(STD_INPUT_HANDLE), SIO_STREAM_READ);
  if (err != SIO_SUCCESS) return err;
  
  err = sio_stream_open_file_from_handle(&g_stdout, GetStdHandle(STD_OUTPUT_HANDLE), SIO_STREAM_WRITE);
  if (err != SIO_SUCCESS) return err;
  
  err = sio_stream_open_file_from_handle(&g_stderr, GetStdHandle(STD_ERROR_HANDLE), SIO_STREAM_WRITE);
  if (err != SIO_SUCCESS) return err;
#else
  /* POSIX standard file descriptors */
  err = sio_stream_open_file_from_handle(&g_stdin, (void*)(intptr_t)0, SIO_STREAM_READ);
  if (err != SIO_SUCCESS) return err;
  
  err = sio_stream_open_file_from_handle(&g_stdout, (void*)(intptr_t)1, SIO_STREAM_WRITE);
  if (err != SIO_SUCCESS) return err;
  
  err = sio_stream_open_file_from_handle(&g_stderr, (void*)(intptr_t)2, SIO_STREAM_WRITE);
  if (err != SIO_SUCCESS) return err;
#endif

  g_std_streams_initialized = 1;
  return SIO_SUCCESS;
}

/**
* @brief Verify that a stream pointer is valid
* 
* @param stream Stream to check
* @return sio_error_t SIO_SUCCESS or error code
*/
static sio_error_t check_stream_valid(sio_stream_t *stream) {
  if (!stream) {
    return SIO_ERROR_PARAM;
  }
  
  if (!stream->ops) {
    return SIO_ERROR_PARAM;
  }
  
  return SIO_SUCCESS;
}

/**
* @brief Verify that a specific operation is implemented for a stream
* 
* @param stream Stream to check
* @param op_func Function pointer to check
* @return sio_error_t SIO_SUCCESS or error code
*/
static sio_error_t check_stream_operation(sio_stream_t *stream, void *op_func) {
  sio_error_t err = check_stream_valid(stream);
  if (err != SIO_SUCCESS) {
    return err;
  }
  
  if (!op_func) {
    return SIO_ERROR_UNSUPPORTED;
  }
  
  return SIO_SUCCESS;
}

/* Core stream operations */

sio_error_t sio_stream_close(sio_stream_t *stream) {
  sio_error_t err = check_stream_valid(stream);
  if (err != SIO_SUCCESS) {
    return err;
  }
  
  if (!stream->ops->close) {
    return SIO_ERROR_UNSUPPORTED;
  }
  
  return stream->ops->close(stream);
}

sio_error_t sio_stream_read(sio_stream_t *stream, void *buffer, size_t size, size_t *bytes_read, sio_stream_fflag_t flags) {
  /* Check parameters */
  if (!buffer && size > 0) {
    return SIO_ERROR_PARAM;
  }
  
  sio_error_t err = check_stream_valid(stream);
  if (err != SIO_SUCCESS) {
    return err;
  }
  
  if (!stream->ops->read) {
    return SIO_ERROR_UNSUPPORTED;
  }
  
  /* Initialize bytes_read to 0 if provided */
  if (bytes_read) {
    *bytes_read = 0;
  }
  
  /* Special optimization for zero-sized reads */
  if (size == 0) {
    return SIO_SUCCESS;
  }
  
  /* Handle DOALL flag special case */
  if (flags & SIO_DOALL) {
    size_t total_read = 0;
    size_t bytes_this_read = 0;
    uint8_t *buf_ptr = (uint8_t*)buffer;
    sio_stream_fflag_t inner_flags = flags & ~SIO_DOALL;
    
    while (total_read < size) {
      bytes_this_read = 0;
      err = stream->ops->read(stream, buf_ptr + total_read, size - total_read, &bytes_this_read, inner_flags);
      
      /* Update total bytes read */
      total_read += bytes_this_read;
      
      /* Check for errors or EOF */
      if (err != SIO_SUCCESS || bytes_this_read == 0) {
        break;
      }
      
      /* Check for NONBLOCK flag */
      if (flags & SIO_DOALL_NONBLOCK) {
        /* In non-blocking mode, return with whatever we got */
        break;
      }
    }
    
    /* Set bytes_read if provided */
    if (bytes_read) {
      *bytes_read = total_read;
    }
    
    /* Return error if we didn't read all requested data, unless we read something */
    if (total_read < size) {
      return (total_read > 0) ? SIO_ERROR_EOF : err;
    }
    
    return SIO_SUCCESS;
  }
  
  /* Standard read */
  return stream->ops->read(stream, buffer, size, bytes_read, flags);
}

sio_error_t sio_stream_write(sio_stream_t *stream, const void *buffer, size_t size, size_t *bytes_written, sio_stream_fflag_t flags) {
  /* Check parameters */
  if (!buffer && size > 0) {
    return SIO_ERROR_PARAM;
  }
  
  sio_error_t err = check_stream_valid(stream);
  if (err != SIO_SUCCESS) {
    return err;
  }
  
  if (!stream->ops->write) {
    return SIO_ERROR_UNSUPPORTED;
  }
  
  /* Initialize bytes_written to 0 if provided */
  if (bytes_written) {
    *bytes_written = 0;
  }
  
  /* Special optimization for zero-sized writes */
  if (size == 0) {
    return SIO_SUCCESS;
  }
  
  /* Handle DOALL flag special case */
  if (flags & SIO_DOALL) {
    size_t total_written = 0;
    size_t bytes_this_write = 0;
    const uint8_t *buf_ptr = (const uint8_t*)buffer;
    sio_stream_fflag_t inner_flags = flags & ~SIO_DOALL;
    
    while (total_written < size) {
      bytes_this_write = 0;
      err = stream->ops->write(stream, buf_ptr + total_written, size - total_written, &bytes_this_write, inner_flags);
      
      /* Update total bytes written */
      total_written += bytes_this_write;
      
      /* Check for errors or if no progress */
      if (err != SIO_SUCCESS || bytes_this_write == 0) {
        break;
      }
      
      /* Check for NONBLOCK flag */
      if (flags & SIO_DOALL_NONBLOCK) {
        /* In non-blocking mode, return with whatever we wrote */
        break;
      }
    }
    
    /* Set bytes_written if provided */
    if (bytes_written) {
      *bytes_written = total_written;
    }
    
    /* Return error if we didn't write all requested data, unless we wrote something */
    if (total_written < size) {
      return (total_written > 0) ? SIO_ERROR_IO : err;
    }
    
    return SIO_SUCCESS;
  }
  
  /* Standard write */
  return stream->ops->write(stream, buffer, size, bytes_written, flags);
}

sio_error_t sio_stream_flush(sio_stream_buffered_t *stream) {
  sio_error_t err = check_stream_valid((sio_stream_t*)stream);
  if (err != SIO_SUCCESS) {
    return err;
  }
  
  if (!((sio_stream_t*)stream)->ops->flush) {
    return SIO_ERROR_UNSUPPORTED;
  }
  
  return ((sio_stream_t*)stream)->ops->flush(stream);
}

/* Extended stream operations */

sio_error_t sio_stream_seek(sio_stream_t *stream, int64_t offset, sio_seek_origin_t origin, uint64_t *new_position) {
  sio_error_t err = check_stream_valid(stream);
  if (err != SIO_SUCCESS) {
    return err;
  }
  
  if (!stream->ops->seek) {
    return SIO_ERROR_UNSUPPORTED;
  }
  
  return stream->ops->seek(stream, offset, origin, new_position);
}

sio_error_t sio_stream_tell(sio_stream_t *stream, uint64_t *position) {
  sio_error_t err = check_stream_valid(stream);
  if (err != SIO_SUCCESS) {
    return err;
  }
  
  if (!stream->ops->tell) {
    return SIO_ERROR_UNSUPPORTED;
  }
  
  return stream->ops->tell(stream, position);
}

sio_error_t sio_stream_truncate(sio_stream_t *stream, uint64_t size) {
  sio_error_t err = check_stream_valid(stream);
  if (err != SIO_SUCCESS) {
    return err;
  }
  
  if (!stream->ops->truncate) {
    return SIO_ERROR_UNSUPPORTED;
  }
  
  return stream->ops->truncate(stream, size);
}

sio_error_t sio_stream_get_size(sio_stream_t *stream, uint64_t *size) {
  if (!size) {
    return SIO_ERROR_PARAM;
  }
  
  sio_error_t err = check_stream_valid(stream);
  if (err != SIO_SUCCESS) {
    return err;
  }
  
  if (!stream->ops->get_size) {
    return SIO_ERROR_UNSUPPORTED;
  }
  
  return stream->ops->get_size(stream, size);
}

/* Stream property and option functions */

sio_error_t sio_stream_set_option(sio_stream_t *stream, sio_stream_option_t option, const void *value, size_t size) {
  sio_error_t err = check_stream_valid(stream);
  if (err != SIO_SUCCESS) {
    return err;
  }
  
  if (!stream->ops->set_option) {
    return SIO_ERROR_UNSUPPORTED;
  }
  
  return stream->ops->set_option(stream, option, value, size);
}

sio_error_t sio_stream_get_option(sio_stream_t *stream, sio_stream_option_t option, void *value, size_t *size) {
  sio_error_t err = check_stream_valid(stream);
  if (err != SIO_SUCCESS) {
    return err;
  }
  
  if (!stream->ops->get_option) {
    return SIO_ERROR_UNSUPPORTED;
  }
  
  return stream->ops->get_option(stream, option, value, size);
}

int sio_stream_eof(const sio_stream_t *stream) {
  if (!stream) {
    return 1; /* Treat NULL stream as EOF */
  }
  
  /* Use get_option to get EOF state */
  int eof = 0;
  size_t size = sizeof(eof);
  
  sio_error_t err = ((sio_stream_t*)stream)->ops->get_option((sio_stream_t*)stream, SIO_INFO_EOF, &eof, &size);
  
  /* If error or option not supported, try to determine from stream state */
  if (err != SIO_SUCCESS) {
    /* We can only make a guess here */
    return 0; /* Assume not EOF by default */
  }
  
  return eof;
}

sio_error_t sio_stream_get_error(const sio_stream_t *stream) {
  if (!stream) {
    return SIO_ERROR_PARAM;
  }
  
  /* Use get_option to get last error */
  sio_error_t last_error = SIO_SUCCESS;
  size_t size = sizeof(last_error);
  
  sio_error_t err = ((sio_stream_t*)stream)->ops->get_option((sio_stream_t*)stream, SIO_INFO_ERROR, &last_error, &size);
  
  /* If error or option not supported, return a generic error */
  if (err != SIO_SUCCESS) {
    return SIO_ERROR_GENERIC;
  }
  
  return last_error;
}

/* Advanced stream operations */

sio_error_t sio_stream_set_buffer(sio_stream_t *stream, size_t buffer_size, int mode) {
  /* This requires creating a buffered stream wrapper around the original stream */
  /* For now, just return unsupported */
  return SIO_ERROR_UNSUPPORTED;
}

/* Factory functions implementation */

sio_error_t sio_stream_from_handle(sio_stream_t *stream, void *fd_or_handle, sio_stream_type_t type, sio_stream_flags_t opt) {
  if (!stream) {
    return SIO_ERROR_PARAM;
  }
  
  /* Initialize the stream structure */
  memset(stream, 0, sizeof(sio_stream_t));
  
  /* Dispatch to the appropriate stream type implementation */
  switch (type) {
    case SIO_STREAM_FILE:
      return sio_stream_open_file_from_handle(stream, fd_or_handle, opt);
      
    case SIO_STREAM_SOCKET:
      return sio_stream_open_socket_from_handle(stream, fd_or_handle, opt);
      
    case SIO_STREAM_PIPE:
      return SIO_ERROR_UNSUPPORTED; // sio_stream_open_pipe_from_handle(stream, fd_or_handle, opt);
      
    case SIO_STREAM_TIMER:
      return sio_stream_open_timer_from_handle(stream, fd_or_handle, opt);
      
    case SIO_STREAM_SIGNAL:
      return sio_stream_open_signal_from_handle(stream, fd_or_handle, opt);
      
    case SIO_STREAM_MSGQUEUE:
      return SIO_ERROR_UNSUPPORTED; // sio_stream_open_msgqueue_from_handle(stream, fd_or_handle, opt);
      
    case SIO_STREAM_SHMEM:
      return SIO_ERROR_UNSUPPORTED; // sio_stream_open_shmem_from_handle(stream, fd_or_handle, opt);
      
    case SIO_STREAM_BUFFER:
      return sio_stream_open_buffer_from_handle(stream, fd_or_handle, opt);
      
    case SIO_STREAM_TERMINAL:
      return SIO_ERROR_UNSUPPORTED; // sio_stream_open_terminal_from_handle(stream, fd_or_handle, opt);
      
    default:
      return SIO_ERROR_UNSUPPORTED;
  }
}

/* Standard streams accessor functions */

sio_error_t sio_stream_stdin(sio_stream_t **stdin) {
  sio_error_t err = initialize_std_streams();
  *stdin = &g_stdin;
  return err;
}

sio_error_t sio_stream_stdout(sio_stream_t **stdout) {
  sio_error_t err = initialize_std_streams();
  *stdout = &g_stdout;
  return err;
}

sio_error_t sio_stream_stderr(sio_stream_t **stderr) {
  sio_error_t err = initialize_std_streams();
  *stderr = &g_stderr;
  return err;
}

/* Helper functions for vector operations */

sio_error_t sio_stream_readv(sio_stream_t *stream, sio_iovec_t *iov, size_t iovcnt, size_t *bytes_read, sio_stream_fflag_t flags) {
  sio_error_t err = check_stream_valid(stream);
  if (err != SIO_SUCCESS) {
    return err;
  }
  
  if (!stream->ops->readv) {
    /* Fallback to loop of reads if readv not implemented */
    size_t total_read = 0;
    
    for (size_t i = 0; i < iovcnt; i++) {
      size_t this_read = 0;
      
#if defined(SIO_OS_WINDOWS)
      err = sio_stream_read(stream, iov[i].buf, iov[i].len, &this_read, flags);
#else
      err = sio_stream_read(stream, iov[i].iov_base, iov[i].iov_len, &this_read, flags);
#endif
      
      if (err != SIO_SUCCESS && err != SIO_ERROR_EOF) {
        /* Set bytes_read if provided */
        if (bytes_read) {
          *bytes_read = total_read;
        }
        return err;
      }
      
      total_read += this_read;
      
#if defined(SIO_OS_WINDOWS)
      if (this_read < iov[i].len) {
#else
      if (this_read < iov[i].iov_len) {
#endif
        /* Partial read, we're done */
        break;
      }
    }
    
    /* Set bytes_read if provided */
    if (bytes_read) {
      *bytes_read = total_read;
    }
    
    return SIO_SUCCESS;
  }
  
  return stream->ops->readv(stream, iov, iovcnt, bytes_read, flags);
}

sio_error_t sio_stream_writev(sio_stream_t *stream, const sio_iovec_t *iov, size_t iovcnt, size_t *bytes_written, sio_stream_fflag_t flags) {
  sio_error_t err = check_stream_valid(stream);
  if (err != SIO_SUCCESS) {
    return err;
  }
  
  if (!stream->ops->writev) {
    /* Fallback to loop of writes if writev not implemented */
    size_t total_written = 0;
    
    for (size_t i = 0; i < iovcnt; i++) {
      size_t this_written = 0;
      
#if defined(SIO_OS_WINDOWS)
      err = sio_stream_write(stream, iov[i].buf, iov[i].len, &this_written, flags);
#else
      err = sio_stream_write(stream, iov[i].iov_base, iov[i].iov_len, &this_written, flags);
#endif
      
      if (err != SIO_SUCCESS) {
        /* Set bytes_written if provided */
        if (bytes_written) {
          *bytes_written = total_written;
        }
        return err;
      }
      
      total_written += this_written;
      
#if defined(SIO_OS_WINDOWS)
      if (this_written < iov[i].len) {
#else
      if (this_written < iov[i].iov_len) {
#endif
        /* Partial write, we're done */
        break;
      }
    }
    
    /* Set bytes_written if provided */
    if (bytes_written) {
      *bytes_written = total_written;
    }
    
    return SIO_SUCCESS;
  }
  
  return stream->ops->writev(stream, iov, iovcnt, bytes_written, flags);
}