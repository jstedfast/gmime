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

#include "gmime-stream-fs.h"

#ifndef HAVE_FSYNC
#ifdef G_OS_WIN32
/* _commit() is the equivalent of fsync() on Windows, but it aborts the
 * program if the fd is a tty, so we'll just no-op for now... */
static int fsync (int fd) { return 0; }
#else
static int fsync (int fd) { return 0; }
#endif
#endif


/**
 * SECTION: gmime-stream-fs
 * @title: GMimeStreamFs
 * @short_description: A low-level FileSystem stream
 * @see_also: #GMimeStream
 *
 * A simple #GMimeStream implementation that sits on top of the
 * low-level UNIX file descriptor based I/O layer.
 **/


static void g_mime_stream_fs_class_init (GMimeStreamFsClass *klass);
static void g_mime_stream_fs_init (GMimeStreamFs *stream, GMimeStreamFsClass *klass);
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


GType
g_mime_stream_fs_get_type (void)
{
	static GType type = 0;
	
	if (!type) {
		static const GTypeInfo info = {
			sizeof (GMimeStreamFsClass),
			NULL, /* base_class_init */
			NULL, /* base_class_finalize */
			(GClassInitFunc) g_mime_stream_fs_class_init,
			NULL, /* class_finalize */
			NULL, /* class_data */
			sizeof (GMimeStreamFs),
			0,    /* n_preallocs */
			(GInstanceInitFunc) g_mime_stream_fs_init,
		};
		
		type = g_type_register_static (GMIME_TYPE_STREAM, "GMimeStreamFs", &info, 0);
	}
	
	return type;
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
g_mime_stream_fs_init (GMimeStreamFs *stream, GMimeStreamFsClass *klass)
{
	stream->owner = TRUE;
	stream->eos = FALSE;
	stream->fd = -1;
}

static void
g_mime_stream_fs_finalize (GObject *object)
{
	GMimeStreamFs *stream = (GMimeStreamFs *) object;
	
	if (stream->owner && stream->fd != -1)
		close (stream->fd);
	
	G_OBJECT_CLASS (parent_class)->finalize (object);
}

static ssize_t
stream_read (GMimeStream *stream, char *buf, size_t len)
{
	GMimeStreamFs *fs = (GMimeStreamFs *) stream;
	ssize_t nread;
	
	if (fs->fd == -1) {
		errno = EBADF;
		return -1;
	}
	
	if (stream->bound_end != -1 && stream->position >= stream->bound_end) {
		errno = EINVAL;
		return -1;
	}
	
	if (stream->bound_end != -1)
		len = (size_t) MIN (stream->bound_end - stream->position, (gint64) len);
	
	/* make sure we are at the right position */
	lseek (fs->fd, (off_t) stream->position, SEEK_SET);
	
	do {
		nread = read (fs->fd, buf, len);
	} while (nread == -1 && errno == EINTR);
	
	if (nread > 0) {
		stream->position += nread;
	} else if (nread == 0) {
		fs->eos = TRUE;
	}
	
	return nread;
}

static ssize_t
stream_write (GMimeStream *stream, const char *buf, size_t len)
{
	GMimeStreamFs *fs = (GMimeStreamFs *) stream;
	size_t nwritten = 0;
	ssize_t n;
	
	if (fs->fd == -1) {
		errno = EBADF;
		return -1;
	}
	
	if (stream->bound_end != -1 && stream->position >= stream->bound_end) {
		errno = EINVAL;
		return -1;
	}
	
	if (stream->bound_end != -1)
		len = (size_t) MIN (stream->bound_end - stream->position, (gint64) len);
	
	/* make sure we are at the right position */
	lseek (fs->fd, (off_t) stream->position, SEEK_SET);
	
	do {
		do {
			n = write (fs->fd, buf + nwritten, len - nwritten);
		} while (n == -1 && (errno == EINTR || errno == EAGAIN));
		
		if (n > 0)
			nwritten += n;
	} while (n != -1 && nwritten < len);
	
	if (n == -1 && (errno == EFBIG || errno == ENOSPC))
		fs->eos = TRUE;
	
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
	GMimeStreamFs *fs = (GMimeStreamFs *) stream;
	
	if (fs->fd == -1) {
		errno = EBADF;
		return -1;
	}
	
	return fsync (fs->fd);
}

static int
stream_close (GMimeStream *stream)
{
	GMimeStreamFs *fs = (GMimeStreamFs *) stream;
	int rv;
	
	if (fs->fd == -1)
		return 0;
	
	do {
		if ((rv = close (fs->fd)) == 0)
			fs->fd = -1;
	} while (rv == -1 && errno == EINTR);
	
	return rv;
}

static gboolean
stream_eos (GMimeStream *stream)
{
	GMimeStreamFs *fs = (GMimeStreamFs *) stream;
	
	if (fs->fd == -1)
		return TRUE;
	
	return fs->eos;
}

static int
stream_reset (GMimeStream *stream)
{
	GMimeStreamFs *fs = (GMimeStreamFs *) stream;
	
	if (fs->fd == -1) {
		errno = EBADF;
		return -1;
	}
	
	if (stream->position == stream->bound_start) {
		fs->eos = FALSE;
		return 0;
	}
	
	/* FIXME: if stream_read/write is always going to lseek to
	 * make sure fd's seek position matches our own, we could just
	 * set stream->position = stream->bound_start and be done. */
	if (lseek (fs->fd, (off_t) stream->bound_start, SEEK_SET) == -1)
		return -1;
	
	fs->eos = FALSE;
	
	return 0;
}

static gint64
stream_seek (GMimeStream *stream, gint64 offset, GMimeSeekWhence whence)
{
	GMimeStreamFs *fs = (GMimeStreamFs *) stream;
	gint64 real;
	
	if (fs->fd == -1) {
		errno = EBADF;
		return -1;
	}
	
	switch (whence) {
	case GMIME_STREAM_SEEK_SET:
		real = offset;
		break;
	case GMIME_STREAM_SEEK_CUR:
		real = stream->position + offset;
		break;
	case GMIME_STREAM_SEEK_END:
		if (offset > 0 || (stream->bound_end == -1 && !fs->eos)) {
			/* need to do an actual lseek() here because
			 * we either don't know the offset of the end
			 * of the stream and/or don't know if we can
			 * seek past the end */
			if ((real = lseek (fs->fd, (off_t) offset, SEEK_END)) == -1)
				return -1;
		} else if (fs->eos && stream->bound_end == -1) {
			/* seeking backwards from eos (which happens
			 * to be our current position) */
			real = stream->position + offset;
		} else {
			/* seeking backwards from a known position */
			real = stream->bound_end + offset;
		}
		
		break;
	default:
		g_assert_not_reached ();
		return -1;
	}
	
	/* sanity check the resultant offset */
	if (real < stream->bound_start) {
		errno = EINVAL;
		return -1;
	}
	
	/* short-cut if we are seeking to our current position */
	if (real == stream->position)
		return real;
	
	if (stream->bound_end != -1 && real > stream->bound_end) {
		errno = EINVAL;
		return -1;
	}
	
	if ((real = lseek (fs->fd, (off_t) real, SEEK_SET)) == -1)
		return -1;
	
	/* reset eos if appropriate */
	if ((stream->bound_end != -1 && real < stream->bound_end) ||
	    (fs->eos && real < stream->position))
		fs->eos = FALSE;
	
	stream->position = real;
	
	return real;
}

static gint64
stream_tell (GMimeStream *stream)
{
	return stream->position;
}

static gint64
stream_length (GMimeStream *stream)
{
	GMimeStreamFs *fs = (GMimeStreamFs *) stream;
	gint64 bound_end;
	
	if (fs->fd == -1) {
		errno = EBADF;
		return -1;
	}
	
	if (stream->bound_end != -1)
		return stream->bound_end - stream->bound_start;
	
	bound_end = lseek (fs->fd, (off_t) 0, SEEK_END);
	lseek (fs->fd, (off_t) stream->position, SEEK_SET);
	
	if (bound_end < stream->bound_start) {
		errno = EINVAL;
		return -1;
	}
	
	return bound_end - stream->bound_start;
}

static GMimeStream *
stream_substream (GMimeStream *stream, gint64 start, gint64 end)
{
	GMimeStreamFs *fs;
	
	fs = g_object_newv (GMIME_TYPE_STREAM_FS, 0, NULL);
	g_mime_stream_construct (GMIME_STREAM (fs), start, end);
	fs->fd = GMIME_STREAM_FS (stream)->fd;
	fs->owner = FALSE;
	fs->eos = FALSE;
	
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
	GMimeStreamFs *fs;
	gint64 start;
	
#ifdef G_OS_WIN32
	_setmode (fd, O_BINARY);
#endif
	
	if ((start = lseek (fd, (off_t) 0, SEEK_CUR)) == -1)
		start = 0;
	
	fs = g_object_newv (GMIME_TYPE_STREAM_FS, 0, NULL);
	g_mime_stream_construct (GMIME_STREAM (fs), start, -1);
	fs->owner = TRUE;
	fs->eos = FALSE;
	fs->fd = fd;
	
	return (GMimeStream *) fs;
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
	GMimeStreamFs *fs;
	
#ifdef G_OS_WIN32
	_setmode (fd, O_BINARY);
#endif
	
	fs = g_object_newv (GMIME_TYPE_STREAM_FS, 0, NULL);
	g_mime_stream_construct (GMIME_STREAM (fs), start, end);
	fs->owner = TRUE;
	fs->eos = FALSE;
	fs->fd = fd;
	
	return (GMimeStream *) fs;
}


/**
 * g_mime_stream_fs_new_for_path:
 * @path: the path to a file
 * @flags: as in open(2)
 * @mode: as in open(2)
 *
 * Creates a new #GMimeStreamFs object for the specified @path.
 *
 * Returns: a stream using for reading and/or writing to the specified
 * file path or %NULL on error.
 *
 * Since: 2.6.18
 **/
GMimeStream *
g_mime_stream_fs_new_for_path (const char *path, int flags, int mode)
{
	int fd;
	
	g_return_val_if_fail (path != NULL, NULL);
	
	if ((fd = g_open (path, flags, mode)) == -1)
		return NULL;
	
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
	g_return_val_if_fail (GMIME_IS_STREAM_FS (stream), FALSE);
	
	return stream->owner;
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
	g_return_if_fail (GMIME_IS_STREAM_FS (stream));
	
	stream->owner = owner;
}
