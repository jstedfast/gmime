testsuite_lib = static_library(
  'testsuite', 'testsuite.c',
  include_directories: gmime_inc,
  dependencies: gmime_dep,
  c_args: ['-DG_LOG_DOMAIN="gmime-tests"'],
)

normal_tests = [
  'test-stream-block', 'test-substreams',
  'test-core', 'test-visitor', 'test-iterator', 'test-attachments', 'test-body-builder', 'test-addresses', 'test-params', 'test-iconv', 'test-streams', 'test-cat',
  'test-encodings', 'test-codecs', 'test-rfc2231', 'test-filters', 'test-best-encoding', 'test-headers',
  'test-mime-part', 'test-partial', 'test-mbox', 'test-autocrypt', 'test-mime', 'test-types',
]
crypto_tests = ['test-pgp', 'test-pgpmime']
if have_crypto
  normal_tests += crypto_tests
endif

test_programs = {}
foreach name : normal_tests
  sources = [name + '.c']
  if name == 'test-core'
    sources += ['../gmime/gmime-events.c']
  elif name == 'test-rfc2231'
    sources += ['../gmime/gmime-rfc2231-encoder.c', '../gmime/gmime-rfc2231-decoder.c']
  endif
  test_programs += {name: executable(
    name, sources,
    include_directories: gmime_inc,
    dependencies: gmime_dep,
    link_with: testsuite_lib,
    c_args: ['-DG_LOG_DOMAIN="gmime-tests"'],
  )}
endforeach

standalone_sources = {
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
  'test-charsets', ['test-charsets.c', '../gmime/gmime-charset.c', '../gmime/gmime-iconv.c'],
  include_directories: gmime_inc,
  dependencies: public_deps + [gthread_dep, iconv_dep],
  c_args: ['-DG_LOG_DOMAIN="gmime-tests"'],
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
      c_args: ['-DG_LOG_DOMAIN="gmime-tests"'] + (scalar ? ['-DGMIME_DISABLE_SIMD'] : []),
    )}
  endforeach
endforeach

foreach name, program : test_programs
  data_name = name.substring(5)
  if name in ['test-streams', 'test-cat']
    data_dir = 'data' / data_name
  else
    data_dir = meson.current_source_dir() / 'data' / data_name
  endif
  # These programs reset a shared tmp directory for their scratch files or keyring.
  serial = name in crypto_tests + ['test-mbox', 'test-autocrypt']
  test(
    name, program,
    args: ['-v', data_dir],
    workdir: meson.current_build_dir(),
    is_parallel: not serial,
    timeout: 180,
    suite: (have_crypto and name in crypto_tests + ['test-autocrypt']) ? 'crypto' : 'core',
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
    c_args: ['-DG_LOG_DOMAIN="gmime-tests"'],
  )
endforeach
