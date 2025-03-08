/**
* @file tests/timer_stream_test.c
* @brief Test cases for SIO timer streams
*
* This file contains test functions for timer stream implementation.
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
* @brief Test one-shot timer operations
*
* @return int 0 if successful, 1 otherwise
*/
static int test_oneshot_timer(void) {
  printf("  Testing one-shot timer...\n");
  
  /* Create a one-shot timer with 200ms duration */
  sio_stream_t timer;
  sio_error_t err = sio_stream_open_timer(&timer, 200, 1, SIO_STREAM_READ);
  if (err != SIO_SUCCESS) {
    printf("    Failed to create one-shot timer: %s\n", sio_strerr(err));
    return 1;
  }
  
  printf("    One-shot timer created with 200ms duration\n");
  
  /* Check timer type */
  sio_stream_type_t type;
  size_t size = sizeof(type);
  err = sio_stream_get_option(&timer, SIO_INFO_TYPE, &type, &size);
  if (err != SIO_SUCCESS) {
    printf("    Failed to get timer type: %s\n", sio_strerr(err));
    sio_stream_close(&timer);
    return 1;
  }
  
  printf("    Timer type: %d (expected: %d)\n", type, SIO_STREAM_TIMER);
  
  /* Verify timer type */
  if (type != SIO_STREAM_TIMER) {
    printf("    Timer type verification failed\n");
    sio_stream_close(&timer);
    return 1;
  }
  
  /* Try to read from the timer without waiting (should fail with WOULDBLOCK) */
  uint64_t expirations;
  size_t bytes_read;
  err = sio_stream_read(&timer, &expirations, sizeof(expirations), &bytes_read, SIO_MSG_DONTWAIT);
  if (err != SIO_ERROR_WOULDBLOCK) {
    printf("    Unexpected result from non-blocking timer read: %s\n", sio_strerr(err));
    sio_stream_close(&timer);
    return 1;
  }
  
  printf("    Non-blocking read correctly returned WOULDBLOCK\n");
  
  /* Wait for timer to expire */
  printf("    Waiting for timer to expire...\n");
  
  /* Try reading with a blocking call */
  err = sio_stream_read(&timer, &expirations, sizeof(expirations), &bytes_read, 0);
  if (err != SIO_SUCCESS) {
    printf("    Failed to read from timer: %s\n", sio_strerr(err));
    sio_stream_close(&timer);
    return 1;
  }
  
  printf("    Timer expired after blocking read, expirations: %llu\n", 
         (unsigned long long)expirations);
  
  /* Try to read again (should fail since it's a one-shot timer) */
  err = sio_stream_read(&timer, &expirations, sizeof(expirations), &bytes_read, SIO_MSG_DONTWAIT);
  if (err != SIO_ERROR_WOULDBLOCK) {
    printf("    Unexpected result from second timer read: %s\n", sio_strerr(err));
    sio_stream_close(&timer);
    return 1;
  }
  
  printf("    One-shot timer did not expire a second time\n");
  
  /* Close the timer */
  err = sio_stream_close(&timer);
  if (err != SIO_SUCCESS) {
    printf("    Failed to close timer: %s\n", sio_strerr(err));
    return 1;
  }
  
  printf("  One-shot timer test passed!\n");
  return 0;
}

/**
* @brief Test periodic timer operations
*
* @return int 0 if successful, 1 otherwise
*/
static int test_periodic_timer(void) {
  printf("  Testing periodic timer...\n");
  
  /* Create a periodic timer with 100ms interval */
  sio_stream_t timer;
  sio_error_t err = sio_stream_open_timer(&timer, 100, 0, SIO_STREAM_RDWR);
  if (err != SIO_SUCCESS) {
    printf("    Failed to create periodic timer: %s\n", sio_strerr(err));
    return 1;
  }
  
  printf("    Periodic timer created with 100ms interval\n");
  
  /* Read up to 3 expirations */
  for (int i = 0; i < 3; i++) {
    uint64_t expirations;
    size_t bytes_read;
    
    err = sio_stream_read(&timer, &expirations, sizeof(expirations), &bytes_read, 0);
    if (err != SIO_SUCCESS) {
      printf("    Failed to read expiration %d: %s\n", i + 1, sio_strerr(err));
      sio_stream_close(&timer);
      return 1;
    }
    
    printf("    Timer expiration %d, count: %llu\n", i + 1, (unsigned long long)expirations);
  }
  
  /* Check the timer options */
  int32_t interval;
  size_t size = sizeof(interval);
  err = sio_stream_get_option(&timer, SIO_OPT_TIMER_INTERVAL, &interval, &size);
  if (err != SIO_SUCCESS) {
    printf("    Failed to get timer interval: %s\n", sio_strerr(err));
    sio_stream_close(&timer);
    return 1;
  }
  
  printf("    Timer interval: %d ms (expected: 100)\n", interval);
  
  /* Verify interval */
  if (interval != 100) {
    printf("    Timer interval verification failed\n");
    sio_stream_close(&timer);
    return 1;
  }
  
  /* Check one-shot flag */
  int oneshot;
  size = sizeof(oneshot);
  err = sio_stream_get_option(&timer, SIO_OPT_TIMER_ONESHOT, &oneshot, &size);
  if (err != SIO_SUCCESS) {
    printf("    Failed to get one-shot flag: %s\n", sio_strerr(err));
    sio_stream_close(&timer);
    return 1;
  }
  
  printf("    Timer one-shot flag: %d (expected: 0)\n", oneshot);
  
  /* Verify one-shot flag */
  if (oneshot != 0) {
    printf("    Timer one-shot flag verification failed\n");
    sio_stream_close(&timer);
    return 1;
  }
  
  /* Change timer interval by writing to it */
  uint64_t new_interval = 500; /* 500ms */
  size_t bytes_written;
  
  err = sio_stream_write(&timer, &new_interval, sizeof(new_interval), &bytes_written, 0);
  if (err != SIO_SUCCESS) {
    printf("    Failed to change timer interval: %s\n", sio_strerr(err));
    sio_stream_close(&timer);
    return 1;
  }
  
  printf("    Changed timer interval to 500ms\n");
  
  /* Verify the new interval took effect by waiting and reading */
  sleep_ms(250); /* Wait more than the original interval but less than the new one */
  
  uint64_t expirations;
  size_t bytes_read;
  err = sio_stream_read(&timer, &expirations, sizeof(expirations), &bytes_read, SIO_MSG_DONTWAIT);
  if (err != SIO_ERROR_WOULDBLOCK) {
    printf("    Timer expired too soon after interval change: %s\n", sio_strerr(err));
    sio_stream_close(&timer);
    return 1;
  }
  
  printf("    Timer correctly did not expire after 250ms with 500ms interval\n");
  
  /* Close the timer */
  err = sio_stream_close(&timer);
  if (err != SIO_SUCCESS) {
    printf("    Failed to close timer: %s\n", sio_strerr(err));
    return 1;
  }
  
  printf("  Periodic timer test passed!\n");
  return 0;
}

/**
* @brief Test changing timer options
*
* @return int 0 if successful, 1 otherwise
*/
static int test_timer_options(void) {
  printf("  Testing timer options...\n");
  
  /* Create a timer */
  sio_stream_t timer;
  sio_error_t err = sio_stream_open_timer(&timer, 200, 0, SIO_STREAM_RDWR);
  if (err != SIO_SUCCESS) {
    printf("    Failed to create timer: %s\n", sio_strerr(err));
    return 1;
  }
  
  /* Change to one-shot mode */
  int oneshot = 1;
  err = sio_stream_set_option(&timer, SIO_OPT_TIMER_ONESHOT, &oneshot, sizeof(oneshot));
  if (err != SIO_SUCCESS) {
    printf("    Failed to set one-shot mode: %s\n", sio_strerr(err));
    sio_stream_close(&timer);
    return 1;
  }
  
  printf("    Changed timer to one-shot mode\n");
  
  /* Verify the change */
  int get_oneshot = 0;
  size_t size = sizeof(get_oneshot);
  err = sio_stream_get_option(&timer, SIO_OPT_TIMER_ONESHOT, &get_oneshot, &size);
  if (err != SIO_SUCCESS) {
    printf("    Failed to get one-shot flag: %s\n", sio_strerr(err));
    sio_stream_close(&timer);
    return 1;
  }
  
  printf("    Timer one-shot flag: %d (expected: 1)\n", get_oneshot);
  
  /* Verify one-shot flag */
  if (get_oneshot != 1) {
    printf("    Timer one-shot flag verification failed\n");
    sio_stream_close(&timer);
    return 1;
  }
  
  /* Change the interval */
  int32_t interval = 300;
  err = sio_stream_set_option(&timer, SIO_OPT_TIMER_INTERVAL, &interval, sizeof(interval));
  if (err != SIO_SUCCESS) {
    printf("    Failed to set timer interval: %s\n", sio_strerr(err));
    sio_stream_close(&timer);
    return 1;
  }
  
  printf("    Changed timer interval to 300ms\n");
  
  /* Verify the change */
  int32_t get_interval = 0;
  size = sizeof(get_interval);
  err = sio_stream_get_option(&timer, SIO_OPT_TIMER_INTERVAL, &get_interval, &size);
  if (err != SIO_SUCCESS) {
    printf("    Failed to get timer interval: %s\n", sio_strerr(err));
    sio_stream_close(&timer);
    return 1;
  }
  
  printf("    Timer interval: %d ms (expected: 300)\n", get_interval);
  
  /* Verify interval */
  if (get_interval != 300) {
    printf("    Timer interval verification failed\n");
    sio_stream_close(&timer);
    return 1;
  }
  
  /* Close the timer */
  err = sio_stream_close(&timer);
  if (err != SIO_SUCCESS) {
    printf("    Failed to close timer: %s\n", sio_strerr(err));
    return 1;
  }
  
  printf("  Timer options test passed!\n");
  return 0;
}

/**
* @brief Run all timer stream tests
*
* @return int 0 if all tests pass, 1 otherwise
*/
int test_timer_streams(void) {
  int failed = 0;
  
  failed |= test_oneshot_timer();
  failed |= test_periodic_timer();
  failed |= test_timer_options();
  
  return failed;
}