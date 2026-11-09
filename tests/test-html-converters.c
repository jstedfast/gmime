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
/* Formatting and regression fixtures are drawn from MimeKit's Text tests. */

#include <errno.h>
#include <string.h>
#include <gmime/gmime.h>
#include <gmime/gmime-html-to-text.h>
#include <gmime/gmime-html-to-html.h>
#include <gmime/gmime-text-previewer.h>

static const char *data_dir = "tests/data/html-converters";

typedef struct {
	const char *input, *expected;
} Case;

typedef struct {
	GMimeStream parent;
	const char *data;
	gsize length, position, chunk;
	gboolean fail_read, fail_write;
	guint closes;
} ProbeStream;

typedef GMimeStreamClass ProbeStreamClass;

G_DEFINE_TYPE (ProbeStream, probe_stream, GMIME_TYPE_STREAM)

static ssize_t
probe_read (GMimeStream *stream, char *buffer, size_t count)
{
	ProbeStream *probe = (ProbeStream *) stream;

	if (probe->fail_read) {
		errno = EIO;
		return -1;
	}

	count = MIN (count, MIN (probe->chunk, probe->length - probe->position));
	memcpy (buffer, probe->data + probe->position, count);
	probe->position += count;
	return count;
}

static ssize_t
probe_write (GMimeStream *stream, const char *buffer, size_t count)
{
	ProbeStream *probe = (ProbeStream *) stream;

	(void) buffer;
	if (probe->fail_write) {
		errno = EIO;
		return -1;
	}

	return count;
}

static gboolean
probe_eos (GMimeStream *stream)
{
	ProbeStream *probe = (ProbeStream *) stream;

	return probe->position == probe->length;
}

static int
probe_close (GMimeStream *stream)
{
	((ProbeStream *) stream)->closes++;
	return 0;
}

static int
probe_flush (GMimeStream *stream)
{
	(void) stream;
	return 0;
}

static void
probe_stream_class_init (ProbeStreamClass *klass)
{
	klass->read = probe_read;
	klass->write = probe_write;
	klass->eos = probe_eos;
	klass->close = probe_close;
	klass->flush = probe_flush;
}

static void
probe_stream_init (ProbeStream *probe)
{
	g_mime_stream_construct (GMIME_STREAM (probe), 0, -1);
	probe->chunk = 1;
}

static void
check (GMimeTextConverter *converter, const char *input, const char *expected)
{
	GError *error = NULL;
	char *actual = g_mime_text_converter_convert_to_string (converter, input, -1, &error);

	g_assert_no_error (error);
	if (g_strcmp0 (actual, expected))
		g_test_message ("Input: %s\nActual: %s\nExpected: %s", input, actual, expected);

	g_assert_cmpstr (actual, ==, expected);
	g_free (actual);
	/* Exercise tokenizer, UTF-8 and newline state across every byte boundary. */
	ProbeStream *chunked = g_object_new (probe_stream_get_type (), NULL);
	GMimeStream *output = g_mime_stream_mem_new ();
	GByteArray *bytes = g_mime_stream_mem_get_byte_array (GMIME_STREAM_MEM (output));

	chunked->data = input;
	chunked->length = strlen (input);
	g_assert_true (g_mime_text_converter_convert (converter, GMIME_STREAM (chunked), output, &error));
	g_assert_no_error (error);
	g_assert_cmpuint (bytes->len, ==, strlen (expected));
	g_assert_cmpmem (bytes->data, bytes->len, expected, strlen (expected));
	g_assert_cmpuint (chunked->closes, ==, 0);
	g_object_unref (output);
	g_object_unref (chunked);
}

static void
test_html_to_text (void)
{
	static const Case cases[] = {
		{ "", "" },
		{ " \t\r\n ", "" },
		{ "one \t\r\n two", "one two" },
		{ "<p>Hello <b>bold</b> and <em>italic</em>.</p><p>Next.</p>", "Hello bold and italic.\n\nNext." },
		{ "<div>one</div><div>two<br>three</div>", "one\ntwo\nthree" },
		{ "one<br><br>two<br>", "one\n\ntwo\n" },
		{ "<br>one</br>two", "\none\ntwo" },
		{ "<p></p><div></div><p>one</p><p></p>", "one" },
		{ "<h1>Title</h1><h2>Subtitle</h2><p>Body.</p><hr><p>End.</p>",
		  "# Title\n\n## Subtitle\n\nBody.\n\n---\n\nEnd." },
		{ "<h1>Title</h2><p>Text", "# Title\n\nText" },
		{ "<p>one<p>two<div>three</div>four", "one\n\ntwo\n\nthree\nfour" },
		{ "one<!-- comment -->two &amp; &lt;three&gt; &#x1f600; &nbsp; end",
		  "onetwo & <three> \360\237\230\200 \302\240 end" },
		{ "<html><head><title>Hidden</title><style>p { color:red }</style><script>x()</script></head><body><p>Visible</p></body></html>",
		  "Visible" },
		{ "<head><title>Hidden</title><p>Visible", "Visible" },
		{ "a<script/>bad <b>stuff</b></script>b<style/>bad</style>c", "abc" },
		{ "<div hidden><p>Hidden</p></div><p>Visible</p>", "Visible" },
		{ "<body hidden><p>Hidden</p></body>", "" },
		{ "<html hidden><body>Hidden</body></html>", "" },
		{ "<p hidden>Hidden<p>Visible", "Visible" },
		{ "<ul><li hidden>Hidden<li>Visible</ul>", "- Visible" },
		{ "<h1 hidden>Hidden<h2>Visible", "## Visible" },
		{ "<ul hidden><li>Hidden<li>Still hidden</ul>Visible", "Visible" },
		{ "<template><p>Hidden</p><template>Also hidden</template></template>Visible", "Visible" },
		{ "<noscript><p>Fallback</p></noscript>", "Fallback" },
		{ "<p>x<template><div>Hidden</div></template>y</p>", "xy" },
		{ "<a href='url'>x<template><a href='hidden'>Hidden</a></template>y</a>", "xy (url)" },
		{ "<p>x<div hidden><p>Hidden</p></div>y", "x\n\ny" },
		{ "<svg><text>SVG &amp; text</text><![CDATA[ <raw> ]]></svg>", "SVG & text <raw>" },
		{ "<ul><li>one</li><li>two</li></ul><p>End</p>", "- one\n- two\n\nEnd" },
		{ "<ol start='3'><li>three<li value='7'>seven<li>eight</ol>", "3. three\n7. seven\n8. eight" },
		{ "<ol start='-2'><li>a<li>b</ol>", "-2. a\n-1. b" },
		{ "<ol start='bad'><li value='bad'>a<li>b</ol>", "1. a\n2. b" },
		{ "<ul><li>outer<ol><li>inner<br>continued</li><li>second</li></ol>tail</li><li>last</li></ul>",
		  "- outer\n  1. inner\n     continued\n  2. second\n  tail\n- last" },
		{ "<ul><li><p>First.</p><p>Second.</p></li><li>Next.</li></ul>", "- First.\n\n  Second.\n- Next." },
		{ "<ul><li></li><li>x</li></ul>", "-\n- x" },
		{ "<ul><li><br>next</li><li>end</li></ul>", "-\n  next\n- end" },
		{ "<blockquote>one<br>two<blockquote>inner</blockquote>tail</blockquote>end",
		  "> one\n> two\n\n>> inner\n\n> tail\n\nend" },
		{ "<blockquote><ul><li>one<li>two</ul></blockquote>", "> - one\n> - two" },
		{ "<dl><dt>Term<dd>Meaning<dt>Other<dd>Definition</dl>", "Term\n  Meaning\nOther\n  Definition" },
		{ "<pre>\n  one\t two\n\nthree  </pre><p>End</p>", "  one\t two\n\nthree  \n\nEnd" },
		{ "<pre>\r\none\r\ntwo\rthree\n</pre>", "one\ntwo\nthree\n" },
		{ "<pre>  &lt;b&gt; <b>bold</b>\t end</pre>", "  <b> bold\t end" },
		{ "<pre><!-- comment -->\nkeep</pre>", "\nkeep" },
		{ "<pre><b>\nkeep</b></pre>", "\nkeep" },
		{ "<pre>\n\nkeep</pre>", "\nkeep" },
		{ "<pre>one<br>two</pre>", "one\ntwo" },
		{ "<plaintext>\n  <b>literal</b>", "\n  <b>literal</b>" },
		{ "<blockquote><pre>\na\n b\n</pre></blockquote>", "> a\n>  b\n" },
		{ "<textarea>\n a &amp; b\n</textarea>", " a & b\n" },
		{ "<xmp><b>literal</b></xmp>", "<b>literal</b>" },
		{ "Visit <a href='https://example.com/?a=1&amp;b=2'>Example</a>.",
		  "Visit Example (https://example.com/?a=1&b=2)." },
		{ "See <a href='https://example.com'>https://example.com</a>.", "See https://example.com." },
		{ "<a href='/relative'><b>Label</b></a>", "Label (/relative)" },
		{ "<a href='url'></a>", "url" },
		{ "<a href='url'>Label", "Label (url)" },
		{ "<a href='url'><p>Label</p></a>", "Label (url)" },
		{ "<a href='outer'>one<a href='inner'>two</a>", "one (outer)two (inner)" },
		{ "<a href='  '>Label</a><a>No URL</a>", "LabelNo URL" },
		{ "<a href='url'> url </a>", "url" },
		{ "<a href='\302\240url\302\240'>url</a>", "url" },
		{ "<a href='url'><pre> url</pre></a>", " url" },
		{ "<a href='url'>Label <script>Hidden</script> text</a>", "Label text (url)" },
		{ "An <img alt='A &amp; B'> image<img src='x'>.", "An A & B image." },
		{ "<a href='url'><img alt='Image'></a>", "Image (url)" },
		{ "<table><tr><th>Name</th><th>Value</th></tr><tr><td>A</td><td>B</td></tr></table>",
		  "Name Value\nA B" },
		{ "<table><tr><td>A<td>B<tr><td>C<td>D</table>", "A B\nC D" },
		{ "<div><b>one</div>two</b>", "one\ntwo" },
		{ "<p>one</unknown>two</p>", "onetwo" },
		{ "one<img alt='truncated", "one" },
		{ "<div/>one</div>two", "one\ntwo" },
		{ "<H1>Title</H1><P><A HREF='u'>link</A><BR><IMG ALT='alt'></P>", "# Title\n\nlink (u)\nalt" }
	};
	GMimeTextConverter *converter = g_mime_html_to_text_new ();

	for (guint i = 0; i < G_N_ELEMENTS (cases); i++)
		check (converter, cases[i].input, cases[i].expected);

	g_mime_html_to_text_set_output_link_urls (GMIME_HTML_TO_TEXT (converter), FALSE);
	g_mime_html_to_text_set_output_image_alt_text (GMIME_HTML_TO_TEXT (converter), FALSE);
	g_mime_text_converter_set_header (converter, "[header]");
	g_mime_text_converter_set_footer (converter, "[footer]");
	check (converter, "<p><a href='url'>Label</a><img alt='Image'></p>", "[header]Label[footer]");
	g_object_unref (converter);
}

static void
test_limits (void)
{
	GMimeTextConverter *converter = g_mime_html_to_text_new ();
	GMimeHtmlToText *html = GMIME_HTML_TO_TEXT (converter);
	GError *error = NULL;
	char *result;

	g_mime_html_to_text_set_max_element_depth (html, 4);
	check (converter, "<b><b><b><b>x</b></b></b></b>", "x");
	result = g_mime_text_converter_convert_to_string (converter, "<b><b><b><b><b>x", -1, &error);
	g_assert_null (result);
	g_assert_nonnull (error);
	g_clear_error (&error);
	check (converter, "ok", "ok");
	g_mime_html_to_text_set_max_input_length (html, 5);
	check (converter, "12345", "12345");
	result = g_mime_text_converter_convert_to_string (converter, "<!--12345", -1, &error);
	g_assert_null (result);
	g_assert_nonnull (error);
	g_clear_error (&error);
	g_mime_html_to_text_set_max_input_length (html, 1024);
	g_mime_html_to_text_set_max_output_length (html, 7);
	check (converter, "<a href='url'>x</a>", "x (url)");
	g_mime_html_to_text_set_max_output_length (html, 6);
	result = g_mime_text_converter_convert_to_string (converter, "<a href='url'>x</a>", -1, &error);
	g_assert_null (result);
	g_assert_nonnull (error);
	g_clear_error (&error);
	check (converter, "ok", "ok");
	/* Limits also cover suppressed and truncated tokens, and fail before
	 * consuming an arbitrarily large tail of the caller's stream. */
	ProbeStream *input = g_object_new (probe_stream_get_type (), NULL);
	GMimeStream *output = g_mime_stream_mem_new ();
	GString *large = g_string_new ("<img alt='");

	for (guint i = 0; i < 10000; i++)
		g_string_append_c (large, 'x');

	input->data = large->str;
	input->length = large->len;
	g_mime_html_to_text_set_max_input_length (html, 64);
	g_assert_false (g_mime_text_converter_convert (converter, GMIME_STREAM (input), output, &error));
	g_assert_nonnull (error);
	g_clear_error (&error);
	g_assert_cmpuint (input->position, <, input->length);
	g_assert_cmpuint (input->closes, ==, 0);
	g_object_unref (output);
	g_object_unref (input);
	g_string_free (large, TRUE);
	g_object_unref (converter);
}

static gboolean
remove_images (GMimeHtmlTagContext *context, GMimeHtmlWriter *writer, gpointer user_data, GError **error)
{
	(void) user_data;
	if (g_mime_html_tag_context_get_tag_id (context) == GMIME_HTML_TAG_IMAGE) {
		g_mime_html_tag_context_set_delete_tag (context, TRUE);
		g_mime_html_tag_context_set_delete_end_tag (context, TRUE);
		return TRUE;
	}

	return g_mime_html_tag_context_write_tag (context, writer, TRUE, error);
}

static gboolean
suppress (GMimeHtmlTagContext *context, GMimeHtmlWriter *writer, gpointer user_data, GError **error)
{
	const char *name = g_mime_html_tag_context_get_tag_name (context);

	if (!g_ascii_strcasecmp (name, user_data)) {
		g_mime_html_tag_context_set_suppress_inner_content (context, TRUE);
		g_mime_html_tag_context_set_delete_tag (context, TRUE);
		g_mime_html_tag_context_set_delete_end_tag (context, TRUE);
		return TRUE;
	}

	return g_mime_html_tag_context_write_tag (context, writer, TRUE, error);
}

static gboolean
remove_images_and_divs (GMimeHtmlTagContext *context, GMimeHtmlWriter *writer, gpointer user_data, GError **error)
{
	if (g_mime_html_tag_context_get_tag_id (context) == GMIME_HTML_TAG_DIV) {
		g_mime_html_tag_context_set_delete_tag (context, TRUE);
		g_mime_html_tag_context_set_delete_end_tag (context, TRUE);
		return TRUE;
	}

	return remove_images (context, writer, user_data, error);
}

static void
test_foreign_content (void)
{
	static const Case cases[] = {
		{ "<svg><body><style><img src=x onerror=alert(1)></style>",
		  "<svg></svg><body><style><img src=x onerror=alert(1)></style>" },
		{ "<svg><img src=y><style><img src=x onerror=alert(1)></style>",
		  "<svg></svg><style><img src=x onerror=alert(1)></style>" },
		{ "<div><svg><g></div><style><img src=x onerror=alert(1)></style>",
		  "<svg><g></g></svg><style><img src=x onerror=alert(1)></style>" },
		{ "<math><mtext><svg><p></math><b>x</b>", "<math><mtext><svg></svg><p></math><b>x</b>" },
		{ "<svg><g><g><g></p>x", "<svg><g><g><g></g></g></g></svg></p>x" },
		{ "<svg><g></svg>x", "<svg><g></g></svg>x" },
		{ "<mi/r><table><math><mi><e><tr>", "<mi r><table><math><mi><e></e></mi></math><tr>" },
		{ "<mi><table><math><mi><ne></mi></math><tr><b>x</b>",
		  "<mi><table><math><mi><ne></mi></math></ne></mi></math><tr><b>x</b>" },
		{ "<table><math><mi><e/>x<table>", "<table><math><mi><e/>x</e></mi></math><table>" },
		{ "<table><math><mi><e/>x</mi></math><table>",
		  "<table><math><mi><e/>x</mi></math></e></mi></math><table>" },
		{ "<svg><noscript><g><p>x", "<svg><g></g></svg><p>x" },
		{ "<frameset><textarea><!--</textarea><img src=x onerror=alert(1)>--></frameset>",
		  "<frameset><!--</textarea><img src=x onerror=alert(1)>--></frameset>" },
		{ "<body><frameset><d>x</d>", "<body><d>x</d>" }
	};
	GMimeTextConverter *converter = g_mime_html_to_html_new ();

	g_mime_html_to_html_set_html_tag_callback (GMIME_HTML_TO_HTML (converter), remove_images_and_divs, NULL, NULL);
	for (guint i = 0; i < G_N_ELEMENTS (cases); i++) {
		check (converter, cases[i].input, cases[i].expected);
		check (converter, cases[i].expected, cases[i].expected);
	}

	g_object_unref (converter);
}

static gboolean
record_callback (GMimeHtmlTagContext *context, GMimeHtmlWriter *writer, gpointer data, GError **error)
{
	GString *tags = data;

	g_mime_html_tag_context_set_invoke_callback_for_end_tag (context, TRUE);
	if (g_mime_html_tag_context_get_is_end_tag (context))
		g_string_append_c (tags, '/');

	g_string_append (tags, g_mime_html_tag_context_get_tag_name (context));
	g_string_append_c (tags, ' ');
	return g_mime_html_tag_context_write_tag (context, writer, TRUE, error);
}

static gboolean
fail_callback (GMimeHtmlTagContext *context, GMimeHtmlWriter *writer, gpointer data, GError **error)
{
	(void) context;
	(void) writer;
	(void) data;
	g_set_error_literal (error, G_MARKUP_ERROR, G_MARKUP_ERROR_INVALID_CONTENT, "callback failure");
	return FALSE;
}

static void
count_issue (const GMimeHtmlComplianceIssue *issue, gpointer data)
{
	guint *count = data;

	g_assert_nonnull (issue->description);
	(*count)++;
}

static void
test_callbacks_compliance (void)
{
	GMimeTextConverter *converter = g_mime_html_to_html_new ();
	GMimeHtmlToHtml *html = GMIME_HTML_TO_HTML (converter);
	GString *tags = g_string_new (NULL);
	GError *error = NULL;
	char *result;
	guint issues = 0;

	g_mime_html_to_html_set_html_tag_callback (html, record_callback, tags, NULL);
	result = g_mime_text_converter_convert_to_string (converter, "<div><noscript><b>x</b></noscript></div>", -1,
							  &error);
	g_assert_no_error (error);
	g_assert_cmpstr (result, ==, "<div><b>x</b></div>");
	g_free (result);
	g_assert_cmpstr (tags->str, ==, "div b /b /div ");
	g_mime_html_to_html_set_html_tag_callback (html, fail_callback, NULL, NULL);
	result = g_mime_text_converter_convert_to_string (converter, "<p>x</p>", -1, &error);
	g_assert_null (result);
	g_assert_error (error, G_MARKUP_ERROR, G_MARKUP_ERROR_INVALID_CONTENT);
	g_clear_error (&error);
	g_mime_html_to_html_set_html_tag_callback (html, NULL, NULL, NULL);
	g_mime_html_to_html_set_compliance_logger (html, count_issue, &issues, NULL);
	for (guint i = 0; i < 2; i++) {
		guint previous = issues;

		result = g_mime_text_converter_convert_to_string (converter, "<p x=1 X=2>&missing;</p>", -1, &error);
		g_assert_no_error (error);
		g_assert_nonnull (result);
		g_free (result);
		g_assert_cmpuint (issues, >, previous);
	}

	g_string_free (tags, TRUE);
	g_object_unref (converter);
}

static void
test_fragments (void)
{
	static const Case cases[] = {
		{ "<!DOCTYPE html><html><head><title>t</title><style>p{}</style></head><body><p>x<br/></p></body></html>",
		  "<p>x<br/></p>" },
		{ "<html><body/><body>x</body></html>", "x" },
		{ "<head><meta charset=utf-8></head>x</body></html><p>y</p>", "x<p>y</p>" },
		{ "<head><math></head><script><t>", "<math><script><t>" },
		{ "<head><title>T</title><div>D</div></head><body>B</body>", "<div>D</div>B" },
		{ "<head><title>T</title>junk<style>s</style></head>B", "junk<style>s</style>B" },
		{ "<head><title>T</title><math><mi>x</mi></head><script><t>", "<math><mi>x</mi><script><t>" },
		{ "<head> \r\n<title>T</title> </head>B", "B" },
		{ "<head><o:p>x</o:p></head>", "<o:p>x</o:p>" },
		{ "<head><body>B</body>", "B" },
		{ "<head><meta charset=utf-8></body>B", "B" },
		{ "<head/><title>T</title>B", "B" },
		{ "<head>A</head><head>B</head>", "AB" },
		{ "<head><head><title>T</title></head>B</head>", "B" },
		{ "<p>A</p><head><style>s</style></head>", "<p>A</p><style>s</style>" },
		{ "A<head><title>T</title></head>", "A<title>T</title>" },
		{ "<title>T</title><head><title>U</title></head>B", "<title>T</title>B" },
		{ "<head><template><div>D</div></head></template>X</head>B", "XB" },
		{ "<head><template><math></template></head>B", "B" },
		{ "<head><noscript><b>x</b></noscript></head>B", "<b>x</b>B" }
	};
	GMimeTextConverter *converter = g_mime_html_to_html_new ();

	g_mime_html_to_html_set_output_html_fragment (GMIME_HTML_TO_HTML (converter), TRUE);
	for (guint i = 0; i < G_N_ELEMENTS (cases); i++) {
		check (converter, cases[i].input, cases[i].expected);
		check (converter, cases[i].expected, cases[i].expected);
	}

	g_object_unref (converter);
}

static void
test_html_filter (void)
{
	GMimeTextConverter *converter = g_mime_html_to_html_new ();
	GMimeHtmlToHtml *html = GMIME_HTML_TO_HTML (converter);

	check (converter, "foo<img src=x", "foo");
	check (converter, "foo<", "foo&lt;");
	check (converter,
	       "<head><noscript><link rel=\"stylesheet\" href=\"x.css\"></noscript></head><body><noscript/>a<NOSCRIPT>b</NoScript>c</body>",
	       "<head><link rel=\"stylesheet\" href=\"x.css\"/></head><body>abc</body>");
	g_mime_html_to_html_set_html_tag_callback (html, remove_images, NULL, NULL);
	check (converter, "<p>text</p><noscript><img src=\"x\"><p>fallback</p></noscript>",
	       "<p>text</p><p>fallback</p>");
	check (converter, "<noscript><style></noscript><img src=x onerror=alert(1)></style></noscript>",
	       "<style></noscript><img src=x onerror=alert(1)></style>");
	check (converter, "<noscript><!--</noscript><img src=x onerror=alert(1)>--></noscript>",
	       "<!--</noscript><img src=x onerror=alert(1)>-->");
	check (converter, "<span><noscript><svg></span>x</noscript></span>y", "<span><svg>x</svg></span>y");
	g_mime_html_to_html_set_no_script_handling (html, GMIME_HTML_NO_SCRIPT_HANDLING_SCRIPTING_ENABLED);
	check (converter, "<noscript><img src=\"x\"></noscript>", "<noscript><img src=\"x\"></noscript>");
	g_mime_html_to_html_set_no_script_handling (html, GMIME_HTML_NO_SCRIPT_HANDLING_SCRIPTING_DISABLED);
	check (converter, "<noscript><img src=\"x\"></noscript>", "<noscript></noscript>");
	g_mime_html_to_html_set_no_script_handling (html, GMIME_HTML_NO_SCRIPT_HANDLING_UNWRAP);
	g_mime_html_to_html_set_filter_comments (html, TRUE);
	check (converter, "<<!--x-->img>", "&lt;img>");
	check (converter, "<svg><img src=y><style><img src=x onerror=alert(1)></style>",
	       "<svg></svg><style><img src=x onerror=alert(1)></style>");
	check (converter, "<svg><g></svg>x", "<svg><g></g></svg>x");
	g_mime_html_to_html_set_output_html_fragment (html, TRUE);
	g_mime_html_to_html_set_html_tag_callback (html, suppress, "svg", NULL);
	check (converter, "<svg><circle><body><b>x</b></svg>y", "<b>x</b>y");
	g_mime_html_to_html_set_html_tag_callback (html, suppress, "div", NULL);
	check (converter, "<div><table><tr><td>x</div>y</td></tr></table></div>z", "z");
	g_mime_html_to_html_set_html_tag_callback (html, suppress, "script", NULL);
	check (converter, "<script/>alert(1)</script><p>x</p>", "<p>x</p>");
	g_mime_html_to_html_set_html_tag_callback (html, NULL, NULL, NULL);
	g_mime_html_to_html_set_output_html_fragment (html, FALSE);
	g_mime_text_converter_set_header (converter, "<html>");
	g_mime_text_converter_set_footer (converter, "</html>");
	check (converter, "<body>x</body>", "&lt;html&gt;<br/><body>x</body>&lt;/html&gt;<br/>");
	g_mime_html_to_html_set_header_format (html, GMIME_HEADER_FOOTER_FORMAT_HTML);
	g_mime_html_to_html_set_footer_format (html, GMIME_HEADER_FOOTER_FORMAT_HTML);
	check (converter, "<body>x</body>", "<html><body>x</body></html>");
	g_mime_text_converter_set_header (converter, NULL);
	g_mime_text_converter_set_footer (converter, NULL);
	g_mime_html_to_html_set_max_element_depth (html, 3);
	check (converter, "<div><span><b><i>x<img src=x onerror=alert(1)>&amp;</i></b>",
	       "<div><span><b><i>x&lt;img src=x onerror=alert(1)&gt;&amp;amp;&lt;/i&gt;&lt;/b&gt;");
	g_object_unref (converter);
}

static void
check_preview (GMimeTextPreviewer *previewer, const char *input, const char *expected)
{
	GError *error = NULL;
	char *actual = g_mime_text_previewer_get_preview_text (previewer, input, &error);

	g_assert_no_error (error);
	g_assert_cmpstr (actual, ==, expected);
	g_free (actual);
	ProbeStream *stream = g_object_new (probe_stream_get_type (), NULL);

	stream->data = input;
	stream->length = strlen (input);
	actual = g_mime_text_previewer_get_preview_text_from_stream (previewer, GMIME_STREAM (stream), "utf-8", &error);
	g_assert_no_error (error);
	g_assert_cmpstr (actual, ==, expected);
	g_assert_cmpuint (stream->closes, ==, 0);
	g_free (actual);
	g_object_unref (stream);
}

static void
test_preview (void)
{
	static const Case cases[] = { { "<body><ol><li>a<ul><li>b</ul><li>c</ol>d", "1. a b 2. cd" },
				      { "<body><ol><li>a<ul><li>b</ol><li>c</ul>d", "1. a b cd" },
				      { "<body><ul><li>a<ol><li>b</ul><li>c</ol>d", "a 1. b 2. cd" },
				      { "<body><ol><li>a<ol><li>b</ol><li>c</ol>d", "1. a 1. b 2. cd" },
				      { "<body><b>a<script>b<i>c</b>d</script>e</i>f", "aef" },
				      { "<body><table><td>a</table>b<tr>c</td>d</tr>e", "abe" },
				      { "<body>a</body>b<body>c", "ac" },
				      { "<body><noscript><b>Fallback &amp; text</b></noscript></body>",
					"Fallback & text" } };
	GMimeTextPreviewer *html = g_mime_html_text_previewer_new ();
	GMimeTextPreviewer *plain = g_mime_plain_text_previewer_new ();

	for (guint i = 0; i < G_N_ELEMENTS (cases); i++)
		check_preview (html, cases[i].input, cases[i].expected);

	check_preview (plain, "  alpha\r\n beta\tgamma  ", "alpha beta gamma");
	g_mime_text_previewer_set_maximum_preview_length (plain, 5);
	check_preview (plain, "abcdef", "abcd\342\200\246");
	check_preview (plain, "abcde", "abcde");
	g_mime_text_previewer_set_maximum_preview_length (html, 5);
	check_preview (html, "<body>abcdef", "abcd\342\200\246");
	g_mime_text_previewer_set_maximum_preview_length (html, 1);
	check_preview (html, "<body>ab", "\342\200\246");
	g_mime_text_previewer_set_maximum_preview_length (plain, 2);
	check_preview (plain, "\360\237\230\200xy", "\360\237\230\200\342\200\246");
	g_mime_text_previewer_set_maximum_preview_length (html, 2);
	check_preview (html, "<body>&#x1f600;xy", "\360\237\230\200\342\200\246");
	check_preview (html, "<body>   ", "");
	g_mime_text_previewer_set_maximum_preview_length (plain, 230);
	GError *error = NULL;
	char *actual = g_mime_text_previewer_get_preview_text (plain, "a\342\200\213b", &error);

	g_assert_no_error (error);
	g_assert_cmpstr (actual, ==, "a b");
	g_free (actual);
	GMimeStream *stream = g_mime_stream_mem_new_with_buffer ("a\342\200\213b", 5);

	actual = g_mime_text_previewer_get_preview_text_from_stream (plain, stream, "x-unknown", &error);
	g_assert_no_error (error);
	g_assert_cmpstr (actual, ==, "a\342\200\213b");
	g_free (actual);
	g_object_unref (stream);
	g_object_unref (html);
	g_object_unref (plain);
}

static void
test_preview_fixtures (void)
{
	static const struct {
		const char *name, *expected;
		gboolean html;
	} fixtures[] = {
		{ "homedepot-check-inside-now.html", "FREE DELIVERY Appliance Purchases $396 or More", TRUE },
		{ "mimekit.net.html",
		  "Toggle navigation MimeKit Home About Help Documentation Donate \303\227 Install with NuGet (recommended) NuGet PM> Install-Package MimeKit PM> Install-Package MailKit or Install via VS Package Management window. Direct Download ZIP fil\342\200\246",
		  TRUE },
		{ "planet-fitness.html", "Don\342\200\231t miss our celebrity guest Monday evening", TRUE },
		{ "planet-fitness.txt",
		  "Planet Fitness https://view.email.planetfitness.com/?qs=9a098a031cabde68c0a4260051cd6fe473a2e997a53678ff26b4b199a711a9d2ad0536530d6f837c246b09f644d42016ecfb298f930b7af058e9e454b34f3d818ceb3052ae317b1ac4594aab28a2d788 View web ver\342\200\246",
		  FALSE }
	};

	for (guint i = 0; i < G_N_ELEMENTS (fixtures); i++) {
		char *path = g_build_filename (data_dir, fixtures[i].name, NULL), *text, *actual;
		GError *error = NULL;
		gsize length;
		GMimeTextPreviewer *previewer =
			fixtures[i].html ? g_mime_html_text_previewer_new () : g_mime_plain_text_previewer_new ();
		GMimeTextPart *part = g_mime_text_part_new_with_subtype (fixtures[i].html ? "html" : "plain");

		g_assert_true (g_file_get_contents (path, &text, &length, &error));
		g_assert_no_error (error);
		g_mime_text_part_set_text (part, text);
		actual = g_mime_text_previewer_get_preview_text_from_part (part, &error);
		g_assert_no_error (error);
		g_assert_cmpstr (actual, ==, fixtures[i].expected);
		g_free (actual);
		text[MIN (length, 16 * 1024)] = 0;
		check_preview (previewer, text, fixtures[i].expected);
		g_object_unref (part);
		g_object_unref (previewer);
		g_free (path);
		g_free (text);
	}
}

static void
test_preview_part (void)
{
	GMimeTextPart *part = g_mime_text_part_new_with_format (GMIME_TEXT_FORMAT_FLOWED);
	GError *error = NULL;
	char *actual;

	g_mime_text_part_set_text (part, "long \r\nline\r\n");
	g_mime_entity_set_content_type_parameter (GMIME_ENTITY (part), "delsp", "yes");
	actual = g_mime_text_previewer_get_preview_text_from_part (part, &error);
	g_assert_no_error (error);
	g_assert_cmpstr (actual, ==, "longline");
	g_free (actual);
	g_object_unref (part);
	/* A declared unknown charset falls back through UTF-8 to Latin-1. */
	part = g_mime_text_part_new ();
	GMimeStream *input = g_mime_stream_mem_new_with_buffer ("caf\351", 4);
	GMimeContent *content = g_mime_content_new_with_stream (input, GMIME_CONTENT_ENCODING_UNSPECIFIED);

	g_mime_part_set_content (GMIME_PART (part), content);
	g_mime_text_part_set_charset (part, "x-unknown");
	actual = g_mime_text_previewer_get_preview_text_from_part (part, &error);
	g_assert_no_error (error);
	g_assert_cmpstr (actual, ==, "caf\303\251");
	g_free (actual);
	g_object_unref (content);
	g_object_unref (input);
	g_object_unref (part);
	GMimeTextPreviewer *previewer = g_mime_plain_text_previewer_new ();

	input = g_mime_stream_mem_new_with_buffer ("caf\351", 4);
	actual = g_mime_text_previewer_get_preview_text_from_stream (previewer, input, "iso-8859-1", &error);
	g_assert_no_error (error);
	g_assert_cmpstr (actual, ==, "caf\303\251");
	g_free (actual);
	g_object_unref (input);
	g_object_unref (previewer);
}

static void
test_io_failures (void)
{
	for (guint i = 0; i < 2; i++) {
		GMimeTextConverter *converter = i ? g_mime_html_to_html_new () : g_mime_html_to_text_new ();
		ProbeStream *input = g_object_new (probe_stream_get_type (), NULL);
		ProbeStream *output = g_object_new (probe_stream_get_type (), NULL);
		GError *error = NULL;

		input->data = "<p>text</p>";
		input->length = strlen (input->data);
		input->fail_read = TRUE;
		g_assert_false (
			g_mime_text_converter_convert (converter, GMIME_STREAM (input), GMIME_STREAM (output), &error));
		g_assert_nonnull (error);
		g_clear_error (&error);
		input->fail_read = FALSE;
		output->fail_write = TRUE;
		g_assert_false (
			g_mime_text_converter_convert (converter, GMIME_STREAM (input), GMIME_STREAM (output), &error));
		g_assert_nonnull (error);
		g_clear_error (&error);
		g_assert_cmpuint (input->closes, ==, 0);
		g_assert_cmpuint (output->closes, ==, 0);
		check (converter, "ok", "ok");
		g_object_unref (input);
		g_object_unref (output);
		g_object_unref (converter);
	}
}

int
main (int argc, char **argv)
{
	int result;

	for (int i = 1; i < argc; i++) {
		if (g_file_test (argv[i], G_FILE_TEST_IS_DIR))
			data_dir = argv[i];
	}

	g_test_init (&argc, &argv, NULL);
	g_mime_init ();
	g_test_add_func ("/html-converters/text", test_html_to_text);
	g_test_add_func ("/html-converters/limits", test_limits);
	g_test_add_func ("/html-converters/fragments", test_fragments);
	g_test_add_func ("/html-converters/filter", test_html_filter);
	g_test_add_func ("/html-converters/foreign", test_foreign_content);
	g_test_add_func ("/html-converters/callbacks-compliance", test_callbacks_compliance);
	g_test_add_func ("/html-converters/preview", test_preview);
	g_test_add_func ("/html-converters/preview-fixtures", test_preview_fixtures);
	g_test_add_func ("/html-converters/preview-part", test_preview_part);
	g_test_add_func ("/html-converters/io-failures", test_io_failures);
	result = g_test_run ();
	g_mime_shutdown ();
	return result;
}
