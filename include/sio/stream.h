/**
* @file sio/stream.h
* @brief Simple I/O (SIO) - Cross-platform I/O library for high-performance systems programming
*
* A unified streaming interface for various I/O types including files, sockets, pipes,
* signals, message queues, shared memory, memory buffers, and terminals.
*
* @author zczxy
* @version 0.1.0
*/

#ifndef SIO_STREAM_H
#define SIO_STREAM_H

#ifdef __cplusplus
extern "C" {
#endif

#include <sio/aux/addr.h>
#include <sio/platform.h>
#include <sio/err.h>
#include <sio/buf.h>
#include <stdint.h>
#include <stddef.h>

/* Platform-specific includes */
#if defined(SIO_OS_WINDOWS)
#elif defined(SIO_OS_POSIX)
  #include <mqueue.h>
  #include <termios.h>
#else
  #error "addr.h - Unsupported operating system"
#endif

/**
* @brief Stream types
*/
enum sio_stream_type {
  SIO_STREAM_UNKNOWN = 0,   /**< Unknown stream type */
  SIO_STREAM_FILE,          /**< File stream */
  SIO_STREAM_SOCKET,        /**< Socket stream */
  SIO_STREAM_PSEUDO_SOCKET, /**< Pseudo socket stream (consists of a socket address for udp writeto and recvfrom compatibility) */
  SIO_STREAM_PIPE,          /**< Pipe stream */
  SIO_STREAM_TIMER,         /**< Timer stream */
  SIO_STREAM_SIGNAL,        /**< Signal stream */
  SIO_STREAM_MSGQUEUE,      /**< Message queue stream */
  SIO_STREAM_SHMEM,         /**< Shared memory stream */
  SIO_STREAM_BUFFER,        /**< Memory buffer stream */
  SIO_STREAM_RAWMEM,        /**< Raw memory stream */
  SIO_STREAM_TERMINAL,      /**< Terminal stream */
  SIO_STREAM_CUSTOM         /**< Custom user-defined stream */
};

typedef enum sio_stream_type sio_stream_type_t;

/**
* @brief Stream flags used for open and operation modes
*/
enum sio_stream_flags {
  /* Access modes */
  SIO_STREAM_READ       = (1 << 0),   /**< Open for reading */
  SIO_STREAM_WRITE      = (1 << 1),   /**< Open for writing */
  SIO_STREAM_RDWR       = (SIO_STREAM_READ | SIO_STREAM_WRITE), /**< Open for reading and writing */
  
  /* Creation modes */
  SIO_STREAM_CREATE     = (1 << 2),   /**< Create if doesn't exist */
  SIO_STREAM_EXCL       = (1 << 3),   /**< Fail if exists with CREATE */
  SIO_STREAM_TRUNC      = (1 << 4),   /**< Truncate if exists */
  SIO_STREAM_APPEND     = (1 << 5),   /**< Append to end of stream */
  
  /* Blocking modes */
  SIO_STREAM_NONBLOCK   = (1 << 6),   /**< Non-blocking operations */
  SIO_STREAM_ASYNC      = (1 << 7),   /**< Asynchronous operations */
  
  /* Caching modes */
  SIO_STREAM_UNBUFFERED = (1 << 8),   /**< Unbuffered I/O */
  SIO_STREAM_SYNC       = (1 << 9),   /**< Synchronous I/O */
  
  /* Special modes */
  SIO_STREAM_TEMP       = (1 << 10),  /**< Temporary (for files) */
  SIO_STREAM_BINARY     = (1 << 11),  /**< Binary mode (for files) */
  SIO_STREAM_MMAP       = (1 << 12),  /**< Use memory mapping if possible */
  SIO_STREAM_DIRECT     = (1 << 13),  /**< Direct I/O (bypass cache if possible) */
  SIO_SERVER            = (1 << 14)   /**< Set the stream to be a host for other streams if applicable */
};

typedef enum sio_stream_flags sio_stream_flags_t;

/**
* @brief Stream option identifiers for configuration
*/
enum sio_stream_option {
  /* General options (0-99) */
  SIO_OPT_TIMEOUT = 1,          /**< I/O timeout in milliseconds (int32_t) */
  SIO_OPT_BUFFER_SIZE,          /**< Internal buffer size (size_t) */
  SIO_OPT_BLOCKING,             /**< Blocking mode (int: 0=nonblocking, 1=blocking) */
  SIO_OPT_CLOSE_ON_EXEC,        /**< Close on exec (int: 0=no, 1=yes) */
  SIO_OPT_AUTOCLOSE,            /**< Auto close file descriptors on stream close (int) */
  
  /* File-specific options (100-199) */
  SIO_OPT_FILE_APPEND = 100,    /**< Append mode (int) */
  SIO_OPT_FILE_SYNC,            /**< Sync after write (int) */
  SIO_OPT_FILE_DIRECT,          /**< Direct I/O (int) */
  SIO_OPT_FILE_SPARSE,          /**< Enable sparse file (int) */
  SIO_OPT_FILE_MMAP,            /**< Memory-mapped I/O (int) */
  
  /* Socket-specific options (200-299) */
  SIO_OPT_SOCK_NODELAY = 200,   /**< TCP no delay (int) */
  SIO_OPT_SOCK_KEEPALIVE,       /**< TCP keepalive (int) */
  SIO_OPT_SOCK_REUSEADDR,       /**< Reuse address (int) */
  SIO_OPT_SOCK_BROADCAST,       /**< Allow broadcast (int) */
  SIO_OPT_SOCK_RCVBUF,          /**< Receive buffer size (int) */
  SIO_OPT_SOCK_SNDBUF,          /**< Send buffer size (int) */
  SIO_OPT_SOCK_LINGER,          /**< Linger on close (struct linger) */
  SIO_OPT_SOCK_OOBINLINE,       /**< Receive OOB data inline (int) */
  SIO_OPT_SOCK_DONTROUTE,       /**< Don't route (int) */
  SIO_OPT_SOCK_RCVTIMEO,        /**< Receive timeout (struct timeval) */
  SIO_OPT_SOCK_SNDTIMEO,        /**< Send timeout (struct timeval) */
  SIO_OPT_SOCK_RCVLOWAT,        /**< Receive low water mark (int) */
  SIO_OPT_SOCK_SNDLOWAT,        /**< Send low water mark (int) */
  
  /* Timer-specific options (300-399) */
  SIO_OPT_TIMER_INTERVAL = 300, /**< Timer interval in milliseconds (int32_t) */
  SIO_OPT_TIMER_ONESHOT,        /**< One-shot timer (int) */
  
  /* Terminal-specific options (400-499) */
  SIO_OPT_TERM_ECHO = 400,      /**< Terminal echo (int) */
  SIO_OPT_TERM_CANONICAL,       /**< Canonical mode (int) */
  SIO_OPT_TERM_RAW,             /**< Raw mode (int) */
  SIO_OPT_TERM_COLOR,           /**< Color support (int) */
  
  /* Stream information (read-only) */
  SIO_INFO_TYPE = 1000,         /**< Stream type (sio_stream_type_t) */
  SIO_INFO_FLAGS,               /**< Stream flags (int) */
  SIO_INFO_POSITION,            /**< Current position (uint64_t) */
  SIO_INFO_SIZE,                /**< Total size, if available (uint64_t) */
  SIO_INFO_READABLE,            /**< Is readable? (int) */
  SIO_INFO_WRITABLE,            /**< Is writable? (int) */
  SIO_INFO_SEEKABLE,            /**< Is seekable? (int) */
  SIO_INFO_EOF,                 /**< At end of stream? (int) */
  SIO_INFO_ERROR,               /**< Last error (sio_error_t) */
  SIO_INFO_HANDLE,              /**< Native handle (platform-specific) */
  SIO_INFO_BUFFER_SIZE          /**< Current buffer size (size_t) */
};

typedef enum sio_stream_option sio_stream_option_t;

/**
* @brief Seek origin values for stream seeking
*/
enum sio_seek_origin {
  SIO_SEEK_SET = 0,   /**< Seek from beginning */
  SIO_SEEK_CUR = 1,   /**< Seek from current position */
  SIO_SEEK_END = 2    /**< Seek from end */
};

typedef enum sio_seek_origin sio_seek_origin_t;

/**
* @brief Function options for configuration
*/
enum sio_stream_fflag {
  SIO_DOALL,
  SIO_DOALL_NONBLOCK,
  SIO_MSG_CONFIRM = MSG_CONFIRM, 
  SIO_MSG_DONTROUTE = MSG_DONTROUTE,
  SIO_MSG_DONTWAIT = MSG_DONTWAIT, 
  SIO_MSG_EOR = MSG_EOR, 
  SIO_MSG_MORE = MSG_MORE, 
  SIO_MSG_NOSIGNAL = MSG_NOSIGNAL, 
  SIO_MSG_OOB = MSG_OOB,
  SIO_MSG_FASTOPEN = MSG_FASTOPEN
};
typedef enum sio_stream_fflag sio_stream_fflag_t;

/**
* @brief Forward declaration of stream operation vtable
*/
struct sio_iovec_t {
  #if defined(SIO_OS_WINDOWS)
    ULONG len;
    CHAR *buf;
  #else
    void  *iov_base;    /* Starting address */
    size_t iov_len;     /* Number of bytes to transfer */
  #endif
}

// forward declare see reference below
struct sio_stream_ops;

/**
* @brief Stream context structure
* 
* This union contains all the possible stream type variations
* including all possible stream types.
*/
typedef union sio_stream_storage {
  /* File stream data */
  struct {
  #if defined(SIO_OS_WINDOWS)
    HANDLE handle;                   /**< Windows file handle */
  #else
    int fd;                          /**< POSIX file descriptor */
  #endif
    void *mmap_data;                 /**< Memory-mapped data */
    size_t mmap_size;                /**< Memory-mapped size */
  } file;
  
  /* Socket stream data */
  struct {
  #if defined(SIO_OS_WINDOWS)
    SOCKET socket;                   /**< Windows socket */
  #else
    int fd;                          /**< POSIX socket descriptor */
  #endif
  } socket;

  struct {
    sio_addr_t addr;
  } pseudo_socket;
  
  /* Pipe stream data */
  struct {
  #if defined(SIO_OS_WINDOWS)
    HANDLE read_handle;              /**< Windows pipe read handle */
    HANDLE write_handle;             /**< Windows pipe write handle */
  #else
    int read_fd;                     /**< POSIX pipe read descriptor */
    int write_fd;                    /**< POSIX pipe write descriptor */
  #endif
  } pipe;
  
  /* Timer stream data */
  struct {
  #if defined(SIO_OS_WINDOWS)
    HANDLE timer;                    /**< Windows timer handle */
  #else
    int fd;                          /**< POSIX timer file descriptor */
    timer_t timerid;                 /**< POSIX timer ID */
  #endif
  } timer;
  
  /* Signal stream data */
  struct {
  #if defined(SIO_OS_WINDOWS)
    HANDLE event;                    /**< Windows event handle */
  #else
    int fd;                          /**< POSIX signalfd descriptor */
    sigset_t mask;                   /**< Signal mask */
  #endif
  } signal;
  
  /* Message queue stream data */
  struct {
  #if defined(SIO_OS_WINDOWS)
    HANDLE handle;                   /**< Windows message queue handle */
  #else
    int fd;                          /**< POSIX message queue descriptor */
    mqd_t mqd;                       /**< POSIX message queue descriptor */
    char *name;                      /**< Queue name (owned by stream) */
  #endif
    size_t msg_size;                 /**< Message size */
  } msgqueue;
  
  /* Shared memory stream data */
  struct {
  #if defined(SIO_OS_WINDOWS)
    HANDLE mapping;                  /**< Windows file mapping object */
  #else
    int fd;                          /**< POSIX shared memory descriptor */
    char *name;                      /**< Shared memory name (owned by stream) */
  #endif
    void *addr;                      /**< Mapped address */
    size_t size;                     /**< Mapped size */
    int is_owner;                    /**< Is owner (should unlink on close) */
  } shmem;
  
  /* Buffer stream data */
  struct {
    sio_buffer_t *buffer;            /**< Buffer reference */
    int owns_buffer;                 /**< Whether the stream owns the buffer */
  } buffer;
  
  /* Raw memory stream data */
  struct {
    void *data;                       /**< Memory data */
    size_t size;                      /**< Memory size */
    size_t position;                  /**< Current position */
  } rawmem;
  
  /* Terminal stream data */
  struct {
  #if defined(SIO_OS_WINDOWS)
    HANDLE input;                     /**< Windows console input handle */
    HANDLE output;                    /**< Windows console output handle */
    DWORD orig_input_mode;            /**< Original input mode */
    DWORD orig_output_mode;           /**< Original output mode */
  #else
    int fd;                           /**< POSIX terminal file descriptor */
    struct termios orig_termios;      /**< Original terminal attributes */
  #endif
    int is_raw;                       /**< Is in raw mode? */
    int is_canonical;                 /**< Is in canonical mode? */
    int has_color;                    /**< Has color support? */
  } terminal;
  
  /* Custom user-defined data */
  struct {
    void *data;                        /**< User data */
    size_t size;                       /**< Data size */
  } custom;
} sio_stream_storage_t;


/**
* @brief Stream context structure
* 
* This structure contains all the necessary information for a stream
* including type, flags, state, and platform-specific handles.
*/
typedef struct sio_stream {
  const struct sio_stream_ops *ops;    /**< Stream operations */
  
  /* Union for platform-specific and stream-type-specific data */
  sio_stream_storage_t data;

  sio_stream_type_t type;              /**< Stream type */
  int flags;                           /**< Stream flags */  
} sio_stream_t;

/**
* @brief Buffered stream context structure
* 
* This structure contains all the necessary information for a stream
* including type, flags, state, platform-specific handles and a buffer.
*/
typedef struct sio_stream_buffered {
  const struct sio_stream_ops *ops; /**< Stream operations */

  sio_stream_storage_t data; /* Union for platform-specific and stream-type-specific data */

  sio_stream_type_t type;    /**< Stream type */
  int flags;                 /**< Stream flags */

  sio_buffer_t buffer;       /**< Optional buffer */  
} sio_stream_buffered_t;

/**
* @brief Stream operations vtable
* 
* Virtual function table for stream operations. Each stream type
* implements its own set of operations.
*/
typedef struct sio_stream_ops {
  /* Core operations */
  sio_error_t (*close)(sio_stream_t *stream);
  sio_error_t (*read)(sio_stream_t *stream, void *buffer, size_t size, size_t *bytes_read, int flags);
  sio_error_t (*write)(sio_stream_t *stream, const void *buffer, size_t size, size_t *bytes_written, int flags);
  sio_error_t (*readv)(sio_stream_t *stream, sio_iovec_t *buf, size_t len, size_t *bytes_read, int flags);
  sio_error_t (*writev)(sio_stream_t *stream, const sio_iovec_t *buf, size_t len, size_t *bytes_written, int flags);

  /* Other operations */
  sio_error_t (*flush)(sio_stream_buffered_t *stream);

  /* Advanced operations */
  sio_error_t (*get_option)(sio_stream_t *stream, sio_stream_option_t option, void *value, size_t *size);
  sio_error_t (*set_option)(sio_stream_t *stream, sio_stream_option_t option, const void *value, size_t size);

  /* Optional operations - can be NULL if not implemented */
  sio_error_t (*seek)(sio_stream_t *stream, int64_t offset, sio_seek_origin_t origin, uint64_t *new_position);
  sio_error_t (*tell)(sio_stream_t *stream, uint64_t *position);
  sio_error_t (*truncate)(sio_stream_t *stream, uint64_t size);
  sio_error_t (*get_size)(sio_stream_t *stream, uint64_t *size);
} sio_stream_ops_t;

/* 
 * Stream creation functions for various stream types
 */

/**
* @brief Create a file stream
* 
* @param stream Pointer to stream structure to initialize
* @param path File path
* @param opt Combination of SIO_STREAM_* flags
* @param mode File mode for creation (e.g., 0644)
* @return sio_error_t SIO_SUCCESS or error code
*/
SIO_EXPORT sio_error_t sio_stream_open_file(sio_stream_t *stream, const char *path, sio_stream_flags_t opt, int mode);

/**
* @brief Create a socket stream
* 
* @param stream Pointer to stream structure to initialize
* @param addr SimpleIO Socket address
* @param opt Combination of SIO_STREAM_* flags
* @return sio_error_t SIO_SUCCESS or error code
*/
SIO_EXPORT sio_error_t sio_stream_open_socket(sio_stream_t *stream, sio_addr_t *addr, sio_stream_flags_t opt);

/**
* @brief Create a pipe stream
* 
* @param read_stream Pointer to stream structure for reading
* @param write_stream Pointer to stream structure for writing
* @param opt Combination of SIO_STREAM_* flags
* @return sio_error_t SIO_SUCCESS or error code
*/
SIO_EXPORT sio_error_t sio_stream_open_pipe(sio_stream_t *read_stream, sio_stream_t *write_stream, sio_stream_flags_t opt);

/**
* @brief Create a timer stream
* 
* @param stream Pointer to stream structure to initialize
* @param interval_ms Timer interval in milliseconds
* @param is_oneshot Whether timer fires once (1) or repeatedly (0)
* @param opt Combination of SIO_STREAM_* flags
* @return sio_error_t SIO_SUCCESS or error code
*/
SIO_EXPORT sio_error_t sio_stream_open_timer(sio_stream_t *stream, uint64_t interval_ms, int is_oneshot, sio_stream_flags_t opt);

/**
* @brief Create a signal stream
* 
* @param stream Pointer to stream structure to initialize
* @param signals Array of signal numbers
* @param signal_count Number of signals in the array
* @param opt Combination of SIO_STREAM_* flags
* @return sio_error_t SIO_SUCCESS or error code
*/
SIO_EXPORT sio_error_t sio_stream_open_signal(sio_stream_t *stream, const int *signals, size_t signal_count, sio_stream_flags_t opt);

/**
* @brief Create a message queue stream
* 
* @param stream Pointer to stream structure to initialize
* @param name Message queue name
* @param max_msg Maximum number of messages in queue
* @param msg_size Size of each message
* @param opt Combination of SIO_STREAM_* flags
* @return sio_error_t SIO_SUCCESS or error code
*/
SIO_EXPORT sio_error_t sio_stream_open_msgqueue(sio_stream_t *stream, const char *name, size_t max_msg, size_t msg_size, sio_stream_flags_t opt);

/**
* @brief Create a shared memory stream
* 
* @param stream Pointer to stream structure to initialize
* @param name Shared memory segment name
* @param size Size of shared memory segment
* @param opt Combination of SIO_STREAM_* flags
* @return sio_error_t SIO_SUCCESS or error code
*/
SIO_EXPORT sio_error_t sio_stream_open_shmem(sio_stream_t *stream, const char *name, size_t size, sio_stream_flags_t opt);

/**
* @brief Create a buffer stream
* 
* @param stream Pointer to stream structure to initialize
* @param buffer Pointer to existing buffer (NULL to create new one)
* @param initial_size Initial size if creating a new buffer
* @param opt Combination of SIO_STREAM_* flags
* @return sio_error_t SIO_SUCCESS or error code
*/
SIO_EXPORT sio_error_t sio_stream_open_buffer(sio_stream_t *stream, sio_buffer_t *buffer, size_t initial_size, sio_stream_flags_t opt);

/**
* @brief Create a raw memory stream
* 
* @param stream Pointer to stream structure to initialize
* @param memory Pointer to memory block
* @param size Size of memory block
* @param opt Combination of SIO_STREAM_* flags
* @return sio_error_t SIO_SUCCESS or error code
*/
SIO_EXPORT sio_error_t sio_stream_open_memory(sio_stream_t *stream, void *memory, size_t size, sio_stream_flags_t opt);

/**
* @brief Create a terminal stream
* 
* @param stream Pointer to stream structure to initialize
* @param opt Combination of SIO_STREAM_* flags
* @return sio_error_t SIO_SUCCESS or error code
*/
SIO_EXPORT sio_error_t sio_stream_open_terminal(sio_stream_t *stream, const char *name, sio_stream_flags_t opt);

/**
* @brief Create a custom stream with user-defined operations
* 
* @param stream Pointer to stream structure to initialize
* @param ops Pointer to operations table
* @param user_data User data pointer
* @param opt Combination of SIO_STREAM_* flags
* @return sio_error_t SIO_SUCCESS or error code
*/
SIO_EXPORT sio_error_t sio_stream_open_custom(sio_stream_t *stream, const sio_stream_ops_t *ops, void *user_data, sio_stream_flags_t opt);

/**
* @brief Create a stream from an existing file descriptor or handle
* 
* @param stream Pointer to stream structure to initialize
* @param fd_or_handle File descriptor (POSIX) or handle (Windows)
* @param type Type of stream to create
* @param opt Combination of SIO_STREAM_* flags
* @return sio_error_t SIO_SUCCESS or error code
*/
SIO_EXPORT sio_error_t sio_stream_from_handle(sio_stream_t *stream, void *fd_or_handle, sio_stream_type_t type, sio_stream_flags_t opt);

/*
 * Core stream operations
 */

/**
* @brief Close a stream and release resources
* 
* @param stream Stream to close
* @return sio_error_t SIO_SUCCESS or error code
*/
SIO_EXPORT sio_error_t sio_stream_close(sio_stream_t *stream);

/**
* @brief Read data from a stream
* 
* @param stream Stream to read from
* @param buffer Buffer to read into
* @param size Maximum number of bytes to read
* @param bytes_read Pointer to store actual bytes read (can be NULL)
* @param flags sio flags like readall or socket flags in send
* @return sio_error_t SIO_SUCCESS or error code (SIO_ERROR_EOF at end of stream)
*/
SIO_EXPORT sio_error_t sio_stream_read(sio_stream_t *stream, void *buffer, size_t size, size_t *bytes_read, sio_stream_fflag_t flags);

/**
* @brief Write data to a stream
* 
* @param stream Stream to write to
* @param buffer Buffer containing data to write
* @param size Number of bytes to write
* @param bytes_written Pointer to store actual bytes written (can be NULL)
* @param flags sio flags like writeall or socket flags in send
* @return sio_error_t SIO_SUCCESS or error code
*/
SIO_EXPORT sio_error_t sio_stream_write(sio_stream_t *stream, const void *buffer, size_t size, size_t *bytes_written, sio_stream_fflag_t flags);

/**
* @brief Flush buffered data to the underlying device
* 
* @param stream Stream to flush
* @return sio_error_t SIO_SUCCESS or error code
*/
SIO_EXPORT sio_error_t sio_stream_flush(sio_stream_buffered_t *stream);

/*
 * Extended stream operations (may not be supported by all stream types)
 */

/**
* @brief Seek to a position in a stream
* 
* @param stream Stream to seek in
* @param offset Offset to seek to
* @param origin Origin of seek (SIO_SEEK_SET, SIO_SEEK_CUR, SIO_SEEK_END)
* @param new_position Pointer to store new position (can be NULL)
* @return sio_error_t SIO_SUCCESS or error code
*/
SIO_EXPORT sio_error_t sio_stream_seek(sio_stream_t *stream, int64_t offset, sio_seek_origin_t origin, uint64_t *new_position);

/**
* @brief Get the current position in a stream
* 
* @param stream Stream to get position of
* @param position Pointer to store position
* @return sio_error_t SIO_SUCCESS or error code
*/
SIO_EXPORT sio_error_t sio_stream_tell(sio_stream_t *stream, uint64_t *position);

/**
* @brief Truncate a stream to a specified size
* 
* @param stream Stream to truncate
* @param size Size to truncate to
* @return sio_error_t SIO_SUCCESS or error code
*/
SIO_EXPORT sio_error_t sio_stream_truncate(sio_stream_t *stream, uint64_t size);

/**
* @brief Get the size of a stream (if available)
* 
* @param stream Stream to get size of
* @param size Pointer to store size
* @return sio_error_t SIO_SUCCESS or error code
*/
SIO_EXPORT sio_error_t sio_stream_get_size(sio_stream_t *stream, uint64_t *size);

/*
 * Stream property and option functions
 */

/**
* @brief Set a stream option
* 
* @param stream Stream to configure
* @param option Option identifier
* @param value Pointer to option value
* @param size Size of option value in bytes
* @return sio_error_t SIO_SUCCESS or error code
*/
SIO_EXPORT sio_error_t sio_stream_set_option(sio_stream_t *stream, sio_stream_option_t option, const void *value, size_t size);

/**
* @brief Get a stream option or information
* 
* @param stream Stream to query
* @param option Option identifier
* @param value Pointer to store option value
* @param size Pointer to size of value buffer (updated with actual size)
* @return sio_error_t SIO_SUCCESS or error code
*/
SIO_EXPORT sio_error_t sio_stream_get_option(sio_stream_t *stream, sio_stream_option_t option, void *value, size_t *size);

/**
* @brief Check if a stream is at the end
* 
* @param stream Stream to check
* @return int Non-zero if at end, 0 otherwise
*/
SIO_EXPORT int sio_stream_eof(const sio_stream_t *stream);

/**
* @brief Get the last error that occurred on a stream
* 
* @param stream Stream to get error from
* @return sio_error_t Last error code
*/
SIO_EXPORT sio_error_t sio_stream_get_error(const sio_stream_t *stream);

/*
 * Advanced stream operations
 */

/**
* @brief Attach a buffer to a stream for buffered I/O
* 
* @param stream Stream to attach buffer to
* @param buffer_size Size of buffer to create (0 for default)
* @param mode Buffer mode (combination of SIO_STREAM_* flags)
* @return sio_error_t SIO_SUCCESS or error code
*/
SIO_EXPORT sio_error_t sio_stream_set_buffer(sio_stream_t *stream, size_t buffer_size, int mode);

/*
 * Specialized stream functions for specific stream types
 */

/* File-specific operations */

/**
* @brief Lock a region of a file stream
* 
* @param stream File stream to lock
* @param offset Offset to start lock
* @param size Size of region to lock
* @param exclusive Non-zero for exclusive lock, 0 for shared
* @param wait_for_lock Non-zero to wait for lock, 0 to fail immediately if locked
* @return sio_error_t SIO_SUCCESS or error code
*/
SIO_EXPORT sio_error_t sio_file_lock(sio_stream_t *stream, uint64_t offset, uint64_t size, int exclusive, int wait_for_lock);

/**
* @brief Unlock a region of a file stream
* 
* @param stream File stream to unlock
* @param offset Offset to start unlock
* @param size Size of region to unlock
* @return sio_error_t SIO_SUCCESS or error code
*/
SIO_EXPORT sio_error_t sio_file_unlock(sio_stream_t *stream, uint64_t offset, uint64_t size);

/* Terminal-specific operations */

/**
* @brief Set terminal to raw mode
* 
* @param stream Terminal stream
* @return sio_error_t SIO_SUCCESS or error code
*/
SIO_EXPORT sio_error_t sio_terminal_set_raw(sio_stream_t *stream);

/**
* @brief Set terminal to canonical mode
* 
* @param stream Terminal stream
* @return sio_error_t SIO_SUCCESS or error code
*/
SIO_EXPORT sio_error_t sio_terminal_set_canonical(sio_stream_t *stream);

/**
* @brief Get terminal dimensions
* 
* @param stream Terminal stream
* @param width Pointer to store width
* @param height Pointer to store height
* @return sio_error_t SIO_SUCCESS or error code
*/
SIO_EXPORT sio_error_t sio_terminal_get_size(sio_stream_t *stream, int *width, int *height);

/**
* @brief Check if terminal supports colors
* 
* @param stream Terminal stream
* @return int Non-zero if color supported, 0 otherwise
*/
SIO_EXPORT int sio_terminal_has_color(sio_stream_t *stream);

/**
* @brief Set terminal text color
* 
* @param stream Terminal stream
* @param foreground Foreground color code
* @param background Background color code
* @return sio_error_t SIO_SUCCESS or error code
*/
SIO_EXPORT sio_error_t sio_terminal_set_color(sio_stream_t *stream, int foreground, int background);

/**
* @brief Reset terminal colors to default
* 
* @param stream Terminal stream
* @return sio_error_t SIO_SUCCESS or error code
*/
SIO_EXPORT sio_error_t sio_terminal_reset_color(sio_stream_t *stream);

/**
* @brief Clear terminal screen
* 
* @param stream Terminal stream
* @return sio_error_t SIO_SUCCESS or error code
*/
SIO_EXPORT sio_error_t sio_terminal_clear(sio_stream_t *stream);

/* Standard streams */

/**
* @brief Get standard input stream
* 
* @return sio_stream_t* Standard input stream
*/
SIO_EXPORT sio_stream_t* sio_stream_stdin(void);

/**
* @brief Get standard output stream
* 
* @return sio_stream_t* Standard output stream
*/
SIO_EXPORT sio_stream_t* sio_stream_stdout(void);

/**
* @brief Get standard error stream
* 
* @return sio_stream_t* Standard error stream
*/
SIO_EXPORT sio_stream_t* sio_stream_stderr(void);

#ifdef __cplusplus
}
#endif

#endif /* SIO_STREAM_H */