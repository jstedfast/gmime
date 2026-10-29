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
	guint max_line_length;
	guint column;
	guint buffered;
	guint8 buffer[3];
} Base64EncoderState;

struct _GMimeBase64Encoder {
	GObject parent_instance;
	Base64EncoderState state;
};

static void base64_encoder_iface_init (GMimeEncoderInterface *iface);

G_DEFINE_FINAL_TYPE_WITH_CODE (GMimeBase64Encoder, g_mime_base64_encoder, G_TYPE_OBJECT,
	G_IMPLEMENT_INTERFACE (GMIME_TYPE_ENCODER, base64_encoder_iface_init))

static void
base64_encoder_reset (GMimeEncoder *encoder)
{
	Base64EncoderState *state = &GMIME_BASE64_ENCODER (encoder)->state;

	state->column = state->buffered = 0;
	memset (state->buffer, 0, sizeof (state->buffer));
}

static void
g_mime_base64_encoder_class_init (GMimeBase64EncoderClass *klass)
{
}

static void
g_mime_base64_encoder_init (GMimeBase64Encoder *self)
{
	self->state.newline_format = GMIME_NEWLINE_FORMAT_UNIX;
	self->state.max_line_length = 76;
	base64_encoder_reset (GMIME_ENCODER (self));
}

static GMimeContentEncoding
base64_encoder_encoding (GMimeEncoder *encoder)
{
	return GMIME_CONTENT_ENCODING_BASE64;
}

static GMimeEncoder *
base64_encoder_clone (GMimeEncoder *encoder)
{
	GMimeBase64Encoder *copy = g_mime_base64_encoder_new ();

	copy->state = GMIME_BASE64_ENCODER (encoder)->state;
	return GMIME_ENCODER (copy);
}

static gsize
base64_encoder_estimate (GMimeEncoder *encoder, gsize length)
{
	Base64EncoderState *state = &GMIME_BASE64_ENCODER (encoder)->state;
	guint per_line = state->max_line_length / 4;
	guint newline_length = state->newline_format == GMIME_NEWLINE_FORMAT_DOS ? 2 : 1;
	guint line_length = per_line * 4 + newline_length;
	gsize groups, estimate;

	/* Saved input can complete a nearly full line, and flushing can terminate
	 * another. Reserve an extra newline beyond the input-only line count,
	 * including both bytes when using CRLF. */
	if (!g_size_checked_add (&groups, length, 2) ||
	    !g_size_checked_mul (&estimate, groups / (per_line * 3), line_length) ||
	    !g_size_checked_add (&estimate, estimate, line_length + newline_length))
		return G_MAXSIZE;
	return estimate;
}

static gsize
base64_encoder_encode (GMimeEncoder *encoder, const guint8 *input, gsize length,
		       guint8 *output, gsize capacity)
{
	Base64EncoderState *state = &GMIME_BASE64_ENCODER (encoder)->state;
	guint per_line = state->max_line_length / 4;
	gsize written = 0, n;

	while (length) {
		if (!state->buffered && length >= 16 && per_line - state->column >= 4) {
			n = _g_mime_codec_encode_blocks (input, length, output + written,
							per_line - state->column, FALSE);
			input += n;
			length -= n;
			written += n / 3 * 4;
			state->column += n / 3;
			if (state->column == per_line) {
				if (state->newline_format == GMIME_NEWLINE_FORMAT_DOS)
					output[written++] = '\r';
				output[written++] = '\n';
				state->column = 0;
			}
			if (n)
				continue;
		}

		state->buffer[state->buffered++] = *input++;
		length--;
		if (state->buffered == 3) {
			codec_encode_triplet (state->buffer, output + written, FALSE);
			written += 4;
			state->buffered = 0;
			if (++state->column == per_line) {
				if (state->newline_format == GMIME_NEWLINE_FORMAT_DOS)
					output[written++] = '\r';
				output[written++] = '\n';
				state->column = 0;
			}
		}
	}
	return written;
}

static gsize
base64_encoder_flush (GMimeEncoder *encoder, const guint8 *input, gsize length,
		      guint8 *output, gsize capacity)
{
	Base64EncoderState *state = &GMIME_BASE64_ENCODER (encoder)->state;
	gsize written = base64_encoder_encode (encoder, input, length, output, capacity);
	guint buffered;

	if ((buffered = state->buffered)) {
		/* Only the final incomplete triplet is padded; the saved bytes span calls. */
		memset (state->buffer + buffered, 0, 3 - buffered);
		codec_encode_triplet (state->buffer, output + written, FALSE);
		output[written + 3] = '=';
		if (buffered == 1)
			output[written + 2] = '=';
		written += 4;
		state->column++;
	}

	if (state->column) {
		if (state->newline_format == GMIME_NEWLINE_FORMAT_DOS)
			output[written++] = '\r';
		output[written++] = '\n';
	}

	base64_encoder_reset (encoder);

	return written;
}

static void
base64_encoder_iface_init (GMimeEncoderInterface *iface)
{
	iface->get_encoding = base64_encoder_encoding;
	iface->clone = base64_encoder_clone;
	iface->estimate_output_length = base64_encoder_estimate;
	iface->encode = base64_encoder_encode;
	iface->flush = base64_encoder_flush;
	iface->reset = base64_encoder_reset;
}

/**
 * g_mime_base64_encoder_new:
 *
 * Uses LF line endings for compatibility with existing native callers.
 *
 * Returns: (transfer full): a Base64 encoder using 76-character lines.
 **/
GMimeBase64Encoder *
g_mime_base64_encoder_new (void)
{
	return g_object_new (GMIME_TYPE_BASE64_ENCODER, NULL);
}

/**
 * g_mime_base64_encoder_new_with_max_line_length:
 * @max_line_length: a line length between 60 and 998
 *
 * The effective length is capped at 76 and rounded down to a multiple of four.
 * Uses LF line endings.
 *
 * Returns: (transfer full): a Base64 encoder.
 **/
GMimeBase64Encoder *
g_mime_base64_encoder_new_with_max_line_length (guint max_line_length)
{
	return g_mime_base64_encoder_new_with_newline_format (GMIME_NEWLINE_FORMAT_UNIX, max_line_length);
}

/**
 * g_mime_base64_encoder_new_with_newline_format:
 * @newline_format: LF or CRLF line endings
 * @max_line_length: a line length between 60 and 998
 *
 * Creates a Base64 encoder with the selected line endings. The effective
 * line length excludes the terminator, is capped at 76, and is rounded down
 * to a multiple of four. Clone, reset and flush preserve the configuration.
 *
 * Returns: (transfer full): a Base64 encoder.
 **/
GMimeBase64Encoder *
g_mime_base64_encoder_new_with_newline_format (GMimeNewLineFormat newline_format, guint max_line_length)
{
	GMimeBase64Encoder *encoder;

	g_return_val_if_fail (newline_format == GMIME_NEWLINE_FORMAT_UNIX || newline_format == GMIME_NEWLINE_FORMAT_DOS, NULL);
	g_return_val_if_fail (max_line_length >= 60 && max_line_length <= 998, NULL);

	encoder = g_mime_base64_encoder_new ();
	encoder->state.newline_format = newline_format;
	/* RFC 2045 caps base64 lines at 76 characters, even with a larger format limit. */
	encoder->state.max_line_length = MIN (max_line_length, 76);
	return encoder;
}
