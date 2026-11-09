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

#ifndef __GMIME_ENRICHED_TO_HTML_H__
#define __GMIME_ENRICHED_TO_HTML_H__

#include <gmime/gmime-text-to-html.h>

G_BEGIN_DECLS

#define GMIME_TYPE_ENRICHED_TO_HTML (g_mime_enriched_to_html_get_type ())

/**
 * GMimeEnrichedToHtml:
 *
 * Converts RFC 1896 text/enriched to HTML with configurable resource limits.
 **/
G_DECLARE_FINAL_TYPE (GMimeEnrichedToHtml, g_mime_enriched_to_html, GMIME, ENRICHED_TO_HTML, GMimeTextConverter)

GMimeTextConverter *g_mime_enriched_to_html_new (void);

gboolean g_mime_enriched_to_html_get_output_html_fragment (GMimeEnrichedToHtml *converter);
void g_mime_enriched_to_html_set_output_html_fragment (GMimeEnrichedToHtml *converter, gboolean value);

GMimeHeaderFooterFormat g_mime_enriched_to_html_get_header_format (GMimeEnrichedToHtml *converter);
void g_mime_enriched_to_html_set_header_format (GMimeEnrichedToHtml *converter, GMimeHeaderFooterFormat value);

GMimeHeaderFooterFormat g_mime_enriched_to_html_get_footer_format (GMimeEnrichedToHtml *converter);
void g_mime_enriched_to_html_set_footer_format (GMimeEnrichedToHtml *converter, GMimeHeaderFooterFormat value);

guint g_mime_enriched_to_html_get_max_element_depth (GMimeEnrichedToHtml *converter);
void g_mime_enriched_to_html_set_max_element_depth (GMimeEnrichedToHtml *converter, guint value);

guint g_mime_enriched_to_html_get_max_parameter_length (GMimeEnrichedToHtml *converter);
void g_mime_enriched_to_html_set_max_parameter_length (GMimeEnrichedToHtml *converter, guint value);

gsize g_mime_enriched_to_html_get_max_input_length (GMimeEnrichedToHtml *converter);
void g_mime_enriched_to_html_set_max_input_length (GMimeEnrichedToHtml *converter, gsize value);

gsize g_mime_enriched_to_html_get_max_output_length (GMimeEnrichedToHtml *converter);
void g_mime_enriched_to_html_set_max_output_length (GMimeEnrichedToHtml *converter, gsize value);

G_END_DECLS

#endif
