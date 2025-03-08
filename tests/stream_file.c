/**
* @file tests/file_stream_test.c
* @brief Test cases for SIO file streams
*
* This file contains test functions for the file stream implementation.
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

/* Forward declaration for the error reporting function */
void report_error_and_exit(sio_error_t error_code, const char *message);

/**
* @brief Test basic file operations (create, write, read, seek)
*
* @return int 0 if successful, 1 otherwise
*/
static int test_file_basic_operations(void) {
  printf("  Testing basic file operations...\n");
  
  const char *test_filename = "test_file.dat";
  const char *test_data = "Hello, SIO File Stream!";
  const size_t test_data_len = strlen(test_data);
  
  /* Create a file stream for writing */
  sio_stream_t write_stream;
  sio_error_t err = sio_stream_open_file(&write_stream, test_filename, 
                                     SIO_STREAM_WRITE | SIO_STREAM_CREATE | SIO_STREAM_TRUNC, 0644);
  if (err != SIO_SUCCESS) {
    printf("    Failed to open file for writing: %s\n", sio_strerr(err));
    return 1;
  }
  
  /* Write data to the file */
  size_t bytes_written;
  err = sio_stream_write(&write_stream, test_data, test_data_len, &bytes_written, 0);
  if (err != SIO_SUCCESS || bytes_written != test_data_len) {
    printf("    Failed to write to file: %s\n", sio_strerr(err));
    sio_stream_close(&write_stream);
    return 1;
  }
  
  printf("    Wrote %zu bytes to file\n", bytes_written);
  
  /* Close the file */
  err = sio_stream_close(&write_stream);
  if (err != SIO_SUCCESS) {
    printf("    Failed to close file after writing: %s\n", sio_strerr(err));
    return 1;
  }
  
  /* Open the file for reading */
  sio_stream_t read_stream;
  err = sio_stream_open_file(&read_stream, test_filename, SIO_STREAM_READ, 0);
  if (err != SIO_SUCCESS) {
    printf("    Failed to open file for reading: %s\n", sio_strerr(err));
    return 1;
  }
  
  /* Read the data */
  char buffer[128] = {0};
  size_t bytes_read;
  err = sio_stream_read(&read_stream, buffer, sizeof(buffer) - 1, &bytes_read, 0);
  if (err != SIO_SUCCESS && err != SIO_ERROR_EOF) {
    printf("    Failed to read from file: %s\n", sio_strerr(err));
    sio_stream_close(&read_stream);
    return 1;
  }
  
  printf("    Read %zu bytes from file: \"%s\"\n", bytes_read, buffer);
  
  /* Verify the data */
  if (bytes_read != test_data_len || strcmp(buffer, test_data) != 0) {
    printf("    Data verification failed\n");
    printf("    Expected: \"%s\"\n", test_data);
    printf("    Got: \"%s\"\n", buffer);
    sio_stream_close(&read_stream);
    return 1;
  }
  
  /* Test seeking */
  err = sio_stream_seek(&read_stream, 0, SIO_SEEK_SET, NULL);
  if (err != SIO_SUCCESS) {
    printf("    Failed to seek to beginning of file: %s\n", sio_strerr(err));
    sio_stream_close(&read_stream);
    return 1;
  }
  
  /* Read first 5 bytes */
  memset(buffer, 0, sizeof(buffer));
  err = sio_stream_read(&read_stream, buffer, 5, &bytes_read, 0);
  if (err != SIO_SUCCESS || bytes_read != 5) {
    printf("    Failed to read 5 bytes after seek: %s\n", sio_strerr(err));
    sio_stream_close(&read_stream);
    return 1;
  }
  
  buffer[5] = '\0';
  printf("    Read 5 bytes after seek: \"%s\"\n", buffer);
  
  /* Verify first 5 bytes */
  if (strncmp(buffer, test_data, 5) != 0) {
    printf("    First 5 bytes verification failed\n");
    sio_stream_close(&read_stream);
    return 1;
  }
  
  /* Get current position */
  uint64_t position;
  err = sio_stream_tell(&read_stream, &position);
  if (err != SIO_SUCCESS) {
    printf("    Failed to get current position: %s\n", sio_strerr(err));
    sio_stream_close(&read_stream);
    return 1;
  }
  
  printf("    Current position: %zu\n", (size_t)position);
  
  /* Verify position */
  if (position != 5) {
    printf("    Position verification failed\n");
    sio_stream_close(&read_stream);
    return 1;
  }
  
  /* Close the file */
  err = sio_stream_close(&read_stream);
  if (err != SIO_SUCCESS) {
    printf("    Failed to close file after reading: %s\n", sio_strerr(err));
    return 1;
  }
  
  /* Clean up test file */
  remove(test_filename);
  
  printf("  Basic file operations test passed!\n");
  return 0;
}

/**
* @brief Test file stream options and properties
*
* @return int 0 if successful, 1 otherwise
*/
static int test_file_options(void) {
  printf("  Testing file stream options...\n");
  
  const char *test_filename = "test_file_opts.dat";
  
  /* Create a file stream */
  sio_stream_t stream;
  sio_error_t err = sio_stream_open_file(&stream, test_filename, 
                                     SIO_STREAM_RDWR | SIO_STREAM_CREATE | SIO_STREAM_TRUNC, 0644);
  if (err != SIO_SUCCESS) {
    printf("    Failed to open file: %s\n", sio_strerr(err));
    return 1;
  }
  
  /* Test type info */
  sio_stream_type_t type;
  size_t size = sizeof(type);
  err = sio_stream_get_option(&stream, SIO_INFO_TYPE, &type, &size);
  if (err != SIO_SUCCESS) {
    printf("    Failed to get stream type: %s\n", sio_strerr(err));
    sio_stream_close(&stream);
    remove(test_filename);
    return 1;
  }
  
  printf("    Stream type: %d (expected: %d)\n", type, SIO_STREAM_FILE);
  
  /* Verify type */
  if (type != SIO_STREAM_FILE) {
    printf("    Stream type verification failed\n");
    sio_stream_close(&stream);
    remove(test_filename);
    return 1;
  }
  
  /* Test flags info */
  int flags;
  size = sizeof(flags);
  err = sio_stream_get_option(&stream, SIO_INFO_FLAGS, &flags, &size);
  if (err != SIO_SUCCESS) {
    printf("    Failed to get stream flags: %s\n", sio_strerr(err));
    sio_stream_close(&stream);
    remove(test_filename);
    return 1;
  }
  
  printf("    Stream flags: 0x%x\n", flags);
  
  /* Check readable flag */
  int readable;
  size = sizeof(readable);
  err = sio_stream_get_option(&stream, SIO_INFO_READABLE, &readable, &size);
  if (err != SIO_SUCCESS) {
    printf("    Failed to get readable flag: %s\n", sio_strerr(err));
    sio_stream_close(&stream);
    remove(test_filename);
    return 1;
  }
  
  printf("    Stream readable: %d (expected: 1)\n", readable);
  
  /* Verify readable */
  if (!readable) {
    printf("    Readable flag verification failed\n");
    sio_stream_close(&stream);
    remove(test_filename);
    return 1;
  }
  
  /* Check writable flag */
  int writable;
  size = sizeof(writable);
  err = sio_stream_get_option(&stream, SIO_INFO_WRITABLE, &writable, &size);
  if (err != SIO_SUCCESS) {
    printf("    Failed to get writable flag: %s\n", sio_strerr(err));
    sio_stream_close(&stream);
    remove(test_filename);
    return 1;
  }
  
  printf("    Stream writable: %d (expected: 1)\n", writable);
  
  /* Verify writable */
  if (!writable) {
    printf("    Writable flag verification failed\n");
    sio_stream_close(&stream);
    remove(test_filename);
    return 1;
  }
  
  /* Check seekable flag */
  int seekable;
  size = sizeof(seekable);
  err = sio_stream_get_option(&stream, SIO_INFO_SEEKABLE, &seekable, &size);
  if (err != SIO_SUCCESS) {
    printf("    Failed to get seekable flag: %s\n", sio_strerr(err));
    sio_stream_close(&stream);
    remove(test_filename);
    return 1;
  }
  
  printf("    Stream seekable: %d (expected: 1)\n", seekable);
  
  /* Verify seekable */
  if (!seekable) {
    printf("    Seekable flag verification failed\n");
    sio_stream_close(&stream);
    remove(test_filename);
    return 1;
  }
  
  /* Write some data for size testing */
  const char *test_data = "SIO File Stream Options Test";
  size_t bytes_written;
  err = sio_stream_write(&stream, test_data, strlen(test_data), &bytes_written, 0);
  if (err != SIO_SUCCESS) {
    printf("    Failed to write test data: %s\n", sio_strerr(err));
    sio_stream_close(&stream);
    remove(test_filename);
    return 1;
  }
  
  /* Get stream size */
  uint64_t stream_size;
  size = sizeof(stream_size);
  err = sio_stream_get_option(&stream, SIO_INFO_SIZE, &stream_size, &size);
  if (err != SIO_SUCCESS) {
    printf("    Failed to get stream size: %s\n", sio_strerr(err));
    sio_stream_close(&stream);
    remove(test_filename);
    return 1;
  }
  
  printf("    Stream size: %zu (expected: %zu)\n", (size_t)stream_size, strlen(test_data));
  
  /* Verify size */
  if (stream_size != strlen(test_data)) {
    printf("    Stream size verification failed\n");
    sio_stream_close(&stream);
    remove(test_filename);
    return 1;
  }
  
  /* Close the file */
  err = sio_stream_close(&stream);
  if (err != SIO_SUCCESS) {
    printf("    Failed to close file: %s\n", sio_strerr(err));
    remove(test_filename);
    return 1;
  }
  
  /* Clean up test file */
  remove(test_filename);
  
  printf("  File stream options test passed!\n");
  return 0;
}

/**
* @brief Test file locking operations
*
* @return int 0 if successful, 1 otherwise
*/
static int test_file_locking(void) {
  printf("  Testing file locking...\n");
  
  const char *test_filename = "test_file_lock.dat";
  
  /* Create a file stream */
  sio_stream_t stream;
  sio_error_t err = sio_stream_open_file(&stream, test_filename, 
                                     SIO_STREAM_RDWR | SIO_STREAM_CREATE | SIO_STREAM_TRUNC, 0644);
  if (err != SIO_SUCCESS) {
    printf("    Failed to open file: %s\n", sio_strerr(err));
    return 1;
  }
  
  /* Write some data */
  const char *test_data = "SIO File Stream Locking Test";
  size_t bytes_written;
  err = sio_stream_write(&stream, test_data, strlen(test_data), &bytes_written, 0);
  if (err != SIO_SUCCESS) {
    printf("    Failed to write test data: %s\n", sio_strerr(err));
    sio_stream_close(&stream);
    remove(test_filename);
    return 1;
  }
  
  /* Apply an exclusive lock to the entire file */
  err = sio_file_lock(&stream, 0, 0, 1, 1);
  if (err != SIO_SUCCESS) {
    printf("    Failed to lock file: %s\n", sio_strerr(err));
    sio_stream_close(&stream);
    remove(test_filename);
    return 1;
  }
  
  printf("    File exclusively locked\n");
  
  /* Unlock the file */
  err = sio_file_unlock(&stream, 0, 0);
  if (err != SIO_SUCCESS) {
    printf("    Failed to unlock file: %s\n", sio_strerr(err));
    sio_stream_close(&stream);
    remove(test_filename);
    return 1;
  }
  
  printf("    File unlocked\n");
  
  /* Apply a shared lock to a region */
  err = sio_file_lock(&stream, 0, 10, 0, 1);
  if (err != SIO_SUCCESS) {
    printf("    Failed to apply shared lock: %s\n", sio_strerr(err));
    sio_stream_close(&stream);
    remove(test_filename);
    return 1;
  }
  
  printf("    File region shared locked\n");
  
  /* Unlock the region */
  err = sio_file_unlock(&stream, 0, 10);
  if (err != SIO_SUCCESS) {
    printf("    Failed to unlock file region: %s\n", sio_strerr(err));
    sio_stream_close(&stream);
    remove(test_filename);
    return 1;
  }
  
  printf("    File region unlocked\n");
  
  /* Close the file */
  err = sio_stream_close(&stream);
  if (err != SIO_SUCCESS) {
    printf("    Failed to close file: %s\n", sio_strerr(err));
    remove(test_filename);
    return 1;
  }
  
  /* Clean up test file */
  remove(test_filename);
  
  printf("  File locking test passed!\n");
  return 0;
}

/**
* @brief Test standard streams (stdin, stdout, stderr)
*
* @return int 0 if successful, 1 otherwise
*/
static int test_standard_streams(void) {
  printf("  Testing standard streams...\n");
  
  sio_stream_t *stdin_stream = NULL;
  sio_stream_t *stdout_stream = NULL;
  sio_stream_t *stderr_stream = NULL;

  /* Initalize standard streams manually for error checking */
  sio_error_t err = sio_stream_stdin(&stdin_stream);
  if (err != SIO_SUCCESS) {
    printf("    Failed to initalize standard input stream: %s\n", sio_strerr(err));
    return 1;
  }
  err = sio_stream_stdout(&stdout_stream);
  if (err != SIO_SUCCESS) {
    printf("    Failed to initalize standard output stream: %s\n", sio_strerr(err));
    return 1;
  }
  err = sio_stream_stderr(&stderr_stream);
  if (err != SIO_SUCCESS) {
    printf("    Failed to initalize standard error stream: %s\n", sio_strerr(err));
    return 1;
  }
  
  /* Check if streams are valid */
  if (!stdin_stream || !stdout_stream || !stderr_stream) {
    printf("    Failed to get standard streams %p, %p, %p\n", (void*)stdin_stream, (void*)stdout_stream, (void*)stderr_stream);
    return 1;
  }
  
  /* Write to stdout */
  const char *stdout_msg = "  This is a test message to stdout via SIO\n";
  size_t bytes_written;
  err = sio_stream_write(stdout_stream, stdout_msg, strlen(stdout_msg), &bytes_written, 0);
  if (err != SIO_SUCCESS) {
    printf("    Failed to write to stdout: %s\n", sio_strerr(err));
    return 1;
  }
  
  /* Write to stderr */
  const char *stderr_msg = "  This is a test message to stderr via SIO\n";
  err = sio_stream_write(stderr_stream, stderr_msg, strlen(stderr_msg), &bytes_written, 0);
  if (err != SIO_SUCCESS) {
    printf("    Failed to write to stderr: %s\n", sio_strerr(err));
    return 1;
  }
  
  /* Check stdin properties */
  int readable;
  size_t size = sizeof(readable);
  err = sio_stream_get_option(stdin_stream, SIO_INFO_READABLE, &readable, &size);
  if (err != SIO_SUCCESS) {
    printf("    Failed to get stdin readable flag: %s\n", sio_strerr(err));
    return 1;
  }
  
  printf("    stdin readable: %d (expected: 1)\n", readable);
  
  /* Check stdin type */
  sio_stream_type_t type;
  size = sizeof(type);
  err = sio_stream_get_option(stdin_stream, SIO_INFO_TYPE, &type, &size);
  if (err != SIO_SUCCESS) {
    printf("    Failed to get stdin type: %s\n", sio_strerr(err));
    return 1;
  }
  
  printf("    stdin type: %d (expected: %d)\n", type, SIO_STREAM_FILE);
  
  printf("  Standard streams test passed!\n");
  return 0;
}

/**
* @brief Run all file stream tests
*
* @return int 0 if all tests pass, 1 otherwise
*/
int test_file_streams(void) {
  int failed = 0;
  
  failed |= test_file_basic_operations();
  failed |= test_file_options();
  failed |= test_file_locking();
  failed |= test_standard_streams();
  
  return failed;
}