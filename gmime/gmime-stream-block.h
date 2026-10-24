/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*  GMime
 *  Copyright (C) 2026 Jeffrey Stedfast
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

#ifndef __GMIME_STREAM_BLOCK_H__
#define __GMIME_STREAM_BLOCK_H__

#include <gmime/gmime-stream.h>

G_BEGIN_DECLS

#define GMIME_TYPE_STREAM_BLOCK            (g_mime_stream_block_get_type ())
#define GMIME_STREAM_BLOCK(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), GMIME_TYPE_STREAM_BLOCK, GMimeStreamBlock))
#define GMIME_STREAM_BLOCK_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), GMIME_TYPE_STREAM_BLOCK, GMimeStreamBlockClass))
#define GMIME_IS_STREAM_BLOCK(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GMIME_TYPE_STREAM_BLOCK))
#define GMIME_IS_STREAM_BLOCK_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), GMIME_TYPE_STREAM_BLOCK))
#define GMIME_STREAM_BLOCK_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), GMIME_TYPE_STREAM_BLOCK, GMimeStreamBlockClass))

typedef struct _GMimeStreamBlock GMimeStreamBlock;
typedef struct _GMimeStreamBlockClass GMimeStreamBlockClass;

/**
 * GMimeStreamBlock:
 * @parent_object: parent #GMimeStream
 *
 * A memory stream backed by non-contiguous 2048-byte blocks.
 **/
struct _GMimeStreamBlock {
	GMimeStream parent_object;

	/* <private> */
	struct _GMimeStreamBlockPrivate *priv;
};

struct _GMimeStreamBlockClass {
	GMimeStreamClass parent_class;
};

GType g_mime_stream_block_get_type (void);

GMimeStream *g_mime_stream_block_new (void);
int g_mime_stream_block_set_length (GMimeStreamBlock *stream, gint64 length);
GByteArray *g_mime_stream_block_to_byte_array (GMimeStreamBlock *stream, GError **error);

G_END_DECLS

#endif /* __GMIME_STREAM_BLOCK_H__ */
