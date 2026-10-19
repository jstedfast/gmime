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

typedef struct {
	guint max_line_length;
	guint column;
	gint saved;
	gboolean saved_cr;
} QPEncoderState;

struct _GMimeQuotedPrintableEncoder {
	GObject parent_object;
	QPEncoderState state;
};

static const guint8 hex_alphabet[] = "0123456789ABCDEF";
static void qp_encoder_iface_init (GMimeEncoderInterface *iface);

G_DEFINE_FINAL_TYPE_WITH_CODE (GMimeQuotedPrintableEncoder, g_mime_quoted_printable_encoder, G_TYPE_OBJECT,
	G_IMPLEMENT_INTERFACE (GMIME_TYPE_ENCODER, qp_encoder_iface_init))

static void
qp_encoder_reset (GMimeEncoder *encoder)
{
	QPEncoderState *state = &GMIME_QUOTED_PRINTABLE_ENCODER (encoder)->state;

	state->column = 0;
	state->saved = -1;
	state->saved_cr = FALSE;
}

static void
g_mime_quoted_printable_encoder_class_init (GMimeQuotedPrintableEncoderClass *klass)
{
}

static void
g_mime_quoted_printable_encoder_init (GMimeQuotedPrintableEncoder *self)
{
	self->state.max_line_length = 76;
	qp_encoder_reset (GMIME_ENCODER (self));
}

static GMimeContentEncoding
qp_encoder_encoding (GMimeEncoder *encoder)
{
	return GMIME_CONTENT_ENCODING_QUOTEDPRINTABLE;
}

static GMimeEncoder *
qp_encoder_clone (GMimeEncoder *encoder)
{
	GMimeQuotedPrintableEncoder *copy = g_mime_quoted_printable_encoder_new ();

	copy->state = GMIME_QUOTED_PRINTABLE_ENCODER (encoder)->state;
	return GMIME_ENCODER (copy);
}

static gsize
qp_encoder_estimate (GMimeEncoder *encoder, gsize length)
{
	QPEncoderState *state = &GMIME_QUOTED_PRINTABLE_ENCODER (encoder)->state;
	guint per_line = (state->max_line_length - 1) / 3;
	gsize estimate, tail;

	/* Every input byte may become a triplet. Include saved input, an already
	 * started line and the trailing soft break that flush may append. */
	if (!g_size_checked_add (&length, length, (state->saved != -1) + state->saved_cr) ||
	    !g_size_checked_mul (&estimate, length / per_line, per_line * 3 + 2))
		return G_MAXSIZE;
	tail = (length % per_line) * 3 + 2 + (state->column ? 2 : 0);
	return g_size_checked_add (&estimate, estimate, tail) ? estimate : G_MAXSIZE;
}

static gsize
qp_write_saved (QPEncoderState *state, guint8 *output, gboolean end_of_line, gboolean encode_blank)
{
	guint8 c = state->saved;
	gboolean blank = c == ' ' || c == '\t';
	gboolean encode = !(blank || (c >= 33 && c <= 126 && c != '=')) || (encode_blank && blank);
	guint length = encode ? 3 : 1;
	gsize written = 0;

	/* Only the last octet on a hard line may use the final column; all others
	 * must leave room for the '=' of a possible soft break. */
	if (state->column + length + (end_of_line ? 0 : 1) > state->max_line_length) {
		output[written++] = '=';
		output[written++] = '\n';
		state->column = 0;
	}
	if (encode) {
		output[written++] = '=';
		output[written++] = hex_alphabet[c >> 4];
		output[written++] = hex_alphabet[c & 15];
	} else {
		output[written++] = c;
	}
	state->column += length;
	state->saved = -1;
	return written;
}

static gsize
qp_encoder_encode (GMimeEncoder *encoder, const guint8 *input, gsize length,
		   guint8 *output, gsize capacity)
{
	QPEncoderState *state = &GMIME_QUOTED_PRINTABLE_ENCODER (encoder)->state;
	gsize written = 0, i;
	guint8 c;

	for (i = 0; i < length; i++) {
		c = input[i];

		if (state->saved_cr) {
			state->saved_cr = FALSE;

			if (c == '\n') {
				/* Spaces and tabs at the end of a line must be encoded. */
				if (state->saved != -1)
					written += qp_write_saved (state, output + written, TRUE, TRUE);
				output[written++] = '\n';
				state->column = 0;
				continue;
			}

			/* A lone CR is content, not a line break, and must be encoded. */
			if (state->saved != -1)
				written += qp_write_saved (state, output + written, FALSE, FALSE);
			state->saved = '\r';
		}

		if (c == '\r') {
			state->saved_cr = TRUE;
		} else if (c == '\n') {
			/* The same trailing-whitespace rule applies to bare LF line endings. */
			if (state->saved != -1)
				written += qp_write_saved (state, output + written, TRUE, TRUE);

			output[written++] = '\n';
			state->column = 0;
		} else {
			if (state->saved != -1) {
				written += qp_write_saved (state, output + written, FALSE, FALSE);

				if (state->column + 1 >= state->max_line_length) {
					output[written++] = '=';
					output[written++] = '\n';
					state->column = 0;
				}
			}

			state->saved = c;
		}
	}
	return written;
}

static gsize
qp_encoder_flush (GMimeEncoder *encoder, const guint8 *input, gsize length,
		  guint8 *output, gsize capacity)
{
	QPEncoderState *state = &GMIME_QUOTED_PRINTABLE_ENCODER (encoder)->state;
	gsize written = qp_encoder_encode (encoder, input, length, output, capacity);

	if (state->saved_cr) {
		if (state->saved != -1)
			written += qp_write_saved (state, output + written, FALSE, FALSE);
		state->saved = '\r';
	}

	if (state->saved != -1) {
		/* Encode trailing spaces/tabs, and terminate with a soft break so the
		 * added newline does not become part of the decoded content. */
		written += qp_write_saved (state, output + written, FALSE, TRUE);
		output[written++] = '=';
		output[written++] = '\n';
	}

	qp_encoder_reset (encoder);

	return written;
}

static void
qp_encoder_iface_init (GMimeEncoderInterface *iface)
{
	iface->get_encoding = qp_encoder_encoding;
	iface->clone = qp_encoder_clone;
	iface->estimate_output_length = qp_encoder_estimate;
	iface->encode = qp_encoder_encode;
	iface->flush = qp_encoder_flush;
	iface->reset = qp_encoder_reset;
}

/**
 * g_mime_quoted_printable_encoder_new:
 *
 * Returns: (transfer full): a quoted-printable encoder using 76-character lines.
 **/
GMimeQuotedPrintableEncoder *
g_mime_quoted_printable_encoder_new (void)
{
	return g_object_new (GMIME_TYPE_QUOTED_PRINTABLE_ENCODER, NULL);
}

/**
 * g_mime_quoted_printable_encoder_new_with_max_line_length:
 * @max_line_length: a line length between 60 and 998
 *
 * The effective length is capped at 76.
 *
 * Returns: (transfer full): a quoted-printable encoder.
 **/
GMimeQuotedPrintableEncoder *
g_mime_quoted_printable_encoder_new_with_max_line_length (guint max_line_length)
{
	GMimeQuotedPrintableEncoder *encoder;

	g_return_val_if_fail (max_line_length >= 60 && max_line_length <= 998, NULL);

	encoder = g_mime_quoted_printable_encoder_new ();
	/* RFC 2045 caps quoted-printable lines at 76 characters. */
	encoder->state.max_line_length = MIN (max_line_length, 76);
	return encoder;
}
