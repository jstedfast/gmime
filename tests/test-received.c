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

#include <string.h>
#include <gmime/gmime.h>
#include "testsuite.h"

static GMimeReceived *
parse (const char *text, gssize length)
{
	GError *error = NULL;
	GMimeReceived *received = g_mime_received_parse (NULL, text, length, &error);

	g_assert_no_error (error);
	g_assert_nonnull (received);
	return received;
}

static void
assert_clause (GMimeReceived *received, int index, GMimeReceivedClauseId id,
	       const char *keyword, const char *value)
{
	GMimeReceivedClause *clause = g_mime_received_get_clause (received, index);

	g_assert_cmpint (g_mime_received_clause_get_id (clause), ==, id);
	g_assert_cmpstr (g_mime_received_clause_get_keyword (clause), ==, keyword);
	g_assert_cmpstr (g_mime_received_clause_get_value (clause), ==, value);
}

static void
test_clauses (void)
{
	GMimeReceived *received;
	GMimeReceivedClause *clause;
	GError *error = NULL;
	const char *empty[] = { "", " \r\n\t", ";", "; \r\n\t" };
	guint i;

	for (i = 0; i < G_N_ELEMENTS (empty); i++) {
		received = parse (empty[i], -1);
		g_assert_cmpint (g_mime_received_get_clause_count (received), ==, 0);
		g_assert_null (g_mime_received_get_date_time (received));
		g_object_unref (received);
	}

	received = parse (" From  host.example by [::1]\r\n\tvia Frontend Transport "
			  "with Microsoft SMTP Server id <a@b> for <user@example.com>", -1);
	g_assert_cmpint (g_mime_received_get_clause_count (received), ==, 6);
	assert_clause (received, 0, GMIME_RECEIVED_CLAUSE_FROM, "From", "host.example");
	assert_clause (received, 1, GMIME_RECEIVED_CLAUSE_BY, "by", "[::1]");
	assert_clause (received, 2, GMIME_RECEIVED_CLAUSE_VIA, "via", "Frontend Transport");
	assert_clause (received, 3, GMIME_RECEIVED_CLAUSE_WITH, "with", "Microsoft SMTP Server");
	assert_clause (received, 4, GMIME_RECEIVED_CLAUSE_ID, "id", "<a@b>");
	assert_clause (received, 5, GMIME_RECEIVED_CLAUSE_FOR, "for", "<user@example.com>");
	g_assert_null (g_mime_received_get_date_time (received));
	g_object_unref (received);

	received = parse ("from by (before) with; ", -1);
	g_assert_cmpint (g_mime_received_get_clause_count (received), ==, 3);
	assert_clause (received, 0, GMIME_RECEIVED_CLAUSE_FROM, "from", "");
	assert_clause (received, 1, GMIME_RECEIVED_CLAUSE_BY, "by", "");
	assert_clause (received, 2, GMIME_RECEIVED_CLAUSE_WITH, "with", "");
	clause = g_mime_received_get_clause (received, 1);
	g_assert_cmpstr (g_mime_received_clause_get_comment (clause, 0), ==, "before");
	g_object_unref (received);

	received = parse ("-x! (before) not-a-domain.. (after) -x! end from -invalid", -1);
	g_assert_cmpint (g_mime_received_get_clause_count (received), ==, 3);
	assert_clause (received, 0, GMIME_RECEIVED_CLAUSE_UNKNOWN, "-x!", "not-a-domain..");
	assert_clause (received, 1, GMIME_RECEIVED_CLAUSE_UNKNOWN, "-x!", "end");
	assert_clause (received, 2, GMIME_RECEIVED_CLAUSE_FROM, "from", "-invalid");
	clause = g_mime_received_get_clause (received, 2);
	g_assert_false (g_mime_received_clause_set_value (clause, "-invalid", &error));
	g_assert_error (error, GMIME_ERROR, GMIME_ERROR_PARSE_ERROR);
	g_clear_error (&error);
	g_assert_cmpstr (g_mime_received_clause_get_value (clause), ==, "-invalid");
	g_object_ref (clause);
	g_object_unref (received);
	g_assert_cmpstr (g_mime_received_clause_get_keyword (clause), ==, "from");
	g_object_unref (clause);
}

static void
test_comments (void)
{
	GMimeReceived *received;
	GMimeReceivedClause *clause;
	GString *text;
	int i;

	received = parse (" (leading (nested)) (second)\r\nfrom (before)\t"
			  "host (a\\(b\\)\\\\c\\\t\\\r\\\n) (HELO\r\n\tworld) by target", -1);
	g_assert_cmpint (g_mime_received_get_clause_count (received), ==, 3);
	assert_clause (received, 0, GMIME_RECEIVED_CLAUSE_NONE, "", "");
	clause = g_mime_received_get_clause (received, 0);
	g_assert_cmpint (g_mime_received_clause_get_comment_count (clause), ==, 2);
	g_assert_cmpstr (g_mime_received_clause_get_comment (clause, 0), ==, "leading (nested)");
	g_assert_cmpstr (g_mime_received_clause_get_comment (clause, 1), ==, "second");
	clause = g_mime_received_get_clause (received, 1);
	g_assert_cmpint (g_mime_received_clause_get_comment_count (clause), ==, 3);
	g_assert_cmpstr (g_mime_received_clause_get_comment (clause, 0), ==, "before");
	g_assert_cmpstr (g_mime_received_clause_get_comment (clause, 1), ==, "a(b)\\c ");
	g_assert_cmpstr (g_mime_received_clause_get_comment (clause, 2), ==, "HELO world");
	g_object_unref (received);

	text = g_string_new ("");
	for (i = 0; i < 10000; i++)
		g_string_append_c (text, '(');
	for (i = 0; i < 10000; i++)
		g_string_append_c (text, ')');
	received = parse (text->str, text->len);
	g_assert_cmpint (g_mime_received_get_clause_count (received), ==, 1);
	clause = g_mime_received_get_clause (received, 0);
	g_assert_cmpint (strlen (g_mime_received_clause_get_comment (clause, 0)), ==, 19998);
	g_object_unref (received);
	g_string_free (text, TRUE);
}

static void
test_dates (void)
{
	GMimeReceived *received = parse ("from host; Sat, 18 Apr 2026\r\n\t20:05:38 -0400 \r\n", -1);
	GDateTime *date = g_mime_received_get_date_time (received);
	char *formatted;

	g_assert_nonnull (date);
	g_assert_cmpint (g_date_time_get_year (date), ==, 2026);
	g_assert_cmpint (g_date_time_get_month (date), ==, 4);
	g_assert_cmpint (g_date_time_get_day_of_month (date), ==, 18);
	g_assert_cmpint (g_date_time_get_hour (date), ==, 20);
	g_assert_cmpint (g_date_time_get_minute (date), ==, 5);
	g_assert_cmpint (g_date_time_get_second (date), ==, 38);
	g_assert_cmpint (g_date_time_get_utc_offset (date), ==, -4 * G_TIME_SPAN_HOUR);
	g_date_time_ref (date);
	g_object_unref (received);
	formatted = g_mime_utils_header_format_date (date);
	g_assert_cmpstr (formatted, ==, "Sat, 18 Apr 2026 20:05:38 -0400");
	g_free (formatted);
	g_date_time_unref (date);

	received = parse ("; Fri, 29 Nov 91 07:13:33 EST", -1);
	date = g_mime_received_get_date_time (received);
	g_assert_cmpint (g_date_time_get_year (date), ==, 1991);
	g_assert_cmpint (g_date_time_get_utc_offset (date), ==, -5 * G_TIME_SPAN_HOUR);
	g_object_unref (received);
}

static void
test_bounded_input (void)
{
	static const char bytes[] = "fr\0om va\0lue (a\0b) by host";
	static const char date[] = "; ignored\0 18 Apr 2026 20:05:38 -0400";
	static const char bounded_date[] = "; (nested (date)) 18 Apr 2026 20:05:38 -0400";
	GMimeParserOptions *options = g_mime_parser_options_new ();
	GMimeReceived *received;
	GMimeReceivedClause *clause;
	char *buffer = g_memdup2 ("from host by host", 9);

	received = parse (buffer, 9);
	memset (buffer, 'x', 9);
	g_free (buffer);
	g_assert_cmpint (g_mime_received_get_clause_count (received), ==, 1);
	assert_clause (received, 0, GMIME_RECEIVED_CLAUSE_FROM, "from", "host");
	g_object_unref (received);

	received = parse (bytes, sizeof (bytes) - 1);
	g_assert_cmpint (g_mime_received_get_clause_count (received), ==, 2);
	assert_clause (received, 0, GMIME_RECEIVED_CLAUSE_UNKNOWN, "fr\357\277\275om", "va\357\277\275lue");
	clause = g_mime_received_get_clause (received, 0);
	g_assert_cmpstr (g_mime_received_clause_get_comment (clause, 0), ==, "a\357\277\275b");
	g_object_unref (received);

	received = parse (date, sizeof (date) - 1);
	g_assert_nonnull (g_mime_received_get_date_time (received));
	g_object_unref (received);

	buffer = g_memdup2 (bounded_date, sizeof (bounded_date) - 1);
	received = parse (buffer, sizeof (bounded_date) - 1);
	g_free (buffer);
	g_assert_cmpint (g_date_time_get_day_of_month (g_mime_received_get_date_time (received)), ==, 18);
	g_object_unref (received);

	g_assert_true (g_mime_parser_options_set_charset_encoding (options, "windows-1252", NULL));
	received = g_mime_received_parse (options, "from \200 (\200)", -1, NULL);
	g_assert_nonnull (received);
	assert_clause (received, 0, GMIME_RECEIVED_CLAUSE_FROM, "from", "\342\202\254");
	clause = g_mime_received_get_clause (received, 0);
	g_assert_cmpstr (g_mime_received_clause_get_comment (clause, 0), ==, "\342\202\254");
	g_object_unref (received);
	g_mime_parser_options_free (options);

	received = parse ("from =?utf-8?q?caf=C3=A9?=", -1);
	assert_clause (received, 0, GMIME_RECEIVED_CLAUSE_FROM, "from", "=?utf-8?q?caf=C3=A9?=");
	g_object_unref (received);

	received = parse ("from a\v\fb (a\v\fb)", -1);
	assert_clause (received, 0, GMIME_RECEIVED_CLAUSE_FROM, "from", "a\v\fb");
	clause = g_mime_received_get_clause (received, 0);
	g_assert_cmpstr (g_mime_received_clause_get_comment (clause, 0), ==, "a\v\fb");
	g_object_unref (received);
}

static void
test_errors (void)
{
	static const struct {
		const char *text;
		int offset;
	} inputs[] = {
		{ "from a FROM b", 7 }, { "by a by b", 5 }, { "via a VIA b", 6 },
		{ "with a WITH b", 7 }, { "id a ID b", 5 }, { "for a FOR b", 6 },
		{ "(incomplete", 0 }, { "(escaped\\", 0 }, { "from (incomplete", 5 },
		{ "from host (incomplete", 10 }, { "; not a date", 2 }
	};
	GMimeReceived *received;
	GError *error = NULL;
	char *buffer, *suffix;
	guint i;

	for (i = 0; i < G_N_ELEMENTS (inputs); i++) {
		buffer = g_memdup2 (inputs[i].text, strlen (inputs[i].text));
		g_assert_null (g_mime_received_parse (NULL, buffer, strlen (inputs[i].text), &error));
		g_free (buffer);
		g_assert_error (error, GMIME_ERROR, GMIME_ERROR_PARSE_ERROR);
		suffix = g_strdup_printf ("offset %d.", inputs[i].offset);
		g_assert_true (g_str_has_suffix (error->message, suffix));
		g_free (suffix);
		g_clear_error (&error);
		g_assert_null (g_mime_received_parse (NULL, inputs[i].text, -1, NULL));
	}

	if (G_MAXSSIZE > G_MAXINT) {
		g_assert_null (g_mime_received_parse (NULL, "", (gssize) ((guint64) G_MAXINT + 1), &error));
		g_assert_error (error, GMIME_ERROR, GMIME_ERROR_PARSE_ERROR);
		g_clear_error (&error);
	}

	g_test_expect_message ("gmime", G_LOG_LEVEL_CRITICAL, "*text != NULL*");
	g_assert_null (g_mime_received_parse (NULL, NULL, 0, &error));
	g_test_assert_expected_messages ();
	g_test_expect_message ("gmime", G_LOG_LEVEL_CRITICAL, "*length >= -1*");
	g_assert_null (g_mime_received_parse (NULL, "", -2, &error));
	g_test_assert_expected_messages ();
	received = parse ("", 0);
	g_test_expect_message ("gmime", G_LOG_LEVEL_CRITICAL, "*index >= 0*");
	g_assert_null (g_mime_received_get_clause (received, 0));
	g_test_assert_expected_messages ();
	g_object_unref (received);
	g_assert_no_error (error);
}

int
main (int argc, char **argv)
{
	g_mime_init ();
	testsuite_init (argc, argv);
	testsuite_start ("Received header parsing");
	testsuite_check ("Clause recovery, ordering and retained ownership");
	test_clauses ();
	testsuite_check_passed ();
	testsuite_check ("Leading, nested, escaped and folded comments");
	test_comments ();
	testsuite_check_passed ();
	testsuite_check ("Date parsing and timezone offsets");
	test_dates ();
	testsuite_check_passed ();
	testsuite_check ("Bounded byte input, NULs and fallback charsets");
	test_bounded_input ();
	testsuite_check_passed ();
	testsuite_check ("Parse errors and programming-error guards");
	test_errors ();
	testsuite_check_passed ();
	testsuite_end ();
	g_mime_shutdown ();

	return testsuite_exit ();
}
