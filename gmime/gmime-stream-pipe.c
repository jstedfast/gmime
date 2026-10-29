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

#include <glib.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>

#include "gmime-stream-pipe.h"


#include "gmime-object-private.h"

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
static void g_mime_stream_pipe_init (GMimeStreamPipe *stream);
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



G_DEFINE_TYPE_WITH_PRIVATE (GMimeStreamPipe, g_mime_stream_pipe, GMIME_TYPE_STREAM)

GMimeStreamPipePrivate *
_g_mime_stream_pipe_get_private (GMimeStreamPipe *object)
{
	return g_mime_stream_pipe_get_instance_private (object);
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
g_mime_stream_pipe_init (GMimeStreamPipe *stream)
{
	GMimeStreamPipePrivate *priv = _g_mime_stream_pipe_get_private (stream);

	priv->owner = TRUE;
	priv->eos = FALSE;
	priv->fd = -1;
}

static void
g_mime_stream_pipe_finalize (GObject *object)
{
	GMimeStream *stream = (GMimeStream *) object;
	
	stream_close (stream);
	
	G_OBJECT_CLASS (parent_class)->finalize (object);
}

static ssize_t
stream_read (GMimeStream *stream, char *buf, size_t len)
{
	GMimeStreamPipe *pipes = (GMimeStreamPipe *) stream;
	GMimeStreamPipePrivate *priv = _g_mime_stream_pipe_get_private (pipes);
	GMimeStreamPrivate *stream_priv;
	ssize_t nread;

	if (priv->fd == -1) {
		errno = EBADF;
		return -1;
	}
	
	stream_priv = _g_mime_stream_get_private (stream);

	if (stream_priv->bound_end != -1 && stream_priv->position >= stream_priv->bound_end) {
		errno = EINVAL;
		return -1;
	}
	
	if (stream_priv->bound_end != -1)
		len = (size_t) MIN (stream_priv->bound_end - stream_priv->position, (gint64) len);
	
	do {
		nread = read (priv->fd, buf, len);
	} while (nread == -1 && errno == EINTR);
	
	if (nread > 0) {
		stream_priv->position += nread;
	} else if (nread == 0) {
		priv->eos = TRUE;
	}
	
	return nread;
}

static ssize_t
stream_write (GMimeStream *stream, const char *buf, size_t len)
{
	GMimeStreamPipe *pipes = (GMimeStreamPipe *) stream;
	GMimeStreamPipePrivate *priv = _g_mime_stream_pipe_get_private (pipes);
	GMimeStreamPrivate *stream_priv;
	size_t nwritten = 0;
	ssize_t n;

	if (priv->fd == -1) {
		errno = EBADF;
		return -1;
	}
	
	stream_priv = _g_mime_stream_get_private (stream);

	if (stream_priv->bound_end != -1 && stream_priv->position >= stream_priv->bound_end) {
		errno = EINVAL;
		return -1;
	}
	
	if (stream_priv->bound_end != -1)
		len = (size_t) MIN (stream_priv->bound_end - stream_priv->position, (gint64) len);
	
	do {
		do {
			n = write (priv->fd, buf + nwritten, len - nwritten);
		} while (n == -1 && (errno == EINTR || errno == EAGAIN));
		
		if (n > 0)
			nwritten += n;
	} while (n != -1 && nwritten < len);
	
	if (n == -1 && (errno == EFBIG || errno == ENOSPC))
		priv->eos = TRUE;
	
	if (nwritten > 0) {
		stream_priv->position += nwritten;
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
	GMimeStreamPipePrivate *priv = _g_mime_stream_pipe_get_private (pipes);

	if (priv->fd == -1) {
		errno = EBADF;
		return -1;
	}
	
	return 0;
}

static int
stream_close (GMimeStream *stream)
{
	GMimeStreamPipe *pipes = (GMimeStreamPipe *) stream;
	GMimeStreamPipePrivate *priv = _g_mime_stream_pipe_get_private (pipes);
	int rv;

	if (priv->fd == -1)
		return 0;
	
	if (priv->owner) {
		do {
			if ((rv = close (priv->fd)) == 0)
				priv->fd = -1;
		} while (rv == -1 && errno == EINTR);
	} else {
		priv->fd = -1;
		rv = 0;
	}
	
	return rv;
}

static gboolean
stream_eos (GMimeStream *stream)
{
	GMimeStreamPipe *pipes = (GMimeStreamPipe *) stream;
	GMimeStreamPipePrivate *priv = _g_mime_stream_pipe_get_private (pipes);

	if (priv->fd == -1)
		return TRUE;
	
	return priv->eos;
}

static int
stream_reset (GMimeStream *stream)
{
	GMimeStreamPipe *pipes = (GMimeStreamPipe *) stream;
	GMimeStreamPipePrivate *priv = _g_mime_stream_pipe_get_private (pipes);

	if (priv->fd == -1) {
		errno = EBADF;
		return -1;
	}
	
	return 0;
}

static gint64
stream_seek (GMimeStream *stream, gint64 offset, GMimeSeekWhence whence)
{
	errno = ESPIPE;
	return -1;
}

static gint64
stream_tell (GMimeStream *stream)
{
	errno = ESPIPE;
	return -1;
}

static gint64
stream_length (GMimeStream *stream)
{
	errno = ESPIPE;
	return -1;
}

static GMimeStream *
stream_substream (GMimeStream *stream, gint64 start, gint64 end)
{
	GMimeStreamPipePrivate *stream_priv;
	GMimeStreamPipePrivate *priv;
	GMimeStreamPipe *pipes;
	
	pipes = g_object_new (GMIME_TYPE_STREAM_PIPE, NULL);
	g_mime_stream_construct ((GMimeStream *) pipes, start, end);
	stream_priv = _g_mime_stream_pipe_get_private ((GMimeStreamPipe *) stream);
	priv = _g_mime_stream_pipe_get_private (pipes);

	priv->fd = stream_priv->fd;
	priv->owner = FALSE;
	priv->eos = FALSE;
	
	return (GMimeStream *) pipes;
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
	GMimeStreamPipePrivate *priv;
	GMimeStreamPipe *pipes;
	
	pipes = g_object_new (GMIME_TYPE_STREAM_PIPE, NULL);
	g_mime_stream_construct ((GMimeStream *) pipes, 0, -1);
	priv = _g_mime_stream_pipe_get_private (pipes);

	priv->owner = TRUE;
	priv->eos = FALSE;
	priv->fd = fd;
	
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
	GMimeStreamPipePrivate *priv;

	g_return_val_if_fail (GMIME_IS_STREAM_PIPE (stream), FALSE);
	
	priv = _g_mime_stream_pipe_get_private (stream);

	return priv->owner;
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
	GMimeStreamPipePrivate *priv;

	g_return_if_fail (GMIME_IS_STREAM_PIPE (stream));
	
	priv = _g_mime_stream_pipe_get_private (stream);

	priv->owner = owner;
}
