project(
  'gmime', 'c',
  version: '4.0.0',
  meson_version: '>= 1.3.0',
  license: 'LGPL-2.1-or-later',
  default_options: ['c_std=gnu11', 'warning_level=1', 'default_library=shared'],
)

cc = meson.get_compiler('c')
gnome = import('gnome')
pkgconfig = import('pkgconfig')
add_project_arguments('-DHAVE_CONFIG_H', language: 'c')
add_project_arguments(cc.get_supported_arguments('-fno-strict-aliasing'), language: 'c')

if get_option('fuzzing') == 'libfuzzer'
  if cc.get_id() != 'clang' or not cc.has_argument('-fsanitize=fuzzer-no-link')
    error('libFuzzer requires Clang with -fsanitize=fuzzer-no-link support')
  endif
  # Instrument the library as well as the harness, without adding a fuzzer main.
  add_project_arguments('-fsanitize=fuzzer-no-link', language: 'c')
  add_project_link_arguments('-fsanitize=fuzzer-no-link', language: 'c')
endif

api_version = '4.0'
version_parts = meson.project_version().split('.')
interface_age = 0
binary_age = 100 * version_parts[1].to_int() + version_parts[2].to_int()
lt_current = binary_age - interface_age
lt_age = binary_age - interface_age
library_soversion = (lt_current - lt_age).to_string()
library_version = '@0@.@1@.@2@'.format(library_soversion, lt_age, interface_age)

glib_version = '>= 2.88.0'
glib_dep = dependency('glib-2.0', version: glib_version)
gobject_dep = dependency('gobject-2.0', version: glib_version)
gio_dep = dependency('gio-2.0', version: glib_version)
gthread_dep = dependency('gthread-2.0', version: glib_version)
zlib_dep = dependency('zlib')
iconv_dep = dependency('iconv')
public_deps = [glib_dep, gobject_dep, gio_dep]
private_deps = [gthread_dep, zlib_dep, iconv_dep]

gpgme_dep = dependency('gpgme', version: '>= 1.6.0', required: get_option('crypto'))
gpg_error_dep = dependency('gpg-error', required: get_option('crypto'))
have_crypto = gpgme_dep.found() and gpg_error_dep.found()
if have_crypto
  private_deps += [gpgme_dep, gpg_error_dep]
endif

idn_dep = dependency('libidn2', version: '>= 2.0.0', required: get_option('libidn'))
if idn_dep.found()
  private_deps += idn_dep
endif

if host_machine.system() == 'windows'
  private_deps += cc.find_library('ws2_32')
elif host_machine.system() == 'sunos'
  private_deps += [cc.find_library('socket'), cc.find_library('nsl')]
endif
gmime_deps = public_deps + private_deps

introspection_option = get_option('introspection')
if get_option('default_library') == 'static' and introspection_option.enabled()
  error('Introspection requires a shared library; use -Ddefault_library=shared or both')
endif
introspection_dep = dependency(
  'gobject-introspection-1.0',
  version: '>= 1.30.0',
  required: introspection_option.disable_auto_if(get_option('default_library') == 'static'),
)
have_introspection = introspection_dep.found()
if get_option('vapi').enabled() and not have_introspection
  error('Vala bindings require introspection')
endif
vapigen = find_program('vapigen', required: get_option('vapi').disable_auto_if(not have_introspection))
have_vapi = vapigen.found()

config = configuration_data()
config.set_quoted('GMIME_VERSION', meson.project_version())
config.set('ENABLE_CRYPTO', have_crypto)
config.set('ENABLE_WARNINGS', get_option('mime_warnings'))
config.set('LIBIDN', idn_dep.found())

foreach header : [
  'fcntl.h', 'getopt.h', 'inttypes.h', 'langinfo.h', 'netdb.h', 'poll.h',
  'stdint.h', 'stdlib.h', 'string.h', 'sys/mman.h', 'sys/param.h',
  'sys/time.h', 'time.h', 'unistd.h', 'windows.h', 'winsock2.h',
]
  config.set('HAVE_' + header.underscorify().to_upper(), cc.has_header(header))
endforeach
foreach function : [
  'fsync', 'getaddrinfo', 'getdomainname', 'gethostname', 'gettimeofday',
  'mmap', 'msync', 'munmap', 'nl_langinfo', 'poll', 'select', 'tzset', 'uname',
]
  config.set('HAVE_' + function.to_upper(), cc.has_function(function, dependencies: private_deps))
endforeach
config.set('HAVE_CODESET', cc.has_header_symbol('langinfo.h', 'CODESET'))
config.set('HAVE_GETOPT_H', cc.has_function('getopt_long', prefix: '#include <getopt.h>'))
config.set('HAVE_UTSNAME_DOMAINNAME', cc.has_member(
  'struct utsname', 'domainname',
  prefix: '#define _GNU_SOURCE\n#include <sys/utsname.h>',
))
if not cc.has_header_symbol('sys/param.h', 'MAXHOSTNAMELEN')
  config.set('MAXHOSTNAMELEN', 64)
endif
if not cc.has_header_symbol('fcntl.h', 'O_LARGEFILE', prefix: '#define _GNU_SOURCE')
  config.set('O_LARGEFILE', 0)
endif
if not cc.has_type('nfds_t', prefix: '#include <sys/types.h>\n#include <poll.h>')
  config.set('nfds_t', 'unsigned long')
endif

iconv_header = []
if meson.can_run_host_binaries()
  iconv_detector = executable('iconv-detect', 'iconv-detect.c', dependencies: iconv_dep, build_by_default: false)
  iconv_header = custom_target('iconv-detect.h', output: 'iconv-detect.h', command: iconv_detector)
  config.set('HAVE_ICONV_DETECT_H', 1)
else
  warning('Cannot run the iconv detector; using the platform charset aliases in gmime-charset.c')
endif
configure_file(output: 'config.h', configuration: config)
spec_config = configuration_data()
spec_config.set('VERSION', meson.project_version())
spec_config.set('GMIME_API_VERSION', api_version)
configure_file(input: 'gmime.spec.in', output: 'gmime.spec', configuration: spec_config)

gmime_inc = include_directories('.', 'util')
subdir('util')
subdir('gmime')
if get_option('tests')
  subdir('tests')
endif
if get_option('fuzzing') != 'disabled'
  subdir('fuzzing')
endif
if get_option('examples') and host_machine.system() != 'windows'
  subdir('examples')
endif
if get_option('gtk_doc')
  subdir('docs')
endif

summary({
  'Crypto (GPGME)': have_crypto,
  'IDN backend': idn_dep.found() ? 'libidn2' : 'GLib',
  'Introspection': have_introspection,
  'Vala bindings': have_vapi,
  'API reference': get_option('gtk_doc'),
  'Tests': get_option('tests'),
  'Examples': get_option('examples') and host_machine.system() != 'windows',
}, section: 'GMime')
