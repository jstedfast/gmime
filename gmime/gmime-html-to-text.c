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
/* Plain-text rendering follows MimeKit HtmlToText. */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <string.h>
#include <errno.h>

#include "gmime-html-to-text.h"
#include "gmime-html-tokenizer.h"
#include "gmime-html-tree-private.h"
#include "gmime-text-private.h"
#include "gmime-reader-simd-private.h"

/**
 * SECTION: gmime-html-to-text
 * @title: HTML to plain text
 * @short_description: Plain-text rendering of HTML
 *
 * Renders HTML as plain text with optional link destinations and image
 * alternative text. Lists, quotations and preformatted text retain their
 * plain-text structure, and generated line endings are LF.
 *
 * Length limits count Unicode scalar values rather than bytes or UTF-16
 * code units. Headers and footers are copied as plain text and are not
 * included in the input or output length limits.
 **/

struct _GMimeHtmlToText {
	GMimeTextConverter parent_object;
	gboolean links, images;
	guint depth;
	gsize input_limit, output_limit;
};

G_DEFINE_TYPE (GMimeHtmlToText, g_mime_html_to_text, GMIME_TYPE_TEXT_CONVERTER)

typedef struct _Element Element;

struct _Element {
	char *name, *url;
	guint index;
	int indent, quote_depth, pre_depth;
	Element *list, *item, *link, *heading, *same_name;
	gboolean visible, matches;
	gsize link_length;
	gint64 next_number;
};

typedef struct {
	GMimeHtmlToText *options;
	GMimeStream *output;
	GError **error;
	GHashTable *open;
	GPtrArray *elements;
	Element *list, *item, *link, *heading;
	int indent, quote_depth, pre_depth, breaks, newlines;
	gsize output_length;
	guint suppressed;
	gboolean line_start, has_output, space, skip_pre_newline, previous_cr, failed;
	char *marker;
} TextState;

static gboolean
named (const char *name, const char *names)
{
	const char *p = names;
	gsize n = strlen (name);

	while (*p) {
		const char *end = strchr (p, '|');
		gsize len = end ? (gsize) (end - p) : strlen (p);

		if (len == n && !g_ascii_strncasecmp (name, p, n))
			return TRUE;

		p += len;

		if (*p)
			p++;
	}

	return FALSE;
}

static int
heading_level (const char *name)
{
	return g_ascii_tolower (name[0]) == 'h' && name[1] >= '1' && name[1] <= '6' && !name[2] ? name[1] - '0' : 0;
}

static gboolean
is_list (const char *name)
{
	return named (name, "ol|ul|menu|dir");
}

static int
spacing (const char *name)
{
	if (heading_level (name) || named (name, "p|pre|listing|xmp|plaintext|blockquote|table|dl"))
		return 2;

	return named (name,
		      "address|article|aside|center|details|dialog|div|fieldset|figcaption|figure|footer|form|header|main|nav|section|summary|tr|caption|dd|dt|textarea|li|ol|ul|menu|dir");
}

static void
boundary (TextState *state, int count)
{
	if (!state->marker)
		state->breaks = MAX (state->breaks, count);

	state->space = FALSE;
}

static void
output (TextState *state, const char *text, gsize length)
{
	gsize count;

	if (state->failed || !length)
		return;

	count = g_utf8_strlen (text, length);
	if (count > state->options->output_limit - state->output_length) {
		g_set_error_literal (state->error, G_MARKUP_ERROR, G_MARKUP_ERROR_INVALID_CONTENT,
				     "The HTML output length limit has been exceeded.");
		state->failed = TRUE;
		return;
	}

	state->output_length += count;
	state->failed = !_g_mime_text_write (state->output, text, length, state->error);
}

static void
flush_breaks (TextState *state)
{
	if (state->has_output && state->breaks > state->newlines) {
		for (int i = state->newlines; i < state->breaks; i++)
			output (state, "\n", 1);

		state->line_start = TRUE;
		state->newlines = state->breaks;
		state->space = FALSE;
	}

	state->breaks = 0;
}

static void
track_link (TextState *state, const char *text, gsize length)
{
	Element *link = state->link;
	gsize url_length;

	if (!link || !link->url)
		return;

	while (!link->link_length && length && *text == ' ') {
		text++;
		length--;
	}

	if (!length)
		return;

	url_length = strlen (link->url);
	if (link->link_length <= url_length && length <= url_length - link->link_length &&
	    !memcmp (link->url + link->link_length, text, length))
		link->link_length += length;
	else {
		link->matches = FALSE;
		link->link_length = url_length + 1;
	}
}

static void
write_run (TextState *state, const char *text, gsize length)
{
	flush_breaks (state);

	if (state->space && !state->line_start) {
		output (state, " ", 1);
		track_link (state, " ", 1);
	}

	state->space = FALSE;

	if (state->line_start) {
		int padding = state->marker ? state->indent - (int) strlen (state->marker) : state->indent;

		for (int i = 0; i < state->quote_depth && !state->failed; i++)
			output (state, ">", 1);

		if (state->quote_depth)
			output (state, " ", 1);

		for (int i = 0; i < padding && !state->failed; i++)
			output (state, " ", 1);

		if (state->marker) {
			output (state, state->marker, strlen (state->marker));
			g_clear_pointer (&state->marker, g_free);
		}

		state->line_start = FALSE;
	}

	output (state, text, length);
	track_link (state, text, length);
	state->has_output = TRUE;
	state->newlines = 0;
}

static void
write_text (TextState *state, const char *text)
{
	static const GMimeReaderByteSet whitespace = { { [0] = 4, [9] = 1, [10] = 1, [12] = 1, [13] = 1 }, FALSE };
	gsize remaining = strlen (text);

	while (remaining && !state->failed) {
		gsize n = _g_mime_reader_find_byte ((const guint8 *) text, remaining, &whitespace);

		if (n)
			write_run (state, text, n);

		text += n;
		remaining -= n;

		if (remaining) {
			state->space = TRUE;
			text++;
			remaining--;
		}
	}
}

static void
finish_marker (TextState *state)
{
	char *text;

	if (!state->marker)
		return;

	text = state->marker;
	state->indent -= strlen (text);
	state->marker = NULL;

	g_strchomp (text);
	write_text (state, text);
	g_free (text);
}

static void
line_break (TextState *state)
{
	flush_breaks (state);

	if (state->marker) {
		int indent = state->indent;

		finish_marker (state);
		state->indent = indent;
	}

	output (state, "\n", 1);

	state->has_output = state->line_start = TRUE;
	state->newlines = MIN (state->newlines + 1, 2);
	state->space = FALSE;

	track_link (state, "\n", 1);
}

static void
write_preformatted (TextState *state, const char *text)
{
	static const GMimeReaderByteSet newlines = { { [10] = 1, [13] = 1 }, FALSE };
	gsize remaining = strlen (text);

	while (remaining && !state->failed) {
		char c = *text;

		if (state->previous_cr && c == '\n') {
			state->previous_cr = FALSE;
			text++;
			remaining--;
			continue;
		}

		state->previous_cr = c == '\r';
		if (state->skip_pre_newline) {
			state->skip_pre_newline = FALSE;
			if (c == '\r' || c == '\n') {
				text++;
				remaining--;
				continue;
			}
		}

		if (c == '\r' || c == '\n') {
			line_break (state);
			text++;
			remaining--;
		} else {
			gsize len = _g_mime_reader_find_byte ((const guint8 *) text, remaining, &newlines);

			write_run (state, text, len);
			text += len;
			remaining -= len;
		}
	}
}

static Element *
peek (TextState *state, const char *name)
{
	char *lower = g_ascii_strdown (name, -1);
	Element *element = g_hash_table_lookup (state->open, lower);

	g_free (lower);

	return element;
}

static void
close_at (TextState *state, guint index)
{
	while (state->elements->len > index) {
		Element *element = g_ptr_array_index (state->elements, state->elements->len - 1);

		if (element->visible) {
			if (!strcmp (element->name, "a") && element->url) {
				int breaks = state->breaks;

				state->link = NULL;
				state->breaks = 0;
				if (!element->link_length) {
					write_text (state, element->url);
				} else if (!element->matches || element->link_length != strlen (element->url)) {
					write_text (state, " (");
					write_text (state, element->url);
					write_run (state, ")", 1);
				}

				state->breaks = MAX (state->breaks, breaks);
			}

			if (!strcmp (element->name, "li")) {
				finish_marker (state);
				state->breaks = 1;
			}

			int count = is_list (element->name) ? (element->list ? 1 : 2) : spacing (element->name);

			if (count)
				boundary (state, count);

			if (named (element->name, "td|th"))
				state->space = TRUE;
		} else {
			state->suppressed--;
		}

		state->indent = element->indent;
		state->quote_depth = element->quote_depth;
		state->pre_depth = element->pre_depth;
		state->list = element->list;
		state->item = element->item;
		state->link = element->link;
		state->heading = element->heading;

		if (element->same_name)
			g_hash_table_replace (state->open, element->same_name->name, element->same_name);
		else
			g_hash_table_remove (state->open, element->name);

		g_ptr_array_set_size (state->elements, state->elements->len - 1);
		g_free (element->name);
		g_free (element->url);
		g_free (element);

		state->previous_cr = state->skip_pre_newline = FALSE;
	}
}

static void
close_name (TextState *state, const char *name)
{
	Element *element = peek (state, name);

	if (element)
		close_at (state, element->index);
}

static gboolean
in_template_scope (TextState *state, Element *element)
{
	Element *boundary = peek (state, "template");

	return !boundary || element->index > boundary->index;
}

static void
close_in_scope (TextState *state, const char *name, const char *scope)
{
	Element *element = peek (state, name), *scope_boundary = peek (state, scope);

	if (element && in_template_scope (state, element) &&
	    (!scope_boundary || element->index > scope_boundary->index))
		close_at (state, element->index);
}

static void
close_paragraph (TextState *state)
{
	static const char *scopes[] = { "button", "table",   "td",     "th",      "caption",
					"applet", "marquee", "object", "template" };
	Element *paragraph = peek (state, "p");

	if (!paragraph)
		return;

	for (guint i = 0; i < G_N_ELEMENTS (scopes); i++) {
		Element *scope_boundary = peek (state, scopes[i]);

		if (scope_boundary && scope_boundary->index > paragraph->index)
			return;
	}

	close_at (state, paragraph->index);
}

static const char *
attribute (GMimeHtmlToken *tag, const char *name)
{
	const GMimeHtmlAttribute *attr = g_mime_html_token_lookup_attribute (tag, name);

	return attr ? g_mime_html_attribute_get_value (attr) : NULL;
}

static gint64
number (GMimeHtmlToken *tag, const char *name, gint64 fallback)
{
	const char *value = attribute (tag, name);
	char *end;
	gint64 n;

	if (!value)
		return fallback;

	errno = 0;
	n = g_ascii_strtoll (value, &end, 10);

	if (end == value)
		return fallback;

	while (g_ascii_isspace (*end))
		end++;

	return !errno && !*end && n >= G_MININT32 && n <= G_MAXINT32 ? n : fallback;
}

static char *
trim_url (const char *value)
{
	const char *end = value + strlen (value), *previous;

	while (value < end && g_unichar_isspace (g_utf8_get_char (value)))
		value = g_utf8_next_char (value);

	while (end > value) {
		previous = g_utf8_find_prev_char (value, end);
		if (!g_unichar_isspace (g_utf8_get_char (previous)))
			break;

		end = previous;
	}

	return g_strndup (value, end - value);
}

static void
start_tag (TextState *state, GMimeHtmlToken *tag)
{
	g_autofree char *name = g_ascii_strdown (g_mime_html_token_get_name (tag), -1);
	const char *value;
	Element *element;
	gboolean suppressed;

	if (!strcmp (name, "head"))
		return;

	if (named (name, "html|body") && !state->suppressed && !g_mime_html_token_lookup_attribute (tag, "hidden"))
		return;

	if (spacing (name))
		close_paragraph (state);

	if (!strcmp (name, "li") && state->item && state->item->list == state->list &&
	    in_template_scope (state, state->item))
		close_at (state, state->item->index);

	if (heading_level (name) && state->heading && in_template_scope (state, state->heading))
		close_at (state, state->heading->index);

	if (!strcmp (name, "a"))
		close_in_scope (state, "a", "template");

	if (named (name, "dd|dt")) {
		close_in_scope (state, "dd", "dl");
		close_in_scope (state, "dt", "dl");
	}

	suppressed = state->suppressed || named (name, "script|style|title|iframe|noframes|template") ||
		     g_mime_html_token_lookup_attribute (tag, "hidden") != NULL;
	if (named (name,
		   "area|base|basefont|bgsound|br|col|embed|frame|hr|img|input|keygen|link|meta|param|source|track|wbr")) {
		if (!suppressed) {
			if (!strcmp (name, "br"))
				line_break (state);
			else if (!strcmp (name, "hr")) {
				boundary (state, 2);
				write_text (state, "---");
				boundary (state, 2);
			} else if (!strcmp (name, "img") && state->options->images && (value = attribute (tag, "alt")))
				write_text (state, value);
		}

		return;
	}

	if (state->elements->len >= state->options->depth) {
		if (!state->failed)
			g_set_error_literal (state->error, G_MARKUP_ERROR, G_MARKUP_ERROR_INVALID_CONTENT,
					     "The HTML element depth limit has been exceeded.");
		state->failed = TRUE;
		return;
	}

	element = g_new0 (Element, 1);
	element->name = g_ascii_strdown (name, -1);
	element->index = state->elements->len;
	element->matches = TRUE;
	element->indent = state->indent;
	element->quote_depth = state->quote_depth;
	element->pre_depth = state->pre_depth;
	element->list = state->list;
	element->item = state->item;
	element->link = state->link;
	element->heading = state->heading;
	element->visible = !suppressed;
	element->same_name = peek (state, element->name);

	g_hash_table_replace (state->open, element->name, element);
	g_ptr_array_add (state->elements, element);

	if (suppressed) {
		state->suppressed++;

		if (is_list (name))
			state->list = element;

		if (!strcmp (name, "li"))
			state->item = element;

		if (heading_level (name))
			state->heading = element;
	} else {
		int count = spacing (name);

		if (count)
			boundary (state, count);

		if (!strcmp (name, "blockquote")) {
			state->quote_depth++;
		} else if (named (name, "pre|listing|xmp|textarea|plaintext")) {
			state->pre_depth++;
			state->skip_pre_newline = !named (name, "xmp|plaintext");
			state->previous_cr = FALSE;
		} else if (is_list (name)) {
			finish_marker (state);
			boundary (state, state->list ? 1 : 2);
			element->next_number = number (tag, "start", 1);
			state->list = element;
		} else if (!strcmp (name, "li")) {
			state->item = element;
			state->indent = state->list ? state->list->indent : state->indent;

			g_free (state->marker);

			if (state->list && !strcmp (state->list->name, "ol")) {
				state->list->next_number = number (tag, "value", state->list->next_number);
				state->marker = g_strdup_printf ("%" G_GINT64_FORMAT ". ", state->list->next_number++);
			} else {
				state->marker = g_strdup ("- ");
			}

			state->indent += strlen (state->marker);
		} else if (!strcmp (name, "dd")) {
			state->indent += 2;
		} else if (!strcmp (name, "a") && state->options->links && (value = attribute (tag, "href"))) {
			element->url = trim_url (value);
			if (*element->url)
				state->link = element;
		} else if (named (name, "td|th")) {
			state->space = TRUE;
		} else if (heading_level (name)) {
			char prefix[8];
			int n = heading_level (name);

			memset (prefix, '#', n);
			prefix[n++] = ' ';
			prefix[n] = 0;
			state->heading = element;
			write_text (state, prefix);
		}
	}

	/* HTML ignores self-closing syntax on non-void elements, including scripts and templates. */
	if (g_mime_html_token_is_empty_element (tag) && g_mime_html_token_get_tag_id (tag) == GMIME_HTML_TAG_UNKNOWN &&
	    strcmp (name, "template"))
		close_at (state, element->index);
}

static gboolean
convert_html_to_text (GMimeTextConverter *converter, GMimeStream *input, GMimeStream *output_stream, GError **error)
{
	GMimeHtmlToText *options = GMIME_HTML_TO_TEXT (converter);
	GMimeHtmlTokenizer *tokenizer;
	GMimeHtmlToken *token = NULL;
	TextState state = { 0 };
	const char *header = g_mime_text_converter_get_header (converter);
	const char *footer = g_mime_text_converter_get_footer (converter);

	if (header && !_g_mime_text_write (output_stream, header, strlen (header), error))
		return FALSE;

	state.options = options;
	state.output = output_stream;
	state.error = error;
	state.line_start = TRUE;
	state.open = g_hash_table_new (g_str_hash, g_str_equal);
	state.elements = g_ptr_array_new ();

	tokenizer = g_mime_html_tokenizer_new (input);
	g_mime_html_tokenizer_set_decode_character_references (tokenizer, TRUE);
	g_mime_html_tokenizer_set_scripting_enabled (tokenizer, FALSE);
	g_mime_html_tokenizer_set_ignore_truncated_tags (tokenizer, TRUE);
	g_mime_html_tokenizer_set_max_element_depth (tokenizer, options->depth);
	_g_mime_html_tokenizer_set_max_input_length (tokenizer, options->input_limit);
	_g_mime_html_tokenizer_set_track_closed_elements (tokenizer, TRUE);

	while (!state.failed) {
		const GMimeHtmlClosedElement *closed;
		gsize nclosed;

		if (!g_mime_html_tokenizer_read_next_token (tokenizer, &token, error)) {
			state.failed = TRUE;
			break;
		}

		if (!token)
			break;

		if (_g_mime_html_tokenizer_get_depth_exceeded (tokenizer)) {
			g_set_error_literal (error, G_MARKUP_ERROR, G_MARKUP_ERROR_INVALID_CONTENT,
					     "The HTML element depth limit has been exceeded.");
			state.failed = TRUE;
			g_mime_html_token_unref (token);
			break;
		}

		closed = _g_mime_html_tokenizer_get_closed_elements (tokenizer, &nclosed);

		for (gsize i = 0; i < nclosed; i++) {
			const GMimeHtmlClosedElement *element = closed + i;

			for (guint n = 0; n < element->count; n++)
				close_name (&state, element->name);
		}

		_g_mime_html_tokenizer_clear_closed_elements (tokenizer);
		GMimeHtmlTokenKind kind = g_mime_html_token_get_kind (token);

		if (kind != GMIME_HTML_TOKEN_DATA && kind != GMIME_HTML_TOKEN_CDATA)
			state.previous_cr = state.skip_pre_newline = FALSE;

		if (kind == GMIME_HTML_TOKEN_TAG) {
			int disposition = _g_mime_html_tokenizer_get_tag_disposition (tokenizer);
			const char *name = g_mime_html_token_get_name (token);

			if (disposition != GMIME_HTML_TAG_DISPOSITION_IGNORED) {
				if (g_mime_html_token_is_end_tag (token)) {
					if (heading_level (name) && state.heading)
						close_at (&state, state.heading->index);
					else if (!g_ascii_strcasecmp (name, "br") && !state.suppressed)
						line_break (&state);
					else
						close_name (&state, name);
				} else if (disposition != GMIME_HTML_TAG_DISPOSITION_ATTRIBUTES_MERGED)
					start_tag (&state, token);
			}
		} else if (!state.suppressed && !_g_mime_html_tokenizer_get_in_frameset (tokenizer) &&
			   (kind == GMIME_HTML_TOKEN_DATA || kind == GMIME_HTML_TOKEN_CDATA)) {
			if (strlen (g_mime_html_token_get_data (token)) != g_mime_html_token_get_data_length (token)) {
				g_set_error_literal (error, G_CONVERT_ERROR, G_CONVERT_ERROR_EMBEDDED_NUL,
						     "The HTML text contains an embedded NUL.");
				state.failed = TRUE;
			} else if (state.pre_depth) {
				write_preformatted (&state, g_mime_html_token_get_data (token));
			} else {
				write_text (&state, g_mime_html_token_get_data (token));
			}
		}

		g_mime_html_token_unref (token);
	}

	close_at (&state, 0);

	g_free (state.marker);
	g_hash_table_destroy (state.open);
	g_ptr_array_free (state.elements, TRUE);
	g_object_unref (tokenizer);

	if (!state.failed && footer)
		state.failed = !_g_mime_text_write (output_stream, footer, strlen (footer), error);

	return !state.failed;
}

static void
g_mime_html_to_text_class_init (GMimeHtmlToTextClass *klass)
{
	GMimeTextConverterClass *converter = GMIME_TEXT_CONVERTER_CLASS (klass);

	converter->input_format = GMIME_TEXT_FORMAT_HTML;
	converter->output_format = GMIME_TEXT_FORMAT_PLAIN;
	converter->convert = convert_html_to_text;
}

static void
g_mime_html_to_text_init (GMimeHtmlToText *self)
{
	self->links = self->images = TRUE;
	self->depth = 4096;
	self->input_limit = 16 * 1024 * 1024;
	self->output_limit = 64 * 1024 * 1024;
}

/**
 * g_mime_html_to_text_new:
 *
 * Creates an HTML to plain-text converter. Link URLs and image alternative
 * text are included by default. Generated line endings are LF; configured
 * headers and footers are copied as plain text.
 *
 * Returns: (transfer full): a new #GMimeHtmlToText.
 **/
GMimeTextConverter *
g_mime_html_to_text_new (void)
{
	return g_object_new (GMIME_TYPE_HTML_TO_TEXT, NULL);
}

/**
 * g_mime_html_to_text_get_output_link_urls:
 * @converter: a #GMimeHtmlToText
 *
 * Gets whether link destinations are included in the output.
 *
 * Returns: %TRUE if link URLs are included (the default).
 **/
gboolean
g_mime_html_to_text_get_output_link_urls (GMimeHtmlToText *converter)
{
	g_return_val_if_fail (GMIME_IS_HTML_TO_TEXT (converter), FALSE);

	return converter->links;
}

/**
 * g_mime_html_to_text_set_output_link_urls:
 * @converter: a #GMimeHtmlToText
 * @value: whether to include link destinations
 *
 * Controls whether a link's URL is appended in parentheses when its visible
 * text differs from the URL. Empty links output the URL alone. Disabling
 * this option preserves visible link text but omits the destination.
 **/
void
g_mime_html_to_text_set_output_link_urls (GMimeHtmlToText *converter, gboolean value)
{
	g_return_if_fail (GMIME_IS_HTML_TO_TEXT (converter));

	converter->links = !!value;
}

/**
 * g_mime_html_to_text_get_output_image_alt_text:
 * @converter: a #GMimeHtmlToText
 *
 * Gets whether image alternative text is included in the output.
 *
 * Returns: %TRUE if image alt attributes are rendered (the default).
 **/
gboolean
g_mime_html_to_text_get_output_image_alt_text (GMimeHtmlToText *converter)
{
	g_return_val_if_fail (GMIME_IS_HTML_TO_TEXT (converter), FALSE);

	return converter->images;
}

/**
 * g_mime_html_to_text_set_output_image_alt_text:
 * @converter: a #GMimeHtmlToText
 * @value: whether to render image alt attributes
 *
 * Controls whether an image's alternative text is rendered as plain text.
 * Images without an alt attribute contribute no text.
 **/
void
g_mime_html_to_text_set_output_image_alt_text (GMimeHtmlToText *converter, gboolean value)
{
	g_return_if_fail (GMIME_IS_HTML_TO_TEXT (converter));

	converter->images = !!value;
}

/**
 * g_mime_html_to_text_get_max_element_depth:
 * @converter: a #GMimeHtmlToText
 *
 * Gets the tokenizer's maximum number of distinct open-element stack entries.
 *
 * Returns: the element depth limit, initially 4096.
 **/
guint
g_mime_html_to_text_get_max_element_depth (GMimeHtmlToText *converter)
{
	g_return_val_if_fail (GMIME_IS_HTML_TO_TEXT (converter), 0);

	return converter->depth;
}

/**
 * g_mime_html_to_text_set_max_element_depth:
 * @converter: a #GMimeHtmlToText
 * @value: the maximum element depth, from 1 to %G_MAXINT
 *
 * Limits the tokenizer's distinct open-element stack entries. After this
 * limit is exceeded, the remaining input is treated as literal text rather
 * than parsed as markup.
 **/
void
g_mime_html_to_text_set_max_element_depth (GMimeHtmlToText *converter, guint value)
{
	g_return_if_fail (GMIME_IS_HTML_TO_TEXT (converter));
	g_return_if_fail (value > 0);

	converter->depth = value;
}

/**
 * g_mime_html_to_text_get_max_input_length:
 * @converter: a #GMimeHtmlToText
 *
 * Gets the input length limit in Unicode scalar values, including HTML
 * markup but excluding the configured header and footer.
 *
 * Returns: the input length limit, initially 16 * 1024 * 1024.
 **/
gsize
g_mime_html_to_text_get_max_input_length (GMimeHtmlToText *converter)
{
	g_return_val_if_fail (GMIME_IS_HTML_TO_TEXT (converter), 0);

	return converter->input_limit;
}

/**
 * g_mime_html_to_text_set_max_input_length:
 * @converter: a #GMimeHtmlToText
 * @value: the maximum input length in Unicode scalar values, greater than zero
 *
 * Limits decoded HTML input, including markup. The limit counts Unicode
 * scalar values, not bytes or UTF-16 code units. Exceeding it causes
 * conversion to fail with an error.
 **/
void
g_mime_html_to_text_set_max_input_length (GMimeHtmlToText *converter, gsize value)
{
	g_return_if_fail (GMIME_IS_HTML_TO_TEXT (converter));
	g_return_if_fail (value > 0);

	converter->input_limit = value;
}

/**
 * g_mime_html_to_text_get_max_output_length:
 * @converter: a #GMimeHtmlToText
 *
 * Gets the rendered text length limit in Unicode scalar values. Configured
 * headers and footers do not count toward this limit.
 *
 * Returns: the output length limit, initially 64 * 1024 * 1024.
 **/
gsize
g_mime_html_to_text_get_max_output_length (GMimeHtmlToText *converter)
{
	g_return_val_if_fail (GMIME_IS_HTML_TO_TEXT (converter), 0);

	return converter->output_limit;
}

/**
 * g_mime_html_to_text_set_max_output_length:
 * @converter: a #GMimeHtmlToText
 * @value: the maximum output length in Unicode scalar values, greater than zero
 *
 * Limits the rendered body text before output charset encoding. Headers
 * and footers are excluded. Exceeding the limit causes conversion to fail
 * with an error.
 **/
void
g_mime_html_to_text_set_max_output_length (GMimeHtmlToText *converter, gsize value)
{
	g_return_if_fail (GMIME_IS_HTML_TO_TEXT (converter));
	g_return_if_fail (value > 0);

	converter->output_limit = value;
}
