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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "gmime-flowed-to-html.h"
#include "gmime-text-private.h"

/**
 * SECTION: gmime-flowed-to-html
 * @title: Flowed text to HTML
 * @short_description: RFC 3676 paragraphs and blockquotes
 *
 * Reassembles flowed paragraphs, removes space stuffing, and honors quote-depth
 * transitions and optional delsp processing. Generated paragraph separators
 * use LF regardless of the host platform.
 **/

struct _GMimeFlowedToHtml {
	GMimeTextConverter parent_object;
	gboolean fragment, delete_space;
	GMimeHeaderFooterFormat header_format, footer_format;
	GMimeHtmlTagCallback callback;
	gpointer user_data;
	GDestroyNotify destroy_notify;
};

G_DEFINE_FINAL_TYPE (GMimeFlowedToHtml, g_mime_flowed_to_html, GMIME_TYPE_TEXT_CONVERTER)

static gboolean
flowed_convert (GMimeTextConverter *base, GMimeStream *input, GMimeStream *output, GError **error)
{
	GMimeFlowedToHtml *self = GMIME_FLOWED_TO_HTML (base);

	return _g_mime_html_output_convert (base, input, output, TRUE, self->delete_space, self->fragment,
					    self->header_format, self->footer_format, self->callback, self->user_data,
					    error);
}

static void
flowed_finalize (GObject *object)
{
	GMimeFlowedToHtml *self = GMIME_FLOWED_TO_HTML (object);

	if (self->destroy_notify)
		self->destroy_notify (self->user_data);

	G_OBJECT_CLASS (g_mime_flowed_to_html_parent_class)->finalize (object);
}

static void
g_mime_flowed_to_html_class_init (GMimeFlowedToHtmlClass *klass)
{
	GMimeTextConverterClass *base = GMIME_TEXT_CONVERTER_CLASS (klass);

	G_OBJECT_CLASS (klass)->finalize = flowed_finalize;

	base->input_format = GMIME_TEXT_FORMAT_FLOWED;
	base->output_format = GMIME_TEXT_FORMAT_HTML;
	base->convert = flowed_convert;
}

static void
g_mime_flowed_to_html_init (GMimeFlowedToHtml *self)
{
}

/**
 * g_mime_flowed_to_html_new:
 *
 * Creates an RFC 3676 format=flowed to HTML converter. By default, delsp
 * processing is disabled, output includes html and body wrappers, and
 * headers and footers are interpreted as text.
 *
 * Returns: (transfer full): a new #GMimeFlowedToHtml.
 **/
GMimeTextConverter *
g_mime_flowed_to_html_new (void)
{
	return g_object_new (GMIME_TYPE_FLOWED_TO_HTML, NULL);
}

/**
 * g_mime_flowed_to_html_get_output_html_fragment:
 * @converter: a #GMimeFlowedToHtml
 *
 * Gets whether the generated html and body wrappers are omitted.
 *
 * Returns: %TRUE for fragment output, or %FALSE for document output (the default).
 **/
gboolean
g_mime_flowed_to_html_get_output_html_fragment (GMimeFlowedToHtml *converter)
{
	g_return_val_if_fail (GMIME_IS_FLOWED_TO_HTML (converter), 0);

	return converter->fragment;
}

/**
 * g_mime_flowed_to_html_set_output_html_fragment:
 * @converter: a #GMimeFlowedToHtml
 * @value: whether to omit the generated html and body wrappers
 *
 * Selects fragment or document output. Headers and footers are included in
 * either mode. This setting cannot be changed during conversion.
 **/
void
g_mime_flowed_to_html_set_output_html_fragment (GMimeFlowedToHtml *converter, gboolean value)
{
	g_return_if_fail (GMIME_IS_FLOWED_TO_HTML (converter));
	g_return_if_fail (!_g_mime_text_converter_is_active (GMIME_TEXT_CONVERTER (converter)));

	converter->fragment = value;
}

/**
 * g_mime_flowed_to_html_get_delete_space:
 * @converter: a #GMimeFlowedToHtml
 *
 * Gets whether the trailing space marking a flowed line is deleted when
 * joining it to the next line.
 *
 * Returns: %TRUE for delsp=yes, or %FALSE for delsp=no (the default).
 **/
gboolean
g_mime_flowed_to_html_get_delete_space (GMimeFlowedToHtml *converter)
{
	g_return_val_if_fail (GMIME_IS_FLOWED_TO_HTML (converter), 0);

	return converter->delete_space;
}

/**
 * g_mime_flowed_to_html_set_delete_space:
 * @converter: a #GMimeFlowedToHtml
 * @value: whether to apply delsp=yes processing
 *
 * Selects RFC 3676 delsp processing. Set this to match the input MIME part's
 * delsp parameter; %FALSE preserves the joining space. This setting cannot
 * be changed during conversion.
 **/
void
g_mime_flowed_to_html_set_delete_space (GMimeFlowedToHtml *converter, gboolean value)
{
	g_return_if_fail (GMIME_IS_FLOWED_TO_HTML (converter));
	g_return_if_fail (!_g_mime_text_converter_is_active (GMIME_TEXT_CONVERTER (converter)));

	converter->delete_space = value;
}

/**
 * g_mime_flowed_to_html_get_header_format:
 * @converter: a #GMimeFlowedToHtml
 *
 * Gets how the configured header is interpreted.
 *
 * Returns: the header format, initially %GMIME_HEADER_FOOTER_FORMAT_TEXT.
 **/
GMimeHeaderFooterFormat
g_mime_flowed_to_html_get_header_format (GMimeFlowedToHtml *converter)
{
	g_return_val_if_fail (GMIME_IS_FLOWED_TO_HTML (converter), 0);

	return converter->header_format;
}

/**
 * g_mime_flowed_to_html_set_header_format:
 * @converter: a #GMimeFlowedToHtml
 * @value: the format of the configured header
 *
 * Selects how the header set by g_mime_text_converter_set_header() is written.
 * Text is converted as plain text, not flowed text, without invoking the
 * body's tag callback. HTML is copied verbatim and is not sanitized.
 * This setting cannot be changed during conversion.
 **/
void
g_mime_flowed_to_html_set_header_format (GMimeFlowedToHtml *converter, GMimeHeaderFooterFormat value)
{
	g_return_if_fail (GMIME_IS_FLOWED_TO_HTML (converter));
	g_return_if_fail (!_g_mime_text_converter_is_active (GMIME_TEXT_CONVERTER (converter)));

	converter->header_format = value;
}

/**
 * g_mime_flowed_to_html_get_footer_format:
 * @converter: a #GMimeFlowedToHtml
 *
 * Gets how the configured footer is interpreted.
 *
 * Returns: the footer format, initially %GMIME_HEADER_FOOTER_FORMAT_TEXT.
 **/
GMimeHeaderFooterFormat
g_mime_flowed_to_html_get_footer_format (GMimeFlowedToHtml *converter)
{
	g_return_val_if_fail (GMIME_IS_FLOWED_TO_HTML (converter), 0);

	return converter->footer_format;
}

/**
 * g_mime_flowed_to_html_set_footer_format:
 * @converter: a #GMimeFlowedToHtml
 * @value: the format of the configured footer
 *
 * Selects how the footer set by g_mime_text_converter_set_footer() is written.
 * Text is converted as plain text, not flowed text, without invoking the
 * body's tag callback. HTML is copied verbatim and is not sanitized.
 * This setting cannot be changed during conversion.
 **/
void
g_mime_flowed_to_html_set_footer_format (GMimeFlowedToHtml *converter, GMimeHeaderFooterFormat value)
{
	g_return_if_fail (GMIME_IS_FLOWED_TO_HTML (converter));
	g_return_if_fail (!_g_mime_text_converter_is_active (GMIME_TEXT_CONVERTER (converter)));

	converter->footer_format = value;
}

/**
 * g_mime_flowed_to_html_set_html_tag_callback:
 * @converter: a #GMimeFlowedToHtml
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
g_mime_flowed_to_html_set_html_tag_callback (GMimeFlowedToHtml *converter, GMimeHtmlTagCallback callback,
					  gpointer user_data, GDestroyNotify destroy_notify)
{
	g_return_if_fail (GMIME_IS_FLOWED_TO_HTML (converter));
	g_return_if_fail (!_g_mime_text_converter_is_active (GMIME_TEXT_CONVERTER (converter)));

	if (converter->destroy_notify)
		converter->destroy_notify (converter->user_data);

	converter->callback = callback;
	converter->user_data = user_data;
	converter->destroy_notify = destroy_notify;
}

/**
 * g_mime_flowed_to_html_get_html_tag_callback: (skip)
 * @converter: a #GMimeFlowedToHtml
 * @user_data: (out) (optional) (nullable) (transfer none): return location for
 *   the callback's borrowed user data
 *
 * Gets the registered HTML tag callback. This function is C-only because
 * introspection does not support returning callback function pointers.
 *
 * Returns: (nullable): the registered callback, or %NULL if none is set.
 **/
GMimeHtmlTagCallback
g_mime_flowed_to_html_get_html_tag_callback (GMimeFlowedToHtml *converter, gpointer *user_data)
{
	g_return_val_if_fail (GMIME_IS_FLOWED_TO_HTML (converter), NULL);

	if (user_data)
		*user_data = converter->user_data;

	return converter->callback;
}
