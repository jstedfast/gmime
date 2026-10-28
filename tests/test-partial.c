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
#include <stdlib.h>
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
	char buf[4096], dbuf[4096], errstr[1024], *bufptr, *bufend, *dbufptr;
	gint64 len, totalsize, totalread = 0;
	size_t nread, size;
	gint64 offset = 0;
	ssize_t n;
	
	v(fprintf (stdout, "Checking if streams match... "));
	
	if (expected->bound_end != -1) {
		totalsize = expected->bound_end - expected->position;
	} else if ((len = g_mime_stream_length (expected)) == -1) {
		sprintf (errstr, "Error: Unable to get length of expected stream\n");
		goto fail;
	} else if (len < (expected->position - expected->bound_start)) {
		sprintf (errstr, "Error: Overflow on expected stream?\n");
		goto fail;
	} else {
		totalsize = len - (expected->position - expected->bound_start);
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
	GMimeParamList *parameters;
	guint i;

	testsuite_check ("reconstruction rejects invalid numbers before sorting partials");
	parameters = g_mime_content_type_get_parameters (g_mime_entity_get_content_type (GMIME_ENTITY (second)));
	for (i = 0; i < G_N_ELEMENTS (invalid); i++) {
		if (invalid[i])
			g_mime_param_list_set_parameter (parameters, "number", invalid[i]);
		else
			g_mime_param_list_remove (parameters, "number");
		g_assert_null (g_mime_message_partial_reconstruct_message (partials, G_N_ELEMENTS (partials)));
		g_assert_true (partials[0] == first);
		g_assert_true (partials[1] == second);
	}
	g_mime_param_list_set_parameter (parameters, "number", "1");
	g_mime_param_list_set_parameter (parameters, "id", "other");
	g_assert_null (g_mime_message_partial_reconstruct_message (partials, G_N_ELEMENTS (partials)));
	g_assert_true (partials[0] == first);
	g_assert_true (partials[1] == second);
	g_object_unref (first);
	g_object_unref (second);
	testsuite_check_passed ();
}

static void
test_split_without_message_id (void)
{
	GMimeMessage *message = g_mime_message_new (FALSE), **messages, *reconstructed;
	GMimeMessagePartial **partials;
	char *id;
	size_t count, i;

	testsuite_check ("splitting a message without Message-ID generates a shared partial id");
	g_mime_entity_set_header (GMIME_ENTITY (message), "Subject", "Split without an id", NULL);
	g_assert_null (g_mime_message_get_message_id (message));
	messages = g_mime_message_partial_split_message (message, 8, &count);
	g_assert_nonnull (messages);
	g_assert_cmpuint (count, >, 1);
	partials = g_new (GMimeMessagePartial *, count);
	id = g_strdup (g_mime_message_partial_get_id (GMIME_MESSAGE_PARTIAL (messages[0]->mime_part)));
	g_assert_nonnull (id);
	g_assert_cmpstr (id, !=, "");
	for (i = 0; i < count; i++) {
		partials[i] = GMIME_MESSAGE_PARTIAL (messages[i]->mime_part);
		assert_partial_parameters (partials[i], id, i + 1, count);
	}
	g_assert_null (g_mime_message_get_message_id (message));
	reconstructed = g_mime_message_partial_reconstruct_message (partials, count);
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
	messages = g_mime_message_partial_split_message (message, 32, &count);
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

int main (int argc, char **argv)
{
	const char *datadir = "data/partial";
	GMimeStream *stream, *combined, *expected;
	GMimeMessage *message, **messages;
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
	test_split_without_message_id ();
	test_split_raw_headers ();
	
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
		partials = g_ptr_array_new ();
		dir = NULL;
		
		testsuite_check ("%s", dent);
		try {
			if (!(dir = g_dir_open (input->str, 0, NULL)))
				throw (exception_new ("Failed to open `%s'", input->str));
			
			while ((dent = g_dir_read_name (dir))) {
				path = g_build_filename (input->str, dent, NULL);
				
				if (!(stream = g_mime_stream_file_open (path, "r", NULL)))
					throw (exception_new ("Failed to open `%s'", path));
				
				g_mime_parser_init_with_stream (parser, stream);
				g_object_unref (stream);
				
				if (!(message = g_mime_parser_construct_message (parser, NULL, NULL)))
					throw (exception_new ("Failed to parse `%s'", path));
				
				if (!GMIME_IS_MESSAGE_PARTIAL (message->mime_part)) {
					g_object_unref (message);
					throw (exception_new ("`%s' is not a message/partial", path));
				}
				
				g_ptr_array_add (partials, message->mime_part);
				g_object_ref (message->mime_part);
				g_object_unref (message);
				g_free (path);
				path = NULL;
			}
			
			parts = (GMimeMessagePartial **) partials->pdata;
			n = partials->len;
			
			if (!(message = g_mime_message_partial_reconstruct_message (parts, n))) {
				for (i = 0; i < n; i++)
					g_object_unref (parts[i]);
				
				throw (exception_new ("Failed to recombine message/partial `%s'", input->str));
			}
			
			for (i = 0; i < n; i++)
				g_object_unref (parts[i]);
			
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
			
			if (!(messages = g_mime_message_partial_split_message (message, 4096, &n))) {
				g_object_unref (expected);
				g_object_unref (message);
				
				throw (exception_new ("Failed to split message `%s'", input->str));
			}
			
			g_object_unref (message);
			
			g_ptr_array_set_size (partials, 0);
			for (i = 0; i < n; i++) {
				g_ptr_array_add (partials, messages[i]->mime_part);
				g_object_ref (messages[i]->mime_part);
				g_object_unref (messages[i]);
			}
			g_free (messages);
			
			parts = (GMimeMessagePartial **) partials->pdata;
			n = partials->len;
			
			if (!(message = g_mime_message_partial_reconstruct_message (parts, n))) {
				for (i = 0; i < n; i++)
					g_object_unref (parts[i]);
				g_object_unref (expected);
				
				throw (exception_new ("Failed to recombine split message/partial `%s'", input->str));
			}
			
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
