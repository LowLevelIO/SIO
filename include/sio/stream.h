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

#include "sio/platform.h"
#include "sio/buf.h"

/**
* @brief Stream types
*/
typedef enum {
  SIO_STREAM_FILE,         /**< File stream */
  SIO_STREAM_SOCKET_TCP,   /**< TCP socket stream */
  SIO_STREAM_SOCKET_UDP,   /**< UDP socket stream */
  SIO_STREAM_SOCKET_UNIX,  /**< Unix domain socket stream */
  SIO_STREAM_PIPE,         /**< Pipe stream */
  SIO_STREAM_TERMINAL,     /**< Terminal stream */
  SIO_STREAM_WINDOW,       /**< Window stream (platform-dependent) */
  SIO_STREAM_DEVICE,       /**< Device stream */
  SIO_STREAM_TIMER,        /**< Timer stream */
  SIO_STREAM_SIGNAL,       /**< Signal stream */
  SIO_STREAM_MSGQUEUE,     /**< Message queue stream */
  SIO_STREAM_SHMEM,        /**< Shared memory stream */
  SIO_STREAM_MEMORY        /**< Memory stream */
} sio_stream_type_t;

/**
* @brief Stream flags for controlling behavior
*/
typedef enum {
  SIO_STREAM_NONBLOCK = 0x01,   /**< Non-blocking I/O */
  SIO_STREAM_CLOEXEC = 0x02,    /**< Close on exec */
  SIO_STREAM_ASYNC = 0x04,      /**< Asynchronous I/O */
  SIO_STREAM_DIRECT = 0x08      /**< Direct I/O (if supported) */
} sio_stream_flags_t;

/**
* @brief Stream structure
* 
* Opaque structure that contains the internal state of a stream.
*/
typedef struct sio_stream {
  sio_stream_type_t type;       /**< Type of the stream */
  sio_handle_t handle;          /**< OS handle or file descriptor */
  uint32_t flags;               /**< Stream flags */
  void* data;                   /**< Stream-specific data */
} sio_stream_t;

/**
* @brief Open a file stream
* 
* @param path Path to the file
* @param flags Combination of O_* flags (platform specific)
* @param mode File mode for creation (ignored if not creating)
* @return sio_stream_t* New stream or NULL on error
*/
sio_stream_t* sio_file_open(const char* path, int flags, int mode);

/**
* @brief Create a TCP socket stream
* 
* @param flags Stream flags
* @return sio_stream_t* New stream or NULL on error
*/
sio_stream_t* sio_tcp_socket(uint32_t flags);

/**
* @brief Create a UDP socket stream
* 
* @param flags Stream flags
* @return sio_stream_t* New stream or NULL on error
*/
sio_stream_t* sio_udp_socket(uint32_t flags);

/**
* @brief Create a Unix domain socket stream
* 
* @param flags Stream flags
* @return sio_stream_t* New stream or NULL on error
*/
sio_stream_t* sio_unix_socket(uint32_t flags);

/**
* @brief Create a pipe
* 
* @param read_stream Pointer to store the read end of the pipe
* @param write_stream Pointer to store the write end of the pipe
* @param flags Stream flags
* @return int SIO_SUCCESS on success, or an error code
*/
int sio_pipe_create(sio_stream_t** read_stream, sio_stream_t** write_stream, uint32_t flags);

/**
* @brief Open a terminal stream
* 
* @param device Terminal device name (can be NULL for default)
* @param flags Stream flags
* @return sio_stream_t* New stream or NULL on error
*/
sio_stream_t* sio_terminal_open(const char* device, uint32_t flags);

/**
* @brief Open a device stream
* 
* @param device Device path
* @param flags Stream flags
* @return sio_stream_t* New stream or NULL on error
*/
sio_stream_t* sio_device_open(const char* device, uint32_t flags);

/**
* @brief Create a timer stream
* 
* @param flags Stream flags
* @return sio_stream_t* New stream or NULL on error
*/
sio_stream_t* sio_timer_create(uint32_t flags);

/**
* @brief Create a signal stream
* 
* @param signal_num Signal number
* @param flags Stream flags
* @return sio_stream_t* New stream or NULL on error
*/
sio_stream_t* sio_signal_create(int signal_num, uint32_t flags);

/**
* @brief Create a message queue stream
* 
* @param name Message queue name
* @param flags Stream flags
* @return sio_stream_t* New stream or NULL on error
*/
sio_stream_t* sio_msgqueue_open(const char* name, uint32_t flags);

/**
* @brief Create a shared memory stream
* 
* @param name Shared memory name
* @param size Size of the shared memory in bytes
* @param flags Stream flags
* @return sio_stream_t* New stream or NULL on error
*/
sio_stream_t* sio_shmem_open(const char* name, size_t size, uint32_t flags);

/**
* @brief Create a memory stream
* 
* @param buffer Initial buffer (can be NULL)
* @param size Initial buffer size
* @param flags Stream flags
* @return sio_stream_t* New stream or NULL on error
*/
sio_stream_t* sio_memory_create(void* buffer, size_t size, uint32_t flags);

/**
* @brief Close a stream
* 
* @param stream Stream to close
* @return int SIO_SUCCESS on success, or an error code
*/
int sio_stream_close(sio_stream_t* stream);

/**
* @brief Read data from a stream
* 
* @param stream Stream to read from
* @param buffer Buffer to store data
* @param size Maximum number of bytes to read
* @return ssize_t Number of bytes read, or an error code
*/
ssize_t sio_stream_read(sio_stream_t* stream, void* buffer, size_t size);

/**
* @brief Write data to a stream
* 
* @param stream Stream to write to
* @param buffer Data to write
* @param size Number of bytes to write
* @return ssize_t Number of bytes written, or an error code
*/
ssize_t sio_stream_write(sio_stream_t* stream, const void* buffer, size_t size);

/**
* @brief Read data from a stream at specified offset (for files)
* 
* @param stream Stream to read from
* @param buffer Buffer to store data
* @param size Maximum number of bytes to read
* @param offset Offset to read from
* @return ssize_t Number of bytes read, or an error code
*/
ssize_t sio_stream_pread(sio_stream_t* stream, void* buffer, size_t size, off_t offset);

/**
* @brief Write data to a stream at specified offset (for files)
* 
* @param stream Stream to write to
* @param buffer Data to write
* @param size Number of bytes to write
* @param offset Offset to write to
* @return ssize_t Number of bytes written, or an error code