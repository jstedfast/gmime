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

#ifndef __GMIME_FLOWED_TO_HTML_H__
#define __GMIME_FLOWED_TO_HTML_H__

#include <gmime/gmime-text-to-html.h>

G_BEGIN_DECLS

#define GMIME_TYPE_FLOWED_TO_HTML (g_mime_flowed_to_html_get_type ())

/**
 * GMimeFlowedToHtml:
 *
 * Converts RFC 3676 format=flowed text to HTML paragraphs and blockquotes.
 **/
G_DECLARE_FINAL_TYPE (GMimeFlowedToHtml, g_mime_flowed_to_html, GMIME, FLOWED_TO_HTML, GMimeTextConverter)

GMimeTextConverter *g_mime_flowed_to_html_new (void);

gboolean g_mime_flowed_to_html_get_delete_space (GMimeFlowedToHtml *converter);
void g_mime_flowed_to_html_set_delete_space (GMimeFlowedToHtml *converter, gboolean value);

gboolean g_mime_flowed_to_html_get_output_html_fragment (GMimeFlowedToHtml *converter);
void g_mime_flowed_to_html_set_output_html_fragment (GMimeFlowedToHtml *converter, gboolean value);

GMimeHeaderFooterFormat g_mime_flowed_to_html_get_header_format (GMimeFlowedToHtml *converter);
void g_mime_flowed_to_html_set_header_format (GMimeFlowedToHtml *converter, GMimeHeaderFooterFormat value);

GMimeHeaderFooterFormat g_mime_flowed_to_html_get_footer_format (GMimeFlowedToHtml *converter);
void g_mime_flowed_to_html_set_footer_format (GMimeFlowedToHtml *converter, GMimeHeaderFooterFormat value);

void g_mime_flowed_to_html_set_html_tag_callback (GMimeFlowedToHtml *converter, GMimeHtmlTagCallback callback,
						  gpointer user_data, GDestroyNotify destroy_notify);

GMimeHtmlTagCallback g_mime_flowed_to_html_get_html_tag_callback (GMimeFlowedToHtml *converter, gpointer *user_data);

G_END_DECLS

#endif
