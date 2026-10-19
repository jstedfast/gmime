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
#include "gmime-table-private.h"

typedef struct {
	GObject parent_instance;
	GMimeQEncodeMode mode;
} GMimeRfc2047QuotedPrintableEncoder;

typedef GObjectClass GMimeRfc2047QuotedPrintableEncoderClass;

G_GNUC_INTERNAL GType _g_mime_rfc2047_quoted_printable_encoder_get_type (void);
static void qp_encoder_interface_init (GMimeRfc2047EncoderInterface *iface);

G_DEFINE_TYPE_WITH_CODE (GMimeRfc2047QuotedPrintableEncoder, _g_mime_rfc2047_quoted_printable_encoder, G_TYPE_OBJECT,
			G_IMPLEMENT_INTERFACE (GMIME_TYPE_RFC2047_ENCODER, qp_encoder_interface_init))

static char
qp_get_encoding (GMimeRfc2047Encoder *encoder)
{
	return 'q';
}

static gsize
qp_estimate (GMimeRfc2047Encoder *encoder, gsize length)
{
	gsize estimate;

	return g_size_checked_mul (&estimate, length, 3) ? estimate : G_MAXSIZE;
}

static gsize
qp_encode (GMimeRfc2047Encoder *encoder, const guint8 *input, gsize length, guint8 *output)
{
	GMimeQEncodeMode mode = ((GMimeRfc2047QuotedPrintableEncoder *) encoder)->mode;
	gushort safemask = mode == GMIME_QENCODE_MODE_PHRASE ? IS_PSAFE : IS_ESAFE;
	static const char hex[] = "0123456789ABCDEF";
	gsize i, written = 0;
	guint8 c;

	for (i = 0; i < length; i++) {
		c = input[i];
		if (c == ' ') {
			output[written++] = '_';
		} else if (c != '_' && c != '=' && c != '\\' && (gmime_special_table[c] & safemask)) {
			output[written++] = c;
		} else {
			output[written++] = '=';
			output[written++] = hex[c >> 4];
			output[written++] = hex[c & 15];
		}
	}

	return written;
}

static void
qp_encoder_interface_init (GMimeRfc2047EncoderInterface *iface)
{
	iface->get_encoding = qp_get_encoding;
	iface->estimate_output_length = qp_estimate;
	iface->encode = qp_encode;
}

static void
_g_mime_rfc2047_quoted_printable_encoder_class_init (GMimeRfc2047QuotedPrintableEncoderClass *klass)
{
}

static void
_g_mime_rfc2047_quoted_printable_encoder_init (GMimeRfc2047QuotedPrintableEncoder *encoder)
{
	encoder->mode = GMIME_QENCODE_MODE_PHRASE;
}

GMimeRfc2047Encoder *
_g_mime_rfc2047_quoted_printable_encoder_new (GMimeQEncodeMode mode)
{
	GMimeRfc2047QuotedPrintableEncoder *encoder;

	g_return_val_if_fail (mode == GMIME_QENCODE_MODE_PHRASE || mode == GMIME_QENCODE_MODE_TEXT, NULL);

	encoder = g_object_new (_g_mime_rfc2047_quoted_printable_encoder_get_type (), NULL);
	encoder->mode = mode;

	return GMIME_RFC2047_ENCODER (encoder);
}
