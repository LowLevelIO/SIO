# SIO - Simple I/O Library

## Overview

SIO (Simple I/O) is a high-performance, cross-platform I/O library designed for systems programming. It provides a unified interface for various I/O operations while maintaining close-to-metal performance and minimal overhead.

**Key Features:**
- Zero-copy I/O operations when possible
- Stack-based allocation with minimal heap usage
- Cross-platform compatibility (Linux, Windows, macOS, BSD)
- Support for various I/O types: files, sockets, pipes, terminals, devices, timers, signals, message queues, and shared memory
- Asynchronous I/O operations with callback support
- Event-driven architecture with efficient polling mechanisms
- Direct access to platform-specific optimizations

## Design Philosophy

SIO is built on several core principles:

1. **Performance First**: Designed for high-throughput, low-latency applications where every CPU cycle and memory access matters.

2. **Zero Copy**: Wherever possible, SIO avoids unnecessary data copying by using platform-specific zero-copy mechanisms.

3. **Minimal Allocation**: SIO prefers stack allocation and avoids heap allocation (no `malloc`), relying instead on buffer management that aligns with OS-specific optimizations.

4. **Platform-Aware**: While providing a unified API, SIO leverages the best performing I/O mechanisms on each platform:
   - Windows: IOCP (I/O Completion Ports)
   - Linux: io_uring (when available), epoll, or optimized poll
   - BSD/macOS: kqueue

5. **No Dependencies**: SIO is self-contained with no external library dependencies.

## Memory Management

SIO's approach to memory management is what sets it apart from many I/O libraries:

- **Buffer Registration**: Rather than allocating buffers internally, SIO allows registering existing buffers with the kernel for optimal I/O performance.
- **Alignment-Aware**: The library provides functions to create properly aligned buffers that work optimally with the underlying hardware and OS.
- **Direct I/O**: Support for bypassing the OS cache when appropriate.
- **Memory Mapping**: First-class support for memory-mapped I/O.

Instead of hidden allocations, SIO makes memory management explicit, giving you control over where and how memory is allocated while ensuring maximum performance.

## Platform Support

SIO automatically detects and uses the most efficient I/O mechanisms available on each platform:

| Platform | Primary Mechanism | Fallback |
|----------|-------------------|----------|
| Windows  | IOCP             | Overlapped I/O |
| Linux    | io_uring         | epoll, poll |
| macOS/BSD| kqueue           | poll |

The `sio_query_features` function allows checking which optimizations are available at runtime.

## Getting Started

### Building SIO

SIO uses a simple build system with minimal dependencies:

```bash
# Clone the repository
git clone https://github.com/zczxy/sio.git
cd sio

# Build the library
make

# Install (optional)
make install
```

For Windows:
```batch
build.bat
```

### Using SIO in Your Project

Include the header and link against the library:

```c
#include <sio.h>

int main() {
    // Initialize the library
    sio_initialize();
    
    // Create an I/O context
    sio_context_t* ctx = sio_context_create(100);
    
    // Use SIO functions...
    
    // Cleanup
    sio_context_destroy(ctx);
    sio_cleanup();
    
    return 0;
}
```

Compile with:
```bash
gcc -o myapp myapp.c -lsio
```

## Examples

### TCP Echo Server

```c
#include <sio.h>
#include <stdio.h>
#include <string.h>

#define BUFFER_SIZE 4096

// Callback function for handling client data
void handle_client_data(sio_event_t* event, void* user_data) {
    if (event->type == SIO_EVENT_READ) {
        // Stack-allocated buffer - no malloc needed
        char buffer[BUFFER_SIZE];
        size_t bytes_read;
        
        // Read data from client
        int result = sio_read(event->handle, buffer, BUFFER_SIZE, &bytes_read);
        
        if (result == SIO_SUCCESS && bytes_read > 0) {
            // Echo the data back
            size_t bytes_written;
            sio_write(event->handle, buffer, bytes_read, &bytes_written);
        } else if (result == SIO_CLOSED || bytes_read == 0) {
            // Connection closed
            sio_close(event->handle);
        }
    }
}

// Callback function for accepting new connections
void handle_accept(sio_event_t* event, void* user_data) {
    sio_context_t* ctx = (sio_context_t*)user_data;
    
    if (event->type == SIO_EVENT_ACCEPT) {
        // Accept the new connection
        sio_handle_t* client = sio_socket_accept(event->handle, NULL, NULL);
        
        if (client) {
            // Set the socket to non-blocking mode
            sio_set_nonblocking(client, true);
            
            // Register for read events
            sio_register(ctx, client, SIO_MASK_READ | SIO_MASK_CLOSE, 
                         handle_client_data, NULL);
        }
    }
}

int main() {
    // Initialize SIO
    sio_initialize();
    
    // Create an I/O context
    sio_context_t* ctx = sio_context_create(100);
    
    // Create server socket
    sio_handle_t* server = sio_socket_create(ctx, SIO_AF_INET, SIO_SOCK_STREAM, 0);
    
    // Bind to all interfaces on port 8080
    sio_socket_bind(server, "0.0.0.0", 8080);
    
    // Start listening
    sio_socket_listen(server, 10);
    
    // Register for accept events
    sio_register(ctx, server, SIO_MASK_ACCEPT, handle_accept, ctx);
    
    printf("Echo server listening on port 8080...\n");
    
    // Event loop
    while (1) {
        // Wait for events with infinite timeout
        sio_wait(ctx, SIO_INFINITE_TIMEOUT, 10);
    }
    
    // Cleanup (never reached in this example)
    sio_close(server);
    sio_context_destroy(ctx);
    sio_cleanup();
    
    return 0;
}
```

### Asynchronous File Copy with Zero Copy

```c
#include <sio.h>
#include <stdio.h>
#include <stdbool.h>

// Completion callback
void transfer_complete(sio_event_t* event, void* user_data) {
    bool* done = (bool*)user_data;
    
    if (event->status == SIO_SUCCESS) {
        printf("Transfer complete: %zu bytes transferred\n", event->bytes_transferred);
    } else {
        printf("Transfer failed: %s\n", sio_error_string(event->status));
    }
    
    *done = true;
}

int main(int argc, char* argv[]) {
    if (argc != 3) {
        printf("Usage: %s <source_file> <dest_file>\n", argv[0]);
        return 1;
    }
    
    // Initialize SIO
    sio_initialize();
    
    // Create an I/O context
    sio_context_t* ctx = sio_context_create(1);
    
    // Open source file
    sio_handle_t* src = sio_file_open(ctx, argv[1], SIO_FILE_READ, 0);
    if (!src) {
        printf("Error opening source file: %s\n", argv[1]);
        return 1;
    }
    
    // Open destination file
    sio_handle_t* dest = sio_file_open(ctx, argv[2], 
                                      SIO_FILE_WRITE | SIO_FILE_CREATE | SIO_FILE_TRUNCATE, 
                                      0644);
    if (!dest) {
        printf("Error opening destination file: %s\n", argv[2]);
        sio_close(src);
        return 1;
    }
    
    // Get file size
    size_t file_size;
    sio_file_info(src, &file_size, NULL, NULL);
    
    // Flag to track completion
    bool done = false;
    
    // Perform zero-copy transfer
    int result = sio_async_transfer(src, dest, file_size, 0, transfer_complete, &done);
    
    if (result != SIO_SUCCESS) {
        printf("Failed to start transfer: %s\n", sio_error_string(result));
        sio_close(src);
        sio_close(dest);
        return 1;
    }
    
    // Wait for completion
    while (!done) {
        sio_wait(ctx, 100, 1);
    }
    
    // Cleanup
    sio_close(src);
    sio_close(dest);
    sio_context_destroy(ctx);
    sio_cleanup();
    
    return 0;
}
```

## Implementation Details

### I/O Context

At the core of SIO is the I/O context (`sio_context_t`), which encapsulates platform-specific event mechanisms:

- **Windows**: Maps to an IOCP object
- **Linux**: Maps to an epoll fd or io_uring instance
- **BSD/macOS**: Maps to a kqueue fd

Each context manages a set of handles and associated events. For optimal performance, it's recommended to create one context per thread rather than sharing contexts across threads.

### Handles

SIO handles (`sio_handle_t`) abstract various I/O resources:

- **Opaque wrapper**: Each handle wraps a native OS resource (file descriptor, socket, etc.)
- **Type safety**: The API provides type-specific functions while maintaining a unified interface
- **Resource management**: SIO tracks all handles and ensures proper cleanup

### Asynchronous I/O

SIO supports both synchronous and asynchronous I/O modes:

- **Callbacks**: Asynchronous operations use callbacks to notify completion
- **Event polling**: Applications can wait for events using `sio_wait()` or `sio_dispatch()`
- **Event loop**: A built-in event loop is provided with `sio_event_loop_start()`

### Zero-Copy Transfers

For maximum performance, SIO provides zero-copy data transfers:

- **File-to-Socket**: Uses `sendfile()` on Linux, `TransmitFile()` on Windows
- **Socket-to-File**: Uses optimized implementations based on platform capabilities
- **Scatter-Gather I/O**: Supports vectored reads and writes with minimal copying

### Buffer Management

SIO's buffer management focuses on performance:

- **Registered buffers**: Buffers can be registered with the OS for faster I/O
- **Aligned allocation**: Functions for creating aligned buffers optimal for I/O
- **Direct I/O**: Support for bypassing the OS cache (useful for storage applications)

## API Organization

SIO's API is organized into logical groups:

1. **Initialization Functions**: Library setup and teardown
2. **Socket Functions**: TCP/UDP networking operations
3. **File Functions**: File I/O operations
4. **Terminal Functions**: Console/terminal handling
5. **Device Functions**: Direct device access
6. **Timer Functions**: High-precision timers
7. **Signal Functions**: Signal handling
8. **Message Queue Functions**: Inter-process communication
9. **Shared Memory Functions**: Shared memory operations
10. **Memory Stream Functions**: In-memory I/O operations
11. **I/O Functions**: Core read/write operations
12. **Option Functions**: Configuration settings
13. **Event Handling Functions**: Event registration and dispatch
14. **Asynchronous I/O Functions**: Non-blocking operations
15. **Utility Functions**: Helper functions and tools

## Best Practices

For optimal performance with SIO:

1. **Use stack allocation** whenever possible
2. **Register buffers** for frequent I/O operations
3. **Reuse handles** rather than repeatedly opening/closing
4. **Use zero-copy** transfers for large data movements
5. **Match buffer sizes** to OS page sizes for best performance
6. **Consider direct I/O** for applications that manage their own caching
7. **Use scatter-gather I/O** to minimize system calls
8. **Set appropriate socket options** early for best networking performance

## Error Handling

SIO uses consistent error codes across platforms:

- All functions return `SIO_SUCCESS` (0) on success
- Negative return values indicate specific error conditions
- `sio_error_string()` provides human-readable error descriptions

Always check return values from SIO functions for robust error handling.

## Thread Safety

SIO handles are not thread-safe by default. For multi-threaded applications:

- Use separate contexts for each thread, or
- Implement your own synchronization around shared handles

The exception is the event loop, which is designed to be thread-safe.

## License

SIO is distributed under the MIT License. See LICENSE file for more information.

## Contributing

Contributions are welcome! Please feel free to submit a Pull Request.

## Author

SIO is created and maintained by zczxy.

---

*This document describes SIO version 0.1.0*