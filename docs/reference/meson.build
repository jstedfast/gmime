gmime_doc = gnome.gtkdoc(
  'gmime',
  main_xml: 'gmime-docs.sgml',
  module_version: api_version,
  src_dir: include_directories('../../gmime'),
  dependencies: gmime_dep,
  content_files: files(
    'building.sgml', 'compiling.sgml', 'content.sgml', 'filters.sgml',
    'question_index.sgml', 'resources.sgml', 'streams.sgml', 'tree_index.sgml',
  ),
  ignore_headers: [
    'gmime-internet-address-private.h', 'gmime-codec-private.h',
    'gmime-rfc2047-private.h', 'gmime-encoding-validator-private.h',
    'gmime-charset-map-private.h', 'gmime-table-private.h', 'gmime-parse-utils.h',
    'gmime-gpgme-utils.h', 'gmime-internal.h', 'gmime-common.h', 'gmime-events.h',
    'charset-map.c', 'gen-table.c',
  ],
  scan_args: ['--deprecated-guards=GMIME_DISABLE_DEPRECATED'],
  scanobjs_args: ['--type-init-func=g_mime_init ()'],
  mkdb_args: ['--sgml-mode', '--output-format=xml'],
  install: true,
)
alias_target('gmime-doc', gmime_doc)
