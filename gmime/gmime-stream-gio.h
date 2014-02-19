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


#ifndef __GMIME_STREAM_GIO_H__
#define __GMIME_STREAM_GIO_H__

#include <gio/gio.h>

#include <gmime/gmime-stream.h>

G_BEGIN_DECLS

#define GMIME_TYPE_STREAM_GIO            (g_mime_stream_gio_get_type ())
#define GMIME_STREAM_GIO(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), GMIME_TYPE_STREAM_GIO, GMimeStreamGIO))
#define GMIME_STREAM_GIO_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), GMIME_TYPE_STREAM_GIO, GMimeStreamGIOClass))
#define GMIME_IS_STREAM_GIO(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GMIME_TYPE_STREAM_GIO))
#define GMIME_IS_STREAM_GIO_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), GMIME_TYPE_STREAM_GIO))
#define GMIME_STREAM_GIO_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), GMIME_TYPE_STREAM_GIO, GMimeStreamGIOClass))

typedef struct _GMimeStreamGIO GMimeStreamGIO;
typedef struct _GMimeStreamGIOClass GMimeStreamGIOClass;

/**
 * GMimeStreamGIO:
 * @parent_object: parent #GMimeStream
 * @ostream: a #GOutputStream
 * @istream: a #GInputStream
 * @file: a #GFile
 * @owner: %TRUE if this stream owns the #GFile or %FALSE otherwise
 * @eos: %TRUE if the end of the stream has been reached or %FALSE otherwise
 *
 * A #GMimeStream wrapper around GLib's GIO streams.
 **/
struct _GMimeStreamGIO {
	GMimeStream parent_object;
	
	GOutputStream *ostream;
	GInputStream *istream;
	GFile *file;
	
	gboolean owner;
	gboolean eos;
};

struct _GMimeStreamGIOClass {
	GMimeStreamClass parent_class;
	
};


GType g_mime_stream_gio_get_type (void);

GMimeStream *g_mime_stream_gio_new (GFile *file);
GMimeStream *g_mime_stream_gio_new_with_bounds (GFile *file, gint64 start, gint64 end);

gboolean g_mime_stream_gio_get_owner (GMimeStreamGIO *stream);
void g_mime_stream_gio_set_owner (GMimeStreamGIO *stream, gboolean owner);

G_END_DECLS

#endif /* __GMIME_STREAM_GIO_H__ */
