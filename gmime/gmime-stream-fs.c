/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 *  Authors: Jeffrey Stedfast <fejj@ximian.com>
 *
 *  Copyright 2001 Ximian, Inc. (www.ximian.com)
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

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>

#include "gmime-stream-fs.h"

static void g_mime_stream_fs_class_init (GMimeStreamFsClass *klass);
static void g_mime_stream_fs_init (GMimeStreamFs *stream, GMimeStreamFsClass *klass);
static void g_mime_stream_fs_finalize (GObject *object);

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
			16,   /* n_preallocs */
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
	GMimeStreamFs *fstream = (GMimeStreamFs *) stream;
	ssize_t nread;
	
	if (stream->bound_end != -1 && stream->position >= stream->bound_end)
		return -1;
	
	if (stream->bound_end != -1)
		len = MIN (stream->bound_end - stream->position, len);
	
	/* make sure we are at the right position */
	lseek (fstream->fd, stream->position, SEEK_SET);
	
	do {
		nread = read (fstream->fd, buf, len);
	} while (nread == -1 && errno == EINTR);
	
	if (nread > 0)
		stream->position += nread;
	else if (nread == 0)
		fstream->eos = TRUE;
	
	return nread;
}

static ssize_t
stream_write (GMimeStream *stream, char *buf, size_t len)
{
	GMimeStreamFs *fstream = (GMimeStreamFs *) stream;
	ssize_t nwritten = 0, n;
	
	if (stream->bound_end != -1 && stream->position >= stream->bound_end)
		return -1;
	
	if (stream->bound_end != -1)
		len = MIN (stream->bound_end - stream->position, len);
	
	/* make sure we are at the right position */
	lseek (fstream->fd, stream->position, SEEK_SET);
	
	do {
		do {
			n = write (fstream->fd, buf + nwritten, len - nwritten);
		} while (n == -1 && (errno == EINTR || errno == EAGAIN));
		
		if (n > 0)
			nwritten += n;
	} while (n != -1 && nwritten < len);
	
	if (nwritten > 0)
		stream->position += nwritten;
	else if (n == -1)
		return -1;
	
	return nwritten;
}

static int
stream_flush (GMimeStream *stream)
{
	GMimeStreamFs *fstream = (GMimeStreamFs *) stream;
	
	g_return_val_if_fail (fstream->fd != -1, -1);
	
	return fsync (fstream->fd);
}

static int
stream_close (GMimeStream *stream)
{
	GMimeStreamFs *fstream = (GMimeStreamFs *) stream;
	int ret;
	
	g_return_val_if_fail (fstream->fd != -1, -1);
	
	ret = close (fstream->fd);
	if (ret != -1)
		fstream->fd = -1;
	
	return ret;
}

static gboolean
stream_eos (GMimeStream *stream)
{
	GMimeStreamFs *fstream = (GMimeStreamFs *) stream;
	
	g_return_val_if_fail (fstream->fd != -1, TRUE);
	
	return fstream->eos;
}

static int
stream_reset (GMimeStream *stream)
{
	GMimeStreamFs *fstream = (GMimeStreamFs *) stream;
	off_t ret;
	
	g_return_val_if_fail (fstream->fd != -1, -1);
	
	if (stream->position == stream->bound_start)
		return 0;
	
	ret = lseek (fstream->fd, stream->bound_start, SEEK_SET);
	if (ret != -1) {
		fstream->eos = FALSE;
		stream->position = stream->bound_start;
	}
	
	return ret;
}

static off_t
stream_seek (GMimeStream *stream, off_t offset, GMimeSeekWhence whence)
{
	GMimeStreamFs *fstream = (GMimeStreamFs *) stream;
	off_t real = stream->position;
	
	g_return_val_if_fail (fstream->fd != -1, -1);
	
	switch (whence) {
	case GMIME_STREAM_SEEK_SET:
		real = offset;
		break;
	case GMIME_STREAM_SEEK_CUR:
		real = stream->position + offset;
		break;
	case GMIME_STREAM_SEEK_END:
		if (stream->bound_end == -1) {
			real = lseek (fstream->fd, offset, SEEK_END);
			if (real != -1) {
				if (real < stream->bound_start)
					real = stream->bound_start;
				stream->position = real;
			}
			return real;
		}
		real = stream->bound_end + offset;
		break;
	}
	
	if (stream->bound_end != -1)
		real = MIN (real, stream->bound_end);
	real = MAX (real, stream->bound_start);
	
	real = lseek (fstream->fd, real, SEEK_SET);
	if (real == -1)
		return -1;
	
	if (real != stream->position && fstream->eos)
		fstream->eos = FALSE;
	
	stream->position = real;
	
	return real;
}

static off_t
stream_tell (GMimeStream *stream)
{
	return stream->position;
}

static ssize_t
stream_length (GMimeStream *stream)
{
	GMimeStreamFs *fstream = (GMimeStreamFs *) stream;
	off_t bound_end;
	
	if (stream->bound_start != -1 && stream->bound_end != -1)
		return stream->bound_end - stream->bound_start;
	
	bound_end = lseek (fstream->fd, 0, SEEK_END);
	lseek (fstream->fd, stream->position, SEEK_SET);
	
	if (bound_end < stream->bound_start)
		return -1;
	
	return bound_end - stream->bound_start;
}

static GMimeStream *
stream_substream (GMimeStream *stream, off_t start, off_t end)
{
	GMimeStreamFs *fstream;
	
	fstream = g_object_new (GMIME_TYPE_STREAM_FS, NULL, NULL);
	fstream->owner = FALSE;
	fstream->fd = GMIME_STREAM_FS (stream)->fd;
	
	g_mime_stream_construct (GMIME_STREAM (fstream), start, end);
	
	return GMIME_STREAM (fstream);
}


/**
 * g_mime_stream_fs_new:
 * @fd: file descriptor
 *
 * Creates a new GMimeStreamFs object around @fd.
 *
 * Returns a stream using @fd.
 **/
GMimeStream *
g_mime_stream_fs_new (int fd)
{
	GMimeStreamFs *fstream;
	off_t start;
	
	fstream = g_object_new (GMIME_TYPE_STREAM_FS, NULL, NULL);
	fstream->owner = TRUE;
	fstream->eos = FALSE;
	fstream->fd = fd;
	
	start = lseek (fd, 0, SEEK_CUR);
	
	g_mime_stream_construct (GMIME_STREAM (fstream), start, -1);
	
	return GMIME_STREAM (fstream);
}


/**
 * g_mime_stream_fs_new_with_bounds:
 * @fd: file descriptor
 * @start: start boundary
 * @end: end boundary
 *
 * Creates a new GMimeStreamFs object around @fd with bounds @start
 * and @end.
 *
 * Returns a stream using @fd with bounds @start and @end.
 **/
GMimeStream *
g_mime_stream_fs_new_with_bounds (int fd, off_t start, off_t end)
{
	GMimeStreamFs *fstream;
	
	fstream = g_object_new (GMIME_TYPE_STREAM_FS, NULL, NULL);
	fstream->owner = TRUE;
	fstream->eos = FALSE;
	fstream->fd = fd;
	
	g_mime_stream_construct (GMIME_STREAM (fstream), start, end);
	
	return GMIME_STREAM (fstream);
}


/**
 * g_mime_stream_fs_get_owner:
 * @stream: fs stream
 *
 * Gets whether or not @stream owns the backend file descriptor.
 *
 * Returns %TRUE if @stream owns the backend file descriptor or %FALSE
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
 * @stream: fs stream
 * @owner: owner
 *
 * Sets whether or not @stream owns the backend FS pointer.
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
