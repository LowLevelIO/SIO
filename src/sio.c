/**
* @file sio/sio.c
* @brief Simple I/O (SIO) - Cross-platform I/O library for high-performance systems programming
*
* Implementation of library initalization and destruction.
* 
* @author zczxy
* @version 0.1.0
*/
#include <sio.h>

sio_error_t sio_initialize(long conf) {
#if defined(SIO_OS_WINDOW)
  if (conf & SIO_INITALIZE_RAW_SOCK) {
    WSADATA wsa_data;
    if (WSAStartup(MAKEWORD(2, 2), &wsa_data) != 0) 
      return sio_get_last_error();
  }
#endif
  (void)conf;
  return SIO_SUCCESS;
}

void sio_cleanup() {}