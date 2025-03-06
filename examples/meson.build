# Examples build configuration

example_programs = [

]

foreach example : example_programs
  executable(example[0],
    example[1],
    dependencies : [sio_dep],
    install : false
  )
endforeach