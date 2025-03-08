/**
* @file src/stream/file.c
* @brief Implementation of file stream functionality
*
* This file provides the implementation of file I/O operations for the SIO library.
* It handles file opening, closing, reading, writing, and other operations across
* different platforms (Windows and POSIX).
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
  #include <io.h>
  #include <fcntl.h>
#else
  #include <sys/types.h>
  #include <sys/stat.h>
  #include <fcntl.h>
  #include <unistd.h>
  #include <errno.h>
#endif

/* Forward declarations of file stream operations */
static sio_error_t file_close(sio_stream_t *stream);
static sio_error_t file_read(sio_stream_t *stream, void *buffer, size_t size, size_t *bytes_read, int flags);
static sio_error_t file_write(sio_stream_t *stream, const void *buffer, size_t size, size_t *bytes_written, int flags);
static sio_error_t file_seek(sio_stream_t *stream, int64_t offset, sio_seek_origin_t origin, uint64_t *new_position);
static sio_error_t file_tell(sio_stream_t *stream, uint64_t *position);
static sio_error_t file_truncate(sio_stream_t *stream, uint64_t size);
static sio_error_t file_get_size(sio_stream_t *stream, uint64_t *size);
static sio_error_t file_get_option(sio_stream_t *stream, sio_stream_option_t option, void *value, size_t *size);
static sio_error_t file_set_option(sio_stream_t *stream, sio_stream_option_t option, const void *value, size_t size);
static sio_error_t file_flush(sio_stream_buffered_t *stream);

/* File stream operations vtable */
static const sio_stream_ops_t file_ops = {
  .close = file_close,
  .read = file_read,
  .write = file_write,
  .readv = NULL, /* Will use fallback in stream.c */
  .writev = NULL, /* Will use fallback in stream.c */
  .flush = file_flush,
  .get_option = file_get_option,
  .set_option = file_set_option,
  .seek = file_seek,
  .tell = file_tell,
  .truncate = file_truncate,
  .get_size = file_get_size
};

/**
* @brief Convert SIO flags to platform-specific open flags
* 
* @param opt SIO stream flags
* @param mode Pointer to store the file mode (for POSIX)
* @return Platform-specific open flags
*/
#if defined(SIO_OS_WINDOWS)
static DWORD file_convert_flags(sio_stream_flags_t opt, DWORD *creation_disposition, DWORD *flags_and_attrs) {
  DWORD access = 0;
  *creation_disposition = 0;
  *flags_and_attrs = FILE_ATTRIBUTE_NORMAL;
  
  /* Access mode */
  if (opt & SIO_STREAM_READ) {
    access |= GENERIC_READ;
  }
  if (opt & SIO_STREAM_WRITE) {
    access |= GENERIC_WRITE;
  }
  
  /* Creation mode */
  if (opt & SIO_STREAM_CREATE) {
    if (opt & SIO_STREAM_EXCL) {
      *creation_disposition = CREATE_NEW;
    } else if (opt & SIO_STREAM_TRUNC) {
      *creation_disposition = CREATE_ALWAYS;
    } else {
      *creation_disposition = OPEN_ALWAYS;
    }
  } else {
    if (opt & SIO_STREAM_TRUNC) {
      *creation_disposition = TRUNCATE_EXISTING;
    } else {
      *creation_disposition = OPEN_EXISTING;
    }
  }
  
  /* Other flags */
  if (opt & SIO_STREAM_ASYNC) {
    *flags_and_attrs |= FILE_FLAG_OVERLAPPED;
  }
  
  if (opt & SIO_STREAM_DIRECT) {
    *flags_and_attrs |= FILE_FLAG_NO_BUFFERING;
  }
  
  if (opt & SIO_STREAM_SYNC) {
    *flags_and_attrs |= FILE_FLAG_WRITE_THROUGH;
  }
  
  if (opt & SIO_STREAM_TEMP) {
    *flags_and_attrs |= FILE_ATTRIBUTE_TEMPORARY;
  }
  
  return access;
}
#else
static int file_convert_flags(sio_stream_flags_t opt, mode_t *mode) {
  int flags = 0;
  
  /* Access mode */
  if ((opt & SIO_STREAM_READ) && (opt & SIO_STREAM_WRITE)) {
    flags |= O_RDWR;
  } else if (opt & SIO_STREAM_READ) {
    flags |= O_RDONLY;
  } else if (opt & SIO_STREAM_WRITE) {
    flags |= O_WRONLY;
  }
  
  /* Creation mode */
  if (opt & SIO_STREAM_CREATE) {
    flags |= O_CREAT;
  }
  
  if (opt & SIO_STREAM_EXCL) {
    flags |= O_EXCL;
  }
  
  if (opt & SIO_STREAM_TRUNC) {
    flags |= O_TRUNC;
  }
  
  if (opt & SIO_STREAM_APPEND) {
    flags |= O_APPEND;
  }
  
  /* Other flags */
  if (opt & SIO_STREAM_NONBLOCK) {
    flags |= O_NONBLOCK;
  }
  
  #ifdef O_DIRECT
  if (opt & SIO_STREAM_DIRECT) {
    flags |= O_DIRECT;
  }
  #endif
  
  #ifdef O_SYNC
  if (opt & SIO_STREAM_SYNC) {
    flags |= O_SYNC;
  }
  #endif
  
  /* Set default mode if not specified */
  if (!*mode) {
    *mode = 0666; /* rw-rw-rw- modified by umask */
  }
  
  return flags;
}
#endif

/**
* @brief Create a file stream
*/
sio_error_t sio_stream_open_file(sio_stream_t *stream, const char *path, sio_stream_flags_t opt, int mode) {
  if (!stream || !path) {
    return SIO_ERROR_PARAM;
  }
  
  /* Initialize the stream structure */
  memset(stream, 0, sizeof(sio_stream_t));
  stream->type = SIO_STREAM_FILE;
  stream->flags = opt;
  stream->ops = &file_ops;
  
#if defined(SIO_OS_WINDOWS)
  DWORD creation_disposition;
  DWORD flags_and_attrs;
  DWORD access = file_convert_flags(opt, &creation_disposition, &flags_and_attrs);
  
  /* Convert path to wide characters (Windows API expects UTF-16) */
  WCHAR wide_path[MAX_PATH];
  if (MultiByteToWideChar(CP_UTF8, 0, path, -1, wide_path, MAX_PATH) == 0) {
    return sio_get_last_error();
  }
  
  /* Open the file */
  HANDLE handle = CreateFileW(wide_path, access, FILE_SHARE_READ, NULL, 
                             creation_disposition, flags_and_attrs, NULL);
  
  if (handle == INVALID_HANDLE_VALUE) {
    return sio_get_last_error();
  }
  
  /* Store the handle */
  stream->data.file.handle = handle;
  
#else
  mode_t file_mode = (mode_t)mode;
  int flags = file_convert_flags(opt, &file_mode);
  
  /* Open the file */
  int fd = open(path, flags, file_mode);
  if (fd < 0) {
    return sio_get_last_error();
  }
  
  /* Set close-on-exec flag if available */
  #ifdef FD_CLOEXEC
  fcntl(fd, F_SETFD, FD_CLOEXEC);
  #endif
  
  /* Store the file descriptor */
  stream->data.file.fd = fd;
#endif
  
  return SIO_SUCCESS;
}

/**
* @brief Open a file stream from an existing handle
*/
sio_error_t sio_stream_open_file_from_handle(sio_stream_t *stream, void *handle, sio_stream_flags_t opt) {
  if (!stream) {
    return SIO_ERROR_PARAM;
  }
  
  /* Initialize the stream structure */
  memset(stream, 0, sizeof(sio_stream_t));
  stream->type = SIO_STREAM_FILE;
  stream->flags = opt;
  stream->ops = &file_ops;
  
#if defined(SIO_OS_WINDOWS)
  stream->data.file.handle = handle;
#else
  stream->data.file.fd = (int)(intptr_t)handle;
#endif
  
  return SIO_SUCCESS;
}

/**
* @brief Close a file stream
*/
static sio_error_t file_close(sio_stream_t *stream) {
  assert(stream && stream->type == SIO_STREAM_FILE);
  
#if defined(SIO_OS_WINDOWS)
  /* Close the file handle */
  if (stream->data.file.handle && stream->data.file.handle != INVALID_HANDLE_VALUE) {
    if (!CloseHandle(stream->data.file.handle)) {
      return sio_get_last_error();
    }
    stream->data.file.handle = INVALID_HANDLE_VALUE;
  }
#else
  /* Close the file descriptor */
  if (stream->data.file.fd >= 0) {
    if (close(stream->data.file.fd) < 0) {
      return sio_get_last_error();
    }
    stream->data.file.fd = -1;
  }
#endif

  /* Unmap memory if it was mapped */
  if (stream->data.file.mmap_data) {
#if defined(SIO_OS_WINDOWS)
    if (!UnmapViewOfFile(stream->data.file.mmap_data)) {
      return sio_get_last_error();
    }
#else
    if (munmap(stream->data.file.mmap_data, stream->data.file.mmap_size) < 0) {
      return sio_get_last_error();
    }
#endif
    stream->data.file.mmap_data = NULL;
    stream->data.file.mmap_size = 0;
  }
  
  return SIO_SUCCESS;
}

/**
* @brief Read from a file stream
*/
static sio_error_t file_read(sio_stream_t *stream, void *buffer, size_t size, size_t *bytes_read, int flags) {
  assert(stream && stream->type == SIO_STREAM_FILE);
  
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
  
#if defined(SIO_OS_WINDOWS)
  DWORD bytes_read_win = 0;
  BOOL result;
  
  if (flags & SIO_DOALL) {
    /* Read all requested bytes (may require multiple reads) */
    size_t total_read = 0;
    BYTE *buf_ptr = (BYTE*)buffer;
    
    while (total_read < size) {
      result = ReadFile(stream->data.file.handle, buf_ptr + total_read, 
                        (DWORD)(size - total_read), &bytes_read_win, NULL);
      
      if (!result) {
        DWORD error = GetLastError();
        if (error == ERROR_HANDLE_EOF || error == ERROR_BROKEN_PIPE) {
          /* End of file reached */
          if (bytes_read) {
            *bytes_read = total_read;
          }
          return (total_read > 0) ? SIO_SUCCESS : SIO_ERROR_EOF;
        }
        /* Other error */
        if (bytes_read) {
          *bytes_read = total_read;
        }
        return sio_win_error_to_sio_error(error);
      }
      
      total_read += bytes_read_win;
      
      if (bytes_read_win == 0) {
        /* End of file reached */
        break;
      }
      
      /* If non-blocking read all, return after first read */
      if (flags & SIO_DOALL_NONBLOCK) {
        break;
      }
    }
    
    if (bytes_read) {
      *bytes_read = total_read;
    }
    
    return (total_read == size) ? SIO_SUCCESS : SIO_ERROR_EOF;
  } else {
    /* Single read operation */
    result = ReadFile(stream->data.file.handle, buffer, (DWORD)size, &bytes_read_win, NULL);
    
    if (!result) {
      DWORD error = GetLastError();
      if (error == ERROR_HANDLE_EOF || error == ERROR_BROKEN_PIPE) {
        /* End of file reached */
        if (bytes_read) {
          *bytes_read = 0;
        }
        return SIO_ERROR_EOF;
      }
      /* Other error */
      return sio_win_error_to_sio_error(error);
    }
    
    if (bytes_read) {
      *bytes_read = bytes_read_win;
    }
    
    return (bytes_read_win > 0) ? SIO_SUCCESS : SIO_ERROR_EOF;
  }
#else
  /* POSIX implementation */
  ssize_t result;
  
  if (flags & SIO_DOALL) {
    /* Read all requested bytes (may require multiple reads) */
    size_t total_read = 0;
    uint8_t *buf_ptr = (uint8_t*)buffer;
    
    while (total_read < size) {
      result = read(stream->data.file.fd, buf_ptr + total_read, size - total_read);
      
      if (result < 0) {
        if (errno == EINTR) {
          /* Interrupted, try again */
          continue;
        }
        /* Other error */
        if (bytes_read) {
          *bytes_read = total_read;
        }
        return sio_get_last_error();
      }
      
      if (result == 0) {
        /* End of file reached */
        break;
      }
      
      total_read += result;
      
      /* If non-blocking read all, return after first read */
      if (flags & SIO_DOALL_NONBLOCK) {
        break;
      }
    }
    
    if (bytes_read) {
      *bytes_read = total_read;
    }
    
    return (total_read == size) ? SIO_SUCCESS : SIO_ERROR_EOF;
  } else {
    /* Single read operation */
    do {
      result = read(stream->data.file.fd, buffer, size);
    } while (result < 0 && errno == EINTR);
    
    if (result < 0) {
      return sio_get_last_error();
    }
    
    if (bytes_read) {
      *bytes_read = result;
    }
    
    return (result > 0) ? SIO_SUCCESS : SIO_ERROR_EOF;
  }
#endif
}

/**
* @brief Write to a file stream
*/
static sio_error_t file_write(sio_stream_t *stream, const void *buffer, size_t size, size_t *bytes_written, int flags) {
  assert(stream && stream->type == SIO_STREAM_FILE);
  
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
  
#if defined(SIO_OS_WINDOWS)
  DWORD bytes_written_win = 0;
  BOOL result;
  
  if (flags & SIO_DOALL) {
    /* Write all requested bytes (may require multiple writes) */
    size_t total_written = 0;
    const BYTE *buf_ptr = (const BYTE*)buffer;
    
    while (total_written < size) {
      result = WriteFile(stream->data.file.handle, buf_ptr + total_written, 
                         (DWORD)(size - total_written), &bytes_written_win, NULL);
      
      if (!result) {
        if (bytes_written) {
          *bytes_written = total_written;
        }
        return sio_get_last_error();
      }
      
      total_written += bytes_written_win;
      
      if (bytes_written_win == 0) {
        /* Disk full or other error */
        break;
      }
      
      /* If non-blocking write all, return after first write */
      if (flags & SIO_DOALL_NONBLOCK) {
        break;
      }
    }
    
    if (bytes_written) {
      *bytes_written = total_written;
    }
    
    return (total_written == size) ? SIO_SUCCESS : SIO_ERROR_IO;
  } else {
    /* Single write operation */
    result = WriteFile(stream->data.file.handle, buffer, (DWORD)size, &bytes_written_win, NULL);
    
    if (!result) {
      return sio_get_last_error();
    }
    
    if (bytes_written) {
      *bytes_written = bytes_written_win;
    }
    
    return SIO_SUCCESS;
  }
#else
  /* POSIX implementation */
  ssize_t result;
  
  if (flags & SIO_DOALL) {
    /* Write all requested bytes (may require multiple writes) */
    size_t total_written = 0;
    const uint8_t *buf_ptr = (const uint8_t*)buffer;
    
    while (total_written < size) {
      result = write(stream->data.file.fd, buf_ptr + total_written, size - total_written);
      
      if (result < 0) {
        if (errno == EINTR) {
          /* Interrupted, try again */
          continue;
        }
        /* Other error */
        if (bytes_written) {
          *bytes_written = total_written;
        }
        return sio_get_last_error();
      }
      
      total_written += result;
      
      if (result == 0) {
        /* May indicate disk full or other error */
        break;
      }
      
      /* If non-blocking write all, return after first write */
      if (flags & SIO_DOALL_NONBLOCK) {
        break;
      }
    }
    
    if (bytes_written) {
      *bytes_written = total_written;
    }
    
    return (total_written == size) ? SIO_SUCCESS : SIO_ERROR_IO;
  } else {
    /* Single write operation */
    do {
      result = write(stream->data.file.fd, buffer, size);
    } while (result < 0 && errno == EINTR);
    
    if (result < 0) {
      return sio_get_last_error();
    }
    
    if (bytes_written) {
      *bytes_written = result;
    }
    
    return SIO_SUCCESS;
  }
#endif
}

/**
* @brief Seek in a file stream
*/
static sio_error_t file_seek(sio_stream_t *stream, int64_t offset, sio_seek_origin_t origin, uint64_t *new_position) {
  assert(stream && stream->type == SIO_STREAM_FILE);
  
#if defined(SIO_OS_WINDOWS)
  DWORD move_method;
  
  /* Convert seek origin */
  switch (origin) {
    case SIO_SEEK_SET:
      move_method = FILE_BEGIN;
      break;
    case SIO_SEEK_CUR:
      move_method = FILE_CURRENT;
      break;
    case SIO_SEEK_END:
      move_method = FILE_END;
      break;
    default:
      return SIO_ERROR_PARAM;
  }
  
  /* Set file pointer */
  LARGE_INTEGER li_offset, li_new_pos;
  li_offset.QuadPart = offset;
  
  if (!SetFilePointerEx(stream->data.file.handle, li_offset, &li_new_pos, move_method)) {
    return sio_get_last_error();
  }
  
  if (new_position) {
    *new_position = (uint64_t)li_new_pos.QuadPart;
  }
  
  return SIO_SUCCESS;
#else
  /* POSIX implementation */
  int whence;
  
  /* Convert seek origin */
  switch (origin) {
    case SIO_SEEK_SET:
      whence = SEEK_SET;
      break;
    case SIO_SEEK_CUR:
      whence = SEEK_CUR;
      break;
    case SIO_SEEK_END:
      whence = SEEK_END;
      break;
    default:
      return SIO_ERROR_PARAM;
  }
  
  /* Seek in the file */
  off_t result = lseek(stream->data.file.fd, (off_t)offset, whence);
  if (result < 0) {
    return sio_get_last_error();
  }
  
  if (new_position) {
    *new_position = (uint64_t)result;
  }
  
  return SIO_SUCCESS;
#endif
}

/**
* @brief Get current position in a file stream
*/
static sio_error_t file_tell(sio_stream_t *stream, uint64_t *position) {
  assert(stream && stream->type == SIO_STREAM_FILE);
  
  if (!position) {
    return SIO_ERROR_PARAM;
  }
  
#if defined(SIO_OS_WINDOWS)
  LARGE_INTEGER li_distance, li_pos;
  li_distance.QuadPart = 0;
  
  if (!SetFilePointerEx(stream->data.file.handle, li_distance, &li_pos, FILE_CURRENT)) {
    return sio_get_last_error();
  }
  
  *position = (uint64_t)li_pos.QuadPart;
  
  return SIO_SUCCESS;
#else
  /* POSIX implementation */
  off_t result = lseek(stream->data.file.fd, 0, SEEK_CUR);
  if (result < 0) {
    return sio_get_last_error();
  }
  
  *position = (uint64_t)result;
  
  return SIO_SUCCESS;
#endif
}

/**
* @brief Truncate a file stream
*/
static sio_error_t file_truncate(sio_stream_t *stream, uint64_t size) {
  assert(stream && stream->type == SIO_STREAM_FILE);
  
#if defined(SIO_OS_WINDOWS)
  /* Get current position */
  LARGE_INTEGER li_distance, li_current_pos;
  li_distance.QuadPart = 0;
  
  if (!SetFilePointerEx(stream->data.file.handle, li_distance, &li_current_pos, FILE_CURRENT)) {
    return sio_get_last_error();
  }
  
  /* Seek to the requested size */
  LARGE_INTEGER li_size;
  li_size.QuadPart = (LONGLONG)size;
  
  if (!SetFilePointerEx(stream->data.file.handle, li_size, NULL, FILE_BEGIN)) {
    return sio_get_last_error();
  }
  
  /* Truncate the file at the current position */
  if (!SetEndOfFile(stream->data.file.handle)) {
    DWORD error = GetLastError();
    
    /* Try to restore the original position */
    SetFilePointerEx(stream->data.file.handle, li_current_pos, NULL, FILE_BEGIN);
    
    return sio_win_error_to_sio_error(error);
  }
  
  /* Restore the original position */
  if (!SetFilePointerEx(stream->data.file.handle, li_current_pos, NULL, FILE_BEGIN)) {
    return sio_get_last_error();
  }
  
  return SIO_SUCCESS;
#else
  /* POSIX implementation */
  /* Some platforms may require the file to be open with write permission */
  if (ftruncate(stream->data.file.fd, (off_t)size) < 0) {
    return sio_get_last_error();
  }
  
  return SIO_SUCCESS;
#endif
}

/**
* @brief Get the size of a file stream
*/
static sio_error_t file_get_size(sio_stream_t *stream, uint64_t *size) {
  assert(stream && stream->type == SIO_STREAM_FILE);
  
  if (!size) {
    return SIO_ERROR_PARAM;
  }
  
#if defined(SIO_OS_WINDOWS)
  LARGE_INTEGER li_size;
  
  if (!GetFileSizeEx(stream->data.file.handle, &li_size)) {
    return sio_get_last_error();
  }
  
  *size = (uint64_t)li_size.QuadPart;
  
  return SIO_SUCCESS;
#else
  /* POSIX implementation */
  struct stat st;
  
  if (fstat(stream->data.file.fd, &st) < 0) {
    return sio_get_last_error();
  }
  
  *size = (uint64_t)st.st_size;
  
  return SIO_SUCCESS;
#endif
}

/**
* @brief Flush a file stream
*/
static sio_error_t file_flush(sio_stream_buffered_t *stream) {
  assert(stream && ((sio_stream_t*)stream)->type == SIO_STREAM_FILE);
  
#if defined(SIO_OS_WINDOWS)
  if (!FlushFileBuffers(((sio_stream_t*)stream)->data.file.handle)) {
    return sio_get_last_error();
  }
  
  return SIO_SUCCESS;
#else
  /* POSIX implementation */
  if (fsync(((sio_stream_t*)stream)->data.file.fd) < 0) {
    return sio_get_last_error();
  }
  
  return SIO_SUCCESS;
#endif
}

/**
* @brief Get stream options
*/
static sio_error_t file_get_option(sio_stream_t *stream, sio_stream_option_t option, void *value, size_t *size) {
  assert(stream && stream->type == SIO_STREAM_FILE);
  
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
      
    case SIO_INFO_POSITION: {
      if (*size < sizeof(uint64_t)) {
        return SIO_ERROR_BUFFER_TOO_SMALL;
      }
      uint64_t position;
      sio_error_t err = file_tell(stream, &position);
      if (err != SIO_SUCCESS) {
        return err;
      }
      *((uint64_t*)value) = position;
      *size = sizeof(uint64_t);
      break;
    }
      
    case SIO_INFO_SIZE: {
      if (*size < sizeof(uint64_t)) {
        return SIO_ERROR_BUFFER_TOO_SMALL;
      }
      uint64_t file_size;
      sio_error_t err = file_get_size(stream, &file_size);
      if (err != SIO_SUCCESS) {
        return err;
      }
      *((uint64_t*)value) = file_size;
      *size = sizeof(uint64_t);
      break;
    }
      
    case SIO_INFO_READABLE: {
      if (*size < sizeof(int)) {
        return SIO_ERROR_BUFFER_TOO_SMALL;
      }
      int readable = (stream->flags & SIO_STREAM_READ) ? 1 : 0;
      *((int*)value) = readable;
      *size = sizeof(int);
      break;
    }
      
    case SIO_INFO_WRITABLE: {
      if (*size < sizeof(int)) {
        return SIO_ERROR_BUFFER_TOO_SMALL;
      }
      int writable = (stream->flags & SIO_STREAM_WRITE) ? 1 : 0;
      *((int*)value) = writable;
      *size = sizeof(int);
      break;
    }
      
    case SIO_INFO_SEEKABLE: {
      if (*size < sizeof(int)) {
        return SIO_ERROR_BUFFER_TOO_SMALL;
      }
      /* Files are generally seekable */
      *((int*)value) = 1;
      *size = sizeof(int);
      break;
    }
      
    case SIO_INFO_EOF: {
      if (*size < sizeof(int)) {
        return SIO_ERROR_BUFFER_TOO_SMALL;
      }
      
      /* Check if at EOF by trying to read 0 bytes */
      size_t bytes_read;
      sio_error_t err = file_read(stream, NULL, 0, &bytes_read, 0);
      int eof = (err == SIO_ERROR_EOF) ? 1 : 0;
      
      *((int*)value) = eof;
      *size = sizeof(int);
      break;
    }
      
    case SIO_INFO_HANDLE: {
#if defined(SIO_OS_WINDOWS)
      if (*size < sizeof(HANDLE)) {
        return SIO_ERROR_BUFFER_TOO_SMALL;
      }
      *((HANDLE*)value) = stream->data.file.handle;
      *size = sizeof(HANDLE);
#else
      if (*size < sizeof(int)) {
        return SIO_ERROR_BUFFER_TOO_SMALL;
      }
      *((int*)value) = stream->data.file.fd;
      *size = sizeof(int);
#endif
      break;
    }
      
    case SIO_OPT_BLOCKING: {
      if (*size < sizeof(int)) {
        return SIO_ERROR_BUFFER_TOO_SMALL;
      }
      
#if defined(SIO_OS_WINDOWS)
      /* Windows doesn't really have non-blocking file I/O */
      *((int*)value) = 1;
#else
      int flags = fcntl(stream->data.file.fd, F_GETFL);
      if (flags < 0) {
        return sio_get_last_error();
      }
      
      *((int*)value) = ((flags & O_NONBLOCK) == 0) ? 1 : 0;
#endif
      *size = sizeof(int);
      break;
    }
      
    case SIO_OPT_CLOSE_ON_EXEC: {
      if (*size < sizeof(int)) {
        return SIO_ERROR_BUFFER_TOO_SMALL;
      }
      
#if defined(SIO_OS_WINDOWS)
      /* Windows handles are not inherited by default */
      *((int*)value) = 1;
#else
      int flags = fcntl(stream->data.file.fd, F_GETFD);
      if (flags < 0) {
        return sio_get_last_error();
      }
      
      *((int*)value) = ((flags & FD_CLOEXEC) != 0) ? 1 : 0;
#endif
      *size = sizeof(int);
      break;
    }
      
    case SIO_OPT_FILE_APPEND: {
      if (*size < sizeof(int)) {
        return SIO_ERROR_BUFFER_TOO_SMALL;
      }
      
      *((int*)value) = ((stream->flags & SIO_STREAM_APPEND) != 0) ? 1 : 0;
      *size = sizeof(int);
      break;
    }
      
    case SIO_OPT_FILE_SYNC: {
      if (*size < sizeof(int)) {
        return SIO_ERROR_BUFFER_TOO_SMALL;
      }
      
      *((int*)value) = ((stream->flags & SIO_STREAM_SYNC) != 0) ? 1 : 0;
      *size = sizeof(int);
      break;
    }
      
    case SIO_OPT_FILE_DIRECT: {
      if (*size < sizeof(int)) {
        return SIO_ERROR_BUFFER_TOO_SMALL;
      }
      
      *((int*)value) = ((stream->flags & SIO_STREAM_DIRECT) != 0) ? 1 : 0;
      *size = sizeof(int);
      break;
    }
      
    default:
      return SIO_ERROR_UNSUPPORTED;
  }
  
  return SIO_SUCCESS;
}

/**
* @brief Set stream options
*/
static sio_error_t file_set_option(sio_stream_t *stream, sio_stream_option_t option, const void *value, size_t size) {
  assert(stream && stream->type == SIO_STREAM_FILE);
  
  if (!value) {
    return SIO_ERROR_PARAM;
  }
  
  switch (option) {
    case SIO_OPT_BLOCKING: {
      if (size < sizeof(int)) {
        return SIO_ERROR_PARAM;
      }
      
      int blocking = *((const int*)value);
      
#if defined(SIO_OS_WINDOWS)
      /* Windows doesn't support non-blocking file I/O */
      if (!blocking) {
        return SIO_ERROR_UNSUPPORTED;
      }
#else
      int flags = fcntl(stream->data.file.fd, F_GETFL);
      if (flags < 0) {
        return sio_get_last_error();
      }
      
      if (blocking) {
        flags &= ~O_NONBLOCK;
      } else {
        flags |= O_NONBLOCK;
      }
      
      if (fcntl(stream->data.file.fd, F_SETFL, flags) < 0) {
        return sio_get_last_error();
      }
      
      /* Update flags */
      if (blocking) {
        stream->flags &= ~SIO_STREAM_NONBLOCK;
      } else {
        stream->flags |= SIO_STREAM_NONBLOCK;
      }
#endif
      break;
    }
      
    case SIO_OPT_CLOSE_ON_EXEC: {
      if (size < sizeof(int)) {
        return SIO_ERROR_PARAM;
      }
      
      int close_on_exec = *((const int*)value);
      
#if defined(SIO_OS_WINDOWS)
      /* Windows doesn't support FD_CLOEXEC directly */
      HANDLE current_handle = stream->data.file.handle;
      HANDLE new_handle;
      
      DWORD flags = close_on_exec ? 0 : HANDLE_FLAG_INHERIT;
      
      if (!SetHandleInformation(current_handle, HANDLE_FLAG_INHERIT, flags)) {
        return sio_get_last_error();
      }
#else
      int flags = fcntl(stream->data.file.fd, F_GETFD);
      if (flags < 0) {
        return sio_get_last_error();
      }
      
      if (close_on_exec) {
        flags |= FD_CLOEXEC;
      } else {
        flags &= ~FD_CLOEXEC;
      }
      
      if (fcntl(stream->data.file.fd, F_SETFD, flags) < 0) {
        return sio_get_last_error();
      }
#endif
      break;
    }
      
    case SIO_OPT_FILE_SYNC: {
      if (size < sizeof(int)) {
        return SIO_ERROR_PARAM;
      }
      
      int sync = *((const int*)value);
      
      if (sync) {
        stream->flags |= SIO_STREAM_SYNC;
      } else {
        stream->flags &= ~SIO_STREAM_SYNC;
      }
      
#if defined(SIO_OS_WINDOWS)
      /* In Windows, we need to use FlushFileBuffers for synchronous I/O */
#else
      /* On some POSIX systems, we can set O_SYNC flag */
#ifdef O_SYNC
      int flags = fcntl(stream->data.file.fd, F_GETFL);
      if (flags < 0) {
        return sio_get_last_error();
      }
      
      if (sync) {
        flags |= O_SYNC;
      } else {
        flags &= ~O_SYNC;
      }
      
      if (fcntl(stream->data.file.fd, F_SETFL, flags) < 0) {
        return sio_get_last_error();
      }
#endif
#endif
      break;
    }
      
    default:
      return SIO_ERROR_UNSUPPORTED;
  }
  
  return SIO_SUCCESS;
}

/* Specialized file operations */

/**
* @brief Lock a region of a file
*/
sio_error_t sio_file_lock(sio_stream_t *stream, uint64_t offset, uint64_t size, int exclusive, int wait_for_lock) {
  if (!stream || stream->type != SIO_STREAM_FILE) {
    return SIO_ERROR_PARAM;
  }
  
#if defined(SIO_OS_WINDOWS)
  DWORD flags = 0;
  if (exclusive) {
    flags |= LOCKFILE_EXCLUSIVE_LOCK;
  }
  if (!wait_for_lock) {
    flags |= LOCKFILE_FAIL_IMMEDIATELY;
  }
  
  OVERLAPPED overlapped;
  memset(&overlapped, 0, sizeof(overlapped));
  
  /* Set up offset */
  LARGE_INTEGER li;
  li.QuadPart = offset;
  overlapped.Offset = li.LowPart;
  overlapped.OffsetHigh = li.HighPart;
  
  /* Lock the region */
  DWORD size_low, size_high;
  if (size == 0) {
    /* Lock to end of file */
    size_low = size_high = 0xFFFFFFFF;
  } else {
    li.QuadPart = size;
    size_low = li.LowPart;
    size_high = li.HighPart;
  }
  
  if (!LockFileEx(stream->data.file.handle, flags, 0, size_low, size_high, &overlapped)) {
    return sio_get_last_error();
  }
  
  return SIO_SUCCESS;
#else
  /* POSIX implementation */
  struct flock fl;
  memset(&fl, 0, sizeof(fl));
  
  fl.l_type = exclusive ? F_WRLCK : F_RDLCK;
  fl.l_whence = SEEK_SET;
  fl.l_start = offset;
  fl.l_len = size;
  
  int cmd = wait_for_lock ? F_SETLKW : F_SETLK;
  
  if (fcntl(stream->data.file.fd, cmd, &fl) < 0) {
    return sio_get_last_error();
  }
  
  return SIO_SUCCESS;
#endif
}

/**
* @brief Unlock a region of a file
*/
sio_error_t sio_file_unlock(sio_stream_t *stream, uint64_t offset, uint64_t size) {
  if (!stream || stream->type != SIO_STREAM_FILE) {
    return SIO_ERROR_PARAM;
  }
  
#if defined(SIO_OS_WINDOWS)
  OVERLAPPED overlapped;
  memset(&overlapped, 0, sizeof(overlapped));
  
  /* Set up offset */
  LARGE_INTEGER li;
  li.QuadPart = offset;
  overlapped.Offset = li.LowPart;
  overlapped.OffsetHigh = li.HighPart;
  
  /* Unlock the region */
  DWORD size_low, size_high;
  if (size == 0) {
    /* Unlock to end of file */
    size_low = size_high = 0xFFFFFFFF;
  } else {
    li.QuadPart = size;
    size_low = li.LowPart;
    size_high = li.HighPart;
  }
  
  if (!UnlockFileEx(stream->data.file.handle, 0, size_low, size_high, &overlapped)) {
    return sio_get_last_error();
  }
  
  return SIO_SUCCESS;
#else
  /* POSIX implementation */
  struct flock fl;
  memset(&fl, 0, sizeof(fl));
  
  fl.l_type = F_UNLCK;
  fl.l_whence = SEEK_SET;
  fl.l_start = offset;
  fl.l_len = size;
  
  if (fcntl(stream->data.file.fd, F_SETLK, &fl) < 0) {
    return sio_get_last_error();
  }
  
  return SIO_SUCCESS;
#endif
}