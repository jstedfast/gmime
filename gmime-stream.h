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
#include <glib-object.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdarg.h>

#include "gmime-type-utils.h"

#define GMIME_TYPE_STREAM                 (g_mime_stream_get_type ())
#define GMIME_STREAM(obj)                 (GMIME_CHECK_CAST ((obj), GMIME_TYPE_STREAM, GMimeStream))
#define GMIME_STREAM_CLASS(klass)         (GMIME_CHECK_CLASS_CAST ((klass), GMIME_TYPE_STREAM, GMimeStreamClass))
#define GMIME_IS_STREAM(obj)              (GMIME_CHECK_TYPE ((obj), GMIME_TYPE_STREAM))
#define GMIME_IS_STREAM_CLASS(klass)      (GMIME_CHECK_CLASS_TYPE ((klass), GMIME_TYPE_STREAM))
#define GMIME_STREAM_GET_CLASS(obj)       (GMIME_CHECK_GET_CLASS ((obj), GMIME_TYPE_STREAM, GMimeStreamClass))

typedef enum {
	GMIME_STREAM_SEEK_SET = SEEK_SET,
	GMIME_STREAM_SEEK_CUR = SEEK_CUR,
	GMIME_STREAM_SEEK_END = SEEK_END,
} GMimeSeekWhence;

typedef struct _GMimeStreamIOVector {
	gpointer data;
	size_t len;
} GMimeStreamIOVector;

typedef struct _GMimeStream GMimeStream;
typedef struct _GMimeStreamClass GMimeStreamClass;

struct _GMimeStream {
	GObject parent_object;
	
	/* Note: these are private fields!! */
	GMimeStream *super_stream;
	
	off_t position;
	off_t bound_start;
	off_t bound_end;
};

struct _GMimeStreamClass {
	GObjectClass parent_class;
	
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


GType g_mime_stream_get_type (void);

void g_mime_stream_construct (GMimeStream *stream, off_t start, off_t end);


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


ssize_t    g_mime_stream_writev (GMimeStream *stream, GMimeStreamIOVector *vector, size_t count);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* __GMIME_STREAM_H__ */
