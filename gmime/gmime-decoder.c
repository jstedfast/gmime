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
 * SECTION: gmime-decoder
 * @title: GMimeDecoder
 * @short_description: Incremental MIME decoder interface
 *
 * Decoders retain incomplete sequences between calls. Malformed encoded input
 * follows MimeKit's tolerant recovery rather than reporting argument errors.
 * Overlapping buffers are supported, copying the input when necessary.
 **/

G_DEFINE_INTERFACE (GMimeDecoder, g_mime_decoder, G_TYPE_OBJECT)

static void
g_mime_decoder_default_init (GMimeDecoderInterface *iface)
{
}

/**
 * g_mime_decoder_get_encoding:
 * @decoder: a decoder
 *
 * Returns: the content encoding.
 **/
GMimeContentEncoding
g_mime_decoder_get_encoding (GMimeDecoder *decoder)
{
	g_return_val_if_fail (GMIME_IS_DECODER (decoder), GMIME_CONTENT_ENCODING_UNSPECIFIED);
	return GMIME_DECODER_GET_IFACE (decoder)->get_encoding (decoder);
}

/**
 * g_mime_decoder_clone:
 * @decoder: a decoder
 *
 * Returns: (transfer full): an independent decoder with identical state.
 **/
GMimeDecoder *
g_mime_decoder_clone (GMimeDecoder *decoder)
{
	g_return_val_if_fail (GMIME_IS_DECODER (decoder), NULL);
	return GMIME_DECODER_GET_IFACE (decoder)->clone (decoder);
}

/**
 * g_mime_decoder_estimate_output_length:
 * @decoder: a decoder
 * @input_length: number of input bytes
 *
 * Includes buffered input without changing the state.
 *
 * Returns: the required capacity, or %G_MAXSIZE if it is not representable.
 **/
gsize
g_mime_decoder_estimate_output_length (GMimeDecoder *decoder, gsize input_length)
{
	g_return_val_if_fail (GMIME_IS_DECODER (decoder), 0);
	return GMIME_DECODER_GET_IFACE (decoder)->estimate_output_length (decoder, input_length);
}

/**
 * g_mime_decoder_decode:
 * @decoder: a decoder
 * @input: (nullable) (array length=input_length): input bytes
 * @input_length: input length
 * @output: (out caller-allocates) (array length=output_capacity): writable output buffer
 * @output_capacity: capacity, at least the estimated output length
 *
 * Argument violations are programming errors checked before changing state.
 * This does not finalize incomplete sequences.
 *
 * Returns: the number of bytes written.
 **/
gsize
g_mime_decoder_decode (GMimeDecoder *decoder, const guint8 *input, gsize input_length,
		       guint8 *output, gsize output_capacity)
{
	GMimeDecoderInterface *iface;
	guint8 *copy = NULL;
	gsize estimate, written;

	g_return_val_if_fail (GMIME_IS_DECODER (decoder), 0);
	g_return_val_if_fail (input != NULL || input_length == 0, 0);
	g_return_val_if_fail (output != NULL || output_capacity == 0, 0);

	iface = GMIME_DECODER_GET_IFACE (decoder);
	estimate = iface->estimate_output_length (decoder, input_length);
	g_return_val_if_fail (estimate != G_MAXSIZE, 0);
	g_return_val_if_fail (output_capacity >= estimate, 0);

	if (!GMIME_IS_PASS_THROUGH_DECODER (decoder) &&
	    codec_buffers_overlap (input, input_length, output, estimate)) {
		copy = g_memdup2 (input, input_length);
		input = copy;
	}

	written = iface->decode (decoder, input, input_length, output, output_capacity);
	g_free (copy);
	return written;
}

/**
 * g_mime_decoder_reset:
 * @decoder: a decoder
 *
 * Discards incomplete sequences and resets the decoder.
 **/
void
g_mime_decoder_reset (GMimeDecoder *decoder)
{
	g_return_if_fail (GMIME_IS_DECODER (decoder));
	GMIME_DECODER_GET_IFACE (decoder)->reset (decoder);
}
