/**
* @file tests/stream_test.c
* @brief Test program for the SIO stream system
*
* This program tests various stream implementations in the SIO library.
*
* @author zczxy
* @version 0.1.0
*/

#include <sio.h>
#include <sio/stream.h>
#include <sio/err.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

/* Function declarations for test suites */
int test_file_streams(void);
int test_memory_streams(void);
int test_socket_streams(void);
int test_timer_streams(void);
int test_signal_streams(void);

/**
* @brief Report an error and exit
*
* @param error_code The SIO error code
* @param message Custom error message
*/
void report_error_and_exit(sio_error_t error_code, const char *message) {
  fprintf(stderr, "Error: %s: %s\n", message, sio_strerr(error_code));
  exit(EXIT_FAILURE);
}

/**
* @brief Main test function
* 
* @return int 0 if all tests pass, 1 otherwise
*/
int main(void) {
  int failed = 0;
  
  printf("===== SIO Stream System Tests =====\n\n");
  
  /* Initialize SIO library */
  sio_error_t err = sio_initialize(SIO_INITALIZE_RAW_SOCK);
  if (err != SIO_SUCCESS) {
    report_error_and_exit(err, "Failed to initialize SIO library");
  }
  
  /* Run test suites */
  printf("Running file stream tests...\n");
  failed |= test_file_streams();
  
  printf("\nRunning memory stream tests...\n");
  failed |= test_memory_streams();
  
  printf("\nRunning socket stream tests...\n");
  failed |= test_socket_streams();
  
  printf("\nRunning timer stream tests...\n");
  failed |= test_timer_streams();
  
  printf("\nRunning signal stream tests...\n");
  failed |= test_signal_streams();
  
  /* Clean up SIO library */
  sio_cleanup();
  
  /* Report results */
  if (failed) {
    printf("\nSome tests failed!\n");
    return 1;
  }
  
  printf("\nAll tests passed successfully!\n");
  return 0;
}