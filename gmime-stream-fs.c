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


#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>

#include "gmime-stream-fs.h"


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

static GMimeStream template = {
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
	GMimeStreamFs *fstream = (GMimeStreamFs *) stream;
	
	if (fstream->owner && fstream->fd)
		close (fstream->fd);
	
	g_free (fstream);
}

static ssize_t
stream_read (GMimeStream *stream, char *buf, size_t len)
{
	GMimeStreamFs *fstream = (GMimeStreamFs *) stream;
	ssize_t nread;
	
	if (stream->bound_end != -1 && stream->position > stream->bound_end)
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
	ssize_t written = 0, n;
	
	if (stream->bound_end == -1 && stream->position > stream->bound_end)
		return -1;
	
	if (stream->bound_end != -1)
		len = MIN (stream->bound_end - stream->position, len);
	
	do {
		n = write (fstream->fd, buf + written, len - written);
		if (n > 0)
			written += n;
	} while (n == -1 && errno == EINTR);
	
	return written;
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
	
	ret = lseek (fstream->fd, stream->bound_start, SEEK_SET);
	if (ret != -1)
		fstream->eos = FALSE;
	
	stream->position = stream->bound_start;
	
	return ret;
}

static off_t
stream_seek (GMimeStream *stream, off_t offset, GMimeSeekWhence whence)
{
	GMimeStreamFs *fstream = (GMimeStreamFs *) stream;
	off_t ret;
	
	g_return_val_if_fail (fstream->fd != -1, -1);
	
	switch (whence) {
	case GMIME_STREAM_SEEK_SET:
		ret = lseek (fstream->fd, offset + stream->bound_start, SEEK_SET);
		break;
	case GMIME_STREAM_SEEK_END:
		if (offset < 0) {
			offset += stream->bound_end;
			ret = lseek (fstream->fd, offset, SEEK_SET);
		} else {
			ret = lseek (fstream->fd, stream->bound_end, SEEK_SET);
		}
		break;
	case GMIME_STREAM_SEEK_CUR:
		offset += stream->position;
		if (offset < stream->bound_start) {
			ret = lseek (fstream->fd, stream->bound_start, SEEK_SET);
		} else if (offset > stream->bound_end) {
			ret = lseek (fstream->fd, stream->bound_end, SEEK_SET);
		} else {
			ret = lseek (fstream->fd, offset, SEEK_CUR);
		}
	}
	
	stream->position = lseek (fstream->fd, 0, SEEK_CUR);
	
	return ret;
}

static off_t
stream_tell (GMimeStream *stream)
{
	return stream->position - stream->bound_start;
}

static ssize_t
stream_length (GMimeStream *stream)
{
	GMimeStreamFs *fstream = (GMimeStreamFs *) stream;
	off_t len;
	
	if (stream->bound_start != -1 && stream->bound_end != -1)
		return stream->bound_end - stream->bound_start;
	
	len = lseek (fstream->fd, 0, SEEK_END);
	lseek (fstream->fd, stream->position, SEEK_SET);
	
	return len;
}

static GMimeStream *
stream_substream (GMimeStream *stream, off_t start, off_t end)
{
	GMimeStreamFs *fstream;
	
	fstream = g_new (GMimeStreamFs, 1);
	fstream->owner = FALSE;
	fstream->fd = GMIME_STREAM_FS (stream)->fd;
	
	g_mime_stream_construct (GMIME_STREAM (fstream), &template, GMIME_STREAM_FS_TYPE, start, end);
	
	return GMIME_STREAM (fstream);
}


/**
 * g_mime_stream_fs_new:
 * @fd: file descriptor
 *
 * Returns a stream using @fd.
 **/
GMimeStream *
g_mime_stream_fs_new (int fd)
{
	GMimeStreamFs *fstream;
	off_t start;
	
	fstream = g_new (GMimeStreamFs, 1);
	fstream->owner = TRUE;
	fstream->eos = FALSE;
	fstream->fd = fd;
	
	start = lseek (fd, 0, SEEK_CUR);
	
	g_mime_stream_construct (GMIME_STREAM (fstream), &template, GMIME_STREAM_FS_TYPE, start, -1);
	
	return GMIME_STREAM (fstream);
}


/**
 * g_mime_stream_fs_new_with_bounds:
 * @fd: file descriptor
 * @start: start boundary
 * @end: end boundary
 *
 * Returns a stream using @fd with bounds @start and @end.
 **/
GMimeStream *
g_mime_stream_fs_new_with_bounds (int fd, off_t start, off_t end)
{
	GMimeStreamFs *fstream;
	
	fstream = g_new (GMimeStreamFs, 1);
	fstream->owner = TRUE;
	fstream->eos = FALSE;
	fstream->fd = fd;
	
	g_mime_stream_construct (GMIME_STREAM (fstream), &template, GMIME_STREAM_FS_TYPE, start, end);
	
	return GMIME_STREAM (fstream);
}
