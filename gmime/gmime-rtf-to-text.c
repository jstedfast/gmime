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
#include "gmime-rtf-to-text.h"
#include "gmime-rtf-interpreter-private.h"
#include "gmime-text-private.h"

/**
 * SECTION: gmime-rtf-to-text
 * @title: GMimeRtfToText
 * @short_description: Render Rich Text Format as plain text
 *
 * The stream input charset defaults to ISO-8859-1 to preserve wire bytes;
 * document and font code pages determine the interpretation of those bytes.
 * The RTF rendition of encapsulated HTML is used, not its HTML markup.
 */
struct _GMimeRtfToText {
	GMimeTextConverter parent_object;
	guint depth, fonts, colors;
	GMimeRtfComplianceOptions *compliance;
	GMimeRtfComplianceLogger logger;
	GMimeRtfToTextAttachmentPlaceholderCallback callback;
	gpointer logger_data, callback_data;
	GDestroyNotify logger_destroy, callback_destroy;
};

G_DEFINE_TYPE (GMimeRtfToText, g_mime_rtf_to_text, GMIME_TYPE_TEXT_CONVERTER)

typedef struct {
	GMimeRtfToText *converter;
	GMimeStream *output;
	/* Even the last cell ends with \cell. Defer its tab so a following
	 * \row can discard it rather than leave a trailing tab. */
	gboolean pending_cell;
	guint index;
} TextHandler;

static gboolean
text_event (GMimeRtfEvent event, const GMimeRtfFormat *format, const char *text, gsize length,
	    gpointer user_data, GError **error)
{
	TextHandler *h = user_data;
	guint index;

	if (event == GMIME_RTF_EVENT_BEGIN || event == GMIME_RTF_EVENT_HTML_TAG)
		return TRUE;

	if (event == GMIME_RTF_EVENT_ROW)
		h->pending_cell = FALSE;

	if (event == GMIME_RTF_EVENT_OBJECT_PLACEHOLDER && !h->converter->callback)
		return TRUE;

	if (h->pending_cell && !_g_mime_text_write (h->output, "\t", 1, error))
		return FALSE;

	h->pending_cell = event == GMIME_RTF_EVENT_CELL;

	switch (event) {
	case GMIME_RTF_EVENT_TEXT:
		return _g_mime_text_write (h->output, text, length, error);
	case GMIME_RTF_EVENT_ROW:
	case GMIME_RTF_EVENT_PARAGRAPH:
	case GMIME_RTF_EVENT_LINE_BREAK:
		return _g_mime_text_write (h->output, "\n", 1, error);
	case GMIME_RTF_EVENT_OBJECT_PLACEHOLDER:
		index = h->index;

		if (h->index < G_MAXINT)
			h->index++;

		return h->converter->callback (index, h->output, h->converter->callback_data, error);
	default:
		return TRUE;
	}
}

static gboolean
convert (GMimeTextConverter *converter, GMimeStream *input, GMimeStream *output, GError **error)
{
	const char *header = g_mime_text_converter_get_header (converter);
	const char *footer = g_mime_text_converter_get_footer (converter);
	GMimeRtfToText *self = GMIME_RTF_TO_TEXT (converter);
	TextHandler handler = { self, output, FALSE, 0 };
	GMimeRtfDiagnostics diag;

	_g_mime_rtf_diagnostics_init (&diag, self->compliance, self->logger, self->logger_data);

	return (!header || _g_mime_text_write (output, header, strlen (header), error)) &&
		_g_mime_rtf_interpret (input, FALSE, self->depth, self->fonts, self->colors,
				      &diag, text_event, &handler, error) &&
		(!footer || _g_mime_text_write (output, footer, strlen (footer), error));
}

static void
rtf_to_text_finalize (GObject *object)
{
	GMimeRtfToText *self = GMIME_RTF_TO_TEXT (object);

	if (self->logger_destroy)
		self->logger_destroy (self->logger_data);

	if (self->callback_destroy)
		self->callback_destroy (self->callback_data);

	g_mime_rtf_compliance_options_unref (self->compliance);

	G_OBJECT_CLASS (g_mime_rtf_to_text_parent_class)->finalize (object);
}

static void
g_mime_rtf_to_text_class_init (GMimeRtfToTextClass *klass)
{
	GMimeTextConverterClass *converter = GMIME_TEXT_CONVERTER_CLASS (klass);

	converter->input_format = GMIME_TEXT_FORMAT_RICH_TEXT;
	converter->output_format = GMIME_TEXT_FORMAT_PLAIN;
	converter->convert = convert;

	G_OBJECT_CLASS (klass)->finalize = rtf_to_text_finalize;
}

static void
g_mime_rtf_to_text_init (GMimeRtfToText *self)
{
	self->depth = self->fonts = self->colors = 4096;
	self->compliance = g_mime_rtf_compliance_options_new ();

	g_mime_text_converter_set_input_charset (GMIME_TEXT_CONVERTER (self), "iso-8859-1", NULL);
}

/**
 * g_mime_rtf_to_text_new:
 *
 * Returns: (transfer full): a new RTF to plain text converter
 */
GMimeTextConverter *
g_mime_rtf_to_text_new (void)
{
	return g_object_new (GMIME_TYPE_RTF_TO_TEXT, NULL);
}

/**
 * g_mime_rtf_to_text_get_max_group_depth:
 * @converter: an RTF converter
 *
 * Returns: the maximum number of distinct saved group states (initially 4096).
 */
guint
g_mime_rtf_to_text_get_max_group_depth (GMimeRtfToText *converter)
{
	g_return_val_if_fail (GMIME_IS_RTF_TO_TEXT (converter), 0);

	return converter->depth;
}

/**
 * g_mime_rtf_to_text_set_max_group_depth:
 * @converter: an RTF converter
 * @maximum: maximum number of distinct saved group states, at least one
 *
 * Sets the limit for subsequent conversions.
 */
void
g_mime_rtf_to_text_set_max_group_depth (GMimeRtfToText *converter, guint maximum)
{
	g_return_if_fail (GMIME_IS_RTF_TO_TEXT (converter));
	g_return_if_fail (maximum > 0);

	converter->depth = maximum;
}

/**
 * g_mime_rtf_to_text_get_max_font_table_entries:
 * @converter: an RTF converter
 *
 * Returns: the maximum number of font table entries (initially 4096).
 */
guint
g_mime_rtf_to_text_get_max_font_table_entries (GMimeRtfToText *converter)
{
	g_return_val_if_fail (GMIME_IS_RTF_TO_TEXT (converter), 0);

	return converter->fonts;
}

/**
 * g_mime_rtf_to_text_set_max_font_table_entries:
 * @converter: an RTF converter
 * @maximum: maximum number of font table entries, at least one
 *
 * Sets the limit for subsequent conversions.
 */
void
g_mime_rtf_to_text_set_max_font_table_entries (GMimeRtfToText *converter, guint maximum)
{
	g_return_if_fail (GMIME_IS_RTF_TO_TEXT (converter));
	g_return_if_fail (maximum > 0);

	converter->fonts = maximum;
}

/**
 * g_mime_rtf_to_text_get_max_color_table_entries:
 * @converter: an RTF converter
 *
 * Returns: the maximum number of color table entries (initially 4096).
 */
guint
g_mime_rtf_to_text_get_max_color_table_entries (GMimeRtfToText *converter)
{
	g_return_val_if_fail (GMIME_IS_RTF_TO_TEXT (converter), 0);

	return converter->colors;
}

/**
 * g_mime_rtf_to_text_set_max_color_table_entries:
 * @converter: an RTF converter
 * @maximum: maximum number of color table entries, at least one
 *
 * Sets the limit for subsequent conversions.
 */
void
g_mime_rtf_to_text_set_max_color_table_entries (GMimeRtfToText *converter, guint maximum)
{
	g_return_if_fail (GMIME_IS_RTF_TO_TEXT (converter));
	g_return_if_fail (maximum > 0);

	converter->colors = maximum;
}

/**
 * g_mime_rtf_to_text_set_attachment_placeholder_callback:
 * @converter: a #GMimeRtfToText
 * @callback: (nullable) (scope notified) (closure user_data) (destroy destroy): the attachment placeholder callback
 * @user_data: (nullable): data passed to the callback
 * @destroy: (nullable): destroy notifier for @user_data
 *
 * Invoked for visible placeholders only. With no callback the placeholder is omitted.
 *
 * The previous destroy notifier is called when replacing the registration.
 * @destroy is called when this registration is replaced or the converter
 * is finalized. The callback cannot be changed during conversion.
 **/
void
g_mime_rtf_to_text_set_attachment_placeholder_callback (GMimeRtfToText *converter, GMimeRtfToTextAttachmentPlaceholderCallback callback,
					   gpointer user_data, GDestroyNotify destroy)
{
	g_return_if_fail (GMIME_IS_RTF_TO_TEXT (converter));
	g_return_if_fail (!_g_mime_text_converter_is_active (GMIME_TEXT_CONVERTER (converter)));

	if (converter->callback_destroy)
		converter->callback_destroy (converter->callback_data);

	converter->callback = callback;
	converter->callback_data = user_data;
	converter->callback_destroy = destroy;
}

/**
 * g_mime_rtf_to_text_get_attachment_placeholder_callback: (skip)
 * @converter: a #GMimeRtfToText
 * @user_data: (out) (optional) (nullable) (transfer none): the callback's borrowed user data
 *
 * Gets the currently registered attachment placeholder callback.
 *
 * Returns: (nullable): the attachment placeholder callback, or %NULL if none is registered.
 **/
GMimeRtfToTextAttachmentPlaceholderCallback
g_mime_rtf_to_text_get_attachment_placeholder_callback (GMimeRtfToText *converter, gpointer *user_data)
{
	g_return_val_if_fail (GMIME_IS_RTF_TO_TEXT (converter), NULL);

	if (user_data)
		*user_data = converter->callback_data;

	return converter->callback;
}

/**
 * g_mime_rtf_to_text_set_compliance_logger:
 * @converter: a #GMimeRtfToText
 * @logger: (nullable) (scope notified) (closure user_data) (destroy destroy): the compliance logger
 * @user_data: (nullable): data passed to the logger
 * @destroy: (nullable): destroy notifier for @user_data
 *
 * Sets the compliance logger, or clears it if @logger is %NULL. Diagnostics
 * are borrowed for the duration of the callback and do not change output.
 *
 * The previous destroy notifier is called when replacing the registration.
 * @destroy is called when this registration is replaced or the converter
 * is finalized. The logger cannot be changed during conversion.
 **/
void
g_mime_rtf_to_text_set_compliance_logger (GMimeRtfToText *converter, GMimeRtfComplianceLogger logger,
					   gpointer user_data, GDestroyNotify destroy)
{
	g_return_if_fail (GMIME_IS_RTF_TO_TEXT (converter));
	g_return_if_fail (!_g_mime_text_converter_is_active (GMIME_TEXT_CONVERTER (converter)));

	if (converter->logger_destroy)
		converter->logger_destroy (converter->logger_data);

	converter->logger = logger;
	converter->logger_data = user_data;
	converter->logger_destroy = destroy;
}

/**
 * g_mime_rtf_to_text_get_compliance_logger: (skip)
 * @converter: a #GMimeRtfToText
 * @user_data: (out) (optional) (nullable) (transfer none): the logger's borrowed user data
 *
 * Gets the currently registered RTF compliance logger.
 *
 * Returns: (nullable): the compliance logger, or %NULL if none is registered.
 **/
GMimeRtfComplianceLogger
g_mime_rtf_to_text_get_compliance_logger (GMimeRtfToText *converter, gpointer *user_data)
{
	g_return_val_if_fail (GMIME_IS_RTF_TO_TEXT (converter), NULL);

	if (user_data)
		*user_data = converter->logger_data;

	return converter->logger;
}

/**
 * g_mime_rtf_to_text_get_compliance_options:
 * @converter: a #GMimeRtfToText
 *
 * Gets the converter's RTF compliance options.
 *
 * Returns: (transfer none): the converter's compliance options.
 **/
GMimeRtfComplianceOptions *
g_mime_rtf_to_text_get_compliance_options (GMimeRtfToText *converter)
{
	g_return_val_if_fail (GMIME_IS_RTF_TO_TEXT (converter), NULL);

	return converter->compliance;
}

/**
 * g_mime_rtf_to_text_set_compliance_options:
 * @converter: a #GMimeRtfToText
 * @options: (transfer none): the RTF compliance options
 *
 * Sets the compliance options. The converter retains a reference to @options.
 **/
void
g_mime_rtf_to_text_set_compliance_options (GMimeRtfToText *converter, GMimeRtfComplianceOptions *options)
{
	g_return_if_fail (GMIME_IS_RTF_TO_TEXT (converter));
	g_return_if_fail (options != NULL);

	g_mime_rtf_compliance_options_ref (options);
	g_mime_rtf_compliance_options_unref (converter->compliance);

	converter->compliance = options;
}
