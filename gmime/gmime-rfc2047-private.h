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

#ifndef __GMIME_RFC2047_PRIVATE_H__
#define __GMIME_RFC2047_PRIVATE_H__

#include <glib-object.h>

G_BEGIN_DECLS

typedef enum {
	GMIME_QENCODE_MODE_PHRASE,
	GMIME_QENCODE_MODE_TEXT
} GMimeQEncodeMode;

#define GMIME_TYPE_RFC2047_ENCODER (_g_mime_rfc2047_encoder_get_type ())
G_GNUC_INTERNAL GType _g_mime_rfc2047_encoder_get_type (void);
G_DECLARE_INTERFACE (GMimeRfc2047Encoder, _g_mime_rfc2047_encoder, GMIME, RFC2047_ENCODER, GObject)

struct _GMimeRfc2047EncoderInterface {
	GTypeInterface parent_iface;
	char (*get_encoding) (GMimeRfc2047Encoder *encoder);
	gsize (*estimate_output_length) (GMimeRfc2047Encoder *encoder, gsize input_length);
	gsize (*encode) (GMimeRfc2047Encoder *encoder, const guint8 *input, gsize length, guint8 *output);
};

/* Stateless encoded-word payloads, without wrapping, framing, or a terminating NUL. */
G_GNUC_INTERNAL char _g_mime_rfc2047_encoder_get_encoding (GMimeRfc2047Encoder *encoder);
G_GNUC_INTERNAL gsize _g_mime_rfc2047_encoder_estimate_output_length (GMimeRfc2047Encoder *encoder,
								 gsize input_length);
G_GNUC_INTERNAL gsize _g_mime_rfc2047_encoder_encode (GMimeRfc2047Encoder *encoder, const guint8 *input,
						  gsize length, guint8 *output, gsize capacity);
G_GNUC_INTERNAL GMimeRfc2047Encoder *_g_mime_rfc2047_base64_encoder_new (void);
G_GNUC_INTERNAL GMimeRfc2047Encoder *_g_mime_rfc2047_quoted_printable_encoder_new (GMimeQEncodeMode mode);

G_END_DECLS
#endif
