/**
* @file sio/aux/addr.h
* @brief Simple I/O (SIO) - Cross-platform I/O library for high-performance systems programming
*
* A simple way to interact with socket address and the dns service provided by the operating system.
* Allows resolving of domain names and provides utilities for IP address manipulation.
* 
* @author zczxy
* @version 0.1.0
*/

#ifndef SIO_AUX_DNS_H
#define SIO_AUX_DNS_H

#ifdef __cplusplus
extern "C" {
#endif

#include <sio/platform.h>
#include <sio/err.h>

/* Platform-specific includes */
#if defined(SIO_OS_WINDOWS)
  #include <winsock2.h>
  #include <ws2tcpip.h>
  #pragma comment(lib, "ws2_32.lib")
#elif defined(SIO_OS_POSIX)
  #include <sys/types.h>
  #include <sys/socket.h>
  #include <netdb.h>
  #include <arpa/inet.h>
  #include <netinet/in.h>
  #include <unistd.h>
#else
  #error "addr.h - Unsupported operating system"
#endif

/**
* @name IP Protocol families
* @{
*/
#define SIO_AF_INET     AF_INET     /**< IPv4 address family */
#define SIO_AF_INET6    AF_INET6    /**< IPv6 address family */
#define SIO_AF_UNSPEC   AF_UNSPEC   /**< Unspecified address family */
/** @} */

/**
* @name Socket types
* @{
*/
#define SIO_SOCK_STREAM   SOCK_STREAM   /**< Stream socket */
#define SIO_SOCK_DGRAM    SOCK_DGRAM    /**< Datagram socket */
/** @} */

/**
* @name IP Protocol constants
* @{
*/
#define SIO_IPPROTO_TCP   IPPROTO_TCP   /**< TCP protocol */
#define SIO_IPPROTO_UDP   IPPROTO_UDP   /**< UDP protocol */
/** @} */

/**
* @name Address info flags
* @{
*/
#define SIO_AI_PASSIVE     AI_PASSIVE     /**< Socket address for bind */
#define SIO_AI_CANONNAME   AI_CANONNAME   /**< Get canonical name */
#define SIO_AI_NUMERICHOST AI_NUMERICHOST /**< Prevent host name resolution */
#define SIO_AI_NUMERICSERV AI_NUMERICSERV /**< Prevent service name resolution */
#define SIO_AI_V4MAPPED    AI_V4MAPPED    /**< Map IPv4 to IPv6 addresses */
#define SIO_AI_ALL         AI_ALL         /**< Return all matching addresses */
#define SIO_AI_ADDRCONFIG  AI_ADDRCONFIG  /**< Return addresses for configured interfaces */
/** @} */

/**
* @brief Uniform socket address structure
* 
* This structure provides a unified interface for handling both IPv4 and IPv6 socket addresses.
*/
typedef struct sio_addr {
  union {
    struct sockaddr sa;             /**< Generic socket address */
    struct sockaddr_in sin;         /**< IPv4 socket address */
    struct sockaddr_in6 sin6;       /**< IPv6 socket address */
    struct sockaddr_storage ss;     /**< Generic storage for any type of socket address */
  } addr;
  socklen_t len;                    /**< Length of the socket address */
} sio_addr_t;

/**
* @brief DNS resolution hints - direct mapping to addrinfo
*/
typedef struct addrinfo sio_addrinfo_t;

/**
* @brief Resolve a hostname to addresses
* 
* @param node The hostname to resolve or a numeric address string
* @param service The service name or port number as string
* @param hints Optional hints to modify the resolution behavior
* @param result Pointer to a pointer that will receive the result
* @return sio_error_t Error code (0 on success)
*/
SIO_EXPORT sio_error_t sio_getaddrinfo(const char *node, const char *service, const sio_addrinfo_t *hints, sio_addrinfo_t **result);

/**
* @brief Free resources allocated by sio_getaddrinfo
* 
* @param result The linked list of results to free
*/
SIO_EXPORT void sio_freeaddrinfo(sio_addrinfo_t *result);

/**
* @brief Get string error description for getaddrinfo errors
* 
* @param errcode The error code returned by sio_getaddrinfo
* @return const char* String describing the error
*/
SIO_EXPORT const char *sio_gai_strerror(int errcode);

/**
* @brief Convert string to IP address
* 
* @param af Address family (SIO_AF_INET or SIO_AF_INET6)
* @param src String containing the IP address to convert
* @param dst Buffer to store the binary representation of the address
* @return int 1 on success, 0 on failure, -1 on error
*/
SIO_EXPORT int sio_inet_pton(int af, const char *src, void *dst);

/**
* @brief Convert IP address to string
* 
* @param af Address family (SIO_AF_INET or SIO_AF_INET6)
* @param src Buffer containing the binary representation of the address
* @param dst Buffer to store the string representation of the address
* @param size Size of the destination buffer
* @return const char* Pointer to the destination buffer on success, NULL on failure
*/
SIO_EXPORT const char *sio_inet_ntop(int af, const void *src, char *dst, socklen_t size);

/**
* @brief Create a socket address from components
* 
* @param addr Pointer to the socket address structure to initialize
* @param af Address family (SIO_AF_INET or SIO_AF_INET6)
* @param ip_addr Binary representation of the IP address
* @param port Port number in host byte order
* @return sio_error_t Error code (0 on success)
*/
SIO_EXPORT sio_error_t sio_addr_from_parts(sio_addr_t *addr, int af, const void *ip_addr, uint16_t port);

/**
* @brief Extract components from a socket address
* 
* @param addr Pointer to the socket address to extract components from
* @param af Pointer to store the address family
* @param ip_addr Buffer to store the binary representation of the IP address
* @param ip_addr_size Size of the ip_addr buffer
* @param port Pointer to store the port number in host byte order
* @return sio_error_t Error code (0 on success)
*/
SIO_EXPORT sio_error_t sio_addr_get_parts(const sio_addr_t *addr, int *af, void *ip_addr, size_t ip_addr_size, uint16_t *port);

/**
* @brief Get string representation of address
* 
* @param addr Pointer to the socket address
* @param buf Buffer to store the string representation
* @param size Size of the buffer
* @return sio_error_t Error code (0 on success)
*/
SIO_EXPORT sio_error_t sio_addr_to_string(const sio_addr_t *addr, char *buf, size_t size);

/**
* @brief Create address from null-delimited string (host:0port or [host]:port for IPv6)
* 
* @param addr Pointer to the socket address to initialize
* @param str String containing the address in "host:port" format
* @return sio_error_t Error code (0 on success)
*/
SIO_EXPORT sio_error_t sio_addr_from_string(sio_addr_t *addr, const char *str);

/**
* @name Address comparison flags
* @{
*/
#define SIO_ADDR_EQ_FAMILY  (1 << 0)  /**< Equal family */
#define SIO_ADDR_NEQ_FAMILY (1 << 0)  /**< Not equal family */
#define SIO_ADDR_EQ_IP      (1 << 1)  /**< Equal IP address */
#define SIO_ADDR_NEQ_IP     (1 << 1)  /**< Not equal IP address */
#define SIO_ADDR_EQ_PORT    (1 << 2)  /**< Equal port */
#define SIO_ADDR_NEQ_PORT   (1 << 2)  /**< Not equal port */
/** @} */

/**
* @brief Compare two socket addresses for equality
* 
* @param a First socket address
* @param b Second socket address
* @param comp Comparison flags (combination of SIO_ADDR_* constants)
* @return int Non-zero if the comparison is true, 0 otherwise
*/
SIO_EXPORT int sio_addr_cmp(const sio_addr_t *a, const sio_addr_t *b, int comp);

/**
* @brief Get loopback address
* 
* @param addr Pointer to the socket address to initialize
* @param af Address family (SIO_AF_INET or SIO_AF_INET6)
* @param port Port number in host byte order
*/
SIO_EXPORT void sio_addr_loopback(sio_addr_t *addr, int af, uint16_t port);

/**
* @brief Get any address (0.0.0.0 or ::)
* 
* @param addr Pointer to the socket address to initialize
* @param af Address family (SIO_AF_INET or SIO_AF_INET6)
* @param port Port number in host byte order
*/
SIO_EXPORT void sio_addr_any(sio_addr_t *addr, int af, uint16_t port);

/**
* @brief Check if address is loopback
* 
* @param addr Pointer to the socket address to check
* @return int Non-zero if the address is a loopback address, 0 otherwise
*/
SIO_EXPORT int sio_addr_is_loopback(const sio_addr_t *addr);

/**
* @brief Check if address is multicast
* 
* @param addr Pointer to the socket address to check
* @return int Non-zero if the address is a multicast address, 0 otherwise
*/
SIO_EXPORT int sio_addr_is_multicast(const sio_addr_t *addr);

#ifdef __cplusplus
}
#endif

#endif /* SIO_AUX_DNS_H */