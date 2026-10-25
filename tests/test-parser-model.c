/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*  GMime
 *  Copyright (C) 2026 Jeffrey Stedfast
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public License
 *  as published by the Free Software Foundation; either version 2.1
 *  of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library; if not, write to the Free
 *  Software Foundation, 51 Franklin Street, Fifth Floor, Boston, MA
 *  02110-1301, USA.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <errno.h>
#include <string.h>
#include <gmime/gmime.h>
#include <gmime/gmime-parser-reader-private.h>
#include <gmime/gmime-reader-private.h>

#include "testsuite.h"

typedef struct {
	TestStreamOneByte parent_object;
	gsize chunk, bytes_read, fail_after;
	gboolean nonseekable, fail_seek, fail_substream, unset_errno;
	gint64 origin;
	guint reads, seeks, views, resets, fail_reset_after;
} ModelInput;

typedef TestStreamOneByteClass ModelInputClass;

G_DEFINE_TYPE (ModelInput, model_input, TEST_TYPE_STREAM_ONEBYTE)

static ssize_t
model_input_read (GMimeStream *stream, char *buffer, size_t length)
{
	ModelInput *input = (ModelInput *) stream;
	ssize_t nread;

	input->reads++;
	if (input->bytes_read == input->fail_after) {
		errno = input->unset_errno ? 0 : EIO;
		return -1;
	}
	length = MIN (length, MIN (input->chunk, input->fail_after - input->bytes_read));
	nread = g_mime_stream_read (input->parent_object.source, buffer, length);
	if (nread > 0)
		input->bytes_read += nread;

	return nread;
}

static gint64
model_input_tell (GMimeStream *stream)
{
	ModelInput *input = (ModelInput *) stream;

	if (input->nonseekable) {
		errno = ESPIPE;
		return -1;
	}

	return input->origin + g_mime_stream_tell (input->parent_object.source);
}

static int
model_input_reset (GMimeStream *stream)
{
	ModelInput *input = (ModelInput *) stream;

	if (input->resets++ == input->fail_reset_after) {
		errno = input->unset_errno ? 0 : EIO;
		return -1;
	}
	return GMIME_STREAM_CLASS (model_input_parent_class)->reset (stream);
}

static gint64
model_input_seek (GMimeStream *stream, gint64 offset, GMimeSeekWhence whence)
{
	ModelInput *input = (ModelInput *) stream;
	gint64 position;

	input->seeks++;
	if (input->fail_seek) {
		errno = input->unset_errno ? 0 : EIO;
		return -1;
	}
	if (whence == GMIME_STREAM_SEEK_SET)
		offset -= input->origin;
	position = g_mime_stream_seek (input->parent_object.source, offset, whence);

	return position < 0 ? position : position + input->origin;
}

static GMimeStream *
model_input_substream (GMimeStream *stream, gint64 begin, gint64 end)
{
	ModelInput *input = (ModelInput *) stream;

	input->views++;
	if (input->fail_substream) {
		errno = input->unset_errno ? 0 : EIO;
		return NULL;
	}

	return GMIME_STREAM_GET_CLASS (input->parent_object.source)->substream (
		input->parent_object.source, begin - input->origin, end < 0 ? end : end - input->origin);
}

static void
model_input_class_init (ModelInputClass *klass)
{
	GMimeStreamClass *stream_class = GMIME_STREAM_CLASS (klass);

	stream_class->read = model_input_read;
	stream_class->reset = model_input_reset;
	stream_class->tell = model_input_tell;
	stream_class->seek = model_input_seek;
	stream_class->substream = model_input_substream;
}

static void
model_input_init (ModelInput *input)
{
	input->chunk = G_MAXSIZE;
	input->fail_after = G_MAXSIZE;
	input->fail_reset_after = G_MAXUINT;
}

static ModelInput *
new_input (const char *data, gsize length, gsize chunk)
{
	ModelInput *input = g_object_new (model_input_get_type (), NULL);

	input->parent_object.source = g_mime_stream_mem_new_with_buffer (data, length);
	g_mime_stream_construct ((GMimeStream *) input, 0, -1);
	input->chunk = chunk;
	return input;
}

static GByteArray *
serialize_headers (GMimeHeaderList *headers, gboolean international)
{
	GMimeFormatOptions *options = g_mime_format_options_new ();
	GMimeStream *stream = g_mime_stream_block_new ();
	GByteArray *bytes;
	int i;

	g_mime_format_options_set_allow_international (options, international);
	if (international)
		g_mime_format_options_set_newline_format (options, GMIME_NEWLINE_FORMAT_DOS);
	for (i = 0; i < g_mime_header_list_get_count (headers); i++)
		g_assert_cmpint (g_mime_header_write_to_stream (g_mime_header_list_get_header_at (headers, i),
							      options, stream), >=, 0);
	bytes = g_mime_stream_block_to_byte_array (GMIME_STREAM_BLOCK (stream), NULL);
	g_mime_format_options_free (options);
	g_object_unref (stream);

	return bytes;
}

typedef struct {
	GMimeStream parent_object;
	gsize written, limit, chunk;
	gsize short_start, short_end;
	gboolean zero_write;
} ModelOutput;

typedef GMimeStreamClass ModelOutputClass;

G_DEFINE_TYPE (ModelOutput, model_output, GMIME_TYPE_STREAM)

static ssize_t
model_output_write (GMimeStream *stream, const char *buffer, size_t length)
{
	ModelOutput *output = (ModelOutput *) stream;

	if (output->written >= output->short_start && output->written < output->short_end)
		length = MIN (length, output->chunk);
	if (length > output->limit - output->written) {
		if (output->zero_write)
			return 0;
		errno = EIO;
		return -1;
	}
	output->written += length;
	return length;
}

static void
model_output_class_init (ModelOutputClass *klass)
{
	klass->write = model_output_write;
}

static void
model_output_init (ModelOutput *output)
{
	output->limit = G_MAXSIZE;
	output->chunk = G_MAXSIZE;
	output->short_end = G_MAXSIZE;
}

static void
test_header_models (void)
{
	static const char wire[] = "X \t: a\0b\r\nX: second\r\nbroken line\r\n\tfold\r\n"
		"Content-Type\0not-a-field\r\n\r\nbody";
	static const char first[] = "X \t: a\0b\r\n";
	const gsize header_length = sizeof (wire) - 1 - 6;
	gsize chunks[] = { 1, 2, 7, 4096 };
	guint i;

	testsuite_check ("Reader-backed headers preserve raw bytes, invalid fields, clones and offsets");
	for (i = 0; i < G_N_ELEMENTS (chunks); i++) {
		ModelInput *input = new_input (wire, sizeof (wire) - 1, chunks[i]);
		GMimeReader *reader;
		GMimeHeaderList *headers, *copy;
		GMimeHeader *header, *clone;
		GError *error = NULL;
		GByteArray *bytes;
		int j;

		input->origin = ((gint64) 1 << 34);
		reader = _g_mime_parser_reader_new (NULL, (GMimeStream *) input, FALSE);
		headers = _g_mime_parser_reader_construct_headers (reader, FALSE, &error);
		g_assert_no_error (error);
		g_assert_nonnull (headers);
		g_assert_cmpint (g_mime_header_list_get_count (headers), ==, 4);
		g_assert_true (headers->has_body_separator);
		header = g_mime_header_list_get_header (headers, "X");
		g_assert_true (header == g_mime_header_list_get_header_at (headers, 0));
		g_assert_cmpint (g_mime_header_get_offset (header), ==, input->origin);
		g_assert_cmpstr (g_mime_header_get_raw_name (header), ==, "X \t");
		g_assert_null (g_mime_header_list_get_header (headers, "Content-Type"));
		bytes = serialize_headers (headers, FALSE);
		g_assert_cmpmem (bytes->data, bytes->len, wire, header_length);
		g_byte_array_unref (bytes);
		bytes = serialize_headers (headers, TRUE);
		g_assert_cmpmem (bytes->data, bytes->len, wire, header_length);
		g_byte_array_unref (bytes);

		copy = g_mime_header_list_new (NULL);
		for (j = 0; j < g_mime_header_list_get_count (headers); j++) {
			clone = g_mime_header_clone (g_mime_header_list_get_header_at (headers, j));
			g_assert_cmpint (g_mime_header_get_offset (clone), ==, -1);
			g_mime_header_list_add (copy, clone);
			g_object_unref (clone);
		}
		g_mime_header_set_raw_value (header, " changed\r\n");
		g_assert_cmpstr (g_mime_header_get_raw_value (header), ==, " changed\r\n");
		bytes = serialize_headers (headers, TRUE);
		g_assert_cmpuint (bytes->len, ==, strlen ("X \t: changed\r\n") + header_length - (sizeof (first) - 1));
		g_assert_cmpmem (bytes->data, strlen ("X \t: changed\r\n"), "X \t: changed\r\n", strlen ("X \t: changed\r\n"));
		g_assert_cmpmem (bytes->data + strlen ("X \t: changed\r\n"), bytes->len - strlen ("X \t: changed\r\n"),
				 wire + sizeof (first) - 1, header_length - (sizeof (first) - 1));
		g_byte_array_unref (bytes);
		g_mime_header_set_value (header, NULL, "decoded", NULL);
		g_assert_cmpstr (g_mime_header_get_value (header), ==, "decoded");
		bytes = serialize_headers (headers, FALSE);
		g_assert_cmpuint (bytes->len, ==, strlen ("X \t: decoded\n") + header_length - (sizeof (first) - 1));
		g_assert_cmpmem (bytes->data, strlen ("X \t: decoded\n"), "X \t: decoded\n", strlen ("X \t: decoded\n"));
		g_assert_cmpmem (bytes->data + strlen ("X \t: decoded\n"), bytes->len - strlen ("X \t: decoded\n"),
				 wire + sizeof (first) - 1, header_length - (sizeof (first) - 1));
		g_byte_array_unref (bytes);
		g_object_unref (headers);
		g_object_unref (reader);
		g_object_unref (input);

		bytes = serialize_headers (copy, FALSE);
		g_assert_cmpmem (bytes->data, bytes->len, wire, header_length);
		g_byte_array_unref (bytes);
		g_object_unref (copy);
	}
	testsuite_check_passed ();
}

static GByteArray *
read_content (GMimeContent *content, gboolean decode)
{
	GMimeStream *stream = g_mime_stream_block_new ();
	GByteArray *bytes;

	if (decode)
		g_assert_cmpint (g_mime_content_decode_to_stream (content, stream), >=, 0);
	else
		g_assert_cmpint (g_mime_content_write_to_stream (content, stream), >=, 0);
	bytes = g_mime_stream_block_to_byte_array (GMIME_STREAM_BLOCK (stream), NULL);
	g_object_unref (stream);

	return bytes;
}

static void
test_leaf_models (void)
{
	static const struct {
		const char *headers, *raw, *decoded, *type;
		GMimeContentEncoding encoding;
		int newline;
	} cases[] = {
		{ "X: a\n\n", "abc", "abc", "GMimeTextPart", GMIME_CONTENT_ENCODING_UNSPECIFIED, -1 },
		{ "\n", "", "", "GMimeTextPart", GMIME_CONTENT_ENCODING_UNSPECIFIED, -1 },
		{ "Content-Type: text/plain\n", "", "", "GMimeTextPart", GMIME_CONTENT_ENCODING_UNSPECIFIED, -1 },
		{ "Content-Type: text/plain\n\n", "", "", "GMimeTextPart", GMIME_CONTENT_ENCODING_UNSPECIFIED, -1 },
		{ "Content-Type: text/plain\n\n", "a\nb\n", "a\nb\n", "GMimeTextPart",
		  GMIME_CONTENT_ENCODING_UNSPECIFIED, GMIME_NEWLINE_FORMAT_UNIX },
		{ "Content-Type: text/plain\n\n", "a\r\nb\r\n", "a\r\nb\r\n", "GMimeTextPart",
		  GMIME_CONTENT_ENCODING_UNSPECIFIED, GMIME_NEWLINE_FORMAT_DOS },
		{ "Content-Type: text/plain\n\n", "a\nb\r\n", "a\nb\r\n", "GMimeTextPart",
		  GMIME_CONTENT_ENCODING_UNSPECIFIED, GMIME_NEWLINE_FORMAT_MIXED },
		{ "Content-Type: application/rtf\n\n", "{\\rtf1 body}\r\n", "{\\rtf1 body}\r\n", "GMimeTextPart",
		  GMIME_CONTENT_ENCODING_UNSPECIFIED, GMIME_NEWLINE_FORMAT_DOS },
		{ "Content-Type: message/rfc822\nContent-Transfer-Encoding: base64\n\n", "YWJj\n", "abc", "GMimePart",
		  GMIME_CONTENT_ENCODING_BASE64, GMIME_NEWLINE_FORMAT_UNIX },
		{ "Content-Type: message/partial\nContent-Transfer-Encoding: base64\n\n", "YWJj\n", "abc", "GMimePart",
		  GMIME_CONTENT_ENCODING_BASE64, GMIME_NEWLINE_FORMAT_UNIX },
		{ "Content-Type: message/partial; id=x; number=1\n\n", "fragment", "fragment", "GMimeMessagePartial",
		  GMIME_CONTENT_ENCODING_UNSPECIFIED, -1 },
		{ "Content-Type: text/rfc822-headers\nContent-Transfer-Encoding: base64\n\n", "YWJj", "abc", "GMimeTextPart",
		  GMIME_CONTENT_ENCODING_BASE64, -1 },
		{ "Content-Type: message/global-headers\nContent-Transfer-Encoding: quoted-printable\n\n", "a=3Db", "a=b",
		  "GMimePart", GMIME_CONTENT_ENCODING_QUOTEDPRINTABLE, -1 },
		{ "Content-Type: message/delivery-status\n\n", "X: value\r\n", "X: value\r\n", "GMimeMessageDeliveryStatus",
		  GMIME_CONTENT_ENCODING_UNSPECIFIED, GMIME_NEWLINE_FORMAT_DOS },
		{ "Content-Type: application/octet-stream\nContent-Transfer-Encoding: base64\n"
		  "Content-Transfer-Encoding: quoted-printable\n\n", "YWJj", "abc", "GMimePart",
		  GMIME_CONTENT_ENCODING_BASE64, -1 },
	};
	guint i, mode;

	testsuite_check ("Reader-backed leaves retain encoded bodies, specialized types and newline hints");
	for (i = 0; i < G_N_ELEMENTS (cases); i++) {
		char *wire = g_strconcat (cases[i].headers, cases[i].raw, NULL);

		for (mode = 0; mode < 8; mode++) {
			ModelInput *input = new_input (wire, strlen (wire), mode & 1 ? 1 : 4096);
			GMimeReader *reader;
			GMimeEntity *entity;
			GMimeContent *content;
			GMimeNewLineFormat newline;
			GByteArray *bytes;
			GError *error = NULL;

			input->nonseekable = (mode & 2) != 0;
			reader = _g_mime_parser_reader_new (NULL, (GMimeStream *) input, (mode & 4) != 0);
			entity = _g_mime_parser_reader_construct_part (reader, &error);
			g_assert_no_error (error);
			g_assert_nonnull (entity);
			g_assert_cmpstr (G_OBJECT_TYPE_NAME (entity), ==, cases[i].type);
			g_assert_false (entity->ensure_newline);
			g_assert_cmpint (g_mime_part_get_content_encoding (GMIME_PART (entity)), ==, cases[i].encoding);
			content = g_mime_part_get_content (GMIME_PART (entity));
			g_assert_cmpint (input->views, ==, *cases[i].raw && (mode & 4) && !(mode & 2) ? 1 : 0);
			g_object_unref (reader);
			g_object_unref (input);

			if (!*cases[i].raw) {
				g_assert_null (content);
			} else {
				g_assert_nonnull (content);
				g_assert_cmpint (g_mime_content_get_encoding (content), ==, cases[i].encoding);
				g_assert_cmpint (g_mime_content_get_newline_format (content, &newline), ==, cases[i].newline >= 0);
				if (cases[i].newline >= 0)
					g_assert_cmpint (newline, ==, cases[i].newline);
				bytes = read_content (content, FALSE);
				g_assert_cmpmem (bytes->data, bytes->len, cases[i].raw, strlen (cases[i].raw));
				g_byte_array_unref (bytes);
				bytes = read_content (content, TRUE);
				g_assert_cmpmem (bytes->data, bytes->len, cases[i].decoded, strlen (cases[i].decoded));
				g_byte_array_unref (bytes);
			}
			g_object_unref (entity);
		}
		g_free (wire);
	}
	testsuite_check_passed ();
}

static void
test_binary_encoding_headers (void)
{
	static const struct {
		const char *value;
		gsize length;
		GMimeContentEncoding encoding;
	} cases[] = {
		{ " base64\0x\n", sizeof (" base64\0x\n") - 1, GMIME_CONTENT_ENCODING_UNSPECIFIED },
		{ "\0base64\n", sizeof ("\0base64\n") - 1, GMIME_CONTENT_ENCODING_UNSPECIFIED },
		{ " base64 \0x\n", sizeof (" base64 \0x\n") - 1, GMIME_CONTENT_ENCODING_BASE64 },
		{ " base64;\0x\n", sizeof (" base64;\0x\n") - 1, GMIME_CONTENT_ENCODING_BASE64 },
		{ " =?us-ascii?q?base64?=\0x\n", sizeof (" =?us-ascii?q?base64?=\0x\n") - 1,
		  GMIME_CONTENT_ENCODING_UNSPECIFIED },
		{ " =?us-ascii?q?base64?= \0x\n", sizeof (" =?us-ascii?q?base64?= \0x\n") - 1,
		  GMIME_CONTENT_ENCODING_BASE64 },
	};
	guint i, mode;

	testsuite_check ("Binary encoding headers do not turn a malformed token into a valid C-string prefix");
	for (i = 0; i < G_N_ELEMENTS (cases); i++) {
		GString *wire = g_string_new ("Content-Type: message/partial; id=x; number=1\nContent-Transfer-Encoding:");

		g_string_append_len (wire, cases[i].value, cases[i].length);
		g_string_append (wire, "\nYWJj\n");
		for (mode = 0; mode < 4; mode++) {
			ModelInput *input = new_input (wire->str, wire->len, mode & 1 ? 1 : 4096);
			GMimeReader *reader = _g_mime_parser_reader_new (NULL, (GMimeStream *) input, (mode & 2) != 0);
			GError *error = NULL;
			GMimeEntity *entity = _g_mime_parser_reader_construct_part (reader, &error);
			GMimeHeader *header;
			GByteArray *bytes;
			const char *decoded = cases[i].encoding == GMIME_CONTENT_ENCODING_BASE64 ? "abc" : "YWJj\n";

			g_assert_no_error (error);
			g_assert_nonnull (entity);
			g_assert_cmpstr (G_OBJECT_TYPE_NAME (entity), ==,
					cases[i].encoding == GMIME_CONTENT_ENCODING_BASE64 ? "GMimePart" : "GMimeMessagePartial");
			g_assert_cmpint (g_mime_part_get_content_encoding (GMIME_PART (entity)), ==, cases[i].encoding);
			bytes = read_content (g_mime_part_get_content (GMIME_PART (entity)), TRUE);
			g_assert_cmpmem (bytes->data, bytes->len, decoded, strlen (decoded));
			g_byte_array_unref (bytes);
			bytes = serialize_headers (entity->headers, FALSE);
			g_assert_cmpmem (bytes->data, bytes->len, wire->str, wire->len - 6);
			g_byte_array_unref (bytes);

			header = g_mime_header_list_get_header (entity->headers, "Content-Transfer-Encoding");
			g_mime_header_set_raw_value (header, " base64\n");
			g_assert_cmpint (g_mime_part_get_content_encoding (GMIME_PART (entity)), ==, GMIME_CONTENT_ENCODING_BASE64);
			g_mime_header_set_value (header, NULL, "quoted-printable", NULL);
			g_assert_cmpint (g_mime_part_get_content_encoding (GMIME_PART (entity)), ==,
					GMIME_CONTENT_ENCODING_QUOTEDPRINTABLE);
			g_object_unref (entity);
			g_object_unref (reader);
			g_object_unref (input);
		}
		g_string_free (wire, TRUE);
	}
	testsuite_check_passed ();
}

static void
test_model_metadata (void)
{
	static const char wire[] = "Content-Type: text/plain; charset=us-ascii\n"
		"Content-Type: application/octet-stream\nContent-Disposition: inline\n"
		"Content-Disposition: attachment\nContent-Id: <first@example.com>\n"
		"Content-Id: <second@example.com>\nX: a\0b\nbad header\n\nbody\0tail\n";
	ModelInput *input = new_input (wire, sizeof (wire) - 1, 1);
	GMimeParserOptions *options = g_mime_parser_options_new ();
	GMimeReader *reader = _g_mime_parser_reader_new (options, (GMimeStream *) input, FALSE);
	GMimeEntity *entity;
	GMimeHeaderList *headers;
	GMimeContent *content;
	GByteArray *bytes;
	GError *error = NULL;
	GMimeNewLineFormat newline;

	testsuite_check ("Reader-backed model caches use first headers and preserve binary content");
	g_mime_parser_options_set_max_header_length (options, 1);
	g_mime_parser_options_free (options);
	entity = _g_mime_parser_reader_construct_part (reader, &error);
	g_assert_no_error (error);
	g_assert_true (GMIME_IS_TEXT_PART (entity));
	g_assert_true (g_mime_content_type_is_type (entity->content_type, "text", "plain"));
	g_assert_cmpstr (g_mime_content_type_get_parameter (entity->content_type, "charset"), ==, "us-ascii");
	g_assert_cmpstr (g_mime_entity_get_disposition (entity), ==, "inline");
	g_assert_cmpstr (g_mime_entity_get_content_id (entity), ==, "first@example.com");
	headers = g_mime_entity_get_header_list (entity);
	bytes = serialize_headers (headers, FALSE);
	g_assert_cmpmem (bytes->data, bytes->len, wire, sizeof (wire) - 1 - sizeof ("\nbody\0tail\n") + 1);
	g_byte_array_unref (bytes);
	content = g_mime_part_get_content (GMIME_PART (entity));
	bytes = read_content (content, FALSE);
	g_assert_cmpmem (bytes->data, bytes->len, "body\0tail\n", 10);
	g_byte_array_unref (bytes);
	g_assert_true (g_mime_content_get_newline_format (content, &newline));
	g_assert_cmpint (newline, ==, GMIME_NEWLINE_FORMAT_UNIX);
	g_mime_content_clear_newline_format (content);
	g_assert_false (g_mime_content_get_newline_format (content, NULL));
	g_mime_content_set_newline_format (content, GMIME_NEWLINE_FORMAT_DOS);
	g_assert_true (g_mime_content_get_newline_format (content, &newline));
	g_assert_cmpint (newline, ==, GMIME_NEWLINE_FORMAT_DOS);
	g_mime_content_set_stream (content, input->parent_object.source);
	g_assert_false (g_mime_content_get_newline_format (content, NULL));
	g_object_unref (entity);
	g_object_unref (reader);
	g_object_unref (input);
	testsuite_check_passed ();
}

static void
test_persistent_resume (void)
{
	GString *wire = g_string_new ("X: outer\n\nContent-Type: text/plain\n\n");
	ModelInput *input;
	GMimeReader *reader;
	GMimeHeaderList *headers;
	GMimeEntity *entity;
	GByteArray *bytes;
	GError *error = NULL;
	gsize body_offset = wire->len;
	guint i;

	testsuite_check ("Persistent model reads restore physical read-ahead without replaying buffered bytes");
	for (i = 0; i < 14000; i++)
		g_string_append_c (wire, 'a' + i % 26);
	input = new_input (wire->str, wire->len, 83);
	reader = _g_mime_parser_reader_new (NULL, (GMimeStream *) input, TRUE);
	headers = _g_mime_parser_reader_construct_headers (reader, FALSE, &error);
	g_assert_no_error (error);
	g_assert_cmpint (g_mime_reader_tell (reader), ==, 10);
	g_assert_cmpint (g_mime_stream_tell ((GMimeStream *) input), >, 10);
	g_assert_cmpint (g_mime_stream_seek ((GMimeStream *) input, 0, GMIME_STREAM_SEEK_SET), ==, 0);
	entity = _g_mime_parser_reader_construct_part (reader, &error);
	g_assert_no_error (error);
	g_assert_nonnull (entity);
	g_assert_cmpint (input->seeks, ==, 2);
	g_assert_cmpint (g_mime_reader_tell (reader), ==, wire->len);
	g_object_unref (reader);
	g_object_unref (input);
	bytes = read_content (g_mime_part_get_content (GMIME_PART (entity)), FALSE);
	g_assert_cmpmem (bytes->data, bytes->len, wire->str + body_offset, wire->len - body_offset);
	g_byte_array_unref (bytes);
	g_object_unref (entity);
	g_object_unref (headers);
	g_string_free (wire, TRUE);
	testsuite_check_passed ();
}

static void
test_model_stream_replacement (void)
{
	static const char wire[] = "Content-Type: text/plain\n\nreplacement\n";
	ModelInput *first = new_input ("X: initial\n\nbody", 16, 1);
	GMimeReader *reader = _g_mime_parser_reader_new (NULL, (GMimeStream *) first, TRUE);
	GMimeHeaderList *headers;
	GError *error = NULL;
	guint i;

	testsuite_check ("Model stream replacement references the new source and rechecks persistence");
	headers = _g_mime_parser_reader_construct_headers (reader, FALSE, &error);
	g_assert_no_error (error);
	g_object_unref (headers);
	g_object_unref (first);
	for (i = 0; i < 3; i++) {
		ModelInput *input = new_input (wire, sizeof (wire) - 1, 3);
		GMimeEntity *entity;
		GByteArray *bytes;

		input->nonseekable = i == 1;
		g_mime_reader_set_stream (reader, (GMimeStream *) input);
		entity = _g_mime_parser_reader_construct_part (reader, &error);
		g_assert_no_error (error);
		g_assert_nonnull (entity);
		g_assert_cmpuint (input->views, ==, i == 1 ? 0 : 1);
		bytes = read_content (g_mime_part_get_content (GMIME_PART (entity)), FALSE);
		g_assert_cmpmem (bytes->data, bytes->len, "replacement\n", 12);
		g_byte_array_unref (bytes);
		g_object_unref (entity);
		g_object_unref (input);
	}
	g_object_unref (reader);
	testsuite_check_passed ();
}

static void
test_model_type_registration (void)
{
	static const char wire[] = "Content-Type: application/x-reader-leaf\n\nbody";
	static const char container[] = "Content-Type: application/x-reader-container\n\nbody";
	ModelInput *input = new_input (wire, sizeof (wire) - 1, 3);
	GMimeReader *reader = _g_mime_parser_reader_new (NULL, (GMimeStream *) input, FALSE);
	GMimeHeaderList *headers;
	GMimeEntity *entity;
	GError *error = NULL;

	testsuite_check ("Registered leaf types are retained and unsupported custom containers are not coerced");
	g_mime_entity_register_type ("application", "x-reader-leaf", GMIME_TYPE_TEXT_PART);
	entity = _g_mime_parser_reader_construct_part (reader, &error);
	g_assert_no_error (error);
	g_assert_true (GMIME_IS_TEXT_PART (entity));
	g_assert_true (g_mime_content_type_is_type (entity->content_type, "application", "x-reader-leaf"));
	g_object_unref (entity);
	g_object_unref (input);

	g_mime_entity_register_type ("application", "x-reader-container", GMIME_TYPE_MESSAGE_PART);
	input = new_input (container, sizeof (container) - 1, 3);
	g_mime_reader_set_stream (reader, (GMimeStream *) input);
	g_assert_null (_g_mime_parser_reader_construct_part (reader, &error));
	g_assert_error (error, GMIME_ERROR, GMIME_ERROR_NOT_SUPPORTED);
	g_clear_error (&error);

	g_assert_cmpint (g_mime_stream_seek ((GMimeStream *) input, 0, GMIME_STREAM_SEEK_SET), ==, 0);
	g_mime_reader_set_stream (reader, (GMimeStream *) input);
	headers = _g_mime_parser_reader_construct_headers (reader, FALSE, &error);
	g_assert_no_error (error);
	g_assert_nonnull (headers);
	g_assert_cmpstr (g_mime_header_get_value (g_mime_header_list_get_header (headers, "Content-Type")), ==,
			 "application/x-reader-container");
	g_object_unref (headers);
	g_object_unref (reader);
	g_object_unref (input);
	testsuite_check_passed ();
}

static void
test_message_models (void)
{
	static const char *types[] = {
		"message/rfc822", "message/rfc2822", "message/global", "message/news",
		"message/external-body", "text/rfc822-headers", "message/global-headers"
	};
	static const char inner[] = "Subject: first\nContent-Type: text/plain; charset=utf-8\n"
		"Subject: second\nTo: a@example.com\nTo: b@example.com\n"
		"Content-Transfer-Encoding: base64\nX: a\0b\n\nYWJj\n";
	guint i, mode;

	testsuite_check ("Message models split headers without losing offsets, duplicates or binary fields");
	for (i = 0; i <= G_N_ELEMENTS (types); i++) {
		GString *wire = g_string_new (NULL);
		gsize inner_offset;

		if (i < G_N_ELEMENTS (types))
			g_string_append_printf (wire, "X-Outer: retained\nContent-Type: %s\n\n", types[i]);
		inner_offset = wire->len;
		g_string_append_len (wire, inner, sizeof (inner) - 1);
		for (mode = 0; mode < 8; mode++) {
			ModelInput *input = new_input (wire->str, wire->len, mode & 1 ? 1 : 4096);
			GMimeReader *reader;
			GMimeEntity *entity, *body;
			GMimeMessage *message;
			GMimeHeaderList *headers;
			GError *error = NULL;
			GByteArray *bytes;

			input->nonseekable = (mode & 2) != 0;
			reader = _g_mime_parser_reader_new (NULL, (GMimeStream *) input, (mode & 4) != 0);
			if (i == G_N_ELEMENTS (types)) {
				message = _g_mime_parser_reader_construct_message (reader, &error);
				entity = (GMimeEntity *) message;
			} else {
				entity = _g_mime_parser_reader_construct_part (reader, &error);
				g_assert_no_error (error);
				g_assert_true (GMIME_IS_MESSAGE_PART (entity));
				message = g_mime_message_part_get_message (GMIME_MESSAGE_PART (entity));
				g_assert_cmpint (g_mime_header_list_get_count (entity->headers), ==, 2);
			}
			g_assert_no_error (error);
			g_assert_nonnull (message);
			g_assert_cmpstr (g_mime_message_get_subject (message), ==, "first");
			g_assert_cmpint (g_mime_internet_address_list_length (g_mime_message_get_to (message)), ==, 2);
			headers = ((GMimeEntity *) message)->headers;
			g_assert_cmpint (g_mime_header_list_get_count (headers), ==, 5);
			g_assert_false (g_mime_header_list_contains (headers, "MIME-Version"));
			g_assert_cmpint (g_mime_header_get_offset (g_mime_header_list_get_header (headers, "Subject")), ==, inner_offset);
			body = g_mime_message_get_mime_part (message);
			g_assert_true (GMIME_IS_TEXT_PART (body));
			g_assert_cmpint (g_mime_header_list_get_count (body->headers), ==, 2);
			g_assert_true (body->headers->has_body_separator);
			g_assert_cmpint (g_mime_header_get_offset (g_mime_header_list_get_header (body->headers, "Content-Type")), ==,
					inner_offset + strlen ("Subject: first\n"));
			g_assert_cmpint (g_mime_reader_tell (reader), ==, wire->len);
			g_object_unref (reader);
			g_object_unref (input);

			bytes = read_content (g_mime_part_get_content (GMIME_PART (body)), TRUE);
			g_assert_cmpmem (bytes->data, bytes->len, "abc", 3);
			g_byte_array_unref (bytes);
			g_mime_message_set_subject (message, "changed", NULL);
			g_assert_cmpstr (g_mime_message_get_subject (message), ==, "changed");
			g_object_unref (entity);
		}
		g_string_free (wire, TRUE);
	}
	testsuite_check_passed ();
}

static void
test_message_depth (void)
{
	GString *wire = g_string_new (NULL);
	guint i, limit;

	testsuite_check ("Message model stacks obey the reader's nesting limit and preserve opaque descendants");
	for (i = 0; i < 32; i++)
		g_string_append (wire, "Content-Type: message/rfc822\n\n");
	g_string_append (wire, "Subject: deepest\n\nbody");
	for (limit = 0; limit <= 32; limit++) {
		ModelInput *input = new_input (wire->str, wire->len, 3);
		GMimeParserOptions *options = g_mime_parser_options_new ();
		GMimeReader *reader;
		GMimeEntity *entity, *part;
		GError *error = NULL;

		g_mime_parser_options_set_max_mime_depth (options, limit);
		reader = _g_mime_parser_reader_new (options, (GMimeStream *) input, FALSE);
		g_mime_parser_options_free (options);
		entity = _g_mime_parser_reader_construct_part (reader, &error);
		g_assert_no_error (error);
		g_assert_nonnull (entity);
		part = entity;
		for (i = 0; i < limit; i++) {
			GMimeMessage *message;

			g_assert_true (GMIME_IS_MESSAGE_PART (part));
			message = g_mime_message_part_get_message (GMIME_MESSAGE_PART (part));
			g_assert_nonnull (message);
			part = g_mime_message_get_mime_part (message);
		}
		g_assert_true (GMIME_IS_PART (part));
		g_assert_false (GMIME_IS_MESSAGE_PART (part));
		g_object_unref (entity);
		g_object_unref (reader);
		g_object_unref (input);
	}
	g_string_free (wire, TRUE);
	testsuite_check_passed ();
}

static void
test_message_model_errors (void)
{
	static const char wire[] = "Subject: outer\nContent-Type: message/rfc822\n\n"
		"Subject: inner\nContent-Type: message/rfc822\n\nSubject: last\n\nbody\n";
	static const char recovered[] = "Subject: recovered\n\nreplacement\n";
	static const char multipart_wire[] = "Content-Type: multipart/mixed; boundary=b\n\npre\0amble\n"
		"--b\nContent-Type: multipart/digest; boundary=c\n\n--c\n\ninvalid\0marker\n"
		"Subject: last\n\nbody\n--c--\n--b--\nepi\0logue";
	guint mode, i;

	testsuite_check ("Message and multipart failures clean nested stacks and recover after stream replacement");
	for (mode = 0; mode < 32; mode++) {
		const char *input_wire = mode & 16 ? multipart_wire : wire;
		gsize length = mode & 16 ? sizeof (multipart_wire) - 1 : sizeof (wire) - 1;
		int code = mode & 1 ? GMIME_ERROR_GENERAL : EIO;

		for (i = 0; i <= length; i++) {
			ModelInput *input = new_input (input_wire, length, 1);
			ModelInput *replacement;
			GMimeReader *reader;
			GMimeMessage *message;
			GMimeEntity *entity;
			GError *error = NULL;
			GByteArray *bytes;
			guint reads;

			input->fail_after = i;
			input->unset_errno = (mode & 1) != 0;
			input->nonseekable = (mode & 8) != 0;
			reader = _g_mime_parser_reader_new (NULL, (GMimeStream *) input, (mode & 2) != 0);
			entity = mode & 4 ? (GMimeEntity *) _g_mime_parser_reader_construct_message (reader, &error) :
				_g_mime_parser_reader_construct_part (reader, &error);
			g_assert_null (entity);
			g_assert_error (error, GMIME_ERROR, code);
			g_clear_error (&error);
			reads = input->reads;
			g_assert_null (_g_mime_parser_reader_construct_message (reader, &error));
			g_assert_error (error, GMIME_ERROR, code);
			g_assert_cmpuint (input->reads, ==, reads);
			g_clear_error (&error);
			g_object_unref (input);

			replacement = new_input (recovered, sizeof (recovered) - 1, 3);
			replacement->nonseekable = (mode & 8) != 0;
			g_mime_reader_set_stream (reader, (GMimeStream *) replacement);
			message = _g_mime_parser_reader_construct_message (reader, &error);
			g_assert_no_error (error);
			g_assert_nonnull (message);
			g_assert_cmpstr (g_mime_message_get_subject (message), ==, "recovered");
			g_assert_cmpuint (replacement->views, ==, (mode & 2) && !(mode & 8) ? 1 : 0);
			g_object_unref (replacement);
			g_object_unref (reader);

			entity = g_mime_message_get_mime_part (message);
			bytes = read_content (g_mime_part_get_content (GMIME_PART (entity)), FALSE);
			g_assert_cmpmem (bytes->data, bytes->len, "replacement\n", 12);
			g_byte_array_unref (bytes);
			g_object_unref (message);
		}
		}
	testsuite_check_passed ();
}

static void
assert_raw_bytes (GBytes *bytes, const char *expected, gsize length)
{
	gsize size;
	const void *data;

	g_assert_nonnull (bytes);
	data = g_bytes_get_data (bytes, &size);
	g_assert_cmpmem (data, size, expected, length);
}

static void
test_mailbox_models (void)
{
	static const char first[] = ">From invalid\0preheader\r\nSubject: first\r\n"
		"Content-Type: multipart/mixed; boundary=b\r\n\r\n"
		"--b\r\nContent-Type: application/octet-stream\r\n"
		"Content-Transfer-Encoding: binary\r\n\r\none\0tail\r\n"
		"--b\r\nContent-Type: message/rfc822\r\n\r\nSubject: inner\r\n\r\ninner\r\n--b--\r\n";
	static const char second[] = "Subject: second\r\n"
		"Content-Type: application/octet-stream\r\nContent-Transfer-Encoding: binary\r\n\r\ntwo\0tail";
	static const char preheader[] = ">From invalid\0preheader\r\n";
	gsize chunks[] = { 1, 7, 4096 };
	guint mode, chunk, format;

	testsuite_check ("Mailbox models retain opening envelopes, sequential state and persistent content lifetimes");
	for (format = 0; format < 2; format++) {
		GString *wire = g_string_new ("\357\273\277ignored pretext\r\n");
		GString *marker = g_string_new (format ? "\001\001\001\001" : "From ");
		const char *next_marker = format ? "\001\001\001\001 next" : "From next";
		gsize begin = wire->len, first_headers, next_begin;
		guint i;

		/* Force marker callbacks across multiple reader buffers, including
		 * NUL bytes which must not truncate the retained envelope. */
		for (i = 0; i < 10000; i++)
			g_string_append_c (marker, i == 5000 ? '\0' : 'a' + i % 26);
		g_string_append_len (wire, marker->str, marker->len);
		g_string_append (wire, "\r\n");
		first_headers = wire->len + sizeof (preheader) - 1;
		g_string_append_len (wire, first, sizeof (first) - 1);
		if (format)
			g_string_append (wire, "\001\001\001\001 close\r\n");
		next_begin = wire->len;
		g_string_append (wire, next_marker);
		g_string_append (wire, "\r\n");
		g_string_append_len (wire, second, sizeof (second) - 1);
		if (format)
			g_string_append (wire, "\r\n\001\001\001\001 close\r\n");

		for (mode = 0; mode < 4; mode++) {
			for (chunk = 0; chunk < G_N_ELEMENTS (chunks); chunk++) {
				ModelInput *input = new_input (wire->str, wire->len, chunks[chunk]);
				GMimeReader *reader;
				GMimeMessage *message, *next;
				GMimeMultipart *multipart;
				GMimeContent *content;
				GBytes *saved_marker;
				GByteArray *bytes;
				GError *error = NULL;
				gint64 origin;

				input->origin = ((gint64) 1 << 34);
				input->nonseekable = (mode & 1) != 0;
				origin = input->nonseekable ? 0 : input->origin;
				reader = _g_mime_parser_reader_new (NULL, (GMimeStream *) input, (mode & 2) != 0);
				g_mime_reader_set_stream_with_format (reader, (GMimeStream *) input,
								     format ? GMIME_FORMAT_MMDF : GMIME_FORMAT_MBOX);
				g_assert_null (_g_mime_parser_reader_get_mbox_marker (reader));
				g_assert_cmpint (_g_mime_parser_reader_get_mbox_marker_offset (reader), ==, -1);
				message = _g_mime_parser_reader_construct_message (reader, &error);
				g_assert_no_error (error);
				g_assert_nonnull (message);
				g_assert_false (g_mime_reader_eos (reader));
				g_assert_cmpstr (g_mime_message_get_subject (message), ==, "first");
				g_assert_cmpint (g_mime_header_get_offset (g_mime_header_list_get_header (
					((GMimeEntity *) message)->headers, "Subject")), ==, origin + first_headers);
				g_assert_cmpint (_g_mime_parser_reader_get_mbox_marker_offset (reader), ==, origin + begin);
				saved_marker = g_bytes_ref (_g_mime_parser_reader_get_mbox_marker (reader));
				assert_raw_bytes (saved_marker, marker->str, marker->len);
				assert_raw_bytes (message->raw_marker, preheader, sizeof (preheader) - 1);
				multipart = GMIME_MULTIPART (message->mime_part);
				g_assert_cmpint (g_mime_multipart_get_count (multipart), ==, 2);
				content = g_mime_part_get_content (GMIME_PART (g_mime_multipart_get_part (multipart, 0)));
				bytes = read_content (content, FALSE);
				g_assert_cmpmem (bytes->data, bytes->len, "one\0tail", 8);
				g_byte_array_unref (bytes);

				/* Reading persistent content may move the source behind
				 * buffered mailbox data. The next parse restores it. */
				next = _g_mime_parser_reader_construct_message (reader, &error);
				g_assert_no_error (error);
				g_assert_nonnull (next);
				g_assert_cmpstr (g_mime_message_get_subject (next), ==, "second");
				g_assert_cmpint (_g_mime_parser_reader_get_mbox_marker_offset (reader), ==, origin + next_begin);
				assert_raw_bytes (_g_mime_parser_reader_get_mbox_marker (reader), next_marker, strlen (next_marker));
				g_assert_null (next->raw_marker);
				g_assert_true (g_mime_reader_eos (reader));
				g_assert_null (_g_mime_parser_reader_construct_message (reader, &error));
				g_assert_no_error (error);
				g_assert_null (_g_mime_parser_reader_get_mbox_marker (reader));
				g_assert_cmpint (_g_mime_parser_reader_get_mbox_marker_offset (reader), ==, -1);
				g_object_unref (reader);
				g_object_unref (input);
				assert_raw_bytes (saved_marker, marker->str, marker->len);
				g_bytes_unref (saved_marker);
				bytes = read_content (g_mime_part_get_content (GMIME_PART (next->mime_part)), FALSE);
				g_assert_cmpmem (bytes->data, bytes->len, "two\0tail", 8);
				g_byte_array_unref (bytes);
				bytes = read_content (content, FALSE);
				g_assert_cmpmem (bytes->data, bytes->len, "one\0tail", 8);
				g_byte_array_unref (bytes);
				g_object_unref (message);
				g_object_unref (next);
			}
		}
		g_string_free (marker, TRUE);
		g_string_free (wire, TRUE);
	}
	testsuite_check_passed ();

	testsuite_check ("Mailbox boundaries immediately after message/rfc822 headers leave embedded messages absent");
	{
		static const char *wires[] = {
			"From first\nContent-Type: message/rfc822\n\nFrom second\nSubject: second\n\nlast",
			"\001\001\001\001\nContent-Type: message/rfc822\n\n\001\001\001\001\n"
			"\001\001\001\001\nSubject: second\n\nlast\n\001\001\001\001\n"
		};

		for (format = 0; format < G_N_ELEMENTS (wires); format++) {
			for (chunk = 0; chunk < G_N_ELEMENTS (chunks); chunk++) {
				ModelInput *input = new_input (wires[format], strlen (wires[format]), chunks[chunk]);
				GMimeReader *reader = _g_mime_parser_reader_new (NULL, (GMimeStream *) input, TRUE);
				GError *error = NULL;
				GMimeMessage *message;

				g_mime_reader_set_stream_with_format (reader, (GMimeStream *) input,
								     format ? GMIME_FORMAT_MMDF : GMIME_FORMAT_MBOX);
				message = _g_mime_parser_reader_construct_message (reader, &error);
				g_assert_no_error (error);
				g_assert_nonnull (message);
				g_assert_true (GMIME_IS_MESSAGE_PART (message->mime_part));
				g_assert_null (g_mime_message_part_get_message (GMIME_MESSAGE_PART (message->mime_part)));
				g_object_unref (message);
				message = _g_mime_parser_reader_construct_message (reader, &error);
				g_assert_no_error (error);
				g_assert_nonnull (message);
				g_assert_cmpstr (g_mime_message_get_subject (message), ==, "second");
				g_assert_true (g_mime_reader_eos (reader));
				g_object_unref (message);
				g_object_unref (reader);
				g_object_unref (input);
			}
		}
	}
	testsuite_check_passed ();

	testsuite_check ("Embedded-message mailbox lookahead does not read an entire long envelope");
	{
		static const char prefix[] = "From first\nContent-Type: message/rfc822\n\n";
		GString *wire = g_string_new (prefix);
		ModelInput *input;
		GMimeReader *reader;
		GMimeMessage *message;
		GError *error = NULL;

		g_string_append (wire, "From ");
		for (mode = 0; mode < 10000; mode++)
			g_string_append_c (wire, 'x');
		g_string_append (wire, "\nSubject: second\n\nbody\n");
		input = new_input (wire->str, wire->len, 1);
		input->fail_after = sizeof (prefix) - 1 + 128;
		reader = _g_mime_parser_reader_new (NULL, (GMimeStream *) input, FALSE);
		g_mime_reader_set_stream_with_format (reader, (GMimeStream *) input, GMIME_FORMAT_MBOX);
		message = _g_mime_parser_reader_construct_message (reader, &error);
		g_assert_no_error (error);
		g_assert_nonnull (message);
		g_assert_true (GMIME_IS_MESSAGE_PART (message->mime_part));
		g_assert_null (g_mime_message_part_get_message (GMIME_MESSAGE_PART (message->mime_part)));
		g_assert_cmpint (g_mime_reader_tell (reader), ==, sizeof (prefix) - 1);
		g_assert_cmpuint (input->bytes_read, <=, input->fail_after);
		g_object_unref (message);
		g_object_unref (reader);
		g_object_unref (input);
		g_string_free (wire, TRUE);
	}
	testsuite_check_passed ();

	testsuite_check ("Mbox Content-Length suppresses premature markers without changing retained content");
	for (mode = 0; mode < 8; mode++) {
		static const char body[] = "before\nFrom false\n\ninside\n";
		GString *wire = g_string_new ("From first\nSubject: first\n");

		g_string_append_printf (wire, "Content-Length: %u\n\n%sFrom real\nSubject: second\n\nlast",
				       (guint) strlen (body), body);
		for (chunk = 0; chunk < G_N_ELEMENTS (chunks); chunk++) {
			ModelInput *input = new_input (wire->str, wire->len, chunks[chunk]);
			GMimeParserOptions *options = g_mime_parser_options_new ();
			GMimeReader *reader;
			GError *error = NULL;
			guint count = 0;

			input->nonseekable = (mode & 1) != 0;
			g_mime_parser_options_set_respect_content_length (options, (mode & 4) != 0);
			reader = _g_mime_parser_reader_new (options, (GMimeStream *) input, (mode & 2) != 0);
			g_mime_parser_options_free (options);
			g_mime_reader_set_stream_with_format (reader, (GMimeStream *) input, GMIME_FORMAT_MBOX);
			while (!g_mime_reader_eos (reader)) {
				GMimeMessage *message = _g_mime_parser_reader_construct_message (reader, &error);

				g_assert_no_error (error);
				g_assert_nonnull (message);
				if (!count) {
					GByteArray *bytes = read_content (g_mime_part_get_content (GMIME_PART (message->mime_part)), FALSE);
					gsize length = mode & 4 ? sizeof (body) - 2 : strlen ("before");

					g_assert_cmpmem (bytes->data, bytes->len, body, length);
					g_byte_array_unref (bytes);
				}
				g_object_unref (message);
				g_assert_cmpuint (++count, <=, 3);
			}
			g_assert_cmpuint (count, ==, mode & 4 ? 2 : 3);
			g_object_unref (reader);
			g_object_unref (input);
		}
		g_string_free (wire, TRUE);
	}
	testsuite_check_passed ();
}

static void
test_mailbox_model_errors (void)
{
	static const char *wires[] = {
		"From first\r\nSubject: first\r\n\r\nbody\r\nFrom second\r\nSubject: second\r\n\r\nlast",
		"\001\001\001\001\r\nSubject: first\r\n\r\nbody\r\n\001\001\001\001\r\n"
		"\001\001\001\001\r\nSubject: second\r\n\r\nlast\r\n\001\001\001\001\r\n"
	};
	static const char recovered[] = "Subject: recovered\n\nreplacement\n";
	guint mode, format;
	gsize offset;

	testsuite_check ("Mailbox read failures clear envelopes, stay sticky and recover with a replacement stream");
	for (format = 0; format < G_N_ELEMENTS (wires); format++) {
		for (mode = 0; mode < 8; mode++) {
			for (offset = 0; offset <= strlen (wires[format]); offset++) {
				ModelInput *input = new_input (wires[format], strlen (wires[format]), 1);
				ModelInput *replacement;
				GMimeReader *reader;
				GMimeMessage *message;
				GError *error = NULL;
				int code = mode & 4 ? GMIME_ERROR_GENERAL : EIO;
				guint count = 0, reads;

				input->fail_after = offset;
				input->nonseekable = (mode & 1) != 0;
				input->unset_errno = (mode & 4) != 0;
				reader = _g_mime_parser_reader_new (NULL, (GMimeStream *) input, (mode & 2) != 0);
				g_mime_reader_set_stream_with_format (reader, (GMimeStream *) input,
								     format ? GMIME_FORMAT_MMDF : GMIME_FORMAT_MBOX);
				while ((message = _g_mime_parser_reader_construct_message (reader, &error))) {
					g_object_unref (message);
					g_assert_cmpuint (++count, <=, 2);
				}
				g_assert_error (error, GMIME_ERROR, code);
				g_clear_error (&error);
				g_assert_null (_g_mime_parser_reader_get_mbox_marker (reader));
				g_assert_cmpint (_g_mime_parser_reader_get_mbox_marker_offset (reader), ==, -1);
				reads = input->reads;
				g_assert_null (_g_mime_parser_reader_construct_message (reader, &error));
				g_assert_error (error, GMIME_ERROR, code);
				g_assert_cmpuint (input->reads, ==, reads);
				g_clear_error (&error);
				replacement = new_input (recovered, sizeof (recovered) - 1, 3);
				replacement->nonseekable = (mode & 1) != 0;
				g_mime_reader_set_stream (reader, (GMimeStream *) replacement);
				message = _g_mime_parser_reader_construct_message (reader, &error);
				g_assert_no_error (error);
				g_assert_nonnull (message);
				g_assert_cmpstr (g_mime_message_get_subject (message), ==, "recovered");
				g_assert_null (_g_mime_parser_reader_get_mbox_marker (reader));
				g_assert_cmpint (_g_mime_parser_reader_get_mbox_marker_offset (reader), ==, -1);
				g_object_unref (message);
				g_object_unref (replacement);
				g_object_unref (reader);
				g_object_unref (input);
			}
		}
	}
	testsuite_check_passed ();
}

static GMimeMultipart *
parse_multipart (const char *wire)
{
	ModelInput *input = new_input (wire, strlen (wire), 1);
	GMimeReader *reader = _g_mime_parser_reader_new (NULL, (GMimeStream *) input, FALSE);
	GError *error = NULL;
	GMimeEntity *entity = _g_mime_parser_reader_construct_part (reader, &error);

	g_assert_no_error (error);
	g_assert_true (GMIME_IS_MULTIPART (entity));
	g_object_unref (reader);
	g_object_unref (input);
	return (GMimeMultipart *) entity;
}

static void
test_multipart_models (void)
{
	static const char wire[] = "Content-Type: multipart/mixed; boundary=b\n\npre\0one\n"
		"--b \t\r\n--b\r\nContent-Type: application/octet-stream\n"
		"Content-Transfer-Encoding: binary\n\nbody\0tail\r\n--b-- \t\r\nepi\0tail\n";
	static const char expected[] = "Content-Type: multipart/mixed; boundary=b\n\npre\0one\n"
		"--b \t\r\n--b\r\nContent-Type: application/octet-stream\n"
		"Content-Transfer-Encoding: binary\n\nbody\0tail\n--b-- \t\nepi\0tail\n";
	static const char mutations[] = "Content-Type: multipart/mixed; boundary=b\n\n"
		"--b \t\n\none\n--b\t\n\ntwo\n--b  ";
	gsize chunks[] = { 1, 7, 4096 };
	guint mode, chunk, operation;

	testsuite_check ("Multipart models preserve binary text, raw markers and source lifetimes");
	for (mode = 0; mode < 8; mode++) {
		for (chunk = 0; chunk < G_N_ELEMENTS (chunks); chunk++) {
			ModelInput *input = new_input (wire, sizeof (wire) - 1, chunks[chunk]);
			GMimeReader *reader;
			GMimeEntity *root;
			GMimeMultipart *multipart;
			GMimeStream *output;
			GError *error = NULL;
			GByteArray *bytes;
			ssize_t written;

			input->nonseekable = (mode & 1) != 0;
			reader = _g_mime_parser_reader_new (NULL, (GMimeStream *) input, (mode & 2) != 0);
			root = mode & 4 ? (GMimeEntity *) _g_mime_parser_reader_construct_message (reader, &error) :
				_g_mime_parser_reader_construct_part (reader, &error);
			g_assert_no_error (error);
			g_assert_nonnull (root);
			multipart = (GMimeMultipart *) (mode & 4 ? ((GMimeMessage *) root)->mime_part : root);
			g_assert_true (GMIME_IS_MULTIPART (multipart));
			g_assert_cmpuint (input->views, ==, (mode & 2) && !(mode & 1) ? 1 : 0);
			g_object_unref (reader);
			g_object_unref (input);
			g_assert_cmpuint (multipart->children->len, ==, 2);
			assert_raw_bytes (multipart->raw_preamble, "pre\0one\n", 8);
			assert_raw_bytes (multipart->raw_epilogue, "\r\nepi\0tail\n", 11);
			assert_raw_bytes (multipart->raw_end_boundary, "--b-- \t", 7);
			assert_raw_bytes (multipart->raw_boundaries->pdata[0], "--b \t\r\n", 7);
			assert_raw_bytes (multipart->raw_boundaries->pdata[1], "--b\r\n", 5);
			g_assert_cmpstr (g_mime_multipart_get_prologue (multipart), ==, "pre");
			g_assert_cmpstr (g_mime_multipart_get_epilogue (multipart), ==, "epi");
			/* Reassigning the decoded view must not discard binary bytes. */
			g_mime_multipart_set_prologue (multipart, multipart->prologue);
			g_mime_multipart_set_epilogue (multipart, multipart->epilogue);
			output = g_mime_stream_block_new ();
			written = g_mime_entity_write_to_stream (root, NULL, output);
			bytes = g_mime_stream_block_to_byte_array ((GMimeStreamBlock *) output, NULL);
			g_assert_cmpint (written, ==, bytes->len);
			g_assert_cmpmem (bytes->data, bytes->len, expected, sizeof (expected) - 1);
			g_byte_array_unref (bytes);
			g_object_unref (output);
			g_object_unref (root);
		}
	}
	testsuite_check_passed ();

	testsuite_check ("Multipart EOF markers produce the same children regardless of read-ahead");
	{
		static const char *bodies[] = { "--b", "--b\n", "--b\n--b\n", "--b\n\nx\n--b\n" };
		static const guint counts[] = { 0, 1, 2, 2 };
		guint body;

		for (body = 0; body < G_N_ELEMENTS (bodies); body++) {
			char *text = g_strconcat ("Content-Type: multipart/mixed; boundary=b\n\n", bodies[body], NULL);

			for (chunk = 0; chunk < G_N_ELEMENTS (chunks); chunk++) {
				ModelInput *input = new_input (text, strlen (text), chunks[chunk]);
				GMimeReader *reader = _g_mime_parser_reader_new (NULL, (GMimeStream *) input, FALSE);
				GError *error = NULL;
				GMimeEntity *entity = _g_mime_parser_reader_construct_part (reader, &error);
				GMimeMultipart *multipart = (GMimeMultipart *) entity;
				GMimeStream *output = g_mime_stream_block_new ();
				GByteArray *bytes;

				g_assert_no_error (error);
				g_assert_true (GMIME_IS_MULTIPART (multipart));
				g_assert_cmpuint (multipart->children->len, ==, counts[body]);
				g_assert_cmpint (multipart->raw_trailing_boundary != NULL, ==, body == 0);
				g_assert_cmpuint (g_bytes_get_size (multipart->raw_end_boundary), ==, 0);
				g_assert_cmpint (g_mime_entity_write_to_stream (entity, NULL, output), ==, strlen (text));
				bytes = g_mime_stream_block_to_byte_array ((GMimeStreamBlock *) output, NULL);
				g_assert_cmpmem (bytes->data, bytes->len, text, strlen (text));
				g_byte_array_unref (bytes);
				g_object_unref (output);
				g_object_unref (entity);
				g_object_unref (reader);
				g_object_unref (input);
			}
			g_free (text);
		}
	}
	testsuite_check_passed ();

	testsuite_check ("Multipart mutations keep delimiter slots aligned and invalidate changed boundaries");
	for (operation = 0; operation < 11; operation++) {
		GMimeMultipart *multipart = parse_multipart (mutations);
		GMimeEntity *first = g_object_ref (multipart->children->pdata[0]);
		GMimeEntity *removed;
		GBytes *original = g_bytes_ref (multipart->raw_boundaries->pdata[0]);
		GMimeContentType *type;
		GMimeStream *output;
		GByteArray *bytes;

		g_assert_cmpuint (multipart->children->len, ==, 2);
		g_assert_nonnull (multipart->raw_trailing_boundary);
		if (operation == 0) {
			g_mime_multipart_add (multipart, first);
			g_assert_null (multipart->raw_boundaries->pdata[2]);
		} else if (operation == 1) {
			g_mime_multipart_insert (multipart, 0, first);
			g_assert_null (multipart->raw_boundaries->pdata[0]);
			g_assert_true (multipart->raw_boundaries->pdata[1] == original);
		} else if (operation == 2) {
			g_assert_true (g_mime_multipart_remove (multipart, first));
			g_assert_cmpuint (multipart->raw_boundaries->len, ==, 1);
		} else if (operation == 3) {
			removed = g_mime_multipart_remove_at (multipart, 0);
			g_object_unref (removed);
			g_assert_cmpuint (multipart->raw_boundaries->len, ==, 1);
		} else if (operation == 4) {
			removed = g_mime_multipart_replace (multipart, 1, first);
			g_object_unref (removed);
			g_assert_cmpuint (multipart->raw_boundaries->len, ==, 2);
			g_assert_true (multipart->raw_boundaries->pdata[0] == original);
		} else if (operation == 5) {
			g_mime_multipart_clear (multipart);
			g_assert_null (multipart->raw_boundaries);
			g_assert_null (multipart->raw_end_boundary);
		} else if (operation == 6) {
			g_mime_multipart_set_boundary (multipart, "changed");
			g_mime_multipart_set_boundary (multipart, "b");
		} else if (operation == 7) {
			g_mime_content_type_set_parameter (((GMimeEntity *) multipart)->content_type, "boundary", "changed");
		} else if (operation == 8) {
			type = g_mime_content_type_parse (NULL, "multipart/mixed; boundary=changed");
			g_mime_entity_set_content_type ((GMimeEntity *) multipart, type);
			g_object_unref (type);
		} else if (operation == 9) {
			g_mime_entity_set_header ((GMimeEntity *) multipart, "Content-Type",
						 "multipart/mixed; boundary=changed", NULL);
		} else {
			g_mime_content_type_set_parameter (((GMimeEntity *) multipart)->content_type, "x", "value");
			g_assert_true (multipart->raw_boundaries->pdata[0] == original);
			g_assert_nonnull (multipart->raw_trailing_boundary);
		}
		if (operation < 6)
			g_assert_null (multipart->raw_trailing_boundary);
		else if (operation < 10) {
			g_assert_null (multipart->raw_boundaries);
			g_assert_null (multipart->raw_end_boundary);
		}
		output = g_mime_stream_block_new ();
		g_assert_cmpint (g_mime_entity_write_to_stream ((GMimeEntity *) multipart, NULL, output), >, 0);
		bytes = g_mime_stream_block_to_byte_array ((GMimeStreamBlock *) output, NULL);
		g_assert_cmpuint (bytes->len, >, 0);
		g_byte_array_unref (bytes);
		g_object_unref (output);
		g_bytes_unref (original);
		g_object_unref (first);
		type = g_object_ref (((GMimeEntity *) multipart)->content_type);
		g_object_unref (multipart);
		g_mime_content_type_set_parameter (type, "boundary", "after-owner-finalization");
		g_object_unref (type);
	}
	testsuite_check_passed ();
}

static void
test_multipart_output (void)
{
	static const char *wires[] = {
		"Content-Type: multipart/mixed; boundary=b\n\npre\n--b\n\nbody\n--b--\nepilogue",
		"Content-Type: multipart/mixed; boundary=b\n\n--b\n\nbody",
		"Content-Type: multipart/mixed; boundary=b\n\n--b\n\nbody\n--b",
		"Content-Type: multipart/mixed; boundary=b\n\n--b\nContent-Type: message/rfc822\n\n"
		"invalid marker\nSubject: test\n\nbody"
	};
	guint i, newline;

	testsuite_check ("Multipart output counts normalized bytes and propagates write/filter-completion failures");
	for (i = 0; i < G_N_ELEMENTS (wires); i++) {
		GMimeMultipart *multipart = parse_multipart (wires[i]);

		((GMimeEntity *) multipart)->ensure_newline = TRUE;
		for (newline = 0; newline < 2; newline++) {
			GMimeFormatOptions *options = g_mime_format_options_new ();
			GMimeStream *stream = g_mime_stream_block_new ();
			GByteArray *bytes;
			gsize offset;

			g_mime_format_options_set_newline_format (options, newline);
			g_assert_cmpint (g_mime_entity_write_to_stream ((GMimeEntity *) multipart, options, stream), >, 0);
			bytes = g_mime_stream_block_to_byte_array ((GMimeStreamBlock *) stream, NULL);
			for (offset = 0; offset <= bytes->len; offset++) {
				ModelOutput *output = g_object_new (model_output_get_type (), NULL);
				ssize_t result;

				output->limit = offset;
				errno = 0;
				result = g_mime_entity_write_to_stream ((GMimeEntity *) multipart, options, (GMimeStream *) output);
				if (offset < bytes->len) {
					g_assert_cmpint (result, ==, -1);
					g_assert_cmpint (errno, ==, EIO);
				} else {
					g_assert_cmpint (result, ==, bytes->len);
					g_assert_cmpuint (output->written, ==, bytes->len);
				}
				g_object_unref (output);
			}
			g_byte_array_unref (bytes);
			g_object_unref (stream);
			g_mime_format_options_free (options);
		}
		g_object_unref (multipart);
	}
	testsuite_check_passed ();
}

static void
test_model_errors (void)
{
	static const char *containers[] = {
		"Content-Type: application/x-reader-unsupported\n\nbody",
		"Content-Type: message/rfc822\n\nContent-Type: application/x-reader-unsupported\n\nbody",
		"Content-Type: text/rfc822-headers\n\nContent-Type: application/x-reader-unsupported\n\nbody"
	};
	static const char wire[] = "X: a\n\nbody";
	guint i, unset;

	g_mime_entity_register_type ("application", "x-reader-unsupported", GMIME_TYPE_MESSAGE_PART);
	testsuite_check ("Model construction reports unsupported containers and cleans partial failures");
	for (i = 0; i < G_N_ELEMENTS (containers); i++) {
		ModelInput *input = new_input (containers[i], strlen (containers[i]), 1);
		GMimeReader *reader = _g_mime_parser_reader_new (NULL, (GMimeStream *) input, FALSE);
		GError *error = NULL;
		guint reads;

		g_assert_null (_g_mime_parser_reader_construct_part (reader, &error));
		g_assert_error (error, GMIME_ERROR, GMIME_ERROR_NOT_SUPPORTED);
		g_clear_error (&error);
		reads = input->reads;
		g_assert_null (_g_mime_parser_reader_construct_headers (reader, FALSE, &error));
		g_assert_error (error, GMIME_ERROR, GMIME_ERROR_NOT_SUPPORTED);
		g_assert_cmpuint (input->reads, ==, reads);
		g_clear_error (&error);
		g_object_unref (reader);
		g_object_unref (input);
	}

	for (unset = 0; unset < 2; unset++) {
		int code = unset ? GMIME_ERROR_GENERAL : EIO;

		for (i = 0; i < sizeof (wire) - 1; i++) {
			ModelInput *input = new_input (wire, sizeof (wire) - 1, 1);
			GMimeReader *reader = _g_mime_parser_reader_new (NULL, (GMimeStream *) input, FALSE);
			GError *error = NULL;

			input->fail_after = i;
			input->unset_errno = unset;
			g_assert_null (_g_mime_parser_reader_construct_part (reader, &error));
			g_assert_error (error, GMIME_ERROR, code);
			g_clear_error (&error);
			g_object_unref (reader);
			g_object_unref (input);
		}
		for (i = 0; i < 2; i++) {
			ModelInput *input = new_input (wire, sizeof (wire) - 1, 1);
			GMimeReader *reader = _g_mime_parser_reader_new (NULL, (GMimeStream *) input, TRUE);
			GError *error = NULL;

			input->unset_errno = unset;
			input->fail_substream = i == 0;
			if (i == 1) {
				GMimeHeaderList *headers = _g_mime_parser_reader_construct_headers (reader, FALSE, &error);

				g_assert_no_error (error);
				g_object_unref (headers);
				g_assert_cmpint (g_mime_stream_seek ((GMimeStream *) input, 0, GMIME_STREAM_SEEK_SET), ==, 0);
				input->fail_seek = TRUE;
			}
			g_assert_null (_g_mime_parser_reader_construct_part (reader, &error));
			g_assert_error (error, GMIME_ERROR, code);
			g_clear_error (&error);
			g_assert_false (_g_mime_reader_restore_stream_position (reader, &error));
			g_assert_error (error, GMIME_ERROR, code);
			g_clear_error (&error);
			g_object_unref (reader);
			g_object_unref (input);
		}
	}
	testsuite_check_passed ();
}

static void
test_empty_headers (void)
{
	static const char *cases[] = { "", "\n", "\r\n", "X: a", "X: a\n", "X: a\n\n" };
	guint i;

	testsuite_check ("Header models preserve separator state at empty input and EOF");
	for (i = 0; i < G_N_ELEMENTS (cases); i++) {
		ModelInput *input = new_input (cases[i], strlen (cases[i]), 1);
		GMimeReader *reader = _g_mime_parser_reader_new (NULL, (GMimeStream *) input, FALSE);
		GError *error = NULL;
		GMimeHeaderList *headers = _g_mime_parser_reader_construct_headers (reader, FALSE, &error);

		g_assert_no_error (error);
		g_assert_nonnull (headers);
		g_assert_cmpint (headers->has_body_separator, ==, i == 1 || i == 2 || i == 5);
		g_object_unref (headers);
		headers = _g_mime_parser_reader_construct_headers (reader, FALSE, &error);
		g_assert_no_error (error);
		g_assert_false (headers->has_body_separator);
		g_object_unref (headers);
		g_object_unref (reader);
		g_object_unref (input);
	}
	testsuite_check_passed ();
}

static void
release_public_parser (GMimeParser *parser, const char *name, const char *value, gint64 offset, gpointer user_data)
{
	guint *calls = user_data;

	(*calls)++;
	g_assert_cmpstr (name, ==, "Subject");
	g_assert_cmpstr (value, ==, " retained\n");
	g_assert_cmpint (offset, ==, 0);
	g_test_expect_message ("gmime", G_LOG_LEVEL_CRITICAL, "*!parser->priv->reading*");
	g_assert_null (g_mime_parser_construct_part (parser, NULL, NULL));
	g_test_assert_expected_messages ();
	g_mime_parser_set_header_regex (parser, NULL, NULL, NULL);
	g_object_unref (parser);
}

static void
test_public_parser_models (void)
{
	static const char wire[] = "X-Skip: first\n\n"
		"From sender\0binary\nSubject: first\nContent-Type: text/plain\n\none\0tail\n"
		"From next\nSubject: second\n\nlast";
	const gsize chunks[] = { 1, 7, 4096 };
	guint mode, chunk;

	testsuite_check ("Public parser retains one buffer across headers, formats, persistence and mailbox reads");
	for (mode = 0; mode < 4; mode++) {
		for (chunk = 0; chunk < G_N_ELEMENTS (chunks); chunk++) {
			ModelInput *input = new_input (wire, sizeof (wire) - 1, chunks[chunk]);
			GMimeParser *parser;
			GMimeHeaderList *headers;
			GMimeMessage *first, *second;
			GMimeContent *saved;
			GByteArray *bytes;
			GError *error = NULL;
			char *marker;

			input->nonseekable = (mode & 1) != 0;
			parser = g_mime_parser_new_with_stream ((GMimeStream *) input);
			g_mime_parser_set_persist_stream (parser, (mode & 2) != 0);
			headers = g_mime_parser_construct_headers (parser, NULL, &error);
			g_assert_no_error (error);
			g_assert_cmpint (g_mime_header_list_get_count (headers), ==, 1);
			g_assert_cmpint (g_mime_parser_get_headers_begin (parser), ==, 0);
			g_assert_cmpint (g_mime_parser_get_headers_end (parser), ==, 14);
			g_assert_cmpint (g_mime_parser_tell (parser), ==, 15);
			g_object_unref (headers);
			g_mime_parser_set_format (parser, GMIME_FORMAT_MBOX);
			first = g_mime_parser_construct_message (parser, NULL, &error);
			g_assert_no_error (error);
			g_assert_nonnull (first);
			g_assert_cmpstr (g_mime_message_get_subject (first), ==, "first");
			g_assert_cmpint (g_mime_parser_get_mbox_marker_offset (parser), ==, 15);
			marker = g_mime_parser_get_mbox_marker (parser);
			g_assert_cmpstr (marker, ==, "From sender");
			g_free (marker);
			saved = g_mime_part_get_content (GMIME_PART (first->mime_part));
			bytes = read_content (saved, FALSE);
			g_assert_cmpmem (bytes->data, bytes->len, "one\0tail", 8);
			g_byte_array_unref (bytes);
			if ((mode & 2) && !(mode & 1))
				g_assert_cmpint (g_mime_stream_seek ((GMimeStream *) input, 0, GMIME_STREAM_SEEK_SET), ==, 0);
			g_mime_parser_set_persist_stream (parser, FALSE);
			second = g_mime_parser_construct_message (parser, NULL, &error);
			g_assert_no_error (error);
			g_assert_nonnull (second);
			g_assert_cmpstr (g_mime_message_get_subject (second), ==, "second");
			g_assert_cmpint (g_mime_parser_get_mbox_marker_offset (parser), ==, 84);
			g_assert_null (g_mime_parser_construct_message (parser, NULL, &error));
			g_assert_no_error (error);
			g_assert_true (g_mime_parser_eos (parser));
			g_assert_cmpint (g_mime_parser_get_mbox_marker_offset (parser), ==, -1);
			g_object_unref (parser);
			g_object_unref (input);
			bytes = read_content (saved, FALSE);
			g_assert_cmpmem (bytes->data, bytes->len, "one\0tail", 8);
			g_byte_array_unref (bytes);
			g_object_unref (second);
			g_object_unref (first);
		}
	}
	testsuite_check_passed ();

	testsuite_check ("Public parsing retains incomplete embedded-message pre-header bytes at EOF");
	for (chunk = 0; chunk < G_N_ELEMENTS (chunks); chunk++) {
		static const char incomplete[] = "Content-Type: message/rfc822\n\nSubject: outer\n"
			"Content-Type: multipart/digest; boundary=b\n\n--b\n\nx";
		ModelInput *input = new_input (incomplete, sizeof (incomplete) - 1, chunks[chunk]);
		GMimeParser *parser = g_mime_parser_new_with_stream ((GMimeStream *) input);
		GMimeEntity *entity, *child;
		GMimeMessage *outer, *inner;
		GError *error = NULL;
		gconstpointer bytes;
		gsize length;

		entity = g_mime_parser_construct_part (parser, NULL, &error);
		g_assert_no_error (error);
		g_assert_true (GMIME_IS_MESSAGE_PART (entity));
		outer = g_mime_message_part_get_message (GMIME_MESSAGE_PART (entity));
		g_assert_true (GMIME_IS_MULTIPART (outer->mime_part));
		child = g_mime_multipart_get_part (GMIME_MULTIPART (outer->mime_part), 0);
		g_assert_true (GMIME_IS_MESSAGE_PART (child));
		inner = g_mime_message_part_get_message (GMIME_MESSAGE_PART (child));
		g_assert_nonnull (inner);
		g_assert_nonnull (inner->raw_marker);
		bytes = g_bytes_get_data (inner->raw_marker, &length);
		g_assert_cmpmem (bytes, length, "x", 1);
		g_object_unref (entity);
		g_object_unref (parser);
		g_object_unref (input);
	}
	testsuite_check_passed ();

	testsuite_check ("Public header callbacks may unregister and release the parser without reentrant reads");
	{
		ModelInput *input = new_input ("Subject: retained\nX: ignored\n\nbody", 34, 1);
		GMimeParser *parser = g_mime_parser_new_with_stream ((GMimeStream *) input);
		GMimeParser *weak = parser;
		GMimeMessage *message;
		GError *error = NULL;
		guint calls = 0;

		g_object_add_weak_pointer (G_OBJECT (parser), (gpointer *) &weak);
		g_mime_parser_set_header_regex (parser, "^subject$", release_public_parser, &calls);
		message = g_mime_parser_construct_message (parser, NULL, &error);
		g_assert_no_error (error);
		g_assert_nonnull (message);
		g_assert_cmpuint (calls, ==, 1);
		g_assert_null (weak);
		g_assert_cmpstr (g_mime_message_get_subject (message), ==, "retained");
		g_object_unref (message);
		g_object_unref (input);
	}
	testsuite_check_passed ();
}

static void
count_public_warning (gint64 offset, GMimeParserWarning warning, const char *item, gpointer user_data)
{
	guint *counts = user_data;

	g_assert_cmpuint (warning, <=, GMIME_CRIT_PART_WITHOUT_HEADERS_OR_CONTENT);
	counts[warning]++;
}

static void
test_public_long_header_names (void)
{
	const gsize lengths[] = { 4095, 4096, 4097, 4223, 4224, 4225, 8192, 8448 };
	const gsize chunks[] = { 1, 7, 4096 };
	guint i, j, blank;

	testsuite_check ("Public header names spanning the reader buffer retain their names and wire bytes");
	for (i = 0; i < G_N_ELEMENTS (lengths); i++) {
		for (j = 0; j < G_N_ELEMENTS (chunks); j++) {
			for (blank = 0; blank < 2; blank++) {
				char *name = g_strnfill (lengths[i], 'X');
				char *wire = g_strconcat (name, blank ? " \t: value\n\n" : ": value\n\n", NULL);
				ModelInput *input = new_input (wire, strlen (wire), chunks[j]);
				GMimeParser *parser = g_mime_parser_new_with_stream ((GMimeStream *) input);
				GError *error = NULL;
				GMimeHeaderList *headers = g_mime_parser_construct_headers (parser, NULL, &error);
				GMimeHeader *header;

				if (error)
					testsuite_printf (stderr, 1, "Long header: length=%" G_GSIZE_FORMAT
							  ", chunk=%" G_GSIZE_FORMAT ", blanks=%u: %s\n",
							  lengths[i], chunks[j], blank, error->message);
				g_assert_no_error (error);
				g_assert_nonnull (headers);
				g_assert_cmpint (g_mime_header_list_get_count (headers), ==, 1);
				header = g_mime_header_list_get_header_at (headers, 0);
				g_assert_false (header->invalid);
				g_assert_cmpstr (header->name, ==, name);
				g_assert_cmpstr (header->raw_value, ==, " value\n");
				g_assert_cmpuint (g_bytes_get_size (header->raw_field_bytes), ==, lengths[i] + blank * 2);
				g_assert_cmpint (g_mime_parser_tell (parser), ==, strlen (wire));
				g_object_unref (headers);
				g_object_unref (parser);
				g_object_unref (input);
				g_free (wire);
				g_free (name);
			}
		}
	}
	testsuite_check_passed ();
}

static void
test_public_parser_warnings (void)
{
	static const struct {
		const char *wire;
		GMimeParserWarning warning;
	} cases[] = {
		{ "Subject: same\nSubject: same\n\n", GMIME_WARN_DUPLICATED_HEADER },
		{ "Subject: one\nSubject: two\n\n", GMIME_CRIT_CONFLICTING_HEADER },
		{ "Subject: \xc3\xa9\n\n", GMIME_WARN_UNENCODED_8BIT_HEADER },
		{ "Content-Type: multipart/mixed\n\nbody", GMIME_CRIT_MULTIPART_WITHOUT_BOUNDARY },
		{ "Content-Type: multipart/mixed; boundary=b\n\nbody", GMIME_WARN_TRUNCATED_MESSAGE },
		{ "Content-Type: multipart/mixed; boundary=b\n\n--b\n"
		  "Content-Type: multipart/mixed; boundary=c\n\n--c\n\nbody\n--b--\n",
		  GMIME_WARN_MALFORMED_MULTIPART },
		{ "Content-Type: multipart/mixed; boundary=b\n\n--b\nX: value\n--b--\n",
		  GMIME_WARN_PART_WITHOUT_CONTENT },
		{ "Content-Type: multipart/mixed; boundary=b\n\n--b\n--b--\n",
		  GMIME_CRIT_PART_WITHOUT_HEADERS_OR_CONTENT }
	};
	const gsize chunks[] = { 1, 7, 4096 };
	guint i, j;

	testsuite_check ("Public parser warnings distinguish malformed ancestors from actual EOF truncation");
	for (i = 0; i < G_N_ELEMENTS (cases); i++) {
		for (j = 0; j < G_N_ELEMENTS (chunks); j++) {
			ModelInput *input = new_input (cases[i].wire, strlen (cases[i].wire), chunks[j]);
			GMimeParser *parser = g_mime_parser_new_with_stream ((GMimeStream *) input);
			GMimeParserOptions *options = g_mime_parser_options_new ();
			guint counts[GMIME_CRIT_PART_WITHOUT_HEADERS_OR_CONTENT + 1] = { 0 };
			GMimeMessage *message;
			GError *error = NULL;

			g_mime_parser_options_set_warning_callback (options, count_public_warning, counts);
			message = g_mime_parser_construct_message (parser, options, &error);
			g_assert_no_error (error);
			g_assert_nonnull (message);
			g_assert_cmpuint (counts[cases[i].warning], ==, 1);
			g_assert_cmpuint (counts[GMIME_WARN_TRUNCATED_MESSAGE], ==,
					  cases[i].warning == GMIME_WARN_TRUNCATED_MESSAGE);
			g_object_unref (message);
			g_mime_parser_options_free (options);
			g_object_unref (parser);
			g_object_unref (input);
		}
	}
	testsuite_check_passed ();
}

typedef GMimeCryptoContext ModelVerifyContext;
typedef GMimeCryptoContextClass ModelVerifyContextClass;

static GByteArray *verify_content_bytes, *verify_signature_bytes;
static GMimeVerifyFlags verify_flags;
static guint verify_calls;

G_DEFINE_TYPE (ModelVerifyContext, model_verify_context, GMIME_TYPE_CRYPTO_CONTEXT)

static const char *
model_verify_protocol (GMimeCryptoContext *ctx)
{
	return "application/x-model-signature";
}

static GByteArray *
capture_verify_stream (GMimeStream *stream)
{
	GByteArray *bytes = g_byte_array_new ();
	char buffer[127];
	ssize_t nread;

	g_assert_cmpint (g_mime_stream_tell (stream), ==, 0);
	while ((nread = g_mime_stream_read (stream, buffer, sizeof (buffer))) > 0)
		g_byte_array_append (bytes, (const guint8 *) buffer, nread);
	g_assert_cmpint (nread, ==, 0);
	return bytes;
}

static GMimeSignatureList *
model_verify (GMimeCryptoContext *ctx, GMimeVerifyFlags flags, GMimeStream *istream,
	      GMimeStream *sigstream, GMimeStream *ostream, GError **error)
{
	verify_calls++;
	verify_flags = flags;
	g_clear_pointer (&verify_content_bytes, g_byte_array_unref);
	g_clear_pointer (&verify_signature_bytes, g_byte_array_unref);
	verify_content_bytes = capture_verify_stream (istream);
	verify_signature_bytes = capture_verify_stream (sigstream);

	/* Capture the verification inputs; this backend does not verify cryptography. */
	return g_mime_signature_list_new ();
}

static void
model_verify_context_class_init (ModelVerifyContextClass *klass)
{
	klass->get_signature_protocol = model_verify_protocol;
	klass->verify = model_verify;
}

static void
model_verify_context_init (ModelVerifyContext *ctx)
{
}

static GMimeCryptoContext *
new_model_verify_context (void)
{
	return g_object_new (model_verify_context_get_type (), NULL);
}

typedef struct {
	GMimeMultipart parent_object;
	GMimeStream *output;
} ModelVerifyMultipart;

typedef GMimeMultipartClass ModelVerifyMultipartClass;

G_DEFINE_TYPE (ModelVerifyMultipart, model_verify_multipart, GMIME_TYPE_MULTIPART)

static ssize_t
model_verify_multipart_write (GMimeEntity *entity, GMimeFormatOptions *options,
			      gboolean content_only, GMimeStream *stream)
{
	ModelVerifyMultipart *multipart = (ModelVerifyMultipart *) entity;

	/* Inject output failures into the real writer with verification options. */
	return GMIME_ENTITY_CLASS (model_verify_multipart_parent_class)->write_to_stream (
		entity, options, content_only, multipart->output ? multipart->output : stream);
}

static void
model_verify_multipart_class_init (ModelVerifyMultipartClass *klass)
{
	GMIME_ENTITY_CLASS (klass)->write_to_stream = model_verify_multipart_write;
}

static void
model_verify_multipart_init (ModelVerifyMultipart *multipart)
{
}

static GMimeMultipartSigned *
new_model_signed (GMimeEntity *content)
{
	GMimeMultipartSigned *signed_part = g_mime_multipart_signed_new ();
	GMimePart *signature = g_mime_part_new_with_type ("application", "x-model-signature");
	GMimeStream *stream = g_mime_stream_mem_new_with_buffer ("c2lnAGJ5dGVz", 12);
	GMimeContent *wrapper = g_mime_content_new_with_stream (stream, GMIME_CONTENT_ENCODING_BASE64);

	g_mime_part_set_content (signature, wrapper);
	g_mime_entity_set_content_type_parameter ((GMimeEntity *) signed_part, "protocol", "application/x-model-signature");
	g_mime_multipart_add ((GMimeMultipart *) signed_part, content);
	g_mime_multipart_add ((GMimeMultipart *) signed_part, (GMimeEntity *) signature);
	g_object_unref (wrapper);
	g_object_unref (stream);
	g_object_unref (signature);
	return signed_part;
}

static GByteArray *
serialize_verify_content (GMimeEntity *entity)
{
	GMimeFormatOptions *options = g_mime_format_options_new ();
	GMimeStream *stream = g_mime_stream_mem_new ();
	GByteArray *bytes;
	ssize_t written;

	g_mime_format_options_set_newline_format (options, GMIME_NEWLINE_FORMAT_DOS);
	written = g_mime_entity_write_to_stream (entity, options, stream);
	g_assert_cmpint (written, >=, 0);
	bytes = g_byte_array_ref (g_mime_stream_mem_get_byte_array ((GMimeStreamMem *) stream));
	g_assert_cmpint (written, ==, bytes->len);
	g_object_unref (stream);
	g_mime_format_options_free (options);
	return bytes;
}

static void
test_signature_markers (void)
{
	static const char wire[] = "Content-Type: multipart/x-model-verify; boundary=b\r\n\r\n"
		"--b\r\nContent-Type: multipart/signed; boundary=c\r\n\r\n--c\r\n"
		"Content-Type: application/octet-stream\r\nContent-Transfer-Encoding: binary\r\n\r\n"
		"A\0B\nC\r\n--c--\r\n--b--\r\n";
	static const char trailing[] = "Content-Type: multipart/x-model-verify; boundary=b\r\n\r\n"
		"--b\r\n\r\nbody\r\n--b";
	static const struct {
		const char *raw, *canonical;
		gsize raw_length, canonical_length;
	} markers[] = {
		{ "", "", 0, 0 },
		{ "\n", "\r\n", 1, 2 },
		{ "\r\n", "\r\n", 2, 2 },
		{ "--b\r", "--b\r", 4, 4 },
		{ "--b", "--b", 3, 3 },
		{ "--b\n--b\r\n--b\n", "--b\r\n--b\r\n--b\r\n", 13, 15 },
		{ "--b\0x\n--b\r\r\n", "--b\0x\r\n--b\r\r\n", 12, 13 },
		{ "\n--b \t\n--b--", "\r\n--b \t\r\n--b--", 12, 14 }
	};
	guint slot_index, i, mode;

	g_mime_crypto_context_register ("application/x-model-signature", new_model_verify_context);
	g_mime_entity_register_type ("multipart", "x-model-verify", model_verify_multipart_get_type ());
	testsuite_check ("Verification canonicalizes only raw marker LF and preserves binary data and ordinary output");
	for (slot_index = 0; slot_index < 3; slot_index++) {
		for (i = 0; i < G_N_ELEMENTS (markers); i++) {
			for (mode = 0; mode < 2; mode++) {
				const char *text = slot_index == 2 ? trailing : wire;
				gsize length = slot_index == 2 ? sizeof (trailing) - 1 : sizeof (wire) - 1;
				ModelInput *input = new_input (text, length, mode ? 4096 : 1);
				GMimeParser *parser = g_mime_parser_new_with_stream ((GMimeStream *) input);
				GMimeEntity *entity;
				GMimeMultipartSigned *signed_part;
				GMimeSignatureList *signatures;
				GMimeMultipart *multipart;
				GByteArray *expected, *before, *after;
				GBytes **slot;
				GError *error = NULL;

				g_mime_parser_set_persist_stream (parser, mode != 0);
				entity = g_mime_parser_construct_part (parser, NULL, &error);
				g_assert_no_error (error);
				g_assert_true (GMIME_IS_MULTIPART (entity));
				multipart = (GMimeMultipart *) entity;
				slot = slot_index == 0 ? (GBytes **) &multipart->raw_boundaries->pdata[0] :
					slot_index == 1 ? &multipart->raw_end_boundary : &multipart->raw_trailing_boundary;
				g_assert_nonnull (*slot);
				g_bytes_unref (*slot);
				*slot = g_bytes_new_static (markers[i].canonical, markers[i].canonical_length);
				expected = serialize_verify_content (entity);
				g_bytes_unref (*slot);
				*slot = g_bytes_new_static (markers[i].raw, markers[i].raw_length);
				before = serialize_verify_content (entity);
				signed_part = new_model_signed (entity);
				signatures = g_mime_multipart_signed_verify (signed_part, GMIME_VERIFY_ENABLE_KEYSERVER_LOOKUPS, &error);
				g_assert_no_error (error);
				g_assert_nonnull (signatures);
				g_assert_cmpmem (verify_content_bytes->data, verify_content_bytes->len, expected->data, expected->len);
				g_assert_cmpmem (verify_signature_bytes->data, verify_signature_bytes->len, "sig\0bytes", 9);
				g_assert_cmpint (verify_flags, ==, GMIME_VERIFY_ENABLE_KEYSERVER_LOOKUPS);
				after = serialize_verify_content (entity);
				g_assert_cmpmem (after->data, after->len, before->data, before->len);
				g_byte_array_unref (after);
				g_byte_array_unref (before);
				g_byte_array_unref (expected);
				g_object_unref (signatures);
				g_object_unref (signed_part);
				g_object_unref (entity);
				g_object_unref (parser);
				g_object_unref (input);
			}
		}
	}
	testsuite_check_passed ();

	testsuite_check ("Verification preserves mixed-newline source bodies without bypassing transcoding");
	for (mode = 0; mode < 10; mode++) {
		static const char text[] = "Content-Type: text/plain\n\none\nsecond\r\nthird\r";
		static const char raw[] = "Content-Type: text/plain\r\n\r\none\nsecond\r\nthird\r";
		ModelInput *input = new_input (text, sizeof (text) - 1, 1);
		GMimeParser *parser = g_mime_parser_new_with_stream ((GMimeStream *) input);
		GError *error = NULL;
		GMimeEntity *entity = g_mime_parser_construct_part (parser, NULL, &error);
		GMimeContent *content;
		GMimeMultipartSigned *signed_part;
		GMimeSignatureList *signatures;
		GByteArray *expected, *after;

		g_assert_no_error (error);
		g_assert_true (GMIME_IS_PART (entity));
		content = g_mime_part_get_content ((GMimePart *) entity);
		if (mode % 5 == 0)
			g_mime_content_clear_newline_format (content);
		else if (mode % 5 < 4)
			g_mime_content_set_newline_format (content, (GMimeNewLineFormat) (mode % 5 - 1));
		if (mode >= 5)
			g_mime_part_set_content_encoding ((GMimePart *) entity, GMIME_CONTENT_ENCODING_BASE64);
		expected = serialize_verify_content (entity);
		signed_part = new_model_signed (entity);
		signatures = g_mime_multipart_signed_verify (signed_part, GMIME_VERIFY_NONE, &error);
		g_assert_no_error (error);
		g_assert_nonnull (signatures);
		if (mode == 3 || mode == 4)
			g_assert_cmpmem (verify_content_bytes->data, verify_content_bytes->len, raw, sizeof (raw) - 1);
		else
			g_assert_cmpmem (verify_content_bytes->data, verify_content_bytes->len, expected->data, expected->len);
		after = serialize_verify_content (entity);
		g_assert_cmpmem (after->data, after->len, expected->data, expected->len);
		g_byte_array_unref (after);
		g_byte_array_unref (expected);
		g_object_unref (signatures);
		g_object_unref (signed_part);
		g_object_unref (entity);
		g_object_unref (parser);
		g_object_unref (input);
	}
	testsuite_check_passed ();

	testsuite_check ("Verification aborts on canonical marker write failures, including short and zero writes");
	{
		ModelVerifyMultipart *multipart = (ModelVerifyMultipart *) parse_multipart (
			"Content-Type: multipart/x-model-verify; boundary=b\n\n--b\n--b\n\nbody\n--b--\n");
		GMimeMultipartSigned *signed_part = new_model_signed ((GMimeEntity *) multipart);
		GMimeSignatureList *signatures;
		GError *error = NULL;
		gsize offset, length, marker_start, marker_end;
		const char *marker;

		signatures = g_mime_multipart_signed_verify (signed_part, GMIME_VERIFY_NONE, &error);
		g_assert_no_error (error);
		g_assert_nonnull (signatures);
		length = verify_content_bytes->len;
		marker = g_strstr_len ((const char *) verify_content_bytes->data, length, "\r\n\r\n");
		g_assert_nonnull (marker);
		marker_start = marker - (const char *) verify_content_bytes->data + 4;
		marker_end = marker_start + strlen ("--b\r\n--b\r\n");
		g_object_unref (signatures);
		for (mode = 0; mode < 3; mode++) {
			for (offset = 0; offset <= length; offset++) {
				ModelOutput *output;
				guint calls;

				if (mode == 2 && (offset < marker_start || offset >= marker_end))
					continue;
				output = g_object_new (model_output_get_type (), NULL);
				calls = verify_calls;

				output->limit = offset;
				output->chunk = mode ? 1 : G_MAXSIZE;
				output->short_start = marker_start;
				output->short_end = marker_end;
				output->zero_write = mode == 2;
				multipart->output = (GMimeStream *) output;
				signatures = g_mime_multipart_signed_verify (signed_part, GMIME_VERIFY_NONE, &error);
				if (offset < length) {
					g_assert_null (signatures);
					g_assert_error (error, GMIME_ERROR, EIO);
					g_assert_cmpuint (verify_calls, ==, calls);
					g_clear_error (&error);
				} else {
					g_assert_no_error (error);
					g_assert_nonnull (signatures);
					g_assert_cmpuint (verify_calls, ==, calls + 1);
					g_assert_cmpuint (output->written, ==, length);
					g_object_unref (signatures);
				}
				multipart->output = NULL;
				g_object_unref (output);
			}
		}
		g_object_unref (signed_part);
		g_object_unref (multipart);
	}
	testsuite_check_passed ();

	testsuite_check ("Verification rejects missing signatures and propagates source failures before invoking crypto");
	for (mode = 0; mode < 14; mode++) {
		GMimePart *part = g_mime_part_new ();
		GMimeMultipartSigned *signed_part = new_model_signed ((GMimeEntity *) part);
		GMimeEntity *signature = g_mime_multipart_get_part ((GMimeMultipart *) signed_part, 1);
		ModelInput *input = new_input ("data", 4, 1);
		GMimeContent *content = g_mime_content_new_with_stream ((GMimeStream *) input, GMIME_CONTENT_ENCODING_UNSPECIFIED);
		GMimeSignatureList *signatures;
		GError *error = NULL;
		guint calls = verify_calls;
		int code = GMIME_ERROR_PARSE_ERROR;

		if (mode < 2) {
			GMimeEntity *invalid = mode ? (GMimeEntity *) g_mime_multipart_new () :
				(GMimeEntity *) g_mime_part_new ();
			g_mime_entity_set_content_type (invalid, signature->content_type);
			g_object_unref (g_mime_multipart_replace ((GMimeMultipart *) signed_part, 1, invalid));
			g_object_unref (invalid);
		} else {
			guint fault = (mode - 2) / 2;

			if (fault < 2)
				input->fail_after = 1;
			else
				input->fail_reset_after = (fault - 2) / 2;
			input->unset_errno = (mode & 1) != 0;
			g_mime_part_set_content ((fault & 1) ? (GMimePart *) signature : part, content);
			code = input->unset_errno ? GMIME_ERROR_GENERAL : EIO;
		}
		signatures = g_mime_multipart_signed_verify (signed_part, GMIME_VERIFY_NONE, &error);
		g_assert_null (signatures);
		g_assert_error (error, GMIME_ERROR, code);
		g_assert_cmpuint (verify_calls, ==, calls);
		g_clear_error (&error);
		g_object_unref (content);
		g_object_unref (input);
		g_object_unref (signed_part);
		g_object_unref (part);
	}
	testsuite_check_passed ();
	g_clear_pointer (&verify_content_bytes, g_byte_array_unref);
	g_clear_pointer (&verify_signature_bytes, g_byte_array_unref);
}

static GByteArray *
serialize_writer (GMimeEntity *entity, GMimeFormatOptions *options, gboolean content_only)
{
	GMimeStream *stream = g_mime_stream_mem_new ();
	GByteArray *bytes;
	ssize_t written;

	written = content_only ? g_mime_entity_write_content_to_stream (entity, options, stream) :
		g_mime_entity_write_to_stream (entity, options, stream);
	bytes = g_byte_array_ref (g_mime_stream_mem_get_byte_array ((GMimeStreamMem *) stream));
	g_assert_cmpint (written, ==, bytes->len);
	g_object_unref (stream);
	return bytes;
}

static void
test_writer_depth (void)
{
	const guint depth = 8192;
	guint kind, dos, i;

	testsuite_check ("Iterative writers preserve exact bytes through deep multipart, message and mixed graphs");
	for (kind = 0; kind < 3; kind++) {
		for (dos = 0; dos < 2; dos++) {
			GMimeFormatOptions *options = g_mime_format_options_new ();
			GMimePart *leaf = g_mime_part_new_with_type ("application", "octet-stream");
			GMimeStream *stream = g_mime_stream_mem_new_with_buffer ("A\0B\nC\rD", 7);
			GMimeContent *content = g_mime_content_new_with_stream (stream, GMIME_CONTENT_ENCODING_BINARY);
			GPtrArray *entities = g_ptr_array_new ();
			GByteArray *flags = g_byte_array_new ();
			GByteArray *leaf_bytes, *bytes;
			GMimeEntity *root = (GMimeEntity *) leaf;
			ModelOutput *output = g_object_new (model_output_get_type (), NULL);
			const char *newline = dos ? "\r\n" : "\n";
			char *multipart_prefix, *message_prefix, *ending;
			GString *expected = g_string_new (NULL);
			gsize header_length;

			g_mime_format_options_set_newline_format (options, dos ? GMIME_NEWLINE_FORMAT_DOS : GMIME_NEWLINE_FORMAT_UNIX);
			g_mime_part_set_content (leaf, content);
			g_mime_part_set_content_encoding (leaf, GMIME_CONTENT_ENCODING_BINARY);
			g_object_unref (content);
			g_object_unref (stream);
			g_ptr_array_add (entities, leaf);
			leaf_bytes = serialize_writer (root, options, FALSE);
			multipart_prefix = g_strdup_printf ("Content-Type: multipart/mixed; boundary=deep%s%s--deep%s",
							  newline, newline, newline);
			message_prefix = g_strdup_printf ("Content-Type: message/rfc822%s%sMIME-Version: 1.0%s",
							newline, newline, newline);
			ending = g_strdup_printf ("%s--deep--%s", newline, newline);
			for (i = 0; i < depth; i++) {
				if (kind == 0 || (kind == 2 && (i & 1))) {
					GMimeMultipart *multipart = g_mime_multipart_new ();

					g_mime_multipart_set_boundary (multipart, "deep");
					g_mime_multipart_add (multipart, root);
					root = (GMimeEntity *) multipart;
				} else {
					GMimeMessage *message = g_mime_message_new (FALSE);

					g_mime_message_set_mime_part (message, root);
					g_ptr_array_add (entities, message);
					root = (GMimeEntity *) g_mime_message_part_new_with_message ("rfc822", message);
				}
				g_ptr_array_add (entities, root);
			}
			for (i = 0; i < entities->len; i++) {
				guint8 ensure = ((GMimeEntity *) entities->pdata[i])->ensure_newline;

				g_byte_array_append (flags, &ensure, 1);
			}
			for (i = depth; i > 0; i--)
				g_string_append (expected, kind == 0 || (kind == 2 && ((i - 1) & 1)) ?
						 multipart_prefix : message_prefix);
			g_string_append_len (expected, (const char *) leaf_bytes->data, leaf_bytes->len);
			for (i = 0; i < depth; i++)
				if (kind == 0 || (kind == 2 && (i & 1)))
					g_string_append (expected, ending);
			bytes = serialize_writer (root, options, FALSE);
			g_assert_cmpmem (bytes->data, bytes->len, expected->str, expected->len);
			g_byte_array_unref (bytes);
			header_length = strlen (GMIME_IS_MULTIPART (root) ?
				"Content-Type: multipart/mixed; boundary=deep" : "Content-Type: message/rfc822") + 2 * strlen (newline);
			bytes = serialize_writer (root, options, TRUE);
			g_assert_cmpmem (bytes->data, bytes->len, expected->str + header_length, expected->len - header_length);
			g_byte_array_unref (bytes);
			output->limit = expected->len / 2;
			errno = 0;
			g_assert_cmpint (g_mime_entity_write_to_stream (root, options, (GMimeStream *) output), ==, -1);
			g_assert_cmpint (errno, ==, EIO);
			for (i = 0; i < entities->len; i++)
				g_assert_cmpint (((GMimeEntity *) entities->pdata[i])->ensure_newline, ==, flags->data[i]);

			/* Keep each descendant alive until its parent has been released:
			 * model finalizers, unlike the writer, still recurse. */
			for (i = entities->len; i > 0; i--)
				g_object_unref (entities->pdata[i - 1]);
			g_ptr_array_unref (entities);
			g_byte_array_unref (flags);
			g_byte_array_unref (leaf_bytes);
			g_object_unref (output);
			g_mime_format_options_free (options);
			g_string_free (expected, TRUE);
			g_free (multipart_prefix);
			g_free (message_prefix);
			g_free (ending);
		}
	}
	testsuite_check_passed ();
}

typedef struct {
	GMimePart parent_object;
	GMimeEntity *nested;
	guint calls;
	gboolean content_only, international, hidden, fail;
} ModelWriterPart;

typedef GMimePartClass ModelWriterPartClass;

G_DEFINE_TYPE (ModelWriterPart, model_writer_part, GMIME_TYPE_PART)

static ssize_t
model_writer_part_write (GMimeEntity *entity, GMimeFormatOptions *options, gboolean content_only, GMimeStream *stream)
{
	ModelWriterPart *part = (ModelWriterPart *) entity;

	part->calls++;
	part->content_only = content_only;
	part->international = g_mime_format_options_get_allow_international (options);
	part->hidden = g_mime_format_options_is_hidden_header (options, "X-Test");
	if (part->fail) {
		errno = EACCES;
		return -1;
	}
	if (part->nested) {
		GByteArray *bytes = serialize_writer (part->nested, options, FALSE);

		g_byte_array_unref (bytes);
	}
	return GMIME_ENTITY_CLASS (model_writer_part_parent_class)->write_to_stream (entity, options, content_only, stream);
}

static void
model_writer_part_class_init (ModelWriterPartClass *klass)
{
	GMIME_ENTITY_CLASS (klass)->write_to_stream = model_writer_part_write;
}

static void
model_writer_part_init (ModelWriterPart *part)
{
}

static void
test_writer_dispatch (void)
{
	GMimeFormatOptions *options = g_mime_format_options_new ();
	GMimeMultipart *multipart = g_mime_multipart_new_with_subtype ("signed");
	ModelWriterPart *part = g_object_new (model_writer_part_get_type (), NULL);
	ModelWriterPart *nested = g_object_new (model_writer_part_get_type (), NULL);
	GMimeStream *stream = g_mime_stream_mem_new_with_buffer ("body", 4);
	GMimeContent *content = g_mime_content_new_with_stream (stream, GMIME_CONTENT_ENCODING_UNSPECIFIED);
	GMimeContentType *type = g_mime_content_type_new ("text", "plain");
	GMimeMessage *message = g_mime_message_new (FALSE);
	GByteArray *bytes;
	ModelOutput *output;
	guint calls;

	testsuite_check ("Custom writers retain dispatch, parent chaining, formatting scope and reentrant writes");
	g_mime_entity_set_content_type ((GMimeEntity *) part, type);
	g_mime_entity_set_content_type ((GMimeEntity *) nested, type);
	g_mime_part_set_content ((GMimePart *) part, content);
	g_mime_part_set_content ((GMimePart *) nested, content);
	g_mime_multipart_set_boundary (multipart, "signed");
	g_mime_multipart_add (multipart, (GMimeEntity *) part);
	g_mime_format_options_set_allow_international (options, TRUE);
	g_mime_format_options_add_hidden_header (options, "X-Test");
	g_mime_format_options_set_newline_format (options, GMIME_NEWLINE_FORMAT_UNIX);
	part->nested = (GMimeEntity *) nested;

	bytes = serialize_writer ((GMimeEntity *) part, options, FALSE);
	g_assert_cmpuint (part->calls, ==, 1);
	g_assert_cmpuint (nested->calls, ==, 1);
	g_assert_true (part->international);
	g_assert_true (part->hidden);
	g_assert_false (part->content_only);
	g_byte_array_unref (bytes);
	bytes = serialize_writer ((GMimeEntity *) multipart, options, FALSE);
	g_assert_cmpuint (part->calls, ==, 2);
	g_assert_cmpuint (nested->calls, ==, 2);
	g_assert_false (part->international);
	g_assert_false (part->hidden);
	g_assert_false (nested->international);
	g_assert_false (nested->hidden);
	g_assert_true (g_mime_format_options_get_allow_international (options));
	g_assert_true (g_mime_format_options_is_hidden_header (options, "X-Test"));
	g_byte_array_unref (bytes);

	output = g_object_new (model_output_get_type (), NULL);
	part->fail = TRUE;
	errno = 0;
	g_assert_cmpint (g_mime_entity_write_to_stream ((GMimeEntity *) multipart, options, (GMimeStream *) output), ==, -1);
	g_assert_cmpint (errno, ==, EACCES);
	part->fail = FALSE;
	bytes = serialize_writer ((GMimeEntity *) part, options, FALSE);
	g_assert_true (part->international);
	g_assert_true (part->hidden);
	g_byte_array_unref (bytes);
	testsuite_check_passed ();

	testsuite_check ("Message writing passes newline requirements without mutating its body");
	g_mime_message_set_mime_part (message, (GMimeEntity *) part);
	((GMimeEntity *) message)->ensure_newline = FALSE;
	((GMimeEntity *) part)->ensure_newline = TRUE;
	calls = part->calls;
	bytes = serialize_writer ((GMimeEntity *) message, options, TRUE);
	g_assert_cmpmem (bytes->data, bytes->len, "body", 4);
	g_assert_cmpuint (part->calls, ==, calls + 1);
	g_assert_true (part->content_only);
	g_assert_true (((GMimeEntity *) part)->ensure_newline);
	g_byte_array_unref (bytes);
	bytes = serialize_writer ((GMimeEntity *) part, options, TRUE);
	g_assert_cmpmem (bytes->data, bytes->len, "body\n", 5);
	g_byte_array_unref (bytes);
	output->written = 0;
	output->limit = 0;
	errno = 0;
	g_assert_cmpint (g_mime_entity_write_content_to_stream ((GMimeEntity *) message, options, (GMimeStream *) output), ==, -1);
	g_assert_cmpint (errno, ==, EIO);
	g_assert_true (((GMimeEntity *) part)->ensure_newline);
	g_assert_false (((GMimeEntity *) message)->ensure_newline);
	testsuite_check_passed ();

	testsuite_check ("Legacy multipart writing retains text and does not invent a removed boundary");
	g_mime_multipart_clear (multipart);
	g_assert_true (g_mime_param_list_remove (g_mime_content_type_get_parameters (
		((GMimeEntity *) multipart)->content_type), "boundary"));
	g_mime_multipart_set_prologue (multipart, "pre");
	g_mime_multipart_set_epilogue (multipart, "post");
	bytes = serialize_writer ((GMimeEntity *) multipart, options, TRUE);
	g_assert_cmpmem (bytes->data, bytes->len, "pre\npost", 8);
	g_assert_null (g_mime_entity_get_content_type_parameter ((GMimeEntity *) multipart, "boundary"));
	g_byte_array_unref (bytes);
	testsuite_check_passed ();

	g_object_unref (output);
	g_object_unref (message);
	g_object_unref (multipart);
	g_object_unref (part);
	g_object_unref (nested);
	g_object_unref (type);
	g_object_unref (content);
	g_object_unref (stream);
	g_mime_format_options_free (options);
}

int
main (int argc, char **argv)
{
	g_mime_init ();
	testsuite_init (argc, argv);
	testsuite_start ("Reader-backed MIME model foundation");
	test_header_models ();
	test_empty_headers ();
	test_leaf_models ();
	test_binary_encoding_headers ();
	test_model_metadata ();
	test_persistent_resume ();
	test_model_stream_replacement ();
	test_model_type_registration ();
	test_message_models ();
	test_message_depth ();
	test_message_model_errors ();
	test_mailbox_models ();
	test_mailbox_model_errors ();
	test_multipart_models ();
	test_multipart_output ();
	test_model_errors ();
	test_public_parser_models ();
	test_public_parser_warnings ();
	test_public_long_header_names ();
	test_signature_markers ();
	test_writer_depth ();
	test_writer_dispatch ();
	testsuite_end ();
	g_mime_shutdown ();

	return testsuite_exit ();
}
