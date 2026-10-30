fuzz_support = static_library(
  'fuzz-support', ['fuzz.c', 'mime-reader.c', 'mime-parser.c', 'codecs.c', 'rfc2047.c'],
  include_directories: gmime_inc,
  dependencies: gmime_dep,
)

fuzz_replay = executable(
  'gmime-fuzz', 'replay.c',
  include_directories: gmime_inc,
  dependencies: gmime_dep,
  link_with: fuzz_support,
)

regression_seeds = custom_target(
  'fuzz-regression-seeds',
  input: 'seed-corpora.py',
  output: 'seeds.stamp',
  command: [find_program('python3'), '@INPUT@', meson.current_build_dir() / 'seeds',
            '--regressions-only', '--stamp', '@OUTPUT@'],
)

foreach target, max_length : {'mime-reader': 1048576, 'mime-parser': 1048576,
                             'encoders': 262144, 'decoders': 1048576, 'rfc2047': 65536}
  corpus_args = [meson.current_build_dir() / 'seeds' / target / 'corpus']
  if target in ['mime-reader', 'mime-parser']
    corpus_args = [meson.current_source_dir() / 'corpus']
    foreach directory : ['messages', 'compliance', 'partial', 'openpgp', 'dkim', 'smime']
      corpus_args += [meson.project_source_root() / 'tests' / 'data' / 'roundtrip' / directory]
    endforeach
  endif
  test(
    'fuzz-replay-' + target, fuzz_replay,
    args: ['--replay', target] + corpus_args,
    suite: 'fuzzing',
    timeout: 180,
    depends: regression_seeds,
  )
  if get_option('fuzzing') == 'libfuzzer'
    executable(
      'fuzz-' + target, 'libfuzzer.c',
      include_directories: gmime_inc,
      dependencies: gmime_dep,
      link_with: fuzz_support,
      c_args: ['-DFUZZ_TARGET=fuzz_' + target.replace('-', '_'),
               '-DFUZZ_MAX_INPUT_LENGTH=' + max_length.to_string()],
      link_args: ['-fsanitize=fuzzer'],
    )
  endif
endforeach
