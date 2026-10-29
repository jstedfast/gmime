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


#ifndef __GMIME_STREAM_BUFFER_H__
#define __GMIME_STREAM_BUFFER_H__

#include <gmime/gmime-stream.h>

G_BEGIN_DECLS

#define GMIME_TYPE_STREAM_BUFFER            (g_mime_stream_buffer_get_type ())



/**
 * GMimeStreamBufferMode:
 * @GMIME_STREAM_BUFFER_BLOCK_READ: Read in 4k blocks.
 * @GMIME_STREAM_BUFFER_BLOCK_WRITE: Write in 4k blocks.
 *
 * The buffering mode for a #GMimeStreamBuffer stream.
 **/
typedef enum {
	GMIME_STREAM_BUFFER_BLOCK_READ,
	GMIME_STREAM_BUFFER_BLOCK_WRITE
} GMimeStreamBufferMode;


/**
 * GMimeStreamBuffer:
 *
 * A buffered stream wrapper around any #GMimeStream object.
 **/
G_DECLARE_DERIVABLE_TYPE (GMimeStreamBuffer, g_mime_stream_buffer, GMIME, STREAM_BUFFER, GMimeStream)

struct _GMimeStreamBufferClass {
	GMimeStreamClass parent_class;

	/*< private >*/
	gpointer reserved[8];
};


GMimeStream *g_mime_stream_buffer_new (GMimeStream *source, GMimeStreamBufferMode mode);

ssize_t g_mime_stream_buffer_gets (GMimeStream *stream, char *buf, size_t max);

void    g_mime_stream_buffer_readln (GMimeStream *stream, GByteArray *buffer);

G_END_DECLS

#endif /* __GMIME_STREAM_BUFFER_H__ */
