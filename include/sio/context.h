/**
* @file sio/context.h
* @brief Simple I/O (SIO) - Cross-platform I/O library for high-performance systems programming
*
* An event handling context can be thought of as a wrapper for a poll like instance.
* Windows developers can think of IOCP and linux of epoll/io_uring.
* The context used can be decided at runtime and selected manually.
* By default the most recent and performant option is chosen which for linux is io_uring if not available epoll is considered a fallback.
* An end user should not notice any difference in use cases only in speed.
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
#include <stdint.h>

/**
* @brief Event context forward declaration
*/
typedef struct sio_context_s sio_context_t;

/**
* @brief Event handler callback function
*
* @param stream Stream that triggered the event
* @param events Events that occurred
* @param user_data User data associated with the handler
* @return int Non-zero to remove the handler, 0 to keep it active
*/
typedef int (*sio_event_handler_t)(sio_stream_t *stream, sio_stream_event_t events, void *user_data);

/**
* @brief Event context types
*/
typedef enum sio_context_type {
  SIO_CONTEXT_AUTO,             /**< Choose the best available method */
  SIO_CONTEXT_POLL,             /**< Use poll() - portable fallback */
  SIO_CONTEXT_EPOLL,            /**< Use epoll - Linux */
  SIO_CONTEXT_KQUEUE,           /**< Use kqueue - BSD, macOS */
  SIO_CONTEXT_IOCP,             /**< Use IOCP - Windows */
  SIO_CONTEXT_IO_URING          /**< Use io_uring - Modern Linux */
} sio_context_type_t;

/**
* @brief Stream event types for asynchronous I/O
*/
typedef enum sio_stream_event {
  SIO_STREAM_EVENT_READ = 1,     /**< Data available for reading */
  SIO_STREAM_EVENT_WRITE = 2,    /**< Stream ready for writing */
  SIO_STREAM_EVENT_ERROR = 4,    /**< Error condition */
  SIO_STREAM_EVENT_HUP = 8,      /**< Hangup (connection closed) */
  SIO_STREAM_EVENT_TIMEOUT = 16  /**< Timeout occurred */
} sio_stream_event_t;

/**
* @brief Create a new event context
*
* @param context Pointer to receive the context handle
* @param type Type of context to create
* @return sio_error_t Error code (0 on success)
*/
SIO_EXPORT sio_error_t sio_context_create(sio_context_t **context, sio_context_type_t type);

/**
* @brief Destroy an event context
*
* @param context Context to destroy
* @return sio_error_t Error code (0 on success)
*/
SIO_EXPORT sio_error_t sio_context_destroy(sio_context_t *context);

/**
* @brief Add a stream to a context for event monitoring
*
* @param context Context to add to
* @param stream Stream to monitor
* @param events Events to monitor (combination of SIO_STREAM_EVENT_* flags)
* @param handler Event handler function
* @param user_data User data to pass to the handler
* @return sio_error_t Error code (0 on success)
*/
SIO_EXPORT sio_error_t sio_context_add(sio_context_t *context, sio_stream_t *stream, sio_stream_event_t events, sio_event_handler_t handler, void *user_data);

/**
* @brief Modify events for a stream in a context
*
* @param context Context to modify
* @param stream Stream to modify
* @param events New events to monitor (combination of SIO_STREAM_EVENT_* flags)
* @return sio_error_t Error code (0 on success)
*/
SIO_EXPORT sio_error_t sio_context_modify(sio_context_t *context, sio_stream_t *stream, sio_stream_event_t events);

/**
* @brief Remove a stream from a context
*
* @param context Context to remove from
* @param stream Stream to remove
* @return sio_error_t Error code (0 on success)
*/
SIO_EXPORT sio_error_t sio_context_remove(sio_context_t *context, sio_stream_t *stream);

/**
* @brief Wait for events on streams in a context
*
* @param context Context to wait on
* @param timeout_ms Timeout in milliseconds (-1 for infinite)
* @param events_occurred Pointer to store the number of events that occurred
* @return sio_error_t Error code (0 on success, SIO_ERROR_TIMEOUT on timeout)
*/
SIO_EXPORT sio_error_t sio_context_wait(sio_context_t *context, int64_t timeout_ms, int *events_occurred);

/**
* @brief Process any pending events without waiting
*
* @param context Context to process
* @param events_occurred Pointer to store the number of events that occurred
* @return sio_error_t Error code (0 on success)
*/
SIO_EXPORT sio_error_t sio_context_process(sio_context_t *context, int *events_occurred);

/**
* @brief Get the active context type
*
* @param context Context to query
* @param type Pointer to store the context type
* @return sio_error_t Error code (0 on success)
*/
SIO_EXPORT sio_error_t sio_context_get_type(sio_context_t *context, sio_context_type_t *type);

/**
* @brief Check if a context type is supported on the current platform
*
* @param type Context type to check
* @param supported Pointer to store the result (non-zero if supported)
* @return sio_error_t Error code (0 on success)
*/
SIO_EXPORT sio_error_t sio_context_is_supported(sio_context_type_t type, int *supported);

#ifdef __cplusplus
}
#endif

#endif /* SIO_CONTEXT_H */