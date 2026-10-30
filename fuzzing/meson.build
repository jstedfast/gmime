fuzz_support = static_library(
  'fuzz-support', ['fuzz.c', 'mime-reader.c', 'mime-parser.c'],
  include_directories: gmime_inc,
  dependencies: gmime_dep,
)

fuzz_replay = executable(
  'gmime-fuzz', 'replay.c',
  include_directories: gmime_inc,
  dependencies: gmime_dep,
  link_with: fuzz_support,
)

foreach target : ['mime-reader', 'mime-parser']
  corpus_args = [meson.current_source_dir() / 'corpus']
  foreach directory : ['messages', 'compliance', 'partial', 'openpgp', 'dkim', 'smime']
    corpus_args += [meson.project_source_root() / 'tests' / 'data' / 'roundtrip' / directory]
  endforeach
  test(
    'fuzz-replay-' + target, fuzz_replay,
    args: ['--replay', target] + corpus_args,
    suite: 'fuzzing',
    timeout: 180,
  )
  if get_option('fuzzing') == 'libfuzzer'
    executable(
      'fuzz-' + target, 'libfuzzer.c',
      include_directories: gmime_inc,
      dependencies: gmime_dep,
      link_with: fuzz_support,
      c_args: ['-DFUZZ_TARGET=fuzz_' + target.replace('-', '_')],
      link_args: ['-fsanitize=fuzzer'],
    )
  endif
endforeach
