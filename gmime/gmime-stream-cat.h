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


#ifndef __GMIME_STREAM_CAT_H__
#define __GMIME_STREAM_CAT_H__

#include <glib.h>
#include <gmime/gmime-stream.h>

G_BEGIN_DECLS

#define GMIME_TYPE_STREAM_CAT            (g_mime_stream_cat_get_type ())


/**
 * GMimeStreamCat:
 *
 * A concatenation of other #GMimeStream objects.
 **/
G_DECLARE_DERIVABLE_TYPE (GMimeStreamCat, g_mime_stream_cat, GMIME, STREAM_CAT, GMimeStream)

struct _GMimeStreamCatClass {
	GMimeStreamClass parent_class;

	/*< private >*/
	gpointer reserved[8];
};


GMimeStream *g_mime_stream_cat_new (void);

int g_mime_stream_cat_add_source (GMimeStreamCat *cat, GMimeStream *source);

G_END_DECLS

#endif /* __GMIME_STREAM_CAT_H__ */
