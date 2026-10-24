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
test_content_type_constructors (void)
{
	static const char *types[] = { "application", "text", "multipart", "message" };
	static const char *subtypes[] = { "octet-stream", "plain", "mixed", "rfc822" };
	static const char *invalid[] = { "", "text", "/plain", "text/", "text/plain junk",
		"text/plain; charset", "text/plain; name=\"unterminated" };
	GMimeContentType *type, *parsed;
	GMimeParserOptions *options;
	GMimeParam *param;
	GMimePart *first, *second;
	GMimeEntity *entity;
	GMimeHeaderList *headers;
	GObject *alive;
	GError *error = NULL;
	guint i;

	testsuite_check ("Content-type constructors retain shared models and parameter metadata");
	for (i = 0; i < G_N_ELEMENTS (types); i++) {
		type = g_mime_content_type_new (types[i], subtypes[i]);
		g_mime_content_type_set_name (type, " caf\303\251.txt ");
		param = g_mime_param_list_get_parameter (g_mime_content_type_get_parameters (type), "name");
		g_mime_param_set_charset (param, "iso-8859-1");
		g_mime_param_set_lang (param, "fr");
		g_mime_param_set_encoding_method (param, GMIME_PARAM_ENCODING_METHOD_RFC2231);
		g_mime_param_set_always_quote (param, TRUE);
		alive = G_OBJECT (type);
		g_object_add_weak_pointer (alive, (gpointer *) &alive);
		first = g_mime_part_new_with_content_type (type);
		second = g_mime_part_new_with_content_type (type);
		entity = GMIME_ENTITY (first);
		g_assert_true (G_OBJECT_TYPE (first) == GMIME_TYPE_PART);
		g_assert_true (g_mime_entity_get_content_type (entity) == type);
		g_assert_true (g_mime_entity_get_content_type (GMIME_ENTITY (second)) == type);
		g_assert_null (g_mime_part_get_content (first));
		g_assert_null (g_mime_entity_get_content_disposition (entity));
		g_assert_cmpint (g_mime_part_get_content_encoding (first), ==, GMIME_CONTENT_ENCODING_UNSPECIFIED);
		g_assert_cmpstr (g_mime_part_get_filename (first), ==, "caf\303\251.txt");
		headers = g_mime_entity_get_header_list (entity);
		g_assert_cmpint (g_mime_header_list_get_count (headers), ==, 1);
		g_assert_true (g_mime_param_list_get_parameter (g_mime_content_type_get_parameters (type), "name") == param);
		g_assert_cmpstr (g_mime_param_get_charset (param), ==, "iso-8859-1");
		g_assert_cmpstr (g_mime_param_get_lang (param), ==, "fr");
		g_assert_cmpint (g_mime_param_get_encoding_method (param), ==, GMIME_PARAM_ENCODING_METHOD_RFC2231);
		g_assert_true (g_mime_param_get_always_quote (param));
		g_object_unref (type);
		g_assert_nonnull (alive);
		g_mime_content_type_set_name (type, "changed.txt");
		g_assert_cmpstr (g_mime_part_get_filename (first), ==, "changed.txt");
		g_assert_cmpstr (g_mime_part_get_filename (second), ==, "changed.txt");
		parsed = g_mime_content_type_parse_full (NULL, g_mime_entity_get_header (entity, "Content-Type"), -1, &error);
		g_assert_no_error (error);
		g_assert_nonnull (parsed);
		g_assert_cmpstr (g_mime_content_type_get_name (parsed), ==, "changed.txt");
		g_object_unref (parsed);
		g_object_unref (first);
		g_assert_nonnull (alive);
		g_mime_content_type_set_name (type, "survivor.txt");
		g_assert_cmpstr (g_mime_part_get_filename (second), ==, "survivor.txt");
		g_object_unref (second);
		g_assert_null (alive);
	}
	testsuite_check_passed ();

	testsuite_check ("String constructors parse complete content types without subclass dispatch");
	first = g_mime_part_new_with_content_type_string (NULL,
		"TEXT/PLAIN; charset=utf-8; name*=utf-8'fr'caf%C3%A9.txt", &error);
	g_assert_no_error (error);
	g_assert_nonnull (first);
	g_assert_true (G_OBJECT_TYPE (first) == GMIME_TYPE_PART);
	entity = GMIME_ENTITY (first);
	type = g_mime_entity_get_content_type (entity);
	g_assert_true (g_mime_content_type_is_type (type, "text", "plain"));
	g_assert_cmpstr (g_mime_content_type_get_charset (type), ==, "utf-8");
	g_assert_cmpstr (g_mime_part_get_filename (first), ==, "caf\303\251.txt");
	param = g_mime_param_list_get_parameter (g_mime_content_type_get_parameters (type), "name");
	g_assert_cmpstr (g_mime_param_get_charset (param), ==, "UTF-8");
	g_assert_cmpstr (g_mime_param_get_lang (param), ==, "fr");
	g_assert_null (g_mime_part_get_content (first));
	g_assert_null (g_mime_entity_get_content_disposition (entity));
	g_object_unref (first);
	for (i = 0; i < G_N_ELEMENTS (invalid); i++) {
		first = g_mime_part_new_with_content_type_string (NULL, invalid[i], &error);
		g_assert_null (first);
		g_assert_error (error, GMIME_ERROR, GMIME_ERROR_PARSE_ERROR);
		g_clear_error (&error);
		g_assert_null (g_mime_part_new_with_content_type_string (NULL, invalid[i], NULL));
	}
	testsuite_check_passed ();

	testsuite_check ("String constructors honor parameter compliance options");
	options = g_mime_parser_options_new ();
	g_mime_parser_options_set_parameter_compliance_mode (options, GMIME_RFC_COMPLIANCE_LOOSE);
	first = g_mime_part_new_with_content_type_string (options, "text/plain; name=two words.txt", &error);
	g_assert_no_error (error);
	g_assert_nonnull (first);
	g_assert_cmpstr (g_mime_part_get_filename (first), ==, "two words.txt");
	g_object_unref (first);
	g_mime_parser_options_set_parameter_compliance_mode (options, GMIME_RFC_COMPLIANCE_STRICT);
	first = g_mime_part_new_with_content_type_string (options, "text/plain; name=two words.txt", &error);
	g_assert_null (first);
	g_assert_error (error, GMIME_ERROR, GMIME_ERROR_PARSE_ERROR);
	g_clear_error (&error);
	g_mime_parser_options_free (options);
	testsuite_check_passed ();
}

static void
test_filename_property (void)
{
	static const char filename[] = " \302\205caf\303\251.txt\343\200\200 ";
	GMimePart *part;
	GMimeEntity *entity;
	GMimeContentType *type, *replacement;
	GMimeContentDisposition *disposition;
	GMimeHeaderList *headers;
	GMimeHeader *header;
	GMimeParam *param;
	const char *stored;
	char *type_raw, *disposition_raw;
	guint text;

	testsuite_check ("Filename lookup trims live parameters without changing raw headers");
	for (text = 0; text < 2; text++) {
		part = text ? GMIME_PART (g_mime_text_part_new ()) : g_mime_part_new ();
		entity = GMIME_ENTITY (part);
		headers = g_mime_entity_get_header_list (entity);
		type = g_mime_entity_get_content_type (entity);
		header = g_mime_header_list_get_header (headers, "Content-Type");
		type_raw = g_strdup (g_mime_header_get_raw_value (header));
		g_mime_part_set_filename (part, NULL);
		g_assert_null (g_mime_part_get_filename (part));
		g_assert_null (g_mime_entity_get_content_disposition (entity));
		g_assert_false (g_mime_header_list_contains (headers, "Content-Disposition"));
		g_assert_cmpstr (g_mime_header_get_raw_value (header), ==, type_raw);
		g_free (type_raw);

		g_mime_content_type_set_name (type, " \302\240fallback.txt\342\200\257 ");
		type_raw = g_strdup (g_mime_header_get_raw_value (header));
		stored = g_mime_part_get_filename (part);
		g_assert_cmpstr (stored, ==, "fallback.txt");
		g_assert_true (g_mime_part_get_filename (part) == stored);
		g_assert_cmpstr (g_mime_content_type_get_name (type), ==, " \302\240fallback.txt\342\200\257 ");
		g_assert_cmpstr (g_mime_header_get_raw_value (header), ==, type_raw);
		g_assert_null (g_mime_entity_get_content_disposition (entity));
		g_free (type_raw);

		g_mime_part_set_filename (part, filename);
		disposition = g_mime_entity_get_content_disposition (entity);
		g_assert_true (g_mime_part_is_attachment (part));
		g_assert_cmpstr (g_mime_content_type_get_name (type), ==, filename);
		g_assert_cmpstr (g_mime_content_disposition_get_filename (disposition), ==, filename);
		g_assert_cmpstr (g_mime_part_get_filename (part), ==, "caf\303\251.txt");
		g_mime_entity_set_disposition (entity, "inline");
		g_mime_content_disposition_set_parameter (disposition, "keep", "stable");
		param = g_mime_param_list_get_parameter (g_mime_content_disposition_get_parameters (disposition), "filename");
		g_mime_param_set_charset (param, "iso-8859-1");
		g_mime_param_set_lang (param, "fr");
		g_mime_param_set_encoding_method (param, GMIME_PARAM_ENCODING_METHOD_RFC2231);
		g_mime_param_set_always_quote (param, TRUE);
		type_raw = g_strdup (g_mime_header_get_raw_value (header));
		header = g_mime_header_list_get_header (headers, "Content-Disposition");
		disposition_raw = g_strdup (g_mime_header_get_raw_value (header));
		g_mime_part_set_filename (part, filename);
		g_assert_cmpstr (g_mime_header_get_raw_value (header), ==, disposition_raw);
		g_assert_cmpstr (g_mime_header_get_raw_value (g_mime_header_list_get_header (headers, "Content-Type")), ==, type_raw);
		g_free (type_raw);
		g_free (disposition_raw);
		g_mime_part_set_filename (part, "changed.txt");
		g_assert_true (g_mime_entity_get_content_disposition (entity) == disposition);
		g_assert_cmpstr (g_mime_entity_get_disposition (entity), ==, "inline");
		g_assert_true (g_mime_param_list_get_parameter (g_mime_content_disposition_get_parameters (disposition), "filename") == param);
		g_assert_cmpstr (g_mime_param_get_charset (param), ==, "iso-8859-1");
		g_assert_cmpstr (g_mime_param_get_lang (param), ==, "fr");
		g_assert_cmpint (g_mime_param_get_encoding_method (param), ==, GMIME_PARAM_ENCODING_METHOD_RFC2231);
		g_assert_true (g_mime_param_get_always_quote (param));
		g_assert_cmpstr (g_mime_part_get_filename (part), ==, "changed.txt");
		g_mime_part_set_filename (part, NULL);
		g_assert_null (g_mime_part_get_filename (part));
		g_assert_null (g_mime_content_type_get_name (type));
		g_assert_null (g_mime_content_disposition_get_filename (disposition));
		g_assert_cmpstr (g_mime_entity_get_disposition (entity), ==, "inline");
		g_assert_cmpstr (g_mime_content_disposition_get_parameter (disposition, "keep"), ==, "stable");
		disposition_raw = g_strdup (g_mime_header_get_raw_value (header));
		g_mime_part_set_filename (part, NULL);
		g_assert_cmpstr (g_mime_header_get_raw_value (header), ==, disposition_raw);
		g_free (disposition_raw);

		g_mime_part_set_filename (part, "\v\302\205\343\200\200");
		g_mime_content_type_set_name (type, "fallback.txt");
		g_assert_cmpstr (g_mime_part_get_filename (part), ==, "");
		g_mime_content_disposition_set_filename (disposition, "");
		g_assert_cmpstr (g_mime_part_get_filename (part), ==, "");
		g_mime_content_disposition_set_filename (disposition, NULL);
		g_assert_cmpstr (g_mime_part_get_filename (part), ==, "fallback.txt");
		replacement = g_mime_content_type_new ("text", "plain");
		g_mime_content_type_set_name (replacement, " replacement.txt ");
		g_mime_entity_set_content_type (entity, replacement);
		g_object_unref (replacement);
		g_assert_cmpstr (g_mime_part_get_filename (part), ==, "replacement.txt");
		header = g_mime_header_list_get_header (headers, "Content-Type");
		g_mime_header_set_raw_value (header, " text/plain; name=\" raw fallback.txt \"\n");
		g_assert_cmpstr (g_mime_part_get_filename (part), ==, "raw fallback.txt");
		header = g_mime_header_list_get_header (headers, "Content-Disposition");
		g_mime_header_set_raw_value (header, " attachment; filename=\" raw primary.txt \"\n");
		g_assert_cmpstr (g_mime_part_get_filename (part), ==, "raw primary.txt");
		g_mime_header_list_remove (headers, "Content-Disposition");
		g_assert_cmpstr (g_mime_part_get_filename (part), ==, "raw fallback.txt");
		g_object_unref (part);
	}
	testsuite_check_passed ();
}

static void
test_filename_assignment_aliases (void)
{
	GMimePart *part = g_mime_part_new ();
	GMimeEntity *entity = GMIME_ENTITY (part);
	GMimeContentType *type = g_mime_entity_get_content_type (entity);
	GMimeContentDisposition *disposition;
	GMimeHeaderList *headers;
	char *type_raw, *disposition_raw;
	guint existing;

	testsuite_check ("Filename assignments safely copy borrowed values and substrings");
	g_mime_part_set_filename (part, "prefix-borrowed.txt");
	disposition = g_mime_entity_get_content_disposition (entity);
	g_mime_part_set_filename (part, g_mime_content_disposition_get_filename (disposition) + 7);
	g_assert_cmpstr (g_mime_content_disposition_get_filename (disposition), ==, "borrowed.txt");
	g_assert_cmpstr (g_mime_content_type_get_name (type), ==, "borrowed.txt");
	g_mime_part_set_filename (part, "xgetter.txt");
	g_mime_part_set_filename (part, g_mime_part_get_filename (part) + 1);
	g_assert_cmpstr (g_mime_part_get_filename (part), ==, "getter.txt");
	g_assert_cmpstr (g_mime_content_type_get_name (type), ==, "getter.txt");
	g_mime_content_type_set_name (type, "xtype.txt");
	g_mime_part_set_filename (part, g_mime_content_type_get_name (type) + 1);
	g_assert_cmpstr (g_mime_part_get_filename (part), ==, "type.txt");
	g_assert_cmpstr (g_mime_content_type_get_name (type), ==, "type.txt");
	g_mime_part_set_filename (part, g_mime_part_get_filename (part));
	g_assert_cmpstr (g_mime_part_get_filename (part), ==, "type.txt");
	g_object_unref (part);
	testsuite_check_passed ();

	testsuite_check ("Invalid UTF-8 filenames leave models, headers and cached lookup unchanged");
	for (existing = 0; existing < 2; existing++) {
		part = g_mime_part_new ();
		entity = GMIME_ENTITY (part);
		headers = g_mime_entity_get_header_list (entity);
		if (existing)
			g_mime_part_set_filename (part, "stable.txt");
		g_assert_cmpstr (g_mime_part_get_filename (part), ==, existing ? "stable.txt" : NULL);
		type_raw = g_strdup (g_mime_header_get_raw_value (g_mime_header_list_get_header (headers, "Content-Type")));
		disposition_raw = existing ? g_strdup (g_mime_header_get_raw_value (g_mime_header_list_get_header (headers, "Content-Disposition"))) : NULL;
		g_test_expect_message ("gmime", G_LOG_LEVEL_CRITICAL, "*g_utf8_validate*");
		g_mime_part_set_filename (part, "\377");
		g_test_assert_expected_messages ();
		g_assert_cmpstr (g_mime_part_get_filename (part), ==, existing ? "stable.txt" : NULL);
		g_assert_cmpstr (g_mime_header_get_raw_value (g_mime_header_list_get_header (headers, "Content-Type")), ==, type_raw);
		if (existing)
			g_assert_cmpstr (g_mime_header_get_raw_value (g_mime_header_list_get_header (headers, "Content-Disposition")), ==, disposition_raw);
		else
			g_assert_null (g_mime_entity_get_content_disposition (entity));
		g_free (type_raw);
		g_free (disposition_raw);
		g_object_unref (part);
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
test_content_md5_lazy_property (void)
{
	static const char digest[] = "kAFQmDzST7DWlj99KOF/cg==";
	GMimePart *part;
	GMimeHeaderList *headers;
	GMimeHeader *header, *duplicate;
	char *raw, *value;
	guint text, loaded;
	int count;

	testsuite_check ("Content-MD5 lazy reads, cached assignment and duplicate invalidation");
	for (text = 0; text < 2; text++) {
		for (loaded = 0; loaded < 2; loaded++) {
			part = text ? GMIME_PART (g_mime_text_part_new ()) : g_mime_part_new ();
			headers = g_mime_entity_get_header_list (GMIME_ENTITY (part));
			count = g_mime_header_list_get_count (headers);
			g_assert_null (g_mime_part_get_content_md5 (part));
			g_mime_header_list_append (headers, "Content-Md5", digest, NULL);
			header = g_mime_header_list_get_header (headers, "Content-Md5");
			raw = g_strdup_printf (" \302\205%s\343\200\200\t\n", digest);
			g_mime_header_set_raw_value (header, raw);
			g_mime_header_list_append (headers, "Content-Md5", "second", NULL);
			if (loaded)
				g_assert_cmpstr (g_mime_part_get_content_md5 (part), ==, digest);
			g_assert_cmpstr (g_mime_header_get_raw_value (header), ==, raw);
			g_mime_part_set_content_md5 (part, digest);
			g_assert_cmpint (g_mime_header_list_get_count (headers), ==, count + (loaded ? 2 : 1));
			if (loaded)
				g_assert_cmpstr (g_mime_header_get_raw_value (header), ==, raw);
			g_free (raw);

			g_mime_header_list_append (headers, "Content-Md5", "second", NULL);
			duplicate = g_mime_header_list_get_header_at (headers, g_mime_header_list_get_count (headers) - 1);
			g_mime_header_set_value (duplicate, NULL, "changed duplicate", NULL);
			g_assert_cmpstr (g_mime_part_get_content_md5 (part), ==, digest);
			value = g_strconcat (" ", digest, " ", NULL);
			g_mime_part_set_content_md5 (part, value);
			g_free (value);
			g_assert_cmpint (g_mime_header_list_get_count (headers), ==, count + 1);
			g_mime_header_list_append (headers, "Content-Md5", "next", NULL);
			g_mime_header_list_remove_at (headers, count);
			g_assert_cmpstr (g_mime_part_get_content_md5 (part), ==, "next");
			header = g_mime_header_list_get_header (headers, "Content-Md5");
			g_mime_header_set_raw_value (header, " \302\240raw value\342\200\257\n");
			g_assert_cmpstr (g_mime_part_get_content_md5 (part), ==, "raw value");
			g_mime_header_list_prepend (headers, "Content-Md5", "", NULL);
			g_assert_cmpstr (g_mime_part_get_content_md5 (part), ==, "");
			header = g_mime_header_list_get_header (headers, "Content-Md5");
			raw = g_strdup (g_mime_header_get_raw_value (header));
			g_mime_header_list_set (headers, "X-Unrelated", "unchanged cache", NULL);
			g_mime_part_set_content_md5 (part, "");
			g_assert_cmpstr (g_mime_header_get_raw_value (header), ==, raw);
			g_assert_cmpint (g_mime_header_list_get_count (headers), ==, count + 3);
			g_free (raw);
			g_mime_part_set_content_md5 (part, NULL);
			g_assert_null (g_mime_part_get_content_md5 (part));
			g_assert_false (g_mime_header_list_contains (headers, "Content-Md5"));
			g_mime_header_list_append (headers, "Content-Md5", "after removal", NULL);
			g_assert_cmpstr (g_mime_part_get_content_md5 (part), ==, "after removal");
			g_mime_header_list_clear (headers);
			g_assert_null (g_mime_part_get_content_md5 (part));
			g_mime_part_set_content_md5 (part, NULL);
			g_assert_cmpint (g_mime_header_list_get_count (headers), ==, 0);
			g_object_unref (part);
		}
	}
	testsuite_check_passed ();
}

static void
test_content_md5_lazy_verification (void)
{
	static const char digest[] = "kAFQmDzST7DWlj99KOF/cg==";
	GMimePart *part = g_mime_part_new ();
	GMimeHeaderList *headers = g_mime_entity_get_header_list (GMIME_ENTITY (part));
	GMimeStream *source = g_mime_stream_mem_new_with_buffer ("abc", 3);
	GMimeContent *content = g_mime_content_new_with_stream (source, GMIME_CONTENT_ENCODING_UNSPECIFIED);
	GMimeHeader *header, *duplicate;
	GError *error = NULL;
	char *raw;

	testsuite_check ("Content-MD5 verification resolves the first header without a preceding getter");
	g_mime_part_set_content (part, content);
	g_mime_header_list_append (headers, "Content-Md5", digest, NULL);
	g_mime_header_list_append (headers, "Content-Md5", "incorrect", NULL);
	header = g_mime_header_list_get_header (headers, "Content-Md5");
	raw = g_strdup (g_mime_header_get_raw_value (header));
	g_assert_true (g_mime_part_verify_content_md5 (part, &error));
	g_assert_no_error (error);
	g_assert_cmpstr (g_mime_header_get_raw_value (header), ==, raw);
	g_assert_cmpint (g_mime_header_list_get_count (headers), ==, 3);
	g_free (raw);
	duplicate = g_mime_header_list_get_header_at (headers, 2);
	g_mime_header_set_value (duplicate, NULL, "changed duplicate", NULL);
	g_assert_true (g_mime_part_verify_content_md5 (part, &error));
	g_assert_no_error (error);
	g_mime_header_list_remove_at (headers, 1);
	g_assert_false (g_mime_part_verify_content_md5 (part, &error));
	g_assert_no_error (error);
	g_mime_header_list_prepend (headers, "Content-Md5", "", NULL);
	g_assert_cmpint (g_mime_stream_seek (source, 1, GMIME_STREAM_SEEK_SET), ==, 1);
	g_assert_false (g_mime_part_verify_content_md5 (part, &error));
	g_assert_no_error (error);
	g_assert_cmpint (g_mime_stream_tell (source), ==, 1);
	header = g_mime_header_list_get_header (headers, "Content-Md5");
	g_mime_header_set_raw_value (header, " \302\205\343\200\200\n");
	g_assert_false (g_mime_part_verify_content_md5 (part, &error));
	g_assert_no_error (error);
	g_assert_cmpint (g_mime_stream_tell (source), ==, 1);
	g_mime_header_set_value (header, NULL, digest, NULL);
	g_assert_true (g_mime_part_verify_content_md5 (part, &error));
	g_assert_no_error (error);
	g_assert_true (g_mime_part_get_content (part) == content);
	g_assert_cmpint (g_mime_content_get_encoding (content), ==, GMIME_CONTENT_ENCODING_UNSPECIFIED);
	g_object_unref (content);
	g_object_unref (source);
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
test_text_with_charset (void)
{
	static const struct {
		const char *charset, *name, *text, *bytes;
		gsize length;
	} cases[] = {
		{ "UTF8", "utf-8", "caf\303\251", "caf\303\251", 5 },
		{ "latin1", "iso-8859-1", "caf\303\251", "caf\351", 4 },
		{ "CP1252", "windows-1252", "\342\202\254", "\200", 1 },
		{ "us-ascii", "us-ascii", "a\r\nb\nc\rd", "a\r\nb\nc\rd", 8 },
		{ "UTF-16", "utf-16", "caf\303\251", "c\0a\0f\0\351\0", 8 },
		{ "utf16", "utf-16", "caf\303\251", "c\0a\0f\0\351\0", 8 },
		{ "Unicode", "utf-8", "caf\303\251", "caf\303\251", 5 },
		{ "csUnicode", "utf-16", "caf\303\251", "c\0a\0f\0\351\0", 8 },
		{ "utf-16le", "utf-16le", "\360\237\230\200", "\075\330\0\336", 4 },
		{ "utf-16be", "utf-16be", "caf\303\251", "\0c\0a\0f\0\351", 8 },
		{ "UTF-32", "utf-32", "caf\303\251", "c\0\0\0a\0\0\0f\0\0\0\351\0\0\0", 16 },
		{ "utf32", "utf-32", "caf\303\251", "c\0\0\0a\0\0\0f\0\0\0\351\0\0\0", 16 },
		{ "utf-32be", "utf-32be", "\360\237\230\200", "\0\1\366\0", 4 },
		{ "utf-8", "utf-8", "\357\273\277text", "\357\273\277text", 7 },
		{ "utf-8", "utf-8", "", "", 0 },
		{ "utf-16", "utf-16", "", "", 0 },
	};
	static const struct {
		const char *charset, *text;
		int code;
	} invalid[] = {
		{ "", "text", G_CONVERT_ERROR_NO_CONVERSION },
		{ "not-a-text-charset", "text", -1 },
		{ "utf-8//IGNORE", "text", G_CONVERT_ERROR_NO_CONVERSION },
		{ "ascii//TRANSLIT", "text", G_CONVERT_ERROR_NO_CONVERSION },
		{ "utf-8 ", "text", G_CONVERT_ERROR_NO_CONVERSION },
		{ "\377", "text", G_CONVERT_ERROR_NO_CONVERSION },
		{ "us-ascii", "caf\303\251", G_CONVERT_ERROR_ILLEGAL_SEQUENCE },
		{ "utf-8", "\377", G_CONVERT_ERROR_ILLEGAL_SEQUENCE },
		{ "utf-8", "a\303", G_CONVERT_ERROR_PARTIAL_INPUT },
	};
	GMimeTextPart *part = g_mime_text_part_new_with_subtype ("html");
	GMimeContentType *type = g_mime_entity_get_content_type (GMIME_ENTITY (part));
	GMimePart *base = GMIME_PART (part);
	GMimeContent *previous, *content;
	GMimeStream *stream;
	GMimeParam *param;
	GByteArray *buffer;
	GError *error = NULL;
	char *header;
	guint i, encoding;

	testsuite_check ("Explicit text charsets preserve exact bytes and transfer encodings");
	g_mime_content_type_set_name (type, "body.html");
	for (i = 0; i < G_N_ELEMENTS (cases); i++) {
		for (encoding = GMIME_CONTENT_ENCODING_UNSPECIFIED; encoding <= GMIME_CONTENT_ENCODING_UUENCODE; encoding++) {
			g_mime_part_set_content_encoding (base, encoding);
			g_assert_true (g_mime_text_part_set_text_with_charset (part, cases[i].charset, cases[i].text, &error));
			g_assert_no_error (error);
			g_assert_cmpstr (g_mime_text_part_get_charset (part), ==, cases[i].name);
			g_assert_true (g_mime_entity_get_content_type (GMIME_ENTITY (part)) == type);
			g_assert_true (g_mime_text_part_is_html (part));
			g_assert_cmpstr (g_mime_content_type_get_name (type), ==, "body.html");
			g_assert_cmpint (g_mime_part_get_content_encoding (base), ==, encoding);
			content = g_mime_part_get_content (base);
			g_assert_nonnull (content);
			g_assert_cmpint (g_mime_content_get_encoding (content), ==, GMIME_CONTENT_ENCODING_UNSPECIFIED);
			stream = g_mime_content_get_stream (content);
			g_assert_cmpint (g_mime_stream_tell (stream), ==, 0);
			buffer = g_mime_stream_mem_get_byte_array (GMIME_STREAM_MEM (stream));
			g_assert_cmpmem (buffer->data, buffer->len, cases[i].bytes, cases[i].length);
		}
	}
	testsuite_check_passed ();

	testsuite_check ("Explicit text charset conversion failures leave content and headers unchanged");
	g_assert_true (g_mime_text_part_set_text_with_charset (part, "utf-8", "original", &error));
	g_assert_no_error (error);
	previous = g_object_ref (g_mime_part_get_content (base));
	header = g_strdup (g_mime_entity_get_header (GMIME_ENTITY (part), "Content-Type"));
	for (i = 0; i < G_N_ELEMENTS (invalid); i++) {
		g_assert_false (g_mime_text_part_set_text_with_charset (part, invalid[i].charset, invalid[i].text, &error));
		if (invalid[i].code == -1) {
			g_assert_nonnull (error);
			g_assert_cmpuint (error->domain, ==, G_CONVERT_ERROR);
		} else {
			g_assert_error (error, G_CONVERT_ERROR, invalid[i].code);
		}
		g_clear_error (&error);
		g_assert_false (g_mime_text_part_set_text_with_charset (part, invalid[i].charset, invalid[i].text, NULL));
		g_assert_true (g_mime_part_get_content (base) == previous);
		g_assert_cmpstr (g_mime_entity_get_header (GMIME_ENTITY (part), "Content-Type"), ==, header);
		g_assert_cmpint (g_mime_part_get_content_encoding (base), ==, GMIME_CONTENT_ENCODING_UUENCODE);
	}
	g_free (header);
	g_object_unref (previous);
	testsuite_check_passed ();

	testsuite_check ("Explicit text assignment accepts borrowed charset and parameter strings");
	param = g_mime_param_list_get_parameter (g_mime_content_type_get_parameters (type), "charset");
	g_mime_param_set_charset (param, "iso-8859-1");
	g_mime_param_set_lang (param, "fr");
	g_mime_param_set_always_quote (param, TRUE);
	g_assert_true (g_mime_text_part_set_text_with_charset (part, g_mime_text_part_get_charset (part),
							     g_mime_text_part_get_charset (part), &error));
	g_assert_no_error (error);
	g_assert_cmpstr (g_mime_text_part_get_charset (part), ==, "utf-8");
	g_assert_true (g_mime_param_list_get_parameter (g_mime_content_type_get_parameters (type), "charset") == param);
	g_assert_cmpstr (g_mime_param_get_charset (param), ==, "iso-8859-1");
	g_assert_cmpstr (g_mime_param_get_lang (param), ==, "fr");
	g_assert_true (g_mime_param_get_always_quote (param));
	buffer = g_mime_stream_mem_get_byte_array (GMIME_STREAM_MEM (g_mime_content_get_stream (g_mime_part_get_content (base))));
	g_assert_cmpmem (buffer->data, buffer->len, "utf-8", 5);
	g_assert_true (g_mime_text_part_set_text_with_charset (part, "latin1", g_mime_text_part_get_charset (part), &error));
	g_assert_no_error (error);
	g_assert_cmpstr (g_mime_text_part_get_charset (part), ==, "iso-8859-1");
	buffer = g_mime_stream_mem_get_byte_array (GMIME_STREAM_MEM (g_mime_content_get_stream (g_mime_part_get_content (base))));
	g_assert_cmpmem (buffer->data, buffer->len, "utf-8", 5);
	testsuite_check_passed ();
	g_object_unref (part);
}

static void
test_text_decode_with_charset (void)
{
	static const struct {
		const char *charset, *bytes, *expected;
		gsize length;
	} cases[] = {
		{ "utf8", "caf\303\251\r\nlast\r", "caf\303\251\nlast\r", 12 },
		{ "latin1", "caf\351", "caf\303\251", 4 },
		{ "CP1252", "\200", "\342\202\254", 1 },
		{ "us-ascii", "a\r\nb\nc\rd", "a\nb\nc\rd", 8 },
		{ "utf-16", "c\0a\0f\0\351\0", "caf\303\251", 8 },
		{ "Unicode", "caf\303\251", "caf\303\251", 5 },
		{ "csUnicode", "c\0a\0f\0\351\0", "caf\303\251", 8 },
		{ "utf16", "\r\1\n\1", "\304\215\304\212", 4 },
		{ "utf-16le", "\075\330\0\336", "\360\237\230\200", 4 },
		{ "utf-16be", "\0c\0a\0f\0\351", "caf\303\251", 8 },
		{ "utf-32", "c\0\0\0a\0\0\0f\0\0\0\351\0\0\0", "caf\303\251", 16 },
		{ "utf32", "\r\1\0\0\n\1\0\0", "\304\215\304\212", 8 },
		{ "utf-32be", "\0\1\366\0", "\360\237\230\200", 4 },
		{ "utf-8", "\357\273\277text", "\357\273\277text", 7 },
		{ "utf-16", "\377\376A\0", "\357\273\277A", 4 },
		{ "iso-2022-jp", "\033$BF|K\\8l\033(B", "\346\227\245\346\234\254\350\252\236", 12 },
		{ "utf-8", "", "", 0 },
		{ "utf-16", "", "", 0 },
	};
	static const struct {
		const char *charset, *bytes, *expected;
		GMimeContentEncoding encoding;
	} encoded_cases[] = {
		{ "utf-16", "=41=00=0D=00=0A=00=0D=01=0A=01", "A\n\304\215\304\212",
		  GMIME_CONTENT_ENCODING_QUOTEDPRINTABLE },
		{ "utf-8", "begin 644 text.txt\r\n#0V%T\r\n`\r\nend\r\n", "Cat",
		  GMIME_CONTENT_ENCODING_UUENCODE },
	};
	static const char *invalid_charsets[] = {
		"not-a-text-charset", "", "utf-8//IGNORE", "utf-8//TRANSLIT", "utf 8"
	};
	static const struct {
		const char *charset, *bytes;
		gsize length;
		int code;
	} invalid[] = {
		{ "utf-8", "valid\377", 6, G_CONVERT_ERROR_ILLEGAL_SEQUENCE },
		{ "utf-8", "valid\303", 6, G_CONVERT_ERROR_PARTIAL_INPUT },
		{ "us-ascii", "\377", 1, G_CONVERT_ERROR_ILLEGAL_SEQUENCE },
		{ "utf-16", "a", 1, G_CONVERT_ERROR_PARTIAL_INPUT },
		{ "utf-8", "a\0b", 3, G_CONVERT_ERROR_EMBEDDED_NUL },
		{ "utf-16", "A\0\0\0B\0", 6, G_CONVERT_ERROR_EMBEDDED_NUL },
	};
	GMimeTextPart *part = g_mime_text_part_new ();
	GMimeEntity *entity = GMIME_ENTITY (part);
	GMimeContent *content;
	GMimeStream *stream;
	GError *error = NULL;
	char *text, *encoded, *headers, *after;
	guint i, encoding;

	testsuite_check ("Explicit text decoding validates charsets even without content");
	text = g_mime_text_part_get_text_with_charset (part, "utf-16", &error);
	g_assert_no_error (error);
	g_assert_cmpstr (text, ==, "");
	g_free (text);
	for (i = 0; i < G_N_ELEMENTS (invalid_charsets); i++) {
		g_assert_null (g_mime_text_part_get_text_with_charset (part, invalid_charsets[i], &error));
		g_assert_nonnull (error);
		g_assert_cmpuint (error->domain, ==, G_CONVERT_ERROR);
		g_clear_error (&error);
	}
	g_assert_null (g_mime_part_get_content (GMIME_PART (part)));
	testsuite_check_passed ();

	testsuite_check ("Explicit text decoding overrides headers and normalizes only decoded newlines");
	g_mime_text_part_set_charset (part, "not-the-source-charset");
	g_mime_part_set_content_encoding (GMIME_PART (part), GMIME_CONTENT_ENCODING_QUOTEDPRINTABLE);
	headers = g_mime_entity_get_headers (entity, NULL);
	for (i = 0; i < G_N_ELEMENTS (cases); i++) {
		for (encoding = GMIME_CONTENT_ENCODING_UNSPECIFIED; encoding <= GMIME_CONTENT_ENCODING_BASE64; encoding++) {
			if (encoding == GMIME_CONTENT_ENCODING_BASE64) {
				encoded = g_base64_encode ((const guchar *) cases[i].bytes, cases[i].length);
				stream = g_mime_stream_mem_new_with_buffer (encoded, strlen (encoded));
				g_free (encoded);
			} else {
				stream = g_mime_stream_mem_new_with_buffer (cases[i].bytes, cases[i].length);
			}
			content = g_mime_content_new_with_stream (stream, encoding);
			g_mime_part_set_content (GMIME_PART (part), content);
			g_object_unref (content);
			text = g_mime_text_part_get_text_with_charset (part, cases[i].charset, &error);
			g_assert_no_error (error);
			g_assert_cmpstr (text, ==, cases[i].expected);
			g_free (text);
			g_assert_cmpint (g_mime_stream_tell (stream), ==, 0);
			text = g_mime_text_part_get_text_with_charset (part, cases[i].charset, &error);
			g_assert_no_error (error);
			g_assert_cmpstr (text, ==, cases[i].expected);
			g_free (text);
			g_object_unref (stream);
			after = g_mime_entity_get_headers (entity, NULL);
			g_assert_cmpstr (after, ==, headers);
			g_free (after);
		}
	}
	testsuite_check_passed ();

	testsuite_check ("Explicit text decoding handles quoted-printable binary bytes and uuencoded content");
	for (i = 0; i < G_N_ELEMENTS (encoded_cases); i++) {
		stream = g_mime_stream_mem_new_with_buffer (encoded_cases[i].bytes, strlen (encoded_cases[i].bytes));
		content = g_mime_content_new_with_stream (stream, encoded_cases[i].encoding);
		g_mime_part_set_content (GMIME_PART (part), content);
		g_object_unref (content);
		text = g_mime_text_part_get_text_with_charset (part, encoded_cases[i].charset, &error);
		g_assert_no_error (error);
		g_assert_cmpstr (text, ==, encoded_cases[i].expected);
		g_assert_cmpint (g_mime_stream_tell (stream), ==, 0);
		g_free (text);
		g_object_unref (stream);
	}
	testsuite_check_passed ();

	testsuite_check ("Explicit text decoding rejects malformed or NUL-containing text without partial results");
	for (i = 0; i < G_N_ELEMENTS (invalid); i++) {
		stream = g_mime_stream_mem_new_with_buffer (invalid[i].bytes, invalid[i].length);
		content = g_mime_content_new_with_stream (stream, GMIME_CONTENT_ENCODING_UNSPECIFIED);
		g_mime_part_set_content (GMIME_PART (part), content);
		g_object_unref (stream);
		g_assert_null (g_mime_text_part_get_text_with_charset (part, invalid[i].charset, &error));
		g_assert_error (error, G_CONVERT_ERROR, invalid[i].code);
		g_clear_error (&error);
		g_assert_null (g_mime_text_part_get_text_with_charset (part, invalid[i].charset, NULL));
		g_assert_true (g_mime_part_get_content (GMIME_PART (part)) == content);
		after = g_mime_entity_get_headers (entity, NULL);
		g_assert_cmpstr (after, ==, headers);
		g_free (after);
		g_object_unref (content);
	}
	testsuite_check_passed ();
	g_free (headers);
	g_object_unref (part);
}

static void
test_part_prepare (void)
{
	static const guint limits[] = { 60, 61, 76, 78, 998 };
	GMimePart *part;
	GMimeStream *source, *output;
	GMimeContent *content;
	GMimeHeaderList *headers;
	GMimeHeader *header;
	GByteArray *buffer;
	GMimeFormatOptions *options = g_mime_format_options_new ();
	GError *error = NULL;
	char text[1202], *before, *after;
	guint i, j, kind, newline, length, cap;

	memset (text, 'a', sizeof (text) - 2);
	text[sizeof (text) - 2] = '\n';
	text[sizeof (text) - 1] = '\0';

	testsuite_check ("Preparing unspecified 7bit content preserves raw transfer headers and duplicates");
	part = g_mime_part_new_with_type ("text", "plain");
	source = g_mime_stream_mem_new_with_buffer ("ASCII\n", 6);
	content = g_mime_content_new_with_stream (source, GMIME_CONTENT_ENCODING_UNSPECIFIED);
	g_mime_part_set_content (part, content);
	g_object_unref (content);
	g_object_unref (source);
	headers = g_mime_entity_get_header_list (GMIME_ENTITY (part));
	g_mime_header_list_append (headers, "Content-Transfer-Encoding", "unknown", NULL);
	g_mime_header_list_append (headers, "Content-Transfer-Encoding", "base64", NULL);
	header = g_mime_header_list_get_header (headers, "Content-Transfer-Encoding");
	g_mime_header_set_raw_value (header, " x-unrecognized \t\n");
	before = g_mime_entity_get_headers (GMIME_ENTITY (part), NULL);
	g_assert_true (g_mime_part_prepare (part, GMIME_ENCODING_CONSTRAINT_7BIT, &error));
	g_assert_no_error (error);
	after = g_mime_entity_get_headers (GMIME_ENTITY (part), NULL);
	g_assert_cmpstr (after, ==, before);
	g_assert_cmpint (g_mime_part_get_content_encoding (part), ==, GMIME_CONTENT_ENCODING_UNSPECIFIED);
	g_free (before);
	g_free (after);
	g_object_unref (part);
	testsuite_check_passed ();

	testsuite_check ("Prepared encoders honor line limits, newline options and safe-encoding retries");
	for (kind = 0; kind < 2; kind++) {
		for (i = 0; i < G_N_ELEMENTS (limits); i++) {
			for (newline = GMIME_NEWLINE_FORMAT_UNIX; newline <= GMIME_NEWLINE_FORMAT_DOS; newline++) {
				part = g_mime_part_new_with_type (kind ? "application" : "text", kind ? "octet-stream" : "plain");
				source = g_mime_stream_mem_new_with_buffer (text, strlen (text));
				content = g_mime_content_new_with_stream (source, GMIME_CONTENT_ENCODING_UNSPECIFIED);
				g_mime_part_set_content (part, content);
				g_assert_true (g_mime_part_prepare_with_max_line_length (part, GMIME_ENCODING_CONSTRAINT_7BIT,
										      limits[i], &error));
				g_assert_no_error (error);
				g_assert_cmpint (g_mime_part_get_content_encoding (part), ==,
						kind ? GMIME_CONTENT_ENCODING_BASE64 : GMIME_CONTENT_ENCODING_QUOTEDPRINTABLE);
				g_assert_true (g_mime_part_get_content (part) == content);
				g_assert_cmpint (g_mime_content_get_encoding (content), ==, GMIME_CONTENT_ENCODING_UNSPECIFIED);
				g_mime_format_options_set_newline_format (options, newline);
				output = g_mime_stream_mem_new ();
				g_assert_cmpint (g_mime_entity_write_content_to_stream (GMIME_ENTITY (part), options, output), >=, 0);
				buffer = g_mime_stream_mem_get_byte_array (GMIME_STREAM_MEM (output));
				cap = MIN (limits[i], 76);
				length = 0;
				for (j = 0; j < buffer->len; j++) {
					if (buffer->data[j] == '\n') {
						g_assert_cmpuint (length, <=, cap);
						if (newline == GMIME_NEWLINE_FORMAT_DOS)
							g_assert_true (j > 0 && buffer->data[j - 1] == '\r');
						length = 0;
					} else if (buffer->data[j] != '\r') {
						length++;
					}
				}
				g_assert_cmpuint (length, ==, 0);
				g_assert_true (g_mime_part_prepare_with_max_line_length (part, GMIME_ENCODING_CONSTRAINT_7BIT,
										      998, &error));
				g_assert_no_error (error);
				g_assert_cmpuint (part->encoder_max_line_length, ==, limits[i]);
				g_object_unref (output);
				g_object_unref (content);
				g_object_unref (source);
				g_object_unref (part);
			}
		}
	}
	g_mime_format_options_free (options);
	testsuite_check_passed ();
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
	test_content_type_constructors ();
	test_filename_property ();
	test_filename_assignment_aliases ();
	test_content_md5_vectors ();
	test_content_md5_assignment ();
	test_content_md5_lazy_property ();
	test_content_md5_lazy_verification ();
	test_content_description_assignment ();
	test_content_encoding_assignment ();
	test_content_encoding_invalid_assignment ();
	test_content_encoding_consumers ();
	test_part_prepare ();
	
	test_write_to_stream (datadir, "raptors.b64.txt", GMIME_CONTENT_ENCODING_UNSPECIFIED);
	test_write_to_stream (datadir, "raptors.uu.txt", GMIME_CONTENT_ENCODING_UUENCODE);
	
	test_openpgp_data (datadir, "raptors.png", GMIME_OPENPGP_DATA_NONE);
	test_openpgp_data (datadir, "signed-body.txt", GMIME_OPENPGP_DATA_SIGNED);
	test_openpgp_data (datadir, "encrypted-body.txt", GMIME_OPENPGP_DATA_ENCRYPTED);
	test_openpgp_data (datadir, "pubkey-body.txt", GMIME_OPENPGP_DATA_PUBLIC_KEY);
	test_openpgp_data (datadir, "privkey-body.txt", GMIME_OPENPGP_DATA_PRIVATE_KEY);
	
	test_text_part (datadir, "french-fable.txt", "iso-8859-1");
	test_text_with_charset ();
	test_text_decode_with_charset ();
	
	testsuite_end ();

	testsuite_start ("GMimeMultipart boundaries");
	test_generated_boundaries ();
	testsuite_end ();
	
	g_mime_shutdown ();
	
	return testsuite_exit ();
}
