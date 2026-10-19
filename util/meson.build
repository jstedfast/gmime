util_lib = static_library(
  'gmime-util',
  'gtrie.c', 'packed.c', 'url-scanner.c',
  include_directories: gmime_inc,
  dependencies: glib_dep,
  c_args: ['-DG_LOG_DOMAIN="util"'],
  pic: true,
)
