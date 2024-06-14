libgmime_src = files(
  'gmime-application-pkcs7-mime.c',
  'gmime-autocrypt.c',
  'gmime.c',
  'gmime-certificate.c',
  'gmime-charset.c',
  'gmime-common.c',
  'gmime-content-type.c',
  'gmime-crypto-context.c',
  'gmime-data-wrapper.c',
  'gmime-disposition.c',
  'gmime-encodings.c',
  'gmime-events.c',
  'gmime-filter-basic.c',
  'gmime-filter-best.c',
  'gmime-filter.c',
  'gmime-filter-charset.c',
  'gmime-filter-checksum.c',
  'gmime-filter-dos2unix.c',
  'gmime-filter-enriched.c',
  'gmime-filter-from.c',
  'gmime-filter-gzip.c',
  'gmime-filter-html.c',
  'gmime-filter-openpgp.c',
  'gmime-filter-smtp-data.c',
  'gmime-filter-strip.c',
  'gmime-filter-unix2dos.c',
  'gmime-filter-windows.c',
  'gmime-filter-yenc.c',
  'gmime-format-options.c',
  'gmime-gpg-context.c',
  'gmime-gpgme-utils.c',
  'gmime-header.c',
  'gmime-iconv.c',
  'gmime-iconv-utils.c',
  'gmime-message.c',
  'gmime-message-part.c',
  'gmime-message-partial.c',
  'gmime-multipart.c',
  'gmime-multipart-encrypted.c',
  'gmime-multipart-signed.c',
  'gmime-object.c',
  'gmime-param.c',
  'gmime-parser.c',
  'gmime-parser-options.c',
  'gmime-parse-utils.c',
  'gmime-part.c',
  'gmime-part-iter.c',
  'gmime-pkcs7-context.c',
  'gmime-references.c',
  'gmime-signature.c',
  'gmime-stream-buffer.c',
  'gmime-stream.c',
  'gmime-stream-cat.c',
  'gmime-stream-file.c',
  'gmime-stream-filter.c',
  'gmime-stream-fs.c',
  'gmime-stream-gio.c',
  'gmime-stream-mem.c',
  'gmime-stream-mmap.c',
  'gmime-stream-null.c',
  'gmime-stream-pipe.c',
  'gmime-text-part.c',
  'gmime-utils.c',
  'internet-address.c',
)

private = [
  'gmime-charset-map-private.h',
  'gmime-table-private.h',
  'gmime-parse-utils.h',
  'gmime-gpgme-utils.h',
  'gmime-internal.h',
  'gmime-common.h',
  'gmime-events.h',
]

libgmime_headers = [
  'gmime-application-pkcs7-mime.h',
  'gmime-autocrypt.h',
  'gmime-certificate.h',
  'gmime-charset.h',
  'gmime-content-type.h',
  'gmime-crypto-context.h',
  'gmime-data-wrapper.h',
  'gmime-disposition.h',
  'gmime-encodings.h',
  'gmime-error.h',
  'gmime-filter-basic.h',
  'gmime-filter-best.h',
  'gmime-filter-charset.h',
  'gmime-filter-checksum.h',
  'gmime-filter-dos2unix.h',
  'gmime-filter-enriched.h',
  'gmime-filter-from.h',
  'gmime-filter-gzip.h',
  'gmime-filter.h',
  'gmime-filter-html.h',
  'gmime-filter-openpgp.h',
  'gmime-filter-smtp-data.h',
  'gmime-filter-strip.h',
  'gmime-filter-unix2dos.h',
  'gmime-filter-windows.h',
  'gmime-filter-yenc.h',
  'gmime-format-options.h',
  'gmime-gpg-context.h',
  'gmime.h',
  'gmime-header.h',
  'gmime-iconv.h',
  'gmime-iconv-utils.h',
  'gmime-message.h',
  'gmime-message-part.h',
  'gmime-message-partial.h',
  'gmime-multipart-encrypted.h',
  'gmime-multipart.h',
  'gmime-multipart-signed.h',
  'gmime-object.h',
  'gmime-param.h',
  'gmime-parser.h',
  'gmime-parser-options.h',
  'gmime-part.h',
  'gmime-part-iter.h',
  'gmime-pkcs7-context.h',
  'gmime-references.h',
  'gmime-signature.h',
  'gmime-stream-buffer.h',
  'gmime-stream-cat.h',
  'gmime-stream-file.h',
  'gmime-stream-filter.h',
  'gmime-stream-fs.h',
  'gmime-stream-gio.h',
  'gmime-stream.h',
  'gmime-stream-mem.h',
  'gmime-stream-mmap.h',
  'gmime-stream-null.h',
  'gmime-stream-pipe.h',
  'gmime-text-part.h',
  'gmime-utils.h',
  'internet-address.h',
]

inc_conf = include_directories('..', '../util')

version_data = configuration_data()
version_data.set('GMIME_MAJOR_VERSION', GMIME_MAJOR_VERSION)
version_data.set('GMIME_MINOR_VERSION', GMIME_MINOR_VERSION)
version_data.set('GMIME_MICRO_VERSION', GMIME_MICRO_VERSION)
version_data.set('GMIME_BINARY_AGE', GMIME_BINARY_AGE)
version_data.set('GMIME_INTERFACE_AGE', GMIME_INTERFACE_AGE)
configure_file(input: 'gmime-version_meson.h.in',
  output: 'gmime-version.h',
  configuration : version_data
)

gen_table = executable('gen-table', 'gen-table.c', native: true)
charset_map = executable('charset-map', 'charset-map.c', dependencies : deps, 
  include_directories: inc_conf, native: true)

# For the future, we can generate the source code at runtime instead of having
# it in the filesystem
# table_h = custom_target('gen-table',
#   capture: true,
#   output : ['gmime-table-private.h'],
#   command : [gen_table])
# charset_h = custom_target('charset-map',
#   capture: true,
#   output : ['gmime-charset-map-private.h'],
#   command : [charset_map])
#
# libgmime = library(gmime_package, libgmime_src + table_h + charset_h, dependencies : deps, 
#   include_directories: inc_conf, link_with: libutil, install: true)

libgmime = library(gmime_package, libgmime_src, dependencies : deps, 
  include_directories: inc_conf, link_with: libutil, install: true)

pkg = import('pkgconfig')

pkg.generate(libgmime,
  name: meson.project_name(),
  filebase: gmime_package,
  subdirs: gmime_package,
  description: 'MIME parser and utility library',
  requires: LIBS,
  # libraries_private: EXTRA_LIBS,
  )

install_headers(libgmime_headers, subdir: gmime_package / 'gmime')

gnome = import('gnome')

src_doc = meson.project_source_root() / 'gmime'

if get_option('introspection')
  gir_args = [
    '--accept-unprefixed',
    ]

  gmime_gir = gnome.generate_gir(
    libgmime,
    sources: libgmime_src + libgmime_headers,
    export_packages: [gmime_package],
    header: 'gmime/gmime.h',
    namespace: 'GMime',
    nsversion: '3.0',
    symbol_prefix: ['g_mime', 'gmime'],
    identifier_prefix : 'GMime',
    includes: [ 'GObject-2.0', 'Gio-2.0'],
    dependencies: deps,
    extra_args: gir_args,
    fatal_warnings: true,
    install: true,
  )
endif

if get_option('vala')
  gnome.generate_vapi(gmime_package,
    sources: gmime_gir[0],
    packages: [ 'gio-2.0' ],
    install: true)
endif
