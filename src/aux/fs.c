/**
* @file src/aux/fs.c
* @brief Simple I/O (SIO) - Cross-platform I/O library implementation
* 
* Implementation of filesystem functions defined in sio/aux/fs.h
* Provides cross-platform filesystem operations for Windows and POSIX systems
*
* @author zczxy
* @version 0.1.0
*/

#include <sio/aux/fs.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

/* Platform-specific includes */
#if defined(SIO_OS_WINDOWS)
  #include <windows.h>
  #include <direct.h>
  #include <io.h>
  #include <shlwapi.h>
  #pragma comment(lib, "shlwapi.lib")
#elif defined(SIO_OS_POSIX)
  #include <limits.h>
  #include <sys/stat.h>
  #include <sys/types.h>
  #include <dirent.h>
  #include <unistd.h>
  #include <fcntl.h>
  #include <libgen.h>
  #include <sys/statvfs.h>
#endif

/*====================== Path Functions ======================*/
/*====================== File Functions ======================*/
/*====================== Dir  Functions ======================*/
/*====================== Disk Functions ======================*/