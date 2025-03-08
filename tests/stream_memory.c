/**
* @file tests/memory_stream_test.c
* @brief Test cases for SIO memory streams
*
* This file contains test functions for both buffer and raw memory stream implementations.
*
* @author zczxy
* @version 0.1.0
*/

#include <sio/stream.h>
#include <sio/buf.h>
#include <sio/err.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

/* Forward declaration for the error reporting function */
void report_error_and_exit(sio_error_t error_code, const char *message);

/**
* @brief Test buffer stream operations
*
* @return int 0 if successful, 1 otherwise
*/
static int test_buffer_stream(void) {
  printf("  Testing buffer stream...\n");
  
  const char *test_data = "Hello, SIO Buffer Stream!";
  const size_t test_data_len = strlen(test_data);
  
  /* Create a buffer stream with a new buffer */
  sio_stream_t stream;
  sio_error_t err = sio_stream_open_buffer(&stream, NULL, 128, SIO_STREAM_RDWR);
  if (err != SIO_SUCCESS) {
    printf("    Failed to create buffer stream: %s\n", sio_strerr(err));
    return 1;
  }
  
  /* Write data to the buffer stream */
  size_t bytes_written;
  err = sio_stream_write(&stream, test_data, test_data_len, &bytes_written, 0);
  if (err != SIO_SUCCESS || bytes_written != test_data_len) {
    printf("    Failed to write to buffer stream: %s\n", sio_strerr(err));
    sio_stream_close(&stream);
    return 1;
  }
  
  printf("    Wrote %zu bytes to buffer stream\n", bytes_written);
  
  /* Seek back to beginning */
  err = sio_stream_seek(&stream, 0, SIO_SEEK_SET, NULL);
  if (err != SIO_SUCCESS) {
    printf("    Failed to seek to beginning of buffer stream: %s\n", sio_strerr(err));
    sio_stream_close(&stream);
    return 1;
  }
  
  /* Read data from the buffer stream */
  char buffer[128] = {0};
  size_t bytes_read;
  err = sio_stream_read(&stream, buffer, sizeof(buffer) - 1, &bytes_read, 0);
  if (err != SIO_SUCCESS && err != SIO_ERROR_EOF) {
    printf("    Failed to read from buffer stream: %s\n", sio_strerr(err));
    sio_stream_close(&stream);
    return 1;
  }
  
  printf("    Read %zu bytes from buffer stream: \"%s\"\n", bytes_read, buffer);
  
  /* Verify the data */
  if (bytes_read != test_data_len || strcmp(buffer, test_data) != 0) {
    printf("    Data verification failed\n");
    printf("    Expected: \"%s\"\n", test_data);
    printf("    Got: \"%s\"\n", buffer);
    sio_stream_close(&stream);
    return 1;
  }
  
  /* Test buffer info */
  sio_stream_type_t type;
  size_t size = sizeof(type);
  err = sio_stream_get_option(&stream, SIO_INFO_TYPE, &type, &size);
  if (err != SIO_SUCCESS) {
    printf("    Failed to get stream type: %s\n", sio_strerr(err));
    sio_stream_close(&stream);
    return 1;
  }
  
  printf("    Stream type: %d (expected: %d)\n", type, SIO_STREAM_BUFFER);
  
  /* Verify size */
  uint64_t stream_size;
  size = sizeof(stream_size);
  err = sio_stream_get_option(&stream, SIO_INFO_SIZE, &stream_size, &size);
  if (err != SIO_SUCCESS) {
    printf("    Failed to get stream size: %s\n", sio_strerr(err));
    sio_stream_close(&stream);
    return 1;
  }
  
  printf("    Stream size: %zu (expected: %zu)\n", (size_t)stream_size, test_data_len);
  
  /* Test truncation (extend buffer) */
  err = sio_stream_truncate(&stream, test_data_len + 10);
  if (err != SIO_SUCCESS) {
    printf("    Failed to extend buffer via truncate: %s\n", sio_strerr(err));
    sio_stream_close(&stream);
    return 1;
  }
  
  /* Get new size */
  size = sizeof(stream_size);
  err = sio_stream_get_option(&stream, SIO_INFO_SIZE, &stream_size, &size);
  if (err != SIO_SUCCESS) {
    printf("    Failed to get stream size after extension: %s\n", sio_strerr(err));
    sio_stream_close(&stream);
    return 1;
  }
  
  printf("    Stream size after extension: %zu (expected: %zu)\n", 
         (size_t)stream_size, test_data_len + 10);
  
  /* Test truncation (shrink buffer) */
  err = sio_stream_truncate(&stream, 10);
  if (err != SIO_SUCCESS) {
    printf("    Failed to shrink buffer via truncate: %s\n", sio_strerr(err));
    sio_stream_close(&stream);
    return 1;
  }
  
  /* Get new size */
  size = sizeof(stream_size);
  err = sio_stream_get_option(&stream, SIO_INFO_SIZE, &stream_size, &size);
  if (err != SIO_SUCCESS) {
    printf("    Failed to get stream size after shrinking: %s\n", sio_strerr(err));
    sio_stream_close(&stream);
    return 1;
  }
  
  printf("    Stream size after shrinking: %zu (expected: %zu)\n", (size_t)stream_size, 10ul);
  
  /* Close the stream */
  err = sio_stream_close(&stream);
  if (err != SIO_SUCCESS) {
    printf("    Failed to close buffer stream: %s\n", sio_strerr(err));
    return 1;
  }
  
  printf("  Buffer stream test passed!\n");
  return 0;
}

/**
* @brief Test using an existing buffer with a buffer stream
*
* @return int 0 if successful, 1 otherwise
*/
static int test_existing_buffer_stream(void) {
  printf("  Testing buffer stream with existing buffer...\n");
  
  /* Create a buffer */
  sio_buffer_t buffer;
  sio_error_t err = sio_buffer_create(&buffer, 128);
  if (err != SIO_SUCCESS) {
    printf("    Failed to create buffer: %s\n", sio_strerr(err));
    return 1;
  }
  
  /* Write some data to the buffer directly */
  const char *test_data = "Existing Buffer Test";
  err = sio_buffer_write(&buffer, test_data, strlen(test_data));
  if (err != SIO_SUCCESS) {
    printf("    Failed to write to buffer: %s\n", sio_strerr(err));
    sio_buffer_destroy(&buffer);
    return 1;
  }
  
  /* Reset buffer position */
  err = sio_buffer_seek(&buffer, 0);
  if (err != SIO_SUCCESS) {
    printf("    Failed to seek buffer: %s\n", sio_strerr(err));
    sio_buffer_destroy(&buffer);
    return 1;
  }
  
  /* Create a stream with the existing buffer */
  sio_stream_t stream;
  err = sio_stream_open_buffer(&stream, &buffer, 0, SIO_STREAM_RDWR);
  if (err != SIO_SUCCESS) {
    printf("    Failed to create stream from existing buffer: %s\n", sio_strerr(err));
    sio_buffer_destroy(&buffer);
    return 1;
  }
  
  /* Read data from the stream */
  char read_buffer[128] = {0};
  size_t bytes_read;
  err = sio_stream_read(&stream, read_buffer, sizeof(read_buffer) - 1, &bytes_read, 0);
  if (err != SIO_SUCCESS && err != SIO_ERROR_EOF) {
    printf("    Failed to read from stream: %s\n", sio_strerr(err));
    sio_stream_close(&stream);
    sio_buffer_destroy(&buffer);
    return 1;
  }
  
  printf("    Read %zu bytes from existing buffer: \"%s\"\n", bytes_read, read_buffer);
  
  /* Verify the data */
  if (bytes_read != strlen(test_data) || strcmp(read_buffer, test_data) != 0) {
    printf("    Data verification failed\n");
    printf("    Expected: \"%s\"\n", test_data);
    printf("    Got: \"%s\"\n", read_buffer);
    sio_stream_close(&stream);
    sio_buffer_destroy(&buffer);
    return 1;
  }
  
  /* Close the stream */
  err = sio_stream_close(&stream);
  if (err != SIO_SUCCESS) {
    printf("    Failed to close stream: %s\n", sio_strerr(err));
    sio_buffer_destroy(&buffer);
    return 1;
  }
  
  /* Verify that the buffer still exists and has the same data */
  char verify_buffer[128] = {0};
  size_t verify_bytes;
  
  /* Reset buffer position */
  err = sio_buffer_seek(&buffer, 0);
  if (err != SIO_SUCCESS) {
    printf("    Failed to seek buffer after stream close: %s\n", sio_strerr(err));
    sio_buffer_destroy(&buffer);
    return 1;
  }
  
  /* Read from buffer */
  err = sio_buffer_read(&buffer, verify_buffer, sizeof(verify_buffer) - 1, &verify_bytes);
  if (err != SIO_SUCCESS && err != SIO_ERROR_EOF) {
    printf("    Failed to read from buffer after stream close: %s\n", sio_strerr(err));
    sio_buffer_destroy(&buffer);
    return 1;
  }
  
  /* Verify data is still intact */
  if (verify_bytes != strlen(test_data) || strcmp(verify_buffer, test_data) != 0) {
    printf("    Buffer data verification after stream close failed\n");
    printf("    Expected: \"%s\"\n", test_data);
    printf("    Got: \"%s\"\n", verify_buffer);
    sio_buffer_destroy(&buffer);
    return 1;
  }
  
  printf("    Buffer data still intact after stream close\n");
  
  /* Destroy the buffer */
  err = sio_buffer_destroy(&buffer);
  if (err != SIO_SUCCESS) {
    printf("    Failed to destroy buffer: %s\n", sio_strerr(err));
    return 1;
  }
  
  printf("  Buffer stream with existing buffer test passed!\n");
  return 0;
}

/**
* @brief Test raw memory stream operations
*
* @return int 0 if successful, 1 otherwise
*/
static int test_raw_memory_stream(void) {
  printf("  Testing raw memory stream...\n");
  
  /* Allocate memory for testing */
  const size_t mem_size = 128;
  uint8_t *memory = (uint8_t*)malloc(mem_size);
  if (!memory) {
    printf("    Failed to allocate memory for test\n");
    return 1;
  }
  
  /* Initialize memory with pattern */
  for (size_t i = 0; i < mem_size; i++) {
    memory[i] = (uint8_t)(i & 0xFF);
  }
  
  /* Create a raw memory stream */
  sio_stream_t stream;
  sio_error_t err = sio_stream_open_memory(&stream, memory, mem_size, SIO_STREAM_RDWR);
  if (err != SIO_SUCCESS) {
    printf("    Failed to create raw memory stream: %s\n", sio_strerr(err));
    free(memory);
    return 1;
  }
  
  /* Read first 16 bytes */
  uint8_t read_buffer[16];
  size_t bytes_read;
  err = sio_stream_read(&stream, read_buffer, sizeof(read_buffer), &bytes_read, 0);
  if (err != SIO_SUCCESS) {
    printf("    Failed to read from raw memory stream: %s\n", sio_strerr(err));
    sio_stream_close(&stream);
    free(memory);
    return 1;
  }
  
  printf("    Read %zu bytes from raw memory stream\n", bytes_read);
  
  /* Verify read data */
  for (size_t i = 0; i < bytes_read; i++) {
    if (read_buffer[i] != (uint8_t)(i & 0xFF)) {
      printf("    Data verification failed at index %zu\n", i);
      printf("    Expected: %u, Got: %u\n", (unsigned int)(i & 0xFF), (unsigned int)read_buffer[i]);
      sio_stream_close(&stream);
      free(memory);
      return 1;
    }
  }
  
  /* Seek to position 32 */
  err = sio_stream_seek(&stream, 32, SIO_SEEK_SET, NULL);
  if (err != SIO_SUCCESS) {
    printf("    Failed to seek in raw memory stream: %s\n", sio_strerr(err));
    sio_stream_close(&stream);
    free(memory);
    return 1;
  }
  
  /* Read 16 more bytes */
  err = sio_stream_read(&stream, read_buffer, sizeof(read_buffer), &bytes_read, 0);
  if (err != SIO_SUCCESS) {
    printf("    Failed to read after seek: %s\n", sio_strerr(err));
    sio_stream_close(&stream);
    free(memory);
    return 1;
  }
  
  /* Verify read data */
  for (size_t i = 0; i < bytes_read; i++) {
    if (read_buffer[i] != (uint8_t)((i + 32) & 0xFF)) {
      printf("    Data verification after seek failed at index %zu\n", i);
      printf("    Expected: %u, Got: %u\n", 
             (unsigned int)((i + 32) & 0xFF), (unsigned int)read_buffer[i]);
      sio_stream_close(&stream);
      free(memory);
      return 1;
    }
  }
  
  /* Seek to position 64 */
  err = sio_stream_seek(&stream, 64, SIO_SEEK_SET, NULL);
  if (err != SIO_SUCCESS) {
    printf("    Failed to seek to position 64: %s\n", sio_strerr(err));
    sio_stream_close(&stream);
    free(memory);
    return 1;
  }
  
  /* Write 16 bytes */
  uint8_t write_buffer[16];
  for (size_t i = 0; i < sizeof(write_buffer); i++) {
    write_buffer[i] = (uint8_t)(0xFF - i);
  }
  
  size_t bytes_written;
  err = sio_stream_write(&stream, write_buffer, sizeof(write_buffer), &bytes_written, 0);
  if (err != SIO_SUCCESS) {
    printf("    Failed to write to raw memory stream: %s\n", sio_strerr(err));
    sio_stream_close(&stream);
    free(memory);
    return 1;
  }
  
  printf("    Wrote %zu bytes to raw memory stream at position 64\n", bytes_written);
  
  /* Verify memory was modified correctly */
  for (size_t i = 0; i < bytes_written; i++) {
    if (memory[i + 64] != (uint8_t)(0xFF - i)) {
      printf("    Memory modification verification failed at index %zu\n", i + 64);
      printf("    Expected: %u, Got: %u\n", 
             (unsigned int)(0xFF - i), (unsigned int)memory[i + 64]);
      sio_stream_close(&stream);
      free(memory);
      return 1;
    }
  }
  
  /* Get stream size */
  uint64_t stream_size;
  size_t size = sizeof(stream_size);
  err = sio_stream_get_option(&stream, SIO_INFO_SIZE, &stream_size, &size);
  if (err != SIO_SUCCESS) {
    printf("    Failed to get stream size: %s\n", sio_strerr(err));
    sio_stream_close(&stream);
    free(memory);
    return 1;
  }
  
  printf("    Stream size: %zu (expected: %zu)\n", (size_t)stream_size, mem_size);
  
  /* Verify size */
  if (stream_size != mem_size) {
    printf("    Stream size verification failed\n");
    sio_stream_close(&stream);
    free(memory);
    return 1;
  }
  
  /* Test that you can't write past the end of the memory */
  err = sio_stream_seek(&stream, (int64_t)mem_size - 8, SIO_SEEK_SET, NULL);
  if (err != SIO_SUCCESS) {
    printf("    Failed to seek to near end: %s\n", sio_strerr(err));
    sio_stream_close(&stream);
    free(memory);
    return 1;
  }
  
  uint8_t end_buffer[16] = {0x42}; /* Initialize with pattern */
  err = sio_stream_write(&stream, end_buffer, sizeof(end_buffer), &bytes_written, 0);
  if (err != SIO_SUCCESS) {
    /* Expected to succeed but write less than requested */
    printf("    Unexpected error on partial write: %s\n", sio_strerr(err));
    sio_stream_close(&stream);
    free(memory);
    return 1;
  }
  
  printf("    Partial write at end: requested %zu bytes, wrote %zu bytes\n", 
         sizeof(end_buffer), bytes_written);
  
  /* Verify that only 8 bytes were written */
  if (bytes_written != 8) {
    printf("    Expected 8 bytes written at end, got %zu\n", bytes_written);
    sio_stream_close(&stream);
    free(memory);
    return 1;
  }
  
  /* Close the stream */
  err = sio_stream_close(&stream);
  if (err != SIO_SUCCESS) {
    printf("    Failed to close raw memory stream: %s\n", sio_strerr(err));
    free(memory);
    return 1;
  }
  
  /* Free the memory */
  free(memory);
  
  printf("  Raw memory stream test passed!\n");
  return 0;
}

/**
* @brief Run all memory stream tests
*
* @return int 0 if all tests pass, 1 otherwise
*/
int test_memory_streams(void) {
  int failed = 0;
  
  failed |= test_buffer_stream();
  failed |= test_existing_buffer_stream();
  failed |= test_raw_memory_stream();
  
  return failed;
}