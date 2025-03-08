/**
* @file tests/thread_test.c
* @brief Thread functionality test suite
*
* This file contains test routines for the SIO threading functionality.
* It tests threads, mutexes, condition variables, and other synchronization primitives.
*
* @author zczxy
* @version 0.1.0
*/

#include <sio/aux/thread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

/* Global counter for thread test */
static int g_counter = 0;

/* Global mutex for thread test */
static sio_mutex_t g_mutex;

/* Global condition variable for thread test */
static sio_cond_t g_cond;

/* Global flag for condition variable test */
static int g_cond_flag = 0;

/* Global count for event completion */
static int g_event_count = 0;

/* Global barrier for barrier test */
static sio_barrier_t g_barrier;

/* Global semaphore for semaphore test */
static sio_sem_t g_semaphore;

/* Global thread pool for thread pool test */
static sio_threadpool_t g_threadpool;

/**
* @brief Test thread function for basic thread test
*
* @param arg Thread argument
* @return void* Thread result
*/
void *thread_test_func(void *arg) {
  int *value = (int*)arg;
  
  printf("Thread %d running\n", *value);
  
  /* Sleep to allow other threads to run */
  sio_thread_sleep(100);
  
  /* Increment the counter */
  sio_mutex_lock(&g_mutex);
  g_counter++;
  sio_mutex_unlock(&g_mutex);
  
  /* Free the argument */
  free(arg);
  
  /* Return the thread ID as the result */
  return (void*)(uintptr_t)sio_thread_get_id();
}

/**
* @brief Test thread function for condition variable test
*
* @param arg Thread argument
* @return void* Thread result
*/
void *cond_test_func(void *arg) {
  int wait_time_ms = arg ? *(int*)arg : 1000;
  
  printf("Condition thread waiting for signal\n");
  
  /* Lock the mutex */
  sio_mutex_lock(&g_mutex);
  
  /* Wait for the condition */
  while (!g_cond_flag) {
    if (wait_time_ms < 0) {
      /* Infinite wait */
      sio_cond_wait(&g_cond, &g_mutex);
    } else {
      /* Timed wait */
      sio_error_t err = sio_cond_timedwait(&g_cond, &g_mutex, wait_time_ms);
      
      if (err == SIO_ERROR_TIMEOUT) {
        printf("Condition wait timed out\n");
        sio_mutex_unlock(&g_mutex);
        return (void*)(uintptr_t)SIO_ERROR_TIMEOUT;
      }
    }
  }
  
  printf("Condition signal received\n");
  
  /* Reset the flag */
  g_cond_flag = 0;
  
  /* Unlock the mutex */
  sio_mutex_unlock(&g_mutex);
  
  return (void*)1;
}

/**
* @brief Test thread function for barrier test
*
* @param arg Thread argument
* @return void* Thread result
*/
void *barrier_test_func(void *arg) {
  int thread_id = arg ? *(int*)arg : 0;
  
  printf("Barrier thread %d running\n", thread_id);
  
  /* Sleep for a random time to simulate work */
  sio_thread_sleep((thread_id + 1) * 100);
  
  printf("Barrier thread %d waiting at barrier\n", thread_id);
  
  /* Wait at the barrier */
  sio_barrier_wait(&g_barrier);
  
  printf("Barrier thread %d passed barrier\n", thread_id);
  
  /* Increment the event count */
  sio_mutex_lock(&g_mutex);
  g_event_count++;
  sio_mutex_unlock(&g_mutex);
  
  return (void*)(uintptr_t)thread_id;
}

/**
* @brief Task function for thread pool test
*
* @param arg Task argument
*/
void threadpool_task_func(void *arg) {
  int task_id = arg ? *(int*)arg : 0;
  
  printf("Thread pool task %d running\n", task_id);
  
  /* Sleep to simulate work */
  sio_thread_sleep(100);
  
  /* Increment the event count */
  sio_mutex_lock(&g_mutex);
  g_event_count++;
  sio_mutex_unlock(&g_mutex);
  
  /* Free the argument */
  free(arg);
}

/**
* @brief Run basic thread tests
*
* @return int 0 on success, non-zero on failure
*/
int test_threads(void) {
  printf("=== Running thread tests ===\n");
  
  /* Initialize the mutex */
  sio_error_t err = sio_mutex_init(&g_mutex, 0);
  assert(err == SIO_SUCCESS);
  
  /* Reset the counter */
  g_counter = 0;
  
  /* Create multiple threads */
  sio_thread_t threads[5];
  void *results[5];
  
  for (int i = 0; i < 5; i++) {
    int *arg = (int*)malloc(sizeof(int));
    *arg = i;
    
    err = sio_thread_create(&threads[i], thread_test_func, arg, SIO_THREAD_DEFAULT);
    assert(err == SIO_SUCCESS);
  }
  
  /* Wait for all threads to complete */
  for (int i = 0; i < 5; i++) {
    err = sio_thread_join(&threads[i], &results[i]);
    assert(err == SIO_SUCCESS);
    
    printf("Thread %d completed with result %p\n", i, results[i]);
  }
  
  /* Check the counter */
  assert(g_counter == 5);
  
  /* Clean up */
  sio_mutex_destroy(&g_mutex);
  
  printf("Thread tests passed\n");
  return 0;
}

/**
* @brief Run mutex tests
*
* @return int 0 on success, non-zero on failure
*/
int test_mutexes(void) {
  printf("=== Running mutex tests ===\n");
  
  /* Initialize the mutex */
  sio_mutex_t mutex;
  sio_error_t err = sio_mutex_init(&mutex, 0);
  assert(err == SIO_SUCCESS);
  
  /* Test lock and unlock */
  err = sio_mutex_lock(&mutex);
  assert(err == SIO_SUCCESS);
  
  /* Test trylock while locked (should fail) */
  err = sio_mutex_trylock(&mutex);
  assert(err == SIO_ERROR_BUSY);
  
  /* Test timedlock while locked (should time out) */
  err = sio_mutex_timedlock(&mutex, 100);
  assert(err == SIO_ERROR_TIMEOUT);
  
  /* Unlock and try again */
  err = sio_mutex_unlock(&mutex);
  assert(err == SIO_SUCCESS);
  
  /* Test trylock while unlocked (should succeed) */
  err = sio_mutex_trylock(&mutex);
  assert(err == SIO_SUCCESS);
  
  /* Unlock */
  err = sio_mutex_unlock(&mutex);
  assert(err == SIO_SUCCESS);
  
  /* Test timedlock while unlocked (should succeed) */
  err = sio_mutex_timedlock(&mutex, 100);
  assert(err == SIO_SUCCESS);
  
  /* Unlock */
  err = sio_mutex_unlock(&mutex);
  assert(err == SIO_SUCCESS);
  
  /* Test recursive mutex */
  sio_mutex_t rec_mutex;
  err = sio_mutex_init(&rec_mutex, 1);
  assert(err == SIO_SUCCESS);
  
  /* Lock multiple times */
  err = sio_mutex_lock(&rec_mutex);
  assert(err == SIO_SUCCESS);
  
  err = sio_mutex_lock(&rec_mutex);
  assert(err == SIO_SUCCESS);
  
  /* Unlock multiple times */
  err = sio_mutex_unlock(&rec_mutex);
  assert(err == SIO_SUCCESS);
  
  err = sio_mutex_unlock(&rec_mutex);
  assert(err == SIO_SUCCESS);
  
  /* Clean up */
  sio_mutex_destroy(&mutex);
  sio_mutex_destroy(&rec_mutex);
  
  printf("Mutex tests passed\n");
  return 0;
}

/**
* @brief Run condition variable tests
*
* @return int 0 on success, non-zero on failure
*/
int test_condition_variables(void) {
  printf("=== Running condition variable tests ===\n");
  
  /* Initialize the mutex and condition variable */
  sio_error_t err = sio_mutex_init(&g_mutex, 0);
  assert(err == SIO_SUCCESS);
  
  err = sio_cond_init(&g_cond);
  assert(err == SIO_SUCCESS);
  
  /* Reset the flag */
  g_cond_flag = 0;
  
  /* Create a thread waiting on the condition */
  sio_thread_t thread;
  void *result;
  
  err = sio_thread_create(&thread, cond_test_func, NULL, SIO_THREAD_DEFAULT);
  assert(err == SIO_SUCCESS);
  
  /* Sleep to allow the thread to start waiting */
  sio_thread_sleep(200);
  
  /* Set the flag and signal the condition */
  sio_mutex_lock(&g_mutex);
  g_cond_flag = 1;
  err = sio_cond_signal(&g_cond);
  assert(err == SIO_SUCCESS);
  sio_mutex_unlock(&g_mutex);
  
  /* Wait for the thread to complete */
  err = sio_thread_join(&thread, &result);
  assert(err == SIO_SUCCESS);
  
  /* Check the result */
  assert(result == (void*)1);
  
  /* Test condition variable with timeout */
  int wait_time_ms = 200;
  
  err = sio_thread_create(&thread, cond_test_func, &wait_time_ms, SIO_THREAD_DEFAULT);
  assert(err == SIO_SUCCESS);
  
  /* Wait for the thread to time out */
  err = sio_thread_join(&thread, &result);
  assert(err == SIO_SUCCESS);
  
  /* Check the result */
  assert((sio_error_t)(uintptr_t)result == SIO_ERROR_TIMEOUT);
  
  /* Clean up */
  sio_cond_destroy(&g_cond);
  sio_mutex_destroy(&g_mutex);
  
  printf("Condition variable tests passed\n");
  return 0;
}

/**
* @brief Run barrier tests
*
* @return int 0 on success, non-zero on failure
*/
int test_barriers(void) {
  printf("=== Running barrier tests ===\n");
  
  const int num_threads = 5;
  
  /* Initialize the mutex and barrier */
  sio_error_t err = sio_mutex_init(&g_mutex, 0);
  assert(err == SIO_SUCCESS);
  
  err = sio_barrier_init(&g_barrier, num_threads);
  assert(err == SIO_SUCCESS);
  
  /* Reset the event count */
  g_event_count = 0;
  
  /* Create multiple threads */
  sio_thread_t threads[num_threads];
  int thread_ids[num_threads];
  
  for (int i = 0; i < num_threads; i++) {
    thread_ids[i] = i;
    
    err = sio_thread_create(&threads[i], barrier_test_func, &thread_ids[i], SIO_THREAD_DEFAULT);
    assert(err == SIO_SUCCESS);
  }
  
  /* Wait for all threads to complete */
  for (int i = 0; i < num_threads; i++) {
    void *result;
    err = sio_thread_join(&threads[i], &result);
    assert(err == SIO_SUCCESS);
    
    printf("Barrier thread %d completed with result %d\n", i, (int)(uintptr_t)result);
  }
  
  /* Check that all threads passed the barrier */
  assert(g_event_count == num_threads);
  
  /* Clean up */
  sio_barrier_destroy(&g_barrier);
  sio_mutex_destroy(&g_mutex);
  
  printf("Barrier tests passed\n");
  return 0;
}

/**
* @brief Run semaphore tests
*
* @return int 0 on success, non-zero on failure
*/
int test_semaphores(void) {
  printf("=== Running semaphore tests ===\n");
  
  /* Initialize the semaphore */
  sio_error_t err = sio_sem_init(&g_semaphore, 2, 2);
  assert(err == SIO_SUCCESS);
  
  /* Test basic operations */
  err = sio_sem_wait(&g_semaphore);
  assert(err == SIO_SUCCESS);
  
  err = sio_sem_wait(&g_semaphore);
  assert(err == SIO_SUCCESS);
  
  /* Try to wait again (should fail) */
  err = sio_sem_trywait(&g_semaphore);
  assert(err == SIO_ERROR_BUSY);
  
  /* Post to the semaphore */
  err = sio_sem_post(&g_semaphore);
  assert(err == SIO_SUCCESS);
  
  /* Try to wait again (should succeed) */
  err = sio_sem_trywait(&g_semaphore);
  assert(err == SIO_SUCCESS);
  
  /* Test timedwait */
  err = sio_sem_timedwait(&g_semaphore, 100);
  assert(err == SIO_ERROR_TIMEOUT);
  
  /* Post to the semaphore */
  err = sio_sem_post(&g_semaphore);
  assert(err == SIO_SUCCESS);
  
  /* Test get_value */
  int value;
  err = sio_sem_get_value(&g_semaphore, &value);
  if (err == SIO_SUCCESS) {
    printf("Semaphore value: %d\n", value);
  } else {
    printf("sio_sem_get_value not supported on this platform\n");
  }
  
  /* Clean up */
  sio_sem_destroy(&g_semaphore);
  
  printf("Semaphore tests passed\n");
  return 0;
}

/**
* @brief Run thread pool tests
*
* @return int 0 on success, non-zero on failure
*/
int test_threadpool(void) {
  printf("=== Running thread pool tests ===\n");
  
  const int num_threads = 3;
  const int num_tasks = 10;
  
  /* Initialize the mutex */
  sio_error_t err = sio_mutex_init(&g_mutex, 0);
  assert(err == SIO_SUCCESS);
  
  /* Create the thread pool */
  err = sio_threadpool_create(&g_threadpool, num_threads, num_tasks);
  assert(err == SIO_SUCCESS);
  
  /* Reset the event count */
  g_event_count = 0;
  
  /* Add tasks to the thread pool */
  for (int i = 0; i < num_tasks; i++) {
    int *arg = (int*)malloc(sizeof(int));
    *arg = i;
    
    err = sio_threadpool_add_task(&g_threadpool, threadpool_task_func, arg, 1);
    assert(err == SIO_SUCCESS);
  }
  
  /* Wait for all tasks to complete (simple sleep-based approach) */
  while (g_event_count < num_tasks) {
    sio_thread_sleep(100);
  }
  
  /* Verify that all tasks have run */
  assert(g_event_count == num_tasks);
  
  /* Test pausing the thread pool */
  g_event_count = 0;
  
  /* Pause the thread pool */
  err = sio_threadpool_pause(&g_threadpool);
  assert(err == SIO_SUCCESS);
  
  /* Add more tasks */
  for (int i = 0; i < num_tasks; i++) {
    int *arg = (int*)malloc(sizeof(int));
    *arg = i + num_tasks;
    
    err = sio_threadpool_add_task(&g_threadpool, threadpool_task_func, arg, 1);
    assert(err == SIO_SUCCESS);
  }
  
  /* Sleep to give time for tasks to run (they shouldn't) */
  sio_thread_sleep(500);
  
  /* Verify that no tasks have run */
  assert(g_event_count == 0);
  
  /* Resume the thread pool */
  err = sio_threadpool_resume(&g_threadpool);
  assert(err == SIO_SUCCESS);
  
  /* Wait for all tasks to complete */
  while (g_event_count < num_tasks) {
    sio_thread_sleep(100);
  }
  
  /* Verify that all tasks have run */
  assert(g_event_count == num_tasks);
  
  /* Clean up */
  err = sio_threadpool_destroy(&g_threadpool, 1);
  assert(err == SIO_SUCCESS);
  
  sio_mutex_destroy(&g_mutex);
  
  printf("Thread pool tests passed\n");
  return 0;
}

/**
* @brief Run atomic operation tests
*
* @return int 0 on success, non-zero on failure
*/
int test_atomic_operations(void) {
  printf("=== Running atomic operation tests ===\n");
  
  /* Test atomic operations */
  volatile int32_t value = 0;
  
  /* Test atomic increment */
  int32_t result = SIO_ATOMIC_INC(&value);
  assert(result == 1);
  assert(value == 1);
  
  /* Test atomic decrement */
  result = SIO_ATOMIC_DEC(&value);
  assert(result == 0);
  assert(value == 0);
  
  /* Test atomic add */
  result = SIO_ATOMIC_ADD(&value, 5);
  assert(result == 5);
  assert(value == 5);
  
  /* Test atomic subtract */
  result = SIO_ATOMIC_SUB(&value, 2);
  assert(result == 3);
  assert(value == 3);
  
  /* Test atomic compare-and-swap */
  int oldval = 3;
  int success = SIO_ATOMIC_CAS(&value, oldval, 10);
  assert(success);
  assert(value == 10);
  
  success = SIO_ATOMIC_CAS(&value, oldval, 20);
  assert(!success);
  assert(value == 10);
  
  /* Test atomic store and load */
  SIO_ATOMIC_STORE(&value, 42);
  result = SIO_ATOMIC_LOAD(&value);
  assert(result == 42);
  assert(value == 42);
  
  /* Test memory barriers */
  SIO_MEMORY_BARRIER();
  SIO_READ_BARRIER();
  SIO_WRITE_BARRIER();
  
  printf("Atomic operation tests passed\n");
  return 0;
}

/**
* @brief Run advanced OS-specific tests if available
*
* @return int 0 on success, non-zero on failure
*/
int test_os_specific_features(void) {
  printf("=== Running OS-specific feature tests ===\n");
  
  sio_error_t err;

#if defined(SIO_OS_LINUX)
  printf("Testing Linux-specific features\n");
  
  /* Test futex operations */
  int32_t futex_word = 0;
  
  /* Start a thread that waits on the futex */
  sio_thread_t thread;
  void *thread_arg = (void*)&futex_word;
  
  sio_thread_create(&thread, (sio_thread_func_t)sio_futex_wait, thread_arg, SIO_THREAD_DEFAULT);
  
  /* Sleep to allow the thread to start waiting */
  sio_thread_sleep(100);
  
  /* Wake up the waiting thread */
  err = sio_futex_wake(&futex_word, 1);
  assert(err == SIO_SUCCESS);
  
  /* Wait for the thread to complete */
  sio_thread_join(&thread, NULL);
  
#elif defined(SIO_OS_WINDOWS)
  printf("Testing Windows-specific features\n");
  
  /* Test waitable timer */
  HANDLE timer_handle;
  sio_error_t err = sio_win_create_waitable_timer(&timer_handle, 0, NULL);
  assert(err == SIO_SUCCESS);
  
  /* Set the timer */
  err = sio_win_set_waitable_timer(timer_handle, -100, 0);
  assert(err == SIO_SUCCESS);
  
  /* Wait for the timer to fire */
  DWORD wait_result = WaitForSingleObject(timer_handle, INFINITE);
  assert(wait_result == WAIT_OBJECT_0);
  
  /* Clean up */
  CloseHandle(timer_handle);
#endif
#if defined(SIO_OS_POSIX) && !defined(SIO_OS_MACOS)
  printf("Testing POSIX-specific features\n");
  
  /* Test POSIX timer */
  timer_t timer_id;
  err = sio_posix_create_timer(&timer_id, SIGALRM, CLOCK_REALTIME);
  assert(err == SIO_SUCCESS);
  
  /* Set the timer */
  err = sio_posix_set_timer(timer_id, 100, 0);
  assert(err == SIO_SUCCESS);
  
  /* Sleep to allow the timer to fire */
  sio_thread_sleep(200);
  
  /* Clean up */
  timer_delete(timer_id);
  
#endif
  
  printf("OS-specific feature tests passed\n");
  return 0;
}

/**
* @brief Main entry point for the thread test program
*
* @return int 0 on success, non-zero on failure
*/
int main(void) {
  printf("SIO Thread Test\n");
  
  /* Run the tests */
  int result = 0;
  
  result |= test_threads();
  result |= test_mutexes();
  result |= test_condition_variables();
  result |= test_barriers();
  result |= test_semaphores();
  result |= test_threadpool();
  result |= test_atomic_operations();
  result |= test_os_specific_features();
  
  if (result == 0) {
    printf("All tests passed!\n");
  } else {
    printf("Some tests failed!\n");
  }
  
  return result;
}