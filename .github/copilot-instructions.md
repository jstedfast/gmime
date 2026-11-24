# Working on GMime 4

## Build and test

Use an out-of-tree Meson build. The baseline is Meson 1.3+, Ninja, pkg-config,
a C compiler, GLib 2.70.2+, zlib, and iconv. On macOS, pass
`-Diconv_prefix="$(brew --prefix libiconv)"` at setup to select GNU libiconv.

```sh
meson setup _build
meson compile -C _build
meson test -C _build --print-errorlogs
meson test -C _build --print-errorlogs test-reader   # one registered test
meson test -C _build --list
```

Prefer the Meson test name over invoking a binary directly: `tests/meson.build`
supplies fixture paths, working directories, and any serialization requirements.
For fuzz regressions, configure a separate build with `-Dfuzzing=replay`, then
run `meson test -C _build-replay --suite fuzzing --print-errorlogs`. Individual
replay tests such as `fuzz-replay-html-tokenizer` work the same way. See
`fuzzing/README` for corpus generation and libFuzzer runs.

For static analysis, build first so Meson's compilation database and generated
headers exist. Run Cppcheck from the repository root:

```sh
cppcheck --project=_build/compile_commands.json \
  --file-filter='*/gmime/gmime*.c' --file-filter='*/util/*.c' \
  --library=gtk --library=posix --enable=warning,performance,portability
```

`README.md` gives the exact CI analysis configuration and the release-build
benchmark commands. For API documentation, configure with `-Dgtk_doc=true`
and run `meson compile -C _build gmime-doc`.

## Where behavior lives

- **MIME input:** `GMimeReader` scans a `GMimeStream` and emits callbacks for
  headers, boundaries, and content. `GMimeParser` uses the private
  `GMimeParserReader` subclass to turn those events into messages, multiparts,
  and leaf entities. Fix byte scanning and diagnostic positions in the reader;
  fix how events populate the object tree in the parser-reader. Exercise both
  `test-reader` (callback traces) and `test-parser-model` when a change crosses
  that boundary.
- **Storage and output:** `GMimeContent` can retain source-backed content
  instead of copying a body into memory. Stored bytes, transfer-decoded bytes,
  and display text are different representations. Follow the stream and writer
  paths when changing serialization; `test-roundtrip` checks wire preservation.
  Raw header getters return owned `GBytes`, not NUL-terminated strings. Use
  their explicit lengths and keep the reference alive while using the data.
- **Optional backends:** GPGME provides OpenPGP and S/MIME operations.
  DKIM/ARC use a separate GnuTLS or OpenSSL provider. Keep MIME parsing and
  backend-independent features usable without either. Compliance reports
  describe what the parser encountered; they are distinct from `GError`
  failures and from legacy warning callbacks.

## Repository conventions

- For MimeKit ports, use the upstream behavior **and its test vectors** as the
  comparison point. `STATUS.md` records parity decisions and deliberate
  exclusions; `PORTING.md` describes GMime 3-to-4 contracts for callers.
- Fixtures under `tests/data` are byte-sensitive. Preserve line endings,
  folding, embedded NULs, and offsets. For streaming changes, compare buffered
  and fragmented reads, not just parsed output. SIMD scanners and codecs have
  scalar reference paths; preserve agreement between them.
- Public APIs need gtk-doc ownership, nullability, and buffer-length annotations
  for GIR/Vala consumers. When adding a public header, check `gmime/gmime.h`,
  the installed-header list in `gmime/meson.build`, and the symbol groups in
  `docs/reference/gmime-sections.txt`. Keep internal declarations in private
  headers. Document breaking API changes and replacements in `PORTING.md`.
- Follow `HACKING` and adjacent C code: tabs with 8-column stops, a space before
  function-call parentheses, and the opening brace of a function on its own
  line. Keep commit subjects and body lines within 72 columns.
