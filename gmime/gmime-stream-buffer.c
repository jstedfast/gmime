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


#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <string.h>
#include <errno.h>

#include "gmime-stream-buffer.h"

#include "gmime-object-private.h"

/**
 * SECTION: gmime-stream-buffer
 * @title: GMimeStreamBuffer
 * @short_description: A buffered stream
 * @see_also: #GMimeStream
 *
 * A #GMimeStreamBuffer can be used on top of any other type of stream
 * and has 3 modes: block reads, block writes, and cached reads. Block
 * reads are especially useful if you will be making a lot of small
 * reads from a stream that accesses the file system. Block writes are
 * useful for very much the same reason. The final mode, cached reads,
 * can become memory intensive but can be very helpful when inheriting
 * from a stream that does not support seeking (Note: this mode is the
 * least tested so be careful using it).
 **/

#define BLOCK_BUFFER_LEN   4096

static void g_mime_stream_buffer_class_init (GMimeStreamBufferClass *klass);
static void g_mime_stream_buffer_init (GMimeStreamBuffer *stream);
static void g_mime_stream_buffer_finalize (GObject *object);

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


static GMimeStreamClass *parent_class = NULL;



G_DEFINE_TYPE_WITH_PRIVATE (GMimeStreamBuffer, g_mime_stream_buffer, GMIME_TYPE_STREAM)

GMimeStreamBufferPrivate *
_g_mime_stream_buffer_get_private (GMimeStreamBuffer *object)
{
	return g_mime_stream_buffer_get_instance_private (object);
}


static void
g_mime_stream_buffer_class_init (GMimeStreamBufferClass *klass)
{
	GMimeStreamClass *stream_class = GMIME_STREAM_CLASS (klass);
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	
	parent_class = g_type_class_ref (GMIME_TYPE_STREAM);
	
	object_class->finalize = g_mime_stream_buffer_finalize;
	
	stream_class->read = stream_read;
	stream_class->write = stream_write;
	stream_class->flush = stream_flush;
	stream_class->close = stream_close;
	stream_class->eos = stream_eos;
	stream_class->reset = stream_reset;
	stream_class->seek = stream_seek;
	stream_class->tell = stream_tell;
	stream_class->length = stream_length;
	stream_class->substream = stream_substream;
}

static void
g_mime_stream_buffer_init (GMimeStreamBuffer *stream)
{
	GMimeStreamBufferPrivate *priv = _g_mime_stream_buffer_get_private (stream);

	priv->source = NULL;
	priv->buffer = NULL;
	priv->bufptr = NULL;
	priv->bufend = NULL;
	priv->buflen = 0;
	priv->mode = 0;
}

static void
g_mime_stream_buffer_finalize (GObject *object)
{
	GMimeStream *stream = (GMimeStream *) object;
	
	stream_close (stream);
	
	G_OBJECT_CLASS (parent_class)->finalize (object);
}


static ssize_t
stream_read (GMimeStream *stream, char *buf, size_t len)
{
	GMimeStreamBuffer *buffer = (GMimeStreamBuffer *) stream;
	GMimeStreamBufferPrivate *buffer_priv = _g_mime_stream_buffer_get_private (buffer);
	GMimeStreamPrivate *stream_priv;
	ssize_t n, nread = 0;

	if (buffer_priv->source == NULL) {
		errno = EBADF;
		return -1;
	}
	
	if (buffer_priv->mode == GMIME_STREAM_BUFFER_BLOCK_READ) {
		while (len > 0) {
			/* consume what we can from any pre-buffered data we have left */
			if ((n = MIN (buffer_priv->buflen, len)) > 0) {
				memcpy (buf + nread, buffer_priv->bufptr, n);
				buffer_priv->bufptr += n;
				buffer_priv->buflen -= n;
				nread += n;
				len -= n;
			}
			
			if (len >= BLOCK_BUFFER_LEN) {
				/* bypass intermediate buffer, read straight from disk */
				buffer_priv->bufptr = buffer_priv->buffer;
				if ((n = g_mime_stream_read (buffer_priv->source, buf + nread, len)) > 0) {
					nread += n;
					len -= n;
				}
				
				break;
			} else if (len > 0) {
				/* buffer more data */
				if ((n = g_mime_stream_read (buffer_priv->source, buffer_priv->buffer, BLOCK_BUFFER_LEN)) > 0)
					buffer_priv->buflen = n;
				
				buffer_priv->bufptr = buffer_priv->buffer;
			}
			
			if (n <= 0) {
				if (nread == 0)
					return n;
				
				break;
			}
		}
	} else {
		if ((nread = g_mime_stream_read (buffer_priv->source, buf, len)) == -1)
			return -1;
	}
	
	stream_priv = _g_mime_stream_get_private (stream);
	stream_priv->position += nread;
	
	return nread;
}

static ssize_t
stream_write (GMimeStream *stream, const char *buf, size_t len)
{
	GMimeStreamBuffer *buffer = (GMimeStreamBuffer *) stream;
	GMimeStreamBufferPrivate *buffer_priv = _g_mime_stream_buffer_get_private (buffer);
	GMimeStream *source = buffer_priv->source;
	GMimeStreamPrivate *stream_priv;
	ssize_t n, nwritten = 0;
	size_t left = len;
	
	if (buffer_priv->source == NULL) {
		errno = EBADF;
		return -1;
	}
	
	if (buffer_priv->mode == GMIME_STREAM_BUFFER_BLOCK_WRITE) {
		while (left > 0) {
			n = MIN (BLOCK_BUFFER_LEN - buffer_priv->buflen, left);
			if (buffer_priv->buflen > 0 || n < BLOCK_BUFFER_LEN) {
				/* add the data to our pending write buffer */
				memcpy (buffer_priv->bufptr, buf + nwritten, n);
				buffer_priv->bufptr += n;
				buffer_priv->buflen += n;
				nwritten += n;
				left -= n;
			}
			
			if (buffer_priv->buflen == BLOCK_BUFFER_LEN) {
				/* flush our buffer... */
				n = g_mime_stream_write (source, buffer_priv->buffer, BLOCK_BUFFER_LEN);
				if (n == BLOCK_BUFFER_LEN) {
					/* wrote everything... */
					buffer_priv->bufptr = buffer_priv->buffer;
					buffer_priv->buflen = 0;
				} else if (n > 0) {
					/* still have buffered data left... */
					memmove (buffer_priv->buffer, buffer_priv->buffer + n, BLOCK_BUFFER_LEN - n);
					buffer_priv->bufptr -= n;
					buffer_priv->buflen -= n;
				} else if (n == -1) {
					if (nwritten == 0)
						return -1;
					
					break;
				}
			}
			
			if (buffer_priv->buflen == 0 && left >= BLOCK_BUFFER_LEN) {
				while (left >= BLOCK_BUFFER_LEN) {
					if ((n = g_mime_stream_write (source, buf + nwritten, BLOCK_BUFFER_LEN)) == -1) {
						if (nwritten == 0)
							return -1;
						
						break;
					}
					
					nwritten += n;
					left -= n;
				}
				
				if (left >= BLOCK_BUFFER_LEN)
					break;
			}
		}
	} else {
		if ((nwritten = g_mime_stream_write (source, buf, len)) == -1)
			return -1;
	}
	
	stream_priv = _g_mime_stream_get_private (stream);
	stream_priv->position += nwritten;
	
	return nwritten;
}

static int
stream_flush (GMimeStream *stream)
{
	GMimeStreamBuffer *buffer = (GMimeStreamBuffer *) stream;
	GMimeStreamBufferPrivate *priv = _g_mime_stream_buffer_get_private (buffer);

	if (priv->mode == GMIME_STREAM_BUFFER_BLOCK_WRITE && priv->buflen > 0) {
		ssize_t written = 0;
		
		written = g_mime_stream_write (priv->source, priv->buffer, priv->buflen);
		if (written > 0) {
			memmove (priv->buffer, priv->buffer + written, priv->buflen - written);
			priv->bufptr -= written;
			priv->buflen -= written;
		}
		
		if (priv->buflen != 0)
			return -1;
	}
	
	return g_mime_stream_flush (priv->source);
}

static int
stream_close (GMimeStream *stream)
{
	GMimeStreamBuffer *buffer = (GMimeStreamBuffer *) stream;
	GMimeStreamBufferPrivate *priv = _g_mime_stream_buffer_get_private (buffer);

	if (priv->source == NULL)
		return 0;
	
	g_object_unref (priv->source);
	priv->source = NULL;
	
	g_free (priv->buffer);
	priv->buffer = NULL;
	priv->bufptr = NULL;
	priv->bufend = NULL;
	priv->buflen = 0;
	
	return 0;
}

static gboolean
stream_eos (GMimeStream *stream)
{
	GMimeStreamBuffer *buffer = (GMimeStreamBuffer *) stream;
	GMimeStreamBufferPrivate *priv = _g_mime_stream_buffer_get_private (buffer);

	if (priv->source == NULL)
		return TRUE;
	
	if (!g_mime_stream_eos (priv->source))
		return FALSE;
	
	if (priv->mode == GMIME_STREAM_BUFFER_BLOCK_READ)
		return priv->buflen == 0;
	
	return TRUE;
}

static int
stream_reset (GMimeStream *stream)
{
	GMimeStreamBuffer *buffer = (GMimeStreamBuffer *) stream;
	GMimeStreamBufferPrivate *priv = _g_mime_stream_buffer_get_private (buffer);

	if (priv->source == NULL) {
		errno = EBADF;
		return -1;
	}
	
	if (g_mime_stream_reset (priv->source) == -1)
		return -1;
	
	priv->bufptr = priv->buffer;
	priv->buflen = 0;
	
	return 0;
}

static gint64
stream_seek_block_read (GMimeStream *stream, gint64 offset, GMimeSeekWhence whence)
{
	GMimeStreamBuffer *buffer = (GMimeStreamBuffer *) stream;
	GMimeStreamPrivate *stream_priv = _g_mime_stream_get_private (stream);
	GMimeStreamBufferPrivate *buffer_priv = _g_mime_stream_buffer_get_private (buffer);
	gint64 real;
	
	/* convert all seeks into a relative seek (aka SEEK_CUR) */
	switch (whence) {
	case GMIME_STREAM_SEEK_CUR:
		/* no-op for now */
		break;
	case GMIME_STREAM_SEEK_SET:
		if (stream_priv->position == offset)
			return stream_priv->position;
		
		if (offset < 0) {
			/* not allowed to seek to a negative position */
			errno = EINVAL;
			return -1;
		}
		
		offset -= stream_priv->position;
		break;
	case GMIME_STREAM_SEEK_END:
		if (stream_priv->bound_end == -1) {
			/* gotta do this the slow way */
			if ((real = g_mime_stream_seek (buffer_priv->source, offset, GMIME_STREAM_SEEK_END)) == -1)
				return -1;
			
			stream_priv->position = real;
			
			buffer_priv->bufptr = buffer_priv->buffer;
			buffer_priv->buflen = 0;
			
			return real;
		}
		
		if (offset > 0) {
			/* not allowed to seek past bound_end */
			errno = EINVAL;
			return -1;
		}
		
		offset += stream_priv->bound_end;
		break;
	default:
		/* invalid whence argument */
		errno = EINVAL;
		return -1;
	}
	
	/* now that offset is relative to our current position... */
	
	if (offset == 0)
		return stream_priv->position;
	
	if ((offset < 0 && offset >= (buffer_priv->buffer - buffer_priv->bufptr))
	    || (offset > 0 && offset <= (gint64) buffer_priv->buflen)) {
		/* the position is within our pre-buffered region */
		stream_priv->position += offset;
		buffer_priv->bufptr += (size_t) offset;
		buffer_priv->buflen -= (size_t) offset;
		
		return stream_priv->position;
	}
	
	/* we are now forced to do an actual seek */
	offset += stream_priv->position;
	if ((real = g_mime_stream_seek (buffer_priv->source, offset, GMIME_STREAM_SEEK_SET)) == -1)
		return -1;
	
	stream_priv->position = real;
	
	buffer_priv->bufptr = buffer_priv->buffer;
	buffer_priv->buflen = 0;
	
	return real;
}

static gint64
stream_seek (GMimeStream *stream, gint64 offset, GMimeSeekWhence whence)
{
	GMimeStreamBuffer *buffer = (GMimeStreamBuffer *) stream;
	GMimeStreamBufferPrivate *buffer_priv = _g_mime_stream_buffer_get_private (buffer);
	gint64 real;

	if (buffer_priv->source == NULL) {
		errno = EBADF;
		return -1;
	}
	
	if (buffer_priv->mode == GMIME_STREAM_BUFFER_BLOCK_READ)
		return stream_seek_block_read (stream, offset, whence);
	
	if (stream_flush (stream) != 0)
		return -1;
	
	if ((real = g_mime_stream_seek (buffer_priv->source, offset, whence)) != -1) {
		GMimeStreamPrivate *stream_priv = _g_mime_stream_get_private (stream);

		stream_priv->position = real;
		buffer_priv->buflen = 0;
	}
	
	return real;
}

static gint64
stream_tell (GMimeStream *stream)
{
	GMimeStreamPrivate *stream_priv;
	GMimeStreamBuffer *buffer = (GMimeStreamBuffer *) stream;
	GMimeStreamBufferPrivate *buffer_priv = _g_mime_stream_buffer_get_private (buffer);

	if (buffer_priv->source == NULL) {
		errno = EBADF;
		return -1;
	}
	
	stream_priv = _g_mime_stream_get_private (stream);

	return stream_priv->position;
}

static gint64
stream_length (GMimeStream *stream)
{
	GMimeStreamBuffer *buffer = (GMimeStreamBuffer *) stream;
	GMimeStreamBufferPrivate *priv = _g_mime_stream_buffer_get_private (buffer);

	if (priv->source == NULL) {
		errno = EBADF;
		return -1;
	}
	
	return g_mime_stream_length (priv->source);
}

static GMimeStream *
stream_substream (GMimeStream *stream, gint64 start, gint64 end)
{
	GMimeStreamBuffer *buffer = (GMimeStreamBuffer *) stream;
	GMimeStreamBufferPrivate *priv = _g_mime_stream_buffer_get_private (buffer);

	/* substream our source */
	return GMIME_STREAM_GET_CLASS (priv->source)->substream (priv->source, start, end);
}


/**
 * g_mime_stream_buffer_new:
 * @source: source stream
 * @mode: buffering mode
 *
 * Creates a new GMimeStreamBuffer object.
 *
 * Returns: a new buffer stream with source @source and mode @mode.
 **/
GMimeStream *
g_mime_stream_buffer_new (GMimeStream *source, GMimeStreamBufferMode mode)
{
	GMimeStreamBufferPrivate *priv;
	GMimeStreamPrivate *source_priv;
	GMimeStreamBuffer *buffer;
	
	g_return_val_if_fail (GMIME_IS_STREAM (source), NULL);
	
	buffer = g_object_new (GMIME_TYPE_STREAM_BUFFER, NULL);
	priv = _g_mime_stream_buffer_get_private (buffer);

	priv->source = source;
	g_object_ref (source);
	
	priv->mode = mode;
	priv->buffer = g_malloc (BLOCK_BUFFER_LEN);
	priv->bufend = priv->buffer + BLOCK_BUFFER_LEN;
	priv->bufptr = priv->buffer;
	priv->buflen = 0;
	
	source_priv = _g_mime_stream_get_private (source);

	g_mime_stream_construct ((GMimeStream *) buffer,
				 source_priv->bound_start,
				 source_priv->bound_end);
	
	return (GMimeStream *) buffer;
}


/**
 * g_mime_stream_buffer_gets:
 * @stream: stream
 * @buf: (array length=max) (element-type guint8): line buffer
 * @max: max length of a line
 *
 * Reads in at most one less than @max characters from @stream and
 * stores them into the buffer pointed to by @buf. Reading stops after
 * an EOS or newline ('\n'). If a newline is read, it is stored into
 * the buffer. A '\0' is stored after the last character in the
 * buffer.
 *
 * Returns: the number of characters read into @buf on success or %-1
 * on fail.
 **/
ssize_t
g_mime_stream_buffer_gets (GMimeStream *stream, char *buf, size_t max)
{
	register char *inptr, *outptr;
	char *inend, *outend;
	ssize_t nread, n;
	char c = '\0';
	
	g_return_val_if_fail (GMIME_IS_STREAM (stream), -1);
	
	outptr = buf;
	outend = buf + max - 1;
	
	if (GMIME_IS_STREAM_BUFFER (stream)) {
		GMimeStreamBuffer *buffer = (GMimeStreamBuffer *) stream;
		GMimeStreamBufferPrivate *buffer_priv = _g_mime_stream_buffer_get_private (buffer);

		if (buffer_priv->mode == GMIME_STREAM_BUFFER_BLOCK_READ) {
			GMimeStreamPrivate *stream_priv;

			while (outptr < outend) {
				inptr = buffer_priv->bufptr;
				inend = inptr + buffer_priv->buflen;
				
				while (outptr < outend && inptr < inend && *inptr != '\n')
					c = *outptr++ = *inptr++;
				
				if (outptr < outend && inptr < inend && c != '\n')
					c = *outptr++ = *inptr++;
				
				buffer_priv->buflen = inend - inptr;
				buffer_priv->bufptr = inptr;
				
				if (c == '\n')
					break;
				
				if (buffer_priv->buflen == 0) {
					/* buffer more data */
					buffer_priv->bufptr = buffer_priv->buffer;
					n = g_mime_stream_read (buffer_priv->source, buffer_priv->buffer, BLOCK_BUFFER_LEN);
					if (n <= 0)
						break;
					
					buffer_priv->buflen = n;
				}
			}
			
			/* increment our stream position pointer */
			stream_priv = _g_mime_stream_get_private (stream);
			stream_priv->position += (outptr - buf);
		} else {
			goto slow_and_painful;
		}
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
	
	return (ssize_t) (outptr - buf);
}


/**
 * g_mime_stream_buffer_readln:
 * @stream: stream
 * @buffer: output buffer
 *
 * Reads a single line into @buffer.
 **/
void
g_mime_stream_buffer_readln (GMimeStream *stream, GByteArray *buffer)
{
	char linebuf[1024];
	ssize_t len;
	
	g_return_if_fail (GMIME_IS_STREAM (stream));
	
	while (!g_mime_stream_eos (stream)) {
		if ((len = g_mime_stream_buffer_gets (stream, linebuf, sizeof (linebuf))) <= 0)
			break;
		
		if (buffer)
			g_byte_array_append (buffer, (unsigned char *) linebuf, len);
		
		if (linebuf[len - 1] == '\n')
			break;
	}
}
