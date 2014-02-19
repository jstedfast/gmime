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


#ifndef __GMIME_STREAM_FILE_H__
#define __GMIME_STREAM_FILE_H__

#include <glib.h>
#include <stdio.h>
#include <gmime/gmime-stream.h>

G_BEGIN_DECLS

#define GMIME_TYPE_STREAM_FILE            (g_mime_stream_file_get_type ())
#define GMIME_STREAM_FILE(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), GMIME_TYPE_STREAM_FILE, GMimeStreamFile))
#define GMIME_STREAM_FILE_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), GMIME_TYPE_STREAM_FILE, GMimeStreamFileClass))
#define GMIME_IS_STREAM_FILE(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GMIME_TYPE_STREAM_FILE))
#define GMIME_IS_STREAM_FILE_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), GMIME_TYPE_STREAM_FILE))
#define GMIME_STREAM_FILE_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), GMIME_TYPE_STREAM_FILE, GMimeStreamFileClass))

typedef struct _GMimeStreamFile GMimeStreamFile;
typedef struct _GMimeStreamFileClass GMimeStreamFileClass;

/**
 * GMimeStreamFile:
 * @parent_object: parent #GMimeStream
 * @owner: %TRUE if this stream owns @fd
 * @fp: standard-c FILE pointer
 *
 * A #GMimeStream wrapper around standard-c FILE pointers.
 **/
struct _GMimeStreamFile {
	GMimeStream parent_object;
	
	gboolean owner;
	FILE *fp;
};

struct _GMimeStreamFileClass {
	GMimeStreamClass parent_class;
	
};


GType g_mime_stream_file_get_type (void);

GMimeStream *g_mime_stream_file_new (FILE *fp);
GMimeStream *g_mime_stream_file_new_with_bounds (FILE *fp, gint64 start, gint64 end);

GMimeStream *g_mime_stream_file_new_for_path (const char *path, const char *mode);

gboolean g_mime_stream_file_get_owner (GMimeStreamFile *stream);
void g_mime_stream_file_set_owner (GMimeStreamFile *stream, gboolean owner);

G_END_DECLS

#endif /* __GMIME_STREAM_FILE_H__ */
