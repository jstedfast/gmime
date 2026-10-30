tests_private_args = ['-DGMIME_PRIVATE_TESTS']

testsuite_lib = static_library(
  'testsuite', 'testsuite.c',
  include_directories: gmime_inc,
  dependencies: gmime_deps,
  c_args: ['-DG_LOG_DOMAIN="gmime-tests"'],
)

if have_crypto
  # Capture canonicalized verification input without platform-specific interposition.
  verify_test_lib = static_library(
    'gmime-verify-test', gmime_gpgme_sources,
    objects: libgmime.extract_objects(gmime_sources),
    include_directories: gmime_inc,
    dependencies: gmime_deps,
    link_whole: util_lib,
    c_args: ['-DG_LOG_DOMAIN="gmime"', '-Dgpgme_op_verify=gmime_test_op_verify'],
  )
  verify_test_dep = declare_dependency(
    link_with: verify_test_lib,
    include_directories: gmime_inc,
    dependencies: gmime_deps,
  )
endif

normal_tests = [
  'test-stream-block', 'test-substreams',
  'test-received-clauses', 'test-received',
  'test-anonymizer',
  'test-content-type',
  'test-openpgp',
  'test-reader', 'test-parser-model', 'test-roundtrip',
  'test-core', 'test-visitor', 'test-iterator', 'test-attachments', 'test-body-builder', 'test-addresses', 'test-params', 'test-iconv', 'test-streams', 'test-cat',
  'test-encodings', 'test-codecs', 'test-yenc', 'test-rfc2231', 'test-filters', 'test-best-encoding', 'test-headers',
  'test-mime-part', 'test-mime-whitespace', 'test-partial', 'test-mbox', 'test-autocrypt', 'test-mime', 'test-types',
]
crypto_tests = ['test-pgp', 'test-pgpmime']
if have_crypto
  normal_tests += crypto_tests
endif

test_programs = {
  'test-encapsulation': executable(
    'test-encapsulation', 'test-encapsulation.c',
    include_directories: gmime_inc,
    dependencies: gmime_dep,
    link_with: testsuite_lib,
    c_args: ['-DG_LOG_DOMAIN="gmime-tests"'],
  ),
}
foreach name : normal_tests
  sources = [name + '.c']
  test_dep = have_crypto and name == 'test-parser-model' ? verify_test_dep : gmime_dep
  if name == 'test-core'
    sources += ['../gmime/gmime-events.c']
  elif name == 'test-rfc2231'
    sources += ['../gmime/gmime-rfc2231-encoder.c', '../gmime/gmime-rfc2231-decoder.c']
  endif
  test_programs += {name: executable(
    name, sources,
    include_directories: gmime_inc,
    dependencies: test_dep,
    link_with: testsuite_lib,
    c_args: ['-DG_LOG_DOMAIN="gmime-tests"'] + tests_private_args,
  )}
endforeach

standalone_sources = {
  'address-validator': ['../gmime/gmime-address-validator.c'],
  'reader-simd': ['../gmime/gmime-reader-simd.c'],
  'codecs': [
    '../gmime/gmime-encoder.c', '../gmime/gmime-decoder.c', '../gmime/gmime-codec-simd.c',
    '../gmime/gmime-base64-encoder.c', '../gmime/gmime-base64-decoder.c',
    '../gmime/gmime-qp-encoder.c', '../gmime/gmime-qp-decoder.c',
    '../gmime/gmime-uu-encoder.c', '../gmime/gmime-uu-decoder.c',
    '../gmime/gmime-pass-through-encoder.c', '../gmime/gmime-pass-through-decoder.c',
    '../gmime/gmime-yenc-encoder.c', '../gmime/gmime-yenc-decoder.c', '../gmime/gmime-crc32.c',
  ],
  'rfc2047': [
    '../gmime/gmime-rfc2047-encoder.c', '../gmime/gmime-rfc2047-base64-encoder.c',
    '../gmime/gmime-rfc2047-qp-encoder.c', '../gmime/gmime-codec-simd.c',
  ],
  'encoding-validators': ['../gmime/gmime-encoding-validator.c', '../gmime/gmime-codec-simd.c'],
}
test_programs += {'test-charsets': executable(
  'test-charsets', ['test-charsets.c', '../gmime/gmime-charset.c', '../gmime/gmime-iconv.c'], iconv_header,
  include_directories: gmime_inc,
  # Compile the private charset helpers locally, but resolve their stream API in GMime.
  dependencies: gmime_dep,
  c_args: ['-DG_LOG_DOMAIN="gmime-tests"'] + tests_private_args,
)}
foreach name, sources : standalone_sources
  foreach scalar : [false, true]
    if name == 'codecs' and not scalar
      continue
    endif
    test_name = 'test-' + name + (scalar ? '-scalar' : '')
    test_programs += {test_name: executable(
      test_name, ['test-' + name + '.c'] + sources,
      include_directories: gmime_inc,
      dependencies: public_deps,
      c_args: ['-DG_LOG_DOMAIN="gmime-tests"'] + tests_private_args + (scalar ? ['-DGMIME_DISABLE_SIMD'] : []),
    )}
  endforeach
endforeach

foreach name, program : test_programs
  data_name = name.substring(5)
  if name == 'test-openpgp'
    data_name = 'pgp'
  endif
  if name in ['test-streams', 'test-cat']
    data_dir = 'data' / data_name
  else
    data_dir = meson.current_source_dir() / 'data' / data_name
  endif
  # The mbox test still uses a shared tmp directory for its scratch files.
  serial = name == 'test-mbox'
  test(
    name, program,
    args: ['-v', data_dir],
    workdir: meson.current_build_dir(),
    is_parallel: not serial,
    timeout: 180,
    suite: (have_crypto and name in crypto_tests + ['test-autocrypt', 'test-openpgp']) ? 'crypto' : 'core',
  )
endforeach

manual_programs = ['test-best', 'test-parser', 'test-html']
if have_crypto
  manual_programs += ['test-pkcs7', 'test-smime']
endif
foreach name : manual_programs
  executable(
    name, name + '.c',
    include_directories: gmime_inc,
    dependencies: gmime_dep,
    link_with: testsuite_lib,
    c_args: ['-DG_LOG_DOMAIN="gmime-tests"'] + tests_private_args,
  )
endforeach
