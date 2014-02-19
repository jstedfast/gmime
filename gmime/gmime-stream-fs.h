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


#ifndef __GMIME_STREAM_FS_H__
#define __GMIME_STREAM_FS_H__

#include <gmime/gmime-stream.h>

G_BEGIN_DECLS

#define GMIME_TYPE_STREAM_FS            (g_mime_stream_fs_get_type ())
#define GMIME_STREAM_FS(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), GMIME_TYPE_STREAM_FS, GMimeStreamFs))
#define GMIME_STREAM_FS_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), GMIME_TYPE_STREAM_FS, GMimeStreamFsClass))
#define GMIME_IS_STREAM_FS(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GMIME_TYPE_STREAM_FS))
#define GMIME_IS_STREAM_FS_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), GMIME_TYPE_STREAM_FS))
#define GMIME_STREAM_FS_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), GMIME_TYPE_STREAM_FS, GMimeStreamFsClass))

typedef struct _GMimeStreamFs GMimeStreamFs;
typedef struct _GMimeStreamFsClass GMimeStreamFsClass;

/**
 * GMimeStreamFs:
 * @parent_object: parent #GMimeStream
 * @owner: %TRUE if this stream owns @fd
 * @eos: %TRUE if end-of-stream
 * @fd: file descriptor
 *
 * A #GMimeStream wrapper around POSIX file descriptors.
 **/
struct _GMimeStreamFs {
	GMimeStream parent_object;
	
	gboolean owner;
	gboolean eos;
	int fd;
};

struct _GMimeStreamFsClass {
	GMimeStreamClass parent_class;
	
};


GType g_mime_stream_fs_get_type (void);

GMimeStream *g_mime_stream_fs_new (int fd);
GMimeStream *g_mime_stream_fs_new_with_bounds (int fd, gint64 start, gint64 end);

GMimeStream *g_mime_stream_fs_new_for_path (const char *path, int flags, int mode);

gboolean g_mime_stream_fs_get_owner (GMimeStreamFs *stream);
void g_mime_stream_fs_set_owner (GMimeStreamFs *stream, gboolean owner);

G_END_DECLS

#endif /* __GMIME_STREAM_FS_H__ */
