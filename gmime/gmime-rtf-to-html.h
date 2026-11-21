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

#ifndef __GMIME_RTF_TO_HTML_H__
#define __GMIME_RTF_TO_HTML_H__

#include <gmime/gmime-html-to-html.h>
#include <gmime/gmime-rtf-compliance.h>

G_BEGIN_DECLS

#define GMIME_TYPE_RTF_TO_HTML (g_mime_rtf_to_html_get_type ())

/**
 * GMimeRtfToHtml:
 *
 * Renders rich text as HTML, or rewrites the original HTML of messages
 * declaring `\fromhtml1`. Extracted HTML is not sanitized.
 **/
G_DECLARE_FINAL_TYPE (GMimeRtfToHtml, g_mime_rtf_to_html, GMIME, RTF_TO_HTML, GMimeTextConverter)

/**
 * GMimeRtfToHtmlAttachmentPlaceholderCallback:
 * @index: zero-based attachment placeholder index, in document order
 * @writer: (transfer none): borrowed HTML writer
 * @user_data: (nullable): callback data
 * @error: return location for a #GError, or %NULL
 *
 * Writes replacement content for an Outlook `\objattph` mark. Output from
 * this callback is not passed to the tag callback. Hidden marks are skipped;
 * extraction uses the attachment references already present in the HTML.
 *
 * Returns: %TRUE on success, or %FALSE to abort conversion.
 **/
typedef gboolean (*GMimeRtfToHtmlAttachmentPlaceholderCallback) (int index, GMimeHtmlWriter *writer,
								 gpointer user_data, GError **error);

GMimeTextConverter *g_mime_rtf_to_html_new (void);

gboolean g_mime_rtf_to_html_get_extract_encapsulated_html (GMimeRtfToHtml *converter);
void g_mime_rtf_to_html_set_extract_encapsulated_html (GMimeRtfToHtml *converter, gboolean value);
gboolean g_mime_rtf_to_html_get_output_html_fragment (GMimeRtfToHtml *converter);
void g_mime_rtf_to_html_set_output_html_fragment (GMimeRtfToHtml *converter, gboolean value);

GMimeHeaderFooterFormat g_mime_rtf_to_html_get_header_format (GMimeRtfToHtml *converter);
void g_mime_rtf_to_html_set_header_format (GMimeRtfToHtml *converter, GMimeHeaderFooterFormat value);
GMimeHeaderFooterFormat g_mime_rtf_to_html_get_footer_format (GMimeRtfToHtml *converter);
void g_mime_rtf_to_html_set_footer_format (GMimeRtfToHtml *converter, GMimeHeaderFooterFormat value);
GMimeHtmlNoScriptHandling g_mime_rtf_to_html_get_no_script_handling (GMimeRtfToHtml *converter);
void g_mime_rtf_to_html_set_no_script_handling (GMimeRtfToHtml *converter, GMimeHtmlNoScriptHandling value);

void g_mime_rtf_to_html_set_html_tag_callback (GMimeRtfToHtml *converter, GMimeHtmlTagCallback callback,
					      gpointer user_data, GDestroyNotify destroy);
GMimeHtmlTagCallback g_mime_rtf_to_html_get_html_tag_callback (GMimeRtfToHtml *converter, gpointer *user_data);

void g_mime_rtf_to_html_set_attachment_placeholder_callback (GMimeRtfToHtml *converter,
							     GMimeRtfToHtmlAttachmentPlaceholderCallback callback,
							     gpointer user_data, GDestroyNotify destroy);
GMimeRtfToHtmlAttachmentPlaceholderCallback g_mime_rtf_to_html_get_attachment_placeholder_callback (
	GMimeRtfToHtml *converter, gpointer *user_data);

guint g_mime_rtf_to_html_get_max_group_depth (GMimeRtfToHtml *converter);
void g_mime_rtf_to_html_set_max_group_depth (GMimeRtfToHtml *converter, guint value);
guint g_mime_rtf_to_html_get_max_font_table_entries (GMimeRtfToHtml *converter);
void g_mime_rtf_to_html_set_max_font_table_entries (GMimeRtfToHtml *converter, guint value);
guint g_mime_rtf_to_html_get_max_color_table_entries (GMimeRtfToHtml *converter);
void g_mime_rtf_to_html_set_max_color_table_entries (GMimeRtfToHtml *converter, guint value);
guint g_mime_rtf_to_html_get_max_element_depth (GMimeRtfToHtml *converter);
void g_mime_rtf_to_html_set_max_element_depth (GMimeRtfToHtml *converter, guint value);

void g_mime_rtf_to_html_set_compliance_logger (GMimeRtfToHtml *converter, GMimeRtfComplianceLogger logger,
					      gpointer user_data, GDestroyNotify destroy);
GMimeRtfComplianceLogger g_mime_rtf_to_html_get_compliance_logger (GMimeRtfToHtml *converter, gpointer *user_data);
GMimeRtfComplianceOptions *g_mime_rtf_to_html_get_compliance_options (GMimeRtfToHtml *converter);
void g_mime_rtf_to_html_set_compliance_options (GMimeRtfToHtml *converter, GMimeRtfComplianceOptions *options);

G_END_DECLS

#endif /* __GMIME_RTF_TO_HTML_H__ */
