project('GMime',
	['c'],
	license: 'GPLv2',
    version: '3.2.13',
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

iconv = dependency('iconv', required : false, method: get_option('iconv'))
libidn2 = dependency('libidn2', version: '>=2.0.0', required : false)
gpgme = dependency('gpgme', version: '>=1.6.0', required: get_option('crypto'))
glib =  dependency('glib-2.0', version: '>=2.68')
gobject = dependency('gobject-2.0')
deps = [
  dependency('zlib', version: '>=1.2.5.2'),
  glib,
  gpgme,
  gobject,
  dependency('gio-2.0'),
  libidn2,
  iconv,
]

gmime_prefix = get_option('prefix')
gmime_datadir = join_paths(gmime_prefix, get_option('datadir'))

cc = meson.get_compiler('c')

conf_data = configuration_data()
conf_data.set_quoted('GMIME_VERSION', meson.project_version())

LIBS = ['glib-2.0', 'gio-2.0', 'gobject-2.0' ]
EXTRA_LIBS = ['zlib']

if cc.has_header('getopt.h')
  if cc.has_function('getopt_long')
    conf_data.set('HAVE_GETOPT_H', 1)
  endif
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
  # not cross-compiler secure
  root = include_directories('.')
  has_detect = cc.run(iconv_detect, name: 'iconv-detect', 
    include_directories: root).returncode() == 0
  if has_detect
    conf_data.set('HAVE_ICONV_DETECT_H', 1)
  endif
endif

foreach hd : [
    { 'm': 'HAVE_DLFCN_H',            'v': 'dlfcn.h', },
    { 'm': 'HAVE_INTTYPES_H',         'v': 'inttypes.h', },
    { 'm': 'HAVE_NETDB_H',            'v': 'netdb.h', },
    { 'm': 'HAVE_STDINT_H',           'v': 'stdint.h', },
    { 'm': 'HAVE_WINDOWS_H',          'v': 'windows.h'},
    { 'm': 'HAVE_SYS_MMAN_H',         'v': 'sys/mman.h', },
    { 'm': 'HAVE_SYS_PARAM_H',        'v': 'sys/param.h', },
  ]
  if cc.has_header(hd['v'])
    conf_data.set10(hd['m'], true)
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

foreach fn : [
    { 'm': 'HAVE_FSYNC',                    'v': 'fsync', },
    { 'm': 'HAVE_GETADDRINFO',              'v': 'getaddrinfo', },
    { 'm': 'HAVE_GETDOMAINNAME',            'v': 'getdomainname', },
    { 'm': 'HAVE_GETHOSTNAME',              'v': 'gethostname', },
    { 'm': 'HAVE_GETPAGESIZE',              'v': 'getpagesize', },
    { 'm': 'HAVE_MMAP',                     'v': 'mmap', },
    { 'm': 'HAVE_MSYNC',                    'v': 'msync', },
    { 'm': 'HAVE_MUNMAP',                   'v': 'munmap', },
    { 'm': 'HAVE_POLL',                     'v': 'poll', },
    { 'm': 'HAVE_SELECT',                   'v': 'select', },
  ]
  if cc.has_function(fn['v'])
    conf_data.set(fn['m'], 1)
  endif
endforeach

configure_file(input: 'config_meson.h.in',
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
