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


#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <string.h>

#include "gmime-stream.h"

#define d(x)


/**
 * SECTION: gmime-stream
 * @title: GMimeStream
 * @short_description: Abstract stream class
 * @see_also:
 *
 * Streams are the fundamental method for reading and writing data
 * used by GMime. You'll probably notice that the basic API is similar
 * to that of the low-level Unix I/O layer (read(), write(), lseek(),
 * etc) with some additional nicities such as a printf-like function.
 **/


static void g_mime_stream_class_init (GMimeStreamClass *klass);
static void g_mime_stream_init (GMimeStream *stream, GMimeStreamClass *klass);
static void g_mime_stream_finalize (GObject *object);

static ssize_t stream_read (GMimeStream *stream, char *buf, size_t len);
static ssize_t stream_write (GMimeStream *stream, const char *buf, size_t len);
static int stream_flush (GMimeStream *stream);
static int stream_close (GMimeStream *stream);
static gboolean stream_eos (GMimeStream *stream);
static int stream_reset (GMimeStream *stream);
static gint64 stream_seek (GMimeStream *stream, gint64 offset, GMimeSeekWhence whence);
static gint64 stream_tell (GMimeStream *stream);
static gint64 stream_length (GMimeStream *stream);
static GMimeStream *stream_substream (GMimeStream *stream, gint64 start, gint64 end);


static GObjectClass *parent_class = NULL;


GType
g_mime_stream_get_type (void)
{
	static GType type = 0;
	
	if (!type) {
		static const GTypeInfo info = {
			sizeof (GMimeStreamClass),
			NULL, /* base_class_init */
			NULL, /* base_class_finalize */
			(GClassInitFunc) g_mime_stream_class_init,
			NULL, /* class_finalize */
			NULL, /* class_data */
			sizeof (GMimeStream),
			0,    /* n_preallocs */
			(GInstanceInitFunc) g_mime_stream_init,
		};
		
		type = g_type_register_static (G_TYPE_OBJECT, "GMimeStream",
					       &info, G_TYPE_FLAG_ABSTRACT);
	}
	
	return type;
}


static void
g_mime_stream_class_init (GMimeStreamClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	
	parent_class = g_type_class_ref (G_TYPE_OBJECT);
	
	object_class->finalize = g_mime_stream_finalize;
	
	klass->read = stream_read;
	klass->write = stream_write;
	klass->flush = stream_flush;
	klass->close = stream_close;
	klass->eos = stream_eos;
	klass->reset = stream_reset;
	klass->seek = stream_seek;
	klass->tell = stream_tell;
	klass->length = stream_length;
	klass->substream = stream_substream;
}

static void
g_mime_stream_init (GMimeStream *stream, GMimeStreamClass *klass)
{
	stream->super_stream = NULL;
	
	stream->position = 0;
	stream->bound_start = 0;
	stream->bound_end = 0;
}

static void
g_mime_stream_finalize (GObject *object)
{
	GMimeStream *stream = (GMimeStream *) object;
	
	if (stream->super_stream)
		g_object_unref (stream->super_stream);
	
	G_OBJECT_CLASS (parent_class)->finalize (object);
}


/**
 * g_mime_stream_construct:
 * @stream: a #GMimeStream
 * @start: start boundary
 * @end: end boundary
 *
 * Initializes a new stream with bounds @start and @end.
 **/
void
g_mime_stream_construct (GMimeStream *stream, gint64 start, gint64 end)
{
	stream->position = start;
	stream->bound_start = start;
	stream->bound_end = end;
}


static ssize_t
stream_read (GMimeStream *stream, char *buf, size_t len)
{
	d(g_warning ("Invoked default stream_read implementation."));
	return 0;
}


/**
 * g_mime_stream_read:
 * @stream: a #GMimeStream
 * @buf: (array length=len) (element-type guint8): buffer
 * @len: buffer length
 *
 * Attempts to read up to @len bytes from @stream into @buf.
 *
 * Returns: the number of bytes read or %-1 on fail.
 **/
ssize_t
g_mime_stream_read (GMimeStream *stream, char *buf, size_t len)
{
	g_return_val_if_fail (GMIME_IS_STREAM (stream), -1);
	g_return_val_if_fail (buf != NULL, -1);
	
	if (len == 0)
		return 0;
	
	return GMIME_STREAM_GET_CLASS (stream)->read (stream, buf, len);
}


static ssize_t
stream_write (GMimeStream *stream, const char *buf, size_t len)
{
	d(g_warning ("Invoked default stream_write implementation."));
	return 0;
}


/**
 * g_mime_stream_write:
 * @stream: a #GMimeStream
 * @buf: buffer
 * @len: buffer length
 *
 * Attempts to write up to @len bytes of @buf to @stream.
 *
 * Returns: the number of bytes written or %-1 on fail.
 **/
ssize_t
g_mime_stream_write (GMimeStream *stream, const char *buf, size_t len)
{
	g_return_val_if_fail (GMIME_IS_STREAM (stream), -1);
	g_return_val_if_fail (buf != NULL, -1);
	
	if (len == 0)
		return 0;
	
	return GMIME_STREAM_GET_CLASS (stream)->write (stream, buf, len);
}


static int
stream_flush (GMimeStream *stream)
{
	d(g_warning ("Invoked default stream_flush implementation."));
	return 0;
}


/**
 * g_mime_stream_flush:
 * @stream: a #GMimeStream
 *
 * Sync's the stream to disk.
 *
 * Returns: %0 on success or %-1 on fail.
 **/
int
g_mime_stream_flush (GMimeStream *stream)
{
	g_return_val_if_fail (GMIME_IS_STREAM (stream), -1);
	
	return GMIME_STREAM_GET_CLASS (stream)->flush (stream);
}


static int
stream_close (GMimeStream *stream)
{
	d(g_warning ("Invoked default stream_close implementation."));
	return 0;
}


/**
 * g_mime_stream_close:
 * @stream: a #GMimeStream
 *
 * Closes the stream.
 *
 * Returns: %0 on success or %-1 on fail.
 **/
int
g_mime_stream_close (GMimeStream *stream)
{
	g_return_val_if_fail (GMIME_IS_STREAM (stream), -1);
	
	return GMIME_STREAM_GET_CLASS (stream)->close (stream);
}


static gboolean
stream_eos (GMimeStream *stream)
{
	d(g_warning ("Invoked default stream_eos implementation."));
	return stream->position >= stream->bound_end;
}


/**
 * g_mime_stream_eos:
 * @stream: a #GMimeStream
 *
 * Tests the end-of-stream indicator for @stream.
 *
 * Returns: %TRUE on EOS or %FALSE otherwise.
 **/
gboolean
g_mime_stream_eos (GMimeStream *stream)
{
	g_return_val_if_fail (GMIME_IS_STREAM (stream), TRUE);
	
	if (stream->bound_end != -1 && stream->position >= stream->bound_end)
		return TRUE;
	
	return GMIME_STREAM_GET_CLASS (stream)->eos (stream);
}


static int
stream_reset (GMimeStream *stream)
{
	d(g_warning ("Invoked default stream_reset implementation."));
	return 0;
}


/**
 * g_mime_stream_reset:
 * @stream: a #GMimeStream
 *
 * Resets the stream.
 *
 * Returns: %0 on success or %-1 on fail.
 **/
int
g_mime_stream_reset (GMimeStream *stream)
{
	int rv;
	
	g_return_val_if_fail (GMIME_IS_STREAM (stream), -1);
	
	if ((rv = GMIME_STREAM_GET_CLASS (stream)->reset (stream)) == 0)
		stream->position = stream->bound_start;
	
	return rv;
}


static gint64
stream_seek (GMimeStream *stream, gint64 offset, GMimeSeekWhence whence)
{
	d(g_warning ("Invoked default stream_seek implementation."));
	return -1;
}


/**
 * g_mime_stream_seek:
 * @stream: a #GMimeStream
 * @offset: positional offset
 * @whence: seek directive
 *
 * Repositions the offset of the stream @stream to
 * the argument @offset according to the
 * directive @whence as follows:
 *
 *     #GMIME_STREAM_SEEK_SET: Seek @offset bytes relative to
 *     the beginning (bound_start) of the stream.
 *
 *     #GMIME_STREAM_SEEK_CUR: Seek @offset bytes relative to the
 *     current offset of the stream.
 *
 *     #GMIME_STREAM_SEEK_END: Seek @offset bytes relative to the
 *     end of the stream (bound_end if non-negative).
 *
 * Returns: the resultant position on success or %-1 on fail.
 **/
gint64
g_mime_stream_seek (GMimeStream *stream, gint64 offset, GMimeSeekWhence whence)
{
	g_return_val_if_fail (GMIME_IS_STREAM (stream), -1);
	
	return GMIME_STREAM_GET_CLASS (stream)->seek (stream, offset, whence);
}


static gint64
stream_tell (GMimeStream *stream)
{
	d(g_warning ("Invoked default stream_tell implementation."));
	return stream->position;
}


/**
 * g_mime_stream_tell:
 * @stream: a #GMimeStream
 *
 * Gets the current offset within the stream.
 *
 * Returns: the current position within the stream or %-1 on fail.
 **/
gint64
g_mime_stream_tell (GMimeStream *stream)
{
	g_return_val_if_fail (GMIME_IS_STREAM (stream), -1);
	
	return GMIME_STREAM_GET_CLASS (stream)->tell (stream);
}


static gint64
stream_length (GMimeStream *stream)
{
	gint64 position = stream->position;
	gint64 bound_end;
	
	if (stream->bound_end != -1)
		return stream->bound_end - stream->bound_start;
	
	bound_end = g_mime_stream_seek (stream, 0, GMIME_STREAM_SEEK_END);
	g_mime_stream_seek (stream, position, GMIME_STREAM_SEEK_SET);
	
	if (bound_end < stream->bound_start)
		return -1;
	
	return bound_end - stream->bound_start;
}


/**
 * g_mime_stream_length:
 * @stream: a #GMimeStream
 *
 * Gets the length of the stream.
 *
 * Returns: the length of the stream or %-1 if unknown.
 **/
gint64
g_mime_stream_length (GMimeStream *stream)
{
	g_return_val_if_fail (GMIME_IS_STREAM (stream), -1);
	
	return GMIME_STREAM_GET_CLASS (stream)->length (stream);
}


static GMimeStream *
stream_substream (GMimeStream *stream, gint64 start, gint64 end)
{
	d(g_warning ("Invoked default stream_tell implementation."));
	return NULL;
}


/**
 * g_mime_stream_substream:
 * @stream: a #GMimeStream
 * @start: start boundary
 * @end: end boundary
 *
 * Creates a new substream of @stream with bounds @start and @end.
 *
 * Returns: (transfer full): a substream of @stream with bounds @start
 * and @end.
 **/
GMimeStream *
g_mime_stream_substream (GMimeStream *stream, gint64 start, gint64 end)
{
	GMimeStream *sub;
	
	g_return_val_if_fail (GMIME_IS_STREAM (stream), NULL);
	
	if ((sub = GMIME_STREAM_GET_CLASS (stream)->substream (stream, start, end))) {
		sub->super_stream = stream;
		g_object_ref (stream);
	}
	
	return sub;
}


/**
 * g_mime_stream_set_bounds:
 * @stream: a #GMimeStream
 * @start: start boundary
 * @end: end boundary
 *
 * Set the bounds on a stream.
 **/
void
g_mime_stream_set_bounds (GMimeStream *stream, gint64 start, gint64 end)
{
	g_return_if_fail (GMIME_IS_STREAM (stream));
	
	stream->bound_start = start;
	stream->bound_end = end;
	
	if (stream->position < start)
		stream->position = start;
	else if (stream->position > end && end != -1)
		stream->position = end;
}


/**
 * g_mime_stream_write_string:
 * @stream: a #GMimeStream
 * @str: string to write
 *
 * Writes @string to @stream.
 *
 * Returns: the number of bytes written or %-1 on fail.
 **/
ssize_t
g_mime_stream_write_string (GMimeStream *stream, const char *str)
{
	g_return_val_if_fail (GMIME_IS_STREAM (stream), -1);
	g_return_val_if_fail (str != NULL, -1);
	
	return g_mime_stream_write (stream, str, strlen (str));
}


/**
 * g_mime_stream_printf:
 * @stream: a #GMimeStream
 * @fmt: format
 * @Varargs: arguments
 *
 * Write formatted output to a stream.
 *
 * Returns: the number of bytes written or %-1 on fail.
 **/
ssize_t
g_mime_stream_printf (GMimeStream *stream, const char *fmt, ...)
{
	va_list args;
	char *string;
	ssize_t ret;
	
	g_return_val_if_fail (GMIME_IS_STREAM (stream), -1);
	g_return_val_if_fail (fmt != NULL, -1);
	
	va_start (args, fmt);
	string = g_strdup_vprintf (fmt, args);
	va_end (args);
	
	if (!string)
		return -1;
	
	ret = g_mime_stream_write (stream, string, strlen (string));
	g_free (string);
	
	return ret;
}


/**
 * g_mime_stream_write_to_stream:
 * @src: source stream
 * @dest: destination stream
 *
 * Attempts to write the source stream to the destination stream.
 *
 * Returns: the number of bytes written or %-1 on fail.
 **/
ssize_t
g_mime_stream_write_to_stream (GMimeStream *src, GMimeStream *dest)
{
	ssize_t nread, nwritten, total = 0;
	char buf[4096];
	
	g_return_val_if_fail (GMIME_IS_STREAM (src), -1);
	g_return_val_if_fail (GMIME_IS_STREAM (dest), -1);
	
	while (!g_mime_stream_eos (src)) {
		if ((nread = g_mime_stream_read (src, buf, sizeof (buf))) < 0)
			return -1;
		
		if (nread > 0) {
			nwritten = 0;
			while (nwritten < nread) {
				ssize_t len;
				
				if ((len = g_mime_stream_write (dest, buf + nwritten, nread - nwritten)) < 0)
					return -1;
				
				nwritten += len;
			}
			
			total += nwritten;
		}
	}
	
	return total;
}


/**
 * g_mime_stream_writev:
 * @stream: a #GMimeStream
 * @vector: a #GMimeStreamIOVector
 * @count: number of vector elements
 *
 * Writes at most @count blocks described by @vector to @stream.
 *
 * Returns: the number of bytes written or %-1 on fail.
 **/
ssize_t
g_mime_stream_writev (GMimeStream *stream, GMimeStreamIOVector *vector, size_t count)
{
	ssize_t total = 0;
	size_t i;
	
	g_return_val_if_fail (GMIME_IS_STREAM (stream), -1);
	
	for (i = 0; i < count; i++) {
		char *buffer = vector[i].data;
		size_t nwritten = 0;
		ssize_t n;
		
		while (nwritten < vector[i].len) {
			if ((n = g_mime_stream_write (stream, buffer + nwritten,
						      vector[i].len - nwritten)) < 0)
				return -1;
			
			nwritten += n;
		}
		
		total += nwritten;
	}
	
	return total;
}
