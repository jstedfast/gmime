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


#ifndef __GMIME_STREAM_MMAP_H__
#define __GMIME_STREAM_MMAP_H__

#include <gmime/gmime-stream.h>

#ifdef __cplusplus
extern "C" {
#pragma }
#endif /* __cplusplus */

#define GMIME_TYPE_STREAM_MMAP            (g_mime_stream_mmap_get_type ())
#define GMIME_STREAM_MMAP(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), GMIME_TYPE_STREAM_MMAP, GMimeStreamMmap))
#define GMIME_STREAM_MMAP_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), GMIME_TYPE_STREAM_MMAP, GMimeStreamMmapClass))
#define GMIME_IS_STREAM_MMAP(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GMIME_TYPE_STREAM_MMAP))
#define GMIME_IS_STREAM_MMAP_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), GMIME_TYPE_STREAM_MMAP))
#define GMIME_STREAM_MMAP_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), GMIME_TYPE_STREAM_MMAP, GMimeStreamMmapClass))

typedef struct _GMimeStreamMmap GMimeStreamMmap;
typedef struct _GMimeStreamMmapClass GMimeStreamMmapClass;

struct _GMimeStreamMmap {
	GMimeStream parent_object;
	
	gboolean owner;
	gboolean eos;
	int fd;
	
	char *map;
	size_t maplen;
};

struct _GMimeStreamMmapClass {
	GMimeStreamClass parent_class;
	
};


GType g_mime_stream_mmap_get_type (void);

GMimeStream *g_mime_stream_mmap_new (int fd, int prot, int flags);
GMimeStream *g_mime_stream_mmap_new_with_bounds (int fd, int prot, int flags, off_t start, off_t end);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* __GMIME_STREAM_MMAP_H__ */
