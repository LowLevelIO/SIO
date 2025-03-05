#ifdef __cplusplus
#pragma once
extern "C" {
#endif


#ifndef SIO_HEADER_DEFINED
#define SIO_HEADER_DEFINED

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

/**
* @file sio.h
* @brief Simple I/O (SIO) - Cross-platform I/O library for high-performance systems programming
*
* SIO provides a unified interface for various I/O operations across different
* operating systems without sacrificing performance. It abstracts platform-specific
* details while allowing direct access to optimized system calls when needed.
*
* On Windows, it leverages IOCP (I/O Completion Ports) and overlapped I/O.
* On Linux, it uses epoll, io_uring (when available), or falls back to optimized poll.
*
* @author zczxy
* @version 0.1.0
*/

/**
* @defgroup SIO_ErrorCodes Error Codes
* @{
*/

/** Operation completed successfully */
#define SIO_SUCCESS             0
/** Generic error code */
#define SIO_ERROR              -1
/** Operation would block */
#define SIO_WOULD_BLOCK        -2
/** Operation timed out */
#define SIO_TIMEOUT            -3
/** Connection was closed */
#define SIO_CLOSED             -4
/** Invalid parameter */
#define SIO_INVALID_PARAM      -5
/** Out of memory */
#define SIO_OUT_OF_MEMORY      -6
/** Operation not supported */
#define SIO_NOT_SUPPORTED      -7
/** Resource not found */
#define SIO_NOT_FOUND          -8
/** Resource already exists */
#define SIO_ALREADY_EXISTS     -9
/** Permission denied */
#define SIO_PERMISSION_DENIED -10
/** Operation in progress */
#define SIO_IN_PROGRESS       -11

/** @} */

/**
* @defgroup SIO_Types Common Types
* @{
*/

/**
* @brief Context handle for I/O operations
*
* Represents an I/O context that manages events and completions.
* Each context typically maps to an IOCP instance on Windows or
* an epoll/io_uring instance on Linux.
*/
typedef struct sio_context_t sio_context_t;

/**
* @brief Handle to an I/O resource
*
* Represents any I/O resource that can be used with the library,
* such as a socket, file, pipe, etc.
*/
typedef struct sio_handle_t sio_handle_t;

/**
* @brief Buffer descriptor for I/O operations
*
* Describes a memory buffer for read/write operations.
*/
typedef struct sio_buffer {
  /** Pointer to the data buffer */
  void* data;
  /** Length of the data buffer in bytes */
  size_t length;
} sio_buffer_t;

/**
* @brief I/O handle types
*
* Defines the different types of I/O resources that can be managed.
*/
typedef enum sio_handle_type {
  SIO_HANDLE_SOCKET,     /**< Network socket */
  SIO_HANDLE_FILE,       /**< File handle */
  SIO_HANDLE_PIPE,       /**< Pipe or FIFO */
  SIO_HANDLE_TERMINAL,   /**< Terminal/console */
  SIO_HANDLE_DEVICE,     /**< Device handle */
  SIO_HANDLE_TIMER,      /**< Timer */
  SIO_HANDLE_SIGNAL,     /**< Signal */
  SIO_HANDLE_MSGQUEUE,   /**< Message queue */
  SIO_HANDLE_SHAREDMEM,  /**< Shared memory */
  SIO_HANDLE_MEMORY      /**< Memory buffer as stream */
} sio_handle_type_t;

/**
* @brief Event types for I/O operations
*
* Defines the different event types that can be monitored or reported.
*/
typedef enum sio_event_type {
  SIO_EVENT_READ,        /**< Data available for reading */
  SIO_EVENT_WRITE,       /**< Buffer available for writing */
  SIO_EVENT_ACCEPT,      /**< Connection ready to accept */
  SIO_EVENT_CONNECT,     /**< Connection established */
  SIO_EVENT_CLOSE,       /**< Connection closed */
  SIO_EVENT_TIMEOUT,     /**< Operation timed out */
  SIO_EVENT_ERROR,       /**< Error occurred */
  SIO_EVENT_CUSTOM       /**< Custom user-defined event */
} sio_event_type_t;

/**
* @brief Event mask for registering interest
* 
* Used to specify which events to monitor on a handle.
*/
typedef enum sio_event_mask {
  SIO_MASK_READ    = (1 << 0), /**< Interest in read events */
  SIO_MASK_WRITE   = (1 << 1), /**< Interest in write events */
  SIO_MASK_ACCEPT  = (1 << 2), /**< Interest in accept events */
  SIO_MASK_CONNECT = (1 << 3), /**< Interest in connect events */
  SIO_MASK_CLOSE   = (1 << 4), /**< Interest in close events */
  SIO_MASK_ERROR   = (1 << 5), /**< Interest in error events */
  SIO_MASK_TIMEOUT = (1 << 6), /**< Interest in timeout events */
  SIO_MASK_CUSTOM  = (1 << 7)  /**< Interest in custom events */
} sio_event_mask_t;

/**
* @brief Event structure for I/O operations
*
* Contains information about an I/O event.
*/
typedef struct sio_event {
  /** The handle that triggered the event */
  sio_handle_t* handle;
  /** The type of event that occurred */
  sio_event_type_t type;
  /** Status code for the event (SIO_SUCCESS or an error code) */
  int status;
  /** Number of bytes transferred (for read/write events) */
  size_t bytes_transferred;
  /** User data associated with the handle */
  void* user_data;
} sio_event_t;

/**
* @brief Event callback function type
*
* Function signature for event callbacks.
*
* @param event Pointer to the event structure
* @param user_data User-provided context data
*/
typedef void (*sio_event_callback_t)(sio_event_t* event, void* user_data);

/**
* @brief Feature flags for platform capabilities
*
* Used to query available platform features.
*/
typedef struct sio_features {
  /** True if IOCP is available (Windows) */
  bool has_iocp;
  /** True if epoll is available (Linux) */
  bool has_epoll;
  /** True if io_uring is available (Linux) */
  bool has_io_uring;
  /** True if kqueue is available (BSD) */
  bool has_kqueue;
  /** True if zero-copy operations are supported */
  bool has_zero_copy;
  /** True if true asynchronous file I/O is supported */
  bool has_async_file_io;
  /** True if memory-mapped I/O is supported */
  bool has_mmap;
  /** True if scatter-gather I/O is supported */
  bool has_scatter_gather;
} sio_features_t;

/**
* @brief Socket address family
*
* Defines the address family for socket operations.
*/
typedef enum sio_address_family {
  SIO_AF_INET,   /**< IPv4 */
  SIO_AF_INET6,  /**< IPv6 */
  SIO_AF_UNIX    /**< Unix domain socket */
} sio_address_family_t;

/**
* @brief Socket type
*
* Defines the socket type for socket operations.
*/
typedef enum sio_socket_type {
  SIO_SOCK_STREAM,    /**< Stream socket (TCP) */
  SIO_SOCK_DGRAM,     /**< Datagram socket (UDP) */
  SIO_SOCK_RAW        /**< Raw socket */
} sio_socket_type_t;

/**
* @brief File open flags
*
* Defines the flags for opening files.
*/
typedef enum sio_file_flags {
  SIO_FILE_READ       = (1 << 0), /**< Open for reading */
  SIO_FILE_WRITE      = (1 << 1), /**< Open for writing */
  SIO_FILE_APPEND     = (1 << 2), /**< Append to file */
  SIO_FILE_CREATE     = (1 << 3), /**< Create if doesn't exist */
  SIO_FILE_TRUNCATE   = (1 << 4), /**< Truncate if exists */
  SIO_FILE_NONBLOCK   = (1 << 5), /**< Non-blocking I/O */
  SIO_FILE_DIRECT     = (1 << 6), /**< Direct I/O (bypass cache) */
  SIO_FILE_SYNC       = (1 << 7), /**< Synchronous I/O */
  SIO_FILE_MMAP       = (1 << 8)  /**< Use memory mapping when possible */
} sio_file_flags_t;

/** @} */

/**
* @defgroup SIO_Initialization Initialization Functions
* @{
*/

/**
* @brief Initialize the SIO library
*
* Must be called before any other SIO function.
* Initializes internal state and detects platform capabilities.
*
* @return SIO_SUCCESS on success, error code on failure
*/
int sio_initialize(void);

/**
* @brief Clean up and release resources used by the SIO library
*
* Should be called when the application is done using SIO.
*/
void sio_cleanup(void);

/**
* @brief Query available features and capabilities
*
* Detects which features are supported on the current platform.
*
* @param features Pointer to a features structure to fill
* @return SIO_SUCCESS on success, error code on failure
*/
int sio_query_features(sio_features_t* features);

/**
* @brief Create a new I/O context
*
* Creates an I/O context for managing events and completions.
* On Windows, this creates an IOCP. On Linux, it creates an
* epoll instance or io_uring if available.
*
* @param max_concurrent_events Suggested maximum number of concurrent events
* @return Pointer to the new context, or NULL on failure
*/
sio_context_t* sio_context_create(int max_concurrent_events);

/**
* @brief Destroy an I/O context
*
* Cleans up and releases resources associated with an I/O context.
*
* @param context The context to destroy
*/
void sio_context_destroy(sio_context_t* context);

/** @} */

/**
* @defgroup SIO_Socket Socket Functions
* @{
*/

/**
* @brief Create a socket
*
* Creates a new socket with the specified address family and type.
*
* @param context The I/O context to associate with the socket
* @param family The address family (IPv4, IPv6, etc.)
* @param type The socket type (stream, datagram, etc.)
* @param flags Additional flags for socket creation
* @return A handle to the new socket, or NULL on failure
*/
sio_handle_t* sio_socket_create(sio_context_t* context, 
                              sio_address_family_t family,
                              sio_socket_type_t type, 
                              int flags);

/**
* @brief Bind a socket to an address
*
* Associates a socket with a local address and port.
*
* @param socket The socket handle
* @param address The address string (IPv4, IPv6, or path for Unix sockets)
* @param port The port number (ignored for Unix sockets)
* @return SIO_SUCCESS on success, error code on failure
*/
int sio_socket_bind(sio_handle_t* socket, 
                  const char* address, 
                  uint16_t port);

/**
* @brief Start listening for connections
*
* Marks a socket as passive and ready to accept incoming connections.
*
* @param socket The socket handle
* @param backlog Maximum length of the pending connection queue
* @return SIO_SUCCESS on success, error code on failure
*/
int sio_socket_listen(sio_handle_t* socket, 
                    int backlog);

/**
* @brief Accept an incoming connection
*
* Accepts a new connection on a listening socket.
* If the socket is in non-blocking mode, this may return SIO_WOULD_BLOCK.
*
* @param socket The listening socket handle
* @param client_address Buffer to store the client address (can be NULL)
* @param client_port Pointer to store the client port (can be NULL)
* @return A handle to the new client socket, or NULL on failure
*/
sio_handle_t* sio_socket_accept(sio_handle_t* socket, 
                              char* client_address, 
                              uint16_t* client_port);

/**
* @brief Connect to a remote host
*
* Establishes a connection to a remote host.
* If the socket is in non-blocking mode, this may return SIO_IN_PROGRESS.
*
* @param socket The socket handle
* @param address The remote address string
* @param port The remote port number
* @return SIO_SUCCESS on success, error code on failure
*/
int sio_socket_connect(sio_handle_t* socket, 
                    const char* address, 
                    uint16_t port);

/**
* @brief Perform asynchronous name resolution
*
* Resolves a hostname to an IP address asynchronously.
* Results are delivered via a callback.
*
* @param context The I/O context
* @param hostname The hostname to resolve
* @param family The address family to resolve for
* @param callback Function to call when resolution completes
* @param user_data User data to pass to the callback
* @return SIO_SUCCESS on success, error code on failure
*/
int sio_resolve_hostname(sio_context_t* context,
                      const char* hostname,
                      sio_address_family_t family,
                      sio_event_callback_t callback,
                      void* user_data);

/** @} */

/**
* @defgroup SIO_File File Functions
* @{
*/

/**
* @brief Open a file
*
* Opens a file with the specified flags.
*
* @param context The I/O context to associate with the file
* @param path The path to the file
* @param flags Flags controlling how the file is opened
* @param mode File permissions for new files
* @return A handle to the file, or NULL on failure
*/
sio_handle_t* sio_file_open(sio_context_t* context,
                          const char* path,
                          sio_file_flags_t flags,
                          int mode);

/**
* @brief Create a memory-mapped file
*
* Maps a file into memory for direct access.
*
* @param context The I/O context
* @param path The path to the file
* @param flags Flags controlling how the file is opened
* @param size The size of the mapping (0 for entire file)
* @param offset The offset within the file to start mapping
* @param mapped_addr Pointer to store the mapped address
* @return A handle to the mapped file, or NULL on failure
*/
sio_handle_t* sio_file_mmap(sio_context_t* context,
                          const char* path,
                          sio_file_flags_t flags,
                          size_t size,
                          off_t offset,
                          void** mapped_addr);

/**
* @brief Get file information
*
* Retrieves information about a file.
*
* @param file The file handle
* @param size Pointer to store the file size
* @param mtime Pointer to store the modification time
* @param is_directory Pointer to store whether the file is a directory
* @return SIO_SUCCESS on success, error code on failure
*/
int sio_file_info(sio_handle_t* file,
                size_t* size,
                uint64_t* mtime,
                bool* is_directory);

/** @} */

/**
* @defgroup SIO_Terminal Terminal Functions
* @{
*/

/**
* @brief Open a terminal
*
* Opens a terminal device for I/O.
*
* @param context The I/O context
* @param device The terminal device name (NULL for default)
* @param flags Flags controlling how the terminal is opened
* @return A handle to the terminal, or NULL on failure
*/
sio_handle_t* sio_terminal_open(sio_context_t* context,
                              const char* device,
                              int flags);

/**
* @brief Set terminal mode
*
* Controls terminal modes such as echo, canonical, etc.
*
* @param terminal The terminal handle
* @param raw If true, set raw mode (no echo, no line buffering)
* @param echo If true, enable character echo
* @return SIO_SUCCESS on success, error code on failure
*/
int sio_terminal_set_mode(sio_handle_t* terminal,
                        bool raw,
                        bool echo);

/**
* @brief Get terminal size
*
* Retrieves the dimensions of a terminal.
*
* @param terminal The terminal handle
* @param width Pointer to store the width in columns
* @param height Pointer to store the height in rows
* @return SIO_SUCCESS on success, error code on failure
*/
int sio_terminal_get_size(sio_handle_t* terminal,
                        int* width,
                        int* height);

/** @} */

/**
* @defgroup SIO_Device Device Functions
* @{
*/

/**
* @brief Open a device
*
* Opens a device for I/O operations.
*
* @param context The I/O context
* @param device_path The path to the device
* @param flags Flags controlling how the device is opened
* @return A handle to the device, or NULL on failure
*/
sio_handle_t* sio_device_open(sio_context_t* context,
                            const char* device_path,
                            int flags);

/**
* @brief Control a device
*
* Sends a control command to a device.
*
* @param device The device handle
* @param request The control request code
* @param input Input data for the request
* @param input_size Size of the input data
* @param output Buffer for output data
* @param output_size Size of the output buffer
* @param bytes_returned Pointer to store the number of bytes returned
* @return SIO_SUCCESS on success, error code on failure
*/
int sio_device_control(sio_handle_t* device,
                    unsigned long request,
                    void* input,
                    size_t input_size,
                    void* output,
                    size_t output_size,
                    size_t* bytes_returned);

/** @} */

/**
* @defgroup SIO_Timer Timer Functions
* @{
*/

/**
* @brief Create a timer
*
* Creates a timer that can be used for timeouts and periodic events.
*
* @param context The I/O context
* @param initial_delay Initial delay before the first event (in milliseconds)
* @param interval Interval between periodic events (0 for one-shot)
* @return A handle to the timer, or NULL on failure
*/
sio_handle_t* sio_timer_create(sio_context_t* context,
                            uint64_t initial_delay,
                            uint64_t interval);

/**
* @brief Modify a timer
*
* Changes the timing parameters of a timer.
*
* @param timer The timer handle
* @param initial_delay Initial delay before the next event (in milliseconds)
* @param interval Interval between periodic events (0 for one-shot)
* @return SIO_SUCCESS on success, error code on failure
*/
int sio_timer_modify(sio_handle_t* timer,
                  uint64_t initial_delay,
                  uint64_t interval);

/**
* @brief Cancel a timer
*
* Stops a timer from generating events.
*
* @param timer The timer handle
* @return SIO_SUCCESS on success, error code on failure
*/
int sio_timer_cancel(sio_handle_t* timer);

/** @} */

/**
* @defgroup SIO_Signal Signal Functions
* @{
*/

/**
* @brief Register for signal events
*
* Sets up handling for system signals such as SIGINT, SIGTERM, etc.
*
* @param context The I/O context
* @param signal_number The signal number to handle
* @return A handle for the signal, or NULL on failure
*/
sio_handle_t* sio_signal_register(sio_context_t* context,
                                int signal_number);

/** @} */

/**
* @defgroup SIO_MessageQueue Message Queue Functions
* @{
*/

/**
* @brief Create a message queue
*
* Creates a message queue for inter-process communication.
*
* @param context The I/O context
* @param name The name of the message queue
* @param max_msg The maximum number of messages in the queue
* @param msg_size The maximum size of each message
* @return A handle to the message queue, or NULL on failure
*/
sio_handle_t* sio_msgqueue_create(sio_context_t* context,
                                const char* name,
                                size_t max_msg,
                                size_t msg_size);

/**
* @brief Open an existing message queue
*
* Opens an existing message queue by name.
*
* @param context The I/O context
* @param name The name of the message queue
* @return A handle to the message queue, or NULL on failure
*/
sio_handle_t* sio_msgqueue_open(sio_context_t* context,
                              const char* name);

/**
* @brief Send a message to a queue
*
* Places a message in a message queue.
*
* @param queue The message queue handle
* @param message The message data
* @param message_size The size of the message
* @param priority The priority of the message (higher values = higher priority)
* @return SIO_SUCCESS on success, error code on failure
*/
int sio_msgqueue_send(sio_handle_t* queue,
                    const void* message,
                    size_t message_size,
                    unsigned int priority);

/**
* @brief Receive a message from a queue
*
* Retrieves a message from a message queue.
*
* @param queue The message queue handle
* @param buffer Buffer to store the message
* @param buffer_size Size of the buffer
* @param message_size Pointer to store the actual message size
* @param priority Pointer to store the message priority
* @return SIO_SUCCESS on success, error code on failure
*/
int sio_msgqueue_receive(sio_handle_t* queue,
                      void* buffer,
                      size_t buffer_size,
                      size_t* message_size,
                      unsigned int* priority);

/** @} */

/**
* @defgroup SIO_SharedMemory Shared Memory Functions
* @{
*/

/**
* @brief Create a shared memory segment
*
* Creates a shared memory segment that can be accessed by multiple processes.
*
* @param context The I/O context
* @param name The name of the shared memory segment
* @param size The size of the shared memory segment
* @param mapped_addr Pointer to store the mapped address
* @return A handle to the shared memory segment, or NULL on failure
*/
sio_handle_t* sio_sharedmem_create(sio_context_t* context,
                                const char* name,
                                size_t size,
                                void** mapped_addr);

/**
* @brief Open an existing shared memory segment
*
* Opens an existing shared memory segment by name.
*
* @param context The I/O context
* @param name The name of the shared memory segment
* @param mapped_addr Pointer to store the mapped address
* @return A handle to the shared memory segment, or NULL on failure
*/
sio_handle_t* sio_sharedmem_open(sio_context_t* context,
                              const char* name,
                              void** mapped_addr);

/** @} */

/**
* @defgroup SIO_Memory Memory Stream Functions
* @{
*/

/**
* @brief Create a memory stream
*
* Creates a stream-like interface to a memory buffer.
*
* @param context The I/O context
* @param buffer The memory buffer (NULL to allocate internally)
* @param size The size of the buffer
* @param flags Flags controlling the memory stream behavior
* @return A handle to the memory stream, or NULL on failure
*/
sio_handle_t* sio_memory_create(sio_context_t* context,
                              void* buffer,
                              size_t size,
                              int flags);

/** @} */

/**
* @defgroup SIO_IO Input/Output Functions
* @{
*/

/**
* @brief Read data from a handle
*
* Reads data from a handle into a buffer.
* If the handle is in non-blocking mode, this may return SIO_WOULD_BLOCK.
*
* @param handle The I/O handle
* @param buffer The buffer to read into
* @param size The maximum number of bytes to read
* @param bytes_read Pointer to store the number of bytes read
* @return SIO_SUCCESS on success, error code on failure
*/
int sio_read(sio_handle_t* handle,
          void* buffer,
          size_t size,
          size_t* bytes_read);

/**
* @brief Read data from a handle at a specific offset
*
* Reads data from a handle into a buffer, starting at the specified offset.
* This is primarily useful for file handles.
*
* @param handle The I/O handle
* @param buffer The buffer to read into
* @param size The maximum number of bytes to read
* @param offset The offset to start reading from
* @param bytes_read Pointer to store the number of bytes read
* @return SIO_SUCCESS on success, error code on failure
*/
int sio_read_at(sio_handle_t* handle,
              void* buffer,
              size_t size,
              uint64_t offset,
              size_t* bytes_read);

/**
* @brief Read data from a handle into multiple buffers
*
* Performs scatter-gather I/O, reading data into multiple buffers.
*
* @param handle The I/O handle
* @param buffers Array of buffer descriptors
* @param buffer_count Number of buffers in the array
* @param bytes_read Pointer to store the total number of bytes read
* @return SIO_SUCCESS on success, error code on failure
*/
int sio_readv(sio_handle_t* handle,
            sio_buffer_t* buffers,
            int buffer_count,
            size_t* bytes_read);

/**
* @brief Write data to a handle
*
* Writes data from a buffer to a handle.
* If the handle is in non-blocking mode, this may return SIO_WOULD_BLOCK.
*
* @param handle The I/O handle
* @param buffer The buffer to write from
* @param size The number of bytes to write
* @param bytes_written Pointer to store the number of bytes written
* @return SIO_SUCCESS on success, error code on failure
*/
int sio_write(sio_handle_t* handle,
            const void* buffer,
            size_t size,
            size_t* bytes_written);

/**
* @brief Write data to a handle at a specific offset
*
* Writes data from a buffer to a handle, starting at the specified offset.
* This is primarily useful for file handles.
*
* @param handle The I/O handle
* @param buffer The buffer to write from
* @param size The number of bytes to write
* @param offset The offset to start writing at
* @param bytes_written Pointer to store the number of bytes written
* @return SIO_SUCCESS on success, error code on failure
*/
int sio_write_at(sio_handle_t* handle,
              const void* buffer,
              size_t size,
              uint64_t offset,
              size_t* bytes_written);

/**
* @brief Write data to a handle from multiple buffers
*
* Performs gather-scatter I/O, writing data from multiple buffers.
*
* @param handle The I/O handle
* @param buffers Array of buffer descriptors
* @param buffer_count Number of buffers in the array
* @param bytes_written Pointer to store the total number of bytes written
* @return SIO_SUCCESS on success, error code on failure
*/
int sio_writev(sio_handle_t* handle,
            const sio_buffer_t* buffers,
            int buffer_count,
            size_t* bytes_written);

/**
* @brief Perform zero-copy transfer between two handles
*
* Transfers data directly between two handles without copying to user space.
* This is most efficient for file-to-socket or socket-to-file transfers.
*
* @param src_handle Source handle
* @param dest_handle Destination handle
* @param size Number of bytes to transfer (0 for all available)
* @param src_offset Offset in the source (ignored for some handle types)
* @param bytes_transferred Pointer to store the number of bytes transferred
* @return SIO_SUCCESS on success, error code on failure
*/
int sio_transfer(sio_handle_t* src_handle,
              sio_handle_t* dest_handle,
              size_t size,
              uint64_t src_offset,
              size_t* bytes_transferred);

/**
* @brief Flush any buffered data to the underlying medium
*
* Ensures that any buffered data is written to the underlying medium.
*
* @param handle The I/O handle
* @return SIO_SUCCESS on success, error code on failure
*/
int sio_flush(sio_handle_t* handle);

/**
* @brief Close an I/O handle
*
* Closes the handle and releases associated resources.
*
* @param handle The I/O handle to close
* @return SIO_SUCCESS on success, error code on failure
*/
int sio_close(sio_handle_t* handle);

/** @} */

/**
* @defgroup SIO_Options Option Functions
* @{
*/

/**
* @brief Set an option on a handle
*
* Sets various options to control the behavior of a handle.
*
* @param handle The I/O handle
* @param option The option to set
* @param value Pointer to the option value
* @param value_size Size of the option value
* @return SIO_SUCCESS on success, error code on failure
*/
int sio_set_option(sio_handle_t* handle,
int option,
const void* value,
size_t value_size);

/**
* @brief Get an option from a handle
*
* Retrieves the current value of an option from a handle.
*
* @param handle The I/O handle
* @param option The option to get
* @param value Buffer to store the option value
* @param value_size Size of the value buffer
* @param actual_size Pointer to store the actual size of the option value
* @return SIO_SUCCESS on success, error code on failure
*/
int sio_get_option(sio_handle_t* handle,
int option,
void* value,
size_t value_size,
size_t* actual_size);

/**
* @brief Set socket option
*
* Sets a socket-specific option.
* This is a convenience wrapper around sio_set_option for socket handles.
*
* @param socket The socket handle
* @param level The protocol level (SOL_SOCKET, IPPROTO_TCP, etc.)
* @param option The socket option to set
* @param value Pointer to the option value
* @param value_size Size of the option value
* @return SIO_SUCCESS on success, error code on failure
*/
int sio_socket_set_option(sio_handle_t* socket,
        int level,
        int option,
        const void* value,
        size_t value_size);

/**
* @brief Get socket option
*
* Retrieves a socket-specific option.
* This is a convenience wrapper around sio_get_option for socket handles.
*
* @param socket The socket handle
* @param level The protocol level (SOL_SOCKET, IPPROTO_TCP, etc.)
* @param option The socket option to get
* @param value Buffer to store the option value
* @param value_size Size of the value buffer
* @param actual_size Pointer to store the actual size of the option value
* @return SIO_SUCCESS on success, error code on failure
*/
int sio_socket_get_option(sio_handle_t* socket,
        int level,
        int option,
        void* value,
        size_t value_size,
        size_t* actual_size);

/**
* @brief Set file option
*
* Sets a file-specific option.
* This is a convenience wrapper around sio_set_option for file handles.
*
* @param file The file handle
* @param option The file option to set
* @param value Pointer to the option value
* @param value_size Size of the option value
* @return SIO_SUCCESS on success, error code on failure
*/
int sio_file_set_option(sio_handle_t* file,
      int option,
      const void* value,
      size_t value_size);

/**
* @brief Get file option
*
* Retrieves a file-specific option.
* This is a convenience wrapper around sio_get_option for file handles.
*
* @param file The file handle
* @param option The file option to get
* @param value Buffer to store the option value
* @param value_size Size of the value buffer
* @param actual_size Pointer to store the actual size of the option value
* @return SIO_SUCCESS on success, error code on failure
*/
int sio_file_get_option(sio_handle_t* file,
      int option,
      void* value,
      size_t value_size,
      size_t* actual_size);

/** @} */

/**
* @defgroup SIO_EventHandling Event Handling Functions
* @{
*/

/**
* @brief Register a handle for event monitoring
*
* Registers interest in specific events on a handle.
*
* @param context The I/O context
* @param handle The handle to monitor
* @param event_mask Bitmask of events to monitor (SIO_MASK_* values)
* @param callback Function to call when events occur
* @param user_data User data to pass to the callback
* @return SIO_SUCCESS on success, error code on failure
*/
int sio_register(sio_context_t* context,
sio_handle_t* handle,
sio_event_mask_t event_mask,
sio_event_callback_t callback,
void* user_data);

/**
* @brief Modify the events being monitored for a handle
*
* Updates the set of events being monitored for a previously registered handle.
*
* @param context The I/O context
* @param handle The handle to update
* @param event_mask New bitmask of events to monitor
* @return SIO_SUCCESS on success, error code on failure
*/
int sio_modify(sio_context_t* context,
sio_handle_t* handle,
sio_event_mask_t event_mask);

/**
* @brief Unregister a handle from event monitoring
*
* Stops monitoring events for a previously registered handle.
*
* @param context The I/O context
* @param handle The handle to unregister
* @return SIO_SUCCESS on success, error code on failure
*/
int sio_unregister(sio_context_t* context,
sio_handle_t* handle);

/**
* @brief Wait for events
*
* Waits for events on registered handles and dispatches them to callbacks.
* This blocks until events occur or the timeout expires.
*
* @param context The I/O context
* @param timeout_ms Maximum time to wait in milliseconds (-1 for infinite)
* @param max_events Maximum number of events to process
* @return Number of events processed, or error code on failure
*/
int sio_wait(sio_context_t* context,
int timeout_ms,
int max_events);

/**
* @brief Dispatch pending events
*
* Processes any pending events without waiting.
* This is non-blocking and returns immediately if no events are pending.
*
* @param context The I/O context
* @param max_events Maximum number of events to process
* @return Number of events processed, or error code on failure
*/
int sio_dispatch(sio_context_t* context,
int max_events);

/**
* @brief Start an event loop
*
* Starts a dedicated event loop that runs until stopped.
* This typically runs in a separate thread.
*
* @param context The I/O context
* @return SIO_SUCCESS on success, error code on failure
*/
int sio_event_loop_start(sio_context_t* context);

/**
* @brief Stop an event loop
*
* Stops a previously started event loop.
*
* @param context The I/O context
* @return SIO_SUCCESS on success, error code on failure
*/
int sio_event_loop_stop(sio_context_t* context);

/** @} */

/**
* @defgroup SIO_AsyncIO Asynchronous I/O Functions
* @{
*/

/**
* @brief Perform an asynchronous read operation
*
* Initiates an asynchronous read operation.
* The callback will be invoked when the operation completes.
*
* @param handle The I/O handle
* @param buffer The buffer to read into
* @param size The maximum number of bytes to read
* @param offset The offset to start reading from (ignored for some handle types)
* @param callback Function to call when the operation completes
* @param user_data User data to pass to the callback
* @return SIO_SUCCESS on success, error code on failure
*/
int sio_async_read(sio_handle_t* handle,
void* buffer,
size_t size,
uint64_t offset,
sio_event_callback_t callback,
void* user_data);

/**
* @brief Perform an asynchronous scatter-gather read operation
*
* Initiates an asynchronous read operation into multiple buffers.
* The callback will be invoked when the operation completes.
*
* @param handle The I/O handle
* @param buffers Array of buffer descriptors
* @param buffer_count Number of buffers in the array
* @param offset The offset to start reading from (ignored for some handle types)
* @param callback Function to call when the operation completes
* @param user_data User data to pass to the callback
* @return SIO_SUCCESS on success, error code on failure
*/
int sio_async_readv(sio_handle_t* handle,
  sio_buffer_t* buffers,
  int buffer_count,
  uint64_t offset,
  sio_event_callback_t callback,
  void* user_data);

/**
* @brief Perform an asynchronous write operation
*
* Initiates an asynchronous write operation.
* The callback will be invoked when the operation completes.
*
* @param handle The I/O handle
* @param buffer The buffer to write from
* @param size The number of bytes to write
* @param offset The offset to start writing at (ignored for some handle types)
* @param callback Function to call when the operation completes
* @param user_data User data to pass to the callback
* @return SIO_SUCCESS on success, error code on failure
*/
int sio_async_write(sio_handle_t* handle,
  const void* buffer,
  size_t size,
  uint64_t offset,
  sio_event_callback_t callback,
  void* user_data);

/**
* @brief Perform an asynchronous gather-scatter write operation
*
* Initiates an asynchronous write operation from multiple buffers.
* The callback will be invoked when the operation completes.
*
* @param handle The I/O handle
* @param buffers Array of buffer descriptors
* @param buffer_count Number of buffers in the array
* @param offset The offset to start writing at (ignored for some handle types)
* @param callback Function to call when the operation completes
* @param user_data User data to pass to the callback
* @return SIO_SUCCESS on success, error code on failure
*/
int sio_async_writev(sio_handle_t* handle,
  const sio_buffer_t* buffers,
  int buffer_count,
  uint64_t offset,
  sio_event_callback_t callback,
  void* user_data);

/**
* @brief Perform an asynchronous accept operation
*
* Initiates an asynchronous operation to accept a connection.
* The callback will be invoked when a connection is accepted.
*
* @param socket The listening socket handle
* @param callback Function to call when a connection is accepted
* @param user_data User data to pass to the callback
* @return SIO_SUCCESS on success, error code on failure
*/
int sio_async_accept(sio_handle_t* socket,
  sio_event_callback_t callback,
  void* user_data);

/**
* @brief Perform an asynchronous connect operation
*
* Initiates an asynchronous operation to establish a connection.
* The callback will be invoked when the connection is established or fails.
*
* @param socket The socket handle
* @param address The remote address string
* @param port The remote port number
* @param callback Function to call when the connection completes
* @param user_data User data to pass to the callback
* @return SIO_SUCCESS on success, error code on failure
*/
int sio_async_connect(sio_handle_t* socket,
    const char* address,
    uint16_t port,
    sio_event_callback_t callback,
    void* user_data);

/**
* @brief Cancel an asynchronous operation
*
* Attempts to cancel a pending asynchronous operation.
* The callback will be invoked with a cancellation status if successful.
*
* @param handle The I/O handle with pending operations
* @return SIO_SUCCESS on success, error code on failure
*/
int sio_async_cancel(sio_handle_t* handle);

/**
* @brief Perform an asynchronous zero-copy transfer
*
* Initiates an asynchronous transfer between two handles without copying to user space.
* The callback will be invoked when the transfer completes.
*
* @param src_handle Source handle
* @param dest_handle Destination handle
* @param size Number of bytes to transfer (0 for all available)
* @param src_offset Offset in the source (ignored for some handle types)
* @param callback Function to call when the transfer completes
* @param user_data User data to pass to the callback
* @return SIO_SUCCESS on success, error code on failure
*/
int sio_async_transfer(sio_handle_t* src_handle,
    sio_handle_t* dest_handle,
    size_t size,
    uint64_t src_offset,
    sio_event_callback_t callback,
    void* user_data);

/**
* @brief Register a buffer for optimized I/O
*
* Registers a buffer with the system for optimized I/O operations.
* This can reduce memory copies on some platforms.
*
* @param context The I/O context
* @param buffer The buffer to register
* @param size The size of the buffer
* @return A registered buffer, or NULL on failure
*/
sio_buffer_t* sio_buffer_register(sio_context_t* context,
                void* buffer,
                size_t size);

/**
* @brief Unregister a previously registered buffer
*
* Releases a buffer previously registered with sio_buffer_register.
*
* @param buffer The registered buffer
* @return SIO_SUCCESS on success, error code on failure
*/
int sio_buffer_unregister(sio_buffer_t* buffer);

/**
* @brief Allocate an aligned buffer for optimal I/O performance
*
* Allocates a buffer with proper alignment for optimal I/O performance.
* The buffer is automatically registered with the system if supported.
*
* @param context The I/O context
* @param size The size of the buffer
* @param alignment The alignment requirement (0 for system default)
* @return A registered buffer, or NULL on failure
*/
sio_buffer_t* sio_buffer_alloc(sio_context_t* context,
            size_t size,
            size_t alignment);

/**
* @brief Free a buffer allocated with sio_buffer_alloc
*
* Releases a buffer previously allocated with sio_buffer_alloc.
*
* @param buffer The buffer to free
*/
void sio_buffer_free(sio_buffer_t* buffer);

/** @} */

/**
* @defgroup SIO_Utility Utility Functions
* @{
*/

/**
* @brief Get a string representation of an error code
*
* Converts an error code to a human-readable string.
*
* @param error The error code
* @return A string describing the error
*/
const char* sio_error_string(int error);

/**
* @brief Get information about a handle
*
* Retrieves detailed information about a handle.
*
* @param handle The I/O handle
* @param type Pointer to store the handle type
* @param flags Pointer to store the handle flags
* @param native_handle Pointer to store the native OS handle/descriptor
* @return SIO_SUCCESS on success, error code on failure
*/
int sio_handle_info(sio_handle_t* handle,
  sio_handle_type_t* type,
  int* flags,
  void** native_handle);

/**
* @brief Set handle to non-blocking mode
*
* Configures a handle for non-blocking I/O operations.
*
* @param handle The I/O handle
* @param non_blocking If true, set non-blocking mode; if false, set blocking mode
* @return SIO_SUCCESS on success, error code on failure
*/
int sio_set_nonblocking(sio_handle_t* handle,
      bool non_blocking);

/**
* @brief Create a pair of connected handles
*
* Creates a pair of connected handles (like socketpair or pipe).
*
* @param context The I/O context
* @param type The type of handles to create
* @param flags Additional flags
* @param handle1 Pointer to store the first handle
* @param handle2 Pointer to store the second handle
* @return SIO_SUCCESS on success, error code on failure
*/
int sio_pair(sio_context_t* context,
sio_handle_type_t type,
int flags,
sio_handle_t** handle1,
sio_handle_t** handle2);

/**
* @brief Duplicate a handle
*
* Creates a duplicate of an existing handle.
*
* @param handle The handle to duplicate
* @return A new handle, or NULL on failure
*/
sio_handle_t* sio_dup(sio_handle_t* handle);

/**
* @brief Set I/O priority
*
* Sets the priority level for I/O operations on a handle.
*
* @param handle The I/O handle
* @param priority The priority level (platform-specific)
* @return SIO_SUCCESS on success, error code on failure
*/
int sio_set_priority(sio_handle_t* handle,
  int priority);

/** @} */

/**
* @defgroup SIO_Macros Common Macros
* @{
*/

/**
* @brief Check if a handle is valid
*
* Tests whether a handle is valid and can be used.
*
* @param handle The handle to check
* @return true if the handle is valid, false otherwise
*/
#define SIO_HANDLE_IS_VALID(handle) ((handle) != NULL)

/**
* @brief Get the type of a handle
*
* Retrieves the type of a handle.
*
* @param handle The handle to query
* @param type Pointer to store the handle type
* @return SIO_SUCCESS on success, error code on failure
*/
#define SIO_HANDLE_GET_TYPE(handle, type) sio_handle_info((handle), (type), NULL, NULL)

/**
* @brief Infinite timeout value
*
* Use this value for timeout parameters to wait indefinitely.
*/
#define SIO_INFINITE_TIMEOUT (-1)

/**
* @brief No timeout value
*
* Use this value for timeout parameters to return immediately.
*/
#define SIO_NO_TIMEOUT 0

/** @} */

#endif /* SIO_HEADER_DEFINED */

#ifdef __cplusplus
}
#endif