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

typedef enum {
	Y_EXPECT_BEGIN,
	Y_BEGIN_EQUAL,
	Y_BEGIN_EQUAL_Y,
	Y_BEGIN_EQUAL_YB,
	Y_BEGIN_EQUAL_YBE,
	Y_BEGIN_EQUAL_YBEG,
	Y_BEGIN_EQUAL_YBEGI,
	Y_BEGIN_EQUAL_YBEGIN,
	Y_EXPECT_BEGIN_NEWLINE,
	Y_EXPECT_PART_OR_PAYLOAD,
	Y_PART_EQUAL,
	Y_PART_EQUAL_Y,
	Y_PART_EQUAL_YP,
	Y_PART_EQUAL_YPA,
	Y_PART_EQUAL_YPAR,
	Y_PART_EQUAL_YPART,
	Y_EXPECT_PART_NEWLINE,
	Y_PAYLOAD,
	Y_ENDED
} YencDecoderMode;

typedef struct {
	YencDecoderMode mode;
	gboolean payload_only;
	gboolean escaped;
	gboolean eoln;
	guint8 octet;
	guint32 checksum;
} YencDecoderState;

struct _GMimeYencDecoder {
	GObject parent_instance;
	YencDecoderState state;
};

static void yenc_decoder_iface_init (GMimeDecoderInterface *iface);

G_DEFINE_FINAL_TYPE_WITH_CODE (GMimeYencDecoder, g_mime_yenc_decoder, G_TYPE_OBJECT,
	G_IMPLEMENT_INTERFACE (GMIME_TYPE_DECODER, yenc_decoder_iface_init))

static void
yenc_decoder_reset (GMimeDecoder *decoder)
{
	YencDecoderState *state = &GMIME_YENC_DECODER (decoder)->state;

	state->mode = state->payload_only ? Y_PAYLOAD : Y_EXPECT_BEGIN;
	state->escaped = FALSE;
	state->eoln = TRUE;
	state->octet = '\n';
	state->checksum = G_MAXUINT32;
}

static void
g_mime_yenc_decoder_class_init (GMimeYencDecoderClass *klass)
{
}

static void
g_mime_yenc_decoder_init (GMimeYencDecoder *self)
{
	yenc_decoder_reset (GMIME_DECODER (self));
}

static GMimeContentEncoding
yenc_decoder_encoding (GMimeDecoder *decoder)
{
	return GMIME_CONTENT_ENCODING_UNSPECIFIED;
}

static GMimeDecoder *
yenc_decoder_clone (GMimeDecoder *decoder)
{
	GMimeYencDecoder *self = GMIME_YENC_DECODER (decoder);
	GMimeYencDecoder *copy = g_mime_yenc_decoder_new (self->state.payload_only);

	copy->state = self->state;

	return GMIME_DECODER (copy);
}

static gsize
yenc_decoder_estimate (GMimeDecoder *decoder, gsize length)
{
	return length;
}

static gsize
yenc_decoder_scan_begin (YencDecoderState *state, const guint8 *input, gsize length)
{
	static const guint8 begin[] = "=ybegin ";
	static const guint8 part[] = "=ypart ";
	gsize i = 0;
	guint8 c;

	while (i < length && state->mode < Y_PAYLOAD) {
		switch (state->mode) {
		case Y_EXPECT_BEGIN:
			c = input[i++];

			if (state->octet == '\n' && c == '=')
				state->mode = Y_BEGIN_EQUAL;

			state->octet = c;
			break;
		case Y_BEGIN_EQUAL:
		case Y_BEGIN_EQUAL_Y:
		case Y_BEGIN_EQUAL_YB:
		case Y_BEGIN_EQUAL_YBE:
		case Y_BEGIN_EQUAL_YBEG:
		case Y_BEGIN_EQUAL_YBEGI:
		case Y_BEGIN_EQUAL_YBEGIN:
			c = input[i++];
			state->mode = c == begin[state->mode] ? state->mode + 1 : Y_EXPECT_BEGIN;
			state->octet = c;
			break;
		case Y_EXPECT_BEGIN_NEWLINE:
		case Y_EXPECT_PART_NEWLINE:
			state->octet = input[i++];

			if (state->octet == '\n') {
				if (state->mode == Y_EXPECT_BEGIN_NEWLINE) {
					state->mode = Y_EXPECT_PART_OR_PAYLOAD;
				} else {
					state->mode = Y_PAYLOAD;
					state->escaped = FALSE;
					state->eoln = TRUE;
				}
			}
			break;
		case Y_EXPECT_PART_OR_PAYLOAD:
			if (input[i] != '=') {
				state->mode = Y_PAYLOAD;
				state->escaped = FALSE;
				state->eoln = TRUE;
			} else {
				state->mode = Y_PART_EQUAL;
				state->octet = input[i++];
				state->escaped = TRUE;
			}
			break;
		case Y_PART_EQUAL:
			if (input[i] != 'y') {
				state->mode = Y_PAYLOAD;
				state->escaped = FALSE;
				state->eoln = TRUE;
			} else {
				state->mode = Y_PART_EQUAL_Y;
				state->octet = input[i++];
			}
			break;
		case Y_PART_EQUAL_Y:
			if (input[i] == 'e') {
				/* After "=ye" the marker can only be "=yend". */
				state->mode = Y_ENDED;
			} else if (input[i] != 'p') {
				state->mode = Y_EXPECT_BEGIN_NEWLINE;
			} else {
				state->mode = Y_PART_EQUAL_YP;
				state->octet = input[i++];
			}
			break;
		case Y_PART_EQUAL_YP:
		case Y_PART_EQUAL_YPA:
		case Y_PART_EQUAL_YPAR:
		case Y_PART_EQUAL_YPART:
			if (input[i] != part[state->mode - Y_EXPECT_PART_OR_PAYLOAD]) {
				state->mode = Y_EXPECT_BEGIN_NEWLINE;
			} else {
				state->mode++;
				state->octet = input[i++];
			}
			break;
		default:
			g_assert_not_reached ();
		}
	}

	return i;
}

static gsize
yenc_decoder_decode (GMimeDecoder *decoder, const guint8 *input, gsize length,
		  guint8 *output, gsize capacity)
{
	YencDecoderState *state = &GMIME_YENC_DECODER (decoder)->state;
	gsize i = 0, written = 0;
	guint8 c;

	if (state->mode < Y_PAYLOAD)
		i = yenc_decoder_scan_begin (state, input, length);

	if (state->mode == Y_ENDED)
		return 0;

	for (; i < length; i++) {
		c = state->octet = input[i];

		if (c == '\r') {
			state->escaped = FALSE;
			continue;
		}

		if (c == '\n') {
			state->escaped = FALSE;
			state->eoln = TRUE;
			continue;
		}

		if (state->escaped) {
			if (state->eoln && c == 'y') {
				/* An unescaped "=y" at the start of a payload line introduces the end marker. */
				state->mode = Y_ENDED;
				break;
			}

			state->escaped = FALSE;
			state->eoln = FALSE;
			c -= 64;
		} else if (c == '=') {
			state->escaped = TRUE;
			continue;
		} else {
			state->eoln = FALSE;
		}

		c -= 42;
		state->checksum = codec_crc32_update (state->checksum, c);
		output[written++] = c;
	}

	return written;
}

static void
yenc_decoder_iface_init (GMimeDecoderInterface *iface)
{
	iface->get_encoding = yenc_decoder_encoding;
	iface->clone = yenc_decoder_clone;
	iface->estimate_output_length = yenc_decoder_estimate;
	iface->decode = yenc_decoder_decode;
	iface->reset = yenc_decoder_reset;
}

/**
 * g_mime_yenc_decoder_new:
 * @payload_only: whether to decode immediately instead of finding a =ybegin line
 *
 * Full-block decoding skips the =ybegin and optional =ypart header lines.
 * A line beginning with =y ends decoding until reset. Metadata, sizes, and
 * advertised checksums are not validated; callers can compare the computed
 * checksum themselves.
 *
 * Returns: (transfer full): a yEnc decoder.
 **/
GMimeYencDecoder *
g_mime_yenc_decoder_new (gboolean payload_only)
{
	GMimeYencDecoder *decoder = g_object_new (GMIME_TYPE_YENC_DECODER, NULL);

	decoder->state.payload_only = payload_only;
	yenc_decoder_reset (GMIME_DECODER (decoder));

	return decoder;
}

/**
 * g_mime_yenc_decoder_get_checksum:
 * @decoder: a yEnc decoder
 *
 * The running CRC-32 starts at 0xffffffff. Complement the returned value
 * to obtain the conventional final CRC-32. Reset restores the initial value.
 *
 * Returns: the uncomplemented running checksum.
 **/
guint32
g_mime_yenc_decoder_get_checksum (GMimeYencDecoder *decoder)
{
	g_return_val_if_fail (GMIME_IS_YENC_DECODER (decoder), 0);

	return decoder->state.checksum;
}
