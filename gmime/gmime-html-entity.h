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

#ifndef __GMIME_HTML_ENTITY_H__
#define __GMIME_HTML_ENTITY_H__

#include <glib-object.h>

G_BEGIN_DECLS

#define GMIME_TYPE_HTML_ENTITY_DECODER (g_mime_html_entity_decoder_get_type ())

/**
 * GMimeHtmlEntityDecoder:
 *
 * An incremental decoder for named and numeric HTML character references.
 **/
G_DECLARE_FINAL_TYPE (GMimeHtmlEntityDecoder, g_mime_html_entity_decoder, GMIME, HTML_ENTITY_DECODER, GObject)

GMimeHtmlEntityDecoder *g_mime_html_entity_decoder_new (void);

gboolean g_mime_html_entity_decoder_push (GMimeHtmlEntityDecoder *decoder, gunichar c);

const char *g_mime_html_entity_decoder_get_value (GMimeHtmlEntityDecoder *decoder);

const char *g_mime_html_entity_decoder_get_attribute_value (GMimeHtmlEntityDecoder *decoder, gunichar next);
void g_mime_html_entity_decoder_reset (GMimeHtmlEntityDecoder *decoder);

G_END_DECLS

#endif
