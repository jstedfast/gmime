/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* GMime
 * Copyright (C) 2026 Jeffrey Stedfast
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * as published by the Free Software Foundation; either version 2.1
 * of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * Lesser General Public License for more details.
 */

#include <stdarg.h>
#include <string.h>

#include <glib/gstdio.h>
#include <gmime/gmime.h>

#define HEADERS \
	"Date: Sat, 27 Jun 2026 12:00:00 +0000\r\n" \
	"From: Sender <sender@example.com>\r\n" \
	"Message-Id: <test@example.com>\r\n" \
	"MIME-Version: 1.0\r\n"

typedef struct {
	char *output;
	char *errors;
} Result;

static const char *analyzer;
static char *directory;
static GPtrArray *files;
static char *clean_file, *lf_file, *repeated_file;

static char *
write_file (const char *name, const char *content, gssize length)
{
	GError *error = NULL;
	char *path = g_build_filename (directory, name, NULL);

	g_assert_true (g_file_set_contents (path, content, length, &error));
	g_assert_no_error (error);
	g_ptr_array_add (files, path);
	return path;
}

static Result
run_analyzer (int expected_status, ...)
{
	GPtrArray *args = g_ptr_array_new_with_free_func (g_free);
	GError *error = NULL;
	Result result = { 0 };
	const char *arg;
	gboolean success;
	int status;
	va_list ap;

	g_ptr_array_add (args, g_strdup (analyzer));
	va_start (ap, expected_status);
	while ((arg = va_arg (ap, const char *)))
		g_ptr_array_add (args, g_strdup (arg));
	va_end (ap);
	g_ptr_array_add (args, NULL);
	g_assert_true (g_spawn_sync (NULL, (char **) args->pdata, NULL, 0, NULL, NULL,
				    &result.output, &result.errors, &status, &error));
	g_assert_no_error (error);
	success = g_spawn_check_wait_status (status, &error);
	if ((success && expected_status != 0) || (!success && expected_status == 0))
		g_test_message ("stdout:\n%s\nstderr:\n%s", result.output, result.errors);
	if (expected_status == 0) {
		g_assert_true (success);
		g_assert_no_error (error);
	} else {
		g_assert_false (success);
		g_assert_error (error, G_SPAWN_EXIT_ERROR, expected_status);
		g_clear_error (&error);
	}
	g_ptr_array_unref (args);
	return result;
}

static void
free_result (Result *result)
{
	g_free (result->output);
	g_free (result->errors);
}

static void
assert_contains (const char *text, const char *expected)
{
	if (!strstr (text, expected))
		g_error ("Expected '%s' in:\n%s", expected, text);
}

static void
test_options (void)
{
	const char *bad_options[][2] = {
		{ "--unknown", NULL }, { "--context", "invalid" }, { "--severity", "invalid" },
		{ "--category", "invalid" }, { "--category", "" }, { "--category", "," },
		{ "--max-issues", "0" }, { "--max-issues", "-1" }, { "--max-issues", "NaN" },
		{ "--max-issues", "2147483648" }, { "--max-per-violation", "-1" },
		{ "--max-per-violation", "2147483648" }
	};
	Result result;
	guint i;

	result = run_analyzer (0, "--help", NULL);
	assert_contains (result.output, "--max-per-violation");
	assert_contains (result.output, "Exit codes:");
	g_assert_cmpstr (result.errors, ==, "");
	free_result (&result);
	result = run_analyzer (0, "-h", NULL);
	assert_contains (result.output, "--context");
	free_result (&result);
	result = run_analyzer (2, NULL);
	assert_contains (result.errors, "No input files");
	free_result (&result);
	result = run_analyzer (2, "--context", NULL);
	assert_contains (result.errors, "error:");
	free_result (&result);
	for (i = 0; i < G_N_ELEMENTS (bad_options); i++) {
		if (bad_options[i][1])
			result = run_analyzer (2, bad_options[i][0], bad_options[i][1], clean_file, NULL);
		else
			result = run_analyzer (2, bad_options[i][0], clean_file, NULL);
		assert_contains (result.errors, "error:");
		free_result (&result);
	}
	result = run_analyzer (0, "--context=STORAGE", "--severity=Minor",
			       "--category=interop, DataLoss,Security", "--", clean_file, NULL);
	assert_contains (result.output, "no matching compliance issues");
	free_result (&result);
}

static void
test_clean_and_errors (void)
{
	char *missing = g_build_filename (directory, "nonexistent.eml", NULL);
	char *empty = write_file ("empty.eml", "", 0);
	Result result = run_analyzer (0, clean_file, NULL);

	assert_contains (result.output, "no compliance issues found");
	g_assert_cmpstr (result.errors, ==, "");
	free_result (&result);
	result = run_analyzer (1, missing, clean_file, NULL);
	assert_contains (result.errors, missing);
	assert_contains (result.output, "no compliance issues found");
	assert_contains (result.output, "2 files analyzed: 0 errors, 0 warnings, 0 notes");
	free_result (&result);
	result = run_analyzer (1, empty, NULL);
	assert_contains (result.errors, "error:");
	g_assert_null (strstr (result.output, "no compliance issues"));
	free_result (&result);
	result = run_analyzer (1, directory, NULL);
	assert_contains (result.errors, "error:");
	free_result (&result);
	g_free (missing);
}

static void
test_levels_and_filters (void)
{
	Result result = run_analyzer (0, "--no-color", lf_file, NULL);

	assert_contains (result.output, "warning:");
	assert_contains (result.output, "[BareLinefeedInHeader]");
	assert_contains (result.output, "[BareLinefeedInBody]");
	g_assert_null (strchr (result.output, '\033'));
	free_result (&result);
	result = run_analyzer (0, "-c", "storage", lf_file, NULL);
	assert_contains (result.output, "note:");
	g_assert_null (strstr (result.output, "warning:"));
	free_result (&result);
	result = run_analyzer (0, "-s", "critical", lf_file, NULL);
	assert_contains (result.output, "no matching compliance issues");
	g_assert_null (strstr (result.output, "[BareLinefeed"));
	free_result (&result);
	result = run_analyzer (0, "--category", "dataloss", lf_file, NULL);
	assert_contains (result.output, "no matching compliance issues");
	free_result (&result);
	result = run_analyzer (0, "--category", "security", lf_file, NULL);
	assert_contains (result.output, "[BareLinefeedInHeader]");
	free_result (&result);
	result = run_analyzer (1, "-W", "-q", lf_file, clean_file, NULL);
	assert_contains (result.output, "9 errors");
	assert_contains (result.output, "no compliance issues found");
	assert_contains (result.output, "2 files analyzed: 9 errors, 0 warnings, 0 notes");
	g_assert_null (strstr (result.output, "[BareLinefeed"));
	free_result (&result);
}

static void
test_limits (void)
{
	Result result;

	result = run_analyzer (0, "--max-issues", "1", repeated_file, NULL);
	assert_contains (result.output, "1 warning\n");
	assert_contains (result.output, "issue limit reached");
	g_assert_null (strstr (result.output, "TooManyComplianceIssues"));
	free_result (&result);
	result = run_analyzer (0, "--max-issues", "2", repeated_file, NULL);
	assert_contains (result.output, "2 warnings\n");
	g_assert_null (strstr (result.output, "issue limit reached"));
	free_result (&result);
	result = run_analyzer (1, "--max-per-violation", "1", "-W", repeated_file, NULL);
	assert_contains (result.output, "1 error\n");
	assert_contains (result.output, "issue limit reached");
	g_assert_null (strstr (result.output, "TooManyComplianceIssues"));
	free_result (&result);
	result = run_analyzer (0, "--max-per-violation", "1", "--category", "dataloss",
			       "-W", repeated_file, NULL);
	assert_contains (result.output, "no matching compliance issues");
	assert_contains (result.output, "issue limit reached");
	g_assert_null (strstr (result.output, "error:"));
	free_result (&result);
	result = run_analyzer (0, "--max-issues", "1", repeated_file, repeated_file, NULL);
	assert_contains (result.output, "2 files analyzed: 0 errors, 2 warnings, 0 notes");
	free_result (&result);
	result = run_analyzer (0, "--max-per-violation", "0", repeated_file, NULL);
	assert_contains (result.output, "2 warnings\n");
	g_assert_null (strstr (result.output, "issue limit reached"));
	free_result (&result);
}

static void
test_output_options (void)
{
	Result result = run_analyzer (0, "-r", "--no-caret", repeated_file, NULL);

	assert_contains (result.output, "= categories: Interoperability, Security");
	assert_contains (result.output, "The Internet Message Format specification");
	g_assert_null (strstr (result.output, "^"));
	g_assert_null (strstr (result.output, "| Subject:"));
	free_result (&result);
	result = run_analyzer (0, "-q", "-r", repeated_file, NULL);
	assert_contains (result.output, "2 warnings\n");
	g_assert_null (strstr (result.output, "warning:"));
	g_assert_null (strstr (result.output, "categories:"));
	g_assert_null (strstr (result.output, " | "));
	free_result (&result);
	g_setenv ("NO_COLOR", "", TRUE);
	result = run_analyzer (0, repeated_file, NULL);
	g_assert_null (strchr (result.output, '\033'));
	free_result (&result);
	g_unsetenv ("NO_COLOR");
}

static void
test_byte_positions (void)
{
	const char message[] = HEADERS "Subject: bytes\r\nContent-Type: text/plain\r\n\r\n\tA\001B\000C\377\r\n";
	char *path = write_file ("bytes.eml", message, sizeof (message) - 1);
	Result result = run_analyzer (1, path, NULL);

	assert_contains (result.output, ":8:5: error:");
	assert_contains (result.output, "[UnexpectedNullBytesInBody]");
	assert_contains (result.output, ":8:7: note:");
	assert_contains (result.output, "[Unexpected8BitBytesInBody]");
	assert_contains (result.output, "8 |         A\\x01B\\x00C\\xFF\n");
	assert_contains (result.output, "  |               ^\n");
	assert_contains (result.output, "  |                    ^\n");
	g_assert_null (strchr (result.output, '\t'));
	g_assert_null (strchr (result.output, '\377'));
	free_result (&result);
}

static void
test_folded_headers (void)
{
	char *path = write_file ("folded.eml", HEADERS "Subject: ascii\r\n\tcaf\351\r\n\r\nbody\r\n", -1);
	Result result = run_analyzer (0, path, NULL);

	assert_contains (result.output, ":6:5: note:");
	assert_contains (result.output, "6 |         caf\\xE9\n");
	assert_contains (result.output, "  |            ^\n");
	free_result (&result);
	path = write_file ("mixed-utf8.eml", HEADERS "Subject: ascii\r\n\tcaf\303\251 \377\r\n\r\nbody\r\n", -1);
	result = run_analyzer (0, path, NULL);
	assert_contains (result.output, ":6:5: note:");
	assert_contains (result.output, "6 |         caf\\xC3\\xA9 \\xFF\n");
	free_result (&result);
	path = write_file ("blank-continuation.eml", HEADERS "Subject: one\r\n \t \r\n\r\nbody\r\n", -1);
	result = run_analyzer (0, path, NULL);
	assert_contains (result.output, ":6:4: warning:");
	assert_contains (result.output, "[BlankHeaderContinuationLine]");
	assert_contains (result.output, "6 |          \n  |          ^\n");
	free_result (&result);
	path = write_file ("blank-eof.eml", HEADERS "Subject: one\r\n \t ", -1);
	result = run_analyzer (0, path, NULL);
	assert_contains (result.output, ":6:4: warning:");
	assert_contains (result.output, "[BlankHeaderContinuationLine]");
	free_result (&result);
	path = write_file ("bare-cr.eml", HEADERS "Subject: a\rb\r\n\r\nbody\r\n", -1);
	result = run_analyzer (1, path, NULL);
	assert_contains (result.output, ":5:11: error:");
	assert_contains (result.output, "5 | Subject: a\\x0Db\n");
	free_result (&result);
}

static void
test_long_lines (void)
{
	GString *message = g_string_new (HEADERS "Subject: long\r\nContent-Type: text/plain\r\n\r\n");
	Result result;
	char *path, **lines;
	guint i, nsource = 0;

	for (i = 0; i < 1500; i++)
		g_string_append_c (message, 'A');
	g_string_append (message, "\377\r\n");
	path = write_file ("long.eml", message->str, message->len);
	g_string_free (message, TRUE);
	result = run_analyzer (0, path, NULL);
	assert_contains (result.output, ":8:1501: note:");
	assert_contains (result.output, "[OversizedLine]");
	assert_contains (result.output, "...AAAA");
	assert_contains (result.output, "\\xFF\n");
	lines = g_strsplit (result.output, "\n", -1);
	for (i = 0; lines[i]; i++) {
		const char *gutter = strstr (lines[i], " | ");

		if (!gutter)
			continue;
		if (!strchr (gutter, '^')) {
			g_assert_cmpuint (strlen (gutter + 3), <=, 100);
			nsource++;
		} else {
			g_assert_cmpuint (strlen (gutter + 3), <=, 101);
		}
	}
	g_assert_cmpuint (nsource, ==, 2);
	g_strfreev (lines);
	free_result (&result);
}

static void
test_metadata (void)
{
	GMimeComplianceViolation violation;
	guint i;

	for (i = 1; i <= 91; i++) {
		GMimeComplianceIssue issue = { GMIME_COMPLIANCE_CONTEXT_TRANSPORT, 0, 0, 1, 1,
					      GMIME_COMPLIANCE_POSITION_EXACT };
		const char *name;
		char *text;

		violation = i == 91 ? GMIME_COMPLIANCE_TOO_MANY_COMPLIANCE_ISSUES : i;
		name = g_mime_compliance_violation_get_name (violation);
		g_assert_nonnull (name);
		g_assert_cmpstr (name, !=, "");
		g_assert_null (strpbrk (name, " \t\r\n"));
		issue.violation = violation;
		text = g_mime_compliance_issue_to_string (&issue);
		g_assert_true (g_str_has_prefix (text, name));
		g_free (text);
	}
	g_assert_cmpstr (g_mime_compliance_violation_get_name (GMIME_COMPLIANCE_UNEXPECTED_8BIT_BYTES_IN_HEADER),
			 ==, "Unexpected8BitBytesInHeader");
}

int
main (int argc, char **argv)
{
	GString *lf_message;
	GError *error = NULL;
	const char *p;
	guint i;
	int result;

	g_test_init (&argc, &argv, NULL);
	g_assert_cmpint (argc, ==, 2);
	analyzer = argv[1];
	directory = g_dir_make_tmp ("gmime-analyzer-XXXXXX", &error);
	g_assert_no_error (error);
	files = g_ptr_array_new_with_free_func (g_free);
	clean_file = write_file ("clean.eml",
				 HEADERS "Subject: Clean\r\nContent-Type: text/plain\r\n\r\nHello.\r\n", -1);
	lf_message = g_string_new (NULL);
	for (p = HEADERS "Subject: LF\r\nContent-Type: text/plain\r\n\r\none\r\ntwo\r\n"; *p; p++) {
		if (*p != '\r')
			g_string_append_c (lf_message, *p);
	}
	lf_file = write_file ("lf.eml", lf_message->str, lf_message->len);
	g_string_free (lf_message, TRUE);
	repeated_file = write_file ("repeated.eml",
		HEADERS "Subject: one\r\nSubject: two\r\nSubject: three\r\n\r\nbody\r\n", -1);

	g_mime_init ();
	g_test_add_func ("/mime-analyzer/options", test_options);
	g_test_add_func ("/mime-analyzer/clean-and-errors", test_clean_and_errors);
	g_test_add_func ("/mime-analyzer/levels-and-filters", test_levels_and_filters);
	g_test_add_func ("/mime-analyzer/limits", test_limits);
	g_test_add_func ("/mime-analyzer/output-options", test_output_options);
	g_test_add_func ("/mime-analyzer/byte-positions", test_byte_positions);
	g_test_add_func ("/mime-analyzer/folded-headers", test_folded_headers);
	g_test_add_func ("/mime-analyzer/long-lines", test_long_lines);
	g_test_add_func ("/mime-analyzer/metadata", test_metadata);
	result = g_test_run ();
	g_mime_shutdown ();
	for (i = 0; i < files->len; i++)
		g_assert_cmpint (g_unlink (g_ptr_array_index (files, i)), ==, 0);
	g_ptr_array_unref (files);
	g_assert_cmpint (g_rmdir (directory), ==, 0);
	g_free (directory);
	return result;
}
