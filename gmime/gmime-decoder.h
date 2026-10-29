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

#ifndef __GMIME_DECODER_H__
#define __GMIME_DECODER_H__

#include <glib-object.h>
#include <gmime/gmime-encodings.h>

G_BEGIN_DECLS

#define GMIME_TYPE_DECODER (g_mime_decoder_get_type ())
G_DECLARE_INTERFACE (GMimeDecoder, g_mime_decoder, GMIME, DECODER, GObject)

/**
 * GMimeDecoderInterface:
 * @parent_iface: parent interface
 * @get_encoding: gets the content encoding
 * @clone: copies the current state
 * @estimate_output_length: estimates the output capacity
 * @decode: incrementally decodes input
 * @reset: resets the state
 *
 * Interface for incremental MIME decoders. Incomplete input is retained
 * until a subsequent decode or reset; decoders have no flush operation.
 **/
struct _GMimeDecoderInterface {
	GTypeInterface parent_iface;
	GMimeContentEncoding (*get_encoding) (GMimeDecoder *decoder);
	GMimeDecoder *(*clone) (GMimeDecoder *decoder);
	gsize (*estimate_output_length) (GMimeDecoder *decoder, gsize input_length);
	gsize (*decode) (GMimeDecoder *decoder, const guint8 *input, gsize input_length,
			 guint8 *output, gsize output_capacity);
	void (*reset) (GMimeDecoder *decoder);

	/*< private >*/
	gpointer reserved[8];
};

GMimeContentEncoding g_mime_decoder_get_encoding (GMimeDecoder *decoder);
GMimeDecoder *g_mime_decoder_clone (GMimeDecoder *decoder);
gsize g_mime_decoder_estimate_output_length (GMimeDecoder *decoder, gsize input_length);
gsize g_mime_decoder_decode (GMimeDecoder *decoder, const guint8 *input, gsize input_length,
			    guint8 *output, gsize output_capacity);
void g_mime_decoder_reset (GMimeDecoder *decoder);

G_END_DECLS
#endif
