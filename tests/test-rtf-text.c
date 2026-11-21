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
	g_test_init (&argc, &argv, NULL);
	g_mime_init ();
	g_test_add_func ("/rtf/text/codepage-and-placeholders", test_render);
	g_test_add_func ("/rtf/text/diagnostic-budget", test_budget);
	int result = g_test_run ();
	g_mime_shutdown ();
	return result;
}
