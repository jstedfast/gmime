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
#include "gmime-rfc2047-private.h"
#include "gmime-codec-private.h"

G_DEFINE_INTERFACE (GMimeRfc2047Encoder, _g_mime_rfc2047_encoder, G_TYPE_OBJECT)

static void
_g_mime_rfc2047_encoder_default_init (GMimeRfc2047EncoderInterface *iface)
{
}

char
_g_mime_rfc2047_encoder_get_encoding (GMimeRfc2047Encoder *encoder)
{
	g_return_val_if_fail (GMIME_IS_RFC2047_ENCODER (encoder), '\0');

	return GMIME_RFC2047_ENCODER_GET_IFACE (encoder)->get_encoding (encoder);
}

gsize
_g_mime_rfc2047_encoder_estimate_output_length (GMimeRfc2047Encoder *encoder, gsize length)
{
	g_return_val_if_fail (GMIME_IS_RFC2047_ENCODER (encoder), G_MAXSIZE);

	return GMIME_RFC2047_ENCODER_GET_IFACE (encoder)->estimate_output_length (encoder, length);
}

gsize
_g_mime_rfc2047_encoder_encode (GMimeRfc2047Encoder *encoder, const guint8 *input, gsize length,
			       guint8 *output, gsize capacity)
{
	guint8 *copy = NULL;
	gsize estimate, written;

	g_return_val_if_fail (GMIME_IS_RFC2047_ENCODER (encoder), 0);
	g_return_val_if_fail (input != NULL || length == 0, 0);
	g_return_val_if_fail (output != NULL || capacity == 0, 0);

	estimate = _g_mime_rfc2047_encoder_estimate_output_length (encoder, length);
	g_return_val_if_fail (estimate != G_MAXSIZE && capacity >= estimate, 0);

	if (codec_buffers_overlap (input, length, output, capacity))
		input = copy = g_memdup2 (input, length);

	written = GMIME_RFC2047_ENCODER_GET_IFACE (encoder)->encode (encoder, input, length, output);
	g_free (copy);

	return written;
}
