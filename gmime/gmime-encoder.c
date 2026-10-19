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
#include "gmime-codec-private.h"

/**
 * SECTION: gmime-encoder
 * @title: GMimeEncoder
 * @short_description: Incremental MIME encoder interface
 *
 * Encoders support state-preserving clones and caller-owned output buffers.
 * Allocate the estimated output capacity before encoding or flushing.
 * Overlapping buffers are supported, copying the input when necessary.
 **/

G_DEFINE_INTERFACE (GMimeEncoder, g_mime_encoder, G_TYPE_OBJECT)

static void
g_mime_encoder_default_init (GMimeEncoderInterface *iface)
{
}

/**
 * g_mime_encoder_get_encoding:
 * @encoder: an encoder
 *
 * Returns: the content encoding.
 **/
GMimeContentEncoding
g_mime_encoder_get_encoding (GMimeEncoder *encoder)
{
	g_return_val_if_fail (GMIME_IS_ENCODER (encoder), GMIME_CONTENT_ENCODING_UNSPECIFIED);
	return GMIME_ENCODER_GET_IFACE (encoder)->get_encoding (encoder);
}

/**
 * g_mime_encoder_clone:
 * @encoder: an encoder
 *
 * Returns: (transfer full): an independent encoder with identical state.
 **/
GMimeEncoder *
g_mime_encoder_clone (GMimeEncoder *encoder)
{
	g_return_val_if_fail (GMIME_IS_ENCODER (encoder), NULL);
	return GMIME_ENCODER_GET_IFACE (encoder)->clone (encoder);
}

/**
 * g_mime_encoder_estimate_output_length:
 * @encoder: an encoder
 * @input_length: number of input bytes
 *
 * Includes buffered input and finalization overhead without changing state.
 *
 * Returns: the required capacity, or %G_MAXSIZE if it is not representable.
 **/
gsize
g_mime_encoder_estimate_output_length (GMimeEncoder *encoder, gsize input_length)
{
	g_return_val_if_fail (GMIME_IS_ENCODER (encoder), 0);
	return GMIME_ENCODER_GET_IFACE (encoder)->estimate_output_length (encoder, input_length);
}

static gsize
encoder_process (GMimeEncoder *encoder, const guint8 *input, gsize input_length,
		 guint8 *output, gsize output_capacity, gboolean flush)
{
	GMimeEncoderInterface *iface;
	guint8 *copy = NULL;
	gsize estimate, written;

	g_return_val_if_fail (GMIME_IS_ENCODER (encoder), 0);
	g_return_val_if_fail (input != NULL || input_length == 0, 0);
	g_return_val_if_fail (output != NULL || output_capacity == 0, 0);

	iface = GMIME_ENCODER_GET_IFACE (encoder);
	estimate = iface->estimate_output_length (encoder, input_length);
	g_return_val_if_fail (estimate != G_MAXSIZE, 0);
	g_return_val_if_fail (output_capacity >= estimate, 0);

	if (!GMIME_IS_PASS_THROUGH_ENCODER (encoder) &&
	    codec_buffers_overlap (input, input_length, output, estimate)) {
		copy = g_memdup2 (input, input_length);
		input = copy;
	}

	written = flush ? iface->flush (encoder, input, input_length, output, output_capacity) :
		iface->encode (encoder, input, input_length, output, output_capacity);
	g_free (copy);
	return written;
}

/**
 * g_mime_encoder_encode:
 * @encoder: an encoder
 * @input: (nullable) (array length=input_length): input bytes, or %NULL for empty input
 * @input_length: input length
 * @output: (out caller-allocates) (array length=output_capacity): writable output buffer
 * @output_capacity: capacity, at least the estimated output length
 *
 * Argument violations are programming errors checked before changing state.
 *
 * Returns: the number of bytes written, excluding any terminating NUL.
 **/
gsize
g_mime_encoder_encode (GMimeEncoder *encoder, const guint8 *input, gsize input_length,
		       guint8 *output, gsize output_capacity)
{
	return encoder_process (encoder, input, input_length, output, output_capacity, FALSE);
}

/**
 * g_mime_encoder_flush:
 * @encoder: an encoder
 * @input: (nullable) (array length=input_length): final input bytes
 * @input_length: input length
 * @output: (out caller-allocates) (array length=output_capacity): writable output buffer
 * @output_capacity: capacity, at least the estimated output length
 *
 * Encodes the final input and emits buffered output. Standard MIME codecs
 * reset their encoding state; #GMimeYencEncoder retains its running checksum
 * until g_mime_encoder_reset() is called.
 *
 * Returns: the number of bytes written.
 **/
gsize
g_mime_encoder_flush (GMimeEncoder *encoder, const guint8 *input, gsize input_length,
		      guint8 *output, gsize output_capacity)
{
	return encoder_process (encoder, input, input_length, output, output_capacity, TRUE);
}

/**
 * g_mime_encoder_reset:
 * @encoder: an encoder
 *
 * Discards buffered input without emitting output.
 **/
void
g_mime_encoder_reset (GMimeEncoder *encoder)
{
	g_return_if_fail (GMIME_IS_ENCODER (encoder));
	GMIME_ENCODER_GET_IFACE (encoder)->reset (encoder);
}
