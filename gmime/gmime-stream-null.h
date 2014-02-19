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


#ifndef __GMIME_STREAM_NULL_H__
#define __GMIME_STREAM_NULL_H__

#include <glib.h>
#include <gmime/gmime-stream.h>

G_BEGIN_DECLS

#define GMIME_TYPE_STREAM_NULL            (g_mime_stream_null_get_type ())
#define GMIME_STREAM_NULL(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), GMIME_TYPE_STREAM_NULL, GMimeStreamNull))
#define GMIME_STREAM_NULL_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), GMIME_TYPE_STREAM_NULL, GMimeStreamNullClass))
#define GMIME_IS_STREAM_NULL(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GMIME_TYPE_STREAM_NULL))
#define GMIME_IS_STREAM_NULL_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), GMIME_TYPE_STREAM_NULL))
#define GMIME_STREAM_NULL_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), GMIME_TYPE_STREAM_NULL, GMimeStreamNullClass))

typedef struct _GMimeStreamNull GMimeStreamNull;
typedef struct _GMimeStreamNullClass GMimeStreamNullClass;

/**
 * GMimeStreamNull:
 * @parent_object: parent #GMimeStream
 * @written: number of bytes written to this stream
 * @newlines: the number of newlines written to this stream
 *
 * A #GMimeStream which has no backing store.
 **/
struct _GMimeStreamNull {
	GMimeStream parent_object;
	
	size_t written;
	size_t newlines;
};

struct _GMimeStreamNullClass {
	GMimeStreamClass parent_class;
	
};


GType g_mime_stream_null_get_type (void);

GMimeStream *g_mime_stream_null_new (void);

G_END_DECLS

#endif /* __GMIME_STREAM_NULL_H__ */
