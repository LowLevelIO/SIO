/**
* @file sio/aux/thread.h
* @brief Simple I/O (SIO) - Cross-platform threading and synchronization primitives
*
* This file provides a unified interface for threads, mutexes, condition variables,
* thread-local storage, and other synchronization primitives across different platforms.
*
* @author zczxy
* @version 0.1.0
*/

#ifndef SIO_THREAD_H
#define SIO_THREAD_H

#ifdef __cplusplus
extern "C" {
#endif

#include <sio/platform.h>
#include <sio/err.h>
#include <stdint.h>
#include <stddef.h>

/* Platform-specific includes */
#if defined(SIO_OS_WINDOWS)
  #include <windows.h>
  #include <process.h>
#elif defined(SIO_OS_POSIX)
  #include <pthread.h>
  #include <sched.h>
  #include <semaphore.h>
  #include <time.h>
  #include <sys/time.h>
  #include <signal.h>
  #include <unistd.h>
  #if defined(SIO_OS_LINUX)
    #include <sys/syscall.h>
    #include <linux/futex.h>
  #endif
#else
  #error "thread.h - Unsupported operating system"
#endif

/**
* @brief Thread attributes
*/
typedef enum sio_thread_attr {
  SIO_THREAD_DEFAULT     = 0,       /**< Default thread attributes */
  SIO_THREAD_DETACHED    = (1 << 0), /**< Create thread in detached state */
  SIO_THREAD_REALTIME    = (1 << 1), /**< Request realtime scheduling (if available) */
  SIO_THREAD_HIGH_PRIO   = (1 << 2), /**< Request high priority scheduling */
  SIO_THREAD_LOW_PRIO    = (1 << 3), /**< Request low priority scheduling */
  SIO_THREAD_AFFINITY    = (1 << 4)  /**< Set CPU affinity (requires additional parameters) */
} sio_thread_attr_t;

/**
* @brief Thread function prototype
*/
typedef void* (*sio_thread_func_t)(void* arg);

/**
* @brief Thread identifier
*/
#if defined(SIO_OS_WINDOWS)
  typedef DWORD sio_thread_id_t;
#elif defined(SIO_OS_POSIX)
  #if defined(SIO_OS_LINUX)
    typedef pid_t sio_thread_id_t;
  #else
    typedef pthread_t sio_thread_id_t;
  #endif
#else
  typedef uintptr_t sio_thread_id_t;
#endif

/**
* @brief Thread structure
*/
typedef struct sio_thread {
#if defined(SIO_OS_WINDOWS)
  HANDLE handle;                 /**< Thread handle */
  DWORD thread_id;               /**< Thread identifier */
  unsigned int (__stdcall *func)(void*); /**< Thread function */
#elif defined(SIO_OS_POSIX)
  pthread_t thread;              /**< Thread identifier */
  pthread_attr_t attr;           /**< Thread attributes */
  int attr_initialized;          /**< Whether attributes have been initialized */
#endif
  sio_thread_func_t func;
  void* arg;                     /**< Thread function argument */
  int detached;                  /**< Whether thread is detached */
  int running;                   /**< Whether thread is running */
} sio_thread_t;

/**
* @brief Mutex structure
*/
typedef struct sio_mutex {
#if defined(SIO_OS_WINDOWS)
  CRITICAL_SECTION cs;           /**< Critical section (fast mutex) */
  HANDLE mutex;                  /**< Mutex handle (for timed functions) */
  int is_cs;                     /**< Whether using critical section */
#elif defined(SIO_OS_POSIX)
  pthread_mutex_t mutex;         /**< Mutex handle */
  pthread_mutexattr_t attr;      /**< Mutex attributes */
  int attr_initialized;          /**< Whether attributes have been initialized */
#endif
  int recursive;                 /**< Whether mutex is recursive */
  int initialized;               /**< Whether mutex is initialized */
} sio_mutex_t;

/**
* @brief Read-write lock structure
*/
typedef struct sio_rwlock {
#if defined(SIO_OS_WINDOWS)
  SRWLOCK rwlock;                /**< Slim reader/writer lock */
#elif defined(SIO_OS_POSIX)
  pthread_rwlock_t rwlock;       /**< Read-write lock */
  pthread_rwlockattr_t attr;     /**< Read-write lock attributes */
  int attr_initialized;          /**< Whether attributes have been initialized */
#endif
  int initialized;               /**< Whether rwlock is initialized */
} sio_rwlock_t;

/**
* @brief Condition variable structure
*/
typedef struct sio_cond {
#if defined(SIO_OS_WINDOWS)
  CONDITION_VARIABLE cond;       /**< Condition variable */
#elif defined(SIO_OS_POSIX)
  pthread_cond_t cond;           /**< Condition variable */
  pthread_condattr_t attr;       /**< Condition variable attributes */
  int attr_initialized;          /**< Whether attributes have been initialized */
#endif
  int initialized;               /**< Whether cond is initialized */
} sio_cond_t;

/**
* @brief Semaphore structure
*/
typedef struct sio_sem {
#if defined(SIO_OS_WINDOWS)
  HANDLE sem;                    /**< Semaphore handle */
#elif defined(SIO_OS_LINUX) || defined(SIO_OS_BSD)
  sem_t sem;                     /**< Semaphore object */
  sem_t *psem;                   /**< For semaphore open */
  char *name;                    /**< Named semaphore (NULL for unnamed) */
  int is_named;                  /**< Whether semaphore is named */
#else /* Fallback implementation */
  sio_mutex_t mutex;             /**< Mutex for fallback implementation */
  sio_cond_t cond;               /**< Condition for fallback implementation */
  unsigned int count;            /**< Current count for fallback implementation */
  unsigned int max_count;        /**< Maximum count for fallback implementation */
#endif
  int initialized;               /**< Whether semaphore is initialized */
} sio_sem_t;

/**
* @brief Barrier structure
*/
typedef struct sio_barrier {
#if defined(SIO_OS_WINDOWS)
  SYNCHRONIZATION_BARRIER barrier; /**< Synchronization barrier (Windows 8+) */
  /* Fallback for older Windows versions */
  CRITICAL_SECTION cs;           /**< Critical section for fallback */
  CONDITION_VARIABLE cv;         /**< Condition variable for fallback */
  unsigned int threshold;        /**< Thread count needed */
  unsigned int count;            /**< Current thread count */
  unsigned int generation;       /**< Current generation (to prevent spurious wakeups) */
#elif defined(SIO_OS_POSIX) && !defined(SIO_OS_MACOS)
  pthread_barrier_t barrier;     /**< POSIX barrier */
  pthread_barrierattr_t attr;    /**< POSIX barrier attributes */
  int attr_initialized;          /**< Whether attributes are initialized */
#else /* Fallback implementation for platforms without native barriers */
  sio_mutex_t mutex;             /**< Mutex for fallback implementation */
  sio_cond_t cond;               /**< Condition for fallback implementation */
  unsigned int threshold;        /**< Thread count needed */
  unsigned int count;            /**< Current thread count */
  unsigned int generation;       /**< Current generation (to prevent spurious wakeups) */
#endif
  int initialized;               /**< Whether barrier is initialized */
} sio_barrier_t;

/**
* @brief Spinlock structure (lightweight, busy-waiting lock)
*/
typedef struct sio_spinlock {
#if defined(SIO_OS_WINDOWS)
  LONG lock;                     /**< Interlocked value for spinlock */
#elif defined(SIO_OS_POSIX) && defined(_POSIX_SPIN_LOCKS)
  pthread_spinlock_t lock;       /**< POSIX spinlock */
#else /* Fallback atomic implementation */
  volatile uint32_t lock;        /**< Atomic flag for fallback */
#endif
  int initialized;               /**< Whether spinlock is initialized */
} sio_spinlock_t;

/**
* @brief Thread-local storage key structure
*/
typedef struct sio_tls_key {
#if defined(SIO_OS_WINDOWS)
  DWORD key;                     /**< TLS index */
#elif defined(SIO_OS_POSIX)
  pthread_key_t key;             /**< TLS key */
#endif
  void (*destructor)(void*);     /**< Value destructor function */
  int initialized;               /**< Whether key is initialized */
} sio_tls_key_t;

/**
* @brief Thread-local storage value structure (for compilers that don't support thread_local)
*/
typedef struct sio_tls_value {
  sio_tls_key_t key;            /**< TLS key */
  void *value;                  /**< Value pointer */
} sio_tls_value_t;

/**
* @brief Thread once control structure (for one-time initialization)
*/
typedef struct sio_once {
#if defined(SIO_OS_WINDOWS)
  LONG state;                    /**< Initialization state */
  LONG lock;                     /**< Lock for initialization */
#elif defined(SIO_OS_POSIX)
  pthread_once_t once;           /**< POSIX once control */
#endif
} sio_once_t;

/**
* @brief Thread once control initializer
*/
#if defined(SIO_OS_WINDOWS)
  #define SIO_ONCE_INIT {0, 0}
#elif defined(SIO_OS_POSIX)
  #define SIO_ONCE_INIT {PTHREAD_ONCE_INIT}
#endif

/**
* @brief Thread local storage macro (for compilers that support it)
*/
#if defined(SIO_COMPILER_GCC) || defined(SIO_COMPILER_CLANG)
  #define SIO_THREAD_LOCAL __thread
#elif defined(SIO_COMPILER_MSVC)
  #define SIO_THREAD_LOCAL __declspec(thread)
#else
  /* Fallback to the TLS API if compiler doesn't support thread-local storage */
  #define SIO_THREAD_LOCAL_UNSUPPORTED
#endif

/**
* @brief Atomic operations for lightweight synchronization
*/
#if defined(SIO_COMPILER_GCC) || defined(SIO_COMPILER_CLANG)
  #define SIO_ATOMIC_ADD(ptr, val) __atomic_add_fetch(ptr, val, __ATOMIC_SEQ_CST)
  #define SIO_ATOMIC_SUB(ptr, val) __atomic_sub_fetch(ptr, val, __ATOMIC_SEQ_CST)
  #define SIO_ATOMIC_INC(ptr) __atomic_add_fetch(ptr, 1, __ATOMIC_SEQ_CST)
  #define SIO_ATOMIC_DEC(ptr) __atomic_sub_fetch(ptr, 1, __ATOMIC_SEQ_CST)
  #define SIO_ATOMIC_CAS(ptr, oldval, newval) __atomic_compare_exchange_n(ptr, &(oldval), newval, 0, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST)
  #define SIO_ATOMIC_STORE(ptr, val) __atomic_store_n(ptr, val, __ATOMIC_SEQ_CST)
  #define SIO_ATOMIC_LOAD(ptr) __atomic_load_n(ptr, __ATOMIC_SEQ_CST)
#elif defined(SIO_COMPILER_MSVC)
  #define SIO_ATOMIC_ADD(ptr, val) InterlockedAdd((LONG volatile*)ptr, val)
  #define SIO_ATOMIC_SUB(ptr, val) InterlockedAdd((LONG volatile*)ptr, -(LONG)(val))
  #define SIO_ATOMIC_INC(ptr) InterlockedIncrement((LONG volatile*)ptr)
  #define SIO_ATOMIC_DEC(ptr) InterlockedDecrement((LONG volatile*)ptr)
  #define SIO_ATOMIC_CAS(ptr, oldval, newval) (InterlockedCompareExchange((LONG volatile*)ptr, newval, oldval) == (LONG)(oldval))
  #define SIO_ATOMIC_STORE(ptr, val) InterlockedExchange((LONG volatile*)ptr, val)
  #define SIO_ATOMIC_LOAD(ptr) InterlockedCompareExchange((LONG volatile*)ptr, 0, 0)
#else
  /* Fallback using mutex for platforms without atomic operations */
  /* These will be implemented in the thread.c file */
  int32_t sio_atomic_add(int32_t volatile *ptr, int32_t val);
  int32_t sio_atomic_sub(int32_t volatile *ptr, int32_t val);
  int32_t sio_atomic_inc(int32_t volatile *ptr);
  int32_t sio_atomic_dec(int32_t volatile *ptr);
  int sio_atomic_cas(int32_t volatile *ptr, int32_t oldval, int32_t newval);
  void sio_atomic_store(int32_t volatile *ptr, int32_t val);
  int32_t sio_atomic_load(int32_t volatile *ptr);
  
  #define SIO_ATOMIC_ADD(ptr, val) sio_atomic_add(ptr, val)
  #define SIO_ATOMIC_SUB(ptr, val) sio_atomic_sub(ptr, val)
  #define SIO_ATOMIC_INC(ptr) sio_atomic_inc(ptr)
  #define SIO_ATOMIC_DEC(ptr) sio_atomic_dec(ptr)
  #define SIO_ATOMIC_CAS(ptr, oldval, newval) sio_atomic_cas(ptr, oldval, newval)
  #define SIO_ATOMIC_STORE(ptr, val) sio_atomic_store(ptr, val)
  #define SIO_ATOMIC_LOAD(ptr) sio_atomic_load(ptr)
#endif

/**
* @brief Memory barrier operations for advanced synchronization
*/
#if defined(SIO_COMPILER_GCC) || defined(SIO_COMPILER_CLANG)
  #define SIO_MEMORY_BARRIER() __atomic_thread_fence(__ATOMIC_SEQ_CST)
  #define SIO_READ_BARRIER() __atomic_thread_fence(__ATOMIC_ACQUIRE)
  #define SIO_WRITE_BARRIER() __atomic_thread_fence(__ATOMIC_RELEASE)
#elif defined(SIO_COMPILER_MSVC)
  #define SIO_MEMORY_BARRIER() MemoryBarrier()
  #define SIO_READ_BARRIER() _ReadBarrier()
  #define SIO_WRITE_BARRIER() _WriteBarrier()
#else
  /* These will be implemented in the thread.c file */
  void sio_memory_barrier(void);
  void sio_read_barrier(void);
  void sio_write_barrier(void);
  
  #define SIO_MEMORY_BARRIER() sio_memory_barrier()
  #define SIO_READ_BARRIER() sio_read_barrier()
  #define SIO_WRITE_BARRIER() sio_write_barrier()
#endif

/*
 * Thread management functions
 */

/**
* @brief Create a new thread
*
* @param thread Pointer to thread structure to initialize
* @param func Thread function
* @param arg Argument to pass to thread function
* @param attr Thread attributes
* @return sio_error_t SIO_SUCCESS or error code
*/
SIO_EXPORT sio_error_t sio_thread_create(sio_thread_t *thread, sio_thread_func_t func, void *arg, sio_thread_attr_t attr);

/**
* @brief Wait for a thread to complete
*
* @param thread Thread to join
* @param result Pointer to store thread result value (can be NULL)
* @return sio_error_t SIO_SUCCESS or error code
*/
SIO_EXPORT sio_error_t sio_thread_join(sio_thread_t *thread, void **result);

/**
* @brief Detach a thread (resources will be freed automatically when thread exits)
*
* @param thread Thread to detach
* @return sio_error_t SIO_SUCCESS or error code
*/
SIO_EXPORT sio_error_t sio_thread_detach(sio_thread_t *thread);

/**
* @brief Get the current thread identifier
*
* @return sio_thread_id_t Thread identifier
*/
SIO_EXPORT sio_thread_id_t sio_thread_get_id(void);

/**
* @brief Compare two thread IDs for equality
*
* @param a First thread ID
* @param b Second thread ID
* @return int Non-zero if equal, 0 if not equal
*/
SIO_EXPORT int sio_thread_id_equal(sio_thread_id_t a, sio_thread_id_t b);

/**
* @brief Set thread's CPU affinity
*
* @param thread Thread to set affinity for (NULL for current thread)
* @param cpu_id CPU index to bind to
* @return sio_error_t SIO_SUCCESS or error code
*/
SIO_EXPORT sio_error_t sio_thread_set_affinity(sio_thread_t *thread, int cpu_id);

/**
* @brief Set thread priority
*
* @param thread Thread to set priority for (NULL for current thread)
* @param priority Priority level (platform-specific)
* @return sio_error_t SIO_SUCCESS or error code
*/
SIO_EXPORT sio_error_t sio_thread_set_priority(sio_thread_t *thread, int priority);

/**
* @brief Yield execution to another thread
*
* @return sio_error_t SIO_SUCCESS or error code
*/
SIO_EXPORT sio_error_t sio_thread_yield(void);

/**
* @brief Pause execution for a specified time
*
* @param milliseconds Time to sleep in milliseconds
* @return sio_error_t SIO_SUCCESS or error code
*/
SIO_EXPORT sio_error_t sio_thread_sleep(uint32_t milliseconds);

/**
* @brief Get the number of hardware threads (logical cores)
*
* @return int Number of hardware threads, or 0 if unknown
*/
SIO_EXPORT int sio_thread_get_hardware_threads(void);

/*
 * Mutex functions
 */

/**
* @brief Initialize a mutex
*
* @param mutex Pointer to mutex structure to initialize
* @param recursive Non-zero to create a recursive mutex, 0 for normal mutex
* @return sio_error_t SIO_SUCCESS or error code
*/
SIO_EXPORT sio_error_t sio_mutex_init(sio_mutex_t *mutex, int recursive);

/**
* @brief Destroy a mutex
*
* @param mutex Mutex to destroy
* @return sio_error_t SIO_SUCCESS or error code
*/
SIO_EXPORT sio_error_t sio_mutex_destroy(sio_mutex_t *mutex);

/**
* @brief Lock a mutex
*
* @param mutex Mutex to lock
* @return sio_error_t SIO_SUCCESS or error code
*/
SIO_EXPORT sio_error_t sio_mutex_lock(sio_mutex_t *mutex);

/**
* @brief Try to lock a mutex without blocking
*
* @param mutex Mutex to lock
* @return sio_error_t SIO_SUCCESS if locked, SIO_ERROR_BUSY if already locked, or other error code
*/
SIO_EXPORT sio_error_t sio_mutex_trylock(sio_mutex_t *mutex);

/**
* @brief Try to lock a mutex with timeout
*
* @param mutex Mutex to lock
* @param timeout_ms Timeout in milliseconds (0 for immediate, -1 for infinite)
* @return sio_error_t SIO_SUCCESS if locked, SIO_ERROR_TIMEOUT if timed out, or other error code
*/
SIO_EXPORT sio_error_t sio_mutex_timedlock(sio_mutex_t *mutex, int32_t timeout_ms);

/**
* @brief Unlock a mutex
*
* @param mutex Mutex to unlock
* @return sio_error_t SIO_SUCCESS or error code
*/
SIO_EXPORT sio_error_t sio_mutex_unlock(sio_mutex_t *mutex);

/*
 * Read-write lock functions
 */

/**
* @brief Initialize a read-write lock
*
* @param rwlock Pointer to rwlock structure to initialize
* @return sio_error_t SIO_SUCCESS or error code
*/
SIO_EXPORT sio_error_t sio_rwlock_init(sio_rwlock_t *rwlock);

/**
* @brief Destroy a read-write lock
*
* @param rwlock Read-write lock to destroy
* @return sio_error_t SIO_SUCCESS or error code
*/
SIO_EXPORT sio_error_t sio_rwlock_destroy(sio_rwlock_t *rwlock);

/**
* @brief Acquire a read-write lock for reading
*
* @param rwlock Read-write lock to acquire
* @return sio_error_t SIO_SUCCESS or error code
*/
SIO_EXPORT sio_error_t sio_rwlock_read_lock(sio_rwlock_t *rwlock);

/**
* @brief Try to acquire a read-write lock for reading without blocking
*
* @param rwlock Read-write lock to acquire
* @return sio_error_t SIO_SUCCESS if locked, SIO_ERROR_BUSY if already locked, or other error code
*/
SIO_EXPORT sio_error_t sio_rwlock_try_read_lock(sio_rwlock_t *rwlock);

/**
* @brief Acquire a read-write lock for writing
*
* @param rwlock Read-write lock to acquire
* @return sio_error_t SIO_SUCCESS or error code
*/
SIO_EXPORT sio_error_t sio_rwlock_write_lock(sio_rwlock_t *rwlock);

/**
* @brief Try to acquire a read-write lock for writing without blocking
*
* @param rwlock Read-write lock to acquire
* @return sio_error_t SIO_SUCCESS if locked, SIO_ERROR_BUSY if already locked, or other error code
*/
SIO_EXPORT sio_error_t sio_rwlock_try_write_lock(sio_rwlock_t *rwlock);

/**
* @brief Release a read-write lock from a read lock
*
* @param rwlock Read-write lock to release
* @return sio_error_t SIO_SUCCESS or error code
*/
SIO_EXPORT sio_error_t sio_rwlock_read_unlock(sio_rwlock_t *rwlock);

/**
* @brief Release a read-write lock from a write lock
*
* @param rwlock Read-write lock to release
* @return sio_error_t SIO_SUCCESS or error code
*/
SIO_EXPORT sio_error_t sio_rwlock_write_unlock(sio_rwlock_t *rwlock);

/*
 * Condition variable functions
 */

/**
* @brief Initialize a condition variable
*
* @param cond Pointer to condition variable structure to initialize
* @return sio_error_t SIO_SUCCESS or error code
*/
SIO_EXPORT sio_error_t sio_cond_init(sio_cond_t *cond);

/**
* @brief Destroy a condition variable
*
* @param cond Condition variable to destroy
* @return sio_error_t SIO_SUCCESS or error code
*/
SIO_EXPORT sio_error_t sio_cond_destroy(sio_cond_t *cond);

/**
* @brief Wait on a condition variable
*
* @param cond Condition variable to wait on
* @param mutex Mutex to atomically release while waiting
* @return sio_error_t SIO_SUCCESS or error code
*/
SIO_EXPORT sio_error_t sio_cond_wait(sio_cond_t *cond, sio_mutex_t *mutex);

/**
* @brief Wait on a condition variable with timeout
*
* @param cond Condition variable to wait on
* @param mutex Mutex to atomically release while waiting
* @param timeout_ms Timeout in milliseconds (0 for immediate, -1 for infinite)
* @return sio_error_t SIO_SUCCESS if signaled, SIO_ERROR_TIMEOUT if timed out, or other error code
*/
SIO_EXPORT sio_error_t sio_cond_timedwait(sio_cond_t *cond, sio_mutex_t *mutex, int32_t timeout_ms);

/**
* @brief Signal a condition variable (wake one waiter)
*
* @param cond Condition variable to signal
* @return sio_error_t SIO_SUCCESS or error code
*/
SIO_EXPORT sio_error_t sio_cond_signal(sio_cond_t *cond);

/**
* @brief Broadcast a condition variable (wake all waiters)
*
* @param cond Condition variable to broadcast
* @return sio_error_t SIO_SUCCESS or error code
*/
SIO_EXPORT sio_error_t sio_cond_broadcast(sio_cond_t *cond);

/*
 * Semaphore functions
 */

/**
* @brief Initialize a semaphore
*
* @param sem Pointer to semaphore structure to initialize
* @param initial_count Initial count value
* @param max_count Maximum count value (0 for unlimited)
* @return sio_error_t SIO_SUCCESS or error code
*/
SIO_EXPORT sio_error_t sio_sem_init(sio_sem_t *sem, unsigned int initial_count, unsigned int max_count);

/**
* @brief Initialize a named semaphore (for inter-process synchronization)
*
* @param sem Pointer to semaphore structure to initialize
* @param name Name of the semaphore (platform-specific format)
* @param initial_count Initial count value
* @param max_count Maximum count value (0 for unlimited)
* @param create_new Non-zero to create new, 0 to open existing
* @return sio_error_t SIO_SUCCESS or error code
*/
SIO_EXPORT sio_error_t sio_sem_init_named(sio_sem_t *sem, const char *name, unsigned int initial_count, unsigned int max_count, int create_new);

/**
* @brief Destroy a semaphore
*
* @param sem Semaphore to destroy
* @return sio_error_t SIO_SUCCESS or error code
*/
SIO_EXPORT sio_error_t sio_sem_destroy(sio_sem_t *sem);

/**
* @brief Wait on a semaphore
*
* @param sem Semaphore to wait on
* @return sio_error_t SIO_SUCCESS or error code
*/
SIO_EXPORT sio_error_t sio_sem_wait(sio_sem_t *sem);

/**
* @brief Try to wait on a semaphore without blocking
*
* @param sem Semaphore to wait on
* @return sio_error_t SIO_SUCCESS if waited, SIO_ERROR_BUSY if would block, or other error code
*/
SIO_EXPORT sio_error_t sio_sem_trywait(sio_sem_t *sem);

/**
* @brief Wait on a semaphore with timeout
*
* @param sem Semaphore to wait on
* @param timeout_ms Timeout in milliseconds (0 for immediate, -1 for infinite)
* @return sio_error_t SIO_SUCCESS if waited, SIO_ERROR_TIMEOUT if timed out, or other error code
*/
SIO_EXPORT sio_error_t sio_sem_timedwait(sio_sem_t *sem, int32_t timeout_ms);

/**
* @brief Post to a semaphore (increment count)
*
* @param sem Semaphore to post to
* @return sio_error_t SIO_SUCCESS or error code
*/
SIO_EXPORT sio_error_t sio_sem_post(sio_sem_t *sem);

/**
* @brief Get the current value of a semaphore
*
* @param sem Semaphore to query
* @param value Pointer to store the current value
* @return sio_error_t SIO_SUCCESS or error code
*/
SIO_EXPORT sio_error_t sio_sem_get_value(sio_sem_t *sem, int *value);

/*
 * Barrier functions
 */

/**
* @brief Initialize a barrier
*
* @param barrier Pointer to barrier structure to initialize
* @param count Number of threads that must reach the barrier before any proceed
* @return sio_error_t SIO_SUCCESS or error code
*/
SIO_EXPORT sio_error_t sio_barrier_init(sio_barrier_t *barrier, unsigned int count);

/**
* @brief Destroy a barrier
*
* @param barrier Barrier to destroy
* @return sio_error_t SIO_SUCCESS or error code
*/
SIO_EXPORT sio_error_t sio_barrier_destroy(sio_barrier_t *barrier);

/**
* @brief Wait at a barrier until all threads reach it
*
* @param barrier Barrier to wait at
* @return sio_error_t SIO_SUCCESS or error code
*/
SIO_EXPORT sio_error_t sio_barrier_wait(sio_barrier_t *barrier);

/*
 * Spinlock functions (lightweight, busy-waiting locks)
 */

/**
* @brief Initialize a spinlock
*
* @param spinlock Pointer to spinlock structure to initialize
* @return sio_error_t SIO_SUCCESS or error code
*/
SIO_EXPORT sio_error_t sio_spinlock_init(sio_spinlock_t *spinlock);

/**
* @brief Destroy a spinlock
*
* @param spinlock Spinlock to destroy
* @return sio_error_t SIO_SUCCESS or error code
*/
SIO_EXPORT sio_error_t sio_spinlock_destroy(sio_spinlock_t *spinlock);

/**
* @brief Acquire a spinlock
*
* @param spinlock Spinlock to acquire
* @return sio_error_t SIO_SUCCESS or error code
*/
SIO_EXPORT sio_error_t sio_spinlock_lock(sio_spinlock_t *spinlock);

/**
* @brief Try to acquire a spinlock without blocking
*
* @param spinlock Spinlock to acquire
* @return sio_error_t SIO_SUCCESS if locked, SIO_ERROR_BUSY if already locked, or other error code
*/
SIO_EXPORT sio_error_t sio_spinlock_trylock(sio_spinlock_t *spinlock);

/**
* @brief Release a spinlock
*
* @param spinlock Spinlock to release
* @return sio_error_t SIO_SUCCESS or error code
*/
SIO_EXPORT sio_error_t sio_spinlock_unlock(sio_spinlock_t *spinlock);

/*
 * Thread-local storage functions
 */

/**
* @brief Create a thread-local storage key
*
* @param key Pointer to TLS key structure to initialize
* @param destructor Function to call on thread exit to clean up value (can be NULL)
* @return sio_error_t SIO_SUCCESS or error code
*/
SIO_EXPORT sio_error_t sio_tls_key_create(sio_tls_key_t *key, void (*destructor)(void*));

/**
* @brief Delete a thread-local storage key
*
* @param key TLS key to delete
* @return sio_error_t SIO_SUCCESS or error code
*/
SIO_EXPORT sio_error_t sio_tls_key_delete(sio_tls_key_t *key);

/**
* @brief Set thread-local value for a key
*
* @param key TLS key to set value for
* @param value Value to set
* @return sio_error_t SIO_SUCCESS or error code
*/
SIO_EXPORT sio_error_t sio_tls_set_value(sio_tls_key_t *key, void *value);

/**
* @brief Get thread-local value for a key
*
* @param key TLS key to get value for
* @param value Pointer to store the current value
* @return sio_error_t SIO_SUCCESS or error code
*/
SIO_EXPORT sio_error_t sio_tls_get_value(sio_tls_key_t *key, void **value);

/* For compilers without thread_local support */
#ifdef SIO_THREAD_LOCAL_UNSUPPORTED
/**
* @brief Initialize a thread-local storage value
*
* @param tls Pointer to TLS value structure to initialize
* @param destructor Function to call on thread exit to clean up value (can be NULL)
* @return sio_error_t SIO_SUCCESS or error code
*/
SIO_EXPORT sio_error_t sio_tls_value_init(sio_tls_value_t *tls, void (*destructor)(void*));

/**
* @brief Clean up a thread-local storage value
*
* @param tls TLS value to clean up
* @return sio_error_t SIO_SUCCESS or error code
*/
SIO_EXPORT sio_error_t sio_tls_value_destroy(sio_tls_value_t *tls);

/**
* @brief Set value for a thread-local storage value
*
* @param tls TLS value to set
* @param value Value to set
* @return sio_error_t SIO_SUCCESS or error code
*/
SIO_EXPORT sio_error_t sio_tls_value_set(sio_tls_value_t *tls, void *value);

/**
* @brief Get value from a thread-local storage value
*
* @param tls TLS value to get
* @return void* Current value
*/
SIO_EXPORT void *sio_tls_value_get(sio_tls_value_t *tls);
#endif

/*
 * Once control functions (for one-time initialization)
 */

/**
* @brief Perform one-time initialization
*
* @param once Pointer to once control structure
* @param func Function to call once
* @return sio_error_t SIO_SUCCESS or error code
*/
SIO_EXPORT sio_error_t sio_once(sio_once_t *once, void (*func)(void));

/*
 * Advanced OS-specific functionality
 */

#if defined(SIO_OS_LINUX)
/**
* @brief Use Linux futex for low-level waiting
*
* @param addr Address to wait on
* @param expected Expected value at addr
* @param timeout_ms Timeout in milliseconds (-1 for infinite)
* @return sio_error_t SIO_SUCCESS if value changed, SIO_ERROR_TIMEOUT if timed out, or other error code
*/
SIO_EXPORT sio_error_t sio_futex_wait(int32_t *addr, int32_t expected, int32_t timeout_ms);

/**
* @brief Wake threads waiting on a futex
*
* @param addr Address to wake waiters on
* @param count Number of waiters to wake (0 for all)
* @return sio_error_t SIO_SUCCESS or error code
*/
SIO_EXPORT sio_error_t sio_futex_wake(int32_t *addr, int32_t count);
#endif

#if defined(SIO_OS_WINDOWS)
/**
* @brief Create a Windows waitable timer
* 
* @param timer_handle Pointer to store created timer handle
* @param manual_reset Non-zero for manual reset, 0 for auto-reset
* @param name Optional name for the timer (can be NULL)
* @return sio_error_t SIO_SUCCESS or error code
*/
SIO_EXPORT sio_error_t sio_win_create_waitable_timer(HANDLE *timer_handle, int manual_reset, const char *name);

/**
* @brief Set a Windows waitable timer
* 
* @param timer_handle Timer handle
* @param due_time Time until timer expires in milliseconds (negative for relative time)
* @param period Period for recurring timer in milliseconds (0 for one-shot)
* @return sio_error_t SIO_SUCCESS or error code
*/
SIO_EXPORT sio_error_t sio_win_set_waitable_timer(HANDLE timer_handle, int64_t due_time, uint32_t period);
#endif

#if defined(SIO_OS_POSIX) && !defined(SIO_OS_MACOS)
/**
* @brief Create a POSIX real-time timer
* 
* @param timer_id Pointer to store created timer ID
* @param signal Signal to use for timer expiration
* @param clock_id Clock ID to use (CLOCK_REALTIME, CLOCK_MONOTONIC, etc.)
* @return sio_error_t SIO_SUCCESS or error code
*/
SIO_EXPORT sio_error_t sio_posix_create_timer(timer_t *timer_id, int signal, clockid_t clock_id);

/**
* @brief Set a POSIX real-time timer
* 
* @param timer_id Timer ID
* @param initial_ms Initial expiration time in milliseconds
* @param interval_ms Interval for recurring timer in milliseconds (0 for one-shot)
* @return sio_error_t SIO_SUCCESS or error code
*/
SIO_EXPORT sio_error_t sio_posix_set_timer(timer_t timer_id, uint32_t initial_ms, uint32_t interval_ms);
#endif

/**
* @brief Process creation flags
*/
typedef enum sio_process_flags {
  SIO_PROCESS_DEFAULT       = 0,        /**< Default process creation flags */
  SIO_PROCESS_DETACHED      = (1 << 0), /**< Detach process (don't wait for exit) */
  SIO_PROCESS_NEW_CONSOLE   = (1 << 1), /**< Create new console (Windows) */
  SIO_PROCESS_INHERIT_ENV   = (1 << 2), /**< Inherit environment variables */
  SIO_PROCESS_REDIRECT_IO   = (1 << 3)  /**< Redirect standard I/O */
} sio_process_flags_t;

/**
* @brief Process information structure
*/
typedef struct sio_process {
#if defined(SIO_OS_WINDOWS)
  PROCESS_INFORMATION process_info;    /**< Windows process information */
  HANDLE stdin_write;                  /**< Write handle for child's stdin */
  HANDLE stdout_read;                  /**< Read handle for child's stdout */
  HANDLE stderr_read;                  /**< Read handle for child's stderr */
#elif defined(SIO_OS_POSIX)
  pid_t pid;                           /**< Process ID */
  int stdin_write;                     /**< Write file descriptor for child's stdin */
  int stdout_read;                     /**< Read file descriptor for child's stdout */
  int stderr_read;                     /**< Read file descriptor for child's stderr */
#endif
  int running;                         /**< Whether process is running */
} sio_process_t;

/**
* @brief Create a new process
*
* @param process Pointer to process structure to initialize
* @param executable Path to executable
* @param args Array of arguments (NULL-terminated)
* @param flags Process creation flags
* @return sio_error_t SIO_SUCCESS or error code
*/
SIO_EXPORT sio_error_t sio_process_create(sio_process_t *process, const char *executable, 
                                      const char **args, sio_process_flags_t flags);

/**
* @brief Wait for a process to complete
*
* @param process Process to wait for
* @param exit_code Pointer to store exit code (can be NULL)
* @param timeout_ms Timeout in milliseconds (-1 for infinite)
* @return sio_error_t SIO_SUCCESS, SIO_ERROR_TIMEOUT, or other error code
*/
SIO_EXPORT sio_error_t sio_process_wait(sio_process_t *process, int *exit_code, int32_t timeout_ms);

/**
* @brief Terminate a process
*
* @param process Process to terminate
* @param exit_code Exit code to set for the process
* @return sio_error_t SIO_SUCCESS or error code
*/
SIO_EXPORT sio_error_t sio_process_terminate(sio_process_t *process, int exit_code);

/**
* @brief Cleanup process resources
*
* @param process Process to clean up
* @return sio_error_t SIO_SUCCESS or error code
*/
SIO_EXPORT sio_error_t sio_process_destroy(sio_process_t *process);

/**
* @brief Write to process stdin
*
* @param process Process to write to
* @param buffer Data to write
* @param size Size of data to write
* @param bytes_written Pointer to store number of bytes written (can be NULL)
* @return sio_error_t SIO_SUCCESS or error code
*/
SIO_EXPORT sio_error_t sio_process_write(sio_process_t *process, const void *buffer, 
                                     size_t size, size_t *bytes_written);

/**
* @brief Read from process stdout
*
* @param process Process to read from
* @param buffer Buffer to read into
* @param size Size of buffer
* @param bytes_read Pointer to store number of bytes read (can be NULL)
* @return sio_error_t SIO_SUCCESS or error code
*/
SIO_EXPORT sio_error_t sio_process_read_stdout(sio_process_t *process, void *buffer, 
                                          size_t size, size_t *bytes_read);

/**
* @brief Read from process stderr
*
* @param process Process to read from
* @param buffer Buffer to read into
* @param size Size of buffer
* @param bytes_read Pointer to store number of bytes read (can be NULL)
* @return sio_error_t SIO_SUCCESS or error code
*/
SIO_EXPORT sio_error_t sio_process_read_stderr(sio_process_t *process, void *buffer, 
                                          size_t size, size_t *bytes_read);

/**
* @brief Thread pool task function prototype
*/
typedef void (*sio_threadpool_task_func_t)(void* arg);

/**
* @brief Thread pool structure
*/
typedef struct sio_threadpool {
  sio_thread_t *threads;               /**< Array of worker threads */
  size_t thread_count;                 /**< Number of worker threads */
  
  /* Task queue */
  struct {
    sio_threadpool_task_func_t func;   /**< Task function */
    void *arg;                         /**< Task argument */
  } *tasks;
  
  size_t task_capacity;                /**< Maximum number of queued tasks */
  size_t task_count;                   /**< Current number of tasks in queue */
  size_t task_head;                    /**< Index of first task in queue */
  size_t task_tail;                    /**< Index where to add next task */
  
  /* Synchronization primitives */
  sio_mutex_t lock;                    /**< Mutex for task queue access */
  sio_cond_t not_empty;                /**< Condition for tasks available */
  sio_cond_t not_full;                 /**< Condition for space available */
  
  int shutdown;                        /**< Flag indicating shutdown */
  int paused;                          /**< Flag indicating pause state */
} sio_threadpool_t;

/**
* @brief Create a thread pool
*
* @param pool Pointer to thread pool structure to initialize
* @param thread_count Number of worker threads to create
* @param task_capacity Maximum number of queued tasks
* @return sio_error_t SIO_SUCCESS or error code
*/
SIO_EXPORT sio_error_t sio_threadpool_create(sio_threadpool_t *pool, 
                                      size_t thread_count, size_t task_capacity);

/**
* @brief Destroy a thread pool
*
* @param pool Thread pool to destroy
* @param finish_tasks Non-zero to finish queued tasks before shutdown, 0 to discard them
* @return sio_error_t SIO_SUCCESS or error code
*/
SIO_EXPORT sio_error_t sio_threadpool_destroy(sio_threadpool_t *pool, int finish_tasks);

/**
* @brief Add a task to the thread pool
*
* @param pool Thread pool to add task to
* @param func Task function to execute
* @param arg Argument to pass to task function
* @param wait_if_full Non-zero to wait if queue is full, 0 to return error
* @return sio_error_t SIO_SUCCESS or error code
*/
SIO_EXPORT sio_error_t sio_threadpool_add_task(sio_threadpool_t *pool, 
                                        sio_threadpool_task_func_t func, void *arg, 
                                        int wait_if_full);

/**
* @brief Pause all worker threads in the pool
*
* @param pool Thread pool to pause
* @return sio_error_t SIO_SUCCESS or error code
*/
SIO_EXPORT sio_error_t sio_threadpool_pause(sio_threadpool_t *pool);

/**
* @brief Resume all worker threads in the pool
*
* @param pool Thread pool to resume
* @return sio_error_t SIO_SUCCESS or error code
*/
SIO_EXPORT sio_error_t sio_threadpool_resume(sio_threadpool_t *pool);

/**
* @brief Get the number of threads in the thread pool
*
* @param pool Thread pool to query
* @return size_t Number of worker threads
*/
SIO_EXPORT size_t sio_threadpool_get_thread_count(const sio_threadpool_t *pool);

/**
* @brief Get the number of pending tasks in the thread pool
*
* @param pool Thread pool to query
* @return size_t Number of pending tasks
*/
SIO_EXPORT size_t sio_threadpool_get_pending_tasks(const sio_threadpool_t *pool);

#ifdef __cplusplus
}
#endif

#endif /* SIO_THREAD_H */