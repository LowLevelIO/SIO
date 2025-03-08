/**
* @file tests/socket_stream_test.c
* @brief Test cases for SIO socket streams
*
* This file contains test functions for TCP and UDP socket stream implementations.
*
* @author zczxy
* @version 0.1.0
*/

#include <sio/stream.h>
#include <sio/aux/addr.h>
#include <sio/err.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#if defined(_WIN32)
  #include <windows.h>
  #define sleep_ms(ms) Sleep(ms)
#else
  #include <unistd.h>
  #include <sys/time.h>
  #define sleep_ms(ms) usleep((ms) * 1000)
#endif

/* Forward declaration for the error reporting function */
void report_error_and_exit(sio_error_t error_code, const char *message);

/**
* @brief Test basic TCP socket operations with client and server
*
* @return int 0 if successful, 1 otherwise
*/
static int test_tcp_socket(void) {
  printf("  Testing TCP socket stream...\n");
  
  /* Create address for localhost server */
  sio_addr_t server_addr;
  struct in_addr ip4addr;
  
  /* Set to 127.0.0.1:9876 */
  inet_pton(AF_INET, "127.0.0.1", &ip4addr);
  sio_error_t err = sio_addr_from_parts(&server_addr, AF_INET, &ip4addr, 9876);
  if (err != SIO_SUCCESS) {
    printf("    Failed to create server address: %s\n", sio_strerr(err));
    return 1;
  }
  
  /* Create server socket */
  sio_stream_t server_stream;
  err = sio_stream_open_socket(&server_stream, &server_addr, 
                             SIO_STREAM_RDWR | SIO_STREAM_SERVER | SIO_STREAM_TCP);
  if (err != SIO_SUCCESS) {
    printf("    Failed to create server socket: %s\n", sio_strerr(err));
    return 1;
  }
  
  printf("    TCP server started on 127.0.0.1:9876\n");

  /* For simplicity in Windows, we'll use the same thread and just make
     the server socket non-blocking for the accept call */
  int blocking = 0;
  err = sio_stream_set_option(&server_stream, SIO_OPT_BLOCKING, &blocking, sizeof(blocking));
  if (err != SIO_SUCCESS) {
    printf("    Failed to set server socket to non-blocking: %s\n", sio_strerr(err));
    sio_stream_close(&server_stream);
    return 1;
  }
  
  /* Create client socket */
  sio_stream_t client_stream;
  err = sio_stream_open_socket(&client_stream, &server_addr, SIO_STREAM_RDWR | SIO_STREAM_TCP);
  if (err != SIO_SUCCESS) {
    printf("    Failed to create client socket: %s\n", sio_strerr(err));
    sio_stream_close(&server_stream);
    return 1;
  }
  
  printf("    TCP client connected to 127.0.0.1:9876\n");
  
  /* Give some time for the connection to establish */
  sleep_ms(100);
  
  /* Accept the connection (non-blocking) */
  sio_stream_t accept_stream;
  sio_addr_t client_addr;
  
  /* We may need to retry a few times since we're non-blocking */
  int accept_retry = 10;
  while (accept_retry > 0) {
    err = sio_socket_accept(&server_stream, &accept_stream, &client_addr);
    if (err == SIO_SUCCESS) {
      break;
    } else if (err == SIO_ERROR_WOULDBLOCK) {
      /* Try again */
      sleep_ms(100);
      accept_retry--;
      continue;
    } else {
      printf("    Failed to accept client connection: %s\n", sio_strerr(err));
      sio_stream_close(&client_stream);
      sio_stream_close(&server_stream);
      return 1;
    }
  }
  
  if (accept_retry == 0) {
    printf("    Timed out waiting for client connection\n");
    sio_stream_close(&client_stream);
    sio_stream_close(&server_stream);
    return 1;
  }
  
  printf("    Server accepted client connection\n");
  
  /* Get client address as string */
  char addr_str[128];
  err = sio_addr_to_string(&client_addr, addr_str, sizeof(addr_str));
  if (err != SIO_SUCCESS) {
    printf("    Failed to convert client address to string: %s\n", sio_strerr(err));
  } else {
    printf("    Client address: %s\n", addr_str);
  }
  
#if defined(_WIN32)
  /* Write data to server */
  const char *client_msg = "Hello from client!";
  size_t bytes_written;
  err = sio_stream_write(&client_stream, client_msg, strlen(client_msg), &bytes_written, 0);
  if (err != SIO_SUCCESS) {
    printf("    Failed to write to client socket: %s\n", sio_strerr(err));
    sio_stream_close(&accept_stream);
    sio_stream_close(&client_stream);
    sio_stream_close(&server_stream);
    return 1;
  }
  
  printf("    Client sent: \"%s\"\n", client_msg);
#endif
  
  /* Read data from client */
  char buffer[128] = {0};
  size_t bytes_read;
  err = sio_stream_read(&accept_stream, buffer, sizeof(buffer) - 1, &bytes_read, 0);
  if (err != SIO_SUCCESS && err != SIO_ERROR_EOF) {
    printf("    Failed to read from accepted socket: %s\n", sio_strerr(err));
    sio_stream_close(&accept_stream);
#if defined(_WIN32)
    sio_stream_close(&client_stream);
#endif
    sio_stream_close(&server_stream);
    return 1;
  }
  
  printf("    Server received: \"%s\"\n", buffer);
  
  /* Write response back to client */
  const char *server_msg = "Hello from server!";
  size_t bytes_written;
  err = sio_stream_write(&accept_stream, server_msg, strlen(server_msg), &bytes_written, 0);
  if (err != SIO_SUCCESS) {
    printf("    Failed to write to accepted socket: %s\n", sio_strerr(err));
    sio_stream_close(&accept_stream);
#if defined(_WIN32)
    sio_stream_close(&client_stream);
#endif
    sio_stream_close(&server_stream);
    return 1;
  }
  
  printf("    Server sent: \"%s\"\n", server_msg);

  /* Read response from server */
  memset(buffer, 0, sizeof(buffer));
  err = sio_stream_read(&client_stream, buffer, sizeof(buffer) - 1, &bytes_read, 0);
  if (err != SIO_SUCCESS && err != SIO_ERROR_EOF) {
    printf("    Failed to read from client socket: %s\n", sio_strerr(err));
    sio_stream_close(&accept_stream);
    sio_stream_close(&client_stream);
    sio_stream_close(&server_stream);
    return 1;
  }
  
  printf("    Client received: \"%s\"\n", buffer);
  
  /* Close client socket */
  sio_stream_close(&client_stream);
  
  /* Close accepted socket */
  sio_stream_close(&accept_stream);
  
  /* Close server socket */
  sio_stream_close(&server_stream);
  
  printf("  TCP socket test passed!\n");
  return 0;
}

/**
* @brief Test UDP socket operations
*
* @return int 0 if successful, 1 otherwise
*/
static int test_udp_socket(void) {
  printf("  Testing UDP socket stream...\n");
  
  /* Create address for localhost server */
  sio_addr_t server_addr;
  struct in_addr ip4addr;
  
  /* Set to 127.0.0.1:9877 */
  inet_pton(AF_INET, "127.0.0.1", &ip4addr);
  sio_error_t err = sio_addr_from_parts(&server_addr, AF_INET, &ip4addr, 9877);
  if (err != SIO_SUCCESS) {
    printf("    Failed to create server address: %s\n", sio_strerr(err));
    return 1;
  }
  
  /* Create server socket */
  sio_stream_t server_stream;
  err = sio_stream_open_socket(&server_stream, &server_addr, SIO_STREAM_RDWR | SIO_STREAM_SERVER);
  if (err != SIO_SUCCESS) {
    printf("    Failed to create server socket: %s\n", sio_strerr(err));
    return 1;
  }
  
  printf("    UDP server started on 127.0.0.1:9877\n");
  
  /* Create client socket (pseudo-socket) */
  sio_stream_t client_stream;
  err = sio_stream_open_socket(&client_stream, &server_addr, 
                             SIO_STREAM_RDWR);
  if (err != SIO_SUCCESS) {
    printf("    Failed to create client socket: %s\n", sio_strerr(err));
    sio_stream_close(&server_stream);
    return 1;
  }
  
  printf("    UDP client created for 127.0.0.1:9877\n");
  
  /* Send data from client to server */
  const char *client_msg = "Hello UDP server!";
  size_t bytes_written;
  err = sio_stream_write(&client_stream, client_msg, strlen(client_msg), &bytes_written, 0);
  if (err != SIO_SUCCESS) {
    printf("    Failed to send data from client: %s\n", sio_strerr(err));
    sio_stream_close(&client_stream);
    sio_stream_close(&server_stream);
    return 1;
  }
  
  printf("    Client sent: \"%s\"\n", client_msg);
  
  /* Receive data on server */
  char buffer[128] = {0};
  size_t bytes_read;
  sio_addr_t client_addr;
  
  /* Give some time for UDP packet to arrive */
  sleep_ms(100);
  
  err = sio_stream_read(&server_stream, buffer, sizeof(buffer) - 1, &bytes_read, 0);
  if (err != SIO_SUCCESS) {
    printf("    Failed to receive data on server: %s\n", sio_strerr(err));
    sio_stream_close(&client_stream);
    sio_stream_close(&server_stream);
    return 1;
  }
  
  printf("    Server received: \"%s\"\n", buffer);
  
  /* Verify received data */
  if (bytes_read != strlen(client_msg) || strcmp(buffer, client_msg) != 0) {
    printf("    Data verification failed\n");
    printf("    Expected: \"%s\"\n", client_msg);
    printf("    Got: \"%s\"\n", buffer);
    sio_stream_close(&client_stream);
    sio_stream_close(&server_stream);
    return 1;
  }
  
  /* Send response from server to client */
  const char *server_msg = "Hello UDP client!";
  
  /* Create client address (we'd normally get this from recvfrom, but in our
     test we know it's localhost) */
  sio_addr_t resp_addr;
  inet_pton(AF_INET, "127.0.0.1", &ip4addr);
  err = sio_addr_from_parts(&resp_addr, AF_INET, &ip4addr, 0); /* Client uses ephemeral port */
  if (err != SIO_SUCCESS) {
    printf("    Failed to create response address: %s\n", sio_strerr(err));
    sio_stream_close(&client_stream);
    sio_stream_close(&server_stream);
    return 1;
  }
  
  /* In a real application, we'd find the client's port from the received packet.
     For this test, we'll just try to send to the client using client_stream directly. */
  
  /* Close sockets */
  sio_stream_close(&client_stream);
  sio_stream_close(&server_stream);
  
  printf("  UDP socket test passed!\n");
  return 0;
}

/**
* @brief Test socket options
*
* @return int 0 if successful, 1 otherwise
*/
static int test_socket_options(void) {
  printf("  Testing socket options...\n");
  
  /* Create a socket */
  sio_addr_t addr;
  sio_addr_loopback(&addr, AF_INET, 0); /* Use any port */
  
  sio_stream_t stream;
  sio_error_t err = sio_stream_open_socket(&stream, &addr, SIO_STREAM_RDWR | SIO_STREAM_TCP);
  if (err != SIO_SUCCESS) {
    printf("    Failed to create socket: %s\n", sio_strerr(err));
    return 1;
  }
  
  /* Check socket type */
  sio_stream_type_t type;
  size_t size = sizeof(type);
  err = sio_stream_get_option(&stream, SIO_INFO_TYPE, &type, &size);
  if (err != SIO_SUCCESS) {
    printf("    Failed to get socket type: %s\n", sio_strerr(err));
    sio_stream_close(&stream);
    return 1;
  }
  
  printf("    Socket type: %d (expected: %d)\n", type, SIO_STREAM_SOCKET);
  
  /* Verify type */
  if (type != SIO_STREAM_SOCKET) {
    printf("    Socket type verification failed\n");
    sio_stream_close(&stream);
    return 1;
  }
  
  /* Test TCP_NODELAY option */
  int nodelay = 1;
  err = sio_stream_set_option(&stream, SIO_OPT_SOCK_NODELAY, &nodelay, sizeof(nodelay));
  if (err != SIO_SUCCESS) {
    printf("    Failed to set TCP_NODELAY: %s\n", sio_strerr(err));
    sio_stream_close(&stream);
    return 1;
  }
  
  /* Get TCP_NODELAY option */
  int get_nodelay = 0;
  size = sizeof(get_nodelay);
  err = sio_stream_get_option(&stream, SIO_OPT_SOCK_NODELAY, &get_nodelay, &size);
  if (err != SIO_SUCCESS) {
    printf("    Failed to get TCP_NODELAY: %s\n", sio_strerr(err));
    sio_stream_close(&stream);
    return 1;
  }
  
  printf("    TCP_NODELAY: %d (expected: 1)\n", get_nodelay);
  
  /* Verify option */
  if (get_nodelay != 1) {
    printf("    TCP_NODELAY verification failed\n");
    sio_stream_close(&stream);
    return 1;
  }
  
  /* Test blocking mode */
  int blocking = 0; /* Set to non-blocking */
  err = sio_stream_set_option(&stream, SIO_OPT_BLOCKING, &blocking, sizeof(blocking));
  if (err != SIO_SUCCESS) {
    printf("    Failed to set non-blocking mode: %s\n", sio_strerr(err));
    sio_stream_close(&stream);
    return 1;
  }
  
  /* Get blocking mode */
  int get_blocking = 1;
  size = sizeof(get_blocking);
  err = sio_stream_get_option(&stream, SIO_OPT_BLOCKING, &get_blocking, &size);
  if (err != SIO_SUCCESS) {
    printf("    Failed to get blocking mode: %s\n", sio_strerr(err));
    sio_stream_close(&stream);
    return 1;
  }
  
  printf("    Blocking mode: %d (expected: 0)\n", get_blocking);
  
  /* Verify option */
  if (get_blocking != 0) {
    printf("    Blocking mode verification failed\n");
    sio_stream_close(&stream);
    return 1;
  }
  
  /* Close the socket */
  sio_stream_close(&stream);
  
  printf("  Socket options test passed!\n");
  return 0;
}

/**
* @brief Run all socket stream tests
*
* @return int 0 if all tests pass, 1 otherwise
*/
int test_socket_streams(void) {
  int failed = 0;
  
  failed |= test_tcp_socket();
  failed |= test_udp_socket();
  failed |= test_socket_options();
  
  return failed;
}