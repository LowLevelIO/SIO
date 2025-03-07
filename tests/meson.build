# Testing build configuration

test_programs = [
  ['testaddr', 'aux_addr.c'],
  ['testbuf', 'buf.c']
]

foreach example : test_programs
  executable(example[0],
    example[1],
    dependencies : [sio_dep],
    install : false
  )
endforeach