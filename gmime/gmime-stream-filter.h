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


#ifndef __GMIME_STREAM_FILTER_H__
#define __GMIME_STREAM_FILTER_H__

#include <gmime/gmime-stream.h>
#include <gmime/gmime-filter.h>

G_BEGIN_DECLS

#define GMIME_TYPE_STREAM_FILTER            (g_mime_stream_filter_get_type ())


/**
 * GMimeStreamFilter:
 *
 * A #GMimeStream which passes data through any #GMimeFilter objects.
 **/
G_DECLARE_DERIVABLE_TYPE (GMimeStreamFilter, g_mime_stream_filter, GMIME, STREAM_FILTER, GMimeStream)

struct _GMimeStreamFilterClass {
	GMimeStreamClass parent_class;

	/*< private >*/
	gpointer reserved[8];
};


GMimeStream *g_mime_stream_filter_new (GMimeStream *stream);

GMimeStream *g_mime_stream_filter_get_source (GMimeStreamFilter *stream);

int g_mime_stream_filter_add (GMimeStreamFilter *stream, GMimeFilter *filter);
gboolean g_mime_stream_filter_contains (GMimeStreamFilter *stream, GMimeFilter *filter);
gboolean g_mime_stream_filter_remove_filter (GMimeStreamFilter *stream, GMimeFilter *filter);
void g_mime_stream_filter_remove (GMimeStreamFilter *stream, int id);

void g_mime_stream_filter_set_owner (GMimeStreamFilter *stream, gboolean owner);
gboolean g_mime_stream_filter_get_owner (GMimeStreamFilter *stream);

G_END_DECLS

#endif /* __GMIME_STREAM_FILTER_H__ */
