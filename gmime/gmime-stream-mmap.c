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
 *
 * Substreams retain independent positions and EOF state over a fixed-size
 * mapping. Reads and partial writes stop at both the view's end and the
 * mapped extent; seeking does not grow the file or the mapping.
 *
 * Views borrow the mapping and descriptor. Finish using them before
 * explicitly closing an owning parent, and do not truncate the underlying
 * file while any mapped view is active.
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
static gint64 stream_seek (GMimeStream *stream, gint64 offset, GMimeSeekWhence whence);
static gint64 stream_tell (GMimeStream *stream);
static gint64 stream_length (GMimeStream *stream);
static GMimeStream *stream_substream (GMimeStream *stream, gint64 start, gint64 end);


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
	stream->prot = 0;
}

static void
g_mime_stream_mmap_finalize (GObject *object)
{
	GMimeStream *stream = (GMimeStream *) object;
	
	stream_close (stream);
	
	G_OBJECT_CLASS (parent_class)->finalize (object);
}


static gboolean
check_open (GMimeStream *stream)
{
	GMimeStreamMmap *mm = GMIME_STREAM_MMAP (stream);

	if (mm->fd < 0) {
		errno = EBADF;
		return FALSE;
	}

	if (stream->bound_start < 0 || stream->bound_end < -1 ||
	    (stream->bound_end != -1 && stream->bound_end < stream->bound_start) ||
	    stream->position < stream->bound_start ||
	    (stream->bound_end != -1 && stream->position > stream->bound_end) ||
	    mm->maplen > (guint64) G_MAXINT64) {
		errno = EINVAL;
		return FALSE;
	}

	return TRUE;
}

static size_t
bounded_count (GMimeStream *stream, size_t len)
{
	GMimeStreamMmap *mm = GMIME_STREAM_MMAP (stream);
	guint64 end = mm->maplen;

	if (stream->bound_end != -1)
		end = MIN (end, (guint64) stream->bound_end);

	/* Validate integer offsets before forming a pointer: a view may start
	 * beyond storage or declare an end farther than the actual mapping. */
	if ((guint64) stream->position >= end)
		return 0;

	return (size_t) MIN ((guint64) len, end - (guint64) stream->position);
}

static ssize_t
stream_read (GMimeStream *stream, char *buf, size_t len)
{
	GMimeStreamMmap *mm = (GMimeStreamMmap *) stream;
	size_t nread;
	
	if (!check_open (stream))
		return -1;

	if (len > G_MAXSSIZE) {
		errno = EOVERFLOW;
		return -1;
	}

#ifdef HAVE_MMAP
	if (!(mm->prot & PROT_READ)) {
		errno = EACCES;
		return -1;
	}
#endif

	nread = bounded_count (stream, len);
	
	if (nread > 0) {
		memcpy (buf, mm->map + (size_t) stream->position, nread);
		stream->position += nread;
	} else
		mm->eos = TRUE;
	
	return (ssize_t) nread;
}

static ssize_t
stream_write (GMimeStream *stream, const char *buf, size_t len)
{
	GMimeStreamMmap *mm = (GMimeStreamMmap *) stream;
	size_t nwritten;
	
	if (!check_open (stream))
		return -1;

	if (len > G_MAXSSIZE) {
		errno = EOVERFLOW;
		return -1;
	}

#ifdef HAVE_MMAP
	if (!(mm->prot & PROT_WRITE)) {
		errno = EACCES;
		return -1;
	}
#endif
	
	if (stream->bound_end != -1 && stream->position >= stream->bound_end) {
		errno = EINVAL;
		return -1;
	}
	
	nwritten = bounded_count (stream, len);
	
	if (nwritten > 0) {
		memcpy (mm->map + (size_t) stream->position, buf, nwritten);
		stream->position += nwritten;
	}
	
	return (ssize_t) nwritten;
}

static int
stream_flush (GMimeStream *stream)
{
#ifdef HAVE_MSYNC
	GMimeStreamMmap *mm = (GMimeStreamMmap *) stream;
#endif
	
	if (!check_open (stream))
		return -1;
	
#ifdef HAVE_MSYNC
	return msync (mm->map, mm->maplen, MS_SYNC /* | MS_INVALIDATE */);
#else
	return 0;
#endif
}

static int
stream_close (GMimeStream *stream)
{
	GMimeStreamMmap *mm = (GMimeStreamMmap *) stream;
	int rv = 0;
	
	if (mm->fd == -1)
		return 0;
	
	if (mm->owner) {
#ifdef HAVE_MUNMAP
		munmap (mm->map, mm->maplen);
#endif
		
		do {
			rv = close (mm->fd);
		} while (rv == -1 && errno == EINTR);
	}
	
	mm->map = NULL;
	mm->maplen = 0;
	mm->fd = -1;
	
	return rv;
}

static gboolean
stream_eos (GMimeStream *stream)
{
	GMimeStreamMmap *mm = (GMimeStreamMmap *) stream;
	
	if (mm->fd == -1)
		return TRUE;
	
	return mm->eos;
}

static int
stream_reset (GMimeStream *stream)
{
	GMimeStreamMmap *mm = (GMimeStreamMmap *) stream;
	
	if (!check_open (stream))
		return -1;
	
	mm->eos = FALSE;
	
	return 0;
}

static gint64
stream_seek (GMimeStream *stream, gint64 offset, GMimeSeekWhence whence)
{
	GMimeStreamMmap *mm = (GMimeStreamMmap *) stream;
	gint64 origin, real;
	
	if (!check_open (stream))
		return -1;
	
	switch (whence) {
	case GMIME_STREAM_SEEK_SET:
		origin = 0;
		break;
	case GMIME_STREAM_SEEK_CUR:
		origin = stream->position;
		break;
	case GMIME_STREAM_SEEK_END:
		if (stream->bound_end == -1) {
			/* Keep native fixed-map END behavior: positive offsets fail,
			 * and negative targets before the view clamp to its start.
			 * maplen is already an absolute extent, not a view length. */
			if (offset > 0) {
				errno = EINVAL;
				return -1;
			}
			origin = (gint64) mm->maplen;
		} else {
			origin = stream->bound_end;
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

	if (whence == GMIME_STREAM_SEEK_END && stream->bound_end == -1)
		real = MAX (real, stream->bound_start);
	
	/* sanity check the resultant offset */
	if (real < stream->bound_start) {
		errno = EINVAL;
		return -1;
	}
	
	if (stream->bound_end != -1 && real > stream->bound_end) {
		errno = EINVAL;
		return -1;
	}

	if (real == stream->position)
		return real;
	
	/* reset eos if appropriate */
	if ((stream->bound_end != -1 && real < stream->bound_end) ||
	    (mm->eos && real < stream->position))
		mm->eos = FALSE;
	
	stream->position = real;
	
	return real;
}

static gint64
stream_tell (GMimeStream *stream)
{
	if (!check_open (stream))
		return -1;
	
	return stream->position;
}

static gint64
stream_length (GMimeStream *stream)
{
	GMimeStreamMmap *mm = (GMimeStreamMmap *) stream;
	
	if (!check_open (stream))
		return -1;
	
	if (stream->bound_end != -1)
		return stream->bound_end - stream->bound_start;
	
	return MAX (0, (gint64) mm->maplen - stream->bound_start);
}

static GMimeStream *
stream_substream (GMimeStream *stream, gint64 start, gint64 end)
{
	GMimeStreamMmap *mm;

	if (!check_open (stream))
		return NULL;
	
	mm = g_object_new (GMIME_TYPE_STREAM_MMAP, NULL);
	g_mime_stream_construct ((GMimeStream *) mm, start, end);
	mm->maplen = ((GMimeStreamMmap *) stream)->maplen;
	mm->map = ((GMimeStreamMmap *) stream)->map;
	mm->fd = ((GMimeStreamMmap *) stream)->fd;
	mm->owner = FALSE;
	mm->prot = ((GMimeStreamMmap *) stream)->prot;
	
	return (GMimeStream *) mm;
}


/**
 * g_mime_stream_mmap_new:
 * @fd: file descriptor
 * @prot: protection flags
 * @flags: map flags
 *
 * Creates a new #GMimeStreamMmap object around @fd.
 *
 * Returns: (nullable): a stream using @fd, or %NULL on error with errno set.
 * Empty files cannot be mapped. On failure, @fd remains owned by the caller.
 **/
GMimeStream *
g_mime_stream_mmap_new (int fd, int prot, int flags)
{
#ifdef HAVE_MMAP
	gint64 start;
	
	if ((start = lseek (fd, 0, SEEK_CUR)) == -1)
		return NULL;
	
	return g_mime_stream_mmap_new_with_bounds (fd, prot, flags, start, -1);
#else
	errno = ENOSYS;
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
 * Creates a new #GMimeStreamMmap object around @fd with bounds @start
 * and @end.
 * The mapping stops at the actual file extent even when @end extends
 * farther; finite length still reports the declared range. Nonempty reads
 * require PROT_READ and writes require PROT_WRITE in @prot.
 *
 * Returns: (nullable): a stream using @fd with bounds @start and @end,
 * or %NULL on error with errno set. Empty mappings fail with EINVAL;
 * unsupported platforms fail with ENOSYS. On failure, @fd remains owned
 * by the caller.
 **/
GMimeStream *
g_mime_stream_mmap_new_with_bounds (int fd, int prot, int flags, gint64 start, gint64 end)
{
#ifdef HAVE_MMAP
	GMimeStreamMmap *mm;
	struct stat st;
	guint64 extent;
	size_t len;
	char *map;
	
	if (start < 0 || end < -1 || (end != -1 && end < start)) {
		errno = EINVAL;
		return NULL;
	}

	if (fstat (fd, &st) == -1)
		return NULL;

	if (st.st_size < 0 || (guint64) st.st_size > G_MAXINT64) {
		errno = EOVERFLOW;
		return NULL;
	}

	/* A declared view end is not permission to access bytes beyond EOF.
	 * Retain the declared range, but map only actual file storage. */
	extent = (guint64) st.st_size;
	if (end != -1)
		extent = MIN (extent, (guint64) end);
	if (extent > G_MAXSIZE) {
		errno = EOVERFLOW;
		return NULL;
	}
	len = (size_t) extent;

	/* Preserve mmap's rejection of an empty root mapping. Empty substreams
	 * of a nonempty mapping remain valid and never form a data pointer. */
	if (len == 0) {
		errno = EINVAL;
		return NULL;
	}
	
	if ((map = mmap (NULL, len, prot, flags, fd, 0)) == MAP_FAILED)
		return NULL;
	
	mm = g_object_new (GMIME_TYPE_STREAM_MMAP, NULL);
	g_mime_stream_construct ((GMimeStream *) mm, start, end);
	mm->owner = TRUE;
	mm->eos = FALSE;
	mm->fd = fd;
	mm->map = map;
	mm->maplen = len;
	mm->prot = prot;
	
	return (GMimeStream *) mm;
#else
	errno = ENOSYS;
	return NULL;
#endif /* HAVE_MMAP */
}


/**
 * g_mime_stream_mmap_get_owner:
 * @stream: a #GMimeStreamMmap
 *
 * Gets whether or not @stream owns the backend file descriptor.
 *
 * Returns: %TRUE if @stream owns the backend file descriptor or %FALSE
 * otherwise.
 *
 * Since: 3.2
 **/
gboolean
g_mime_stream_mmap_get_owner (GMimeStreamMmap *stream)
{
	g_return_val_if_fail (GMIME_IS_STREAM_MMAP (stream), FALSE);
	
	return stream->owner;
}


/**
 * g_mime_stream_mmap_set_owner:
 * @stream: a #GMimeStreamMmap
 * @owner: %TRUE if this stream should own the file descriptor or %FALSE otherwise
 *
 * Sets whether or not @stream owns the backend file descriptor.
 *
 * Note: @owner should be %TRUE if the stream should close() the
 * backend file descriptor when destroyed or %FALSE otherwise.
 *
 * Since: 3.2
 **/
void
g_mime_stream_mmap_set_owner (GMimeStreamMmap *stream, gboolean owner)
{
	g_return_if_fail (GMIME_IS_STREAM_MMAP (stream));
	
	stream->owner = owner;
}
