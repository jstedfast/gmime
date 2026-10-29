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


#ifndef __GMIME_FILTER_BEST_H__
#define __GMIME_FILTER_BEST_H__

#include <gmime/gmime-encodings.h>
#include <gmime/gmime-filter.h>
#include <gmime/gmime-charset.h>
#include <gmime/gmime-utils.h>

G_BEGIN_DECLS

#define GMIME_TYPE_FILTER_BEST            (g_mime_filter_best_get_type ())


/**
 * GMimeFilterBestFlags:
 * @GMIME_FILTER_BEST_CHARSET: Enable best-charset detection.
 * @GMIME_FILTER_BEST_ENCODING: Enable best-encoding detection.
 *
 * Bit flags to enable charset and/or encoding scanning to make
 * educated guesses as to what the best charset and/or encodings to
 * use for the content passed through the filter.
 **/
typedef enum {
	GMIME_FILTER_BEST_CHARSET  = (1 << 0),
	GMIME_FILTER_BEST_ENCODING = (1 << 1)
} GMimeFilterBestFlags;


/**
 * GMimeFilterBest:
 *
 * A filter for calculating the best encoding and/or charset to encode
 * the data passed through it.
 **/
G_DECLARE_DERIVABLE_TYPE (GMimeFilterBest, g_mime_filter_best, GMIME, FILTER_BEST, GMimeFilter)

struct _GMimeFilterBestClass {
	GMimeFilterClass parent_class;

	/*< private >*/
	gpointer reserved[8];
};


GMimeFilter *g_mime_filter_best_new (GMimeFilterBestFlags flags);

const char *g_mime_filter_best_charset (GMimeFilterBest *best);

GMimeContentEncoding g_mime_filter_best_encoding (GMimeFilterBest *best, GMimeEncodingConstraint constraint);
GMimeContentEncoding g_mime_filter_best_encoding_with_max_line_length (GMimeFilterBest *best,
								    GMimeEncodingConstraint constraint,
								    guint max_line_length);

G_END_DECLS

#endif /* __GMIME_FILTER_BEST_H__ */
