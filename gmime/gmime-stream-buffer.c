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
#include <errno.h>

#include "gmime-stream-buffer.h"

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
#define BUFFER_GROW_SIZE   1024  /* should this also be 4k? */

static void g_mime_stream_buffer_class_init (GMimeStreamBufferClass *klass);
static void g_mime_stream_buffer_init (GMimeStreamBuffer *stream, GMimeStreamBufferClass *klass);
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


GType
g_mime_stream_buffer_get_type (void)
{
	static GType type = 0;
	
	if (!type) {
		static const GTypeInfo info = {
			sizeof (GMimeStreamBufferClass),
			NULL, /* base_class_init */
			NULL, /* base_class_finalize */
			(GClassInitFunc) g_mime_stream_buffer_class_init,
			NULL, /* class_finalize */
			NULL, /* class_data */
			sizeof (GMimeStreamBuffer),
			0,    /* n_preallocs */
			(GInstanceInitFunc) g_mime_stream_buffer_init,
		};
		
		type = g_type_register_static (GMIME_TYPE_STREAM, "GMimeStreamBuffer", &info, 0);
	}
	
	return type;
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
g_mime_stream_buffer_init (GMimeStreamBuffer *stream, GMimeStreamBufferClass *klass)
{
	stream->source = NULL;
	stream->buffer = NULL;
	stream->bufptr = NULL;
	stream->bufend = NULL;
	stream->buflen = 0;
	stream->mode = 0;
}

static void
g_mime_stream_buffer_finalize (GObject *object)
{
	GMimeStreamBuffer *stream = (GMimeStreamBuffer *) object;
	
	if (stream->source)
		g_object_unref (stream->source);
	
	g_free (stream->buffer);
	
	G_OBJECT_CLASS (parent_class)->finalize (object);
}


static ssize_t
stream_read (GMimeStream *stream, char *buf, size_t len)
{
	GMimeStreamBuffer *buffer = (GMimeStreamBuffer *) stream;
	size_t buflen, offset;
	ssize_t n, nread = 0;
	
	if (buffer->source == NULL) {
		errno = EBADF;
		return -1;
	}
	
	switch (buffer->mode) {
	case GMIME_STREAM_BUFFER_BLOCK_READ:
		while (len > 0) {
			/* consume what we can from any pre-buffered data we have left */
			if ((n = MIN (buffer->buflen, len)) > 0) {
				memcpy (buf + nread, buffer->bufptr, n);
				buffer->bufptr += n;
				buffer->buflen -= n;
				nread += n;
				len -= n;
			}
			
			if (len >= BLOCK_BUFFER_LEN) {
				/* bypass intermediate buffer, read straight from disk */
				buffer->bufptr = buffer->buffer;
				if ((n = g_mime_stream_read (buffer->source, buf + nread, len)) > 0) {
					nread += n;
					len -= n;
				}
				
				break;
			} else if (len > 0) {
				/* buffer more data */
				if ((n = g_mime_stream_read (buffer->source, buffer->buffer, BLOCK_BUFFER_LEN)) > 0)
					buffer->buflen = n;
				
				buffer->bufptr = buffer->buffer;
			}
			
			if (n <= 0) {
				if (nread == 0)
					return n;
				
				break;
			}
		}
		break;
	case GMIME_STREAM_BUFFER_CACHE_READ:
		while (len > 0) {
			buflen = (size_t) (buffer->bufend - buffer->bufptr);
			if ((n = MIN (buflen, len)) > 0) {
				memcpy (buf + nread, buffer->bufptr, n);
				buffer->bufptr += n;
				nread += n;
				len -= n;
			}
			
			if (len > 0) {
				/* we need to read more data... */
				offset = buffer->bufptr - buffer->buffer;
				
				buffer->buflen = buffer->bufend - buffer->buffer + MAX (BUFFER_GROW_SIZE, len);
				buffer->buffer = g_realloc (buffer->buffer, buffer->buflen);
				buffer->bufend = buffer->buffer + buffer->buflen;
				buffer->bufptr = buffer->buffer + offset;
				
				n = g_mime_stream_read (buffer->source, buffer->bufptr,
							buffer->bufend - buffer->bufptr);
				
				buffer->bufend = n > 0 ? buffer->bufptr + n : buffer->bufptr;
				
				if (n <= 0) {
					if (nread == 0)
						return n;
					
					break;
				}
			}
		}
		break;
	default:
		if ((nread = g_mime_stream_read (buffer->source, buf, len)) == -1)
			return -1;
		
		break;
	}
	
	stream->position += nread;
	
	return nread;
}

static ssize_t
stream_write (GMimeStream *stream, const char *buf, size_t len)
{
	GMimeStreamBuffer *buffer = (GMimeStreamBuffer *) stream;
	GMimeStream *source = buffer->source;
	ssize_t n, nwritten = 0;
	size_t left = len;
	
	if (buffer->source == NULL) {
		errno = EBADF;
		return -1;
	}
	
	switch (buffer->mode) {
	case GMIME_STREAM_BUFFER_BLOCK_WRITE:
		while (left > 0) {
			n = MIN (BLOCK_BUFFER_LEN - buffer->buflen, left);
			if (buffer->buflen > 0 || n < BLOCK_BUFFER_LEN) {
				/* add the data to our pending write buffer */
				memcpy (buffer->bufptr, buf + nwritten, n);
				buffer->bufptr += n;
				buffer->buflen += n;
				nwritten += n;
				left -= n;
			}
			
			if (buffer->buflen == BLOCK_BUFFER_LEN) {
				/* flush our buffer... */
				n = g_mime_stream_write (source, buffer->buffer, BLOCK_BUFFER_LEN);
				if (n == BLOCK_BUFFER_LEN) {
					/* wrote everything... */
					buffer->bufptr = buffer->buffer;
					buffer->buflen = 0;
				} else if (n > 0) {
					/* still have buffered data left... */
					memmove (buffer->buffer, buffer->buffer + n, BLOCK_BUFFER_LEN - n);
					buffer->bufptr -= n;
					buffer->buflen -= n;
				} else if (n == -1) {
					if (nwritten == 0)
						return -1;
					
					break;
				}
			}
			
			if (buffer->buflen == 0 && left >= BLOCK_BUFFER_LEN) {
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
		break;
	default:
		if ((nwritten = g_mime_stream_write (source, buf, len)) == -1)
			return -1;
		
		break;
	}
	
	stream->position += nwritten;
	
	return nwritten;
}

static int
stream_flush (GMimeStream *stream)
{
	GMimeStreamBuffer *buffer = (GMimeStreamBuffer *) stream;
	
	if (buffer->mode == GMIME_STREAM_BUFFER_BLOCK_WRITE && buffer->buflen > 0) {
		ssize_t written = 0;
		
		written = g_mime_stream_write (buffer->source, buffer->buffer, buffer->buflen);
		if (written > 0) {
			memmove (buffer->buffer, buffer->buffer + written, buffer->buflen - written);
			buffer->bufptr -= written;
			buffer->buflen -= written;
		}
		
		if (buffer->buflen != 0)
			return -1;
	}
	
	return g_mime_stream_flush (buffer->source);
}

static int
stream_close (GMimeStream *stream)
{
	GMimeStreamBuffer *buffer = (GMimeStreamBuffer *) stream;
	
	if (buffer->source == NULL)
		return 0;
	
	g_mime_stream_close (buffer->source);
	g_object_unref (buffer->source);
	buffer->source = NULL;
	
	g_free (buffer->buffer);
	buffer->buffer = NULL;
	buffer->bufptr = NULL;
	buffer->bufend = NULL;
	buffer->buflen = 0;
	
	return 0;
}

static gboolean
stream_eos (GMimeStream *stream)
{
	GMimeStreamBuffer *buffer = (GMimeStreamBuffer *) stream;
	
	if (buffer->source == NULL)
		return TRUE;
	
	if (!g_mime_stream_eos (buffer->source))
		return FALSE;
	
	switch (buffer->mode) {
	case GMIME_STREAM_BUFFER_BLOCK_READ:
		return buffer->buflen == 0;
	case GMIME_STREAM_BUFFER_CACHE_READ:
		return buffer->bufptr == buffer->bufend;
	default:
		break;
	}
	
	return TRUE;
}

static int
stream_reset (GMimeStream *stream)
{
	GMimeStreamBuffer *buffer = (GMimeStreamBuffer *) stream;
	
	if (buffer->source == NULL) {
		errno = EBADF;
		return -1;
	}
	
	switch (buffer->mode) {
	case GMIME_STREAM_BUFFER_BLOCK_READ:
	case GMIME_STREAM_BUFFER_BLOCK_WRITE:
		if (g_mime_stream_reset (buffer->source) == -1)
			return -1;
		
		buffer->bufptr = buffer->buffer;
		buffer->buflen = 0;
		break;
	case GMIME_STREAM_BUFFER_CACHE_READ:
		buffer->bufptr = buffer->buffer;
		break;
	default:
		if (g_mime_stream_reset (buffer->source) == -1)
			return -1;
		
		break;
	}
	
	return 0;
}

static gint64
stream_seek_block_read (GMimeStream *stream, gint64 offset, GMimeSeekWhence whence)
{
	GMimeStreamBuffer *buffer = (GMimeStreamBuffer *) stream;
	gint64 real;
	
	/* convert all seeks into a relative seek (aka SEEK_CUR) */
	switch (whence) {
	case GMIME_STREAM_SEEK_CUR:
		/* no-op for now */
		break;
	case GMIME_STREAM_SEEK_SET:
		if (stream->position == offset)
			return stream->position;
		
		if (offset < 0) {
			/* not allowed to seek to a negative position */
			errno = EINVAL;
			return -1;
		}
		
		offset -= stream->position;
		break;
	case GMIME_STREAM_SEEK_END:
		if (stream->bound_end == -1) {
			/* gotta do this the slow way */
			if ((real = g_mime_stream_seek (buffer->source, offset, GMIME_STREAM_SEEK_END) == -1))
				return -1;
			
			stream->position = real;
			
			buffer->bufptr = buffer->buffer;
			buffer->buflen = 0;
			
			return real;
		}
		
		if (offset > 0) {
			/* not allowed to seek past bound_end */
			errno = EINVAL;
			return -1;
		}
		
		offset += stream->bound_end;
		break;
	default:
		/* invalid whence argument */
		errno = EINVAL;
		return -1;
	}
	
	/* now that offset is relative to our current position... */
	
	if (offset == 0)
		return stream->position;
	
	if ((offset < 0 && offset >= (buffer->buffer - buffer->bufptr))
	    || (offset > 0 && offset <= buffer->buflen)) {
		/* the position is within our pre-buffered region */
		stream->position += offset;
		buffer->bufptr += (size_t) offset;
		buffer->buflen -= (size_t) offset;
		
		return stream->position;
	}
	
	/* we are now forced to do an actual seek */
	offset += stream->position;
	if ((real = g_mime_stream_seek (buffer->source, offset, GMIME_STREAM_SEEK_SET)) == -1)
		return -1;
	
	stream->position = real;
	
	buffer->bufptr = buffer->buffer;
	buffer->buflen = 0;
	
	return real;
}

static gint64
stream_seek_cache_read (GMimeStream *stream, gint64 offset, GMimeSeekWhence whence)
{
	GMimeStreamBuffer *buffer = (GMimeStreamBuffer *) stream;
	gint64 buflen, len, total = 0;
	gint64 pos, real;
	ssize_t nread;
	
	switch (whence) {
	case GMIME_STREAM_SEEK_SET:
		real = offset;
		break;
	case GMIME_STREAM_SEEK_CUR:
		real = stream->position + offset;
		break;
	case GMIME_STREAM_SEEK_END:
		if (stream->bound_end == -1) {
			/* we have to do a real seek because the end boundary is unknown */
			if ((real = g_mime_stream_seek (buffer->source, offset, whence)) == -1)
				return -1;
			
			if (real < stream->bound_start) {
				/* seek offset out of bounds */
				errno = EINVAL;
				return -1;
			}
		} else {
			real = stream->bound_end + offset;
			if (real > stream->bound_end || real < stream->bound_start) {
				/* seek offset out of bounds */
				errno = EINVAL;
				return -1;
			}
		}
		break;
	default:
		/* invalid whence argument */
		errno = EINVAL;
		return -1;
	}
	
	if (real > stream->position) {
		/* buffer any data between position and real */
		len = real - (stream->bound_start + (buffer->bufend - buffer->bufptr));
		
		if (buffer->bufptr + len <= buffer->bufend) {
			buffer->bufptr += len;
			stream->position = real;
			return real;
		}
		
		pos = buffer->bufptr - buffer->buffer;
		
		buflen = (buffer->bufend - buffer->buffer) + len;
		if (buflen < G_MAXSIZE)
			buffer->buflen = (size_t) buflen;
		else
			buffer->buflen = G_MAXSIZE;
		
		buffer->buffer = g_realloc (buffer->buffer, buffer->buflen);
		buffer->bufend = buffer->buffer + buffer->buflen;
		buffer->bufptr = buffer->buffer + pos;
		
		do {
			nread = g_mime_stream_read (buffer->source, buffer->bufptr,
						    buffer->bufend - buffer->bufptr);
			if (nread > 0) {
				total += nread;
				buffer->bufptr += nread;
			}
		} while (nread != -1);
		
		buffer->bufend = buffer->bufptr;
		if (total < len) {
			/* we failed to seek that far so reset our bufptr */
			buffer->bufptr = buffer->buffer + pos;
			errno = EINVAL;
			return -1;
		}
	} else if (real < stream->bound_start) {
		/* seek offset out of bounds */
		errno = EINVAL;
		return -1;
	} else {
		/* seek our cache pointer backwards */
		buffer->bufptr = buffer->buffer + (real - stream->bound_start);
	}
	
	stream->position = real;
	
	return real;
}

static gint64
stream_seek (GMimeStream *stream, gint64 offset, GMimeSeekWhence whence)
{
	GMimeStreamBuffer *buffer = (GMimeStreamBuffer *) stream;
	gint64 real;
	
	if (buffer->source == NULL) {
		errno = EBADF;
		return -1;
	}
	
	switch (buffer->mode) {
	case GMIME_STREAM_BUFFER_BLOCK_WRITE:
		if (stream_flush (stream) != 0)
			return -1;
		
		if ((real = g_mime_stream_seek (buffer->source, offset, whence)) != -1) {
			stream->position = real;
			buffer->buflen = 0;
		}
		
		return real;
	case GMIME_STREAM_BUFFER_BLOCK_READ:
		return stream_seek_block_read (stream, offset, whence);
	case GMIME_STREAM_BUFFER_CACHE_READ:
		return stream_seek_cache_read (stream, offset, whence);
	default:
		/* invalid whence argument */
		errno = EINVAL;
		return -1;
	}
}

static gint64
stream_tell (GMimeStream *stream)
{
	GMimeStreamBuffer *buffer = (GMimeStreamBuffer *) stream;
	
	if (buffer->source == NULL) {
		errno = EBADF;
		return -1;
	}
	
	return stream->position;
}

static gint64
stream_length (GMimeStream *stream)
{
	GMimeStreamBuffer *buffer = (GMimeStreamBuffer *) stream;
	
	if (buffer->source == NULL) {
		errno = EBADF;
		return -1;
	}
	
	return g_mime_stream_length (buffer->source);
}

static GMimeStream *
stream_substream (GMimeStream *stream, gint64 start, gint64 end)
{
	GMimeStreamBuffer *buffer = (GMimeStreamBuffer *) stream;
	
	/* FIXME: for cached reads we want to substream ourself rather
           than substreaming our source because we have to assume that
           the reason this stream is setup to do cached reads is
           because the source stream is unseekable. */
	
	return GMIME_STREAM_GET_CLASS (buffer->source)->substream (buffer->source, start, end);
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
	GMimeStreamBuffer *buffer;
	
	g_return_val_if_fail (GMIME_IS_STREAM (source), NULL);
	
	buffer = g_object_newv (GMIME_TYPE_STREAM_BUFFER, 0, NULL);
	
	buffer->source = source;
	g_object_ref (source);
	
	buffer->mode = mode;
	
	switch (buffer->mode) {
	case GMIME_STREAM_BUFFER_BLOCK_READ:
	case GMIME_STREAM_BUFFER_BLOCK_WRITE:
		buffer->buffer = g_malloc (BLOCK_BUFFER_LEN);
		buffer->bufend = buffer->buffer + BLOCK_BUFFER_LEN;
		buffer->bufptr = buffer->buffer;
		buffer->buflen = 0;
		break;
	default:
		buffer->buffer = g_malloc (BUFFER_GROW_SIZE);
		buffer->bufptr = buffer->buffer;
		buffer->bufend = buffer->buffer;
		buffer->buflen = BUFFER_GROW_SIZE;
		break;
	}
	
	g_mime_stream_construct (GMIME_STREAM (buffer),
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
		GMimeStreamBuffer *buffer = GMIME_STREAM_BUFFER (stream);
		
		switch (buffer->mode) {
		case GMIME_STREAM_BUFFER_BLOCK_READ:
			while (outptr < outend) {
				inptr = buffer->bufptr;
				inend = inptr + buffer->buflen;
				
				while (outptr < outend && inptr < inend && *inptr != '\n')
					c = *outptr++ = *inptr++;
				
				if (outptr < outend && inptr < inend && c != '\n')
					c = *outptr++ = *inptr++;
				
				buffer->buflen = inend - inptr;
				buffer->bufptr = inptr;
				
				if (c == '\n')
					break;
				
				if (buffer->buflen == 0) {
					/* buffer more data */
					buffer->bufptr = buffer->buffer;
					n = g_mime_stream_read (buffer->source, buffer->buffer, BLOCK_BUFFER_LEN);
					if (n <= 0)
						break;
					
					buffer->buflen = n;
				}
			}
			break;
		case GMIME_STREAM_BUFFER_CACHE_READ:
			while (outptr < outend) {
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
					size_t offset = (size_t) (buffer->bufptr - buffer->buffer);
					
					buffer->buflen = buffer->bufend - buffer->buffer +
						MAX (BUFFER_GROW_SIZE, outend - outptr + 1);
					buffer->buffer = g_realloc (buffer->buffer, buffer->buflen);
					buffer->bufend = buffer->buffer + buffer->buflen;
					buffer->bufptr = buffer->buffer + offset;
					nread = g_mime_stream_read (buffer->source, buffer->bufptr,
								    buffer->bufend - buffer->bufptr);
					
					buffer->bufend = nread >= 0 ? buffer->bufptr + nread : buffer->bufptr;
					
					if (nread <= 0)
						break;
				}
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
