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
#include <gmime/gmime-object-private.h>
#endif

#include <errno.h>
#include <string.h>
#include <gmime/gmime.h>
#include <gmime/gmime-reader-private.h>

#include "testsuite.h"

typedef struct {
	TestStreamOneByte parent_instance;
	gsize chunk, bytes_read, fail_after;
	guint reads, resets, closes;
	gboolean nonseekable, unset_errno, zero_read;
	gint64 origin;
} ReaderInput;

typedef TestStreamOneByteClass ReaderInputClass;

G_DEFINE_TYPE (ReaderInput, reader_input, TEST_TYPE_STREAM_ONEBYTE)

static ssize_t
reader_input_read (GMimeStream *stream, char *buffer, size_t length)
{
	ReaderInput *input = (ReaderInput *) stream;
	ssize_t nread;

	input->reads++;
	if (input->zero_read)
		return 0;
	if (input->bytes_read == input->fail_after) {
		errno = input->unset_errno ? 0 : EIO;
		return -1;
	}
	length = MIN (length, MIN (input->chunk, input->fail_after - input->bytes_read));
	nread = g_mime_stream_read (input->parent_instance.source, buffer, length);
	if (nread > 0)
		input->bytes_read += nread;
	return nread;
}

static gint64
reader_input_tell (GMimeStream *stream)
{
	ReaderInput *input = (ReaderInput *) stream;

	if (input->nonseekable) {
		errno = ESPIPE;
		return -1;
	}
	return input->origin + g_mime_stream_tell (input->parent_instance.source);
}

static int
reader_input_reset (GMimeStream *stream)
{
	((ReaderInput *) stream)->resets++;
	return g_mime_stream_reset (((ReaderInput *) stream)->parent_instance.source);
}

static int
reader_input_close (GMimeStream *stream)
{
	((ReaderInput *) stream)->closes++;
	return 0;
}

static void
reader_input_class_init (ReaderInputClass *klass)
{
	GMimeStreamClass *stream_class = GMIME_STREAM_CLASS (klass);

	stream_class->read = reader_input_read;
	stream_class->tell = reader_input_tell;
	stream_class->reset = reader_input_reset;
	stream_class->close = reader_input_close;
}

static void
reader_input_init (ReaderInput *input)
{
	input->chunk = G_MAXSIZE;
	input->fail_after = G_MAXSIZE;
}

static ReaderInput *
new_input (const char *data, gsize length, gsize chunk)
{
	ReaderInput *input = g_object_new (reader_input_get_type (), NULL);

	input->parent_instance.source = g_mime_stream_mem_new_with_buffer (data, length);
	input->chunk = chunk;
	return input;
}

typedef struct {
	GMimeReader parent_instance;
	GString *trace;
	GByteArray *content, *boundary;
	guint entities, max_entities, content_chunks;
	gsize max_chunk;
	guint stage, fail_stage, release_stage;
	gboolean no_error, error_on_success, release;
	GMimeReaderNewLineFormat newline_format;
	gboolean trace_newlines;
} TestReader;

typedef GMimeReaderClass TestReaderClass;

G_DEFINE_TYPE (TestReader, test_reader, GMIME_TYPE_READER)

static gboolean
record_callback (TestReader *reader, GError **error)
{
	reader->stage++;
	if (reader->release || reader->stage == reader->release_stage) {
		reader->release = FALSE;
		g_object_unref (reader);
	}
	if (reader->stage != reader->fail_stage)
		return TRUE;
	if (!reader->no_error)
		g_set_error_literal (error, GMIME_ERROR, GMIME_ERROR_NOT_SUPPORTED, "Callback stopped.");
	return reader->error_on_success;
}

static gboolean
test_headers_begin (GMimeReader *reader, gint64 offset, guint64 line, GError **error)
{
	TestReader *test = (TestReader *) reader;

	g_string_append_printf (test->trace, "B:%" G_GINT64_FORMAT ":%" G_GUINT64_FORMAT ";", offset, line);
	return record_callback (test, error);
}

static gboolean
test_header_read (GMimeReader *reader, const guint8 *buffer, gsize length,
		  gsize name_length, gsize field_length, gboolean invalid,
		  gint64 offset, guint64 line, GError **error)
{
	TestReader *test = (TestReader *) reader;
	char *encoded = g_base64_encode (buffer, length);

	g_string_append_printf (test->trace, "H:%" G_GINT64_FORMAT ":%" G_GUINT64_FORMAT ":%"
				G_GSIZE_FORMAT ":%" G_GSIZE_FORMAT ":%d:%s;",
				offset, line, name_length, field_length, invalid, encoded);
	g_free (encoded);
	return record_callback (test, error);
}

static gboolean
test_headers_end (GMimeReader *reader, gint64 begin, guint64 begin_line,
		  gint64 end, guint64 end_line, GError **error)
{
	TestReader *test = (TestReader *) reader;

	g_string_append_printf (test->trace, "E:%" G_GINT64_FORMAT ":%" G_GUINT64_FORMAT ":%"
				G_GINT64_FORMAT ":%" G_GUINT64_FORMAT ";", begin, begin_line, end, end_line);
	return record_callback (test, error);
}

static gboolean
test_body_separator (GMimeReader *reader, gint64 begin, guint64 line, gint64 end, GError **error)
{
	TestReader *test = (TestReader *) reader;

	g_string_append_printf (test->trace, "S:%" G_GINT64_FORMAT ":%" G_GUINT64_FORMAT ":%"
				G_GINT64_FORMAT ";", begin, line, end);
	return record_callback (test, error);
}

static void
test_reader_finalize (GObject *object)
{
	g_string_free (((TestReader *) object)->trace, TRUE);
	g_byte_array_unref (((TestReader *) object)->content);
	g_byte_array_unref (((TestReader *) object)->boundary);
	G_OBJECT_CLASS (test_reader_parent_class)->finalize (object);
}

static gboolean
test_entity_begin (GMimeReader *reader, GMimeReaderEntityType type, GMimeContentType *content_type,
		   gint64 begin, guint64 line, GError **error)
{
	TestReader *test = (TestReader *) reader;
	char *mime = g_mime_content_type_get_mime_type (content_type);

	test->entities++;
	test->max_entities = MAX (test->max_entities, test->entities);
	g_string_append_printf (test->trace, "I:%d:%s:%" G_GINT64_FORMAT ":%" G_GUINT64_FORMAT ";", type, mime, begin, line);
	g_free (mime);
	return record_callback (test, error);
}

static gboolean
test_entity_end (GMimeReader *reader, GMimeReaderEntityType type, GMimeContentType *content_type,
		 gint64 begin, guint64 line, gint64 headers_end, gint64 end, guint64 lines, GError **error)
{
	TestReader *test = (TestReader *) reader;

	test->entities--;
	g_string_append_printf (test->trace, "O:%d:%" G_GINT64_FORMAT ":%" G_GUINT64_FORMAT ":%"
				G_GINT64_FORMAT ":%" G_GINT64_FORMAT ":%" G_GUINT64_FORMAT ";",
				type, begin, line, headers_end, end, lines);
	return record_callback (test, error);
}

static gboolean
test_content_begin (GMimeReader *reader, GMimeReaderContentType type, gint64 begin, guint64 line, GError **error)
{
	TestReader *test = (TestReader *) reader;

	g_byte_array_set_size (test->content, 0);
	g_string_append_printf (test->trace, "C:%d:%" G_GINT64_FORMAT ":%" G_GUINT64_FORMAT ";", type, begin, line);
	return record_callback (test, error);
}

static gboolean
test_content_read (GMimeReader *reader, GMimeReaderContentType type, const guint8 *buffer, gsize length, GError **error)
{
	TestReader *test = (TestReader *) reader;

	g_byte_array_append (test->content, buffer, length);
	test->content_chunks++;
	test->max_chunk = MAX (test->max_chunk, length);
	return record_callback (test, error);
}

static gboolean
test_content_end (GMimeReader *reader, GMimeReaderContentType type, gint64 begin, guint64 line,
		  gint64 end, guint64 lines, GMimeReaderNewLineFormat newline_format, GError **error)
{
	TestReader *test = (TestReader *) reader;
	char *encoded = g_base64_encode (test->content->data, test->content->len);

	g_string_append_printf (test->trace, "D:%d:%" G_GINT64_FORMAT ":%" G_GUINT64_FORMAT ":%"
				G_GINT64_FORMAT ":%" G_GUINT64_FORMAT ":%s;", type, begin, line, end, lines, encoded);
	g_free (encoded);
	if (type == GMIME_READER_CONTENT_PART)
		test->newline_format = newline_format;
	else
		g_assert_cmpint (newline_format, ==, GMIME_READER_NEWLINE_UNKNOWN);
	if (test->trace_newlines)
		g_string_append_printf (test->trace, "F:%d:%d;", type, newline_format);
	return record_callback (test, error);
}

static gboolean
test_boundary_begin (GMimeReader *reader, gboolean final, gint64 begin, guint64 line, GError **error)
{
	TestReader *test = (TestReader *) reader;

	g_byte_array_set_size (test->boundary, 0);
	g_string_append_printf (test->trace, "J:%d:%" G_GINT64_FORMAT ":%" G_GUINT64_FORMAT ";", final, begin, line);
	return record_callback (test, error);
}

static gboolean
test_boundary_read (GMimeReader *reader, gboolean final, const guint8 *buffer, gsize length,
		    gint64 begin, guint64 line, GError **error)
{
	TestReader *test = (TestReader *) reader;

	g_byte_array_append (test->boundary, buffer, length);
	return record_callback (test, error);
}

static gboolean
test_boundary_end (GMimeReader *reader, gboolean final, gint64 begin, guint64 line, gint64 end, GError **error)
{
	TestReader *test = (TestReader *) reader;
	char *encoded = g_base64_encode (test->boundary->data, test->boundary->len);

	g_string_append_printf (test->trace, "K:%d:%" G_GINT64_FORMAT ":%" G_GUINT64_FORMAT ":%"
				G_GINT64_FORMAT ":%s;", final, begin, line, end, encoded);
	g_free (encoded);
	return record_callback (test, error);
}

static gboolean
test_message_begin (GMimeReader *reader, gint64 begin, guint64 line, GError **error)
{
	TestReader *test = (TestReader *) reader;

	g_string_append_printf (test->trace, "M:%" G_GINT64_FORMAT ":%" G_GUINT64_FORMAT ";", begin, line);
	return record_callback (test, error);
}

static gboolean
test_message_end (GMimeReader *reader, gint64 begin, guint64 line, gint64 headers_end,
		  gint64 end, guint64 lines, GError **error)
{
	TestReader *test = (TestReader *) reader;

	g_string_append_printf (test->trace, "N:%" G_GINT64_FORMAT ":%" G_GUINT64_FORMAT ":%"
				G_GINT64_FORMAT ":%" G_GINT64_FORMAT ":%" G_GUINT64_FORMAT ";",
				begin, line, headers_end, end, lines);
	return record_callback (test, error);
}

static gboolean
test_marker_begin (GMimeReader *reader, gint64 begin, guint64 line, GError **error)
{
	TestReader *test = (TestReader *) reader;

	g_byte_array_set_size (test->boundary, 0);
	g_string_append_printf (test->trace, "V:%" G_GINT64_FORMAT ":%" G_GUINT64_FORMAT ";", begin, line);
	return record_callback (test, error);
}

static gboolean
test_marker_read (GMimeReader *reader, const guint8 *buffer, gsize length, GError **error)
{
	TestReader *test = (TestReader *) reader;

	test->max_chunk = MAX (test->max_chunk, length);
	g_byte_array_append (test->boundary, buffer, length);
	return record_callback (test, error);
}

static gboolean
test_marker_end (GMimeReader *reader, gint64 begin, guint64 line, gint64 end, GError **error)
{
	TestReader *test = (TestReader *) reader;
	char *encoded = g_base64_encode (test->boundary->data, test->boundary->len);

	g_string_append_printf (test->trace, "W:%" G_GINT64_FORMAT ":%" G_GUINT64_FORMAT ":%"
				G_GINT64_FORMAT ":%s;", begin, line, end, encoded);
	g_free (encoded);
	return record_callback (test, error);
}

static void
test_reader_class_init (TestReaderClass *klass)
{
	G_OBJECT_CLASS (klass)->finalize = test_reader_finalize;
	klass->headers_begin = test_headers_begin;
	klass->header_read = test_header_read;
	klass->headers_end = test_headers_end;
	klass->body_separator = test_body_separator;
	klass->entity_begin = test_entity_begin;
	klass->entity_end = test_entity_end;
	klass->content_begin = test_content_begin;
	klass->content_read = test_content_read;
	klass->content_end = test_content_end;
	klass->boundary_begin = test_boundary_begin;
	klass->boundary_read = test_boundary_read;
	klass->boundary_end = test_boundary_end;
	klass->message_begin = test_message_begin;
	klass->message_end = test_message_end;
	klass->marker_begin = test_marker_begin;
	klass->marker_read = test_marker_read;
	klass->marker_end = test_marker_end;
}

static void
test_reader_init (TestReader *reader)
{
	reader->trace = g_string_new (NULL);
	reader->content = g_byte_array_new ();
	reader->boundary = g_byte_array_new ();
}

static TestReader *
new_reader (ReaderInput *input, GMimeParserOptions *options)
{
	TestReader *reader = g_object_new (test_reader_get_type (), NULL);

	g_mime_reader_set_options ((GMimeReader *) reader, options);
	g_mime_reader_set_stream ((GMimeReader *) reader, (GMimeStream *) input);
	return reader;
}

static void
test_headers (void)
{
	static const gsize chunks[] = { 1, 2, 3, 5, 7, 127, 128, 129, 4095, 4096, 4097, G_MAXSIZE };
	static const char wire[] = "X \t: a\r\n\tb\nBad header\n\nbody";
	GMimeParserOptions *options = g_mime_parser_options_new ();
	char *first = g_base64_encode ((const guint8 *) "X \t: a\r\n\tb\n", 11);
	char *second = g_base64_encode ((const guint8 *) "Bad header\n", 11);
	char *expected = g_strdup_printf ("B:0:1;H:0:1:1:3:0:%s;H:11:3:11:11:1:%s;S:22:4:23;E:0:1:23:5;",
					  first, second);
	guint i;

	for (i = 0; i < G_N_ELEMENTS (chunks); i++) {
		ReaderInput *input = new_input (wire, sizeof (wire) - 1, chunks[i]);
		TestReader *reader = new_reader (input, options);
		GError *error = NULL;

		g_assert_true (g_mime_reader_read_headers ((GMimeReader *) reader, &error));
		g_assert_no_error (error);
		g_assert_cmpstr (reader->trace->str, ==, expected);
		g_assert_cmpint (g_mime_reader_tell ((GMimeReader *) reader), ==, 23);
		g_assert_false (g_mime_reader_eos ((GMimeReader *) reader));
		g_assert_cmpuint (input->resets, ==, 0);
		g_assert_cmpuint (input->closes, ==, 0);
		g_object_unref (reader);
		g_object_unref (input);
	}
	g_free (first);
	g_free (second);
	g_free (expected);
	g_mime_parser_options_free (options);
}

static void
test_reader_edge_cases (void)
{
	static const struct {
		const char *wire;
		gsize length;
		gboolean success;
		guint callbacks;
	} cases[] = {
		{ "", 0, TRUE, 1 },
		{ "\n", 1, TRUE, 3 },
		{ "\r", 1, TRUE, 3 },
		{ "\r\n", 2, TRUE, 3 },
		{ "X: v", 4, TRUE, 3 },
		{ "X: v\n", 5, TRUE, 3 },
		{ "garbage", 7, FALSE, 1 },
		{ "garbage\n", 8, FALSE, 1 },
		{ "From sender\nX: v\n\n", 18, FALSE, 2 },
		{ "\xef\xbb\xbfX: v\n\n", 9, FALSE, 1 },
		{ "X\xff: v\n\n", 7, FALSE, 1 },
		{ "X: a\0b\n\n", 8, TRUE, 4 },
		{ "X: a\n\xff: b\n\n", 11, TRUE, 5 },
		{ "X: v\nF\n", 7, TRUE, 4 }
	};
	static const gsize chunks[] = { 1, 2, 3, 127, 128, 4096, G_MAXSIZE };
	guint i, j;

	for (i = 0; i < G_N_ELEMENTS (cases); i++) {
		for (j = 0; j < G_N_ELEMENTS (chunks); j++) {
			ReaderInput *input = new_input (cases[i].wire, cases[i].length, chunks[j]);
			TestReader *reader = new_reader (input, NULL);
			GError *error = NULL;

			g_assert_cmpint (g_mime_reader_read_headers ((GMimeReader *) reader, &error), ==, cases[i].success);
			g_assert_cmpuint (reader->stage, ==, cases[i].callbacks);
			if (cases[i].success)
				g_assert_no_error (error);
			else
				g_assert_error (error, GMIME_ERROR, GMIME_ERROR_PARSE_ERROR);
			if (i == 11)
				g_assert_nonnull (strstr (reader->trace->str, "H:0:1:1:1:0:WDogYQBiCg==;"));
			if (i == 12)
				g_assert_nonnull (strstr (reader->trace->str, "H:5:2:5:5:1:/zogYgo=;"));
			g_clear_error (&error);
			g_object_unref (reader);
			g_object_unref (input);
		}
	}

	for (i = 4094; i <= 4226; i++) {
		GString *wire = g_string_new ("X: ");

		for (j = 0; j < i; j++)
			g_string_append_c (wire, 'a');
		g_string_append (wire, "\r\n\tfold\r\n\r\nbody");
		for (j = 0; j < G_N_ELEMENTS (chunks); j++) {
			ReaderInput *input = new_input (wire->str, wire->len, chunks[j]);
			TestReader *reader = new_reader (input, NULL);
			GError *error = NULL;

			g_assert_true (g_mime_reader_read_headers ((GMimeReader *) reader, &error));
			g_assert_no_error (error);
			g_assert_cmpuint (reader->stage, ==, 4);
			g_assert_cmpint (g_mime_reader_tell ((GMimeReader *) reader), ==, wire->len - 4);
			g_object_unref (reader);
			g_object_unref (input);
		}
		g_string_free (wire, TRUE);
	}
}

static void
test_reader_limits_and_errors (void)
{
	static const char wire[] = "X: a\r\n\tb\r\n\r\nbody";
	GMimeParserOptions *options = g_mime_parser_options_new ();
	guint limit, stage, missing_errno;

	for (limit = 1; limit <= 12; limit++) {
		ReaderInput *input = new_input (wire, sizeof (wire) - 1, 1);
		TestReader *reader;
		GError *error = NULL;
		gboolean result;

		g_mime_parser_options_set_max_header_length (options, limit);
		reader = new_reader (input, options);
		/* Changing the original options must not change the reader's copy. */
		g_mime_parser_options_set_max_header_length (options, 1);
		result = g_mime_reader_read_headers ((GMimeReader *) reader, &error);
		g_assert_cmpint (result, ==, limit >= 10);
		if (result) {
			g_assert_no_error (error);
		} else {
			g_assert_error (error, GMIME_ERROR, GMIME_ERROR_PARSE_ERROR);
			g_clear_error (&error);
		}
		g_object_unref (reader);
		g_object_unref (input);
	}
	g_mime_parser_options_free (options);

	for (stage = 1; stage <= 4; stage++) {
		guint mode;

		for (mode = 0; mode < 3; mode++) {
			ReaderInput *input = new_input (wire, sizeof (wire) - 1, 1);
			TestReader *reader = new_reader (input, NULL);
			GError *error = NULL;
			guint reads;

			reader->fail_stage = stage;
			reader->no_error = mode == 1;
			reader->error_on_success = mode == 2;
			g_assert_false (g_mime_reader_read_headers ((GMimeReader *) reader, &error));
			g_assert_error (error, GMIME_ERROR, (mode == 1 ? GMIME_ERROR_GENERAL : GMIME_ERROR_NOT_SUPPORTED));
			g_clear_error (&error);
			reads = input->reads;
			g_assert_false (g_mime_reader_read_headers ((GMimeReader *) reader, &error));
			g_assert_error (error, GMIME_ERROR, (mode == 1 ? GMIME_ERROR_GENERAL : GMIME_ERROR_NOT_SUPPORTED));
			g_assert_cmpuint (input->reads, ==, reads);
			g_assert_cmpuint (reader->stage, ==, stage);
			g_clear_error (&error);
			g_object_unref (reader);
			g_object_unref (input);
		}
	}

	for (missing_errno = 0; missing_errno < 2; missing_errno++) {
		gsize offset;

		for (offset = 0; offset < 12; offset++) {
			ReaderInput *input = new_input (wire, sizeof (wire) - 1, 1);
			TestReader *reader = new_reader (input, NULL);
			GError *error = NULL;

			input->fail_after = offset;
			input->unset_errno = missing_errno;
			g_assert_false (g_mime_reader_read_headers ((GMimeReader *) reader, &error));
			g_assert_error (error, GMIME_ERROR, (missing_errno ? GMIME_ERROR_GENERAL : EIO));
			g_clear_error (&error);
			g_object_unref (reader);
			g_object_unref (input);
		}
	}
}

static void
test_reader_state (void)
{
	const char *wire = "X: a\n\nY: b\n\n";
	ReaderInput *input = new_input (wire, strlen (wire), G_MAXSIZE);
	TestReader *reader = new_reader (input, NULL);
	GError *error = NULL;
	GMimeReader *weak;
	guint reads;

	g_assert_true (g_mime_reader_read_headers ((GMimeReader *) reader, &error));
	reads = input->reads;
	g_assert_true (g_mime_reader_read_headers ((GMimeReader *) reader, &error));
	g_assert_no_error (error);
	/* The second block is buffered; its read-ahead only probes EOF. */
	g_assert_cmpuint (input->reads, ==, reads + 1);
	g_assert_cmpint (g_mime_reader_tell ((GMimeReader *) reader), ==, 12);
	g_assert_nonnull (strstr (reader->trace->str, "B:6:3;"));
	g_assert_true (g_mime_reader_eos ((GMimeReader *) reader));
	g_assert_true (g_mime_reader_read_headers ((GMimeReader *) reader, &error));
	g_assert_true (g_mime_reader_eos ((GMimeReader *) reader));
	g_assert_true (g_str_has_suffix (reader->trace->str, "B:12:5;"));

	/* Reinitialization clears sticky failures and starts a new line origin. */
	g_mime_reader_set_stream ((GMimeReader *) reader, (GMimeStream *) input);
	g_string_truncate (reader->trace, 0);
	g_assert_true (g_mime_reader_read_headers ((GMimeReader *) reader, &error));
	g_assert_cmpstr (reader->trace->str, ==, "B:12:1;");
	g_object_unref (reader);
	g_object_unref (input);

	input = new_input ("garbage", 7, 1);
	reader = new_reader (input, NULL);
	g_assert_false (g_mime_reader_read_headers ((GMimeReader *) reader, &error));
	g_clear_error (&error);
	g_object_unref (input);
	input = new_input ("X: a\n", 5, 1);
	input->nonseekable = TRUE;
	g_mime_reader_set_stream ((GMimeReader *) reader, (GMimeStream *) input);
	g_assert_true (g_mime_reader_read_headers ((GMimeReader *) reader, &error));
	g_assert_no_error (error);
	g_assert_cmpint (g_mime_reader_tell ((GMimeReader *) reader), ==, 5);
	g_assert_true (g_mime_reader_eos ((GMimeReader *) reader));
	g_object_unref (reader);
	g_object_unref (input);

	input = new_input ("\n", 1, 1);
	input->origin = 100;
	reader = new_reader (input, NULL);
	g_assert_true (g_mime_reader_read_headers ((GMimeReader *) reader, &error));
	g_assert_cmpstr (reader->trace->str, ==, "B:100:1;S:100:1:101;E:100:1:101:2;");
	g_object_unref (reader);

	input->origin = G_MAXINT64;
	/* Reset only the test source, never the reader's stream. */
	g_mime_stream_reset (input->parent_instance.source);
	reader = new_reader (input, NULL);
	g_assert_false (g_mime_reader_read_headers ((GMimeReader *) reader, &error));
	g_assert_error (error, GMIME_ERROR, GMIME_ERROR_PARSE_ERROR);
	g_clear_error (&error);
	g_object_unref (reader);
	g_object_unref (input);

	input = new_input (wire, strlen (wire), 1);
	reader = new_reader (input, NULL);
	weak = (GMimeReader *) reader;
	g_object_add_weak_pointer (G_OBJECT (reader), (gpointer *) &weak);
	reader->release = TRUE;
	g_assert_true (g_mime_reader_read_headers ((GMimeReader *) reader, &error));
	g_assert_no_error (error);
	g_assert_null (weak);
	g_object_unref (input);

	input = new_input (wire, strlen (wire), 1);
	input->zero_read = TRUE;
	reader = new_reader (input, NULL);
	g_assert_true (g_mime_reader_read_headers ((GMimeReader *) reader, &error));
	g_assert_cmpstr (reader->trace->str, ==, "B:0:1;");
	g_assert_true (g_mime_reader_eos ((GMimeReader *) reader));
	g_assert_cmpuint (input->reads, ==, 1);
	g_object_unref (reader);
	g_object_unref (input);
}

static void
test_reader_default_callbacks (void)
{
	static const char wire[] = "X: a\r\n\tb\r\n\r\n";
	GMimeParserOptions *options = g_mime_parser_options_new ();
	ReaderInput *input = new_input (wire, sizeof (wire) - 1, 1);
	GMimeReader *reader;
	GError *error = NULL;

	g_mime_parser_options_set_detect_openpgp (options, TRUE);
	reader = g_mime_reader_new (options, (GMimeStream *) input);
	g_mime_parser_options_set_detect_openpgp (options, FALSE);
	g_assert_true (g_mime_parser_options_get_detect_openpgp (g_mime_reader_get_options (reader)));
	g_assert_true (g_mime_reader_read_headers (reader, &error));
	g_assert_no_error (error);
	g_assert_cmpint (g_mime_reader_tell (reader), ==, sizeof (wire) - 1);
	g_assert_false (g_mime_reader_eos (reader));
	g_assert_true (g_mime_reader_read_headers (reader, &error));
	g_assert_no_error (error);
	g_assert_true (g_mime_reader_eos (reader));
	g_assert_cmpuint (input->resets, ==, 0);
	g_assert_cmpuint (input->closes, ==, 0);
	g_mime_reader_set_options (reader, g_mime_reader_get_options (reader));
	g_assert_true (g_mime_parser_options_get_detect_openpgp (g_mime_reader_get_options (reader)));
	g_mime_reader_set_options (reader, NULL);
	g_assert_false (g_mime_parser_options_get_detect_openpgp (g_mime_reader_get_options (reader)));
	g_object_unref (reader);
	g_assert_cmpuint (input->closes, ==, 0);
	g_object_unref (input);
	g_mime_parser_options_free (options);
}

static void
test_openpgp_option (void)
{
	static const struct {
		const char *armor;
		GMimeOpenPGPData type;
	} cases[] = {
		{ "-----BEGIN PGP MESSAGE-----\nabc\n-----END PGP MESSAGE-----\n", GMIME_OPENPGP_DATA_ENCRYPTED },
		{ "-----BEGIN PGP SIGNED MESSAGE-----\nabc\n-----BEGIN PGP SIGNATURE-----\nabc\n-----END PGP SIGNATURE-----\n", GMIME_OPENPGP_DATA_SIGNED },
		{ "-----BEGIN PGP PUBLIC KEY BLOCK-----\nabc\n-----END PGP PUBLIC KEY BLOCK-----\n", GMIME_OPENPGP_DATA_PUBLIC_KEY },
		{ "-----BEGIN PGP PRIVATE KEY BLOCK-----\nabc\n-----END PGP PRIVATE KEY BLOCK-----\n", GMIME_OPENPGP_DATA_PRIVATE_KEY },
		{ "-----BEGIN PGP MESSAGE-----\nabc\n", GMIME_OPENPGP_DATA_NONE },
		{ "-----END PGP MESSAGE-----\n", GMIME_OPENPGP_DATA_NONE },
		{ "prefix -----BEGIN PGP MESSAGE-----\nabc\n-----END PGP MESSAGE-----\n", GMIME_OPENPGP_DATA_NONE },
		{ "ordinary content\n", GMIME_OPENPGP_DATA_NONE }
	};
	GMimeParserOptions *options = g_mime_parser_options_new ();
	GMimeParserOptions *clone;
	guint i, enabled, encoded, construct;

	g_assert_false (g_mime_parser_options_get_detect_openpgp (NULL));
	g_assert_false (g_mime_parser_options_get_detect_openpgp (options));
	g_mime_parser_options_set_detect_openpgp (options, TRUE);
	clone = g_mime_parser_options_clone (options);
	g_mime_parser_options_set_detect_openpgp (options, FALSE);
	g_assert_true (g_mime_parser_options_get_detect_openpgp (clone));
	g_mime_parser_options_free (clone);

	for (i = 0; i < G_N_ELEMENTS (cases); i++) {
		for (enabled = 0; enabled < 2; enabled++) {
			for (encoded = 0; encoded < 2; encoded++) {
				for (construct = 0; construct < 4; construct++) {
					GMimePartPrivate *priv;
					char *body = encoded ? g_base64_encode ((const guint8 *) cases[i].armor, strlen (cases[i].armor)) : g_strdup (cases[i].armor);
					char *wire = g_strdup_printf ("%sContent-Type: text/plain\nContent-Transfer-Encoding: %s\n\n%s%s",
								      construct == 2 ? "From sender Sat Jan  1 00:00:00 2000\n" :
								      construct == 3 ? "\1\1\1\1\n" : "",
								      encoded ? "base64" : "7bit", body,
								      construct == 3 ? "\n\1\1\1\1\n" : "");
					GMimeStream *source = g_mime_stream_mem_new_with_buffer (wire, strlen (wire));
					GMimeStream *onebyte = test_stream_onebyte_new (source);
					GMimeParser *parser = g_mime_parser_new_with_stream (onebyte);
					GMimeEntity *entity;
					GMimePart *part;
					GError *error = NULL;

					g_mime_parser_options_set_detect_openpgp (options, enabled);
					if (construct == 2)
						g_mime_parser_set_format (parser, GMIME_FORMAT_MBOX);
					else if (construct == 3)
						g_mime_parser_set_format (parser, GMIME_FORMAT_MMDF);
					if (construct == 0)
						entity = g_mime_parser_construct_part (parser, options, &error);
					else
						entity = GMIME_ENTITY (g_mime_parser_construct_message (parser, options, &error));
					g_assert_no_error (error);
					g_assert_nonnull (entity);
					part = construct == 0 ? GMIME_PART (entity) : GMIME_PART (g_mime_message_get_mime_part (GMIME_MESSAGE (entity)));
					priv = _g_mime_part_get_private (part);

					if (!enabled || encoded)
						g_assert_cmpint (priv->openpgp, ==, (GMimeOpenPGPData) -1);
					else
						g_assert_cmpint (priv->openpgp, ==, cases[i].type);
					g_assert_cmpint (g_mime_part_get_openpgp_data (part), ==, cases[i].type);
					g_object_unref (entity);
					g_object_unref (parser);
					g_object_unref (onebyte);
					g_object_unref (source);
					g_free (wire);
					g_free (body);
				}
			}
		}
	}
	g_mime_parser_options_free (options);
}

static void
test_openpgp_reuse (void)
{
	const char *armor = "-----BEGIN PGP MESSAGE-----\nabc\n-----END PGP MESSAGE-----\n";
	GString *wire = g_string_new (NULL);
	GMimeParserOptions *options = g_mime_parser_options_new ();
	GMimeStream *stream;
	GMimeParser *parser;
	guint i;

	for (i = 0; i < 5; i++)
		g_string_append_printf (wire, "From sender Sat Jan  1 00:00:00 2000\nContent-Type: text/plain\n\n%s", armor);
	stream = g_mime_stream_mem_new_with_buffer (wire->str, wire->len);
	parser = g_mime_parser_new_with_stream (stream);
	g_mime_parser_set_format (parser, GMIME_FORMAT_MBOX);
	for (i = 0; i < 5; i++) {
		GMimePartPrivate *part_priv;
		GError *error = NULL;
		GMimeMessage *message;
		GMimePart *part;
		gboolean enabled = i == 0 || i == 3;

		g_mime_parser_options_set_detect_openpgp (options, enabled);
		message = g_mime_parser_construct_message (parser, i == 2 ? NULL : options, &error);
		g_assert_no_error (error);
		g_assert_nonnull (message);
		part = GMIME_PART (g_mime_message_get_mime_part (message));
		part_priv = _g_mime_part_get_private (part);

		g_assert_cmpint (part_priv->openpgp, ==, (enabled ? GMIME_OPENPGP_DATA_ENCRYPTED : (GMimeOpenPGPData) -1));
		g_object_unref (message);
	}
	g_object_unref (parser);
	g_object_unref (stream);
	g_string_free (wire, TRUE);

	for (i = 0; i < 2; i++) {
		GMimePartPrivate *first_priv;
		GMimePartPrivate *second_priv;
		GError *error = NULL;
		GMimeEntity *entity;
		GMimeMultipart *multipart;
		GMimePart *first, *second;

		wire = g_string_new ("Content-Type: multipart/mixed; boundary=b\n\n--b\nContent-Type: text/plain\n\n");
		g_string_append (wire, armor);
		g_string_append (wire, "--b\nContent-Type: text/plain\n\nordinary\n--b--\n");
		stream = g_mime_stream_mem_new_with_buffer (wire->str, wire->len);
		parser = g_mime_parser_new_with_stream (stream);
		g_mime_parser_options_set_detect_openpgp (options, i != 0);
		entity = g_mime_parser_construct_part (parser, options, &error);
		g_assert_no_error (error);
		g_assert_true (GMIME_IS_MULTIPART (entity));
		multipart = GMIME_MULTIPART (entity);
		g_assert_cmpint (g_mime_multipart_get_count (multipart), ==, 2);
		first = GMIME_PART (g_mime_multipart_get_part (multipart, 0));
		second = GMIME_PART (g_mime_multipart_get_part (multipart, 1));
		first_priv = _g_mime_part_get_private (first);

		g_assert_cmpint (first_priv->openpgp, ==, (i ? GMIME_OPENPGP_DATA_ENCRYPTED : (GMimeOpenPGPData) -1));
		second_priv = _g_mime_part_get_private (second);

		g_assert_cmpint (second_priv->openpgp, ==, (i ? GMIME_OPENPGP_DATA_NONE : (GMimeOpenPGPData) -1));
		g_object_unref (entity);
		g_object_unref (parser);
		g_object_unref (stream);
		g_string_free (wire, TRUE);
	}
	g_mime_parser_options_free (options);
}

static void
test_entity_traversal (void)
{
	static const char *fixtures[] = {
		"\nhello\n",
		"Content-Type: multipart/mixed; boundary=b\r\n\r\npre\r\n--b \t\r\nX: y\r\n\r\n"
		"abc\r\n--not-b\r\n--b\r\nContent-Type: multipart/alternative; boundary=c\r\n\r\n"
		"--c\r\n\r\nnested\r\n--c--\r\nepi\r\n--b-- \t\r\nouter\r\n",
		"Content-Type: multipart/mixed; boundary=b\n\n--b\nContent-Type: multipart/mixed; boundary=c\n\n"
		"--c\nX: y\n\ntruncated\n--b\nX: z\n\nlast\n--b--",
		"Content-Type: multipart/mixed; boundary=b\n\n--b\nX: y\n--b\n\nsecond\n--b--\n",
		"Content-Type: multipart/mixed\n\nall preamble\n",
		"Content-Type: multipart/mixed; boundary=b\n\n--b--\r\n",
		"Content-Type: multipart/digest; boundary=b\n\n--b\n\nSubject: embedded\n\ntext\n--b--\n",
		"Content-Type: message/rfc822\n\nSubject: embedded\nContent-Type: multipart/mixed; boundary=c\n\n"
		"--c\n\ntext\n--c--\n",
		"Content-Type: message/rfc822\nContent-Transfer-Encoding: base64\n\nYWJjCg==\n",
		"Content-Type: multipart/mixed; boundary=b\n\n--b\nContent-Type: message/rfc822\n\n--b--\n",
		"Content-Type: multipart/mixed; boundary=\"b--\"\n\n--b--\n\none\n--b----\n",
		"Content-Type: multipart/mixed; boundary=b\n\n--b\nBad header\n\nbody\n--b--\n",
		"Content-Type: message/rfc822\n\n",
		"Content-Type: multipart/mixed; boundary=b; broken\n\n--b\n\nbody\n--b--\n",
		"Content-Type: multipart/mixed junk; boundary=b\n\n--b\n\nbody\n--b--\n",
		"Content-Type: multipart/mixed; boundary=\"\"\r\n\r\n--\r\n"
		"Content-Type: message/rfc822\r\n\r\n----\r\n",
		"Content-Type: multipart/digest; boundary=b0\n\n--b0\nX: y\r\n\r\n\r\n"
		"--b0\nX: y\r\n\r\nbody\n--b0\n"
		"Content-Type: multipart/alternative; boundary=shared\n\n--shared\nX: y\n\n\n\n--shared--\nepi\n\n"
	};
	static const gsize chunks[] = { 1, 2, 3, 7, 127, 128, 129, 4095, 4096, 4097, G_MAXSIZE };
	guint i, j;

	for (i = 0; i < G_N_ELEMENTS (fixtures); i++) {
		char *expected = NULL;

		for (j = 0; j < G_N_ELEMENTS (chunks); j++) {
			ReaderInput *input = new_input (fixtures[i], strlen (fixtures[i]), chunks[j]);
			TestReader *reader = new_reader (input, NULL);
			GError *error = NULL;

			g_assert_true (g_mime_reader_read_entity ((GMimeReader *) reader, &error));
			g_assert_no_error (error);
			g_assert_cmpuint (reader->entities, ==, 0);
			g_assert_true (g_mime_reader_eos ((GMimeReader *) reader));
			g_assert_cmpint (g_mime_reader_tell ((GMimeReader *) reader), ==, strlen (fixtures[i]));
			g_assert_cmpuint (input->resets, ==, 0);
			g_assert_cmpuint (input->closes, ==, 0);
			if (!expected)
				expected = g_strdup (reader->trace->str);
			g_assert_cmpstr (reader->trace->str, ==, expected);
			if (i == 0)
				g_assert_nonnull (strstr (expected, "D:0:1:2:7:1:aGVsbG8K;O:0:0:1:1:7:1;"));
			if (i == 1) {
				g_assert_nonnull (strstr (expected, ":YWJjDQotLW5vdC1i;"));
				g_assert_nonnull (strstr (expected, ":bmVzdGVk;"));
				g_assert_nonnull (strstr (expected, ":DQplcGk=;"));
			}
			g_object_unref (reader);
			g_object_unref (input);
		}
		g_free (expected);
	}

	for (i = 0; i < 8; i++) {
		const char *newline = (i & 1) ? "\r\n" : "\n";
		char *boundary = g_strnfill ((i & 2) ? 140 : 1, 'b');
		GString *wire = g_string_new (NULL);
		gsize begin, end;
		char *event, *expected = NULL;

		g_string_append_printf (wire, "Content-Type: multipart/mixed; boundary=%s%s%s--%s%s",
					boundary, newline, newline, boundary, newline);
		begin = wire->len;
		g_string_append_printf (wire, "Content-Type: message/rfc822%s%s", newline, newline);
		end = wire->len - strlen (newline);
		g_string_append_printf (wire, "--%s%s%s", boundary, (i & 4) ? "--" : "", newline);
		if (!(i & 4))
			g_string_append_printf (wire, "%ssibling%s--%s--%s", newline, newline, boundary, newline);
		event = g_strdup_printf ("O:%d:%" G_GSIZE_FORMAT ":4:%" G_GSIZE_FORMAT
					 ":%" G_GSIZE_FORMAT ":0;", GMIME_READER_ENTITY_MESSAGE_PART, begin, end, end);

		for (j = 0; j < G_N_ELEMENTS (chunks); j++) {
			ReaderInput *input = new_input (wire->str, wire->len, chunks[j]);
			TestReader *reader = new_reader (input, NULL);
			GError *error = NULL;

			g_assert_true (g_mime_reader_read_entity ((GMimeReader *) reader, &error));
			g_assert_no_error (error);
			g_assert_cmpuint (reader->max_entities, ==, 2);
			g_assert_nonnull (strstr (reader->trace->str, event));
			g_assert_true (g_mime_reader_eos ((GMimeReader *) reader));
			g_assert_cmpint (g_mime_reader_tell ((GMimeReader *) reader), ==, wire->len);
			if (!expected)
				expected = g_strdup (reader->trace->str);
			g_assert_cmpstr (reader->trace->str, ==, expected);
			g_object_unref (reader);
			g_object_unref (input);
		}
		g_free (expected);
		g_free (event);
		g_string_free (wire, TRUE);
		g_free (boundary);
	}
}

static void
test_binary_transfer_encoding (void)
{
	static const struct {
		const char *value;
		gsize length;
		gboolean encoded;
	} cases[] = {
		{ "base64\0x", sizeof ("base64\0x") - 1, FALSE },
		{ "\0base64", sizeof ("\0base64") - 1, FALSE },
		{ "base64 \0x", sizeof ("base64 \0x") - 1, TRUE },
		{ "base64;\0x", sizeof ("base64;\0x") - 1, TRUE },
		{ "\tbase64", sizeof ("\tbase64") - 1, TRUE },
		{ "base64\tx", sizeof ("base64\tx") - 1, TRUE },
		{ "\vbase64", sizeof ("\vbase64") - 1, FALSE },
		{ "base64\f", sizeof ("base64\f") - 1, FALSE },
		{ "\302\240base64", sizeof ("\302\240base64") - 1, FALSE },
		{ "base64\343\200\200x", sizeof ("base64\343\200\200x") - 1, FALSE },
		{ "base64\302", sizeof ("base64\302") - 1, FALSE },
		{ "base64;\377", sizeof ("base64;\377") - 1, TRUE },
	};
	guint i, j;

	for (i = 0; i < G_N_ELEMENTS (cases); i++) {
		GString *wire = g_string_new ("Content-Type: message/rfc822\nContent-Transfer-Encoding: ");
		char *event = g_strdup_printf ("I:%d:message/rfc822:0:1;",
					      cases[i].encoded ? GMIME_READER_ENTITY_PART : GMIME_READER_ENTITY_MESSAGE_PART);

		g_string_append_len (wire, cases[i].value, cases[i].length);
		g_string_append (wire, "\n\nSubject: embedded\n\nbody\n");
		for (j = 0; j < 2; j++) {
			ReaderInput *input = new_input (wire->str, wire->len, j ? 4096 : 1);
			TestReader *reader = new_reader (input, NULL);
			GError *error = NULL;

			g_assert_true (g_mime_reader_read_entity ((GMimeReader *) reader, &error));
			g_assert_no_error (error);
			g_assert_nonnull (strstr (reader->trace->str, event));
			g_assert_cmpuint (reader->max_entities, ==, cases[i].encoded ? 1 : 2);
			g_assert_cmpint (g_mime_reader_tell ((GMimeReader *) reader), ==, wire->len);
			g_object_unref (reader);
			g_object_unref (input);
		}
		g_free (event);
		g_string_free (wire, TRUE);
	}
}

static void
test_entity_oversized_delimiter (void)
{
	static const gsize chunks[] = { 4095, 4096, 4097, G_MAXSIZE };
	char *boundary = g_strnfill (1001, 'b');
	guint i, newline;

	for (newline = 0; newline < 2; newline++) {
		char *wire = g_strdup_printf ("Content-Type: multipart/mixed; boundary=\"%s\"\n\n"
					     "pre\n--%s\nX: y\n\nbody\n--%s--%s",
					     boundary, boundary, boundary, newline ? "\n" : "");
		char *body = g_strdup_printf ("body\n--%s--", boundary);

		for (i = 0; i < G_N_ELEMENTS (chunks); i++) {
			ReaderInput *input = new_input (wire, strlen (wire), chunks[i]);
			TestReader *reader = new_reader (input, NULL);
			GError *error = NULL;

			g_assert_true (g_mime_reader_read_entity ((GMimeReader *) reader, &error));
			g_assert_no_error (error);
			g_assert_cmpint (g_mime_reader_tell ((GMimeReader *) reader), ==, strlen (wire));
			if (newline) {
				g_assert_nonnull (strstr (reader->trace->str, "J:1:"));
				g_assert_cmpmem (reader->content->data, reader->content->len, "\n", 1);
			} else {
				/* MimeReader consumes an incomplete oversized line as
				 * content before its next refill can establish EOF. */
				g_assert_null (strstr (reader->trace->str, "J:1:"));
				g_assert_cmpmem (reader->content->data, reader->content->len, body, strlen (body));
			}
			g_object_unref (reader);
			g_object_unref (input);
		}
		g_free (body);
		g_free (wire);
	}
	g_free (boundary);
}

static void
test_message_reading (void)
{
	static const gsize chunks[] = { 1, 2, 3, 7, 127, 128, 129, 4096, G_MAXSIZE };
	static const char wire[] = "\xef\xbb\xbfSubject: test\r\n\r\na\r\nb\n";
	char *header = g_base64_encode ((const guint8 *) "Subject: test\r\n", 15);
	char *body = g_base64_encode ((const guint8 *) "a\r\nb\n", 5);
	guint i, mode;

	for (mode = 0; mode < 3; mode++) {
		gint64 origin = mode == 1 ? 100 : 0;
		char *expected = g_strdup_printf (
			"B:%" G_GINT64_FORMAT ":1;H:%" G_GINT64_FORMAT ":1:7:7:0:%s;"
			"S:%" G_GINT64_FORMAT ":2:%" G_GINT64_FORMAT ";E:%" G_GINT64_FORMAT ":1:%" G_GINT64_FORMAT ":3;"
			"M:%" G_GINT64_FORMAT ":1;I:0:text/plain:%" G_GINT64_FORMAT ":1;"
			"C:0:%" G_GINT64_FORMAT ":3;D:0:%" G_GINT64_FORMAT ":3:%" G_GINT64_FORMAT ":2:%s;"
			"O:0:%" G_GINT64_FORMAT ":1:%" G_GINT64_FORMAT ":%" G_GINT64_FORMAT ":2;"
			"N:%" G_GINT64_FORMAT ":1:%" G_GINT64_FORMAT ":%" G_GINT64_FORMAT ":2;",
			origin + 3, origin + 3, header, origin + 18, origin + 20, origin + 3, origin + 20,
			origin + 3, origin + 3, origin + 20, origin + 20, origin + 25, body,
			origin + 3, origin + 20, origin + 25, origin + 3, origin + 20, origin + 25);

		for (i = 0; i < G_N_ELEMENTS (chunks); i++) {
			ReaderInput *input = new_input (wire, sizeof (wire) - 1, chunks[i]);
			TestReader *reader;
			GError *error = NULL;
			guint stages, reads;

			input->origin = 100;
			input->nonseekable = mode == 2;
			if (mode == 0)
				input->origin = 0;
			reader = new_reader (input, NULL);
			g_assert_true (g_mime_reader_read_message ((GMimeReader *) reader, &error));
			g_assert_no_error (error);
			g_assert_cmpstr (reader->trace->str, ==, expected);
			g_assert_cmpint (reader->newline_format, ==, GMIME_READER_NEWLINE_MIXED);
			g_assert_cmpint (g_mime_reader_tell ((GMimeReader *) reader), ==, origin + sizeof (wire) - 1);
			g_assert_true (g_mime_reader_eos ((GMimeReader *) reader));
			stages = reader->stage;
			reads = input->reads;
			g_assert_false (g_mime_reader_read_message ((GMimeReader *) reader, &error));
			g_assert_error (error, GMIME_ERROR, GMIME_ERROR_PARSE_ERROR);
			g_clear_error (&error);
			g_assert_cmpuint (reader->stage, ==, stages);
			g_assert_cmpuint (input->reads, ==, reads);
			g_assert_cmpuint (input->resets, ==, 0);
			g_assert_cmpuint (input->closes, ==, 0);
			g_object_unref (reader);
			g_object_unref (input);
		}
		g_free (expected);
	}
	g_free (header);
	g_free (body);
}

static void
test_message_bom_and_state (void)
{
	static const struct {
		const char *wire;
		gsize length;
		const char *trace;
	} cases[] = {
		{ "", 0, "" },
		{ "\xef", 1, "" },
		{ "\xef\xbb", 2, "" },
		{ "\xef!", 2, "" },
		{ "\xef\xbb!", 3, "" },
		{ "\xef\xbb\xbf", 3, "B:3:1;" },
		{ "\xef\xbb\xbf\xef\xbb\xbf", 6, "B:3:1;" },
		{ "\xff\xfe", 2, "B:0:1;" },
	};
	guint i, chunk;

	for (i = 0; i < G_N_ELEMENTS (cases); i++) {
		for (chunk = 1; chunk <= 7; chunk++) {
			ReaderInput *input = new_input (cases[i].wire, cases[i].length, chunk);
			TestReader *reader = new_reader (input, NULL);
			GError *error = NULL;
			guint reads;

			g_assert_false (g_mime_reader_read_message ((GMimeReader *) reader, &error));
			g_assert_error (error, GMIME_ERROR, GMIME_ERROR_PARSE_ERROR);
			g_clear_error (&error);
			g_assert_cmpstr (reader->trace->str, ==, cases[i].trace);
			g_assert_false (g_mime_reader_eos ((GMimeReader *) reader));
			reads = input->reads;
			g_assert_false (g_mime_reader_read_message ((GMimeReader *) reader, &error));
			g_assert_error (error, GMIME_ERROR, GMIME_ERROR_PARSE_ERROR);
			g_clear_error (&error);
			g_assert_cmpuint (input->reads, ==, reads);
			g_object_unref (input);
			input = new_input ("\xef\xbb\xbf\nbody", 8, chunk);
			g_mime_reader_set_stream ((GMimeReader *) reader, (GMimeStream *) input);
			g_assert_true (g_mime_reader_read_message ((GMimeReader *) reader, &error));
			g_assert_no_error (error);
			g_assert_cmpint (g_mime_reader_tell ((GMimeReader *) reader), ==, 8);
			g_object_unref (reader);
			g_object_unref (input);
		}
		for (chunk = 1; chunk <= 129; chunk++) {
			const char *wire = ">From sender\nFrom other\nSubject: test\n\nFrom body\n";
			ReaderInput *input = new_input (wire, strlen (wire), chunk);
			TestReader *reader = new_reader (input, NULL);
			GError *error = NULL;

			g_assert_true (g_mime_reader_read_message ((GMimeReader *) reader, &error));
			g_assert_no_error (error);
			g_assert_nonnull (strstr (reader->trace->str, "H:0:1:13:13:1:"));
			g_assert_nonnull (strstr (reader->trace->str, "H:13:2:11:11:1:"));
			g_assert_nonnull (strstr (reader->trace->str, "M:0:1;"));
			g_assert_cmpmem (reader->content->data, reader->content->len, "From body\n", 10);
			g_object_unref (reader);
			g_object_unref (input);
		}
	}
	{
		const char *wire = "\n\xef\xbb\xbfSubject: test\n\nbody";
		ReaderInput *input = new_input (wire, strlen (wire), 1);
		TestReader *reader = new_reader (input, NULL);
		GError *error = NULL;

		/* BOM handling belongs to stream initialization, not every public
		 * operation. A preceding header read must not re-enable it. */
		g_assert_true (g_mime_reader_read_headers ((GMimeReader *) reader, &error));
		g_assert_false (g_mime_reader_read_message ((GMimeReader *) reader, &error));
		g_assert_error (error, GMIME_ERROR, GMIME_ERROR_PARSE_ERROR);
		g_clear_error (&error);
		g_assert_null (strstr (reader->trace->str, "M:"));
		g_object_unref (reader);
		g_object_unref (input);
	}
}

static void
test_message_errors (void)
{
	const char *wire = "\xef\xbb\xbf" "Content-Type: message/rfc822\n\nSubject: inner\n\nbody\r\n";
	ReaderInput *input = new_input (wire, strlen (wire), 1);
	TestReader *reader = new_reader (input, NULL);
	GError *error = NULL;
	guint stages, stage, mode;

	g_assert_true (g_mime_reader_read_message ((GMimeReader *) reader, &error));
	g_assert_no_error (error);
	stages = reader->stage;
	g_object_unref (reader);
	g_object_unref (input);
	for (mode = 0; mode < 3; mode++) {
		for (stage = 1; stage <= stages; stage++) {
			guint reads;

			input = new_input (wire, strlen (wire), 1);
			reader = new_reader (input, NULL);
			reader->fail_stage = stage;
			reader->no_error = mode == 1;
			reader->error_on_success = mode == 2;
			g_assert_false (g_mime_reader_read_message ((GMimeReader *) reader, &error));
			g_assert_error (error, GMIME_ERROR, (mode == 1 ? GMIME_ERROR_GENERAL : GMIME_ERROR_NOT_SUPPORTED));
			g_clear_error (&error);
			reads = input->reads;
			g_assert_cmpuint (reader->stage, ==, stage);
			g_assert_false (g_mime_reader_read_message ((GMimeReader *) reader, &error));
			g_assert_nonnull (error);
			g_clear_error (&error);
			g_assert_cmpuint (reader->stage, ==, stage);
			g_assert_cmpuint (input->reads, ==, reads);
			g_object_unref (reader);
			g_object_unref (input);
		}
	}
	for (stage = 0; stage <= strlen (wire); stage++) {
		input = new_input (wire, strlen (wire), 1);
		input->fail_after = stage;
		reader = new_reader (input, NULL);
		g_assert_false (g_mime_reader_read_message ((GMimeReader *) reader, &error));
		g_assert_error (error, GMIME_ERROR, EIO);
		g_clear_error (&error);
		g_object_unref (reader);
		g_object_unref (input);
	}
	for (stage = 0; stage < 2; stage++) {
		GMimeReader *weak;

		input = new_input ("\nbody", 5, G_MAXSIZE);
		reader = new_reader (input, NULL);
		weak = (GMimeReader *) reader;
		g_object_add_weak_pointer (G_OBJECT (reader), (gpointer *) &weak);
		/* The outer envelope's end callback is the final notification. */
		reader->release_stage = 10;
		reader->fail_stage = stage ? 10 : 0;
		g_assert_cmpint (g_mime_reader_read_message ((GMimeReader *) reader, &error), ==, !stage);
		if (stage) {
			g_assert_error (error, GMIME_ERROR, GMIME_ERROR_NOT_SUPPORTED);
			g_clear_error (&error);
		} else {
			g_assert_no_error (error);
		}
		g_assert_null (weak);
		g_object_unref (input);
	}
}

static void
test_content_newlines (void)
{
	static const struct {
		const char *body;
		GMimeReaderNewLineFormat format;
	} cases[] = {
		{ "", GMIME_READER_NEWLINE_UNKNOWN },
		{ "body\r", GMIME_READER_NEWLINE_UNKNOWN },
		{ "body\n", GMIME_READER_NEWLINE_UNIX },
		{ "body\r\n", GMIME_READER_NEWLINE_DOS },
		{ "\r\n\n", GMIME_READER_NEWLINE_MIXED },
		{ "\n\r\n", GMIME_READER_NEWLINE_MIXED },
		{ "a\rb\r\n", GMIME_READER_NEWLINE_DOS },
	};
	guint i, chunk;

	for (i = 0; i < G_N_ELEMENTS (cases); i++) {
		char *wire = g_strconcat ("\n", cases[i].body, NULL);

		for (chunk = 1; chunk <= 129; chunk++) {
			ReaderInput *input = new_input (wire, strlen (wire), chunk);
			TestReader *reader = new_reader (input, NULL);
			GError *error = NULL;

			g_assert_true (g_mime_reader_read_message ((GMimeReader *) reader, &error));
			g_assert_no_error (error);
			g_assert_cmpint (reader->newline_format, ==, cases[i].format);
			g_object_unref (reader);
			g_object_unref (input);
		}
		g_free (wire);
	}
	for (chunk = 1; chunk <= 129; chunk++) {
		const char *wire = "Content-Type: multipart/mixed; boundary=b\n\npreamble\n--b\r\n"
			"Content-Transfer-Encoding: base64\n\nYQ==\r\n--b--\nepilogue\n";
		ReaderInput *input = new_input (wire, strlen (wire), chunk);
		TestReader *reader = new_reader (input, NULL);
		GError *error = NULL;

		g_assert_true (g_mime_reader_read_message ((GMimeReader *) reader, &error));
		g_assert_no_error (error);
		/* Header, delimiter and epilogue line endings do not affect the
		 * leaf's format, but its boundary-owned trailing CRLF does. */
		g_assert_cmpint (reader->newline_format, ==, GMIME_READER_NEWLINE_DOS);
		g_object_unref (reader);
		g_object_unref (input);
	}
}

static void
test_entity_limits_and_errors (void)
{
	const char *wire = "Content-Type: multipart/mixed; boundary=b\n\n--b\nContent-Type: message/rfc822\n\n"
		"Subject: embedded\n\nbody\n--b--\nepilogue\n";
	GMimeParserOptions *options = g_mime_parser_options_new ();
	ReaderInput *input = new_input (wire, strlen (wire), 1);
	TestReader *reader = new_reader (input, options);
	GError *error = NULL;
	guint stages, stage, mode;

	g_assert_true (g_mime_reader_read_entity ((GMimeReader *) reader, &error));
	g_assert_no_error (error);
	stages = reader->stage;
	g_object_unref (reader);
	g_object_unref (input);
	for (mode = 0; mode < 3; mode++) {
		for (stage = 1; stage <= stages; stage++) {
			guint reads;

			input = new_input (wire, strlen (wire), 1);
			reader = new_reader (input, options);
			reader->fail_stage = stage;
			reader->no_error = mode == 1;
			reader->error_on_success = mode == 2;
			g_assert_false (g_mime_reader_read_entity ((GMimeReader *) reader, &error));
			g_assert_error (error, GMIME_ERROR, (mode == 1 ? GMIME_ERROR_GENERAL : GMIME_ERROR_NOT_SUPPORTED));
			g_clear_error (&error);
			g_assert_cmpuint (reader->stage, ==, stage);
			reads = input->reads;
			g_assert_false (g_mime_reader_read_entity ((GMimeReader *) reader, &error));
			g_assert_nonnull (error);
			g_clear_error (&error);
			g_assert_cmpuint (input->reads, ==, reads);
			g_assert_cmpuint (reader->stage, ==, stage);
			g_mime_reader_set_stream ((GMimeReader *) reader, (GMimeStream *) input);
			reader->fail_stage = 0;
			if (mode == 0) {
				ReaderInput *replacement = new_input ("\nrecovered", 10, 1);

				g_mime_reader_set_stream ((GMimeReader *) reader, (GMimeStream *) replacement);
				reader->entities = 0;
				g_assert_true (g_mime_reader_read_entity ((GMimeReader *) reader, &error));
				g_assert_no_error (error);
				g_assert_cmpuint (reader->entities, ==, 0);
				g_assert_cmpuint (replacement->resets, ==, 0);
				g_assert_cmpuint (replacement->closes, ==, 0);
				g_object_unref (replacement);
			}
			g_object_unref (reader);
			g_object_unref (input);
		}
	}
	for (stage = 0; stage <= strlen (wire); stage++) {
		input = new_input (wire, strlen (wire), 1);
		input->fail_after = stage;
		reader = new_reader (input, NULL);
		g_assert_false (g_mime_reader_read_entity ((GMimeReader *) reader, &error));
		g_assert_error (error, GMIME_ERROR, EIO);
		g_clear_error (&error);
		g_object_unref (reader);
		g_object_unref (input);
	}
	for (stage = 0; stage < 3; stage++) {
		g_mime_parser_options_set_max_mime_depth (options, stage);
		input = new_input (wire, strlen (wire), 1);
		reader = new_reader (input, options);
		g_assert_true (g_mime_reader_read_entity ((GMimeReader *) reader, &error));
		g_assert_no_error (error);
		g_assert_cmpuint (reader->max_entities, ==, stage + 1);
		g_object_unref (reader);
		g_object_unref (input);
	}
	for (mode = 0; mode < 2; mode++) {
		GMimeReader *weak;

		input = new_input ("\nbody", 5, G_MAXSIZE);
		reader = new_reader (input, NULL);
		weak = (GMimeReader *) reader;
		g_object_add_weak_pointer (G_OBJECT (reader), (gpointer *) &weak);
		reader->release_stage = 6; /* Release the caller's reference from content_read(). */
		reader->fail_stage = mode ? 6 : 0;
		g_assert_cmpint (g_mime_reader_read_entity ((GMimeReader *) reader, &error), ==, !mode);
		if (mode) {
			g_assert_error (error, GMIME_ERROR, GMIME_ERROR_NOT_SUPPORTED);
			g_clear_error (&error);
		} else {
			g_assert_no_error (error);
		}
		g_assert_null (weak);
		g_assert_cmpuint (input->closes, ==, 0);
		g_object_unref (input);
	}
	g_mime_parser_options_free (options);
}

static void
test_entity_streaming_and_depth (void)
{
	GString *wire = g_string_new ("Content-Type: multipart/mixed; boundary=b\r\n\r\n--b\r\n\r\n");
	GMimeParserOptions *options = g_mime_parser_options_new ();
	ReaderInput *input;
	TestReader *reader;
	GMimeReader *plain;
	GError *error = NULL;
	guint i;

	/* Binary data, a very long line and split CRLFs must stream without
	 * mistaking a midline '--b' for a delimiter or buffering the body. */
	for (i = 0; i < 20000; i++)
		g_string_append_len (wire, "\0\r--b", 5);
	g_string_append (wire, "\r\n--b--\r\n");
	input = new_input (wire->str, wire->len, 7);
	input->nonseekable = TRUE;
	reader = new_reader (input, NULL);
	g_assert_true (g_mime_reader_read_entity ((GMimeReader *) reader, &error));
	g_assert_no_error (error);
	g_assert_cmpuint (reader->max_chunk, <=, 4226);
	g_assert_cmpuint (reader->content_chunks, >, 10);
	g_assert_cmpuint (reader->max_entities, ==, 2);
	g_assert_nonnull (strstr (reader->trace->str, "D:0:52:5:100052:1:"));
	g_object_unref (reader);
	g_object_unref (input);
	g_string_set_size (wire, 0);
	for (i = 0; i < 2048; i++)
		g_string_append (wire, "Content-Type: message/rfc822\n\n");
	g_string_append (wire, "\nleaf\n");
	g_mime_parser_options_set_max_mime_depth (options, 4096);
	input = new_input (wire->str, wire->len, G_MAXSIZE);
	reader = new_reader (input, options);
	g_assert_true (g_mime_reader_read_entity ((GMimeReader *) reader, &error));
	g_assert_no_error (error);
	g_assert_cmpuint (reader->max_entities, ==, 2049);
	g_assert_cmpuint (reader->entities, ==, 0);
	g_object_unref (reader);
	g_object_unref (input);
	g_string_set_size (wire, 0);
	for (i = 0; i < 512; i++)
		g_string_append_printf (wire, "Content-Type: multipart/mixed; boundary=b%u\n\n--b%u\n", i, i);
	g_string_append (wire, "\nleaf\n--b0--\n");
	input = new_input (wire->str, wire->len, 3);
	reader = new_reader (input, options);
	g_assert_true (g_mime_reader_read_entity ((GMimeReader *) reader, &error));
	g_assert_no_error (error);
	g_assert_cmpuint (reader->max_entities, ==, 513);
	g_assert_cmpuint (reader->entities, ==, 0);
	g_assert_nonnull (strstr (reader->trace->str, ":bGVhZg==;"));
	g_object_unref (reader);
	g_object_unref (input);

	input = new_input ("\nbody", 5, 1);
	plain = g_mime_reader_new (NULL, (GMimeStream *) input);
	g_assert_true (g_mime_reader_read_entity (plain, &error));
	g_assert_no_error (error);
	g_mime_reader_set_stream (plain, (GMimeStream *) input);
	g_assert_false (g_mime_reader_read_entity (plain, &error));
	g_assert_error (error, GMIME_ERROR, GMIME_ERROR_PARSE_ERROR);
	g_clear_error (&error);
	g_object_unref (plain);
	g_object_unref (input);
	g_mime_parser_options_free (options);
	g_string_free (wire, TRUE);
}

static void
test_mailboxes (void)
{
	static const char mbox[] = "\xef\xbb\xbf" "junk\r\nFrom one\r\nX: a\r\n\r\nbody\r\nFrom two\nX: b\n\nlast\n";
	static const char mmdf[] = "\1\1\1\1\nX: a\n\nbody\n\1\1\1\1\n\1\1\1\1\nX: b\n\nlast\n\1\1\1\1\n";
	static const char protected[] = "body\nFrom fake\nnot a header\nend\n";
	GMimeParserOptions *options = g_mime_parser_options_new ();
	char *length_wire = g_strdup_printf ("From one\nX: a\nContent-Length: %" G_GSIZE_FORMAT
					    "\n\n%sFrom two\nX: b\n\nlast\n", strlen (protected), protected);
	const char *wires[] = { mbox, mmdf, length_wire };
	guint kind, chunk;

	g_mime_parser_options_set_respect_content_length (options, TRUE);
	{
		ReaderInput *input = new_input ("From one\nX: a\n\nbody", 19, 1);
		TestReader *reader = new_reader (input, options);
		GMimeReader *base = (GMimeReader *) reader;
		GError *error = NULL;

		g_mime_reader_set_stream_with_format (base, (GMimeStream *) input, GMIME_FORMAT_MBOX);
		/* Entity reads do not skip mailbox markers. An immediate marker
		 * delimits empty content, whose line count must not underflow. */
		g_assert_true (g_mime_reader_read_entity (base, &error));
		g_assert_no_error (error);
		g_assert_cmpint (g_mime_reader_tell (base), ==, 0);
		g_assert_cmpuint (reader->content->len, ==, 0);
		g_assert_nonnull (strstr (reader->trace->str, "D:0:0:1:0:0:;"));
		g_assert_nonnull (strstr (reader->trace->str, "O:0:0:1:0:0:0;"));
		g_object_unref (reader);
		g_object_unref (input);
	}
	for (kind = 0; kind < G_N_ELEMENTS (wires); kind++) {
		char *expected = NULL;

		for (chunk = 1; chunk <= 129; chunk++) {
			ReaderInput *input = new_input (wires[kind], strlen (wires[kind]), chunk);
			TestReader *reader = new_reader (input, options);
			GMimeReader *base = (GMimeReader *) reader;
			GMimeFormat format = kind == 1 ? GMIME_FORMAT_MMDF : GMIME_FORMAT_MBOX;
			GError *error = NULL;

			g_mime_reader_set_stream_with_format (base, (GMimeStream *) input, format);
			g_assert_cmpint (g_mime_reader_get_format (base), ==, format);
			g_assert_true (g_mime_reader_read_message (base, &error));
			g_assert_no_error (error);
			g_assert_false (g_mime_reader_eos (base));
			g_assert_cmpuint (reader->content->len, ==, kind == 2 ? strlen (protected) - 1 : 4);
			if (kind == 2)
				g_assert_cmpmem (reader->content->data, reader->content->len, protected, strlen (protected) - 1);
			else
				g_assert_cmpmem (reader->content->data, reader->content->len, "body", 4);
			g_assert_true (g_mime_reader_read_message (base, &error));
			g_assert_no_error (error);
			g_assert_true (g_mime_reader_eos (base));
			g_assert_cmpmem (reader->content->data, reader->content->len, kind == 1 ? "last" : "last\n", kind == 1 ? 4 : 5);
			if (!expected)
				expected = g_strdup (reader->trace->str);
			g_assert_cmpstr (reader->trace->str, ==, expected);
			g_assert_false (g_mime_reader_read_message (base, &error));
			g_assert_error (error, GMIME_ERROR, GMIME_ERROR_PARSE_ERROR);
			g_clear_error (&error);
			g_assert_cmpuint (input->resets, ==, 0);
			g_assert_cmpuint (input->closes, ==, 0);
			g_object_unref (reader);
			g_object_unref (input);
		}
		g_free (expected);
	}
	g_mime_parser_options_free (options);
	g_free (length_wire);
}

static void
test_long_mailbox_marker (void)
{
	GString *wire = g_string_new ("From ");
	guint i, chunk;

	for (i = 0; i < 20000; i++)
		g_string_append_c (wire, 'a');
	g_string_append (wire, "\r\nX: a\r\n\r\nbody\r\nFrom two\nX: b\n\nlast\n");
	for (chunk = 1; chunk <= 4096; chunk *= 8) {
		ReaderInput *input = new_input (wire->str, wire->len, chunk);
		TestReader *reader = new_reader (input, NULL);
		GMimeReader *base = (GMimeReader *) reader;
		GError *error = NULL;

		g_mime_reader_set_stream_with_format (base, (GMimeStream *) input, GMIME_FORMAT_MBOX);
		g_assert_true (g_mime_reader_read_message (base, &error));
		g_assert_no_error (error);
		g_assert_cmpuint (reader->max_chunk, <=, 4226);
		g_assert_cmpuint (reader->boundary->len, ==, 20005);
		g_assert_cmpmem (reader->boundary->data, reader->boundary->len, wire->str, 20005);
		g_assert_cmpmem (reader->content->data, reader->content->len, "body", 4);
		g_assert_true (g_mime_reader_read_message (base, &error));
		g_assert_no_error (error);
		g_assert_true (g_mime_reader_eos (base));
		g_assert_cmpmem (reader->boundary->data, reader->boundary->len, "From two", 8);
		g_object_unref (reader);
		g_object_unref (input);
	}
	g_string_free (wire, TRUE);
}

static void
test_mailbox_failures (void)
{
	static const char *wires[] = {
		"From one\r\nX: a\r\n\r\nbody\r\nFrom two\nX: b\n\nlast",
		"\1\1\1\1\nX: a\n\nbody\n\1\1\1\1\n"
	};
	static const char *truncated[] = { "", "F", "From ", "From one", "From one\r", "junk\nFrom one" };
	guint kind, mode, stage, i;
	GError *error = NULL;

	for (kind = 0; kind < G_N_ELEMENTS (wires); kind++) {
		GMimeFormat format = kind ? GMIME_FORMAT_MMDF : GMIME_FORMAT_MBOX;
		ReaderInput *input = new_input (wires[kind], strlen (wires[kind]), 1);
		TestReader *reader = new_reader (input, NULL);
		guint stages;

		g_mime_reader_set_stream_with_format ((GMimeReader *) reader, (GMimeStream *) input, format);
		g_assert_true (g_mime_reader_read_message ((GMimeReader *) reader, &error));
		g_assert_no_error (error);
		stages = reader->stage;
		g_object_unref (reader);
		g_object_unref (input);
		for (mode = 0; mode < 3; mode++) {
			for (stage = 1; stage <= stages; stage++) {
				guint reads;

				input = new_input (wires[kind], strlen (wires[kind]), 1);
				reader = new_reader (input, NULL);
				g_mime_reader_set_stream_with_format ((GMimeReader *) reader, (GMimeStream *) input, format);
				reader->fail_stage = stage;
				reader->no_error = mode == 1;
				reader->error_on_success = mode == 2;
				g_assert_false (g_mime_reader_read_message ((GMimeReader *) reader, &error));
				g_assert_error (error, GMIME_ERROR, (mode == 1 ? GMIME_ERROR_GENERAL : GMIME_ERROR_NOT_SUPPORTED));
				g_clear_error (&error);
				g_assert_cmpuint (reader->stage, ==, stage);
				reads = input->reads;
				g_assert_false (g_mime_reader_read_message ((GMimeReader *) reader, &error));
				g_assert_nonnull (error);
				g_clear_error (&error);
				g_assert_cmpuint (input->reads, ==, reads);
				g_assert_cmpuint (reader->stage, ==, stage);
				g_object_unref (reader);
				g_object_unref (input);
			}
		}
		for (stage = 0; stage < strlen (wires[kind]); stage++) {
			guint messages = 0;

			input = new_input (wires[kind], strlen (wires[kind]), 1);
			input->fail_after = stage;
			reader = new_reader (input, NULL);
			g_mime_reader_set_stream_with_format ((GMimeReader *) reader, (GMimeStream *) input, format);
			/* A failure beyond the first marker belongs to the next message. */
			while (g_mime_reader_read_message ((GMimeReader *) reader, &error)) {
				g_assert_no_error (error);
				g_assert_cmpuint (++messages, <=, 2);
			}
			g_assert_error (error, GMIME_ERROR, EIO);
			g_clear_error (&error);
			g_object_unref (reader);
			g_object_unref (input);
		}
	}
	for (i = 0; i < G_N_ELEMENTS (truncated); i++) {
		ReaderInput *input = new_input (truncated[i], strlen (truncated[i]), 1);
		GMimeReader *reader = g_mime_reader_new_with_format (NULL, (GMimeStream *) input, GMIME_FORMAT_MBOX);

		g_assert_false (g_mime_reader_read_message (reader, &error));
		g_assert_error (error, GMIME_ERROR, GMIME_ERROR_PARSE_ERROR);
		g_clear_error (&error);
		g_assert_cmpuint (input->resets, ==, 0);
		g_assert_cmpuint (input->closes, ==, 0);
		g_object_unref (reader);
		g_object_unref (input);
	}
}

static void
test_status_reader_helpers (void)
{
	static const char *wires[] = { "", "\n", "\r\n\n", "\r", "\rX", "\n\r\nX: a\n\nbody" };
	guint i, chunk;

	for (i = 0; i < G_N_ELEMENTS (wires); i++) {
		for (chunk = 1; chunk <= 129; chunk++) {
			ReaderInput *input = new_input (wires[i], strlen (wires[i]), chunk);
			TestReader *reader = new_reader (input, NULL);
			GMimeReader *base = (GMimeReader *) reader;
			GMimeStream *remainder;
			GError *error = NULL;
			char bytes[32];
			gsize skip = 0, length = strlen (wires[i]);
			ssize_t count;

			while (skip < length) {
				if (wires[i][skip] == '\n')
					skip++;
				else if (skip + 1 < length && wires[i][skip] == '\r' && wires[i][skip + 1] == '\n')
					skip += 2;
				else
					break;
			}
			input->nonseekable = TRUE;
			g_assert_cmpint (_g_mime_reader_skip_blank_lines (base, &error), ==, skip < length);
			g_assert_no_error (error);
			g_assert_cmpint (g_mime_reader_tell (base), ==, skip);
			remainder = _g_mime_reader_read_to_eos (base, &error);
			g_assert_no_error (error);
			g_assert_nonnull (remainder);
			g_assert_cmpint (g_mime_stream_tell (remainder), ==, 0);
			count = g_mime_stream_read (remainder, bytes, sizeof bytes);
			g_assert_cmpint (count, ==, length - skip);
			g_assert_cmpmem (bytes, count, wires[i] + skip, length - skip);
			g_assert_true (g_mime_reader_eos (base));
			g_assert_cmpint (g_mime_reader_tell (base), ==, length);
			g_assert_cmpuint (reader->stage, ==, 0);
			g_assert_cmpuint (input->resets, ==, 0);
			g_assert_cmpuint (input->closes, ==, 0);
			g_object_unref (remainder);
			g_object_unref (reader);
			g_object_unref (input);
		}
	}
	for (i = 0; i < 12; i++) {
		ReaderInput *input = new_input ("\n\r\nX: a\n\nb\0y", 12, 1);
		GMimeReader *reader = g_mime_reader_new (NULL, (GMimeStream *) input);
		GMimeStream *content;
		GError *error = NULL;
		gboolean more;
		guint reads;

		input->fail_after = i;
		more = _g_mime_reader_skip_blank_lines (reader, &error);
		if (!error) {
			g_assert_true (more);
			content = _g_mime_reader_read_to_eos (reader, &error);
			g_assert_null (content);
		}
		g_assert_error (error, GMIME_ERROR, EIO);
		g_clear_error (&error);
		reads = input->reads;
		content = _g_mime_reader_read_to_eos (reader, &error);
		g_assert_null (content);
		g_assert_error (error, GMIME_ERROR, EIO);
		g_clear_error (&error);
		g_assert_cmpuint (input->reads, ==, reads);
		g_object_unref (reader);
		g_object_unref (input);
	}
}

static void
collect_compliance (const GMimeComplianceIssue *issue, gpointer user_data)
{
	g_array_append_val ((GArray *) user_data, *issue);
}

static void
test_compliance_options_and_issues (void)
{
	GMimeComplianceOptions *defaults = g_mime_compliance_options_get_default ();
	GMimeComplianceOptions *saved = g_mime_compliance_options_clone (defaults);
	GMimeComplianceOptions *options, *copy;
	GMimeComplianceIssue *issue, *duplicate;
	GMimeReader *reader;
	ReaderInput *input;
	char *text;
	guint i;

	options = g_mime_compliance_options_new ();
	g_assert_cmpint (g_mime_compliance_options_get_context (options), ==, GMIME_COMPLIANCE_CONTEXT_TRANSPORT);
	g_assert_cmpint (g_mime_compliance_options_get_enabled_validators (options), ==, GMIME_COMPLIANCE_VALIDATOR_ALL);
	g_assert_cmpuint (g_mime_compliance_options_get_max_issues_per_violation (options), ==, 0);
	g_mime_compliance_options_set_context (defaults, GMIME_COMPLIANCE_CONTEXT_STORAGE);
	g_mime_compliance_options_set_max_issues_per_violation (defaults, 7);
	g_mime_compliance_options_set_enabled_validators (defaults, GMIME_COMPLIANCE_VALIDATOR_ADDRESS);
	input = new_input ("\n", 1, 1);
	reader = g_mime_reader_new (NULL, (GMimeStream *) input);
	copy = g_mime_reader_get_compliance_options (reader);
	g_assert_true (copy != defaults);
	g_assert_cmpint (g_mime_compliance_options_get_context (copy), ==, GMIME_COMPLIANCE_CONTEXT_STORAGE);
	g_assert_cmpint (g_mime_compliance_options_get_enabled_validators (copy), ==, GMIME_COMPLIANCE_VALIDATOR_ADDRESS);
	g_assert_cmpuint (g_mime_compliance_options_get_max_issues_per_violation (copy), ==, 7);
	g_mime_reader_set_compliance_options (reader, options);
	g_mime_compliance_options_set_context (options, GMIME_COMPLIANCE_CONTEXT_STORAGE);
	g_assert_cmpint (g_mime_compliance_options_get_context (g_mime_reader_get_compliance_options (reader)),
			 ==, GMIME_COMPLIANCE_CONTEXT_TRANSPORT);
	g_mime_reader_set_compliance_options (reader, NULL);
	g_assert_cmpint (g_mime_compliance_options_get_context (g_mime_reader_get_compliance_options (reader)),
			 ==, GMIME_COMPLIANCE_CONTEXT_STORAGE);
	g_mime_compliance_options_set_context (defaults, g_mime_compliance_options_get_context (saved));
	g_mime_compliance_options_set_enabled_validators (defaults, g_mime_compliance_options_get_enabled_validators (saved));
	g_mime_compliance_options_set_max_issues_per_violation (defaults, g_mime_compliance_options_get_max_issues_per_violation (saved));
	g_mime_compliance_options_free (options);
	g_mime_compliance_options_free (saved);
	g_object_unref (reader);
	g_object_unref (input);

	for (i = 1; i <= 91; i++) {
		GMimeComplianceViolation violation = i == 91 ? GMIME_COMPLIANCE_TOO_MANY_COMPLIANCE_ISSUES : i;

		issue = g_mime_compliance_issue_new (GMIME_COMPLIANCE_CONTEXT_STORAGE, violation, G_MAXINT64,
						   (guint64) G_MAXUINT + 1, (guint64) G_MAXUINT + 2,
						   GMIME_COMPLIANCE_POSITION_LINE_START);
		duplicate = g_boxed_copy (GMIME_TYPE_COMPLIANCE_ISSUE, issue);
		g_assert_true (g_mime_compliance_issue_equal (issue, duplicate));
		duplicate->column_number++;
		g_assert_false (g_mime_compliance_issue_equal (issue, duplicate));
		g_assert_nonnull (g_mime_compliance_violation_get_description (violation));
		g_assert_nonnull (g_mime_compliance_violation_get_remarks (violation));
		g_assert_cmpint (g_mime_compliance_violation_get_severity (violation, issue->context),
				 <=, GMIME_COMPLIANCE_SEVERITY_CRITICAL);
		g_assert_cmpint (g_mime_compliance_violation_get_categories (violation), <=, 7);
		text = g_mime_compliance_issue_to_string (issue);
		g_assert_nonnull (strstr (text, "line 4294967296, column 4294967297 (offset 9223372036854775807)"));
		g_free (text);
		issue->column_number = 0;
		text = g_mime_compliance_issue_to_string (issue);
		g_assert_null (strstr (text, "column"));
		g_free (text);
		g_boxed_free (GMIME_TYPE_COMPLIANCE_ISSUE, duplicate);
		g_mime_compliance_issue_free (issue);
	}
}

typedef struct {
	GMimeReader *reader;
	GArray *issues;
	guint destroyed;
	gboolean replace;
} ChangingLogger;

static void
logger_destroyed (gpointer user_data)
{
	((ChangingLogger *) user_data)->destroyed++;
}

static void
changing_logger (const GMimeComplianceIssue *issue, gpointer user_data)
{
	ChangingLogger *logger = user_data;

	g_assert_cmpuint (logger->destroyed, ==, 0);
	collect_compliance (issue, logger->issues);
	if (logger->replace) {
		GMimeComplianceOptions *options = g_mime_compliance_options_new ();

		logger->replace = FALSE;
		g_mime_compliance_options_set_context (options, GMIME_COMPLIANCE_CONTEXT_STORAGE);
		g_mime_compliance_options_set_enabled_validators (options, GMIME_COMPLIANCE_VALIDATOR_NONE);
		g_mime_compliance_options_set_max_issues_per_violation (options, 1);
		g_mime_reader_set_compliance_options (logger->reader, options);
		g_mime_compliance_options_free (options);
		g_mime_reader_set_compliance_logger (logger->reader, collect_compliance, logger->issues, NULL);
	}
}

static void
test_compliance_logging (void)
{
	static const char wire[] = "X: a\nX: b\n\n\xff\0body\nmore\nlast\n";
	static const char encoded[] = "Content-Transfer-Encoding: base64\n\n!!!!\n";
	static const char mailbox[] = "From a\nX: a\nX: b\n\nbody\nmore\n"
		"From b\nX: a\nX: b\n\nbody\nmore\n";
	GArray *issues = g_array_new (FALSE, FALSE, sizeof (GMimeComplianceIssue));
	GMimeComplianceOptions *options = g_mime_compliance_options_new ();
	GError *error = NULL;
	guint chunk, i;

	for (chunk = 1; chunk <= 129; chunk++) {
		ReaderInput *input = new_input (wire, sizeof (wire) - 1, chunk);
		TestReader *reader = new_reader (input, NULL);
		GMimeReader *base = (GMimeReader *) reader;
		guint counts[91] = { 0 }, truncated = 0;

		g_array_set_size (issues, 0);
		g_mime_compliance_options_set_max_issues_per_violation (options, 1);
		g_mime_reader_set_compliance_options (base, options);
		g_mime_reader_set_compliance_logger (base, collect_compliance, issues, NULL);
		g_assert_true (g_mime_reader_read_message (base, &error));
		g_assert_no_error (error);
		for (i = 0; i < issues->len; i++) {
			const GMimeComplianceIssue *issue = &g_array_index (issues, GMimeComplianceIssue, i);

			g_assert_cmpint (issue->context, ==, GMIME_COMPLIANCE_CONTEXT_TRANSPORT);
			if (issue->violation == GMIME_COMPLIANCE_TOO_MANY_COMPLIANCE_ISSUES) {
				truncated++;
				g_assert_cmpint (issue->position_kind, ==, GMIME_COMPLIANCE_POSITION_EXACT);
			} else {
				g_assert_cmpuint (++counts[issue->violation], ==, 1);
			}
		}
		g_assert_cmpuint (truncated, ==, 1);
		g_assert_cmpuint (counts[GMIME_COMPLIANCE_MISSING_DATE], ==, 1);
		g_assert_cmpuint (counts[GMIME_COMPLIANCE_MISSING_FROM], ==, 1);
		g_assert_cmpuint (counts[GMIME_COMPLIANCE_MISSING_MESSAGE_ID], ==, 1);
		g_assert_cmpuint (counts[GMIME_COMPLIANCE_UNEXPECTED_8BIT_BYTES_IN_BODY], ==, 1);
		g_assert_cmpuint (counts[GMIME_COMPLIANCE_UNEXPECTED_NULL_BYTES_IN_BODY], ==, 1);
		g_assert_cmpuint (counts[GMIME_COMPLIANCE_BARE_LINEFEED_IN_BODY], ==, 1);
		g_assert_cmpuint (issues->len, ==, 8);
		g_assert_cmpmem (reader->content->data, reader->content->len, wire + 11, sizeof (wire) - 12);
		g_object_unref (reader);
		g_object_unref (input);
	}
	{
		ReaderInput *input = new_input (encoded, sizeof (encoded) - 1, 1);
		GMimeReader *reader = g_mime_reader_new (NULL, (GMimeStream *) input);
		ChangingLogger logger = { reader, issues, 0, TRUE };
		gboolean have_encoding_issue = FALSE;

		g_array_set_size (issues, 0);
		g_mime_reader_set_compliance_logger (reader, changing_logger, &logger, logger_destroyed);
		g_assert_true (g_mime_reader_read_entity (reader, &error));
		g_assert_no_error (error);
		g_assert_cmpuint (logger.destroyed, ==, 1);
		for (i = 0; i < issues->len; i++) {
			GMimeComplianceIssue *issue = &g_array_index (issues, GMimeComplianceIssue, i);

			g_assert_cmpint (issue->context, ==, GMIME_COMPLIANCE_CONTEXT_TRANSPORT);
			have_encoding_issue |= issue->violation == GMIME_COMPLIANCE_INVALID_BASE64_CHARACTER;
		}
		g_assert_true (have_encoding_issue);
		g_object_unref (input);
		input = new_input (encoded, sizeof (encoded) - 1, 1);
		g_mime_reader_set_stream (reader, (GMimeStream *) input);
		g_array_set_size (issues, 0);
		g_assert_true (g_mime_reader_read_entity (reader, &error));
		g_assert_no_error (error);
		for (i = 0; i < issues->len; i++) {
			GMimeComplianceIssue *issue = &g_array_index (issues, GMimeComplianceIssue, i);

			g_assert_cmpint (issue->context, ==, GMIME_COMPLIANCE_CONTEXT_STORAGE);
			g_assert_cmpint (issue->violation, !=, GMIME_COMPLIANCE_INVALID_BASE64_CHARACTER);
		}
		g_mime_reader_set_compliance_logger (reader, NULL, NULL, NULL);
		g_array_set_size (issues, 0);
		g_object_unref (input);
		input = new_input (encoded, sizeof (encoded) - 1, 1);
		g_mime_reader_set_stream (reader, (GMimeStream *) input);
		g_assert_true (g_mime_reader_read_entity (reader, &error));
		g_assert_no_error (error);
		g_assert_cmpuint (issues->len, ==, 0);
		g_object_unref (reader);
		g_object_unref (input);
	}
	{
		ReaderInput *input = new_input (mailbox, sizeof (mailbox) - 1, 1);
		GMimeReader *reader = g_mime_reader_new_with_format (NULL, (GMimeStream *) input, GMIME_FORMAT_MBOX);
		guint first_count;

		g_array_set_size (issues, 0);
		g_mime_reader_set_compliance_options (reader, options);
		g_mime_reader_set_compliance_logger (reader, collect_compliance, issues, NULL);
		g_assert_true (g_mime_reader_read_message (reader, &error));
		g_assert_no_error (error);
		first_count = issues->len;
		g_assert_cmpuint (first_count, ==, 6);
		g_assert_true (g_mime_reader_read_message (reader, &error));
		g_assert_no_error (error);
		g_assert_cmpuint (issues->len, ==, 2 * first_count);
		for (i = 0; i < first_count; i++)
			g_assert_cmpint (g_array_index (issues, GMimeComplianceIssue, i).violation, ==,
					 g_array_index (issues, GMimeComplianceIssue, first_count + i).violation);
		g_object_unref (reader);
		g_object_unref (input);
	}
	g_mime_compliance_options_free (options);
	g_array_unref (issues);
}

static void
test_compliance_failures (void)
{
	static const char wire[] = "From: bad address\nContent-Transfer-Encoding: base64\n\n!!!!\n";
	GArray *issues = g_array_new (FALSE, FALSE, sizeof (GMimeComplianceIssue));
	guint stage, stages, offset;
	ReaderInput *input = new_input (wire, sizeof (wire) - 1, 1);
	TestReader *reader = new_reader (input, NULL);
	GError *error = NULL;

	g_assert_true (g_mime_reader_read_message ((GMimeReader *) reader, &error));
	g_assert_no_error (error);
	stages = reader->stage;
	g_object_unref (reader);
	g_object_unref (input);
	for (stage = 0; stage <= stages; stage++) {
		for (offset = 0; offset < (stage ? 1 : sizeof (wire) - 1); offset++) {
			ChangingLogger logger;
			guint count, reads;

			input = new_input (wire, sizeof (wire) - 1, 1);
			reader = new_reader (input, NULL);
			reader->fail_stage = stage;
			if (!stage)
				input->fail_after = offset;
			logger = (ChangingLogger) { (GMimeReader *) reader, issues, 0, FALSE };
			g_array_set_size (issues, 0);
			g_mime_reader_set_compliance_logger (logger.reader, changing_logger, &logger, logger_destroyed);
			g_assert_false (g_mime_reader_read_message (logger.reader, &error));
			g_assert_error (error, GMIME_ERROR, (stage ? GMIME_ERROR_NOT_SUPPORTED : EIO));
			g_clear_error (&error);
			count = issues->len;
			reads = input->reads;
			g_assert_false (g_mime_reader_read_message (logger.reader, &error));
			g_assert_nonnull (error);
			g_clear_error (&error);
			g_assert_cmpuint (issues->len, ==, count);
			g_assert_cmpuint (input->reads, ==, reads);
			g_assert_cmpuint (logger.destroyed, ==, 0);
			g_object_unref (reader);
			g_assert_cmpuint (logger.destroyed, ==, 1);
			g_object_unref (input);
		}
	}
	g_array_unref (issues);
}

static void
test_mimekit_reference (const char *datadir)
{
	char *path = g_build_filename (datadir, "mimekit.txt", NULL), *data;
	GError *error = NULL;
	char **lines;
	guint i, count = 0, empty_failures = 0;

	g_assert_true (g_file_get_contents (path, &data, NULL, &error));
	g_assert_no_error (error);
	lines = g_strsplit (data, "\n", -1);
	for (i = 0; lines[i]; i++) {
		GMimeParserOptions *options;
		ReaderInput *input;
		TestReader *reader;
		char **fields, *actual;
		guint8 *bytes;
		gsize length;
		gboolean success;

		if (!lines[i][0] || lines[i][0] == '#')
			continue;
		fields = g_strsplit (lines[i], "|", -1);
		g_assert_cmpuint (g_strv_length (fields), ==, 8);
		bytes = g_base64_decode (fields[0], &length);
		input = new_input ((const char *) bytes, length, g_ascii_strtoull (fields[1], NULL, 10));
		input->origin = g_ascii_strtoll (fields[2], NULL, 10);
		input->nonseekable = !strcmp (fields[3], "1");
		options = g_mime_parser_options_new ();
		g_mime_parser_options_set_max_mime_depth (options, 512);
		reader = new_reader (input, options);
		reader->trace_newlines = TRUE;
		success = g_mime_reader_read_message ((GMimeReader *) reader, &error);
		if (success)
			g_assert_no_error (error);
		else
			g_assert_nonnull (error);
		actual = g_strdup_printf ("%d|%" G_GINT64_FORMAT "|%d|%s", success,
					  g_mime_reader_tell ((GMimeReader *) reader),
					  g_mime_reader_eos ((GMimeReader *) reader), reader->trace->str);
		{
			gboolean expected_eos = !strcmp (fields[6], "1");
			char *expected;

			/* Native EOS denotes successful completion, not a failed empty read. */
			if (length == 0) {
				g_assert_false (success);
				g_assert_true (expected_eos);
				expected_eos = FALSE;
				empty_failures++;
			}
			expected = g_strdup_printf ("%s|%s|%d|%s", fields[4], fields[5], expected_eos, fields[7]);

			g_assert_cmpstr (actual, ==, expected);
			g_free (expected);
		}
		g_clear_error (&error);
		g_free (actual);
		g_object_unref (reader);
		g_object_unref (input);
		g_mime_parser_options_free (options);
		g_free (bytes);
		g_strfreev (fields);
		count++;
	}
	g_assert_cmpuint (count, ==, 90);
	g_assert_cmpuint (empty_failures, ==, 6);
	g_strfreev (lines);
	g_free (data);
	g_free (path);
}

int
main (int argc, char **argv)
{
	int errors;
	const char *datadir = "data" G_DIR_SEPARATOR_S "reader";
	int i;

	g_mime_init ();
	testsuite_init (argc, argv);
	testsuite_start ("GMimeReader headers, entities and parser OpenPGP option");

	for (i = 1; i < argc; i++) {
		if (argv[i][0] != '-') {
			datadir = argv[i];
			break;
		}
	}
	testsuite_check ("Frozen actual-MimeKit message and newline differential cases");
	test_mimekit_reference (datadir);
	testsuite_check_passed ();

	testsuite_check ("Raw header callbacks and chunk boundaries");
	test_headers ();
	test_reader_edge_cases ();
	testsuite_check_passed ();
	testsuite_check ("Explicit-stack entity, multipart and embedded-message traversal");
	test_entity_traversal ();
	test_binary_transfer_encoding ();
	test_entity_oversized_delimiter ();
	testsuite_check_passed ();
	testsuite_check ("Entity callback aborts, sticky failures, I/O errors and depth flattening");
	test_entity_limits_and_errors ();
	testsuite_check_passed ();
	testsuite_check ("Bounded content streaming and deep nonrecursive entity traversal");
	test_entity_streaming_and_depth ();
	testsuite_check_passed ();
	testsuite_check ("Top-level message envelopes, BOM handling and stream state");
	test_message_reading ();
	test_message_bom_and_state ();
	testsuite_check_passed ();
	testsuite_check ("Message callback failures, input errors and callback lifetime");
	test_message_errors ();
	testsuite_check_passed ();
	testsuite_check ("Raw leaf newline detection across short reads");
	test_content_newlines ();
	testsuite_check_passed ();
	testsuite_check ("Mailbox markers, repeated messages and Content-Length framing");
	test_mailboxes ();
	test_mailbox_failures ();
	test_status_reader_helpers ();
	testsuite_check_passed ();
	testsuite_check ("Compliance defaults, boxed diagnostics, logger snapshots and per-violation budgets");
	test_compliance_options_and_issues ();
	test_compliance_logging ();
	test_compliance_failures ();
	test_long_mailbox_marker ();
	testsuite_check_passed ();
	testsuite_check ("Header limits, callback failures and input errors");
	test_reader_limits_and_errors ();
	testsuite_check_passed ();
	testsuite_check ("Reader positions, state, ownership and reinitialization");
	test_reader_state ();
	test_reader_default_callbacks ();
	testsuite_check_passed ();
	testsuite_check ("Opt-in OpenPGP parsing and explicit lazy detection");
	test_openpgp_option ();
	test_openpgp_reuse ();
	testsuite_check_passed ();

	testsuite_end ();
	errors = testsuite_total_errors ();
	g_mime_shutdown ();
	return errors;
}
