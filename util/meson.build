util_src = files(
  'gtrie.c',
  'packed.c',
  'url-scanner.c',
)

inc_conf = include_directories('..')
libutil = static_library('util', util_src, dependencies : deps, include_directories: inc_conf)
