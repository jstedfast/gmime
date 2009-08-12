/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*  GMime
 *  Copyright (C) 2000-2009 Jeffrey Stedfast
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
#include <sys/types.h>
#include <sys/stat.h>
#ifdef HAVE_SYS_MMAN_H
#include <sys/mman.h>
#endif
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>

#include "gmime-stream-mmap.h"


/**
 * SECTION: gmime-stream-mmap
 * @title: GMimeStreamMmap
 * @short_description: A memory-mapped file stream
 * @see_also: #GMimeStream
 *
 * A #GMimeStream implementation using a memory-mapped file backing
 * store. This may be faster than #GMimeStreamFs or #GMimeStreamFile
 * but you'll have to do your own performance checking to be sure for
 * your particular application/platform.
 **/


static void g_mime_stream_mmap_class_init (GMimeStreamMmapClass *klass);
static void g_mime_stream_mmap_init (GMimeStreamMmap *stream, GMimeStreamMmapClass *klass);
static void g_mime_stream_mmap_finalize (GObject *object);

static ssize_t stream_read (GMimeStream *stream, char *buf, size_t len);
static ssize_t stream_write (GMimeStream *stream, const char *buf, size_t len);
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
g_mime_stream_mmap_get_type (void)
{
	static GType type = 0;
	
	if (!type) {
		static const GTypeInfo info = {
			sizeof (GMimeStreamMmapClass),
			NULL, /* base_class_init */
			NULL, /* base_class_finalize */
			(GClassInitFunc) g_mime_stream_mmap_class_init,
			NULL, /* class_finalize */
			NULL, /* class_data */
			sizeof (GMimeStreamMmap),
			0,    /* n_preallocs */
			(GInstanceInitFunc) g_mime_stream_mmap_init,
		};
		
		type = g_type_register_static (GMIME_TYPE_STREAM, "GMimeStreamMmap", &info, 0);
	}
	
	return type;
}


static void
g_mime_stream_mmap_class_init (GMimeStreamMmapClass *klass)
{
	GMimeStreamClass *stream_class = GMIME_STREAM_CLASS (klass);
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	
	parent_class = g_type_class_ref (GMIME_TYPE_STREAM);
	
	object_class->finalize = g_mime_stream_mmap_finalize;
	
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
g_mime_stream_mmap_init (GMimeStreamMmap *stream, GMimeStreamMmapClass *klass)
{
	stream->owner = TRUE;
	stream->eos = FALSE;
	stream->fd = -1;
	stream->map = NULL;
	stream->maplen = 0;
}

static void
g_mime_stream_mmap_finalize (GObject *object)
{
	GMimeStreamMmap *stream = (GMimeStreamMmap *) object;
	
	if (stream->owner) {
#ifdef HAVE_MUNMAP
		if (stream->map)
			munmap (stream->map, stream->maplen);
#endif
		
		if (stream->fd != -1)
			close (stream->fd);
	}
	
	G_OBJECT_CLASS (parent_class)->finalize (object);
}


static ssize_t
stream_read (GMimeStream *stream, char *buf, size_t len)
{
	GMimeStreamMmap *mstream = (GMimeStreamMmap *) stream;
	register char *mapptr;
	ssize_t nread;
	
	if (stream->bound_end != -1 && stream->position >= stream->bound_end)
		return -1;
	
	/* make sure we are at the right position */
	mapptr = mstream->map + stream->position;
	
	if (stream->bound_end == -1)
		nread = MIN ((off_t) ((mstream->map + mstream->maplen) - mapptr), (off_t) len);
	else
		nread = MIN (stream->bound_end - stream->position, (off_t) len);
	
	if (nread > 0) {
		memcpy (buf, mapptr, nread);
		stream->position += nread;
	} else
		mstream->eos = TRUE;
	
	return nread;
}

static ssize_t
stream_write (GMimeStream *stream, const char *buf, size_t len)
{
	GMimeStreamMmap *mstream = (GMimeStreamMmap *) stream;
	register char *mapptr;
	ssize_t nwritten;
	
	if (stream->bound_end != -1 && stream->position >= stream->bound_end)
		return -1;
	
	/* make sure we are at the right position */
	mapptr = mstream->map + stream->position;
	
	if (stream->bound_end == -1)
		nwritten = MIN ((off_t) ((mstream->map + mstream->maplen) - mapptr), (off_t) len);
	else
		nwritten = MIN (stream->bound_end - stream->position, (off_t) len);
	
	if (nwritten > 0) {
		memcpy (mapptr, buf, nwritten);
		stream->position += nwritten;
	}
	
	return nwritten;
}

static int
stream_flush (GMimeStream *stream)
{
	GMimeStreamMmap *mstream = (GMimeStreamMmap *) stream;
	
	g_return_val_if_fail (mstream->fd != -1, -1);
	
#ifdef HAVE_MSYNC
	return msync (mstream->map, mstream->maplen, MS_SYNC /* | MS_INVALIDATE */);
#else
	return 0;
#endif
}

static int
stream_close (GMimeStream *stream)
{
	GMimeStreamMmap *mstream = (GMimeStreamMmap *) stream;
	int ret = 0;
	
	if (mstream->owner && mstream->map) {
#ifdef HAVE_MUNMAP
		munmap (mstream->map, mstream->maplen);
		mstream->map = NULL;
#endif
	}
	
	if (mstream->owner && mstream->fd != -1) {
		if ((ret = close (mstream->fd)) != -1)
			mstream->fd = -1;
	}
	
	return ret;
}

static gboolean
stream_eos (GMimeStream *stream)
{
	GMimeStreamMmap *mstream = (GMimeStreamMmap *) stream;
	
	g_return_val_if_fail (mstream->fd != -1, TRUE);
	
	return mstream->eos;
}

static int
stream_reset (GMimeStream *stream)
{
	GMimeStreamMmap *mstream = (GMimeStreamMmap *) stream;
	
	if (mstream->fd == -1)
		return -1;
	
	mstream->eos = FALSE;
	
	return 0;
}

static off_t
stream_seek (GMimeStream *stream, off_t offset, GMimeSeekWhence whence)
{
	GMimeStreamMmap *mstream = (GMimeStreamMmap *) stream;
	off_t real = stream->position;
	
	g_return_val_if_fail (mstream->fd != -1, -1);
	
	switch (whence) {
	case GMIME_STREAM_SEEK_SET:
		real = offset;
		break;
	case GMIME_STREAM_SEEK_CUR:
		real = stream->position + offset;
		break;
	case GMIME_STREAM_SEEK_END:
		if (stream->bound_end == -1) {
			real = offset <= 0 ? stream->bound_start + (off_t) mstream->maplen + offset : -1;
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
	
	/* sanity check the resultant offset */
	if (real < stream->bound_start)
		return -1;
	
	if (stream->bound_end != -1 && real > stream->bound_end)
		return -1;
	
	/* reset eos if appropriate */
	if ((stream->bound_end != -1 && real < stream->bound_end) ||
	    (mstream->eos && real < stream->position))
		mstream->eos = FALSE;
	
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
	GMimeStreamMmap *mstream = (GMimeStreamMmap *) stream;
	
	if (stream->bound_start != -1 && stream->bound_end != -1)
		return stream->bound_end - stream->bound_start;
	
	return mstream->maplen - stream->bound_start;
}

static GMimeStream *
stream_substream (GMimeStream *stream, off_t start, off_t end)
{
	/* FIXME: maybe we should return a GMimeStreamFs? */
	GMimeStreamMmap *mstream;
	
	mstream = g_object_new (GMIME_TYPE_STREAM_MMAP, NULL);
	mstream->owner = FALSE;
	mstream->fd = GMIME_STREAM_MMAP (stream)->fd;
	
	mstream->map = GMIME_STREAM_MMAP (stream)->map;
	mstream->maplen = GMIME_STREAM_MMAP (stream)->maplen;
	
	g_mime_stream_construct (GMIME_STREAM (mstream), start, end);
	
	return GMIME_STREAM (mstream);
}


/**
 * g_mime_stream_mmap_new:
 * @fd: file descriptor
 * @prot: protection flags
 * @flags: map flags
 *
 * Creates a new GMimeStreamMmap object around @fd.
 *
 * Returns a stream using @fd.
 **/
GMimeStream *
g_mime_stream_mmap_new (int fd, int prot, int flags)
{
#ifdef HAVE_MMAP
	GMimeStreamMmap *mstream;
	struct stat st;
	off_t start;
	char *map;
	
	start = lseek (fd, 0, SEEK_CUR);
	if (start == -1)
		return NULL;
	
	if (fstat (fd, &st) == -1)
		return NULL;
	
	map = mmap (NULL, st.st_size, prot, flags, fd, 0);
	if (map == MAP_FAILED)
		return NULL;
	
	mstream = g_object_new (GMIME_TYPE_STREAM_MMAP, NULL);
	mstream->owner = TRUE;
	mstream->eos = FALSE;
	mstream->fd = fd;
	mstream->map = map;
	mstream->maplen = st.st_size;
	
	g_mime_stream_construct (GMIME_STREAM (mstream), start, -1);
	
	return GMIME_STREAM (mstream);
#else
	return NULL;
#endif /* HAVE_MMAP */
}


/**
 * g_mime_stream_mmap_new_with_bounds:
 * @fd: file descriptor
 * @prot: protection flags
 * @flags: map flags
 * @start: start boundary
 * @end: end boundary
 *
 * Creates a new GMimeStreamMmap object around @fd with bounds @start
 * and @end.
 *
 * Returns a stream using @fd with bounds @start and @end.
 **/
GMimeStream *
g_mime_stream_mmap_new_with_bounds (int fd, int prot, int flags, off_t start, off_t end)
{
#ifdef HAVE_MMAP
	GMimeStreamMmap *mstream;
	struct stat st;
	char *map;
	
	if (end == -1) {
		if (fstat (fd, &st) == -1)
			return NULL;
	} else
		st.st_size = end /* - start */;
	
	map = mmap (NULL, st.st_size, prot, flags, fd, 0);
	if (map == MAP_FAILED)
		return NULL;
	
	mstream = g_object_new (GMIME_TYPE_STREAM_MMAP, NULL);
	mstream->owner = TRUE;
	mstream->eos = FALSE;
	mstream->fd = fd;
	mstream->map = map;
	mstream->maplen = st.st_size;
	
	g_mime_stream_construct (GMIME_STREAM (mstream), start, end);
	
	return GMIME_STREAM (mstream);
#else
	return NULL;
#endif /* HAVE_MMAP */
}
