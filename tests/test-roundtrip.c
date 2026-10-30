/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Ported from MimeKit's MimeParserTests.TestRoundTripTestDataMessages.
 *
 * Copyright (C) 2013-2026 .NET Foundation and Contributors
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <errno.h>
#include <gmime/gmime.h>

#include "testsuite.h"

static GMimeNewLineFormat
detect_newline_format (const char *data, gsize length)
{
	gsize i;

	for (i = 0; i < MIN (length, 1024); i++) {
		if (data[i] == '\n')
			return i && data[i - 1] == '\r' ? GMIME_NEWLINE_FORMAT_DOS : GMIME_NEWLINE_FORMAT_UNIX;
	}
	return GMIME_NEWLINE_FORMAT_DOS;
}

static void
test_roundtrip (const char *path, const char *name)
{
	GMimeParserOptions *options = g_mime_parser_options_new ();
	GMimeFormatOptions *format = g_mime_format_options_new ();
	GMimeContentStorage storage;
	GMimeFormat input_format;
	GError *error = NULL;
	char *data, *expected;
	gsize length, expected_length;

	g_assert_true (g_file_get_contents (path, &data, &length, &error));
	g_assert_no_error (error);
	expected = data;
	expected_length = length;
	if (length >= 3 && !memcmp (data, "\357\273\277", 3)) {
		expected += 3;
		expected_length -= 3;
	}
	input_format = expected_length >= 5 && !memcmp (expected, "From ", 5) ? GMIME_FORMAT_MBOX : GMIME_FORMAT_MESSAGE;
	g_mime_parser_options_set_max_header_length (options, 16 * 1024 * 1024);
	g_mime_format_options_set_newline_format (format, detect_newline_format (data, length));

	for (storage = GMIME_CONTENT_STORAGE_MEMORY; storage <= GMIME_CONTENT_STORAGE_SOURCE; storage++) {
		GMimeStream *input = g_mime_stream_mem_new_with_buffer (data, length);
		GMimeStream *output = g_mime_stream_mem_new ();
		GMimeParser *parser;
		GMimeMessage *message;
		GByteArray *actual;
		gsize offset;

		testsuite_check ("Round-trip %s (%s content)", name,
				 storage == GMIME_CONTENT_STORAGE_MEMORY ? "memory" : "source");
		parser = g_mime_parser_new_with_stream_and_storage (input, input_format, storage, &error);
		g_assert_no_error (error);
		g_assert_nonnull (parser);
		message = g_mime_parser_construct_message (parser, options, &error);
		g_assert_no_error (error);
		g_assert_nonnull (message);
		g_assert_true (g_mime_parser_eos (parser));
		if (input_format == GMIME_FORMAT_MBOX) {
			char *marker = g_mime_parser_get_mbox_marker (parser);

			g_assert_nonnull (marker);
			g_assert_cmpint (g_mime_stream_printf (output, "%s%s", marker,
							     g_mime_format_options_get_newline (format)), >, 0);
			g_free (marker);
		}
		if (g_mime_entity_write_to_stream (GMIME_ENTITY (message), format, output) < 0) {
			g_printerr ("%s (%s content): serialization failed: %s\n", name,
				    storage == GMIME_CONTENT_STORAGE_MEMORY ? "memory" : "source", g_strerror (errno));
			testsuite_check_failed ("serialization failed: %s", g_strerror (errno));
		} else {
			actual = g_mime_stream_mem_get_byte_array (GMIME_STREAM_MEM (output));
			for (offset = 0; offset < MIN (expected_length, actual->len); offset++)
				if ((guint8) expected[offset] != actual->data[offset])
					break;
			if (offset != expected_length || offset != actual->len) {
				g_printerr ("%s (%s content): first mismatch at offset %" G_GSIZE_FORMAT
					    " (expected %" G_GSIZE_FORMAT " bytes, got %u)\n", name,
					    storage == GMIME_CONTENT_STORAGE_MEMORY ? "memory" : "source",
					    offset, expected_length, actual->len);
				testsuite_check_failed ("first mismatch at offset %" G_GSIZE_FORMAT
							" (expected %" G_GSIZE_FORMAT " bytes, got %u)",
							offset, expected_length, actual->len);
			} else {
				testsuite_check_passed ();
			}
		}
		g_object_unref (message);
		g_object_unref (parser);
		g_object_unref (input);
		g_object_unref (output);
	}
	g_mime_format_options_free (format);
	g_mime_parser_options_free (options);
	g_free (data);
}

int
main (int argc, char **argv)
{
	static const char *directories[] = { "messages", "compliance", "partial", "openpgp", "dkim", "smime" };
	const char *datadir = "data" G_DIR_SEPARATOR_S "roundtrip";
	GError *error = NULL;
	guint i, count = 0;
	int arg;

	g_mime_init ();
	testsuite_init (argc, argv);
	testsuite_start ("MimeKit whole-message round-trip corpus");
	for (arg = 1; arg < argc; arg++)
		if (argv[arg][0] != '-')
			datadir = argv[arg];

	for (i = 0; i < G_N_ELEMENTS (directories); i++) {
		char *directory = g_build_filename (datadir, directories[i], NULL);
		GDir *dir = g_dir_open (directory, 0, &error);
		GList *files = NULL, *node;
		const char *name;

		g_assert_no_error (error);
		g_assert_nonnull (dir);
		while ((name = g_dir_read_name (dir)))
			files = g_list_insert_sorted (files, g_strdup (name), (GCompareFunc) strcmp);
		g_dir_close (dir);
		for (node = files; node; node = node->next) {
			char *path = g_build_filename (directory, node->data, NULL);
			char *label = g_build_filename (directories[i], node->data, NULL);

			test_roundtrip (path, label);
			g_free (label);
			g_free (path);
			count++;
		}
		g_list_free_full (files, g_free);
		g_free (directory);
	}
	g_assert_cmpuint (count, ==, 68);
	testsuite_end ();
	g_mime_shutdown ();
	return testsuite_exit ();
}
