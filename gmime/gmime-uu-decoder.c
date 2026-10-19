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

enum {
	UU_EXPECT_BEGIN,
	UU_B,
	UU_BE,
	UU_BEG,
	UU_BEGI,
	UU_BEGIN,
	UU_EXPECT_PAYLOAD,
	UU_PAYLOAD,
	UU_ENDED
};

typedef struct {
	gboolean payload_only;
	guint state;
	guint nsaved;
	guint uulen;
	guint32 saved;
	gboolean eol;
} UUDecoderState;

struct _GMimeUUDecoder {
	GObject parent_object;
	UUDecoderState state;
};

static void uu_decoder_iface_init (GMimeDecoderInterface *iface);

G_DEFINE_FINAL_TYPE_WITH_CODE (GMimeUUDecoder, g_mime_uu_decoder, G_TYPE_OBJECT,
	G_IMPLEMENT_INTERFACE (GMIME_TYPE_DECODER, uu_decoder_iface_init))

static void
uu_decoder_reset (GMimeDecoder *decoder)
{
	UUDecoderState *state = &GMIME_UU_DECODER (decoder)->state;

	state->state = state->payload_only ? UU_PAYLOAD : UU_EXPECT_BEGIN;
	/* While scanning for begin, nsaved is the previous character, not a sextet count.
	 * Start at a line boundary so a begin marker at offset zero is recognized. */
	state->nsaved = state->payload_only ? 0 : '\n';
	state->uulen = state->saved = 0;
	state->eol = FALSE;
}

static void
g_mime_uu_decoder_class_init (GMimeUUDecoderClass *klass)
{
}

static void
g_mime_uu_decoder_init (GMimeUUDecoder *self)
{
	uu_decoder_reset (GMIME_DECODER (self));
}

static GMimeContentEncoding
uu_decoder_encoding (GMimeDecoder *decoder)
{
	return GMIME_CONTENT_ENCODING_UUENCODE;
}

static GMimeDecoder *
uu_decoder_clone (GMimeDecoder *decoder)
{
	GMimeUUDecoder *self = GMIME_UU_DECODER (decoder);
	GMimeUUDecoder *copy = g_mime_uu_decoder_new (self->state.payload_only);

	copy->state = self->state;

	return GMIME_DECODER (copy);
}

static gsize
uu_decoder_estimate (GMimeDecoder *decoder, gsize length)
{
	/* Saved input from the previous decode step can contribute another three bytes. */
	return g_size_checked_add (&length, length, 3) ? length : G_MAXSIZE;
}

static gsize
uu_scan_begin (UUDecoderState *state, const guint8 *input, gsize length)
{
	static const guint8 marker[] = "begin ";
	gsize i = 0;
	guint8 c;

	while (i < length && state->state < UU_PAYLOAD) {
		if (state->state == UU_EXPECT_PAYLOAD) {
			if (input[i++] == '\n') {
				state->state = UU_PAYLOAD;
				state->nsaved = 0;
			}
		} else if (state->state == UU_EXPECT_BEGIN) {
			c = input[i++];
			if (state->nsaved == '\n' && c == 'b')
				state->state = UU_B;
			state->nsaved = c;
		} else {
			c = input[i++];
			if (c == marker[state->state])
				state->state++;
			else
				state->state = UU_EXPECT_BEGIN;
			state->nsaved = c;
		}
	}

	return i;
}

static gsize
uu_decoder_decode (GMimeDecoder *decoder, const guint8 *input, gsize length,
		   guint8 *output, gsize capacity)
{
	UUDecoderState *state = &GMIME_UU_DECODER (decoder)->state;
	gsize i = 0, written = 0;
	guint count, a, b, c, d;
	guint8 ch;

	if (state->state == UU_ENDED)
		return 0;

	if (state->state < UU_PAYLOAD)
		i = uu_scan_begin (state, input, length);

	while (i < length) {
		/* Only decode full quartets within the declared line length. The scalar
		 * path handles partial sextets, short final output and line endings. */
		if (!state->nsaved && state->uulen >= 12 && !state->eol && length - i >= 16 &&
		    _g_mime_codec_decode_block (input + i, output + written, TRUE)) {
			i += 16;
			written += 12;
			state->uulen -= 12;
			continue;
		}

		ch = input[i++];

		if (ch == '\r')
			continue;

		if (ch == '\n') {
			state->eol = TRUE;
			continue;
		}

		if (!state->uulen || state->eol) {
			state->uulen = (ch - 32) & 63;
			state->eol = FALSE;

			if (!state->uulen) {
				state->state = UU_ENDED;
				break;
			}

			continue;
		}

		state->saved = (state->saved << 8) | ch;

		if (++state->nsaved == 4) {
			a = ((state->saved >> 24) - 32) & 63;
			b = ((state->saved >> 16) - 32) & 63;
			c = ((state->saved >> 8) - 32) & 63;
			d = (state->saved - 32) & 63;
			count = MIN (state->uulen, 3);
			output[written++] = (a << 2) | (b >> 4);

			if (count > 1)
				output[written++] = (b << 4) | (c >> 2);

			if (count > 2)
				output[written++] = (c << 6) | d;

			state->uulen -= count;
			state->saved = state->nsaved = 0;
		}
	}

	return written;
}

static void
uu_decoder_iface_init (GMimeDecoderInterface *iface)
{
	iface->get_encoding = uu_decoder_encoding;
	iface->clone = uu_decoder_clone;
	iface->estimate_output_length = uu_decoder_estimate;
	iface->decode = uu_decoder_decode;
	iface->reset = uu_decoder_reset;
}

/**
 * g_mime_uu_decoder_new:
 * @payload_only: whether decoding begins immediately instead of finding a begin-line
 *
 * A zero-length payload line ends decoding. Reset restores the initial mode.
 *
 * Returns: (transfer full): a uudecoder.
 **/
GMimeUUDecoder *
g_mime_uu_decoder_new (gboolean payload_only)
{
	GMimeUUDecoder *decoder = g_object_new (GMIME_TYPE_UU_DECODER, NULL);

	decoder->state.payload_only = payload_only;
	uu_decoder_reset (GMIME_DECODER (decoder));

	return decoder;
}
