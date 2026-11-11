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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <string.h>
#include <gmime/gmime.h>
#include "testsuite.h"

static GMimeAuthenticationResults *
parse (const char *text, gssize length)
{
	GError *error = NULL;
	GMimeAuthenticationResults *results = g_mime_authentication_results_parse (text, length, &error);

	g_assert_no_error (error);
	g_assert_nonnull (results);
	return results;
}

static void
put (GString *snapshot, const char *value)
{
	char *encoded = value ? g_base64_encode ((const guchar *) value, strlen (value)) : g_strdup ("-");

	g_string_append_printf (snapshot, "%s\n", encoded);
	g_free (encoded);
}

static void
put_number (GString *snapshot, gboolean present, int value)
{
	char number[32];

	g_snprintf (number, sizeof (number), "%d", value);
	put (snapshot, present ? number : NULL);
}

static char *
snapshot (GMimeAuthenticationResults *results)
{
	GString *str = g_string_new (NULL);
	GMimeAuthenticationMethodResult *result;
	GMimeAuthenticationMethodProperty *property;
	gboolean present;
	int value, i, j;

	put (str, g_mime_authentication_results_get_authentication_service_identifier (results));
	present = g_mime_authentication_results_get_instance (results, &value);
	put_number (str, present, value);
	present = g_mime_authentication_results_get_version (results, &value);
	put_number (str, present, value);
	put_number (str, TRUE, g_mime_authentication_results_get_result_count (results));
	for (i = 0; i < g_mime_authentication_results_get_result_count (results); i++) {
		result = g_mime_authentication_results_get_result (results, i);
		put (str, g_mime_authentication_method_result_get_method (result));
		put (str, g_mime_authentication_method_result_get_result (result));
		put (str, g_mime_authentication_method_result_get_office365_authentication_service_identifier (result));
		present = g_mime_authentication_method_result_get_version (result, &value);
		put_number (str, present, value);
		put (str, g_mime_authentication_method_result_get_result_comment (result));
		put (str, g_mime_authentication_method_result_get_reason (result));
		put (str, g_mime_authentication_method_result_get_action (result));
		put_number (str, TRUE, g_mime_authentication_method_result_get_property_count (result));
		for (j = 0; j < g_mime_authentication_method_result_get_property_count (result); j++) {
			property = g_mime_authentication_method_result_get_property (result, j);
			put (str, g_mime_authentication_method_property_get_property_type (property));
			put (str, g_mime_authentication_method_property_get_property (property));
			put (str, g_mime_authentication_method_property_get_value (property));
		}
	}
	return g_string_free (str, FALSE);
}

static void
assert_encoded (char *actual, const char *expected)
{
	char *encoded = g_base64_encode ((const guchar *) actual, strlen (actual));

	g_assert_cmpstr (encoded, ==, expected);
	g_free (encoded);
	g_free (actual);
}

static void
test_vectors (const char *path)
{
	GMimeFormatOptions *options = g_mime_format_options_new ();
	GMimeHeader *header = g_mime_header_new ("Authentication-Results", "x; none", NULL);
	GMimeAuthenticationResults *results;
	GError *error = NULL;
	char *contents, **lines, **fields, *exact, *padded;
	guchar *input;
	gsize length;
	guint i, variant;

	g_assert_true (g_file_get_contents (path, &contents, NULL, &error));
	g_assert_no_error (error);
	lines = g_strsplit (contents, "\n", -1);
	g_mime_format_options_set_newline_format (options, GMIME_NEWLINE_FORMAT_UNIX);
	for (i = 0; lines[i]; i++) {
		if (!*lines[i] || lines[i][0] == '#')
			continue;
		g_strchomp (lines[i]);
		fields = g_strsplit (lines[i], "\t", -1);
		g_assert_cmpuint (g_strv_length (fields), ==, 5);
		input = g_base64_decode (fields[0], &length);
		exact = g_malloc (MAX (length, 1));
		memcpy (exact, input, length);
		padded = g_malloc (length + 32);
		memcpy (padded, input, length);
		memcpy (padded + length, "; outside=bad", 14);
		for (variant = 0; variant < 3; variant++) {
			if (variant == 2)
				padded[length] = '\0';
			results = g_mime_authentication_results_parse (variant ? padded : exact,
								      variant == 2 ? -1 : (gssize) length, &error);
			if (!strcmp (fields[1], "-")) {
				g_assert_null (results);
				g_assert_error (error, GMIME_ERROR, GMIME_ERROR_PARSE_ERROR);
				g_assert_nonnull (strstr (error->message, "offset"));
				g_clear_error (&error);
			} else {
				if (!results)
					g_printerr ("Reference vector %u rejected: %s\n", i + 1, error->message);
				g_assert_no_error (error);
				g_assert_nonnull (results);
				assert_encoded (g_mime_authentication_results_to_string (results), fields[1]);
				assert_encoded (snapshot (results), fields[2]);
				g_object_unref (results);
			}
		}
		if (strcmp (fields[1], "-")) {
			g_mime_format_options_set_max_line_length (options, 78);
			assert_encoded (g_mime_header_format_authentication_results (header, options, padded, NULL), fields[3]);
			g_mime_format_options_set_max_line_length (options, 60);
			assert_encoded (g_mime_header_format_authentication_results (header, options, padded, NULL), fields[4]);
		}
		g_assert_cmpmem (exact, length, input, length);
		g_free (exact);
		g_free (padded);
		g_free (input);
		g_strfreev (fields);
	}
	g_strfreev (lines);
	g_free (contents);
	g_object_unref (header);
	g_mime_format_options_unref (options);
}

static void
test_models (void)
{
	GMimeAuthenticationResults *results = g_mime_authentication_results_new ("example.com");
	GMimeAuthenticationMethodResult *result = g_mime_authentication_method_result_new ("dkim", "pass");
	GMimeAuthenticationMethodProperty *property = g_mime_authentication_method_property_new ("header", "b", "a/b==");
	GMimeAuthenticationMethodResult *weak_result = result;
	GMimeAuthenticationMethodProperty *weak_property = property;
	char *text;
	int number = 99;

	g_object_add_weak_pointer (G_OBJECT (result), (gpointer *) &weak_result);
	g_object_add_weak_pointer (G_OBJECT (property), (gpointer *) &weak_property);
	g_assert_false (g_mime_authentication_results_get_instance (results, &number));
	g_assert_cmpint (number, ==, 0);
	g_mime_authentication_results_set_instance (results, 0);
	g_assert_true (g_mime_authentication_results_get_instance (results, NULL));
	g_mime_authentication_results_set_version (results, 1);
	g_mime_authentication_method_result_set_version (result, G_MININT);
	g_assert_true (g_mime_authentication_method_result_get_version (result, &number));
	g_assert_cmpint (number, ==, G_MININT);
	g_mime_authentication_method_result_clear_version (result);
	g_assert_false (g_mime_authentication_method_result_get_version (result, &number));
	g_assert_cmpint (number, ==, 0);
	g_mime_authentication_results_add_result (results, result);
	g_mime_authentication_results_insert_result (results, 0, result);
	g_mime_authentication_results_set_result (results, 1, result);
	g_mime_authentication_method_result_add_property (result, property);
	g_mime_authentication_method_result_insert_property (result, 0, property);
	g_mime_authentication_method_result_set_property (result, 1, property);
	g_object_unref (property);
	g_object_unref (result);
	g_assert_cmpint (g_mime_authentication_results_get_result_count (results), ==, 2);
	g_assert_cmpint (g_mime_authentication_method_result_get_property_count (result), ==, 2);
	g_assert_cmpint (g_mime_authentication_results_index_of_result (results, result), ==, 0);
	g_assert_cmpint (g_mime_authentication_method_result_index_of_property (result, property), ==, 0);
	g_assert_true (g_mime_authentication_results_contains_result (results, result));
	g_assert_true (g_mime_authentication_method_result_contains_property (result, property));
	g_assert_true (g_mime_authentication_results_remove_result (results, result));
	g_assert_true (g_mime_authentication_method_result_remove_property (result, property));
	g_assert_nonnull (weak_result);
	g_assert_nonnull (weak_property);
	g_mime_authentication_method_result_set_result_comment (result, "verified (nested)");
	g_mime_authentication_method_result_set_result_comment (result, g_mime_authentication_method_result_get_result_comment (result));
	g_mime_authentication_method_result_set_reason (result, "a \"reason\"");
	g_mime_authentication_method_result_set_reason (result, g_mime_authentication_method_result_get_reason (result));
	text = g_mime_authentication_results_to_string (results);
	g_assert_cmpstr (text, ==, "i=0; example.com 1; dkim=pass (verified (nested)) reason=\"a \\\"reason\\\"\" header.b=\"a/b==\"");
	g_free (text);
	g_mime_authentication_method_result_set_result_comment (result, NULL);
	g_mime_authentication_method_result_set_reason (result, NULL);
	g_mime_authentication_method_result_remove_property_at (result, 0);
	g_assert_null (weak_property);
	g_mime_authentication_method_result_clear_properties (result);
	g_mime_authentication_results_remove_result_at (results, 0);
	g_assert_null (weak_result);
	g_mime_authentication_results_clear_results (results);
	g_mime_authentication_results_clear_instance (results);
	g_mime_authentication_results_clear_version (results);
	g_assert_false (g_mime_authentication_results_get_version (results, NULL));
	text = g_mime_authentication_results_to_string (results);
	g_assert_cmpstr (text, ==, "example.com; none");
	g_free (text);
	g_object_unref (results);

	results = parse ("x; dmarc=fail action=reject", -1);
	result = g_mime_authentication_results_get_result (results, 0);
	g_mime_authentication_method_result_set_reason (result, "policy");
	text = g_mime_authentication_method_result_to_string (result);
	g_assert_cmpstr (text, ==, "dmarc=fail reason=\"policy\"");
	g_free (text);
	g_mime_authentication_method_result_set_reason (result, "");
	text = g_mime_authentication_method_result_to_string (result);
	g_assert_cmpstr (text, ==, "dmarc=fail action=\"reject\"");
	g_free (text);
	g_object_unref (results);
}

static void
test_native (void)
{
	const char nul[] = "x; m=pass\0; a=fail";
	const char bad_utf8[] = "x; m=pass header.d=\xff";
	const char subrange[] = "garbage; i=1; spf=pass header.d=x";
	const char *value = strstr (subrange, "spf=");
	GMimeAuthenticationResults *results;
	GMimeAuthenticationMethodResult *result;
	GMimeAuthenticationMethodProperty *property;
	GError *error = NULL;
	GString *nested = g_string_new ("x; m=pass ");
	char *text;
	int i;

	g_assert_null (g_mime_authentication_results_parse (nul, sizeof (nul) - 1, &error));
	g_assert_error (error, GMIME_ERROR, GMIME_ERROR_PARSE_ERROR);
	g_clear_error (&error);
	results = parse (bad_utf8, sizeof (bad_utf8) - 1);
	result = g_mime_authentication_results_get_result (results, 0);
	property = g_mime_authentication_method_result_get_property (result, 0);
	g_assert_cmpstr (g_mime_authentication_method_property_get_value (property), ==, "\xef\xbf\xbd");
	g_object_unref (results);
	results = parse (value, strlen (value));
	g_assert_null (g_mime_authentication_results_get_authentication_service_identifier (results));
	g_assert_false (g_mime_authentication_results_get_instance (results, NULL));
	g_object_unref (results);
	for (i = 0; i < 10000; i++)
		g_string_append_c (nested, '(');
	g_string_append (nested, "verified");
	for (i = 0; i < 10000; i++)
		g_string_append_c (nested, ')');
	results = parse (nested->str, nested->len);
	text = g_mime_authentication_results_to_string (results);
	g_assert_cmpstr (text, ==, nested->str);
	g_free (text);
	g_object_unref (results);
	g_string_truncate (nested, nested->len - 1);
	g_assert_null (g_mime_authentication_results_parse (nested->str, nested->len, &error));
	g_assert_error (error, GMIME_ERROR, GMIME_ERROR_PARSE_ERROR);
	g_clear_error (&error);
	g_string_free (nested, TRUE);
}

static void
test_headers (void)
{
	const char raw[] = "Authentication-Results: x; gateway.spf (original) = pass\r\n";
	GMimeHeader *header = g_mime_header_parse (NULL, (const guint8 *) raw, strlen (raw), NULL);
	GMimeFormatOptions *options = g_mime_format_options_new ();
	GBytes *bytes = g_mime_header_get_raw_value (header);
	char *text, *fallback;
	gsize length;
	const char *data = g_bytes_get_data (bytes, &length);
	GString *unicode;
	int i;

	g_assert_cmpmem (data, length, strchr (raw, ':') + 1, strlen (strchr (raw, ':') + 1));
	g_bytes_unref (bytes);
	g_object_unref (header);
	header = g_mime_header_new ("Authentication-Results", "\"x\"; gateway.spf (discarded) = pass", NULL);
	bytes = g_mime_header_get_raw_value (header);
	data = g_bytes_get_data (bytes, &length);
	g_assert_nonnull (g_strstr_len (data, length, " x; gateway.spf=pass"));
	g_bytes_unref (bytes);
	g_mime_format_options_set_newline_format (options, GMIME_NEWLINE_FORMAT_UNIX);
	text = g_mime_header_format_authentication_results (header, options, "x; invalid=", NULL);
	fallback = g_mime_header_format_default (header, options, "x; invalid=", NULL);
	g_assert_cmpstr (text, ==, fallback);
	g_free (text);
	g_free (fallback);
	unicode = g_string_new ("\"");
	for (i = 0; i < 28; i++)
		g_string_append (unicode, "\xc3\xa9");
	g_string_append (unicode, "\"; none");
	text = g_mime_header_format_authentication_results (header, options, unicode->str, NULL);
	g_assert_true (g_str_has_prefix (text, "\n\t\""));
	g_assert_cmpuint (strlen (text + 1) - 1, <=, 78);
	g_free (text);
	g_string_free (unicode, TRUE);
	g_object_unref (header);
	header = g_mime_header_new ("ARC-Authentication-Results", "i=1; x; spf=pass", NULL);
	text = g_mime_header_format_authentication_results (header, options, "i=1; x; spf=pass", NULL);
	g_assert_cmpstr (text, ==, " i=1; x; spf=pass\n");
	g_free (text);
	g_object_unref (header);
	g_mime_format_options_unref (options);
}

int
main (int argc, char **argv)
{
	char *path;

	g_mime_init ();
	testsuite_init (argc, argv);
	path = argc > 2 ? g_build_filename (argv[argc - 1], "vectors.tsv", NULL) :
		g_build_filename ("data", "authentication-results", "vectors.tsv", NULL);
	testsuite_start ("Authentication-Results");
	testsuite_check ("Actual-source reference parsing, model fields, serialization and folding");
	test_vectors (path);
	testsuite_check_passed ();
	testsuite_check ("Mutable models, nullable numbers and retaining ordered collections");
	test_models ();
	testsuite_check_passed ();
	testsuite_check ("Bounded input, native text recovery and deep comments");
	test_native ();
	testsuite_check_passed ();
	testsuite_check ("Header formatter registration, raw preservation and fallback");
	test_headers ();
	testsuite_check_passed ();
	testsuite_end ();
	g_free (path);
	g_mime_shutdown ();
	return testsuite_exit ();
}
