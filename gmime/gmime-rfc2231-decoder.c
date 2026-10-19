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

#include "gmime-codec-private.h"

enum {
	RFC2231_PASS_THROUGH,
	RFC2231_PERCENT,
	RFC2231_DECODE_BYTE
};

typedef struct {
	GObject parent_instance;
	guint state;
	guint8 saved;
} GMimeRfc2231Decoder;

typedef GObjectClass GMimeRfc2231DecoderClass;

static void rfc2231_decoder_iface_init (GMimeDecoderInterface *iface);

G_DEFINE_TYPE_WITH_CODE (GMimeRfc2231Decoder, _g_mime_rfc2231_decoder, G_TYPE_OBJECT,
			G_IMPLEMENT_INTERFACE (GMIME_TYPE_DECODER, rfc2231_decoder_iface_init))

static void
_g_mime_rfc2231_decoder_class_init (GMimeRfc2231DecoderClass *klass)
{
}

static void
_g_mime_rfc2231_decoder_init (GMimeRfc2231Decoder *decoder)
{
}

static GMimeContentEncoding
rfc2231_decoder_get_encoding (GMimeDecoder *decoder)
{
	return GMIME_CONTENT_ENCODING_UNSPECIFIED;
}

static GMimeDecoder *
rfc2231_decoder_clone (GMimeDecoder *decoder)
{
	GMimeRfc2231Decoder *source = (GMimeRfc2231Decoder *) decoder;
	GMimeRfc2231Decoder *copy = (GMimeRfc2231Decoder *) _g_mime_rfc2231_decoder_new ();

	copy->state = source->state;
	copy->saved = source->saved;

	return GMIME_DECODER (copy);
}

static gsize
rfc2231_decoder_estimate (GMimeDecoder *decoder, gsize length)
{
	GMimeRfc2231Decoder *rfc2231 = (GMimeRfc2231Decoder *) decoder;

	return g_size_checked_add (&length, length, rfc2231->state) ? length : G_MAXSIZE;
}

static gsize
rfc2231_decoder_decode (GMimeDecoder *decoder, const guint8 *input, gsize length,
		    guint8 *output, gsize capacity)
{
	GMimeRfc2231Decoder *rfc2231 = (GMimeRfc2231Decoder *) decoder;
	gsize written = 0, i;
	guint8 c;
	int hi, lo;

	for (i = 0; i < length; i++) {
		c = input[i];
		switch (rfc2231->state) {
		case RFC2231_PASS_THROUGH:
			if (c == '%')
				rfc2231->state = RFC2231_PERCENT;
			else
				output[written++] = c;
			break;
		case RFC2231_PERCENT:
			rfc2231->saved = c;
			rfc2231->state = RFC2231_DECODE_BYTE;
			break;
		case RFC2231_DECODE_BYTE:
			hi = g_ascii_xdigit_value (rfc2231->saved);
			lo = g_ascii_xdigit_value (c);

			if (hi >= 0 && lo >= 0) {
				output[written++] = (hi << 4) | lo;
			} else {
				/* Preserve invalid percent-encoded triplets literally. */
				output[written++] = '%';
				output[written++] = rfc2231->saved;
				output[written++] = c;
			}

			rfc2231->state = RFC2231_PASS_THROUGH;
			break;
		}
	}

	return written;
}

static void
rfc2231_decoder_reset (GMimeDecoder *decoder)
{
	GMimeRfc2231Decoder *rfc2231 = (GMimeRfc2231Decoder *) decoder;

	rfc2231->state = RFC2231_PASS_THROUGH;
	rfc2231->saved = 0;
}

static void
rfc2231_decoder_iface_init (GMimeDecoderInterface *iface)
{
	iface->get_encoding = rfc2231_decoder_get_encoding;
	iface->clone = rfc2231_decoder_clone;
	iface->estimate_output_length = rfc2231_decoder_estimate;
	iface->decode = rfc2231_decoder_decode;
	iface->reset = rfc2231_decoder_reset;
}

GMimeDecoder *
_g_mime_rfc2231_decoder_new (void)
{
	return g_object_new (_g_mime_rfc2231_decoder_get_type (), NULL);
}
