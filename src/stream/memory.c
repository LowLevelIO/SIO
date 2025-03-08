/**
* @file src/stream/memory.c
* @brief Implementation of memory stream functionality
*
* This file provides the implementation of memory-based I/O operations for the SIO library.
* It supports two types of memory streams:
* 1. Buffer stream - backed by a sio_buffer_t
* 2. Raw memory stream - backed by a raw memory block
*
* @author zczxy
* @version 0.1.0
*/

#include <sio/stream.h>
#include <sio/buf.h>
#include <sio/err.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>

/* Forward declarations of buffer stream operations */
static sio_error_t buffer_close(sio_stream_t *stream);
static sio_error_t buffer_read(sio_stream_t *stream, void *buffer, size_t size, size_t *bytes_read, int flags);
static sio_error_t buffer_write(sio_stream_t *stream, const void *buffer, size_t size, size_t *bytes_written, int flags);
static sio_error_t buffer_seek(sio_stream_t *stream, int64_t offset, sio_seek_origin_t origin, uint64_t *new_position);
static sio_error_t buffer_tell(sio_stream_t *stream, uint64_t *position);
static sio_error_t buffer_get_size(sio_stream_t *stream, uint64_t *size);
static sio_error_t buffer_truncate(sio_stream_t *stream, uint64_t size);
static sio_error_t buffer_get_option(sio_stream_t *stream, sio_stream_option_t option, void *value, size_t *size);
static sio_error_t buffer_set_option(sio_stream_t *stream, sio_stream_option_t option, const void *value, size_t size);

/* Forward declarations of raw memory stream operations */
static sio_error_t rawmem_close(sio_stream_t *stream);
static sio_error_t rawmem_read(sio_stream_t *stream, void *buffer, size_t size, size_t *bytes_read, int flags);
static sio_error_t rawmem_write(sio_stream_t *stream, const void *buffer, size_t size, size_t *bytes_written, int flags);
static sio_error_t rawmem_seek(sio_stream_t *stream, int64_t offset, sio_seek_origin_t origin, uint64_t *new_position);
static sio_error_t rawmem_tell(sio_stream_t *stream, uint64_t *position);
static sio_error_t rawmem_get_size(sio_stream_t *stream, uint64_t *size);
static sio_error_t rawmem_get_option(sio_stream_t *stream, sio_stream_option_t option, void *value, size_t *size);
static sio_error_t rawmem_set_option(sio_stream_t *stream, sio_stream_option_t option, const void *value, size_t size);

/* Buffer stream operations vtable */
static const sio_stream_ops_t buffer_ops = {
  .close = buffer_close,
  .read = buffer_read,
  .write = buffer_write,
  .readv = NULL, /* Will use fallback in stream.c */
  .writev = NULL, /* Will use fallback in stream.c */
  .flush = NULL, /* No flush needed for memory */
  .get_option = buffer_get_option,
  .set_option = buffer_set_option,
  .seek = buffer_seek,
  .tell = buffer_tell,
  .truncate = buffer_truncate,
  .get_size = buffer_get_size
};

/* Raw memory stream operations vtable */
static const sio_stream_ops_t rawmem_ops = {
  .close = rawmem_close,
  .read = rawmem_read,
  .write = rawmem_write,
  .readv = NULL, /* Will use fallback in stream.c */
  .writev = NULL, /* Will use fallback in stream.c */
  .flush = NULL, /* No flush needed for memory */
  .get_option = rawmem_get_option,
  .set_option = rawmem_set_option,
  .seek = rawmem_seek,
  .tell = rawmem_tell,
  .truncate = NULL, /* Raw memory can't be truncated */
  .get_size = rawmem_get_size
};

/**
* @brief Create a buffer stream
* 
* @param stream Pointer to stream structure to initialize
* @param buffer Pointer to existing buffer (NULL to create new one)
* @param initial_size Initial size if creating a new buffer
* @param opt Combination of SIO_STREAM_* flags
* @return sio_error_t SIO_SUCCESS or error code
*/
sio_error_t sio_stream_open_buffer(sio_stream_t *stream, sio_buffer_t *buffer, size_t initial_size, sio_stream_flags_t opt) {
  if (!stream) {
    return SIO_ERROR_PARAM;
  }
  
  /* Initialize the stream structure */
  memset(stream, 0, sizeof(sio_stream_t));
  stream->type = SIO_STREAM_BUFFER;
  stream->flags = opt;
  stream->ops = &buffer_ops;
  
  if (buffer) {
    /* Use existing buffer */
    stream->data.buffer.buffer = buffer;
    stream->data.buffer.owns_buffer = 0;
  } else {
    /* Create a new buffer */
    sio_buffer_t *new_buffer = (sio_buffer_t*)malloc(sizeof(sio_buffer_t));
    if (!new_buffer) {
      return SIO_ERROR_MEM;
    }
    
    sio_error_t err = sio_buffer_create(new_buffer, initial_size);
    if (err != SIO_SUCCESS) {
      free(new_buffer);
      return err;
    }
    
    stream->data.buffer.buffer = new_buffer;
    stream->data.buffer.owns_buffer = 1;
  }
  
  return SIO_SUCCESS;
}

/**
* @brief Create a raw memory stream
* 
* @param stream Pointer to stream structure to initialize
* @param memory Pointer to memory block
* @param size Size of memory block
* @param opt Combination of SIO_STREAM_* flags
* @return sio_error_t SIO_SUCCESS or error code
*/
sio_error_t sio_stream_open_memory(sio_stream_t *stream, void *memory, size_t size, sio_stream_flags_t opt) {
  if (!stream || !memory) {
    return SIO_ERROR_PARAM;
  }
  
  /* Initialize the stream structure */
  memset(stream, 0, sizeof(sio_stream_t));
  stream->type = SIO_STREAM_RAWMEM;
  stream->flags = opt;
  stream->ops = &rawmem_ops;
  
  /* Set up raw memory */
  stream->data.rawmem.data = memory;
  stream->data.rawmem.size = size;
  stream->data.rawmem.position = 0;
  
  return SIO_SUCCESS;
}

/**
* @brief Open a buffer stream from an existing handle
*/
sio_error_t sio_stream_open_buffer_from_handle(sio_stream_t *stream, void *handle, sio_stream_flags_t opt) {
  if (!stream || !handle) {
    return SIO_ERROR_PARAM;
  }
  
  /* Initialize the stream structure */
  memset(stream, 0, sizeof(sio_stream_t));
  stream->type = SIO_STREAM_BUFFER;
  stream->flags = opt;
  stream->ops = &buffer_ops;
  
  /* Use the provided buffer */
  stream->data.buffer.buffer = (sio_buffer_t*)handle;
  stream->data.buffer.owns_buffer = 0;
  
  return SIO_SUCCESS;
}

/* Buffer stream implementation */

/**
* @brief Close a buffer stream
*/
static sio_error_t buffer_close(sio_stream_t *stream) {
  assert(stream && stream->type == SIO_STREAM_BUFFER);
  
  if (stream->data.buffer.buffer && stream->data.buffer.owns_buffer) {
    /* Destroy the buffer if we own it */
    sio_error_t err = sio_buffer_destroy(stream->data.buffer.buffer);
    if (err != SIO_SUCCESS) {
      return err;
    }
    
    free(stream->data.buffer.buffer);
    stream->data.buffer.buffer = NULL;
  }
  
  return SIO_SUCCESS;
}

/**
* @brief Read from a buffer stream
*/
static sio_error_t buffer_read(sio_stream_t *stream, void *buffer, size_t size, size_t *bytes_read, int flags) {
  assert(stream && stream->type == SIO_STREAM_BUFFER);
  
  if (!buffer && size > 0) {
    return SIO_ERROR_PARAM;
  }
  
  /* Initialize bytes_read if provided */
  if (bytes_read) {
    *bytes_read = 0;
  }
  
  /* Early return if size is 0 */
  if (size == 0) {
    return SIO_SUCCESS;
  }
  
  /* Check if stream is readable */
  if (!(stream->flags & SIO_STREAM_READ)) {
    return SIO_ERROR_PERM;
  }
  
  /* Get the buffer */
  sio_buffer_t *buf = stream->data.buffer.buffer;
  if (!buf) {
    return SIO_ERROR_IO;
  }
  
  /* Read from the buffer */
  size_t read_size;
  sio_error_t err = sio_buffer_read(buf, buffer, size, &read_size);
  
  /* Set bytes_read if provided */
  if (bytes_read) {
    *bytes_read = read_size;
  }
  
  /* Map SIO_ERROR_EOF to success if we read something */
  if (err == SIO_ERROR_EOF && read_size > 0) {
    return SIO_SUCCESS;
  }
  
  return err;
}

/**
* @brief Write to a buffer stream
*/
static sio_error_t buffer_write(sio_stream_t *stream, const void *buffer, size_t size, size_t *bytes_written, int flags) {
  assert(stream && stream->type == SIO_STREAM_BUFFER);
  
  if (!buffer && size > 0) {
    return SIO_ERROR_PARAM;
  }
  
  /* Initialize bytes_written if provided */
  if (bytes_written) {
    *bytes_written = 0;
  }
  
  /* Early return if size is 0 */
  if (size == 0) {
    return SIO_SUCCESS;
  }
  
  /* Check if stream is writable */
  if (!(stream->flags & SIO_STREAM_WRITE)) {
    return SIO_ERROR_PERM;
  }
  
  /* Get the buffer */
  sio_buffer_t *buf = stream->data.buffer.buffer;
  if (!buf) {
    return SIO_ERROR_IO;
  }
  
  /* Write to the buffer */
  sio_error_t err = sio_buffer_write(buf, buffer, size);
  
  /* Set bytes_written if provided and successful */
  if (err == SIO_SUCCESS && bytes_written) {
    *bytes_written = size;
  }
  
  return err;
}

/**
* @brief Seek in a buffer stream
*/
static sio_error_t buffer_seek(sio_stream_t *stream, int64_t offset, sio_seek_origin_t origin, uint64_t *new_position) {
  assert(stream && stream->type == SIO_STREAM_BUFFER);
  
  /* Get the buffer */
  sio_buffer_t *buf = stream->data.buffer.buffer;
  if (!buf) {
    return SIO_ERROR_IO;
  }
  
  sio_error_t err;
  
  switch (origin) {
    case SIO_SEEK_SET:
      if (offset < 0) {
        return SIO_ERROR_PARAM;
      }
      err = sio_buffer_seek(buf, (size_t)offset);
      break;
      
    case SIO_SEEK_CUR:
      err = sio_buffer_seek_relative(buf, offset);
      break;
      
    case SIO_SEEK_END:
      if (offset > 0) {
        return SIO_ERROR_PARAM;
      }
      err = sio_buffer_seek(buf, buf->size + offset);
      break;
      
    default:
      return SIO_ERROR_PARAM;
  }
  
  /* Set new position if provided and successful */
  if (err == SIO_SUCCESS && new_position) {
    *new_position = buf->position;
  }
  
  return err;
}

/**
* @brief Get current position in a buffer stream
*/
static sio_error_t buffer_tell(sio_stream_t *stream, uint64_t *position) {
  assert(stream && stream->type == SIO_STREAM_BUFFER);
  
  if (!position) {
    return SIO_ERROR_PARAM;
  }
  
  /* Get the buffer */
  sio_buffer_t *buf = stream->data.buffer.buffer;
  if (!buf) {
    return SIO_ERROR_IO;
  }
  
  *position = sio_buffer_tell(buf);
  
  return SIO_SUCCESS;
}

/**
* @brief Get size of a buffer stream
*/
static sio_error_t buffer_get_size(sio_stream_t *stream, uint64_t *size) {
  assert(stream && stream->type == SIO_STREAM_BUFFER);
  
  if (!size) {
    return SIO_ERROR_PARAM;
  }
  
  /* Get the buffer */
  sio_buffer_t *buf = stream->data.buffer.buffer;
  if (!buf) {
    return SIO_ERROR_IO;
  }
  
  *size = buf->size;
  
  return SIO_SUCCESS;
}

/**
* @brief Truncate a buffer stream
*/
static sio_error_t buffer_truncate(sio_stream_t *stream, uint64_t size) {
  assert(stream && stream->type == SIO_STREAM_BUFFER);
  
  /* Check if stream is writable */
  if (!(stream->flags & SIO_STREAM_WRITE)) {
    return SIO_ERROR_PERM;
  }
  
  /* Get the buffer */
  sio_buffer_t *buf = stream->data.buffer.buffer;
  if (!buf) {
    return SIO_ERROR_IO;
  }
  
  /* Check if size is valid */
  if (size > SIZE_MAX) {
    return SIO_ERROR_PARAM;
  }
  
  /* Resize the buffer */
  sio_error_t err = SIO_SUCCESS;
  
  if (size < buf->size) {
    /* Truncate by setting new size */
    buf->size = (size_t)size;
    
    /* Adjust position if needed */
    if (buf->position > buf->size) {
      buf->position = buf->size;
    }
    
    /* Optionally shrink capacity */
    if (buf->size < buf->capacity / 2) {
      err = sio_buffer_shrink_to_fit(buf);
    }
  } else if (size > buf->size) {
    /* Expand by ensuring capacity and adding zeros */
    err = sio_buffer_ensure_capacity(buf, (size_t)size);
    if (err != SIO_SUCCESS) {
      return err;
    }
    
    /* Fill new space with zeros */
    if (buf->size < (size_t)size) {
      size_t old_size = buf->size;
      memset(buf->data + old_size, 0, (size_t)size - old_size);
      buf->size = (size_t)size;
    }
  }
  
  return err;
}

/**
* @brief Get buffer stream options
*/
static sio_error_t buffer_get_option(sio_stream_t *stream, sio_stream_option_t option, void *value, size_t *size) {
  assert(stream && stream->type == SIO_STREAM_BUFFER);
  
  if (!value || !size || *size == 0) {
    return SIO_ERROR_PARAM;
  }
  
  /* Get the buffer */
  sio_buffer_t *buf = stream->data.buffer.buffer;
  if (!buf) {
    return SIO_ERROR_IO;
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
      
    case SIO_INFO_POSITION:
      if (*size < sizeof(uint64_t)) {
        return SIO_ERROR_BUFFER_TOO_SMALL;
      }
      *((uint64_t*)value) = buf->position;
      *size = sizeof(uint64_t);
      break;
      
    case SIO_INFO_SIZE:
      if (*size < sizeof(uint64_t)) {
        return SIO_ERROR_BUFFER_TOO_SMALL;
      }
      *((uint64_t*)value) = buf->size;
      *size = sizeof(uint64_t);
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
      *((int*)value) = 1; /* Buffers are always seekable */
      *size = sizeof(int);
      break;
      
    case SIO_INFO_EOF:
      if (*size < sizeof(int)) {
        return SIO_ERROR_BUFFER_TOO_SMALL;
      }
      *((int*)value) = sio_buffer_at_end(buf);
      *size = sizeof(int);
      break;
      
    case SIO_INFO_HANDLE:
      if (*size < sizeof(void*)) {
        return SIO_ERROR_BUFFER_TOO_SMALL;
      }
      *((void**)value) = buf;
      *size = sizeof(void*);
      break;
      
    case SIO_INFO_BUFFER_SIZE:
      if (*size < sizeof(size_t)) {
        return SIO_ERROR_BUFFER_TOO_SMALL;
      }
      *((size_t*)value) = buf->capacity;
      *size = sizeof(size_t);
      break;
      
    default:
      return SIO_ERROR_UNSUPPORTED;
  }
  
  return SIO_SUCCESS;
}

/**
* @brief Set buffer stream options
*/
static sio_error_t buffer_set_option(sio_stream_t *stream, sio_stream_option_t option, const void *value, size_t size) {
  assert(stream && stream->type == SIO_STREAM_BUFFER);
  
  if (!value) {
    return SIO_ERROR_PARAM;
  }
  
  /* Get the buffer */
  sio_buffer_t *buf = stream->data.buffer.buffer;
  if (!buf) {
    return SIO_ERROR_IO;
  }
  
  switch (option) {
    case SIO_OPT_BUFFER_SIZE:
      if (size < sizeof(size_t)) {
        return SIO_ERROR_PARAM;
      }
      
      return sio_buffer_resize(buf, *((const size_t*)value));
      
    default:
      return SIO_ERROR_UNSUPPORTED;
  }
}

/* Raw memory stream implementation */

/**
* @brief Close a raw memory stream
*/
static sio_error_t rawmem_close(sio_stream_t *stream) {
  assert(stream && stream->type == SIO_STREAM_RAWMEM);
  
  /* Just clear the pointer and size */
  stream->data.rawmem.data = NULL;
  stream->data.rawmem.size = 0;
  stream->data.rawmem.position = 0;
  
  return SIO_SUCCESS;
}

/**
* @brief Read from a raw memory stream
*/
static sio_error_t rawmem_read(sio_stream_t *stream, void *buffer, size_t size, size_t *bytes_read, int flags) {
  assert(stream && stream->type == SIO_STREAM_RAWMEM);
  
  if (!buffer && size > 0) {
    return SIO_ERROR_PARAM;
  }
  
  /* Initialize bytes_read if provided */
  if (bytes_read) {
    *bytes_read = 0;
  }
  
  /* Early return if size is 0 */
  if (size == 0) {
    return SIO_SUCCESS;
  }
  
  /* Check if stream is readable */
  if (!(stream->flags & SIO_STREAM_READ)) {
    return SIO_ERROR_PERM;
  }
  
  /* Get the raw memory */
  void *data = stream->data.rawmem.data;
  size_t mem_size = stream->data.rawmem.size;
  size_t position = stream->data.rawmem.position;
  
  if (!data) {
    return SIO_ERROR_IO;
  }
  
  /* Check if we're at the end */
  if (position >= mem_size) {
    return SIO_ERROR_EOF;
  }
  
  /* Calculate how much we can read */
  size_t remaining = mem_size - position;
  size_t read_size = (size <= remaining) ? size : remaining;
  
  /* Copy the data */
  memcpy(buffer, (uint8_t*)data + position, read_size);
  
  /* Update position */
  stream->data.rawmem.position += read_size;
  
  /* Set bytes_read if provided */
  if (bytes_read) {
    *bytes_read = read_size;
  }
  
  /* Return EOF if we couldn't read all requested bytes */
  return (read_size < size) ? SIO_ERROR_EOF : SIO_SUCCESS;
}

/**
* @brief Write to a raw memory stream
*/
static sio_error_t rawmem_write(sio_stream_t *stream, const void *buffer, size_t size, size_t *bytes_written, int flags) {
  assert(stream && stream->type == SIO_STREAM_RAWMEM);
  
  if (!buffer && size > 0) {
    return SIO_ERROR_PARAM;
  }
  
  /* Initialize bytes_written if provided */
  if (bytes_written) {
    *bytes_written = 0;
  }
  
  /* Early return if size is 0 */
  if (size == 0) {
    return SIO_SUCCESS;
  }
  
  /* Check if stream is writable */
  if (!(stream->flags & SIO_STREAM_WRITE)) {
    return SIO_ERROR_PERM;
  }
  
  /* Get the raw memory */
  void *data = stream->data.rawmem.data;
  size_t mem_size = stream->data.rawmem.size;
  size_t position = stream->data.rawmem.position;
  
  if (!data) {
    return SIO_ERROR_IO;
  }
  
  /* Check if we're at the end */
  if (position >= mem_size) {
    return SIO_ERROR_IO;
  }
  
  /* Calculate how much we can write */
  size_t remaining = mem_size - position;
  size_t write_size = (size <= remaining) ? size : remaining;
  
  /* Copy the data */
  memcpy((uint8_t*)data + position, buffer, write_size);
  
  /* Update position */
  stream->data.rawmem.position += write_size;
  
  /* Set bytes_written if provided */
  if (bytes_written) {
    *bytes_written = write_size;
  }
  
  /* Return success even for partial writes, as that's expected behavior 
     for raw memory streams when writing near the end of the buffer */
  return SIO_SUCCESS;
}

/**
* @brief Seek in a raw memory stream
*/
static sio_error_t rawmem_seek(sio_stream_t *stream, int64_t offset, sio_seek_origin_t origin, uint64_t *new_position) {
  assert(stream && stream->type == SIO_STREAM_RAWMEM);
  
  /* Get the raw memory */
  size_t mem_size = stream->data.rawmem.size;
  size_t position = stream->data.rawmem.position;
  
  int64_t new_pos;
  
  switch (origin) {
    case SIO_SEEK_SET:
      new_pos = offset;
      break;
      
    case SIO_SEEK_CUR:
      new_pos = (int64_t)position + offset;
      break;
      
    case SIO_SEEK_END:
      new_pos = (int64_t)mem_size + offset;
      break;
      
    default:
      return SIO_ERROR_PARAM;
  }
  
  /* Check if the new position is valid */
  if (new_pos < 0 || (uint64_t)new_pos > mem_size) {
    return SIO_ERROR_PARAM;
  }
  
  /* Update position */
  stream->data.rawmem.position = (size_t)new_pos;
  
  /* Set new position if provided */
  if (new_position) {
    *new_position = (uint64_t)new_pos;
  }
  
  return SIO_SUCCESS;
}

/**
* @brief Get current position in a raw memory stream
*/
static sio_error_t rawmem_tell(sio_stream_t *stream, uint64_t *position) {
  assert(stream && stream->type == SIO_STREAM_RAWMEM);
  
  if (!position) {
    return SIO_ERROR_PARAM;
  }
  
  *position = stream->data.rawmem.position;
  
  return SIO_SUCCESS;
}

/**
* @brief Get size of a raw memory stream
*/
static sio_error_t rawmem_get_size(sio_stream_t *stream, uint64_t *size) {
  assert(stream && stream->type == SIO_STREAM_RAWMEM);
  
  if (!size) {
    return SIO_ERROR_PARAM;
  }
  
  *size = stream->data.rawmem.size;
  
  return SIO_SUCCESS;
}

/**
* @brief Get raw memory stream options
*/
static sio_error_t rawmem_get_option(sio_stream_t *stream, sio_stream_option_t option, void *value, size_t *size) {
  assert(stream && stream->type == SIO_STREAM_RAWMEM);
  
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
      
    case SIO_INFO_POSITION:
      if (*size < sizeof(uint64_t)) {
        return SIO_ERROR_BUFFER_TOO_SMALL;
      }
      *((uint64_t*)value) = stream->data.rawmem.position;
      *size = sizeof(uint64_t);
      break;
      
    case SIO_INFO_SIZE:
      if (*size < sizeof(uint64_t)) {
        return SIO_ERROR_BUFFER_TOO_SMALL;
      }
      *((uint64_t*)value) = stream->data.rawmem.size;
      *size = sizeof(uint64_t);
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
      *((int*)value) = 1; /* Raw memory is always seekable */
      *size = sizeof(int);
      break;
      
    case SIO_INFO_EOF:
      if (*size < sizeof(int)) {
        return SIO_ERROR_BUFFER_TOO_SMALL;
      }
      *((int*)value) = (stream->data.rawmem.position >= stream->data.rawmem.size) ? 1 : 0;
      *size = sizeof(int);
      break;
      
    case SIO_INFO_HANDLE:
      if (*size < sizeof(void*)) {
        return SIO_ERROR_BUFFER_TOO_SMALL;
      }
      *((void**)value) = stream->data.rawmem.data;
      *size = sizeof(void*);
      break;
      
    default:
      return SIO_ERROR_UNSUPPORTED;
  }
  
  return SIO_SUCCESS;
}

/**
* @brief Set raw memory stream options
*/
static sio_error_t rawmem_set_option(sio_stream_t *stream, sio_stream_option_t option, const void *value, size_t size) {
  assert(stream && stream->type == SIO_STREAM_RAWMEM);
  
  /* Raw memory streams have no settable options */
  return SIO_ERROR_UNSUPPORTED;
}