/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 *  Authors: Jeffrey Stedfast <fejj@ximian.com>
 *
 *  Copyright 2001 Ximain, Inc. (www.ximian.com)
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


#ifndef __GMIME_STREAM_H__
#define __GMIME_STREAM_H__

#ifdef __cplusplus
extern "C" {
#pragma }
#endif /* __cplusplus */

#include <glib.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdarg.h>

typedef enum {
	GMIME_STREAM_SEEK_SET = SEEK_SET,
	GMIME_STREAM_SEEK_CUR = SEEK_CUR,
	GMIME_STREAM_SEEK_END = SEEK_END,
} GMimeSeekWhence;

typedef struct _GMimeStream GMimeStream;

struct _GMimeStream {
	/* Note: these are private fields!! */
	GMimeStream *super_stream;
	
	int type;
	int refcount;
	
	off_t position;
	off_t bound_start;
	off_t bound_end;
	
	void     (*destroy) (GMimeStream *stream);
	
	ssize_t  (*read)    (GMimeStream *stream, char *buf, size_t len);
	ssize_t  (*write)   (GMimeStream *stream, char *buf, size_t len);
	int      (*flush)   (GMimeStream *stream);
	int      (*close)   (GMimeStream *stream);
	gboolean (*eos)     (GMimeStream *stream);
	int      (*reset)   (GMimeStream *stream);
	off_t    (*seek)    (GMimeStream *stream, off_t offset, GMimeSeekWhence whence);
	off_t    (*tell)    (GMimeStream *stream);
	ssize_t  (*length)  (GMimeStream *stream);
	GMimeStream *(*substream) (GMimeStream *stream, off_t start, off_t end);
};

#define GMIME_STREAM(stream)  ((GMimeStream *) stream)

void g_mime_stream_construct (GMimeStream *stream, GMimeStream *stream_template,
			      int type, off_t start, off_t end);

/* public methods */
ssize_t   g_mime_stream_read    (GMimeStream *stream, char *buf, size_t len);
ssize_t   g_mime_stream_write   (GMimeStream *stream, char *buf, size_t len);
int       g_mime_stream_flush   (GMimeStream *stream);
int       g_mime_stream_close   (GMimeStream *stream);
gboolean  g_mime_stream_eos     (GMimeStream *stream);
int       g_mime_stream_reset   (GMimeStream *stream);
off_t     g_mime_stream_seek    (GMimeStream *stream, off_t offset, GMimeSeekWhence whence);
off_t     g_mime_stream_tell    (GMimeStream *stream);
ssize_t   g_mime_stream_length  (GMimeStream *stream);

GMimeStream *g_mime_stream_substream (GMimeStream *stream, off_t start, off_t end);

void      g_mime_stream_ref     (GMimeStream *stream);
void      g_mime_stream_unref   (GMimeStream *stream);

void      g_mime_stream_set_bounds (GMimeStream *stream, off_t start, off_t end);

ssize_t   g_mime_stream_write_string (GMimeStream *stream, const char *string);
ssize_t   g_mime_stream_printf       (GMimeStream *stream, const char *fmt, ...) G_GNUC_PRINTF (2, 3);

ssize_t   g_mime_stream_write_to_stream (GMimeStream *src, GMimeStream *dest);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* __GMIME_STREAM_H__ */
