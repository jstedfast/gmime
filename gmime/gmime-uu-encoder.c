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
#include <string.h>
#include "gmime-codecs.h"
#include "gmime-codec-private.h"

typedef struct {
	GMimeNewLineFormat newline_format;
	guint buffered;
	guint8 buffer[45];
} UUEncoderState;

struct _GMimeUUEncoder {
	GObject parent_instance;
	UUEncoderState state;
};

static void uu_encoder_iface_init (GMimeEncoderInterface *iface);

G_DEFINE_FINAL_TYPE_WITH_CODE (GMimeUUEncoder, g_mime_uu_encoder, G_TYPE_OBJECT,
	G_IMPLEMENT_INTERFACE (GMIME_TYPE_ENCODER, uu_encoder_iface_init))

static void
uu_encoder_reset (GMimeEncoder *encoder)
{
	UUEncoderState *state = &GMIME_UU_ENCODER (encoder)->state;

	state->buffered = 0;
	memset (state->buffer, 0, sizeof (state->buffer));
}

static void
g_mime_uu_encoder_class_init (GMimeUUEncoderClass *klass)
{
}

static void
g_mime_uu_encoder_init (GMimeUUEncoder *self)
{
	self->state.newline_format = GMIME_NEWLINE_FORMAT_UNIX;
	uu_encoder_reset (GMIME_ENCODER (self));
}

static GMimeContentEncoding
uu_encoder_encoding (GMimeEncoder *encoder)
{
	return GMIME_CONTENT_ENCODING_UUENCODE;
}

static GMimeEncoder *
uu_encoder_clone (GMimeEncoder *encoder)
{
	GMimeUUEncoder *copy = g_mime_uu_encoder_new ();

	copy->state = GMIME_UU_ENCODER (encoder)->state;
	return GMIME_ENCODER (copy);
}

static gsize
uu_encoder_estimate (GMimeEncoder *encoder, gsize length)
{
	UUEncoderState *state = &GMIME_UU_ENCODER (encoder)->state;
	guint newline_length = state->newline_format == GMIME_NEWLINE_FORMAT_DOS ? 2 : 1;
	guint line_length = 61 + newline_length;
	gsize groups, estimate;

	/* A previous call may have buffered up to 44 bytes; include the completed
	 * lines and the final partial and terminating lines in the estimate.
	 * Every line, including the terminator, uses the configured newline. */
	if (!g_size_checked_add (&groups, length, 44) ||
	    !g_size_checked_mul (&estimate, groups / 45, line_length) ||
	    !g_size_checked_add (&estimate, estimate, line_length + 1 + newline_length))
		return G_MAXSIZE;
	return estimate;
}

static gsize
uu_write_line (UUEncoderState *state, guint8 *output)
{
	guint8 padded[3] = { 0, 0, 0 };
	gsize written = 1, offset = 0, n;

	output[0] = state->buffered + 32;
	n = _g_mime_codec_encode_blocks (state->buffer, state->buffered, output + written, 15, TRUE);
	offset += n;
	written += n / 3 * 4;
	while (state->buffered - offset >= 3) {
		codec_encode_triplet (state->buffer + offset, output + written, TRUE);
		written += 4;
		offset += 3;
	}
	if (offset < state->buffered) {
		memcpy (padded, state->buffer + offset, state->buffered - offset);
		codec_encode_triplet (padded, output + written, TRUE);
		written += 4;
	}
	if (state->newline_format == GMIME_NEWLINE_FORMAT_DOS)
		output[written++] = '\r';
	output[written++] = '\n';
	state->buffered = 0;
	return written;
}

static gsize
uu_encoder_encode (GMimeEncoder *encoder, const guint8 *input, gsize length,
		   guint8 *output, gsize capacity)
{
	UUEncoderState *state = &GMIME_UU_ENCODER (encoder)->state;
	gsize n, written = 0;

	while (length) {
		n = MIN (length, 45 - state->buffered);
		memcpy (state->buffer + state->buffered, input, n);
		state->buffered += n;
		input += n;
		length -= n;
		if (state->buffered == 45)
			written += uu_write_line (state, output + written);
	}
	return written;
}

static gsize
uu_encoder_flush (GMimeEncoder *encoder, const guint8 *input, gsize length,
		  guint8 *output, gsize capacity)
{
	UUEncoderState *state = &GMIME_UU_ENCODER (encoder)->state;
	gsize written = uu_encoder_encode (encoder, input, length, output, capacity);

	if (state->buffered)
		written += uu_write_line (state, output + written);
	/* A zero-length line terminates the payload; begin/end markers belong to the caller. */
	output[written++] = '`';
	if (state->newline_format == GMIME_NEWLINE_FORMAT_DOS)
		output[written++] = '\r';
	output[written++] = '\n';
	uu_encoder_reset (encoder);
	return written;
}

static void
uu_encoder_iface_init (GMimeEncoderInterface *iface)
{
	iface->get_encoding = uu_encoder_encoding;
	iface->clone = uu_encoder_clone;
	iface->estimate_output_length = uu_encoder_estimate;
	iface->encode = uu_encoder_encode;
	iface->flush = uu_encoder_flush;
	iface->reset = uu_encoder_reset;
}

/**
 * g_mime_uu_encoder_new:
 *
 * Emits payload lines and a zero-length terminating line, not begin/end markers.
 * Uses LF line endings for compatibility with existing native callers.
 *
 * Returns: (transfer full): a uuencoder.
 **/
GMimeUUEncoder *
g_mime_uu_encoder_new (void)
{
	return g_object_new (GMIME_TYPE_UU_ENCODER, NULL);
}

/**
 * g_mime_uu_encoder_new_with_newline_format:
 * @newline_format: LF or CRLF line endings
 *
 * Creates a uuencoder using the selected line endings for payload lines
 * and the zero-length terminating line. It does not emit begin/end markers.
 * Clone, reset and flush preserve the configuration.
 *
 * Returns: (transfer full): a uuencoder.
 **/
GMimeUUEncoder *
g_mime_uu_encoder_new_with_newline_format (GMimeNewLineFormat newline_format)
{
	GMimeUUEncoder *encoder;

	g_return_val_if_fail (newline_format == GMIME_NEWLINE_FORMAT_UNIX || newline_format == GMIME_NEWLINE_FORMAT_DOS, NULL);

	encoder = g_mime_uu_encoder_new ();
	encoder->state.newline_format = newline_format;
	return encoder;
}
