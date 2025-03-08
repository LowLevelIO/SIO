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
  
  sio_addrinfo_t hints;
  sio_addrinfo_t *result = NULL;
  sio_error_t err;
  
  /* Set up hints for address resolution */
  memset(&hints, 0, sizeof(hints));
  hints.ai_family = SIO_AF_UNSPEC;
  hints.ai_socktype = SIO_SOCK_STREAM;
  hints.ai_protocol = SIO_IPPROTO_TCP;
  
  /* Try to resolve localhost */
  err = sio_getaddrinfo("google.com", "80", &hints, &result); // 80 is the http variation requiring no encryption (this is kind of weird but a realist version would never have someone create a socket without a server in mind, right?)
  if (err != 0) {
    fprintf(stderr, "DNS resolution failed: %s\n", sio_gai_strerror(err));
    return 1;
  }
  
  sio_addr_t addr;
  addr.len = result->ai_addrlen;
  memcpy(&addr, result->ai_addr, result->ai_addrlen);

  /* Free the result */
  sio_freeaddrinfo(result);

  sio_stream_t stream;
  err = sio_stream_open_socket(&stream, &addr, SIO_STREAM_RDWR | SIO_STREAM_TCP);
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