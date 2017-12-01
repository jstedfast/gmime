/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*  GMime
 *  Copyright (C) 2000-2017 Jeffrey Stedfast
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


#ifndef __GMIME_UTILS_H__
#define __GMIME_UTILS_H__

#include <glib.h>
#include <stdarg.h>

#include <gmime/gmime-format-options.h>
#include <gmime/gmime-parser-options.h>
#include <gmime/gmime-encodings.h>

G_BEGIN_DECLS

GDateTime *g_mime_utils_header_decode_date (const char *str);
char *g_mime_utils_header_format_date (GDateTime *date);

char *g_mime_utils_generate_message_id (const char *fqdn);

/* decode a message-id */
char *g_mime_utils_decode_message_id (const char *message_id);

char  *g_mime_utils_structured_header_fold (GMimeParserOptions *options, GMimeFormatOptions *format, const char *header);
char  *g_mime_utils_unstructured_header_fold (GMimeParserOptions *options, GMimeFormatOptions *format, const char *header);
char  *g_mime_utils_header_printf (GMimeParserOptions *options, GMimeFormatOptions *format, const char *text, ...) G_GNUC_PRINTF (3, 4);
char  *g_mime_utils_header_unfold (const char *value);

char  *g_mime_utils_quote_string (const char *str);
void   g_mime_utils_unquote_string (char *str);

/* encoding decision making utilities ;-) */
gboolean g_mime_utils_text_is_8bit (const unsigned char *text, size_t len);
GMimeContentEncoding g_mime_utils_best_encoding (const unsigned char *text, size_t len);

/* utility function to convert text in an unknown 8bit/multibyte charset to UTF-8 */
char *g_mime_utils_decode_8bit (GMimeParserOptions *options, const char *text, size_t len);

/* utilities to (de/en)code headers */
char *g_mime_utils_header_decode_text (GMimeParserOptions *options, const char *text);
char *g_mime_utils_header_encode_text (GMimeFormatOptions *options, const char *text, const char *charset);

char *g_mime_utils_header_decode_phrase (GMimeParserOptions *options, const char *phrase);
char *g_mime_utils_header_encode_phrase (GMimeFormatOptions *options, const char *phrase, const char *charset);

G_END_DECLS

#endif /* __GMIME_UTILS_H__ */
