/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*  GMime
 *  Copyright (C) 2000-2022 Jeffrey Stedfast
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */


#include <stdio.h>
#include <gmime/gmime-object-private.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>

#include <gmime/gmime.h>

#include "testsuite.h"

extern int verbose;

#define d(x)
#define v(x) if (verbose > 3) x

static gboolean
streams_match (GMimeStream *expected, GMimeStream *actual)
{
	GMimeStreamPrivate *priv;
	char buf[4096], dbuf[4096], errstr[1024], *bufptr, *bufend, *dbufptr;
	gint64 len, totalsize, totalread = 0;
	size_t nread, size;
	gint64 offset = 0;
	ssize_t n;
	
	v(fprintf (stdout, "Checking if streams match... "));
	
	priv = _g_mime_stream_get_private (expected);

	if (priv->bound_end != -1) {
		totalsize = priv->bound_end - priv->position;
	} else if ((len = g_mime_stream_length (expected)) == -1) {
		sprintf (errstr, "Error: Unable to get length of expected stream\n");
		goto fail;
	} else if (len < (priv->position - priv->bound_start)) {
		sprintf (errstr, "Error: Overflow on expected stream?\n");
		goto fail;
	} else {
		totalsize = len - (priv->position - priv->bound_start);
	}
	
	while (totalread < totalsize) {
		if ((n = g_mime_stream_read (expected, buf, sizeof (buf))) <= 0)
			break;
		
		size = n;
		nread = 0;
		totalread += n;
		
		d(fprintf (stderr, "read %zu bytes from expected stream\n", size));
		
		do {
			if ((n = g_mime_stream_read (actual, dbuf + nread, size - nread)) <= 0) {
				fprintf (stderr, "actual stream's read(%p, dbuf + %zu, %zu) returned %zd, EOF\n", actual, nread, size - nread, n);
				break;
			}
			d(fprintf (stderr, "read %zd bytes from actual stream\n", n));
			nread += n;
		} while (nread < size);
		
		if (nread < size) {
			sprintf (errstr, "Error: actual stream appears to be truncated, short %zu+ bytes\n",
				 size - nread);
			goto fail;
		}
		
		bufend = buf + size;
		dbufptr = dbuf;
		bufptr = buf;
		
		while (bufptr < bufend) {
			if (*bufptr != *dbufptr)
				break;
			
			dbufptr++;
			bufptr++;
		}
		
		if (bufptr < bufend) {
			sprintf (errstr, "Error: content does not match at offset %" G_GINT64_FORMAT "\n",
				 offset + (bufptr - buf));
			/*fprintf (stderr, "-->'%.*s'<--\nvs\n-->'%.*s'<--\n",
			  bufend - bufptr, bufptr, bufend - bufptr, dbufptr);*/
			goto fail;
		} else {
			d(fprintf (stderr, "%zu bytes identical\n", size));
		}
		
		offset += size;
	}
	
	if (totalread < totalsize) {
		strcpy (errstr, "Error: expected more data from input stream\n");
		goto fail;
	}
	
	if ((n = g_mime_stream_read (actual, buf, sizeof (buf))) > 0) {
		strcpy (errstr, "Error: actual stream appears to contain extra content\n");
		goto fail;
	}
	
	v(fputs ("passed\n", stdout));
	
	return TRUE;
	
 fail:
	
	v(fputs ("failed\n", stdout));
	v(fputs (errstr, stderr));
	
	return FALSE;
}

static void
assert_partial_parameters (GMimeMessagePartial *partial, const char *id, int number, int total)
{
	g_assert_cmpstr (g_mime_message_partial_get_id (partial), ==, id);
	g_assert_cmpint (g_mime_message_partial_get_number (partial), ==, number);
	g_assert_cmpint (g_mime_message_partial_get_total (partial), ==, total);
}

static void
test_partial_constructor (void)
{
	GMimeMessagePartial *partial;
	char *id;

	testsuite_check ("message/partial constructor validates and owns its parameters");
	g_test_expect_message ("gmime", G_LOG_LEVEL_CRITICAL, "*id != NULL*");
	g_assert_null (g_mime_message_partial_new (NULL, 1, 1));
	g_test_assert_expected_messages ();
	g_test_expect_message ("gmime", G_LOG_LEVEL_CRITICAL, "*g_utf8_validate*");
	g_assert_null (g_mime_message_partial_new ("\xff", 1, 1));
	g_test_assert_expected_messages ();
	g_test_expect_message ("gmime", G_LOG_LEVEL_CRITICAL, "*number >= 1*");
	g_assert_null (g_mime_message_partial_new ("id", 0, 1));
	g_test_assert_expected_messages ();
	g_test_expect_message ("gmime", G_LOG_LEVEL_CRITICAL, "*number >= 1*");
	g_assert_null (g_mime_message_partial_new ("id", G_MININT, 1));
	g_test_assert_expected_messages ();
	g_test_expect_message ("gmime", G_LOG_LEVEL_CRITICAL, "*total >= number*");
	g_assert_null (g_mime_message_partial_new ("id", 2, 1));
	g_test_assert_expected_messages ();
	g_test_expect_message ("gmime", G_LOG_LEVEL_CRITICAL, "*total >= number*");
	g_assert_null (g_mime_message_partial_new ("id", 1, G_MININT));
	g_test_assert_expected_messages ();

	partial = g_mime_message_partial_new ("", 1, 1);
	g_assert_true (g_mime_content_type_is_type (g_mime_entity_get_content_type (GMIME_ENTITY (partial)),
						  "message", "partial"));
	assert_partial_parameters (partial, "", 1, 1);
	g_object_unref (partial);

	id = g_strdup ("caf\xc3\xa9@example.com");
	partial = g_mime_message_partial_new (id, G_MAXINT32, G_MAXINT32);
	g_free (id);
	assert_partial_parameters (partial, "caf\xc3\xa9@example.com", G_MAXINT32, G_MAXINT32);
	g_object_unref (partial);
	testsuite_check_passed ();
}

static void
test_partial_parameters (void)
{
	static const struct {
		const char *value;
		int expected;
	} cases[] = {
		{ "", -1 }, { " \t\r\n\v\f", -1 }, { "0", 0 }, { "000", 0 },
		{ "0012", 12 }, { "2147483647", G_MAXINT32 }, { "2147483648", -1 },
		{ "4294967297", -1 }, { "999999999999999999999999999999", -1 },
		{ "+1", -1 }, { "-1", -1 }, { "-0", -1 }, { "1a", -1 },
		{ "1 2", -1 }, { "1.0", -1 }, { "0x10", -1 }, { "1e2", -1 },
		{ " \t\r\n\v\f12\f\v\n\r\t ", 12 },
		{ "\302\24012", -1 }, { "12\302\240", -1 },
		{ "\342\200\20312", -1 }, { "\331\241\331\242", -1 }
	};
	GMimeMessagePartial *partial;
	GMimeContentType *content_type, *replacement;
	GMimeParamList *parameters;
	GMimeParam *param;
	guint i;

	testsuite_check ("message/partial properties follow live Content-Type parameters");
	partial = g_object_new (GMIME_TYPE_MESSAGE_PARTIAL, NULL);
	assert_partial_parameters (partial, NULL, -1, -1);
	g_object_unref (partial);
	partial = g_mime_message_partial_new ("original", 1, 2);
	content_type = g_mime_entity_get_content_type (GMIME_ENTITY (partial));
	g_object_ref (content_type);
	parameters = g_mime_content_type_get_parameters (content_type);
	param = g_mime_param_list_get_parameter (parameters, "id");
	g_mime_param_set_value (param, "edited");
	assert_partial_parameters (partial, "edited", 1, 2);

	for (i = 0; i < G_N_ELEMENTS (cases); i++) {
		g_mime_entity_set_content_type_parameter (GMIME_ENTITY (partial), "number", cases[i].value);
		param = g_mime_param_list_get_parameter (parameters, "total");
		g_mime_param_set_value (param, cases[i].value);
		assert_partial_parameters (partial, "edited", cases[i].expected, cases[i].expected);
		g_assert_cmpstr (g_mime_param_get_value (param), ==, cases[i].value);
		g_assert_cmpstr (g_mime_content_type_get_parameter (content_type, "number"), ==, cases[i].value);
	}

	g_assert_true (g_mime_param_list_remove (parameters, "number"));
	g_assert_true (g_mime_param_list_remove (parameters, "total"));
	g_assert_true (g_mime_param_list_remove (parameters, "id"));
	assert_partial_parameters (partial, NULL, -1, -1);
	g_mime_param_list_set_parameter (parameters, "id", "");
	g_mime_param_list_set_parameter (parameters, "number", "0");
	g_mime_param_list_set_parameter (parameters, "total", "0");
	assert_partial_parameters (partial, "", 0, 0);

	replacement = g_mime_content_type_new ("message", "partial");
	g_mime_content_type_set_parameter (replacement, "id", "replacement");
	g_mime_content_type_set_parameter (replacement, "number", "3");
	g_mime_entity_set_content_type (GMIME_ENTITY (partial), replacement);
	g_object_unref (replacement);
	g_mime_content_type_set_parameter (content_type, "id", "detached");
	assert_partial_parameters (partial, "replacement", 3, -1);
	g_object_unref (content_type);

	g_mime_entity_set_header (GMIME_ENTITY (partial), "Content-Type",
				"message/partial; id=header; number=7; total=9", NULL);
	assert_partial_parameters (partial, "header", 7, 9);
	g_mime_entity_set_header (GMIME_ENTITY (partial), "Content-Type", "message/partial", NULL);
	assert_partial_parameters (partial, NULL, -1, -1);
	g_object_unref (partial);
	testsuite_check_passed ();
}

static void
test_parsed_partial_parameters (void)
{
	static const char wire[] =
		"Content-Type: message/partial;\r\n"
		"\tid=\"parsed id\"; number=\" 0002 \"; total=\"2147483648\"\r\n"
		"\r\nfragment";
	GMimeStream *stream = g_mime_stream_mem_new_with_buffer (wire, sizeof (wire) - 1);
	GMimeParser *parser = g_mime_parser_new_with_stream (stream);
	GMimeEntity *entity;
	GMimeHeader *header;
	GBytes *before, *after;
	GError *error = NULL;

	testsuite_check ("parsed message/partial properties preserve raw headers");
	entity = g_mime_parser_construct_part (parser, NULL, &error);
	g_assert_no_error (error);
	g_assert_true (GMIME_IS_MESSAGE_PARTIAL (entity));
	header = g_mime_header_list_get_header (g_mime_entity_get_header_list (entity), "Content-Type");
	before = g_mime_header_get_raw_value (header);
	assert_partial_parameters (GMIME_MESSAGE_PARTIAL (entity), "parsed id", 2, -1);
	after = g_mime_header_get_raw_value (header);
	g_assert_true (g_bytes_equal (before, after));
	g_bytes_unref (before);
	g_bytes_unref (after);
	g_object_unref (entity);
	g_object_unref (parser);
	g_object_unref (stream);
	testsuite_check_passed ();
}

static void
test_partial_reconstruction_parameters (void)
{
	static const char *invalid[] = { NULL, "", "not-a-number", "2147483648", "-1", "+1", "1a" };
	GMimeMessagePartial *first = g_mime_message_partial_new ("id", G_MAXINT32, G_MAXINT32);
	GMimeMessagePartial *second = g_mime_message_partial_new ("id", 1, 2);
	GMimeMessagePartial *partials[] = { first, second };
	GMimeMessage *envelope = g_mime_message_new (FALSE);
	GMimeParamList *parameters;
	guint i;

	testsuite_check ("reconstruction rejects invalid numbers before sorting partials");
	parameters = g_mime_content_type_get_parameters (g_mime_entity_get_content_type (GMIME_ENTITY (second)));
	for (i = 0; i < G_N_ELEMENTS (invalid); i++) {
		if (invalid[i])
			g_mime_param_list_set_parameter (parameters, "number", invalid[i]);
		else
			g_mime_param_list_remove (parameters, "number");
		g_assert_null (g_mime_message_partial_join (NULL, envelope, partials, G_N_ELEMENTS (partials), NULL));
		g_assert_true (partials[0] == first);
		g_assert_true (partials[1] == second);
	}
	g_mime_param_list_set_parameter (parameters, "number", "1");
	g_mime_param_list_set_parameter (parameters, "id", "other");
	g_assert_null (g_mime_message_partial_join (NULL, envelope, partials, G_N_ELEMENTS (partials), NULL));
	g_assert_true (partials[0] == first);
	g_assert_true (partials[1] == second);
	g_object_unref (first);
	g_object_unref (second);
	g_object_unref (envelope);
	testsuite_check_passed ();
}

static void
test_split_without_message_id (void)
{
	GMimeMessagePrivate *messages_priv;
	GMimeMessage *message = g_mime_message_new (FALSE), **messages, *reconstructed;
	GMimeMessagePartial **partials;
	char *id;
	size_t count, i;

	testsuite_check ("splitting a message without Message-ID generates a shared partial id");
	g_mime_entity_set_header (GMIME_ENTITY (message), "Subject", "Split without an id", NULL);
	g_assert_null (g_mime_message_get_message_id (message));
	messages = g_mime_message_partial_split (message, 8, &count, NULL);
	g_assert_nonnull (messages);
	g_assert_cmpuint (count, >, 1);
	partials = g_new (GMimeMessagePartial *, count);
	messages_priv = _g_mime_message_get_private (messages[0]);

	id = g_strdup (g_mime_message_partial_get_id (GMIME_MESSAGE_PARTIAL (messages_priv->mime_part)));
	g_assert_nonnull (id);
	g_assert_cmpstr (id, !=, "");
	for (i = 0; i < count; i++) {
		GMimeMessagePrivate *message_priv = _g_mime_message_get_private (messages[i]);

		partials[i] = GMIME_MESSAGE_PARTIAL (message_priv->mime_part);
		assert_partial_parameters (partials[i], id, i + 1, count);
	}
	g_assert_null (g_mime_message_get_message_id (message));
	reconstructed = g_mime_message_partial_join (NULL, messages[0], partials, count, NULL);
	g_assert_nonnull (reconstructed);
	g_object_unref (reconstructed);
	for (i = 0; i < count; i++)
		g_object_unref (messages[i]);
	g_free (partials);
	g_free (messages);
	g_free (id);
	g_object_unref (message);
	testsuite_check_passed ();
}

static void
test_split_arguments (void)
{
	GMimeMessage *message = g_mime_message_new (FALSE);
	size_t count = 42;

	testsuite_check ("message splitting validates the size and result count");
	g_test_expect_message ("gmime", G_LOG_LEVEL_CRITICAL, "*nparts != NULL*");
	g_assert_null (g_mime_message_partial_split (message, 1, NULL, NULL));
	g_test_assert_expected_messages ();
	g_test_expect_message ("gmime", G_LOG_LEVEL_CRITICAL, "*GMIME_IS_MESSAGE*");
	g_assert_null (g_mime_message_partial_split (NULL, 1, &count, NULL));
	g_test_assert_expected_messages ();
	g_assert_cmpuint (count, ==, 0);
	count = 42;
	g_test_expect_message ("gmime", G_LOG_LEVEL_CRITICAL, "*max_size > 0*");
	g_assert_null (g_mime_message_partial_split (message, 0, &count, NULL));
	g_test_assert_expected_messages ();
	g_assert_cmpuint (count, ==, 0);
	g_object_unref (message);
	testsuite_check_passed ();
}

static GMimeMessage *
parse_split_message (const char *wire, size_t length, GMimeParserOptions *options)
{
	GMimeStream *stream = g_mime_stream_mem_new_with_buffer (wire, length);
	GMimeParser *parser = g_mime_parser_new_with_stream (stream);
	GMimeMessage *message;
	GError *error = NULL;

	message = g_mime_parser_construct_message (parser, options, &error);
	g_assert_no_error (error);
	g_assert_nonnull (message);
	g_object_unref (parser);
	g_object_unref (stream);

	return message;
}

static GBytes *
split_message_bytes (GMimeMessage *message)
{
	GMimeStream *stream = g_mime_stream_mem_new ();
	GByteArray *buffer;
	GBytes *bytes;

	g_assert_cmpint (g_mime_entity_write_to_stream (GMIME_ENTITY (message), NULL, stream), >=, 0);
	buffer = g_mime_stream_mem_get_byte_array (GMIME_STREAM_MEM (stream));
	bytes = g_bytes_new (buffer->data, buffer->len);
	g_object_unref (stream);

	return bytes;
}

static void
set_reconstruction_content (GMimeMessagePartial *partial, const char *data, size_t length,
			    GMimeContentEncoding encoding)
{
	GMimeContent *content;
	GMimeStream *stream;
	char *encoded = NULL;

	if (encoding == GMIME_CONTENT_ENCODING_BASE64) {
		encoded = g_base64_encode ((const guchar *) data, length);
		length = strlen (encoded);
	} else if (encoding == GMIME_CONTENT_ENCODING_QUOTEDPRINTABLE) {
		GString *quoted = g_string_new (NULL);
		size_t i;

		for (i = 0; i < length; i++) {
			if (i && i % 23 == 0)
				g_string_append (quoted, "=\r\n");
			g_string_append_printf (quoted, "=%02X", (unsigned char) data[i]);
		}
		length = quoted->len;
		encoded = g_string_free (quoted, FALSE);
	}

	stream = g_mime_stream_mem_new_with_buffer (encoded ? encoded : data, length);
	if (length > 1)
		g_assert_cmpint (g_mime_stream_seek (stream, 1, GMIME_STREAM_SEEK_SET), ==, 1);
	content = g_mime_content_new_with_stream (stream, encoding);
	g_mime_part_set_content (GMIME_PART (partial), content);
	g_object_unref (content);
	g_object_unref (stream);
	g_free (encoded);
}

static void
test_reconstruction_content (void)
{
	static const char headers[] =
		"Subject: decoded fragments\r\n"
		"Content-Type: application/octet-stream\r\n\r\n";
	static const char body[] = "before\0after\377\r\n";
	static const GMimeContentEncoding encodings[] = {
		GMIME_CONTENT_ENCODING_UNSPECIFIED, GMIME_CONTENT_ENCODING_7BIT,
		GMIME_CONTENT_ENCODING_8BIT, GMIME_CONTENT_ENCODING_BINARY,
		GMIME_CONTENT_ENCODING_BASE64, GMIME_CONTENT_ENCODING_QUOTEDPRINTABLE
	};
	GMimeMessagePartial *parts[3], *input[3];
	GMimeMessage *message;
	GMimeMessage *envelope = g_mime_message_new (FALSE);
	GMimeStream *output;
	GByteArray *buffer;
	GString *wire = g_string_new (headers);
	size_t offset, length;
	int missing, pass;
	guint c, i;

	testsuite_check ("reconstruction decodes content, skips absent content and preserves input order");
	g_string_append_len (wire, body, sizeof (body) - 1);
	for (c = 0; c < G_N_ELEMENTS (encodings); c++) {
		for (missing = -1; missing < 3; missing++) {
			offset = 0;
			for (i = 0; i < G_N_ELEMENTS (parts); i++) {
				parts[i] = g_mime_message_partial_new ("reconstruction", i + 1, 3);
				if (i != 2)
					g_mime_param_list_remove (g_mime_content_type_get_parameters (
						g_mime_entity_get_content_type (GMIME_ENTITY (parts[i]))), "total");
				if ((int) i == missing) {
					set_reconstruction_content (parts[i], "discarded", 9, encodings[c]);
					g_mime_part_set_content (GMIME_PART (parts[i]), NULL);
					continue;
				}
				length = (wire->len - offset) / (3 - i - (missing >= (int) i));
				set_reconstruction_content (parts[i], wire->str + offset, length,
							    encodings[(c + i) % G_N_ELEMENTS (encodings)]);
				offset += length;
			}
			g_assert_cmpuint (offset, ==, wire->len);
			input[0] = parts[2];
			input[1] = parts[0];
			input[2] = parts[1];
			for (pass = 0; pass < 2; pass++) {
				GMimeMessagePrivate *priv;

				message = g_mime_message_partial_join (NULL, envelope, input, G_N_ELEMENTS (input), NULL);
				g_assert_nonnull (message);
				g_assert_true (input[0] == parts[2]);
				g_assert_true (input[1] == parts[0]);
				g_assert_true (input[2] == parts[1]);
				g_assert_cmpstr (g_mime_message_get_subject (message), ==, "decoded fragments");
				output = g_mime_stream_mem_new ();
				priv = _g_mime_message_get_private (message);

				g_assert_cmpint (g_mime_content_decode_to_stream (
					g_mime_part_get_content (GMIME_PART (priv->mime_part)), output), ==, sizeof (body) - 1);
				buffer = g_mime_stream_mem_get_byte_array (GMIME_STREAM_MEM (output));
				g_assert_cmpmem (buffer->data, buffer->len, body, sizeof (body) - 1);
				g_object_unref (output);
				g_object_unref (message);
			}
			for (i = 0; i < G_N_ELEMENTS (parts); i++)
				g_object_unref (parts[i]);
		}
	}
	g_string_free (wire, TRUE);
	g_object_unref (envelope);
	testsuite_check_passed ();
}

static void
test_reconstruction_failures (void)
{
	GMimeMessagePrivate *priv;
	static const char wire[] = "Subject: rewind failure\r\n\r\nbody";
	GMimeMessagePartial *first = g_mime_message_partial_new ("id", 1, 2);
	GMimeMessagePartial *last = g_mime_message_partial_new ("id", 2, 2);
	GMimeMessagePartial *input[] = { last, first };
	GMimeParamList *parameters = g_mime_content_type_get_parameters (
		g_mime_entity_get_content_type (GMIME_ENTITY (last)));
	GMimeContent *content;
	GMimeStream *stream;
	GByteArray *buffer;
	GMimeMessage *message;
	GMimeMessage *envelope = g_mime_message_new (FALSE);
	GError *error = NULL;

	testsuite_check ("reconstruction failures preserve input order and release decoded readers");
	g_assert_null (g_mime_message_partial_join (NULL, envelope, NULL, 0, NULL));
	g_test_expect_message ("gmime", G_LOG_LEVEL_CRITICAL, "*partials != NULL || num == 0*");
	g_assert_null (g_mime_message_partial_join (NULL, envelope, NULL, 1, NULL));
	g_test_assert_expected_messages ();
	input[1] = NULL;
	g_test_expect_message ("gmime", G_LOG_LEVEL_CRITICAL, "*GMIME_IS_MESSAGE_PARTIAL*");
	g_assert_null (g_mime_message_partial_join (NULL, envelope, input, G_N_ELEMENTS (input), NULL));
	g_test_assert_expected_messages ();
	g_assert_true (input[0] == last);
	g_assert_null (input[1]);
	input[1] = first;
	g_mime_param_list_remove (parameters, "total");
	g_assert_null (g_mime_message_partial_join (NULL, envelope, input, G_N_ELEMENTS (input), NULL));
	g_assert_true (input[0] == last && input[1] == first);
	g_mime_param_list_set_parameter (parameters, "total", "3");
	g_assert_null (g_mime_message_partial_join (NULL, envelope, input, G_N_ELEMENTS (input), NULL));
	g_assert_true (input[0] == last && input[1] == first);
	g_mime_param_list_set_parameter (parameters, "total", "2");
	g_mime_param_list_set_parameter (parameters, "number", "1");
	g_assert_null (g_mime_message_partial_join (NULL, envelope, input, G_N_ELEMENTS (input), NULL));
	g_assert_true (input[0] == last && input[1] == first);
	g_mime_param_list_set_parameter (parameters, "number", "2");
	g_assert_null (g_mime_message_partial_join (NULL, envelope, input, G_N_ELEMENTS (input), NULL));
	g_assert_true (input[0] == last && input[1] == first);

	set_reconstruction_content (first, wire, sizeof (wire) - 1, GMIME_CONTENT_ENCODING_BASE64);
	stream = g_mime_stream_pipe_new (-1);
	content = g_mime_content_new_with_stream (stream, GMIME_CONTENT_ENCODING_UNSPECIFIED);
	g_object_unref (stream);
	g_mime_part_set_content (GMIME_PART (last), content);
	g_object_unref (content);
	errno = 0;
	g_assert_null (g_mime_message_partial_join (NULL, envelope, input, G_N_ELEMENTS (input), &error));
	g_assert_error (error, GMIME_ERROR, EBADF);
	g_clear_error (&error);
	g_assert_true (input[0] == last && input[1] == first);
	content = g_mime_content_new ();
	g_mime_part_set_content (GMIME_PART (last), content);
	g_object_unref (content);
	g_assert_null (g_mime_message_partial_join (NULL, envelope, input, G_N_ELEMENTS (input), &error));
	g_assert_error (error, GMIME_ERROR, GMIME_ERROR_INVALID_OPERATION);
	g_clear_error (&error);
	g_mime_part_set_content (GMIME_PART (last), NULL);
	message = g_mime_message_partial_join (NULL, envelope, input, G_N_ELEMENTS (input), &error);
	g_assert_no_error (error);
	g_assert_nonnull (message);
	g_assert_cmpstr (g_mime_message_get_subject (message), ==, "rewind failure");
	g_assert_true (input[0] == last && input[1] == first);
	g_object_unref (first);
	g_object_unref (last);
	g_assert_cmpstr (g_mime_message_get_subject (message), ==, "rewind failure");
	stream = g_mime_stream_mem_new ();
	priv = _g_mime_message_get_private (message);

	g_assert_cmpint (g_mime_content_decode_to_stream (
		g_mime_part_get_content (GMIME_PART (priv->mime_part)), stream), ==, 4);
	buffer = g_mime_stream_mem_get_byte_array (GMIME_STREAM_MEM (stream));
	g_assert_cmpmem (buffer->data, buffer->len, "body", 4);
	g_object_unref (stream);
	g_object_unref (message);
	g_object_unref (envelope);
	testsuite_check_passed ();
}

static void
test_split_failures (void)
{
	GMimeStream *stream = g_mime_stream_pipe_new (-1);
	GMimeContent *content = g_mime_content_new_with_stream (stream, GMIME_CONTENT_ENCODING_UNSPECIFIED);
	GMimeMessage *message = g_mime_message_new (FALSE);
	GMimePart *part = g_mime_part_new ();
	GError *error = NULL;
	size_t count = 42;

	testsuite_check ("Split reports serialization failures and clears the output count");
	g_mime_part_set_content (part, content);
	g_mime_message_set_mime_part (message, GMIME_ENTITY (part));
	g_assert_null (g_mime_message_partial_split (message, 1, &count, &error));
	g_assert_error (error, GMIME_ERROR, EBADF);
	g_assert_cmpuint (count, ==, 0);
	g_clear_error (&error);
	g_object_unref (message);
	g_object_unref (part);
	g_object_unref (content);
	g_object_unref (stream);
	testsuite_check_passed ();
}

static void
test_split_message_clones (void)
{
	static const char wire[] =
		"Subject: cloning\r\n"
		"mEsSaGe-Id: (original) <source@example.com>\r\n"
		"X-CaSe: before\0after\r\n"
		"Message-Id: <source@example.com>\r\n"
		"Content-Type: text/plain\r\n\r\nbody\n";
	GMimeParserOptions *options = g_mime_parser_options_new ();
	GMimeMessage *message, **messages;
	GMimeHeaderList *original, *cloned;
	GMimeHeader *source, *header;
	GBytes *raw, *name, *actual;
	GHashTable *ids = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, NULL);
	GError *error = NULL;
	const char *id;
	size_t count, i;
	int j, id_count;

	testsuite_check ("fragment messages clone raw headers and options and replace envelope ids");
	g_mime_parser_options_set_address_compliance_mode (options, GMIME_RFC_COMPLIANCE_STRICT);
	g_mime_parser_options_set_max_mime_depth (options, 23);
	g_assert_true (g_mime_parser_options_set_charset_encoding (options, "iso-8859-1", &error));
	g_assert_no_error (error);
	message = parse_split_message (wire, sizeof (wire) - 1, options);
	g_mime_parser_options_free (options);
	original = g_mime_entity_get_header_list (GMIME_ENTITY (message));
	source = g_mime_header_list_get_header (original, "X-CaSe");
	g_assert_cmpint (g_mime_header_get_offset (source), >=, 0);
	raw = g_mime_header_get_raw_value (source);
	name = g_mime_header_get_raw_name (source);
	messages = g_mime_message_partial_split (message, 16, &count, NULL);
	g_assert_nonnull (messages);
	g_assert_cmpuint (count, >, 1);

	for (i = 0; i < count; i++) {
		GMimeMessagePrivate *priv;

		id = g_mime_message_get_message_id (messages[i]);
		g_assert_nonnull (id);
		g_assert_cmpstr (id, !=, "source@example.com");
		g_assert_true (g_hash_table_add (ids, g_strdup (id)));
		priv = _g_mime_message_get_private (messages[i]);

		assert_partial_parameters (GMIME_MESSAGE_PARTIAL (priv->mime_part),
					   "source@example.com", i + 1, count);
		cloned = g_mime_entity_get_header_list (GMIME_ENTITY (messages[i]));
		g_assert_true (cloned->options != original->options);
		g_assert_cmpint (g_mime_parser_options_get_address_compliance_mode (cloned->options),
				==, GMIME_RFC_COMPLIANCE_STRICT);
		g_assert_cmpint (g_mime_parser_options_get_max_mime_depth (cloned->options), ==, 23);
		g_assert_cmpstr (g_mime_parser_options_get_charset_encoding (cloned->options), ==,
				g_mime_parser_options_get_charset_encoding (original->options));
		id_count = 0;
		for (j = 0; j < g_mime_header_list_get_count (cloned); j++) {
			header = g_mime_header_list_get_header_at (cloned, j);
			g_assert_cmpint (g_mime_header_get_offset (header), ==, -1);
			if (g_mime_header_get_id (header) == GMIME_HEADER_MESSAGE_ID) {
				g_assert_cmpint (j, ==, 1);
				g_assert_cmpstr (g_mime_header_get_name (header), ==, "Message-Id");
				id_count++;
			}
		}
		g_assert_cmpint (id_count, ==, 1);
		header = g_mime_header_list_get_header (cloned, "X-CaSe");
		g_assert_true (header != source);
		actual = g_mime_header_get_raw_value (header);
		g_assert_true (g_bytes_equal (raw, actual));
		g_bytes_unref (actual);
		actual = g_mime_header_get_raw_name (header);
		g_assert_true (g_bytes_equal (name, actual));
		g_bytes_unref (actual);
	}

	g_assert_cmpint (g_mime_header_list_get_count (original), ==, 4);
	g_assert_cmpstr (g_mime_header_get_name (g_mime_header_list_get_header_at (original, 1)),
			==, "mEsSaGe-Id");
	g_mime_parser_options_set_max_mime_depth (original->options, 24);
	cloned = g_mime_entity_get_header_list (GMIME_ENTITY (messages[0]));
	g_assert_cmpint (g_mime_parser_options_get_max_mime_depth (cloned->options), ==, 23);
	g_mime_header_set_value (g_mime_header_list_get_header (cloned, "X-CaSe"), NULL, "edited", NULL);
	actual = g_mime_header_get_raw_value (source);
	g_assert_true (g_bytes_equal (raw, actual));
	g_bytes_unref (actual);
	g_object_unref (message);
	cloned = g_mime_entity_get_header_list (GMIME_ENTITY (messages[1]));
	actual = g_mime_header_get_raw_value (g_mime_header_list_get_header (cloned, "X-CaSe"));
	g_assert_true (g_bytes_equal (raw, actual));
	g_bytes_unref (actual);
	for (i = 0; i < count; i++)
		g_object_unref (messages[i]);
	g_free (messages);
	g_bytes_unref (raw);
	g_bytes_unref (name);
	g_hash_table_destroy (ids);
	testsuite_check_passed ();
}

static void
test_split_sizes (void)
{
	static const char wire[] =
		"Message-Id: <sizes@example.com>\r\n"
		"Subject: sizes\r\n"
		"MIME-Version: 1.0\r\n"
		"Content-Type: application/octet-stream\r\n"
		"\r\nab\ncdef\r\nghij\0klmn\377opqr\n";
	static const size_t limits[] = { 1, 2, 3, 7, 8, 15, 16, 31, 32, 63, 64, G_MAXSIZE };
	GMimeMessage *message = parse_split_message (wire, sizeof (wire) - 1, NULL), **messages, *joined;
	GMimeMessagePartial **partials;
	GMimeStream *combined;
	GByteArray *buffer;
	GBytes *before, *after;
	gboolean boundary_newline = FALSE;
	const char *data;
	size_t length, limit, count, i;
	guint c;

	testsuite_check ("split sizes preserve exact bytes, identity and reconstruction");
	before = split_message_bytes (message);
	data = g_bytes_get_data (before, &length);
	for (c = 0; c < G_N_ELEMENTS (limits) + 3; c++) {
		limit = c < G_N_ELEMENTS (limits) ? limits[c] : length + c - G_N_ELEMENTS (limits) - 1;
		messages = g_mime_message_partial_split (message, limit, &count, NULL);
		g_assert_nonnull (messages);
		g_assert_cmpuint (count, >=, 1);
		if (limit >= length) {
			g_assert_cmpuint (count, ==, 1);
			g_assert_true (messages[0] == message);
		} else {
			combined = g_mime_stream_mem_new ();
			partials = g_new (GMimeMessagePartial *, count);
			for (i = 0; i < count; i++) {
				ssize_t size;
				GMimeMessagePrivate *priv = _g_mime_message_get_private (messages[i]);

				partials[i] = GMIME_MESSAGE_PARTIAL (priv->mime_part);
				assert_partial_parameters (partials[i], "sizes@example.com", i + 1, count);
				size = g_mime_content_write_to_stream (
					g_mime_part_get_content (GMIME_PART (partials[i])), combined);
				g_assert_cmpint (size, >, 0);
				g_assert_cmpuint (size, <=, limit + 1);
				if ((size_t) size == limit + 1) {
					buffer = g_mime_stream_mem_get_byte_array (GMIME_STREAM_MEM (combined));
					g_assert_cmpint (buffer->data[buffer->len - 1], ==, '\n');
					boundary_newline = TRUE;
				}
			}
			buffer = g_mime_stream_mem_get_byte_array (GMIME_STREAM_MEM (combined));
			g_assert_cmpmem (buffer->data, buffer->len, data, length);
			joined = g_mime_message_partial_join (NULL, messages[0], partials, count, NULL);
			g_assert_nonnull (joined);
			after = split_message_bytes (joined);
			g_assert_true (g_bytes_equal (before, after));
			g_bytes_unref (after);
			g_object_unref (joined);
			g_free (partials);
			g_object_unref (combined);
		}
		for (i = 0; i < count; i++)
			g_object_unref (messages[i]);
		g_free (messages);
		after = split_message_bytes (message);
		g_assert_true (g_bytes_equal (before, after));
		g_bytes_unref (after);
	}
	g_assert_true (boundary_newline);
	g_bytes_unref (before);
	g_object_unref (message);
	testsuite_check_passed ();
}

static void
test_split_raw_headers (void)
{
	static const char raw[] = " before\0after\r\n";
	static const char wire[] = "X-Binary: before\0after\r\n";
	GMimeMessage *message = g_mime_message_new (FALSE), **messages;
	GMimeHeader *header = g_mime_header_new ("X-Binary", "initial", NULL);
	GBytes *bytes = g_bytes_new_static (raw, sizeof (raw) - 1), *result;
	GMimeStream *stream;
	GByteArray *output;
	GError *error = NULL;
	size_t count, i;

	testsuite_check ("splitting preserves complete binary raw headers");
	g_assert_true (g_mime_header_set_raw_value (header, bytes, &error));
	g_assert_no_error (error);
	g_mime_header_list_add (g_mime_entity_get_header_list (GMIME_ENTITY (message)), header);
	g_mime_message_set_message_id (message, "split@example.com");
	messages = g_mime_message_partial_split (message, 16, &count, NULL);
	g_assert_nonnull (messages);
	g_assert_cmpuint (count, >, 1);
	g_object_unref (header);
	g_object_unref (message);

	for (i = 0; i < count; i++) {
		header = g_mime_header_list_get_header (g_mime_entity_get_header_list (GMIME_ENTITY (messages[i])), "X-Binary");
		result = g_mime_header_get_raw_value (header);
		g_assert_true (g_bytes_equal (result, bytes));
		g_bytes_unref (result);
		stream = g_mime_stream_mem_new ();
		g_assert_cmpint (g_mime_header_write_to_stream (header, NULL, stream), ==, sizeof (wire) - 1);
		output = g_mime_stream_mem_get_byte_array (GMIME_STREAM_MEM (stream));
		g_assert_cmpmem (output->data, output->len, wire, sizeof (wire) - 1);
		g_object_unref (stream);
		g_object_unref (messages[i]);
	}
	g_free (messages);
	g_bytes_unref (bytes);
	testsuite_check_passed ();
}

static void
test_join_headers (void)
{
	GMimeMessagePrivate *priv;
	static const char outer[] =
		"x-TrAcE: one\0binary\r\nSubject: outer first\r\nReceived: outer trace\r\n"
		"Message-Id: <outer@example.com>\r\nSubject: outer second\r\nX-Trace: two\r\n"
		"Encrypted: outer encryption\r\nMIME-Version: 2.0\r\nDate: Fri, 1 Jan 2021 00:00:00 +0000\r\n"
		"Content-Type: message/partial; id=id; number=1; total=3\r\n\r\n";
	static const char inner[] =
		"From: discarded@example.com\r\nSubject: inner first\r\nX-Drop: discarded\r\n"
		"Message-Id: <inner@example.com>\r\nSubject: inner second\r\nEncrypted: inner first\r\n"
		"MIME-Version: 1.0\r\nEncrypted: inner second\r\nMessage-Id: <second@example.com>\r\n"
		"Content-Type: application/octet-stream\r\nContent-X-Body: retained\r\n\r\nbefore\0after\377";
	static const char *names[] = {
		"x-TrAcE", "Subject", "Received", "Message-Id", "Subject", "X-Trace",
		"Encrypted", "MIME-Version", "Date", "Encrypted", "Message-Id"
	};
	GMimeMessage *envelope = parse_split_message (outer, sizeof (outer) - 1, NULL), *joined;
	GMimeMessagePartial *parts[3], *input[3];
	GMimeParserOptions *options = g_mime_parser_options_new ();
	GMimeHeaderList *headers, *original;
	GMimeHeader *header;
	GBytes *before, *after, *raw, *copy;
	GMimeStream *output;
	GByteArray *buffer;
	GError *error = NULL;
	guint i;

	testsuite_check ("Join combines ordered raw headers, clones envelope fields and clears offsets");
	before = split_message_bytes (envelope);
	original = g_mime_entity_get_header_list (GMIME_ENTITY (envelope));
	raw = g_mime_header_get_raw_value (g_mime_header_list_get_header_at (original, 0));
	g_mime_parser_options_set_address_compliance_mode (options, GMIME_RFC_COMPLIANCE_STRICT);
	g_mime_parser_options_set_max_mime_depth (options, 23);
	for (i = 0; i < G_N_ELEMENTS (parts); i++) {
		size_t start = (sizeof (inner) - 1) * i / 3;
		size_t end = (sizeof (inner) - 1) * (i + 1) / 3;

		parts[i] = g_mime_message_partial_new ("id", i + 1, 3);
		set_reconstruction_content (parts[i], inner + start, end - start,
					    i % 2 ? GMIME_CONTENT_ENCODING_QUOTEDPRINTABLE : GMIME_CONTENT_ENCODING_BASE64);
		input[2 - i] = parts[i];
	}
	joined = g_mime_message_partial_join (options, envelope, input, G_N_ELEMENTS (input), &error);
	g_assert_no_error (error);
	g_assert_nonnull (joined);
	headers = g_mime_entity_get_header_list (GMIME_ENTITY (joined));
	g_assert_cmpint (g_mime_header_list_get_count (headers), ==, G_N_ELEMENTS (names));
	g_assert_cmpint (g_mime_parser_options_get_address_compliance_mode (headers->options),
			==, GMIME_RFC_COMPLIANCE_STRICT);
	g_assert_cmpint (g_mime_parser_options_get_max_mime_depth (headers->options), ==, 23);
	g_mime_parser_options_set_max_mime_depth (options, 24);
	g_assert_cmpint (g_mime_parser_options_get_max_mime_depth (headers->options), ==, 23);
	for (i = 0; i < G_N_ELEMENTS (names); i++) {
		header = g_mime_header_list_get_header_at (headers, i);
		g_assert_cmpstr (g_mime_header_get_name (header), ==, names[i]);
		g_assert_cmpint (g_mime_header_get_offset (header), ==, -1);
	}
	g_assert_cmpstr (g_mime_header_get_value (g_mime_header_list_get_header_at (headers, 1)), ==, "inner first");
	g_assert_cmpstr (g_mime_header_get_value (g_mime_header_list_get_header_at (headers, 4)), ==, "inner second");
	g_assert_cmpstr (g_mime_header_get_value (g_mime_header_list_get_header_at (headers, 9)), ==, "inner second");
	header = g_mime_header_list_get_header_at (headers, 0);
	g_assert_true (header != g_mime_header_list_get_header_at (original, 0));
	copy = g_mime_header_get_raw_value (header);
	g_assert_true (g_bytes_equal (raw, copy));
	g_bytes_unref (copy);
	g_bytes_unref (raw);
	g_mime_header_set_value (header, NULL, "independent", NULL);
	after = split_message_bytes (envelope);
	g_assert_true (g_bytes_equal (before, after));
	g_bytes_unref (before);
	g_bytes_unref (after);
	priv = _g_mime_message_get_private (joined);

	headers = g_mime_entity_get_header_list (priv->mime_part);
	for (i = 0; i < (guint) g_mime_header_list_get_count (headers); i++)
		g_assert_cmpint (g_mime_header_get_offset (g_mime_header_list_get_header_at (headers, i)), ==, -1);
	g_assert_cmpstr (g_mime_entity_get_header (priv->mime_part, "Content-X-Body"), ==, "retained");
	for (i = 0; i < G_N_ELEMENTS (parts); i++) {
		g_assert_true (input[2 - i] == parts[i]);
		g_object_unref (parts[i]);
	}
	g_object_unref (envelope);
	g_mime_parser_options_free (options);
	output = g_mime_stream_mem_new ();
	g_assert_cmpint (g_mime_content_decode_to_stream (g_mime_part_get_content (GMIME_PART (priv->mime_part)), output), ==, 13);
	buffer = g_mime_stream_mem_get_byte_array (GMIME_STREAM_MEM (output));
	g_assert_cmpmem (buffer->data, buffer->len, "before\0after\377", 13);
	g_object_unref (output);
	g_object_unref (joined);
	testsuite_check_passed ();
}

static void
test_join_nullable_ids (void)
{
	static const char wire[] = "Subject: nullable ids\r\n\r\nbody";
	static const char *ids[] = { NULL, "", "same" };
	GMimeMessage *envelope = g_mime_message_new (FALSE), *joined;
	GMimeMessagePartial *parts[] = {
		g_mime_message_partial_new ("same", 1, 2),
		g_mime_message_partial_new ("same", 2, 2)
	};
	GError *error = NULL;
	guint i, j;

	testsuite_check ("Join accepts matching nullable ids and reports metadata errors");
	set_reconstruction_content (parts[0], wire, sizeof (wire) - 1, GMIME_CONTENT_ENCODING_UNSPECIFIED);
	g_assert_null (g_mime_message_partial_join (NULL, envelope, NULL, 0, &error));
	g_assert_no_error (error);
	for (i = 0; i < G_N_ELEMENTS (ids); i++) {
		for (j = 0; j < G_N_ELEMENTS (parts); j++) {
			GMimeParamList *parameters = g_mime_content_type_get_parameters (
				g_mime_entity_get_content_type (GMIME_ENTITY (parts[j])));
			if (ids[i])
				g_mime_param_list_set_parameter (parameters, "id", ids[i]);
			else
				g_mime_param_list_remove (parameters, "id");
		}
		joined = g_mime_message_partial_join (NULL, envelope, parts, G_N_ELEMENTS (parts), &error);
		g_assert_no_error (error);
		g_assert_nonnull (joined);
		g_object_unref (joined);
		g_mime_entity_set_content_type_parameter (GMIME_ENTITY (parts[1]), "id", "different");
		g_assert_null (g_mime_message_partial_join (NULL, envelope, parts, G_N_ELEMENTS (parts), &error));
		g_assert_error (error, GMIME_ERROR, GMIME_ERROR_INVALID_OPERATION);
		g_clear_error (&error);
	}
	g_assert_null (g_mime_message_partial_join (NULL, envelope, parts, (size_t) G_MAXINT32 + 1, &error));
	g_assert_error (error, GMIME_ERROR, GMIME_ERROR_INVALID_OPERATION);
	g_clear_error (&error);
	g_mime_entity_set_content_type_parameter (GMIME_ENTITY (parts[1]), "id", "same");
	g_mime_part_set_content (GMIME_PART (parts[0]), NULL);
	g_assert_null (g_mime_message_partial_join (NULL, envelope, parts, G_N_ELEMENTS (parts), &error));
	g_assert_error (error, GMIME_ERROR, GMIME_ERROR_PARSE_ERROR);
	g_clear_error (&error);
	g_object_unref (parts[0]);
	g_object_unref (parts[1]);
	g_object_unref (envelope);
	testsuite_check_passed ();
}

static void
test_split_header_policy (void)
{
	guint kind;

	for (kind = 0; kind < 4; kind++) {
		GMimeMessagePrivate *priv;
		char *wire = g_strdup_printf (
			"From: envelope@example.com\r\nSubject: header policy\r\n"
			"X-Envelope: restored\r\nMessage-Id: <policy@example.com>\r\n"
			"MIME-Version: 1.0\r\nContent-Type: multipart/%s; boundary=b\r\n"
			"Content-X-Root: hidden\r\n\r\n--b\r\nContent-Type: %s\r\n"
			"X-Child: extension\r\nReceived: child trace\r\n\r\n%sbody\r\n--b--\r\n",
			kind & 1 ? "signed" : "mixed", kind & 2 ? "message/rfc822" : "text/plain",
			kind & 2 ? "From: inner@example.com\r\nSubject: inner\r\n"
			"Content-Type: text/plain\r\n\r\n" : "");
		GMimeMessage *message = parse_split_message (wire, strlen (wire), NULL), **messages, *joined;
		GMimeMessagePartial **partials;
		GMimeStream *combined = g_mime_stream_mem_new ();
		GByteArray *buffer;
		GBytes *before = split_message_bytes (message), *after;
		GError *error = NULL;
		size_t count, i;

		testsuite_check ("Split suppresses envelope/unknown fields and preserves signed children (%u)", kind);
		messages = g_mime_message_partial_split (message, 20, &count, &error);
		g_assert_no_error (error);
		g_assert_nonnull (messages);
		g_assert_cmpuint (count, >, 1);
		partials = g_new (GMimeMessagePartial *, count);
		for (i = 0; i < count; i++) {
			GMimeMessagePrivate *messages_priv = _g_mime_message_get_private (messages[i]);

			partials[i] = GMIME_MESSAGE_PARTIAL (messages_priv->mime_part);
			g_assert_cmpint (g_mime_content_write_to_stream (
				g_mime_part_get_content (GMIME_PART (partials[i])), combined), >, 0);
		}
		buffer = g_mime_stream_mem_get_byte_array (GMIME_STREAM_MEM (combined));
		g_assert_cmpint (g_strstr_len ((char *) buffer->data, buffer->len, "From:") != NULL,
				==, kind == 3);
		g_assert_null (g_strstr_len ((char *) buffer->data, buffer->len, "X-Envelope:"));
		g_assert_null (g_strstr_len ((char *) buffer->data, buffer->len, "Content-X-Root:"));
		g_assert_nonnull (g_strstr_len ((char *) buffer->data, buffer->len, "X-Child:"));
		g_assert_nonnull (g_strstr_len ((char *) buffer->data, buffer->len, "Received: child trace"));
		joined = g_mime_message_partial_join (NULL, messages[0], partials, count, &error);
		g_assert_no_error (error);
		g_assert_nonnull (joined);
		g_assert_cmpstr (g_mime_entity_get_header (GMIME_ENTITY (joined), "X-Envelope"), ==, "restored");
		priv = _g_mime_message_get_private (joined);

		g_assert_null (g_mime_entity_get_header (priv->mime_part, "Content-X-Root"));
		g_assert_false (g_mime_format_options_is_hidden_header (NULL, "X-Envelope"));
		g_assert_false (g_mime_format_options_is_hidden_header (NULL, "From"));
		after = split_message_bytes (message);
		g_assert_true (g_bytes_equal (before, after));
		g_bytes_unref (before);
		g_bytes_unref (after);
		g_free (partials);
		for (i = 0; i < count; i++)
			g_object_unref (messages[i]);
		g_free (messages);
		g_free (wire);
		g_object_unref (combined);
		g_object_unref (message);
		g_object_unref (joined);
		testsuite_check_passed ();
	}
}

int main (int argc, char **argv)
{
	const char *datadir = "data/partial";
	GMimeStream *stream, *combined, *expected;
	GMimeMessage *message, **messages, *envelope;
	GMimeMessagePartial **parts;
	GMimeFormatOptions *format;
	GString *input, *output;
	GMimeParser *parser;
	GPtrArray *partials;
	int inlen, outlen;
	GDir *data, *dir;
	const char *dent;
	size_t i, n;
	char *path;
	
	g_mime_init ();
	
	testsuite_init (argc, argv);
	
	testsuite_start ("message/partial");
	test_partial_constructor ();
	test_partial_parameters ();
	test_parsed_partial_parameters ();
	test_partial_reconstruction_parameters ();
	test_reconstruction_content ();
	test_reconstruction_failures ();
	test_split_without_message_id ();
	test_split_arguments ();
	test_split_failures ();
	test_split_message_clones ();
	test_split_sizes ();
	test_split_raw_headers ();
	test_join_headers ();
	test_join_nullable_ids ();
	test_split_header_policy ();
	
	format = g_mime_format_options_get_default ();
	
	for (i = 1; i < argc; i++) {
		if (argv[i][0] != '-') {
			datadir = argv[i];
			break;
		}
	}
	
	output = g_string_new (datadir);
	g_string_append_c (output, G_DIR_SEPARATOR);
	g_string_append (output, "output");
	
	input = g_string_new (datadir);
	g_string_append_c (input, G_DIR_SEPARATOR);
	g_string_append (input, "input");
	
	if (!(data = g_dir_open (input->str, 0, NULL)))
		goto exit;
	
	g_string_append_c (output, G_DIR_SEPARATOR);
	g_string_append_c (input, G_DIR_SEPARATOR);
	outlen = output->len;
	inlen = input->len;
	
	while ((dent = g_dir_read_name (data))) {
		g_string_append (output, dent);
		g_string_append (input, dent);
		
		parser = g_mime_parser_new ();
		partials = g_ptr_array_new_with_free_func (g_object_unref);
		envelope = NULL;
		dir = NULL;
		
		testsuite_check ("%s", dent);
		try {
			if (!(dir = g_dir_open (input->str, 0, NULL)))
				throw (exception_new ("Failed to open `%s'", input->str));
			
			while ((dent = g_dir_read_name (dir))) {
				GMimeMessagePrivate *priv;

				path = g_build_filename (input->str, dent, NULL);
				
				if (!(stream = g_mime_stream_file_open (path, "r", NULL)))
					throw (exception_new ("Failed to open `%s'", path));
				
				g_mime_parser_init_with_stream (parser, stream);
				g_object_unref (stream);
				
				if (!(message = g_mime_parser_construct_message (parser, NULL, NULL)))
					throw (exception_new ("Failed to parse `%s'", path));
				
				priv = _g_mime_message_get_private (message);

				if (!GMIME_IS_MESSAGE_PARTIAL (priv->mime_part)) {
					g_object_unref (message);
					throw (exception_new ("`%s' is not a message/partial", path));
				}
				
				g_ptr_array_add (partials, priv->mime_part);
				g_object_ref (priv->mime_part);
				if (g_mime_message_partial_get_number (GMIME_MESSAGE_PARTIAL (priv->mime_part)) == 1)
					g_set_object (&envelope, message);
				g_object_unref (message);
				g_free (path);
				path = NULL;
			}
			
			parts = (GMimeMessagePartial **) partials->pdata;
			n = partials->len;
			
			if (!(message = g_mime_message_partial_join (NULL, envelope, parts, n, NULL))) {
				throw (exception_new ("Failed to recombine message/partial `%s'", input->str));
			}
			
			g_ptr_array_set_size (partials, 0);
			
			combined = g_mime_stream_mem_new ();
			g_mime_entity_write_to_stream (GMIME_ENTITY (message), format, combined);
			g_mime_stream_reset (combined);
			
			if (!(expected = g_mime_stream_file_open (output->str, "r", NULL))) {
				expected = g_mime_stream_file_open (output->str, "w", NULL);
				g_mime_stream_write_to_stream (combined, expected);
				g_mime_stream_flush (expected);
				g_object_unref (expected);
				g_object_unref (combined);
				g_object_unref (message);
				
				throw (exception_new ("Failed to open `%s'", output->str));
			}
			
			if (!streams_match (expected, combined)) {
				g_object_unref (combined);
				g_object_unref (expected);
				g_object_unref (message);
				
				throw (exception_new ("messages do not match for `%s'", input->str));
			}
			
			g_object_unref (combined);
			
			if (!(messages = g_mime_message_partial_split (message, 4096, &n, NULL))) {
				g_object_unref (expected);
				g_object_unref (message);
				throw (exception_new ("Failed to split message `%s'", input->str));
			}
			
			g_object_unref (message);
			g_set_object (&envelope, messages[0]);
			
			for (i = 0; i < n; i++) {
				GMimeMessagePrivate *messages_priv = _g_mime_message_get_private (messages[i]);
				GMimeMessagePrivate *message_priv;

				g_ptr_array_add (partials, messages_priv->mime_part);
				message_priv = _g_mime_message_get_private (messages[i]);

				g_object_ref (message_priv->mime_part);
				g_object_unref (messages[i]);
			}
			g_free (messages);
			
			parts = (GMimeMessagePartial **) partials->pdata;
			n = partials->len;
			
			if (!(message = g_mime_message_partial_join (NULL, envelope, parts, n, NULL))) {
				g_object_unref (expected);
				
				throw (exception_new ("Failed to recombine split message/partial `%s'", input->str));
			}
			g_ptr_array_set_size (partials, 0);
			
			combined = g_mime_stream_mem_new ();
			g_mime_entity_write_to_stream (GMIME_ENTITY (message), format, combined);
			g_mime_stream_reset (combined);
			g_mime_stream_reset (expected);
			g_object_unref (message);
			
			if (!streams_match (expected, combined)) {
				g_object_unref (combined);
				g_object_unref (expected);
				
				throw (exception_new ("re-split/combined messages do not match for `%s'", input->str));
			}
			
			g_object_unref (combined);
			g_object_unref (expected);
			g_dir_close (dir);
			
			testsuite_check_passed ();
		} catch (ex) {
			testsuite_check_failed ("%s: %s", input->str, ex->message);
			
			if (dir != NULL)
				g_dir_close (dir);
			
			g_free (path);
		} finally;
		g_clear_object (&envelope);
		
		g_string_truncate (output, outlen);
		g_string_truncate (input, inlen);
		g_ptr_array_free (partials, TRUE);
		g_object_unref (parser);
	}
	
	g_dir_close (data);
	
 exit:
	g_string_free (output, TRUE);
	g_string_free (input, TRUE);
	
	testsuite_end ();
	
	g_mime_shutdown ();
	
	return testsuite_exit ();
}
