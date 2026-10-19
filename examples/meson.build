foreach name : ['basic-example', 'imap-example', 'uuencode', 'uudecode', 'msgcheck']
  executable(
    name, name + '.c',
    dependencies: gmime_dep,
    link_with: util_lib,
  )
endforeach
