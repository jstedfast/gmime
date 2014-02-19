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


#ifndef __GMIME_ENCODINGS_H__
#define __GMIME_ENCODINGS_H__

#include <glib.h>
#include <sys/types.h>

G_BEGIN_DECLS


/**
 * GMimeContentEncoding:
 * @GMIME_CONTENT_ENCODING_DEFAULT: Default transfer encoding.
 * @GMIME_CONTENT_ENCODING_7BIT: 7bit text transfer encoding.
 * @GMIME_CONTENT_ENCODING_8BIT: 8bit text transfer encoding.
 * @GMIME_CONTENT_ENCODING_BINARY: Binary transfer encoding.
 * @GMIME_CONTENT_ENCODING_BASE64: Base64 transfer encoding.
 * @GMIME_CONTENT_ENCODING_QUOTEDPRINTABLE: Quoted-printable transfer encoding.
 * @GMIME_CONTENT_ENCODING_UUENCODE: Uuencode transfer encoding.
 *
 * A Content-Transfer-Encoding enumeration.
 **/
typedef enum {
	GMIME_CONTENT_ENCODING_DEFAULT,
	GMIME_CONTENT_ENCODING_7BIT,
	GMIME_CONTENT_ENCODING_8BIT,
	GMIME_CONTENT_ENCODING_BINARY,
	GMIME_CONTENT_ENCODING_BASE64,
	GMIME_CONTENT_ENCODING_QUOTEDPRINTABLE,
	GMIME_CONTENT_ENCODING_UUENCODE
} GMimeContentEncoding;


/**
 * GMimeEncodingConstraint:
 * @GMIME_ENCODING_CONSTRAINT_7BIT: The stream data must fit within the 7bit ASCII range.
 * @GMIME_ENCODING_CONSTRAINT_8BIT: The stream data may have bytes with the high bit set, but no null bytes.
 * @GMIME_ENCODING_CONSTRAINT_BINARY: The stream may contain any binary data.
 *
 * Used with functions like g_mime_filter_best_encoding() and
 * g_mime_object_encode() as the 'constraint' argument. These values
 * provide a means of letting the filter know what the encoding
 * constraints are for the stream.
 **/
typedef enum {
	GMIME_ENCODING_CONSTRAINT_7BIT,
	GMIME_ENCODING_CONSTRAINT_8BIT,
	GMIME_ENCODING_CONSTRAINT_BINARY
} GMimeEncodingConstraint;


GMimeContentEncoding g_mime_content_encoding_from_string (const char *str);
const char *g_mime_content_encoding_to_string (GMimeContentEncoding encoding);


/**
 * GMIME_BASE64_ENCODE_LEN:
 * @x: Length of the input data to encode
 *
 * Calculates the maximum number of bytes needed to base64 encode the
 * full input buffer of length @x.
 *
 * Returns: the number of output bytes needed to base64 encode an input
 * buffer of size @x.
 **/
#define GMIME_BASE64_ENCODE_LEN(x) ((size_t) (((((x) + 2) / 57) * 77) + 77))


/**
 * GMIME_QP_ENCODE_LEN:
 * @x: Length of the input data to encode
 *
 * Calculates the maximum number of bytes needed to encode the full
 * input buffer of length @x using the quoted-printable encoding.
 *
 * Returns: the number of output bytes needed to encode an input buffer
 * of size @x using the quoted-printable encoding.
 **/
#define GMIME_QP_ENCODE_LEN(x)     ((size_t) ((((x) / 24) * 74) + 74))


/**
 * GMIME_UUENCODE_LEN:
 * @x: Length of the input data to encode
 *
 * Calculates the maximum number of bytes needed to uuencode the full
 * input buffer of length @x.
 *
 * Returns: the number of output bytes needed to uuencode an input
 * buffer of size @x.
 **/
#define GMIME_UUENCODE_LEN(x)      ((size_t) (((((x) + 2) / 45) * 62) + 64))


/**
 * GMIME_UUDECODE_STATE_INIT:
 *
 * Initial state for the g_mime_encoding_uudecode_step() function.
 **/
#define GMIME_UUDECODE_STATE_INIT   (0)


/**
 * GMIME_UUDECODE_STATE_BEGIN:
 *
 * State for the g_mime_encoding_uudecode_step() function, denoting that
 * the 'begin' line has been found.
 **/
#define GMIME_UUDECODE_STATE_BEGIN  (1 << 16)


/**
 * GMIME_UUDECODE_STATE_END:
 *
 * State for the g_mime_encoding_uudecode_step() function, denoting that
 * the end of the UU encoded block has been found.
 **/
#define GMIME_UUDECODE_STATE_END    (1 << 17)
#define GMIME_UUDECODE_STATE_MASK   (GMIME_UUDECODE_STATE_BEGIN | GMIME_UUDECODE_STATE_END)

typedef struct _GMimeEncoding GMimeEncoding;

/**
 * GMimeEncoding:
 * @encoding: the type of encoding
 * @uubuf: a temporary buffer needed when uuencoding data
 * @encode: %TRUE if encoding or %FALSE if decoding
 * @save: saved bytes from the previous step
 * @state: current encder/decoder state
 *
 * A context used for encoding or decoding data.
 **/
struct _GMimeEncoding {
	GMimeContentEncoding encoding;
	unsigned char uubuf[60];
	gboolean encode;
	guint32 save;
	int state;
};


void g_mime_encoding_init_encode (GMimeEncoding *state, GMimeContentEncoding encoding);
void g_mime_encoding_init_decode (GMimeEncoding *state, GMimeContentEncoding encoding);
void g_mime_encoding_reset (GMimeEncoding *state);

size_t g_mime_encoding_outlen (GMimeEncoding *state, size_t inlen);

size_t g_mime_encoding_step (GMimeEncoding *state, const char *inbuf, size_t inlen, char *outbuf);
size_t g_mime_encoding_flush (GMimeEncoding *state, const char *inbuf, size_t inlen, char *outbuf);


/* do incremental base64 (de/en)coding */
size_t g_mime_encoding_base64_decode_step (const unsigned char *inbuf, size_t inlen, unsigned char *outbuf, int *state, guint32 *save);
size_t g_mime_encoding_base64_encode_step (const unsigned char *inbuf, size_t inlen, unsigned char *outbuf, int *state, guint32 *save);
size_t g_mime_encoding_base64_encode_close (const unsigned char *inbuf, size_t inlen, unsigned char *outbuf, int *state, guint32 *save);

/* do incremental uu (de/en)coding */
size_t g_mime_encoding_uudecode_step (const unsigned char *inbuf, size_t inlen, unsigned char *outbuf, int *state, guint32 *save);
size_t g_mime_encoding_uuencode_step (const unsigned char *inbuf, size_t inlen, unsigned char *outbuf, unsigned char *uubuf, int *state, guint32 *save);
size_t g_mime_encoding_uuencode_close (const unsigned char *inbuf, size_t inlen, unsigned char *outbuf, unsigned char *uubuf, int *state, guint32 *save);

/* do incremental quoted-printable (de/en)coding */
size_t g_mime_encoding_quoted_decode_step (const unsigned char *inbuf, size_t inlen, unsigned char *outbuf, int *state, guint32 *save);
size_t g_mime_encoding_quoted_encode_step (const unsigned char *inbuf, size_t inlen, unsigned char *outbuf, int *state, guint32 *save);
size_t g_mime_encoding_quoted_encode_close (const unsigned char *inbuf, size_t inlen, unsigned char *outbuf, int *state, guint32 *save);


G_END_DECLS

#endif /* __GMIME_ENCODINGS_H__ */
