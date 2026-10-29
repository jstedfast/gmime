/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*  GMime
 *  Copyright (C) 2000-2022 Jeffrey Stedfast
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


#ifndef __GMIME_FILTER_BASIC_H__
#define __GMIME_FILTER_BASIC_H__

#include <gmime/gmime-codecs.h>
#include <gmime/gmime-filter.h>

G_BEGIN_DECLS

#define GMIME_TYPE_FILTER_BASIC            (g_mime_filter_basic_get_type ())


/**
 * GMimeFilterBasic:
 *
 * A basic encoder/decoder filter for the MIME encodings.
 **/
G_DECLARE_DERIVABLE_TYPE (GMimeFilterBasic, g_mime_filter_basic, GMIME, FILTER_BASIC, GMimeFilter)

struct _GMimeFilterBasicClass {
	GMimeFilterClass parent_class;

	/*< private >*/
	gpointer reserved[8];
};


GMimeFilter *g_mime_filter_basic_new (GMimeContentEncoding encoding, gboolean encode);
GMimeFilter *g_mime_filter_basic_new_with_encoder (GMimeEncoder *encoder);
GMimeFilter *g_mime_filter_basic_new_with_decoder (GMimeDecoder *decoder);

GMimeFilter *g_mime_filter_basic_create_encoder (GMimeContentEncoding encoding);
GMimeFilter *g_mime_filter_basic_create_encoder_with_newline_format (GMimeContentEncoding encoding,
								  GMimeNewLineFormat newline_format,
								  guint max_line_length);
GMimeFilter *g_mime_filter_basic_create_encoder_by_name (const char *name);
GMimeFilter *g_mime_filter_basic_create_encoder_by_name_with_newline_format (const char *name,
									  GMimeNewLineFormat newline_format,
									  guint max_line_length);
GMimeFilter *g_mime_filter_basic_create_decoder (GMimeContentEncoding encoding);
GMimeFilter *g_mime_filter_basic_create_decoder_by_name (const char *name);

G_END_DECLS

#endif /* __GMIME_FILTER_BASIC_H__ */
