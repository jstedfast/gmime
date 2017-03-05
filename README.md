# GMime

[![Build Status](https://travis-ci.org/jstedfast/gmime.svg)](https://travis-ci.org/jstedfast/gmime)[![Coverity Scan Build Status](https://scan.coverity.com/projects/11948/badge.svg)](https://scan.coverity.com/projects/jstedfast-gmime)[![Coverage Status](https://coveralls.io/repos/github/jstedfast/gmime/badge.svg?branch=master)](https://coveralls.io/github/jstedfast/gmime?branch=master)

## What is GMime?

GMime is a C/C++ library which may be used for the creation and parsing of messages using the Multipurpose
Internet Mail Extension (MIME) as defined by [numerous IETF specifications](https://github.com/jstedfast/gmime/blob/master/RFCs.md).

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

The GMime library is Copyright (C) 2000-2017 Jeffrey Stedfast and is licensed under the LGPL v2.1

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

You can download either official public release tarballs of GMime at
[http://download.gnome.org/pub/GNOME/sources/gmime/](http://download.gnome.org/pub/GNOME/sources/gmime/)
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

## Using GMime

### Parsing Messages

One of the more common operations that GMime is meant for is parsing email messages from arbitrary streams.
To do this, you will need to use a `GMimeParser`:

```c
/* load a GMimeMessage from a stream */
GMimeMessage *message;
GMimeStream *stream;
GMimeParser *parser;

stream = g_mime_stream_file_open ("message.eml", "r");
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

stream = g_mime_stream_file_open ("Inbox.mbox", "r");
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

### Traversing a MimeMessage

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
GMimeMessage *message = g_mime_message_new (TRUE);
InternetAddressMailbox *mailbox;
InternetAddressList *list;
GMimeDataWrapper *content;
GMimeStream *stream;
GMimePart *body;

list = g_mime_message_get_from (message);
mailbox = internet_address_mailbox_new ("Joey", "joey@friends.com");
internet_address_list_add (list, mailbox);
g_object_unref (mailbox);

list = g_mime_message_get_to (message);
mailbox = internet_address_mailbox_new ("Alice", "alice@wonderland.com");
internet_address_list_add (list, mailbox);
g_object_unref (mailbox);

g_mime_message_set_subject (message, "How you doin?", NULL);

body = g_mime_part_new_with_type ("text", "plain");

stream = g_mime_stream_mem_new ();
g_mime_stream_printf (stream, "Hey Alice,\n\n");
g_mime_stream_printf (stream, "What are you up to this weekend? Monica is throwing one of her parties on\n");
g_mime_stream_printf (stream, "Saturday and I was hoping you could make it.\n\n);
g_mime_stream_printf (stream, "Will you be my +1?\n\n");
g_mime_stream_printf (stream, "-- Joey\n");
g_mime_stream_reset (stream);

content = g_mime_data_wrapper_new_with_stream (stream, GMIME_CONTENT_ENCODING_DEFAULT);
g_object_unref (stream);

g_mime_part_set_content_object (body, content);
g_object_unref (content);

g_mime_message_set_mime_part (message, body);
g_object_unref (body);
```

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
|.NET (Mono)|Included in this distribution (Obsolete)|

Note: It is recommended that [MimeKit](https://github.com/jstedfast/MimeKit) be used in place of GMime-Sharp in
.NET 4.0 (or later) environments.


# Reporting Bugs

Bugs may be reported to the GMime development team by creating a new
[issue](https://github.com/jstedfast/gmime/issues).
