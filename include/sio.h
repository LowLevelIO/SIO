/**
* @file sio.h
* @brief Simple I/O (SIO) - Cross-platform I/O library for high-performance systems programming
*
* The main frontend for the API consisting of all possible library functionality
*
* @author zczxy
* @version 0.1.0
*/

/**
* @brief Version information
*/
#define SIO_VERSION_MAJOR 0
#define SIO_VERSION_MINOR 1
#define SIO_VERSION_PATCH 0
#define SIO_VERSION_STRING "0.1.0"

#define SIO_INITALIZE_RAW_SOCK (1 << 0)

SIO_EXPORT sio_error_t sio_initialize(long flags);
SIO_EXPORT void sio_cleanup();

