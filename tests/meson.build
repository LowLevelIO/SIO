# Test configuration

# Source files for the stream tests
stream_test_sources = [
  'stream.c',      # Main test program
  'stream_file.c', # File stream tests
  'stream_memory.c', # Memory stream tests
  'stream_socket.c', # Socket stream tests 
  'stream_timer.c',  # Timer stream tests
  'stream_signal.c'  # Signal stream tests
]

# Create the stream test executable
stream_test = executable('stream_test',
  stream_test_sources,
  dependencies : [sio_dep],
  install : false
)

# Add other existing tests
test_programs = [
  ['testaddr', 'aux_addr.c'],
  ['testthread', 'aux_thread.c'],
  ['testbuf', 'buf.c']
]

foreach example : test_programs
  executable(example[0],
    example[1],
    dependencies : [sio_dep],
    install : false
  )
endforeach

# Register tests
test('stream', stream_test)
# test('buffer', executable('testbuf', 'buf.c', dependencies : [sio_dep]))
# test('address', executable('testaddr', 'aux_addr.c', dependencies : [sio_dep]))