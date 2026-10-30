foreach name : ['basic-example', 'imap-example', 'uuencode', 'uudecode', 'msgcheck']
  executable(
    name, name + '.c',
    dependencies: gmime_dep,
    link_with: util_lib,
  )
endforeach

mime_analyzer = executable(
  'mime-analyzer', 'mime-analyzer.c',
  dependencies: gmime_dep,
)

if get_option('tests')
  mime_analyzer_test = executable(
    'test-mime-analyzer', '../tests/test-mime-analyzer.c',
    dependencies: gmime_dep,
  )
  test(
    'test-mime-analyzer', mime_analyzer_test,
    args: [mime_analyzer.full_path()],
    depends: [mime_analyzer],
    suite: 'examples',
  )
endif
