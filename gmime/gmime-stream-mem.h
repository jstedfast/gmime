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


#ifndef __GMIME_STREAM_MEM_H__
#define __GMIME_STREAM_MEM_H__

#include <glib.h>
#include <gmime/gmime-stream.h>

G_BEGIN_DECLS

#define GMIME_TYPE_STREAM_MEM            (g_mime_stream_mem_get_type ())
#define GMIME_STREAM_MEM(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), GMIME_TYPE_STREAM_MEM, GMimeStreamMem))
#define GMIME_STREAM_MEM_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), GMIME_TYPE_STREAM_MEM, GMimeStreamMemClass))
#define GMIME_IS_STREAM_MEM(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GMIME_TYPE_STREAM_MEM))
#define GMIME_IS_STREAM_MEM_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), GMIME_TYPE_STREAM_MEM))
#define GMIME_STREAM_MEM_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), GMIME_TYPE_STREAM_MEM, GMimeStreamMemClass))

typedef struct _GMimeStreamMem GMimeStreamMem;
typedef struct _GMimeStreamMemClass GMimeStreamMemClass;

/**
 * GMimeStreamMem:
 * @parent_object: parent #GMimeStream
 * @buffer: a memory buffer
 * @owner: %TRUE if this stream owns the memory buffer
 *
 * A memory-backed #GMimeStream.
 **/
struct _GMimeStreamMem {
	GMimeStream parent_object;
	
	GByteArray *buffer;
	gboolean owner;
};

struct _GMimeStreamMemClass {
	GMimeStreamClass parent_class;
	
};


GType g_mime_stream_mem_get_type (void);

GMimeStream *g_mime_stream_mem_new (void);
GMimeStream *g_mime_stream_mem_new_with_byte_array (GByteArray *array);
GMimeStream *g_mime_stream_mem_new_with_buffer (const char *buffer, size_t len);

GByteArray *g_mime_stream_mem_get_byte_array (GMimeStreamMem *mem);
void g_mime_stream_mem_set_byte_array (GMimeStreamMem *mem, GByteArray *array);

gboolean g_mime_stream_mem_get_owner (GMimeStreamMem *mem);
void g_mime_stream_mem_set_owner (GMimeStreamMem *mem, gboolean owner);

G_END_DECLS

#endif /* __GMIME_STREAM_MEM_H__ */
