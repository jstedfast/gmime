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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <errno.h>
#include <string.h>

#include "gmime-rtf-to-html.h"
#include "gmime-rtf-compliance-private.h"
#include "gmime-rtf-interpreter-private.h"
#include "gmime-text-private.h"

/**
 * SECTION: gmime-rtf-to-html
 * @title: Rich text to HTML
 * @short_description: Render RTF or extract its encapsulated HTML
 *
 * Renders character and paragraph formatting, tables, links and attachment
 * placeholders. Outlook HTML encapsulation is detected before rendering.
 * The extracted text is fed through #GMimeHtmlToHtml so filtering, fragment
 * output and noscript handling work as for a standalone HTML document.
 * Neither converter is an HTML sanitizer. Compliance checks apply to RTF,
 * not the embedded HTML. Raw HTML headers and footers bypass callbacks.
 **/

struct _GMimeRtfToHtml {
	GMimeTextConverter parent_object;
	gboolean extract, fragment;
	guint max_groups, max_fonts, max_colors, max_elements;
	GMimeHeaderFooterFormat header_format, footer_format;
	GMimeHtmlNoScriptHandling noscript;
	GMimeHtmlTagCallback tag_callback;
	gpointer tag_data;
	GDestroyNotify tag_destroy;
	GMimeRtfToHtmlAttachmentPlaceholderCallback attachment_callback;
	gpointer attachment_data;
	GDestroyNotify attachment_destroy;
	GMimeRtfComplianceLogger logger;
	gpointer logger_data;
	GDestroyNotify logger_destroy;
	GMimeRtfComplianceOptions *compliance;
};

G_DEFINE_FINAL_TYPE (GMimeRtfToHtml, g_mime_rtf_to_html, GMIME_TYPE_TEXT_CONVERTER)

typedef struct {
	guint decorations;
	GMimeRtfVerticalAlignment vertical;
	int size, foreground, background;
} SpanFormat;

enum {
	BOLD = 1, ITALIC = 2, UNDERLINE = 4, STRIKE = 8
};

typedef struct {
	GMimeHtmlTagId id;
	GMimeHtmlTagContext *context;
} RenderTag;

typedef struct {
	GMimeRtfToHtml *options;
	GMimeHtmlWriter *writer;
	/* At most one of each generated element, regardless of RTF nesting. */
	RenderTag tags[6];
	int count, placeholder_index;
	SpanFormat span;
	GHashTable *styles;
	char *href;
	gboolean last_was_space;
	gboolean begun, extracting, allow_extract;
	GString *extracted;
} RenderState;

static int
find_tag (RenderState *state, GMimeHtmlTagId id)
{
	for (int i = state->count - 1; i >= 0; i--) {
		if (state->tags[i].id == id)
			return i;
	}

	return -1;
}

static gboolean
suppressed (RenderState *state)
{
	for (int i = 0; i < state->count; i++) {
		if (g_mime_html_tag_context_get_suppress_inner_content (state->tags[i].context))
			return TRUE;
	}

	return FALSE;
}

static gboolean
write_tag (RenderState *state, GMimeHtmlTagContext *context, GError **error)
{
	GMimeRtfToHtml *options = state->options;

	if (!options->tag_callback)
		return g_mime_html_tag_context_write_tag (context, state->writer, TRUE, error);

	if (options->tag_callback (context, state->writer, options->tag_data, error))
		return TRUE;

	if (error && !*error)
		g_set_error_literal (error, G_MARKUP_ERROR, G_MARKUP_ERROR_INVALID_CONTENT, "The HTML tag callback failed.");

	return FALSE;
}

static gboolean
open_tag (RenderState *state, GMimeHtmlTagId id, const char *name, const char *value, GError **error)
{
	GMimeHtmlAttribute *attr = name ? g_mime_html_attribute_new (name, value) : NULL;
	GMimeHtmlToken *token = g_mime_html_token_new_tag (g_mime_html_tag_id_to_name (id), FALSE,
							  id == GMIME_HTML_TAG_BR, attr ? &attr : NULL, attr ? 1 : 0);
	GMimeHtmlTagContext *context = g_mime_html_tag_context_new (token);
	gboolean ok = suppressed (state) || write_tag (state, context, error);

	g_mime_html_token_unref (token);

	if (attr)
		g_mime_html_attribute_unref (attr);

	if (ok && id != GMIME_HTML_TAG_BR) {
		g_assert (state->count < (int) G_N_ELEMENTS (state->tags));
		state->tags[state->count].id = id;
		state->tags[state->count++].context = context;
	} else {
		g_mime_html_tag_context_free (context);
	}

	return ok;
}

static gboolean
close_to (RenderState *state, int count, GError **error)
{
	while (state->count > count) {
		RenderTag *tag = &state->tags[--state->count];
		GMimeHtmlTagContext *ctx = tag->context;
		gboolean ok = TRUE;

		if (tag->id == GMIME_HTML_TAG_A)
			g_clear_pointer (&state->href, g_free);

		if (!suppressed (state) && !g_mime_html_tag_context_get_delete_end_tag (ctx)) {
			g_mime_html_tag_context_set_is_end_tag (ctx, TRUE);

			if (g_mime_html_tag_context_get_invoke_callback_for_end_tag (ctx))
				ok = write_tag (state, ctx, error);
			else
				ok = g_mime_html_tag_context_write_tag (ctx, state->writer, FALSE, error);

			if (ok && (tag->id == GMIME_HTML_TAG_DIV || tag->id == GMIME_HTML_TAG_TR ||
				   tag->id == GMIME_HTML_TAG_TABLE))
				ok = g_mime_html_writer_write_markup_text (state->writer, "\n", 1, error);
		}

		g_mime_html_tag_context_free (ctx);
		if (!ok)
			return FALSE;
	}

	return TRUE;
}

static gboolean
close_tag (RenderState *state, GMimeHtmlTagId id, GError **error)
{
	int index = find_tag (state, id);

	return index < 0 || close_to (state, index, error);
}

static gboolean
open_table (RenderState *state, gboolean cell, GError **error)
{
	if (find_tag (state, GMIME_HTML_TAG_TABLE) < 0 && !open_tag (state, GMIME_HTML_TAG_TABLE, NULL, NULL, error))
		return FALSE;

	if (find_tag (state, GMIME_HTML_TAG_TR) < 0 && !open_tag (state, GMIME_HTML_TAG_TR, NULL, NULL, error))
		return FALSE;

	return !cell || find_tag (state, GMIME_HTML_TAG_TD) >= 0 || open_tag (state, GMIME_HTML_TAG_TD, NULL, NULL, error);
}

static gboolean
open_block (RenderState *state, const GMimeRtfFormat *format, GError **error)
{
	const char *style;

	if (format->in_table) {
		if (!open_table (state, TRUE, error))
			return FALSE;
	} else if (!close_tag (state, GMIME_HTML_TAG_TABLE, error)) {
		return FALSE;
	}

	if (find_tag (state, GMIME_HTML_TAG_DIV) >= 0)
		return TRUE;

	switch (format->alignment) {
	case GMIME_RTF_ALIGN_CENTER:  style = "text-align: center;"; break;
	case GMIME_RTF_ALIGN_RIGHT:   style = "text-align: right;"; break;
	case GMIME_RTF_ALIGN_JUSTIFY: style = "text-align: justify;"; break;
	default:                      style = NULL; break;
	}

	state->last_was_space = TRUE;

	return open_tag (state, GMIME_HTML_TAG_DIV, style ? "style" : NULL, style, error);
}

static gboolean
same_span (gconstpointer key1, gconstpointer key2)
{
	const SpanFormat *a = key1, *b = key2;

	return a->decorations == b->decorations && a->vertical == b->vertical && a->size == b->size &&
		a->foreground == b->foreground && a->background == b->background;
}

static guint
span_hash (gconstpointer key)
{
	const SpanFormat *span = key;
	guint hash = span->decorations * 31 + (guint) span->vertical;

	hash = hash * 31 + (guint) span->size;
	hash = hash * 31 + (guint) span->foreground;

	return hash * 31 + (guint) span->background;
}

static char *
span_style (const SpanFormat *span)
{
	GString *style = g_string_sized_new (128);

	if (span->decorations & BOLD)
		g_string_append (style, "font-weight: bold; ");

	if (span->decorations & ITALIC)
		g_string_append (style, "font-style: italic; ");

	if (span->decorations & (UNDERLINE | STRIKE)) {
		g_string_append (style, "text-decoration:");

		if (span->decorations & UNDERLINE)
			g_string_append (style, " underline");

		if (span->decorations & STRIKE)
			g_string_append (style, " line-through");

		g_string_append (style, "; ");
	}

	if (span->vertical == GMIME_RTF_SUPERSCRIPT)
		g_string_append (style, "vertical-align: super; ");
	else if (span->vertical == GMIME_RTF_SUBSCRIPT)
		g_string_append (style, "vertical-align: sub; ");

	if (span->size != 24)
		g_string_append_printf (style, "font-size: %d%spt; ", span->size / 2, span->size & 1 ? ".5" : "");

	if (span->foreground != -1)
		g_string_append_printf (style, "color: #%06X; ", (guint) span->foreground);

	if (span->background != -1)
		g_string_append_printf (style, "background-color: #%06X; ", (guint) span->background);

	g_string_truncate (style, style->len - 1);

	return g_string_free (style, FALSE);
}

static const char *
get_style (RenderState *state, const SpanFormat *span)
{
	SpanFormat *key;
	char *style;

	if (!state->styles)
		state->styles = g_hash_table_new_full (span_hash, same_span, g_free, g_free);
	else if ((style = g_hash_table_lookup (state->styles, span)) != NULL)
		return style;

	/* Documents commonly reuse a handful of formats across many paragraphs. */
	if (g_hash_table_size (state->styles) >= 256)
		g_hash_table_remove_all (state->styles);

	key = g_new (SpanFormat, 1);
	*key = *span;

	style = span_style (span);

	g_hash_table_insert (state->styles, key, style);

	return style;
}

static gboolean
open_inline (RenderState *state, const GMimeRtfFormat *format, GError **error)
{
	const SpanFormat plain = { 0, GMIME_RTF_BASELINE, 24, -1, -1 };
	SpanFormat span = {
		(format->bold ? BOLD : 0) | (format->italic ? ITALIC : 0) |
		(format->underline ? UNDERLINE : 0) | (format->strike ? STRIKE : 0),
		format->vertical_alignment, format->font_size > 0 ? format->font_size : 24,
		format->foreground, format->background
	};

	if (state->href && g_strcmp0 (state->href, format->hyperlink) != 0) {
		if (!close_tag (state, GMIME_HTML_TAG_A, error))
			return FALSE;
	}

	if (find_tag (state, GMIME_HTML_TAG_SPAN) >= 0 && !same_span (&state->span, &span)) {
		if (!close_tag (state, GMIME_HTML_TAG_SPAN, error))
			return FALSE;
	}

	if (format->hyperlink && !state->href) {
		if (!open_tag (state, GMIME_HTML_TAG_A, "href", format->hyperlink, error))
			return FALSE;
		state->href = g_strdup (format->hyperlink);
	}

	if (!same_span (&plain, &span) && find_tag (state, GMIME_HTML_TAG_SPAN) < 0) {
		state->span = span;
		return open_tag (state, GMIME_HTML_TAG_SPAN, "style", get_style (state, &span), error);
	}

	return TRUE;
}

/* Alternating ordinary and nonbreaking spaces preserves indentation while
 * still allowing line wrapping. Keep the phase across interpreter events. */
static gboolean
write_text (RenderState *state, const char *text, gsize length, GError **error)
{
	static const char tabs[] = "\xc2\xa0\xc2\xa0\xc2\xa0\xc2\xa0";
	gsize start = 0;

	for (gsize i = 0; i < length; i++) {
		if (!text[i]) {
			/* RTF can produce U+0000; omit it from HTML text. */
			if (i > start && !g_mime_html_writer_write_text (state->writer, text + start, i - start, error))
				return FALSE;

			start = i + 1;
		} else if (text[i] == '\t' || (text[i] == ' ' && state->last_was_space)) {
			if ((i > start && !g_mime_html_writer_write_text (state->writer, text + start, i - start, error)) ||
			    !g_mime_html_writer_write_text (state->writer, tabs, text[i] == '\t' ? 8 : 2, error))
				return FALSE;

			start = i + 1;
			state->last_was_space = FALSE;
		} else {
			state->last_was_space = text[i] == ' ';
		}
	}

	return start == length || g_mime_html_writer_write_text (state->writer, text + start, length - start, error);
}

static gboolean
handle_event (GMimeRtfEvent event, const GMimeRtfFormat *format, const char *text, gsize length,
	      gpointer user_data, GError **error)
{
	RenderState *state = user_data;
	GMimeRtfToHtml *options = state->options;

	if (event == GMIME_RTF_EVENT_BEGIN) {
		state->begun = TRUE;
		state->extracting = state->allow_extract && format->from_html;

		if (state->extracting)
			return TRUE;

		return (options->fragment || g_mime_html_writer_write_markup_text (state->writer, "<html><body>", -1, error)) &&
			_g_mime_html_output_header_footer (state->writer,
				g_mime_text_converter_get_header (GMIME_TEXT_CONVERTER (options)), options->header_format, error);
	}

	if (state->extracting) {
		/* Attachment placeholders belong to the RTF rendition
		 * ([MS-OXRTFEX] 2.2.3.4). The HTML has its own references, e.g. cid: URLs. */
		if (event == GMIME_RTF_EVENT_TEXT || event == GMIME_RTF_EVENT_HTML_TAG)
			g_string_append_len (state->extracted, text, length);
		else if (event == GMIME_RTF_EVENT_PARAGRAPH || event == GMIME_RTF_EVENT_LINE_BREAK)
			g_string_append_len (state->extracted, "\r\n", 2);

		return TRUE;
	}

	switch (event) {
	case GMIME_RTF_EVENT_TEXT:
		return open_block (state, format, error) && open_inline (state, format, error) &&
			(suppressed (state) || write_text (state, text, length, error));
	case GMIME_RTF_EVENT_PARAGRAPH: {
		gboolean empty = find_tag (state, GMIME_HTML_TAG_DIV) < 0;

		return open_block (state, format, error) &&
			(!empty || open_tag (state, GMIME_HTML_TAG_BR, NULL, NULL, error)) &&
			close_tag (state, GMIME_HTML_TAG_DIV, error);
	}
	case GMIME_RTF_EVENT_LINE_BREAK:
		if (!open_block (state, format, error) || !open_inline (state, format, error))
			return FALSE;

		state->last_was_space = TRUE;

		return open_tag (state, GMIME_HTML_TAG_BR, NULL, NULL, error);
	case GMIME_RTF_EVENT_CELL:
		return open_table (state, TRUE, error) && close_tag (state, GMIME_HTML_TAG_TD, error);
	case GMIME_RTF_EVENT_ROW:
		return open_table (state, FALSE, error) && close_tag (state, GMIME_HTML_TAG_TR, error);
	case GMIME_RTF_EVENT_OBJECT_PLACEHOLDER: {
		int index = state->placeholder_index;

		if (index < G_MAXINT)
			state->placeholder_index++;

		if (!options->attachment_callback)
			return TRUE;

		if (!open_block (state, format, error) || !open_inline (state, format, error))
			return FALSE;

		state->last_was_space = FALSE;

		if (suppressed (state) || options->attachment_callback (index, state->writer, options->attachment_data, error))
			return TRUE;

		if (error && !*error)
			g_set_error_literal (error, G_MARKUP_ERROR, G_MARKUP_ERROR_INVALID_CONTENT,
					     "The attachment placeholder callback failed.");

		return FALSE;
	}
	default:
		return TRUE;
	}
}

/* Pull adapter for the existing HTML tokenizer. A step may deliver more than
 * one event, but its output is bounded by the size of one interpreter token.
 * Drain that output before advancing again; never accumulate a document. */
typedef struct {
	GMimeStream parent_object;
	GMimeRtfInterpreter *interpreter;
	GString *pending;
	gsize position;
	gboolean eof;
	GError *error;
} EncapsulatedHtmlStream;

typedef GMimeStreamClass EncapsulatedHtmlStreamClass;

G_DEFINE_FINAL_TYPE (EncapsulatedHtmlStream, encapsulated_html_stream, GMIME_TYPE_STREAM)

static ssize_t
extracted_read (GMimeStream *base, char *buffer, size_t length)
{
	EncapsulatedHtmlStream *self = (EncapsulatedHtmlStream *) base;
	gsize n;

	while (self->position == self->pending->len) {
		if (self->error) {
			errno = EIO;
			return -1;
		}

		if (self->eof)
			return 0;

		g_string_truncate (self->pending, 0);
		self->position = 0;

		if (!_g_mime_rtf_interpreter_step (self->interpreter, &self->eof, &self->error)) {
			errno = EIO;
			return -1;
		}
	}

	n = MIN (length, self->pending->len - self->position);
	memcpy (buffer, self->pending->str + self->position, n);
	self->position += n;

	return (ssize_t) n;
}

static gboolean
extracted_eos (GMimeStream *base)
{
	EncapsulatedHtmlStream *self = (EncapsulatedHtmlStream *) base;

	return self->eof && self->position == self->pending->len;
}

static void
encapsulated_html_stream_class_init (EncapsulatedHtmlStreamClass *klass)
{
	klass->read = extracted_read;
	klass->eos = extracted_eos;
}

static void
encapsulated_html_stream_init (EncapsulatedHtmlStream *stream)
{
	g_mime_stream_construct (GMIME_STREAM (stream), 0, -1);
}

static gboolean
convert_extracted (RenderState *state, GMimeRtfInterpreter *interpreter, gboolean eof,
		   GMimeStream *output, GError **error)
{
	EncapsulatedHtmlStream *stream = g_object_new (encapsulated_html_stream_get_type (), NULL);
	GMimeTextConverter *html = g_mime_html_to_html_new ();
	GMimeRtfToHtml *options = state->options;
	GMimeTextConverter *base = GMIME_TEXT_CONVERTER (options);
	GMimeHtmlToHtml *filter = GMIME_HTML_TO_HTML (html);
	GError *html_error = NULL;
	gboolean ok;

	stream->interpreter = interpreter;
	stream->pending = state->extracted;
	stream->eof = eof;

	g_mime_text_converter_set_header (html, g_mime_text_converter_get_header (base));
	g_mime_text_converter_set_footer (html, g_mime_text_converter_get_footer (base));
	g_mime_html_to_html_set_header_format (filter, options->header_format);
	g_mime_html_to_html_set_footer_format (filter, options->footer_format);
	g_mime_html_to_html_set_output_html_fragment (filter, options->fragment);
	g_mime_html_to_html_set_no_script_handling (filter, options->noscript);
	g_mime_html_to_html_set_max_element_depth (filter, options->max_elements);
	g_mime_html_to_html_set_html_tag_callback (filter, options->tag_callback, options->tag_data, NULL);

	/* Already UTF-8: charset conversion and flushing belong to the outer converter. */
	ok = GMIME_TEXT_CONVERTER_GET_CLASS (html)->convert (html, GMIME_STREAM (stream), output, &html_error);

	if (stream->error) {
		g_clear_error (&html_error);
		g_propagate_error (error, g_steal_pointer (&stream->error));
		ok = FALSE;
	} else if (html_error) {
		g_propagate_error (error, html_error);
	}

	g_object_unref (html);
	g_object_unref (stream);

	return ok;
}

static gboolean
rtf_to_html_convert (GMimeTextConverter *base, GMimeStream *input, GMimeStream *output, GError **error)
{
	GMimeRtfToHtml *options = GMIME_RTF_TO_HTML (base);
	GMimeRtfDiagnostics diagnostics;
	GMimeRtfInterpreter *interpreter;
	RenderState state = { 0 };
	gboolean eof = FALSE, ok = TRUE;

	state.options = options;
	state.allow_extract = options->extract;
	state.writer = g_mime_html_writer_new (output);
	state.extracted = g_string_sized_new (256);
	_g_mime_rtf_diagnostics_init (&diagnostics, options->compliance, options->logger, options->logger_data);
	interpreter = _g_mime_rtf_interpreter_new (input, state.allow_extract, options->max_groups, options->max_fonts,
						  options->max_colors, &diagnostics, handle_event, &state);

	while (!eof && !state.begun && ok)
		ok = _g_mime_rtf_interpreter_step (interpreter, &eof, error);

	if (ok && state.extracting) {
		ok = convert_extracted (&state, interpreter, eof, output, error);
	} else if (ok) {
		while (!eof && ok)
			ok = _g_mime_rtf_interpreter_step (interpreter, &eof, error);

		ok = ok && close_to (&state, 0, error) &&
			_g_mime_html_output_header_footer (state.writer, g_mime_text_converter_get_footer (base),
							   options->footer_format, error) &&
			(options->fragment || g_mime_html_writer_write_markup_text (state.writer, "</body></html>", -1, error)) &&
			g_mime_html_writer_finish (state.writer, error);
	}

	for (int i = 0; i < state.count; i++)
		g_mime_html_tag_context_free (state.tags[i].context);

	g_clear_pointer (&state.styles, g_hash_table_unref);
	g_free (state.href);
	g_object_unref (state.writer);
	g_string_free (state.extracted, TRUE);
	_g_mime_rtf_interpreter_free (interpreter);

	return ok;
}

static void
rtf_to_html_finalize (GObject *object)
{
	GMimeRtfToHtml *self = GMIME_RTF_TO_HTML (object);

	if (self->tag_destroy)
		self->tag_destroy (self->tag_data);
	if (self->attachment_destroy)
		self->attachment_destroy (self->attachment_data);
	if (self->logger_destroy)
		self->logger_destroy (self->logger_data);
	g_mime_rtf_compliance_options_unref (self->compliance);

	G_OBJECT_CLASS (g_mime_rtf_to_html_parent_class)->finalize (object);
}

typedef struct {
	gboolean allow_extract, begun, extracts;
	int count;
} PlaceholderCount;

static gboolean
count_event (GMimeRtfEvent event, const GMimeRtfFormat *format, const char *text, gsize length,
	     gpointer user_data, GError **error)
{
	PlaceholderCount *count = user_data;

	if (event == GMIME_RTF_EVENT_BEGIN) {
		count->begun = TRUE;
		count->extracts = count->allow_extract && format->from_html;
	} else if (event == GMIME_RTF_EVENT_OBJECT_PLACEHOLDER && count->count < G_MAXINT) {
		count->count++;
	}

	return TRUE;
}

/* Preflight for TNEF attachment matching. Use the same recognition window,
 * visibility rules and limits as the renderer, without invoking user callbacks
 * or reporting the same diagnostics twice. Input is a decoded reader stream. */
static gboolean
preflight (GMimeRtfToHtml *converter, GMimeStream *input, gboolean count_all, PlaceholderCount *count, GError **error)
{
	GMimeRtfInterpreter *interpreter;
	gboolean eof = FALSE, ok;

	count->allow_extract = converter->extract;
	interpreter = _g_mime_rtf_interpreter_new (input, converter->extract, converter->max_groups, converter->max_fonts,
						  converter->max_colors, NULL, count_event, count);
	do {
		ok = _g_mime_rtf_interpreter_step (interpreter, &eof, error);
	} while (ok && !eof && !count->extracts && (!count->begun || count_all));

	_g_mime_rtf_interpreter_free (interpreter);

	return ok;
}

gboolean
_g_mime_rtf_to_html_count_object_placeholders (GMimeRtfToHtml *converter, GMimeStream *input, int *count, GError **error)
{
	PlaceholderCount result = { 0 };

	if (!preflight (converter, input, TRUE, &result, error))
		return FALSE;

	*count = result.extracts ? -1 : result.count;

	return TRUE;
}

gboolean
_g_mime_rtf_to_html_extracts_encapsulated_html (GMimeRtfToHtml *converter, GMimeStream *input,
					       gboolean *extracts, GError **error)
{
	PlaceholderCount result = { 0 };

	if (!preflight (converter, input, FALSE, &result, error))
		return FALSE;

	*extracts = result.extracts;

	return TRUE;
}

static void
g_mime_rtf_to_html_class_init (GMimeRtfToHtmlClass *klass)
{
	GMimeTextConverterClass *base = GMIME_TEXT_CONVERTER_CLASS (klass);

	base->input_format = GMIME_TEXT_FORMAT_RICH_TEXT;
	base->output_format = GMIME_TEXT_FORMAT_HTML;
	base->convert = rtf_to_html_convert;

	G_OBJECT_CLASS (klass)->finalize = rtf_to_html_finalize;
}

static void
g_mime_rtf_to_html_init (GMimeRtfToHtml *self)
{
	self->extract = TRUE;
	self->max_groups = self->max_fonts = self->max_colors = self->max_elements = 4096;
	self->compliance = g_mime_rtf_compliance_options_new ();

	g_mime_text_converter_set_input_charset (GMIME_TEXT_CONVERTER (self), "iso-8859-1", NULL);
}

/**
 * g_mime_rtf_to_html_new:
 *
 * Creates a rich text converter. The default ISO-8859-1 input mapping
 * preserves each wire byte; character decoding follows the RTF code page.
 *
 * Returns: (transfer full): a new #GMimeRtfToHtml
 **/
GMimeTextConverter *
g_mime_rtf_to_html_new (void)
{
	return g_object_new (GMIME_TYPE_RTF_TO_HTML, NULL);
}

/**
 * g_mime_rtf_to_html_get_extract_encapsulated_html:
 * @converter: a #GMimeRtfToHtml
 *
 * Returns: whether to extract the original HTML when available (default %TRUE)
 **/
gboolean
g_mime_rtf_to_html_get_extract_encapsulated_html (GMimeRtfToHtml *converter)
{
	g_return_val_if_fail (GMIME_IS_RTF_TO_HTML (converter), FALSE);

	return converter->extract;
}

/**
 * g_mime_rtf_to_html_set_extract_encapsulated_html:
 * @converter: a #GMimeRtfToHtml
 * @value: whether to extract encapsulated HTML
 *
 * If disabled, renders the RTF representation even in documents declaring
 * `\fromhtml1`. Extracted HTML is rewritten, not sanitized.
 **/
void
g_mime_rtf_to_html_set_extract_encapsulated_html (GMimeRtfToHtml *converter, gboolean value)
{
	g_return_if_fail (GMIME_IS_RTF_TO_HTML (converter));

	converter->extract = !!value;
}

/**
 * g_mime_rtf_to_html_get_output_html_fragment:
 * @converter: a #GMimeRtfToHtml
 *
 * Returns: whether to produce only a body fragment (default %FALSE)
 **/
gboolean
g_mime_rtf_to_html_get_output_html_fragment (GMimeRtfToHtml *converter)
{
	g_return_val_if_fail (GMIME_IS_RTF_TO_HTML (converter), FALSE);

	return converter->fragment;
}

/**
 * g_mime_rtf_to_html_set_output_html_fragment:
 * @converter: a #GMimeRtfToHtml
 * @value: whether to omit document wrappers
 *
 * Fragment extraction also omits the doctype and head contents.
 **/
void
g_mime_rtf_to_html_set_output_html_fragment (GMimeRtfToHtml *converter, gboolean value)
{
	g_return_if_fail (GMIME_IS_RTF_TO_HTML (converter));

	converter->fragment = !!value;
}

/**
 * g_mime_rtf_to_html_get_header_format:
 * @converter: a #GMimeRtfToHtml
 *
 * Returns: the header format, initially %GMIME_HEADER_FOOTER_FORMAT_TEXT
 **/
GMimeHeaderFooterFormat
g_mime_rtf_to_html_get_header_format (GMimeRtfToHtml *converter)
{
	g_return_val_if_fail (GMIME_IS_RTF_TO_HTML (converter), GMIME_HEADER_FOOTER_FORMAT_TEXT);

	return converter->header_format;
}

/**
 * g_mime_rtf_to_html_set_header_format:
 * @converter: a #GMimeRtfToHtml
 * @value: plain text or raw HTML
 *
 * Determines how to write the configured header. HTML bypasses filtering.
 **/
void
g_mime_rtf_to_html_set_header_format (GMimeRtfToHtml *converter, GMimeHeaderFooterFormat value)
{
	g_return_if_fail (GMIME_IS_RTF_TO_HTML (converter));
	g_return_if_fail (value == GMIME_HEADER_FOOTER_FORMAT_TEXT || value == GMIME_HEADER_FOOTER_FORMAT_HTML);

	converter->header_format = value;
}

/**
 * g_mime_rtf_to_html_get_footer_format:
 * @converter: a #GMimeRtfToHtml
 *
 * Returns: the footer format, initially %GMIME_HEADER_FOOTER_FORMAT_TEXT
 **/
GMimeHeaderFooterFormat
g_mime_rtf_to_html_get_footer_format (GMimeRtfToHtml *converter)
{
	g_return_val_if_fail (GMIME_IS_RTF_TO_HTML (converter), GMIME_HEADER_FOOTER_FORMAT_TEXT);

	return converter->footer_format;
}

/**
 * g_mime_rtf_to_html_set_footer_format:
 * @converter: a #GMimeRtfToHtml
 * @value: plain text or raw HTML
 *
 * Determines how to write the configured footer. HTML bypasses filtering.
 **/
void
g_mime_rtf_to_html_set_footer_format (GMimeRtfToHtml *converter, GMimeHeaderFooterFormat value)
{
	g_return_if_fail (GMIME_IS_RTF_TO_HTML (converter));
	g_return_if_fail (value == GMIME_HEADER_FOOTER_FORMAT_TEXT || value == GMIME_HEADER_FOOTER_FORMAT_HTML);

	converter->footer_format = value;
}

/**
 * g_mime_rtf_to_html_get_no_script_handling:
 * @converter: a #GMimeRtfToHtml
 *
 * Returns: the noscript mode, initially %GMIME_HTML_NO_SCRIPT_HANDLING_UNWRAP
 **/
GMimeHtmlNoScriptHandling
g_mime_rtf_to_html_get_no_script_handling (GMimeRtfToHtml *converter)
{
	g_return_val_if_fail (GMIME_IS_RTF_TO_HTML (converter), GMIME_HTML_NO_SCRIPT_HANDLING_UNWRAP);

	return converter->noscript;
}

/**
 * g_mime_rtf_to_html_set_no_script_handling:
 * @converter: a #GMimeRtfToHtml
 * @value: noscript handling mode for encapsulated HTML
 *
 * Unwrap mode parses noscript contents as markup and omits the tags.
 * In the other modes the renderer's scripting setting must match @value.
 **/
void
g_mime_rtf_to_html_set_no_script_handling (GMimeRtfToHtml *converter, GMimeHtmlNoScriptHandling value)
{
	g_return_if_fail (GMIME_IS_RTF_TO_HTML (converter));
	g_return_if_fail (value >= GMIME_HTML_NO_SCRIPT_HANDLING_UNWRAP &&
			  value <= GMIME_HTML_NO_SCRIPT_HANDLING_SCRIPTING_DISABLED);

	converter->noscript = value;
}

/**
 * g_mime_rtf_to_html_get_max_group_depth:
 * @converter: a #GMimeRtfToHtml
 *
 * Returns: the maximum number of distinct saved group states (default 4096)
 **/
guint
g_mime_rtf_to_html_get_max_group_depth (GMimeRtfToHtml *converter)
{
	g_return_val_if_fail (GMIME_IS_RTF_TO_HTML (converter), 0);

	return converter->max_groups;
}

/**
 * g_mime_rtf_to_html_set_max_group_depth:
 * @converter: a #GMimeRtfToHtml
 * @value: maximum number of distinct group states (at least 1)
 *
 * Content in groups exceeding this limit is discarded.
 **/
void
g_mime_rtf_to_html_set_max_group_depth (GMimeRtfToHtml *converter, guint value)
{
	g_return_if_fail (GMIME_IS_RTF_TO_HTML (converter));
	g_return_if_fail (value > 0);

	converter->max_groups = value;
}

/**
 * g_mime_rtf_to_html_get_max_font_table_entries:
 * @converter: a #GMimeRtfToHtml
 *
 * Returns: the maximum number of saved font entries (default 4096)
 **/
guint
g_mime_rtf_to_html_get_max_font_table_entries (GMimeRtfToHtml *converter)
{
	g_return_val_if_fail (GMIME_IS_RTF_TO_HTML (converter), 0);

	return converter->max_fonts;
}

/**
 * g_mime_rtf_to_html_set_max_font_table_entries:
 * @converter: a #GMimeRtfToHtml
 * @value: maximum number of font entries (at least 1)
 *
 * References to excess entries use the document's default code page.
 **/
void
g_mime_rtf_to_html_set_max_font_table_entries (GMimeRtfToHtml *converter, guint value)
{
	g_return_if_fail (GMIME_IS_RTF_TO_HTML (converter));
	g_return_if_fail (value > 0);

	converter->max_fonts = value;
}

/**
 * g_mime_rtf_to_html_get_max_color_table_entries:
 * @converter: a #GMimeRtfToHtml
 *
 * Returns: the maximum number of saved color entries (default 4096)
 **/
guint
g_mime_rtf_to_html_get_max_color_table_entries (GMimeRtfToHtml *converter)
{
	g_return_val_if_fail (GMIME_IS_RTF_TO_HTML (converter), 0);

	return converter->max_colors;
}

/**
 * g_mime_rtf_to_html_set_max_color_table_entries:
 * @converter: a #GMimeRtfToHtml
 * @value: maximum number of color entries (at least 1)
 *
 * References to excess entries are rendered using the default color.
 **/
void
g_mime_rtf_to_html_set_max_color_table_entries (GMimeRtfToHtml *converter, guint value)
{
	g_return_if_fail (GMIME_IS_RTF_TO_HTML (converter));
	g_return_if_fail (value > 0);

	converter->max_colors = value;
}

/**
 * g_mime_rtf_to_html_get_max_element_depth:
 * @converter: a #GMimeRtfToHtml
 *
 * Returns: the saved element depth limit for encapsulated HTML (default 4096)
 **/
guint
g_mime_rtf_to_html_get_max_element_depth (GMimeRtfToHtml *converter)
{
	g_return_val_if_fail (GMIME_IS_RTF_TO_HTML (converter), 0);

	return converter->max_elements;
}

/**
 * g_mime_rtf_to_html_set_max_element_depth:
 * @converter: a #GMimeRtfToHtml
 * @value: maximum element depth (at least 1)
 *
 * Sets the #GMimeHtmlToHtml depth limit for encapsulated HTML. Once it
 * is exceeded the remaining markup is treated as literal text.
 **/
void
g_mime_rtf_to_html_set_max_element_depth (GMimeRtfToHtml *converter, guint value)
{
	g_return_if_fail (GMIME_IS_RTF_TO_HTML (converter));
	g_return_if_fail (value > 0);

	converter->max_elements = value;
}

/**
 * g_mime_rtf_to_html_set_html_tag_callback:
 * @converter: a #GMimeRtfToHtml
 * @callback: (nullable) (scope notified) (closure user_data) (destroy destroy): tag writer
 * @user_data: (nullable): data passed to @callback
 * @destroy: (nullable): destroy notifier for @user_data
 *
 * The callback must explicitly write each desired tag. It is not invoked
 * for generated document wrappers, configured headers/footers or attachment
 * callback output. Pass %NULL to restore normal tag output. The registration
 * is destroyed when replaced or finalized, and cannot change during conversion.
 **/
void
g_mime_rtf_to_html_set_html_tag_callback (GMimeRtfToHtml *converter, GMimeHtmlTagCallback callback,
					 gpointer user_data, GDestroyNotify destroy)
{
	g_return_if_fail (GMIME_IS_RTF_TO_HTML (converter));
	g_return_if_fail (!_g_mime_text_converter_is_active (GMIME_TEXT_CONVERTER (converter)));

	if (converter->tag_destroy)
		converter->tag_destroy (converter->tag_data);

	converter->tag_callback = callback;
	converter->tag_data = user_data;
	converter->tag_destroy = destroy;
}

/**
 * g_mime_rtf_to_html_get_html_tag_callback: (skip)
 * @converter: a #GMimeRtfToHtml
 * @user_data: (out) (optional) (nullable) (transfer none): receives the borrowed callback data
 *
 * Returns: (nullable): the HTML tag callback
 **/
GMimeHtmlTagCallback
g_mime_rtf_to_html_get_html_tag_callback (GMimeRtfToHtml *converter, gpointer *user_data)
{
	g_return_val_if_fail (GMIME_IS_RTF_TO_HTML (converter), NULL);

	if (user_data)
		*user_data = converter->tag_data;

	return converter->tag_callback;
}

/**
 * g_mime_rtf_to_html_set_attachment_placeholder_callback:
 * @converter: a #GMimeRtfToHtml
 * @callback: (nullable) (scope notified) (closure user_data) (destroy destroy): placeholder writer
 * @user_data: (nullable): callback data
 * @destroy: (nullable): destroy notifier for @user_data
 *
 * Registers a writer for rendered attachment placeholders. If %NULL, no
 * replacement is written. Like tag callbacks, this registration cannot
 * change during conversion and is destroyed when replaced or finalized.
 **/
void
g_mime_rtf_to_html_set_attachment_placeholder_callback (GMimeRtfToHtml *converter,
						       GMimeRtfToHtmlAttachmentPlaceholderCallback callback,
						       gpointer user_data, GDestroyNotify destroy)
{
	g_return_if_fail (GMIME_IS_RTF_TO_HTML (converter));
	g_return_if_fail (!_g_mime_text_converter_is_active (GMIME_TEXT_CONVERTER (converter)));

	if (converter->attachment_destroy)
		converter->attachment_destroy (converter->attachment_data);

	converter->attachment_callback = callback;
	converter->attachment_data = user_data;
	converter->attachment_destroy = destroy;
}

/**
 * g_mime_rtf_to_html_get_attachment_placeholder_callback: (skip)
 * @converter: a #GMimeRtfToHtml
 * @user_data: (out) (optional) (nullable) (transfer none): receives the borrowed callback data
 *
 * Returns: (nullable): the attachment placeholder callback
 **/
GMimeRtfToHtmlAttachmentPlaceholderCallback
g_mime_rtf_to_html_get_attachment_placeholder_callback (GMimeRtfToHtml *converter, gpointer *user_data)
{
	g_return_val_if_fail (GMIME_IS_RTF_TO_HTML (converter), NULL);

	if (user_data)
		*user_data = converter->attachment_data;

	return converter->attachment_callback;
}

/**
 * g_mime_rtf_to_html_set_compliance_logger:
 * @converter: a #GMimeRtfToHtml
 * @logger: (nullable) (scope notified) (closure user_data) (destroy destroy): diagnostic callback
 * @user_data: (nullable): logger data
 * @destroy: (nullable): destroy notifier for @user_data
 *
 * Reports RTF diagnostics during conversion. Encapsulated HTML markup
 * is not checked. Pass %NULL to disable reporting. The registration cannot
 * change during conversion and is destroyed when replaced or finalized.
 **/
void
g_mime_rtf_to_html_set_compliance_logger (GMimeRtfToHtml *converter, GMimeRtfComplianceLogger logger,
					 gpointer user_data, GDestroyNotify destroy)
{
	g_return_if_fail (GMIME_IS_RTF_TO_HTML (converter));
	g_return_if_fail (!_g_mime_text_converter_is_active (GMIME_TEXT_CONVERTER (converter)));

	if (converter->logger_destroy)
		converter->logger_destroy (converter->logger_data);

	converter->logger = logger;
	converter->logger_data = user_data;
	converter->logger_destroy = destroy;
}

/**
 * g_mime_rtf_to_html_get_compliance_logger: (skip)
 * @converter: a #GMimeRtfToHtml
 * @user_data: (out) (optional) (nullable) (transfer none): receives the borrowed logger data
 *
 * Returns: (nullable): the RTF compliance logger
 **/
GMimeRtfComplianceLogger
g_mime_rtf_to_html_get_compliance_logger (GMimeRtfToHtml *converter, gpointer *user_data)
{
	g_return_val_if_fail (GMIME_IS_RTF_TO_HTML (converter), NULL);

	if (user_data)
		*user_data = converter->logger_data;

	return converter->logger;
}

/**
 * g_mime_rtf_to_html_get_compliance_options:
 * @converter: a #GMimeRtfToHtml
 *
 * Returns: (transfer none): the RTF compliance options used by the converter
 **/
GMimeRtfComplianceOptions *
g_mime_rtf_to_html_get_compliance_options (GMimeRtfToHtml *converter)
{
	g_return_val_if_fail (GMIME_IS_RTF_TO_HTML (converter), NULL);

	return converter->compliance;
}

/**
 * g_mime_rtf_to_html_set_compliance_options:
 * @converter: a #GMimeRtfToHtml
 * @options: (transfer none): the RTF compliance options
 *
 * Retains a reference to @options. Diagnostic limits restart for each
 * conversion and do not affect rendering.
 **/
void
g_mime_rtf_to_html_set_compliance_options (GMimeRtfToHtml *converter, GMimeRtfComplianceOptions *options)
{
	g_return_if_fail (GMIME_IS_RTF_TO_HTML (converter));
	g_return_if_fail (options != NULL);

	g_mime_rtf_compliance_options_ref (options);
	g_mime_rtf_compliance_options_unref (converter->compliance);

	converter->compliance = options;
}
