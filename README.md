# GMime

[![Build Status](https://github.com/jstedfast/gmime/actions/workflows/build.yml/badge.svg?event=push)](https://github.com/jstedfast/gmime/actions/workflows/build.yml)[![Coverity Scan Build Status](https://scan.coverity.com/projects/11948/badge.svg)](https://scan.coverity.com/projects/jstedfast-gmime)[![Coverage Status](https://coveralls.io/repos/github/jstedfast/gmime/badge.svg?branch=master)](https://coveralls.io/github/jstedfast/gmime?branch=master)

## What is GMime?

GMime is a C/C++ library which may be used for the creation and parsing of messages using the Multipurpose
Internet Mail Extension (MIME) as defined by [numerous IETF specifications](https://github.com/jstedfast/gmime/blob/master/RFCs.md).

See [PORTING.md](PORTING.md) to migrate applications from GMime 3.x, and
[STATUS.md](STATUS.md) for implementation scope and compatibility decisions.

GMime features an extremely robust high-performance parser designed to be able to preserve byte-for-byte information
allowing developers to re-seralize the parsed messages back to a stream exactly as the parser found them. It also features
integrated GnuPG and S/MIME v3.2 support.

Built on top of GObject (the object system used by the [GNOME desktop](https://www.gnome.org)), many developers should find
its API design and memory management very familiar.

## History

As a developer and user of Electronic Mail clients, I had come to
realize that the vast majority of E-Mail client (and server) software
had less-than-satisfactory MIME implementations. More often than not
these E-Mail clients created broken MIME messages and/or would
incorrectly try to parse a MIME message thus subtracting from the full
benefits that MIME was meant to provide. GMime is meant to address
this issue by following the MIME specification as closely as possible
while also providing programmers with an extremely easy to use
high-level application programming interface (API).

## License Information

The GMime library is Copyright (C) 2000-2024 Jeffrey Stedfast and is licensed under the LGPL v2.1

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Lesser General Public
    License as published by the Free Software Foundation; either
    version 2.1 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public
    License along with this library; if not, write to the Free Software
    Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.


## Getting the Source Code

You can download official public release tarballs of GMime from the
[GitHub release page](https://github.com/jstedfast/gmime/releases) of this repository.

If you would like to contribute to the GMime project, it is recommended that you grab the
source code from the official GitHub repository at
[https://github.com/jstedfast/gmime](https://github.com/jstedfast/gmime). Cloning this repository
can be done using the following command:

    git clone https://github.com/jstedfast/gmime.git


## Requirements

Building GMime requires a C compiler, Meson >= 1.3.0, Ninja, and pkg-config.

For proper compilation and functionality of GMime, the following packages
are REQUIRED:

  - GLib version >= 2.88.0

    Glib provides a number of portability-enhancing functions and types.
    Glib is included in most GMime-supported operating system
    distributions.  Glib sources may be obtained from:
      ftp://ftp.gtk.org/pub/glib

  - zlib

  - iconv (provided by the C library on some platforms, or GNU libiconv)

The following packages are RECOMMENDED:

  - Libidn2 >= 2.0.0

    Libidn2 provides APIs needed for encoding and decoding Internationalized
    Domain Names. Libidn2 sources may be obtained from:
      https://www.gnu.org/software/libidn/#downloading

  - GPGME >= 1.6.0 (newer native features require a supporting GPGME version)

    GPGME (GnuPG Made Easy) provides high-level crypto APIs needed for
    encryption, decryption, signing and signature verification for both
    OpenPGP and S/MIME. GPGME sources may be obtained from:
      https://www.gnupg.org/download/index.html#gpgme

## Building

GMime 4 uses Meson instead of Autotools. Builds must use a separate build
directory:

```sh
meson setup _build
meson compile -C _build
meson test -C _build --print-errorlogs
meson install -C _build
```

Use `--prefix` when setting up the build to choose the installation prefix.
Packagers can stage installation with `meson install -C _build --destdir DESTDIR`.

Crypto, libidn2, introspection, and Vala bindings are detected automatically.
Use `-Dcrypto=enabled`, `-Dlibidn=enabled`, `-Dintrospection=enabled`, or
`-Dvapi=enabled` to require them, or `disabled` to omit them. Without libidn2,
IDN conversion uses GLib. Vala bindings require introspection, and introspection
requires a shared library.

The API reference is optional: configure with `-Dgtk_doc=true` and build the
`gmime-doc` target. This requires gtk-doc and DocBook tools. Tests and examples
are built by default; disable them with `-Dtests=false` or `-Dexamples=false`.
Crypto tests additionally require GnuPG and the supplied test keys.
Test fixtures are excluded from Git's line-ending conversion so messages,
byte offsets, and expected output retain their exact repository bytes.

Meson's standard options provide static libraries (`-Ddefault_library=static`),
coverage (`-Db_coverage=true`), and sanitizers (`-Db_sanitize=address,undefined`).
When cross-compiling without an executable wrapper, GMime uses its built-in
platform charset aliases instead of running the iconv alias detector.

### Continuous integration and reference tests

CI builds optimized, assertion-enabled configurations on native Linux x64
and ARM64 and macOS 15 and 26 ARM64, with optional features enabled and
disabled. Both macOS versions retain the full test suite so native iconv
behavior can be compared without dropping older-system coverage.
Linux additionally checks Clang ASan/UBSan, static linking, source
distributions, documentation, and installed C/GIR/Vala consumers.
The SIMD test reports available and unavailable backends; CI explicitly
requires AVX2 on its x64 entries and NEON on ARM64. It fails instead of
silently substituting scalar coverage. Hosted-runner timings are not
performance gates. A required Windows job is deferred until a full-library
integration build is available; scanner-only MSVC checks are not equivalent.

The normal reader test includes 90 frozen actual-MimeKit callback traces,
including newline detection, multipart/embedded messages, truncated input,
short reads, nonseekable streams, and a 4096-byte buffer edge. These run
without .NET or a MimeKit checkout on every CI platform. To regenerate them,
use the .NET 10 SDK and the actual reference sources, not GMime output:

```sh
dotnet run --project tools/mimekit-reader-reference \
  -p:MimeKitSource=/path/to/MimeKit -- \
  /path/to/MimeKit tests/data/reader/mimekit.txt
```

Use the same reference directory for the build property and program argument.
The fixture records both the reference Git revision and a SHA-256 digest of
the compiled source files, including local source edits. Review regenerated
differences and the associated reference changes before accepting them.
The harness compiles the reference sources without modifying their checkout.
The six empty-input cases explicitly retain GMime's documented EOS-on-success
contract: unlike MimeKit, a failed empty read does not report successful EOS.

### Static analysis

The `Static analysis` workflow runs CodeQL and Cppcheck independently on pushes
and pull requests targeting `master` or `v4.0-dev`. It also supports manual runs
and a weekly schedule. GitHub only runs scheduled workflows from the default
branch, so the schedule takes effect once the workflow is on `master`.

Both jobs build the Linux x64 library with assertions, GPGME, and libidn2
enabled. Tests, examples, documentation, and language bindings are excluded.
This is not coverage of every platform, SIMD backend, or optional-feature
configuration.

CodeQL uses a clean, manual Meson build and the `security-extended` query suite.
Results appear in GitHub's **Security > Code scanning** view and in the
`codeql` workflow artifact as SARIF. Code scanning alerts do not themselves
fail the analysis job; merge enforcement requires repository code scanning
rules.

Cppcheck reads Meson's compilation database after the build has generated all
headers. It scans `gmime/gmime*.c` and `util/*.c`, using the bundled `gtk`
(GLib/GObject) and `posix` library models. Maintenance generators are not in
this scope; in particular, the GTK model's global `TRUE`/`FALSE` definitions
conflict with the standalone `gen-table.c` enum. Error, warning, performance,
and portability diagnostics are retained without blanket suppressions.
Findings are initially advisory while the baseline is triaged; build failures,
tool failures, and reported parser/internal analysis errors fail the job.
The job summary shows diagnostic counts, and the `cppcheck` artifact contains
the XML findings, active-checker report, and analysis log.

Artifacts are retained for 30 days and include the analyzed commit, tool and
dependency versions, compilation database, and Meson configuration. Tools
follow the runner's distribution packages and the CodeQL action release;
versions are recorded, not frozen. Archive reports separately if they are
needed as long-term release evidence.

To reproduce the Cppcheck scan, install Cppcheck and the normal build
dependencies, then run:

```sh
meson setup _build-cppcheck --buildtype=debug -Db_ndebug=false \
  -Dcrypto=enabled -Dlibidn=enabled -Dintrospection=disabled \
  -Dvapi=disabled -Dgtk_doc=false -Dtests=false -Dexamples=false
meson compile -C _build-cppcheck
mkdir -p _build-cppcheck/cppcheck-cache _build-cppcheck/reports
cppcheck --project=_build-cppcheck/compile_commands.json \
  --file-filter='*/gmime/gmime*.c' --file-filter='*/util/*.c' \
  --cppcheck-build-dir=_build-cppcheck/cppcheck-cache \
  --library=gtk --library=posix \
  --enable=warning,performance,portability --check-level=normal \
  --checkers-report=_build-cppcheck/reports/checkers.txt \
  --error-exitcode=2 --xml \
  --output-file=_build-cppcheck/reports/cppcheck.xml -j 2
```

Exit status 2 means diagnostics were reported and need review. These scans
complement testing and fuzzing; they do not establish MISRA C or CERT C
compliance. Formal conformance requires a defined standard/version and scope,
rule-coverage assessment, manual review, and documented finding dispositions.

## Using GMime

### Parsing Messages

One of the more common operations that GMime is meant for is parsing email messages from arbitrary streams.
To do this, you will need to use a `GMimeParser`:

```c
/* load a GMimeMessage from a stream */
GMimeMessage *message;
GMimeStream *stream;
GMimeParser *parser;
GError *error = NULL;

stream = g_mime_stream_fs_open ("message.eml", O_RDONLY, 0644, NULL);
parser = g_mime_parser_new_with_stream (stream);

/* Note: we can unref the stream now since the GMimeParser has a reference to it... */
g_object_unref (stream);

message = g_mime_parser_construct_message (parser, NULL, &error);
if (error != NULL) {
    g_warning ("Failed to parse message: %s", error->message);
    g_error_free (error);
}

/* unref the parser since we no longer need it */
g_object_unref (parser);
```

If you are planning to parse messages from an mbox stream, you can do something like this:

```c
GMimeMessage *message;
GMimeStream *stream;
GMimeParser *parser;
GError *error = NULL;
gint64 offset;
char *marker;

stream = g_mime_stream_fs_open ("Inbox.mbox", O_RDONLY, 0644, NULL);
parser = g_mime_parser_new_with_stream (stream);

/* tell the parser to scan mbox-style "From " markers */
g_mime_parser_set_format (parser, GMIME_FORMAT_MBOX);

/* Note: we can unref the stream now since the GMimeParser has a reference to it... */
g_object_unref (stream);

while (!g_mime_parser_eos (parser)) {
    /* load the next message from the mbox */
    if ((message = g_mime_parser_construct_message (parser, NULL, &error)) != NULL)
        g_object_unref (message);
    if (error != NULL) {
        g_warning ("Failed to parse mbox message: %s", error->message);
        g_error_free (error);
        break;
    }

    /* get information about the mbox "From " marker... */
    offset = g_mime_parser_get_mbox_marker_offset (parser);
    marker = g_mime_parser_get_mbox_marker (parser);
}

g_object_unref (parser);
```

### Getting the Body of a Message

A common misunderstanding about email is that there is a well-defined message body and then a list
of attachments. This is not really the case. The reality is that MIME is a tree structure of content,
much like a file system.

Luckily, MIME does define a set of general rules for how mail clients should interpret this tree
structure of MIME parts. The `Content-Disposition` header is meant to provide hints to the receiving
client as to which parts are meant to be displayed as part of the message body and which are meant
to be interpreted as attachments.

The `Content-Disposition` header will generally have one of two values: `inline` or `attachment`.

The meaning of these value should be fairly obvious. If the value is `attachment`, then the content
of said MIME part is meant to be presented as a file attachment separate from the core message.
However, if the value is `inline`, then the content of that MIME part is meant to be displayed inline
within the mail client's rendering of the core message body. If the `Content-Disposition` header does
not exist, then it should be treated as if the value were `inline`.

Technically, every part that lacks a `Content-Disposition` header or that is marked as `inline`, then,
is part of the core message body.

There's a bit more to it than that, though.

Modern MIME messages will often contain a `multipart/alternative` MIME container which will generally contain
a `text/plain` and `text/html` version of the text that the sender wrote. The `text/html` version is typically
formatted much closer to what the sender saw in his or her WYSIWYG editor than the `text/plain` version.

The reason for sending the message text in both formats is that not all mail clients are capable of displaying
HTML.

The receiving client should only display one of the alternative views contained within the `multipart/alternative`
container. Since alternative views are listed in order of least faithful to most faithful with what the sender
saw in his or her WYSIWYG editor, the receiving client *should* walk over the list of alternative views starting
at the end and working backwards until it finds a part that it is capable of displaying.

Example:
```
multipart/alternative
  text/plain
  text/html
```

As seen in the example above, the `text/html` part is listed last because it is the most faithful to
what the sender saw in his or her WYSIWYG editor when writing the message.

To make matters even more complicated, sometimes modern mail clients will use a `multipart/related`
MIME container instead of a simple `text/html` part in order to embed images and other content
within the HTML.

Example:
```
multipart/alternative
  text/plain
  multipart/related
    text/html
    image/jpeg
    video/mp4
    image/png
```

In the example above, one of the alternative views is a `multipart/related` container which contains
an HTML version of the message body that references the sibling video and images.

Now that you have a rough idea of how a message is structured and how to interpret various MIME entities,
the next step is learning how to traverse the MIME tree using GMime.

### Traversing a GMimeMessage

The top-level MIME entity of the message will generally either be a `GMimePart` or a `GMimeMultipart`.

As an example, if you wanted to rip out all of the attachments of a message, your code might look
something like this:

```c
GMimePartIter *iter = g_mime_part_iter_new (message);
GPtrArray *attachments = g_ptr_array_new ();
GPtrArray *multiparts = g_ptr_array_new ();

/* collect our list of attachments and their parent multiparts */
while (g_mime_part_iter_next (iter)) {
    GMimeEntity *current = g_mime_part_iter_get_current (iter);
    GMimeEntity *parent = g_mime_part_iter_get_parent (iter);

    if (GMIME_IS_MULTIPART (parent) && GMIME_IS_PART (current)) {
        GMimePart *part = (GMimePart *) current;

        if (g_mime_part_is_attachment (part)) {
            /* keep track of each attachment's parent multipart */
            g_ptr_array_add (multiparts, parent);
            g_ptr_array_add (attachments, part);
        }
    }
}

/* now remove each attachment its parent multipart... */
for (int i = 0; i < attachments->len; i++) {
    GMimeMultipart *multipart = (GMimeMultipart *) multiparts->pdata[i];
    GMimeEntity *attachment = (GMimeEntity *) attachments->pdata[i];

    g_mime_multipart_remove (multipart, attachment);
}
```

### Creating a Simple Message

Creating MIME messages using GMime is really trivial.

```csharp
GMimeMessage *message;
GMimeTextPart *body;

message = g_mime_message_new (TRUE);

g_mime_message_add_mailbox (message, GMIME_ADDRESS_TYPE_FROM, "Joey", "joey@friends.com", NULL);
g_mime_message_add_mailbox (message, GMIME_ADDRESS_TYPE_TO, "Alice", "alice@wonderland.com", NULL);
g_mime_message_set_subject (message, "How you doin?", NULL);

body = g_mime_text_part_new_with_subtype ("plain");
g_mime_text_part_set_text (body, "Hey Alice,\n\n"
    "What are you up to this weekend? Monica is throwing one of her parties on\n"
    "Saturday and I was hoping you could make it.\n\n"
    "Will you be my +1?\n\n"
    "-- Joey\n");

g_mime_message_set_mime_part (message, (GMimeEntity *) body);
g_object_unref (body);
```

### Creating a Message with Attachments

Attachments are just like any other `GMimePart`, the only difference is that they typically have
a `Content-Disposition` header with a value of "attachment" instead of "inline" or no
`Content-Disposition` header at all.

Typically, when a mail client adds attachments to a message, it will create a `multipart/mixed`
part and add the text body part and all of the file attachments to the `multipart/mixed`.

Here's how you can do that with GMime:

```c
GMimeMultipart *multipart;
GMimeContent *content;
GMimeMessage *message;
GMimePart *attachment;
GMimeTextPart *body;
GMimeStream *stream;

message = g_mime_message_new (TRUE);
g_mime_message_add_mailbox (message, GMIME_ADDRESS_TYPE_FROM, "Joey", "joey@friends.com", NULL);
g_mime_message_add_mailbox (message, GMIME_ADDRESS_TYPE_TO, "Alice", "alice@wonderland.com", NULL);
g_mime_message_set_subject (message, "How you doin?", NULL);

/* create our message text, just like before... */
body = g_mime_text_part_new_with_subtype ("plain");
g_mime_text_part_set_text (body, "Hey Alice,\n\n"
    "What are you up to this weekend? Monica is throwing one of her parties on\n"
    "Saturday and I was hoping you could make it.\n\n"
    "Will you be my +1?\n\n"
    "-- Joey\n");

/* create an image attachment for the file located at path */
attachment = g_mime_part_new_with_type ("image", "gif");
g_mime_part_set_filename (attachment, basename (path));

/* create the content for the image attachment */
stream = g_mime_stream_fs_open (path, O_RDONLY, 0644, NULL);
content = g_mime_content_new_with_stream (stream, GMIME_CONTENT_ENCODING_UNSPECIFIED);
g_object_unref (stream);

/* set the content of the attachment */
g_mime_part_set_content (attachment, content);
g_object_unref (content);

/* set the Content-Transfer-Encoding to base64 */
g_mime_part_set_content_encoding (attachment, GMIME_CONTENT_ENCODING_BASE64);

/* now create the multipart/mixed container to hold the message text and the image attachment */
multipart = g_mime_multipart_new_with_subtype ("mixed");

/* add the text body first */
g_mime_multipart_add (multipart, (GMimeEntity *) body);
g_object_unref (body);

/* now add the attachment(s) */
g_mime_multipart_add (multipart, (GMimeEntity *) attachment);
g_object_unref (attachment);

/* set the multipart/mixed as the message body */
g_mime_message_set_mime_part (message, (GMimeEntity *) multipart);
g_object_unref (multipart);
```

Of course, that is just a simple example. A lot of modern mail clients such as Outlook or Thunderbird will 
send out both a `text/html` and a `text/plain` version of the message text. To do this, you'd create a
`GMimeTextPart` for the `text/plain` part and another `GMimeTextPart` for the `text/html` part and then add
them to a `multipart/alternative` like so:

```c
GMimeMultipart *mixed, *alternative;
GMimeTextPart *plain, *html;
GMimePart *attachment;

/* see above for how to create each of these... */
attachment = CreateAttachment ();
plain = CreateTextPlainPart ();
html = CreateTextHtmlPart ();

/* Note: it is important that the text/html part is added second, because it is the
 * most expressive version and (probably) the most faithful to the sender's WYSIWYG 
 * editor. */
alternative = g_mime_multipart_new_with_subtype ("alternative");

g_mime_multipart_add (alternative, (GMimeEntity *) plain);
g_object_unref (plain);

g_mime_multipart_add (alternative, (GMimeEntity *) html);
g_object_unref (html);

/* now create the multipart/mixed container to hold the multipart/alternative
 * and the image attachment */
mixed = g_mime_multipart_new_with_subtype ("mixed");

g_mime_multipart_add (mixed, (GMimeEntity *) alternative);
g_object_unref (alternative);

g_mime_multipart_add (mixed, (GMimeEntity *) attachment);
g_object_unref (attachment);

/* now set the multipart/mixed as the message body */
g_mime_message_set_mime_part (message, (GMimeEntity *) mixed);
g_object_unref (mixed);
```

### OpenPGP and S/MIME

Include `<gmime/gmime-gpgme.h>` and use the `gmime-gpgme-4.0` pkg-config
package. GMime borrows native GPGME contexts and keys; it handles MIME
canonicalization and packaging, not key selection or application trust
policy. Native crypto methods are not exposed through introspection.
See [PORTING.md](PORTING.md#8-migrate-cryptography-to-native-gpgme) for
migration and ownership details.

After `g_mime_init()`, create a context and choose OpenPGP or CMS (S/MIME).
Always check configuration errors:

```c
gpgme_ctx_t ctx;
gpgme_error_t rc;

if ((rc = gpgme_new (&ctx))) {
    fprintf (stderr, "context creation failed: %s\n", gpgme_strerror (rc));
    return;
}
if ((rc = gpgme_set_protocol (ctx, GPGME_PROTOCOL_OpenPGP))) {
    fprintf (stderr, "protocol selection failed: %s\n", gpgme_strerror (rc));
    gpgme_release (ctx);
    return;
}
```

Configure callbacks, engine paths, network policy and signers through GPGME.
Use `gpgme_get_key()` to resolve recipient keys; signing uses the context's
`gpgme_signers_add()` list. GMime preserves that list and restores any
temporary MIME-specific armor/textmode settings. Do not concurrently share
a context between operations.

### Encrypting and decrypting MIME content

PGP/MIME uses `multipart/encrypted`. Given an OpenPGP context, a borrowed
recipient key selected by the application, and the message's body:

```c
gpgme_key_t recipients[] = { recipient_key, NULL };
GError *error = NULL;
GMimeMultipartEncrypted *encrypted;

encrypted = g_mime_multipart_encrypted_encrypt (
    ctx, body, FALSE, 0, recipients, NULL, NULL, &error);
if (encrypted) {
    g_mime_message_set_mime_part (message, GMIME_ENTITY (encrypted));
    g_object_unref (encrypted);
} else {
    fprintf (stderr, "encryption failed: %s\n", error->message);
    g_clear_error (&error);
}
```

Decrypt with `g_mime_multipart_encrypted_decrypt(encrypted, ctx,
&decrypt_result, &verify_result, &error)`. It returns an owned MIME entity,
or NULL on failure. Inspect any verification result before treating the
extracted content as authenticated.

S/MIME instead uses an `application/pkcs7-mime` part. With a CMS context,
call `g_mime_application_pkcs7_mime_encrypt(ctx, body, flags, recipients,
&result, &error)` or
`g_mime_application_pkcs7_mime_decrypt(part, ctx, &result, &error)`.
Signed content inside a decrypted CMS entity requires separate verification.

### Signing and verifying MIME content

Both protocols support detached `multipart/signed` signatures:
`g_mime_multipart_signed_sign(ctx, body, &result, &error)` uses the
context's configured signers. To verify, supply a context matching the
received MIME `protocol` parameter:

```c
gpgme_verify_result_t result = NULL;
GError *error = NULL;

if (!g_mime_multipart_signed_verify (signed_part, ctx, &result, &error)) {
    fprintf (stderr, "verification failed: %s\n", error->message);
    g_clear_error (&error);
} else if (result) {
    for (gpgme_signature_t sig = result->signatures; sig; sig = sig->next) {
        fprintf (stdout, "%s: %s (validity %d)\n",
                 sig->fpr ? sig->fpr : "(unknown)",
                 gpgme_strerror (sig->status), sig->validity);
    }
}
if (result)
    gpgme_result_unref (result);
```

**Operational success does not imply a valid or trusted signature.**
Evaluate each native signature's status, summary and validity against
application policy, including the absence of signatures. Results can also
be returned on failure for diagnostics: release every non-NULL result with
`gpgme_result_unref()`. Returned results remain valid after another operation
or context release; nested list nodes are borrowed from their result.

CMS also supports opaque signed-data parts through
`g_mime_application_pkcs7_mime_sign(ctx, body, &result, &error)` and
`g_mime_application_pkcs7_mime_verify(part, ctx, &extracted, &result, &error)`.
The latter returns operational success and an owned extracted MIME entity.

Release application-owned keys with `gpgme_key_unref()` and contexts with
`gpgme_release()`, never with GObject functions. For raw streams and key
import/export, see `g_mime_gpgme_*()` and `g_mime_gpgme_data_new()`.

## Understanding Memory Management in GMime

- Functions that return `const char *` return a pointer to a string stored on the `GObject` that will be released
when the object itself is finalized and therefore should *never* be fed to `g_free()` by the caller.

- Functions that return `char *` are left up to the caller to `g_free()` when the caller is done with them.

### For functions throughout the GMime API that return an object reference:

- If the function that returned the object allocated the object, then you need to unref it, otherwise you don't.

For example, the `stream` in the following code would need to be unreffed (using `g_object_unref()`) because it was
freshly allocated by the function:

```c
GMimeStream *stream = g_mime_stream_mem_new ();
```

The next example shows a function that returns a reference to an *existing* `GMimeStream` object that is stored on
(and therefor managed by) the `GMimeContent` object:

```c
GMimeStream *stream = g_mime_content_get_stream (wrapper);
```

In the above example, the `stream` *should not* be unreffed because the `stream` object's memory is managed by the
`wrapper` object. In other words, when the `wrapper` object is freed, the `stream` will be freed as well.

Where many developers get confused is when they create a new object and then add it to another object.

For example:

```c
GMimeStream *stream = g_mime_stream_mem_new ();
GMimeContent *wrapper = g_mime_content_new_with_stream (stream, GMIME_CONTENT_ENCODING_UNSPECIFIED);
g_object_unref (stream);
```

Even in this situation, it is necessary to unref the allocated object because all GMime functions that add an
object to another object will ref that object themselves.

In other words, in the above example, the `GMimeStream` is being added to the `GMimeContent`. The
`GMimeContent` calls `g_object_ref()` on the `stream` which means that the `stream` object now has
a ref count of 2.

When `g_object_unref()` is called on the stream after adding it to the `GMimeContent`, the ref count drops
down to 1.

And when the `wrapper` object is eventually finalized after calling `g_object_unref()` on it, the ref count
on the `stream` object will be reduced to 0 as well, thereby freeing the `stream` object.

## Documentation

This is the README file for GMime. Additional documentation related to
development using GMime has been included within the source release
of GMime.

|               |Description                                                  |
|---------------|-------------------------------------------------------------|
|docs/reference/|Contains SGML and HTML versions of the GMime reference manual|
|docs/tutorial/ |Contains SGML and HTML versions of the GMime tutorial|
|AUTHORS        |List of primary authors (source code developers)|
|LICENSE        |The GNU Lesser General Public License, version 2.1|
|INSTALL        |In-depth installation instructions|
|TODO           |Description of planned GMime development|
|[PORTING.md](PORTING.md)|Application migration from GMime 3.2.15 to 4.0|
|[STATUS.md](STATUS.md)|Implementation status, compatibility decisions, and validation history|


## Discussion Forums

For discussion of GMime development (either of GMime itself or using GMime in your own application),
you may find the [GMime Discussion](https://github.com/jstedfast/gmime/discussions) forum helpful.


## Bindings

Other developers have been working to make GMime available to programmers in other languages.
The current list of known bindings are:

|Language   |Location                                |
|-----------|----------------------------------------|
|Go         |https://github.com/sendgrid/go-gmime    |
|Perl       |http://search.cpan.org/dist/MIME-Fast   |
|C#         |https://github.com/jstedfast/MimeKit    |
|Julia      |https://github.com/bhftbootcamp/GMime.jl|

## Reporting Bugs

Bugs may be reported to the GMime development team by creating a new
[issue](https://github.com/jstedfast/gmime/issues).

## Reporting Security Issues

Security issues should be reported to <a href="mailto:jestedfa@microsoft.com?subject=GMime%20security%20issue">jestedfa@microsoft.com</a>.

Using a Subject along the lines of "GMime security issue" would be helpful in helping to spot the email message.
