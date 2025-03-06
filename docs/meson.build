# Documentation build configuration

# Find Doxygen
doxygen = find_program('doxygen', required : true)

# Configure Doxyfile
doxyfile = configure_file(
  input : 'Doxyfile.in',
  output : 'Doxyfile',
  configuration : {
    'SIO_VERSION' : meson.project_version(),
    'SIO_SOURCE_DIR' : meson.project_source_root(),
    'SIO_BUILD_DIR' : meson.project_build_root(),
    'SIO_DOCS_OUTPUT_DIR' : meson.current_build_dir() / 'html',
  }
)

# Copy additional documentation files
markdown_files = [
#  'additional_docs/usage_guide.md',
#  'additional_docs/performance_tips.md',
#  'additional_docs/platform_notes.md',
]

foreach file : markdown_files
  configure_file(
    input : file,
    output : '@BASENAME@.md',
    copy : true
  )
endforeach

# Create custom target for documentation
custom_target('docs',
  input : doxyfile,
  output : 'html',
  command : [doxygen, '@INPUT@'],
  build_by_default : false,
  install : false
)
