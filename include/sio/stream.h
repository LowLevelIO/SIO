/**
* @file sio/stream.h
* @brief Simple I/O (SIO) - Cross-platform I/O library for high-performance systems programming
*
* The direct functionality for streams including creation, option manipulation, input, output and deletion.
* The streams supported are:
*   Files
*   Sockets
*   Pipe
*   Terminal
*   Window (optional depending on operating system as linux does not come with this functionality)
*   Device
*   Timer
*   Signal
*   Message Queue
*   Shared Memory
*   Memory
*
* @author zczxy
* @version 0.1.0
*/

#ifndef SIO_STREAM_H
#define SIO_STREAM_H

#ifdef __cplusplus
extern "C" {
#endif

#include <sio/platform.h>
#include <sio/err.h>
#include <sio/buf.h>
#include <stdint.h>
#include <stddef.h>

/**
* @brief Stream types
*/
typedef enum sio_stream_type {
  SIO_STREAM_TYPE_FILE,        /**< Regular file */
  SIO_STREAM_TYPE_SOCKET,      /**< Network socket */
  SIO_STREAM_TYPE_PIPE,        /**< Pipe */
  SIO_STREAM_TYPE_TERMINAL,    /**< Terminal (console) */
  SIO_STREAM_TYPE_WINDOW,      /**< Window (GUI, platform-specific) */
  SIO_STREAM_TYPE_DEVICE,      /**< Hardware device */
  SIO_STREAM_TYPE_TIMER,       /**< Timer */
  SIO_STREAM_TYPE_SIGNAL,      /**< Signal */
  SIO_STREAM_TYPE_MSGQUEUE,    /**< Message queue */
  SIO_STREAM_TYPE_SHAREDMEM,   /**< Shared memory */
  SIO_STREAM_TYPE_MEMORY       /**< In-memory buffer */
} sio_stream_type_t;

/**
* @brief for vector read and write operations
*/
#if defined(SIO_OS_WINDOWS)
  typedef struct { // windows compilant iovec structure (WSABUF-https://learn.microsoft.com/en-us/windows/win32/api/ws2def/ns-ws2def-wsabuf)
    ULONG len;
    CHAR  *buf;
  } sio_iovec_t;
#elif defined(SIO_OS_POSIX)
  typedef struct { // posix compilant iovec structure (iovec-https://linux.die.net/man/2/writev)
    void *buf;
    size_t len;
  } sio_iovec_t;
#else
  #error "Unsupported Operating System
#endif

/*================================ Base Stream Functionality ================================*/
/**
* @brief Close a stream
*
* @param stream Pointer to the stream to close
* @return sio_error_t Error code (0 on success)
*/
SIO_EXPORT sio_error_t sio_stream_close(sio_stream_t *stream);

/**
* @brief Read data from a stream
*
* @param stream Source stream
* @param buffer Destination buffer
* @param size Maximum number of bytes to read
* @param bytes_read Pointer to store the actual number of bytes read (can be NULL)
* @return sio_error_t Error code (0 on success, SIO_ERROR_EOF at end of stream)
*/
SIO_EXPORT sio_error_t sio_stream_read(sio_stream_t *stream, void *buffer, size_t size, size_t *bytes_read);

/**
* @brief Write data to a stream
*
* @param stream Destination stream
* @param buffer Source buffer
* @param size Number of bytes to write
* @param bytes_written Pointer to store the actual number of bytes written (can be NULL)
* @return sio_error_t Error code (0 on success)
*/
SIO_EXPORT sio_error_t sio_stream_write(sio_stream_t *stream, const void *buffer, size_t size, size_t *bytes_written);

/**
* @brief Flush a stream's buffers
*
* @param stream Stream to flush
* @return sio_error_t Error code (0 on success)
*/
SIO_EXPORT sio_error_t sio_stream_flush(sio_stream_t *stream);

/*================================ Optimized Stream Functionality ================================*/

/**
* @brief Read data from a stream
*
* @param stream Source stream
* @param buffer Destination buffer
* @param size Maximum number of bytes to read
* @param bytes_read Pointer to store the actual number of bytes read (can be NULL)
* @return sio_error_t Error code (0 on success, SIO_ERROR_EOF at end of stream)
*/
SIO_EXPORT sio_error_t sio_stream_readv(sio_stream_t *stream, sio_iovec_t *bufs, size_t bufcnt, size_t *bytes_read);

/**
* @brief Write data to a stream
*
* @param stream Destination stream
* @param buffer Source buffer
* @param size Number of bytes to write
* @param bytes_written Pointer to store the actual number of bytes written (can be NULL)
* @return sio_error_t Error code (0 on success)
*/
SIO_EXPORT sio_error_t sio_stream_writev(sio_stream_t *stream, const sio_iovec_t *bufs, size_t bufcnt, size_t *bytes_written);

// TODO: add more optimized system calls like send and recv file.

/*================================ Advanced Stream Functionality ================================*/
/**
* @brief Copy data from one stream to another
*
* @param dst Destination stream
* @param src Source stream
* @param max_bytes Maximum bytes to copy (-1 for unlimited)
* @param bytes_copied Pointer to store the total bytes copied (can be NULL)
* @return sio_error_t Error code (0 on success)
*/
SIO_EXPORT sio_error_t sio_stream_copy(sio_stream_t *dst, sio_stream_t *src, int64_t max_bytes, uint64_t *bytes_copied);

/**
* @brief Read a line from a stream
*
* @param stream Source stream
* @param buffer Buffer to receive the line
* @param size Size of the buffer
* @param bytes_read Pointer to store the number of bytes read (can be NULL)
* @return sio_error_t Error code (0 on success, SIO_ERROR_EOF at end of stream)
*/
SIO_EXPORT sio_error_t sio_stream_read_line(sio_stream_t *stream, char *buffer, size_t size, size_t *bytes_read);

/**
* @brief Write a line to a stream
*
* @param stream Destination stream
* @param line Line to write
* @param add_newline Whether to append a newline if not present
* @return sio_error_t Error code (0 on success)
*/
SIO_EXPORT sio_error_t sio_stream_write_line(sio_stream_t *stream, const char *line, int add_newline);

/**
* @brief Read a formatted value from a stream
*
* @param stream Source stream
* @param format Format string
* @param ... Variable arguments corresponding to the format string
* @return sio_error_t Error code (0 on success)
*/
SIO_EXPORT sio_error_t sio_stream_scanf(sio_stream_t *stream, const char *format, ...);

/**
* @brief Write a formatted value to a stream
*
* @param stream Destination stream
* @param format Format string
* @param ... Variable arguments corresponding to the format string
* @return sio_error_t Error code (0 on success)
*/
SIO_EXPORT sio_error_t sio_stream_printf(sio_stream_t *stream, const char *format, ...);

/**
* @brief Duplicate a stream
*
* Creates a new stream that is a duplicate of the original.
* For files, this opens the same file at the same position.
* For sockets, this creates a duplicate socket with its own file descriptor.
*
* @param dst Pointer to receive the duplicate stream
* @param src Source stream to duplicate
* @return sio_error_t Error code (0 on success)
*/
SIO_EXPORT sio_error_t sio_stream_duplicate(sio_stream_t **dst, sio_stream_t *src);

/**
* @brief Split a stream into multiple parts
*
* Creates a specified number of substreams, each covering a portion of the original stream.
* Useful for parallel processing of large files or memory regions.
*
* @param stream Source stream to split
* @param substreams Array to receive the substream handles
* @param count Number of substreams to create
* @return sio_error_t Error code (0 on success)
*/
SIO_EXPORT sio_error_t sio_stream_split(sio_stream_t *stream, sio_stream_t **substreams, size_t count);

/**
* @brief Create a buffered stream wrapper
*
* Wraps an existing stream with buffering to improve performance for small reads/writes.
*
* @param buffered_stream Pointer to receive the buffered stream handle
* @param raw_stream Stream to wrap
* @param buffer_size Size of the buffer to use
* @return sio_error_t Error code (0 on success)
*/
SIO_EXPORT sio_error_t sio_stream_make_buffered(sio_stream_t **buffered_stream, sio_stream_t *raw_stream, size_t buffer_size);

/**
* @brief Create a tee stream
*
* Creates a stream that duplicates all writes to two output streams.
* Useful for logging or monitoring data flowing through a stream.
*
* @param tee_stream Pointer to receive the tee stream handle
* @param stream1 First output stream
* @param stream2 Second output stream
* @return sio_error_t Error code (0 on success)
*/
SIO_EXPORT sio_error_t sio_stream_tee(sio_stream_t **tee_stream, sio_stream_t *stream1, sio_stream_t *stream2);

/**
* @brief Lock a portion of a stream
*
* Locks a region of a stream for exclusive access.
* Useful for concurrent access to shared files.
*
* @param stream Stream to lock
* @param offset Offset of the region to lock
* @param size Size of the region to lock
* @param exclusive Non-zero for exclusive lock, 0 for shared lock
* @return sio_error_t Error code (0 on success)
*/
SIO_EXPORT sio_error_t sio_stream_lock(sio_stream_t *stream, uint64_t offset, uint64_t size, int exclusive);

/**
* @brief Unlock a previously locked portion of a stream
*
* @param stream Stream to unlock
* @param offset Offset of the region to unlock
* @param size Size of the region to unlock
* @return sio_error_t Error code (0 on success)
*/
SIO_EXPORT sio_error_t sio_stream_unlock(sio_stream_t *stream, uint64_t offset, uint64_t size);

/*================================ Optional Stream Functionality ================================*/

/**
* @brief Change the current position in a stream
*
* @param stream Stream to seek in
* @param offset Offset from the origin
* @param origin Origin of seek operation (start, current, or end)
* @return sio_error_t Error code (0 on success)
*/
SIO_EXPORT sio_error_t sio_stream_seek(sio_stream_t *stream, int64_t offset, sio_stream_seek_t origin);

/**
* @brief Get the current position in a stream
*
* @param stream Stream to get position from
* @param position Pointer to store the position
* @return sio_error_t Error code (0 on success)
*/
SIO_EXPORT sio_error_t sio_stream_tell(sio_stream_t *stream, uint64_t *position);

/**
* @brief Get the size of a stream
*
* @param stream Stream to get size from
* @param size Pointer to store the size
* @return sio_error_t Error code (0 on success)
*/
SIO_EXPORT sio_error_t sio_stream_size(sio_stream_t *stream, uint64_t *size);

/**
* @brief Check if a stream is at the end, streams that don't have a typical end like sockets will always set is_eof to 0
*
* @param stream Stream to check
* @param is_eof Pointer to store the result (non-zero if at end)
* @return sio_error_t Error code (0 on success)
*/
SIO_EXPORT sio_error_t sio_stream_eof(sio_stream_t *stream, int *is_eof);

/*================================ Stream Options ================================*/

/**
* @brief Set an option on a stream
*
* @param stream Stream to modify
* @param option Option to set
* @param value Pointer to the option value
* @param size Size of the option value
* @return sio_error_t Error code (0 on success)
*/
SIO_EXPORT sio_error_t sio_stream_set_option(sio_stream_t *stream, sio_stream_option_t option, const void *value, size_t size);

/**
* @brief Get an option from a stream
*
* @param stream Stream to query
* @param option Option to get
* @param value Pointer to store the option value
* @param size Size of the buffer / size of retrieved value
* @return sio_error_t Error code (0 on success)
*/
SIO_EXPORT sio_error_t sio_stream_get_option(sio_stream_t *stream, sio_stream_option_t option, void *value, size_t *size);

/*================================ Stream Creation ================================*/
/**
* @brief Open a file stream
*
* @param stream Pointer to receive the stream handle
* @param path Path to the file
* @param mode Opening mode (combination of SIO_STREAM_MODE_* flags)
* @param permissions File permissions for new files
* @return sio_error_t Error code (0 on success)
*/
SIO_EXPORT sio_error_t sio_stream_open_file(sio_stream_t **stream, const char *path, int mode, uint32_t permissions);

/**
* @brief Create a socket stream
*
* @param stream Pointer to receive the stream handle
* @param domain Address family (e.g., SIO_AF_INET)
* @param type Socket type (e.g., SIO_SOCK_STREAM)
* @param protocol Protocol (e.g., SIO_IPPROTO_TCP)
* @return sio_error_t Error code (0 on success)
*/
SIO_EXPORT sio_error_t sio_stream_open_socket(sio_stream_t **stream, int domain, int type, int protocol);

/**
* @brief Create a pipe
*
* @param read_stream Pointer to receive the read end of the pipe
* @param write_stream Pointer to receive the write end of the pipe
* @return sio_error_t Error code (0 on success)
*/
SIO_EXPORT sio_error_t sio_stream_open_pipe(sio_stream_t **read_stream, sio_stream_t **write_stream);

/**
* @brief Open a terminal stream
*
* @param stream Pointer to receive the stream handle
* @param name Terminal device name (can be NULL for default)
* @param mode Opening mode (combination of SIO_STREAM_MODE_* flags)
* @return sio_error_t Error code (0 on success)
*/
SIO_EXPORT sio_error_t sio_stream_open_terminal(sio_stream_t **stream, const char *name, int mode);

/**
* @brief Open a device stream
*
* @param stream Pointer to receive the stream handle
* @param device_path Path to the device
* @param mode Opening mode (combination of SIO_STREAM_MODE_* flags)
* @return sio_error_t Error code (0 on success)
*/
SIO_EXPORT sio_error_t sio_stream_open_device(sio_stream_t **stream, const char *device_path, int mode);

/**
* @brief Create a memory stream
*
* @param stream Pointer to receive the stream handle
* @param buffer Buffer to use (NULL to allocate internally)
* @param size Size of the buffer
* @param owns_buffer Whether the stream should take ownership of the buffer
* @return sio_error_t Error code (0 on success)
*/
SIO_EXPORT sio_error_t sio_stream_open_memory(sio_stream_t **stream, sio_buffer_t *buffer);

/**
* @brief Create a timer stream
*
* @param stream Pointer to receive the stream handle
* @param initial_ms Initial timeout in milliseconds
* @param period_ms Period in milliseconds (0 for one-shot)
* @return sio_error_t Error code (0 on success)
*/
SIO_EXPORT sio_error_t sio_stream_open_timer(sio_stream_t **stream, uint64_t initial_ms, uint64_t period_ms);

/**
* @brief Create a signal stream
*
* @param stream Pointer to receive the stream handle
* @param signal_number Signal number to monitor
* @return sio_error_t Error code (0 on success)
*/
SIO_EXPORT sio_error_t sio_stream_open_signal(sio_stream_t **stream, int signal_number);

/**
* @brief Create a message queue stream
*
* @param stream Pointer to receive the stream handle
* @param name Name of the message queue
* @param mode Opening mode (combination of SIO_STREAM_MODE_* flags)
* @param max_msg_size Maximum message size
* @param max_msgs Maximum number of messages
* @return sio_error_t Error code (0 on success)
*/
SIO_EXPORT sio_error_t sio_stream_open_msgqueue(sio_stream_t **stream, const char *name, int mode, size_t max_msg_size, size_t max_msgs);

/**
* @brief Create a shared memory stream
*
* @param stream Pointer to receive the stream handle
* @param name Name of the shared memory region
* @param size Size of the shared memory region
* @param mode Opening mode (combination of SIO_STREAM_MODE_* flags)
* @return sio_error_t Error code (0 on success)
*/
SIO_EXPORT sio_error_t sio_stream_open_sharedmem(sio_stream_t **stream, const char *name, size_t size, int mode);

/**
* @brief Create a window stream (windows api (windows), wayland api (linux/bsd), x11 api (linux/bsd), metal api (osx))
*
* @param stream Pointer to receive the stream handle
* @param window_handle Window handle (HWND)
* @return sio_error_t Error code (0 on success)
*/
SIO_EXPORT sio_error_t sio_stream_open_window(sio_stream_t **stream, void *window_handle);


/*================================ Stream Information ================================*/

/**
* @brief Get the type of a stream
*
* @param stream Stream to query
* @param type Pointer to store the stream type
* @return sio_error_t Error code (0 on success)
*/
SIO_EXPORT sio_error_t sio_stream_get_type(sio_stream_t *stream, sio_stream_type_t *type);

/**
* @brief Check if two streams are equivalent
*
* Compares two streams to determine if they refer to the same underlying resource.
*
* @param stream1 First stream to compare
* @param stream2 Second stream to compare
* @param equivalent Pointer to store the result (non-zero if equivalent)
* @return sio_error_t Error code (0 on success)
*/
SIO_EXPORT sio_error_t sio_stream_equivalent(sio_stream_t *stream1, sio_stream_t *stream2, int *equivalent);

/**
* @brief Check if the size feature is available
*
* @param stream stream to check
* @return int 0 on not available or 1 on available
*/
SIO_EXPORT int sio_stream_size_available(sio_stream_t *stream);

/**
* @brief Check if the tell feature is available
*
* @param stream stream to check
* @return int 0 on not available or 1 on available
*/
SIO_EXPORT int sio_stream_tell_available(sio_stream_t *stream);

/**
* @brief Check if the seek feature is available
*
* @param stream stream to check
* @return int 0 on not available or 1 on available
*/
SIO_EXPORT int sio_stream_seek_available(sio_stream_t *stream);

#ifdef __cplusplus
}
#endif

#endif /* SIO_STREAM_H */