compliance_benchmark = executable(
  'benchmark-compliance-validators',
  ['benchmark-compliance-validators.c', '../gmime/gmime-encoded-word-validator.c',
   '../gmime/gmime-parameter-validator.c'],
  include_directories: gmime_inc,
  dependencies: public_deps,
  build_by_default: false,
)
benchmark('compliance-validators', compliance_benchmark)

benchmark_exe = executable(
  'benchmark-gmime',
  ['benchmark.c', 'benchmark-headers.c', 'benchmark-parser.c', 'benchmark-codecs.c'],
  # Private validators must share the parser's GTypes, not register a second copy.
  objects: libgmime.extract_all_objects(recursive: false),
  include_directories: gmime_inc,
  dependencies: gmime_deps,
  link_whole: util_lib,
  build_by_default: false,
)
benchmark_data_args = [
  '--data', meson.current_source_dir() / 'data',
  '--test-data', meson.project_source_root() / 'tests' / 'data',
]
benchmark('benchmark', benchmark_exe,
  args: benchmark_data_args,
  timeout: 300,
)
test('benchmark-smoke', benchmark_exe,
  args: ['--verify'] + benchmark_data_args,
  suite: 'benchmarks',
  is_parallel: false,
  timeout: 120,
)
