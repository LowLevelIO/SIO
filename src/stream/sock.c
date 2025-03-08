/**
* @file src/stream/sock.c
* @brief Implementation of socket stream functionality
*
* This file provides the implementation of socket I/O operations for the SIO library.
* It handles socket creation, connection, sending, receiving, and other operations
* across different platforms (Windows and POSIX).
*
* @author zczxy
* @version 0.1.0
*/

#include <sio/stream.h>
#include <sio/aux/addr.h>
#include <sio/err.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>

#if defined(SIO_OS_WINDOWS)
  #include <winsock2.h>
  #include <ws2tcpip.h>
  #pragma comment(lib, "ws2_32.lib")
#else
  #include <sys/types.h>
  #include <sys/socket.h>
  #include <netinet/in.h>
  #include <netinet/tcp.h>
  #include <unistd.h>
  #include <fcntl.h>
  #include <errno.h>
#endif

/* Forward declarations of socket stream operations */
static sio_error_t socket_close(sio_stream_t *stream);
static sio_error_t socket_read(sio_stream_t *stream, void *buffer, size_t size, size_t *bytes_read, int flags);
static sio_error_t socket_write(sio_stream_t *stream, const void *buffer, size_t size, size_t *bytes_written, int flags);
static sio_error_t socket_readv(sio_stream_t *stream, sio_iovec_t *iov, size_t iovcnt, size_t *bytes_read, int flags);
static sio_error_t socket_writev(sio_stream_t *stream, const sio_iovec_t *iov, size_t iovcnt, size_t *bytes_written, int flags);
static sio_error_t socket_get_option(sio_stream_t *stream, sio_stream_option_t option, void *value, size_t *size);
static sio_error_t socket_set_option(sio_stream_t *stream, sio_stream_option_t option, const void *value, size_t size);

/* Socket stream operations vtable */
static const sio_stream_ops_t socket_ops = {
  .close = socket_close,
  .read = socket_read,
  .write = socket_write,
  .readv = socket_readv,
  .writev = socket_writev,
  .flush = NULL, /* No buffer flush needed for sockets */
  .get_option = socket_get_option,
  .set_option = socket_set_option,
  .seek = NULL, /* Sockets are not seekable */
  .tell = NULL, /* Sockets don't have a position */
  .truncate = NULL, /* Sockets can't be truncated */
  .get_size = NULL /* Sockets don't have a size */
};

/* Pseudo socket operations vtable (for UDP client sockets) */
static const sio_stream_ops_t pseudo_socket_ops = {
  .close = socket_close,
  .read = socket_read,
  .write = socket_write,
  .readv = socket_readv,
  .writev = socket_writev,
  .flush = NULL, /* No buffer flush needed for sockets */
  .get_option = socket_get_option,
  .set_option = socket_set_option,
  .seek = NULL, /* Sockets are not seekable */
  .tell = NULL, /* Sockets don't have a position */
  .truncate = NULL, /* Sockets can't be truncated */
  .get_size = NULL /* Sockets don't have a size */
};

/**
* @brief Create a socket
* 
* @param domain Socket domain (AF_INET, AF_INET6, etc.)
* @param type Socket type (SOCK_STREAM, SOCK_DGRAM, etc.)
* @param protocol Socket protocol (0, IPPROTO_TCP, etc.)
* @param non_blocking Whether to create a non-blocking socket
* @return Platform-specific socket handle, or invalid value on error
*/
#if defined(SIO_OS_WINDOWS)
static SOCKET create_socket(int domain, int type, int protocol, int non_blocking) {
  /* Create the socket */
  SOCKET sock = socket(domain, type, protocol);
  if (sock == INVALID_SOCKET) {
    return INVALID_SOCKET;
  }
  
  /* Set to non-blocking mode if requested */
  if (non_blocking) {
    u_long mode = 1; /* 1 = non-blocking */
    if (ioctlsocket(sock, FIONBIO, &mode) == SOCKET_ERROR) {
      closesocket(sock);
      return INVALID_SOCKET;
    }
  }
  
  return sock;
}
#else
static int create_socket(int domain, int type, int protocol, int non_blocking) {
  /* Set non-blocking flag directly in socket() call if supported */
#ifdef SOCK_NONBLOCK
  if (non_blocking) {
    type |= SOCK_NONBLOCK;
  }
#endif
  
  /* Create the socket */
  int sock = socket(domain, type, protocol);
  if (sock < 0) {
    return -1;
  }
  
  /* Set to non-blocking mode if requested and not set during creation */
#ifndef SOCK_NONBLOCK
  if (non_blocking) {
    int flags = fcntl(sock, F_GETFL, 0);
    if (flags == -1) {
      close(sock);
      return -1;
    }
    
    if (fcntl(sock, F_SETFL, flags | O_NONBLOCK) == -1) {
      close(sock);
      return -1;
    }
  }
#endif
  
  /* Set close-on-exec flag if supported */
#ifdef SOCK_CLOEXEC
  /* Already set in socket() call */
#elif defined(FD_CLOEXEC)
  int flags = fcntl(sock, F_GETFD, 0);
  if (flags != -1) {
    fcntl(sock, F_SETFD, flags | FD_CLOEXEC);
  }
#endif
  
  return sock;
}
#endif

/**
* @brief Create a socket stream
*/
sio_error_t sio_stream_open_socket(sio_stream_t *stream, sio_addr_t *addr, sio_stream_flags_t opt) {
  if (!stream || !addr) {
    return SIO_ERROR_PARAM;
  }
  
  /* Initialize the stream structure */
  memset(stream, 0, sizeof(sio_stream_t));
  stream->flags = opt;
  
  int domain = addr->addr.sa.sa_family;
  int type = (opt & SIO_STREAM_TCP) ?  SOCK_STREAM : SOCK_DGRAM;
  int protocol = (type == SOCK_STREAM) ? IPPROTO_TCP : IPPROTO_UDP;
  int non_blocking = (opt & SIO_STREAM_NONBLOCK) ? 1 : 0;
  
  /* If this is a UDP client socket without binding, create a pseudo socket */
  if (type == SOCK_DGRAM && !(opt & SIO_STREAM_SERVER)) {
    stream->type = SIO_STREAM_PSEUDO_SOCKET;
    stream->ops = &pseudo_socket_ops;
    memcpy(&stream->data.pseudo_socket.addr, addr, sizeof(sio_addr_t));
    
    /* Create an actual socket */
#if defined(SIO_OS_WINDOWS)
    SOCKET sock = create_socket(domain, type, protocol, non_blocking);
    if (sock == INVALID_SOCKET) {
      return sio_get_last_error();
    }
    stream->data.socket.socket = sock;
#else
    int sock = create_socket(domain, type, protocol, non_blocking);
    if (sock < 0) {
      return sio_get_last_error();
    }
    stream->data.socket.fd = sock;
#endif
    
    return SIO_SUCCESS;
  }
  
  /* Regular socket */
  stream->type = SIO_STREAM_SOCKET;
  stream->ops = &socket_ops;
  
  /* If this is a UDP socket, set the type to PSEUDO_SOCKET to match expected enum value */
  if (type == SOCK_DGRAM) {
    stream->type = SIO_STREAM_PSEUDO_SOCKET;
  }
  
  /* Create the socket */
#if defined(SIO_OS_WINDOWS)
  SOCKET sock = create_socket(domain, type, protocol, non_blocking);
  if (sock == INVALID_SOCKET) {
    return sio_get_last_error();
  }
  
  /* Set SO_REUSEADDR for server sockets */
  if (opt & SIO_STREAM_SERVER) {
    BOOL reuse = TRUE;
    if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, (char*)&reuse, sizeof(reuse)) == SOCKET_ERROR) {
      closesocket(sock);
      return sio_get_last_error();
    }
  }
  
  /* Bind or connect the socket */
  if (opt & SIO_STREAM_SERVER) {
    /* Bind the socket */
    if (bind(sock, &addr->addr.sa, addr->len) == SOCKET_ERROR) {
      closesocket(sock);
      return sio_get_last_error();
    }
    
    /* Listen if TCP */
    if (type == SOCK_STREAM) {
      if (listen(sock, SOMAXCONN) == SOCKET_ERROR) {
        closesocket(sock);
        return sio_get_last_error();
      }
    }
  } else {
    /* Connect the socket (TCP only) */
    if (type == SOCK_STREAM) {
      if (connect(sock, &addr->addr.sa, addr->len) == SOCKET_ERROR) {
        DWORD err = WSAGetLastError();
        if (err != WSAEWOULDBLOCK && err != WSAEINPROGRESS) {
          closesocket(sock);
          return sio_win_error_to_sio_error(err);
        }
        /* For non-blocking sockets, WSAEWOULDBLOCK is expected */
      }
    }
  }
  
  /* Store the socket */
  stream->data.socket.socket = sock;
#else
  /* POSIX implementation */
  int sock = create_socket(domain, type, protocol, non_blocking);
  if (sock < 0) {
    return sio_get_last_error();
  }
  
  /* Set SO_REUSEADDR for server sockets */
  if (opt & SIO_STREAM_SERVER) {
    int reuse = 1;
    if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse)) < 0) {
      close(sock);
      return sio_get_last_error();
    }
  }
  
  /* Bind or connect the socket */
  if (opt & SIO_STREAM_SERVER) {
    /* Bind the socket */
    if (bind(sock, &addr->addr.sa, addr->len) < 0) {
      close(sock);
      return sio_get_last_error();
    }
    
    /* Listen if TCP */
    if (type == SOCK_STREAM) {
      if (listen(sock, SOMAXCONN) < 0) {
        close(sock);
        return sio_get_last_error();
      }
    }
  } else {
    /* Connect the socket (TCP only) */
    if (type == SOCK_STREAM) {
      if (connect(sock, &addr->addr.sa, addr->len) < 0) {
        if (errno != EINPROGRESS && errno != EWOULDBLOCK) {
          close(sock);
          return sio_get_last_error();
        }
        /* For non-blocking sockets, EINPROGRESS is expected */
      }
    }
  }
  
  /* Store the socket */
  stream->data.socket.fd = sock;
#endif
  
  return SIO_SUCCESS;
}

/**
* @brief Open a socket stream from an existing socket handle
*/
sio_error_t sio_stream_open_socket_from_handle(sio_stream_t *stream, void *handle, sio_stream_flags_t opt) {
  if (!stream || !handle) {
    return SIO_ERROR_PARAM;
  }
  
  /* Initialize the stream structure */
  memset(stream, 0, sizeof(sio_stream_t));
  stream->type = SIO_STREAM_SOCKET;
  stream->flags = opt;
  stream->ops = &socket_ops;
  
#if defined(SIO_OS_WINDOWS)
  stream->data.socket.socket = (SOCKET)handle;
#else
  stream->data.socket.fd = (int)(intptr_t)handle;
#endif
  
  return SIO_SUCCESS;
}

/**
* @brief Accept a new connection on a server socket
* 
* @param server_stream Server socket stream
* @param client_stream Pointer to store the new client socket stream
* @param client_addr Pointer to store the client address (can be NULL)
* @return sio_error_t SIO_SUCCESS or error code
*/
sio_error_t sio_socket_accept(sio_stream_t *server_stream, sio_stream_t *client_stream, sio_addr_t *client_addr) {
  if (!server_stream || !client_stream || 
      (server_stream->type != SIO_STREAM_SOCKET && server_stream->type != SIO_STREAM_PSEUDO_SOCKET)) {
    return SIO_ERROR_PARAM;
  }
  
  /* Check if the server socket is in listening mode */
  if (!(server_stream->flags & SIO_STREAM_SERVER)) {
    return SIO_ERROR_PARAM;
  }
  
  /* Initialize client stream */
  memset(client_stream, 0, sizeof(sio_stream_t));
  client_stream->type = SIO_STREAM_SOCKET;
  client_stream->flags = server_stream->flags & ~SIO_STREAM_SERVER; /* Copy flags but remove server flag */
  client_stream->ops = &socket_ops;
  
  /* Accept the connection */
#if defined(SIO_OS_WINDOWS)
  struct sockaddr_storage addr;
  int addr_len = sizeof(addr);
  
  SOCKET client_sock = accept(server_stream->data.socket.socket, 
                              client_addr ? &client_addr->addr.sa : (struct sockaddr*)&addr, 
                              client_addr ? &client_addr->len : &addr_len);
  
  if (client_sock == INVALID_SOCKET) {
    return sio_get_last_error();
  }
  
  /* Set non-blocking mode if server socket is non-blocking */
  if (server_stream->flags & SIO_STREAM_NONBLOCK) {
    u_long mode = 1; /* 1 = non-blocking */
    if (ioctlsocket(client_sock, FIONBIO, &mode) == SOCKET_ERROR) {
      closesocket(client_sock);
      return sio_get_last_error();
    }
  }
  
  /* Store socket in client stream */
  client_stream->data.socket.socket = client_sock;
#else
  /* POSIX implementation */
  struct sockaddr_storage addr;
  socklen_t addr_len = sizeof(addr);
  
  int client_sock = accept(server_stream->data.socket.fd, 
                           client_addr ? &client_addr->addr.sa : (struct sockaddr*)&addr, 
                           client_addr ? &client_addr->len : &addr_len);
  
  if (client_sock < 0) {
    return sio_get_last_error();
  }
  
  /* Set non-blocking mode if server socket is non-blocking */
  if (server_stream->flags & SIO_STREAM_NONBLOCK) {
    int flags = fcntl(client_sock, F_GETFL, 0);
    if (flags == -1) {
      close(client_sock);
      return sio_get_last_error();
    }
    
    if (fcntl(client_sock, F_SETFL, flags | O_NONBLOCK) == -1) {
      close(client_sock);
      return sio_get_last_error();
    }
  }
  
  /* Set close-on-exec flag if supported */
#ifdef FD_CLOEXEC
  int flags = fcntl(client_sock, F_GETFD, 0);
  if (flags != -1) {
    fcntl(client_sock, F_SETFD, flags | FD_CLOEXEC);
  }
#endif
  
  /* Store socket in client stream */
  client_stream->data.socket.fd = client_sock;
#endif
  
  return SIO_SUCCESS;
}

/**
* @brief Close a socket stream
*/
static sio_error_t socket_close(sio_stream_t *stream) {
  assert(stream && (stream->type == SIO_STREAM_SOCKET || stream->type == SIO_STREAM_PSEUDO_SOCKET));
  
#if defined(SIO_OS_WINDOWS)
  /* Close the socket */
  if (stream->type == SIO_STREAM_SOCKET || stream->type == SIO_STREAM_PSEUDO_SOCKET) {
    SOCKET sock = stream->type == SIO_STREAM_SOCKET ? 
                  stream->data.socket.socket : stream->data.socket.socket;
    
    if (sock != INVALID_SOCKET) {
      if (closesocket(sock) == SOCKET_ERROR) {
        return sio_get_last_error();
      }
      if (stream->type == SIO_STREAM_SOCKET) {
        stream->data.socket.socket = INVALID_SOCKET;
      } else {
        stream->data.socket.socket = INVALID_SOCKET;
      }
    }
  }
#else
  /* POSIX implementation */
  if (stream->type == SIO_STREAM_SOCKET || stream->type == SIO_STREAM_PSEUDO_SOCKET) {
    int fd = stream->type == SIO_STREAM_SOCKET ? 
             stream->data.socket.fd : stream->data.socket.fd;
    
    if (fd >= 0) {
      if (close(fd) < 0) {
        return sio_get_last_error();
      }
      if (stream->type == SIO_STREAM_SOCKET) {
        stream->data.socket.fd = -1;
      } else {
        stream->data.socket.fd = -1;
      }
    }
  }
#endif
  
  return SIO_SUCCESS;
}

/**
* @brief Read from a socket stream
*/
static sio_error_t socket_read(sio_stream_t *stream, void *buffer, size_t size, size_t *bytes_read, int flags) {
  assert(stream && (stream->type == SIO_STREAM_SOCKET || stream->type == SIO_STREAM_PSEUDO_SOCKET));
  
  if (!buffer && size > 0) {
    return SIO_ERROR_PARAM;
  }
  
  /* Initialize bytes_read if provided */
  if (bytes_read) {
    *bytes_read = 0;
  }
  
  /* Early return if size is 0 */
  if (size == 0) {
    return SIO_SUCCESS;
  }
  
#if defined(SIO_OS_WINDOWS)
  SOCKET sock = stream->type == SIO_STREAM_SOCKET ? 
                stream->data.socket.socket : stream->data.socket.socket;
  
  if (sock == INVALID_SOCKET) {
    return SIO_ERROR_NET_NOT_SOCK;
  }
  
  /* For UDP sockets with an address, use recvfrom */
  if (stream->type == SIO_STREAM_PSEUDO_SOCKET) {
    struct sockaddr_storage addr;
    int addr_len = sizeof(addr);
    
    int recv_flags = 0;
    /* Convert SIO socket flags to native socket flags */
    if (flags & SIO_MSG_DONTWAIT) recv_flags |= MSG_DONTWAIT;
    if (flags & SIO_MSG_OOB) recv_flags |= MSG_OOB;
    
    int result = recvfrom(sock, (char*)buffer, (int)size, recv_flags, 
                         (struct sockaddr*)&addr, &addr_len);
    
    if (result == SOCKET_ERROR) {
      int err = WSAGetLastError();
      if (err == WSAEWOULDBLOCK) {
        /* Non-blocking socket would block */
        return SIO_ERROR_WOULDBLOCK;
      }
      return sio_win_error_to_sio_error(err);
    }
    
    if (bytes_read) {
      *bytes_read = result;
    }
    
    return (result > 0) ? SIO_SUCCESS : SIO_ERROR_EOF;
  }
  
  /* For TCP sockets, use recv */
  if (flags & SIO_DOALL) {
    /* Read all requested bytes (may require multiple reads) */
    size_t total_read = 0;
    char *buf_ptr = (char*)buffer;
    
    int recv_flags = 0;
    /* Convert SIO socket flags to native socket flags */
    if (flags & SIO_MSG_DONTWAIT) recv_flags |= MSG_DONTWAIT;
    if (flags & SIO_MSG_OOB) recv_flags |= MSG_OOB;
    
    while (total_read < size) {
      int result = recv(sock, buf_ptr + total_read, (int)(size - total_read), recv_flags);
      
      if (result == SOCKET_ERROR) {
        int err = WSAGetLastError();
        if (err == WSAEWOULDBLOCK) {
          /* Non-blocking socket would block */
          if (total_read > 0) {
            /* We've read some data, return success */
            if (bytes_read) {
              *bytes_read = total_read;
            }
            return SIO_SUCCESS;
          }
          return SIO_ERROR_WOULDBLOCK;
        }
        
        /* Other error */
        if (bytes_read) {
          *bytes_read = total_read;
        }
        return sio_win_error_to_sio_error(err);
      }
      
      if (result == 0) {
        /* Connection closed */
        if (bytes_read) {
          *bytes_read = total_read;
        }
        return (total_read > 0) ? SIO_SUCCESS : SIO_ERROR_EOF;
      }
      
      total_read += result;
      
      /* If non-blocking read all, return after first read */
      if (flags & SIO_DOALL_NONBLOCK) {
        break;
      }
    }
    
    if (bytes_read) {
      *bytes_read = total_read;
    }
    
    return SIO_SUCCESS;
  } else {
    /* Single read operation */
    int recv_flags = 0;
    /* Convert SIO socket flags to native socket flags */
    if (flags & SIO_MSG_DONTWAIT) recv_flags |= MSG_DONTWAIT;
    if (flags & SIO_MSG_OOB) recv_flags |= MSG_OOB;
    
    int result = recv(sock, (char*)buffer, (int)size, recv_flags);
    
    if (result == SOCKET_ERROR) {
      int err = WSAGetLastError();
      if (err == WSAEWOULDBLOCK) {
        /* Non-blocking socket would block */
        return SIO_ERROR_WOULDBLOCK;
      }
      return sio_win_error_to_sio_error(err);
    }
    
    if (bytes_read) {
      *bytes_read = result;
    }
    
    return (result > 0) ? SIO_SUCCESS : SIO_ERROR_EOF;
  }
#else
  /* POSIX implementation */
  int fd = stream->type == SIO_STREAM_SOCKET ? 
           stream->data.socket.fd : stream->data.socket.fd;
  
  if (fd < 0) {
    return SIO_ERROR_NET_NOT_SOCK;
  }
  
  /* For UDP sockets with an address, use recvfrom */
  if (stream->type == SIO_STREAM_PSEUDO_SOCKET) {
    struct sockaddr_storage addr;
    socklen_t addr_len = sizeof(addr);
    
    int recv_flags = 0;
    /* Convert SIO socket flags to native socket flags */
    if (flags & SIO_MSG_DONTWAIT) recv_flags |= MSG_DONTWAIT;
    if (flags & SIO_MSG_OOB) recv_flags |= MSG_OOB;
    
    ssize_t result = recvfrom(fd, buffer, size, recv_flags, 
                             (struct sockaddr*)&addr, &addr_len);
    
    if (result < 0) {
      if (errno == EAGAIN || errno == EWOULDBLOCK) {
        /* Non-blocking socket would block */
        return SIO_ERROR_WOULDBLOCK;
      }
      return sio_get_last_error();
    }
    
    if (bytes_read) {
      *bytes_read = result;
    }
    
    return (result > 0) ? SIO_SUCCESS : SIO_ERROR_EOF;
  }
  
  /* For TCP sockets, use recv */
  if (flags & SIO_DOALL) {
    /* Read all requested bytes (may require multiple reads) */
    size_t total_read = 0;
    uint8_t *buf_ptr = (uint8_t*)buffer;
    
    int recv_flags = 0;
    /* Convert SIO socket flags to native socket flags */
    if (flags & SIO_MSG_DONTWAIT) recv_flags |= MSG_DONTWAIT;
    if (flags & SIO_MSG_OOB) recv_flags |= MSG_OOB;
    
    while (total_read < size) {
      ssize_t result = recv(fd, buf_ptr + total_read, size - total_read, recv_flags);
      
      if (result < 0) {
        if (errno == EINTR) {
          /* Interrupted, try again */
          continue;
        }
        
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
          /* Non-blocking socket would block */
          if (total_read > 0) {
            /* We've read some data, return success */
            if (bytes_read) {
              *bytes_read = total_read;
            }
            return SIO_SUCCESS;
          }
          return SIO_ERROR_WOULDBLOCK;
        }
        
        /* Other error */
        if (bytes_read) {
          *bytes_read = total_read;
        }
        return sio_get_last_error();
      }
      
      if (result == 0) {
        /* Connection closed */
        if (bytes_read) {
          *bytes_read = total_read;
        }
        return (total_read > 0) ? SIO_SUCCESS : SIO_ERROR_EOF;
      }
      
      total_read += result;
      
      /* If non-blocking read all, return after first read */
      if (flags & SIO_DOALL_NONBLOCK) {
        break;
      }
    }
    
    if (bytes_read) {
      *bytes_read = total_read;
    }
    
    return SIO_SUCCESS;
  } else {
    /* Single read operation */
    int recv_flags = 0;
    /* Convert SIO socket flags to native socket flags */
    if (flags & SIO_MSG_DONTWAIT) recv_flags |= MSG_DONTWAIT;
    if (flags & SIO_MSG_OOB) recv_flags |= MSG_OOB;
    
    ssize_t result;
    do {
      result = recv(fd, buffer, size, recv_flags);
    } while (result < 0 && errno == EINTR);
    
    if (result < 0) {
      if (errno == EAGAIN || errno == EWOULDBLOCK) {
        /* Non-blocking socket would block */
        return SIO_ERROR_WOULDBLOCK;
      }
      return sio_get_last_error();
    }
    
    if (bytes_read) {
      *bytes_read = result;
    }
    
    return (result > 0) ? SIO_SUCCESS : SIO_ERROR_EOF;
  }
#endif
}

/**
* @brief Write to a socket stream
*/
static sio_error_t socket_write(sio_stream_t *stream, const void *buffer, size_t size, size_t *bytes_written, int flags) {
  assert(stream && (stream->type == SIO_STREAM_SOCKET || stream->type == SIO_STREAM_PSEUDO_SOCKET));
  
  if (!buffer && size > 0) {
    return SIO_ERROR_PARAM;
  }
  
  /* Initialize bytes_written if provided */
  if (bytes_written) {
    *bytes_written = 0;
  }
  
  /* Early return if size is 0 */
  if (size == 0) {
    return SIO_SUCCESS;
  }
  
#if defined(SIO_OS_WINDOWS)
  SOCKET sock = stream->type == SIO_STREAM_SOCKET ? 
                stream->data.socket.socket : stream->data.socket.socket;
  
  if (sock == INVALID_SOCKET) {
    return SIO_ERROR_NET_NOT_SOCK;
  }
  
  /* For UDP sockets with an address, use sendto */
  if (stream->type == SIO_STREAM_PSEUDO_SOCKET) {
    int send_flags = 0;
    /* Convert SIO socket flags to native socket flags */
    if (flags & SIO_MSG_DONTWAIT) send_flags |= MSG_DONTWAIT;
    if (flags & SIO_MSG_OOB) send_flags |= MSG_OOB;
    if (flags & SIO_MSG_DONTROUTE) send_flags |= MSG_DONTROUTE;
    if (flags & SIO_MSG_NOSIGNAL) send_flags |= MSG_NOSIGNAL;
    
    int result = sendto(sock, (const char*)buffer, (int)size, send_flags, 
                       &stream->data.pseudo_socket.addr.addr.sa, 
                       stream->data.pseudo_socket.addr.len);
    
    if (result == SOCKET_ERROR) {
      int err = WSAGetLastError();
      if (err == WSAEWOULDBLOCK) {
        /* Non-blocking socket would block */
        return SIO_ERROR_WOULDBLOCK;
      }
      return sio_win_error_to_sio_error(err);
    }
    
    if (bytes_written) {
      *bytes_written = result;
    }
    
    return SIO_SUCCESS;
  }
  
  /* For TCP sockets, use send */
  if (flags & SIO_DOALL) {
    /* Write all requested bytes (may require multiple writes) */
    size_t total_written = 0;
    const char *buf_ptr = (const char*)buffer;
    
    int send_flags = 0;
    /* Convert SIO socket flags to native socket flags */
    if (flags & SIO_MSG_DONTWAIT) send_flags |= MSG_DONTWAIT;
    if (flags & SIO_MSG_OOB) send_flags |= MSG_OOB;
    if (flags & SIO_MSG_DONTROUTE) send_flags |= MSG_DONTROUTE;
    if (flags & SIO_MSG_NOSIGNAL) send_flags |= MSG_NOSIGNAL;
    
    while (total_written < size) {
      int result = send(sock, buf_ptr + total_written, (int)(size - total_written), send_flags);
      
      if (result == SOCKET_ERROR) {
        int err = WSAGetLastError();
        if (err == WSAEWOULDBLOCK) {
          /* Non-blocking socket would block */
          if (total_written > 0) {
            /* We've written some data, return success */
            if (bytes_written) {
              *bytes_written = total_written;
            }
            return SIO_SUCCESS;
          }
          return SIO_ERROR_WOULDBLOCK;
        }
        
        /* Other error */
        if (bytes_written) {
          *bytes_written = total_written;
        }
        return sio_win_error_to_sio_error(err);
      }
      
      total_written += result;
      
      /* If non-blocking write all, return after first write */
      if (flags & SIO_DOALL_NONBLOCK) {
        break;
      }
    }
    
    if (bytes_written) {
      *bytes_written = total_written;
    }
    
    return SIO_SUCCESS;
  } else {
    /* Single write operation */
    int send_flags = 0;
    /* Convert SIO socket flags to native socket flags */
    if (flags & SIO_MSG_DONTWAIT) send_flags |= MSG_DONTWAIT;
    if (flags & SIO_MSG_OOB) send_flags |= MSG_OOB;
    if (flags & SIO_MSG_DONTROUTE) send_flags |= MSG_DONTROUTE;
    if (flags & SIO_MSG_NOSIGNAL) send_flags |= MSG_NOSIGNAL;
    
    int result = send(sock, (const char*)buffer, (int)size, send_flags);
    
    if (result == SOCKET_ERROR) {
      int err = WSAGetLastError();
      if (err == WSAEWOULDBLOCK) {
        /* Non-blocking socket would block */
        return SIO_ERROR_WOULDBLOCK;
      }
      return sio_win_error_to_sio_error(err);
    }
    
    if (bytes_written) {
      *bytes_written = result;
    }
    
    return SIO_SUCCESS;
  }
#else
  /* POSIX implementation */
  int fd = stream->type == SIO_STREAM_SOCKET ? 
           stream->data.socket.fd : stream->data.socket.fd;
  
  if (fd < 0) {
    return SIO_ERROR_NET_NOT_SOCK;
  }
  
  /* For UDP sockets with an address, use sendto */
  if (stream->type == SIO_STREAM_PSEUDO_SOCKET) {
    int send_flags = 0;
    /* Convert SIO socket flags to native socket flags */
    if (flags & SIO_MSG_DONTWAIT) send_flags |= MSG_DONTWAIT;
    if (flags & SIO_MSG_OOB) send_flags |= MSG_OOB;
    if (flags & SIO_MSG_DONTROUTE) send_flags |= MSG_DONTROUTE;
    if (flags & SIO_MSG_NOSIGNAL) send_flags |= MSG_NOSIGNAL;
    
    struct sockaddr *sa = &stream->data.pseudo_socket.addr.addr.sa;
    socklen_t len = stream->data.pseudo_socket.addr.len;
    
    /* Make sure we have a valid sockaddr length */
    if (len == 0) {
      if (sa->sa_family == AF_INET) {
        len = sizeof(struct sockaddr_in);
      } else if (sa->sa_family == AF_INET6) {
        len = sizeof(struct sockaddr_in6);
      }
    }
    
    ssize_t result = sendto(fd, buffer, size, send_flags, sa, len);
    
    if (result < 0) {
      if (errno == EAGAIN || errno == EWOULDBLOCK) {
        /* Non-blocking socket would block */
        return SIO_ERROR_WOULDBLOCK;
      }
      return sio_get_last_error();
    }
    
    if (bytes_written) {
      *bytes_written = result;
    }
    
    return SIO_SUCCESS;
  }
  
  /* For TCP sockets, use send */
  if (flags & SIO_DOALL) {
    /* Write all requested bytes (may require multiple writes) */
    size_t total_written = 0;
    const uint8_t *buf_ptr = (const uint8_t*)buffer;
    
    int send_flags = 0;
    /* Convert SIO socket flags to native socket flags */
    if (flags & SIO_MSG_DONTWAIT) send_flags |= MSG_DONTWAIT;
    if (flags & SIO_MSG_OOB) send_flags |= MSG_OOB;
    if (flags & SIO_MSG_DONTROUTE) send_flags |= MSG_DONTROUTE;
    if (flags & SIO_MSG_NOSIGNAL) send_flags |= MSG_NOSIGNAL;
    
    while (total_written < size) {
      ssize_t result = send(fd, buf_ptr + total_written, size - total_written, send_flags);
      
      if (result < 0) {
        if (errno == EINTR) {
          /* Interrupted, try again */
          continue;
        }
        
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
          /* Non-blocking socket would block */
          if (total_written > 0) {
            /* We've written some data, return success */
            if (bytes_written) {
              *bytes_written = total_written;
            }
            return SIO_SUCCESS;
          }
          return SIO_ERROR_WOULDBLOCK;
        }
        
        /* Other error */
        if (bytes_written) {
          *bytes_written = total_written;
        }
        return sio_get_last_error();
      }
      
      total_written += result;
      
      /* If non-blocking write all, return after first write */
      if (flags & SIO_DOALL_NONBLOCK) {
        break;
      }
    }
    
    if (bytes_written) {
      *bytes_written = total_written;
    }
    
    return SIO_SUCCESS;
  } else {
    /* Single write operation */
    int send_flags = 0;
    /* Convert SIO socket flags to native socket flags */
    if (flags & SIO_MSG_DONTWAIT) send_flags |= MSG_DONTWAIT;
    if (flags & SIO_MSG_OOB) send_flags |= MSG_OOB;
    if (flags & SIO_MSG_DONTROUTE) send_flags |= MSG_DONTROUTE;
    if (flags & SIO_MSG_NOSIGNAL) send_flags |= MSG_NOSIGNAL;
    
    ssize_t result;
    do {
      result = send(fd, buffer, size, send_flags);
    } while (result < 0 && errno == EINTR);
    
    if (result < 0) {
      if (errno == EAGAIN || errno == EWOULDBLOCK) {
        /* Non-blocking socket would block */
        return SIO_ERROR_WOULDBLOCK;
      }
      return sio_get_last_error();
    }
    
    if (bytes_written) {
      *bytes_written = result;
    }
    
    return SIO_SUCCESS;
  }
#endif
}

/**
* @brief Read from a socket stream using scatter-gather I/O
*/
static sio_error_t socket_readv(sio_stream_t *stream, sio_iovec_t *iov, size_t iovcnt, size_t *bytes_read, int flags) {
  assert(stream && (stream->type == SIO_STREAM_SOCKET || stream->type == SIO_STREAM_PSEUDO_SOCKET));
  
  if (!iov || iovcnt == 0) {
    return SIO_ERROR_PARAM;
  }
  
  /* Initialize bytes_read if provided */
  if (bytes_read) {
    *bytes_read = 0;
  }
  
#if defined(SIO_OS_WINDOWS)
  SOCKET sock = stream->type == SIO_STREAM_SOCKET ? 
                stream->data.socket.socket : stream->data.socket.socket;
  
  if (sock == INVALID_SOCKET) {
    return SIO_ERROR_NET_NOT_SOCK;
  }
  
  /* For UDP, we have to fallback to a single recvfrom */
  if (stream->type == SIO_STREAM_PSEUDO_SOCKET) {
    /* Calculate total buffer size */
    size_t total_size = 0;
    for (size_t i = 0; i < iovcnt; i++) {
      total_size += iov[i].len;
    }
    
    /* Allocate a temporary buffer */
    char *temp_buffer = (char*)malloc(total_size);
    if (!temp_buffer) {
      return SIO_ERROR_MEM;
    }
    
    /* Receive into the temporary buffer */
    struct sockaddr_storage addr;
    int addr_len = sizeof(addr);
    
    int recv_flags = 0;
    /* Convert SIO socket flags to native socket flags */
    if (flags & SIO_MSG_DONTWAIT) recv_flags |= MSG_DONTWAIT;
    if (flags & SIO_MSG_OOB) recv_flags |= MSG_OOB;
    
    int result = recvfrom(sock, temp_buffer, (int)total_size, recv_flags, 
                         (struct sockaddr*)&addr, &addr_len);
    
    if (result == SOCKET_ERROR) {
      int err = WSAGetLastError();
      free(temp_buffer);
      if (err == WSAEWOULDBLOCK) {
        /* Non-blocking socket would block */
        return SIO_ERROR_WOULDBLOCK;
      }
      return sio_win_error_to_sio_error(err);
    }
    
    /* Copy data to the iovec buffers */
    size_t copied = 0;
    for (size_t i = 0; i < iovcnt && copied < (size_t)result; i++) {
      size_t to_copy = iov[i].len;
      if (copied + to_copy > (size_t)result) {
        to_copy = result - copied;
      }
      
      memcpy(iov[i].buf, temp_buffer + copied, to_copy);
      copied += to_copy;
    }
    
    free(temp_buffer);
    
    if (bytes_read) {
      *bytes_read = result;
    }
    
    return (result > 0) ? SIO_SUCCESS : SIO_ERROR_EOF;
  }
  
  /* For TCP, use WSARecv */
  DWORD total_read = 0;
  WSABUF *wsabufs = (WSABUF*)malloc(iovcnt * sizeof(WSABUF));
  if (!wsabufs) {
    return SIO_ERROR_MEM;
  }
  
  /* Set up WSABUFs */
  for (size_t i = 0; i < iovcnt; i++) {
    wsabufs[i].buf = iov[i].buf;
    wsabufs[i].len = iov[i].len;
  }
  
  /* Set flags */
  DWORD recv_flags = 0;
  /* Convert SIO socket flags to native socket flags */
  if (flags & SIO_MSG_OOB) recv_flags |= MSG_OOB;
  
  /* Read from the socket */
  int result = WSARecv(sock, wsabufs, (DWORD)iovcnt, &total_read, &recv_flags, NULL, NULL);
  
  free(wsabufs);
  
  if (result == SOCKET_ERROR) {
    int err = WSAGetLastError();
    if (err == WSAEWOULDBLOCK) {
      /* Non-blocking socket would block */
      return SIO_ERROR_WOULDBLOCK;
    }
    return sio_win_error_to_sio_error(err);
  }
  
  if (bytes_read) {
    *bytes_read = total_read;
  }
  
  return (total_read > 0) ? SIO_SUCCESS : SIO_ERROR_EOF;
#else
  /* POSIX implementation */
  int fd = stream->type == SIO_STREAM_SOCKET ? 
           stream->data.socket.fd : stream->data.socket.fd;
  
  if (fd < 0) {
    return SIO_ERROR_NET_NOT_SOCK;
  }
  
  /* For UDP, we have to fallback to a single recvfrom */
  if (stream->type == SIO_STREAM_PSEUDO_SOCKET) {
    /* Calculate total buffer size */
    size_t total_size = 0;
    for (size_t i = 0; i < iovcnt; i++) {
      total_size += iov[i].iov_len;
    }
    
    /* Allocate a temporary buffer */
    void *temp_buffer = malloc(total_size);
    if (!temp_buffer) {
      return SIO_ERROR_MEM;
    }
    
    /* Receive into the temporary buffer */
    struct sockaddr_storage addr;
    socklen_t addr_len = sizeof(addr);
    
    int recv_flags = 0;
    /* Convert SIO socket flags to native socket flags */
    if (flags & SIO_MSG_DONTWAIT) recv_flags |= MSG_DONTWAIT;
    if (flags & SIO_MSG_OOB) recv_flags |= MSG_OOB;
    
    ssize_t result = recvfrom(fd, temp_buffer, total_size, recv_flags, 
                             (struct sockaddr*)&addr, &addr_len);
    
    if (result < 0) {
      free(temp_buffer);
      if (errno == EAGAIN || errno == EWOULDBLOCK) {
        /* Non-blocking socket would block */
        return SIO_ERROR_WOULDBLOCK;
      }
      return sio_get_last_error();
    }
    
    /* Copy data to the iovec buffers */
    size_t copied = 0;
    for (size_t i = 0; i < iovcnt && copied < (size_t)result; i++) {
      size_t to_copy = iov[i].iov_len;
      if (copied + to_copy > (size_t)result) {
        to_copy = result - copied;
      }
      
      memcpy(iov[i].iov_base, (uint8_t*)temp_buffer + copied, to_copy);
      copied += to_copy;
    }
    
    free(temp_buffer);
    
    if (bytes_read) {
      *bytes_read = result;
    }
    
    return (result > 0) ? SIO_SUCCESS : SIO_ERROR_EOF;
  }
  
  /* For TCP, use readv */
  struct iovec *posix_iov = (struct iovec*)malloc(iovcnt * sizeof(struct iovec));
  if (!posix_iov) {
    return SIO_ERROR_MEM;
  }
  
  /* Set up iovec structures */
  for (size_t i = 0; i < iovcnt; i++) {
    posix_iov[i].iov_base = iov[i].iov_base;
    posix_iov[i].iov_len = iov[i].iov_len;
  }
  
  /* We can't use msg_flags with readv, so if special flags are needed,
     we have to use recvmsg instead */
  if (flags & (SIO_MSG_DONTWAIT | SIO_MSG_OOB)) {
    struct msghdr msg;
    memset(&msg, 0, sizeof(msg));
    msg.msg_iov = posix_iov;
    msg.msg_iovlen = iovcnt;
    
    int recv_flags = 0;
    /* Convert SIO socket flags to native socket flags */
    if (flags & SIO_MSG_DONTWAIT) recv_flags |= MSG_DONTWAIT;
    if (flags & SIO_MSG_OOB) recv_flags |= MSG_OOB;
    
    ssize_t result;
    do {
      result = recvmsg(fd, &msg, recv_flags);
    } while (result < 0 && errno == EINTR);
    
    free(posix_iov);
    
    if (result < 0) {
      if (errno == EAGAIN || errno == EWOULDBLOCK) {
        /* Non-blocking socket would block */
        return SIO_ERROR_WOULDBLOCK;
      }
      return sio_get_last_error();
    }
    
    if (bytes_read) {
      *bytes_read = result;
    }
    
    return (result > 0) ? SIO_SUCCESS : SIO_ERROR_EOF;
  } else {
    /* Use readv for simple cases */
    ssize_t result;
    do {
      result = readv(fd, posix_iov, iovcnt);
    } while (result < 0 && errno == EINTR);
    
    free(posix_iov);
    
    if (result < 0) {
      if (errno == EAGAIN || errno == EWOULDBLOCK) {
        /* Non-blocking socket would block */
        return SIO_ERROR_WOULDBLOCK;
      }
      return sio_get_last_error();
    }
    
    if (bytes_read) {
      *bytes_read = result;
    }
    
    return (result > 0) ? SIO_SUCCESS : SIO_ERROR_EOF;
  }
#endif
}

/**
* @brief Write to a socket stream using scatter-gather I/O
*/
static sio_error_t socket_writev(sio_stream_t *stream, const sio_iovec_t *iov, size_t iovcnt, size_t *bytes_written, int flags) {
  assert(stream && (stream->type == SIO_STREAM_SOCKET || stream->type == SIO_STREAM_PSEUDO_SOCKET));
  
  if (!iov || iovcnt == 0) {
    return SIO_ERROR_PARAM;
  }
  
  /* Initialize bytes_written if provided */
  if (bytes_written) {
    *bytes_written = 0;
  }
  
#if defined(SIO_OS_WINDOWS)
  SOCKET sock = stream->type == SIO_STREAM_SOCKET ? 
                stream->data.socket.socket : stream->data.socket.socket;
  
  if (sock == INVALID_SOCKET) {
    return SIO_ERROR_NET_NOT_SOCK;
  }
  
  /* For UDP, we have to fallback to a single sendto */
  if (stream->type == SIO_STREAM_PSEUDO_SOCKET) {
    /* Calculate total buffer size */
    size_t total_size = 0;
    for (size_t i = 0; i < iovcnt; i++) {
      total_size += iov[i].len;
    }
    
    /* Allocate a temporary buffer */
    char *temp_buffer = (char*)malloc(total_size);
    if (!temp_buffer) {
      return SIO_ERROR_MEM;
    }
    
    /* Copy data from the iovec buffers */
    size_t offset = 0;
    for (size_t i = 0; i < iovcnt; i++) {
      memcpy(temp_buffer + offset, iov[i].buf, iov[i].len);
      offset += iov[i].len;
    }
    
    /* Send the buffer */
    int send_flags = 0;
    /* Convert SIO socket flags to native socket flags */
    if (flags & SIO_MSG_DONTWAIT) send_flags |= MSG_DONTWAIT;
    if (flags & SIO_MSG_OOB) send_flags |= MSG_OOB;
    if (flags & SIO_MSG_DONTROUTE) send_flags |= MSG_DONTROUTE;
    if (flags & SIO_MSG_NOSIGNAL) send_flags |= MSG_NOSIGNAL;
    
    int result = sendto(sock, temp_buffer, (int)total_size, send_flags, 
                       &stream->data.pseudo_socket.addr.addr.sa, 
                       stream->data.pseudo_socket.addr.len);
    
    free(temp_buffer);
    
    if (result == SOCKET_ERROR) {
      int err = WSAGetLastError();
      if (err == WSAEWOULDBLOCK) {
        /* Non-blocking socket would block */
        return SIO_ERROR_WOULDBLOCK;
      }
      return sio_win_error_to_sio_error(err);
    }
    
    if (bytes_written) {
      *bytes_written = result;
    }
    
    return SIO_SUCCESS;
  }
  
  /* For TCP, use WSASend */
  DWORD total_sent = 0;
  WSABUF *wsabufs = (WSABUF*)malloc(iovcnt * sizeof(WSABUF));
  if (!wsabufs) {
    return SIO_ERROR_MEM;
  }
  
  /* Set up WSABUFs */
  for (size_t i = 0; i < iovcnt; i++) {
    wsabufs[i].buf = (char*)iov[i].buf;
    wsabufs[i].len = iov[i].len;
  }
  
  /* Set flags */
  DWORD send_flags = 0;
  /* Convert SIO socket flags to native socket flags */
  if (flags & SIO_MSG_OOB) send_flags |= MSG_OOB;
  if (flags & SIO_MSG_DONTROUTE) send_flags |= MSG_DONTROUTE;
  if (flags & SIO_MSG_NOSIGNAL) send_flags |= MSG_NOSIGNAL;
  
  /* Write to the socket */
  int result = WSASend(sock, wsabufs, (DWORD)iovcnt, &total_sent, send_flags, NULL, NULL);
  
  free(wsabufs);
  
  if (result == SOCKET_ERROR) {
    int err = WSAGetLastError();
    if (err == WSAEWOULDBLOCK) {
      /* Non-blocking socket would block */
      return SIO_ERROR_WOULDBLOCK;
    }
    return sio_win_error_to_sio_error(err);
  }
  
  if (bytes_written) {
    *bytes_written = total_sent;
  }
  
  return SIO_SUCCESS;
#else
  /* POSIX implementation */
  int fd = stream->type == SIO_STREAM_SOCKET ? 
           stream->data.socket.fd : stream->data.socket.fd;
  
  if (fd < 0) {
    return SIO_ERROR_NET_NOT_SOCK;
  }
  
  /* For UDP, we have to fallback to a single sendto */
  if (stream->type == SIO_STREAM_PSEUDO_SOCKET) {
    /* Calculate total buffer size */
    size_t total_size = 0;
    for (size_t i = 0; i < iovcnt; i++) {
      total_size += iov[i].iov_len;
    }
    
    /* Allocate a temporary buffer */
    void *temp_buffer = malloc(total_size);
    if (!temp_buffer) {
      return SIO_ERROR_MEM;
    }
    
    /* Copy data from the iovec buffers */
    size_t offset = 0;
    for (size_t i = 0; i < iovcnt; i++) {
      memcpy((uint8_t*)temp_buffer + offset, iov[i].iov_base, iov[i].iov_len);
      offset += iov[i].iov_len;
    }
    
    /* Send the buffer */
    int send_flags = 0;
    /* Convert SIO socket flags to native socket flags */
    if (flags & SIO_MSG_DONTWAIT) send_flags |= MSG_DONTWAIT;
    if (flags & SIO_MSG_OOB) send_flags |= MSG_OOB;
    if (flags & SIO_MSG_DONTROUTE) send_flags |= MSG_DONTROUTE;
    if (flags & SIO_MSG_NOSIGNAL) send_flags |= MSG_NOSIGNAL;
    
    ssize_t result = sendto(fd, temp_buffer, total_size, send_flags, 
                           &stream->data.pseudo_socket.addr.addr.sa, 
                           stream->data.pseudo_socket.addr.len);
    
    free(temp_buffer);
    
    if (result < 0) {
      if (errno == EAGAIN || errno == EWOULDBLOCK) {
        /* Non-blocking socket would block */
        return SIO_ERROR_WOULDBLOCK;
      }
      return sio_get_last_error();
    }
    
    if (bytes_written) {
      *bytes_written = result;
    }
    
    return SIO_SUCCESS;
  }
  
  /* For TCP, use writev */
  struct iovec *posix_iov = (struct iovec*)malloc(iovcnt * sizeof(struct iovec));
  if (!posix_iov) {
    return SIO_ERROR_MEM;
  }
  
  /* Set up iovec structures */
  for (size_t i = 0; i < iovcnt; i++) {
    posix_iov[i].iov_base = (void*)iov[i].iov_base;
    posix_iov[i].iov_len = iov[i].iov_len;
  }
  
  /* We can't use msg_flags with writev, so if special flags are needed,
     we have to use sendmsg instead */
  if (flags & (SIO_MSG_DONTWAIT | SIO_MSG_OOB | SIO_MSG_DONTROUTE | SIO_MSG_NOSIGNAL)) {
    struct msghdr msg;
    memset(&msg, 0, sizeof(msg));
    msg.msg_iov = posix_iov;
    msg.msg_iovlen = iovcnt;
    
    int send_flags = 0;
    /* Convert SIO socket flags to native socket flags */
    if (flags & SIO_MSG_DONTWAIT) send_flags |= MSG_DONTWAIT;
    if (flags & SIO_MSG_OOB) send_flags |= MSG_OOB;
    if (flags & SIO_MSG_DONTROUTE) send_flags |= MSG_DONTROUTE;
    if (flags & SIO_MSG_NOSIGNAL) send_flags |= MSG_NOSIGNAL;
    
    ssize_t result;
    do {
      result = sendmsg(fd, &msg, send_flags);
    } while (result < 0 && errno == EINTR);
    
    free(posix_iov);
    
    if (result < 0) {
      if (errno == EAGAIN || errno == EWOULDBLOCK) {
        /* Non-blocking socket would block */
        return SIO_ERROR_WOULDBLOCK;
      }
      return sio_get_last_error();
    }
    
    if (bytes_written) {
      *bytes_written = result;
    }
    
    return SIO_SUCCESS;
  } else {
    /* Use writev for simple cases */
    ssize_t result;
    do {
      result = writev(fd, posix_iov, iovcnt);
    } while (result < 0 && errno == EINTR);
    
    free(posix_iov);
    
    if (result < 0) {
      if (errno == EAGAIN || errno == EWOULDBLOCK) {
        /* Non-blocking socket would block */
        return SIO_ERROR_WOULDBLOCK;
      }
      return sio_get_last_error();
    }
    
    if (bytes_written) {
      *bytes_written = result;
    }
    
    return SIO_SUCCESS;
  }
#endif
}

/**
* @brief Get socket stream options
*/
static sio_error_t socket_get_option(sio_stream_t *stream, sio_stream_option_t option, void *value, size_t *size) {
  assert(stream && (stream->type == SIO_STREAM_SOCKET || stream->type == SIO_STREAM_PSEUDO_SOCKET));
  
  if (!value || !size || *size == 0) {
    return SIO_ERROR_PARAM;
  }
  
#if defined(SIO_OS_WINDOWS)
  SOCKET sock = stream->type == SIO_STREAM_SOCKET ? 
                stream->data.socket.socket : stream->data.socket.socket;
  
  if (sock == INVALID_SOCKET) {
    return SIO_ERROR_NET_NOT_SOCK;
  }
#else
  int fd = stream->type == SIO_STREAM_SOCKET ? 
           stream->data.socket.fd : stream->data.socket.fd;
  
  if (fd < 0) {
    return SIO_ERROR_NET_NOT_SOCK;
  }
#endif
  
  switch (option) {
    case SIO_INFO_TYPE:
      if (*size < sizeof(sio_stream_type_t)) {
        return SIO_ERROR_BUFFER_TOO_SMALL;
      }
      *((sio_stream_type_t*)value) = stream->type;
      *size = sizeof(sio_stream_type_t);
      break;
      
    case SIO_INFO_FLAGS:
      if (*size < sizeof(int)) {
        return SIO_ERROR_BUFFER_TOO_SMALL;
      }
      *((int*)value) = stream->flags;
      *size = sizeof(int);
      break;
      
    case SIO_INFO_READABLE: {
      if (*size < sizeof(int)) {
        return SIO_ERROR_BUFFER_TOO_SMALL;
      }
      int readable = (stream->flags & SIO_STREAM_READ) ? 1 : 0;
      *((int*)value) = readable;
      *size = sizeof(int);
      break;
    }
      
    case SIO_INFO_WRITABLE: {
      if (*size < sizeof(int)) {
        return SIO_ERROR_BUFFER_TOO_SMALL;
      }
      int writable = (stream->flags & SIO_STREAM_WRITE) ? 1 : 0;
      *((int*)value) = writable;
      *size = sizeof(int);
      break;
    }
      
    case SIO_INFO_SEEKABLE: {
      if (*size < sizeof(int)) {
        return SIO_ERROR_BUFFER_TOO_SMALL;
      }
      /* Sockets are not seekable */
      *((int*)value) = 0;
      *size = sizeof(int);
      break;
    }
      
    case SIO_INFO_HANDLE: {
#if defined(SIO_OS_WINDOWS)
      if (*size < sizeof(SOCKET)) {
        return SIO_ERROR_BUFFER_TOO_SMALL;
      }
      *((SOCKET*)value) = sock;
      *size = sizeof(SOCKET);
#else
      if (*size < sizeof(int)) {
        return SIO_ERROR_BUFFER_TOO_SMALL;
      }
      *((int*)value) = fd;
      *size = sizeof(int);
#endif
      break;
    }
      
    case SIO_OPT_BLOCKING: {
      if (*size < sizeof(int)) {
        return SIO_ERROR_BUFFER_TOO_SMALL;
      }
      
      int blocking;
      
#if defined(SIO_OS_WINDOWS)
      u_long mode;
      if (ioctlsocket(sock, FIONBIO, &mode) == SOCKET_ERROR) {
        return sio_get_last_error();
      }
      blocking = (mode == 0) ? 1 : 0;
#else
      int flags = fcntl(fd, F_GETFL, 0);
      if (flags < 0) {
        return sio_get_last_error();
      }
      blocking = ((flags & O_NONBLOCK) == 0) ? 1 : 0;
#endif
      
      *((int*)value) = blocking;
      *size = sizeof(int);
      break;
    }
      
    case SIO_OPT_SOCK_NODELAY: {
      if (*size < sizeof(int)) {
        return SIO_ERROR_BUFFER_TOO_SMALL;
      }
      
      int nodelay;
      socklen_t optlen = sizeof(nodelay);
      
#if defined(SIO_OS_WINDOWS)
      if (getsockopt(sock, IPPROTO_TCP, TCP_NODELAY, (char*)&nodelay, &optlen) == SOCKET_ERROR) {
        return sio_get_last_error();
      }
#else
      if (getsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &nodelay, &optlen) < 0) {
        return sio_get_last_error();
      }
#endif
      
      *((int*)value) = nodelay;
      *size = sizeof(int);
      break;
    }
      
    case SIO_OPT_SOCK_KEEPALIVE: {
      if (*size < sizeof(int)) {
        return SIO_ERROR_BUFFER_TOO_SMALL;
      }
      
      int keepalive;
      socklen_t optlen = sizeof(keepalive);
      
#if defined(SIO_OS_WINDOWS)
      if (getsockopt(sock, SOL_SOCKET, SO_KEEPALIVE, (char*)&keepalive, &optlen) == SOCKET_ERROR) {
        return sio_get_last_error();
      }
#else
      if (getsockopt(fd, SOL_SOCKET, SO_KEEPALIVE, &keepalive, &optlen) < 0) {
        return sio_get_last_error();
      }
#endif
      
      *((int*)value) = keepalive;
      *size = sizeof(int);
      break;
    }
      
    case SIO_OPT_SOCK_REUSEADDR: {
      if (*size < sizeof(int)) {
        return SIO_ERROR_BUFFER_TOO_SMALL;
      }
      
      int reuseaddr;
      socklen_t optlen = sizeof(reuseaddr);
      
#if defined(SIO_OS_WINDOWS)
      if (getsockopt(sock, SOL_SOCKET, SO_REUSEADDR, (char*)&reuseaddr, &optlen) == SOCKET_ERROR) {
        return sio_get_last_error();
      }
#else
      if (getsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &reuseaddr, &optlen) < 0) {
        return sio_get_last_error();
      }
#endif
      
      *((int*)value) = reuseaddr;
      *size = sizeof(int);
      break;
    }
      
    case SIO_OPT_SOCK_BROADCAST: {
      if (*size < sizeof(int)) {
        return SIO_ERROR_BUFFER_TOO_SMALL;
      }
      
      int broadcast;
      socklen_t optlen = sizeof(broadcast);
      
#if defined(SIO_OS_WINDOWS)
      if (getsockopt(sock, SOL_SOCKET, SO_BROADCAST, (char*)&broadcast, &optlen) == SOCKET_ERROR) {
        return sio_get_last_error();
      }
#else
      if (getsockopt(fd, SOL_SOCKET, SO_BROADCAST, &broadcast, &optlen) < 0) {
        return sio_get_last_error();
      }
#endif
      
      *((int*)value) = broadcast;
      *size = sizeof(int);
      break;
    }
      
    case SIO_OPT_SOCK_RCVBUF: {
      if (*size < sizeof(int)) {
        return SIO_ERROR_BUFFER_TOO_SMALL;
      }
      
      int rcvbuf;
      socklen_t optlen = sizeof(rcvbuf);
      
#if defined(SIO_OS_WINDOWS)
      if (getsockopt(sock, SOL_SOCKET, SO_RCVBUF, (char*)&rcvbuf, &optlen) == SOCKET_ERROR) {
        return sio_get_last_error();
      }
#else
      if (getsockopt(fd, SOL_SOCKET, SO_RCVBUF, &rcvbuf, &optlen) < 0) {
        return sio_get_last_error();
      }
#endif
      
      *((int*)value) = rcvbuf;
      *size = sizeof(int);
      break;
    }
      
    case SIO_OPT_SOCK_SNDBUF: {
      if (*size < sizeof(int)) {
        return SIO_ERROR_BUFFER_TOO_SMALL;
      }
      
      int sndbuf;
      socklen_t optlen = sizeof(sndbuf);
      
#if defined(SIO_OS_WINDOWS)
      if (getsockopt(sock, SOL_SOCKET, SO_SNDBUF, (char*)&sndbuf, &optlen) == SOCKET_ERROR) {
        return sio_get_last_error();
      }
#else
      if (getsockopt(fd, SOL_SOCKET, SO_SNDBUF, &sndbuf, &optlen) < 0) {
        return sio_get_last_error();
      }
#endif
      
      *((int*)value) = sndbuf;
      *size = sizeof(int);
      break;
    }
      
    default:
      return SIO_ERROR_UNSUPPORTED;
  }
  
  return SIO_SUCCESS;
}

/**
* @brief Set socket stream options
*/
static sio_error_t socket_set_option(sio_stream_t *stream, sio_stream_option_t option, const void *value, size_t size) {
  assert(stream && (stream->type == SIO_STREAM_SOCKET || stream->type == SIO_STREAM_PSEUDO_SOCKET));
  
  if (!value) {
    return SIO_ERROR_PARAM;
  }
  
#if defined(SIO_OS_WINDOWS)
  SOCKET sock = stream->type == SIO_STREAM_SOCKET ? 
                stream->data.socket.socket : stream->data.socket.socket;
  
  if (sock == INVALID_SOCKET) {
    return SIO_ERROR_NET_NOT_SOCK;
  }
#else
  int fd = stream->type == SIO_STREAM_SOCKET ? 
           stream->data.socket.fd : stream->data.socket.fd;
  
  if (fd < 0) {
    return SIO_ERROR_NET_NOT_SOCK;
  }
#endif
  
  switch (option) {
    case SIO_OPT_BLOCKING: {
      if (size < sizeof(int)) {
        return SIO_ERROR_PARAM;
      }
      
      int blocking = *((const int*)value);
      
#if defined(SIO_OS_WINDOWS)
      u_long mode = blocking ? 0 : 1; /* 0 = blocking, 1 = non-blocking */
      if (ioctlsocket(sock, FIONBIO, &mode) == SOCKET_ERROR) {
        return sio_get_last_error();
      }
#else
      int flags = fcntl(fd, F_GETFL, 0);
      if (flags < 0) {
        return sio_get_last_error();
      }
      
      if (blocking) {
        flags &= ~O_NONBLOCK;
      } else {
        flags |= O_NONBLOCK;
      }
      
      if (fcntl(fd, F_SETFL, flags) < 0) {
        return sio_get_last_error();
      }
#endif
      
      /* Update flags */
      if (blocking) {
        stream->flags &= ~SIO_STREAM_NONBLOCK;
      } else {
        stream->flags |= SIO_STREAM_NONBLOCK;
      }
      
      break;
    }
      
    case SIO_OPT_SOCK_NODELAY: {
      if (size < sizeof(int)) {
        return SIO_ERROR_PARAM;
      }
      
      int nodelay = *((const int*)value);
      
#if defined(SIO_OS_WINDOWS)
      if (setsockopt(sock, IPPROTO_TCP, TCP_NODELAY, (const char*)&nodelay, sizeof(nodelay)) == SOCKET_ERROR) {
        return sio_get_last_error();
      }
#else
      if (setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &nodelay, sizeof(nodelay)) < 0) {
        return sio_get_last_error();
      }
#endif
      
      break;
    }
      
    case SIO_OPT_SOCK_KEEPALIVE: {
      if (size < sizeof(int)) {
        return SIO_ERROR_PARAM;
      }
      
      int keepalive = *((const int*)value);
      
#if defined(SIO_OS_WINDOWS)
      if (setsockopt(sock, SOL_SOCKET, SO_KEEPALIVE, (const char*)&keepalive, sizeof(keepalive)) == SOCKET_ERROR) {
        return sio_get_last_error();
      }
#else
      if (setsockopt(fd, SOL_SOCKET, SO_KEEPALIVE, &keepalive, sizeof(keepalive)) < 0) {
        return sio_get_last_error();
      }
#endif
      
      break;
    }
      
    case SIO_OPT_SOCK_REUSEADDR: {
      if (size < sizeof(int)) {
        return SIO_ERROR_PARAM;
      }
      
      int reuseaddr = *((const int*)value);
      
#if defined(SIO_OS_WINDOWS)
      if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, (const char*)&reuseaddr, sizeof(reuseaddr)) == SOCKET_ERROR) {
        return sio_get_last_error();
      }
#else
      if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &reuseaddr, sizeof(reuseaddr)) < 0) {
        return sio_get_last_error();
      }
#endif
      
      break;
    }
      
    case SIO_OPT_SOCK_BROADCAST: {
      if (size < sizeof(int)) {
        return SIO_ERROR_PARAM;
      }
      
      int broadcast = *((const int*)value);
      
#if defined(SIO_OS_WINDOWS)
      if (setsockopt(sock, SOL_SOCKET, SO_BROADCAST, (const char*)&broadcast, sizeof(broadcast)) == SOCKET_ERROR) {
        return sio_get_last_error();
      }
#else
      if (setsockopt(fd, SOL_SOCKET, SO_BROADCAST, &broadcast, sizeof(broadcast)) < 0) {
        return sio_get_last_error();
      }
#endif
      
      break;
    }
      
    case SIO_OPT_SOCK_RCVBUF: {
      if (size < sizeof(int)) {
        return SIO_ERROR_PARAM;
      }
      
      int rcvbuf = *((const int*)value);
      
#if defined(SIO_OS_WINDOWS)
      if (setsockopt(sock, SOL_SOCKET, SO_RCVBUF, (const char*)&rcvbuf, sizeof(rcvbuf)) == SOCKET_ERROR) {
        return sio_get_last_error();
      }
#else
      if (setsockopt(fd, SOL_SOCKET, SO_RCVBUF, &rcvbuf, sizeof(rcvbuf)) < 0) {
        return sio_get_last_error();
      }
#endif
      
      break;
    }
      
    case SIO_OPT_SOCK_SNDBUF: {
      if (size < sizeof(int)) {
        return SIO_ERROR_PARAM;
      }
      
      int sndbuf = *((const int*)value);
      
#if defined(SIO_OS_WINDOWS)
      if (setsockopt(sock, SOL_SOCKET, SO_SNDBUF, (const char*)&sndbuf, sizeof(sndbuf)) == SOCKET_ERROR) {
        return sio_get_last_error();
      }
#else
      if (setsockopt(fd, SOL_SOCKET, SO_SNDBUF, &sndbuf, sizeof(sndbuf)) < 0) {
        return sio_get_last_error();
      }
#endif
      
      break;
    }
      
    default:
      return SIO_ERROR_UNSUPPORTED;
  }
  
  return SIO_SUCCESS;
}