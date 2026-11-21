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

#ifndef __GMIME_RTF_TO_TEXT_H__
#define __GMIME_RTF_TO_TEXT_H__

#include <gmime/gmime-rtf-compliance.h>
#include <gmime/gmime-text-converter.h>

G_BEGIN_DECLS

#define GMIME_TYPE_RTF_TO_TEXT (g_mime_rtf_to_text_get_type ())
/**
 * GMimeRtfToText:
 *
 * Renders Rich Text Format as plain text.
 */
G_DECLARE_FINAL_TYPE (GMimeRtfToText, g_mime_rtf_to_text, GMIME, RTF_TO_TEXT, GMimeTextConverter)

/**
 * GMimeRtfToTextAttachmentPlaceholderCallback:
 * @index: zero-based index of this visible attachment placeholder
 * @output: (transfer none): stream for UTF-8 replacement text
 * @user_data: (closure): caller data
 * @error: return location for an error
 *
 * Returns: %TRUE on success; on failure set @error and return %FALSE.
 */
typedef gboolean (*GMimeRtfToTextAttachmentPlaceholderCallback) (guint index, GMimeStream *output,
								gpointer user_data, GError **error);

GMimeTextConverter *g_mime_rtf_to_text_new (void);
guint g_mime_rtf_to_text_get_max_group_depth (GMimeRtfToText *converter);
void g_mime_rtf_to_text_set_max_group_depth (GMimeRtfToText *converter, guint maximum);
guint g_mime_rtf_to_text_get_max_font_table_entries (GMimeRtfToText *converter);
void g_mime_rtf_to_text_set_max_font_table_entries (GMimeRtfToText *converter, guint maximum);
guint g_mime_rtf_to_text_get_max_color_table_entries (GMimeRtfToText *converter);
void g_mime_rtf_to_text_set_max_color_table_entries (GMimeRtfToText *converter, guint maximum);

void g_mime_rtf_to_text_set_attachment_placeholder_callback (GMimeRtfToText *converter,
		GMimeRtfToTextAttachmentPlaceholderCallback callback, gpointer user_data, GDestroyNotify destroy);
GMimeRtfToTextAttachmentPlaceholderCallback g_mime_rtf_to_text_get_attachment_placeholder_callback (
		GMimeRtfToText *converter, gpointer *user_data);
void g_mime_rtf_to_text_set_compliance_logger (GMimeRtfToText *converter, GMimeRtfComplianceLogger logger,
					      gpointer user_data, GDestroyNotify destroy);
GMimeRtfComplianceLogger g_mime_rtf_to_text_get_compliance_logger (GMimeRtfToText *converter, gpointer *user_data);
GMimeRtfComplianceOptions *g_mime_rtf_to_text_get_compliance_options (GMimeRtfToText *converter);
void g_mime_rtf_to_text_set_compliance_options (GMimeRtfToText *converter, GMimeRtfComplianceOptions *options);

G_END_DECLS
#endif
