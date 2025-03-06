/**
* @file sio/context.h
* @brief Simple I/O (SIO) - Cross-platform I/O library for high-performance systems programming
*
* An event handling context can be thought of as a wrapper for a poll like instance.
* Windows developers can think of IOCP and linux of epoll/io_uring.
* The context used can be decided at runtime and selected manually.
* By default the most recent and performant option is chosen which for linux is io_uring if not available epoll is considered a fallback.
* An end user should not notice any difference in use cases only in speed.
*
* @author zczxy
* @version 0.1.0
*/
