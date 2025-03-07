/**
* @file sio/buf.c
* @brief Simple I/O (SIO) - Implementation of the buffer system
*
* @author zczxy
* @version 0.1.0
*/

#include <sio/buf.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#if defined(SIO_OS_POSIX)
  #include <sys/mman.h>
  #include <fcntl.h>
  #include <unistd.h>
  #include <errno.h>
#elif defined(SIO_OS_WINDOWS)
  #include <windows.h>
#endif

/**
* @brief Align a size to the required memory alignment
*
* @param size Size to align
* @return size_t Aligned size
*/
static SIO_INLINE size_t sio_align_size(size_t size) {
  return (size + SIO_BUFFER_ALIGNMENT - 1) & ~(SIO_BUFFER_ALIGNMENT - 1);
}

/**
* @brief Allocate aligned memory
*
* @param size Size to allocate
* @return void* Allocated memory or NULL on failure
*/
static void *sio_aligned_alloc(size_t size) {
  size_t aligned_size = sio_align_size(size);
  
#if defined(SIO_OS_POSIX)
  void *ptr;
  #if defined(_POSIX_C_SOURCE) && _POSIX_C_SOURCE >= 200112L
    if (posix_memalign(&ptr, SIO_BUFFER_ALIGNMENT, aligned_size) != 0) {
      return NULL;
    }
    return ptr;
  #else
    /* Fallback for older POSIX systems */
    ptr = malloc(aligned_size + SIO_BUFFER_ALIGNMENT);
    if (!ptr) {
      return NULL;
    }
    void *aligned_ptr = (void*)(((uintptr_t)ptr + SIO_BUFFER_ALIGNMENT) & ~(SIO_BUFFER_ALIGNMENT - 1));
    ((void**)aligned_ptr)[-1] = ptr;
    return aligned_ptr;
  #endif
#elif defined(SIO_OS_WINDOWS)
  return _aligned_malloc(aligned_size, SIO_BUFFER_ALIGNMENT);
#else
  /* Simple fallback for other platforms */
  return malloc(aligned_size);
#endif
}

/**
* @brief Free aligned memory
*
* @param ptr Memory to free
*/
static void sio_aligned_free(void *ptr) {
  if (!ptr) {
    return;
  }
  
#if defined(SIO_OS_POSIX)
  #if defined(_POSIX_C_SOURCE) && _POSIX_C_SOURCE >= 200112L
    free(ptr);
  #else
    /* Fallback for older POSIX systems */
    free(((void**)ptr)[-1]);
  #endif
#elif defined(SIO_OS_WINDOWS)
  _aligned_free(ptr);
#else
  /* Simple fallback for other platforms */
  free(ptr);
#endif
}

/**
* @brief Calculate new capacity based on growth strategy
*
* @param buffer Buffer containing growth strategy
* @param min_capacity Minimum required capacity
* @return size_t New capacity
*/
static size_t sio_calculate_new_capacity(const sio_buffer_t *buffer, size_t min_capacity) {
  size_t new_capacity = buffer->capacity;
  
  switch (buffer->growth_strategy) {
    case SIO_BUFFER_GROWTH_FIXED:
      new_capacity = min_capacity;
      break;
      
    case SIO_BUFFER_GROWTH_DOUBLE:
      while (new_capacity < min_capacity) {
        /* Check for overflow */
        if (new_capacity > SIO_BUFFER_MAX_SIZE / 2) {
          new_capacity = min_capacity;
          break;
        }
        new_capacity *= 2;
      }
      break;
      
    case SIO_BUFFER_GROWTH_LINEAR:
      while (new_capacity < min_capacity) {
        /* Check for overflow */
        if (new_capacity > SIO_BUFFER_MAX_SIZE - buffer->growth_factor) {
          new_capacity = min_capacity;
          break;
        }
        new_capacity += buffer->growth_factor;
      }
      break;
      
    case SIO_BUFFER_GROWTH_OPTIMAL:
      /* For optimal growth, we use a hybrid approach that grows
         more aggressively at smaller sizes and less at larger sizes */
      while (new_capacity < min_capacity) {
        /* For small buffers, grow by doubling */
        if (new_capacity < 65536) {
          if (new_capacity > SIO_BUFFER_MAX_SIZE / 2) {
            new_capacity = min_capacity;
            break;
          }
          new_capacity *= 2;
        } else {
          /* For larger buffers, grow by 50% */
          if (new_capacity > SIO_BUFFER_MAX_SIZE - (new_capacity / 2)) {
            new_capacity = min_capacity;
            break;
          }
          new_capacity += new_capacity / 2;
        }
      }
      break;
  }
  
  /* Ensure the capacity is at least the minimum requested */
  return new_capacity < min_capacity ? min_capacity : new_capacity;
}

sio_error_t sio_buffer_create(sio_buffer_t *buffer, size_t initial_capacity) {
  return sio_buffer_create_ex(buffer, initial_capacity, 
                              SIO_BUFFER_GROWTH_OPTIMAL, 0);
}

sio_error_t sio_buffer_create_ex(sio_buffer_t *buffer, size_t initial_capacity, 
                              sio_buffer_growth_strategy_t growth_strategy, 
                              size_t growth_factor) {
  if (!buffer) {
    return SIO_ERROR_PARAM;
  }
  
  /* Set up default values */
  memset(buffer, 0, sizeof(sio_buffer_t));
  buffer->growth_strategy = growth_strategy;
  buffer->growth_factor = growth_factor;
  buffer->owns_memory = 1;
  
  /* Use default size if none specified */
  if (initial_capacity == 0) {
    initial_capacity = SIO_BUFFER_DEFAULT_SIZE;
  }
  
  /* Align the initial capacity */
  initial_capacity = sio_align_size(initial_capacity);
  
  /* Allocate the buffer */
  buffer->data = (uint8_t*)sio_aligned_alloc(initial_capacity);
  if (!buffer->data) {
    return SIO_ERROR_MEM;
  }
  
  buffer->capacity = initial_capacity;
  return SIO_SUCCESS;
}

sio_error_t sio_buffer_from_memory(sio_buffer_t *buffer, void *data, size_t size) {
  if (!buffer || !data) {
    return SIO_ERROR_PARAM;
  }
  
  /* Set up the buffer structure */
  memset(buffer, 0, sizeof(sio_buffer_t));
  buffer->data = (uint8_t*)data;
  buffer->size = size;
  buffer->capacity = size;
  buffer->owns_memory = 0; /* Does not own the memory */
  buffer->growth_strategy = SIO_BUFFER_GROWTH_FIXED; /* Fixed size since we don't own the memory */
  
  return SIO_SUCCESS;
}

sio_error_t sio_buffer_mmap_file(sio_buffer_t *buffer, const char *filepath, int read_only) {
  if (!buffer || !filepath) {
    return SIO_ERROR_PARAM;
  }
  
  /* Initialize the buffer structure */
  memset(buffer, 0, sizeof(sio_buffer_t));
  buffer->is_mmap = 1;
  buffer->owns_memory = 1; /* We'll handle unmapping */
  buffer->growth_strategy = SIO_BUFFER_GROWTH_FIXED; /* Memory mapped files have fixed size */
  
#if defined(SIO_OS_POSIX)
  int flags = read_only ? O_RDONLY : O_RDWR;
  int fd = open(filepath, flags);
  if (fd == -1) {
    return sio_posix_error_to_sio_error(errno);
  }
  
  /* Get the file size */
  off_t file_size = lseek(fd, 0, SEEK_END);
  if (file_size == -1) {
    close(fd);
    return sio_posix_error_to_sio_error(errno);
  }
  
  /* Return to the start of the file */
  if (lseek(fd, 0, SEEK_SET) == -1) {
    close(fd);
    return sio_posix_error_to_sio_error(errno);
  }
  
  /* Map the file */
  int prot = read_only ? PROT_READ : (PROT_READ | PROT_WRITE);
  void *mapped = mmap(NULL, (size_t)file_size, prot, MAP_SHARED, fd, 0);
  close(fd); /* We can close the file descriptor after mapping */
  
  if (mapped == MAP_FAILED) {
    return sio_posix_error_to_sio_error(errno);
  }
  
  buffer->data = (uint8_t*)mapped;
  buffer->size = (size_t)file_size;
  buffer->capacity = (size_t)file_size;
  
  return SIO_SUCCESS;
  
#elif defined(SIO_OS_WINDOWS)
  /* Open the file */
  DWORD access = read_only ? GENERIC_READ : (GENERIC_READ | GENERIC_WRITE);
  DWORD share_mode = FILE_SHARE_READ;
  HANDLE file_handle = CreateFileA(filepath, access, share_mode, NULL, OPEN_EXISTING, 
                                  FILE_ATTRIBUTE_NORMAL, NULL);
  
  if (file_handle == INVALID_HANDLE_VALUE) {
    return sio_win_error_to_sio_error(GetLastError());
  }
  
  /* Get the file size */
  LARGE_INTEGER file_size;
  if (!GetFileSizeEx(file_handle, &file_size)) {
    CloseHandle(file_handle);
    return sio_win_error_to_sio_error(GetLastError());
  }
  
  /* Create a file mapping object */
  DWORD protect = read_only ? PAGE_READONLY : PAGE_READWRITE;
  HANDLE mapping_handle = CreateFileMappingA(file_handle, NULL, protect, 
                                             file_size.HighPart, file_size.LowPart, NULL);
  
  if (!mapping_handle) {
    CloseHandle(file_handle);
    return sio_win_error_to_sio_error(GetLastError());
  }
  
  /* Map the file */
  DWORD map_access = read_only ? FILE_MAP_READ : FILE_MAP_ALL_ACCESS;
  void *mapped = MapViewOfFile(mapping_handle, map_access, 0, 0, (SIZE_T)file_size.QuadPart);
  
  /* Close handles since we don't need them anymore */
  CloseHandle(mapping_handle);
  CloseHandle(file_handle);
  
  if (!mapped) {
    return sio_win_error_to_sio_error(GetLastError());
  }
  
  buffer->data = (uint8_t*)mapped;
  buffer->size = (size_t)file_size.QuadPart;
  buffer->capacity = (size_t)file_size.QuadPart;
  
  return SIO_SUCCESS;
#else
  return SIO_ERROR_UNSUPPORTED;
#endif
}

sio_error_t sio_buffer_destroy(sio_buffer_t *buffer) {
  if (!buffer) {
    return SIO_ERROR_PARAM;
  }
  
  if (buffer->data && buffer->owns_memory) {
    if (buffer->is_mmap) {
#if defined(SIO_OS_POSIX)
      if (munmap(buffer->data, buffer->capacity) != 0) {
        return sio_posix_error_to_sio_error(errno);
      }
#elif defined(SIO_OS_WINDOWS)
      if (!UnmapViewOfFile(buffer->data)) {
        return sio_win_error_to_sio_error(GetLastError());
      }
#else
      return SIO_ERROR_UNSUPPORTED;
#endif
    } else {
      sio_aligned_free(buffer->data);
    }
  }
  
  /* Clear the buffer structure */
  memset(buffer, 0, sizeof(sio_buffer_t));
  
  return SIO_SUCCESS;
}

sio_error_t sio_buffer_reserve(sio_buffer_t *buffer, size_t additional_capacity) {
  if (!buffer) {
    return SIO_ERROR_PARAM;
  }
  
  /* Check if we need to grow */
  if (buffer->capacity - buffer->size >= additional_capacity) {
    return SIO_SUCCESS; /* Already have enough capacity */
  }
  
  /* Calculate required capacity */
  size_t required_capacity = buffer->size + additional_capacity;
  
  /* Check for overflow */
  if (required_capacity < buffer->size) {
    return SIO_ERROR_BUFFER_TOO_SMALL;
  }
  
  return sio_buffer_resize(buffer, required_capacity);
}

sio_error_t sio_buffer_ensure_capacity(sio_buffer_t *buffer, size_t min_capacity) {
  if (!buffer) {
    return SIO_ERROR_PARAM;
  }
  
  /* Check if we already have enough capacity */
  if (buffer->capacity >= min_capacity) {
    return SIO_SUCCESS;
  }
  
  return sio_buffer_resize(buffer, min_capacity);
}

sio_error_t sio_buffer_resize(sio_buffer_t *buffer, size_t new_capacity) {
  if (!buffer) {
    return SIO_ERROR_PARAM;
  }
  
  /* Cannot resize if we don't own the memory or if it's memory-mapped */
  if (!buffer->owns_memory || buffer->is_mmap) {
    return SIO_ERROR_FILE_READONLY;
  }
  
  /* Align the new capacity */
  new_capacity = sio_align_size(new_capacity);
  
  /* Reallocate the buffer */
  uint8_t *new_data;
  
#if defined(SIO_OS_POSIX)
  #if defined(_POSIX_C_SOURCE) && _POSIX_C_SOURCE >= 200112L
    /* Try to reallocate in-place if possible */
    void *ptr = buffer->data;
    if (posix_memalign(&ptr, SIO_BUFFER_ALIGNMENT, new_capacity) != 0) {
      return SIO_ERROR_MEM;
    }
    new_data = (uint8_t*)ptr;
    
    /* If we got a new pointer, copy the data and free the old buffer */
    if (new_data != buffer->data) {
      memcpy(new_data, buffer->data, buffer->size < new_capacity ? buffer->size : new_capacity);
      free(buffer->data);
    }
  #else
    /* Simple reallocation for older POSIX systems */
    new_data = (uint8_t*)sio_aligned_alloc(new_capacity);
    if (!new_data) {
      return SIO_ERROR_MEM;
    }
    
    /* Copy data to new buffer */
    memcpy(new_data, buffer->data, buffer->size < new_capacity ? buffer->size : new_capacity);
    sio_aligned_free(buffer->data);
  #endif
#elif defined(SIO_OS_WINDOWS)
  /* _aligned_realloc copies the data for us */
  new_data = (uint8_t*)_aligned_realloc(buffer->data, new_capacity, SIO_BUFFER_ALIGNMENT);
  if (!new_data) {
    return SIO_ERROR_MEM;
  }
#else
  /* Simple reallocation for other platforms */
  new_data = (uint8_t*)realloc(buffer->data, new_capacity);
  if (!new_data) {
    return SIO_ERROR_MEM;
  }
#endif
  
  buffer->data = new_data;
  buffer->capacity = new_capacity;
  
  /* If the new capacity is smaller than the current size, adjust size */
  if (new_capacity < buffer->size) {
    buffer->size = new_capacity;
    /* Adjust position if it's now beyond the end of the buffer */
    if (buffer->position > buffer->size) {
      buffer->position = buffer->size;
    }
  }
  
  return SIO_SUCCESS;
}

sio_error_t sio_buffer_shrink_to_fit(sio_buffer_t *buffer) {
  if (!buffer) {
    return SIO_ERROR_PARAM;
  }
  
  /* Cannot resize if we don't own the memory or if it's memory-mapped */
  if (!buffer->owns_memory || buffer->is_mmap) {
    return SIO_ERROR_FILE_READONLY;
  }
  
  /* No need to shrink if size matches capacity */
  if (buffer->size == buffer->capacity) {
    return SIO_SUCCESS;
  }
  
  /* Resize to the current size */
  return sio_buffer_resize(buffer, buffer->size);
}

sio_error_t sio_buffer_write(sio_buffer_t *buffer, const void *data, size_t size) {
  if (!buffer || (!data && size > 0)) {
    return SIO_ERROR_PARAM;
  }
  
  /* Check if the buffer is read-only (memory-mapped file opened read-only) */
  if (buffer->is_mmap && !buffer->owns_memory) {
    return SIO_ERROR_FILE_READONLY;
  }
  
  /* Calculate the new size after write */
  size_t new_size = buffer->position + size;
  
  /* Check for overflow */
  if (new_size < buffer->position) {
    return SIO_ERROR_BUFFER_TOO_SMALL;
  }
  
  /* Ensure we have enough capacity */
  if (new_size > buffer->capacity) {
    size_t new_capacity = sio_calculate_new_capacity(buffer, new_size);
    sio_error_t err = sio_buffer_resize(buffer, new_capacity);
    if (err != SIO_SUCCESS) {
      return err;
    }
  }
  
  /* Copy the data */
  if (size > 0) {
    memcpy(buffer->data + buffer->position, data, size);
    buffer->position += size;
  }
  
  /* Update the size if needed */
  if (buffer->position > buffer->size) {
    buffer->size = buffer->position;
  }
  
  return SIO_SUCCESS;
}

sio_error_t sio_buffer_read(sio_buffer_t *buffer, void *data, size_t size, size_t *bytes_read) {
  if (!buffer || (!data && size > 0)) {
    return SIO_ERROR_PARAM;
  }
  
  /* Calculate how many bytes we can actually read */
  size_t available = buffer->size - buffer->position;
  size_t to_read = size < available ? size : available;
  
  /* Copy the data */
  if (to_read > 0) {
    memcpy(data, buffer->data + buffer->position, to_read);
    buffer->position += to_read;
  }
  
  /* Set bytes_read if provided */
  if (bytes_read) {
    *bytes_read = to_read;
  }
  
  /* Return EOF if we couldn't read all requested bytes */
  return (to_read < size) ? SIO_ERROR_EOF : SIO_SUCCESS;
}

sio_error_t sio_buffer_seek(sio_buffer_t *buffer, size_t position) {
  if (!buffer) {
    return SIO_ERROR_PARAM;
  }
  
  /* Check if position is valid */
  if (position > buffer->size) {
    return SIO_ERROR_PARAM;
  }
  
  buffer->position = position;
  return SIO_SUCCESS;
}

sio_error_t sio_buffer_seek_relative(sio_buffer_t *buffer, int64_t offset) {
  if (!buffer) {
    return SIO_ERROR_PARAM;
  }
  
  /* Calculate new position with bounds checking */
  int64_t new_pos;
  if (offset < 0) {
    /* Moving backward */
    if ((size_t)(-offset) > buffer->position) {
      /* Cannot seek before start of buffer */
      return SIO_ERROR_PARAM;
    }
    new_pos = (int64_t)buffer->position + offset;
  } else {
    /* Moving forward */
    new_pos = (int64_t)buffer->position + offset;
    if ((size_t)new_pos > buffer->size) {
      /* Cannot seek past end of buffer */
      return SIO_ERROR_PARAM;
    }
  }
  
  buffer->position = (size_t)new_pos;
  return SIO_SUCCESS;
}

size_t sio_buffer_tell(const sio_buffer_t *buffer) {
  return buffer ? buffer->position : 0;
}

sio_error_t sio_buffer_clear(sio_buffer_t *buffer) {
  if (!buffer) {
    return SIO_ERROR_PARAM;
  }
  
  buffer->size = 0;
  buffer->position = 0;
  return SIO_SUCCESS;
}

void *sio_buffer_current_ptr(const sio_buffer_t *buffer) {
  return buffer ? (buffer->data + buffer->position) : NULL;
}

size_t sio_buffer_remaining(const sio_buffer_t *buffer) {
  return buffer ? (buffer->size - buffer->position) : 0;
}

int sio_buffer_at_end(const sio_buffer_t *buffer) {
  return buffer ? (buffer->position >= buffer->size) : 1;
}

sio_error_t sio_buffer_copy(sio_buffer_t *dest, const sio_buffer_t *src) {
  if (!dest || !src) {
    return SIO_ERROR_PARAM;
  }
  
  /* Create a new buffer with the same size as the source */
  sio_error_t err = sio_buffer_create(dest, src->size);
  if (err != SIO_SUCCESS) {
    return err;
  }
  
  /* Copy the data */
  memcpy(dest->data, src->data, src->size);
  dest->size = src->size;
  dest->position = 0; /* Reset position to start */
  
  return SIO_SUCCESS;
}

void *sio_buffer_data(const sio_buffer_t *buffer) {
  return buffer ? buffer->data : NULL;
}

/* Integer type read/write functions */

sio_error_t sio_buffer_write_uint8(sio_buffer_t *buffer, uint8_t value) {
  return sio_buffer_write(buffer, &value, sizeof(value));
}

sio_error_t sio_buffer_write_uint16(sio_buffer_t *buffer, uint16_t value) {
  return sio_buffer_write(buffer, &value, sizeof(value));
}

sio_error_t sio_buffer_write_uint32(sio_buffer_t *buffer, uint32_t value) {
  return sio_buffer_write(buffer, &value, sizeof(value));
}

sio_error_t sio_buffer_write_uint64(sio_buffer_t *buffer, uint64_t value) {
  return sio_buffer_write(buffer, &value, sizeof(value));
}

sio_error_t sio_buffer_read_uint8(sio_buffer_t *buffer, uint8_t *value) {
  size_t bytes_read;
  sio_error_t err = sio_buffer_read(buffer, value, sizeof(*value), &bytes_read);
  
  /* Only return success if we read the entire value */
  return (err == SIO_SUCCESS && bytes_read == sizeof(*value)) ? SIO_SUCCESS : SIO_ERROR_EOF;
}

sio_error_t sio_buffer_read_uint16(sio_buffer_t *buffer, uint16_t *value) {
  size_t bytes_read;
  sio_error_t err = sio_buffer_read(buffer, value, sizeof(*value), &bytes_read);
  
  /* Only return success if we read the entire value */
  return (err == SIO_SUCCESS && bytes_read == sizeof(*value)) ? SIO_SUCCESS : SIO_ERROR_EOF;
}

sio_error_t sio_buffer_read_uint32(sio_buffer_t *buffer, uint32_t *value) {
  size_t bytes_read;
  sio_error_t err = sio_buffer_read(buffer, value, sizeof(*value), &bytes_read);
  
  /* Only return success if we read the entire value */
  return (err == SIO_SUCCESS && bytes_read == sizeof(*value)) ? SIO_SUCCESS : SIO_ERROR_EOF;
}

sio_error_t sio_buffer_read_uint64(sio_buffer_t *buffer, uint64_t *value) {
  size_t bytes_read;
  sio_error_t err = sio_buffer_read(buffer, value, sizeof(*value), &bytes_read);
  
  /* Only return success if we read the entire value */
  return (err == SIO_SUCCESS && bytes_read == sizeof(*value)) ? SIO_SUCCESS : SIO_ERROR_EOF;
}

/* Buffer pool implementation */

sio_error_t sio_buffer_pool_create(sio_buffer_pool_t *pool, size_t buffer_count, size_t buffer_size) {
  if (!pool || buffer_count == 0 || buffer_size == 0) {
    return SIO_ERROR_PARAM;
  }
  
  /* Initialize pool structure */
  memset(pool, 0, sizeof(sio_buffer_pool_t));
  
  /* Allocate buffer array */
  pool->buffers = (sio_buffer_t*)malloc(buffer_count * sizeof(sio_buffer_t));
  if (!pool->buffers) {
    return SIO_ERROR_MEM;
  }
  
  /* Allocate used flags array */
  pool->used_flags = (int*)calloc(buffer_count, sizeof(int));
  if (!pool->used_flags) {
    free(pool->buffers);
    return SIO_ERROR_MEM;
  }
  
  /* Initialize each buffer */
  for (size_t i = 0; i < buffer_count; i++) {
    sio_error_t err = sio_buffer_create(&pool->buffers[i], buffer_size);
    if (err != SIO_SUCCESS) {
      /* Clean up already created buffers */
      for (size_t j = 0; j < i; j++) {
        sio_buffer_destroy(&pool->buffers[j]);
      }
      free(pool->used_flags);
      free(pool->buffers);
      return err;
    }
  }
  
  pool->capacity = buffer_count;
  pool->buffer_size = buffer_size;
  pool->size = 0; /* No buffers in use initially */
  
  return SIO_SUCCESS;
}

sio_error_t sio_buffer_pool_destroy(sio_buffer_pool_t *pool) {
  if (!pool) {
    return SIO_ERROR_PARAM;
  }
  
  /* Destroy all buffers */
  if (pool->buffers) {
    for (size_t i = 0; i < pool->capacity; i++) {
      sio_buffer_destroy(&pool->buffers[i]);
    }
    free(pool->buffers);
  }
  
  /* Free the used flags array */
  if (pool->used_flags) {
    free(pool->used_flags);
  }
  
  /* Clear the pool structure */
  memset(pool, 0, sizeof(sio_buffer_pool_t));
  
  return SIO_SUCCESS;
}

sio_error_t sio_buffer_pool_acquire(sio_buffer_pool_t *pool, sio_buffer_t **buffer) {
  if (!pool || !buffer) {
    return SIO_ERROR_PARAM;
  }
  
  *buffer = NULL; /* Initialize to NULL in case of failure */
  
  /* Find an unused buffer */
  for (size_t i = 0; i < pool->capacity; i++) {
    if (!pool->used_flags[i]) {
      /* Found an unused buffer */
      pool->used_flags[i] = 1;
      *buffer = &pool->buffers[i];
      
      /* Clear the buffer for reuse */
      sio_buffer_clear(*buffer);
      
      pool->size++;
      return SIO_SUCCESS;
    }
  }
  
  /* No available buffers */
  return SIO_ERROR_BUSY;
}

sio_error_t sio_buffer_pool_release(sio_buffer_pool_t *pool, sio_buffer_t *buffer) {
  if (!pool || !buffer) {
    return SIO_ERROR_PARAM;
  }
  
  /* Find this buffer in the pool */
  for (size_t i = 0; i < pool->capacity; i++) {
    if (&pool->buffers[i] == buffer) {
      /* Mark as unused */
      if (pool->used_flags[i]) {
        pool->used_flags[i] = 0;
        pool->size--;
        return SIO_SUCCESS;
      } else {
        /* Buffer was already released */
        return SIO_ERROR_FILE_CLOSED;
      }
    }
  }
  
  /* Buffer not found in this pool */
  return SIO_ERROR_PARAM;
}

sio_error_t sio_buffer_pool_resize(sio_buffer_pool_t *pool, size_t new_buffer_count) {
  if (!pool) {
    return SIO_ERROR_PARAM;
  }
  
  /* Can't shrink below the number of buffers in use */
  if (new_buffer_count < pool->size) {
    return SIO_ERROR_BUSY;
  }
  
  if (new_buffer_count == pool->capacity) {
    return SIO_SUCCESS; /* No change needed */
  }
  
  /* Allocate new arrays */
  sio_buffer_t *new_buffers = (sio_buffer_t*)malloc(new_buffer_count * sizeof(sio_buffer_t));
  if (!new_buffers) {
    return SIO_ERROR_MEM;
  }
  
  int *new_used_flags = (int*)calloc(new_buffer_count, sizeof(int));
  if (!new_used_flags) {
    free(new_buffers);
    return SIO_ERROR_MEM;
  }
  
  /* Copy existing buffers */
  size_t copy_count = new_buffer_count < pool->capacity ? new_buffer_count : pool->capacity;
  
  for (size_t i = 0; i < copy_count; i++) {
    /* Copy buffer and used flag */
    memcpy(&new_buffers[i], &pool->buffers[i], sizeof(sio_buffer_t));
    new_used_flags[i] = pool->used_flags[i];
    
    /* Reset the old buffer so it won't be destroyed */
    memset(&pool->buffers[i], 0, sizeof(sio_buffer_t));
  }
  
  /* Initialize any new buffers */
  for (size_t i = copy_count; i < new_buffer_count; i++) {
    sio_error_t err = sio_buffer_create(&new_buffers[i], pool->buffer_size);
    if (err != SIO_SUCCESS) {
      /* Clean up new buffers we've created */
      for (size_t j = copy_count; j < i; j++) {
        sio_buffer_destroy(&new_buffers[j]);
      }
      
      /* Restore original buffers and clean up */
      for (size_t j = 0; j < copy_count; j++) {
        memcpy(&pool->buffers[j], &new_buffers[j], sizeof(sio_buffer_t));
        memset(&new_buffers[j], 0, sizeof(sio_buffer_t));
      }
      
      free(new_used_flags);
      free(new_buffers);
      return err;
    }
  }
  
  /* Destroy any excess buffers */
  for (size_t i = copy_count; i < pool->capacity; i++) {
    sio_buffer_destroy(&pool->buffers[i]);
  }
  
  /* Update the pool */
  free(pool->used_flags);
  free(pool->buffers);
  
  pool->buffers = new_buffers;
  pool->used_flags = new_used_flags;
  pool->capacity = new_buffer_count;
  
  return SIO_SUCCESS;
}