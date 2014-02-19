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


#ifndef __GMIME_STREAM_H__
#define __GMIME_STREAM_H__

#include <glib.h>
#include <glib-object.h>

#include <stdio.h>
#include <sys/types.h>
#include <stdarg.h>

G_BEGIN_DECLS

#define GMIME_TYPE_STREAM            (g_mime_stream_get_type ())
#define GMIME_STREAM(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), GMIME_TYPE_STREAM, GMimeStream))
#define GMIME_STREAM_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), GMIME_TYPE_STREAM, GMimeStreamClass))
#define GMIME_IS_STREAM(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GMIME_TYPE_STREAM))
#define GMIME_IS_STREAM_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), GMIME_TYPE_STREAM))
#define GMIME_STREAM_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), GMIME_TYPE_STREAM, GMimeStreamClass))

typedef struct _GMimeStream GMimeStream;
typedef struct _GMimeStreamClass GMimeStreamClass;


/**
 * GMimeSeekWhence:
 * @GMIME_STREAM_SEEK_SET: Seek relative to the beginning of the stream.
 * @GMIME_STREAM_SEEK_CUR: Seek relative to the current position in the stream.
 * @GMIME_STREAM_SEEK_END: Seek relative to the end of the stream.
 *
 * Relative seek position.
 **/
typedef enum {
	GMIME_STREAM_SEEK_SET = SEEK_SET,
	GMIME_STREAM_SEEK_CUR = SEEK_CUR,
	GMIME_STREAM_SEEK_END = SEEK_END
} GMimeSeekWhence;


/**
 * GMimeStreamIOVector:
 * @data: data to pass to the I/O function.
 * @len: length of the data, in bytes.
 *
 * An I/O vector for use with g_mime_stream_writev().
 **/
typedef struct {
	void *data;
	size_t len;
} GMimeStreamIOVector;


/**
 * GMimeStream:
 * @parent_object: parent #GObject
 * @super_stream: parent stream if this is a substream
 * @position: the current stream position
 * @bound_start: start boundary of the stream
 * @bound_end: end boundary of the stream
 *
 * Abstract I/O stream class.
 **/
struct _GMimeStream {
	GObject parent_object;
	
	/* <private> */
	GMimeStream *super_stream;
	
	gint64 position;
	gint64 bound_start;
	gint64 bound_end;
};

struct _GMimeStreamClass {
	GObjectClass parent_class;
	
	ssize_t  (* read)   (GMimeStream *stream, char *buf, size_t len);
	ssize_t  (* write)  (GMimeStream *stream, const char *buf, size_t len);
	int      (* flush)  (GMimeStream *stream);
	int      (* close)  (GMimeStream *stream);
	gboolean (* eos)    (GMimeStream *stream);
	int      (* reset)  (GMimeStream *stream);
	gint64   (* seek)   (GMimeStream *stream, gint64 offset, GMimeSeekWhence whence);
	gint64   (* tell)   (GMimeStream *stream);
	gint64   (* length) (GMimeStream *stream);
	GMimeStream * (* substream) (GMimeStream *stream, gint64 start, gint64 end);
};


GType g_mime_stream_get_type (void);

void g_mime_stream_construct (GMimeStream *stream, gint64 start, gint64 end);


/* public methods */
ssize_t   g_mime_stream_read    (GMimeStream *stream, char *buf, size_t len);
ssize_t   g_mime_stream_write   (GMimeStream *stream, const char *buf, size_t len);
int       g_mime_stream_flush   (GMimeStream *stream);
int       g_mime_stream_close   (GMimeStream *stream);
gboolean  g_mime_stream_eos     (GMimeStream *stream);
int       g_mime_stream_reset   (GMimeStream *stream);
gint64    g_mime_stream_seek    (GMimeStream *stream, gint64 offset, GMimeSeekWhence whence);
gint64    g_mime_stream_tell    (GMimeStream *stream);
gint64    g_mime_stream_length  (GMimeStream *stream);

GMimeStream *g_mime_stream_substream (GMimeStream *stream, gint64 start, gint64 end);

void      g_mime_stream_set_bounds (GMimeStream *stream, gint64 start, gint64 end);

ssize_t   g_mime_stream_write_string (GMimeStream *stream, const char *str);
ssize_t   g_mime_stream_printf       (GMimeStream *stream, const char *fmt, ...) G_GNUC_PRINTF (2, 3);

ssize_t   g_mime_stream_write_to_stream (GMimeStream *src, GMimeStream *dest);

ssize_t   g_mime_stream_writev (GMimeStream *stream, GMimeStreamIOVector *vector, size_t count);

G_END_DECLS

#endif /* __GMIME_STREAM_H__ */
