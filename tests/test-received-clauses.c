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

static void
test_keywords (void)
{
	static const char *keywords[] = { "from", "BY", "via", "With", "id", "For", "X-Trace" };
	static const char *values[] = { "host.example", "[::1]", "TCP", "ESMTP", "<a@b>", "<a@b>", "text" };
	static const char *invalid[] = { "", "-from", "with2", "with ", "a_b", "a.b", "\303\251", "from\n" };
	GMimeReceivedClause *clause;
	GError *error = NULL;
	guint i;
	char *keyword, *value, *comment;

	for (i = 0; i < G_N_ELEMENTS (keywords); i++) {
		keyword = g_strdup (keywords[i]);
		value = g_strdup (values[i]);
		comment = g_strdup ("comment");
		clause = g_mime_received_clause_new_with_comment (keyword, value, comment, &error);
		g_assert_no_error (error);
		g_assert_nonnull (clause);
		g_free (keyword);
		g_free (value);
		g_free (comment);
		g_assert_cmpint (g_mime_received_clause_get_id (clause), ==, i + 1);
		g_assert_cmpstr (g_mime_received_clause_get_keyword (clause), ==, keywords[i]);
		g_assert_cmpstr (g_mime_received_clause_get_value (clause), ==, values[i]);
		g_assert_cmpint (g_mime_received_clause_get_comment_count (clause), ==, 1);
		g_assert_cmpstr (g_mime_received_clause_get_comment (clause, 0), ==, "comment");
		g_object_unref (clause);
	}

	for (i = 0; i < G_N_ELEMENTS (invalid); i++) {
		g_assert_null (g_mime_received_clause_new (invalid[i], "value", &error));
		g_assert_error (error, GMIME_ERROR, GMIME_ERROR_PARSE_ERROR);
		g_clear_error (&error);
		g_assert_null (g_mime_received_clause_new (invalid[i], "value", NULL));
	}
}

static void
test_values (void)
{
	static const struct {
		const char *keyword;
		const char *value;
		gboolean valid;
	} cases[] = {
		{ "from", "example.com", TRUE },
		{ "from", "example.", TRUE },
		{ "from", "a-/b", TRUE },
		{ "from", "\346\227\245\346\234\254.example", TRUE },
		{ "from", "[127.1]", TRUE },
		{ "from", "[0x7f000001]", TRUE },
		{ "from", "[0177.1]", TRUE },
		{ "from", "[1.2.65535]", TRUE },
		{ "from", "[::1]", TRUE },
		{ "from", "[[::1]:0x50]", TRUE },
		{ "from", "[fe80::1%unknown-interface]", TRUE },
		{ "from", "[fe80::1%a%b]", TRUE },
		{ "from", "[fe80::1%]", TRUE },
		{ "from", "[fe80::1%a/b]", FALSE },
		{ "from", "[fe80::1%a]b]", FALSE },
		{ "from", "[[fe80::1%a/b]:25]", FALSE },
		{ "by", "[::ffff:127.0.0.1%a/b]", FALSE },
		{ "from", "[4294967296]", FALSE },
		{ "from", "[1.2.65536]", FALSE },
		{ "from", "[08]", FALSE },
		{ "from", "[1.2.3.]", FALSE },
		{ "from", "[::ffff:127.1]", FALSE },
		{ "from", "[not-an-ip]", FALSE },
		{ "by", "a..b", FALSE },
		{ "by", "a.-b", FALSE },
		{ "by", "-host", FALSE },
		{ "by", "a b", FALSE },
		{ "by", "a\\b", FALSE },
		{ "by", "a[b", FALSE },
		{ "id", "a b", TRUE },
		{ "id", "<local@@host>", TRUE },
		{ "id", "<local@>", TRUE },
		{ "id", "<a@b> (comment)", TRUE },
		{ "id", "<>", FALSE },
		{ "id", "<a@b> <c@d>", FALSE },
		{ "id", "a\nb", FALSE },
		{ "for", "user@example.com", TRUE },
		{ "for", "User <user@example.com>", TRUE },
		{ "for", "user", TRUE },
		{ "for", "user@example.com,other@example.com", FALSE },
		{ "for", "Group: user@example.com;", FALSE },
		{ "with", " Microsoft SMTP Server ", TRUE },
		{ "with", "(comment)", FALSE },
		{ "via", "a;b", FALSE },
		{ "x-custom", "a)b", FALSE },
		{ "with", "a\tb", FALSE },
		{ "with", "a\177b", FALSE },
		{ "with", "caf\303\251", TRUE },
		{ "with", "", FALSE },
		{ "with", "\377", FALSE }
	};
	GMimeReceivedClause *clause;
	GError *error = NULL;
	const char *seed, *before;
	guint i;

	for (i = 0; i < G_N_ELEMENTS (cases); i++) {
		clause = g_mime_received_clause_new (cases[i].keyword, cases[i].value, &error);
		if (cases[i].valid) {
			g_assert_no_error (error);
			g_assert_nonnull (clause);
			g_assert_cmpstr (g_mime_received_clause_get_value (clause), ==, cases[i].value);
			g_object_unref (clause);
		} else {
			g_assert_null (clause);
			g_assert_error (error, GMIME_ERROR, GMIME_ERROR_PARSE_ERROR);
			g_clear_error (&error);
		}

		seed = !strcmp (cases[i].keyword, "for") ? "user@example.com" : "seed";
		clause = g_mime_received_clause_new_with_comment (cases[i].keyword, seed, "", &error);
		g_assert_no_error (error);
		before = g_mime_received_clause_get_value (clause);
		g_assert_cmpint (g_mime_received_clause_set_value (clause, cases[i].value, &error), ==, cases[i].valid);
		if (cases[i].valid) {
			g_assert_no_error (error);
			g_assert_cmpstr (g_mime_received_clause_get_value (clause), ==, cases[i].value);
			g_assert_true (g_mime_received_clause_set_value (clause, g_mime_received_clause_get_value (clause), &error));
			g_assert_no_error (error);
		} else {
			g_assert_error (error, GMIME_ERROR, GMIME_ERROR_PARSE_ERROR);
			g_clear_error (&error);
			g_assert_true (before == g_mime_received_clause_get_value (clause));
			g_assert_false (g_mime_received_clause_set_value (clause, cases[i].value, NULL));
		}
		g_assert_cmpint (g_mime_received_clause_get_comment_count (clause), ==, 1);
		g_assert_cmpstr (g_mime_received_clause_get_comment (clause, 0), ==, "");
		g_object_unref (clause);
	}
}

static void
test_comments (void)
{
	GMimeReceivedClause *clause;
	GError *error = NULL;

	clause = g_mime_received_clause_new ("with", "ESMTP", &error);
	g_assert_no_error (error);
	g_assert_cmpint (g_mime_received_clause_get_comment_count (clause), ==, 0);
	g_mime_received_clause_clear_comments (clause);
	g_mime_received_clause_insert_comment (clause, 0, "");
	g_mime_received_clause_add_comment (clause, "(nested)\\escaped\r\n\tcomment");
	g_mime_received_clause_insert_comment (clause, 1, "same");
	g_mime_received_clause_insert_comment (clause, 3, "SAME");
	g_mime_received_clause_add_comment (clause, g_mime_received_clause_get_comment (clause, 1));
	g_assert_cmpint (g_mime_received_clause_index_of_comment (clause, "same"), ==, 1);
	g_assert_cmpint (g_mime_received_clause_index_of_comment (clause, "SAME"), ==, 3);
	g_assert_cmpint (g_mime_received_clause_index_of_comment (clause, "Same"), ==, -1);
	g_mime_received_clause_set_comment (clause, 1, g_mime_received_clause_get_comment (clause, 1));
	g_mime_received_clause_set_comment (clause, 0, g_mime_received_clause_get_comment (clause, 2));
	g_assert_cmpstr (g_mime_received_clause_get_comment (clause, 0), ==, "(nested)\\escaped\r\n\tcomment");
	g_assert_true (g_mime_received_clause_remove_comment (clause, g_mime_received_clause_get_comment (clause, 1)));
	g_assert_cmpint (g_mime_received_clause_index_of_comment (clause, "same"), ==, 3);
	g_assert_false (g_mime_received_clause_remove_comment (clause, "missing"));
	g_mime_received_clause_remove_comment_at (clause, 1);
	g_assert_cmpstr (g_mime_received_clause_get_comment (clause, 1), ==, "SAME");
	g_mime_received_clause_set_comment (clause, 0, g_mime_received_clause_get_value (clause));
	g_assert_true (g_mime_received_clause_set_value (clause, g_mime_received_clause_get_comment (clause, 0), &error));
	g_assert_no_error (error);
	g_mime_received_clause_clear_comments (clause);
	g_assert_cmpint (g_mime_received_clause_get_comment_count (clause), ==, 0);
	g_assert_cmpstr (g_mime_received_clause_get_keyword (clause), ==, "with");
	g_assert_cmpstr (g_mime_received_clause_get_value (clause), ==, "ESMTP");
	g_object_unref (clause);
}

static void
test_preconditions (void)
{
	GMimeReceivedClause *clause = g_mime_received_clause_new ("with", "ESMTP", NULL);
	GError *error = NULL;

	g_test_expect_message ("gmime", G_LOG_LEVEL_CRITICAL, "*keyword != NULL*");
	g_assert_null (g_mime_received_clause_new (NULL, "value", &error));
	g_test_assert_expected_messages ();
	g_test_expect_message ("gmime", G_LOG_LEVEL_CRITICAL, "*value != NULL*");
	g_assert_false (g_mime_received_clause_set_value (clause, NULL, &error));
	g_test_assert_expected_messages ();
	g_test_expect_message ("gmime", G_LOG_LEVEL_CRITICAL, "*comment != NULL*");
	g_assert_null (g_mime_received_clause_new_with_comment ("with", "ESMTP", NULL, &error));
	g_test_assert_expected_messages ();
	g_test_expect_message ("gmime", G_LOG_LEVEL_CRITICAL, "*index >= 0*");
	g_mime_received_clause_insert_comment (clause, -1, "comment");
	g_test_assert_expected_messages ();
	g_test_expect_message ("gmime", G_LOG_LEVEL_CRITICAL, "*index >= 0*");
	g_mime_received_clause_insert_comment (clause, 1, "comment");
	g_test_assert_expected_messages ();
	g_test_expect_message ("gmime", G_LOG_LEVEL_CRITICAL, "*index >= 0*");
	g_mime_received_clause_remove_comment_at (clause, 0);
	g_test_assert_expected_messages ();
	g_test_expect_message ("gmime", G_LOG_LEVEL_CRITICAL, "*index >= 0*");
	g_mime_received_clause_set_comment (clause, 0, "comment");
	g_test_assert_expected_messages ();
	g_test_expect_message ("gmime", G_LOG_LEVEL_CRITICAL, "*index >= 0*");
	g_assert_null (g_mime_received_clause_get_comment (clause, 0));
	g_test_assert_expected_messages ();
	g_test_expect_message ("gmime", G_LOG_LEVEL_CRITICAL, "*g_utf8_validate*");
	g_mime_received_clause_add_comment (clause, "\377");
	g_test_assert_expected_messages ();
	g_assert_no_error (error);
	g_assert_cmpint (g_mime_received_clause_get_comment_count (clause), ==, 0);
	g_assert_cmpstr (g_mime_received_clause_get_value (clause), ==, "ESMTP");
	g_object_unref (clause);
}

int
main (int argc, char **argv)
{
	g_mime_init ();
	testsuite_init (argc, argv);
	testsuite_start ("Received header clauses");
	testsuite_check ("Clause keywords and owned constructor strings");
	test_keywords ();
	testsuite_check_passed ();
	testsuite_check ("Value validation and atomic assignment");
	test_values ();
	testsuite_check_passed ();
	testsuite_check ("Ordered comment mutations and string aliases");
	test_comments ();
	testsuite_check_passed ();
	testsuite_check ("Programming-error guards");
	test_preconditions ();
	testsuite_check_passed ();
	testsuite_end ();
	g_mime_shutdown ();

	return testsuite_exit ();
}
