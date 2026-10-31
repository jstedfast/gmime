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

#include <string.h>
#include <gmime/gmime.h>

#include "testsuite.h"

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
check_string (char *actual, const char *expected, const char *description)
{
	testsuite_check ("%s", description);
	if (!g_strcmp0 (actual, expected))
		testsuite_check_passed ();
	else
		testsuite_check_failed ("expected [%s], got [%s]", expected, actual ? actual : "(null)");
	g_free (actual);
}

static void
test_construction (void)
{
	char name[] = "filename", value[] = "report.txt";
	GMimeParam *param = g_mime_param_new (name, value), *clone;
	const char *stored;

	name[0] = 'X';
	value[0] = 'X';
	check (!strcmp (g_mime_param_get_name (param), "filename") &&
	       !strcmp (g_mime_param_get_value (param), "report.txt"), "constructor copies the name and value");
	check (g_mime_param_get_charset (param) == NULL && g_mime_param_get_lang (param) == NULL &&
	       g_mime_param_get_encoding_method (param) == GMIME_PARAM_ENCODING_METHOD_DEFAULT &&
	       !g_mime_param_get_always_quote (param), "default parameter formatting preferences");
	g_mime_param_set_charset (param, "XUTF-8");
	g_mime_param_set_lang (param, "x-en");
	g_mime_param_set_encoding_method (param, GMIME_PARAM_ENCODING_METHOD_RFC2047);
	g_mime_param_set_always_quote (param, TRUE);
	clone = g_mime_param_clone (param);
	check (clone != param && !strcmp (g_mime_param_get_name (clone), "filename") &&
	       !strcmp (g_mime_param_get_value (clone), "report.txt") &&
	       !strcmp (g_mime_param_get_charset (clone), "XUTF-8") &&
	       !strcmp (g_mime_param_get_lang (clone), "x-en") &&
	       g_mime_param_get_encoding_method (clone) == GMIME_PARAM_ENCODING_METHOD_RFC2047 &&
	       g_mime_param_get_always_quote (clone), "clone retains every formatting preference");
	g_mime_param_set_value (clone, g_mime_param_get_value (clone) + 7);
	g_mime_param_set_charset (clone, g_mime_param_get_charset (clone) + 1);
	g_mime_param_set_lang (clone, g_mime_param_get_lang (clone) + 2);
	check (!strcmp (g_mime_param_get_value (clone), "txt") &&
	       !strcmp (g_mime_param_get_charset (clone), "UTF-8") &&
	       !strcmp (g_mime_param_get_lang (clone), "en"), "setters safely copy aliased substrings");
	check (!strcmp (g_mime_param_get_value (param), "report.txt") &&
	       !strcmp (g_mime_param_get_charset (param), "XUTF-8") &&
	       !strcmp (g_mime_param_get_lang (param), "x-en"), "clone mutations leave the original independent");
	stored = g_mime_param_get_value (param);
	g_mime_param_set_value (param, stored);
	check (g_mime_param_get_value (param) == stored, "equivalent value assignment preserves storage");
	stored = g_mime_param_get_charset (param);
	g_mime_param_set_charset (param, stored);
	check (g_mime_param_get_charset (param) == stored, "equivalent charset assignment preserves storage");
	stored = g_mime_param_get_lang (param);
	g_mime_param_set_lang (param, stored);
	check (g_mime_param_get_lang (param) == stored, "equivalent language assignment preserves storage");
	check_string (g_mime_param_to_string (param), "filename=\"report.txt\"", "display strings always quote values");
	g_mime_param_set_value (param, "a\"b\\c\r\n\t\346\227\245\346\234\254");
	check_string (g_mime_param_to_string (param), "filename=\"a\\\"b\\\\c\r\n\t\346\227\245\346\234\254\"",
		      "display strings escape quotes and backslashes without transport encoding");
	g_mime_param_set_value (param, "");
	check_string (g_mime_param_to_string (param), "filename=\"\"", "empty parameter display value");
	g_object_unref (clone);
	g_object_unref (param);
}

static void
test_invalid_arguments (void)
{
	GMimeParam *param;
	GMimeParamList *list = g_mime_param_list_new ();
	const char *invalid[] = { NULL, "", "file name", "filename*", "X-\346\227\245\346\234\254" };
	char name[2] = { 0, 0 };
	guint i;

	for (i = 0; i < G_N_ELEMENTS (invalid); i++) {
		g_test_expect_message ("gmime", G_LOG_LEVEL_CRITICAL, "*param_name_is_valid*");
		param = g_mime_param_new (invalid[i], "value");
		g_test_assert_expected_messages ();
		check (param == NULL, "invalid parameter names fail explicitly");
		g_test_expect_message ("gmime", G_LOG_LEVEL_CRITICAL, "*param_name_is_valid*");
		g_mime_param_list_set_parameter (list, invalid[i], "value");
		g_test_assert_expected_messages ();
		check (g_mime_param_list_length (list) == 0, "invalid named insertion preserves the list");
	}
	for (i = 1; i < 256; i++) {
		gboolean valid = i > 32 && i < 127 && !strchr ("()<>@,;:\\\"/[]?=*'%", i);

		name[0] = (char) i;
		if (!valid)
			g_test_expect_message ("gmime", G_LOG_LEVEL_CRITICAL, "*param_name_is_valid*");
		param = g_mime_param_new (name, "");
		if (!valid)
			g_test_assert_expected_messages ();
		check ((param != NULL) == valid, "constructor enforces the complete ASCII attribute-character set");
		g_clear_object (&param);
	}
	g_test_expect_message ("gmime", G_LOG_LEVEL_CRITICAL, "*value != NULL*");
	param = g_mime_param_new ("filename", NULL);
	g_test_assert_expected_messages ();
	check (param == NULL, "NULL parameter values fail explicitly");
	g_test_expect_message ("gmime", G_LOG_LEVEL_CRITICAL, "*g_utf8_validate*");
	param = g_mime_param_new ("filename", "\377");
	g_test_assert_expected_messages ();
	check (param == NULL, "invalid UTF-8 parameter values fail explicitly");
	param = g_mime_param_new ("filename", "original");
	g_test_expect_message ("gmime", G_LOG_LEVEL_CRITICAL, "*g_utf8_validate*");
	g_mime_param_set_value (param, "\377");
	g_test_assert_expected_messages ();
	check (!strcmp (g_mime_param_get_value (param), "original"), "invalid mutation preserves the value");
	g_object_unref (param);
	g_object_unref (list);
}

static void
test_lists (void)
{
	GMimeParamList *list = g_mime_param_list_new ();
	GMimeParam *abc = g_mime_param_new ("abc", "0"), *def = g_mime_param_new ("def", "1");
	GMimeParam *ghi = g_mime_param_new ("ghi", "2"), *replacement = g_mime_param_new ("ABC", "3");
	GMimeParam *renamed = g_mime_param_new ("xyz", "4"), *clone = g_mime_param_clone (abc);

	check_string (g_mime_param_list_to_string (list), "", "empty parameter list display string");
	check (g_mime_param_list_add (list, abc) == 0, "append returns the added index");
	g_mime_param_list_insert (list, 0, def);
	g_mime_param_list_insert (list, 2, ghi);
	check_string (g_mime_param_list_to_string (list), "; def=\"1\"; abc=\"0\"; ghi=\"2\"",
		      "insertion preserves order and accepts the end index");
	check (g_mime_param_list_contains (list, "AbC") &&
	       g_mime_param_list_index_of (list, "ABC") == 1 &&
	       g_mime_param_list_get_parameter (list, "aBc") == abc, "name lookup is case-insensitive");
	check (g_mime_param_list_contains_parameter (list, abc) &&
	       g_mime_param_list_index_of_parameter (list, ghi) == 2 &&
	       !g_mime_param_list_contains_parameter (list, clone) &&
	       g_mime_param_list_index_of_parameter (list, clone) == -1, "object lookup uses reference identity");
	check (!g_mime_param_list_contains (list, "missing") &&
	       g_mime_param_list_get_parameter (list, "missing") == NULL, "absent name lookup");
	g_test_expect_message ("gmime", G_LOG_LEVEL_CRITICAL, "*g_mime_param_list_index_of*");
	check (g_mime_param_list_add (list, replacement) == -1, "addition rejects differently cased duplicate names");
	g_test_assert_expected_messages ();
	g_test_expect_message ("gmime", G_LOG_LEVEL_CRITICAL, "*g_mime_param_list_index_of*");
	g_mime_param_list_insert (list, 0, replacement);
	g_test_assert_expected_messages ();
	g_mime_param_set_always_quote (abc, TRUE);
	g_mime_param_list_set_parameter (list, "ABC", "updated");
	check (g_mime_param_list_get_parameter_at (list, 1) == abc &&
	       g_mime_param_get_always_quote (abc) && !strcmp (g_mime_param_get_name (abc), "abc") &&
	       !strcmp (g_mime_param_get_value (abc), "updated"), "named assignment preserves the object, spelling, and metadata");
	g_mime_param_list_set_parameter_at (list, 1, replacement);
	check (g_mime_param_list_get_parameter (list, "abc") == replacement &&
	       !g_mime_param_list_contains_parameter (list, abc), "same-name replacement retains the new object");
	g_mime_param_list_set_parameter_at (list, 1, renamed);
	check (!g_mime_param_list_contains (list, "ABC") &&
	       g_mime_param_list_get_parameter (list, "XYZ") == renamed, "replacement can introduce a different unique name");
	g_test_expect_message ("gmime", G_LOG_LEVEL_CRITICAL, "*g_mime_param_list_index_of*");
	check (g_mime_param_list_add (list, def) == -1, "duplicate object addition fails");
	g_test_assert_expected_messages ();
	g_test_expect_message ("gmime", G_LOG_LEVEL_CRITICAL, "*g_mime_param_list_index_of*");
	g_mime_param_list_insert (list, 1, def);
	g_test_assert_expected_messages ();
	g_test_expect_message ("gmime", G_LOG_LEVEL_CRITICAL, "*g_mime_param_list_index_of*");
	g_mime_param_list_set_parameter_at (list, 1, ghi);
	g_test_assert_expected_messages ();
	g_mime_param_list_set_parameter_at (list, 1, renamed);
	check_string (g_mime_param_list_to_string (list), "; def=\"1\"; xyz=\"4\"; ghi=\"2\"",
		      "duplicate-name failures and same-object replacement leave the list unchanged");
	g_test_expect_message ("gmime", G_LOG_LEVEL_CRITICAL, "*index >= 0*");
	g_mime_param_list_insert (list, -1, abc);
	g_test_assert_expected_messages ();
	g_test_expect_message ("gmime", G_LOG_LEVEL_CRITICAL, "*index >= 0*");
	g_mime_param_list_insert (list, 4, abc);
	g_test_assert_expected_messages ();
	g_test_expect_message ("gmime", G_LOG_LEVEL_CRITICAL, "*index >= 0*");
	g_mime_param_list_set_parameter_at (list, 3, abc);
	g_test_assert_expected_messages ();
	check (g_mime_param_list_length (list) == 3, "invalid indices preserve membership");
	check (!g_mime_param_list_remove_parameter (list, clone) &&
	       g_mime_param_list_remove (list, "DEF") &&
	       g_mime_param_list_remove_parameter (list, renamed) &&
	       g_mime_param_list_remove_at (list, 0), "removal by name, object identity, and index");
	check (!g_mime_param_list_remove_at (list, 0) &&
	       !g_mime_param_list_remove (list, "missing"), "absent removals return FALSE");
	g_mime_param_list_set_parameter (list, "new", "value");
	check_string (g_mime_param_list_to_string (list), "; new=\"value\"", "named assignment appends an absent parameter");
	g_mime_param_list_clear (list);
	check (g_mime_param_list_length (list) == 0, "clear removes all parameters");
	g_object_unref (list);
	g_object_unref (abc);
	g_object_unref (def);
	g_object_unref (ghi);
	g_object_unref (replacement);
	g_object_unref (renamed);
	g_object_unref (clone);
}

static void
test_ownership (void)
{
	GMimeParamList *first = g_mime_param_list_new (), *second = g_mime_param_list_new ();
	GMimeParam *param = g_mime_param_new ("filename", "shared");
	GMimeParam *replacement;
	gpointer weak = param;

	g_object_add_weak_pointer (G_OBJECT (param), &weak);
	g_mime_param_list_add (first, param);
	g_mime_param_list_insert (second, 0, param);
	g_object_unref (param);
	check (weak != NULL, "lists retain references to inserted objects");
	g_mime_param_list_clear (first);
	g_object_unref (first);
	g_mime_param_set_value (param, "still shared");
	check (g_mime_param_list_get_parameter_at (second, 0) == param &&
	       !strcmp (g_mime_param_get_value (param), "still shared"), "clearing one list preserves shared parameters");
	g_mime_param_list_remove_at (second, 0);
	check (weak == NULL, "removal releases the final reference");
	param = g_mime_param_new ("filename", "old");
	weak = param;
	g_object_add_weak_pointer (G_OBJECT (param), &weak);
	g_mime_param_list_add (second, param);
	g_object_unref (param);
	replacement = g_mime_param_new ("filename", "replacement");
	g_mime_param_list_set_parameter_at (second, 0, replacement);
	check (weak == NULL, "replacement releases the old parameter reference");
	weak = replacement;
	g_object_add_weak_pointer (G_OBJECT (replacement), &weak);
	g_object_unref (replacement);
	check (weak != NULL, "replacement retains the new parameter reference");
	g_mime_param_list_clear (second);
	check (weak == NULL, "clear releases the replacement reference");
	param = g_mime_param_new ("filename", "detached");
	g_mime_param_list_add (second, param);
	g_object_unref (second);
	g_mime_param_set_value (param, "after list finalization");
	check (!strcmp (g_mime_param_get_value (param), "after list finalization"),
	       "list finalization detaches its parameter change subscriptions");
	g_object_unref (param);
}

static void
test_headers (void)
{
	const char *raw = " text/plain;\r\n\tname=\"original.txt\"\r\n";
	const char *disposition_raw = " attachment;\r\n\tname=\"replacement.txt\"\r\n";
	GMimePart *first = g_mime_part_new (), *second = g_mime_part_new ();
	GMimeEntity *entity = GMIME_ENTITY (first);
	GMimeHeader *header = g_mime_header_list_get_header (g_mime_entity_get_header_list (entity), "Content-Type");
	GMimeParamList *list, *other;
	GMimeParam *param, *replacement;

	testsuite_header_set_raw_text (header, raw);
	list = g_mime_content_type_get_parameters (g_mime_entity_get_content_type (entity));
	param = g_mime_param_list_get_parameter (list, "name");
	g_mime_param_set_value (param, "original.txt");
	g_mime_param_set_charset (param, g_mime_param_get_charset (param));
	g_mime_param_set_lang (param, g_mime_param_get_lang (param));
	g_mime_param_set_encoding_method (param, g_mime_param_get_encoding_method (param));
	g_mime_param_set_always_quote (param, g_mime_param_get_always_quote (param));
	g_mime_param_list_set_parameter (list, "NAME", g_mime_param_get_value (param));
	g_mime_param_list_set_parameter_at (list, 0, param);
	check (!strcmp (testsuite_header_raw_text (header), raw), "equivalent assignments preserve untouched raw header folding");
	g_object_ref (param);
	other = g_mime_content_type_get_parameters (g_mime_entity_get_content_type (GMIME_ENTITY (second)));
	g_mime_param_list_add (other, param);
	g_mime_param_set_value (param, "shared.txt");
	check (strstr (testsuite_header_raw_text (header), "shared.txt") != NULL &&
	       strstr (g_mime_entity_get_header (GMIME_ENTITY (second), "Content-Type"), "shared.txt") != NULL,
	       "shared parameter changes update both owning headers");
	replacement = g_mime_param_clone (param);
	g_mime_param_set_value (replacement, "replacement.txt");
	g_mime_param_list_set_parameter_at (list, 0, replacement);
	g_mime_param_set_value (param, "second.txt");
	check (strstr (testsuite_header_raw_text (header), "replacement.txt") != NULL &&
	       strstr (g_mime_entity_get_header (GMIME_ENTITY (second), "Content-Type"), "second.txt") != NULL,
	       "replacing a parameter transfers the owning header subscription");
	check (g_mime_param_list_remove (list, "NAME") &&
	       strstr (testsuite_header_raw_text (header), "name=") == NULL, "name removal updates the owning Content-Type header");
	g_mime_param_list_add (list, replacement);
	check (strstr (testsuite_header_raw_text (header), "replacement.txt") != NULL, "object addition updates the owning header");
	check (g_mime_param_list_remove_at (list, 0) &&
	       strstr (testsuite_header_raw_text (header), "name=") == NULL, "indexed removal updates the owning header");
	g_mime_entity_set_disposition (entity, "attachment");
	list = g_mime_content_disposition_get_parameters (g_mime_entity_get_content_disposition (entity));
	g_mime_param_list_insert (list, 0, replacement);
	check (strstr (g_mime_entity_get_header (entity, "Content-Disposition"), "replacement.txt") != NULL &&
	       g_mime_param_list_remove_parameter (list, replacement) &&
	       strstr (g_mime_entity_get_header (entity, "Content-Disposition"), "name=") == NULL,
	       "object insertion and removal update Content-Disposition headers");
	header = g_mime_header_list_get_header (g_mime_entity_get_header_list (entity), "Content-Disposition");
	testsuite_header_set_raw_text (header, disposition_raw);
	list = g_mime_content_disposition_get_parameters (g_mime_entity_get_content_disposition (entity));
	g_mime_param_list_set_parameter (list, "NAME", "replacement.txt");
	check (!strcmp (testsuite_header_raw_text (header), disposition_raw),
	       "equivalent disposition parameter assignments preserve raw header folding");
	g_mime_param_list_clear (list);
	check (strstr (testsuite_header_raw_text (header), "name=") == NULL,
	       "clearing parameters updates the owning disposition header");
	g_object_unref (first);
	g_object_unref (second);
	g_mime_param_set_value (param, "detached.txt");
	g_mime_param_set_value (replacement, "detached.txt");
	g_object_unref (param);
	g_object_unref (replacement);
}

static void
test_parsed_parameters (void)
{
	GMimeParamList *list = g_mime_param_list_parse (NULL,
		"filename*=utf-8'en'%E6%97%A5%E6%9C%AC.txt; Name=first; name=second");
	GMimeParam *param = g_mime_param_list_get_parameter (list, "FILENAME");
	GMimeParam *clone = g_mime_param_clone (param);

	check (!strcmp (g_mime_param_get_value (clone), "\346\227\245\346\234\254.txt") &&
	       !g_strcmp0 (g_mime_param_get_charset (clone), g_mime_param_get_charset (param)) &&
	       !strcmp (g_mime_param_get_lang (clone), "en") &&
	       g_mime_param_get_encoding_method (clone) == GMIME_PARAM_ENCODING_METHOD_RFC2231,
	       "parsed parameter clones preserve decoded text and wire metadata");
	check (g_mime_param_list_length (list) == 3 &&
	       !strcmp (g_mime_param_get_value (g_mime_param_list_get_parameter (list, "NAME")), "first"),
	       "legacy parsing retains duplicate parameters and first-name lookup");
	check_string (g_mime_param_list_to_string (list),
		      "; filename=\"\346\227\245\346\234\254.txt\"; Name=\"first\"; name=\"second\"",
		      "parsed list display preserves order, spelling, and Unicode");
	g_object_unref (clone);
	g_object_unref (list);
	list = g_mime_param_list_parse (NULL, "filename*1*=.txt; filename*0*=utf-8'en'%E6%97%A5%E6%9C%AC");
	check (g_mime_param_list_length (list) == 1 &&
	       !strcmp (g_mime_param_get_value (g_mime_param_list_get_parameter_at (list, 0)),
			"\346\227\245\346\234\254.txt"), "private parser insertion still merges out-of-order continuations");
	g_object_unref (list);
	list = g_mime_param_list_parse (NULL, "; ; name=first; NAME=last; broken=");
	check (list && g_mime_param_list_length (list) == 2 &&
	       !strcmp (g_mime_param_get_value (g_mime_param_list_get_parameter (list, "name")), "first"),
	       "legacy parsing still skips empty entries and retains duplicates before an invalid suffix");
	g_clear_object (&list);
}

static void
test_large_parameter_charsets (void)
{
	char *charset = g_strnfill (16 * 1024 * 1024, 'x');
	char *text;
	GMimeParamList *list;
	GMimeParam *param;
	guint i;

	for (i = 0; i < 3; i++) {
		if (i == 0)
			text = g_strconcat ("filename*=", charset, "'en'caf%E9.txt", NULL);
		else if (i == 1)
			text = g_strconcat ("filename*0*=", charset, "'en'caf%E9; filename*1*=.txt", NULL);
		else
			text = g_strconcat ("filename*=", charset, "'en", NULL);

		list = g_mime_param_list_parse (NULL, text);
		g_assert_nonnull (list);
		param = g_mime_param_list_get_parameter (list, "filename");
		g_assert_nonnull (param);
		if (i < 2) {
			check (!strcmp (g_mime_param_get_value (param), "caf\303\251.txt") &&
			       !strcmp (g_mime_param_get_lang (param), "en") &&
			       !strcmp (g_mime_param_get_charset (param), "iso-8859-1"),
			       "oversized unknown parameter charsets retain fallback decoding and language");
		} else {
			const char *value = g_mime_param_get_value (param);

			check (strlen (value) == strlen (charset) + 3 &&
			       !memcmp (value, charset, strlen (charset)) && g_str_has_suffix (value, "'en") &&
			       g_mime_param_get_lang (param) == NULL,
			       "missing second charset delimiter retains the original parameter value");
		}
		g_object_unref (list);
		g_free (text);
	}
	g_free (charset);
}

static void
test_checked_parsing (void)
{
	static const struct {
		const char *text, *display;
	} cases[] = {
		{ "", "" },
		{ " (empty (nested)) ; ;\t;", "" },
		{ "name=;", "; name=\"\"" },
		{ "name=\"\"", "; name=\"\"" },
		{ "; name = (comment) \"value\"; ; other=last;",
		  "; name=\"value\"; other=\"last\"" },
		{ "name (a) * (b) 0 (c) * (d) = (e) utf-8'en'%E2%82%AC",
		  "; name=\"\342\202\254\"" },
		{ "Name=first; other=middle; name=last", "; Name=\"first\"; other=\"middle\"" },
		{ "Name=first; other=middle; NAME*=utf-8'en'%E2%82%AC",
		  "; Name=\"\342\202\254\"; other=\"middle\"" },
		{ "Name*=utf-8'en'%E2%82%AC; other=middle; NAME=last",
		  "; Name=\"\342\202\254\"; other=\"middle\"" },
		{ "Name*=utf-8'en'%E2%82%AC; NAME*=iso-8859-1'fr'%E9",
		  "; Name=\"\303\251\"" },
		{ "name=fallback; other=middle; NAME*1*=AC; name*0*=utf-8'en'%E2%82%",
		  "; name=\"\342\202\254\"; other=\"middle\"" },
		{ "name*2147483647=last; NAME*0=first", "; name=\"firstlast\"" },
		{ "name*0*=utf-8''%E2%82; NAME*2*=%AC", "; name=\"\342\202\254\"" },
		{ "name*0*=\"utf-8''%E2%82\"; NAME*1*=\"%AC\"", "; name=\"\342\202\254\"" },
		{ "name*=\"x-unsupported''%E9\"", "; name=\"\303\251\"" },
		{ "name*=us-ascii'incomplete%20declaration", "; name=\"us-ascii'incomplete declaration\"" },
		{ "name*0=\"a\\\\b\\\"c\r\n\td\"; name*1=.txt", "; name=\"a\\\\b\\\"c\td.txt\"" },
		{ "name=\"a\\\\b\\\"c\r\n\td\"", "; name=\"a\\\\b\\\"c\td\"" },
		{ "name=\"=?utf-8?q?caf=C3=A9?=\"", "; name=\"caf\303\251\"" },
		{ "name=\"=?iso-8859-1?q?caf=E9?=\"", "; name=\"caf\303\251\"" },
		{ "name=\"=?not-an-encoded-word\"", "; name=\"=?not-an-encoded-word\"" }
	};
	GMimeParserOptions *options = g_mime_parser_options_new ();
	GMimeParamList *list;
	GMimeParam *param;
	GError *error = NULL;
	guint i, mode;

	for (mode = GMIME_RFC_COMPLIANCE_LOOSE; mode <= GMIME_RFC_COMPLIANCE_LOOSER; mode++) {
		g_mime_parser_options_set_parameter_compliance_mode (options, mode);
		for (i = 0; i < G_N_ELEMENTS (cases); i++) {
			list = g_mime_param_list_parse_full (options, cases[i].text, -1, &error);
			check (list != NULL && error == NULL, cases[i].text);
			if (list) {
				check_string (g_mime_param_list_to_string (list), cases[i].display, cases[i].text);
				g_object_unref (list);
			}
			g_clear_error (&error);
		}
	}

	list = g_mime_param_list_parse_full (NULL, "name=plain; empty=\"\"; bare=;", -1, &error);
	check (list && error == NULL &&
	       g_mime_param_get_encoding_method (g_mime_param_list_get_parameter (list, "name")) ==
	       GMIME_PARAM_ENCODING_METHOD_RFC2047 &&
	       g_mime_param_get_encoding_method (g_mime_param_list_get_parameter (list, "empty")) ==
	       GMIME_PARAM_ENCODING_METHOD_RFC2047 &&
	       g_mime_param_get_encoding_method (g_mime_param_list_get_parameter (list, "bare")) ==
	       GMIME_PARAM_ENCODING_METHOD_DEFAULT, "ordinary and empty values retain MimeKit's parsing preferences");
	g_clear_object (&list);
	list = g_mime_param_list_parse_full (NULL, "name=\"=?utf-8?q?caf=C3=A9?=\"", -1, &error);
	param = g_mime_param_list_get_parameter (list, "name");
	check (error == NULL && g_mime_param_get_charset (param) == NULL &&
	       g_mime_param_get_encoding_method (param) == GMIME_PARAM_ENCODING_METHOD_RFC2047,
	       "checked RFC 2047 parsing preserves the method without a redundant UTF-8 preference");
	g_object_unref (list);
	list = g_mime_param_list_parse_full (NULL, "name=\"=?not-an-encoded-word\"", -1, &error);
	param = g_mime_param_list_get_parameter (list, "name");
	check (error == NULL && g_mime_param_get_encoding_method (param) == GMIME_PARAM_ENCODING_METHOD_RFC2047,
	       "ordinary decoded values retain MimeKit's RFC 2047 formatting preference");
	g_object_unref (list);
	list = g_mime_param_list_parse_full (NULL, "Name=fallback; name*=iso-8859-1'fr'%E9", -1, &error);
	param = g_mime_param_list_get_parameter (list, "name");
	check (error == NULL && !strcmp (g_mime_param_get_name (param), "Name") &&
	       !g_ascii_strcasecmp (g_mime_param_get_charset (param), "iso-8859-1") &&
	       !strcmp (g_mime_param_get_lang (param), "fr") &&
	       g_mime_param_get_encoding_method (param) == GMIME_PARAM_ENCODING_METHOD_RFC2231,
	       "RFC 2231 precedence preserves the winning charset, language, and encoding method");
	g_mime_param_set_value (param, "updated");
	check_string (g_mime_param_list_to_string (list), "; Name=\"updated\"",
		      "normalized parsed parameters remain mutable after temporary lists are released");
	check (g_mime_param_list_remove_parameter (list, param) && g_mime_param_list_length (list) == 0,
	       "normalized parsed parameters retain ordinary list ownership");
	g_object_unref (list);

	for (mode = GMIME_RFC_COMPLIANCE_LOOSE; mode <= GMIME_RFC_COMPLIANCE_LOOSER; mode++) {
		g_mime_parser_options_set_parameter_compliance_mode (options, mode);
		list = g_mime_param_list_parse_full (options, "name=\"first\" other=last", -1, &error);
		if (mode == GMIME_RFC_COMPLIANCE_STRICT) {
			check (list == NULL && error && error->code == GMIME_ERROR_PARSE_ERROR &&
			       g_str_has_suffix (error->message, "at byte offset 13."),
			       "strict parsing rejects missing semicolons after quoted values");
		} else {
			check (list && error == NULL && g_mime_param_list_length (list) == 2,
			       "loose parsing recovers a missing semicolon after a quoted value");
		}
		g_clear_object (&list);
		g_clear_error (&error);
		list = g_mime_param_list_parse_full (options, "name=two words\r\n\tcontinued  ; other=last", -1, &error);
		if (mode == GMIME_RFC_COMPLIANCE_STRICT) {
			check (list == NULL && error && error->code == GMIME_ERROR_PARSE_ERROR,
			       "strict parsing rejects unquoted spaces");
		} else {
			check (list && error == NULL, "loose parsing accepts unquoted folded values");
			if (list)
				check_string (g_mime_param_list_to_string (list),
					      "; name=\"two words\tcontinued\"; other=\"last\"",
					      "loose values are unfolded and trailing whitespace is trimmed");
		}
		g_clear_object (&list);
		g_clear_error (&error);
		list = g_mime_param_list_parse_full (options, "name=\303\251; other=last", -1, &error);
		check (mode == GMIME_RFC_COMPLIANCE_STRICT ?
		       list == NULL && error && error->code == GMIME_ERROR_PARSE_ERROR :
		       list && error == NULL && g_mime_param_list_length (list) == 2,
		       "strict parsing requires quoting around raw UTF-8 values");
		g_clear_object (&list);
		g_clear_error (&error);
	}
	list = g_mime_param_list_parse_full (NULL, "x=y2147483648 =a", -1, &error);
	check (list && error == NULL, "loose parsing accepts spaces within unquoted values");
	if (list)
		check_string (g_mime_param_list_to_string (list), "; x=\"y2147483648 =a\"",
			      "loose parsing never truncates real characters when trimming whitespace");
	g_clear_object (&list);
	g_mime_parser_options_unref (options);
}

static void
test_parameter_comments_and_empty_charsets (void)
{
	/* Regression cases inspired by https://github.com/noxxi/mime-is-broken. */
	static const struct {
		const char *text, *value;
		gboolean loose;
	} cases[] = {
		{ "name*=''file.zip", "file.zip", FALSE },
		{ "name*='en'file%2Ezip", "file.zip", FALSE },
		{ "name*=''caf%E9%27s%20file.zip", "caf\303\251's file.zip", FALSE },
		{ "name*=''", "", FALSE },
		{ "name*='en'", "", FALSE },
		{ "name*0*=''file; name*1*=%2Ezip", "file.zip", FALSE },
		{ "name*0*='en'file; name*1=.zip", "file.zip", FALSE },
		{ "name*0*=''; name*1*=file%2Ezip", "file.zip", FALSE },
		{ "name*0*='en'caf%E9; name*1*=%27s%20file.zip", "caf\303\251's file.zip", FALSE },
		{ "name*=\"''file%2Ezip\"", "file.zip", FALSE },
		{ "name*='file%2Ezip", "'file.zip", FALSE },
		{ "name=value(comment)", "value", FALSE },
		{ "name=value (comment) (another)", "value", FALSE },
		{ "name=value(nested (comment); ignored=yes)", "value", FALSE },
		{ "name=value(escaped \\) comment)", "value", FALSE },
		{ "name=value(comment\r\n\tcontinued)", "value", FALSE },
		{ "name=value\r\n\t(comment)", "value", FALSE },
		{ "name=\"value(literal)\"(comment)", "value(literal)", FALSE },
		{ "name*=us-ascii''value%28literal%29(comment)", "value(literal)", FALSE },
		{ "name*0=value(comment); name*1=.zip(other)", "value.zip", FALSE },
		{ "name*0*=''value(comment); name*1*=%2Ezip(other)", "value.zip", FALSE },
		{ "name=Test Name.pdf (comment)", "Test Name.pdf", TRUE },
		{ "name=Test\r\n\tName.pdf(comment)", "Test\tName.pdf", TRUE },
		{ "name=Test Name.pdf\r\n\t(comment)", "Test Name.pdf", TRUE },
		{ "name=Test\r\n\tName.pdf\r\n\t(comment)", "Test\tName.pdf", TRUE }
	};
	GMimeParserOptions *options = g_mime_parser_options_new ();
	GMimeParamList *list;
	GMimeParam *param;
	GError *error = NULL;
	guint mode, i, suffix, api;
	char *text;
	gsize length;

	for (mode = GMIME_RFC_COMPLIANCE_LOOSE; mode <= GMIME_RFC_COMPLIANCE_LOOSER; mode++) {
		g_mime_parser_options_set_parameter_compliance_mode (options, mode);
		for (i = 0; i < G_N_ELEMENTS (cases); i++) {
			if (cases[i].loose && mode == GMIME_RFC_COMPLIANCE_STRICT)
				continue;
			for (suffix = 0; suffix < 2; suffix++) {
				for (api = 0; api < 3; api++) {
					if (api == 2 && mode == GMIME_RFC_COMPLIANCE_LOOSER)
						continue;
					length = strlen (cases[i].text) + (suffix ? strlen ("; next=ok") : 0);
					text = g_strconcat ("prefix", cases[i].text, suffix ? "; next=ok" : "",
							    api == 0 ? "; ignored=yes" : "", NULL);
					if (api == 0)
						list = g_mime_param_list_parse_full (options, text + 6, length, &error);
					else if (api == 1)
						list = g_mime_param_list_parse_full (options, text + 6, -1, NULL);
					else
						list = g_mime_param_list_parse (options, text + 6);
					check (list != NULL && error == NULL, cases[i].text);
					if (list) {
						check (g_mime_param_list_length (list) == (int) suffix + 1, cases[i].text);
						param = g_mime_param_list_get_parameter (list, "name");
						check_string (param ? g_strdup (g_mime_param_get_value (param)) : NULL,
							      cases[i].value, cases[i].text);
						if (suffix) {
							param = g_mime_param_list_get_parameter (list, "next");
							check_string (param ? g_strdup (g_mime_param_get_value (param)) : NULL,
								      "ok", cases[i].text);
						}
						g_object_unref (list);
					}
					g_clear_error (&error);
					g_free (text);
				}
			}
		}
	}
	g_mime_parser_options_unref (options);
}

static void
test_checked_parse_errors (void)
{
	static const struct {
		const char *text;
		gsize offset;
	} cases[] = {
		{ "name", 4 },
		{ "name*", 5 },
		{ "name*0", 6 },
		{ "name*0*", 7 },
		{ "name=", 5 },
		{ "name= (comment)", 15 },
		{ "name*bogus=value", 5 },
		{ "name**=value", 5 },
		{ "name*2147483648=value", 14 },
		{ "name*99999999999999999999=value", 14 },
		{ "%name=value", 0 },
		{ "\303\251=value", 0 },
		{ "(comment", 8 },
		{ "name(comment", 12 },
		{ "name*(comment", 13 },
		{ "name*0(comment", 14 },
		{ "name*0*(comment", 15 },
		{ "name=(comment", 13 },
		{ "name=value(comment", 18 },
		{ "name=value\r\n\t(comment", 21 },
		{ "name=\"unterminated", 18 },
		{ "name=\"escape\\", 13 },
		{ "name=\"value\"(comment", 20 },
		{ "first*0*=utf-8''%E2; first*1*=%82%AC; broken=", 45 },
		{ "first=good; second=\"unterminated", 32 },
		{ "first=\"\303\251\"; broken=", 19 }
	};
	GMimeParserOptions *options = g_mime_parser_options_new ();
	GMimeParamList *list;
	GError *error = NULL;
	guint i, mode;
	char *ending;

	for (mode = GMIME_RFC_COMPLIANCE_LOOSE; mode <= GMIME_RFC_COMPLIANCE_LOOSER; mode++) {
		g_mime_parser_options_set_parameter_compliance_mode (options, mode);
		for (i = 0; i < G_N_ELEMENTS (cases); i++) {
			list = g_mime_param_list_parse_full (options, cases[i].text, -1, &error);
			ending = g_strdup_printf ("at byte offset %" G_GSIZE_FORMAT ".", cases[i].offset);
			check (list == NULL && error && error->domain == GMIME_ERROR &&
			       error->code == GMIME_ERROR_PARSE_ERROR && g_str_has_suffix (error->message, ending),
			       cases[i].text);
			g_free (ending);
			g_clear_object (&list);
			g_clear_error (&error);
			list = g_mime_param_list_parse_full (options, cases[i].text, -1, NULL);
			check (list == NULL, "checked parsing fails without an error output");
			g_clear_object (&list);
		}
	}
	g_mime_parser_options_unref (options);
}

static void
test_checked_parse_bounds (void)
{
	const char bounded[] = { 'n', 'a', 'm', 'e', '=', 'x' };
	const char embedded[] = { 'n', '=', 'x', '\0', ';', 'n', '=', 'y' };
	GMimeParamList *list;
	GError *error = NULL;
	guint length;

	list = g_mime_param_list_parse_full (NULL, bounded, sizeof (bounded), &error);
	check (list && error == NULL, "bounded input need not have a trailing NUL");
	if (list)
		check_string (g_mime_param_list_to_string (list), "; name=\"x\"", "bounded input stops at the supplied length");
	g_clear_object (&list);
	list = g_mime_param_list_parse_full (NULL, embedded, sizeof (embedded), &error);
	check (list == NULL && error && error->code == GMIME_ERROR_PARSE_ERROR &&
	       g_str_has_suffix (error->message, "at byte offset 3."), "bounded input rejects embedded NUL bytes");
	g_clear_object (&list);
	g_clear_error (&error);
	list = g_mime_param_list_parse_full (NULL, embedded, -1, &error);
	check (list && error == NULL && g_mime_param_list_length (list) == 1, "-1 length stops at the first NUL");
	g_clear_object (&list);
	list = g_mime_param_list_parse_full (NULL, bounded, 0, &error);
	check (list && error == NULL && g_mime_param_list_length (list) == 0, "zero length parses an empty list");
	g_clear_object (&list);
	for (length = 1; length < sizeof (bounded); length++) {
		list = g_mime_param_list_parse_full (NULL, bounded, length, &error);
		check (list == NULL && error && error->code == GMIME_ERROR_PARSE_ERROR,
		       "bounded incomplete tokens fail without reading past the boundary");
		g_clear_object (&list);
		g_clear_error (&error);
	}
#if GLIB_SIZEOF_SIZE_T > 4
	{
		list = g_mime_param_list_parse_full (NULL, bounded, (gssize) G_MAXINT + 1, &error);
		check (list == NULL && error && error->code == GMIME_ERROR_PARSE_ERROR,
		       "oversized lengths are rejected before reading the input");
		g_clear_object (&list);
		g_clear_error (&error);
	}
#endif
	g_test_expect_message ("gmime", G_LOG_LEVEL_CRITICAL, "*str != NULL*");
	list = g_mime_param_list_parse_full (NULL, NULL, -1, &error);
	g_test_assert_expected_messages ();
	check (list == NULL && error == NULL, "NULL parser input is a programming error");
	g_test_expect_message ("gmime", G_LOG_LEVEL_CRITICAL, "*length >= -1*");
	list = g_mime_param_list_parse_full (NULL, bounded, -2, &error);
	g_test_assert_expected_messages ();
	check (list == NULL && error == NULL, "invalid negative lengths are programming errors");
	g_set_error_literal (&error, GMIME_ERROR, GMIME_ERROR_GENERAL, "existing error");
	g_test_expect_message ("gmime", G_LOG_LEVEL_CRITICAL, "*error == NULL || *error == NULL*");
	list = g_mime_param_list_parse_full (NULL, bounded, sizeof (bounded), &error);
	g_test_assert_expected_messages ();
	check (list == NULL && error->code == GMIME_ERROR_GENERAL, "checked parsing does not overwrite an existing error");
	g_clear_error (&error);
}

static GObject *
parse_content_field (gboolean is_type, GMimeParserOptions *options, const char *text, gssize length, GError **error)
{
	if (is_type)
		return (GObject *) g_mime_content_type_parse_full (options, text, length, error);

	return (GObject *) g_mime_content_disposition_parse_full (options, text, length, error);
}

static void
test_checked_content_fields (void)
{
	static const struct {
		gboolean is_type;
		const char *text, *value, *parameters;
	} cases[] = {
		{ TRUE, "text/plain", "text/plain", "" },
		{ TRUE, " (a (b)) TEXT (c) / (d) Plain (e) ; (f) ;", "TEXT/Plain", "" },
		{ TRUE, "application/x/type", "application/x/type", "" },
		{ TRUE, "x?=type/x.y", "x?=type/x.y", "" },
		{ TRUE, "text/plain; Name=first; name=last", "text/plain", "; Name=\"first\"" },
		{ TRUE, "text/plain; name=first; NAME*=utf-8'en'%E2%82%AC",
		  "text/plain", "; name=\"\342\202\254\"" },
		{ FALSE, "attachment", "attachment", "" },
		{ FALSE, "(a (b)) InLine (c) ; (d) ;", "InLine", "" },
		{ FALSE, "x/type?=value", "x/type?=value", "" },
		{ FALSE, "\303\251", "??", "" },
		{ FALSE, "attachment; filename=\"=?utf-8?q?caf=C3=A9?=\"",
		  "attachment", "; filename=\"caf\303\251\"" },
		{ FALSE, "attachment; Filename=first; filename*=iso-8859-1'fr'%E9",
		  "attachment", "; Filename=\"\303\251\"" },
		{ FALSE, "attachment; name*1*=AC; NAME*0*=utf-8'en'%E2%82%",
		  "attachment", "; name=\"\342\202\254\"" }
	};
	GMimeParserOptions *options = g_mime_parser_options_new ();
	GMimeParamList *params;
	GObject *object;
	GError *error = NULL;
	guint i, mode;
	char *value;

	for (mode = GMIME_RFC_COMPLIANCE_LOOSE; mode <= GMIME_RFC_COMPLIANCE_LOOSER; mode++) {
		g_mime_parser_options_set_parameter_compliance_mode (options, mode);
		for (i = 0; i < G_N_ELEMENTS (cases); i++) {
			object = parse_content_field (cases[i].is_type, options, cases[i].text, -1, &error);
			check (object != NULL && error == NULL, cases[i].text);
			if (object) {
				if (cases[i].is_type) {
					value = g_mime_content_type_get_mime_type (GMIME_CONTENT_TYPE (object));
					params = g_mime_content_type_get_parameters (GMIME_CONTENT_TYPE (object));
				} else {
					value = g_strdup (g_mime_content_disposition_get_disposition (GMIME_CONTENT_DISPOSITION (object)));
					params = g_mime_content_disposition_get_parameters (GMIME_CONTENT_DISPOSITION (object));
				}
				check_string (value, cases[i].value, "checked content fields preserve token spelling");
				check_string (g_mime_param_list_to_string (params), cases[i].parameters,
					      "checked content fields use checked parameter decoding and precedence");
			}
			g_clear_object (&object);
			g_clear_error (&error);
		}
		for (i = 0; i < 2; i++) {
			const char *prefix = i ? "text/plain" : "attachment";
			char *text = g_strconcat (prefix, "; name=\"first\" other=last", NULL);

			object = parse_content_field (i, options, text, -1, &error);
			check (mode == GMIME_RFC_COMPLIANCE_STRICT ?
			       object == NULL && error && error->code == GMIME_ERROR_PARSE_ERROR :
			       object != NULL && error == NULL,
			       "content fields honor parameter separator compliance");
			g_clear_object (&object);
			g_clear_error (&error);
			g_free (text);
			text = g_strconcat (prefix, "; name=two words \r\n\tcontinued  ", NULL);
			object = parse_content_field (i, options, text, -1, &error);
			check (mode == GMIME_RFC_COMPLIANCE_STRICT ?
			       object == NULL && error && error->code == GMIME_ERROR_PARSE_ERROR :
			       object != NULL && error == NULL,
			       "content fields honor unquoted parameter value compliance");
			if (object) {
				params = i ? g_mime_content_type_get_parameters (GMIME_CONTENT_TYPE (object)) :
					g_mime_content_disposition_get_parameters (GMIME_CONTENT_DISPOSITION (object));
				check_string (g_mime_param_list_to_string (params), "; name=\"two words \tcontinued\"",
					      "content field loose values unfold and trim without truncation");
			}
			g_clear_object (&object);
			g_clear_error (&error);
			g_free (text);
		}
	}
	g_mime_parser_options_unref (options);
}

static void
test_checked_content_errors (void)
{
	static const struct {
		gboolean is_type;
		const char *text;
		gssize offset;
	} cases[] = {
		{ TRUE, "", 0 },
		{ TRUE, "(empty)", -1 },
		{ TRUE, "/plain", 0 },
		{ TRUE, "text", -1 },
		{ TRUE, "text/", -1 },
		{ TRUE, "text/; name=value", 5 },
		{ TRUE, "te.xt/plain", 2 },
		{ TRUE, "\303\251/plain", 0 },
		{ TRUE, "text/\303\251", 5 },
		{ TRUE, "text/plain junk; name=value", 11 },
		{ TRUE, "text/(unfinished", -1 },
		{ TRUE, "text/plain (unfinished", -1 },
		{ TRUE, "text/plain; name=\"unfinished", -1 },
		{ TRUE, "text/plain; name=value; broken=", -1 },
		{ TRUE, "text/plain; name*2147483648=value", 26 },
		{ FALSE, "", 0 },
		{ FALSE, "(empty)", -1 },
		{ FALSE, "\"attachment\"", 0 },
		{ FALSE, "; filename=x", 0 },
		{ FALSE, "attach.ment", 6 },
		{ FALSE, "attachment junk; filename=x", 11 },
		{ FALSE, "attachment(comment", -1 },
		{ FALSE, "attachment; filename=\"unfinished", -1 },
		{ FALSE, "attachment; name=value; broken=", -1 },
		{ FALSE, "attachment; name*2147483648=value", 26 },
		{ FALSE, "attachment; name*0*=utf-8''%E2; name*1*=%82%AC; broken=", -1 }
	};
	GMimeParserOptions *options = g_mime_parser_options_new ();
	GError *error = NULL;
	GObject *object;
	guint i, mode;
	char *ending;

	for (mode = GMIME_RFC_COMPLIANCE_LOOSE; mode <= GMIME_RFC_COMPLIANCE_LOOSER; mode++) {
		g_mime_parser_options_set_parameter_compliance_mode (options, mode);
		for (i = 0; i < G_N_ELEMENTS (cases); i++) {
			object = parse_content_field (cases[i].is_type, options, cases[i].text, -1, &error);
			ending = g_strdup_printf ("at byte offset %" G_GSIZE_FORMAT ".",
						  cases[i].offset < 0 ? strlen (cases[i].text) : (gsize) cases[i].offset);
			check (object == NULL && error && error->domain == GMIME_ERROR &&
			       error->code == GMIME_ERROR_PARSE_ERROR && g_str_has_suffix (error->message, ending),
			       cases[i].text);
			g_free (ending);
			g_clear_object (&object);
			g_clear_error (&error);
			object = parse_content_field (cases[i].is_type, options, cases[i].text, -1, NULL);
			check (object == NULL, "checked content fields fail even without an error output");
			g_clear_object (&object);
		}
	}
	g_mime_parser_options_unref (options);
}

static void
test_checked_content_bounds (void)
{
	const char bounded[] = { 't', 'e', 'x', 't', '/', 'p' };
	const char embedded[] = { 'x', '/', 'y', '\0', ';', 'n', '=', 'y' };
	GObject *object;
	GError *error = NULL;
	guint i;

	for (i = 0; i < 2; i++) {
		object = parse_content_field (i, NULL, bounded, sizeof (bounded), &error);
		check (object && error == NULL, "content field bounded input need not be NUL-terminated");
		g_clear_object (&object);
		object = parse_content_field (i, NULL, embedded, sizeof (embedded), &error);
		check (object == NULL && error && error->code == GMIME_ERROR_PARSE_ERROR &&
		       g_str_has_suffix (error->message, "at byte offset 3."), "content fields reject embedded NUL bytes");
		g_clear_object (&object);
		g_clear_error (&error);
		object = parse_content_field (i, NULL, embedded, -1, &error);
		check (object && error == NULL, "content field -1 length stops at the first NUL");
		g_clear_object (&object);
		object = parse_content_field (i, NULL, bounded, 0, &error);
		check (object == NULL && error && error->code == GMIME_ERROR_PARSE_ERROR,
		       "empty bounded content fields fail instead of substituting defaults");
		g_clear_object (&object);
		g_clear_error (&error);
#if GLIB_SIZEOF_SIZE_T > 4
		object = parse_content_field (i, NULL, bounded, (gssize) G_MAXINT + 1, &error);
		check (object == NULL && error && error->code == GMIME_ERROR_PARSE_ERROR,
		       "oversized content field lengths fail before reading input");
		g_clear_object (&object);
		g_clear_error (&error);
#endif
		g_test_expect_message ("gmime", G_LOG_LEVEL_CRITICAL, "*str != NULL*");
		object = parse_content_field (i, NULL, NULL, -1, &error);
		g_test_assert_expected_messages ();
		check (object == NULL && error == NULL, "NULL content field input is a programming error");
		g_test_expect_message ("gmime", G_LOG_LEVEL_CRITICAL, "*length >= -1*");
		object = parse_content_field (i, NULL, bounded, -2, &error);
		g_test_assert_expected_messages ();
		check (object == NULL && error == NULL, "invalid content field lengths are programming errors");
		g_set_error_literal (&error, GMIME_ERROR, GMIME_ERROR_GENERAL, "existing error");
		g_test_expect_message ("gmime", G_LOG_LEVEL_CRITICAL, "*error == NULL || *error == NULL*");
		object = parse_content_field (i, NULL, bounded, sizeof (bounded), &error);
		g_test_assert_expected_messages ();
		check (object == NULL && error->code == GMIME_ERROR_GENERAL, "content field parsing preserves existing errors");
		g_clear_error (&error);
	}
}

static void
test_checked_content_ownership (void)
{
	GMimeContentType *type = g_mime_content_type_parse_full (NULL, "text/plain; name=first; NAME=last", -1, NULL);
	GMimeContentDisposition *disposition = g_mime_content_disposition_parse_full (NULL, "attachment; filename=x", -1, NULL);
	GMimePart *part = g_mime_part_new ();
	GMimeEntity *entity = GMIME_ENTITY (part);
	GMimeParamList *params, *disposition_params;
	GMimeParam *param;

	check (type != NULL && disposition != NULL, "checked content models are available for ownership checks");
	if (!type || !disposition) {
		g_clear_object (&type);
		g_clear_object (&disposition);
		g_object_unref (part);
		return;
	}
	params = g_object_ref (g_mime_content_type_get_parameters (type));
	disposition_params = g_object_ref (g_mime_content_disposition_get_parameters (disposition));
	g_mime_entity_set_content_type (entity, type);
	g_mime_entity_set_content_disposition (entity, disposition);
	g_object_unref (type);
	g_object_unref (disposition);
	param = g_mime_param_list_get_parameter (params, "name");
	g_mime_param_set_value (param, "updated");
	g_mime_param_list_set_parameter (disposition_params, "filename", "updated.txt");
	check (strstr (g_mime_entity_get_header (entity, "Content-Type"), "updated") &&
	       strstr (g_mime_entity_get_header (entity, "Content-Disposition"), "updated.txt"),
	       "checked content parameter mutations propagate to owning entity headers");
	g_object_unref (part);
	g_mime_param_set_value (param, "detached");
	g_mime_param_list_set_parameter (disposition_params, "filename", "detached.txt");
	check (!strcmp (g_mime_param_get_value (param), "detached") &&
	       !strcmp (g_mime_param_get_value (g_mime_param_list_get_parameter (disposition_params, "filename")), "detached.txt"),
	       "retained checked parameter lists safely outlive their content models");
	g_object_unref (params);
	g_object_unref (disposition_params);

	type = g_mime_content_type_parse (NULL, "not-a-content-type");
	check_string (g_mime_content_type_get_mime_type (type), "application/octet-stream",
		      "legacy content type parsing retains its fallback");
	g_object_unref (type);
	type = g_mime_content_type_parse (NULL, "text/plain junk; name=first; NAME=last; broken=");
	params = g_mime_content_type_get_parameters (type);
	check (g_mime_param_list_length (params) == 2 &&
	       !strcmp (g_mime_content_type_get_parameter (type, "name"), "first"),
	       "legacy content type parsing retains junk recovery and duplicate names");
	g_object_unref (type);
	disposition = g_mime_content_disposition_parse (NULL, "\"attachment\"; name=first; NAME=last; broken=");
	check (!strcmp (g_mime_content_disposition_get_disposition (disposition), "\"attachment\"") &&
	       g_mime_param_list_length (g_mime_content_disposition_get_parameters (disposition)) == 2,
	       "legacy disposition parsing retains quoted tokens and partial parameters");
	g_object_unref (disposition);
}

static GMimeParamList *
content_field_parameters (gboolean is_type, GObject *object)
{
	if (is_type)
		return g_mime_content_type_get_parameters (GMIME_CONTENT_TYPE (object));

	return g_mime_content_disposition_get_parameters (GMIME_CONTENT_DISPOSITION (object));
}

static GObject *
clone_content_field (gboolean is_type, GObject *object)
{
	if (is_type)
		return (GObject *) g_mime_content_type_clone (GMIME_CONTENT_TYPE (object));

	return (GObject *) g_mime_content_disposition_clone (GMIME_CONTENT_DISPOSITION (object));
}

static char *
content_field_to_string (gboolean is_type, GObject *object)
{
	if (is_type)
		return g_mime_content_type_to_string (GMIME_CONTENT_TYPE (object));

	return g_mime_content_disposition_to_string (GMIME_CONTENT_DISPOSITION (object));
}

static void
test_content_cloning (void)
{
	GMimePart *first, *second;
	GObject *original, *clone;
	GMimeParamList *params, *copied;
	GMimeParam *param, *copy;
	guint i, j;
	char *before;

	for (i = 0; i < 2; i++) {
		original = parse_content_field (i, NULL, i ?
			"TEXT/Plain; Name*=iso-8859-1'fr'%E9; other=\"=?windows-1252?q?=80?=\"" :
			"InLine; Name*=iso-8859-1'fr'%E9; other=\"=?windows-1252?q?=80?=\"", -1, NULL);
		params = content_field_parameters (i, original);
		param = g_mime_param_list_get_parameter (params, "name");
		g_mime_param_set_always_quote (param, TRUE);
		clone = clone_content_field (i, original);
		copied = g_object_ref (content_field_parameters (i, clone));
		check (clone != original && copied != params &&
		       g_mime_param_list_length (copied) == g_mime_param_list_length (params),
		       "content model clones have independent objects and parameter lists");
		before = content_field_to_string (i, original);
		check_string (content_field_to_string (i, clone), before, "content clones preserve primary-token spelling and parameter order");
		g_free (before);
		for (j = 0; j < (guint) g_mime_param_list_length (params); j++) {
			param = g_mime_param_list_get_parameter_at (params, j);
			copy = g_mime_param_list_get_parameter_at (copied, j);
			check (copy != param &&
			       !g_strcmp0 (g_mime_param_get_name (copy), g_mime_param_get_name (param)) &&
			       !g_strcmp0 (g_mime_param_get_value (copy), g_mime_param_get_value (param)) &&
			       !g_strcmp0 (g_mime_param_get_charset (copy), g_mime_param_get_charset (param)) &&
			       !g_strcmp0 (g_mime_param_get_lang (copy), g_mime_param_get_lang (param)) &&
			       g_mime_param_get_encoding_method (copy) == g_mime_param_get_encoding_method (param) &&
			       g_mime_param_get_always_quote (copy) == g_mime_param_get_always_quote (param),
			       "content model cloning preserves every parameter formatting preference independently");
		}
		first = g_mime_part_new ();
		second = g_mime_part_new ();
		if (i) {
			g_mime_entity_set_content_type (GMIME_ENTITY (first), GMIME_CONTENT_TYPE (original));
			g_mime_entity_set_content_type (GMIME_ENTITY (second), GMIME_CONTENT_TYPE (clone));
			g_mime_content_type_set_media_type (GMIME_CONTENT_TYPE (clone), "application");
			g_mime_content_type_set_media_subtype (GMIME_CONTENT_TYPE (clone), "x-clone");
			check (!strcmp (g_mime_content_type_get_media_type (GMIME_CONTENT_TYPE (original)), "TEXT") &&
			       !strcmp (g_mime_content_type_get_media_subtype (GMIME_CONTENT_TYPE (original)), "Plain"),
			       "cloned media-type and subtype assignments leave the original independent");
		} else {
			g_mime_entity_set_content_disposition (GMIME_ENTITY (first), GMIME_CONTENT_DISPOSITION (original));
			g_mime_entity_set_content_disposition (GMIME_ENTITY (second), GMIME_CONTENT_DISPOSITION (clone));
			g_mime_content_disposition_set_disposition (GMIME_CONTENT_DISPOSITION (clone), "attachment");
			check (!strcmp (g_mime_content_disposition_get_disposition (GMIME_CONTENT_DISPOSITION (original)), "InLine"),
			       "cloned disposition assignments leave the original independent");
		}
		param = g_mime_param_list_get_parameter (params, "name");
		copy = g_mime_param_list_get_parameter (copied, "name");
		g_mime_param_set_value (copy, "clone-only");
		g_mime_param_set_charset (copy, "utf-8");
		g_mime_param_set_lang (copy, "en");
		g_mime_param_set_encoding_method (copy, GMIME_PARAM_ENCODING_METHOD_DEFAULT);
		g_mime_param_set_always_quote (copy, FALSE);
		check (!strcmp (g_mime_param_get_value (param), "\303\251") &&
		       !g_ascii_strcasecmp (g_mime_param_get_charset (param), "iso-8859-1") &&
		       !strcmp (g_mime_param_get_lang (param), "fr") &&
		       g_mime_param_get_encoding_method (param) == GMIME_PARAM_ENCODING_METHOD_RFC2231 &&
		       g_mime_param_get_always_quote (param), "clone metadata and value mutations do not affect the original");
		check (strstr (g_mime_entity_get_header (GMIME_ENTITY (second), i ? "Content-Type" : "Content-Disposition"), "clone-only") &&
		       !strstr (g_mime_entity_get_header (GMIME_ENTITY (first), i ? "Content-Type" : "Content-Disposition"), "clone-only"),
		       "cloned parameter changes notify only the cloned content model's owner");
		g_mime_param_list_remove (copied, "other");
		check (g_mime_param_list_length (params) == 2 && g_mime_param_list_length (copied) == 1,
		       "cloned parameter removal does not affect the original");
		g_object_unref (original);
		g_object_unref (clone);
		g_object_unref (first);
		g_object_unref (second);
		g_mime_param_set_value (copy, "after finalization");
		check (!strcmp (g_mime_param_get_value (copy), "after finalization"),
		       "retained cloned lists safely outlive both content models and their owners");
		g_object_unref (copied);

		original = i ? (GObject *) g_mime_content_type_parse (NULL, "text/plain; Name=first; NAME=last") :
			(GObject *) g_mime_content_disposition_parse (NULL, "\"attachment\"; Name=first; NAME=last");
		clone = clone_content_field (i, original);
		params = content_field_parameters (i, clone);
		check (g_mime_param_list_length (params) == 2 &&
		       !strcmp (g_mime_param_get_value (g_mime_param_list_get_parameter (params, "name")), "first"),
		       "content cloning preserves legacy duplicate names and first-name lookup");
		before = content_field_to_string (i, original);
		check_string (content_field_to_string (i, clone), before, "content cloning preserves legacy display text without normalization");
		g_free (before);
		g_object_unref (original);
		g_object_unref (clone);
	}
}

static void
test_content_display (void)
{
	static const struct {
		const char *value, *quoted;
	} cases[] = {
		{ "", "\"\"" },
		{ "ordinary.txt", "\"ordinary.txt\"" },
		{ "caf\303\251.txt", "\"caf\303\251.txt\"" },
		{ "a\"b\\c", "\"a\\\"b\\\\c\"" },
		{ "\001\177\r\n\t", "\"\001\177\r\n\t\"" },
		{ "=?utf-8?q?encoded-word?=", "\"=?utf-8?q?encoded-word?=\"" }
	};
	GMimeContentType *type = g_mime_content_type_new ("TEXT", "Plain");
	GMimeContentDisposition *disposition = g_mime_content_disposition_new (), *clone;
	GMimeContentType *type_clone;
	GMimeParam *param;
	guint i, method;
	char *expected, *long_value;

	check_string (g_mime_content_type_to_string (type), "Content-Type: TEXT/Plain", "empty content type display includes its field name without a newline");
	check_string (g_mime_content_disposition_to_string (disposition), "Content-Disposition: attachment",
		      "empty disposition display includes its field name without a newline");
	for (i = 0; i < G_N_ELEMENTS (cases); i++) {
		for (method = GMIME_PARAM_ENCODING_METHOD_DEFAULT; method <= GMIME_PARAM_ENCODING_METHOD_RFC2047; method++) {
			g_mime_content_type_set_parameter (type, "Name", cases[i].value);
			param = g_mime_param_list_get_parameter (g_mime_content_type_get_parameters (type), "name");
			g_mime_param_set_encoding_method (param, method);
			expected = g_strconcat ("Content-Type: TEXT/Plain; Name=", cases[i].quoted, NULL);
			check_string (g_mime_content_type_to_string (type), expected, "content type display quotes and escapes without transport encoding");
			g_free (expected);
			g_mime_content_disposition_set_parameter (disposition, "Filename", cases[i].value);
			param = g_mime_param_list_get_parameter (g_mime_content_disposition_get_parameters (disposition), "filename");
			g_mime_param_set_encoding_method (param, method);
			expected = g_strconcat ("Content-Disposition: attachment; Filename=", cases[i].quoted, NULL);
			check_string (g_mime_content_disposition_to_string (disposition), expected, "disposition display quotes and escapes without transport encoding");
			g_free (expected);
		}
	}
	long_value = g_strnfill (300, 'x');
	g_mime_content_type_set_parameter (type, "Name", long_value);
	expected = g_strconcat ("Content-Type: TEXT/Plain; Name=\"", long_value, "\"", NULL);
	check_string (g_mime_content_type_to_string (type), expected, "long content display values are never folded or split");
	g_free (expected);
	g_free (long_value);
	g_mime_content_type_set_media_type (type, "");
	g_mime_content_type_set_media_subtype (type, "");
	type_clone = g_mime_content_type_clone (type);
	check_string (g_mime_content_type_get_mime_type (type_clone), "/", "cloning empty assigned media tokens does not apply constructor defaults");
	g_object_unref (type_clone);
	g_mime_content_disposition_set_disposition (disposition, "");
	clone = g_mime_content_disposition_clone (disposition);
	check (!strcmp (g_mime_content_disposition_get_disposition (clone), ""), "cloning empty legacy disposition text does not apply defaults");
	g_object_unref (clone);
	g_object_unref (type);
	g_object_unref (disposition);
}

static void
test_content_token_assignments (void)
{
	const char *raw_type = " TEXT (type) / Plain;\r\n\tname=original.txt\r\n";
	const char *raw_disposition = " InLine;\r\n\tfilename=original.txt\r\n";
	GMimePart *part = g_mime_part_new ();
	GMimeEntity *entity = GMIME_ENTITY (part);
	GMimeHeaderList *headers = g_mime_entity_get_header_list (entity);
	GMimeHeader *type_header = g_mime_header_list_get_header (headers, "Content-Type"), *disposition_header;
	GMimeContentType *type;
	GMimeContentDisposition *disposition;
	const char *stored;
	char *equal;

	testsuite_header_set_raw_text (type_header, raw_type);
	g_mime_entity_set_disposition (entity, "inline");
	disposition_header = g_mime_header_list_get_header (headers, "Content-Disposition");
	testsuite_header_set_raw_text (disposition_header, raw_disposition);
	type = g_mime_entity_get_content_type (entity);
	disposition = g_mime_entity_get_content_disposition (entity);
	stored = g_mime_content_type_get_media_type (type);
	equal = g_strdup (stored);
	g_mime_content_type_set_media_type (type, stored);
	g_mime_content_type_set_media_type (type, equal);
	check (g_mime_content_type_get_media_type (type) == stored, "equivalent media-type assignments retain their storage");
	g_free (equal);
	stored = g_mime_content_type_get_media_subtype (type);
	equal = g_strdup (stored);
	g_mime_content_type_set_media_subtype (type, stored);
	g_mime_content_type_set_media_subtype (type, equal);
	check (g_mime_content_type_get_media_subtype (type) == stored, "equivalent subtype assignments retain their storage");
	g_free (equal);
	stored = g_mime_content_disposition_get_disposition (disposition);
	equal = g_strdup (stored);
	g_mime_content_disposition_set_disposition (disposition, stored);
	g_mime_content_disposition_set_disposition (disposition, equal);
	check (g_mime_content_disposition_get_disposition (disposition) == stored, "equivalent disposition assignments retain their storage");
	g_free (equal);
	check (!strcmp (testsuite_header_raw_text (type_header), raw_type) &&
	       !strcmp (testsuite_header_raw_text (disposition_header), raw_disposition),
	       "equivalent primary-token assignments preserve raw header comments and folding");
	g_mime_content_type_set_media_type (type, "text");
	g_mime_content_type_set_media_subtype (type, "plain");
	g_mime_content_disposition_set_disposition (disposition, "inline");
	check (strstr (g_mime_entity_get_header (entity, "Content-Type"), "text/plain") &&
	       strstr (g_mime_entity_get_header (entity, "Content-Disposition"), "inline"),
	       "primary-token spelling changes still update owning headers");
	g_mime_content_type_set_media_type (type, g_mime_content_type_get_media_type (type) + 1);
	g_mime_content_type_set_media_subtype (type, g_mime_content_type_get_media_subtype (type) + 1);
	g_mime_content_disposition_set_disposition (disposition, g_mime_content_disposition_get_disposition (disposition) + 1);
	check (!strcmp (g_mime_content_type_get_media_type (type), "ext") &&
	       !strcmp (g_mime_content_type_get_media_subtype (type), "lain") &&
	       !strcmp (g_mime_content_disposition_get_disposition (disposition), "nline"),
	       "primary-token assignments safely copy aliased substrings");
	g_object_unref (part);
}

static void
test_content_type_properties (void)
{
	static const struct {
		const char *name;
		const char * (*get) (GMimeContentType *);
		void (*set) (GMimeContentType *, const char *);
	} properties[] = {
		{ "boundary", g_mime_content_type_get_boundary, g_mime_content_type_set_boundary },
		{ "charset", g_mime_content_type_get_charset, g_mime_content_type_set_charset },
		{ "format", g_mime_content_type_get_format, g_mime_content_type_set_format },
		{ "name", g_mime_content_type_get_name, g_mime_content_type_set_name }
	};
	GMimePart *part = g_mime_part_new ();
	GMimeEntity *entity = GMIME_ENTITY (part);
	GMimeHeader *header = g_mime_header_list_get_header (g_mime_entity_get_header_list (entity), "Content-Type");
	GMimeContentType *type, *clone;
	GMimeParamList *params;
	GMimeParam *param;
	const char *stored;
	char *upper, *raw, *same, *before;
	guint i;

	for (i = 0; i < G_N_ELEMENTS (properties); i++) {
		upper = g_ascii_strup (properties[i].name, -1);
		raw = g_strdup_printf (" TEXT/Plain;\r\n\t%s*=iso-8859-1'fr'%%E9;\r\n\tkeep=stable\r\n", upper);
		testsuite_header_set_raw_text (header, raw);
		type = g_mime_entity_get_content_type (entity);
		params = g_mime_content_type_get_parameters (type);
		param = g_mime_param_list_get_parameter (params, properties[i].name);
		stored = properties[i].get (type);
		check (stored && !strcmp (stored, "\303\251") &&
		       stored == g_mime_param_get_value (param),
		       "content type properties return borrowed values using case-insensitive parameter lookup");
		same = g_strdup (stored);
		properties[i].set (type, stored);
		properties[i].set (type, same);
		g_free (same);
		check (properties[i].get (type) == stored &&
		       !strcmp (testsuite_header_raw_text (header), raw),
		       "equivalent convenience assignments preserve parameter storage and raw folding");
		g_test_expect_message ("gmime", G_LOG_LEVEL_CRITICAL, "*g_utf8_validate*");
		properties[i].set (type, "\377");
		g_test_assert_expected_messages ();
		check (properties[i].get (type) == stored &&
		       !strcmp (testsuite_header_raw_text (header), raw),
		       "invalid UTF-8 convenience assignments leave parameters and headers unchanged");
		g_mime_param_set_always_quote (param, TRUE);
		properties[i].set (type, "prefix-value");
		check (g_mime_param_list_get_parameter_at (params, 0) == param &&
		       !strcmp (g_mime_param_get_name (param), upper) &&
		       !g_ascii_strcasecmp (g_mime_param_get_charset (param), "iso-8859-1") &&
		       !strcmp (g_mime_param_get_lang (param), "fr") &&
		       g_mime_param_get_encoding_method (param) == GMIME_PARAM_ENCODING_METHOD_RFC2231 &&
		       g_mime_param_get_always_quote (param) &&
		       strstr (testsuite_header_raw_text (header), "prefix-value"),
		       "convenience assignments retain parameter identity, position, spelling, and metadata while updating headers");
		properties[i].set (type, properties[i].get (type) + 7);
		check (!strcmp (properties[i].get (type), "value"), "content type convenience assignments copy aliased substrings safely");
		properties[i].set (type, "  caf\303\251  ");
		check (!strcmp (properties[i].get (type), "  caf\303\251  "), "content type convenience values preserve Unicode and whitespace");
		properties[i].set (type, "");
		check (properties[i].get (type) && !*properties[i].get (type) &&
		       g_mime_param_list_length (params) == 2, "empty convenience values remain explicit parameters");
		properties[i].set (type, NULL);
		check (properties[i].get (type) == NULL && g_mime_param_list_length (params) == 1 &&
		       strstr (testsuite_header_raw_text (header), upper) == NULL,
		       "NULL convenience assignments remove the matching parameter and update the owning header");
		before = g_strdup (testsuite_header_raw_text (header));
		properties[i].set (type, NULL);
		check (!strcmp (testsuite_header_raw_text (header), before), "removing an absent convenience parameter does not rewrite its header");
		g_free (before);
		clone = g_mime_content_type_clone (type);
		properties[i].set (clone, "clone-only");
		check (properties[i].get (type) == NULL && !strcmp (properties[i].get (clone), "clone-only"),
		       "convenience assignment on a cloned content type does not affect the original");
		g_object_unref (clone);
		g_test_expect_message ("gmime", G_LOG_LEVEL_CRITICAL, "*GMIME_IS_CONTENT_TYPE*");
		properties[i].set (NULL, "value");
		g_test_assert_expected_messages ();
		g_test_expect_message ("gmime", G_LOG_LEVEL_CRITICAL, "*GMIME_IS_CONTENT_TYPE*");
		stored = properties[i].get (NULL);
		g_test_assert_expected_messages ();
		check (stored == NULL, "convenience getters reject invalid content type objects");
		g_free (raw);
		raw = g_strdup_printf (" text/plain; %s=first; %s=second\r\n", upper, properties[i].name);
		testsuite_header_set_raw_text (header, raw);
		type = g_mime_entity_get_content_type (entity);
		params = g_mime_content_type_get_parameters (type);
		param = g_mime_param_list_get_parameter_at (params, 1);
		properties[i].set (type, "changed");
		check (g_mime_param_list_length (params) == 2 && !strcmp (properties[i].get (type), "changed") &&
		       !strcmp (g_mime_param_get_value (param), "second"), "convenience assignment only changes the first legacy duplicate");
		properties[i].set (type, NULL);
		check (g_mime_param_list_length (params) == 1 && g_mime_param_list_get_parameter_at (params, 0) == param &&
		       !strcmp (properties[i].get (type), "second"), "convenience removal only removes the first legacy duplicate");
		g_free (raw);
		g_free (upper);
	}
	type = g_mime_entity_get_content_type (entity);
	g_mime_content_type_set_charset (type, "X-Not-A-Real-Charset");
	check (!strcmp (g_mime_content_type_get_charset (type), "X-Not-A-Real-Charset"),
	       "charset convenience values are stored without resolution or normalization");
	g_mime_content_type_set_parameter (type, "format", "flowed");
	check (!strcmp (g_mime_content_type_get_format (type), "flowed"), "convenience getters observe generic parameter assignments");
	g_object_unref (part);
}

static void
test_disposition_properties (void)
{
	const char *raw = " attachment;\r\n\tFILENAME*=iso-8859-1'fr'caf%E9.txt;\r\n\tkeep=stable\r\n";
	GMimePart *part = g_mime_part_new ();
	GMimeEntity *entity = GMIME_ENTITY (part);
	GMimeContentType *type = g_mime_entity_get_content_type (entity);
	GMimeContentDisposition *disposition, *clone;
	GMimeHeader *header;
	GMimeParamList *params;
	GMimeParam *param;
	const char *stored;
	char *same;

	g_mime_content_type_set_name (type, "type-fallback.txt");
	g_mime_entity_set_disposition (entity, "attachment");
	disposition = g_mime_entity_get_content_disposition (entity);
	check (g_mime_content_disposition_get_filename (disposition) == NULL &&
	       !strcmp (g_mime_part_get_filename (part), "type-fallback.txt"),
	       "disposition filename lookup has no Content-Type fallback while part-level lookup retains it");
	header = g_mime_header_list_get_header (g_mime_entity_get_header_list (entity), "Content-Disposition");
	testsuite_header_set_raw_text (header, raw);
	disposition = g_mime_entity_get_content_disposition (entity);
	params = g_mime_content_disposition_get_parameters (disposition);
	param = g_mime_param_list_get_parameter (params, "filename");
	stored = g_mime_content_disposition_get_filename (disposition);
	check (stored && !strcmp (stored, "caf\303\251.txt") && stored == g_mime_param_get_value (param),
	       "disposition filename is a borrowed, case-insensitive decoded parameter value");
	same = g_strdup (stored);
	g_mime_content_disposition_set_filename (disposition, stored);
	g_mime_content_disposition_set_filename (disposition, same);
	g_free (same);
	check (g_mime_content_disposition_get_filename (disposition) == stored &&
	       !strcmp (testsuite_header_raw_text (header), raw), "equivalent filename assignments preserve stored values and raw folding");
	g_test_expect_message ("gmime", G_LOG_LEVEL_CRITICAL, "*g_utf8_validate*");
	g_mime_content_disposition_set_filename (disposition, "\377");
	g_test_assert_expected_messages ();
	check (g_mime_content_disposition_get_filename (disposition) == stored &&
	       !strcmp (testsuite_header_raw_text (header), raw), "invalid UTF-8 filenames leave parameters and raw headers unchanged");
	g_mime_param_set_always_quote (param, TRUE);
	g_mime_content_disposition_set_filename (disposition, "prefix-file.txt");
	check (g_mime_param_list_get_parameter_at (params, 0) == param &&
	       !strcmp (g_mime_param_get_name (param), "FILENAME") &&
	       !g_ascii_strcasecmp (g_mime_param_get_charset (param), "iso-8859-1") &&
	       !strcmp (g_mime_param_get_lang (param), "fr") &&
	       g_mime_param_get_encoding_method (param) == GMIME_PARAM_ENCODING_METHOD_RFC2231 &&
	       g_mime_param_get_always_quote (param) &&
	       strstr (testsuite_header_raw_text (header), "prefix-file.txt"),
	       "filename assignment preserves parameter identity and metadata and updates its owning header");
	g_mime_content_disposition_set_filename (disposition, g_mime_content_disposition_get_filename (disposition) + 7);
	check (!strcmp (g_mime_content_disposition_get_filename (disposition), "file.txt"), "filename assignment safely copies aliased substrings");
	g_mime_content_disposition_set_filename (disposition, "  caf\303\251.txt  ");
	check (!strcmp (g_mime_content_disposition_get_filename (disposition), "  caf\303\251.txt  ") &&
	       !strcmp (g_mime_part_get_filename (part), "  caf\303\251.txt  ") &&
	       !strcmp (g_mime_content_type_get_name (type), "type-fallback.txt"),
	       "disposition filenames and part lookup preserve whitespace without synchronizing the Content-Type name");
	g_mime_content_disposition_set_filename (disposition, "");
	check (g_mime_content_disposition_get_filename (disposition) && !*g_mime_content_disposition_get_filename (disposition) &&
	       !*g_mime_part_get_filename (part), "an empty disposition filename does not trigger the part-level fallback");
	g_mime_content_disposition_set_filename (disposition, NULL);
	check (g_mime_content_disposition_get_filename (disposition) == NULL &&
	       !strcmp (g_mime_part_get_filename (part), "type-fallback.txt") &&
	       g_mime_param_list_length (params) == 1 && strstr (testsuite_header_raw_text (header), "FILENAME") == NULL,
	       "removing a disposition filename updates its header and restores the part-level fallback");
	same = g_strdup (testsuite_header_raw_text (header));
	g_mime_content_disposition_set_filename (disposition, NULL);
	check (!strcmp (testsuite_header_raw_text (header), same), "removing an absent filename leaves raw headers unchanged");
	g_free (same);
	clone = g_mime_content_disposition_clone (disposition);
	g_mime_content_disposition_set_filename (clone, "clone-only.txt");
	g_mime_content_disposition_set_is_attachment (clone, FALSE);
	check (!strcmp (g_mime_content_disposition_get_filename (clone), "clone-only.txt") &&
	       !g_mime_content_disposition_is_attachment (clone) &&
	       g_mime_content_disposition_get_filename (disposition) == NULL && g_mime_content_disposition_is_attachment (disposition),
	       "filename and attachment assignment on a cloned disposition leave the original independent");
	g_object_unref (clone);
	g_test_expect_message ("gmime", G_LOG_LEVEL_CRITICAL, "*GMIME_IS_CONTENT_DISPOSITION*");
	g_mime_content_disposition_set_filename (NULL, "value");
	g_test_assert_expected_messages ();
	g_test_expect_message ("gmime", G_LOG_LEVEL_CRITICAL, "*GMIME_IS_CONTENT_DISPOSITION*");
	stored = g_mime_content_disposition_get_filename (NULL);
	g_test_assert_expected_messages ();
	check (stored == NULL, "filename lookup rejects invalid disposition objects");
	g_object_unref (part);
}

static void
test_attachment_assignment (void)
{
	const char *raw = " attachment;\r\n\tfilename=stable.txt\r\n";
	GMimePart *part = g_mime_part_new ();
	GMimeEntity *entity = GMIME_ENTITY (part);
	GMimeContentDisposition *disposition;
	GMimeHeader *header;
	char *before;

	g_mime_entity_set_disposition (entity, "attachment");
	header = g_mime_header_list_get_header (g_mime_entity_get_header_list (entity), "Content-Disposition");
	testsuite_header_set_raw_text (header, raw);
	disposition = g_mime_entity_get_content_disposition (entity);
	g_mime_content_disposition_set_is_attachment (disposition, TRUE);
	check (g_mime_content_disposition_is_attachment (disposition) &&
	       g_mime_part_is_attachment (part) && !strcmp (testsuite_header_raw_text (header), raw),
	       "equivalent attachment assignment preserves raw header folding");
	g_mime_content_disposition_set_is_attachment (disposition, FALSE);
	check (!g_mime_content_disposition_is_attachment (disposition) && !g_mime_part_is_attachment (part) &&
	       !strcmp (g_mime_content_disposition_get_disposition (disposition), "inline") &&
	       strstr (testsuite_header_raw_text (header), "inline") &&
	       !strcmp (g_mime_content_disposition_get_filename (disposition), "stable.txt"),
	       "attachment assignment selects inline and notifies header owners without changing parameters");
	before = g_strdup (testsuite_header_raw_text (header));
	g_mime_content_disposition_set_is_attachment (disposition, FALSE);
	check (!strcmp (testsuite_header_raw_text (header), before), "equivalent inline assignment does not rewrite its header");
	g_free (before);
	g_mime_content_disposition_set_is_attachment (disposition, TRUE);
	check (g_mime_part_is_attachment (part) &&
	       strstr (testsuite_header_raw_text (header), "attachment"), "changing back to attachment updates the owning header");
	testsuite_header_set_raw_text (header, " AtTaChMeNt;\r\n\tfilename=stable.txt\r\n");
	disposition = g_mime_entity_get_content_disposition (entity);
	g_mime_content_disposition_set_is_attachment (disposition, TRUE);
	check (!strcmp (g_mime_content_disposition_get_disposition (disposition), "attachment") &&
	       strstr (testsuite_header_raw_text (header), "attachment"),
	       "attachment assignment canonicalizes token spelling and notifies header owners");
	g_test_expect_message ("gmime", G_LOG_LEVEL_CRITICAL, "*GMIME_IS_CONTENT_DISPOSITION*");
	g_mime_content_disposition_set_is_attachment (NULL, TRUE);
	g_test_assert_expected_messages ();
	g_object_unref (part);
}

static void
test_disposition_dates (void)
{
	static const struct {
		const char *name;
		GDateTime * (*get) (GMimeContentDisposition *);
		void (*set) (GMimeContentDisposition *, GDateTime *);
	} properties[] = {
		{ "creation-date", g_mime_content_disposition_get_creation_date, g_mime_content_disposition_set_creation_date },
		{ "modification-date", g_mime_content_disposition_get_modification_date, g_mime_content_disposition_set_modification_date },
		{ "read-date", g_mime_content_disposition_get_read_date, g_mime_content_disposition_set_read_date }
	};
	static const char *invalid[] = { "", " \t\r\n", "\302\240\342\200\203", "not a date", "31 Feb 2025 09:00:00 -0500" };
	GMimePart *part;
	GMimeEntity *entity;
	GMimeContentDisposition *disposition, *clone, *parsed;
	GMimeHeader *header;
	GMimeParamList *params;
	GMimeParam *param;
	GDateTime *date, *retained, *input, *utc, *copy_date;
	GTimeZone *zone;
	const char *stored;
	char *upper, *raw, *before;
	guint i, j;

	for (i = 0; i < G_N_ELEMENTS (properties); i++) {
		part = g_mime_part_new ();
		entity = GMIME_ENTITY (part);
		g_mime_entity_set_disposition (entity, "attachment");
		disposition = g_mime_entity_get_content_disposition (entity);
		header = g_mime_header_list_get_header (g_mime_entity_get_header_list (entity), "Content-Disposition");
		check (properties[i].get (disposition) == NULL, "missing disposition date parameters return NULL");
		before = g_strdup (testsuite_header_raw_text (header));
		properties[i].set (disposition, NULL);
		check (!strcmp (testsuite_header_raw_text (header), before), "removing an absent date does not rewrite its owning header");
		g_free (before);
		upper = g_ascii_strup (properties[i].name, -1);
		raw = g_strdup_printf (" attachment;\r\n\t%s*=us-ascii'fr'Fri%%2C%%2028%%20Feb%%202025%%2009%%3A00%%3A00%%20-0500;\r\n\tfilename=stable.txt\r\n", upper);
		testsuite_header_set_raw_text (header, raw);
		disposition = g_mime_entity_get_content_disposition (entity);
		params = g_mime_content_disposition_get_parameters (disposition);
		param = g_mime_param_list_get_parameter (params, properties[i].name);
		stored = g_mime_param_get_value (param);
		date = properties[i].get (disposition);
		retained = properties[i].get (disposition);
		check (date && retained && date != retained && g_date_time_equal (date, retained) &&
		       g_date_time_get_utc_offset (date) == -5 * G_TIME_SPAN_HOUR &&
		       g_date_time_get_hour (date) == 9 && g_date_time_get_day_of_month (date) == 28,
		       "disposition date getters decode case-insensitively and return independent owned dates with their offset");
		check (g_mime_param_get_value (param) == stored && !strcmp (testsuite_header_raw_text (header), raw),
		       "disposition date getters do not rewrite parameter text or raw headers");
		properties[i].set (disposition, date);
		check (g_mime_param_get_value (param) == stored && !strcmp (testsuite_header_raw_text (header), raw),
		       "equivalent formatted date assignments preserve stored text and raw folding");
		g_date_time_unref (date);
		g_mime_param_set_always_quote (param, TRUE);
		input = g_date_time_new_from_iso8601 ("2026-01-02T03:04:05.987654+05:45", NULL);
		properties[i].set (disposition, input);
		check (!strcmp (g_mime_param_get_value (param), "Fri, 02 Jan 2026 03:04:05 +0545") &&
		       g_mime_param_list_get_parameter_at (params, 0) == param &&
		       !strcmp (g_mime_param_get_name (param), upper) &&
		       !g_ascii_strcasecmp (g_mime_param_get_charset (param), "us-ascii") &&
		       !strcmp (g_mime_param_get_lang (param), "fr") &&
		       g_mime_param_get_encoding_method (param) == GMIME_PARAM_ENCODING_METHOD_RFC2231 &&
		       g_mime_param_get_always_quote (param), "date assignment uses whole seconds while retaining parameter identity, order, spelling, and metadata");
		date = properties[i].get (disposition);
		check (date && g_date_time_to_unix (date) == g_date_time_to_unix (input) &&
		       g_date_time_get_microsecond (date) == 0 &&
		       g_date_time_get_utc_offset (date) == g_date_time_get_utc_offset (input),
		       "disposition dates round-trip the instant to whole seconds and retain minute-aligned offsets");
		g_date_time_unref (date);
		parsed = g_mime_content_disposition_parse_full (NULL, g_mime_entity_get_header (entity, "Content-Disposition"), -1, NULL);
		date = properties[i].get (parsed);
		check (date && g_date_time_to_unix (date) == g_date_time_to_unix (input) &&
		       !strcmp (g_mime_content_disposition_get_filename (disposition), "stable.txt"),
		       "date assignment updates owning headers without changing unrelated parameters");
		g_date_time_unref (date);
		g_object_unref (parsed);
		stored = g_mime_param_get_value (param);
		before = g_strdup (testsuite_header_raw_text (header));
		properties[i].set (disposition, input);
		check (g_mime_param_get_value (param) == stored && !strcmp (testsuite_header_raw_text (header), before),
		       "subsecond date input does not rewrite an identical whole-second parameter");
		g_free (before);
		utc = g_date_time_to_utc (input);
		properties[i].set (disposition, utc);
		date = properties[i].get (disposition);
		check (date && g_date_time_get_utc_offset (date) == 0 && g_date_time_to_unix (date) == g_date_time_to_unix (input) &&
		       strcmp (g_mime_param_get_value (param), "Fri, 02 Jan 2026 03:04:05 +0545"),
		       "assigning the same instant with another offset changes its formatted parameter");
		g_date_time_unref (date);
		g_date_time_unref (utc);
		g_date_time_unref (input);
		zone = g_time_zone_new_offset (37);
		input = g_date_time_new (zone, 2025, 2, 28, 1, 0, 0.987654);
		g_time_zone_unref (zone);
		properties[i].set (disposition, input);
		date = properties[i].get (disposition);
		check (!strcmp (g_mime_param_get_value (param), "Fri, 28 Feb 2025 00:59:23 -0000") &&
		       date && g_date_time_get_utc_offset (date) == 0 &&
		       g_date_time_to_unix (date) == g_date_time_to_unix (input) &&
		       g_date_time_get_microsecond (date) == 0,
		       "sub-minute disposition date offsets convert to UTC while retaining whole-second instants");
		g_date_time_unref (date);
		g_date_time_unref (input);
		for (j = 0; j < 2; j++) {
			zone = g_time_zone_new_offset (j ? -37 : 37);
			input = j ? g_date_time_new (zone, 9999, 12, 31, 23, 59, 59) :
				g_date_time_new (zone, 1, 1, 1, 0, 0, 0);
			g_time_zone_unref (zone);
			stored = g_mime_param_get_value (param);
			before = g_strdup (testsuite_header_raw_text (header));
			g_test_expect_message ("gmime", G_LOG_LEVEL_CRITICAL, "*utc != NULL*");
			properties[i].set (disposition, input);
			g_test_assert_expected_messages ();
			check (g_mime_param_get_value (param) == stored &&
			       !strcmp (testsuite_header_raw_text (header), before),
			       "unrepresentable UTC conversions leave disposition dates and owning headers unchanged");
			g_free (before);
			g_date_time_unref (input);
		}
		clone = g_mime_content_disposition_clone (disposition);
		properties[i].set (clone, NULL);
		date = properties[i].get (disposition);
		check (date && properties[i].get (clone) == NULL, "removing a date from a clone leaves the original parameter independent");
		g_date_time_unref (date);
		input = g_date_time_new_utc (1970, 1, 1, 0, 0, 0);
		properties[i].set (clone, input);
		g_date_time_unref (input);
		copy_date = properties[i].get (clone);
		g_object_unref (clone);
		check (copy_date && g_date_time_to_unix (copy_date) == 0, "the Unix epoch is an ordinary date and returned dates outlive cloned models");
		g_date_time_unref (copy_date);
		properties[i].set (disposition, NULL);
		check (properties[i].get (disposition) == NULL && g_mime_param_list_length (params) == 1 &&
		       strstr (testsuite_header_raw_text (header), upper) == NULL,
		       "NULL date assignments remove the matching parameter and update owning headers");
		for (j = 0; j < G_N_ELEMENTS (invalid); j++) {
			g_mime_content_disposition_set_parameter (disposition, properties[i].name, invalid[j]);
			before = g_strdup (testsuite_header_raw_text (header));
			check (properties[i].get (disposition) == NULL &&
			       !strcmp (g_mime_content_disposition_get_parameter (disposition, properties[i].name), invalid[j]) &&
			       !strcmp (testsuite_header_raw_text (header), before),
			       "invalid stored dates return NULL without altering parameters or headers");
			g_free (before);
		}
		g_test_expect_message ("gmime", G_LOG_LEVEL_CRITICAL, "*GMIME_IS_CONTENT_DISPOSITION*");
		properties[i].set (NULL, retained);
		g_test_assert_expected_messages ();
		g_test_expect_message ("gmime", G_LOG_LEVEL_CRITICAL, "*GMIME_IS_CONTENT_DISPOSITION*");
		date = properties[i].get (NULL);
		g_test_assert_expected_messages ();
		check (date == NULL, "date accessors reject invalid disposition objects");
		g_object_unref (part);
		check (g_date_time_get_year (retained) == 2025 && g_date_time_get_utc_offset (retained) == -5 * G_TIME_SPAN_HOUR,
		       "returned disposition dates remain valid after source replacement, removal, and owner finalization");
		g_date_time_unref (retained);
		g_free (upper);
		g_free (raw);
	}
}

static void
test_disposition_size (void)
{
	static const struct {
		const char *value;
		gint64 expected;
	} cases[] = {
		{ "", -1 }, { " ", -1 }, { "\t\r\n", -1 },
		{ "0", 0 }, { "1", 1 }, { "010", 10 }, { "00000", 0 },
		{ "2147483648", G_GINT64_CONSTANT (2147483648) },
		{ "9007199254740993", G_GINT64_CONSTANT (9007199254740993) },
		{ "9223372036854775807", G_MAXINT64 },
		{ "00009223372036854775807", G_MAXINT64 },
		{ "9223372036854775808", -1 }, { "18446744073709551615", -1 },
		{ "999999999999999999999999999999999999999999", -1 },
		{ "-1", -1 }, { "-0", -1 }, { "+0", -1 }, { "+1", -1 },
		{ " 1", -1 }, { "1 ", -1 }, { "1\t", -1 }, { "1\r\n", -1 },
		{ "1.0", -1 }, { "1,000", -1 }, { "1e3", -1 },
		{ "0x10", -1 }, { "1 (comment)", -1 }, { "\"1\"", -1 },
		{ "\302\240", -1 }, { "\302\2401", -1 }, { "1\302\240", -1 },
		{ "\331\241", -1 }, { "\357\274\221", -1 }, { "invalid", -1 }
	};
	const char *raw = " attachment;\r\n\tSIZE*=us-ascii'fr'42;\r\n\tfilename=stable.txt\r\n";
	GMimePart *part = g_mime_part_new ();
	GMimeEntity *entity = GMIME_ENTITY (part);
	GMimeContentDisposition *disposition, *clone, *parsed;
	GMimeParamList *params;
	GMimeParam *param;
	GMimeHeader *header;
	const char *stored;
	char *before, *zeros, *padded;
	gint64 size;
	guint i;

	g_mime_entity_set_disposition (entity, "attachment");
	disposition = g_mime_entity_get_content_disposition (entity);
	header = g_mime_header_list_get_header (g_mime_entity_get_header_list (entity), "Content-Disposition");
	check (g_mime_content_disposition_get_size (disposition) == -1, "missing disposition sizes return -1");
	before = g_strdup (testsuite_header_raw_text (header));
	g_mime_content_disposition_set_size (disposition, -1);
	check (!strcmp (testsuite_header_raw_text (header), before), "removing an absent size does not rewrite its owning header");
	g_free (before);
	testsuite_header_set_raw_text (header, raw);
	disposition = g_mime_entity_get_content_disposition (entity);
	params = g_mime_content_disposition_get_parameters (disposition);
	param = g_mime_param_list_get_parameter (params, "size");
	stored = g_mime_param_get_value (param);
	check (g_mime_content_disposition_get_size (disposition) == 42 &&
	       g_mime_param_get_value (param) == stored && !strcmp (testsuite_header_raw_text (header), raw),
	       "size lookup is case-insensitive and does not rewrite parameter text or raw folding");
	g_mime_content_disposition_set_size (disposition, 42);
	check (g_mime_param_get_value (param) == stored && !strcmp (testsuite_header_raw_text (header), raw),
	       "equivalent size assignment preserves storage and raw folding");
	g_mime_param_set_always_quote (param, TRUE);
	g_mime_content_disposition_set_size (disposition, G_MAXINT64);
	check (g_mime_content_disposition_get_size (disposition) == G_MAXINT64 &&
	       !strcmp (g_mime_param_get_value (param), "9223372036854775807") &&
	       g_mime_param_list_get_parameter_at (params, 0) == param &&
	       !strcmp (g_mime_param_get_name (param), "SIZE") &&
	       !g_ascii_strcasecmp (g_mime_param_get_charset (param), "us-ascii") &&
	       !strcmp (g_mime_param_get_lang (param), "fr") &&
	       g_mime_param_get_encoding_method (param) == GMIME_PARAM_ENCODING_METHOD_RFC2231 &&
	       g_mime_param_get_always_quote (param),
	       "size assignment formats the full signed 64-bit range while preserving parameter identity and metadata");
	parsed = g_mime_content_disposition_parse_full (NULL, g_mime_entity_get_header (entity, "Content-Disposition"), -1, NULL);
	check (parsed && g_mime_content_disposition_get_size (parsed) == G_MAXINT64 &&
	       !strcmp (g_mime_content_disposition_get_filename (parsed), "stable.txt"),
	       "size changes update owning headers without changing unrelated parameters");
	g_object_unref (parsed);
	before = g_strdup (testsuite_header_raw_text (header));
	g_test_expect_message ("gmime", G_LOG_LEVEL_CRITICAL, "*size >= -1*");
	g_mime_content_disposition_set_size (disposition, -2);
	g_test_assert_expected_messages ();
	g_test_expect_message ("gmime", G_LOG_LEVEL_CRITICAL, "*size >= -1*");
	g_mime_content_disposition_set_size (disposition, G_MININT64);
	g_test_assert_expected_messages ();
	check (g_mime_content_disposition_get_size (disposition) == G_MAXINT64 &&
	       !strcmp (testsuite_header_raw_text (header), before), "invalid negative sizes leave existing parameters and headers unchanged");
	g_free (before);
	for (i = 0; i < G_N_ELEMENTS (cases); i++) {
		g_mime_content_disposition_set_parameter (disposition, "size", cases[i].value);
		stored = g_mime_param_get_value (param);
		before = g_strdup (testsuite_header_raw_text (header));
		check (g_mime_content_disposition_get_size (disposition) == cases[i].expected &&
		       g_mime_param_get_value (param) == stored && !strcmp (testsuite_header_raw_text (header), before),
		       "size lookup accepts only nonempty ASCII decimal integers within the signed 64-bit range and never rewrites headers");
		g_free (before);
	}
	zeros = g_strnfill (4096, '0');
	padded = g_strconcat (zeros, "9223372036854775807", NULL);
	g_mime_content_disposition_set_parameter (disposition, "size", padded);
	check (g_mime_content_disposition_get_size (disposition) == G_MAXINT64, "arbitrarily many leading zeros do not cause false size overflow");
	g_free (padded);
	g_free (zeros);
	g_mime_content_disposition_set_parameter (disposition, "size", "00042");
	g_mime_content_disposition_set_size (disposition, 42);
	check (!strcmp (g_mime_param_get_value (param), "42"), "size assignment canonicalizes leading zeros");
	clone = g_mime_content_disposition_clone (disposition);
	g_mime_content_disposition_set_size (clone, -1);
	check (g_mime_content_disposition_get_size (clone) == -1 && g_mime_content_disposition_get_size (disposition) == 42,
	       "removing a cloned size leaves the original parameter independent");
	g_object_unref (clone);
	g_mime_content_disposition_set_size (disposition, 0);
	check (g_mime_content_disposition_get_size (disposition) == 0 && !strcmp (g_mime_param_get_value (param), "0"),
	       "zero sizes remain explicitly stored rather than absent");
	g_mime_content_disposition_set_size (disposition, -1);
	check (g_mime_content_disposition_get_size (disposition) == -1 && g_mime_param_list_length (params) == 1 &&
	       !strstr (testsuite_header_raw_text (header), "SIZE"),
	       "the -1 sentinel removes the size parameter and updates owning headers");
	g_test_expect_message ("gmime", G_LOG_LEVEL_CRITICAL, "*GMIME_IS_CONTENT_DISPOSITION*");
	size = g_mime_content_disposition_get_size (NULL);
	g_test_assert_expected_messages ();
	check (size == -1, "size lookup rejects invalid disposition objects");
	g_test_expect_message ("gmime", G_LOG_LEVEL_CRITICAL, "*GMIME_IS_CONTENT_DISPOSITION*");
	g_mime_content_disposition_set_size (NULL, 0);
	g_test_assert_expected_messages ();
	g_object_unref (part);
	disposition = g_mime_content_disposition_parse (NULL, "attachment; Size=1; SIZE=2");
	params = g_mime_content_disposition_get_parameters (disposition);
	param = g_mime_param_list_get_parameter_at (params, 1);
	g_mime_content_disposition_set_size (disposition, 3);
	check (g_mime_param_list_length (params) == 2 && g_mime_content_disposition_get_size (disposition) == 3 &&
	       !strcmp (g_mime_param_get_value (param), "2"), "size assignment only changes the first legacy duplicate");
	g_mime_content_disposition_set_size (disposition, -1);
	check (g_mime_param_list_length (params) == 1 && g_mime_content_disposition_get_size (disposition) == 2 &&
	       g_mime_param_list_get_parameter_at (params, 0) == param, "size removal only removes the first legacy duplicate");
	g_object_unref (disposition);
}

static void
test_content_boundary_policy (void)
{
	GMimeMultipart *multipart = g_mime_multipart_new ();
	GMimeContentType *type = g_mime_entity_get_content_type (GMIME_ENTITY (multipart));
	const char *boundary;

	g_mime_content_type_set_boundary (type, NULL);
	check (g_mime_content_type_get_boundary (type) == NULL, "Content-Type boundary removal and lookup never generate a replacement");
	boundary = g_mime_multipart_get_boundary (multipart);
	check (boundary && *boundary && boundary == g_mime_content_type_get_boundary (type),
	       "multipart boundary lookup still generates and stores a missing boundary");
	g_mime_content_type_set_boundary (type, "explicit-boundary");
	check (!strcmp (g_mime_multipart_get_boundary (multipart), "explicit-boundary"),
	       "multipart boundary lookup observes Content-Type convenience assignments");
	g_object_unref (multipart);
}

static void
test_international_values (void)
{
	static const struct {
		const char *value, *wire;
		gboolean unicode;
	} cases[] = {
		{ "report.txt", "filename=report.txt", FALSE },
		{ "", "filename=\"\"", FALSE },
		{ "caf\303\251.txt", "filename=\"caf\303\251.txt\"", TRUE },
		{ "\346\265\213\350\257\225\346\226\207\346\234\254.doc",
		  "filename=\"\346\265\213\350\257\225\346\226\207\346\234\254.doc\"", TRUE },
		{ "\"\\\360\237\230\200.txt", "filename=\"\\\"\\\\\360\237\230\200.txt\"", TRUE },
		{ "line\r\nnext", NULL, FALSE },
		{ "\tcontrol\177", NULL, FALSE },
		{ "=?utf-8?q?caf=C3=A9?=", NULL, FALSE },
		{ "\303\251=?utf-8?q?hello?=", NULL, TRUE }
	};
	const char *prefix = "Content-Disposition: attachment";
	GMimeFormatOptions *options = g_mime_format_options_new ();
	GString *str = g_string_new (NULL);
	GMimeParamList *list, *parsed;
	GMimeParam *param, *restored;
	guint i, international, method, fold;
	char *expected;

	g_mime_format_options_set_newline_format (options, GMIME_NEWLINE_FORMAT_DOS);
	for (i = 0; i < G_N_ELEMENTS (cases); i++) {
		list = g_mime_param_list_new ();
		param = g_mime_param_new ("filename", cases[i].value);
		g_mime_param_set_charset (param, "utf-16be");
		g_mime_param_list_add (list, param);
		for (international = 0; international < 2; international++) {
			g_mime_format_options_set_allow_international (options, international);
			for (method = 0; method < 3; method++) {
				g_mime_param_set_encoding_method (param, method);
				for (fold = 0; fold < 2; fold++) {
					g_string_assign (str, prefix);
					g_mime_param_list_encode (list, options, fold, str);
					testsuite_check ("international parameter value[%u,%u,%u,%u]", i, international, method, fold);
					if (cases[i].wire && (international || !cases[i].unicode)) {
						expected = g_strdup_printf ("%s; %s%s", prefix, cases[i].wire, fold ? "\r\n" : "");
						if (!strcmp (str->str, expected))
							testsuite_check_passed ();
						else
							testsuite_check_failed ("expected [%s], got [%s]", expected, str->str);
						g_free (expected);
					} else if (strstr (str->str, method == GMIME_PARAM_ENCODING_METHOD_RFC2047 ?
							   "filename=\"=?" : "filename*") != NULL) {
						testsuite_check_passed ();
					} else {
						testsuite_check_failed ("value requiring transport encoding was written raw: %s", str->str);
					}
					parsed = g_mime_param_list_parse (NULL, str->str + strlen (prefix));
					restored = g_mime_param_list_get_parameter (parsed, "filename");
					check (restored && !strcmp (g_mime_param_get_value (restored), cases[i].value),
					       "international parameter values round-trip without decoding literal encoded words");
					check (!strcmp (g_mime_param_get_charset (param), "utf-16be"),
					       "international formatting preserves the preferred charset");
					g_object_unref (parsed);
				}
			}
		}
		g_object_unref (param);
		g_object_unref (list);
	}
	g_string_free (str, TRUE);
	g_mime_format_options_unref (options);
}

static void
test_international_continuations (void)
{
	static const struct {
		const char *charset, *prefix, *text, *lang;
	} cases[] = {
		{ NULL, "", "\346\227\245\346\234\254", NULL },
		{ "iso-8859-1", "", "\303\251", NULL },
		{ "utf-16be", "", "\360\237\230\200", NULL },
		{ "iso-2022-jp", "", "\346\227\245", NULL },
		{ "x-gmime-unsupported", "", "\346\227\245", NULL },
		{ NULL, "", " \\\"\346\227\245", NULL },
		{ NULL, "\r\n", "\346\227\245", NULL },
		{ NULL, "=?utf-8?q?hello?=", "\360\237\230\200", NULL },
		{ "utf-16be", "", "\346\227\245", "en" },
		{ "iso-2022-jp", "", "\346\227\245", "ja" }
	};
	static const guint widths[] = { 60, 78, 120 };
	const char *prefix = "Content-Disposition: attachment", *line, *end;
	GMimeFormatOptions *options = g_mime_format_options_new ();
	GString *input = g_string_new (NULL), *str = g_string_new (NULL);
	GMimeParamList *list, *parsed = NULL;
	GMimeParam *param, *restored, *after;
	guint i, j, width, international, method, fold, parser;

	for (i = 0; i < G_N_ELEMENTS (cases); i++) {
		g_string_assign (input, cases[i].prefix);
		for (j = 0; j < 100; j++)
			g_string_append (input, cases[i].text);
		list = g_mime_param_list_new ();
		g_mime_param_list_set_parameter (list, "filename", input->str);
		g_mime_param_list_set_parameter (list, "after", "end");
		param = g_mime_param_list_get_parameter (list, "filename");
		g_mime_param_set_charset (param, cases[i].charset);
		g_mime_param_set_lang (param, cases[i].lang);
		for (width = 0; width < G_N_ELEMENTS (widths); width++) {
			g_mime_format_options_set_max_line_length (options, widths[width]);
			g_mime_format_options_set_newline_format (options, width & 1 ?
				GMIME_NEWLINE_FORMAT_DOS : GMIME_NEWLINE_FORMAT_UNIX);
			for (international = 0; international < 2; international++) {
				g_mime_format_options_set_allow_international (options, international);
				for (method = 0; method < 2; method++) {
					g_mime_param_set_encoding_method (param, method ?
						GMIME_PARAM_ENCODING_METHOD_RFC2047 : GMIME_PARAM_ENCODING_METHOD_RFC2231);
					for (fold = 0; fold < 2; fold++) {
						testsuite_check ("international parameter continuations[%u,%u,%u,%u,%u]",
								 i, widths[width], international, method, fold);
						g_string_assign (str, prefix);
						try {
							g_mime_param_list_encode (list, options, fold, str);
							if (!g_utf8_validate (str->str, str->len, NULL))
								throw (exception_new ("output splits a Unicode character"));
							if (!fold && (strchr (str->str, '\r') || strchr (str->str, '\n')))
								throw (exception_new ("non-folding output contains line breaks"));
							for (line = str->str; fold && *line; line = *end ? end + 1 : end) {
								end = strchr (line, '\n');
								if (!end)
									end = line + strlen (line);
								if (end - line - (end > line && end[-1] == '\r' ? 1 : 0) > widths[width])
									throw (exception_new ("physical line exceeds the configured byte width"));
								if (*end && (width & 1) && (end == line || end[-1] != '\r'))
									throw (exception_new ("DOS folding lacks CRLF"));
							}
							if (international && !method && !*cases[i].prefix && !cases[i].lang &&
							    (!strstr (str->str, "filename*0=") || strstr (str->str, "*=")))
								throw (exception_new ("safe international value was transport encoded"));
							if (!international) {
								for (line = str->str; *line; line++)
									if ((unsigned char) *line > 127)
										throw (exception_new ("non-international output contains raw UTF-8"));
							}
							for (parser = 0; parser < 2; parser++) {
								parsed = parser ? g_mime_param_list_parse_full (NULL, str->str + strlen (prefix),
									str->len - strlen (prefix), NULL) :
									g_mime_param_list_parse (NULL, str->str + strlen (prefix));
								if (!parsed)
									throw (exception_new ("parser rejected serialized parameters"));
								restored = g_mime_param_list_get_parameter (parsed, "filename");
								after = g_mime_param_list_get_parameter (parsed, "after");
								if (!restored || strcmp (g_mime_param_get_value (restored), input->str) ||
								    !after || strcmp (g_mime_param_get_value (after), "end"))
									throw (exception_new ("parameter or following entry did not round-trip"));
								if (!method && cases[i].lang && g_strcmp0 (g_mime_param_get_lang (restored), cases[i].lang))
									throw (exception_new ("RFC 2231 language was not preserved"));
								g_clear_object (&parsed);
							}
							if (g_strcmp0 (g_mime_param_get_charset (param), cases[i].charset) ||
							    g_strcmp0 (g_mime_param_get_lang (param), cases[i].lang))
								throw (exception_new ("serialization changed the stored metadata"));
							testsuite_check_passed ();
						} catch (ex) {
							testsuite_check_failed ("%s", ex->message);
						} finally;
						g_clear_object (&parsed);
					}
				}
			}
		}
		g_object_unref (list);
	}
	g_string_free (input, TRUE);
	g_string_free (str, TRUE);
	g_mime_format_options_unref (options);
}

static void
test_international_headers (void)
{
	const char *value = "\346\265\213\350\257\225\346\226\207\346\234\254.doc";
	GMimeFormatOptions *options = g_mime_format_options_new ();
	GMimeContentType *type = g_mime_content_type_new ("application", "octet-stream");
	GMimeContentDisposition *disposition = g_mime_content_disposition_new ();
	GMimeHeaderList *headers = g_mime_header_list_new (NULL);
	GMimeHeader *header;
	char *wire;

	g_mime_format_options_set_allow_international (options, TRUE);
	g_mime_format_options_set_newline_format (options, GMIME_NEWLINE_FORMAT_DOS);
	g_mime_content_type_set_parameter (type, "name", value);
	g_mime_content_disposition_set_disposition (disposition, "attachment");
	g_mime_content_disposition_set_parameter (disposition, "filename", value);
	check_string (g_mime_content_type_encode (type, options),
		      " application/octet-stream; name=\"\346\265\213\350\257\225\346\226\207\346\234\254.doc\"\r\n",
		      "Content-Type encoding honors international parameter formatting");
	check_string (g_mime_content_disposition_encode (disposition, options),
		      " attachment; filename=\"\346\265\213\350\257\225\346\226\207\346\234\254.doc\"\r\n",
		      "Content-Disposition encoding honors international parameter formatting");
	g_mime_header_list_append (headers, "Content-Disposition", "", NULL);
	header = g_mime_header_list_get_header_at (headers, 0);
	g_mime_header_set_value (header, options, "attachment; filename*=utf-8''%E6%B5%8B%E8%AF%95%E6%96%87%E6%9C%AC.doc", NULL);
	check_string (g_mime_header_list_to_string (headers, options),
		      "Content-Disposition: attachment; filename=\"\346\265\213\350\257\225\346\226\207\346\234\254.doc\"\r\n",
		      "header stream serialization preserves international parameters");
	g_mime_format_options_set_allow_international (options, FALSE);
	wire = g_mime_header_list_to_string (headers, options);
	check (strstr (wire, "filename*=") != NULL && strstr (wire, value) == NULL,
	       "disabling international formatting restores encoded parameter output");
	g_free (wire);
	check (strstr (testsuite_header_raw_text (header), value) != NULL,
	       "serialization with different options does not alter stored raw headers");
	g_mime_format_options_set_allow_international (options, TRUE);
	testsuite_header_set_raw_text (header, " attachment;\r\n\tfilename*=utf-8''caf%C3%A9.doc\r\n");
	check_string (g_mime_header_list_to_string (headers, options),
		      "Content-Disposition: attachment;\r\n\tfilename*=utf-8''caf%C3%A9.doc\r\n",
		      "untouched raw parameter headers retain their original encoding and folding");
	g_object_unref (headers);
	g_object_unref (disposition);
	g_object_unref (type);
	g_mime_format_options_unref (options);
}

static void
test_international_oversized_name (void)
{
	GMimeFormatOptions *options = g_mime_format_options_new ();
	GMimeParamList *list = g_mime_param_list_new (), *parsed;
	GString *str = g_string_new ("Content-Type: text/plain");
	char *name = g_strnfill (100, 'n');
	GMimeParam *restored;

	g_mime_format_options_set_allow_international (options, TRUE);
	g_mime_format_options_set_max_line_length (options, 60);
	g_mime_param_list_set_parameter (list, name, "\360\237\230\200\360\237\230\200");
	g_mime_param_list_encode (list, options, TRUE, str);
	check (g_utf8_validate (str->str, str->len, NULL) && strstr (str->str, "*0=\"\360\237\230\200\"") &&
	       strstr (str->str, "*1=\"\360\237\230\200\""), "oversized names still emit complete international characters and make progress");
	parsed = g_mime_param_list_parse (NULL, strchr (str->str, ';'));
	restored = g_mime_param_list_get_parameter (parsed, name);
	check (restored && !strcmp (g_mime_param_get_value (restored), "\360\237\230\200\360\237\230\200"),
	       "oversized international continuations round-trip");
	g_object_unref (parsed);
	g_object_unref (list);
	g_string_free (str, TRUE);
	g_mime_format_options_unref (options);
	g_free (name);
}

int
main (int argc, char **argv)
{
	g_mime_init ();
	testsuite_init (argc, argv);
	testsuite_start ("MIME parameter model");
	test_construction ();
	test_invalid_arguments ();
	test_lists ();
	test_ownership ();
	test_headers ();
	test_parsed_parameters ();
	test_large_parameter_charsets ();
	test_checked_parsing ();
	test_parameter_comments_and_empty_charsets ();
	test_checked_parse_errors ();
	test_checked_parse_bounds ();
	test_checked_content_fields ();
	test_checked_content_errors ();
	test_checked_content_bounds ();
	test_checked_content_ownership ();
	test_content_cloning ();
	test_content_display ();
	test_content_token_assignments ();
	test_content_type_properties ();
	test_disposition_properties ();
	test_attachment_assignment ();
	test_disposition_dates ();
	test_disposition_size ();
	test_content_boundary_policy ();
	test_international_values ();
	test_international_continuations ();
	test_international_headers ();
	test_international_oversized_name ();
	testsuite_end ();
	g_mime_shutdown ();

	return testsuite_exit ();
}
