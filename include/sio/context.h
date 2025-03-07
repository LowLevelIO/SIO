/**
* @file sio/context.h
* @brief Simple I/O (SIO) - Cross-platform I/O library for high-performance systems programming
*
* Unified event multiplexing interface providing cross-platform, high-performance I/O event handling.
* Automatically selects the best available mechanism for each platform:
* - Linux: io_uring → epoll → poll/select
* - Windows: IOCP
* - BSD/macOS: kqueue
* - Other POSIX: poll/select
*
* @author zczxy
* @version 0.1.0
*/

#ifndef SIO_CONTEXT_H
#define SIO_CONTEXT_H

#ifdef __cplusplus
extern "C" {
#endif

#include <sio/platform.h>
#include <sio/err.h>
#include <sio/stream.h>

/**
* @brief I/O Context backend types
*/
typedef enum sio_context_backend {
  SIO_CONTEXT_AUTO = 0,      /**< Automatically select best available backend */
  
  /* Linux-specific backends */
  SIO_CONTEXT_IO_URING,      /**< Linux io_uring (kernel 5.1+) */
  SIO_CONTEXT_EPOLL,         /**< Linux epoll (kernel 2.6+) */
  
  /* BSD/macOS backend */
  SIO_CONTEXT_KQUEUE,        /**< BSD/macOS kqueue */
  
  /* Windows backend */
  SIO_CONTEXT_IOCP,          /**< Windows I/O Completion Ports */
  
  /* Fallback backends */
  SIO_CONTEXT_POLL,          /**< POSIX poll() */
  SIO_CONTEXT_SELECT         /**< POSIX select() */
} sio_context_backend_t;

/**
* @brief I/O context operation flags
*/
typedef enum sio_context_flags {
  SIO_CTX_NONE       = 0,          /**< No flags */
  SIO_CTX_NONBLOCK   = (1 << 0),   /**< Non-blocking operations */
  SIO_CTX_THREAD_SAFE = (1 << 1)   /**< Thread-safe context */
} sio_context_flags_t;

/**
* @brief I/O operation types
*/
typedef enum sio_op_type {
  SIO_OP_READ,               /**< Read operation */
  SIO_OP_WRITE,              /**< Write operation */
  SIO_OP_ACCEPT,             /**< Accept connection operation */
  SIO_OP_CONNECT,            /**< Connect operation */
  SIO_OP_CLOSE,              /**< Close operation */
  SIO_OP_CUSTOM              /**< Custom user-defined operation */
} sio_op_type_t;

/**
* @brief I/O operation status
*/
typedef enum sio_op_status {
  SIO_OP_PENDING,            /**< Operation is pending */
  SIO_OP_COMPLETE,           /**< Operation completed successfully */
  SIO_OP_ERROR,              /**< Operation failed with error */
  SIO_OP_CANCELLED,          /**< Operation was cancelled */
  SIO_OP_TIMEOUT             /**< Operation timed out */
} sio_op_status_t;

/**
* @brief I/O operation structure
*/
typedef struct sio_op {
  sio_op_type_t type;        /**< Operation type */
  sio_op_status_t status;    /**< Operation status */
  sio_error_t error;         /**< Error code if status is SIO_OP_ERROR */
  sio_stream_t *stream;      /**< Stream associated with operation */
  void *buffer;              /**< Buffer for data transfer */
  size_t size;               /**< Buffer size */
  size_t result;             /**< Bytes transferred or operation-specific result */
  void *user_data;           /**< User-defined data associated with operation */
  uint64_t timeout_ms;       /**< Timeout in milliseconds (0 = no timeout) */
  int priority;              /**< Operation priority (implementation-defined) */
  uint32_t flags;            /**< Operation-specific flags */
  
  /* Internal fields - do not modify directly */
  void *internal;            /**< Internal implementation data */
} sio_op_t;

/**
* @brief I/O context structure (opaque)
*/
typedef struct sio_context sio_context_t;

/**
* @brief Completion callback function
* 
* @param op Completed operation
* @param user_data User data associated with the context
*/
typedef void (*sio_completion_fn)(sio_op_t *op, void *user_data);

/**
* @brief Return value from sio_context_wait
*/
typedef enum sio_wait_result {
  SIO_WAIT_COMPLETED,        /**< One or more operations completed */
  SIO_WAIT_TIMEOUT,          /**< Wait timed out with no completions */
  SIO_WAIT_INTERRUPTED,      /**< Wait was interrupted */
  SIO_WAIT_ERROR             /**< An error occurred during wait */
} sio_wait_result_t;

/**
* @brief Context configuration structure
*/
typedef struct sio_context_config {
  sio_context_backend_t backend;  /**< Backend to use */
  uint32_t flags;                 /**< Context flags */
  uint32_t max_events;            /**< Maximum number of events (hint) */
  uint32_t queue_depth;           /**< Queue depth for operations (hint) */
  sio_completion_fn completion_fn; /**< Completion callback function */
  void *user_data;                /**< User data for completion callback */
} sio_context_config_t;

/**
* @brief Initialize a context configuration with default values
* 
* @param config Configuration structure to initialize
*/
SIO_EXPORT void sio_context_config_init(sio_context_config_t *config);

/**
* @brief Create a new I/O context
* 
* @param context Pointer to receive the new context
* @param config Configuration options (NULL for defaults)
* @return sio_error_t SIO_SUCCESS or error code
*/
SIO_EXPORT sio_error_t sio_context_create(sio_context_t **context, const sio_context_config_t *config);

/**
* @brief Destroy an I/O context
* 
* @param context Context to destroy
* @return sio_error_t SIO_SUCCESS or error code
*/
SIO_EXPORT sio_error_t sio_context_destroy(sio_context_t *context);

/**
* @brief Get the backend type that was selected for a context
* 
* @param context Context to query
* @return sio_context_backend_t Backend type
*/
SIO_EXPORT sio_context_backend_t sio_context_get_backend(const sio_context_t *context);

/**
* @brief Register a stream with a context
* 
* @param context Context to register with
* @param stream Stream to register
* @param user_data User data to associate with this stream
* @return sio_error_t SIO_SUCCESS or error code
*/
SIO_EXPORT sio_error_t sio_context_register(sio_context_t *context, sio_stream_t *stream, void *user_data);

/**
* @brief Unregister a stream from a context
* 
* @param context Context to unregister from
* @param stream Stream to unregister
* @return sio_error_t SIO_SUCCESS or error code
*/
SIO_EXPORT sio_error_t sio_context_unregister(sio_context_t *context, sio_stream_t *stream);

/**
* @brief Initialize an operation structure for submission
* 
* @param op Operation structure to initialize
* @param type Operation type
* @param stream Stream to operate on
* @param buffer Buffer for data transfer (can be NULL for some operations)
* @param size Size of buffer
* @param user_data User-defined data to associate with operation
* @return sio_error_t SIO_SUCCESS or error code
*/
SIO_EXPORT sio_error_t sio_op_init(sio_op_t *op, sio_op_type_t type, sio_stream_t *stream, void *buffer, size_t size, void *user_data);

/**
* @brief Submit an operation to a context
* 
* @param context Context to submit to
* @param op Operation to submit
* @return sio_error_t SIO_SUCCESS or error code
*/
SIO_EXPORT sio_error_t sio_context_submit(sio_context_t *context, sio_op_t *op);

/**
* @brief Submit multiple operations to a context
* 
* @param context Context to submit to
* @param ops Array of operations to submit
* @param count Number of operations in array
* @return sio_error_t SIO_SUCCESS or error code
*/
SIO_EXPORT sio_error_t sio_context_submit_batch(sio_context_t *context, sio_op_t **ops, size_t count);

/**
* @brief Wait for operations to complete on a context
* 
* @param context Context to wait on
* @param timeout_ms Timeout in milliseconds (SIO_WAIT_FOREVER for no timeout)
* @param max_events Maximum number of events to process
* @return sio_wait_result_t Wait result
*/
SIO_EXPORT sio_wait_result_t sio_context_wait(sio_context_t *context, uint64_t timeout_ms, uint32_t max_events);

/**
* @brief Cancel a pending operation
* 
* @param context Context that the operation was submitted to
* @param op Operation to cancel
* @return sio_error_t SIO_SUCCESS or error code
*/
SIO_EXPORT sio_error_t sio_context_cancel(sio_context_t *context, sio_op_t *op);

/**
* @brief Cancel all pending operations on a stream
* 
* @param context Context that contains the operations
* @param stream Stream whose operations should be cancelled
* @return sio_error_t SIO_SUCCESS or error code
*/
SIO_EXPORT sio_error_t sio_context_cancel_stream(sio_context_t *context, sio_stream_t *stream);

/**
* @brief Check if a context has pending operations
* 
* @param context Context to check
* @return int Non-zero if there are pending operations, zero otherwise
*/
SIO_EXPORT int sio_context_has_pending(const sio_context_t *context);

/**
* @brief Get the number of pending operations on a context
* 
* @param context Context to query
* @return size_t Number of pending operations
*/
SIO_EXPORT size_t sio_context_pending_count(const sio_context_t *context);

/* Backend-specific initialization options */

/**
* @brief io_uring specific configuration
*/
typedef struct sio_io_uring_config {
  uint32_t flags;        /**< IORING_SETUP_* flags */
  uint32_t sq_entries;   /**< Submission queue entries */
  uint32_t cq_entries;   /**< Completion queue entries */
  /* Other io_uring specific options */
} sio_io_uring_config_t;

/**
* @brief IOCP specific configuration
*/
typedef struct sio_iocp_config {
  uint32_t concurrent_threads;  /**< Number of concurrent threads */
  /* Other IOCP specific options */
} sio_iocp_config_t;

/**
* @brief Set backend-specific configuration
* 
* @param context Context to configure
* @param backend Backend to configure
* @param config Backend-specific configuration structure
* @param config_size Size of configuration structure
* @return sio_error_t SIO_SUCCESS or error code
*/
SIO_EXPORT sio_error_t sio_context_backend_config(sio_context_t *context, sio_context_backend_t backend, const void *config, size_t config_size);

/* Utility functions */

/**
* @brief Check if a backend is available on this system
* 
* @param backend Backend to check
* @return int Non-zero if available, zero otherwise
*/
SIO_EXPORT int sio_context_backend_available(sio_context_backend_t backend);

/**
* @brief Get a string description of a backend
* 
* @param backend Backend to describe
* @return const char* Description string
*/
SIO_EXPORT const char *sio_context_backend_name(sio_context_backend_t backend);

/**
* @brief Special timeout value for sio_context_wait to wait indefinitely
*/
#define SIO_WAIT_FOREVER UINT64_MAX


#ifdef __cplusplus
}
#endif

#endif /* SIO_CONTEXT_H */