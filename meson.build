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

# Include directories
inc = include_directories('include')
srcinc = include_directories('.')

# Auxiliary Sources
aux_sources = [
  'src/aux/dir.c',
  'src/aux/addr.c'
]

# Global Sources
global_sources = [
  'src/err.c',
  'src/buf.c',
  'src/stream.c'
]

# Stream Sources
stream_sources = [
  'src/stream/device.c',
  'src/stream/file.c',
  'src/stream/memory.c',
  'src/stream/msgqueue.c',
  'src/stream/pipe.c',
  'src/stream/shared_memory.c',
  'src/stream/signal.c',
  'src/stream/sock.c',
  'src/stream/terminal.c',
  'src/stream/timer.c',
  'src/stream/window.c'
]

# Context Sources
context_sources = [
  'src/context/epoll.c',
  'src/context/io_uring.c',
  'src/context/IOCP.c',
  'src/context/kqueue.c'
]

# Combined source list
sio_sources = global_sources + aux_sources + stream_sources + context_sources

# Library
sio_lib = library('sio',
  sio_sources,
  include_directories : [inc, srcinc],
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
