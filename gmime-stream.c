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


#include "gmime-stream.h"
#include <string.h>


/**
 * g_mime_stream_construct:
 * @stream: stream
 * @stream_template: stream template
 * @type: stream type
 * @start: start boundary
 * @end: end boundary
 *
 * Initializes a new stream of type @type, using the virtual methods
 * from @stream_template, with bounds @start and @end.
 **/
void
g_mime_stream_construct (GMimeStream *stream, GMimeStream *stream_template, int type, off_t start, off_t end)
{
	stream->super_stream = NULL;
	stream->refcount = 1;
	stream->type = type;
	
	stream->position = start;
	stream->bound_start = start;
	stream->bound_end = end;
	
	stream->destroy = stream_template->destroy;
	stream->read = stream_template->read;
	stream->write = stream_template->write;
	stream->flush = stream_template->flush;
	stream->close = stream_template->close;
	stream->reset = stream_template->reset;
	stream->seek = stream_template->seek;
	stream->tell = stream_template->tell;
	stream->eos = stream_template->eos;
	stream->length = stream_template->length;
	stream->substream = stream_template->substream;
}

/**
 * g_mime_stream_read:
 * @stream: stream
 * @buf: buffer
 * @len: buffer length
 *
 * Attempts to read up to @len bytes from @stream into @buf.
 *
 * Returns the number of bytes read or -1 on fail.
 **/
ssize_t
g_mime_stream_read (GMimeStream *stream, char *buf, size_t len)
{
	g_return_val_if_fail (stream != NULL, -1);
	g_return_val_if_fail (buf != NULL, -1);
	
	return stream->read (stream, buf, len);
}


/**
 * g_mime_stream_write:
 * @stream: stream
 * @buf: buffer
 * @len: buffer length
 *
 * Attempts to write up to @len bytes of @buf to @stream.
 *
 * Returns the number of bytes written or -1 on fail.
 **/
ssize_t
g_mime_stream_write (GMimeStream *stream, char *buf, size_t len)
{
	g_return_val_if_fail (stream != NULL, -1);
	g_return_val_if_fail (buf != NULL, -1);
	
	return stream->write (stream, buf, len);
}


/**
 * g_mime_stream_flush:
 * @stream: stream
 *
 * Sync's the stream to disk.
 *
 * Returns 0 on success or -1 on fail.
 **/
int
g_mime_stream_flush (GMimeStream *stream)
{
	g_return_val_if_fail (stream != NULL, -1);
	
	return stream->flush (stream);
}


/**
 * g_mime_stream_close:
 * @stream: stream
 *
 * Closes the stream.
 *
 * Returns 0 on success or -1 on fail.
 **/
int
g_mime_stream_close (GMimeStream *stream)
{
	g_return_val_if_fail (stream != NULL, -1);
	
	return stream->close (stream);
}


/**
 * g_mime_stream_eos:
 * @stream: stream
 *
 * Tests the end-of-stream indicator for @stream.
 *
 * Returns TRUE on EOS or FALSE otherwise.
 **/
gboolean
g_mime_stream_eos (GMimeStream *stream)
{
	g_return_val_if_fail (stream != NULL, TRUE);
	
	if (stream->bound_end != -1 && stream->position >= stream->bound_end)
		return TRUE;
	
	return stream->eos (stream);
}


/**
 * g_mime_stream_reset:
 * @stream: stream
 *
 * Resets the stream.
 *
 * Returns 0 on success or -1 on fail.
 **/
int
g_mime_stream_reset (GMimeStream *stream)
{
	g_return_val_if_fail (stream != NULL, -1);
	
	return stream->reset (stream);
}


/**
 * g_mime_stream_seek:
 * @stream: stream
 * @offset: positional offset
 * @whence: seek directive
 *
 * Repositions the offset of the stream @stream to
 * the argument @offset according to the
 * directive @whence as follows:
 *
 *     GMIME_STREAM_SEEK_SET: The offset is set to @offset bytes.
 *
 *     GMIME_STREAM_SEEK_CUR: The offset is set to its current
 *     location plus @offset bytes.
 *
 *     GMIME_STREAM_SEEK_END: The offset is set to the size of the
 *     stream plus @offset bytes.
 *
 * Returns the resultant position on success or -1 on fail.
 **/
off_t
g_mime_stream_seek (GMimeStream *stream, off_t offset, GMimeSeekWhence whence)
{
	g_return_val_if_fail (stream != NULL, -1);
	
	return stream->seek (stream, offset, whence);
}


/**
 * g_mime_stream_tell:
 * @stream: stream
 *
 * Returns the current position within the stream.
 **/
off_t
g_mime_stream_tell (GMimeStream *stream)
{
	g_return_val_if_fail (stream != NULL, -1);
	
	return stream->tell (stream);
}


/**
 * g_mime_stream_length:
 * @stream: stream
 *
 * Returns the length of the stream or -1 on fail.
 **/
ssize_t
g_mime_stream_length (GMimeStream *stream)
{
	g_return_val_if_fail (stream != NULL, -1);
	
	return stream->length (stream);
}


/**
 * g_mime_stream_substream:
 * @stream: stream
 * @start: start boundary
 * @end: end boundary
 *
 * Returns a substream of @stream with bounds @start and @end.
 **/
GMimeStream *
g_mime_stream_substream (GMimeStream *stream, off_t start, off_t end)
{
	GMimeStream *sub;
	
	g_return_val_if_fail (stream != NULL, NULL);
	
	sub = stream->substream (stream, start, end);
	sub->super_stream = stream;
	g_mime_stream_ref (stream);
	
	return sub;
}


/**
 * g_mime_stream_ref:
 * @stream: stream
 *
 * Ref's a stream.
 **/
void
g_mime_stream_ref (GMimeStream *stream)
{
	g_return_if_fail (stream != NULL);
	
	stream->refcount++;
}


/**
 * g_mime_stream_unref:
 * @stream: stream
 *
 * Unref's a stream.
 **/
void
g_mime_stream_unref (GMimeStream *stream)
{
	g_return_if_fail (stream != NULL);
	
	if (stream->refcount <= 1) {
		if (stream->super_stream)
			g_mime_stream_unref (stream->super_stream);
		
		stream->destroy (stream);
	} else {
		stream->refcount--;
	}
}


/**
 * g_mime_stream_set_bounds:
 * @stream: stream
 * @start: start boundary
 * @end: end boundary
 *
 * Set the bounds on a stream.
 **/
void
g_mime_stream_set_bounds (GMimeStream *stream, off_t start, off_t end)
{
	g_return_if_fail (stream != NULL);
	
	stream->bound_start = start;
	stream->bound_end = end;
	
	if (stream->position < start)
		stream->position = start;
	else if (stream->position > end && end != -1)
		stream->position = end;
}


/**
 * g_mime_stream_write_string:
 * @stream: stream
 * @string: string to write
 *
 * Writes @string to @stream.
 *
 * Returns the number of bytes written.
 **/
ssize_t
g_mime_stream_write_string (GMimeStream *stream, const char *string)
{
	g_return_val_if_fail (stream != NULL, -1);
	g_return_val_if_fail (string != NULL, -1);
	
	return g_mime_stream_write (stream, (char *) string, strlen (string));
}


/**
 * g_mime_stream_printf:
 * @stream: stream
 * @fmt: format
 * @Varargs: arguments
 *
 * Write formatted output to a stream.
 *
 * Returns the number of bytes written.
 **/
ssize_t
g_mime_stream_printf (GMimeStream *stream, const char *fmt, ...)
{
	va_list args;
	char *string;
	ssize_t ret;
	
	g_return_val_if_fail (stream != NULL, -1);
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
 * Attempts to write stream @src to stream @dest.
 *
 * Returns the number of bytes written.
 **/
ssize_t
g_mime_stream_write_to_stream (GMimeStream *src, GMimeStream *dest)
{
	ssize_t nread, nwritten, total = 0;
	char buf[4096];
	
	g_return_val_if_fail (src != NULL, -1);
	g_return_val_if_fail (dest != NULL, -1);
	
	while (!g_mime_stream_eos (src)) {
		nread = g_mime_stream_read (src, buf, sizeof (buf));
		if (nread < 0)
			return -1;
		
		if (nread > 0) {
			nwritten = 0;
			while (nwritten < nread) {
				ssize_t len;
				
				len = g_mime_stream_write (dest, buf + nwritten, nread - nwritten);
				if (len < 0)
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
 * @stream: stream
 * @vector: i/o vector
 * @count: number of vector elements
 *
 * Writes at most @count blocks described by @vector to @stream.
 *
 * Returns the number of bytes written.
 **/
size_t
g_mime_stream_writev (GMimeStream *stream, IOVector *vector, size_t count)
{
	size_t i, total = 0;
	
	for (i = 0; i < count; i++) {
		ssize_t n, nwritten = 0;
		
		while (nwritten < vector[i].len) {
			n = g_mime_stream_write (stream, vector[i].data + nwritten,
						 vector[i].len - nwritten);
			if (n > 0)
				nwritten += n;
		}
		
		total += nwritten;
	}
	
	return total;
}
