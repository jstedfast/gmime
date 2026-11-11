# Porting applications to GMime 4.0

This guide covers migration from **GMime 3.2.15**, the latest released 3.x
version when this guide was written. GMime 4 is neither source nor ABI
compatible with 3.x. Rebuild applications, bindings, plugins, and subclasses;
do not mix headers or libraries from the two major versions.

Use this guide as an ordered migration checklist, whether making changes
manually or with an AI assistant. The installed public headers and reference
manual define exact signatures and ownership. See
[GMime 4 features](README.md#gmime-4-features) for an overview.
Historical implementation notes and test results are not current compatibility
or CI guarantees.

## 1. Update the build and dependencies

GMime now uses Meson >= 1.3.0, a C11 compiler, and GLib/GObject/GIO >= 2.70.2.
Autotools support has been removed. zlib and iconv are required.
On macOS, GNU libiconv >= 1.12 is required instead of Apple's system iconv;
see the macOS build instructions in [README.md](README.md).

`g_mime_iconv()` is now a library function, not a macro calling the application's
iconv backend. Use only GMime's conversion/close wrappers with descriptors from
`g_mime_iconv_open()`, never GLib's or system iconv functions.

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
| `gmime/gmime-part-iter.h` | `gmime/gmime-iterator.h` |

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

### Address construction, equality, and ownership

`g_mime_mailbox_address_new()` now takes a trailing `GError **`.
`g_mime_mailbox_address_set_addrspec()`, `g_mime_message_add_mailbox()`, and
`g_mime_autocrypt_header_set_address_from_string()` return `gboolean` and
take `GError **`; invalid addresses leave existing state unchanged.
Empty mailbox strings remain valid model values.

Mailbox `get_local_part()`, `get_address(idn_encode)`, and
`get_domain(idn_encode)` return allocated strings. `get_addrspec()`,
`get_raw_domain()`, and `get_route()` return borrowed values.
Replace the former cached IDN getter with `get_address(TRUE)` and free its
result. Stored domains retain their original Unicode/A-label spelling.
Routes are mutable `GMimeDomainList` objects; edits update owning headers.

Address-list lookup/removal now uses value equality rather than identity.
Equality is case-sensitive for names and stored addresses, distinguishes NULL
from empty names, and compares groups recursively; charsets/routes are ignored.
Ordering is a separate operation: names use UTF-8 byte order and mailbox
domains use IDN-encoded, ASCII-lowercase spelling. Do not treat comparison
equality as value equality. `get_mailboxes()` returns an owned snapshot
retaining the original mailboxes, not clones; edits still affect their owners.

New typed address parsers and `g_mime_internet_address_list_parse_full()`
accept explicit byte lengths and `GError **`, reject embedded NULs, and require
complete values. Legacy list/message-header parsing remains best-effort.
`GMIME_RFC_COMPLIANCE_LOOSER` additionally accepts consecutive/trailing
local-part dots and escaped at-signs; it is not an SMTP validation policy.

### MIME tree iteration

`GMimePartIter` has been removed. Use the GObject-based `GMimeIterator`:
`g_mime_iterator_new(message)` traverses a message's live body;
`g_mime_iterator_new_for_entity(entity)` traverses an entity subtree, including
its root. Passing a message to the latter is equivalent to `new(message)`.
Release iterators with `g_object_unref()`.

New and reset iterators are positioned **before** the first entity:

```c
GMimeIterator *iter = g_mime_iterator_new (message);

while (g_mime_iterator_move_next (iter)) {
    GMimeEntity *entity = g_mime_iterator_get_current (iter);
    /* Process entity. */
}

g_object_unref (iter);
```

Replace `next()` with `move_next()`, `jump_to()` with `move_to(path, error)`,
and `get_path()` with `get_path_specifier()`. Current/parent access and
`is_valid()` remain available under the new prefix. Accessors for the current
position require a successful move. `get_message()` returns the original
message; there is no `get_toplevel()` replacement for entity roots, so retain
the supplied root separately if needed.

Paths are not universally interchangeable with the old iterator's paths.
A non-multipart root is `1`; a multipart root is `0`, with children numbered
`1`, `2`, etc. Multipart bodies inside attached messages are visited explicitly
(for example, `3.0`), with children at `3.1`, `3.2`, etc. For entity-root
iterators, paths and depths are relative to the supplied root. The root has
no parent. The containing message is not inferred: `get_message()` is NULL,
and `get_current_message()` is NULL outside encapsulated messages.

Malformed paths set `GMIME_ERROR_PARSE_ERROR` and preserve the cursor;
well-formed missing paths exhaust it without an error. Exhaustion persists
until reset or navigation. `skip_children()` prunes the current subtree.

There are no backward-navigation, replace/remove, or clone methods.
`g_object_ref()` shares the cursor, not an independent traversal position.
Use a separate iterator for an independent cursor. For tree edits, collect
referenced entities and their owners, then use message/multipart APIs after
traversal. The iterator retains its active ancestry, but external edits do not
repair stored indexes; reset or seek before relying on them.

### SMTP envelope address validation

`g_mime_smtp_address_try_validate(address, length, options, &result)` checks a
bare RFC 5321 mailbox; `g_mime_smtp_utf8_address_try_validate()` adds
Unicode/NFC/context/script validation. These are not MIME header parsers or the
reader's compliance validator. They do not trim, normalize, rewrite or accept
display names. SMTPUTF8 policy is stricter than transport syntax and does not
guarantee protection against every spoof.

Pass a byte length or `-1` for a NUL-terminated string. Invalid UTF-8 and embedded
NULs are syntax errors. The boolean result and optional `GMimeSmtpAddressError`
or `GMimeSmtpUtf8AddressError` output describe validity; these are not `GError`
domains. Pass NULL for the output when only a boolean is needed.

Length limits are opt-in. Local-parts and complete addresses use their original
UTF-8 byte lengths, including quoting; domain limits use raw Punycode A-label
lengths. SMTP's ISO-2022-JP option only extends quoted local-parts. SMTPUTF8's
`DECODE_IDN` permits canonical domain A-labels, not local-part A-label atoms;
`ASCII_IS_LATIN` additionally rejects Latin/non-Latin mixtures within a
local-part or domain label. Unicode 18.0.0 validation tables are bundled, so
results do not depend on the installed GLib/IDNA implementation.

The corresponding `*_try_validate_mailbox()` functions validate
`g_mime_mailbox_address_get_addrspec()` as stored, ignoring display names and
routes. Use an explicitly IDN-encoded address from
`g_mime_mailbox_address_get_address(mailbox, TRUE)` if that is what will be
sent via non-international SMTP; validation itself does not perform that
conversion.

### Authentication-Results models

`GMimeAuthenticationResults`, `GMimeAuthenticationMethodResult` and
`GMimeAuthenticationMethodProperty` are final, reference-counted GObjects.
They model both Authentication-Results and ARC-Authentication-Results; no
cryptography backend is needed. They describe claims made by a header, not
verified authentication or a trusted authentication service.

`g_mime_authentication_results_parse(text, length, &error)` accepts an explicit
byte range or `-1` for a C string. Malformed input returns NULL with
`GMIME_ERROR_PARSE_ERROR` and a relative byte offset. Embedded NULs are rejected;
invalid UTF-8 in value/comment text uses GLib replacement characters. Token
names remain ASCII. Office365's missing or method-specific service IDs,
dotted methods, empty properties and unquoted property tspecials are retained.

Nullable instance/version values use boolean getters with optional integer
outputs, setters, and explicit clear methods; absent outputs are zero.
Setters do not range-check numbers, although parsing only accepts
nonnegative Int32 values. Result/property collections retain references,
permit duplicates, use identity comparisons and preserve insertion order.
Returned child objects and strings are borrowed.

Constructors and text setters copy UTF-8 strings without syntax validation.
Comments are inner comment syntax, not automatically escaped plain text.
Parsed properties retain quoting, except empty values are always quoted.
`*_to_string()` returns allocated model text without a field name or newline.
Both header names use a structured formatter, falling back to the default
formatter for unparseable values; existing raw headers remain unchanged.
Folding counts UTF-8 octets and does not split indivisible quoted strings,
comments or tokens.

### Instance state and subclassing

GObject instance state is now private. Use public getters, setters, and
collection methods rather than accessing fields or copying instance structs.
In particular, use `g_mime_stream_mem_get_byte_array()` for a memory stream's
buffer and `g_mime_stream_null_get_written()` /
`g_mime_stream_null_get_newlines()` for null-stream statistics.

MIME entities, streams, filters, addresses, content, readers, and visitors
remain derivable. Their public class structs expose
virtual methods for subclasses, not an alternative calling API: invoke
public functions to retain validation and dispatch. Reserved class slots
are private and must not be used by applications.

Data objects and utilities (headers, parameters, collections,
content types/dispositions, parsers, body builders,
anonymizers, iterators, and Received models) are opaque, final types.
The existing opaque codec and yEnc types are also
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

### Stream ownership and explicit close

Streams retain reference-counted lifetime rules.
`g_mime_stream_close()` does not unref the wrapper, and an extra
reference does not prevent another caller from explicitly closing a shared
resource. Flush and close explicitly, checking return values and `errno`;
finalization cannot report delayed I/O failures. A failed close may already
have released its backend, so do not assume the resource is available to retry.

| Wrapper | Explicit close | Finalization |
|---|---|---|
| `GMimeStreamBuffer` | Discards unflushed output and releases its source reference | Same |
| `GMimeStreamCat` | Releases its child references without flushing or explicitly closing them | Same |
| `GMimeStreamFilter` | Completes writes; closes its source only if owner is true | Releases its source reference without completing writes or explicitly closing it |

Releasing the final reference can still finalize a source and release its
resources. Filter ownership defaults to false. Flushing a filter completes its
output but does not flush its source; flush downstream buffering separately.
Retained parents keep borrowed Fs/File/Mmap/GIO substreams alive under normal
unref, but do not explicitly close an owning parent while its views are needed.
Memory and block substreams retain their shared storage independently.

Both `GMimeStreamGIO` constructors consume the supplied `GFile` reference,
as before; their introspection annotations now explicitly say `transfer full`.
C callers retaining the file must pass `g_object_ref(file)`. The owner flag
controls releasing that reference, not closing the wrapper's internal GIO
streams, which are always closed and released. Changing owner to false transfers
responsibility for the existing reference back to the caller; it does not
change reference counts. Language bindings should retain default ownership.
GIO close now returns -1 and sets `errno` on failure, preserving the first error
while closing and releasing both internal streams. The wrapper is closed even
on failure, and a subsequent close does not retry its released resources.

### Stream bounds and storage

Use `g_mime_stream_substream()` for bounded views. Negative starts, reversed ranges, and
ranges outside the parent fail with `EINVAL`. An end of -1 inherits a finite
parent's end. Bounds are absolute backend coordinates, not offsets from the
parent's current position. Consult the backend's seek contract rather than
assuming every stream uses identical coordinates or supports seeking.

Memory and block views share bytes but have independent positions and bounds.
Closing a parent memory view or replacing its byte array does not invalidate
existing views. A finite declared length can exceed available storage; reads
still stop at actual storage. `GMimeStreamMem` can grow on seeks and zero-fills
newly exposed gaps; storage is limited to `G_MAXUINT` bytes.
`GMimeStreamBlock` uses non-contiguous blocks, rejects seeks beyond its current
extent, and supports explicit resizing. Resizing affects shared storage, not
other views' positions; contiguous copies are independent.

`GMimeStreamNull` measures the highest written position, not cumulative bytes.
Its written/newline statistics remain cumulative, including overwrites.
Nonempty reads now fail with `EBADF`; reset clears measurement and statistics.
`GMimeStreamCat` reads across children at their current positions without
requiring seeking, but explicit seek/reset operations require support from
the affected children. Partial progress and subsequent I/O errors must still
be handled.

Fs/File/Mmap views must not outlive an explicit close of their owning backend.
Never truncate a file while its mapping is active. Nonempty mapped operations
require the corresponding read/write protection. Stream limits depend on the
backend and platform; check `EOVERFLOW` rather than assuming every stream can
address every 64-bit offset or accept arbitrarily large counts.

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

`g_mime_part_set_content(part, NULL)` clears content without removing headers;
the part then serializes an empty body. Assignments retain content references.
Use `g_mime_content_type_set_charset_encoding()` for checked charset resolution;
the older `get_charset()` / `set_charset()` operate on the raw parameter.
`get_charset_encoding()` returns an owned resolved name, or NULL without error
for an absent or unsupported name. A failed checked assignment preserves the
previous parameter, and NULL removes it.

### Decoded message bodies

Use `g_mime_message_get_text_body(message, error)` for plain text,
`g_mime_message_get_html_body(message, error)` for HTML, or
`g_mime_message_get_text_body_with_format(message, format, error)` for another
`GMimeTextFormat`. Each returns owned UTF-8 text; release it with `g_free()`.
NULL without an error means no matching body; an allocated empty string means
a selected empty body. I/O and conversion errors never return partial text.

Selection follows the multipart classes, including alternative preference
and related roots, rather than scanning attachments or embedded messages.
A standalone attachment is excluded. Transfer decoding precedes charset
conversion: a supported declared charset wins, otherwise a UTF-8/UTF-16 BOM
or UTF-8 is used. Declared charsets and UTF-16 BOMs replace malformed sequences
(U+FFFD for Unicode, `?` otherwise); strict default/BOM UTF-8 retries the whole
body as Latin-1 on malformed input. Charset mappings still follow native iconv.
Incomplete final characters are processed rather than silently dropped.
Decoded NULs are errors. Newlines use the default format options.
HTML is not sanitized. Existing `g_mime_message_get_body()` still returns a
borrowed entity, and the legacy `g_mime_text_part_get_text()` is unchanged.

### Text encoding detection and reporting

`g_mime_text_part_try_detect_encoding(part, &charset, &confidence, error)`
returns an owned charset name and `GMimeTextEncodingConfidence`. A supported
Content-Type charset or decoded UTF-8/UTF-16 BOM gives `CERTAIN`; absent
content gives `"us-ascii"` and `IRRELEVANT`. Empty content is not absent.
No match returns FALSE, NULL and `UNDEFINED`, without an error. Detection
does not validate the whole body or guess UTF-8. `TENTATIVE` is reserved.

Without a usable declaration, detection opens a decoded reader and makes
one read of up to three bytes; short reads are not retried. The backing
stream may be read ahead and is not closed or rewound afterward. Unlike
checked decoding, this uses the backing stream, not content copy overrides.

Use `g_mime_text_part_get_text_with_encoding(part, &used_charset, error)` for
checked automatic decoding and the charset actually used, including Latin-1
after fallback. It shares the message-body decoding policy above, honors
content decoding overrides, and does not change headers or the legacy getter.
It uses buffered bytes for BOM detection, including across short source reads.
The charset output is optional and is NULL on failure. Free both returned
strings with `g_free()`. The explicit `get_text_with_charset()` remains strict.

### Body selection, related resources, and attachments

Multipart selection follows the concrete class, not merely its current
Content-Type string. `GMimeMultipartAlternative` searches alternatives in
reverse order; `GMimeMultipartRelated` considers only its selected root.
Constructing a generic multipart with subtype `"alternative"` or `"related"`
does not give it the specialized class's selection behavior. The parser and
entity factory select specialized classes automatically.

Related-resource lookup searches immediate children and never fetches external
URLs or files. CID paths match Content-ID case-sensitively; URI comparisons
use GLib's normalization. Relative locations
resolve against the child's Content-Base or the container's Content-Base,
not the container's Content-Location. `g_mime_multipart_related_open()` returns
an owned decoded stream and optional allocated MIME-type/charset strings.
Its source-sharing restrictions are the same as `g_mime_content_open()`.

Content-Base and Content-Location are nullable entity-level UTF-8 strings.
Content-Base must be absolute; Content-Location may be relative. Setters
validate syntax but retain the supplied spelling rather than canonicalizing
it. Getters do not resolve relative values.

Attachment status is determined by Content-Disposition, not by a filename.
`g_mime_message_get_body_parts()` and `get_attachments()` return owned
`GPtrArray` snapshots of retained entities; release them with
`g_ptr_array_unref()`. Membership is snapshotted, but entities are shared.
Traversal excludes multipart containers and does not enter attached messages.
A multipart marked as an attachment still contributes its individual leaves,
not the container itself.

`GMimeBodyBuilder` combines plain and HTML bodies as multipart/alternative,
wraps HTML with linked resources as multipart/related, and adds attachments
using multipart/mixed. NULL text means absent; empty text is a present body.
Resources are ignored without HTML. With no text, one attachment is returned
directly; an otherwise empty builder returns an empty text/plain part.
Builds create new text parts/containers but retain attachment/resource objects.
The returned body always carries an owned reference.

Attachment collection stream inputs are copied from their current position
without being closed; filename inputs are opened and closed internally.
Byte inputs are copied, not shared. An explicit Content-Type is retained for
ordinary parts. Only auto-detected seekable message inputs recover from a
format error as application/octet-stream; explicit types and I/O failures do
not silently fall back. Linked filenames must be valid relative URI references,
so unescaped spaces and backslashes are not accepted.

The `g_mime_types_*()` filename/type helpers use bundled extension mappings,
not content sniffing or the host MIME database. Unknown extensions resolve
to application/octet-stream. Registrations are process-wide; returned strings
are owned. This filename registry is separate from the global entity-subclass
registry used by the parser/factory.

### Structured reports and traversal

Delivery-status, disposition-notification, and feedback-report accessors lazily
return borrowed cached header collections. Reading them preserves content;
editing them regenerates it. Their caches survive inherited content replacement,
so editing an old cached collection can replace newly assigned content.
Retain a collection explicitly if it must outlive its report, and check loading
errors. `GMimeTextRfc822Headers` can retain an embedded body: it is not a
body-stripping API.

`GMimeVisitor` traverses live MIME trees without decoding content or performing
cryptography. Overriding a typed visit without chaining prunes that subtree.
It does not snapshot collections.
Retain borrowed entities across traversal or mutation when needed.

### Message/partial splitting and joining

Replace the old split/reconstruction entrypoints with
`g_mime_message_partial_split(message, max_size, &count, &error)` and
`g_mime_message_partial_join(options, first_message, partials, count, &error)`.
`first_message` is the enclosing message of the first numbered fragment,
not just its message/partial body. Both return owned results; release every
message in a split result and free the array.

Split preserves whole lines, so a fragment may exceed `max_size` by one byte
when LF falls at the boundary. An unnecessary split returns a reference to
the original message, not a clone. Otherwise envelopes get independent headers
and fresh Message-IDs; their partial bodies share a separate reconstruction ID.
Payload header filtering is intentional, so splitting is not arbitrary
byte-chunking of the original full wire message.

Join sorts a private copy of the fragment array and combines the first
enclosing message's envelope with the payload's Subject, Message-ID, Encrypted,
and MIME-Version fields according to RFC 2046. It loads decoded content into
memory, independent of fragment streams. Invalid IDs, sequences, or totals
are errors. An empty input array returns NULL without error; supplied fragments
without a reconstructable message produce a parse error. Parameter getters
return -1 for absent/invalid numbers; absent and empty IDs are distinct.

## 4. Handle parser errors and options

`GMimeParserOptions`, `GMimeFormatOptions`, and `GMimeComplianceOptions` are
reference-counted boxed types, not GObjects. Replace
`g_mime_parser_options_free()`, `g_mime_format_options_free()`, and
`g_mime_compliance_options_free()` with the corresponding `*_unref()` functions.
`new()` and `clone()` return one owned reference; `ref()` retains the same
instance. Boxed copies (including `GValue` copies and binding-owned copies)
now share the instance instead of cloning its settings.

Use `clone()` when you need independent settings. Existing internal cloning
is unchanged, including parser/header snapshots, reader/parser compliance
settings, and scoped writer options.
Parser clones still borrow warning callback data and do not copy its destroy
notification; keep that data alive for all users of the clones.
Reference counting is atomic, but concurrent setting changes require external
synchronization.

`get_default()` returns a borrowed reference owned by GMime. Do not unref
that borrowed reference; acquire your own reference to retain it. Shutdown
releases GMime's reference rather than invalidating independently held
references. Clone the defaults before changing settings.

`g_mime_parser_get_persist_stream()` and `g_mime_parser_set_persist_stream()`
are removed. Choose `GMIME_CONTENT_STORAGE_MEMORY` (copy content) or
`GMIME_CONTENT_STORAGE_SOURCE` (retain source-backed content) using
`g_mime_parser_new_with_stream_and_storage()` or
`g_mime_parser_init_with_stream_and_storage()`. Source storage requires working
tell, seek, and substream operations; it reports errors rather than silently
falling back to memory. Keep the backing input open and unchanged while
source-backed content is in use. Memory storage does not require seeking.
Reinitialization discards buffered input, so it is not a replacement for
changing storage between messages in an active mailbox parse.
The basic stream constructor retains its existing automatic behavior:
source-backed content for seekable streams, memory for nonseekable streams.

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

Generic UTF-16 and UTF-32 text and RFC 2047 encoded words use little-endian
bytes, consistently with GMime's encoders rather than the platform iconv
default. Explicit-endian charsets retain their byte order. The low-level
`GMimeFilterCharset` and iconv wrappers still use native conversion behavior.

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

The MIME root has depth zero; multipart children and embedded message bodies
each add a level. At the configured depth limit, containers become leaf parts
retaining raw body content. The raw-header limit includes the field name,
colon, folds, and terminating newline, and is per header, not per header block.
`allow_addresses_without_domain=FALSE` rejects bare local addresses but still
permits them inside angle brackets; it is not SMTP envelope validation.

`UnexpectedNullBytesInHeader` identifies the first NUL in each header.
`Unexpected8BitBytesInHeader` identifies the leading byte of the first malformed
UTF-8 sequence, not the first non-ASCII byte. Both now report `EXACT` positions,
including physical lines and byte columns across folded headers and initial
BOMs. Consumers of earlier GMime 4 development builds should remove heuristic
relocation of these diagnostics. Generic UTF-8 warnings remain suppressed for
address headers in favor of their specialized diagnostics. Body-byte positions
remain exact and unchanged.

Reader content callbacks now borrow input-buffer slices directly; their lifetime
is still limited to the callback. Copy bytes that must survive it. A trailing
lone CR at EOF is delivered before encoding-validator finalization;
withheld LF/CRLF sequences at EOF are delivered afterward.

Compliance-enabled content scanning now combines structural and transfer-encoding
checks for clean Base64, quoted-printable and UU content. This is an internal
optimization: logger setup, diagnostic positions and ordering, issue budgets,
and raw content callbacks are unchanged. No application migration is required.

### Header inspection

`GMimeParserHeaderRegexFunc` and `g_mime_parser_set_header_regex()` have been
removed. For streaming inspection, subclass `GMimeReader` and override
`GMimeReaderClass::header_read`. It supplies the complete raw header, explicit
field-name/value boundaries, and the stream offset. Filter header names in the
application; the borrowed bytes are not NUL-terminated and must be copied if
retained after the callback. Skip headers marked invalid if matching the old
callback's valid-header-only behavior.

Applications that also need a constructed message can instead inspect its
`GMimeHeaderList` through `g_mime_entity_get_header_list()`. Use
`g_mime_header_get_name()`, `g_mime_header_get_offset()` and
`g_mime_header_get_raw_value()` to obtain the name, original stream offset and
raw value. Inspect nested entities/messages explicitly if their headers matter;
the old regex callback observed those as well. `GMimeParser` remains final and
does not accept an application-supplied reader subclass.

### Loading and saving files

The convenience message/entity loaders use memory storage by default, unlike
the parser's automatic source-storage selection. They consume input at its
current position without closing caller-owned streams, parse one message or
entity rather than a mailbox, and report empty input as a parse error.
Use their `_with_storage()` variants for explicit source-backed storage.
`g_mime_entity_load_with_content_type()` parses body-only input with a synthetic
Content-Type header and memory storage; its header offsets refer to that
synthetic input.

`g_mime_entity_write_to_file()` and `write_content_to_file()` create or truncate
the destination and report open/write/flush/close failures through `GError`.
They do not provide atomic replacement, delete partial files on failure, or
guarantee fsync-style durability. Never write over a file still backing the
entity's source content. File loaders and writers close their own streams;
an earlier operation error takes precedence over a cleanup error.

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

Raw assignment only requires a nonempty value ending in LF; it does not
validate header syntax or prevent line injection. Explicit raw values bypass
international reformatting. Decoded header getters replace embedded NULs with
U+FFFD while leaving raw bytes unchanged. This differs from checked text-body
decoding, which rejects decoded NULs.

### Checked headers, duplicates, and display strings

`g_mime_header_new_full()` and `set_value_full()` validate UTF-8, charsets,
and structured Content-Type/Disposition values before publishing changes.
Failures preserve the old header and owner state. NULL charset selects UTF-8;
an available charset that cannot represent the text may still use the
formatter's lossless UTF-8 fallback. These are not replacements for every
legacy tolerant parser or setter.

Choose header-list operations deliberately:

| Operation | Duplicate and identity behavior |
|---|---|
| `set_value_full()` | Updates the first object, preserving later duplicates |
| Legacy `set()` | Updates the first object and collapses duplicates |
| `replace()` / `replace_value_full()` | Replaces at the first match and removes later matches |
| `set_header_at()` | Replaces one slot without collapsing duplicates |
| `add()` / `insert_header()` | Retains the supplied object, permitting shared/duplicate instances |

Instance lookup/removal uses identity; field-name lookup ignores ASCII case.
Shared header edits affect every containing list. Clone before insertion for
independent edits; clones preserve raw bytes but reset the source offset to -1.
Parser-option snapshots still borrow warning callback data.

`g_mime_header_to_string()` is an unfolded display string without a final
newline, not transport serialization. Parameter display strings are likewise
not transport encoding. Use the corresponding stream writers for wire output.
`g_mime_header_get_value_with_charset()` returns independently allocated text,
without replacing the cached value or modifying raw bytes.

Parameter names are unique within a list, unlike header names; object insertion
with a duplicate name is a programming error. Parameter mutations can update
all Content-Type/Disposition owners sharing the instance. The `always_quote`
parameter setting and `always_quote_param_values` formatting option request
quoting but do not override required RFC 2231 encoding. Empty values are
always quoted; encoded-word lookalikes and control characters require encoding.
Property/header trimming uses ASCII linear whitespace, not arbitrary Unicode
whitespace.

### Received models and diagnostic anonymization

`GMimeReceived` exposes ordered clauses and an optional `GDateTime` retaining
its timezone offset. Parsed clauses may contain recovery values that checked
constructors/setters reject. Collections retain shared instances; named setters
update the first matching clause, and adding a missing named clause sorts
stably by identifier. Parsing or editing a model does not authenticate a
Received header.

`g_mime_received_to_string()` returns the value only, including leading
whitespace and the configured final newline, not the `Received:` field name.
Parsed date spelling survives until date assignment. Model serialization does
not perform RFC 2047 or IDN conversion; comment parentheses remain literal.

`GMimeAnonymizer` is for structure-preserving diagnostics, not guaranteed
redaction. Preserved fields, dates, delimiters, whitespace, and byte counts
remain visible. Its writers preserve duplicates and ignore ordinary
hidden-header suppression. They leave the destination open, flush successful
output, and report I/O failures through `GError`; failed output may be partial.
Message/entity operations may insert MIME-Version on constructed messages and
populate lazy report caches. Review the output before sharing it.

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

Certificate and signature wrappers have been removed. Native GPGME key and
signature timestamps retain GPGME's types and sentinel conventions.

Message dates already used `GDateTime` in 3.x; do not change them unnecessarily.

## 7. Review formatting, codecs, and subclasses

The public `GMimeEncoding` state structure, state/sizing macros, and
`g_mime_encoding_*` step functions are removed. Use the `GMimeEncoder` and
`GMimeDecoder` GObject interfaces and concrete types from `gmime-codecs.h`.
Release codecs with `g_object_unref()`. Cloning preserves incremental state;
reset begins a new stream. `GMimeFilterBasic` direct-codec constructors retain
the supplied codec and its current state, while filter copies use reset clones.

Codec calls return byte counts, not `GError`. Insufficient output capacity or
invalid arguments are programming errors, not malformed-input reports.
An output estimate of `G_MAXSIZE` means overflow and must not be passed to
encode/decode/flush. Select LF/CRLF explicitly: existing direct MIME codec
constructors retain LF defaults, while the newer encoder-filter factories
default to CRLF. UU encoding emits payload and the zero-length line, not
the surrounding begin/end markers.

Finish every encoded stream with `g_mime_decoder_flush()`, including when
the final input is empty. Empty `g_mime_decoder_decode()` calls are not EOF.
Allocate the capacity reported by `g_mime_decoder_estimate_output_length()`
before either operation; it includes buffered bytes. Repeated empty flushes
emit nothing. Call `g_mime_decoder_reset()` before decoding another stream.
Custom `GMimeDecoder` implementations must provide the new `flush` virtual
and estimates covering both decode and flush. The native bounded-buffer API
is retained.

EOF recovery now emits complete bytes from partial Base64/uuencode groups
and preserves incomplete quoted-printable and RFC 2231 percent escapes
literally. yEnc discards dangling escapes while preserving its running CRC.
Decoder filters, decoded content, encoded-word runs, and encoded parameter
runs finalize automatically. An unencoded parameter segment ends the
preceding encoded run, even when that segment is empty.

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

`ensure_newline` defaults to FALSE and applies at message boundaries, not to
every standalone entity or multipart child. It does not alter binary content.
Built-in serialization is iterative and retains custom `write_to_stream`
dispatch, but custom recursive overrides remain the application's
responsibility. Signed children suppress international reformatting and
hidden-header filtering; do not replace verification serialization with
ordinary reformatting.

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

The new body builder, attachment collection, visitor, and report helpers are
optional conveniences; a working explicit MIME tree need not be rewritten
to use them.

### Plain text to HTML

`GMimeFilterHTML`, its flags, and `gmime-filter-html.h` are removed.
Use `GMimeTextToHtml` for plain text or `GMimeFlowedToHtml` for RFC 3676
flowed text. These are `GMimeTextConverter` objects, not `GMimeFilter`
subclasses: call `g_mime_text_converter_convert()` with input/output streams
or `g_mime_text_converter_convert_to_string()` for UTF-8 text.

The converters generate a document wrapper by default; enable
`output_html_fragment` when embedding the result. Plain text produces line
breaks, while flowed text is reassembled into paragraphs. Both recognize
links and nested quotations. Use `GMimeHtmlTagCallback` to customize or omit
tags; see [examples/custom-blockquotes.c](examples/custom-blockquotes.c)
for depth-specific classes and matching end-tag callbacks.

The old flags do not have one-to-one replacements. Leading/repeated spaces
and tabs are not converted to `&nbsp;`; supply appropriate CSS such as
`white-space: break-spaces` when preserving plain-text layout. Citation
colors/classes belong in callbacks and CSS. Link callbacks can omit anchors
while retaining their text. Do not expect byte-identical legacy output or
automatic compatibility with an existing filter pipeline.

For link detection without HTML conversion, use `g_mime_url_scan()` from
`gmime-url-scanner.h` (also included by `gmime.h`). It scans a bounded UTF-8
range and returns byte offsets plus a borrowed URI prefix in `GMimeUrlMatch`.
Resume at `match.end` to find subsequent links. The caller must supply valid
UTF-8 and character-aligned bounds; the scanner does not validate the encoding.

### Text conversion and previews

The standalone converters also support plain copying, flowed encoding/decoding,
enriched-to-HTML, HTML-to-text, and HTML-to-HTML. String APIs consume and return
UTF-8; stream APIs expose input/output charsets and optional BOM detection and
emission. Both BOM options default to off. Malformed, incomplete, or
unrepresentable characters are errors rather than silently replaced.
This strict policy is different from tolerant message-body decoding and the
legacy `GMimeFilterCharset` byte-skipping policy.

Stream conversion starts at current positions, flushes successful output,
and leaves both streams open. Output may be partial on failure; string
conversion returns no partial result. Converters can be reused after failure
but not concurrently/reentrantly, or with overlapping input/output storage.
Plain copying preserves newlines; generated converter newlines are LF.
Flowed wrapping and preview lengths count Unicode scalar values.

HTML-to-HTML conversion and tag callbacks are not a general-purpose sanitizer.
Choose application-specific filtering, URI, and rendering policies for
untrusted content.

### Low-level TNEF

`GMimeTnefReader` and `GMimeTnefPropertyReader` expose forward-only attributes,
recipient rows, named/multi-valued MAPI properties, typed values, raw streams,
and embedded readers. Advancing the enclosing attribute invalidates its
property reader and child streams/readers. Fixed-width scalar values can be
reread; variable-length values are claimed by the first consuming operation.
Use immutable `GMimeTnefProperty` / `GMimeTnefPropertySet` snapshots when values
must survive advancement. They retain ordering and duplicates and use boxed
reference counting rather than GObject ownership.

Advancing skips unread attribute bytes and checks the preceding checksum.
Malformed-input diagnostics use the compliance logger; I/O and invalid
operations use `GError`. Successful advancement alone is not proof that all
attributes were well formed.

Values use UTF-8, `GBytes`, `GValue`, and `GDateTime`; currency uses an exact
signed integer in units of 1/10000. FILETIME loses sub-microsecond precision.
String conversion uses GMime's selected iconv backend.
Reader options are cloned at construction. Configure nesting, per-value and
cumulative materialization limits and compliance reporting before reading.
Embedded readers share the root's materialization/reporting budgets; raw
streaming bypasses materialization limits, so impose separate byte budgets
when streaming untrusted values.

TNEF writers require seekable temporary storage but can target nonseekable
output. Embedded writers inherit the configured temporary-stream factory.
Explicitly close value streams, embedded writers, property writers, and the
outer writer in order, checking errors. Finalization does not complete output;
abandoning a child faults its parent.

The compressed-RTF writer supports LZFu and the uncompressed wrapper.
`GMimeFilterRtfCompressed` incrementally decodes these formats and exposes
mode/CRC status; callers must inspect it rather than assume output is valid.
Unknown modes are discarded and trailing data is ignored.

## 8. Migrate cryptography to native GPGME

Include `<gmime/gmime-gpgme.h>` and use the `gmime-gpgme-4.0` pkg-config
dependency. It is installed only with cryptography enabled. Ordinary
`<gmime/gmime.h>` and `gmime-4.0` users do not need GPGME headers.

`GMimeCryptoContext`, `GMimeGpgContext`, `GMimePkcs7Context`,
`GMimeCertificate`, `GMimeCertificateList`, `GMimeSignature`,
`GMimeSignatureList`, and `GMimeDecryptResult` have been removed, along
with their headers, registration APIs, enums, and password callbacks.
There is no replacement GObject context or backend registry.

| Former API | Replacement |
|---|---|
| Context constructors | `gpgme_new()` and `gpgme_set_protocol()` |
| Password callbacks, engine and network settings | Native GPGME context configuration |
| User ID signing arguments | `gpgme_get_key()` and `gpgme_signers_add()` |
| Recipient string arrays | NULL-terminated `gpgme_key_t` arrays |
| `g_mime_crypto_context_sign/verify/encrypt/decrypt()` | `g_mime_gpgme_sign/verify/encrypt/decrypt()` |
| Key import/export | `gpgme_op_import/export()` with `g_mime_gpgme_data_new()` |
| Certificate/signature lists | Native keys and result-linked lists |
| GMime encrypt/verify/decrypt flags | Native encryption flags and context settings |
| Session-key argument/export flag | GPGME `override-session-key` / `export-session-key` context flags |

All MIME crypto functions now take an explicit, caller-owned `gpgme_ctx_t`.
Create and configure it before calling; release it with `gpgme_release()`,
not `g_object_unref()`. Call `g_mime_init()` before creating contexts
(it initializes GPGME in crypto-enabled builds). Use
`GPGME_PROTOCOL_OpenPGP` for PGP/MIME and existing inline helpers,
`GPGME_PROTOCOL_CMS` for S/MIME, and the protocol matching the received
MIME `protocol` parameter for detached verification. GMime validates the
match; it does not switch protocols or create a context automatically.

Configure signing keys with `gpgme_signers_add()`. GMime preserves the
signer list, callbacks, engine and policy settings; with no configured
signers, GPGME's default-key behavior applies. Recipient keys are borrowed
for the call. Applications resolve and validate keys using GPGME.
Stream, PGP/MIME and inline OpenPGP encryption take a second NULL-terminated
key array, `hidden_recipients`, immediately after `recipients`. Pass NULL
to retain ordinary encryption, or supply Bcc keys to hide only their key IDs.
The arrays must not overlap. Passing NULL for both selects native symmetric encryption.
With hidden recipients, keys must have valid OpenPGP fingerprints. GMime
constructs GPGME's recipient string internally, retaining native encryption
subkey selection. `GPGME_ENCRYPT_THROW_KEYIDS` still hides all recipients.
Hidden recipients are rejected for CMS; the S/MIME convenience API is unchanged.
Applications remain responsible for removing Bcc headers before sending.
Crypto-enabled builds now require GPGME 1.11.0 or later.
Session-key and other optional context flags require a GPGME version
supporting those flags; always check native configuration errors.

Calls are synchronous and require exclusive use of the context. Low-level
stream helpers honor its armor and textmode settings. MIME helpers
temporarily override those two settings to produce the required wire format
and restore them on success or failure. They do not silently enable network
access, trust bypasses, session-key export, or passphrase handling.

Verify now returns `gboolean` for **operational success**, not signature
validity. Inspect each `gpgme_signature_t` in the verification result for
status, summary, validity and application trust policy. A bad signature may
accompany successful verification. Decrypting OpenPGP can return separate
native decrypt and verification results; CMS signed-data verification
returns an extracted entity through an output parameter.

Optional native result outputs are initialized to NULL and independently
retained with `gpgme_result_ref()`. Release every non-NULL result with
`gpgme_result_unref()`, even when the operation fails: diagnostic results
may still be returned. They survive the next operation and context release.
Nested signature/recipient nodes are borrowed from their parent result.
For keys that must outlive a lookup/result, follow GPGME's key-ref rules.
Never use `g_object_unref()` on native keys or results.

The stream adapter holds a reference to its `GMimeStream` until
`gpgme_data_release()`. It starts at the current stream position and neither
rewinds nor closes it. Discard partial output when an operation fails.
Existing inline helpers still mutate a part's content on success.
For non-mutating multi-block receiving, use `g_mime_part_scan_openpgp()`
and process each `GMimeOpenPGPBlock` independently with
`g_mime_openpgp_block_verify()` or `g_mime_openpgp_block_decrypt()`.
The document retains all decoded bytes, including unprotected gaps;
large snapshots automatically spill to temporary files. Discovery works
without crypto and is introspectable.

### Inline OpenPGP discovery and authentication boundaries

Discovery snapshots transfer-decoded bytes before charset, HTML, or flowed
processing. It does not mutate the part, perform cryptography, import keys,
prompt, or access the network, but can reposition the backing stream.
No candidates means a valid document with zero blocks; missing content and
I/O failure are errors. Only exact, unquoted line-start armor markers with
LF/CRLF endings are recognized.

Block status describes framing, not cryptographic validity. `COMPLETE` means
matching markers; `TRUNCATED`, `INTERRUPTED`, and `MALFORMED` retain recoverable
original bytes. Public/private key blocks are never automatically imported,
and standalone signature blocks are not processed by the receiving helpers.
Verification accepts complete clear-signed and PGP MESSAGE blocks; decryption
accepts complete PGP MESSAGE blocks and verifies embedded signatures. GPGME
decides whether the payload supports the requested operation.

Documents, blocks, and read-only streams independently retain the snapshot,
which survives replacement or destruction of the original part. Document
streams expose all bytes; block streams start at zero within their block.
Use document-relative block offsets and lengths to preserve unprotected gaps.
Closing one view does not invalidate other views. Large snapshots use temporary
files, so impose input-size/disk budgets. Automatic cleanup is not secure
erasure, and crashes can leave temporary files behind.

Process each block with separate output, results, and error handling. Failure
does not prevent processing later blocks; discard partial output. Protection
covers only the processed payload, not adjacent text, other blocks, MIME headers,
or the whole document. Decryption alone does not authenticate the sender.
Do not automatically render decrypted bytes as HTML or parse them as nested
MIME; preserve visible protection boundaries and apply application policy.
The existing single-value OpenPGP classification still describes the first
detected block, not this multi-block model.

### PKCS#7 extraction without verification

`g_mime_application_pkcs7_mime_unwrap()` extracts encapsulated MIME content
from SignedData **without verifying signatures**. It supports DER/BER framing
and requires encapsulated id-data; detached content and other payload types
are rejected. Use native GPGME verification when authenticity matters.
Compression/decompression and unwrapping are available without GPGME; use the
decompressor's `max_size` argument to bound expanded data.

Native crypto methods are deliberately excluded from GIR/Vala. Ordinary
MIME objects, compression, OpenPGP detection and parsing remain available
to GNOME bindings, including builds without cryptography. Binding clients
that need crypto must supply GPGME and integration bindings themselves.

## 9. Validate the migrated application

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
