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
#include <gmime/gmime-object-private.h>
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
	g_mime_parser_options_unref (options);

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
test_named_values (void)
{
	static const struct {
		const char *(*get) (GMimeReceived *);
		gboolean (*set) (GMimeReceived *, const char *, GError **);
		const char *value;
		const char *invalid;
	} properties[] = {
		{ g_mime_received_get_from, g_mime_received_set_from, "source.example", "-invalid" },
		{ g_mime_received_get_by, g_mime_received_set_by, "[127.1]", "smtp..example" },
		{ g_mime_received_get_via, g_mime_received_set_via, " Frontend Transport ", "a;b" },
		{ g_mime_received_get_with, g_mime_received_set_with, "Microsoft SMTP Server", "a(b)" },
		{ g_mime_received_get_id, g_mime_received_set_id, "<id@example.com> (comment)", "<>" },
		{ g_mime_received_get_for, g_mime_received_set_for, "User <user@example.com>", "Group: a@b;" }
	};
	GMimeReceived *received = g_mime_received_new ();
	GMimeReceivedClause *clause;
	GError *error = NULL;
	guint i;

	g_assert_cmpint (g_mime_received_get_clause_count (received), ==, 0);
	g_assert_null (g_mime_received_get_date_time (received));
	for (i = 0; i < G_N_ELEMENTS (properties); i++) {
		g_assert_null (properties[i].get (received));
		g_assert_true (properties[i].set (received, NULL, &error));
		g_assert_false (properties[i].set (received, "", &error));
		g_assert_error (error, GMIME_ERROR, GMIME_ERROR_PARSE_ERROR);
		g_clear_error (&error);
		g_assert_cmpint (g_mime_received_get_clause_count (received), ==, 0);
	}

	for (i = G_N_ELEMENTS (properties); i > 0; i--)
		g_assert_true (properties[i - 1].set (received, properties[i - 1].value, &error));
	g_assert_no_error (error);
	for (i = 0; i < G_N_ELEMENTS (properties); i++) {
		clause = g_mime_received_get_clause (received, i);
		g_assert_cmpint (g_mime_received_clause_get_id (clause), ==, i + 1);
		g_assert_cmpstr (properties[i].get (received), ==, properties[i].value);
		g_assert_false (properties[i].set (received, properties[i].invalid, &error));
		g_assert_error (error, GMIME_ERROR, GMIME_ERROR_PARSE_ERROR);
		g_clear_error (&error);
		g_assert_cmpstr (properties[i].get (received), ==, properties[i].value);
		g_assert_true (properties[i].set (received, properties[i].get (received), &error));
		g_assert_cmpstr (properties[i].get (received), ==, properties[i].value);
	}
	for (i = 0; i < G_N_ELEMENTS (properties); i++) {
		g_assert_true (properties[i].set (received, NULL, &error));
		g_assert_null (properties[i].get (received));
	}
	g_assert_no_error (error);
	g_assert_cmpint (g_mime_received_get_clause_count (received), ==, 0);
	g_object_unref (received);

	received = parse ("with ESMTP From host (first) (second) by target", -1);
	clause = g_mime_received_get_clause (received, 1);
	g_assert_true (g_mime_received_set_from (received, "new.example", &error));
	g_assert_true (g_mime_received_get_clause (received, 1) == clause);
	g_assert_cmpstr (g_mime_received_clause_get_keyword (clause), ==, "From");
	g_assert_cmpint (g_mime_received_clause_get_comment_count (clause), ==, 2);
	g_assert_cmpint (g_mime_received_clause_get_id (g_mime_received_get_clause (received, 0)), ==,
			 GMIME_RECEIVED_CLAUSE_WITH);
	g_assert_false (g_mime_received_set_from (received, "", &error));
	g_assert_error (error, GMIME_ERROR, GMIME_ERROR_PARSE_ERROR);
	g_clear_error (&error);
	g_assert_cmpstr (g_mime_received_get_from (received), ==, "new.example");
	g_assert_true (g_mime_received_set_via (received, "TCP", &error));
	g_assert_true (g_mime_received_get_clause (received, 0) == clause);
	g_assert_cmpstr (g_mime_received_clause_get_keyword (clause), ==, "From");
	g_assert_no_error (error);
	g_object_unref (received);
}

static void
test_tcp_info (void)
{
	GMimeReceived *received = g_mime_received_new ();
	GMimeReceivedClause *clause;
	const char *alias;
	GError *error = NULL;

	g_mime_received_set_from_tcp_info (received, NULL);
	g_mime_received_set_by_tcp_info (received, NULL);
	g_assert_cmpint (g_mime_received_get_clause_count (received), ==, 0);
	g_assert_null (g_mime_received_get_from_tcp_info (received));
	g_assert_null (g_mime_received_get_by_tcp_info (received));

	g_mime_received_set_by_tcp_info (received, " \tHELO\r\n\v\f\302\240 ");
	g_mime_received_set_from_tcp_info (received, "");
	assert_clause (received, 0, GMIME_RECEIVED_CLAUSE_FROM, "from", "");
	assert_clause (received, 1, GMIME_RECEIVED_CLAUSE_BY, "by", "");
	g_assert_cmpstr (g_mime_received_get_from_tcp_info (received), ==, "");
	g_assert_cmpstr (g_mime_received_get_by_tcp_info (received), ==, "  HELO\v\f\302\240 ");
	g_assert_false (g_mime_received_set_by (received, "", &error));
	g_assert_error (error, GMIME_ERROR, GMIME_ERROR_PARSE_ERROR);
	g_clear_error (&error);
	g_assert_cmpstr (g_mime_received_get_by_tcp_info (received), ==, "  HELO\v\f\302\240 ");
	g_assert_true (g_mime_received_set_from (received, "source.example", &error));
	g_assert_true (g_mime_received_set_by (received, "target.example", &error));

	clause = g_mime_received_get_clause (received, 0);
	g_mime_received_clause_add_comment (clause, "second\r\n\tcomment");
	alias = g_mime_received_clause_get_comment (clause, 1);
	g_mime_received_set_from_tcp_info (received, alias);
	g_assert_cmpint (g_mime_received_clause_get_comment_count (clause), ==, 1);
	g_assert_cmpstr (g_mime_received_get_from_tcp_info (received), ==, "second comment");
	g_mime_received_set_from_tcp_info (received, g_mime_received_get_from_tcp_info (received));
	g_assert_cmpstr (g_mime_received_get_from_tcp_info (received), ==, "second comment");
	g_mime_received_set_from_tcp_info (received, NULL);
	g_assert_null (g_mime_received_get_from_tcp_info (received));
	g_assert_cmpstr (g_mime_received_get_from (received), ==, "source.example");
	g_assert_cmpint (g_mime_received_get_clause_count (received), ==, 2);
	g_mime_received_set_by_tcp_info (received, g_mime_received_get_by (received));
	g_assert_cmpstr (g_mime_received_get_by_tcp_info (received), ==, "target.example");
	g_test_expect_message ("gmime", G_LOG_LEVEL_CRITICAL, "*g_utf8_validate*");
	g_mime_received_set_by_tcp_info (received, "\377");
	g_test_assert_expected_messages ();
	g_assert_cmpstr (g_mime_received_get_by_tcp_info (received), ==, "target.example");
	g_assert_no_error (error);
	g_object_unref (received);
}

static void
test_stable_sorting (void)
{
	GMimeReceived *received = g_mime_received_new ();
	GMimeReceivedClause *first = g_mime_received_clause_new_with_comment ("BY", "target.example", "first", NULL);
	GMimeReceivedClause *second = g_mime_received_clause_new_with_comment ("by", "other.example", "second", NULL);
	GError *error = NULL;

	g_mime_received_add_clause (received, first);
	g_mime_received_add_clause (received, second);
	g_assert_true (g_mime_received_set_from (received, "sender.example", &error));
	g_assert_true (g_mime_received_get_clause (received, 1) == first);
	g_assert_true (g_mime_received_get_clause (received, 2) == second);
	g_assert_cmpstr (g_mime_received_get_by (received), ==, "target.example");
	g_assert_true (g_mime_received_set_by (received, "updated.example", &error));
	g_assert_cmpstr (g_mime_received_clause_get_value (first), ==, "updated.example");
	g_assert_cmpstr (g_mime_received_clause_get_value (second), ==, "other.example");
	g_assert_true (g_mime_received_set_from (received, NULL, &error));
	g_mime_received_set_from_tcp_info (received, "HELO sender.example");
	g_assert_true (g_mime_received_get_clause (received, 1) == first);
	g_assert_true (g_mime_received_get_clause (received, 2) == second);
	g_assert_cmpstr (g_mime_received_get_by_tcp_info (received), ==, "first");
	g_mime_received_set_by_tcp_info (received, "updated");
	g_assert_cmpstr (g_mime_received_clause_get_comment (first, 0), ==, "updated");
	g_assert_cmpstr (g_mime_received_clause_get_comment (second, 0), ==, "second");
	g_assert_true (g_mime_received_set_by (received, NULL, &error));
	g_assert_true (g_mime_received_get_clause (received, 1) == second);
	g_assert_cmpstr (g_mime_received_get_by (received), ==, "other.example");
	g_assert_no_error (error);
	g_object_unref (first);
	g_object_unref (second);
	g_object_unref (received);
}

static void
test_collection (void)
{
	GMimeReceived *received = g_mime_received_new ();
	GMimeReceived *other = g_mime_received_new ();
	GMimeReceivedClause *first = g_mime_received_clause_new ("FrOm", "first.example", NULL);
	GMimeReceivedClause *second = g_mime_received_clause_new ("from", "second.example", NULL);
	GMimeReceivedClause *extension = g_mime_received_clause_new ("x-trace", "same", NULL);
	GMimeReceivedClause *equal = g_mime_received_clause_new ("x-trace", "same", NULL);
	GMimeReceivedClause *weak = first;
	GMimeReceivedClause *extensions[33];
	GError *error = NULL;
	int i;

	g_object_add_weak_pointer (G_OBJECT (first), (gpointer *) &weak);
	g_mime_received_add_clause (received, first);
	g_mime_received_add_clause (received, first);
	g_mime_received_insert_clause (received, 1, second);
	g_mime_received_add_clause (other, first);
	g_object_unref (first);
	g_assert_cmpint (g_mime_received_index_of_clause (received, first), ==, 0);
	g_assert_cmpint (g_mime_received_index_of_clause (received, second), ==, 1);
	g_assert_cmpstr (g_mime_received_get_from (received), ==, "first.example");
	g_assert_true (g_mime_received_set_from (received, "updated.example", &error));
	g_assert_cmpstr (g_mime_received_get_from (other), ==, "updated.example");
	g_assert_cmpstr (g_mime_received_clause_get_value (second), ==, "second.example");
	g_assert_true (g_mime_received_set_from (received, NULL, &error));
	g_assert_cmpint (g_mime_received_index_of_clause (received, first), ==, 1);
	g_assert_cmpstr (g_mime_received_get_from (received), ==, "second.example");
	g_mime_received_set_clause (received, 0, g_mime_received_get_clause (received, 0));
	g_assert_true (g_mime_received_get_clause (received, 0) == second);
	g_assert_true (g_mime_received_remove_clause (received, first));
	g_assert_false (g_mime_received_remove_clause (received, first));
	g_assert_nonnull (weak);
	g_mime_received_set_clause (received, 0, g_mime_received_get_clause (other, 0));
	g_mime_received_clear_clauses (other);
	g_assert_nonnull (weak);
	g_mime_received_remove_clause_at (received, 0);
	g_assert_null (weak);
	g_assert_cmpint (g_mime_received_get_clause_count (received), ==, 0);

	g_mime_received_insert_clause (received, 0, extension);
	g_assert_true (g_mime_received_contains_clause (received, extension));
	g_assert_false (g_mime_received_contains_clause (received, equal));
	g_assert_cmpint (g_mime_received_index_of_clause (received, equal), ==, -1);
	g_assert_false (g_mime_received_remove_clause (received, equal));
	extensions[0] = extension;
	for (i = 0; i < 32; i++) {
		GMimeReceivedClause *clause = g_mime_received_clause_new ("x-other", "value", NULL);
		extensions[i + 1] = clause;
		g_mime_received_add_clause (received, clause);
		g_object_unref (clause);
	}
	g_assert_true (g_mime_received_set_via (received, "TCP", &error));
	g_assert_cmpint (g_mime_received_get_clause_count (received), ==, 34);
	g_assert_cmpint (g_mime_received_clause_get_id (g_mime_received_get_clause (received, 0)), ==,
			 GMIME_RECEIVED_CLAUSE_VIA);
	for (i = 0; i < 33; i++)
		g_assert_true (g_mime_received_get_clause (received, i + 1) == extensions[i]);

	g_test_expect_message ("gmime", G_LOG_LEVEL_CRITICAL, "*GMIME_IS_RECEIVED_CLAUSE*");
	g_mime_received_add_clause (received, NULL);
	g_test_assert_expected_messages ();
	g_test_expect_message ("gmime", G_LOG_LEVEL_CRITICAL, "*index >= 0*");
	g_mime_received_insert_clause (received, -1, extension);
	g_test_assert_expected_messages ();
	g_test_expect_message ("gmime", G_LOG_LEVEL_CRITICAL, "*index >= 0*");
	g_mime_received_set_clause (received, 34, extension);
	g_test_assert_expected_messages ();
	g_test_expect_message ("gmime", G_LOG_LEVEL_CRITICAL, "*index >= 0*");
	g_mime_received_remove_clause_at (received, 34);
	g_test_assert_expected_messages ();
	g_assert_cmpint (g_mime_received_get_clause_count (received), ==, 34);
	g_mime_received_clear_clauses (received);
	g_mime_received_clear_clauses (received);
	g_assert_no_error (error);
	g_object_unref (extension);
	g_object_unref (equal);
	g_object_unref (second);
	g_object_unref (received);
	g_object_unref (other);
}

static void
test_date_assignment (void)
{
	GMimeReceived *received = parse ("From host; Fri, 29 Nov 91 07:13:33 EST", -1);
	GDateTime *date = g_mime_received_get_date_time (received);
	GDateTime *replacement = g_date_time_new_from_iso8601 ("2026-04-18T20:05:38.123456-04:00", NULL);

	g_assert_nonnull (received->date_time_str);
	g_assert_true (g_mime_received_set_from (received, "source.example", NULL));
	g_assert_nonnull (received->date_time_str);
	g_mime_received_set_date_time (received, date);
	g_assert_true (g_mime_received_get_date_time (received) == date);
	g_assert_null (received->date_time_str);
	g_assert_cmpint (g_date_time_get_year (date), ==, 1991);
	g_mime_received_set_date_time (received, replacement);
	g_date_time_unref (replacement);
	date = g_mime_received_get_date_time (received);
	g_assert_cmpint (g_date_time_get_microsecond (date), ==, 123456);
	g_assert_cmpint (g_date_time_get_utc_offset (date), ==, -4 * G_TIME_SPAN_HOUR);
	g_mime_received_clear_clauses (received);
	g_assert_true (g_mime_received_get_date_time (received) == date);
	g_date_time_ref (date);
	g_mime_received_set_date_time (received, NULL);
	g_assert_null (g_mime_received_get_date_time (received));
	g_mime_received_set_date_time (received, NULL);
	g_assert_cmpint (g_date_time_get_year (date), ==, 2026);
	g_date_time_unref (date);
	g_object_unref (received);
}

static void
test_host_constructor (void)
{
	static const struct {
		const char *address;
		const char *comment;
	} literals[] = {
		{ "::", "[::]" }, { "::1", "[::1]" }, { "::2", "[::2]" },
		{ "::c000:201", "[::192.0.2.1]" },
		{ "::ffff:c000:201", "[::ffff:192.0.2.1]" },
		{ "::ffff:0:c000:201", "[::ffff:0:192.0.2.1]" },
		{ "::ffff:0:1", "[::ffff:0:1]" },
		{ "2001:db8:0:1:0:5efe:c000:201", "[2001:db8:0:1:0:5efe:192.0.2.1]" },
		{ "::5efe:0:1", "[::5efe:0.0.0.1]" },
		{ "1:0:0:2:0:0:3:4", "[1::2:0:0:3:4]" }
	};
	GInetAddress *from = g_inet_address_new_from_string ("192.168.1.1");
	GInetAddress *by = g_inet_address_new_from_string ("2001:db8::1");
	GDateTime *date = g_date_time_new_from_iso8601 ("2026-04-18T20:05:38.123456-04:00", NULL);
	GMimeReceived *received;
	GError *error = NULL;
	guint i;

	received = g_mime_received_new_with_hosts ("smtp.source.com", from, "smtp.target.com", by, date, &error);
	g_assert_no_error (error);
	g_assert_nonnull (received);
	g_assert_cmpint (g_mime_received_get_clause_count (received), ==, 2);
	assert_clause (received, 0, GMIME_RECEIVED_CLAUSE_FROM, "from", "smtp.source.com");
	assert_clause (received, 1, GMIME_RECEIVED_CLAUSE_BY, "by", "smtp.target.com");
	g_assert_cmpstr (g_mime_received_get_from_tcp_info (received), ==, "[192.168.1.1]");
	g_assert_cmpstr (g_mime_received_get_by_tcp_info (received), ==, "[2001:db8::1]");
	g_assert_true (g_mime_received_get_date_time (received) == date);
	g_assert_null (received->date_time_str);
	for (i = 0; i < G_N_ELEMENTS (literals); i++) {
		GInetAddress *address = g_inet_address_new_from_string (literals[i].address);
		GMimeReceived *other = g_mime_received_new_with_hosts ("source", address, "target", by, date, &error);

		g_assert_no_error (error);
		g_assert_nonnull (other);
		g_assert_cmpstr (g_mime_received_get_from_tcp_info (other), ==, literals[i].comment);
		g_object_unref (address);
		g_object_unref (other);
	}

	g_assert_null (g_mime_received_new_with_hosts ("-invalid", from, "target", by, date, &error));
	g_assert_error (error, GMIME_ERROR, GMIME_ERROR_PARSE_ERROR);
	g_clear_error (&error);
	g_assert_null (g_mime_received_new_with_hosts ("source", from, "bad..domain", by, date, &error));
	g_assert_error (error, GMIME_ERROR, GMIME_ERROR_PARSE_ERROR);
	g_clear_error (&error);
	g_test_expect_message ("gmime", G_LOG_LEVEL_CRITICAL, "*from != NULL*");
	g_assert_null (g_mime_received_new_with_hosts (NULL, from, "target", by, date, &error));
	g_test_assert_expected_messages ();
	g_test_expect_message ("gmime", G_LOG_LEVEL_CRITICAL, "*G_IS_INET_ADDRESS*");
	g_assert_null (g_mime_received_new_with_hosts ("source", NULL, "target", by, date, &error));
	g_test_assert_expected_messages ();
	g_test_expect_message ("gmime", G_LOG_LEVEL_CRITICAL, "*by != NULL*");
	g_assert_null (g_mime_received_new_with_hosts ("source", from, NULL, by, date, &error));
	g_test_assert_expected_messages ();
	g_test_expect_message ("gmime", G_LOG_LEVEL_CRITICAL, "*G_IS_INET_ADDRESS*");
	g_assert_null (g_mime_received_new_with_hosts ("source", from, "target", NULL, date, &error));
	g_test_assert_expected_messages ();
	g_test_expect_message ("gmime", G_LOG_LEVEL_CRITICAL, "*date_time != NULL*");
	g_assert_null (g_mime_received_new_with_hosts ("source", from, "target", by, NULL, &error));
	g_test_assert_expected_messages ();
	g_assert_no_error (error);
	g_object_unref (from);
	g_object_unref (by);
	g_date_time_unref (date);
	g_assert_cmpstr (g_mime_received_get_by_tcp_info (received), ==, "[2001:db8::1]");
	g_assert_cmpint (g_date_time_get_microsecond (g_mime_received_get_date_time (received)), ==, 123456);
	g_object_unref (received);
}

static void
assert_format (GMimeReceived *received, GMimeFormatOptions *options, const char *expected)
{
	char *formatted = g_mime_received_to_string (received, options);

	g_assert_cmpstr (formatted, ==, expected);
	g_free (formatted);
}

static void
test_formatting (void)
{
	static const struct {
		const char *input;
		const char *output;
	} cases[] = {
		{ "", "\r\n" }, { ";", "\r\n" },
		{ "(first)(second)", " (first) (second)\r\n" },
		{ "From a BY b", " From a BY b\r\n" },
		{ "from (before) host (after)", " from host (before) (after)\r\n" },
		{ "from host (a(b)c)", " from host (a(b)c)\r\n" },
		{ "from host (a\\\\b)", " from host (a\\\\b)\r\n" },
		{ "from", " from \r\n" },
		{ "From host; Fri, 29 Nov 91 07:13:33 EST", " From host; Fri, 29 Nov 91 07:13:33 EST\r\n" },
		{ "; Fri, 29 Nov 91 07:13:33 EST", "; Fri, 29 Nov 91 07:13:33 EST\r\n" }
	};
	GMimeFormatOptions *options = g_mime_format_options_new ();
	GMimeReceived *received;
	GMimeReceivedClause *clause;
	GDateTime *date;
	GTimeZone *zone;
	GString *unicode;
	char *value, *expected, *keyword;
	guint i;

	g_mime_format_options_set_newline_format (options, GMIME_NEWLINE_FORMAT_DOS);
	for (i = 0; i < G_N_ELEMENTS (cases); i++) {
		received = parse (cases[i].input, -1);
		assert_format (received, options, cases[i].output);
		g_object_unref (received);
	}
	received = parse ("From host; Fri, 29 Nov 91 07:13:33 EST", -1);
	assert_format (received, options, " From host; Fri, 29 Nov 91 07:13:33 EST\r\n");
	g_mime_received_set_date_time (received, g_mime_received_get_date_time (received));
	assert_format (received, options, " From host; Fri, 29 Nov 1991 07:13:33 -0500\r\n");
	g_mime_received_set_date_time (received, NULL);
	clause = g_mime_received_get_clause (received, 0);
	g_mime_received_clause_add_comment (clause, "comment\r\nBcc: evil@example.com\t\\");
	assert_format (received, options, " From host (commentBcc: evil@example.com \\\\)\r\n");
	g_assert_cmpstr (g_mime_received_clause_get_comment (clause, 0), ==, "comment\r\nBcc: evil@example.com\t\\");
	g_mime_received_clause_clear_comments (clause);
	g_mime_received_add_clause (received, clause);
	assert_format (received, options, " From host From host\r\n");
	g_mime_received_clear_clauses (received);

	g_mime_format_options_set_max_line_length (options, 60);
	value = g_strnfill (52, 'x');
	clause = g_mime_received_clause_new_with_comment ("via", value, "", NULL);
	g_mime_received_add_clause (received, clause);
	g_object_unref (clause);
	expected = g_strdup_printf ("\r\n\tvia %s ()\r\n", value);
	assert_format (received, options, expected);
	g_free (expected);
	g_free (value);
	value = g_strnfill (53, 'x');
	g_assert_true (g_mime_received_set_via (received, value, NULL));
	expected = g_strdup_printf ("\r\n\tvia %s\r\n\t()\r\n", value);
	assert_format (received, options, expected);
	g_free (expected);
	g_free (value);
	g_mime_received_clear_clauses (received);

	keyword = g_strnfill (80, 'x');
	clause = g_mime_received_clause_new (keyword, "v", NULL);
	g_mime_received_add_clause (received, clause);
	g_object_unref (clause);
	expected = g_strdup_printf ("\r\n\t%s\r\n\tv\r\n", keyword);
	assert_format (received, options, expected);
	g_free (expected);
	g_free (keyword);
	g_mime_received_clear_clauses (received);

	value = g_strnfill (80, 'x');
	g_assert_true (g_mime_received_set_via (received, value, NULL));
	expected = g_strdup_printf (" via\r\n\t%s\r\n", value);
	assert_format (received, options, expected);
	g_free (expected);
	g_free (value);
	g_mime_received_clear_clauses (received);

	value = g_strnfill (46, 'x');
	g_assert_true (g_mime_received_set_via (received, value, NULL));
	date = g_date_time_new_from_iso8601 ("2026-04-18T20:05:38-04:00", NULL);
	g_mime_received_set_date_time (received, date);
	g_date_time_unref (date);
	expected = g_strdup_printf (" via %s\r\n\t; Sat, 18 Apr 2026 20:05:38 -0400\r\n", value);
	assert_format (received, options, expected);
	g_free (expected);
	value[45] = '\0';
	g_assert_true (g_mime_received_set_via (received, value, NULL));
	expected = g_strdup_printf (" via %s;\r\n\tSat, 18 Apr 2026 20:05:38 -0400\r\n", value);
	assert_format (received, options, expected);
	g_free (expected);
	g_free (value);
	g_mime_received_clear_clauses (received);

	zone = g_time_zone_new_offset (30);
	date = g_date_time_new (zone, 2026, 4, 18, 20, 5, 38);
	g_mime_received_set_date_time (received, date);
	g_date_time_unref (date);
	assert_format (received, options, "; Sat, 18 Apr 2026 20:05:08 -0000\r\n");
	date = g_date_time_new (zone, 1, 1, 1, 0, 0, 0);
	g_mime_received_set_date_time (received, date);
	g_date_time_unref (date);
	g_time_zone_unref (zone);
	g_test_expect_message ("gmime", G_LOG_LEVEL_CRITICAL, "*utc != NULL*");
	g_assert_null (g_mime_received_to_string (received, options));
	g_test_assert_expected_messages ();
	g_mime_received_set_date_time (received, NULL);
	unicode = g_string_new (NULL);
	for (i = 0; i < 23; i++)
		g_string_append (unicode, "\360\237\230\200");
	g_assert_true (g_mime_received_set_via (received, unicode->str, NULL));
	expected = g_strdup_printf (" via %s\r\n", unicode->str);
	assert_format (received, options, expected);
	g_free (expected);
	g_string_append (unicode, "\360\237\230\200");
	g_assert_true (g_mime_received_set_via (received, unicode->str, NULL));
	expected = g_strdup_printf ("\r\n\tvia %s\r\n", unicode->str);
	assert_format (received, options, expected);
	g_free (expected);
	g_string_free (unicode, TRUE);
	g_object_unref (received);

	received = parse ("x \303\251\360\237\230\200", -1);
	assert_format (received, options, " x \303\251\360\237\230\200\r\n");
	g_mime_format_options_set_newline_format (options, GMIME_NEWLINE_FORMAT_UNIX);
	assert_format (received, options, " x \303\251\360\237\230\200\n");
	g_mime_format_options_set_max_line_length (options, 78);
	assert_format (received, NULL, g_str_has_suffix (g_mime_format_options_get_newline (NULL), "\r\n") ?
		       " x \303\251\360\237\230\200\r\n" : " x \303\251\360\237\230\200\n");
	g_object_unref (received);
	g_mime_format_options_unref (options);
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
	testsuite_check ("Named clause values, validation and sorting");
	test_named_values ();
	testsuite_check_passed ();
	testsuite_check ("TCP comments, unfolding and aliased assignment");
	test_tcp_info ();
	testsuite_check_passed ();
	testsuite_check ("Stable duplicate ordering and first-match mutation");
	test_stable_sorting ();
	testsuite_check_passed ();
	testsuite_check ("Owning clause collection and shared instances");
	test_collection ();
	testsuite_check_passed ();
	testsuite_check ("Date assignment, aliases and cached spelling");
	test_date_assignment ();
	testsuite_check_passed ();
	testsuite_check ("Host/IP construction, ownership and validation");
	test_host_constructor ();
	testsuite_check_passed ();
	testsuite_check ("Serialization, folding, comment escaping and cached dates");
	test_formatting ();
	testsuite_check_passed ();
	testsuite_check ("Parse errors and programming-error guards");
	test_errors ();
	testsuite_check_passed ();
	testsuite_end ();
	g_mime_shutdown ();

	return testsuite_exit ();
}
