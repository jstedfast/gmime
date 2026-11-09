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

#ifndef __GMIME_HTML_UTILS_H__
#define __GMIME_HTML_UTILS_H__

#include <gmime/gmime-html-token.h>

G_BEGIN_DECLS

char *g_mime_html_encode (const char *text);

char *g_mime_html_attribute_encode (const char *text);

char *g_mime_html_attribute_encode_quoted (const char *text, char quote);

char *g_mime_html_decode (const char *text);

char *g_mime_html_attribute_decode (const char *text);

G_END_DECLS

#endif
