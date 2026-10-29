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


#ifndef __GMIME_FILTER_FROM_H__
#define __GMIME_FILTER_FROM_H__

#include <gmime/gmime-filter.h>

G_BEGIN_DECLS

#define GMIME_TYPE_FILTER_FROM            (g_mime_filter_from_get_type ())



/**
 * GMimeFilterFromMode:
 * @GMIME_FILTER_FROM_MODE_DEFAULT: Default mode.
 * @GMIME_FILTER_FROM_MODE_ESCAPE: Escape 'From ' lines with a '>'
 * @GMIME_FILTER_FROM_MODE_ARMOR: QP-Encode 'From ' lines
 *
 * The mode for a #GMimeFilterFrom filter.
 **/
typedef enum {
	GMIME_FILTER_FROM_MODE_DEFAULT  = 0,
	GMIME_FILTER_FROM_MODE_ESCAPE   = 0,
	GMIME_FILTER_FROM_MODE_ARMOR    = 1
} GMimeFilterFromMode;


/**
 * GMimeFilterFrom:
 *
 * A filter for armoring or escaping exact "From " markers at the beginning
 * of input or after LF. Incomplete prefixes span calls and are emitted
 * unchanged on completion. Bare CR does not begin a new line.
 **/
G_DECLARE_DERIVABLE_TYPE (GMimeFilterFrom, g_mime_filter_from, GMIME, FILTER_FROM, GMimeFilter)

struct _GMimeFilterFromClass {
	GMimeFilterClass parent_class;

	/*< private >*/
	gpointer reserved[8];
};


GMimeFilter *g_mime_filter_from_new (GMimeFilterFromMode mode);

G_END_DECLS

#endif /* __GMIME_FILTER_FROM_H__ */
