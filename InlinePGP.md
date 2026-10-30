# GMime 4 inline-PGP support

## Discovery

```c
GMimeOpenPGPDocument *
g_mime_part_scan_openpgp (GMimePart *part, GError **error);
```

The document owns a stable snapshot of the complete transfer-decoded content,
before charset conversion, HTML rendering, or `format=flowed` processing.
Scanning does no cryptography, prompting, network access, or key import.
It does not replace content or headers, but may reposition the backing stream.
Missing content or I/O failure returns NULL with an error; no candidates
returns a valid document with zero blocks.

The immutable document implements `GListModel` with `GMimeOpenPGPBlock`
items in source order. It also exposes `get_count()` and a borrowed
`get_block(index)` accessor. Reference a borrowed block to retain it.

Each block has a kind, structural status, decoded byte offset, and length:

| Kind | Meaning |
|---|---|
| `GMIME_OPENPGP_BLOCK_MESSAGE` | PGP MESSAGE armor; not necessarily encrypted |
| `GMIME_OPENPGP_BLOCK_SIGNED_MESSAGE` | Clear-signed content and its signature armor |
| `GMIME_OPENPGP_BLOCK_PUBLIC_KEY` | Public-key armor; never automatically imported |
| `GMIME_OPENPGP_BLOCK_PRIVATE_KEY` | Private-key armor; never automatically imported |
| `GMIME_OPENPGP_BLOCK_SIGNATURE` | Standalone signature armor, discoverable but not processed by receiving helpers |

`COMPLETE` means matching structural markers, **not** valid armor, OpenPGP
packets, signatures, or trust. `TRUNCATED` means EOF before completion;
`INTERRUPTED` means a new block started; `MALFORMED` means an unexpected
end marker terminated the candidate.

Only exact, unquoted markers at line starts are recognized, with LF or CRLF
line endings; a final marker at EOF needs no newline. Leading/trailing
whitespace, bare-CR lines, quoted armor, and unknown armor types are not
recognized. The signature within a clear-signed message is not a separate
top-level block. Another beginning interrupts an unfinished candidate without
consuming that new block. Unmatched end markers remain ordinary source bytes.
An end marker's newline, when present, belongs to its block.

## Stable bytes and storage

`g_mime_openpgp_document_open_stream()` returns an independently positioned,
read-only view of the **entire** snapshot. Use block offsets and lengths to
open substreams for unprotected prefixes, gaps, and suffixes. The document's
`get_length()` reports the complete decoded length.

`g_mime_openpgp_block_open_stream()` returns an independent read-only view of
that block's original bytes, including incomplete/malformed content. Its
position starts at zero, not at the block's document offset. Substreams use
absolute coordinates within their originating view, as usual for GMime.
Reads, seeks, and resets report failures through errno. Closing one view
does not invalidate siblings or substreams.

Documents, blocks, and streams retain the shared snapshot independently.
Replacing or destroying the original part cannot change their bytes.
Stream writes are rejected; opening views does not copy the content.

Up to one MiB of decoded data is retained in memory. Larger snapshots
automatically spill to a private temporary file, removed when the last
reference to its storage is released. Temporary-file creation, writing, or
flushing failure fails the scan; there is no silent truncation or fallback
to unbounded memory. Read failures after scanning are ordinary stream errors.
Final cleanup failures produce warnings.

The threshold is an implementation detail, not a limit on accepted content.
Disk usage scales with decoded size and metadata memory scales with the
number of blocks. Applications should impose message-size and processing
budgets appropriate to their environment. Automatic cleanup is not secure
erasure, and a process crash can leave a temporary file behind.

## Receiving with native GPGME

Include `<gmime/gmime-gpgme.h>` and use `gmime-gpgme-4.0`:

```c
gboolean
g_mime_openpgp_block_verify (GMimeOpenPGPBlock *block,
                           gpgme_ctx_t ctx,
                           GMimeStream *output,
                           gpgme_verify_result_t *result,
                           GError **error);

gboolean
g_mime_openpgp_block_decrypt (GMimeOpenPGPBlock *block,
                            gpgme_ctx_t ctx,
                            GMimeStream *output,
                            gpgme_decrypt_result_t *decrypt_result,
                            gpgme_verify_result_t *verify_result,
                            GError **error);
```

Verification accepts complete clear-signed blocks and PGP MESSAGE armor
containing opaque signed data. Decryption accepts complete PGP MESSAGE blocks
and verifies embedded signatures. GPGME determines whether the contents
support the requested operation. Incomplete blocks, standalone signatures,
and key blocks are rejected explicitly.

Contexts are caller-owned, OpenPGP, synchronous, and exclusively borrowed.
Configure keyrings, signers, callbacks, network policy, and session-key flags
directly. These operations preserve caller settings and never mutate the
source part, document, or block.

Process each block independently, retaining its output, results, and errors
separately. A failure leaves original bytes available and does not prevent
processing later blocks. No part-wide decrypt/verify result is provided.

TRUE means operational completion, not valid or trusted signatures. Inspect
every native signature's status, summary, and validity, including missing-key
results and empty signature lists. Native result outputs start as NULL.
Release every returned result with `gpgme_result_unref()`, even diagnostic
results accompanying failure. Results survive subsequent operations and
context release. Discard partial output on failure.

Protection applies only to each processed payload, not neighboring blocks,
unprotected text, MIME headers, or the document as a whole. Decryption alone
does not authenticate a sender. Do not automatically interpret decrypted
bytes as HTML or nested MIME. Charset/display transformations happen after
processing; retain visible protection boundaries and provenance.

Discovery remains available without crypto and through GIR/Vala. Native
receiving calls are excluded from introspection, matching the rest of the
[native GPGME integration](GpgMe-Design-Proposal.md).

## Sending and compatibility

Existing `g_mime_part_openpgp_sign()` and `g_mime_part_openpgp_encrypt()`
already support sending with native contexts and retain their mutating,
whole-part semantics. Existing verify/decrypt helpers are also unchanged.
`GMimeFilterOpenPGP` and the part's single-value classification still describe
the first detected block; they do not silently become multi-block APIs.

To compose independent blocks without mutating a part, use the existing raw
`g_mime_gpgme_sign()` / `g_mime_gpgme_encrypt()` stream helpers and combine
their armored outputs with explicitly unprotected text. Configure armor and
clear-signing mode yourself. Do not rewrap or apply flowed-text transformations
to signed output. New part-copying sending conveniences and recursive
processing are not part of this receiving layer.
