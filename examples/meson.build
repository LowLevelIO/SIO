# Examples build configuration

example_programs = [
  ['tcp_echo_server', 'tcp_echo_server.c'],
  ['file_copy', 'file_copy.c'],
  ['socket_client', 'socket_client.c'],
  ['device_access', 'device_access.c'],
  ['async_io', 'async_io.c'],
  ['event_loop', 'event_loop.c'],
  ['zero_copy', 'zero_copy.c'],
  ['memory_mapping', 'memory_mapping.c'],
  ['timer_example', 'timer_example.c'],
  ['signal_handling', 'signal_handling.c'],
  ['terminal_io', 'terminal_io.c'],
]

foreach example : example_programs
  executable(example[0],
    example[1],
    dependencies : [sio_dep],
    install : false
  )
endforeach