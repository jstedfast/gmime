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


#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <string.h>
#include "gmime-stream-buffer.h"

#define BLOCK_BUFFER_LEN   4096
#define BUFFER_GROW_SIZE   1024

static void stream_destroy (GMimeStream *stream);
static ssize_t stream_read (GMimeStream *stream, char *buf, size_t len);
static ssize_t stream_write (GMimeStream *stream, char *buf, size_t len);
static int stream_flush (GMimeStream *stream);
static int stream_close (GMimeStream *stream);
static gboolean stream_eos (GMimeStream *stream);
static int stream_reset (GMimeStream *stream);
static off_t stream_seek (GMimeStream *stream, off_t offset, GMimeSeekWhence whence);
static off_t stream_tell (GMimeStream *stream);
static ssize_t stream_length (GMimeStream *stream);
static GMimeStream *stream_substream (GMimeStream *stream, off_t start, off_t end);

static GMimeStream stream_template = {
	NULL, 0,
	1, 0, 0, 0, stream_destroy,
	stream_read, stream_write,
	stream_flush, stream_close,
	stream_eos, stream_reset,
	stream_seek, stream_tell,
	stream_length, stream_substream,
};

static void
stream_destroy (GMimeStream *stream)
{
	GMimeStreamBuffer *buffer = (GMimeStreamBuffer *) stream;
	
	if (buffer->source)
		g_mime_stream_unref (buffer->source);
	
	if (buffer->buffer)
		g_free (buffer->buffer);
	
	g_free (buffer);
}

static ssize_t
stream_read (GMimeStream *stream, char *buf, size_t len)
{
	GMimeStreamBuffer *buffer = (GMimeStreamBuffer *) stream;
	ssize_t n, nread = 0;
	
 again:
	switch (buffer->mode) {
	case GMIME_STREAM_BUFFER_BLOCK_READ:
		n = MIN (buffer->buflen, len);
		if (n > 0) {
			memcpy (buf + nread, buffer->buffer, n);
			buffer->buflen -= n;
			memmove (buffer->buffer, buffer->buffer + n, buffer->buflen);
			nread += n;
			len -= n;
		}
		
		if (buffer->buflen == 0) {
			/* buffer more data */
			buffer->buflen = g_mime_stream_read (buffer->source, buffer->buffer,
							     BLOCK_BUFFER_LEN);
			if (len && buffer->buflen > 0)
				goto again;
			
			if (buffer->buflen == -1) {
				if (nread == 0)
					return -1;
				else
					buffer->buflen = 0;
			}
		}
		break;
	case GMIME_STREAM_BUFFER_CACHE_READ:
		n = MIN (buffer->bufend - buffer->bufptr, len);
		if (n > 0) {
			memcpy (buf + nread, buffer->bufptr, n);
			buffer->bufptr += n;
			nread += n;
			len -= n;
		}
		
		if (len) {
			/* we need to read more data... */
			unsigned int offset = buffer->bufptr - buffer->buffer;
			
			buffer->buflen = buffer->bufend - buffer->buffer + MAX (BUFFER_GROW_SIZE, len);
			buffer->buffer = g_realloc (buffer->buffer, buffer->buflen);
			buffer->bufend = buffer->buffer + buffer->buflen;
			buffer->bufptr = buffer->buffer + offset;
			
			n = g_mime_stream_read (buffer->source, buffer->bufptr,
						buffer->bufend - buffer->bufptr);
			
			buffer->bufend = n > 0 ? buffer->bufptr + n : buffer->bufptr;
			
			if (n > 0)
				goto again;
		}
		break;
	default:
		nread = g_mime_stream_read (buffer->source, buf, len);
	}
	
	if (nread != -1)
		stream->position += nread;
	
	return nread;
}

static ssize_t
stream_write (GMimeStream *stream, char *buf, size_t len)
{
	GMimeStreamBuffer *buffer = (GMimeStreamBuffer *) stream;
	ssize_t written = 0, n;
	
 again:
	switch (buffer->mode) {
	case GMIME_STREAM_BUFFER_BLOCK_WRITE:
		n = MIN (BLOCK_BUFFER_LEN - buffer->buflen, len);
		memcpy (buffer->buffer + buffer->buflen, buf, n);
		buffer->buflen += n;
		written += n;
		len -= n;
		if (len) {
			/* flush our buffer... */
			n = g_mime_stream_write (buffer->source, buffer->buffer, BLOCK_BUFFER_LEN);
			if (n > 0) {
				memmove (buffer->buffer, buffer->buffer + n, BLOCK_BUFFER_LEN - n);
				goto again;
			}
		}
		break;
	default:
		written = g_mime_stream_write (buffer->source, buf, len);
	}
	
	if (written != -1)
		stream->position += written;
	
	return written;
}

static int
stream_flush (GMimeStream *stream)
{
	GMimeStreamBuffer *buffer = (GMimeStreamBuffer *) stream;
	
	if (buffer->mode == GMIME_STREAM_BUFFER_BLOCK_WRITE) {
		ssize_t written = 0;
		
		if (!buffer->buflen > 0) {
			written = g_mime_stream_write (buffer->source, buffer->buffer, buffer->buflen);
			if (written > 0)
				buffer->buflen -= written;
			
			if (buffer->buflen != 0)
				return -1;
		}
	}
	
	return g_mime_stream_flush (buffer->source);
}

static int
stream_close (GMimeStream *stream)
{
	GMimeStreamBuffer *buffer = (GMimeStreamBuffer *) stream;
	
	g_free (buffer->buffer);
	buffer->buffer = NULL;
	
	return g_mime_stream_close (buffer->source);
}

static gboolean
stream_eos (GMimeStream *stream)
{
	GMimeStreamBuffer *buffer = (GMimeStreamBuffer *) stream;
	gboolean eos;
	
	eos = g_mime_stream_eos (buffer->source);
	
	if (eos) {
		switch (buffer->mode) {
		case GMIME_STREAM_BUFFER_BLOCK_READ:
			return buffer->buflen == 0;
		case GMIME_STREAM_BUFFER_CACHE_READ:
			return buffer->bufptr == buffer->bufend;
		default:
			break;
		}
	}
	
	return eos;
}

static int
stream_reset (GMimeStream *stream)
{
	GMimeStreamBuffer *buffer = (GMimeStreamBuffer *) stream;
	int reset;
	
	switch (buffer->mode) {
	case GMIME_STREAM_BUFFER_BLOCK_READ:
		reset = g_mime_stream_reset (buffer->source);
		if (reset == -1)
			return -1;
		
		buffer->buflen = 0;
		break;
	case GMIME_STREAM_BUFFER_CACHE_READ:
		buffer->bufptr = buffer->buffer;
		break;
	default:
		reset = g_mime_stream_reset (buffer->source);
		if (reset == -1)
			return -1;
		
		break;
	}
	
	stream->position = stream->bound_start;
	
	return 0;
}

static off_t
stream_seek (GMimeStream *stream, off_t offset, GMimeSeekWhence whence)
{
	/* FIXME: implement me */
	/*xGMimeStreamBuffer *buffer = (GMimeStreamBuffer *) stream;*/
	
	return -1;
}

static off_t
stream_tell (GMimeStream *stream)
{
	return stream->position;
}

static ssize_t
stream_length (GMimeStream *stream)
{
	return g_mime_stream_length (GMIME_STREAM_BUFFER (stream)->source);
}

static GMimeStream *
stream_substream (GMimeStream *stream, off_t start, off_t end)
{
	GMimeStreamBuffer *buffer = (GMimeStreamBuffer *) stream;
	
	/* FIXME: for cached reads we want to substream ourself rather
           than substreaming our source because we have to assume that
           the reason this stream is setup to do cached reads is
           because the source streem is unseekable. */
	
	return buffer->source->substream (buffer->source, start, end);
}


/**
 * g_mime_stream_buffer_new:
 * @source: source stream
 * @mode: buffering mode
 *
 * Returns a new buffer stream with source @source and mode @mode.
 **/
GMimeStream *
g_mime_stream_buffer_new (GMimeStream *source, GMimeStreamBufferMode mode)
{
	GMimeStreamBuffer *buffer;
	
	g_return_val_if_fail (source != NULL, NULL);
	
	buffer = g_new (GMimeStreamBuffer, 1);
	
	buffer->source = source;
	g_mime_stream_ref (source);
	
	buffer->mode = mode;
	
	switch (buffer->mode) {
	case GMIME_STREAM_BUFFER_BLOCK_READ:
	case GMIME_STREAM_BUFFER_BLOCK_WRITE:
		buffer->buffer = g_malloc (BLOCK_BUFFER_LEN);
		buffer->bufptr = NULL;
		buffer->bufend = NULL;
		buffer->buflen = 0;
		break;
	default:
		buffer->buffer = g_malloc (BUFFER_GROW_SIZE);
		buffer->bufptr = buffer->buffer;
		buffer->bufend = buffer->buffer;
		buffer->buflen = BUFFER_GROW_SIZE;
	}
	
	g_mime_stream_construct (GMIME_STREAM (buffer), &stream_template,
				 GMIME_STREAM_BUFFER_TYPE,
				 source->bound_start,
				 source->bound_end);
	
	return GMIME_STREAM (buffer);
}


/**
 * g_mime_stream_buffer_gets:
 * @stream: stream
 * @buf: line buffer
 * @max: max length of a line
 *
 * Reads in at most one less than @max characters from @stream and
 * stores them into the buffer pointed to by @buf. Reading stops after
 * an EOS or newline (#'\n'). If a newline is read, it is stored into
 * the buffer. A #'\0' is stored after the last character in the
 * buffer.
 *
 * Returns the number of characters read into @buf on success and -1
 * on fail.
 **/
ssize_t
g_mime_stream_buffer_gets (GMimeStream *stream, char *buf, size_t max)
{
	char *outptr, *outend, *inptr, *inend, c = '\0';
	ssize_t nread;
	
	g_return_val_if_fail (stream != NULL, -1);
	
	outptr = buf;
	outend = buf + max - 1;
	
	if (GMIME_IS_STREAM_BUFFER (stream)) {
		GMimeStreamBuffer *buffer = GMIME_STREAM_BUFFER (stream);
		
	again:
		switch (buffer->mode) {
		case GMIME_STREAM_BUFFER_BLOCK_READ:
			inptr = buffer->buffer;
			inend = buffer->buffer + buffer->buflen;
			while (outptr < outend && inptr < inend && *inptr != '\n')
				c = *outptr++ = *inptr++;
			
			if (outptr < outend && inptr < inend && c != '\n')
				c = *outptr++ = *inptr++;
			
			memmove (buffer->buffer, inptr, inend - inptr);
			buffer->buflen = inend - inptr;
			
			if (c == '\n')
				break;
			
			if (buffer->buflen == 0) {
				/* buffer more data */
				buffer->buflen = g_mime_stream_read (buffer->source, buffer->buffer,
								     BLOCK_BUFFER_LEN);
				if (buffer->buflen <= 0) {
					buffer->buflen = 0;
					break;
				}
				
				if (outptr < outend)
					goto again;
			}
			
			break;
		case GMIME_STREAM_BUFFER_CACHE_READ:
			inptr = buffer->bufptr;
			inend = buffer->bufend;
			while (outptr < outend && inptr < inend && *inptr != '\n')
				c = *outptr++ = *inptr++;
			
			if (outptr < outend && inptr < inend && c != '\n')
				c = *outptr++ = *inptr++;
			
			buffer->bufptr = inptr;
			
			if (c == '\n')
				break;
			
			if (inptr == inend && outptr < outend) {
				/* buffer more data */
				unsigned int offset = buffer->bufptr - buffer->buffer;
				
				buffer->buflen = buffer->bufend - buffer->buffer + MAX (BUFFER_GROW_SIZE,
											outend - outptr + 1);
				buffer->buffer = g_realloc (buffer->buffer, buffer->buflen);
				buffer->bufend = buffer->buffer + buffer->buflen;
				buffer->bufptr = buffer->buffer + offset;
				nread = g_mime_stream_read (buffer->source, buffer->bufptr,
							    buffer->bufend - buffer->bufptr);
				
				buffer->bufend = nread >= 0 ? buffer->bufptr + nread : buffer->bufptr;
				
				if (nread <= 0)
					break;
				
				goto again;
			}
			break;
		default:
			goto slow_and_painful;
			break;
		}
		
		/* increment our stream position pointer */
		stream->position += (outptr - buf);
		
	} else {
		/* ugh...do it the slow and painful way... */
	slow_and_painful:
		while (outptr < outend && c != '\n' && (nread = g_mime_stream_read (stream, &c, 1)) == 1)
			*outptr++ = c;
	}
	
	if (outptr <= outend) {
		/* this should always be true unless @max == 0 */
		*outptr = '\0';
	}
	
	return (outptr - buf);
}


/**
 * g_mime_stream_buffer_readln:
 * @stream:
 * @buffer:
 *
 * Reads a single line into @buffer.
 **/
void
g_mime_stream_buffer_readln (GMimeStream *stream, GByteArray *buffer)
{
	char linebuf[1024];
	ssize_t len;
	
	while (!g_mime_stream_eos (stream)) {
		len = g_mime_stream_buffer_gets (stream, linebuf, sizeof (linebuf));
		if (len <= 0)
			break;
		
		g_byte_array_append (buffer, linebuf, len);
		if (linebuf[len - 1] == '\n')
			break;
	}
}
