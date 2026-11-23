/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*  GMime
 *  Copyright (C) Jeffrey Stedfast
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
#include <gmime/gmime-reader-simd-private.h>
#include <string.h>

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

/* Validation and scanner inputs follow MimeKit's benchmarks (see data/LICENSE). */
static void
append_run (GString *text, char c, guint length)
{
	gsize offset = text->len;

	g_string_set_size (text, offset + length);
	memset (text->str + offset, c, length);
}

static GBytes *
header_message (const char *datadir, const char *scenario)
{
	static const char invalid[] = "Subject: bad\0value\r\n another \xff\r\n"
		"Cc: Display\0Name <a@example.com>, \xff@example.com\r\n"
		"Content-Type: text/plain; charset=\"ut\0f-8\"; name=\"\xff\"\r\nTrace: a\0b\xff\r\n";
	GString *text;
	guint i;

	if (!strcmp (scenario, "HeaderStress"))
		return benchmark_load (datadir, "header-stress.eml");
	if (!strcmp (scenario, "StarTrek"))
		return benchmark_load (datadir, "startrek.eml");

	text = g_string_new ("From: Alice <alice@example.com>\r\nTo: Bob <bob@example.com>\r\n"
			     "Date: Mon, 2 Nov 2026 12:00:00 -0500\r\nMessage-Id: <experiment@example.com>\r\n");
	if (!strcmp (scenario, "Short")) {
		g_string_append (text, "Subject: Hello\r\n");
		for (i = 0; i < 12; i++)
			g_string_append_printf (text, "Trace-%u: ordinary header text\r\n", i);
	} else if (!strcmp (scenario, "Folded")) {
		g_string_append (text, "Subject: first line\r\n");
		for (i = 0; i < 64; i++) {
			g_string_append_c (text, ' ');
			append_run (text, 'a', 72);
			g_string_append (text, "\r\n");
		}
		g_string_append (text, "Trace: first line\r\n");
		for (i = 0; i < 64; i++)
			g_string_append (text, " folded plain text\r\n");
	} else if (!strcmp (scenario, "LongLine")) {
		g_string_append (text, "Subject: ");
		append_run (text, 'a', 8192);
		g_string_append (text, "\r\nTrace: ");
		append_run (text, 'b', 8192);
		g_string_append (text, "\r\n");
	} else if (!strcmp (scenario, "Addresses")) {
		g_string_append (text, "Cc: ");
		for (i = 0; i < 80; i++)
			g_string_append_printf (text, "%sPerson %u <person%u@example.com>", i ? ",\r\n " : "", i, i);
		g_string_append (text, "\r\n");
	} else if (!strcmp (scenario, "Parameters")) {
		g_string_append (text, "Content-Type: text/plain");
		for (i = 0; i < 32; i++)
			g_string_append_printf (text, ";\r\n x%u=\"ordinary value %u\"", i, i);
		g_string_append (text, "\r\nContent-Disposition: attachment; filename*0*=utf-8''monthly%20;\r\n"
				 " filename*1*=report.pdf\r\n");
	} else if (!strcmp (scenario, "Utf8")) {
		const char *japanese = "\xe6\x97\xa5\xe6\x9c\xac\xe8\xaa\x9e";
		g_string_append_printf (text, "Subject: caf\xc3\xa9 %s\r\n", japanese);
		for (i = 0; i < 64; i++)
			g_string_append_printf (text, " %s caf\xc3\xa9 ordinary text\r\n", japanese);
		g_string_append (text, "Cc: \xc3\x89lodie <elodie@example.com>\r\nTrace: caf\xc3\xa9\r\n");
	} else if (!strcmp (scenario, "Invalid")) {
		g_string_append_len (text, invalid, sizeof (invalid) - 1);
	} else {
		g_assert_not_reached ();
	}

	g_string_append (text, "\r\nbody\r\n");
	return g_string_free_to_bytes (text);
}

static GBytes *
release_message (const char *datadir, const char *scenario)
{
	GString *text;

	if (!strcmp (scenario, "Utf8"))
		return header_message (datadir, scenario);

	if (!strcmp (scenario, "QP-Utf8-CRLF")) {
		text = g_string_new ("Content-Type: text/plain\r\nContent-Transfer-Encoding: quoted-printable\r\n\r\n");
		while (text->len < 65536)
			g_string_append (text, "=E6=97=A5=E6=9C=AC=E8=AA=9E=C3=A9=E6=97=A5=E6=9C=AC=E8=AA=9E=C3=A9\r\n");
	} else {
		guint width = !strcmp (scenario, "Body-LineWidth-32") ? 32 : 998;
		gsize start;

		text = g_string_new ("Content-Type: text/plain\r\nContent-Transfer-Encoding: 7bit\r\n\r\n");
		start = text->len;
		while (text->len - start < 65536) {
			append_run (text, 'x', width);
			g_string_append (text, "\r\n");
		}
	}

	return g_string_free_to_bytes (text);
}

typedef enum {
	LOG_OFF,
	LOG_STRUCTURAL,
	LOG_ALL
} LoggingMode;

typedef struct {
	GPtrArray *streams;
	GMimeContentStorage storage;
	LoggingMode logging;
	guint issue_limit;
} ValidationData;

static void
count_issue (const GMimeComplianceIssue *issue, gpointer data)
{
	(*(guint *) data)++;
}

static GMimeMessage *
parse_fixture (ValidationData *test, GMimeStream *stream, guint *issues, GError **error)
{
	GMimeParser *parser;
	GMimeMessage *message;
	GMimeComplianceOptions *options;

	if (g_mime_stream_reset (stream) < 0)
		g_error ("Cannot rewind memory stream");
	parser = g_mime_parser_new_with_stream_and_storage (stream, GMIME_FORMAT_MESSAGE, test->storage, error);
	if (!parser)
		return NULL;
	options = g_mime_parser_get_compliance_options (parser);
	g_mime_compliance_options_set_max_issues_per_violation (options, test->issue_limit);
	g_mime_compliance_options_set_enabled_validators (options, test->logging == LOG_STRUCTURAL ?
							  GMIME_COMPLIANCE_VALIDATOR_NONE : GMIME_COMPLIANCE_VALIDATOR_ALL);
	if (test->logging != LOG_OFF)
		g_mime_parser_set_compliance_logger (parser, count_issue, issues, NULL);
	message = g_mime_parser_construct_message (parser, NULL, error);
	g_object_unref (parser);
	return message;
}

static guint64
parse_validated (gpointer data)
{
	ValidationData *test = data;
	guint64 result = 0;
	guint i;

	for (i = 0; i < test->streams->len; i++) {
		GError *error = NULL;
		guint issues = 0;
		GMimeMessage *message = parse_fixture (test, test->streams->pdata[i], &issues, &error);

		benchmark_check (message != NULL, "parsing validation fixture", &error);
		result += issues + g_mime_header_list_get_count (g_mime_entity_get_header_list (GMIME_ENTITY (message)));
		g_object_unref (message);
	}
	return result;
}

static void
add_fixture (ValidationData *test, GBytes *bytes)
{
	const char *data = g_bytes_get_data (bytes, NULL);
	g_ptr_array_add (test->streams, g_mime_stream_mem_new_with_buffer (data, g_bytes_get_size (bytes)));
}

/* MKCORP01: little-endian message count followed by length-prefixed messages. */
static guint32
corpus_uint32 (const guint8 **cursor, const guint8 *end)
{
	guint32 value;

	if ((gsize) (end - *cursor) < sizeof (value))
		g_error ("Truncated packed corpus");
	memcpy (&value, *cursor, sizeof (value));
	*cursor += sizeof (value);
	return GUINT32_FROM_LE (value);
}

static gsize
load_release_corpus (ValidationData *test, const char *dir, const char *file)
{
	GBytes *packed = benchmark_load (dir, file);
	gsize length, accepted_bytes = 0;
	const guint8 *cursor = g_bytes_get_data (packed, &length), *end;
	guint count, i;

	if (length < 8 || memcmp (cursor, "MKCORP01", 8))
		g_error ("%s: invalid packed corpus signature", file);
	end = cursor + length;
	cursor += 8;
	count = corpus_uint32 (&cursor, end);
	if (!count || count > 100000)
		g_error ("%s: invalid packed corpus count %u", file, count);
	for (i = 0; i < count; i++) {
		guint32 size = corpus_uint32 (&cursor, end);
		GMimeStream *stream;
		GMimeMessage *message;
		GError *error = NULL;
		guint issues = 0;

		if (size > (gsize) (end - cursor))
			g_error ("%s: truncated message %u", file, i);
		stream = g_mime_stream_mem_new_with_buffer ((const char *) cursor, size);
		cursor += size;
		message = parse_fixture (test, stream, &issues, &error);
		if (!message && (g_error_matches (error, GMIME_ERROR, GMIME_ERROR_PARSE_ERROR) ||
				 g_error_matches (error, GMIME_ERROR, GMIME_ERROR_HEADER_TOO_LONG))) {
			g_clear_error (&error);
			g_object_unref (stream);
			continue;
		}
		benchmark_check (message != NULL, "preflighting packed message", &error);
		g_object_unref (message);
		g_ptr_array_add (test->streams, stream);
		accepted_bytes += size;
	}
	if (cursor != end || !test->streams->len)
		g_error ("%s: trailing data or no parseable messages", file);
	g_printerr ("%s: benchmarking %u messages; excluded %u\n", file, test->streams->len, count - test->streams->len);
	g_bytes_unref (packed);
	return accepted_bytes;
}

static void
benchmark_validation (const char *datadir, gboolean release)
{
	static const char *headers[] = {
		"Short", "Folded", "LongLine", "Addresses", "Parameters", "Utf8", "Invalid", "HeaderStress", "StarTrek"
	};
	static const char *release_cases[] = {
		"Utf8", "Body-LineWidth-32", "Body-LineWidth-998", "QP-Utf8-CRLF", "enron-crlf", "attachments-crlf"
	};
	static const char *modes[] = { "Off", "Structural", "All" };
	const char *corpus = g_getenv ("MIMEKIT_BENCHMARK_CORPUS");
	guint i, mode;

	for (i = 0; i < (release ? G_N_ELEMENTS (release_cases) : G_N_ELEMENTS (headers)); i++) {
		const char *scenario = release ? release_cases[i] : headers[i];
		ValidationData test = { NULL, release ? GMIME_CONTENT_STORAGE_SOURCE : GMIME_CONTENT_STORAGE_MEMORY,
					LOG_OFF, release ? 1 : 0 };
		char *file = NULL;
		gsize bytes = 0;

		if (release && g_str_has_suffix (scenario, "-crlf")) {
			char *path;
			gboolean exists;

			if (!corpus || !*corpus)
				continue;
			file = g_strconcat (scenario, ".bin", NULL);
			path = g_build_filename (corpus, file, NULL);
			exists = g_file_test (path, G_FILE_TEST_IS_REGULAR);
			g_free (path);
			if (!exists) {
				g_free (file);
				continue;
			}
		}
		for (mode = LOG_OFF; mode <= LOG_ALL; mode++) {
			char *name;

			if (release && mode == LOG_STRUCTURAL)
				continue;
			name = g_strdup_printf ("%s/%s/%s", release ? "ReleaseParser" : "HeaderByteValidation",
						scenario, modes[mode]);
			if (benchmark_selected (name)) {
				if (!test.streams) {
					test.streams = g_ptr_array_new_with_free_func (g_object_unref);
					if (file) {
						bytes = load_release_corpus (&test, corpus, file);
					} else {
						GBytes *input = release ? release_message (datadir, scenario) :
							header_message (datadir, scenario);
						bytes = g_bytes_get_size (input);
						add_fixture (&test, input);
						g_bytes_unref (input);
					}
				}
				test.logging = mode;
				benchmark_run (name, bytes, parse_validated, &test);
			}
			g_free (name);
		}
		if (test.streams)
			g_ptr_array_unref (test.streams);
		g_free (file);
	}
}

typedef struct {
	GPtrArray *values;
	gsize bytes;
	guint64 lines, flags;
	gboolean detect;
} ScannerData;

/* Preserve the following-header lookahead, including bytes that must not
 * be attributed to the header being scanned. Only count actual value bytes. */
static void
add_scan_value (ScannerData *test, const guint8 *value, gsize length)
{
	static const char next[] = "Next: \0\xc3\xbf";
	GString *input = g_string_sized_new (length + 80);
	guint flags = 0;
	gsize i;

	g_string_append_len (input, (const char *) value, length);
	for (i = 0; i < length; i++) {
		if (!value[i])
			flags |= 1;
		if (value[i] & 0x80)
			flags |= 2;
		if (value[i] == '\n')
			test->lines++;
	}
	if (!length || value[length - 1] != '\n') {
		g_string_append_c (input, '\n');
		test->lines++;
	}
	test->bytes += input->len;
	test->flags += flags;
	g_string_append_len (input, next, sizeof (next) - 1);
	append_run (input, 'x', 64);
	g_string_append (input, "\r\n\n");
	g_ptr_array_add (test->values, g_string_free_to_bytes (input));
}

static void
setup_scanner (ScannerData *test, const char *datadir, const char *scenario)
{
	GString *value;
	guint i;

	if (!strcmp (scenario, "Corpus") || !strcmp (scenario, "HeaderStress")) {
		GBytes *bytes = header_message (datadir, !strcmp (scenario, "Corpus") ? "StarTrek" : scenario);
		GMimeStream *stream = g_mime_stream_mem_new_with_buffer (g_bytes_get_data (bytes, NULL), g_bytes_get_size (bytes));
		GMimeParser *parser = g_mime_parser_new_with_stream (stream);
		GError *error = NULL;
		GMimeMessage *message = g_mime_parser_construct_message (parser, NULL, &error);
		GMimeHeaderList *headers;
		int h;

		benchmark_check (message != NULL, "collecting raw header values", &error);
		headers = g_mime_entity_get_header_list (GMIME_ENTITY (message));
		for (h = 0; h < g_mime_header_list_get_count (headers); h++) {
			GMimeHeader *header = g_mime_header_list_get_header_at (headers, h);
			GBytes *raw = g_mime_header_get_raw_value (header);

			if (raw) {
				add_scan_value (test, g_bytes_get_data (raw, NULL), g_bytes_get_size (raw));
				g_bytes_unref (raw);
			}
		}
		g_object_unref (message);
		g_object_unref (parser);
		g_object_unref (stream);
		g_bytes_unref (bytes);
		return;
	}

	value = g_string_new (NULL);
	if (!strcmp (scenario, "Short")) {
		g_string_append (value, " hello\r\n");
	} else if (!strcmp (scenario, "DenseFolds")) {
		for (i = 0; i < 64; i++)
			g_string_append (value, " a\r\n\tb\r\n");
	} else {
		guint lines = !strcmp (scenario, "Folded") ? 16 : 1;
		guint width = !strcmp (scenario, "LongLine") ? 4096 : 75;

		for (i = 0; i < lines; i++) {
			g_string_append_c (value, ' ');
			append_run (value, 'x', width);
			g_string_append (value, "\r\n");
		}
	}
	add_scan_value (test, (const guint8 *) value->str, value->len);
	g_string_free (value, TRUE);
}

static guint64
scan_header_values (gpointer data)
{
	ScannerData *test = data;
	guint64 result = 0;
	guint i;

	for (i = 0; i < test->values->len; i++) {
		gsize size;
		const guint8 *start = g_bytes_get_data (test->values->pdata[i], &size);
		const guint8 *p = start, *end = start + size;
		guint flags = 0, lines = 0;

		do {
			const guint8 *lf;

			/* Same two paths as GMimeReader's header loop. */
			if (test->detect && flags != 3) {
				GMimeReaderLineScan scan = _g_mime_reader_scan_line (p, end - p, !(flags & 1), !(flags & 2));
				if (scan.null_index != G_MAXSIZE)
					flags |= 1;
				if (scan.eight_bit_index != G_MAXSIZE)
					flags |= 2;
				lf = p + scan.length;
			} else {
				lf = memchr (p, '\n', end - p);
			}
			if (!lf || lf == end)
				g_error ("Missing header lookahead");
			p = lf + 1;
			lines++;
		} while (p < end && (*p == ' ' || *p == '\t'));
		result += (guint64) (p - start) + lines + flags;
	}
	return result;
}

static void
benchmark_header_scanner (const char *datadir)
{
	static const char *cases[] = { "Short", "Typical", "Folded", "DenseFolds", "LongLine", "Corpus", "HeaderStress" };
	guint i, detect;

	for (i = 0; i < G_N_ELEMENTS (cases); i++) {
		ScannerData test = { 0 };

		for (detect = 0; detect < 2; detect++) {
			char *name = g_strdup_printf ("HeaderValueScanner/%s/%s", cases[i], detect ? "DetectBytes" : "Lines");
			if (benchmark_selected (name)) {
				if (!test.values) {
					test.values = g_ptr_array_new_with_free_func ((GDestroyNotify) g_bytes_unref);
					setup_scanner (&test, datadir, cases[i]);
				}
				test.detect = detect;
				if (!test.bytes || scan_header_values (&test) != test.bytes + test.lines + (detect ? test.flags : 0))
					g_error ("%s: incorrect header scan result", name);
				benchmark_run (name, test.bytes, scan_header_values, &test);
			}
			g_free (name);
		}
		if (test.values)
			g_ptr_array_unref (test.values);
	}
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
		{ "ContentLengthMbox", "mbox/input/content-length.mbox", 29 },
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
							bytes = benchmark_load (i == 2 ? testdatadir : datadir, inputs[i].file);
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

	benchmark_validation (datadir, FALSE);
	benchmark_header_scanner (datadir);
	benchmark_validation (datadir, TRUE);
	benchmark_prepare (datadir, testdatadir);
}
