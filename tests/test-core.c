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

#include <stdio.h>
#include <string.h>
#include <errno.h>

#include <glib/gstdio.h>

#include <gmime/gmime.h>
#include <gmime/gmime-events.h>
#include <gmime/gmime-internal.h>

#include "testsuite.h"

typedef struct {
	TestStreamOneByte parent_instance;
	gsize fail_after;
	gsize bytes_read;
	guint read_calls;
	gsize bytes_written;
	guint write_calls;
	gboolean zero_write;
	gboolean onebyte;
	gboolean nonseekable;
	gint64 position_offset;
	guint reset_calls;
	guint fail_reset_on;
	guint flush_calls;
	guint close_calls;
	gboolean fail_close;
	gboolean zero_read;
	gboolean unset_error;
} TestStreamFailure;

typedef TestStreamOneByteClass TestStreamFailureClass;

G_DEFINE_TYPE (TestStreamFailure, test_stream_failure, TEST_TYPE_STREAM_ONEBYTE)

static ssize_t
failure_stream_read (GMimeStream *stream, char *buf, size_t len)
{
	TestStreamFailure *test = (TestStreamFailure *) stream;
	ssize_t nread;

	test->read_calls++;
	if (test->zero_read) {
		test->zero_read = FALSE;
		return 0;
	}
	if (test->bytes_read == test->fail_after) {
		errno = test->unset_error ? 0 : EIO;
		return -1;
	}

	len = MIN (len, test->fail_after - test->bytes_read);
	if (test->onebyte)
		len = MIN (len, 1);
	nread = g_mime_stream_read (test->parent_instance.source, buf, len);
	if (nread > 0)
		test->bytes_read += nread;
	return nread;
}

static gint64
failure_stream_tell (GMimeStream *stream)
{
	TestStreamFailure *test = (TestStreamFailure *) stream;

	if (test->nonseekable) {
		errno = ESPIPE;
		return -1;
	}
	return test->position_offset + g_mime_stream_tell (test->parent_instance.source);
}

static ssize_t
failure_stream_write (GMimeStream *stream, const char *buf, size_t len)
{
	TestStreamFailure *test = (TestStreamFailure *) stream;
	ssize_t nwritten;

	test->write_calls++;
	if (test->bytes_written == test->fail_after) {
		if (test->zero_write)
			return 0;
		errno = ENOSPC;
		return -1;
	}

	len = MIN (len, test->fail_after - test->bytes_written);
	if (test->onebyte)
		len = MIN (len, 1);
	nwritten = g_mime_stream_write (test->parent_instance.source, buf, len);
	if (nwritten > 0)
		test->bytes_written += nwritten;
	return nwritten;
}

static int
failure_stream_reset (GMimeStream *stream)
{
	TestStreamFailure *test = (TestStreamFailure *) stream;

	if (++test->reset_calls == test->fail_reset_on) {
		errno = test->unset_error ? 0 : ESPIPE;
		return -1;
	}

	return g_mime_stream_reset (test->parent_instance.source);
}

static int
failure_stream_flush (GMimeStream *stream)
{
	((TestStreamFailure *) stream)->flush_calls++;
	errno = EIO;
	return -1;
}

static int
failure_stream_close (GMimeStream *stream)
{
	TestStreamFailure *test = (TestStreamFailure *) stream;

	test->close_calls++;
	if (test->fail_close) {
		errno = EBADF;
		return -1;
	}

	return g_mime_stream_close (test->parent_instance.source);
}

static void
test_stream_failure_class_init (TestStreamFailureClass *klass)
{
	GMimeStreamClass *stream_class = GMIME_STREAM_CLASS (klass);

	stream_class->read = failure_stream_read;
	stream_class->write = failure_stream_write;
	stream_class->tell = failure_stream_tell;
	stream_class->reset = failure_stream_reset;
	stream_class->flush = failure_stream_flush;
	stream_class->close = failure_stream_close;
}

static void
test_stream_failure_init (TestStreamFailure *stream)
{
}

typedef struct {
	GMimeContent parent_instance;
	guint write_calls;
	guint decode_calls;
	gboolean fail_decode;
} TestContent;

typedef GMimeContentClass TestContentClass;

G_DEFINE_TYPE (TestContent, test_content, GMIME_TYPE_CONTENT)

static ssize_t
test_content_write_to_stream (GMimeContent *content, GMimeStream *stream)
{
	((TestContent *) content)->write_calls++;
	return g_mime_stream_write (stream, "Q2F0", 4);
}

static ssize_t
test_content_decode_to_stream (GMimeContent *content, GMimeStream *stream)
{
	TestContent *custom = (TestContent *) content;

	custom->decode_calls++;
	if (custom->fail_decode) {
		errno = 0;
		return -1;
	}
	return GMIME_CONTENT_CLASS (test_content_parent_class)->decode_to_stream (content, stream);
}

static void
test_content_class_init (TestContentClass *klass)
{
	klass->write_to_stream = test_content_write_to_stream;
	klass->decode_to_stream = test_content_decode_to_stream;
}

static void
test_content_init (TestContent *content)
{
}

typedef struct {
	GMimePart parent_instance;
	guint calls;
	guint max_line_length;
	GMimeEncodingConstraint constraint;
} TestPreparePart;

typedef GMimePartClass TestPreparePartClass;

G_DEFINE_TYPE (TestPreparePart, test_prepare_part, GMIME_TYPE_PART)

static gboolean
test_prepare_part_prepare (GMimeEntity *object, GMimeEncodingConstraint constraint, guint max_line_length, GError **error)
{
	TestPreparePart *part = (TestPreparePart *) object;

	part->calls++;
	part->max_line_length = max_line_length;
	part->constraint = constraint;
	return GMIME_ENTITY_CLASS (test_prepare_part_parent_class)->prepare_with_max_line_length (
		object, constraint, max_line_length, error);
}

static void
test_prepare_part_class_init (TestPreparePartClass *klass)
{
	GMIME_ENTITY_CLASS (klass)->prepare_with_max_line_length = test_prepare_part_prepare;
}

static void
test_prepare_part_init (TestPreparePart *part)
{
}

typedef GMimeEntity TestUnpreparedEntity;
typedef GMimeEntityClass TestUnpreparedEntityClass;

G_DEFINE_TYPE (TestUnpreparedEntity, test_unprepared_entity, GMIME_TYPE_ENTITY)

static void
test_unprepared_entity_class_init (TestUnpreparedEntityClass *klass)
{
}

static void
test_unprepared_entity_init (TestUnpreparedEntity *entity)
{
}

static void
check (gboolean condition, const char *description)
{
	testsuite_check ("%s", description);
	if (condition)
		testsuite_check_passed ();
	else
		testsuite_check_failed ("%s", description);
}

static void
test_header_list_loading (void)
{
	static const char text[] = "Subject: first\r\n"
		"Content-Type: multipart/mixed; boundary=unused\r\n"
		"sUbJeCt \t: second\r\nX-Folded: one\r\n\ttwo\r\n"
		"X-Raw: \317\360\350\342\345\362\r\n\r\nNot a MIME body";
	static const char blocks[] = "X-One: one\n\nX-Two: two\r\n\r\n";
	const char *empty[] = { "", "\n", "\r\n" };
	GMimeParserOptions *options;
	GMimeHeaderList *headers, *next;
	GMimeStream *stream, *input;
	GMimeParser *parser;
	GMimeHeader *header;
	GError *error = NULL;
	gint64 position;
	guint i;
	int onebyte;

	for (onebyte = 0; onebyte < 2; onebyte++) {
		options = g_mime_parser_options_new ();
		check (g_mime_parser_options_set_charset_encoding (options, "windows-1251", NULL),
		       "set standalone header fallback charset");
		stream = g_mime_stream_mem_new_with_buffer (text, sizeof (text) - 1);
		input = onebyte ? test_stream_onebyte_new (stream) : g_object_ref (stream);
		parser = g_mime_parser_new_with_stream (input);
		headers = g_mime_parser_construct_headers (parser, options, &error);
		g_assert_nonnull (headers);
		position = strstr (text, "\r\n\r\n") - text;
		check (error == NULL && g_mime_header_list_get_count (headers) == 5,
		       "standalone parsing retains both message and content headers");
		check (g_mime_parser_get_headers_begin (parser) == 0 &&
		       g_mime_parser_get_headers_end (parser) == position + 2 &&
		       g_mime_parser_tell (parser) == position + 4 && !g_mime_parser_eos (parser),
		       "standalone parsing stops logically after the body separator");
		check (g_mime_header_list_index_of (headers, "SUBJECT") == 0 &&
		       g_mime_header_list_last_index_of (headers, "subject") == 2,
		       "loaded duplicate fields retain order and first-name lookup");
		header = g_mime_header_list_get_header_at (headers, 2);
		check (!strcmp (g_mime_header_get_name (header), "sUbJeCt") &&
		       !strcmp (testsuite_header_raw_name (header), "sUbJeCt \t") &&
		       !strcmp (testsuite_header_raw_text (header), " second\r\n") &&
		       g_mime_header_get_offset (header) == strstr (text, "sUbJeCt") - text &&
		       header->value == NULL, "loaded headers preserve raw names, values, offsets and lazy decoding");
		check (!strcmp (testsuite_header_raw_text (g_mime_header_list_get_header (headers, "X-Folded")),
				" one\r\n\ttwo\r\n"), "standalone parsing preserves header folding");
		g_mime_parser_options_set_charset_encoding (options, "windows-1252", NULL);
		g_mime_parser_options_free (options);
		g_object_unref (parser);
		g_object_unref (input);
		g_object_unref (stream);
		check (!strcmp (g_mime_header_get_value (g_mime_header_list_get_header (headers, "X-Raw")),
				"\320\237\321\200\320\270\320\262\320\265\321\202"),
		       "loaded headers own raw data and a parser-option snapshot after input teardown");
		g_object_unref (headers);

		stream = g_mime_stream_mem_new_with_buffer (blocks, sizeof (blocks) - 1);
		input = onebyte ? test_stream_onebyte_new (stream) : g_object_ref (stream);
		parser = g_mime_parser_new_with_stream (input);
		headers = g_mime_parser_construct_headers (parser, NULL, &error);
		position = g_mime_parser_tell (parser);
		next = g_mime_parser_construct_headers (parser, NULL, &error);
		g_assert_nonnull (headers);
		g_assert_nonnull (next);
		check (error == NULL && g_mime_header_list_get_count (headers) == 1 &&
		       g_mime_header_list_get_count (next) == 1 &&
		       !strcmp (g_mime_header_get_value (g_mime_header_list_get_header (headers, "X-One")), "one") &&
		       !strcmp (g_mime_header_get_value (g_mime_header_list_get_header (next, "X-Two")), "two") &&
		       g_mime_header_get_offset (g_mime_header_list_get_header_at (next, 0)) == position &&
		       g_mime_parser_get_headers_begin (parser) == position &&
		       g_mime_parser_get_headers_end (parser) == sizeof (blocks) - 3,
		       "header-only parsing can consume successive blocks from buffered input");
		g_object_unref (headers);
		g_object_unref (next);
		for (i = 0; i < 2; i++) {
			headers = g_mime_parser_construct_headers (parser, NULL, &error);
			g_assert_nonnull (headers);
			check (error == NULL && g_mime_header_list_get_count (headers) == 0 &&
			       g_mime_parser_eos (parser) && g_mime_parser_tell (parser) == sizeof (blocks) - 1 &&
			       g_mime_parser_get_headers_begin (parser) == sizeof (blocks) - 1 &&
			       g_mime_parser_get_headers_end (parser) == -1,
			       "repeated header-only EOF returns an empty list without stale offsets or fields");
			g_object_unref (headers);
		}
		g_object_unref (parser);
		g_object_unref (input);
		g_object_unref (stream);
	}

	for (i = 0; i < G_N_ELEMENTS (empty); i++) {
		stream = g_mime_stream_mem_new_with_buffer (empty[i], strlen (empty[i]));
		headers = g_mime_header_list_load (NULL, stream, &error);
		g_assert_nonnull (headers);
		check (error == NULL && g_mime_header_list_get_count (headers) == 0,
		       "loading an empty stream or empty header block returns an empty list");
		check (g_mime_stream_reset (stream) == 0, "header loading leaves the input stream open");
		g_object_unref (stream);
		g_object_unref (headers);
	}

	stream = g_mime_stream_mem_new_with_buffer (text, sizeof (text) - 1);
	g_mime_stream_seek (stream, strstr (text, "sUbJeCt") - text, GMIME_STREAM_SEEK_SET);
	headers = g_mime_header_list_load (NULL, stream, &error);
	g_assert_nonnull (headers);
	g_object_unref (stream);
	header = g_mime_header_list_get_header_at (headers, 0);
	check (error == NULL && g_mime_header_list_get_count (headers) == 3 &&
	       g_mime_header_get_offset (header) == strstr (text, "sUbJeCt") - text &&
	       !strcmp (g_mime_header_get_value (header), "second"),
	       "header-list loading begins at the current stream offset and outlives its temporary parser");
	g_object_unref (headers);
}

static void
test_header_list_loading_errors (void)
{
	const char *fields[] = { "X: abc\n", "X \t: abc\r\n", "X-Fold:\n\tone\n two\n", "X: unterminated" };
	const char *invalid[] = { "broken", "broken\n\n", ": invalid\n\n", " Subject: invalid\n\n" };
	static const char text[] = "X-One: one\nX-Two: two\n\nunparsed body\n";
	GMimeParserOptions *options = g_mime_parser_options_new ();
	GMimeHeaderList *headers, *next;
	GMimeStream *stream, *input;
	TestStreamFailure *failure;
	GMimeParser *parser;
	GError *error = NULL;
	char *wire;
	guint i, calls;
	int onebyte, delta;

	g_test_expect_message ("gmime", G_LOG_LEVEL_CRITICAL, "*GMIME_IS_STREAM (stream)*");
	check (g_mime_header_list_load (NULL, NULL, &error) == NULL && error == NULL,
	       "header loading rejects a NULL input stream");
	g_test_assert_expected_messages ();
	g_test_expect_message ("gmime", G_LOG_LEVEL_CRITICAL, "*GMIME_IS_PARSER (parser)*");
	check (g_mime_parser_construct_headers (NULL, NULL, &error) == NULL && error == NULL,
	       "header-only parsing rejects a NULL parser");
	g_test_assert_expected_messages ();

	for (onebyte = 0; onebyte < 2; onebyte++) {
		for (i = 0; i < G_N_ELEMENTS (fields); i++) {
			wire = g_strconcat (fields[i], i == G_N_ELEMENTS (fields) - 1 ? "" : "\nbody", NULL);
			for (delta = -1; delta <= 1; delta++) {
				g_mime_parser_options_set_max_header_length (options, strlen (fields[i]) + delta);
				stream = g_mime_stream_mem_new_with_buffer (wire, strlen (wire));
				input = onebyte ? test_stream_onebyte_new (stream) : g_object_ref (stream);
				headers = g_mime_header_list_load (options, input, &error);
				if (delta < 0) {
					check (headers == NULL && g_error_matches (error, GMIME_ERROR, GMIME_ERROR_HEADER_TOO_LONG),
					       "header loading enforces the exact raw-header byte limit");
				} else {
					check (headers != NULL && error == NULL && g_mime_header_list_get_count (headers) == 1,
					       "header loading accepts raw headers at and below the configured limit");
				}
				g_clear_object (&headers);
				g_clear_error (&error);
				g_object_unref (input);
				g_object_unref (stream);
			}
			g_free (wire);
		}

		for (i = 0; i < G_N_ELEMENTS (invalid); i++) {
			stream = g_mime_stream_mem_new_with_buffer (invalid[i], strlen (invalid[i]));
			input = onebyte ? test_stream_onebyte_new (stream) : g_object_ref (stream);
			headers = g_mime_header_list_load (NULL, input, &error);
			check (headers == NULL && g_error_matches (error, GMIME_ERROR, GMIME_ERROR_PARSE_ERROR),
			       "header loading reports malformed or incomplete initial fields");
			g_clear_object (&headers);
			g_clear_error (&error);
			g_object_unref (input);
			g_object_unref (stream);
		}

		for (i = 0; i < 4; i++) {
			failure = g_object_new (test_stream_failure_get_type (), NULL);
			failure->parent_instance.source = g_mime_stream_mem_new_with_buffer (text, sizeof (text) - 1);
			failure->fail_after = i == 3 ? strstr (text, "\n\n") - text + 2 : i * 7;
			failure->onebyte = onebyte;
			failure->nonseekable = TRUE;
			g_mime_stream_construct (GMIME_STREAM (failure), 0, -1);
			parser = g_mime_parser_new_with_stream (GMIME_STREAM (failure));
			headers = g_mime_parser_construct_headers (parser, NULL, &error);
			if (i == 3) {
				check (headers != NULL && error == NULL && g_mime_header_list_get_count (headers) == 2 &&
				       failure->bytes_read == failure->fail_after,
				       "header-only parsing does not request body data after consuming the separator");
				next = g_mime_parser_construct_headers (parser, NULL, &error);
				check (next == NULL && g_error_matches (error, GMIME_ERROR, EIO) &&
				       g_mime_parser_get_headers_begin (parser) == -1 &&
				       g_mime_parser_get_headers_end (parser) == -1,
				       "failure on a subsequent header block clears the preceding block's offsets");
				g_clear_object (&next);
			} else {
				check (headers == NULL && g_error_matches (error, GMIME_ERROR, EIO),
				       "header-only parsing discards partial fields and propagates input I/O errors");
				check (g_mime_parser_get_headers_begin (parser) == -1 &&
				       g_mime_parser_get_headers_end (parser) == -1,
				       "failed header-only parsing does not expose stale header-block offsets");
				g_clear_error (&error);
				calls = failure->read_calls;
				headers = g_mime_parser_construct_headers (parser, NULL, &error);
				check (headers == NULL && g_error_matches (error, GMIME_ERROR, EIO) &&
				       failure->read_calls == calls, "a failed header parser preserves its error without reading again");
				g_clear_error (&error);
				stream = g_mime_stream_mem_new_with_buffer (text, sizeof (text) - 1);
				g_mime_parser_init_with_stream (parser, stream);
				g_object_unref (stream);
				headers = g_mime_parser_construct_headers (parser, NULL, &error);
				check (headers != NULL && error == NULL && g_mime_header_list_get_count (headers) == 2,
				       "reinitializing a failed header parser clears its error and partial input");
			}
			g_clear_object (&headers);
			g_clear_error (&error);
			g_object_unref (parser);
			g_object_unref (failure);
		}
	}
	g_mime_parser_options_free (options);
}

static void
test_header_list_file_loading (void)
{
	static const char text[] = "Subject: first\r\nsUbJeCt \t: second\r\n"
		"Content-Type: multipart/mixed; boundary=unused\r\n"
		"X-Raw: \317\360\350\342\345\362\r\n\r\nNot a MIME body";
	const char *empty[] = { "", "\n", "\r\n" };
	GMimeParserOptions *options;
	GMimeHeaderList *headers;
	GMimeHeader *header;
	GError *error = NULL;
	char *directory, *filename, *missing;
	guint i;

	directory = g_dir_make_tmp ("gmime-header-list-XXXXXX", &error);
	g_assert_no_error (error);
	g_assert_nonnull (directory);
	filename = g_build_filename (directory, "headers-\303\251 file.eml", NULL);
	missing = g_build_filename (directory, "missing.eml", NULL);

	g_test_expect_message ("gmime", G_LOG_LEVEL_CRITICAL, "*filename != NULL*");
	check (g_mime_header_list_load_file (NULL, NULL, &error) == NULL && error == NULL,
	       "filename header loading rejects a NULL filename");
	g_test_assert_expected_messages ();

	for (i = 0; i < G_N_ELEMENTS (empty); i++) {
		g_assert_true (g_file_set_contents (filename, empty[i], -1, &error));
		g_assert_no_error (error);
		headers = g_mime_header_list_load_file (NULL, filename, &error);
		check (headers != NULL && error == NULL && g_mime_header_list_get_count (headers) == 0,
		       "filename header loading accepts empty files and empty header blocks");
		g_clear_object (&headers);
	}

	g_assert_true (g_file_set_contents (filename, text, sizeof (text) - 1, &error));
	options = g_mime_parser_options_new ();
	check (g_mime_parser_options_set_charset_encoding (options, "windows-1251", NULL),
	       "set file-loaded header fallback charset");
	headers = g_mime_header_list_load_file (options, filename, &error);
	g_mime_parser_options_free (options);
	check (headers != NULL && error == NULL && g_mime_header_list_get_count (headers) == 4,
	       "filename header loading accepts UTF-8 paths and retains all headers, not the body");
	g_assert_cmpint (g_unlink (filename), ==, 0);
	if (headers) {
		header = g_mime_header_list_get_header_at (headers, 1);
		check (!strcmp (testsuite_header_raw_name (header), "sUbJeCt \t") &&
		       !strcmp (testsuite_header_raw_text (header), " second\r\n") &&
		       g_mime_header_get_offset (header) == 16,
		       "filename header loading preserves raw fields, duplicate order and file offsets");
		header = g_mime_header_list_get_header (headers, "X-Raw");
		check (!strcmp (g_mime_header_get_value (header), "\320\237\321\200\320\270\320\262\320\265\321\202"),
		       "file-loaded headers retain their data and fallback charset after file and options teardown");
		g_object_unref (headers);
	}
	g_clear_error (&error);

	headers = g_mime_header_list_load_file (NULL, missing, &error);
	check (headers == NULL && g_error_matches (error, GMIME_ERROR, ENOENT) &&
	       strstr (error->message, missing) != NULL,
	       "filename header loading reports missing files with their filename and errno");
	g_clear_object (&headers);
	g_clear_error (&error);
	check (g_mime_header_list_load_file (NULL, missing, NULL) == NULL,
	       "filename header loading can omit the error location on an open failure");
	headers = g_mime_header_list_load_file (NULL, directory, &error);
	check (headers == NULL && error != NULL && error->domain == GMIME_ERROR && error->code > 0,
	       "filename header loading reports directory open or read failures");
	g_clear_object (&headers);
	g_clear_error (&error);

	g_assert_true (g_file_set_contents (filename, "broken", -1, &error));
	headers = g_mime_header_list_load_file (NULL, filename, &error);
	check (headers == NULL && g_error_matches (error, GMIME_ERROR, GMIME_ERROR_PARSE_ERROR),
	       "filename header loading propagates fatal syntax errors");
	g_clear_object (&headers);
	g_clear_error (&error);
	check (g_mime_header_list_load_file (NULL, filename, NULL) == NULL,
	       "filename header loading can omit the error location on a parse failure");
	g_assert_cmpint (g_unlink (filename), ==, 0);

	g_assert_true (g_file_set_contents (filename, "X: abc\n\nbody", -1, &error));
	options = g_mime_parser_options_new ();
	for (i = 6; i <= 8; i++) {
		g_mime_parser_options_set_max_header_length (options, i);
		headers = g_mime_header_list_load_file (options, filename, &error);
		if (i == 6)
			check (headers == NULL && g_error_matches (error, GMIME_ERROR, GMIME_ERROR_HEADER_TOO_LONG),
			       "filename header loading propagates the configured raw-header limit");
		else
			check (headers != NULL && error == NULL && g_mime_header_list_get_count (headers) == 1,
			       "filename header loading accepts raw headers at and below the configured limit");
		g_clear_object (&headers);
		g_clear_error (&error);
	}
	g_mime_parser_options_free (options);
	g_assert_cmpint (g_unlink (filename), ==, 0);
	g_assert_cmpint (g_rmdir (directory), ==, 0);
	g_free (missing);
	g_free (filename);
	g_free (directory);
}

static void
check_header_list_separator (GMimeHeaderList *headers, GMimeFormatOptions *format, gboolean separator)
{
	GMimeStream *output = g_mime_stream_mem_new ();
	GByteArray *array = g_mime_stream_mem_get_byte_array ((GMimeStreamMem *) output);
	const char *newline = g_mime_format_options_get_newline (format);
	char *fields = g_mime_header_list_to_string (headers, format);
	char *expected = g_strconcat (fields, separator ? newline : "", NULL);
	ssize_t nwritten;

	nwritten = g_mime_header_list_write_to_stream (headers, format, output);
	check (nwritten == (ssize_t) strlen (expected) && array->len == strlen (expected) &&
	       (array->len == 0 || !memcmp (array->data, expected, array->len)),
	       "header-list writing preserves separator state, headers-only strings and normalized byte counts");
	check (g_mime_stream_write_string (output, "still open") == 10,
	       "header-list writing leaves the caller's stream open");
	g_free (expected);
	g_free (fields);
	g_object_unref (output);
}

static void
test_header_list_separators (void)
{
	const char *wires[] = { "", "\n", "\r\n", "X-One: one", "X-One: one\n",
		"X-One: one\r\n", "X-One: one\n\nbody", "X-One: one\r\n\r\nbody" };
	const char *blocks = "X-One: one\n\nX-Two: two\n";
	GMimeFormatOptions *format = g_mime_format_options_new ();
	GMimeHeaderList *headers;
	GMimeHeader *header, *replacement;
	GMimeParser *parser;
	GMimeStream *input;
	GError *error = NULL;
	guint i, op;
	int dos;

	for (dos = 0; dos < 2; dos++) {
		g_mime_format_options_set_newline_format (format, dos ? GMIME_NEWLINE_FORMAT_DOS : GMIME_NEWLINE_FORMAT_UNIX);
		headers = g_mime_header_list_new (NULL);
		check_header_list_separator (headers, format, TRUE);
		g_object_unref (headers);

		for (i = 0; i < G_N_ELEMENTS (wires); i++) {
			input = g_mime_stream_mem_new_with_buffer (wires[i], strlen (wires[i]));
			headers = g_mime_header_list_load (NULL, input, &error);
			g_assert_no_error (error);
			g_assert_nonnull (headers);
			g_object_unref (input);
			check_header_list_separator (headers, format, i == 1 || i == 2 || i >= 6);
			g_mime_format_options_add_hidden_header (format, "X-One");
			check_header_list_separator (headers, format, i == 1 || i == 2 || i >= 6);
			g_mime_format_options_clear_hidden_headers (format);
			g_object_unref (headers);
		}

		for (op = 0; op < 24; op++) {
			const char *wire = op == 23 ? "X-One: one\nX-One: duplicate\n" : "X-One: one\nX-Two: two\n";

			input = g_mime_stream_mem_new_with_buffer (wire, strlen (wire));
			headers = g_mime_header_list_load (NULL, input, &error);
			g_assert_no_error (error);
			g_assert_nonnull (headers);
			g_object_unref (input);
			header = g_mime_header_list_get_header_at (headers, 0);
			replacement = g_mime_header_new ("X-One", "replacement", NULL);
			switch (op) {
			case 0: g_mime_header_set_value (header, NULL, "changed", NULL); break;
			case 1: testsuite_header_set_raw_text (header, " changed\r\n"); break;
			case 2: g_mime_header_list_set_header_at (headers, 0, header); break;
			case 3: g_mime_header_list_remove (headers, "missing"); break;
			case 4: g_mime_header_list_remove_all (headers, "missing"); break;
			case 5: g_mime_header_list_remove_header (headers, replacement); break;
			case 6: g_mime_header_list_remove_at (headers, 2); break;
			case 7: g_mime_header_list_append (headers, "X-New", "new", NULL); break;
			case 8: g_mime_header_list_prepend (headers, "X-New", "new", NULL); break;
			case 9: g_mime_header_list_insert (headers, 1, "X-New", "new", NULL); break;
			case 10: g_mime_header_list_insert_header (headers, 1, replacement); break;
			case 11: g_mime_header_list_add (headers, replacement); break;
			case 12: g_mime_header_list_set_header_at (headers, 0, replacement); break;
			case 13: g_mime_header_list_replace (headers, header); break;
			case 14: g_mime_header_list_replace (headers, replacement); break;
			case 15: g_mime_header_list_remove (headers, "X-One"); break;
			case 16: g_mime_header_list_remove_all (headers, "X-One"); break;
			case 17: g_mime_header_list_remove_at (headers, 0); break;
			case 18: g_mime_header_list_remove_header (headers, header); break;
			case 19: g_mime_header_list_clear (headers); break;
			case 20: g_mime_header_list_set (headers, "X-One", "changed", NULL); break;
			case 21: g_mime_header_list_set (headers, "X-New", "new", NULL); break;
			case 22: g_mime_header_list_set_header_at (headers, 1, replacement); break;
			case 23: g_mime_header_list_set (headers, "X-One", "changed", NULL); break;
			}
			g_object_unref (replacement);
			check_header_list_separator (headers, format, op >= 7 && op != 20);
			g_object_unref (headers);
		}
		input = g_mime_stream_mem_new_with_buffer (blocks, strlen (blocks));
		parser = g_mime_parser_new_with_stream (input);
		g_object_unref (input);
		for (i = 0; i < 3; i++) {
			headers = g_mime_parser_construct_headers (parser, NULL, &error);
			g_assert_no_error (error);
			g_assert_nonnull (headers);
			check_header_list_separator (headers, format, i == 0);
			g_object_unref (headers);
		}
		g_object_unref (parser);
	}
	g_mime_format_options_free (format);
}

static char *
failing_header_formatter (GMimeHeader *header, GMimeFormatOptions *options, const char *value, const char *charset)
{
	return NULL;
}

static void
test_header_list_writing_errors (void)
{
	const char *expected = "X-One: one\r\n\r\n";
	GMimeFormatOptions *format = g_mime_format_options_new ();
	GMimeHeaderList *headers = g_mime_header_list_new (NULL);
	TestStreamFailure *failure;
	GMimeStream *output;
	GByteArray *array;
	GWeakRef reference;
	GObject *retained;
	GMimeHeader *header;
	GMimeMessage *message;
	GMimePart *part;
	GMimeStream *filtered;
	GMimeFilter *filter;
	char *text;
	guint limit;
	ssize_t nwritten;
	int onebyte, zero;

	g_mime_format_options_set_newline_format (format, GMIME_NEWLINE_FORMAT_DOS);
	g_mime_header_list_append (headers, "X-One", "one", NULL);
	for (onebyte = 0; onebyte < 2; onebyte++) {
		for (zero = 0; zero < 2; zero++) {
			for (limit = 0; limit <= strlen (expected); limit++) {
				output = g_mime_stream_mem_new ();
				failure = g_object_new (test_stream_failure_get_type (), NULL);
				g_mime_stream_construct (GMIME_STREAM (failure), 0, -1);
				failure->parent_instance.source = g_object_ref (output);
				failure->fail_after = limit;
				failure->onebyte = onebyte;
				failure->zero_write = zero;
				failure->nonseekable = TRUE;
				g_weak_ref_init (&reference, failure);
				errno = 0;
				nwritten = g_mime_header_list_write_to_stream (headers, format, GMIME_STREAM (failure));
				if (limit == strlen (expected))
					check (nwritten == (ssize_t) limit, "header-list writing completes short writes on a nonseekable output");
				else
					check (nwritten == -1 && errno == (zero ? EIO : ENOSPC),
					       "header-list writing propagates header and separator failures, including zero writes");
				g_object_unref (failure);
				retained = g_weak_ref_get (&reference);
				check (retained == NULL, "header-list write failures release their filter and the caller's stream reference");
				g_clear_object (&retained);
				g_weak_ref_clear (&reference);
				array = g_mime_stream_mem_get_byte_array ((GMimeStreamMem *) output);
				check (array->len == limit && (limit == 0 || !memcmp (array->data, expected, limit)),
				       "header-list failure leaves exactly the accepted output prefix");
				g_object_unref (output);
			}
		}
	}
	for (zero = 0; zero < 2; zero++) {
		for (limit = 0; limit <= 5; limit++) {
			output = g_mime_stream_mem_new ();
			failure = g_object_new (test_stream_failure_get_type (), NULL);
			g_mime_stream_construct (GMIME_STREAM (failure), 0, -1);
			failure->parent_instance.source = g_object_ref (output);
			failure->fail_after = limit;
			failure->onebyte = TRUE;
			failure->zero_write = zero;
			filtered = g_mime_stream_filter_new (GMIME_STREAM (failure));
			filter = g_mime_filter_basic_new (GMIME_CONTENT_ENCODING_BASE64, TRUE);
			g_mime_stream_filter_add (GMIME_STREAM_FILTER (filtered), filter);
			g_object_unref (filter);
			check (g_mime_stream_write (filtered, "a", 1) == 1,
			       "filter buffers an incomplete base64 input block");
			errno = 0;
			if (limit == 5)
				check (g_mime_stream_flush (filtered) == 0, "filter completion output handles short writes");
			else
				check (g_mime_stream_flush (filtered) == -1 && errno == (zero ? EIO : ENOSPC),
				       "filter completion output propagates failures and zero writes");
			array = g_mime_stream_mem_get_byte_array (GMIME_STREAM_MEM (output));
			check (array->len == limit && (limit == 0 || !memcmp (array->data, "YQ==\n", limit)),
			       "filter completion leaves exactly the accepted output prefix");
			g_object_unref (filtered);
			g_object_unref (failure);
			g_object_unref (output);
		}
	}
	header = g_mime_header_list_get_header_at (headers, 0);
	header->formatter = failing_header_formatter;
	output = g_mime_stream_mem_new ();
	errno = 0;
	check (g_mime_header_list_write_to_stream (headers, format, output) == -1 && errno == EINVAL,
	       "header-list writing propagates formatter failures");
	g_object_unref (output);
	errno = 0;
	text = g_mime_header_list_to_string (headers, format);
	check (text == NULL && errno == EINVAL, "header-list to_string does not return partial text on formatter failure");
	g_free (text);
	message = g_mime_message_new (FALSE);
	g_mime_header_list_add (g_mime_entity_get_header_list (GMIME_ENTITY (message)), header);
	errno = 0;
	text = g_mime_entity_get_headers (GMIME_ENTITY (message), format);
	check (text == NULL && errno == EINVAL, "message get_headers does not return partial text on formatter failure");
	g_free (text);
	part = g_mime_part_new ();
	g_mime_message_set_mime_part (message, GMIME_ENTITY (part));
	g_object_unref (part);
	errno = 0;
	text = g_mime_entity_get_headers (GMIME_ENTITY (message), format);
	check (text == NULL && errno == EINVAL, "merged message get_headers propagates formatter failures");
	g_free (text);
	g_object_unref (message);
	g_object_unref (headers);
	g_mime_format_options_free (format);
}

static void
test_entity_header_separators (void)
{
	const char *wires[] = {
		"Content-Type: text/plain",
		"Content-Type: text/plain\n",
		"Content-Type: text/plain\n\n",
		"Content-Type: text/plain\n\nbody",
		"Content-Type: multipart/mixed; boundary=b\n\n--b--\n",
		"Content-Type: message/rfc822\n\nSubject: nested\n\nbody"
	};
	const char *truncated = "Content-Type: multipart/mixed; boundary=b\n\n--b\nContent-Type: text/plain\n--b--\n";
	GMimeFormatOptions *format = g_mime_format_options_new ();
	GMimeEntity *entity;
	GMimeMessage *message;
	GMimeParser *parser;
	GMimeStream *input;
	GError *error = NULL;
	const char *separator;
	char *text, *fields, *expected;
	guint i;
	int as_message;

	g_mime_format_options_set_newline_format (format, GMIME_NEWLINE_FORMAT_UNIX);
	for (as_message = 0; as_message < 2; as_message++) {
		for (i = 0; i < G_N_ELEMENTS (wires); i++) {
			input = g_mime_stream_mem_new_with_buffer (wires[i], strlen (wires[i]));
			parser = g_mime_parser_new_with_stream (input);
			if (as_message)
				entity = GMIME_ENTITY (g_mime_parser_construct_message (parser, NULL, &error));
			else
				entity = g_mime_parser_construct_part (parser, NULL, &error);
			g_assert_no_error (error);
			g_assert_nonnull (entity);
			g_object_unref (parser);
			g_object_unref (input);
			text = g_mime_entity_to_string (entity, format);
			check (!strcmp (text, wires[i]), "entity/message writers preserve absent separators and write present separators exactly once");
			separator = strstr (wires[i], "\n\n");
			expected = separator ? g_strndup (wires[i], separator - wires[i] + 1) : g_strdup (wires[i]);
			fields = g_mime_entity_get_headers (entity, format);
			check (!strcmp (fields, expected), "entity/message get_headers remains headers-only");
			g_free (expected);
			g_free (fields);
			g_free (text);
			g_object_unref (entity);
		}
	}

	input = g_mime_stream_mem_new_with_buffer (truncated, strlen (truncated));
	parser = g_mime_parser_new_with_stream (input);
	entity = g_mime_parser_construct_part (parser, NULL, &error);
	g_assert_no_error (error);
	g_assert_true (GMIME_IS_MULTIPART (entity));
	check_header_list_separator (g_mime_entity_get_header_list (g_mime_multipart_get_part (GMIME_MULTIPART (entity), 0)),
				     format, FALSE);
	g_object_unref (entity);
	g_object_unref (parser);
	g_object_unref (input);

	message = g_mime_message_new (FALSE);
	text = g_mime_entity_to_string (GMIME_ENTITY (message), format);
	check (!strcmp (text, "\n"), "a newly constructed empty message writes one separator without a body");
	g_free (text);
	fields = g_mime_entity_get_headers (GMIME_ENTITY (message), format);
	check (!strcmp (fields, ""), "a body-less message's get_headers does not acquire a separator");
	g_free (fields);
	g_object_unref (message);
	g_mime_format_options_free (format);
}

static void
test_type_names (void)
{
	GType types[] = {
		GMIME_TYPE_ENTITY, GMIME_TYPE_CONTENT, GMIME_TYPE_INTERNET_ADDRESS,
		GMIME_TYPE_MAILBOX_ADDRESS, GMIME_TYPE_GROUP_ADDRESS, GMIME_TYPE_INTERNET_ADDRESS_LIST
	};
	const char *names[] = {
		"GMimeEntity", "GMimeContent", "GMimeInternetAddress",
		"GMimeMailboxAddress", "GMimeGroupAddress", "GMimeInternetAddressList"
	};
	GMimeInternetAddress *mailbox, *group;
	GMimeInternetAddressList *list;
	GMimeMessage *message;
	guint i;

	for (i = 0; i < G_N_ELEMENTS (types); i++)
		check (!strcmp (g_type_name (types[i]), names[i]), names[i]);

	mailbox = g_mime_mailbox_address_new ("Example", "example@example.com", NULL);
	group = g_mime_group_address_new ("Group");
	list = g_mime_internet_address_list_new ();
	message = g_mime_message_new (FALSE);

	check (GMIME_IS_MAILBOX_ADDRESS (mailbox), "mailbox type");
	check (GMIME_IS_INTERNET_ADDRESS (mailbox), "mailbox base type");
	check (GMIME_IS_GROUP_ADDRESS (group), "group type");
	check (GMIME_IS_INTERNET_ADDRESS_LIST (list), "address list type");
	check (GMIME_IS_ENTITY (message), "message base type");

	g_mime_group_address_add_member (GMIME_GROUP_ADDRESS (group), mailbox);
	g_mime_internet_address_list_add (list, group);
	g_mime_message_add_mailbox (message, GMIME_ADDRESS_TYPE_FROM, "Example", "example@example.com", NULL);
	check (g_mime_internet_address_list_length (g_mime_message_get_from (message)) == 1,
	       "renamed address API updates message headers");

	g_object_unref (message);
	g_object_unref (list);
	g_object_unref (group);
	g_object_unref (mailbox);
}

static char *
header_wire_string (GMimeHeader *header, GMimeFormatOptions *options)
{
	GMimeStream *stream = g_mime_stream_mem_new ();
	GByteArray *bytes = g_mime_stream_mem_get_byte_array (GMIME_STREAM_MEM (stream));
	char *str;

	check (g_mime_header_write_to_stream (header, options, stream) >= 0, "write standalone header");
	str = bytes->len ? g_strndup ((const char *) bytes->data, bytes->len) : g_strdup ("");
	g_object_unref (stream);

	return str;
}

static void
test_mixed_header_charsets (void)
{
	static const struct {
		const char *charset, *input, *single, *mixed;
	} cases[] = {
		{ NULL, "\303\251", "=?utf-8?b?w6k=?=", "=?iso-8859-1?b?6Q==?=" },
		{ "utf-8", "\303\251", "=?utf-8?b?w6k=?=", "=?iso-8859-1?b?6Q==?=" },
		{ "us-ascii", "\303\251", "=?utf-8?b?w6k=?=", "=?iso-8859-1?b?6Q==?=" },
		{ "iso-8859-1", "\303\251", "=?iso-8859-1?b?6Q==?=", "=?iso-8859-1?b?6Q==?=" },
		{ NULL, "=?", "=?utf-8?q?=3D=3F?=", "=?us-ascii?q?=3D=3F?=" },
		{ "utf-8", "=?", "=?utf-8?q?=3D=3F?=", "=?us-ascii?q?=3D=3F?=" },
		{ "iso-8859-1", "=?", "=?iso-8859-1?q?=3D=3F?=", "=?us-ascii?q?=3D=3F?=" },
		{ "utf-16", "=?", "=?utf-16?q?=3D=00=3F=00?=", "=?us-ascii?q?=3D=3F?=" },
		{ "iso-2022-jp", "=?", "=?iso-2022-jp?b?PT8=?=", "=?us-ascii?q?=3D=3F?=" },
		{ "us-ascii", "\320\226", "=?utf-8?b?0JY=?=", "=?utf-8?b?0JY=?=" },
		{ "iso-8859-5", "\320\226", "=?iso-8859-5?b?tg==?=", "=?iso-8859-5?b?tg==?=" },
		{ NULL, "\320\226", "=?utf-8?b?0JY=?=", "=?utf-8?b?0JY=?=" },
		{ NULL, "\303\251 one two three four \320\226",
		  "=?utf-8?b?w6k=?= one two three four =?utf-8?b?0JY=?=",
		  "=?iso-8859-1?b?6Q==?= one two three four =?utf-8?b?0JY=?=" }
	};
	static const struct {
		const char *name, *value;
	} headers[] = {
		{ "Subject", "caf\303\251" },
		{ "From", "caf\303\251 <user@example.com>" },
		{ "List-Post", "(caf\303\251) <a>" }
	};
	GMimeFormatOptions *options = g_mime_format_options_new (), *clone;
	GMimeHeader *header, *copy;
	const char *stored, *expected;
	char *encoded, *decoded, *wire;
	guint i, mixed, phrase;

	check (!g_mime_format_options_get_allow_mixed_charsets (options) &&
	       !g_mime_format_options_get_allow_mixed_charsets (NULL),
	       "mixed header charsets are disabled by default");
	g_mime_format_options_set_allow_mixed_charsets (options, TRUE);
	clone = g_mime_format_options_clone (options);
	g_mime_format_options_set_allow_mixed_charsets (options, FALSE);
	check (g_mime_format_options_get_allow_mixed_charsets (clone),
	       "format option clones retain an independent mixed-charset policy");
	g_mime_format_options_free (clone);
	clone = g_mime_format_options_clone (NULL);
	check (!g_mime_format_options_get_allow_mixed_charsets (clone),
	       "cloning the default preserves the disabled mixed-charset policy");
	g_mime_format_options_free (clone);
	g_test_expect_message ("gmime", G_LOG_LEVEL_CRITICAL, "*options != NULL*");
	g_mime_format_options_set_allow_mixed_charsets (NULL, TRUE);
	g_test_assert_expected_messages ();

	for (mixed = 0; mixed < 2; mixed++) {
		g_mime_format_options_set_allow_mixed_charsets (options, mixed != 0);
		for (phrase = 0; phrase < 2; phrase++) {
			for (i = 0; i < G_N_ELEMENTS (cases); i++) {
				encoded = phrase ? g_mime_utils_header_encode_phrase (options, cases[i].input, cases[i].charset) :
					g_mime_utils_header_encode_text (options, cases[i].input, cases[i].charset);
				expected = mixed ? cases[i].mixed : cases[i].single;
				check (!strcmp (encoded, expected), "RFC 2047 encoding obeys the mixed-charset policy");
				decoded = phrase ? g_mime_utils_header_decode_phrase (NULL, encoded) :
					g_mime_utils_header_decode_text (NULL, encoded);
				check (!strcmp (decoded, cases[i].input), "charset selection retains lossless decoded text");
				g_free (decoded);
				g_free (encoded);
			}
		}
		for (i = 0; i < G_N_ELEMENTS (headers); i++) {
			header = g_mime_header_new (headers[i].name, headers[i].value, "utf-8");
			g_mime_format_options_set_allow_international (options, FALSE);
			g_mime_header_set_value (header, options, headers[i].value, "utf-8");
			stored = testsuite_header_raw_text (header);
			encoded = g_ascii_strdown (stored, -1);
			check (strstr (encoded, mixed ? "=?iso-8859-1?" : "=?utf-8?") != NULL,
			       "text, address and comment headers use the selected charset policy");
			g_free (encoded);
			g_mime_format_options_set_allow_international (options, TRUE);
			wire = header_wire_string (header, options);
			check (strstr (wire, "caf\303\251") != NULL && strstr (wire, "=?") == NULL,
			       "International output bypasses mixed-charset selection");
			check (testsuite_header_raw_text (header) == stored,
			       "International output leaves the assigned raw value unchanged");
			g_free (wire);
			testsuite_header_set_raw_text (header, " =?iso-8859-1?q?caf=E9?=\n");
			copy = g_mime_header_clone (header);
			g_object_unref (header);
			wire = header_wire_string (copy, options);
			check (strstr (wire, "=?iso-8859-1?q?caf=E9?=") != NULL,
			       "explicit raw clones bypass charset and International policies");
			g_free (wire);
			g_object_unref (copy);
		}
	}
	g_mime_format_options_free (options);
}

static void
check_phrase_group (GMimeFormatOptions *options, const char *value, const char *expected)
{
	GMimeInternetAddressList *parsed;
	GMimeInternetAddress *mailbox;
	char *encoded, *decoded, *wire;

	encoded = g_mime_utils_header_encode_phrase (options, value, "utf-8");
	if (expected)
		check (!strcmp (encoded, expected), "phrase group encoding matches MimeKit");
	wire = g_strconcat (encoded, " <a@b>", NULL);
	parsed = g_mime_internet_address_list_parse (NULL, wire);
	g_assert_nonnull (parsed);
	check (g_mime_internet_address_list_length (parsed) == 1 &&
	       !g_strcmp0 (g_mime_internet_address_get_name (
		       g_mime_internet_address_list_get_address (parsed, 0)), value),
	       "encoded phrase groups retain every literal character when parsed");
	g_object_unref (parsed);
	g_free (wire);
	g_free (encoded);

	mailbox = g_mime_mailbox_address_new (value, "a@b", NULL);
	g_mime_internet_address_set_charset (mailbox, "utf-8");
	wire = g_mime_internet_address_to_string (mailbox, options, TRUE);
	parsed = g_mime_internet_address_list_parse (NULL, wire);
	g_assert_nonnull (parsed);
	check (g_mime_internet_address_list_length (parsed) == 1 &&
	       !g_strcmp0 (g_mime_internet_address_get_name (
		       g_mime_internet_address_list_get_address (parsed, 0)), value),
	       "mailbox serialization preserves literal phrase groups across folding");
	g_object_unref (parsed);
	g_object_unref (mailbox);
	g_free (wire);

	encoded = g_mime_utils_header_encode_text (options, value, "utf-8");
	decoded = g_mime_utils_header_decode_text (NULL, encoded);
	check (!strcmp (decoded, value), "phrase group handling leaves unstructured text intact");
	g_free (decoded);
	g_free (encoded);
}

static void
test_phrase_groups (void)
{
	static const struct {
		const char *value, *encoded;
	} cases[] = {
		{ "\"a \303\251 b\"", "=?utf-8?b?ImEgw6kgYiI=?=" },
		{ "(a \303\251 b)", "=?utf-8?b?KGEgw6kgYik=?=" },
		{ "(a (\303\251) b)", "=?utf-8?b?KGEgKMOpKSBiKQ==?=" },
		{ "\"a\\\" \303\251 b\"", "=?utf-8?b?ImFcIiDDqSBiIg==?=" },
		{ "(a \"b\" \303\251)", "=?utf-8?b?KGEgImIiIMOpKQ==?=" },
		{ "\"a\tb\"", "=?utf-8?q?=22a=09b=22?=" },
		{ "(a\tb)", "=?utf-8?q?=28a=09b=29?=" },
		{ "\"a\"\303\251", "=?utf-8?b?ImEiw6k=?=" },
		{ "(a)\303\251", "=?utf-8?b?KGEpw6k=?=" },
		{ "foo(a \303\251)bar", "=?utf-8?q?foo=28a_=C3=A9=29bar?=" },
		{ "\"a\" \303\251", "\"\\\"a\\\"\" =?utf-8?b?w6k=?=" },
		{ "\"unclosed \303\251", "=?utf-8?q?=22unclosed_=C3=A9?=" },
		{ "(unclosed \303\251", "=?utf-8?q?=28unclosed_=C3=A9?=" },
		{ "(\303\251) \"a\"", "=?utf-8?b?KMOpKQ==?= \"\\\"a\\\"\"" },
		{ "(a)(\303\251)", "=?utf-8?b?KGEpKMOpKQ==?=" },
		{ "\"a\"\"\303\251\"", "=?utf-8?b?ImEiIsOpIg==?=" },
		{ "(a (\303\251) (b c))", "=?utf-8?q?=28a_=28=C3=A9=29_=28b_c=29=29?=" },
		{ "(a \"b \303\251\" c)", "=?utf-8?q?=28a_=22b_=C3=A9=22_c=29?=" },
		{ "\"=?utf-8?q?x?=\"", "=?utf-8?q?=22=3D=3Futf-8=3Fq=3Fx=3F=3D=22?=" },
		{ "\"a \360\237\230\200 b\"", "=?utf-8?b?ImEg8J+YgCBiIg==?=" },
		{ "a\\(b \303\251)", "=?utf-8?b?YVwoYiDDqSk=?=" },
		{ "\"a\\x\" \303\251", "=?utf-8?b?ImFceCIgw6k=?=" }
	};
	static const char *groups[][2] = { { "\"", "\"" }, { "(", ")" }, { "(((", ")))" } };
	static const char *glyphs[] = { "", "\303\251", "\346\227\245", "\360\237\230\200" };
	static const guint lengths[] = { 0, 37, 38, 39, 40, 41, 73, 74, 75, 76, 77, 78, 153 };
	static const guint widths[] = { 60, 78, 998 };
	GMimeFormatOptions *options = g_mime_format_options_new ();
	char *prefix, *value, *expected;
	guint width, mixed, i, group, glyph, length, position;

	for (width = 0; width < G_N_ELEMENTS (widths); width++) {
		g_mime_format_options_set_max_line_length (options, widths[width]);
		for (mixed = 0; mixed < 2; mixed++) {
			g_mime_format_options_set_allow_mixed_charsets (options, mixed != 0);
			for (i = 0; i < G_N_ELEMENTS (cases); i++)
				check_phrase_group (options, cases[i].value, mixed ? NULL : cases[i].encoded);
			check_phrase_group (options, "\"a\\\\\" \303\251", NULL);
			for (group = 0; group < G_N_ELEMENTS (groups); group++) {
				for (glyph = 0; glyph < G_N_ELEMENTS (glyphs); glyph++) {
					for (length = 0; length < G_N_ELEMENTS (lengths); length++) {
						prefix = g_strnfill (lengths[length], 'a');
						for (position = 0; position < 2; position++) {
							value = g_strconcat (groups[group][0], position ? "" : prefix,
									    glyphs[glyph], position ? prefix : "", groups[group][1], NULL);
							expected = NULL;
							if (glyph == 0)
								expected = group == 0 ? g_strconcat ("\"\\\"", prefix, "\\\"\"", NULL)
									: g_mime_utils_quote_string (value);
							check_phrase_group (options, value, expected);
							g_free (expected);
							g_free (value);
						}
						if (glyph > 0) {
							value = g_strconcat (groups[group][0], prefix, glyphs[glyph],
									    prefix, groups[group][1], NULL);
							check_phrase_group (options, value, NULL);
							g_free (value);
							value = g_strconcat (groups[group][0], prefix, groups[group][1],
									    glyphs[glyph], NULL);
							check_phrase_group (options, value, NULL);
							g_free (value);
						}
						g_free (prefix);
					}
				}
			}
		}
	}
	g_mime_format_options_free (options);
}

static void
test_rfc2047_word_merging (void)
{
	static const struct {
		guint width;
		const char *charset, *value, *phrase, *text;
	} cases[] = {
		{ 60, "utf-8", "\346\227\245 aaaaaaaaaaaaaaaaaaaaaaaaaaa \346\227\245",
		  "=?utf-8?q?=E6=97=A5_aaaaaaaaaaaaaaaaaaaaaaaaaaa?=\t=?utf-8?b?IOaXpQ==?=" },
		{ 61, "utf-8", "\346\227\245 aaaaaaaaaaaaaaaaaaaaaaaaaaa \346\227\245",
		  "=?utf-8?q?=E6=97=A5_aaaaaaaaaaaaaaaaaaaaaaaaaaa?=\t=?utf-8?b?IOaXpQ==?=" },
		{ 62, "utf-8", "\346\227\245 aaaaaaaaaaaaaaaaaaaaaaaaaaa \346\227\245",
		  "=?utf-8?b?5pelIGFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYSDml6U=?=" },
		{ 60, "utf-8", "=? aaaaaaaaaaaaaaaaaaaaaaaaaaaaaa =?",
		  "=?utf-8?q?=3D=3F_aaaaaaaaaaaaaaaaaaaaaaaaaaaaaa?=\t=?utf-8?q?_=3D=3F?=" },
		{ 61, "utf-8", "=? aaaaaaaaaaaaaaaaaaaaaaaaaaaaaa =?",
		  "=?utf-8?q?=3D=3F_aaaaaaaaaaaaaaaaaaaaaaaaaaaaaa_=3D=3F?=" },
		{ 60, "utf-8", "\346\227\245 one two three \346\227\245",
		  "=?utf-8?b?5pelIG9uZSB0d28gdGhyZWUg5pel?=" },
		{ 60, "utf-8", "\346\227\245 one two three four \346\227\245",
		  "=?utf-8?b?5pel?= one two three four =?utf-8?b?5pel?=" },
		{ 60, "utf-8", "\346\227\245 \346\227\245 \346\227\245 \346\227\245 \346\227\245 "
		  "\346\227\245 \346\227\245 \346\227\245 \346\227\245",
		  "=?utf-8?b?5pelIOaXpSDml6Ug5pelIOaXpSDml6Ug5pelIOaXpQ==?=\t=?utf-8?b?IOaXpQ==?=" },
		{ 60, "iso-2022-jp", "\346\227\245 \346\227\245 \346\227\245 \346\227\245 \346\227\245 "
		  "\346\227\245 \346\227\245 \346\227\245 \346\227\245",
		  "=?iso-2022-jp?b?GyRCRnwbKEIgGyRCRnwbKEIgGyRCRnwbKEI=?=\t"
		  "=?iso-2022-jp?b?IBskQkZ8GyhCIBskQkZ8GyhCIBskQkZ8GyhC?=\t"
		  "=?iso-2022-jp?b?IBskQkZ8GyhCIBskQkZ8GyhCIBskQkZ8GyhC?=" },
		{ 60, "utf-16", "\346\227\245 \346\227\245 \346\227\245 \346\227\245 \346\227\245 "
		  "\346\227\245 \346\227\245 \346\227\245 \346\227\245",
		  "=?utf-16?b?5WUgAOVlIADlZSAA5WUgAOVlIADlZSAA5WUgAOVlIADlZQ==?=",
		  "=?utf-16?b?5WUgAOVlIADlZSAA5WUgAA==?= =?utf-16?b?5WUgAOVlIADlZSAA5WUgAA==?= =?utf-16?b?5WU=?=" }
	};
	GMimeFormatOptions *options = g_mime_format_options_new ();
	char *encoded, *decoded;
	GString *expected;
	guint i, mixed, phrase;

	for (i = 0; i < G_N_ELEMENTS (cases); i++) {
		g_mime_format_options_set_max_line_length (options, cases[i].width);
		for (mixed = 0; mixed < 2; mixed++) {
			g_mime_format_options_set_allow_mixed_charsets (options, mixed != 0);
			for (phrase = 0; phrase < 2; phrase++) {
				expected = g_string_new (!phrase && cases[i].text ? cases[i].text : cases[i].phrase);
				if (mixed && g_str_has_prefix (cases[i].value, "=?"))
					g_string_replace (expected, "=?utf-8?", "=?us-ascii?", 0);
				if (!phrase)
					g_strdelimit (expected->str, "\t", ' ');
				encoded = phrase ? g_mime_utils_header_encode_phrase (options, cases[i].value, cases[i].charset) :
					g_mime_utils_header_encode_text (options, cases[i].value, cases[i].charset);
				testsuite_check ("RFC 2047 merging [%u/%u/%u]", i, mixed, phrase);
				if (!strcmp (encoded, expected->str))
					testsuite_check_passed ();
				else
					testsuite_check_failed ("actual=\"%s\", expected=\"%s\"", encoded, expected->str);
				decoded = phrase ? g_mime_utils_header_decode_phrase (NULL, encoded) :
					g_mime_utils_header_decode_text (NULL, encoded);
				check (!strcmp (decoded, cases[i].value), "encoded-word merging retains original whitespace and text");
				g_free (decoded);
				g_free (encoded);
				g_string_free (expected, TRUE);
			}
		}
	}
	g_mime_format_options_free (options);
}

static void
test_rfc2047_charset_aliases (void)
{
	static const struct {
		const char *alias, *canonical, *glyph;
	} cases[] = {
		{ "UTF8", "utf-8", "\346\227\245" },
		{ "ASCII", "us-ascii", "=?" },
		{ "ANSI_X3.4-1968", "us-ascii", "=?" },
		{ "iso-ir-6", "us-ascii", "=?" },
		{ "LATIN1", "iso-8859-1", "\303\251" },
		{ "ISO_8859-1", "iso-8859-1", "\303\251" },
		{ "ISO8859-5", "iso-8859-5", "\320\226" },
		{ "latin2", "iso-8859-2", "\305\201" },
		{ "latin9", "iso-8859-15", "\342\202\254" },
		{ "ansi_x3.110-1983", "iso-8859-1", "\303\251" },
		{ "CP1251", "windows-1251", "\320\226" },
		{ "cp1251", "windows-1251", "\320\226" },
		{ "windows-cp1251", "windows-1251", "\320\226" },
		{ "microsoft-cp1251", "windows-1251", "\320\226" },
		{ "cp1252", "windows-1252", "\342\202\254" },
		{ "SHIFT-JIS", "shift_jis", "\346\227\245" },
		{ "SJIS", "shift_jis", "\346\227\245" },
		{ "pck", "shift_jis", "\346\227\245" },
		{ "eucjp-0", "euc-jp", "\346\227\245" },
		{ "ujis-0", "euc-jp", "\346\227\245" },
		{ "UJIS", "euc-jp", "\346\227\245" },
		{ "ks_c_5601-1987", "euc-kr", "\355\225\234" },
		{ "ksc-5601_1987", "euc-kr", "\355\225\234" },
		{ "euckr-0", "euc-kr", "\355\225\234" },
		{ "gb2312-0", "gb2312", "\344\270\255" },
		{ "gb-2312", "gb2312", "\344\270\255" },
		{ "gbk-0", "euc-cn", "\344\270\255" },
		{ "GBK", "euc-cn", "\344\270\255" },
		{ "big5-0", "big5", "\344\270\255" },
		{ "big5.eten-0", "big5", "\344\270\255" },
		{ "BIG5HKSCS-0", "big5-hkscs", "\360\240\200\241" },
		{ "gb18030-0", "gb18030", "\344\270\255" }
	};
	static const guint widths[] = { 60, 61, 78, 120, 998 };
	GMimeFormatOptions *options = g_mime_format_options_new ();
	GMimeHeader *header;
	char *encoded, *expected, *decoded, *prefix, *raw;
	GString *value;
	guint i, j, width, mixed, phrase;

	for (i = 0; i < G_N_ELEMENTS (cases); i++) {
		prefix = g_strdup_printf ("=?%s?", cases[i].canonical);
		encoded = g_mime_utils_header_encode_text (NULL, cases[i].glyph, cases[i].alias);
		check (g_str_has_prefix (encoded, prefix), "common charset aliases emit canonical MIME labels");
		decoded = g_mime_utils_header_decode_text (NULL, encoded);
		check (!strcmp (decoded, cases[i].glyph), "canonical MIME labels describe the emitted bytes");
		g_free (decoded);
		raw = g_strdup_printf ("=?%s?%s", cases[i].alias, strchr (encoded + 2, '?') + 1);
		decoded = g_mime_utils_header_decode_text (NULL, raw);
		check (!strcmp (decoded, cases[i].glyph), "shared aliases decode the same byte repertoire as their MIME labels");
		g_free (decoded);
		g_free (raw);
		g_free (encoded);
		g_free (prefix);

		for (width = 0; width < G_N_ELEMENTS (widths); width++) {
			g_mime_format_options_set_max_line_length (options, widths[width]);
			for (mixed = 0; mixed < 2; mixed++) {
				g_mime_format_options_set_allow_mixed_charsets (options, mixed != 0);
				for (phrase = 0; phrase < 2; phrase++) {
					value = g_string_new (NULL);
					for (j = 0; j < 80; j++)
						g_string_append (value, cases[i].glyph);
					g_string_append (value, " alpha \346\227\245");
					encoded = phrase ? g_mime_utils_header_encode_phrase (options, value->str, cases[i].alias) :
						g_mime_utils_header_encode_text (options, value->str, cases[i].alias);
					expected = phrase ? g_mime_utils_header_encode_phrase (options, value->str, cases[i].canonical) :
						g_mime_utils_header_encode_text (options, value->str, cases[i].canonical);
					check (!strcmp (encoded, expected), "charset aliases use canonical splitting and merging boundaries");
					decoded = g_mime_utils_header_decode_text (NULL, encoded);
					check (!strcmp (decoded, value->str), "charset alias normalization preserves lossless fallback");
					g_free (decoded);
					g_free (expected);
					g_free (encoded);
					g_string_free (value, TRUE);
				}
			}
		}
	}
	g_mime_format_options_free (options);

	check (!strcmp (g_mime_charset_canon_name ("cp1251"), "windows-cp1251"),
	       "canonical Windows code-page lookup is case insensitive");
	check (!strcmp (g_mime_charset_canon_name ("cp1251junk"), "cp1251junk"),
	       "canonical Windows code-page lookup does not accept a trailing suffix");

	header = g_mime_header_new ("Subject", "\320\226", "cp1251");
	check (!strcmp (header->charset, "cp1251") &&
	       strstr (testsuite_header_raw_text (header), "=?windows-1251?") != NULL,
	       "canonical emission does not replace a header's configured charset");
	testsuite_header_set_raw_text (header, " =?CP1251?b?xg==?=\n");
	check (!strcmp (testsuite_header_raw_text (header), " =?CP1251?b?xg==?=\n"),
	       "explicit raw headers retain their charset spelling");
	g_object_unref (header);
}

static void
count_rfc2047_warning (gint64 offset, GMimeParserWarning warning, const gchar *item, gpointer user_data)
{
	guint *count = user_data;

	check (offset == -1 && warning == GMIME_WARN_INVALID_RFC2047_HEADER_VALUE,
	       "tolerated encoded-word payload specials retain their warning");
	(*count)++;
}

static void
test_rfc2047_token_recovery (void)
{
	static const struct {
		const char *input, *loose, *strict;
	} cases[] = {
		{ "=?utf-8?Q?A=00B?=", "A\357\277\275B", "A\357\277\275B" },
		{ "=?utf-8?B?QQBC?=", "A\357\277\275B", "A\357\277\275B" },
		{ "=?iso-8859-1?Q?A=00B?=", "A\357\277\275B", "A\357\277\275B" },
		{ "=?x-unknown?Q?A=00B?=", "A\357\277\275B", "A\357\277\275B" },
		{ "=?utf-16le?B?QQAAAEIA?=", "A\357\277\275B", "A\357\277\275B" },
		{ "=?utf-8?Q?=00=00?= =?utf-8?Q?B=00?=",
		  "\357\277\275\357\277\275B\357\277\275", "\357\277\275\357\277\275B\357\277\275" },
		{ "=?utf-8?Q?bad =?utf-8?Q?ok?=", "=?utf-8?Q?bad ok", "=?utf-8?Q?bad ok" },
		{ "=?utf-8?Q?lost=?utf-8?B?QQ==?=", "=?utf-8?Q?lostA", "lost=?utf-8?B?QQ=" },
		{ "=?utf-8?X?lost =?utf-8?Q?ok?=", "=?utf-8?X?lost ok", "=?utf-8?X?lost ok" },
		{ "=?broken =?utf-8?Q?ok?=", "=?broken ok", "=?broken ok" },
		{ "=?utf-8?Q?bad =?utf-8?Q?ok?= tail",
		  "=?utf-8?Q?bad ok tail", "=?utf-8?Q?bad ok tail" },
		{ "=?utf-8*en-US?q?hello_world?=", "hello world", "hello world" },
		{ "=?utf-8*?Q?=41?=", "A", "A" },
		{ "=?utf-8*en*US?Q?=41?=", "A", "A" },
		{ "=?utf-8?Q?" "?= \t=?utf-8?Q?=41?=", "A", "A" },
		{ "=?utf-8?Q?=41?= \t=?utf-8?Q?=42?= ", "AB ", "AB " },
		{ "=?utf-8?Q?abc=?=", "abc", "abc" },
		{ "=?utf-8?Q?abc=?", "=?utf-8?Q?abc=?", "=?utf-8?Q?abc=?" },
		{ "=?bad charset?Q?=41?=", "=?bad charset?Q?=41?=", "=?bad charset?Q?=41?=" },
		{ "=?utf:8?Q?=41?=", "=?utf:8?Q?=41?=", "=?utf:8?Q?=41?=" },
		{ "=?\"utf-8\"?Q?=41?=", "=?\"utf-8\"?Q?=41?=", "=?\"utf-8\"?Q?=41?=" },
		{ "=?utf-8*en US?Q?=41?=", "=?utf-8*en US?Q?=41?=", "=?utf-8*en US?Q?=41?=" },
		{ "=?utf-8*en:US?Q?=41?=", "=?utf-8*en:US?Q?=41?=", "=?utf-8*en:US?Q?=41?=" },
		{ "=?utf-8*en.US?Q?=41?=", "=?utf-8*en.US?Q?=41?=", "=?utf-8*en.US?Q?=41?=" },
		{ "=?utf\303\2518?Q?=41?=", "=?utf\303\2518?Q?=41?=", "=?utf\303\2518?Q?=41?=" },
		{ "=?utf-8*\303\251?Q?=41?=", "=?utf-8*\303\251?Q?=41?=", "=?utf-8*\303\251?Q?=41?=" },
		{ "=??Q?=41?=", "=??Q?=41?=", "=??Q?=41?=" },
		{ "=?*en?Q?=41?=", "=?*en?Q?=41?=", "=?*en?Q?=41?=" },
		{ "=?utf-8?QQ?=41?=", "=?utf-8?QQ?=41?=", "=?utf-8?QQ?=41?=" },
		{ "=?utf-8?Q?literal_=?bad_Q?tail?=", "literal =?bad Q?tail", "literal =?bad Q?tail" }
	};
	static const struct {
		const char *input, *decoded;
	} adjacent[] = {
		{ "\303\251=?utf-8?Q?=41?=", "\303\251A" },
		{ "=?utf-8?Q?=41?=\303\251", "A\303\251" },
		{ "\303\251=?utf-8?Q?=41?=\303\251", "\303\251A\303\251" }
	};
	const char *word = "=?utf-8*en?Q?=41?=";
	const char *raw = "Subject: =?utf-8?Q?bad =?UTF-8*en?b?QQ==?=\n";
	GMimeParserOptions *options = g_mime_parser_options_new ();
	char *decoded, *input, *charset, *folded;
	const char *expected;
	guint mode, phrase, i, n, warnings;

	for (mode = 0; mode < 2; mode++) {
		g_mime_parser_options_set_rfc2047_compliance_mode (options,
			mode ? GMIME_RFC_COMPLIANCE_STRICT : GMIME_RFC_COMPLIANCE_LOOSE);
		for (phrase = 0; phrase < 2; phrase++) {
			for (i = 0; i < G_N_ELEMENTS (cases); i++) {
				expected = mode ? cases[i].strict : cases[i].loose;
				decoded = phrase ? g_mime_utils_header_decode_phrase (options, cases[i].input) :
					g_mime_utils_header_decode_text (options, cases[i].input);
				testsuite_check ("encoded-word recovery [%u,%u,%u]", mode, phrase, i);
				if (!strcmp (decoded, expected))
					testsuite_check_passed ();
				else
					testsuite_check_failed ("actual=\"%s\", expected=\"%s\"", decoded, expected);
				g_free (decoded);
			}
			for (i = 0; i < G_N_ELEMENTS (adjacent); i++) {
				expected = mode && !phrase ? adjacent[i].input : adjacent[i].decoded;
				decoded = phrase ? g_mime_utils_header_decode_phrase (options, adjacent[i].input) :
					g_mime_utils_header_decode_text (options, adjacent[i].input);
				check (!strcmp (decoded, expected), "phrase tokens separate adjacent raw UTF-8 in both modes");
				g_free (decoded);
			}
			for (n = 0; n <= strlen (word); n++) {
				input = g_strndup (word, n);
				expected = n == strlen (word) ? "A" : input;
				decoded = phrase ? g_mime_utils_header_decode_phrase (options, input) :
					g_mime_utils_header_decode_text (options, input);
				check (!strcmp (decoded, expected), "every encoded-word truncation stays within its buffer");
				g_free (decoded);
				if (!phrase) {
					char *wire = g_strconcat ("Subject:", input, NULL);
					GMimeStream *stream = g_mime_stream_mem_new_with_buffer (wire, strlen (wire));
					GError *error = NULL;
					GMimeHeaderList *headers = g_mime_header_list_load (options, stream, &error);
					GMimeHeader *header;

					g_assert_no_error (error);
					g_assert_nonnull (headers);
					header = g_mime_header_list_get_header_at (headers, 0);
					g_assert_nonnull (header);
					check (g_bytes_get_size (header->raw_value) == n &&
					       !strcmp (g_mime_header_get_value (header), expected),
					       "parsed raw byte spans decode every truncation without a NUL terminator");
					decoded = g_mime_header_get_value_with_charset (header, "utf-8", &error);
					g_assert_no_error (error);
					check (!g_strcmp0 (decoded, expected),
					       "checked header decoding uses the same exact byte span");
					g_free (decoded);
					g_object_unref (headers);
					g_object_unref (stream);
					g_free (wire);
				}
				g_free (input);
			}
		}
	}

	g_mime_parser_options_set_rfc2047_compliance_mode (options, GMIME_RFC_COMPLIANCE_LOOSE);
	charset = g_strnfill (256 * 1024, 'x');
	input = g_strdup_printf ("=?%s?Q?=41?=", charset);
	decoded = g_mime_utils_header_decode_text (options, input);
	check (!strcmp (decoded, "A"), "long unknown charset names use bounded parsing and heap storage");
	g_free (decoded);
	g_free (input);
	g_free (charset);

	for (phrase = 0; phrase < 2; phrase++) {
		warnings = 0;
		g_mime_parser_options_set_warning_callback (options, count_rfc2047_warning, &warnings);
		decoded = phrase ? g_mime_utils_header_decode_phrase (options, "=?utf-8?Q?a b?=") :
			g_mime_utils_header_decode_text (options, "=?utf-8?Q?a b?=");
		check (!strcmp (decoded, "a b") && warnings == 1, "accepted payload whitespace still reports one warning");
		g_free (decoded);
	}
	g_mime_parser_options_set_warning_callback (options, NULL, NULL);

	folded = g_mime_utils_unstructured_header_fold (options, NULL, raw);
	check (!strcmp (folded, raw), "recovery preserves original charset, language, and encoding spelling when folding");
	g_free (folded);
	folded = g_mime_utils_structured_header_fold (options, NULL, raw);
	check (!strcmp (folded, raw), "phrase recovery preserves raw encoded-word spelling when folding");
	g_free (folded);
	g_mime_parser_options_free (options);
}

static void
test_rfc2047_charset_selection (void)
{
	static const struct {
		const char *input, *decoded, *charset;
	} cases[] = {
		{ "=?utf-8?Q?=C3=A9?= =?latin1?Q?=E9?= =?iso-8859-1?B?6Q==?=",
		  "\303\251\303\251\303\251", "ISO-8859-1" },
		{ "=?latin1?Q?=E9?= =?utf-8?B?w6k=?=", "\303\251\303\251", "ISO-8859-1" },
		{ "=?utf-8?B?w6k=?= =?latin1?Q?=E9?=", "\303\251\303\251", "UTF-8" },
		{ "=?latin1?Q?A?= =?utf-8?Q?B?= =?utf8?Q?C?= =?latin1?Q?D?=",
		  "ABCD", "ISO-8859-1" },
		{ "=?latin1?Q?A?= =?windows-1251?Q?B?= =?cp1251?B?Qw==?=",
		  "ABC", "CP1251" },
		{ "=?utf-8?Q?A?= =?euc-jp?Q?B?= =?ujis?B?Qw==?=",
		  "ABC", "euc-jp" },
		{ "=?utf-8?Q?A?= =?latin1?Q?" "?= =?iso-8859-1?B?" "?=",
		  "A", "ISO-8859-1" },
		{ "=?utf-8?Q?A?= =?latin1*en?Q?B?= =?latin1*fr?Q?C?=",
		  "ABC", "ISO-8859-1" },
		{ "=?utf-8?Q?AAAAAAAAAAAAAAAA?= =?latin1?Q?B?= =?latin1?Q?C?=",
		  "AAAAAAAAAAAAAAAABC", "ISO-8859-1" },
		{ "=?utf-8?Q?A?= raw =?latin1?Q?B?= =?latin1?Q?C?=",
		  "A raw BC", "ISO-8859-1" },
		{ "=?utf-8?Q?A?= =?latin1?X?B?= =?latin1?Q?C?=",
		  "A =?latin1?X?B?= C", "UTF-8" },
		{ "=?latin1?Q?A?= =?x-one?Q?B?= =?x-two?Q?C?=",
		  "ABC", "ISO-8859-1" },
		{ "=?latin1?Q?A?= =?x-one?Q?B?= =?x-one?Q?C?=",
		  "ABC", "x-one" },
		{ "plain \303\251", "plain \303\251", NULL },
		{ "plain", "plain", NULL },
		{ "=?broken?X?A?=", "=?broken?X?A?=", NULL }
	};
	GMimeParserOptions *options = g_mime_parser_options_new ();
	GMimeInternetAddressList *addresses;
	GMimeInternetAddress *address;
	GMimeParamList *params;
	GMimeParam *param;
	char *input, *text, *encoded;
	const char *charset;
	guint mode, i;

	for (mode = 0; mode < 2; mode++) {
		g_mime_parser_options_set_rfc2047_compliance_mode (options,
			mode ? GMIME_RFC_COMPLIANCE_STRICT : GMIME_RFC_COMPLIANCE_LOOSE);
		for (i = 0; i < G_N_ELEMENTS (cases); i++) {
			input = g_strdup_printf ("%s <probe@example.com>", cases[i].input);
			addresses = g_mime_internet_address_list_parse (options, input);
			check (addresses && g_mime_internet_address_list_length (addresses) == 1,
			       "mixed-charset mailbox parses");
			address = g_mime_internet_address_list_get_address (addresses, 0);
			check (!strcmp (g_mime_internet_address_get_name (address), cases[i].decoded),
			       "charset selection preserves decoded mailbox names");
			charset = g_mime_internet_address_get_charset (address);
			check ((!charset && !cases[i].charset) ||
			       (charset && cases[i].charset && !g_ascii_strcasecmp (charset, cases[i].charset)),
			       "mailbox charset counts individual words and uses first-seen ties");
			encoded = g_mime_internet_address_to_string (address, NULL, TRUE);
			text = g_mime_utils_header_decode_phrase (options, encoded);
			check (strstr (text, cases[i].decoded) != NULL,
			       "selected mailbox charset preserves display-name round trips");
			g_free (text);
			g_free (encoded);
			g_object_unref (addresses);
			g_free (input);

			input = g_strdup_printf ("%s: probe@example.com;", cases[i].input);
			addresses = g_mime_internet_address_list_parse (options, input);
			check (addresses && g_mime_internet_address_list_length (addresses) == 1,
			       "mixed-charset group parses");
			address = g_mime_internet_address_list_get_address (addresses, 0);
			check (!strcmp (g_mime_internet_address_get_name (address), cases[i].decoded),
			       "charset selection preserves decoded group names");
			charset = g_mime_internet_address_get_charset (address);
			check ((!charset && !cases[i].charset) ||
			       (charset && cases[i].charset && !g_ascii_strcasecmp (charset, cases[i].charset)),
			       "group charset counts individual words and uses first-seen ties");
			g_object_unref (addresses);
			g_free (input);

			input = g_strdup_printf ("filename=\"%s\"", cases[i].input);
			params = g_mime_param_list_parse (options, input);
			param = g_mime_param_list_get_parameter (params, "filename");
			check (param && !strcmp (g_mime_param_get_value (param), cases[i].decoded),
			       "charset selection preserves decoded parameter values");
			charset = g_mime_param_get_charset (param);
			check ((!charset && (!cases[i].charset || !g_ascii_strcasecmp (cases[i].charset, "UTF-8"))) ||
			       (charset && cases[i].charset && !g_ascii_strcasecmp (charset, cases[i].charset)),
			       "RFC 2047 parameters retain the most common nondefault charset");
			check (g_mime_param_get_encoding_method (param) == (strstr (cases[i].input, "=?") ?
			       GMIME_PARAM_ENCODING_METHOD_RFC2047 : GMIME_PARAM_ENCODING_METHOD_DEFAULT),
			       "charset selection preserves legacy parameter encoding-method detection");
			g_object_unref (params);
			params = g_mime_param_list_parse_full (options, input, -1, NULL);
			param = g_mime_param_list_get_parameter (params, "filename");
			check (g_mime_param_get_encoding_method (param) == GMIME_PARAM_ENCODING_METHOD_RFC2047,
			       "charset selection preserves checked quoted-parameter encoding-method detection");
			g_object_unref (params);
			g_free (input);
		}
	}

	g_mime_parser_options_free (options);
}

static void
test_message_id_parsing (void)
{
	static const struct {
		const char *input, *expected;
	} cases[] = {
		{ "", NULL },
		{ " (comment) ", NULL },
		{ " (unterminated", NULL },
		{ "<", NULL },
		{ "<>", NULL },
		{ "<local", NULL },
		{ "<local@", NULL },
		{ "<local-part>", "local-part" },
		{ "local-part", "local-part" },
		{ "<local-part@>", "local-part@" },
		{ "<local-part@@@>", "local-part@" },
		{ "<local-part@@example.com>", "local-part@example.com" },
		{ "<local-part@domain1@domain2>", "local-part@domain1@domain2" },
		{ "<:invalid-local-part;@domain.com>", ":invalid-local-part;@domain.com" },
		{ "<[local-part@example.com]>", "[local-part@example.com]" },
		{ " (comment) < local . part @ (host) example . com. > trailing",
		  "local.part@example.com" },
		{ "<\"a\\\"b\\\\c\"@example.com>", "\"a\\\"b\\\\c\"@example.com" },
		{ "<\"folded\r\n\tlocal\"@example.com>", "\"folded\tlocal\"@example.com" },
		{ "<\303\251@\344\276\213.example>", "\303\251@\344\276\213.example" },
		{ "<local@[ 127.0.\r\n\t0.1 ]>", "local@[127.0.0.1]" },
		{ "<local@[IPv6:2001:db8::1]>", "local@[IPv6:2001:db8::1]" },
		{ "<local@[127.0.0.1>", NULL },
		{ "<local@bad..domain>", NULL },
		{ "<local@\"domain\">", NULL },
		{ "<local@domain (unterminated>", NULL },
		{ "<\"unterminated@domain>", NULL },
		{ "<\377@domain>", NULL },
		{ "<local@\377>", NULL },
		{ "<\"\377\"@domain>", NULL }
	};
	static const struct {
		const char *input;
		guint count;
		const char *ids[2];
	} references[] = {
		{ ",;:@{}", 0, { NULL, NULL } },
		{ "\"unterminated", 0, { NULL, NULL } },
		{ "<first> \"unterminated", 1, { "first", NULL } },
		{ "noise,;: <> <first@@example.com> <bad@,> ; <second@domain1@domain2>",
		  2, { "first@example.com", "second@domain1@domain2" } },
		{ "<first> \"<not-an-id>\" <second@>", 2, { "first", "second@" } },
		{ "<first@host> (unfinished <second@host>", 1, { "first@host", NULL } },
		{ "<first@host> <bad@<second@host>>", 2, { "first@host", "second@host" } },
		{ "<first@host> <bad@domain..> <second@host>", 2, { "first@host", "second@host" } },
		{ "<first@host> <bad@\"unterminated <second@host>", 1, { "first@host", NULL } },
		{ "<\377@domain> <good@domain>", 1, { "good@domain", NULL } }
	};
	GMimeMessage *message = g_mime_message_new (FALSE);
	GMimePart *part = g_mime_part_new ();
	GMimeHeaderList *headers;
	GMimeHeader *header;
	GMimeMessageIdList *refs;
	char *decoded;
	guint i, j;

	for (i = 0; i < G_N_ELEMENTS (cases); i++) {
		decoded = g_mime_utils_decode_message_id (cases[i].input);
		testsuite_check ("message-id parsing [%u]", i);
		if (!g_strcmp0 (decoded, cases[i].expected))
			testsuite_check_passed ();
		else
			testsuite_check_failed ("actual=\"%s\", expected=\"%s\"",
					       decoded ? decoded : "(null)", cases[i].expected ? cases[i].expected : "(null)");
		g_free (decoded);
	}

	for (i = 0; i < G_N_ELEMENTS (references); i++) {
		refs = g_mime_message_id_list_parse (NULL, references[i].input);
		testsuite_check ("reference recovery count [%u]", i);
		if (g_mime_message_id_list_length (refs) == references[i].count)
			testsuite_check_passed ();
		else
			testsuite_check_failed ("actual=%d, expected=%u",
					       g_mime_message_id_list_length (refs), references[i].count);
		for (j = 0; j < references[i].count; j++)
			check (!g_strcmp0 (g_mime_message_id_list_get_message_id (refs, j), references[i].ids[j]),
			       "reference recovery retains message-id order and normalization");
		g_object_unref (refs);
	}

	headers = g_mime_entity_get_header_list (GMIME_ENTITY (message));
	g_mime_header_list_set (headers, "Message-Id", "initial@host", NULL);
	header = g_mime_header_list_get_header (headers, "Message-Id");
	testsuite_header_set_raw_text (header, " <local@@example.com>\n");
	check (!g_strcmp0 (g_mime_message_get_message_id (message), "local@example.com"),
	       "message-id recovery updates the owning message");
	check (!strcmp (testsuite_header_raw_text (header), " <local@@example.com>\n"),
	       "message-id recovery preserves the original raw header");
	testsuite_header_set_raw_text (header, " <incomplete\n");
	check (g_mime_message_get_message_id (message) == NULL, "malformed message ids clear the cached message id");

	headers = g_mime_entity_get_header_list (GMIME_ENTITY (part));
	g_mime_header_list_set (headers, "Content-Id", "initial@host", NULL);
	header = g_mime_header_list_get_header (headers, "Content-Id");
	testsuite_header_set_raw_text (header, " <[local@example.com]>\n");
	check (!g_strcmp0 (g_mime_entity_get_content_id (GMIME_ENTITY (part)), "[local@example.com]"),
	       "content-id parsing preserves tolerated Exchange brackets");
	testsuite_header_set_raw_text (header, " <incomplete\n");
	check (g_mime_entity_get_content_id (GMIME_ENTITY (part)) == NULL,
	       "malformed content ids clear the cached content id");
	g_object_unref (part);
	g_object_unref (message);
}

static void
test_message_id_assignment (void)
{
	static const struct {
		const char *input, *expected;
	} cases[] = {
		{ "local@host", "local@host" },
		{ "<local@host>", "local@host" },
		{ " \t<local@@host> (comment)\r\n", "local@host" },
		{ "\v\302\205<local@host>\302\240\342\200\203", NULL },
		{ "\f<local@host>\f", NULL },
		{ "\302\240<local@host>", NULL },
		{ "<local@host>\342\200\203", NULL },
		{ "\302\240local@host\342\200\203", "\302\240local@host\342\200\203" },
		{ " \t\302\240local@host\342\200\203\r\n", "\302\240local@host\342\200\203" },
		{ "\302\205", "\302\205" },
		{ "\r\nlocal@host\r\n", "local@host" },
		{ "< local . part @ (comment) host. >", "local.part@host" },
		{ "local-part", "local-part" },
		{ "local@", "local@" },
		{ "local@one@two", "local@one@two" },
		{ "[local@host]", "[local@host]" },
		{ ":odd;part@host", ":odd;part@host" },
		{ "\"a b\"@host", "\"a b\"@host" },
		{ "\"a\\\"b\\\\c\"@host", "\"a\\\"b\\\\c\"@host" },
		{ "\"folded\r\n\tlocal\"@host", "\"folded\tlocal\"@host" },
		{ "<local@\r\n host>", "local@host" },
		{ "<local@\n\thost>", "local@host" },
		{ "<local@\r host>", "local@host" },
		{ "\303\251@\344\276\213.example", "\303\251@\344\276\213.example" },
		{ "local@[127.0.0.1]", "local@[127.0.0.1]" },
		{ "", NULL },
		{ " \t\r\n", NULL },
		{ "<>", NULL },
		{ "<local", NULL },
		{ "<local@host> extra", NULL },
		{ "<local@host> <second@host>", NULL },
		{ "local@host trailing", NULL },
		{ "local part@host", NULL },
		{ "local . part@host", NULL },
		{ "local@\r\n host", NULL },
		{ "local@bad..host", NULL },
		{ "<local@host> (unterminated", NULL },
		{ "<local@host>\r\nBcc: other@host", NULL },
		{ "\"a\nb\"@host", NULL },
		{ "<local@\rhost>", NULL },
		{ "<local@\nhost>", NULL },
		{ "<\377@host>", NULL },
		{ "<local@host> (\377)", NULL }
	};
	GMimeMessage *message = g_mime_message_new (FALSE);
	GMimeEntity *entity = GMIME_ENTITY (message);
	GMimePart *part = g_mime_part_new ();
	GMimeEntity *content = GMIME_ENTITY (part);
	GMimeMessageIdList *refs = g_mime_message_get_references (message);
	GMimeHeaderList *headers;
	GMimeHeader *header;
	char *expected, *raw;
	guint i;

	for (i = 0; i < G_N_ELEMENTS (cases); i++) {
		g_mime_message_set_message_id (message, "initial@host");
		g_mime_entity_set_content_id (content, "initial@host");
		g_mime_message_set_in_reply_to (message, "initial@host");
		g_mime_message_set_resent_message_id (message, "initial@host");
		g_mime_message_id_list_clear (refs);
		g_mime_message_id_list_add (refs, "initial@host");
		if (!cases[i].expected) {
			g_test_expect_message ("gmime", G_LOG_LEVEL_CRITICAL, "*id != NULL*");
			g_test_expect_message ("gmime", G_LOG_LEVEL_CRITICAL, "*id != NULL*");
			g_test_expect_message ("gmime", G_LOG_LEVEL_CRITICAL, "*id != NULL*");
			g_test_expect_message ("gmime", G_LOG_LEVEL_CRITICAL, "*id != NULL*");
			g_test_expect_message ("gmime", G_LOG_LEVEL_CRITICAL, "*id != NULL*");
		}
		g_mime_message_set_message_id (message, cases[i].input);
		g_mime_part_set_content_id (part, cases[i].input);
		g_mime_message_set_in_reply_to (message, cases[i].input);
		g_mime_message_set_resent_message_id (message, cases[i].input);
		g_mime_message_id_list_set_message_id (refs, 0, cases[i].input);
		if (!cases[i].expected)
			g_test_assert_expected_messages ();

		expected = g_strdup_printf ("<%s>", cases[i].expected ? cases[i].expected : "initial@host");
		testsuite_check ("message-id assignment [%u]", i);
		if (!g_strcmp0 (g_mime_message_get_message_id (message),
				cases[i].expected ? cases[i].expected : "initial@host") &&
		    !g_strcmp0 (g_mime_entity_get_header (entity, "Message-Id"), expected))
			testsuite_check_passed ();
		else
			testsuite_check_failed ("ID=\"%s\", header=\"%s\", expected=\"%s\"",
					       g_mime_message_get_message_id (message),
					       g_mime_entity_get_header (entity, "Message-Id"), expected);
		testsuite_check ("content-id assignment [%u]", i);
		if (!g_strcmp0 (g_mime_entity_get_content_id (content),
				cases[i].expected ? cases[i].expected : "initial@host") &&
		    !g_strcmp0 (g_mime_entity_get_header (content, "Content-Id"), expected))
			testsuite_check_passed ();
		else
			testsuite_check_failed ("ID=\"%s\", header=\"%s\", expected=\"%s\"",
					       g_mime_entity_get_content_id (content),
					       g_mime_entity_get_header (content, "Content-Id"), expected);
		check (!g_strcmp0 (g_mime_message_get_in_reply_to (message),
				  cases[i].expected ? cases[i].expected : "initial@host") &&
		       !g_strcmp0 (g_mime_entity_get_header (entity, "In-Reply-To"), expected),
		       "in-reply-to assignment normalizes complete IDs and preserves rejected state");
		check (!g_strcmp0 (g_mime_message_get_resent_message_id (message),
				  cases[i].expected ? cases[i].expected : "initial@host") &&
		       !g_strcmp0 (g_mime_entity_get_header (entity, "Resent-Message-Id"), expected),
		       "resent-message-id assignment normalizes complete IDs and preserves rejected state");
		check (!g_strcmp0 (g_mime_message_id_list_get_message_id (refs, 0),
				  cases[i].expected ? cases[i].expected : "initial@host") &&
		       !g_strcmp0 (g_mime_entity_get_header (entity, "References"), expected),
		       "reference assignment validates complete IDs before updating the list and header");
		g_free (expected);
	}

	headers = g_mime_entity_get_header_list (entity);
	header = g_mime_header_list_get_header (headers, "Message-Id");
	testsuite_header_set_raw_text (header, " <local@@host> (original)\n");
	raw = g_strdup (testsuite_header_raw_text (header));
	g_mime_message_set_message_id (message, g_mime_message_get_message_id (message));
	check (!strcmp (testsuite_header_raw_text (header), raw), "unchanged message id preserves raw header");
	g_test_expect_message ("gmime", G_LOG_LEVEL_CRITICAL, "*id != NULL*");
	g_mime_message_set_message_id (message, "<invalid");
	g_test_assert_expected_messages ();
	check (!strcmp (testsuite_header_raw_text (header), raw), "invalid message id preserves raw header");
	g_free (raw);
	g_mime_header_list_append (headers, "mEsSaGe-Id", "<duplicate@host>", NULL);
	g_mime_message_set_message_id (message, g_mime_message_get_message_id (message));
	check (g_mime_header_list_index_of (headers, "Message-Id") ==
	       g_mime_header_list_last_index_of (headers, "Message-Id") &&
	       !strcmp (g_mime_message_get_message_id (message), "local@host"),
	       "same-value message id assignment removes case-insensitive duplicates with an aliased cache");
	g_mime_message_set_message_id (message, testsuite_header_raw_text (header));
	check (!g_strcmp0 (g_mime_message_get_message_id (message), "local@host"),
	       "message-id assignment safely consumes an aliased raw header");
	g_mime_message_set_message_id (message, g_mime_message_get_message_id (message) + 1);
	check (!g_strcmp0 (g_mime_message_get_message_id (message), "ocal@host"),
	       "message-id assignment safely consumes part of its cached value");
	g_test_expect_message ("gmime", G_LOG_LEVEL_CRITICAL, "*message_id != NULL*");
	g_mime_message_set_message_id (message, NULL);
	g_test_assert_expected_messages ();
	check (!g_strcmp0 (g_mime_message_get_message_id (message), "ocal@host"),
	       "NULL message id is rejected without changing the cache");
	g_mime_message_set_subject (message, "Original subject", NULL);
	g_mime_header_list_append (headers, "sUbJeCt", "Duplicate subject", NULL);
	check (!strcmp (g_mime_message_get_subject (message), "Original subject"),
	       "the first subject remains cached after appending a duplicate");
	g_mime_message_set_subject (message, g_mime_message_get_subject (message), NULL);
	check (!g_strcmp0 (g_mime_message_get_subject (message), "Original subject") &&
	       g_mime_header_list_index_of (headers, "Subject") == g_mime_header_list_last_index_of (headers, "Subject"),
	       "same-value subject assignment deduplicates without clearing the cached subject");

	headers = g_mime_entity_get_header_list (content);
	header = g_mime_header_list_get_header (headers, "Content-Id");
	testsuite_header_set_raw_text (header, " <local@@host> (original)\n");
	raw = g_strdup (testsuite_header_raw_text (header));
	g_mime_entity_set_content_id (content, g_mime_entity_get_content_id (content));
	check (!strcmp (testsuite_header_raw_text (header), raw), "unchanged content id preserves raw header");
	g_test_expect_message ("gmime", G_LOG_LEVEL_CRITICAL, "*id != NULL*");
	g_mime_entity_set_content_id (content, "<invalid");
	g_test_assert_expected_messages ();
	check (!strcmp (testsuite_header_raw_text (header), raw), "invalid content id preserves raw header");
	g_free (raw);
	g_mime_entity_set_content_id (content, testsuite_header_raw_text (header));
	check (!g_strcmp0 (g_mime_entity_get_content_id (content), "local@host"),
	       "content-id assignment safely consumes an aliased raw header");
	g_mime_entity_set_content_id (content, g_mime_entity_get_content_id (content) + 1);
	check (!g_strcmp0 (g_mime_entity_get_content_id (content), "ocal@host"),
	       "content-id assignment safely consumes part of its cached value");
	g_mime_header_list_append (headers, "Content-Id", "<duplicate@host>", NULL);
	g_mime_header_list_append (headers, "X-Keep", "unchanged", NULL);
	g_mime_entity_set_content_id (content, NULL);
	check (!g_mime_entity_get_content_id (content) && !g_mime_header_list_contains (headers, "Content-Id"),
	       "NULL content id clears the cache and removes all duplicate headers");
	check (!g_strcmp0 (g_mime_entity_get_header (content, "X-Keep"), "unchanged"),
	       "clearing a content id preserves other headers");
	g_mime_header_list_append (headers, "Content-Id", "<incomplete", NULL);
	g_mime_part_set_content_id (part, NULL);
	check (!g_mime_header_list_contains (headers, "Content-Id"),
	       "NULL content id removes a malformed header even when the cache is already NULL");
	g_object_unref (part);
	g_object_unref (message);
}

static void
test_message_reply_ids (void)
{
	static const struct {
		const char *raw, *reply, *resent;
	} cases[] = {
		{ " <first@@host> <second@host>\n", "first@host", "first@host" },
		{ " bare@host\n", NULL, "bare@host" },
		{ " noise; <> <bad@,> <good@host>\n", "good@host", "noise;<" },
		{ " \"<quoted@host>\" <good@host>\n", "good@host", "\"<quoted@host>\"<good@host" },
		{ " (comment <ignored@host>) <good@host>\n", "good@host", "good@host" },
		{ " <bad@bad..host> <good@host>\n", "good@host", NULL },
		{ " <bad@<good@host>>\n", "good@host", NULL },
		{ " \"unterminated\n", NULL, NULL },
		{ " (unterminated <good@host>\n", NULL, NULL },
		{ " <\"folded\r\n\tlocal\"@host>\n", "\"folded\tlocal\"@host", "\"folded\tlocal\"@host" },
		{ " <\377@host> <good@host>\n", "good@host", NULL },
		{ " <incomplete\n", NULL, NULL }
	};
	static const struct {
		const char *name;
		void (*set) (GMimeMessage *, const char *);
		const char *(*get) (GMimeMessage *);
	} properties[] = {
		{ "In-Reply-To", g_mime_message_set_in_reply_to, g_mime_message_get_in_reply_to },
		{ "Resent-Message-Id", g_mime_message_set_resent_message_id, g_mime_message_get_resent_message_id }
	};
	GMimeMessage *message = g_mime_message_new (FALSE);
	GMimeHeaderList *headers = g_mime_entity_get_header_list (GMIME_ENTITY (message));
	GMimeHeader *header, *duplicate, *replacement;
	GMimeStream *stream;
	GMimeParser *parser;
	GError *error = NULL;
	char *raw;
	guint i, j;

	for (i = 0; i < G_N_ELEMENTS (cases); i++) {
		g_mime_header_list_set (headers, "In-Reply-To", "", NULL);
		header = g_mime_header_list_get_header (headers, "In-Reply-To");
		testsuite_header_set_raw_text (header, cases[i].raw);
		check (!g_strcmp0 (g_mime_message_get_in_reply_to (message), cases[i].reply),
		       "in-reply-to recovers the first reference rather than a bare addr-spec");
		check (!strcmp (testsuite_header_raw_text (header), cases[i].raw),
		       "in-reply-to recovery preserves raw header bytes");
		g_mime_header_list_set (headers, "Resent-Message-Id", "", NULL);
		header = g_mime_header_list_get_header (headers, "Resent-Message-Id");
		testsuite_header_set_raw_text (header, cases[i].raw);
		testsuite_check ("resent-message-id uses tolerant single-ID parsing [%u]", i);
		if (!g_strcmp0 (g_mime_message_get_resent_message_id (message), cases[i].resent))
			testsuite_check_passed ();
		else
			testsuite_check_failed ("actual=\"%s\", expected=\"%s\"",
					       g_mime_message_get_resent_message_id (message), cases[i].resent);
		check (!strcmp (testsuite_header_raw_text (header), cases[i].raw),
		       "resent-message-id recovery preserves raw header bytes");
	}

	for (j = 0; j < G_N_ELEMENTS (properties); j++) {
		g_mime_header_list_clear (headers);
		check (properties[j].get (message) == NULL, "clearing headers resets message reply IDs");
		properties[j].set (message, "<local@@host>");
		header = g_mime_header_list_get_header (headers, properties[j].name);
		testsuite_header_set_raw_text (header, " <local@@host> (original)\n");
		raw = g_strdup (testsuite_header_raw_text (header));
		properties[j].set (message, properties[j].get (message));
		check (!strcmp (testsuite_header_raw_text (header), raw),
		       "exact reply ID reassignment preserves the raw header");
		g_test_expect_message ("gmime", G_LOG_LEVEL_CRITICAL, "*id != NULL*");
		properties[j].set (message, "<incomplete");
		g_test_assert_expected_messages ();
		check (!strcmp (testsuite_header_raw_text (header), raw) &&
		       !g_strcmp0 (properties[j].get (message), "local@host"),
		       "invalid reply ID assignment preserves the cache and raw header");
		g_free (raw);
		properties[j].set (message, testsuite_header_raw_text (header));
		check (!g_strcmp0 (properties[j].get (message), "local@host"),
		       "reply ID assignment safely consumes an aliased raw header");
		properties[j].set (message, properties[j].get (message) + 1);
		check (!g_strcmp0 (properties[j].get (message), "ocal@host"),
		       "reply ID assignment safely consumes an aliased cached substring");
		g_mime_header_list_append (headers, properties[j].name, "<duplicate@host>", NULL);
		duplicate = g_mime_header_list_get_header_at (headers, 1);
		testsuite_header_set_raw_text (duplicate, " <changed@host>\n");
		check (!g_strcmp0 (properties[j].get (message), "ocal@host"),
		       "appending or changing a duplicate does not override the first reply ID");
		properties[j].set (message, properties[j].get (message));
		check (g_mime_header_list_get_count (headers) == (j ? 2 : 1) &&
		       !strcmp (properties[j].get (message), "ocal@host"),
		       "equal reply IDs remove singleton duplicates but preserve older resent headers");
		properties[j].set (message, "updated@host");
		check (g_mime_header_list_get_count (headers) == (j ? 2 : 1) &&
		       (!j || !strcmp (testsuite_header_raw_text (duplicate), " <changed@host>\n")),
		       "changed resent IDs preserve older header objects and raw values");
		if (!j) {
			g_mime_header_list_append (headers, properties[j].name, "<changed@host>", NULL);
			duplicate = g_mime_header_list_get_header_at (headers, 1);
		}
		testsuite_header_set_raw_text (header, " <incomplete\n");
		check (properties[j].get (message) == NULL,
		       "a malformed first reply ID does not fall back to a later header");
		testsuite_header_set_raw_text (header, " <ocal@host>\n");
		g_mime_header_list_insert (headers, 0, properties[j].name, "<inserted@host>", NULL);
		check (!g_strcmp0 (properties[j].get (message), "inserted@host"),
		       "inserting an earlier header changes the cached reply ID");
		g_mime_header_list_remove_at (headers, 0);
		check (!g_strcmp0 (properties[j].get (message), "ocal@host"),
		       "removing the first header promotes the next reply ID");
		g_mime_header_list_remove_at (headers, 0);
		check (!g_strcmp0 (properties[j].get (message), "changed@host"),
		       "removing the original header promotes its modified duplicate");
		replacement = g_mime_header_new (properties[j].name, "<replaced@host>", NULL);
		g_mime_header_list_replace (headers, replacement);
		check (!g_strcmp0 (properties[j].get (message), "replaced@host"),
		       "replacing a header updates the cached reply ID");
		g_mime_header_list_add (headers, replacement);
		g_mime_header_list_remove_at (headers, 0);
		testsuite_header_set_raw_text (replacement, " <shared@host>\n");
		check (!g_strcmp0 (properties[j].get (message), "shared@host"),
		       "a repeated header instance remains connected after removing its first occurrence");
		g_mime_header_list_remove_all (headers, properties[j].name);
		check (properties[j].get (message) == NULL, "removing all reply ID headers clears the cache");
		testsuite_header_set_raw_text (replacement, " <detached@host>\n");
		check (properties[j].get (message) == NULL, "detached headers no longer change reply IDs");
		g_object_unref (replacement);
	}

	g_mime_message_set_resent_message_id (message, "retained@host");
	g_test_expect_message ("gmime", G_LOG_LEVEL_CRITICAL, "*message_id != NULL*");
	g_mime_message_set_resent_message_id (message, NULL);
	g_test_assert_expected_messages ();
	check (!g_strcmp0 (g_mime_message_get_resent_message_id (message), "retained@host"),
	       "NULL resent-message-id is rejected without mutation");
	g_mime_message_set_in_reply_to (message, "first@host");
	g_mime_header_list_append (headers, "in-reply-to", "<second@host>", NULL);
	g_mime_message_set_in_reply_to (message, NULL);
	check (!g_mime_message_get_in_reply_to (message) && !g_mime_header_list_contains (headers, "In-Reply-To"),
	       "NULL in-reply-to clears the cache and all matching headers");
	g_mime_header_list_append (headers, "In-Reply-To", "<incomplete", NULL);
	g_mime_message_set_in_reply_to (message, NULL);
	check (!g_mime_header_list_contains (headers, "In-Reply-To"),
	       "NULL in-reply-to removes malformed headers even with an empty cache");
	check (!g_strcmp0 (g_mime_message_get_resent_message_id (message), "retained@host"),
	       "clearing in-reply-to preserves resent-message-id");
	g_object_unref (message);

	raw = "In-Reply-To: noise; <> <reply@@host> <ignored@host>\r\n"
	      "Resent-Message-Id: <resent@@host>\r\n\r\nbody";
	stream = g_mime_stream_mem_new_with_buffer (raw, strlen (raw));
	parser = g_mime_parser_new_with_stream (stream);
	message = g_mime_parser_construct_message (parser, NULL, &error);
	g_assert_no_error (error);
	check (message && !g_strcmp0 (g_mime_message_get_in_reply_to (message), "reply@host") &&
	       !g_strcmp0 (g_mime_message_get_resent_message_id (message), "resent@host"),
	       "MIME parsing populates reply and resent message IDs");
	g_object_unref (message);
	g_object_unref (parser);
	g_object_unref (stream);
}

static void
test_message_id_whitespace (void)
{
	static const gunichar whitespace[] = {
		'\t', '\n', '\v', '\f', '\r', ' ', 0x85, 0xa0, 0x1680,
		0x2000, 0x2001, 0x2002, 0x2003, 0x2004, 0x2005, 0x2006,
		0x2007, 0x2008, 0x2009, 0x200a, 0x2028, 0x2029, 0x202f, 0x205f, 0x3000
	};
	GMimeMessageIdList *list = g_mime_message_id_list_new ();
	guint i, bracketed, edge;

	g_mime_message_id_list_add (list, "initial@host");
	for (i = 0; i < G_N_ELEMENTS (whitespace); i++) {
		char buffer[7] = { 0 };
		gboolean lwsp = whitespace[i] == ' ' || whitespace[i] == '\t' ||
			whitespace[i] == '\r' || whitespace[i] == '\n';

		g_unichar_to_utf8 (whitespace[i], buffer);
		for (bracketed = 0; bracketed < 2; bracketed++) {
			for (edge = 0; edge < 3; edge++) {
				char *value = g_strconcat (edge != 1 ? buffer : "",
							  bracketed ? "<local@host>" : "local@host",
							  edge != 0 ? buffer : "", NULL);
				char *input = g_strconcat (" \t\r\n", value, "\r\n\t ", NULL);
				const char *expected = lwsp ? "local@host" : NULL;

				/* Bare local-parts tolerate controls; domain atoms do not. */
				if (!lwsp && !bracketed && (whitespace[i] >= 0x80 || edge == 0))
					expected = value;

				g_mime_message_id_list_set_message_id (list, 0, "initial@host");
				if (!expected)
					g_test_expect_message ("gmime", G_LOG_LEVEL_CRITICAL, "*id != NULL*");
				g_mime_message_id_list_set_message_id (list, 0, input);
				if (!expected)
					g_test_assert_expected_messages ();
				testsuite_check ("Message-ID whitespace U+%04X [%u/%u]", whitespace[i], bracketed, edge);
				if (g_mime_message_id_list_length (list) == 1 &&
				    !g_strcmp0 (g_mime_message_id_list_get_message_id (list, 0),
						expected ? expected : "initial@host"))
					testsuite_check_passed ();
				else
					testsuite_check_failed ("unexpected normalization or mutation of a rejected ID");
				g_free (input);
				g_free (value);
			}
		}
	}
	g_object_unref (list);
}

static void
test_message_id_list (void)
{
	GMimeMessageIdList *list = g_mime_message_id_list_new (), *clone;
	const char *range[] = { "<first@@host>", "\"a b\"@host", "last@host" };
	const char *invalid[] = { "valid@host", "<incomplete" };
	const char *null_entry[] = { "valid@host", NULL };
	const char *aliases[2];
	char *text;

	check (G_IS_OBJECT (list) && g_mime_message_id_list_length (list) == 0,
	       "message ID lists are initially empty GObjects");
	text = g_mime_message_id_list_to_string (list);
	check (text && !*text, "empty message ID lists format as an empty string");
	g_free (text);
	g_mime_message_id_list_add_range (list, range, G_N_ELEMENTS (range));
	check (g_mime_message_id_list_length (list) == 3 &&
	       !g_strcmp0 (g_mime_message_id_list_get_message_id (list, 0), "first@host"),
	       "bulk message IDs are normalized before insertion");
	check (g_mime_message_id_list_index_of (list, "first@host") == 0 &&
	       !g_mime_message_id_list_contains (list, "<first@host>") &&
	       !g_mime_message_id_list_contains (list, "FIRST@host"),
	       "message ID lookup is exact and case-sensitive without normalization");
	g_mime_message_id_list_insert (list, 0, "<inserted@host>");
	g_mime_message_id_list_insert (list, g_mime_message_id_list_length (list), "end@host");
	check (g_mime_message_id_list_length (list) == 5 &&
	       !g_strcmp0 (g_mime_message_id_list_get_message_id (list, 0), "inserted@host") &&
	       !g_strcmp0 (g_mime_message_id_list_get_message_id (list, 4), "end@host"),
	       "message IDs can be inserted at either boundary");
	g_mime_message_id_list_set_message_id (list, 0, g_mime_message_id_list_get_message_id (list, 0) + 1);
	check (!g_strcmp0 (g_mime_message_id_list_get_message_id (list, 0), "nserted@host"),
	       "message ID replacement safely consumes an aliased substring");
	aliases[0] = g_mime_message_id_list_get_message_id (list, 0);
	aliases[1] = g_mime_message_id_list_get_message_id (list, 1);
	g_mime_message_id_list_add_range (list, aliases, G_N_ELEMENTS (aliases));
	check (g_mime_message_id_list_length (list) == 7 &&
	       !g_strcmp0 (g_mime_message_id_list_get_message_id (list, 5), aliases[0]) &&
	       !g_strcmp0 (g_mime_message_id_list_get_message_id (list, 6), aliases[1]),
	       "bulk insertion snapshots aliased input values");
	g_test_expect_message ("gmime", G_LOG_LEVEL_CRITICAL, "*id != NULL*");
	g_mime_message_id_list_add_range (list, invalid, G_N_ELEMENTS (invalid));
	g_test_assert_expected_messages ();
	g_test_expect_message ("gmime", G_LOG_LEVEL_CRITICAL, "*id != NULL*");
	g_mime_message_id_list_add_range (list, null_entry, G_N_ELEMENTS (null_entry));
	g_test_assert_expected_messages ();
	check (g_mime_message_id_list_length (list) == 7,
	       "invalid or NULL bulk entries leave the entire list unchanged");
	g_test_expect_message ("gmime", G_LOG_LEVEL_CRITICAL, "*id != NULL*");
	check (g_mime_message_id_list_add (list, "<incomplete") == -1, "invalid message ID additions fail");
	g_test_assert_expected_messages ();
	g_test_expect_message ("gmime", G_LOG_LEVEL_CRITICAL, "*id != NULL*");
	g_mime_message_id_list_insert (list, 0, "<incomplete");
	g_test_assert_expected_messages ();
	g_test_expect_message ("gmime", G_LOG_LEVEL_CRITICAL, "*message_id != NULL*");
	g_mime_message_id_list_set_message_id (list, 0, NULL);
	g_test_assert_expected_messages ();
	g_test_expect_message ("gmime", G_LOG_LEVEL_CRITICAL, "*index >= 0*");
	g_mime_message_id_list_insert (list, 8, "valid@host");
	g_test_assert_expected_messages ();
	g_test_expect_message ("gmime", G_LOG_LEVEL_CRITICAL, "*index >= 0*");
	check (!g_mime_message_id_list_remove_at (list, -1), "negative message ID removal indexes fail");
	g_test_assert_expected_messages ();
	check (g_mime_message_id_list_remove (list, aliases[0]) &&
	       g_mime_message_id_list_index_of (list, "nserted@host") == 4,
	       "removing a message ID only removes its first exact occurrence");
	check (!g_mime_message_id_list_remove (list, "<first@host>"),
	       "removing a bracketed search value does not normalize it");
	clone = g_mime_message_id_list_clone (list);
	g_mime_message_id_list_clear (list);
	text = g_mime_message_id_list_to_string (clone);
	check (!strcmp (text, "<first@host> <\"a b\"@host> <last@host> <end@host> <nserted@host> <first@host>"),
	       "clones retain independent storage and ordered bracketed string formatting");
	g_free (text);
	g_object_unref (clone);
	g_mime_message_id_list_add_range (list, NULL, 0);
	check (g_mime_message_id_list_length (list) == 0, "empty bulk insertion accepts NULL input");
	g_object_unref (list);
}

static void
test_message_references (void)
{
	GMimeMessage *message = g_mime_message_new (FALSE);
	GMimeHeaderList *headers = g_mime_entity_get_header_list (GMIME_ENTITY (message));
	GMimeMessageIdList *refs = g_object_ref (g_mime_message_get_references (message)), *clone;
	const char *invalid[] = { "valid@host", "<incomplete" };
	GMimeHeader *header, *duplicate, *replacement;
	GMimeStream *stream;
	GMimeParser *parser;
	GError *error = NULL;
	char *raw, *text;
	const char *input = "References: noise <> <first@@host>\r\n"
			    "\t<bad@,> <second@host>\r\n"
			    "References: <ignored@host>\r\n\r\nbody";

	g_mime_header_list_append (headers, "References", "noise <> <first@@host> <second@host>", NULL);
	header = g_mime_header_list_get_header (headers, "References");
	testsuite_header_set_raw_text (header, " noise <> <first@@host> (original) <second@host>\n");
	raw = g_strdup (testsuite_header_raw_text (header));
	check (g_mime_message_get_references (message) == refs &&
	       g_mime_message_id_list_length (refs) == 2 &&
	       !g_strcmp0 (g_mime_message_id_list_get_message_id (refs, 0), "first@host"),
	       "raw References edits populate the persistent list");
	g_mime_message_id_list_set_message_id (refs, 0, "first@host");
	g_mime_message_id_list_set_message_id (refs, 0, "<first@@host>");
	check (!strcmp (testsuite_header_raw_text (header), raw),
	       "exact and normalized reference reassignment preserve the raw header");
	g_test_expect_message ("gmime", G_LOG_LEVEL_CRITICAL, "*id != NULL*");
	g_mime_message_id_list_add_range (refs, invalid, G_N_ELEMENTS (invalid));
	g_test_assert_expected_messages ();
	check (g_mime_message_id_list_length (refs) == 2 &&
	       !strcmp (testsuite_header_raw_text (header), raw),
	       "failed bulk reference validation preserves both list and raw header");
	g_free (raw);
	g_mime_header_list_append (headers, "references", "<duplicate@host>", NULL);
	duplicate = g_mime_header_list_get_header_at (headers, 1);
	testsuite_header_set_raw_text (duplicate, " <edited@host>\n");
	check (g_mime_message_id_list_length (refs) == 2,
	       "later References headers do not override the first");
	testsuite_header_set_raw_text (header, " <incomplete\n");
	check (g_mime_message_id_list_length (refs) == 0,
	       "a malformed first References header does not fall back to a duplicate");
	g_mime_header_list_remove_at (headers, 0);
	check (g_mime_message_get_references (message) == refs &&
	       !g_strcmp0 (g_mime_message_id_list_get_message_id (refs, 0), "edited@host"),
	       "removing the first References header promotes the next into the same list");
	g_mime_header_list_insert (headers, 0, "References", "<inserted@host>", NULL);
	check (!g_strcmp0 (g_mime_message_id_list_get_message_id (refs, 0), "inserted@host"),
	       "inserting an earlier References header updates the list");
	replacement = g_mime_header_new ("References", "<replaced@host>", NULL);
	g_mime_header_list_replace (headers, replacement);
	check (!g_strcmp0 (g_mime_message_id_list_get_message_id (refs, 0), "replaced@host"),
	       "replacing References refreshes the list");
	g_mime_header_list_add (headers, replacement);
	g_mime_header_list_remove_at (headers, 0);
	testsuite_header_set_raw_text (replacement, " <shared@host>\n");
	check (!g_strcmp0 (g_mime_message_id_list_get_message_id (refs, 0), "shared@host"),
	       "repeated References header instances retain owner notifications");
	g_mime_header_list_append (headers, "References", "<duplicate@host>", NULL);
	g_mime_message_id_list_add (refs, "<added@@host>");
	check (g_mime_header_list_get_count (headers) == 1 &&
	       !g_strcmp0 (g_mime_entity_get_header (GMIME_ENTITY (message), "References"),
			   "<shared@host> <added@host>"),
	       "reference mutations rewrite the header once and remove duplicates");
	clone = g_mime_message_id_list_clone (refs);
	g_mime_message_id_list_add (clone, "independent@host");
	check (g_mime_message_id_list_length (refs) == 2,
	       "cloned References lists are disconnected from their source message");
	g_object_unref (clone);
	g_mime_message_id_list_clear (refs);
	check (!g_mime_header_list_contains (headers, "References"), "clearing references removes all headers");
	testsuite_header_set_raw_text (replacement, " <detached@host>\n");
	check (g_mime_message_id_list_length (refs) == 0, "detached References headers no longer update the list");
	g_object_unref (replacement);
	g_mime_header_list_append (headers, "References", "<incomplete", NULL);
	g_mime_message_id_list_clear (refs);
	check (!g_mime_header_list_contains (headers, "References"),
	       "clearing an empty references list removes malformed raw headers");
	g_mime_header_list_append (headers, "References", "<incomplete", NULL);
	g_mime_message_id_list_add_range (refs, NULL, 0);
	check (!g_mime_header_list_contains (headers, "References"),
	       "an empty bulk addition still synchronizes an empty references list");
	g_mime_message_id_list_add (refs, "retained@host");
	g_mime_header_list_clear (headers);
	check (g_mime_message_get_references (message) == refs && g_mime_message_id_list_length (refs) == 0,
	       "clearing message headers resets the persistent References list");
	g_object_unref (message);
	g_mime_message_id_list_add (refs, "surviving@host");
	text = g_mime_message_id_list_to_string (refs);
	check (!strcmp (text, "<surviving@host>"), "retained References lists remain usable after owner finalization");
	g_free (text);
	g_object_unref (refs);

	stream = g_mime_stream_mem_new_with_buffer (input, strlen (input));
	parser = g_mime_parser_new_with_stream (stream);
	message = g_mime_parser_construct_message (parser, NULL, &error);
	g_assert_no_error (error);
	refs = g_mime_message_get_references (message);
	check (g_mime_message_id_list_length (refs) == 2 &&
	       !g_strcmp0 (g_mime_message_id_list_get_message_id (refs, 1), "second@host"),
	       "MIME parsing recovers References from only the first matching header");
	g_object_unref (message);
	g_object_unref (parser);
	g_object_unref (stream);
}

static void
test_message_references_folding (void)
{
	static const struct {
		const char *glyph;
		guint first, second;
		gboolean fold, third;
	} cases[] = {
		{ "a", 25, 28, FALSE, FALSE },
		{ "a", 25, 29, TRUE, FALSE },
		{ "\303\251", 25, 28, FALSE, FALSE },
		{ "\303\251", 25, 29, TRUE, FALSE },
		{ "\360\237\230\200", 12, 29, FALSE, FALSE },
		{ "\360\237\230\200", 12, 30, TRUE, FALSE },
		{ "a", 100, 1, TRUE, FALSE },
		{ "a", 15, 15, FALSE, TRUE }
	};
	const char *newline = g_mime_format_options_get_newline (NULL);
	GMimeMessage *message = g_mime_message_new (FALSE);
	GMimeMessageIdList *refs = g_mime_message_get_references (message);
	GMimeHeaderList *headers = g_mime_entity_get_header_list (GMIME_ENTITY (message));
	GString *first = g_string_new (NULL);
	const char *ids[3], *actual;
	char *padding, *second, *expected, *separator;
	guint i, j;

	for (i = 0; i < G_N_ELEMENTS (cases); i++) {
		g_string_truncate (first, 0);
		for (j = 0; j < cases[i].first; j++)
			g_string_append (first, cases[i].glyph);
		g_string_append (first, "@host");
		padding = g_strnfill (cases[i].second, 'b');
		second = g_strconcat (padding, "@host", NULL);
		ids[0] = first->str;
		ids[1] = second;
		ids[2] = ids[1];
		g_free (padding);
		g_mime_message_id_list_clear (refs);
		g_mime_message_id_list_add_range (refs, ids, cases[i].third ? 3 : 2);
		separator = cases[i].fold ? g_strconcat (newline, "\t", NULL) : g_strdup (" ");
		if (cases[i].third)
			expected = g_strdup_printf (" <%s>%s<%s> <%s>%s", ids[0], separator, ids[1], ids[2], newline);
		else
			expected = g_strdup_printf (" <%s>%s<%s>%s", ids[0], separator, ids[1], newline);
		actual = testsuite_header_raw_text (g_mime_header_list_get_header (headers, "References"));
		testsuite_check ("References mutation folding boundary [%u]", i);
		if (!strcmp (actual, expected))
			testsuite_check_passed ();
		else
			testsuite_check_failed ("actual=\"%s\", expected=\"%s\"", actual, expected);
		g_free (separator);
		g_free (expected);
		g_free (second);
	}

	g_string_free (first, TRUE);
	g_object_unref (message);
}

static int
message_priority_get (GMimeMessage *message, guint type)
{
	switch (type) {
	case 0: return g_mime_message_get_importance (message);
	case 1: return g_mime_message_get_priority (message);
	default: return g_mime_message_get_x_priority (message);
	}
}

static void
message_priority_set (GMimeMessage *message, guint type, int value)
{
	switch (type) {
	case 0: g_mime_message_set_importance (message, (GMimeMessageImportance) value); break;
	case 1: g_mime_message_set_priority (message, (GMimeMessagePriority) value); break;
	default: g_mime_message_set_x_priority (message, (GMimeXMessagePriority) value); break;
	}
}

static void
test_message_priorities (void)
{
	static const char *names[] = { "Importance", "Priority", "X-Priority" };
	static const struct {
		const char *raw;
		int importance, priority, xpriority;
	} cases[] = {
		{ "\n", 1, 1, 3 },
		{ " hIgH \t\n", 2, 1, 3 },
		{ " LOW\n", 0, 1, 3 },
		{ " normal\n", 1, 1, 3 },
		{ " urgent\n", 1, 2, 3 },
		{ " NON-URGENT\n", 1, 0, 3 },
		{ " \302\240HIGH\342\200\203\n", 1, 1, 3 },
		{ "\vlow\f\n", 1, 1, 3 },
		{ " =?utf-8?q?high?=\n", 2, 1, 3 },
		{ " =?utf-8?q?urgent?=\n", 1, 2, 3 },
		{ " =?utf-8?q?=C2=A0HIGH=E2=80=83?=\n", 1, 1, 3 },
		{ " =\r\n?utf-8?q?high?=\n", 1, 1, 3 },
		{ " =?utf-8?q?high?\r=\n", 1, 1, 3 },
		{ " =?utf-8?q?ur=0D=0Agent?=\n", 1, 2, 3 },
		{ " =?utf-8?q?hi=0Agh?=\n", 2, 1, 3 },
		{ " =?utf.8?q?high?=\n", 2, 1, 3 },
		{ " high (comment)\n", 1, 1, 3 },
		{ " (comment)low\n", 1, 1, 3 },
		{ " \"urgent\"\n", 1, 1, 3 },
		{ " non-\r\n\turgent\n", 1, 1, 3 },
		{ " 0\n", 1, 1, 1 },
		{ " 1.9\n", 1, 1, 1 },
		{ " 2garbage\n", 1, 1, 2 },
		{ " 3 (Normal)\n", 1, 1, 3 },
		{ " 4 (Low)\n", 1, 1, 4 },
		{ " 5more\n", 1, 1, 5 },
		{ " 6\n", 1, 1, 5 },
		{ " 2147483647\n", 1, 1, 5 },
		{ " 2147483648\n", 1, 1, 3 },
		{ " 0000000000000002147483647x\n", 1, 1, 5 },
		{ " 0000000000000002147483648x\n", 1, 1, 3 },
		{ " +1\n", 1, 1, 3 },
		{ " -1\n", 1, 1, 3 },
		{ " (comment)1\n", 1, 1, 3 },
		{ " =?utf-8?q?1?=\n", 1, 1, 3 },
		{ "\v1\n", 1, 1, 3 },
		{ "\f1\n", 1, 1, 3 },
		{ " \302\2401\n", 1, 1, 3 },
		{ "\r\n\t2\n", 1, 1, 2 },
		{ " \377\n", 1, 1, 3 }
	};
	static const char *values[][5] = {
		{ "low", "normal", "high" },
		{ "non-urgent", "normal", "urgent" },
		{ "1 (Highest)", "2 (High)", "3 (Normal)", "4 (Low)", "5 (Lowest)" }
	};
	static const char *original[] = { " hIgH \t\n", " UrGent \t\n", " 001 (custom)\n" };
	static const int normal[] = { 1, 1, 3 }, high[] = { 2, 2, 1 }, low[] = { 0, 0, 5 };
	GMimeMessage *message = g_mime_message_new (FALSE);
	GMimeHeaderList *headers = g_mime_entity_get_header_list (GMIME_ENTITY (message));
	GMimeHeader *header, *duplicate, *replacement;
	GMimeStream *stream;
	GMimeParser *parser;
	GError *error = NULL;
	const char *input = "Importance: high\r\nPriority: urgent\r\nX-Priority: 2 (High)\r\n"
			    "Importance: low\r\nPriority: non-urgent\r\nX-Priority: 5\r\n\r\nbody";
	char *raw;
	guint i, type;
	int first, last, value, expected;

	for (type = 0; type < G_N_ELEMENTS (names); type++) {
		check (message_priority_get (message, type) == normal[type], "missing priority headers default to normal");
		message_priority_set (message, type, normal[type]);
	}
	check (g_mime_header_list_get_count (headers) == 0, "assigning absent normal priorities does not add headers");
	for (type = 0; type < G_N_ELEMENTS (names); type++)
		g_mime_header_list_append (headers, names[type], "", NULL);

	for (i = 0; i < G_N_ELEMENTS (cases); i++) {
		for (type = 0; type < G_N_ELEMENTS (names); type++) {
			header = g_mime_header_list_get_header (headers, names[type]);
			testsuite_header_set_raw_text (header, cases[i].raw);
		}
		testsuite_check ("message priority parsing [%u]", i);
		if (g_mime_message_get_importance (message) == cases[i].importance &&
		    g_mime_message_get_priority (message) == cases[i].priority &&
		    g_mime_message_get_x_priority (message) == cases[i].xpriority)
			testsuite_check_passed ();
		else
			testsuite_check_failed ("actual=%d,%d,%d; expected=%d,%d,%d",
				g_mime_message_get_importance (message), g_mime_message_get_priority (message),
				g_mime_message_get_x_priority (message), cases[i].importance, cases[i].priority, cases[i].xpriority);
	}

	for (type = 0; type < G_N_ELEMENTS (names); type++) {
		g_mime_header_list_clear (headers);
		first = type == 2 ? 1 : 0;
		last = type == 2 ? 5 : 2;
		for (value = first; value <= last; value++) {
			message_priority_set (message, type, value);
			expected = value - first;
			check (message_priority_get (message, type) == value &&
			       !g_strcmp0 (g_mime_entity_get_header (GMIME_ENTITY (message), names[type]), values[type][expected]),
			       "priority setters write canonical tokens and numeric labels");
		}
		header = g_mime_header_list_get_header (headers, names[type]);
		raw = g_strdup (testsuite_header_raw_text (header));
		for (i = 0; i < 3; i++) {
			int invalid = i == 0 ? -1 : (i == 1 ? last + 1 : G_MAXINT);
			g_test_expect_message ("gmime", G_LOG_LEVEL_CRITICAL, "*assertion*failed*");
			message_priority_set (message, type, invalid);
			g_test_assert_expected_messages ();
		}
		check (message_priority_get (message, type) == last &&
		       !strcmp (testsuite_header_raw_text (header), raw),
		       "invalid priority enum values preserve cache and raw header");
		g_free (raw);
		testsuite_header_set_raw_text (header, original[type]);
		g_mime_header_list_append (headers, names[type], values[type][low[type] - first], NULL);
		duplicate = g_mime_header_list_get_header_at (headers, 1);
		check (message_priority_get (message, type) == high[type], "the first priority header wins");
		testsuite_header_set_raw_text (duplicate, " unknown\n");
		message_priority_set (message, type, high[type]);
		check (g_mime_header_list_get_count (headers) == 1 &&
		       !strcmp (g_mime_header_get_value (header), values[type][high[type] - first]),
		       "equal priority assignments replace duplicate headers");
		g_mime_header_list_append (headers, names[type], "unknown", NULL);
		duplicate = g_mime_header_list_get_header_at (headers, 1);
		testsuite_header_set_raw_text (header, " unknown\n");
		g_mime_header_set_value (duplicate, NULL, values[type][low[type] - first], NULL);
		check (message_priority_get (message, type) == normal[type],
		       "an unknown first priority header does not fall back to a duplicate");
		g_mime_header_list_remove_at (headers, 0);
		check (message_priority_get (message, type) == low[type], "removal promotes the next priority header");
		g_mime_header_list_insert (headers, 0, names[type], values[type][high[type] - first], NULL);
		check (message_priority_get (message, type) == high[type], "earlier insertion refreshes the priority cache");
		replacement = g_mime_header_new (names[type], values[type][normal[type] - first], NULL);
		g_mime_header_list_replace (headers, replacement);
		check (message_priority_get (message, type) == normal[type], "replacement refreshes priority caches");
		g_mime_header_list_add (headers, replacement);
		g_mime_header_list_remove_at (headers, 0);
		testsuite_header_set_raw_text (replacement, original[type]);
		check (message_priority_get (message, type) == high[type],
		       "repeated priority header instances retain owner notifications");
		g_mime_header_list_append (headers, names[type], "unknown", NULL);
		message_priority_set (message, type, low[type]);
		check (message_priority_get (message, type) == low[type] &&
		       g_mime_header_list_get_count (headers) == 1,
		       "changed priorities replace the first header and remove duplicates");
		g_mime_header_list_clear (headers);
		testsuite_header_set_raw_text (replacement, original[type]);
		check (message_priority_get (message, type) == normal[type], "detached priority headers no longer update the message");
		g_object_unref (replacement);
	}
	g_object_unref (message);

	stream = g_mime_stream_mem_new_with_buffer (input, strlen (input));
	parser = g_mime_parser_new_with_stream (stream);
	message = g_mime_parser_construct_message (parser, NULL, &error);
	g_assert_no_error (error);
	check (g_mime_message_get_importance (message) == GMIME_MESSAGE_IMPORTANCE_HIGH &&
	       g_mime_message_get_priority (message) == GMIME_MESSAGE_PRIORITY_URGENT &&
	       g_mime_message_get_x_priority (message) == GMIME_X_MESSAGE_PRIORITY_HIGH,
	       "MIME parsing populates priorities from the first matching headers");
	g_mime_header_list_clear (g_mime_entity_get_header_list (GMIME_ENTITY (message)));
	check (g_mime_message_get_importance (message) == GMIME_MESSAGE_IMPORTANCE_NORMAL &&
	       g_mime_message_get_priority (message) == GMIME_MESSAGE_PRIORITY_NORMAL &&
	       g_mime_message_get_x_priority (message) == GMIME_X_MESSAGE_PRIORITY_NORMAL,
	       "clearing headers resets all priorities to normal");
	g_object_unref (message);
	g_object_unref (parser);
	g_object_unref (stream);
}

static void
test_message_dates (void)
{
	static const struct {
		const char *name;
		GDateTime * (*get) (GMimeMessage *);
		void (*set) (GMimeMessage *, GDateTime *);
	} properties[] = {
		{ "Date", g_mime_message_get_date, g_mime_message_set_date },
		{ "Resent-Date", g_mime_message_get_resent_date, g_mime_message_set_resent_date }
	};
	static const struct {
		const char *raw, *expected;
	} cases[] = {
		{ "\n", NULL },
		{ " invalid\n", NULL },
		{ " Fri, 28 Feb 2025 09:00:00 +0000\n", "2025-02-28T09:00:00Z" },
		{ " 28 Feb 2025 14:30 +0530 (comment)\n", "2025-02-28T14:30:00+05:30" },
		{ " Fri,\r\n\t28 Feb 2025 09:00:00 -0330\n", "2025-02-28T09:00:00-03:30" },
		{ " 1 Jan 0001 00:00:00 +0000\n", "2001-01-01T00:00:00Z" },
		{ " 31 Dec 9999 23:59:59 +0000\n", "9999-12-31T23:59:59Z" },
		{ " =?utf-8?b?RnJpLCAyOCBGZWIgMjAyNSAwOTowMDowMCArMDAwMA==?=\n", NULL }
	};
	const char *first = " Fri, 28 Feb 2025 09:00:00 +0000 (original)\n";
	const char *second = "Sat, 01 Mar 2025 10:00:00 +0000";
	const char *input = "Date: Fri, 28 Feb 2025 09:00:00 +0000\r\n"
			    "Resent-Date: Sat, 01 Mar 2025 10:00:00 +0000\r\n"
			    "Date: invalid\r\nResent-Date: invalid\r\n\r\nbody";
	GDateTime *date, *expected, *retained, *alias, *changed;
	GMimeHeader *header, *duplicate, *replacement;
	GMimeMessage *message;
	GMimeHeaderList *headers;
	GMimeStream *stream;
	GMimeParser *parser;
	GTimeZone *zone;
	GError *error = NULL;
	char *raw, *formatted;
	guint i, type;

	for (type = 0; type < G_N_ELEMENTS (properties); type++) {
		message = g_mime_message_new (TRUE);
		headers = g_mime_entity_get_header_list (GMIME_ENTITY (message));
		check (properties[type].get (message) == NULL, "missing or placeholder message dates are NULL");
		g_mime_header_list_clear (headers);
		g_mime_header_list_append (headers, properties[type].name, "", NULL);
		header = g_mime_header_list_get_header (headers, properties[type].name);
		for (i = 0; i < G_N_ELEMENTS (cases); i++) {
			testsuite_header_set_raw_text (header, cases[i].raw);
			date = properties[type].get (message);
			expected = cases[i].expected ? g_date_time_new_from_iso8601 (cases[i].expected, NULL) : NULL;
			check ((!expected && !date) ||
			       (expected && date && g_date_time_equal (date, expected) &&
				g_date_time_get_utc_offset (date) == g_date_time_get_utc_offset (expected)),
			       "message dates parse raw bytes with shared date recovery and timezone semantics");
			check (!strcmp (testsuite_header_raw_text (header), cases[i].raw),
			       "date getters do not rewrite raw headers");
			g_clear_pointer (&expected, g_date_time_unref);
		}
		testsuite_header_set_raw_text (header, first);
		g_mime_header_list_append (headers, properties[type].name, second, NULL);
		duplicate = g_mime_header_list_get_header_at (headers, 1);
		date = properties[type].get (message);
		retained = g_date_time_ref (date);
		check (g_date_time_get_day_of_month (date) == 28, "the first date header wins over duplicates");
		testsuite_header_set_raw_text (duplicate, " invalid\n");
		check (properties[type].get (message) != NULL &&
		       g_date_time_equal (properties[type].get (message), retained),
		       "later duplicate edits do not override the first date header");
		zone = g_time_zone_new_offset (19800);
		alias = g_date_time_to_timezone (retained, zone);
		g_time_zone_unref (zone);
		date = properties[type].get (message);
		properties[type].set (message, date);
		properties[type].set (message, alias);
		check (properties[type].get (message) == date &&
		       g_mime_header_list_get_count (headers) == (type ? 2 : 1) &&
		       (!type || !strcmp (testsuite_header_raw_text (header), first)),
		       "equal dates remove singleton duplicates but preserve resent dates and cached timezone");
		g_date_time_unref (alias);
		if (!type) {
			g_mime_header_list_append (headers, properties[type].name, second, NULL);
			duplicate = g_mime_header_list_get_header_at (headers, 1);
		}
		testsuite_header_set_raw_text (header, " invalid\n");
		g_mime_header_set_value (duplicate, NULL, second, NULL);
		check (properties[type].get (message) == NULL, "an invalid first date does not fall back to duplicates");
		g_mime_header_list_remove_at (headers, 0);
		check (g_date_time_get_day_of_month (properties[type].get (message)) == 1,
		       "removing the first date promotes the next header");
		g_mime_header_list_insert (headers, 0, properties[type].name, "", NULL);
		check (properties[type].get (message) == NULL, "earlier empty date insertion clears the cache");
		replacement = g_mime_header_new (properties[type].name, second, NULL);
		g_mime_header_list_replace (headers, replacement);
		check (g_date_time_get_day_of_month (properties[type].get (message)) == 1 &&
		       g_mime_header_list_get_count (headers) == 1, "date header replacement refreshes the cache");
		g_mime_header_list_add (headers, replacement);
		g_mime_header_list_remove_at (headers, 0);
		testsuite_header_set_raw_text (replacement, first);
		check (g_date_time_get_day_of_month (properties[type].get (message)) == 28,
		       "repeated date header instances retain owner notifications");
		g_mime_header_list_append (headers, properties[type].name, second, NULL);
		date = g_date_time_new_from_iso8601 ("2040-01-02T03:04:05.123456+05:30", NULL);
		properties[type].set (message, date);
		g_date_time_unref (date);
		date = properties[type].get (message);
		formatted = g_mime_utils_header_format_date (date);
		check (g_date_time_get_microsecond (date) == 123456 &&
		       g_date_time_get_utc_offset (date) == 19800 * G_TIME_SPAN_SECOND &&
		       !g_strcmp0 (g_mime_entity_get_header (GMIME_ENTITY (message), properties[type].name), formatted) &&
		       g_mime_header_list_get_count (headers) == (type ? 2 : 1) &&
		       (!type || !strcmp (g_mime_header_get_value (g_mime_header_list_get_header_at (headers, 1)), second)),
		       "date assignments retain precision and preserve only older resent headers");
		g_free (formatted);
		changed = g_date_time_add (date, 1);
		raw = g_strdup (testsuite_header_raw_text (replacement));
		properties[type].set (message, changed);
		check (properties[type].get (message) == changed &&
		       !strcmp (testsuite_header_raw_text (replacement), raw),
		       "subsecond date changes update the cache even when formatted seconds are unchanged");
		g_free (raw);
		g_test_expect_message ("gmime", G_LOG_LEVEL_CRITICAL, "*date != NULL*");
		properties[type].set (message, NULL);
		g_test_assert_expected_messages ();
		check (properties[type].get (message) == changed, "NULL date assignments are programming errors without mutation");
		for (i = 0; i < 2; i++) {
			zone = g_time_zone_new_offset (i ? -37 : 37);
			date = i ? g_date_time_new (zone, 9999, 12, 31, 23, 59, 59) :
				g_date_time_new (zone, 1, 1, 1, 0, 0, 0);
			g_time_zone_unref (zone);
			raw = g_strdup (testsuite_header_raw_text (replacement));
			g_test_expect_message ("gmime", G_LOG_LEVEL_CRITICAL, "*utc != NULL*");
			properties[type].set (message, date);
			g_test_assert_expected_messages ();
			check (properties[type].get (message) == changed &&
			       !strcmp (testsuite_header_raw_text (replacement), raw),
			       "unrepresentable UTC date formatting preserves the cache and raw header");
			g_free (raw);
			g_date_time_unref (date);
		}
		check (properties[1 - type].get (message) == NULL, "Date and Resent-Date assignments are independent");
		testsuite_header_set_raw_text (replacement, first);
		check (g_date_time_get_microsecond (properties[type].get (message)) == 0,
		       "raw date edits replace assigned precision with the parsed header value");
		g_mime_header_list_clear (headers);
		check (properties[type].get (message) == NULL, "clearing headers resets message dates");
		testsuite_header_set_raw_text (replacement, "Sat, 01 Mar 2025 10:00:00 +0000\n");
		check (properties[type].get (message) == NULL, "detached date headers no longer update the message");
		for (i = 0; i < 2; i++) {
			date = i ? g_date_time_new_utc (9999, 12, 31, 23, 59, 59.999999) :
				g_date_time_new_utc (1, 1, 1, 0, 0, 0);
			properties[type].set (message, date);
			check (properties[type].get (message) == date,
			       "assigned date caches retain full calendar boundaries instead of reparsing headers");
			g_date_time_unref (date);
		}
		zone = g_time_zone_new_offset (37);
		date = g_date_time_new (zone, 2025, 2, 28, 9, 0, 0.125);
		g_time_zone_unref (zone);
		properties[type].set (message, date);
		check (properties[type].get (message) == date &&
		       !g_strcmp0 (g_mime_entity_get_header (GMIME_ENTITY (message), properties[type].name),
				  "Fri, 28 Feb 2025 08:59:23 -0000"),
		       "sub-minute timezone dates retain caller precision while formatting the header in UTC");
		g_date_time_unref (date);
		properties[type].set (message, changed);
		g_object_unref (message);
		check (g_date_time_get_microsecond (changed) == 123457 &&
		       g_date_time_get_day_of_month (retained) == 28,
		       "caller-retained dates survive cache replacement and message finalization");
		g_date_time_unref (retained);
		g_date_time_unref (changed);
		g_object_unref (replacement);
	}

	stream = g_mime_stream_mem_new_with_buffer (input, strlen (input));
	parser = g_mime_parser_new_with_stream (stream);
	message = g_mime_parser_construct_message (parser, NULL, &error);
	g_assert_no_error (error);
	check (g_date_time_get_day_of_month (g_mime_message_get_date (message)) == 28 &&
	       g_date_time_get_day_of_month (g_mime_message_get_resent_date (message)) == 1,
	       "MIME parsing populates independent message dates from the first headers");
	g_mime_header_list_clear (g_mime_entity_get_header_list (GMIME_ENTITY (message)));
	check (!g_mime_message_get_date (message) && !g_mime_message_get_resent_date (message),
	       "clearing parsed headers resets both date caches");
	g_object_unref (message);
	g_object_unref (parser);
	g_object_unref (stream);

	{
		static const char *names[] = { "Resent-Date", "Resent-Message-Id", "Resent-To" };
		static const char *values[] = {
			"Sat, 01 Mar 2025 09:00:00 +0000", "<newer@example.com>", "newer@example.com",
			"Fri, 28 Feb 2025 09:00:00 +0000", "<older@example.com>", "older@example.com"
		};
		GMimeHeader *snapshots[G_N_ELEMENTS (values)];
		char *raw[G_N_ELEMENTS (values)];
		gboolean preserved;

		message = g_mime_message_new (FALSE);
		headers = g_mime_entity_get_header_list (GMIME_ENTITY (message));
		for (i = 0; i < G_N_ELEMENTS (values); i++) {
			g_mime_header_list_append (headers, names[i % G_N_ELEMENTS (names)], values[i], NULL);
			snapshots[i] = g_object_ref (g_mime_header_list_get_header_at (headers, i));
			raw[i] = g_strdup (testsuite_header_raw_text (snapshots[i]));
		}

		g_mime_message_set_resent_date (message, g_mime_message_get_resent_date (message));
		g_mime_message_set_resent_message_id (message, g_mime_message_get_resent_message_id (message));
		preserved = g_mime_header_list_get_count (headers) == G_N_ELEMENTS (values);
		for (i = 0; preserved && i < G_N_ELEMENTS (values); i++)
			preserved = g_mime_header_list_get_header_at (headers, i) == snapshots[i] &&
				!strcmp (testsuite_header_raw_text (snapshots[i]), raw[i]);
		check (preserved, "same-value resent assignments preserve the order and raw bytes of complete resent blocks");

		date = g_date_time_new_utc (2025, 3, 2, 9, 0, 0);
		g_mime_message_set_resent_date (message, date);
		g_date_time_unref (date);
		g_mime_message_set_resent_message_id (message, "updated@example.com");
		preserved = g_mime_header_list_get_count (headers) == G_N_ELEMENTS (values);
		for (i = 0; preserved && i < G_N_ELEMENTS (values); i++)
			preserved = g_mime_header_list_get_header_at (headers, i) == snapshots[i] &&
				(i < 2 || !strcmp (testsuite_header_raw_text (snapshots[i]), raw[i]));
		check (preserved && g_date_time_get_day_of_month (g_mime_message_get_resent_date (message)) == 2 &&
		       !g_strcmp0 (g_mime_message_get_resent_message_id (message), "updated@example.com"),
		       "changed resent assignments update only the newest fields without disturbing older blocks");

		for (i = 0; i < G_N_ELEMENTS (values); i++) {
			g_object_unref (snapshots[i]);
			g_free (raw[i]);
		}
		g_object_unref (message);
	}
}

static void
test_message_resent_addresses (void)
{
	static const struct {
		const char *name;
		GMimeAddressType type;
		GMimeInternetAddressList * (*get) (GMimeMessage *);
	} properties[] = {
		{ "Resent-From", GMIME_ADDRESS_TYPE_RESENT_FROM, g_mime_message_get_resent_from },
		{ "Resent-Reply-To", GMIME_ADDRESS_TYPE_RESENT_REPLY_TO, g_mime_message_get_resent_reply_to },
		{ "Resent-To", GMIME_ADDRESS_TYPE_RESENT_TO, g_mime_message_get_resent_to },
		{ "Resent-Cc", GMIME_ADDRESS_TYPE_RESENT_CC, g_mime_message_get_resent_cc },
		{ "Resent-Bcc", GMIME_ADDRESS_TYPE_RESENT_BCC, g_mime_message_get_resent_bcc },
		{ "Sender", GMIME_ADDRESS_TYPE_SENDER, g_mime_message_get_sender },
		{ "Resent-Sender", GMIME_ADDRESS_TYPE_RESENT_SENDER, g_mime_message_get_resent_sender }
	};
	const char *original = " First\r\n\t<first@example.com> (original)\r\n";
	GMimeInternetAddressList *list, *members;
	GMimeInternetAddress *group, *member;
	GMimeHeader *first, *replacement, *unrelated;
	GMimeMessage *message;
	GMimeHeaderList *headers;
	GMimeParserOptions *options;
	GMimeStream *stream;
	GMimeParser *parser;
	GError *error = NULL;
	const char *value;
	char *text, *wire, *name;
	guint type;

	check (GMIME_ADDRESS_TYPE_SENDER == 0 && GMIME_ADDRESS_TYPE_BCC == 5 &&
	       GMIME_ADDRESS_TYPE_RESENT_FROM == 6 && GMIME_ADDRESS_TYPE_RESENT_BCC == 10 &&
	       GMIME_ADDRESS_TYPE_RESENT_SENDER == 11,
	       "adding resent address types preserves existing enum values");
	for (type = 0; type < G_N_ELEMENTS (properties); type++) {
		message = g_mime_message_new (FALSE);
		headers = g_mime_entity_get_header_list (GMIME_ENTITY (message));
		list = g_object_ref (properties[type].get (message));
		check (list == g_mime_message_get_addresses (message, properties[type].type) &&
		       g_mime_internet_address_list_length (list) == 0 && g_mime_header_list_get_count (headers) == 0,
		       "resent address getters return persistent empty lists without creating headers");
		g_mime_header_list_append (headers, properties[type].name, "Empty:;", NULL);
		group = g_mime_internet_address_list_get_address (list, 0);
		check (g_mime_internet_address_list_length (list) == 1 &&
		       GMIME_IS_GROUP_ADDRESS (group) &&
		       g_mime_internet_address_list_length (g_mime_group_address_get_members (GMIME_GROUP_ADDRESS (group))) == 0,
		       "originator and recipient address lists preserve empty groups");
		g_mime_internet_address_list_clear (list);
		g_mime_header_list_append (headers, properties[type].name, "", NULL);
		first = g_object_ref (g_mime_header_list_get_header_at (headers, 0));
		testsuite_header_set_raw_text (first, original);
		name = g_ascii_strdown (properties[type].name, -1);
		g_mime_header_list_append (headers, name, "Second <second@example.com>", NULL);
		g_free (name);
		text = g_mime_internet_address_list_to_string (list, NULL, FALSE);
		check (g_mime_internet_address_list_length (list) == 2 &&
		       !strcmp (text, "First <first@example.com>, Second <second@example.com>") &&
		       !strcmp (testsuite_header_raw_text (first), original),
		       "resent address parsing combines duplicate headers without rewriting their raw bytes");
		g_free (text);
		g_mime_header_list_insert (headers, 0, properties[type].name, "Before <before@example.com>", NULL);
		testsuite_header_set_raw_text (first, " Updated <updated@example.com>\n");
		text = g_mime_internet_address_list_to_string (list, NULL, FALSE);
		check (properties[type].get (message) == list &&
		       !strcmp (text, "Before <before@example.com>, Updated <updated@example.com>, Second <second@example.com>"),
		       "earlier resent header insertion and editing refresh the same list in header order");
		g_free (text);
		g_mime_header_list_remove_header (headers, first);
		testsuite_header_set_raw_text (first, " Detached <detached@example.com>\n");
		text = g_mime_internet_address_list_to_string (list, NULL, FALSE);
		check (!strcmp (text, "Before <before@example.com>, Second <second@example.com>"),
		       "resent header removal refreshes the list and detaches removed instances");
		g_free (text);
		replacement = g_mime_header_new (properties[type].name, "Group: Member <member@example.com>;", NULL);
		g_mime_header_list_replace (headers, replacement);
		check (g_mime_internet_address_list_length (list) == 1 && g_mime_header_list_get_count (headers) == 1,
		       "resent header replacement collapses duplicates and updates the list");
		g_mime_header_list_insert_header (headers, 0, replacement);
		check (g_mime_internet_address_list_length (list) == 2,
		       "inserting a resent header that shares the final instance reconstructs ordered addresses");
		unrelated = g_mime_header_new ("X-Unrelated", "preserved", NULL);
		g_mime_header_list_set_header_at (headers, 0, unrelated);
		g_object_unref (unrelated);
		testsuite_header_set_raw_text (replacement, " Group: Member <member@example.com>;\n");
		check (g_mime_internet_address_list_length (list) == 1,
		       "indexed replacement and repeated-instance removal preserve resent header notifications");
		g_mime_header_list_add (headers, replacement);
		group = g_mime_internet_address_list_get_address (list, 0);
		members = g_mime_group_address_get_members (GMIME_GROUP_ADDRESS (group));
		member = g_mime_internet_address_list_get_address (members, 0);
		g_mime_internet_address_set_name (group, "Changed");
		g_mime_internet_address_set_name (member, "Renamed");
		check (g_mime_mailbox_address_set_addrspec (GMIME_MAILBOX_ADDRESS (member), "renamed@example.com", &error),
		       "resent group member address changes succeed");
		g_assert_no_error (error);
		value = g_mime_entity_get_header (GMIME_ENTITY (message), properties[type].name);
		check (strstr (value, "Changed: Renamed <renamed@example.com>;") != NULL &&
		       g_mime_header_list_index_of (headers, properties[type].name) ==
		       g_mime_header_list_last_index_of (headers, properties[type].name) &&
		       !g_strcmp0 (g_mime_entity_get_header (GMIME_ENTITY (message), "X-Unrelated"), "preserved"),
		       "nested resent address edits serialize the retained list and remove only matching duplicates");
		g_mime_internet_address_list_clear (list);
		check (!g_mime_header_list_contains (headers, properties[type].name) &&
		       g_mime_header_list_get_count (headers) == 1, "emptying resent lists removes matching headers");
		g_mime_header_list_append (headers, properties[type].name, "(", NULL);
		g_mime_header_list_append (headers, properties[type].name, "", NULL);
		g_mime_internet_address_list_clear (list);
		check (!g_mime_header_list_contains (headers, properties[type].name),
		       "clearing an already empty resent list removes malformed and empty duplicates");
		check (g_mime_message_add_mailbox (message, properties[type].type, "Added", "added@example.com", &error),
		       "generic mailbox addition accepts resent address types");
		g_assert_no_error (error);
		check (g_mime_internet_address_list_length (list) == 1 &&
		       g_mime_internet_address_list_length (g_mime_message_get_to (message)) == 0,
		       "resent address additions do not change ordinary recipients");
		g_mime_header_list_clear (headers);
		check (properties[type].get (message) == list && g_mime_internet_address_list_length (list) == 0,
		       "header clearing empties the same resent list");
		g_mime_message_add_mailbox (message, properties[type].type, NULL, "retained@example.com", &error);
		g_assert_no_error (error);
		g_object_unref (message);
		member = g_mime_internet_address_list_get_address (list, 0);
		g_mime_internet_address_set_name (member, "After finalization");
		g_mime_internet_address_list_clear (list);
		check (g_mime_internet_address_list_length (list) == 0, "retained resent lists detach safely from finalized owners");
		g_object_unref (list);
		g_object_unref (replacement);
		g_object_unref (first);

		wire = g_strdup_printf ("%s: local\r\n%s: Accepted <accepted@example.com>\r\n\r\nbody",
					properties[type].name, properties[type].name);
		stream = g_mime_stream_mem_new_with_buffer (wire, strlen (wire));
		parser = g_mime_parser_new_with_stream (stream);
		options = g_mime_parser_options_new ();
		g_mime_parser_options_set_allow_addresses_without_domain (options, FALSE);
		message = g_mime_parser_construct_message (parser, options, &error);
		g_assert_no_error (error);
		list = properties[type].get (message);
		text = g_mime_internet_address_list_to_string (list, NULL, FALSE);
		check (g_mime_internet_address_list_length (list) == 1 &&
		       !strcmp (text, "Accepted <accepted@example.com>"),
		       "MIME parsing applies message parser options to each resent address header");
		g_free (text);
		headers = g_mime_entity_get_header_list (GMIME_ENTITY (message));
		testsuite_header_set_raw_text (g_mime_header_list_get_header_at (headers, 0), " another-local\n");
		check (g_mime_internet_address_list_length (list) == 1,
		       "resent raw-header edits retain message parser options");
		g_mime_parser_options_free (options);
		g_object_unref (message);
		g_object_unref (parser);
		g_object_unref (stream);
		g_free (wire);
	}
	{
		static const char *names[] = { "Sender", "From", "Reply-To", "To", "Cc", "Bcc" };
		guint i;

		message = g_mime_message_new (FALSE);
		headers = g_mime_entity_get_header_list (GMIME_ENTITY (message));
		for (i = 0; i < G_N_ELEMENTS (names); i++) {
			g_mime_header_list_append (headers, names[i], "Existing <existing@example.com>", NULL);
			g_mime_header_list_append (headers, names[i], "Duplicate <duplicate@example.com>", NULL);
			g_mime_internet_address_list_clear (g_mime_message_get_addresses (message, (GMimeAddressType) i));
			check (!g_mime_header_list_contains (headers, names[i]),
			       "ordinary address lists share the empty-list header removal behavior");
		}
		g_object_unref (message);
	}
	{
		GMimeInternetAddressList *senders, *resent_senders;
		GMimeInternetAddress *detached;

		message = g_mime_message_new (FALSE);
		headers = g_mime_entity_get_header_list (GMIME_ENTITY (message));
		senders = g_mime_message_get_sender (message);
		resent_senders = g_mime_message_get_resent_sender (message);
		g_mime_header_list_append (headers, "Sender", "Ordinary:;", NULL);
		g_mime_header_list_append (headers, "Resent-Sender", "Resent:;", NULL);
		group = g_mime_internet_address_list_get_address (resent_senders, 0);
		g_mime_internet_address_set_name (group, "Changed");
		check (senders != resent_senders && senders == g_mime_message_get_sender (message) &&
		       !strcmp (g_mime_entity_get_header (GMIME_ENTITY (message), "Sender"), "Ordinary:;") &&
		       strstr (g_mime_entity_get_header (GMIME_ENTITY (message), "Resent-Sender"), "Changed:") != NULL,
		       "Sender and Resent-Sender lists and nested notifications are independent");
		detached = g_object_ref (group);
		g_mime_header_list_remove_all (headers, "Resent-Sender");
		g_mime_internet_address_set_name (detached, "Detached");
		check (g_mime_internet_address_list_length (resent_senders) == 0 &&
		       !g_mime_header_list_contains (headers, "Resent-Sender") &&
		       g_mime_internet_address_list_length (senders) == 1,
		       "removed sender groups detach without recreating headers or changing the other list");
		g_object_unref (detached);
		g_object_unref (message);
	}
}

static void
test_message_recipients (void)
{
	static const struct {
		const char *sender, *from;
		gboolean resent;
	} selectors[] = {
		{ NULL, NULL, FALSE },
		{ "", "", FALSE },
		{ "(", "(", FALSE },
		{ "Group: member@example.com;", NULL, TRUE },
		{ "Empty:;", NULL, TRUE },
		{ "(comment)", NULL, FALSE },
		{ ", Sender <sender@example.com>", NULL, TRUE },
		{ NULL, "Empty:;", TRUE },
		{ NULL, "Author <author@example.com>", TRUE },
		{ "Sender <sender@example.com>, trailing", NULL, TRUE },
		{ "Sender <sender@example.com>", "(", TRUE }
	};
	static const struct {
		const char *left, *right;
		gboolean equal;
	} pairs[] = {
		{ "local@EXAMPLE.COM", "LOCAL@example.com", FALSE },
		{ "local@EXAMPLE.COM", "local@example.com", TRUE },
		{ "\303\251@example.com", "\303\211@example.com", FALSE },
		{ "\317\202@example.com", "\316\243@example.com", FALSE },
		{ "\315\205@example.com", "\316\231@example.com", FALSE },
		{ "\342\205\260@example.com", "\342\205\240@example.com", FALSE },
		{ "\342\223\220@example.com", "\342\222\266@example.com", FALSE },
		{ "\304\261@example.com", "I@example.com", FALSE },
		{ "\305\277@example.com", "S@example.com", FALSE },
		{ "\342\204\252@example.com", "k@example.com", FALSE },
		{ "\303\237@example.com", "SS@example.com", FALSE },
		{ "\303\251@example.com", "e\314\201@example.com", FALSE },
		{ "a@b\303\274cher.example", "a@xn--bcher-kva.example", TRUE },
		{ "a@b\303\274cher.example", "a@XN--BCHER-KVA.EXAMPLE", TRUE },
		{ "A@b\303\274cher.example", "a@xn--bcher-kva.example", FALSE },
		{ "a@[IPv6:ABCD::1]", "a@[ipv6:abcd::1]", TRUE },
		{ "User", "user", FALSE },
		{ "\"local\"@example.com", "local@example.com", FALSE },
		{ "prefix@example.com", "prefix@example.com.extra", FALSE },
		{ "a@example.com", "aa@example.com", FALSE },
		{ "\360\220\220\250@example.com", "\360\220\220\200@example.com", FALSE },
		{ "", "", TRUE }
	};
	GMimeMessage *message = g_mime_message_new (FALSE);
	GMimeHeaderList *headers = g_mime_entity_get_header_list (GMIME_ENTITY (message));
	GMimeInternetAddressList *recipients, *unique, *legacy, *selected;
	GMimeInternetAddress *first, *left, *right, *sender;
	GError *error = NULL;
	guint i;

	recipients = g_mime_message_get_recipients (message, FALSE);
	unique = g_mime_message_get_recipients (message, TRUE);
	check (recipients != unique && !g_mime_internet_address_list_length (recipients) &&
	       !g_mime_internet_address_list_length (unique) && !g_mime_message_get_all_recipients (message),
	       "new recipient snapshots are non-NULL when empty while the legacy accessor remains nullable");
	g_object_unref (recipients);
	g_object_unref (unique);
	g_mime_header_list_append (headers, "To",
		"Root: First <same@example.com>, Inner: Second <second@example.com>, Duplicate <same@EXAMPLE.COM>;;, Tail <tail@example.com>", NULL);
	g_mime_header_list_append (headers, "Cc", "Third <third@example.com>, Again <second@EXAMPLE.COM>", NULL);
	g_mime_header_list_append (headers, "Bcc", "Hidden <hidden@example.com>", NULL);
	recipients = g_mime_message_get_recipients (message, FALSE);
	unique = g_mime_message_get_recipients (message, TRUE);
	legacy = g_mime_message_get_all_recipients (message);
	first = g_mime_internet_address_list_get_address (
		g_mime_group_address_get_members (GMIME_GROUP_ADDRESS (
			g_mime_internet_address_list_get_address (g_mime_message_get_to (message), 0))), 0);
	check (g_mime_internet_address_list_length (recipients) == 7 &&
	       g_mime_internet_address_list_length (unique) == 5 &&
	       g_mime_internet_address_list_get_address (recipients, 0) == first &&
	       g_mime_internet_address_list_get_address (unique, 0) == first &&
	       !strcmp (g_mime_internet_address_get_name (g_mime_internet_address_list_get_address (unique, 1)), "Second") &&
	       !strcmp (g_mime_internet_address_get_name (g_mime_internet_address_list_get_address (unique, 2)), "Tail") &&
	       !strcmp (g_mime_internet_address_get_name (g_mime_internet_address_list_get_address (unique, 4)), "Hidden"),
	       "recipient snapshots flatten groups depth-first and keep the first mailbox across To, Cc and Bcc");
	check (g_mime_internet_address_list_length (legacy) == 5 &&
	       GMIME_IS_GROUP_ADDRESS (g_mime_internet_address_list_get_address (legacy, 0)),
	       "legacy recipients preserve ordinary groups and duplicates");
	g_object_unref (legacy);
	g_mime_internet_address_set_name (first, "Shared");
	check (strstr (g_mime_entity_get_header (GMIME_ENTITY (message), "To"), "Shared") != NULL,
	       "recipient snapshots retain original mailboxes whose edits update their source message");
	g_mime_internet_address_list_clear (unique);
	check (g_mime_internet_address_list_length (g_mime_message_get_to (message)) == 2 &&
	       g_mime_internet_address_list_length (recipients) == 7, "snapshot membership is independent of source and other snapshots");
	g_object_unref (unique);
	g_object_unref (message);
	g_mime_internet_address_set_name (g_mime_internet_address_list_get_address (recipients, 0), "Owner gone");
	g_mime_internet_address_list_clear (recipients);
	g_object_unref (recipients);

	for (i = 0; i < G_N_ELEMENTS (selectors); i++) {
		message = g_mime_message_new (FALSE);
		headers = g_mime_entity_get_header_list (GMIME_ENTITY (message));
		g_mime_header_list_append (headers, "To", "Ordinary <ordinary@example.com>", NULL);
		g_mime_header_list_append (headers, "Sender", "Ordinary-Sender:;", NULL);
		g_mime_header_list_append (headers, "Resent-To", "Resent <resent@example.com>", NULL);
		g_mime_header_list_append (headers, "Resent-Reply-To", "Reply <reply@example.com>", NULL);
		if (selectors[i].sender)
			g_mime_header_list_append (headers, "Resent-Sender", selectors[i].sender, NULL);
		if (selectors[i].from)
			g_mime_header_list_append (headers, "Resent-From", selectors[i].from, NULL);
		selected = g_mime_message_get_recipients (message, TRUE);
		testsuite_check ("parsed resent recipient selector [%u]", i);
		if (g_mime_internet_address_list_length (selected) == 1 &&
		    !strcmp (g_mime_mailbox_address_get_addrspec (GMIME_MAILBOX_ADDRESS (
			g_mime_internet_address_list_get_address (selected, 0))),
			selectors[i].resent ? "resent@example.com" : "ordinary@example.com"))
			testsuite_check_passed ();
		else
			testsuite_check_failed ("unexpected recipient selection");
		g_object_unref (selected);
		if (selectors[i].resent) {
			g_mime_internet_address_list_clear (g_mime_message_get_resent_to (message));
			selected = g_mime_message_get_recipients (message, FALSE);
			check (g_mime_internet_address_list_length (selected) == 0,
			       "empty selected resent recipient lists do not fall back to ordinary recipients");
			g_object_unref (selected);
		}
		g_object_unref (message);
	}

	for (i = 0; i < G_N_ELEMENTS (pairs); i++) {
		message = g_mime_message_new (FALSE);
		left = g_mime_mailbox_address_new ("First", pairs[i].left, &error);
		g_assert_no_error (error);
		right = g_mime_mailbox_address_new ("Second", pairs[i].right, &error);
		g_assert_no_error (error);
		if (i == 0) {
			g_mime_domain_list_add (g_mime_mailbox_address_get_route (GMIME_MAILBOX_ADDRESS (left)), "router.example");
			g_mime_domain_list_add (g_mime_mailbox_address_get_route (GMIME_MAILBOX_ADDRESS (right)), "other.example");
		}
		g_mime_internet_address_list_add (g_mime_message_get_to (message), left);
		g_mime_internet_address_list_add (g_mime_message_get_bcc (message), right);
		if (i == 0) {
			g_mime_internet_address_list_add (g_mime_message_get_bcc (message), left);
			selected = g_mime_message_get_recipients (message, FALSE);
			check (g_mime_internet_address_list_length (selected) == 3 &&
			       g_mime_internet_address_list_get_address (selected, 2) == left,
			       "nonunique recipient snapshots preserve repeated references to the same mailbox");
			g_object_unref (selected);
		}
		selected = g_mime_message_get_recipients (message, TRUE);
		testsuite_check ("IDN-canonicalized recipient deduplication [%u]", i);
		if (g_mime_internet_address_list_length (selected) == (pairs[i].equal ? 1 : 2) &&
		    g_mime_internet_address_list_get_address (selected, 0) == left)
			testsuite_check_passed ();
		else
			testsuite_check_failed ("unexpected deduplication");
		g_object_unref (selected);
		g_object_unref (left);
		g_object_unref (right);
		g_object_unref (message);
	}

	message = g_mime_message_new (FALSE);
	headers = g_mime_entity_get_header_list (GMIME_ENTITY (message));
	g_mime_header_list_append (headers, "To", "Ordinary <ordinary@example.com>", NULL);
	sender = g_mime_mailbox_address_new (NULL, "", &error);
	g_assert_no_error (error);
	g_mime_internet_address_list_add (g_mime_message_get_resent_sender (message), sender);
	selected = g_mime_message_get_recipients (message, FALSE);
	check (!g_mime_internet_address_list_length (selected), "an assigned empty mailbox still activates resent recipient selection");
	g_object_unref (selected);
	g_mime_header_list_append (headers, "Resent-Sender", "(", NULL);
	g_mime_header_list_append (headers, "Resent-Sender", "Later <later@example.com>", NULL);
	selected = g_mime_message_get_recipients (message, FALSE);
	check (!g_mime_internet_address_list_length (selected),
	       "a later parsed Resent-Sender keeps resent recipient selection active");
	g_object_unref (selected);
	g_mime_internet_address_list_clear (g_mime_message_get_resent_sender (message));
	selected = g_mime_message_get_recipients (message, FALSE);
	check (g_mime_internet_address_list_length (selected) == 1,
	       "clearing all resent senders restores ordinary recipient selection");
	g_object_unref (selected);
	g_mime_header_list_append (headers, "To", "First <same@example.com>, Again <SAME@example.com>", NULL);
	g_mime_header_list_append (headers, "Resent-From", "Active:;", NULL);
	g_mime_header_list_append (headers, "Resent-To", "One <one@example.com>", NULL);
	g_mime_header_list_append (headers, "Resent-Cc", "Again <one@EXAMPLE.COM>", NULL);
	selected = g_mime_message_get_recipients (message, TRUE);
	check (g_mime_internet_address_list_length (selected) == 1 &&
	       !strcmp (g_mime_internet_address_get_name (g_mime_internet_address_list_get_address (selected, 0)), "One"),
	       "selected resent mailboxes use the same first-occurrence deduplication");
	g_object_unref (selected);
	g_object_unref (sender);
	g_object_unref (message);
}

static void
test_part_content_duration (void)
{
	static const gunichar whitespace[] = {
		'\t', '\n', '\v', '\f', '\r', ' ', 0x85, 0xa0, 0x1680,
		0x2000, 0x2001, 0x2002, 0x2003, 0x2004, 0x2005, 0x2006,
		0x2007, 0x2008, 0x2009, 0x200a, 0x2028, 0x2029, 0x202f, 0x205f, 0x3000
	};
	static const struct {
		const char *raw;
		int duration;
	} cases[] = {
		{ "\n", -1 },
		{ " \r\n\t\n", -1 },
		{ " 0\r\n", 0 },
		{ " 000007 \t\r\n", 7 },
		{ " \t\v\f42\r\n", -1 },
		{ " 2147483647\n", G_MAXINT32 },
		{ " 00000000002147483647\n", G_MAXINT32 },
		{ " 2147483648\n", -1 },
		{ " 9999999999999999999999999999\n", -1 },
		{ " -1\n", -1 },
		{ " +7\n", -1 },
		{ " 7 (seconds)\n", -1 },
		{ " (comment) 7\n", -1 },
		{ " 7.0\n", -1 },
		{ " 7 0\n", -1 },
		{ " 0x7\n", -1 },
		{ " \302\2407\n", -1 },
		{ " 7\302\240\n", -1 },
		{ " \302\2057\n", -1 },
		{ " \342\200\2037\n", -1 },
		{ " 7\302\2400\n", -1 },
		{ " \357\274\227\n", -1 },
		{ " =?us-ascii?q?7?=\n", 7 }
	};
	GMimePart *part;
	GMimeEntity *entity;
	GMimeHeaderList *headers;
	GMimeHeader *first, *later, *replacement;
	GMimeStream *stream;
	GMimeParser *parser;
	GError *error = NULL;
	char *wire;
	guint i, side, type;

	for (type = 0; type < 2; type++) {
		part = type ? GMIME_PART (g_mime_text_part_new ()) : g_mime_part_new ();
		entity = GMIME_ENTITY (part);
		headers = g_mime_entity_get_header_list (entity);
		for (i = 0; i < G_N_ELEMENTS (whitespace); i++) {
			char buffer[7] = { 0 };
			int expected = whitespace[i] == ' ' || whitespace[i] == '\t' ||
				whitespace[i] == '\r' || whitespace[i] == '\n' ? 7 : -1;

			g_unichar_to_utf8 (whitespace[i], buffer);
			for (side = 0; side < 3; side++) {
				char *value = g_strconcat (" \t", side != 1 ? buffer : "", "007",
							  side != 0 ? buffer : "", "\r\n", NULL);
				char *raw;

				g_mime_header_list_set (headers, "Content-Duration", value, NULL);
				first = g_mime_header_list_get_header (headers, "Content-Duration");
				raw = g_strdup (testsuite_header_raw_text (first));
				testsuite_check ("Decoded Content-Duration whitespace U+%04X [%u/%u]", whitespace[i], type, side);
				if (g_mime_part_get_content_duration (part) == expected &&
				    g_mime_part_get_content_duration (part) == expected &&
				    !strcmp (testsuite_header_raw_text (first), raw))
					testsuite_check_passed ();
				else
					testsuite_check_failed ("unexpected duration or raw-header mutation");
				g_free (raw);
				g_free (value);
			}
		}
		g_object_unref (part);
	}

	for (i = 0; i < G_N_ELEMENTS (cases); i++) {
		part = g_mime_part_new ();
		entity = GMIME_ENTITY (part);
		headers = g_mime_entity_get_header_list (entity);
		g_mime_header_list_clear (headers);
		check (g_mime_part_get_content_duration (part) == -1 && g_mime_header_list_get_count (headers) == 0,
		       "missing content duration returns -1 without creating headers");
		g_mime_header_list_append (headers, "Content-Duration", "", NULL);
		first = g_mime_header_list_get_header_at (headers, 0);
		testsuite_header_set_raw_text (first, cases[i].raw);
		g_mime_header_list_append (headers, "content-duration", "99", NULL);
		testsuite_check ("Content-Duration parsing [%u]", i);
		if (g_mime_part_get_content_duration (part) == cases[i].duration &&
		    !strcmp (testsuite_header_raw_text (first), cases[i].raw))
			testsuite_check_passed ();
		else
			testsuite_check_failed ("unexpected duration or raw-header mutation");
		g_mime_part_set_content_duration (part, cases[i].duration);
		check (g_mime_header_list_get_count (headers) == 2 &&
		       !strcmp (testsuite_header_raw_text (first), cases[i].raw),
		       "same cached duration preserves raw spelling and duplicate headers, including invalid values");
		testsuite_header_set_raw_text (g_mime_header_list_get_header_at (headers, 1), " 100\n");
		g_mime_part_set_content_duration (part, cases[i].duration);
		check (g_mime_header_list_get_count (headers) == (cases[i].duration < 0 ? 0 : 1) &&
		       g_mime_part_get_content_duration (part) == cases[i].duration,
		       "later duplicate edits invalidate the cache before assignment replaces matching duplicates");
		g_mime_part_set_content_duration (part, G_MAXINT32);
		check (g_mime_part_get_content_duration (part) == G_MAXINT32 &&
		       g_mime_header_list_get_count (headers) == 1 &&
		       !strcmp (g_mime_entity_get_header (entity, "Content-Duration"), "2147483647"),
		       "changed duration assignments use invariant decimal formatting");
		g_test_expect_message ("gmime", G_LOG_LEVEL_CRITICAL, "*duration >= -1*");
		g_mime_part_set_content_duration (part, -2);
		g_test_assert_expected_messages ();
		check (g_mime_part_get_content_duration (part) == G_MAXINT32 &&
		       !strcmp (g_mime_entity_get_header (entity, "Content-Duration"), "2147483647"),
		       "invalid negative duration assignments leave cache and headers unchanged");
		g_mime_header_list_clear (headers);
		g_mime_header_list_append (headers, "Content-Duration", "invalid", NULL);
		g_mime_header_list_append (headers, "Content-Duration", "9", NULL);
		g_mime_part_set_content_duration (part, -1);
		check (!g_mime_header_list_contains (headers, "Content-Duration") &&
		       g_mime_part_get_content_duration (part) == -1,
		       "clearing duration before reading removes malformed headers and duplicates");
		g_object_unref (part);
	}

	part = g_mime_part_new ();
	entity = GMIME_ENTITY (part);
	headers = g_mime_entity_get_header_list (entity);
	g_mime_header_list_clear (headers);
	g_mime_header_list_append (headers, "Content-Duration", "7", NULL);
	first = g_object_ref (g_mime_header_list_get_header_at (headers, 0));
	g_mime_header_list_append (headers, "Content-Duration", "9", NULL);
	later = g_mime_header_list_get_header_at (headers, 1);
	check (g_mime_part_get_content_duration (part) == 7, "duration getter uses the first duplicate");
	g_mime_header_list_insert (headers, 0, "Content-Duration", "3", NULL);
	check (g_mime_part_get_content_duration (part) == 3, "earlier insertion changes the selected duration");
	testsuite_header_set_raw_text (first, " 8\n");
	check (g_mime_part_get_content_duration (part) == 3, "later header edits do not replace the first duration");
	g_mime_header_list_remove_at (headers, 0);
	check (g_mime_part_get_content_duration (part) == 8, "removal reparses the surviving first duration");
	g_mime_header_list_remove_header (headers, first);
	testsuite_header_set_raw_text (first, " 10\n");
	check (g_mime_part_get_content_duration (part) == 9, "removed duration headers detach from their former owner");
	g_mime_header_list_insert_header (headers, 0, later);
	check (g_mime_part_get_content_duration (part) == 9, "repeated header instances preserve duration parsing");
	replacement = g_mime_header_new ("Content-Duration", "11", NULL);
	g_mime_header_list_set_header_at (headers, 0, replacement);
	check (g_mime_part_get_content_duration (part) == 11, "indexed replacement invalidates the duration cache");
	g_mime_header_list_replace (headers, first);
	check (g_mime_part_get_content_duration (part) == 10 && g_mime_header_list_get_count (headers) == 1,
	       "replacing duration headers collapses duplicates and reparses");
	g_mime_header_list_clear (headers);
	check (g_mime_part_get_content_duration (part) == -1, "clearing all headers resets the duration cache");
	g_object_unref (replacement);
	g_object_unref (first);
	g_object_unref (part);

	wire = g_strdup ("Content-Type: text/plain\r\nContent-Duration: 0007\r\nContent-Duration: 9\r\n\r\nbody");
	stream = g_mime_stream_mem_new_with_buffer (wire, strlen (wire));
	parser = g_mime_parser_new_with_stream (stream);
	entity = g_mime_parser_construct_part (parser, NULL, &error);
	g_assert_no_error (error);
	check (GMIME_IS_TEXT_PART (entity) && g_mime_part_get_content_duration (GMIME_PART (entity)) == 7,
	       "parsed text parts inherit content duration and first-header semantics");
	g_mime_part_set_content_duration (GMIME_PART (entity), 0);
	check (!strcmp (g_mime_entity_get_header (entity, "Content-Duration"), "0") &&
	       g_mime_header_list_index_of (g_mime_entity_get_header_list (entity), "Content-Duration") ==
	       g_mime_header_list_last_index_of (g_mime_entity_get_header_list (entity), "Content-Duration"),
	       "assigning zero to a parsed text part replaces duplicate duration headers");
	g_object_unref (entity);
	g_object_unref (parser);
	g_object_unref (stream);
	g_free (wire);
}

static void
test_text_part_charset_errors (void)
{
	GMimeTextPart *part = g_mime_text_part_new ();
	GMimeContent *content;
	GMimeStream *stream;
	TestStreamFailure *failure;
	TestContent *custom;
	GError *error = NULL;
	char *text;
	guint i, j, offset;

	content = g_mime_content_new ();
	g_mime_part_set_content (GMIME_PART (part), content);
	g_object_unref (content);
	check (!g_mime_text_part_get_text_with_charset (part, "utf-8", &error),
	       "explicit text decoding rejects content without a backing stream");
	g_assert_error (error, GMIME_ERROR, GMIME_ERROR_INVALID_OPERATION);
	g_clear_error (&error);

	for (i = 0; i < 2; i++) {
		for (j = 0; j < 2; j++) {
			for (offset = 0; offset <= 8; offset++) {
				failure = g_object_new (test_stream_failure_get_type (), NULL);
				failure->parent_instance.source = g_mime_stream_mem_new_with_buffer ("SGVsbG8=", 8);
				failure->fail_after = offset;
				failure->onebyte = i;
				failure->unset_error = j;
				g_mime_stream_construct (GMIME_STREAM (failure), 0, -1);
				content = g_mime_content_new_with_stream (GMIME_STREAM (failure), GMIME_CONTENT_ENCODING_BASE64);
				g_mime_part_set_content (GMIME_PART (part), content);
				g_object_unref (content);
				check (!g_mime_text_part_get_text_with_charset (part, "utf-8", &error),
				       "explicit text decoding never returns a prefix after a read failure");
				g_assert_error (error, GMIME_ERROR, (j ? GMIME_ERROR_GENERAL : EIO));
				g_clear_error (&error);
				g_assert_cmpuint (failure->bytes_read, ==, offset);
				g_assert_cmpuint (failure->reset_calls, ==, 1);
				g_assert_cmpuint (failure->close_calls, ==, 0);
				failure->fail_after = G_MAXSIZE;
				failure->fail_reset_on = failure->reset_calls + 1;
				check (!g_mime_text_part_get_text_with_charset (part, "utf-8", &error),
				       "explicit text decoding reports rewind failures");
				g_assert_error (error, GMIME_ERROR, (j ? GMIME_ERROR_GENERAL : ESPIPE));
				g_clear_error (&error);
				failure->bytes_read = 0;
				text = g_mime_text_part_get_text_with_charset (part, "utf-8", &error);
				g_assert_no_error (error);
				check (text && !strcmp (text, "Hello"), "explicit text decoding can retry after an I/O failure");
				g_free (text);

				failure->bytes_read = 0;
				failure->fail_reset_on = failure->reset_calls + 2;
				check (!g_mime_text_part_get_text_with_charset (part, "utf-8", &error),
				       "explicit text decoding propagates the final rewind error after complete input");
				g_assert_error (error, GMIME_ERROR, (j ? GMIME_ERROR_GENERAL : ESPIPE));
				g_clear_error (&error);
				g_object_unref (failure);
			}
		}
	}

	stream = g_mime_stream_mem_new_with_buffer ("ignored", 7);
	custom = g_object_new (test_content_get_type (), NULL);
	g_mime_content_set_stream (GMIME_CONTENT (custom), stream);
	g_object_unref (stream);
	g_mime_content_set_encoding (GMIME_CONTENT (custom), GMIME_CONTENT_ENCODING_BASE64);
	g_mime_part_set_content (GMIME_PART (part), GMIME_CONTENT (custom));
	text = g_mime_text_part_get_text_with_charset (part, "utf-8", &error);
	g_assert_no_error (error);
	check (text && !strcmp (text, "Cat") && custom->decode_calls == 1 && custom->write_calls == 1,
	       "explicit text decoding honors content decode and stream-copy overrides");
	g_free (text);
	custom->fail_decode = TRUE;
	check (!g_mime_text_part_get_text_with_charset (part, "utf-8", &error),
	       "explicit text decoding reports override failures without errno");
	g_assert_error (error, GMIME_ERROR, GMIME_ERROR_GENERAL);
	g_clear_error (&error);
	g_object_unref (custom);
	g_object_unref (part);
}

static void
test_charset_bom_detection (void)
{
	static const struct {
		guint8 bytes[6];
		gsize length;
		const char *charset;
	} cases[] = {
		{ { 0 }, 0, NULL },
		{ { 0xff }, 1, NULL },
		{ { 0xfe }, 1, NULL },
		{ { 0xff, 0xfe }, 2, "utf-16le" },
		{ { 0xfe, 0xff }, 2, "utf-16be" },
		{ { 0xef }, 1, NULL },
		{ { 0xef, 0xbb }, 2, NULL },
		{ { 0xef, 0xbb, 0xbf }, 3, "utf-8" },
		{ { 0xef, 0xbb, 0xbe }, 3, NULL },
		{ { 0xff, 0xfe, 0, 0 }, 4, "utf-16le" },
		{ { 0, 0, 0xfe, 0xff }, 4, NULL },
		{ { 0xff, 0xfe, 0x41, 0, 0xff, 0xfe }, 6, "utf-16le" },
		{ { 0xfe, 0xff, 0, 0x41 }, 4, "utf-16be" },
		{ { 0xef, 0xbb, 0xbf, 0x41 }, 4, "utf-8" },
		{ { 0x41, 0xff, 0xfe }, 3, NULL },
		{ { 0, 0xef, 0xbb, 0xbf }, 4, NULL }
	};
	const char *charset, *expected;
	TestStreamFailure *failure;
	GMimeStream *source;
	GError *error = NULL;
	guint8 bytes[3], payload[7], *exact;
	gsize available;
	gboolean found;
	guint i, first, second, mode;
	int unset;

	charset = "stale";
	check (!g_mime_charset_try_get_bom_encoding (NULL, 0, &charset) && charset == NULL,
	       "empty BOM buffers clear the output without accessing NULL");
	for (i = 0; i < G_N_ELEMENTS (cases); i++) {
		charset = "stale";
		exact = g_memdup2 (cases[i].bytes, cases[i].length);
		found = g_mime_charset_try_get_bom_encoding (exact, cases[i].length, &charset);
		g_free (exact);
		check (found == (cases[i].charset != NULL) && !g_strcmp0 (charset, cases[i].charset),
		       "BOM detection matches initial complete marks without interpreting UTF-32");

		payload[0] = 'x';
		memcpy (payload + 1, cases[i].bytes, cases[i].length);
		for (mode = 0; mode < 4; mode++) {
			source = g_mime_stream_mem_new_with_buffer ((const char *) payload, cases[i].length + 1);
			g_assert_cmpint (g_mime_stream_seek (source, 1, GMIME_STREAM_SEEK_SET), ==, 1);
			failure = g_object_new (test_stream_failure_get_type (), NULL);
			failure->parent_instance.source = g_object_ref (source);
			failure->fail_after = mode == 2 ? 2 : G_MAXSIZE;
			failure->onebyte = mode == 1;
			failure->zero_read = mode == 3;
			failure->nonseekable = TRUE;
			g_mime_stream_construct (GMIME_STREAM (failure), 0, -1);
			available = MIN (cases[i].length, mode == 3 ? 0 : mode == 2 ? 2 : mode == 1 ? 1 : 3);
			g_mime_charset_try_get_bom_encoding (cases[i].bytes, available, &expected);
			charset = "stale";
			errno = ENOENT;
			found = g_mime_charset_try_get_bom_encoding_from_stream (GMIME_STREAM (failure), &charset, &error);
			g_assert_no_error (error);
			check (found == (expected != NULL) && !g_strcmp0 (charset, expected) &&
			       failure->read_calls == 1 && failure->bytes_read == available &&
			       g_mime_stream_tell (source) == (gint64) (1 + available),
			       "stream BOM detection makes one read at the current position and never fills short reads");
			check (failure->reset_calls == 0 && failure->close_calls == 0 &&
			       g_mime_stream_read (source, (char *) bytes, 1) == (available < cases[i].length ? 1 : 0),
			       "stream BOM detection does not rewind, close or consume the remaining content");
			g_object_unref (failure);
			g_object_unref (source);
		}
	}

	for (first = 0; first < 256; first++) {
		for (second = 0; second < 256; second++) {
			bytes[0] = first;
			bytes[1] = second;
			expected = first == 0xff && second == 0xfe ? "utf-16le" :
				first == 0xfe && second == 0xff ? "utf-16be" : NULL;
			found = g_mime_charset_try_get_bom_encoding (bytes, 2, &charset);
			g_assert_cmpint (found, ==, expected != NULL);
			g_assert_cmpstr (charset, ==, expected);
			bytes[2] = 0xbf;
			if (first == 0xef && second == 0xbb)
				expected = "utf-8";
			found = g_mime_charset_try_get_bom_encoding (bytes, 3, &charset);
			g_assert_cmpint (found, ==, expected != NULL);
			g_assert_cmpstr (charset, ==, expected);
		}
	}
	check (TRUE, "every two-byte prefix and every three-byte prefix ending in BF is classified correctly");

	for (unset = 0; unset < 2; unset++) {
		failure = g_object_new (test_stream_failure_get_type (), NULL);
		failure->parent_instance.source = g_mime_stream_mem_new_with_buffer ("\357\273\277tail", 7);
		failure->unset_error = unset;
		g_mime_stream_construct (GMIME_STREAM (failure), 0, -1);
		charset = "stale";
		check (!g_mime_charset_try_get_bom_encoding_from_stream (GMIME_STREAM (failure), &charset, &error) &&
		       charset == NULL && failure->read_calls == 1 && failure->bytes_read == 0 &&
		       failure->reset_calls == 0 && failure->close_calls == 0,
		       "BOM read failures clear the charset without seeking, retrying or closing");
		g_assert_error (error, GMIME_ERROR, (unset ? GMIME_ERROR_GENERAL : EIO));
		g_clear_error (&error);
		check (!g_mime_charset_try_get_bom_encoding_from_stream (GMIME_STREAM (failure), &charset, NULL),
		       "BOM read failures can omit the error location");
		failure->fail_after = G_MAXSIZE;
		check (g_mime_charset_try_get_bom_encoding_from_stream (GMIME_STREAM (failure), &charset, &error) &&
		       !g_strcmp0 (charset, "utf-8"), "BOM reads can be retried after a non-consuming read failure");
		g_assert_no_error (error);
		g_object_unref (failure);
	}

	g_test_expect_message ("gmime", G_LOG_LEVEL_CRITICAL, "*buffer != NULL || length == 0*");
	check (!g_mime_charset_try_get_bom_encoding (NULL, 1, &charset) && charset == NULL,
	       "nonempty NULL BOM buffers fail explicitly and clear the output");
	g_test_assert_expected_messages ();
	g_test_expect_message ("gmime", G_LOG_LEVEL_CRITICAL, "*charset != NULL*");
	g_mime_charset_try_get_bom_encoding (bytes, sizeof (bytes), NULL);
	g_test_assert_expected_messages ();
	g_test_expect_message ("gmime", G_LOG_LEVEL_CRITICAL, "*GMIME_IS_STREAM (stream)*");
	check (!g_mime_charset_try_get_bom_encoding_from_stream (NULL, &charset, &error) &&
	       charset == NULL && error == NULL, "BOM detection rejects invalid streams before I/O");
	g_test_assert_expected_messages ();

	failure = g_object_new (test_stream_failure_get_type (), NULL);
	error = g_error_new_literal (GMIME_ERROR, EIO, "existing error");
	g_test_expect_message ("gmime", G_LOG_LEVEL_CRITICAL, "*error == NULL || *error == NULL*");
	check (!g_mime_charset_try_get_bom_encoding_from_stream (GMIME_STREAM (failure), &charset, &error) &&
	       charset == NULL && failure->read_calls == 0 && !strcmp (error->message, "existing error"),
	       "BOM detection preserves an existing error and performs no read");
	g_test_assert_expected_messages ();
	g_clear_error (&error);
	g_test_expect_message ("gmime", G_LOG_LEVEL_CRITICAL, "*charset != NULL*");
	check (!g_mime_charset_try_get_bom_encoding_from_stream (GMIME_STREAM (failure), NULL, &error) &&
	       failure->read_calls == 0 && error == NULL, "BOM detection requires an output location before I/O");
	g_test_assert_expected_messages ();
	g_object_unref (failure);
}

static void
test_text_part_formats (void)
{
	enum { PLAIN = 1, FLOWED = 2, HTML = 4, ENRICHED = 8, RTF = 16 };
	static const struct {
		const char *type, *subtype, *parameter;
		GMimeTextFormat format;
		guint flags;
	} cases[] = {
		{ "text", "plain", NULL, GMIME_TEXT_FORMAT_PLAIN, PLAIN },
		{ "text", "plain", "flowed", GMIME_TEXT_FORMAT_FLOWED, PLAIN | FLOWED },
		{ "TEXT", "plain", "\t FlOwEd \r\n", GMIME_TEXT_FORMAT_FLOWED, PLAIN | FLOWED },
		{ "text", "PLAIN", "flowed", GMIME_TEXT_FORMAT_FLOWED, PLAIN | FLOWED },
		{ "text", "Plain", "flowed", GMIME_TEXT_FORMAT_FLOWED, PLAIN | FLOWED },
		{ "TeXt", "pLaIn", "\t FlOwEd \r\n", GMIME_TEXT_FORMAT_FLOWED, PLAIN | FLOWED },
		{ "TEXT", "PLAIN", "fixed", GMIME_TEXT_FORMAT_PLAIN, PLAIN },
		{ "text", "plain", "", GMIME_TEXT_FORMAT_PLAIN, PLAIN },
		{ "text", "plain", "fixed", GMIME_TEXT_FORMAT_PLAIN, PLAIN },
		{ "text", "plain", "flowed extra", GMIME_TEXT_FORMAT_PLAIN, PLAIN },
		{ "text", "plain", "flowedx", GMIME_TEXT_FORMAT_PLAIN, PLAIN },
		{ "text", "plain", "xflowed", GMIME_TEXT_FORMAT_PLAIN, PLAIN },
		{ "text", "plain", "flo wed", GMIME_TEXT_FORMAT_PLAIN, PLAIN },
		{ "text", "plain", "\342\200\213flowed", GMIME_TEXT_FORMAT_PLAIN, PLAIN },
		{ "text", "plain", "\357\273\277flowed", GMIME_TEXT_FORMAT_PLAIN, PLAIN },
		{ "text", "plain", "\341\240\216flowed", GMIME_TEXT_FORMAT_PLAIN, PLAIN },
		{ "text", "HTML", "flowed", GMIME_TEXT_FORMAT_HTML, HTML },
		{ "TEXT", "html", NULL, GMIME_TEXT_FORMAT_HTML, HTML },
		{ "text", "enriched", NULL, GMIME_TEXT_FORMAT_ENRICHED, ENRICHED },
		{ "text", "RichText", NULL, GMIME_TEXT_FORMAT_ENRICHED, ENRICHED },
		{ "text", "rtf", NULL, GMIME_TEXT_FORMAT_RICH_TEXT, RTF },
		{ "APPLICATION", "RTF", "flowed", GMIME_TEXT_FORMAT_RICH_TEXT, RTF },
		{ "text", "markdown", "flowed", GMIME_TEXT_FORMAT_PLAIN, 0 },
		{ "application", "plain", "flowed", GMIME_TEXT_FORMAT_PLAIN, 0 },
		{ "application", "html", NULL, GMIME_TEXT_FORMAT_PLAIN, 0 },
		{ "application", "enriched", NULL, GMIME_TEXT_FORMAT_PLAIN, 0 },
		{ "application", "richtext", NULL, GMIME_TEXT_FORMAT_PLAIN, 0 },
		{ "application", "rtf-compressed", NULL, GMIME_TEXT_FORMAT_PLAIN, 0 },
		{ "application", "octet-stream", NULL, GMIME_TEXT_FORMAT_PLAIN, 0 }
	};
	static const gunichar whitespace[] = {
		'\t', '\n', '\v', '\f', '\r', ' ', 0x85, 0xa0, 0x1680,
		0x2000, 0x2001, 0x2002, 0x2003, 0x2004, 0x2005, 0x2006,
		0x2007, 0x2008, 0x2009, 0x200a, 0x2028, 0x2029, 0x202f, 0x205f, 0x3000
	};
	static const char *subtypes[] = { "plain", "plain", "html", "enriched", "rtf", "rtf" };
	GMimeTextPart *part;
	GMimeContentType *content_type;
	GMimeHeaderList *headers;
	GMimeHeader *header;
	GMimeTextFormat format;
	char encoded[7], *parameter, *raw;
	guint i, flags;

	check (GMIME_TEXT_FORMAT_TEXT == GMIME_TEXT_FORMAT_PLAIN &&
	       GMIME_TEXT_FORMAT_PLAIN == 0 && GMIME_TEXT_FORMAT_FLOWED == 1 &&
	       GMIME_TEXT_FORMAT_HTML == 2 && GMIME_TEXT_FORMAT_ENRICHED == 3 &&
	       GMIME_TEXT_FORMAT_COMPRESSED_RICH_TEXT == 4 && GMIME_TEXT_FORMAT_RICH_TEXT == 5,
	       "text-format values and alias match MimeKit");
	for (i = 0; i < G_N_ELEMENTS (cases); i++) {
		part = g_mime_text_part_new ();
		content_type = g_mime_content_type_new (cases[i].type, cases[i].subtype);
		if (cases[i].parameter != NULL)
			g_mime_content_type_set_parameter (content_type, "format", cases[i].parameter);
		g_mime_entity_set_content_type (GMIME_ENTITY (part), content_type);
		headers = g_mime_entity_get_header_list (GMIME_ENTITY (part));
		header = g_mime_header_list_get_header (headers, "Content-Type");
		raw = g_strdup (testsuite_header_raw_text (header));
		flags = (g_mime_text_part_is_plain (part) ? PLAIN : 0) |
			(g_mime_text_part_is_flowed (part) ? FLOWED : 0) |
			(g_mime_text_part_is_html (part) ? HTML : 0) |
			(g_mime_text_part_is_enriched (part) ? ENRICHED : 0) |
			(g_mime_text_part_is_rich_text (part) ? RTF : 0);
		check (g_mime_text_part_get_format (part) == cases[i].format && flags == cases[i].flags,
		       "text-part classification matches the current MIME type and format parameter");
		check (g_mime_entity_get_content_type (GMIME_ENTITY (part)) == content_type &&
		       !g_strcmp0 (raw, testsuite_header_raw_text (header)),
		       "text-part classification does not modify Content-Type or raw headers");
		g_free (raw);
		g_object_unref (content_type);
		g_object_unref (part);
	}

	for (i = 0; i < G_N_ELEMENTS (subtypes); i++) {
		part = g_mime_text_part_new_with_format ((GMimeTextFormat) i);
		content_type = g_mime_entity_get_content_type (GMIME_ENTITY (part));
		format = i == GMIME_TEXT_FORMAT_COMPRESSED_RICH_TEXT ? GMIME_TEXT_FORMAT_RICH_TEXT : (GMimeTextFormat) i;
		check (g_mime_content_type_is_type (content_type, "text", subtypes[i]) &&
		       g_mime_text_part_get_format (part) == format &&
		       !g_strcmp0 (g_mime_content_type_get_parameter (content_type, "format"),
				  i == GMIME_TEXT_FORMAT_FLOWED ? "flowed" : NULL),
		       "text-format constructor assigns the expected subtype and flowed parameter");
		check (g_mime_part_get_content (GMIME_PART (part)) == NULL,
		       "text-format constructor does not invent or compress content");
		g_object_unref (part);
	}
	g_test_expect_message ("gmime", G_LOG_LEVEL_CRITICAL, "*should not be reached*");
	check (g_mime_text_part_new_with_format ((GMimeTextFormat) -1) == NULL,
	       "text-format constructor rejects negative enum values");
	g_test_assert_expected_messages ();
	g_test_expect_message ("gmime", G_LOG_LEVEL_CRITICAL, "*should not be reached*");
	check (g_mime_text_part_new_with_format ((GMimeTextFormat) 6) == NULL,
	       "text-format constructor rejects unknown enum values");
	g_test_assert_expected_messages ();

	part = g_mime_text_part_new ();
	for (i = 0; i < G_N_ELEMENTS (whitespace); i++) {
		gboolean linear = whitespace[i] == ' ' || whitespace[i] == '\t' ||
			whitespace[i] == '\r' || whitespace[i] == '\n';

		encoded[g_unichar_to_utf8 (whitespace[i], encoded)] = '\0';
		parameter = g_strconcat (encoded, "FlOwEd", encoded, NULL);
		g_mime_entity_set_content_type_parameter (GMIME_ENTITY (part), "format", parameter);
		check (g_mime_text_part_is_flowed (part) == linear &&
		       g_mime_text_part_get_format (part) == (linear ? GMIME_TEXT_FORMAT_FLOWED : GMIME_TEXT_FORMAT_PLAIN),
		       "flowed text accepts only surrounding ASCII linear whitespace");
		g_free (parameter);
	}
	g_mime_entity_set_header (GMIME_ENTITY (part), "Content-Type", "text/html; format=flowed", NULL);
	check (g_mime_text_part_is_html (part) && !g_mime_text_part_is_flowed (part) &&
	       g_mime_text_part_get_format (part) == GMIME_TEXT_FORMAT_HTML,
	       "raw Content-Type mutations immediately update text classification");
	g_mime_entity_set_header (GMIME_ENTITY (part), "Content-Type", "TeXt/pLaIn; format=FlOwEd", NULL);
	check (g_mime_text_part_is_flowed (part) && g_mime_text_part_get_format (part) == GMIME_TEXT_FORMAT_FLOWED,
	       "raw format changes are not cached and ignore MIME type and parameter case");
	content_type = g_mime_entity_get_content_type (GMIME_ENTITY (part));
	g_mime_param_list_remove (g_mime_content_type_get_parameters (content_type), "format");
	check (g_mime_text_part_is_plain (part) && !g_mime_text_part_is_flowed (part) &&
	       g_mime_text_part_get_format (part) == GMIME_TEXT_FORMAT_PLAIN,
	       "removing format restores non-flowed plain text");
	g_object_unref (part);
}

static void
check_multipart_text_value (GMimeMultipart *multipart, GMimeTextFormat format, GMimeTextPart *expected, const char *description)
{
	GMimeTextPart *sentinel = g_mime_text_part_new ();
	GMimeTextPart *body = sentinel;
	gboolean found = g_mime_multipart_try_get_value (multipart, format, &body);

	check (found == (expected != NULL) && body == expected, description);
	g_object_unref (sentinel);
}

static void
test_multipart_text_selection (void)
{
	static const char wire[] = "Content-Type: multipart/ALTERNATIVE; boundary=x\n\n"
		"--x\nContent-Type: text/plain\n\nfirst\n"
		"--x\nContent-Type: text/plain\nContent-Disposition: attachment\n\nlast\n--x--\n";
	GMimeMultipart *mixed = g_mime_multipart_new ();
	GMimeMultipart *alternative = GMIME_MULTIPART (g_mime_multipart_alternative_new ());
	GMimeMultipart *generic = g_mime_multipart_new_with_subtype ("alternative");
	GMimeMultipart *nested = g_mime_multipart_new ();
	GMimeMultipart *signed_part = GMIME_MULTIPART (g_mime_multipart_signed_new ());
	GMimeTextPart *plain = g_mime_text_part_new ();
	GMimeTextPart *last = g_mime_text_part_new ();
	GMimeTextPart *html = g_mime_text_part_new_with_format (GMIME_TEXT_FORMAT_HTML);
	GMimeTextPart *flowed = g_mime_text_part_new_with_format (GMIME_TEXT_FORMAT_FLOWED);
	GMimeTextPart *rtf = g_mime_text_part_new_with_format (GMIME_TEXT_FORMAT_RICH_TEXT);
	GMimeTextPart *enriched = g_mime_text_part_new_with_subtype ("richtext");
	GMimePart *blob = g_mime_part_new ();
	GMimePart *text_like = g_mime_part_new_with_type ("text", "plain");
	GMimeMessage *message = g_mime_message_new (FALSE);
	GMimeMessagePart *attached = g_mime_message_part_new ("rfc822");
	GMimeTextPart *body, *weak;
	GMimeEntity *entity, *removed;
	GMimeParser *parser;
	GMimeStream *stream;
	GError *error = NULL;
	GMimeHeader *header;
	char *raw;
	int i;

	check (GMIME_IS_MULTIPART_ALTERNATIVE (alternative) &&
	       !strcmp (G_OBJECT_TYPE_NAME (alternative), "GMimeMultipartAlternative") &&
	       g_mime_content_type_is_type (g_mime_entity_get_content_type (GMIME_ENTITY (alternative)), "multipart", "alternative"),
	       "specialized alternative constructor exposes the expected type and Content-Type");
	for (i = -1; i <= 6; i++) {
		check_multipart_text_value (mixed, (GMimeTextFormat) i, NULL, "empty multipart has no text body");
		check_multipart_text_value (alternative, (GMimeTextFormat) i, NULL, "empty alternative has no text body");
	}

	g_mime_message_set_mime_part (message, GMIME_ENTITY (plain));
	g_mime_message_part_set_message (attached, message);
	g_mime_entity_set_is_attachment (GMIME_ENTITY (last), TRUE);
	g_mime_multipart_add (mixed, GMIME_ENTITY (blob));
	g_mime_multipart_add (mixed, GMIME_ENTITY (text_like));
	g_mime_multipart_add (mixed, GMIME_ENTITY (attached));
	g_mime_multipart_add (mixed, GMIME_ENTITY (last));
	g_mime_multipart_add (mixed, GMIME_ENTITY (html));
	g_mime_multipart_add (mixed, GMIME_ENTITY (plain));
	check_multipart_text_value (mixed, GMIME_TEXT_FORMAT_PLAIN, NULL,
				   "base selection skips non-text, embedded messages and attachments but stops at wrong-format text");
	check_multipart_text_value (mixed, GMIME_TEXT_FORMAT_HTML, html, "base selection returns the first eligible text part");
	g_mime_multipart_clear (mixed);
	g_mime_multipart_add (mixed, GMIME_ENTITY (last));
	g_mime_multipart_add (mixed, GMIME_ENTITY (flowed));
	g_mime_multipart_add (mixed, GMIME_ENTITY (plain));
	check_multipart_text_value (mixed, GMIME_TEXT_FORMAT_TEXT, flowed, "plain and Text formats also match flowed text");
	check_multipart_text_value (mixed, GMIME_TEXT_FORMAT_FLOWED, flowed, "flowed format matches flowed text");
	check_multipart_text_value (mixed, GMIME_TEXT_FORMAT_COMPRESSED_RICH_TEXT, NULL, "compressed rich text has no format predicate");
	g_mime_multipart_clear (mixed);
	g_mime_multipart_add (mixed, GMIME_ENTITY (plain));
	g_mime_multipart_add (mixed, GMIME_ENTITY (flowed));
	check_multipart_text_value (mixed, GMIME_TEXT_FORMAT_FLOWED, NULL, "non-flowed plain text blocks later flowed text");

	g_mime_multipart_add (generic, GMIME_ENTITY (plain));
	g_mime_multipart_add (generic, GMIME_ENTITY (html));
	g_mime_multipart_add (generic, GMIME_ENTITY (last));
	check_multipart_text_value (generic, GMIME_TEXT_FORMAT_PLAIN, plain, "generic alternative subtype retains base selection");
	check_multipart_text_value (generic, GMIME_TEXT_FORMAT_HTML, NULL, "generic alternative subtype does not enable reverse selection");
	g_mime_multipart_add (alternative, GMIME_ENTITY (plain));
	g_mime_multipart_add (alternative, GMIME_ENTITY (html));
	g_mime_multipart_add (alternative, GMIME_ENTITY (last));
	check_multipart_text_value (alternative, GMIME_TEXT_FORMAT_PLAIN, last, "specialized alternative selects the last matching text even when attached");
	check_multipart_text_value (alternative, GMIME_TEXT_FORMAT_HTML, html, "specialized alternative continues past wrong-format text");
	g_mime_content_type_set_media_subtype (g_mime_entity_get_content_type (GMIME_ENTITY (alternative)), "mixed");
	check_multipart_text_value (alternative, GMIME_TEXT_FORMAT_PLAIN, last, "selection follows the object's class after subtype mutation");
	g_mime_multipart_add (alternative, GMIME_ENTITY (enriched));
	g_mime_multipart_add (alternative, GMIME_ENTITY (rtf));
	check_multipart_text_value (alternative, GMIME_TEXT_FORMAT_ENRICHED, enriched, "alternative selection recognizes enriched aliases");
	check_multipart_text_value (alternative, GMIME_TEXT_FORMAT_RICH_TEXT, rtf, "alternative selection recognizes RTF");
	check_multipart_text_value (alternative, GMIME_TEXT_FORMAT_COMPRESSED_RICH_TEXT, NULL, "compressed format does not match uncompressed RTF");
	check_multipart_text_value (alternative, (GMimeTextFormat) -1, NULL, "unknown formats clear the output and do not match");

	g_mime_multipart_clear (mixed);
	g_mime_multipart_add (mixed, GMIME_ENTITY (nested));
	g_mime_multipart_add (mixed, GMIME_ENTITY (plain));
	check_multipart_text_value (mixed, GMIME_TEXT_FORMAT_PLAIN, NULL, "unsuccessful nested multipart blocks later base siblings");
	g_mime_multipart_add (nested, GMIME_ENTITY (html));
	check_multipart_text_value (mixed, GMIME_TEXT_FORMAT_HTML, html, "base selection delegates to the first nested multipart");
	g_mime_entity_set_is_attachment (GMIME_ENTITY (nested), TRUE);
	check_multipart_text_value (mixed, GMIME_TEXT_FORMAT_HTML, html, "attached multipart containers remain searchable");
	g_mime_multipart_add (alternative, GMIME_ENTITY (mixed));
	check_multipart_text_value (alternative, GMIME_TEXT_FORMAT_PLAIN, last, "alternative selection continues after nested selection fails");
	check_multipart_text_value (alternative, GMIME_TEXT_FORMAT_HTML, html, "alternative selection returns the original nested body");
	removed = g_mime_multipart_remove_at (alternative, g_mime_multipart_get_count (alternative) - 1);
	g_object_unref (removed);
	g_mime_multipart_clear (mixed);
	g_mime_multipart_add (mixed, GMIME_ENTITY (alternative));
	check_multipart_text_value (mixed, GMIME_TEXT_FORMAT_PLAIN, last, "base selection respects a specialized nested alternative override");
	g_mime_multipart_clear (mixed);
	g_mime_multipart_add (signed_part, GMIME_ENTITY (blob));
	g_mime_multipart_add (signed_part, GMIME_ENTITY (html));
	check_multipart_text_value (signed_part, GMIME_TEXT_FORMAT_HTML, html, "signed multipart inherits base selection rather than legacy body-guessing rules");

	stream = g_mime_stream_mem_new_with_buffer (wire, sizeof (wire) - 1);
	parser = g_mime_parser_new_with_stream (stream);
	entity = g_mime_parser_construct_part (parser, NULL, &error);
	g_assert_no_error (error);
	g_assert_true (GMIME_IS_MULTIPART_ALTERNATIVE (entity));
	header = g_mime_header_list_get_header (g_mime_entity_get_header_list (entity), "Content-Type");
	raw = g_strdup (testsuite_header_raw_text (header));
	body = GMIME_TEXT_PART (g_mime_multipart_get_part (GMIME_MULTIPART (entity), 1));
	check_multipart_text_value (GMIME_MULTIPART (entity), GMIME_TEXT_FORMAT_PLAIN, body,
				   "parser registers specialized alternatives case-insensitively");
	check (!g_strcmp0 (raw, testsuite_header_raw_text (header)), "selection preserves parsed raw headers");
	g_free (raw);
	weak = body;
	g_object_add_weak_pointer (G_OBJECT (body), (gpointer *) &weak);
	for (i = 0; i < 100; i++)
		check_multipart_text_value (GMIME_MULTIPART (entity), GMIME_TEXT_FORMAT_PLAIN, body, "repeated lookup preserves original identity");
	g_object_ref (body);
	g_object_unref (entity);
	g_object_unref (parser);
	g_object_unref (stream);
	check (weak == body && g_mime_text_part_is_plain (body), "retained selected body outlives its multipart");
	g_object_unref (body);
	check (weak == NULL, "borrowed selection does not accumulate hidden references");

	g_object_unref (signed_part);
	g_object_unref (nested);
	g_object_unref (generic);
	g_object_unref (alternative);
	g_object_unref (mixed);
	g_object_unref (attached);
	g_object_unref (message);
	g_object_unref (text_like);
	g_object_unref (blob);
	g_object_unref (enriched);
	g_object_unref (rtf);
	g_object_unref (flowed);
	g_object_unref (html);
	g_object_unref (last);
	g_object_unref (plain);
}

static void
test_multipart_related (void)
{
	static const struct {
		const char *start;
		const char *type;
		int index;
	} cases[] = {
		{ NULL, NULL, 0 }, { NULL, "TeXt/HtMl", 1 }, { NULL, "text/plain", 2 },
		{ NULL, "text/html; charset=utf-8", 0 }, { NULL, " text/html ", 0 },
		{ NULL, "image/missing", 0 }, { "", "text/html", 0 },
		{ "<root@host>", NULL, 1 }, { "root@host", NULL, 1 },
		{ "(comment) <root@host> <first@host>", NULL, 1 },
		{ "<missing@host> <root@host>", "text/html", 0 },
		{ "<ROOT@host>", "text/html", 0 }, { "<root@HOST>", NULL, 0 },
		{ "<missing@host>", "text/html", 0 }, { "<broken", "text/html", 0 },
		{ "r%6Fot@host", NULL, 1 }, { "root@host?query", NULL, 1 },
		{ "root@host#fragment", NULL, 1 }, { "root@host ", NULL, 1 },
		{ "//host/root@host", NULL, 0 }, { "root@host\v", NULL, 0 },
		{ "root@host\f", NULL, 0 },
		{ "<root@host>", "text/plain", 1 }
	};
	static const char wire[] = "Content-Type: multipart/RELATED; boundary=x; start=\"<root@host>\"; type=\"image/png\"\n\n"
		"--x\nContent-Type: image/png\nContent-Id: <image@host>\n\nimage\n"
		"--x\nContent-Type: text/html\nContent-Id: <root@host>\n\n<p>root</p>\n--x--\n";
	GMimeMultipartRelated *related = g_mime_multipart_related_new ();
	GMimeMultipart *multipart = GMIME_MULTIPART (related);
	GMimeMultipart *alternative = GMIME_MULTIPART (g_mime_multipart_alternative_new ());
	GMimeMultipart *generic = g_mime_multipart_new_with_subtype ("related");
	GMimePart *image = g_mime_part_new_with_type ("image", "png");
	GMimeTextPart *html = g_mime_text_part_new_with_format (GMIME_TEXT_FORMAT_HTML);
	GMimeTextPart *plain = g_mime_text_part_new ();
	GMimeTextPart *replacement = g_mime_text_part_new_with_format (GMIME_TEXT_FORMAT_HTML);
	GMimeEntity *entity, *parsed, *weak;
	GMimeContentType *content_type;
	GMimeParamList *parameters;
	GMimeParser *parser;
	GMimeStream *stream;
	GMimeHeader *header;
	GError *error = NULL;
	char *raw, *start, *serialized;
	const char *generated;
	guint i;

	check (!strcmp (G_OBJECT_TYPE_NAME (related), "GMimeMultipartRelated") &&
	       g_mime_content_type_is_type (g_mime_entity_get_content_type (GMIME_ENTITY (related)), "multipart", "related") &&
	       g_mime_multipart_related_get_root (related) == NULL,
	       "related constructor has the expected GType and Content-Type and an empty root");
	check_multipart_text_value (multipart, GMIME_TEXT_FORMAT_HTML, NULL, "an empty related has no text body");
	g_mime_entity_set_content_id (GMIME_ENTITY (image), "first@host");
	g_mime_entity_set_content_id (GMIME_ENTITY (html), "root@host");
	g_mime_entity_set_content_id (GMIME_ENTITY (plain), "plain@host");
	g_mime_multipart_add (multipart, GMIME_ENTITY (image));
	g_mime_multipart_add (multipart, GMIME_ENTITY (html));
	g_mime_multipart_add (multipart, GMIME_ENTITY (plain));
	content_type = g_mime_entity_get_content_type (GMIME_ENTITY (related));
	parameters = g_mime_content_type_get_parameters (content_type);
	for (i = 0; i < G_N_ELEMENTS (cases); i++) {
		g_mime_param_list_remove (parameters, "start");
		g_mime_param_list_remove (parameters, "type");
		if (cases[i].start)
			g_mime_content_type_set_parameter (content_type, "start", cases[i].start);
		if (cases[i].type)
			g_mime_content_type_set_parameter (content_type, "type", cases[i].type);
		header = g_mime_header_list_get_header (g_mime_entity_get_header_list (GMIME_ENTITY (related)), "Content-Type");
		raw = g_strdup (testsuite_header_raw_text (header));
		check (g_mime_multipart_related_get_root (related) == g_mime_multipart_get_part (multipart, cases[i].index),
		       "related root selection follows start precedence, first references, exact IDs, type fallback and default first child");
		check_multipart_text_value (multipart, GMIME_TEXT_FORMAT_HTML, cases[i].index == 1 ? html : NULL,
					   "related text-body lookup examines only its root, not later sibling resources");
		check (!g_strcmp0 (raw, testsuite_header_raw_text (header)), "root and text selection preserve raw Content-Type headers");
		g_free (raw);
	}
	g_mime_entity_set_content_id (GMIME_ENTITY (html), "/root@host");
	g_mime_content_type_set_parameter (content_type, "start", "//host/a/../root@host");
	check (g_mime_multipart_related_get_root (related) == GMIME_ENTITY (html),
	       "CID authority paths remove dot segments before root matching");
	g_mime_content_type_set_parameter (content_type, "start", "//host/%2e%2e/root@host");
	check (g_mime_multipart_related_get_root (related) == GMIME_ENTITY (html),
	       "CID authority paths decode escaped dot segments before root matching");
	g_mime_content_type_set_parameter (content_type, "start", "/a/../root@host");
	check (g_mime_multipart_related_get_root (related) == GMIME_ENTITY (image),
	       "opaque CID paths preserve dot segments");
	g_mime_entity_set_content_id (GMIME_ENTITY (html), "root@host");
	g_mime_content_type_set_parameter (content_type, "start", "<root@host>");
	g_mime_entity_set_is_attachment (GMIME_ENTITY (html), TRUE);
	check_multipart_text_value (multipart, GMIME_TEXT_FORMAT_HTML, html, "an attached root text part remains eligible");
	g_mime_multipart_add (alternative, GMIME_ENTITY (plain));
	g_mime_multipart_add (alternative, GMIME_ENTITY (html));
	g_mime_entity_set_content_id (GMIME_ENTITY (alternative), "nested@host");
	g_mime_multipart_add (multipart, GMIME_ENTITY (alternative));
	g_mime_content_type_set_parameter (content_type, "start", "<nested@host>");
	check_multipart_text_value (multipart, GMIME_TEXT_FORMAT_HTML, html, "related body selection delegates to a multipart root");
	check_multipart_text_value (multipart, GMIME_TEXT_FORMAT_PLAIN, plain, "nested alternatives retain their own selection rules");
	g_mime_content_type_set_parameter (content_type, "start", "<plain@host>");
	check (g_mime_multipart_related_get_root (related) == GMIME_ENTITY (plain),
	       "root lookup does not recurse into earlier multipart descendants");
	g_mime_content_type_set_media_subtype (content_type, "mixed");
	g_mime_content_type_set_parameter (content_type, "start", "<first@host>");
	check_multipart_text_value (multipart, GMIME_TEXT_FORMAT_HTML, NULL, "related selection follows the class after subtype changes");
	g_mime_multipart_add (generic, GMIME_ENTITY (image));
	g_mime_multipart_add (generic, GMIME_ENTITY (html));
	check_multipart_text_value (generic, GMIME_TEXT_FORMAT_HTML, NULL, "generic multipart still skips attached text");
	g_mime_entity_set_is_attachment (GMIME_ENTITY (html), FALSE);
	check_multipart_text_value (generic, GMIME_TEXT_FORMAT_HTML, html, "a generic related subtype retains base multipart behavior");

	g_mime_multipart_clear (multipart);
	g_mime_param_list_clear (parameters);
	g_mime_multipart_related_set_root (related, GMIME_ENTITY (replacement));
	check (g_mime_multipart_get_count (multipart) == 1 &&
	       g_mime_multipart_related_get_root (related) == GMIME_ENTITY (replacement) &&
	       !g_strcmp0 (g_mime_content_type_get_parameter (content_type, "type"), "text/html") &&
	       g_mime_content_type_get_parameter (content_type, "start") == NULL &&
	       g_mime_entity_get_content_id (GMIME_ENTITY (replacement)) == NULL,
	       "setting an empty root adds it first and sets type without generating an ID or start");
	g_mime_multipart_related_set_root (related, GMIME_ENTITY (replacement));
	check (g_mime_multipart_get_count (multipart) == 1, "assigning the selected root to itself safely replaces its existing slot");
	g_mime_multipart_clear (multipart);
	g_mime_param_list_clear (parameters);
	g_mime_multipart_add (multipart, GMIME_ENTITY (image));
	g_mime_multipart_add (multipart, GMIME_ENTITY (html));
	g_mime_multipart_related_set_root (related, GMIME_ENTITY (replacement));
	check (g_mime_multipart_get_count (multipart) == 3 &&
	       g_mime_multipart_get_part (multipart, 0) == GMIME_ENTITY (replacement) &&
	       g_mime_multipart_get_part (multipart, 1) == GMIME_ENTITY (image),
	       "an implicit first-child root is not replaced: an unmatched selector inserts the new root first");
	g_mime_content_type_set_parameter (content_type, "start", "<root@host>");
	g_mime_multipart_related_set_root (related, GMIME_ENTITY (replacement));
	generated = g_mime_entity_get_content_id (GMIME_ENTITY (replacement));
	start = g_strdup_printf ("<%s>", generated);
	check (generated && *generated && g_mime_multipart_get_count (multipart) == 3 &&
	       g_mime_multipart_get_part (multipart, 2) == GMIME_ENTITY (replacement) &&
	       !g_strcmp0 (start, g_mime_content_type_get_parameter (content_type, "start")),
	       "setting an explicit later root replaces that slot, generates an ID and updates start without deduplicating existing occurrences");
	g_free (start);
	g_mime_content_type_set_parameter (content_type, "start", "<missing@host>");
	g_mime_multipart_related_set_root (related, GMIME_ENTITY (plain));
	check (g_mime_multipart_get_count (multipart) == 4 &&
	       g_mime_multipart_get_part (multipart, 0) == GMIME_ENTITY (plain) &&
	       g_mime_content_type_get_parameter (content_type, "start") == NULL &&
	       !g_strcmp0 (g_mime_content_type_get_parameter (content_type, "type"), "text/plain"),
	       "an unmatched explicit start inserts first and removes start even when type matches an existing part");
	g_test_expect_message ("gmime", G_LOG_LEVEL_CRITICAL, "*GMIME_IS_ENTITY (root)*");
	g_mime_multipart_related_set_root (related, NULL);
	g_test_assert_expected_messages ();
	check (g_mime_multipart_get_count (multipart) == 4, "a null root is a programming error and leaves children unchanged");

	stream = g_mime_stream_mem_new_with_buffer (wire, sizeof (wire) - 1);
	parser = g_mime_parser_new_with_stream (stream);
	parsed = g_mime_parser_construct_part (parser, NULL, &error);
	g_assert_no_error (error);
	g_assert_true (GMIME_IS_MULTIPART_RELATED (parsed));
	entity = g_mime_multipart_get_part (GMIME_MULTIPART (parsed), 1);
	check (g_mime_multipart_related_get_root (GMIME_MULTIPART_RELATED (parsed)) == entity,
	       "parser registers specialized related containers case-insensitively and respects start over type");
	check_multipart_text_value (GMIME_MULTIPART (parsed), GMIME_TEXT_FORMAT_HTML, GMIME_TEXT_PART (entity),
				   "parsed related text selection returns the original root text object");
	serialized = g_mime_entity_to_string (parsed, NULL);
	check (serialized && strstr (serialized, "multipart/RELATED") && strstr (serialized, "<root@host>") &&
	       strstr (serialized, "<p>root</p>"), "related specialization preserves serialization and content");
	g_free (serialized);
	weak = entity;
	g_object_add_weak_pointer (G_OBJECT (entity), (gpointer *) &weak);
	for (i = 0; i < 100; i++)
		check (g_mime_multipart_related_get_root (GMIME_MULTIPART_RELATED (parsed)) == entity,
		       "root lookup preserves identity without retaining hidden references");
	g_object_ref (entity);
	g_object_unref (parsed);
	check (weak == entity, "a retained root outlives its parsed parent");
	g_object_unref (entity);
	check (weak == NULL, "root ownership is released when the caller and multipart release their references");
	g_object_unref (parser);
	g_object_unref (stream);

	entity = g_mime_entity_new_type (NULL, "MuLtIpArT", "ReLaTeD");
	check (GMIME_IS_MULTIPART_RELATED (entity), "the MIME factory registers related containers case-insensitively");
	g_object_unref (entity);
	g_object_unref (generic);
	g_object_unref (alternative);
	g_object_unref (related);
	g_object_unref (replacement);
	g_object_unref (plain);
	g_object_unref (html);
	g_object_unref (image);
}

static void
test_multipart_related_resources (void)
{
	static const struct {
		const char *location, *child_base, *parent_base, *query;
		gboolean match;
	} cases[] = {
		{ NULL, NULL, NULL, "cid:root@host", TRUE },
		{ NULL, NULL, NULL, "CiD:r%6Fot@host?q=x#fragment", TRUE },
		{ NULL, NULL, NULL, "cid:ROOT@host", FALSE },
		{ NULL, NULL, NULL, "cid:root%40host", FALSE },
		{ "cid:missing@host", NULL, NULL, "cid:missing@host", FALSE },
		{ "image.png", NULL, NULL, "image.png", TRUE },
		{ "image.png", NULL, NULL, "IMAGE.png", FALSE },
		{ "a%41.png", NULL, NULL, "aA.png", FALSE },
		{ "", NULL, NULL, "", TRUE },
		{ "image.png", NULL, "https://host/base/", "image.png", TRUE },
		{ "image.png", NULL, NULL, "https://host/base/image.png", FALSE },
		{ "image.png", NULL, "https://host/base/", "https://host/base/image.png", TRUE },
		{ "image.png", "https://child/base/", "https://host/base/", "https://child/base/image.png", TRUE },
		{ "image.png", "https://child/base/", "https://host/base/", "https://host/base/image.png", FALSE },
		{ "../image.png", NULL, "https://host/base/", "https://host/image.png", TRUE },
		{ "/image.png", NULL, "https://host/base/", "https://host/image.png", TRUE },
		{ "//other/image.png", NULL, "https://host/base/", "https://other/image.png", TRUE },
		{ "https://host/image.png", "https://child/base/", NULL, "https://host/image.png", TRUE },
		{ "https://host/image.png", NULL, NULL, "image.png", FALSE },
		{ "http://host", NULL, NULL, "HTTP://HOST:80/", TRUE },
		{ "https://host/a/../image.png", NULL, NULL, "https://host/image.png", TRUE },
		{ "https://host/image.png#one", NULL, NULL, "https://host/image.png#two", TRUE },
		{ "https://user@host/image.png", NULL, NULL, "https://other@host/image.png", TRUE },
		{ "https://host/image.png", NULL, NULL, "https://host/IMAGE.png", FALSE },
		{ "https://host/image.png?q=x", NULL, NULL, "https://host/image.png?q=X", FALSE },
		{ "https://host/image.png?q=x", NULL, NULL, "https://host/image.png?q=x#ignored", TRUE },
		{ "https://host/a%2fb", NULL, NULL, "https://host/a%2Fb", TRUE },
		{ "https://host/a%2fb", NULL, NULL, "https://host/a/b", FALSE },
		{ "https://host/%7e.png", NULL, NULL, "https://host/~.png", TRUE },
		{ "https://host/\303\251.png", NULL, NULL, "https://host/%C3%A9.png", TRUE },
		{ "urn:a/../image", NULL, NULL, "urn:/image", TRUE },
		{ "?q=x", NULL, "https://host/base/index.html", "https://host/base/index.html?q=x", TRUE }
	};
	static const struct {
		GMimeContentEncoding encoding;
		const char *encoded, *decoded;
		gsize encoded_length, decoded_length;
	} encodings[] = {
		{ GMIME_CONTENT_ENCODING_UNSPECIFIED, "a\0\377", "a\0\377", 3, 3 },
		{ GMIME_CONTENT_ENCODING_7BIT, "a\r\n", "a\r\n", 3, 3 },
		{ GMIME_CONTENT_ENCODING_8BIT, "a\377b", "a\377b", 3, 3 },
		{ GMIME_CONTENT_ENCODING_BINARY, "a\0\377", "a\0\377", 3, 3 },
		{ GMIME_CONTENT_ENCODING_BASE64, "AP9B", "\0\377A", 4, 3 },
		{ GMIME_CONTENT_ENCODING_QUOTEDPRINTABLE, "a=00=FF", "a\0\377", 7, 3 },
		{ GMIME_CONTENT_ENCODING_UUENCODE, "begin 644 file\n#0V%T\n`\nend\n", "Cat", 27, 3 }
	};
	GMimeMultipartRelated *related = g_mime_multipart_related_new ();
	GMimePart *part = g_mime_part_new_with_type ("image", "png");
	GMimeMultipart *nested = g_mime_multipart_new ();
	GMimePart *descendant = g_mime_part_new ();
	GMimeEntity *entity = GMIME_ENTITY (part);
	GMimeContent *content;
	GMimeStream *source, *reader, *output, *weak;
	GMimeHeader *header;
	TestStreamFailure *failure;
	GByteArray *bytes;
	GError *error = NULL;
	char *mime_type, *charset, *raw;
	char buffer[16];
	guint i;
	int index;

	g_mime_entity_set_content_id (entity, "root@host");
	g_mime_multipart_add (GMIME_MULTIPART (related), entity);
	for (i = 0; i < G_N_ELEMENTS (cases); i++) {
		g_mime_entity_set_content_location (entity, cases[i].location);
		g_mime_entity_set_content_base (entity, cases[i].child_base);
		g_mime_entity_set_content_base (GMIME_ENTITY (related), cases[i].parent_base);
		raw = g_mime_entity_get_headers (entity, NULL);
		index = g_mime_multipart_related_index_of_uri (related, cases[i].query, &error);
		g_assert_no_error (error);
		check (index == (cases[i].match ? 0 : -1), "related URI lookup follows CID, location, base precedence and URI comparison rules");
		check (g_mime_multipart_related_contains_uri (related, cases[i].query, &error) == cases[i].match,
		       "contains_uri agrees with index_of_uri");
		g_assert_no_error (error);
		mime_type = g_mime_entity_get_headers (entity, NULL);
		check (!strcmp (raw, mime_type), "URI lookup does not change raw resource headers");
		g_free (raw);
		g_free (mime_type);
	}
	g_mime_entity_set_header (entity, "Content-Base", "invalid base", NULL);
	g_mime_entity_set_content_location (entity, "image.png");
	check (g_mime_multipart_related_index_of_uri (related, "https://host/base/image.png", &error) == 0,
	       "a malformed child base permits the valid container base fallback");
	g_assert_no_error (error);
	g_mime_entity_set_content_base (GMIME_ENTITY (related), NULL);
	g_mime_entity_set_content_location (GMIME_ENTITY (related), "https://host/base/");
	check (g_mime_multipart_related_index_of_uri (related, "https://host/base/image.png", &error) == -1,
	       "the container's Content-Location is not a resolution base");
	g_assert_no_error (error);
	g_mime_entity_set_header (entity, "Content-Location", "https://host/base/image.png", NULL);
	check (g_mime_multipart_related_index_of_uri (related, "https://host/base/image.png", &error) == 0,
	       "lookup observes direct resource header changes");
	g_assert_no_error (error);
	g_mime_entity_set_header (entity, "Content-Location", "bad space", NULL);
	g_mime_entity_append_header (entity, "Content-Location", "https://host/base/image.png", NULL);
	check (g_mime_multipart_related_index_of_uri (related, "https://host/base/image.png", &error) == -1,
	       "a malformed first location does not fall through to a valid duplicate");
	g_assert_no_error (error);
	g_mime_entity_set_content_id (entity, "a%2fb@host");
	check (g_mime_multipart_related_index_of_uri (related, "CID:a%2fb@host#ignored", &error) == 0 &&
	       g_mime_multipart_related_index_of_uri (related, "cid:a%2Fb@host", &error) == -1,
	       "CID paths preserve the spelling of reserved percent escapes");
	g_assert_no_error (error);
	g_mime_entity_set_content_id (entity, "root@host");
	g_mime_entity_set_content_id (GMIME_ENTITY (descendant), "nested@host");
	g_mime_multipart_add (nested, GMIME_ENTITY (descendant));
	g_mime_multipart_add (GMIME_MULTIPART (related), GMIME_ENTITY (nested));
	check (g_mime_multipart_related_index_of_uri (related, "cid:nested@host", &error) == -1,
	       "related resource lookup never traverses descendants");
	g_assert_no_error (error);
	g_mime_entity_set_content_id (GMIME_ENTITY (nested), "root@host");
	check (g_mime_multipart_related_index_of_uri (related, "cid:root@host", &error) == 0,
	       "duplicate URI matches select the first immediate child");
	g_assert_no_error (error);
	g_mime_entity_set_content_id (GMIME_ENTITY (nested), "container@host");
	reader = g_mime_multipart_related_open (related, "cid:container@host", &mime_type, &charset, &error);
	check (reader == NULL && mime_type == NULL && charset == NULL, "non-leaf matches cannot be opened as resources");
	g_assert_error (error, GMIME_ERROR, ENOENT);
	g_clear_error (&error);
	reader = g_mime_multipart_related_open (related, "cid:missing@host", &mime_type, &charset, &error);
	g_assert_error (error, GMIME_ERROR, ENOENT);
	g_clear_error (&error);
	reader = g_mime_multipart_related_open (related, "cid:root@host", &mime_type, &charset, &error);
	g_assert_error (error, GMIME_ERROR, ENOENT);
	g_clear_error (&error);
	check (reader == NULL && mime_type == NULL && charset == NULL, "missing resources and content report ENOENT with cleared outputs");
	check (g_mime_multipart_related_index_of_uri (related, "bad%escape", &error) == -1, "malformed URI queries return an explicit parse error");
	g_assert_error (error, GMIME_ERROR, GMIME_ERROR_PARSE_ERROR);
	g_clear_error (&error);
	check (!g_mime_multipart_related_contains_uri (related, "bad space", &error), "contains_uri propagates invalid URI errors");
	g_assert_error (error, GMIME_ERROR, GMIME_ERROR_PARSE_ERROR);
	g_clear_error (&error);
	reader = g_mime_multipart_related_open (related, "bad space", &mime_type, &charset, &error);
	g_assert_error (error, GMIME_ERROR, GMIME_ERROR_PARSE_ERROR);
	g_clear_error (&error);
	check (!reader && !mime_type && !charset, "open propagates lookup errors rather than reporting a missing resource");

	content = g_mime_content_new ();
	g_mime_part_set_content (part, content);
	g_object_unref (content);
	reader = g_mime_multipart_related_open (related, "cid:root@host", &mime_type, &charset, &error);
	g_assert_error (error, GMIME_ERROR, GMIME_ERROR_INVALID_OPERATION);
	g_clear_error (&error);
	check (!reader && !mime_type && !charset, "unbacked content reports an explicit invalid-operation error");
	failure = g_object_new (test_stream_failure_get_type (), NULL);
	failure->parent_instance.source = g_mime_stream_mem_new_with_buffer ("SGVsbG8=", 8);
	failure->fail_after = G_MAXSIZE;
	failure->fail_reset_on = 1;
	g_mime_stream_construct (GMIME_STREAM (failure), 0, -1);
	content = g_mime_content_new_with_stream (GMIME_STREAM (failure), GMIME_CONTENT_ENCODING_BASE64);
	g_mime_part_set_content (part, content);
	g_object_unref (content);
	reader = g_mime_multipart_related_open (related, "cid:root@host", &mime_type, &charset, &error);
	g_assert_error (error, GMIME_ERROR, ESPIPE);
	g_clear_error (&error);
	check (!reader && !mime_type && !charset && failure->read_calls == 0, "rewind errors propagate before reading or returning metadata");
	reader = g_mime_multipart_related_open (related, "cid:root@host", NULL, NULL, &error);
	g_assert_no_error (error);
	g_assert_nonnull (reader);
	check (failure->read_calls == 0, "resource opening remains lazy when metadata outputs are omitted");
	failure->fail_after = 0;
	errno = 0;
	check (g_mime_stream_read (reader, buffer, sizeof (buffer)) == -1 && errno == EIO,
	       "lazy decoded resource reads propagate backing-stream failures");
	g_object_unref (reader);
	g_mime_entity_set_content_id (GMIME_ENTITY (nested), "root@host");
	g_mime_multipart_insert (GMIME_MULTIPART (related), 0, GMIME_ENTITY (nested));
	reader = g_mime_multipart_related_open (related, "cid:root@host", &mime_type, &charset, &error);
	g_assert_error (error, GMIME_ERROR, ENOENT);
	g_clear_error (&error);
	check (!reader && !mime_type && !charset, "opening does not skip an unreadable first match for a later readable child");
	g_object_unref (failure);
	g_object_unref (related);
	g_object_unref (part);
	g_object_unref (nested);
	g_object_unref (descendant);

	for (i = 0; i < G_N_ELEMENTS (encodings); i++) {
		related = g_mime_multipart_related_new ();
		part = g_mime_part_new_with_type ("image", "png");
		entity = GMIME_ENTITY (part);
		g_mime_entity_set_content_id (entity, "root@host");
		g_mime_entity_set_content_type_parameter (entity, "charset", "utf-8");
		source = g_mime_stream_mem_new_with_buffer (encodings[i].encoded, encodings[i].encoded_length);
		weak = source;
		g_object_add_weak_pointer (G_OBJECT (source), (gpointer *) &weak);
		content = g_mime_content_new_with_stream (source, encodings[i].encoding);
		g_mime_part_set_content (part, content);
		g_mime_multipart_add (GMIME_MULTIPART (related), entity);
		header = g_mime_header_list_get_header (g_mime_entity_get_header_list (entity), "Content-Type");
		raw = g_strdup (testsuite_header_raw_text (header));
		g_mime_stream_seek (source, 0, GMIME_STREAM_SEEK_END);
		reader = g_mime_multipart_related_open (related, "cid:root@host", &mime_type, &charset, &error);
		g_assert_no_error (error);
		g_assert_nonnull (reader);
		check (!strcmp (mime_type, "image/png") && !strcmp (charset, "utf-8") &&
		       !strcmp (raw, testsuite_header_raw_text (header)), "resource opening returns allocated metadata without rewriting headers");
		g_free (raw);
		g_object_unref (content);
		g_object_unref (source);
		g_object_unref (part);
		g_object_unref (related);
		check (weak != NULL && !strcmp (mime_type, "image/png") && !strcmp (charset, "utf-8"),
		       "returned reader and metadata outlive the multipart and resource");
		output = g_mime_stream_mem_new ();
		check (g_mime_stream_write_to_stream (reader, output) == (ssize_t) encodings[i].decoded_length,
		       "resource opening decodes each content transfer encoding");
		bytes = g_mime_stream_mem_get_byte_array (GMIME_STREAM_MEM (output));
		check (bytes->len == encodings[i].decoded_length && !memcmp (bytes->data, encodings[i].decoded, bytes->len),
		       "decoded resources preserve binary bytes including NUL");
		check (g_mime_stream_close (reader) == 0 && g_mime_stream_reset (weak) == 0, "closing a resource reader does not close its backing stream");
		g_object_unref (reader);
		check (weak == NULL, "resource reader releases its retained backing stream");
		g_object_unref (output);
		g_free (mime_type);
		g_free (charset);
	}
}

static void
test_entity_content_uris (void)
{
	static const gunichar whitespace[] = {
		'\t', '\n', '\v', '\f', '\r', ' ', 0x85, 0xa0, 0x1680,
		0x2000, 0x2001, 0x2002, 0x2003, 0x2004, 0x2005, 0x2006,
		0x2007, 0x2008, 0x2009, 0x200a, 0x2028, 0x2029, 0x202f, 0x205f, 0x3000
	};
	static const struct {
		const char *value;
		gboolean absolute;
		gboolean valid;
	} cases[] = {
		{ "", FALSE, TRUE }, { "image.png", FALSE, TRUE },
		{ "../images/a%20b.png", FALSE, TRUE }, { "//host/image.png", FALSE, TRUE },
		{ "?query=x", FALSE, TRUE }, { "#fragment", FALSE, TRUE },
		{ "https://host/a%2fb?q=x#fragment", TRUE, TRUE },
		{ "HTTP://HOST:80/a/../b", TRUE, TRUE }, { "cid:root@host", TRUE, TRUE },
		{ "http:/opaque", TRUE, TRUE },
		{ "urn:example:test", TRUE, TRUE }, { "file:///tmp/a", TRUE, TRUE },
		{ "https://host/a%00%ff", TRUE, TRUE }, { "https://host/\303\251", TRUE, TRUE },
		{ "https://b\303\274cher.test/a", TRUE, TRUE },
		{ "bad space", FALSE, FALSE }, { "bad%escape", FALSE, FALSE },
		{ "https://host/%", TRUE, FALSE }, { "https://host:65536/", TRUE, FALSE },
		{ "bad\\path", FALSE, FALSE }, { "bad<path", FALSE, FALSE },
		{ "bad|path", FALSE, FALSE }, { "bad\tpath", FALSE, FALSE },
		{ "bad\177path", FALSE, FALSE }, { "\377", FALSE, FALSE }
	};
	static const struct {
		const char *name;
		const char *(*get) (GMimeEntity *);
		void (*set) (GMimeEntity *, const char *);
		gboolean absolute;
	} properties[] = {
		{ "Content-Base", g_mime_entity_get_content_base, g_mime_entity_set_content_base, TRUE },
		{ "Content-Location", g_mime_entity_get_content_location, g_mime_entity_set_content_location, FALSE }
	};
	GMimeEntity *entities[] = {
		GMIME_ENTITY (g_mime_part_new ()),
		GMIME_ENTITY (g_mime_text_part_new ()),
		GMIME_ENTITY (g_mime_multipart_new ()),
		GMIME_ENTITY (g_mime_message_part_new ("rfc822")),
		GMIME_ENTITY (g_mime_multipart_related_new ())
	};
	const char *uri = "https://host/a%2fb?q=x#fragment", *value, *expected;
	GMimeHeaderList *headers;
	GMimeHeader *first, *later, *replacement;
	GMimeEntity *entity, *parsed;
	GMimeParser *parser;
	GMimeStream *stream;
	GError *error = NULL;
	char *raw, *wire;
	guint i, j, k, side;

	for (i = 0; i < G_N_ELEMENTS (entities); i++) {
		entity = entities[i];
		headers = g_mime_entity_get_header_list (entity);
		for (j = 0; j < G_N_ELEMENTS (properties); j++) {
			for (k = 0; k < G_N_ELEMENTS (cases); k++) {
				g_mime_header_list_clear (headers);
				check (properties[j].get (entity) == NULL, "missing URI headers return NULL");
				g_mime_header_list_append (headers, properties[j].name, cases[k].value, NULL);
				first = g_mime_header_list_get_header_at (headers, 0);
				raw = g_strdup (testsuite_header_raw_text (first));
				expected = cases[k].valid && (!properties[j].absolute || cases[k].absolute) ? cases[k].value : NULL;
				check (!g_strcmp0 (properties[j].get (entity), expected),
				       "URI getters validate absolute/relative references on every entity class");
				if (!expected)
					g_test_expect_message ("gmime", G_LOG_LEVEL_CRITICAL, "*is_content_uri*");
				properties[j].set (entity, cases[k].value);
				if (!expected)
					g_test_assert_expected_messages ();
				check (!g_strcmp0 (testsuite_header_raw_text (first), raw),
				       "URI reads, cached assignments, and rejected setters preserve raw header spelling");
				g_free (raw);
			}
			for (k = 0; k < G_N_ELEMENTS (whitespace); k++) {
				char buffer[7] = { 0 };
				gboolean lwsp = whitespace[k] == ' ' || whitespace[k] == '\t' ||
					whitespace[k] == '\r' || whitespace[k] == '\n';
				const char *reference = properties[j].absolute ? uri : "../a.png";

				g_unichar_to_utf8 (whitespace[k], buffer);
				for (side = 0; side < 3; side++) {
					char *text = g_strconcat (side != 1 ? buffer : "", reference,
								 side != 0 ? buffer : "", NULL);
					char *assignment = g_strconcat (" \t", text, "\r\n", NULL);

					expected = lwsp ? reference : text;
					if (whitespace[k] == '\v' || whitespace[k] == '\f' ||
					    (!lwsp && properties[j].absolute && side != 1))
						expected = NULL;
					g_mime_header_list_clear (headers);
					g_mime_header_list_set (headers, properties[j].name, assignment, NULL);
					first = g_mime_header_list_get_header (headers, properties[j].name);
					g_mime_header_list_append (headers, properties[j].name, uri, NULL);
					raw = g_strdup (testsuite_header_raw_text (first));
					testsuite_check ("Decoded %s whitespace U+%04X [%u/%u]",
							 properties[j].name, whitespace[k], i, side);
					if (!g_strcmp0 (properties[j].get (entity), expected) &&
					    !g_strcmp0 (properties[j].get (entity), expected) &&
					    !strcmp (testsuite_header_raw_text (first), raw) &&
					    !strcmp (g_mime_header_get_value (first), assignment) &&
					    g_mime_header_list_get_count (headers) == 2)
						testsuite_check_passed ();
					else
						testsuite_check_failed ("unexpected URI, duplicate fallback or header mutation");
					g_free (raw);
					g_free (assignment);
					g_free (text);
				}
			}
			g_mime_header_list_clear (headers);
			g_mime_header_list_append (headers, properties[j].name, "https://host/first", NULL);
			first = g_object_ref (g_mime_header_list_get_header_at (headers, 0));
			testsuite_header_set_raw_text (first, " \thttps://host/a%2fb?q=x#fragment \t\n");
			g_mime_header_list_append (headers, properties[j].name, "https://host/later", NULL);
			later = g_mime_header_list_get_header_at (headers, 1);
			check (!g_strcmp0 (properties[j].get (entity), uri), "URI getter trims and selects the first duplicate");
			raw = g_strdup (testsuite_header_raw_text (first));
			properties[j].set (entity, uri);
			check (g_mime_header_list_get_count (headers) == 2 &&
			       !g_strcmp0 (testsuite_header_raw_text (first), raw),
			       "assigning the cached URI preserves raw headers and duplicates");
			g_free (raw);
			testsuite_header_set_raw_text (later, " https://host/changed\n");
			check (!g_strcmp0 (properties[j].get (entity), uri), "later duplicate edits still select the first URI");
			g_mime_header_list_insert (headers, 0, properties[j].name, "https://host/inserted", NULL);
			check (!g_strcmp0 (properties[j].get (entity), "https://host/inserted"), "earlier insertion invalidates URI caches");
			g_mime_header_list_remove_at (headers, 0);
			g_mime_header_list_remove_header (headers, first);
			check (!g_strcmp0 (properties[j].get (entity), "https://host/changed"), "URI removal exposes the surviving duplicate");
			testsuite_header_set_raw_text (first, " https://host/detached\n");
			check (!g_strcmp0 (properties[j].get (entity), "https://host/changed"), "detached headers no longer affect URI caches");
			replacement = g_mime_header_new (properties[j].name, "https://host/replaced", NULL);
			g_mime_header_list_set_header_at (headers, 0, replacement);
			check (!g_strcmp0 (properties[j].get (entity), "https://host/replaced"), "indexed replacement invalidates URI caches");
			g_object_unref (replacement);
			properties[j].set (entity, uri);
			g_mime_header_list_append (headers, properties[j].name, "https://host/duplicate", NULL);
			properties[j].set (entity, "urn:example:replacement");
			check (g_mime_header_list_get_count (headers) == 1 &&
			       !g_strcmp0 (properties[j].get (entity), "urn:example:replacement"),
			       "changed URI assignments replace duplicate headers");
			value = properties[j].get (entity);
			properties[j].set (entity, value + 4);
			check (!g_strcmp0 (properties[j].get (entity), "example:replacement"),
			       "URI setters safely accept substrings of their borrowed cached value");
			g_test_expect_message ("gmime", G_LOG_LEVEL_CRITICAL, "*is_content_uri*");
			properties[j].set (entity, "bad space");
			g_test_assert_expected_messages ();
			check (!g_strcmp0 (properties[j].get (entity), "example:replacement"),
			       "invalid URI assignments leave the entity unchanged");
			if (properties[j].absolute) {
				g_test_expect_message ("gmime", G_LOG_LEVEL_CRITICAL, "*is_content_uri*");
				properties[j].set (entity, "relative.png");
				g_test_assert_expected_messages ();
			}
			g_mime_header_list_clear (headers);
			g_mime_header_list_append (headers, properties[j].name, "bad space", NULL);
			g_mime_header_list_append (headers, properties[j].name, uri, NULL);
			check (properties[j].get (entity) == NULL, "an invalid first URI does not fall through to a valid duplicate");
			properties[j].set (entity, NULL);
			check (g_mime_header_list_get_count (headers) == 2, "a cached NULL URI assignment preserves malformed headers");
			g_mime_header_list_append (headers, properties[j].name, "urn:changed", NULL);
			properties[j].set (entity, NULL);
			check (g_mime_header_list_get_count (headers) == 0 && properties[j].get (entity) == NULL,
			       "a NULL URI assignment before reading removes all matching headers");
			g_object_unref (first);
		}
		g_mime_entity_set_content_base (entity, "https://host/base/");
		g_mime_entity_set_content_location (entity, "images/a.png");
		if (GMIME_IS_PART (entity)) {
			check (!g_strcmp0 (g_mime_part_get_content_location (GMIME_PART (entity)), "images/a.png"),
			       "part-level location getter delegates to the entity property");
			g_mime_part_set_content_location (GMIME_PART (entity), NULL);
			check (g_mime_entity_get_content_location (entity) == NULL &&
			       g_mime_header_list_get_header (headers, "Content-Location") == NULL,
			       "legacy part-level location setter removes the inherited header");
		}
		check (!g_strcmp0 (g_mime_entity_get_content_base (entity), "https://host/base/"),
		       "location changes do not invalidate Content-Base");
		g_mime_entity_set_header (entity, "Content-Location", "\302\240../a.png\342\200\203", NULL);
		check (!g_strcmp0 (g_mime_entity_get_content_location (entity), "\302\240../a.png\342\200\203"),
		       "URI getters preserve Unicode whitespace in decoded header values");
		g_mime_header_list_clear (headers);
		check (g_mime_entity_get_content_base (entity) == NULL && g_mime_entity_get_content_location (entity) == NULL,
		       "clearing headers resets both inherited URI caches");
		g_object_unref (entity);
	}

	wire = g_strdup ("Content-Type: multipart/related; boundary=x\r\nContent-Base: https://host/base/\r\n"
			 "Content-Location: index.html\r\n\r\n--x\r\nContent-Type: text/html\r\n"
			 "Content-Location: ../page.html\r\n\r\nbody\r\n--x--\r\n");
	stream = g_mime_stream_mem_new_with_buffer (wire, strlen (wire));
	parser = g_mime_parser_new_with_stream (stream);
	parsed = g_mime_parser_construct_part (parser, NULL, &error);
	g_assert_no_error (error);
	g_assert_true (GMIME_IS_MULTIPART_RELATED (parsed));
	check (!g_strcmp0 (g_mime_entity_get_content_base (parsed), "https://host/base/") &&
	       !g_strcmp0 (g_mime_entity_get_content_location (parsed), "index.html") &&
	       !g_strcmp0 (g_mime_entity_get_content_location (g_mime_multipart_get_part (GMIME_MULTIPART (parsed), 0)), "../page.html"),
	       "parsed related containers and child parts inherit URI properties");
	raw = g_mime_entity_to_string (parsed, NULL);
	check (raw && strstr (raw, "https://host/base/") && strstr (raw, "../page.html"), "URI headers survive serialization");
	g_free (raw);
	g_free (wire);
	g_object_unref (parsed);
	g_object_unref (parser);
	g_object_unref (stream);
}

static void
test_entity_attachments (void)
{
	GMimeEntity *entities[] = {
		GMIME_ENTITY (g_mime_part_new_with_type ("application", "octet-stream")),
		GMIME_ENTITY (g_mime_multipart_new ()),
		GMIME_ENTITY (g_mime_message_part_new ("rfc822"))
	};
	GMimeHeaderList *headers;
	GMimeContentDisposition *disposition;
	GMimeHeader *first;
	const char *raw = "\tAtTaChMeNt (keep); filename=\"file.txt\"\n";
	guint i;

	for (i = 0; i < G_N_ELEMENTS (entities); i++) {
		headers = g_mime_entity_get_header_list (entities[i]);
		g_mime_entity_set_is_attachment (entities[i], FALSE);
		check (!g_mime_entity_is_attachment (entities[i]) &&
		       !g_mime_header_list_contains (headers, "Content-Disposition"),
		       "clearing attachment status on an entity without a disposition creates no header");
		g_mime_entity_set_content_type_parameter (entities[i], "name", "file.txt");
		check (!g_mime_entity_is_attachment (entities[i]), "a Content-Type filename does not imply attachment status");
		g_mime_entity_set_is_attachment (entities[i], TRUE);
		check (g_mime_entity_is_attachment (entities[i]) &&
		       !strcmp (g_mime_entity_get_disposition (entities[i]), "attachment"),
		       "all entity types can create an attachment disposition");
		g_mime_entity_set_disposition (entities[i], "custom");
		g_mime_entity_set_content_disposition_parameter (entities[i], "filename", "file.txt");
		disposition = g_mime_entity_get_content_disposition (entities[i]);
		g_mime_entity_set_is_attachment (entities[i], FALSE);
		check (g_mime_entity_get_content_disposition (entities[i]) == disposition &&
		       !strcmp (g_mime_entity_get_disposition (entities[i]), "custom"),
		       "clearing a custom disposition preserves its value and object identity");
		g_mime_entity_set_is_attachment (entities[i], TRUE);
		check (g_mime_entity_get_content_disposition (entities[i]) == disposition &&
		       !strcmp (g_mime_entity_get_content_disposition_parameter (entities[i], "filename"), "file.txt"),
		       "changing attachment status retains the disposition instance and parameters");
		first = g_mime_header_list_get_header (headers, "Content-Disposition");
		testsuite_header_set_raw_text (first, raw);
		g_mime_header_list_append (headers, "Content-Disposition", "attachment; filename=file.txt", NULL);
		g_mime_entity_set_is_attachment (entities[i], TRUE);
		check (g_mime_entity_is_attachment (entities[i]) &&
		       !strcmp (testsuite_header_raw_text (first), raw) &&
		       g_mime_header_list_last_index_of (headers, "Content-Disposition") !=
		       g_mime_header_list_index_of (headers, "Content-Disposition"),
		       "already-attached entities preserve raw capitalization, comments and duplicate headers");
		g_mime_entity_set_is_attachment (entities[i], FALSE);
		check (!g_mime_entity_is_attachment (entities[i]) &&
		       !strcmp (g_mime_entity_get_disposition (entities[i]), "inline") &&
		       !strcmp (g_mime_entity_get_content_disposition_parameter (entities[i], "filename"), "file.txt") &&
		       g_mime_header_list_last_index_of (headers, "Content-Disposition") ==
		       g_mime_header_list_index_of (headers, "Content-Disposition"),
		       "clearing a true attachment preserves parameters and serializes inline without duplicates");
		if (GMIME_IS_PART (entities[i]))
			check (!g_mime_part_is_attachment (GMIME_PART (entities[i])),
			       "the legacy part attachment predicate agrees with the entity predicate");
		g_mime_header_list_clear (headers);
		check (!g_mime_entity_is_attachment (entities[i]), "clearing raw headers resets entity attachment status");
		g_object_unref (entities[i]);
	}
}

static void
test_message_body_parts (void)
{
	GMimeMessage *message = g_mime_message_new (FALSE);
	GMimeMessage *inner = g_mime_message_new (FALSE);
	GMimeMultipart *root = g_mime_multipart_new ();
	GMimeMultipart *nested = g_mime_multipart_new_with_subtype ("alternative");
	GMimeMultipart *empty = g_mime_multipart_new ();
	GMimePart *inline_part = g_mime_part_new_with_type ("text", "plain");
	GMimePart *attachment = g_mime_part_new_with_type ("application", "octet-stream");
	GMimePart *named_inline = g_mime_part_new_with_type ("image", "png");
	GMimePart *hidden = g_mime_part_new_with_type ("text", "plain");
	GMimeMessagePart *attached_message = g_mime_message_part_new_with_message ("rfc822", inner);
	GMimeMessagePart *empty_message = g_mime_message_part_new ("rfc822");
	GMimeEntity *expected[] = {
		GMIME_ENTITY (inline_part), GMIME_ENTITY (attachment), GMIME_ENTITY (named_inline),
		GMIME_ENTITY (attached_message), GMIME_ENTITY (empty_message), GMIME_ENTITY (attachment)
	};
	GPtrArray *parts, *attachments, *fresh;
	GMimeMultipart *parent, *child;
	gpointer weak = attachment;
	guint i;

	parts = g_mime_message_get_body_parts (message);
	attachments = g_mime_message_get_attachments (message);
	check (parts && attachments && parts != attachments && !parts->len && !attachments->len,
	       "absent message bodies return independent non-NULL empty arrays");
	g_ptr_array_unref (parts);
	g_ptr_array_unref (attachments);
	g_mime_entity_set_is_attachment (GMIME_ENTITY (root), TRUE);
	g_mime_entity_set_is_attachment (GMIME_ENTITY (nested), TRUE);
	g_mime_entity_set_is_attachment (GMIME_ENTITY (attachment), TRUE);
	g_mime_entity_set_is_attachment (GMIME_ENTITY (attached_message), TRUE);
	g_mime_entity_set_is_attachment (GMIME_ENTITY (hidden), TRUE);
	g_mime_entity_set_disposition (GMIME_ENTITY (named_inline), "inline");
	g_mime_entity_set_content_disposition_parameter (GMIME_ENTITY (named_inline), "filename", "image.png");
	g_mime_message_set_mime_part (inner, GMIME_ENTITY (hidden));
	g_mime_multipart_add (nested, GMIME_ENTITY (attachment));
	g_mime_multipart_add (nested, GMIME_ENTITY (named_inline));
	g_mime_multipart_add (nested, GMIME_ENTITY (empty));
	g_mime_multipart_add (root, GMIME_ENTITY (inline_part));
	g_mime_multipart_add (root, GMIME_ENTITY (nested));
	g_mime_multipart_add (root, GMIME_ENTITY (attached_message));
	g_mime_multipart_add (root, GMIME_ENTITY (empty_message));
	g_mime_multipart_add (root, GMIME_ENTITY (attachment));
	g_mime_message_set_mime_part (message, GMIME_ENTITY (root));
	parts = g_mime_message_get_body_parts (message);
	check (parts->len == G_N_ELEMENTS (expected), "body parts exclude containers and embedded-message descendants");
	for (i = 0; i < parts->len && i < G_N_ELEMENTS (expected); i++)
		check (parts->pdata[i] == expected[i], "body parts retain depth-first identity and repeated occurrences");
	attachments = g_mime_message_get_attachments (message);
	check (attachments->len == 3 && attachments->pdata[0] == attachment &&
	       attachments->pdata[1] == attached_message && attachments->pdata[2] == attachment,
	       "attachment snapshots exclude inline filenames, multipart containers and inner attachments");
	g_mime_entity_set_is_attachment (GMIME_ENTITY (attachment), FALSE);
	fresh = g_mime_message_get_attachments (message);
	check (attachments->len == 3 && fresh->len == 1 && fresh->pdata[0] == attached_message,
	       "attachment membership is a snapshot while new calls evaluate shared entity mutations");
	g_ptr_array_unref (fresh);
	g_ptr_array_set_size (attachments, 0);
	check (g_mime_multipart_get_count (root) == 5 && parts->len == 6,
	       "editing attachment snapshot membership does not change the MIME tree or other snapshots");
	g_ptr_array_unref (attachments);
	g_mime_multipart_clear (root);
	check (parts->len == 6 && g_mime_message_get_mime_part (message) == GMIME_ENTITY (root),
	       "removing source entities preserves the retained body-part snapshot");
	g_object_add_weak_pointer (G_OBJECT (attachment), &weak);
	g_object_unref (root);
	g_object_unref (nested);
	g_object_unref (empty);
	g_object_unref (inline_part);
	g_object_unref (attachment);
	g_object_unref (named_inline);
	g_object_unref (hidden);
	g_object_unref (attached_message);
	g_object_unref (empty_message);
	g_object_unref (inner);
	g_object_unref (message);
	check (weak != NULL && GMIME_IS_ENTITY (parts->pdata[0]), "body-part snapshots own entity references after message finalization");
	g_mime_entity_set_is_attachment (parts->pdata[0], TRUE);
	g_ptr_array_unref (parts);
	check (weak == NULL, "releasing the body-part array releases every retained occurrence");

	message = g_mime_message_new (FALSE);
	root = g_mime_multipart_new ();
	g_mime_message_set_mime_part (message, GMIME_ENTITY (root));
	parts = g_mime_message_get_body_parts (message);
	check (parts->len == 0, "an empty root multipart contributes no leaf");
	g_ptr_array_unref (parts);
	parent = root;
	for (i = 0; i < 256; i++) {
		child = g_mime_multipart_new ();
		g_mime_multipart_add (parent, GMIME_ENTITY (child));
		g_object_unref (child);
		parent = child;
	}
	attachment = g_mime_part_new_with_type ("application", "octet-stream");
	g_mime_entity_set_is_attachment (GMIME_ENTITY (attachment), TRUE);
	g_mime_multipart_add (parent, GMIME_ENTITY (attachment));
	parts = g_mime_message_get_body_parts (message);
	attachments = g_mime_message_get_attachments (message);
	check (parts->len == 1 && parts->pdata[0] == attachment && attachments->len == 1 &&
	       attachments->pdata[0] == attachment, "body-part enumeration traverses deeply nested containers");
	g_ptr_array_unref (parts);
	g_ptr_array_unref (attachments);
	g_mime_message_set_mime_part (message, GMIME_ENTITY (attachment));
	parts = g_mime_message_get_body_parts (message);
	check (parts->len == 1 && parts->pdata[0] == attachment, "a non-multipart root body is included");
	g_ptr_array_unref (parts);
	g_object_unref (attachment);
	g_object_unref (root);
	g_object_unref (message);
}

static void
test_message_entity_paths (void)
{
	static const char *invalid[] = {
		"", ".", ".1", "1.", "1..1", "+1", "-1", " 1", "1 ", "1\n",
		"1. 1", "1.+1", "1.-1", "0.1", "00.1", "1.0", "2.0",
		"2.1.1", "3.0.1", "3.1.0", "3.2", "4.0", "5.1", "6.0",
		"7", "2147483647", "2147483648", "4294967296", "99999999999999999999",
		"1.2147483648", "0.2147483648", "1x", "0x1", "1/1",
		"\331\241", "\357\274\221", "1.\331\241"
	};
	GMimeMessage *message = g_mime_message_new (FALSE);
	GMimeMessage *inner = g_mime_message_new (FALSE);
	GMimeMessage *plain_message = g_mime_message_new (FALSE);
	GMimeMessage *bodyless = g_mime_message_new (FALSE);
	GMimeMultipart *root = g_mime_multipart_new ();
	GMimeMultipart *nested = g_mime_multipart_new_with_subtype ("alternative");
	GMimeMultipart *inner_root = g_mime_multipart_new ();
	GMimePart *plain = g_mime_part_new_with_type ("text", "plain");
	GMimePart *html = g_mime_part_new_with_type ("text", "html");
	GMimeMessagePart *attached = g_mime_message_part_new_with_message ("rfc822", inner);
	GMimeMessagePart *plain_attached = g_mime_message_part_new_with_message ("global", plain_message);
	GMimeMessagePart *empty = g_mime_message_part_new ("rfc822");
	GMimeMessagePart *bodyless_attached = g_mime_message_part_new_with_message ("rfc822", bodyless);
	GMimeEntity *entity = GMIME_ENTITY (root);
	struct {
		const char *path;
		GMimeEntity *expected;
	} valid[] = {
		{ "0", GMIME_ENTITY (root) }, { "000", GMIME_ENTITY (root) },
		{ "1", GMIME_ENTITY (plain) }, { "0001", GMIME_ENTITY (plain) },
		{ "2", GMIME_ENTITY (nested) }, { "2.1", GMIME_ENTITY (html) },
		{ "3", GMIME_ENTITY (attached) }, { "3.0", GMIME_ENTITY (inner_root) },
		{ "03.00", GMIME_ENTITY (inner_root) }, { "3.1", GMIME_ENTITY (html) },
		{ "4", GMIME_ENTITY (plain_attached) }, { "4.1", GMIME_ENTITY (plain) },
		{ "5", GMIME_ENTITY (empty) }, { "6", GMIME_ENTITY (bodyless_attached) }
	};
	GMimeMultipart *parent, *child;
	GString *path = g_string_new (NULL);
	gpointer weak;
	guint i;

	check (!g_mime_message_try_get_entity (message, "1", &entity) && !entity,
	       "entity lookup clears its output for an absent message body");
	g_mime_multipart_add (root, GMIME_ENTITY (plain));
	g_mime_multipart_add (nested, GMIME_ENTITY (html));
	g_mime_multipart_add (inner_root, GMIME_ENTITY (html));
	g_mime_message_set_mime_part (inner, GMIME_ENTITY (inner_root));
	g_mime_message_set_mime_part (plain_message, GMIME_ENTITY (plain));
	g_mime_multipart_add (root, GMIME_ENTITY (nested));
	g_mime_multipart_add (root, GMIME_ENTITY (attached));
	g_mime_multipart_add (root, GMIME_ENTITY (plain_attached));
	g_mime_multipart_add (root, GMIME_ENTITY (empty));
	g_mime_multipart_add (root, GMIME_ENTITY (bodyless_attached));
	g_mime_message_set_mime_part (message, GMIME_ENTITY (root));
	for (i = 0; i < G_N_ELEMENTS (valid); i++) {
		check (g_mime_message_try_get_entity (message, valid[i].path, &entity) && entity == valid[i].expected,
		       "MIME paths preserve identity across containers and embedded message bodies");
	}
	for (i = 0; i < G_N_ELEMENTS (invalid); i++) {
		entity = GMIME_ENTITY (root);
		check (!g_mime_message_try_get_entity (message, invalid[i], &entity) && !entity,
		       "malformed and missing MIME paths return FALSE and clear the output");
	}
	g_string_append (path, "00000000000000000000000000000000000000001");
	check (g_mime_message_try_get_entity (message, path->str, &entity) && entity == GMIME_ENTITY (plain),
	       "leading zeros do not cause spurious path overflow");
	g_mime_message_set_mime_part (message, GMIME_ENTITY (plain));
	check (g_mime_message_try_get_entity (message, "1", &entity) && entity == GMIME_ENTITY (plain),
	       "a non-multipart root body is part 1");
	check (!g_mime_message_try_get_entity (message, "0", &entity) && !entity &&
	       !g_mime_message_try_get_entity (message, "1.1", &entity) && !entity,
	       "a non-multipart root body has neither a part 0 nor child parts");
	g_mime_message_set_mime_part (message, GMIME_ENTITY (attached));
	check (g_mime_message_try_get_entity (message, "1.0", &entity) && entity == GMIME_ENTITY (inner_root) &&
	       g_mime_message_try_get_entity (message, "1.1", &entity) && entity == GMIME_ENTITY (html),
	       "a root message part uses part 1 before entering its embedded body");
	g_mime_message_set_mime_part (inner, GMIME_ENTITY (plain_attached));
	check (g_mime_message_try_get_entity (message, "1.1", &entity) && entity == GMIME_ENTITY (plain_attached) &&
	       g_mime_message_try_get_entity (message, "1.1.1", &entity) && entity == GMIME_ENTITY (plain),
	       "chains of attached messages consume one component per non-multipart body");

	g_mime_message_set_mime_part (message, GMIME_ENTITY (root));
	g_mime_multipart_clear (root);
	check (g_mime_message_try_get_entity (message, "0", &entity) && entity == GMIME_ENTITY (root) &&
	       !g_mime_message_try_get_entity (message, "1", &entity) && !entity,
	       "an empty multipart message body still has a part 0");
	parent = root;
	g_string_truncate (path, 0);
	for (i = 0; i < 256; i++) {
		child = g_mime_multipart_new ();
		g_mime_multipart_add (parent, GMIME_ENTITY (child));
		g_object_unref (child);
		parent = child;
		g_string_append (path, "1.");
	}
	g_mime_multipart_add (parent, GMIME_ENTITY (plain));
	g_string_append_c (path, '1');
	check (g_mime_message_try_get_entity (message, path->str, &entity) && entity == GMIME_ENTITY (plain),
	       "entity lookup follows deeply nested paths without recursive navigation");
	g_object_ref (entity);
	g_string_free (path, TRUE);
	g_object_unref (plain);
	g_object_unref (html);
	g_object_unref (nested);
	g_object_unref (inner_root);
	g_object_unref (attached);
	g_object_unref (plain_attached);
	g_object_unref (empty);
	g_object_unref (bodyless_attached);
	g_object_unref (inner);
	g_object_unref (plain_message);
	g_object_unref (bodyless);
	weak = entity;
	g_object_add_weak_pointer (G_OBJECT (entity), &weak);
	g_object_unref (root);
	g_object_unref (message);
	check (weak == entity && GMIME_IS_PART (entity), "callers can retain a looked-up entity beyond message finalization");
	g_object_unref (entity);
	check (!weak, "entity lookup does not add a hidden reference to the returned entity");
}

static void
test_rfc2047_alias_runs (void)
{
	static const struct {
		const char *input, *decoded;
	} cases[] = {
		{ "=?windows-1251?Q?=C?= =?cp1251?q?6?=", "\320\226" },
		{ "=?cp1251?b?x?= =?windows-1251?B?g==?=", "\320\226" },
		{ "=?euc-jp?Q?=A4?= =?ujis?Q?=A2?=", "\343\201\202" },
		{ "=?ujis?B?p?= =?euc-jp?B?KI=?=", "\343\201\202" },
		{ "=?euc-jp?Q?=?= =?ujis?Q?A?= =?eucJP?Q?4=A2?=", "\343\201\202" },
		{ "=?ujis?Q?=A4?= =?euc-jp?Q?" "?= =?eucJP?Q?=A2?=", "\343\201\202" },
		{ "=?euc-jp*ja?Q?=A4?=\r\n\t=?ujis*en?Q?=A2?=", "\343\201\202" },
		{ "=?utf8?Q?=C3?= =?UTF-8?Q?=A9?=", "\303\251" },
		{ "=?euc-jp?Q?=?= =?latin1?Q?41?=", "41" },
		{ "=?euc-jp?Q?=?= =?ujis?B?NDE=?=", "41" },
		{ "=?euc-jp?Q?=?= raw =?ujis?Q?41?=", " raw 41" },
		{ "=?euc-jp?Q?=?= =?ujis?B?QQ==?= =?eucJP?Q?41?=", "A41" },
		{ "=?x-one?Q?=?= =?x-two?Q?41?=", "41" },
		{ "=?x-one?Q?=?= =?X-ONE?Q?41?=", "A" },
		{ "=?euc-jp?B?Q?= =?latin1?B?Q==?=", "" }
	};
	const char *raw = "Subject: =?euc-jp?Q?=A4?= =?UJIS?Q?=A2?=\n";
	GMimeParserOptions *options = g_mime_parser_options_new ();
	GMimeInternetAddressList *addresses;
	GMimeInternetAddress *address;
	GMimeParamList *params;
	GMimeParam *param;
	char *decoded, *folded;
	guint mode, phrase, i;

	for (mode = 0; mode < 2; mode++) {
		g_mime_parser_options_set_rfc2047_compliance_mode (options,
			mode ? GMIME_RFC_COMPLIANCE_STRICT : GMIME_RFC_COMPLIANCE_LOOSE);
		for (phrase = 0; phrase < 2; phrase++) {
			for (i = 0; i < G_N_ELEMENTS (cases); i++) {
				decoded = phrase ? g_mime_utils_header_decode_phrase (options, cases[i].input) :
					g_mime_utils_header_decode_text (options, cases[i].input);
				testsuite_check ("alias-aware encoded-word runs [%u,%u,%u]", mode, phrase, i);
				if (!strcmp (decoded, cases[i].decoded))
					testsuite_check_passed ();
				else
					testsuite_check_failed ("actual=\"%s\", expected=\"%s\"", decoded, cases[i].decoded);
				g_free (decoded);
			}
		}

		addresses = g_mime_internet_address_list_parse (options,
			"=?euc-jp?Q?=A4?= =?ujis?Q?=A2?= <probe@example.com>");
		address = g_mime_internet_address_list_get_address (addresses, 0);
		check (!strcmp (g_mime_internet_address_get_name (address), "\343\201\202") &&
		       !g_ascii_strcasecmp (g_mime_internet_address_get_charset (address), "euc-jp"),
		       "alias-aware runs preserve mailbox names and the first native charset");
		g_object_unref (addresses);

		addresses = g_mime_internet_address_list_parse (options,
			"=?euc-jp?Q?=A4?= =?ujis?Q?=A2?=: probe@example.com;");
		address = g_mime_internet_address_list_get_address (addresses, 0);
		check (!strcmp (g_mime_internet_address_get_name (address), "\343\201\202") &&
		       !g_ascii_strcasecmp (g_mime_internet_address_get_charset (address), "euc-jp"),
		       "alias-aware runs preserve group names and the first native charset");
		g_object_unref (addresses);

		params = g_mime_param_list_parse_full (options,
			"filename=\"=?euc-jp?Q?=A4?= =?ujis?Q?=A2?=\"", -1, NULL);
		param = g_mime_param_list_get_parameter (params, "filename");
		check (!strcmp (g_mime_param_get_value (param), "\343\201\202") &&
		       !g_ascii_strcasecmp (g_mime_param_get_charset (param), "euc-jp"),
		       "alias-aware runs preserve parameter values and the first native charset");
		g_object_unref (params);
	}

	folded = g_mime_utils_unstructured_header_fold (options, NULL, raw);
	check (!strcmp (folded, raw), "alias-aware decoding does not rewrite raw unstructured headers");
	g_free (folded);
	folded = g_mime_utils_structured_header_fold (options, NULL, raw);
	check (!strcmp (folded, raw), "alias-aware decoding does not rewrite raw structured headers");
	g_free (folded);
	g_mime_parser_options_free (options);
}

static void
test_rfc2047_token_splitting (void)
{
	static const struct {
		guint width;
		const char *charset, *glyph;
		guint count, first, words, text_first[2], text_words[2];
	} cases[] = {
		{ 60, "utf-8", "a", 58, 58, 1, { 58, 58 }, { 1, 1 } },
		{ 60, "utf-8", "a", 59, 43, 2, { 43, 43 }, { 2, 2 } },
		{ 61, "utf-8", "a", 60, 44, 2, { 44, 44 }, { 2, 2 } },
		{ 78, "utf-8", "a", 77, 61, 2, { 61, 30 }, { 2, 4 } },
		{ 120, "utf-8", "a", 119, 103, 2, { 51, 51 }, { 4, 4 } },
		{ 998, "utf-8", "a", 997, 981, 2, { 61, 30 }, { 18, 34 } },
		{ 60, "utf-8", "\303\251", 31, 30, 2, { 15, 30 }, { 3, 2 } },
		{ 78, "utf-8", "\303\251", 43, 42, 2, { 21, 42 }, { 3, 2 } },
		{ 60, "utf-8", "\346\227\245", 12, 11, 2, { 11, 11 }, { 2, 2 } },
		{ 62, "utf-8", "\346\227\245", 13, 12, 2, { 12, 12 }, { 2, 2 } },
		{ 60, "utf-8", "\360\237\230\200", 9, 8, 2, { 8, 8 }, { 2, 2 } },
		{ 60, "utf-16le", "\346\227\245", 17, 16, 2, { 16, 16 }, { 2, 2 } },
		{ 60, "utf-16be", "\346\227\245", 16, 15, 2, { 15, 15 }, { 2, 2 } },
		{ 60, "utf-32le", "\346\227\245", 9, 8, 2, { 8, 8 }, { 2, 2 } },
		{ 60, "utf-32be", "\346\227\245", 8, 7, 2, { 7, 7 }, { 2, 2 } },
		{ 60, "utf-16le", "\360\237\230\200", 9, 8, 2, { 8, 8 }, { 2, 2 } },
		{ 78, "utf-8", "\360\237\230\200", 30, 12, 3, { 6, 6 }, { 5, 5 } },
		{ 120, "utf-8", "\360\237\230\200", 30, 19, 2, { 9, 9 }, { 4, 4 } },
		{ 60, "iso-2022-jp", "\346\227\245", 4, 3, 2, { 3, 3 }, { 2, 2 } }
	};
	static const char *fallback_values[] = {
		"\346\227\245", "a\346\227\245", "aaaaaaaaaaaaaaa\346\227\245",
		"aaaaaaaaaaaaaaaa\346\227\245"
	};
	GMimeFormatOptions *options = g_mime_format_options_new ();
	char *encoded, *decoded, **words, *charset;
	GString *value;
	guint i, j, mixed, phrase;

	for (i = 0; i < G_N_ELEMENTS (cases); i++) {
		value = g_string_new (NULL);
		for (j = 0; j < cases[i].count; j++)
			g_string_append (value, cases[i].glyph);
		g_mime_format_options_set_max_line_length (options, cases[i].width);
		for (mixed = 0; mixed < 2; mixed++) {
			g_mime_format_options_set_allow_mixed_charsets (options, mixed != 0);
			for (phrase = 0; phrase < 2; phrase++) {
				encoded = phrase ? g_mime_utils_header_encode_phrase (options, value->str, cases[i].charset) :
					g_mime_utils_header_encode_text (options, value->str, cases[i].charset);
				decoded = g_mime_utils_header_decode_text (NULL, encoded);
				check (!strcmp (decoded, value->str), "token splitting preserves complete Unicode scalars and decoded text");
				g_free (decoded);
				words = g_strsplit_set (encoded, " \t", -1);
				check (g_strv_length (words) == (phrase ? cases[i].words : cases[i].text_words[mixed]),
				       "token splitting matches MimeKit encoded-word counts");
				decoded = g_mime_utils_header_decode_text (NULL, words[0]);
				check (g_utf8_strlen (decoded, -1) == (phrase ? cases[i].first : cases[i].text_first[mixed]),
				       "the first token matches MimeKit's strict width and charset boundary");
				for (j = 0; !phrase && cases[i].text_words[mixed] > 1 && words[j]; j++)
					check (strlen (words[j]) <= MIN (75, cases[i].width - 1),
					       "text encoded words obey the final actual-size bound");
				check (g_str_has_prefix (encoded, "=?") == (cases[i].words > 1),
				       "oversized ASCII atoms become encoded words");
				g_free (decoded);
				g_strfreev (words);
				g_free (encoded);
			}
		}
		g_string_free (value, TRUE);
	}

	g_mime_format_options_set_max_line_length (options, 60);
	g_mime_format_options_set_allow_mixed_charsets (options, FALSE);
	charset = g_strnfill (80, 'x');
	for (i = 0; i < G_N_ELEMENTS (fallback_values); i++) {
		for (phrase = 0; phrase < 2; phrase++) {
			encoded = phrase ? g_mime_utils_header_encode_phrase (options, fallback_values[i], charset) :
				g_mime_utils_header_encode_text (options, fallback_values[i], charset);
			decoded = g_mime_utils_header_decode_text (NULL, encoded);
			check (!strcmp (decoded, fallback_values[i]), "oversized unsupported charset labels cannot rewind before a token");
			g_free (decoded);
			g_free (encoded);
		}
	}
	g_free (charset);
	for (phrase = 0; phrase < 2; phrase++) {
		encoded = phrase ? g_mime_utils_header_encode_phrase (options, "A\344\270\255", "us-ascii//IGNORE") :
			g_mime_utils_header_encode_text (options, "A\344\270\255", "us-ascii//IGNORE");
		decoded = g_mime_utils_header_decode_text (NULL, encoded);
		check (!strcmp (decoded, "A\344\270\255"), "token measurement does not discard unrepresentable text via iconv extensions");
		g_free (decoded);
		g_free (encoded);
	}
	g_mime_format_options_free (options);
}

static void
test_header_construction (void)
{
	static const struct {
		const char *name, *value, *normalized;
	} cases[] = {
		{ "sUbJeCt", " \tcaf\303\251\r\n ", "caf\303\251" },
		{ "X-Folded", "\r\none\r\ntwo\nthree\rfour\tfive\r\n", "onetwothreefour\tfive" },
		{ "To", " Alice <alice@example.com> ", "Alice <alice@example.com>" },
		{ "Content-Type", " text/plain; charset=utf-8 ", "text/plain; charset=utf-8" },
		{ "Content-Disposition", " attachment; filename=\"a file.txt\" ", "attachment; filename=\"a file.txt\"" },
		{ "References", " <one@example.com> <two@example.com> ", "<one@example.com> <two@example.com>" },
		{ "DKIM-Signature", " v=1; b=YWJj; ", "v=1; b=YWJj;" },
		{ "X-Empty", "", "" }
	};
	static const char *whitespace[] = {
		"\t", "\n", "\v", "\f", "\r", " ", "\302\205", "\302\240",
		"\341\232\200", "\342\200\200", "\342\200\201", "\342\200\202",
		"\342\200\203", "\342\200\204", "\342\200\205", "\342\200\206",
		"\342\200\207", "\342\200\210", "\342\200\211", "\342\200\212",
		"\342\200\250", "\342\200\251", "\342\200\257", "\342\201\237", "\343\200\200"
	};
	static const char *invalid_names[] = {
		"", "X:Test", "X Test", "X\tTest", "X\rTest", "X\nTest",
		"X\001Test", "X\177Test", "X-\303\251"
	};
	GMimeHeaderList *headers = g_mime_header_list_new (NULL);
	GMimeHeader *header, *clone, *other;
	char *wire, *expected, *text;
	char name[] = "sUbJeCt", value[] = " caf\303\251 ", charset[] = "iso-8859-1";
	guint i;

	for (i = 0; i < G_N_ELEMENTS (cases); i++) {
		header = g_mime_header_new (cases[i].name, cases[i].value, NULL);
		g_assert_nonnull (header);
		check (!strcmp (g_mime_header_get_name (header), cases[i].name) &&
		       !strcmp (testsuite_header_raw_name (header), cases[i].name),
		       "standalone construction preserves field-name case");
		check (!strcmp (g_mime_header_get_value (header), cases[i].normalized),
		       "standalone construction trims and unfolds the cached value");
		check (g_mime_header_get_offset (header) == -1 && header->owns_options,
		       "standalone construction owns parser options and has no source offset");
		g_mime_header_list_clear (headers);
		g_mime_header_list_append (headers, cases[i].name, cases[i].normalized, "utf-8");
		wire = header_wire_string (header, NULL);
		expected = g_mime_header_list_to_string (headers, NULL);
		check (!strcmp (wire, expected), "standalone construction uses the existing field-specific formatter");
		g_free (wire);
		g_free (expected);
		g_object_unref (header);
	}
	g_object_unref (headers);

	for (i = 0; i < G_N_ELEMENTS (whitespace); i++) {
		gboolean trim = whitespace[i][0] == ' ' || whitespace[i][0] == '\t' ||
			whitespace[i][0] == '\r' || whitespace[i][0] == '\n';

		text = g_strconcat (whitespace[i], "one\r\n\ttwo", whitespace[i], NULL);
		expected = g_strconcat (trim ? "" : whitespace[i], "one\ttwo", trim ? "" : whitespace[i], NULL);
		header = g_mime_header_new ("X-Test", text, NULL);
		check (!strcmp (g_mime_header_get_value (header), expected),
		       "standalone construction trims only linear whitespace");
		g_object_unref (header);
		g_free (expected);
		g_free (text);
		header = g_mime_header_new ("X-Test", whitespace[i], NULL);
		check (!strcmp (g_mime_header_get_value (header), trim ? "" : whitespace[i]),
		       "only linear-whitespace-only values construct an empty header");
		check (testsuite_header_raw_text (header) != NULL, "empty headers have a formatted transport value");
		g_object_unref (header);
	}

	header = g_mime_header_new (name, value, charset);
	name[0] = value[0] = charset[0] = '!';
	check (!strcmp (g_mime_header_get_name (header), "sUbJeCt") &&
	       !strcmp (g_mime_header_get_value (header), "caf\303\251") &&
	       !strcmp (header->charset, "iso-8859-1"), "standalone headers copy caller-owned input strings");
	other = g_mime_header_new ("X-Other", "other", NULL);
	check (header->options != other->options, "standalone headers own independent default parser options");
	check (g_mime_parser_options_set_charset_encoding (header->options, "windows-1251", NULL),
	       "customize standalone header parser options");
	check (!strcmp (g_mime_parser_options_get_charset_encoding (other->options), "utf-8"),
	       "customizing one standalone header does not change another header's options");
	clone = g_mime_header_clone (header);
	g_object_unref (header);
	testsuite_header_set_raw_text (clone, " \317\360\350\342\345\362\r\n");
	check (!strcmp (g_mime_header_get_value (clone), "\320\237\321\200\320\270\320\262\320\265\321\202"),
	       "standalone clones retain parser options after their source is destroyed");
	g_mime_header_set_value (other, NULL, "changed", NULL);
	check (!strcmp (g_mime_header_get_value (other), "changed"), "standalone headers support value setters");
	g_object_unref (clone);
	g_object_unref (other);

	header = g_mime_header_new ("Subject", "caf\303\251", "utf-8");
	/* GLib can report FAILED on the first unknown-converter lookup and
	 * NO_CONVERSION afterward. Verify fallback bytes, not that warning. */
	other = g_mime_header_new ("Subject", "caf\303\251", "x-unknown-charset");
	check (!strcmp (testsuite_header_raw_text (header), testsuite_header_raw_text (other)),
	       "standalone construction falls back to UTF-8 for an unknown charset");
	g_object_unref (other);
	other = g_mime_header_new ("Subject", "caf\303\251", "us-ascii");
	check (!strcmp (testsuite_header_raw_text (header), testsuite_header_raw_text (other)),
	       "standalone construction falls back to UTF-8 for an unrepresentable charset");
	g_object_unref (other);
	g_object_unref (header);

	for (i = 0; i < G_N_ELEMENTS (invalid_names); i++) {
		g_test_expect_message ("gmime", G_LOG_LEVEL_CRITICAL, "*assertion*failed*");
		header = g_mime_header_new (invalid_names[i], "value", NULL);
		g_test_assert_expected_messages ();
		check (header == NULL, "standalone construction rejects invalid field names");
		g_clear_object (&header);
	}
	g_test_expect_message ("gmime", G_LOG_LEVEL_CRITICAL, "*name != NULL*");
	check (g_mime_header_new (NULL, "value", NULL) == NULL, "standalone construction rejects a NULL field name");
	g_test_assert_expected_messages ();
	g_test_expect_message ("gmime", G_LOG_LEVEL_CRITICAL, "*value != NULL*");
	check (g_mime_header_new ("Subject", NULL, NULL) == NULL, "standalone construction rejects a NULL value");
	g_test_assert_expected_messages ();
	g_test_expect_message ("gmime", G_LOG_LEVEL_CRITICAL, "*g_utf8_validate*");
	check (g_mime_header_new ("Subject", "\377", NULL) == NULL, "standalone construction rejects invalid UTF-8");
	g_test_assert_expected_messages ();
}

static void
test_header_identifiers (void)
{
	static const struct {
		GMimeHeaderId id;
		const char *name;
	} cases[] = {
		{ GMIME_HEADER_ACCEPT_LANGUAGE, "Accept-Language" },
		{ GMIME_HEADER_ARC_AUTHENTICATION_RESULTS, "ARC-Authentication-Results" },
		{ GMIME_HEADER_AUTO_SUBMITTED, "Auto-Submitted" },
		{ GMIME_HEADER_AUTOSUBMITTED, "Autosubmitted" },
		{ GMIME_HEADER_CONTENT_MD5, "Content-Md5" },
		{ GMIME_HEADER_DKIM_SIGNATURE, "DKIM-Signature" },
		{ GMIME_HEADER_DOMAIN_KEY_SIGNATURE, "DomainKey-Signature" },
		{ GMIME_HEADER_MIME_VERSION, "MIME-Version" },
		{ GMIME_HEADER_PREVENT_NON_DELIVERY_REPORT, "Prevent-NonDelivery-Report" },
		{ GMIME_HEADER_RECEIVED_SPF, "Received-SPF" },
		{ GMIME_HEADER_SUBJECT, "Subject" },
		{ GMIME_HEADER_TLS_REQUIRED, "TLS-Required" },
		{ GMIME_HEADER_X400_MTS_IDENTIFIER, "X400-MTS-Identifier" },
		{ GMIME_HEADER_X_MSMAIL_PRIORITY, "X-MSMail-Priority" },
		{ GMIME_HEADER_X_STATUS, "X-Status" }
	};
	static const char *unknown[] = {
		"", "Unknown", "X-Custom", "Subject ", " Subject", "Subject:", "Subject\t",
		"ContentType", "MIME_VERSION", "SUBJ\304\260CT", "Subject\377"
	};
	static const char binary_header[] = "sUbJeCt \t: before\0after\r\n";
	GMimeHeaderList *headers = g_mime_header_list_new (NULL), *loaded;
	GHashTable *names = g_hash_table_new (g_str_hash, g_str_equal);
	GString *wire = g_string_new (NULL);
	GMimeHeader *header, *clone;
	GMimeStream *stream;
	GByteArray *bytes;
	GError *error = NULL;
	const char *name;
	char *lower, *upper, *text;
	GBytes *raw;
	guint i;

	for (i = 0; i < G_N_ELEMENTS (cases); i++) {
		check (!strcmp (g_mime_header_id_to_name (cases[i].id), cases[i].name),
		       "canonical header names retain MimeKit's acronym spelling");
		check (g_mime_header_id_from_name (cases[i].name) == cases[i].id,
		       "canonical field names map to the expected enum constants");
	}

	for (i = 0; i <= GMIME_HEADER_X_STATUS; i++) {
		name = g_mime_header_id_to_name ((GMimeHeaderId) i);
		check (strcmp (name, "Unknown") && g_hash_table_add (names, (gpointer) name),
		       "each common header identifier has a unique canonical name");
		lower = g_ascii_strdown (name, -1);
		upper = g_ascii_strup (name, -1);
		check (g_mime_header_id_from_name (name) == (GMimeHeaderId) i &&
		       g_mime_header_id_from_name (lower) == (GMimeHeaderId) i &&
		       g_mime_header_id_from_name (upper) == (GMimeHeaderId) i,
		       "header name matching ignores ASCII case");
		text = g_strconcat (lower, " \t: value\r\n", NULL);
		header = g_mime_header_parse_text (NULL, text, &error);
		g_assert_no_error (error);
		g_assert_nonnull (header);
		check (g_mime_header_get_id (header) == (GMimeHeaderId) i && header->value == NULL,
		       "standalone parsing identifies headers without decoding their values");
		check (!strcmp (g_mime_header_get_name (header), lower),
		       "identifier lookup does not canonicalize parsed field spelling");
		g_mime_header_list_add (headers, header);
		clone = g_mime_header_clone (header);
		g_object_unref (header);
		check (g_mime_header_get_id (clone) == (GMimeHeaderId) i && clone->value == NULL,
		       "cloning preserves identifier and lazy decoding");
		raw = g_mime_header_get_raw_name (clone);
		check (g_bytes_get_size (raw) == strlen (lower) + 2 &&
		       !memcmp (g_bytes_get_data (raw, NULL), text, strlen (lower) + 2),
		       "identifying and cloning headers preserves trailing raw field blanks");
		g_bytes_unref (raw);
		g_object_unref (clone);
		g_string_append (wire, text);
		g_free (text);
		g_free (lower);
		g_free (upper);
	}
	g_string_append (wire, "\r\n");
	stream = g_mime_stream_mem_new_with_buffer (wire->str, wire->len);
	loaded = g_mime_header_list_load (NULL, stream, &error);
	g_assert_no_error (error);
	g_assert_nonnull (loaded);
	check (g_mime_header_list_get_count (loaded) == GMIME_HEADER_X_STATUS + 1,
	       "stream parsing retains every common header");
	for (i = 0; i <= GMIME_HEADER_X_STATUS; i++) {
		header = g_mime_header_list_get_header_at (loaded, i);
		check (g_mime_header_get_id (header) == (GMimeHeaderId) i && header->value == NULL,
		       "stream parsing uses the same identifiers without forcing value decoding");
		header = g_mime_header_list_get_header_at (headers, i);
		check (g_mime_header_get_id (header) == (GMimeHeaderId) i,
		       "adding an existing header preserves its identifier");
	}
	g_object_unref (loaded);
	g_object_unref (stream);
	g_string_free (wire, TRUE);
	g_hash_table_unref (names);
	g_object_unref (headers);

	for (i = 0; i < G_N_ELEMENTS (unknown); i++)
		check (g_mime_header_id_from_name (unknown[i]) == GMIME_HEADER_UNKNOWN,
		       "unrecognized or decorated field names have no common identifier");
	check (!strcmp (g_mime_header_id_to_name (GMIME_HEADER_UNKNOWN), "Unknown") &&
	       !strcmp (g_mime_header_id_to_name ((GMimeHeaderId) -2), "Unknown") &&
	       !strcmp (g_mime_header_id_to_name ((GMimeHeaderId) G_MININT), "Unknown") &&
	       !strcmp (g_mime_header_id_to_name ((GMimeHeaderId) G_MAXINT), "Unknown") &&
	       !strcmp (g_mime_header_id_to_name ((GMimeHeaderId) (GMIME_HEADER_X_STATUS + 1)), "Unknown"),
	       "unknown and out-of-range identifiers have MimeKit's fallback name");

	header = g_mime_header_new_full ("sUbJeCt", "value", NULL, &error);
	g_assert_no_error (error);
	check (g_mime_header_get_id (header) == GMIME_HEADER_SUBJECT, "checked construction identifies common headers");
	g_object_unref (header);
	header = g_mime_header_new ("X-Custom", "value", NULL);
	check (g_mime_header_get_id (header) == GMIME_HEADER_UNKNOWN, "extension headers retain an unknown identifier");
	g_object_unref (header);
	header = g_mime_header_new ("sUbJeCt", "value", NULL);
	check (g_mime_header_get_id (header) == GMIME_HEADER_SUBJECT, "standalone construction identifies common headers");
	g_mime_header_set_value (header, NULL, "changed", NULL);
	check (g_mime_header_get_id (header) == GMIME_HEADER_SUBJECT, "changing a value preserves the header identifier");
	g_object_unref (header);
	header = g_mime_header_parse (NULL, (const guint8 *) binary_header, sizeof (binary_header) - 1, &error);
	g_assert_no_error (error);
	clone = g_mime_header_clone (header);
	g_object_unref (header);
	check (g_mime_header_get_id (clone) == GMIME_HEADER_SUBJECT, "binary header clones retain their identifier");
	stream = g_mime_stream_mem_new ();
	check (g_mime_header_write_to_stream (clone, NULL, stream) >= 0, "serialize a binary header clone");
	bytes = g_mime_stream_mem_get_byte_array (GMIME_STREAM_MEM (stream));
	check (bytes->len == sizeof (binary_header) - 1 &&
	       !memcmp (bytes->data, binary_header, sizeof (binary_header) - 1),
	       "header identification preserves binary raw serialization");
	g_object_unref (stream);
	g_object_unref (clone);
	g_test_expect_message ("gmime", G_LOG_LEVEL_CRITICAL, "*name != NULL*");
	check (g_mime_header_id_from_name (NULL) == GMIME_HEADER_UNKNOWN, "NULL field names are programming errors");
	g_test_assert_expected_messages ();
	g_test_expect_message ("gmime", G_LOG_LEVEL_CRITICAL, "*GMIME_IS_HEADER*");
	check (g_mime_header_get_id (NULL) == GMIME_HEADER_UNKNOWN, "NULL headers are programming errors");
	g_test_assert_expected_messages ();
}

static void
test_header_parsing (void)
{
	static const struct {
		const char *input, *name, *raw_name, *raw_value, *decoded;
		gsize length, value_length;
	} cases[] = {
		{ "Content-Type: text/plain", "Content-Type", "Content-Type", " text/plain\n", "text/plain", 0, 0 },
		{ "Subject:", "Subject", "Subject", "\n", "", 0, 0 },
		{ ": value", "", "", " value\n", "value", 0, 0 },
		{ " \t: value", "", " \t", " value\n", "value", 0, 0 },
		{ "sUbJeCt \t: one\r\n\ttwo\r\n", "sUbJeCt", "sUbJeCt \t", " one\r\n\ttwo\r\n", "one\ttwo", 0, 0 },
		{ "Subject: value\r", "Subject", "Subject", " value\r\n", "value", 0, 0 },
		{ "X!: value\nOther: data\n", "X!", "X!", " value\nOther: data\n", "valueOther: data", 0, 0 },
		{ "Subject: =?utf-8?Q?caf=C3=A9?=", "Subject", "Subject", " =?utf-8?Q?caf=C3=A9?=\n", "caf\303\251", 0, 0 },
		{ "Subject: caf\303\251", "Subject", "Subject", " caf\303\251\n", "caf\303\251", 0, 0 },
		{ "Subject: before\0after", "Subject", "Subject", " before\0after\n", "before\357\277\275after",
		  sizeof ("Subject: before\0after") - 1, sizeof (" before\0after\n") - 1 },
		{ "Subject:\0\0\n", "Subject", "Subject", "\0\0\n", "\357\277\275\357\277\275",
		  sizeof ("Subject:\0\0\n") - 1, sizeof ("\0\0\n") - 1 }
	};
	static const char *invalid[] = {
		"", "Subject", "Subject \t", "Subject value: value", "\rSubject: v", "Subject\r: v"
	};
	static const char cyrillic[] = "Subject: \317\360\350\342\345\362";
	GMimeParserOptions *options;
	GMimeHeader *header, *clone;
	GMimeStream *stream;
	GByteArray *output;
	GBytes *name, *raw;
	GError *error = NULL;
	guint8 *span;
	gsize length, value_length;
	guint i, c, prefix;

	for (i = 0; i < G_N_ELEMENTS (cases); i++) {
		length = cases[i].length ? cases[i].length : strlen (cases[i].input);
		value_length = cases[i].value_length ? cases[i].value_length : strlen (cases[i].raw_value);
		span = g_memdup2 (cases[i].input, length);
		header = g_mime_header_parse (NULL, span, length, &error);
		g_assert_no_error (error);
		g_assert_nonnull (header);
		memset (span, 0xff, length);
		g_free (span);
		check (!strcmp (g_mime_header_get_name (header), cases[i].name),
		       "standalone parsing preserves field case and excludes trailing blanks from its name");
		check (header->owns_options && header->offset == -1 && !header->invalid &&
		       !header->reformat && !header->explicit_raw_value && header->value == NULL,
		       "standalone parsing owns its options and leaves decoded values lazy");
		name = g_mime_header_get_raw_name (header);
		raw = g_mime_header_get_raw_value (header);
		g_assert_cmpmem (g_bytes_get_data (name, NULL), g_bytes_get_size (name),
				 cases[i].raw_name, strlen (cases[i].raw_name));
		g_assert_cmpmem (g_bytes_get_data (raw, NULL), g_bytes_get_size (raw),
				 cases[i].raw_value, value_length);
		stream = g_mime_stream_mem_new ();
		check (g_mime_header_write_to_stream (header, NULL, stream) ==
		       (ssize_t) (strlen (cases[i].raw_name) + 1 + value_length),
		       "standalone parsed headers serialize their complete raw byte spans");
		output = g_mime_stream_mem_get_byte_array (GMIME_STREAM_MEM (stream));
		g_assert_cmpmem (output->data, strlen (cases[i].raw_name), cases[i].raw_name, strlen (cases[i].raw_name));
		g_assert_cmpint (output->data[strlen (cases[i].raw_name)], ==, ':');
		g_assert_cmpmem (output->data + strlen (cases[i].raw_name) + 1, value_length, cases[i].raw_value, value_length);
		g_object_unref (stream);
		clone = g_mime_header_clone (header);
		g_object_unref (header);
		check (!strcmp (g_mime_header_get_value (clone), cases[i].decoded),
		       "standalone parsed clones decode encoded words and safely replace decoded NULs");
		check (g_bytes_equal (clone->raw_name, name) && g_bytes_equal (clone->raw_value, raw),
		       "decoding does not change independently owned raw bytes");
		g_object_unref (clone);
		g_bytes_unref (name);
		g_bytes_unref (raw);
		if (!cases[i].length) {
			header = g_mime_header_parse_text (NULL, cases[i].input, &error);
			g_assert_no_error (error);
			g_assert_nonnull (header);
			check (!strcmp (g_mime_header_get_value (header), cases[i].decoded),
			       "standalone UTF-8 text parsing uses the bounded header parser");
			g_object_unref (header);
		}
	}

	for (i = 0; i < G_N_ELEMENTS (invalid); i++) {
		check (g_mime_header_parse_text (NULL, invalid[i], &error) == NULL &&
		       g_error_matches (error, GMIME_ERROR, GMIME_ERROR_PARSE_ERROR),
		       "standalone parsing reports a missing field separator as a syntax error");
		g_clear_error (&error);
		check (g_mime_header_parse_text (NULL, invalid[i], NULL) == NULL,
		       "standalone parsing can report failure without an error destination");
	}
	check (g_mime_header_parse (NULL, NULL, 0, &error) == NULL &&
	       g_error_matches (error, GMIME_ERROR, GMIME_ERROR_PARSE_ERROR),
	       "empty byte spans report a parse error without reading a NULL buffer");
	g_clear_error (&error);

	for (prefix = 0; prefix < 2; prefix++) {
		for (c = 0; c < 256; c++) {
			guint8 input[] = { 'X', c, ':', 'v' };
			gboolean valid = (c >= 32 && c < 127) || c == '\t';

			length = sizeof (input) - (prefix ? 0 : 1);
			span = g_memdup2 (input + (prefix ? 0 : 1), length);
			header = g_mime_header_parse (NULL, span, length, &error);
			check ((header != NULL) == valid,
			       "field text and trailing blanks use MimeKit's exact ASCII byte classes");
			if (valid) {
				g_assert_no_error (error);
				stream = g_mime_stream_mem_new ();
				g_assert_cmpint (g_mime_header_write_to_stream (header, NULL, stream), ==, length + 1);
				output = g_mime_stream_mem_get_byte_array (GMIME_STREAM_MEM (stream));
				g_assert_cmpmem (output->data, length, span, length);
				g_assert_cmpint (output->data[length], ==, '\n');
				g_object_unref (stream);
				g_object_unref (header);
			} else {
				g_assert_error (error, GMIME_ERROR, GMIME_ERROR_PARSE_ERROR);
				g_clear_error (&error);
			}
			g_free (span);
		}
	}

	options = g_mime_parser_options_new ();
	g_assert_true (g_mime_parser_options_set_charset_encoding (options, "windows-1251", &error));
	header = g_mime_header_parse (options, (const guint8 *) cyrillic, sizeof (cyrillic) - 1, &error);
	g_assert_no_error (error);
	g_assert_nonnull (header);
	g_assert_true (g_mime_parser_options_set_charset_encoding (options, "utf-8", &error));
	g_mime_parser_options_free (options);
	clone = g_mime_header_clone (header);
	g_object_unref (header);
	check (!strcmp (g_mime_parser_options_get_charset_encoding (clone->options), "windows-1251") &&
	       !strcmp (g_mime_header_get_value (clone), "\320\237\321\200\320\270\320\262\320\265\321\202"),
	       "standalone parsing snapshots options for lazy decoding after caller and source lifetimes");
	g_object_unref (clone);

	g_test_expect_message ("gmime", G_LOG_LEVEL_CRITICAL, "*buffer != NULL || length == 0*");
	check (g_mime_header_parse (NULL, NULL, 1, NULL) == NULL, "nonempty byte spans require a buffer");
	g_test_assert_expected_messages ();
	g_test_expect_message ("gmime", G_LOG_LEVEL_CRITICAL, "*text != NULL*");
	check (g_mime_header_parse_text (NULL, NULL, NULL) == NULL, "text parsing requires a string");
	g_test_assert_expected_messages ();
}

static void
count_checked_header_changes (gpointer sender, gpointer args, gpointer user_data)
{
	(*(guint *) user_data)++;
}

static char *
fail_checked_header_format (GMimeHeader *header, GMimeFormatOptions *options,
			    const char *value, const char *charset)
{
	errno = !strcmp (value, "errno") ? EIO : 0;
	return NULL;
}

static void
test_header_unfolding (void)
{
	static const struct {
		const char *input, *expected;
	} cases[] = {
		{ NULL, "" }, { "", "" }, { "\r\n \t", "" }, { "\r\n \t\v\f", "\v\f" },
		{ " \t\r\none\r\n\ttwo\nthree\rfour \t", "one\ttwothreefour" },
		{ "\302\240one\302\205two\342\200\203", "\302\240one\302\205two\342\200\203" },
		{ " \t\v\r\n ", "\v" },
		{ " \302\240 one \302\205\t", "\302\240 one \302\205" },
		{ "\357\273\277value\357\273\277", "\357\273\277value\357\273\277" },
		{ "\342\200\213value\342\200\213", "\342\200\213value\342\200\213" },
		{ "\034value\034", "\034value\034" },
		{ "\vone\vtwo\v", "\vone\vtwo\v" },
		{ " \360\237\230\200\r\nx ", "\360\237\230\200x" },
		{ " =?utf-8?Q?caf=C3=A9?= ", "=?utf-8?Q?caf=C3=A9?=" }
	};
	static const gunichar whitespace[] = {
		0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x20, 0x85, 0xa0, 0x1680,
		0x2000, 0x2001, 0x2002, 0x2003, 0x2004, 0x2005, 0x2006,
		0x2007, 0x2008, 0x2009, 0x200a, 0x2028, 0x2029, 0x202f,
		0x205f, 0x3000
	};
	static const char body[] = "one\302\240two\r\nthree";
	static const char unfolded_body[] = "one\302\240twothree";
	static const char binary[] = "Subject:\302\240before\0after\302\240\n";
	GMimeHeader *header, *clone;
	GError *error = NULL;
	GBytes *raw;
	GString *text, *expected;
	char *unfolded, *decoded, *wire;
	const char *cached;
	guint i, j, changes;

	for (i = 0; i < G_N_ELEMENTS (cases); i++) {
		unfolded = g_mime_header_unfold (cases[i].input);
		check (!strcmp (unfolded, cases[i].expected),
		       "header unfolding trims only linear whitespace, removes CR/LF and does not decode words");
		g_free (unfolded);
	}

	for (i = 0; i < G_N_ELEMENTS (whitespace); i++) {
		for (j = 0; j < G_N_ELEMENTS (whitespace); j++) {
			text = g_string_new (NULL);
			g_string_append_unichar (text, whitespace[i]);
			g_string_append (text, body);
			g_string_append_unichar (text, whitespace[j]);
			expected = g_string_new (NULL);
			if (whitespace[i] != ' ' && whitespace[i] != '\t' && whitespace[i] != '\r' && whitespace[i] != '\n')
				g_string_append_unichar (expected, whitespace[i]);
			g_string_append (expected, unfolded_body);
			if (whitespace[j] != ' ' && whitespace[j] != '\t' && whitespace[j] != '\r' && whitespace[j] != '\n')
				g_string_append_unichar (expected, whitespace[j]);
			unfolded = g_mime_header_unfold (text->str);
			check (!strcmp (unfolded, expected->str),
			       "all whitespace boundary pairs trim only linear whitespace without collapsing interior text");
			g_free (unfolded);
			wire = g_strconcat ("Subject:", text->str, NULL);
			header = g_mime_header_parse_text (NULL, wire, &error);
			g_assert_no_error (error);
			g_assert_nonnull (header);
			g_free (wire);
			raw = g_mime_header_get_raw_value (header);
			changes = 0;
			g_mime_event_add (header->changed, count_checked_header_changes, &changes);
			decoded = g_mime_header_get_value_with_charset (header, "utf-8", &error);
			g_assert_no_error (error);
			check (!strcmp (decoded, expected->str) && header->value == NULL,
			       "explicit decoded getters trim only linear whitespace without populating the cache");
			g_free (decoded);
			cached = g_mime_header_get_value (header);
			check (!strcmp (cached, expected->str) && cached == g_mime_header_get_value (header),
			       "ordinary decoded getters cache the same unfolding result");
			check (!changes && g_bytes_equal (header->raw_value, raw),
			       "header unfolding preserves original raw bytes and sends no change events");
			clone = g_mime_header_clone (header);
			g_object_unref (header);
			check (!strcmp (g_mime_header_get_value (clone), expected->str),
			       "cloning preserves unfolded Unicode values after the source is released");
			g_object_unref (clone);
			g_bytes_unref (raw);
			header = g_mime_header_new_full ("Subject", text->str, "utf-8", &error);
			g_assert_no_error (error);
			g_assert_nonnull (header);
			check (!strcmp (g_mime_header_get_value (header), expected->str),
			       "checked construction reuses linear-whitespace header unfolding");
			g_assert_true (g_mime_header_set_value_full (header, NULL, text->str, "utf-8", &error));
			g_assert_no_error (error);
			check (!strcmp (g_mime_header_get_value (header), expected->str),
			       "checked assignment reuses linear-whitespace header unfolding");
			g_object_unref (header);
			g_string_free (expected, TRUE);
			g_string_free (text, TRUE);
		}
	}

	header = g_mime_header_parse_text (NULL, "Subject: =?utf-8?Q?=C2=A0one=C2=A0?=\r\n", &error);
	g_assert_no_error (error);
	g_assert_nonnull (header);
	check (!strcmp (g_mime_header_get_value (header), "\302\240one\302\240"),
	       "Unicode whitespace is retained after RFC 2047 transfer and charset decoding");
	g_object_unref (header);
	header = g_mime_header_parse_text (NULL, "Subject: =?utf-8?Q?=C2=A0=E2=80=83?=\n", &error);
	g_assert_no_error (error);
	g_assert_nonnull (header);
	check (!strcmp (g_mime_header_get_value (header), "\302\240\342\200\203"),
	       "decoded Unicode whitespace-only values are retained");
	g_object_unref (header);
	header = g_mime_header_parse (NULL, (const guint8 *) binary, sizeof (binary) - 1, &error);
	g_assert_no_error (error);
	g_assert_nonnull (header);
	raw = g_mime_header_get_raw_value (header);
	check (!strcmp (g_mime_header_get_value (header), "\302\240before\357\277\275after\302\240") &&
	       g_bytes_equal (header->raw_value, raw),
	       "header unfolding follows NUL replacement without trimming Unicode or altering binary raw values");
	g_bytes_unref (raw);
	g_object_unref (header);

	wire = g_strdup (" \302\240mutable\r\nvalue\342\200\203");
	unfolded = g_mime_header_unfold (wire);
	memset (wire, 'x', strlen (wire));
	g_free (wire);
	check (!strcmp (unfolded, "\302\240mutablevalue\342\200\203"),
	       "public unfolding returns independently owned UTF-8 text without trimming Unicode");
	g_free (unfolded);

	unfolded = g_mime_utils_header_unfold ("\302\240legacy\302\240");
	check (!strcmp (unfolded, "\302\240legacy\302\240"),
	       "the legacy raw-byte unfolding utility retains its existing ASCII whitespace policy");
	g_free (unfolded);
	g_test_expect_message ("gmime", G_LOG_LEVEL_CRITICAL, "*g_utf8_validate*");
	check (g_mime_header_unfold ("\377") == NULL, "decoded header unfolding requires valid UTF-8");
	g_test_assert_expected_messages ();
}

static void
test_checked_header_assignment (void)
{
	static const struct {
		const char *name, *value, *normalized, *raw;
	} cases[] = {
		{ "sUbJeCt", "\302\240 one\r\n\ttwo \342\200\203", "\302\240 one\ttwo \342\200\203",
		  " \302\240 one\ttwo \342\200\203\n" },
		{ "X-Empty", " \t\r\n", "", " \n" },
		{ "X-Unicode", "\342\200\205\r\n", "\342\200\205", " \342\200\205\n" },
		{ "Content-Type", " text/plain; charset=utf-8 ", "text/plain; charset=utf-8",
		  " text/plain; charset=utf-8\n" },
		{ "Content-Disposition", " attachment; filename=\"file.txt\" ",
		  "attachment; filename=\"file.txt\"", " attachment; filename=file.txt\n" }
	};
	static const struct {
		const char *name, *value;
	} invalid[] = {
		{ "Content-Type", "" }, { "Content-Type", "plain" },
		{ "Content-Type", "text/" }, { "Content-Type", "text/plain garbage" },
		{ "Content-Type", "text/plain; name=\"unfinished" },
		{ "Content-Disposition", "" }, { "Content-Disposition", "\"attachment\"" },
		{ "Content-Disposition", "attachment garbage" },
		{ "Content-Disposition", "attachment; filename=\"unfinished" }
	};
	static const char *bad_names[] = { "", "X Test", "X:Test", "X\177Test", "X-\303\251" };
	static const char binary[] = "Subject: one\0two\r\n\r\n";
	GMimeFormatOptions *options = g_mime_format_options_new ();
	GMimeHeaderRawValueFormatter formatter;
	GMimeHeader *header, *copy;
	GMimeHeaderList *headers;
	GMimePart *part;
	GMimeStream *source;
	GError *error = NULL;
	GBytes *raw_bytes, *raw;
	const char *value, *charset;
	char *wire, *expected;
	guint i, changes;

	g_mime_format_options_set_newline_format (options, GMIME_NEWLINE_FORMAT_UNIX);
	g_mime_format_options_set_allow_international (options, TRUE);
	for (i = 0; i < G_N_ELEMENTS (cases); i++) {
		header = g_mime_header_new_full (cases[i].name, cases[i].value, NULL, &error);
		g_assert_no_error (error);
		g_assert_nonnull (header);
		check (!strcmp (g_mime_header_get_name (header), cases[i].name) &&
		       !strcmp (g_mime_header_get_value (header), cases[i].normalized),
		       "checked header construction preserves names and normalizes values");
		check (header->owns_options && header->offset == -1, "checked headers own options and have no offset");
		check (g_mime_header_set_value_full (header, options, cases[i].value, NULL, &error),
		       "checked header assignment succeeds");
		g_assert_no_error (error);
		check (!strcmp (testsuite_header_raw_text (header), cases[i].raw),
		       "checked header assignment caches the requested formatting");
		check (!header->reformat && !header->explicit_raw_value,
		       "checked header assignment is cached but is not an explicit raw assignment");
		copy = g_mime_header_clone (header);
		g_object_unref (header);
		check (!strcmp (g_mime_header_get_value (copy), cases[i].normalized) &&
		       !strcmp (testsuite_header_raw_text (copy), cases[i].raw),
		       "checked assignments survive independent cloning");
		g_object_unref (copy);
	}
	g_mime_format_options_set_allow_international (options, FALSE);

	for (i = 0; i < G_N_ELEMENTS (invalid); i++) {
		header = g_mime_header_new_full (invalid[i].name, invalid[i].value, NULL, &error);
		check (header == NULL && g_error_matches (error, GMIME_ERROR, GMIME_ERROR_PARSE_ERROR),
		       "checked construction rejects malformed structured headers");
		g_clear_error (&error);
		header = g_mime_header_new (invalid[i].name,
					   !strcmp (invalid[i].name, "Content-Type") ? "text/plain" : "attachment", NULL);
		testsuite_header_set_raw_text (header, " explicit raw\r\n");
		value = g_mime_header_get_value (header);
		raw = header->raw_value;
		charset = header->charset;
		changes = 0;
		g_mime_event_add (header->changed, count_checked_header_changes, &changes);
		check (!g_mime_header_set_value_full (header, options, invalid[i].value, NULL, &error) &&
		       g_error_matches (error, GMIME_ERROR, GMIME_ERROR_PARSE_ERROR),
		       "checked assignment reports malformed structured headers");
		check (header->value == value && header->raw_value == raw && header->charset == charset &&
		       header->explicit_raw_value && !header->reformat && changes == 0,
		       "failed checked assignment retains all existing state and emits no event");
		g_clear_error (&error);
		check (!g_mime_header_set_value_full (header, options, invalid[i].value, NULL, NULL),
		       "checked parse failures also work without an error destination");
		g_object_unref (header);
	}

	header = g_mime_header_new_full ("Subject", "original", NULL, &error);
	g_assert_nonnull (header);
	changes = 0;
	g_mime_event_add (header->changed, count_checked_header_changes, &changes);
	value = header->value;
	raw = header->raw_value;
	charset = header->charset;
	check (!g_mime_header_set_value_full (header, options, "\377", NULL, &error) &&
	       g_error_matches (error, G_CONVERT_ERROR, G_CONVERT_ERROR_ILLEGAL_SEQUENCE),
	       "checked assignment rejects malformed UTF-8 with an error");
	g_clear_error (&error);
	check (!g_mime_header_set_value_full (header, options, "changed", "x-no-such-charset", &error) &&
	       error != NULL && error->domain == G_CONVERT_ERROR, "checked assignment rejects unsupported charsets");
	g_clear_error (&error);
	check (!g_mime_header_set_value_full (header, options, "changed", "", &error) &&
	       g_error_matches (error, G_CONVERT_ERROR, G_CONVERT_ERROR_NO_CONVERSION),
	       "checked assignment rejects empty charset names");
	g_clear_error (&error);
	check (header->value == value && header->raw_value == raw && header->charset == charset && changes == 0,
	       "encoding failures retain header state and emit no event");
	formatter = header->formatter;
	header->formatter = fail_checked_header_format;
	check (!g_mime_header_set_value_full (header, options, "errno", NULL, &error) &&
	       g_error_matches (error, GMIME_ERROR, EIO), "checked assignment reports formatter errno");
	g_clear_error (&error);
	check (!g_mime_header_set_value_full (header, options, "no errno", NULL, &error) &&
	       g_error_matches (error, GMIME_ERROR, GMIME_ERROR_GENERAL),
	       "checked assignment reports a formatter failure even without errno");
	g_clear_error (&error);
	check (header->value == value && header->raw_value == raw && header->charset == charset && changes == 0,
	       "formatter failures retain header state and emit no event");
	header->formatter = formatter;
	check (g_mime_header_set_value_full (header, options, "caf\303\251", "latin1", &error) &&
	       changes == 1 && !strcmp (header->charset, "iso-8859-1"),
	       "successful checked assignment normalizes charset aliases and emits one event");
	g_assert_no_error (error);
	g_mime_format_options_set_allow_international (options, TRUE);
	wire = header_wire_string (header, options);
	check (!strcmp (wire, "Subject: caf\303\251\n"), "checked assignments support International reformatting");
	g_free (wire);
	g_mime_format_options_set_allow_international (options, FALSE);
	check (g_mime_header_set_value_full (header, options, header->value, header->charset, &error) && changes == 2,
	       "checked assignment accepts values and charsets borrowed from itself");
	g_assert_no_error (error);
	check (g_mime_header_set_value_full (header, options, "\346\227\245\346\234\254", "us-ascii", &error),
	       "supported charsets retain the existing lossless UTF-8 fallback");
	g_assert_no_error (error);
	check (strstr (testsuite_header_raw_text (header), "=?utf-8?") != NULL, "unrepresentable header text uses UTF-8");
	g_object_unref (header);

	source = g_mime_stream_mem_new_with_buffer (binary, sizeof (binary) - 1);
	headers = g_mime_header_list_load (NULL, source, &error);
	g_assert_no_error (error);
	g_assert_nonnull (headers);
	g_object_unref (source);
	header = g_mime_header_list_get_header_at (headers, 0);
	raw_bytes = header->raw_value;
	check (!g_mime_header_set_value_full (header, options, "changed", "x-no-such-charset", &error) &&
	       error != NULL && header->raw_value == raw_bytes && header->offset == 0,
	       "failed checked assignment retains binary raw bytes and parsed offsets");
	g_clear_error (&error);
	check (g_mime_header_set_value_full (header, options, "changed", NULL, &error) &&
	       header->raw_value != raw_bytes && header->raw_value != NULL && header->offset == 0,
	       "successful checked assignment replaces binary raw bytes without changing the offset");
	g_assert_no_error (error);
	wire = header_wire_string (header, options);
	check (!strcmp (wire, "Subject: changed\n"), "replaced binary headers serialize the new value");
	g_free (wire);
	g_object_unref (headers);

	part = g_mime_part_new ();
	headers = g_mime_entity_get_header_list (GMIME_ENTITY (part));
	header = g_mime_header_list_get_header (headers, "Content-Type");
	check (g_mime_header_set_value_full (header, options, "text/html; charset=utf-8", NULL, &error) &&
	       g_mime_content_type_is_type (g_mime_entity_get_content_type (GMIME_ENTITY (part)), "text", "html"),
	       "checked assignment updates the containing entity's structured properties");
	g_assert_no_error (error);
	check (!g_mime_header_set_value_full (header, options, "invalid", NULL, &error) &&
	       g_error_matches (error, GMIME_ERROR, GMIME_ERROR_PARSE_ERROR) &&
	       g_mime_content_type_is_type (g_mime_entity_get_content_type (GMIME_ENTITY (part)), "text", "html"),
	       "failed checked assignment preserves containing entity properties");
	g_clear_error (&error);
	g_object_unref (part);

	for (i = 0; i < G_N_ELEMENTS (bad_names); i++) {
		g_test_expect_message ("gmime", G_LOG_LEVEL_CRITICAL, "*assertion*failed*");
		check (g_mime_header_new_full (bad_names[i], "value", NULL, &error) == NULL && error == NULL,
		       "checked construction rejects invalid field names as programming errors");
		g_test_assert_expected_messages ();
	}
	check (g_mime_header_new_full ("Subject", "\377", NULL, &error) == NULL &&
	       g_error_matches (error, G_CONVERT_ERROR, G_CONVERT_ERROR_ILLEGAL_SEQUENCE),
	       "checked construction reports malformed UTF-8");
	g_clear_error (&error);
	check (g_mime_header_new_full ("Subject", "value", "x-no-such-charset", &error) == NULL &&
	       error != NULL && error->domain == G_CONVERT_ERROR, "checked construction reports unsupported charsets");
	g_clear_error (&error);
	g_test_expect_message ("gmime", G_LOG_LEVEL_CRITICAL, "*name != NULL*");
	check (g_mime_header_new_full (NULL, "value", NULL, &error) == NULL && error == NULL,
	       "checked construction requires a field name");
	g_test_assert_expected_messages ();
	g_test_expect_message ("gmime", G_LOG_LEVEL_CRITICAL, "*value != NULL*");
	check (g_mime_header_new_full ("Subject", NULL, NULL, &error) == NULL && error == NULL,
	       "checked construction requires a value");
	g_test_assert_expected_messages ();
	g_mime_format_options_set_newline_format (options, GMIME_NEWLINE_FORMAT_DOS);
	header = g_mime_header_new_full ("Subject", "value", NULL, &error);
	g_assert_nonnull (header);
	check (g_mime_header_set_value_full (header, options, "one\r\n\ttwo", NULL, &error) &&
	       !strcmp (testsuite_header_raw_text (header), " one\ttwo\r\n"), "checked assignment honors DOS newline formatting");
	g_assert_no_error (error);
	expected = g_strdup (testsuite_header_raw_text (header));
	wire = header_wire_string (header, NULL);
	check (g_str_has_suffix (wire, expected), "checked assignments cache their chosen formatting until reassigned");
	g_free (expected);
	g_free (wire);
	g_test_expect_message ("gmime", G_LOG_LEVEL_CRITICAL, "*value != NULL*");
	check (!g_mime_header_set_value_full (header, options, NULL, NULL, &error) && error == NULL,
	       "checked assignment requires a value");
	g_test_assert_expected_messages ();
	error = g_error_new_literal (GMIME_ERROR, GMIME_ERROR_GENERAL, "existing error");
	g_test_expect_message ("gmime", G_LOG_LEVEL_CRITICAL, "*error == NULL || *error == NULL*");
	check (!g_mime_header_set_value_full (header, options, "value", NULL, &error) &&
	       !strcmp (error->message, "existing error"), "checked assignment requires a clear error destination");
	g_test_assert_expected_messages ();
	g_clear_error (&error);
	g_object_unref (header);
	g_mime_format_options_free (options);
}

static void
checked_header_list_changed (gpointer sender, gpointer args, gpointer user_data)
{
	GMimeHeaderList *headers = sender;
	GMimeHeaderListChangedEventArgs *change = args;
	guint *count = user_data;
	GMimeHeader *header;
	guint i;

	(*count)++;
	check (headers->has_body_separator, "checked list notifications see the final body-separator state");
	check (change->header != NULL, "checked list changes identify the affected header");
	for (i = 0; i < headers->array->len; i++) {
		header = headers->array->pdata[i];
		if (g_mime_header_list_index_of (headers, header->name) == (int) i)
			check (g_mime_header_list_get_header (headers, header->name) == header,
			       "checked list notifications see the final first-name lookup");
	}
}

static gboolean
change_checked_header_list (GMimeHeaderList *headers, guint operation, int index, const char *name,
			    const char *value, const char *charset, GError **error)
{
	switch (operation) {
	case 0:
		return g_mime_header_list_append_full (headers, name, value, charset, error);
	case 1:
		return g_mime_header_list_prepend_full (headers, name, value, charset, error);
	case 2:
		return g_mime_header_list_insert_full (headers, index, name, value, charset, error);
	default:
		return g_mime_header_list_replace_value_full (headers, name, value, charset, error);
	}
}

static void
test_checked_header_list_values (void)
{
	static const char text[] = "X-Before: one\r\nSubject: first\r\nX-Middle: preserved\r\n\tfold\r\n"
		"sUbJeCt: second\r\nX-After: last\r\n";
	static const struct {
		const char *name, *value, *charset;
	} invalid[] = {
		{ "Content-Type", "invalid", NULL },
		{ "Content-Type", "text/plain; name=\"unfinished", NULL },
		{ "Content-Disposition", "\"attachment\"", NULL },
		{ "Subject", "\377", NULL },
		{ "Subject", "value", "" },
		{ "Subject", "value", "x-no-such-charset" }
	};
	GMimeHeaderList *headers, *shared;
	GMimeHeader *original[5], *header;
	GMimeMessage *message;
	GMimePart *part;
	GMimeStream *stream;
	GError *error = NULL;
	GBytes *raw[5];
	char *before, *after;
	guint operation, i, j, changes, shared_changes, empty;
	int index, next;

	for (operation = 0; operation < 4; operation++) {
		for (index = 0; index <= 5; index++) {
			stream = g_mime_stream_mem_new_with_buffer (text, sizeof (text) - 1);
			headers = g_mime_header_list_load (NULL, stream, &error);
			g_assert_no_error (error);
			g_assert_nonnull (headers);
			g_object_unref (stream);
			check (!headers->has_body_separator, "checked list success starts with an unterminated header block");
			for (i = 0; i < 5; i++) {
				original[i] = g_object_ref (g_mime_header_list_get_header_at (headers, i));
				raw[i] = original[i]->raw_value;
			}
			changes = 0;
			g_mime_event_add (headers->changed, checked_header_list_changed, &changes);
			check (change_checked_header_list (headers, operation, index, "sUbJeCt",
							  "\302\240 replacement\r\n\tvalue \342\200\203", NULL, &error),
			       "checked field/value list operation succeeds");
			g_assert_no_error (error);
			next = operation == 0 ? 5 : operation == 1 ? 0 : operation == 2 ? index : 1;
			header = g_mime_header_list_get_header_at (headers, next);
			check (!strcmp (header->name, "sUbJeCt") &&
			       !strcmp (header->value, "\302\240 replacement\tvalue \342\200\203"),
			       "checked list operations preserve field case and Unicode whitespace while unfolding values");
			check (header->owns_options && header->options != headers->options && header->offset == -1 &&
			       !header->reformat && !header->explicit_raw_value,
			       "checked list values own default options and retain checked-constructor formatting");
			check (changes == 1, "checked list success emits exactly one notification");
			check (g_mime_header_list_get_count (headers) == (operation == 3 ? 4 : 6),
			       "only checked replacement collapses duplicate fields");
			for (i = j = 0; i < 5; i++) {
				check (original[i]->raw_value == raw[i] && original[i]->offset >= 0,
				       "checked list changes leave retained originals and source offsets untouched");
				if (operation == 3 && (i == 1 || i == 3))
					continue;
				if ((int) j == next)
					j++;
				check (g_mime_header_list_get_header_at (headers, j++) == original[i],
				       "checked list changes preserve unrelated identity and relative order");
			}
			header = g_object_ref (header);
			g_object_unref (headers);
			check (g_mime_header_set_value_full (header, NULL, header->value, header->charset, &error),
			       "checked list-created headers remain usable after the list is destroyed");
			g_assert_no_error (error);
			g_object_unref (header);
			for (i = 0; i < 5; i++)
				g_object_unref (original[i]);
		}
		for (empty = 0; empty < 2; empty++) {
			stream = g_mime_stream_mem_new_with_buffer (text, empty ? 0 : sizeof (text) - 1);
			headers = g_mime_header_list_load (NULL, stream, &error);
			g_assert_no_error (error);
			g_assert_nonnull (headers);
			g_object_unref (stream);
			changes = 0;
			g_mime_event_add (headers->changed, checked_header_list_changed, &changes);
			before = g_mime_header_list_to_string (headers, NULL);
			for (i = 0; i < G_N_ELEMENTS (invalid); i++) {
				header = g_mime_header_list_get_header (headers, "Subject");
				check (!change_checked_header_list (headers, operation, 0, invalid[i].name,
								   invalid[i].value, invalid[i].charset, &error) && error != NULL,
				       "checked list failures report an error");
				check (error->domain == (i < 3 ? GMIME_ERROR : G_CONVERT_ERROR),
				       "checked list operations propagate constructor error domains");
				g_clear_error (&error);
				check (!change_checked_header_list (headers, operation, 0, invalid[i].name,
								   invalid[i].value, invalid[i].charset, NULL),
				       "checked list failures work without an error destination");
				after = g_mime_header_list_to_string (headers, NULL);
				check (!strcmp (before, after) && changes == 0 && !headers->has_body_separator &&
				       g_mime_header_list_get_header (headers, "Subject") == header &&
				       g_mime_header_list_get_count (headers) == (empty ? 0 : 5),
				       "checked failure preserves raw output, lookup, count, separator state and notifications");
				g_free (after);
			}
			g_test_expect_message ("gmime", G_LOG_LEVEL_CRITICAL, "*value != NULL*");
			check (!change_checked_header_list (headers, operation, 0, "Subject", NULL, NULL, &error),
			       "checked list operations reject NULL values rather than creating placeholders");
			g_test_assert_expected_messages ();
			g_test_expect_message ("gmime", G_LOG_LEVEL_CRITICAL, "*name != NULL*");
			check (!change_checked_header_list (headers, operation, 0, NULL, "value", NULL, &error),
			       "checked list operations require field names");
			g_test_assert_expected_messages ();
			g_test_expect_message ("gmime", G_LOG_LEVEL_CRITICAL, "*assertion*failed*");
			check (!change_checked_header_list (headers, operation, 0, "Bad Name", "value", NULL, &error),
			       "checked list operations reject invalid field names");
			g_test_assert_expected_messages ();
			error = g_error_new_literal (GMIME_ERROR, GMIME_ERROR_GENERAL, "existing error");
			g_test_expect_message ("gmime", G_LOG_LEVEL_CRITICAL, "*error == NULL || *error == NULL*");
			check (!change_checked_header_list (headers, operation, 0, "Subject", "value", NULL, &error) &&
			       !strcmp (error->message, "existing error"), "checked list operations require a clear error destination");
			g_test_assert_expected_messages ();
			g_clear_error (&error);
			check (changes == 0 && !headers->has_body_separator, "invalid checked arguments do not notify or add a separator");
			g_free (before);
			g_object_unref (headers);
		}
	}

	headers = g_mime_header_list_new (NULL);
	g_test_expect_message ("gmime", G_LOG_LEVEL_CRITICAL, "*index >= 0*");
	check (!g_mime_header_list_insert_full (headers, -1, "Subject", "value", NULL, &error),
	       "checked insertion rejects negative indexes");
	g_test_assert_expected_messages ();
	g_test_expect_message ("gmime", G_LOG_LEVEL_CRITICAL, "*index*headers->array->len*");
	check (!g_mime_header_list_insert_full (headers, 1, "Subject", "value", NULL, &error),
	       "checked insertion rejects indexes beyond the count");
	g_test_assert_expected_messages ();
	for (operation = 0; operation < 4; operation++)
		check (change_checked_header_list (headers, operation, 0, "X-Empty", "", NULL, &error),
		       "checked list operations accept empty values and absent-name replacement");
	g_assert_no_error (error);
	check (g_mime_header_list_get_count (headers) == 1, "checked replacement collapses empty-valued duplicates");
	header = g_object_ref (g_mime_header_list_get_header_at (headers, 0));
	g_mime_header_list_add (headers, header);
	shared = g_mime_header_list_new (NULL);
	g_mime_header_list_add (shared, header);
	changes = shared_changes = 0;
	g_mime_event_add (headers->changed, checked_header_list_changed, &changes);
	g_mime_event_add (shared->changed, checked_header_list_changed, &shared_changes);
	check (g_mime_header_list_replace_value_full (headers, header->name, header->value, header->charset, &error) &&
	       changes == 1 && shared_changes == 0 && g_mime_header_list_get_count (headers) == 1 &&
	       g_mime_header_list_get_header_at (headers, 0) != header,
	       "checked replacement safely accepts borrowed arguments and replaces repeated instances");
	g_assert_no_error (error);
	check (g_mime_header_set_value_full (header, NULL, "shared-only", NULL, &error) &&
	       changes == 1 && shared_changes == 1,
	       "checked replacement detaches removed listeners only from the receiving list");
	g_assert_no_error (error);
	g_object_unref (header);
	g_object_unref (headers);
	g_object_unref (shared);

	message = g_mime_message_new (FALSE);
	headers = g_mime_entity_get_header_list (GMIME_ENTITY (message));
	check (g_mime_header_list_append_full (headers, "Subject", "original", NULL, &error) &&
	       !g_strcmp0 (g_mime_message_get_subject (message), "original"), "checked append updates the owning message");
	check (g_mime_header_list_replace_value_full (headers, "sUbJeCt", "replacement", NULL, &error) &&
	       !g_strcmp0 (g_mime_message_get_subject (message), "replacement"), "checked replacement updates the owning message");
	check (g_mime_header_list_append_full (headers, "To", "one@example.com", NULL, &error) &&
	       g_mime_header_list_prepend_full (headers, "To", "two@example.com", NULL, &error) &&
	       g_mime_header_list_insert_full (headers, 1, "To", "three@example.com", NULL, &error) &&
	       g_mime_internet_address_list_length (g_mime_message_get_to (message)) == 3,
	       "checked insertion operations update the recipient model exactly once");
	check (g_mime_header_list_replace_value_full (headers, "To", "new@example.com", NULL, &error) &&
	       g_mime_internet_address_list_length (g_mime_message_get_to (message)) == 1,
	       "checked replacement collapses the recipient model without double-appending");
	g_assert_no_error (error);
	g_object_unref (message);
	part = g_mime_part_new ();
	headers = g_mime_entity_get_header_list (GMIME_ENTITY (part));
	check (!g_mime_header_list_replace_value_full (headers, "Content-Type", "invalid", NULL, &error) &&
	       g_mime_content_type_is_type (g_mime_entity_get_content_type (GMIME_ENTITY (part)), "application", "octet-stream"),
	       "checked replacement failures preserve the owning content model");
	g_clear_error (&error);
	check (g_mime_header_list_replace_value_full (headers, "Content-Type", "text/html; charset=utf-8", NULL, &error) &&
	       g_mime_content_type_is_type (g_mime_entity_get_content_type (GMIME_ENTITY (part)), "text", "html"),
	       "checked replacement updates the owning content model");
	g_assert_no_error (error);
	g_object_unref (part);
}

static void
test_named_header_list_values (void)
{
	static const char text[] = "sUbJeCt \t: =?iso-8859-1?q?caf=E9?=\r\n\tfirst\r\n"
		"Subject: later\r\nX-Keep: untouched\r\n";
	static const char invalid_field[] = "X-First: valid\r\nbroken line\r\n";
	static const struct {
		const char *name, *initial, *value, *charset;
	} invalid[] = {
		{ "Content-Type", "text/plain", "invalid", NULL },
		{ "Content-Type", "text/plain", "text/plain; name=\"unfinished", NULL },
		{ "Content-Disposition", "attachment", "\"attachment\"", NULL },
		{ "Subject", "first", "\377", NULL },
		{ "Subject", "first", "new", "" },
		{ "Subject", "first", "new", "x-no-such-charset" }
	};
	GMimeHeaderList *headers, *shared;
	GMimeHeader *header, *later, *keep;
	GMimeHeaderRawValueFormatter formatter;
	GMimeParserOptions *options;
	GMimeMessage *message, *other;
	GMimePart *part;
	GMimeStream *stream;
	GError *error = NULL;
	GBytes *raw_field, *raw_value;
	const char *value, *charset;
	char *before, *after, *source;
	guint i, changes, shared_changes, missing;
	gint64 offset;

	headers = g_mime_header_list_new (NULL);
	check (g_mime_header_list_get_value (headers, "Subject") == NULL &&
	       g_mime_header_list_get_value (headers, "") == NULL, "named value lookup returns NULL for missing fields");
	g_mime_header_list_append (headers, "Subject", NULL, NULL);
	header = g_mime_header_list_get_header_at (headers, 0);
	g_mime_header_list_append (headers, "SUBJECT", "later", NULL);
	check (g_mime_header_list_get_value (headers, "subject") == NULL,
	       "named value lookup does not skip an unset first occurrence");
	check (g_mime_header_list_set_value_full (headers, "sUbJeCt", "", NULL, &error) &&
	       g_mime_header_list_get_header_at (headers, 0) == header &&
	       g_mime_header_list_get_count (headers) == 2 &&
	       !g_strcmp0 (g_mime_header_list_get_value (headers, "SUBJECT"), ""),
	       "named assignment fills an existing placeholder without removing duplicates");
	g_assert_no_error (error);
	g_object_unref (headers);

	stream = g_mime_stream_mem_new_with_buffer (text, sizeof (text) - 1);
	headers = g_mime_header_list_load (NULL, stream, &error);
	g_assert_no_error (error);
	g_assert_nonnull (headers);
	g_object_unref (stream);
	header = g_mime_header_list_get_header_at (headers, 0);
	later = g_mime_header_list_get_header_at (headers, 1);
	keep = g_mime_header_list_get_header_at (headers, 2);
	check (header->value == NULL, "named lookup starts with a lazy decoded cache");
	raw_field = header->raw_name;
	raw_value = header->raw_value;
	offset = header->offset;
	options = header->options;
	changes = 0;
	g_mime_event_add (headers->changed, count_checked_header_changes, &changes);
	value = g_mime_header_list_get_value (headers, "SUBJECT");
	check (!g_strcmp0 (value, "caf\303\251\tfirst") && value == header->value &&
	       g_mime_header_list_get_value (headers, "subject") == value,
	       "named lookup returns the first header's borrowed decoded cache");
	check (header->raw_name == raw_field && header->raw_value == raw_value &&
	       changes == 0 && !headers->has_body_separator,
	       "named lookup preserves raw bytes, separator state and notifications");
	check (g_mime_header_list_set_value_full (headers, later->name, "caf\303\251", "latin1", &error),
	       "named assignment accepts a lookup name borrowed from a later duplicate");
	g_assert_no_error (error);
	check (g_mime_header_list_get_header (headers, "subject") == header &&
	       g_mime_header_list_get_header_at (headers, 1) == later &&
	       g_mime_header_list_get_header_at (headers, 2) == keep &&
	       g_mime_header_list_get_count (headers) == 3 && !strcmp (g_mime_header_get_value (later), "later"),
	       "named assignment preserves every object, duplicate and list position");
	check (header->raw_name == raw_field && header->raw_value != raw_value &&
	       header->raw_value != NULL &&
	       header->offset == offset && header->options == options && !header->reformat &&
	       !header->explicit_raw_value && changes == 1 && !headers->has_body_separator,
	       "named assignment preserves source identity and separator state while caching the new formatting");
	check (g_mime_header_list_set_value_full (headers, header->name, header->value, header->charset, &error) &&
	       changes == 2 && !strcmp (header->charset, "iso-8859-1"),
	       "named assignment accepts its own borrowed value and charset");
	g_assert_no_error (error);
	formatter = header->formatter;
	raw_value = header->raw_value;
	value = header->value;
	charset = header->charset;
	header->formatter = fail_checked_header_format;
	check (!g_mime_header_list_set_value_full (headers, "Subject", "errno", NULL, &error) &&
	       g_error_matches (error, GMIME_ERROR, EIO) && header->raw_value == raw_value &&
	       header->value == value && header->charset == charset && changes == 2,
	       "named assignment propagates formatter failure without changing the first header or notifying");
	g_clear_error (&error);
	header->formatter = formatter;
	g_test_expect_message ("gmime", G_LOG_LEVEL_CRITICAL, "*name != NULL*");
	check (g_mime_header_list_get_value (headers, NULL) == NULL, "named lookup requires a field name");
	g_test_assert_expected_messages ();
	g_test_expect_message ("gmime", G_LOG_LEVEL_CRITICAL, "*value != NULL*");
	check (!g_mime_header_list_set_value_full (headers, "Subject", NULL, NULL, &error),
	       "named assignment requires a non-NULL value");
	g_test_assert_expected_messages ();
	g_test_expect_message ("gmime", G_LOG_LEVEL_CRITICAL, "*name != NULL*");
	check (!g_mime_header_list_set_value_full (headers, NULL, "new", NULL, &error),
	       "named assignment requires a field name");
	g_test_assert_expected_messages ();
	error = g_error_new_literal (GMIME_ERROR, GMIME_ERROR_GENERAL, "existing error");
	g_test_expect_message ("gmime", G_LOG_LEVEL_CRITICAL, "*error == NULL || *error == NULL*");
	check (!g_mime_header_list_set_value_full (headers, "Subject", "new", NULL, &error) &&
	       !strcmp (error->message, "existing error"), "named assignment requires a clear error destination");
	g_test_assert_expected_messages ();
	g_clear_error (&error);
	g_object_unref (headers);

	for (missing = 0; missing < 2; missing++) {
		for (i = 0; i < G_N_ELEMENTS (invalid); i++) {
			source = g_strdup_printf ("%s: %s\r\nX-Keep: untouched\r\n%s: %s\r\n",
						  missing ? "X-Other" : invalid[i].name, invalid[i].initial,
						  missing ? "X-Other" : invalid[i].name, invalid[i].initial);
			stream = g_mime_stream_mem_new_with_buffer (source, strlen (source));
			headers = g_mime_header_list_load (NULL, stream, &error);
			g_assert_no_error (error);
			g_assert_nonnull (headers);
			g_object_unref (stream);
			g_free (source);
			header = g_mime_header_list_get_header_at (headers, 0);
			raw_value = header->raw_value;
			changes = 0;
			g_mime_event_add (headers->changed, count_checked_header_changes, &changes);
			before = g_mime_header_list_to_string (headers, NULL);
			check (!g_mime_header_list_set_value_full (headers, invalid[i].name, invalid[i].value,
								  invalid[i].charset, &error) && error != NULL,
			       "named assignment reports invalid structured values and encodings");
			g_clear_error (&error);
			check (!g_mime_header_list_set_value_full (headers, invalid[i].name, invalid[i].value,
								  invalid[i].charset, NULL),
			       "named assignment can fail without an error destination");
			after = g_mime_header_list_to_string (headers, NULL);
			check (!strcmp (before, after) && g_mime_header_list_get_count (headers) == 3 &&
			       g_mime_header_list_get_header_at (headers, 0) == header && header->value == NULL &&
			       header->raw_value == raw_value && !headers->has_body_separator && changes == 0,
			       "failed named assignment preserves lazy/raw state, duplicates, order, separators and events");
			g_free (before);
			g_free (after);
			g_object_unref (headers);
		}
	}

	stream = g_mime_stream_mem_new_with_buffer (invalid_field, sizeof (invalid_field) - 1);
	headers = g_mime_header_list_load (NULL, stream, &error);
	g_assert_no_error (error);
	g_assert_nonnull (headers);
	g_object_unref (stream);
	header = g_mime_header_list_get_header_at (headers, 1);
	g_assert_nonnull (header);
	raw_field = header->raw_name;
	check (header->invalid && g_mime_header_list_set_value_full (headers, header->name, "new", NULL, &error) &&
	       header->invalid && header->raw_name == raw_field,
	       "named assignment updates an existing invalid parsed field without repairing its name");
	g_assert_no_error (error);
	g_test_expect_message ("gmime", G_LOG_LEVEL_CRITICAL, "*assertion*failed*");
	check (!g_mime_header_list_set_value_full (headers, "Another Bad Name", "new", NULL, &error),
	       "named assignment validates a newly appended field name");
	g_test_assert_expected_messages ();
	g_object_unref (headers);

	message = g_mime_message_new (FALSE);
	other = g_mime_message_new (FALSE);
	headers = g_mime_entity_get_header_list (GMIME_ENTITY (message));
	shared = g_mime_entity_get_header_list (GMIME_ENTITY (other));
	check (g_mime_header_list_set_value_full (headers, "Subject", "original", NULL, &error) &&
	       !g_strcmp0 (g_mime_message_get_subject (message), "original"),
	       "absent named assignment appends and updates the owning message");
	header = g_mime_header_list_get_header (headers, "Subject");
	g_mime_header_list_add (headers, header);
	g_mime_header_list_append (headers, "SUBJECT", "later", NULL);
	g_mime_header_list_add (shared, header);
	changes = shared_changes = 0;
	g_mime_event_add (headers->changed, count_checked_header_changes, &changes);
	g_mime_event_add (shared->changed, count_checked_header_changes, &shared_changes);
	check (g_mime_header_list_set_value_full (headers, "SUBJECT", "shared", NULL, &error) &&
	       !g_strcmp0 (g_mime_message_get_subject (message), "shared") &&
	       !g_strcmp0 (g_mime_message_get_subject (other), "shared") &&
	       g_mime_header_list_get_count (headers) == 3 && changes == 2 && shared_changes == 1,
	       "named assignment updates repeated/shared instances using their existing event subscriptions");
	check (!strcmp (g_mime_header_get_value (g_mime_header_list_get_header_at (headers, 2)), "later"),
	       "named assignment leaves distinct later duplicates unchanged");
	g_assert_no_error (error);
	g_object_unref (message);
	check (g_mime_header_list_set_value_full (shared, "Subject", "survivor", NULL, &error) &&
	       !g_strcmp0 (g_mime_message_get_subject (other), "survivor") &&
	       changes == 2 && shared_changes == 2,
	       "named assignment detaches destroyed lists while preserving surviving shared owners");
	g_assert_no_error (error);
	g_object_unref (other);
	message = g_mime_message_new (FALSE);
	headers = g_mime_entity_get_header_list (GMIME_ENTITY (message));
	g_mime_header_list_append (headers, "To", "one@example.com", NULL);
	g_mime_header_list_append (headers, "TO", "two@example.com", NULL);
	check (g_mime_header_list_set_value_full (headers, "to", "new@example.com", NULL, &error) &&
	       g_mime_internet_address_list_length (g_mime_message_get_to (message)) == 2,
	       "named address assignment rebuilds recipients without removing later fields");
	after = g_mime_internet_address_list_to_string (g_mime_message_get_to (message), NULL, FALSE);
	check (strstr (after, "new@example.com") != NULL && strstr (after, "two@example.com") != NULL &&
	       strstr (after, "one@example.com") == NULL,
	       "named address assignment refreshes the first recipient and retains the later recipient");
	g_free (after);
	g_assert_no_error (error);
	g_object_unref (message);
	part = g_mime_part_new ();
	headers = g_mime_entity_get_header_list (GMIME_ENTITY (part));
	check (g_mime_header_list_set_value_full (headers, "Content-Type", "text/html; charset=utf-8", NULL, &error) &&
	       g_mime_content_type_is_type (g_mime_entity_get_content_type (GMIME_ENTITY (part)), "text", "html"),
	       "named assignment updates the owning structured content model");
	g_assert_no_error (error);
	check (!g_mime_header_list_set_value_full (headers, "Content-Type", "invalid", NULL, &error) &&
	       g_mime_content_type_is_type (g_mime_entity_get_content_type (GMIME_ENTITY (part)), "text", "html"),
	       "failed named assignment preserves the owning structured content model");
	g_clear_error (&error);
	g_object_unref (part);
}

static void
test_header_charset_value (void)
{
	static const struct {
		const char *raw, *charset, *expected;
	} cases[] = {
		{ " caf\351\r\n", "windows-1252", "caf\303\251" },
		{ " caf\351\r\n", "windows-1251", "caf\320\271" },
		{ " \317\360\350\342\345\362\r\n", "windows-1251", "\320\237\321\200\320\270\320\262\320\265\321\202" },
		{ " \302\240 caf\303\251\r\n\tfin \342\200\203\r\n", "windows-1251",
		  "\302\240 caf\303\251\tfin \342\200\203" },
		{ " =?iso-8859-1?q?=85one=A0?=\r\n", "windows-1251", "\302\205one\302\240" },
		{ " =?iso-8859-1?q?caf=E9?=\r\n\tfin\r\n", "windows-1251", "caf\303\251\tfin" },
		{ " =?x-unknown?q?caf=E9?=\r\n", "windows-1251", "caf\320\271" },
		{ " caf\351\r\n", "utf-8", "caf\303\251" },
		{ " caf\351\r\n", "unicode", "caf\303\251" },
		{ " =?utf-16le?b?YwBhAGYA6QA=?=\r\n", "windows-1251", "caf\303\251" },
		{ " \t\r\n", "utf-8", "" },
		{ " =?utf-8?q?A=00B?=\r\n", "utf-8", "A\357\277\275B" },
		{ " =?iso-8859-1?q?A=00B?=\r\n", "utf-8", "A\357\277\275B" },
		{ " =?x-unknown?q?A=00B?=\r\n", "utf-8", "A\357\277\275B" },
		{ " =?utf-16le?b?QQAAAEIA?=\r\n", "utf-8", "A\357\277\275B" }
	};
	static const char nul_wire[] = "Subject: before\0after\r\n\r\n";
	GMimeParserOptions *options, *original_options;
	GMimeHeaderList *headers;
	GMimeHeader *header, *clone;
	GMimeStream *stream;
	GError *error = NULL;
	GBytes *raw_field, *raw_value;
	const char *cached, *charset;
	char *source, *value, *again, *before, *after;
	guint i, populated, changes;
	gint64 offset;

	options = g_mime_parser_options_new ();
	check (g_mime_parser_options_set_charset_encoding (options, "windows-1252", &error),
	       "explicit header decoding starts with a distinct configured fallback");
	g_assert_no_error (error);
	for (populated = 0; populated < 2; populated++) {
		for (i = 0; i < G_N_ELEMENTS (cases); i++) {
			source = g_strconcat ("sUbJeCt: ", cases[i].raw, NULL);
			stream = g_mime_stream_mem_new_with_buffer (source, strlen (source));
			headers = g_mime_header_list_load (options, stream, &error);
			g_assert_no_error (error);
			g_assert_nonnull (headers);
			g_object_unref (stream);
			g_free (source);
			header = g_mime_header_list_get_header_at (headers, 0);
			if (populated)
				g_mime_header_get_value (header);
			cached = header->value;
			charset = header->charset;
			raw_field = header->raw_name;
			raw_value = header->raw_value;
			offset = header->offset;
			original_options = header->options;
			before = g_mime_header_list_to_string (headers, NULL);
			changes = 0;
			g_mime_event_add (headers->changed, count_checked_header_changes, &changes);
			g_mime_event_add (header->changed, count_checked_header_changes, &changes);
			value = g_mime_header_get_value_with_charset (header, cases[i].charset, &error);
			g_assert_no_error (error);
			check (!g_strcmp0 (value, cases[i].expected),
			       "explicit header decoding applies charsets and NUL replacement while trimming only linear whitespace");
			again = g_mime_header_get_value_with_charset (header, cases[i].charset, &error);
			g_assert_no_error (error);
			check (again != value && again != cached && !g_strcmp0 (again, value),
			       "explicit header decoding returns an independent owned string on every call");
			g_free (again);
			g_free (value);
			after = g_mime_header_list_to_string (headers, NULL);
			check (header->value == cached && header->charset == charset &&
			       header->raw_name == raw_field && header->raw_value == raw_value &&
			       header->offset == offset && header->options == original_options &&
			       !strcmp (g_mime_parser_options_get_charset_encoding (header->options), "windows-1252") &&
			       !strcmp (before, after) && changes == 0 && !headers->has_body_separator,
			       "explicit header decoding preserves lazy/cached state, raw bytes, parser options and events");
			g_free (before);
			g_free (after);
			g_object_unref (headers);
		}
	}
	stream = g_mime_stream_mem_new_with_buffer (nul_wire, sizeof (nul_wire) - 1);
	headers = g_mime_header_list_load (options, stream, &error);
	g_assert_no_error (error);
	g_assert_nonnull (headers);
	g_object_unref (stream);
	header = g_mime_header_list_get_header_at (headers, 0);
	value = g_mime_header_get_value_with_charset (header, "utf-8", &error);
	g_assert_no_error (error);
	check (!g_strcmp0 (value, "before\357\277\275after") && header->value == NULL,
	       "explicit header decoding replaces literal NUL bytes without populating the cache");
	g_free (value);
	check (g_mime_header_get_value_with_charset (header, "x-no-such-charset", &error) == NULL &&
	       error && error->domain == G_CONVERT_ERROR && error->code != G_CONVERT_ERROR_EMBEDDED_NUL,
	       "explicit header decoding validates the charset before inspecting raw bytes");
	g_clear_error (&error);
	g_object_unref (headers);
	headers = g_mime_header_list_new (options);
	g_mime_header_list_append (headers, "Subject", NULL, NULL);
	header = g_mime_header_list_get_header_at (headers, 0);
	value = g_mime_header_get_value_with_charset (header, "utf-8", &error);
	check (value != NULL && *value == '\0' && header->value == NULL,
	       "explicit header decoding returns owned empty text for an unset legacy placeholder");
	g_free (value);
	for (i = 0; i < 3; i++) {
		const char *invalid = i == 0 ? "" : i == 1 ? "x-no-such-charset" : "x-unknown";
		check (g_mime_header_get_value_with_charset (header, invalid, &error) == NULL &&
		       error && error->domain == G_CONVERT_ERROR,
		       "explicit header decoding rejects empty or unsupported charsets even without a raw value");
		g_clear_error (&error);
		check (g_mime_header_get_value_with_charset (header, invalid, NULL) == NULL,
		       "explicit header decoding supports an omitted error destination");
	}
	g_test_expect_message ("gmime", G_LOG_LEVEL_CRITICAL, "*charset != NULL*");
	check (g_mime_header_get_value_with_charset (header, NULL, &error) == NULL,
	       "explicit header decoding requires a charset");
	g_test_assert_expected_messages ();
	error = g_error_new_literal (GMIME_ERROR, GMIME_ERROR_GENERAL, "existing error");
	g_test_expect_message ("gmime", G_LOG_LEVEL_CRITICAL, "*error == NULL || *error == NULL*");
	check (g_mime_header_get_value_with_charset (header, "utf-8", &error) == NULL &&
	       !strcmp (error->message, "existing error"), "explicit header decoding requires a clear error destination");
	g_test_assert_expected_messages ();
	g_clear_error (&error);
	testsuite_header_set_raw_text (header, " caf\351\r\n");
	clone = g_mime_header_clone (header);
	g_object_unref (headers);
	g_mime_parser_options_free (options);
	value = g_mime_header_get_value_with_charset (clone, "windows-1251", &error);
	check (!g_strcmp0 (value, "caf\320\271") && clone->value == NULL,
	       "explicit header decoding honors a clone's owned options after the source list is released");
	g_assert_no_error (error);
	g_free (value);
	g_object_unref (clone);
}

static void
test_header_raw_bytes (void)
{
	static const char source[] = "X-First: first\r\nsUbJeCt \t: initial\r\n\r\n";
	static const char binary[] = " before\0after\r\n";
	static const char encoded[] = " =?utf-8?q?caf=C3=A9?=\r\n";
	static const struct {
		const char *text, *decoded;
		gsize length;
	} cases[] = {
		{ "\n", "", 1 }, { "\r\n", "", 2 }, { "value\n", "value", 6 },
		{ " caf\351\r\n", "caf\303\251", 7 },
		{ binary, "before\357\277\275after", sizeof (binary) - 1 },
		{ "\0\n", "\357\277\275", 2 },
		{ " first\r\n\tsecond\n", "first\tsecond", 16 },
		{ "\0\0A\0\n", "\357\277\275\357\277\275A\357\277\275", 5 },
		{ " \303\251\0z\n", "\303\251\357\277\275z", 6 },
		{ " \351\0z\n", "\303\251\357\277\275z", 5 },
		{ " =?utf-8?q?A=00B?=\n", "A\357\277\275B", sizeof (" =?utf-8?q?A=00B?=\n") - 1 },
		{ " =?utf-8?b?QQBC?=\n", "A\357\277\275B", sizeof (" =?utf-8?b?QQBC?=\n") - 1 },
		{ " =?iso-8859-1?q?A=00B?=\n", "A\357\277\275B", sizeof (" =?iso-8859-1?q?A=00B?=\n") - 1 },
		{ " =?x-unknown?q?A=00B?=\n", "A\357\277\275B", sizeof (" =?x-unknown?q?A=00B?=\n") - 1 },
		{ " =?utf-16le?b?QQAAAEIA?=\n", "A\357\277\275B", sizeof (" =?utf-16le?b?QQAAAEIA?=\n") - 1 }
	};
	GMimeParserOptions *options = g_mime_parser_options_new (), *original_options;
	GMimeFormatOptions *format = g_mime_format_options_new ();
	GMimeHeaderList *headers, *shared;
	GMimeHeader *header, *clone;
	GMimeMessage *message;
	GMimeStream *stream;
	GBytes *bytes, *before, *retained, *result, *raw, *name;
	GError *error = NULL;
	GByteArray *output;
	const char *cached, *charset;
	gboolean reformat, explicit_raw;
	gint64 offset;
	guint parsed, populated, i, international, changes;
	unsigned char *span;
	char *display;

	span = g_memdup2 ("a\0\377", 3);
	check (!g_mime_utils_text_is_8bit (span, 0) && !g_mime_utils_text_is_8bit (span, 2) &&
	       g_mime_utils_text_is_8bit (span, 3),
	       "8-bit checks honor the full byte span, including bytes after NULs");
	span[2] = 'z';
	check (!g_mime_utils_text_is_8bit (span, 3), "8-bit checks do not read beyond nonterminated byte spans");
	g_free (span);

	for (parsed = 0; parsed < 2; parsed++) {
		for (populated = 0; populated < 2; populated++) {
			for (i = 0; i < G_N_ELEMENTS (cases); i++) {
				if (parsed) {
					stream = g_mime_stream_mem_new_with_buffer (source, sizeof (source) - 1);
					headers = g_mime_header_list_load (options, stream, &error);
					g_assert_no_error (error);
					g_object_unref (stream);
				} else {
					headers = g_mime_header_list_new (options);
					g_assert_true (g_mime_header_list_append_full (headers, "sUbJeCt", "initial", NULL, &error));
				}
				header = g_mime_header_list_get_header (headers, "Subject");
				if (populated)
					g_mime_header_get_value (header);
				raw = header->raw_value;
				cached = header->value;
				charset = header->charset;
				name = header->raw_name;
				original_options = header->options;
				offset = header->offset;
				reformat = header->reformat;
				explicit_raw = header->explicit_raw_value;
				changes = 0;
				g_mime_event_add (headers->changed, count_checked_header_changes, &changes);
				g_mime_event_add (header->changed, count_checked_header_changes, &changes);
				before = g_mime_header_get_raw_value (header);
				check (header->raw_value == raw && header->value == cached && !changes &&
				       header->reformat == reformat && header->explicit_raw_value == explicit_raw,
				       "raw byte lookup does not populate caches, reformat or notify owners");
				bytes = g_bytes_new_static (" missing newline", 16);
				check (!g_mime_header_set_raw_value (header, bytes, &error) &&
				       g_error_matches (error, GMIME_ERROR, GMIME_ERROR_PARSE_ERROR) &&
				       header->raw_value == raw && header->value == cached && !changes &&
				       header->reformat == reformat && header->explicit_raw_value == explicit_raw,
				       "raw byte assignment checks the final newline before changing header state");
				g_clear_error (&error);
				check (!g_mime_header_set_raw_value (header, bytes, NULL),
				       "raw byte assignment reports failure without an error destination");
				g_bytes_unref (bytes);
				bytes = g_bytes_new (cases[i].text, cases[i].length);
				check (g_mime_header_set_raw_value (header, bytes, &error),
				       "raw byte assignment accepts complete LF/CRLF values without encoding or folding");
				g_assert_no_error (error);
				result = g_mime_header_get_raw_value (header);
				check (g_bytes_equal (result, bytes) && header->value == NULL &&
				       header->raw_name == name &&
				       header->charset == charset && header->options == original_options &&
				       header->offset == offset && !header->reformat && header->explicit_raw_value && changes == 2,
				       "raw byte assignment retains all bytes, clears decoding and preserves metadata");
				g_bytes_unref (bytes);
				display = g_mime_header_get_value_with_charset (header, "utf-8", &error);
				g_assert_no_error (error);
				check (!g_strcmp0 (display, cases[i].decoded) && header->value == NULL,
				       "explicit header decoding replaces NULs without caching or truncating the value");
				g_free (display);
				check (!g_strcmp0 (g_mime_header_get_value (header), cases[i].decoded) &&
				       g_bytes_equal (header->raw_value, result) && changes == 2,
				       "ordinary header decoding replaces NULs without changing raw bytes or notifying owners");
				clone = g_mime_header_clone (header);
				for (international = 0; international < 2; international++) {
					g_mime_format_options_set_allow_international (format, international);
					stream = g_mime_stream_mem_new ();
					output = g_mime_stream_mem_get_byte_array (GMIME_STREAM_MEM (stream));
					check (g_mime_header_write_to_stream (header, format, stream) ==
					       (ssize_t) (g_bytes_get_size (name) + 1 + cases[i].length) &&
					       output->len == g_bytes_get_size (name) + 1 + cases[i].length &&
					       !memcmp (output->data, g_bytes_get_data (name, NULL), g_bytes_get_size (name)) &&
					       output->data[g_bytes_get_size (name)] == ':' &&
					       !memcmp (output->data + g_bytes_get_size (name) + 1, cases[i].text, cases[i].length),
					       "raw byte transport preserves embedded NULs and raw field names with International output");
					g_object_unref (stream);
				}
				check (g_mime_header_set_raw_value (header, header->raw_value, &error) && changes == 4,
				       "reassigning the current immutable bytes is alias-safe and still emits a change");
				check (g_mime_header_set_raw_value (header, result, &error) && changes == 6,
				       "reassigning a public raw-value snapshot retains all bytes and notifies owners");
				retained = g_mime_header_get_raw_value (header);
				check (g_bytes_equal (retained, result) && g_bytes_get_size (retained) == cases[i].length,
				       "raw-value access never truncates embedded NUL bytes");
				g_bytes_unref (retained);
				g_object_unref (headers);
				retained = g_mime_header_get_raw_value (clone);
				check (g_bytes_equal (result, retained) && g_bytes_get_size (before) > 0,
				       "raw byte snapshots and clones survive assignment and destruction of the source");
				g_bytes_unref (retained);
				g_bytes_unref (result);
				g_bytes_unref (before);
				g_object_unref (clone);
			}
		}
	}
	headers = g_mime_header_list_new (NULL);
	g_mime_header_list_append (headers, "Subject", NULL, NULL);
	header = g_mime_header_list_get_header_at (headers, 0);
	check (g_mime_header_get_raw_value (header) == NULL && header->value == NULL,
	       "unset legacy placeholders have no raw bytes");
	bytes = g_bytes_new_static ("", 0);
	check (!g_mime_header_set_raw_value (header, bytes, &error) &&
	       g_error_matches (error, GMIME_ERROR, GMIME_ERROR_PARSE_ERROR) && header->raw_value == NULL,
	       "empty raw byte assignment is rejected without changing an unset placeholder");
	g_clear_error (&error);
	g_bytes_unref (bytes);
	bytes = g_bytes_new_static ("\r", 1);
	check (!g_mime_header_set_raw_value (header, bytes, &error),
	       "a lone carriage return is not a valid raw-value terminator");
	g_clear_error (&error);
	g_bytes_unref (bytes);
	g_test_expect_message ("gmime", G_LOG_LEVEL_CRITICAL, "*raw_value != NULL*");
	check (!g_mime_header_set_raw_value (header, NULL, &error), "raw byte assignment requires non-NULL bytes");
	g_test_assert_expected_messages ();
	bytes = g_bytes_new_static (" valid\n", 7);
	error = g_error_new_literal (GMIME_ERROR, GMIME_ERROR_GENERAL, "existing error");
	g_test_expect_message ("gmime", G_LOG_LEVEL_CRITICAL, "*error == NULL || *error == NULL*");
	check (!g_mime_header_set_raw_value (header, bytes, &error) && header->raw_value == NULL &&
	       !strcmp (error->message, "existing error"), "raw byte assignment requires a clear error destination");
	g_test_assert_expected_messages ();
	g_clear_error (&error);
	g_bytes_unref (bytes);
	g_object_unref (headers);

	message = g_mime_message_new (FALSE);
	g_mime_message_set_subject (message, "original", NULL);
	headers = g_mime_entity_get_header_list (GMIME_ENTITY (message));
	header = g_mime_header_list_get_header (headers, "Subject");
	shared = g_mime_header_list_new (NULL);
	g_mime_header_list_add (shared, header);
	g_mime_header_list_add (shared, header);
	changes = 0;
	g_mime_event_add (shared->changed, count_checked_header_changes, &changes);
	bytes = g_bytes_new_static (encoded, sizeof (encoded) - 1);
	g_assert_true (g_mime_header_set_raw_value (header, bytes, &error));
	display = g_strdup (g_mime_message_get_subject (message));
	check (!g_strcmp0 (display, "caf\303\251") && changes == 2,
	       "raw byte assignment updates owning message caches and repeated-instance subscriptions");
	g_free (display);
	g_object_unref (message);
	g_assert_true (g_mime_header_set_raw_value (header, bytes, &error));
	check (changes == 4, "raw byte assignment preserves sharing owners after another owner is destroyed");
	g_bytes_unref (bytes);
	g_object_unref (shared);
	g_mime_format_options_free (format);
	g_mime_parser_options_free (options);
}

static void
test_header_cloning (void)
{
	static const char text[] = "X-First: first\r\nX-SuBjEcT \t: \317\360\350\342\345\362\r\n\tworld\r\n\r\n";
	static const char value[] = "\320\237\321\200\320\270\320\262\320\265\321\202\tworld";
	GMimeParserOptions *options = g_mime_parser_options_new ();
	GMimeHeader *header, *clone, *copy;
	GMimeHeaderList *headers;
	GMimeMessage *message;
	GMimeParser *parser;
	GMimeStream *stream;
	char *raw, *wire, *display;
	guint cached;

	check (g_mime_parser_options_set_charset_encoding (options, "windows-1251", NULL),
	       "set cloned header fallback charset");
	for (cached = 0; cached < 2; cached++) {
		stream = g_mime_stream_mem_new_with_buffer (text, strlen (text));
		parser = g_mime_parser_new_with_stream (stream);
		message = g_mime_parser_construct_message (parser, options, NULL);
		g_object_unref (parser);
		g_object_unref (stream);
		g_assert_nonnull (message);
		headers = g_mime_entity_get_header_list (GMIME_ENTITY (message));
		header = g_mime_header_list_get_header (headers, "X-Subject");
		g_assert_nonnull (header);
		check (g_mime_header_get_offset (header) == strlen ("X-First: first\r\n"),
		       "parsed header retains source offset");
		if (cached)
			check (!strcmp (g_mime_header_get_value (header), value), "cache parsed header value");
		check ((header->value != NULL) == cached, "exercise both lazy and cached header cloning");
		raw = g_strdup (testsuite_header_raw_text (header));
		clone = g_mime_header_clone (header);
		check (clone != header && (!header->value || clone->value != header->value),
		       "clone has independent header storage");
		check ((clone->value != NULL) == (header->value != NULL), "clone preserves decoded cache state");
		check (clone->options != header->options, "clone owns independent parser options");
		check (!strcmp (g_mime_header_get_name (clone), "X-SuBjEcT"), "clone preserves field-name case");
		check (!strcmp (testsuite_header_raw_name (clone), "X-SuBjEcT \t"), "clone preserves raw field whitespace");
		check (clone->raw_name == header->raw_name && clone->raw_value == header->raw_value,
		       "clone shares immutable raw bytes");
		check (g_mime_header_get_offset (clone) == -1, "cloned header has no source offset");
		check (!strcmp (testsuite_header_raw_text (clone), raw), "clone does not reformat parsed header");
		check (g_mime_parser_options_set_charset_encoding (header->options, "windows-1252", NULL),
		       "change source header parser options");
		check (!strcmp (g_mime_parser_options_get_charset_encoding (clone->options), "windows-1251"),
		       "clone retains parser options snapshot");

		copy = g_mime_header_clone (clone);
		g_mime_header_set_value (clone, NULL, "Changed", NULL);
		check (!strcmp (testsuite_header_raw_text (header), raw), "clone edits leave owning header untouched");
		g_object_unref (message);
		g_object_unref (clone);
		check (!strcmp (g_mime_header_get_value (copy), value), "clone decodes after all source owners are freed");
		wire = header_wire_string (copy, NULL);
		display = g_mime_header_to_string (copy);
		check (g_str_has_prefix (wire, "X-SuBjEcT \t: "), "clone writes original raw field name");
		check (!strcmp (testsuite_header_raw_text (copy), raw), "display and serialization preserve raw value");
		check (g_str_has_prefix (display, "X-SuBjEcT: ") && !strcmp (display + strlen ("X-SuBjEcT: "), value),
		       "header display uses decoded value without raw name whitespace or final newline");
		testsuite_header_set_raw_text (copy, " =?iso-8859-1?q?caf=E9?=\r\n");
		g_free (display);
		display = g_mime_header_to_string (copy);
		check (!strcmp (display, "X-SuBjEcT: caf\303\251"), "clone raw setter clears cached decoded value");
		g_free (display);
		g_free (wire);
		g_free (raw);
		g_object_unref (copy);
	}
	g_mime_parser_options_free (options);
}

static void
test_header_clone_formatting (void)
{
	static const struct {
		const char *name, *value, *charset;
	} cases[] = {
		{ "Subject", "This is a sufficiently long subject with caf\303\251 and several words for folding", "iso-8859-1" },
		{ "To", "Alice <alice@example.com>, Bob <bob@example.com>, Carol <carol@example.com>", NULL },
		{ "Content-Type", "text/plain; charset=utf-8; name=\"a long file name.txt\"", NULL },
		{ "Content-Disposition", "attachment; filename=\"a long file name.txt\"", NULL },
		{ "Received", "from example.com by localhost with SMTP; Fri, 21 Nov 1997 09:55:06 -0600", NULL },
		{ "X-Empty", "", NULL }
	};
	GMimeFormatOptions *options = g_mime_format_options_new ();
	GMimeHeaderList *headers;
	GMimeHeader *header, *clone;
	char *wire, *expected, *display;
	guint i, mode;

	for (i = 0; i < G_N_ELEMENTS (cases); i++) {
		headers = g_mime_header_list_new (NULL);
		g_mime_header_list_append (headers, cases[i].name, cases[i].value, cases[i].charset);
		header = g_mime_header_list_get_header_at (headers, 0);
		clone = g_mime_header_clone (header);
		check (clone->formatter == header->formatter && clone->reformat == header->reformat,
		       "clone preserves header-specific formatter and reformat policy");
		check (g_strcmp0 (clone->charset, header->charset) == 0, "clone preserves charset");
		check (clone->charset == NULL || clone->charset != header->charset, "clone copies charset storage");
		for (mode = 0; mode < 2; mode++) {
			g_mime_format_options_set_max_line_length (options, mode ? 120 : 60);
			g_mime_format_options_set_newline_format (options, mode ? GMIME_NEWLINE_FORMAT_DOS : GMIME_NEWLINE_FORMAT_UNIX);
			expected = header_wire_string (header, options);
			wire = header_wire_string (clone, options);
			check (!strcmp (wire, expected), "mutable clone retains formatting with supplied options");
			g_free (expected);
			g_free (wire);
		}
		g_mime_header_set_value (header, NULL, "replacement", NULL);
		check (!strcmp (g_mime_header_get_value (clone), cases[i].value), "source edits leave clone unchanged");
		g_object_unref (headers);
		display = g_mime_header_to_string (clone);
		expected = g_strdup_printf ("%s: %s", cases[i].name, cases[i].value);
		check (!strcmp (display, expected), "mutable clone displays cached decoded text after source disposal");
		g_free (expected);
		g_free (display);
		wire = header_wire_string (clone, options);
		check (g_str_has_prefix (wire, cases[i].name), "mutable clone writes after source options are freed");
		g_free (wire);
		g_object_unref (clone);
	}
	g_mime_format_options_free (options);

	headers = g_mime_header_list_new (NULL);
	g_mime_header_list_append (headers, "X-Placeholder", NULL, NULL);
	clone = g_mime_header_clone (g_mime_header_list_get_header_at (headers, 0));
	g_object_unref (headers);
	check (g_mime_header_get_value (clone) == NULL && testsuite_header_raw_text (clone) == NULL,
	       "clone preserves an unset placeholder without materializing a value");
	display = g_mime_header_to_string (clone);
	check (!strcmp (display, "X-Placeholder: "), "unset placeholder displays an empty value");
	g_free (display);
	wire = header_wire_string (clone, NULL);
	check (!*wire, "unset placeholder emits no transport header");
	g_free (wire);
	g_mime_header_set_value (clone, NULL, "filled", NULL);
	check (!strcmp (g_mime_header_get_value (clone), "filled"), "standalone placeholder clone can be filled");
	g_object_unref (clone);

	g_test_expect_message ("gmime", G_LOG_LEVEL_CRITICAL, "*GMIME_IS_HEADER*");
	check (g_mime_header_clone (NULL) == NULL, "header clone rejects NULL");
	g_test_assert_expected_messages ();
	g_test_expect_message ("gmime", G_LOG_LEVEL_CRITICAL, "*GMIME_IS_HEADER*");
	check (g_mime_header_to_string (NULL) == NULL, "header display rejects NULL");
	g_test_assert_expected_messages ();
}

static void
check_international_header (GMimeFormatOptions *options, const char *name, const char *value, const char *expected)
{
	GMimeHeaderList *headers = g_mime_header_list_new (NULL);
	GMimeHeader *header;
	char *wire, *transport;
	const char *raw;

	g_mime_header_list_append (headers, name, "initial", NULL);
	header = g_mime_header_list_get_header_at (headers, 0);
	g_mime_header_set_value (header, options, value, "utf-16be");
	check (g_utf8_validate (testsuite_header_raw_text (header), -1, NULL),
	       "International header contains valid UTF-8");
	raw = testsuite_header_raw_text (header);
	if (expected) {
		testsuite_check ("International header has expected folding");
		if (!strcmp (raw, expected))
			testsuite_check_passed ();
		else
			testsuite_check_failed ("width=%u value-length=%" G_GSIZE_FORMAT " actual=\"%.120s\", expected=\"%.120s\"",
					       g_mime_format_options_get_max_line_length (options), strlen (value), raw, expected);
	} else {
		char *decoded = g_mime_utils_header_decode_text (NULL, raw);
		char *unfolded = g_mime_utils_header_unfold (value);
		char *actual = g_mime_utils_header_unfold (decoded);
		char **words = g_strsplit_set (raw, " \t\r\n", -1);
		char **lines = g_strsplit (raw, g_mime_format_options_get_newline (options), -1);
		guint i, maxline = g_mime_format_options_get_max_line_length (options);

		check (!strcmp (actual, unfolded), "oversized International words round-trip without inserted whitespace");
		check (strstr (raw, "=?") != NULL, "oversized International words use lossless RFC 2047 fallback");
		for (i = 0; words[i]; i++)
			check (strlen (words[i]) <= MIN (75, maxline - 1), "fallback encoded words respect their size limit");
		for (i = 0; lines[i]; i++)
			check (strlen (lines[i]) <= maxline, "fallback folding respects the value line limit");
		g_strfreev (lines);
		g_strfreev (words);
		g_free (unfolded);
		g_free (decoded);
		g_free (actual);
	}
	transport = g_strdup_printf ("%s:%s", name, expected ? expected : raw);
	wire = header_wire_string (header, options);
	check (!strcmp (wire, transport), "International serialization uses requested folding and newline format");
	g_free (wire);
	g_free (transport);
	g_object_unref (headers);
}

static void
test_international_text_headers (void)
{
	static const guint widths[] = { 60, 78, 120, 998 };
	static const char *glyphs[] = { "a", "\303\251", "\346\227\245" };
	GMimeFormatOptions *options = g_mime_format_options_new (), *copy;
	GMimeHeaderList *headers;
	GMimeHeader *header;
	GMimeMessage *message;
	GMimeParser *parser;
	GMimeStream *stream;
	GString *value = g_string_new (NULL), *expected = g_string_new (NULL);
	const char *newline, *old_value, *old_raw;
	char *word, *prefix, *name, *raw, *wire;
	guint i, j, k, mode, width;

	g_mime_format_options_set_allow_international (options, TRUE);
	for (mode = 0; mode < 2; mode++) {
		g_mime_format_options_set_newline_format (options, mode ? GMIME_NEWLINE_FORMAT_DOS : GMIME_NEWLINE_FORMAT_UNIX);
		newline = g_mime_format_options_get_newline (options);
		g_string_printf (expected, " caf\303\251 \346\227\245 \360\237\230\200%s", newline);
		check_international_header (options, "Subject", "caf\303\251 \346\227\245 \360\237\230\200", expected->str);
		g_string_printf (expected, " =?utf-8?q?literal?=%s", newline);
		check_international_header (options, "Subject", "=?utf-8?q?literal?=", expected->str);
		g_string_printf (expected, " first\tsecond third%s", newline);
		check_international_header (options, "Subject", " first\r\n\tsecond\n third \r\n", expected->str);
		g_string_printf (expected, " %s", newline);
		check_international_header (options, "X-Empty", "", expected->str);

		for (i = 0; i < G_N_ELEMENTS (widths); i++) {
			width = widths[i];
			g_mime_format_options_set_max_line_length (options, width);
			for (j = 0; j < G_N_ELEMENTS (glyphs); j++) {
				g_string_truncate (value, 0);
				for (k = 0; k < width - 9; k++)
					g_string_append (value, glyphs[j]);
				g_string_printf (expected, " %s%s", value->str, newline);
				check_international_header (options, "Subject", value->str, expected->str);
				g_string_append (value, glyphs[j]);
				g_string_printf (expected, "%s %s%s", newline, value->str, newline);
				check_international_header (options, "Subject", value->str, expected->str);
				for (k = width - 8; k < width; k++)
					g_string_append (value, glyphs[j]);
				check_international_header (options, "Subject", value->str, NULL);
				g_string_append (value, glyphs[j]);
				check_international_header (options, "Subject", value->str, NULL);
			}

			word = g_strnfill (width - 15, 'x');
			g_string_printf (value, "prefix\t%s", word);
			g_string_printf (expected, " prefix%s\t%s%s", newline, word, newline);
			check_international_header (options, "Subject", value->str, expected->str);
			g_free (word);
			word = g_strnfill (width - 17, 'x');
			g_string_printf (value, "prefix \t %s", word);
			g_string_printf (expected, " prefix \t%s %s%s", newline, word, newline);
			check_international_header (options, "Subject", value->str, expected->str);
			g_free (word);

			prefix = g_strnfill (width - 10, 'a');
			word = g_strnfill (width + 2, 'b');
			g_string_printf (value, "%s\360\237\230\200%s", prefix, word);
			check_international_header (options, "Subject", value->str, NULL);
			g_free (prefix);
			g_free (word);

			word = g_strnfill (width * 2, ' ');
			g_string_printf (value, "first%stail", word);
			g_string_assign (expected, " first");
			g_string_append_len (expected, word, width - 14);
			g_string_append (expected, newline);
			g_string_append_len (expected, word, width);
			g_string_append (expected, newline);
			g_string_append_printf (expected, "              tail%s", newline);
			check_international_header (options, "Subject", value->str, expected->str);
			g_free (word);

			for (j = 0; j < 2; j++) {
				name = g_strnfill (j ? width + 10 : width - 2, 'X');
				word = g_strnfill (width + 1, 'x');
				check_international_header (options, name, word, NULL);
				g_free (name);
				g_free (word);
			}
		}
	}

	copy = g_mime_format_options_clone (options);
	check (g_mime_format_options_get_allow_international (copy), "cloned options preserve International formatting");
	headers = g_mime_header_list_new (NULL);
	g_mime_header_list_append (headers, "Subject", "original caf\303\251", "iso-8859-1");
	header = g_mime_header_list_get_header_at (headers, 0);
	raw = g_strdup (testsuite_header_raw_text (header));
	check (strstr (raw, "=?") != NULL, "ordinary header still uses RFC 2047 encoding");
	wire = header_wire_string (header, copy);
	check (strstr (wire, "caf\303\251") && !strstr (wire, "=?"), "International write bypasses stored RFC 2047 encoding");
	check (!strcmp (testsuite_header_raw_text (header), raw), "International write leaves stored raw value untouched");
	g_free (wire);
	g_free (raw);
	g_mime_header_set_value (header, copy, "original caf\303\251", header->charset);
	check (!strcmp (header->charset, "iso-8859-1"), "International setter preserves an aliased charset argument");

	old_value = g_mime_header_get_value (header);
	old_raw = testsuite_header_raw_text (header);
	g_test_expect_message ("gmime", G_LOG_LEVEL_CRITICAL, "*g_utf8_validate*");
	g_mime_header_set_value (header, copy, "\xff", "utf-8");
	g_test_assert_expected_messages ();
	check (g_mime_header_get_value (header) == old_value && testsuite_header_raw_text (header) == old_raw,
	       "invalid UTF-8 setter preserves the previous header state");
	check (!strcmp (header->charset, "iso-8859-1"), "invalid UTF-8 setter preserves charset");
	header->value[0] = (char) 0xff;
	stream = g_mime_stream_mem_new ();
	g_test_expect_message ("gmime", G_LOG_LEVEL_CRITICAL, "*g_utf8_validate*");
	check (g_mime_header_write_to_stream (header, copy, stream) == -1 && errno == EINVAL,
	       "invalid UTF-8 serialization reports failure");
	g_test_assert_expected_messages ();
	check (g_mime_stream_mem_get_byte_array (GMIME_STREAM_MEM (stream))->len == 0,
	       "failed International formatting writes no partial header");
	header->value[0] = 'o';
	g_object_unref (stream);
	testsuite_header_set_raw_text (header, " =?iso-8859-1?q?caf=E9?=\r\n\tuntouched\r\n");
	wire = header_wire_string (header, copy);
	check (!strcmp (wire, "Subject: =?iso-8859-1?q?caf=E9?=\r\n\tuntouched\r\n"),
	       "International options preserve explicitly assigned raw header text");
	g_free (wire);
	g_object_unref (headers);
	{
		static const char text[] = "Subject: =?iso-8859-1?q?caf=E9?=\r\n\tparsed\r\n\r\n";

		stream = g_mime_stream_mem_new_with_buffer (text, strlen (text));
		parser = g_mime_parser_new_with_stream (stream);
		message = g_mime_parser_construct_message (parser, NULL, NULL);
		g_object_unref (parser);
		g_object_unref (stream);
		g_assert_nonnull (message);
		headers = g_mime_entity_get_header_list (GMIME_ENTITY (message));
		header = g_mime_header_list_get_header (headers, "Subject");
		wire = header_wire_string (header, copy);
		check (!strcmp (wire, "Subject: caf\303\251\tparsed\r\n"),
		       "International options decode and reformat parsed text headers");
		g_free (wire);
		g_object_unref (message);
	}
	g_mime_format_options_free (copy);
	g_mime_format_options_free (options);
	g_string_free (value, TRUE);
	g_string_free (expected, TRUE);
}

static void
check_parsed_international_header (const char *input, const char *expected, GMimeParserOptions *parser_options)
{
	GMimeFormatOptions *format = g_mime_format_options_new ();
	GMimeHeaderList *headers;
	GMimeHeader *header, *clone;
	GMimeStream *stream;
	GBytes *bytes;
	GError *error = NULL;
	const char *stored;
	char *raw, *wire;
	gboolean terminated;
	int cached;

	g_mime_format_options_set_allow_international (format, TRUE);
	g_mime_format_options_set_newline_format (format, GMIME_NEWLINE_FORMAT_UNIX);
	for (cached = 0; cached < 2; cached++) {
		stream = g_mime_stream_mem_new_with_buffer (input, strlen (input));
		headers = g_mime_header_list_load (parser_options, stream, NULL);
		g_object_unref (stream);
		g_assert_nonnull (headers);
		header = g_mime_header_list_get_header_at (headers, 0);
		if (cached)
			g_mime_header_get_value (header);
		stored = testsuite_header_raw_text (header);
		raw = g_strdup (stored);
		wire = header_wire_string (header, format);
		check (!strcmp (wire, expected), "parsed International header has expected UTF-8 formatting");
		g_free (wire);
		check (testsuite_header_raw_text (header) == stored && !strcmp (stored, raw),
		       "International formatting preserves the stored raw bytes and pointer");
		wire = g_mime_header_list_to_string (headers, format);
		check (!strcmp (wire, expected), "header-list strings use parsed International formatting");
		g_free (wire);
		check_header_list_separator (headers, format, FALSE);
		clone = g_mime_header_clone (header);
		g_object_unref (headers);
		check (!clone->explicit_raw_value && g_mime_header_get_offset (clone) == -1,
		       "a parsed clone preserves reformat eligibility after its list is released");
		wire = header_wire_string (clone, format);
		check (!strcmp (wire, expected), "a detached parsed clone retains International formatting");
		g_free (wire);
		terminated = g_str_has_suffix (raw, "\n");
		if (terminated) {
			testsuite_header_set_raw_text (clone, raw);
		} else {
			bytes = g_bytes_new (raw, strlen (raw));
			check (!g_mime_header_set_raw_value (clone, bytes, &error) &&
			       g_error_matches (error, GMIME_ERROR, GMIME_ERROR_PARSE_ERROR),
			       "explicit raw assignment rejects an unterminated parsed value");
			g_clear_error (&error);
			g_bytes_unref (bytes);
		}
		header = g_mime_header_clone (clone);
		g_object_unref (clone);
		check (header->explicit_raw_value == terminated, "clones retain explicit raw assignment state");
		wire = header_wire_string (header, format);
		check (!strcmp (wire, terminated ? input : expected),
		       "only successful explicit raw assignments bypass International reformatting");
		g_free (wire);
		g_free (raw);
		g_object_unref (header);
	}
	g_mime_format_options_free (format);
}

static void
test_parsed_international_headers (void)
{
	static const struct {
		const char *input, *expected;
	} cases[] = {
		{ "Subject: =?utf-8?q?caf=C3=A9?=\n", "Subject: caf\303\251\n" },
		{ "Subject: =?utf-8?q?caf=C3=A9?=", "Subject: caf\303\251\n" },
		{ "X-Text \t: \t=?utf-8?q?caf=C3=A9?=\n", "X-Text \t: caf\303\251\n" },
		{ "X-Empty: \t\n", "X-Empty: \n" },
		{ "From: =?utf-8?q?Last=2C_First?= <user@example.com>\n",
		  "From: \"Last, First\" <user@example.com>\n" },
		{ "To: Group: =?utf-8?q?caf=C3=A9?= <user@example.com>;\n",
		  "To: Group: caf\303\251 <user@example.com>;\n" },
		{ "Reply-To: bad <\n", "Reply-To: bad <\n" },
		{ "To: \t\n", "To: \t\n" },
		{ "Content-Type: text/plain; name*=utf-8''caf%C3%A9.txt\n",
		  "Content-Type: text/plain; name=\"caf\303\251.txt\"\n" },
		{ "Content-Type: invalid\n", "Content-Type: invalid\n" },
		{ "Content-Disposition: attachment; filename*=utf-8''caf%C3%A9.txt\n",
		  "Content-Disposition: attachment; filename=\"caf\303\251.txt\"\n" },
		{ "Content-Disposition: \t\n", "Content-Disposition: \t\n" }
	};
	static const char *protected[] = {
		"Received", "Message-Id", "Original-Message-Id", "Resent-Message-Id",
		"Content-Id", "In-Reply-To", "References", "Disposition-Notification-Options",
		"Authentication-Results", "ARC-Authentication-Results", "DKIM-Signature",
		"ARC-Seal", "ARC-Message-Signature", "dKiM-sIgNaTuRe"
	};
	GMimeParserOptions *options = g_mime_parser_options_new ();
	GMimeFormatOptions *format = g_mime_format_options_new ();
	GMimeHeader *header;
	GMimePart *part;
	char *wire, *raw, *expected;
	guint i;

	for (i = 0; i < G_N_ELEMENTS (cases); i++)
		check_parsed_international_header (cases[i].input, cases[i].expected, NULL);
	for (i = 0; i < G_N_ELEMENTS (protected); i++) {
		wire = g_strdup_printf ("%s: \t=?utf-8?q?caf=C3=A9?=\n\tfolded\n", protected[i]);
		check_parsed_international_header (wire, wire, NULL);
		g_free (wire);
	}
	g_mime_parser_options_set_charset_encoding (options, "windows-1251", NULL);
	check_parsed_international_header ("Subject: \317\360\350\342\345\362\n",
					  "Subject: \320\237\321\200\320\270\320\262\320\265\321\202\n", options);
	g_mime_parser_options_free (options);

	g_mime_format_options_set_allow_international (format, TRUE);
	g_mime_format_options_set_newline_format (format, GMIME_NEWLINE_FORMAT_UNIX);
	header = g_mime_header_new ("Subject", "original", NULL);
	testsuite_header_set_raw_text (header, " =?utf-8?q?caf=C3=A9?=\n");
	g_mime_header_set_value (header, NULL, "caf\303\251", "utf-8");
	check (!header->explicit_raw_value, "assigning a decoded value clears explicit raw state");
	wire = header_wire_string (header, format);
	check (!strcmp (wire, "Subject: caf\303\251\n"), "decoded assignment restores International formatting");
	g_free (wire);
	g_object_unref (header);

	part = g_mime_part_new_with_type ("text", "plain");
	header = g_mime_header_list_get_header (g_mime_entity_get_header_list (GMIME_ENTITY (part)), "Content-Type");
	check (header->explicit_raw_value, "new entity-generated raw headers are marked before insertion notifications");
	g_mime_entity_set_content_type_parameter (GMIME_ENTITY (part), "name", "caf\303\251.txt");
	check (header->explicit_raw_value, "entity header updates retain explicit raw state");
	raw = g_strdup (testsuite_header_raw_text (header));
	expected = g_strconcat ("Content-Type:", raw, NULL);
	wire = header_wire_string (header, format);
	check (!strcmp (wire, expected), "entity-generated raw parameter encoding remains explicit");
	g_free (expected);
	g_free (wire);
	g_free (raw);
	g_object_unref (part);
	g_mime_format_options_free (format);
}

static void
check_mailing_list_header (GMimeHeader *header, GMimeFormatOptions *options, const char *value, const char *expected)
{
	char **lines = g_strsplit (expected, "\n", -1);
	char *raw = g_strjoinv (g_mime_format_options_get_newline (options), lines);
	char *formatted, *transport, *wire;

	g_strfreev (lines);
	g_mime_header_set_value (header, options, value, "utf-8");
	check (!strcmp (testsuite_header_raw_text (header), raw), "mailing-list assignment uses command formatting");
	formatted = g_mime_header_format_mailing_list_command (header, options, value, "utf-8");
	check (!strcmp (formatted, raw), "the public mailing-list formatter uses the selected options");
	g_free (formatted);
	transport = g_strdup_printf ("%s:%s", testsuite_header_raw_name (header), raw);
	wire = header_wire_string (header, options);
	check (!strcmp (wire, transport), "mailing-list serialization retains command formatting");
	g_free (wire);
	g_free (transport);
	g_free (raw);
}

static void
test_mailing_list_headers (void)
{
	static const char *names[] = {
		"List-Archive", "List-Help", "List-Owner", "List-Post",
		"List-Subscribe", "List-Unsubscribe", "lIsT-pOsT"
	};
	static const struct {
		const char *value, *expected;
	} cases[] = {
		{ "", "\n" },
		{ " \t<a> ,,, \t <b> ", " <a>, <b>\n" },
		{ "NO", " NO\n" },
		{ ",, , \t,,", ",\n" },
		{ "one,,two", " one, two\n" },
		{ "word(comment),<url>", " word (comment), <url>\n" },
		{ "(outer (inner\\) still) end)", " (outer (inner\\) still) end)\n" },
		{ "<unfinished (comment), tail", " <unfinished (comment), tail\n" },
		{ "(unfinished <url>, tail", " (unfinished <url>, tail\n" },
		{ "one\r\n\ttwo", " one two\n" },
		{ "=?utf-8?q?literal?=", " =?utf-8?q?literal?=\n" }
	};
	static const guint widths[] = { 60, 78, 120 };
	GMimeFormatOptions *options = g_mime_format_options_new ();
	GMimeParserOptions *parser_options;
	GString *value = g_string_new (NULL), *expected = g_string_new (NULL);
	GMimeHeader *header;
	char *word, *raw;
	const char *stored;
	guint i, j, k, mode, width, international;

	for (i = 0; i < G_N_ELEMENTS (names); i++) {
		header = g_mime_header_new (names[i], "initial", NULL);
		for (mode = 0; mode < 2; mode++) {
			g_mime_format_options_set_newline_format (options, mode ? GMIME_NEWLINE_FORMAT_DOS : GMIME_NEWLINE_FORMAT_UNIX);
			for (international = 0; international < 2; international++) {
				g_mime_format_options_set_allow_international (options, international != 0);
				for (j = 0; j < G_N_ELEMENTS (widths); j++) {
					width = widths[j];
					g_mime_format_options_set_max_line_length (options, width);
					for (k = 0; k < G_N_ELEMENTS (cases); k++)
						check_mailing_list_header (header, options, cases[k].value, cases[k].expected);
					check_mailing_list_header (header, options, "( \t )", international ? " ( \t )\n" : " ()\n");
					check_mailing_list_header (header, options, "( \303\251 )",
								  international ? " ( \303\251 )\n" : " (=?utf-8?b?w6k=?=)\n");

					word = g_strnfill (width - strlen (names[i]) - 5, 'a');
					g_string_printf (value, "<%s>", word);
					g_string_printf (expected, " %s\n", value->str);
					check_mailing_list_header (header, options, value->str, expected->str);
					g_string_insert_c (value, value->len - 1, 'a');
					g_string_printf (expected, "\n %s\n", value->str);
					check_mailing_list_header (header, options, value->str, expected->str);
					g_free (word);

					word = g_strnfill (width - strlen (names[i]) - 4, 'a');
					g_string_printf (value, "(%s)", word);
					g_string_printf (expected, " %s\n", value->str);
					if (international)
						check_mailing_list_header (header, options, value->str, expected->str);
					g_free (word);
				}
			}
		}
		stored = testsuite_header_raw_text (header);
		g_test_expect_message ("gmime", G_LOG_LEVEL_CRITICAL, "*g_utf8_validate*");
		g_mime_header_set_value (header, options, "\xff", "utf-8");
		g_test_assert_expected_messages ();
		check (testsuite_header_raw_text (header) == stored, "invalid command UTF-8 preserves assigned state");
		g_object_unref (header);
	}

	g_mime_format_options_set_allow_international (options, TRUE);
	g_mime_format_options_set_newline_format (options, GMIME_NEWLINE_FORMAT_UNIX);
	g_mime_format_options_set_max_line_length (options, 60);
	header = g_mime_header_new ("List-Post", "initial", NULL);
	word = g_strnfill (125, 'a');
	g_string_printf (expected, " %.*s\n %.*s\n %s\n", 49, word, 59, word, word + 108);
	check_mailing_list_header (header, options, word, expected->str);
	g_string_printf (value, "(%s)", word);
	g_string_printf (expected, " (%.*s\n %.*s\n %s)\n", 48, word, 59, word, word + 107);
	check_mailing_list_header (header, options, value->str, expected->str);
	g_string_printf (value, "<%s> %s", word, word);
	g_string_printf (expected, "\n <%s>\n %.*s\n %.*s\n %s\n", word, 59, word, 59, word, word + 118);
	check_mailing_list_header (header, options, value->str, expected->str);
	g_free (word);

	g_string_assign (value, "(");
	g_string_truncate (expected, 0);
	for (i = 0; i < 30; i++) {
		g_string_append (value, "a  \t");
		if (i == 24)
			g_string_append_c (expected, '\n');
		g_string_append (expected, i ? " a" : " (a");
	}
	g_string_append_c (value, ')');
	g_string_append (expected, " )\n");
	check_mailing_list_header (header, options, value->str, expected->str);

	g_string_truncate (value, 0);
	g_string_assign (expected, " ");
	for (i = 0; i < 80; i++) {
		g_string_append (value, "\360\237\230\200");
		if (i == 24 || i == 53)
			g_string_append (expected, "\n ");
		g_string_append (expected, "\360\237\230\200");
	}
	g_string_append_c (expected, '\n');
	check_mailing_list_header (header, options, value->str, expected->str);
	g_object_unref (header);

	word = g_strnfill (80, 'X');
	header = g_mime_header_new (word, "initial", NULL);
	raw = g_mime_header_format_mailing_list_command (header, options, value->str, "utf-8");
	g_string_assign (expected, "\n ");
	for (i = 0; i < 80; i++) {
		if (i == 29 || i == 58)
			g_string_append (expected, "\n ");
		g_string_append (expected, "\360\237\230\200");
	}
	g_string_append_c (expected, '\n');
	check (!strcmp (raw, expected->str),
	       "an oversized field prefix does not underflow the available command width");
	g_free (raw);
	g_free (word);
	g_object_unref (header);

	check_parsed_international_header ("List-Post: \t<a> ,,, \t<b>\n", "List-Post: <a>, <b>\n", NULL);
	check_parsed_international_header ("lIsT-pOsT \t: (=?utf-8?b?w6k=?=) <a>\n",
					  "lIsT-pOsT \t: (\303\251) <a>\n", NULL);
	parser_options = g_mime_parser_options_new ();
	g_mime_parser_options_set_charset_encoding (parser_options, "windows-1251", NULL);
	check_parsed_international_header ("List-Help: (\317\360\350\342\345\362) <a>\n",
					  "List-Help: (\320\237\321\200\320\270\320\262\320\265\321\202) <a>\n",
					  parser_options);
	g_mime_parser_options_free (parser_options);
	g_mime_format_options_free (options);
	g_string_free (value, TRUE);
	g_string_free (expected, TRUE);
}

static void
check_signature_header (GMimeHeader *header, GMimeFormatOptions *options, const char *value, const char *expected)
{
	char *wire, *transport;

	g_mime_header_set_value (header, options, value, "utf-8");
	check (!g_strcmp0 (testsuite_header_raw_text (header), expected), "signature header has expected tag folding");
	transport = g_strdup_printf ("%s:%s", testsuite_header_raw_name (header), expected);
	wire = header_wire_string (header, options);
	check (!strcmp (wire, transport), "signature serialization preserves selected folding and newline format");
	g_free (transport);
	g_free (wire);
}

static void
test_signature_headers (void)
{
	static const char *names[] = { "DKIM-Signature", "ARC-Message-Signature", "ARC-Seal", "dKiM-sIgNaTuRe" };
	static const guint widths[] = { 60, 78, 120, 998 };
	GMimeFormatOptions *options = g_mime_format_options_new ();
	GMimeHeaderList *headers;
	GMimeHeader *header, *clone;
	GString *value = g_string_new (NULL), *expected = g_string_new (NULL);
	const char *newline, *old_raw;
	char *word, *wire, *transport;
	guint i, j, mode, width;

	for (i = 0; i < G_N_ELEMENTS (names); i++) {
		headers = g_mime_header_list_new (NULL);
		g_mime_header_list_append (headers, names[i], "initial", NULL);
		header = g_mime_header_list_get_header_at (headers, 0);
		for (mode = 0; mode < 2; mode++) {
			g_mime_format_options_set_newline_format (options, mode ? GMIME_NEWLINE_FORMAT_DOS : GMIME_NEWLINE_FORMAT_UNIX);
			g_mime_format_options_set_allow_international (options, mode != 0);
			newline = g_mime_format_options_get_newline (options);
			for (j = 0; j < G_N_ELEMENTS (widths); j++) {
				width = widths[j];
				g_mime_format_options_set_max_line_length (options, width);
				g_string_printf (expected, " v=1; a=rsa-sha256;%s\tbh=YWJj;%s\tb=ZGVm;%s",
						 newline, newline, newline);
				check_signature_header (header, options, " v = 1 ; a = rsa-sha256 ;\r\n bh=YWJj; b=ZGVm; ", expected->str);
				g_string_printf (expected, " bh=YWJj; b=ZGVm;%s", newline);
				check_signature_header (header, options, "bh = YWJj ; b = ZGVm ;", expected->str);
				g_string_printf (expected, " BH=x; B=y;%s", newline);
				check_signature_header (header, options, "BH=x; B=y;", expected->str);
				check_signature_header (header, options, "", newline);
				g_string_printf (expected, "%s\tb=%s", newline, newline);
				check_signature_header (header, options, "b=", expected->str);
				g_string_printf (expected, " z=A||B|%s", newline);
				check_signature_header (header, options, "z=A||B|", expected->str);
				g_string_printf (expected, " x=\303\251\346\227\245\360\237\230\200;%s", newline);
				check_signature_header (header, options, "x=\303\251\346\227\245\360\237\230\200;", expected->str);

				word = g_strnfill (width - strlen (names[i]) - 4, 'a');
				g_string_printf (value, "x=%s", word);
				g_string_printf (expected, " %s%s", value->str, newline);
				check_signature_header (header, options, value->str, expected->str);
				g_string_append_c (value, 'a');
				g_string_printf (expected, "%s\t%s%s", newline, value->str, newline);
				check_signature_header (header, options, value->str, expected->str);
				g_free (word);

				word = g_strnfill (width - strlen (names[i]) - 5, 'a');
				g_string_printf (value, "x=%s\303\251", word);
				g_string_printf (expected, "%s\t%s%s", newline, value->str, newline);
				check_signature_header (header, options, value->str, expected->str);
				g_free (word);

				word = g_strnfill (width - 1, 'A');
				g_string_printf (value, "b=%s", word);
				g_string_printf (expected, "%s\tb=", newline);
				g_string_append_len (expected, word, width - 3);
				g_string_append_printf (expected, "%s\tAA%s", newline, newline);
				check_signature_header (header, options, value->str, expected->str);
				g_free (word);

				word = g_strnfill (width / 2, 'a');
				g_string_printf (value, "h=%s:%s", word, word);
				g_string_printf (expected, "%s\th=%s:%s\t%s%s", newline, word, newline, word, newline);
				check_signature_header (header, options, value->str, expected->str);
				g_string_printf (value, "z=%s|%s", word, word);
				g_string_printf (expected, "%s\tz=%s|%s\t%s%s", newline, word, newline, word, newline);
				check_signature_header (header, options, value->str, expected->str);
				g_free (word);

				word = g_strnfill (width - 6, 'a');
				g_string_printf (value, "h=%s:\303\251:end", word);
				g_string_printf (expected, "%s\th=%s:%s\t\303\251:end%s", newline, word, newline, newline);
				check_signature_header (header, options, value->str, expected->str);
				g_string_printf (value, "z=%s|\303\251|end", word);
				g_string_printf (expected, "%s\tz=%s|%s\t\303\251|end%s", newline, word, newline, newline);
				check_signature_header (header, options, value->str, expected->str);
				g_free (word);

				word = g_strnfill (width + 3, 'a');
				g_string_printf (value, "h=%s:bb::cc:", word);
				g_string_printf (expected, "%s\t%s\th=%s:%s\tbb::cc:%s", newline, newline, word, newline, newline);
				check_signature_header (header, options, value->str, expected->str);
				g_free (word);

				word = g_strnfill (width - 4, 'a');
				g_string_printf (value, "x=%s\360\237\230\200bb", word);
				g_string_printf (expected, "%s\tx=%s\360%s\t\237\230\200bb%s", newline, word, newline, newline);
				check_signature_header (header, options, value->str, expected->str);
				g_free (word);
			}
		}

		old_raw = testsuite_header_raw_text (header);
		g_test_expect_message ("gmime", G_LOG_LEVEL_CRITICAL, "*g_utf8_validate*");
		g_mime_header_set_value (header, options, "\xff", "utf-8");
		g_test_assert_expected_messages ();
		check (testsuite_header_raw_text (header) == old_raw, "invalid signature UTF-8 preserves assigned header state");
		testsuite_header_set_raw_text (header, " v = 1;\r\n  b = preserved;\r\n");
		(void) g_mime_header_get_value (header);
		clone = g_mime_header_clone (header);
		wire = header_wire_string (clone, options);
		transport = g_strdup_printf ("%s: v = 1;\r\n  b = preserved;\r\n", names[i]);
		check (!strcmp (wire, transport), "raw signature folding survives decoding, cloning and serialization");
		g_free (transport);
		g_free (wire);
		g_object_unref (clone);
		g_object_unref (headers);
	}
	{
		static const char text[] = "DKIM-Signature: v = 1;\r\n  b = original;\r\n"
			"ARC-Message-Signature: i = 1;\r\n  b = original;\r\n"
			"ARC-Seal: i = 1;\r\n  b = original;\r\n\r\n";
		GMimeStream *stream = g_mime_stream_mem_new_with_buffer (text, strlen (text));
		GMimeParser *parser = g_mime_parser_new_with_stream (stream);
		GMimeMessage *message = g_mime_parser_construct_message (parser, NULL, NULL);

		g_assert_nonnull (message);
		headers = g_mime_entity_get_header_list (GMIME_ENTITY (message));
		for (i = 0; i < 3; i++) {
			header = g_mime_header_list_get_header (headers, names[i]);
			wire = header_wire_string (header, options);
			transport = g_strdup_printf ("%s: %s = 1;\r\n  b = original;\r\n", names[i], i ? "i" : "v");
			check (!strcmp (wire, transport), "parsed signatures preserve original raw whitespace and folding");
			g_free (transport);
			g_free (wire);
		}
		g_object_unref (message);
		g_object_unref (parser);
		g_object_unref (stream);
	}
	g_mime_format_options_free (options);
	g_string_free (expected, TRUE);
	g_string_free (value, TRUE);
}

static void
test_header_list_positions (void)
{
	static const char *names[] = { "X-First", "X-Between", "sUbJeCt", "Subject", "Subject", "SUBJECT" };
	static const char *values[] = { "first", "between", "before", "middle", "original", "after" };
	GMimeHeaderList *headers = g_mime_header_list_new (NULL);
	GMimeHeader *header, *retained;
	GMimeMessage *message;
	char *wire, *before, *after;
	guint i;

	check (g_mime_header_list_index_of (headers, "Subject") == -1, "empty header list has no first match");
	check (g_mime_header_list_last_index_of (headers, "Subject") == -1, "empty header list has no last match");
	g_mime_header_list_insert (headers, 0, "X-First", "first", NULL);
	g_mime_header_list_insert (headers, 1, "Subject", "original", NULL);
	g_mime_header_list_insert (headers, 1, "X-Between", "between", NULL);
	g_mime_header_list_insert (headers, 2, "sUbJeCt", "before", NULL);
	g_mime_header_list_insert (headers, 4, "SUBJECT", "after", NULL);
	g_mime_header_list_insert (headers, 3, "Subject", "middle", NULL);
	check (g_mime_header_list_get_count (headers) == G_N_ELEMENTS (names), "indexed insertion preserves all duplicate headers");
	for (i = 0; i < G_N_ELEMENTS (names); i++) {
		header = g_mime_header_list_get_header_at (headers, i);
		check (!strcmp (g_mime_header_get_name (header), names[i]), "indexed insertion preserves field order and case");
		check (!strcmp (g_mime_header_get_value (header), values[i]), "indexed insertion preserves value order");
		check (g_mime_header_get_offset (header) == -1, "inserted header has no source offset");
	}
	check (g_mime_header_list_index_of (headers, "SUBJECT") == 2, "first index ignores field-name case");
	check (g_mime_header_list_last_index_of (headers, "subject") == 5, "last index ignores field-name case");
	check (g_mime_header_list_get_header (headers, "subject") == g_mime_header_list_get_header_at (headers, 2),
	       "insertion at the old first index updates name lookup");
	check (g_mime_header_list_index_of (headers, "missing") == -1 &&
	       g_mime_header_list_last_index_of (headers, "missing") == -1, "missing header indexes are -1");
	g_mime_header_list_remove_at (headers, 2);
	check (g_mime_header_list_get_header (headers, "subject") == g_mime_header_list_get_header_at (headers, 2),
	       "removing inserted first duplicate promotes the next header");
	check (g_mime_header_list_index_of (headers, "subject") == 2 &&
	       g_mime_header_list_last_index_of (headers, "subject") == 4, "duplicate removal updates first and last indexes");
	g_mime_header_list_remove (headers, "subject");
	check (!strcmp (g_mime_header_get_value (g_mime_header_list_get_header (headers, "subject")), "original"),
	       "name removal after insertion promotes the correct duplicate");
	g_mime_header_list_set (headers, "subject", "single", NULL);
	check (g_mime_header_list_index_of (headers, "subject") == 2 &&
	       g_mime_header_list_last_index_of (headers, "subject") == 2, "set removes remaining inserted duplicates");

	g_mime_header_list_insert (headers, 1, "X-Empty", NULL, NULL);
	header = g_mime_header_list_get_header_at (headers, 1);
	check (g_mime_header_get_value (header) == NULL, "indexed insertion accepts an unset placeholder");
	wire = g_mime_header_list_to_string (headers, NULL);
	check (!strstr (wire, "X-Empty:"), "inserted placeholder is not serialized");
	g_free (wire);
	g_mime_header_set_value (header, NULL, "now assigned", NULL);
	wire = g_mime_header_list_to_string (headers, NULL);
	check (strstr (wire, "X-Empty: now assigned") != NULL, "assigned inserted placeholder is serialized");
	g_free (wire);

	before = g_mime_header_list_to_string (headers, NULL);
	g_test_expect_message ("gmime", G_LOG_LEVEL_CRITICAL, "*index >= 0*");
	g_mime_header_list_insert (headers, -1, "X-Invalid", "negative", NULL);
	g_test_assert_expected_messages ();
	g_test_expect_message ("gmime", G_LOG_LEVEL_CRITICAL, "*index*headers->array->len*");
	g_mime_header_list_insert (headers, g_mime_header_list_get_count (headers) + 1, "X-Invalid", "past end", NULL);
	g_test_assert_expected_messages ();
	g_test_expect_message ("gmime", G_LOG_LEVEL_CRITICAL, "*name != NULL*");
	g_mime_header_list_insert (headers, 0, NULL, "invalid", NULL);
	g_test_assert_expected_messages ();
	g_test_expect_message ("gmime", G_LOG_LEVEL_CRITICAL, "*name != NULL*");
	check (g_mime_header_list_index_of (headers, NULL) == -1, "first-index lookup guards a NULL name");
	g_test_assert_expected_messages ();
	g_test_expect_message ("gmime", G_LOG_LEVEL_CRITICAL, "*name != NULL*");
	check (g_mime_header_list_last_index_of (headers, NULL) == -1, "last-index lookup guards a NULL name");
	g_test_assert_expected_messages ();
	after = g_mime_header_list_to_string (headers, NULL);
	check (!strcmp (before, after), "invalid positional operations leave header list unchanged");
	g_free (before);
	g_free (after);
	g_object_unref (headers);

	message = g_mime_message_new (FALSE);
	headers = g_mime_entity_get_header_list (GMIME_ENTITY (message));
	g_mime_header_list_append (headers, "X-Before", "before", NULL);
	g_mime_header_list_append (headers, "X-After", "after", NULL);
	g_mime_header_list_insert (headers, 1, "Subject", "inserted", NULL);
	check (!g_strcmp0 (g_mime_message_get_subject (message), "inserted"), "middle insertion updates owning message subject");
	retained = g_object_ref (g_mime_header_list_get_header (headers, "Subject"));
	g_mime_header_set_value (retained, NULL, "changed", NULL);
	check (!g_strcmp0 (g_mime_message_get_subject (message), "changed"), "inserted header changes notify the owner");
	g_mime_header_list_remove_at (headers, 1);
	check (g_mime_message_get_subject (message) == NULL, "removing inserted header notifies the owner");
	g_mime_header_list_insert (headers, 1, "Subject", "replacement", NULL);
	g_mime_header_set_value (retained, NULL, "detached", NULL);
	check (!g_strcmp0 (g_mime_message_get_subject (message), "replacement"), "removed inserted header stops notifying the owner");
	g_object_unref (retained);
	g_mime_header_list_append (headers, "To", "after@example.com", NULL);
	g_mime_header_list_insert (headers, 1, "To", "before@example.com", NULL);
	{
		GMimeInternetAddressList *addresses = g_mime_message_get_to (message);

		check (g_mime_internet_address_list_length (addresses) == 2, "middle insertion rebuilds duplicate address headers");
		check (!strcmp (g_mime_mailbox_address_get_addrspec (GMIME_MAILBOX_ADDRESS (
			g_mime_internet_address_list_get_address (addresses, 0))), "before@example.com"),
		       "inserted address precedes the existing address in the owner");
	}
	g_object_unref (message);
	{
		static const char text[] = "Subject: original\r\nX-Raw: one\r\n\t two\r\n\r\n";
		GMimeStream *stream = g_mime_stream_mem_new_with_buffer (text, strlen (text));
		GMimeParser *parser = g_mime_parser_new_with_stream (stream);

		message = g_mime_parser_construct_message (parser, NULL, NULL);
		g_assert_nonnull (message);
		headers = g_mime_entity_get_header_list (GMIME_ENTITY (message));
		header = g_mime_header_list_get_header (headers, "X-Raw");
		before = header_wire_string (header, NULL);
		g_mime_header_list_insert (headers, 1, "X-Inserted", "new", NULL);
		after = header_wire_string (header, NULL);
		check (g_mime_header_list_get_header_at (headers, 2) == header && !strcmp (before, after),
		       "insertion preserves existing parsed header identity and raw folding");
		g_free (before);
		g_free (after);
		g_object_unref (message);
		g_object_unref (parser);
		g_object_unref (stream);
	}
}

static void
test_header_list_identifiers (void)
{
	static const GMimeHeaderId invalid_ids[] = {
		GMIME_HEADER_UNKNOWN, -2, GMIME_HEADER_X_STATUS + 1, G_MININT, G_MAXINT
	};
	static const char binary[] = "sUbJeCt \t: a\0b\r\n";
	GMimeHeaderList *headers, *other;
	GMimeHeader *header, *second, *keep;
	GMimeMessage *message;
	GMimeHeaderId id;
	GMimeStream *stream;
	GBytes *raw_name, *raw_value;
	GError *error = NULL;
	char *name, *wire;
	guint changes, other_changes, i;

	for (id = GMIME_HEADER_ACCEPT_LANGUAGE; id <= GMIME_HEADER_X_STATUS; id++) {
		headers = g_mime_header_list_new (NULL);
		check (!g_mime_header_list_contains_id (headers, id) &&
		       g_mime_header_list_index_of_id (headers, id) == -1 &&
		       g_mime_header_list_last_index_of_id (headers, id) == -1 &&
		       g_mime_header_list_get_value_by_id (headers, id) == NULL &&
		       !g_mime_header_list_remove_id (headers, id), "empty lists have no matching header identifier");
		g_mime_header_list_remove_all_id (headers, id);
		g_mime_header_list_append (headers, "X-Id-Keep", "before", NULL);
		wire = g_strdup_printf ("%s \t: first\r\n", g_mime_header_id_to_name (id));
		header = g_mime_header_parse (NULL, (const guint8 *) wire, strlen (wire), &error);
		g_assert_no_error (error);
		g_assert_nonnull (header);
		g_free (wire);
		g_mime_header_list_add (headers, header);
		g_mime_header_list_append (headers, "X-Id-Keep", "after", NULL);
		keep = g_mime_header_list_get_header_at (headers, 2);
		name = g_ascii_strup (g_mime_header_id_to_name (id), -1);
		g_mime_header_list_append (headers, name, "second", NULL);
		g_free (name);
		second = g_mime_header_list_get_header_at (headers, 3);
		raw_name = header->raw_name;
		raw_value = header->raw_value;
		changes = 0;
		g_mime_event_add (headers->changed, checked_header_list_changed, &changes);
		headers->has_body_separator = FALSE;
		check (g_mime_header_list_contains_id (headers, id) &&
		       g_mime_header_list_index_of_id (headers, id) == 1 &&
		       g_mime_header_list_last_index_of_id (headers, id) == 3 &&
		       header->value == NULL, "identifier lookup uses cached IDs without decoding values");
		check (!strcmp (g_mime_header_list_get_value_by_id (headers, id), "first") &&
		       header->raw_name == raw_name && header->raw_value == raw_value &&
		       changes == 0 && !headers->has_body_separator,
		       "identifier value lookup decodes only the first value without changing raw bytes or notifying owners");
		check (g_mime_header_list_remove_id (headers, id) && changes == 1 &&
		       g_mime_header_list_index_of_id (headers, id) == 2 &&
		       g_mime_header_list_last_index_of_id (headers, id) == 2 &&
		       g_mime_header_list_get_header (headers, g_mime_header_id_to_name (id)) == second &&
		       !strcmp (g_mime_header_list_get_value_by_id (headers, id), "second"),
		       "identifier removal removes only the first occurrence and promotes the next case variant");
		g_mime_header_set_value (header, NULL, "detached", NULL);
		check (changes == 1, "removed headers stop notifying the identifier-removal list");
		g_mime_header_list_remove_all_id (headers, id);
		check (changes == 2 && g_mime_header_list_get_count (headers) == 2 &&
		       !g_mime_header_list_contains_id (headers, id) &&
		       g_mime_header_list_get_header_at (headers, 1) == keep,
		       "identifier bulk removal notifies for each occurrence and preserves unrelated header order");
		headers->has_body_separator = FALSE;
		check (!g_mime_header_list_remove_id (headers, id), "absent identifier removal returns false");
		g_mime_header_list_remove_all_id (headers, id);
		check (changes == 2 && !headers->has_body_separator, "absent identifier removal leaves separator state unchanged");
		g_object_unref (header);
		g_object_unref (headers);
	}

	stream = g_mime_stream_mem_new_with_buffer (binary, sizeof (binary) - 1);
	headers = g_mime_header_list_load (NULL, stream, &error);
	g_assert_no_error (error);
	g_assert_nonnull (headers);
	g_object_unref (stream);
	header = g_mime_header_list_get_header_at (headers, 0);
	raw_name = header->raw_name;
	raw_value = header->raw_value;
	check (g_mime_header_list_contains_id (headers, GMIME_HEADER_SUBJECT) &&
	       g_mime_header_list_index_of_id (headers, GMIME_HEADER_SUBJECT) == 0 &&
	       g_mime_header_list_last_index_of_id (headers, GMIME_HEADER_SUBJECT) == 0 && header->value == NULL,
	       "identifier lookup preserves a binary header's lazy decoded-value cache");
	check (!strcmp (g_mime_header_list_get_value_by_id (headers, GMIME_HEADER_SUBJECT), "a\357\277\275b") &&
	       header->raw_name == raw_name && header->raw_value == raw_value &&
	       g_bytes_get_size (raw_value) == 6 &&
	       !memcmp (g_bytes_get_data (raw_value, NULL), " a\0b\r\n", 6),
	       "identifier value lookup replaces decoded NULs without changing the binary raw value");
	g_mime_header_list_clear (headers);
	g_mime_header_list_append (headers, "Subject", NULL, NULL);
	g_mime_header_list_append (headers, "SUBJECT", "second", NULL);
	check (g_mime_header_list_contains_id (headers, GMIME_HEADER_SUBJECT) &&
	       g_mime_header_list_get_value_by_id (headers, GMIME_HEADER_SUBJECT) == NULL,
	       "identifier value lookup does not skip the first unset placeholder");
	g_mime_header_list_clear (headers);
	g_mime_header_list_append (headers, "Unknown", "literal unknown field", NULL);
	changes = 0;
	g_mime_event_add (headers->changed, checked_header_list_changed, &changes);
	headers->has_body_separator = FALSE;
	for (i = 0; i < G_N_ELEMENTS (invalid_ids); i++) {
		id = invalid_ids[i];
		g_test_expect_message ("gmime", G_LOG_LEVEL_CRITICAL, "*id >= GMIME_HEADER_ACCEPT_LANGUAGE*");
		check (!g_mime_header_list_contains_id (headers, id), "invalid contains identifiers are rejected");
		g_test_assert_expected_messages ();
		g_test_expect_message ("gmime", G_LOG_LEVEL_CRITICAL, "*id >= GMIME_HEADER_ACCEPT_LANGUAGE*");
		check (g_mime_header_list_index_of_id (headers, id) == -1, "invalid first-index identifiers are rejected");
		g_test_assert_expected_messages ();
		g_test_expect_message ("gmime", G_LOG_LEVEL_CRITICAL, "*id >= GMIME_HEADER_ACCEPT_LANGUAGE*");
		check (g_mime_header_list_last_index_of_id (headers, id) == -1, "invalid last-index identifiers are rejected");
		g_test_assert_expected_messages ();
		g_test_expect_message ("gmime", G_LOG_LEVEL_CRITICAL, "*id >= GMIME_HEADER_ACCEPT_LANGUAGE*");
		check (g_mime_header_list_get_value_by_id (headers, id) == NULL, "invalid value identifiers are rejected");
		g_test_assert_expected_messages ();
		g_test_expect_message ("gmime", G_LOG_LEVEL_CRITICAL, "*id >= GMIME_HEADER_ACCEPT_LANGUAGE*");
		check (!g_mime_header_list_remove_id (headers, id), "invalid removal identifiers are rejected");
		g_test_assert_expected_messages ();
		g_test_expect_message ("gmime", G_LOG_LEVEL_CRITICAL, "*id >= GMIME_HEADER_ACCEPT_LANGUAGE*");
		g_mime_header_list_remove_all_id (headers, id);
		g_test_assert_expected_messages ();
	}
	check (g_mime_header_list_get_count (headers) == 1 && changes == 0 && !headers->has_body_separator &&
	       !strcmp (g_mime_header_list_get_value (headers, "Unknown"), "literal unknown field"),
	       "invalid identifiers never match extension fields or mutate the list");
	g_object_unref (headers);

	headers = g_mime_header_list_new (NULL);
	other = g_mime_header_list_new (NULL);
	header = g_mime_header_new ("Subject", "shared", NULL);
	g_mime_header_list_add (headers, header);
	g_mime_header_list_append (headers, "sUbJeCt", "middle", NULL);
	second = g_mime_header_list_get_header_at (headers, 1);
	g_mime_header_list_add (headers, header);
	g_mime_header_list_add (other, header);
	changes = other_changes = 0;
	g_mime_event_add (headers->changed, checked_header_list_changed, &changes);
	g_mime_event_add (other->changed, checked_header_list_changed, &other_changes);
	check (g_mime_header_list_remove_id (headers, GMIME_HEADER_SUBJECT) &&
	       g_mime_header_list_index_of_id (headers, GMIME_HEADER_SUBJECT) == 0 &&
	       g_mime_header_list_last_index_of_id (headers, GMIME_HEADER_SUBJECT) == 1 &&
	       g_mime_header_list_get_header (headers, "Subject") == second,
	       "identifier removal preserves first-name lookup when the same header instance occurs again");
	g_mime_header_set_value (header, NULL, "changed", NULL);
	check (changes == 2 && other_changes == 1, "surviving shared occurrences retain their change subscriptions");
	g_mime_header_list_remove_all_id (headers, GMIME_HEADER_SUBJECT);
	check (changes == 4 && g_mime_header_list_get_count (headers) == 0, "bulk identifier removal notifies for every remaining occurrence");
	g_mime_header_set_value (header, NULL, "other only", NULL);
	check (changes == 4 && other_changes == 2 &&
	       !strcmp (g_mime_header_list_get_value_by_id (other, GMIME_HEADER_SUBJECT), "other only"),
	       "bulk identifier removal detaches only the target list from shared headers");
	g_object_unref (header);
	g_object_unref (headers);
	g_object_unref (other);

	message = g_mime_message_new (FALSE);
	headers = g_mime_entity_get_header_list (GMIME_ENTITY (message));
	g_mime_header_list_append (headers, "Subject", "first", NULL);
	g_mime_header_list_append (headers, "SUBJECT", "second", NULL);
	g_mime_header_list_append (headers, "To", "one@example.com", NULL);
	g_mime_header_list_append (headers, "TO", "two@example.com", NULL);
	check (g_mime_header_list_remove_id (headers, GMIME_HEADER_SUBJECT) &&
	       !g_strcmp0 (g_mime_message_get_subject (message), "second") &&
	       !g_strcmp0 (g_mime_header_list_get_value_by_id (headers, GMIME_HEADER_SUBJECT), "second"),
	       "identifier removal promotes the next subject in the owning message");
	g_mime_header_list_remove_all_id (headers, GMIME_HEADER_SUBJECT);
	g_mime_header_list_remove_all_id (headers, GMIME_HEADER_TO);
	check (g_mime_message_get_subject (message) == NULL &&
	       g_mime_internet_address_list_length (g_mime_message_get_to (message)) == 0,
	       "bulk identifier removal synchronizes owner subjects and address lists");
	g_object_unref (message);
}

static void
test_header_id_construction (void)
{
	static const GMimeHeaderId invalid[] = {
		GMIME_HEADER_UNKNOWN, -2, GMIME_HEADER_X_STATUS + 1, G_MININT, G_MAXINT
	};
	GMimeHeader *header, *named, *clone;
	GMimeHeaderId id;
	const char *name, *value;
	GError *error = NULL;
	guint checked, i;

	for (id = GMIME_HEADER_ACCEPT_LANGUAGE; id <= GMIME_HEADER_X_STATUS; id++) {
		name = g_mime_header_id_to_name (id);
		value = id == GMIME_HEADER_CONTENT_TYPE ? "text/plain; charset=utf-8" :
			id == GMIME_HEADER_CONTENT_DISPOSITION ? "attachment; filename=example.txt" : "first";
		for (checked = 0; checked < 2; checked++) {
			header = checked ? g_mime_header_new_with_id_full (id, value, NULL, &error) :
				g_mime_header_new_with_id (id, value, NULL);
			named = checked ? g_mime_header_new_full (name, value, NULL, &error) :
				g_mime_header_new (name, value, NULL);
			g_assert_no_error (error);
			g_assert_nonnull (header);
			g_assert_nonnull (named);
			check (g_mime_header_get_id (header) == id && !strcmp (header->name, name) &&
			       !strcmp (header->value, value) && g_bytes_equal (header->raw_name, named->raw_name) &&
			       g_bytes_equal (header->raw_value, named->raw_value),
			       "identifier constructors use canonical names and the existing field-specific formatting");
			check (header->owns_options && header->options != named->options && header->offset == -1,
			       "identifier constructors own independent default parser options and have no source offset");
			clone = g_mime_header_clone (header);
			g_object_unref (header);
			check (g_mime_header_get_id (clone) == id && g_bytes_equal (clone->raw_value, named->raw_value),
			       "identifier-constructed clones retain their identifier and raw bytes after the original is freed");
			g_object_unref (clone);
			g_object_unref (named);
		}
	}
	header = g_mime_header_new_with_id_full (GMIME_HEADER_SUBJECT, "\302\240 caf\303\251\r\n \342\200\203",
						"iso-8859-1", &error);
	g_assert_no_error (error);
	g_assert_nonnull (header);
	check (!strcmp (header->value, "\302\240 caf\303\251 \342\200\203") &&
	       !strcmp (header->charset, "iso-8859-1") &&
	       strstr (testsuite_header_raw_text (header), "utf-8") != NULL,
	       "identifier construction preserves Unicode whitespace and uses lossless charset fallback");
	g_object_unref (header);
	check (g_mime_header_new_with_id_full (GMIME_HEADER_CONTENT_TYPE, "invalid", NULL, &error) == NULL &&
	       g_error_matches (error, GMIME_ERROR, GMIME_ERROR_PARSE_ERROR), "checked identifier construction reports invalid structured values");
	g_clear_error (&error);
	check (g_mime_header_new_with_id_full (GMIME_HEADER_SUBJECT, "\377", NULL, &error) == NULL &&
	       g_error_matches (error, G_CONVERT_ERROR, G_CONVERT_ERROR_ILLEGAL_SEQUENCE),
	       "checked identifier construction reports invalid UTF-8");
	g_clear_error (&error);
	check (g_mime_header_new_with_id_full (GMIME_HEADER_SUBJECT, "value", "x-no-such-charset", &error) == NULL &&
	       error != NULL && error->domain == G_CONVERT_ERROR, "checked identifier construction reports unsupported charsets");
	g_clear_error (&error);
	for (i = 0; i < G_N_ELEMENTS (invalid); i++) {
		g_test_expect_message ("gmime", G_LOG_LEVEL_CRITICAL, "*id >= GMIME_HEADER_ACCEPT_LANGUAGE*");
		check (g_mime_header_new_with_id (invalid[i], "value", NULL) == NULL, "identifier construction rejects unknown and out-of-range IDs");
		g_test_assert_expected_messages ();
		g_test_expect_message ("gmime", G_LOG_LEVEL_CRITICAL, "*id >= GMIME_HEADER_ACCEPT_LANGUAGE*");
		check (g_mime_header_new_with_id_full (invalid[i], "value", NULL, &error) == NULL && error == NULL,
		       "checked identifier construction rejects invalid IDs as programming errors");
		g_test_assert_expected_messages ();
	}
	g_test_expect_message ("gmime", G_LOG_LEVEL_CRITICAL, "*value != NULL*");
	check (g_mime_header_new_with_id (GMIME_HEADER_SUBJECT, NULL, NULL) == NULL, "identifier construction requires a value");
	g_test_assert_expected_messages ();
	g_test_expect_message ("gmime", G_LOG_LEVEL_CRITICAL, "*value != NULL*");
	check (g_mime_header_new_with_id_full (GMIME_HEADER_SUBJECT, NULL, NULL, &error) == NULL,
	       "checked identifier construction requires a value");
	g_test_assert_expected_messages ();
}

static gboolean
change_header_list_id_value (GMimeHeaderList *headers, guint operation, int index, GMimeHeaderId id,
			    const char *value, const char *charset, GError **error)
{
	switch (operation) {
	case 0:
		return g_mime_header_list_append_id_full (headers, id, value, charset, error);
	case 1:
		return g_mime_header_list_insert_id_full (headers, index, id, value, charset, error);
	case 2:
		return g_mime_header_list_replace_value_by_id_full (headers, id, value, charset, error);
	default:
		return g_mime_header_list_set_value_by_id_full (headers, id, value, charset, error);
	}
}

static void
test_header_list_id_values (void)
{
	static const struct {
		GMimeHeaderId id;
		const char *value, *charset;
	} invalid[] = {
		{ GMIME_HEADER_CONTENT_TYPE, "invalid", NULL },
		{ GMIME_HEADER_CONTENT_TYPE, "text/plain; name=\"unfinished", NULL },
		{ GMIME_HEADER_CONTENT_DISPOSITION, "\"attachment\"", NULL },
		{ GMIME_HEADER_CONTENT_DISPOSITION, "attachment; filename=\"unfinished", NULL },
		{ GMIME_HEADER_SUBJECT, "\377", NULL },
		{ GMIME_HEADER_SUBJECT, "value", "" },
		{ GMIME_HEADER_SUBJECT, "value", "x-no-such-charset" }
	};
	static const GMimeHeaderId invalid_ids[] = {
		GMIME_HEADER_UNKNOWN, -2, GMIME_HEADER_X_STATUS + 1, G_MININT, G_MAXINT
	};
	GMimeHeaderList *headers, *shared;
	GMimeHeader *original[5], *header;
	GMimeMessage *message, *other;
	GMimeParserOptions *options;
	GMimeHeaderId id;
	GMimeStream *stream;
	GBytes *raw[5], *bytes, *field;
	GError *error = NULL;
	const char *value, *seed;
	char *lower, *upper, *wire;
	guint operation, i, j, next, changes, shared_changes;
	gint64 offset;

	for (id = GMIME_HEADER_ACCEPT_LANGUAGE; id <= GMIME_HEADER_X_STATUS; id++) {
		seed = id == GMIME_HEADER_CONTENT_TYPE ? "text/plain" :
			id == GMIME_HEADER_CONTENT_DISPOSITION ? "attachment" : "first";
		value = id == GMIME_HEADER_CONTENT_TYPE ? "text/html" :
			id == GMIME_HEADER_CONTENT_DISPOSITION ? "inline" : "replacement";
		lower = g_ascii_strdown (g_mime_header_id_to_name (id), -1);
		upper = g_ascii_strup (g_mime_header_id_to_name (id), -1);
		wire = g_strdup_printf ("X-Before: before\r\n%s \t: %s\r\nX-Keep: keep\r\n%s: %s\r\nX-After: after\r\n",
					lower, seed, upper, seed);
		for (operation = 0; operation < 4; operation++) {
			stream = g_mime_stream_mem_new_with_buffer (wire, strlen (wire));
			headers = g_mime_header_list_load (NULL, stream, &error);
			g_assert_no_error (error);
			g_assert_nonnull (headers);
			g_object_unref (stream);
			bytes = g_bytes_new_static (" \0keep\r\n", 8);
			check (g_mime_header_set_raw_value (g_mime_header_list_get_header_at (headers, 2), bytes, &error),
			       "prepare a binary unrelated header for identifier mutations");
			g_bytes_unref (bytes);
			for (i = 0; i < 5; i++) {
				original[i] = g_object_ref (g_mime_header_list_get_header_at (headers, i));
				raw[i] = original[i]->raw_value;
			}
			field = original[1]->raw_name;
			options = original[1]->options;
			offset = original[1]->offset;
			changes = 0;
			g_mime_event_add (headers->changed, count_checked_header_changes, &changes);
			check (change_header_list_id_value (headers, operation, 2, id, value, NULL, &error),
			       "checked identifier list mutation succeeds for every known field");
			g_assert_no_error (error);
			next = operation == 0 ? 5 : operation == 1 ? 2 : 1;
			header = g_mime_header_list_get_header_at (headers, next);
			check (header->id == id && !strcmp (header->value, value) &&
			       !strcmp (header->name, operation == 3 ? lower : g_mime_header_id_to_name (id)) &&
			       changes == 1 && headers->has_body_separator == (operation != 3),
			       "identifier mutations use canonical new names, preserve existing spelling on assignment and retain notification/separator semantics");
			check (g_mime_header_list_get_count (headers) == (operation < 2 ? 6 : operation == 2 ? 4 : 5) &&
			       (operation == 3 ? header == original[1] && header->offset == offset &&
				header->raw_name == field && header->options == options :
				header != original[1] && header->owns_options && header->offset == -1),
			       "identifier assignment preserves identity and duplicates while replacement creates one new header");
			for (i = j = 0; i < 5; i++) {
				if (operation == 3 && i == 1)
					continue;
				check (original[i]->raw_value == raw[i], "identifier mutation preserves unrelated and detached raw bytes");
				if (operation == 2 && (i == 1 || i == 3))
					continue;
				if (operation != 3 && j == next)
					j++;
				check (g_mime_header_list_get_header_at (headers, operation == 3 ? i : j++) == original[i],
				       "identifier mutations preserve surviving header identity and relative order");
			}
			g_object_unref (headers);
			for (i = 0; i < 5; i++)
				g_object_unref (original[i]);
			headers = g_mime_header_list_new (NULL);
			check (change_header_list_id_value (headers, operation, 0, id, value, NULL, &error) &&
			       g_mime_header_list_get_count (headers) == 1 &&
			       g_mime_header_list_get_header_at (headers, 0)->id == id,
			       "identifier mutations append canonical headers when absent, including assignment and replacement");
			g_assert_no_error (error);
			g_object_unref (headers);
		}
		g_free (wire);
		g_free (lower);
		g_free (upper);
	}

	for (operation = 0; operation < 4; operation++) {
		for (i = 0; i < G_N_ELEMENTS (invalid); i++) {
			headers = g_mime_header_list_new (NULL);
			seed = invalid[i].id == GMIME_HEADER_CONTENT_TYPE ? "text/plain" :
				invalid[i].id == GMIME_HEADER_CONTENT_DISPOSITION ? "attachment" : "original";
			check (g_mime_header_list_append_id_full (headers, invalid[i].id, seed, NULL, &error), "prepare a valid original identifier header");
			header = g_mime_header_list_get_header_at (headers, 0);
			bytes = g_bytes_ref (header->raw_value);
			headers->has_body_separator = FALSE;
			changes = 0;
			g_mime_event_add (headers->changed, count_checked_header_changes, &changes);
			check (!change_header_list_id_value (headers, operation, 0, invalid[i].id,
							    invalid[i].value, invalid[i].charset, &error) && error != NULL,
			       "identifier list mutations propagate structured-value and conversion errors");
			g_clear_error (&error);
			check (!change_header_list_id_value (headers, operation, 0, invalid[i].id,
							    invalid[i].value, invalid[i].charset, NULL),
			       "identifier list mutations can fail without an error destination");
			check (changes == 0 && !headers->has_body_separator && g_mime_header_list_get_count (headers) == 1 &&
			       g_mime_header_list_get_header_at (headers, 0) == header && header->raw_value == bytes &&
			       !strcmp (header->value, seed), "failed identifier mutations preserve identity, cached/raw state, separator state and events");
			g_bytes_unref (bytes);
			g_object_unref (headers);
		}
		headers = g_mime_header_list_new (NULL);
		for (i = 0; i < G_N_ELEMENTS (invalid_ids); i++) {
			g_test_expect_message ("gmime", G_LOG_LEVEL_CRITICAL, "*id >= GMIME_HEADER_ACCEPT_LANGUAGE*");
			check (!change_header_list_id_value (headers, operation, 0, invalid_ids[i], "value", NULL, &error),
			       "identifier list mutations reject unknown and out-of-range IDs");
			g_test_assert_expected_messages ();
		}
		g_test_expect_message ("gmime", G_LOG_LEVEL_CRITICAL, "*value != NULL*");
		check (!change_header_list_id_value (headers, operation, 0, GMIME_HEADER_SUBJECT, NULL, NULL, &error),
		       "identifier list mutations reject NULL values instead of creating placeholders");
		g_test_assert_expected_messages ();
		error = g_error_new_literal (GMIME_ERROR, GMIME_ERROR_GENERAL, "existing error");
		g_test_expect_message ("gmime", G_LOG_LEVEL_CRITICAL, "*error == NULL || *error == NULL*");
		check (!change_header_list_id_value (headers, operation, 0, GMIME_HEADER_SUBJECT, "value", NULL, &error) &&
		       !strcmp (error->message, "existing error"), "identifier mutations require a clear error destination");
		g_test_assert_expected_messages ();
		g_clear_error (&error);
		if (operation == 1) {
			g_test_expect_message ("gmime", G_LOG_LEVEL_CRITICAL, "*index >= 0*");
			check (!change_header_list_id_value (headers, operation, -1, GMIME_HEADER_SUBJECT, "value", NULL, &error),
			       "identifier insertion rejects negative indexes");
			g_test_assert_expected_messages ();
			g_test_expect_message ("gmime", G_LOG_LEVEL_CRITICAL, "*index <= headers->array->len*");
			check (!change_header_list_id_value (headers, operation, 1, GMIME_HEADER_SUBJECT, "value", NULL, &error),
			       "identifier insertion rejects indexes beyond the current count");
			g_test_assert_expected_messages ();
		}
		check (g_mime_header_list_get_count (headers) == 0, "invalid identifier mutations leave an empty list unchanged");
		g_object_unref (headers);
	}

	message = g_mime_message_new (FALSE);
	other = g_mime_message_new (FALSE);
	headers = g_mime_entity_get_header_list (GMIME_ENTITY (message));
	shared = g_mime_entity_get_header_list (GMIME_ENTITY (other));
	check (g_mime_header_list_append_id_full (headers, GMIME_HEADER_SUBJECT, "original", NULL, &error),
	       "identifier append updates an owning message");
	header = g_mime_header_list_get_header_at (headers, 0);
	g_mime_header_list_add (headers, header);
	g_mime_header_list_add (shared, header);
	changes = shared_changes = 0;
	g_mime_event_add (headers->changed, count_checked_header_changes, &changes);
	g_mime_event_add (shared->changed, count_checked_header_changes, &shared_changes);
	check (g_mime_header_list_set_value_by_id_full (headers, GMIME_HEADER_SUBJECT, "changed", NULL, &error) &&
	       changes == 2 && shared_changes == 1 && !g_strcmp0 (g_mime_message_get_subject (message), "changed") &&
	       !g_strcmp0 (g_mime_message_get_subject (other), "changed"),
	       "identifier assignment updates repeated/shared instances and every owning message");
	check (g_mime_header_list_replace_value_by_id_full (headers, GMIME_HEADER_SUBJECT, "replacement", NULL, &error) &&
	       !g_strcmp0 (g_mime_message_get_subject (message), "replacement") &&
	       !g_strcmp0 (g_mime_message_get_subject (other), "changed") && g_mime_header_list_get_count (headers) == 1,
	       "identifier replacement collapses duplicates without changing removed headers in other owners");
	g_assert_no_error (error);
	g_object_unref (message);
	g_object_unref (other);
}

static void
test_header_list_remove_all (void)
{
	GMimeHeaderList *headers = g_mime_header_list_new (NULL);
	GMimeHeader *header, *keep, *retained;
	GMimeMessage *message;
	GWeakRef removed;
	const char *name;
	char *before, *after;

	g_mime_header_list_remove_all (headers, "missing");
	check (g_mime_header_list_get_count (headers) == 0, "bulk removal from an empty list is a no-op");
	g_mime_header_list_append (headers, "Subject", "one", NULL);
	g_mime_header_list_append (headers, "X-Keep", "keep", NULL);
	keep = g_mime_header_list_get_header (headers, "X-Keep");
	testsuite_header_set_raw_text (keep, " one\r\n\t two\r\n");
	g_mime_header_list_append (headers, "SUBJECT", NULL, NULL);
	g_mime_header_list_append (headers, "X-After", "after", NULL);
	g_mime_header_list_append (headers, "sUbJeCt", "two", NULL);
	header = g_mime_header_list_get_header_at (headers, 4);
	g_weak_ref_init (&removed, header);
	name = g_mime_header_get_name (header);
	before = header_wire_string (keep, NULL);
	g_mime_header_list_remove_all (headers, name);
	check (g_mime_header_list_get_count (headers) == 2, "bulk removal removes all case variants and unset placeholders");
	check (g_mime_header_list_get_header_at (headers, 0) == keep &&
	       !strcmp (g_mime_header_get_name (g_mime_header_list_get_header_at (headers, 1)), "X-After"),
	       "bulk removal preserves remaining header identity and order");
	check (!g_mime_header_list_contains (headers, "Subject") &&
	       g_mime_header_list_index_of (headers, "Subject") == -1 &&
	       g_mime_header_list_last_index_of (headers, "Subject") == -1 &&
	       g_mime_header_list_get_header (headers, "Subject") == NULL,
	       "bulk removal clears all name and positional lookups");
	header = g_weak_ref_get (&removed);
	check (header == NULL, "bulk removal releases headers even when passed a borrowed header name");
	g_clear_object (&header);
	g_weak_ref_clear (&removed);
	after = header_wire_string (keep, NULL);
	check (!strcmp (before, after), "bulk removal preserves remaining raw header folding");
	g_free (before);
	g_free (after);

	before = g_mime_header_list_to_string (headers, NULL);
	g_mime_header_list_remove_all (headers, "Subject");
	g_mime_header_list_remove_all (headers, "missing");
	g_test_expect_message ("gmime", G_LOG_LEVEL_CRITICAL, "*name != NULL*");
	g_mime_header_list_remove_all (headers, NULL);
	g_test_assert_expected_messages ();
	after = g_mime_header_list_to_string (headers, NULL);
	check (!strcmp (before, after), "missing and invalid bulk-removal names do not change the list");
	g_free (before);
	g_free (after);
	g_mime_header_list_insert (headers, 1, "subject", "replacement", NULL);
	check (g_mime_header_list_get_header (headers, "Subject") == g_mime_header_list_get_header_at (headers, 1),
	       "reinsertion after bulk removal restores name lookup");
	g_mime_header_list_remove_all (headers, "x-keep");
	g_mime_header_list_remove_all (headers, "X-AFTER");
	g_mime_header_list_remove_all (headers, "SUBJECT");
	check (g_mime_header_list_get_count (headers) == 0, "bulk removal can empty the entire list");
	g_object_unref (headers);

	message = g_mime_message_new (FALSE);
	headers = g_mime_entity_get_header_list (GMIME_ENTITY (message));
	g_mime_header_list_append (headers, "Subject", "one", NULL);
	retained = g_object_ref (g_mime_header_list_get_header (headers, "Subject"));
	g_mime_header_list_append (headers, "X-Keep", "keep", NULL);
	g_mime_header_list_append (headers, "sUbJeCt", "two", NULL);
	g_mime_header_list_append (headers, "To", "one@example.com", NULL);
	g_mime_header_list_append (headers, "TO", "two@example.com", NULL);
	g_mime_header_list_remove_all (headers, "subject");
	check (g_mime_message_get_subject (message) == NULL, "bulk removal clears the owning message subject");
	g_mime_header_list_insert (headers, 0, "Subject", "replacement", NULL);
	g_mime_header_set_value (retained, NULL, "detached", NULL);
	check (!g_strcmp0 (g_mime_message_get_subject (message), "replacement"),
	       "headers retained after bulk removal stop notifying their former owner");
	g_object_unref (retained);
	check (g_mime_internet_address_list_length (g_mime_message_get_to (message)) == 2,
	       "duplicate address headers populate the owning message");
	g_mime_header_list_remove_all (headers, "to");
	check (g_mime_internet_address_list_length (g_mime_message_get_to (message)) == 0,
	       "bulk removal updates the owning message address list");
	check (g_mime_header_list_get_count (headers) == 2 && g_mime_header_list_contains (headers, "X-Keep"),
	       "bulk removal leaves unrelated owner headers unchanged");
	g_object_unref (message);
}

static void
test_header_list_instances (void)
{
	static const char raw_message[] = "X-First: first\r\nX-Raw \t: \317\360\350\342\345\362\r\n\tworld\r\n\r\n";
	GMimeHeaderList *headers = g_mime_header_list_new (NULL), *source;
	GMimeHeader *header = g_mime_header_new ("Subject", "caf\303\251", "iso-8859-1");
	GMimeHeader *equal = g_mime_header_clone (header), *original, *address;
	GMimeHeaderRawValueFormatter formatter = header->formatter;
	GMimeMessage *message, *other;
	GMimeParserOptions *options;
	GMimeParser *parser;
	GMimeStream *stream;
	GMimePart *part;
	GWeakRef removed;
	GMimeInternetAddressList *addresses;
	const char *raw_name, *raw_value, *cached_value, *charset;
	gint64 offset;

	check (!g_mime_header_list_contains_header (headers, header) &&
	       g_mime_header_list_index_of_header (headers, header) == -1 &&
	       !g_mime_header_list_remove_header (headers, header), "absent header instances are not found or removed");
	g_test_expect_message ("gmime", G_LOG_LEVEL_CRITICAL, "*GMIME_IS_HEADER (header)*");
	check (g_mime_header_list_add (headers, NULL) == -1, "adding a NULL instance fails");
	g_test_assert_expected_messages ();
	original = g_object_new (GMIME_TYPE_HEADER, NULL);
	g_test_expect_message ("gmime", G_LOG_LEVEL_CRITICAL, "*header->name != NULL*");
	check (g_mime_header_list_add (headers, original) == -1, "adding an uninitialized instance fails");
	g_test_assert_expected_messages ();
	g_object_unref (original);
	check (g_mime_header_list_get_count (headers) == 0, "invalid header instances leave the list unchanged");
	g_mime_header_list_append (headers, "X-First", "first", NULL);
	g_mime_header_list_append (headers, "Subject", "original", NULL);
	original = g_mime_header_list_get_header (headers, "Subject");
	g_mime_header_list_append (headers, "X-Last", "last", NULL);
	raw_name = testsuite_header_raw_name (header);
	raw_value = testsuite_header_raw_text (header);
	cached_value = g_mime_header_get_value (header);
	charset = header->charset;
	g_mime_header_list_insert_header (headers, 1, header);
	check (g_mime_header_list_get_header (headers, "subject") == header &&
	       g_mime_header_list_get_header_at (headers, 1) == header,
	       "instance insertion retains identity and updates first-name lookup");
	check (testsuite_header_raw_name (header) == raw_name && testsuite_header_raw_text (header) == raw_value &&
	       g_mime_header_get_value (header) == cached_value && header->charset == charset && header->formatter == formatter,
	       "instance insertion preserves cached text, nondefault charset and formatter without copying raw text");
	check (g_mime_header_list_add (headers, header) == 4, "adding an existing instance returns its appended index");
	check (g_mime_header_list_index_of_header (headers, header) == 1 &&
	       g_mime_header_list_contains_header (headers, header) &&
	       !g_mime_header_list_contains_header (headers, equal) &&
	       !g_mime_header_list_remove_header (headers, equal), "instance lookup and removal use identity rather than value");
	g_mime_header_list_remove_at (headers, 4);
	check (g_mime_header_list_get_header (headers, "Subject") == header,
	       "removing a later repeated instance preserves the first-name lookup");
	g_mime_header_list_add (headers, header);
	check (g_mime_header_list_remove_header (headers, header) &&
	       g_mime_header_list_index_of_header (headers, header) == 3 &&
	       g_mime_header_list_get_header (headers, "Subject") == original,
	       "identity removal removes only the first occurrence and promotes the next field");
	g_mime_header_list_remove_header (headers, header);
	g_mime_header_list_insert_header (headers, 1, header);
	g_mime_header_list_add (headers, header);
	g_mime_header_list_set (headers, "subject", "single", NULL);
	check (g_mime_header_list_get_count (headers) == 3 &&
	       g_mime_header_list_get_header_at (headers, 1) == header &&
	       g_mime_header_list_last_index_of (headers, "Subject") == 1 &&
	       !strcmp (g_mime_header_get_value (header), "single"),
	       "set collapses repeated instances and distinct duplicates to the first occurrence");
	g_mime_header_list_add (headers, header);
	g_mime_header_list_add (headers, header);
	g_mime_header_list_remove_all (headers, "Subject");
	check (!g_mime_header_list_contains_header (headers, header) && g_mime_header_list_get_count (headers) == 2,
	       "bulk removal removes every repeated instance while retaining unrelated fields");
	g_object_unref (equal);
	g_object_unref (headers);
	g_object_unref (header);

	headers = g_mime_header_list_new (NULL);
	header = g_mime_header_new ("Subject", "shared", NULL);
	g_weak_ref_init (&removed, header);
	g_mime_header_list_add (headers, header);
	g_mime_header_list_add (headers, header);
	g_mime_header_list_add (headers, header);
	g_object_unref (header);
	g_mime_header_list_remove_at (headers, 1);
	header = g_weak_ref_get (&removed);
	check (header != NULL && g_mime_header_list_get_header (headers, "Subject") == header,
	       "each repeated occurrence retains an independent reference");
	g_clear_object (&header);
	g_mime_header_list_clear (headers);
	header = g_weak_ref_get (&removed);
	check (header == NULL, "clearing a list releases its last repeated instance");
	g_clear_object (&header);
	g_weak_ref_clear (&removed);

	options = g_mime_parser_options_new ();
	check (g_mime_parser_options_set_charset_encoding (options, "windows-1251", NULL), "set source header fallback charset");
	stream = g_mime_stream_mem_new_with_buffer (raw_message, strlen (raw_message));
	parser = g_mime_parser_new_with_stream (stream);
	message = g_mime_parser_construct_message (parser, options, NULL);
	g_assert_nonnull (message);
	source = g_mime_entity_get_header_list (GMIME_ENTITY (message));
	header = g_mime_header_list_get_header (source, "X-Raw");
	raw_name = testsuite_header_raw_name (header);
	raw_value = testsuite_header_raw_text (header);
	offset = g_mime_header_get_offset (header);
	check (!header->owns_options && header->value == NULL && offset > 0, "exercise a lazy parsed header borrowing source options");
	g_test_expect_message ("gmime", G_LOG_LEVEL_CRITICAL, "*index >= 0*");
	g_mime_header_list_insert_header (headers, -1, header);
	g_test_assert_expected_messages ();
	g_test_expect_message ("gmime", G_LOG_LEVEL_CRITICAL, "*index*headers->array->len*");
	g_mime_header_list_insert_header (headers, 1, header);
	g_test_assert_expected_messages ();
	check (g_mime_header_list_get_count (headers) == 0 && !header->owns_options,
	       "invalid instance insertion does not mutate either the list or header");
	g_mime_header_list_add (headers, header);
	check (header->owns_options && header->options != source->options &&
	       header->value == NULL && testsuite_header_raw_name (header) == raw_name &&
	       testsuite_header_raw_text (header) == raw_value && g_mime_header_get_offset (header) == offset,
	       "instance insertion snapshots borrowed options without decoding, copying or reformatting raw state");
	check (g_mime_parser_options_set_charset_encoding (source->options, "windows-1252", NULL),
	       "change source list options after instance insertion");
	g_object_unref (message);
	g_object_unref (parser);
	g_object_unref (stream);
	g_mime_parser_options_free (options);
	header = g_object_ref (g_mime_header_list_get_header_at (headers, 0));
	g_object_unref (headers);
	check (!strcmp (g_mime_header_get_value (header), "\320\237\321\200\320\270\320\262\320\265\321\202\tworld"),
	       "shared parsed header decodes using its snapshot after all containing lists are destroyed");
	testsuite_header_set_raw_text (header, " changed\r\n");
	check (!strcmp (g_mime_header_get_value (header), "changed"), "shared header has no stale listeners after list destruction");
	g_object_unref (header);

	message = g_mime_message_new (FALSE);
	other = g_mime_message_new (FALSE);
	headers = g_mime_entity_get_header_list (GMIME_ENTITY (message));
	source = g_mime_entity_get_header_list (GMIME_ENTITY (other));
	header = g_mime_header_new ("Subject", "shared", NULL);
	g_mime_header_list_add (headers, header);
	g_mime_header_list_add (headers, header);
	g_mime_header_list_add (source, header);
	g_mime_header_set_value (header, NULL, "both", NULL);
	check (!g_strcmp0 (g_mime_message_get_subject (message), "both") &&
	       !g_strcmp0 (g_mime_message_get_subject (other), "both"), "shared instance edits notify all owning messages");
	g_mime_header_list_remove_header (headers, header);
	g_mime_header_set_value (header, NULL, "remaining", NULL);
	check (!g_strcmp0 (g_mime_message_get_subject (message), "remaining"),
	       "removing one repeated instance retains the owner's remaining listener");
	g_mime_header_list_remove_all (headers, "Subject");
	g_mime_message_set_subject (message, "replacement", NULL);
	g_mime_header_set_value (header, NULL, "other-only", NULL);
	check (!g_strcmp0 (g_mime_message_get_subject (message), "replacement") &&
	       !g_strcmp0 (g_mime_message_get_subject (other), "other-only"),
	       "removing the last instance detaches only that owner's listener");
	g_object_unref (message);
	g_mime_header_set_value (header, NULL, "survivor", NULL);
	check (!g_strcmp0 (g_mime_message_get_subject (other), "survivor"),
	       "destroying one owner leaves other shared-header listeners working");
	g_mime_header_list_add (source, header);
	g_object_unref (other);
	g_mime_header_set_value (header, NULL, "detached", NULL);
	g_object_unref (header);

	message = g_mime_message_new (FALSE);
	headers = g_mime_entity_get_header_list (GMIME_ENTITY (message));
	header = g_mime_header_new ("To", "one@example.com", NULL);
	address = g_mime_header_new ("To", "two@example.com", NULL);
	g_mime_header_list_add (headers, header);
	g_mime_header_list_add (headers, address);
	g_mime_header_list_insert_header (headers, 0, address);
	addresses = g_mime_message_get_to (message);
	check (g_mime_internet_address_list_length (addresses) == 3 &&
	       !strcmp (g_mime_mailbox_address_get_addrspec (GMIME_MAILBOX_ADDRESS (
		       g_mime_internet_address_list_get_address (addresses, 0))), "two@example.com") &&
	       !strcmp (g_mime_mailbox_address_get_addrspec (GMIME_MAILBOX_ADDRESS (
		       g_mime_internet_address_list_get_address (addresses, 1))), "one@example.com"),
	       "inserting an instance also present at the end rebuilds addresses in header order");
	g_mime_header_list_add (headers, address);
	check (g_mime_internet_address_list_length (addresses) == 4, "appending a repeated instance adds exactly one address occurrence");
	g_mime_header_list_remove_at (headers, 3);
	g_mime_header_list_remove_header (headers, address);
	check (g_mime_internet_address_list_length (addresses) == 2 &&
	       g_mime_header_list_get_header (headers, "To") == header,
	       "removing repeated address instances updates lookup and owner recipients");
	g_object_unref (message);
	g_object_unref (header);
	g_object_unref (address);

	part = g_mime_part_new ();
	headers = g_mime_entity_get_header_list (GMIME_ENTITY (part));
	header = g_mime_header_list_get_header (headers, "Content-Type");
	g_mime_header_list_add (headers, header);
	g_mime_header_list_append (headers, "Content-Type", "text/html", NULL);
	g_mime_header_list_add (headers, header);
	g_mime_entity_set_content_type_parameter (GMIME_ENTITY (part), "charset", "utf-8");
	check (g_mime_header_list_index_of (headers, "Content-Type") ==
	       g_mime_header_list_last_index_of (headers, "Content-Type") &&
	       g_mime_header_list_get_header (headers, "Content-Type") == header,
	       "raw model updates collapse repeated header instances as well as distinct duplicates");
	g_object_unref (part);
}

static void
test_header_list_replace (void)
{
	static const char raw_message[] = "X-First: first\r\nx-rAw \t: \317\360\350\342\345\362\r\n\r\n";
	GMimeHeaderList *headers = g_mime_header_list_new (NULL), *shared;
	GMimeHeader *original = g_mime_header_new ("Subject", "original", NULL);
	GMimeHeader *replacement = g_mime_header_new ("sUbJeCt", "caf\303\251", "iso-8859-1");
	GMimeHeader *duplicate;
	GMimeHeaderRawValueFormatter formatter = replacement->formatter;
	GMimeParserOptions *options;
	GMimeMessage *message, *other;
	GMimeParser *parser;
	GMimeStream *stream;
	GMimePart *part;
	GWeakRef removed;
	const char *raw_name, *raw_value, *value, *charset;
	gint64 offset;

	g_test_expect_message ("gmime", G_LOG_LEVEL_CRITICAL, "*GMIME_IS_HEADER (header)*");
	g_mime_header_list_replace (headers, NULL);
	g_test_assert_expected_messages ();
	duplicate = g_object_new (GMIME_TYPE_HEADER, NULL);
	g_test_expect_message ("gmime", G_LOG_LEVEL_CRITICAL, "*header->name != NULL*");
	g_mime_header_list_replace (headers, duplicate);
	g_test_assert_expected_messages ();
	g_object_unref (duplicate);
	check (g_mime_header_list_get_count (headers) == 0, "invalid replacement instances leave the list unchanged");

	g_mime_header_list_append (headers, "X-First", "first", NULL);
	g_mime_header_list_replace (headers, original);
	check (g_mime_header_list_get_count (headers) == 2 && g_mime_header_list_get_header_at (headers, 1) == original,
	       "replacement appends the supplied instance when its field is absent");
	g_mime_header_list_append (headers, "X-Last", "last", NULL);
	g_mime_header_list_append (headers, "SUBJECT", "duplicate", NULL);
	duplicate = g_mime_header_list_get_header_at (headers, 3);
	g_weak_ref_init (&removed, duplicate);
	raw_name = testsuite_header_raw_name (replacement);
	raw_value = testsuite_header_raw_text (replacement);
	value = g_mime_header_get_value (replacement);
	charset = replacement->charset;
	g_mime_header_list_replace (headers, replacement);
	check (g_mime_header_list_get_count (headers) == 3 &&
	       g_mime_header_list_get_header_at (headers, 1) == replacement &&
	       g_mime_header_list_get_header (headers, "SUBJECT") == replacement &&
	       !strcmp (g_mime_header_get_name (g_mime_header_list_get_header_at (headers, 2)), "X-Last"),
	       "replacement preserves the first field position and unrelated order while collapsing duplicates");
	check (!strcmp (g_mime_header_get_value (original), "original") &&
	       !g_mime_header_list_contains_header (headers, original), "replacement does not mutate the detached original");
	check (testsuite_header_raw_name (replacement) == raw_name && testsuite_header_raw_text (replacement) == raw_value &&
	       g_mime_header_get_value (replacement) == value && replacement->charset == charset && replacement->formatter == formatter,
	       "replacement preserves raw text, cached value, charset and formatter");
	duplicate = g_weak_ref_get (&removed);
	check (duplicate == NULL, "replacement releases discarded duplicate references");
	g_clear_object (&duplicate);
	g_weak_ref_clear (&removed);
	g_mime_header_list_add (headers, replacement);
	g_mime_header_list_add (headers, replacement);
	g_mime_header_list_replace (headers, replacement);
	check (g_mime_header_list_get_count (headers) == 3 &&
	       g_mime_header_list_index_of_header (headers, replacement) == 1 &&
	       g_mime_header_list_last_index_of (headers, "Subject") == 1,
	       "self-replacement collapses repeated occurrences of the original first instance");
	g_weak_ref_init (&removed, replacement);
	g_object_unref (replacement);
	g_mime_header_list_append (headers, "Subject", "borrowed-later", NULL);
	replacement = g_mime_header_list_get_header_at (headers, 3);
	check (!replacement->owns_options, "exercise a replacement borrowed from a later duplicate");
	g_mime_header_list_replace (headers, replacement);
	check (g_mime_header_list_get_count (headers) == 3 && replacement->owns_options &&
	       g_mime_header_list_get_header_at (headers, 1) == replacement &&
	       !strcmp (g_mime_header_get_value (replacement), "borrowed-later"),
	       "replacement retains a later borrowed instance before removing its occurrence");
	duplicate = g_weak_ref_get (&removed);
	check (duplicate == NULL, "replacement releases the original first slot's reference");
	g_clear_object (&duplicate);
	g_weak_ref_clear (&removed);
	replacement = g_object_ref (replacement);
	g_object_unref (headers);
	g_mime_header_set_value (replacement, NULL, "detached", NULL);
	g_object_unref (replacement);
	g_object_unref (original);

	options = g_mime_parser_options_new ();
	check (g_mime_parser_options_set_charset_encoding (options, "windows-1251", NULL), "set replacement source fallback charset");
	stream = g_mime_stream_mem_new_with_buffer (raw_message, strlen (raw_message));
	parser = g_mime_parser_new_with_stream (stream);
	message = g_mime_parser_construct_message (parser, options, NULL);
	g_assert_nonnull (message);
	shared = g_mime_entity_get_header_list (GMIME_ENTITY (message));
	replacement = g_mime_header_list_get_header (shared, "X-Raw");
	raw_name = testsuite_header_raw_name (replacement);
	raw_value = testsuite_header_raw_text (replacement);
	offset = g_mime_header_get_offset (replacement);
	headers = g_mime_header_list_new (NULL);
	g_mime_header_list_append (headers, "X-Raw", NULL, NULL);
	g_mime_header_list_replace (headers, replacement);
	check (replacement->owns_options && replacement->value == NULL && offset > 0 &&
	       testsuite_header_raw_name (replacement) == raw_name && testsuite_header_raw_text (replacement) == raw_value &&
	       g_mime_header_get_offset (replacement) == offset, "replacement preserves lazy parsed state and stream offset");
	g_object_unref (message);
	g_object_unref (parser);
	g_object_unref (stream);
	g_mime_parser_options_free (options);
	check (!strcmp (g_mime_header_get_value (g_mime_header_list_get_header_at (headers, 0)),
			"\320\237\321\200\320\270\320\262\320\265\321\202"),
	       "replacement snapshots source parser options for decoding after the source is destroyed");
	g_object_unref (headers);

	message = g_mime_message_new (FALSE);
	other = g_mime_message_new (FALSE);
	headers = g_mime_entity_get_header_list (GMIME_ENTITY (message));
	shared = g_mime_entity_get_header_list (GMIME_ENTITY (other));
	original = g_mime_header_new ("Subject", "original", NULL);
	replacement = g_mime_header_new ("Subject", "replacement", NULL);
	g_mime_header_list_add (headers, original);
	g_mime_header_list_add (headers, original);
	g_mime_header_list_add (shared, original);
	g_mime_header_list_add (shared, replacement);
	g_mime_header_list_replace (headers, replacement);
	check (g_mime_header_list_get_count (headers) == 1 &&
	       !g_strcmp0 (g_mime_message_get_subject (message), "replacement"),
	       "replacement updates the owning message subject after collapsing repeated instances");
	g_mime_header_set_value (original, NULL, "other-only", NULL);
	check (!g_strcmp0 (g_mime_message_get_subject (message), "replacement") &&
	       !g_strcmp0 (g_mime_message_get_subject (other), "other-only"),
	       "replacement detaches removed instances only from the receiving list");
	g_mime_header_list_replace (headers, replacement);
	check (!g_strcmp0 (g_mime_message_get_subject (other), "other-only"),
	       "a shared duplicate does not override the first subject");
	g_mime_header_list_remove_at (shared, 0);
	g_mime_header_set_value (replacement, NULL, "both", NULL);
	check (!g_strcmp0 (g_mime_message_get_subject (message), "both") &&
	       !g_strcmp0 (g_mime_message_get_subject (other), "both"),
	       "self-replacement keeps the replacement's listeners in all containing lists");
	g_object_unref (message);
	g_mime_header_set_value (replacement, NULL, "survivor", NULL);
	check (!g_strcmp0 (g_mime_message_get_subject (other), "survivor"), "shared replacement survives the receiving owner");
	g_object_unref (other);
	g_mime_header_set_value (original, NULL, "detached-original", NULL);
	g_mime_header_set_value (replacement, NULL, "detached-replacement", NULL);
	g_object_unref (original);
	g_object_unref (replacement);

	message = g_mime_message_new (FALSE);
	headers = g_mime_entity_get_header_list (GMIME_ENTITY (message));
	replacement = g_mime_header_new ("To", "new@example.com", NULL);
	g_mime_header_list_append (headers, "X-First", "first", NULL);
	g_mime_header_list_append (headers, "To", "old@example.com", NULL);
	g_mime_header_list_replace (headers, replacement);
	check (g_mime_internet_address_list_length (g_mime_message_get_to (message)) == 1 &&
	       !strcmp (g_mime_mailbox_address_get_addrspec (GMIME_MAILBOX_ADDRESS (
		       g_mime_internet_address_list_get_address (g_mime_message_get_to (message), 0))), "new@example.com"),
	       "replacing an address field at the tail rebuilds recipients without double-appending");
	g_mime_header_list_append (headers, "To", "duplicate@example.com", NULL);
	g_mime_header_list_add (headers, replacement);
	g_mime_header_list_replace (headers, replacement);
	check (g_mime_internet_address_list_length (g_mime_message_get_to (message)) == 1 &&
	       g_mime_header_list_get_count (headers) == 2, "replacing repeated address instances collapses all cached recipients too");
	g_object_unref (message);
	g_object_unref (replacement);

	part = g_mime_part_new ();
	headers = g_mime_entity_get_header_list (GMIME_ENTITY (part));
	g_mime_header_list_append (headers, "Content-Type", "text/html", NULL);
	replacement = g_mime_header_new ("Content-Type", "application/json", NULL);
	g_mime_header_list_replace (headers, replacement);
	check (g_mime_content_type_is_type (g_mime_entity_get_content_type (GMIME_ENTITY (part)), "application", "json") &&
	       g_mime_header_list_index_of (headers, "Content-Type") == g_mime_header_list_last_index_of (headers, "Content-Type"),
	       "header replacement updates entity content models");
	g_object_unref (part);
	g_object_unref (replacement);
}

static void
test_header_list_set_header_at (void)
{
	static const char raw_message[] = "X-First: first\r\nx-rAw \t: \317\360\350\342\345\362\r\n\r\n";
	GMimeHeaderList *headers = g_mime_header_list_new (NULL), *shared;
	GMimeHeader *original, *replacement = g_mime_header_new ("sUbJeCt", "caf\303\251", "iso-8859-1"), *note, *retained;
	GMimeHeaderRawValueFormatter formatter = replacement->formatter;
	GMimeParserOptions *options;
	GMimeMessage *message, *other;
	GMimeParser *parser;
	GMimeStream *stream;
	GMimeInternetAddressList *addresses;
	GWeakRef removed;
	const char *raw_name, *raw_value, *value, *charset;
	gint64 offset;

	g_test_expect_message ("gmime", G_LOG_LEVEL_CRITICAL, "*index*headers->array->len*");
	g_mime_header_list_set_header_at (headers, 0, replacement);
	g_test_assert_expected_messages ();
	g_mime_header_list_append (headers, "Subject", "original", NULL);
	original = g_mime_header_list_get_header_at (headers, 0);
	g_mime_header_list_set_header_at (headers, 0, original);
	check (!original->owns_options, "self-assignment does not change borrowed parser-option ownership");
	g_test_expect_message ("gmime", G_LOG_LEVEL_CRITICAL, "*index >= 0*");
	g_mime_header_list_set_header_at (headers, -1, replacement);
	g_test_assert_expected_messages ();
	g_test_expect_message ("gmime", G_LOG_LEVEL_CRITICAL, "*GMIME_IS_HEADER (header)*");
	g_mime_header_list_set_header_at (headers, 0, NULL);
	g_test_assert_expected_messages ();
	check (g_mime_header_list_get_header_at (headers, 0) == original &&
	       g_mime_header_list_get_count (headers) == 1, "invalid indexed assignments leave the list unchanged");
	g_mime_header_list_append (headers, "X-One", "note", NULL);
	g_mime_header_list_add (headers, original);
	g_mime_header_list_append (headers, "X-Last", "last", NULL);
	raw_name = testsuite_header_raw_name (replacement);
	raw_value = testsuite_header_raw_text (replacement);
	value = g_mime_header_get_value (replacement);
	charset = replacement->charset;
	g_mime_header_list_set_header_at (headers, 2, replacement);
	check (g_mime_header_list_get_count (headers) == 4 &&
	       g_mime_header_list_get_header_at (headers, 2) == replacement &&
	       g_mime_header_list_get_header (headers, "Subject") == original,
	       "indexed assignment changes one slot and preserves earlier lookup for a repeated original");
	check (testsuite_header_raw_name (replacement) == raw_name && testsuite_header_raw_text (replacement) == raw_value &&
	       g_mime_header_get_value (replacement) == value && replacement->charset == charset && replacement->formatter == formatter,
	       "indexed assignment preserves raw text, cached value, charset and formatter");
	g_weak_ref_init (&removed, original);
	g_mime_header_list_set_header_at (headers, 0, replacement);
	original = g_weak_ref_get (&removed);
	check (original == NULL && g_mime_header_list_get_header (headers, "Subject") == replacement,
	       "indexed assignment releases the original slot and updates the first matching instance");
	g_clear_object (&original);
	g_weak_ref_clear (&removed);
	note = g_mime_header_list_get_header (headers, "X-One");
	retained = g_mime_header_new ("X-One", "new-note", NULL);
	g_mime_header_list_set_header_at (headers, 2, retained);
	check (g_mime_header_list_get_header (headers, "Subject") == replacement &&
	       g_mime_header_list_get_header (headers, "X-One") == note,
	       "different-name assignment preserves earlier matches for both affected fields");
	g_mime_header_list_set_header_at (headers, 0, retained);
	check (!g_mime_header_list_contains (headers, "Subject") &&
	       g_mime_header_list_get_header (headers, "X-One") == retained &&
	       g_mime_header_list_get_count (headers) == 4, "different-name assignment removes absent lookup and promotes a new first match");
	g_mime_header_list_set_header_at (headers, 2, replacement);
	check (g_mime_header_list_get_header (headers, "X-One") == retained &&
	       g_mime_header_list_get_header (headers, "Subject") == replacement,
	       "changing a later repeated instance to another field does not lose the earlier name lookup");
	g_object_unref (headers);
	g_mime_header_set_value (retained, NULL, "detached-note", NULL);
	g_mime_header_set_value (replacement, NULL, "detached-subject", NULL);
	g_object_unref (retained);
	g_object_unref (replacement);

	options = g_mime_parser_options_new ();
	check (g_mime_parser_options_set_charset_encoding (options, "windows-1251", NULL), "set indexed replacement source fallback charset");
	stream = g_mime_stream_mem_new_with_buffer (raw_message, strlen (raw_message));
	parser = g_mime_parser_new_with_stream (stream);
	message = g_mime_parser_construct_message (parser, options, NULL);
	g_assert_nonnull (message);
	shared = g_mime_entity_get_header_list (GMIME_ENTITY (message));
	replacement = g_mime_header_list_get_header (shared, "X-Raw");
	raw_name = testsuite_header_raw_name (replacement);
	raw_value = testsuite_header_raw_text (replacement);
	offset = g_mime_header_get_offset (replacement);
	headers = g_mime_header_list_new (NULL);
	g_mime_header_list_append (headers, "Subject", NULL, NULL);
	g_test_expect_message ("gmime", G_LOG_LEVEL_CRITICAL, "*index*headers->array->len*");
	g_mime_header_list_set_header_at (headers, 1, replacement);
	g_test_assert_expected_messages ();
	check (!replacement->owns_options, "out-of-range assignment does not snapshot the supplied header's options");
	g_mime_header_list_set_header_at (headers, 0, replacement);
	check (replacement->owns_options && replacement->value == NULL && offset > 0 &&
	       testsuite_header_raw_name (replacement) == raw_name && testsuite_header_raw_text (replacement) == raw_value &&
	       g_mime_header_get_offset (replacement) == offset, "indexed assignment preserves lazy parsed state and stream offset");
	g_object_unref (message);
	g_object_unref (parser);
	g_object_unref (stream);
	g_mime_parser_options_free (options);
	check (!strcmp (g_mime_header_get_value (g_mime_header_list_get_header_at (headers, 0)),
			"\320\237\321\200\320\270\320\262\320\265\321\202"),
	       "indexed assignment snapshots parser options before the source is destroyed");
	g_object_unref (headers);

	message = g_mime_message_new (FALSE);
	other = g_mime_message_new (FALSE);
	headers = g_mime_entity_get_header_list (GMIME_ENTITY (message));
	shared = g_mime_entity_get_header_list (GMIME_ENTITY (other));
	original = g_mime_header_new ("Subject", "original", NULL);
	replacement = g_mime_header_new ("Subject", "replacement", NULL);
	g_mime_header_list_add (headers, original);
	g_mime_header_list_add (shared, original);
	g_mime_header_list_set_header_at (headers, 0, replacement);
	g_mime_header_set_value (original, NULL, "other-only", NULL);
	check (!g_strcmp0 (g_mime_message_get_subject (message), "replacement") &&
	       !g_strcmp0 (g_mime_message_get_subject (other), "other-only"),
	       "indexed assignment detaches only the replaced occurrence's owner listener");
	g_mime_header_list_add (shared, replacement);
	check (!g_strcmp0 (g_mime_message_get_subject (other), "other-only"),
	       "adding a shared duplicate preserves the first subject");
	g_mime_header_list_remove_at (shared, 0);
	g_mime_header_set_value (replacement, NULL, "both", NULL);
	check (!g_strcmp0 (g_mime_message_get_subject (message), "both") &&
	       !g_strcmp0 (g_mime_message_get_subject (other), "both"), "indexed replacement remains editable across containing lists");
	note = g_mime_header_new ("X-Note", "note", NULL);
	g_mime_header_list_set_header_at (headers, 0, note);
	check (g_mime_message_get_subject (message) == NULL, "changing the indexed field name clears the original owner cache");
	g_object_unref (message);
	g_mime_header_set_value (replacement, NULL, "survivor", NULL);
	check (!g_strcmp0 (g_mime_message_get_subject (other), "survivor"), "indexed replacement survives destruction of one owner");
	g_object_unref (other);
	g_mime_header_set_value (original, NULL, "detached-original", NULL);
	g_mime_header_set_value (replacement, NULL, "detached-replacement", NULL);
	g_mime_header_set_value (note, NULL, "detached-note", NULL);
	g_object_unref (original);
	g_object_unref (replacement);
	g_object_unref (note);

	message = g_mime_message_new (FALSE);
	headers = g_mime_entity_get_header_list (GMIME_ENTITY (message));
	g_mime_header_list_append (headers, "X-First", "first", NULL);
	g_mime_header_list_append (headers, "Cc", "earlier@example.com", NULL);
	g_mime_header_list_append (headers, "To", "old@example.com", NULL);
	replacement = g_mime_header_new ("Cc", "last@example.com", NULL);
	g_mime_header_list_set_header_at (headers, 2, replacement);
	check (g_mime_internet_address_list_length (g_mime_message_get_to (message)) == 0 &&
	       g_mime_internet_address_list_length (g_mime_message_get_cc (message)) == 2,
	       "different-name assignment at the tail removes old recipients and appends new recipients exactly once");
	original = g_mime_header_new ("To", "new@example.com", NULL);
	g_mime_header_list_set_header_at (headers, 1, original);
	g_mime_header_list_set_header_at (headers, 2, original);
	g_mime_header_list_set_header_at (headers, 1, original);
	check (g_mime_internet_address_list_length (g_mime_message_get_cc (message)) == 0 &&
	       g_mime_internet_address_list_length (g_mime_message_get_to (message)) == 2,
	       "indexed assignment of a repeated address instance rebuilds recipients and self-assignment is a no-op");
	retained = g_mime_header_new ("to", "other@example.com", NULL);
	g_mime_header_list_set_header_at (headers, 2, retained);
	addresses = g_mime_message_get_to (message);
	check (g_mime_internet_address_list_length (addresses) == 2 &&
	       !strcmp (g_mime_mailbox_address_get_addrspec (GMIME_MAILBOX_ADDRESS (
		       g_mime_internet_address_list_get_address (addresses, 1))), "other@example.com") &&
	       g_mime_header_list_get_header (headers, "To") == original,
	       "same-name indexed assignment at the tail rebuilds recipient values without double-appending");
	g_object_unref (message);
	g_object_unref (original);
	g_object_unref (replacement);
	g_object_unref (retained);
}

static void
test_content (void)
{
	const char encoded[] = "SGVsbG8sIHdvcmxkIQ==";
	const char decoded[] = "Hello, world!";
	GMimeStream *input, *output;
	GMimeContent *content;
	GByteArray *bytes;

	input = g_mime_stream_mem_new_with_buffer (encoded, strlen (encoded));
	content = g_mime_content_new_with_stream (input, GMIME_CONTENT_ENCODING_BASE64);
	g_object_unref (input);

	check (GMIME_IS_CONTENT (content), "content type");
	check (g_mime_content_get_encoding (content) == GMIME_CONTENT_ENCODING_BASE64, "content encoding");

	output = g_mime_stream_mem_new ();
	check (g_mime_content_write_to_stream (content, output) == strlen (encoded), "content raw encoded length");
	bytes = g_mime_stream_mem_get_byte_array (GMIME_STREAM_MEM (output));
	check (bytes->len == strlen (encoded) && !memcmp (bytes->data, encoded, bytes->len),
	       "content raw copying preserves transfer-encoded bytes");
	g_object_unref (output);

	output = g_mime_stream_mem_new ();
	check (g_mime_content_decode_to_stream (content, output) == strlen (decoded), "content decoded length");
	bytes = g_mime_stream_mem_get_byte_array (GMIME_STREAM_MEM (output));
	check (bytes->len == strlen (decoded) && !memcmp (bytes->data, decoded, bytes->len),
	       "content retains stream reference and decodes bytes");

	g_object_unref (output);
	g_object_unref (content);
}

static void
test_content_open (void)
{
	static const struct {
		GMimeContentEncoding encoding;
		const char *input;
		const char *expected;
		gsize length;
	} cases[] = {
		{ GMIME_CONTENT_ENCODING_UNSPECIFIED, "raw\001\377", "raw\001\377", 5 },
		{ GMIME_CONTENT_ENCODING_7BIT, "raw\r\n", "raw\r\n", 5 },
		{ GMIME_CONTENT_ENCODING_8BIT, "raw\377\n", "raw\377\n", 5 },
		{ GMIME_CONTENT_ENCODING_BINARY, "raw\001\377", "raw\001\377", 5 },
		{ GMIME_CONTENT_ENCODING_BASE64, "", "", 0 },
		{ GMIME_CONTENT_ENCODING_BASE64, " \r\n\t", "", 0 },
		{ GMIME_CONTENT_ENCODING_BASE64, "SGVsbG8=", "Hello", 5 },
		{ GMIME_CONTENT_ENCODING_BASE64, " A P9B\r\n", "\000\377A", 3 },
		{ GMIME_CONTENT_ENCODING_QUOTEDPRINTABLE, "", "", 0 },
		{ GMIME_CONTENT_ENCODING_QUOTEDPRINTABLE, "=\r\n=\n", "", 0 },
		{ GMIME_CONTENT_ENCODING_QUOTEDPRINTABLE, "a=00=FF=\r\nb", "a\000\377b", 4 },
		{ GMIME_CONTENT_ENCODING_QUOTEDPRINTABLE, "a_b=20c", "a_b c", 5 },
		{ GMIME_CONTENT_ENCODING_UUENCODE, "", "", 0 },
		{ GMIME_CONTENT_ENCODING_UUENCODE, "begin 644 file\n#0V%T\n`\nend\n", "Cat", 3 }
	};
	static const gsize sizes[] = { 1, 7, 4096 };
	GMimeStream *source, *input, *reader, *output, *replacement;
	GMimeContent *content;
	TestStreamFailure *failure;
	GByteArray *bytes;
	char buffer[4096], *padded;
	gpointer weak;
	ssize_t nread;
	guint i, j;
	int onebyte;

	for (i = 0; i < G_N_ELEMENTS (cases); i++) {
		for (onebyte = 0; onebyte < 2; onebyte++) {
			for (j = 0; j < G_N_ELEMENTS (sizes); j++) {
				source = g_mime_stream_mem_new_with_buffer (cases[i].input, strlen (cases[i].input));
				input = onebyte ? test_stream_onebyte_new (source) : g_object_ref (source);
				content = g_mime_content_new_with_stream (input, cases[i].encoding);
				output = g_mime_stream_mem_new ();
				check (g_mime_content_write_to_stream (content, output) == strlen (cases[i].input) &&
				       g_mime_stream_tell (input) == 0,
				       "raw content copies preserve encoded length and rewind on success");
				bytes = g_mime_stream_mem_get_byte_array (GMIME_STREAM_MEM (output));
				check (bytes->len == strlen (cases[i].input) &&
				       (!bytes->len || !memcmp (bytes->data, cases[i].input, bytes->len)),
				       "raw content copying never decodes or normalizes bytes");
				g_object_unref (output);
				output = g_mime_stream_mem_new ();
				check (g_mime_content_decode_to_stream (content, output) == cases[i].length &&
				       g_mime_stream_tell (input) == 0,
				       "decoded copies report decoded length and rewind one-byte and buffered sources");
				bytes = g_mime_stream_mem_get_byte_array (GMIME_STREAM_MEM (output));
				check (bytes->len == cases[i].length &&
				       (!bytes->len || !memcmp (bytes->data, cases[i].expected, bytes->len)),
				       "decoded copies match decoded reads for every supported encoding");
				g_object_unref (output);
				g_mime_stream_seek (input, 0, GMIME_STREAM_SEEK_END);
				reader = g_mime_content_open (content);
				check (reader && reader != input && g_mime_stream_tell (input) == 0,
				       "opening decoded content creates a distinct reader and rewinds its source");
				g_assert_nonnull (reader);
				g_mime_content_set_encoding (content, GMIME_CONTENT_ENCODING_BINARY);
				g_object_unref (content);
				bytes = g_byte_array_new ();
				while ((nread = g_mime_stream_read (reader, buffer, sizes[j])) > 0)
					g_byte_array_append (bytes, (const guint8 *) buffer, nread);
				check (nread == 0 && g_mime_stream_eos (reader) && bytes->len == cases[i].length &&
				       (!bytes->len || !memcmp (bytes->data, cases[i].expected, bytes->len)),
				       "decoded readers retain sources and encoding state across one-byte and buffered reads");
				check (g_mime_stream_read (reader, buffer, 1) == 0,
				       "decoded EOF is stable on repeated reads");
				check (g_mime_stream_reset (reader) == 0 &&
				       g_mime_stream_read (reader, buffer, 0) == 0 && g_mime_stream_tell (input) == 0,
				       "reset rewinds the source and zero-length reads do not consume it");
				output = g_mime_stream_mem_new ();
				check (g_mime_stream_write_to_stream (reader, output) == cases[i].length,
				       "resetting a decoded reader resets its decoder and buffered output");
				check (g_mime_stream_close (reader) == 0 && g_mime_stream_reset (input) == 0,
				       "closing a decoded reader does not close the shared source");
				g_object_unref (output);
				g_byte_array_unref (bytes);
				g_object_unref (reader);
				g_object_unref (input);
				g_object_unref (source);
			}
		}
	}

	padded = g_malloc (8202);
	memset (padded, ' ', 8193);
	memcpy (padded + 8193, "SGVsbG8=", 9);
	source = g_mime_stream_mem_new_with_buffer (padded, 8201);
	g_free (padded);
	content = g_mime_content_new_with_stream (source, GMIME_CONTENT_ENCODING_BASE64);
	reader = g_mime_content_open (content);
	check (g_mime_stream_read (reader, buffer, sizeof (buffer)) == 5 && !memcmp (buffer, "Hello", 5),
	       "decoded reads skip multiple complete input chunks with no decoder output");
	check (g_mime_stream_read (reader, buffer, 1) == 0 && g_mime_stream_eos (reader),
	       "decoded readers reach EOF after ignored input and decoded data");
	g_object_unref (reader);
	g_object_unref (content);
	g_object_unref (source);

	source = g_mime_stream_mem_new_with_buffer ("preSGVsbG8=post", 15);
	input = g_mime_stream_substream (source, 3, 11);
	content = g_mime_content_new_with_stream (input, GMIME_CONTENT_ENCODING_BASE64);
	reader = g_mime_content_open (content);
	check (g_mime_stream_read (reader, buffer, sizeof (buffer)) == 5 && !memcmp (buffer, "Hello", 5) &&
	       g_mime_stream_read (reader, buffer, 1) == 0,
	       "decoded readers respect nonzero source bounds");
	check (g_mime_stream_tell (input) == 11 && g_mime_stream_close (reader) == 0 &&
	       g_mime_stream_tell (input) == 11,
	       "closing a decoded reader leaves the source at its current position");
	g_object_unref (reader);
	g_object_unref (content);
	g_object_unref (input);
	g_object_unref (source);

	source = g_mime_stream_mem_new_with_buffer ("SGVsbG8=", 8);
	weak = source;
	g_object_add_weak_pointer (G_OBJECT (source), &weak);
	content = g_mime_content_new_with_stream (source, GMIME_CONTENT_ENCODING_BASE64);
	reader = g_mime_content_open (content);
	replacement = g_mime_stream_mem_new ();
	g_mime_content_set_stream (content, replacement);
	g_object_unref (replacement);
	g_object_unref (source);
	g_object_unref (content);
	check (weak != NULL && g_mime_stream_read (reader, buffer, sizeof (buffer)) == 5,
	       "replacing the content stream does not invalidate an existing decoded reader");
	g_object_unref (reader);
	check (weak == NULL, "releasing a decoded reader releases its retained source");

	failure = g_object_new (test_stream_failure_get_type (), NULL);
	failure->parent_instance.source = g_mime_stream_mem_new_with_buffer ("SGVsbG8=", 8);
	g_mime_stream_construct (GMIME_STREAM (failure), 0, -1);
	failure->fail_after = G_MAXSIZE;
	content = g_mime_content_new_with_stream (GMIME_STREAM (failure), GMIME_CONTENT_ENCODING_BASE64);
	reader = g_mime_content_open (content);
	check (reader && failure->read_calls == 0 && failure->reset_calls == 1,
	       "opening decoded content rewinds without eagerly reading or decoding");
	g_assert_nonnull (reader);
	g_mime_stream_close (failure->parent_instance.source);
	errno = 0;
	check (g_mime_stream_read (reader, buffer, sizeof (buffer)) == -1 && errno == EBADF,
	       "decoded reads report source errors even when the source also reports EOF");
	g_object_unref (reader);
	replacement = g_mime_stream_mem_new_with_buffer ("SGVsbG8=", 8);
	g_object_unref (failure->parent_instance.source);
	failure->parent_instance.source = replacement;
	failure->fail_after = 4;
	failure->onebyte = TRUE;
	reader = g_mime_content_open (content);
	while ((nread = g_mime_stream_read (reader, buffer, sizeof (buffer))) > 0)
		;
	check (nread == -1 && errno == EIO, "decoded read errors after partial output propagate unchanged");
	g_object_unref (reader);

	failure->fail_reset_on = failure->reset_calls + 1;
	errno = 0;
	check (g_mime_content_open (content) == NULL && errno == ESPIPE,
	       "opening decoded content reports a failed rewind instead of returning a partial reader");
	output = g_mime_stream_mem_new ();
	failure->fail_reset_on = failure->reset_calls + 1;
	errno = 0;
	check (g_mime_content_decode_to_stream (content, output) == -1 && errno == ESPIPE,
	       "decoded copying reports an initial rewind failure");
	failure->fail_after = G_MAXSIZE;
	failure->fail_reset_on = failure->reset_calls + 2;
	failure->bytes_read = 0;
	errno = 0;
	check (g_mime_content_decode_to_stream (content, output) == -1 && errno == ESPIPE,
	       "decoded copying reports a final rewind failure");
	failure->fail_after = 0;
	failure->bytes_read = 0;
	failure->fail_reset_on = failure->reset_calls + 2;
	errno = 0;
	check (g_mime_content_decode_to_stream (content, output) == -1 && errno == EIO &&
	       failure->reset_calls + 1 == failure->fail_reset_on,
	       "decoded copying preserves a read error without attempting a cleanup rewind");
	g_object_unref (output);
	g_object_unref (content);
	g_object_unref (failure);
}

static void
test_content_md5_errors (void)
{
	GMimePart *part = g_mime_part_new_with_type ("text", "plain");
	GMimeEntity *entity = GMIME_ENTITY (part);
	GMimeContent *content = g_mime_content_new ();
	TestStreamFailure *source = g_object_new (test_stream_failure_get_type (), NULL);
	TestContent *custom;
	GMimeStream *bounded, *memory;
	GMimeHeader *header;
	GError *error = NULL;
	char *digest, *raw;
	guint i;
	const char *blank[] = { NULL, "", " \t\r\n" };

	check (g_mime_part_compute_content_md5 (part, &error) == NULL &&
	       g_error_matches (error, GMIME_ERROR, GMIME_ERROR_INVALID_OPERATION),
	       "computing Content-MD5 without content reports an invalid operation");
	g_clear_error (&error);
	g_mime_part_set_content_md5 (part, "digest");
	check (!g_mime_part_verify_content_md5 (part, &error) && error == NULL,
	       "verification without content is an ordinary negative result");
	g_mime_part_set_content (part, content);
	check (g_mime_part_compute_content_md5 (part, &error) == NULL &&
	       g_error_matches (error, GMIME_ERROR, GMIME_ERROR_INVALID_OPERATION),
	       "computing Content-MD5 without a backing stream reports an invalid operation");
	g_clear_error (&error);
	check (!g_mime_part_verify_content_md5 (part, &error) &&
	       g_error_matches (error, GMIME_ERROR, GMIME_ERROR_INVALID_OPERATION),
	       "verification propagates a missing backing stream instead of reporting a mismatch");
	g_clear_error (&error);

	source->parent_instance.source = g_mime_stream_mem_new_with_buffer ("abc", 3);
	g_mime_stream_construct (GMIME_STREAM (source), 0, -1);
	source->fail_after = G_MAXSIZE;
	source->onebyte = TRUE;
	g_mime_content_set_stream (content, GMIME_STREAM (source));
	for (i = 0; i < G_N_ELEMENTS (blank); i++) {
		g_mime_part_set_content_md5 (part, blank[i]);
		check (!g_mime_part_verify_content_md5 (part, &error) && error == NULL &&
		       source->reset_calls == 0 && source->read_calls == 0,
		       "missing and ASCII-blank digests skip all content I/O");
	}
	g_mime_part_set_content_md5 (part, "kAFQmDzST7DWlj99KOF/cg==");
	header = g_mime_header_list_get_header (g_mime_entity_get_header_list (entity), "Content-Md5");
	raw = g_strdup (testsuite_header_raw_text (header));
	source->fail_reset_on = 1;
	check (g_mime_part_compute_content_md5 (part, &error) == NULL &&
	       g_error_matches (error, GMIME_ERROR, ESPIPE) && source->read_calls == 0,
	       "Content-MD5 computation reports a rewind failure before reading");
	g_clear_error (&error);
	source->fail_reset_on = source->reset_calls + 1;
	check (!g_mime_part_verify_content_md5 (part, &error) &&
	       g_error_matches (error, GMIME_ERROR, ESPIPE),
	       "Content-MD5 verification propagates rewind failures");
	g_clear_error (&error);
	source->fail_after = 1;
	check (g_mime_part_compute_content_md5 (part, &error) == NULL &&
	       g_error_matches (error, GMIME_ERROR, EIO) && source->bytes_read == 1,
	       "a read failure never returns a Content-MD5 of the consumed prefix");
	g_clear_error (&error);
	source->bytes_read = 0;
	check (!g_mime_part_verify_content_md5 (part, &error) &&
	       g_error_matches (error, GMIME_ERROR, EIO),
	       "verification distinguishes a read failure from an ordinary mismatch");
	g_clear_error (&error);
	source->unset_error = TRUE;
	source->bytes_read = 0;
	errno = EACCES;
	check (g_mime_part_compute_content_md5 (part, &error) == NULL &&
	       g_error_matches (error, GMIME_ERROR, GMIME_ERROR_GENERAL),
	       "read failures without errno report an explicit general error, not stale errno");
	g_clear_error (&error);
	source->fail_reset_on = source->reset_calls + 1;
	errno = EACCES;
	check (g_mime_part_compute_content_md5 (part, &error) == NULL &&
	       g_error_matches (error, GMIME_ERROR, GMIME_ERROR_GENERAL),
	       "rewind failures without errno report an explicit general error");
	g_clear_error (&error);
	source->unset_error = FALSE;
	source->fail_after = G_MAXSIZE;
	source->bytes_read = 0;
	source->zero_read = TRUE;
	check (g_mime_part_compute_content_md5 (part, &error) == NULL &&
	       g_error_matches (error, GMIME_ERROR, EIO),
	       "a zero-byte read without EOF is an error, not a successful partial hash");
	g_clear_error (&error);

	digest = g_mime_part_compute_content_md5 (part, &error);
	check (error == NULL && !g_strcmp0 (digest, "kAFQmDzST7DWlj99KOF/cg==") &&
	       g_mime_stream_tell (GMIME_STREAM (source)) == 3 &&
	       source->close_calls == 0 && source->flush_calls == 0 &&
	       !g_strcmp0 (raw, testsuite_header_raw_text (header)),
	       "retry rewinds after failure, leaves the source open at EOF and preserves raw headers");
	g_free (digest);
	source->bytes_read = 0;
	check (g_mime_part_verify_content_md5 (part, &error) && error == NULL,
	       "verification can be retried after read and rewind failures");
	g_free (raw);
	g_object_unref (part);
	g_object_unref (content);
	g_object_unref (source);

	memory = g_mime_stream_mem_new_with_buffer ("preYWJjpost", 11);
	bounded = g_mime_stream_substream (memory, 3, 7);
	custom = g_object_new (test_content_get_type (), NULL);
	g_mime_content_set_stream (GMIME_CONTENT (custom), bounded);
	g_mime_content_set_encoding (GMIME_CONTENT (custom), GMIME_CONTENT_ENCODING_BASE64);
	part = g_mime_part_new ();
	g_mime_part_set_content (part, GMIME_CONTENT (custom));
	digest = g_mime_part_compute_content_md5 (part, &error);
	check (error == NULL && !g_strcmp0 (digest, "kAFQmDzST7DWlj99KOF/cg==") &&
	       custom->write_calls == 0 && custom->decode_calls == 0 &&
	       g_mime_stream_tell (bounded) == 7,
	       "Content-MD5 hashes bounded decoded readers rather than content write overrides");
	g_free (digest);
	g_object_unref (part);
	g_object_unref (custom);
	g_object_unref (bounded);
	g_object_unref (memory);
}

static void
test_part_best_encoding_errors (void)
{
	GMimePart *part = g_mime_part_new_with_type ("text", "plain");
	GMimeContent *content = g_mime_content_new ();
	TestStreamFailure *source = g_object_new (test_stream_failure_get_type (), NULL);
	GMimeContentType *content_type;
	TestContent *custom;
	GError *error = NULL;
	GMimeContentEncoding encoding;
	guint i, resets, reads;
	gint64 position;

	check (g_mime_part_get_best_content_encoding_with_max_line_length (part, -1, 0, &error) ==
	       GMIME_CONTENT_ENCODING_7BIT && error == NULL,
	       "missing text content skips argument validation and analysis");
	g_mime_part_set_content (part, content);
	check (g_mime_part_get_best_content_encoding (part, GMIME_ENCODING_CONSTRAINT_7BIT, &error) ==
	       GMIME_CONTENT_ENCODING_UNSPECIFIED &&
	       g_error_matches (error, GMIME_ERROR, GMIME_ERROR_INVALID_OPERATION),
	       "encoding analysis without a backing stream reports an invalid operation");
	g_clear_error (&error);
	source->parent_instance.source = g_mime_stream_mem_new_with_buffer ("abc", 3);
	g_mime_stream_construct (GMIME_STREAM (source), 0, -1);
	source->onebyte = TRUE;
	g_mime_content_set_stream (content, GMIME_STREAM (source));

	/* Cover failures both before and after useful bytes have been scanned,
	 * including the final rewind that otherwise hides behind a valid choice. */
	for (i = 0; i < 6; i++) {
		source->bytes_read = 0;
		source->fail_after = i % 3 == 1 ? 1 : G_MAXSIZE;
		source->fail_reset_on = i % 3 == 0 ? source->reset_calls + 1 :
			(i % 3 == 2 ? source->reset_calls + 2 : 0);
		source->unset_error = i >= 3;
		errno = EACCES;
		encoding = g_mime_part_get_best_content_encoding (part, GMIME_ENCODING_CONSTRAINT_7BIT, &error);
		position = i % 3 == 0 ? 0 : (i % 3 == 1 ? 1 : 3);
		check (encoding == GMIME_CONTENT_ENCODING_UNSPECIFIED &&
		       g_error_matches (error, GMIME_ERROR, i >= 3 ? GMIME_ERROR_GENERAL :
					(i % 3 == 1 ? EIO : ESPIPE)) &&
		       g_mime_stream_tell (GMIME_STREAM (source)) == position,
		       "initial rewind, partial read and final rewind failures never return a partial encoding");
		g_clear_error (&error);
		source->fail_after = G_MAXSIZE;
		source->fail_reset_on = 0;
		source->bytes_read = 0;
		source->unset_error = FALSE;
		resets = source->reset_calls;
		check (g_mime_part_get_best_content_encoding (part, GMIME_ENCODING_CONSTRAINT_7BIT, &error) ==
		       GMIME_CONTENT_ENCODING_7BIT && error == NULL &&
		       source->reset_calls == resets + 2 && g_mime_stream_tell (GMIME_STREAM (source)) == 0 &&
		       source->close_calls == 0 && source->flush_calls == 0,
		       "analysis retries rewind before and after reading without closing or flushing the source");
	}

	for (i = 0; i < 4; i++) {
		source->bytes_read = 0;
		resets = source->reset_calls;
		encoding = g_mime_part_get_best_content_encoding_with_max_line_length (
			part, i < 2 ? GMIME_ENCODING_CONSTRAINT_7BIT : (i == 2 ? -1 : 3),
			i < 2 ? (i == 0 ? 59 : 999) : 78, &error);
		check (encoding == GMIME_CONTENT_ENCODING_UNSPECIFIED && g_error_matches (error, GMIME_ERROR, EINVAL) &&
		       source->reset_calls == resets + 2 && g_mime_stream_tell (GMIME_STREAM (source)) == 0,
		       "invalid analysis arguments are reported after a successful decoded copy");
		g_clear_error (&error);
	}

	source->bytes_read = 0;
	source->fail_after = 1;
	check (g_mime_part_get_best_content_encoding_with_max_line_length (part, -1, 59, &error) ==
	       GMIME_CONTENT_ENCODING_UNSPECIFIED && g_error_matches (error, GMIME_ERROR, EIO),
	       "content I/O failures take precedence over invalid analysis arguments");
	g_clear_error (&error);
	resets = source->reset_calls;
	reads = source->read_calls;
	content_type = g_mime_content_type_new ("application", "octet-stream");
	g_mime_entity_set_content_type (GMIME_ENTITY (part), content_type);
	g_object_unref (content_type);
	check (g_mime_part_get_best_content_encoding_with_max_line_length (part, -1, 0, &error) ==
	       GMIME_CONTENT_ENCODING_BASE64 && error == NULL &&
	       g_mime_part_get_best_content_encoding_with_max_line_length (part, GMIME_ENCODING_CONSTRAINT_BINARY,
									  G_MAXUINT, &error) ==
	       GMIME_CONTENT_ENCODING_BINARY && error == NULL &&
	       source->reset_calls == resets && source->read_calls == reads,
	       "nontext attachments bypass content I/O and analysis argument validation");
	g_object_unref (part);
	part = g_mime_part_new_with_type ("application", "octet-stream");
	check (g_mime_part_get_best_content_encoding (part, GMIME_ENCODING_CONSTRAINT_7BIT, &error) ==
	       GMIME_CONTENT_ENCODING_BASE64 && error == NULL,
	       "missing nontext content still selects base64 rather than the text fast path");
	g_object_unref (part);
	g_object_unref (content);
	g_object_unref (source);

	custom = g_object_new (test_content_get_type (), NULL);
	source = g_object_new (test_stream_failure_get_type (), NULL);
	source->parent_instance.source = g_mime_stream_mem_new_with_buffer ("AA==", 4);
	source->fail_after = 0;
	source->fail_reset_on = 1;
	g_mime_content_set_stream (GMIME_CONTENT (custom), GMIME_STREAM (source));
	g_mime_content_set_encoding (GMIME_CONTENT (custom), GMIME_CONTENT_ENCODING_BASE64);
	part = g_mime_part_new_with_type ("message", "rfc822");
	g_mime_part_set_content (part, GMIME_CONTENT (custom));
	check (g_mime_part_get_best_content_encoding (part, GMIME_ENCODING_CONSTRAINT_7BIT, &error) ==
	       GMIME_CONTENT_ENCODING_7BIT && error == NULL &&
	       custom->decode_calls == 1 && custom->write_calls == 1 &&
	       source->reset_calls == 0 && source->read_calls == 0,
	       "analysis uses decoded writer overrides, not a reader of the backing stream");
	custom->fail_decode = TRUE;
	errno = EACCES;
	check (g_mime_part_get_best_content_encoding (part, GMIME_ENCODING_CONSTRAINT_7BIT, &error) ==
	       GMIME_CONTENT_ENCODING_UNSPECIFIED && g_error_matches (error, GMIME_ERROR, GMIME_ERROR_GENERAL),
	       "decode override failures without errno report a general error, not stale errno");
	g_clear_error (&error);
	g_object_unref (part);
	g_object_unref (custom);
	g_object_unref (source);
}

static void
test_part_prepare_errors (void)
{
	GMimePartPrivate *priv;
	GMimePart *part = g_mime_part_new_with_type ("text", "plain");
	GMimeContent *content = g_mime_content_new ();
	TestStreamFailure *source = g_object_new (test_stream_failure_get_type (), NULL);
	TestContent *custom;
	GError *error = NULL;
	char *before, *after;
	guint i, reads, resets;

	check (g_mime_part_prepare (part, GMIME_ENCODING_CONSTRAINT_7BIT, &error) &&
	       error == NULL && g_mime_part_get_content_encoding (part) == GMIME_CONTENT_ENCODING_UNSPECIFIED,
	       "preparing an empty text part preserves an unspecified transfer encoding");
	g_mime_part_set_content (part, content);
	for (i = GMIME_CONTENT_ENCODING_BINARY; i <= GMIME_CONTENT_ENCODING_UUENCODE; i++) {
		g_mime_part_set_content_encoding (part, i);
		g_assert_true (g_mime_part_prepare (part, i == GMIME_CONTENT_ENCODING_BINARY ?
						 GMIME_ENCODING_CONSTRAINT_BINARY : GMIME_ENCODING_CONSTRAINT_7BIT, &error));
		g_assert_no_error (error);
		g_assert_cmpint (g_mime_part_get_content_encoding (part), ==, i);
		if (i != GMIME_CONTENT_ENCODING_BINARY) {
			g_assert_true (g_mime_part_prepare (part, -1, &error));
			g_assert_no_error (error);
		}
		g_assert_false (g_mime_part_prepare_with_max_line_length (part, GMIME_ENCODING_CONSTRAINT_7BIT, 59, &error));
		g_assert_error (error, GMIME_ERROR, EINVAL);
		g_clear_error (&error);
	}
	g_mime_part_set_content_encoding (part, GMIME_CONTENT_ENCODING_7BIT);
	g_assert_false (g_mime_part_prepare (part, GMIME_ENCODING_CONSTRAINT_7BIT, &error));
	g_assert_error (error, GMIME_ERROR, GMIME_ERROR_INVALID_OPERATION);
	g_clear_error (&error);

	source->parent_instance.source = g_mime_stream_mem_new_with_buffer ("abc", 3);
	source->onebyte = TRUE;
	source->fail_after = G_MAXSIZE;
	g_mime_stream_construct (GMIME_STREAM (source), 0, -1);
	g_mime_content_set_stream (content, GMIME_STREAM (source));
	g_assert_true (g_mime_part_prepare_with_max_line_length (part, GMIME_ENCODING_CONSTRAINT_7BIT, 60, &error));
	g_assert_no_error (error);
	priv = _g_mime_part_get_private (part);

	g_assert_cmpuint (priv->encoder_max_line_length, ==, 60);
	before = g_mime_entity_get_headers (GMIME_ENTITY (part), NULL);

	for (i = 0; i < 6; i++) {
		source->bytes_read = 0;
		source->fail_after = i % 3 == 1 ? 1 : G_MAXSIZE;
		source->fail_reset_on = i % 3 == 0 ? source->reset_calls + 1 :
			(i % 3 == 2 ? source->reset_calls + 2 : 0);
		source->unset_error = i >= 3;
		g_assert_false (g_mime_part_prepare_with_max_line_length (part, GMIME_ENCODING_CONSTRAINT_7BIT, 70, &error));
		g_assert_error (error, GMIME_ERROR, (i >= 3 ? GMIME_ERROR_GENERAL : (i % 3 == 1 ? EIO : ESPIPE)));
		g_clear_error (&error);
		g_assert_cmpuint (priv->encoder_max_line_length, ==, 60);
		after = g_mime_entity_get_headers (GMIME_ENTITY (part), NULL);
		g_assert_cmpstr (after, ==, before);
		g_free (after);
		g_assert_cmpuint (source->close_calls, ==, 0);
	}

	source->bytes_read = 0;
	source->fail_after = 0;
	reads = source->read_calls;
	resets = source->reset_calls;
	g_assert_false (g_mime_part_prepare_with_max_line_length (part, GMIME_ENCODING_CONSTRAINT_7BIT, 999, &error));
	g_assert_error (error, GMIME_ERROR, EINVAL);
	g_clear_error (&error);
	g_assert_cmpuint (source->bytes_read, ==, 0);
	g_assert_cmpuint (source->read_calls, ==, reads);
	g_assert_cmpuint (source->reset_calls, ==, resets);
	source->fail_after = G_MAXSIZE;
	source->fail_reset_on = 0;
	source->unset_error = FALSE;
	g_assert_true (g_mime_part_prepare_with_max_line_length (part, GMIME_ENCODING_CONSTRAINT_7BIT, 70, &error));
	g_assert_no_error (error);
	g_assert_cmpuint (priv->encoder_max_line_length, ==, 70);
	g_assert_cmpint (g_mime_stream_tell (GMIME_STREAM (source)), ==, 0);
	resets = source->reset_calls;
	g_assert_false (g_mime_part_prepare (part, -1, &error));
	g_assert_error (error, GMIME_ERROR, EINVAL);
	g_clear_error (&error);
	g_assert_cmpuint (source->reset_calls, ==, resets + 2);
	g_assert_cmpuint (priv->encoder_max_line_length, ==, 70);

	custom = g_object_new (test_content_get_type (), NULL);
	g_mime_content_set_stream (GMIME_CONTENT (custom), GMIME_STREAM (source));
	g_mime_content_set_encoding (GMIME_CONTENT (custom), GMIME_CONTENT_ENCODING_BASE64);
	g_mime_part_set_content (part, GMIME_CONTENT (custom));
	g_assert_true (g_mime_part_prepare (part, GMIME_ENCODING_CONSTRAINT_7BIT, &error));
	g_assert_no_error (error);
	g_assert_cmpuint (custom->decode_calls, ==, 1);
	g_assert_cmpuint (custom->write_calls, ==, 1);
	check (priv->encoder_max_line_length == 78,
	       "successful preparation honors content overrides and records the default encoder limit");
	custom->fail_decode = TRUE;
	g_assert_false (g_mime_part_prepare (part, GMIME_ENCODING_CONSTRAINT_7BIT, &error));
	g_assert_error (error, GMIME_ERROR, GMIME_ERROR_GENERAL);
	g_clear_error (&error);
	g_assert_cmpuint (priv->encoder_max_line_length, ==, 78);
	g_free (before);
	g_object_unref (custom);
	g_object_unref (source);
	g_object_unref (content);
	g_object_unref (part);
}

static void
test_entity_prepare (void)
{
	GMimePartPrivate *first_priv;
	GMimePartPrivate *body_priv;
	GMimePartPrivate *failing_priv;
	GMimeMessage *message = g_mime_message_new (FALSE), *inner = g_mime_message_new (FALSE);
	GMimeMultipart *root = g_mime_multipart_new (), *nested = g_mime_multipart_new ();
	GMimeMultipartSigned *signed_part = g_mime_multipart_signed_new ();
	GMimeMessagePart *attached = g_mime_message_part_new_with_message ("rfc822", inner);
	GMimePart *first = g_mime_part_new_with_type ("application", "octet-stream");
	GMimePart *failing = g_mime_part_new_with_type ("text", "plain");
	GMimePart *body = g_mime_part_new_with_type ("text", "plain");
	TestPreparePart *last = g_object_new (test_prepare_part_get_type (), NULL);
	TestPreparePart *protected = g_object_new (test_prepare_part_get_type (), NULL);
	TestStreamFailure *source = g_object_new (test_stream_failure_get_type (), NULL);
	GMimeContent *content;
	GMimeContentType *content_type;
	GMimeStream *memory;
	GMimeEntity *unsupported;
	GError *error = NULL;

	content_type = g_mime_content_type_new ("application", "octet-stream");
	g_mime_entity_set_content_type (GMIME_ENTITY (last), content_type);
	g_object_unref (content_type);
	content_type = g_mime_content_type_new ("text", "plain");
	g_mime_entity_set_content_type (GMIME_ENTITY (protected), content_type);
	g_object_unref (content_type);
	memory = g_mime_stream_mem_new_with_buffer ("From text\n", 10);
	content = g_mime_content_new_with_stream (memory, GMIME_CONTENT_ENCODING_UNSPECIFIED);
	g_mime_part_set_content (first, content);
	g_mime_part_set_content (body, content);
	g_object_unref (content);
	g_object_unref (memory);
	source->parent_instance.source = g_mime_stream_mem_new_with_buffer ("abc", 3);
	g_mime_stream_construct (GMIME_STREAM (source), 0, -1);
	content = g_mime_content_new_with_stream (GMIME_STREAM (source), GMIME_CONTENT_ENCODING_UNSPECIFIED);
	g_mime_part_set_content (failing, content);
	g_mime_part_set_content_encoding (failing, GMIME_CONTENT_ENCODING_7BIT);
	g_object_unref (content);
	content = g_mime_content_new ();
	g_mime_part_set_content (GMIME_PART (protected), content);
	g_mime_part_set_content_encoding (GMIME_PART (protected), GMIME_CONTENT_ENCODING_8BIT);
	g_object_unref (content);
	g_mime_multipart_add (GMIME_MULTIPART (signed_part), GMIME_ENTITY (protected));
	g_mime_multipart_add (root, GMIME_ENTITY (signed_part));
	g_mime_multipart_add (root, GMIME_ENTITY (first));
	g_mime_multipart_add (root, GMIME_ENTITY (nested));
	g_mime_multipart_add (nested, GMIME_ENTITY (failing));
	g_mime_multipart_add (nested, GMIME_ENTITY (last));
	g_mime_multipart_add (root, GMIME_ENTITY (attached));
	g_mime_message_set_mime_part (message, GMIME_ENTITY (root));
	g_mime_message_set_mime_part (inner, GMIME_ENTITY (body));
	g_mime_entity_remove_header (GMIME_ENTITY (message), "MIME-Version");
	g_mime_entity_set_header (GMIME_ENTITY (inner), "MIME-Version", "invalid", NULL);
	g_mime_entity_set_header (GMIME_ENTITY (attached), "Content-Transfer-Encoding", "binary", NULL);

	check (!g_mime_entity_prepare_with_max_line_length (GMIME_ENTITY (message), GMIME_ENCODING_CONSTRAINT_7BIT, 59, &error) &&
	       g_error_matches (error, GMIME_ERROR, EINVAL) &&
	       g_mime_entity_get_header (GMIME_ENTITY (message), "MIME-Version") == NULL &&
	       source->read_calls == 0 && source->reset_calls == 0,
	       "invalid tree line limit fails before MIME-Version changes or child analysis");
	g_clear_error (&error);
	check (!g_mime_entity_prepare_with_max_line_length (GMIME_ENTITY (message), GMIME_ENCODING_CONSTRAINT_7BIT, 60, &error) &&
	       g_error_matches (error, GMIME_ERROR, EIO),
	       "recursive preparation reports the first failing child's I/O error");
	g_clear_error (&error);
	failing_priv = _g_mime_part_get_private (failing);

	first_priv = _g_mime_part_get_private (first);

	check (g_mime_part_get_content_encoding (first) == GMIME_CONTENT_ENCODING_BASE64 &&
	       first_priv->encoder_max_line_length == 60 && failing_priv->encoder_max_line_length == 0 &&
	       g_mime_part_get_content_encoding (failing) == GMIME_CONTENT_ENCODING_7BIT &&
	       last->calls == 0 && protected->calls == 0 &&
	       g_mime_part_get_content_encoding (GMIME_PART (protected)) == GMIME_CONTENT_ENCODING_8BIT &&
	       g_mime_part_get_content_encoding (body) == GMIME_CONTENT_ENCODING_UNSPECIFIED,
	       "ordered tree preparation retains earlier changes, skips signed content and stops before later siblings");
	g_assert_cmpstr (g_mime_entity_get_header (GMIME_ENTITY (message), "MIME-Version"), ==, "1.0");
	g_assert_cmpstr (g_mime_entity_get_header (GMIME_ENTITY (inner), "MIME-Version"), ==, "invalid");
	g_assert_false (g_mime_entity_prepare (GMIME_ENTITY (message), GMIME_ENCODING_CONSTRAINT_7BIT, NULL));

	source->fail_after = G_MAXSIZE;
	g_assert_true (g_mime_entity_prepare_with_max_line_length (GMIME_ENTITY (message), GMIME_ENCODING_CONSTRAINT_7BIT, 61, &error));
	g_assert_no_error (error);
	body_priv = _g_mime_part_get_private (body);

	check (first_priv->encoder_max_line_length == 60 && failing_priv->encoder_max_line_length == 61 &&
	       last->calls == 1 && last->max_line_length == 61 && last->constraint == GMIME_ENCODING_CONSTRAINT_7BIT &&
	       protected->calls == 0 && g_mime_part_get_content_encoding (body) == GMIME_CONTENT_ENCODING_QUOTEDPRINTABLE &&
	       body_priv->encoder_max_line_length == 61,
	       "successful retry delegates through nested multipart, message and subclass prepare implementations");
	g_assert_cmpstr (g_mime_entity_get_header (GMIME_ENTITY (inner), "MIME-Version"), ==, "1.0");
	g_assert_cmpstr (g_mime_entity_get_header (GMIME_ENTITY (attached), "Content-Transfer-Encoding"), ==, "binary");
	g_assert_true (g_mime_part_prepare (GMIME_PART (last), GMIME_ENCODING_CONSTRAINT_BINARY, &error));
	g_assert_no_error (error);
	check (last->calls == 2 && last->max_line_length == 78 && last->constraint == GMIME_ENCODING_CONSTRAINT_BINARY,
	       "the existing leaf-part convenience API dispatches the prepare virtual with its default limit");
	g_assert_true (g_mime_entity_prepare (GMIME_ENTITY (signed_part), -1, &error));
	g_assert_no_error (error);
	g_assert_cmpuint (protected->calls, ==, 0);
	g_assert_false (g_mime_entity_prepare_with_max_line_length (GMIME_ENTITY (signed_part), -1, 999, &error));
	g_assert_error (error, GMIME_ERROR, EINVAL);
	g_clear_error (&error);
	unsupported = g_object_new (test_unprepared_entity_get_type (), NULL);
	g_assert_false (g_mime_entity_prepare (unsupported, GMIME_ENCODING_CONSTRAINT_7BIT, &error));
	g_assert_error (error, GMIME_ERROR, GMIME_ERROR_NOT_SUPPORTED);
	g_clear_error (&error);
	g_object_unref (unsupported);

	g_object_unref (message);
	g_object_unref (inner);
	g_object_unref (root);
	g_object_unref (nested);
	g_object_unref (signed_part);
	g_object_unref (attached);
	g_object_unref (first);
	g_object_unref (failing);
	g_object_unref (body);
	g_object_unref (last);
	g_object_unref (protected);
	g_object_unref (source);
}

static void
test_message_prepare_versions (void)
{
	static const struct {
		const char *raw;
		gboolean valid;
	} cases[] = {
		{ NULL, FALSE }, { " invalid\n", FALSE }, { " 1.0\n", TRUE },
		{ " (old) 2 (a) . (b) 3 (end)\n", TRUE }, { " 1.2.3.4\n", TRUE },
		{ " 1.0 (incomplete\n", FALSE }, { " 2147483648.0\n", FALSE },
		{ " 1.2.3.4.5\n", FALSE }, { " 1.0.", TRUE }, { " 1.0.\n", FALSE }
	};
	GMimeMessage *message;
	GMimePart *body;
	GMimeHeaderList *headers;
	GMimeHeader *header;
	GMimeEntity *empty[] = {
		GMIME_ENTITY (g_mime_multipart_new ()),
		GMIME_ENTITY (g_mime_message_new (FALSE)),
		GMIME_ENTITY (g_mime_message_part_new ("rfc822"))
	};
	GError *error = NULL;
	guint i, clear;

	for (i = 0; i < G_N_ELEMENTS (empty); i++) {
		g_assert_true (g_mime_entity_prepare (empty[i], -1, &error));
		g_assert_no_error (error);
		g_assert_false (g_mime_entity_prepare_with_max_line_length (empty[i], -1, 59, &error));
		g_assert_error (error, GMIME_ERROR, EINVAL);
		g_clear_error (&error);
		g_object_unref (empty[i]);
	}

	for (clear = 0; clear < 2; clear++) {
		for (i = 0; i < G_N_ELEMENTS (cases); i++) {
			message = g_mime_message_new (FALSE);
			body = g_mime_part_new_with_type ("text", "plain");
			g_mime_message_set_mime_part (message, GMIME_ENTITY (body));
			if (clear)
				g_mime_header_list_clear (g_mime_entity_get_header_list (GMIME_ENTITY (body)));
			headers = g_mime_entity_get_header_list (GMIME_ENTITY (message));
			g_mime_header_list_clear (headers);
			if (cases[i].raw) {
				char *wire = g_strdup_printf ("MIME-Version:%s", cases[i].raw);
				GMimeStream *stream = g_mime_stream_mem_new_with_buffer (wire, strlen (wire));
				GMimeHeaderList *parsed = g_mime_header_list_load (NULL, stream, &error);

				g_assert_no_error (error);
				header = g_mime_header_list_get_header (parsed, "MIME-Version");
				g_mime_header_list_add (headers, header);
				g_object_unref (parsed);
				g_object_unref (stream);
				g_free (wire);
				g_mime_header_list_append (headers, "MIME-Version", "9.9", NULL);
			}
			g_assert_true (g_mime_entity_prepare (GMIME_ENTITY (message), GMIME_ENCODING_CONSTRAINT_7BIT, &error));
			g_assert_no_error (error);
			header = g_mime_header_list_get_header (headers, "MIME-Version");
			if (clear && !cases[i].raw) {
				g_assert_null (header);
			} else if (clear || cases[i].valid) {
				g_assert_cmpstr (testsuite_header_raw_text (header), ==, cases[i].raw);
				g_assert_cmpint (g_mime_header_list_get_count (headers), ==, 2);
			} else {
				g_assert_cmpstr (g_mime_header_get_value (header), ==, "1.0");
				g_assert_cmpint (g_mime_header_list_get_count (headers), ==, 1);
			}
			g_object_unref (body);
			g_object_unref (message);
		}
	}
	check (TRUE, "message preparation preserves valid raw MIME versions and duplicates, replacing invalid versions only when the body has headers");
}

static void
test_content_copy (void)
{
	GMimeStream *source, *output, *reader;
	GMimeContent *content;
	TestStreamFailure *input, *sink;
	TestContent *custom;
	GByteArray *bytes;
	char buffer[8];
	ssize_t written;
	guint resets;
	int decode, mode;

	source = g_mime_stream_mem_new_with_buffer ("A\000\377B", 4);
	content = g_mime_content_new_with_stream (source, GMIME_CONTENT_ENCODING_BINARY);
	for (decode = 0; decode < 2; decode++) {
		output = g_mime_stream_mem_new_with_buffer ("before____after", 15);
		g_mime_stream_seek (output, 6, GMIME_STREAM_SEEK_SET);
		g_mime_stream_seek (source, 3, GMIME_STREAM_SEEK_SET);
		written = decode ? g_mime_content_decode_to_stream (content, output) :
			g_mime_content_write_to_stream (content, output);
		bytes = g_mime_stream_mem_get_byte_array (GMIME_STREAM_MEM (output));
		check (written == 4 && bytes->len == 15 &&
		       !memcmp (bytes->data, "beforeA\000\377Bafter", 15) &&
		       g_mime_stream_tell (output) == 10 && g_mime_stream_tell (source) == 0,
		       "content copying preserves binary NULs, output position, prefix and suffix");
		g_object_unref (output);
	}
	g_object_unref (content);
	g_object_unref (source);

	for (decode = 0; decode < 2; decode++) {
		for (mode = 0; mode < 4; mode++) {
			GMimeStreamMemPrivate *input_priv, *sink_priv;

			input = g_object_new (test_stream_failure_get_type (), NULL);
			input->parent_instance.source = g_mime_stream_mem_new_with_buffer ("Q2F0", 4);
			input->onebyte = TRUE;
			input->fail_after = mode == 3 ? 2 : G_MAXSIZE;
			g_mime_stream_construct (GMIME_STREAM (input), 0, -1);
			content = g_mime_content_new_with_stream (GMIME_STREAM (input), GMIME_CONTENT_ENCODING_BASE64);
			sink = g_object_new (test_stream_failure_get_type (), NULL);
			sink->parent_instance.source = g_mime_stream_mem_new ();
			sink->onebyte = TRUE;
			sink->fail_after = mode == 1 || mode == 2 ? 1 : G_MAXSIZE;
			sink->zero_write = mode == 2;
			g_mime_stream_construct (GMIME_STREAM (sink), 0, -1);
			errno = 0;
			written = decode ? g_mime_content_decode_to_stream (content, GMIME_STREAM (sink)) :
				g_mime_content_write_to_stream (content, GMIME_STREAM (sink));
			if (mode == 0) {
				bytes = g_mime_stream_mem_get_byte_array (GMIME_STREAM_MEM (sink->parent_instance.source));
				check (written == (decode ? 3 : 4) && bytes->len == written &&
				       !memcmp (bytes->data, decode ? "Cat" : "Q2F0", written) && input->reset_calls == 2,
				       "raw and decoded copies complete short writes and rewind after success");
			} else {
				check (written == -1 && errno == (mode == 1 ? ENOSPC : EIO) && input->reset_calls == 1 &&
				       g_mime_stream_tell (input->parent_instance.source) > 0,
				       "failed and zero-progress writes or reads preserve errors and source progress without rewinding");
			}
			input_priv = _g_mime_stream_mem_get_private (GMIME_STREAM_MEM (input->parent_instance.source));
			sink_priv = _g_mime_stream_mem_get_private (GMIME_STREAM_MEM (sink->parent_instance.source));

			check (input_priv->buffer != NULL &&
			       sink_priv->buffer != NULL &&
			       input->flush_calls == 0 && sink->flush_calls == 0,
			       "content copying never closes or flushes the input or output stream");
			g_object_unref (content);
			g_object_unref (input);
			g_object_unref (sink);
		}

		input = g_object_new (test_stream_failure_get_type (), NULL);
		input->parent_instance.source = g_mime_stream_mem_new_with_buffer ("Q2F0", 4);
		input->fail_after = G_MAXSIZE;
		input->fail_reset_on = 1;
		g_mime_stream_construct (GMIME_STREAM (input), 0, -1);
		content = g_mime_content_new_with_stream (GMIME_STREAM (input), GMIME_CONTENT_ENCODING_BASE64);
		output = g_mime_stream_mem_new ();
		errno = 0;
		written = decode ? g_mime_content_decode_to_stream (content, output) :
			g_mime_content_write_to_stream (content, output);
		check (written == -1 && errno == ESPIPE && input->read_calls == 0 &&
		       g_mime_stream_tell (output) == 0,
		       "copying reports initial rewind failures without reading or producing output");
		input->fail_reset_on = input->reset_calls + 2;
		errno = 0;
		written = decode ? g_mime_content_decode_to_stream (content, output) :
			g_mime_content_write_to_stream (content, output);
		check (written == -1 && errno == ESPIPE && g_mime_stream_tell (output) == (decode ? 3 : 4),
		       "a failed final rewind is reported even when raw or decoded output has been written");
		resets = input->reset_calls;
		input->fail_after = 0;
		input->bytes_read = 0;
		input->fail_reset_on = resets + 2;
		errno = 0;
		written = decode ? g_mime_content_decode_to_stream (content, output) :
			g_mime_content_write_to_stream (content, output);
		check (written == -1 && errno == EIO && input->reset_calls == resets + 1,
		       "copying does not hide an I/O error behind a cleanup rewind failure");
		g_object_unref (output);
		g_object_unref (content);
		g_object_unref (input);
	}

	source = g_mime_stream_mem_new_with_buffer ("xxQ2F0tail", 10);
	reader = g_mime_stream_substream (source, 2, 6);
	content = g_mime_content_new_with_stream (reader, GMIME_CONTENT_ENCODING_BASE64);
	for (decode = 0; decode < 2; decode++) {
		output = g_mime_stream_mem_new ();
		g_mime_stream_seek (reader, 0, GMIME_STREAM_SEEK_END);
		written = decode ? g_mime_content_decode_to_stream (content, output) :
			g_mime_content_write_to_stream (content, output);
		bytes = g_mime_stream_mem_get_byte_array (GMIME_STREAM_MEM (output));
		check (written == (decode ? 3 : 4) && bytes->len == written &&
		       !memcmp (bytes->data, decode ? "Cat" : "Q2F0", written) &&
		       g_mime_stream_tell (reader) == 2,
		       "raw and decoded copying honor nonzero bounded source regions and rewind to their start");
		g_object_unref (output);
	}
	g_object_unref (content);
	g_object_unref (reader);
	g_object_unref (source);

	source = g_mime_stream_mem_new_with_buffer ("RG9n", 4);
	custom = g_object_new (test_content_get_type (), NULL);
	g_mime_content_set_stream (GMIME_CONTENT (custom), source);
	g_mime_content_set_encoding (GMIME_CONTENT (custom), GMIME_CONTENT_ENCODING_BASE64);
	output = g_mime_stream_mem_new ();
	check (g_mime_content_write_to_stream (GMIME_CONTENT (custom), output) == 4 &&
	       custom->write_calls == 1 && custom->decode_calls == 0,
	       "raw content copying dispatches to the raw virtual method");
	g_object_unref (output);
	output = g_mime_stream_mem_new ();
	written = g_mime_content_decode_to_stream (GMIME_CONTENT (custom), output);
	bytes = g_mime_stream_mem_get_byte_array (GMIME_STREAM_MEM (output));
	check (written == 3 && bytes->len == 3 && !memcmp (bytes->data, "Cat", 3) &&
	       custom->write_calls == 2 && custom->decode_calls == 1,
	       "decoded content copying dispatches both virtual methods and decodes the raw override");
	reader = g_mime_content_open (GMIME_CONTENT (custom));
	check (reader && g_mime_stream_read (reader, buffer, sizeof (buffer)) == 3 &&
	       !memcmp (buffer, "Dog", 3) && custom->write_calls == 2 && custom->decode_calls == 1,
	       "opening decoded content reads the backing stream without invoking copy overrides");
	g_object_unref (reader);
	g_object_unref (output);
	g_object_unref (custom);
	g_object_unref (source);
}

static GMimeApplicationPkcs7Mime *
test_compressed_part_new (const void *data, size_t length, GMimeContentEncoding encoding)
{
	GMimeApplicationPkcs7Mime *part;
	GMimeContent *content;
	GMimeStream *stream;

	part = g_mime_application_pkcs7_mime_new (GMIME_SECURE_MIME_TYPE_COMPRESSED_DATA);
	stream = g_mime_stream_mem_new_with_buffer (data, length);
	content = g_mime_content_new_with_stream (stream, encoding);
	g_mime_part_set_content (GMIME_PART (part), content);
	g_object_unref (content);
	g_object_unref (stream);
	return part;
}

static void
test_compressed_data (void)
{
	/* Independently encoded RFC 3274 fixtures with RFC 1950 zlib content. */
	static const char *valid[] = {
		"MGkGCyqGSIb3DQEJEAEJoFowWAIBADANBgsqhkiG9w0BCRADCDBEBgkqhkiG9w0BBwGgNwQ1eJxzzs8rSc0r0Q2pLEi1UihJrSjRL8hJzMzj5eLlcs7PLShKLS5OTVFIhijT4+UCALDBELA=",
		"MGsGCyqGSIb3DQEJEAEJoFwwWgIBADAPBgsqhkiG9w0BCRADCAUAMEQGCSqGSIb3DQEHAaA3BDV4nHPOzytJzSvRDaksSLVSKEmtKNEvyEnMzOPl4uVyzs8tKEotLk5NUUiGKNPj5QIAsMEQsA==",
		"MIAGCyqGSIb3DQEJEAEJoIAwgAIBADCABgsqhkiG9w0BCRADCAUAAAAwgAYJKoZIhvcNAQcBoIAkgAQIeJxzzs8rSc0kgAQtK9ENqSxItVIoSa0o0S/ISczM4+Xi5XLOzy0oSi0uTk1RSIYo0+PlAgCwwRCwAAAAAAAAAAAAAAAAAAA=",
		"MG0GCyqGSIb3DQEJEAEJoF4wXAIBADANBgsqhkiG9w0BCRADCDBIBgkqhkiG9w0BBwGgOyQ5BAh4nHPOzytJzQQtK9ENqSxItVIoSa0o0S/ISczM4+Xi5XLOzy0oSi0uTk1RSIYo0+PlAgCwwRCw"
	};
	static const char *invalid[] = {
		"MGgGCyqGSIb3DQEJEAEJoFkwVwIBADANBgsqhkiG9w0BCRADCDBDBgkqhkiG9w0BBwGgNgQ0eJxzzs8rSc0r0Q2pLEi1UihJrSjRL8hJzMzj5eLlcs7PLShKLS5OTVFIhijT4+UCALDBEA==",
		"MGoGCyqGSIb3DQEJEAEJoFswWQIBADANBgsqhkiG9w0BCRADCDBFBgkqhkiG9w0BBwGgOAQ2eJxzzs8rSc0r0Q2pLEi1UihJrSjRL8hJzMzj5eLlcs7PLShKLS5OTVFIhijT4+UCALDBELB4",
		"MIGgBgsqhkiG9w0BCRABCaCBkDCBjQIBADANBgsqhkiG9w0BCRADCDB5BgkqhkiG9w0BBwGgbARqeJxzzs8rSc0r0Q2pLEi1UihJrSjRL8hJzMzj5eLlcs7PLShKLS5OTVFIhijT4+UCALDBELB4nHPOzytJzSvRDaksSLVSKEmtKNEvyEnMzOPl4uVyzs8tKEotLk5NUUiGKNPj5QIAsMEQsA==",
		"MHUGCyqGSIb3DQEJEAEJoGYwZAIBADANBgsqhkiG9w0BCRADCDBQBgkqhkiG9w0BBwGgQwRBH4sIAAAAAAAACnPOzytJzSvRDaksSLVSKEmtKNEvyEnMzOPl4uVyzs8tKEotLk5NUUiGKNPj5QIAZ9ZRXDEAAAA=",
		"MGMGCyqGSIb3DQEJEAEJoFQwUgIBADANBgsqhkiG9w0BCRADCDA+BgkqhkiG9w0BBwGgMQQvc87PK0nNK9ENqSxItVIoSa0o0S/ISczM4+Xi5XLOzy0oSi0uTk1RSIYo0+PlAgA=",
		"MDwGCyqGSIb3DQEJEAEJoC0wKwIBADANBgsqhkiG9w0BCRADCDAXBgkqhkiG9w0BBwGgCgQIeJwDAAAAAAE=",
		"MIIBDQYLKoZIhvcNAQkQAQmggf0wgfoCAQAwDQYLKoZIhvcNAQkQAwgwgeUGCSqGSIb3DQEHAaCB1ySAJIAkgCSAJIAkgCSAJIAkgCSAJIAkgCSAJIAkgCSAJIAkgCSAJIAkgCSAJIAkgCSAJIAkgCSAJIAkgCSAJIAkgCSAJIAkgCSAJIAkgCSABDV4nHPOzytJzSvRDaksSLVSKEmtKNEvyEnMzOPl4uVyzs8tKEotLk5NUUiGKNPj5QIAsMEQsAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
	};
	static const struct {
		guint offset;
		unsigned char value;
		int code;
	} mutations[] = {
		{ 0, 0x31, GMIME_ERROR_PARSE_ERROR },
		{ 1, 0xff, GMIME_ERROR_PARSE_ERROR },
		{ 1, 0x68, GMIME_ERROR_PARSE_ERROR },
		{ 14, 8, GMIME_ERROR_PARSE_ERROR },
		{ 21, 1, GMIME_ERROR_PARSE_ERROR },
		{ 36, 7, GMIME_ERROR_NOT_SUPPORTED },
		{ 49, 2, GMIME_ERROR_NOT_SUPPORTED },
		{ 53, 0x80, GMIME_ERROR_PARSE_ERROR },
		{ 106, 0, GMIME_ERROR_PARSE_ERROR }
	};
	GMimeApplicationPkcs7Mime *part, *compressed;
	GMimeEntity *entity;
	GMimeTextPart *text_part;
	GMimePart *binary;
	GMimeContent *content;
	GMimeContentType *type;
	GMimeFormatOptions *options;
	GMimeStream *stream, *decoded;
	TestStreamFailure *failure;
	GByteArray *bytes, *original;
	GError *error = NULL;
	unsigned char *data, *changed, *payload;
	gsize length;
	char *text;
	guint i, j;
	guint32 random = 0x12345678;
	gboolean rejected;

	for (i = 0; i < G_N_ELEMENTS (valid); i++) {
		data = g_base64_decode (valid[i], &length);
		part = test_compressed_part_new (data, length, GMIME_CONTENT_ENCODING_BINARY);
		entity = g_mime_application_pkcs7_mime_decompress (part, 49, &error);
		g_assert_no_error (error);
		check (GMIME_IS_TEXT_PART (entity), "DER and BER CompressedData decode without a crypto context");
		if (GMIME_IS_TEXT_PART (entity)) {
			text = g_mime_text_part_get_text (GMIME_TEXT_PART (entity));
			check (text && !strcmp (text, "Compressed content.\n"),
			       "decompressed MIME content normalizes DOS newlines");
			g_free (text);
		}
		g_clear_object (&entity);
		entity = g_mime_application_pkcs7_mime_decompress (part, 48, &error);
		check (!entity && error && strstr (error->message, "maximum size"),
		       "decompression limit includes MIME headers and raw DOS newlines");
		g_assert_error (error, GMIME_ERROR, GMIME_ERROR_PARSE_ERROR);
		g_clear_error (&error);
		g_clear_object (&entity);
		g_object_unref (part);

		rejected = TRUE;
		for (j = 0; j < length; j++) {
			part = test_compressed_part_new (data, j, GMIME_CONTENT_ENCODING_BINARY);
			entity = g_mime_application_pkcs7_mime_decompress (part, 1024, &error);
			rejected = rejected && !entity && error != NULL;
			g_clear_error (&error);
			g_clear_object (&entity);
			g_object_unref (part);
		}
		check (rejected, "every truncated DER or BER prefix fails with an explicit error");
		g_free (data);
	}

	for (i = 0; i < G_N_ELEMENTS (invalid); i++) {
		data = g_base64_decode (invalid[i], &length);
		part = test_compressed_part_new (data, length, GMIME_CONTENT_ENCODING_BINARY);
		entity = g_mime_application_pkcs7_mime_decompress (part, 1024, &error);
		check (!entity && error,
		       "truncated, trailing, concatenated, non-zlib, empty, and deeply nested payloads are rejected");
		g_clear_error (&error);
		g_clear_object (&entity);
		g_object_unref (part);
		g_free (data);
	}

	data = g_base64_decode (valid[0], &length);
	for (i = 0; i < G_N_ELEMENTS (mutations); i++) {
		changed = g_memdup2 (data, length);
		changed[mutations[i].offset] = mutations[i].value;
		part = test_compressed_part_new (changed, length, GMIME_CONTENT_ENCODING_BINARY);
		entity = g_mime_application_pkcs7_mime_decompress (part, 1024, &error);
		check (!entity, "invalid CMS tags, lengths, OIDs, versions, and zlib checksums fail");
		g_assert_error (error, GMIME_ERROR, mutations[i].code);
		g_clear_error (&error);
		g_clear_object (&entity);
		g_object_unref (part);
		g_free (changed);
	}
	changed = g_malloc (length + 1);
	memcpy (changed, data, length);
	changed[length] = 0;
	part = test_compressed_part_new (changed, length + 1, GMIME_CONTENT_ENCODING_BINARY);
	entity = g_mime_application_pkcs7_mime_decompress (part, 1024, &error);
	check (!entity && error, "trailing CMS data is rejected");
	g_clear_error (&error);
	g_clear_object (&entity);
	g_object_unref (part);
	g_free (changed);

	for (i = 0; i < 2; i++) {
		for (j = 0; j <= length; j++) {
			failure = g_object_new (test_stream_failure_get_type (), NULL);
			failure->parent_instance.source = g_mime_stream_mem_new_with_buffer ((char *) data, length);
			failure->fail_after = j;
			failure->unset_error = i;
			failure->onebyte = TRUE;
			g_mime_stream_construct (GMIME_STREAM (failure), 0, -1);
			part = g_mime_application_pkcs7_mime_new (GMIME_SECURE_MIME_TYPE_COMPRESSED_DATA);
			content = g_mime_content_new_with_stream (GMIME_STREAM (failure), GMIME_CONTENT_ENCODING_BINARY);
			g_mime_part_set_content (GMIME_PART (part), content);
			g_object_unref (content);
			entity = g_mime_application_pkcs7_mime_decompress (part, 1024, &error);
			check (!entity, "CMS read failures never return partial or complete-looking entities");
			g_assert_error (error, GMIME_ERROR, (i ? GMIME_ERROR_GENERAL : EIO));
			g_clear_error (&error);
			g_clear_object (&entity);
			failure->bytes_read = 0;
			failure->fail_after = G_MAXSIZE;
			failure->fail_reset_on = failure->reset_calls + 1;
			entity = g_mime_application_pkcs7_mime_decompress (part, 1024, &error);
			g_assert_error (error, GMIME_ERROR, (i ? GMIME_ERROR_GENERAL : ESPIPE));
			g_clear_error (&error);
			g_clear_object (&entity);
			entity = g_mime_application_pkcs7_mime_decompress (part, 1024, &error);
			check (entity && !error, "decompression supports one-byte reads and retrying after an I/O failure");
			g_clear_error (&error);
			g_clear_object (&entity);
			check (!failure->close_calls, "decompression does not close the caller's content stream");
			g_object_unref (part);
			g_object_unref (failure);
		}
	}
	g_free (data);

	part = test_compressed_part_new (valid[0], strlen (valid[0]), GMIME_CONTENT_ENCODING_BASE64);
	type = g_mime_content_type_new ("application", "pkcs7-mime");
	g_mime_entity_set_content_type (GMIME_ENTITY (part), type);
	g_object_unref (type);
	entity = g_mime_application_pkcs7_mime_decompress (part, 49, &error);
	check (entity && !error, "base64 transfer decoding and missing smime-type parameters are supported");
	g_clear_object (&entity);
	g_clear_error (&error);
	g_object_unref (part);

	part = g_mime_application_pkcs7_mime_new (GMIME_SECURE_MIME_TYPE_ENVELOPED_DATA);
	check (!g_mime_application_pkcs7_mime_decompress (part, 1024, &error),
	       "decompression rejects incompatible S/MIME types");
	g_assert_error (error, GMIME_ERROR, GMIME_ERROR_INVALID_OPERATION);
	g_clear_error (&error);
	g_object_unref (part);
	part = g_mime_application_pkcs7_mime_new (GMIME_SECURE_MIME_TYPE_COMPRESSED_DATA);
	check (!g_mime_application_pkcs7_mime_decompress (part, 1024, &error),
	       "decompression reports absent content");
	g_assert_error (error, GMIME_ERROR, GMIME_ERROR_INVALID_OPERATION);
	g_clear_error (&error);
	content = g_mime_content_new ();
	g_mime_part_set_content (GMIME_PART (part), content);
	g_object_unref (content);
	check (!g_mime_application_pkcs7_mime_decompress (part, 1024, &error),
	       "decompression reports missing backing streams");
	g_assert_error (error, GMIME_ERROR, GMIME_ERROR_INVALID_OPERATION);
	g_clear_error (&error);
	g_object_unref (part);

	text_part = g_mime_text_part_new ();
	g_mime_text_part_set_text (text_part, "Compressed content.\n");
	compressed = g_mime_application_pkcs7_mime_compress (GMIME_ENTITY (text_part), &error);
	g_assert_no_error (error);
	check (compressed && g_mime_application_pkcs7_mime_get_smime_type (compressed) ==
	       GMIME_SECURE_MIME_TYPE_COMPRESSED_DATA &&
	       !strcmp (g_mime_part_get_filename (GMIME_PART (compressed)), "smime.p7z") &&
	       g_mime_part_get_content_encoding (GMIME_PART (compressed)) == GMIME_CONTENT_ENCODING_BASE64,
	       "compression constructs a compressed-data S/MIME attachment");
	if (compressed) {
		entity = g_mime_application_pkcs7_mime_decompress (compressed, 1024, &error);
		check (GMIME_IS_TEXT_PART (entity) && !error, "compressed MIME entities round-trip");
		g_clear_object (&entity);
		g_clear_error (&error);
		stream = g_mime_stream_mem_new ();
		g_mime_entity_write_to_stream (GMIME_ENTITY (compressed), NULL, stream);
		g_mime_stream_reset (stream);
		entity = g_mime_entity_load (NULL, stream, &error);
		g_assert_no_error (error);
		check (GMIME_IS_APPLICATION_PKCS7_MIME (entity), "serialized compressed attachments retain their MIME type");
		if (GMIME_IS_APPLICATION_PKCS7_MIME (entity)) {
			GMimeEntity *expanded;

			expanded = g_mime_application_pkcs7_mime_decompress (GMIME_APPLICATION_PKCS7_MIME (entity),
									   1024, &error);
			check (GMIME_IS_TEXT_PART (expanded) && !error,
			       "serialized compressed attachments decode their base64 content before decompression");
			g_clear_error (&error);
			g_clear_object (&expanded);
		}
		g_clear_object (&entity);
		g_object_unref (stream);
		g_object_unref (compressed);
	}
	text = g_strnfill (1024 * 1024, 'A');
	g_mime_text_part_set_text (text_part, text);
	g_free (text);
	compressed = g_mime_application_pkcs7_mime_compress (GMIME_ENTITY (text_part), &error);
	g_assert_no_error (error);
	check (compressed != NULL, "highly compressible MIME content can be compressed");
	if (compressed) {
		entity = g_mime_application_pkcs7_mime_decompress (compressed, 4096, &error);
		check (!entity && error && strstr (error->message, "maximum size"),
		       "high-expansion zlib payloads stop at the decompressed-size limit");
		g_clear_error (&error);
		g_clear_object (&entity);
		g_object_unref (compressed);
	}
	g_object_unref (text_part);

	payload = g_malloc (60000);
	for (i = 0; i < 60000; i++) {
		random ^= random << 13;
		random ^= random >> 17;
		random ^= random << 5;
		payload[i] = random;
	}
	binary = g_mime_part_new_with_type ("application", "octet-stream");
	stream = g_mime_stream_mem_new_with_buffer ((char *) payload, 60000);
	content = g_mime_content_new_with_stream (stream, GMIME_CONTENT_ENCODING_BINARY);
	g_mime_part_set_content (binary, content);
	g_mime_part_set_content_encoding (binary, GMIME_CONTENT_ENCODING_BASE64);
	g_object_unref (content);
	g_object_unref (stream);
	options = g_mime_format_options_clone (NULL);
	g_mime_format_options_set_newline_format (options, GMIME_NEWLINE_FORMAT_DOS);
	stream = g_mime_stream_mem_new ();
	g_mime_entity_write_to_stream (GMIME_ENTITY (binary), options, stream);
	g_mime_format_options_free (options);
	original = g_mime_stream_mem_get_byte_array (GMIME_STREAM_MEM (stream));
	compressed = g_mime_application_pkcs7_mime_compress (GMIME_ENTITY (binary), &error);
	g_assert_no_error (error);
	check (compressed != NULL, "compression handles incompressible content spanning many buffers");
	if (compressed) {
		entity = g_mime_application_pkcs7_mime_decompress (compressed, original->len, &error);
		check (GMIME_IS_PART (entity) && !error, "long-form DER lengths and multi-buffer zlib output round-trip");
		if (GMIME_IS_PART (entity)) {
			decoded = g_mime_stream_mem_new ();
			g_mime_content_decode_to_stream (g_mime_part_get_content (GMIME_PART (entity)), decoded);
			bytes = g_mime_stream_mem_get_byte_array (GMIME_STREAM_MEM (decoded));
			check (bytes->len == 60000 && !memcmp (bytes->data, payload, 60000),
			       "compression preserves arbitrary binary MIME content exactly");
			g_object_unref (decoded);
		}
		g_clear_error (&error);
		g_clear_object (&entity);
		entity = g_mime_application_pkcs7_mime_decompress (compressed, original->len - 1, &error);
		check (!entity && error, "multi-buffer decompression enforces the exact expanded-byte limit");
		g_clear_error (&error);
		g_clear_object (&entity);
		g_object_unref (compressed);
	}
	g_object_unref (stream);
	g_object_unref (binary);
	g_free (payload);

	for (i = 0; i < 2; i++) {
		failure = g_object_new (test_stream_failure_get_type (), NULL);
		failure->parent_instance.source = g_mime_stream_mem_new_with_buffer ("content", 7);
		failure->fail_after = 3;
		failure->unset_error = i;
		g_mime_stream_construct (GMIME_STREAM (failure), 0, -1);
		binary = g_mime_part_new ();
		content = g_mime_content_new_with_stream (GMIME_STREAM (failure), GMIME_CONTENT_ENCODING_BINARY);
		g_mime_part_set_content (binary, content);
		g_object_unref (content);
		compressed = g_mime_application_pkcs7_mime_compress (GMIME_ENTITY (binary), &error);
		check (!compressed, "compression propagates MIME serialization read failures");
		g_assert_error (error, GMIME_ERROR, (i ? GMIME_ERROR_GENERAL : EIO));
		g_clear_error (&error);
		g_clear_object (&compressed);
		g_object_unref (binary);
		g_object_unref (failure);
	}
}

typedef struct {
	GMimeCryptoContext parent_instance;
	guint calls;
	guint flags;
} TestCryptoContext;

typedef GMimeCryptoContextClass TestCryptoContextClass;

G_DEFINE_TYPE (TestCryptoContext, test_crypto_context, GMIME_TYPE_CRYPTO_CONTEXT)

#if 0 /* AuthEnvelopedData prototype (#65). */
static TestCryptoContext *authenveloped_context;

static GMimeCryptoContext *
test_authenveloped_context_new (void)
{
	return g_object_ref (GMIME_CRYPTO_CONTEXT (authenveloped_context));
}
#endif

static GMimeSignatureList *
test_crypto_verify (GMimeCryptoContext *ctx, GMimeVerifyFlags flags, GMimeStream *istream,
		    GMimeStream *sigstream, GMimeStream *ostream, GError **err)
{
	TestCryptoContext *test = (TestCryptoContext *) ctx;

	test->calls++;
	test->flags = flags;
	return g_mime_signature_list_new ();
}

static int
test_crypto_encrypt (GMimeCryptoContext *ctx, gboolean sign, const char *userid, GMimeEncryptFlags flags,
		     GPtrArray *recipients, GMimeStream *istream, GMimeStream *ostream, GError **err)
{
	TestCryptoContext *test = (TestCryptoContext *) ctx;

	test->calls++;
	test->flags = flags;
	return 0;
}

static GMimeDecryptResult *
test_crypto_decrypt (GMimeCryptoContext *ctx, GMimeDecryptFlags flags, const char *session_key,
		     GMimeStream *istream, GMimeStream *ostream, GError **err)
{
	TestCryptoContext *test = (TestCryptoContext *) ctx;

	test->calls++;
	test->flags = flags;
#if 0 /* AuthEnvelopedData prototype (#65). */
	/* Routing-only mock: no CMS parsing or cryptographic authentication. */
	if (test == authenveloped_context) {
		static const char plaintext[] = "Content-Type: text/plain\r\n\r\nAuthenticated content.\r\n";
		char marker[3];

		/* Model a backend that emits plaintext before checking the tag. */
		if (g_mime_stream_write (ostream, plaintext, sizeof (plaintext) - 1) != sizeof (plaintext) - 1) {
			g_set_error_literal (err, GMIME_ERROR, GMIME_ERROR_PROTOCOL_ERROR,
					     "Could not write mock decrypted content");
			return NULL;
		}

		if (g_mime_stream_read (istream, marker, sizeof (marker)) != sizeof (marker) ||
		    memcmp (marker, "CMS", sizeof (marker))) {
			g_set_error_literal (err, GMIME_ERROR, GMIME_ERROR_PROTOCOL_ERROR,
					     "Mock authentication failure");
			return NULL;
		}
	}
#endif
	return g_mime_decrypt_result_new ();
}

static void
test_authenveloped_data (void)
{
	static const char message[] =
		"Content-Type: application/pkcs7-mime; smime-type=AUTHENVELOPED-DATA\r\n"
		"Content-Transfer-Encoding: base64\r\n\r\nQ01T\r\n";
	GMimeApplicationPkcs7Mime *part;
	GMimeEntity *parsed;
	GMimeStream *stream;
	GMimeParser *parser;
	GMimeContentType *type;
	GError *error = NULL;

	part = g_mime_application_pkcs7_mime_new (GMIME_SECURE_MIME_TYPE_AUTH_ENVELOPED_DATA);
	type = g_mime_entity_get_content_type (GMIME_ENTITY (part));
	check (g_mime_application_pkcs7_mime_get_smime_type (part) == GMIME_SECURE_MIME_TYPE_AUTH_ENVELOPED_DATA &&
	       !strcmp (g_mime_content_type_get_parameter (type, "smime-type"), "authEnveloped-data") &&
	       !strcmp (g_mime_part_get_filename (GMIME_PART (part)), "smime.p7m") &&
	       g_mime_part_get_content_encoding (GMIME_PART (part)) == GMIME_CONTENT_ENCODING_BASE64,
	       "AuthEnvelopedData construction uses its own MIME type and normal S/MIME attachment conventions");
	g_object_unref (part);

	stream = g_mime_stream_mem_new_with_buffer (message, sizeof (message) - 1);
	parser = g_mime_parser_new ();
	g_mime_parser_init_with_stream (parser, stream);
	parsed = g_mime_parser_construct_part (parser, NULL, &error);
	g_object_unref (parser);
	g_object_unref (stream);
	check (GMIME_IS_APPLICATION_PKCS7_MIME (parsed) && !error,
	       "AuthEnvelopedData parses as an S/MIME part");
	g_clear_error (&error);
	if (!GMIME_IS_APPLICATION_PKCS7_MIME (parsed)) {
		g_clear_object (&parsed);
		return;
	}

	part = GMIME_APPLICATION_PKCS7_MIME (parsed);
	check (g_mime_application_pkcs7_mime_get_smime_type (part) == GMIME_SECURE_MIME_TYPE_AUTH_ENVELOPED_DATA,
	       "AuthEnvelopedData recognition is case-insensitive");

#if 0 /* AuthEnvelopedData prototype (#65). */
	/*
	 * Enable all matching prototype blocks together. GPGME/GPGSM/KSBA still
	 * need fixes for GCM parameters and the missing DECRYPTION_OKAY status.
	 * Backend authentication errors must never become successful decryptions.
	 */
	GMimeDecryptResult *result = NULL;
	GMimeEntity *decrypted;
	GMimeContent *content;
	char *text;

	authenveloped_context = g_object_new (test_crypto_context_get_type (), NULL);
	g_mime_crypto_context_register ("application/pkcs7-mime", test_authenveloped_context_new);
	decrypted = g_mime_application_pkcs7_mime_decrypt (part, GMIME_DECRYPT_ENABLE_ONLINE_CERTIFICATE_CHECKS,
							NULL, &result, &error);
	check (GMIME_IS_TEXT_PART (decrypted) && result && !error &&
	       authenveloped_context->calls == 1 &&
	       authenveloped_context->flags == GMIME_DECRYPT_ENABLE_ONLINE_CERTIFICATE_CHECKS,
	       "AuthEnvelopedData uses the existing CMS decrypt operation and preserves its options");
	if (GMIME_IS_TEXT_PART (decrypted)) {
		text = g_mime_text_part_get_text (GMIME_TEXT_PART (decrypted));
		check (text && !strcmp (text, "Authenticated content.\n"),
		       "decrypted MIME content is parsed only after backend success");
		g_free (text);
	}
	check (result && !g_mime_decrypt_result_get_signatures (result),
	       "authenticated encryption does not manufacture a digital signature");
	g_clear_object (&decrypted);
	g_clear_object (&result);
	g_clear_error (&error);

	stream = g_mime_stream_mem_new_with_buffer ("BAD", 3);
	content = g_mime_content_new_with_stream (stream, GMIME_CONTENT_ENCODING_UNSPECIFIED);
	g_mime_part_set_content (GMIME_PART (part), content);
	g_object_unref (content);
	g_object_unref (stream);
	decrypted = g_mime_application_pkcs7_mime_decrypt (part, GMIME_DECRYPT_NO_VERIFY, NULL, &result, &error);
	check (!decrypted && !result && error && error->domain == GMIME_ERROR &&
	       error->code == GMIME_ERROR_PROTOCOL_ERROR &&
	       !strcmp (error->message, "Mock authentication failure") &&
	       authenveloped_context->calls == 2 && authenveloped_context->flags == GMIME_DECRYPT_NO_VERIFY,
	       "authentication failure discards plaintext even when signature verification is disabled");
	g_clear_object (&decrypted);
	g_clear_object (&result);
	g_clear_error (&error);
	g_mime_crypto_context_register ("application/pkcs7-mime", g_mime_pkcs7_context_new);
	g_clear_object (&authenveloped_context);
#endif
	g_object_unref (parsed);
}

static void
test_crypto_context_class_init (TestCryptoContextClass *klass)
{
	klass->verify = test_crypto_verify;
	klass->encrypt = test_crypto_encrypt;
	klass->decrypt = test_crypto_decrypt;
}

static void
test_crypto_context_init (TestCryptoContext *ctx)
{
}

static gboolean
test_crypto_dispatch (TestCryptoContext *test, guint operation, guint flags, GPtrArray *recipients,
		      GMimeStream *input, GMimeStream *output, GError **error)
{
	GMimeCryptoContext *ctx = GMIME_CRYPTO_CONTEXT (test);
	GMimeSignatureList *signatures;
	GMimeDecryptResult *result;
	gboolean success;

	switch (operation) {
	case 0:
		signatures = g_mime_crypto_context_verify (ctx, flags, input, NULL, output, error);
		success = signatures != NULL;
		g_clear_object (&signatures);
		return success;
	case 1:
		return g_mime_crypto_context_encrypt (ctx, FALSE, NULL, flags, recipients, input, output, error) == 0;
	default:
		result = g_mime_crypto_context_decrypt (ctx, flags, NULL, input, output, error);
		success = result != NULL;
		g_clear_object (&result);
		return success;
	}
}

static void
test_crypto_flags (void)
{
	static const guint masks[] = {
		GMIME_VERIFY_ENABLE_KEYSERVER_LOOKUPS | GMIME_VERIFY_ENABLE_ONLINE_CERTIFICATE_CHECKS,
		GMIME_ENCRYPT_ALWAYS_TRUST | GMIME_ENCRYPT_NO_COMPRESS |
		GMIME_ENCRYPT_SYMMETRIC | GMIME_ENCRYPT_THROW_KEYIDS,
		GMIME_DECRYPT_EXPORT_SESSION_KEY | GMIME_DECRYPT_NO_VERIFY |
		GMIME_DECRYPT_ENABLE_KEYSERVER_LOOKUPS | GMIME_DECRYPT_ENABLE_ONLINE_CERTIFICATE_CHECKS
	};
	TestCryptoContext *ctx = g_object_new (test_crypto_context_get_type (), NULL);
	GMimeStream *input = g_mime_stream_mem_new_with_buffer ("input", 5);
	GMimeStream *output = g_mime_stream_mem_new ();
	GPtrArray *recipients = g_ptr_array_new ();
	GError *error = NULL;
	guint operation, flags, bit, calls, mixed;
	char *unknown;

	for (operation = 0; operation < G_N_ELEMENTS (masks); operation++) {
		flags = masks[operation];
		do {
			calls = ctx->calls;
			check (test_crypto_dispatch (ctx, operation, flags, recipients, input, output, &error) &&
			       !error && ctx->calls == calls + 1 && ctx->flags == flags,
			       "every defined crypto flag combination reaches the backend unchanged");
			g_clear_error (&error);
			flags = (flags - 1) & masks[operation];
		} while (flags != masks[operation]);

		for (bit = 0; bit < sizeof (guint) * 8; bit++) {
			if ((1u << bit) & masks[operation])
				continue;

			for (mixed = 0; mixed < 2; mixed++) {
				flags = (1u << bit) | (mixed ? masks[operation] : 0);
				calls = ctx->calls;
				unknown = g_strdup_printf ("0x%x", 1u << bit);
				check (!test_crypto_dispatch (ctx, operation, flags, recipients, input, output, &error) &&
				       error && error->domain == GMIME_ERROR && error->code == GMIME_ERROR_NOT_SUPPORTED &&
				       strstr (error->message, unknown) && ctx->calls == calls,
				       "undefined crypto bits fail explicitly without reaching the backend");
				g_free (unknown);
				g_clear_error (&error);
				check (!test_crypto_dispatch (ctx, operation, flags, recipients, input, output, NULL) &&
				       ctx->calls == calls, "undefined crypto bits also fail without an error destination");
			}
		}
	}

	check (g_mime_stream_tell (input) == 0 && g_mime_stream_length (output) == 0,
	       "flag validation leaves input and output streams untouched");
	g_ptr_array_unref (recipients);
	g_object_unref (output);
	g_object_unref (input);
	g_object_unref (ctx);
}

static void
test_crypto_backend_handles (void)
{
	GMimeCertificate *cert = g_mime_certificate_new ();
	GMimeGpgContext *gpg = g_object_new (GMIME_TYPE_GPG_CONTEXT, NULL);
	GMimePkcs7Context *pkcs7 = g_object_new (GMIME_TYPE_PKCS7_CONTEXT, NULL);

	check (g_mime_certificate_get_gpgme_key (cert) == NULL,
	       "application-created certificates have no backend key");
	check (g_mime_gpg_context_get_gpgme_ctx (gpg) == NULL &&
	       g_mime_pkcs7_context_get_gpgme_ctx (pkcs7) == NULL,
	       "uninitialized crypto contexts have no backend handle");
#ifndef ENABLE_CRYPTO
	check (g_mime_gpg_context_new () == NULL && g_mime_pkcs7_context_new () == NULL,
	       "crypto-disabled builds expose nullable handles without requiring GPGME");
#endif

	g_object_unref (pkcs7);
	g_object_unref (gpg);
	g_object_unref (cert);
}

static void
test_crypto_dates (void)
{
	static const gint64 timestamps[] = {
		-G_GINT64_CONSTANT (62135596800000000),
		-G_GINT64_CONSTANT (2208988800000000),
		-G_GINT64_CONSTANT (1000000), -1, 0,
		G_GINT64_CONSTANT (2147483648000000),
		G_GINT64_CONSTANT (2208988800123456),
		G_GINT64_CONSTANT (253402300799999999)
	};
	GMimeCertificate *cert = g_mime_certificate_new ();
	GMimeSignature *sig = g_mime_signature_new ();
	GDateTime *date;
	GTimeZone *zone;
	guint i;

	check (g_mime_certificate_get_created (cert) == NULL, "certificate creation defaults to unknown");
	check (g_mime_certificate_get_expires (cert) == NULL, "certificate expiration defaults to unknown");
	check (g_mime_signature_get_created (sig) == NULL, "signature creation defaults to unknown");
	check (g_mime_signature_get_expires (sig) == NULL, "signature expiration defaults to unknown");

	for (i = 0; i < G_N_ELEMENTS (timestamps); i++) {
		date = g_date_time_new_from_unix_utc_usec (timestamps[i]);
		g_mime_certificate_set_created (cert, date);
		g_date_time_unref (date);
		g_mime_certificate_set_created (cert, g_mime_certificate_get_created (cert));
		check (g_mime_certificate_get_created (cert) == date &&
		       g_date_time_to_unix_usec (date) == timestamps[i], "certificate creation retains date and supports self-assignment");

		date = g_date_time_new_from_unix_utc_usec (timestamps[i]);
		g_mime_certificate_set_expires (cert, date);
		g_date_time_unref (date);
		g_mime_certificate_set_expires (cert, g_mime_certificate_get_expires (cert));
		check (g_mime_certificate_get_expires (cert) == date &&
		       g_date_time_to_unix_usec (date) == timestamps[i], "certificate expiration retains date and supports self-assignment");

		date = g_date_time_new_from_unix_utc_usec (timestamps[i]);
		g_mime_signature_set_created (sig, date);
		g_date_time_unref (date);
		g_mime_signature_set_created (sig, g_mime_signature_get_created (sig));
		check (g_mime_signature_get_created (sig) == date &&
		       g_date_time_to_unix_usec (date) == timestamps[i], "signature creation retains date and supports self-assignment");

		date = g_date_time_new_from_unix_utc_usec (timestamps[i]);
		g_mime_signature_set_expires (sig, date);
		g_date_time_unref (date);
		g_mime_signature_set_expires (sig, g_mime_signature_get_expires (sig));
		check (g_mime_signature_get_expires (sig) == date &&
		       g_date_time_to_unix_usec (date) == timestamps[i], "signature expiration retains date and supports self-assignment");
	}

	g_mime_certificate_set_created (cert, NULL);
	g_mime_certificate_set_expires (cert, NULL);
	g_mime_signature_set_created (sig, NULL);
	g_mime_signature_set_expires (sig, NULL);
	check (g_mime_certificate_get_created (cert) == NULL && g_mime_certificate_get_expires (cert) == NULL,
	       "certificate dates can be cleared");
	check (g_mime_signature_get_created (sig) == NULL && g_mime_signature_get_expires (sig) == NULL,
	       "signature dates can be cleared");

	zone = g_time_zone_new_offset (19800);
	date = g_date_time_new (zone, 2040, 1, 2, 3, 4, 5.123456);
	g_time_zone_unref (zone);
	g_mime_certificate_set_created (cert, date);
	g_mime_certificate_set_expires (cert, date);
	g_mime_signature_set_created (sig, date);
	g_mime_signature_set_expires (sig, date);
	check (g_mime_certificate_get_created (cert) == date && g_mime_certificate_get_expires (cert) == date &&
	       g_mime_signature_get_created (sig) == date && g_mime_signature_get_expires (sig) == date,
	       "dates preserve caller timezone and microseconds and may be shared");
	g_object_unref (cert);
	g_object_unref (sig);
	check (g_date_time_get_utc_offset (date) == 19800 * G_TIME_SPAN_SECOND &&
	       g_date_time_get_microsecond (date) == 123456, "caller date survives certificate and signature finalization");
	g_date_time_unref (date);
}

static void
test_options (void)
{
	GMimeParserOptions *options, *clone;

	options = g_mime_parser_options_new ();
	check (g_mime_parser_options_get_max_address_group_depth (options) == 3, "default address group depth");
	check (g_mime_parser_options_get_max_mime_depth (options) == 1024, "default MIME depth");
	check (g_mime_parser_options_get_max_address_group_depth (NULL) == 3, "implicit default address group depth");
	check (g_mime_parser_options_get_max_mime_depth (NULL) == 1024, "implicit default MIME depth");
	check (g_mime_parser_options_get_max_header_length (options) == 16 * 1024, "default header length");
	check (g_mime_parser_options_get_max_header_length (NULL) == 16 * 1024, "implicit default header length");
	check (g_mime_parser_options_get_allow_unquoted_commas_in_addresses (options), "default unquoted commas");
	check (g_mime_parser_options_get_allow_unquoted_commas_in_addresses (NULL), "implicit default unquoted commas");
	check (g_mime_parser_options_get_allow_addresses_without_domain (options), "default local addresses");
	check (!g_mime_parser_options_get_respect_content_length (options), "default ignores Content-Length");
	check (!g_mime_parser_options_get_respect_content_length (NULL), "implicit default ignores Content-Length");
	check (!strcmp (g_mime_parser_options_get_charset_encoding (options), "utf-8"), "default charset encoding");
	check (!strcmp (g_mime_parser_options_get_charset_encoding (NULL), "utf-8"), "implicit default charset encoding");

	g_mime_parser_options_set_max_address_group_depth (options, 1);
	g_mime_parser_options_set_max_mime_depth (options, 2);
	g_mime_parser_options_set_max_header_length (options, 4096);
	g_mime_parser_options_set_allow_unquoted_commas_in_addresses (options, FALSE);
	g_mime_parser_options_set_allow_addresses_without_domain (options, FALSE);
	g_mime_parser_options_set_respect_content_length (options, TRUE);
	clone = g_mime_parser_options_clone (options);
	g_mime_parser_options_set_respect_content_length (options, FALSE);
	check (g_mime_parser_options_get_respect_content_length (clone), "independent cloned Content-Length option");
	g_mime_parser_options_free (options);

	check (g_mime_parser_options_get_max_address_group_depth (clone) == 1, "cloned address group depth");
	check (g_mime_parser_options_get_max_mime_depth (clone) == 2, "cloned MIME depth");
	check (g_mime_parser_options_get_max_header_length (clone) == 4096, "cloned header length");
	check (!g_mime_parser_options_get_allow_unquoted_commas_in_addresses (clone), "cloned unquoted commas");
	check (!g_mime_parser_options_get_allow_addresses_without_domain (clone), "cloned local addresses");
	check (g_mime_parser_options_get_respect_content_length (clone), "cloned Content-Length option");
	g_mime_parser_options_free (clone);
}

static void
test_charset_encoding (void)
{
	static const struct {
		const char *charset;
		const char *input;
		const char *expected;
	} cases[] = {
		{ "utf-8", "", "" },
		{ "utf-8", "hello world", "hello world" },
		{ "utf-8", "\xc3\xa9", "\xc3\xa9" },
		{ "utf-8", "\xe9", "\xc3\xa9" },
		{ "utf-8", "bad=?\xe9", "bad=?\xc3\xa9" },
		{ "iso-8859-1", "\xc3\xa9", "\xc3\xa9" },
		{ "windows-1251", "\xc3\xa9", "\xc3\xa9" },
		{ "windows-1251", "\xcf\xf0\xe8\xe2\xe5\xf2", "\xd0\x9f\xd1\x80\xd0\xb8\xd0\xb2\xd0\xb5\xd1\x82" },
		{ "windows-1252", "\x80\x93\x94", "\xe2\x82\xac\xe2\x80\x9c\xe2\x80\x9d" },
		{ "us-ascii", "\xe9", "\xc3\xa9" },
		{ "shift_jis", "\x82\xa0", "\xe3\x81\x82" },
		{ "shift_jis", "\x82\xa0\x82", "\xc2\x82\xc2\xa0\xc2\x82" },
		{ "UTF-8", "\xff", "\xc3\xbf" }
	};
	GMimeParserOptions *options = g_mime_parser_options_new (), *clone;
	GMimeParserOptions *defaults = g_mime_parser_options_get_default ();
	GMimeContentDisposition *disposition;
	GMimeInternetAddressList *addresses;
	GMimeStream *stream;
	GMimeParser *parser;
	GMimeMessage *message;
	GError *error = NULL;
	char *text, *decoded, *saved, *name;
	guint i;

	for (i = 0; i < G_N_ELEMENTS (cases); i++) {
		check (g_mime_parser_options_set_charset_encoding (options, cases[i].charset, &error) &&
		       error == NULL, "configured charset is supported");
		decoded = g_mime_utils_decode_8bit (options, cases[i].input, strlen (cases[i].input));
		check (g_strcmp0 (decoded, cases[i].expected) == 0, "8-bit decoding honors charset fallback order");
		g_free (decoded);
		decoded = g_mime_utils_header_decode_text (options, cases[i].input);
		check (g_strcmp0 (decoded, cases[i].expected) == 0, "header text honors charset encoding");
		g_free (decoded);
		decoded = g_mime_utils_header_decode_phrase (options, cases[i].input);
		check (g_strcmp0 (decoded, cases[i].expected) == 0, "header phrase honors charset encoding");
		g_free (decoded);
		text = g_strdup_printf ("attachment; filename=\"%s\"", cases[i].input);
		disposition = g_mime_content_disposition_parse (options, text);
		check (disposition && g_strcmp0 (g_mime_content_disposition_get_parameter (disposition, "filename"),
					       cases[i].expected) == 0, "raw parameter honors charset encoding");
		g_clear_object (&disposition);
		g_free (text);
		if (*cases[i].input) {
			text = g_strdup_printf ("Subject: %s\n\nbody\n", cases[i].input);
			stream = g_mime_stream_mem_new_with_buffer (text, strlen (text));
			parser = g_mime_parser_new_with_stream (stream);
			message = g_mime_parser_construct_message (parser, options, &error);
			check (message && !error && g_strcmp0 (g_mime_message_get_subject (message), cases[i].expected) == 0,
			       "message subject honors charset encoding");
			g_clear_object (&message);
			g_object_unref (parser);
			g_object_unref (stream);
			g_free (text);
		}
	}
	name = g_strdup ("windows-1251");
	check (g_mime_parser_options_set_charset_encoding (options, name, &error), "charset setter copies its name");
	g_free (name);
	check (g_mime_parser_options_set_charset_encoding (
		options, g_mime_parser_options_get_charset_encoding (options), &error), "charset self-assignment is safe");
	clone = g_mime_parser_options_clone (options);
	check (g_mime_parser_options_set_charset_encoding (options, NULL, &error), "NULL resets charset encoding");
	check (!strcmp (g_mime_parser_options_get_charset_encoding (options), "utf-8"), "reset charset encoding is UTF-8");
	g_mime_parser_options_free (options);
	check (!strcmp (g_mime_parser_options_get_charset_encoding (clone), "windows-1251"),
	       "cloned charset owns an independent name");
	options = clone;
	check (!g_mime_parser_options_set_charset_encoding (options, "x-gmime-unsupported-charset", &error) &&
	       error && error->domain == GMIME_ERROR && error->code == GMIME_ERROR_NOT_SUPPORTED,
	       "unsupported charset is an explicit error");
	g_clear_error (&error);
	check (!g_mime_parser_options_set_charset_encoding (options, "", &error) &&
	       error && error->domain == GMIME_ERROR && error->code == GMIME_ERROR_NOT_SUPPORTED,
	       "empty charset is an explicit error");
	g_clear_error (&error);
	check (!strcmp (g_mime_parser_options_get_charset_encoding (options), "windows-1251"),
	       "failed charset setters preserve the current option");
	decoded = g_mime_utils_decode_8bit (options, "A\0\xc3\xa9", 4);
	check (!memcmp (decoded, "A\0\xc3\xa9", 4), "UTF-8 priority preserves embedded NUL bytes");
	g_free (decoded);
	g_mime_parser_options_set_rfc2047_compliance_mode (options, GMIME_RFC_COMPLIANCE_STRICT);
	decoded = g_mime_utils_header_decode_phrase (options, "\xcf\xf0\xe8\xe2\xe5\xf2");
	check (g_strcmp0 (decoded, "\xd0\x9f\xd1\x80\xd0\xb8\xd0\xb2\xd0\xb5\xd1\x82") == 0,
	       "strict header phrase honors charset encoding");
	g_free (decoded);
	g_mime_parser_options_set_rfc2047_compliance_mode (options, GMIME_RFC_COMPLIANCE_LOOSE);
	decoded = g_mime_utils_header_decode_text (options, "=?iso-8859-1?q?=E9?=");
	check (g_strcmp0 (decoded, "\xc3\xa9") == 0, "declared encoded-word charset overrides fallback encoding");
	g_free (decoded);
	disposition = g_mime_content_disposition_parse (options, "attachment; filename*=iso-8859-1''%E9");
	check (disposition && g_strcmp0 (g_mime_content_disposition_get_parameter (disposition, "filename"),
				       "\xc3\xa9") == 0, "declared parameter charset overrides fallback encoding");
	g_clear_object (&disposition);
	addresses = g_mime_internet_address_list_parse (options, "\xcf\xf0\xe8\xe2\xe5\xf2 <user@example.com>");
	check (addresses && g_mime_internet_address_list_length (addresses) == 1 &&
	       g_strcmp0 (g_mime_internet_address_get_name (g_mime_internet_address_list_get_address (addresses, 0)),
			  "\xd0\x9f\xd1\x80\xd0\xb8\xd0\xb2\xd0\xb5\xd1\x82") == 0,
	       "address display names honor charset encoding");
	g_clear_object (&addresses);
	saved = g_strdup (g_mime_parser_options_get_charset_encoding (defaults));
	check (g_mime_parser_options_set_charset_encoding (defaults, "windows-1252", &error),
	       "default charset encoding can be configured");
	decoded = g_mime_utils_decode_8bit (NULL, "\x80", 1);
	check (g_strcmp0 (decoded, "\xe2\x82\xac") == 0, "NULL options use the configured default charset");
	g_free (decoded);
	clone = g_mime_parser_options_clone (NULL);
	check (!strcmp (g_mime_parser_options_get_charset_encoding (clone), "windows-1252"),
	       "NULL clone copies the configured default charset");
	g_mime_parser_options_free (clone);
	decoded = g_mime_utils_decode_8bit (options, "\x80", 1);
	check (g_strcmp0 (decoded, "\xd0\x82") == 0, "explicit charset overrides global defaults");
	g_free (decoded);
	check (g_mime_parser_options_set_charset_encoding (defaults, saved, &error), "default charset is restored");
	g_free (saved);
	g_mime_parser_options_free (options);
	g_clear_error (&error);
}

static const char mbox_length_body[] =
	"prefix\nFrom embedded@example.com Sat Jan  1 00:00:00 2022\n"
	"Subject: Embedded\n\nembedded\n";
static const char mbox_length_tail[] =
	"From second@example.com Sat Jan  1 00:00:00 2022\n"
	"Subject: Second\nContent-Type: text/plain\n\nlast\n";

static void
check_message_body (GMimeMessage *message, const char *expected)
{
	GMimeStream *output = g_mime_stream_mem_new ();
	GByteArray *bytes;

	check (g_mime_content_decode_to_stream (
		g_mime_part_get_content (GMIME_PART (g_mime_message_get_mime_part (message))),
		output) >= 0, "Content-Length body can be read");
	bytes = g_mime_stream_mem_get_byte_array (GMIME_STREAM_MEM (output));
	check (bytes->len == strlen (expected) &&
	       !memcmp (bytes->data, expected, bytes->len), "Content-Length preserves exact body bytes");
	g_object_unref (output);
}

static void
check_content_length (const char *headers, GMimeParserOptions *options, GMimeFormat format,
		      int expected_count, gboolean onebyte, gboolean persist)
{
	GMimeStream *stream, *input;
	GMimeParser *parser;
	GMimeMessage *message;
	GError *error = NULL;
	char *text, *body, *expected;
	int count = 0;

	body = g_strconcat (mbox_length_body, mbox_length_tail, NULL);
	text = g_strdup_printf ("%sSubject: First\nContent-Type: text/plain\n%s\n%s",
				format == GMIME_FORMAT_MBOX ?
				"From first@example.com Sat Jan  1 00:00:00 2022\n" : "", headers, body);
	if (expected_count == 3)
		expected = g_strdup ("prefix");
	else if (expected_count == 2)
		expected = g_strndup (mbox_length_body, strlen (mbox_length_body) - 1);
	else
		expected = g_strdup (body);
	stream = g_mime_stream_mem_new_with_buffer (text, strlen (text));
	input = onebyte ? test_stream_onebyte_new (stream) : g_object_ref (stream);
	parser = g_mime_parser_new_with_stream (input);
	g_mime_parser_set_format (parser, format);
	g_mime_parser_set_persist_stream (parser, persist);
	while (count < 4) {
		message = g_mime_parser_construct_message (parser, options, &error);
		check (error == NULL, "Content-Length parsing succeeds");
		if (!message)
			break;
		if (count == 0) {
			check_message_body (message, expected);
		} else {
			const char *subject = expected_count == 3 && count == 1 ? "Embedded" : "Second";

			check (g_strcmp0 (g_mime_message_get_subject (message), subject) == 0,
			       "Content-Length selects the expected next message");
		}
		count++;
		g_object_unref (message);
	}
	check (count == expected_count, "Content-Length message count");
	g_clear_error (&error);
	g_object_unref (parser);
	g_object_unref (input);
	g_object_unref (stream);
	g_free (expected);
	g_free (text);
	g_free (body);
}

static void
test_content_length (void)
{
	static const struct {
		const char *headers;
		int count;
	} cases[] = {
		{ "", 3 },
		{ "Content-Length: %u\n", 2 },
		{ "Content-Length:%u\n", 3 },
		{ "Content-Length:\t%u\n", 2 },
		{ "Content-Length:\n %u\n", 2 },
		{ "Content-Length:\r\n\t%u\r\n", 2 },
		{ "Content-Length: 000000000000%u suffix\n", 2 },
		{ "Content-Length: %ux\n", 2 },
		{ "Content-Length: -1\n", 3 },
		{ "Content-Length: +2147483647\n", 3 },
		{ "Content-Length: 2147483647\n", 1 },
		{ "Content-Length: 2147483648\n", 3 },
		{ "Content-Length: 9223372036854775807\n", 3 },
		{ "Content-Length: 18446744073709551616\n", 3 },
		{ "Content-Length: (comment)%u\n", 3 },
		{ "Content-Length: \n", 3 },
		{ "Content-Length: 0x10\n", 3 },
		{ "Content-Length: invalid\nContent-Length: %u\n", 2 },
		{ "Content-Length: 2147483648\nContent-Length: %u\n", 2 },
		{ "Content-Length: %u\nContent-Length: invalid\n", 2 },
		{ "Content-Length: %u\nContent-Length: 2147483647\n", 2 },
		{ "Content-Length: 0\nContent-Length: %u\n", 3 },
		{ "cOnTeNt-LeNgTh: %u\nContent-Length: 0\n", 2 }
	};
	GMimeParserOptions *options = g_mime_parser_options_new (), *clone;
	GMimeParserOptions *defaults = g_mime_parser_options_get_default ();
	gboolean saved_default = g_mime_parser_options_get_respect_content_length (defaults);
	char *headers;
	guint i;
	int onebyte, persist, respect;

	for (onebyte = 0; onebyte < 2; onebyte++) {
		for (persist = 0; persist < 2; persist++) {
			for (i = 0; i < G_N_ELEMENTS (cases); i++) {
				headers = g_strdup_printf (cases[i].headers, (guint) strlen (mbox_length_body));
				for (respect = 0; respect < 2; respect++) {
					g_mime_parser_options_set_respect_content_length (options, respect);
					check_content_length (headers, options, GMIME_FORMAT_MBOX,
							      respect ? cases[i].count : 3, onebyte, persist);
				}
				check_content_length (headers, options, GMIME_FORMAT_MESSAGE, 1, onebyte, persist);
				g_free (headers);
			}
		}
	}
	headers = g_strdup_printf ("Content-Length: %u\n", (guint) strlen (mbox_length_body));
	check_content_length (headers, NULL, GMIME_FORMAT_MBOX, 3, TRUE, FALSE);
	g_mime_parser_options_set_respect_content_length (defaults, TRUE);
	clone = g_mime_parser_options_clone (NULL);
	check (g_mime_parser_options_get_respect_content_length (clone), "cloned default Content-Length option");
	g_mime_parser_options_free (clone);
	check_content_length (headers, NULL, GMIME_FORMAT_MBOX, 2, TRUE, FALSE);
	g_mime_parser_options_set_respect_content_length (options, FALSE);
	check_content_length (headers, options, GMIME_FORMAT_MBOX, 3, TRUE, FALSE);
	g_mime_parser_options_set_respect_content_length (defaults, saved_default);
	g_mime_parser_options_free (options);
	g_free (headers);
}

static void
test_content_length_per_message (void)
{
	const char *subjects[] = { "First", "Second", "Embedded", "Third" };
	GMimeParserOptions *options = g_mime_parser_options_new ();
	GMimeStream *stream, *input;
	GMimeParser *parser;
	GMimeMessage *message;
	GError *error = NULL;
	char *text, *body;
	guint i;

	text = g_strdup_printf (
		"From first@example.com Sat Jan  1 00:00:00 2022\nSubject: First\nContent-Length: %u\n\n%s"
		"From second@example.com Sat Jan  1 00:00:00 2022\nSubject: Second\nContent-Length: %u\n\n%s"
		"From third@example.com Sat Jan  1 00:00:00 2022\nSubject: Third\n\nend\n",
		(guint) strlen (mbox_length_body), mbox_length_body,
		(guint) strlen (mbox_length_body), mbox_length_body);
	body = g_strndup (mbox_length_body, strlen (mbox_length_body) - 1);
	stream = g_mime_stream_mem_new_with_buffer (text, strlen (text));
	input = test_stream_onebyte_new (stream);
	parser = g_mime_parser_new_with_stream (input);
	g_mime_parser_set_format (parser, GMIME_FORMAT_MBOX);
	for (i = 0; i < G_N_ELEMENTS (subjects); i++) {
		g_mime_parser_options_set_respect_content_length (options, i == 0);
		message = g_mime_parser_construct_message (parser, i == 2 ? NULL : options, &error);
		check (message && error == NULL, "Content-Length options can change between messages");
		if (!message)
			break;
		check (g_strcmp0 (g_mime_message_get_subject (message), subjects[i]) == 0,
		       "Content-Length option changes take effect on the current message");
		check_message_body (message, i == 0 ? body : i == 1 ? "prefix" : i == 2 ? "embedded" : "end\n");
		g_object_unref (message);
	}
	check (g_mime_parser_eos (parser), "per-message Content-Length controls consume the whole mbox");
	g_clear_error (&error);
	g_object_unref (parser);
	g_object_unref (input);
	g_object_unref (stream);
	g_mime_parser_options_free (options);
	g_free (text);
	g_free (body);
}

static void
test_content_length_offset_overflow (void)
{
	const char text[] = "From first@example.com Sat Jan  1 00:00:00 2022\n"
		"Content-Length: 2147483647\n\nbody\n";
	GMimeParserOptions *options = g_mime_parser_options_new ();
	TestStreamFailure *input = g_object_new (test_stream_failure_get_type (), NULL);
	GMimeParser *parser;
	GMimeMessage *message;
	GError *error = NULL;

	input->parent_instance.source = g_mime_stream_mem_new_with_buffer (text, sizeof (text) - 1);
	input->fail_after = G_MAXSIZE;
	input->position_offset = G_MAXINT64 - 1024;
	g_mime_stream_construct (GMIME_STREAM (input), input->position_offset, -1);
	parser = g_mime_parser_new_with_stream (GMIME_STREAM (input));
	g_mime_parser_set_format (parser, GMIME_FORMAT_MBOX);
	g_mime_parser_set_persist_stream (parser, FALSE);
	g_mime_parser_options_set_respect_content_length (options, TRUE);
	message = g_mime_parser_construct_message (parser, options, &error);
	check (message == NULL && error && error->domain == GMIME_ERROR &&
	       error->code == GMIME_ERROR_PARSE_ERROR, "Content-Length stream offset overflow is an explicit error");
	g_clear_object (&message);
	g_clear_error (&error);
	g_object_unref (parser);
	g_object_unref (input);
	g_mime_parser_options_free (options);
}

static void
count_warning (gint64 offset, GMimeParserWarning warning, const gchar *item, gpointer user_data)
{
	guint *warnings = user_data;

	if (warning == GMIME_WARN_INVALID_ADDRESS_LIST || warning == GMIME_CRIT_NESTING_OVERFLOW)
		(*warnings)++;
}

static void
test_address_depth (void)
{
	const char *nested = "Outer: Inner: example@example.com;;";
	GMimeParserOptions *options = g_mime_parser_options_new ();
	GMimeInternetAddressList *list, *members;
	GMimeInternetAddress *address;
	guint warnings;
	int limit;

	for (limit = 0; limit <= 2; limit++) {
		g_mime_parser_options_set_max_address_group_depth (options, limit);
		warnings = 0;
		g_mime_parser_options_set_warning_callback (options, count_warning, &warnings);
		list = g_mime_internet_address_list_parse (options, nested);
		check ((warnings != 0) == (limit < 2), "group depth warning");
		if (limit == 0) {
			check (list == NULL, "zero group depth rejects group addresses");
		} else {
			check (list != NULL, "outer address group parsed");
			if (list != NULL) {
				address = g_mime_internet_address_list_get_address (list, 0);
				check (GMIME_IS_GROUP_ADDRESS (address), "outer group type");
				if (GMIME_IS_GROUP_ADDRESS (address)) {
					members = g_mime_group_address_get_members (GMIME_GROUP_ADDRESS (address));
					check (g_mime_internet_address_list_length (members) == (limit == 2 ? 1 : 0),
					       "group nesting stops at configured depth");
				}
			}
		}
		if (list != NULL)
			g_object_unref (list);
	}

	g_mime_parser_options_set_max_address_group_depth (options, 0);
	list = g_mime_internet_address_list_parse (options, "example@example.com");
	check (list != NULL, "zero group depth still accepts mailboxes");
	if (list != NULL) {
		g_mime_internet_address_list_append_parse (list, options,
			"Blocked: member@example.com;, next@example.com");
		check (g_mime_internet_address_list_length (list) == 2,
		       "append recovers mailbox after rejected group");
		g_object_unref (list);
	}

	g_mime_parser_options_set_max_address_group_depth (options, 3);
	warnings = 0;
	list = g_mime_internet_address_list_parse (options,
		"First: Second: Third: Fourth: example@example.com;;;;");
	check (warnings > 0, "default group depth reports overflow");
	members = list;
	for (limit = 0; limit < 3 && members != NULL; limit++) {
		address = g_mime_internet_address_list_get_address (members, 0);
		check (GMIME_IS_GROUP_ADDRESS (address), "default group depth accepts allowed groups");
		members = GMIME_IS_GROUP_ADDRESS (address) ?
			g_mime_group_address_get_members (GMIME_GROUP_ADDRESS (address)) : NULL;
	}
	check (members != NULL && g_mime_internet_address_list_length (members) == 0,
	       "default group depth rejects fourth group");
	if (list != NULL)
		g_object_unref (list);

	g_mime_parser_options_free (options);
}

static void
test_mime_depth (gboolean multipart, gboolean onebyte)
{
	const char *message =
		"Content-Type: message/rfc822\n\n"
		"Content-Type: message/rfc822\n\n"
		"Content-Type: message/rfc822\n\n"
		"Content-Type: text/plain\n\n"
		"Body\n";
	const char *mixed =
		"Content-Type: multipart/mixed; boundary=b0\n\n--b0\n"
		"Content-Type: multipart/mixed; boundary=b1\n\n--b1\n"
		"Content-Type: multipart/mixed; boundary=b2\n\n--b2\n"
		"Content-Type: text/plain\n\nBody\n"
		"--b2--\n--b1--\n--b0--\n";
	const char *text = multipart ? mixed : message;
	GMimeParserOptions *options = g_mime_parser_options_new ();
	GMimeEntity *entity, *current;
	GMimeStream *input, *stream;
	GMimeParser *parser;
	guint warnings;
	int limit, depth;

	for (limit = 0; limit <= 4; limit++) {
		g_mime_parser_options_set_max_mime_depth (options, limit);
		warnings = 0;
		g_mime_parser_options_set_warning_callback (options, count_warning, &warnings);
		input = g_mime_stream_mem_new_with_buffer (text, strlen (text));
		if (onebyte) {
			stream = test_stream_onebyte_new (input);
			g_object_unref (input);
			input = stream;
		}
		parser = g_mime_parser_new_with_stream (input);
		g_object_unref (input);
		entity = g_mime_parser_construct_part (parser, options, NULL);
		check (entity != NULL, "depth-limited entity parsed");
		current = entity;
		depth = 0;

		while (current != NULL) {
			if (GMIME_IS_MULTIPART (current)) {
				check (g_mime_multipart_get_count (GMIME_MULTIPART (current)) == 1,
				       "nested multipart child count");
				current = g_mime_multipart_get_part (GMIME_MULTIPART (current), 0);
			} else if (GMIME_IS_MESSAGE_PART (current)) {
				GMimeMessage *embedded = g_mime_message_part_get_message (GMIME_MESSAGE_PART (current));
				check (embedded != NULL, "embedded message parsed");
				current = embedded ? g_mime_message_get_mime_part (embedded) : NULL;
			} else {
				break;
			}
			depth++;
		}

		check (depth == MIN (limit, 3), "exact MIME depth threshold");
		check ((warnings != 0) == (limit < 3), "MIME depth warning");
		check (GMIME_IS_PART (current), "depth-limited content is a leaf");
		if (GMIME_IS_PART (current)) {
			GMimeContent *content = g_mime_part_get_content (GMIME_PART (current));
			GByteArray *bytes;

			check (content != NULL, "leaf retains content");
			if (content != NULL) {
				stream = g_mime_stream_mem_new ();
				check (g_mime_content_decode_to_stream (content, stream) > 0, "leaf content is readable");
				bytes = g_mime_stream_mem_get_byte_array (GMIME_STREAM_MEM (stream));
				if (limit == 0) {
					const char *body = strstr (text, "\n\n") + 2;
					check (bytes->len == strlen (body) && !memcmp (bytes->data, body, bytes->len),
					       "zero-depth leaf preserves exact body bytes");
				}
				g_object_unref (stream);
			}
		}

		if (entity != NULL)
			g_object_unref (entity);
		g_object_unref (parser);
	}

	g_mime_parser_options_free (options);
}

static void
test_depth_siblings (gboolean multipart, gboolean onebyte, gboolean persist)
{
	const char *message =
		"Content-Type: multipart/mixed; boundary=outer\n\n--outer\n"
		"Content-Type: message/rfc822\n\n"
		"Content-Type: text/plain\n\nNested\n"
		"--outer\nContent-Type: text/plain\n\nSibling\n--outer--\n";
	const char *mixed =
		"Content-Type: multipart/mixed; boundary=outer\n\n--outer\n"
		"Content-Type: multipart/signed; boundary=inner\n\n"
		"--inner\nContent-Type: text/plain\n\nNested\n--inner--\n"
		"--outer\nContent-Type: text/plain\n\nSibling\n--outer--\n";
	const char *expected[] = {
		multipart ? "--inner\nContent-Type: text/plain\n\nNested\n--inner--" :
		            "Content-Type: text/plain\n\nNested",
		"Sibling"
	};
	const char *text = multipart ? mixed : message;
	GMimeParserOptions *options = g_mime_parser_options_new ();
	GMimeStream *input, *stream;
	GMimeMessage *parsed;
	GMimeEntity *entity;
	GMimeParser *parser;
	guint warnings = 0;
	int i;

	g_mime_parser_options_set_max_mime_depth (options, 1);
	g_mime_parser_options_set_warning_callback (options, count_warning, &warnings);
	input = g_mime_stream_mem_new_with_buffer (text, strlen (text));
	if (onebyte) {
		stream = test_stream_onebyte_new (input);
		g_object_unref (input);
		input = stream;
	}
	parser = g_mime_parser_new_with_stream (input);
	g_object_unref (input);
	g_mime_parser_set_persist_stream (parser, persist);

	/* A cutoff must not select a container via the fallback type registry. */
	g_mime_entity_register_type ("application", "octet-stream", GMIME_TYPE_MESSAGE_PART);
	parsed = g_mime_parser_construct_message (parser, options, NULL);
	g_mime_entity_register_type ("application", "octet-stream", GMIME_TYPE_PART);
	entity = parsed ? g_mime_message_get_mime_part (parsed) : NULL;
	check (warnings == 1, "child cutoff emits exactly one nesting warning");
	check (GMIME_IS_MULTIPART (entity), "message root has depth zero");
	if (GMIME_IS_MULTIPART (entity)) {
		GMimeMultipart *parent = GMIME_MULTIPART (entity);

		check (g_mime_multipart_get_count (parent) == 2, "cutoff preserves following sibling");
		for (i = 0; i < g_mime_multipart_get_count (parent) && i < 2; i++) {
			GMimeEntity *child = g_mime_multipart_get_part (parent, i);
			GMimeContent *content;
			GByteArray *bytes;

			check (GMIME_IS_PART (child), "cutoff bypasses container type registration");
			if (!GMIME_IS_PART (child))
				continue;
			if (i == 0)
				check (g_mime_content_type_is_type (g_mime_entity_get_content_type (child),
					multipart ? "multipart" : "message", multipart ? "signed" : "rfc822"),
				       "cutoff retains original content type");
			content = g_mime_part_get_content (GMIME_PART (child));
			check (content != NULL, "cutoff child retains content");
			if (content == NULL)
				continue;
			stream = g_mime_stream_mem_new ();
			check (g_mime_content_decode_to_stream (content, stream) == strlen (expected[i]),
			       "child content length excludes parent boundary");
			bytes = g_mime_stream_mem_get_byte_array (GMIME_STREAM_MEM (stream));
			check (bytes->len == strlen (expected[i]) &&
			       !memcmp (bytes->data, expected[i], bytes->len),
			       "child content preserves exact bytes");
			g_object_unref (stream);
		}
	}
	if (parsed != NULL)
		g_object_unref (parsed);
	g_object_unref (parser);
	g_mime_parser_options_free (options);
}

static void
test_address_commas (void)
{
	static const struct {
		const char *text;
		gboolean allow_commas;
		gboolean allow_local;
		int count;
		const char *first_name;
	} cases[] = {
		{ "Doe, Jane <jane@example.com>", TRUE, TRUE, 1, "Doe, Jane" },
		{ "Doe, Jane <jane@example.com>", TRUE, FALSE, 1, "Doe, Jane" },
		{ "Doe, Jane <jane@example.com>", FALSE, TRUE, 2, "" },
		{ "Doe, Jane <jane@example.com>", FALSE, FALSE, 1, "Jane" },
		{ "\"Doe, Jane\" <jane@example.com>, Other <other@example.com>", FALSE, FALSE, 2, "Doe, Jane" },
		{ "\"local\", Other <other@example.com>", TRUE, TRUE, 2, "" },
		{ "\"local\", Other <other@example.com>", TRUE, FALSE, 1, "Other" },
		{ "one@example.com, Two <two@example.com>", TRUE, FALSE, 2, "" },
		{ "local", FALSE, FALSE, 0, NULL },
		{ "local", FALSE, TRUE, 1, "" },
		{ "local1, local2", TRUE, TRUE, 2, "" },
		{ "Group: Doe, Jane <jane@example.com>;", TRUE, FALSE, 1, "Group" }
	};
	GMimeParserOptions *options = g_mime_parser_options_new ();
	GMimeInternetAddressList *list;
	GMimeInternetAddress *address;
	GMimeStream *stream;
	GMimeParser *parser;
	GMimeMessage *message;
	GError *error = NULL;
	char *text;
	guint i;
	int strict;

	for (strict = 0; strict < 2; strict++) {
		g_mime_parser_options_set_address_compliance_mode (options,
			strict ? GMIME_RFC_COMPLIANCE_STRICT : GMIME_RFC_COMPLIANCE_LOOSE);
		for (i = 0; i < G_N_ELEMENTS (cases); i++) {
			g_mime_parser_options_set_allow_unquoted_commas_in_addresses (options, cases[i].allow_commas);
			g_mime_parser_options_set_allow_addresses_without_domain (options, cases[i].allow_local);
			list = g_mime_internet_address_list_parse (options, cases[i].text);
			check ((list ? g_mime_internet_address_list_length (list) : 0) == cases[i].count,
			       cases[i].text);
			if (list != NULL) {
				address = g_mime_internet_address_list_get_address (list, 0);
				check (!g_strcmp0 (g_mime_internet_address_get_name (address), cases[i].first_name),
				       "comma option preserves expected display name");
				g_object_unref (list);
			}
			list = g_mime_internet_address_list_new ();
			address = g_mime_mailbox_address_new ("", "seed@example.com", NULL);
			g_mime_internet_address_list_add (list, address);
			g_object_unref (address);
			g_mime_internet_address_list_append_parse (list, options, cases[i].text);
			check (g_mime_internet_address_list_length (list) == cases[i].count + 1,
			       "append parsing honors independent address options");
			g_object_unref (list);

			text = g_strconcat ("To: ", cases[i].text, "\n\nBody", NULL);
			stream = g_mime_stream_mem_new_with_buffer (text, strlen (text));
			parser = g_mime_parser_new_with_stream (stream);
			g_object_unref (stream);
			message = g_mime_parser_construct_message (parser, options, &error);
			check (message != NULL && error == NULL, "address options preserve recoverable message parsing");
			if (message != NULL) {
				list = g_mime_message_get_to (message);
				check (g_mime_internet_address_list_length (list) == cases[i].count,
				       "message address headers honor parser options");
				g_object_unref (message);
			}
			g_clear_error (&error);
			g_object_unref (parser);
			g_free (text);
		}
	}

	g_mime_parser_options_free (options);
}

static void
check_header_length (const char *text, int limit, gboolean message, gboolean onebyte, gboolean overflow)
{
	GMimeParserOptions *options = NULL;
	GMimeStream *input, *stream;
	GMimeEntity *entity;
	GMimeParser *parser;
	GError *error = NULL;

	if (limit > 0) {
		options = g_mime_parser_options_new ();
		g_mime_parser_options_set_max_header_length (options, limit);
	}
	input = g_mime_stream_mem_new_with_buffer (text, strlen (text));
	if (onebyte) {
		stream = test_stream_onebyte_new (input);
		g_object_unref (input);
		input = stream;
	}
	parser = g_mime_parser_new_with_stream (input);
	g_mime_parser_set_persist_stream (parser, message != onebyte);
	g_object_unref (input);

	if (message)
		entity = (GMimeEntity *) g_mime_parser_construct_message (parser, options, &error);
	else
		entity = g_mime_parser_construct_part (parser, options, &error);
	check ((entity == NULL) == overflow, "header threshold returns complete object or NULL");
	if (overflow) {
		check (g_error_matches (error, GMIME_ERROR, GMIME_ERROR_HEADER_TOO_LONG),
		       "header overflow reports explicit limit error");
		check (error != NULL && strstr (error->message, "offset ") != NULL,
		       "header error includes stream offset");
	} else {
		check (error == NULL, "accepted header has no error");
		g_clear_object (&entity);
		if (message)
			entity = (GMimeEntity *) g_mime_parser_construct_message (parser, options, &error);
		else
			entity = g_mime_parser_construct_part (parser, options, &error);
		check (entity == NULL && error == NULL, "clean EOF is repeatable without an error");
	}

	g_clear_object (&entity);
	g_clear_error (&error);
	g_object_unref (parser);
	if (options != NULL)
		g_mime_parser_options_free (options);
}

static void
test_header_lengths (void)
{
	const char *headers[] = {
		"X: abc\n",
		"X: abc\r\n",
		"X:\n\tabc\n def\n",
		"X:\r\n\tabc\r\n def\r\n",
		"X: \303\251\n",
		"X \t: abc\n",
		"Longer-Field-Name: abc\n",
		"X: abc:def\n",
		"X:\n",
		"X:\r\n",
		"X: unterminated"
	};
	const int name_lengths[] = { 4095, 4096, 4097, 8192 };
	char *text, *name, *value;
	const int default_limit = 16 * 1024;
	guint i, j;
	int message, onebyte, delta, length;

	for (message = 0; message < 2; message++) {
		for (onebyte = 0; onebyte < 2; onebyte++) {
			for (i = 0; i < G_N_ELEMENTS (headers); i++) {
				length = strlen (headers[i]);
				text = g_strconcat (headers[i], i == G_N_ELEMENTS (headers) - 1 ? "" : "\nBody", NULL);
				for (delta = -1; delta <= 1; delta++)
					check_header_length (text, length + delta, message, onebyte, delta < 0);
				g_free (text);
			}
			check_header_length ("X-One: 1234\nX-Two: 5678\n\nBody", 12, message, onebyte, FALSE);
			check_header_length ("X: 1234\nLonger-Field-Name: 5678\n\nBody", 24, message, onebyte, FALSE);
			check_header_length ("X: 1234\nLonger-Field-Name: 56789\n\nBody", 24, message, onebyte, TRUE);
			check_header_length ("\nBody", 1, message, onebyte, FALSE);
			check_header_length ("X:\n\nBody", 1, message, onebyte, TRUE);
			check_header_length ("X:\r\n\r\nBody", 1, message, onebyte, TRUE);
			check_header_length ("X:", 2, message, onebyte, FALSE);
			check_header_length ("X:", 1, message, onebyte, TRUE);

			for (j = 0; j < G_N_ELEMENTS (name_lengths); j++) {
				name = g_strnfill (name_lengths[j], 'X');
				text = g_strconcat (name, ": ok\r\nX: ok\r\n\r\nBody", NULL);
				check_header_length (text, name_lengths[j] + 6, message, onebyte, FALSE);
				check_header_length (text, name_lengths[j] + 5, message, onebyte, TRUE);
				g_free (text);
				g_free (name);
			}

			value = g_strnfill (8192, 'a');
			text = g_strconcat ("X: ", value, "\r\n\r\nBody", NULL);
			check_header_length (text, 8197, message, onebyte, FALSE);
			check_header_length (text, 8196, message, onebyte, TRUE);
			g_free (text);
			g_free (value);

			value = g_strnfill (128, 'a');
			text = g_strconcat ("Content-Type: multipart/mixed; boundary=b\n\n--b\nX: ", value,
					    "\n\nBody\n--b--\n", NULL);
			check_header_length (text, 64, message, onebyte, TRUE);
			g_free (text);
			text = g_strconcat ("Content-Type: message/rfc822\n\nX: ", value, "\n\nBody", NULL);
			check_header_length (text, 64, message, onebyte, TRUE);
			g_free (text);
			g_free (value);
		}
	}

	text = g_malloc (default_limit + 3);
	memcpy (text, "X: ", 3);
	memset (text + 3, 'a', default_limit - 4);
	text[default_limit] = '\n';
	text[default_limit + 2] = '\0';
	for (message = 0; message < 2; message++) {
		for (onebyte = 0; onebyte < 2; onebyte++) {
			text[default_limit - 1] = '\n';
			text[default_limit + 1] = '\0';
			check_header_length (text, 0, message, onebyte, FALSE);
			check_header_length (text, default_limit, message, onebyte, FALSE);
			text[default_limit - 1] = 'a';
			text[default_limit + 1] = '\n';
			check_header_length (text, 0, message, onebyte, TRUE);
			check_header_length (text, default_limit, message, onebyte, TRUE);
			check_header_length (text, 2 * default_limit, message, onebyte, FALSE);
		}
	}
	g_free (text);
}

static void
test_parser_read_errors (void)
{
	const char *texts[] = {
		"Subject: Example\n\nBody\n",
		"Content-Type: message/rfc822\n\nSubject: Inner\n\nBody\n",
		"Content-Type: text/rfc822-headers\n\nSubject: Inner\n\nBody\n",
		"Content-Type: message/disposition-notification\n\nDisposition: displayed\n",
		"Content-Type: multipart/report; boundary=b; report-type=delivery-status\n\n"
		"--b\nContent-Type: text/plain\n\nReport\n--b--\n",
		"Content-Type: multipart/mixed; boundary=b\n\nPreamble\n--b\n"
		"Subject: Child\n\nBody\n--b--\nEpilogue\n"
	};
	const char *valid = "Subject: Reset\n\nBody\n";
	TestStreamFailure *failure;
	GMimeStream *input, *stream;
	GMimeEntity *entity;
	GMimeParser *parser;
	GError *error = NULL;
	gsize offset;
	guint i, calls;
	int message, onebyte;

	for (message = 0; message < 2; message++) {
		for (onebyte = 0; onebyte < 2; onebyte++) {
			for (i = 0; i < G_N_ELEMENTS (texts); i++) {
				for (offset = 0; offset < strlen (texts[i]); offset++) {
					input = g_mime_stream_mem_new_with_buffer (texts[i], strlen (texts[i]));
					failure = g_object_new (test_stream_failure_get_type (), NULL);
					failure->parent_instance.source = input;
					failure->fail_after = offset;
					failure->onebyte = onebyte;
					failure->nonseekable = onebyte;
					stream = (GMimeStream *) failure;
					g_mime_stream_construct (stream, 0, -1);
					parser = g_mime_parser_new_with_stream (stream);
					g_mime_parser_set_persist_stream (parser, message);
					if (message)
						entity = (GMimeEntity *) g_mime_parser_construct_message (parser, NULL, &error);
					else
						entity = g_mime_parser_construct_part (parser, NULL, &error);
					check (entity == NULL, "read error discards entire partial parse");
					check (g_error_matches (error, GMIME_ERROR, EIO), "read error preserves errno");
					g_clear_object (&entity);
					g_clear_error (&error);
					calls = failure->read_calls;
					entity = g_mime_parser_construct_part (parser, NULL, &error);
					check (entity == NULL && g_error_matches (error, GMIME_ERROR, EIO),
					       "parser failure remains explicit on subsequent calls");
					check (calls == failure->read_calls, "failed parser does not retry input");
					g_clear_object (&entity);
					g_clear_error (&error);
					g_object_unref (stream);

					input = g_mime_stream_mem_new_with_buffer (valid, strlen (valid));
					g_mime_parser_init_with_stream (parser, input);
					g_object_unref (input);
					entity = g_mime_parser_construct_part (parser, NULL, &error);
					check (entity != NULL && error == NULL, "reinitializing parser clears fatal error");
					g_clear_object (&entity);
					g_clear_error (&error);
					g_object_unref (parser);
				}
			}
		}
	}
}

static void
count_header (GMimeParser *parser, const char *name, const char *value, gint64 offset, gpointer user_data)
{
	guint *count = user_data;

	(*count)++;
}

static void
test_parser_errors (void)
{
	static const struct {
		const char *text;
		gboolean valid;
		int code;
		int children;
	} cases[] = {
		{ "", FALSE, 0, 0 },
		{ "Broken", FALSE, GMIME_ERROR_PARSE_ERROR, 0 },
		{ ": empty name\n\nBody", FALSE, GMIME_ERROR_PARSE_ERROR, 0 },
		{ "Broken header\n\nBody", FALSE, GMIME_ERROR_PARSE_ERROR, 0 },
		{ "Subject: ok\nBroken", FALSE, GMIME_ERROR_PARSE_ERROR, 0 },
		{ "Subject: ok\nbroken header\n\nBody", TRUE, 0, 0 },
		{ "Subject: ok\n", TRUE, 0, 0 },
		{ "\nBody", TRUE, 0, 0 },
		/* MimeKit retains the empty child rather than discarding its
		 * opening delimiter and changing the parsed MIME tree. */
		{ "Content-Type: multipart/mixed; boundary=b\n\n--b\n--b\n\nSecond\n--b--\n", TRUE, 0, 2 },
		{ "Content-Type: multipart/mixed; boundary=b\n\n--b\nX: First\n--b\n\nSecond\n--b--\n", TRUE, 0, 2 }
	};
	const char *marker = "From sender@example.com Sat Jan 01 00:00:00 2022\n";
	GMimeParserOptions *options = g_mime_parser_options_new ();
	GMimeStream *input, *stream;
	GMimeParser *parser;
	GMimeEntity *entity;
	GMimeMessage *message;
	GError *error = NULL;
	guint i, count;
	char *text, *value;
	int onebyte, part;

	for (part = 0; part < 2; part++) {
		for (onebyte = 0; onebyte < 2; onebyte++) {
			for (i = 0; i < G_N_ELEMENTS (cases); i++) {
				input = g_mime_stream_mem_new_with_buffer (cases[i].text, strlen (cases[i].text));
				stream = onebyte ? test_stream_onebyte_new (input) : g_object_ref (input);
				parser = g_mime_parser_new_with_stream (stream);
				g_object_unref (stream);
				g_object_unref (input);
				if (part)
					entity = g_mime_parser_construct_part (parser, NULL, &error);
				else
					entity = (GMimeEntity *) g_mime_parser_construct_message (parser, NULL, &error);
				check ((entity != NULL) == cases[i].valid, cases[i].text);
				check (cases[i].code ? g_error_matches (error, GMIME_ERROR, cases[i].code) : error == NULL,
				       "fatal syntax errors are distinct from EOF and recoverable defects");
				if (entity != NULL && cases[i].children > 0) {
					GMimeEntity *multipart = part ? entity : g_mime_message_get_mime_part (GMIME_MESSAGE (entity));

					check (GMIME_IS_MULTIPART (multipart) &&
					       g_mime_multipart_get_count (GMIME_MULTIPART (multipart)) == cases[i].children,
					       "short reads preserve boundary recovery for incomplete part headers");
				}
				g_clear_object (&entity);
				g_clear_error (&error);
				g_object_unref (parser);
			}

			if (!part) {
				text = g_strconcat (marker, "Subject: Leading marker\n\nBody\n", NULL);
				input = g_mime_stream_mem_new_with_buffer (text, strlen (text));
				stream = onebyte ? test_stream_onebyte_new (input) : g_object_ref (input);
				parser = g_mime_parser_new_with_stream (stream);
				g_object_unref (stream);
				g_object_unref (input);
				message = g_mime_parser_construct_message (parser, NULL, &error);
				check (message != NULL && error == NULL, "single-message parser recovers leading From-line");
				if (message != NULL) {
					check (!g_strcmp0 (g_mime_message_get_subject (message), "Leading marker"),
					       "leading From-line does not swallow subsequent headers");
					g_object_unref (message);
				}
				g_clear_error (&error);
				g_object_unref (parser);
				g_free (text);
			}

			value = g_strnfill (128, 'a');
			text = g_strconcat (marker, "Subject: One\n\nBody\n", marker, "X: ", value, "\n\nBody\n", NULL);
			g_free (value);
			input = g_mime_stream_mem_new_with_buffer (text, strlen (text));
			stream = onebyte ? test_stream_onebyte_new (input) : g_object_ref (input);
			parser = g_mime_parser_new_with_stream (stream);
			g_object_unref (stream);
			g_object_unref (input);
			g_mime_parser_set_format (parser, GMIME_FORMAT_MBOX);
			g_mime_parser_options_set_max_header_length (options, 64);
			count = 0;
			g_mime_parser_set_header_regex (parser, ".*", count_header, &count);
			message = g_mime_parser_construct_message (parser, options, &error);
			check (message != NULL && error == NULL, "mbox message before oversized header is preserved");
			g_clear_object (&message);
			check (count == 1, "accepted header invokes callback once");
			message = g_mime_parser_construct_message (parser, options, &error);
			check (message == NULL && g_error_matches (error, GMIME_ERROR, GMIME_ERROR_HEADER_TOO_LONG),
			       "next mbox message reports oversized header");
			check (g_mime_parser_get_headers_begin (parser) == strstr (text, "X: ") - text,
			       "header failure retains absolute mbox header offset");
			check (count == 1, "oversized header is not exposed through callbacks");
			g_clear_object (&message);
			g_clear_error (&error);
			entity = g_mime_parser_construct_part (parser, options, NULL);
			check (entity == NULL, "error reporting is optional without losing sticky failure");
			g_clear_object (&entity);
			message = g_mime_parser_construct_message (parser, options, &error);
			check (message == NULL && g_error_matches (error, GMIME_ERROR, GMIME_ERROR_HEADER_TOO_LONG),
			       "header limit failure persists across parse calls");
			g_clear_object (&message);
			g_clear_error (&error);
			g_object_unref (parser);
			g_free (text);
		}
	}

	g_mime_parser_options_free (options);
}

static void
test_bounded_streams (void)
{
	const char *prefix = "Padding\n";
	const char *text = "Subject: Bounded\r\n\r\nBody\r\n";
	const char *suffix = "Not part of this message";
	GMimeStream *input, *stream, *output;
	GMimeParser *parser;
	GMimeMessage *message;
	GByteArray *bytes;
	GError *error = NULL;
	gint64 start = strlen (prefix), end = start + strlen (text);
	char *path = NULL;
	int file, onebyte, fd;

	for (file = 0; file < 2; file++) {
		for (onebyte = 0; onebyte < 2; onebyte++) {
			if (file) {
				fd = g_file_open_tmp ("gmime-parser-XXXXXX", &path, &error);
				check (fd != -1 && error == NULL, "create bounded filesystem stream");
				if (fd == -1) {
					g_clear_error (&error);
					continue;
				}
				input = g_mime_stream_fs_new (fd);
			} else {
				input = g_mime_stream_mem_new ();
			}
			g_mime_stream_write_string (input, prefix);
			g_mime_stream_write_string (input, text);
			g_mime_stream_write_string (input, suffix);
			g_mime_stream_set_bounds (input, start, end);
			g_mime_stream_reset (input);
			stream = onebyte ? test_stream_onebyte_new (input) : g_object_ref (input);
			parser = g_mime_parser_new_with_stream (stream);
			g_object_unref (stream);
			g_object_unref (input);
			message = g_mime_parser_construct_message (parser, NULL, &error);
			check (message != NULL && error == NULL, "exact stream boundary is EOF, not an I/O error");
			check (g_mime_parser_get_headers_begin (parser) == start, "bounded input retains header offset");
			if (message != NULL) {
				output = g_mime_stream_mem_new ();
				g_mime_content_decode_to_stream (g_mime_part_get_content (
					GMIME_PART (g_mime_message_get_mime_part (message))), output);
				bytes = g_mime_stream_mem_get_byte_array (GMIME_STREAM_MEM (output));
				check (bytes->len == 6 && !memcmp (bytes->data, "Body\r\n", 6),
				       "bounded parse neither truncates content nor consumes suffix");
				g_object_unref (output);
				g_object_unref (message);
			}
			g_clear_error (&error);
			g_object_unref (parser);
			if (file) {
				g_unlink (path);
				g_clear_pointer (&path, g_free);
			}
		}
	}
}

static void
test_format_line_length (void)
{
	static const guint invalid[] = { 0, 59, 999, G_MAXUINT };
	GMimeFormatOptions *options = g_mime_format_options_new (), *clone;
	guint i;

	check (g_mime_format_options_get_max_line_length (NULL) == 78, "default formatting line length");
	check (g_mime_format_options_get_max_line_length (options) == 78, "new formatting line length");
	clone = g_mime_format_options_clone (NULL);
	check (g_mime_format_options_get_max_line_length (clone) == 78, "cloned default line length");
	g_mime_format_options_free (clone);

	g_mime_format_options_set_max_line_length (options, 120);
	g_mime_format_options_set_newline_format (options, GMIME_NEWLINE_FORMAT_DOS);
	clone = g_mime_format_options_clone (options);
	g_mime_format_options_set_max_line_length (options, 60);
	check (g_mime_format_options_get_max_line_length (options) == 60, "minimum formatting line length");
	check (g_mime_format_options_get_max_line_length (clone) == 120 &&
	       g_mime_format_options_get_newline_format (clone) == GMIME_NEWLINE_FORMAT_DOS,
	       "clone independently retains line length and newline format");
	g_mime_format_options_free (clone);

	g_mime_format_options_set_max_line_length (options, 998);
	check (g_mime_format_options_get_max_line_length (options) == 998, "maximum formatting line length");
	clone = g_boxed_copy (GMIME_TYPE_FORMAT_OPTIONS, options);
	check (g_mime_format_options_get_max_line_length (clone) == 998, "boxed copy retains line length");
	g_mime_format_options_free (clone);

	for (i = 0; i < G_N_ELEMENTS (invalid); i++) {
		g_test_expect_message ("gmime", G_LOG_LEVEL_CRITICAL, "*max_line_length >= 60*");
		g_mime_format_options_set_max_line_length (options, invalid[i]);
		g_test_assert_expected_messages ();
		check (g_mime_format_options_get_max_line_length (options) == 998, "invalid length preserves setting");
	}
	g_test_expect_message ("gmime", G_LOG_LEVEL_CRITICAL, "*options != NULL*");
	g_mime_format_options_set_max_line_length (NULL, 120);
	g_test_assert_expected_messages ();
	g_test_expect_message ("gmime", G_LOG_LEVEL_CRITICAL, "*options != default_options*");
	g_mime_format_options_set_max_line_length (g_mime_format_options_get_default (), 120);
	g_test_assert_expected_messages ();
	check (g_mime_format_options_get_max_line_length (NULL) == 78, "formatting defaults remain immutable");
	g_mime_format_options_free (options);
}

static GMimeEntity *
parse_core_entity (const char *text, int limit, gboolean onebyte, guint *warnings)
{
	GMimeParserOptions *options = g_mime_parser_options_new ();
	GMimeStream *input = g_mime_stream_mem_new_with_buffer (text, strlen (text));
	GMimeParser *parser;
	GMimeEntity *entity;
	GError *error = NULL;

	if (onebyte) {
		GMimeStream *stream = test_stream_onebyte_new (input);
		g_object_unref (input);
		input = stream;
	}
	g_mime_parser_options_set_max_mime_depth (options, limit);
	if (warnings)
		g_mime_parser_options_set_warning_callback (options, count_warning, warnings);
	parser = g_mime_parser_new_with_stream (input);
	g_mime_parser_set_persist_stream (parser, !onebyte);
	entity = g_mime_parser_construct_part (parser, options, &error);
	g_assert_no_error (error);
	g_assert_nonnull (entity);
	g_object_unref (parser);
	g_object_unref (input);
	g_mime_parser_options_free (options);
	return entity;
}

static void
test_text_rfc822_headers (void)
{
	static const struct {
		const char *payload;
		const char *subject;
		const char *body;
	} cases[] = {
		{ "", NULL, "" },
		{ "\n", NULL, "" },
		{ "Subject: Inner", "Inner", "" },
		{ "Subject: Inner\n", "Inner", "" },
		{ "Subject: Inner\n\n", "Inner", "" },
		{ "Subject: Inner\n\nbody", "Inner", "body" },
		{ "Subject: Inner\r\n\r\nbody", "Inner", "body" },
		{ "Subject: Inner\nContent-Type: text/html\n\n<b>body</b>", "Inner", "<b>body</b>" }
	};
	static const char *encodings[] = {
		"", "7bit", "8bit", "binary", "unknown", "base64", "quoted-printable", "x-uuencode",
		"7bit\nContent-Transfer-Encoding: base64", "base64\nContent-Transfer-Encoding: 7bit"
	};
	const char *nested = "Content-Type: text/rfc822-headers\n\n"
		"Subject: Outer\nContent-Type: text/rfc822-headers\n\nSubject: Inner\n\nBody";
	const char *mixed = "Content-Type: multipart/mixed; boundary=b\n\n--b\n"
		"Content-Type: text/rfc822-headers\nContent-Id: <headers@host>\n\n"
		"Subject: Inner\n\nBody\n--b\nContent-Type: text/plain\n\nSibling\n--b--\n";
	GMimeTextRfc822Headers *headers = g_mime_text_rfc822_headers_new ();
	GMimeEntity *entity, *parsed, *body;
	GMimeMessage *message, *weak;
	GMimeStream *output;
	GByteArray *bytes;
	char *text, *serialized, *decoded;
	guint i, warnings;
	int onebyte, limit;

	check (GMIME_IS_MESSAGE_PART (headers) && !GMIME_IS_PART (headers) &&
	       g_mime_content_type_is_type (g_mime_entity_get_content_type (GMIME_ENTITY (headers)), "text", "rfc822-headers"),
	       "RFC822 headers constructor creates an embedded-message subtype");
	check (g_mime_message_part_get_message (GMIME_MESSAGE_PART (headers)) == NULL,
	       "RFC822 headers initially contain no message");
	message = g_mime_message_new (FALSE);
	weak = message;
	g_object_add_weak_pointer (G_OBJECT (message), (gpointer *) &weak);
	g_mime_message_part_set_message (GMIME_MESSAGE_PART (headers), message);
	g_object_unref (message);
	message = g_mime_message_part_get_message (GMIME_MESSAGE_PART (headers));
	g_mime_message_part_set_message (GMIME_MESSAGE_PART (headers), message);
	check (weak == message, "RFC822 headers retain the message across self-assignment");
	g_mime_message_part_set_message (GMIME_MESSAGE_PART (headers), NULL);
	check (!weak && !g_mime_message_part_get_message (GMIME_MESSAGE_PART (headers)),
	       "clearing RFC822 headers releases the retained message");
	g_object_unref (headers);
	entity = g_mime_entity_new_type (NULL, "TEXT", "RFC822-HEADERS");
	check (GMIME_IS_TEXT_RFC822_HEADERS (entity), "RFC822 headers factory registration ignores media-type case");
	g_object_unref (entity);

	for (onebyte = 0; onebyte < 2; onebyte++) {
		for (i = 0; i < G_N_ELEMENTS (cases); i++) {
			text = g_strconcat ("Content-Type: TeXt/RFC822-Headers\nContent-Id: <headers@host>\n"
					    "X-Outer: preserved\n\n", cases[i].payload, NULL);
			entity = parse_core_entity (text, 16, onebyte, NULL);
			g_free (text);
			for (limit = 0; limit < 2; limit++) {
				g_assert_true (GMIME_IS_TEXT_RFC822_HEADERS (entity));
				check (!strcmp (g_mime_entity_get_content_id (entity), "headers@host") &&
				       !strcmp (g_mime_entity_get_header (entity, "X-Outer"), "preserved"),
				       "RFC822 headers preserve outer headers");
				message = g_mime_message_part_get_message (GMIME_MESSAGE_PART (entity));
				g_assert_nonnull (message);
				check (!g_strcmp0 (g_mime_message_get_subject (message), cases[i].subject),
				       "RFC822 headers parse encapsulated message fields");
				body = g_mime_message_get_mime_part (message);
				g_assert_true (GMIME_IS_TEXT_PART (body));
				decoded = g_mime_text_part_get_text (GMIME_TEXT_PART (body));
				check (!g_strcmp0 (decoded, cases[i].body), "RFC822 headers retain empty and optional embedded bodies");
				g_free (decoded);
				if (limit == 0) {
					serialized = g_mime_entity_to_string (entity, NULL);
					g_assert_nonnull (serialized);
					parsed = parse_core_entity (serialized, 16, onebyte, NULL);
					g_free (serialized);
					g_object_unref (entity);
					entity = parsed;
				}
			}
			g_object_unref (entity);
		}
		for (i = 0; i < G_N_ELEMENTS (encodings); i++) {
			gboolean encoded = i == 5 || i == 6 || i == 7 || i == 9;
			text = g_strdup_printf ("Content-Type: text/rfc822-headers\nContent-Transfer-Encoding: %s\n\n"
						"Subject: Inner\n\nBody", encodings[i]);
			entity = parse_core_entity (text, 16, onebyte, NULL);
			g_free (text);
			check (g_mime_content_type_is_type (g_mime_entity_get_content_type (entity), "text", "rfc822-headers") &&
			       (encoded ? GMIME_IS_TEXT_PART (entity) : GMIME_IS_TEXT_RFC822_HEADERS (entity)),
			       "first transfer encoding controls RFC822 headers' text-leaf fallback");
			check (g_mime_header_list_get_count (g_mime_entity_get_header_list (entity)) == (i >= 8 ? 3 : 2),
			       "RFC822 headers' text-leaf fallback does not synthesize duplicate Content-Type headers");
			if (encoded) {
				output = g_mime_stream_mem_new ();
				check (g_mime_content_write_to_stream (g_mime_part_get_content (GMIME_PART (entity)), output) == 20,
				       "encoded RFC822 headers preserve the raw body length");
				bytes = g_mime_stream_mem_get_byte_array (GMIME_STREAM_MEM (output));
				check (bytes->len == 20 && !memcmp (bytes->data, "Subject: Inner\n\nBody", 20),
				       "encoded RFC822 headers preserve the original body bytes");
				g_object_unref (output);
			}
			g_object_unref (entity);
		}
		for (limit = 0; limit <= 2; limit++) {
			warnings = 0;
			entity = parse_core_entity (nested, limit, onebyte, &warnings);
			body = entity;
			for (i = 0; i < (guint) limit; i++) {
				g_assert_true (GMIME_IS_TEXT_RFC822_HEADERS (body));
				message = g_mime_message_part_get_message (GMIME_MESSAGE_PART (body));
				g_assert_nonnull (message);
				body = g_mime_message_get_mime_part (message);
			}
			check (GMIME_IS_TEXT_PART (body) && warnings == (limit < 2 ? 1 : 0),
			       "RFC822 headers depth limits use text leaves and report the overflow once");
			if (limit < 2)
				check (g_mime_header_list_get_count (g_mime_entity_get_header_list (body)) == 1,
				       "depth-limited RFC822 headers retain exactly the original Content-Type header");
			g_object_unref (entity);
		}
		serialized = g_strnfill (64, 'x');
		text = g_strconcat ("Content-Type: text/rfc822-headers\n\nSubject: ", serialized, "\n\n", NULL);
		check_header_length (text, 74, FALSE, onebyte, FALSE);
		check_header_length (text, 73, FALSE, onebyte, TRUE);
		g_free (serialized);
		g_free (text);
		entity = parse_core_entity (mixed, 16, onebyte, NULL);
		g_assert_true (GMIME_IS_MULTIPART (entity));
		check (g_mime_multipart_get_count (GMIME_MULTIPART (entity)) == 2,
		       "RFC822 headers parsing remains synchronized with multipart siblings");
		body = g_mime_multipart_get_part (GMIME_MULTIPART (entity), 0);
		g_assert_true (GMIME_IS_TEXT_RFC822_HEADERS (body));
		message = g_mime_message_part_get_message (GMIME_MESSAGE_PART (body));
		check_message_body (message, "Body");
		body = g_mime_multipart_get_part (GMIME_MULTIPART (entity), 1);
		decoded = g_mime_text_part_get_text (GMIME_TEXT_PART (body));
		check (!strcmp (decoded, "Sibling"), "RFC822 headers do not consume the following sibling's body");
		g_free (decoded);
		g_object_unref (entity);
	}
}

static void
test_multipart_report (void)
{
	static const gunichar whitespace[] = {
		'\t', '\n', '\v', '\f', '\r', ' ', 0x85, 0xa0, 0x1680,
		0x2000, 0x2001, 0x2002, 0x2003, 0x2004, 0x2005, 0x2006,
		0x2007, 0x2008, 0x2009, 0x200a, 0x2028, 0x2029, 0x202f, 0x205f, 0x3000
	};
	const char *dsn = "Content-Type: MuLtIpArT/RePoRt; boundary=b;\n"
		"\treport-type=delivery-status\nContent-Id: <report@host>\n\n"
		"--b\nContent-Type: text/plain\n\nDelivery failed.\n"
		"--b\nContent-Type: message/delivery-status\n\n"
		"Reporting-MTA: dns; mx.example\n\n"
		"Final-Recipient: rfc822; user@example.com\nAction: failed\nStatus: 5.1.1\n"
		"--b\nContent-Type: text/rfc822-headers\n\nSubject: Original\n\n--b--\n";
	const char *missing = "Content-Type: multipart/report; boundary=b\n\n--b--\n";
	const char *padded = "Content-Type: multipart/report; boundary=b;\n"
		"\tREPORT-Type=\"  delivery-status  \"\n\n--b--\n";
	GMimeMultipartReport *report = g_mime_multipart_report_new (" \tDelivery-Status\r\n");
	GMimeEntity *entity, *parsed, *child;
	GMimeContentType *content_type;
	GMimeParamList *parameters;
	GMimeParam *param;
	GMimeHeader *header;
	GMimeMessage *message;
	GMimeStream *output;
	GByteArray *bytes;
	char buffer[7] = { 0 };
	char *text, *raw, *serialized;
	const char *value;
	guint i, warnings;
	int onebyte, limit;

	check (GMIME_IS_MULTIPART (report) && !strcmp (g_mime_multipart_report_get_report_type (report), "Delivery-Status") &&
	       g_mime_content_type_is_type (g_mime_entity_get_content_type (GMIME_ENTITY (report)), "multipart", "report"),
	       "report constructor creates a multipart/report and trims the report type without changing case");
	check (g_mime_multipart_get_count (GMIME_MULTIPART (report)) == 0, "report constructor does not create report payloads");
	check (g_mime_entity_get_content_type_parameter (GMIME_ENTITY (report), "boundary") != NULL,
	       "report constructor generates its boundary before serialization");
	for (i = 0; i < G_N_ELEMENTS (whitespace); i++) {
		GMimeMultipartReport *constructed;
		char *assignment;
		const char *expected;

		memset (buffer, 0, sizeof (buffer));
		g_unichar_to_utf8 (whitespace[i], buffer);
		text = g_strconcat (buffer, "delivery-status", buffer, NULL);
		expected = whitespace[i] == ' ' || whitespace[i] == '\t' ||
			whitespace[i] == '\r' || whitespace[i] == '\n' ? "delivery-status" : text;
		assignment = g_strconcat (" \t", text, "\r\n", NULL);
		g_mime_multipart_report_set_report_type (report, assignment);
		check (!strcmp (g_mime_multipart_report_get_report_type (report), expected),
		       "report-type assignment trims only ASCII linear whitespace");
		constructed = g_mime_multipart_report_new (assignment);
		check (!strcmp (g_mime_multipart_report_get_report_type (constructed), expected),
		       "report construction preserves non-linear and Unicode whitespace");
		g_object_unref (constructed);
		g_free (assignment);
		g_free (text);
	}
	g_mime_multipart_report_set_report_type (report, "\342\200\213delivery-status\357\273\277");
	check (!strcmp (g_mime_multipart_report_get_report_type (report), "\342\200\213delivery-status\357\273\277"),
	       "report-type trimming preserves zero-width space and byte-order marks");
	g_mime_multipart_report_set_report_type (report, " \t ");
	check (!strcmp (g_mime_multipart_report_get_report_type (report), ""), "empty report types remain present");
	g_mime_multipart_report_set_report_type (report, "delivery-status");
	value = g_mime_multipart_report_get_report_type (report);
	g_mime_multipart_report_set_report_type (report, value + 9);
	check (!strcmp (g_mime_multipart_report_get_report_type (report), "status"), "report-type assignment accepts aliased substrings");
	g_test_expect_message ("gmime", G_LOG_LEVEL_CRITICAL, "*report_type != NULL*");
	g_mime_multipart_report_set_report_type (report, NULL);
	g_test_expect_message ("gmime", G_LOG_LEVEL_CRITICAL, "*g_utf8_validate*");
	g_mime_multipart_report_set_report_type (report, "\377");
	g_test_expect_message ("gmime", G_LOG_LEVEL_CRITICAL, "*report_type != NULL*");
	check (g_mime_multipart_report_new (NULL) == NULL, "report construction rejects NULL explicitly");
	g_test_expect_message ("gmime", G_LOG_LEVEL_CRITICAL, "*g_utf8_validate*");
	check (g_mime_multipart_report_new ("\377") == NULL, "report construction rejects invalid UTF-8 explicitly");
	g_test_assert_expected_messages ();
	check (!strcmp (g_mime_multipart_report_get_report_type (report), "status"), "invalid report types leave the current value unchanged");
	child = GMIME_ENTITY (g_mime_text_part_new ());
	g_mime_text_part_set_text (GMIME_TEXT_PART (child), "Report");
	g_object_add_weak_pointer (G_OBJECT (child), (gpointer *) &child);
	g_mime_multipart_add (GMIME_MULTIPART (report), child);
	g_object_unref (child);
	check (child && g_mime_multipart_get_part (GMIME_MULTIPART (report), 0) == child, "reports retain child ownership");
	serialized = g_mime_entity_to_string (GMIME_ENTITY (report), NULL);
	parsed = parse_core_entity (serialized, 16, FALSE, NULL);
	g_assert_true (GMIME_IS_MULTIPART_REPORT (parsed));
	g_assert_cmpint (g_mime_multipart_get_count (GMIME_MULTIPART (parsed)), ==, 1);
	text = g_mime_text_part_get_text (GMIME_TEXT_PART (g_mime_multipart_get_part (GMIME_MULTIPART (parsed), 0)));
	check (!strcmp (g_mime_multipart_report_get_report_type (GMIME_MULTIPART_REPORT (parsed)), "status") &&
	       text && !strcmp (text, "Report"),
	       "constructed reports round-trip without needing a caller-supplied boundary");
	g_free (text);
	g_free (serialized);
	g_object_unref (parsed);
	g_object_unref (report);
	check (child == NULL, "reports release their retained children on destruction");

	entity = g_mime_entity_new_type (NULL, "MULTIPART", "REPORT");
	check (GMIME_IS_MULTIPART_REPORT (entity), "type factory selects report types case-insensitively");
	g_object_unref (entity);
	content_type = g_mime_content_type_new ("multipart", "report");
	entity = g_mime_entity_new (NULL, content_type);
	g_object_unref (content_type);
	check (GMIME_IS_MULTIPART_REPORT (entity) && !g_mime_multipart_report_get_report_type (GMIME_MULTIPART_REPORT (entity)),
	       "Content-Type factory does not invent a report-type parameter");
	g_object_unref (entity);
	for (onebyte = 0; onebyte < 2; onebyte++) {
		entity = parse_core_entity (missing, 16, onebyte, NULL);
		g_assert_true (GMIME_IS_MULTIPART_REPORT (entity));
		check (g_mime_multipart_report_get_report_type (GMIME_MULTIPART_REPORT (entity)) == NULL,
		       "parsed report types may be absent");
		g_object_unref (entity);
		entity = parse_core_entity (padded, 16, onebyte, NULL);
		report = GMIME_MULTIPART_REPORT (entity);
		header = g_mime_header_list_get_header (g_mime_entity_get_header_list (entity), "Content-Type");
		raw = g_strdup (testsuite_header_raw_text (header));
		value = g_mime_multipart_report_get_report_type (report);
		check (!strcmp (value, "  delivery-status  "), "report-type getters do not trim parsed values");
		g_mime_multipart_report_set_report_type (report, value);
		check (!strcmp (testsuite_header_raw_text (header), raw), "assigning the exact parsed report type preserves padding and raw headers");
		g_free (raw);
		g_mime_multipart_report_set_report_type (report, "\tdelivery-status ");
		parameters = g_mime_content_type_get_parameters (g_mime_entity_get_content_type (entity));
		param = g_mime_param_list_get_parameter (parameters, "report-type");
		g_mime_param_set_charset (param, "utf-8");
		g_mime_param_set_lang (param, "en");
		g_mime_param_set_encoding_method (param, GMIME_PARAM_ENCODING_METHOD_RFC2047);
		raw = g_strdup (testsuite_header_raw_text (header));
		g_mime_multipart_report_set_report_type (report, " delivery-status ");
		check (!strcmp (testsuite_header_raw_text (header), raw), "trim-equivalent report types preserve the raw header");
		g_free (raw);
		g_mime_multipart_report_set_report_type (report, "disposition-notification");
		check (param == g_mime_param_list_get_parameter (parameters, "report-type") &&
		       !strcmp (g_mime_param_get_name (param), "REPORT-Type") &&
		       !strcmp (g_mime_param_get_charset (param), "utf-8") &&
		       !strcmp (g_mime_param_get_lang (param), "en") &&
		       g_mime_param_get_encoding_method (param) == GMIME_PARAM_ENCODING_METHOD_RFC2047 &&
		       !strcmp (g_mime_multipart_get_boundary (GMIME_MULTIPART (report)), "b"),
		       "report-type mutations preserve parameter identity, spelling, metadata and boundary");
		g_mime_param_set_value (param, "feedback-report");
		check (!strcmp (g_mime_multipart_report_get_report_type (report), "feedback-report"), "report types observe direct parameter changes");
		g_mime_param_list_remove (parameters, "report-type");
		check (!g_mime_multipart_report_get_report_type (report), "report types observe parameter removal");
		content_type = g_mime_content_type_new ("multipart", "report");
		g_mime_content_type_set_parameter (content_type, "report-type", "delivery-status");
		g_mime_entity_set_content_type (entity, content_type);
		g_object_unref (content_type);
		check (!strcmp (g_mime_multipart_report_get_report_type (report), "delivery-status"), "report types observe Content-Type replacement");
		g_mime_entity_set_header (entity, "Content-Type", "multipart/report; boundary=c; report-type=feedback-report", NULL);
		check (!strcmp (g_mime_multipart_report_get_report_type (report), "feedback-report"), "report types observe direct header changes");
		g_object_unref (entity);

		for (limit = 0; limit < 2; limit++) {
			warnings = 0;
			entity = parse_core_entity (dsn, limit ? 16 : 0, onebyte, &warnings);
			if (!limit) {
				g_assert_true (GMIME_IS_PART (entity));
				check (!GMIME_IS_MULTIPART_REPORT (entity) && warnings == 1 &&
				       g_mime_content_type_is_type (g_mime_entity_get_content_type (entity), "multipart", "report"),
				       "depth-limited reports retain the declared type as opaque MIME parts");
				output = g_mime_stream_mem_new ();
				g_mime_content_write_to_stream (g_mime_part_get_content (GMIME_PART (entity)), output);
				bytes = g_mime_stream_mem_get_byte_array (GMIME_STREAM_MEM (output));
				check (bytes->len > 0 && bytes->len == strlen (strstr (dsn, "\n\n") + 2) &&
				       !memcmp (bytes->data, strstr (dsn, "\n\n") + 2, bytes->len), "depth-limited reports preserve the complete raw body");
				g_object_unref (output);
			} else {
				for (i = 0; i < 2; i++) {
					g_assert_true (GMIME_IS_MULTIPART_REPORT (entity));
					report = GMIME_MULTIPART_REPORT (entity);
					check (!strcmp (g_mime_multipart_report_get_report_type (report), "delivery-status") &&
					       g_mime_multipart_get_count (GMIME_MULTIPART (report)) == 3,
					       "delivery reports retain their report type and three ordered children across round-trips");
					child = g_mime_multipart_get_part (GMIME_MULTIPART (report), 1);
					check (GMIME_IS_PART (child) && !GMIME_IS_MESSAGE_PART (child) &&
					       g_mime_content_type_is_type (g_mime_entity_get_content_type (child), "message", "delivery-status"),
					       "unported delivery-status payloads remain opaque MIME parts");
					child = g_mime_multipart_get_part (GMIME_MULTIPART (report), 2);
					g_assert_true (GMIME_IS_TEXT_RFC822_HEADERS (child));
					message = g_mime_message_part_get_message (GMIME_MESSAGE_PART (child));
					check (message && !strcmp (g_mime_message_get_subject (message), "Original"), "delivery reports retain original message headers");
					if (i == 0) {
						serialized = g_mime_entity_to_string (entity, NULL);
						parsed = parse_core_entity (serialized, 16, onebyte, NULL);
						g_free (serialized);
						g_object_unref (entity);
						entity = parsed;
					}
				}
				g_mime_entity_set_content_type_parameter (child, "test", "value");
				check (!strcmp (g_mime_multipart_report_get_report_type (GMIME_MULTIPART_REPORT (entity)), "delivery-status"),
				       "child changes do not infer or rewrite the report type");
			}
			g_object_unref (entity);
		}
	}
}

static void
test_disposition_notification (void)
{
	const char *payload = "Reporting-UA: client.example; Test\n"
		"Disposition: manual-action/MDN-sent-manually; displayed\n";
	const char *subtypes[] = { "disposition-notification", "global-disposition-notification" };
	const char *encodings[] = { "7bit", "base64", "quoted-printable" };
	const char *blocks[] = {
		"", "\n", "X-Test: value\n", "X-Test: value\n\n",
		"X-Test: value\n\nignored: body\n", "X-Test: value\nnot a header\n"
	};
	GMimeMessageDispositionNotification *notification = g_mime_message_disposition_notification_new ();
	GMimeHeaderList *fields, *retained;
	GMimeHeader *header;
	GMimeContent *content, *original;
	GMimeEntity *entity, *parsed;
	GMimeStream *stream, *output;
	GByteArray *bytes;
	TestStreamFailure *failure;
	GError *error = NULL;
	char *text, *encoded, *raw, *serialized;
	const char *body;
	guint i, j, offset;
	int onebyte, depth;

	check (GMIME_IS_PART (notification) && !GMIME_IS_MESSAGE_PART (notification) &&
	       g_mime_content_type_is_type (g_mime_entity_get_content_type (GMIME_ENTITY (notification)), "message", "disposition-notification"),
	       "disposition notifications are leaf parts rather than embedded messages");
	check (g_mime_part_get_content (GMIME_PART (notification)) == NULL, "notification construction does not eagerly create content");
	fields = g_mime_message_disposition_notification_get_fields (notification, &error);
	g_assert_no_error (error);
	g_assert_nonnull (fields);
	check (g_mime_header_list_get_count (fields) == 0 && g_mime_part_get_content (GMIME_PART (notification)) != NULL &&
	       fields == g_mime_message_disposition_notification_get_fields (notification, &error),
	       "first field access creates empty content and caches the same field list");
	g_assert_no_error (error);
	output = g_mime_stream_mem_new ();
	check (g_mime_content_decode_to_stream (g_mime_part_get_content (GMIME_PART (notification)), output) == 0 &&
	       g_mime_stream_mem_get_byte_array (GMIME_STREAM_MEM (output))->len == 0,
	       "lazy empty content has no header/body separator until a field-list mutation");
	g_object_unref (output);
	g_mime_header_list_append (fields, "Disposition", "displayed", NULL);
	output = g_mime_stream_mem_new ();
	g_mime_content_decode_to_stream (g_mime_part_get_content (GMIME_PART (notification)), output);
	bytes = g_mime_stream_mem_get_byte_array (GMIME_STREAM_MEM (output));
	text = g_strdup_printf ("Disposition: displayed%s%s", g_mime_format_options_get_newline (NULL), g_mime_format_options_get_newline (NULL));
	check (bytes->len == strlen (text) && !memcmp (bytes->data, text, bytes->len), "field edits serialize content including the header/body separator");
	g_free (text);
	g_object_unref (output);
	original = g_object_ref (g_mime_part_get_content (GMIME_PART (notification)));
	header = g_mime_header_list_get_header (fields, "Disposition");
	g_mime_header_set_value (header, NULL, "deleted", NULL);
	check (g_mime_part_get_content (GMIME_PART (notification)) != original, "contained-header edits replace content immediately");
	g_object_unref (original);
	stream = g_mime_stream_mem_new_with_buffer ("X-Replacement: other\n", 21);
	content = g_mime_content_new_with_stream (stream, GMIME_CONTENT_ENCODING_UNSPECIFIED);
	g_object_unref (stream);
	g_mime_part_set_content (GMIME_PART (notification), content);
	check (g_mime_message_disposition_notification_get_fields (notification, &error) == fields &&
	       !strcmp (g_mime_header_get_value (header), "deleted") && g_mime_part_get_content (GMIME_PART (notification)) == content,
	       "replacing content does not invalidate or refresh an already cached field list");
	g_assert_no_error (error);
	g_mime_header_list_set (fields, "Disposition", "displayed", NULL);
	check (g_mime_part_get_content (GMIME_PART (notification)) != content, "cached field edits replace externally assigned content");
	g_object_unref (content);
	content = g_object_ref (g_mime_part_get_content (GMIME_PART (notification)));
	header = g_mime_header_new ("X-Failure", "value", NULL);
	header->formatter = failing_header_formatter;
	g_test_expect_message ("gmime", G_LOG_LEVEL_WARNING, "*Failed to serialize disposition notification fields*");
	g_mime_header_list_add (fields, header);
	g_test_assert_expected_messages ();
	check (g_mime_part_get_content (GMIME_PART (notification)) == content, "serialization failure is logged without replacing content with partial data");
	g_mime_header_list_remove_header (fields, header);
	g_object_unref (header);
	g_object_unref (content);
	retained = g_object_ref (fields);
	g_object_unref (notification);
	g_mime_header_list_set (retained, "Disposition", "deleted", NULL);
	check (!strcmp (g_mime_header_get_value (g_mime_header_list_get_header (retained, "Disposition")), "deleted"),
	       "retained fields can be modified after their notification is released");
	g_object_unref (retained);

	encoded = g_base64_encode ((const guchar *) payload, strlen (payload));
	for (i = 0; i < G_N_ELEMENTS (subtypes); i++) {
		entity = g_mime_entity_new_type (NULL, "MESSAGE", subtypes[i]);
		check (GMIME_IS_MESSAGE_DISPOSITION_NOTIFICATION (entity), "factories select both disposition notification subtypes");
		g_object_unref (entity);
		for (j = 0; j < G_N_ELEMENTS (encodings); j++) {
			body = j == 1 ? encoded : payload;
			for (onebyte = 0; onebyte < 2; onebyte++) {
				for (depth = 0; depth < 2; depth++) {
					text = g_strdup_printf ("Content-Type: message/%s\nContent-Transfer-Encoding: %s\n\n%s", subtypes[i], encodings[j], body);
					entity = parse_core_entity (text, depth ? 16 : 0, onebyte, NULL);
					g_free (text);
					g_assert_true (GMIME_IS_MESSAGE_DISPOSITION_NOTIFICATION (entity));
					notification = GMIME_MESSAGE_DISPOSITION_NOTIFICATION (entity);
					original = g_mime_part_get_content (GMIME_PART (entity));
					header = g_mime_header_list_get_header (g_mime_entity_get_header_list (entity), "Content-Transfer-Encoding");
					raw = g_strdup (testsuite_header_raw_text (header));
					fields = g_mime_message_disposition_notification_get_fields (notification, &error);
					g_assert_no_error (error);
					g_assert_nonnull (fields);
					check (g_mime_header_list_get_count (fields) == 2 &&
					       !strcmp (g_mime_header_get_value (g_mime_header_list_get_header (fields, "Disposition")), "manual-action/MDN-sent-manually; displayed") &&
					       g_mime_part_get_content (GMIME_PART (entity)) == original &&
					       !strcmp (raw, testsuite_header_raw_text (header)),
					       "lazy notification parsing decodes fields without rewriting content or transfer encoding, even at the depth limit");
					output = g_mime_stream_mem_new ();
					g_mime_content_write_to_stream (original, output);
					bytes = g_mime_stream_mem_get_byte_array (GMIME_STREAM_MEM (output));
					check (bytes->len == strlen (body) && !memcmp (bytes->data, body, bytes->len), "field reads preserve the original raw content bytes");
					g_object_unref (output);
					g_mime_header_list_set (fields, "Disposition", "deleted", NULL);
					g_mime_header_list_append (fields, "X-Notification", "updated", NULL);
					check (!strcmp (raw, testsuite_header_raw_text (header)), "field edits preserve the transfer-encoding header");
					check (g_mime_content_get_encoding (g_mime_part_get_content (GMIME_PART (entity))) == GMIME_CONTENT_ENCODING_UNSPECIFIED,
					       "field edits replace encoded backing data with decoded content");
					g_free (raw);
					serialized = g_mime_entity_to_string (entity, NULL);
					parsed = parse_core_entity (serialized, 16, onebyte, NULL);
					g_free (serialized);
					g_assert_true (GMIME_IS_MESSAGE_DISPOSITION_NOTIFICATION (parsed));
					fields = g_mime_message_disposition_notification_get_fields (GMIME_MESSAGE_DISPOSITION_NOTIFICATION (parsed), &error);
					g_assert_no_error (error);
					g_assert_nonnull (fields);
					check (g_mime_header_list_get_count (fields) == 3 &&
					       !strcmp (g_mime_header_get_value (g_mime_header_list_get_header (fields, "Disposition")), "deleted"),
					       "notification field mutations survive MIME serialization and reparsing");
					g_object_unref (parsed);
					g_object_unref (entity);
				}
			}
		}
	}
	g_free (encoded);

	for (i = 0; i < G_N_ELEMENTS (blocks); i++) {
		text = g_strdup_printf ("Content-Type: message/disposition-notification\n\n%s", blocks[i]);
		entity = parse_core_entity (text, 16, FALSE, NULL);
		g_free (text);
		fields = g_mime_message_disposition_notification_get_fields (GMIME_MESSAGE_DISPOSITION_NOTIFICATION (entity), &error);
		g_assert_no_error (error);
		g_assert_nonnull (fields);
		check (g_mime_header_list_get_count (fields) == (i < 2 ? 0 : i == 5 ? 2 : 1),
		       "notification fields retain invalid wire headers in the first block");
		output = g_mime_stream_mem_new ();
		g_mime_content_decode_to_stream (g_mime_part_get_content (GMIME_PART (entity)), output);
		bytes = g_mime_stream_mem_get_byte_array (GMIME_STREAM_MEM (output));
		check (bytes->len == strlen (blocks[i]) && (!bytes->len || !memcmp (bytes->data, blocks[i], bytes->len)), "loading fields preserves separators and ignored trailing content");
		g_object_unref (output);
		if (i == 2) {
			g_mime_header_set_value (g_mime_header_list_get_header_at (fields, 0), NULL, "changed", NULL);
			output = g_mime_stream_mem_new ();
			g_mime_content_decode_to_stream (g_mime_part_get_content (GMIME_PART (entity)), output);
			bytes = g_mime_stream_mem_get_byte_array (GMIME_STREAM_MEM (output));
			text = g_strconcat ("X-Test: changed", g_mime_format_options_get_newline (NULL), NULL);
			check (bytes->len == strlen (text) && !memcmp (bytes->data, text, bytes->len), "contained-header edits preserve a missing field-block separator");
			g_free (text);
			g_object_unref (output);
		}
		g_mime_header_list_append (fields, "Disposition", "displayed", NULL);
		serialized = g_mime_entity_to_string (entity, NULL);
		check (!strstr (serialized, "ignored: body"), "field changes discard content following the first header block");
		parsed = parse_core_entity (serialized, 16, FALSE, NULL);
		g_free (serialized);
		fields = g_mime_message_disposition_notification_get_fields (GMIME_MESSAGE_DISPOSITION_NOTIFICATION (parsed), &error);
		g_assert_no_error (error);
		check (fields && g_mime_header_list_get_count (fields) == (i < 2 ? 1 : i == 5 ? 3 : 2),
		       "empty, unterminated and invalid notification fields round-trip after changes");
		g_object_unref (parsed);
		g_object_unref (entity);
	}
	text = g_strdup_printf ("Content-Type: multipart/report; boundary=b; report-type=disposition-notification\n\n"
				"--b\nContent-Type: text/plain\n\nReport\n"
				"--b\nContent-Type: message/disposition-notification\n\n%s\n"
				"--b\nContent-Type: text/plain\n\nSibling\n--b--\n", payload);
	entity = parse_core_entity (text, 16, TRUE, NULL);
	g_free (text);
	g_assert_true (GMIME_IS_MULTIPART_REPORT (entity));
	check (g_mime_multipart_get_count (GMIME_MULTIPART (entity)) == 3, "notification parsing preserves report siblings");
	parsed = g_mime_multipart_get_part (GMIME_MULTIPART (entity), 1);
	g_assert_true (GMIME_IS_MESSAGE_DISPOSITION_NOTIFICATION (parsed));
	fields = g_mime_message_disposition_notification_get_fields (GMIME_MESSAGE_DISPOSITION_NOTIFICATION (parsed), &error);
	g_assert_no_error (error);
	check (fields && g_mime_header_list_get_count (fields) == 2, "report notification fields stop at multipart boundaries");
	g_object_unref (entity);
	{
		GMimeParserOptions *options = g_mime_parser_options_new ();
		GMimeParser *parser;
		char *value = g_strnfill (100, 'x');

		text = g_strdup_printf ("Content-Type: message/disposition-notification\n\nX-Test: %s\n", value);
		stream = g_mime_stream_mem_new_with_buffer (text, strlen (text));
		parser = g_mime_parser_new_with_stream (stream);
		g_mime_parser_options_set_max_header_length (options, 64);
		entity = g_mime_parser_construct_part (parser, options, &error);
		g_assert_no_error (error);
		g_assert_true (GMIME_IS_MESSAGE_DISPOSITION_NOTIFICATION (entity));
		fields = g_mime_message_disposition_notification_get_fields (GMIME_MESSAGE_DISPOSITION_NOTIFICATION (entity), &error);
		g_assert_no_error (error);
		g_assert_nonnull (fields);
		check (!strcmp (g_mime_header_get_value (g_mime_header_list_get_header (fields, "X-Test")), value), "lazy notification fields use default options rather than outer header limits");
		g_object_unref (entity);
		g_object_unref (parser);
		g_object_unref (stream);
		g_mime_parser_options_free (options);
		g_free (text);
		g_free (value);
	}

	notification = g_mime_message_disposition_notification_new ();
	text = ": malformed\n";
	stream = g_mime_stream_mem_new_with_buffer (text, strlen (text));
	content = g_mime_content_new_with_stream (stream, GMIME_CONTENT_ENCODING_UNSPECIFIED);
	g_mime_part_set_content (GMIME_PART (notification), content);
	check (!g_mime_message_disposition_notification_get_fields (notification, &error), "notification fields preserve native rejection of empty field names");
	g_assert_error (error, GMIME_ERROR, GMIME_ERROR_PARSE_ERROR);
	g_clear_error (&error);
	g_object_unref (stream);
	stream = g_mime_stream_mem_new_with_buffer (payload, strlen (payload));
	g_mime_content_set_stream (content, stream);
	g_object_unref (stream);
	fields = g_mime_message_disposition_notification_get_fields (notification, &error);
	g_assert_no_error (error);
	check (fields && g_mime_header_list_get_count (fields) == 2, "notification parsing can be retried after malformed content is repaired");
	g_object_unref (content);
	g_object_unref (notification);

	notification = g_mime_message_disposition_notification_new ();
	content = g_mime_content_new ();
	g_mime_part_set_content (GMIME_PART (notification), content);
	check (!g_mime_message_disposition_notification_get_fields (notification, &error), "content without a backing stream fails explicitly");
	g_assert_error (error, GMIME_ERROR, GMIME_ERROR_INVALID_OPERATION);
	g_clear_error (&error);
	stream = g_mime_stream_mem_new_with_buffer (payload, strlen (payload));
	g_mime_content_set_stream (content, stream);
	g_object_unref (stream);
	check (g_mime_message_disposition_notification_get_fields (notification, &error) != NULL, "a failed lazy load can be repaired and retried");
	g_assert_no_error (error);
	g_object_unref (content);
	g_object_unref (notification);
	for (offset = 0; offset < strlen (payload); offset++) {
		notification = g_mime_message_disposition_notification_new ();
		failure = g_object_new (test_stream_failure_get_type (), NULL);
		failure->parent_instance.source = g_mime_stream_mem_new_with_buffer (payload, strlen (payload));
		failure->fail_after = offset;
		failure->onebyte = TRUE;
		g_mime_stream_construct (GMIME_STREAM (failure), 0, -1);
		content = g_mime_content_new_with_stream (GMIME_STREAM (failure), GMIME_CONTENT_ENCODING_UNSPECIFIED);
		g_mime_part_set_content (GMIME_PART (notification), content);
		g_object_unref (content);
		check (!g_mime_message_disposition_notification_get_fields (notification, &error), "lazy field loads do not return partial lists after input errors");
		g_assert_error (error, GMIME_ERROR, EIO);
		g_clear_error (&error);
		failure->bytes_read = 0;
		failure->fail_after = G_MAXSIZE;
		failure->fail_reset_on = failure->reset_calls + 1;
		check (!g_mime_message_disposition_notification_get_fields (notification, &error), "lazy field loads propagate rewind errors");
		g_assert_error (error, GMIME_ERROR, ESPIPE);
		g_clear_error (&error);
		fields = g_mime_message_disposition_notification_get_fields (notification, &error);
		g_assert_no_error (error);
		check (fields && g_mime_header_list_get_count (fields) == 2, "failed notification loads do not poison the cached field list");
		g_object_unref (failure);
		g_object_unref (notification);
	}
}

static char *
encode_delivery_status (const char *text, GMimeContentEncoding encoding)
{
	GMimeStream *output = g_mime_stream_mem_new ();
	GMimeStream *filtered = g_mime_stream_filter_new (output);
	GMimeFilter *filter = g_mime_filter_basic_new (encoding, TRUE);
	GByteArray *bytes;
	char *result;

	g_mime_stream_filter_add (GMIME_STREAM_FILTER (filtered), filter);
	g_object_unref (filter);
	g_assert_cmpint (g_mime_stream_write (filtered, text, strlen (text)), ==, strlen (text));
	g_assert_cmpint (g_mime_stream_flush (filtered), ==, 0);
	bytes = g_mime_stream_mem_get_byte_array (GMIME_STREAM_MEM (output));
	result = g_strndup ((const char *) bytes->data, bytes->len);
	if (encoding == GMIME_CONTENT_ENCODING_UUENCODE) {
		char *wrapped = g_strconcat ("begin 644 status\n", result, "end\n", NULL);
		g_free (result);
		result = wrapped;
	}
	g_object_unref (filtered);
	g_object_unref (output);
	return result;
}

static void
test_delivery_status (void)
{
	const char *payload = "Reporting-MTA: dns; example.org\n\n"
		"Final-Recipient: rfc822; user@example.org\nAction: failed\nStatus: 5.1.1\n\n"
		"Final-Recipient: rfc822; other@example.org\nAction: delivered\n";
	const char *recipient = "Final-Recipient: rfc822; user@example.org\nAction: failed\n\n"
		"Final-Recipient: rfc822; other@example.org\nAction: delivered\n";
	const char *subtypes[] = { "delivery-status", "global-delivery-status" };
	const char *blocks[] = {
		"", "\n", "\r\n\n\r\n", "X-Test: value\n", "X-Test: value\n\n\n",
		"\n\r\nX-Test: value\r\n\r\n\nX-Other: value", "X-Test: value\n\n\r"
	};
	const int counts[] = { 0, 0, 0, 1, 1, 2, 1 };
	const GMimeContentEncoding encodings[] = {
		GMIME_CONTENT_ENCODING_7BIT, GMIME_CONTENT_ENCODING_BASE64,
		GMIME_CONTENT_ENCODING_QUOTEDPRINTABLE, GMIME_CONTENT_ENCODING_UUENCODE
	};
	GMimeMessageDeliveryStatus *status = g_mime_message_delivery_status_new ();
	GMimeHeaderListCollection *groups, *retained;
	GMimeHeaderList *group, *replacement;
	GMimeHeader *header;
	GMimeEntity *entity, *parsed;
	GMimeContent *content, *original;
	GMimeStream *stream, *output;
	GByteArray *bytes;
	TestStreamFailure *failure;
	GError *error = NULL;
	char *text, *encoded, *body, *serialized, *raw;
	guint i, j, k, offset;
	int onebyte, depth;

	check (GMIME_IS_PART (status) && !GMIME_IS_MESSAGE_PART (status) &&
	       g_mime_content_type_is_type (g_mime_entity_get_content_type (GMIME_ENTITY (status)), "message", "delivery-status"),
	       "delivery status is a leaf part with the correct default content type");
	check (!g_mime_part_get_content (GMIME_PART (status)), "delivery status construction is lazy");
	groups = g_mime_message_delivery_status_get_status_groups (status, &error);
	g_assert_no_error (error);
	g_assert_nonnull (groups);
	check (g_mime_header_list_collection_get_count (groups) == 0 &&
	       groups == g_mime_message_delivery_status_get_status_groups (status, &error),
	       "first status access creates and caches an empty collection");
	g_assert_no_error (error);
	output = g_mime_stream_mem_new ();
	check (g_mime_content_decode_to_stream (g_mime_part_get_content (GMIME_PART (status)), output) == 0,
	       "first status access creates truly empty content");
	g_object_unref (output);
	group = g_mime_header_list_new (NULL);
	g_mime_header_list_append (group, "Reporting-MTA", "dns; example.org", NULL);
	g_mime_header_list_collection_add (groups, group);
	output = g_mime_stream_mem_new ();
	g_mime_content_decode_to_stream (g_mime_part_get_content (GMIME_PART (status)), output);
	bytes = g_mime_stream_mem_get_byte_array (GMIME_STREAM_MEM (output));
	text = g_strdup_printf ("Reporting-MTA: dns; example.org%s%s",
			       g_mime_format_options_get_newline (NULL), g_mime_format_options_get_newline (NULL));
	check (bytes->len == strlen (text) && !memcmp (bytes->data, text, bytes->len),
	       "adding groups serializes full header blocks including their separators");
	g_free (text);
	g_object_unref (output);
	original = g_object_ref (g_mime_part_get_content (GMIME_PART (status)));
	header = g_mime_header_list_get_header_at (group, 0);
	g_mime_header_set_value (header, NULL, "dns; changed.org", NULL);
	check (g_mime_part_get_content (GMIME_PART (status)) != original, "contained status-header edits replace content");
	g_object_unref (original);
	replacement = g_mime_header_list_new (NULL);
	g_mime_header_list_append (replacement, "X-Replacement", "other", NULL);
	original = g_object_ref (g_mime_part_get_content (GMIME_PART (status)));
	g_mime_header_list_collection_set_group (groups, 0, replacement);
	check (g_mime_part_get_content (GMIME_PART (status)) == original, "indexed group replacement does not notify or rewrite content");
	g_mime_header_list_append (group, "X-Detached", "ignored", NULL);
	check (g_mime_part_get_content (GMIME_PART (status)) == original, "replaced groups no longer update status content");
	g_mime_header_list_append (replacement, "X-Active", "updated", NULL);
	check (g_mime_part_get_content (GMIME_PART (status)) != original, "replacement group edits remain subscribed");
	g_object_unref (original);
	g_object_unref (group);
	group = replacement;
	g_mime_header_list_collection_add (groups, group);
	g_mime_header_list_append (group, "X-Duplicate", "updated", NULL);
	output = g_mime_stream_mem_new ();
	g_mime_content_decode_to_stream (g_mime_part_get_content (GMIME_PART (status)), output);
	bytes = g_mime_stream_mem_get_byte_array (GMIME_STREAM_MEM (output));
	text = g_strndup ((const char *) bytes->data, bytes->len);
	check (strstr (text, "X-Duplicate: updated") &&
	       strstr (strstr (text, "X-Duplicate: updated") + 1, "X-Duplicate: updated"),
	       "duplicate groups serialize independently after shared-header changes");
	g_free (text);
	g_object_unref (output);
	g_mime_header_list_collection_remove (groups, group);
	stream = g_mime_stream_mem_new_with_buffer ("X-Replacement: content\n", 23);
	content = g_mime_content_new_with_stream (stream, GMIME_CONTENT_ENCODING_UNSPECIFIED);
	g_object_unref (stream);
	g_mime_part_set_content (GMIME_PART (status), content);
	check (g_mime_message_delivery_status_get_status_groups (status, &error) == groups &&
	       g_mime_part_get_content (GMIME_PART (status)) == content, "replacing inherited content preserves cached status groups");
	g_assert_no_error (error);
	g_mime_header_list_append (group, "X-Cached", "updated", NULL);
	check (g_mime_part_get_content (GMIME_PART (status)) != content, "cached groups replace externally assigned content when edited");
	g_object_unref (content);
	content = g_object_ref (g_mime_part_get_content (GMIME_PART (status)));
	header = g_mime_header_new ("X-Failure", "value", NULL);
	header->formatter = failing_header_formatter;
	g_test_expect_message ("gmime", G_LOG_LEVEL_WARNING, "*Failed to serialize delivery status groups*");
	g_mime_header_list_add (group, header);
	g_test_assert_expected_messages ();
	check (g_mime_part_get_content (GMIME_PART (status)) == content, "failed group serialization warns and preserves previous content");
	g_mime_header_list_remove_header (group, header);
	g_object_unref (header);
	g_object_unref (content);
	g_mime_header_list_collection_clear (groups);
	output = g_mime_stream_mem_new ();
	check (g_mime_content_decode_to_stream (g_mime_part_get_content (GMIME_PART (status)), output) == 0,
	       "clearing status groups replaces the content with an empty stream");
	g_object_unref (output);
	g_mime_header_list_collection_add (groups, group);
	retained = g_object_ref (groups);
	g_object_unref (status);
	g_mime_header_list_append (group, "X-Retained", "value", NULL);
	g_mime_header_list_collection_clear (retained);
	check (g_mime_header_list_collection_get_count (retained) == 0, "retained status groups remain usable after entity finalization");
	g_object_unref (retained);
	g_object_unref (group);

	for (i = 0; i < G_N_ELEMENTS (subtypes); i++) {
		entity = g_mime_entity_new_type (NULL, "MESSAGE", subtypes[i]);
		check (GMIME_IS_MESSAGE_DELIVERY_STATUS (entity), "factories select both delivery status subtypes");
		g_object_unref (entity);
		for (j = 0; j < G_N_ELEMENTS (encodings); j++) {
			encoded = encode_delivery_status (payload, encodings[j]);
			for (onebyte = 0; onebyte < 2; onebyte++) {
				for (depth = 0; depth < 2; depth++) {
					text = g_strdup_printf ("Content-Type: message/%s\nContent-Transfer-Encoding: %s\n\n%s",
							       subtypes[i], g_mime_content_encoding_to_string (encodings[j]), encoded);
					entity = parse_core_entity (text, depth ? 16 : 0, onebyte, NULL);
					g_free (text);
					g_assert_true (GMIME_IS_MESSAGE_DELIVERY_STATUS (entity));
					status = GMIME_MESSAGE_DELIVERY_STATUS (entity);
					original = g_mime_part_get_content (GMIME_PART (entity));
					header = g_mime_header_list_get_header (g_mime_entity_get_header_list (entity), "Content-Transfer-Encoding");
					raw = g_strdup (testsuite_header_raw_text (header));
					groups = g_mime_message_delivery_status_get_status_groups (status, &error);
					g_assert_no_error (error);
					g_assert_nonnull (groups);
					check (g_mime_header_list_collection_get_count (groups) == 3 &&
					       g_mime_part_get_content (GMIME_PART (entity)) == original &&
					       !strcmp (raw, testsuite_header_raw_text (header)),
					       "lazy status parsing decodes all groups without rewriting content, even at the depth limit");
					group = g_mime_header_list_collection_get_group (groups, 1);
					check (g_mime_header_list_get_count (group) == 3 &&
					       !strcmp (g_mime_header_get_value (g_mime_header_list_get_header (group, "Action")), "failed"),
					       "recipient fields are parsed in the correct group");
					output = g_mime_stream_mem_new ();
					g_mime_content_write_to_stream (original, output);
					bytes = g_mime_stream_mem_get_byte_array (GMIME_STREAM_MEM (output));
					check (bytes->len == strlen (encoded) && !memcmp (bytes->data, encoded, bytes->len), "group reads preserve raw status bytes");
					g_object_unref (output);
					g_mime_header_list_set (group, "Action", "delivered", NULL);
					check (!strcmp (raw, testsuite_header_raw_text (header)) &&
					       g_mime_content_get_encoding (g_mime_part_get_content (GMIME_PART (entity))) == GMIME_CONTENT_ENCODING_UNSPECIFIED,
					       "status edits preserve the desired transfer encoding but store decoded content");
					g_free (raw);
					serialized = g_mime_entity_to_string (entity, NULL);
					parsed = parse_core_entity (serialized, 16, onebyte, NULL);
					g_free (serialized);
					groups = g_mime_message_delivery_status_get_status_groups (GMIME_MESSAGE_DELIVERY_STATUS (parsed), &error);
					g_assert_no_error (error);
					g_assert_nonnull (groups);
					check (g_mime_header_list_collection_get_count (groups) == 3 &&
					       !strcmp (g_mime_header_get_value (g_mime_header_list_get_header (
					       g_mime_header_list_collection_get_group (groups, 1), "Action")), "delivered"),
					       "status mutations survive MIME serialization and reparsing");
					g_object_unref (parsed);
					g_object_unref (entity);
				}
			}
			g_free (encoded);
		}
	}

	for (i = 0; i < G_N_ELEMENTS (blocks); i++) {
		for (onebyte = 0; onebyte < 2; onebyte++) {
			text = g_strdup_printf ("Content-Type: message/delivery-status\n\n%s", blocks[i]);
			entity = parse_core_entity (text, 16, onebyte, NULL);
			g_free (text);
			groups = g_mime_message_delivery_status_get_status_groups (GMIME_MESSAGE_DELIVERY_STATUS (entity), &error);
			g_assert_no_error (error);
			g_assert_nonnull (groups);
			check (g_mime_header_list_collection_get_count (groups) == counts[i],
			       "empty blocks, mixed separators, unterminated groups and a terminal CR do not invent groups");
			g_object_unref (entity);
		}
	}

	for (i = 1; i < G_N_ELEMENTS (encodings); i++) {
		encoded = encode_delivery_status (recipient, encodings[i]);
		text = g_strdup_printf ("Reporting-MTA: dns; example.org\nContent-Transfer-Encoding: %s\n\n%s",
					g_mime_content_encoding_to_string (encodings[i]), encoded);
		for (j = 0; j < G_N_ELEMENTS (encodings); j++) {
			body = encode_delivery_status (text, encodings[j]);
			for (onebyte = 0; onebyte < 2; onebyte++) {
				status = g_mime_message_delivery_status_new ();
				failure = g_object_new (test_stream_failure_get_type (), NULL);
				failure->parent_instance.source = g_mime_stream_mem_new_with_buffer (body, strlen (body));
				failure->fail_after = G_MAXSIZE;
				failure->nonseekable = TRUE;
				failure->onebyte = onebyte;
				g_mime_stream_construct (GMIME_STREAM (failure), 0, -1);
				content = g_mime_content_new_with_stream (GMIME_STREAM (failure), encodings[j]);
				g_mime_part_set_content (GMIME_PART (status), content);
				groups = g_mime_message_delivery_status_get_status_groups (status, &error);
				g_assert_no_error (error);
				g_assert_nonnull (groups);
				check (g_mime_header_list_collection_get_count (groups) == 3 &&
				       g_mime_part_get_content (GMIME_PART (status)) == content,
				       "Office365 recovery handles every inner/outer encoding combination and nonseekable read-ahead");
				g_object_unref (content);
				g_object_unref (failure);
				g_object_unref (status);
			}
			g_free (body);
		}
		g_free (text);
		g_free (encoded);
	}

	{
		const char *values[] = { "7bit", "8bit", "binary", "unknown", "7bit\nContent-Transfer-Encoding: base64" };
		for (i = 0; i < G_N_ELEMENTS (values); i++) {
			text = g_strdup_printf ("Content-Type: message/delivery-status\n\n"
						"Content-Transfer-Encoding: %s\n\n%s", values[i], recipient);
			entity = parse_core_entity (text, 16, TRUE, NULL);
			g_free (text);
			groups = g_mime_message_delivery_status_get_status_groups (GMIME_MESSAGE_DELIVERY_STATUS (entity), &error);
			g_assert_no_error (error);
			check (groups && g_mime_header_list_collection_get_count (groups) == 3,
			       "nonencoding values require no decoder and the first duplicate inner encoding wins");
			g_object_unref (entity);
		}
	}
	encoded = encode_delivery_status ("Content-Transfer-Encoding: base64\n\nX-Plain: not encoded\n", GMIME_CONTENT_ENCODING_BASE64);
	text = g_strdup_printf ("Content-Type: message/delivery-status\n\nContent-Transfer-Encoding: base64\n\n%s", encoded);
	entity = parse_core_entity (text, 16, FALSE, NULL);
	g_free (text);
	g_free (encoded);
	groups = g_mime_message_delivery_status_get_status_groups (GMIME_MESSAGE_DELIVERY_STATUS (entity), &error);
	g_assert_no_error (error);
	check (groups && g_mime_header_list_collection_get_count (groups) == 3 &&
	       g_mime_header_list_get_header (g_mime_header_list_collection_get_group (groups, 2), "X-Plain"),
	       "transfer encodings in the decoded remainder do not recursively decode subsequent groups");
	g_object_unref (entity);

	text = g_strdup_printf ("Content-Type: multipart/report; boundary=b; report-type=delivery-status\n\n"
				"--b\nContent-Type: text/plain\n\nReport\n"
				"--b\nContent-Type: message/delivery-status\n\n%s\n"
				"--b\nContent-Type: text/plain\n\nSibling\n--b--\n", payload);
	entity = parse_core_entity (text, 16, TRUE, NULL);
	g_free (text);
	g_assert_true (GMIME_IS_MULTIPART_REPORT (entity));
	check (g_mime_multipart_get_count (GMIME_MULTIPART (entity)) == 3, "delivery status parts preserve multipart report siblings");
	groups = g_mime_message_delivery_status_get_status_groups (
		GMIME_MESSAGE_DELIVERY_STATUS (g_mime_multipart_get_part (GMIME_MULTIPART (entity), 1)), &error);
	g_assert_no_error (error);
	check (groups && g_mime_header_list_collection_get_count (groups) == 3, "delivery status groups stop at the report boundary");
	g_object_unref (entity);

	text = "X-Complete: value\n\n: malformed\n\nX-Ignored: value\n";
	for (onebyte = 0; onebyte < 2; onebyte++) {
		status = g_mime_message_delivery_status_new ();
		failure = g_object_new (test_stream_failure_get_type (), NULL);
		failure->parent_instance.source = g_mime_stream_mem_new_with_buffer (text, strlen (text));
		failure->fail_after = G_MAXSIZE;
		failure->onebyte = onebyte;
		g_mime_stream_construct (GMIME_STREAM (failure), 0, -1);
		stream = GMIME_STREAM (failure);
		content = g_mime_content_new_with_stream (stream, GMIME_CONTENT_ENCODING_UNSPECIFIED);
		g_object_unref (stream);
		g_mime_part_set_content (GMIME_PART (status), content);
		g_test_expect_message ("gmime", G_LOG_LEVEL_WARNING, "*Malformed delivery status content*");
		groups = g_mime_message_delivery_status_get_status_groups (status, &error);
		g_test_assert_expected_messages ();
		g_assert_no_error (error);
		check (groups && g_mime_header_list_collection_get_count (groups) == 1 &&
		       g_mime_part_get_content (GMIME_PART (status)) == content,
		       "malformed later groups warn and retain only complete earlier groups without rewriting content");
		g_object_unref (content);
		g_object_unref (status);
	}

	status = g_mime_message_delivery_status_new ();
	content = g_mime_content_new ();
	g_mime_part_set_content (GMIME_PART (status), content);
	check (!g_mime_message_delivery_status_get_status_groups (status, &error), "status content without a backing stream fails explicitly");
	g_assert_error (error, GMIME_ERROR, GMIME_ERROR_INVALID_OPERATION);
	g_clear_error (&error);
	stream = g_mime_stream_mem_new_with_buffer (payload, strlen (payload));
	g_mime_content_set_stream (content, stream);
	g_object_unref (stream);
	groups = g_mime_message_delivery_status_get_status_groups (status, &error);
	g_assert_no_error (error);
	check (groups && g_mime_header_list_collection_get_count (groups) == 3, "missing status content streams can be repaired and retried");
	g_object_unref (content);
	g_object_unref (status);

	{
		GMimeParserOptions *options = g_mime_parser_options_new ();
		GMimeParser *parser;
		gsize limit = g_mime_parser_options_get_max_header_length (NULL);
		char *value = g_strnfill (100, 'x');

		text = g_strdup_printf ("Content-Type: message/delivery-status\n\nX-Test: %s\n\nX-Other: value\n", value);
		stream = g_mime_stream_mem_new_with_buffer (text, strlen (text));
		parser = g_mime_parser_new_with_stream (stream);
		g_mime_parser_options_set_max_header_length (options, 64);
		entity = g_mime_parser_construct_part (parser, options, &error);
		g_assert_no_error (error);
		groups = g_mime_message_delivery_status_get_status_groups (GMIME_MESSAGE_DELIVERY_STATUS (entity), &error);
		g_assert_no_error (error);
		check (groups && g_mime_header_list_collection_get_count (groups) == 2,
		       "lazy status groups use default options rather than outer header limits");
		g_object_unref (entity);
		g_object_unref (parser);
		g_object_unref (stream);
		g_mime_parser_options_free (options);
		g_free (text);
		g_free (value);

		value = g_strnfill (limit, 'x');
		text = g_strdup_printf ("X-Complete: value\n\nX-Too-Long: %s\n\nX-Ignored: value\n", value);
		g_free (value);
		stream = g_mime_stream_mem_new_with_buffer (text, strlen (text));
		content = g_mime_content_new_with_stream (stream, GMIME_CONTENT_ENCODING_UNSPECIFIED);
		status = g_mime_message_delivery_status_new ();
		g_mime_part_set_content (GMIME_PART (status), content);
		g_test_expect_message ("gmime", G_LOG_LEVEL_WARNING, "*Malformed delivery status content*");
		groups = g_mime_message_delivery_status_get_status_groups (status, &error);
		g_test_assert_expected_messages ();
		g_assert_no_error (error);
		check (groups && g_mime_header_list_collection_get_count (groups) == 1,
		       "default header-limit violations warn and retain completed groups, not an oversized partial group");
		g_object_unref (status);
		g_object_unref (content);
		g_object_unref (stream);
		g_free (text);
	}

	encoded = encode_delivery_status (recipient, GMIME_CONTENT_ENCODING_BASE64);
	text = g_strdup_printf ("Reporting-MTA: dns; example.org\nContent-Transfer-Encoding: base64\n\n%s", encoded);
	g_free (encoded);
	for (k = 0; k < 2; k++) {
		const char *input = k ? text : payload;
		for (offset = 0; offset < strlen (input); offset++) {
			status = g_mime_message_delivery_status_new ();
			failure = g_object_new (test_stream_failure_get_type (), NULL);
			failure->parent_instance.source = g_mime_stream_mem_new_with_buffer (input, strlen (input));
			failure->fail_after = offset;
			failure->onebyte = TRUE;
			g_mime_stream_construct (GMIME_STREAM (failure), 0, -1);
			content = g_mime_content_new_with_stream (GMIME_STREAM (failure), GMIME_CONTENT_ENCODING_UNSPECIFIED);
			g_mime_part_set_content (GMIME_PART (status), content);
			g_object_unref (content);
			check (!g_mime_message_delivery_status_get_status_groups (status, &error),
			       "status loading propagates errors at every offset, including while draining encoded read-ahead");
			g_assert_error (error, GMIME_ERROR, EIO);
			g_clear_error (&error);
			failure->bytes_read = 0;
			failure->fail_after = G_MAXSIZE;
			failure->fail_reset_on = failure->reset_calls + 1;
			check (!g_mime_message_delivery_status_get_status_groups (status, &error), "status loading propagates rewind errors");
			g_assert_error (error, GMIME_ERROR, ESPIPE);
			g_clear_error (&error);
			groups = g_mime_message_delivery_status_get_status_groups (status, &error);
			g_assert_no_error (error);
			check (groups && g_mime_header_list_collection_get_count (groups) == 3, "failed status loads do not cache incomplete collections");
			g_object_unref (failure);
			g_object_unref (status);
		}
	}
	g_free (text);
}

static void
test_feedback_report (void)
{
	const char *payload = "Feedback-Type: abuse\nUser-Agent: client.example\nVersion: 1\n";
	const GMimeContentEncoding encodings[] = {
		GMIME_CONTENT_ENCODING_7BIT, GMIME_CONTENT_ENCODING_BASE64,
		GMIME_CONTENT_ENCODING_QUOTEDPRINTABLE, GMIME_CONTENT_ENCODING_UUENCODE
	};
	const char *blocks[] = { "", "\n", "X-Test: value", "X-Test: value\n",
		"X-Test: value\n\nignored: body\n", "X-Test: value\nnot a header\n" };
	GMimeMessageFeedbackReport *report = g_mime_message_feedback_report_new ();
	GMimeHeaderList *fields, *retained;
	GMimeHeader *header;
	GMimeContent *content, *original;
	GMimeStream *stream, *output;
	GMimeEntity *entity, *parsed;
	TestStreamFailure *failure;
	GError *error = NULL;
	GByteArray *bytes;
	char *text, *encoded, *raw, *serialized;
	guint i, offset;
	int onebyte, depth;
	gpointer weak;

	check (GMIME_IS_PART (report) && !GMIME_IS_MESSAGE_PART (report) &&
	       g_mime_content_type_is_type (g_mime_entity_get_content_type (GMIME_ENTITY (report)), "message", "feedback-report"),
	       "feedback reports are leaf parts with the correct default content type");
	check (!g_mime_part_get_content (GMIME_PART (report)), "feedback report content is created lazily");
	fields = g_mime_message_feedback_report_get_fields (report, &error);
	g_assert_no_error (error);
	g_assert_nonnull (fields);
	check (!g_mime_header_list_get_count (fields) &&
	       fields == g_mime_message_feedback_report_get_fields (report, &error),
	       "feedback reports cache an initially empty field list");
	g_assert_no_error (error);
	output = g_mime_stream_mem_new ();
	check (g_mime_content_decode_to_stream (g_mime_part_get_content (GMIME_PART (report)), output) == 0,
	       "lazy empty feedback content has no separator until fields change");
	g_object_unref (output);
	g_mime_header_list_append (fields, "Feedback-Type", "abuse", NULL);
	output = g_mime_stream_mem_new ();
	g_mime_content_decode_to_stream (g_mime_part_get_content (GMIME_PART (report)), output);
	bytes = g_mime_stream_mem_get_byte_array (GMIME_STREAM_MEM (output));
	text = g_strdup_printf ("Feedback-Type: abuse%s%s", g_mime_format_options_get_newline (NULL),
				g_mime_format_options_get_newline (NULL));
	check (bytes->len == strlen (text) && !memcmp (bytes->data, text, bytes->len),
	       "feedback field changes serialize the header/body separator");
	g_free (text);
	g_object_unref (output);
	original = g_object_ref (g_mime_part_get_content (GMIME_PART (report)));
	header = g_mime_header_list_get_header (fields, "Feedback-Type");
	g_mime_header_set_value (header, NULL, "fraud", NULL);
	check (g_mime_part_get_content (GMIME_PART (report)) != original,
	       "contained feedback header edits immediately regenerate content");
	g_object_unref (original);
	stream = g_mime_stream_mem_new_with_buffer ("X-Replacement: other\n", 21);
	content = g_mime_content_new_with_stream (stream, GMIME_CONTENT_ENCODING_UNSPECIFIED);
	g_object_unref (stream);
	g_mime_part_set_content (GMIME_PART (report), content);
	check (g_mime_message_feedback_report_get_fields (report, &error) == fields &&
	       !strcmp (g_mime_header_get_value (header), "fraud"),
	       "replacing feedback content does not invalidate cached fields");
	g_assert_no_error (error);
	g_mime_header_list_set (fields, "Feedback-Type", "abuse", NULL);
	check (g_mime_part_get_content (GMIME_PART (report)) != content, "cached feedback edits replace externally assigned content");
	g_object_unref (content);
	content = g_object_ref (g_mime_part_get_content (GMIME_PART (report)));
	header = g_mime_header_new ("X-Failure", "value", NULL);
	header->formatter = failing_header_formatter;
	g_test_expect_message ("gmime", G_LOG_LEVEL_WARNING, "*Failed to serialize feedback report fields*");
	g_mime_header_list_add (fields, header);
	g_test_assert_expected_messages ();
	check (g_mime_part_get_content (GMIME_PART (report)) == content, "failed feedback serialization preserves old content");
	g_mime_header_list_remove_header (fields, header);
	g_object_unref (header);
	g_object_unref (content);
	retained = g_object_ref (fields);
	weak = report;
	g_object_add_weak_pointer (G_OBJECT (report), &weak);
	g_object_unref (report);
	check (!weak, "retained feedback fields do not keep their owner alive");
	g_mime_header_list_clear (retained);
	g_mime_header_list_append (retained, "Feedback-Type", "abuse", NULL);
	check (g_mime_header_list_get_count (retained) == 1, "feedback fields remain usable after their owner is released");
	g_object_unref (retained);

	entity = g_mime_entity_new_type (NULL, "MESSAGE", "feedback-report");
	check (GMIME_IS_MESSAGE_FEEDBACK_REPORT (entity), "the entity factory selects the feedback report subclass");
	g_object_unref (entity);
	for (i = 0; i < G_N_ELEMENTS (encodings); i++) {
		encoded = encode_delivery_status (payload, encodings[i]);
		for (onebyte = 0; onebyte < 2; onebyte++) {
			for (depth = 0; depth < 2; depth++) {
				text = g_strdup_printf ("Content-Type: message/feedback-report\nContent-Transfer-Encoding: %s\n\n%s",
							g_mime_content_encoding_to_string (encodings[i]), encoded);
				entity = parse_core_entity (text, depth ? 16 : 0, onebyte, NULL);
				g_free (text);
				g_assert_true (GMIME_IS_MESSAGE_FEEDBACK_REPORT (entity));
				report = GMIME_MESSAGE_FEEDBACK_REPORT (entity);
				original = g_mime_part_get_content (GMIME_PART (report));
				header = g_mime_header_list_get_header (g_mime_entity_get_header_list (entity), "Content-Transfer-Encoding");
				raw = g_strdup (testsuite_header_raw_text (header));
				fields = g_mime_message_feedback_report_get_fields (report, &error);
				g_assert_no_error (error);
				g_assert_nonnull (fields);
				check (g_mime_header_list_get_count (fields) == 3 &&
				       !strcmp (g_mime_header_get_value (g_mime_header_list_get_header (fields, "Feedback-Type")), "abuse") &&
				       g_mime_part_get_content (GMIME_PART (report)) == original,
				       "feedback fields decode lazily without rewriting content, including at the MIME depth limit");
				output = g_mime_stream_mem_new ();
				g_mime_content_write_to_stream (original, output);
				bytes = g_mime_stream_mem_get_byte_array (GMIME_STREAM_MEM (output));
				check (bytes->len == strlen (encoded) && !memcmp (bytes->data, encoded, bytes->len),
				       "feedback field reads preserve the original encoded bytes");
				g_object_unref (output);
				g_mime_header_list_set (fields, "Feedback-Type", "fraud", NULL);
				check (!strcmp (raw, testsuite_header_raw_text (header)) &&
				       g_mime_content_get_encoding (g_mime_part_get_content (GMIME_PART (report))) == GMIME_CONTENT_ENCODING_UNSPECIFIED,
				       "feedback edits replace decoded content while preserving the desired transfer encoding");
				g_free (raw);
				serialized = g_mime_entity_to_string (entity, NULL);
				parsed = parse_core_entity (serialized, 16, onebyte, NULL);
				g_free (serialized);
				g_assert_true (GMIME_IS_MESSAGE_FEEDBACK_REPORT (parsed));
				fields = g_mime_message_feedback_report_get_fields (GMIME_MESSAGE_FEEDBACK_REPORT (parsed), &error);
				g_assert_no_error (error);
				check (fields && !strcmp (g_mime_header_get_value (g_mime_header_list_get_header (fields, "Feedback-Type")), "fraud"),
				       "feedback field mutations survive MIME round-trips");
				g_object_unref (parsed);
				g_object_unref (entity);
			}
		}
		g_free (encoded);
	}
	for (i = 0; i < G_N_ELEMENTS (blocks); i++) {
		text = g_strdup_printf ("Content-Type: message/feedback-report\n\n%s", blocks[i]);
		entity = parse_core_entity (text, 16, TRUE, NULL);
		g_free (text);
		fields = g_mime_message_feedback_report_get_fields (GMIME_MESSAGE_FEEDBACK_REPORT (entity), &error);
		g_assert_no_error (error);
		g_assert_nonnull (fields);
		check (g_mime_header_list_get_count (fields) == (i < 2 ? 0 : i == 5 ? 2 : 1),
		       "feedback reports retain invalid wire headers in the first block");
		g_mime_header_list_append (fields, "Feedback-Type", "abuse", NULL);
		serialized = g_mime_entity_to_string (entity, NULL);
		check (!strstr (serialized, "ignored: body"), "feedback edits discard trailing content after the field block");
		g_free (serialized);
		g_object_unref (entity);
	}
	text = g_strdup_printf ("Content-Type: multipart/report; boundary=b; report-type=feedback-report\n\n"
				"--b\nContent-Type: text/plain\n\nReport\n"
				"--b\nContent-Type: message/feedback-report\n\n%s\n"
				"--b\nContent-Type: text/plain\n\nSibling\n--b--\n", payload);
	entity = parse_core_entity (text, 16, TRUE, NULL);
	g_free (text);
	g_assert_true (GMIME_IS_MULTIPART_REPORT (entity));
	check (g_mime_multipart_get_count (GMIME_MULTIPART (entity)) == 3, "feedback reports preserve their multipart siblings");
	parsed = g_mime_multipart_get_part (GMIME_MULTIPART (entity), 1);
	g_assert_true (GMIME_IS_MESSAGE_FEEDBACK_REPORT (parsed));
	fields = g_mime_message_feedback_report_get_fields (GMIME_MESSAGE_FEEDBACK_REPORT (parsed), &error);
	g_assert_no_error (error);
	check (fields && g_mime_header_list_get_count (fields) == 3, "feedback field loading stops at the report boundary");
	g_object_unref (entity);

	report = g_mime_message_feedback_report_new ();
	content = g_mime_content_new ();
	g_mime_part_set_content (GMIME_PART (report), content);
	check (!g_mime_message_feedback_report_get_fields (report, &error), "feedback content without a stream fails explicitly");
	g_assert_error (error, GMIME_ERROR, GMIME_ERROR_INVALID_OPERATION);
	g_clear_error (&error);
	stream = g_mime_stream_mem_new_with_buffer (": malformed\n", 12);
	g_mime_content_set_stream (content, stream);
	g_object_unref (stream);
	check (!g_mime_message_feedback_report_get_fields (report, &error), "feedback fields retain native malformed-header rejection");
	g_assert_error (error, GMIME_ERROR, GMIME_ERROR_PARSE_ERROR);
	g_clear_error (&error);
	stream = g_mime_stream_mem_new_with_buffer (payload, strlen (payload));
	g_mime_content_set_stream (content, stream);
	g_object_unref (stream);
	fields = g_mime_message_feedback_report_get_fields (report, &error);
	g_assert_no_error (error);
	check (fields && g_mime_header_list_get_count (fields) == 3, "failed feedback loads can be repaired and retried");
	g_object_unref (content);
	g_object_unref (report);
	for (offset = 0; offset < strlen (payload); offset++) {
		report = g_mime_message_feedback_report_new ();
		failure = g_object_new (test_stream_failure_get_type (), NULL);
		failure->parent_instance.source = g_mime_stream_mem_new_with_buffer (payload, strlen (payload));
		failure->fail_after = offset;
		failure->onebyte = TRUE;
		g_mime_stream_construct (GMIME_STREAM (failure), 0, -1);
		content = g_mime_content_new_with_stream (GMIME_STREAM (failure), GMIME_CONTENT_ENCODING_UNSPECIFIED);
		g_mime_part_set_content (GMIME_PART (report), content);
		g_object_unref (content);
		check (!g_mime_message_feedback_report_get_fields (report, &error), "feedback input errors do not return partial fields");
		g_assert_error (error, GMIME_ERROR, EIO);
		g_clear_error (&error);
		failure->bytes_read = 0;
		failure->fail_after = G_MAXSIZE;
		failure->fail_reset_on = failure->reset_calls + 1;
		check (!g_mime_message_feedback_report_get_fields (report, &error), "feedback field loading propagates rewind errors");
		g_assert_error (error, GMIME_ERROR, ESPIPE);
		g_clear_error (&error);
		fields = g_mime_message_feedback_report_get_fields (report, &error);
		g_assert_no_error (error);
		check (fields && g_mime_header_list_get_count (fields) == 3, "feedback I/O failures do not poison the cache");
		g_object_unref (failure);
		g_object_unref (report);
	}
}

static void
count_collection_changes (GMimeHeaderListCollection *collection, gpointer args, guint *count)
{
	(*count)++;
}

static void
test_header_list_collection (void)
{
	GMimeHeaderListCollection *collection = g_mime_header_list_collection_new ();
	GMimeHeaderListCollection *shared = g_mime_header_list_collection_new ();
	GMimeHeaderList *first = g_mime_header_list_new (NULL);
	GMimeHeaderList *second = g_mime_header_list_new (NULL);
	GMimeHeaderList *third = g_mime_header_list_new (NULL);
	GMimeHeaderList *group;
	GMimeHeader *header;
	gpointer weak;
	guint changes = 0, shared_changes = 0;
	int index;

	g_mime_event_add (collection->changed, (GMimeEventCallback) count_collection_changes, &changes);
	g_mime_event_add (shared->changed, (GMimeEventCallback) count_collection_changes, &shared_changes);
	check (g_mime_header_list_collection_get_count (collection) == 0 &&
	       !g_mime_header_list_collection_contains (collection, first), "new header-group collections are empty");
	g_mime_header_list_collection_add (collection, first);
	g_mime_header_list_collection_add (collection, first);
	check (changes == 2 && g_mime_header_list_collection_get_count (collection) == 2 &&
	       g_mime_header_list_collection_get_group (collection, 0) == first &&
	       g_mime_header_list_collection_get_group (collection, 1) == first,
	       "header-group additions retain ordered duplicate references and notify once per add");
	check (g_mime_header_list_collection_contains (collection, first) &&
	       !g_mime_header_list_collection_contains (collection, second), "group membership uses identity rather than equal field values");
	changes = 0;
	g_mime_header_list_append (first, "Disposition", "displayed", NULL);
	check (changes == 2, "duplicate groups forward list changes once per occurrence");
	changes = 0;
	header = g_mime_header_list_get_header (first, "Disposition");
	g_mime_header_set_value (header, NULL, "deleted", NULL);
	check (changes == 2, "duplicate groups forward contained-header changes once per occurrence");
	g_mime_header_list_collection_add (shared, first);
	changes = shared_changes = 0;
	g_mime_header_list_append (first, "X-Shared", "value", NULL);
	check (changes == 2 && shared_changes == 1, "shared header groups notify each collection independently");
	changes = 0;
	check (g_mime_header_list_collection_remove (collection, first), "removing a present group succeeds");
	check (changes == 1 && g_mime_header_list_collection_get_count (collection) == 1 &&
	       g_mime_header_list_collection_get_group (collection, 0) == first, "removal deletes only the first occurrence");
	changes = shared_changes = 0;
	g_mime_header_set_value (header, NULL, "displayed", NULL);
	check (changes == 1 && shared_changes == 1, "removing one duplicate preserves remaining group subscriptions");
	changes = 0;
	check (!g_mime_header_list_collection_remove (collection, second) && changes == 0, "removing an absent group does not notify");
	g_mime_header_list_collection_set_group (collection, 0, second);
	check (changes == 0 && g_mime_header_list_collection_get_group (collection, 0) == second,
	       "indexed group replacement updates the collection without emitting a change");
	g_mime_header_list_collection_set_group (collection, 0, second);
	check (changes == 0, "assigning the same group is a no-op");
	shared_changes = 0;
	g_mime_header_set_value (header, NULL, "deleted", NULL);
	check (changes == 0 && shared_changes == 1, "replacement disconnects only the old collection's subscription");
	g_mime_header_list_append (second, "Action", "failed", NULL);
	check (changes == 1, "replacement groups forward later changes");
	g_mime_header_list_collection_add (collection, third);
	g_mime_header_list_collection_add (collection, second);
	check (g_mime_header_list_collection_get_group (collection, 1) == third &&
	       g_mime_header_list_collection_get_group (collection, 2) == second, "group ordering is preserved across replacement and addition");
	changes = 0;
	g_mime_header_list_collection_set_group (collection, 1, second);
	g_mime_header_list_clear (second);
	check (changes == 3, "replacing with an existing group creates another independent occurrence");
	changes = 0;
	g_mime_header_list_collection_set_group (collection, 1, third);
	g_mime_header_list_clear (second);
	check (changes == 2, "replacing one duplicate removes only its subscription");
	g_mime_header_list_collection_remove (collection, second);
	check (g_mime_header_list_collection_get_count (collection) == 2 &&
	       g_mime_header_list_collection_get_group (collection, 0) == third &&
	       g_mime_header_list_collection_get_group (collection, 1) == second, "removal preserves the remaining group order");

	for (index = -1; index <= 2; index += 3) {
		g_test_expect_message ("gmime", G_LOG_LEVEL_CRITICAL, "*index >= 0*");
		check (!g_mime_header_list_collection_get_group (collection, index), "invalid group getters fail explicitly");
		g_test_assert_expected_messages ();
		g_test_expect_message ("gmime", G_LOG_LEVEL_CRITICAL, "*index >= 0*");
		g_mime_header_list_collection_set_group (collection, index, first);
		g_test_assert_expected_messages ();
	}
	changes = 0;
	g_test_expect_message ("gmime", G_LOG_LEVEL_CRITICAL, "*GMIME_IS_HEADER_LIST (group)*");
	g_mime_header_list_collection_add (collection, NULL);
	g_test_assert_expected_messages ();
	g_test_expect_message ("gmime", G_LOG_LEVEL_CRITICAL, "*GMIME_IS_HEADER_LIST (group)*");
	g_mime_header_list_collection_set_group (collection, 0, NULL);
	g_test_assert_expected_messages ();
	g_test_expect_message ("gmime", G_LOG_LEVEL_CRITICAL, "*GMIME_IS_HEADER_LIST (group)*");
	check (!g_mime_header_list_collection_contains (collection, NULL), "NULL group lookup fails explicitly");
	g_test_assert_expected_messages ();
	g_test_expect_message ("gmime", G_LOG_LEVEL_CRITICAL, "*GMIME_IS_HEADER_LIST (group)*");
	check (!g_mime_header_list_collection_remove (collection, NULL), "NULL group removal fails explicitly");
	g_test_assert_expected_messages ();
	check (changes == 0 && g_mime_header_list_collection_get_count (collection) == 2 &&
	       g_mime_header_list_collection_get_group (collection, 0) == third, "invalid mutations leave the collection and notifications unchanged");
	g_mime_header_list_collection_clear (collection);
	check (changes == 1 && g_mime_header_list_collection_get_count (collection) == 0, "clearing removes all groups and notifies");
	g_mime_header_list_collection_clear (collection);
	check (changes == 2, "clearing an empty collection still notifies");
	changes = 0;
	g_mime_header_list_clear (second);
	g_mime_header_list_clear (third);
	check (changes == 0, "cleared groups no longer notify the collection");
	g_mime_header_list_collection_add (collection, second);
	g_mime_header_list_collection_add (collection, second);
	g_object_unref (collection);
	changes = shared_changes = 0;
	g_mime_header_list_clear (second);
	g_mime_header_list_clear (first);
	check (changes == 0 && shared_changes == 1, "finalization disconnects duplicates without disturbing another collection");
	g_object_unref (shared);
	shared_changes = 0;
	g_mime_header_list_clear (first);
	check (shared_changes == 0, "retained groups remain editable after all collections are released");
	g_object_unref (first);
	g_object_unref (second);
	g_object_unref (third);

	collection = g_mime_header_list_collection_new ();
	group = g_mime_header_list_new (NULL);
	weak = group;
	g_object_add_weak_pointer (G_OBJECT (group), &weak);
	g_mime_header_list_collection_add (collection, group);
	g_mime_header_list_collection_add (collection, group);
	g_object_unref (group);
	check (weak != NULL, "collections retain their own group references");
	g_mime_header_list_collection_remove (collection, g_mime_header_list_collection_get_group (collection, 0));
	check (weak != NULL, "removing a borrowed duplicate keeps its remaining occurrence alive");
	g_mime_header_list_collection_clear (collection);
	check (weak == NULL, "clearing releases every duplicate reference");
	group = g_mime_header_list_new (NULL);
	weak = group;
	g_object_add_weak_pointer (G_OBJECT (group), &weak);
	g_mime_header_list_collection_add (collection, group);
	g_object_unref (group);
	group = g_mime_header_list_new (NULL);
	g_mime_header_list_collection_set_group (collection, 0, group);
	check (weak == NULL, "indexed replacement releases the old group");
	weak = group;
	g_object_add_weak_pointer (G_OBJECT (group), &weak);
	g_object_unref (group);
	check (weak != NULL, "indexed replacement retains the new group");
	group = g_object_ref (g_mime_header_list_collection_get_group (collection, 0));
	g_object_unref (collection);
	check (weak == group, "borrowed groups can be retained independently");
	g_mime_header_list_append (group, "Action", "failed", NULL);
	g_object_unref (group);
	check (weak == NULL, "retained groups are released without a collection ownership cycle");
}

typedef struct {
	GMimeFilter parent_instance;
	guint complete_calls;
	guint reset_calls;
	char marker;
} TestCompletionFilter;

typedef GMimeFilterClass TestCompletionFilterClass;

G_DEFINE_TYPE (TestCompletionFilter, test_completion_filter, GMIME_TYPE_FILTER)

static void
completion_filter_filter (GMimeFilter *filter, char *inbuf, size_t inlen, size_t prespace,
			  char **outbuf, size_t *outlen, size_t *outprespace)
{
	*outbuf = inbuf;
	*outlen = inlen;
	*outprespace = prespace;
}

static void
completion_filter_complete (GMimeFilter *filter, char *inbuf, size_t inlen, size_t prespace,
			    char **outbuf, size_t *outlen, size_t *outprespace)
{
	GMimeFilterPrivate *priv;
	TestCompletionFilter *test = (TestCompletionFilter *) filter;

	test->complete_calls++;
	g_mime_filter_set_size (filter, inlen + 1, FALSE);
	priv = _g_mime_filter_get_private (filter);

	memcpy (priv->outbuf, inbuf, inlen);
	priv->outbuf[inlen] = test->marker;
	*outbuf = priv->outbuf;
	*outlen = inlen + 1;
	*outprespace = priv->outpre;
}

static void
completion_filter_reset (GMimeFilter *filter)
{
	((TestCompletionFilter *) filter)->reset_calls++;
}

static void
test_completion_filter_class_init (TestCompletionFilterClass *klass)
{
	klass->filter = completion_filter_filter;
	klass->complete = completion_filter_complete;
	klass->reset = completion_filter_reset;
}

static void
test_completion_filter_init (TestCompletionFilter *filter)
{
	filter->marker = '!';
}

static void
test_filtered_stream_collection (void)
{
	GMimeStream *memory = g_mime_stream_mem_new ();
	GMimeStream *stream = g_mime_stream_filter_new (memory);
	GMimeStreamFilter *filtered = GMIME_STREAM_FILTER (stream);
	TestCompletionFilter *first = g_object_new (test_completion_filter_get_type (), NULL);
	TestCompletionFilter *second = g_object_new (test_completion_filter_get_type (), NULL);
	TestCompletionFilter *third = g_object_new (test_completion_filter_get_type (), NULL);
	TestCompletionFilter *other = g_object_new (test_completion_filter_get_type (), NULL);
	GByteArray *bytes = g_mime_stream_mem_get_byte_array (GMIME_STREAM_MEM (memory));
	int ids[4], id;
	guint i;

	first->marker = other->marker = 'A';
	second->marker = 'B';
	third->marker = 'C';
	g_assert_true (g_mime_stream_filter_get_source (filtered) == memory);
	g_assert_false (g_mime_stream_filter_contains (filtered, GMIME_FILTER (first)));
	g_assert_false (g_mime_stream_filter_remove_filter (filtered, GMIME_FILTER (first)));
	ids[0] = g_mime_stream_filter_add (filtered, GMIME_FILTER (first));
	ids[1] = g_mime_stream_filter_add (filtered, GMIME_FILTER (second));
	ids[2] = g_mime_stream_filter_add (filtered, GMIME_FILTER (first));
	ids[3] = g_mime_stream_filter_add (filtered, GMIME_FILTER (third));
	for (i = 0; i < G_N_ELEMENTS (ids); i++) {
		g_assert_cmpint (ids[i], >=, 0);
		if (i)
			g_assert_cmpint (ids[i], >, ids[i - 1]);
	}
	g_assert_true (g_mime_stream_filter_contains (filtered, GMIME_FILTER (first)));
	g_assert_true (g_mime_stream_filter_contains (filtered, GMIME_FILTER (second)));
	g_assert_true (g_mime_stream_filter_contains (filtered, GMIME_FILTER (third)));
	g_assert_false (g_mime_stream_filter_contains (filtered, GMIME_FILTER (other)));
	g_assert_false (g_mime_stream_filter_remove_filter (filtered, GMIME_FILTER (other)));
	g_assert_cmpint (g_mime_stream_flush (stream), ==, 0);
	g_assert_cmpmem (bytes->data, bytes->len, "ABAC", 4);

	g_assert_true (g_mime_stream_filter_remove_filter (filtered, GMIME_FILTER (first)));
	g_assert_true (g_mime_stream_filter_contains (filtered, GMIME_FILTER (first)));
	g_assert_cmpuint (first->complete_calls, ==, 2);
	g_assert_cmpuint (first->reset_calls, ==, 0);
	g_mime_stream_filter_remove (filtered, ids[0]);
	g_mime_stream_filter_remove (filtered, -1);
	g_mime_stream_filter_remove (filtered, G_MININT);
	g_mime_stream_filter_remove (filtered, G_MAXINT);
	g_assert_cmpint (g_mime_stream_write (stream, "x", 1), ==, 1);
	g_assert_cmpint (g_mime_stream_flush (stream), ==, 0);
	g_assert_cmpmem (bytes->data, bytes->len, "ABACxBAC", 8);

	g_mime_stream_filter_remove (filtered, ids[2]);
	g_assert_false (g_mime_stream_filter_contains (filtered, GMIME_FILTER (first)));
	g_assert_false (g_mime_stream_filter_remove_filter (filtered, GMIME_FILTER (first)));
	g_assert_cmpint (g_mime_stream_write (stream, "x", 1), ==, 1);
	g_assert_cmpint (g_mime_stream_flush (stream), ==, 0);
	g_assert_cmpmem (bytes->data, bytes->len, "ABACxBACxBC", 11);

	g_assert_true (g_mime_stream_filter_remove_filter (filtered, GMIME_FILTER (third)));
	g_assert_true (g_mime_stream_filter_remove_filter (filtered, GMIME_FILTER (second)));
	g_mime_stream_filter_remove (filtered, ids[3]);
	g_assert_cmpint (g_mime_stream_write (stream, "x", 1), ==, 1);
	g_assert_cmpint (g_mime_stream_flush (stream), ==, 0);
	g_assert_cmpmem (bytes->data, bytes->len, "ABACxBACxBCx", 12);

	id = g_mime_stream_filter_add (filtered, GMIME_FILTER (first));
	g_assert_cmpint (id, >, ids[3]);
	g_mime_stream_filter_remove (filtered, ids[0]);
	g_mime_stream_filter_remove (filtered, ids[2]);
	g_assert_true (g_mime_stream_filter_contains (filtered, GMIME_FILTER (first)));
	g_assert_cmpint (g_mime_stream_write (stream, "x", 1), ==, 1);
	g_assert_cmpint (g_mime_stream_flush (stream), ==, 0);
	g_assert_cmpmem (bytes->data, bytes->len, "ABACxBACxBCxxA", 14);
	g_assert_cmpuint (first->complete_calls, ==, 4);
	g_assert_cmpuint (second->complete_calls, ==, 3);
	g_assert_cmpuint (third->complete_calls, ==, 3);
	g_assert_cmpuint (first->reset_calls, ==, 0);
	g_assert_cmpuint (second->reset_calls, ==, 0);
	g_assert_cmpuint (third->reset_calls, ==, 0);
	g_object_unref (first);
	g_object_unref (second);
	g_object_unref (third);
	g_object_unref (other);
	g_object_unref (stream);
	g_object_unref (memory);
	check (TRUE, "filter membership uses identity and first-instance removal preserves order, state and independent IDs");
}

static void
test_filtered_stream_collection_ownership (void)
{
	GMimeStream *memory = g_mime_stream_mem_new ();
	GMimeStream *stream = g_mime_stream_filter_new (memory);
	GMimeStreamFilter *filtered = GMIME_STREAM_FILTER (stream);
	GMimeFilter *filter = g_mime_filter_pass_through_new ();
	gpointer weak_filter = filter, weak_source = memory;
	int first, second;

	g_object_add_weak_pointer (G_OBJECT (filter), &weak_filter);
	g_object_add_weak_pointer (G_OBJECT (memory), &weak_source);
	first = g_mime_stream_filter_add (filtered, filter);
	second = g_mime_stream_filter_add (filtered, filter);
	g_object_unref (filter);
	g_assert_true (g_mime_stream_filter_remove_filter (filtered, weak_filter));
	g_assert_nonnull (weak_filter);
	g_assert_true (g_mime_stream_filter_contains (filtered, weak_filter));
	g_mime_stream_filter_remove (filtered, first);
	g_assert_nonnull (weak_filter);
	g_mime_stream_filter_remove (filtered, second);
	g_assert_null (weak_filter);
	g_object_unref (memory);
	g_assert_nonnull (weak_source);
	g_assert_true (g_mime_stream_filter_get_source (filtered) == weak_source);
	g_object_unref (stream);
	g_assert_null (weak_source);
	check (TRUE, "duplicate filters own separate references and the source getter returns a borrowed reference");

	memory = g_mime_stream_mem_new ();
	weak_source = memory;
	g_object_add_weak_pointer (G_OBJECT (memory), &weak_source);
	stream = g_mime_stream_filter_new (memory);
	g_object_unref (memory);
	memory = g_object_ref (g_mime_stream_filter_get_source (GMIME_STREAM_FILTER (stream)));
	g_object_unref (stream);
	g_assert_true (weak_source == memory);
	g_assert_cmpint (g_mime_stream_write (memory, "x", 1), ==, 1);
	g_object_unref (memory);
	g_assert_null (weak_source);
	check (TRUE, "callers can retain the borrowed source independently without closing it with the filter stream");
}

typedef struct {
	GMimeStreamFilter *stream;
	GMimeFilter *remaining;
	guint calls;
} FilterRemovalNotify;

static void
filter_removal_notify (gpointer user_data, GObject *where_object_was)
{
	FilterRemovalNotify *notify = user_data;

	notify->calls++;
	g_assert_true (g_mime_stream_filter_contains (notify->stream, notify->remaining));
	g_assert_true (g_mime_stream_filter_remove_filter (notify->stream, notify->remaining));
}

static void
test_filtered_stream_collection_finalization (void)
{
	guint by_id;

	for (by_id = 0; by_id < 2; by_id++) {
		GMimeStream *memory = g_mime_stream_mem_new ();
		GMimeStream *stream = g_mime_stream_filter_new (memory);
		GMimeStreamFilter *filtered = GMIME_STREAM_FILTER (stream);
		GMimeFilter *remaining = g_mime_filter_pass_through_new ();
		GMimeFilter *filter = g_mime_filter_pass_through_new ();
		FilterRemovalNotify notify = { filtered, remaining, 0 };
		int id;

		g_mime_stream_filter_add (filtered, remaining);
		id = g_mime_stream_filter_add (filtered, filter);
		g_object_weak_ref (G_OBJECT (filter), filter_removal_notify, &notify);
		g_object_unref (filter);
		/* Releasing the last reference may also remove the preceding entry. */
		if (by_id)
			g_mime_stream_filter_remove (filtered, id);
		else
			g_assert_true (g_mime_stream_filter_remove_filter (filtered, filter));
		g_assert_cmpuint (notify.calls, ==, 1);
		g_assert_false (g_mime_stream_filter_contains (filtered, remaining));
		g_object_unref (remaining);
		g_object_unref (stream);
		g_object_unref (memory);
	}
	check (TRUE, "both removal APIs unlink before releasing filters and tolerate finalization removing a preceding entry");
}

static void
test_filtered_stream_collection_arguments (void)
{
	GMimeStream *memory = g_mime_stream_mem_new ();
	GMimeStream *stream = g_mime_stream_filter_new (memory);
	GMimeStreamFilter *filtered = GMIME_STREAM_FILTER (stream);
	GMimeFilter *filter = g_mime_filter_pass_through_new ();

	g_test_expect_message ("gmime", G_LOG_LEVEL_CRITICAL, "*GMIME_IS_STREAM_FILTER*");
	g_assert_null (g_mime_stream_filter_get_source (NULL));
	g_test_assert_expected_messages ();
	g_test_expect_message ("gmime", G_LOG_LEVEL_CRITICAL, "*GMIME_IS_STREAM_FILTER*");
	g_assert_false (g_mime_stream_filter_contains (NULL, filter));
	g_test_assert_expected_messages ();
	g_test_expect_message ("gmime", G_LOG_LEVEL_CRITICAL, "*GMIME_IS_STREAM_FILTER*");
	g_assert_false (g_mime_stream_filter_remove_filter (NULL, filter));
	g_test_assert_expected_messages ();
	g_test_expect_message ("gmime", G_LOG_LEVEL_CRITICAL, "*GMIME_IS_STREAM_FILTER*");
	g_assert_null (g_mime_stream_filter_get_source ((GMimeStreamFilter *) memory));
	g_test_assert_expected_messages ();
	g_test_expect_message ("gmime", G_LOG_LEVEL_CRITICAL, "*GMIME_IS_STREAM_FILTER*");
	g_assert_false (g_mime_stream_filter_contains ((GMimeStreamFilter *) memory, filter));
	g_test_assert_expected_messages ();
	g_test_expect_message ("gmime", G_LOG_LEVEL_CRITICAL, "*GMIME_IS_STREAM_FILTER*");
	g_assert_false (g_mime_stream_filter_remove_filter ((GMimeStreamFilter *) memory, filter));
	g_test_assert_expected_messages ();
	g_test_expect_message ("gmime", G_LOG_LEVEL_CRITICAL, "*GMIME_IS_FILTER*");
	g_assert_false (g_mime_stream_filter_contains (filtered, NULL));
	g_test_assert_expected_messages ();
	g_test_expect_message ("gmime", G_LOG_LEVEL_CRITICAL, "*GMIME_IS_FILTER*");
	g_assert_false (g_mime_stream_filter_remove_filter (filtered, NULL));
	g_test_assert_expected_messages ();
	g_test_expect_message ("gmime", G_LOG_LEVEL_CRITICAL, "*GMIME_IS_FILTER*");
	g_assert_false (g_mime_stream_filter_contains (filtered, (GMimeFilter *) memory));
	g_test_assert_expected_messages ();
	g_test_expect_message ("gmime", G_LOG_LEVEL_CRITICAL, "*GMIME_IS_FILTER*");
	g_assert_false (g_mime_stream_filter_remove_filter (filtered, (GMimeFilter *) memory));
	g_test_assert_expected_messages ();
	g_object_unref (filter);
	g_object_unref (stream);
	g_object_unref (memory);
	check (TRUE, "filter collection operations reject invalid objects with native programming-error diagnostics");
}

static void
test_filtered_stream_completion (void)
{
	GMimeStream *stream, *memory;
	TestCompletionFilter *first, *second;
	TestStreamFailure *source;
	GByteArray *bytes;
	guint owner, partial, zero, retry;

	for (owner = 0; owner < 2; owner++) {
		for (partial = 0; partial < 2; partial++) {
			for (zero = 0; zero < 2; zero++) {
				for (retry = 0; retry < 3; retry++) {
					bytes = g_byte_array_new ();
					memory = g_mime_stream_mem_new_with_byte_array (bytes);
					g_mime_stream_mem_set_owner (GMIME_STREAM_MEM (memory), FALSE);
					source = g_object_new (test_stream_failure_get_type (), NULL);
					source->parent_instance.source = memory;
					source->onebyte = TRUE;
					source->fail_after = 3 + partial;
					source->zero_write = zero;
					g_mime_stream_construct (GMIME_STREAM (source), 0, -1);
					stream = g_mime_stream_filter_new (GMIME_STREAM (source));
					g_mime_stream_filter_set_owner (GMIME_STREAM_FILTER (stream), owner);
					first = g_object_new (test_completion_filter_get_type (), NULL);
					second = g_object_new (test_completion_filter_get_type (), NULL);
					first->marker = 'A';
					second->marker = 'B';
					g_mime_stream_filter_add (GMIME_STREAM_FILTER (stream), GMIME_FILTER (first));
					g_mime_stream_filter_add (GMIME_STREAM_FILTER (stream), GMIME_FILTER (second));
					g_assert_cmpint (g_mime_stream_write (stream, "abc", 3), ==, 3);
					errno = 0;
					g_assert_cmpint (g_mime_stream_flush (stream), ==, -1);
					g_assert_cmpint (errno, ==, zero ? EIO : ENOSPC);
					g_assert_cmpmem (bytes->data, bytes->len, "abcA", 3 + partial);
					g_assert_cmpuint (first->complete_calls, ==, 1);
					g_assert_cmpuint (second->complete_calls, ==, 1);
					errno = 0;
					g_assert_cmpint (g_mime_stream_close (stream), ==, -1);
					g_assert_cmpint (errno, ==, zero ? EIO : ENOSPC);
					g_assert_cmpuint (source->close_calls, ==, 0);
					g_assert_cmpuint (first->complete_calls, ==, 1);
					/* A failed retry must not accept the next input or
					 * overwrite completion bytes still in a filter. */
					g_assert_cmpint (g_mime_stream_write (stream, "z", 1), ==, -1);
					g_assert_cmpmem (bytes->data, bytes->len, "abcA", 3 + partial);
					source->fail_after = G_MAXSIZE;
					if (retry == 2) {
						g_assert_cmpint (g_mime_stream_write (stream, "z", 1), ==, 1);
						g_assert_cmpmem (bytes->data, bytes->len, "abcABz", 6);
					}
					if (retry != 1) {
						g_assert_cmpint (g_mime_stream_flush (stream), ==, 0);
						g_assert_cmpint (g_mime_stream_flush (stream), ==, 0);
					}
					g_assert_cmpint (g_mime_stream_close (stream), ==, 0);
					g_assert_cmpmem (bytes->data, bytes->len, retry == 2 ? "abcABzAB" : "abcAB", retry == 2 ? 8 : 5);
					g_assert_cmpuint (first->complete_calls, ==, retry == 2 ? 2 : 1);
					g_assert_cmpuint (second->complete_calls, ==, first->complete_calls);
					g_assert_cmpuint (source->close_calls, ==, owner);
					g_assert_cmpuint (source->flush_calls, ==, 0);
					g_object_unref (first);
					g_object_unref (second);
					g_object_unref (stream);
					g_object_unref (source);
					g_byte_array_unref (bytes);
				}
			}
		}
	}
	check (TRUE, "completion retries preserve short-write progress, filter order, errors and source ownership");
}

static void
test_filtered_stream_lifecycle (void)
{
	GMimeStream *stream;
	TestCompletionFilter *filter;
	TestStreamFailure *source;
	GByteArray *bytes;
	guint writes, pending;

	source = g_object_new (test_stream_failure_get_type (), NULL);
	source->parent_instance.source = g_mime_stream_mem_new ();
	source->fail_after = G_MAXSIZE;
	g_mime_stream_construct (GMIME_STREAM (source), 0, -1);
	stream = g_mime_stream_filter_new (GMIME_STREAM (source));
	filter = g_object_new (test_completion_filter_get_type (), NULL);
	g_mime_stream_filter_add (GMIME_STREAM_FILTER (stream), GMIME_FILTER (filter));
	g_assert_cmpint (g_mime_stream_flush (stream), ==, 0);
	g_assert_cmpuint (filter->complete_calls, ==, 1);
	g_assert_cmpint (g_mime_stream_flush (stream), ==, 0);
	g_assert_cmpint (g_mime_stream_write (stream, "", 0), ==, 0);
	g_assert_cmpint (g_mime_stream_flush (stream), ==, 0);
	g_assert_cmpuint (filter->complete_calls, ==, 1);
	g_assert_cmpint (g_mime_stream_write (stream, "\000\377", 2), ==, 2);
	g_assert_cmpint (g_mime_stream_close (stream), ==, 0);
	bytes = g_mime_stream_mem_get_byte_array (GMIME_STREAM_MEM (source->parent_instance.source));
	g_assert_cmpmem (bytes->data, bytes->len, "!\000\377!", 4);
	g_assert_cmpuint (filter->complete_calls, ==, 2);
	g_assert_cmpuint (source->close_calls, ==, 0);
	g_assert_cmpuint (source->flush_calls, ==, 0);
	writes = source->write_calls;
	g_object_unref (stream);
	g_assert_cmpuint (source->write_calls, ==, writes);

	stream = g_mime_stream_filter_new (GMIME_STREAM (source));
	g_mime_stream_filter_add (GMIME_STREAM_FILTER (stream), GMIME_FILTER (filter));
	g_mime_stream_filter_set_owner (GMIME_STREAM_FILTER (stream), TRUE);
	source->fail_close = TRUE;
	errno = 0;
	g_assert_cmpint (g_mime_stream_close (stream), ==, -1);
	g_assert_cmpint (errno, ==, EBADF);
	g_assert_cmpuint (filter->complete_calls, ==, 3);
	source->fail_close = FALSE;
	g_assert_cmpint (g_mime_stream_close (stream), ==, 0);
	g_assert_cmpuint (filter->complete_calls, ==, 3);
	g_assert_cmpuint (source->close_calls, ==, 2);
	g_object_unref (stream);
	g_object_unref (filter);
	g_object_unref (source);
	check (TRUE, "fresh and repeated flushes, binary reuse, zero-length writes and close errors preserve completion state");

	source = g_object_new (test_stream_failure_get_type (), NULL);
	source->parent_instance.source = g_mime_stream_mem_new ();
	source->fail_after = 1;
	g_mime_stream_construct (GMIME_STREAM (source), 0, -1);
	stream = g_mime_stream_filter_new (GMIME_STREAM (source));
	errno = 0;
	g_assert_cmpint (g_mime_stream_write (stream, "abc", 3), ==, -1);
	g_assert_cmpint (errno, ==, ENOSPC);
	bytes = g_mime_stream_mem_get_byte_array (GMIME_STREAM_MEM (source->parent_instance.source));
	g_assert_cmpmem (bytes->data, bytes->len, "a", 1);
	g_object_unref (stream);
	g_object_unref (source);
	check (TRUE, "ordinary failed writes report errors after accepting a prefix");

	source = g_object_new (test_stream_failure_get_type (), NULL);
	source->parent_instance.source = g_mime_stream_mem_new ();
	source->fail_after = 0;
	g_mime_stream_construct (GMIME_STREAM (source), 0, -1);
	stream = g_mime_stream_filter_new (GMIME_STREAM (source));
	filter = g_object_new (test_completion_filter_get_type (), NULL);
	g_mime_stream_filter_add (GMIME_STREAM_FILTER (stream), GMIME_FILTER (filter));
	g_assert_cmpint (g_mime_stream_flush (stream), ==, -1);
	source->fail_reset_on = 1;
	g_assert_cmpint (g_mime_stream_reset (stream), ==, -1);
	g_assert_cmpint (g_mime_stream_flush (stream), ==, -1);
	g_assert_cmpuint (filter->complete_calls, ==, 1);
	g_assert_cmpuint (filter->reset_calls, ==, 0);
	source->fail_after = G_MAXSIZE;
	g_assert_cmpint (g_mime_stream_reset (stream), ==, 0);
	g_assert_cmpuint (filter->reset_calls, ==, 1);
	g_assert_cmpint (g_mime_stream_flush (stream), ==, 0);
	bytes = g_mime_stream_mem_get_byte_array (GMIME_STREAM_MEM (source->parent_instance.source));
	g_assert_cmpmem (bytes->data, bytes->len, "!", 1);
	g_assert_cmpuint (filter->complete_calls, ==, 2);
	g_assert_cmpint (g_mime_stream_write (stream, "x", 1), ==, 1);
	g_mime_stream_filter_set_owner (GMIME_STREAM_FILTER (stream), TRUE);
	g_object_unref (stream);
	g_assert_cmpuint (filter->complete_calls, ==, 2);
	g_assert_cmpuint (source->close_calls, ==, 0);
	g_assert_cmpmem (bytes->data, bytes->len, "!x", 2);
	g_object_unref (filter);
	g_object_unref (source);
	check (TRUE, "reset failures preserve pending completion, successful reset discards it, and destruction does not complete writes");

	for (pending = 0; pending < 2; pending++) {
		source = g_object_new (test_stream_failure_get_type (), NULL);
		source->parent_instance.source = g_mime_stream_mem_new ();
		g_mime_stream_construct (GMIME_STREAM (source), 0, -1);
		stream = g_mime_stream_filter_new (GMIME_STREAM (source));
		g_mime_stream_filter_set_owner (GMIME_STREAM_FILTER (stream), TRUE);
		filter = g_object_new (test_completion_filter_get_type (), NULL);
		g_mime_stream_filter_add (GMIME_STREAM_FILTER (stream), GMIME_FILTER (filter));
		if (pending)
			g_assert_cmpint (g_mime_stream_flush (stream), ==, -1);
		writes = source->write_calls;
		g_object_unref (stream);
		g_assert_cmpuint (filter->complete_calls, ==, pending);
		g_assert_cmpuint (source->write_calls, ==, writes);
		g_assert_cmpuint (source->close_calls, ==, 0);
		g_object_unref (filter);
		g_object_unref (source);
	}
	check (TRUE, "destruction neither completes fresh filters nor retries failed completion writes");
}

static void
test_filtered_stream_reads (void)
{
	static const char expected[] = "ab\r!";
	TestCompletionFilter *tail;
	TestStreamFailure *source;
	GMimeStream *stream;
	GMimeFilter *dos;
	char buffer[sizeof (expected)];
	guint calls, mode, i;

	for (mode = 0; mode < 4; mode++) {
		source = g_object_new (test_stream_failure_get_type (), NULL);
		source->parent_instance.source = g_mime_stream_mem_new_with_buffer ("ab\r", 3);
		source->onebyte = mode != 0;
		source->fail_after = mode == 2 ? 1 : G_MAXSIZE;
		source->zero_read = mode == 3;
		g_mime_stream_construct (GMIME_STREAM (source), 0, -1);
		stream = g_mime_stream_filter_new (GMIME_STREAM (source));
		dos = g_mime_filter_dos2unix_new (FALSE);
		tail = g_object_new (test_completion_filter_get_type (), NULL);
		g_mime_stream_filter_add (GMIME_STREAM_FILTER (stream), dos);
		g_mime_stream_filter_add (GMIME_STREAM_FILTER (stream), GMIME_FILTER (tail));
		g_object_unref (dos);
		g_assert_cmpint (g_mime_stream_read (stream, buffer, 0), ==, 0);
		g_assert_cmpuint (source->read_calls, ==, 0);
		if (mode == 3) {
			g_assert_cmpint (g_mime_stream_read (stream, buffer, 1), ==, 0);
			g_assert_false (g_mime_stream_eos (stream));
			g_assert_cmpuint (tail->complete_calls, ==, 0);
		}
		for (i = 0; i < 4; i++) {
			if (mode == 2 && i == 1) {
				errno = 0;
				g_assert_cmpint (g_mime_stream_read (stream, buffer + i, 1), ==, -1);
				g_assert_cmpint (errno, ==, EIO);
				g_assert_cmpuint (tail->complete_calls, ==, 0);
				g_assert_false (g_mime_stream_eos (stream));
				source->fail_after = G_MAXSIZE;
			}
			g_assert_cmpint (g_mime_stream_read (stream, buffer + i, 1), ==, 1);
			g_assert_cmpint (g_mime_stream_eos (stream), ==, i == 3);
		}
		g_assert_cmpmem (buffer, 4, expected, 4);
		g_assert_cmpuint (tail->complete_calls, ==, 1);
		calls = source->read_calls;
		source->fail_after = source->bytes_read;
		g_assert_cmpint (g_mime_stream_read (stream, buffer, sizeof (buffer)), ==, 0);
		g_assert_cmpint (g_mime_stream_read (stream, buffer, sizeof (buffer)), ==, 0);
		g_assert_cmpuint (source->read_calls, ==, calls);
		g_assert_cmpint (g_mime_stream_flush (stream), ==, 0);
		g_assert_cmpuint (tail->complete_calls, ==, 1);

		source->fail_reset_on = 1;
		errno = 0;
		g_assert_cmpint (g_mime_stream_reset (stream), ==, -1);
		g_assert_cmpint (errno, ==, ESPIPE);
		g_assert_cmpuint (tail->reset_calls, ==, 0);
		g_assert_cmpint (g_mime_stream_read (stream, buffer, 1), ==, 0);
		g_assert_cmpuint (source->read_calls, ==, calls);
		source->fail_after = G_MAXSIZE;
		g_assert_cmpint (g_mime_stream_reset (stream), ==, 0);
		g_assert_false (g_mime_stream_eos (stream));
		g_assert_cmpuint (tail->reset_calls, ==, 1);
		for (i = 0; i < 4; i++)
			g_assert_cmpint (g_mime_stream_read (stream, buffer + i, 1), ==, 1);
		g_assert_cmpmem (buffer, 4, expected, 4);
		g_assert_cmpuint (tail->complete_calls, ==, 2);
		g_object_unref (stream);
		g_object_unref (tail);
		g_object_unref (source);
	}
	check (TRUE, "read completion drains buffered CR and trailers once, caches EOF, and preserves native error/reset semantics");
}

int
main (int argc, char **argv)
{
	int multipart, onebyte, persist;

	g_mime_init ();
	testsuite_init (argc, argv);

	testsuite_start ("filtered stream completion lifecycle");
	test_filtered_stream_completion ();
	test_filtered_stream_lifecycle ();
	test_filtered_stream_reads ();
	testsuite_end ();

	testsuite_start ("filtered stream collection operations");
	test_filtered_stream_collection ();
	test_filtered_stream_collection_ownership ();
	test_filtered_stream_collection_finalization ();
	test_filtered_stream_collection_arguments ();
	testsuite_end ();

	testsuite_start ("core type names and content");
	test_type_names ();
	test_content ();
	testsuite_end ();

	testsuite_start ("decoded content readers");
	test_content_open ();
	testsuite_end ();

	testsuite_start ("checked Content-MD5 computation");
	test_content_md5_errors ();
	testsuite_end ();

	testsuite_start ("checked part encoding selection");
	test_part_best_encoding_errors ();
	testsuite_end ();

	testsuite_start ("checked part transport preparation");
	test_part_prepare_errors ();
	testsuite_end ();

	testsuite_start ("checked recursive transport preparation");
	test_entity_prepare ();
	test_message_prepare_versions ();
	testsuite_end ();

	testsuite_start ("raw and decoded content copying");
	test_content_copy ();
	testsuite_end ();

	testsuite_start ("Message-ID parsing and reference recovery");
	test_message_id_parsing ();
	test_message_id_assignment ();
	test_message_id_whitespace ();
	test_message_reply_ids ();
	test_message_id_list ();
	test_message_references ();
	test_message_references_folding ();
	testsuite_end ();

	testsuite_start ("message importance and priorities");
	test_message_priorities ();
	testsuite_end ();

	testsuite_start ("message dates");
	test_message_dates ();
	testsuite_end ();

	testsuite_start ("resent message addresses");
	test_message_resent_addresses ();
	testsuite_end ();

	testsuite_start ("message recipient selection");
	test_message_recipients ();
	testsuite_end ();

	testsuite_start ("part content duration");
	test_part_content_duration ();
	testsuite_end ();

	testsuite_start ("explicit text charset I/O");
	test_text_part_charset_errors ();
	testsuite_end ();

	testsuite_start ("BOM charset detection");
	test_charset_bom_detection ();
	testsuite_end ();

	testsuite_start ("text-part formats");
	test_text_part_formats ();
	testsuite_end ();

	testsuite_start ("multipart text-body selection");
	test_multipart_text_selection ();
	testsuite_end ();

	testsuite_start ("multipart related roots");
	test_multipart_related ();
	testsuite_end ();

	testsuite_start ("entity attachment properties");
	test_entity_attachments ();
	testsuite_end ();

	testsuite_start ("entity content URI headers");
	test_entity_content_uris ();
	testsuite_end ();

	testsuite_start ("message body parts and attachments");
	test_message_body_parts ();
	testsuite_end ();

	testsuite_start ("message entity paths");
	test_message_entity_paths ();
	testsuite_end ();

	testsuite_start ("standalone header construction");
	test_header_construction ();
	testsuite_end ();

	testsuite_start ("standalone header parsing");
	test_header_parsing ();
	testsuite_end ();

	testsuite_start ("header identifiers");
	test_header_identifiers ();
	testsuite_end ();

	testsuite_start ("linear-whitespace header unfolding");
	test_header_unfolding ();
	testsuite_end ();

	testsuite_start ("checked header assignment");
	test_checked_header_assignment ();
	testsuite_end ();

	testsuite_start ("checked header-list values");
	test_checked_header_list_values ();
	testsuite_end ();

	testsuite_start ("named header-list values");
	test_named_header_list_values ();
	testsuite_end ();

	testsuite_start ("explicit header charset decoding");
	test_header_charset_value ();
	testsuite_end ();

	testsuite_start ("raw header byte access");
	test_header_raw_bytes ();
	testsuite_end ();

	testsuite_start ("mixed header charset policy");
	test_mixed_header_charsets ();
	testsuite_end ();

	testsuite_start ("quoted and comment phrase groups");
	test_phrase_groups ();
	testsuite_end ();

	testsuite_start ("RFC 2047 word merging");
	test_rfc2047_word_merging ();
	testsuite_end ();

	testsuite_start ("RFC 2047 token splitting");
	test_rfc2047_token_splitting ();
	testsuite_end ();

	testsuite_start ("RFC 2047 charset aliases");
	test_rfc2047_charset_aliases ();
	testsuite_end ();

	testsuite_start ("RFC 2047 token recovery");
	test_rfc2047_token_recovery ();
	testsuite_end ();

	testsuite_start ("RFC 2047 decoded charset selection");
	test_rfc2047_charset_selection ();
	testsuite_end ();

	testsuite_start ("RFC 2047 alias-aware decoder runs");
	test_rfc2047_alias_runs ();
	testsuite_end ();

	testsuite_start ("header cloning and display");
	test_header_cloning ();
	test_header_clone_formatting ();
	testsuite_end ();

	testsuite_start ("International text headers");
	test_international_text_headers ();
	test_parsed_international_headers ();
	testsuite_end ();

	testsuite_start ("signature header formatting");
	test_signature_headers ();
	testsuite_end ();

	testsuite_start ("mailing-list command header formatting");
	test_mailing_list_headers ();
	testsuite_end ();

	testsuite_start ("positional header-list operations");
	test_header_list_positions ();
	testsuite_end ();

	testsuite_start ("bulk header removal");
	test_header_list_remove_all ();
	testsuite_end ();

	testsuite_start ("header-list identifier operations");
	test_header_list_identifiers ();
	testsuite_end ();

	testsuite_start ("header identifier construction");
	test_header_id_construction ();
	testsuite_end ();

	testsuite_start ("header-list identifier values");
	test_header_list_id_values ();
	testsuite_end ();

	testsuite_start ("existing header instances");
	test_header_list_instances ();
	testsuite_end ();

	testsuite_start ("existing header replacement");
	test_header_list_replace ();
	testsuite_end ();

	testsuite_start ("indexed header assignment");
	test_header_list_set_header_at ();
	testsuite_end ();

	testsuite_start ("standalone header loading");
	test_header_list_loading ();
	test_header_list_loading_errors ();
	test_header_list_file_loading ();
	test_header_list_separators ();
	test_header_list_writing_errors ();
	test_entity_header_separators ();
	testsuite_end ();

	testsuite_start ("crypto flag validation");
	test_crypto_flags ();
	test_crypto_backend_handles ();
	testsuite_end ();

	testsuite_start ("AuthEnvelopedData MIME recognition and construction");
	test_authenveloped_data ();
	testsuite_end ();

	testsuite_start ("CMS CompressedData");
	test_compressed_data ();
	testsuite_end ();

	testsuite_start ("certificate and signature dates");
	test_crypto_dates ();
	testsuite_end ();

	testsuite_start ("address comma options");
	test_address_commas ();
	testsuite_end ();

	testsuite_start ("raw header length limits");
	test_header_lengths ();
	testsuite_end ();

	testsuite_start ("parser input errors");
	test_parser_read_errors ();
	test_parser_errors ();
	test_bounded_streams ();
	testsuite_end ();

	testsuite_start ("parser depth options");
	test_options ();
	test_address_depth ();
	test_mime_depth (FALSE, FALSE);
	test_mime_depth (TRUE, FALSE);
	test_mime_depth (FALSE, TRUE);
	test_mime_depth (TRUE, TRUE);
	for (multipart = 0; multipart < 2; multipart++) {
		for (onebyte = 0; onebyte < 2; onebyte++) {
			for (persist = 0; persist < 2; persist++)
				test_depth_siblings (multipart, onebyte, persist);
		}
	}
	testsuite_end ();

	testsuite_start ("mbox Content-Length options");
	test_content_length ();
	test_content_length_per_message ();
	test_content_length_offset_overflow ();
	testsuite_end ();

	testsuite_start ("parser charset encoding");
	test_charset_encoding ();
	testsuite_end ();

	testsuite_start ("formatting line length");
	test_format_line_length ();
	testsuite_end ();

	testsuite_start ("multipart related resources");
	test_multipart_related_resources ();
	testsuite_end ();

	testsuite_start ("text RFC822 headers");
	test_text_rfc822_headers ();
	testsuite_end ();

	testsuite_start ("multipart reports");
	test_multipart_report ();
	testsuite_end ();

	testsuite_start ("message disposition notifications");
	test_disposition_notification ();
	testsuite_end ();

	testsuite_start ("message delivery status");
	test_delivery_status ();
	testsuite_end ();

	testsuite_start ("message feedback reports");
	test_feedback_report ();
	testsuite_end ();

	testsuite_start ("header-list collections");
	test_header_list_collection ();
	testsuite_end ();

	g_mime_shutdown ();
	return testsuite_exit ();
}
