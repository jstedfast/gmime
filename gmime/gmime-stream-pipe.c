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

#include <glib.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>

#include "gmime-stream-pipe.h"


/**
 * SECTION: gmime-stream-pipe
 * @title: GMimeStreamPipe
 * @short_description: A low-level pipe stream
 * @see_also: #GMimeStream
 *
 * A simple #GMimeStream implementation that sits on top of low-level
 * POSIX pipes.
 **/


static void g_mime_stream_pipe_class_init (GMimeStreamPipeClass *klass);
static void g_mime_stream_pipe_init (GMimeStreamPipe *stream, GMimeStreamPipeClass *klass);
static void g_mime_stream_pipe_finalize (GObject *object);

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
g_mime_stream_pipe_get_type (void)
{
	static GType type = 0;
	
	if (!type) {
		static const GTypeInfo info = {
			sizeof (GMimeStreamPipeClass),
			NULL, /* base_class_init */
			NULL, /* base_class_finalize */
			(GClassInitFunc) g_mime_stream_pipe_class_init,
			NULL, /* class_finalize */
			NULL, /* class_data */
			sizeof (GMimeStreamPipe),
			0,    /* n_preallocs */
			(GInstanceInitFunc) g_mime_stream_pipe_init,
		};
		
		type = g_type_register_static (GMIME_TYPE_STREAM, "GMimeStreamPipe", &info, 0);
	}
	
	return type;
}


static void
g_mime_stream_pipe_class_init (GMimeStreamPipeClass *klass)
{
	GMimeStreamClass *stream_class = GMIME_STREAM_CLASS (klass);
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	
	parent_class = g_type_class_ref (GMIME_TYPE_STREAM);
	
	object_class->finalize = g_mime_stream_pipe_finalize;
	
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
g_mime_stream_pipe_init (GMimeStreamPipe *stream, GMimeStreamPipeClass *klass)
{
	stream->owner = TRUE;
	stream->eos = FALSE;
	stream->fd = -1;
}

static void
g_mime_stream_pipe_finalize (GObject *object)
{
	GMimeStreamPipe *stream = (GMimeStreamPipe *) object;
	
	if (stream->owner && stream->fd != -1)
		close (stream->fd);
	
	G_OBJECT_CLASS (parent_class)->finalize (object);
}

static ssize_t
stream_read (GMimeStream *stream, char *buf, size_t len)
{
	GMimeStreamPipe *pipes = (GMimeStreamPipe *) stream;
	ssize_t nread;
	
	if (pipes->fd == -1) {
		errno = EBADF;
		return -1;
	}
	
	if (stream->bound_end != -1 && stream->position >= stream->bound_end) {
		errno = EINVAL;
		return -1;
	}
	
	if (stream->bound_end != -1)
		len = (size_t) MIN (stream->bound_end - stream->position, (gint64) len);
	
	do {
		nread = read (pipes->fd, buf, len);
	} while (nread == -1 && errno == EINTR);
	
	if (nread > 0) {
		stream->position += nread;
	} else if (nread == 0) {
		pipes->eos = TRUE;
	}
	
	return nread;
}

static ssize_t
stream_write (GMimeStream *stream, const char *buf, size_t len)
{
	GMimeStreamPipe *pipes = (GMimeStreamPipe *) stream;
	size_t nwritten = 0;
	ssize_t n;
	
	if (pipes->fd == -1) {
		errno = EBADF;
		return -1;
	}
	
	if (stream->bound_end != -1 && stream->position >= stream->bound_end) {
		errno = EINVAL;
		return -1;
	}
	
	if (stream->bound_end != -1)
		len = (size_t) MIN (stream->bound_end - stream->position, (gint64) len);
	
	do {
		do {
			n = write (pipes->fd, buf + nwritten, len - nwritten);
		} while (n == -1 && (errno == EINTR || errno == EAGAIN));
		
		if (n > 0)
			nwritten += n;
	} while (n != -1 && nwritten < len);
	
	if (n == -1 && (errno == EFBIG || errno == ENOSPC))
		pipes->eos = TRUE;
	
	if (nwritten > 0) {
		stream->position += nwritten;
	} else if (n == -1) {
		/* error and nothing written */
		return -1;
	}
	
	return nwritten;
}

static int
stream_flush (GMimeStream *stream)
{
	GMimeStreamPipe *pipes = (GMimeStreamPipe *) stream;
	
	if (pipes->fd == -1) {
		errno = EBADF;
		return -1;
	}
	
	return 0;
}

static int
stream_close (GMimeStream *stream)
{
	GMimeStreamPipe *pipes = (GMimeStreamPipe *) stream;
	int rv;
	
	if (pipes->fd == -1)
		return 0;
	
	do {
		if ((rv = close (pipes->fd)) == 0)
			pipes->fd = -1;
	} while (rv == -1 && errno == EINTR);
	
	return rv;
}

static gboolean
stream_eos (GMimeStream *stream)
{
	GMimeStreamPipe *pipes = (GMimeStreamPipe *) stream;
	
	if (pipes->fd == -1)
		return TRUE;
	
	return pipes->eos;
}

static int
stream_reset (GMimeStream *stream)
{
	GMimeStreamPipe *pipes = (GMimeStreamPipe *) stream;
	
	if (pipes->fd == -1) {
		errno = EBADF;
		return -1;
	}
	
	return 0;
}

static gint64
stream_seek (GMimeStream *stream, gint64 offset, GMimeSeekWhence whence)
{
	return -1;
}

static gint64
stream_tell (GMimeStream *stream)
{
	return -1;
}

static gint64
stream_length (GMimeStream *stream)
{
	return -1;
}

static GMimeStream *
stream_substream (GMimeStream *stream, gint64 start, gint64 end)
{
	return NULL;
}


/**
 * g_mime_stream_pipe_new:
 * @fd: a pipe descriptor
 *
 * Creates a new #GMimeStreamPipe object around @fd.
 *
 * Returns: a stream using @fd.
 **/
GMimeStream *
g_mime_stream_pipe_new (int fd)
{
	GMimeStreamPipe *pipes;
	
	pipes = g_object_newv (GMIME_TYPE_STREAM_PIPE, 0, NULL);
	g_mime_stream_construct (GMIME_STREAM (pipes), 0, -1);
	pipes->owner = TRUE;
	pipes->eos = FALSE;
	pipes->fd = fd;
	
	return (GMimeStream *) pipes;
}


/**
 * g_mime_stream_pipe_get_owner:
 * @stream: a #GMimeStreamPipe
 *
 * Gets whether or not @stream owns the backend pipe descriptor.
 *
 * Returns: %TRUE if @stream owns the backend pipe descriptor or %FALSE
 * otherwise.
 **/
gboolean
g_mime_stream_pipe_get_owner (GMimeStreamPipe *stream)
{
	g_return_val_if_fail (GMIME_IS_STREAM_PIPE (stream), FALSE);
	
	return stream->owner;
}


/**
 * g_mime_stream_pipe_set_owner:
 * @stream: a #GMimeStreamPipe
 * @owner: owner
 *
 * Sets whether or not @stream owns the backend pipe descriptor.
 *
 * Note: @owner should be %TRUE if the stream should close() the
 * backend pipe descriptor when destroyed or %FALSE otherwise.
 **/
void
g_mime_stream_pipe_set_owner (GMimeStreamPipe *stream, gboolean owner)
{
	g_return_if_fail (GMIME_IS_STREAM_PIPE (stream));
	
	stream->owner = owner;
}
