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

#include <string.h>

#include "gmime-text-to-html.h"
#include "gmime-text-private.h"
#include "gmime-url-scanner.h"
#include "gmime-stream-mem.h"
#include "gmime-error.h"

/**
 * SECTION: gmime-text-to-html
 * @title: Plain text to HTML
 * @short_description: Quoting, hyperlink detection, and HTML callbacks
 *
 * Implements MimeKit's line-oriented plain and flowed HTML output algorithms.
 * Input accepts CRLF, LF, and CR; generated output newlines are always LF.
 * Headers and footers in text format are independently converted without the
 * body's callback. HTML-format headers and footers are copied as raw markup.
 **/

struct _GMimeTextToHtml {
	GMimeTextConverter parent_object;
	gboolean fragment;
	GMimeHeaderFooterFormat header_format, footer_format;
	GMimeHtmlTagCallback callback;
	gpointer user_data;
	GDestroyNotify destroy_notify;
};

G_DEFINE_FINAL_TYPE (GMimeTextToHtml, g_mime_text_to_html, GMIME_TYPE_TEXT_CONVERTER)

typedef struct {
	GMimeHtmlWriter *writer;
	GMimeHtmlTagCallback callback;
	gpointer user_data;
	GPtrArray *stack;

	/* Index of the element suppressing inner content, or -1. */
	gssize suppressed;
} Output;

static GMimeHtmlTagContext *
new_context (const char *name, const char *href)
{
	GMimeHtmlAttribute *attr = href ? g_mime_html_attribute_new ("href", href) : NULL;
	GMimeHtmlToken *token =
		g_mime_html_token_new_tag (name, FALSE, !strcmp (name, "br"), attr ? &attr : NULL, attr ? 1 : 0);
	GMimeHtmlTagContext *context = g_mime_html_tag_context_new (token);

	g_mime_html_token_unref (token);

	if (attr)
		g_mime_html_attribute_unref (attr);

	return context;
}

static gboolean
invoke (Output *out, GMimeHtmlTagContext *ctx, GError **error)
{
	if (!out->callback)
		return g_mime_html_tag_context_write_tag (ctx, out->writer, TRUE, error);

	if (out->callback (ctx, out->writer, out->user_data, error))
		return TRUE;

	if (error && !*error)
		g_set_error_literal (error, GMIME_ERROR, GMIME_ERROR_GENERAL, "HTML tag callback failed");

	return FALSE;
}

static gboolean
end_context (Output *out, GMimeHtmlTagContext *ctx, GError **error)
{
	if (g_mime_html_tag_context_get_delete_end_tag (ctx))
		return TRUE;

	g_mime_html_tag_context_set_is_end_tag (ctx, TRUE);

	return g_mime_html_tag_context_get_invoke_callback_for_end_tag (ctx) ?
		       invoke (out, ctx, error) :
		       g_mime_html_tag_context_write_tag (ctx, out->writer, FALSE, error);
}

static gboolean
pop_context (Output *out, GError **error)
{
	GMimeHtmlTagContext *ctx = g_ptr_array_steal_index (out->stack, out->stack->len - 1);
	gboolean result = TRUE;

	if (out->suppressed == (gssize) out->stack->len)
		out->suppressed = -1;

	if (out->suppressed == -1)
		result = end_context (out, ctx, error);

	g_mime_html_tag_context_free (ctx);

	return result;
}

static gboolean
write_text (Output *out, const char *text, gsize length, GError **error)
{
	gsize start = 0;
	GMimeUrlMatch match;

	do {
		if (g_mime_url_scan (text, start, length - start, &match)) {
			char *url, *href;
			GMimeHtmlTagContext *ctx;
			gboolean result;

			if (match.start > start &&
			    !g_mime_html_writer_write_text (out->writer, text + start, match.start - start, error))
				return FALSE;

			url = g_strndup (text + match.start, match.end - match.start);
			href = g_strconcat (match.prefix, url, NULL);
			ctx = new_context ("a", href);
			g_free (href);
			g_free (url);

			result = invoke (out, ctx, error) &&
				 (g_mime_html_tag_context_get_suppress_inner_content (ctx) ||
				  g_mime_html_writer_write_text (out->writer, text + match.start,
								 match.end - match.start, error)) &&
				 end_context (out, ctx, error);

			g_mime_html_tag_context_free (ctx);

			if (!result)
				return FALSE;

			start = match.end;
		} else {
			return g_mime_html_writer_write_text (out->writer, text + start, length - start, error);
		}
	} while (start < length);
	return TRUE;
}

static gboolean
write_paragraph (Output *out, const char *text, gsize length, guint depth, gboolean flowed, GError **error)
{
	GMimeHtmlTagContext *ctx;
	gboolean result;

	while (out->stack->len < depth) {
		ctx = new_context ("blockquote", NULL);

		if (out->suppressed == -1) {
			if (!invoke (out, ctx, error)) {
				g_mime_html_tag_context_free (ctx);
				return FALSE;
			}

			if (g_mime_html_tag_context_get_suppress_inner_content (ctx))
				out->suppressed = out->stack->len;
		}

		g_ptr_array_add (out->stack, ctx);
	}

	while (out->stack->len > depth) {
		if (!pop_context (out, error))
			return FALSE;
	}

	if (out->suppressed != -1)
		return TRUE;

	if (!flowed) {
		if (!write_text (out, text, length, error))
			return FALSE;

		ctx = new_context ("br", NULL);
		result = invoke (out, ctx, error);
	} else {
		ctx = new_context (length ? "p" : "br", NULL);
		result = invoke (out, ctx, error);

		if (result && length)
			result = (g_mime_html_tag_context_get_suppress_inner_content (ctx) ||
				  write_text (out, text, length, error)) &&
				 end_context (out, ctx, error);

		if (result && !g_mime_html_tag_context_get_delete_tag (ctx))
			result = g_mime_html_writer_write_markup_text (out->writer, "\n", 1, error);
	}

	g_mime_html_tag_context_free (ctx);

	return result;
}

static gboolean
convert_body (GMimeStream *input, GMimeHtmlWriter *writer, gboolean flowed, gboolean delete_space,
	      GMimeHtmlTagCallback callback, gpointer data, GError **error)
{
	GMimeTextReader reader = { .stream = input };
	Output out = { writer, callback, data,
		       g_ptr_array_new_with_free_func ((GDestroyNotify) g_mime_html_tag_context_free), -1 };
	GString *line = g_string_new (NULL), *para = g_string_new (NULL);
	guint para_depth = 0;
	gboolean eof, result = FALSE;

	while (TRUE) {
		gsize index = 0, length;
		guint depth = 0;
		const char *text;

		if (!_g_mime_text_reader_read_line (&reader, line, &eof, error))
			goto done;

		if (eof)
			break;

		while (index < line->len && line->str[index] == '>') {
			depth++;
			index++;

			if (!flowed && index < line->len && line->str[index] == ' ')
				index++;
		}

		if (flowed && index < line->len && line->str[index] == ' ')
			index++;

		text = line->str + index;
		length = line->len - index;

		if (!flowed) {
			if (!write_paragraph (&out, text, length, depth, FALSE, error))
				goto done;

			continue;
		}

		if (!para->len) {
			para_depth = depth;
		} else if (depth != para_depth) {
			/* RFC 3676: quote-depth rules win over a preceding flowed line. */
			if (!write_paragraph (&out, para->str, para->len, para_depth, TRUE, error))
				goto done;

			para_depth = depth;
			g_string_truncate (para, 0);
		}

		g_string_append_len (para, text, length);

		if (!length || text[length - 1] != ' ') {
			if (!write_paragraph (&out, para->str, para->len, para_depth, TRUE, error))
				goto done;

			para_depth = 0;
			g_string_truncate (para, 0);
		} else if (delete_space) {
			/* A trailing space indicates continuation, and delsp removes it. */
			g_string_truncate (para, para->len - 1);
		}
	}

	if (flowed && para->len && !write_paragraph (&out, para->str, para->len, para_depth, TRUE, error))
		goto done;

	while (out.stack->len) {
		if (!pop_context (&out, error))
			goto done;
	}

	result = g_mime_html_writer_finish (writer, error);

done:
	g_ptr_array_unref (out.stack);
	g_string_free (line, TRUE);
	g_string_free (para, TRUE);

	return result;
}

gboolean
_g_mime_html_output_header_footer (GMimeHtmlWriter *writer, const char *text, GMimeHeaderFooterFormat format,
				   GError **error)
{
	GMimeStream *input;
	gboolean result;

	if (!text || !*text)
		return TRUE;

	if (format == GMIME_HEADER_FOOTER_FORMAT_HTML)
		return g_mime_html_writer_write_markup_text (writer, text, -1, error);

	input = g_mime_stream_mem_new_with_buffer (text, strlen (text));
	result = convert_body (input, writer, FALSE, FALSE, NULL, NULL, error);
	g_object_unref (input);

	return result;
}

gboolean
_g_mime_html_output_convert (GMimeTextConverter *converter, GMimeStream *input, GMimeStream *output, gboolean flowed,
			     gboolean delete_space, gboolean fragment, GMimeHeaderFooterFormat header_format,
			     GMimeHeaderFooterFormat footer_format, GMimeHtmlTagCallback callback, gpointer user_data,
			     GError **error)
{
	GMimeHtmlWriter *writer = g_mime_html_writer_new (output);
	gboolean result = (fragment || g_mime_html_writer_write_markup_text (writer, "<html><body>", -1, error)) &&
			  _g_mime_html_output_header_footer (writer, g_mime_text_converter_get_header (converter),
							     header_format, error) &&
			  convert_body (input, writer, flowed, delete_space, callback, user_data, error) &&
			  _g_mime_html_output_header_footer (writer, g_mime_text_converter_get_footer (converter),
							     footer_format, error) &&
			  (fragment || g_mime_html_writer_write_markup_text (writer, "</body></html>", -1, error)) &&
			  g_mime_html_writer_finish (writer, error);

	g_object_unref (writer);

	return result;
}

static gboolean
text_convert (GMimeTextConverter *base, GMimeStream *input, GMimeStream *output, GError **error)
{
	GMimeTextToHtml *self = GMIME_TEXT_TO_HTML (base);

	return _g_mime_html_output_convert (base, input, output, FALSE, FALSE, self->fragment, self->header_format,
					    self->footer_format, self->callback, self->user_data, error);
}

static void
text_finalize (GObject *object)
{
	GMimeTextToHtml *self = GMIME_TEXT_TO_HTML (object);

	if (self->destroy_notify)
		self->destroy_notify (self->user_data);

	G_OBJECT_CLASS (g_mime_text_to_html_parent_class)->finalize (object);
}

static void
g_mime_text_to_html_class_init (GMimeTextToHtmlClass *klass)
{
	GMimeTextConverterClass *base = GMIME_TEXT_CONVERTER_CLASS (klass);

	G_OBJECT_CLASS (klass)->finalize = text_finalize;

	base->input_format = GMIME_TEXT_FORMAT_PLAIN;
	base->output_format = GMIME_TEXT_FORMAT_HTML;
	base->convert = text_convert;
}

static void
g_mime_text_to_html_init (GMimeTextToHtml *self)
{
	(void) self;
}

/**
 * g_mime_text_to_html_new:
 *
 * Creates a plain-text to HTML converter. By default, the output includes
 * html and body wrappers, and headers and footers are interpreted as text.
 *
 * Returns: (transfer full): a new #GMimeTextToHtml.
 **/
GMimeTextConverter *
g_mime_text_to_html_new (void)
{
	return g_object_new (GMIME_TYPE_TEXT_TO_HTML, NULL);
}

/**
 * g_mime_text_to_html_get_output_html_fragment:
 * @converter: a #GMimeTextToHtml
 *
 * Gets whether the generated html and body wrappers are omitted.
 *
 * Returns: %TRUE for fragment output, or %FALSE for document output (the default).
 **/
gboolean
g_mime_text_to_html_get_output_html_fragment (GMimeTextToHtml *converter)
{
	g_return_val_if_fail (GMIME_IS_TEXT_TO_HTML (converter), 0);

	return converter->fragment;
}

/**
 * g_mime_text_to_html_set_output_html_fragment:
 * @converter: a #GMimeTextToHtml
 * @value: whether to omit the generated html and body wrappers
 *
 * Selects fragment or document output. Headers and footers are included in
 * either mode. This setting cannot be changed during conversion.
 **/
void
g_mime_text_to_html_set_output_html_fragment (GMimeTextToHtml *converter, gboolean value)
{
	g_return_if_fail (GMIME_IS_TEXT_TO_HTML (converter));
	g_return_if_fail (!_g_mime_text_converter_is_active (GMIME_TEXT_CONVERTER (converter)));

	converter->fragment = value;
}

/**
 * g_mime_text_to_html_get_header_format:
 * @converter: a #GMimeTextToHtml
 *
 * Gets how the configured header is interpreted.
 *
 * Returns: the header format, initially %GMIME_HEADER_FOOTER_FORMAT_TEXT.
 **/
GMimeHeaderFooterFormat
g_mime_text_to_html_get_header_format (GMimeTextToHtml *converter)
{
	g_return_val_if_fail (GMIME_IS_TEXT_TO_HTML (converter), 0);

	return converter->header_format;
}

/**
 * g_mime_text_to_html_set_header_format:
 * @converter: a #GMimeTextToHtml
 * @value: the format of the configured header
 *
 * Selects how the header set by g_mime_text_converter_set_header() is written.
 * Text is converted to HTML without invoking the body's tag callback; HTML
 * is copied verbatim and is not sanitized. This setting cannot be changed
 * during conversion.
 **/
void
g_mime_text_to_html_set_header_format (GMimeTextToHtml *converter, GMimeHeaderFooterFormat value)
{
	g_return_if_fail (GMIME_IS_TEXT_TO_HTML (converter));
	g_return_if_fail (!_g_mime_text_converter_is_active (GMIME_TEXT_CONVERTER (converter)));

	converter->header_format = value;
}

/**
 * g_mime_text_to_html_get_footer_format:
 * @converter: a #GMimeTextToHtml
 *
 * Gets how the configured footer is interpreted.
 *
 * Returns: the footer format, initially %GMIME_HEADER_FOOTER_FORMAT_TEXT.
 **/
GMimeHeaderFooterFormat
g_mime_text_to_html_get_footer_format (GMimeTextToHtml *converter)
{
	g_return_val_if_fail (GMIME_IS_TEXT_TO_HTML (converter), 0);

	return converter->footer_format;
}

/**
 * g_mime_text_to_html_set_footer_format:
 * @converter: a #GMimeTextToHtml
 * @value: the format of the configured footer
 *
 * Selects how the footer set by g_mime_text_converter_set_footer() is written.
 * Text is converted to HTML without invoking the body's tag callback; HTML
 * is copied verbatim and is not sanitized. This setting cannot be changed
 * during conversion.
 **/
void
g_mime_text_to_html_set_footer_format (GMimeTextToHtml *converter, GMimeHeaderFooterFormat value)
{
	g_return_if_fail (GMIME_IS_TEXT_TO_HTML (converter));
	g_return_if_fail (!_g_mime_text_converter_is_active (GMIME_TEXT_CONVERTER (converter)));

	converter->footer_format = value;
}

/**
 * g_mime_text_to_html_set_html_tag_callback:
 * @converter: a #GMimeTextToHtml
 * @callback: (nullable) (scope notified) (closure user_data) (destroy destroy_notify): the tag callback
 * @user_data: (nullable): data passed to @callback
 * @destroy_notify: (nullable): destroy notifier for @user_data
 *
 * Sets the callback for tags generated from the body, or restores default
 * tag output if @callback is %NULL. The callback must explicitly write any
 * desired tag; end-tag callbacks are opt-in through #GMimeHtmlTagContext.
 * Wrappers, headers and footers do not invoke the callback.
 *
 * The previous destroy notifier is called when replacing the registration.
 * @destroy_notify is called when this registration is replaced or the
 * converter is finalized. The callback cannot be changed during conversion.
 **/
void
g_mime_text_to_html_set_html_tag_callback (GMimeTextToHtml *converter, GMimeHtmlTagCallback callback,
					gpointer user_data, GDestroyNotify destroy_notify)
{
	g_return_if_fail (GMIME_IS_TEXT_TO_HTML (converter));
	g_return_if_fail (!_g_mime_text_converter_is_active (GMIME_TEXT_CONVERTER (converter)));

	if (converter->destroy_notify)
		converter->destroy_notify (converter->user_data);

	converter->callback = callback;
	converter->user_data = user_data;
	converter->destroy_notify = destroy_notify;
}

/**
 * g_mime_text_to_html_get_html_tag_callback: (skip)
 * @converter: a #GMimeTextToHtml
 * @user_data: (out) (optional) (nullable) (transfer none): return location for
 *   the callback's borrowed user data
 *
 * Gets the registered HTML tag callback. This function is C-only because
 * introspection does not support returning callback function pointers.
 *
 * Returns: (nullable): the registered callback, or %NULL if none is set.
 **/
GMimeHtmlTagCallback
g_mime_text_to_html_get_html_tag_callback (GMimeTextToHtml *converter, gpointer *user_data)
{
	g_return_val_if_fail (GMIME_IS_TEXT_TO_HTML (converter), NULL);

	if (user_data)
		*user_data = converter->user_data;

	return converter->callback;
}
