/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*  GMime
 *  Copyright (C) 1999-2006 Jeffrey Stedfast
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Street #330, Boston, MA 02111-1307, USA.
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

struct _GMimeStreamFilter {
	GMimeStream parent_object;
	
	struct _GMimeStreamFilterPrivate *priv;
	
	GMimeStream *source;
};

struct _GMimeStreamFilterClass {
	GMimeStreamClass parent_class;
	
};


GType g_mime_stream_filter_get_type (void);

GMimeStream *g_mime_stream_filter_new_with_stream (GMimeStream *stream);

int g_mime_stream_filter_add (GMimeStreamFilter *fstream, GMimeFilter *filter);
void g_mime_stream_filter_remove (GMimeStreamFilter *fstream, int id);

G_END_DECLS

#endif /* __GMIME_STREAM_FILTER_H__ */
