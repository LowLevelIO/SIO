/**
* @file test_buffer.c
* @brief Test program for the SIO buffer system
*
* This program demonstrates proper usage of the SIO buffer system, showing
* various operations like creation, writing, reading, resizing, and using pools.
*
* @author zczxy
* @version 0.1.0
*/

#include <sio/buf.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

/**
* @brief Report an error and exit
*
* @param error_code The SIO error code
* @param message Custom error message
*/
static void report_error_and_exit(sio_error_t error_code, const char *message) {
  fprintf(stderr, "Error: %s: %s\n", message, sio_strerr(error_code));
  exit(EXIT_FAILURE);
}

/**
* @brief Test basic buffer operations
*/
static void test_basic_operations(void) {
  printf("Testing basic buffer operations...\n");
  
  sio_buffer_t buffer;
  sio_error_t err;
  
  /* Create a buffer */
  err = sio_buffer_create(&buffer, 0); /* Use default size */
  if (err != SIO_SUCCESS) {
    report_error_and_exit(err, "Failed to create buffer");
  }
  
  /* Write data to the buffer */
  const char *test_string = "Hello, SIO Buffer!";
  err = sio_buffer_write(&buffer, test_string, strlen(test_string) + 1); /* Include null terminator */
  if (err != SIO_SUCCESS) {
    report_error_and_exit(err, "Failed to write to buffer");
  }
  
  printf("  Wrote %zu bytes to buffer\n", buffer.size);
  
  /* Seek to beginning */
  err = sio_buffer_seek(&buffer, 0);
  if (err != SIO_SUCCESS) {
    report_error_and_exit(err, "Failed to seek buffer");
  }
  
  /* Read data from the buffer */
  char read_buffer[64] = {0};
  size_t bytes_read;
  err = sio_buffer_read(&buffer, read_buffer, sizeof(read_buffer), &bytes_read);
  if (err != SIO_SUCCESS && err != SIO_ERROR_EOF) {
    report_error_and_exit(err, "Failed to read from buffer");
  }
  
  printf("  Read %zu bytes: \"%s\"\n", bytes_read, read_buffer);
  
  /* Verify read data matches written data */
  assert(strcmp(test_string, read_buffer) == 0);
  printf("  Data verification successful\n");
  
  /* Destroy the buffer */
  err = sio_buffer_destroy(&buffer);
  if (err != SIO_SUCCESS) {
    report_error_and_exit(err, "Failed to destroy buffer");
  }
  
  printf("Basic buffer operations test passed!\n\n");
}

/**
* @brief Test buffer resizing
*/
static void test_buffer_resizing(void) {
  printf("Testing buffer resizing...\n");
  
  sio_buffer_t buffer;
  sio_error_t err;
  
  /* Create a small buffer */
  err = sio_buffer_create(&buffer, 16); /* Small initial size */
  if (err != SIO_SUCCESS) {
    report_error_and_exit(err, "Failed to create buffer");
  }
  
  printf("  Initial capacity: %zu bytes\n", buffer.capacity);
  
  /* Write data larger than initial capacity to trigger auto-resize */
  const char *test_data = "This is a test string that is longer than the initial buffer capacity.";
  size_t test_data_len = strlen(test_data) + 1;
  
  err = sio_buffer_write(&buffer, test_data, test_data_len);
  if (err != SIO_SUCCESS) {
    report_error_and_exit(err, "Failed to write to buffer");
  }
  
  printf("  After auto-resize: capacity = %zu bytes, size = %zu bytes\n", 
         buffer.capacity, buffer.size);
  assert(buffer.capacity >= test_data_len);
  
  /* Manual resizing - grow */
  size_t new_capacity = buffer.capacity * 2;
  err = sio_buffer_resize(&buffer, new_capacity);
  if (err != SIO_SUCCESS) {
    report_error_and_exit(err, "Failed to resize buffer");
  }
  
  printf("  After manual resize (grow): capacity = %zu bytes\n", buffer.capacity);
  assert(buffer.capacity >= new_capacity);
  
  /* Manual resizing - shrink */
  err = sio_buffer_shrink_to_fit(&buffer);
  if (err != SIO_SUCCESS) {
    report_error_and_exit(err, "Failed to shrink buffer");
  }
  
  printf("  After shrink_to_fit: capacity = %zu bytes, size = %zu bytes\n", 
         buffer.capacity, buffer.size);
  assert(buffer.capacity >= buffer.size);
  
  /* Verify data integrity after all resizing operations */
  err = sio_buffer_seek(&buffer, 0);
  if (err != SIO_SUCCESS) {
    report_error_and_exit(err, "Failed to seek buffer");
  }
  
  char read_buffer[128] = {0};
  size_t bytes_read;
  err = sio_buffer_read(&buffer, read_buffer, sizeof(read_buffer), &bytes_read);
  if (err != SIO_SUCCESS && err != SIO_ERROR_EOF) {
    report_error_and_exit(err, "Failed to read from buffer");
  }
  
  assert(strcmp(test_data, read_buffer) == 0);
  printf("  Data integrity preserved after resizing\n");
  
  /* Destroy the buffer */
  err = sio_buffer_destroy(&buffer);
  if (err != SIO_SUCCESS) {
    report_error_and_exit(err, "Failed to destroy buffer");
  }
  
  printf("Buffer resizing test passed!\n\n");
}

/**
* @brief Test buffer binary data handling
*/
static void test_binary_data(void) {
  printf("Testing binary data handling...\n");
  
  sio_buffer_t buffer;
  sio_error_t err;
  
  /* Create a buffer */
  err = sio_buffer_create(&buffer, 0);
  if (err != SIO_SUCCESS) {
    report_error_and_exit(err, "Failed to create buffer");
  }
  
  /* Write integers of different sizes */
  uint8_t u8 = 0x42;
  uint16_t u16 = 0xABCD;
  uint32_t u32 = 0x12345678;
  uint64_t u64 = 0x0123456789ABCDEF;
  
  err = sio_buffer_write_uint8(&buffer, u8);
  if (err != SIO_SUCCESS) {
    report_error_and_exit(err, "Failed to write uint8");
  }
  
  err = sio_buffer_write_uint16(&buffer, u16);
  if (err != SIO_SUCCESS) {
    report_error_and_exit(err, "Failed to write uint16");
  }
  
  err = sio_buffer_write_uint32(&buffer, u32);
  if (err != SIO_SUCCESS) {
    report_error_and_exit(err, "Failed to write uint32");
  }
  
  err = sio_buffer_write_uint64(&buffer, u64);
  if (err != SIO_SUCCESS) {
    report_error_and_exit(err, "Failed to write uint64");
  }
  
  printf("  Wrote binary values: 0x%02X, 0x%04X, 0x%08X, 0x%016llX\n", u8, u16, u32, (unsigned long long)u64);
  
  /* Seek to beginning */
  err = sio_buffer_seek(&buffer, 0);
  if (err != SIO_SUCCESS) {
    report_error_and_exit(err, "Failed to seek buffer");
  }
  
  /* Read back values */
  uint8_t r8;
  uint16_t r16;
  uint32_t r32;
  uint64_t r64;
  
  err = sio_buffer_read_uint8(&buffer, &r8);
  if (err != SIO_SUCCESS) {
    report_error_and_exit(err, "Failed to read uint8");
  }
  
  err = sio_buffer_read_uint16(&buffer, &r16);
  if (err != SIO_SUCCESS) {
    report_error_and_exit(err, "Failed to read uint16");
  }
  
  err = sio_buffer_read_uint32(&buffer, &r32);
  if (err != SIO_SUCCESS) {
    report_error_and_exit(err, "Failed to read uint32");
  }
  
  err = sio_buffer_read_uint64(&buffer, &r64);
  if (err != SIO_SUCCESS) {
    report_error_and_exit(err, "Failed to read uint64");
  }
  
  printf("  Read binary values: 0x%02X, 0x%04X, 0x%08X, 0x%016llX\n", r8, r16, r32, (unsigned long long)r64);
  
  /* Verify values */
  assert(r8 == u8);
  assert(r16 == u16);
  assert(r32 == u32);
  assert(r64 == u64);
  
  printf("  Binary value verification successful\n");
  
  /* Destroy the buffer */
  err = sio_buffer_destroy(&buffer);
  if (err != SIO_SUCCESS) {
    report_error_and_exit(err, "Failed to destroy buffer");
  }
  
  printf("Binary data handling test passed!\n\n");
}

/**
* @brief Test buffer pool
*/
static void test_buffer_pool(void) {
  printf("Testing buffer pool...\n");
  
  sio_buffer_pool_t pool;
  sio_error_t err;
  
  /* Create a buffer pool */
  const size_t POOL_SIZE = 4;
  const size_t BUFFER_SIZE = 1024;
  
  err = sio_buffer_pool_create(&pool, POOL_SIZE, BUFFER_SIZE);
  if (err != SIO_SUCCESS) {
    report_error_and_exit(err, "Failed to create buffer pool");
  }
  
  printf("  Created pool with %zu buffers of %zu bytes each\n", POOL_SIZE, BUFFER_SIZE);
  
  /* Acquire and use buffers */
  sio_buffer_t *buffers[POOL_SIZE];
  const char *test_data[] = {
    "Buffer 1 data",
    "Buffer 2 has some different content",
    "Buffer 3 contains yet another test string",
    "And finally, buffer 4 has this message"
  };
  
  for (size_t i = 0; i < POOL_SIZE; i++) {
    err = sio_buffer_pool_acquire(&pool, &buffers[i]);
    if (err != SIO_SUCCESS) {
      report_error_and_exit(err, "Failed to acquire buffer from pool");
    }
    
    printf("  Acquired buffer %zu from pool\n", i + 1);
    
    /* Write some data to this buffer */
    err = sio_buffer_write(buffers[i], test_data[i], strlen(test_data[i]) + 1);
    if (err != SIO_SUCCESS) {
      report_error_and_exit(err, "Failed to write to buffer from pool");
    }
  }
  
  /* Try to acquire one more buffer (should fail) */
  sio_buffer_t *extra_buffer;
  err = sio_buffer_pool_acquire(&pool, &extra_buffer);
  if (err == SIO_ERROR_BUSY) {
    printf("  Correctly failed to acquire buffer beyond pool capacity\n");
  } else {
    report_error_and_exit(err, "Unexpected result when acquiring buffer beyond capacity");
  }
  
  /* Verify data in each buffer */
  for (size_t i = 0; i < POOL_SIZE; i++) {
    err = sio_buffer_seek(buffers[i], 0);
    if (err != SIO_SUCCESS) {
      report_error_and_exit(err, "Failed to seek buffer from pool");
    }
    
    char read_buffer[128] = {0};
    size_t bytes_read;
    err = sio_buffer_read(buffers[i], read_buffer, sizeof(read_buffer), &bytes_read);
    if (err != SIO_SUCCESS && err != SIO_ERROR_EOF) {
      report_error_and_exit(err, "Failed to read from buffer from pool");
    }
    
    printf("  Buffer %zu contains: \"%s\"\n", i + 1, read_buffer);
    assert(strcmp(test_data[i], read_buffer) == 0);
  }
  
  /* Release all buffers */
  for (size_t i = 0; i < POOL_SIZE; i++) {
    err = sio_buffer_pool_release(&pool, buffers[i]);
    if (err != SIO_SUCCESS) {
      report_error_and_exit(err, "Failed to release buffer to pool");
    }
    printf("  Released buffer %zu back to pool\n", i + 1);
  }
  
  /* Try to release a buffer again (should fail) */
  err = sio_buffer_pool_release(&pool, buffers[0]);
  if (err == SIO_ERROR_FILE_CLOSED) {
    printf("  Correctly failed to release already-released buffer\n");
  } else {
    report_error_and_exit(err, "Unexpected result when releasing already-released buffer");
  }
  
  /* Acquire a buffer again (should succeed now) */
  err = sio_buffer_pool_acquire(&pool, &extra_buffer);
  if (err != SIO_SUCCESS) {
    report_error_and_exit(err, "Failed to acquire buffer after releases");
  }
  printf("  Successfully acquired buffer after releases\n");
  
  /* Release the extra buffer */
  err = sio_buffer_pool_release(&pool, extra_buffer);
  if (err != SIO_SUCCESS) {
    report_error_and_exit(err, "Failed to release extra buffer");
  }
  
  /* Resize the pool */
  const size_t NEW_POOL_SIZE = 6;
  err = sio_buffer_pool_resize(&pool, NEW_POOL_SIZE);
  if (err != SIO_SUCCESS) {
    report_error_and_exit(err, "Failed to resize buffer pool");
  }
  printf("  Resized pool from %zu to %zu buffers\n", POOL_SIZE, NEW_POOL_SIZE);
  
  /* Destroy the buffer pool */
  err = sio_buffer_pool_destroy(&pool);
  if (err != SIO_SUCCESS) {
    report_error_and_exit(err, "Failed to destroy buffer pool");
  }
  
  printf("Buffer pool test passed!\n\n");
}

/**
* @brief Test wrapping external memory
*/
static void test_external_memory(void) {
  printf("Testing external memory wrapping...\n");
  
  sio_buffer_t buffer;
  sio_error_t err;
  
  /* Create some external memory */
  const size_t EXT_SIZE = 128;
  uint8_t *ext_memory = (uint8_t*)malloc(EXT_SIZE);
  if (!ext_memory) {
    fprintf(stderr, "Failed to allocate external memory\n");
    exit(EXIT_FAILURE);
  }
  
  /* Fill with pattern */
  for (size_t i = 0; i < EXT_SIZE; i++) {
    ext_memory[i] = (uint8_t)(i & 0xFF);
  }
  
  /* Wrap the external memory in a buffer */
  err = sio_buffer_from_memory(&buffer, ext_memory, EXT_SIZE);
  if (err != SIO_SUCCESS) {
    free(ext_memory);
    report_error_and_exit(err, "Failed to wrap external memory");
  }
  
  printf("  Successfully wrapped %zu bytes of external memory\n", EXT_SIZE);
  
  /* Verify buffer state */
  assert(buffer.data == ext_memory);
  assert(buffer.size == EXT_SIZE);
  assert(buffer.capacity == EXT_SIZE);
  assert(buffer.position == 0);
  assert(buffer.owns_memory == 0);
  
  /* Read some data */
  uint8_t read_bytes[16];
  size_t bytes_read;
  
  err = sio_buffer_read(&buffer, read_bytes, sizeof(read_bytes), &bytes_read);
  if (err != SIO_SUCCESS && err != SIO_ERROR_EOF) {
    free(ext_memory);
    report_error_and_exit(err, "Failed to read from external memory buffer");
  }
  
  printf("  Read %zu bytes from external memory buffer\n", bytes_read);
  
  /* Verify the read data */
  for (size_t i = 0; i < bytes_read; i++) {
    assert(read_bytes[i] == (uint8_t)(i & 0xFF));
  }
  
  /* Try to resize (should fail since we don't own the memory) */
  err = sio_buffer_resize(&buffer, EXT_SIZE * 2);
  if (err == SIO_ERROR_FILE_READONLY) {
    printf("  Correctly failed to resize external memory buffer\n");
  } else {
    free(ext_memory);
    report_error_and_exit(err, "Unexpected result when resizing external memory buffer");
  }
  
  /* Destroy the buffer (should not free the external memory) */
  err = sio_buffer_destroy(&buffer);
  if (err != SIO_SUCCESS) {
    free(ext_memory);
    report_error_and_exit(err, "Failed to destroy external memory buffer");
  }
  
  /* Verify external memory is still intact */
  for (size_t i = 0; i < EXT_SIZE; i++) {
    assert(ext_memory[i] == (uint8_t)(i & 0xFF));
  }
  
  printf("  External memory still valid after buffer destruction\n");
  
  /* Free the external memory */
  free(ext_memory);
  
  printf("External memory wrapping test passed!\n\n");
}

/**
* @brief Main function
*
* @return int Exit code
*/
int main(void) {
  printf("===== SIO Buffer System Test =====\n\n");
  
  /* Run test cases */
  test_basic_operations();
  test_buffer_resizing();
  test_binary_data();
  test_buffer_pool();
  test_external_memory();
  
  printf("All tests passed successfully!\n");
  return EXIT_SUCCESS;
}