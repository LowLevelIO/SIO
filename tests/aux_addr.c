/**
* @file tests/aux_addr.c
* @brief Unit test for SIO address handling functions
* 
* Tests the functionality of the sio/aux/addr.h header file
* 
* @author zczxy
* @version 0.1.0
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

/* Include the header being tested */
#include <sio/aux/addr.h>

/**
* @brief Test conversion between string and binary IP representations
* 
* @return int 0 if successful, 1 otherwise
*/
static int test_inet_conversion(void) {
  char buf[INET6_ADDRSTRLEN + 1];
  struct in_addr addr4;
  struct in6_addr addr6;
  const char *result;
  int ret;
  
  /* Test IPv4 conversion */
  ret = sio_inet_pton(SIO_AF_INET, "192.168.1.1", &addr4);
  if (ret != 1) {
    fprintf(stderr, "Failed to convert IPv4 string to binary\n");
    return 1;
  }
  
  result = sio_inet_ntop(SIO_AF_INET, &addr4, buf, sizeof(buf));
  if (result == NULL) {
    fprintf(stderr, "Failed to convert IPv4 binary to string\n");
    return 1;
  }
  
  if (strcmp(result, "192.168.1.1") != 0) {
    fprintf(stderr, "IPv4 round-trip conversion failed: got %s\n", result);
    return 1;
  }
  
  /* Test IPv6 conversion */
  ret = sio_inet_pton(SIO_AF_INET6, "2001:db8::1", &addr6);
  if (ret != 1) {
    fprintf(stderr, "Failed to convert IPv6 string to binary\n");
    return 1;
  }
  
  result = sio_inet_ntop(SIO_AF_INET6, &addr6, buf, sizeof(buf));
  if (result == NULL) {
    fprintf(stderr, "Failed to convert IPv6 binary to string\n");
    return 1;
  }
  
  if (strcmp(result, "2001:db8::1") != 0) {
    fprintf(stderr, "IPv6 round-trip conversion failed: got %s\n", result);
    return 1;
  }
  
  printf("PASS: inet_conversion\n");
  return 0;
}

/**
* @brief Test socket address creation and manipulation
* 
* @return int 0 if successful, 1 otherwise
*/
static int test_addr_creation(void) {
  sio_addr_t addr;
  struct in_addr ip4addr;
  struct in6_addr ip6addr;
  int af;
  uint16_t port;
  char buf[128];
  sio_error_t err;
  
  /* Test IPv4 address creation */
  sio_inet_pton(SIO_AF_INET, "192.168.1.1", &ip4addr);
  err = sio_addr_from_parts(&addr, SIO_AF_INET, &ip4addr, 8080);
  if (err != 0) {
    fprintf(stderr, "Failed to create IPv4 address from parts\n");
    return 1;
  }
  
  /* Test extraction */
  err = sio_addr_get_parts(&addr, &af, &ip4addr, sizeof(ip4addr), &port);
  if (err != 0) {
    fprintf(stderr, "Failed to extract IPv4 address parts\n");
    return 1;
  }
  
  if (af != SIO_AF_INET || port != 8080) {
    fprintf(stderr, "IPv4 address parts extraction failed\n");
    return 1;
  }
  
  /* Test string conversion */
  err = sio_addr_to_string(&addr, buf, sizeof(buf));
  if (err != 0) {
    fprintf(stderr, "Failed to convert IPv4 address to string\n");
    return 1;
  }
  
  /* Test IPv6 address creation */
  sio_inet_pton(SIO_AF_INET6, "2001:db8::1", &ip6addr);
  err = sio_addr_from_parts(&addr, SIO_AF_INET6, &ip6addr, 8080);
  if (err != 0) {
    fprintf(stderr, "Failed to create IPv6 address from parts\n");
    return 1;
  }
  
  /* Test extraction */
  err = sio_addr_get_parts(&addr, &af, &ip6addr, sizeof(ip6addr), &port);
  if (err != 0) {
    fprintf(stderr, "Failed to extract IPv6 address parts\n");
    return 1;
  }
  
  if (af != SIO_AF_INET6 || port != 8080) {
    fprintf(stderr, "IPv6 address parts extraction failed\n");
    return 1;
  }
  
  printf("PASS: addr_creation\n");
  return 0;
}

/**
* @brief Test address comparison
* 
* @return int 0 if successful, 1 otherwise
*/
static int test_addr_comparison(void) {
  sio_addr_t addr1, addr2;
  struct in_addr ip4addr;
  sio_error_t err;
  
  /* Create two identical IPv4 addresses */
  sio_inet_pton(SIO_AF_INET, "127.0.0.1", &ip4addr);
  err = sio_addr_from_parts(&addr1, SIO_AF_INET, &ip4addr, 8080);
  err = sio_addr_from_parts(&addr2, SIO_AF_INET, &ip4addr, 8080);
  
  /* Test equality */
  if (!sio_addr_cmp(&addr1, &addr2, SIO_ADDR_EQ_FAMILY)) {
    fprintf(stderr, "Address family comparison failed\n");
    return 1;
  }
  
  if (!sio_addr_cmp(&addr1, &addr2, SIO_ADDR_EQ_IP)) {
    fprintf(stderr, "Address IP comparison failed\n");
    return 1;
  }
  
  if (!sio_addr_cmp(&addr1, &addr2, SIO_ADDR_EQ_PORT)) {
    fprintf(stderr, "Address port comparison failed\n");
    return 1;
  }
  
  /* Change port and test inequality */
  err = sio_addr_from_parts(&addr2, SIO_AF_INET, &ip4addr, 9090);
  
  if (sio_addr_cmp(&addr1, &addr2, SIO_ADDR_EQ_PORT)) {
    fprintf(stderr, "Address port inequality comparison failed\n");
    return 1;
  }
  
  printf("PASS: addr_comparison\n");
  return 0;
}

/**
* @brief Test special address creation
* 
* @return int 0 if successful, 1 otherwise
*/
static int test_special_addresses(void) {
  sio_addr_t addr;
  
  /* Test loopback address creation */
  sio_addr_loopback(&addr, SIO_AF_INET, 8080);
  
  if (!sio_addr_is_loopback(&addr)) {
    fprintf(stderr, "IPv4 loopback address check failed\n");
    return 1;
  }
  
  /* Test any address creation */
  sio_addr_any(&addr, SIO_AF_INET, 8080);
  
  if (sio_addr_is_loopback(&addr)) {
    fprintf(stderr, "IPv4 any address check failed\n");
    return 1;
  }
  
  /* Test IPv6 loopback */
  sio_addr_loopback(&addr, SIO_AF_INET6, 8080);
  
  if (!sio_addr_is_loopback(&addr)) {
    fprintf(stderr, "IPv6 loopback address check failed\n");
    return 1;
  }
  
  printf("PASS: special_addresses\n");
  return 0;
}

/**
* @brief Test DNS resolution
* 
* @return int 0 if successful, 1 otherwise
*/
static int test_dns_resolution(void) {
  sio_addrinfo_t hints;
  sio_addrinfo_t *result = NULL;
  sio_error_t err;
  
  /* Set up hints for address resolution */
  memset(&hints, 0, sizeof(hints));
  hints.ai_family = SIO_AF_UNSPEC;
  hints.ai_socktype = SIO_SOCK_STREAM;
  hints.ai_protocol = SIO_IPPROTO_TCP;
  
  /* Try to resolve localhost */
  err = sio_getaddrinfo("localhost", "80", &hints, &result);
  if (err != 0) {
    fprintf(stderr, "DNS resolution failed: %s\n", sio_gai_strerror(err));
    return 1;
  }
  
  /* Free the result */
  sio_freeaddrinfo(result);
  
  printf("PASS: dns_resolution\n");
  return 0;
}

/**
* @brief Main test function
* 
* @return int 0 if all tests pass, 1 otherwise
*/
int main(void) {
  int failed = 0;
  
  printf("Running SIO Address Handling Tests\n");
  printf("----------------------------------\n");
  
  failed |= test_inet_conversion();
  failed |= test_addr_creation();
  failed |= test_addr_comparison();
  failed |= test_special_addresses();
  failed |= test_dns_resolution();
  
  if (failed) {
    printf("\nSome tests failed!\n");
    return 1;
  }
  
  printf("\nAll tests passed successfully!\n");
  return 0;
}