docbook2html = find_program('docbook2html', 'db2html', required: false)
if docbook2html.found()
  run_target(
    'gmime-tutorial',
    command: [docbook2html, '-o', meson.current_build_dir() / 'html', files('gmime-tut.sgml')],
  )
endif
