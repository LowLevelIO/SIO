/**
* @file tests/signal_stream_test.c
* @brief Test cases for SIO signal streams
*
* This file contains test functions for signal stream implementation.
*
* @author zczxy
* @version 0.1.0
*/

#include <sio/stream.h>
#include <sio/err.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <signal.h>

#if defined(_WIN32)
  #include <windows.h>
  #define sleep_ms(ms) Sleep(ms)
  /* Windows-specific signal definitions */
  #ifndef SIGALRM
    #define SIGALRM 14
  #endif
#else
  #include <unistd.h>
  #include <sys/time.h>
  #include <errno.h>
  #define sleep_ms(ms) usleep((ms) * 1000)
#endif

/* Forward declaration for the error reporting function */
void report_error_and_exit(sio_error_t error_code, const char *message);

/**
* @brief Test basic signal stream operations
*
* @return int 0 if successful, 1 otherwise
*/
static int test_signal_basic(void) {
  printf("  Testing basic signal stream...\n");
  
  /* Create a signal stream for SIGALRM and SIGINT */
  int signals[] = { SIGALRM, SIGINT };
  
  sio_stream_t stream;
  sio_error_t err = sio_stream_open_signal(&stream, signals, 2, SIO_STREAM_RDWR);
  if (err != SIO_SUCCESS) {
    printf("    Failed to create signal stream: %s\n", sio_strerr(err));
    return 1;
  }
  
  printf("    Signal stream created for SIGALRM and SIGINT\n");
  
  /* Check stream type */
  sio_stream_type_t type;
  size_t size = sizeof(type);
  err = sio_stream_get_option(&stream, SIO_INFO_TYPE, &type, &size);
  if (err != SIO_SUCCESS) {
    printf("    Failed to get stream type: %s\n", sio_strerr(err));
    sio_stream_close(&stream);
    return 1;
  }
  
  printf("    Stream type: %d (expected: %d)\n", type, SIO_STREAM_SIGNAL);
  
  /* Verify type */
  if (type != SIO_STREAM_SIGNAL) {
    printf("    Stream type verification failed\n");
    sio_stream_close(&stream);
    return 1;
  }
  
  /* Try non-blocking read (should fail with WOULDBLOCK) */
  int signo;
  size_t bytes_read;
  err = sio_stream_read(&stream, &signo, sizeof(signo), &bytes_read, SIO_MSG_DONTWAIT);
  if (err != SIO_ERROR_WOULDBLOCK) {
    printf("    Unexpected result from non-blocking read: %s\n", sio_strerr(err));
    sio_stream_close(&stream);
    return 1;
  }
  
  printf("    Non-blocking read correctly returned WOULDBLOCK\n");
  
  /* Send a signal */
  int signal_to_send = SIGALRM;
  size_t bytes_written;
  err = sio_stream_write(&stream, &signal_to_send, sizeof(signal_to_send), &bytes_written, 0);
  if (err != SIO_SUCCESS) {
    printf("    Failed to send signal: %s\n", sio_strerr(err));
    sio_stream_close(&stream);
    return 1;
  }
  
  printf("    Sent %s signal\n", signal_to_send == SIGALRM ? "SIGALRM" : "SIGINT");
  
  /* Allow time for signal delivery */
  sleep_ms(100);
  
  /* Read the signal */
  err = sio_stream_read(&stream, &signo, sizeof(signo), &bytes_read, 0);
  if (err != SIO_SUCCESS) {
    printf("    Failed to read signal: %s\n", sio_strerr(err));
    sio_stream_close(&stream);
    return 1;
  }
  
  printf("    Received signal: %d\n", signo);
  
  /* Verify signal */
  if (signo != signal_to_send) {
    printf("    Signal verification failed\n");
    printf("    Expected: %d, Got: %d\n", signal_to_send, signo);
    sio_stream_close(&stream);
    return 1;
  }
  
  /* Close the stream */
  err = sio_stream_close(&stream);
  if (err != SIO_SUCCESS) {
    printf("    Failed to close signal stream: %s\n", sio_strerr(err));
    return 1;
  }
  
  printf("  Basic signal stream test passed!\n");
  return 0;
}

#if !defined(_WIN32)
/**
* @brief Test signal delivery from another process
*
* This test only runs on POSIX platforms.
*
* @return int 0 if successful, 1 otherwise
*/
static int test_signal_delivery(void) {
  printf("  Testing signal delivery from another process...\n");
  
  /* Create a signal stream for SIGUSR1 */
  int signals[] = { SIGUSR1 };
  
  sio_stream_t stream;
  sio_error_t err = sio_stream_open_signal(&stream, signals, 1, SIO_STREAM_READ);
  if (err != SIO_SUCCESS) {
    printf("    Failed to create signal stream: %s\n", sio_strerr(err));
    return 1;
  }
  
  printf("    Signal stream created for SIGUSR1\n");
  
  /* Fork a child process to send a signal */
  pid_t pid = fork();
  if (pid < 0) {
    printf("    Failed to fork: %s\n", strerror(errno));
    sio_stream_close(&stream);
    return 1;
  }
  
  if (pid == 0) {
    /* Child process */
    sleep_ms(100); /* Give parent time to set up */
    
    /* Send SIGUSR1 to parent */
    kill(getppid(), SIGUSR1);
    
    /* Exit child process */
    exit(0);
  }
  
  /* Parent process */
  printf("    Child process created to send SIGUSR1\n");
  
  /* Read the signal */
  int signo;
  size_t bytes_read;
  err = sio_stream_read(&stream, &signo, sizeof(signo), &bytes_read, 0);
  if (err != SIO_SUCCESS) {
    printf("    Failed to read signal: %s\n", sio_strerr(err));
    sio_stream_close(&stream);
    return 1;
  }
  
  printf("    Received signal: %d (expected: %d)\n", signo, SIGUSR1);
  
  /* Verify signal */
  if (signo != SIGUSR1) {
    printf("    Signal verification failed\n");
    sio_stream_close(&stream);
    return 1;
  }
  
  /* Wait for child to exit */
  int status;
  waitpid(pid, &status, 0);
  
  /* Close the stream */
  err = sio_stream_close(&stream);
  if (err != SIO_SUCCESS) {
    printf("    Failed to close signal stream: %s\n", sio_strerr(err));
    return 1;
  }
  
  printf("  Signal delivery test passed!\n");
  return 0;
}
#endif

/**
* @brief Test signal stream options
*
* @return int 0 if successful, 1 otherwise
*/
static int test_signal_options(void) {
  printf("  Testing signal stream options...\n");
  
  /* Create a signal stream */
  int signals[] = { SIGINT };
  
  sio_stream_t stream;
  sio_error_t err = sio_stream_open_signal(&stream, signals, 1, SIO_STREAM_RDWR);
  if (err != SIO_SUCCESS) {
    printf("    Failed to create signal stream: %s\n", sio_strerr(err));
    return 1;
  }
  
  /* Check readable flag */
  int readable;
  size_t size = sizeof(readable);
  err = sio_stream_get_option(&stream, SIO_INFO_READABLE, &readable, &size);
  if (err != SIO_SUCCESS) {
    printf("    Failed to get readable flag: %s\n", sio_strerr(err));
    sio_stream_close(&stream);
    return 1;
  }
  
  printf("    Stream readable: %d (expected: 1)\n", readable);
  
  /* Verify readable */
  if (!readable) {
    printf("    Readable flag verification failed\n");
    sio_stream_close(&stream);
    return 1;
  }
  
  /* Check writable flag */
  int writable;
  size = sizeof(writable);
  err = sio_stream_get_option(&stream, SIO_INFO_WRITABLE, &writable, &size);
  if (err != SIO_SUCCESS) {
    printf("    Failed to get writable flag: %s\n", sio_strerr(err));
    sio_stream_close(&stream);
    return 1;
  }
  
  printf("    Stream writable: %d (expected: 1)\n", writable);
  
  /* Verify writable */
  if (!writable) {
    printf("    Writable flag verification failed\n");
    sio_stream_close(&stream);
    return 1;
  }
  
  /* Check seekable flag */
  int seekable;
  size = sizeof(seekable);
  err = sio_stream_get_option(&stream, SIO_INFO_SEEKABLE, &seekable, &size);
  if (err != SIO_SUCCESS) {
    printf("    Failed to get seekable flag: %s\n", sio_strerr(err));
    sio_stream_close(&stream);
    return 1;
  }
  
  printf("    Stream seekable: %d (expected: 0)\n", seekable);
  
  /* Verify seekable */
  if (seekable) {
    printf("    Seekable flag verification failed\n");
    sio_stream_close(&stream);
    return 1;
  }
  
  /* Close the stream */
  err = sio_stream_close(&stream);
  if (err != SIO_SUCCESS) {
    printf("    Failed to close signal stream: %s\n", sio_strerr(err));
    return 1;
  }
  
  printf("  Signal stream options test passed!\n");
  return 0;
}

/**
* @brief Run all signal stream tests
*
* @return int 0 if all tests pass, 1 otherwise
*/
int test_signal_streams(void) {
  int failed = 0;
  
  failed |= test_signal_basic();
  
#if !defined(_WIN32)
  /* Test signal delivery from another process (POSIX only) */
  failed |= test_signal_delivery();
#endif
  
  failed |= test_signal_options();
  
  return failed;
}