# Specifications

This inventory describes GMime's support for Internet message and MIME
specifications. Support below refers to the listed message-format, parsing,
serialization, or conversion features; it is not a claim that GMime
implements every requirement of each RFC.

Numbered links open the reference texts bundled in [rfc/](rfc/). Official
publications and errata are available from the
[RFC Editor](https://www.rfc-editor.org/). The archive also retains historical
and related specifications; the presence of a file is not a support claim.
Reference texts are kept unmodified.

## Internet messages and MIME

| RFC | Specification and implemented scope |
|---|---|
| [5322](rfc/rfc5322.txt) | Internet Message Format: message headers, addresses, dates, message identifiers, parsing, and serialization, including tolerant handling of legacy input. |
| [2045](rfc/rfc2045.txt), [2046](rfc/rfc2046.txt) | MIME message bodies, media types, transfer encodings, multipart entities, embedded messages, and message/partial splitting and reconstruction. |
| [2047](rfc/rfc2047.txt) | Non-ASCII message-header encoded words, including charset conversion and folding. |
| [2183](rfc/rfc2183.txt) | Content-Disposition and attachment metadata. |
| [2231](rfc/rfc2231.txt) | MIME parameter charsets, languages, and continuations. |
| [1864](rfc/rfc1864.txt) | Content-MD5 header access, digest computation, and verification. This legacy integrity checksum is not a cryptographic authenticity guarantee. |
| [1896](rfc/rfc1896.txt) | text/enriched conversion through `GMimeFilterEnriched`, including legacy text/richtext input. |
| [2387](rfc/rfc2387.txt) | multipart/related construction, root selection, and related-part access through `GMimeMultipartRelated`. |
| [2392](rfc/rfc2392.txt) | Content-ID URL lookup within multipart/related. This does not provide a message-store resolver for arbitrary mid: URLs. |
| [2424](rfc/rfc2424.txt) | Content-Duration header access through `GMimePart`. |
| [2557](rfc/rfc2557.txt) | MHTML MIME structure, Content-Location/Content-Base metadata, and related-resource lookup. GMime is not an HTML renderer or a network resource loader. |
| [3492](rfc/rfc3492.txt) | IDN/Punycode address-domain conversion through libidn2 or GLib, rather than a separate GMime implementation of the algorithm. |
| [6532](rfc/rfc6532.txt) | Internationalized UTF-8 message headers and international formatting options. |
| [6854](rfc/rfc6854.txt) | Group syntax in From and Sender fields, including resent originator fields. |

## Delivery, disposition, and feedback reports

| RFC | Specification and implemented scope |
|---|---|
| [6522](rfc/rfc6522.txt) | multipart/report containers and the report-type parameter through `GMimeMultipartReport`. |
| [3464](rfc/rfc3464.txt) | Delivery-status notification content through `GMimeMessageDeliveryStatus`, exposing per-message and per-recipient header groups. |
| [8098](rfc/rfc8098.txt) | Message disposition notification content through `GMimeMessageDispositionNotification`, exposing the notification fields. |
| [6533](rfc/rfc6533.txt) | Internationalized notification and returned-message media types: message/global, message/global-headers, message/global-delivery-status, and message/global-disposition-notification. |
| [5965](rfc/rfc5965.txt) | Abuse Reporting Format content through `GMimeMessageFeedbackReport`, exposing message/feedback-report fields. |

These are content models and parsers, not mail-delivery, receipt-generation,
or abuse-reporting services. Header collections preserve extensible fields;
they do not enforce every mandatory field, field grammar, or report policy.

## OpenPGP and S/MIME

| RFC | Specification and implemented scope |
|---|---|
| [1847](rfc/rfc1847.txt) | multipart/signed and multipart/encrypted MIME containers. |
| [3156](rfc/rfc3156.txt) | OpenPGP/MIME signing, verification, encryption, and decryption using native GPGME OpenPGP contexts and the security multipart classes. |
| [5751](rfc/rfc5751.txt) | S/MIME MIME packaging and signing, verification, encryption and decryption through native GPGME CMS contexts and `GMimeApplicationPkcs7Mime`. |
| [5652](rfc/rfc5652.txt) | Cryptographic Message Syntax payloads handled by the S/MIME backend, not by an independent GMime CMS implementation. |

Cryptographic operations require the optional
[GPGME](https://www.gnupg.org/software/gpgme/index.html)/[GnuPG](https://www.gnupg.org/)
backend. Algorithms, key and certificate handling, trust decisions, and
supported CMS features depend on that backend.

The S/MIME certificate-handling and capability references
[5750](rfc/rfc5750.txt) and [4262](rfc/rfc4262.txt), and the S/MIME 4.0
specifications [8550](rfc/rfc8550.txt) and [8551](rfc/rfc8551.txt), are included
for reference. Their presence does not assert complete certificate-profile
or S/MIME 4.0 conformance.

## Limited support and related specifications

| RFC | Current scope or limitation |
|---|---|
| [3676](rfc/rfc3676.txt) | text/plain; format=flowed construction and recognition. Flowing/unflowing text and DelSp conversion are not implemented. |
| [6376](rfc/rfc6376.txt), [8301](rfc/rfc8301.txt), [8463](rfc/rfc8463.txt) | DKIM references. GMime preserves signature headers and provides signature-value folding, but does not implement DKIM canonicalization, signing, verification, DNS key lookup, or algorithm/key-policy enforcement. |
| [8617](rfc/rfc8617.txt) | ARC header recognition/preservation and folding of ARC-Message-Signature and ARC-Seal values. No ARC chain validation, signing, or verification. |
| [8601](rfc/rfc8601.txt) | Authentication-Results header recognition and preservation, not a structured authentication-result parser or authentication evaluator. |
| [2369](rfc/rfc2369.txt), [2919](rfc/rfc2919.txt), [8058](rfc/rfc8058.txt) | Mailing-list header identifiers and ordinary header storage. No list-command or one-click unsubscribe service. |
| [3834](rfc/rfc3834.txt) | Auto-Submitted header recognition, not an automatic-response policy engine. |
| [6531](rfc/rfc6531.txt) | SMTPUTF8 transport reference. GMime supports internationalized message content, not SMTP negotiation or delivery. |
| [5545](rfc/rfc5545.txt), [5546](rfc/rfc5546.txt) | iCalendar and iTIP reference documents. Calendar bodies can be carried as MIME content; calendar parsing and scheduling semantics are not implemented. |
| [7103](rfc/rfc7103.txt) | Malformed-message handling reference. Tolerant parsing, configurable limits, and compliance diagnostics are not a claim to implement every recommendation. |

Header preservation and generic MIME parsing do not imply implementation of
the associated protocol. In particular, this inventory does not claim SPF or
DMARC evaluation.

## Historical and background references

These files are retained for interpreting legacy messages and understanding
the evolution of the formats. Prefer the newer specifications above for new
applications; inclusion here does not recommend obsolete cryptography.

| RFCs | Subject / relationship |
|---|---|
| [0822](rfc/rfc0822.txt), [2822](rfc/rfc2822.txt) | Predecessors of the Internet Message Format in RFC 5322. |
| [1341](rfc/rfc1341.txt), [1521](rfc/rfc1521.txt) | Earlier MIME body specifications, replaced by the RFC 2045 series. |
| [1342](rfc/rfc1342.txt), [1522](rfc/rfc1522.txt) | Earlier non-ASCII header specifications, replaced by RFC 2047. |
| [1523](rfc/rfc1523.txt) | Earlier text/enriched specification, superseded by RFC 1896. |
| [1544](rfc/rfc1544.txt) | Earlier Content-MD5 specification, replaced by RFC 1864. |
| [1872](rfc/rfc1872.txt), [2112](rfc/rfc2112.txt) | Earlier multipart/related specifications, replaced by RFC 2387. |
| [1892](rfc/rfc1892.txt), [3462](rfc/rfc3462.txt) | Earlier multipart/report specifications, replaced by RFC 6522. |
| [2015](rfc/rfc2015.txt) | Earlier PGP/MIME specification, superseded by RFC 3156. |
| [2110](rfc/rfc2110.txt) | Earlier MHTML specification, replaced by RFC 2557. |
| [2111](rfc/rfc2111.txt) | Earlier Content-ID and Message-ID URL specification, replaced by RFC 2392. |
| [2184](rfc/rfc2184.txt) | Earlier MIME parameter extensions, replaced by RFC 2231. |
| [2298](rfc/rfc2298.txt), [3798](rfc/rfc3798.txt) | Earlier disposition notification specifications, replaced by RFC 8098. |
| [2311](rfc/rfc2311.txt), [2633](rfc/rfc2633.txt), [3851](rfc/rfc3851.txt) | Earlier S/MIME message specifications. |
| [2312](rfc/rfc2312.txt), [2632](rfc/rfc2632.txt), [3850](rfc/rfc3850.txt) | Earlier S/MIME certificate-handling specifications. |
| [2315](rfc/rfc2315.txt), [2630](rfc/rfc2630.txt) | PKCS #7 and an earlier Cryptographic Message Syntax specification. |
| [4871](rfc/rfc4871.txt), [5672](rfc/rfc5672.txt) | Earlier DKIM specification and update, superseded by RFC 6376; not a claim of DKIM implementation. |
| [5451](rfc/rfc5451.txt), [7001](rfc/rfc7001.txt), [7601](rfc/rfc7601.txt) | Earlier Authentication-Results specifications, superseded by RFC 8601. |
| [2048](rfc/rfc2048.txt), [2049](rfc/rfc2049.txt) | MIME registration procedures, conformance criteria, and examples; reference material rather than additional protocol implementations. |
| [1927](rfc/rfc1927.txt), [2388](rfc/rfc2388.txt) | Related document media types and historical multipart/form-data specification. Generic MIME handling is not an HTTP form-data API. |
| [2268](rfc/rfc2268.txt), [2313](rfc/rfc2313.txt), [2314](rfc/rfc2314.txt), [2631](rfc/rfc2631.txt), [2634](rfc/rfc2634.txt) | Legacy RC2, RSA, certification request, Diffie-Hellman, and Enhanced Security Services references. Cryptographic primitives and certificate processing are delegated to the backend. |
| [4134](rfc/rfc4134.txt), [7107](rfc/rfc7107.txt), [7114](rfc/rfc7114.txt) | S/MIME examples and registries; not separate GMime protocol implementations. |
