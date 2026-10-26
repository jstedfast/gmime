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

#include <gmime/gmime.h>
#include "testsuite.h"

static void
test_charset_encoding (void)
{
	static const struct {
		const char *alias, *name;
	} cases[] = {
		{ "utf8", "utf-8" },
		{ "UTF-8", "utf-8" },
		{ "unicode", "utf-8" },
		{ "ASCII", "us-ascii" },
		{ "ANSI_X3.4-1968", "us-ascii" },
		{ "latin1", "iso-8859-1" },
		{ "ansi_x3.110-1983", "iso-8859-1" },
		{ "windows-1251", "windows-1251" },
		{ "windows-1252", "windows-1252" },
		{ "utf16", "utf-16" },
		{ "csunicode", "utf-16" },
		{ "UTF-16BE", "utf-16be" },
		{ "UTF-16LE", "utf-16le" },
		{ "utf32", "utf-32" },
		{ "UTF-32BE", "utf-32be" },
		{ "UTF-32LE", "utf-32le" },
		{ "shift_jis", "shift_jis" },
		{ "ks_c_5601-1987", "euc-kr" },
		{ "gb2312", "gb2312" }
	};
	GMimeContentType *type = g_mime_content_type_new ("text", "plain");
	GError *error = NULL;
	char *name;
	guint i;

	g_assert_null (g_mime_content_type_get_charset_encoding (type, &error));
	g_assert_no_error (error);
	for (i = 0; i < G_N_ELEMENTS (cases); i++) {
		g_mime_content_type_set_charset (type, cases[i].alias);
		name = g_mime_content_type_get_charset_encoding (type, &error);
		g_assert_no_error (error);
		g_assert_cmpstr (name, ==, cases[i].name);
		g_assert_cmpstr (g_mime_content_type_get_charset (type), ==, cases[i].alias);
		g_assert_true (g_mime_content_type_set_charset_encoding (
			type, g_mime_content_type_get_charset (type), &error));
		g_assert_no_error (error);
		g_assert_cmpstr (g_mime_content_type_get_charset (type), ==, cases[i].name);
		g_mime_content_type_set_charset (type, NULL);
		g_assert_cmpstr (name, ==, cases[i].name);
		g_free (name);
	}
	g_object_unref (type);
}

static void
test_invalid_charset_encoding (void)
{
	static const char *invalid[] = {
		"", "x-gmime-unknown", "x-unknown", " utf-8", "utf-8 ", "utf-8\r\n",
		"utf-8//IGNORE", "utf-8//TRANSLIT", "utf-8;name=x",
		"utf\303\251", "utf-8\t", "utf-8\n", "\"utf-8\""
	};
	GMimeContentType *type = g_mime_content_type_new ("text", "plain");
	GMimeParam *param;
	GError *error = NULL;
	guint i;

	for (i = 0; i < G_N_ELEMENTS (invalid); i++) {
		g_mime_content_type_set_charset (type, invalid[i]);
		param = g_mime_param_list_get_parameter (g_mime_content_type_get_parameters (type), "charset");
		g_assert_null (g_mime_content_type_get_charset_encoding (type, &error));
		g_assert_no_error (error);
		g_assert_cmpstr (g_mime_content_type_get_charset (type), ==, invalid[i]);
		g_assert_false (g_mime_content_type_set_charset_encoding (type, invalid[i], &error));
		g_assert_error (error, G_CONVERT_ERROR, G_CONVERT_ERROR_NO_CONVERSION);
		g_clear_error (&error);
		g_assert_true (param == g_mime_param_list_get_parameter (
			g_mime_content_type_get_parameters (type), "charset"));
		g_assert_cmpstr (g_mime_content_type_get_charset (type), ==, invalid[i]);
		g_assert_null (g_mime_content_type_get_charset_encoding (type, NULL));
		g_assert_false (g_mime_content_type_set_charset_encoding (type, invalid[i], NULL));
	}
	g_assert_true (g_mime_content_type_set_charset_encoding (type, NULL, &error));
	g_assert_no_error (error);
	g_assert_null (g_mime_content_type_get_charset (type));
	g_assert_false (g_mime_param_list_contains (g_mime_content_type_get_parameters (type), "charset"));
	g_object_unref (type);
}

static void
test_charset_headers (void)
{
	const char wire[] = "Content-Type: text/plain;\r\n\tcharset=\"utf-8\"; format=flowed\r\n\r\nbody";
	GMimeStream *stream = g_mime_stream_mem_new_with_buffer (wire, sizeof (wire) - 1);
	GError *error = NULL;
	GMimeEntity *entity = g_mime_entity_load (NULL, stream, &error);
	GMimeContentType *type;
	GMimeHeader *header;
	GMimeParam *param;
	GBytes *raw, *current;
	char *name;
	gboolean always_quote;

	g_assert_no_error (error);
	g_assert_nonnull (entity);
	type = g_mime_entity_get_content_type (entity);
	header = g_mime_header_list_get_header (g_mime_entity_get_header_list (entity), "Content-Type");
	raw = g_mime_header_get_raw_value (header);
	param = g_mime_param_list_get_parameter (g_mime_content_type_get_parameters (type), "charset");
	always_quote = g_mime_param_get_always_quote (param);
	name = g_mime_content_type_get_charset_encoding (type, &error);
	g_assert_no_error (error);
	g_assert_cmpstr (name, ==, "utf-8");
	current = g_mime_header_get_raw_value (header);
	g_assert_true (g_bytes_equal (raw, current));
	g_bytes_unref (current);
	g_assert_true (g_mime_content_type_set_charset_encoding (type, name, &error));
	g_assert_no_error (error);
	g_free (name);
	current = g_mime_header_get_raw_value (header);
	g_assert_true (g_bytes_equal (raw, current));
	g_bytes_unref (current);
	g_assert_true (param == g_mime_param_list_get_parameter (g_mime_content_type_get_parameters (type), "charset"));
	g_assert_cmpint (g_mime_param_get_always_quote (param), ==, always_quote);
	g_assert_false (g_mime_content_type_set_charset_encoding (type, "x-gmime-unknown", &error));
	g_assert_error (error, G_CONVERT_ERROR, G_CONVERT_ERROR_NO_CONVERSION);
	g_clear_error (&error);
	current = g_mime_header_get_raw_value (header);
	g_assert_true (g_bytes_equal (raw, current));
	g_bytes_unref (current);
	g_assert_true (g_mime_content_type_set_charset_encoding (type, "latin1", &error));
	g_assert_no_error (error);
	g_assert_cmpstr (g_mime_content_type_get_charset (type), ==, "iso-8859-1");
	current = g_mime_header_get_raw_value (header);
	g_assert_false (g_bytes_equal (raw, current));
	g_bytes_unref (current);
	g_assert_cmpstr (g_mime_content_type_get_format (type), ==, "flowed");
	name = g_mime_content_type_get_charset_encoding (type, &error);
	g_assert_no_error (error);
	g_assert_true (g_mime_content_type_set_charset_encoding (type, NULL, &error));
	g_assert_no_error (error);
	g_assert_cmpstr (name, ==, "iso-8859-1");
	g_assert_null (g_mime_content_type_get_charset (type));
	g_assert_cmpstr (g_mime_content_type_get_format (type), ==, "flowed");
	g_free (name);
	g_bytes_unref (raw);
	g_object_unref (entity);
	g_object_unref (stream);
}

int
main (int argc, char **argv)
{
	g_mime_init ();
	testsuite_init (argc, argv);
	testsuite_start ("Content-Type charset encoding");
	testsuite_check ("Unknown charsets, atomic failures and parameter removal");
	test_invalid_charset_encoding ();
	testsuite_check_passed ();
	testsuite_check ("Charset resolution, canonical assignment and independent names");
	test_charset_encoding ();
	testsuite_check_passed ();
	testsuite_check ("Resolution preserves raw headers; assignments notify the owning entity");
	test_charset_headers ();
	testsuite_check_passed ();
	testsuite_end ();
	g_mime_shutdown ();
	return testsuite_exit ();
}
