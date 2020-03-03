# GMime

[![Build Status](https://travis-ci.org/jstedfast/gmime.svg)](https://travis-ci.org/jstedfast/gmime)[![Coverity Scan Build Status](https://scan.coverity.com/projects/11948/badge.svg)](https://scan.coverity.com/projects/jstedfast-gmime)[![Coverage Status](https://coveralls.io/repos/github/jstedfast/gmime/badge.svg?branch=master)](https://coveralls.io/github/jstedfast/gmime?branch=master)

## What is GMime?

GMime is a C/C++ library which may be used for the creation and parsing of messages using the Multipurpose
Internet Mail Extension (MIME) as defined by [numerous IETF specifications](https://github.com/jstedfast/gmime/blob/master/RFCs.md).

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

The GMime library is Copyright (C) 2000-2020 Jeffrey Stedfast and is licensed under the LGPL v2.1

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

You can download official public release tarballs of GMime at
[https://download.gnome.org/sources/gmime/](https://download.gnome.org/sources/gmime/)
or
[ftp://ftp.gnome.org/pub/GNOME/sources/gmime/](ftp://ftp.gnome.org/pub/GNOME/sources/gmime/).

If you would like to contribute to the GMime project, it is recommended that you grab the
source code from the official GitHub repository at
[http://github.com/jstedfast/gmime](https://github.com/jstedfast/gmime). Cloning this repository
can be done using the following command:

    git clone https://github.com/jstedfast/gmime.git


## Requirements

For proper compilation and functionality of GMime, the following packages
are REQUIRED:

  - Glib version >= 2.32.0

    Glib provides a number of portability-enhancing functions and types.
    Glib is included in most GMime-supported operating system
    distributions.  Glib sources may be obtained from:
      ftp://ftp.gtk.org/pub/glib

The following packages are RECOMMENDED:

  - Libidn2 >= 2.0.0

    Libidn2 provides APIs needed for encoding and decoding Internationalized
    Domain Names. Libidn2 sources may be obtained from:
      https://www.gnu.org/software/libidn/#downloading

  - GPGME >= 1.8.0 (will build with 1.6.0, but will be missing some functionality)

    GPGME (GnuPG Made Easy) provides high-level crypto APIs needed for
    encryption, decryption, signing and signature verification for both
    OpenPGP and S/MIME. GPGME sources may be obtained from:
      https://www.gnupg.org/download/index.html#gpgme

## Using GMime

### Parsing Messages

One of the more common operations that GMime is meant for is parsing email messages from arbitrary streams.
To do this, you will need to use a `GMimeParser`:

```c
/* load a GMimeMessage from a stream */
GMimeMessage *message;
GMimeStream *stream;
GMimeParser *parser;

stream = g_mime_stream_file_open ("message.eml", "r", NULL);
parser = g_mime_parser_new_with_stream (stream);

/* Note: we can unref the stream now since the GMimeParser has a reference to it... */
g_object_unref (stream);

message = g_mime_parser_construct_message (parser);

/* unref the parser since we no longer need it */
g_object_unref (parser);
```

If you are planning to parse messages from an mbox stream, you can do something like this:

```c
GMimeMessage *message;
GMimeStream *stream;
GMimeParser *parser;
gint64 offset;
char *marker;

stream = g_mime_stream_file_open ("Inbox.mbox", "r", NULL);
parser = g_mime_parser_new_with_stream (stream);

/* tell the parser to scan mbox-style "From " markers */
g_mime_parser_set_scan_from (parser, TRUE);

/* Note: we can unref the stream now since the GMimeParser has a reference to it... */
g_object_unref (stream);

while (!g_mime_parser_eos (parser)) {
    /* load the next message from the mbox */
    if ((message = g_mime_parser_construct_message (parser)) != NULL)
        g_object_unref (message);

    /* get information about the mbox "From " marker... */
    offset = g_mime_parser_get_from_offset (parser);
    marker = g_mime_parser_get_from (parser);
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
    GMimeObject *current = g_mime_part_iter_get_current (iter);
    GMimeObject *parent = g_mime_part_iter_get_parent (iter);

    if (GMIME_IS_MULTIPART (parent) && GMIME_IS_PART (current)) {
        GMimePart *part = (GMimePart *) current;

        if (g_mime_part_is_attachment (part)) {
            /* keep track of each attachment's parent multipart */
            g_ptr_array_append (multiparts, parent);
            g_ptr_array_append (attachments, part);
        }
    }
}

/* now remove each attachment from its parent multipart... */
for (int i = 0; i < attachments->len; i++) {
    GMimeMultipart *multipart = (GMimeMultipart *) multiparts->pdata[i];
    GMimeObject *attachment = (GMimeObject *) attachments->pdata[i];

    g_mime_multipart_remove (multipart, attachment);
}
```

### Creating a Simple Message

Creating MIME messages using GMime is really trivial.

```csharp
GMimeMessage *message;
GMimePart *body;

message = g_mime_message_new (TRUE);

g_mime_message_add_mailbox (message, GMIME_ADDRESS_TYPE_FROM, "Joey", "joey@friends.com");
g_mime_message_add_mailbox (message, GMIME_ADDRESS_TYPE_TO, "Alice", "alice@wonderland.com");
g_mime_message_set_subject (message, "How you doin?", NULL);

body = g_mime_text_part_new_with_subtype ("plain");
g_mime_text_part_set_text (body, "Hey Alice,\n\n"
    "What are you up to this weekend? Monica is throwing one of her parties on\n"
    "Saturday and I was hoping you could make it.\n\n"
    "Will you be my +1?\n\n"
    "-- Joey\n");

g_mime_message_set_mime_part (message, (GMimeObject *) body);
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
GMimeDataWrapper *content;
GMimeMessage *message;
GMimePart *attachment;
GMimeTextPart *body;
GMimeStream *stream;

message = g_mime_message_new (TRUE);
g_mime_message_add_mailbox (message, GMIME_ADDRESS_TYPE_FROM, "Joey", "joey@friends.com");
g_mime_message_add_mailbox (message, GMIME_ADDRESS_TYPE_TO, "Alice", "alice@wonderland.com");
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
content = g_mime_data_wrapper_new_with_stream (stream, GMIME_CONTENT_ENCODING_DEFAULT);
g_object_unref (stream);

/* set the content of the attachment */
g_mime_part_set_content (attachment, content);
g_object_unref (content);

/* set the Content-Transfer-Encoding to base64 */
g_mime_part_set_content_encoding (attachment, GMIME_CONTENT_ENCODING_BASE64);

/* now create the multipart/mixed container to hold the message text and the image attachment */
multipart = g_mime_multipart_new_with_subtype ("mixed");

/* add the text body first */
g_mime_multipart_add (multipart, (GMimeObject *) body);
g_object_unref (body);

/* now add the attachment(s) */
g_mime_multipart_add (multipart, (GMimeObject *) attachment);
g_object_unref (attachment);

/* set the multipart/mixed as the message body */
g_mime_message_set_mime_part (message, (GMimeObject *) multipart);
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
alternative = g_mime_multipart_with_subtype ("alternative");

g_mime_multipart_add (alternative, (GMimeObject *) plain);
g_object_unref (plain);

g_mime_multipart_add (alternative, (GMimeObject *) html);
g_object_unref (html);

/* now create the multipart/mixed container to hold the multipart/alternative
 * and the image attachment */
mixed = g_mime_multipart_new_with_subtype ("mixed");

g_mime_multipart_add (mixed, (GMimeObject *) alternative);
g_object_unref (alternative);

g_mime_multipart_add (mixed, (GMimeObject *) attachment);
g_object_unref (attachment);

/* now set the multipart/mixed as the message body */
g_mime_message_set_mime_part (message, (GMimeObject *) mixed);
g_object_unref (mixed);
```

### Encrypting Messages with S/MIME

S/MIME uses an `application/pkcs7-mime` MIME part to encapsulate encrypted content (as well as other things).

```c
GMimeApplicationPkcs7Mime *encrypted;
GMimeMessage *message;
GError *err = NULL;
GMimeObject *body;
GPtrArray *rcpts;

message = g_mime_message_new (TRUE);
g_mime_message_add_mailbox (message, GMIME_ADDRESS_TYPE_FROM, "Joey", "joey@friends.com");
g_mime_message_add_mailbox (message, GMIME_ADDRESS_TYPE_TO, "Alice", "alice@wonderland.com");
g_mime_message_set_subject (message, "How you doin?", NULL);

/* create our message body (perhaps a multipart/mixed with the message text and some
 * image attachments, for example) */
body = CreateMessageBody ();

/* create a list of key ids to encrypt to */
rcpts = g_ptr_array_new ();
g_ptr_array_add (rcpts, "alice@wonderland.com"); // or use her fingerprint

/* now to encrypt our message body */
if (!(encrypted = g_mime_application_pkcs7_mime_encrypt (body, GMIME_ENCRYPT_FLAGS_NONE, rcpts, &err))) {
    fprintf (stderr, "encrypt failed: %s\n", err->message);
    g_object_unref (body);
    g_error_free (err);
    return;
}

g_object_unref (body);

g_mime_message_set_mime_part (message, encrypted);
g_object_unref (encrypted);
```

### Decrypting S/MIME Messages

As mentioned earlier, S/MIME uses an `application/pkcs7-mime` part with an "smime-type" parameter with a value of
"enveloped-data" to encapsulate the encrypted content.

The first thing you must do is find the `GMimeApplicationPkcs7Mime` part (see the section on traversing MIME parts).

```c
if (GMIME_IS_APPLICATION_PKCS7_MIME (entity)) {
    GMimeApplicationPkcs7Mime *pkcs7 = (GMimeApplicationPkcs7Mime *) entity;
    GMimeSecureMimeType smime_type;

    smime_type = g_mime_application_pkcs7_mime_get_smime_type (pkcs7_mime);
    if (smime_type == GMIME_SECURE_MIME_TYPE_ENVELOPED_DATA)
        return g_mime_application_pkcs7_mime_decrypt (pkcs7, 0, NULL, NULL, &err);
}
```

### Encrypting Messages with PGP/MIME

Unlike S/MIME, PGP/MIME uses `multipart/encrypted` to encapsulate its encrypted data.

```c
GMimeMultipartEncrypted *encrypted;
GMimeCryptoContext *ctx;
GMimeMessage *message;
GError *err = NULL;
GMimeObject *body;
GPtrArray *rcpts;

message = g_mime_message_new (TRUE);
g_mime_message_add_mailbox (message, GMIME_ADDRESS_TYPE_FROM, "Joey", "joey@friends.com");
g_mime_message_add_mailbox (message, GMIME_ADDRESS_TYPE_TO, "Alice", "alice@wonderland.com");
g_mime_message_set_subject (message, "How you doin?", NULL);

/* create our message body (perhaps a multipart/mixed with the message text and some
 * image attachments, for example) */
body = CreateMessageBody ();

/* create a list of key ids to encrypt to */
rcpts = g_ptr_array_new ();
g_ptr_array_add (rcpts, "alice@wonderland.com"); // or use her fingerprint

/* now to encrypt our message body */
ctx = g_mime_gpg_context_new ();

encrypted = g_mime_multipart_encrypted_encrypt (ctx, body, FALSE, NULL, 0, 0, rcpts, &err);
g_ptr_array_free (rcpts, TRUE);
g_object_unref (body);
g_object_unref (ctx);

if (encrypted == NULL) {
    fprintf (stderr, "encrypt failed: %s\n", err->message);
    g_error_free (err);
    return;
}

g_mime_message_set_mime_part (message, encrypted);
g_object_unref (encrypted);
```

### Decrypting PGP/MIME Messages

As mentioned earlier, PGP/MIME uses a `multipart/encrypted` part to encapsulate the encrypted content.

A `multipart/encrypted` contains exactly 2 parts: the first `GMimeObject` is the version information while the
second `GMimeObject` is the actual encrypted content and will typically be an `application/octet-stream`.

The first thing you must do is find the `GMimeMultipartEncrypted` part (see the section on traversing MIME parts).

```c
if (GMIME_IS_MULTIPART_ENCRYPTED (entity)) {
    GMimeMultipartEncrypted *encrypted = (GMimeMultipartEncrypted *) entity;

    return g_mime_multipart_encrypted_decrypt (encrypted, 0, NULL, NULL, &err);
}
```

### Digitally Signing Messages with S/MIME or PGP/MIME

Both S/MIME and PGP/MIME use a `multipart/signed` to contain the signed content and the detached signature data.

Here's how you might digitally sign a message using S/MIME:

```c
GMimeApplicationPkcs7Mime *pkcs7;
GMimeMessage *message;
GError *err = NULL;
GMimeObject *body;

message = g_mime_message_new (TRUE);
g_mime_message_add_mailbox (message, GMIME_ADDRESS_TYPE_FROM, "Joey", "joey@friends.com");
g_mime_message_add_mailbox (message, GMIME_ADDRESS_TYPE_TO, "Alice", "alice@wonderland.com");
g_mime_message_set_subject (message, "How you doin?", NULL);

/* create our message body (perhaps a multipart/mixed with the message text and some
 * image attachments, for example) */
body = CreateMessageBody ();

/* now to sign our message body */
if (!(pkcs7 = g_mime_application_pkcs7_mime_sign (body, "joey@friends.com", GMIME_DIGEST_ALGO_DEFAULT, &err))) {
    fprintf (stderr, "sign failed: %s\n", err->message);
    g_object_unref (body);
    g_error_free (err);
    return;
}

g_object_unref (body);

g_mime_message_set_mime_part (message, pkcs7);
g_object_unref (pkcs7);
```

S/MIME also gives you the option of using a `multipart/signed` method of signing a message.

```c
GMimeMultipartSigned *ms;
GMimeCryptoContext *ctx;
GMimeMessage *message;
GError *err = NULL;
GMimeObject *body;

message = g_mime_message_new (TRUE);
g_mime_message_add_mailbox (message, GMIME_ADDRESS_TYPE_FROM, "Joey", "joey@friends.com");
g_mime_message_add_mailbox (message, GMIME_ADDRESS_TYPE_TO, "Alice", "alice@wonderland.com");
g_mime_message_set_subject (message, "How you doin?", NULL);

/* create our message body (perhaps a multipart/mixed with the message text and some
 * image attachments, for example) */
body = CreateMessageBody ();

/* create our crypto context - needed because multipart/signed works for both S/MIME and PGP/MIME */
ctx = g_mime_pkcs7_context_new ();

/* now to sign our message body */
if (!(ms = g_mime_multipart_signed_sign (ctx, body, "joey@friends.com", GMIME_DIGEST_ALGO_DEFAULT, &err))) {
    fprintf (stderr, "sign failed: %s\n", err->message);
    g_object_unref (body);
    g_object_unref (ctx);
    g_error_free (err);
    return;
}

g_object_unref (body);
g_object_unref (ctx);

g_mime_message_set_mime_part (message, ms);
g_object_unref (ms);
```

If you'd prefer to use PGP instead of S/MIME, things work almost exactly the same except that you
would use the `GMimeGpgContext`.

```c
GMimeMultipartSigned *ms;
GMimeCryptoContext *ctx;
GMimeMessage *message;
GError *err = NULL;
GMimeObject *body;

message = g_mime_message_new (TRUE);
g_mime_message_add_mailbox (message, GMIME_ADDRESS_TYPE_FROM, "Joey", "joey@friends.com");
g_mime_message_add_mailbox (message, GMIME_ADDRESS_TYPE_TO, "Alice", "alice@wonderland.com");
g_mime_message_set_subject (message, "How you doin?", NULL);

/* create our message body (perhaps a multipart/mixed with the message text and some
 * image attachments, for example) */
body = CreateMessageBody ();

/* create our crypto context - needed because multipart/signed works for both S/MIME and PGP/MIME */
ctx = g_mime_gpg_context_new ();

/* now to sign our message body */
if (!(ms = g_mime_multipart_signed_sign (ctx, body, "joey@friends.com", GMIME_DIGEST_ALGO_DEFAULT, &err))) {
    fprintf (stderr, "sign failed: %s\n", err->message);
    g_object_unref (body);
    g_object_unref (ctx);
    g_error_free (err);
    return;
}

g_object_unref (body);
g_object_unref (ctx);

g_mime_message_set_mime_part (message, ms);
g_object_unref (ms);
```

### Verifying S/MIME and PGP/MIME Digital Signatures

As mentioned earlier, both S/MIME and PGP/MIME typically use a `multipart/signed` part to contain the
signed content and the detached signature data.

A `multipart/signed` contains exactly 2 parts: the first `GMimeObject` is the signed content while the second
`GMimeObject` is the detached signature and, by default, will either be a `GMimeApplicationPgpSignature` part
or a `GMimeApplicationPkcs7Signature` part (depending on whether the sending client signed using OpenPGP or
S/MIME).

Because the `multipart/signed` part may have been signed by multiple signers, the
`g_mime_multipart_signed_verify()` function returns a `GMimeSignatureList` which contains a list of
digital signatures (one for each signer) that each contain their own metadata describing who that
signer is and what the status of their signature is.

```c
if (GMIME_IS_MULTIPART_SIGNED (entity)) {
    GMimeMultipartSigned *ms = (GMimeMultipartSigned *) entity;
    GMimeSignatureList *signatures;
    GError *err = NULL;
    int i, count;

    if (!(signatures = g_mime_multipart_signed_verify (ms, 0, &err))) {
        fprintf (stderr, "verify failed: %s\n", err->message);
        g_error_free (err);
        return;
    }

    fputs ("\nSignatures:\n", stdout);
    count = g_mime_signature_list_length (signatures);
    for (i = 0; i < count; i++) {
        GMimeSignature *sig = g_mime_signature_list_get_signature (signatures, i);
        GMimeSignatureStatus status = g_mime_signature_get_status (sig);
        GMimeCertificate *cert = g_mime_signature_get_certificate (sig);
        time_t created = g_mime_signature_get_created (sig);
        time_t expires = g_mime_signature_get_expires (sig);

        fprintf (stdout, "\tName: %s\n", g_mime_certificate_get_name (cert));
        fprintf (stdout, "\tKeyId: %s\n", g_mime_certificate_get_key_id (cert));
        fprintf (stdout, "\tFingerprint: %s\n", g_mime_certificate_get_fingerprint (cert));

        fprintf (stdout, "\tTrust: ");
        switch (g_mime_certificate_get_trust (cert)) {
        case GMIME_TRUST_UNKNOWN:   fputs ("Unknown\n", stdout); break;
        case GMIME_TRUST_NEVER:     fputs ("Never\n", stdout); break;
        case GMIME_TRUST_UNDEFINED: fputs ("Undefined\n", stdout); break;
        case GMIME_TRUST_MARGINAL:  fputs ("Marginal\n", stdout); break;
        case GMIME_TRUST_FULL:      fputs ("Full\n", stdout); break;
        case GMIME_TRUST_ULTIMATE:  fputs ("Ultimate\n", stdout); break;
        }

        fprintf (stdout, "\tStatus: ");
        if (status & GMIME_SIGNATURE_STATUS_RED)
            fputs ("BAD\n", stdout);
        else if (status & GMIME_SIGNATURE_STATUS_GREEN)
            fputs ("GOOD\n", stdout);
        else if (status & GMIME_SIGNATURE_STATUS_VALID)
            fputs ("VALID\n", stdout);
        else
            fputs ("UNKNOWN\n", stdout);

        fprintf (stdout, "\tSignature made on %s", ctime (&created));
        if (expires != (time_t) 0)
            fprintf (stdout, "\tSignature expires on %s", ctime (&expires));
        else
            fprintf (stdout, "\tSignature never expires\n");
	
        fprintf (stdout, "\tErrors: ");
        if (status & GMIME_SIGNATURE_STATUS_KEY_REVOKED)
            fputs ("Key Revoked, ", stdout);
        if (status & GMIME_SIGNATURE_STATUS_KEY_EXPIRED)
            fputs ("Key Expired, ", stdout);
        if (status & GMIME_SIGNATURE_STATUS_SIG_EXPIRED)
            fputs ("Sig Expired, ", stdout);
        if (status & GMIME_SIGNATURE_STATUS_KEY_MISSING)
            fputs ("Key Missing, ", stdout);
        if (status & GMIME_SIGNATURE_STATUS_CRL_MISSING)
            fputs ("CRL Missing, ", stdout);
        if (status & GMIME_SIGNATURE_STATUS_CRL_TOO_OLD)
            fputs ("CRL Too Old, ", stdout);
        if (status & GMIME_SIGNATURE_STATUS_BAD_POLICY)
            fputs ("Bad Policy, ", stdout);
        if (status & GMIME_SIGNATURE_STATUS_SYS_ERROR)
            fputs ("System Error, ", stdout);
        if (status & GMIME_SIGNATURE_STATUS_TOFU_CONFLICT)
            fputs ("Tofu Conflict", stdout);
        if ((status & GMIME_SIGNATURE_STATUS_ERROR_MASK) == 0)
            fputs ("None", stdout);
        fputs ("\n\n", stdout);
    }

    g_object_unref (signatures);
}
```

It should be noted, however, that while most S/MIME clients will use the preferred `multipart/signed`
approach, it is possible that you may encounter an `application/pkcs7-mime` part with an "smime-type"
parameter set to "signed-data". Luckily, GMime can handle this format as well:

```c
if (GMIME_IS_APPLICATION_PKCS7_MIME (entity)) {
    GMimeApplicationPkcs7Mime *pkcs7 = (GMimeApplicationPkcs7Mime *) entity;
    GMimeSecureMimeType smime_type;

    smime_type = g_mime_application_pkcs7_mime_get_smime_type (pkcs7);

    if (smime_type == GMIME_SECURE_MIME_TYPE_SIGNED_DATA) {
        /* extract the original content and get a list of signatures */
        GMimeSignatureList *signatures;
        GMimeObject extracted;
        GError *err = NULL;
        int i, count;

        /* Note: if you are rendering the message, you'll want to render the
         * extracted mime part rather than the application/pkcs7-mime part. */

        if (!(signatures = g_mime_application_pkcs7_mime_verify (pkcs7, 0, &extracted, &err))) {
            fprintf (stderr, "verify failed: %s\n", err->message);
            g_error_free (err);
            return;
        }

        count = g_mime_signature_list_length (signatures);
        for (i = 0; i < count; i++) {
            GMimeSignature *sig = g_mime_signature_list_get_signature (signatures, i);
            /* ... */
        }

	g_object_unref (signatures);
    }
}
```

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
(and therefor managed by) the `GMimeDataWrapper` object:

```c
GMimeStream *stream = g_mime_data_wrapper_get_stream (wrapper);
```

In the above example, the `stream` *should not* be unreffed because the `stream` object's memory is managed by the
`wrapper` object. In other words, when the `wrapper` object is freed, the `stream` will be freed as well.

Where many developers get confused is when they create a new object and then add it to another object.

For example:

```c
GMimeStream *stream = g_mime_stream_mem_new ();
GMimeDataWrapper *wrapper = g_mime_data_wrapper_new_with_stream (stream, GMIME_CONTENT_ENCODING_DEFAULT);
g_object_unref (stream);
```

Even in this situation, it is necessary to unref the allocated object because all GMime functions that add an
object to another object will ref that object themselves.

In other words, in the above example, the `GMimeStream` is being added to the `GMimeDataWrapper`. The
`GMimeDataWrapper` calls `g_object_ref()` on the `stream` which means that the `stream` object now has
a ref count of 2.

When `g_object_unref()` is called on the stream after adding it to the `GMimeDataWrapper`, the ref count drops
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
|COPYING        |The GNU Lesser General Public License, version 2|
|INSTALL        |In-depth installation instructions|
|TODO           |Description of planned GMime development|
|PORTING        |Guide for developers porting their application from an older version of GMime|

You can find online developer documentation at
http://library.gnome.org/devel/gmime/stable/


# Mailing Lists

For discussion of GMime development (either of GMime itself or using
GMime in your own application), you may find the GMime-Devel
mailing-list helpful. To subscribe, please see
[http://mail.gnome.org/mailman/listinfo/gmime-devel-list](http://mail.gnome.org/mailman/listinfo/gmime-devel-list)


# Bindings

Other developers have been working to make GMime available to programmers in other languages.
The current list of known bindings are:

|Language   |Location                                |
|-----------|----------------------------------------|
|Go         |https://github.com/sendgrid/go-gmime    |
|Perl       |http://search.cpan.org/dist/MIME-Fast   |
|C#         |https://github.com/jstedfast/MimeKit    |


# Reporting Bugs

Bugs may be reported to the GMime development team by creating a new
[issue](https://github.com/jstedfast/gmime/issues).
