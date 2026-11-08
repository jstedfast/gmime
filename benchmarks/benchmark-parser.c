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

#include "benchmark.h"

typedef struct {
	GByteArray *input;
	GMimeFormat format;
	GMimeContentStorage storage;
	gboolean reader, compliance;
} ParseData;

static void
ignore_issue (const GMimeComplianceIssue *issue, gpointer data)
{
}

static guint64
parse_message (gpointer data)
{
	ParseData *test = data;
	GMimeStream *stream = g_mime_stream_mem_new_with_byte_array (test->input);
	GError *error = NULL;
	guint64 count = 0;
	gint64 position;

	/* The fixture outlives every model and stream; don't copy it into each input. */
	g_mime_stream_mem_set_owner (GMIME_STREAM_MEM (stream), FALSE);

	if (test->reader) {
		GMimeReader *reader = g_mime_reader_new_with_format (NULL, stream, test->format);

		if (test->compliance)
			g_mime_reader_set_compliance_logger (reader, ignore_issue, NULL, NULL);
		do {
			benchmark_check (g_mime_reader_read_message (reader, &error), "reading message", &error);
			count++;
		} while (test->format == GMIME_FORMAT_MBOX && !g_mime_reader_eos (reader));

		position = g_mime_reader_tell (reader);
		g_object_unref (reader);
	} else {
		GMimeParser *parser = g_mime_parser_new_with_stream_and_storage (stream, test->format,
									     test->storage, &error);

		benchmark_check (parser != NULL, "creating parser", &error);

		if (test->compliance)
			g_mime_parser_set_compliance_logger (parser, ignore_issue, NULL, NULL);

		do {
			GMimeMessage *message = g_mime_parser_construct_message (parser, NULL, &error);

			benchmark_check (message != NULL, "parsing message", &error);
			g_object_unref (message);
			count++;
		} while (test->format == GMIME_FORMAT_MBOX && !g_mime_parser_eos (parser));

		position = g_mime_parser_tell (parser);
		g_object_unref (parser);
	}

	g_object_unref (stream);

	if (position != test->input->len)
		g_error ("Parser stopped at %" G_GINT64_FORMAT " of %u bytes", position, test->input->len);

	return count;
}

static GBytes *
nested_message (gboolean mixed, guint depth)
{
	GString *text = g_string_new (mixed ?
		"From: a@example.org\r\nSubject: nested\r\nMIME-Version: 1.0\r\n" :
		"From: a@example.org\r\n");
	guint i;

	for (i = 0; i < depth; i++) {
		if (mixed && i % 2 == 0) {
			g_string_append_printf (text, "Content-Type: multipart/mixed; boundary=\"b%u\"\r\n\r\n"
				"--b%u\r\nContent-Type: text/plain\r\n\r\nbefore %u\r\n--b%u\r\n", i, i, i, i);
		} else {
			g_string_append (text, "Content-Type: message/rfc822\r\n\r\nFrom: a@example.org\r\n");

			if (mixed)
				g_string_append_printf (text, "Subject: level %u\r\n", i);
		}
	}

	g_string_append (text, "Content-Type: text/plain\r\n\r\ninnermost\r\n");

	if (mixed) {
		for (i = depth; i-- > 0;) {
			if (i % 2 == 0)
				g_string_append_printf (text, "--b%u\r\nContent-Type: text/plain\r\n\r\n"
					"after %u\r\n--b%u--\r\n", i, i, i);
		}
	}

	return g_string_free_to_bytes (text);
}

typedef struct {
	GMimeMessage *message;
	GMimePart *text, *attachment;
	GMimeEncodingConstraint constraint;
} PrepareData;

static guint64
prepare_message (gpointer data)
{
	PrepareData *test = data;
	GError *error = NULL;

	g_mime_part_set_content_encoding (test->text, GMIME_CONTENT_ENCODING_UNSPECIFIED);
	g_mime_part_set_content_encoding (test->attachment, GMIME_CONTENT_ENCODING_UNSPECIFIED);

	benchmark_check (g_mime_entity_prepare (GMIME_ENTITY (test->message), test->constraint, &error),
			 "preparing message", &error);

	return ((guint64) g_mime_part_get_content_encoding (test->text) << 32) |
		g_mime_part_get_content_encoding (test->attachment);
}

static void
benchmark_prepare (const char *datadir, const char *testdatadir)
{
	static const struct {
		const char *name;
		GMimeEncodingConstraint constraint;
	} cases[] = {
		{ "Prepare/None", GMIME_ENCODING_CONSTRAINT_BINARY },
		{ "Prepare/SevenBit", GMIME_ENCODING_CONSTRAINT_7BIT },
		{ "Prepare/EightBit", GMIME_ENCODING_CONSTRAINT_8BIT }
	};
	char *encodings = g_build_filename (testdatadir, "encodings", NULL);
	guint i;

	for (i = 0; i < G_N_ELEMENTS (cases); i++) {
		PrepareData test;
		GMimeMultipart *multipart;
		GMimeContent *content;
		GMimeStream *stream;
		GBytes *text, *image;
		char *utf8;
		gsize length;

		if (!benchmark_selected (cases[i].name))
			continue;

		text = benchmark_load (datadir, "lorem-ipsum.txt");
		image = benchmark_load (encodings, "photo.jpg");
		utf8 = g_strndup (g_bytes_get_data (text, NULL), g_bytes_get_size (text));

		test.message = g_mime_message_new (FALSE);
		test.text = GMIME_PART (g_mime_text_part_new ());
		g_mime_text_part_set_text (GMIME_TEXT_PART (test.text), utf8);
		g_free (utf8);

		test.attachment = g_mime_part_new_with_type ("application", "octet-stream");
		stream = g_mime_stream_mem_new_with_buffer (g_bytes_get_data (image, NULL), g_bytes_get_size (image));
		content = g_mime_content_new_with_stream (stream, GMIME_CONTENT_ENCODING_UNSPECIFIED);
		g_mime_part_set_content (test.attachment, content);

		multipart = g_mime_multipart_new_with_subtype ("mixed");
		g_mime_multipart_add (multipart, GMIME_ENTITY (test.text));
		g_mime_multipart_add (multipart, GMIME_ENTITY (test.attachment));
		g_mime_message_set_mime_part (test.message, GMIME_ENTITY (multipart));

		test.constraint = cases[i].constraint;
		length = g_bytes_get_size (text) + g_bytes_get_size (image);
		prepare_message (&test);

		if (g_mime_part_get_content_encoding (test.text) != GMIME_CONTENT_ENCODING_UNSPECIFIED ||
		    g_mime_part_get_content_encoding (test.attachment) !=
		    (i == 0 ? GMIME_CONTENT_ENCODING_BINARY : GMIME_CONTENT_ENCODING_BASE64))
			g_error ("%s: incorrect prepared encoding", cases[i].name);

		benchmark_run (cases[i].name, length, prepare_message, &test);

		g_object_unref (test.message);
		g_object_unref (multipart);
		g_object_unref (test.text);
		g_object_unref (test.attachment);
		g_object_unref (content);
		g_object_unref (stream);
		g_bytes_unref (text);
		g_bytes_unref (image);
	}
	g_free (encodings);
}

void
benchmark_parser (const char *datadir, const char *testdatadir)
{
	static const struct {
		const char *name, *file;
		guint messages;
	} inputs[] = {
		{ "StarTrek", "startrek.eml", 1 },
		{ "HeaderStress", "header-stress.eml", 1 },
		{ "ContentLengthMbox", "content-length.mbox", 29 },
		{ "JwzMbox", NULL, 152 },
		{ "MixedNested64", NULL, 1 },
		{ "NestedRfc822x1000", NULL, 1 }
	};
	guint i, mode, compliance;

	for (i = 0; i < G_N_ELEMENTS (inputs); i++) {
		GByteArray *input = NULL;

		for (mode = 0; mode < 3; mode++) {
			if (i == 5 && mode != 2)
				continue;

			for (compliance = 0; compliance < 2; compliance++) {
				char *name = g_strdup_printf ("%s/%s/%s%s", mode == 2 ? "Reader" : "Parser",
					inputs[i].name, mode == 2 ? "Scan" : mode ? "Source" : "Memory",
					compliance ? "/Compliance" : "");

				if (benchmark_selected (name)) {
					ParseData test;

					if (i >= 4 && g_mime_parser_options_get_max_mime_depth (NULL) < (i == 4 ? 64 : 1000))
						g_error ("%s: default depth limit would truncate the workload", name);

					if (!input) {
						GBytes *bytes;

						if (inputs[i].file) {
							bytes = benchmark_load (datadir, inputs[i].file);
						} else if (i == 3) {
							char *mboxdir = g_build_filename (testdatadir, "mbox", "input", NULL);
							GBytes *lf = benchmark_load (mboxdir, "jwz.mbox");
							bytes = benchmark_newlines (lf, TRUE);
							g_bytes_unref (lf);
							g_free (mboxdir);
						} else {
							bytes = nested_message (i == 4, i == 4 ? 64 : 1000);
						}

						input = g_bytes_unref_to_array (bytes);
					}

					test.input = input;
					test.format = i == 2 || i == 3 ? GMIME_FORMAT_MBOX : GMIME_FORMAT_MESSAGE;
					test.storage = mode ? GMIME_CONTENT_STORAGE_SOURCE : GMIME_CONTENT_STORAGE_MEMORY;
					test.reader = mode == 2;
					test.compliance = compliance;

					if (parse_message (&test) != inputs[i].messages)
						g_error ("%s: unexpected message count", name);

					benchmark_run (name, input->len, parse_message, &test);
				}

				g_free (name);
			}
		}

		if (input)
			g_byte_array_unref (input);
	}

	benchmark_prepare (datadir, testdatadir);
}
