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


#ifndef __GMIME_STREAM_FILE_H__
#define __GMIME_STREAM_FILE_H__

#include <glib.h>
#include <stdio.h>
#include <gmime/gmime-stream.h>

G_BEGIN_DECLS

#define GMIME_TYPE_STREAM_FILE            (g_mime_stream_file_get_type ())


/**
 * GMimeStreamFile:
 *
 * A #GMimeStream wrapper around standard-c FILE pointers.
 **/
G_DECLARE_DERIVABLE_TYPE (GMimeStreamFile, g_mime_stream_file, GMIME, STREAM_FILE, GMimeStream)

struct _GMimeStreamFileClass {
	GMimeStreamClass parent_class;

	/*< private >*/
	gpointer reserved[8];
};


GMimeStream *g_mime_stream_file_new (FILE *fp);
GMimeStream *g_mime_stream_file_new_with_bounds (FILE *fp, gint64 start, gint64 end);

GMimeStream *g_mime_stream_file_open (const char *path, const char *mode, GError **err);

gboolean g_mime_stream_file_get_owner (GMimeStreamFile *stream);
void g_mime_stream_file_set_owner (GMimeStreamFile *stream, gboolean owner);

G_END_DECLS

#endif /* __GMIME_STREAM_FILE_H__ */
