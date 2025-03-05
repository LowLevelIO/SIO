project('sio', 'c',
  version : '0.1.0',
  default_options : [
    'c_std=c11',
    'warning_level=3',
    'optimization=3',
    'default_library=both'
  ]
)

# Dependencies (minimal external dependencies)
threads_dep = dependency('threads')

# Compiler flags
compiler = meson.get_compiler('c')
if compiler.get_id() == 'gcc' or compiler.get_id() == 'clang'
  add_project_arguments('-D_GNU_SOURCE', language : 'c')
  add_project_arguments('-fvisibility=hidden', language : 'c')
  if get_option('optimization') == '3'
    add_project_arguments('-flto', language : 'c')
  endif
endif

# Configuration - detect platform features
conf = configuration_data()
conf.set('HAVE_EPOLL', compiler.has_header('sys/epoll.h'))
conf.set('HAVE_KQUEUE', compiler.has_header('sys/event.h'))
conf.set('HAVE_IOCP', host_machine.system() == 'windows')
conf.set('HAVE_IO_URING', compiler.has_header('liburing.h') and compiler.has_function('io_uring_queue_init'))
conf.set('HAVE_MMAP', compiler.has_function('mmap'))
conf.set('HAVE_SENDFILE', compiler.has_function('sendfile'))
conf.set('HAVE_SPLICE', compiler.has_function('splice'))
conf.set('HAVE_COPY_FILE_RANGE', compiler.has_function('copy_file_range'))
conf.set('SIO_VERSION', '"@0@"'.format(meson.project_version()))

# Generate configuration header
configure_file(
  output : 'sio_config.h',
  configuration : conf
)

# Include directories
inc = include_directories('include')

# Core Source Files
core_sources = [
  'src/core/sio_init.c',       # Initialization and cleanup
  'src/core/sio_error.c',      # Error handling and messages
  'src/core/sio_context.c',    # I/O context management
  'src/core/sio_handle.c',     # Handle management and operations
  'src/core/sio_buffer.c',     # Buffer management
  'src/core/sio_utils.c',      # Utility functions
]

# Platform-specific Source Files
platform_sources = []
if host_machine.system() == 'windows'
  platform_sources += [
    'src/platform/sio_windows.c',           # Windows-specific implementation
    'src/platform/sio_windows_iocp.c',      # IOCP implementation
    'src/platform/sio_windows_socket.c',    # Windows socket implementation
    'src/platform/sio_windows_file.c',      # Windows file implementation
    'src/platform/sio_windows_event.c',     # Windows event handling
    'src/platform/sio_windows_pipe.c',      # Windows pipe implementation
    'src/platform/sio_windows_timer.c',     # Windows timer implementation
    'src/platform/sio_windows_device.c',    # Windows device implementation
  ]
elif host_machine.system() == 'linux'
  platform_sources += [
    'src/platform/sio_linux.c',             # Linux-specific implementation
    'src/platform/sio_linux_epoll.c',       # Linux epoll implementation
    'src/platform/sio_linux_io_uring.c',    # Linux io_uring implementation
    'src/platform/sio_linux_socket.c',      # Linux socket implementation
    'src/platform/sio_linux_file.c',        # Linux file implementation
    'src/platform/sio_linux_event.c',       # Linux event handling
    'src/platform/sio_linux_timer.c',       # Linux timer implementation
    'src/platform/sio_linux_device.c',      # Linux device implementation
  ]
elif host_machine.system() == 'darwin' or host_machine.system() == 'freebsd'
  platform_sources += [
    'src/platform/sio_bsd.c',               # BSD/macOS specific implementation
    'src/platform/sio_bsd_kqueue.c',        # kqueue implementation
    'src/platform/sio_bsd_socket.c',        # BSD socket implementation
    'src/platform/sio_bsd_file.c',          # BSD file implementation
    'src/platform/sio_bsd_event.c',         # BSD event handling
    'src/platform/sio_bsd_timer.c',         # BSD timer implementation
    'src/platform/sio_bsd_device.c',        # BSD device implementation
  ]
endif

# Common POSIX sources (used by Linux, BSD, macOS)
if host_machine.system() != 'windows'
  platform_sources += [
    'src/platform/sio_posix.c',             # Common POSIX implementations
    'src/platform/sio_posix_pipe.c',        # POSIX pipe implementation
    'src/platform/sio_posix_signal.c',      # POSIX signal handling
  ]
endif

# I/O Type Implementation Sources
io_sources = [
  'src/io/sio_socket.c',        # Socket interface
  'src/io/sio_file.c',          # File interface
  'src/io/sio_pipe.c',          # Pipe interface
  'src/io/sio_terminal.c',      # Terminal interface
  'src/io/sio_device.c',        # Device interface
  'src/io/sio_timer.c',         # Timer interface
  'src/io/sio_signal.c',        # Signal interface
  'src/io/sio_msgqueue.c',      # Message queue interface
  'src/io/sio_sharedmem.c',     # Shared memory interface
  'src/io/sio_memory.c',        # Memory stream interface
]

# Event Loop Implementation Sources
event_sources = [
  'src/event/sio_event.c',      # Event handling core
  'src/event/sio_async.c',      # Asynchronous I/O operations
  'src/event/sio_loop.c',       # Event loop implementation
]

# Combined source list
sio_sources = core_sources + platform_sources + io_sources + event_sources

# Library
sio_lib = library('sio',
  sio_sources,
  include_directories : inc,
  dependencies : [threads_dep],
  install : true,
  c_args : ['-DSIO_BUILDING_LIBRARY']
)

# Public headers
install_headers('include/sio.h')

# Dependencies for users of the library
sio_dep = declare_dependency(
  include_directories : inc,
  link_with : sio_lib,
)

# Examples
if get_option('build_examples')
  subdir('examples')
endif

# Tests
if get_option('build_tests')
  subdir('tests')
endif

# Documentation (if Doxygen is available)
doxygen = find_program('doxygen', required : false)
if doxygen.found() and get_option('build_docs')
  subdir('docs')
endif

# Generate pkg-config file
pkg = import('pkgconfig')
pkg.generate(
  name : 'sio',
  description : 'Simple I/O - Cross-platform I/O library for high-performance systems programming',
  libraries : sio_lib,
  version : meson.project_version(),
)