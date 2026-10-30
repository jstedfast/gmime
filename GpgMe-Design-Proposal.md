# GMime 4: native GPGME integration

Status: approved direction. This supersedes the earlier concrete-context
proposal. Exact implemented signatures are in `gmime/gmime-gpgme.h`;
[PORTING.md](PORTING.md#8-migrate-cryptography-to-native-gpgme) is the
application migration guide.

## No context wrapper

Remove `GMimeCryptoContext`, `GMimeGpgContext` and `GMimePkcs7Context`.
Do not introduce a `GMimeGpgMEContext`. Synchronous operations borrow a
caller-owned `gpgme_ctx_t` for exclusive use and never release it.

Applications configure protocol, engine/home directory, callbacks,
pinentry, network policy, signing keys, trust policy and session-key flags
directly through GPGME. They resolve recipient keys themselves.
There is no backend registry or implicit context creation during receiving.

GMime remains responsible for MIME serialization/canonicalization,
transfer decoding, protocol validation, `micalg`, MIME packaging/parsing,
and adapting `GMimeStream` to GPGME data handles.

Raw stream helpers honor native context settings. MIME helpers temporarily
set armor/textmode as required and restore them on every exit. Signer lists,
callbacks and other context settings are not silently changed. Signing uses
the context's configured signers, including GPGME's native default-key
behavior when none are configured.

## Native keys and results

| Removed type | Replacement |
|---|---|
| `GMimeCertificate` | `gpgme_key_t`, when an actual key is available |
| `GMimeCertificateList` | Native key arrays or recipient records |
| `GMimeSignature` | `gpgme_signature_t`, borrowed from its result |
| `GMimeSignatureList` | `gpgme_verify_result_t` |
| `GMimeDecryptResult` | Separate `gpgme_decrypt_result_t` and `gpgme_verify_result_t` |

Recipient arrays are NULL-terminated and borrowed. Native keys use
`gpgme_key_ref/unref()`, not GObject references. Key lookup, identity
selection and timestamp conversion are no longer implicit GMime behavior.
Native algorithms, trust values and timestamps retain GPGME semantics.

Each optional result output starts as NULL and receives an independent
`gpgme_result_ref()`. Returned results survive another operation or context
release. Applications release them with `gpgme_result_unref()`; nested
signature and recipient nodes remain borrowed from their owning result.
Context-current results are replaced normally by subsequent native calls.

Operational success does not imply a valid or trusted signature. Applications
inspect signature status, summary and validity, including empty signature
lists. A failed operation may still return diagnostic results, which must
also be released. Partial stream output must be discarded on failure.

## Stream integration

The public helpers are:

- `g_mime_gpgme_data_new()` adapts a stream at its current position; the data
  handle retains the stream until `gpgme_data_release()`.
- `g_mime_gpgme_sign()` accepts a native signature mode and configured signers.
- `g_mime_gpgme_verify()` handles detached and embedded signatures.
- `g_mime_gpgme_encrypt()` accepts native encryption flags and recipient keys,
  with optional combined OpenPGP signing.
- `g_mime_gpgme_decrypt()` optionally verifies embedded OpenPGP signatures
  and returns separate native results.

Use the adapter with native `gpgme_op_import/export()` for key exchange,
rather than duplicating GPGME key-management APIs.

## MIME APIs

Existing function names remain but signatures now take explicit contexts
and native result outputs:

| MIME operation | Context |
|---|---|
| Multipart signed sign/verify | OpenPGP or CMS, matching the MIME protocol |
| Multipart encrypted encrypt/decrypt | OpenPGP |
| PKCS7 MIME sign/verify/encrypt/decrypt | CMS |
| Existing mutating part OpenPGP helpers | OpenPGP |
| Autocrypt gossip extraction with decryption | OpenPGP |

Verification returns a boolean for operational success. CMS signed-data
verification additionally returns an extracted MIME entity. Decryption
returns the extracted entity plus optional native result metadata.
GMime-specific encryption, verification and decryption flags are removed.
Native flags/context settings are the single configuration mechanism.

## Header and binding boundary

Native APIs live in `<gmime/gmime-gpgme.h>`, with a `gmime-gpgme-4.0`
pkg-config package that exposes GPGME dependencies. Both are installed only
with cryptography enabled. `<gmime/gmime.h>` and the base MIME API do not
expose native GPGME types.

This deliberately gives up introspection-generated bindings for crypto
methods. Ordinary MIME classes, compression, parsing and OpenPGP detection
retain GNOME binding support. Applications such as Convey can provide their
own native GPGME/integration bindings. There is no second binding-friendly
result/context hierarchy in this design.

## Alternative OpenPGP engines

Sequoia is an independent Rust implementation, not a GnuPG fork. Its former
general-purpose C/Python FFI has been discontinued. Chameleon (`gpg-sq`)
offers a possible GPGME-compatible engine route through the `gpg` interface;
compatibility must be tested rather than promised. It does not provide CMS.

The native API intentionally makes the GPGME dependency explicit. Supporting
a future non-GPGME backend would require a separate integration API, not an
opaque handle pretending native GPGME results are backend-independent.

## Commit boundary

First migrate existing cryptography and remove the wrappers. Existing
mutating inline helpers remain functional with native contexts.

Add non-mutating, multi-block inline-PGP support separately: a document
owns a transfer-decoded snapshot, ordered blocks expose stable byte ranges,
and each verify/decrypt operation has its own native results. Preserve
unprotected gaps and never treat one valid block as authentication for
the entire MIME part. Discovery remains backend-independent.
