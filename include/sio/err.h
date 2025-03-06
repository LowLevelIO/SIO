/**
* @file sio/err.h
* @brief Simple I/O (SIO) - Cross-platform I/O library for high-performance systems programming
*
* A simple definition for all error codes utilized in this program and the ability to parse the error into a string
*
* @author zczxy
* @version 0.1.0
*/

#ifndef SIO_ERR_H
#define SIO_ERR_H

#ifdef __cplusplus
extern "C" {
#endif

#include <sio/platform.h>

/**
* @brief Common error codes for SIO functions
*/
typedef enum {
  /* General codes (0 to -19) */
  SIO_SUCCESS = 0,                  /**< Operation completed successfully */
  SIO_ERROR_GENERIC = -1,           /**< Generic error */
  SIO_ERROR_PARAM = -2,             /**< Invalid parameter */
  SIO_ERROR_MEM = -3,               /**< Memory allocation failure */
  SIO_ERROR_IO = -4,                /**< I/O error */
  SIO_ERROR_EOF = -5,               /**< End of file or stream */
  SIO_ERROR_NET = -6,               /**< Network error */
  SIO_ERROR_DNS = -7,               /**< DNS resolution error */
  SIO_ERROR_TIMEOUT = -8,           /**< Operation timed out */
  SIO_ERROR_BUSY = -9,              /**< Resource busy */
  SIO_ERROR_PERM = -10,             /**< Permission denied */
  SIO_ERROR_EXISTS = -11,           /**< Resource already exists */
  SIO_ERROR_NOTFOUND = -12,         /**< Resource not found */
  SIO_ERROR_BUFFER_TOO_SMALL = -13, /**< Destination buffer too small */
  SIO_ERROR_BAD_PATH = -14,         /**< Invalid path format */
  SIO_ERROR_INTERRUPTED = -15,      /**< Operation interrupted */
  SIO_ERROR_WOULDBLOCK = -16,       /**< Operation would block */
  SIO_ERROR_SYSTEM = -17,           /**< System error */
  SIO_ERROR_UNSUPPORTED = -18,      /**< Unsupported operation */
  
  /* File/IO specific errors (-20 to -39) */
  SIO_ERROR_FILE_ISDIR = -20,       /**< File is a directory */
  SIO_ERROR_FILE_NOT_DIR = -21,     /**< Path is not a directory */
  SIO_ERROR_FILE_READONLY = -22,    /**< File is read-only */
  SIO_ERROR_FILE_TOO_LARGE = -23,   /**< File too large */
  SIO_ERROR_FILE_NOSPACE = -24,     /**< No space left on device */
  SIO_ERROR_FILE_CLOSED = -25,      /**< File is already closed */
  SIO_ERROR_FILE_OPEN = -26,        /**< File already open */
  SIO_ERROR_FILE_LOCKED = -27,      /**< File is locked */
  SIO_ERROR_FILE_CORRUPT = -28,     /**< File is corrupted */
  SIO_ERROR_FILE_SEEK = -29,        /**< File seek error */
  SIO_ERROR_FILE_NAME_TOO_LONG = -30, /**< Filename too long */
  SIO_ERROR_FILE_MMAP = -31,        /**< Memory mapping error */
  SIO_ERROR_FILE_FORMAT = -32,      /**< Invalid file format */
  SIO_ERROR_FILE_LOOP = -33,        /**< Too many symbolic links */
  
  /* Network specific errors (-40 to -59) */
  SIO_ERROR_NET_CONN_REFUSED = -40, /**< Connection refused */
  SIO_ERROR_NET_CONN_ABORTED = -41, /**< Connection aborted */
  SIO_ERROR_NET_CONN_RESET = -42,   /**< Connection reset */
  SIO_ERROR_NET_HOST_UNREACHABLE = -43, /**< Host unreachable */
  SIO_ERROR_NET_HOST_DOWN = -44,    /**< Host is down */
  SIO_ERROR_NET_UNKNOWN_HOST = -45, /**< Unknown host */
  SIO_ERROR_NET_ADDR_IN_USE = -46,  /**< Address already in use */
  SIO_ERROR_NET_NOT_CONN = -47,     /**< Socket not connected */
  SIO_ERROR_NET_SHUTDOWN = -48,     /**< Socket shutdown */
  SIO_ERROR_NET_MSG_TOO_LARGE = -49, /**< Message too large */
  SIO_ERROR_NET_CONN_TIMEOUT = -50, /**< Connection timeout */
  SIO_ERROR_NET_PROTO = -51,        /**< Protocol error */
  SIO_ERROR_NET_INVALID_ADDR = -52, /**< Invalid address */
  SIO_ERROR_NET_ADDR_REQUIRED = -53, /**< Destination address required */
  SIO_ERROR_NET_INPROGRESS = -54,   /**< Operation now in progress */
  SIO_ERROR_NET_ALREADY = -55,      /**< Operation already in progress */
  SIO_ERROR_NET_NOT_SOCK = -56,     /**< Socket operation on non-socket */
  SIO_ERROR_NET_NO_PROTO_OPT = -57, /**< Protocol not available */

  /* Thread/Concurrency specific errors (-60 to -69) */
  SIO_ERROR_THREAD_CREATE = -60,    /**< Cannot create thread */
  SIO_ERROR_MUTEX_INIT = -61,       /**< Cannot initialize mutex */
  SIO_ERROR_MUTEX_LOCK = -62,       /**< Cannot lock mutex */
  SIO_ERROR_MUTEX_UNLOCK = -63,     /**< Cannot unlock mutex */
  SIO_ERROR_COND_INIT = -64,        /**< Cannot initialize condition */
  SIO_ERROR_COND_WAIT = -65,        /**< Error in condition wait */
  SIO_ERROR_COND_SIGNAL = -66,      /**< Error in condition signal */
  SIO_ERROR_THREAD_JOIN = -67,      /**< Error in thread join */
  SIO_ERROR_THREAD_DETACH = -68,    /**< Error in thread detach */
  SIO_ERROR_DEADLOCK = -69,         /**< Resource deadlock would occur */

  /* Security specific errors (-70 to -79) */
  SIO_ERROR_SEC_CERT = -70,         /**< Certificate error */
  SIO_ERROR_SEC_AUTH = -71,         /**< Authentication error */
  SIO_ERROR_SEC_VERIFICATION = -72, /**< Verification failed */
  SIO_ERROR_SEC_ENCRYPTION = -73,   /**< Encryption error */
  SIO_ERROR_SEC_DECRYPTION = -74,   /**< Decryption error */
  SIO_ERROR_SEC_BAD_KEY = -75,      /**< Bad key */
  SIO_ERROR_SEC_BAD_SIGNATURE = -76, /**< Bad signature */
  SIO_ERROR_SEC_KEY_EXPIRED = -77,  /**< Key expired */
  SIO_ERROR_SEC_REVOKED = -78,      /**< Certificate revoked */
  SIO_ERROR_SEC_UNTRUSTED = -79,    /**< Untrusted certificate */

  /* Process specific errors (-80 to -89) */
  SIO_ERROR_PROC_FORK = -80,        /**< Fork error */
  SIO_ERROR_PROC_EXEC = -81,        /**< Exec error */
  SIO_ERROR_PROC_PIPE = -82,        /**< Pipe error */
  SIO_ERROR_PROC_WAITPID = -83,     /**< Wait error */
  SIO_ERROR_PROC_KILL = -84,        /**< Kill error */
  SIO_ERROR_PROC_SIGNAL = -85,      /**< Signal error */
  SIO_ERROR_PROC_NOTFOUND = -86,    /**< Process not found */
  SIO_ERROR_PROC_PERM = -87,        /**< Process permission denied */
  SIO_ERROR_PROC_RESOURCES = -88,   /**< Insufficient resources */
  SIO_ERROR_PROC_ZOMBIE = -89,      /**< Zombie process */

  /* System specific errors (-90 to -99) */
  SIO_ERROR_SYS_LIMIT = -90,        /**< System limit reached */
  SIO_ERROR_SYS_RESOURCES = -91,    /**< System resources exhausted */
  SIO_ERROR_SYS_NOSUPPORT = -92,    /**< System does not support */
  SIO_ERROR_SYS_NOTIMPLEMENTED = -93, /**< Not implemented on this system */
  SIO_ERROR_SYS_CALL = -94,         /**< System call error */
  SIO_ERROR_SYS_OVERFLOW = -95,     /**< Value too large for system */
  SIO_ERROR_SYS_NOPROC = -96,       /**< No such process */
  SIO_ERROR_SYS_INVALID = -97,      /**< Invalid system state */
  SIO_ERROR_SYS_DEVICE = -98,       /**< Device error */
  SIO_ERROR_SYS_NOTSUP = -99        /**< Not supported */
} sio_error_t;

/**
* @brief Convert error code to string
* 
* @param err Error code
* @return const char* Error string
*/
const char *sio_strerr(sio_error_t err);

/**
* @brief Platform-specific error conversion functions
*/
#if defined(SIO_OS_WINDOWS)
  sio_error_t sio_win_error_to_sio_error(unsigned long error);
#else /* POSIX */
  sio_error_t sio_posix_error_to_sio_error(int error);
#endif

/**
* @brief Internal function to get the last error code and convert to SIO error code
* 
* @return sio_error_t Converted error code
*/
sio_error_t sio_get_last_error(void); 

/**
* @brief Debug assertion macro
* 
* @param expr Expression to check
* @param err Error code to return if expression is false
* @return Expression result if true, error code as specified if false
*/
#ifdef NDEBUG
  #define SIO_ASSERT_RET(expr, err) (expr)
#else
  #define SIO_ASSERT_RET(expr, err) \
  ((expr) ? (expr) : (sio_set_last_error(err), (typeof(expr))0))
#endif

#ifdef __cplusplus
}
#endif

#endif /* SIO_ERR_H */