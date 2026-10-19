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
#include "gmime-rfc2047-private.h"
#include "gmime-codec-private.h"

typedef struct {
	GObject parent_instance;
} GMimeRfc2047Base64Encoder;

typedef GObjectClass GMimeRfc2047Base64EncoderClass;

G_GNUC_INTERNAL GType _g_mime_rfc2047_base64_encoder_get_type (void);
static void base64_encoder_interface_init (GMimeRfc2047EncoderInterface *iface);

G_DEFINE_TYPE_WITH_CODE (GMimeRfc2047Base64Encoder, _g_mime_rfc2047_base64_encoder, G_TYPE_OBJECT,
			G_IMPLEMENT_INTERFACE (GMIME_TYPE_RFC2047_ENCODER, base64_encoder_interface_init))

static char
base64_get_encoding (GMimeRfc2047Encoder *encoder)
{
	return 'b';
}

static gsize
base64_estimate (GMimeRfc2047Encoder *encoder, gsize length)
{
	gsize estimate, groups = length / 3 + (length % 3 != 0);

	return g_size_checked_mul (&estimate, groups, 4) ? estimate : G_MAXSIZE;
}

static gsize
base64_encode (GMimeRfc2047Encoder *encoder, const guint8 *input, gsize length, guint8 *output)
{
	gsize written = 0, consumed;
	guint8 tail[3] = { 0, 0, 0 };

	while (length >= 16) {
		consumed = _g_mime_codec_encode_blocks (input, length, output + written,
						      MIN (length / 3, (gsize) G_MAXUINT), FALSE);
		if (!consumed)
			break;

		input += consumed;
		length -= consumed;
		written += consumed / 3 * 4;
	}

	while (length >= 3) {
		codec_encode_triplet (input, output + written, FALSE);
		input += 3;
		length -= 3;
		written += 4;
	}

	if (length) {
		memcpy (tail, input, length);
		codec_encode_triplet (tail, output + written, FALSE);
		if (length == 1)
			output[written + 2] = '=';
		output[written + 3] = '=';
		written += 4;
	}

	return written;
}

static void
base64_encoder_interface_init (GMimeRfc2047EncoderInterface *iface)
{
	iface->get_encoding = base64_get_encoding;
	iface->estimate_output_length = base64_estimate;
	iface->encode = base64_encode;
}

static void
_g_mime_rfc2047_base64_encoder_class_init (GMimeRfc2047Base64EncoderClass *klass)
{
}

static void
_g_mime_rfc2047_base64_encoder_init (GMimeRfc2047Base64Encoder *encoder)
{
}

GMimeRfc2047Encoder *
_g_mime_rfc2047_base64_encoder_new (void)
{
	return g_object_new (_g_mime_rfc2047_base64_encoder_get_type (), NULL);
}
