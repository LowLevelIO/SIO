/**
* @file sio/err.c
* @brief Implementation of error handling functions
*
* @author zczxy
* @version 0.1.0
*/

#include "sio/err.h"

#if defined(SIO_OS_WINDOWS)
  #include <errhandlingapi.h>
#else  
  #include <errno.h>
#endif

const char *sio_strerr(sio_error_t err) {
  switch (err) {
    /* General codes */
    case SIO_SUCCESS:
      return "Success";
    case SIO_ERROR_GENERIC:
      return "Generic error";
    case SIO_ERROR_PARAM:
      return "Invalid parameter";
    case SIO_ERROR_MEM:
      return "Memory allocation failure";
    case SIO_ERROR_IO:
      return "I/O error";
    case SIO_ERROR_EOF:
      return "End of file or stream";
    case SIO_ERROR_NET:
      return "Network error";
    case SIO_ERROR_DNS:
      return "DNS resolution error";
    case SIO_ERROR_TIMEOUT:
      return "Operation timed out";
    case SIO_ERROR_BUSY:
      return "Resource busy";
    case SIO_ERROR_PERM:
      return "Permission denied";
    case SIO_ERROR_EXISTS:
      return "Resource already exists";
    case SIO_ERROR_NOTFOUND:
      return "Resource not found";
    case SIO_ERROR_BUFFER_TOO_SMALL:
      return "Destination buffer too small";
    case SIO_ERROR_BAD_PATH:
      return "Invalid path format";
    case SIO_ERROR_INTERRUPTED:
      return "Operation interrupted";
    case SIO_ERROR_WOULDBLOCK:
      return "Operation would block";
    case SIO_ERROR_SYSTEM:
      return "System error";
    case SIO_ERROR_UNSUPPORTED:
      return "Unsupported operation";
      
    /* File/IO specific errors */
    case SIO_ERROR_FILE_ISDIR:
      return "File is a directory";
    case SIO_ERROR_FILE_NOT_DIR:
      return "Path is not a directory";
    case SIO_ERROR_FILE_READONLY:
      return "File is read-only";
    case SIO_ERROR_FILE_TOO_LARGE:
      return "File too large";
    case SIO_ERROR_FILE_NOSPACE:
      return "No space left on device";
    case SIO_ERROR_FILE_CLOSED:
      return "File is already closed";
    case SIO_ERROR_FILE_OPEN:
      return "File already open";
    case SIO_ERROR_FILE_LOCKED:
      return "File is locked";
    case SIO_ERROR_FILE_CORRUPT:
      return "File is corrupted";
    case SIO_ERROR_FILE_SEEK:
      return "File seek error";
    case SIO_ERROR_FILE_NAME_TOO_LONG:
      return "Filename too long";
    case SIO_ERROR_FILE_MMAP:
      return "Memory mapping error";
    case SIO_ERROR_FILE_FORMAT:
      return "Invalid file format";
    case SIO_ERROR_FILE_LOOP:
      return "Too many symbolic links";
      
    /* Network specific errors */
    case SIO_ERROR_NET_CONN_REFUSED:
      return "Connection refused";
    case SIO_ERROR_NET_CONN_ABORTED:
      return "Connection aborted";
    case SIO_ERROR_NET_CONN_RESET:
      return "Connection reset";
    case SIO_ERROR_NET_HOST_UNREACHABLE:
      return "Host unreachable";
    case SIO_ERROR_NET_HOST_DOWN:
      return "Host is down";
    case SIO_ERROR_NET_UNKNOWN_HOST:
      return "Unknown host";
    case SIO_ERROR_NET_ADDR_IN_USE:
      return "Address already in use";
    case SIO_ERROR_NET_NOT_CONN:
      return "Socket not connected";
    case SIO_ERROR_NET_SHUTDOWN:
      return "Socket shutdown";
    case SIO_ERROR_NET_MSG_TOO_LARGE:
      return "Message too large";
    case SIO_ERROR_NET_CONN_TIMEOUT:
      return "Connection timeout";
    case SIO_ERROR_NET_PROTO:
      return "Protocol error";
    case SIO_ERROR_NET_INVALID_ADDR:
      return "Invalid address";
    case SIO_ERROR_NET_ADDR_REQUIRED:
      return "Destination address required";
    case SIO_ERROR_NET_INPROGRESS:
      return "Operation now in progress";
    case SIO_ERROR_NET_ALREADY:
      return "Operation already in progress";
    case SIO_ERROR_NET_NOT_SOCK:
      return "Socket operation on non-socket";
    case SIO_ERROR_NET_NO_PROTO_OPT:
      return "Protocol not available";
      
    /* Thread/Concurrency specific errors */
    case SIO_ERROR_THREAD_CREATE:
      return "Cannot create thread";
    case SIO_ERROR_MUTEX_INIT:
      return "Cannot initialize mutex";
    case SIO_ERROR_MUTEX_LOCK:
      return "Cannot lock mutex";
    case SIO_ERROR_MUTEX_UNLOCK:
      return "Cannot unlock mutex";
    case SIO_ERROR_COND_INIT:
      return "Cannot initialize condition";
    case SIO_ERROR_COND_WAIT:
      return "Error in condition wait";
    case SIO_ERROR_COND_SIGNAL:
      return "Error in condition signal";
    case SIO_ERROR_THREAD_JOIN:
      return "Error in thread join";
    case SIO_ERROR_THREAD_DETACH:
      return "Error in thread detach";
    case SIO_ERROR_DEADLOCK:
      return "Resource deadlock would occur";
      
    /* Security specific errors */
    case SIO_ERROR_SEC_CERT:
      return "Certificate error";
    case SIO_ERROR_SEC_AUTH:
      return "Authentication error";
    case SIO_ERROR_SEC_VERIFICATION:
      return "Verification failed";
    case SIO_ERROR_SEC_ENCRYPTION:
      return "Encryption error";
    case SIO_ERROR_SEC_DECRYPTION:
      return "Decryption error";
    case SIO_ERROR_SEC_BAD_KEY:
      return "Bad key";
    case SIO_ERROR_SEC_BAD_SIGNATURE:
      return "Bad signature";
    case SIO_ERROR_SEC_KEY_EXPIRED:
      return "Key expired";
    case SIO_ERROR_SEC_REVOKED:
      return "Certificate revoked";
    case SIO_ERROR_SEC_UNTRUSTED:
      return "Untrusted certificate";
      
    /* Process specific errors */
    case SIO_ERROR_PROC_FORK:
      return "Fork error";
    case SIO_ERROR_PROC_EXEC:
      return "Exec error";
    case SIO_ERROR_PROC_PIPE:
      return "Pipe error";
    case SIO_ERROR_PROC_WAITPID:
      return "Wait error";
    case SIO_ERROR_PROC_KILL:
      return "Kill error";
    case SIO_ERROR_PROC_SIGNAL:
      return "Signal error";
    case SIO_ERROR_PROC_NOTFOUND:
      return "Process not found";
    case SIO_ERROR_PROC_PERM:
      return "Process permission denied";
    case SIO_ERROR_PROC_RESOURCES:
      return "Insufficient resources";
    case SIO_ERROR_PROC_ZOMBIE:
      return "Zombie process";
      
    /* System specific errors */
    case SIO_ERROR_SYS_LIMIT:
      return "System limit reached";
    case SIO_ERROR_SYS_RESOURCES:
      return "System resources exhausted";
    case SIO_ERROR_SYS_NOSUPPORT:
      return "System does not support";
    case SIO_ERROR_SYS_NOTIMPLEMENTED:
      return "Not implemented on this system";
    case SIO_ERROR_SYS_CALL:
      return "System call error";
    case SIO_ERROR_SYS_OVERFLOW:
      return "Value too large for system";
    case SIO_ERROR_SYS_NOPROC:
      return "No such process";
    case SIO_ERROR_SYS_INVALID:
      return "Invalid system state";
    case SIO_ERROR_SYS_DEVICE:
      return "Device error";
    case SIO_ERROR_SYS_NOTSUP:
      return "Not supported";
    default:
      return "Unknown error";
  }
}


#if defined(SIO_OS_WINDOWS)
/**
* @brief Convert Windows error code to SIO error
* 
* @param error Windows error code
* @return sio_error_t SIO error code
*/
sio_error_t sio_win_error_to_sio_error(unsigned long error) {
  switch (error) {
    case ERROR_SUCCESS:
      return SIO_SUCCESS;
    case ERROR_INVALID_FUNCTION:
      return SIO_ERROR_UNSUPPORTED;
    case ERROR_FILE_NOT_FOUND:
    case ERROR_PATH_NOT_FOUND:
      return SIO_ERROR_NOTFOUND;
    case ERROR_TOO_MANY_OPEN_FILES:
      return SIO_ERROR_SYS_LIMIT;
    case ERROR_ACCESS_DENIED:
      return SIO_ERROR_PERM;
    case ERROR_INVALID_HANDLE:
      return SIO_ERROR_PARAM;
    case ERROR_NOT_ENOUGH_MEMORY:
    case ERROR_OUTOFMEMORY:
      return SIO_ERROR_MEM;
    case ERROR_INVALID_DRIVE:
      return SIO_ERROR_PARAM;
    case ERROR_CURRENT_DIRECTORY:
      return SIO_ERROR_PERM;
    case ERROR_NOT_SAME_DEVICE:
      return SIO_ERROR_PARAM;
    case ERROR_NO_MORE_FILES:
      return SIO_ERROR_EOF;
    case ERROR_WRITE_PROTECT:
      return SIO_ERROR_FILE_READONLY;
    case ERROR_BAD_UNIT:
      return SIO_ERROR_SYS_DEVICE;
    case ERROR_NOT_READY:
      return SIO_ERROR_SYS_DEVICE;
    case ERROR_CRC:
    case ERROR_BAD_LENGTH:
    case ERROR_SEEK:
    case ERROR_NOT_DOS_DISK:
    case ERROR_SECTOR_NOT_FOUND:
    case ERROR_GEN_FAILURE:
      return SIO_ERROR_IO;
    case ERROR_SHARING_VIOLATION:
      return SIO_ERROR_FILE_LOCKED;
    case ERROR_LOCK_VIOLATION:
      return SIO_ERROR_FILE_LOCKED;
    case ERROR_WRONG_DISK:
      return SIO_ERROR_PARAM;
    case ERROR_HANDLE_EOF:
      return SIO_ERROR_EOF;
    case ERROR_HANDLE_DISK_FULL:
      return SIO_ERROR_FILE_NOSPACE;
    case ERROR_NOT_SUPPORTED:
      return SIO_ERROR_UNSUPPORTED;
    case ERROR_REM_NOT_LIST:
      return SIO_ERROR_NET;
    case ERROR_DUP_NAME:
      return SIO_ERROR_EXISTS;
    case ERROR_BAD_NETPATH:
    case ERROR_NETWORK_BUSY:
    case ERROR_DEV_NOT_EXIST:
    case ERROR_BAD_NET_RESP:
    case ERROR_UNEXP_NET_ERR:
    case ERROR_BAD_NET_NAME:
    case ERROR_BAD_NETPATH:
      return SIO_ERROR_NET;
    case ERROR_FILE_EXISTS:
    case ERROR_ALREADY_EXISTS:
      return SIO_ERROR_EXISTS;
    case ERROR_CANNOT_MAKE:
      return SIO_ERROR_PERM;
    case ERROR_INVALID_PARAMETER:
      return SIO_ERROR_PARAM;
    case ERROR_NET_WRITE_FAULT:
      return SIO_ERROR_NET;
    case ERROR_IO_PENDING:
      return SIO_ERROR_WOULDBLOCK;
    case ERROR_NOACCESS:
      return SIO_ERROR_PERM;
    case ERROR_DISK_FULL:
      return SIO_ERROR_FILE_NOSPACE;
    case ERROR_INVALID_ADDRESS:
      return SIO_ERROR_PARAM;
    case ERROR_TIMEOUT:
      return SIO_ERROR_TIMEOUT;
    case ERROR_BUSY:
      return SIO_ERROR_BUSY;
    case ERROR_NOT_ENOUGH_QUOTA:
      return SIO_ERROR_SYS_RESOURCES;
    case ERROR_DIRECTORY:
      return SIO_ERROR_FILE_ISDIR;
    case ERROR_OPERATION_ABORTED:
      return SIO_ERROR_INTERRUPTED;
    case ERROR_BUFFER_OVERFLOW:
      return SIO_ERROR_BUFFER_TOO_SMALL;
    case ERROR_PATH_BUSY:
      return SIO_ERROR_BUSY;
    case ERROR_BAD_PATHNAME:
      return SIO_ERROR_BAD_PATH;
    case WSAEACCES:
      return SIO_ERROR_PERM;
    case WSAEADDRINUSE:
      return SIO_ERROR_NET_ADDR_IN_USE;
    case WSAEADDRNOTAVAIL:
      return SIO_ERROR_NET_INVALID_ADDR;
    case WSAEAFNOSUPPORT:
      return SIO_ERROR_NET;
    case WSAEALREADY:
      return SIO_ERROR_NET_ALREADY;
    case WSAECONNABORTED:
      return SIO_ERROR_NET_CONN_ABORTED;
    case WSAECONNREFUSED:
      return SIO_ERROR_NET_CONN_REFUSED;
    case WSAECONNRESET:
      return SIO_ERROR_NET_CONN_RESET;
    case WSAEDESTADDRREQ:
      return SIO_ERROR_NET_ADDR_REQUIRED;
    case WSAEHOSTDOWN:
      return SIO_ERROR_NET_HOST_DOWN;
    case WSAEHOSTUNREACH:
      return SIO_ERROR_NET_HOST_UNREACHABLE;
    case WSAEINPROGRESS:
      return SIO_ERROR_NET_INPROGRESS;
    case WSAEINTR:
      return SIO_ERROR_INTERRUPTED;
    case WSAEINVAL:
      return SIO_ERROR_PARAM;
    case WSAEISCONN:
      return SIO_ERROR_NET;
    case WSAEMSGSIZE:
      return SIO_ERROR_NET_MSG_TOO_LARGE;
    case WSAENETDOWN:
    case WSAENETRESET:
    case WSAENETUNREACH:
      return SIO_ERROR_NET;
    case WSAENOBUFS:
      return SIO_ERROR_SYS_RESOURCES;
    case WSAENOPROTOOPT:
      return SIO_ERROR_NET_NO_PROTO_OPT;
    case WSAENOTCONN:
      return SIO_ERROR_NET_NOT_CONN;
    case WSAENOTSOCK:
      return SIO_ERROR_NET_NOT_SOCK;
    case WSAEOPNOTSUPP:
      return SIO_ERROR_UNSUPPORTED;
    case WSAEPROTONOSUPPORT:
    case WSAEPROTOTYPE:
      return SIO_ERROR_NET_PROTO;
    case WSAESHUTDOWN:
      return SIO_ERROR_NET_SHUTDOWN;
    case WSAETIMEDOUT:
      return SIO_ERROR_NET_CONN_TIMEOUT;
    case WSAEWOULDBLOCK:
      return SIO_ERROR_WOULDBLOCK;
    case WSANOTINITIALISED:
      return SIO_ERROR_NET;
    case WSASYSNOTREADY:
      return SIO_ERROR_SYS_RESOURCES;
    case WSAVERNOTSUPPORTED:
      return SIO_ERROR_UNSUPPORTED;
    default:
      return SIO_ERROR_GENERIC;
  }
}
#else /* POSIX */
/**
* @brief Convert POSIX error code to SIO error
* 
* @param error POSIX error code
* @return sio_error_t SIO error code
*/
sio_error_t sio_posix_error_to_sio_error(int error) {
  switch (error) {
    case 0:
      return SIO_SUCCESS;
    case EPERM:
      return SIO_ERROR_PERM;
    case ENOENT:
      return SIO_ERROR_NOTFOUND;
    case ESRCH:
      return SIO_ERROR_PROC_NOTFOUND;
    case EINTR:
      return SIO_ERROR_INTERRUPTED;
    case EIO:
      return SIO_ERROR_IO;
    case ENXIO:
      return SIO_ERROR_SYS_DEVICE;
    case E2BIG:
      return SIO_ERROR_PARAM;
    case ENOEXEC:
      return SIO_ERROR_PROC_EXEC;
    case EBADF:
      return SIO_ERROR_PARAM;
    case ECHILD:
      return SIO_ERROR_PROC_WAITPID;
#if EAGAIN != EWOULDBLOCK
    case EAGAIN:
      return SIO_ERROR_WOULDBLOCK;
#endif
    case EWOULDBLOCK:
      return SIO_ERROR_WOULDBLOCK;
    case ENOMEM:
      return SIO_ERROR_MEM;
    case EACCES:
      return SIO_ERROR_PERM;
    case EFAULT:
      return SIO_ERROR_PARAM;
    case EBUSY:
      return SIO_ERROR_BUSY;
    case EEXIST:
      return SIO_ERROR_EXISTS;
    case EXDEV:
      return SIO_ERROR_PARAM;
    case ENODEV:
      return SIO_ERROR_SYS_DEVICE;
    case ENOTDIR:
      return SIO_ERROR_FILE_NOT_DIR;
    case EISDIR:
      return SIO_ERROR_FILE_ISDIR;
    case EINVAL:
      return SIO_ERROR_PARAM;
    case ENFILE:
    case EMFILE:
      return SIO_ERROR_SYS_LIMIT;
    case ENOTTY:
      return SIO_ERROR_PARAM;
    case ETXTBSY:
      return SIO_ERROR_BUSY;
    case EFBIG:
      return SIO_ERROR_FILE_TOO_LARGE;
    case ENOSPC:
      return SIO_ERROR_FILE_NOSPACE;
    case ESPIPE:
      return SIO_ERROR_FILE_SEEK;
    case EROFS:
      return SIO_ERROR_FILE_READONLY;
    case EMLINK:
      return SIO_ERROR_SYS_LIMIT;
    case EPIPE:
      return SIO_ERROR_IO;
    case EDOM:
    case ERANGE:
      return SIO_ERROR_PARAM;
    case EDEADLK:
      return SIO_ERROR_DEADLOCK;
    case ENAMETOOLONG:
      return SIO_ERROR_FILE_NAME_TOO_LONG;
    case ENOTEMPTY:
      return SIO_ERROR_EXISTS;
    case ELOOP:
      return SIO_ERROR_FILE_LOOP;
    case EOVERFLOW:
      return SIO_ERROR_SYS_OVERFLOW;
    case ENOSYS:
      return SIO_ERROR_SYS_NOTIMPLEMENTED;
    case ETIMEDOUT:
      return SIO_ERROR_TIMEOUT;
    case ECANCELED:
      return SIO_ERROR_INTERRUPTED;
    case EOWNERDEAD:
    case ENOTRECOVERABLE:
      return SIO_ERROR_SYS_INVALID;
#ifdef ENOTSUP
    case ENOTSUP:
      return SIO_ERROR_UNSUPPORTED;
#endif
#ifdef EBADMSG
    case EBADMSG:
      return SIO_ERROR_NET_PROTO;
#endif
#ifdef EPROTO
    case EPROTO:
      return SIO_ERROR_NET_PROTO;
#endif
#ifdef EADDRNOTAVAIL
    case EADDRNOTAVAIL:
      return SIO_ERROR_NET_INVALID_ADDR;
#endif
#ifdef EADDRINUSE
    case EADDRINUSE:
      return SIO_ERROR_NET_ADDR_IN_USE;
#endif
#ifdef ECONNREFUSED
    case ECONNREFUSED:
      return SIO_ERROR_NET_CONN_REFUSED;
#endif
#ifdef ECONNRESET
    case ECONNRESET:
      return SIO_ERROR_NET_CONN_RESET;
#endif
#ifdef ECONNABORTED
    case ECONNABORTED:
      return SIO_ERROR_NET_CONN_ABORTED;
#endif
#ifdef EISCONN
    case EISCONN:
      return SIO_ERROR_NET;
#endif
#ifdef ENOTCONN
    case ENOTCONN:
      return SIO_ERROR_NET_NOT_CONN;
#endif
#ifdef EHOSTUNREACH
    case EHOSTUNREACH:
      return SIO_ERROR_NET_HOST_UNREACHABLE;
#endif
#ifdef EHOSTDOWN
    case EHOSTDOWN:
      return SIO_ERROR_NET_HOST_DOWN;
#endif
#ifdef EMSGSIZE
    case EMSGSIZE:
      return SIO_ERROR_NET_MSG_TOO_LARGE;
#endif
#ifdef ENOPROTOOPT
    case ENOPROTOOPT:
      return SIO_ERROR_NET_NO_PROTO_OPT;
#endif
#ifdef EDESTADDRREQ
    case EDESTADDRREQ:
      return SIO_ERROR_NET_ADDR_REQUIRED;
#endif
#ifdef EALREADY
    case EALREADY:
      return SIO_ERROR_NET_ALREADY;
#endif
#ifdef EINPROGRESS
    case EINPROGRESS:
      return SIO_ERROR_NET_INPROGRESS;
#endif
    default:
      return SIO_ERROR_GENERIC;
  }
}
#endif

/**
* @brief Internal function to get the last error code and convert to SIO error code
* 
* @return sio_error_t Converted error code
*/
sio_error_t sio_get_last_error(void) {
#if defined(SIO_OS_WINDOWS)
  return sio_win_error_to_sio_error(GetLastError());
#else
  return sio_posix_error_to_sio_error(errno);
#endif
}