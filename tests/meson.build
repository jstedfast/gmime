automated_tests = [
  {'name': 'cat'},
  {'name': 'encodings' },
  {'name': 'filters' },
  {'name': 'headers' },
  {'name': 'iconv' },
  {'name': 'mbox' },
  {'name': 'mime' },
  {'name': 'mime-part' },
  {'name': 'partial' },
  {'name': 'streams' },
]

manual_tests = [
  {'name': 'best'},
  {'name': 'parser'},
  {'name': 'html'}
]

test_deps = deps

if gpgme.found()
  test_deps += dependency('gpg-error')
  automated_tests += [
    {'name': 'pgp', 'parallel': false}, 
    {'name': 'pgpmime', 'parallel': false},
    {'name': 'autocrypt', 'parallel': false}
  ]
  manual_tests += [
    {'name': 'pkcs7', 'parallel': false},
    {'name': 'smime', 'parallel': false}
  ]
endif

# automated_tests += manual_tests

gmime_include = include_directories('..', '../gmime/', '../util')
fs = import('fs')

test_dir = meson.project_source_root() / 'tests'

foreach at : automated_tests
  name = at.get('name')
  testdatadir = at.get('testdir', 'data' / name)
  parallel = at.get('parallel', true)
  args = at.get('args', ['-v'])

  exec = executable(name, 'test-@0@.c'.format(name), 'testsuite.c', 
    dependencies : test_deps, include_directories: gmime_include, 
    link_with: [libgmime, libutil])

  if fs.is_dir(testdatadir)
    args += testdatadir
  endif
  test(name, exec, args: args, is_parallel: parallel, workdir : test_dir)
endforeach
