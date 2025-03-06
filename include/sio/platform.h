/**
* @file sio/platform.h
* @brief Simple I/O (SIO) - Cross-platform I/O library for high-performance systems programming
*
* Define the macros needed to determine the operating system this library is to be compiled for.
* Also includes the option to be defined manually if potential cross compiling is wanted or the operating system cannot be determined.
*
* @author zczxy
* @version 0.1.0
*/

#ifndef SIO_PLATFORM_H
#define SIO_PLATFORM_H

/**
* @brief OS detection macros
* 
* SIO_OS_LINUX   - Linux operating system
* SIO_OS_WINDOWS - Windows operating system
* SIO_OS_MACOS   - MacOS operating system
* SIO_OS_BSD     - BSD variants
* SIO_OS_UNIX    - Other UNIX-like systems
*/

/* Allow manual override by defining SIO_OS manually before including this header */
#if !defined(SIO_OS_LINUX) && !defined(SIO_OS_WINDOWS) && !defined(SIO_OS_MACOS) && !defined(SIO_OS_BSD) && !defined(SIO_OS_UNIX)

#if defined(__linux__) || defined(__linux)
  #define SIO_OS_LINUX 1
  #define SIO_OS_POSIX 1
#elif defined(_WIN32) || defined(_WIN64)
  #define SIO_OS_WINDOWS 1
#elif defined(__APPLE__) || defined(__MACH__)
  #define SIO_OS_MACOS 1
  #define SIO_OS_POSIX 1
#elif defined(__FreeBSD__) || defined(__NetBSD__) || defined(__OpenBSD__) || defined(__DragonFly__)
  #define SIO_OS_BSD 1
  #define SIO_OS_POSIX 1
#elif defined(__unix__) || defined(__unix)
  #define SIO_OS_UNIX 1
  #define SIO_OS_POSIX 1
#else
  #error "SIO: Unsupported operating system. Please define SIO_OS_* manually."
#endif

#endif /* !defined SIO_OS_* */

/**
* @brief Architecture detection macros
* 
* SIO_ARCH_X86    - x86 32-bit architecture
* SIO_ARCH_X86_64 - x86 64-bit architecture 
* SIO_ARCH_ARM    - ARM 32-bit architecture
* SIO_ARCH_ARM64  - ARM 64-bit architecture
* SIO_ARCH_RISCV  - RISC-V architecture
* SIO_ARCH_PPC    - PowerPC architecture
*/
#if !defined(SIO_ARCH_X86) && !defined(SIO_ARCH_X86_64) && !defined(SIO_ARCH_ARM) && !defined(SIO_ARCH_ARM64) && \
    !defined(SIO_ARCH_RISCV) && !defined(SIO_ARCH_PPC)

#if defined(__i386__) || defined(_M_IX86)
  #define SIO_ARCH_X86 1
#elif defined(__x86_64__) || defined(_M_X64)
  #define SIO_ARCH_X86_64 1
#elif defined(__arm__) || defined(_M_ARM)
  #define SIO_ARCH_ARM 1
#elif defined(__aarch64__) || defined(_M_ARM64)
  #define SIO_ARCH_ARM64 1
#elif defined(__riscv)
  #define SIO_ARCH_RISCV 1
#elif defined(__powerpc__) || defined(__ppc__) || defined(__PPC__)
  #define SIO_ARCH_PPC 1
#else
  #error "SIO: Unsupported architecture. Please define SIO_ARCH_* manually."
#endif

#endif /* !defined SIO_ARCH_* */

/**
* @brief Compiler detection macros
* 
* SIO_COMPILER_GCC     - GCC compiler
* SIO_COMPILER_CLANG   - Clang compiler
* SIO_COMPILER_MSVC    - Microsoft Visual C++ compiler
* SIO_COMPILER_ICC     - Intel C Compiler
*/
#if !defined(SIO_COMPILER_GCC) && !defined(SIO_COMPILER_CLANG) && !defined(SIO_COMPILER_MSVC) && !defined(SIO_COMPILER_ICC)

#if defined(__GNUC__) && !defined(__clang__)
  #define SIO_COMPILER_GCC 1
#elif defined(__clang__)
  #define SIO_COMPILER_CLANG 1
#elif defined(_MSC_VER)
  #define SIO_COMPILER_MSVC 1
#elif defined(__INTEL_COMPILER) || defined(__ICC)
  #define SIO_COMPILER_ICC 1
#else
  #error "SIO: Unsupported compiler. Please define SIO_COMPILER_* manually."
#endif

#endif /* !defined SIO_COMPILER_* */

/* Standard includes needed by most files */
#include <string.h>
#include <stdint.h>

/**
* @brief Platform-specific memory alignment
*/
#if defined(SIO_ARCH_X86_64) || defined(SIO_ARCH_ARM64)
  #define SIO_MEMORY_ALIGNMENT 8
#else
  #define SIO_MEMORY_ALIGNMENT 4
#endif

/**
* @brief Function attributes for optimization
*/
#if defined(SIO_COMPILER_GCC) || defined(SIO_COMPILER_CLANG)
  #define SIO_INLINE inline __attribute__((always_inline))
  #define SIO_NOINLINE __attribute__((noinline))
  #define SIO_ALIGN(x) __attribute__((aligned(x)))
  #define SIO_LIKELY(x) __builtin_expect(!!(x), 1)
  #define SIO_UNLIKELY(x) __builtin_expect(!!(x), 0)
  #define SIO_DEPRECATED __attribute__((deprecated))
  #define SIO_NORETURN __attribute__((noreturn))
  #define SIO_PACKED __attribute__((packed))
  #define SIO_UNUSED __attribute__((unused))
  #define SIO_CONSTRUCTOR __attribute__((constructor))
  #define SIO_DESTRUCTOR __attribute__((destructor))
  #define SIO_THREAD_LOCAL __thread
  #define SIO_PRINTF_CHECK(fmt_idx, args_idx) __attribute__((format(printf, fmt_idx, args_idx)))
  #define SIO_EXPORT __attribute__((visibility("default")))
  #define SIO_IMPORT
#elif defined(SIO_COMPILER_MSVC)
  #define SIO_INLINE __forceinline
  #define SIO_NOINLINE __declspec(noinline)
  #define SIO_ALIGN(x) __declspec(align(x))
  #define SIO_LIKELY(x) (x)
  #define SIO_UNLIKELY(x) (x)
  #define SIO_DEPRECATED __declspec(deprecated)
  #define SIO_NORETURN __declspec(noreturn)
  #define SIO_PACKED
  #define SIO_UNUSED
  #define SIO_CONSTRUCTOR
  #define SIO_DESTRUCTOR
  #define SIO_THREAD_LOCAL __declspec(thread)
  #define SIO_PRINTF_CHECK(fmt_idx, args_idx)
  #define SIO_EXPORT __declspec(dllexport)
  #define SIO_IMPORT __declspec(dllimport)
#else
  #define SIO_INLINE inline
  #define SIO_NOINLINE
  #define SIO_ALIGN(x)
  #define SIO_LIKELY(x) (x)
  #define SIO_UNLIKELY(x) (x)
  #define SIO_DEPRECATED
  #define SIO_NORETURN
  #define SIO_PACKED
  #define SIO_UNUSED
  #define SIO_CONSTRUCTOR
  #define SIO_DESTRUCTOR
  #define SIO_THREAD_LOCAL
  #define SIO_PRINTF_CHECK(fmt_idx, args_idx)
  #define SIO_EXPORT
  #define SIO_IMPORT
#endif

#endif /* SIO_PLATFORM_H */