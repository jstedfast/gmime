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

#include <string.h>

#include <gmime/gmime.h>
#include <gmime/gmime-rtf-to-html.h>

static void
check (GMimeTextConverter *converter, const char *input, const char *expected)
{
	GError *error = NULL;
	char *html = g_mime_text_converter_convert_to_string (converter, input, -1, &error);

	g_assert_no_error (error);
	g_assert_cmpstr (html, ==, expected);
	g_free (html);
}

static void
test_render (void)
{
	GMimeTextConverter *base = g_mime_rtf_to_html_new ();
	GMimeRtfToHtml *rtf = GMIME_RTF_TO_HTML (base);

	check (base, "", "<html><body></body></html>");
	g_mime_text_converter_set_header (base, "<header>");
	g_mime_text_converter_set_footer (base, "<p>footer</p>");
	g_mime_rtf_to_html_set_footer_format (rtf, GMIME_HEADER_FOOTER_FORMAT_HTML);
	check (base, "{\\rtf1 x}", "<html><body>&lt;header&gt;<br/><div>x</div>\n<p>footer</p></body></html>");

	g_mime_text_converter_set_header (base, NULL);
	g_mime_text_converter_set_footer (base, NULL);
	g_mime_rtf_to_html_set_output_html_fragment (rtf, TRUE);
	check (base, "{\\rtf1{\\colortbl;\\red255\\green0\\blue0;\\red0\\green0\\blue255;}"
	       "\\qc a{\\b\\i\\ul\\strike\\super\\fs25\\cf1\\cb2 b}c\\par\\par}",
	       "<div style=\"text-align: center;\">a<span style=\"font-weight: bold; font-style: italic; "
	       "text-decoration: underline line-through; vertical-align: super; font-size: 12.5pt; "
	       "color: #FF0000; background-color: #0000FF;\">b</span>c</div>\n"
	       "<div style=\"text-align: center;\"><br/></div>\n");
	check (base, "{\\rtf1 a  b\\tab c<&>\\line  x}",
	       "<div>a &#160;b&#160;&#160;&#160;&#160;c&lt;&amp;&gt;<br/>&#160;x</div>\n");
	check (base, "{\\rtf1\\trowd\\cellx1000\\cellx2000\\pard\\intbl a\\cell\\cell\\row\\pard end}",
	       "<table><tr><td><div>a</div>\n</td><td></td></tr>\n</table>\n<div>end</div>\n");
	check (base, "{\\rtf1{\\field{\\*\\fldinst HYPERLINK \"https://example.test/?a&b\"}{\\fldrslt link}}"
	       "{\\field{\\*\\fldinst HYPERLINK \"file:///x\"}{\\fldrslt plain}}}",
	       "<div><a href=\"https://example.test/?a&amp;b\">link</a>plain</div>\n");
	/* Nested field results must close and reopen anchors, never nest them. */
	check (base, "{\\rtf1{\\field{\\*\\fldinst HYPERLINK \"http://a/\"}{\\fldrslt a"
	       "{\\field{\\*\\fldinst HYPERLINK \"http://b/\"}{\\fldrslt b}}c}}}",
	       "<div><a href=\"http://a/\">a</a><a href=\"http://b/\">b</a><a href=\"http://a/\">c</a></div>\n");
	check (base, "{\\rtf1 a\\row b}", "<div>a<table><tr></tr>\n</table>\nb</div>\n");
	check (base, "{\\rtf1\\row\\cell\\row}", "<table><tr></tr>\n<tr><td></td></tr>\n</table>\n");
	g_object_unref (base);
}

static void
test_wire_bytes (void)
{
	static const char input[] = "{\\rtf1\\ansi\\ansicpg1252 a\\bin3 {\0}\x80\\'80}";
	static const char expected[] = "<div>a&#8364;&#8364;</div>\n";
	GMimeTextConverter *base = g_mime_rtf_to_html_new ();
	GMimeStream *src = g_mime_stream_mem_new_with_buffer (input, sizeof input - 1);
	GMimeStream *dest = g_mime_stream_mem_new ();
	GByteArray *bytes = g_mime_stream_mem_get_byte_array (GMIME_STREAM_MEM (dest));
	GError *error = NULL;

	g_mime_rtf_to_html_set_output_html_fragment (GMIME_RTF_TO_HTML (base), TRUE);
	g_assert_true (g_mime_text_converter_convert (base, src, dest, &error));
	g_assert_no_error (error);
	g_assert_cmpmem (bytes->data, bytes->len, expected, sizeof expected - 1);
	g_object_unref (src);
	g_object_unref (dest);
	g_object_unref (base);
}

static gboolean
filter (GMimeHtmlTagContext *context, GMimeHtmlWriter *writer, gpointer user_data, GError **error)
{
	switch (g_mime_html_tag_context_get_tag_id (context)) {
	case GMIME_HTML_TAG_SPAN:
	case GMIME_HTML_TAG_IMAGE:
		g_mime_html_tag_context_set_delete_tag (context, TRUE);
		g_mime_html_tag_context_set_delete_end_tag (context, TRUE);
		g_mime_html_tag_context_set_suppress_inner_content (context, TRUE);
		return TRUE;
	case GMIME_HTML_TAG_DIV:
		if (g_mime_html_tag_context_get_is_end_tag (context))
			return g_mime_html_writer_write_end_tag (writer, "p", error);
		g_mime_html_tag_context_set_invoke_callback_for_end_tag (context, TRUE);
		return g_mime_html_writer_write_start_tag (writer, "p", error);
	default:
		return g_mime_html_tag_context_write_tag (context, writer, TRUE, error);
	}
}

static gboolean
attachment (int index, GMimeHtmlWriter *writer, gpointer user_data, GError **error)
{
	int *seen = user_data;

	(*seen)++;
	g_assert_cmpint (index, ==, 1);
	return g_mime_html_writer_write_empty_element_tag (writer, "img", error);
}

static gboolean
fail_tag (GMimeHtmlTagContext *context, GMimeHtmlWriter *writer, gpointer user_data, GError **error)
{
	return FALSE;
}

static void
test_callbacks (void)
{
	GMimeTextConverter *base = g_mime_rtf_to_html_new ();
	GMimeRtfToHtml *rtf = GMIME_RTF_TO_HTML (base);
	GError *error = NULL;
	int seen = 0;

	g_mime_rtf_to_html_set_output_html_fragment (rtf, TRUE);
	g_mime_rtf_to_html_set_html_tag_callback (rtf, filter, NULL, NULL);
	g_mime_rtf_to_html_set_attachment_placeholder_callback (rtf, attachment, &seen, NULL);
	check (base, "{\\rtf1{\\v\\objattph}{\\pict\\objattph}a{\\b secret\\objattph}\\objattph b}", "<p>a<img/>b</p>\n");
	check (base, "{\\rtf1\\fromhtml1 {\\*\\htmltag <p>x</p>}\\objattph}", "<p>x</p>");
	g_assert_cmpint (seen, ==, 1);

	g_mime_rtf_to_html_set_html_tag_callback (rtf, fail_tag, NULL, NULL);
	g_assert_null (g_mime_text_converter_convert_to_string (base, "{\\rtf1 x}", -1, &error));
	g_assert_error (error, G_MARKUP_ERROR, G_MARKUP_ERROR_INVALID_CONTENT);
	g_clear_error (&error);
	g_mime_rtf_to_html_set_html_tag_callback (rtf, NULL, NULL, NULL);
	check (base, "{\\rtf1 x}", "<div>x</div>\n");
	g_object_unref (base);
}

static void
test_extract (void)
{
	static const char input[] = "{\\rtf1\\ansi\\fromhtml1 {\\*\\htmltag <html><head><title>T</title></head><body>"
		"<noscript><img src=\"x\"/></noscript><p>}a\\par b\\line c\\cell d\\row"
		"{\\*\\htmltag </p></body></html>}\\htmlrtf fallback\\htmlrtf0}";
	GMimeTextConverter *base = g_mime_rtf_to_html_new ();
	GMimeRtfToHtml *rtf = GMIME_RTF_TO_HTML (base);

	g_mime_rtf_to_html_set_output_html_fragment (rtf, TRUE);
	g_mime_rtf_to_html_set_html_tag_callback (rtf, filter, NULL, NULL);
	g_mime_text_converter_set_header (base, "&");
	check (base, input, "&amp;<br/><p>a\r\nb\r\ncd</p>");
	g_mime_rtf_to_html_set_no_script_handling (rtf, GMIME_HTML_NO_SCRIPT_HANDLING_SCRIPTING_DISABLED);
	check (base, input, "&amp;<br/><noscript></noscript><p>a\r\nb\r\ncd</p>");

	g_mime_text_converter_set_header (base, NULL);
	g_mime_rtf_to_html_set_html_tag_callback (rtf, NULL, NULL, NULL);
	g_mime_rtf_to_html_set_max_element_depth (rtf, 3);
	check (base, "{\\rtf1\\fromhtml1 {\\*\\htmltag <div><span><b><i>x<img src=x></i></b>}}",
	       "<div><span><b><i>x&lt;img src=x&gt;&lt;/i&gt;&lt;/b&gt;");
	g_mime_rtf_to_html_set_extract_encapsulated_html (rtf, FALSE);
	check (base, "{\\rtf1\\fromhtml1 {\\*\\htmltag <p>}&amp;\\htmlrtf fallback\\htmlrtf0{\\*\\htmltag </p>}}",
	       "<div>&amp;amp;fallback</div>\n");
	g_object_unref (base);
}

static void
log_issue (const GMimeRtfComplianceIssue *issue, gpointer user_data)
{
	guint *seen = user_data;

	if (issue->violation == GMIME_RTF_COMPLIANCE_VIOLATION_GROUP_DEPTH_LIMIT_EXCEEDED)
		(*seen)++;
}

static void
test_diagnostics (void)
{
	GMimeTextConverter *base = g_mime_rtf_to_html_new ();
	GMimeRtfToHtml *rtf = GMIME_RTF_TO_HTML (base);
	GString *input = g_string_new ("{\\rtf1 a");
	guint seen = 0;

	for (int i = 0; i < 32; i++)
		g_string_append (input, i & 1 ? "{\\b0 " : "{\\b ");
	g_string_append (input, "omitted");
	for (int i = 0; i < 32; i++)
		g_string_append_c (input, '}');
	g_string_append (input, "b}");
	g_mime_rtf_to_html_set_output_html_fragment (rtf, TRUE);
	g_mime_rtf_to_html_set_max_group_depth (rtf, 8);
	g_mime_rtf_to_html_set_compliance_logger (rtf, log_issue, &seen, NULL);
	g_mime_rtf_compliance_options_set_max_issues_per_violation (g_mime_rtf_to_html_get_compliance_options (rtf), 1);
	check (base, input->str, "<div>ab</div>\n");
	check (base, input->str, "<div>ab</div>\n");
	g_assert_cmpuint (seen, ==, 2);
	g_string_free (input, TRUE);
	g_object_unref (base);
}

static void
test_extract_stops_reading (void)
{
	GMimeTextConverter *base = g_mime_rtf_to_html_new ();
	GString *document = g_string_new ("{\\rtf1\\fromhtml1 {\\*\\htmltag <p>}");
	GMimeStream *output = g_mime_stream_mem_new ();
	GMimeStream *input;
	GError *error = NULL;

	/* A parser that buffers all HTML before invoking the first tag callback
	 * would consume the entire stream even though the callback rejects it. */
	for (int i = 0; i < 8192; i++)
		g_string_append (document, "some body text ");
	g_string_append (document, "{\\*\\htmltag </p>}}");
	input = g_mime_stream_mem_new_with_buffer (document->str, document->len);
	g_mime_rtf_to_html_set_html_tag_callback (GMIME_RTF_TO_HTML (base), fail_tag, NULL, NULL);
	g_assert_false (g_mime_text_converter_convert (base, input, output, &error));
	g_assert_error (error, G_MARKUP_ERROR, G_MARKUP_ERROR_INVALID_CONTENT);
	g_assert_cmpint (g_mime_stream_tell (input), <, (gint64) document->len);
	g_clear_error (&error);
	g_object_unref (input);
	g_object_unref (output);
	g_string_free (document, TRUE);
	g_object_unref (base);
}

int
main (int argc, char **argv)
{
	int result;

	g_test_init (&argc, &argv, NULL);
	g_mime_init ();
	g_test_add_func ("/rtf-html/render", test_render);
	g_test_add_func ("/rtf-html/wire-bytes", test_wire_bytes);
	g_test_add_func ("/rtf-html/callbacks", test_callbacks);
	g_test_add_func ("/rtf-html/extract", test_extract);
	g_test_add_func ("/rtf-html/extract-stops-reading", test_extract_stops_reading);
	g_test_add_func ("/rtf-html/diagnostics", test_diagnostics);
	result = g_test_run ();
	g_mime_shutdown ();
	return result;
}
