# Testing build configuration

test_programs = [

]

foreach example : test_programs
  executable(example[0],
    example[1],
    dependencies : [sio_dep],
    install : false
  )
endforeach