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


#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>

#include <gmime/gmime.h>

#include "testsuite.h"

extern int verbose;

#define d(x) 
#define v(x) if (verbose > 3) x;

static GByteArray *
read_all_bytes (const char *path, gboolean is_text)
{
	GMimeStream *filtered, *stream, *mem;
	GMimeFilter *filter;
	GByteArray *buffer;
	
	buffer = g_byte_array_new ();
	
	if (!(stream = g_mime_stream_fs_open (path, O_RDONLY, 0644, NULL)))
		return buffer;
	
	mem = g_mime_stream_mem_new_with_byte_array (buffer);
	g_mime_stream_mem_set_owner ((GMimeStreamMem *) mem, FALSE);
	
	if (is_text) {
		filtered = g_mime_stream_filter_new (mem);
		filter = g_mime_filter_dos2unix_new (FALSE);
		g_mime_stream_filter_add ((GMimeStreamFilter *) filtered, filter);
		g_object_unref (filter);
		g_object_unref (mem);
	} else {
		filtered = mem;
	}
	
	g_mime_stream_write_to_stream (stream, filtered);
	g_mime_stream_flush (filtered);
	g_object_unref (filtered);
	g_object_unref (stream);
	
	return buffer;
}

static void
test_content_disposition (GMimePart *mime_part, const char *filename)
{
	const char *what = "Content-Disposition";
	const char *value;
	
	testsuite_check ("%s", what);
	
	if (!(value = g_mime_part_get_filename (mime_part))) {
		testsuite_check_failed ("%s failed: filename and name parameters were NULL", what);
		return;
	}
	
	if (strcmp (value, filename) != 0) {
		testsuite_check_failed ("%s failed: filenames do not match: %s", what, value);
		return;
	}
	
	if (!g_mime_part_is_attachment (mime_part)) {
		testsuite_check_failed ("%s failed: mime part is not an attachment", what);
		return;
	}
	
	testsuite_check_passed ();
}

static void
test_content_description (GMimePart *mime_part, const char *description)
{
	const char *what = "Content-Description";
	const char *value;
	
	testsuite_check ("%s", what);
	
	if (!(value = g_mime_part_get_content_description (mime_part))) {
		testsuite_check_failed ("%s failed: g_mime_part_get_content_description() returned NULL", what);
		return;
	}
	
	if (strcmp (value, description) != 0) {
		testsuite_check_failed ("%s failed: descriptions do not match: %s", what, value);
		return;
	}
	
	testsuite_check_passed ();
}

static void
test_content_location (GMimePart *mime_part, const char *location)
{
	const char *what = "Content-Location";
	const char *value;
	
	testsuite_check ("%s", what);
	
	if (!(value = g_mime_part_get_content_location (mime_part))) {
		testsuite_check_failed ("%s failed: g_mime_part_get_content_location() returned NULL", what);
		return;
	}
	
	if (strcmp (value, location) != 0) {
		testsuite_check_failed ("%s failed: locationss do not match: %s", what, value);
		return;
	}
	
	testsuite_check_passed ();
}

static void
test_content_id (GMimePart *mime_part, const char *id)
{
	const char *what = "Content-Id";
	const char *value;
	
	testsuite_check ("%s", what);
	
	if (!(value = g_mime_part_get_content_id (mime_part))) {
		testsuite_check_failed ("%s failed: g_mime_part_get_content_id() returned NULL", what);
		return;
	}
	
	if (strcmp (value, id) != 0) {
		testsuite_check_failed ("%s failed: ids do not match: %s", what, value);
		return;
	}
	
	testsuite_check_passed ();
}

static void
test_content_md5 (GMimePart *mime_part, const char *md5sum)
{
	const char *what = "Content-Md5";
	GError *error = NULL;
	const char *value;
	
	testsuite_check ("%s", what);
	
	if (!(value = g_mime_part_get_content_md5 (mime_part))) {
		testsuite_check_failed ("%s failed: g_mime_part_get_content_md5() returned NULL", what);
		return;
	}
	
	if (strcmp (value, md5sum) != 0) {
		testsuite_check_failed ("%s failed: md5's do not match: %s", what, value);
		return;
	}
	
	if (!g_mime_part_verify_content_md5 (mime_part, &error)) {
		g_assert_no_error (error);
		testsuite_check_failed ("%s failed: md5sum did not verify", what);
		return;
	}
	
	testsuite_check_passed ();
}

static void
set_computed_content_md5 (GMimePart *part)
{
	GError *error = NULL;
	char *digest = g_mime_part_compute_content_md5 (part, &error);

	g_assert_no_error (error);
	g_assert_nonnull (digest);
	g_mime_part_set_content_md5 (part, digest);
	g_free (digest);
}

static void
test_content_md5_vectors (void)
{
	static const struct {
		gboolean text;
		const char *input;
		size_t length;
		GMimeContentEncoding encoding;
		const char *md5;
	} cases[] = {
		{ FALSE, "", 0, GMIME_CONTENT_ENCODING_UNSPECIFIED, "1B2M2Y8AsgTpgAmY7PhCfg==" },
		{ FALSE, "abc", 3, GMIME_CONTENT_ENCODING_UNSPECIFIED, "kAFQmDzST7DWlj99KOF/cg==" },
		{ FALSE, "a\n", 2, GMIME_CONTENT_ENCODING_BINARY, "YLcl8QychccNl4gN/oGRsw==" },
		{ FALSE, "\0\xff" "ABC\n", 6, GMIME_CONTENT_ENCODING_UNSPECIFIED, "ZCeOppyvk3dBJtrxlivHRg==" },
		{ TRUE, "Hello\nworld\n", 12, GMIME_CONTENT_ENCODING_UNSPECIFIED, "0D25laZp7CmXkep2HGh5kg==" },
		{ TRUE, "Hello\r\nworld\r\n", 14, GMIME_CONTENT_ENCODING_UNSPECIFIED, "0D25laZp7CmXkep2HGh5kg==" },
		{ TRUE, "SGVsbG8Kd29ybGQK", 16, GMIME_CONTENT_ENCODING_BASE64, "0D25laZp7CmXkep2HGh5kg==" },
		{ TRUE, "Hello=0Aworld=0A", 16, GMIME_CONTENT_ENCODING_QUOTEDPRINTABLE, "0D25laZp7CmXkep2HGh5kg==" },
		{ TRUE, "\r", 1, GMIME_CONTENT_ENCODING_UNSPECIFIED, "3Lm+L2BOXfkd65ZZvtR0jQ==" },
		{ TRUE, "\n", 1, GMIME_CONTENT_ENCODING_UNSPECIFIED, "gQUbzCzxvt83giSwqT4odw==" },
		{ TRUE, "a\r\r\nb\rc\nd", 9, GMIME_CONTENT_ENCODING_UNSPECIFIED, "UKY+TF6AtMG9Xqw4mHNwDw==" },
		{ TRUE, "=0D", 3, GMIME_CONTENT_ENCODING_QUOTEDPRINTABLE, "3Lm+L2BOXfkd65ZZvtR0jQ==" },
		{ TRUE, "DQ==", 4, GMIME_CONTENT_ENCODING_BASE64, "3Lm+L2BOXfkd65ZZvtR0jQ==" }
	};
	GMimeContent *content;
	GMimeStream *stream, *input;
	GMimePart *part;
	GError *error = NULL;
	const char *header;
	char *digest;
	guint i, onebyte;

	for (i = 0; i < G_N_ELEMENTS (cases); i++) {
		for (onebyte = 0; onebyte < 2; onebyte++) {
			part = g_mime_part_new_with_type (cases[i].text ? "text" : "application",
							 cases[i].text ? "plain" : "octet-stream");
			stream = g_mime_stream_mem_new_with_buffer (cases[i].input, cases[i].length);
			input = onebyte ? test_stream_onebyte_new (stream) : g_object_ref (stream);
			content = g_mime_content_new_with_stream (input, cases[i].encoding);
			g_mime_part_set_content (part, content);
			g_object_unref (content);
			g_object_unref (input);
			g_object_unref (stream);

			digest = g_mime_part_compute_content_md5 (part, &error);
			g_assert_no_error (error);
			g_assert_cmpstr (digest, ==, cases[i].md5);
			g_assert_null (g_mime_part_get_content_md5 (part));
			g_assert_null (g_mime_entity_get_header (GMIME_ENTITY (part), "Content-Md5"));
			g_assert_true (g_mime_part_get_content (part) == content);
			g_assert_cmpint (g_mime_content_get_encoding (content), ==, cases[i].encoding);
			g_assert_cmpint (g_mime_stream_tell (stream), ==, cases[i].length);
			g_mime_part_set_content_md5 (part, digest);
			g_free (digest);
			test_content_md5 (part, cases[i].md5);

			testsuite_check ("Content-Md5 header matches the unwrapped digest (%u)", i);
			header = g_mime_entity_get_header ((GMimeEntity *) part, "Content-Md5");
			if (!g_strcmp0 (header, cases[i].md5))
				testsuite_check_passed ();
			else
				testsuite_check_failed ("Content-Md5 header does not match the digest");

			g_mime_part_set_content_md5 (part, "AAAAAAAAAAAAAAAAAAAAAA==");
			testsuite_check ("Content-Md5 rejects an incorrect digest (%u)", i);
			if (!g_mime_part_verify_content_md5 (part, &error))
				testsuite_check_passed ();
			else
				testsuite_check_failed ("Incorrect Content-Md5 verified");
			g_assert_no_error (error);

			set_computed_content_md5 (part);
			test_content_md5 (part, cases[i].md5);
			g_mime_part_set_content_md5 (part, NULL);
			g_assert_null (g_mime_part_get_content_md5 (part));
			g_assert_null (g_mime_entity_get_header (GMIME_ENTITY (part), "Content-Md5"));
			g_assert_false (g_mime_part_verify_content_md5 (part, &error));
			g_assert_no_error (error);
			g_object_unref (part);
		}
	}
}

static void
test_content_md5_assignment (void)
{
	static const char digest[] = "kAFQmDzST7DWlj99KOF/cg==";
	GMimePart *part = g_mime_part_new ();
	GMimeEntity *entity = GMIME_ENTITY (part);
	GMimeHeaderList *headers = g_mime_entity_get_header_list (entity);
	GMimeHeader *header;
	GError *error = NULL;
	char *value, *raw;

	testsuite_check ("Content-Md5 assignment, Unicode trimming, aliasing and duplicate removal");
	value = g_strconcat ("\302\205\t\v ", digest, "\r\n\343\200\200", NULL);
	g_mime_part_set_content_md5 (part, value);
	g_free (value);
	g_assert_cmpstr (g_mime_part_get_content_md5 (part), ==, digest);
	g_assert_cmpstr (g_mime_entity_get_header (entity, "Content-Md5"), ==, digest);
	g_assert_false (g_mime_part_verify_content_md5 (part, &error));
	g_assert_no_error (error);
	header = g_mime_header_list_get_header (headers, "Content-Md5");
	raw = g_strdup (g_mime_header_get_raw_value (header));
	g_mime_part_set_content_md5 (part, g_mime_part_get_content_md5 (part));
	g_assert_cmpstr (g_mime_header_get_raw_value (header), ==, raw);
	g_free (raw);
	value = g_strconcat ("x", digest, NULL);
	g_mime_part_set_content_md5 (part, value);
	g_free (value);
	g_mime_part_set_content_md5 (part, g_mime_part_get_content_md5 (part) + 1);
	g_assert_cmpstr (g_mime_part_get_content_md5 (part), ==, digest);

	g_mime_entity_set_header (entity, "Content-Md5", "\302\240 arbitrary digest \342\200\257", NULL);
	g_assert_cmpstr (g_mime_part_get_content_md5 (part), ==, "arbitrary digest");
	g_mime_header_list_append (headers, "Content-Md5", digest, NULL);
	g_mime_part_set_content_md5 (part, NULL);
	g_assert_null (g_mime_part_get_content_md5 (part));
	g_assert_false (g_mime_header_list_contains (headers, "Content-Md5"));
	g_mime_part_set_content_md5 (part, "\v\302\205\343\200\200");
	g_assert_cmpstr (g_mime_part_get_content_md5 (part), ==, "");
	g_assert_nonnull (g_mime_entity_get_header (entity, "Content-Md5"));
	g_mime_part_set_content_md5 (part, NULL);
	g_mime_part_set_content_md5 (part, NULL);
	g_assert_null (g_mime_part_get_content_md5 (part));
	g_object_unref (part);
	testsuite_check_passed ();
}

static void
test_content_description_assignment (void)
{
	static const char *whitespace[] = {
		"\t", "\n", "\v", "\f", "\r", " ", "\302\205", "\302\240",
		"\341\232\200", "\342\200\200", "\342\200\201", "\342\200\202",
		"\342\200\203", "\342\200\204", "\342\200\205", "\342\200\206",
		"\342\200\207", "\342\200\210", "\342\200\211", "\342\200\212",
		"\342\200\250", "\342\200\251", "\342\200\257", "\342\201\237", "\343\200\200"
	};
	GMimePart *part = g_mime_part_new ();
	GMimeEntity *entity = GMIME_ENTITY (part);
	GMimeHeaderList *headers = g_mime_entity_get_header_list (entity);
	GMimeHeader *header, *duplicate;
	char *raw, *value;
	int count = g_mime_header_list_get_count (headers);
	guint i;

	testsuite_check ("Content-Description Unicode trimming, nullable assignment and aliasing");
	g_assert_null (g_mime_part_get_content_description (part));
	for (i = 0; i < G_N_ELEMENTS (whitespace); i++) {
		value = g_strconcat (whitespace[i], "r\303\251sum\303\251", whitespace[i], NULL);
		g_mime_part_set_content_description (part, value);
		g_assert_cmpstr (g_mime_part_get_content_description (part), ==, "r\303\251sum\303\251");
		g_free (value);
	}
	g_mime_part_set_content_description (part, "\342\200\213 kept \357\273\277");
	g_assert_cmpstr (g_mime_part_get_content_description (part), ==, "\342\200\213 kept \357\273\277");
	g_mime_part_set_content_description (part, "xdescription");
	g_mime_part_set_content_description (part, g_mime_part_get_content_description (part) + 1);
	g_assert_cmpstr (g_mime_part_get_content_description (part), ==, "description");
	g_mime_part_set_content_description (part, "\v\302\205\343\200\200");
	g_assert_cmpstr (g_mime_part_get_content_description (part), ==, "");
	g_assert_nonnull (g_mime_entity_get_header (entity, "Content-Description"));
	g_mime_part_set_content_description (part, NULL);
	g_mime_part_set_content_description (part, NULL);
	g_assert_null (g_mime_part_get_content_description (part));
	g_assert_false (g_mime_header_list_contains (headers, "Content-Description"));
	testsuite_check_passed ();

	testsuite_check ("Content-Description lazy first-header reads and unchanged raw assignments");
	g_mime_header_list_append (headers, "Content-Description", "first", NULL);
	header = g_mime_header_list_get_header (headers, "Content-Description");
	g_mime_header_set_raw_value (header, " first\t \n");
	g_mime_header_list_append (headers, "Content-Description", "second", NULL);
	duplicate = g_mime_header_list_get_header_at (headers, count + 1);
	raw = g_strdup (g_mime_header_get_raw_value (header));
	g_assert_cmpstr (g_mime_part_get_content_description (part), ==, "first");
	g_mime_part_set_content_description (part, g_mime_part_get_content_description (part));
	g_assert_cmpstr (g_mime_header_get_raw_value (header), ==, raw);
	g_assert_cmpint (g_mime_header_list_get_count (headers), ==, count + 2);
	g_mime_header_set_value (duplicate, NULL, "changed duplicate", NULL);
	g_assert_cmpstr (g_mime_part_get_content_description (part), ==, "first");
	g_mime_header_list_remove_at (headers, count);
	g_assert_cmpstr (g_mime_part_get_content_description (part), ==, "changed duplicate");
	g_free (raw);
	testsuite_check_passed ();

	testsuite_check ("Content-Description uncached assignment, duplicate removal and cache invalidation");
	g_mime_header_list_append (headers, "Content-Description", "third", NULL);
	g_mime_part_set_content_description (part, "changed duplicate");
	g_assert_cmpint (g_mime_header_list_get_count (headers), ==, count + 1);
	g_mime_header_list_append (headers, "Content-Description", "fourth", NULL);
	g_mime_part_set_content_description (part, NULL);
	g_assert_cmpint (g_mime_header_list_get_count (headers), ==, count);
	g_mime_header_list_append (headers, "Content-Description", " \302\240header value\342\200\257 ", NULL);
	g_assert_cmpstr (g_mime_part_get_content_description (part), ==, "header value");
	g_mime_header_list_set (headers, "X-Unrelated", "unchanged cache", NULL);
	g_mime_part_set_content_description (part, "header value");
	g_mime_header_list_clear (headers);
	g_assert_null (g_mime_part_get_content_description (part));
	g_mime_header_list_append (headers, "Content-Description", "", NULL);
	g_assert_cmpstr (g_mime_part_get_content_description (part), ==, "");
	g_object_unref (part);
	testsuite_check_passed ();
}

static void
test_content_encoding_assignment (void)
{
	GMimePart *part = g_mime_part_new ();
	GMimeEntity *entity = GMIME_ENTITY (part);
	GMimeHeaderList *headers = g_mime_entity_get_header_list (entity);
	GMimeHeader *header, *duplicate;
	GMimeContentEncoding encoding;
	char *raw;
	const char *value;
	int count = g_mime_header_list_get_count (headers);

	testsuite_check ("Transfer-encoding cached assignments preserve raw values and duplicates");
	g_assert_cmpint (g_mime_part_get_content_encoding (part), ==, GMIME_CONTENT_ENCODING_UNSPECIFIED);
	for (encoding = GMIME_CONTENT_ENCODING_UNSPECIFIED; encoding <= GMIME_CONTENT_ENCODING_UUENCODE; encoding++) {
		value = g_mime_content_encoding_to_string (encoding);
		g_mime_header_list_remove_all (headers, "Content-Transfer-Encoding");
		g_mime_header_list_append (headers, "Content-Transfer-Encoding", value ? value : "unknown", NULL);
		header = g_mime_header_list_get_header (headers, "Content-Transfer-Encoding");
		raw = g_strdup_printf (" %s\t \n", value ? value : "unknown");
		g_mime_header_set_raw_value (header, raw);
		g_mime_header_list_append (headers, "Content-Transfer-Encoding", "binary", NULL);
		g_assert_cmpint (g_mime_part_get_content_encoding (part), ==, encoding);
		g_mime_part_set_content_encoding (part, encoding);
		g_assert_cmpint (g_mime_header_list_get_count (headers), ==, count + 2);
		g_assert_cmpstr (g_mime_header_get_raw_value (header), ==, raw);
		g_free (raw);

		g_mime_header_list_append (headers, "Content-Transfer-Encoding", "7bit", NULL);
		g_mime_part_set_content_encoding (part, encoding);
		g_assert_cmpint (g_mime_part_get_content_encoding (part), ==, encoding);
		g_assert_cmpint (g_mime_header_list_get_count (headers), ==, count + (value ? 1 : 0));
		g_assert_cmpstr (g_mime_entity_get_header (entity, "Content-Transfer-Encoding"), ==, value);
	}
	testsuite_check_passed ();

	testsuite_check ("Transfer-encoding first-header selection and matching event invalidation");
	g_mime_header_list_set (headers, "Content-Transfer-Encoding", "7-bit", NULL);
	g_mime_header_list_append (headers, "Content-Transfer-Encoding", "8bit", NULL);
	duplicate = g_mime_header_list_get_header_at (headers, count + 1);
	g_assert_cmpint (g_mime_part_get_content_encoding (part), ==, GMIME_CONTENT_ENCODING_7BIT);
	g_mime_header_set_value (duplicate, NULL, "base64", NULL);
	g_assert_cmpint (g_mime_part_get_content_encoding (part), ==, GMIME_CONTENT_ENCODING_7BIT);
	g_mime_header_list_remove_at (headers, count);
	g_assert_cmpint (g_mime_part_get_content_encoding (part), ==, GMIME_CONTENT_ENCODING_BASE64);
	g_mime_header_set_raw_value (duplicate, " 8-BIT;suffix\n");
	g_assert_cmpint (g_mime_part_get_content_encoding (part), ==, GMIME_CONTENT_ENCODING_8BIT);
	g_mime_header_list_clear (headers);
	g_assert_cmpint (g_mime_part_get_content_encoding (part), ==, GMIME_CONTENT_ENCODING_UNSPECIFIED);
	g_object_unref (part);
	testsuite_check_passed ();
}

static void
test_content_encoding_invalid_assignment (void)
{
	static const int invalid[] = { -1, GMIME_CONTENT_ENCODING_UUENCODE + 1, G_MAXINT };
	GMimePart *part;
	GMimeHeaderList *headers;
	GMimeHeader *header;
	char *raw;
	guint loaded, i;

	testsuite_check ("Invalid transfer encodings leave raw headers and cache state unchanged");
	for (loaded = 0; loaded < 2; loaded++) {
		for (i = 0; i < G_N_ELEMENTS (invalid); i++) {
			part = g_mime_part_new ();
			headers = g_mime_entity_get_header_list (GMIME_ENTITY (part));
			g_mime_header_list_append (headers, "Content-Transfer-Encoding", "BASE64", NULL);
			g_mime_header_list_append (headers, "Content-Transfer-Encoding", "8bit", NULL);
			header = g_mime_header_list_get_header (headers, "Content-Transfer-Encoding");
			raw = g_strdup (g_mime_header_get_raw_value (header));
			if (loaded)
				g_assert_cmpint (g_mime_part_get_content_encoding (part), ==, GMIME_CONTENT_ENCODING_BASE64);
			g_test_expect_message ("gmime", G_LOG_LEVEL_CRITICAL, "*encoding >=*encoding <=*");
			g_mime_part_set_content_encoding (part, (GMimeContentEncoding) invalid[i]);
			g_test_assert_expected_messages ();
			g_assert_cmpstr (g_mime_header_get_raw_value (header), ==, raw);
			g_assert_cmpint (g_mime_header_list_get_count (headers), ==, 3);
			g_mime_part_set_content_encoding (part, GMIME_CONTENT_ENCODING_BASE64);
			g_assert_cmpint (g_mime_header_list_get_count (headers), ==, loaded ? 3 : 2);
			g_assert_cmpint (g_mime_part_get_content_encoding (part), ==, GMIME_CONTENT_ENCODING_BASE64);
			g_free (raw);
			g_object_unref (part);
		}
	}
	testsuite_check_passed ();
}

static void
test_content_encoding_consumers (void)
{
	GMimePart *part = g_mime_part_new_with_type ("text", "plain");
	GMimeEntity *entity = GMIME_ENTITY (part);
	GMimeHeaderList *headers = g_mime_entity_get_header_list (entity);
	GMimeFormatOptions *options = g_mime_format_options_new ();
	GMimeStream *source = g_mime_stream_mem_new_with_buffer ("Hello", 5);
	GMimeContent *content = g_mime_content_new_with_stream (source, GMIME_CONTENT_ENCODING_UNSPECIFIED);
	GMimeStream *output = g_mime_stream_mem_new ();
	GByteArray *bytes = g_mime_stream_mem_get_byte_array (GMIME_STREAM_MEM (output));
	GMimeHeader *header;
	char *raw;

	testsuite_check ("Serialization and legacy encoding resolve the first raw transfer-encoding header");
	g_mime_part_set_content (part, content);
	g_mime_header_list_append (headers, "Content-Transfer-Encoding", "BASE64", NULL);
	g_mime_header_list_append (headers, "Content-Transfer-Encoding", "binary", NULL);
	header = g_mime_header_list_get_header (headers, "Content-Transfer-Encoding");
	raw = g_strdup (g_mime_header_get_raw_value (header));
	g_mime_format_options_set_newline_format (options, GMIME_NEWLINE_FORMAT_UNIX);
	g_assert_cmpint (g_mime_entity_write_content_to_stream (entity, options, output), >=, 0);
	g_assert_cmpmem (bytes->data, bytes->len, "SGVsbG8=\n", 9);
	g_assert_cmpstr (g_mime_header_get_raw_value (header), ==, raw);
	g_mime_header_set_raw_value (header, raw);
	g_mime_entity_encode (entity, GMIME_ENCODING_CONSTRAINT_7BIT);
	g_assert_cmpint (g_mime_header_list_get_count (headers), ==, 3);
	g_assert_cmpstr (g_mime_header_get_raw_value (header), ==, raw);
	g_assert_cmpint (g_mime_content_get_encoding (content), ==, GMIME_CONTENT_ENCODING_UNSPECIFIED);
	g_free (raw);
	g_mime_format_options_free (options);
	g_object_unref (output);
	g_object_unref (content);
	g_object_unref (source);
	g_object_unref (part);
	testsuite_check_passed ();
}

static void
test_generated_boundaries (void)
{
	GMimeMultipart *multipart = g_mime_multipart_new ();
	GMimeTextPart *part = g_mime_text_part_new ();
	GMimeEntity *parsed;
	GMimeParser *parser;
	GMimeStream *stream;
	GError *error = NULL;
	const char *boundary;
	guchar *decoded;
	char *encoded, *text;
	gsize length;
	guint i;

	for (i = 0; i < 16; i++) {
		if (i)
			g_mime_multipart_set_boundary (multipart, NULL);
		boundary = g_mime_multipart_get_boundary (multipart);

		testsuite_check ("Generated multipart boundary includes a complete Base64 block (%u)", i);
		if (!boundary || strlen (boundary) != 26 || strncmp (boundary, "=-", 2) ||
		    strcmp (boundary + 24, "==")) {
			testsuite_check_failed ("Expected =- followed by 24 padded Base64 characters");
			continue;
		}

		decoded = g_base64_decode (boundary + 2, &length);
		encoded = g_base64_encode (decoded, length);
		if (length == 16 && !strcmp (encoded, boundary + 2))
			testsuite_check_passed ();
		else
			testsuite_check_failed ("Boundary is not a canonical encoding of 16 bytes");
		g_free (encoded);
		g_free (decoded);
	}

	g_mime_text_part_set_text (part, "boundary payload\n");
	g_mime_multipart_add (multipart, (GMimeEntity *) part);
	g_object_unref (part);
	text = g_mime_entity_to_string ((GMimeEntity *) multipart, NULL);
	stream = g_mime_stream_mem_new_with_buffer (text, strlen (text));
	parser = g_mime_parser_new_with_stream (stream);
	parsed = g_mime_parser_construct_part (parser, NULL, &error);

	testsuite_check ("Generated boundary survives multipart serialization and parsing");
	if (parsed && !error && GMIME_IS_MULTIPART (parsed) &&
	    g_mime_multipart_get_count ((GMimeMultipart *) parsed) == 1 &&
	    !g_strcmp0 (g_mime_multipart_get_boundary ((GMimeMultipart *) parsed),
			g_mime_multipart_get_boundary (multipart)))
		testsuite_check_passed ();
	else
		testsuite_check_failed ("Generated boundary did not round-trip");

	g_clear_error (&error);
	g_clear_object (&parsed);
	g_object_unref (parser);
	g_object_unref (stream);
	g_free (text);

	g_mime_multipart_set_boundary (multipart, "explicit-boundary");
	testsuite_check ("Explicit multipart boundaries remain unchanged");
	if (!strcmp (g_mime_multipart_get_boundary (multipart), "explicit-boundary"))
		testsuite_check_passed ();
	else
		testsuite_check_failed ("Explicit boundary was changed");
	g_object_unref (multipart);
}

static const char *constraints[] = {
	"7bit", "8bit", "binary"
};

static void
test_content_transfer_encoding (GMimePart *mime_part, GMimeEncodingConstraint constraint, GMimeContentEncoding encoding)
{
	const char *what = "Content-Transfer-Encoding";
	GMimeContentEncoding value;
	GError *error = NULL;
	
	testsuite_check ("%s (constraint: %s)", what, constraints[constraint]);
	
	value = g_mime_part_get_best_content_encoding (mime_part, constraint, &error);
	g_assert_no_error (error);
	
	if (value != encoding) {
		testsuite_check_failed ("%s failed: g_mime_part_get_best_encoding() returned %s",
					what, g_mime_content_encoding_to_string (value));
		return;
	}
	
	g_mime_part_set_content_encoding (mime_part, encoding);
	value = g_mime_part_get_content_encoding (mime_part);
	
	if (value != encoding) {
		testsuite_check_failed ("%s failed: g_mime_part_get_content_encoding() returned %s",
					what, g_mime_content_encoding_to_string (value));
		return;
	}
	
	testsuite_check_passed ();
}

static GMimePart *
create_mime_part (const char *type, const char *subtype, const char *datadir, const char *filename)
{
	GMimeContent *content;
	GMimePart *mime_part;
	GMimeStream *stream;
	char *path;
	
	path = g_build_filename (datadir, filename, NULL);
	stream = g_mime_stream_fs_open (path, O_RDONLY, 0644, NULL);
	content = g_mime_content_new_with_stream (stream, GMIME_CONTENT_ENCODING_UNSPECIFIED);
	g_object_unref (stream);
	g_free (path);
	
	mime_part = g_mime_part_new_with_type (type, subtype);
	g_mime_part_set_content (mime_part, content);
	g_object_unref (content);
	
	return mime_part;
}

static void
test_clearing_headers (GMimePart *mime_part)
{
	const char *what = "GMimePart::HeaderList::clear()";
	GMimeHeaderList *list;
	
	testsuite_check ("%s", what);
	
	list = g_mime_entity_get_header_list ((GMimeEntity *) mime_part);
	g_mime_header_list_clear (list);
	
	if (g_mime_entity_get_content_disposition ((GMimeEntity *) mime_part) != NULL) {
		testsuite_check_failed ("%s: failed: Content-Disposition still set", what);
		return;
	}
	
	if (g_mime_part_get_content_description (mime_part) != NULL) {
		testsuite_check_failed ("%s failed: Content-Description still set", what);
		return;
	}
	
	if (g_mime_part_get_content_location (mime_part) != NULL) {
		testsuite_check_failed ("%s failed: Content-Location still set", what);
		return;
	}
	
	if (g_mime_part_get_content_id (mime_part) != NULL) {
		testsuite_check_failed ("%s failed: Content-Id still set", what);
		return;
	}
	
	if (g_mime_part_get_content_md5 (mime_part) != NULL) {
		testsuite_check_failed ("%s failed: Content-Md5 still set", what);
		return;
	}
	
	if (g_mime_part_get_content_encoding (mime_part) != GMIME_CONTENT_ENCODING_UNSPECIFIED) {
		testsuite_check_failed ("%s failed: Content-Transfer-Encoding still set", what);
		return;
	}
	
	testsuite_check_passed ();
}

static void
test_removing_headers (GMimePart *mime_part, const char *filename)
{
	const char *what = "GMimePart::HeaderList::remove()";
	GMimeHeaderList *list;
	const char *value;
	
	testsuite_check ("%s", what);
	
	list = g_mime_entity_get_header_list ((GMimeEntity *) mime_part);
	
	if (!g_mime_header_list_remove (list, "Content-Disposition")) {
		testsuite_check_failed ("%s failed: could not remove Content-Disposition header", what);
		return;
	}
	
	if ((value = g_mime_part_get_filename (mime_part)) == NULL) {
		testsuite_check_failed ("%s failed: filename should still work (via name parameter)", what);
		return;
	}
	
	if (strcmp (value, filename) != 0) {
		testsuite_check_failed ("%s failed: names do not match: %s", what, value);
		return;
	}
	
	if (!g_mime_header_list_remove (list, "Content-Description")) {
		testsuite_check_failed ("%s failed: could not remove Content-Description header", what);
		return;
	}
	
	if (g_mime_part_get_content_description (mime_part) != NULL) {
		testsuite_check_failed ("%s failed: Content-Description still set", what);
		return;
	}
	
	if (!g_mime_header_list_remove (list, "Content-Location")) {
		testsuite_check_failed ("%s failed: could not remove Content-Location header", what);
		return;
	}
	
	if (g_mime_part_get_content_location (mime_part) != NULL) {
		testsuite_check_failed ("%s failed: Content-Location still set", what);
		return;
	}
	
	if (!g_mime_header_list_remove (list, "Content-Id")) {
		testsuite_check_failed ("%s failed: could not remove Content-Id header", what);
		return;
	}
	
	if (g_mime_part_get_content_id (mime_part) != NULL) {
		testsuite_check_failed ("%s failed: Content-Id still set", what);
		return;
	}
	
	if (!g_mime_header_list_remove (list, "Content-Md5")) {
		testsuite_check_failed ("%s failed: could not remove Content-Md5 header", what);
		return;
	}
	
	if (g_mime_part_get_content_md5 (mime_part) != NULL) {
		testsuite_check_failed ("%s failed: Content-Md5 still set", what);
		return;
	}
	
	if (!g_mime_header_list_remove (list, "Content-Transfer-Encoding")) {
		testsuite_check_failed ("%s failed: could not remove Content-Transfer-Encoding header", what);
		return;
	}
	
	if (g_mime_part_get_content_encoding (mime_part) != GMIME_CONTENT_ENCODING_UNSPECIFIED) {
		testsuite_check_failed ("%s failed: Content-Transfer-Encoding still set", what);
		return;
	}
	
	testsuite_check_passed ();
}

static void
test_content_headers (const char *datadir)
{
	const char *description = "I was such a Jurassic Park fanboy as a kid...";
	const char *location = "http://jurassic-park.com/raptors.png";
	const char *id = "raptors@jurassic-park.com";
	const char *md5 = "Av+KQAT/2KFlDYeGHib8kQ==";
	const char *filename = "raptors.png";
	GMimePart *mime_part;
	
	mime_part = create_mime_part ("image", "png", datadir, filename);
	
	g_mime_part_set_filename (mime_part, filename);
	test_content_disposition (mime_part, filename);
	
	g_mime_part_set_content_description (mime_part, description);
	test_content_description (mime_part, description);
	
	g_mime_part_set_content_location (mime_part, location);
	test_content_location (mime_part, location);
	
	g_mime_part_set_content_id (mime_part, id);
	test_content_id (mime_part, id);
	
	set_computed_content_md5 (mime_part);
	test_content_md5 (mime_part, md5);
	
	test_content_transfer_encoding (mime_part, GMIME_ENCODING_CONSTRAINT_BINARY, GMIME_CONTENT_ENCODING_BINARY);
	test_content_transfer_encoding (mime_part, GMIME_ENCODING_CONSTRAINT_8BIT, GMIME_CONTENT_ENCODING_BASE64);
	test_content_transfer_encoding (mime_part, GMIME_ENCODING_CONSTRAINT_7BIT, GMIME_CONTENT_ENCODING_BASE64);
	
	test_clearing_headers (mime_part);
	
	/* re-set the headers */
	g_mime_part_set_filename (mime_part, filename);
	g_mime_part_set_content_description (mime_part, description);
	g_mime_part_set_content_location (mime_part, location);
	g_mime_part_set_content_id (mime_part, id);
	g_mime_part_set_content_md5 (mime_part, md5);
	g_mime_part_set_content_encoding (mime_part, GMIME_CONTENT_ENCODING_BASE64);
	
	test_removing_headers (mime_part, filename);
	
	g_object_unref (mime_part);
}

static void
test_write_to_stream (const char *datadir, const char *output, GMimeContentEncoding encoding)
{
	const char *description = "I was such a Jurassic Park fanboy as a kid...";
	const char *location = "http://jurassic-park.com/raptors.png";
	const char *id = "raptors@jurassic-park.com";
	const char *md5 = "Av+KQAT/2KFlDYeGHib8kQ==";
	const char *filename = "raptors.png";
	const char *what = "GMimePart::write_to_stream()";
	GMimeFormatOptions *options;
	GMimePart *mime_part;
	GMimeStream *stream;
	GByteArray *expected;
	GByteArray *actual;
	char *path;
	
	testsuite_check ("%s (%s)", what, output);
	
	options = g_mime_format_options_clone (NULL);
	g_mime_format_options_set_newline_format (options, GMIME_NEWLINE_FORMAT_UNIX);
	
	path = g_build_filename (datadir, output, NULL);
	expected = read_all_bytes (path, TRUE);
	
	mime_part = create_mime_part ("image", "png", datadir, filename);
	g_mime_part_set_filename (mime_part, filename);
	g_mime_part_set_content_description (mime_part, description);
	g_mime_part_set_content_location (mime_part, location);
	g_mime_part_set_content_id (mime_part, id);
	g_mime_part_set_content_md5 (mime_part, md5);
	
	if (encoding == GMIME_CONTENT_ENCODING_UNSPECIFIED)
		g_mime_entity_encode ((GMimeEntity *) mime_part, GMIME_ENCODING_CONSTRAINT_7BIT);
	else
		g_mime_part_set_content_encoding (mime_part, encoding);
	
	actual = g_byte_array_new ();
	stream = g_mime_stream_mem_new ();
	g_mime_stream_mem_set_byte_array ((GMimeStreamMem *) stream, actual);
	g_mime_entity_write_to_stream ((GMimeEntity *) mime_part, options, stream);
	g_object_unref (mime_part);
	g_object_unref (stream);
	
	if (actual->len != expected->len) {
		testsuite_check_failed ("%s failed: lengths did not match (%u vs %u)", what, actual->len, expected->len);
		if (expected->len == 0) {
			stream = g_mime_stream_fs_open (path, O_WRONLY | O_CREAT, 0644, NULL);
			g_mime_stream_write (stream, (const char *) actual->data, actual->len);
			g_mime_stream_flush (stream);
			g_object_unref (stream);
		}
		goto error;
	}
	
	if (memcmp (actual->data, expected->data, actual->len) != 0) {
		testsuite_check_failed ("%s failed: streams did not match", what);
		goto error;
	}
	
	testsuite_check_passed ();
	
error:
	g_byte_array_free (expected, TRUE);
	g_byte_array_free (actual, TRUE);
	g_free (path);
}

static char *openpgp_data_types[] = {
	"GMIME_OPENPGP_DATA_NONE",
	"GMIME_OPENPGP_DATA_ENCRYPTED",
	"GMIME_OPENPGP_DATA_SIGNED",
	"GMIME_OPENPGP_DATA_PUBLIC_KEY",
	"GMIME_OPENPGP_DATA_PRIVATE_KEY"
};

static void
test_openpgp_data (const char *datadir, const char *filename, GMimeOpenPGPData expected)
{
	const char *what = "GMimePart::get_openpgp_data()";
	GMimeOpenPGPData actual;
	GMimePart *mime_part;
	
	testsuite_check ("%s (%s)", what, filename);
	
	mime_part = create_mime_part ("application", "octet-stream", datadir, filename);
	
	if ((actual = g_mime_part_get_openpgp_data (mime_part)) != expected) {
		testsuite_check_failed ("%s failed: expected=%s; actual=%s", what,
					openpgp_data_types[expected], openpgp_data_types[actual]);
		goto error;
	}
	
	testsuite_check_passed ();
	
error:
	g_object_unref (mime_part);
}

static void
test_text_part (const char *datadir, const char *filename, const char *expected_charset)
{
	const char *what = "GMimeTextPart";
	GByteArray *expected;
	GMimeTextPart *part;
	const char *charset;
	char *text, *path;
	
	testsuite_check ("%s (%s)", what, filename);
	
	path = g_build_filename (datadir, filename, NULL);
	expected = read_all_bytes (path, TRUE);
	g_byte_array_append (expected, (guint8 *) "", 1);
	g_free (path);
	
	part = g_mime_text_part_new ();
	g_mime_text_part_set_text (part, (const char *) expected->data);
	
	charset = g_mime_text_part_get_charset (part);
	if (charset == NULL) {
		testsuite_check_failed ("%s failed: charset is NULL", what);
		goto cleanup;
	}
	
	if (strcmp (charset, expected_charset) != 0) {
		testsuite_check_failed ("%s failed: charsets do not match: expected=%s; actual=%s",
					what, expected_charset, charset);
		goto cleanup;
	}
	
	text = g_mime_text_part_get_text (part);
	if (text == NULL) {
		testsuite_check_failed ("%s failed: text is NULL", what);
		goto cleanup;
	}
	
	if (strcmp (text, (const char *) expected->data) != 0) {
		testsuite_check_failed ("%s failed: text does not match: expected=%s; actual=%s",
					what, (const char *) expected->data, text);
		g_free (text);
		goto cleanup;
	}
	
	testsuite_check_passed ();
	
cleanup:
	
	g_byte_array_free (expected, TRUE);
	g_object_unref (part);
}

int main (int argc, char **argv)
{
	const char *datadir = "data/mime-part";
	struct stat st;
	int i;
	
	g_mime_init ();
	
	testsuite_init (argc, argv);
	
	//verbose = 4;
	
	for (i = 1; i < argc; i++) {
		if (argv[i][0] != '-') {
			datadir = argv[i];
			break;
		}
	}
	
	if (i < argc && (stat (datadir, &st) == -1 || !S_ISDIR (st.st_mode)))
		return 0;
	
	testsuite_start ("GMimePart");
	
	test_content_headers (datadir);
	test_content_md5_vectors ();
	test_content_md5_assignment ();
	test_content_description_assignment ();
	test_content_encoding_assignment ();
	test_content_encoding_invalid_assignment ();
	test_content_encoding_consumers ();
	
	test_write_to_stream (datadir, "raptors.b64.txt", GMIME_CONTENT_ENCODING_UNSPECIFIED);
	test_write_to_stream (datadir, "raptors.uu.txt", GMIME_CONTENT_ENCODING_UUENCODE);
	
	test_openpgp_data (datadir, "raptors.png", GMIME_OPENPGP_DATA_NONE);
	test_openpgp_data (datadir, "signed-body.txt", GMIME_OPENPGP_DATA_SIGNED);
	test_openpgp_data (datadir, "encrypted-body.txt", GMIME_OPENPGP_DATA_ENCRYPTED);
	test_openpgp_data (datadir, "pubkey-body.txt", GMIME_OPENPGP_DATA_PUBLIC_KEY);
	test_openpgp_data (datadir, "privkey-body.txt", GMIME_OPENPGP_DATA_PRIVATE_KEY);
	
	test_text_part (datadir, "french-fable.txt", "iso-8859-1");
	
	testsuite_end ();

	testsuite_start ("GMimeMultipart boundaries");
	test_generated_boundaries ();
	testsuite_end ();
	
	g_mime_shutdown ();
	
	return testsuite_exit ();
}
