/**
* @file sio/buf.h
* @brief Simple I/O (SIO) - Cross-platform I/O library for high-performance systems programming
*
* A simple buffer system built ontop of direct memory allocation functions from the operating system.
*
* @author zczxy
* @version 0.1.0
*/

#ifndef SIO_BUF_H
#define SIO_BUF_H

#ifdef __cplusplus
extern "C" {
#endif

#include <sio/platform.h>
#include <sio/err.h>
#include <stddef.h>
#include <stdint.h>

/**
* @brief Default buffer sizes and constants
*/
#define SIO_BUFFER_DEFAULT_SIZE 4096  /**< Default initial buffer size */
#define SIO_BUFFER_MAX_SIZE (SIZE_MAX) /**< Maximum buffer size */
#define SIO_BUFFER_ALIGNMENT SIO_MEMORY_ALIGNMENT /**< Buffer alignment requirement */

/**
* @brief Buffer growth strategy enumeration
*/
enum sio_buffer_growth_strategy {
  SIO_BUFFER_GROWTH_FIXED,      /**< Fixed size, no automatic growth */
  SIO_BUFFER_GROWTH_DOUBLE,     /**< Double capacity when needed */
  SIO_BUFFER_GROWTH_LINEAR,     /**< Add fixed amount when needed */
  SIO_BUFFER_GROWTH_OPTIMAL     /**< Use platform-optimal strategy */
};

typedef enum sio_buffer_growth_strategy sio_buffer_growth_strategy_t;

/**
* @brief Buffer structure for memory management
*/
typedef struct sio_buffer {
  uint8_t *data;        /**< Pointer to buffer data */
  size_t size;          /**< Current size (used bytes) of the buffer */
  size_t capacity;      /**< Total capacity (allocated bytes) of the buffer */
  size_t position;      /**< Current read/write position */
  
  /* Internal fields, not to be modified directly */
  int owns_memory;      /**< Whether the buffer owns the memory (should free on destroy) */
  int is_mmap;          /**< Whether the buffer is memory-mapped */
  sio_buffer_growth_strategy_t growth_strategy; /**< Strategy used for buffer growth */
  size_t growth_factor; /**< Factor used for linear growth strategy */
} sio_buffer_t;

/**
* @brief Create a new buffer with the specified initial capacity
*
* @param buffer Pointer to a buffer structure to initialize
* @param initial_capacity Initial capacity in bytes (0 for default)
* @return sio_error_t SIO_SUCCESS or error code
*/
SIO_EXPORT sio_error_t sio_buffer_create(sio_buffer_t *buffer, size_t initial_capacity);

/**
* @brief Extended buffer creation with growth strategy
*
* @param buffer Pointer to a buffer structure to initialize
* @param initial_capacity Initial capacity in bytes (0 for default)
* @param growth_strategy Growth strategy to use
* @param growth_factor For linear growth, the amount to add each time; for others, ignored
* @return sio_error_t SIO_SUCCESS or error code
*/
SIO_EXPORT sio_error_t sio_buffer_create_ex(sio_buffer_t *buffer, size_t initial_capacity, 
                              sio_buffer_growth_strategy_t growth_strategy, 
                              size_t growth_factor);

/**
* @brief Create a buffer from an existing memory block (does not take ownership)
*
* @param buffer Pointer to a buffer structure to initialize
* @param data Pointer to existing memory
* @param size Size of the existing memory
* @return sio_error_t SIO_SUCCESS or error code
*/
SIO_EXPORT sio_error_t sio_buffer_from_memory(sio_buffer_t *buffer, void *data, size_t size);

/**
* @brief Memory-map a file to a buffer
*
* @param buffer Pointer to a buffer structure to initialize
* @param filepath Path to the file to map
* @param read_only Non-zero for read-only mapping, 0 for read-write
* @return sio_error_t SIO_SUCCESS or error code
*/
SIO_EXPORT sio_error_t sio_buffer_mmap_file(sio_buffer_t *buffer, const char *filepath, int read_only);

/**
* @brief Destroy a buffer and free its resources
*
* @param buffer Pointer to the buffer to destroy
* @return sio_error_t SIO_SUCCESS or error code
*/
SIO_EXPORT sio_error_t sio_buffer_destroy(sio_buffer_t *buffer);

/**
* @brief Reserve additional capacity for the buffer
*
* @param buffer Pointer to the buffer
* @param additional_capacity Additional capacity to reserve in bytes
* @return sio_error_t SIO_SUCCESS or error code
*/
SIO_EXPORT sio_error_t sio_buffer_reserve(sio_buffer_t *buffer, size_t additional_capacity);

/**
* @brief Ensure the buffer has at least the specified capacity
*
* @param buffer Pointer to the buffer
* @param min_capacity Minimum capacity required
* @return sio_error_t SIO_SUCCESS or error code
*/
SIO_EXPORT sio_error_t sio_buffer_ensure_capacity(sio_buffer_t *buffer, size_t min_capacity);

/**
* @brief Resize the buffer to exactly the specified capacity
*
* @param buffer Pointer to the buffer
* @param new_capacity New capacity in bytes
* @return sio_error_t SIO_SUCCESS or error code
*/
SIO_EXPORT sio_error_t sio_buffer_resize(sio_buffer_t *buffer, size_t new_capacity);

/**
* @brief Shrink the buffer capacity to match its size
*
* @param buffer Pointer to the buffer
* @return sio_error_t SIO_SUCCESS or error code
*/
SIO_EXPORT sio_error_t sio_buffer_shrink_to_fit(sio_buffer_t *buffer);

/**
* @brief Write data to the buffer at the current position
*
* @param buffer Pointer to the buffer
* @param data Pointer to the data to write
* @param size Size of the data in bytes
* @return sio_error_t SIO_SUCCESS or error code
*/
SIO_EXPORT sio_error_t sio_buffer_write(sio_buffer_t *buffer, const void *data, size_t size);

/**
* @brief Read data from the buffer at the current position
*
* @param buffer Pointer to the buffer
* @param data Pointer to the destination memory
* @param size Maximum number of bytes to read
* @param bytes_read Pointer to store the actual number of bytes read (can be NULL)
* @return sio_error_t SIO_SUCCESS or error code
*/
SIO_EXPORT sio_error_t sio_buffer_read(sio_buffer_t *buffer, void *data, size_t size, size_t *bytes_read);

/**
* @brief Set the current position in the buffer
*
* @param buffer Pointer to the buffer
* @param position New position
* @return sio_error_t SIO_SUCCESS or error code
*/
SIO_EXPORT sio_error_t sio_buffer_seek(sio_buffer_t *buffer, size_t position);

/**
* @brief Move the current position relative to the current position
*
* @param buffer Pointer to the buffer
* @param offset Offset to move (can be negative)
* @return sio_error_t SIO_SUCCESS or error code
*/
SIO_EXPORT sio_error_t sio_buffer_seek_relative(sio_buffer_t *buffer, int64_t offset);

/**
* @brief Get the current position in the buffer
*
* @param buffer Pointer to the buffer
* @return size_t Current position
*/
size_t sio_buffer_tell(const sio_buffer_t *buffer);

/**
* @brief Clear the buffer (reset size and position to 0)
*
* @param buffer Pointer to the buffer
* @return sio_error_t SIO_SUCCESS or error code
*/
SIO_EXPORT sio_error_t sio_buffer_clear(sio_buffer_t *buffer);

/**
* @brief Get a pointer to the buffer data at the current position
*
* @param buffer Pointer to the buffer
* @return void* Pointer to the current position in the buffer data
*/
void *sio_buffer_current_ptr(const sio_buffer_t *buffer);

/**
* @brief Get the remaining bytes available for reading
*
* @param buffer Pointer to the buffer
* @return size_t Number of bytes available
*/
size_t sio_buffer_remaining(const sio_buffer_t *buffer);

/**
* @brief Check if the buffer has reached the end
*
* @param buffer Pointer to the buffer
* @return int Non-zero if at end, 0 otherwise
*/
int sio_buffer_at_end(const sio_buffer_t *buffer);

/**
* @brief Create a new buffer with a copy of the source buffer's content
*
* @param dest Pointer to the destination buffer structure
* @param src Pointer to the source buffer
* @return sio_error_t SIO_SUCCESS or error code
*/
SIO_EXPORT sio_error_t sio_buffer_copy(sio_buffer_t *dest, const sio_buffer_t *src);

/**
* @brief Get direct pointer to buffer data
* 
* @param buffer Pointer to the buffer
* @return void* Pointer to the buffer data
*/
void *sio_buffer_data(const sio_buffer_t *buffer);

/**
* @brief Write a uint8_t value to the buffer
*
* @param buffer Pointer to the buffer
* @param value Value to write
* @return sio_error_t SIO_SUCCESS or error code
*/
SIO_EXPORT sio_error_t sio_buffer_write_uint8(sio_buffer_t *buffer, uint8_t value);

/**
* @brief Write a uint16_t value to the buffer
*
* @param buffer Pointer to the buffer
* @param value Value to write
* @return sio_error_t SIO_SUCCESS or error code
*/
SIO_EXPORT sio_error_t sio_buffer_write_uint16(sio_buffer_t *buffer, uint16_t value);

/**
* @brief Write a uint32_t value to the buffer
*
* @param buffer Pointer to the buffer
* @param value Value to write
* @return sio_error_t SIO_SUCCESS or error code
*/
SIO_EXPORT sio_error_t sio_buffer_write_uint32(sio_buffer_t *buffer, uint32_t value);

/**
* @brief Write a uint64_t value to the buffer
*
* @param buffer Pointer to the buffer
* @param value Value to write
* @return sio_error_t SIO_SUCCESS or error code
*/
SIO_EXPORT sio_error_t sio_buffer_write_uint64(sio_buffer_t *buffer, uint64_t value);

/**
* @brief Read a uint8_t value from the buffer
*
* @param buffer Pointer to the buffer
* @param value Pointer to store the read value
* @return sio_error_t SIO_SUCCESS or error code
*/
SIO_EXPORT sio_error_t sio_buffer_read_uint8(sio_buffer_t *buffer, uint8_t *value);

/**
* @brief Read a uint16_t value from the buffer
*
* @param buffer Pointer to the buffer
* @param value Pointer to store the read value
* @return sio_error_t SIO_SUCCESS or error code
*/
SIO_EXPORT sio_error_t sio_buffer_read_uint16(sio_buffer_t *buffer, uint16_t *value);

/**
* @brief Read a uint32_t value from the buffer
*
* @param buffer Pointer to the buffer
* @param value Pointer to store the read value
* @return sio_error_t SIO_SUCCESS or error code
*/
SIO_EXPORT sio_error_t sio_buffer_read_uint32(sio_buffer_t *buffer, uint32_t *value);

/**
* @brief Read a uint64_t value from the buffer
*
* @param buffer Pointer to the buffer
* @param value Pointer to store the read value
* @return sio_error_t SIO_SUCCESS or error code
*/
SIO_EXPORT sio_error_t sio_buffer_read_uint64(sio_buffer_t *buffer, uint64_t *value);

/**
* @brief Buffer pool structure for managing multiple buffers
*/
typedef struct sio_buffer_pool {
  sio_buffer_t *buffers;        /**< Array of buffers */
  size_t capacity;              /**< Number of buffers in the pool */
  size_t size;                  /**< Number of buffers currently in use */
  size_t buffer_size;           /**< Size of each buffer in the pool */
  int *used_flags;              /**< Array indicating which buffers are in use */
} sio_buffer_pool_t;

/**
* @brief Create a new buffer pool
*
* @param pool Pointer to a pool structure to initialize
* @param buffer_count Number of buffers in the pool
* @param buffer_size Size of each buffer in bytes
* @return sio_error_t SIO_SUCCESS or error code
*/
SIO_EXPORT sio_error_t sio_buffer_pool_create(sio_buffer_pool_t *pool, size_t buffer_count, size_t buffer_size);

/**
* @brief Destroy a buffer pool and free its resources
*
* @param pool Pointer to the pool to destroy
* @return sio_error_t SIO_SUCCESS or error code
*/
SIO_EXPORT sio_error_t sio_buffer_pool_destroy(sio_buffer_pool_t *pool);

/**
* @brief Acquire a buffer from the pool
*
* @param pool Pointer to the pool
* @param buffer Pointer to store the acquired buffer (NULL if none available)
* @return sio_error_t SIO_SUCCESS or error code
*/
SIO_EXPORT sio_error_t sio_buffer_pool_acquire(sio_buffer_pool_t *pool, sio_buffer_t **buffer);

/**
* @brief Release a buffer back to the pool
*
* @param pool Pointer to the pool
* @param buffer Pointer to the buffer to release
* @return sio_error_t SIO_SUCCESS or error code
*/
SIO_EXPORT sio_error_t sio_buffer_pool_release(sio_buffer_pool_t *pool, sio_buffer_t *buffer);

/**
* @brief Resize a buffer pool
*
* @param pool Pointer to the pool
* @param new_buffer_count New number of buffers
* @return sio_error_t SIO_SUCCESS or error code
*/
SIO_EXPORT sio_error_t sio_buffer_pool_resize(sio_buffer_pool_t *pool, size_t new_buffer_count);

#ifdef __cplusplus
}
#endif

#endif /* SIO_BUF_H */