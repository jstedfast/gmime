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

struct _GMimePassThroughDecoder {
	GObject parent_object;
	GMimeContentEncoding encoding;
};

static void pass_through_decoder_iface_init (GMimeDecoderInterface *iface);

G_DEFINE_FINAL_TYPE_WITH_CODE (GMimePassThroughDecoder, g_mime_pass_through_decoder, G_TYPE_OBJECT,
	G_IMPLEMENT_INTERFACE (GMIME_TYPE_DECODER, pass_through_decoder_iface_init))

static void
g_mime_pass_through_decoder_class_init (GMimePassThroughDecoderClass *klass)
{
}

static void
g_mime_pass_through_decoder_init (GMimePassThroughDecoder *self)
{
	self->encoding = GMIME_CONTENT_ENCODING_UNSPECIFIED;
}

static GMimeContentEncoding
pass_through_decoder_encoding (GMimeDecoder *decoder)
{
	return GMIME_PASS_THROUGH_DECODER (decoder)->encoding;
}

static GMimeDecoder *
pass_through_decoder_clone (GMimeDecoder *decoder)
{
	return GMIME_DECODER (g_mime_pass_through_decoder_new (GMIME_PASS_THROUGH_DECODER (decoder)->encoding));
}

static gsize
pass_through_decoder_estimate (GMimeDecoder *decoder, gsize length)
{
	return length;
}

static gsize
pass_through_decoder_decode (GMimeDecoder *decoder, const guint8 *input, gsize length,
			     guint8 *output, gsize capacity)
{
	if (length)
		memmove (output, input, length);

	return length;
}

static void
pass_through_decoder_reset (GMimeDecoder *decoder)
{
	(void) decoder;
}

static void
pass_through_decoder_iface_init (GMimeDecoderInterface *iface)
{
	iface->get_encoding = pass_through_decoder_encoding;
	iface->clone = pass_through_decoder_clone;
	iface->estimate_output_length = pass_through_decoder_estimate;
	iface->decode = pass_through_decoder_decode;
	iface->reset = pass_through_decoder_reset;
}

/**
 * g_mime_pass_through_decoder_new:
 * @encoding: the reported content encoding
 *
 * Returns: (transfer full): a decoder which copies bytes without transformation.
 **/
GMimePassThroughDecoder *
g_mime_pass_through_decoder_new (GMimeContentEncoding encoding)
{
	GMimePassThroughDecoder *decoder;

	g_return_val_if_fail (encoding >= GMIME_CONTENT_ENCODING_UNSPECIFIED && encoding <= GMIME_CONTENT_ENCODING_UUENCODE, NULL);

	decoder = g_object_new (GMIME_TYPE_PASS_THROUGH_DECODER, NULL);
	decoder->encoding = encoding;

	return decoder;
}
