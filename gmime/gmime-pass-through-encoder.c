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

struct _GMimePassThroughEncoder {
	GObject parent_instance;
	GMimeContentEncoding encoding;
};

static void pass_through_encoder_iface_init (GMimeEncoderInterface *iface);

G_DEFINE_FINAL_TYPE_WITH_CODE (GMimePassThroughEncoder, g_mime_pass_through_encoder, G_TYPE_OBJECT,
	G_IMPLEMENT_INTERFACE (GMIME_TYPE_ENCODER, pass_through_encoder_iface_init))

static void
g_mime_pass_through_encoder_class_init (GMimePassThroughEncoderClass *klass)
{
}

static void
g_mime_pass_through_encoder_init (GMimePassThroughEncoder *self)
{
	self->encoding = GMIME_CONTENT_ENCODING_UNSPECIFIED;
}

static GMimeContentEncoding
pass_through_encoder_encoding (GMimeEncoder *encoder)
{
	return GMIME_PASS_THROUGH_ENCODER (encoder)->encoding;
}

static GMimeEncoder *
pass_through_encoder_clone (GMimeEncoder *encoder)
{
	return GMIME_ENCODER (g_mime_pass_through_encoder_new (GMIME_PASS_THROUGH_ENCODER (encoder)->encoding));
}

static gsize
pass_through_encoder_estimate (GMimeEncoder *encoder, gsize length)
{
	return length;
}

static gsize
pass_through_encoder_encode (GMimeEncoder *encoder, const guint8 *input, gsize length,
			     guint8 *output, gsize capacity)
{
	if (length)
		memmove (output, input, length);

	return length;
}

static void
pass_through_encoder_reset (GMimeEncoder *encoder)
{
}

static void
pass_through_encoder_iface_init (GMimeEncoderInterface *iface)
{
	iface->get_encoding = pass_through_encoder_encoding;
	iface->clone = pass_through_encoder_clone;
	iface->estimate_output_length = pass_through_encoder_estimate;
	iface->encode = pass_through_encoder_encode;
	iface->flush = pass_through_encoder_encode;
	iface->reset = pass_through_encoder_reset;
}

/**
 * g_mime_pass_through_encoder_new:
 * @encoding: the reported content encoding
 *
 * Returns: (transfer full): an encoder which copies bytes without transformation.
 **/
GMimePassThroughEncoder *
g_mime_pass_through_encoder_new (GMimeContentEncoding encoding)
{
	GMimePassThroughEncoder *encoder;

	g_return_val_if_fail (encoding >= GMIME_CONTENT_ENCODING_UNSPECIFIED && encoding <= GMIME_CONTENT_ENCODING_UUENCODE, NULL);

	encoder = g_object_new (GMIME_TYPE_PASS_THROUGH_ENCODER, NULL);
	encoder->encoding = encoding;
	return encoder;
}
