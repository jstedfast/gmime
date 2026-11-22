/* Adapted from MimeKit.Fuzzing's text, HTML and RTF targets.
 * Copyright (c) 2013-2026 .NET Foundation and Contributors.
 * Distributed under the MIT license; see LICENSE. */

#include "fuzz.h"
#include <gmime/gmime-html-tree-internal.h>
#include <gmime/gmime-rtf-tokenizer-private.h>
#include <gmime/gmime-rtf-interpreter-private.h>
#include <string.h>

static GByteArray *
convert (GMimeTextConverter *converter, const guint8 *data, gsize length, gsize chunk)
{
	GMimeStream *input = fuzz_stream_new (data, length, chunk);
	GMimeStream *output = g_mime_stream_mem_new ();
	GError *error = NULL;
	gboolean ok = g_mime_text_converter_convert (converter, input, output, &error);
	GByteArray *result = g_byte_array_ref (g_mime_stream_mem_get_byte_array (GMIME_STREAM_MEM (output)));

	g_assert_no_error (error);
	g_assert_true (ok);
	g_object_unref (input);
	g_object_unref (output);
	return result;
}

static GByteArray *
check_converter (GMimeTextConverter *converter, const guint8 *data, gsize length, guint selector)
{
	GByteArray *whole, *split;
	GError *error = NULL;

	/* A single-byte charset exercises every byte without rejecting input at
	 * the charset layer before reaching the renderer. */
	g_assert_true (g_mime_text_converter_set_input_charset (converter, "iso-8859-1", &error));
	g_mime_text_converter_set_detect_encoding_from_byte_order_mark (converter, FALSE);
	whole = convert (converter, data, length, 4096);
	split = convert (converter, data, length, 1 + selector % 31);
	fuzz_equal (whole->data, whole->len, split->data, split->len, G_OBJECT_TYPE_NAME (converter));
	if (length && !memchr (data, 0, length) && !GMIME_IS_HTML_TO_HTML (converter)) {
		char *utf8 = g_convert ((const char *) data, length, "UTF-8", "ISO-8859-1", NULL, NULL, &error);
		char *rendered = g_mime_text_converter_convert_to_string (converter, utf8, -1, &error);
		if (whole->len && memchr (whole->data, 0, whole->len)) {
			g_assert_error (error, G_CONVERT_ERROR, G_CONVERT_ERROR_EMBEDDED_NUL);
			g_assert_null (rendered);
			g_clear_error (&error);
		} else {
			g_assert_no_error (error);
			g_assert_nonnull (rendered);
			fuzz_equal ((guint8 *) rendered, strlen (rendered), whole->data, whole->len, "String vs stream conversion");
		}
		g_free (rendered);
		g_free (utf8);
	}
	g_byte_array_unref (split);
	g_object_unref (converter);
	return whole;
}

static gboolean
quote_callback (GMimeHtmlTagContext *context, GMimeHtmlWriter *writer, gpointer user_data, GError **error)
{
	guint mode = GPOINTER_TO_UINT (user_data);
	if (g_mime_html_tag_context_get_tag_id (context) == GMIME_HTML_TAG_BLOCK_QUOTE && mode == 1) {
		g_mime_html_tag_context_set_delete_tag (context, TRUE);
		g_mime_html_tag_context_set_delete_end_tag (context, TRUE);
		return TRUE;
	}
	if (mode == 2 && g_mime_html_tag_context_get_tag_id (context) == GMIME_HTML_TAG_BLOCK_QUOTE)
		g_mime_html_tag_context_set_suppress_inner_content (context, TRUE);
	if (mode == 3 && !g_mime_html_tag_context_get_is_end_tag (context))
		g_mime_html_tag_context_set_invoke_callback_for_end_tag (context, TRUE);
	return g_mime_html_tag_context_write_tag (context, writer, TRUE, error);
}

static void
check_generated_html (GByteArray *output, guint mode, gboolean fragment)
{
	static const char *schemes[] = { "mailto", "http", "https", "ftp", "sftp", "file", "news",
		"nntp", "telnet", "webcal", "callto", "h323", "sip" };
	GMimeStream *stream = fuzz_stream_new (output->data, output->len, 4096);
	GMimeHtmlTokenizer *reader = g_mime_html_tokenizer_new (stream);
	GArray *stack = g_array_new (FALSE, FALSE, sizeof (GMimeHtmlTagId));
	GMimeHtmlToken *token;
	GError *error = NULL;
	while (g_mime_html_tokenizer_read_next_token (reader, &token, &error) && token) {
		if (g_mime_html_token_get_kind (token) == GMIME_HTML_TOKEN_TAG) {
			GMimeHtmlTagId tag = g_mime_html_token_get_tag_id (token);
			gboolean end = g_mime_html_token_is_end_tag (token);
			switch (tag) {
			case GMIME_HTML_TAG_BODY: case GMIME_HTML_TAG_HTML: g_assert_false (fragment); break;
			case GMIME_HTML_TAG_BLOCK_QUOTE: g_assert_cmpuint (mode, !=, 1); break;
			case GMIME_HTML_TAG_P: case GMIME_HTML_TAG_BR: case GMIME_HTML_TAG_A: break;
			default: g_error ("Unexpected generated tag %s", g_mime_html_token_get_name (token));
			}
			if (tag == GMIME_HTML_TAG_A && !end) {
				const GMimeHtmlAttribute *href = g_mime_html_token_lookup_attribute (token, "href");
				gboolean allowed = FALSE;
				g_assert_cmpuint (g_mime_html_token_get_n_attributes (token), ==, 1);
				g_assert_nonnull (href);
				const char *scheme = g_uri_peek_scheme (g_mime_html_attribute_get_value (href));
				for (guint index = 0; scheme && index < G_N_ELEMENTS (schemes); index++)
					allowed |= !g_ascii_strcasecmp (scheme, schemes[index]);
				g_assert_true (allowed);
			} else {
				g_assert_cmpuint (g_mime_html_token_get_n_attributes (token), ==, 0);
			}
			if (end) {
				g_assert_cmpuint (stack->len, >, 0);
				g_assert_cmpint (g_array_index (stack, GMimeHtmlTagId, stack->len - 1), ==, tag);
				g_array_set_size (stack, stack->len - 1);
			} else if (tag != GMIME_HTML_TAG_BR) {
				g_array_append_val (stack, tag);
			}
		} else {
			g_assert_cmpint (g_mime_html_token_get_kind (token), ==, GMIME_HTML_TOKEN_DATA);
		}
		g_mime_html_token_unref (token);
	}
	g_assert_no_error (error);
	g_assert_cmpuint (stack->len, ==, 0);
	g_array_unref (stack);
	g_object_unref (reader);
	g_object_unref (stream);
}

static void
check_flowed (GByteArray *flowed, const guint8 *text, gsize length)
{
	GString *expected = g_string_new (NULL);
	GMimeTextConverter *decoder = g_mime_flowed_to_text_new ();
	g_mime_flowed_to_text_set_delete_space (GMIME_FLOWED_TO_TEXT (decoder), TRUE);
	GByteArray *plain = convert (decoder, flowed->data, flowed->len, 31);

	for (gsize start = 0; start < length; ) {
		gsize end = start, depth = 0, pos = start;
		while (end < length && text[end] != '\r' && text[end] != '\n')
			end++;
		start = end;
		if (start < length && text[start++] == '\r' && start < length && text[start] == '\n')
			start++;
		while (end > pos && text[end - 1] == ' ')
			end--;
		while (pos < end && text[pos] == '>') {
			g_string_append_c (expected, '>');
			depth++;
			if (++pos < end && text[pos] == ' ')
				pos++;
		}
		if (depth)
			g_string_append_c (expected, ' ');
		while (pos < end)
			g_string_append_unichar (expected, text[pos++]);
		g_string_append_c (expected, '\n');
	}
	fuzz_equal ((guint8 *) expected->str, expected->len, plain->data, plain->len, "Flowed text round trip");
	g_string_free (expected, TRUE);
	g_byte_array_unref (plain);
	g_object_unref (decoder);
}

void
fuzz_text_converters (const guint8 *data, gsize length)
{
	guint selector = fuzz_selector (data, length);
	/* Plain-text conversion rejects NUL; keep the rest of the input reachable. */
	guint8 *text = g_memdup2 (data, length);
	for (gsize index = 0; index < length; index++)
		if (!text[index])
			text[index] = ' ';
	GMimeTextConverter *converters[] = { g_mime_text_to_text_new (), g_mime_text_to_flowed_new (),
		g_mime_flowed_to_text_new (), g_mime_text_to_html_new (), g_mime_flowed_to_html_new () };

	g_mime_flowed_to_text_set_delete_space (GMIME_FLOWED_TO_TEXT (converters[2]), selector & 1);
	g_mime_flowed_to_html_set_delete_space (GMIME_FLOWED_TO_HTML (converters[4]), selector & 1);
	g_mime_text_to_html_set_output_html_fragment (GMIME_TEXT_TO_HTML (converters[3]), (selector & 2) != 0);
	g_mime_flowed_to_html_set_output_html_fragment (GMIME_FLOWED_TO_HTML (converters[4]), (selector & 2) != 0);
	guint mode = (selector >> 2) % 4;
	if (mode) {
		g_mime_text_to_html_set_html_tag_callback (GMIME_TEXT_TO_HTML (converters[3]), quote_callback, GUINT_TO_POINTER (mode), NULL);
		g_mime_flowed_to_html_set_html_tag_callback (GMIME_FLOWED_TO_HTML (converters[4]), quote_callback, GUINT_TO_POINTER (mode), NULL);
	}
	for (guint index = 0; index < G_N_ELEMENTS (converters); index++) {
		GByteArray *output = check_converter (converters[index], text, length, selector);
		if (index == 1)
			check_flowed (output, text, length);
		if (index >= 3)
			check_generated_html (output, mode, (selector & 2) != 0);
		g_byte_array_unref (output);
	}
	g_free (text);
}

static gboolean
count_html_placeholder (int index, GMimeHtmlWriter *writer, gpointer user_data, GError **error)
{
	int *count = user_data;
	g_assert_cmpint (index, ==, *count);
	(*count)++;
	return TRUE;
}

void
fuzz_rtf_converters (const guint8 *data, gsize length)
{
	guint selector = fuzz_selector (data, length);
	GMimeTextConverter *plain = g_mime_rtf_to_text_new (), *html = g_mime_rtf_to_html_new ();
	guint depth = 1 + (selector % 128);

	g_mime_rtf_to_text_set_max_group_depth (GMIME_RTF_TO_TEXT (plain), depth);
	g_mime_rtf_to_html_set_max_group_depth (GMIME_RTF_TO_HTML (html), depth);
	g_mime_rtf_to_html_set_extract_encapsulated_html (GMIME_RTF_TO_HTML (html), selector & 1);
	g_mime_rtf_to_html_set_output_html_fragment (GMIME_RTF_TO_HTML (html), (selector & 2) != 0);
	g_byte_array_unref (check_converter (plain, data, length, selector));
	g_byte_array_unref (check_converter (g_object_ref (html), data, length, selector));

	GError *error = NULL;
	gsize size;
	/* Preflight consumes the UTF-8 stream after public charset conversion. */
	char *utf8 = g_convert (length ? (const char *) data : "", length, "UTF-8", "ISO-8859-1", NULL, &size, &error);
	g_assert_no_error (error);
	GMimeStream *input = fuzz_stream_new ((const guint8 *) utf8, size, 4096);
	int expected, count = 0;
	g_assert_true (_g_mime_rtf_to_html_count_object_placeholders (GMIME_RTF_TO_HTML (html), input, &expected, &error));
	g_assert_no_error (error);
	g_object_unref (input);
	g_free (utf8);
	g_mime_rtf_to_html_set_attachment_placeholder_callback (GMIME_RTF_TO_HTML (html), count_html_placeholder, &count, NULL);
	g_byte_array_unref (convert (html, data, length, 1 + selector % 31));
	/* Preflight returns -1 for encapsulated HTML, which bypasses placeholders. */
	g_assert_cmpint (count, ==, MAX (expected, 0));
	g_object_unref (html);
}

static GString *
trace_tokens (GMimeHtmlTokenizer *tokenizer, gsize length, GError **error)
{
	GMimeHtmlToken *token;
	GString *trace = g_string_new (NULL);
	gsize count = 0;

	while (g_mime_html_tokenizer_read_next_token (tokenizer, &token, error) && token) {
		char *value = g_mime_html_token_to_string (token);
		g_assert_cmpuint (++count, <=, length + 1);
		g_string_append_printf (trace, "%d:%zu:%s;", g_mime_html_token_get_kind (token), strlen (value), value);
		g_free (value);
		g_mime_html_token_unref (token);
	}
	return trace;
}

static GString *
html_trace (GMimeStream *input, gsize length, guint selector)
{
	GError *error = NULL;
	GMimeHtmlTokenizer *tokenizer = g_mime_html_tokenizer_new_with_charset (input, "iso-8859-1", FALSE, 4096, &error);
	GString *trace;

	g_assert_no_error (error);
	g_mime_html_tokenizer_set_decode_character_references (tokenizer, selector & 1);
	g_mime_html_tokenizer_set_ignore_truncated_tags (tokenizer, (selector & 2) != 0);
	g_mime_html_tokenizer_set_scripting_enabled (tokenizer, (selector & 4) != 0);
	g_mime_html_tokenizer_set_max_element_depth (tokenizer, 1 + selector % 64);
	if (selector & 128)
		_g_mime_html_tree_set_formatting_capacity (_g_mime_html_tokenizer_get_tree (tokenizer), 1 + (selector >> 8) % 64);
	trace = trace_tokens (tokenizer, length, &error);
	g_assert_no_error (error);
	g_object_unref (tokenizer);
	return trace;
}

static const char *
formatting_outcome (HtmlTree *tree)
{
	if (tree->depth_exceeded)
		return "Depth";
	switch (tree->formatting_stop) {
	case HTML_FORMATTING_STOP_CAPACITY: return "Capacity";
	case HTML_FORMATTING_STOP_ATTRIBUTE_EQUIVALENCE: return "AttributeEquivalence";
	case HTML_FORMATTING_STOP_COMPLEX_ADOPTION: return "ComplexAdoption";
	case HTML_FORMATTING_STOP_STACK_REMAPPING: return "StackRemapping";
	default: return "trace agreement";
	}
}

gboolean
fuzz_measure_html_formatting (const char *path, const guint8 *data, gsize length)
{
	static const guint capacities[] = { 0, 16, 32, 64 };
	GString *html = g_string_sized_new (length);
	gboolean ok = TRUE;

	for (gsize i = 0; i < length; i++) {
		if (!data[i])
			g_string_append_unichar (html, 0xfffc);
		else if (data[i] == '\r') {
			g_string_append_c (html, '\n');
			if (i + 1 < length && data[i + 1] == '\n')
				i++;
		} else
			g_string_append_c (html, data[i]);
	}
	if (html->len > G_MAXUINT || !g_utf8_validate (html->str, html->len, NULL)) {
		g_printerr ("%s: expected a UTF-8 text fixture\n", path);
		g_string_free (html, TRUE);
		return FALSE;
	}
	for (guint scripting = 0; scripting <= 1; scripting++) {
		for (guint n = 0; n < G_N_ELEMENTS (capacities); n++) {
			GString *traces[2];
			const char *outcomes[2];
			guint peaks[2];
			for (guint j = 0; j < 2; j++) {
				GMimeStream *input = fuzz_stream_new ((guint8 *) html->str, html->len, j ? 13 : 4096);
				GMimeHtmlTokenizer *tokenizer = g_mime_html_tokenizer_new (input);
				HtmlTree *tree = _g_mime_html_tokenizer_get_tree (tokenizer);
				GError *error = NULL;
				g_mime_html_tokenizer_set_scripting_enabled (tokenizer, scripting);
				g_mime_html_tokenizer_set_limit_exceeded_behavior (tokenizer, GMIME_HTML_LIMIT_EXCEEDED_BEHAVIOR_FAIL);
				_g_mime_html_tree_set_formatting_capacity (tree, capacities[n]);
				traces[j] = trace_tokens (tokenizer, html->len, &error);
				outcomes[j] = formatting_outcome (tree);
				peaks[j] = tree->formatting_peak;
				if (error && !(g_error_matches (error, GMIME_ERROR, GMIME_ERROR_PARSE_ERROR) &&
					       (tree->depth_exceeded || _g_mime_html_tree_formatting_exceeded (tree)))) {
					g_printerr ("%s: %s\n", path, error->message);
					outcomes[j] = "error";
					ok = FALSE;
				}
				g_clear_error (&error);
				g_object_unref (tokenizer);
				g_object_unref (input);
			}
			if (!g_string_equal (traces[0], traces[1]) || strcmp (outcomes[0], outcomes[1]) || peaks[0] != peaks[1]) {
				outcomes[0] = "token trace mismatch";
				ok = FALSE;
			}
			g_print ("%s\t%u\t%u\t%u\t%s\n", path, scripting, capacities[n], peaks[0], outcomes[0]);
			g_string_free (traces[0], TRUE);
			g_string_free (traces[1], TRUE);
		}
	}
	g_string_free (html, TRUE);
	return ok;
}

void
fuzz_html_tokenizer (const guint8 *data, gsize length)
{
	guint selector = fuzz_selector (data, length);
	GMimeStream *streams[] = { fuzz_stream_new (data, length, 4096), fuzz_stream_new (data, length, 1 + selector % 31) };
	GString *whole = html_trace (streams[0], length, selector), *split = html_trace (streams[1], length, selector);
	GMimeTextConverter *converter = g_mime_html_to_html_new ();
	GMimeTextPreviewer *previewer = g_mime_html_text_previewer_new ();
	char *preview[2];
	GError *error = NULL;
	guint maximum = 1 + selector % 256;

	fuzz_equal ((guint8 *) whole->str, whole->len, (guint8 *) split->str, split->len, "HTML token trace");
	g_string_free (whole, TRUE);
	g_string_free (split, TRUE);
	g_mime_html_to_html_set_output_html_fragment (GMIME_HTML_TO_HTML (converter), selector & 1);
	g_mime_html_to_html_set_no_script_handling (GMIME_HTML_TO_HTML (converter), (selector >> 4) % 3);
	g_mime_html_to_html_set_filter_comments (GMIME_HTML_TO_HTML (converter), (selector & 64) != 0);
	g_mime_html_to_html_set_max_element_depth (GMIME_HTML_TO_HTML (converter), (selector & 8) ? 4 : 4096);
	GByteArray *rendered = check_converter (g_object_ref (converter), data, length, selector);
	/* The first pass may have encoded over-depth markup as text. Recheck
	 * the normalized UTF-8 output without imposing that small depth limit. */
	g_mime_html_to_html_set_max_element_depth (GMIME_HTML_TO_HTML (converter), 1 << 20);
	g_assert_true (g_mime_text_converter_set_input_charset (converter, "utf-8", &error));
	GByteArray *again = convert (converter, rendered->data, rendered->len, 4096);
	fuzz_equal (again->data, again->len, rendered->data, rendered->len, "HTML conversion fixed point");
	g_byte_array_unref (rendered);
	g_byte_array_unref (again);
	g_object_unref (converter);
	g_mime_text_previewer_set_maximum_preview_length (previewer, maximum);
	for (guint index = 0; index < 2; index++) {
		g_assert_cmpint (g_mime_stream_reset (streams[index]), ==, 0);
		preview[index] = g_mime_text_previewer_get_preview_text_from_stream (previewer, streams[index], "iso-8859-1", &error);
		g_assert_no_error (error);
		g_assert_nonnull (preview[index]);
		g_assert_cmpint (g_utf8_strlen (preview[index], -1), <=, maximum);
		g_object_unref (streams[index]);
	}
	g_assert_cmpstr (preview[0], ==, preview[1]);
	g_free (preview[0]);
	g_free (preview[1]);
	g_object_unref (previewer);
}

static gboolean
record_event (GMimeRtfEvent event, const GMimeRtfFormat *format, const char *text, gsize length, gpointer data, GError **error)
{
	GString *trace = data;
	g_string_append_printf (trace, "E%d:%d:%d:%d:%d:%d:%d:%d:%d:%d:%d:%d:%d:%s;", event,
		format->from_html, format->bold, format->italic, format->underline, format->strike, format->in_table,
		format->alignment, format->vertical_alignment, format->font, format->font_size,
		format->foreground, format->background, format->hyperlink ? format->hyperlink : "");
	if (length)
		g_string_append_len (trace, text, length);
	return TRUE;
}

static GString *
rtf_trace (const guint8 *data, gsize length, guint selector, gsize chunk)
{
	GMimeStream *stream = fuzz_stream_new (data, length, chunk);
	GMimeRtfTokenizer *tokenizer = _g_mime_rtf_tokenizer_new (stream);
	GMimeRtfToken token;
	GString *trace = g_string_new (NULL);
	GError *error = NULL;
	guint count = 0;
	gboolean text = FALSE;

	while (_g_mime_rtf_tokenizer_next (tokenizer, &token, &error) && token.kind != GMIME_RTF_EOF) {
		g_assert_cmpuint (++count, <=, length + 1);
		if (token.kind == GMIME_RTF_TEXT) {
			g_assert_cmpuint (token.length, >, 0);
			if (!text)
				g_string_append_c (trace, 'T');
			g_string_append_len (trace, (const char *) token.text, token.length);
			text = TRUE;
		} else {
			text = FALSE;
			g_string_append_printf (trace, "K%d:%" G_GUINT64_FORMAT ":%s:%d:%d:%d:%d:%u:%u;",
				token.kind, token.depth, token.keyword, token.parameter, token.has_parameter,
				token.parameter_invalid, token.keyword_truncated, token.literal, token.value);
		}
	}
	g_assert_no_error (error);
	_g_mime_rtf_tokenizer_free (tokenizer);
	g_assert_cmpint (g_mime_stream_reset (stream), ==, 0);
	g_assert_true (_g_mime_rtf_interpret (stream, selector & 1, 1 + selector % 64, 32, 32, NULL, record_event, trace, &error));
	g_assert_no_error (error);
	g_object_unref (stream);
	return trace;
}

void
fuzz_rtf_tokenizer (const guint8 *data, gsize length)
{
	guint selector = fuzz_selector (data, length);
	/* This private layer receives decoded UTF-8, not the original wire bytes. */
	GString *utf8 = g_string_sized_new (length);
	for (gsize index = 0; index < length; index++)
		g_string_append_unichar (utf8, data[index]);
	GString *whole = rtf_trace ((guint8 *) utf8->str, utf8->len, selector, 4096);
	GString *split = rtf_trace ((guint8 *) utf8->str, utf8->len, selector, 1 + selector % 31);
	fuzz_equal ((guint8 *) whole->str, whole->len, (guint8 *) split->str, split->len, "RTF token and interpreter trace");
	g_string_free (whole, TRUE);
	g_string_free (split, TRUE);
	g_string_free (utf8, TRUE);
}
