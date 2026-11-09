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

#ifndef __GMIME_HTML_TO_TEXT_H__
#define __GMIME_HTML_TO_TEXT_H__

#include <gmime/gmime-text-converter.h>

G_BEGIN_DECLS

#define GMIME_TYPE_HTML_TO_TEXT (g_mime_html_to_text_get_type ())

/**
 * GMimeHtmlToText:
 *
 * Renders HTML as plain text with configurable links, images and resource limits.
 **/
G_DECLARE_FINAL_TYPE (GMimeHtmlToText, g_mime_html_to_text, GMIME, HTML_TO_TEXT, GMimeTextConverter)

GMimeTextConverter *g_mime_html_to_text_new (void);

gboolean g_mime_html_to_text_get_output_link_urls (GMimeHtmlToText *converter);
void g_mime_html_to_text_set_output_link_urls (GMimeHtmlToText *converter, gboolean value);

gboolean g_mime_html_to_text_get_output_image_alt_text (GMimeHtmlToText *converter);
void g_mime_html_to_text_set_output_image_alt_text (GMimeHtmlToText *converter, gboolean value);

guint g_mime_html_to_text_get_max_element_depth (GMimeHtmlToText *converter);
void g_mime_html_to_text_set_max_element_depth (GMimeHtmlToText *converter, guint value);

gsize g_mime_html_to_text_get_max_input_length (GMimeHtmlToText *converter);
void g_mime_html_to_text_set_max_input_length (GMimeHtmlToText *converter, gsize value);

gsize g_mime_html_to_text_get_max_output_length (GMimeHtmlToText *converter);
void g_mime_html_to_text_set_max_output_length (GMimeHtmlToText *converter, gsize value);

G_END_DECLS

#endif
