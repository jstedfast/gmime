/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*  GMime
 *  Copyright (C) 2000-2007 Jeffrey Stedfast
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

G_BEGIN_DECLS


/**
 * GMimePartEncodingType:
 * @GMIME_PART_ENCODING_DEFAULT: Default transfer encoding.
 * @GMIME_PART_ENCODING_7BIT: 7bit text transfer encoding.
 * @GMIME_PART_ENCODING_8BIT: 8bit text transfer encoding.
 * @GMIME_PART_ENCODING_BINARY: Binary transfer encoding.
 * @GMIME_PART_ENCODING_BASE64: Base64 transfer encoding.
 * @GMIME_PART_ENCODING_QUOTEDPRINTABLE: Quoted-printable transfer encoding.
 * @GMIME_PART_ENCODING_UUENCODE: Uuencode transfer encoding.
 * @GMIME_PART_NUM_ENCODINGS: The number of available transfer encoding enum values.
 *
 * A Content-Transfer-Encoding type.
 **/
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


#define BASE64_ENCODE_LEN(x) ((size_t) ((x) * 5 / 3) + 4)  /* conservative would be ((x * 4 / 3) + 4) */
#define QP_ENCODE_LEN(x)     ((size_t) ((x) * 7 / 2) + 4)  /* conservative would be ((x * 3) + 4) */


/**
 * GMIME_UUDECODE_STATE_INIT:
 *
 * Initial state for the g_mime_utils_uudecode_step() function.
 **/
#define GMIME_UUDECODE_STATE_INIT   (0)


/**
 * GMIME_UUDECODE_STATE_BEGIN:
 *
 * State for the g_mime_utils_uudecode_step() function, denoting that
 * the 'begin' line has been found.
 **/
#define GMIME_UUDECODE_STATE_BEGIN  (1 << 16)


/**
 * GMIME_UUDECODE_STATE_END:
 *
 * State for the g_mime_utils_uudecode_step() function, denoting that
 * the end of the UU encoded block has been found.
 **/
#define GMIME_UUDECODE_STATE_END    (1 << 17)
#define GMIME_UUDECODE_STATE_MASK   (GMIME_UUDECODE_STATE_BEGIN | GMIME_UUDECODE_STATE_END)

time_t g_mime_utils_header_decode_date (const char *in, int *tz_offset);
char  *g_mime_utils_header_format_date (time_t date, int tz_offset);

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

/* utility function to convert text in an unknown 8bit/multibyte charset to UTF-8 */
char *g_mime_utils_decode_8bit (const char *text, size_t len);

/* utilities to (de/en)code headers */
char *g_mime_utils_header_decode_text (const char *in);
char *g_mime_utils_header_encode_text (const char *in);

char *g_mime_utils_header_decode_phrase (const char *in);
char *g_mime_utils_header_encode_phrase (const char *in);

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
size_t g_mime_utils_quoted_decode_step (const unsigned char *in, size_t inlen, unsigned char *out, int *state, guint32 *save);
size_t g_mime_utils_quoted_encode_step (const unsigned char *in, size_t inlen, unsigned char *out, int *state, guint32 *save);
size_t g_mime_utils_quoted_encode_close (const unsigned char *in, size_t inlen, unsigned char *out, int *state, guint32 *save);


G_END_DECLS

#endif /* __GMIME_UTILS_H__ */
