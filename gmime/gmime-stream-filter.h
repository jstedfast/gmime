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


#ifndef __GMIME_STREAM_FILTER_H__
#define __GMIME_STREAM_FILTER_H__

#include <gmime/gmime-stream.h>
#include <gmime/gmime-filter.h>

G_BEGIN_DECLS

#define GMIME_TYPE_STREAM_FILTER            (g_mime_stream_filter_get_type ())
#define GMIME_STREAM_FILTER(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), GMIME_TYPE_STREAM_FILTER, GMimeStreamFilter))
#define GMIME_STREAM_FILTER_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), GMIME_TYPE_STREAM_FILTER, GMimeStreamFilterClass))
#define GMIME_IS_STREAM_FILTER(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GMIME_TYPE_STREAM_FILTER))
#define GMIME_IS_STREAM_FILTER_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), GMIME_TYPE_STREAM_FILTER))
#define GMIME_STREAM_FILTER_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), GMIME_TYPE_STREAM_FILTER, GMimeStreamFilterClass))

typedef struct _GMimeStreamFilter GMimeStreamFilter;
typedef struct _GMimeStreamFilterClass GMimeStreamFilterClass;

/**
 * GMimeStreamFilter:
 * @parent_object: parent #GMimeStream
 * @priv: private state data
 * @source: source stream
 *
 * A #GMimeStream which passes data through any #GMimeFilter objects.
 **/
struct _GMimeStreamFilter {
	GMimeStream parent_object;
	
	struct _GMimeStreamFilterPrivate *priv;
	
	GMimeStream *source;
};

struct _GMimeStreamFilterClass {
	GMimeStreamClass parent_class;
	
};


GType g_mime_stream_filter_get_type (void);

GMimeStream *g_mime_stream_filter_new (GMimeStream *stream);

int g_mime_stream_filter_add (GMimeStreamFilter *stream, GMimeFilter *filter);
void g_mime_stream_filter_remove (GMimeStreamFilter *stream, int id);

G_END_DECLS

#endif /* __GMIME_STREAM_FILTER_H__ */
