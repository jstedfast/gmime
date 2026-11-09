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

#include <errno.h>
#include <string.h>
#include <gmime/gmime.h>
#include <gmime/gmime-html-writer.h>
#include <gmime/gmime-html-utils.h>
#include <gmime/gmime-text-to-html.h>
#include <gmime/gmime-flowed-to-html.h>
#include <gmime/gmime-enriched-to-html.h>

/* Fixture vectors are ported from MimeKit's Text converter and URL tests. */

typedef struct {
	GMimeStream parent;
	const char *text;
	gsize position, length;
	GByteArray *written;
	gboolean fail_read, fail_write, fail_flush;
	guint closes;
} OutputTestStream;

typedef GMimeStreamClass OutputTestStreamClass;

G_DEFINE_FINAL_TYPE (OutputTestStream, output_test_stream, GMIME_TYPE_STREAM)

static ssize_t
test_read (GMimeStream *base, char *buffer, size_t length)
{
	OutputTestStream *test_stream = (OutputTestStream *) base;

	if (test_stream->fail_read) {
		errno = EIO;
		return -1;
	}

	length = MIN (length, MIN ((gsize) 1, test_stream->length - test_stream->position));
	memcpy (buffer, test_stream->text + test_stream->position, length);
	test_stream->position += length;
	return length;
}

static ssize_t
test_write (GMimeStream *base, const char *buffer, size_t length)
{
	OutputTestStream *test_stream = (OutputTestStream *) base;

	if (test_stream->fail_write) {
		errno = ENOSPC;
		return -1;
	}

	length = MIN (length, (gsize) 1);
	g_byte_array_append (test_stream->written, (const guint8 *) buffer, length);
	return length;
}

static gboolean
test_eos (GMimeStream *base)
{
	OutputTestStream *test_stream = (OutputTestStream *) base;

	return test_stream->position == test_stream->length;
}

static int
test_flush (GMimeStream *base)
{
	errno = EIO;
	return ((OutputTestStream *) base)->fail_flush ? -1 : 0;
}

static int
test_close (GMimeStream *base)
{
	((OutputTestStream *) base)->closes++;
	return 0;
}

static void
test_finalize (GObject *object)
{
	g_byte_array_unref (((OutputTestStream *) object)->written);
	G_OBJECT_CLASS (output_test_stream_parent_class)->finalize (object);
}

static void
output_test_stream_class_init (OutputTestStreamClass *klass)
{
	klass->read = test_read;
	klass->write = test_write;
	klass->eos = test_eos;
	klass->flush = test_flush;
	klass->close = test_close;
	G_OBJECT_CLASS (klass)->finalize = test_finalize;
}

static void
output_test_stream_init (OutputTestStream *test_stream)
{
	test_stream->text = "";
	test_stream->written = g_byte_array_new ();
	g_mime_stream_construct (GMIME_STREAM (test_stream), 0, -1);
}

static void
assert_conversion (GMimeTextConverter *converter, const char *input, const char *expected)
{
	GError *error = NULL;
	char *result = g_mime_text_converter_convert_to_string (converter, input, -1, &error);
	OutputTestStream *reader = g_object_new (output_test_stream_get_type (), NULL);
	OutputTestStream *writer = g_object_new (output_test_stream_get_type (), NULL);

	g_assert_no_error (error);
	g_assert_cmpstr (result, ==, expected);
	g_free (result);
	reader->text = input;
	reader->length = strlen (input);
	g_assert_true (g_mime_text_converter_convert (converter, GMIME_STREAM (reader), GMIME_STREAM (writer), &error));
	g_assert_no_error (error);
	g_assert_cmpmem (writer->written->data, writer->written->len, expected, strlen (expected));
	g_assert_cmpuint (reader->closes, ==, 0);
	g_assert_cmpuint (writer->closes, ==, 0);
	g_object_unref (reader);
	g_object_unref (writer);
}

static void
test_writer (void)
{
	GMimeStream *stream = g_mime_stream_mem_new ();
	GMimeHtmlWriter *writer = g_mime_html_writer_new (stream);
	GMimeHtmlAttribute *attribute;
	GError *error = NULL;
	GByteArray *bytes = g_mime_stream_mem_get_byte_array (GMIME_STREAM_MEM (stream));

	g_assert_false (g_mime_html_writer_write_attribute (writer, "href", "x", &error));
	g_assert_error (error, GMIME_ERROR, GMIME_ERROR_INVALID_OPERATION);
	g_clear_error (&error);
	g_assert_false (g_mime_html_writer_write_attribute_value (writer, "x", &error));
	g_assert_error (error, GMIME_ERROR, GMIME_ERROR_INVALID_OPERATION);
	g_clear_error (&error);
	g_assert_false (g_mime_html_writer_write_start_tag (writer, "a b", &error));
	g_clear_error (&error);
	g_assert_true (g_mime_html_writer_write_start_tag (writer, "a", &error));
	g_assert_cmpint (g_mime_html_writer_get_state (writer), ==, GMIME_HTML_WRITER_STATE_TAG);
	g_assert_false (g_mime_html_writer_write_attribute_name (writer, "=bad", &error));
	g_clear_error (&error);
	g_assert_true (g_mime_html_writer_write_attribute_name (writer, "href", &error));
	g_assert_cmpint (g_mime_html_writer_get_state (writer), ==, GMIME_HTML_WRITER_STATE_ATTRIBUTE);
	attribute = g_mime_html_attribute_new ("=javascript:x", NULL);
	g_assert_true (g_mime_html_writer_write_html_attribute (writer, attribute, &error));
	g_mime_html_attribute_unref (attribute);
	g_assert_true (g_mime_html_writer_write_text (writer, "<&\"'\xf0\x9f\x98\xb1", -1, &error));
	g_assert_true (g_mime_html_writer_write_end_tag (writer, "a", &error));
	g_assert_true (g_mime_html_writer_write_empty_element_tag (writer, "br", &error));
	g_assert_true (g_mime_html_writer_flush (writer, &error));
	g_assert_no_error (error);
	g_test_message ("writer output: %.*s", (int) bytes->len, bytes->data);
	g_assert_cmpmem (bytes->data, bytes->len, "<a href=\"\" =javascript:x>&lt;&amp;&quot;&#39;&#128561;</a><br/>",
			 strlen ("<a href=\"\" =javascript:x>&lt;&amp;&quot;&#39;&#128561;</a><br/>"));
	g_object_unref (writer);
	g_assert_cmpint (g_mime_stream_write_string (stream, "open"), ==, 4);
	g_object_unref (stream);
}

static void
test_plain_flowed (void)
{
	GMimeTextConverter *plain = g_mime_text_to_html_new (), *flowed = g_mime_flowed_to_html_new ();

	assert_conversion (plain, "This is the html body", "<html><body>This is the html body<br/></body></html>");
	g_mime_text_to_html_set_output_html_fragment (GMIME_TEXT_TO_HTML (plain), TRUE);
	assert_conversion (plain, "> one\r\n> > two\rthree\n",
			   "<blockquote>one<br/><blockquote>two<br/></blockquote></blockquote>three<br/>");
	assert_conversion (plain, "\xf0\x9f\x98\xb1", "&#128561;<br/>");
	assert_conversion (plain, "", "");
	g_mime_text_converter_set_header (plain, "header &");
	g_mime_text_converter_set_footer (plain, "<b>footer</b>");
	g_mime_text_to_html_set_footer_format (GMIME_TEXT_TO_HTML (plain), GMIME_HEADER_FOOTER_FORMAT_HTML);
	assert_conversion (plain, "body", "header &amp;<br/>body<br/><b>footer</b>");
	g_mime_flowed_to_html_set_output_html_fragment (GMIME_FLOWED_TO_HTML (flowed), TRUE);
	assert_conversion (flowed, "a \r\nb\r\n\r\n> quote \n>> deeper\n From here\n",
			   "<p>a b</p>\n<br/>\n<blockquote><p>quote </p>\n<blockquote><p>deeper</p>\n</blockquote></blockquote><p>From here</p>\n");
	g_mime_flowed_to_html_set_delete_space (GMIME_FLOWED_TO_HTML (flowed), TRUE);
	assert_conversion (flowed, "a \nb \nc", "<p>abc</p>\n");
	assert_conversion (flowed, "a ", "<p>a</p>\n");
	g_object_unref (plain);
	g_object_unref (flowed);
}

static const struct {
	const char *input, *expected;
} enriched_cases[] = {
	{ "", "" },
	{ "plain", "<div>plain</div>" },
	{ "a  b\tc", "<div>a  b\tc</div>" },
	{ "a & b << c > d", "<div>a &amp; b &lt; c &gt; d</div>" },
	{ "&lt;b&gt;", "<div>&amp;lt;b&amp;gt;</div>" },
	{ "<<script>alert(1)<</script>", "<div>&lt;script&gt;alert(1)&lt;/script&gt;</div>" },
	{ "a<bold>b</bold>c", "<div>a<span style=\"font-weight: bold;\">b</span>c</div>" },
	{ "<BoLd><ITALIC>x</italic></bOlD>",
	  "<div><span style=\"font-weight: bold; font-style: italic;\">x</span></div>" },
	{ "<underline>x</underline>", "<div><span style=\"text-decoration: underline;\">x</span></div>" },
	{ "<fixed>x</fixed>", "<div><span style=\"font-family: monospace;\">x</span></div>" },
	{ "<bold>a<bold>b</bold>c</bold>",
	  "<div><span style=\"font-weight: bold;\">a</span><span style=\"font-weight: bold;\">b</span><span style=\"font-weight: bold;\">c</span></div>" },
	{ "<bigger>x</bigger>", "<div><span style=\"font-size: 1.167em;\">x</span></div>" },
	{ "<smaller>x</smaller>", "<div><span style=\"font-size: 0.833em;\">x</span></div>" },
	{ "<bigger><smaller>x</smaller></bigger>", "<div>x</div>" },
	{ "a\nb", "<div>a b</div>" },
	{ "a\r\nb", "<div>a b</div>" },
	{ "a\rb", "<div>a b</div>" },
	{ "a\n\nb", "<div>a</div><div>b</div>" },
	{ "a\n\n\nb", "<div>a</div><div><br/></div><div>b</div>" },
	{ "a\r\r\nb", "<div>a</div><div>b</div>" },
	{ "\n\nx", "<div><br/></div><div>x</div>" },
	{ "x\n", "<div>x </div>" },
	{ "x\n\n", "<div>x</div><div><br/></div>" },
	{ "\n", "<div> </div>" },
	{ "a\n \nb", "<div>a   b</div>" },
	{ "<nofill>a\nb</nofill>", "<div style=\"white-space: pre;\">a</div><div style=\"white-space: pre;\">b</div>" },
	{ "<nofill>a\r\n\r\nb</nofill>",
	  "<div style=\"white-space: pre;\">a</div><div style=\"white-space: pre;\"><br/></div><div style=\"white-space: pre;\">b</div>" },
	{ "a<nofill>b</nofill>c", "<div>a</div><div style=\"white-space: pre;\">b</div><div>c</div>" },
	{ "a<center>b</center>c", "<div>a</div><div style=\"text-align: center;\">b</div><div>c</div>" },
	{ "<center>a</center>\n\nb", "<div style=\"text-align: center;\">a</div><div>b</div>" },
	{ "a<center>\n\nb</center>c", "<div>a</div><div style=\"text-align: center;\">b</div><div>c</div>" },
	{ "<center>a</center><bold></bold>\n\nb", "<div style=\"text-align: center;\">a</div><div>b</div>" },
	{ "<nofill>a\n</nofill>",
	  "<div style=\"white-space: pre;\">a</div><div style=\"white-space: pre;\"><br/></div>" },
	{ "<flushboth><nofill>x</nofill></flushboth>", "<div style=\"white-space: pre; text-align: left;\">x</div>" },
	{ "<center>a<flushright>b</flushright>c</center>",
	  "<div style=\"text-align: center;\">a</div><div style=\"text-align: right;\">b</div><div style=\"text-align: center;\">c</div>" },
	{ "<excerpt>a<excerpt>b</excerpt>c</excerpt>",
	  "<div style=\"margin-left: 4ch;\">a</div><div style=\"margin-left: 8ch;\">b</div><div style=\"margin-left: 4ch;\">c</div>" },
	{ "<paraindent><param>Left, RIGHT, in</param>x\n\ny</paraindent>",
	  "<div style=\"margin-left: 4ch; margin-right: 4ch; text-indent: 4ch;\">x</div><div style=\"margin-left: 4ch; margin-right: 4ch; text-indent: 4ch;\">y</div>" },
	{ "<paraindent><param>left,left,out</param>x</paraindent>",
	  "<div style=\"margin-left: 12ch; text-indent: -4ch;\">x</div>" },
	{ "<paraindent><param>in,out</param>x</paraindent>", "<div>x</div>" },
	{ "<paraindent><param>left,evil</param>x</paraindent>", "<div>x</div>" },
	{ "<color><param> ReD </param>x</color>", "<div><span style=\"color: red;\">x</span></div>" },
	{ "<color><param>FFFF,8000,0000</param>x</color>", "<div><span style=\"color: #ff8000;\">x</span></div>" },
	{ "<color><param>blue</param>a<color><param>white</param>b</color>c</color>",
	  "<div><span style=\"color: blue;\">a</span><span style=\"color: white;\">b</span><span style=\"color: blue;\">c</span></div>" },
	{ "<fontfamily><param>Times New Roman</param>x</fontfamily>",
	  "<div><span style=\"font-family: 'Times New Roman';\">x</span></div>" },
	{ "<lang><param>es-419</param>x</lang>", "<div><span lang=\"es-419\">x</span></div>" },
	{ "<x-new><param>hidden</param>visible</x-new>", "<div>visible</div>" },
	{ "<bold><italic>x</bold>y</italic>",
	  "<div><span style=\"font-weight: bold; font-style: italic;\">x</span>y</div>" },
	{ "<bold>x", "<div><span style=\"font-weight: bold;\">x</span></div>" },
	{ "<param>hidden", "" },
	{ "a<param>hidden</param>b", "<div>ab</div>" },
	{ "<color><param>red<param>hidden</param>more</param>x</color>", "<div>x</div>" },
	{ "<color><param><bold>red</bold></param>x</color>", "<div>x</div>" },
	{ "<color><param>blue</param><color><param>invalid</param>x</color></color>",
	  "<div><span style=\"color: blue;\">x</span></div>" },
	{ "<color> <param>red</param>x</color>", "<div> x</div>" },
	{ "<bold", "<div>&lt;bold</div>" },
	{ "<", "<div>&lt;</div>" },
	{ "</", "<div>&lt;/</div>" },
	{ "<>", "<div>&lt;&gt;</div>" },
	{ "<bold attr=x>visible", "<div>&lt;bold attr=x&gt;visible</div>" }
};

static void
test_enriched (void)
{
	static const struct {
		const char *command, *value;
	} invalid[] = { { "color", "red; background: url(https://example.com)" },
			{ "color", "ffff,0000,000g" },
			{ "color", "#ff0000" },
			{ "fontfamily", "Arial';color:red;/*" },
			{ "fontfamily", "Arial&quot; onclick=&quot;evil" },
			{ "fontfamily", "url(https://example.com)" },
			{ "fontfamily", "Arial\\27" },
			{ "lang", "en\" onclick=\"evil" },
			{ "lang", "-en" },
			{ "lang", "en-" },
			{ "lang", "en--US" },
			{ "lang", "123" },
			{ "lang", "toolonglanguage" },
			{ "paraindent", "left; color:red" } };

	GMimeTextConverter *converter = g_mime_enriched_to_html_new ();

	g_mime_enriched_to_html_set_output_html_fragment (GMIME_ENRICHED_TO_HTML (converter), TRUE);
	for (guint i = 0; i < G_N_ELEMENTS (enriched_cases); i++) {
		char *expected = g_strconcat ("<div style=\"white-space: pre-wrap;\">", enriched_cases[i].expected,
					      "</div>", NULL);

		g_test_message ("enriched case %u: %s", i, enriched_cases[i].input);
		assert_conversion (converter, enriched_cases[i].input, expected);
		g_free (expected);
	}

	for (guint i = 0; i < G_N_ELEMENTS (invalid); i++) {
		char *input = g_strdup_printf ("<%s><param>%s</param>x</%s>", invalid[i].command, invalid[i].value,
					       invalid[i].command);

		assert_conversion (converter, input, "<div style=\"white-space: pre-wrap;\"><div>x</div></div>");
		g_free (input);
	}

	assert_conversion (converter,
			   "<bold>Now</bold> is the time for <italic>all</italic>\n"
			   "good men\n<smaller>(and <<women>)</smaller> to\n<ignoreme>come</ignoreme>\n\n"
			   "to the aid of their\n\n\n<color><param>red</param>beloved</color>\ncountry.\n\n"
			   "By the way,\nI think that <paraindent><param>left</param><<smaller>\n\n"
			   "</paraindent>should REALLY be called\n\n"
			   "<paraindent><param>left</param><<tinier></paraindent>\n\n"
			   "and that I am always right.\n\n-- the end",
			   "<div style=\"white-space: pre-wrap;\"><div><span style=\"font-weight: bold;\">Now</span> is the time for "
			   "<span style=\"font-style: italic;\">all</span> good men "
			   "<span style=\"font-size: 0.833em;\">(and &lt;women&gt;)</span> to come</div>"
			   "<div>to the aid of their</div><div><br/></div>"
			   "<div><span style=\"color: red;\">beloved</span> country.</div>"
			   "<div>By the way, I think that </div>"
			   "<div style=\"margin-left: 4ch;\">&lt;smaller&gt;</div>"
			   "<div>should REALLY be called</div>"
			   "<div style=\"margin-left: 4ch;\">&lt;tinier&gt;</div>"
			   "<div>and that I am always right.</div><div>-- the end</div></div>");
	g_object_unref (converter);
}

static void
assert_limit (GMimeTextConverter *converter, const char *input)
{
	GError *error = NULL;
	char *result = g_mime_text_converter_convert_to_string (converter, input, -1, &error);

	g_assert_null (result);
	g_assert_error (error, GMIME_ERROR, GMIME_ERROR_PARSE_ERROR);
	g_clear_error (&error);
}

static void
test_enriched_limits (void)
{
	GMimeTextConverter *converter = g_mime_enriched_to_html_new ();
	GMimeEnrichedToHtml *enriched = GMIME_ENRICHED_TO_HTML (converter);

	g_mime_enriched_to_html_set_max_element_depth (enriched, 2);
	assert_limit (converter, "<bold><italic><underline>x");
	assert_limit (converter, "<bold><param><param>x");
	g_mime_enriched_to_html_set_max_parameter_length (enriched, 3);
	assert_limit (converter, "<param>1234</param>");
	assert_limit (converter, "<param><xx></param>");
	g_mime_enriched_to_html_set_max_input_length (enriched, 1);
	assert_limit (converter, "ab");
	assert_conversion (converter, "\xf0\x9f\x98\xb1",
			   "<html><body><div style=\"white-space: pre-wrap;\"><div>&#128561;</div></div></body></html>");
	g_mime_enriched_to_html_set_max_output_length (enriched, 1);
	assert_limit (converter, "");
	g_mime_enriched_to_html_set_output_html_fragment (enriched, TRUE);
	g_mime_enriched_to_html_set_max_output_length (enriched,
						       strlen ("<div style=\"white-space: pre-wrap;\"></div>"));
	assert_conversion (converter, "", "<div style=\"white-space: pre-wrap;\"></div>");
	g_mime_text_converter_set_footer (converter, "x");
	assert_limit (converter, "");
	g_object_unref (converter);

	converter = g_mime_enriched_to_html_new ();
	enriched = GMIME_ENRICHED_TO_HTML (converter);
	char *large = g_strnfill (20000, '&');
	OutputTestStream *reader = g_object_new (output_test_stream_get_type (), NULL);
	OutputTestStream *writer = g_object_new (output_test_stream_get_type (), NULL);
	GError *error = NULL;

	reader->text = large;
	reader->length = strlen (large);
	g_mime_enriched_to_html_set_max_output_length (enriched, 64);
	g_assert_false (
		g_mime_text_converter_convert (converter, GMIME_STREAM (reader), GMIME_STREAM (writer), &error));
	g_assert_error (error, GMIME_ERROR, GMIME_ERROR_PARSE_ERROR);
	g_clear_error (&error);
	g_assert_cmpuint (reader->position, <, reader->length);
	g_assert_cmpuint (writer->written->len, <=, 64);
	g_mime_enriched_to_html_set_max_output_length (enriched, 4096);
	g_mime_enriched_to_html_set_header_format (enriched, GMIME_HEADER_FOOTER_FORMAT_HTML);
	g_mime_text_converter_set_header (converter, "\xc3\xa9");
	g_assert_true (g_mime_text_converter_set_output_charset (converter, "us-ascii", &error));
	g_assert_no_error (error);
	reader->position = 0;
	reader->length = 0;
	g_assert_false (
		g_mime_text_converter_convert (converter, GMIME_STREAM (reader), GMIME_STREAM (writer), &error));
	g_assert_error (error, G_CONVERT_ERROR, G_CONVERT_ERROR_ILLEGAL_SEQUENCE);
	g_clear_error (&error);
	g_assert_cmpuint (reader->closes, ==, 0);
	g_assert_cmpuint (writer->closes, ==, 0);
	g_free (large);
	g_object_unref (reader);
	g_object_unref (writer);
	g_object_unref (converter);
}

static const struct {
	const char *input, *url, *prefix;
} url_cases[] = { { "a@[1.2.3.4]", "a@[1.2.3.4]", "mailto:" },
		  { "mailto:a@[1.2.3.4]", "mailto:a@[1.2.3.4]", "" },
		  { "a@[IPv6:::]", "a@[IPv6:::]", "mailto:" },
		  { "(http://example.com/Foo_(bar))", "http://example.com/Foo_(bar)", "" },
		  { "[http://example.com/a[0]]", "http://example.com/a[0]", "" },
		  { "{http://example.com/a{0}}", "http://example.com/a{0}", "" },
		  { "(file:///Foo_(bar))", "file:///Foo_(bar)", "" },
		  { "(mailto:a@example.com?subject=(hi))", "mailto:a@example.com?subject=(hi)", "" },
		  { "http://example.com#section", "http://example.com#section", "" },
		  { "http://example.com?_=1", "http://example.com?_=1", "" },
		  { "http://example.com?%78=1", "http://example.com?%78=1", "" },
		  { "http://example.com?x=1&_=2", "http://example.com?x=1&_=2", "" },
		  { "a@[IPv6:1:2:3:4:5:6:7:]", NULL, NULL },
		  { "mailto:a@[IPv6:1:2:3:4:5:6:7:]", NULL, NULL },
		  { "user@example.xn--p1ai", "user@example.xn--p1ai", "mailto:" },
		  { "user@example.xn--a", NULL, NULL },
		  { "http://[::1]/", "http://[::1]/", "" },
		  { "http://user:pass@example.com/path", "http://user:pass@example.com/path", "" },
		  { "http://u%3aser:p%2Fss@example.com/", "http://u%3aser:p%2Fss@example.com/", "" },
		  { "http://example.com?_=1\xc2\xa0next", "http://example.com?_=1", "" },
		  { "http://example.com#section\xe2\x80\x8bnext", "http://example.com#section", "" },
		  { "first @ broken then www.example.com.", "www.example.com", "http://" },
		  { "foo@bar", NULL, NULL },
		  { "mailto:foo@bar", "mailto:foo@bar", "" },
		  { "http://example.com#", "http://example.com#", "" },
		  { "http://example.com?x=#", "http://example.com?x=#", "" },
		  { "http://example.com?&x=1&&y=", "http://example.com?&x=1&&y=", "" },
		  { "http://example.com?x=a?b&y=/c?d", "http://example.com?x=a?b&y=/c?d", "" },
		  { "http://example.com:00099999#f", "http://example.com:00099999#f", "" },
		  { "(http://example.com/a(b)c).", "http://example.com/a(b)c", "" },
		  { "www.example.com#part_(one)", "www.example.com#part_(one)", "http://" },
		  { "ftp.example.com#section", "ftp.example.com#section", "ftp://" },
		  { "file:///a({b}[c])", "file:///a({b}[c])", "" },
		  { "http://user:pass%21@[::1]:080/path?x=1&_=2#f", "http://user:pass%21@[::1]:080/path?x=1&_=2#f",
		    "" },
		  { "http://@example.com/", "http://@example.com/", "" },
		  { "http://us\303\251r@b\303\274cher.example/", "http://us\303\251r@b\303\274cher.example/", "" },
		  { "http://example.com/path@elsewhere.test", "http://example.com/path@elsewhere.test", "" },
		  { "http://[::]/", "http://[::]/", "" },
		  { "http://[1:2:3:4:5:6::8]/", "http://[1:2:3:4:5:6::8]/", "" },
		  { "http://[::ffff:192.0.2.1]/", "http://[::ffff:192.0.2.1]/", "" },
		  { "mailto:a@[IPv6:::]", "mailto:a@[IPv6:::]", "" },
		  { "mailto:?subject=hi", "mailto:?subject=hi", "" },
		  { "mailto:\"a b\"@example.com?subject=hi", "mailto:\"a b\"@example.com?subject=hi", "" },
		  { "a@[001.002.003.004]", "a@[001.002.003.004]", "mailto:" },
		  { "a@[IPv6:1:2:3:4:5:6::]", "a@[IPv6:1:2:3:4:5:6::]", "mailto:" },
		  { "\"a b\"@example.com", "\"a b\"@example.com", "mailto:" },
		  { "user@EXAMPLE.XN--P1AI", "user@EXAMPLE.XN--P1AI", "mailto:" },
		  { "user@example.\344\270\255\345\233\275", "user@example.\344\270\255\345\233\275", "mailto:" },
		  { "[http://[::1]/a[0]]", "http://[::1]/a[0]", "" },
		  { "{http://example.com/a({b}[c])}.", "http://example.com/a({b}[c])", "" },
		  { "|file:///a|", "file:///a", "" },
		  { "See <http://example.com?_=1&x=\"y\"#f> & done.", "http://example.com?_=1&x=\"y\"#f", "" },
		  { "http://example.com?_=1\342\200\256hidden", "http://example.com?_=1", "" },
		  { "http://example.com?!", "http://example.com", "" },
		  { "@", NULL, NULL },
		  { "http://", NULL, NULL },
		  { "file://", NULL, NULL },
		  { "mailto:", NULL, NULL },
		  { "mailto:a@", NULL, NULL },
		  { "mailto:a@-host", NULL, NULL },
		  { "http://[1:2:3:4:5:6:7:]/", NULL, NULL },
		  { "http://[1::2::3]/", NULL, NULL },
		  { "http://[fffff::1]/", NULL, NULL },
		  { "http://[::ffff:192.0.2.1234]/", NULL, NULL },
		  { "http://[::ffff:192.000.2.1]/", NULL, NULL },
		  { "http://[IPv6:::1]/", NULL, NULL },
		  { "http://[v1.future]/", NULL, NULL },
		  { "http://[127.0.0.1]/", NULL, NULL },
		  { "http://[fe80::1%25eth0]/", NULL, NULL },
		  { "a@[256.1.2.3]", NULL, NULL },
		  { "a@[1.2.3.4444]", NULL, NULL },
		  { "a@[1.2.3.4.5]", NULL, NULL },
		  { "a@[IPv6:1:2:3:4:5:6::8]", NULL, NULL },
		  { "user@example.xn--abc-", NULL, NULL },
		  { "user@example.xn--p1ai-", NULL, NULL },
		  { "user@example.c", NULL, NULL },
		  { "user@example.123", NULL, NULL },
		  { "a@[1.2.3.4", NULL, NULL },
		  { "a@[IPv6:::", NULL, NULL },
		  { "http://[::1", NULL, NULL } };

static void
test_url_scan (void)
{
	for (guint i = 0; i < G_N_ELEMENTS (url_cases); i++) {
		GMimeUrlMatch match = { 17, 29, "unchanged" };
		gboolean found = g_mime_url_scan (url_cases[i].input, 0, strlen (url_cases[i].input), &match);

		g_test_message ("URL scan case %u: %s", i, url_cases[i].input);
		if (url_cases[i].url) {
			gsize start = strstr (url_cases[i].input, url_cases[i].url) - url_cases[i].input;

			g_assert_true (found);
			g_assert_cmpuint (match.start, ==, start);
			g_assert_cmpuint (match.end, ==, start + strlen (url_cases[i].url));
			g_assert_cmpstr (match.prefix, ==, url_cases[i].prefix);
		} else {
			g_assert_false (found);
			g_assert_cmpuint (match.start, ==, 17);
			g_assert_cmpuint (match.end, ==, 29);
			g_assert_cmpstr (match.prefix, ==, "unchanged");
		}
	}
}

static void
test_url_scan_bounds (void)
{
	static const struct {
		const char *text;
		gsize length;
	} truncated[] = { { "a@[1.2.3.4]outside", 10 }, { "a@[IPv6:::]outside", 10 }, { "http://[::1]outside", 11 } };

	const char *text = "\303\251 (www.example.com), a@example.com!";
	const char embedded[] = "www.example.com\0a@example.com";
	const char *unicode = "a@b\303\274cher.example";
	GMimeUrlMatch match;
	gsize length = strlen (text), offset;
	char *buffer;

	g_assert_true (g_mime_url_scan (text, 0, length, &match));
	g_assert_cmpuint (match.start, ==, strstr (text, "www.") - text);
	g_assert_cmpuint (match.end, ==, match.start + strlen ("www.example.com"));
	g_assert_cmpstr (match.prefix, ==, "http://");
	offset = match.end;
	g_assert_true (g_mime_url_scan (text, offset, length - offset, &match));
	g_assert_cmpuint (match.start, ==, strstr (text, "a@") - text);
	g_assert_cmpuint (match.end, ==, length - 1);
	g_assert_cmpstr (match.prefix, ==, "mailto:");
	offset = match.end;
	g_assert_false (g_mime_url_scan (text, offset, length - offset, &match));
	g_assert_cmpuint (match.end, ==, offset);
	g_assert_cmpstr (match.prefix, ==, "mailto:");

	offset = strchr (text, '@') - text;
	g_assert_false (g_mime_url_scan (text, offset, length - offset, &match));
	g_assert_false (g_mime_url_scan (text, length, 0, &match));
	g_assert_false (g_mime_url_scan ("", 0, 0, &match));

	for (guint i = 0; i < G_N_ELEMENTS (truncated); i++) {
		buffer = g_malloc (truncated[i].length);
		memcpy (buffer, truncated[i].text, truncated[i].length);
		g_assert_false (g_mime_url_scan (buffer, 0, truncated[i].length, &match));
		g_free (buffer);
	}

	length = sizeof (embedded) - 1;
	buffer = g_malloc (length);
	memcpy (buffer, embedded, length);
	g_assert_true (g_mime_url_scan (buffer, 0, length, &match));
	g_assert_cmpuint (match.start, ==, 0);
	g_assert_cmpuint (match.end, ==, strlen ("www.example.com"));
	g_assert_cmpstr (match.prefix, ==, "http://");
	offset = match.end;
	g_assert_true (g_mime_url_scan (buffer, offset, length - offset, &match));
	g_assert_cmpuint (match.start, ==, offset + 1);
	g_assert_cmpuint (match.end, ==, length);
	g_assert_cmpstr (match.prefix, ==, "mailto:");
	g_free (buffer);

	length = strlen (unicode);
	buffer = g_malloc (length);
	memcpy (buffer, unicode, length);
	g_assert_true (g_mime_url_scan (buffer, 0, length, &match));
	g_assert_cmpuint (match.start, ==, 0);
	g_assert_cmpuint (match.end, ==, length);
	g_assert_cmpstr (match.prefix, ==, "mailto:");
	g_free (buffer);
}

static void
test_url_scan_preconditions (void)
{
	GMimeUrlMatch match;

	g_test_expect_message ("gmime", G_LOG_LEVEL_CRITICAL, "*text != NULL*");
	g_assert_false (g_mime_url_scan (NULL, 0, 0, &match));
	g_test_assert_expected_messages ();
	g_test_expect_message ("gmime", G_LOG_LEVEL_CRITICAL, "*match != NULL*");
	g_assert_false (g_mime_url_scan ("", 0, 0, NULL));
	g_test_assert_expected_messages ();
	g_test_expect_message ("gmime", G_LOG_LEVEL_CRITICAL, "*length <= G_MAXSIZE - start*");
	g_assert_false (g_mime_url_scan ("", G_MAXSIZE, 1, &match));
	g_test_assert_expected_messages ();
}

static void
assert_url_conversion (GMimeTextConverter *converter, const char *input, const char *url, const char *prefix)
{
	char *expected;

	if (url) {
		const char *at = strstr (input, url);
		char *before = g_strndup (input, at - input);
		char *encoded_before = g_mime_html_encode (before);
		char *after = g_mime_html_encode (at + strlen (url));
		char *encoded_url = g_mime_html_encode (url);
		char *href = g_strconcat (prefix, url, NULL);
		char *attribute = g_mime_html_attribute_encode (href);

		expected =
			g_strdup_printf ("%s<a href=%s>%s</a>%s<br/>", encoded_before, attribute, encoded_url, after);
		g_free (before);
		g_free (encoded_before);
		g_free (after);
		g_free (encoded_url);
		g_free (href);
		g_free (attribute);
	} else {
		char *encoded = g_mime_html_encode (input);

		expected = g_strconcat (encoded, "<br/>", NULL);
		g_free (encoded);
	}

	assert_conversion (converter, input, expected);
	g_free (expected);
}

static void
test_urls (void)
{
	GMimeTextConverter *converter = g_mime_text_to_html_new ();

	g_mime_text_to_html_set_output_html_fragment (GMIME_TEXT_TO_HTML (converter), TRUE);
	for (guint i = 0; i < G_N_ELEMENTS (url_cases); i++) {
		g_test_message ("URL case %u: %s", i, url_cases[i].input);
		assert_url_conversion (converter, url_cases[i].input, url_cases[i].url, url_cases[i].prefix);
	}

	assert_conversion (converter, "See www.example.com & a@example.com.",
			   "See <a href=\"http://www.example.com\">www.example.com</a> &amp; <a href=\"mailto:a@example.com\">a@example.com</a>.<br/>");
	assert_conversion (converter, "http://user%xx@example.com/",
			   "<a href=\"http://user\">http://user</a><a href=\"mailto:%xx@example.com\">%xx@example.com</a>/<br/>");
	assert_conversion (converter, "Bad http://[1:2:3:4:5:6:7:] then www.example.com#f and a@[1.2.3.4]",
			   "Bad http://[1:2:3:4:5:6:7:] then <a href=\"http://www.example.com#f\">www.example.com#f</a>"
			   " and <a href=\"mailto:a@[1.2.3.4]\">a@[1.2.3.4]</a><br/>");
	g_object_unref (converter);
}

static void
test_url_limits (void)
{
	GMimeTextConverter *converter = g_mime_text_to_html_new ();
	char *label = g_strnfill (63, 'a');
	char *host = g_strdup_printf ("%s.%s.%s.%.*s.com", label, label, label, 59, label);
	char *local = g_strnfill (64, 'a');
	char *input;
	GString *text;

	g_mime_text_to_html_set_output_html_fragment (GMIME_TEXT_TO_HTML (converter), TRUE);

	input = g_strdup_printf ("http://%s#f", host);
	assert_url_conversion (converter, input, input, "");
	g_free (input);
	input = g_strdup_printf ("http://%sa", host);
	assert_url_conversion (converter, input, NULL, NULL);
	g_free (input);
	input = g_strdup_printf ("user@example.%sa", label);
	assert_url_conversion (converter, input, NULL, NULL);
	g_free (input);

	input = g_strdup_printf ("%s@example.com", local);
	assert_url_conversion (converter, input, input, "mailto:");
	g_free (input);
	input = g_strdup_printf ("%sa@example.com", local);
	assert_url_conversion (converter, input, NULL, NULL);
	g_free (input);

	text = g_string_new ("user@example.");
	for (guint i = 0; i < 57; i++)
		g_string_append (text, "\303\251");
	assert_url_conversion (converter, text->str, text->str, "mailto:");
	g_string_free (text, TRUE);

	text = g_string_new (NULL);
	for (guint i = 0; i < 5000; i++)
		g_string_append (text, "www.");
	g_string_append (text, local);
	g_string_append (text, " http://example.com#f");
	assert_url_conversion (converter, text->str, "http://example.com#f", "");
	g_string_free (text, TRUE);
	g_free (label);
	g_free (host);
	g_free (local);
	g_object_unref (converter);
}

static gboolean
custom_blockquote_callback (GMimeHtmlTagContext *context, GMimeHtmlWriter *writer, gpointer user_data, GError **error)
{
	guint *depth = user_data;
	char class_name[32];

	if (g_mime_html_tag_context_get_tag_id (context) != GMIME_HTML_TAG_BLOCK_QUOTE)
		return g_mime_html_tag_context_write_tag (context, writer, TRUE, error);

	if (g_mime_html_tag_context_get_is_end_tag (context)) {
		g_assert_cmpuint (*depth, >, 0);
		if (!g_mime_html_tag_context_write_tag (context, writer, FALSE, error))
			return FALSE;

		(*depth)--;
		return TRUE;
	}

	g_mime_html_tag_context_set_invoke_callback_for_end_tag (context, TRUE);
	g_snprintf (class_name, sizeof (class_name), "quote level_%03u", ++(*depth));

	return g_mime_html_tag_context_write_tag (context, writer, TRUE, error) &&
	       g_mime_html_writer_write_attribute (writer, "class", class_name, error);
}

static void
test_custom_blockquotes (void)
{
	static const struct {
		const char *input, *plain, *flowed;
	} cases[] = {
		{ "Intro\n> outer & text\n>> www.example.com\n> outer again\nBack\n>> tail",
		  "Intro<br/><blockquote class=\"quote level_001\">outer &amp; text<br/>"
		  "<blockquote class=\"quote level_002\"><a href=\"http://www.example.com\">www.example.com</a><br/>"
		  "</blockquote>outer again<br/></blockquote>Back<br/>"
		  "<blockquote class=\"quote level_001\"><blockquote class=\"quote level_002\">tail<br/>"
		  "</blockquote></blockquote>",
		  "<p>Intro</p>\n<blockquote class=\"quote level_001\"><p>outer &amp; text</p>\n"
		  "<blockquote class=\"quote level_002\"><p><a href=\"http://www.example.com\">www.example.com</a></p>\n"
		  "</blockquote><p>outer again</p>\n</blockquote><p>Back</p>\n"
		  "<blockquote class=\"quote level_001\"><blockquote class=\"quote level_002\"><p>tail</p>\n"
		  "</blockquote></blockquote>" },
		{ ">\n>>\n",
		  "<blockquote class=\"quote level_001\"><br/><blockquote class=\"quote level_002\"><br/>"
		  "</blockquote></blockquote>",
		  "<blockquote class=\"quote level_001\"><br/>\n<blockquote class=\"quote level_002\"><br/>\n"
		  "</blockquote></blockquote>" }
	};

	GMimeTextConverter *plain = g_mime_text_to_html_new ();
	GMimeTextConverter *flowed = g_mime_flowed_to_html_new ();
	guint plain_depth = 0, flowed_depth = 0;

	g_mime_text_to_html_set_output_html_fragment (GMIME_TEXT_TO_HTML (plain), TRUE);
	g_mime_text_to_html_set_html_tag_callback (GMIME_TEXT_TO_HTML (plain), custom_blockquote_callback, &plain_depth,
						   NULL);
	g_mime_flowed_to_html_set_output_html_fragment (GMIME_FLOWED_TO_HTML (flowed), TRUE);
	g_mime_flowed_to_html_set_html_tag_callback (GMIME_FLOWED_TO_HTML (flowed), custom_blockquote_callback,
						     &flowed_depth, NULL);

	for (guint i = 0; i < G_N_ELEMENTS (cases); i++) {
		assert_conversion (plain, cases[i].input, cases[i].plain);
		g_assert_cmpuint (plain_depth, ==, 0);
		assert_conversion (flowed, cases[i].input, cases[i].flowed);
		g_assert_cmpuint (flowed_depth, ==, 0);
	}

	g_object_unref (plain);
	g_object_unref (flowed);
}

typedef struct {
	guint calls, ends, destroyed;
	gboolean fail;
} CallbackState;

static gboolean
callback (GMimeHtmlTagContext *context, GMimeHtmlWriter *writer, gpointer user_data, GError **error)
{
	CallbackState *state = user_data;

	state->calls++;
	if (state->fail) {
		g_set_error_literal (error, GMIME_ERROR, EACCES, "callback failure");
		return FALSE;
	}

	if (g_mime_html_tag_context_get_is_end_tag (context))
		state->ends++;

	if (g_mime_html_tag_context_get_tag_id (context) == GMIME_HTML_TAG_BLOCK_QUOTE) {
		g_mime_html_tag_context_set_suppress_inner_content (context, TRUE);
		g_mime_html_tag_context_set_delete_tag (context, TRUE);
		g_mime_html_tag_context_set_delete_end_tag (context, TRUE);
		return TRUE;
	}

	g_mime_html_tag_context_set_invoke_callback_for_end_tag (context, TRUE);
	return g_mime_html_tag_context_write_tag (context, writer, TRUE, error);
}

static void
callback_destroy (gpointer data)
{
	((CallbackState *) data)->destroyed++;
}

static void
test_callbacks_errors (void)
{
	GMimeTextConverter *converter = g_mime_text_to_html_new ();
	CallbackState state = { 0 };
	GError *error = NULL;
	OutputTestStream *reader = g_object_new (output_test_stream_get_type (), NULL);
	OutputTestStream *writer = g_object_new (output_test_stream_get_type (), NULL);

	g_mime_text_to_html_set_output_html_fragment (GMIME_TEXT_TO_HTML (converter), TRUE);
	g_mime_text_to_html_set_html_tag_callback (GMIME_TEXT_TO_HTML (converter), callback, &state, callback_destroy);
	assert_conversion (converter, "> hidden\n>> nested\nwww.example.com",
			   "<a href=\"http://www.example.com\">www.example.com</a><br/>");
	g_assert_cmpuint (state.ends, ==, 2);
	state.fail = TRUE;
	g_assert_null (g_mime_text_converter_convert_to_string (converter, "x", -1, &error));
	g_assert_error (error, GMIME_ERROR, EACCES);
	g_clear_error (&error);
	state.fail = FALSE;
	assert_conversion (converter, "x", "x<br/>");
	g_mime_text_to_html_set_html_tag_callback (GMIME_TEXT_TO_HTML (converter), NULL, NULL, NULL);
	g_assert_cmpuint (state.destroyed, ==, 1);
	reader->text = "x";
	reader->length = 1;
	reader->fail_read = TRUE;
	g_assert_false (
		g_mime_text_converter_convert (converter, GMIME_STREAM (reader), GMIME_STREAM (writer), &error));
	g_assert_error (error, GMIME_ERROR, EIO);
	g_clear_error (&error);
	reader->fail_read = FALSE;
	writer->fail_write = TRUE;
	g_assert_false (
		g_mime_text_converter_convert (converter, GMIME_STREAM (reader), GMIME_STREAM (writer), &error));
	g_assert_error (error, GMIME_ERROR, ENOSPC);
	g_clear_error (&error);
	g_assert_cmpuint (reader->closes, ==, 0);
	g_assert_cmpuint (writer->closes, ==, 0);
	g_object_unref (reader);
	g_object_unref (writer);
	g_object_unref (converter);
}

int
main (int argc, char **argv)
{
	g_test_init (&argc, &argv, NULL);
	g_mime_init ();
	g_test_add_func ("/html-output/writer", test_writer);
	g_test_add_func ("/html-output/plain-flowed", test_plain_flowed);
	g_test_add_func ("/html-output/enriched", test_enriched);
	g_test_add_func ("/html-output/enriched-limits", test_enriched_limits);
	g_test_add_func ("/html-output/urls", test_urls);
	g_test_add_func ("/html-output/url-limits", test_url_limits);
	g_test_add_func ("/url-scan/matches", test_url_scan);
	g_test_add_func ("/url-scan/bounds", test_url_scan_bounds);
	g_test_add_func ("/url-scan/preconditions", test_url_scan_preconditions);
	g_test_add_func ("/html-output/custom-blockquotes", test_custom_blockquotes);
	g_test_add_func ("/html-output/callbacks-errors", test_callbacks_errors);
	int result = g_test_run ();

	g_mime_shutdown ();
	return result;
}
