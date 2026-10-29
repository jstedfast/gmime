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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif
#include "gmime-codecs.h"

enum {
	QP_PASS_THROUGH,
	QP_EQUAL_SIGN,
	QP_SOFT_BREAK,
	QP_DECODE_BYTE
};

typedef struct {
	gboolean rfc2047;
	guint state;
	guint8 saved;
} QPDecoderState;

struct _GMimeQuotedPrintableDecoder {
	GObject parent_instance;
	QPDecoderState state;
};

static void qp_decoder_iface_init (GMimeDecoderInterface *iface);

G_DEFINE_FINAL_TYPE_WITH_CODE (GMimeQuotedPrintableDecoder, g_mime_quoted_printable_decoder, G_TYPE_OBJECT,
	G_IMPLEMENT_INTERFACE (GMIME_TYPE_DECODER, qp_decoder_iface_init))

static void
qp_decoder_reset (GMimeDecoder *decoder)
{
	QPDecoderState *state = &GMIME_QUOTED_PRINTABLE_DECODER (decoder)->state;

	state->state = QP_PASS_THROUGH;
	state->saved = 0;
}

static void
g_mime_quoted_printable_decoder_class_init (GMimeQuotedPrintableDecoderClass *klass)
{
}

static void
g_mime_quoted_printable_decoder_init (GMimeQuotedPrintableDecoder *self)
{
	qp_decoder_reset (GMIME_DECODER (self));
}

static GMimeContentEncoding
qp_decoder_encoding (GMimeDecoder *decoder)
{
	return GMIME_CONTENT_ENCODING_QUOTEDPRINTABLE;
}

static GMimeDecoder *
qp_decoder_clone (GMimeDecoder *decoder)
{
	GMimeQuotedPrintableDecoder *self = GMIME_QUOTED_PRINTABLE_DECODER (decoder);
	GMimeQuotedPrintableDecoder *copy = g_mime_quoted_printable_decoder_new (self->state.rfc2047);

	copy->state = self->state;

	return GMIME_DECODER (copy);
}

static gsize
qp_decoder_estimate (GMimeDecoder *decoder, gsize length)
{
	QPDecoderState *state = &GMIME_QUOTED_PRINTABLE_DECODER (decoder)->state;
	gsize extra = state->state == QP_PASS_THROUGH ? 0 : state->state == QP_EQUAL_SIGN ? 1 : 2;

	return g_size_checked_add (&length, length, extra) ? length : G_MAXSIZE;
}

static gint
hex_rank (guint8 c)
{
	if (c >= '0' && c <= '9') return c - '0';
	if (c >= 'A' && c <= 'F') return c - 'A' + 10;
	if (c >= 'a' && c <= 'f') return c - 'a' + 10;
	return -1;
}

static gsize
qp_decoder_decode (GMimeDecoder *decoder, const guint8 *input, gsize length,
		   guint8 *output, gsize capacity)
{
	QPDecoderState *state = &GMIME_QUOTED_PRINTABLE_DECODER (decoder)->state;
	gsize i, written = 0;
	guint8 c;
	gint rank;

	(void) capacity;
	for (i = 0; i < length; i++) {
		c = input[i];
		switch (state->state) {
		case QP_PASS_THROUGH:
			if (c == '=')
				state->state = QP_EQUAL_SIGN;
			else
				output[written++] = state->rfc2047 && c == '_' ? ' ' : c;
			break;
		case QP_EQUAL_SIGN:
			if (hex_rank (c) >= 0) {
				state->saved = c;
				state->state = QP_DECODE_BYTE;
			} else if (c == '=') {
				output[written++] = '=';
			} else if (c == '\r') {
				state->state = QP_SOFT_BREAK;
			} else {
				if (c != '\n') {
					/* Preserve invalid escape sequences literally instead of losing input. */
					output[written++] = '=';
					output[written++] = c;
				}
				state->state = QP_PASS_THROUGH;
			}
			break;
		case QP_SOFT_BREAK:
			if (c != '\n') {
				/* An '=' and CR not followed by LF are not a soft line break. */
				output[written++] = '=';
				output[written++] = '\r';
				output[written++] = c;
			}
			state->state = QP_PASS_THROUGH;
			break;
		case QP_DECODE_BYTE:
			if ((rank = hex_rank (c)) >= 0) {
				output[written++] = (hex_rank (state->saved) << 4) | rank;
			} else {
				/* A malformed hex triplet is passed through undecoded. */
				output[written++] = '=';
				output[written++] = state->saved;
				output[written++] = c;
			}
			state->state = QP_PASS_THROUGH;
			break;
		}
	}
	return written;
}

static void
qp_decoder_iface_init (GMimeDecoderInterface *iface)
{
	iface->get_encoding = qp_decoder_encoding;
	iface->clone = qp_decoder_clone;
	iface->estimate_output_length = qp_decoder_estimate;
	iface->decode = qp_decoder_decode;
	iface->reset = qp_decoder_reset;
}

/**
 * g_mime_quoted_printable_decoder_new:
 * @rfc2047: whether to map unescaped underscores to spaces in encoded words
 *
 * Returns: (transfer full): a quoted-printable decoder.
 **/
GMimeQuotedPrintableDecoder *
g_mime_quoted_printable_decoder_new (gboolean rfc2047)
{
	GMimeQuotedPrintableDecoder *decoder = g_object_new (GMIME_TYPE_QUOTED_PRINTABLE_DECODER, NULL);

	decoder->state.rfc2047 = rfc2047;
	return decoder;
}
