/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 *  Authors: Jeffrey Stedfast <fejj@ximian.com>
 *
 *  Copyright 2001-2004 Ximian, Inc. (www.ximian.com)
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
 *
 */


#ifndef __GMIME_STREAM_FILE_H__
#define __GMIME_STREAM_FILE_H__

#include <glib.h>
#include <stdio.h>
#include <gmime/gmime-stream.h>

#ifdef __cplusplus
extern "C" {
#pragma }
#endif /* __cplusplus */

#define GMIME_TYPE_STREAM_FILE            (g_mime_stream_file_get_type ())
#define GMIME_STREAM_FILE(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), GMIME_TYPE_STREAM_FILE, GMimeStreamFile))
#define GMIME_STREAM_FILE_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), GMIME_TYPE_STREAM_FILE, GMimeStreamFileClass))
#define GMIME_IS_STREAM_FILE(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GMIME_TYPE_STREAM_FILE))
#define GMIME_IS_STREAM_FILE_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), GMIME_TYPE_STREAM_FILE))
#define GMIME_STREAM_FILE_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), GMIME_TYPE_STREAM_FILE, GMimeStreamFileClass))

typedef struct _GMimeStreamFile GMimeStreamFile;
typedef struct _GMimeStreamFileClass GMimeStreamFileClass;

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
GMimeStream *g_mime_stream_file_new_with_bounds (FILE *fp, off_t start, off_t end);

gboolean g_mime_stream_file_get_owner (GMimeStreamFile *stream);
void g_mime_stream_file_set_owner (GMimeStreamFile *stream, gboolean owner);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* __GMIME_STREAM_FILE_H__ */
