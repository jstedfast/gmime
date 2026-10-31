# GMime 4 implementation status

For application migration from GMime 3.2.15, see [PORTING.md](PORTING.md).
This document records implementation scope, compatibility decisions, and
validation history; historical test results are not current CI guarantees.

## Baseline and compatibility contract

GMime 4 is being developed as a C/GObject port of MimeKit.Core. The initial
reference revision is MimeKit 04f17021, with GMime 211a0608 as the baseline.
This is an incremental migration, not yet a declaration of feature parity.

- GLib 2.88.0 or newer is required for building and using GMime 4.
  This is the GLib API baseline packaged by Ubuntu 26.04 LTS.

- `GMimeParserOptions`, `GMimeFormatOptions`, and `GMimeComplianceOptions` are
  atomically reference-counted boxed types. `ref()`/`unref()` replace `free()`
  ownership; boxed copies share the instance, while explicit clones and
  internal snapshots remain independent.
  Defaults remain borrowed, with GMime releasing its reference at shutdown.
  Parser clones still borrow warning callback data without its destroy notify.

- Parser `get/set_persist_stream()` accessors are removed. Choose memory or
  source-backed content with the explicit-storage constructor or stream
  initializer. Reinitialization discards buffered input; it does not change
  storage in the middle of an active mailbox parse. Basic stream construction
  keeps its existing automatic seekable-source/nonseekable-memory behavior.

- GMime 4.0.0 excludes async/cancellation APIs, parser-options-local MIME type
  registration, TNEF, standalone text conversion, and DKIM/ARC/DMARC.
  Sender and Resent-Sender remain address lists. The existing global MIME
  type registry is unchanged. Raw multipart boundary access stays private
  unless users explicitly request it. Deferred features may be considered
  for later 4.x releases; they are not 4.0.0 release requirements.

- Meson >= 1.3.0 replaces Autotools. Use `meson setup _build`,
  `meson compile -C _build`, and `meson test -C _build`. Autogen, configure,
  and the Automake files are no longer provided. See README.md for build
  options and optional dependencies. Installed headers, pkg-config, GIR,
  and Vala packages retain their GMime 4.0 names.

- Crypto now borrows caller-owned `gpgme_ctx_t` contexts and returns
  independently retained native results. The context hierarchy and
  certificate/signature/decryption wrappers are removed; native timestamps
  retain GPGME semantics. Include `gmime/gmime-gpgme.h` and depend on
  `gmime-gpgme-4.0` for crypto. Core headers and GNOME bindings remain
  GPGME-independent; native crypto calls require application-supplied
  bindings. See PORTING.md for ownership, configuration and error handling.
  Existing MIME and mutating inline operations are migrated. Multi-block
  inline-PGP discovery now returns an immutable, transfer-decoded document
  with ordered blocks and preserved unprotected gaps. Large snapshots
  automatically spill to temporary files. Non-mutating per-block native
  verification/decryption returns independent results; discovery remains
  available to GNOME bindings and without crypto. See InlinePGP.md.
  Manual S/MIME programs compile, but runtime roundtrips remain unverified:
  the current headless GPGME 2.0.1 CMS engine fails PKCS#12 import in
  Pinentry despite the context's loopback setting. The committed S/MIME
  certificates also expired in 2018.

- `GMimeObject` is now `GMimeEntity`. Replace `g_mime_object_*` with
  `g_mime_entity_*`, `GMIME_OBJECT*` with `GMIME_ENTITY*`, `GMIME_IS_OBJECT*` with
  `GMIME_IS_ENTITY*`, and `GMIME_TYPE_OBJECT` with `GMIME_TYPE_ENTITY`.
  Include `gmime/gmime-entity.h` instead of `gmime/gmime-object.h`.

- `GMimeDataWrapper` is now `GMimeContent`. Replace `g_mime_data_wrapper_*` with
  `g_mime_content_*` and the `DATA_WRAPPER` portion of its type macros with
  `CONTENT`. Include `gmime/gmime-content.h` instead of `gmime/gmime-data-wrapper.h`.

- `GMimeMultipart` now calls its leading text `preamble`, matching MimeKit
  and MIME terminology. Replace `g_mime_multipart_get_prologue()` and
  `g_mime_multipart_set_prologue()` with `g_mime_multipart_get_preamble()`
  and `g_mime_multipart_set_preamble()`. The public struct field is also
  renamed to `preamble`; no compatibility aliases are retained.

- `InternetAddress`, `InternetAddressMailbox`, `InternetAddressGroup`, and
  `InternetAddressList` are now `GMimeInternetAddress`, `GMimeMailboxAddress`,
  `GMimeGroupAddress`, and `GMimeInternetAddressList`, respectively. The function
  prefixes are `g_mime_internet_address_*`, `g_mime_mailbox_address_*`,
  `g_mime_group_address_*`, and `g_mime_internet_address_list_*`.
  Include `gmime/gmime-internet-address.h` instead of `gmime/internet-address.h`.

- Address type macros now follow the same convention as other GMime types:
  `GMIME_TYPE_INTERNET_ADDRESS`, `GMIME_TYPE_MAILBOX_ADDRESS`,
  `GMIME_TYPE_GROUP_ADDRESS`, and `GMIME_TYPE_INTERNET_ADDRESS_LIST`, with
  `GMIME_IS_*` checks and `GMIME_*` casts.

- These are actual symbol and registered GType renames, not aliases.
  Applications and subclasses must be rebuilt. Use the gmime-4.0
  pkg-config package, GMime-4.0 introspection namespace, and gmime-4.0
  Vala package. GMime 3 headers and binaries are not interchangeable.

- The zero-valued `GMimeContentEncoding` member is now
  `GMIME_CONTENT_ENCODING_UNSPECIFIED` (formerly `DEFAULT`). Its numeric
  value and behavior are unchanged; no compatibility alias is retained.

- g_mime_content_write_to_stream() now copies the original encoded bytes,
  matching MimeKit's WriteTo(). Use g_mime_content_decode_to_stream() for
  the decoded-data behavior of GMime 3's data-wrapper write_to_stream().
  Subclasses must move decoded-output overrides to decode_to_stream; the
  write_to_stream virtual now supplies raw data to the default decoder.

- `GMimePart` now separates Content-MD5 computation from header assignment,
  following MimeKit's ComputeContentMd5(), VerifyContentMd5() and ContentMd5.
  Use g_mime_part_compute_content_md5(part, error) to obtain an owned,
  unwrapped base64 digest without changing headers or content.
  g_mime_part_set_content_md5(part, NULL) now removes every matching header
  instead of generating a digest; compute and assign explicitly when needed.
  Part-property trimming removes only ASCII linear whitespace (SP, HTAB,
  CR and LF), as does header unfolding. Empty values are retained, and unchanged
  assignments preserve existing raw headers.
  g_mime_part_verify_content_md5() now takes a final GError ** argument:
  absent/blank values, missing content and mismatches return FALSE without
  an error, while rewind/read failures propagate instead of comparing a
  partial digest. Computing without content or a backing stream reports
  GMIME_ERROR_INVALID_OPERATION. The helpers read decoded content, not
  overridden content-write methods; text media types normalize LF to CRLF
  without charset conversion or an added final newline. Other media types
  retain their bytes. Each computation rewinds the source, leaves it open at
  EOF on success, and preserves its failure position on error. Shared-source
  concurrency restrictions from g_mime_content_open() still apply.
  Native zero-byte reads without EOF report EIO, and failures without errno
  report GMIME_ERROR_GENERAL. Existing native header-cache and duplicate
  precedence rules remain unchanged.

- GMimeReferences is replaced by the GObject GMimeMessageIdList. Include
  gmime/gmime-message-id-list.h, use g_mime_message_id_list_* functions,
  clone() instead of copy(), add() instead of append(), and g_object_unref()
  instead of free(). Mutations now validate and normalize complete IDs;
  raw-header parsing remains tolerant. GMimeMessage exposes a persistent
  mutable References list through g_mime_message_get_references().

- GMimeParserOptions now exposes max_address_group_depth (default 3) and
  max_mime_depth (default 1024), following MimeKit's depth controls.
  A zero address-group limit rejects groups but permits mailboxes. MIME
  containers at the configured limit become GMimePart leaves retaining
  their raw body content. The root entity has depth 0; each multipart
  child or embedded-message body adds one level. Existing warning callbacks
  report limit violations. Public parsing now uses the explicit-stack
  reader and reader-backed model adapter. Synchronous entity/message
  serialization uses the private iterative writer described below.

- `GMimeMultipartReport` is a `GMimeMultipart` subclass for multipart/report.
  Its constructor requires a UTF-8 report type; get/set_report_type()
  access the Content-Type report-type parameter. The getter is borrowed
  and nullable for parsed reports without the parameter. The setter trims
  ASCII linear whitespace (SP, HTAB, CR and LF), preserves case and internal
  whitespace, and accepts empty strings but not NULL. VT, FF and Unicode
  whitespace remain part of the value. Equivalent assignments preserve raw
  headers and parameter metadata. Direct Content-Type/parameter changes remain
  visible, and the report type is not inferred from or synchronized with
  the children. Factories and the parser select the subclass automatically;
  existing multipart depth limits and generic report payload parts still
  apply.

- `GMimeBodyBuilder` builds the common message-body trees from nullable
  plain-text and HTML strings and persistent attachment/resource collections.
  Both text bodies produce multipart/alternative in plain-then-HTML order;
  linked resources wrap HTML in multipart/related with HTML as its root.
  Attachments wrap the result in multipart/mixed. Without a text body, one
  attachment is returned directly and multiple attachments form mixed.
  Resources are ignored without HTML. An otherwise empty builder returns an
  empty text/plain part. NULL means absent, while an empty string is present.
  Each build creates fresh text parts and containers but retains attachment
  and resource instances, ordering and duplicates without changing headers.
  Returned bodies own a reference, including the single-attachment case.
  BodyEncoding is a native charset name, defaulting to utf-8, rather than a
  managed Encoding object. Native charset aliases/MIME names are reused;
  only MIME-token charset names with available converters are accepted.
  Text is converted from UTF-8 with no preamble or transfer-encoding selection,
  matching TextPart.SetText for representable text. Generic utf-16 and utf-32
  use little-endian converters without a byte-order mark; unicode, csunicode,
  utf16 and utf32 aliases are normalized to those canonical names. Custom
  encoders, managed replacement fallbacks and embedded NULs in UTF-8 C strings
  are not supported. Unrepresentable text and unsupported charsets return explicit
  G_CONVERT_ERROR errors, without changing the selected encoding or collections;
  no charset or replacement fallback silently alters the body. Converted
  text exceeding the native GByteArray limit returns GMIME_ERROR/EOVERFLOW.

- `GMimeAttachmentCollection` ports the synchronous attachment-building
  collection, including existing-entity list operations and creation from
  bytes, streams and files. A NULL content type selects registry detection.
  Text uses the best-encoding filter with the 7bit constraint; other ordinary
  parts use base64. message/rfc822 content becomes a GMimeMessagePart using
  nonpersistent parsing. Only auto-detected, seekable message inputs recover
  from format errors as application/octet-stream, restoring the original
  position first; explicit types, nonseekable inputs and I/O errors do not
  use this recovery. Empty message input counts as a format error. Existing
  native parser tolerance and limits still apply.
  New attachments set disposition filename and Content-Type name; linked
  collections use inline disposition and a relative Content-Location string.
  File-name extraction follows MimeKit's platform-primary separator and
  index-greater-than-zero rule, not generic basename normalization. Native
  Content-Location validation remains GLib-based rather than System.Uri-based.
  Linked file names must be relative references, matching MimeKit.
  Linked file names rejected by the native URI rules (including unescaped
  spaces and backslashes) return a format error without adding an entry.
  Supplied content types are retained, not cloned, for ordinary parts.
  Stream inputs are copied from their current position and are not closed;
  file inputs are opened and closed internally. Byte inputs are copied,
  unlike MimeKit's shared byte-array storage. Ordinary content buffers use
  GMimeStreamMem/GByteArray and reject sizes exceeding G_MAXUINT bytes with
  an explicit EOVERFLOW error.
  Collections retain each occurrence by reference, match entities by identity,
  and leave headers unchanged for direct list operations. Native clear/remove
  release references without force-disposing externally retained entities.
  Insert requires an existing index, including rejecting index == Count,
  matching MimeKit; add appends. Indexed access replaces managed enumeration
  and CopyTo. Cancellation and asynchronous overloads remain unported.

- MimeKit's BoundedStream maps to the existing g_mime_stream_substream()
  abstraction, not a new wrapper class. Substream creation now rejects
  negative starts, ends below -1, reversed ranges and ranges outside the
  parent with EINVAL before dispatch. Bounds use absolute backend
  coordinates; end=-1 inherits a finite parent's end. Empty ranges are valid.
  Each backend still defines its own seek and explicit-close behavior.
  This increment strengthens the contiguous memory backend; file/descriptor
  lifetime handling has not been changed.
  GMimeStreamMem views now retain their shared GByteArray, surviving explicit
  parent close or buffer replacement while keeping independent positions
  and bounds. Reads stop at actual storage even when a finite bound extends
  farther; finite length still describes the declared range. Writes allocate
  safely and zero newly exposed gaps, retaining partial writes and a zero
  result at a full end bound rather than MimeKit's whole-request rejection.
  Native zero-count I/O remains a no-op, without repositioning or caching EOF.
  SEEK_SET remains absolute, finite SEEK_END uses the view's end, and
  unbounded seeks retain native seek-to-grow behavior. Same-position seeks
  do not regrow externally truncated storage. Invalid origins fail with
  EINVAL; overflowing seeks/counts and sizes above G_MAXUINT fail with
  EOVERFLOW (previous oversized memory seeks used ENOSPC).
  Reassigning a memory stream's own byte array is safe, resets its bounds
  and position, and hands ordinary ownership back to the caller; substreams
  retain their internal storage reference until replacement or close.

- GMimeStreamFs now applies bounded read/seek behavior to descriptor-backed
  views. Reads at a finite end return zero instead of EINVAL, without
  repositioning the shared descriptor. Cached EOF does not prevent later
  reads from seeing file growth, and backward seeks clear it as appropriate.
  Unbounded length queries use the actual file size, clamping an extent
  before the view's start to zero rather than failing with EINVAL.
  Counts above G_MAXSSIZE, overflowing seek/position arithmetic and offsets
  not representable as off_t fail with EOVERFLOW. Windows additionally
  limits counts to G_MAXINT for the narrower CRT I/O API. Invalid seek
  origins fail with EINVAL instead of asserting. Closed tell and substream
  creation now fail with EBADF, like other closed operations.
  Preserve native partial writes, EINVAL for a write at a full finite bound,
  absolute SEEK_SET and seek-without-growth semantics. Positive SEEK_END
  offsets consult physical EOF; zero/negative finite offsets use the view's
  end, while unbounded cached EOF supplies its observed seek origin.
  Descriptor ownership and explicit-close behavior are unchanged: views
  borrow the parent's handle, so an owning parent must not be explicitly
  closed until its views are no longer needed.

- GMimeStreamFile now applies the same bounded synchronous behavior to
  buffered FILE views, with private per-view EOF state instead of the shared
  feof indicator. Short positive reads do not cache EOF; a zero read does.
  Other views' I/O and length queries no longer erase or invent that state,
  and reads still retry the file after cached EOF. Same-position seeks
  preserve it; reset clears it even at the start. Finite end reads return
  zero without moving the FILE pointer. Unbounded lengths query actual
  storage and clamp extents ending before the view's start to zero.
  Preserve native partial writes, absolute SEEK_SET, seek-without-growth,
  positive physical SEEK_END and zero/negative declared-end conventions.
  Counts above G_MAXSSIZE, overflowing arithmetic and offsets not
  representable as long fail with EOVERFLOW rather than truncating fseek
  arguments. Invalid origins fail with EINVAL; closed queries fail with
  EBADF. Every nonzero fseek result is an error. Zero-progress stdio errors
  return -1 instead of EOF/success, preserving positive partial counts;
  missing stdio errno values become EIO. Each I/O attempt clears stale
  shared indicators before observing its own result. Buffered writes still
  require flush or close to report delayed output failures.
  fclose failure now detaches the invalidated pointer, preventing a second
  close during finalization. Descriptor/FILE ownership remains unchanged:
  finish all borrowed views before explicitly closing their owning parent.

- GMimeStreamMmap now applies bounded synchronous read/seek behavior to
  fixed-size mapped storage. Finite end reads return zero, and both reads
  and partial writes stop at the actual mapping even when declared bounds
  extend farther. Constructors cap mappings to actual file storage rather
  than mapping pages past EOF; finite length still reports the declared
  range. Unbounded lengths clamp an extent before the view's start to zero.
  Pointer arithmetic occurs only after validating the available byte range.
  Nonempty reads require PROT_READ and writes require PROT_WRITE; attempts
  without the requested protection fail with EACCES instead of faulting.
  Counts above G_MAXSSIZE and overflowing seek arithmetic fail with
  EOVERFLOW; invalid origins/ranges fail with EINVAL, and closed operations
  consistently fail with EBADF. Same-position seeks preserve cached EOF;
  reset clears it, and backward seeks clear it as appropriate.
  Preserve native absolute SEEK_SET, fixed-size partial writes, zero writes
  beyond physical storage, and EINVAL at a full finite end. Finite SEEK_END
  uses the declared end; unbounded SEEK_END uses the fixed absolute mapping
  extent, correcting the old extra start offset. Positive unbounded END
  offsets still fail, and negative targets before the start still clamp.
  Empty root mappings remain unsupported with EINVAL; empty substreams
  remain valid. Platforms without mmap fail construction with ENOSYS.
  Ownership is unchanged: views borrow the descriptor and mapping. Finish
  them before explicitly closing an owning parent, and never truncate an
  underlying file while a mapping remains active.

- GMimeStreamBlock ports MimeKit's MemoryBlockStream synchronous read/write,
  seek, SetLength and ToArray operations using 2048-byte non-contiguous blocks.
  Existing memory-stream consumers remain unchanged. Reads and writes span
  blocks without copying old content when growing. Seeking beyond the current
  extent is rejected, overwrites preserve length, and shrinking clamps the
  caller's position, sheds unused blocks and clears the discarded tail.
  New blocks are zero-initialized and freed directly rather than pooled;
  extension never exposes content retained from another managed stream.
  The backing capacity is G_MAXINT * 2048 bytes. Matching MimeKit, SetLength
  accepts that limit, but writes ending at it are rejected with EOVERFLOW.
  Native zero-length I/O remains a no-op, including after close.
  Native bounds and SEEK_SET use absolute coordinates. Length and contiguous
  copies cover the bounded visible extent. SetLength is relative to the start
  bound, resizes shared storage and updates a finite end bound. Substreams
  share content and backing length without sharing positions or bounds;
  resizing one leaves other views' positions unchanged. Closing a view releases
  its storage reference without invalidating other views; nonempty operations
  on the closed view fail with EBADF. Reset rewinds without clearing data.
  Contiguous copies are independent GByteArrays and fail with EOVERFLOW above
  G_MAXUINT bytes. Copy failures also report an optional GMIME_ERROR using
  errno codes, so language bindings do not mistake NULL for an empty array.
  Memory allocation follows GLib's normal allocation policy.
  Managed capabilities, timeouts and asynchronous methods remain unported.

- GMimeStreamNull now follows MimeKit's MeasuringStream synchronous
  write, seek and length behavior. Logical length is the highest written
  position, not the cumulative byte count; overwrites do not inflate it.
  g_mime_stream_null_set_length() changes the measured size without recording
  output, clamping the position only when shrinking past it. Invalid seeks
  fail with EINVAL instead of clamping, and nonempty reads fail with EBADF
  instead of returning synthetic zero bytes. Length, position and native
  statistics overflow report EOVERFLOW without changing state.
  The existing written/newlines fields remain cumulative statistics,
  including overwrites, and newline counting remains optional.
  Native bounds still limit writes and define a finite stream's length;
  set_length updates a finite end bound relative to its start. SEEK_SET
  retains this stream's existing start-relative convention. Reset clears
  logical measurement and statistics but retains bounds and the counting
  option. Substreams retain independent measurement/statistics, initialized
  from the parent's measured extent. Native zero-length I/O and close remain
  no-ops; managed capabilities, disposal checks and asynchronous methods
  are not ported.

- GMimeStreamCat now ports MimeKit's ChainedStream synchronous read loop.
  Reads fill the requested buffer across short reads and child boundaries,
  without seeking or resetting children, so nonseekable and prepositioned
  sources are supported. Empty and exhausted chains and finite read bounds
  return zero. Exhausted children are not polled again; adding another
  source resumes reading. Native EINVAL read errors at confirmed EOF are
  treated as exhausted boundaries; other errors remain failures even if
  the child reports EOS. A transient zero read without EOS does not advance
  to another child. Source errors are not
  swallowed: bytes already read are returned first, positions retain that
  progress, and the failing child remains current for retry. Signed position
  overflow reports EOVERFLOW without consuming unrepresentable bytes.
  Sources remain retained by reference without being explicitly closed;
  the native substream API remains unchanged.
  Explicit repositioning still requires the affected child
  streams to support it. Asynchronous operations, managed capability checks,
  per-source leaveOpen and disposal state remain unported.

- GMimeStreamCat also ports ChainedStream's synchronous write and flush
  behavior. Writes use each child's current position without seeking or
  resetting it. Non-final children require a usable length and position and
  retain their existing extents; the final child may grow if its own bounds
  permit it, and need not support length, tell or seeking. A full child is
  flushed only when more output needs to spill into the next child. Failed
  boundary flushes retain the current child for retry. Explicit flush affects
  only the current child and is a no-op with no current child; a subsequent
  write resumes at the final child. Read EOF remains cached across such
  writes, without discarding the selected flush destination. Adding a child,
  resetting or seeking to a different position reopens reading.
  Native short writes are retried without
  skipping destinations. Errors return accepted bytes first, retaining errno
  and position; zero-progress writes report EIO. Empty-chain writes report
  EBADF, exhausted outer write bounds report EINVAL, and signed position
  overflow reports EOVERFLOW. Native zero-length writes remain no-ops.
  Child bounds and write failures still prevent a fixed-capacity tail from
  growing; asynchronous writes and flushes remain unported.

- GMimeStreamCat now also follows ChainedStream's synchronous seek traversal.
  Forward seeks physically move each traversed child and leave the previous
  child selected when landing exactly at its end, including the end of the
  entire chain. Backward seeks reposition the selected child and seek only
  the intervening children back to their starts, without resetting filters
  or touching later, unvisited children. Seeking to the current position is
  a no-op, including cached EOF. Successful repositioning updates cached EOF.
  Child positions, length and seek errors remain observable; failed seeks
  return -1 without rolling back already completed child operations. An
  explicit reset rewinds all children even at the logical start, recovering
  displaced children after a partial backward seek.
  Child bounds retain native absolute coordinates, while concatenation
  offsets respect its own start and end bounds. Length sums and seek-origin
  arithmetic report EOVERFLOW rather than wrapping; invalid targets and
  origins report EINVAL, and seeking an empty chain reports EBADF.
  The native substream API and the lack of managed capability prechecks
  remain unchanged.

- GMimeStreamFilter now follows MimeKit's FilteredStream completion
  lifecycle. Flush completes the write-side chain once until another
  nonempty write; flushing a fresh stream also completes empty input.
  Completion output survives source write errors, including short writes,
  so repeated flush/close retries only the unwritten suffix. A new write
  first drains a previously failed completion. Zero-progress writes report
  EIO. Close propagates completion errors before closing an owned source.
  Read-side filters complete once at confirmed source EOF; final output is
  drained before EOF is reported, and subsequent reads do not poll the
  source again. Reset resets the source and filters and discards pending
  output. Do not manipulate the filters or source while output is buffered.
  Native differences remain: explicit close completes writes, destruction
  does not; ownership is configurable and defaults to leaving the source
  open. Flush does not flush the underlying source, matching MimeKit's
  implementation rather than its Flush documentation. Zero-length native
  reads/writes remain no-ops, and native transient zero reads without an
  EOF indication do not complete the filters. Cancellation, asynchronous
  operations and managed disposal state are not added.

- GMimeStreamFilter now provides contains() and remove_filter() for
  MimeKit's filter-instance collection operations. Filters are compared
  by identity; duplicate entries retain independent references, and
  remove_filter() removes only the first occurrence without resetting
  filters or completing output. Existing add() IDs and remove(id) remain
  supported and identify individual occurrences. get_source() returns
  the borrowed source, matching FilteredStream.Source. Do not modify the
  chain or manipulate the source while filtered output is buffered.

- GMimeFilterBasic's create_encoder()/create_decoder() factory families
  port MimeKit's EncoderFilter.Create/DecoderFilter.Create without adding
  duplicate public wrapper types. Enum and by_name forms select Base64,
  quoted-printable or uuencode codecs; all other values, including unknown
  enum values and names, return GMimeFilterPassThrough. Encoder factories
  default to CRLF with a 78-byte requested line limit; explicit newline
  variants accept the existing Unix/DOS enum and a maximum line length.
  Only Base64/quoted-printable validate the 60..998 range and cap it at 76;
  uuencode and pass-through ignore the length. Newline format is always
  validated, including for pass-through. Names use only linear whitespace
  (SP, HTAB, CR and LF) and stop at linear whitespace or a semicolon,
  retaining MimeKit's Google Docs header
  workaround. Name arguments must be non-NULL valid UTF-8 C strings.
  Invalid arguments report native programming-error criticals and return
  NULL. The decoder UU factory searches for begin-lines, not payload-only
  input. Direct-codec constructors and existing new() behavior are unchanged.

- Base64, quoted-printable and uuencode encoders now expose
  new_with_newline_format() constructors matching MimeKit's explicit
  Unix/DOS selection, using the existing GMimeNewLineFormat enum.
  All generated line endings use LF or CRLF, including quoted-printable
  hard/soft breaks and the UU zero-length terminating line. Estimates
  account for the entire selected terminator, including buffered input
  and finalization. Clone, reset and flush preserve the configuration,
  including through GMimeFilterBasic's direct-encoder constructor and copy.
  Base64/quoted-printable retain their 60..998 argument range and 76-byte
  effective cap (Base64 rounds down to complete quartets). Existing native
  constructors and convenience filters deliberately retain LF defaults,
  unlike current MimeKit's default CRLF constructors; choose DOS explicitly
  for equivalent wire output. yEnc and serialization policy are unchanged.

- `GMimeFilterPassThrough` ports MimeKit's public PassThroughFilter as
  a stateless GMimeFilter subclass. Filtering and completion return the
  original input pointer, length and prespace without copying, buffering
  or allocating output storage. Empty calls and reuse after completion
  behave the same way; reset is a no-op for conversion state, and native
  copy creates a fresh filter. Output is borrowed from the supplied input.
  Existing GMimeFilterBasic constructors and pass-through codecs retain
  their codec-backed, copying behavior; this is a separate zero-copy filter.

- `GMimeFilterAnonymize` ports MimeKit's internal AnonymizeFilter as an
  ordinary native GMimeFilter subclass. Only ASCII space, tab, CR and LF
  are preserved; every other byte becomes lowercase ASCII 'x', including
  NUL, vertical tab, form feed and high-bit bytes. The transformation is
  byte-oriented, not Unicode/locale-aware, and never normalizes newlines.
  Output has exactly the input length and uses filter-owned storage even
  for all-whitespace or empty input, leaving caller input/prespace intact.
  All output is immediate; completion adds no bytes, reset has no conversion
  state to discard and native copy creates independent output storage.
  Higher-level header, message and entity APIs are provided by GMimeAnonymizer.
  Byte counts and whitespace remain visible.

- `GMimeFilterCharset` now follows MimeKit CharsetFilter's incremental
  conversion loop for valid, representable input while retaining iconv.
  Complete characters are drained in the current call, growing output
  storage without losing its prefix; only incomplete input is backed up.
  Completion also drains target shift sequences, including empty input.
  Reset discards pending bytes and shift state, while copy creates a fresh
  converter rather than cloning a partial character. New get_from_charset()
  and get_to_charset() accessors return filter-owned copies of the original
  constructor names, not managed Encoding objects or canonicalized names.
  Buffer estimates and growth check arithmetic before allocating. Unexpected
  iconv failures are fatal rather than silently returning unconverted input;
  this void filter API cannot return conversion errors.
  Native recovery remains byte-skipping for invalid/unrepresentable input
  (including old GnuWin32 ERANGE reports) and discards incomplete final bytes.
  Managed replacement fallbacks, custom Encoding objects and code-page
  constructors are not ported. Generic UTF-16/UTF-32 BOM and endian behavior
  remain iconv-defined; use explicit endian names for equivalent wire bytes.

- `GMimeFilterFrom` now follows MimeKit's MboxFromFilter in ESCAPE mode
  and ArmoredFromFilter in ARMOR mode, retaining the native type and mode
  enum. Only exact, case-sensitive "From " markers at the beginning of
  input or after LF are transformed; bare CR and existing ">From " lines
  are not special. Escape inserts ">", while armor replaces only F with
  "=46" and is not a complete quoted-printable encoder. Apply armor after
  quoted-printable encoding and label the containing part accordingly.
  Only exact incomplete prefixes are buffered; completion now emits them
  unchanged instead of retaining and losing them. Match offsets use heap
  storage instead of per-line stack allocations. Unchanged consumed input
  is still returned without copying where native prespace permits it.
  Reset discards buffered input and line state; native copy retains the
  mode but starts fresh. DEFAULT remains an alias for ESCAPE, and invalid
  constructor modes retain their existing fallback to ESCAPE.

- `GMimeFilterDos2Unix` and `GMimeFilterUnix2Dos` now follow MimeKit's
  newline-filter completion rules, retaining the existing native names
  and constructor settings. CRLF pairs are recognized across chunks;
  non-newline bytes are preserved without character-set conversion.
  DOS-to-Unix completion now emits a held-back trailing bare CR instead
  of dropping it. With ensure_newline enabled, that CR and the required
  LF are treated as a CRLF pair and become one LF. Unix-to-DOS completion
  records a generated LF, preventing duplicate output on repeated
  completion and incorrect CRLF handling on subsequent input.
  Empty input with ensure_newline completes with LF or CRLF respectively.
  Reset discards conversion state without changing ensure_newline;
  native copy retains the setting but starts without buffered state.

- `GMimeFilterStrip` now follows MimeKit's TrailingWhitespaceFilter,
  retaining its existing native name and packed whitespace buffer.
  Only ASCII spaces and tabs are stripped before CR, LF and completed
  end-of-input; all other bytes and newline spellings are preserved.
  Pending blanks span filter calls, including empty nonfinal calls, and
  remain available when completion supplies additional text. Previously,
  native completion discarded those blanks before inspecting its final
  nonempty buffer, losing internal whitespace. Completion now clears only
  the blanks remaining after that buffer is processed. Reset discards
  pending blanks; native copy continues to create a fresh filter rather
  than copying buffered input.

- GMimeFilterBest now uses MimeKit's BestEncodingFilter scanning and
  encoding decisions while retaining native charset detection. The default
  encoding query uses a 78-byte line limit; the new
  g_mime_filter_best_encoding_with_max_line_length() accepts 60 through 998.
  CRLF terminators are excluded even across input buffers, and unterminated
  lines are included in queries without requiring a flush. From-lines are
  recognized on LF, matching MimeKit; an unterminated final From-line is
  not recognized as a marker. Plain ASCII now returns explicit 7bit, and
  high-bit data can return 8bit under the 8bit or binary constraints.
  The binary constraint still encodes long lines and From-lines when needed.
  Native counters are 64-bit rather than MimeKit's signed 32-bit counters;
  the 17-percent threshold is calculated without floating-point rounding
  or multiplication overflow. g_mime_part_get_best_content_encoding()
  analyzes text/message content, returns 7bit for missing text/message
  content, and uses base64 (or binary under the binary constraint) for
  other media types.

- `g_mime_part_get_best_content_encoding()` now takes a final GError ** and
  delegates to g_mime_part_get_best_content_encoding_with_max_line_length()
  using the default limit of 78. The new query accepts 60 through 998 bytes,
  excluding CRLF. Both return GMIME_CONTENT_ENCODING_UNSPECIFIED on error,
  with GMIME_ERROR and errno (or GMIME_ERROR_GENERAL when errno is unset).
  Rewind, read, final rewind and analysis-flush failures no longer return an
  encoding chosen from partial data. Text/message content is analyzed through
  decode_to_stream, including content subclass overrides, without changing
  headers or encodings. Default content rewinds before and after a successful
  copy, preserves its failure position, and never closes the source.
  A missing backing stream reports GMIME_ERROR_INVALID_OPERATION.
  Following MimeKit, absent text/message content returns 7bit and other
  media types return base64 or binary without I/O or argument validation.
  For analyzed content, invalid limits or constraints report EINVAL only
  after successful decoding and flushing. Legacy part preparation remains
  unchanged.

- MimeKit's static `MimeTypes` registry is exposed through gmime-types.h:
  g_mime_types_get_mime_type(), g_mime_types_try_get_extension(), and
  g_mime_types_register(). Both built-in lookup tables are ported, including
  preferred extensions and legacy aliases; the host's MIME database is not
  consulted. File lookup uses the final extension with platform-native
  directory separators, and unknown or absent extensions return
  application/octet-stream. Registration prepends a missing dot and
  replaces the two specified mappings without removing older aliases.
  Strings are copied on registration and returned with full ownership.
  Registrations are process-wide, survive shutdown, and are synchronized
  with lookups. Native comparisons ignore ASCII case; unlike MimeKit's
  Unicode ordinal comparer, custom non-ASCII keys only match non-ASCII
  bytes exactly. Inputs are not trimmed or parsed as Content-Type values.
  NULL arguments and empty registration values are programming errors.

  The tables include MimeKit's legacy mapping refresh from commit
  ee3d18f32f2ef37c3d18293ca5cf727883b707f8: 591 extension-to-type and 457
  type-to-extension entries. Updated names cover AIFF, AVI, Debian packages,
  CAD, Excel, WordPerfect, PKCS#12, SMIL, troff and XPM; additions include
  Arrow, SQLite, HLS, Matroska audio, JPEG 2000, HEIF sequences and OpenDocument
  formats. Reverse aliases, including historical misspellings, remain
  supported. Ambiguous extensions such as .ts, .rpm, .xml, .rtf and .db keep
  their prior behavior. Regression cases cover both lookup directions,
  alternate extensions, ASCII case-insensitivity and legacy aliases.

- `GMimeIterator` ports MimeKit's message-based depth-first iterator as a
  GObject. It starts before the first body entity; move_next() visits
  multipart children and encapsulated message bodies without visiting
  message objects or decoding leaf content. move_to() accepts IMAP-style
  paths, with a terminal 0 representing a multipart message body. The API
  exposes current, parent, depth, multipart index, current message and
  ancestor accessors, skip_children(), reset(), and a native is_valid()
  state query. Exhaustion is sticky until reset or navigation. Malformed
  paths return FALSE with GMIME_ERROR_PARSE_ERROR and preserve position
  and pending skips; syntactically valid missing paths exhaust the iterator
  without an error. NULL/empty paths and accessors used without a current
  entity are programming errors. The message and active ancestry are
  retained, so removing entities during traversal does not invalidate
  current pointers; collections and containing-message assignments remain
  live, not snapshotted. Borrowed entities must be retained across navigation.
  new_for_entity() also traverses a retained entity subtree, including its
  root, with root-relative paths and depths. Multipart roots are path 0;
  other roots are path 1. No containing message is inferred, so message
  accessors may return NULL outside encapsulated messages. Supplying a
  message is equivalent to new(message). GMimePartIter has been removed;
  backward traversal, cursor cloning and iterator editing are not carried
  forward. See PORTING.md for changed initialization and path conventions.

- `GMimeVisitor` is an abstract GObject visitor with virtual type-specific
  visit methods and traversal hooks. Subclass it and call
  g_mime_visitor_visit(), or use g_mime_entity_accept() to bypass the
  entry-point override. Default visits traverse message bodies, multipart
  children in order, and encapsulated messages without decoding leaf content.
  Specialized visits chain through the corresponding base-type visits;
  overriding a method without chaining prunes its subtree. Child traversal
  observes the live collection, including additions and removals. Accept
  temporarily retains the current entity so removing it during a visit is
  safe. NULL is accepted only by visit(), not accept() or the typed methods.
  GMimeMessage inherits GMimeEntity, so it shares the same entry point and
  accept virtual. The native visit_entity(), visit_part(), and visit_message()
  correspond to MimeKit's VisitMimeEntity, VisitMimePart, and VisitMimeMessage.
  visit_body() and visit_children() retain their canonical roles;
  visit_message_part_message() is MimeKit's VisitMessage(MessagePart) hook.
  Native crypto subclasses inherit ordinary part/multipart dispatch without
  verification or decryption. TNEF remains outside the initial port scope.

- `GMimeHeaderListCollection` is an ordered, mutable GObject collection of
  `GMimeHeaderList` groups, providing the foundation for delivery-status
  reports. Groups are retained, not cloned; membership and removal use object
  identity. Duplicate groups and sharing across collections are supported.
  Internal owner notifications follow MimeKit: adds, successful removals,
  clears (including an empty clear), and group/header edits notify; each
  duplicate occurrence forwards group changes. Indexed replacement updates
  ownership and subscriptions but does not itself notify, and assigning the
  same group is a no-op. Getters return borrowed groups that can be retained
  independently. Invalid arguments use GLib programming-error checks.
  Native indexed access replaces managed enumeration and CopyTo; no separate
  read-only property is needed for this always-mutable collection.

- `GMimeMessageDeliveryStatus` is a leaf `GMimePart` selected for both
  message/delivery-status and message/global-delivery-status. Its checked
  get_status_groups() lazily loads a borrowed `GMimeHeaderListCollection`
  using default parser options, skipping empty blocks and retaining header
  separators. Missing content becomes an empty stream and collection.
  Reads preserve existing content and transfer-encoding headers. Group,
  collection and contained-header changes serialize all groups using default
  format options and replace the content without changing the desired transfer
  encoding; indexed replacement follows the collection's non-notifying contract.
  Cached groups survive inherited content replacement and can be retained
  independently of the entity. The Office365 workaround decodes the remainder
  after a group declaring base64, quoted-printable or uuencode, including parser
  read-ahead on nonseekable decoded streams; it does not recursively decode
  later transfer-encoding fields. Malformed input and header-limit violations
  retain completed groups, matching MimeKit's FormatException recovery, with
  an explicit native warning. I/O and rewind errors instead return NULL with
  GError, without caching partial results, so loading can be retried.
  Existing native header recovery differences described below still apply.
  Ordinary native MIME parsing/writing also differs from MimeKit around a
  lone terminal CR (including CR immediately before a multipart boundary);
  delivery-status group loading does not alter those generic stream rules.

- `GMimeMessageDispositionNotification` is a `GMimePart` subclass selected
  automatically for message/disposition-notification and
  message/global-disposition-notification. Its checked get_fields() lazily
  decodes the content and loads the first header block with default parser
  options, returning a borrowed `GMimeHeaderList`. Missing content becomes
  an empty content stream and field list. Reading fields preserves existing
  content; list and contained-header edits replace content using native
  default format options while preserving the transfer-encoding header.
  The field list is cached, including across inherited content replacements,
  matching MimeKit; editing cached fields then replaces that content.
  Loading failures return NULL with GError and can be retried. Retained field
  lists remain usable after the notification is released. Existing native
  header-list recovery applies: empty field names are rejected, and colonless
  lines after valid fields are skipped rather than retained as MimeKit does.

- `GMimeMessageFeedbackReport` is a leaf `GMimePart` selected automatically
  for message/feedback-report. Its checked get_fields() lazily loads the
  first decoded header block with default parser options and returns a
  borrowed, cached `GMimeHeaderList`. Reads preserve existing content;
  missing content becomes an empty stream and field list. List and header
  edits regenerate content with default format options while preserving
  the transfer-encoding header. The cache survives inherited content
  replacement, and retained fields outlive the report. Failed loads return
  NULL with GError, cache no partial list, and can be retried. Serialization
  failures warn and preserve the old content. The shared native field-loading
  and serialization helpers also serve disposition notifications.
  Existing native malformed-header recovery differences still apply.

- `GMimeTextRfc822Headers` is a `GMimeMessagePart` subclass for
  text/rfc822-headers. Create one with g_mime_text_rfc822_headers_new()
  and use the inherited message getter/setter; the setter retains a
  nullable message and the getter returns a borrowed nullable reference.
  Factory construction and unencoded parsing select this type. As in
  MimeKit, parsing and writing preserve an optional embedded body;
  this is not a body-stripping operation. An empty standalone payload
  produces an empty embedded message. At the MIME depth limit, or with
  base64, quoted-printable or uuencoded content, parsing instead returns
  a GMimeTextPart retaining the declared type, headers and raw bytes.
  The first transfer-encoding header controls type selection. Existing
  native leaf decoding still uses the last duplicate encoding header,
  unlike MimeKit; legacy message/rfc822 empty-payload behavior is unchanged.

- g_mime_parser_construct_part() and g_mime_parser_construct_message()
  now take a final GError ** argument. Pass NULL only if the caller does
  not need error details. Fatal syntax errors, header length violations,
  and input I/O failures return NULL and discard partial results.
  Clean end of input returns NULL without setting an error; recoverable
  MIME defects continue to use the warning callback. A failed parser
  must be reinitialized with g_mime_parser_init_with_stream() before reuse.
  I/O errors use the GMIME_ERROR domain with a positive errno code;
  fatal syntax errors use GMIME_ERROR_PARSE_ERROR.

- The mbox Content-Length control now belongs to GMimeParserOptions.
  Replace g_mime_parser_get/set_respect_content_length() with
  g_mime_parser_options_get/set_respect_content_length(), and pass those
  options to g_mime_parser_construct_message(). It defaults to FALSE,
  is copied by clone(), and has no effect on other input formats.
  Following MimeKit, the first valid Content-Length header wins. Its raw
  value must begin with whitespace followed by unsigned decimal digits
  in the range 0 through 2147483647. Leading zeroes and trailing suffixes
  are accepted; signs and overflowing numeric prefixes are rejected.
  Invalid headers do not prevent a later valid duplicate from being used.
  This postpones scanning for the next mbox From-line, rather than
  truncating the body. An unrepresentable absolute end offset fails with
  GMIME_ERROR_PARSE_ERROR instead of wrapping.

- The fallback_charsets list has been replaced by the single
  GMimeParserOptions charset_encoding option, following MimeKit's
  CharsetEncoding property. Use get_charset_encoding() and
  set_charset_encoding() instead of get/set_fallback_charsets().
  The getter returns a borrowed charset name; the setter copies it and
  returns gboolean with an optional GError for unsupported or empty names.
  A failed setter preserves the previous value. NULL resets it to UTF-8.
  New options default to UTF-8, and clones own independent copies.
  Unknown 8-bit header text is always tried as UTF-8 first, then as the
  configured charset, then ISO-8859-1. Configuring another charset can no
  longer override valid UTF-8 or remove the final Latin-1 fallback.
  Charset availability and character mappings come from platform iconv.

- GMimeParam now exposes always_quote, and GMimeFormatOptions exposes
  always_quote_param_values, matching MimeKit's AlwaysQuote and
  AlwaysQuoteParameterValues. Both default to FALSE. Either setting
  requests quoting for otherwise unquoted values; FALSE never suppresses
  required quoting or the other setting. RFC 2231 encoded values remain
  unquoted. Empty values are always written as quoted empty strings.
  Folding accounts for both surrounding quotes and quoted-pair escapes.
  Format-option clones retain the setting, and changing a parameter's
  setting updates its owning Content-Type or Content-Disposition header.
  The new format setter requires mutable options from new() or clone(),
  not the global defaults. Other parameter formatting rules remain on the
  existing implementation pending the full formatter port.

- GMimeParserOptions now exposes max_header_length (default 16 KiB,
  matching MimeKit; originally 16 MiB).
  This positive byte limit applies to each individual raw header, including
  its field name, colon, value, folds, and terminating LF or CRLF, not to
  the whole header block. Exceeding it reports GMIME_ERROR_HEADER_TOO_LONG
  before buffering or exposing the oversized header, matching MimeKit.
  Field names may span input buffers, subject to the same limit. This
  intentionally accepts long field names that the current reference
  MimeKit reader rejects because of its fixed-size input buffer.

- allow_unquoted_commas_in_addresses (default TRUE) controls recovery of
  unquoted commas in display names, independently of domainless-address
  handling. A comma immediately following a quoted word remains a separator.
  allow_addresses_without_domain now defaults to TRUE, matching MimeKit.
  Setting it to FALSE rejects bare local addresses; this does not prohibit
  local addresses inside angle brackets, matching the reference parser.
  GMime retains its best-effort address-list parsing, matching MimeKit's
  internal header parser rather than its all-or-nothing public TryParse().
  Strict mode still requires comma separators. A failed header-list parse
  does not leave partial recipients in the message or discard addresses
  from other headers.

## Porting contract

### Scope and dependencies

The first implementation scope is MIME parsing and writing, headers,
addresses, parameters, transfer encodings, and the message object model.
TNEF, standalone text conversion, and DKIM/ARC/DMARC are excluded from 4.0.0
and may be considered for later 4.x releases. Async/cancellation and a
parser-options-local MIME type registry are also outside the 4.0.0 scope.
The structured MIME compliance framework is now implemented on GMimeReader
and exposed by GMimeParser; remaining work is tracked in the migration
roadmap below.

The minimum dependency is GLib 2.88.0, matching
Ubuntu 26.04 LTS's packaged GLib API baseline rather than requiring a
newer upstream patch release. Build and package requirements,
documentation, and supported build environments use the same baseline.

Public APIs should mirror MimeKit's types, members, and behavior as
closely as practical, expressed using GMime naming and C/GObject
conventions rather than preserving GMime 3.x API shapes.

Preserve applicable inline comments from MimeKit when porting code,
including explanations of non-obvious behavior, edge cases, compatibility
workarounds, and algorithm invariants. Adapt the comments to the native
implementation; do not copy managed-only details or explanations that no
longer match the code. Keep these explanations next to the relevant logic,
not only in this migration document or API documentation.

### Dates and cryptography

GMime-owned public dates use GDateTime. Native GPGME keys and results use
GPGME timestamps and sentinel values, without implicit conversion.
Document ownership, timezone, and missing-date semantics for new APIs.

Retain the existing GPGME-backed OpenPGP and S/MIME functionality,
including GMimeMultipartSigned, GMimeApplicationPkcs7Mime, and
GMimeMultipartEncrypted. Preserve crypto-enabled builds and backend
integration throughout the migration. Porting the S/MIME and OpenPGP
implementations from MimeKit.Cryptography is explicitly out of scope;
GPGME remains the backend.

### Transfer encodings

Replace GMime 3.x's public GMimeEncoding state structure and step/outlen/
flush API with GMimeEncoder and GMimeDecoder GObject interfaces modeled
on MimeKit's IMimeEncoder and IMimeDecoder, and corresponding concrete
codec types. Both interfaces expose the content encoding, state-preserving
cloning, output-length estimation, and reset. Encoders provide encode and
flush operations; decoders provide decode. Preserve MimeKit's incremental
semantics while expressing buffer bounds, ownership, and errors safely
in C. Adapt content and filter consumers to these interfaces rather than
retaining the legacy encoding API as the new public contract.

The interfaces and standard MIME codecs are now implemented. The concrete
types are GMimeBase64Encoder/Decoder, GMimeQuotedPrintableEncoder/Decoder,
GMimeUUEncoder/Decoder, and GMimePassThroughEncoder/Decoder. Include
gmime-codecs.h (or gmime.h) to use them. The old public GMimeEncoding
structure, sizing/state macros, and g_mime_encoding_* functions are removed.
The legacy state/step/flush implementation and its private header are now
removed; no in-tree consumers depend on those primitives.
Use GMime's LGPL v2.1-or-later source headers for this port, without copying
MimeKit's MIT license comments.

Each concrete encoder and decoder has its own source file, with only the
private state needed by that implementation. The interface files contain
only their shared API dispatch and buffer checks; there is no generic codec
state or encoding switch for concrete implementations. This separation
also applies to the private percent codecs and RFC 2047 encoders.
Stateless encoding/SIMD helpers, buffer-overlap checks, and the CRC-32
lookup table remain shared.

Codec methods return gsize byte counts without GError. Invalid pointers,
insufficient output capacities, and invalid constructor arguments are
programming errors, checked with GLib precondition macros before changing
state. They are not malformed-input errors. NULL input is allowed only
with zero length. Allocate at least estimate_output_length() bytes for
each operation, including empty flushes; estimates include buffered data
and finalization overhead. An unrepresentable estimate returns G_MAXSIZE,
which must not be passed to encode/decode/flush. Buffers may overlap;
transforming codecs copy overlapping input before processing it.

Encoder flush accepts final input and resets the standard MIME codec state.
Base64 empty flush emits nothing when no line or input is pending.
Quoted-printable flush now follows MimeKit: a final unterminated content
line is followed by "=\n", even when it does not end in whitespace.
Decoders have no flush; incomplete sequences remain buffered until more
input or reset. UUEncoder emits payload and a zero-length line, not
begin/end markers. UUDecoder accepts a payload-only mode; otherwise it
finds a begin-line and owns that scanning state, including markers split
across input chunks.

GMimeYencEncoder/Decoder implement MimeKit's yEnc codec contracts and report
the default content encoding, since yEnc is not a MIME transfer encoding.
The encoder defaults to 128 columns and accepts lengths from 60 to 998;
an escape pair can extend a line by one byte. It emits payload only and
escapes periods as well as NUL, tab, CR, LF, and equals after adding 42.
The decoder supports full-block and payload-only modes, incremental
=ybegin/=ypart scanning, and =y termination. Metadata and advertised sizes
or checksums are not validated. Both expose the raw running CRC-32,
initialized to 0xffffffff; complement it for the conventional final CRC.
Flush finishes the encoder's current line but retains its checksum,
matching MimeKit; reset clears the checksum. Clones retain it. These
codecs work with the direct-codec GMimeFilterBasic constructors.
GMimeFilterYenc, its state/CRC macros and setters, and g_mime_yencode_step(),
g_mime_yencode_close(), and g_mime_ydecode_step() are removed. Use
g_mime_filter_basic_new_with_encoder() with a GMimeYencEncoder, or
g_mime_filter_basic_new_with_decoder() with a GMimeYencDecoder. Retain your
own reference to the codec to read its checksum; the filter also retains
a reference and uses the codec's current state.
Use g_mime_yenc_decoder_new(FALSE) for full-block decoding, including =ybegin
and optional =ypart lines, or TRUE for payload-only input. Unlike the old
filter, completion can consume a complete framed block even when no
earlier filter call has parsed its headers. Both modes ignore CR/LF and
stop at a line beginning with =y until reset, following MimeKit.
The encoder escapes periods as well as the other yEnc escape bytes, and
its output remains payload-only. Complete flushes the line but retains
the checksum; filter reset clears it. Filter copies use independent reset
clones preserving the configured line length or payload-only mode.
Codec checksum getters return the raw running CRC, not the complemented
CRC returned by the old filter getters. There is no legacy multipart CRC
seed or state-bit API. Use the framed block APIs below instead of implementing
framing in the application; aggregate multipart checksums still belong to the
caller.

### Framed yEnc blocks

GMimeYencOptions, GMimeYencHeader, GMimeYencReader and GMimeYencWriter now
provide MimeKit's complete-block API, including its private YEncFormat
parsing. Options are boxed snapshots; headers are immutable GObjects, and
readers/writers are GObjects retaining GMimeStream references. Native APIs
follow the synchronous GMimeStream model, rather than adding managed
Stream disposal, Task or CancellationToken contracts.

Readers skip surrounding text, recognize multiple blocks, validate multipart
ranges, decoded sizes, escapes, line limits and advertised block CRCs.
Checksums become available only after successful validation. Missing checksums
are accepted; an advertised whole-file CRC for a partial range remains
unverified metadata. Readers and writers do not assemble parts, interpret MIME
or perform NNTP dot-stuffing. Failed I/O or in-progress format validation
invalidates the instance; argument/state misuse before I/O does not.

Defaults match MimeKit: CRLF, ISO-8859-1 filenames, a 16 KiB encoded-line limit,
and a 1 GiB decoded-block limit. Public filenames are UTF-8. Conversion errors
are reported without substitution; native charset names with lossy iconv
suffixes are rejected. Control fields reject embedded NUL. Filenames are
untrusted metadata, never automatically opened as paths.

The payload encoder now also supports explicit CRLF. Existing constructors
retain LF for native compatibility. Cloning and reset preserve newline policy.
Block APIs expose finalized CRCs, while payload codecs retain their raw running
CRC contract.

### Parameter parsing and formatting

The Rfc2231Encoder helper and incremental Rfc2231Decoder are private,
matching their visibility in MimeKit. Rfc2231Decoder implements GMimeDecoder,
reports the default encoding, buffers incomplete percent sequences, and
supports cloning, reset, checked estimates, and overlapping buffers.
Rfc2231Encoder emits uppercase percent triplets for non-attribute octets;
literal plus signs remain plus signs rather than becoming spaces.

Parameter encoding and decoding now use these helpers. Continuations are
sorted before interpreting charset/language metadata and percent-decoding.
Decoder state spans adjacent encoded segments, so a percent triplet may
cross segment boundaries. An unencoded segment resets the percent state.
Malformed complete triplets pass through literally as three bytes rather
than retrying an embedded percent sign. A final incomplete triplet is
discarded, as in MimeKit; this intentionally changes GMime 3.x recovery.

RFC 2231 charset conversion now follows MimeKit's segment boundaries.
Adjacent encoded segments share both percent-decoder and charset state;
an unencoded segment ends the encoded run, including quoted empty values.
Each raw segment uses ParserOptions.charset_encoding with UTF-8 first and
Latin-1 last, and quoted raw segments are unquoted after conversion.
The first encoded segment selects the charset for later encoded runs;
raw segments do not inherit that charset. Encoded values with no valid
charset declaration or an unsupported charset use ISO-8859-1, independent
of the locale and configured raw-text fallback. A declaration requires a
nonempty charset and both apostrophes; otherwise its bytes are retained
as part of the value. Invalid declared-charset sequences are replaced
with question marks, including incomplete characters at run boundaries.
The native charset and language metadata are preserved, with Latin-1
recorded when it is used for a missing or unsupported charset. Charset
availability and character mappings still depend on platform iconv.

When RFC 2231 or RFC 2047 parameter encoding is needed, charset selection
now follows MimeKit:
ASCII for printable ASCII text, Latin-1 for values within that repertoire
(including controls), and the parameter's preferred charset or UTF-8 for
other characters. A preferred charset does not override the ASCII or
Latin-1 choices. Unsupported or non-round-tripping encodings fall back
to UTF-8 without changing the parameter's stored charset. Conversion is
byte-counted, preserving embedded zero bytes in UTF-16/UTF-32 output
before percent encoding. Native round-trip verification also rejects
platform best-fit mappings that MimeKit's representability check may
accept. Charset aliases and platform mappings can still differ.
Values passed to the parameter serializer must be valid UTF-8.

Parameter encoding-method selection now protects ASCII control characters
and literal encoded-word lookalikes containing "=?". Both require encoding
rather than ordinary quoting, including when quoting is forced. Selection
also includes the parameter name, equals sign, surrounding quotes, and
quoted-pair escapes in the configured line-length check. Empty values are
always quoted, even with oversized names.
RFC 2231 continuation output keeps the charset/language declaration intact
in the first piece, including when a long name leaves insufficient room.
Continuation pieces are now selected from complete Unicode characters
and independently converted to the chosen charset. Each piece uses raw,
quoted, or percent-encoded syntax as needed; long plain ASCII values no
longer acquire unnecessary charset declarations. Explicit language
metadata still forces a declaration, preserving the native language API.
When the declaration alone exhausts the first piece's budget, it is
emitted separately before consuming the first character.
Initial chunk sizing and shrinking now follow MimeKit's UTF-16 and
converted-byte estimates, including nearest-even rounding. Raw quoted
pieces are not incorrectly charged for percent encoding, and index growth
does not prematurely shorten chunks. A final actual-byte check retains
GMime's physical line bounds, accounting for continuation-index digits and
the following semicolon; it can shorten chunks when the reference overruns.
At least one complete character is emitted per payload piece even if an
oversized name leaves insufficient room. The fold flag and newline format
apply to all continuation breaks.
Converted bytes are written raw only when they match the original ASCII
text, or UTF-8 text when International formatting is enabled, preventing
raw-looking UTF-16/UTF-32 bytes from changing the value.
BOM-emitting Unicode encodings use explicit-endian charset labels without
converter-generated BOMs; literal U+FEFF characters are preserved.
Stateful encodings are flushed at character boundaries, so their byte
streams may contain additional reset sequences.

RFC 2047 parameters now use a dedicated writer rather than the general
header-text encoder. It encodes the entire value, preserving leading and
trailing whitespace, and uses the same lossless charset selection as
RFC 2231. Independently encoded words preserve Unicode character boundaries
and byte-counted UTF-16/UTF-32 data. ISO-2022-JP output uses Base64.
Initial placement and per-character estimates follow MimeKit. The shared
RFC 2047 word codec then checks actual output, preserving the native
75-character word bound and physical-byte line limits when estimates
underestimate escaped or multibyte data. Final words reserve the closing
quote and any following parameter's semicolon, without overfolding a
last parameter that exactly fits. An initial empty word is retained
when the estimator cannot fit the first scalar, as with ISO-2022-JP.
Oversized names still make unavoidable prefixes exceed the target;
progress guards prevent empty-word loops. Subsequent words begin on
tab-indented continuation lines when folding is enabled, or are separated
by spaces otherwise. Nonfolding output introduces no line breaks.

GMimeFormatOptions now exposes get/set_max_line_length(), following
MimeKit's MaxLineLength: default 78, with an inclusive range of 60 to 998.
The setter requires mutable options from new() or clone(), not the global
defaults. Clones preserve the configured length independently of the
newline format. Parameter encoding selection, ordinary parameter folding,
RFC 2231 continuations, and RFC 2047 parameter words honor this setting.
Actual first-word sizing prevents multibyte charsets from overflowing
short lines merely because the initial placement estimate fits.
An ordinary final international parameter can exactly fill the physical
line when MimeKit's UTF-16 estimate fits. Nonfinal parameters retain room
for the next semicolon instead of overrunning the byte limit.

Parameters now have a public `g_mime_param_new(name, value)` constructor,
independent `clone()` copies, and unencoded display strings. Parameter display
strings always quote values and escape quotes and backslashes; list display
strings prefix each entry with `; `. Transport encoding remains separate.
Programmatic names must be nonempty ASCII attribute strings, excluding MIME
specials and the RFC 2231 `*`, `'`, and `%` markers. Values must be UTF-8;
invalid arguments are reported through GLib's programming-error checks.

Parameter lists now support object addition, indexed insertion/replacement,
and object-identity lookup/removal. Name lookup remains case-insensitive.
Adding or inserting duplicate names, or replacing an entry with another
entry's name, is a programming error and leaves the list unchanged. Insertion
accepts indices through Count; replacement requires an existing index. Lists
retain references and observe mutations, allowing a parameter to be shared
by multiple lists. Removing or replacing an entry detaches its subscription.
Name-based assignment preserves the existing object, metadata, and spelling.
Equivalent value, charset, language, encoding-method, and object assignments
do not regenerate raw headers. Aliased string assignments are safe. All
successful additions, replacements, and removals update owning Content-Type
and Content-Disposition headers.

International parameter formatting now follows `allow_international`.
Short Unicode values use quoted UTF-8 even when their configured encoding
method is RFC 2231 or RFC 2047. Controls and text resembling encoded words
still require transport encoding, preventing accidental decoding or raw
header line breaks. Long values selected for RFC 2231 can use unencoded,
quoted UTF-8 continuations, split only at complete Unicode characters.
These raw continuations use UTF-8 regardless of the preferred charset;
the parameter's stored charset and language are not changed. Encoded
continuations still honor the lossless charset choice and language
declaration. Long values selected for RFC 2047 remain encoded.
Physical parameter line widths continue to count actual output bytes,
including UTF-8 bytes, quotes, escapes, and continuation indices, rather
than MimeKit's character estimates. This can produce different splits.
Oversized names still permit at least one complete character per piece.
Folding, newline format, and non-International output retain their existing
behavior, and untouched raw headers are not reformatted merely because
International formatting was requested.

`g_mime_param_list_parse_full()` provides checked, all-or-nothing parsing
with an explicit byte length and `GError`, following the checked address
API. Bounded buffers need not be NUL-terminated; embedded NUL bytes,
incomplete tokens, unterminated comments/quotes, and overflowing RFC 2231
continuation indices are rejected. Syntax errors report a byte offset.
Empty lists, empty semicolon-delimited entries, and empty values before
a semicolon are accepted. A bare equals sign at end of input is incomplete.
Strict compliance requires semicolon separators and quoted non-token
values. Both loose modes recover missing separators after quoted values,
unfold unquoted values, and trim trailing whitespace.

Checked and recovery parameter parsing now consume RFC 2231 prefixes with
an empty charset, including continuations, while retaining the Latin-1
fallback and language metadata. Unquoted values stop at comments, including
nested and folded comments, without losing loose-mode support for spaces
and folded filenames. Literal parentheses must be quoted or percent-encoded.
Regressions cover bounded parameter parsing and chunked multipart messages,
including attachment names, decoded content, and quoted/encoded boundaries.

Checked parsing uses case-insensitive names and MimeKit's duplicate
precedence: the first ordinary value wins, RFC 2231 values override ordinary
values, and the last standalone RFC 2231 value wins. The first occurrence
determines position and spelling. Continuations are grouped case-insensitively and
sorted by numeric index, retaining the existing charset/percent decoding
and language metadata. Ordinary decoded values retain MimeKit's RFC 2047
formatting preference without a redundant UTF-8 charset preference; bare
empty values keep the default method.

The loose-value trimmer matches MimeKit's corrected offset handling:
whitespace is removed from the actual end of the value, never from an
unrelated input offset that can cause retained whitespace or truncated
non-whitespace characters.

The existing `g_mime_param_list_parse()` and MIME-header parsing paths
retain best-effort recovery, duplicate parsed names, and first-name lookup.
Switching message parsing to the checked semantics remains a separate
policy change.

`g_mime_content_type_parse_full()` and
`g_mime_content_disposition_parse_full()` now provide the same bounded,
all-or-nothing contract for complete field values. They follow MimeKit's
throwing Parse APIs: malformed primary tokens, missing parameter separators,
incomplete comments, and invalid parameters fail instead of producing a
default model or a partially recovered result. Error offsets include the
primary tokens and are relative to the start of the supplied field value.
Parameters share the checked parameter-list parser, including compliance,
duplicate precedence, charset/language metadata, and correct loose trimming.

Content-Type accepts an ASCII atom excluding '/' for the type and an ASCII
MIME token including '/' for the subtype, following MimeKit's compatibility
grammar. Content-Disposition accepts an atom and preserves its spelling;
like MimeKit's ASCII conversion, non-ASCII atom bytes become '?'.
Comments and whitespace are accepted between or around primary tokens.
Quoted or missing disposition values are rejected rather than applying
MimeKit's nonthrowing TryParse-only recovery. Existing public legacy
parsers and internal message/header paths retain their previous recovery.
Parsed parameter mutations continue to update owning entity headers.
Retained parameter lists detach their callbacks when either content model
is finalized and can safely outlive it.

Content-Type and Content-Disposition now support deep `clone()` copies
and unencoded `to_string()` display strings. Clones own independent
parameter objects and preserve their order, name spelling, charset,
language, encoding method, and quoting preference. Legacy duplicate
parameters and arbitrary legacy disposition text are retained, rather
than normalized or rejected during copying.

Display strings include the field name and colon, always quote parameter
values, and preserve Unicode without transport encoding or folding.
They do not append a newline, but literal newlines already present in
values remain unchanged. They are not suitable for writing MIME messages;
the existing `encode()` APIs still produce transport field values with
leading whitespace and a final newline, without the field name.
Equivalent media-type, subtype, and disposition assignments now preserve
their existing storage and do not regenerate owning raw headers. Equality
is case-sensitive, so spelling changes still notify header owners.

Content-Type now has get/set accessors for `boundary`, `charset`, `format`,
and `name`; Content-Disposition has get/set accessors for `filename`.
These are case-insensitive parameter-list shortcuts, following MimeKit's
corresponding properties. A NULL assignment removes the first matching
parameter, while an empty string retains an explicit empty value.
Values are UTF-8 and are not trimmed or normalized. Charset names are
stored as supplied without checking encoding availability.
Existing parameter objects, name spelling, order, and formatting metadata
are preserved on assignment. Equivalent values and removal of an absent
parameter leave raw headers untouched; actual changes notify their owners.

The Content-Type boundary getter never generates a boundary, and its
NULL setter removes the parameter. The multipart-level boundary APIs
retain their existing lazy generation behavior. Content-Disposition's
filename property neither sets the Content-Type name nor falls back to it;
the existing part-level filename API retains that higher-level policy.

`g_mime_content_disposition_set_is_attachment()` chooses the lowercase
attachment or inline token without changing parameters. It uses ordinary
disposition change notifications so owning headers stay synchronized,
rather than copying MimeKit's current direct-field assignment, which
does not notify owners.

Content-Disposition creation, modification, and read dates now have
nullable `GDateTime` get/set accessors. Getters return a new owned reference
on each call; unref it with `g_date_time_unref()`. Missing, empty, whitespace,
and unparseable parameter values return NULL without altering the stored
text. Setters format the supplied date to whole seconds, retain no date
reference, and remove the parameter for NULL. As with the string shortcuts,
they preserve existing parameter metadata, notify owning entities on
changes, and avoid rewriting headers for identical formatted values.

The shared `g_mime_utils_header_decode_date()` parser now follows MimeKit's
DateUtils recovery rules, including nested comments, common numeric date
orders, uppercase AM/PM, missing times, and named or unsigned numeric
timezones. UTC, JST, and KST join the existing zone mappings. Numeric
zones must have valid minute fields and be between -1200 and +1400;
unknown or invalid zones fall back to UTC. A 23:59:60 leap second is read
as 23:59:59. Other invalid time tokens may recover as midnight when a
complete calendar date is available. Calendar and UTC-range validation,
two-digit-year handling, and fallback ordering follow MimeKit rather
than the previous GMime heuristics. This also affects Date headers and
other consumers of the shared utility. Date formatting retains its UTC
conversion for sub-minute timezone offsets. If that conversion falls
outside years 1 through 9999, the formatter reports a programming error
and returns NULL; message and disposition setters leave their existing
headers and parameters unchanged.

Content-Disposition size now has `gint64` get/set accessors, with -1
representing absence. Zero is an explicit size, and setting -1 removes
the first matching parameter. Values below -1 are programming errors
and leave the parameter unchanged. Unlike MimeKit's nullable setter,
negative sizes cannot be stored through this shortcut; generic parameter
assignment remains available for arbitrary text. The getter follows
MimeKit's invariant, unsigned decimal parsing: only ASCII digits are
accepted, leading zeros are allowed, and signs, whitespace, and overflow
beyond `G_MAXINT64` return -1 without rewriting the stored text. Assignments
preserve parameter identity and metadata and notify owning entities only
when the formatted value changes.

### Address parsing and formatting

The base address, mailbox, group, and address-list types now have separate
source files. They retain the combined `gmime-internet-address.h` public
header; shared formatting and parsing helpers remain private. List parsing
lives with the address-list implementation and shares its addr-spec parser
with mailbox construction.

Address-list header folding now honors MaxLineLength for mailboxes and
groups, including nested member lists. Exact fits remain on the current
line, and the first address token is never moved off the initial line.
Later addresses and group members can fold before their first token.
Long mailbox and group display names fold between words without breaking
quoted strings or encoded-word tokens. Oversized tokens remain intact
and may exceed the target, as can the first token and group punctuation.
Line wrapping retains an existing separator space or tab as continuation
whitespace; angle-address continuations use tabs. Newline format and
cloned options are respected. Existing nonfolding to_string() behavior
and untouched raw address headers are preserved.
RFC 2047 names share the phrase tokenizer, charset-aware splitting and merging,
and canonical MIME labels described below.
Address-header formatting now requires the entire list to parse. If parsing
fails, decoded assignments use the ordinary text formatter rather than
discarding invalid text or retaining only a valid prefix. Invalid parsed raw
address fields remain unchanged during International serialization, apart
from the writer's selected newline normalization.
Parsed mailbox and group display names now convert literal tabs to spaces
before RFC 2047 decoding, following MimeKit. Trailing comment names use the
same normalization, including domainless mailboxes. Encoded tabs retain
their decoded value. Phrase names are unquoted and unfolded first; trailing
comment names retain literal quotes, backslashes, and embedded newlines.
Direct phrase encoding, general string unquoting, interior whitespace in
programmatically assigned names, and untouched raw headers retain tabs.
Address constructors and the name setter trim only leading and trailing
ASCII linear whitespace (SP, HTAB, CR and LF), matching current MimeKit.
VT, FF, Unicode whitespace and interior whitespace are
preserved, and NULL remains distinct from an empty name.
Setting an equivalent normalized name does not emit a change event
or regenerate its owning header, preserving untouched raw folding.
Domainless local-part parsing now preserves the trailing comment boundary
so its display name is decoded rather than discarded; comments within a
dotted local-part continue to be skipped.
Transport encoding now wraps nonempty domainless mailboxes without display
names in angle brackets, following MimeKit. They remain parseable when
AllowAddressesWithoutDomain is disabled. Folding counts both brackets;
exact fits and oversized first tokens follow the same rules as other
mailboxes. Nonfolding encoded output uses the same framing, while display
output, empty addresses, named mailboxes, and untouched raw headers retain
their existing behavior.
Transport encoding now writes null or empty group names as an empty quoted
phrase (`"": ...;`), following MimeKit. Nested unnamed groups retain their
structure when parsed again, including when domainless mailbox parsing is
disabled. Folding counts both quotes without moving the empty name or group
punctuation to a new line. Display output keeps its existing representation
of unnamed groups; whitespace-only assigned names now normalize to empty.

Addresses and lists support deep cloning, value equality, and ordering.
Equality compares names and mailbox addresses case-sensitively, including
the distinction between NULL and empty names; groups compare ordered members
recursively. Charset choices and source routes do not affect equality.
List lookup and removal now use value equality instead of pointer identity.
Ordering compares names case-sensitively in UTF-8 byte order, then complete
mailbox addr-specs with case-sensitive local parts and IDN-encoded,
ASCII-lowercase domains. Domainless addresses compare as stored. This
intentionally differs from MimeKit's ordinal, case-insensitive ordering and
does not use locale collation or value equality. Equal-name groups compare
equal regardless of members. No display-string-based hash API is provided:
display strings contain routes that equality intentionally ignores.
`g_mime_internet_address_list_get_mailboxes()` returns a new owning,
depth-first snapshot of references to the original nested mailboxes, rather
than a lazy iterator. Mutating a returned mailbox still updates its owners.
List insertion accepts indices through Count; replacement requires an
existing index. Self-append and self-prepend copy the original membership.

Mailboxes expose local-part, domain, international-address detection, and
encoded/decoded addr-spec helpers. `get_local_part()`, `get_address(idn_encode)`,
and `get_domain(idn_encode)` return allocated strings. The latter two encode
the domain when TRUE and decode it when FALSE, without modifying the mailbox.
`get_addrspec()`, `get_raw_domain()`, and `get_route()` return borrowed values.
The stored mailbox field is named `addrspec`, matching its getter and setter.
The cached IDN address and its getter have been removed; use
`get_address(TRUE)` for an allocated IDN-encoded address.
Every mailbox owns a mutable `GMimeDomainList`, retaining obsolete source
routes during parsing and writing them inside angle brackets. Parsed and assigned mailbox
domains preserve their IDN representation: A-labels are not automatically
decoded, and Unicode domains are not automatically encoded. Value equality
uses these stored spellings, while ordering and recipient deduplication use
IDN-encoded, ASCII-lowercase domains. Explicit decoding recognizes A-label prefixes
case-insensitively and leaves domain literals unchanged. Route mutations
update owning headers. Domain collections preserve their assigned strings
and use case-sensitive lookup; formatting omits only empty entries and entries
containing solely ASCII linear whitespace (SP, HTAB, CR and LF). VT, FF and
Unicode whitespace are retained. Nonblank entries are not trimmed.
Source-route parsing likewise retains Unicode-whitespace-only domains accepted
by the domain grammar; VT and FF remain invalid domain characters.
Clones own independent routes and nested groups.

Mailbox construction now takes a trailing `GError **`, returning NULL on an
invalid addr-spec. `g_mime_mailbox_address_set_addrspec()` also takes `GError **`
and returns `gboolean`; failure preserves the old address.
`g_mime_message_add_mailbox()` and
`g_mime_autocrypt_header_set_address_from_string()` likewise take a trailing
`GError **` and return `gboolean` without mutating their owner on failure.
Pass NULL when error details are not needed, but check the return value for
untrusted input. Empty mailbox strings remain valid model values.

New single-address, mailbox, group, and domain-list parse functions, plus
`g_mime_internet_address_list_parse_full()`, accept a byte length and
`GError **`. A length of -1 selects a NUL-terminated string; nonnegative
lengths support nonterminated buffers and reject embedded NUL bytes. Typed
parsers require exactly one complete value of the requested kind; checked
list parsing does not silently discard invalid entries. Parse errors use
`GMIME_ERROR_PARSE_ERROR` and address/list errors identify a byte offset.
The older list parse/append APIs and message-header parsing remain
best-effort. Strict checked parsing requires comma separators and terminated
groups; loose parsing retains MimeKit's mailbox recovery rules. The new
`GMIME_RFC_COMPLIANCE_LOOSER` additionally permits consecutive/trailing
local-part dots and escaped at-signs in addr-spec parsing.

`g_mime_format_options_set_allow_international()` now enables UTF-8 address
names and domains, bypassing RFC 2047 and IDN encoding for addresses and
routes. Ordinary text headers now support International formatting as
described below; specialized headers keep their dedicated formatters.
Ordinary transport formatting uses IDN domains; display and International
output preserve the stored mailbox and route domain spellings. New
addresses use UTF-8 for names unless a charset is explicitly assigned;
parsed RFC 2047 names retain their detected charset. International address
folding counts UTF-16 units, following MimeKit, rather than UTF-8 bytes.
IDNA conversion uses libidn2 when available, otherwise GLib, rather
than .NET's IdnMapping. Mapping failures preserve the original domain,
unsafe mapped address delimiters are rejected, and decoded A-labels must
round-trip through IDNA validation. Backend-specific IDNA mapping and Unicode
case-table versions can differ from .NET; for example, libidn2 rejects a
snowman IDN that IdnMapping accepts, and newer GLib versions recognize case
pairs introduced after the Unicode version used by the .NET runtime.
These backend differences are not a promise of byte-for-byte .NET equivalence.

### General and structured header formatting

General text header folding now honors MaxLineLength and the configured
newline format, including existing newlines passed to the folding helpers.
Exact fits stay on the current line. The first token stays on the initial
line unless it is an unencoded token at least as long as the target; such
tokens are split repeatedly to fit, rather than only once. Tabs are preferred
folding points, and existing separator whitespace is retained. Unstructured
continuations use spaces; the legacy structured folding helpers retain tabs.
Encoded words are kept intact, including their charset/language labels and
encoding-tag casing. Folding preserves whitespace between encoded words
and does not synthesize whitespace between adjacent encoded words; decoding
still ignores inter-word whitespace as required. Untouched raw headers
retain their original folding outside International mode.
Unstructured whitespace runs now fold while consuming the run, including
between encoded words, matching MimeKit's precise space/tab placement.
Empty non-International text values emit only the terminating newline
after the colon; International formatting retains its initial space.

Private folding helpers accept field lengths, excluding the colon, instead
of copying raw field names into temporary strings just to measure them.
Helpers that emit the field name use a bounded pointer and length. Parsed
International formatting still measures the decoded field name without
trailing whitespace; assigned values use the complete raw field length.

RFC 2047 text and phrase encoding now honors `AllowMixedCharsets`, corresponding
to MimeKit's `AllowMixedHeaderCharsets`. It defaults to false: encoded words use
the requested charset, or UTF-8 when none is supplied, instead of automatically
choosing legacy charsets. Enabling it allows US-ASCII and ISO-8859-1 words
alongside the requested charset; other words still default to UTF-8. Selection
happens after word merging. Format-option clones retain the policy, and address
names and mailing-list comments share it. Lossless UTF-8 fallback is retained
when a requested charset cannot represent a word, rather than emitting .NET
best-fit or replacement characters. International output and
explicit raw assignments bypass this policy. These changes also apply to the
low-level RFC 2047 helpers; legacy automatic charset guessing is no longer used.

RFC 2047 phrase tokenization now recognizes quoted text, escapes, nested
comments, and completed group boundaries. Blanks inside groups stay with their
text, including encoded tabs. If a group or unseparated token is split, all
contiguous pieces use encoded words when any piece requires encoding, preserving
literal quotes and parentheses instead of adding syntactic quotes to individual
ASCII fragments. Long ASCII quoted groups still coalesce without forcing
RFC 2047 encoding merely because of their length. Unstructured text does not
interpret these phrase groups.

RFC 2047 word merging now follows MimeKit's two-pass algorithm and uses the
configured maximum line length instead of fixed raw UTF-8 length thresholds.
Estimates track charset byte counts, Q/Base64 costs, literal encoded-word
prefixes, and escaped quotes. Encoded words may absorb up to three following
atoms when another encoded or quoted word is nearby; quoted strings still
coalesce without a size limit. Custom-charset character estimates include
independent converter flushes and replacement costs, matching MimeKit's sizing
model, while emitted words retain lossless UTF-8 fallback. Adjacent encoded
words use tabs in phrases and spaces in unstructured text, with original
whitespace retained inside the encoded payload. These are size estimates;
text output also applies the final actual-size splitting described below.

RFC 2047 token splitting now collects those metrics incrementally and uses the
configured width and requested charset instead of fixed character-count limits.
Tokens split before the next Unicode scalar would reach the width-minus-one
boundary. Oversized ASCII atoms become encoded words with room reserved for
the wrapper, and unseparated pieces retain their encoded-word type. Stateful
encodings are measured with independent character flushes; supplementary
characters are never split. Charset estimates are collected before mixed-charset
selection, just like MimeKit, so Latin-1 costs, actual requested-charset bytes,
and lossless UTF-8 fallback can differ. Text encoding now performs MimeKit's
final actual-size check, halving oversized UTF-16 chunks until each encoded
word fits MIN(75, MaxLineLength - 1), without splitting Unicode scalars.
Phrase encoding retains its single-word emission for each estimated token.
At least one complete scalar is emitted even if its wrapper cannot fit.
Unsupported charset names still fall back losslessly, including names too
long to fit a wrapper; iconv
extensions that can discard input use the existing lossless conversion path
for measurement as well as emission.

RFC 2047 charset labels now reuse the alias and canonical-name machinery in
`gmime-charset.c`. The shared alias table carries MIME names separately from
platform-specific iconv names, and common ASCII/Latin aliases are recognized
there too. ISO names, Windows-125x code pages, and Japanese, Korean, and Chinese
aliases produce lowercase MIME labels. The same normalized charset is used for
conversion, splitting, and merging, so equivalent aliases no longer change
word boundaries just because their supplied names have different lengths.
Header and address charset settings are not rewritten; explicit raw headers
retain their original labels. Existing UTF-16/32 byte-order handling is unchanged.
This is not a replacement of iconv with .NET's encoding registry: native
converter availability and lossless fallback still apply. Big5-HKSCS retains
its distinct label and repertoire instead of being collapsed to Big5.

International formatting of ordinary text headers now follows MimeKit's
`Header.Fold()` path instead of RFC 2047 encoding. It emits UTF-8 regardless
of the requested charset, unfolds input line breaks, and folds at the last
separator character while retaining spaces and tabs. Folding counts UTF-16
units, not UTF-8 bytes; supplementary characters count as two units and
are never split. Words at least as long as the target cause the entire
value to use UTF-8 RFC 2047 encoding and ordinary text folding, following
MimeKit and avoiding inserted decoded whitespace. Shorter first words move
to a continuation if their field prefix leaves insufficient room. Whitespace
runs fold at the target, not one character beyond it. This is not a hard
UTF-8 byte-length limit. In decoded assignments, literal encoded-word-looking
text is written unchanged in International mode, as in MimeKit.

Parsed ordinary headers now follow MimeKit's International `GetRawValue()`
path too: RFC 2047 words and fallback-charset bytes are decoded to UTF-8,
then refolded with line widths based on the parsed field name, excluding
trailing field-name whitespace.
Address fields are reparsed from raw bytes, preserving encoded display-name
boundaries before decoding; valid Content-Type and Content-Disposition fields
are re-encoded through their checked models. Invalid structured fields retain
their raw values instead of becoming empty/default fields. Parsed trace,
message-id, References, notification-options, authentication-results, and
signature fields remain raw.

List-Archive/Help/Owner/Post/Subscribe/Unsubscribe now use mailing-list command
formatting for decoded assignments and parsed International serialization.
Angle-bracket URLs are kept intact, even when they exceed the line-length
target; a URL ending exactly at the target moves to a continuation. Repeated
commas and surrounding whitespace are compacted. Nested and escaped comments
are recognized, with RFC 2047 encoding outside International mode and UTF-8
text inside it. Oversized comments are folded on words and compact their
whitespace; oversized words are split using UTF-16 column counts without
breaking a Unicode character. Full field prefixes and oversized preceding URLs
cannot underflow the available width. Non-International comment encoding
reuses the shared charset-aware text encoder for encoded-word sizing, splitting,
and merging.

Explicit raw assignments bypass International reformatting; decoded value
assignments restore it. Clones retain this distinction, including after their
source list and parser options are released. Entity-generated raw updates are
explicit on both insertion and replacement. Serialization does not replace
stored raw values or emit change notifications. Non-International formatting
otherwise retains the existing formatting algorithms, with charset selection
controlled as described above.
Invalid UTF-8 is a programming error: failed assignments preserve existing
state and failed serialization returns -1
with `errno` set to `EINVAL` without writing a partial header.

References formatting now honors MaxLineLength, allowing lines exactly at
the configured width. Message-id tokens remain intact,
and the first token is never moved off the initial line. Received
formatting now follows MimeKit's clause tokenization: preliminary
comments, keyword/value pairs with attached comments, the semicolon,
and the complete date-time are folded as indivisible tokens. Inter-token
whitespace is normalized, while whitespace inside clauses and comments is
preserved. Unknown or uppercase clause keywords are handled like known
ones. Oversized tokens and the first token can exceed the target.
These rules apply to newly set or explicitly formatted header values;
untouched raw headers continue to preserve their original folding.
In-Reply-To now follows MimeKit's message-id formatter rather than being
parsed and folded as a References list. Its comments and other text are
preserved, and no new folds are introduced, even for oversized values.
Original-Message-Id uses the same formatter as Message-Id, Content-Id, and
Resent-Message-Id. Explicitly formatted message-id and Received values are
unfolded before formatting.

Disposition-Notification-Options now has a dedicated formatter following
MimeKit. It removes whitespace within notification parameters, preserves
their order and terminating semicolons, and folds only between parameters
using MaxLineLength and the selected newline format. The first parameter
is never moved off the initial line; oversized parameters remain intact
and may exceed the target. Newly set headers use this formatter
automatically, while untouched raw headers preserve their original text.

DKIM-Signature, ARC-Message-Signature, and ARC-Seal now share a dedicated
`g_mime_header_format_dkim_signature()` formatter, following MimeKit's
signature-tag folding. It removes tag whitespace, preserves tag order and
semicolons, and starts lowercase `b` and `bh` tags on continuation lines.
Recognition uses the original tag name: uppercase names and names containing
whitespace before `=` retain MimeKit's ordinary-tag behavior. Exact fits
remain on the current line; oversized first tags can move off the field line.

Long `h` and `z` tags fold between colon- and pipe-delimited entries,
respectively, retaining empty entries and never splitting an oversized list
entry. Other long tags are split using tab continuations. Signature values
are treated as ASCII: widths, list-entry lengths, and chunk boundaries count
bytes, without Unicode character handling. The requested charset is unused.
Invalid UTF-8 is still rejected without replacing existing header state.

The formatter applies to newly assigned or explicitly formatted values in
both ordinary and International modes. Parsed or explicitly assigned raw
signatures retain their original whitespace and folding, including when
cloned. This is formatting only: it does not add DKIM/ARC tag validation,
signing, or verification, or change the cryptography backend.

### Header construction, cloning and display

`g_mime_header_new()` creates a standalone header from a field name, decoded
UTF-8 value, and optional encoding charset, following MimeKit's field/value
constructors. A NULL charset selects UTF-8; explicit charsets use the existing
header encoder and its lossless UTF-8 fallback. Names preserve case and must
be nonempty printable ASCII excluding colon. Leading and trailing ASCII linear
whitespace is trimmed and CR/LF characters are removed from the cached value
before invoking the existing field-specific formatter. Empty values are valid;
NULL values, invalid UTF-8, and invalid names are GLib programming errors.

Constructed headers own their strings and an independent default parser-options
snapshot, so no header list is needed to manage their lifetime. Their stream
offset is unknown (-1). They support the existing setters, formatting, display,
serialization, and cloning APIs. This does not change the legacy list
append/prepend/insert normalization or the handling of parsed raw headers.
Field-specific formatting uses the implementations and explicit native
policies described above. The checked constructor and assignment
APIs below provide structured-value rejection without changing these legacy
entry points.

`g_mime_header_clone()` copies a header's current raw name and value,
cached decoded value, charset, and formatter without parsing or formatting
it again. Strings are independent, change listeners are not copied, and
the source stream offset is reset to unknown (-1), following MimeKit.
The clone owns a snapshot of the parser options so it can outlive the
source header list. Warning callback user data remains borrowed, as with
`g_mime_parser_options_clone()`; its lifetime must cover use of the clone.
This differs from MimeKit's shared, garbage-collected ParserOptions.

`g_mime_header_to_string()` produces the display form `Name: decoded value`
using the existing decoded-value getter. It preserves field-name case,
omits raw-name trailing whitespace, and adds no folding or final newline.
Unset placeholder headers display an empty value, but remain unset and
continue to emit no transport header until a value is assigned.
It does not replace the encoded transport output of
`g_mime_header_write_to_stream()` or `g_mime_header_list_to_string()`.
The existing header decoder and value normalization remain unchanged.

### Checked header construction and assignment

`g_mime_header_new_full()` and `g_mime_header_set_value_full()` port MimeKit's
checked field/value construction and `Header.SetValue()` formatting behavior.
Both validate decoded UTF-8 and the selected encoding before publishing state;
a NULL charset selects UTF-8. Charset aliases use the existing native charset
mapping. Unsupported or empty charset names and malformed UTF-8 produce
`G_CONVERT_ERROR`. A supported charset that cannot represent the actual text
still permits the existing header encoder's lossless UTF-8 fallback.

Values are ASCII-linear-whitespace-trimmed and unfolded before field-specific
formatting.
Content-Type and Content-Disposition use the existing checked parsers,
including their configured recovery rules, rather than the legacy fallback
parsers. Parse failures produce `GMIME_ERROR_PARSE_ERROR`. Parsed parameter
charsets take precedence over the requested header charset; parameters without
an explicit charset use the requested encoding, subject to the existing
parameter encoder's ASCII/Latin-1 selection. Other fields retain their existing
formatters. Formatter failures report errno through `GMIME_ERROR`, or
`GMIME_ERROR_GENERAL` if the formatter supplies no errno. Invalid field names,
NULL values, and an occupied error destination are GLib programming errors.

Failed assignment preserves the decoded cache, raw strings/bytes, charset,
formatting flags, source offset, and containing entity properties, and emits
no change event. This is deliberately stronger than MimeKit's exception path,
which can replace its decoded cache before structured formatting throws.
Successful assignment replaces the raw bytes and emits one change event
without changing the parsed source offset or repairing an invalid field name.
The formatted value is cached with the requested formatting options, rather
than being reformatted on every write. It is not an explicit raw assignment:
International output may still reformat it as UTF-8. Independent clones retain
this behavior. The source setter's formatting-cache comment is adapted to
the native cache; its disabled option assignments are not copied.

The actual-source differential covers 9,878 construction/assignment cases in
each normal and ASan/UBSan/leak build, with zero unexpected differences.
It includes Unicode normalization, invalid structured values, charset aliases
and failures, Cyrillic parameter encodings, formatting widths/newlines,
International output, and deterministic malformed-input fixtures. Remaining
differences are explicitly classified: 331 constructor platform-newline cases,
70 preserved RFC 2231 language declarations, 130 ASCII-only linear-whitespace
trimming cases, and 28 lossless UTF-8 fallbacks instead of best-fit mappings.
No prior-native-formatter comparison is accepted as a general exemption.

An additional 6,568 actual-source cases pass in each normal and
ASan/UBSan/leak build, comparing direct text/phrase/parameter formatting,
untouched raw headers, parsed header-list output, and complete message and
entity serialization. Both newline formats, International output, widths
60 through 998, supplementary scalars and charset variants are covered.
There are zero unexpected differences; explicit exceptions comprise 892
native word/physical-line bound cases, 92 lossless parameter-byte/conversion
cases, and 80 explicit-endian native parameter charset labels.

Eight focused native formatter suites pass normally. All formatter and
parameter-list checks also pass in isolation with ASan/UBSan/leak checking.
The complete header suite's sanitizer run retains a
648-byte/25-allocation address-sync fixture
leak, independently reproduced on unchanged HEAD. Unrelated core and parser
model assertion aborts were also reproduced on unchanged HEAD. GIR/typelib
and VAPI builds pass with the existing warning-callback scope annotation
warning. GTK-doc builds with documentation warnings. Native Windows
validation remains pending.

### Checked header-list field/value operations

`g_mime_header_list_append_full()`, `prepend_full()`, and `insert_full()`
port MimeKit's checked field/value `HeaderList.Add()` and `Insert()`
overloads. `g_mime_header_list_replace_value_full()` ports its field/value
`Replace()` overloads. Each constructs a checked header before invoking the
existing instance-based insertion or replacement path. The applicable source
lookup-table comment is retained and adapted in that shared insertion path.

These APIs accept decoded UTF-8, Unicode-trim and unfold it, validate the
selected charset, and propagate the checked constructor's errors, including
Content-Type/Disposition parse failures. NULL charset selects UTF-8. NULL
values and invalid names/indexes are programming errors, not unset-header
reservations. Empty values remain valid. Failures preserve all existing
objects, order, raw values, lookups and body-separator state without notifying
owners or collapsing duplicates.

New headers own independent default parser-options snapshots, matching
MimeKit's use of standalone Header constructors, rather than borrowing the
list's options. Their source offset is unknown (-1). Unrelated parsed headers
keep their raw folding and offsets. Replacement creates a new object at the
first case-insensitive match and removes all later matches, or appends when
absent. Retained references and other lists sharing removed headers are not
mutated. This is distinct from the legacy `g_mime_header_list_set()` operation
that changes the first object's value. Successful operations reuse the normal
owner notifications and the existing single-notification replacement policy.
Legacy field/value APIs, including placeholder support, remain unchanged.

An actual-source differential covers 12,400 cases per normal and
ASan/UBSan/leak build with zero unexpected differences. It compares accepted
values, error categories, field case, object identity, order, first/last
indexes and raw output across empty, matching-duplicate and absent-name lists.
Platform default newlines are normalized for comparison; 1,120 inherited
new-header formatter differences are independently checked against standalone
checked construction. Focused regressions also cover failure notifications
and separator state, borrowed arguments, repeated/shared instances, detached
header lifetimes, and message/recipient/content-model updates.

GIR/typelib, VAPI, GTK-doc and live GI/Python checks pass for all four APIs.
The full core suite remains at 34/35 passing programs, with only test-core's
two established text-format assertions. Native Windows validation remains
pending.

### Named header-list values

`g_mime_header_list_get_value()` and
`g_mime_header_list_set_value_full()` port MimeKit's named HeaderList indexer.
Lookup returns the first matching header's borrowed decoded value, or NULL
when absent. An unset legacy placeholder also returns NULL without falling
through to a later duplicate. Decoding remains lazy and does not notify owners.

Assignment updates the first matching object with the checked header setter,
or appends a checked header when absent. It preserves existing objects,
duplicates, raw field names, source offsets and parser options. Existing
invalid parsed fields can be updated without repairing their names; a new
field must pass the checked constructor's validation. NULL charset selects
UTF-8; the optional explicit charset follows the existing checked APIs.
Legacy `g_mime_header_list_set()` still collapses duplicates.

Changing an existing header preserves body-separator state; appending enables
the separator. Notifications flow through existing header subscriptions,
including subscriptions for repeated occurrences and other lists sharing the
same object. No redundant list event is added. All occurrences of a shared
object see the changed value, while distinct later duplicates stay untouched.
Failures preserve raw values, decoded caches, list state and owner models
without notifications. This intentionally retains the checked setter's
stronger rollback guarantee: MimeKit can change the first decoded cache
before structured formatting throws.

The source indexer bodies have no inline implementation comments to copy.
Normalization and formatting reuse the checked helpers and their adapted
source comments.

An actual-source differential covers 5,328 cases per normal and
ASan/UBSan/leak build with zero unexpected differences. It exercises the
named indexer and explicit-charset assignment across empty, absent-name,
duplicate and repeated-instance lists. It separately accounts for 1,854
intentional reference failure-cache rollbacks and independently verifies 224
inherited formatter differences against standalone checked construction.
Native regressions also cover lazy/raw state, legacy placeholders, borrowed
arguments, invalid parsed fields, detached shared owners, recipient updates
and structured content models.

Both APIs pass GIR/typelib, VAPI, GTK-doc and live GI/Python validation.
The full core baseline remains unchanged: 34/35 programs pass, with only
test-core's two established text-format assertions.

### Explicit header decoding charset

`g_mime_header_get_value_with_charset()` ports MimeKit's
`Header.GetValue(Encoding)` and `Header.GetValue(string)` through a native
charset-name API. It clones the header's parser options, overrides the
fallback charset, decodes the stored raw value and unfolds it with Unicode
whitespace trimming. Valid UTF-8 remains the first choice; supported
encoded-word declarations take precedence over the fallback. Unknown
declarations use the fallback rather than legacy native locale aliases.

Every successful call returns owned UTF-8 text, independently of an existing
decoded cache. Neither success nor failure changes raw bytes, caches,
metadata, formatting state, parser options or owner notifications.
An unset legacy placeholder returns owned empty text after validating the
charset. Empty, invalid or unsupported charset names report G_CONVERT_ERROR.

Literal and decoded Unicode NUL characters are replaced with U+FFFD before
unfolding or other C-string processing, preventing truncation. This policy
is shared with the ordinary header getter and text/phrase decoding helpers;
raw bytes remain unchanged. Replacement happens after charset conversion:
zero bytes in a multibyte encoded-word payload can legitimately decode to
ordinary non-NUL characters. The separate checked text-part getter retains
its existing embedded-NUL error contract.

Charset names reuse the checked text APIs' native aliases and iconv
repertoire, rather than replacing them with .NET's registry. Existing native
`utf16` and `utf32` aliases remain compatibility extensions; the historical
MimeKit `unicode` alias selects UTF-8. UTF-16/32 handling is compatibility
coverage, not a preferred email encoding.

The getter overloads contain no inline implementation comments. The shared
fallback decoder preserves its existing comments and includes MimeKit's
UTF-8-first explanation from CharsetUtils.ConvertToUnicode.

An actual-source differential covers 14,208 cases per normal and
ASan/UBSan/leak build with zero unexpected differences. It independently
checks 1,180 inherited decoder differences against the legacy native decoder
and Unicode normalization, and accounts for 384 intentional U+FFFD
replacement cases. The matrix includes 1,776 native-alias cases compared
against the reference's
canonical UTF-16/32 names; MimeKit rejects the abbreviated names themselves.
Lazy and populated caches, both RFC 2047 compliance modes, raw 8-bit text,
declared and unknown encoded words, invalid charset names and detached
header lifetimes are covered.

GIR/typelib, VAPI, GTK-doc and live GI/Python checks pass, including owned
results, charset errors, NUL replacement and preservation of raw serialized
bytes.

The full core baseline remains unchanged: 34/35 programs pass, with only
test-core's two established text-format assertions. Native Windows
validation remains pending.

### Raw header byte values

`g_mime_header_get_raw_value()` and
`g_mime_header_set_raw_value()` port MimeKit's byte-array RawValue
property and SetRawValue method using immutable GBytes. Lookup returns an
owned reference or snapshot with the complete raw value, including NULs and
non-UTF-8 bytes. It does not decode, populate the cache or notify owners.
An unset legacy placeholder returns NULL. Returned bytes remain valid after
reassignment or destruction of the header.

Assignment requires a nonempty value ending in LF, matching MimeKit's
validation; both LF and CRLF endings are accepted. Missing final LF reports
GMIME_ERROR_PARSE_ERROR before changing any state or notifying owners.
No other syntax, charset, leading whitespace or line-injection validation
is imposed by this low-level API. Valid assignments retain the immutable
bytes, clear the decoded cache, disable reformatting and emit the existing
change event, even when assigning the current bytes. Fields, offsets,
parser options, charsets and body-separator state remain unchanged.

Explicit raw values survive International formatting, including binary
values assigned to newly constructed headers without parsed field bytes.
Invalid parsed fields remain invalid and retain their original transport
field bytes; assignment does not repair them. Clones and shared headers
retain their existing lifetime and notification behavior.

These replace the former raw-string getter and setter; no public raw-value
accessor truncates at embedded NULs. Both decoded-value getters replace
literal and decoded NULs with U+FFFD rather than returning partial values.
The getter returns an owned GBytes reference; callers must unref it. The
setter takes GBytes and a GError destination and returns gboolean. The
temporary `_bytes`-suffixed names are not exposed alongside these APIs.
GBytes sharing replaces MimeKit's mutable array aliasing with native
immutable ownership. Neither source accessor contains inline implementation
comments to copy.

The header's `raw_name` and `raw_value` fields are the sole authoritative
raw storage, both GBytes; no duplicate C-string backing is maintained.
`g_mime_header_get_raw_name()` also returns an owned GBytes reference.
Clones share immutable raw storage, and duplicate-header diagnostics compare
the complete bytes rather than a prefix ending at the first NUL.

Message/partial splitting clones headers rather than copying a raw C-string
view, retaining binary values and raw field bytes in each resulting message.
Text-only native fixtures check the complete GBytes representation before
using test-only cached, terminated snapshots; binary fixtures use GBytes
directly.

An actual-source differential covers 11,832 cases per normal and
ASan/UBSan/leak build with zero differences and no codec exemptions.
It compares exact raw bytes, serialized bytes, cache invalidation, failures
and notifications for standalone, parsed and invalid fields, lazy/populated
caches, International formatting and both newline formats. Fixtures include
all byte values, embedded NULs, empty/unterminated inputs and deterministic
binary data. Native regressions additionally cover borrowed-byte
reassignment, retained snapshots, clones, legacy placeholders, owning
message caches and repeated/shared subscriptions.

Both APIs pass GIR/typelib, VAPI, GTK-doc and live GI/Python validation.
The full core baseline remains unchanged: 34/35 programs pass, with only
test-core's two established text-format assertions. Native Windows
validation remains pending.

The binary message-splitting regression also passes ASan/UBSan/leak
validation in isolation. The complete test-partial sanitizer run reports
leaks from its existing fixture loop, which retains reconstructed partials;
the normal complete test-partial run passes.

### Bounded raw header decoding

The private text and phrase decoders and their RFC 2047 tokenizers consume
explicit byte lengths, including bounded lookahead at incomplete encoded
words. Header getters pass GBytes spans directly without temporary C-string
copies. Public string-based utility signatures remain unchanged. Relevant
MimeKit tokenizer comments are preserved; warning callbacks receive a
temporary terminated copy because that callback API remains string-based.

After transfer decoding and charset conversion, Unicode NULs are replaced
with U+FFFD across literal ASCII, raw 8-bit text and declared or fallback
encoded-word charsets. Other existing malformed-byte replacement policies
remain unchanged. Content-transfer-encoding model updates preserve token
delimiters preceding literal NULs rather than unfolding them away.
The shared 8-bit scan also honors its full supplied span rather than stopping
at NUL or reading past the end of a nonterminated buffer.

Exact-size, nonterminated text/phrase validation covers 16,124 spans per
normal and ASan/UBSan/leak build with no differences from the prior native
token-recovery baseline. The 196 inherited MimeKit/native charset and
replacement differences remain unchanged. Additional regressions cover
empty spans, every encoded-word truncation, terminated warning callbacks,
literal and encoded NUL replacement, consecutive NULs, and preservation of
raw storage and serialized bytes.

Focused header, parser-model, address, parameter and binary-splitting
sanitizer checks are leak-clean. The complete test-core sanitizer run also
reports 435 bytes retained by its existing depth-test type registrations;
with leak detection disabled it has only the two established text-format
assertions and no ASan/UBSan diagnostics.

### Standalone header parsing

`g_mime_header_parse()` ports MimeKit's public `Header.TryParse()` byte-array
overloads as a bounded byte-span API. `g_mime_header_parse_text()` supplies
the UTF-8 string convenience overload. Missing field separators return
GMIME_ERROR_PARSE_ERROR with a relative byte offset; empty input is a parse
failure. Printable ASCII field text, optional trailing spaces/tabs, and
even empty field names follow the reference parser rather than the stricter
standalone header constructor's validation.

Raw field bytes retain case and trailing blanks, while the decoded name
excludes those blanks. Every byte after the colon belongs to the value,
including NULs and subsequent header-like lines. An existing final LF or
CRLF is preserved; otherwise, the default formatting newline is appended.
Relevant MimeKit inline comments, including the canonicalization rationale,
are preserved. Tolerant stream-reader parsing is unchanged.

Parsed headers own their raw bytes and a parser-options snapshot, start with
an unknown offset and no decoded-value cache, and use the existing bounded
decoders and U+FFFD policy when accessed. Persistent regressions cover
nonterminated input, all 256 field-byte classifications at two positions,
binary values, raw serialization, cloning, option lifetimes, UTF-8 text,
empty fields and error paths.

Normal and ASan/UBSan/leak validation each compare 9,704 exact-span
parsing/raw-storage/serialization cases against actual MimeKit source with
zero differences. The differential run aligns the native default newline
with the Windows reference's CRLF default; persistent native tests also
verify the normal LF default. Cases include arbitrary binary values,
every-prefix truncations and both LF/CRLF list serialization.
GIR, typelib, VAPI, GTK-doc and live GI/Python checks pass. Existing header
and parser-model tests pass; test-core retains only its two established
text-format failures. Focused parsing sanitizer checks are leak-clean.
Native Windows execution remains unvalidated.

### Linear-whitespace header unfolding

`g_mime_header_unfold()` ports MimeKit's public `Header.Unfold()` for
NUL-terminated UTF-8 text, restricting edge trimming to linear whitespace
(SP, HTAB, CR and LF), matching current MimeKit.
NULL and linear-whitespace-only inputs return an owned empty
string. VT, FF, Unicode whitespace and internal whitespace are retained,
and CR/LF characters are removed everywhere.
Encoded words are not decoded by this standalone display helper.
Invalid UTF-8 is a GLib programming error.

Standalone construction, checked assignment and explicit charset decoding
reuse this implementation instead of their private normalization helper.
The ordinary decoded-value getter uses the same unfolding. NBSP, NEL and
other Unicode whitespace remain at decoded boundaries, including
whitespace from encoded words. Non-whitespace characters such as BOM and
zero-width space also remain intact. MIME token normalization now uses
the same ASCII linear-whitespace classification.

Raw GBytes, parser options and notification behavior are unchanged, and
decoded NULs still become U+FFFD before unfolding. The legacy
`g_mime_utils_header_unfold()` remains a raw-byte utility with its existing
ASCII whitespace policy; it is not redirected through UTF-8 validation.

Persistent regressions cover all 625 pairs of Unicode whitespace
boundaries, getter caches, raw preservation, source/clone lifetimes,
checked construction and assignment, encoded-word whitespace, binary
values, NULL inputs and non-whitespace controls.
The earlier MimeKit differential covered the superseded Unicode-trimming
policy; current MimeKit now also uses ASCII linear whitespace.
Native Windows execution is unvalidated.

### Common header identifiers

GMimeHeaderId ports all 131 common fields from MimeKit's HeaderId, including
its numeric values, canonical field-name spelling and UNKNOWN = -1.
`g_mime_header_id_to_name()` matches ToHeaderName(), returning "Unknown" for
unknown or out-of-range identifiers. `g_mime_header_id_from_name()` matches
whole names ignoring ASCII case; it does not strip whitespace or colons.
Extension and unrecognized fields return UNKNOWN. The source documentation
warning distinguishing Auto-Submitted from legacy Autosubmitted is retained;
the source conversion methods contain no inline implementation comments.

`g_mime_header_get_id()` exposes the immutable identifier cached during
construction or parsing. Cloning preserves it; reading it does not decode
the value, change the original field spelling or alter raw GBytes. Existing
field-specific formatting, parser options, serialization and value setters
remain unchanged. The existing name-based APIs can use canonical names
from the identifier helper. Identifier-based list lookup, removal,
construction and checked value operations are described below.

Persistent regressions cover every common field, case variants, stream
and standalone parsing, cloning, binary values, raw trailing blanks,
checked/ordinary construction, value mutation and unknown identifiers.
Normal and ASan/UBSan/leak runs each compare 42,274 cases against actual
MimeKit sources with zero differences. The comparison checks that identifier
access preserves each implementation's existing decoded-value cache rather
than requiring their stream parsers to populate caches identically.
GIR, typelib, VAPI, GTK-doc and live GI/Python checks pass for every
identifier. The complete core suite passes 34 of 35 programs; test-core
retains only its two existing text-format failures. Focused sanitizer
checks are leak-clean. Native Windows execution remains unvalidated.

### Header-list identifier lookup and removal

The HeaderId overloads of MimeKit's HeaderList.Contains(), IndexOf(),
LastIndexOf(), getter, Remove() and RemoveAll() are available as
`g_mime_header_list_contains_id()`, `g_mime_header_list_index_of_id()`,
`g_mime_header_list_last_index_of_id()`,
`g_mime_header_list_get_value_by_id()`, `g_mime_header_list_remove_id()`
and `g_mime_header_list_remove_all_id()`. These source implementations
contain no inline comments; the existing native mutation-path comments
and ownership rules are retained.

Index searches compare immutable cached IDs without decoding values.
Contains, first-value access and removal reuse the canonical-name APIs.
Value access does not skip an unset first occurrence, change raw GBytes
or notify owners. Removal retains first-name lookup, surviving header
order, shared-instance subscriptions, separator state and existing owner
notifications. Bulk removal emits one notification per removed occurrence
in reverse order; absent removal is a no-op.

Unknown and out-of-range IDs are native programming errors, reported
through GLib guards without changing the list. MimeKit explicitly rejects
Unknown but does not consistently reject other invalid enum casts; the
native guards also reject those casts rather than accidentally matching
a literal "Unknown" field through the name-conversion fallback.

Persistent regressions cover all 131 known IDs, mixed-case duplicates,
binary values, lazy decoding, absent fields, unset placeholders, invalid
IDs, detached/shared/repeated instances and owning-message updates.
Normal and ASan/UBSan/leak runs each compare 36,453 cases against actual
MimeKit sources with zero unexpected differences, including raw bytes,
lookup results, separator state and removal notification order. Event
actions are compared by meaning, not their differing enum numbers; decoded
Unicode NULs are normalized to U+FFFD for the previously established native
display-string contract. Raw bytes are compared without normalization.
Focused sanitizer regressions are leak-clean. GIR, typelib, VAPI, GTK-doc
and live GI/Python checks pass for all six APIs and all known IDs. The
complete core suite passes 34 of 35 programs; test-core retains only its
two existing text-format failures. Native Windows execution is unvalidated.

### Header identifier construction and checked list values

MimeKit's HeaderId Header constructors and HeaderList.Add(), Insert(),
Replace() and indexer assignment overloads are available as
`g_mime_header_new_with_id()`, `g_mime_header_new_with_id_full()`,
`g_mime_header_list_append_id_full()`, `g_mime_header_list_insert_id_full()`,
`g_mime_header_list_replace_value_by_id_full()` and
`g_mime_header_list_set_value_by_id_full()`. These source constructors and
forwarding methods contain no inline implementation comments; the shared
native construction, formatting and mutation comments remain unchanged.

Each operation resolves the canonical field name and delegates to its
existing name-based implementation. New headers own independent default
parser options and have no source offset. Assignment changes only the
first matching header, retaining its object identity, original field
spelling, raw name, parser options, source offset and distinct duplicates.
Replacement installs a new canonical header and collapses duplicates.
Both append a canonical header if absent. Unrelated and detached headers
retain their raw bytes; shared headers retain the existing native owner
notification behavior. Replacement keeps the native single-notification
contract rather than adopting MimeKit's removal/addition notifications.

Checked operations report malformed structured values, invalid UTF-8 and
unsupported charsets without changing list/header state or notifying
owners. MimeKit's existing-header setter can change its decoded cache
before structured formatting throws; the native checked setter deliberately
retains its established atomic failure contract. The legacy constructor
continues to use existing unchecked construction behavior. Unknown and
out-of-range IDs remain programming errors. The native checked assignment
accepts an optional charset, consistent with its name-based counterpart;
MimeKit's HeaderId indexer uses UTF-8.

Persistent regressions cover all 131 IDs, canonical formatting, cloning,
mixed-case duplicates, raw binary survivors, identity and source metadata,
shared/repeated instances, owning-message updates, insertion bounds,
conversion/parse failures and programming-error guards. Normal and
ASan/UBSan/leak runs each compare 74,468 cases against actual MimeKit
sources and independently assert exact equality to the existing native
canonical-name path. There are zero unexpected differences: 18,435
inherited formatter differences are restricted to newly created or
explicitly updated headers, and eight cases retain the native atomic
failure behavior instead of MimeKit's changed decoded cache. Untouched
raw values are compared exactly; decoded Unicode NULs are normalized to
U+FFFD only for the established native display-string contract.

Focused sanitizer checks are leak-clean. GIR, typelib, VAPI, GTK-doc and
live GI/Python checks pass for all six APIs and all known IDs, including
error propagation and duplicate-preserving assignment versus replacement.
The complete core suite passes 34 of 35 programs; test-core retains only
its two existing text-format failures. Native Windows execution remains
unvalidated.

### Received clause model

`GMimeReceivedClause` ports MimeKit's public ReceivedClause constructors,
Id, Keyword, Value and ordered Comments collection. The corresponding
`GMimeReceivedClauseId` enum preserves its eight numeric values; NONE is
reserved for leading comments in the future Received parser and cannot
be produced by the public constructors. Known keywords ignore ASCII case,
but retain their original spelling. Extension keywords accept ASCII letters
and hyphens, without an initial hyphen.

`g_mime_received_clause_new()` and
`g_mime_received_clause_new_with_comment()` create validated clauses.
`get_id()` and `get_keyword()` are immutable; `get_value()` and `set_value()`
provide verbatim, nonempty UTF-8 values without trimming or normalization.
Invalid keywords or values report GMIME_ERROR_PARSE_ERROR, and failed
assignments preserve the previous value and comments. From/By use MimeKit's
permissive domain rules and IP literals, including legacy one-to-four-part
IPv4, octal/hexadecimal forms and IPv6 scope syntax. ID uses complete,
lenient message-id parsing rather than stricter message-id property
assignment. For validates one mailbox using the existing address parser.
Other clauses reject controls below 32, DEL, parentheses and semicolons.
The source inline comments explaining verbatim ID validation and mailbox
parsing are retained.

Comment APIs expose count, indexed access, exact case-sensitive lookup,
append, insert, replacement, first-match removal, indexed removal and
clearing. Strings are copied on assignment; getters borrow them. Setters
support aliases to existing strings. Comments may be empty and may contain
parentheses, backslashes or line breaks, matching the source collection.
Invalid comment UTF-8, NULL arguments and invalid indexes are programming
errors. Parsing, model mutation and comment serialization are described below.

Persistent standalone regressions cover ownership, aliases, validation,
atomic failure, comment ordering and programming-error guards. Normal and
ASan/UBSan/leak runs each compare 51,174 constructor, assignment and comment
mutation cases against actual MimeKit sources with zero differences.
GIR, typelib, VAPI, GTK-doc and live GI/Python checks pass for the enum and
all 15 APIs. Focused sanitizer checks are leak-clean. The complete core
suite passes 35 of 36 programs; test-core retains only its two existing
text-format failures. Native Windows execution remains unvalidated.

### Received header parsing

`GMimeReceived` ports MimeKit's Received parsing into a GObject with
ordered clauses, indexed borrowed clause access and an optional borrowed
GDateTime retaining its timezone offset. `g_mime_received_parse()` accepts
bounded raw bytes or a NUL-terminated string, copies its parsed state and
does not retain the input. NULL parser options use the repository defaults.
Empty input, missing values and missing dates are accepted. Parsed clauses
bypass the stricter public clause constructor validation, matching the
source's internal parsing constructor.

Leading comments form a NONE clause. Known keywords retain spelling and
ignore ASCII case; duplicates fail, while extension clauses may repeat.
Multi-token values collapse intervening whitespace to one space and stop
at comments, semicolons or the next known keyword. Unknown words within a
value are not automatically treated as extension keywords. Nested and
escaped comments are parsed iteratively, preserving inner parentheses,
removing quoting backslashes and CR/LF, and converting tabs to spaces.
Only space, tab, CR and LF count as whitespace. Applicable MimeKit inline
comments explaining these parsing and recovery stages are retained.

Nonempty dates after the first semicolon use the existing standard and
recovery date parser. Its tokenizer now takes an explicit byte length,
including bounded comment scanning, so embedded NULs do not truncate dates.
The public NUL-terminated date utility retains its existing contract.
Original date spelling is cached privately with CR/LF removed and tabs
changed to spaces for future serialization.

Keyword, value, comment and cached-date decoding reuse the existing raw
charset converter rather than RFC 2047 decoding. Decoded NULs become U+FFFD
for native UTF-8 display strings. Duplicate known clauses, incomplete
comments and invalid dates report GMIME_ERROR_PARSE_ERROR with input-relative
byte offsets. Partial allocations are released on failure.

Persistent standalone regressions cover recovery, ordering, retained
ownership, deeply nested comments, obsolete dates, fallback charsets,
nonterminated buffers, embedded NULs, exact error offsets and argument guards.
Normal and ASan/UBSan/leak runs each compare 30,418 cases against actual
MimeKit sources, including its Received fixtures and every byte prefix,
all byte values in token/comment/date positions and randomized inputs.
30,338 cases match exactly; the remaining 80 are Shift-JIS platform mapping
differences independently reproduced by the existing native and managed
charset converters. Clause structure, dates and error offsets match
throughout, with no unexplained parser differences. A separate comparison
of 14,609 existing date fixtures has zero differences.

Focused sanitizer checks are leak-clean. GIR, typelib, VAPI, GTK-doc and
live GI/Python checks pass for all four APIs, including bounded byte-array
input, errors and retained clause/date ownership. The complete core suite
passes 36 of 37 programs; test-core retains only its two existing text-format
failures. Native Windows execution remains unvalidated. Received construction,
mutation and serialization are described below.

### Received construction and mutation

`g_mime_received_new()` creates an empty Received value. Named From, By,
Via, With, Id and For getters return the first matching clause value.
Their checked setters update that instance without changing its keyword
spelling, comments or position; NULL removes only the first match.
Missing clauses use the validated public clause constructor, and invalid
assignments report GMIME_ERROR_PARSE_ERROR without changing the model.
Adding a missing named clause sorts the entire collection by identifier.

Sorting is deliberately stable, unlike MimeKit's current List.Sort().
Equal identifiers retain their previous order, including distinct extension
keywords, which all have the UNKNOWN identifier. Adding a different clause
therefore cannot change which duplicate is first or which instance subsequent
named setters update. Persistent regressions cover the three-clause duplicate
By case and all 33 equal-ID instances in a larger extension collection.

FromTcpInfo and ByTcpInfo expose the first comment of the first matching
clause. Setters replace all its comments with one unfolded copy, removing
CR/LF and changing tabs to spaces without changing other whitespace.
NULL clears comments without removing or creating clauses. Non-NULL comments,
including empty strings, can create an empty-valued clause through the
parser's internal constructor, retaining MimeKit's explanatory inline comment.
Copies precede comment clearing, so aliases to existing comments are safe.

Clause collection APIs append, insert, replace, locate, test membership,
remove the first instance, remove by index and clear. Entries retain their
exact GObject instance; duplicates and cross-model sharing are supported.
These operations do not validate or sort, and lookup uses pointer identity.
Replacement retains its incoming reference before releasing the old entry.
NULL entries are rejected as programming errors, following native collection
conventions rather than allowing MimeKit's nullable list state.

`g_mime_received_set_date_time()` retains the date and timezone offset or
clears it for NULL. Every assignment, including self-assignment, invalidates
the cached parsed date spelling. Clause mutations leave dates unchanged,
and date assignment leaves clauses unchanged.

Normal and ASan/UBSan/leak runs each compare 8,853 scenarios and 88,413
intermediate states against actual-source MimeKit mutations with only their
sort result replaced by stable ordering. All states match. Running the same
scenarios without this harness-only adaptation identifies 108 differing
states from unstable equal-ID ordering and subsequent first-match mutations;
the MimeKit source tree is unchanged. Checks cover validation failures,
aliases, parsed spelling and date caches, duplicate/shared identities,
all named properties and collection mutations.

Focused native regressions and sanitizer checks pass. GIR, typelib, VAPI,
GTK-doc and live GI/Python checks pass for all 26 construction/mutation APIs,
including nullability, errors and retained clause/date ownership. The complete
core suite passes 36 of 37 programs, with only test-core's two existing
text-format failures. Native Windows execution remains unvalidated.

### Received host construction and serialization

`g_mime_received_new_with_hosts()` creates validated From and By clauses
with bracketed IP address comments and retains the supplied date and offset.
Host strings and address text are copied without retaining the GInetAddress
objects. Invalid hosts report GMIME_ERROR_PARSE_ERROR and release the partial
model. IPv6 spelling matches MimeKit, including zero-run compression and
embedded IPv4 compatible, mapped, SIIT and ISATAP addresses, rather than
depending on platform address formatting. GInetAddress cannot carry a scope
ID; scoped comments can instead use the existing TCP-info setters.

`g_mime_received_to_string()` ports Received.ToString(FormatOptions).
It returns the header value only, including leading whitespace and the
terminating configured newline. NULL options select native defaults.
Folding starts after the nine-character "Received:" field name, counts
UTF-16 units and retains MimeKit's exact keyword/value/first-comment packing
thresholds. Continuation lines start with a tab. Individual tokens are not
split, so oversized tokens may exceed the configured target.

Comments drop CR/LF, replace tabs with spaces and escape backslashes without
mutating stored comments; parentheses remain literal like MimeKit.
The first escaped comment is reused for lookahead and output. Clauses keep
their order, keyword spelling and values. Parsed dates retain their cached
spelling until date assignment invalidates it; otherwise the existing date
formatter supplies the timestamp. Required UTC conversion failures return
NULL after the date helper's critical diagnostic and release the builder.
Applicable MimeKit packing, folding, comment and date-cache inline comments
are retained. This does not replace the separate raw Received header formatter
or introduce RFC 2047 or IDN conversion.

Persistent regressions cover construction, validation, retained ownership,
embedded IPv4 spelling, zero-run ties, comment line-break protection and
escaping, parsed date caches, exact folding boundaries, oversized tokens,
non-BMP UTF-16 column accounting, both newline modes and out-of-range date
conversion. Normal and ASan/UBSan/leak runs each compare 84,346 construction
and serialization cases against actual MimeKit sources with zero differences,
using the established NUL-to-U+FFFD native display-string adaptation.
Cases include Received fixtures and every byte prefix, byte-value and Unicode
inputs, all 256 IPv6 zero-word patterns, embedded IPv4 variants and randomized
models and addresses. MimeKit sources remain unchanged.

Focused native regressions and sanitizer checks pass. GIR, typelib, VAPI,
GTK-doc and live GI/Python checks pass for both new APIs, including ownership,
errors, date caching, Unicode folding and comment protection; existing clause,
parser and mutation binding checks also pass. GTK-doc retains the environment's
missing external GLib/GIO cross-reference warnings. The complete core suite
passes 36 of 37 programs, with only test-core's two existing text-format
failures. Native Windows execution remains unvalidated.

### Header anonymization

`GMimeAnonymizer` ports MimeKit's header masking and configurable preserved
field set, retaining applicable inline state-machine comments and the source
MIT attribution. Received fields retain keywords, comment structure and valid
dates; addresses and identifiers retain delimiters and escapes; potential
RFC 2047 words retain charset/encoding labels while masking payload bytes.
Content-Type and Content-Disposition retain their initial token and the
boundary, charset, delsp and format parameters. Malformed encoded-word and
parameter transitions follow MimeKit, including its incomplete-name behavior.
Date, MIME-Version and Content-Transfer-Encoding pass through by default;
Resent-Date and authentication/signature fields do not.

Preservation names are copied and matched without ASCII case sensitivity.
Duplicate additions retain the first spelling; the getter returns an owned,
independent, NULL-terminated snapshot in unspecified order. Native UTF-8
string arguments replace MimeKit's managed strings.

`g_mime_anonymizer_write_headers()` writes all fields in order, retaining
duplicates and ignoring ordinary hidden-header suppression. It preserves
binary raw values and field spelling, masks invalid fields with the existing
GMimeFilterAnonymize, skips unset native placeholders and adds no body separator
or missing final newline. Formatting uses the same private helper as ordinary
header serialization, including International handling and binary-NUL
protection. Configured newline conversion still applies to preserved fields.
The source headers are not mutated. Short writes are drained; zero writes,
formatting failures and destination write/flush errors propagate through
GError without closing the destination or flushing after a failed write.
Native filter completion additionally flushes the underlying destination to
match MimeKit's FilteredStream behavior.

This is a structure-preserving diagnostic transformation, not a guarantee
that sensitive information is removed. Preserved fields, dates, syntax,
escapes, whitespace and byte counts remain visible. Message/entity traversal
and body anonymization are described below. Async and cancellation remain
unported.

Normal and ASan/UBSan/leak comparisons each cover 58,153 cases against actual
MimeKit sources, including all 48 header NUnit fixtures, prefixes, every byte,
malformed transitions, binary values, loaded headers, incomplete EOF states,
preservation, International formatting and randomized input. There are no new
masking differences. Forty malformed EOF cases fail in the existing native
header loader before anonymization; an isolated committed-HEAD build reproduces
all forty. Ten International cases inherit ordinary native address formatting's
acceptance of malformed address text that MimeKit leaves unformatted. Masking
those exact native-formatted bytes matches actual MimeKit. The standalone
reference parser explicitly uses LF to match the established native default
rather than MimeKit's default CRLF. MimeKit sources remain unchanged.

Focused native tests and ASan/UBSan/leak checks for the anonymizer and parser
model pass, including short/zero writes, partial failure, valid/invalid-field
flush failures and source/destination ownership. GIR, typelib, VAPI, GTK-doc
and live GI/Python checks pass. Existing external documentation-link warnings
remain. The complete core suite passes 37 of 38 programs, with only test-core's
two established text-format failures. The existing test-headers program passes
with leak detection disabled; its 648-byte leak in 25 allocations from
test_address_sync is reproduced at committed HEAD. Native Windows execution
remains unvalidated.

### Message and MIME-body anonymization

`g_mime_anonymizer_write_message()` and
`g_mime_anonymizer_write_entity()` port MimeKit's message and entity overloads,
including applicable inline comments. An explicit stack retains multipart
boundaries and epilogues while embedded messages and message bodies are visited
as tails, avoiding recursive anonymization. Native GMimeMessage instances passed
to the entity API use full message serialization.

Message/body headers share ordinary serialization's offset-based merge, without
header-list insertion or notification side effects. Parsed missing separators
are preserved. As in MimeKit, constructed messages with body headers acquire a
missing MIME-Version; native constructed/parsed newline state supplies the
strict/loose distinction. Nested message newline options are scoped to their
multipart frame and restored for siblings.

Raw multipart delimiters, including trailing whitespace and original newlines,
remain untouched. Preambles and epilogues are masked and newline-normalized.
Truncated trees retain trailing delimiters without inventing closing markers.
Missing boundaries are read without the native getter's boundary-generation
side effects. Legacy string-only multipart text is adapted locally; setters
now supply folded raw bytes as described below. Legacy closing-boundary
suppression is treated as truncation for anonymization only. Raw
boundary-marker API migration remains separate.

Delivery-status groups and disposition-notification fields use header masking
and the same preservation set. Malformed status fields fall back to raw body
masking; native content I/O errors still propagate. Other leaf bodies,
including attachments, use normal transfer serialization before byte masking,
including base64 and quoted-printable transcoding. Destination
short writes are drained, write/read/format/flush failures propagate through
GError, and the destination remains open. Content is rewound by its ordinary
writer; stored bytes are not rewritten. MIME-Version insertion and lazy status
caches are the deliberate source-compatible mutations.

Normal and ASan/UBSan/leak comparisons each cover 12,464 additional cases
against actual MimeKit, including source fixtures, every prefix of truncated
messages, all byte values, both newline modes, preservation, generated messages,
constructed multipart text and transfer transcoding. Deep comparisons cover
1,000 multipart layers and 2,000 embedded-message or mixed layers. Larger
parser-driven stress runs were stopped after minutes without completing; those
depths remain unvalidated.

There are no new masking differences. Sixty-two malformed EOF inputs are
rejected before anonymization; an isolated committed-HEAD build reproduces
all sixty-two. The forty constructed-epilogue differences originally caused
by the legacy setter's missing final-newline folding are resolved by the
multipart text-folding port below. All 100 constructed multipart cases now
match actual MimeKit in normal and ASan/UBSan builds. MimeKit sources remain
unchanged.

Persistent native regressions cover a 2,000-layer mixed tree, raw/truncated and
missing delimiters, binary content, constructed MIME-Version insertion, legacy
multipart text, stream positioning, every destination failure offset through a
multipart body, content read errors and body-phase flush errors. Anonymizer and
parser-model tests pass with ASan/UBSan/leak detection. GIR, typelib, VAPI,
GTK-doc and live GI/Python checks pass for both new methods, including borrowed
inputs, nullable options and checked errors. Existing documentation-link
warnings remain.

The core suite remains at 37 of 38 programs passing, with only test-core's two
existing text-format assertions failing. The unchanged test-partial program
passes with leak detection disabled; its 357,418-byte leak in 2,643 allocations
is reproduced at committed HEAD with the same fixtures. Native Windows
execution remains unvalidated.

### Positional header-list operations

`g_mime_header_list_index_of()` and
`g_mime_header_list_last_index_of()` return the first and last matching
0-based field-name indexes, or -1 when absent, matching MimeKit's
`HeaderList.IndexOf(string)` and `LastIndexOf(string)`. Matching ignores
ASCII case, like existing GMime header-name lookup.

`g_mime_header_list_insert()` creates a header at any index from zero through
the current count, preserving duplicate fields and existing header order.
Inserting at the count appends; inserting at zero prepends. Name lookup
continues to identify the first occurrence, including when insertion precedes
an existing duplicate. Inserted headers use the list's parser options and
the normal formatting and owner-notification paths. Existing raw headers
are not reformatted. As with append/prepend, a NULL value reserves an unset
header that does not serialize until assigned a value. Invalid indexes are
programming errors reported through GLib's argument guards without mutation.
This is the field/value overload; use `g_mime_header_list_insert_header()`
to insert an existing instance.

`g_mime_header_list_add()` and `g_mime_header_list_insert_header()` port
MimeKit's existing-Header overloads. They retain the exact object without
reformatting or resetting its stream offset. Each occurrence holds a reference,
including repeated occurrences in one list, and a header can be shared between
lists. Field-name lookup always identifies the first matching occurrence,
including after removing a later occurrence of the same object.
MimeKit's current later-occurrence removal can instead lose or advance its
name-table entry despite an earlier match remaining; GMime intentionally
preserves first-match lookup in that case.

`g_mime_header_list_index_of_header()`, `contains_header()`, and
`remove_header()` compare object identity, not field name or value.
Identity lookup returns the first index or -1; identity removal removes only
the first occurrence and returns FALSE when absent. Existing field-name
operations retain their separate semantics.

Shared headers notify all containing lists, and removing or destroying one
list detaches only that list's listeners. A header that originally borrowed
parser options takes an owned snapshot on first instance insertion; this
preserves its source configuration without depending on the source list's
lifetime. Later changes to that list's parser options do not affect the
snapshot. Raw text, cached values, charset, and formatter are unchanged.
Use `g_mime_header_clone()` before insertion when independent edits are needed.
Existing list set operations still collapse duplicate field names, including
multiple occurrences of the same object, to their first occurrence.

`g_mime_header_list_replace()` ports `HeaderList.Replace(Header)`: it retains
the supplied instance at the first case-insensitive field-name match, removes
all further matches, or appends when absent. Unlike `g_mime_header_list_set()`,
it replaces the object rather than changing the existing object's value.
Unrelated fields retain their positions relative to one another. The supplied
header can be the first match, a later match borrowed from this list, or an
instance shared with another list; its raw text, charset, formatter, and stream
offset are preserved, with the same parser-options snapshot rules as insertion.
Removed occurrences detach only this list's listeners.

Replacement emits one normal GMime change notification after the final list
and name lookup are established, rather than MimeKit's removed/added pair.
This lets owner caches rebuild once without treating a replacement at the tail
as an additional appended address field. The absent-name path uses the normal
add notification. Repeated occurrences of the original first instance are
collapsed too; MimeKit's current replacement loop can stop at a later
occurrence of that same object and leave earlier duplicates in the list.

`g_mime_header_list_set_header_at()` ports the setter of `HeaderList[int]`.
It replaces one existing slot without appending, changing the count, or
collapsing duplicates. Assigning the same instance to its current slot is a
no-op, including parser-option ownership and notifications. Other assignments
retain the supplied instance with the same ownership and raw-formatting rules
as insertion. Invalid indexes are reported by GLib's argument guards without
mutation.

The final array and first-name lookups are established before owner
notifications. Matching field names emit one change; differing names emit
removal of the original and addition of the replacement, matching MimeKit's
notification policy. Replacing a later occurrence of a repeated instance
still preserves the earliest surviving name lookup. MimeKit's current setter
can instead advance or remove that lookup despite an earlier match remaining.

`g_mime_header_list_remove_all()` ports MimeKit's `HeaderList.RemoveAll(string)`,
removing all case-insensitive field-name matches rather than only the first.
Missing names are a no-op. Remaining header objects, order, and raw values
are unchanged; removed placeholders are discarded too. Names borrowed from
a header in the list can safely be passed to the operation. Removals proceed
from last to first using GMime's normal per-header notifications and listener
detachment. GMime keeps name lookup consistent with the remaining list
throughout removal; MimeKit removes its name-table entry before notifying
individual removals. This does not introduce a new bulk-notification event
or change existing message-property cache semantics.

`g_mime_parser_construct_headers()` ports synchronous
`MimeParser.ParseHeaders()`, and `g_mime_header_list_load()` ports the stream
overloads of `HeaderList.Load()`. `g_mime_header_list_load_file()` ports its
filename overloads, using the platform filename encoding and GMime's existing
filesystem stream. The file is closed on both success and failure; the
returned list does not retain the file. File-open failures use the `GMIME_ERROR`
domain with a positive errno code, as do input I/O failures.

All three retain message and content headers in their original order without
constructing a MIME entity or its body. Raw field names, folding, duplicate
fields, and stream offsets are preserved. Loaded lists own their strings and
an independent parser-options snapshot, so they remain usable after the
parser, source options, and stream are released.

Empty input returns an empty list, unlike the entity/message construction
APIs' NULL-at-EOF contract. A terminating blank line is consumed but is not
part of the list. The parser API can read successive header blocks from its
buffer and updates the header-block offsets for each call. All three APIs use
the existing parser's raw-header limits and fatal syntax/I/O error reporting;
failed parsers must be reinitialized before reuse.

The stream APIs do not rewind or close their input. Parsing stops logically
after the separator, but the input stream may have been read ahead. Use the
parser API and `g_mime_parser_tell()` rather than the temporary-parser load
convenience when the logical position or subsequent buffered input is needed.

`g_mime_header_list_write_to_stream()` now ports synchronous
`HeaderList.WriteTo()`, including the terminating blank line. New lists have a
separator; parsed lists retain whether a blank line was actually consumed,
including empty input, clean EOF after headers, and truncated child parts
ending at a MIME boundary. Successful structural edits restore the separator:
insertion, replacement, removal, and clearing (even an already empty list).
Missing-header removals and indexed self-assignment do not change the state.
Editing a contained header's value alone does not restore it. The existing
value-based `set()` APIs retain their identity semantics, restoring the
separator when adding a field or removing duplicates, but not for a value-only
update to a single existing field.

This intentionally changes the GMime 4 list writer's previous headers-only
contract. Part, multipart, embedded-message, and message writers now account
for the separator exactly once. Messages with a body use the body's header
list state when writing their merged headers. Header-list `to_string()` and
entity/message `get_headers()` remain headers-only. Newline filtering is
completed before writing the separator, and returned header byte counts
reflect normalized output, including on nonseekable streams. Write failures
release the temporary filter; partial writes are completed, and zero-progress
writes fail with EIO. Header-string helpers return NULL rather than partial
strings on a formatting failure. The caller's stream is not closed or flushed.
Existing hidden-header filtering is retained, although MimeKit's standalone
list writer does not apply `HiddenHeaders`. GMime continues to normalize
multipart delimiter newlines rather than preserving the parsed opening
delimiter's newline; delimiter preservation remains separate parity work.

These APIs retain the existing scanner's syntax policies: unlike the current
MimeKit reader, GMime rejects an initial empty field name and accepts field
names spanning the reference reader's fixed-size input buffer, subject to the
configured raw-header limit. They do not introduce a separate header reader
or change existing entity/message parsing.

### RFC 2047 encoding and decoding

The RFC 2047 encoder interface and its Base64 and quoted-printable
implementations are private GObject types, matching MimeKit's internal
IRfc2047Encoder contract. Each call independently encodes an entire
encoded-word payload, without wrapping, framing, or a terminating NUL.
There is no clone, reset, or flush. The encoding tags are lowercase b/q;
Base64 includes padding and reuses the existing SIMD/scalar primitives.
Q encoding has separate phrase/text safe sets, maps spaces to underscores,
and uses uppercase escapes. Literal equals signs are now escaped in phrase
mode rather than being passed through. In text mode, backslashes are now
escaped too, matching MimeKit and avoiding quoted-pair interpretation
inside comments and quoted strings. The existing header encoder uses these
payload codecs. Encoded-word conversion now uses explicit byte lengths,
preserving embedded zero bytes in UTF-16 and UTF-32. Generic UTF-16/32 use
BOM-free little-endian bytes like MimeKit; explicit little-endian aliases
use the corresponding generic framing label. Explicit big-endian variants
retain their byte order and labels. A requested charset is tried directly,
falling back to UTF-8 if unavailable, unrepresentable, or not lossless on
round-trip conversion. Parameter encoding shares this lossless conversion
helper without changing its existing charset selection or BOM policy.
ISO-2022-JP encoded words always use Base64. Other words use Base64 when at
least 17 percent of the converted bytes are non-ASCII, matching MimeKit's
boundary. Charset alias canonicalization, word splitting, encoded-word merging,
mixed-charset policy, and folding of the resulting tokens are described above.
Phrase encoding now merges a quoted string with subsequent atoms and quoted
strings regardless of their combined length, following MimeKit. Embedded
quotes and backslashes remain escaped, separator whitespace is retained,
and encoded words remain separate from quoted strings. Encoded-word
lookahead can include nearby atoms before a quoted string without including
the quoted string itself; encoded-word runs use the shared charset-aware sizing.
Oversized quoted mailbox and group names stay intact and can exceed the
configured folding target. Individual oversized-word classification uses
incremental charset-aware estimates and the configured width as described above.
Text and phrase encoding now protects literal "=?" prefixes, following
MimeKit. Already encoded-looking strings supplied as decoded header text
are encoded as literal text, not interpreted as existing encoded words.
This also applies within a word, next to Unicode text, and across the
existing long-word splits. Raw-header setters and folding helpers still
accept pre-encoded content without introducing another encoding layer.
Header payload decoding now uses GMimeBase64Decoder and the RFC 2047 mode
of GMimeQuotedPrintableDecoder. Matching encoded-word runs retain decoder
state and combine raw octets before charset conversion; other tokens,
charsets, and encodings end the run and discard incomplete codec state.
Temporary buffers use checked decoder estimates with gsize capacities.
This intentionally adopts MimeKit's Q recovery: malformed complete
triplets pass through literally without reinterpreting nested equals or
underscores, and soft line breaks are removed. Base64 malformed-padding
recovery now follows the new decoder too.

Header tokenization now follows MimeKit's charset/language validation and
malformed-word recovery. In loose mode an unterminated encoded word stops
before a later plausible encoded-word start rather than consuming its
terminator. Invalid charset or encoding prefixes are rescanned as ordinary
text, preserving whitespace and allowing later valid words to decode.
Charset and language names must contain ASCII atom characters, with GMime's
existing allowance for dotted charset names retained for native aliases such
as ANSI_X3.4-1968. Invalid language syntax is not silently discarded.
Phrase parsing separates ASCII atoms from adjacent raw UTF-8 in both modes;
strict text parsing still requires whole whitespace-delimited tokens.
Trailing phrase whitespace does not create an empty token or phantom folded
continuation. Empty payloads and empty language suffixes remain accepted.
Folding uses the original encoded-word spelling, and tolerated payload
whitespace or phrase specials still produce the existing parser warning.
Charset-name storage uses the heap rather than an input-sized stack allocation.
Charset conversion is unchanged, including GMime's question-mark substitution
for malformed bytes rather than .NET's Unicode replacement character.

Decoded mailbox/group names and RFC 2047 parameter values now retain the
most frequent encoded-word charset, following MimeKit, instead of preferring
UTF-8 whenever it occurs. Each word counts once, including empty payloads;
payload length, language suffixes, and transfer encoding do not affect the
count. Ties select the charset encountered first. Alias counts share the
existing MIME charset canonicalization, while the retained converter name
and byte conversion remain native. Unknown charsets retain their native
identities rather than sharing .NET's unknown-codepage sentinel. With no
valid encoded words, the existing NULL charset result is preserved.
Parameter encoding-method detection is unchanged.

Adjacent encoded words now retain decoder state across equivalent native
charset aliases. Each token resolves a cached numeric codepage identity in
gmime-charset.c; decoder runs and charset frequency counts use integers
instead of allocating and comparing canonical names. Cache misses reuse the
existing MIME charset canonicalization. Known identities use codepage
numbers; otherwise unmapped native charsets receive distinct negative
identities scoped to the initialized charset maps. This recovers multibyte
characters, quoted-printable triplets, and Base64 quanta split across labels
such as euc-jp/ujis or windows-1251/cp1251. Conversion still uses the first
word's native converter.
GB2312, GBK, and EUC-CN share identity 936, preserving GMime's existing
GB2312-to-GBK compatibility mapping.
Different charset identities, transfer encodings, and intervening plain
text still end the run and discard incomplete codec state. Raw encoded-word
spelling and the selected charset metadata are unchanged.
Native-only aliases such as eucJP remain supported, and distinct unknown
charset names retain separate runs rather than sharing .NET's unknown-codepage
sentinel.

### Message-ID parsing and reference recovery

Message-ID and Content-ID decoding now follows MimeKit's tolerant msg-id
grammar instead of falling back to arbitrary text on a failed addr-spec.
Angle brackets are optional for a single ID, but a token starting with `<`
must close with `>`. Empty and incomplete IDs return NULL. Domainless IDs,
empty domains, unusual local-part punctuation, redundant initial `@` signs,
multiple domains, and Exchange-style square brackets are recovered as in
MimeKit. Quoted local-parts retain their quoting and escapes while removing
folded CR/LF characters. International local-parts and domain atoms must be
valid UTF-8.

Address and message-id parsing share the already ported domain parser,
including comments, domain literals, and trailing-dot normalization; address
behavior is unchanged. References parsing skips stray punctuation and resumes
after a malformed ID rather than abandoning the list. Quoted text and comments
are skipped without extracting apparent IDs inside them; an unterminated
quoted string or comment ends enumeration. Parsed message/content ID caches
use the recovered values without rewriting raw headers.

Message-ID and Content-ID property assignment now uses MimeKit's complete
msg-id validation rather than blindly adding angle brackets. Values may be
bare or bracketed and retain the tolerant grammar used by parsed IDs.
Only outer linear whitespace (SP, HTAB, CR and LF) is trimmed, and valid
folding line breaks are unfolded. Other whitespace remains subject to
the message-id grammar rather than being discarded. Empty or incomplete
IDs, extra tokens, non-folding line breaks,
invalid UTF-8, and unquoted spaces/tabs in bare IDs are rejected through
GLib's programming-error guards without changing the cache or raw header.
As with other native string APIs, inputs are NUL-terminated, not counted.
Reassigning the current cached ID preserves its raw header; aliased inputs
are safe. Message-ID remains non-nullable. A NULL Content-ID clears the
cache and removes all matching headers, including malformed duplicates.
Header, address-name, and ID setters trim only ASCII linear whitespace,
matching current MimeKit rather than .NET's broader whitespace classification.

GMimeMessage now exposes In-Reply-To and Resent-Message-Id accessors.
Both setters reuse complete msg-id assignment validation, normalization,
alias safety, and raw-header preservation on exact reassignment.
In-Reply-To accepts NULL to remove every matching header, including
malformed duplicates; Resent-Message-Id rejects NULL. Raw In-Reply-To
values use the shared References recovery scanner and stop at the first
recovered ID rather than accepting a bare addr-spec. Resent-Message-Id
uses tolerant single-ID parsing. Both caches track the first matching
header, including duplicate insertion, editing, replacement, removal,
and list clearing, without rewriting parsed raw headers.

GMimeMessageIdList replaces the boxed GMimeReferences model with a
reference-counted GObject and MimeKit's mutable collection operations.
Add, insert, and replacement validate and normalize complete IDs.
Bulk addition validates all inputs before mutation, snapshots aliases,
and notifies the owner once, including for an empty range. Replacing an
entry with its exact or normalized current value is a no-op. Search and
removal compare exact, case-sensitive IDs without normalization.
Clones own independent strings and do not retain owner callbacks.
String conversion uses space-separated bracketed IDs without folding.

GMimeMessage owns a persistent References list. Header edits refresh it
from the first matching raw header without changing its identity or raw
bytes; later duplicates do not override it. List changes serialize the
References header using MimeKit's default folding and UTF-16 column counts,
removing duplicate headers. Clearing the list removes all matching headers,
even if it was already empty. Header removal and clearing reset the same
list. Retaining the list beyond its message's lifetime is safe and detaches
the message callback. The existing reference-header formatter retains its
current byte-counting and formatting-options behavior.

GMimeMessage now exposes independent Importance, Priority, and X-Priority
properties through GMimeMessageImportance, GMimeMessagePriority, and
GMimeXMessagePriority. Missing or unrecognized headers yield normal values.
Importance and Priority compare decoded text case-insensitively after
trimming ASCII linear whitespace. X-Priority reads an unsigned Int32 decimal
prefix after ASCII folding whitespace, clamps valid numbers to 1 through 5,
and treats overflow or missing digits as normal. It does not skip comments,
decode encoded words, or accept signs.

Raw header values now decode encoded words before unfolding, following
MimeKit's Header.Value order. This avoids repairing encoded-word syntax by
removing newlines before decoding, and unfolds decoded newlines as well.
The existing native decoder still permits dots in charset names, so its
recognition of malformed encoded words can differ from MimeKit's.

These caches follow the first matching header through insertion, editing,
replacement, removal, and clearing. As with other native message caches,
updates are eager rather than MimeKit's lazy loading. Setters preserve raw
headers when assigning the current value, including absent normal values.
Changed values write canonical tokens or numeric labels and remove duplicate
headers. Invalid enum values are programming errors without mutation.

GMimeMessage now exposes Resent-Date through GDateTime get/set accessors,
independently of Date. Both date caches follow the first matching raw
header through insertion, editing, replacement, removal, and clearing.
Malformed first headers yield NULL without falling back to duplicates,
and date parsing does not decode RFC 2047 encoded words. Missing dates
use native NULL rather than DateTimeOffset's minimum-value sentinel.
Updates are eager, consistent with the other native message properties.

Date and Resent-Date setters retain the supplied immutable GDateTime,
preserving its timezone and microseconds in the cache while formatting
the header to whole seconds. Assigning the same instant, even with a
different timezone, preserves the existing cached value, raw header, and
duplicates. Changed values replace duplicate headers. NULL is a programming
error; unrepresentable UTC conversions leave the cache and headers unchanged.
Getters return borrowed references, which callers may retain independently.
Raw-header edits replace the cached assignment with the parsed header value.

GMimeMessage now exposes persistent Resent-From, Resent-Reply-To, Resent-To,
Resent-Cc, Resent-Bcc, and Resent-Sender address lists and corresponding GMimeAddressType
values. Existing enum values are unchanged. Each list combines all matching
headers in order and retains its identity through raw-header mutations,
positional insertion, replacement, removal, and header clearing. Parsing
uses the message's parser options and preserves raw bytes. Cache updates
are eager rather than MimeKit's lazy loading.

Address-list and nested address mutations serialize through the existing
native formatter, replacing duplicate headers. Emptying a message address
list now removes all matching headers instead of leaving an empty header;
this also applies to the existing ordinary address lists. Retained lists
are detached safely when the message is finalized. Generic address access
and mailbox addition accept the new enum values. The existing
get_all_recipients() continues to concatenate ordinary To, Cc, and Bcc.

Sender and ResentSender deliberately differ from MimeKit's single-mailbox
properties. Both get_sender() and get_resent_sender() return borrowed,
non-NULL, persistent GMimeInternetAddressList instances, retaining group
syntax as well as mailboxes. RFC 6854 permits groups in Sender and
Resent-Sender; its single-address and group-cardinality restrictions are
not enforced by these permissive lists.

Edit the lists in place, just like From or To. There are no mailbox setters
or single-mailbox caches. get_addresses(GMIME_ADDRESS_TYPE_SENDER) and
get_addresses(GMIME_ADDRESS_TYPE_RESENT_SENDER) return the same lists as the
typed getters. GMIME_ADDRESS_TYPE_RESENT_SENDER is appended without
renumbering existing enum values, and add_mailbox() accepts it.

Both lists combine all matching headers in order, rather than selecting
only a mailbox prefix from the first header. Missing or entirely malformed
headers yield empty lists; later valid duplicates still contribute
addresses. Reads preserve original raw headers. List edits, group edits,
and nested mailbox edits serialize automatically; clear() removes all
matching headers, including empty or malformed duplicates. Retained lists
and members remain usable after message finalization.

get_recipients(only_unique) now supplies MimeKit-style recipient selection
without changing the legacy get_all_recipients() contract. It returns a
new, non-NULL mailbox-only list, flattening groups in depth-first To, Cc,
then Bcc order. Any nonempty ResentSender or ResentFrom address list selects
only Resent-To, Resent-Cc, and Resent-Bcc, even when those recipient lists
are empty. An empty group in either originator list counts as an address
and activates resent selection. Empty originator lists do not select
resent recipients merely because their recipient headers exist. This
list-based ResentSender selection intentionally differs from MimeKit.

Optional deduplication retains the first mailbox for each canonical
addrspec, preserving local-part case and IDN-encoding and ASCII-lowercasing
the domain. Domainless addresses compare as stored. Names and source routes
are ignored. This intentionally differs from MimeKit's ordinal
case-insensitive deduplication: differently cased local parts remain distinct,
while equivalent Unicode and punycode domains can match. Returned
lists are independent membership snapshots retaining original mailbox
objects, so editing a shared mailbox still updates its original message
address list, while adding or removing snapshot entries does not.

### Text-part formats

GMimeTextFormat and g_mime_text_part_new_with_format() now mirror MimeKit's
text-format values, including the Text alias for Plain. Flowed construction
uses text/plain with format=flowed; both RichText and CompressedRichText
construct text/rtf. The latter does not compress content, and classification
reports RichText rather than CompressedRichText.

GMimeTextPart exposes get_format() and is_plain(), is_flowed(), is_html(),
is_enriched(), and is_rich_text(). These inspect the current Content-Type
without modifying headers or decoding content. Predicates ignore ASCII case;
enriched includes text/richtext, and RTF includes application/rtf. Flowed
text remains plain text and trims surrounding ASCII linear whitespace from the
format parameter before comparison.

Classification preserves MimeKit's fallback to Plain for unrecognized
media types and subtypes; this does not make is_plain() return TRUE.
It also preserves the reference's case-sensitive "plain" subtype check
in Format: text/PLAIN; format=flowed reports Plain, although is_flowed()
returns TRUE. Existing text-content APIs and legacy body guessing are
unchanged.

### Multipart text-body selection

g_mime_multipart_try_get_value(format, &body) now implements MimeKit's
multipart text-part selection. It returns a borrowed original GMimeTextPart,
or FALSE with NULL when there is no match. It does not decode content or
unflow text. Plain also matches flowed text; CompressedRichText and unknown
format values do not match. Text-like media types on an ordinary GMimePart
do not make it a GMimeTextPart.

The base multipart searches forwards, skipping non-text entities and
attached text parts. It stops at the first non-attachment text part even
if the format differs, and at the first multipart even if its recursive
selection fails. Attached multipart containers are still searched.
Embedded message bodies are not entered.

GMimeMultipartAlternative is now a specialized GObject and the default
factory/parser type for multipart/alternative. Its override searches
children backwards, continuing past mismatched text parts and unsuccessful
nested multipart searches. Like MimeKit, direct text alternatives may be
attachments; nested containers use their own selection rules.

Selection is virtual and depends on the object's class, not its current
Content-Type string. An explicit g_mime_multipart_new_with_subtype("alternative")
remains a base multipart with forwards selection, just as MimeKit's
new Multipart("alternative") does. Changing a specialized alternative's
subtype does not remove its reverse selection behavior. Existing collection
operations, serialization, and legacy message-body guessing are unchanged.
Decoded text-body helpers and flowed-text conversion remain separate work.

### Multipart related roots

GMimeMultipartRelated is now a specialized GObject and the default
factory/parser type for multipart/related. get_root() returns a borrowed
immediate child, selecting the first referenced ID in the start parameter
with case-sensitive Content-Id comparison. Like MimeKit, start matching
uses the escaped CID path: query and fragment are excluded, unreserved
percent escapes are decoded, and authority paths normalize dot segments.
If start is absent, the first
child whose MIME type matches the type parameter is selected, ignoring
ASCII case. An unmatched start does not fall through to type. Missing or
unmatched selectors fall back to the first child; an empty container
returns NULL. Getters preserve raw headers and do not cache selections.

set_root() replaces an explicitly selected child. If neither selector
matches, it inserts the new root first rather than replacing the getter's
implicit first-child fallback. The type parameter follows the new root's
MIME type. A first-position root removes start without generating an ID;
a later root gets a generated Content-Id if needed and a bracketed start.
As in MimeKit, assigning an existing child does not deduplicate other
occurrences. The container retains the root.

Text-body selection examines only the root. A text root is eligible even
when marked as an attachment; a multipart root delegates to its own virtual
selection rules. Sibling resources and embedded message bodies are not
searched. Selection follows the object's class after subtype changes.
Explicit generic multipart construction with subtype "related" keeps
base multipart behavior.

### Multipart related resources

g_mime_multipart_related_index_of_uri() and contains_uri() search immediate
children by URI string. The _uri suffix distinguishes these operations
from inherited multipart collection methods taking entity objects. CID
scheme matching ignores ASCII case; the escaped path matches Content-Id
case-sensitively, with the same path normalization as root selection.
Relative queries compare literal relative Content-Location strings.
Other absolute queries resolve relative locations against the child's
Content-Base, or the container's Content-Base if the child has none.
Content-Location on the container is not a base. The first matching
child wins, without descending into nested containers or messages.

Absolute comparisons use GLib URI normalization: scheme/host comparisons
ignore ASCII case, default ports and dot segments are normalized, and
unreserved percent escapes are decoded. User information and fragments
are ignored; paths and queries remain case-sensitive. Unlike System.Uri,
GLib normalizes the hexadecimal case of reserved percent escapes and
normalizes dot segments in opaque paths. Relative queries retain literal
string comparison. Stored header spelling is never rewritten by lookup.

g_mime_multipart_related_open() returns an owned, lazy decoded GMimeStream
and optional allocated MIME-type and charset strings (free with g_free()).
The charset may be NULL. A missing match, non-leaf match, or leaf without
content reports GMIME_ERROR with ENOENT; a content object without a backing
stream reports GMIME_ERROR_INVALID_OPERATION. Invalid query strings report
GMIME_ERROR_PARSE_ERROR; GLib parsing/resolution errors propagate in their
native domain. Rewind errors report GMIME_ERROR with errno, or
GMIME_ERROR_GENERAL if the source did not set errno. Metadata outputs are
NULL on failure. Opening never skips an unreadable first match.

The reader retains its source after the multipart, child, and content are
released. Decoding, shared backing-stream position, closing behavior, and
subsequent read failures follow g_mime_content_open(). Neither lookup nor
opening accesses external network or filesystem resources.

### Entity content URIs

Content-Base and Content-Location are now GMimeEntity properties, available
on leaf parts, multiparts, and embedded-message entities. The public APIs
use nullable borrowed UTF-8 strings rather than introducing a URI object.
Content-Base requires an absolute URI; Content-Location also accepts
relative references, including an empty string. The existing GMimePart
location accessors delegate to the entity APIs. Direct access to the old
GMimePart.content_location field must migrate to these accessors.

Getters lazily read the first matching decoded header and trim surrounding
ASCII linear whitespace (SP, HTAB, CR and LF), not VT, FF or Unicode
whitespace. Generic header decoding preserves this non-linear padding.
Remaining whitespace is subject to the existing URI validation rules.
Invalid first values return NULL without
trying duplicates.
Reads preserve raw headers; matching edits, insertions, removals, indexed
replacement, and clearing invalidate the cache. Setters replace duplicate
headers or remove all matches for NULL. Assigning the current cached
string is a no-op, including NULL after reading a malformed value; NULL
before the first read removes malformed headers.

URI validation uses GLib's URI parser, rejecting malformed percent escapes,
invalid UTF-8, unescaped ASCII whitespace/controls, and unsafe delimiters.
Invalid setter arguments are programming errors and leave headers unchanged.
Unlike MimeKit's System.Uri properties, strings retain their exact URI
spelling: no scheme/host canonicalization, dot-segment removal, default-port
elision, or URI-equivalence comparison is performed. GLib's generic URI
syntax also differs from .NET's scheme-specific well-formedness rules:
for example, fragment-only relative references and opaque HTTP-scheme
references are accepted. These are native string-API differences, not a
port of System.Uri. Getters do not resolve relative locations against bases.

### Body parts and attachments

GMimeEntity now exposes is_attachment() and set_is_attachment() for all
MIME entities, including multiparts and message parts. Attachment status
depends only on Content-Disposition matching "attachment", ignoring ASCII
case; filenames do not imply attachment status. Setting TRUE creates or
changes the disposition to "attachment" only when needed. Setting FALSE
changes an attachment to "inline", but leaves missing and custom
dispositions untouched. Changes retain parameters; no-op assignments
preserve raw spelling and duplicate headers. The existing
g_mime_part_is_attachment() delegates to the entity-level predicate.

GMimeMessage now exposes get_body_parts() and get_attachments() as owned
GPtrArray snapshots of retained GMimeEntity instances. Free each array
with g_ptr_array_unref(). Elements are borrowed from their array; retain
them separately to outlive it. Introspection transfers the container,
which owns the element references. Unlike MimeKit's lazy enumerables, array
membership is fixed at the time of the call, but the contained entities
are shared, not cloned. They remain valid after removal from the MIME
tree or message finalization.

Both accessors traverse in depth-first order, exclude multipart
containers, and treat message/rfc822 and other message-part objects as
leaves without entering their embedded messages. Attachment enumeration
filters these same leaves by their current Content-Disposition. Marking a
multipart as an attachment neither returns that container nor suppresses
traversal of its children. An absent body or empty multipart returns an
empty, non-NULL array. The legacy body-guessing APIs are unchanged.

try_get_entity(path_specifier, &entity) now provides direct MimeKit-style
MIME-path lookup without enumerating siblings. It returns TRUE with a
borrowed original entity, or FALSE with NULL for malformed or missing paths.
Multipart children are numbered from 1; a non-multipart message body is
part 1. A multipart message body is part 0, but its children's paths omit
that 0. Lookup enters attached messages: "3.0" selects the multipart body
of the message in part 3, and "3.1" its first child. A 0 component is valid
only for a multipart message body and only at the end of the path.
Components accept ASCII digits and leading zeros, without signs or
whitespace, and reject values above 2147483647. Navigation is iterative.
GMimeIterator uses the same path conventions; it replaces GMimePartIter.

### Packed entity state

The private loaded-state flags in GMimeEntity and GMimePart use unsigned
one-bit fields. GMimeEntity groups its newline flag with the loaded flags
after its pointers to avoid wasting space on alignment padding. Lazy cache
loading, invalidation, and no-op assignments retain their existing behavior.
These instance layout changes require rebuilding the library and all
consumers, including subclasses and generated bindings.

### Content duration

GMimePart now exposes get_content_duration() and set_content_duration(),
porting MimeKit's nullable Int32 ContentDuration property. As with the
native Content-Disposition size API, -1 represents absence and removes
all matching headers when assigned; values below -1 are programming errors.
Nonnegative values are durations in seconds, including zero.

The getter parses only the first decoded Content-Duration header, accepting
ASCII decimal digits with surrounding ASCII linear whitespace (SP, HTAB, CR
and LF) and leading zeros up to 2147483647. VT, FF and Unicode whitespace in
the decoded value are not skipped. Generic header decoding preserves this
non-linear padding. Signs, comments, fractions, and overflow are invalid.
A malformed first header does not fall back to later duplicates. Reads
preserve raw bytes; matching header edits, including later duplicates, invalidate the
lazy cache. This applies to GMimeTextPart as well as ordinary GMimePart.

Changed nonnegative assignments write canonical decimal text and remove
later matching duplicate headers. Assigning -1 removes all matching headers.
Assigning the already cached value preserves original raw headers and
duplicates, matching MimeKit. Assigning -1 before the first read removes
malformed headers; assigning it after a getter returned -1 is a no-op
until matching headers change. Header clearing resets the lazy cache.

### Content description

GMimePart's Content-Description property uses MimeKit's lazy
first-header lookup with ASCII linear whitespace trimming. Reads preserve raw
headers, and matching header edits, removals, additions, or clearing
invalidate the cache. Later duplicates do not override the first header.
The getter returns a borrowed UTF-8 string, or NULL for an absent header;
values empty after trimming return an empty string.

The setter accepts NULL to remove all matching headers and trims ASCII
linear whitespace from non-NULL assignments. VT, FF and Unicode whitespace
are preserved. An assignment
equal to the current cached value preserves raw headers and duplicates;
before the first read, assignment replaces duplicates even when the first
value is already equal.
Comparison occurs before trimming, matching MimeKit. Borrowed values,
including substrings of the getter's value, can safely be assigned back.

### Explicit text charset assignment

GMimeTextPart now exposes set_text_with_charset(), porting MimeKit's
TextPart.SetText(charset, text) overload. It encodes UTF-8 input into the
requested charset, updates the Content-Type charset to its MIME name,
and stores unencoded content bytes without selecting or changing the
Content-Transfer-Encoding. Empty text creates present, empty content.
Newlines are preserved and Unicode encoders do not add a BOM; generic
UTF-16/32 aliases produce little-endian bytes.
The historical MimeKit alias `unicode` selects UTF-8, not UTF-16.

The checked native API reports unsupported or invalid charset names,
invalid UTF-8, and unrepresentable text through G_CONVERT_ERROR, leaving
the part unchanged. Charset names must be MIME tokens; iconv modifiers
are rejected. Managed replacement fallbacks and custom encoders are not
supported, consistent with BodyBuilder's existing checked conversion
policy. BodyBuilder now uses the same setter and charset normalization,
but preserves `unicode` as UTF-16 when resolving its Encoding-object name.
Charset repertoires follow the platform's iconv implementation and can
differ from managed code-page mappings. The legacy automatic-charset
set_text() behavior remains unchanged.

### Checked leaf-part transport preparation

GMimePart now exposes prepare() and prepare_with_max_line_length(), porting
MimeKit's MimePart.Prepare() methods. Existing base64, quoted-printable and
uuencode encodings are retained without reading content; binary is retained
when the constraint permits it. Other cases use the existing checked
best-encoding analysis. Unspecified transfer encoding stays unspecified when
7bit is sufficient, preserving absent or unknown raw headers and duplicates.

The line limit defaults to 78 and must be between 60 and 998, including on
fast paths. A selected encoding records that limit for subsequent transcoding,
capped at the codec's MIME maximum. A later safe-encoding fast path preserves
the previous limit. Source bytes, stored content encoding and unrelated headers
are unchanged. Conversion-analysis failures report GError and preserve both
the transfer headers and previous encoder limit.

This leaf policy also covers TextPart subclasses. The legacy entity encode()
policy and encoder defaults for unprepared parts remain unchanged.

### Recursive transport preparation

GMimeEntity now exposes prepare() and prepare_with_max_line_length(), with
a prepare_with_max_line_length virtual shared by parts, multiparts, messages and
encapsulated message parts. The existing part convenience APIs dispatch
through this virtual too. Multipart children are prepared in order, and
message parts delegate to their encapsulated message without rewriting
the outer part's transfer headers.

Messages add MIME-Version: 1.0 before preparing a body that has headers if
the first MIME-Version is absent or invalid. Valid raw versions, including
comments and duplicate headers, remain untouched. Parsing follows MimeKit's
two-to-four-component, nonnegative Int32 version grammar. Signed multipart
children are not visited: changing already-signed headers or encodings
would invalidate the signature.

Line-limit validation precedes virtual dispatch, header updates and child
I/O, including for empty and signed containers. Constraint validation stays
with the leaf policy rather than rejecting empty containers or safe
encodings early. Errors stop traversal immediately, retaining earlier
child changes and MIME-Version updates, as in MimeKit; whole-tree
preparation is not transactional. A failing leaf still preserves its
transfer headers and previous encoder limit. Subclasses without a prepare
implementation report NOT_SUPPORTED instead of silently succeeding.

The added Entity class virtual changes class layout; rebuild subclasses
and consumers against the updated headers.

### Explicit text charset decoding

GMimeTextPart now exposes get_text_with_charset(), porting MimeKit's
TextPart.GetText(charset) overload. The supplied charset overrides the
Content-Type parameter without modifying headers or content. Decoded UTF-8
text uses the default formatting newline convention without adding a
final newline. Bare CR characters and explicit Unicode BOM characters
are preserved; generic UTF-16/32 aliases decode as little-endian.
As with assignment, the MimeKit alias `unicode` selects UTF-8.
Absent content returns an empty string, but unsupported charsets still
fail before content is read.

This checked native API rejects malformed byte sequences and embedded
NUL characters with G_CONVERT_ERROR rather than returning replaced,
truncated, or partial text. Managed decoder fallbacks are not supported;
charset repertoires follow native iconv. Missing backing streams and
stream failures produce explicit GMIME_ERROR errors. Successful content
copying rewinds the source, while I/O failures leave its current position
intact. Content decoding/copy overrides are honored.

The legacy get_text() API retains its existing charset fallback, newline,
missing-content, and error behavior.

### Content-Type charset encoding

GMimeContentType now exposes get_charset_encoding() and the checked
set_charset_encoding(), porting MimeKit's ContentType.CharsetEncoding
property as an owned, resolved charset name rather than a managed Encoding
object. The existing get/set_charset() accessors remain raw parameter
operations: they do not require a supported charset or normalize its name.

The encoding getter returns NULL without an error for absent, empty,
invalid, or unsupported names. Other converter failures report
G_CONVERT_ERROR, unlike the reference's catch-all resolution. The setter
resolves and validates before changing the parameter or any owning headers;
NULL removes the parameter. Failed assignments are atomic. Resolution
does not rewrite original headers or parameter metadata, and returned names
are independent of later edits. Matching canonical assignments retain raw
headers, while changed assignments notify their owning entities normally.
Borrowed parameter names may safely be assigned back.

The implementation shares the existing text-charset name resolver, retaining
its MimeKit alias and endian comments. "unicode" means UTF-8, and generic
UTF-16/32 use the existing little-endian text policy. MIME-token validation,
available encodings, alias recognition, and canonical spellings follow
the established native charset helpers and iconv, not System.Text.Encoding.
Custom managed encodings and their fallback policies are not represented.
Native converter creation preserves errno so unsupported names remain
distinguishable from operational converter failures.

Validation compares 2,047 alias, case, whitespace, invalid-token, modifier,
codepage, and unsupported-name cases against the actual MimeKit sources.
Normal and ASan/UBSan runs agree: 1,903 match exactly, and 144 cases across
23 case-insensitive names differ. All native results also reproduce the
committed native text-name resolver and converter availability. The inherited
differences comprise additional native UTF-16/32 aliases and ISO encodings,
missing x-mac-icelandic/cp65001 support, and native codepage, endian, Thai,
and Big5-HKSCS names. Distinct native repertoires are not collapsed merely
to reproduce managed charset labels.

Native regressions cover checked resolution, canonical assignment, independent
ownership, borrowed-name assignment, raw-header/metadata preservation,
atomic errors, NULL removal, and owning-entity header notifications.
The new test-content-type, test-charsets, and test-anonymizer programs pass
with fatal GLib criticals, ASan, UBSan, and leak detection. Fault injection
checks ENOMEM, EMFILE, unsupported-conversion EINVAL, and failures without
errno in both builds, including omitted GError outputs. GIR/typelib, VAPI,
GTK-doc, and live Python introspection checks cover the new methods,
nullable names, ownership, errors, and raw-header behavior.

The normal core baseline is 38/39 programs passing, retaining the two known
text-format assertions in test-core. test-mime-part passes normally; its
1,168-byte, five-allocation sanitizer fixture leak reproduces in the prior
baseline and is unchanged. Native Windows execution is not validated.

### Byte-order-mark charset detection

g_mime_charset_try_get_bom_encoding() and its _from_stream() variant port
MimeKit's CharsetUtils.TryGetBomEncoding() helpers. They recognize only
initial UTF-16LE, UTF-16BE and UTF-8 marks, returning static, explicit-endian
charset names rather than managed Encoding objects. Incomplete marks do
not match. UTF-32BE is not recognized, and UTF-32LE's FF FE prefix selects
UTF-16LE, matching the reference's two-byte-first policy. These helpers
neither remove the mark nor decode or normalize text.

The stream overload makes exactly one read of up to three bytes at the
current position, without seeking, resetting or closing the stream. Short
reads are not filled; a one-byte read cannot identify even a UTF-16 mark.
Both matches and nonmatches consume the returned bytes. Zero-byte reads
mean no match, while read failures return a GError rather than masquerading
as missing marks. Detection works on nonseekable streams.

These primitives do not change the legacy TextPart.get_text() behavior.
Automatic charset selection, HTML meta-charset detection and the checked
TextPart.TryDetectEncoding()/GetText(out Encoding) APIs remain unported.

### Reader header foundation and opt-in OpenPGP detection

GMimeReader begins the direct port of MimeKit's synchronous MimeReader.
Its header-only operation uses its own 128-byte read-ahead area and
4096-byte block buffer, not the legacy recursive GMimeParser. It preserves
folds, complete wire bytes, malformed trailing headers, stream offsets and
1-based line numbers. It fills short reads when lookahead requires them,
without seeking, resetting or closing the input stream.

The GObject virtual callbacks are headers_begin(), header_read(),
body_separator() and headers_end(). For C, header_read() exposes a borrowed
byte array plus field-name/colon lengths and an invalid flag instead of
allocating a managed Header. This also preserves embedded NULs without
forcing wire data through GMimeHeader's NUL-terminated string interface.
Callbacks must copy retained bytes, return a boolean and optionally set
GError on failure. The reader retains itself during callbacks and makes
failures sticky until set_stream(). Stream and options changes, reentrant
reading and underlying stream manipulation are not allowed during callbacks.
Options are cloned; warning callback user data remains borrowed.

Header-only reads do not seek mailbox markers or strip a UTF-8 BOM.
Empty input succeeds with headers_begin() only. A bare final CR is treated
as a separator. An invalid leading From-line completes the empty
headers_end() notification before failing, matching MimeReader's
header-only operation. Header-length limits include folded lines and
terminators and are checked before growing the header buffer.

The entity, message and synchronous-completion increments below extend this
foundation with mailbox traversal, compliance reporting and SIMD scanning.
The reader-backed parser adapter, loading integration and async operations
remain separate work.

GMimeParserOptions.detect_openpgp defaults to FALSE, as selected for GMime 4.
The legacy parser now scans leaf wire content for inline OpenPGP armor only
when explicitly enabled, preserving its previous state-machine behavior.
Disabling detection leaves the part's OpenPGP cache unknown rather than
claiming no OpenPGP data exists. Explicit Part.get_openpgp_data() and the
OpenPGP filter still inspect decoded content on demand, including Base64
and uuencoded data. Enabling detection does not decode transfer encodings
while parsing. The setting is cloned with parser options and captured for
each part/message construction, including mailbox reuse.

### Reader explicit-stack entity traversal

GMimeReader.read_entity() ports the synchronous entity path from the actual
MimeKit MimeReader.cs and MimeBoundary.cs, retaining their MIT attribution.
It consumes an entity through end of input using the reader's own buffer,
without invoking GMimeParser or building a MIME object tree. Heap-allocated
frames replace recursive descent for multipart and unencoded embedded
message containers. MaxMimeDepth uses the source's strict depth comparison:
containers at the limit become opaque leaves. Transfer-encoded message
parts also remain leaves; multipart encoding does not change its structure.

The new entity_begin()/entity_end() virtuals use GMimeReaderEntityType to
coalesce MimeKit's separate leaf, multipart and message-part callbacks.
content_begin()/content_read()/content_end() use GMimeReaderContentType to
distinguish leaf wire content, preamble and epilogue. Message envelopes
inside message parts have message_begin()/message_end() callbacks around
their root entities. Headers precede entity/message begin callbacks.
Borrowed content types must not be modified during callbacks. Raw data
chunks have unspecified sizes, preserve binary bytes, and are never
transfer-decoded. The content end offset measures delivered bytes, while
entity end offsets are clamped to the entity's start; entity line counts
describe content rather than headers.

Delimiter recognition checks the innermost active boundary first, then
ancestors. An ancestor delimiter unwinds truncated descendants without
consuming the ancestor's delimiter. Missing closing delimiters and missing
body separators are tolerated in nested entities. Preambles keep their
trailing newline. Leaf content and epilogues withhold a possible trailing
CR/LF across refills, discarding it only when a delimiter is recognized.
At EOF it is delivered as content. Opening delimiter callbacks include the
delimiter's newline; closing delimiter callbacks leave that newline for
the epilogue. Neither includes the preceding content newline.

The scalar scanner uses bounded memchr() calls and fixed-size content
staging, not unbounded line/body buffering. Possible incomplete delimiters
are held for lookahead; incomplete lines of at least 1000 bytes are treated
as body data, as in the reference. Boundary-like bytes in the middle of a
long line are not recognized. Header-only behavior is unchanged. Callback
and I/O errors remain sticky, stop notifications immediately, and release
all active frame/type/boundary references. set_stream() discards the failed
operation's state without resetting or closing either stream.

Line positions and the last two consumed bytes are retained independently
of buffer movement. In contrast to the source's short-read-sensitive
lookbehind in malformed/empty nested header blocks, end offsets therefore
consistently exclude the newline before a boundary even after a refill.
An immediate embedded-message delimiter is fully probed before deciding
that the message is absent; a short read does not synthesize an empty
message/root pair. The inherited eos() contract still describes successful
operations, including when an empty entity read fails.

Validation covers native callback order, every callback-abort point, I/O
failure at every byte, stream replacement after failure, binary/long-line
streaming, 2048 embedded messages and 512 truncated multiparts unwound by
an ancestor delimiter. The actual-source harness checks 25,776 entity
cases and verifies delivered wire ranges and entity/message end geometry
for every successful case. 25,248 have identical same-chunk traces. The
remaining 528 are explicitly classified below, not unexplained allowances.
Each of the 100 short-read exceptions must also match the full-buffer
actual-source trace (with only the checked CRLF offset correction).

The original 552 exceptions included a genuine porting bug in 24 cases
using an invalid 1001-byte boundary. The native scanner refilled between
already-buffered complete lines. This could establish EOF before checking
an oversized incomplete delimiter and recognize it as a closing boundary.
MimeReader.ScanContent() scans the buffered batch first, consuming that
incomplete line as content under SmtpMaxLineLength before its next refill.
The native scanner now does the same; all 24 cases have exact same-chunk
trace parity. There is no oversized-delimiter exemption in the harness.
test_entity_oversized_delimiter() checks EOF and newline-terminated variants
at four large read sizes.

The remaining deliberate behavioral differences are reference correctness
fixes or the existing native operation contract, not C ownership changes:

* 380 cases: only entity/message end and clamped header-end offsets differ.
  MimeReader.GetEndOffset() consults input[index - 1/-2], while header reads
  use ReadAhead(..., save: 0); a refill can discard that lookbehind. The
  reference reports an end 1 or 2 bytes too far forward across the actual
  preceding LF/CRLF. Native end offsets consistently exclude that newline.
  Every other event field, including line counts and all bytes, is equal.
* 80 cases: MessagePartBegin() performs one ReadAhead(), treats the buffer
  sentinel as an end-of-line and misses an incomplete immediate delimiter.
  Nested header parsing later finds it, but the reference has already
  emitted an empty headers/message/root sequence. Native lookahead waits
  for the actual line end or EOF and omits that nonexistent message.
* 12 cases: the same sentinel probe mistakes an incomplete closing marker
  for its opening prefix (e.g. "--b" from "--b--\n"). The reference emits
  an opening delimiter, terminates early and leaves input unread; native
  emits the closing delimiter and consumes its newline as epilogue.
* 2 cases: the sentinel probe recognizes a closing marker ending at a
  buffered CR. SkipBoundaryMarkerInternal() lacks the actual LF, consumes
  the CR into the closing delimiter and leaves only LF for the epilogue.
  Native boundary callbacks exclude both CR and LF, delivering CRLF to the
  epilogue, matching the source method's own newline-ownership comment.
* 4 cases: stale CR lookbehind in GetEndOffset() makes the reference end
  one byte too early, excluding a non-newline byte from entity/message
  metadata (not from raw callbacks). Native metadata follows actual
  consumed bytes. These match the
  full-buffer reference, not the broken short-read metadata.
* 1 case: the same stale CR lookbehind removes an extra LF (not a CR),
  shortening entity/message end metadata by one byte and reporting 2
  enclosing message-part lines rather than the native/full-buffer 1.
* 1 case: lost LF lookbehind makes an empty descendant's reference end one
  byte too far forward; its enclosing message-part line count is 1 rather
  than 0. Native reports the actual empty end and zero lines.
* 48 failed-empty cases: both reads fail at the same position with the same
  headers-begin callback and no entity. MimeReader.IsEndOfStream reflects
  its internal Eos state and returns true; g_mime_reader_eos() retains the
  header-reader contract of successful completion and returns false after
  this sticky parse failure. set_stream() is required for recovery.

validate-reader-entity.py now checks exact category counts, the specific
short-read event mechanisms, all delivered byte ranges, and independent
end/header-end geometry inferred from subsequent delimiter callbacks or
EOF. It emits reader-entity-exceptions-{full,sanitized}.json with each case's
input protocol, index, same-chunk reference, full-buffer reference and
native trace. reader-entity-edge-audit.json retains the original 552
before-fix differences; reader-entity-edge-resolution.json records their
current outcomes. These persistent artifacts and the actual-source
reader-reference project are in the session's files/gmime-port directory.
Native test_entity_traversal() also checks short-read/chunk invariance for
immediate absent messages, split CRLF and stale-lookbehind fixtures.
All 54,768 existing header-only differential cases still pass. The entity
and header checks also run under ASan/UBSan; generated GIR/typelib and
compiled Vala callbacks cover the new enums and binary callback arrays.

The following increments add top-level ReadMessage, BOM stripping,
newline-format reporting, mailbox traversal, compliance and SIMD scanning.
The private reader-backed header/leaf model foundation is described below.
Async operations and full model loading remain separate work. The legacy
parser remains in use for public model loading and opt-in OpenPGP armor
detection.

### Reader top-level message operation and newline reporting

GMimeReader.read_message() ports MimeReader.ReadMessage() for the default,
single-message input format. The existing explicit-stack traversal is shared
with read_entity(); the legacy parser is not called. Headers are delivered
once, then message_begin(), the root entity and descendants, and message_end().
The same message callbacks describe embedded messages. A successful operation
consumes through EOF; another message read fails without additional callbacks.

Only the first operation after assigning a stream probes an optional UTF-8
BOM. Header and entity operations do not strip one and disable the initial
message probe. A complete BOM advances offsets by three bytes without changing
line numbers or delivering callbacks. Truncated or mismatching BOM prefixes,
empty input and BOM-only input fail rather than creating an empty message.
set_stream() restores the probe and clears sticky errors without resetting,
seeking or closing the stream.

Default-format message headers tolerate leading From-lines and >From-lines,
retaining them as invalid raw headers, as in the source's MessageHeaders state.
A stream containing only such lines is not a message. Header-only and entity
reads continue to reject these leading lines in the default input format.
From-lines in default-format bodies are ordinary content. The mailbox
extension below adds explicit mbox/MMDF framing.

The content_end() callback now reports GMimeReaderNewLineFormat for leaf wire
content: UNKNOWN, UNIX, DOS or MIXED. This is separate from serialization's
GMimeNewLineFormat, which has no unknown value. The model adapter below adds
an input-only MIXED value; output formatting still accepts only UNIX or DOS.
Preambles
and epilogues report UNKNOWN, matching their source callbacks' lack of a
newline-format argument. Detection spans short reads, including a split CRLF,
and counts the scanned newline preceding a boundary even when that newline is
excluded from delivered leaf bytes. Bare CR alone does not establish a format;
transfer encodings are not decoded.

Native tests cover exact message callback order, BOM and origin offsets,
nonseekable streams, every callback-abort point, every input-error position,
sticky failures, stream replacement, EOF reuse and callback lifetime. The
actual-source message harness also compares raw callback traces and newline
formats across read sizes, nesting, depth limits, binary data and BOM prefixes.
All 16,368 message cases match the source's callbacks, with complete position
and EOF comparisons on success, in normal and ASan/UBSan builds. Existing
header and entity differential suites retain their previous results. Python
GI and compiled Vala subclasses also cover the new operation and enum.
Mailbox handling, compliance, SIMD scanning and public reader-backed model
loading are completed below. Async operations remain separate work.

### Synchronous reader completion: mailboxes, compliance and SIMD

The synchronous MimeReader port now includes header, entity and message
operations, mbox traversal, compliance reporting, optimized line scanning,
and the private SkipBlankLines/ReadToEos helpers used by status parsing.
The helpers consume only unread input, preserve nonseekable streams, and
never seek, reset or close the source. ReadToEos returns a rewound block
stream without interpreting the copied bytes as MIME or physical lines.
Public GMimeParser model-building integration is described below.
Async/cancellation support remains deferred.

new_with_format()/set_stream_with_format() select default, mbox or MMDF
input. Message reads stream separator bytes through marker_begin/read/end
without their line terminators; even long From-lines use bounded chunks.
Mbox framing follows MimeKit, including initial BOM handling, marker
search, repeated messages and RespectContentLength. Content-Length protects
From-lines inside the declared body only for mbox message reads; it does
not turn header/entity operations into message operations. MMDF is an
additional GMime format, with explicit opening/closing separator handling.
Header/entity reads do not seek initial mailbox markers.

The optional compliance logger receives boxed GMimeComplianceIssue values
with original wire offsets, 64-bit physical line/column numbers, position
kinds, contexts, severity and category metadata. All 96 diagnostic identities
and the truncation marker retain MimeKit's descriptions and remarks.
GMimeComplianceOptions selects transport/storage context, parameter/address/Base64/
quoted-printable/UU validators, and per-violation budgets (zero is unlimited).
Generic header/body/structure checks remain active whenever a logger is
installed, independently of the specialized-validator flags. Without a
logger all compliance checking is skipped.

The raw parameter validator reports malformed declarations and repeated boundary,
filename, and other parameter declarations on every Content-Type and Content-Disposition header,
including headers ignored by model precedence. Names are case-insensitive;
competing RFC 2231 representations and repeated continuation indices are
diagnosed, but distinct continuation segments are not. Content-Type `name`
and Content-Disposition `filename` allow one ordinary fallback alongside one
extended or continued representation. Positions identify the repeated name
in the original folded bytes. Duplicate Content-Disposition headers are a
generic check, independent of the parameter-validator flag. Logging leaves
model values and serialization unchanged. The unreleased violation enum has
been renumbered to match MimeKit's added diagnostics (Parameters flag = 16,
All = 31); stored numeric values from earlier 4.0 development builds must not
be treated as stable.

Missing RFC 2231 continuation segments are reported once per parameter after
scanning the header, at the first continuation declaration (ElementStart).
Indices may arrive out of order, but must cover zero through the highest
observed index without gaps. A balanced tree tracks only observed indices,
avoiding adversarial integer hash collisions; gap detection compares the
highest index with the distinct count instead of scanning or allocating by
index. Fallback values do not hide gaps, and tolerant value assembly remains
unchanged. MissingParameterSegment is 48 in the unreleased enum.

InvalidParameter (44) reports missing or invalid names, missing equals signs or
values, and unterminated quotes/comments at the malformed declaration or
fragment's ElementStart. Recovery honors quotes and nested comments; empty
parameter slots, quoted empty values, and loose unquoted values remain accepted.
Declarations with missing values still participate in duplicate and continuation
tracking. This does not validate RFC 2231 charset/language prefixes, percent
escapes, or encoded contents. The new identity shifts subsequent unreleased
numeric values by one while retaining their metadata and reporting budgets.

InvalidContentType also covers unexpected text after a recognizable type/subtype
and before any parameter separator, including folded values and comments.
Malformed parameters after a valid media type do not trigger this diagnostic.
The check is independent of specialized-validator flags and reports the header
start (ElementStart), without changing tolerant model fallback or serialization.

Each read snapshots the settings and logger. Replacing either from a
callback affects the next operation, not the current one; the previous
logger's data remains alive until the active snapshot is released. Budgets
reset for each operation/message, limit each diagnostic identity separately,
and emit one truncation marker on the first suppressed issue. Reporting
never changes parsing policy or masks callback/I/O errors. Raw content is
staged in bounded batches so generic diagnostics precede encoding-validator
writes and raw callbacks, matching MimeReader's ordering.

The strict address validator ports the actual AddressValidator grammar and
recovery, including nested groups, obsolete syntax, contextual NUL/ISO-2022
diagnostics and all 30 address violations. It does not substitute tolerant
address-model parsing for compliance validation. Existing incremental
encoding validators are now connected to reader wire content and flush at
content completion. Validation includes the newline withheld before a
boundary, even when that newline is not delivered as leaf content.

Runtime-selected AVX2/POPCNT, SSE2 and ARM NEON content scanners count
complete LF/CRLF lines and stop before possible MIME/mailbox delimiters.
The 128-bit and 256-bit kernels port MimeReader's vector delimiter-prefix
and CRLF masks, cross-vector CR carry and bulk population counts. They
also retain both final line offsets required by the native reader.
AVX2 selection checks CPU support and OS preservation of XMM/YMM state;
GCC/Clang target attributes isolate the x86 kernels from baseline code.
NEON is available on ARM64 and in NEON-targeted 32-bit ARM builds;
other targets retain the portable scalar fallback.

All kernels use bounded unaligned loads without a readable sentinel.
Shifted loads require two extra readable bytes, with the remaining tail
handled scalarly. Possible one-byte delimiter prefixes remain buffered
for lookahead. GMIME_DISABLE_SIMD builds contain only the scalar backend.
Header scanning and fused LF/NUL/high-bit searches are not changed here.
test-reader-simd exercises every available backend independently, even
when runtime dispatch prefers a wider kernel. It covers every vector
lane, CRLF carry, dense newline masks, delimiter prefixes, short tails,
unaligned input and inaccessible guard-page bounds. ARM64 NEON and scalar
builds also pass these checks under QEMU. The --benchmark mode reports
each available kernel against the same scalar workloads; results are
machine- and workload-dependent.

Local comparisons with the previous SSE2 scanner also use MimeKit's
Star Trek message and content-length/JWZ mailbox benchmark fixtures,
with identical message counts and consumed offsets. Alternating pinned
runs measured roughly 5-10% higher reader throughput with AVX2; full
message construction stayed within about 3% of the baseline. Synthetic
dense-newline workloads improve substantially with either x86 kernel,
but SSE2-only short/ordinary-line workloads can still be slower than
the old scanner. These measurements do not imply universal speedups;
ARM validation establishes correctness under emulation, not throughput.

Native tests cover long markers, short reads, every callback-abort stage,
mailbox input errors, logger replacement/lifetime, diagnostic budgets,
specialized validation during failures, status helpers, deep traversal,
scalar/SIMD equivalence and inaccessible guard-page bounds. Normal and
ASan/UBSan builds pass. Fresh actual-source differential runs cover 54,768
header, 25,776 entity, 16,368 message/newline, 7,392 mbox message, 756 mbox
header/entity, and 21,024 compliance cases, with no unexplained differences.
The separate strict-address corpus covers 81,737 actual-source cases.
Python GI and compiled Vala consumers cover binary marker callbacks,
options, boxed issues, notified logger lifetime and header/entity/message
subclasses. GIR, typelib, VAPI and reference documentation build.

The entity differences described above remain intentional. The mbox
message corpus additionally classifies 24 physical-newline lookbehind
corrections and 12 synthetic empty embedded-message corrections. Compliance
classifies 21 lookbehind, five synthetic-child and three split closing-marker
corrections. Every one of these mbox/compliance traces equals the actual
source's full-buffer trace; per-case input/reference/canonical/native
evidence is retained in session artifacts. The mbox header/entity corpus
also classifies 24 newline-lookbehind corrections and 54 empty-entity cases
where MimeKit reports -1 content/entity lines at an immediate initial marker;
GMime reports zero without consuming the marker. This avoids unsigned
line-count underflow.

The full core suite passes 33 of 34 test programs. Its sole failure remains
the two previously established test-core text-format assertions; no new
core failures were introduced. Native Windows validation remains pending.
OpenPGP marker detection remains the opt-in legacy-parser extension, not an
always-on reader scan.

### Reader-backed header and leaf model foundation

A private GMimeReader subclass now ports MimeParser's header and root-leaf
callbacks without invoking the legacy parser. It builds GMimeHeaderList,
GMimePart and existing specialized leaf models, retains original offsets
and header/body separator state, and preserves the first Content-Type,
Content-Transfer-Encoding and metadata header rather than letting later
duplicates replace the parsed model. Meaningful source implementation
comments accompany the construction, buffering and persistence rules.

Parsed headers retain length-delimited raw field/value bytes independently
of their C-string views. Invalid fields serialize without an invented colon;
binary values, invalid bytes and original field whitespace survive cloning
and transport serialization. Mutating a value discards its old byte cache
without affecting clones. C-string getters still end at embedded NULs;
invalid field display/lookup names escape NUL as "\0". International
formatting preserves NUL-containing raw values instead of truncating them.

Content remains in its original transfer encoding. Nonpersistent input uses
a rewound block stream; persistent input uses exact source substreams.
Empty bodies retain MimeKit's null-content distinction. Before another read,
the adapter restores the physical read-ahead position, not its logical
position, so reading an earlier persistent body cannot replay buffered input.
Inherited stream replacement updates the adapter's source and persistence
decision. Unsupported substreams, restoration failures and input/callback
failures report GError and clean up partial models.

GMimeContent exposes get/set/clear_newline_format() for nullable input hints,
including GMIME_NEWLINE_FORMAT_MIXED. Hints never convert bytes, and replacing
the backing stream clears them. Mixed is not a valid output/encoder format.
The default entity registry now selects GMimeTextPart for application/rtf.
Encoded message/partial is an opaque GMimePart, while unencoded fragments
retain GMimeMessagePartial. Registered leaf subclasses are retained;
unsupported custom containers fail rather than silently changing type.

Transfer-encoding tokens are parsed by length so a literal NUL cannot turn
an invalid token such as "base64\0x" into valid Base64. A NUL after whitespace
or a semicolon remains an ignored suffix, matching MimeKit's first-token
policy. Both wire classification and lazy model encoding caches follow this
rule, and explicit header assignments invalidate the retained binary cache.

The 16,920-case actual-source header/leaf corpus compares raw headers and
offsets, separator and EOF state, model type, transfer encoding, nullable
content/newline hints, encoded/decoded bytes, and LF/CRLF serialization.
Every case matches in normal and ASan/UBSan builds. Native tests additionally
cover mutations, clone lifetimes, binary encoding tokens, persistent
read-ahead restoration, stream replacement, custom types and failure cleanup.
Python GI and compiled Vala consumers exercise the newline hint APIs; GIR,
typelib, VAPI and reference documentation build successfully.

The new model and reader tests pass with leak checking enabled. Existing
test-headers and test-mime-part sanitizer leaks also reproduce on untouched
HEAD (648 and 1,168 bytes respectively). The normal core suite still has
only the established two test-core text-format assertion failures. Native
Windows validation remains pending.

### Reader-backed message model assembly

The private adapter now ports MimeParser's explicit owning object stack,
message begin/end and entity end callbacks. It builds top-level messages
and nested message/rfc822, message/rfc2822, message/global, message/news and
message/external-body models, plus text/rfc822-headers and
message/global-headers containers. Parsed messages retain non-Content-*
headers; their root bodies retain only Content-* headers. Ordinary nested
entities keep every header. The first Subject and Message-ID remain the
semantic values while repeated address fields are collected together.
Meaningful source comments explain the header copying and opaque fallback.

Parsed bodies are attached without the public body setter, preserving
header offsets and avoiding an invented MIME-Version header. Nested models
retain raw binary headers, separator state and buffered/persistent content
after their reader and input are released. Encoded message/header containers
and descendants at the nesting limit remain opaque leaves. Multipart
assembly and raw delimiter storage are described in the following section.

The default registry now maps message/external-body to GMimeMessagePart and
message/global-headers to GMimeTextRfc822Headers, matching MimeKit. This also
affects legacy parsing. The historical jwz.mbox summary now includes its 13
external-body descendants; every added descendant was checked against the
actual MimeKit parser, with all existing offsets and other output unchanged.

A further 17,136-case actual-source corpus compares recursive message trees,
raw headers and offsets, first-value metadata, repeated addresses, nullable
content/newline hints, raw/decoded content and LF/CRLF serialization. Cases
cover buffered/persistent requests, seekable/nonseekable input, short reads,
empty/truncated messages, binary headers, encoded containers and depth
limits. Every case matches in normal and ASan/UBSan builds, as does the
original 16,920-case header/leaf corpus. Native tests also exercise nested
read failures at every byte offset and EOF, missing errno, sticky errors,
stream replacement recovery and retained body lifetimes with leak checking.
Mailbox model construction and envelope preservation are described below.

The following multipart, mailbox and public-integration increments complete
the synchronous model-loading backend, including optional OpenPGP detection.
Public operations share one reader: alternating independent engines on a
nonseekable stream would lose their read-ahead buffers. Async/cancellation
integration and native Windows validation remain pending.

### Reader-backed multipart model assembly

The private adapter now builds multipart trees, including multipart/digest
default message children, nested messages and opaque depth-limit fallbacks.
Preambles and epilogues are buffered even when leaf content uses persistent
substreams. Length-aware immutable bytes retain binary text and exact
per-child, closing and unmatched trailing delimiters. An empty closing
marker means truncated input; a null marker requests a generated delimiter.
Meaningful MimeKit comments accompany delimiter ownership, consecutive
markers, truncation and the epilogue's initial delimiter-owned newline.

Reader-built multipart serialization preserves raw delimiter bytes even
when LF/CRLF output formatting differs. Preamble/epilogue newlines are
normalized, while delimiter-owned newlines are added only where needed.
Final-newline requests propagate through truncated final descendants and
embedded messages without mutating shared child objects or caller options.
Leaf writers now count completed filter output and propagate completion
failures instead of hiding a failed final newline or encoded tail.
Legacy-built multiparts retain their existing serialization path.

Child edits keep raw delimiter slots aligned: replacement retains its slot,
insertion/addition use generated markers, and removals discard matching
slots. Clear restores generated closing markers. Boundary changes discard
obsolete child/end markers, including direct parameter edits, Content-Type
replacement and change-then-restore sequences; unrelated parameter edits
preserve them. Reassigning an unchanged decoded preamble/epilogue retains
its original binary bytes. String-setter folding and the corresponding
constructed-writer adaptation are described below.

The new corpus compares parsed multipart trees, raw text/markers, nullable
content and LF/CRLF serialization against actual MimeKit sources, plus
child/boundary/text mutations and final-newline propagation. Defaults are
aligned to LF for cross-platform mutations. Only the edited Content-Type
header's source offset is excluded from mutation comparisons: GMime's
existing setters retain offsets while MimeKit clears them. Parsed offsets
and the edited header's actual serialized bytes are still compared.

This work also fixes read-ahead-dependent child counts at EOF: a complete
opening marker introduces an empty child, whereas an unterminated marker
remains trailing delimiter data. Leading invalid embedded-message headers
are retained as binary pre-header marker bytes and written unchanged,
matching MimeKit rather than treating them as ordinary message headers.
Native tests cover these cases across read sizes, binary round trips,
content-type event lifetimes, child mutations, output errors, and nested
read failures at every byte offset and EOF with replacement recovery.

All 33,600 multipart cases match in normal and ASan/UBSan builds. The
earlier 17,136 message and 16,920 header/leaf cases also still match after
the leaf output-completion changes. Focused reader/model/encoding tests
pass with leak detection enabled. The full normal core suite passes 34 of
35 programs; only test-core's two established text-format assertions fail.
GIR, typelib, VAPI and GTK-doc builds pass with existing warnings. Native
Windows validation remains pending.

Public parser/loading integration and optional OpenPGP model detection are
completed below, as is signature verification's special raw-marker CRLF
canonicalization. Synchronous serialization now uses MimeKit's explicit
writer-frame stack, as described below. Async/cancellation remains separate
work.

### Multipart binary text views

Parsed multipart preambles and epilogues retain their original binary data
in owned GBytes, independently of the decoded preamble/epilogue strings.
The shared length-aware text decoder now replaces decoded Unicode NULs
with U+FFFD instead of truncating these C-string views. Replacement happens
after charset conversion: zero bytes within UTF-16/UTF-32 characters are
not independently replaced. Existing charset selection remains unchanged.

The epilogue view still omits one initial LF or CRLF belonging to the
closing delimiter, without removing it from the raw bytes. MimeKit's
inline comments about this newline and restoring a truncated closing
delimiter when assigning an epilogue accompany the native implementation.
Getter results are nullable borrowed UTF-8 strings. Assigning the same
decoded value preserves both the original GBytes and serialized output.
Changed strings use the folding semantics described below; NULL removes
the corresponding text and raw bytes.

Native regressions cover consecutive NULs, UTF-8, Latin-1 fallback,
UTF-16LE/BE and UTF-32BE conversion, epilogue newline ownership, both
content storage modes, no-op and changed setters, NULL removal, and raw
snapshots retained after the source, model and replacement data are freed.
These are immutable-byte lifetime checks, not a new multipart cloning API.
Raw boundary-marker APIs are unchanged and their migration remains open.

All 6,180 actual-source display/raw-byte/serialization cases match MimeKit
in normal and ASan/UBSan builds. Only managed display NULs are normalized
to U+FFFD for this native C-string adaptation; raw bytes and wire output
are compared unchanged. Cases cover seven charsets, short and long input,
three read sizes, LF/CRLF output, memory/source storage on seekable input
and memory storage on nonseekable input. The existing 80,472 public parser
and writer cases also still match in both builds. Focused parser-model,
reader and anonymizer tests pass with leak detection. GIR, typelib, VAPI,
GTK-doc, exported preamble symbols and live GI ownership/string checks
pass with existing build warnings. The normal core baseline remains
38/39 programs, with only
test-core's two established text-format assertions failing. Native Windows
execution remains unvalidated.

### Multipart text folding and setters

Preamble and epilogue setters port Multipart.FoldPreambleOrEpilogue,
using the default formatting options. Only ASCII linear whitespace (SP,
HTAB, CR and LF) separates words; VT, FF and Unicode whitespace remain
part of a word, including before an explicit LF. UTF-16 code-unit column
counts still match MimeKit, including supplementary characters.
A following word wraps when it would reach or exceed the
78-column default; oversized words remain intact. Explicit LF sequences
are normalized, preceding whitespace in that run is discarded, and a
nonempty final logical line receives a newline. Other whitespace,
including bare CR, follows the source algorithm.

This intentionally changes constructed and changed parsed text: getters
return the folded value, rather than the original setter argument.
NULL removes the text; empty strings remain distinct from NULL.
Both object modes retain independently owned immutable raw bytes.
An equal decoded assignment preserves raw-byte identity and serialization,
including parsed binary text. Borrowed substrings and cross-field values
are copied before their old storage is released. Invalid UTF-8 produces a
GLib precondition diagnostic without changing the existing value.

Raw epilogues include the closing delimiter's newline; decoded views omit
it. Assigning a non-NULL changed epilogue restores a truncated closing
marker. Applicable MimeKit newline-ownership and restoration comments
remain alongside the implementation; the source folding routine itself
has no inline comments. Constructed serialization now uses the folded
raw bytes without duplicating delimiter newlines. Legacy string-only
fallbacks, missing-boundary behavior and closing-marker suppression remain
available. Raw boundary-marker APIs are unchanged.

Before the intentional ASCII-whitespace restriction, all 9,274 actual-source
folding cases matched in normal and ASan/UBSan builds, comparing decoded
values, raw bytes and exact LF/CRLF output.
The corpus covers constructed, complete parsed and truncated objects,
Unicode whitespace, exact thresholds, long words, randomized text,
NULL/empty values, borrowed substring assignments, no-op binary data,
short reads, seekability and memory/source storage. Harness-only default
newline alignment exercises LF and CRLF independently of output options.
Unicode-whitespace cases now intentionally diverge from MimeKit. Native
regressions cover all 25 managed whitespace characters, LF/CRLF output,
wrap thresholds and whitespace before explicit LF. The earlier validation
also matched all 6,180 binary-view cases, 80,472 public parser/writer cases
and 100 constructed multipart anonymizer cases in both builds.

After the ASCII restriction, all 5,024 folding cases without changed
whitespace semantics still match the actual-source oracle in normal and
ASan/UBSan builds. The updated duration/report tests also pass in isolation
with fatal criticals, ASan/UBSan and leak detection. The complete sanitized
test-core run reports a separate 435-byte type-registry leak from the
depth-sibling tests; those tests and registration code are unchanged.

Focused parser-model, reader and anonymizer tests pass with fatal
criticals, ASan/UBSan and leak detection. The complete normal core suite
retains its 38/39 baseline, with only test-core's two established text-format
assertions failing. Full build, GIR, typelib, VAPI, GTK-doc and live Python
GI checks pass with existing warnings, including nullable UTF-8 setters,
folded constructed serialization and retained string lifetimes.
Native Windows execution remains unvalidated.

### Multipart indexed collections

Built-in multipart insertion now requires an index from zero through the
current child count, inclusive; indexed access, removal and replacement
require an existing child. Invalid indices produce GLib precondition
diagnostics; excessive indices no longer silently append or return NULL.
Rejected operations preserve children, references, raw delimiter slots,
end/trailing markers and serialized bytes. Bounds checks on virtual
operations remain in their default implementations, preserving subclass
dispatch. Applicable MimeKit comments about unmatched trailing markers
are retained beside the corresponding native field.

Valid operations preserve duplicate references and same-object replacement,
keep parsed delimiter slots aligned, and clear unmatched trailing markers
on mutation. Remove-at and replacement transfer the multipart's old
reference to the caller; indexed access remains borrowed. Native regressions
exercise both returned references after the multipart is destroyed.
Constructed multipart output also uses the existing delimiter-newline
check, avoiding an extra blank line after children with no content while
retaining legacy closing-marker suppression.

There is one deliberate rejected-operation difference: MimeKit RemoveAt
and indexed replacement clear RawTrailingBoundary before throwing for an
invalid index. GMime retains its existing atomic rejection and diagnoses
the programming error without discarding that marker.

The 6,372 actual-source collection comparisons cover mixed, alternative
and related constructed containers; complete, empty and truncated parsed
models; negative/count/maximum indices; duplicate children; raw slots;
both output newline formats; short reads; seekability; and memory/source
storage. In normal and ASan/UBSan builds, 5,796 cases match exactly and
576 explicitly reproduce the managed trailing-marker side effect, with
no unexpected differences. All 80,472 existing public parser/writer cases
still match in both builds. Focused parser-model, reader, anonymizer,
body-builder and iterator tests pass with fatal criticals and leak
checking. The full normal core suite retains its 38/39 baseline, with only
test-core's two established text-format assertions failing. Full build,
GIR, typelib, VAPI and GTK-doc generation pass with existing warnings.
Live GI checks confirm borrowed getters, transferred old references,
same-object replacement and object lifetimes for mixed, alternative and
related containers. The existing folding, binary-view and constructed
anonymizer comparisons also still match in both builds.
Native Windows execution remains unvalidated. Raw marker APIs, boundary
validation and managed disposal semantics remain separate work.

### Multipart boundary validation and markers

The multipart boundary setter now enforces RFC 2046's syntax: 1 to 70 ASCII
bchars, without a trailing space. Invalid values produce a GLib precondition
diagnostic before virtual dispatch and leave headers, boundary parameters
and parsed markers unchanged. Values are not trimmed; valid leading and
internal spaces are retained. Empty strings, control bytes, non-ASCII values
and overlength boundaries are rejected. This deliberately resolves
MimeKit's validation TODO rather than retaining its permissive assignment
behavior.

The reader's existing boundary validator is shared with the setter without
changing tolerant parsing or compliance reporting. Low-level Content-Type
parameter edits remain permissive; this setter does not retroactively
validate parsed headers or rewrite malformed input. GMime retains its
native NULL-to-generate setter and generate-if-absent getter. Identical
assignments preserve raw headers and markers; changed values retain the
existing invalidation policy. The parameter setter copies before freeing,
so borrowed boundary strings and substrings remain safe.

Writer and anonymizer generated markers use the boundary's bytes directly.
Their Unicode ASCII-replacement and UTF-16 byte-cast paths are unnecessary
for validated ASCII boundaries and have been removed. The boundary Unicode
trim helper has also been removed. Part headers use the shared ASCII
linear-whitespace trimmer; filename lookup preserves parameter values
without trimming. Raw opening, closing and trailing markers remain
length-aware and byte-preserving, including NUL and non-UTF-8 bytes.
Truncation, epilogue and closing-newline policies
and applicable source comments are unchanged.

Persistent tests cover all nonzero byte values, all bchars, every accepted
length from 1 through 70, 71/1024-byte rejection, managed whitespace,
unchanged raw state/output on rejection, borrowed strings and native
generation. There are also 48 generated-marker and four raw-marker
combinations, with LF/DOS output, short writes and failure injection at
every output byte, checking partial output, errors, nonclosure and reuse.

The 33,600 writer and 31,104 anonymizer actual-source comparisons use valid
non-leading-space boundaries, where MimeKit's trimming does not differ.
Marker bytes and raw state match in normal and ASan/UBSan builds with leak
checking. There are 33,400 and 26,784 exact matches respectively; the
remaining 200 and 4,320 cases differ only in existing encoded-word parameter
header formatting. Standalone header writes independently reproduce these
differences; replacing precisely that header makes the full outputs match.
There are no unexpected differences. Rejection and untrimmed leading-space
behavior are covered by native tests rather than the permissive reference.

The 6,372 collection comparisons retain zero unexpected differences, and
all 100 constructed anonymizer comparisons still match in both builds.
Boundary-focused native tests pass with fatal criticals and sanitizer leak
checking. Updated part-property regressions cover all five GLib whitespace
characters, preserved vertical tabs and Unicode whitespace, NULL, empty
values and borrowed assignments. Normal part tests pass; ASan/UBSan reports
only test-mime-part's existing 1,168-byte leak in five allocations from
legacy text/write tests.
The full normal core suite retains its 38/39 baseline, with only test-core's
two established text-format assertions failing. Full build, GIR, typelib,
VAPI, GTK-doc and live GI checks pass with existing warnings. Native Windows
execution remains unvalidated.

### Reader-backed mailbox model construction

The private adapter now constructs successive mbox and MMDF messages using
one reader and its existing read-ahead buffer. MimeParser's envelope
callbacks retain length-aware opening marker bytes, excluding CRLF, and
their stream offsets. The envelope remains parser metadata, separate from
invalid leading From-header bytes stored on a message. An ordinary message
write does not insert its mailbox envelope.

Markers are published only after the complete message operation succeeds.
Every construction operation clears the previous marker, including failed
reads and operations after stream replacement. MMDF closing callbacks do
not overwrite the opening marker. MMDF support preserves GMime's existing
extension; MimeKit only supports entity and mbox formats.

The existing reader handles mailbox pretext, truncated input and optional
mbox Content-Length suppression. Persistent models restore the physical
source position before the next parse if reading earlier content moved it.
Native regressions cover long binary envelopes, 64-bit source offsets,
split CRLF, binary/nested bodies, Content-Length, buffered/persistent and
seekable/nonseekable sources, retained model/marker lifetimes, and failures
at every input byte and EOF with sticky errors and replacement recovery.
The embedded-message boundary probe now fills its bounded lookahead across
short reads without scanning an entire following envelope. A 10,000-byte
next marker regression rejects reads beyond the message prefix plus 128
bytes. Immediate sibling/closing multipart boundaries of lengths 1 and 140
also check absent embedded messages and exact entity/header end offsets
across LF/CRLF and eleven read sizes. The reader's separate previous-byte
state preserves the preceding newline when refilling its input buffer.

The actual-source differential corpus compares successive message trees,
raw headers/offsets, envelope/pre-header bytes, logical positions, EOF,
LF/CRLF serialization and re-reading earlier persistent messages. With
MimeKit's updated short-read boundary and end-offset fixes, every case is
compared directly against its corresponding source result. The former
4096-byte canonical-result workaround is removed; an independent source
read-size invariance check reports no discrepancies.

All 12,816 mailbox cases match directly in normal and ASan/UBSan builds,
including the added immediate-boundary and embedded-message EOF fixtures.
The earlier 33,600 multipart, 17,136 message and 16,920
header/leaf cases also still match in both builds. Focused model, reader
and mbox tests pass with leak detection enabled. The full normal core
suite remains at 34/35 programs, with only test-core's two established
text-format assertions failing. GIR, typelib, VAPI and GTK-doc builds pass
with existing warnings. Native Windows validation remains pending.

Public parser/loading integration and optional OpenPGP model detection are
completed below, as is signature-specific raw-marker canonicalization.
Iterative writing and async/cancellation remain separate work.

### Public reader-backed parser integration

GMimeParser now delegates header, entity and message construction to the
reader-backed model builder instead of maintaining a second parsing engine.
Its public loading helpers therefore use the same MimeKit-derived model
construction, with the adapted inline implementation comments retained.
Successive header/entity/message operations, format changes and persistence
changes keep one reader and its buffered input. Stream replacement discards
buffered data, errors and metadata without resetting or closing either input.

The compatibility wrapper retains per-operation option snapshots, header
regex callbacks, sticky errors, header byte-limit error codes, root header
offsets and mbox marker metadata. Header callbacks can unregister themselves
or release their parser reference; reentrant reads and stream/settings
replacement during parsing are rejected. Public header-only loading stops
at the separator without an additional body/EOF probe, while the raw reader
retains MimeKit's original read-ahead behavior. Clean entity/message EOF
returns NULL without an error. Nonseekable logical positions start at zero.
Previously returned persistent content remains usable after subsequent reads,
including when persistence is disabled after that content moves the source.

Optional OpenPGP detection feeds wire callbacks into the existing incremental
armor filter without decoding or replaying the input. Encoded content retains
the lazy decoded-inspection fallback. Parser warnings retain duplicate and
conflicting headers, raw 8-bit headers, missing multipart boundaries,
interrupted part headers and nesting overflow. Ancestor-boundary recovery is
reported separately from actual EOF truncation.

Public models now retain empty multipart children and malformed wire header
rows instead of discarding them, matching MimeKit. Text parts without Content
return an empty string. Content-Length requires the leading whitespace used
by MimeKit's SkipWhiteSpace check. Valid field names extending beyond the
reader's fixed lookahead are spilled into the bounded header buffer, including
whitespace before the colon; header limits still apply before each append.

The four actual-source corpora cover 80,472 cases per normal and ASan/UBSan
build, both through the private adapter and through the public parser. Public
comparisons account explicitly for standalone header EOF-observation
differences and the C-string mbox marker getter's inability to expose bytes
after NUL. Private comparisons retain exact raw EOF and binary marker checks.
Trees, raw headers/offsets, content, mutations, LF/CRLF serialization and
retained persistent models otherwise match directly.

Native reader/model/mbox tests pass with leak checking enabled. New public
regressions cover buffer retention across operation/format changes, binary
markers/content, persistence changes, callback lifetime/reentrancy, warnings
and long field names around buffer boundaries. The full normal core suite
passes 34 of 35 programs; only test-core's two established text-format
assertions fail. Full build, GIR, typelib, VAPI and GTK-doc generation pass
with existing warnings. Native Windows validation remains pending.

### Signature-verification serialization

Multipart/signed verification now selects the private writer's
VerifyingSignature state rather than storing it in formatting options,
matching MimeKit's updated ownership. Preserved opening, closing and trailing
multipart delimiters convert bare LF to CRLF only in this mode. Existing
CRLF, lone CR, binary bytes, whitespace and unterminated marker tails remain
intact, including double/triple boundary runs. Ordinary serialization still
preserves raw markers byte-for-byte, even with DOS output selected.

The same mode ports MimeWriter's mixed-newline compatibility rule: retain
original parsed body bytes when the stored newline hint is Mixed and no
transfer-encoding conversion is needed. Binary bodies remain unchanged;
transcoding still takes precedence over pass-through. MimeWriter's raw-marker
explanation and its mixed-newline explanation and issue reference are
retained as adapted inline comments.

Verification rejects signatures without leaf content and propagates
serialization, decoding and rewind failures before calling the crypto
backend. The leaf serialization fast path now checks both source rewinds
and preserves the original output error if cleanup also fails.

All 17,568 new actual-source verification-output cases match MimeKit in
normal and ASan/UBSan builds. The existing 80,472 public parser/model cases
also still match in both builds. Native regressions exercise all three raw
marker slots, empty/binary markers, persistence, nested signed multiparts,
mixed-newline hints, transcoding, decoded signature bytes, short/zero writes
and read/rewind failures with and without errno. A test crypto context
captures exact backend inputs and checks that failures prevent invocation;
these tests do not perform real cryptographic verification.

Focused reader/model/mbox tests pass with leak checking enabled. The full
normal core suite remains at 34/35 programs, with only test-core's two
established text-format assertions failing. Full build, GIR, typelib, VAPI
and GTK-doc generation pass with existing warnings. Native Windows
validation remains pending.

### Synchronous iterative MIME writing

The four built-in entity/message serializers now delegate to a private
GMimeWriter, porting MimeKit's synchronous MimeWriter traversal. Multipart
frames retain their options, next child, cached opening marker and final
newline requirement. Embedded messages and message bodies are tail
continuations rather than recursive calls. Frames and temporary formatting
options are released on success and failure, with checked byte counts and
preservation of the original I/O error.

Newline requirements are passed explicitly to leaf content writing;
serializing a message no longer changes or clears its body's
ensure_newline flag. Embedded-message options can both enable and disable
newline completion. Signed children clear hidden-header filtering and
disable international header reformatting without changing caller options.
Raw markers, binary bodies, truncated multiparts and mixed-newline
verification retain their existing reader-backed behavior. Constructed
legacy multiparts retain their delimiter conventions,
including suppression of the closing delimiter when no boundary is set.

GMime deliberately retains GObject write_to_stream overrides, unlike
MimeKit's direct built-in descendant dispatch. Custom writers still receive
whole-entity/content-only calls and can chain to their parent writer. A
callback-scoped, thread-local context carries verification and explicit
newline requirements through parent chaining, without putting verification
state back into formatting options. Built-in traversal remains iterative;
recursion introduced by a custom override is the override's responsibility.
MimeKit's explanatory inline comments are adapted alongside these paths.

Native regressions compare exact full/content-only output through 8,192
levels of multipart, embedded-message and mixed nesting with both newline
formats. They also exercise deep output failures, model-state preservation,
custom dispatch, signed option scoping, reentrant writes and parent chaining.
Model destruction remains recursive; depth fixtures retain descendants
while releasing parents so teardown does not obscure writer behavior.
The same regressions pass with a 512 KiB process stack.

All 80,472 ordinary public parser/model cases and 17,568 signature-output
cases match the current actual MimeKit source in normal and ASan/UBSan
builds. Focused reader/model/mbox tests pass with leak checking enabled.
The full normal core suite remains at 34/35 programs, with only test-core's
two established text-format assertions failing. Full build, GIR, typelib,
VAPI and GTK-doc generation pass with existing warnings.
Async/cancellation and native Windows validation remain separate work.

### Public message newline completion

GMimeFormatOptions now exposes get_ensure_newline()/set_ensure_newline(),
porting MimeKit's public EnsureNewLine property. The default is FALSE;
clones retain independent values, and the setter rejects NULL and the
shared defaults. Parsed messages can opt into a final newline using the
selected newline format without changing their body's stored flags.

The option applies at message boundaries, not indiscriminately to
standalone entities or every multipart child. Embedded-message options
follow the enclosing part's explicit requirement, including clearing an
incoming TRUE value. Binary content remains unchanged, and no body is
synthesized for bodyless messages. Raw opening markers retain their
original newline bytes.

All 21,600 new actual-source output cases match MimeKit in normal and
ASan/UBSan builds, covering full/content-only entities, messages, nested
messages, multipart endings, transfer encodings, both newline formats and
both content storage modes. Native regressions also check default
protection, clone independence, repeated writes, model-state preservation
and final-newline output failures. Focused reader/model/mbox tests pass
with sanitizer leak checking enabled.

The existing 80,472 ordinary and 17,568 signature-output cases also match
the current actual MimeKit source in both builds. Full core testing
remains at 34/35 programs with only the two established text-format
assertions failing. Full build, GIR, typelib, VAPI and GTK-doc generation
pass with existing warnings; the new accessors are exposed in the bindings
and reference documentation. Native Windows and async/cancellation
remain pending.

### Content-type-aware entity loading

g_mime_entity_load_with_content_type() ports MimeKit's synchronous
MimeEntity.Load(options, contentType, content) overload for body streams
whose Content-Type is supplied separately. It encodes a synthetic
Content-Type header with DOS newlines, chains it before the content, and
uses the ordinary entity parser with memory-backed content storage.
The parser selects the registered entity subclass rather than assigning
the supplied model directly.

The content stream is consumed from its current position without seeking,
resetting or closing it, including for nonseekable inputs. Parsed content
survives source modification, close and release. The Content-Type model is
not modified or retained; its parameters are serialized and reparsed.
NULL parser options select the native defaults. Depth and header-length
controls, malformed-input recovery and explicit I/O errors remain those of
the public parser. Header offsets refer to the synthetic header followed
by the body, not to positions in the original body-only stream.

All 15,552 actual-source cases match MimeKit's entity type and exact
full/content-only output in normal and ASan/UBSan builds. Cases cover
empty/binary bodies, Unicode and quoted parameters, nested messages,
truncated multiparts, specialized subclasses, parser depth limits,
newline controls, chunked and nonseekable inputs, and nonzero source
positions. Native regressions additionally check argument validation,
model/source independence, header limits and read failures at every
body offset, including errors without errno. Focused reader/model/mbox
tests pass with sanitizer leak checking enabled.

Full core testing remains at 34/35 programs with only the two established
text-format assertions failing. Full build, GIR, typelib, VAPI and
GTK-doc generation pass with existing warnings; the loader is exposed in
the generated bindings and reference documentation. Native Windows and
async/cancellation validation remain pending.

### Synchronous entity and message file writing

g_mime_entity_write_to_file() and
g_mime_entity_write_content_to_file() port MimeKit's synchronous filename
WriteTo overloads through the existing public virtual stream writers.
The full writer accepts GMimeMessage as well as other entities; the
content-only writer retains native stream serialization semantics.
NULL formatting options select native defaults.

Files are created or truncated before writing, explicitly flushed, and
closed on success and failure, including when an overriding writer keeps
a reference to the output stream. Partial files are not removed and
replacement is not atomic. Source-backed content must not use the same
file as its destination. The stdio backend drains buffered output without
requesting fsync-style durability; file access and sharing follow native
stdio conventions. Its opener now uses g_fopen() for UTF-8 Windows paths.

Open, write, flush and close failures return FALSE with GMIME_ERROR and
the native errno code, or GMIME_ERROR_GENERAL when a custom writer supplies
none. Cleanup preserves the first failure and its errno rather than
replacing it with a secondary close failure. Native tests cover defaults,
Unicode paths, missing parents, directory destinations, validation before
truncation, repeated writes, binary bytes, full/content-only entities and
messages, custom dispatch, retained-stream closure, partial files,
no-errno failures and combined write/close failures. Linux tests also
inject buffered flush and close-only failures and exercise /dev/full and
/dev/null. The corresponding MimeKit file wrappers contain no inline
implementation comments.

All 2,112 differential cases match native stream output byte-for-byte in
normal and ASan/UBSan builds, with each case written twice. They cover
memory/source-backed models, create/overwrite paths, newline controls,
international and hidden-header formatting, binary/encoded leaves,
specialized entities, nested messages and truncated multiparts.
Of these, 2,096 also match actual current MimeKit file output exactly.
The remaining 16 expose an existing standalone-entity difference:
native header-list writing suppresses configured hidden headers, whereas
MimeKit retains them outside message boundaries. This file-I/O port
deliberately preserves the existing native stream behavior rather than
changing header filtering. No other differential mismatches remain.

Focused reader/model/mbox/substream tests pass with sanitizer leak checking
enabled. Full core testing remains at 34/35 programs with the two
established text-format assertions failing. Full build, GIR, typelib,
VAPI and GTK-doc generation pass with existing warnings; both new APIs
are exposed in the generated bindings and reference documentation.
Native Windows validation and async/cancellation remain pending.

### Headers-only message writing

g_mime_message_write_headers_to_stream() ports the synchronous headers-only
MimeMessage.WriteTo() overload. It merges message and body headers, applies
hidden-header and international formatting, and includes the header/body
separator only when present in the parsed model. NULL formatting options
select native defaults. Full-message serialization shares this path;
g_mime_entity_get_headers() continues to return fields without a separator.

The operation does not read, seek or reset content, invoke the body's
virtual writer, or apply body newline completion. Repeated writes preserve
bytes, source positions and newline settings. The caller's output stream
is neither flushed nor closed; only the internal newline filter is
completed. Short writes are completed, zero writes fail with EIO, and
other write failures return -1 with the destination's errno.

The shared merge now follows MimeKit's offset-aware ordering for edited
headers. Leading headers without source offsets are emitted first;
appended message headers no longer move ahead of parsed body headers.
The upstream merged-header and ordering comments are retained. This
correction also applies to full-message output and get_headers().

All 33,280 actual-source headers-only differential cases match in normal
and ASan/UBSan builds. They cover memory/source-backed content, one-byte
and buffered reads, nonzero input positions, Unix/DOS newlines, newline
completion settings, hidden and international formatting, malformed raw
fields, missing separators, nested/specialized entities, truncated
multiparts, and prepended, appended or replaced headers. Each case writes
twice with content access blocked after parsing.

Native regressions additionally verify exact full-message prefixes and
fields-only output, defaults, failure at every output byte, short writes,
errors without errno, untouched body overrides and newline flags, and
caller stream ownership. Existing malformed UTF-8 encoded-word decoding
differences remain outside this increment: international output can use
question marks where MimeKit emits Unicode replacement characters.

The shared full-message path also retains all 80,472 model/writer,
17,568 signature-marker and 21,600 newline-completion reference matches
in both builds. All 2,112 native stream/file comparisons pass with the
16 documented standalone hidden-header differences unchanged.
Focused reader/model/mbox tests pass with sanitizer leak checking.
Full core testing remains at 34/35 programs with the two established
text-format assertions failing.

Full build, GIR, typelib, VAPI and GTK-doc generation pass with existing
warnings. The new method's return type and nullable formatting options
are verified in generated bindings and its reference entry is present.
GI/Python calls with NULL options and repeated writes also pass.
Native Windows validation and async/cancellation remain pending.

### Memory-backed stream and filename loading

g_mime_entity_load()/load_from_file() and
g_mime_message_load()/load_from_file() port the synchronous MimeKit Load()
overloads with default memory storage. They share the reader-backed public
parser with persistence disabled. NULL parser options select native
defaults; supplied options, warnings and depth/header limits are forwarded.
Explicit storage selection is described below.

Borrowed input is read from its current position without seeking,
resetting or closing it, and need not be seekable. Returned models do not
retain the input and remain usable after it is closed, modified or released.
Message loading consumes an optional leading UTF-8 byte-order mark;
entity loading retains the existing entity-header parsing rules.
These helpers parse a single entity/message, not an mbox or MMDF mailbox.

Filename input uses GMimeStreamFs because GMimeReader already buffers I/O;
it does not add stdio buffering. The existing descriptor opener handles
UTF-8 Windows filenames and selects binary mode on Windows. Owned input
is explicitly closed on success and failure before the helper returns.
Close-only failures discard the parsed model and report the close error;
an earlier parse/read error takes precedence over a later close error.
Existing output helpers retain their separate stdio-based implementation.

The loaders map empty input to GMIME_ERROR_PARSE_ERROR ("End of stream."),
matching MimeKit's Load() failure without changing the parser APIs'
non-error EOF contract. Open, read, parse and close failures return NULL,
with a GError when an error destination is supplied.
The upstream convenience wrappers contain no inline implementation
comments; the shared native cleanup documents first-error precedence.

At the time, each normal and ASan/UBSan run checked 15,360 actual-source loading cases:
15,240 match exactly and 120 exercise the pre-existing default-header-limit
difference (native 16 MiB versus current MimeKit's 16 KiB). Those 120 also
match exactly when the reference uses the explicit native limit. No
unexpected differences remain. Coverage includes stream/file input,
one-byte and buffered reads, nonseekable and nonzero-position input,
parser limits, Unix/DOS output, newline completion, binary/encoded content,
specialized entities, nested messages and truncated multiparts. Repeated
serialization verifies content independence after the source is blocked,
truncated, overwritten or removed.

GMime now uses the same 16 KiB default as MimeKit; the default-limit difference
above describes those earlier validation runs, not the current behavior.

Native regressions cover caller ownership, released source references,
warning forwarding, UTF-8 paths, missing/directory inputs, header limits,
read failures at every byte (with and without errno), invalid arguments
and omitted error destinations. Linux checks verify descriptor release,
close-only errors and preservation of a parse error over a close failure.
Focused reader/model/mbox tests pass with sanitizer leak checking.
Full core testing remains at 34/35 programs with the two established
text-format assertions failing. Full build, GIR, typelib, VAPI and GTK-doc
generation pass with existing warnings. All four APIs' annotations and
reference entries, plus live GI/Python loading and errors, are verified.
Native Windows validation and async/cancellation remain pending.

### Explicit content storage for stream loading

GMimeContentStorage and g_mime_entity_load_with_storage()/
g_mime_message_load_with_storage() port MimeKit's synchronous
MimeContentStorage loading overloads. MEMORY (0) copies content and
SOURCE (1) references encoded leaf content in the original stream.
The existing stream and filename loaders still select MEMORY.

Both modes borrow input at its current position without resetting or
closing it. Memory content is independent of input after parsing.
Source content retains its input through substreams, so callers may
release their stream reference, but must keep the underlying input open
and unchanged while content is used. Releasing a parsed model does not
close a stream still held by its caller.

Source mode rejects unsupported input instead of silently copying.
Native streams have no CanSeek property, so validation requires a working
tell and zero-offset current-position seek before parsing. The seek must
preserve the reported position. Referenced leaf content also requires
working substreams; errors propagate without a memory fallback.
The existing parser API's boolean persistence setting and its historical
fallback for nonseekable input are unchanged. The upstream storage
wrappers and validation routine contain no inline implementation comments;
the native capability-probe adaptation is documented inline.

Each normal and ASan/UBSan run checks 49,152 actual-source storage cases:
48,864 match exactly and 288 exercise the established default-header-limit
difference. Those 288 also match exactly with an explicit native limit.
There are no unexpected differences. Coverage includes entity/message
loading, both storage modes, nonseekable rejection, nonzero positions,
one-byte and buffered reads, depth/header limits, binary and encoded
content, specialized entities, nested messages, truncated multiparts,
Unix/DOS output and newline completion. Controlled byte-mutation probes
distinguish copying from source references; normal Source use still
requires unchanged input. The earlier 15,360 default stream/file cases
also retain their exact matches and 120 established limit differences
in both builds.

Native regressions check retained-source lifetime, caller ownership,
descriptor-backed source loading, read failures at every byte with and
without errno, seek/substream failures, parser options and warnings,
argument validation and repeated serialization. Focused reader/model/
mbox tests pass with sanitizer leak checking. Full core testing remains
at 34/35 programs with the two established text-format assertions failing.
Full build, GIR, typelib, VAPI and GTK-doc generation pass with existing
warnings. The enum values, both loading methods and their annotations
are verified in generated bindings and reference entries. Live GI/Python
checks cover both modes, lifetime, caller ownership, binary content,
empty-input errors and nonseekable pipe rejection.
Native Windows validation and async/cancellation remain pending.

### Explicit parser storage and checked stream replacement

g_mime_parser_new_with_stream_and_storage() and
g_mime_parser_init_with_stream_and_storage() port MimeKit's explicit
MimeContentStorage constructors and SetStream() overloads. Both take an
explicit format and storage mode. Parser options retain the native
per-operation convention. Message/entity, mbox and the existing native
MMDF format are supported. The legacy constructors, boolean persistence
setting and stream replacement API retain their existing behavior.

The checked APIs share source-capability validation with entity/message
loading. Memory mode never probes seeking; Source mode requires working
tell and a zero-offset current-position seek before accepting input.
Substream failures are reported while parsing content. Stream replacement
validates before discarding anything: failure preserves the old input,
buffered bytes, sticky error, positions, mailbox metadata, format and
storage. Success replaces the input, format and storage, clears parsing
state and retains header-regex settings. Neither stream is reset or closed.

Previously parsed models retain their original content across parser
replacement or destruction. Source-backed content retains its old input,
which must remain open and unchanged while used. Memory-backed models
do not retain input after the parser releases it. Empty input remains
valid parser input: constructing an entity/message returns NULL without
an error at EOF, unlike the convenience Load() helpers.

The corresponding upstream constructors, SetStream() wrappers and
OnSetStream() helper contain no inline implementation comments.
The existing native capability-probe comment is retained in the shared
validator rather than duplicating that logic in each API.

Each normal and ASan/UBSan run checks 98,304 actual-source parser cases:
97,728 match exactly and 576 exercise the established header-limit default
difference. Those 576 also match with an explicit native limit. There are
no unexpected differences. Both constructor and replacement paths cover
the two storage modes, parser limits, nonseekable rejection, nonzero
positions, short reads, binary and encoded content, specialized entities,
nesting, truncated input and repeated serialization. Replacement cases
first trigger a parsing failure to verify recovery. The reference's EOF
exception is normalized for comparison without changing native EOF.
The earlier 49,152 explicit-storage and 15,360 default stream/file cases
also retain their documented matches and default-limit differences in
both builds.

Native tests cover sequential mbox/MMDF parsing, all storage transitions,
retained old models, failed replacement with buffered mailbox input,
sticky-error preservation/recovery, metadata reset, callback retention,
reentrant replacement rejection and argument validation before I/O.
Focused reader/model/mbox tests pass with ASan/UBSan leak checking.
The full core baseline remains 34/35 programs, with the same two
text-format assertions failing. Full build, GIR, typelib, VAPI and
GTK-doc generation pass with existing warnings. Both new APIs' signatures,
ownership, errors and reference entries are verified, including live
GI/Python mailbox iteration, pipe rejection, replacement and lifetime
checks. Existing loader bindings also pass their live checks.
Native Windows validation and async/cancellation remain pending.

### Part content-type constructors

GMimePart now exposes new_with_content_type() and
new_with_content_type_string(), porting MimeKit's ContentType and string
constructor overloads. The model constructor retains the supplied object
without cloning or reparsing it, preserving parameter identity and
formatting metadata. Shared models notify all owning parts; releasing one
part disconnects only that owner's notifications. Callers can release
their model reference immediately after construction.

The string constructor uses the existing all-or-nothing Content-Type
parser with optional parser options and GError reporting. Malformed input
returns NULL with GMIME_ERROR_PARSE_ERROR rather than substituting a
default type or retaining partial parameters. Neither constructor selects
an entity subclass: even text, multipart, and message media types produce
an empty GMimePart, as in MimeKit. The default and type/subtype constructors
retain their existing behavior.

### Part filename property

GMimePart's get_filename()/set_filename() use MimeKit's FileName parameter
selection and assignment semantics without edge trimming.
The getter selects Content-Disposition's filename
parameter before Content-Type's name, falling back only when filename is
absent. It preserves the exact parameter value without modifying the
parameters or raw headers. Empty and all-whitespace filenames do not
trigger the fallback. Filenames remain untrusted and are not
sanitized for filesystem use.

The native getter still returns a borrowed UTF-8 string. It refreshes an
owned copy from the live models on each lookup, including direct
parameter edits and model/header replacements. Equal results preserve the
borrowed pointer; a different result or part finalization invalidates it.

The setter preserves the supplied text in both filename and name parameters.
A non-NULL value creates an attachment disposition if necessary; existing
dispositions and parameter metadata are preserved. NULL removes both
parameters without creating a disposition or removing an existing one.
Empty strings remain explicit parameter values. Invalid UTF-8 leaves the
part unchanged and reports a GLib precondition failure. Borrowed getter
values and parameter substrings are safe to assign back, including when
the first parameter update releases the source of the second update.
These semantics apply to GMimeTextPart as well as GMimePart.

### Content-MD5 property

GMimePart's get_content_md5()/set_content_md5() now follow MimeKit's
ContentMd5 property's lazy first-header lookup and assignment semantics.
Reads trim surrounding ASCII linear whitespace without rewriting raw headers.
Matching header additions, edits, removals and clearing invalidate the
cache; later duplicates never override the first value. An absent header
returns NULL, while values empty after trimming return "". VT, FF and
Unicode whitespace are preserved.

Assignments compare against the loaded cache before trimming. Equal
assignments preserve raw headers and duplicates; assignment before a read
replaces duplicates even when the first header already has the same value.
NULL removes all matching headers unless the loaded cache is already NULL.
Borrowed getter values and substrings remain safe to assign back. The same
behavior applies to GMimeTextPart.

Verification resolves the lazy property before checking for a digest,
preserving GMime's ability to verify parsed or edited headers without a
preceding getter call. Empty first headers still short-circuit without
reading content, even if a later duplicate contains a valid digest.
Digest computation, error propagation and source-stream behavior are
unchanged; property assignment does not compute or validate a checksum.

### Transfer-encoding names

g_mime_content_encoding_from_string() now ports MimeUtils.TryParse() for
Content-Transfer-Encoding names. It skips leading ASCII linear whitespace
(SP, HTAB, CR and LF) and matches the first token case-insensitively,
stopping at linear whitespace or a semicolon. VT, FF and Unicode whitespace
are not skipped or accepted as delimiters, matching current MimeKit.
Trailing text after either delimiter is ignored,
including the semicolon suffix sometimes added by Google Docs. Comments
are not skipped; "base64 (comment)" is recognized, but "base64(comment)"
and "(comment) base64" are not.

The uuencode, x-uuencode and x-uue spellings remain supported. GMime also
retains its historical 7-bit and 8-bit aliases as a deliberate compatibility
extension to MimeKit. They use the same case-insensitive matching and
linear whitespace/semicolon delimiters as the canonical 7bit and 8bit
spellings; emitted encoding names remain canonical.
NULL, empty names and unrecognized tokens return
UNSPECIFIED. The internal parser scans bytes within the supplied length;
non-ASCII and malformed UTF-8 bytes before a delimiter cannot match a
supported name. Generic header decoding preserves non-linear padding
before decoded-header consumers call this helper.
Named encoder/decoder filter factories now reuse this shared
helper instead of allocating and parsing a temporary token.
This applies to legacy parser consumers of the shared helper.

### Part transfer-encoding property

GMimePart's get_content_encoding()/set_content_encoding() now follow
MimeKit's ContentTransferEncoding property semantics. The getter lazily
selects only the first matching header; an unrecognized first value does
not fall back to later duplicates. Matching header events invalidate the
cache, while reads preserve raw headers. The 7-bit and 8-bit compatibility
aliases described above remain supported.

Assignments use canonical names and replace later duplicate headers.
UNSPECIFIED removes all matching headers. Assigning the cached value
preserves raw headers and duplicates, including malformed headers after
a getter returned UNSPECIFIED. Before a read, even an UNSPECIFIED assignment
removes matching malformed headers. Invalid enum values are GLib
precondition failures and leave both headers and lazy state unchanged.
The backing content's source encoding is never changed by assignment.

Serialization and legacy encode() resolve lazy state before using the
selected encoding. Existing OpenPGP operations retain their explicit
cached encoding overrides. This increment does not replace legacy
preparation or serialization policy with MimeKit's Prepare()/WriteContent().

### Decoded content streams

g_mime_content_open() ports MimeContent.Open(), returning an owned
GMimeStream that decodes content on demand. It rewinds the backing stream
before returning, without reading any content or making a full decoded
copy. Base64, quoted-printable, and UUEncode use the existing incremental
decoders; default, 7bit, 8bit, and binary data pass through unchanged.

Each reader captures the current encoding and retains the backing stream,
so it remains valid after the GMimeContent is finalized or its stream is
replaced. Closing or releasing a reader leaves the backing stream open
and does not rewind it. Readers share the backing stream's position:
opening another reader, copying the content, or directly accessing that
stream can interfere with an existing reader. This is not an independent
snapshot or a seekable decoded stream; reset() rewinds and resets decoding.

Open failures return NULL and read failures return -1, with errno set by
the backing stream. Filtered reads now continue through input chunks that
produce no output, avoiding premature EOF during incremental decoding.

### Raw and decoded content copying

GMimeContent now follows MimeKit's WriteTo()/DecodeTo() distinction:
write_to_stream() copies the original backing stream verbatim, while
decode_to_stream() copies decoded bytes. The latter returns the number
of bytes emitted by its decoder, not the encoded input size or a
downstream filter's transformed size. All existing decoded consumers,
including attachment extraction, text decoding, checksums, re-encoding,
and crypto input preparation, now use decode_to_stream().

Both methods rewind before copying and again on success, write at the
output's current position, and neither close nor flush the caller's streams.
Only decode_to_stream() flushes its decoder state. Read and write failures
return -1 with errno set and leave the source at its current position,
matching MimeKit's I/O-failure behavior rather than attempting a cleanup
rewind. A final rewind failure also returns -1, even if output was already
written. Short writes are completed; a zero-byte write fails with EIO.

Both operations have GObject virtual methods. The default decoder calls
the virtual raw writer through an output-side decoder, preserving subclass
dispatch. open() continues to read the backing stream directly, as in
MimeKit, without routing through either copy method.

Direct calls are verified through GI/Python and Vala, and Vala subclasses
can override both copy methods. Vala namespace metadata preserves the
GMIME_* class macros separately from the g_mime_* function prefixes, so
base-method chaining uses the library's existing class-macro names.

### Content checksums and MIME boundaries

Content-MD5 generation/verification and multipart boundary generation now
reuse the stateless, unwrapped Base64 codec. Content-MD5 values retain their
existing padded, newline-free representation and text CRLF canonicalization.
Automatically generated boundaries now match MimeKit's shape: "=-" followed
by the complete padded Base64 encoding of 16 random bytes (26 characters
total). The legacy step-only helper emitted just 20 Base64 characters and
discarded the last random byte. Explicitly supplied boundaries are unchanged.

### Encoding validators

Base64, quoted-printable, and UU encoding validators implement a private
GMimeEncodingValidator GObject interface. They consume incremental byte
ranges and report codec violations with exact stream offsets and one-based
line/column numbers, except that a begin-marker mismatch on LF retains
MimeKit's next-line, column-zero diagnostic. Per-line diagnostic latches
survive write boundaries; Base64 stops checking after invalid padding or
data following padding.
Flush reports incomplete input without resetting or closing the validator,
so repeated flushes repeat an outstanding final diagnostic, as in MimeKit.
Unlike the tolerant decoders, these validators report defects without
producing decoded output. Base64 and quoted-printable validation do not
impose a separate line-length limit.
The UU validator checks begin/file-mode/end markers, declared payload
lengths, invalid payload characters, incomplete lines, and extra data.
It stops on a failed end-marker match or non-whitespace after "end" without
consuming the offending byte. Partial payload quanta survive line breaks,
matching MimeKit's malformed-input recovery.

The private diagnostic callback and its user data are borrowed for the
validator's lifetime; the reported issue is borrowed for the callback only.
NULL input is accepted only for zero-length writes. Stream offsets are
nonnegative and checked against signed 64-bit overflow before consuming
input; line/column counters use 64-bit unsigned values rather than wrapping
MimeKit's 32-bit counters. The UU file-mode digit counter is also widened:
unlike MimeKit's byte-sized counter, it cannot wrap after 255 digits and
accidentally accept an overlong mode or move its diagnostic. Overlong
modes consistently report the fifth digit regardless of write boundaries.
Base64 validation and quoted-printable literal scanning use runtime-selected
x86/x64 SSSE3 kernels with portable scalar fallbacks. Base64 scans count
alphabet bytes and line feeds up to the next special byte; quoted-printable
scans find the next equals sign or LF. Loads require sixteen readable bytes,
and scalar tails preserve exact diagnostics and per-line latch resets.
The public compliance framework and reader wiring are described in the
synchronous reader-completion section above. Public parser integration and
its warning compatibility are described separately above.

Focused validator regressions cover diagnostic positions, every split in
representative inputs, byte-at-a-time writes, empty writes, repeated flushes
and continued input, preconditions, offset overflow, wide line counters,
and read-only guard-page bounds. Both test-encoding-validators and its
forced-scalar counterpart exercise byte classifications, unaligned input,
vector boundaries, and line-feed masks against an independent scan oracle.
A targeted comparison against MimeKit covers 12,591 fixtures in dispatched
and scalar-only builds, with only three intentional differences caused by
the UU file-mode counter correction. It includes all byte values at key
codec states and vector lanes, reference vector-width boundaries, split
markers and payload quanta, binary pretext/content, flush-and-continue at
every block prefix, and file-mode counter rollover. Clean-build and native
Windows validation remain pending.

### Content/filter ownership and error handling

GMimeFilterBasic owns an encoder or decoder rather than a legacy state
structure. Its enum-based constructor remains a convenience factory.
new_with_encoder()/new_with_decoder() retain the supplied object's state;
filter copies use independent reset clones, preserving codec configuration.

Existing ownership semantics are retained during the naming migration:
GObjects are reference-counted; getters generally return borrowed values,
and constructors return owned references, as documented by each function.
Recoverable operational failures map to explicit C error results and
GError; invalid API arguments use GLib precondition checks. Recovery from
malformed MIME is distinct from I/O failure and must not hide the latter.
Intentional differences from MimeKit must be documented and covered by
regression cases.

### Synchronous API scope

GMime 4.0.0 will not add cancellation or asynchronous APIs. Keep synchronous
operation signatures without GCancellable; neither GAsyncResult/GTask
wrappers nor cancellable overloads are release requirements.

### SIMD acceleration and differential validation

SIMD acceleration is part of the port, including byte searches, MIME
content/boundary scanning, and Base64 and uuencode codecs. Port these
fast paths with their corresponding primitives, retaining portable
scalar implementations. Select CPU-specific kernels at runtime without
raising the library's baseline instruction-set requirements, and preserve
upstream algorithm attribution and licenses.

Scalar and accelerated paths must agree on output, incremental state,
malformed-input recovery, and error reporting. Exercise both paths across
chunk boundaries, vector-width tails, unaligned input, and exact buffer
bounds. Benchmark representative MIME workloads to verify that the
accelerated paths improve performance.

Base64 and uuencode have runtime-selected x86/x64 SSSE3 kernels and
portable scalar fallbacks. Base64 decoding also has an AVX2 loop ported
from MimeKit: 32 input characters produce 24 bytes, and the twelve-character
tails of 76-character MIME lines produce nine bytes without leaving the
loop for LF or CRLF handling. Loads require a full readable vector; stores
write only result bytes, never vector padding. Other whitespace, garbage,
padding, and incomplete quartets retain scalar recovery behavior.
Base64 encoding also handles nine-byte line/input tails with SSSE3, ported
from MimeKit. An overlapping load/store re-encodes the preceding triplet
only when it was vectorized in the same call and line. This removes the
scalar tail from standard 76-column MIME lines and 60-column lines without
changing streaming state, padding, LF/CRLF output, or uuencoding.
test-codecs and test-codecs-scalar run the same codec regressions against
the dispatched and scalar implementations, including exhaustive MIME-tail
byte mutations, streaming and in-place decoding, and guard-page bounds.
Encoder-tail coverage includes both newline formats, 60-76-column widths,
every first-line chunk split, empty calls, clones, and output canaries.
Both accept --benchmark for the same one-MiB Base64/uuencode workload.
The --benchmark-base64 mode compares 4096-byte streaming chunks across
unwrapped, MIME LF/CRLF, short-line, garbage-containing, and small inputs.
The --benchmark-base64-encoder mode uses preallocated buffers to compare
MIME LF/CRLF, 60-76-column lines, bulk and small chunks, and short inputs,
checking output against an independent reference before timing.
AVX2 encoding, AVX-512 and ARM64 codec kernels remain subsequent SIMD work.
MIME content scanning now has SSE2, AVX2 and ARM NEON kernels, as described
in the synchronous reader section above. Fused header byte searches remain
deferred.

The initial differential run covered 61,200 fixtures against MimeKit,
comparing emitted bytes, per-call output estimates/counts, and clones
every third operation. Both dispatched and scalar-only builds matched.
The fixtures include zero-length chunks, partial final input, all standard
MIME codecs, malformed input, decoder modes, and configured line lengths.
Repeated local x86-64 benchmarks showed roughly 3-6x higher throughput in
the SSSE3 paths than in the scalar-only build; results are machine-dependent.

The expanded codec differential covers 98,208 fixtures, including 37,008
yEnc cases. Both dispatched and scalar-only builds match MimeKit on output,
per-call estimates/counts, and state-preserving clones. The yEnc cases also
compare raw checksums after each operation and exercise repeated flushes,
configured lines up to 998 columns, malformed markers, CRLF, full-block
and payload-only decoding, and arbitrary chunk boundaries.

A separate percent-codec differential covers 211,296 codec fixtures and
1,977 parameter fixtures with no differences from MimeKit. It includes
all 65,536 possible byte pairs following a percent sign, binary input,
mixed-case hex, zero-length chunks, buffered-state clones, all split
positions in representative continuation values, reordered segments, and
encoded/unencoded transitions. Charset conversion remains outside this
codec comparison; parameter fixtures use UTF-8.

The RFC 2047 encoder slice has focused dispatched/scalar regressions for
byte values, Base64 tails and alignment, repeated calls, checked estimates,
preconditions, overlap, and guard-page bounds, plus MIME/header regressions.
A targeted header-decoding differential covers 492 fixtures against MimeKit
with no differences in dispatched or scalar-only builds. It covers text
and phrase decoding in strict and loose modes, malformed recovery, split
payloads and UTF-8 sequences, empty words, decoder reset boundaries, and
long runs of single-character payloads. Full header-policy differential,
clean-build, bindings, and cross-platform validation remain pending.

## Message/partial parameter properties

MessagePartial construction and its Id, Number, and Total properties now
follow MimeKit. Construction requires a non-NULL UTF-8 id (empty is allowed),
number >= 1, and total >= number. The getters read live Content-Type
parameters, including direct parameter edits and header/type replacement.
The id is borrowed; absent or invalid numeric values return -1 instead of
MimeKit's nullable integer. Zero remains valid when reading parameters.
Integer parsing accepts decimal digits and surrounding SP, HTAB, CR or LF,
but rejects signs, VT, FF, Unicode whitespace, trailing garbage, and
values above Int32.MaxValue without changing the parameter or raw header.

The obsolete public id/number/total cache fields were removed from the
GMimeMessagePartial struct for GMime 4; callers must use the getters.
Splitting generates a shared partial id when Message-ID is absent, without
changing the original message. Reconstruction validates ids and numbers
before sorting, including MimeKit's corresponding comparator precondition
comment. Existing applicable split implementation comments are retained.
The remaining split/join algorithm and header-policy differences are not
part of this increment.

The normal library, GIR, typelib, and Vala bindings build successfully.
Related reader, parameter, type, visitor, and partial-message tests pass.
All 49,807 numeric parameter cases match actual MimeKit source in normal
and ASan/UBSan builds. Focused partial regressions also pass leak checking.
The complete partial fixture test passes ASan/UBSan with leak detection
disabled; its older fixture harness reproduces the same existing leaks
(355,174 bytes in 2,599 allocations).

## Message/partial fragment cloning and split sizing

Split fragment envelopes now follow MimeKit's CloneMessage helper: parser
options are preserved using GMime's owned snapshots, headers are cloned
with their binary raw bytes intact and unknown stream offsets, and all
original Message-ID duplicates are replaced by one canonical, fresh header
at the first matching position. Every envelope has its own Message-ID,
distinct from the id shared by the message/partial bodies. Header edits
and parser-option changes remain independent of the source and siblings.
Applicable MimeKit whole-line splitting comments are retained.

Splitting rejects zero sizes and missing output pointers. Oversized native
size_t targets, including G_MAXSIZE, no longer narrow to a negative signed
length. An unnecessary split returns an owned reference to the original
message without changing its id. Whole-line splitting preserves MimeKit's
boundary behavior: an LF at the target boundary is included, so a fragment
can contain max_size + 1 bytes.

Focused regressions cover preconditions, duplicate replacement, binary raw
headers, parser options, offsets, ownership, independence, exact-size and
oversized targets, byte concatenation, and reconstruction. All 2,101
actual-source MimeKit differential cases match exact fragment bytes,
boundaries, unsplit identity, and tested metadata contracts in normal and
ASan/UBSan builds with leak checking. The comparison aligns DOS newlines
and uses headers retained in MimeKit's encapsulated payload. The normal
library and bindings build and the five related suites pass; focused
partial tests pass sanitizers with leak checking. Complete partial
fixtures pass sanitizers with leak detection disabled for the previously
documented fixture-harness leaks. Native C validation was under WSL;
native Windows execution remains pending.

This initial increment left the native payload-header policy unchanged.
The later "Message/partial Split and Join API migration" section supersedes
that policy and completes the outer-message/header-policy migration.

## Message/partial reconstruction content

Reconstruction now follows MimeKit Join's content path: it sorts a private
copy of the fragment array, opens each present content through its decoded
reader, and skips absent content. The caller's array is unchanged on both
success and failure. Applicable MimeKit chain and comparator comments are
preserved. Existing id, sequence, and final-total validation remains; only
the last fragment must provide total.

Decoded readers cannot provide arbitrary source substreams. Reconstruction
therefore explicitly selects memory content storage, matching MimeKit's
default parser storage, instead of trying to persist decoded substreams.
The returned message owns its parsed body bytes independently of fragment
streams; this copies body content into memory rather than retaining source
views. Failed content opening stops reconstruction and releases earlier
decoded readers. The legacy native return remains NULL for malformed or
unreadable input. NULL arrays are accepted only for zero fragments; NULL
or wrongly typed entries use native precondition diagnostics. Nullable
input/output annotations are included in the generated bindings.

Focused regressions cover mixed raw, 7bit, 8bit, binary, Base64 and
quoted-printable content, arbitrary fragment boundaries, absent first,
middle and last content, missing nonfinal totals, repeated reconstruction,
unchanged input order, malformed metadata, absent content streams, failed
rewinds, cleanup and returned-body lifetime. All 1,384 actual-source
MimeKit comparisons match decoded bodies, selected inner headers, failure
outcomes and input-order preservation in normal and ASan/UBSan builds
with leak checking. These comparisons additionally exercise uuencode,
empty bodies, empty streams, shuffled fragments and random binary data.
They use only inner headers unaffected by MimeKit's CombineHeaders policy.

The normal library, GIR, typelib and Vala bindings build, and the six
related partial, concatenated-stream, reader, parameter, type and visitor
suites pass. Focused partial tests pass ASan/UBSan and leak checking;
complete partial fixtures pass with leak detection disabled for the known
fixture-harness leaks. Native C validation was under WSL, not native
Windows.

This content-only increment left the public reconstruction signature,
encapsulated-header policy and nullable-id handling unchanged. The later
"Message/partial Split and Join API migration" section supersedes those
limitations.

## Nullable part content

MimeKit's nullable MimePart.Content assignment is now supported by
g_mime_part_set_content(): NULL clears the content without a GLib critical.
The setter retains assigned content, publishes the replacement before
releasing the previous reference, and invalidates the native OpenPGP cache
on changes. Reassigning the same content remains a no-op. Wrongly typed
objects produce a native precondition diagnostic without changing content
or cache state. Both content accessors have nullable introspection metadata.
The source property has no inline implementation comments to transfer.

Regressions cover ordinary, text and partial parts: initial and repeated
NULL assignment, borrowed self-assignment, replacement, clearing, external
references, backing-stream lifetime, finalization, OpenPGP cache behavior,
and observation of replacement content during old-content finalization.
Cleared parts serialize an empty body and retain their raw headers.
Reconstruction tests now clear actual fragment content instead of creating
replacement fragments to represent missing content.

The normal library, GIR, typelib and Vala bindings build, and the part and
partial suites pass. Isolated assignment and focused partial tests pass
ASan/UBSan with leak checking; both complete fixture suites pass with leak
detection disabled. The complete part fixture test still reports 1,168
bytes in five allocations in unchanged text/format-options paths; the
unchanged HEAD test reproduces the same report against the current library.
The partial fixture-harness leaks remain as previously documented. Live
Python GI checks cover nullable metadata, assignment, caller-reference
release, clearing and empty serialization for all three tested part types.
Native C validation was under WSL, not native Windows.

## Message/partial Split and Join API migration

The legacy split and reconstruction entrypoints have been replaced, without
compatibility wrappers, by g_mime_message_partial_split() and
g_mime_message_partial_join(). All repository callers are migrated. Split
now accepts a GError output. Join accepts parser options, the first numbered
fragment's enclosing GMimeMessage, the fragment array and a GError output.
NULL options use the defaults, corresponding to MimeKit's default-options
overload. Introspection exposes Message.partial_split() and
MessagePartial.join(), including owned arrays/results, nullable arguments
and exception propagation.

Join ports MimeKit's CombineHeaders implementation and its applicable
inline RFC 2046 comments. Inner envelope fields other than Subject,
Message-ID, Encrypted and MIME-Version are discarded. Ordinary outer
envelope fields are cloned in order, preserving raw names and binary values.
Each outer special field positions the next matching inner special field;
unmatched inner duplicates remain at the end. Outer special values do not
replace missing inner values. Joined envelope and root-body header offsets
are cleared; descendant offsets are not. Options and headers remain owned
snapshots, independent of the source messages.

Split clones the default format options and suppresses ordinary envelope
fields, keeping the four special fields and known Content-* fields in the
encapsulated payload. Unknown header ids are suppressed as a group,
including unrecognized Content-* names. Filtering applies to merged message
headers, including embedded messages, not standalone header lists or
ordinary descendant entity headers. Header-list writing has been aligned
with MimeKit to preserve those descendant fields. Bodyless messages also
use unfiltered standalone lists, as in MimeKit. Multipart/signed children
clear hidden-header settings, including the private unknown-id flag, so
their embedded messages retain their headers. The original message and
global format options are not changed. The no-split identity optimization
is based on the filtered payload's size, not the full original wire size.

Matching absent fragment ids are accepted; absent and empty ids remain
distinct. Invalid numbers, sequences, totals and mismatching ids report
GMIME_ERROR_INVALID_OPERATION. Empty fragment arrays return NULL without
error. Present fragments with no reconstructable message report a parse
error, including when all content is absent. Decoded content is loaded
through the existing memory-backed message loader; the returned body
outlives all source fragments. Stream and serialization failures report
errors and clean up temporary ownership. Split clears its output count
before failure. Counts above Int32.MaxValue are rejected before array
traversal or fragment-number narrowing.

The complete normal library, GIR, typelib and Vala build passes. Nine
related suites pass: partial, MIME part, headers, concatenated streams,
reader, types, visitor, MIME and iterator. Actual-source MimeKit comparison
covers 5,149 cases in both normal and ASan/UBSan builds with leak checking:
2,473 exact Split payload/identity/metadata cases and complete Split/Join
message comparisons, 1,292 raw Join header/order/offset/ownership cases,
and 1,384 decoded-content reconstruction cases. Fixtures include duplicate
and missing special fields, folded and binary raw headers, nullable ids,
malformed metadata, empty content, bodyless messages, nested messages,
mixed/signed multiparts, unknown fields and header-heavy identity cases.

Focused partial and isolated nullable-content regressions pass ASan/UBSan
with leak checking. Both complete part/partial fixture suites pass with
leak detection disabled for the previously documented unchanged leaks.
Live Python GI verifies the new API round trip, identity, nullable
ids/content, header offsets, unchanged caller order, errors and independent
body lifetime. Native C validation remains under WSL, not native Windows.

## ASCII MIME whitespace normalization

Ports MimeKit commit 20ab449ec5ee334efd6c5d909424ee61971267d5.
Shared trimming and token comparison recognize only SP, HTAB, CR and LF.
Content-Description, Content-MD5, decoded address names, disposition text,
flowed format, message priority and cryptographic protocol lookup preserve
VT, FF and Unicode whitespace rather than treating them as padding.
Message/partial numbers reject non-linear padding, retaining unsigned
Int32 parsing and live parameter reads. Filename lookup preserves the exact
Content-Disposition filename or fallback Content-Type name without trimming.

Existing header unfolding, message-ID normalization, URI and duration
parsing, transfer-encoding names, report types and international address
folding already use the required linear-whitespace rules. Persistent
regressions cover these surfaces, filename/boundary round trips and protocol
lookup without rewriting its source parameter.

GMime retains RFC 2046 boundary validation and NULL-to-generate behavior,
including preservation of valid leading spaces. Its permissive disposition
parser still retains arbitrary disposition text rather than adopting
MimeKit's invalid-token fallback. There is no native TNEF converter or
Authentication-Results model to update in this increment.

The WSL Meson build and dedicated whitespace suite pass. Of 46 suites,
42 pass; the reader, partial-message, mbox and PGP suites reproduce their
same failures against unchanged HEAD. Native Windows execution remains
unvalidated.

## Public parser compliance

GMimeParser exposes get_compliance_options(), set_compliance_options() and
set_compliance_logger() for header, entity and message construction. These
forward to the existing reader's diagnostic engine and follow its native
notified callback API.

The options getter returns borrowed mutable settings, available before input
is assigned and valid until options are replaced or the parser is released.
The setter copies settings, supports self-assignment and restores defaults
when passed NULL. Input replacement preserves options and logger while clearing
buffered input, parsing errors and model metadata. Failed checked replacement
preserves existing state; format and content-storage changes also retain the
compliance configuration.

Each construction operation snapshots its options and logger. Callback changes
apply to the next operation, and replaced logger data survives until the active
operation ends. Diagnostic budgets reset per operation/message. Disabling
reporting requires NULL callback, data and destroy notifier. Without a logger,
compliance validation is skipped. Reporting does not change model construction,
legacy warnings or fatal syntax, header-limit and I/O error handling.

Callbacks may inspect the parser, change compliance settings or release their
parser reference. Reentrant parsing, input replacement and manipulation of the
underlying stream during callbacks remain prohibited. One private model reader
is retained across input replacement to preserve configuration and callback
ownership, without changing no-input position, EOF or storage behavior.
The parser fuzz target now exercises this public API for its logged path
rather than bypassing the wrapper through the private model builder.

## Remaining migration phases

This roadmap supersedes the original phase list. It reflects a source-level
comparison of GMime 28aec310 with MimeKit.Core 20ab449e, not a declaration of
complete member-by-member or behavioral parity. Earlier sections retain
historical implementation decisions and validation results; their pending-work
and failure notes may have been superseded by later changes.

### Completed foundations

The public parser already uses the explicit-stack reader and model builder;
synchronous serialization uses the iterative writer. Specialized alternative,
related and report multiparts, delivery/disposition/feedback reports,
text/rfc822-headers, attachment collections, body building, Received models,
anonymization and message/partial reconstruction are implemented. MIME
compliance diagnostics, options, budgets and reader/parser callbacks are
implemented.
Runtime-dispatched content scanners and selected transfer-encoding kernels,
scalar fallbacks, reader reference fixtures and parser/reader, codec and
RFC 2047 fuzz targets also exist. These are not outstanding whole-subsystem
ports.

### Application-facing API completion

- Add decoded message TextBody, HtmlBody and GetTextBody(format) equivalents.
  Reuse specialized multipart selection; the legacy get_body() entity-selection
  helper is not the decoded-text API. Flowed-text conversion is deferred with
  the standalone text utilities.
- Complete TextPart encoding detection: detected charset and confidence,
  HTML meta charset discovery, and reporting the charset used by decoding.
  Explicit charset conversion and BOM detection already exist.
- Port the public SMTP and SMTPUTF8 address validators and their options and
  error models. The reader's MIME address compliance validator is a different
  API, not a replacement for these utilities.
- Add AuthenticationResults, AuthenticationMethodResult and
  AuthenticationMethodProperty. These models belong to MimeKit.Core despite
  their Cryptography namespace; they do not require a new crypto backend.
- Add a typed message MIME-Version accessor. Sender and Resent-Sender
  intentionally remain address lists, not single-mailbox properties.

### API contract and settled 4.0.0 decisions

- Continue aligning public APIs while retaining intentional, documented native
  differences. Resolve body accessor naming and review stream close/ownership
  contracts before the GMime 4 API/ABI is frozen.
- Legacy parser persistence accessors are removed in favor of explicit content
  storage at construction or stream reinitialization. Internal snapshots remain
  independent despite reference-counted parser, format, and compliance options.
- Skip cancellation and asynchronous APIs for 4.0.0. Preserve existing
  synchronous error reporting and shared-source lifetimes.
- Skip parser-options-local MIME type registration; retain the existing global
  entity type registry unchanged.
- Keep raw multipart boundary markers private unless users explicitly request
  public access. Internal byte-preserving storage and serialization remain.

Managed collection CopyTo(T[] array, int offset) convenience APIs, including
Multipart.CopyTo(), are intentionally excluded from the port. Stream and
content copying APIs are unaffected.

### Deferred subsystems and release integration

- Exclude standalone text utilities from 4.0.0: flowed text,
  HTML parsing/conversion/filtering, text previews and RTF conversion.
  These may be considered for a later 4.x release.
  Existing HTML/enriched filters cover only part of this scope.
- Exclude TNEF reading/writing, properties, attachment extraction, MIME/calendar
  conversion and compressed RTF from 4.0.0; reconsider in a later 4.x release.
- Promote the broader historical actual-source differential comparisons into
  reproducible repository tooling with pinned reference revisions and explicit
  expected differences. The checked-in reader reference generator and
  round-trip fixtures are a foundation, not the entire parity harness.
- Add native Windows full-library integration. Preserve Linux/macOS,
  optional-feature, sanitizer, installed C, GIR and Vala integration coverage.
- Require GNU libiconv >= 1.12 on macOS, including for checked conversions
  previously delegated to GLib. Both macOS CI versions use the selected GNU
  backend and check installed C consumers; Apple's native converter is not
  a supported macOS backend. GnuPG test homes use short paths when nested
  temporary directories would exceed Unix-domain socket limits.
- Extend fuzzing beyond the reader/parser, encoder/decoder and RFC 2047
  targets as appropriate. The codec targets cover chunking, clone/reset,
  exact output bounds, scalar-sized equivalence and stream-filter round trips;
  all five targets have corpus replay and nightly libFuzzer integration.
  Additional ARM64/wider codec kernels and fused header scans remain
  performance work, with scalar equivalence and workload benchmarks required.

GPGME remains the OpenPGP/S/MIME backend; porting MimeKit.Cryptography's
implementations is out of scope. DKIM/ARC/DMARC are excluded from 4.0.0 and
may be considered for later 4.x releases.
