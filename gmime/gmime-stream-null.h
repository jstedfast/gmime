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


#ifndef __GMIME_STREAM_NULL_H__
#define __GMIME_STREAM_NULL_H__

#include <glib.h>
#include <gmime/gmime-stream.h>

G_BEGIN_DECLS

#define GMIME_TYPE_STREAM_NULL            (g_mime_stream_null_get_type ())


/**
 * GMimeStreamNull:
 *
 * A #GMimeStream which has no backing store.
 **/
G_DECLARE_DERIVABLE_TYPE (GMimeStreamNull, g_mime_stream_null, GMIME, STREAM_NULL, GMimeStream)

struct _GMimeStreamNullClass {
	GMimeStreamClass parent_class;

	/*< private >*/
	gpointer reserved[8];
};


GMimeStream *g_mime_stream_null_new (void);

int g_mime_stream_null_set_length (GMimeStreamNull *stream, gint64 length);

void g_mime_stream_null_set_count_newlines (GMimeStreamNull *stream, gboolean count);
gboolean g_mime_stream_null_get_count_newlines (GMimeStreamNull *stream);
size_t g_mime_stream_null_get_written (GMimeStreamNull *stream);
size_t g_mime_stream_null_get_newlines (GMimeStreamNull *stream);

G_END_DECLS

#endif /* __GMIME_STREAM_NULL_H__ */
