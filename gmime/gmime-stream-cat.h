/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*  GMime
 *  Copyright (C) 2000-2014 Jeffrey Stedfast
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
#define GMIME_STREAM_CAT(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), GMIME_TYPE_STREAM_CAT, GMimeStreamCat))
#define GMIME_STREAM_CAT_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), GMIME_TYPE_STREAM_CAT, GMimeStreamCatClass))
#define GMIME_IS_STREAM_CAT(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GMIME_TYPE_STREAM_CAT))
#define GMIME_IS_STREAM_CAT_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), GMIME_TYPE_STREAM_CAT))
#define GMIME_STREAM_CAT_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), GMIME_TYPE_STREAM_CAT, GMimeStreamCatClass))

typedef struct _GMimeStreamCat GMimeStreamCat;
typedef struct _GMimeStreamCatClass GMimeStreamCatClass;

/**
 * GMimeStreamCat:
 * @parent_object: parent #GMimeStream
 * @sources: list of sources
 * @current: current source
 *
 * A concatenation of other #GMimeStream objects.
 **/
struct _GMimeStreamCat {
	GMimeStream parent_object;
	
	struct _cat_node *sources;
	struct _cat_node *current;
};

struct _GMimeStreamCatClass {
	GMimeStreamClass parent_class;
	
};


GType g_mime_stream_cat_get_type (void);

GMimeStream *g_mime_stream_cat_new (void);

int g_mime_stream_cat_add_source (GMimeStreamCat *cat, GMimeStream *source);

G_END_DECLS

#endif /* __GMIME_STREAM_CAT_H__ */
