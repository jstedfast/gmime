/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 *  Authors: Michael Zucchi <notzed@ximian.com>
 *           Jeffrey Stedfast <fejj@ximian.com>
 *
 *  Copyright 2000-2004 Ximian, Inc. (www.ximian.com)
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Street #330, Boston, MA 02111-1307, USA.
 *
 */


#ifndef __GMIME_UTILS_H__
#define __GMIME_UTILS_H__

#include <glib.h>
#include <time.h>
#include <stdarg.h>

#ifdef __cplusplus
extern "C" {
#pragma }
#endif /* __cplusplus */

typedef enum {
	GMIME_PART_ENCODING_DEFAULT,
	GMIME_PART_ENCODING_7BIT,
	GMIME_PART_ENCODING_8BIT,
	GMIME_PART_ENCODING_BINARY,
	GMIME_PART_ENCODING_BASE64,
	GMIME_PART_ENCODING_QUOTEDPRINTABLE,
	GMIME_PART_ENCODING_UUENCODE,
	GMIME_PART_NUM_ENCODINGS
} GMimePartEncodingType;

struct _GMimeReferences {
	struct _GMimeReferences *next;
	char *msgid;
};

typedef struct _GMimeReferences GMimeReferences;

#define BASE64_ENCODE_LEN(x) ((size_t) ((x) * 5 / 3) + 4)  /* conservative would be ((x * 4 / 3) + 4) */
#define QP_ENCODE_LEN(x)     ((size_t) ((x) * 7 / 2) + 4)  /* conservative would be ((x * 3) + 4) */

#define GMIME_UUDECODE_STATE_INIT   (0)
#define GMIME_UUDECODE_STATE_BEGIN  (1 << 16)
#define GMIME_UUDECODE_STATE_END    (1 << 17)
#define GMIME_UUDECODE_STATE_MASK   (GMIME_UUDECODE_STATE_BEGIN | GMIME_UUDECODE_STATE_END)

time_t g_mime_utils_header_decode_date (const char *in, int *saveoffset);
char  *g_mime_utils_header_format_date (time_t time, int offset);

char *g_mime_utils_generate_message_id (const char *fqdn);

/* decode a message-id */
char *g_mime_utils_decode_message_id (const char *message_id);

/* decode a References or In-Reply-To header */
GMimeReferences *g_mime_references_decode (const char *text);
void g_mime_references_append (GMimeReferences **refs, const char *msgid);
void g_mime_references_clear (GMimeReferences **refs);
GMimeReferences *g_mime_references_next (const GMimeReferences *ref);

char  *g_mime_utils_structured_header_fold (const char *in);
char  *g_mime_utils_unstructured_header_fold (const char *in);
char  *g_mime_utils_header_fold (const char *in);
char  *g_mime_utils_header_printf (const char *format, ...);

char  *g_mime_utils_quote_string (const char *string);
void   g_mime_utils_unquote_string (char *string);

/* encoding decision making utilities ;-) */
gboolean g_mime_utils_text_is_8bit (const unsigned char *text, size_t len);
GMimePartEncodingType g_mime_utils_best_encoding (const unsigned char *text, size_t len);

/* utilities to (de/en)code headers */
char *g_mime_utils_header_decode_text (const unsigned char *in);
char *g_mime_utils_header_encode_text (const unsigned char *in);

char *g_mime_utils_header_decode_phrase (const unsigned char *in);
char *g_mime_utils_header_encode_phrase (const unsigned char *in);

#ifndef GMIME_DISABLE_DEPRECATED
char *g_mime_utils_8bit_header_decode (const unsigned char *in);
char *g_mime_utils_8bit_header_encode (const unsigned char *in);
char *g_mime_utils_8bit_header_encode_phrase (const unsigned char *in);
#endif

/* do incremental base64 (de/en)coding */
size_t g_mime_utils_base64_decode_step (const unsigned char *in, size_t inlen, unsigned char *out, int *state, guint32 *save);
size_t g_mime_utils_base64_encode_step (const unsigned char *in, size_t inlen, unsigned char *out, int *state, guint32 *save);
size_t g_mime_utils_base64_encode_close (const unsigned char *in, size_t inlen, unsigned char *out, int *state, guint32 *save);

/* do incremental uu (de/en)coding */
size_t g_mime_utils_uudecode_step (const unsigned char *in, size_t inlen, unsigned char *out, int *state, guint32 *save);
size_t g_mime_utils_uuencode_step (const unsigned char *in, size_t inlen, unsigned char *out, unsigned char *uubuf, int *state, guint32 *save);
size_t g_mime_utils_uuencode_close (const unsigned char *in, size_t inlen, unsigned char *out, unsigned char *uubuf, int *state, guint32 *save);

/* do incremental quoted-printable (de/en)coding */
size_t g_mime_utils_quoted_decode_step (const unsigned char *in, size_t inlen, unsigned char *out, int *savestate, int *saved);
size_t g_mime_utils_quoted_encode_step (const unsigned char *in, size_t inlen, unsigned char *out, int *state, int *save);
size_t g_mime_utils_quoted_encode_close (const unsigned char *in, size_t inlen, unsigned char *out, int *state, int *save);


#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* __GMIME_UTILS_H__ */
