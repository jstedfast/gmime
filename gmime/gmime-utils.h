/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*  GMime
 *  Copyright (C) 2000-2014 Jeffrey Stedfast
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
#include <time.h>
#include <stdarg.h>

#include <gmime/gmime-encodings.h>

G_BEGIN_DECLS

typedef struct _GMimeReferences GMimeReferences;

/**
 * GMimeReferences:
 * @next: Pointer to the next reference.
 * @msgid: Reference message-id.
 *
 * A List of references, as per the References or In-Reply-To header
 * fields.
 **/
struct _GMimeReferences {
	GMimeReferences *next;
	char *msgid;
};


time_t g_mime_utils_header_decode_date (const char *str, int *tz_offset);
char  *g_mime_utils_header_format_date (time_t date, int tz_offset);

char *g_mime_utils_generate_message_id (const char *fqdn);

/* decode a message-id */
char *g_mime_utils_decode_message_id (const char *message_id);

/* decode a References or In-Reply-To header */
GMimeReferences *g_mime_references_decode (const char *text);
void g_mime_references_append (GMimeReferences **refs, const char *msgid);
void g_mime_references_clear (GMimeReferences **refs);
void g_mime_references_free (GMimeReferences *refs);
const GMimeReferences *g_mime_references_get_next (const GMimeReferences *ref);
const char *g_mime_references_get_message_id (const GMimeReferences *ref);

char  *g_mime_utils_structured_header_fold (const char *header);
char  *g_mime_utils_unstructured_header_fold (const char *header);
char  *g_mime_utils_header_fold (const char *header);
char  *g_mime_utils_header_printf (const char *format, ...) G_GNUC_PRINTF (1, 2);

char  *g_mime_utils_quote_string (const char *str);
void   g_mime_utils_unquote_string (char *str);

/* encoding decision making utilities ;-) */
gboolean g_mime_utils_text_is_8bit (const unsigned char *text, size_t len);
GMimeContentEncoding g_mime_utils_best_encoding (const unsigned char *text, size_t len);

/* utility function to convert text in an unknown 8bit/multibyte charset to UTF-8 */
char *g_mime_utils_decode_8bit (const char *text, size_t len);

/* utilities to (de/en)code headers */
char *g_mime_utils_header_decode_text (const char *text);
char *g_mime_utils_header_encode_text (const char *text);

char *g_mime_utils_header_decode_phrase (const char *phrase);
char *g_mime_utils_header_encode_phrase (const char *phrase);

G_END_DECLS

#endif /* __GMIME_UTILS_H__ */
