fuzz_support = static_library(
  'fuzz-support', ['fuzz.c', 'mime-reader.c', 'mime-parser.c', 'codecs.c', 'rfc2047.c',
                   'structured-headers.c', 'smtp-address-validator.c', 'tnef.c', 'filters.c', 'text.c', 'dkim.c'],
  # Reuse production objects to reach private parsers in shared-library builds too.
  objects: libgmime.extract_objects(gmime_sources, gmime_gpgme_sources),
  include_directories: gmime_inc,
  dependencies: gmime_deps,
  link_whole: util_lib,
)

fuzz_replay = executable(
  'gmime-fuzz', 'replay.c',
  include_directories: gmime_inc,
  dependencies: gmime_deps,
  link_with: fuzz_support,
)

regression_seeds = custom_target(
  'fuzz-regression-seeds',
  input: 'seed-corpora.py',
  depend_files: files('../tests/smtp-address-vectors.h', '../tests/data/authentication-results/vectors.tsv'),
  output: 'seeds.stamp',
  command: [find_program('python3'), '@INPUT@', meson.current_build_dir() / 'seeds',
            '--regressions-only', '--stamp', '@OUTPUT@'],
)

foreach target, max_length : {'mime-reader': 1048576, 'mime-parser': 1048576,
                             'encoders': 262144, 'decoders': 1048576, 'rfc2047': 65536,
                             'structured-headers': 65536, 'smtp-address-validator': 4096,
                             'dns-records': 16384, 'dkim-arc-verifier': 65536, 'dmarc-verifier': 65536,
                             'tnef-reader': 1048576, 'tnef-message': 4194304,
                             'filters': 262144, 'compliance-validators': 262144,
                             'html-tokenizer': 262144, 'text-converters': 262144,
                             'rtf-compressed': 1048576, 'rtf-converters': 1048576, 'rtf-tokenizer': 1048576}
  corpus_args = [meson.current_build_dir() / 'seeds' / target / 'corpus']
  if target == 'dmarc-verifier'
    corpus_args += [meson.current_source_dir() / 'seeds' / 'dkim-arc-verifier']
  endif
  if target in ['tnef-reader', 'tnef-message']
    corpus_args += [meson.project_source_root() / 'tests' / 'data' / 'tnef']
  endif
  if target in ['compliance-validators', 'html-tokenizer', 'rtf-converters', 'rtf-tokenizer', 'rtf-compressed', 'dkim-arc-verifier']
    corpus_args += [meson.current_source_dir() / 'seeds' / target]
  endif
  if target in ['mime-reader', 'mime-parser']
    corpus_args += [meson.current_source_dir() / 'corpus']
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
      dependencies: gmime_deps,
      link_with: fuzz_support,
      c_args: ['-DFUZZ_TARGET=fuzz_' + target.replace('-', '_'),
               '-DFUZZ_MAX_INPUT_LENGTH=' + max_length.to_string()],
      link_args: ['-fsanitize=fuzzer'],
    )
  endif
endforeach
