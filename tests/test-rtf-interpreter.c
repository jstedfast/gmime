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
#include <gmime/gmime-rtf-interpreter-private.h>

typedef struct {
	GString *text, *html, *links;
	gboolean from_html, check_format;
	guint begun;
	guint issues[GMIME_RTF_COMPLIANCE_VIOLATION_GROUP_DEPTH_LIMIT_EXCEEDED + 1];
} Capture;

static void
issue (const GMimeRtfComplianceIssue *issue, gpointer data)
{
	Capture *c = data;
	g_assert_cmpuint (issue->violation, <, G_N_ELEMENTS (c->issues));
	c->issues[issue->violation]++;
}

static gboolean
capture (GMimeRtfEvent event, const GMimeRtfFormat *f, const char *s, gsize n, gpointer data, GError **error)
{
	Capture *c = data;
	gsize k;

	(void) error;
	if (event == GMIME_RTF_EVENT_BEGIN) {
		c->begun++;
		c->from_html = f->from_html;
		return TRUE;
	}
	g_assert_cmpuint (c->begun, ==, 1);
	g_assert_cmpint (c->from_html, ==, f->from_html);
	switch (event) {
	case GMIME_RTF_EVENT_TEXT:
		g_string_append_len (c->text, s, n);
		if (f->hyperlink)
			for (k = 0; k < n; k++)
				g_string_append_printf (c->links, "%c=%s;", s[k], f->hyperlink);
		if (c->check_format) {
			for (k = 0; k < n; k++) {
				gboolean styled = s[k] == 'A' || s[k] == 'C';
				g_assert_cmpint (f->bold, ==, styled);
				g_assert_cmpint (f->italic, ==, styled);
				g_assert_cmpint (f->underline, ==, styled);
				g_assert_cmpint (f->strike, ==, styled);
				g_assert_cmpint (f->vertical_alignment, ==, styled ? GMIME_RTF_SUPERSCRIPT : GMIME_RTF_BASELINE);
				g_assert_cmpint (f->font_size, ==, styled ? 43 : 24);
				g_assert_cmpint (f->foreground, ==, styled ? 0xff0022 : -1);
				g_assert_cmpint (f->background, ==, styled ? 0xff0022 : -1);
				g_assert_cmpint (f->in_table, ==, s[k] != 'D');
				g_assert_cmpint (f->alignment, ==, s[k] == 'D' ? GMIME_RTF_ALIGN_LEFT : GMIME_RTF_ALIGN_CENTER);
			}
		}
		break;
	case GMIME_RTF_EVENT_HTML_TAG: g_string_append_len (c->html, s, n); break;
	case GMIME_RTF_EVENT_PARAGRAPH: g_string_append (c->text, "[par]"); break;
	case GMIME_RTF_EVENT_LINE_BREAK: g_string_append (c->text, "[br]"); break;
	case GMIME_RTF_EVENT_CELL: g_string_append (c->text, "[cell]"); break;
	case GMIME_RTF_EVENT_ROW: g_string_append (c->text, "[row]"); break;
	case GMIME_RTF_EVENT_OBJECT_PLACEHOLDER: g_string_append (c->text, "[obj]"); break;
	default: g_assert_not_reached ();
	}
	return TRUE;
}

static void
read_rtf (Capture *c, const char *rtf, gboolean extract, guint depth, guint fonts, guint colors)
{
	GMimeStream *in = g_mime_stream_mem_new_with_buffer (rtf, strlen (rtf));
	GMimeRtfDiagnostics diag;
	GError *error = NULL;

	c->text = g_string_new (NULL);
	c->html = g_string_new (NULL);
	c->links = g_string_new (NULL);
	_g_mime_rtf_diagnostics_init (&diag, NULL, issue, c);
	g_assert_true (_g_mime_rtf_interpret (in, extract, depth, fonts, colors, &diag, capture, c, &error));
	g_assert_no_error (error);
	g_assert_cmpuint (c->begun, ==, 1);
	g_object_unref (in);
}

static void
clear_capture (Capture *c)
{
	g_string_free (c->text, TRUE);
	g_string_free (c->html, TRUE);
	g_string_free (c->links, TRUE);
	memset (c, 0, sizeof *c);
}

static void
test_unicode (void)
{
	Capture c = { 0 };

	read_rtf (&c, "{\\rtf1\\uc2\\u945\\'3f\\~{\\uc0\\u946}X"
		  "\\u-10179??\\u-8704??\\u-10179??{Z}\\u-8704??"
		  "\\u65\xe2\x98\x83\\par\\u66?{Q}\\uc0\xe2\x98\x83}", FALSE, 8, 8, 8);
	g_assert_cmpstr (c.text->str, ==, "\xce\xb1\xce\xb2X\xf0\x9f\x98\x80\xef\xbf\xbdZ\xef\xbf\xbd"
			 "ABQ\xe2\x98\x83");
	g_assert_cmpuint (c.issues[GMIME_RTF_COMPLIANCE_VIOLATION_UNPAIRED_SURROGATE], ==, 2);
	clear_capture (&c);
}

static void
test_format (void)
{
	Capture c = { 0 };
	c.check_format = TRUE;
	read_rtf (&c, "{\\rtf1{\\colortbl\\blue255;\\red999\\green-4\\blue34;}\\intbl\\qc"
		  "{\\b\\i\\ulwave\\striked\\super\\fs43\\cf1\\highlight1 A{\\plain B}"
		  "{\\v hidden\\par\\objattph\\cell}C}\\nestcell{\\*\\nesttableprops\\nestrow}"
		  "\\pard D\\page\\line}", FALSE, 8, 8, 8);
	g_assert_cmpstr (c.text->str, ==, "ABC[cell][row]D[par][br]");
	clear_capture (&c);
}

static void
test_pages (void)
{
	Capture c = { 0 };
	GString *rtf = g_string_new ("{\\rtf1\\ansi\\deff1{\\fonttbl{\\f1{\\*\\panose 42}\\fcharset204}"
				    "{\\f2\\fcharset204\\cpg932;}}\\'df{\\f2 ");
	guint k;

	/* The lead byte lands at the very end of the decoder's internal buffer.
	 * The trail byte (escaped backslash) must not flush or reset the decoder. */
	for (k = 0; k < 1023; k++)
		g_string_append_c (rtf, 'a');
	g_string_append (rtf, "\\'95\\\\}\\'df{\\f0\\'80}\\ansicpg99999\\f0\\'80}");
	read_rtf (&c, rtf->str, FALSE, 8, 8, 8);
	g_assert_cmpuint (c.text->len, ==, 2 + 1023 + 3 + 2 + 3 + 3);
	g_assert_true (g_str_has_prefix (c.text->str, "\xd0\xaf"));
	g_assert_true (g_str_has_suffix (c.text->str, "\xe8\xa1\xa8\xd0\xaf\xe2\x82\xac\xe2\x82\xac"));
	g_assert_cmpuint (c.issues[GMIME_RTF_COMPLIANCE_VIOLATION_UNSUPPORTED_CODE_PAGE], ==, 1);
	g_string_free (rtf, TRUE);
	clear_capture (&c);

	read_rtf (&c, "{\\rtf1\\mac\\ansicpg10001\\'82\\'a8\\ansicpg950\\'a4\\'20Z}", FALSE, 8, 8, 8);
	g_assert_cmpstr (c.text->str, ==, "\xe3\x81\x8a\xef\xbf\xbd Z");
	g_assert_cmpuint (c.issues[GMIME_RTF_COMPLIANCE_VIOLATION_UNSUPPORTED_CODE_PAGE], ==, 0);
	clear_capture (&c);
}

static void
test_links (void)
{
	Capture c = { 0 };
	read_rtf (&c, "{\\rtf1{\\field{\\*\\fldinst HYPERLINK \\\\o \"tooltip\" \"https://a\"}"
		  "{\\fldrslt A{\\field{\\*\\fldinst HYPERLINK mailto:b}{\\fldrslt B}}C}}"
		  "{\\field{\\*\\fldinst HYPERLINK \"javascript:alert(1)\"}{\\fldrslt D}}"
		  "{\\field{\\*\\fldinst HYPERLINK \\\\l \"bookmark\"}{\\fldrslt E}}F}", FALSE, 16, 8, 8);
	g_assert_cmpstr (c.text->str, ==, "ABCDEF");
	g_assert_cmpstr (c.links->str, ==, "A=https://a;B=mailto:b;C=https://a;");
	clear_capture (&c);
}

static void
test_destinations (void)
{
	const char *rtf = "{\\rtf1\\ansi\\fromhtml1{\\*\\htmltag7 <p>\\par\\_}A"
		"{\\htmlrtf fallback{\\htmlrtf0 B}\\objattph\\'20}"
		"{\\upr{ansi}{\\*\\ud U}}{\\mmathPr secret}{\\mmathpr V}{\\*\\unknown X}"
		"{\\object{\\*\\objdata 01}{\\result O}}\\objattph\\'20Z}";
	Capture c = { 0 };

	read_rtf (&c, rtf, TRUE, 12, 8, 8);
	g_assert_true (c.from_html);
	g_assert_cmpstr (c.text->str, ==, "ABUVO[obj]Z");
	g_assert_cmpstr (c.html->str, ==, "<p>\r\n\xc2\xad");
	clear_capture (&c);
	read_rtf (&c, rtf, FALSE, 12, 8, 8);
	g_assert_cmpstr (c.text->str, ==, "AfallbackB[obj]UVO[obj]Z");
	g_assert_cmpstr (c.html->str, ==, "");
	clear_capture (&c);
	read_rtf (&c, "{\\rtf1\\b\\i\\ul\\qc\\f0\\deff0\\fs24\\ansi\\fromhtml1 X{\\*\\htmltag N}}", TRUE, 4, 4, 4);
	g_assert_false (c.from_html);
	g_assert_cmpstr (c.text->str, ==, "X");
	clear_capture (&c);
}

static void
test_limits (void)
{
	Capture c = { 0 };
	GString *rtf = g_string_new ("{\\rtf1{\\fonttbl\\f1;\\f2;\\f3;}{\\colortbl;;;}");
	guint k;

	for (k = 0; k < 4096; k++)
		g_string_append_c (rtf, '{');
	g_string_append (rtf, "A\\b{B\\i{drop{more}}C}D");
	for (k = 0; k < 4096; k++)
		g_string_append_c (rtf, '}');
	g_string_append (rtf, "{\\field{\\*\\fldinst ");
	for (k = 0; k < 3000; k++)
		g_string_append_c (rtf, 'a');
	g_string_append (rtf, "}{\\fldrslt E}}}");
	read_rtf (&c, rtf->str, FALSE, 2, 1, 1);
	g_assert_cmpstr (c.text->str, ==, "ABCDE");
	g_assert_cmpuint (c.issues[GMIME_RTF_COMPLIANCE_VIOLATION_GROUP_DEPTH_LIMIT_EXCEEDED], ==, 1);
	g_assert_cmpuint (c.issues[GMIME_RTF_COMPLIANCE_VIOLATION_FONT_TABLE_LIMIT_EXCEEDED], ==, 1);
	g_assert_cmpuint (c.issues[GMIME_RTF_COMPLIANCE_VIOLATION_COLOR_TABLE_LIMIT_EXCEEDED], ==, 1);
	g_assert_cmpuint (c.issues[GMIME_RTF_COMPLIANCE_VIOLATION_FIELD_INSTRUCTION_TOO_LONG], ==, 1);
	clear_capture (&c);
	g_string_free (rtf, TRUE);
}

static gboolean
abort_on_text (GMimeRtfEvent event, const GMimeRtfFormat *f, const char *s, gsize n, gpointer data, GError **error)
{
	guint *events = data;
	(void) f; (void) s; (void) n;
	(*events)++;
	if (event == GMIME_RTF_EVENT_BEGIN)
		return TRUE;
	g_set_error_literal (error, G_FILE_ERROR, G_FILE_ERROR_FAILED, "sink closed");
	return FALSE;
}

static void
test_step_error (void)
{
	const char *rtf = "{\\rtf1 X\\par Y}";
	GMimeStream *in = g_mime_stream_mem_new_with_buffer (rtf, strlen (rtf));
	GMimeRtfInterpreter *interpreter;
	guint events = 0;
	gboolean eof = FALSE;
	GError *error = NULL;

	interpreter = _g_mime_rtf_interpreter_new (in, FALSE, 8, 8, 8, NULL, abort_on_text, &events);
	while (_g_mime_rtf_interpreter_step (interpreter, &eof, &error))
		g_assert_false (eof);
	g_assert_error (error, G_FILE_ERROR, G_FILE_ERROR_FAILED);
	g_assert_cmpuint (events, ==, 2);
	g_clear_error (&error);
	_g_mime_rtf_interpreter_free (interpreter);
	g_object_unref (in);
}

int
main (int argc, char **argv)
{
	int result;

	g_test_init (&argc, &argv, NULL);
	g_mime_init ();
	g_test_add_func ("/rtf/interpreter/unicode", test_unicode);
	g_test_add_func ("/rtf/interpreter/format", test_format);
	g_test_add_func ("/rtf/interpreter/pages", test_pages);
	g_test_add_func ("/rtf/interpreter/links", test_links);
	g_test_add_func ("/rtf/interpreter/destinations", test_destinations);
	g_test_add_func ("/rtf/interpreter/limits", test_limits);
	g_test_add_func ("/rtf/interpreter/step-error", test_step_error);
	result = g_test_run ();
	g_mime_shutdown ();
	return result;
}
