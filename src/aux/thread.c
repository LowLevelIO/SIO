/**
* @file src/aux/thread.c
* @brief Implementation of thread and synchronization primitives
*
* This file implements the cross-platform threading functionality declared in sio/thread.h
* 
* @author zczxy
* @version 0.1.0
*/

#include <sio/aux/thread.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <limits.h>
#include <time.h>

#if defined(SIO_OS_POSIX)
#include <fcntl.h>
#endif

/* Thread function wrapper for Windows */
#if defined(SIO_OS_WINDOWS)
unsigned int __stdcall sio_thread_start_routine(void *arg) {
  sio_thread_t *thread = (sio_thread_t*)arg;
  void *result = ((sio_thread_func_t)thread->func)(thread->arg);
  
  /* Mark thread as not running */
  thread->running = 0;
  
  /* If detached, free the thread structure */
  if (thread->detached) {
    CloseHandle(thread->handle);
    free(thread);
  }
  
  return (unsigned int)(uintptr_t)result;
}
#endif

/* Thread function wrapper for POSIX */
#if defined(SIO_OS_POSIX)
void *sio_thread_start_routine(void *arg) {
  sio_thread_t *thread = (sio_thread_t*)arg;
  void *result = ((sio_thread_func_t)thread->func)(thread->arg);
  
  /* Mark thread as not running */
  thread->running = 0;
  
  /* If detached, free the thread structure */
  if (thread->detached) {
    if (thread->attr_initialized) {
      pthread_attr_destroy(&thread->attr);
    }
    free(thread);
  }
  
  return result;
}
#endif

sio_error_t sio_thread_create(sio_thread_t *thread, sio_thread_func_t func, void *arg, sio_thread_attr_t attr) {
  if (!thread || !func) {
    return SIO_ERROR_PARAM;
  }
  
  /* Initialize thread structure */
  memset(thread, 0, sizeof(sio_thread_t));
  thread->func = (void*)func;
  thread->arg = arg;
  thread->running = 1;
  
  /* Check if thread should be detached */
  if (attr & SIO_THREAD_DETACHED) {
    thread->detached = 1;
  }
  
#if defined(SIO_OS_WINDOWS)
  /* Windows thread creation */
  thread->handle = (HANDLE)_beginthreadex(NULL, 0, sio_thread_start_routine, thread, 0, (unsigned int*)&thread->thread_id);
  
  if (!thread->handle) {
    return sio_get_last_error();
  }
  
  /* Apply thread attributes */
  if (attr & SIO_THREAD_HIGH_PRIO) {
    SetThreadPriority(thread->handle, THREAD_PRIORITY_ABOVE_NORMAL);
  } else if (attr & SIO_THREAD_LOW_PRIO) {
    SetThreadPriority(thread->handle, THREAD_PRIORITY_BELOW_NORMAL);
  }
  
  if (attr & SIO_THREAD_REALTIME) {
    SetThreadPriority(thread->handle, THREAD_PRIORITY_TIME_CRITICAL);
  }
  
#elif defined(SIO_OS_POSIX)
  /* Initialize thread attributes */
  if (pthread_attr_init(&thread->attr) != 0) {
    return sio_posix_error_to_sio_error(errno);
  }
  thread->attr_initialized = 1;
  
  /* Set detached state if requested */
  if (thread->detached) {
    if (pthread_attr_setdetachstate(&thread->attr, PTHREAD_CREATE_DETACHED) != 0) {
      pthread_attr_destroy(&thread->attr);
      thread->attr_initialized = 0;
      return sio_posix_error_to_sio_error(errno);
    }
  }
  
  /* Apply thread attributes */
  if (attr & (SIO_THREAD_HIGH_PRIO | SIO_THREAD_LOW_PRIO | SIO_THREAD_REALTIME)) {
    int policy;
    struct sched_param param;
    
    /* Get current scheduling policy and priority */
    if (pthread_attr_getschedpolicy(&thread->attr, &policy) != 0) {
      pthread_attr_destroy(&thread->attr);
      thread->attr_initialized = 0;
      return sio_posix_error_to_sio_error(errno);
    }
    
    if (pthread_attr_getschedparam(&thread->attr, &param) != 0) {
      pthread_attr_destroy(&thread->attr);
      thread->attr_initialized = 0;
      return sio_posix_error_to_sio_error(errno);
    }
    
    /* Set scheduling policy based on attributes */
    if (attr & SIO_THREAD_REALTIME) {
      policy = SCHED_RR; /* Round-robin real-time scheduling */
      param.sched_priority = sched_get_priority_max(policy) / 2;
    } else {
      policy = SCHED_OTHER; /* Normal scheduling */
      
      if (attr & SIO_THREAD_HIGH_PRIO) {
        param.sched_priority = sched_get_priority_max(policy);
      } else if (attr & SIO_THREAD_LOW_PRIO) {
        param.sched_priority = sched_get_priority_min(policy);
      }
    }
    
    /* Apply scheduling policy and priority */
    if (pthread_attr_setschedpolicy(&thread->attr, policy) != 0) {
      /* Some systems may not support setting policy, just continue */
    }
    
    if (pthread_attr_setschedparam(&thread->attr, &param) != 0) {
      /* Some systems may not support setting priority, just continue */
    }
  }
  
  /* Create the thread */
  if (pthread_create(&thread->thread, &thread->attr, sio_thread_start_routine, thread) != 0) {
    pthread_attr_destroy(&thread->attr);
    thread->attr_initialized = 0;
    return sio_posix_error_to_sio_error(errno);
  }
#endif
  
  return SIO_SUCCESS;
}

sio_error_t sio_thread_join(sio_thread_t *thread, void **result) {
  if (!thread) {
    return SIO_ERROR_PARAM;
  }
  
  if (thread->detached) {
    return SIO_ERROR_THREAD_DETACH;
  }
  
#if defined(SIO_OS_WINDOWS)
  DWORD wait_result = WaitForSingleObject(thread->handle, INFINITE);
  
  if (wait_result == WAIT_FAILED) {
    return sio_get_last_error();
  }
  
  if (result) {
    DWORD exit_code;
    if (!GetExitCodeThread(thread->handle, &exit_code)) {
      return sio_get_last_error();
    }
    *result = (void*)(uintptr_t)exit_code;
  }
  
  CloseHandle(thread->handle);
  thread->handle = NULL;
  
#elif defined(SIO_OS_POSIX)
  void *thread_result;
  int ret = pthread_join(thread->thread, &thread_result);
  
  if (ret != 0) {
    return sio_posix_error_to_sio_error(ret);
  }
  
  if (result) {
    *result = thread_result;
  }
  
  if (thread->attr_initialized) {
    pthread_attr_destroy(&thread->attr);
    thread->attr_initialized = 0;
  }
#endif
  
  return SIO_SUCCESS;
}

sio_error_t sio_thread_detach(sio_thread_t *thread) {
  if (!thread) {
    return SIO_ERROR_PARAM;
  }
  
  if (thread->detached) {
    return SIO_ERROR_THREAD_DETACH;
  }
  
#if defined(SIO_OS_WINDOWS)
  CloseHandle(thread->handle);
  thread->handle = NULL;
  
#elif defined(SIO_OS_POSIX)
  int ret = pthread_detach(thread->thread);
  
  if (ret != 0) {
    return sio_posix_error_to_sio_error(ret);
  }
  
  if (thread->attr_initialized) {
    pthread_attr_destroy(&thread->attr);
    thread->attr_initialized = 0;
  }
#endif
  
  thread->detached = 1;
  return SIO_SUCCESS;
}

sio_thread_id_t sio_thread_get_id(void) {
#if defined(SIO_OS_WINDOWS)
  return GetCurrentThreadId();
#elif defined(SIO_OS_LINUX)
  return syscall(SYS_gettid); /* Use syscall for Linux to get kernel thread ID */
#elif defined(SIO_OS_POSIX)
  return pthread_self();
#else
  return 0;
#endif
}

int sio_thread_id_equal(sio_thread_id_t a, sio_thread_id_t b) {
#if defined(SIO_OS_WINDOWS) || defined(SIO_OS_LINUX)
  return a == b;
#elif defined(SIO_OS_POSIX)
  return pthread_equal(a, b);
#else
  return a == b;
#endif
}

sio_error_t sio_thread_set_affinity(sio_thread_t *thread, int cpu_id) {
  if (cpu_id < 0) {
    return SIO_ERROR_PARAM;
  }
  
#if defined(SIO_OS_WINDOWS)
  HANDLE thread_handle;
  
  if (thread) {
    thread_handle = thread->handle;
  } else {
    thread_handle = GetCurrentThread();
  }
  
  DWORD_PTR mask = (DWORD_PTR)(1ULL << cpu_id);
  if (!SetThreadAffinityMask(thread_handle, mask)) {
    return sio_get_last_error();
  }
  
#elif defined(SIO_OS_LINUX)
  pthread_t thread_handle;
  
  if (thread) {
    thread_handle = thread->thread;
  } else {
    thread_handle = pthread_self();
  }
  
  cpu_set_t cpuset;
  CPU_ZERO(&cpuset);
  CPU_SET(cpu_id, &cpuset);
  
  int ret = pthread_setaffinity_np(thread_handle, sizeof(cpu_set_t), &cpuset);
  if (ret != 0) {
    return sio_posix_error_to_sio_error(ret);
  }
  
#elif defined(SIO_OS_POSIX)
  /* Affinity setting not available on all POSIX systems */
  return SIO_ERROR_UNSUPPORTED;
#else
  return SIO_ERROR_UNSUPPORTED;
#endif
  
  return SIO_SUCCESS;
}

sio_error_t sio_thread_set_priority(sio_thread_t *thread, int priority) {
#if defined(SIO_OS_WINDOWS)
  HANDLE thread_handle;
  
  if (thread) {
    thread_handle = thread->handle;
  } else {
    thread_handle = GetCurrentThread();
  }
  
  if (!SetThreadPriority(thread_handle, priority)) {
    return sio_get_last_error();
  }
  
#elif defined(SIO_OS_POSIX)
  pthread_t thread_handle;
  
  if (thread) {
    thread_handle = thread->thread;
  } else {
    thread_handle = pthread_self();
  }
  
  int policy;
  struct sched_param param;
  
  int ret = pthread_getschedparam(thread_handle, &policy, &param);
  if (ret != 0) {
    return sio_posix_error_to_sio_error(ret);
  }
  
  param.sched_priority = priority;
  
  ret = pthread_setschedparam(thread_handle, policy, &param);
  if (ret != 0) {
    return sio_posix_error_to_sio_error(ret);
  }
#else
  return SIO_ERROR_UNSUPPORTED;
#endif
  
  return SIO_SUCCESS;
}

sio_error_t sio_thread_yield(void) {
#if defined(SIO_OS_WINDOWS)
  SwitchToThread();
#elif defined(SIO_OS_POSIX)
  sched_yield();
#endif
  
  return SIO_SUCCESS;
}

sio_error_t sio_thread_sleep(uint32_t milliseconds) {
#if defined(SIO_OS_WINDOWS)
  Sleep(milliseconds);
#elif defined(SIO_OS_POSIX)
  struct timespec ts;
  ts.tv_sec = milliseconds / 1000;
  ts.tv_nsec = (milliseconds % 1000) * 1000000;
  
  while (nanosleep(&ts, &ts) == -1) {
    if (errno != EINTR) {
      return sio_posix_error_to_sio_error(errno);
    }
  }
#endif
  
  return SIO_SUCCESS;
}

int sio_thread_get_hardware_threads(void) {
#if defined(SIO_OS_WINDOWS)
  SYSTEM_INFO sysinfo;
  GetSystemInfo(&sysinfo);
  return (int)sysinfo.dwNumberOfProcessors;
#elif defined(SIO_OS_POSIX)
  long nprocs = -1;
  
  #if defined(_SC_NPROCESSORS_ONLN)
    nprocs = sysconf(_SC_NPROCESSORS_ONLN);
  #elif defined(_SC_NPROC_ONLN)
    nprocs = sysconf(_SC_NPROC_ONLN);
  #elif defined(SIO_OS_BSD)
    int mib[4];
    size_t len = sizeof(nprocs);
    
    mib[0] = CTL_HW;
    mib[1] = HW_AVAILCPU;
    sysctl(mib, 2, &nprocs, &len, NULL, 0);
    
    if (nprocs < 1) {
      mib[1] = HW_NCPU;
      sysctl(mib, 2, &nprocs, &len, NULL, 0);
    }
  #endif
  
  return (nprocs > 0) ? (int)nprocs : 0;
#else
  return 0;
#endif
}

/*
 * Mutex implementation
 */

sio_error_t sio_mutex_init(sio_mutex_t *mutex, int recursive) {
  if (!mutex) {
    return SIO_ERROR_PARAM;
  }
  
  /* Initialize mutex structure */
  memset(mutex, 0, sizeof(sio_mutex_t));
  mutex->recursive = recursive;
  
#if defined(SIO_OS_WINDOWS)
  /* For Windows, prefer critical sections for better performance */
  mutex->is_cs = 1;
  InitializeCriticalSection(&mutex->cs);
  
  /* Also create a mutex handle for timed operations */
  mutex->mutex = CreateMutexA(NULL, FALSE, NULL);
  if (!mutex->mutex) {
    DeleteCriticalSection(&mutex->cs);
    return sio_get_last_error();
  }
  
#elif defined(SIO_OS_POSIX)
  /* Initialize mutex attributes if recursive mutex requested */
  if (recursive) {
    if (pthread_mutexattr_init(&mutex->attr) != 0) {
      return sio_posix_error_to_sio_error(errno);
    }
    mutex->attr_initialized = 1;
    
    if (pthread_mutexattr_settype(&mutex->attr, PTHREAD_MUTEX_RECURSIVE) != 0) {
      pthread_mutexattr_destroy(&mutex->attr);
      mutex->attr_initialized = 0;
      return sio_posix_error_to_sio_error(errno);
    }
    
    if (pthread_mutex_init(&mutex->mutex, &mutex->attr) != 0) {
      pthread_mutexattr_destroy(&mutex->attr);
      mutex->attr_initialized = 0;
      return sio_posix_error_to_sio_error(errno);
    }
  } else {
    /* Standard mutex */
    if (pthread_mutex_init(&mutex->mutex, NULL) != 0) {
      return sio_posix_error_to_sio_error(errno);
    }
  }
#endif
  
  mutex->initialized = 1;
  return SIO_SUCCESS;
}

sio_error_t sio_mutex_destroy(sio_mutex_t *mutex) {
  if (!mutex || !mutex->initialized) {
    return SIO_ERROR_PARAM;
  }
  
#if defined(SIO_OS_WINDOWS)
  DeleteCriticalSection(&mutex->cs);
  
  if (mutex->mutex) {
    CloseHandle(mutex->mutex);
    mutex->mutex = NULL;
  }
  
#elif defined(SIO_OS_POSIX)
  int ret = pthread_mutex_destroy(&mutex->mutex);
  
  if (ret != 0) {
    return sio_posix_error_to_sio_error(ret);
  }
  
  if (mutex->attr_initialized) {
    pthread_mutexattr_destroy(&mutex->attr);
    mutex->attr_initialized = 0;
  }
#endif
  
  mutex->initialized = 0;
  return SIO_SUCCESS;
}

sio_error_t sio_mutex_lock(sio_mutex_t *mutex) {
  if (!mutex || !mutex->initialized) {
    return SIO_ERROR_PARAM;
  }
  
#if defined(SIO_OS_WINDOWS)
  if (mutex->is_cs) {
    EnterCriticalSection(&mutex->cs);
  } else {
    DWORD wait_result = WaitForSingleObject(mutex->mutex, INFINITE);
    
    if (wait_result == WAIT_FAILED) {
      return sio_get_last_error();
    }
  }
  
#elif defined(SIO_OS_POSIX)
  int ret = pthread_mutex_lock(&mutex->mutex);
  
  if (ret != 0) {
    return sio_posix_error_to_sio_error(ret);
  }
#endif
  
  return SIO_SUCCESS;
}

sio_error_t sio_mutex_trylock(sio_mutex_t *mutex) {
  if (!mutex || !mutex->initialized) {
    return SIO_ERROR_PARAM;
  }
  
#if defined(SIO_OS_WINDOWS)
  if (mutex->is_cs) {
    if (!TryEnterCriticalSection(&mutex->cs)) {
      return SIO_ERROR_BUSY;
    }
  } else {
    DWORD wait_result = WaitForSingleObject(mutex->mutex, 0);
    
    if (wait_result == WAIT_TIMEOUT) {
      return SIO_ERROR_BUSY;
    } else if (wait_result == WAIT_FAILED) {
      return sio_get_last_error();
    }
  }
  
#elif defined(SIO_OS_POSIX)
  int ret = pthread_mutex_trylock(&mutex->mutex);
  
  if (ret == EBUSY) {
    return SIO_ERROR_BUSY;
  } else if (ret != 0) {
    return sio_posix_error_to_sio_error(ret);
  }
#endif
  
  return SIO_SUCCESS;
}

sio_error_t sio_mutex_timedlock(sio_mutex_t *mutex, int32_t timeout_ms) {
  if (!mutex || !mutex->initialized) {
    return SIO_ERROR_PARAM;
  }
  
  /* Handle special cases */
  if (timeout_ms == 0) {
    return sio_mutex_trylock(mutex);
  } else if (timeout_ms < 0) {
    return sio_mutex_lock(mutex);
  }
  
#if defined(SIO_OS_WINDOWS)
  /* For Windows, we need to use mutex handle, not critical section */
  DWORD wait_result = WaitForSingleObject(mutex->mutex, (DWORD)timeout_ms);
  
  if (wait_result == WAIT_TIMEOUT) {
    return SIO_ERROR_TIMEOUT;
  } else if (wait_result == WAIT_FAILED) {
    return sio_get_last_error();
  }
  
#elif defined(SIO_OS_POSIX)
  #if defined(_POSIX_TIMEOUTS) && _POSIX_TIMEOUTS >= 200112L
    /* Use pthread_mutex_timedlock if available */
    struct timespec ts;
    
    #if defined(CLOCK_REALTIME)
      clock_gettime(CLOCK_REALTIME, &ts);
    #else
      struct timeval tv;
      gettimeofday(&tv, NULL);
      ts.tv_sec = tv.tv_sec;
      ts.tv_nsec = tv.tv_usec * 1000;
    #endif
    
    /* Add timeout to current time */
    ts.tv_sec += timeout_ms / 1000;
    ts.tv_nsec += (timeout_ms % 1000) * 1000000;
    
    /* Handle nanosecond overflow */
    if (ts.tv_nsec >= 1000000000) {
      ts.tv_sec += 1;
      ts.tv_nsec -= 1000000000;
    }
    
    int ret = pthread_mutex_timedlock(&mutex->mutex, &ts);
    
    if (ret == ETIMEDOUT) {
      return SIO_ERROR_TIMEOUT;
    } else if (ret != 0) {
      return sio_posix_error_to_sio_error(ret);
    }
  #else
    /* Fallback implementation using polling */
    struct timespec ts;
    ts.tv_sec = 0;
    ts.tv_nsec = 1000000; /* 1ms */
    
    uint32_t elapsed = 0;
    
    while (elapsed < timeout_ms) {
      sio_error_t err = sio_mutex_trylock(mutex);
      
      if (err == SIO_SUCCESS) {
        return SIO_SUCCESS;
      } else if (err != SIO_ERROR_BUSY) {
        return err;
      }
      
      /* Sleep for a short time to avoid tight spinning */
      nanosleep(&ts, NULL);
      elapsed += 1;
    }
    
    return SIO_ERROR_TIMEOUT;
  #endif
#endif
  
  return SIO_SUCCESS;
}

sio_error_t sio_mutex_unlock(sio_mutex_t *mutex) {
  if (!mutex || !mutex->initialized) {
    return SIO_ERROR_PARAM;
  }
  
#if defined(SIO_OS_WINDOWS)
  if (mutex->is_cs) {
    LeaveCriticalSection(&mutex->cs);
  } else {
    if (!ReleaseMutex(mutex->mutex)) {
      return sio_get_last_error();
    }
  }
  
#elif defined(SIO_OS_POSIX)
  int ret = pthread_mutex_unlock(&mutex->mutex);
  
  if (ret != 0) {
    return sio_posix_error_to_sio_error(ret);
  }
#endif
  
  return SIO_SUCCESS;
}

/*
 * Read-write lock implementation
 */

sio_error_t sio_rwlock_init(sio_rwlock_t *rwlock) {
  if (!rwlock) {
    return SIO_ERROR_PARAM;
  }
  
  /* Initialize rwlock structure */
  memset(rwlock, 0, sizeof(sio_rwlock_t));
  
#if defined(SIO_OS_WINDOWS)
  InitializeSRWLock(&rwlock->rwlock);
  
#elif defined(SIO_OS_POSIX)
  int ret = pthread_rwlockattr_init(&rwlock->attr);
  if (ret != 0) {
    return sio_posix_error_to_sio_error(ret);
  }
  rwlock->attr_initialized = 1;
  
  /* Make rwlock prefer writers to avoid starvation */
  #if defined(PTHREAD_RWLOCK_PREFER_WRITER_NONRECURSIVE_NP)
    pthread_rwlockattr_setkind_np(&rwlock->attr, PTHREAD_RWLOCK_PREFER_WRITER_NONRECURSIVE_NP);
  #endif
  
  ret = pthread_rwlock_init(&rwlock->rwlock, &rwlock->attr);
  if (ret != 0) {
    pthread_rwlockattr_destroy(&rwlock->attr);
    rwlock->attr_initialized = 0;
    return sio_posix_error_to_sio_error(ret);
  }
#endif
  
  rwlock->initialized = 1;
  return SIO_SUCCESS;
}

sio_error_t sio_rwlock_destroy(sio_rwlock_t *rwlock) {
  if (!rwlock || !rwlock->initialized) {
    return SIO_ERROR_PARAM;
  }
  
#if defined(SIO_OS_WINDOWS)
  /* SRWLock doesn't need explicit destruction */
  
#elif defined(SIO_OS_POSIX)
  int ret = pthread_rwlock_destroy(&rwlock->rwlock);
  
  if (ret != 0) {
    return sio_posix_error_to_sio_error(ret);
  }
  
  if (rwlock->attr_initialized) {
    pthread_rwlockattr_destroy(&rwlock->attr);
    rwlock->attr_initialized = 0;
  }
#endif
  
  rwlock->initialized = 0;
  return SIO_SUCCESS;
}

sio_error_t sio_rwlock_read_lock(sio_rwlock_t *rwlock) {
  if (!rwlock || !rwlock->initialized) {
    return SIO_ERROR_PARAM;
  }
  
#if defined(SIO_OS_WINDOWS)
  AcquireSRWLockShared(&rwlock->rwlock);
  
#elif defined(SIO_OS_POSIX)
  int ret = pthread_rwlock_rdlock(&rwlock->rwlock);
  
  if (ret != 0) {
    return sio_posix_error_to_sio_error(ret);
  }
#endif
  
  return SIO_SUCCESS;
}

sio_error_t sio_rwlock_try_read_lock(sio_rwlock_t *rwlock) {
  if (!rwlock || !rwlock->initialized) {
    return SIO_ERROR_PARAM;
  }
  
#if defined(SIO_OS_WINDOWS)
  if (!TryAcquireSRWLockShared(&rwlock->rwlock)) {
    return SIO_ERROR_BUSY;
  }
  
#elif defined(SIO_OS_POSIX)
  int ret = pthread_rwlock_tryrdlock(&rwlock->rwlock);
  
  if (ret == EBUSY) {
    return SIO_ERROR_BUSY;
  } else if (ret != 0) {
    return sio_posix_error_to_sio_error(ret);
  }
#endif
  
  return SIO_SUCCESS;
}

sio_error_t sio_rwlock_write_lock(sio_rwlock_t *rwlock) {
  if (!rwlock || !rwlock->initialized) {
    return SIO_ERROR_PARAM;
  }
  
#if defined(SIO_OS_WINDOWS)
  AcquireSRWLockExclusive(&rwlock->rwlock);
  
#elif defined(SIO_OS_POSIX)
  int ret = pthread_rwlock_wrlock(&rwlock->rwlock);
  
  if (ret != 0) {
    return sio_posix_error_to_sio_error(ret);
  }
#endif
  
  return SIO_SUCCESS;
}

sio_error_t sio_rwlock_try_write_lock(sio_rwlock_t *rwlock) {
  if (!rwlock || !rwlock->initialized) {
    return SIO_ERROR_PARAM;
  }
  
#if defined(SIO_OS_WINDOWS)
  if (!TryAcquireSRWLockExclusive(&rwlock->rwlock)) {
    return SIO_ERROR_BUSY;
  }
  
#elif defined(SIO_OS_POSIX)
  int ret = pthread_rwlock_trywrlock(&rwlock->rwlock);
  
  if (ret == EBUSY) {
    return SIO_ERROR_BUSY;
  } else if (ret != 0) {
    return sio_posix_error_to_sio_error(ret);
  }
#endif
  
  return SIO_SUCCESS;
}

sio_error_t sio_rwlock_read_unlock(sio_rwlock_t *rwlock) {
  if (!rwlock || !rwlock->initialized) {
    return SIO_ERROR_PARAM;
  }
  
#if defined(SIO_OS_WINDOWS)
  ReleaseSRWLockShared(&rwlock->rwlock);
  
#elif defined(SIO_OS_POSIX)
  int ret = pthread_rwlock_unlock(&rwlock->rwlock);
  
  if (ret != 0) {
    return sio_posix_error_to_sio_error(ret);
  }
#endif
  
  return SIO_SUCCESS;
}

sio_error_t sio_rwlock_write_unlock(sio_rwlock_t *rwlock) {
  if (!rwlock || !rwlock->initialized) {
    return SIO_ERROR_PARAM;
  }
  
#if defined(SIO_OS_WINDOWS)
  ReleaseSRWLockExclusive(&rwlock->rwlock);
  
#elif defined(SIO_OS_POSIX)
  int ret = pthread_rwlock_unlock(&rwlock->rwlock);
  
  if (ret != 0) {
    return sio_posix_error_to_sio_error(ret);
  }
#endif
  
  return SIO_SUCCESS;
}

/*
 * Condition variable implementation
 */

sio_error_t sio_cond_init(sio_cond_t *cond) {
  if (!cond) {
    return SIO_ERROR_PARAM;
  }
  
  /* Initialize condition variable structure */
  memset(cond, 0, sizeof(sio_cond_t));
  
#if defined(SIO_OS_WINDOWS)
  InitializeConditionVariable(&cond->cond);
  
#elif defined(SIO_OS_POSIX)
  int ret = pthread_condattr_init(&cond->attr);
  if (ret != 0) {
    return sio_posix_error_to_sio_error(ret);
  }
  cond->attr_initialized = 1;
  
  /* Use monotonic clock if available for better timeout handling */
  #if defined(_POSIX_TIMERS) && _POSIX_TIMERS > 0 && defined(CLOCK_MONOTONIC)
    pthread_condattr_setclock(&cond->attr, CLOCK_MONOTONIC);
  #endif
  
  ret = pthread_cond_init(&cond->cond, &cond->attr);
  if (ret != 0) {
    pthread_condattr_destroy(&cond->attr);
    cond->attr_initialized = 0;
    return sio_posix_error_to_sio_error(ret);
  }
#endif
  
  cond->initialized = 1;
  return SIO_SUCCESS;
}

sio_error_t sio_cond_destroy(sio_cond_t *cond) {
  if (!cond || !cond->initialized) {
    return SIO_ERROR_PARAM;
  }
  
#if defined(SIO_OS_WINDOWS)
  /* Windows condition variables don't need explicit destruction */
  
#elif defined(SIO_OS_POSIX)
  int ret = pthread_cond_destroy(&cond->cond);
  
  if (ret != 0) {
    return sio_posix_error_to_sio_error(ret);
  }
  
  if (cond->attr_initialized) {
    pthread_condattr_destroy(&cond->attr);
    cond->attr_initialized = 0;
  }
#endif
  
  cond->initialized = 0;
  return SIO_SUCCESS;
}

sio_error_t sio_cond_wait(sio_cond_t *cond, sio_mutex_t *mutex) {
  if (!cond || !cond->initialized || !mutex || !mutex->initialized) {
    return SIO_ERROR_PARAM;
  }
  
#if defined(SIO_OS_WINDOWS)
  if (mutex->is_cs) {
    if (!SleepConditionVariableCS(&cond->cond, &mutex->cs, INFINITE)) {
      return sio_get_last_error();
    }
  } else {
    /* Convert mutex handle to critical section for condition variable */
    /* This is a simplification - in real code we'd need proper mutex-to-CS conversion */
    return SIO_ERROR_UNSUPPORTED;
  }
  
#elif defined(SIO_OS_POSIX)
  int ret = pthread_cond_wait(&cond->cond, &mutex->mutex);
  
  if (ret != 0) {
    return sio_posix_error_to_sio_error(ret);
  }
#endif
  
  return SIO_SUCCESS;
}

sio_error_t sio_cond_timedwait(sio_cond_t *cond, sio_mutex_t *mutex, int32_t timeout_ms) {
  if (!cond || !cond->initialized || !mutex || !mutex->initialized) {
    return SIO_ERROR_PARAM;
  }
  
  /* Handle special cases */
  if (timeout_ms < 0) {
    return sio_cond_wait(cond, mutex);
  }
  
#if defined(SIO_OS_WINDOWS)
  if (mutex->is_cs) {
    if (!SleepConditionVariableCS(&cond->cond, &mutex->cs, (DWORD)timeout_ms)) {
      DWORD err = GetLastError();
      if (err == ERROR_TIMEOUT) {
        return SIO_ERROR_TIMEOUT;
      }
      return sio_win_error_to_sio_error(err);
    }
  } else {
    /* Convert mutex handle to critical section for condition variable */
    /* This is a simplification - in real code we'd need proper mutex-to-CS conversion */
    return SIO_ERROR_UNSUPPORTED;
  }
  
#elif defined(SIO_OS_POSIX)
  struct timespec ts;
  
  #if defined(_POSIX_TIMERS) && _POSIX_TIMERS > 0 && defined(CLOCK_MONOTONIC)
    /* Use monotonic clock if it was configured in condattr */
    clockid_t clock_id = CLOCK_REALTIME;
    pthread_condattr_getclock(&cond->attr, &clock_id);
    clock_gettime(clock_id, &ts);
  #else
    /* Fall back to realtime clock */
    #if defined(CLOCK_REALTIME)
      clock_gettime(CLOCK_REALTIME, &ts);
    #else
      struct timeval tv;
      gettimeofday(&tv, NULL);
      ts.tv_sec = tv.tv_sec;
      ts.tv_nsec = tv.tv_usec * 1000;
    #endif
  #endif
  
  /* Add timeout to current time */
  ts.tv_sec += timeout_ms / 1000;
  ts.tv_nsec += (timeout_ms % 1000) * 1000000;
  
  /* Handle nanosecond overflow */
  if (ts.tv_nsec >= 1000000000) {
    ts.tv_sec += 1;
    ts.tv_nsec -= 1000000000;
  }
  
  int ret = pthread_cond_timedwait(&cond->cond, &mutex->mutex, &ts);
  
  if (ret == ETIMEDOUT) {
    return SIO_ERROR_TIMEOUT;
  } else if (ret != 0) {
    return sio_posix_error_to_sio_error(ret);
  }
#endif
  
  return SIO_SUCCESS;
}

sio_error_t sio_cond_signal(sio_cond_t *cond) {
  if (!cond || !cond->initialized) {
    return SIO_ERROR_PARAM;
  }
  
#if defined(SIO_OS_WINDOWS)
  WakeConditionVariable(&cond->cond);
  
#elif defined(SIO_OS_POSIX)
  int ret = pthread_cond_signal(&cond->cond);
  
  if (ret != 0) {
    return sio_posix_error_to_sio_error(ret);
  }
#endif
  
  return SIO_SUCCESS;
}

sio_error_t sio_cond_broadcast(sio_cond_t *cond) {
  if (!cond || !cond->initialized) {
    return SIO_ERROR_PARAM;
  }
  
#if defined(SIO_OS_WINDOWS)
  WakeAllConditionVariable(&cond->cond);
  
#elif defined(SIO_OS_POSIX)
  int ret = pthread_cond_broadcast(&cond->cond);
  
  if (ret != 0) {
    return sio_posix_error_to_sio_error(ret);
  }
#endif
  
  return SIO_SUCCESS;
}

/*
 * Semaphore implementation
 */

sio_error_t sio_sem_init(sio_sem_t *sem, unsigned int initial_count, unsigned int max_count) {
  if (!sem) {
    return SIO_ERROR_PARAM;
  }
  
  /* Initialize semaphore structure */
  memset(sem, 0, sizeof(sio_sem_t));
  
  /* If max_count is 0, set to a very large value */
  if (max_count == 0) {
    max_count = UINT_MAX;
  }
  
#if defined(SIO_OS_WINDOWS)
  &sem->sem = CreateSemaphoreA(NULL, initial_count, max_count, NULL);
  
  if (!&sem->sem) {
    return sio_get_last_error();
  }
  
#elif defined(SIO_OS_LINUX) || defined(SIO_OS_BSD)
  /* Use unnamed semaphore */
  if (sem_init(&sem->sem, 0, initial_count) != 0) {
    return sio_posix_error_to_sio_error(errno);
  }
  
#else /* Fallback implementation using mutex and condition variable */
  /* Initialize mutex */
  sio_error_t err = sio_mutex_init(&sem->mutex, 0);
  if (err != SIO_SUCCESS) {
    return err;
  }
  
  /* Initialize condition variable */
  err = sio_cond_init(&sem->cond);
  if (err != SIO_SUCCESS) {
    sio_mutex_destroy(&sem->mutex);
    return err;
  }
  
  sem->count = initial_count;
  sem->max_count = max_count;
#endif
  
  sem->initialized = 1;
  return SIO_SUCCESS;
}

sio_error_t sio_sem_init_named(sio_sem_t *sem, const char *name, unsigned int initial_count, 
                           unsigned int max_count, int create_new) {
  if (!sem || !name) {
    return SIO_ERROR_PARAM;
  }
  
  /* Initialize semaphore structure */
  memset(sem, 0, sizeof(sio_sem_t));
  
  /* If max_count is 0, set to a very large value */
  if (max_count == 0) {
    max_count = UINT_MAX;
  }
  
#if defined(SIO_OS_WINDOWS)
  /* Set security attributes to allow cross-process sharing */
  SECURITY_ATTRIBUTES sa;
  sa.nLength = sizeof(sa);
  sa.lpSecurityDescriptor = NULL;
  sa.bInheritHandle = TRUE;
  
  DWORD access = SEMAPHORE_ALL_ACCESS;
  
  if (create_new) {
    sem->sem = CreateSemaphoreA(&sa, initial_count, max_count, name);
  } else {
    sem->sem = OpenSemaphoreA(access, TRUE, name);
  }
  
  if (!sem->sem) {
    return sio_get_last_error();
  }
  
#elif defined(SIO_OS_LINUX) || defined(SIO_OS_BSD)
  /* For named semaphores, handle / properly in name */
  char sem_name[256];
  if (name[0] != '/') {
    snprintf(sem_name, sizeof(sem_name), "/%s", name);
  } else {
    strncpy(sem_name, name, sizeof(sem_name) - 1);
    sem_name[sizeof(sem_name) - 1] = '\0';
  }
  
  /* Store name for cleanup */
  sem->name = strdup(sem_name);
  if (!sem->name) {
    return SIO_ERROR_MEM;
  }
  
  sem->is_named = 1;
  
  if (create_new) {
    /* Create new semaphore */
    sem->psem = sem_open(sem_name, O_CREAT | O_EXCL, 0666, initial_count);
    
    if (sem->psem == SEM_FAILED) {
      free(sem->name);
      sem->name = NULL;
      return sio_posix_error_to_sio_error(errno);
    }
  } else {
    /* Open existing semaphore */
    sem->psem = sem_open(sem_name, 0);
    
    if (sem->psem == SEM_FAILED) {
      free(sem->name);
      sem->name = NULL;
      return sio_posix_error_to_sio_error(errno);
    }
  }
  
#else /* Fallback implementation using mutex and condition variable */
  /* Named semaphores not supported in fallback mode */
  return SIO_ERROR_UNSUPPORTED;
#endif
  
  sem->initialized = 1;
  return SIO_SUCCESS;
}

sio_error_t sio_sem_destroy(sio_sem_t *sem) {
  if (!sem || !sem->initialized) {
    return SIO_ERROR_PARAM;
  }
  
#if defined(SIO_OS_WINDOWS)
  if (!CloseHandle(&sem->sem)) {
    return sio_get_last_error();
  }
  
#elif defined(SIO_OS_LINUX) || defined(SIO_OS_BSD)
  if (sem->is_named) {
    /* Close named semaphore */
    if (sem_close(&sem->sem) != 0) {
      return sio_posix_error_to_sio_error(errno);
    }
    
    /* Unlink if we created it */
    if (sem->name) {
      sem_unlink(sem->name);
      free(sem->name);
      sem->name = NULL;
    }
  } else {
    /* Destroy unnamed semaphore */
    if (sem_destroy(&sem->sem) != 0) {
      return sio_posix_error_to_sio_error(errno);
    }
  }
  
#else /* Fallback implementation */
  sio_cond_destroy(&sem->cond);
  sio_mutex_destroy(&sem->mutex);
#endif
  
  sem->initialized = 0;
  return SIO_SUCCESS;
}

sio_error_t sio_sem_wait(sio_sem_t *sem) {
  if (!sem || !sem->initialized) {
    return SIO_ERROR_PARAM;
  }
  
#if defined(SIO_OS_WINDOWS)
  DWORD wait_result = WaitForSingleObject(&sem->sem, INFINITE);
  
  if (wait_result == WAIT_FAILED) {
    return sio_get_last_error();
  }
  
#elif defined(SIO_OS_LINUX) || defined(SIO_OS_BSD)
  while (sem_wait(&sem->sem) != 0) {
    if (errno != EINTR) {
      return sio_posix_error_to_sio_error(errno);
    }
    /* Retry if interrupted by signal */
  }
  
#else /* Fallback implementation */
  sio_error_t err = sio_mutex_lock(&sem->mutex);
  if (err != SIO_SUCCESS) {
    return err;
  }
  
  while (sem->count == 0) {
    err = sio_cond_wait(&sem->cond, &sem->mutex);
    if (err != SIO_SUCCESS) {
      sio_mutex_unlock(&sem->mutex);
      return err;
    }
  }
  
  sem->count--;
  
  err = sio_mutex_unlock(&sem->mutex);
  if (err != SIO_SUCCESS) {
    return err;
  }
#endif
  
  return SIO_SUCCESS;
}

sio_error_t sio_sem_trywait(sio_sem_t *sem) {
  if (!sem || !sem->initialized) {
    return SIO_ERROR_PARAM;
  }
  
#if defined(SIO_OS_WINDOWS)
  DWORD wait_result = WaitForSingleObject(&sem->sem, 0);
  
  if (wait_result == WAIT_TIMEOUT) {
    return SIO_ERROR_BUSY;
  } else if (wait_result == WAIT_FAILED) {
    return sio_get_last_error();
  }
  
#elif defined(SIO_OS_LINUX) || defined(SIO_OS_BSD)
  if (sem_trywait(&sem->sem) != 0) {
    if (errno == EAGAIN) {
      return SIO_ERROR_BUSY;
    }
    return sio_posix_error_to_sio_error(errno);
  }
  
#else /* Fallback implementation */
  sio_error_t err = sio_mutex_lock(&sem->mutex);
  if (err != SIO_SUCCESS) {
    return err;
  }
  
  if (sem->count == 0) {
    sio_mutex_unlock(&sem->mutex);
    return SIO_ERROR_BUSY;
  }
  
  sem->count--;
  
  err = sio_mutex_unlock(&sem->mutex);
  if (err != SIO_SUCCESS) {
    return err;
  }
#endif
  
  return SIO_SUCCESS;
}

sio_error_t sio_sem_timedwait(sio_sem_t *sem, int32_t timeout_ms) {
  if (!sem || !sem->initialized) {
    return SIO_ERROR_PARAM;
  }
  
  /* Handle special cases */
  if (timeout_ms == 0) {
    return sio_sem_trywait(sem);
  } else if (timeout_ms < 0) {
    return sio_sem_wait(sem);
  }
  
#if defined(SIO_OS_WINDOWS)
  DWORD wait_result = WaitForSingleObject(&sem->sem, (DWORD)timeout_ms);
  
  if (wait_result == WAIT_TIMEOUT) {
    return SIO_ERROR_TIMEOUT;
  } else if (wait_result == WAIT_FAILED) {
    return sio_get_last_error();
  }
  
#elif defined(SIO_OS_LINUX) || defined(SIO_OS_BSD)
  struct timespec ts;
  
  #if defined(CLOCK_REALTIME)
    clock_gettime(CLOCK_REALTIME, &ts);
  #else
    struct timeval tv;
    gettimeofday(&tv, NULL);
    ts.tv_sec = tv.tv_sec;
    ts.tv_nsec = tv.tv_usec * 1000;
  #endif
  
  /* Add timeout to current time */
  ts.tv_sec += timeout_ms / 1000;
  ts.tv_nsec += (timeout_ms % 1000) * 1000000;
  
  /* Handle nanosecond overflow */
  if (ts.tv_nsec >= 1000000000) {
    ts.tv_sec += 1;
    ts.tv_nsec -= 1000000000;
  }
  
  while (sem_timedwait(&sem->sem, &ts) != 0) {
    if (errno == ETIMEDOUT) {
      return SIO_ERROR_TIMEOUT;
    } else if (errno != EINTR) {
      return sio_posix_error_to_sio_error(errno);
    }
    /* Retry if interrupted by signal */
  }
  
#else /* Fallback implementation */
  sio_error_t err = sio_mutex_lock(&sem->mutex);
  if (err != SIO_SUCCESS) {
    return err;
  }
  
  if (sem->count == 0) {
    err = sio_cond_timedwait(&sem->cond, &sem->mutex, timeout_ms);
    if (err != SIO_SUCCESS) {
      sio_mutex_unlock(&sem->mutex);
      return err;
    }
    
    if (sem->count == 0) {
      sio_mutex_unlock(&sem->mutex);
      return SIO_ERROR_TIMEOUT;
    }
  }
  
  sem->count--;
  
  err = sio_mutex_unlock(&sem->mutex);
  if (err != SIO_SUCCESS) {
    return err;
  }
#endif
  
  return SIO_SUCCESS;
}

sio_error_t sio_sem_post(sio_sem_t *sem) {
  if (!sem || !sem->initialized) {
    return SIO_ERROR_PARAM;
  }
  
#if defined(SIO_OS_WINDOWS)
  if (!ReleaseSemaphore(&sem->sem, 1, NULL)) {
    return sio_get_last_error();
  }
  
#elif defined(SIO_OS_LINUX) || defined(SIO_OS_BSD)
  if (sem_post(&sem->sem) != 0) {
    return sio_posix_error_to_sio_error(errno);
  }
  
#else /* Fallback implementation */
  sio_error_t err = sio_mutex_lock(&sem->mutex);
  if (err != SIO_SUCCESS) {
    return err;
  }
  
  if (sem->count < sem->max_count) {
    sem->count++;
    
    /* Signal one waiter */
    err = sio_cond_signal(&sem->cond);
    if (err != SIO_SUCCESS) {
      sio_mutex_unlock(&sem->mutex);
      return err;
    }
  }
  
  err = sio_mutex_unlock(&sem->mutex);
  if (err != SIO_SUCCESS) {
    return err;
  }
#endif
  
  return SIO_SUCCESS;
}

sio_error_t sio_sem_get_value(sio_sem_t *sem, int *value) {
  if (!sem || !sem->initialized || !value) {
    return SIO_ERROR_PARAM;
  }
  
#if defined(SIO_OS_WINDOWS)
  /* Windows doesn't provide a way to get semaphore value without modifying it */
  return SIO_ERROR_UNSUPPORTED;
  
#elif defined(SIO_OS_LINUX) || defined(SIO_OS_BSD)
  if (sem_getvalue(&sem->sem, value) != 0) {
    return sio_posix_error_to_sio_error(errno);
  }
  
#else /* Fallback implementation */
  sio_error_t err = sio_mutex_lock(&sem->mutex);
  if (err != SIO_SUCCESS) {
    return err;
  }
  
  *value = (int)sem->count;
  
  err = sio_mutex_unlock(&sem->mutex);
  if (err != SIO_SUCCESS) {
    return err;
  }
#endif
  
  return SIO_SUCCESS;
}

/*
 * Barrier implementation
 */

sio_error_t sio_barrier_init(sio_barrier_t *barrier, unsigned int count) {
  if (!barrier || count == 0) {
    return SIO_ERROR_PARAM;
  }
  
  /* Initialize barrier structure */
  memset(barrier, 0, sizeof(sio_barrier_t));
  
#if defined(SIO_OS_WINDOWS)
  #if _WIN32_WINNT >= 0x0602 /* Windows 8 and later */
    if (!InitializeSynchronizationBarrier(&barrier->barrier, count, -1)) {
      return sio_get_last_error();
    }
  #else
    /* Fallback implementation using critical section and condition variable */
    InitializeCriticalSection(&barrier->cs);
    InitializeConditionVariable(&barrier->cv);
    barrier->threshold = count;
    barrier->count = 0;
    barrier->generation = 0;
  #endif
  
#elif defined(SIO_OS_POSIX) && !defined(SIO_OS_MACOS)
  int ret = pthread_barrierattr_init(&barrier->attr);
  if (ret != 0) {
    return sio_posix_error_to_sio_error(ret);
  }
  barrier->attr_initialized = 1;
  
  ret = pthread_barrier_init(&barrier->barrier, &barrier->attr, count);
  if (ret != 0) {
    pthread_barrierattr_destroy(&barrier->attr);
    barrier->attr_initialized = 0;
    return sio_posix_error_to_sio_error(ret);
  }
  
#else /* Fallback implementation */
  sio_error_t err = sio_mutex_init(&barrier->mutex, 0);
  if (err != SIO_SUCCESS) {
    return err;
  }
  
  err = sio_cond_init(&barrier->cond);
  if (err != SIO_SUCCESS) {
    sio_mutex_destroy(&barrier->mutex);
    return err;
  }
  
  barrier->threshold = count;
  barrier->count = 0;
  barrier->generation = 0;
#endif
  
  barrier->initialized = 1;
  return SIO_SUCCESS;
}

sio_error_t sio_barrier_destroy(sio_barrier_t *barrier) {
  if (!barrier || !barrier->initialized) {
    return SIO_ERROR_PARAM;
  }
  
#if defined(SIO_OS_WINDOWS)
  #if _WIN32_WINNT >= 0x0602 /* Windows 8 and later */
    DeleteSynchronizationBarrier(&barrier->barrier);
  #else
    /* Fallback implementation */
    DeleteCriticalSection(&barrier->cs);
  #endif
  
#elif defined(SIO_OS_POSIX) && !defined(SIO_OS_MACOS)
  int ret = pthread_barrier_destroy(&barrier->barrier);
  
  if (ret != 0) {
    return sio_posix_error_to_sio_error(ret);
  }
  
  if (barrier->attr_initialized) {
    pthread_barrierattr_destroy(&barrier->attr);
    barrier->attr_initialized = 0;
  }
  
#else /* Fallback implementation */
  sio_cond_destroy(&barrier->cond);
  sio_mutex_destroy(&barrier->mutex);
#endif
  
  barrier->initialized = 0;
  return SIO_SUCCESS;
}

sio_error_t sio_barrier_wait(sio_barrier_t *barrier) {
  if (!barrier || !barrier->initialized) {
    return SIO_ERROR_PARAM;
  }
  
#if defined(SIO_OS_WINDOWS)
  #if _WIN32_WINNT >= 0x0602 /* Windows 8 and later */
    BOOL is_last = EnterSynchronizationBarrier(&barrier->barrier, 0);
    (void)is_last; /* Unused */
    
  #else
    /* Fallback implementation */
    BOOL is_last = FALSE;
    unsigned int my_generation;
    
    EnterCriticalSection(&barrier->cs);
    
    my_generation = barrier->generation;
    
    barrier->count++;
    if (barrier->count == barrier->threshold) {
      /* This thread is the last to reach the barrier */
      barrier->count = 0;
      barrier->generation++;
      is_last = TRUE;
      
      /* Wake up all waiting threads */
      WakeAllConditionVariable(&barrier->cv);
    } else {
      /* Wait for all threads to reach the barrier */
      while (my_generation == barrier->generation) {
        SleepConditionVariableCS(&barrier->cv, &barrier->cs, INFINITE);
      }
    }
    
    LeaveCriticalSection(&barrier->cs);
  #endif
  
#elif defined(SIO_OS_POSIX) && !defined(SIO_OS_MACOS)
  int ret = pthread_barrier_wait(&barrier->barrier);
  
  if (ret != 0 && ret != PTHREAD_BARRIER_SERIAL_THREAD) {
    return sio_posix_error_to_sio_error(ret);
  }
  
#else /* Fallback implementation */
  int is_last = 0;
  unsigned int my_generation;
  
  sio_error_t err = sio_mutex_lock(&barrier->mutex);
  if (err != SIO_SUCCESS) {
    return err;
  }
  
  my_generation = barrier->generation;
  
  barrier->count++;
  if (barrier->count == barrier->threshold) {
    /* This thread is the last to reach the barrier */
    barrier->count = 0;
    barrier->generation++;
    is_last = 1;
    
    /* Wake up all waiting threads */
    err = sio_cond_broadcast(&barrier->cond);
    if (err != SIO_SUCCESS) {
      sio_mutex_unlock(&barrier->mutex);
      return err;
    }
  } else {
    /* Wait for all threads to reach the barrier */
    while (my_generation == barrier->generation) {
      err = sio_cond_wait(&barrier->cond, &barrier->mutex);
      if (err != SIO_SUCCESS) {
        sio_mutex_unlock(&barrier->mutex);
        return err;
      }
    }
  }
  
  err = sio_mutex_unlock(&barrier->mutex);
  if (err != SIO_SUCCESS) {
    return err;
  }
#endif
  
  return SIO_SUCCESS;
}

/*
 * Spinlock implementation
 */

sio_error_t sio_spinlock_init(sio_spinlock_t *spinlock) {
  if (!spinlock) {
    return SIO_ERROR_PARAM;
  }
  
  /* Initialize spinlock structure */
  memset(spinlock, 0, sizeof(sio_spinlock_t));
  
#if defined(SIO_OS_WINDOWS)
  spinlock->lock = 0;
  
#elif defined(SIO_OS_POSIX) && defined(_POSIX_SPIN_LOCKS)
  int ret = pthread_spin_init(&spinlock->lock, PTHREAD_PROCESS_PRIVATE);
  
  if (ret != 0) {
    return sio_posix_error_to_sio_error(ret);
  }
  
#else /* Fallback atomic implementation */
  spinlock->lock = 0;
#endif
  
  spinlock->initialized = 1;
  return SIO_SUCCESS;
}

sio_error_t sio_spinlock_destroy(sio_spinlock_t *spinlock) {
  if (!spinlock || !spinlock->initialized) {
    return SIO_ERROR_PARAM;
  }
  
#if defined(SIO_OS_WINDOWS)
  /* No explicit destruction needed */
  
#elif defined(SIO_OS_POSIX) && defined(_POSIX_SPIN_LOCKS)
  int ret = pthread_spin_destroy(&spinlock->lock);
  
  if (ret != 0) {
    return sio_posix_error_to_sio_error(ret);
  }
  
#else /* Fallback atomic implementation */
  /* No explicit destruction needed */
#endif
  
  spinlock->initialized = 0;
  return SIO_SUCCESS;
}

sio_error_t sio_spinlock_lock(sio_spinlock_t *spinlock) {
  if (!spinlock || !spinlock->initialized) {
    return SIO_ERROR_PARAM;
  }
  
#if defined(SIO_OS_WINDOWS)
  /* Spin until we acquire the lock */
  while (InterlockedExchange(&spinlock->lock, 1) != 0) {
    /* Yield the processor while spinning to reduce contention */
    YieldProcessor();
  }
  
#elif defined(SIO_OS_POSIX) && defined(_POSIX_SPIN_LOCKS)
  int ret = pthread_spin_lock(&spinlock->lock);
  
  if (ret != 0) {
    return sio_posix_error_to_sio_error(ret);
  }
  
#else /* Fallback atomic implementation */
  /* Spin until we acquire the lock */
  while (1) {
    /* Try to acquire the lock */
    if (!SIO_ATOMIC_LOAD(&spinlock->lock) && 
        SIO_ATOMIC_CAS(&spinlock->lock, 0, 1)) {
      break;
    }
    
    /* Yield the processor while spinning to reduce contention */
    sio_thread_yield();
  }
#endif
  
  return SIO_SUCCESS;
}

sio_error_t sio_spinlock_trylock(sio_spinlock_t *spinlock) {
  if (!spinlock || !spinlock->initialized) {
    return SIO_ERROR_PARAM;
  }
  
#if defined(SIO_OS_WINDOWS)
  if (InterlockedCompareExchange(&spinlock->lock, 1, 0) != 0) {
    return SIO_ERROR_BUSY;
  }
  
#elif defined(SIO_OS_POSIX) && defined(_POSIX_SPIN_LOCKS)
  int ret = pthread_spin_trylock(&spinlock->lock);
  
  if (ret == EBUSY) {
    return SIO_ERROR_BUSY;
  } else if (ret != 0) {
    return sio_posix_error_to_sio_error(ret);
  }
  
#else /* Fallback atomic implementation */
  if (!SIO_ATOMIC_CAS(&spinlock->lock, 0, 1)) {
    return SIO_ERROR_BUSY;
  }
#endif
  
  return SIO_SUCCESS;
}

sio_error_t sio_spinlock_unlock(sio_spinlock_t *spinlock) {
  if (!spinlock || !spinlock->initialized) {
    return SIO_ERROR_PARAM;
  }
  
#if defined(SIO_OS_WINDOWS)
  spinlock->lock = 0;
  
#elif defined(SIO_OS_POSIX) && defined(_POSIX_SPIN_LOCKS)
  int ret = pthread_spin_unlock(&spinlock->lock);
  
  if (ret != 0) {
    return sio_posix_error_to_sio_error(ret);
  }
  
#else /* Fallback atomic implementation */
  SIO_ATOMIC_STORE(&spinlock->lock, 0);
#endif
  
  return SIO_SUCCESS;
}

/*
 * Thread-local storage implementation
 */

sio_error_t sio_tls_key_create(sio_tls_key_t *key, void (*destructor)(void*)) {
  if (!key) {
    return SIO_ERROR_PARAM;
  }
  
  /* Initialize key structure */
  memset(key, 0, sizeof(sio_tls_key_t));
  key->destructor = destructor;
  
#if defined(SIO_OS_WINDOWS)
  key->key = TlsAlloc();
  
  if (key->key == TLS_OUT_OF_INDEXES) {
    return sio_get_last_error();
  }
  
#elif defined(SIO_OS_POSIX)
  int ret = pthread_key_create(&key->key, destructor);
  
  if (ret != 0) {
    return sio_posix_error_to_sio_error(ret);
  }
#endif
  
  key->initialized = 1;
  return SIO_SUCCESS;
}

sio_error_t sio_tls_key_delete(sio_tls_key_t *key) {
  if (!key || !key->initialized) {
    return SIO_ERROR_PARAM;
  }
  
#if defined(SIO_OS_WINDOWS)
  if (!TlsFree(key->key)) {
    return sio_get_last_error();
  }
  
#elif defined(SIO_OS_POSIX)
  int ret = pthread_key_delete(key->key);
  
  if (ret != 0) {
    return sio_posix_error_to_sio_error(ret);
  }
#endif
  
  key->initialized = 0;
  return SIO_SUCCESS;
}

sio_error_t sio_tls_set_value(sio_tls_key_t *key, void *value) {
  if (!key || !key->initialized) {
    return SIO_ERROR_PARAM;
  }
  
#if defined(SIO_OS_WINDOWS)
  if (!TlsSetValue(key->key, value)) {
    return sio_get_last_error();
  }
  
#elif defined(SIO_OS_POSIX)
  int ret = pthread_setspecific(key->key, value);
  
  if (ret != 0) {
    return sio_posix_error_to_sio_error(ret);
  }
#endif
  
  return SIO_SUCCESS;
}

sio_error_t sio_tls_get_value(sio_tls_key_t *key, void **value) {
  if (!key || !key->initialized || !value) {
    return SIO_ERROR_PARAM;
  }
  
#if defined(SIO_OS_WINDOWS)
  *value = TlsGetValue(key->key);
  
  if (GetLastError() != ERROR_SUCCESS) {
    return sio_get_last_error();
  }
  
#elif defined(SIO_OS_POSIX)
  *value = pthread_getspecific(key->key);
#endif
  
  return SIO_SUCCESS;
}

#ifdef SIO_THREAD_LOCAL_UNSUPPORTED
/* Implementation for compilers without thread_local support */

sio_error_t sio_tls_value_init(sio_tls_value_t *tls, void (*destructor)(void*)) {
  if (!tls) {
    return SIO_ERROR_PARAM;
  }
  
  return sio_tls_key_create(&tls->key, destructor);
}

sio_error_t sio_tls_value_destroy(sio_tls_value_t *tls) {
  if (!tls) {
    return SIO_ERROR_PARAM;
  }
  
  return sio_tls_key_delete(&tls->key);
}

sio_error_t sio_tls_value_set(sio_tls_value_t *tls, void *value) {
  if (!tls) {
    return SIO_ERROR_PARAM;
  }
  
  return sio_tls_set_value(&tls->key, value);
}

void *sio_tls_value_get(sio_tls_value_t *tls) {
  if (!tls) {
    return NULL;
  }
  
  void *value = NULL;
  sio_tls_get_value(&tls->key, &value);
  return value;
}
#endif

/*
 * Once control implementation
 */

#if defined(SIO_OS_WINDOWS)
/* Once control function for Windows */
static void sio_once_callback(sio_once_t *once, void (*func)(void)) {
  /* Try to acquire the lock */
  if (InterlockedCompareExchange(&once->lock, 1, 0) == 0) {
    /* Check if initialization has been completed */
    if (once->state == 0) {
      /* Call the initialization function */
      func();
      
      /* Mark initialization as completed */
      InterlockedExchange(&once->state, 1);
    }
    
    /* Release the lock */
    InterlockedExchange(&once->lock, 0);
  } else {
    /* Wait for other thread to complete initialization */
    while (once->state == 0) {
      SwitchToThread();
    }
  }
}
#endif

sio_error_t sio_once(sio_once_t *once, void (*func)(void)) {
  if (!once || !func) {
    return SIO_ERROR_PARAM;
  }
  
#if defined(SIO_OS_WINDOWS)
  sio_once_callback(once, func);
  
#elif defined(SIO_OS_POSIX)
  int ret = pthread_once(&once->once, func);
  
  if (ret != 0) {
    return sio_posix_error_to_sio_error(ret);
  }
#endif
  
  return SIO_SUCCESS;
}

/*
 * Advanced OS-specific functionality
 */

#if defined(SIO_OS_LINUX)
sio_error_t sio_futex_wait(int32_t *addr, int32_t expected, int32_t timeout_ms) {
  struct timespec ts;
  
  if (timeout_ms >= 0) {
    clock_gettime(CLOCK_MONOTONIC, &ts);
    ts.tv_sec += timeout_ms / 1000;
    ts.tv_nsec += (timeout_ms % 1000) * 1000000;
    
    /* Handle nanosecond overflow */
    if (ts.tv_nsec >= 1000000000) {
      ts.tv_sec += 1;
      ts.tv_nsec -= 1000000000;
    }
  }
  
  int ret = syscall(SYS_futex, addr, FUTEX_WAIT_PRIVATE, expected, 
                  (timeout_ms >= 0) ? &ts : NULL, NULL, 0);
  
  if (ret < 0) {
    if (errno == ETIMEDOUT) {
      return SIO_ERROR_TIMEOUT;
    } else if (errno == EAGAIN) {
      /* Value changed before we could wait */
      return SIO_SUCCESS;
    }
    return sio_posix_error_to_sio_error(errno);
  }
  
  return SIO_SUCCESS;
}

sio_error_t sio_futex_wake(int32_t *addr, int32_t count) {
  int ret = syscall(SYS_futex, addr, FUTEX_WAKE_PRIVATE, count, NULL, NULL, 0);
  
  if (ret < 0) {
    return sio_posix_error_to_sio_error(errno);
  }
  
  return SIO_SUCCESS;
}
#endif

#if defined(SIO_OS_WINDOWS)
sio_error_t sio_win_create_waitable_timer(HANDLE *timer_handle, int manual_reset, const char *name) {
  if (!timer_handle) {
    return SIO_ERROR_PARAM;
  }
  
  *timer_handle = CreateWaitableTimerA(NULL, manual_reset, name);
  
  if (!*timer_handle) {
    return sio_get_last_error();
  }
  
  return SIO_SUCCESS;
}

sio_error_t sio_win_set_waitable_timer(HANDLE timer_handle, int64_t due_time, uint32_t period) {
  if (!timer_handle) {
    return SIO_ERROR_PARAM;
  }
  
  LARGE_INTEGER li_due_time;
  
  /* Convert milliseconds to 100-nanosecond intervals */
  li_due_time.QuadPart = due_time * -10000LL;
  
  if (!SetWaitableTimer(timer_handle, &li_due_time, period, NULL, NULL, FALSE)) {
    return sio_get_last_error();
  }
  
  return SIO_SUCCESS;
}
#endif

#if defined(SIO_OS_POSIX) && !defined(SIO_OS_MACOS)
sio_error_t sio_posix_create_timer(timer_t *timer_id, int signal, clockid_t clock_id) {
  if (!timer_id) {
    return SIO_ERROR_PARAM;
  }
  
  struct sigevent sev;
  memset(&sev, 0, sizeof(struct sigevent));
  
  sev.sigev_notify = SIGEV_SIGNAL;
  sev.sigev_signo = signal;
  
  if (timer_create(clock_id, &sev, timer_id) != 0) {
    return sio_posix_error_to_sio_error(errno);
  }
  
  return SIO_SUCCESS;
}

sio_error_t sio_posix_set_timer(timer_t timer_id, uint32_t initial_ms, uint32_t interval_ms) {
  struct itimerspec its;
  
  /* Initial expiration */
  its.it_value.tv_sec = initial_ms / 1000;
  its.it_value.tv_nsec = (initial_ms % 1000) * 1000000;
  
  /* Periodic interval */
  its.it_interval.tv_sec = interval_ms / 1000;
  its.it_interval.tv_nsec = (interval_ms % 1000) * 1000000;
  
  if (timer_settime(timer_id, 0, &its, NULL) != 0) {
    return sio_posix_error_to_sio_error(errno);
  }
  
  return SIO_SUCCESS;
}
#endif

/*
 * Process implementation
 */

sio_error_t sio_process_create(sio_process_t *process, const char *executable, 
                          const char **args, sio_process_flags_t flags) {
  if (!process || !executable || !args) {
    return SIO_ERROR_PARAM;
  }
  
  /* Initialize process structure */
  memset(process, 0, sizeof(sio_process_t));
  
#if defined(SIO_OS_WINDOWS)
  SECURITY_ATTRIBUTES sa;
  sa.nLength = sizeof(SECURITY_ATTRIBUTES);
  sa.bInheritHandle = TRUE;
  sa.lpSecurityDescriptor = NULL;
  
  HANDLE stdin_read = NULL;
  HANDLE stdout_write = NULL;
  HANDLE stderr_write = NULL;
  
  /* Create pipes for I/O redirection if requested */
  if (flags & SIO_PROCESS_REDIRECT_IO) {
    /* Create stdin pipe */
    if (!CreatePipe(&stdin_read, &process->stdin_write, &sa, 0)) {
      return sio_get_last_error();
    }
    
    /* Don't inherit the write end of stdin pipe */
    SetHandleInformation(process->stdin_write, HANDLE_FLAG_INHERIT, 0);
    
    /* Create stdout pipe */
    if (!CreatePipe(&process->stdout_read, &stdout_write, &sa, 0)) {
      CloseHandle(stdin_read);
      CloseHandle(process->stdin_write);
      return sio_get_last_error();
    }
    
    /* Don't inherit the read end of stdout pipe */
    SetHandleInformation(process->stdout_read, HANDLE_FLAG_INHERIT, 0);
    
    /* Create stderr pipe */
    if (!CreatePipe(&process->stderr_read, &stderr_write, &sa, 0)) {
      CloseHandle(stdin_read);
      CloseHandle(process->stdin_write);
      CloseHandle(process->stdout_read);
      CloseHandle(stdout_write);
      return sio_get_last_error();
    }
    
    /* Don't inherit the read end of stderr pipe */
    SetHandleInformation(process->stderr_read, HANDLE_FLAG_INHERIT, 0);
  }
  
  /* Build command line */
  char cmdline[32768] = {0}; /* Windows has a 32K command line limit */
  int pos = 0;
  
  /* Add executable */
  pos += snprintf(cmdline + pos, sizeof(cmdline) - pos, "\"%s\"", executable);
  
  /* Add arguments */
  for (int i = 0; args[i] != NULL; i++) {
    pos += snprintf(cmdline + pos, sizeof(cmdline) - pos, " \"%s\"", args[i]);
  }
  
  /* Set up process creation info */
  STARTUPINFOA si;
  memset(&si, 0, sizeof(STARTUPINFOA));
  si.cb = sizeof(STARTUPINFOA);
  
  /* Set up I/O redirection if requested */
  if (flags & SIO_PROCESS_REDIRECT_IO) {
    si.dwFlags |= STARTF_USESTDHANDLES;
    si.hStdInput = stdin_read;
    si.hStdOutput = stdout_write;
    si.hStdError = stderr_write;
  }
  
  /* Set up console window if requested */
  if (flags & SIO_PROCESS_NEW_CONSOLE) {
    si.dwFlags |= STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_SHOW;
  }
  
  /* Set up process creation flags */
  DWORD creation_flags = 0;
  
  if (flags & SIO_PROCESS_NEW_CONSOLE) {
    creation_flags |= CREATE_NEW_CONSOLE;
  }
  
  /* Create the process */
  if (!CreateProcessA(NULL, cmdline, NULL, NULL, TRUE, creation_flags,
                     NULL, NULL, &si, &process->process_info)) {
    DWORD err = GetLastError();
    
    /* Clean up resources */
    if (flags & SIO_PROCESS_REDIRECT_IO) {
      CloseHandle(stdin_read);
      CloseHandle(process->stdin_write);
      CloseHandle(process->stdout_read);
      CloseHandle(stdout_write);
      CloseHandle(process->stderr_read);
      CloseHandle(stderr_write);
    }
    
    return sio_win_error_to_sio_error(err);
  }
  
  /* Close unused pipe ends */
  if (flags & SIO_PROCESS_REDIRECT_IO) {
    CloseHandle(stdin_read);
    CloseHandle(stdout_write);
    CloseHandle(stderr_write);
  }
  
#elif defined(SIO_OS_POSIX)
  int stdin_pipe[2] = {-1, -1};
  int stdout_pipe[2] = {-1, -1};
  int stderr_pipe[2] = {-1, -1};
  
  /* Create pipes for I/O redirection if requested */
  if (flags & SIO_PROCESS_REDIRECT_IO) {
    if (pipe(stdin_pipe) < 0 || pipe(stdout_pipe) < 0 || pipe(stderr_pipe) < 0) {
      /* Clean up resources */
      if (stdin_pipe[0] >= 0) { close(stdin_pipe[0]); close(stdin_pipe[1]); }
      if (stdout_pipe[0] >= 0) { close(stdout_pipe[0]); close(stdout_pipe[1]); }
      if (stderr_pipe[0] >= 0) { close(stderr_pipe[0]); close(stderr_pipe[1]); }
      
      return sio_posix_error_to_sio_error(errno);
    }
  }
  
  /* Fork the process */
  pid_t pid = fork();
  
  if (pid < 0) {
    /* Fork failed */
    /* Clean up resources */
    if (flags & SIO_PROCESS_REDIRECT_IO) {
      close(stdin_pipe[0]); close(stdin_pipe[1]);
      close(stdout_pipe[0]); close(stdout_pipe[1]);
      close(stderr_pipe[0]); close(stderr_pipe[1]);
    }
    
    return sio_posix_error_to_sio_error(errno);
  } else if (pid == 0) {
    /* Child process */
    
    /* Set up I/O redirection if requested */
    if (flags & SIO_PROCESS_REDIRECT_IO) {
      /* Close unused pipe ends */
      close(stdin_pipe[1]); /* Close write end of stdin pipe */
      close(stdout_pipe[0]); /* Close read end of stdout pipe */
      close(stderr_pipe[0]); /* Close read end of stderr pipe */
      
      /* Redirect stdin/stdout/stderr */
      dup2(stdin_pipe[0], STDIN_FILENO);
      dup2(stdout_pipe[1], STDOUT_FILENO);
      dup2(stderr_pipe[1], STDERR_FILENO);
      
      /* Close original pipe ends */
      close(stdin_pipe[0]);
      close(stdout_pipe[1]);
      close(stderr_pipe[1]);
    }
    
    /* Build argument array */
    char **argv = NULL;
    int argc = 0;
    
    /* Count arguments */
    while (args[argc] != NULL) {
      argc++;
    }
    
    /* Allocate argument array (argc + 2: executable + args + NULL) */
    argv = (char**)malloc((argc + 2) * sizeof(char*));
    if (!argv) {
      _exit(EXIT_FAILURE);
    }
    
    /* Set up arguments */
    argv[0] = (char*)executable;
    for (int i = 0; i < argc; i++) {
      argv[i + 1] = (char*)args[i];
    }
    argv[argc + 1] = NULL;
    
    /* Execute the process */
    execvp(executable, argv);
    
    /* If we get here, execvp failed */
    free(argv);
    _exit(EXIT_FAILURE);
  } else {
    /* Parent process */
    process->pid = pid;
    
    /* Set up I/O redirection if requested */
    if (flags & SIO_PROCESS_REDIRECT_IO) {
      /* Close unused pipe ends */
      close(stdin_pipe[0]); /* Close read end of stdin pipe */
      close(stdout_pipe[1]); /* Close write end of stdout pipe */
      close(stderr_pipe[1]); /* Close write end of stderr pipe */
      
      /* Store pipe handles */
      process->stdin_write = stdin_pipe[1];
      process->stdout_read = stdout_pipe[0];
      process->stderr_read = stderr_pipe[0];
    }
  }
#endif
  
  process->running = 1;
  return SIO_SUCCESS;
}

sio_error_t sio_process_wait(sio_process_t *process, int *exit_code, int32_t timeout_ms) {
  if (!process || !process->running) {
    return SIO_ERROR_PARAM;
  }
  
#if defined(SIO_OS_WINDOWS)
  DWORD wait_result;
  
  if (timeout_ms < 0) {
    wait_result = WaitForSingleObject(process->process_info.hProcess, INFINITE);
  } else {
    wait_result = WaitForSingleObject(process->process_info.hProcess, (DWORD)timeout_ms);
  }
  
  if (wait_result == WAIT_TIMEOUT) {
    return SIO_ERROR_TIMEOUT;
  } else if (wait_result == WAIT_FAILED) {
    return sio_get_last_error();
  }
  
  if (exit_code) {
    DWORD win_exit_code;
    if (!GetExitCodeProcess(process->process_info.hProcess, &win_exit_code)) {
      return sio_get_last_error();
    }
    *exit_code = (int)win_exit_code;
  }
  
  process->running = 0;
  
#elif defined(SIO_OS_POSIX)
  int status;
  pid_t ret;
  
  if (timeout_ms < 0) {
    /* Wait indefinitely */
    ret = waitpid(process->pid, &status, 0);
  } else if (timeout_ms == 0) {
    /* Non-blocking wait */
    ret = waitpid(process->pid, &status, WNOHANG);
  } else {
    /* Wait with timeout using polling */
    struct timespec ts;
    ts.tv_sec = 0;
    ts.tv_nsec = 10000000; /* 10ms */
    
    uint32_t elapsed = 0;
    
    while (elapsed < (uint32_t)timeout_ms) {
      ret = waitpid(process->pid, &status, WNOHANG);
      
      if (ret > 0) {
        /* Process exited */
        break;
      } else if (ret < 0) {
        /* Error */
        return sio_posix_error_to_sio_error(errno);
      }
      
      /* Sleep for a short time */
      nanosleep(&ts, NULL);
      elapsed += 10;
    }
    
    if (elapsed >= (uint32_t)timeout_ms) {
      return SIO_ERROR_TIMEOUT;
    }
  }
  
  if (ret < 0) {
    return sio_posix_error_to_sio_error(errno);
  } else if (ret == 0) {
    /* Process still running (non-blocking wait) */
    return SIO_ERROR_TIMEOUT;
  }
  
  if (exit_code) {
    if (WIFEXITED(status)) {
      *exit_code = WEXITSTATUS(status);
    } else if (WIFSIGNALED(status)) {
      *exit_code = -WTERMSIG(status);
    } else {
      *exit_code = -1;
    }
  }
  
  process->running = 0;
#endif
  
  return SIO_SUCCESS;
}

sio_error_t sio_process_terminate(sio_process_t *process, int exit_code) {
  if (!process || !process->running) {
    return SIO_ERROR_PARAM;
  }
  
#if defined(SIO_OS_WINDOWS)
  if (!TerminateProcess(process->process_info.hProcess, (UINT)exit_code)) {
    return sio_get_last_error();
  }
  
#elif defined(SIO_OS_POSIX)
  if (kill(process->pid, SIGTERM) != 0) {
    return sio_posix_error_to_sio_error(errno);
  }
#endif
  
  return SIO_SUCCESS;
}

sio_error_t sio_process_destroy(sio_process_t *process) {
  if (!process) {
    return SIO_ERROR_PARAM;
  }
  
#if defined(SIO_OS_WINDOWS)
  /* Close process and thread handles */
  if (process->process_info.hProcess) {
    CloseHandle(process->process_info.hProcess);
  }
  
  if (process->process_info.hThread) {
    CloseHandle(process->process_info.hThread);
  }
  
  /* Close I/O handles if redirected */
  if (process->stdin_write) {
    CloseHandle(process->stdin_write);
  }
  
  if (process->stdout_read) {
    CloseHandle(process->stdout_read);
  }
  
  if (process->stderr_read) {
    CloseHandle(process->stderr_read);
  }
  
#elif defined(SIO_OS_POSIX)
  /* Close I/O handles if redirected */
  if (process->stdin_write >= 0) {
    close(process->stdin_write);
  }
  
  if (process->stdout_read >= 0) {
    close(process->stdout_read);
  }
  
  if (process->stderr_read >= 0) {
    close(process->stderr_read);
  }
#endif
  
  /* Clear process structure */
  memset(process, 0, sizeof(sio_process_t));
  
  return SIO_SUCCESS;
}

sio_error_t sio_process_write(sio_process_t *process, const void *buffer, 
                         size_t size, size_t *bytes_written) {
  if (!process || !buffer) {
    return SIO_ERROR_PARAM;
  }
  
  if (bytes_written) {
    *bytes_written = 0;
  }
  
#if defined(SIO_OS_WINDOWS)
  if (!process->stdin_write) {
    return SIO_ERROR_PARAM;
  }
  
  DWORD written;
  
  if (!WriteFile(process->stdin_write, buffer, (DWORD)size, &written, NULL)) {
    return sio_get_last_error();
  }
  
  if (bytes_written) {
    *bytes_written = (size_t)written;
  }
  
#elif defined(SIO_OS_POSIX)
  if (process->stdin_write < 0) {
    return SIO_ERROR_PARAM;
  }
  
  ssize_t written = write(process->stdin_write, buffer, size);
  
  if (written < 0) {
    return sio_posix_error_to_sio_error(errno);
  }
  
  if (bytes_written) {
    *bytes_written = (size_t)written;
  }
#endif
  
  return SIO_SUCCESS;
}

sio_error_t sio_process_read_stdout(sio_process_t *process, void *buffer, 
                               size_t size, size_t *bytes_read) {
  if (!process || !buffer) {
    return SIO_ERROR_PARAM;
  }
  
  if (bytes_read) {
    *bytes_read = 0;
  }
  
#if defined(SIO_OS_WINDOWS)
  if (!process->stdout_read) {
    return SIO_ERROR_PARAM;
  }
  
  DWORD read;
  
  if (!ReadFile(process->stdout_read, buffer, (DWORD)size, &read, NULL)) {
    DWORD err = GetLastError();
    if (err == ERROR_BROKEN_PIPE) {
      /* End of pipe */
      return SIO_ERROR_EOF;
    }
    return sio_win_error_to_sio_error(err);
  }
  
  if (bytes_read) {
    *bytes_read = (size_t)read;
  }
  
  if (read == 0) {
    return SIO_ERROR_EOF;
  }
  
#elif defined(SIO_OS_POSIX)
  if (process->stdout_read < 0) {
    return SIO_ERROR_PARAM;
  }
  
  ssize_t read_count = read(process->stdout_read, buffer, size);
  
  if (read_count < 0) {
    return sio_posix_error_to_sio_error(errno);
  }
  
  if (bytes_read) {
    *bytes_read = (size_t)read_count;
  }
  
  if (read_count == 0) {
    return SIO_ERROR_EOF;
  }
#endif
  
  return SIO_SUCCESS;
}

sio_error_t sio_process_read_stderr(sio_process_t *process, void *buffer, 
                               size_t size, size_t *bytes_read) {
  if (!process || !buffer) {
    return SIO_ERROR_PARAM;
  }
  
  if (bytes_read) {
    *bytes_read = 0;
  }
  
#if defined(SIO_OS_WINDOWS)
  if (!process->stderr_read) {
    return SIO_ERROR_PARAM;
  }
  
  DWORD read;
  
  if (!ReadFile(process->stderr_read, buffer, (DWORD)size, &read, NULL)) {
    DWORD err = GetLastError();
    if (err == ERROR_BROKEN_PIPE) {
      /* End of pipe */
      return SIO_ERROR_EOF;
    }
    return sio_win_error_to_sio_error(err);
  }
  
  if (bytes_read) {
    *bytes_read = (size_t)read;
  }
  
  if (read == 0) {
    return SIO_ERROR_EOF;
  }
  
#elif defined(SIO_OS_POSIX)
  if (process->stderr_read < 0) {
    return SIO_ERROR_PARAM;
  }
  
  ssize_t read_count = read(process->stderr_read, buffer, size);
  
  if (read_count < 0) {
    return sio_posix_error_to_sio_error(errno);
  }
  
  if (bytes_read) {
    *bytes_read = (size_t)read_count;
  }
  
  if (read_count == 0) {
    return SIO_ERROR_EOF;
  }
#endif
  
  return SIO_SUCCESS;
}

/*
 * Thread pool implementation
 */

/* Thread pool worker function */
static void *sio_threadpool_worker(void *arg) {
  sio_threadpool_t *pool = (sio_threadpool_t*)arg;
  sio_threadpool_task_func_t task_func;
  void *task_arg;
  
  while (1) {
    /* Lock the pool mutex */
    sio_mutex_lock(&pool->lock);
    
    /* Wait for tasks or shutdown signal */
    while (pool->task_count == 0 && !pool->shutdown && pool->paused) {
      sio_cond_wait(&pool->not_empty, &pool->lock);
    }
    
    /* Check if we should exit */
    if (pool->shutdown && pool->task_count == 0) {
      sio_mutex_unlock(&pool->lock);
      break;
    }
    
    /* Check if pool is paused */
    if (pool->paused) {
      sio_mutex_unlock(&pool->lock);
      sio_thread_yield();
      continue;
    }
    
    /* Get a task from the queue */
    if (pool->task_count > 0) {
      task_func = pool->tasks[pool->task_head].func;
      task_arg = pool->tasks[pool->task_head].arg;
      
      pool->task_head = (pool->task_head + 1) % pool->task_capacity;
      pool->task_count--;
      
      /* Signal that queue is not full */
      sio_cond_signal(&pool->not_full);
      
      /* Unlock the mutex before executing the task */
      sio_mutex_unlock(&pool->lock);
      
      /* Execute the task */
      if (task_func) {
        task_func(task_arg);
      }
    } else {
      sio_mutex_unlock(&pool->lock);
    }
  }
  
  return NULL;
}

sio_error_t sio_threadpool_create(sio_threadpool_t *pool, size_t thread_count, size_t task_capacity) {
  if (!pool || thread_count == 0 || task_capacity == 0) {
    return SIO_ERROR_PARAM;
  }
  
  /* Initialize pool structure */
  memset(pool, 0, sizeof(sio_threadpool_t));
  
  /* Initialize synchronization primitives */
  sio_error_t err = sio_mutex_init(&pool->lock, 0);
  if (err != SIO_SUCCESS) {
    return err;
  }
  
  err = sio_cond_init(&pool->not_empty);
  if (err != SIO_SUCCESS) {
    sio_mutex_destroy(&pool->lock);
    return err;
  }
  
  err = sio_cond_init(&pool->not_full);
  if (err != SIO_SUCCESS) {
    sio_cond_destroy(&pool->not_empty);
    sio_mutex_destroy(&pool->lock);
    return err;
  }
  
  /* Allocate task queue */
  pool->tasks = (struct { sio_threadpool_task_func_t func; void *arg; }*)
    malloc(task_capacity * sizeof(struct { sio_threadpool_task_func_t func; void *arg; }));
  
  if (!pool->tasks) {
    sio_cond_destroy(&pool->not_full);
    sio_cond_destroy(&pool->not_empty);
    sio_mutex_destroy(&pool->lock);
    return SIO_ERROR_MEM;
  }
  
  /* Initialize task queue */
  pool->task_capacity = task_capacity;
  pool->task_count = 0;
  pool->task_head = 0;
  pool->task_tail = 0;
  
  /* Allocate thread array */
  pool->threads = (sio_thread_t*)malloc(thread_count * sizeof(sio_thread_t));
  
  if (!pool->threads) {
    free(pool->tasks);
    sio_cond_destroy(&pool->not_full);
    sio_cond_destroy(&pool->not_empty);
    sio_mutex_destroy(&pool->lock);
    return SIO_ERROR_MEM;
  }
  
  /* Initialize thread count */
  pool->thread_count = thread_count;
  
  /* Create worker threads */
  for (size_t i = 0; i < thread_count; i++) {
    err = sio_thread_create(&pool->threads[i], sio_threadpool_worker, pool, SIO_THREAD_DEFAULT);
    
    if (err != SIO_SUCCESS) {
      /* Clean up already created threads */
      pool->shutdown = 1;
      sio_cond_broadcast(&pool->not_empty);
      
      for (size_t j = 0; j < i; j++) {
        sio_thread_join(&pool->threads[j], NULL);
      }
      
      free(pool->threads);
      free(pool->tasks);
      sio_cond_destroy(&pool->not_full);
      sio_cond_destroy(&pool->not_empty);
      sio_mutex_destroy(&pool->lock);
      return err;
    }
  }
  
  return SIO_SUCCESS;
}

sio_error_t sio_threadpool_destroy(sio_threadpool_t *pool, int finish_tasks) {
  if (!pool) {
    return SIO_ERROR_PARAM;
  }
  
  /* Lock the pool mutex */
  sio_error_t err = sio_mutex_lock(&pool->lock);
  if (err != SIO_SUCCESS) {
    return err;
  }
  
  /* Set shutdown flag */
  pool->shutdown = 1;
  
  /* Clear task queue if not finishing tasks */
  if (!finish_tasks) {
    pool->task_count = 0;
    pool->task_head = 0;
    pool->task_tail = 0;
  }
  
  /* Wake up all worker threads */
  err = sio_cond_broadcast(&pool->not_empty);
  if (err != SIO_SUCCESS) {
    sio_mutex_unlock(&pool->lock);
    return err;
  }
  
  /* Unlock the mutex */
  err = sio_mutex_unlock(&pool->lock);
  if (err != SIO_SUCCESS) {
    return err;
  }
  
  /* Wait for worker threads to finish */
  for (size_t i = 0; i < pool->thread_count; i++) {
    err = sio_thread_join(&pool->threads[i], NULL);
    if (err != SIO_SUCCESS) {
      return err;
    }
  }
  
  /* Free resources */
  free(pool->threads);
  free(pool->tasks);
  
  /* Destroy synchronization primitives */
  sio_cond_destroy(&pool->not_full);
  sio_cond_destroy(&pool->not_empty);
  sio_mutex_destroy(&pool->lock);
  
  /* Clear pool structure */
  memset(pool, 0, sizeof(sio_threadpool_t));
  
  return SIO_SUCCESS;
}

sio_error_t sio_threadpool_add_task(sio_threadpool_t *pool, 
                              sio_threadpool_task_func_t func, void *arg, 
                              int wait_if_full) {
  if (!pool || !func) {
    return SIO_ERROR_PARAM;
  }
  
  /* Lock the pool mutex */
  sio_error_t err = sio_mutex_lock(&pool->lock);
  if (err != SIO_SUCCESS) {
    return err;
  }
  
  /* Check if pool is shutting down */
  if (pool->shutdown) {
    sio_mutex_unlock(&pool->lock);
    return SIO_ERROR_BUSY;
  }
  
  /* Wait if the queue is full and wait_if_full is set */
  while (pool->task_count == pool->task_capacity) {
    if (!wait_if_full) {
      sio_mutex_unlock(&pool->lock);
      return SIO_ERROR_BUSY;
    }
    
    err = sio_cond_wait(&pool->not_full, &pool->lock);
    if (err != SIO_SUCCESS) {
      sio_mutex_unlock(&pool->lock);
      return err;
    }
    
    /* Check if pool is shutting down again after waiting */
    if (pool->shutdown) {
      sio_mutex_unlock(&pool->lock);
      return SIO_ERROR_BUSY;
    }
  }
  
  /* Add the task to the queue */
  pool->tasks[pool->task_tail].func = func;
  pool->tasks[pool->task_tail].arg = arg;
  
  pool->task_tail = (pool->task_tail + 1) % pool->task_capacity;
  pool->task_count++;
  
  /* Signal that queue is not empty */
  err = sio_cond_signal(&pool->not_empty);
  if (err != SIO_SUCCESS) {
    sio_mutex_unlock(&pool->lock);
    return err;
  }
  
  /* Unlock the mutex */
  err = sio_mutex_unlock(&pool->lock);
  if (err != SIO_SUCCESS) {
    return err;
  }
  
  return SIO_SUCCESS;
}

sio_error_t sio_threadpool_pause(sio_threadpool_t *pool) {
  if (!pool) {
    return SIO_ERROR_PARAM;
  }
  
  /* Lock the pool mutex */
  sio_error_t err = sio_mutex_lock(&pool->lock);
  if (err != SIO_SUCCESS) {
    return err;
  }
  
  /* Set pause flag */
  pool->paused = 1;
  
  /* Unlock the mutex */
  err = sio_mutex_unlock(&pool->lock);
  if (err != SIO_SUCCESS) {
    return err;
  }
  
  return SIO_SUCCESS;
}

sio_error_t sio_threadpool_resume(sio_threadpool_t *pool) {
  if (!pool) {
    return SIO_ERROR_PARAM;
  }
  
  /* Lock the pool mutex */
  sio_error_t err = sio_mutex_lock(&pool->lock);
  if (err != SIO_SUCCESS) {
    return err;
  }
  
  /* Clear pause flag */
  pool->paused = 0;
  
  /* Wake up all worker threads */
  err = sio_cond_broadcast(&pool->not_empty);
  if (err != SIO_SUCCESS) {
    sio_mutex_unlock(&pool->lock);
    return err;
  }
  
  /* Unlock the mutex */
  err = sio_mutex_unlock(&pool->lock);
  if (err != SIO_SUCCESS) {
    return err;
  }
  
  return SIO_SUCCESS;
}

size_t sio_threadpool_get_thread_count(const sio_threadpool_t *pool) {
  return pool ? pool->thread_count : 0;
}

size_t sio_threadpool_get_pending_tasks(const sio_threadpool_t *pool) {
  if (!pool) {
    return 0;
  }
  
  /* Lock the pool mutex */
  if (sio_mutex_lock((sio_mutex_t*)&pool->lock) != SIO_SUCCESS) {
    return 0;
  }
  
  size_t count = pool->task_count;
  
  /* Unlock the mutex */
  sio_mutex_unlock((sio_mutex_t*)&pool->lock);
  
  return count;
}

/* Non-compiler atomic operations for platforms without intrinsics */
#if !defined(SIO_COMPILER_GCC) && !defined(SIO_COMPILER_CLANG) && !defined(SIO_COMPILER_MSVC)

/* Global mutex for atomic operations */
static sio_mutex_t g_atomic_mutex;
static int g_atomic_mutex_initialized = 0;

/* Initialize the atomic mutex if needed */
static void sio_atomic_init(void) {
  if (!g_atomic_mutex_initialized) {
    sio_mutex_init(&g_atomic_mutex, 0);
    g_atomic_mutex_initialized = 1;
  }
}

int32_t sio_atomic_add(int32_t volatile *ptr, int32_t val) {
  sio_atomic_init();
  sio_mutex_lock(&g_atomic_mutex);
  int32_t old = *ptr;
  *ptr += val;
  sio_mutex_unlock(&g_atomic_mutex);
  return old + val;
}

int32_t sio_atomic_sub(int32_t volatile *ptr, int32_t val) {
  sio_atomic_init();
  sio_mutex_lock(&g_atomic_mutex);
  int32_t old = *ptr;
  *ptr -= val;
  sio_mutex_unlock(&g_atomic_mutex);
  return old - val;
}

int32_t sio_atomic_inc(int32_t volatile *ptr) {
  return sio_atomic_add(ptr, 1);
}

int32_t sio_atomic_dec(int32_t volatile *ptr) {
  return sio_atomic_sub(ptr, 1);
}

int sio_atomic_cas(int32_t volatile *ptr, int32_t oldval, int32_t newval) {
  sio_atomic_init();
  sio_mutex_lock(&g_atomic_mutex);
  int result = (*ptr == oldval);
  if (result) {
    *ptr = newval;
  }
  sio_mutex_unlock(&g_atomic_mutex);
  return result;
}

void sio_atomic_store(int32_t volatile *ptr, int32_t val) {
  sio_atomic_init();
  sio_mutex_lock(&g_atomic_mutex);
  *ptr = val;
  sio_mutex_unlock(&g_atomic_mutex);
}

int32_t sio_atomic_load(int32_t volatile *ptr) {
  sio_atomic_init();
  sio_mutex_lock(&g_atomic_mutex);
  int32_t val = *ptr;
  sio_mutex_unlock(&g_atomic_mutex);
  return val;
}

void sio_memory_barrier(void) {
  sio_atomic_init();
  sio_mutex_lock(&g_atomic_mutex);
  sio_mutex_unlock(&g_atomic_mutex);
}

void sio_read_barrier(void) {
  sio_memory_barrier();
}

void sio_write_barrier(void) {
  sio_memory_barrier();
}

#endif /* !defined(SIO_COMPILER_GCC) && !defined(SIO_COMPILER_CLANG) && !defined(SIO_COMPILER_MSVC) */