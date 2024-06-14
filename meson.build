project('GMime',
	['c'],
	license: 'GPLv2',
    version: '3.2.14',
    meson_version: '>=1.0.0',
    default_options : [
    ])

gmime_version = meson.project_version()
ver_arr = gmime_version.split('.')

GMIME_MAJOR_VERSION=ver_arr[0].to_int()
GMIME_MINOR_VERSION=ver_arr[1].to_int()
GMIME_MICRO_VERSION=ver_arr[2].to_int()
GMIME_BINARY_AGE= 100 * GMIME_MINOR_VERSION + GMIME_MICRO_VERSION
GMIME_INTERFACE_AGE=0

gmime_package = 'gmime-@0@.0'.format(GMIME_MAJOR_VERSION)

add_global_arguments('-DHAVE_CONFIG_H=1', language : 'c')

iconv = dependency('iconv', method: get_option('iconv'))
libidn2 = dependency('libidn2', version: '>=2.0.0', required : false)
gpgme = dependency('gpgme', version: '>=1.6.0', required: get_option('crypto'))
glib =  dependency('glib-2.0', version: '>=2.68')
gobject = dependency('gobject-2.0')
gio = dependency('gio-2.0')
deps = [
  dependency('zlib', version: '>=1.2.5.2'),
  glib,
  gpgme,
  gobject,
  gio,
  libidn2,
  iconv,
]

cc = meson.get_compiler('c')

conf_data = configuration_data()
conf_data.set_quoted('GMIME_VERSION', meson.project_version())

LIBS = [glib, gio, gobject]
EXTRA_LIBS = ['zlib']

if cc.has_header_symbol('getopt.h', 'getopt_long')
  conf_data.set('HAVE_GETOPT_H', 1)
endif

if gpgme.found()
  conf_data.set('ENABLE_CRYPTO', 1)
  EXTRA_LIBS += 'gpgme'
  EXTRA_LIBS += 'gpg-error'
endif

if libidn2.found()
  conf_data.set('LIBIDN', 1)
  EXTRA_LIBS += 'libidn2'
endif


iconv_detect = '''
#define CONFIGURE_IN
#include "iconv-detect.c"
'''

if get_option('have_iconv_detect_h')
  conf_data.set('HAVE_ICONV_DETECT_H', 1)
else
  if not meson.can_run_host_binaries()
    error('Can\'t run the iconv-detect binary. Set the have_iconv_detect_h flag or use an exe_wrapper')
  endif
  root = include_directories('.')
  has_detect = cc.run(iconv_detect, name: 'iconv-detect', 
    include_directories: root).returncode() == 0
  if has_detect
    conf_data.set('HAVE_ICONV_DETECT_H', 1)
  endif
endif

check_headers = [
  'dlfcn.h',
  'getopt.h',
  'inttypes.h',
  'netdb.h',
  'poll.h',
  'stdint.h',
  'stdio.h',
  'stdlib.h',
  'strings.h',
  'string.h',
  'sys/mman.h',
  'sys/param.h',
  'sys/stat.h',
  'sys/types.h',
  'time.h',
  'unistd.h',
  'winsock2.h',
]

foreach h : check_headers
  if cc.has_header(h)
    conf_data.set('HAVE_' + h.underscorify().to_upper(), 1)
  endif
endforeach

uts_prefix = '''
#define _GNU_SOURCE
#include <sys/utsname.h>
'''

if cc.has_member('struct utsname', 'domainname', prefix : uts_prefix)
  conf_data.set10('HAVE_UTSNAME_DOMAINNAME', true)
endif

# Don't do this, meson does it automatically if needed
# if cc.has_header_symbol('fcntl.h', 'O_LARGEFILE')
#   conf_data.set10('O_LARGEFILE', true)
# endif
#
check_functions = [
  ['HAVE_FSYNC', 'fsync', '#include<unistd.h>'],
  ['HAVE_GETADDRINFO', 'getaddrinfo', '#include<netdb.h>'],
  ['HAVE_GETDOMAINNAME', 'getdomainname', '#include <unistd.h>'],
  ['HAVE_GETHOSTNAME', 'gethostname', '#include <unistd.h>'],
  ['HAVE_GETPAGESIZE', 'getpagesize', '#include<unistd.h>'],
  ['HAVE_MMAP', 'mmap', '#include<sys/mman.h>'],
  ['HAVE_MSYNC', 'msync', '#include <sys/mman.h>'],
  ['HAVE_MUNMAP', 'munmap', '#include <sys/mman.h>'],
  ['HAVE_POLL', 'poll', '#include<poll.h>'],
  ['HAVE_SELECT', 'select', '#include<sys/select.h>'],
]

foreach f : check_functions
  if cc.has_function(f.get(1), prefix : f.get(2))
    conf_data.set(f.get(0), 1)
  endif
endforeach

configure_file(input: 'config.h.meson',
  output: 'config.h',
  configuration : conf_data
)

subdir('util')
subdir('gmime')
# if get_option('gtk_doc')
  # subdir('docs/reference_gi')
# endif
subdir('tests')

summary({'Install Prefix': get_option('prefix'),
        'Compiler': cc.get_id(),
        'Documentation': get_option('gtk_doc'),
        'Profiling enabled': false, #TODO
        'Coverage enabled': false, #TODO
        'PGP/MIME support': get_option('crypto'),
        'S/MIME support': get_option('crypto'),
        'libidn2 support': libidn2.found(),
        'GObject introspection': get_option('introspection'),
        'Vala bindings': get_option('vala'),
        }, section: 'Configuration')
