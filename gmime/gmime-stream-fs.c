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
#include <glib/gstdio.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>

#include "gmime-stream-fs.h"
#include "gmime-error.h"

/* The Windows CRT uses an int result and narrower counts even on Win64. */
#ifdef G_OS_WIN32
#define MAX_IO_COUNT G_MAXINT
#else
#define MAX_IO_COUNT G_MAXSSIZE
#endif

#ifndef HAVE_FSYNC
#ifdef G_OS_WIN32
/* _commit() is the equivalent of fsync() on Windows, but it aborts the
 * program if the fd is a tty, so we'll just no-op for now... */
static int fsync (int fd) { return 0; }
#else
static int fsync (int fd) { return 0; }
#endif
#endif

#include "gmime-object-private.h"

/**
 * SECTION: gmime-stream-fs
 * @title: GMimeStreamFs
 * @short_description: A low-level FileSystem stream
 * @see_also: #GMimeStream
 *
 * A simple #GMimeStream implementation that sits on top of the
 * low-level UNIX file descriptor based I/O layer.
 *
 * Substreams share the descriptor but retain independent logical positions
 * and EOF state. Reads reposition the descriptor and stop at the view's end,
 * returning zero there. Partial writes and absolute SEEK_SET coordinates
 * retain their native conventions.
 *
 * Views borrow the descriptor: keeping the parent object alive does not
 * protect them against explicitly closing an owning parent. Finish using
 * all views before closing its descriptor.
 **/


static void g_mime_stream_fs_class_init (GMimeStreamFsClass *klass);
static void g_mime_stream_fs_init (GMimeStreamFs *stream);
static void g_mime_stream_fs_finalize (GObject *object);

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

G_DEFINE_TYPE_WITH_PRIVATE (GMimeStreamFs, g_mime_stream_fs, GMIME_TYPE_STREAM)

GMimeStreamFsPrivate *
_g_mime_stream_fs_get_private (GMimeStreamFs *object)
{
	return g_mime_stream_fs_get_instance_private (object);
}


static void
g_mime_stream_fs_class_init (GMimeStreamFsClass *klass)
{
	GMimeStreamClass *stream_class = GMIME_STREAM_CLASS (klass);
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	
	parent_class = g_type_class_ref (GMIME_TYPE_STREAM);
	
	object_class->finalize = g_mime_stream_fs_finalize;
	
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
g_mime_stream_fs_init (GMimeStreamFs *stream)
{
	GMimeStreamFsPrivate *priv = _g_mime_stream_fs_get_private (stream);

	priv->owner = TRUE;
	priv->eos = FALSE;
	priv->fd = -1;
}

static void
g_mime_stream_fs_finalize (GObject *object)
{
	GMimeStream *stream = (GMimeStream *) object;
	
	stream_close (stream);
	
	G_OBJECT_CLASS (parent_class)->finalize (object);
}

static gboolean
check_open (GMimeStream *stream)
{
	GMimeStreamFsPrivate *priv = _g_mime_stream_fs_get_private (GMIME_STREAM_FS (stream));
	GMimeStreamPrivate *stream_priv;

	if (priv->fd < 0) {
		errno = EBADF;
		return FALSE;
	}

	stream_priv = _g_mime_stream_get_private (stream);

	if (stream_priv->bound_start < 0 || stream_priv->bound_end < -1 ||
	    (stream_priv->bound_end != -1 && stream_priv->bound_end < stream_priv->bound_start) ||
	    stream_priv->position < stream_priv->bound_start ||
	    (stream_priv->bound_end != -1 && stream_priv->position > stream_priv->bound_end)) {
		errno = EINVAL;
		return FALSE;
	}

	return TRUE;
}

static gint64
seek_fd (GMimeStreamFs *fs, gint64 offset, int whence)
{
	GMimeStreamFsPrivate *priv;

	/* off_t may be narrower than the public 64-bit position, particularly
	 * on Windows. Reject truncation before moving the shared descriptor. */
	if ((gint64) (off_t) offset != offset) {
		errno = EOVERFLOW;
		return -1;
	}

	priv = _g_mime_stream_fs_get_private (fs);

	return lseek (priv->fd, (off_t) offset, whence);
}

static ssize_t
stream_read (GMimeStream *stream, char *buf, size_t len)
{
	GMimeStreamFs *fs = (GMimeStreamFs *) stream;
	GMimeStreamPrivate *stream_priv;
	GMimeStreamFsPrivate *fs_priv;
	ssize_t nread;
	
	if (!check_open (stream))
		return -1;

	if (len > MAX_IO_COUNT) {
		errno = EOVERFLOW;
		return -1;
	}
	
	/* A finite view's end is EOF, not an invalid read. Do not reposition
	 * the shared descriptor or inspect bytes belonging to a sibling view. */
	stream_priv = _g_mime_stream_get_private (stream);
	fs_priv = _g_mime_stream_fs_get_private (fs);

	if (stream_priv->bound_end != -1 && stream_priv->position == stream_priv->bound_end) {
		fs_priv->eos = TRUE;
		return 0;
	}
	
	if (stream_priv->bound_end != -1)
		len = (size_t) MIN (stream_priv->bound_end - stream_priv->position, (gint64) len);

	if (len > (guint64) (G_MAXINT64 - stream_priv->position)) {
		errno = EOVERFLOW;
		return -1;
	}
	
	/* Do not short-circuit cached EOF: another writer may have extended the
	 * file. Reposition for this view before attempting another read. */
	if (seek_fd (fs, stream_priv->position, SEEK_SET) == -1)
		return -1;
	
	do {
		nread = read (fs_priv->fd, buf, len);
	} while (nread == -1 && errno == EINTR);
	
	if (nread > 0) {
		stream_priv->position += nread;
	} else if (nread == 0) {
		fs_priv->eos = TRUE;
	}
	
	return nread;
}

static ssize_t
stream_write (GMimeStream *stream, const char *buf, size_t len)
{
	GMimeStreamFs *fs = (GMimeStreamFs *) stream;
	GMimeStreamPrivate *stream_priv;
	GMimeStreamFsPrivate *fs_priv;
	size_t nwritten = 0;
	ssize_t n;
	
	if (!check_open (stream))
		return -1;

	if (len > MAX_IO_COUNT) {
		errno = EOVERFLOW;
		return -1;
	}
	
	stream_priv = _g_mime_stream_get_private (stream);

	if (stream_priv->bound_end != -1 && stream_priv->position >= stream_priv->bound_end) {
		errno = EINVAL;
		return -1;
	}
	
	if (stream_priv->bound_end != -1)
		len = (size_t) MIN (stream_priv->bound_end - stream_priv->position, (gint64) len);

	/* Bound clipping comes first so a fitting prefix is still writable
	 * even when the original request would overflow the logical position. */
	if (len > (guint64) (G_MAXINT64 - stream_priv->position)) {
		errno = EOVERFLOW;
		return -1;
	}
	
	/* make sure we are at the right position */
	if (seek_fd (fs, stream_priv->position, SEEK_SET) == -1)
		return -1;
	
	fs_priv = _g_mime_stream_fs_get_private (fs);

	do {
		do {
			n = write (fs_priv->fd, buf + nwritten, len - nwritten);
		} while (n == -1 && (errno == EINTR || errno == EAGAIN));
		
		if (n > 0)
			nwritten += n;
	} while (n != -1 && nwritten < len);
	
	if (n == -1 && (errno == EFBIG || errno == ENOSPC))
		fs_priv->eos = TRUE;
	
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
	GMimeStreamFs *fs = (GMimeStreamFs *) stream;
	GMimeStreamFsPrivate *priv;
	
	if (!check_open (stream))
		return -1;
	
	priv = _g_mime_stream_fs_get_private (fs);

	return fsync (priv->fd);
}

static int
stream_close (GMimeStream *stream)
{
	GMimeStreamFs *fs = (GMimeStreamFs *) stream;
	GMimeStreamFsPrivate *priv = _g_mime_stream_fs_get_private (fs);
	int rv = 0;

	if (priv->fd == -1)
		return 0;
	
	if (priv->owner) {
		do {
			rv = close (priv->fd);
		} while (rv == -1 && errno == EINTR);
	}
	
	priv->fd = -1;
	
	return rv;
}

static gboolean
stream_eos (GMimeStream *stream)
{
	GMimeStreamFs *fs = (GMimeStreamFs *) stream;
	GMimeStreamFsPrivate *priv = _g_mime_stream_fs_get_private (fs);

	if (priv->fd == -1)
		return TRUE;
	
	return priv->eos;
}

static int
stream_reset (GMimeStream *stream)
{
	GMimeStreamFs *fs = (GMimeStreamFs *) stream;
	GMimeStreamPrivate *stream_priv;
	GMimeStreamFsPrivate *fs_priv;
	
	if (!check_open (stream))
		return -1;
	
	stream_priv = _g_mime_stream_get_private (stream);

	fs_priv = _g_mime_stream_fs_get_private (fs);

	if (stream_priv->position == stream_priv->bound_start) {
		fs_priv->eos = FALSE;
		return 0;
	}
	
	/* FIXME: if stream_read/write is always going to lseek to
	 * make sure fd's seek position matches our own, we could just
	 * set stream->position = stream->bound_start and be done. */
	if (seek_fd (fs, stream_priv->bound_start, SEEK_SET) == -1)
		return -1;
	
	fs_priv->eos = FALSE;
	
	return 0;
}

static gint64
stream_seek (GMimeStream *stream, gint64 offset, GMimeSeekWhence whence)
{
	GMimeStreamFs *fs = (GMimeStreamFs *) stream;
	GMimeStreamPrivate *stream_priv;
	GMimeStreamFsPrivate *fs_priv;
	gint64 origin, real;
	
	if (!check_open (stream))
		return -1;
	
	stream_priv = _g_mime_stream_get_private (stream);

	fs_priv = _g_mime_stream_fs_get_private (fs);

	switch (whence) {
	case GMIME_STREAM_SEEK_SET:
		origin = 0;
		break;
	case GMIME_STREAM_SEEK_CUR:
		origin = stream_priv->position;
		break;
	case GMIME_STREAM_SEEK_END:
		if (offset > 0 || (stream_priv->bound_end == -1 && !fs_priv->eos)) {
			/* Query the actual end before adding the offset, so signed
			 * overflow is detected rather than delegated to lseek(). */
			if ((origin = seek_fd (fs, 0, SEEK_END)) == -1)
				return -1;
		} else if (fs_priv->eos && stream_priv->bound_end == -1) {
			/* seeking backwards from eos (which happens
			 * to be our current position) */
			origin = stream_priv->position;
		} else {
			/* seeking backwards from a known position */
			origin = stream_priv->bound_end;
		}
		
		break;
	default:
		errno = EINVAL;
		return -1;
	}

	if ((offset > 0 && origin > G_MAXINT64 - offset) ||
	    (offset < 0 && origin < G_MININT64 - offset)) {
		errno = EOVERFLOW;
		return -1;
	}

	real = origin + offset;
	
	/* sanity check the resultant offset */
	if (real < stream_priv->bound_start) {
		errno = EINVAL;
		return -1;
	}
	
	/* short-cut if we are seeking to our current position */
	if (real == stream_priv->position)
		return real;
	
	if (stream_priv->bound_end != -1 && real > stream_priv->bound_end) {
		errno = EINVAL;
		return -1;
	}
	
	if ((real = seek_fd (fs, real, SEEK_SET)) == -1)
		return -1;
	
	/* reset eos if appropriate */
	if ((stream_priv->bound_end != -1 && real < stream_priv->bound_end) ||
	    (fs_priv->eos && real < stream_priv->position))
		fs_priv->eos = FALSE;
	
	stream_priv->position = real;
	
	return real;
}

static gint64
stream_tell (GMimeStream *stream)
{
	GMimeStreamPrivate *priv;

	if (!check_open (stream))
		return -1;

	priv = _g_mime_stream_get_private (stream);

	return priv->position;
}

static gint64
stream_length (GMimeStream *stream)
{
	GMimeStreamFs *fs = (GMimeStreamFs *) stream;
	GMimeStreamPrivate *priv;
	gint64 bound_end;
	
	if (!check_open (stream))
		return -1;
	
	priv = _g_mime_stream_get_private (stream);

	if (priv->bound_end != -1)
		return priv->bound_end - priv->bound_start;
	
	if ((bound_end = seek_fd (fs, 0, SEEK_END)) == -1)
		return -1;
	
	if (seek_fd (fs, priv->position, SEEK_SET) == -1)
		return -1;
	
	/* A view starting beyond current file data is empty, not negative. */
	return MAX (0, bound_end - priv->bound_start);
}

static GMimeStream *
stream_substream (GMimeStream *stream, gint64 start, gint64 end)
{
	GMimeStreamFsPrivate *stream_priv;
	GMimeStreamFsPrivate *fs_priv;
	GMimeStreamFs *fs;

	if (!check_open (stream))
		return NULL;

	fs = g_object_new (GMIME_TYPE_STREAM_FS, NULL);
	g_mime_stream_construct ((GMimeStream *) fs, start, end);
	fs_priv = _g_mime_stream_fs_get_private (fs);

	stream_priv = _g_mime_stream_fs_get_private ((GMimeStreamFs *) stream);

	fs_priv->fd = stream_priv->fd;
	fs_priv->owner = FALSE;
	fs_priv->eos = FALSE;
	
	return (GMimeStream *) fs;
}


/**
 * g_mime_stream_fs_new:
 * @fd: a file descriptor
 *
 * Creates a new #GMimeStreamFs object around @fd.
 *
 * Returns: a stream using @fd.
 **/
GMimeStream *
g_mime_stream_fs_new (int fd)
{
	gint64 start;
	
#ifdef G_OS_WIN32
	_setmode (fd, O_BINARY);
#endif
	
	if ((start = lseek (fd, (off_t) 0, SEEK_CUR)) == -1)
		start = 0;
	
	return g_mime_stream_fs_new_with_bounds (fd, start, -1);
}


/**
 * g_mime_stream_fs_new_with_bounds:
 * @fd: a file descriptor
 * @start: start boundary
 * @end: end boundary
 *
 * Creates a new #GMimeStreamFs object around @fd with bounds @start
 * and @end.
 *
 * Returns: a stream using @fd with bounds @start and @end.
 **/
GMimeStream *
g_mime_stream_fs_new_with_bounds (int fd, gint64 start, gint64 end)
{
	GMimeStreamFsPrivate *priv;
	GMimeStreamFs *fs;
	
#ifdef G_OS_WIN32
	_setmode (fd, O_BINARY);
#endif
	
	fs = g_object_new (GMIME_TYPE_STREAM_FS, NULL);
	g_mime_stream_construct ((GMimeStream *) fs, start, end);
	priv = _g_mime_stream_fs_get_private (fs);

	priv->owner = TRUE;
	priv->eos = FALSE;
	priv->fd = fd;
	
	return (GMimeStream *) fs;
}


/**
 * g_mime_stream_fs_open:
 * @path: the path to a file
 * @flags: as in open(2)
 * @mode: as in open(2)
 * @err: a #GError
 *
 * Creates a new #GMimeStreamFs object for the specified @path.
 *
 * Returns: (transfer full): a stream using for reading and/or writing to the
 * specified file path or %NULL on error.
 **/
GMimeStream *
g_mime_stream_fs_open (const char *path, int flags, int mode, GError **err)
{
	int fd;
	
	g_return_val_if_fail (path != NULL, NULL);
	
	if ((fd = g_open (path, flags, mode)) == -1) {
		g_set_error (err, GMIME_ERROR, errno, "Failed to open `%s': %s", path, g_strerror (errno));
		return NULL;
	}
	
	return g_mime_stream_fs_new (fd);
}


/**
 * g_mime_stream_fs_get_owner:
 * @stream: a #GMimeStreamFs
 *
 * Gets whether or not @stream owns the backend file descriptor.
 *
 * Returns: %TRUE if @stream owns the backend file descriptor or %FALSE
 * otherwise.
 **/
gboolean
g_mime_stream_fs_get_owner (GMimeStreamFs *stream)
{
	GMimeStreamFsPrivate *priv;

	g_return_val_if_fail (GMIME_IS_STREAM_FS (stream), FALSE);
	
	priv = _g_mime_stream_fs_get_private (stream);

	return priv->owner;
}


/**
 * g_mime_stream_fs_set_owner:
 * @stream: a #GMimeStreamFs
 * @owner: %TRUE if this stream should own the file descriptor or %FALSE otherwise
 *
 * Sets whether or not @stream owns the backend file descriptor.
 *
 * Note: @owner should be %TRUE if the stream should close() the
 * backend file descriptor when destroyed or %FALSE otherwise.
 **/
void
g_mime_stream_fs_set_owner (GMimeStreamFs *stream, gboolean owner)
{
	GMimeStreamFsPrivate *priv;

	g_return_if_fail (GMIME_IS_STREAM_FS (stream));
	
	priv = _g_mime_stream_fs_get_private (stream);

	priv->owner = owner;
}
