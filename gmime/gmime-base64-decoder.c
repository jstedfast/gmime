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

static unsigned char base64_rank[256] = {
	255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
	255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
	255,255,255,255,255,255,255,255,255,255,255, 62,255,255,255, 63,
	 52, 53, 54, 55, 56, 57, 58, 59, 60, 61,255,255,255,  0,255,255,
	255,  0,  1,  2,  3,  4,  5,  6,  7,  8,  9, 10, 11, 12, 13, 14,
	 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25,255,255,255,255,255,
	255, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40,
	 41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51,255,255,255,255,255,
	255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
	255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
	255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
	255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
	255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
	255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
	255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
	255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
};

typedef struct {
	guint nsaved;
	guint32 saved;
	guint32 previous;
} Base64DecoderState;

struct _GMimeBase64Decoder {
	GObject parent_instance;
	Base64DecoderState state;
};

static void base64_decoder_iface_init (GMimeDecoderInterface *iface);

G_DEFINE_FINAL_TYPE_WITH_CODE (GMimeBase64Decoder, g_mime_base64_decoder, G_TYPE_OBJECT,
	G_IMPLEMENT_INTERFACE (GMIME_TYPE_DECODER, base64_decoder_iface_init))

static void
base64_decoder_reset (GMimeDecoder *decoder)
{
	Base64DecoderState *state = &GMIME_BASE64_DECODER (decoder)->state;

	state->nsaved = state->saved = state->previous = 0;
}

static void
g_mime_base64_decoder_class_init (GMimeBase64DecoderClass *klass)
{
}

static void
g_mime_base64_decoder_init (GMimeBase64Decoder *self)
{
	base64_decoder_reset (GMIME_DECODER (self));
}

static GMimeContentEncoding
base64_decoder_encoding (GMimeDecoder *decoder)
{
	return GMIME_CONTENT_ENCODING_BASE64;
}

static GMimeDecoder *
base64_decoder_clone (GMimeDecoder *decoder)
{
	GMimeBase64Decoder *copy = g_mime_base64_decoder_new ();

	copy->state = GMIME_BASE64_DECODER (decoder)->state;
	return GMIME_DECODER (copy);
}

static gsize
base64_decoder_estimate (GMimeDecoder *decoder, gsize length)
{
	return (length / 4) * 3 + 3;
}

static gsize
base64_decoder_decode (GMimeDecoder *decoder, const guint8 *input, gsize length,
		       guint8 *output, gsize capacity)
{
	Base64DecoderState *state = &GMIME_BASE64_DECODER (decoder)->state;
	const guint8 *start;
	gsize written = 0;
	guint8 c;
	gint rank;

	while (length) {
		if (!state->nsaved && length >= 16) {
			start = input;
			written += _g_mime_codec_decode_base64_blocks (&input, length, output + written);
			length -= input - start;
			/* Complete quartets replace previous before it is inspected again. */
			if (!length)
				break;
		}

		c = *input++;
		length--;

		if ((rank = base64_rank[c]) == 0xFF)
			continue;

		state->previous = (state->previous << 8) | c;
		state->saved = (state->saved << 6) | rank;

		if (++state->nsaved == 4) {
			if ((state->previous & 0xff0000) != ('=' << 16)) {
				output[written++] = state->saved >> 16;

				if ((state->previous & 0xff00) != ('=' << 8)) {
					output[written++] = state->saved >> 8;

					if ((state->previous & 0xff) != '=')
						output[written++] = state->saved;
				}
			}

			state->saved = state->nsaved = 0;
		}
	}

	return written;
}

static void
base64_decoder_iface_init (GMimeDecoderInterface *iface)
{
	iface->get_encoding = base64_decoder_encoding;
	iface->clone = base64_decoder_clone;
	iface->estimate_output_length = base64_decoder_estimate;
	iface->decode = base64_decoder_decode;
	iface->reset = base64_decoder_reset;
}

/**
 * g_mime_base64_decoder_new:
 *
 * Returns: (transfer full): a Base64 decoder.
 **/
GMimeBase64Decoder *
g_mime_base64_decoder_new (void)
{
	return g_object_new (GMIME_TYPE_BASE64_DECODER, NULL);
}
