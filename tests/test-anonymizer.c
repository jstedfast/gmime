/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* GMime - Copyright (C) 2026 Jeffrey Stedfast
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <errno.h>
#include <string.h>
#include <gmime/gmime.h>
#include "testsuite.h"

typedef struct {
	GMimeStreamMem parent;
	gsize fail_after;
	gsize chunk_size;
	gboolean zero_write;
	gboolean fail_flush;
	gboolean fail_read;
	guint fail_flush_after;
	guint flushes;
	guint closes;
} TestOutput;

typedef struct {
	GMimeStreamMemClass parent_class;
} TestOutputClass;

G_DEFINE_TYPE (TestOutput, test_output, GMIME_TYPE_STREAM_MEM)

static ssize_t
output_write (GMimeStream *stream, const char *buffer, size_t length)
{
	TestOutput *output = (TestOutput *) stream;

	if ((guint64) stream->position >= output->fail_after) {
		errno = EIO;
		return -1;
	}
	if (output->zero_write)
		return 0;
	length = MIN (length, output->chunk_size);
	length = MIN (length, output->fail_after - stream->position);
	return GMIME_STREAM_CLASS (test_output_parent_class)->write (stream, buffer, length);
}

static int
output_flush (GMimeStream *stream)
{
	TestOutput *output = (TestOutput *) stream;

	output->flushes++;
	if (output->fail_flush || output->flushes > output->fail_flush_after) {
		errno = ENOSPC;
		return -1;
	}
	return 0;
}

static ssize_t
output_read (GMimeStream *stream, char *buffer, size_t length)
{
	if (((TestOutput *) stream)->fail_read) {
		errno = EIO;
		return -1;
	}
	return GMIME_STREAM_CLASS (test_output_parent_class)->read (stream, buffer, length);
}

static int
output_close (GMimeStream *stream)
{
	((TestOutput *) stream)->closes++;
	return GMIME_STREAM_CLASS (test_output_parent_class)->close (stream);
}

static void
test_output_class_init (TestOutputClass *klass)
{
	GMimeStreamClass *stream_class = GMIME_STREAM_CLASS (klass);

	stream_class->write = output_write;
	stream_class->read = output_read;
	stream_class->flush = output_flush;
	stream_class->close = output_close;
}

static void
test_output_init (TestOutput *output)
{
	g_mime_stream_mem_set_byte_array (GMIME_STREAM_MEM (output), g_byte_array_new ());
	g_mime_stream_mem_set_owner (GMIME_STREAM_MEM (output), TRUE);
	output->fail_after = G_MAXSIZE;
	output->chunk_size = 3;
	output->fail_flush_after = G_MAXUINT;
}

static GMimeHeader *
add_raw_header (GMimeHeaderList *headers, const char *field, const char *raw, gsize length)
{
	GMimeHeader *header = g_mime_header_new (field, "", NULL);
	GBytes *value = g_bytes_new (raw, length);
	GError *error = NULL;

	g_assert_true (g_mime_header_set_raw_value (header, value, &error));
	g_assert_no_error (error);
	g_assert_cmpint (g_mime_header_list_add (headers, header), >=, 0);
	g_object_unref (header);
	g_bytes_unref (value);
	return header;
}

static void
assert_output (GMimeStream *stream, const char *expected, gsize length)
{
	GByteArray *bytes = g_mime_stream_mem_get_byte_array (GMIME_STREAM_MEM (stream));

	if (bytes->len != length || (length && memcmp (bytes->data, expected, length)))
		g_printerr ("Expected: %.*s\nActual: %.*s\n", (int) length, expected, (int) bytes->len, bytes->data);
	g_assert_cmpuint (bytes->len, ==, length);
	g_assert_cmpmem (bytes->data, bytes->len, expected, length);
}

static void
test_configuration (void)
{
	GMimeAnonymizer *anonymizer = g_mime_anonymizer_new ();
	char name[] = "SuBJeCt";
	char **names;

	names = g_mime_anonymizer_get_preserved_headers (anonymizer);
	g_assert_nonnull (names);
	g_assert_null (names[0]);
	g_strfreev (names);
	g_assert_false (g_mime_anonymizer_contains_preserved_header (anonymizer, "Date"));
	g_assert_true (g_mime_anonymizer_add_preserved_header (anonymizer, name));
	name[0] = '!';
	g_assert_false (g_mime_anonymizer_add_preserved_header (anonymizer, "subject"));
	g_assert_true (g_mime_anonymizer_contains_preserved_header (anonymizer, "SUBJECT"));
	names = g_mime_anonymizer_get_preserved_headers (anonymizer);
	g_assert_cmpstr (names[0], ==, "SuBJeCt");
	g_assert_null (names[1]);
	g_assert_true (g_mime_anonymizer_remove_preserved_header (anonymizer, "subject"));
	g_assert_false (g_mime_anonymizer_remove_preserved_header (anonymizer, "subject"));
	g_assert_cmpstr (names[0], ==, "SuBJeCt");
	g_strfreev (names);
	g_assert_true (g_mime_anonymizer_add_preserved_header (anonymizer, "X-Custom"));
	g_mime_anonymizer_clear_preserved_headers (anonymizer);
	g_assert_false (g_mime_anonymizer_contains_preserved_header (anonymizer, "X-Custom"));
	g_object_unref (anonymizer);
}

static void
test_masking (void)
{
	static const struct {
		const char *field;
		const char *raw;
		const char *expected;
	} cases[] = {
		{ "Received", " from host.example by [::1] with ESMTP id abc\r\n",
		  " from xxxx.xxxxxxx by [::x] with xxxxx id xxx\r\n" },
		{ "Received", " (qmail 21619 invoked from network); 15 Nov 2017 14:16:18 -0000\r\n",
		  " (xxxxx xxxxx xxxxxxx xxxx xxxxxxx); 15 Nov 2017 14:16:18 -0000\r\n" },
		{ "Received", " from x (a (b) \\) c) BY y; nonsense\r\n",
		  " from x (x (x) \\x x) BY x; xxxxxxxx\r\n" },
		{ "Received", " by from\r\n", " by from\r\n" },
		{ "From", " \"User Name\" <user@example.org>, a\\)b@host\r\n",
		  " \"xxxxxxxxx\" <xxxx@xxxxxxx.xxx>, x\\)x@xxxx\r\n" },
		{ "Message-Id", " <abc.123@example.org>\r\n", " <xxx.xxx@xxxxxxx.xxx>\r\n" },
		{ "Subject", " =?utf-8?Q?Secret=20text?= plain?\r\n",
		  " =?utf-8?Q?xxxxxxxxxxxxx?= xxxxx?\r\n" },
		{ "Subject", " =a =?x?y?p?z =?x?y?p==?= ??\r\n",
		  " =x =?x?y?x?x =?x?y?xxx?= ??\r\n" },
		{ "Content-Type", " text/plain; charset=utf-8; name=\"private.txt\"; format=flowed; delsp=yes\r\n",
		  " text/plain; charset=utf-8; name=\"xxxxxxxxxxx\"; format=flowed; delsp=yes\r\n" },
		{ "Content-Disposition", " attachment; filename*=utf-8''secret.txt; boundary=abc\r\n",
		  " attachment; filename*=xxxxxxxxxxxxxxxxx; boundary=abc\r\n" },
		{ "Content-Type", " text/plain; charset =utf-8; x=\"a\\\"b\r\n c\"; ; format=flowed\r\n",
		  " text/plain; charset =xxxxx; x=\"x\\\"x\r\nxx\"; ; format=flowed\r\n" },
		{ "Content-Type", " text/plain; char*;set=utf-8\r\n", " text/plain; char*;set=utf-8\r\n" },
		{ "Date", " Fri, 29 Nov 91 07:13:33 EST\r\n", " Fri, 29 Nov 91 07:13:33 EST\r\n" },
		{ "MIME-Version", " 1.0\r\n", " 1.0\r\n" },
		{ "Content-Transfer-Encoding", " base64\r\n", " base64\r\n" },
		{ "Resent-Date", " Fri, 29 Nov 91 07:13:33 EST\r\n", " xxxx xx xxx xx xxxxxxxx xxx\r\n" },
		{ "DKIM-Signature", " v=1; b=secret;\r\n", " x=xx x=xxxxxxx\r\n" }
	};
	GMimeAnonymizer *anonymizer = g_mime_anonymizer_new ();
	GMimeFormatOptions *options = g_mime_format_options_new ();
	GMimeHeaderList *headers;
	GMimeHeader *header;
	GMimeStream *stream;
	GBytes *before, *after;
	GError *error = NULL;
	char *expected;
	guint i;

	g_mime_format_options_set_newline_format (options, GMIME_NEWLINE_FORMAT_DOS);
	for (i = 0; i < G_N_ELEMENTS (cases); i++) {
		headers = g_mime_header_list_new (NULL);
		header = add_raw_header (headers, cases[i].field, cases[i].raw, strlen (cases[i].raw));
		before = g_mime_header_get_raw_value (header);
		stream = g_mime_stream_mem_new ();
		g_assert_true (g_mime_anonymizer_write_headers (anonymizer, options, headers, stream, &error));
		g_assert_no_error (error);
		expected = g_strconcat (cases[i].field, ":", cases[i].expected, NULL);
		assert_output (stream, expected, strlen (expected));
		after = g_mime_header_get_raw_value (header);
		g_assert_true (g_bytes_equal (before, after));
		g_bytes_unref (before);
		g_bytes_unref (after);
		g_free (expected);
		g_object_unref (stream);
		g_object_unref (headers);
	}
	g_mime_format_options_free (options);
	g_object_unref (anonymizer);
}

static void
test_writing (void)
{
	static const char raw[] = " \0\xff\v\f A\r\n";
	static const char expected[] = "Subject: \0\xff\v\f A\nSubject: keep me\n";
	GMimeAnonymizer *anonymizer = g_mime_anonymizer_new ();
	GMimeHeaderList *headers = g_mime_header_list_new (NULL);
	GMimeFormatOptions *options = g_mime_format_options_new ();
	GMimeStream *stream;
	GError *error = NULL;

	add_raw_header (headers, "Subject", raw, sizeof raw - 1);
	add_raw_header (headers, "Subject", " keep me\r\n", 10);
	g_mime_header_list_append (headers, "X-Unset", NULL, NULL);
	g_mime_format_options_set_newline_format (options, GMIME_NEWLINE_FORMAT_UNIX);
	g_mime_format_options_add_hidden_header (options, "Subject");
	stream = g_mime_stream_mem_new ();
	g_assert_true (g_mime_anonymizer_write_headers (anonymizer, options, headers, stream, &error));
	g_assert_no_error (error);
	assert_output (stream, "Subject: xxxx x\nSubject: xxxx xx\n", sizeof expected - 1);
	g_object_unref (stream);
	g_assert_true (g_mime_anonymizer_add_preserved_header (anonymizer, "subject"));
	stream = g_mime_stream_mem_new ();
	g_assert_true (g_mime_anonymizer_write_headers (anonymizer, options, headers, stream, &error));
	g_assert_no_error (error);
	assert_output (stream, "Subject: \0\xff\v\f A\nSubject: keep me\n", sizeof expected - 1);
	g_object_unref (stream);
	g_mime_format_options_free (options);
	g_object_unref (headers);
	g_object_unref (anonymizer);
}

static void
test_loaded_headers (void)
{
	static const char raw[] = "Subject: secret\r\nbad\0field\r\nNext: private\r\nLast: eof";
	static const char expected[] = "Subject: xxxxxx\nxxxxxxxxx\nNext: xxxxxxx\nLast: xxx";
	GMimeAnonymizer *anonymizer = g_mime_anonymizer_new ();
	GMimeStream *input = g_mime_stream_mem_new_with_buffer (raw, sizeof raw - 1);
	GMimeStream *output = g_mime_stream_mem_new ();
	GMimeHeaderList *headers;
	GError *error = NULL;
	TestOutput *failing;

	headers = g_mime_header_list_load (NULL, input, &error);
	g_assert_no_error (error);
	g_assert_nonnull (headers);
	g_assert_true (g_mime_anonymizer_write_headers (anonymizer, NULL, headers, output, &error));
	g_assert_no_error (error);
	assert_output (output, expected, sizeof expected - 1);

	failing = g_object_new (test_output_get_type (), NULL);
	failing->fail_after = 20;
	g_assert_false (g_mime_anonymizer_write_headers (anonymizer, NULL, headers, GMIME_STREAM (failing), &error));
	g_assert_error (error, GMIME_ERROR, EIO);
	g_clear_error (&error);
	g_assert_cmpuint (failing->flushes, ==, 0);
	g_assert_cmpuint (failing->closes, ==, 0);
	g_object_unref (failing);

	failing = g_object_new (test_output_get_type (), NULL);
	failing->fail_flush = TRUE;
	g_assert_false (g_mime_anonymizer_write_headers (anonymizer, NULL, headers, GMIME_STREAM (failing), &error));
	g_assert_error (error, GMIME_ERROR, ENOSPC);
	g_clear_error (&error);
	g_assert_cmpuint (failing->flushes, ==, 1);
	g_assert_cmpuint (failing->closes, ==, 0);
	assert_output (GMIME_STREAM (failing), "Subject: xxxxxx\nxxxxxxxxx\n", 26);
	g_object_unref (failing);
	g_object_unref (output);
	g_object_unref (input);
	g_object_unref (headers);
	g_object_unref (anonymizer);
}

static char *
fail_formatting (GMimeHeader *header, GMimeFormatOptions *options, const char *value, const char *charset)
{
	return NULL;
}

static void
test_formatting (void)
{
	GMimeAnonymizer *anonymizer = g_mime_anonymizer_new ();
	GMimeFormatOptions *options = g_mime_format_options_new ();
	GMimeHeaderList *headers = g_mime_header_list_new (NULL);
	GMimeHeader *header = g_mime_header_parse_text (NULL, "Subject: =?utf-8?q?caf=C3=A9?=\r\n", NULL);
	GMimeStream *output = g_mime_stream_mem_new ();
	GError *error = NULL;

	g_assert_nonnull (header);
	g_mime_header_list_add (headers, header);
	g_mime_format_options_set_allow_international (options, TRUE);
	g_assert_true (g_mime_anonymizer_write_headers (anonymizer, options, headers, output, &error));
	g_assert_no_error (error);
	assert_output (output, "Subject: xxxxx\n", 15);
	g_object_unref (output);
	g_assert_true (g_mime_anonymizer_add_preserved_header (anonymizer, "Subject"));
	output = g_mime_stream_mem_new ();
	g_assert_true (g_mime_anonymizer_write_headers (anonymizer, options, headers, output, &error));
	g_assert_no_error (error);
	assert_output (output, "Subject: caf\xc3\xa9\n", 15);
	g_object_unref (output);
	g_object_unref (header);

	header = g_mime_header_new ("X-Failure", "value", NULL);
	header->reformat = TRUE;
	header->formatter = fail_formatting;
	g_mime_header_list_add (headers, header);
	output = g_mime_stream_mem_new ();
	g_assert_false (g_mime_anonymizer_write_headers (anonymizer, options, headers, output, &error));
	g_assert_error (error, GMIME_ERROR, EINVAL);
	g_clear_error (&error);
	g_assert_cmpint (g_mime_header_write_to_stream (header, options, output), ==, -1);
	g_assert_cmpint (errno, ==, EINVAL);
	g_object_unref (output);
	g_object_unref (header);
	g_object_unref (headers);
	g_mime_format_options_free (options);
	g_object_unref (anonymizer);
}

static void
test_failures (void)
{
	GMimeAnonymizer *anonymizer = g_mime_anonymizer_new ();
	GMimeHeaderList *headers = g_mime_header_list_new (NULL);
	GMimeStream *stream;
	TestOutput *output;
	GError *error = NULL;
	guint i;

	add_raw_header (headers, "Subject", " secret\n", 8);
	add_raw_header (headers, "Subject", " secret\n", 8);
	for (i = 0; i < 5; i++) {
		output = g_object_new (test_output_get_type (), NULL);
		stream = GMIME_STREAM (output);
		if (i == 0 || i == 1)
			output->fail_after = i == 0 ? 0 : 20;
		else if (i == 2)
			output->zero_write = TRUE;
		else if (i == 3)
			output->fail_flush = TRUE;
		if (i < 4) {
			g_assert_false (g_mime_anonymizer_write_headers (anonymizer, NULL, headers, stream, &error));
			g_assert_error (error, GMIME_ERROR, (i == 3 ? ENOSPC : EIO));
			g_clear_error (&error);
			g_assert_cmpuint (output->flushes, ==, i == 3 ? 1 : 0);
		} else {
			g_assert_true (g_mime_anonymizer_write_headers (anonymizer, NULL, headers, stream, &error));
			g_assert_no_error (error);
			assert_output (stream, "Subject: xxxxxx\nSubject: xxxxxx\n", 32);
		}
		g_assert_cmpuint (output->closes, ==, 0);
		g_object_unref (output);
	}
	g_object_unref (headers);
	g_object_unref (anonymizer);
}

static void
test_entities (void)
{
	static const struct {
		const char *wire;
		const char *expected;
	} cases[] = {
		{ "Subject: private\r\nContent-Type: text/plain\r\n\r\nbody",
		  "Subject: xxxxxxx\r\nContent-Type: text/plain\r\n\r\nxxxx" },
		{ "Content-Type: application/octet-stream\r\nContent-Transfer-Encoding: binary\r\n\r\nprivate",
		  "Content-Type: application/octet-stream\r\nContent-Transfer-Encoding: binary\r\n\r\nxxxxxxx" },
		{ "Content-Type: message/rfc822\r\n\r\nSubject: private\r\n\r\nbody",
		  "Content-Type: message/rfc822\r\n\r\nSubject: xxxxxxx\r\n\r\nxxxx" },
		{ "Content-Type: message/delivery-status\r\n\r\nX-Private: secret\r\n\r\nAction: failed\r\n",
		  "Content-Type: message/delivery-status\r\n\r\nX-Private: xxxxxx\r\n\r\nAction: failed\r\n" },
		{ "Content-Type: message/disposition-notification\r\n\r\nTo: user@example.org\r\n",
		  "Content-Type: message/disposition-notification\r\n\r\nTo: xxxx@xxxxxxx.xxx\r\n" },
		{ "Content-Type: multipart/mixed\r\n\r\nprivate",
		  "Content-Type: multipart/mixed\r\n\r\nxxxxxxx" },
		{ "Content-Type: multipart/mixed",
		  "Content-Type: multipart/mixed" },
		{ "Content-Type: multipart/mixed; boundary=x\r\n\r\nprivate\r\n"
		  "--x \t\r\nContent-Type: text/plain\r\n\r\nsecret\r\n--x-- \t\r\nepilogue",
		  "Content-Type: multipart/mixed; boundary=x\r\n\r\nxxxxxxx\r\n"
		  "--x \t\r\nContent-Type: text/plain\r\n\r\nxxxxxx\r\n--x-- \t\r\nxxxxxxxx" },
		{ "Content-Type: multipart/mixed; boundary=x\r\n\r\n--x\r\n"
		  "Content-Type: text/plain\r\n\r\nsecret\r\n--x",
		  "Content-Type: multipart/mixed; boundary=x\r\n\r\n--x\r\n"
		  "Content-Type: text/plain\r\n\r\nxxxxxx\r\n--x" }
	};
	GMimeAnonymizer *anonymizer = g_mime_anonymizer_new ();
	GMimeFormatOptions *options = g_mime_format_options_new ();
	GError *error = NULL;
	guint i, message;

	g_mime_format_options_set_newline_format (options, GMIME_NEWLINE_FORMAT_DOS);
	g_mime_format_options_add_hidden_header (options, "Subject");
	g_mime_anonymizer_add_preserved_header (anonymizer, "Action");
	for (i = 0; i < G_N_ELEMENTS (cases); i++) {
		for (message = 0; message < 2; message++) {
			GMimeStream *input = g_mime_stream_mem_new_with_buffer (cases[i].wire, strlen (cases[i].wire));
			GMimeEntity *entity = message ? (GMimeEntity *) g_mime_message_load (NULL, input, &error) :
				g_mime_entity_load (NULL, input, &error);
			GMimeStream *output = g_object_new (test_output_get_type (), NULL);
			GMimeEntity *body;
			gboolean missing_boundary;

			g_assert_no_error (error);
			g_assert_nonnull (entity);
			body = message ? g_mime_message_get_mime_part (GMIME_MESSAGE (entity)) : entity;
			missing_boundary = GMIME_IS_MULTIPART (body) &&
				!g_mime_entity_get_content_type_parameter (body, "boundary");
			g_assert_true (g_mime_anonymizer_write_entity (anonymizer, options, entity, output, &error));
			g_assert_no_error (error);
			if (missing_boundary)
				g_assert_null (g_mime_entity_get_content_type_parameter (body, "boundary"));
			assert_output (output, cases[i].expected, strlen (cases[i].expected));
			g_assert_cmpuint (((TestOutput *) output)->closes, ==, 0);
			g_object_unref (output);
			g_object_unref (entity);
			g_object_unref (input);
		}
	}
	g_mime_format_options_free (options);
	g_object_unref (anonymizer);
}

static void
test_constructed_message (void)
{
	static const char expected[] = "Subject: xxxxxxx\nMIME-Version: 1.0\nContent-Type: application/octet-stream\n"
		"Content-Transfer-Encoding: binary\n\nxxxxxxxx";
	GMimeAnonymizer *anonymizer = g_mime_anonymizer_new ();
	GMimeMessage *message = g_mime_message_new (FALSE);
	GMimePart *part = g_mime_part_new_with_type ("application", "octet-stream");
	const char binary[] = "secret\0\xff";
	GMimeStream *input = g_mime_stream_mem_new_with_buffer (binary, sizeof binary - 1);
	GMimeContent *content = g_mime_content_new_with_stream (input, GMIME_CONTENT_ENCODING_BINARY);
	GMimeStream *output = g_mime_stream_mem_new ();
	GError *error = NULL;

	g_mime_header_list_clear (GMIME_ENTITY (message)->headers);
	g_mime_message_set_subject (message, "private", NULL);
	g_mime_part_set_content (part, content);
	g_mime_part_set_content_encoding (part, GMIME_CONTENT_ENCODING_BINARY);
	g_mime_message_set_mime_part (message, GMIME_ENTITY (part));
	g_mime_header_list_remove (GMIME_ENTITY (message)->headers, "MIME-Version");
	g_assert_false (g_mime_header_list_contains (GMIME_ENTITY (message)->headers, "MIME-Version"));
	g_assert_true (g_mime_anonymizer_write_message (anonymizer, NULL, message, output, &error));
	g_assert_no_error (error);
	g_assert_true (g_mime_header_list_contains (GMIME_ENTITY (message)->headers, "MIME-Version"));
	assert_output (output, expected, sizeof expected - 1);
	g_assert_cmpint (g_mime_stream_tell (input), ==, 0);
	g_assert_cmpint (g_mime_stream_read (input, (char [8]) { 0 }, 8), ==, 8);
	g_object_unref (output);
	g_object_unref (content);
	g_object_unref (input);
	g_object_unref (part);
	g_object_unref (message);
	g_object_unref (anonymizer);
}

static void
test_constructed_multipart (void)
{
	static const char wire[] = "Content-Type: text/plain\n\nprivate";
	static const char prefix[] = "Content-Type: multipart/mixed; boundary=x\n\nxxxxxxx\n"
		"--x\nContent-Type: text/plain\n\nxxxxxxx";
	GMimeAnonymizer *anonymizer = g_mime_anonymizer_new ();
	GMimeMultipart *multipart = g_mime_multipart_new ();
	GMimeStream *input = g_mime_stream_mem_new_with_buffer (wire, sizeof wire - 1);
	GError *error = NULL;
	GMimeEntity *child = g_mime_entity_load (NULL, input, &error);
	guint i;

	g_assert_no_error (error);
	g_mime_multipart_set_boundary (multipart, "x");
	g_mime_multipart_set_prologue (multipart, "private");
	g_mime_multipart_set_epilogue (multipart, "private");
	g_mime_multipart_add (multipart, child);
	for (i = 0; i < 2; i++) {
		GMimeStream *output = g_mime_stream_mem_new ();
		char *expected = i ? g_strdup (prefix) : g_strconcat (prefix, "\n--x--\nxxxxxxx", NULL);

		multipart->write_end_boundary = i == 0;
		g_assert_true (g_mime_anonymizer_write_entity (anonymizer, NULL, GMIME_ENTITY (multipart), output, &error));
		g_assert_no_error (error);
		assert_output (output, expected, strlen (expected));
		g_assert_cmpstr (g_mime_multipart_get_prologue (multipart), ==, "private");
		g_assert_cmpstr (g_mime_multipart_get_epilogue (multipart), ==, "private");
		g_free (expected);
		g_object_unref (output);
	}
	{
		static const char expected[] = "Content-Type: multipart/mixed; boundary=outer\n\n--outer\n"
			"Content-Type: multipart/mixed; boundary=x\n\nxxxxxxx\n--x\n"
			"Content-Type: text/plain\n\n--outer--\n";
		GMimeMultipart *outer = g_mime_multipart_new ();
		GMimeStream *output = g_mime_stream_mem_new ();
		GMimePart *empty = g_mime_part_new_with_type ("text", "plain");

		g_mime_multipart_set_boundary (outer, "outer");
		g_mime_multipart_clear (multipart);
		g_mime_multipart_add (multipart, GMIME_ENTITY (empty));
		g_object_unref (empty);
		multipart->write_end_boundary = FALSE;
		g_mime_multipart_add (outer, GMIME_ENTITY (multipart));
		g_assert_true (g_mime_anonymizer_write_entity (anonymizer, NULL, GMIME_ENTITY (outer), output, &error));
		g_assert_no_error (error);
		assert_output (output, expected, sizeof expected - 1);
		g_object_unref (output);
		g_object_unref (outer);
	}
	g_object_unref (input);
	g_object_unref (child);
	g_object_unref (multipart);
	g_object_unref (anonymizer);
}

static void
test_deep_entities (void)
{
	GMimeAnonymizer *anonymizer = g_mime_anonymizer_new ();
	GMimeParserOptions *parser_options = g_mime_parser_options_new ();
	GString *wire = g_string_new (NULL);
	GString *expected;
	GMimeEntity *entity;
	GMimeStream *input, *output;
	GError *error = NULL;
	guint i;

	g_mime_parser_options_set_max_mime_depth (parser_options, G_MAXINT);
	for (i = 0; i < 2000; i++) {
		if (i % 2)
			g_string_append (wire, "Content-Type: message/rfc822\n\n");
		else
			g_string_append_printf (wire, "Content-Type: multipart/mixed; boundary=%u\n\n--%u\n", i, i);
	}
	g_string_append (wire, "Content-Type: text/plain\n\n");
	expected = g_string_new_len (wire->str, wire->len);
	g_string_append (wire, "innermost");
	g_string_append (expected, "xxxxxxxxx");
	for (i = 2000; i > 0; i--) {
		if ((i - 1) % 2)
			continue;
		g_string_append_printf (wire, "\n--%u--", i - 1);
		g_string_append_printf (expected, "\n--%u--", i - 1);
	}
	g_string_append_c (wire, '\n');
	g_string_append_c (expected, '\n');
	input = g_mime_stream_mem_new_with_buffer (wire->str, wire->len);
	entity = g_mime_entity_load (parser_options, input, &error);
	g_assert_no_error (error);
	g_assert_nonnull (entity);
	output = g_mime_stream_mem_new ();
	g_assert_true (g_mime_anonymizer_write_entity (anonymizer, NULL, entity, output, &error));
	g_assert_no_error (error);
	assert_output (output, expected->str, expected->len);
	g_object_unref (entity);
	g_object_unref (input);
	g_object_unref (output);
	g_mime_parser_options_free (parser_options);
	g_string_free (wire, TRUE);
	g_string_free (expected, TRUE);
	g_object_unref (anonymizer);
}

static void
test_body_failures (void)
{
	static const char wire[] = "Content-Type: multipart/mixed; boundary=x\n\npreamble\n"
		"--x\nContent-Type: text/plain\n\nprivate\n--x--\nepilogue";
	static const char expected[] = "Content-Type: multipart/mixed; boundary=x\n\nxxxxxxxx\n"
		"--x\nContent-Type: text/plain\n\nxxxxxxx\n--x--\nxxxxxxxx";
	GMimeAnonymizer *anonymizer = g_mime_anonymizer_new ();
	GMimeStream *input = g_mime_stream_mem_new_with_buffer (wire, sizeof wire - 1);
	GMimeEntity *entity;
	GError *error = NULL;
	guint i;

	entity = g_mime_entity_load (NULL, input, &error);
	g_assert_no_error (error);
	for (i = 0; i < sizeof expected - 1; i++) {
		TestOutput *output = g_object_new (test_output_get_type (), NULL);

		output->fail_after = i;
		g_assert_false (g_mime_anonymizer_write_entity (anonymizer, NULL, entity, GMIME_STREAM (output), &error));
		g_assert_error (error, GMIME_ERROR, EIO);
		g_clear_error (&error);
		assert_output (GMIME_STREAM (output), expected, i);
		g_assert_cmpuint (output->closes, ==, 0);
		g_object_unref (output);
	}
	g_object_unref (entity);
	g_object_unref (input);

	for (i = 0; i < 2; i++) {
		GMimePart *part = g_mime_part_new ();
		TestOutput *source = g_object_new (test_output_get_type (), NULL);
		TestOutput *output = g_object_new (test_output_get_type (), NULL);
		GMimeContent *content;

		g_byte_array_append (g_mime_stream_mem_get_byte_array (GMIME_STREAM_MEM (source)), (const guint8 *) "private", 7);
		content = g_mime_content_new_with_stream (GMIME_STREAM (source), GMIME_CONTENT_ENCODING_UNSPECIFIED);
		g_mime_part_set_content (part, content);
		source->fail_read = i == 0;
		output->fail_flush_after = i == 1 ? 1 : G_MAXUINT;
		g_assert_false (g_mime_anonymizer_write_entity (anonymizer, NULL, GMIME_ENTITY (part), GMIME_STREAM (output), &error));
		g_assert_error (error, GMIME_ERROR, (i == 0 ? EIO : ENOSPC));
		g_clear_error (&error);
		g_assert_cmpuint (output->closes, ==, 0);
		g_object_unref (content);
		g_object_unref (source);
		g_object_unref (output);
		g_object_unref (part);
	}
	g_object_unref (anonymizer);
}

int
main (int argc, char **argv)
{
	g_mime_init ();
	testsuite_init (argc, argv);
	testsuite_start ("MIME anonymization");
	testsuite_check ("Case-insensitive preservation and independent snapshots");
	test_configuration ();
	testsuite_check_passed ();
	testsuite_check ("Received, addresses, encoded words and MIME parameters");
	test_masking ();
	testsuite_check_passed ();
	testsuite_check ("Binary values, duplicate headers, hidden fields and newlines");
	test_writing ();
	testsuite_check_passed ();
	testsuite_check ("Invalid raw fields, EOF values and invalid-field write/flush errors");
	test_loaded_headers ();
	testsuite_check_passed ();
	testsuite_check ("Shared international formatting and formatting failures");
	test_formatting ();
	testsuite_check_passed ();
	testsuite_check ("Short writes, write/flush errors and stream ownership");
	test_failures ();
	testsuite_check_passed ();
	testsuite_check ("Message/entity traversal, status fields and raw multipart delimiters");
	test_entities ();
	testsuite_check_passed ();
	testsuite_check ("Constructed MIME-Version insertion, binary content and stream positioning");
	test_constructed_message ();
	testsuite_check_passed ();
	testsuite_check ("Legacy constructed multipart text and closing-boundary policy");
	test_constructed_multipart ();
	testsuite_check_passed ();
	testsuite_check ("Nested multipart and embedded messages use an explicit traversal stack");
	test_deep_entities ();
	testsuite_check_passed ();
	testsuite_check ("Every output failure offset, content read failures and body flush errors");
	test_body_failures ();
	testsuite_check_passed ();
	testsuite_end ();
	g_mime_shutdown ();
	return testsuite_exit ();
}
