# Porting applications to GMime 4.0

This guide covers migration from **GMime 3.2.15**, the latest released 3.x
version when this guide was written. GMime 4 is neither source nor ABI
compatible with 3.x. Rebuild applications, bindings, plugins, and subclasses;
do not mix headers or libraries from the two major versions.

Use this guide as an ordered migration checklist, whether making changes
manually or with an AI assistant. The installed public headers and reference
manual define exact signatures and ownership. [STATUS.md](STATUS.md) records
detailed implementation decisions, intentional differences, and historical
validation; it is not an application migration checklist.

## 1. Update the build and dependencies

GMime now uses Meson >= 1.3.0, a C11 compiler, and GLib/GObject/GIO >= 2.88.0.
Autotools support has been removed. zlib and iconv are required.

```sh
meson setup _build --buildtype=debugoptimized
meson compile -C _build
meson test -C _build --print-errorlogs
meson install -C _build
```

Optional features include GPGME-backed cryptography, libidn2, GObject
introspection, Vala bindings, and GTK-doc. See [README.md](README.md) for
dependencies and Meson options. Do not assume an optional feature was enabled:
use explicit `-Dcrypto=enabled`, for example, when it is a requirement.

| Integration | GMime 3.x | GMime 4.0 |
|---|---|---|
| pkg-config / Meson dependency | `gmime-3.0` | `gmime-4.0` |
| Include search directory | `gmime-3.0` | `gmime-4.0` |
| Shared library | `libgmime-3.0` | `libgmime-4.0` |
| GIR namespace/version | `GMime`, `3.0` | `GMime`, `4.0` |
| Vala package | `gmime-3.0` | `gmime-4.0` |

Continue to include `<gmime/gmime.h>`. Update dependency declarations, linker
flags, runtime library paths, introspection version requirements, and package
dependencies. For example:

```meson
gmime = dependency('gmime-4.0', version: '>=4.0.0')
executable('mail-app', 'mail-app.c', dependencies: gmime)
```

For Python/GObject introspection, request the new version before importing:

```python
import gi
gi.require_version("GMime", "4.0")
from gi.repository import GMime
```

## 2. Rename types and symbols

These are real symbol and registered GType renames, not compatibility aliases.
Update forward declarations, public application APIs, subclass parent types,
casts, type checks, signal/property types, binding names, and direct includes.

| 3.x type | 3.x function prefix | 4.0 type | 4.0 function prefix |
|---|---|---|---|
| `GMimeObject` | `g_mime_object_` | `GMimeEntity` | `g_mime_entity_` |
| `GMimeDataWrapper` | `g_mime_data_wrapper_` | `GMimeContent` | `g_mime_content_` |
| `InternetAddress` | `internet_address_` | `GMimeInternetAddress` | `g_mime_internet_address_` |
| `InternetAddressMailbox` | `internet_address_mailbox_` | `GMimeMailboxAddress` | `g_mime_mailbox_address_` |
| `InternetAddressGroup` | `internet_address_group_` | `GMimeGroupAddress` | `g_mime_group_address_` |
| `InternetAddressList` | `internet_address_list_` | `GMimeInternetAddressList` | `g_mime_internet_address_list_` |

Rename the corresponding `Class` types too. In macro names, replace `OBJECT`
with `ENTITY` and `DATA_WRAPPER` with `CONTENT`; for example,
`GMIME_TYPE_OBJECT` becomes `GMIME_TYPE_ENTITY`. Address macros are now:

| Type | Type macro | Cast / check |
|---|---|---|
| Base address | `GMIME_TYPE_INTERNET_ADDRESS` | `GMIME_INTERNET_ADDRESS` / `GMIME_IS_INTERNET_ADDRESS` |
| Mailbox | `GMIME_TYPE_MAILBOX_ADDRESS` | `GMIME_MAILBOX_ADDRESS` / `GMIME_IS_MAILBOX_ADDRESS` |
| Group | `GMIME_TYPE_GROUP_ADDRESS` | `GMIME_GROUP_ADDRESS` / `GMIME_IS_GROUP_ADDRESS` |
| Address list | `GMIME_TYPE_INTERNET_ADDRESS_LIST` | `GMIME_INTERNET_ADDRESS_LIST` / `GMIME_IS_INTERNET_ADDRESS_LIST` |

Direct include replacements:

| 3.x header | 4.0 header |
|---|---|
| `gmime/gmime-object.h` | `gmime/gmime-entity.h` |
| `gmime/gmime-data-wrapper.h` | `gmime/gmime-content.h` |
| `gmime/internet-address.h` | `gmime/gmime-internet-address.h` |
| `gmime/gmime-references.h` | `gmime/gmime-message-id-list.h` |

Additional replacements:

* `GMIME_CONTENT_ENCODING_DEFAULT` becomes
  `GMIME_CONTENT_ENCODING_UNSPECIFIED`; its numeric value remains zero.
* Multipart `get_prologue()` / `set_prologue()` become `get_preamble()` /
  `set_preamble()`.
* `GMimeReferences` is replaced by the GObject-based `GMimeMessageIdList`.
  Migrate its operations explicitly; it is not a boxed linked-list rename.
  Release lists with `g_object_unref()`, not the old references free function.
  A message's References accessor returns its persistent, mutable list:
  editing that list updates the header. Take a reference to retain it beyond
  the message's lifetime.

Do not perform an unchecked global substring replacement: distinguish a
GMime type macro from unrelated GObject or application identifiers.

### Instance state and subclassing

GObject instance state is now private. Use public getters, setters, and
collection methods rather than accessing fields or copying instance structs.
In particular, use `g_mime_stream_mem_get_byte_array()` for a memory stream's
buffer and `g_mime_stream_null_get_written()` /
`g_mime_stream_null_get_newlines()` for null-stream statistics.

MIME entities, streams, filters, addresses, content, readers, visitors, and
the crypto-context base remain derivable. Their public class structs expose
virtual methods for subclasses, not an alternative calling API: invoke
public functions to retain validation and dispatch. Reserved class slots
are private and must not be used by applications.

Data objects and utilities (headers, parameters, collections, certificates,
signatures, results, content types/dispositions, parsers, body builders,
anonymizers, iterators, and Received models) are opaque, final types.
The existing opaque codec, yEnc, and built-in crypto backend types are also
final. Use composition rather than deriving from these types.

Stream implementations can use `g_mime_stream_get_position()` /
`g_mime_stream_set_position()` for base-class position bookkeeping and
`g_mime_stream_get_bounds()` for bounds. These helpers do not invoke virtual
methods or seek the backend. Applications should continue to use
`g_mime_stream_tell()` and `g_mime_stream_seek()`.

Filter implementations should allocate output with `g_mime_filter_set_size()`
and obtain the writable buffer, capacity, and prespace using
`g_mime_filter_get_output_buffer()`. Track any output cursor in the subclass's
own private state. Rebuild all subclasses: parent instance and class layouts
have changed.

## 3. Preserve decoded-content behavior explicitly

**This is the most important behavioral change after renaming.**

| Operation | Meaning |
|---|---|
| 3.x `g_mime_data_wrapper_write_to_stream()` | Write decoded content |
| 4.0 `g_mime_content_write_to_stream()` | Copy original, possibly transfer-encoded bytes |
| 4.0 `g_mime_content_decode_to_stream()` | Write decoded content |

When porting attachment extraction, hashing, indexing, or text conversion,
replace the old wrapper write with **decode**, not merely the renamed write:

```c
GMimeContent *content = g_mime_part_get_content (part); /* borrowed */

if (content == NULL) {
    /* Handle missing content according to the application's policy. */
} else if (g_mime_content_decode_to_stream (content, output) < 0) {
    /* Handle the native stream error (errno); do not consume partial output. */
}
```

Both stream-writing functions return `ssize_t`; they do not take a `GError **`.
The part's content getter is borrowed. Content and stream constructors return
owned references, and assigning content to a part retains a reference.

`g_mime_content_open()` returns an owned decoded stream. Content may be
memory-backed or source-backed. Retain the source for as long as its content
views need it; do not read multiple views of a shared seekable source
concurrently without synchronization. Rewinding/reading content can reposition
that source. Selecting memory storage avoids that shared-source dependency,
at the cost of retaining content in memory.

For content subclasses, move decoded-output overrides to `decode_to_stream`.
The `write_to_stream` virtual now supplies raw bytes to the default decoder.
Revisit subclass chaining and test both raw and decoded output.

## 4. Handle parser errors and options

`g_mime_parser_construct_part()` and `g_mime_parser_construct_message()` now
take a final `GError **`. Do not treat every NULL result as clean EOF:

```c
GError *error = NULL;
GMimeMessage *message = g_mime_parser_construct_message (parser, options, &error);

if (message != NULL) {
    /* Process the message, then g_object_unref (message). */
} else if (error != NULL) {
    /* Report the failure; release error with g_clear_error (&error). */
} else {
    /* Clean end of input. */
}
```

After a parser failure, reinitialize it with a stream before another read.
Convenience APIs such as `g_mime_message_load()` and
`g_mime_message_load_from_file()` also accept `GError **`. Native
`write_to_stream()` APIs still use their documented return value and errno;
do not mechanically add error arguments to all stream APIs.

Move Content-Length handling from the parser to
`g_mime_parser_options_set_respect_content_length()`. The default is FALSE.
Set it explicitly if mailbox framing depends on it.

The fallback charset array is replaced by a single charset:
`g_mime_parser_options_set_charset_encoding(options, charset, error)` returns
`gboolean`. Its getter is borrowed; NULL in the setter resets to UTF-8.
Undeclared text is tried as UTF-8, then the configured charset, then Latin-1.
Check setter failure rather than silently proceeding with another charset.

Review these defaults against application/security requirements:

| Option | 4.0 default |
|---|---|
| Maximum raw length of one header | 16 KiB |
| Maximum MIME nesting depth | 1024 |
| Maximum address-group nesting depth | 3 |
| Addresses without a domain | Allowed |
| Unquoted commas in address display names | Allowed |
| Respect Content-Length | FALSE |

Use `GMimeParserOptions` setters to choose application-specific limits and
compliance modes. Best-effort recovery is not input validation. Checked
parsing/assignment APIs accepting `GError **` are available where rejection
is required; consult each API rather than assuming legacy parse calls became
strict. Compliance logging and the streaming `GMimeReader` API are optional
new facilities, not prerequisites for migrating an ordinary parser.

To collect structured diagnostics while building a message, use
`g_mime_parser_set_compliance_logger(parser, callback, user_data, destroy)`.
The callback receives a borrowed `GMimeComplianceIssue`; copy it to retain it.
Configure checks and per-violation budgets through the borrowed mutable
`g_mime_parser_get_compliance_options()` result, or pass settings to
`g_mime_parser_set_compliance_options()` to copy them (NULL restores defaults).
These settings and the logger survive input replacement and may be assigned
before input. Callback changes take effect at the next construction operation;
old logger data stays alive until its active operation finishes.
Passing NULL for the callback, data and destroy notifier disables reporting.
Compliance reporting neither changes parsing policy nor replaces legacy
warning callbacks or `GError` handling.

## 5. Treat raw headers as bytes, not C strings

`g_mime_header_get_raw_name()` and `g_mime_header_get_raw_value()` now return
**owned `GBytes` references**. Release them with `g_bytes_unref()`.
Their contents may contain NUL or invalid UTF-8 and are not NUL-terminated.

```c
GBytes *raw = g_mime_header_get_raw_value (header);
gsize length;
const guint8 *data = g_bytes_get_data (raw, &length);

/* Use data and length together, e.g. fwrite (data, 1, length, file). */
g_bytes_unref (raw);
```

The pointer is valid while its `GBytes` reference is retained. Do not pass it
to `strlen()`, `%s`, `g_strdup()`, or string comparison functions.
Decoded name/value getters remain borrowed strings. Duplicate a decoded value
when it must survive a header mutation.

`g_mime_header_set_raw_value(header, bytes, error)` now returns `gboolean`
and validates the assignment. Supply the complete raw value, including its
required line ending, and check errors. Invalid assignment preserves the
previous state. Use `g_mime_header_set_value_full()` for checked decoded-value
assignment; do not construct raw headers by concatenating untrusted input.

## 6. Update preparation, digest, and date calls

The following APIs now propagate failures rather than hiding partial reads:

| API | 4.0 change |
|---|---|
| `g_mime_part_get_best_content_encoding()` | Adds final `GError **` |
| `g_mime_part_prepare()` | Returns `gboolean`, adds final `GError **` |
| `g_mime_part_verify_content_md5()` | Adds final `GError **` |

The encoding/preparation APIs also have `_with_max_line_length()` variants.
Replace 3.x `g_mime_object_encode()` calls with
`g_mime_entity_prepare(entity, constraint, error)` and check its return value.
The intermediate 4.0 `g_mime_entity_encode()` API and the entity class's
`encode` virtual method have been removed. Subclasses should implement
`prepare_with_max_line_length` instead.
Do not serialize after failed preparation without handling the failure.
For MD5 verification, FALSE without an error means no usable digest or a
mismatch; FALSE with an error means content could not be read.

`g_mime_part_set_content_md5(part, NULL)` now **removes** Content-MD5; it
does not compute a digest. To create one:

```c
char *digest = g_mime_part_compute_content_md5 (part, &error);

if (digest != NULL) {
    g_mime_part_set_content_md5 (part, digest);
    g_free (digest);
}
```

Computing does not modify headers. It reads decoded content, with canonical
CRLF normalization for text, and can reposition the source. Content-MD5 is
an interoperability feature, not a modern security primitive.

Certificate and signature creation/expiration accessors now use `GDateTime *`
instead of `time_t`. Getters return borrowed, nullable values; setters retain
a reference. NULL means unknown. The UTC date
`9999-12-31 23:59:59.999999` means never expires. The Unix epoch is an ordinary
date, not a sentinel. Removed `get_created64()` / `get_expires64()` calls
should use the corresponding date getter and `g_date_time_to_unix()`, after
checking for NULL and any never-expires policy.

Message dates already used `GDateTime` in 3.x; do not change them unnecessarily.

## 7. Review formatting, codecs, and subclasses

Default formatting options are immutable. Allocate options with
`g_mime_format_options_new()` or clone them before changing settings.
The default maximum line length is 78; valid configured lengths are 60..998.
Choose LF/CRLF and international UTF-8 output explicitly when transport
requirements matter.

Newly generated or edited headers can have different charset selection,
encoded-word grouping, folding, and whitespace normalization. Tests should
distinguish untouched raw-byte preservation from semantic round trips and
intentional reformatting. Do not normalize signed bytes before verification.
Oversized words may require encoded output even with international formatting
to avoid inserting whitespace into the decoded value.

The old yEnc filter/step API is removed. Use `GMimeYencEncoder` /
`GMimeYencDecoder`, directly or through `GMimeFilterBasic`, and review the new
encoder/decoder state and buffer contracts rather than translating internal
state fields. Their checksum getters return the raw running CRC; complement
it for the finalized CRC returned by the old filter. Existing encoder
constructors retain LF output; use the newline-format constructor for CRLF.

For complete blocks, use `GMimeYencReader` (`read_next`, then `decode_to`) and
`GMimeYencWriter` (`write`) with `GMimeYencHeader` and `GMimeYencOptions`.
These handle `=ybegin`, optional `=ypart`, `=yend`, multipart ranges, size
checks and trailer CRC validation. Their checksums are **finalized**, unlike
the payload codecs. The writer defaults to CRLF; filenames default to
ISO-8859-1 on the wire and are UTF-8 in the public API. Options are copied at
construction. Streams are referenced, not explicitly closed.

A block is not valid until decoding completes successfully; discard output
on error. Missing trailer checksums are accepted. A whole-file CRC advertised
for a partial range remains unverified metadata; these APIs do not assemble
multipart files or undo NNTP dot-stuffing. There is no legacy arbitrary-state
setter or multipart aggregate CRC seed API.

Other new codec/validator types need not replace application code that still
uses supported high-level filters.

Recheck every application subclass against the current class struct:
parent names, changed content virtuals, return/error contracts, and ownership
all matter. Multipart collection index violations now produce programming
error diagnostics; boundaries must satisfy MIME syntax and the 70-character
limit. Parser factories may create specialized multipart/report subclasses.
Prefer `GMIME_IS_*` inheritance checks to exact `G_OBJECT_TYPE()` comparisons
unless an exact concrete type is truly required.

Cryptography remains backed by GPGME/GnuPG. This migration does not introduce
the managed cryptography backend or DKIM/ARC signing and verification.
Verification, decryption, and encryption reject undefined flag bits with
`GMIME_ERROR_NOT_SUPPORTED` before invoking the backend. Pass only flags
defined for that operation; do not rely on unknown bits being ignored.

GPGME-aware C applications can borrow the backend context through
`g_mime_gpg_context_get_gpgme_ctx()` or
`g_mime_pkcs7_context_get_gpgme_ctx()`. Certificates returned by GPGME-backed
verification or decryption retain their backend key when available;
`g_mime_certificate_get_gpgme_key()` exposes it for inspecting all user IDs,
per-identity validity, and subkeys. These accessors return `gpointer`, require
no GPGME types in GMime's public headers, and are excluded from introspection.
Do not release borrowed contexts or keys. A key remains valid for the
certificate's lifetime, independently of the context; use `gpgme_key_ref()`
to keep it longer. Accessors return `NULL` when the backend handle is
unavailable. Existing certificate getters keep their single-identity behavior.

The new body builder, attachment collection, visitor, and report helpers are
optional conveniences; a working explicit MIME tree need not be rewritten
to use them.

## 8. Validate the migrated application

1. Build with warnings enabled and verify all binaries/bindings load GMime 4.
2. Search for old type names, function prefixes, macros, headers, and package
   names. Manually review every former data-wrapper write.
3. Exercise plain, HTML, multipart, embedded-message, attachment, and mailbox
   inputs, including malformed/truncated data, short reads, and I/O failures.
4. Compare extracted **decoded bytes**, not only attachment sizes or names.
   Verify both raw and decoded paths for custom content implementations.
5. Test EOF separately from parser failure, limit violations, invalid charset
   selection, failed preparation, and output errors.
6. Check Unicode addresses, IDNs, legacy charsets, raw/binary headers, LF/CRLF
   output, and preservation of untouched and signed messages.
7. Test object lifetimes, retained `GBytes`/list/date references, source-backed
   content, and subclass virtual dispatch under the application's memory and
   concurrency checks.

For AI-assisted migrations, make API renames and behavioral changes in
separate reviewable steps. Do not silence new diagnostics, discard `GError`,
regenerate expected output from the new implementation without independent
checks, or assume successful compilation proves decoded-content parity.
