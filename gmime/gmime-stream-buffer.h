/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 *  Authors: Jeffrey Stedfast <fejj@ximian.com>
 *
 *  Copyright 2001 Ximian, Inc. (www.ximian.com)
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


#ifndef __GMIME_STREAM_BUFFER_H__
#define __GMIME_STREAM_BUFFER_H__

#ifdef __cplusplus
extern "C" {
#pragma }
#endif /* __cplusplus */

#include <gmime/gmime-stream.h>

#define GMIME_TYPE_STREAM_BUFFER            (g_mime_stream_buffer_get_type ())
#define GMIME_STREAM_BUFFER(obj)            (GMIME_CHECK_CAST ((obj), GMIME_TYPE_STREAM_BUFFER, GMimeStreamBuffer))
#define GMIME_STREAM_BUFFER_CLASS(klass)    (GMIME_CHECK_CLASS_CAST ((klass), GMIME_TYPE_STREAM_BUFFER, GMimeStreamBufferClass))
#define GMIME_IS_STREAM_BUFFER(obj)         (GMIME_CHECK_TYPE ((obj), GMIME_TYPE_STREAM_BUFFER))
#define GMIME_IS_STREAM_BUFFER_CLASS(klass) (GMIME_CHECK_CLASS_TYPE ((klass), GMIME_TYPE_STREAM_BUFFER))
#define GMIME_STREAM_BUFFER_GET_CLASS(obj)  (GMIME_CHECK_GET_CLASS ((obj), GMIME_TYPE_STREAM_BUFFER, GMimeStreamBufferClass))


typedef enum {
	GMIME_STREAM_BUFFER_CACHE_READ,
	GMIME_STREAM_BUFFER_BLOCK_READ,
	GMIME_STREAM_BUFFER_BLOCK_WRITE
} GMimeStreamBufferMode;

typedef struct _GMimeStreamBuffer GMimeStreamBuffer;
typedef struct _GMimeStreamBufferClass GMimeStreamBufferClass;

struct _GMimeStreamBuffer {
	GMimeStream parent_object;
	
	GMimeStream *source;
	
	unsigned char *buffer;
	unsigned char *bufptr;
	unsigned char *bufend;
	ssize_t buflen;
	
	GMimeStreamBufferMode mode;
};

struct _GMimeStreamBufferClass {
	GMimeStreamClass parent_class;
	
};


GType g_mime_stream_buffer_get_type (void);

GMimeStream *g_mime_stream_buffer_new (GMimeStream *source, GMimeStreamBufferMode mode);

ssize_t g_mime_stream_buffer_gets (GMimeStream *stream, char *buf, size_t max);

void    g_mime_stream_buffer_readln (GMimeStream *stream, GByteArray *buffer);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* __GMIME_STREAM_BUFFER_H__ */
