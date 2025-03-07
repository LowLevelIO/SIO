/**
* @file src/aux/addr.c
* @brief Implementation of the SIO address and DNS functionality
* 
* This file contains the implementation of functions for working with
* socket addresses and DNS resolution.
* 
* @author zczxy
* @version 0.1.0
*/

#include <sio/aux/addr.h>
#include <string.h>
#include <stdio.h> // snprintf
#include <stdlib.h> // strtoul (why is this in stdlib?)

/**
* @brief Resolve a hostname to addresses
*/
sio_error_t sio_getaddrinfo(const char *node, const char *service, const sio_addrinfo_t *hints, sio_addrinfo_t **result) {
  /* Simple wrapper around system getaddrinfo */
  return getaddrinfo(node, service, hints, result);
}

/**
* @brief Free resources allocated by sio_getaddrinfo
*/
void sio_freeaddrinfo(sio_addrinfo_t *result) {
  freeaddrinfo(result);
}

/**
* @brief Get string error description for getaddrinfo errors
*/
const char *sio_gai_strerror(int errcode) {
  return gai_strerror(errcode);
}

/**
* @brief Convert string to IP address
*/
int sio_inet_pton(int af, const char *src, void *dst) {
  return inet_pton(af, src, dst);
}

/**
* @brief Convert IP address to string
*/
const char *sio_inet_ntop(int af, const void *src, char *dst, socklen_t size) {
  return inet_ntop(af, src, dst, size);
}

/**
* @brief Create a socket address from components
*/
sio_error_t sio_addr_from_parts(sio_addr_t *addr, int af, const void *ip_addr, uint16_t port) {
  if (!addr || !ip_addr) {
    return SIO_ERROR_PARAM;
  }

  memset(addr, 0, sizeof(*addr));

  if (af == SIO_AF_INET) {
    struct sockaddr_in *sin = &addr->addr.sin;
    sin->sin_family = AF_INET;
    sin->sin_port = htons(port);
    memcpy(&sin->sin_addr, ip_addr, sizeof(sin->sin_addr));
    addr->len = sizeof(*sin);
    return 0;
  } 
  else if (af == SIO_AF_INET6) {
    struct sockaddr_in6 *sin6 = &addr->addr.sin6;
    sin6->sin6_family = AF_INET6;
    sin6->sin6_port = htons(port);
    memcpy(&sin6->sin6_addr, ip_addr, sizeof(sin6->sin6_addr));
    addr->len = sizeof(*sin6);
    return 0;
  }

  return SIO_ERROR_PARAM;
}

/**
* @brief Extract components from a socket address
*/
sio_error_t sio_addr_get_parts(const sio_addr_t *addr, int *af, void *ip_addr, size_t ip_addr_size, uint16_t *port) {
  if (!addr) {
    return SIO_ERROR_PARAM;
  }

  int family = addr->addr.sa.sa_family;
  
  if (af) {
    *af = family;
  }

  if (port) {
    if (family == AF_INET) {
      *port = ntohs(addr->addr.sin.sin_port);
    } 
    else if (family == AF_INET6) {
      *port = ntohs(addr->addr.sin6.sin6_port);
    }
  }

  if (ip_addr) {
    if (family == AF_INET) {
      if (ip_addr_size < sizeof(addr->addr.sin.sin_addr)) {
        return SIO_ERROR_BUFFER_TOO_SMALL;
      }
      memcpy(ip_addr, &addr->addr.sin.sin_addr, sizeof(addr->addr.sin.sin_addr));
    } 
    else if (family == AF_INET6) {
      if (ip_addr_size < sizeof(addr->addr.sin6.sin6_addr)) {
        return SIO_ERROR_BUFFER_TOO_SMALL;
      }
      memcpy(ip_addr, &addr->addr.sin6.sin6_addr, sizeof(addr->addr.sin6.sin6_addr));
    }
  }

  return 0;
}

/**
* @brief Get string representation of address
*/
sio_error_t sio_addr_to_string(const sio_addr_t *addr, char *buf, size_t size) {
  if (!addr || !buf || size == 0) {
    return SIO_ERROR_PARAM;
  }

  int family = addr->addr.sa.sa_family;
  uint16_t port = 0;
  const void *ip_addr = NULL;

  if (family == AF_INET) {
    ip_addr = &addr->addr.sin.sin_addr;
    port = ntohs(addr->addr.sin.sin_port);
  } 
  else if (family == AF_INET6) {
    ip_addr = &addr->addr.sin6.sin6_addr;
    port = ntohs(addr->addr.sin6.sin6_port);
  } 
  else {
    return SIO_ERROR_PARAM;
  }

  // Convert IP to string directly in the buffer
  // Leave space for []:port suffix for IPv6
  char *end_ptr = buf;
  size_t remaining = size;
  
  if (family == AF_INET6) {
    if (remaining <= 1) return SIO_ERROR_BUFFER_TOO_SMALL;
    *end_ptr++ = '[';
    remaining--;
  }
  
  if (!inet_ntop(family, ip_addr, end_ptr, remaining)) {
    return SIO_ERROR_SYSTEM;
  }
  
  // Find end of IP string
  end_ptr = strchr(end_ptr, '\0');
  remaining = size - (end_ptr - buf);
  
  // Add port
  int ret;
  if (family == AF_INET) {
    ret = snprintf(end_ptr, remaining, ":%u", port);
  } else {
    ret = snprintf(end_ptr, remaining, "]:%u", port);
  }
  
  if (ret < 0 || (size_t)ret >= remaining) {
    return SIO_ERROR_BUFFER_TOO_SMALL;
  }
  
  return 0;
}

/**
* @brief Compare two socket addresses for equality
*/
int sio_addr_cmp(const sio_addr_t *a, const sio_addr_t *b, int comp) {
  if (!a || !b) {
    return 0;
  }

  int result = 1;
  
  /* Compare address families */
  if (comp & SIO_ADDR_EQ_FAMILY) {
    result = (a->addr.sa.sa_family == b->addr.sa.sa_family);
  } else if (comp & SIO_ADDR_NEQ_FAMILY) {
    result = (a->addr.sa.sa_family != b->addr.sa.sa_family);
  }
  
  /* Compare IP addresses */
  if (result && ((comp & SIO_ADDR_EQ_IP) || (comp & SIO_ADDR_NEQ_IP))) {
    int ip_equal = 0;
    
    if (a->addr.sa.sa_family == b->addr.sa.sa_family) {
      if (a->addr.sa.sa_family == AF_INET) {
        ip_equal = (a->addr.sin.sin_addr.s_addr == b->addr.sin.sin_addr.s_addr);
      } 
      else if (a->addr.sa.sa_family == AF_INET6) {
        ip_equal = (memcmp(&a->addr.sin6.sin6_addr, &b->addr.sin6.sin6_addr, sizeof(a->addr.sin6.sin6_addr)) == 0);
      }
    }

    if (comp & SIO_ADDR_EQ_IP) {
      result = ip_equal;
    } else { // SIO_ADDR_NEQ_IP
      result = !ip_equal;
    }
  }
  
  /* Compare ports */
  if (result && ((comp & SIO_ADDR_EQ_PORT) || (comp & SIO_ADDR_NEQ_PORT))) {
    uint16_t port_a;
    uint16_t port_b;

    if (a->addr.sa.sa_family == AF_INET) {
      port_a = ntohs(a->addr.sin.sin_port);
    } else if (a->addr.sa.sa_family == AF_INET6) {
      port_a = ntohs(a->addr.sin6.sin6_port);
    }

    if (b->addr.sa.sa_family == AF_INET) {
      port_b = ntohs(b->addr.sin.sin_port);
    } else if (b->addr.sa.sa_family == AF_INET6) {
      port_b = ntohs(b->addr.sin6.sin6_port);
    }
    
    if (comp & SIO_ADDR_EQ_PORT) {
      result = (port_a == port_b);
    } else { // SIO_ADDR_NEQ_PORT
      result = (port_a != port_b);
    }
  }

  return result;
}

/**
* @brief Get loopback address
*/
void sio_addr_loopback(sio_addr_t *addr, int af, uint16_t port) {
  if (!addr) {
    return;
  }

  memset(addr, 0, sizeof(*addr));
  
  if (af == AF_INET) {
    struct sockaddr_in *sin = &addr->addr.sin;
    sin->sin_family = AF_INET;
    sin->sin_port = htons(port);
    sin->sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    addr->len = sizeof(*sin);
  } 
  else if (af == AF_INET6) {
    struct sockaddr_in6 *sin6 = &addr->addr.sin6;
    sin6->sin6_family = AF_INET6;
    sin6->sin6_port = htons(port);
    sin6->sin6_addr = in6addr_loopback;
    addr->len = sizeof(*sin6);
  }
}

/**
* @brief Get any address (0.0.0.0 or ::)
*/
void sio_addr_any(sio_addr_t *addr, int af, uint16_t port) {
  if (!addr) {
    return;
  }

  memset(addr, 0, sizeof(*addr));
  
  if (af == AF_INET) {
    struct sockaddr_in *sin = &addr->addr.sin;
    sin->sin_family = AF_INET;
    sin->sin_port = htons(port);
    sin->sin_addr.s_addr = htonl(INADDR_ANY);
    addr->len = sizeof(*sin);
  } 
  else if (af == AF_INET6) {
    struct sockaddr_in6 *sin6 = &addr->addr.sin6;
    sin6->sin6_family = AF_INET6;
    sin6->sin6_port = htons(port);
    sin6->sin6_addr = in6addr_any;
    addr->len = sizeof(*sin6);
  }
}

/**
* @brief Check if address is loopback
*/
int sio_addr_is_loopback(const sio_addr_t *addr) {
  if (!addr) {
    return 0;
  }

  if (addr->addr.sa.sa_family == AF_INET) {
    uint32_t host_addr = ntohl(addr->addr.sin.sin_addr.s_addr);
    return (host_addr >> 24) == 127; /* 127.x.x.x */
  } 
  else if (addr->addr.sa.sa_family == AF_INET6) {
    /* IPv6 loopback is ::1 */
    static const struct in6_addr loopback = IN6ADDR_LOOPBACK_INIT;
    return memcmp(&addr->addr.sin6.sin6_addr, &loopback, sizeof(loopback)) == 0;
  }
  
  return 0;
}

/**
* @brief Check if address is multicast
*/
int sio_addr_is_multicast(const sio_addr_t *addr) {
  if (!addr) {
    return 0;
  }

  if (addr->addr.sa.sa_family == AF_INET) {
    uint32_t host_addr = ntohl(addr->addr.sin.sin_addr.s_addr);
    return (host_addr >> 28) == 14; /* 224.0.0.0 - 239.255.255.255 */
  } 
  else if (addr->addr.sa.sa_family == AF_INET6) {
    /* IPv6 multicast addresses start with ff00::/8 */
    return addr->addr.sin6.sin6_addr.s6_addr[0] == 0xff;
  }
  
  return 0;
}