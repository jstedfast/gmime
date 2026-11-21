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

#include <gmime/gmime.h>
#include <string.h>

static const char *data_dir = "tests/data/rtf-text";

static void
check_snapshot (GMimeTextConverter *converter, const char *rtf, gsize length, const char *path)
{
	GMimeStream *input = g_mime_stream_mem_new_with_buffer (rtf, length);
	GMimeStream *output = g_mime_stream_mem_new ();
	GByteArray *bytes = g_mime_stream_mem_get_byte_array (GMIME_STREAM_MEM (output));
	GString *expected, *actual;
	GError *error = NULL;
	char *contents;
	gsize size;

	g_test_message ("%s", path);
	g_file_get_contents (path, &contents, &size, &error);
	g_assert_no_error (error);
	g_assert_true (g_mime_text_converter_convert (converter, input, output, &error));
	g_assert_no_error (error);
	expected = g_string_new_len (contents, size);
	actual = g_string_new_len ((const char *) bytes->data, bytes->len);
	g_string_replace (expected, "\r\n", "\n", 0);
	g_string_replace (actual, "\r\n", "\n", 0);
	g_assert_cmpmem (actual->str, actual->len, expected->str, expected->len);
	g_string_free (actual, TRUE);
	g_string_free (expected, TRUE);
	g_free (contents);
	g_object_unref (output);
	g_object_unref (input);
}

static void
test_corpus (gconstpointer data)
{
	char *base = g_build_filename (data_dir, (const char *) data, NULL);
	char *rtf_path = g_strconcat (base, ".rtf", NULL);
	char *text_path = g_strconcat (base, ".txt", NULL);
	char *html_path = g_strconcat (base, ".html", NULL);
	GMimeTextConverter *text = g_mime_rtf_to_text_new ();
	GMimeTextConverter *html = g_mime_rtf_to_html_new ();
	GError *error = NULL;
	char *rtf;
	gsize length;

	g_file_get_contents (rtf_path, &rtf, &length, &error);
	g_assert_no_error (error);
	check_snapshot (text, rtf, length, text_path);
	check_snapshot (html, rtf, length, html_path);
	g_free (rtf);
	g_free (rtf_path);
	g_free (text_path);
	g_free (html_path);
	g_free (base);
	g_object_unref (html);
	g_object_unref (text);
}

static gboolean
placeholder (guint index, GMimeStream *out, gpointer data, GError **error)
{
	g_assert_cmpuint (index, ==, 0);
	g_assert_cmpint (g_mime_stream_write (out, "#", 1), ==, 1);
	return TRUE;
}

static void
test_render (void)
{
	const char input[] = "{\\rtf1\\ansi\\ansicpg1252 \x93Hi\x94\\par "
			     "{\\*\\unknown skip}{\\v hidden\\objattph}\\uc1\\u945?\\cell "
			     "\\objattph\\cell\\row}";
	GMimeTextConverter *c = g_mime_rtf_to_text_new ();
	GMimeStream *source = g_mime_stream_mem_new_with_buffer (input, sizeof input - 1);
	GMimeStream *dest = g_mime_stream_mem_new ();
	GByteArray *b = g_mime_stream_mem_get_byte_array (GMIME_STREAM_MEM (dest));
	GError *error = NULL;
	const char expected[] = "[\xe2\x80\x9cHi\xe2\x80\x9d\n\xce\xb1\t#\n]";

	g_mime_text_converter_set_header (c, "[");
	g_mime_text_converter_set_footer (c, "]");
	g_mime_rtf_to_text_set_attachment_placeholder_callback (GMIME_RTF_TO_TEXT (c), placeholder, NULL, NULL);
	g_assert_true (g_mime_text_converter_convert (c, source, dest, &error));
	g_assert_no_error (error);
	g_assert_cmpmem (b->data, b->len, expected, sizeof expected - 1);
	g_object_unref (dest);
	g_object_unref (source);
	g_object_unref (c);
}

static void
log_issue (const GMimeRtfComplianceIssue *issue, gpointer data)
{
	GArray *log = data;
	g_array_append_val (log, *issue);
}

static void
test_logger_parity (gconstpointer data)
{
	static const char input[] = "{\\rtf1 \\'zz\\'zz\\'zz\\'zz\\'zz\\f9 x}";
	const gboolean html = GPOINTER_TO_INT (data);
	GMimeTextConverter *converter = html ? g_mime_rtf_to_html_new () : g_mime_rtf_to_text_new ();
	GMimeRtfComplianceOptions *options;
	GArray *log = g_array_new (FALSE, FALSE, sizeof (GMimeRtfComplianceIssue));
	GError *error = NULL;
	char *expected = g_mime_text_converter_convert_to_string (converter, input, -1, &error);

	g_assert_no_error (error);
	if (html) {
		options = g_mime_rtf_to_html_get_compliance_options (GMIME_RTF_TO_HTML (converter));
		g_mime_rtf_to_html_set_compliance_logger (GMIME_RTF_TO_HTML (converter), log_issue, log, NULL);
	} else {
		options = g_mime_rtf_to_text_get_compliance_options (GMIME_RTF_TO_TEXT (converter));
		g_mime_rtf_to_text_set_compliance_logger (GMIME_RTF_TO_TEXT (converter), log_issue, log, NULL);
	}

	for (int document = 0; document < 3; document++) {
		gboolean capped = document < 2;
		char *actual;

		g_mime_rtf_compliance_options_set_max_issues_per_violation (options, capped ? 2 : 0);
		actual = g_mime_text_converter_convert_to_string (converter, input, -1, &error);
		g_assert_no_error (error);
		g_assert_cmpstr (actual, ==, expected);
		g_assert_cmpuint (log->len, ==, capped ? 4 : 6);
		for (guint i = 0; i < log->len; i++) {
			GMimeRtfComplianceViolation violation = GMIME_RTF_COMPLIANCE_VIOLATION_INVALID_HEX_ESCAPE;
			if (i == log->len - 1)
				violation = GMIME_RTF_COMPLIANCE_VIOLATION_UNDEFINED_FONT;
			else if (capped && i == 2)
				violation = GMIME_RTF_COMPLIANCE_VIOLATION_TOO_MANY_COMPLIANCE_ISSUES;
			g_assert_cmpint (g_array_index (log, GMimeRtfComplianceIssue, i).violation, ==, violation);
		}
		g_free (actual);
		g_array_set_size (log, 0);
	}
	g_free (expected);
	g_array_unref (log);
	g_object_unref (converter);
}

static void
test_budget (void)
{
	GMimeTextConverter *c = g_mime_rtf_to_text_new ();
	GMimeRtfToText *converter = GMIME_RTF_TO_TEXT (c);
	GArray *log = g_array_new (FALSE, FALSE, sizeof (GMimeRtfComplianceIssue));
	GError *error = NULL;

	g_mime_rtf_compliance_options_set_max_issues_per_violation (g_mime_rtf_to_text_get_compliance_options (converter), 1);
	g_mime_rtf_to_text_set_compliance_logger (converter, log_issue, log, NULL);
	for (int i = 0; i < 2; i++) {
		char *text = g_mime_text_converter_convert_to_string (c, "{\\rtf1\n\\'x\\'y\\'z}", -1, &error);
		g_assert_no_error (error);
		g_assert_cmpstr (text, ==, "xyz");
		g_assert_cmpuint (log->len, ==, 2);
		GMimeRtfComplianceIssue *issues = (GMimeRtfComplianceIssue *) log->data;
		g_assert_cmpint (issues[0].violation, ==, GMIME_RTF_COMPLIANCE_VIOLATION_INVALID_HEX_ESCAPE);
		g_assert_cmpint (issues[0].line_number, ==, 2);
		g_assert_cmpint (issues[0].depth, ==, 1);
		g_assert_cmpint (issues[1].violation, ==, GMIME_RTF_COMPLIANCE_VIOLATION_TOO_MANY_COMPLIANCE_ISSUES);
		g_assert_cmpint (issues[1].line_position, ==, 4);
		g_free (text);
		g_array_set_size (log, 0);
	}
	g_object_unref (c);
	g_array_unref (log);
}

int
main (int argc, char **argv)
{
	static const char *corpus[] = {
		"apache-tika/testRTFHyperlinkAndStyles", "apache-tika/testRTFInvalidUnicode",
		"apache-tika/testRTFTIKA_1713", "apache-tika/testRTFUnicodeUCNControlWordCharacterDoubling",
		"apache-tika/testRTFWindowsCodepage1250",
		"mimekit/charsets-and-sections", "mimekit/nested-tables", "mimekit/skipped-destinations",
		"rtf-parse/extractedNonshppict", "rtf.js/simplepngbin", "rtf2html/Basic_Test",
		"rtfparserkit/test10001Encoding", "rtfparserkit/test10007Encoding", "rtfparserkit/test437Encoding",
		"rtfparserkit/test874Encoding", "rtfparserkit/test950Encoding", "rtfparserkit/testGitHubIssue6",
		"rtfparserkit/testJapaneseUtf8Encoding", "rtfparserkit/testKoreanEncoding", "rtfparserkit/testMultiByteHex",
		"rtfparserkit/testNegativeUnicode", "rtfparserkit/testSpecialChars", "rtfparserkit/testTurkishEncoding",
		"RtfPipe/Issue35", "RtfPipe/RtfParserTest_fail_3", "RtfPipe/RtfParserTest_fail_4",
		"RtfPipe/tabs", "RtfPipe/Test01", "striprtf/issue_20", "striprtf/issue_28",
		"striprtf/specialchars", "striprtf/TX_RTF32_18.0.541.501", "striprtf/unicode"
	};

	g_test_init (&argc, &argv, NULL);
	for (int i = 1; i < argc; i++) {
		if (g_file_test (argv[i], G_FILE_TEST_IS_DIR))
			data_dir = argv[i];
	}
	g_mime_init ();
	for (guint i = 0; i < G_N_ELEMENTS (corpus); i++) {
		char *path = g_strconcat ("/rtf/corpus/", corpus[i], NULL);
		g_test_add_data_func (path, corpus[i], test_corpus);
		g_free (path);
	}
	g_test_add_func ("/rtf/text/codepage-and-placeholders", test_render);
	g_test_add_func ("/rtf/text/diagnostic-budget", test_budget);
	g_test_add_data_func ("/rtf/text/logger-parity", GINT_TO_POINTER (FALSE), test_logger_parity);
	g_test_add_data_func ("/rtf/html/logger-parity", GINT_TO_POINTER (TRUE), test_logger_parity);
	int result = g_test_run ();
	g_mime_shutdown ();
	return result;
}
