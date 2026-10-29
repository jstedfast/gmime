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

struct _GMimeYencEncoder {
	GObject parent_instance;

	GMimeNewLineFormat newline_format;
	guint max_line_length;
	guint column;

	guint32 checksum;
};

static void yenc_encoder_iface_init (GMimeEncoderInterface *iface);

G_DEFINE_FINAL_TYPE_WITH_CODE (GMimeYencEncoder, g_mime_yenc_encoder, G_TYPE_OBJECT,
	G_IMPLEMENT_INTERFACE (GMIME_TYPE_ENCODER, yenc_encoder_iface_init))

static void
g_mime_yenc_encoder_class_init (GMimeYencEncoderClass *klass)
{
}

static void
g_mime_yenc_encoder_init (GMimeYencEncoder *self)
{
	self->newline_format = GMIME_NEWLINE_FORMAT_UNIX;
	self->max_line_length = 128;
	self->checksum = G_MAXUINT32;
}

static GMimeContentEncoding
yenc_encoder_encoding (GMimeEncoder *encoder)
{
	return GMIME_CONTENT_ENCODING_UNSPECIFIED;
}

static GMimeEncoder *
yenc_encoder_clone (GMimeEncoder *encoder)
{
	GMimeYencEncoder *self = GMIME_YENC_ENCODER (encoder);
	GMimeYencEncoder *copy = g_mime_yenc_encoder_new_with_newline_format (self->newline_format, self->max_line_length);

	copy->column = self->column;
	copy->checksum = self->checksum;

	return GMIME_ENCODER (copy);
}

static gsize
yenc_encoder_estimate (GMimeEncoder *encoder, gsize length)
{
	GMimeYencEncoder *self = GMIME_YENC_ENCODER (encoder);
	gsize estimate, newlines;
	guint newline_length = self->newline_format == GMIME_NEWLINE_FORMAT_DOS ? 2 : 1;

	/* Every byte may need escaping, a previous call may have partly filled the
	 * current line, and flush may add a final newline. */
	if (!g_size_checked_mul (&estimate, length, 2) ||
	    !g_size_checked_add (&newlines, estimate / self->max_line_length, 2) ||
	    !g_size_checked_mul (&newlines, newlines, newline_length) ||
	    !g_size_checked_add (&estimate, estimate, newlines))
		return G_MAXSIZE;

	return estimate;
}

static gsize
yenc_encoder_encode (GMimeEncoder *encoder, const guint8 *input, gsize length,
		  guint8 *output, gsize capacity)
{
	GMimeYencEncoder *self = GMIME_YENC_ENCODER (encoder);
	gsize i, written = 0;
	guint8 c;

	for (i = 0; i < length; i++) {
		c = input[i];
		self->checksum = codec_crc32_update (self->checksum, c);
		c += 42;

		if (c == 0 || c == '\t' || c == '\r' || c == '\n' || c == '=' || c == '.') {
			output[written++] = '=';
			output[written++] = c + 64;
			self->column += 2;
		} else {
			output[written++] = c;
			self->column++;
		}

		if (self->column >= self->max_line_length) {
			if (self->newline_format == GMIME_NEWLINE_FORMAT_DOS)
				output[written++] = '\r';

			output[written++] = '\n';
			self->column = 0;
		}
	}

	return written;
}

static gsize
yenc_encoder_flush (GMimeEncoder *encoder, const guint8 *input, gsize length,
		 guint8 *output, gsize capacity)
{
	GMimeYencEncoder *self = GMIME_YENC_ENCODER (encoder);
	gsize written = yenc_encoder_encode (encoder, input, length, output, capacity);

	if (self->column) {
		if (self->newline_format == GMIME_NEWLINE_FORMAT_DOS)
			output[written++] = '\r';

		output[written++] = '\n';
	}

	self->column = 0;
	return written;
}

static void
yenc_encoder_reset (GMimeEncoder *encoder)
{
	GMimeYencEncoder *self = GMIME_YENC_ENCODER (encoder);

	self->column = 0;
	self->checksum = G_MAXUINT32;
}

static void
yenc_encoder_iface_init (GMimeEncoderInterface *iface)
{
	iface->get_encoding = yenc_encoder_encoding;
	iface->clone = yenc_encoder_clone;
	iface->estimate_output_length = yenc_encoder_estimate;
	iface->encode = yenc_encoder_encode;
	iface->flush = yenc_encoder_flush;
	iface->reset = yenc_encoder_reset;
}

/**
 * g_mime_yenc_encoder_new:
 *
 * Emits payload only, without =ybegin, =ypart, or =yend lines.
 *
 * Returns: (transfer full): a yEnc encoder using 128-character lines.
 **/
GMimeYencEncoder *
g_mime_yenc_encoder_new (void)
{
	return g_object_new (GMIME_TYPE_YENC_ENCODER, NULL);
}

/**
 * g_mime_yenc_encoder_new_with_max_line_length:
 * @max_line_length: a line length between 60 and 998
 *
 * An escape pair is never split, so a line can exceed the limit by one byte.
 *
 * Returns: (transfer full): a yEnc encoder.
 **/
GMimeYencEncoder *
g_mime_yenc_encoder_new_with_max_line_length (guint max_line_length)
{
	return g_mime_yenc_encoder_new_with_newline_format (GMIME_NEWLINE_FORMAT_UNIX, max_line_length);
}

/**
 * g_mime_yenc_encoder_new_with_newline_format:
 * @newline_format: DOS (CRLF) or Unix (LF) line endings
 * @max_line_length: a line length between 60 and 998
 *
 * Creates a payload encoder. An escape pair may extend a line by one byte.
 * The constructors without a newline argument retain their LF default.
 *
 * Returns: (transfer full): a yEnc encoder.
 **/
GMimeYencEncoder *
g_mime_yenc_encoder_new_with_newline_format (GMimeNewLineFormat newline_format, guint max_line_length)
{
	GMimeYencEncoder *encoder;

	g_return_val_if_fail (newline_format == GMIME_NEWLINE_FORMAT_UNIX || newline_format == GMIME_NEWLINE_FORMAT_DOS, NULL);
	g_return_val_if_fail (max_line_length >= 60 && max_line_length <= 998, NULL);

	encoder = g_mime_yenc_encoder_new ();
	encoder->max_line_length = max_line_length;
	encoder->newline_format = newline_format;

	return encoder;
}

/**
 * g_mime_yenc_encoder_get_checksum:
 * @encoder: a yEnc encoder
 *
 * The running CRC-32 starts at 0xffffffff. Complement the returned value
 * to obtain the conventional final CRC-32. Flush retains the checksum;
 * only reset restores the initial value.
 *
 * Returns: the uncomplemented running checksum.
 **/
guint32
g_mime_yenc_encoder_get_checksum (GMimeYencEncoder *encoder)
{
	g_return_val_if_fail (GMIME_IS_YENC_ENCODER (encoder), 0);

	return encoder->checksum;
}
