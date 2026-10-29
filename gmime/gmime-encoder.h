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

#ifndef __GMIME_ENCODER_H__
#define __GMIME_ENCODER_H__

#include <glib-object.h>
#include <gmime/gmime-encodings.h>

G_BEGIN_DECLS

#define GMIME_TYPE_ENCODER (g_mime_encoder_get_type ())
G_DECLARE_INTERFACE (GMimeEncoder, g_mime_encoder, GMIME, ENCODER, GObject)

/**
 * GMimeEncoderInterface:
 * @parent_iface: parent interface
 * @get_encoding: gets the content encoding
 * @clone: copies the current state
 * @estimate_output_length: estimates the output capacity
 * @encode: incrementally encodes input
 * @flush: encodes input and finishes the content
 * @reset: resets the state
 *
 * Interface for incremental MIME encoders.
 **/
struct _GMimeEncoderInterface {
	GTypeInterface parent_iface;
	GMimeContentEncoding (*get_encoding) (GMimeEncoder *encoder);
	GMimeEncoder *(*clone) (GMimeEncoder *encoder);
	gsize (*estimate_output_length) (GMimeEncoder *encoder, gsize input_length);
	gsize (*encode) (GMimeEncoder *encoder, const guint8 *input, gsize input_length,
			 guint8 *output, gsize output_capacity);
	gsize (*flush) (GMimeEncoder *encoder, const guint8 *input, gsize input_length,
			guint8 *output, gsize output_capacity);
	void (*reset) (GMimeEncoder *encoder);

	/*< private >*/
	gpointer reserved[8];
};

GMimeContentEncoding g_mime_encoder_get_encoding (GMimeEncoder *encoder);
GMimeEncoder *g_mime_encoder_clone (GMimeEncoder *encoder);
gsize g_mime_encoder_estimate_output_length (GMimeEncoder *encoder, gsize input_length);
gsize g_mime_encoder_encode (GMimeEncoder *encoder, const guint8 *input, gsize input_length,
			    guint8 *output, gsize output_capacity);
gsize g_mime_encoder_flush (GMimeEncoder *encoder, const guint8 *input, gsize input_length,
			   guint8 *output, gsize output_capacity);
void g_mime_encoder_reset (GMimeEncoder *encoder);

G_END_DECLS
#endif
