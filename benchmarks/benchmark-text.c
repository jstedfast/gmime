/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* GMime. Workloads adapted from MimeKit's HtmlBenchmarks and
 * RtfConverterBenchmarks, copyright (c) .NET Foundation and Contributors.
 * Distributed under the MIT license; see data/LICENSE.
 */

#include "benchmark.h"
#include <string.h>

#define TARGET_SIZE (1024 * 1024)
#define SENTENCE "The quick brown fox jumps over the lazy dog. "

typedef enum {
	WORD, UNICODE_RTF, ENCAPSULATED, EXTRACTED, NESTED, FORMATTED, FONTS, BINARY,
	HTML_SPEC, HTML_ARTICLE
} Document;

typedef enum {
	TEXT, RTF_HTML, RTF_RENDER, HTML_STREAM, HTML_STRING, HTML_CALLBACK, TOKENS
} Operation;

static GString *
make_document (Document document)
{
	GString *text = g_string_sized_new (TARGET_SIZE + 4096);
	guint i = 0;

	switch (document) {
	case WORD:
		g_string_append (text, "{\\rtf1\\ansi\\ansicpg1252\\deff0\\deflang1033"
			"{\\fonttbl{\\f0\\froman\\fprq2\\fcharset0 Times New Roman;}{\\f1\\fswiss\\fprq2\\fcharset0 Arial;}"
			"{\\f2\\fmodern\\fprq1\\fcharset0 Courier New;}{\\f3\\fswiss\\fprq2{\\*\\panose 020b0604020202020204}\\fcharset204 Arial Cyr;}}"
			"{\\colortbl ;\\red255\\green0\\blue0;\\red0\\green0\\blue255;\\red0\\green128\\blue0;}"
			"{\\stylesheet{ Normal;}{\\s1 heading 1;}}{\\*\\generator Riched20 10.0.19041}\\viewkind4\\uc1\r\n");
		while (text->len < TARGET_SIZE) {
			g_string_append_printf (text, "\\pard\\sa200\\sl276\\slmult1\\f1\\fs22 " SENTENCE
				"{\\b bold %u} and {\\i italic} and {\\cf1\\ul colored underline}. Caf\\'e9 na\\'efve "
				"\\u8364? r\\'e9sum\\'e9. {\\f3\\'cf\\'f0\\'e8\\'e2\\'e5\\'f2} " SENTENCE SENTENCE "\\par\r\n", i);
			if (i % 10 == 0)
				g_string_append_printf (text, "{\\field{\\*\\fldinst{HYPERLINK \"https://www.example.com/page/%u\"}}"
					"{\\fldrslt{\\ul\\cf2 link text}}}\\par\r\n", i);
			if (i % 25 == 0)
				g_string_append (text, "\\trowd\\cellx3000\\cellx6000\\cellx9000\r\n"
					"\\pard\\intbl cell one\\cell {\\b cell two}\\cell cell three\\cell\\row\r\n"
					"\\pard\\intbl cell four\\cell cell five\\cell cell six\\cell\\row\r\n\\pard ");
			i++;
		}
		break;
	case UNICODE_RTF:
		g_string_append (text, "{\\rtf1\\ansi\\ansicpg932\\deff0{\\fonttbl{\\f0\\fnil\\fcharset128 MS Gothic;}"
			"{\\f1\\fnil\\fcharset0 Arial;}}\\uc1\\f0 ");
		while (text->len < TARGET_SIZE)
			g_string_append (text, "\\'93\\'fa\\'96\\'7b\\'8c\\'ea\\'82\\'cc\\'83\\'65\\'83\\'4c\\'83\\'58\\'83\\'67 "
				"{\\f1\\u1055?\\u1088?\\u1080?\\u1074?\\u1077?\\u1090? \\u20320?\\u22909?\\u19990?\\u30028?}\\par\r\n");
		break;
	case ENCAPSULATED:
	case EXTRACTED: {
		/* Generate the HTML baseline alongside its RTF envelope so that both
		 * contain precisely the same number of paragraphs. */
		GString *html = g_string_new ("<html><head><meta charset=\"utf-8\"></head><body style=\"font-family: Arial\">");
		g_string_append (text, "{\\rtf1\\ansi\\ansicpg1252\\fromhtml1 \\fbidis \\deff0{\\fonttbl{\\f0\\fswiss Arial;}"
			"{\\f1\\fmodern Courier New;}{\\f2\\fnil\\fcharset2 Symbol;}}{\\colortbl\\red0\\green0\\blue0;\\red0\\green0\\blue255;}\r\n"
			"{\\*\\htmltag19 <html>}{\\*\\htmltag34 <head>}{\\*\\htmltag1 <meta charset=\"utf-8\">}"
			"{\\*\\htmltag41 </head>}{\\*\\htmltag50 <body style=\"font-family: Arial\">}\\htmlrtf \\lang1033 \\htmlrtf0\r\n");
		while (text->len < TARGET_SIZE) {
			g_string_append_printf (text, "{\\*\\htmltag64 <p class=\"MsoNormal\">}\\htmlrtf {\\htmlrtf0 "
				SENTENCE "{\\*\\htmltag84 <b>}\\htmlrtf {\\b \\htmlrtf0 bold %u"
				"\\htmlrtf }\\htmlrtf0 {\\*\\htmltag92 </b>} caf\\'e9 "
				"{\\*\\htmltag84 <a href=\"https://www.example.com/page/%u\">}"
				"\\htmlrtf {\\field{\\*\\fldinst{HYPERLINK \"https://www.example.com/\"}}{\\fldrslt\\cf1\\ul \\htmlrtf0 link"
				"\\htmlrtf }\\htmlrtf0 \\htmlrtf }}\\htmlrtf0 {\\*\\htmltag92 </a>} "
				SENTENCE "\\htmlrtf\\par}\\htmlrtf0\r\n{\\*\\htmltag72 </p>}\r\n", i, i);
			g_string_append_printf (html, "<p class=\"MsoNormal\">" SENTENCE "<b>bold %u</b> caf\303\251 "
				"<a href=\"https://www.example.com/page/%u\">link</a> " SENTENCE "</p>", i, i);
			i++;
		}
		g_string_append (html, "</body></html>");
		g_string_append (text, "{\\*\\htmltag58 </body>}{\\*\\htmltag27 </html>}");
		if (document == EXTRACTED) {
			g_string_free (text, TRUE);
			return html;
		}
		g_string_free (html, TRUE);
		break;
	}
	case NESTED:
		g_string_append (text, "{\\rtf1 ");
		for (i = 0; i < TARGET_SIZE / 2; i++)
			g_string_append_c (text, '{');
		g_string_append_c (text, 'x');
		while (i--)
			g_string_append_c (text, '}');
		break;
	case FORMATTED:
		g_string_append (text, "{\\rtf1 ");
		while (text->len < TARGET_SIZE / 2)
			g_string_append (text, i++ % 2 ? "{\\b0 y" : "{\\b x");
		while (i--)
			g_string_append_c (text, '}');
		break;
	case FONTS:
		g_string_append (text, "{\\rtf1\\ansi{\\fonttbl");
		while (text->len < TARGET_SIZE - 4096) {
			g_string_append_printf (text, "{\\f%u\\fcharset%u Font%u;}", i, i % 2 ? 0 : 204, i);
			i++;
		}
		g_string_append_c (text, '}');
		for (guint font = 0; font < i; font += i / 100)
			g_string_append_printf (text, "\\f%u \\'cf\\'f0 ", font);
		break;
	case BINARY:
		g_string_append_printf (text, "{\\rtf1 before {\\*\\blipuid x}{\\pict\\bin%u ", TARGET_SIZE - 64);
		for (i = 0; i < TARGET_SIZE - 64; i++)
			g_string_append_c (text, '\377');
		g_string_append (text, "} after");
		break;
	default:
		g_assert_not_reached ();
	}
	g_string_append_c (text, '}');
	return text;
}

static gboolean
filter_tag (GMimeHtmlTagContext *context, GMimeHtmlWriter *writer, gpointer user_data, GError **error)
{
	GMimeHtmlTagId tag = g_mime_html_tag_context_get_tag_id (context);
	if (tag == GMIME_HTML_TAG_SCRIPT || tag == GMIME_HTML_TAG_STYLE) {
		g_mime_html_tag_context_set_suppress_inner_content (context, TRUE);
		g_mime_html_tag_context_set_delete_end_tag (context, TRUE);
		return TRUE;
	}
	return g_mime_html_tag_context_write_tag (context, writer, TRUE, error);
}

typedef struct {
	GByteArray *input;
	Operation operation;
	gboolean recovery, fail_on_limit;
} TextData;

static guint64
process_document (gpointer data)
{
	TextData *test = data;
	GMimeStream *input = NULL;
	GError *error = NULL;
	guint64 result = 0;

	if (test->operation != HTML_STRING) {
		input = g_mime_stream_mem_new_with_byte_array (test->input);
		g_mime_stream_mem_set_owner (GMIME_STREAM_MEM (input), FALSE);
	}
	if (test->operation == TOKENS) {
		GMimeHtmlTokenizer *tokenizer = g_mime_html_tokenizer_new (input);
		GMimeHtmlToken *token;
		g_mime_html_tokenizer_set_formatting_recovery_enabled (tokenizer, test->recovery);
		if (test->fail_on_limit)
			g_mime_html_tokenizer_set_limit_exceeded_behavior (tokenizer, GMIME_HTML_LIMIT_EXCEEDED_BEHAVIOR_FAIL);
		for (;;) {
			benchmark_check (g_mime_html_tokenizer_read_next_token (tokenizer, &token, &error), "reading HTML token", &error);
			if (!token)
				break;
			g_mime_html_token_unref (token);
			result++;
		}
		g_object_unref (tokenizer);
	} else {
		GMimeTextConverter *converter;
		switch (test->operation) {
		case TEXT: converter = g_mime_rtf_to_text_new (); break;
		case RTF_HTML: case RTF_RENDER:
			converter = g_mime_rtf_to_html_new ();
			g_mime_rtf_to_html_set_extract_encapsulated_html (GMIME_RTF_TO_HTML (converter), test->operation == RTF_HTML);
			break;
		default: converter = g_mime_html_to_html_new (); break;
		}
		if (test->recovery)
			g_mime_html_to_html_set_formatting_recovery_enabled (GMIME_HTML_TO_HTML (converter), TRUE);
		if (test->fail_on_limit)
			g_mime_html_to_html_set_limit_exceeded_behavior (GMIME_HTML_TO_HTML (converter), GMIME_HTML_LIMIT_EXCEEDED_BEHAVIOR_FAIL);
		if (test->operation == HTML_CALLBACK) {
			g_mime_html_to_html_set_filter_comments (GMIME_HTML_TO_HTML (converter), TRUE);
			g_mime_html_to_html_set_html_tag_callback (GMIME_HTML_TO_HTML (converter), filter_tag, NULL, NULL);
		}
		if (test->operation == HTML_STRING) {
			char *html = g_mime_text_converter_convert_to_string (converter, (const char *) test->input->data, test->input->len, &error);
			benchmark_check (html != NULL, "converting HTML to string", &error);
			result = strlen (html);
			g_free (html);
		} else {
			GMimeStream *output = g_mime_stream_null_new ();
			benchmark_check (g_mime_text_converter_convert (converter, input, output, &error), "converting document", &error);
			result = g_mime_stream_null_get_written (GMIME_STREAM_NULL (output));
			g_object_unref (output);
		}
		g_object_unref (converter);
	}
	g_clear_object (&input);
	if (!result)
		g_error ("No output from text benchmark");
	return result;
}

void
benchmark_text (const char *datadir, const char *testdatadir)
{
	static const struct {
		Document document;
		Operation operation;
		const char *name;
	} cases[] = {
		{ WORD, TEXT, "Rtf/Text/WordDocument" }, { WORD, RTF_HTML, "Rtf/Html/WordDocument" },
		{ UNICODE_RTF, TEXT, "Rtf/Text/UnicodeDocument" }, { UNICODE_RTF, RTF_HTML, "Rtf/Html/UnicodeDocument" },
		{ ENCAPSULATED, TEXT, "Rtf/Text/EncapsulatedHtml" }, { ENCAPSULATED, RTF_HTML, "Rtf/Html/EncapsulatedHtml/Extract" },
		{ ENCAPSULATED, RTF_RENDER, "Rtf/Html/EncapsulatedHtml/Render" }, { EXTRACTED, HTML_STREAM, "Html/Stream/ExtractedHtml" },
		{ NESTED, TEXT, "Rtf/Text/DeepNesting" }, { FORMATTED, TEXT, "Rtf/Text/DeepFormattedNesting" },
		{ FORMATTED, RTF_HTML, "Rtf/Html/DeepFormattedNesting" }, { FONTS, TEXT, "Rtf/Text/LargeFontTable" },
		{ BINARY, TEXT, "Rtf/Text/LargeBinary" },
		{ HTML_SPEC, TOKENS, "Html/Tokens/Test" }, { HTML_SPEC, HTML_STREAM, "Html/Stream/Test" },
		{ HTML_SPEC, HTML_STRING, "Html/String/Test" }, { HTML_SPEC, HTML_CALLBACK, "Html/Callback/Test" },
		{ HTML_ARTICLE, TOKENS, "Html/Tokens/Gimhae" }, { HTML_ARTICLE, HTML_STREAM, "Html/Stream/Gimhae" },
		{ HTML_ARTICLE, HTML_STRING, "Html/String/Gimhae" }, { HTML_ARTICLE, HTML_CALLBACK, "Html/Callback/Gimhae" }
	};

	for (guint i = 0; i < G_N_ELEMENTS (cases); i++) {
		if (benchmark_selected (cases[i].name)) {
			GBytes *bytes;
			TextData data = { 0 };

			if (cases[i].document == HTML_SPEC)
				bytes = benchmark_load (testdatadir, "html-tokenizer/test.html");
			else if (cases[i].document == HTML_ARTICLE)
				bytes = benchmark_load (datadir, "Gimhae_Kim_clan.utf-8.html");
			else
				bytes = g_string_free_to_bytes (make_document (cases[i].document));
			data.operation = cases[i].operation;
			data.input = g_bytes_unref_to_array (bytes);
			/* File.ReadAllText consumes the UTF-8 BOM before invoking MimeKit. */
			if (cases[i].document == HTML_ARTICLE && data.input->len >= 3 &&
			    !memcmp (data.input->data, "\357\273\277", 3))
				g_byte_array_remove_range (data.input, 0, 3);
			benchmark_run (cases[i].name, data.input->len, process_document, &data);
			g_byte_array_unref (data.input);
		}
	}

	for (guint sections = 0; sections <= 1024; sections = sections ? sections * 2 : 256) {
		for (int enabled = 0; enabled <= 1; enabled++) {
			const char *mode = enabled ? "Enabled" : "Disabled";
			char *name = sections ? g_strdup_printf ("Html/Formatting/Scaling/%u/%s", sections, mode) :
						g_strdup_printf ("Html/Formatting/Newsletter/%s", mode);
			if (benchmark_selected (name)) {
				TextData data = { NULL, sections ? TOKENS : HTML_STRING, TRUE, TRUE };
				if (sections) {
					GString *html = g_string_new (NULL);
					/* Keep fifteen distinct entries open; each section uses the sixteenth. */
					for (guint i = 0; i < 15; i++)
						g_string_append_printf (html, "<i title='%u'>", i);
					for (guint i = 0; i < sections; i++)
						g_string_append (html, "<b>word</b>");
					for (guint i = 0; i < 15; i++)
						g_string_append (html, "</i>");
					data.input = g_bytes_unref_to_array (g_string_free_to_bytes (html));
				} else {
					data.input = g_bytes_unref_to_array (benchmark_load (testdatadir, "html-converters/planet-fitness.html"));
				}
				guint64 count = process_document (&data);
				if (sections && count != 30 + 3 * sections)
					g_error ("Unexpected token count in %s", name);
				data.recovery = enabled;
				data.fail_on_limit = FALSE;
				benchmark_run (name, data.input->len, process_document, &data);
				g_byte_array_unref (data.input);
			}
			g_free (name);
		}
	}
}
