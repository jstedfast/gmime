# Porting to GMime 4

## Migrating from GMime 3.0 (in development)

GMime 4 is being developed as a C/GObject port of MimeKit.Core. The initial
reference revision is MimeKit 04f17021, with GMime 211a0608 as the baseline.
This is an incremental migration, not yet a declaration of feature parity.

- GLib 2.88.0 or newer is required for building and using GMime 4.
  This is the GLib API baseline packaged by Ubuntu 26.04 LTS.

- Meson >= 1.3.0 replaces Autotools. Use `meson setup _build`,
  `meson compile -C _build`, and `meson test -C _build`. Autogen, configure,
  and the Automake files are no longer provided. See README.md for build
  options and optional dependencies. Installed headers, pkg-config, GIR,
  and Vala packages retain their GMime 4.0 names.

- GMimeCertificate and GMimeSignature creation/expiration fields and
  accessors now use GDateTime instead of time_t. Setters retain a reference;
  getters return a borrowed, nullable reference. NULL means unknown.
  An expiration date of 9999-12-31 23:59:59.999999 UTC means never expires;
  construct it with g_date_time_new_utc (9999, 12, 31, 23, 59, 59.999999).
  The Unix epoch is an ordinary date, not a sentinel. Supplied timezones
  and microseconds are preserved; GPGME timestamps are converted to UTC.
  GPGME creation timestamp 0 means unknown and expiration timestamp 0
  means never expires. Invalid or out-of-range backend timestamps now
  fail verification/decryption with a GError rather than wrapping dates.
  The get_created64() and get_expires64() accessors have been removed;
  use g_date_time_to_unix() after checking for NULL instead.

- `GMimeObject` is now `GMimeEntity`. Replace `g_mime_object_*` with
  `g_mime_entity_*`, `GMIME_OBJECT*` with `GMIME_ENTITY*`, `GMIME_IS_OBJECT*` with
  `GMIME_IS_ENTITY*`, and `GMIME_TYPE_OBJECT` with `GMIME_TYPE_ENTITY`.
  Include `gmime/gmime-entity.h` instead of `gmime/gmime-object.h`.

- `GMimeDataWrapper` is now `GMimeContent`. Replace `g_mime_data_wrapper_*` with
  `g_mime_content_*` and the `DATA_WRAPPER` portion of its type macros with
  `CONTENT`. Include `gmime/gmime-content.h` instead of `gmime/gmime-data-wrapper.h`.

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
  Assigned and parsed values are trimmed of Unicode whitespace. Empty values
  are retained, and unchanged assignments preserve existing raw headers.
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
  report limit violations. The legacy recursive parser is still in use;
  the explicit-stack reader and writer have not yet been ported.

- `GMimeMultipartReport` is a `GMimeMultipart` subclass for multipart/report.
  Its constructor requires a UTF-8 report type; get/set_report_type()
  access the Content-Type report-type parameter. The getter is borrowed
  and nullable for parsed reports without the parameter. The setter trims
  Unicode whitespace, preserves case and internal whitespace, and accepts
  empty strings but not NULL. Equivalent assignments preserve raw headers
  and parameter metadata. Direct Content-Type/parameter changes remain
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
  validated, including for pass-through. Names use Unicode whitespace and
  stop at whitespace or semicolon, retaining MimeKit's Google Docs header
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
  This does not port the higher-level MimeAnonymizer or automatically
  rewrite headers/MIME structure. Byte counts and whitespace remain visible.

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
  The legacy boxed GMimePartIter remains unchanged, including its distinct
  initialization, path, backward traversal and editing behavior.

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

- GMimeParserOptions now exposes max_header_length (default 16 MiB).
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
TNEF, standalone text conversion utilities, and the complete structured
compliance framework are deferred.

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

Public dates use GDateTime, including certificate and signature
creation/expiration accessors and public structure members. Document
ownership, timezone, and missing-date semantics for new APIs as well.
Integer timestamps may remain internal where required for backend
interoperability.

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
seed or state-bit API; applications manage framing and aggregate multipart
checksums themselves.

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
Fitting uses actual quoted/encoded sizes and accounts for additional
continuation-index digits, so wrapping can differ from MimeKit's estimates.
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
Word fitting uses actual encoded bytes rather than MimeKit's estimated
widths, so wrapping can differ for escaped or multibyte data. Oversized
parameter names still make the first line exceed the target width, but
the writer always consumes at least one character and makes progress.
The fold flag controls both parameter and encoded-word line breaks.
Wider physical lines can hold multiple words, each independently limited
to 75 characters.

GMimeFormatOptions now exposes get/set_max_line_length(), following
MimeKit's MaxLineLength: default 78, with an inclusive range of 60 to 998.
The setter requires mutable options from new() or clone(), not the global
defaults. Clones preserve the configured length independently of the
newline format. Parameter encoding selection, ordinary parameter folding,
RFC 2231 continuations, and RFC 2047 parameter words honor this setting.
Actual first-word sizing prevents multibyte charsets from overflowing
short lines merely because the initial placement estimate fits.

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
Parsed mailbox and group display names now convert literal tabs to spaces
before RFC 2047 decoding, following MimeKit. Trailing comment names use the
same normalization, including domainless mailboxes. Encoded tabs retain
their decoded value. Phrase names are unquoted and unfolded first; trailing
comment names retain literal quotes, backslashes, and embedded newlines.
Direct phrase encoding, general string unquoting, interior whitespace in
programmatically assigned names, and untouched raw headers retain tabs.
Address constructors and the name setter now trim leading and trailing
Unicode whitespace, following MimeKit, including vertical tab and NEXT LINE.
Interior whitespace is preserved, and NULL remains distinct from an empty
name. Setting an equivalent normalized name does not emit a change event
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
and use case-sensitive lookup; empty and whitespace-only entries are omitted only
when formatting. Clones own independent routes and nested groups.

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
whitespace retained inside the encoded payload. These are size estimates, not
hard output-length guarantees.

RFC 2047 token splitting now collects those metrics incrementally and uses the
configured width and requested charset instead of fixed character-count limits.
Tokens split before the next Unicode scalar would reach the width-minus-one
boundary. Oversized ASCII atoms become encoded words with room reserved for
the wrapper, and unseparated pieces retain their encoded-word type. Stateful
encodings are measured with independent character flushes; supplementary
characters are never split. Charset estimates are collected before mixed-charset
selection, just like MimeKit, so Latin-1 costs, actual requested-charset bytes,
and lossless UTF-8 fallback can differ. This follows MimeKit's approximation
rather than imposing a hard encoded-byte limit. Unsupported charset names
still fall back losslessly, including names too long to fit a wrapper; iconv
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
are never split. Words longer than the target are split over continuation
lines. Shorter first words remain intact even if the field prefix makes
the first line exceed the target, and long whitespace runs can exceed it
by one unit, matching MimeKit. This is not a hard UTF-8 byte-length limit.
If a full or oversized field prefix leaves no room for a long word, GMime
moves the word to a continuation rather than reproducing MimeKit's invalid
substring-range failure. In decoded assignments, literal encoded-word-looking
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
entry. Other long tags are split using tab continuations. Widths count
UTF-16 units, matching MimeKit; GMime keeps Unicode characters intact even
where MimeKit's substring slicing would split a surrogate pair. Signature
values are normally ASCII; this formatter retains UTF-8 text rather than
converting to the requested charset, consistent with GMime's other structured
header formatters. Invalid UTF-8 is rejected without replacing existing
header state.

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
be nonempty printable ASCII excluding colon. Leading and trailing Unicode
whitespace is trimmed and CR/LF characters are removed from the cached value
before invoking the existing field-specific formatter. Empty values are valid;
NULL values, invalid UTF-8, and invalid names are GLib programming errors.

Constructed headers own their strings and an independent default parser-options
snapshot, so no header list is needed to manage their lifetime. Their stream
offset is unknown (-1). They support the existing setters, formatting, display,
serialization, and cloning APIs. This does not change the legacy list
append/prepend/insert normalization or the handling of parsed raw headers.
Field-specific formatting still uses GMime's existing implementations,
including their malformed structured-value recovery and the RFC 2047
formatting differences noted below; strict structured-value constructor
rejection has not yet been ported.

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
Outer Unicode whitespace is trimmed and valid folding line breaks are
unfolded. Empty or incomplete IDs, extra tokens, non-folding line breaks,
invalid UTF-8, and unquoted spaces/tabs in bare IDs are rejected through
GLib's programming-error guards without changing the cache or raw header.
As with other native string APIs, inputs are NUL-terminated, not counted.
Reassigning the current cached ID preserves its raw header; aliased inputs
are safe. Message-ID remains non-nullable. A NULL Content-ID clears the
cache and removes all matching headers, including malformed duplicates.
The header, address, and ID setters share the same Unicode whitespace
classification, including .NET's VT and NEXT LINE additions.

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
trimming Unicode whitespace. X-Priority reads an unsigned Int32 decimal
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
text remains plain text and trims surrounding Unicode whitespace from the
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

Getters lazily read the first matching header and trim surrounding Unicode
whitespace. Invalid first values return NULL without trying duplicates.
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
empty, non-NULL array. The legacy body-guessing and traversal APIs are
unchanged.

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
The legacy GMimePartIter API and its path conventions are unchanged.

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

The getter parses only the first Content-Duration header, accepting ASCII
decimal digits with surrounding Unicode whitespace and leading zeros up to
2147483647. Signs, comments, fractions, and overflow are invalid. A malformed
first header does not fall back to later duplicates. Reads preserve raw
bytes; matching header edits, including later duplicates, invalidate the
lazy cache. This applies to GMimeTextPart as well as ordinary GMimePart.

Changed nonnegative assignments write canonical decimal text and remove
later matching duplicate headers. Assigning -1 removes all matching headers.
Assigning the already cached value preserves original raw headers and
duplicates, matching MimeKit. Assigning -1 before the first read removes
malformed headers; assigning it after a getter returned -1 is a no-op
until matching headers change. Header clearing resets the lazy cache.

### Content description

GMimePart's Content-Description property now follows MimeKit's lazy
first-header lookup and Unicode whitespace trimming. Reads preserve raw
headers, and matching header edits, removals, additions, or clearing
invalidate the cache. Later duplicates do not override the first header.
The getter returns a borrowed UTF-8 string, or NULL for an absent header;
an empty or all-whitespace header returns an empty string.

The setter accepts NULL to remove all matching headers and trims Unicode
whitespace from non-NULL assignments. An assignment equal to the current
cached value preserves raw headers and duplicates; before the first read,
assignment replaces duplicates even when the first value is already equal.
Comparison occurs before trimming, matching MimeKit. Borrowed values,
including substrings of the getter's value, can safely be assigned back.

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

GMimePart's get_filename()/set_filename() now follow MimeKit's FileName
property semantics. The getter selects Content-Disposition's filename
parameter before Content-Type's name, falling back only when filename is
absent. It trims surrounding Unicode whitespace without modifying the
parameters or raw headers. Empty and all-whitespace filenames return ""
and do not trigger the fallback. Filenames remain untrusted and are not
sanitized for filesystem use.

The native getter still returns a borrowed UTF-8 string. It refreshes an
owned trimmed copy from the live models on each lookup, including direct
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
Reads trim surrounding Unicode whitespace without rewriting raw headers.
Matching header additions, edits, removals and clearing invalidate the
cache; later duplicates never override the first value. An absent header
returns NULL, while an empty or all-whitespace first header returns "".

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
Content-Transfer-Encoding names. It skips leading Unicode whitespace and
matches the first token case-insensitively, stopping at any Unicode
whitespace or a semicolon. Trailing text after either delimiter is ignored,
including the semicolon suffix sometimes added by Google Docs. Comments
are not skipped; "base64 (comment)" is recognized, but "base64(comment)"
and "(comment) base64" are not.

The uuencode, x-uuencode and x-uue spellings remain supported. GMime also
retains its historical 7-bit and 8-bit aliases as a deliberate compatibility
extension to MimeKit. They use the same case-insensitive matching and
Unicode whitespace/semicolon delimiters as the canonical 7bit and 8bit
spellings; emitted encoding names remain canonical.
NULL, empty names and unrecognized tokens return
UNSPECIFIED. As a native safety extension, malformed UTF-8 encountered
before a delimiter returns UNSPECIFIED without advancing past its NUL
terminator. Named encoder/decoder filter factories now reuse this shared
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
The public compliance framework and wiring validation into the new reader
remain pending. This slice does not change the legacy parser's warning or
decoding behavior.

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

### Asynchronous APIs

The synchronous port comes first. GIO's GCancellable, GAsyncResult, and
GTask are the intended building blocks for future cancellation and async
APIs; neither capability is implemented by the initial type renames.

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

Base64 and uuencode currently have runtime-selected x86/x64 SSSE3 kernels
and portable scalar fallbacks. SIMD loads require sixteen readable input
bytes; decoding stores only the twelve result bytes, never vector padding.
test-codecs and test-codecs-scalar run the same codec regressions against
the dispatched and scalar implementations, including guard-page bounds.
Both accept --benchmark for the same one-MiB Base64/uuencode workload.
AVX2, AVX-512, ARM64, and MIME content/boundary scanner kernels remain
subsequent SIMD work.

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

## Remaining migration phases

- Continue aligning public APIs with MimeKit while retaining the GPGME
  backend.
- Build a shared-fixture differential harness against the reference
  MimeKit revision, recording intentional behavioral differences.
- Port the remaining parser controls and header/address/parameter parsing
  primitives.
- Port SIMD scanning and transfer-encoding kernels with runtime CPU
  dispatch, scalar fallbacks, differential coverage, and benchmarks.
- Replace recursive MIME parsing with an explicit-stack GMimeReader and
  construct the message model on top of it.
- Port iterative serialization and missing formatting controls.
- Add the missing specialized multipart, report, notification, and
  header-only entity types and message-model helpers.
