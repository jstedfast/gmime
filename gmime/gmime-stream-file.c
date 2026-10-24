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

#ifdef G_OS_WIN32
#include <fcntl.h>
#include <io.h>
#endif
#include <errno.h>

#include "gmime-stream-file.h"
#include "gmime-error.h"


/**
 * SECTION: gmime-stream-file
 * @title: GMimeStreamFile
 * @short_description: A Standard-C FILE-based stream
 * @see_also: #GMimeStream
 *
 * A simple #GMimeStream implementation that sits on top of the
 * Standard C FILE pointer based I/O layer. Unlike #GMimeStreamFs, a
 * #GMimeStreamFile will typically buffer read and write operations at
 * the FILE level and so it may be wasteful to wrap one in a
 * #GMimeStreamBuffer stream.
 *
 * Substreams share the FILE pointer, not their logical positions or EOF
 * state. Reads stop at finite bounds and retry the underlying file after
 * observed EOF, so another writer can extend it. Native absolute SEEK_SET
 * coordinates, partial writes and seek-without-growth behavior remain.
 *
 * Views borrow the FILE pointer. Finish using them before explicitly
 * closing an owning parent, even though they retain the parent object.
 **/


static void g_mime_stream_file_class_init (GMimeStreamFileClass *klass);
static void g_mime_stream_file_init (GMimeStreamFile *stream, GMimeStreamFileClass *klass);
static void g_mime_stream_file_finalize (GObject *object);

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
g_mime_stream_file_get_type (void)
{
	static GType type = 0;
	
	if (!type) {
		static const GTypeInfo info = {
			sizeof (GMimeStreamFileClass),
			NULL, /* base_class_init */
			NULL, /* base_class_finalize */
			(GClassInitFunc) g_mime_stream_file_class_init,
			NULL, /* class_finalize */
			NULL, /* class_data */
			sizeof (GMimeStreamFile),
			0,    /* n_preallocs */
			(GInstanceInitFunc) g_mime_stream_file_init,
		};
		
		type = g_type_register_static (GMIME_TYPE_STREAM, "GMimeStreamFile", &info, 0);
	}
	
	return type;
}


static void
g_mime_stream_file_class_init (GMimeStreamFileClass *klass)
{
	GMimeStreamClass *stream_class = GMIME_STREAM_CLASS (klass);
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	
	parent_class = g_type_class_ref (GMIME_TYPE_STREAM);
	
	object_class->finalize = g_mime_stream_file_finalize;
	
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
g_mime_stream_file_init (GMimeStreamFile *stream, GMimeStreamFileClass *klass)
{
	stream->owner = TRUE;
	stream->fp = NULL;
	stream->cached_eos = FALSE;
}

static void
g_mime_stream_file_finalize (GObject *object)
{
	GMimeStream *stream = (GMimeStream *) object;
	
	stream_close (stream);
	
	G_OBJECT_CLASS (parent_class)->finalize (object);
}


static gboolean
check_open (GMimeStream *stream)
{
	if (GMIME_STREAM_FILE (stream)->fp == NULL) {
		errno = EBADF;
		return FALSE;
	}

	if (stream->bound_start < 0 || stream->bound_end < -1 ||
	    (stream->bound_end != -1 && stream->bound_end < stream->bound_start) ||
	    stream->position < stream->bound_start ||
	    (stream->bound_end != -1 && stream->position > stream->bound_end)) {
		errno = EINVAL;
		return FALSE;
	}

	return TRUE;
}

static int
stdio_error (void)
{
	/* ISO C reports failures through return values and ferror(), but does
	 * not require every stdio implementation to supply an errno value. */
	if (errno == 0)
		errno = EIO;

	return -1;
}

static int
seek_file (GMimeStreamFile *fstream, gint64 offset, int whence)
{
	/* fseek takes long, which may be narrower than our logical position. */
	if ((gint64) (long) offset != offset) {
		errno = EOVERFLOW;
		return -1;
	}

	errno = 0;
	/* Any nonzero fseek result is a failure, not just -1. */
	if (fseek (fstream->fp, (long) offset, whence) != 0)
		return stdio_error ();

	return 0;
}

static ssize_t
stream_read (GMimeStream *stream, char *buf, size_t len)
{
	GMimeStreamFile *fstream = (GMimeStreamFile *) stream;
	size_t nread;
	
	if (!check_open (stream))
		return -1;

	if (len > G_MAXSSIZE) {
		errno = EOVERFLOW;
		return -1;
	}
	
	/* A finite end is EOF without moving the FILE pointer for a sibling. */
	if (stream->bound_end != -1 && stream->position == stream->bound_end) {
		fstream->cached_eos = TRUE;
		return 0;
	}
	
	if (stream->bound_end != -1)
		len = (size_t) MIN (stream->bound_end - stream->position, (gint64) len);
	
	if (len > (guint64) (G_MAXINT64 - stream->position)) {
		errno = EOVERFLOW;
		return -1;
	}

	/* Do not short-circuit local EOF: the file may have grown since then. */
	if (seek_file (fstream, stream->position, SEEK_SET) == -1)
		return -1;
	
	/* The shared FILE's indicators belong to its previous operation, which
	 * may have been performed by another view. Observe this attempt alone. */
	clearerr (fstream->fp);
	errno = 0;
	nread = fread (buf, 1, len, fstream->fp);
	if (nread > 0)
		stream->position += nread;

	if (ferror (fstream->fp)) {
		stdio_error ();
		if (nread == 0)
			return -1;
	} else if (nread == 0) {
		fstream->cached_eos = TRUE;
	}
	
	return (ssize_t) nread;
}

static ssize_t
stream_write (GMimeStream *stream, const char *buf, size_t len)
{
	GMimeStreamFile *fstream = (GMimeStreamFile *) stream;
	size_t nwritten;
	
	if (!check_open (stream))
		return -1;

	if (len > G_MAXSSIZE) {
		errno = EOVERFLOW;
		return -1;
	}
	
	if (stream->bound_end != -1 && stream->position >= stream->bound_end) {
		errno = EINVAL;
		return -1;
	}
	
	if (stream->bound_end != -1)
		len = (size_t) MIN (stream->bound_end - stream->position, (gint64) len);
	
	/* Clip first so a fitting native partial write need not overflow. */
	if (len > (guint64) (G_MAXINT64 - stream->position)) {
		errno = EOVERFLOW;
		return -1;
	}

	if (seek_file (fstream, stream->position, SEEK_SET) == -1)
		return -1;

	clearerr (fstream->fp);
	errno = 0;
	nwritten = fwrite (buf, 1, len, fstream->fp);
	if (nwritten > 0)
		stream->position += nwritten;

	if (ferror (fstream->fp)) {
		stdio_error ();
		if (nwritten == 0)
			return -1;
	}
	
	return (ssize_t) nwritten;
}

static int
stream_flush (GMimeStream *stream)
{
	GMimeStreamFile *fstream = (GMimeStreamFile *) stream;
	
	if (!check_open (stream))
		return -1;
	
	errno = 0;
	return fflush (fstream->fp) == 0 ? 0 : stdio_error ();
}

static int
stream_close (GMimeStream *stream)
{
	GMimeStreamFile *fstream = (GMimeStreamFile *) stream;
	FILE *fp;
	
	if (fstream->fp == NULL)
		return 0;
	
	/* fclose invalidates the FILE even on failure. Detach before calling
	 * it so a later close or finalizer cannot use the released pointer. */
	fp = fstream->fp;
	fstream->fp = NULL;
	if (!fstream->owner)
		return 0;
	
	errno = 0;
	return fclose (fp) == 0 ? 0 : stdio_error ();
}

static gboolean
stream_eos (GMimeStream *stream)
{
	GMimeStreamFile *fstream = (GMimeStreamFile *) stream;
	
	if (fstream->fp == NULL)
		return TRUE;
	
	return fstream->cached_eos;
}

static int
stream_reset (GMimeStream *stream)
{
	GMimeStreamFile *fstream = (GMimeStreamFile *) stream;
	
	if (!check_open (stream))
		return -1;
	
	if (stream->position != stream->bound_start &&
	    seek_file (fstream, stream->bound_start, SEEK_SET) == -1)
		return -1;

	fstream->cached_eos = FALSE;
	
	return 0;
}

static gint64
stream_seek (GMimeStream *stream, gint64 offset, GMimeSeekWhence whence)
{
	GMimeStreamFile *fstream = (GMimeStreamFile *) stream;
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
		if (offset > 0 || (stream->bound_end == -1 && !fstream->cached_eos)) {
			/* Query physical EOF before checked addition. Other views'
			 * stdio operations must not change our cached seek origin. */
			if (seek_file (fstream, 0, SEEK_END) == -1)
				return -1;
			errno = 0;
			if ((origin = ftell (fstream->fp)) == -1)
				return stdio_error ();
		} else if (stream->bound_end == -1) {
			/* seeking backwards from eos (which happens
			 * to be our current position) */
			origin = stream->position;
		} else {
			/* seeking backwards from a known position */
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
	
	/* sanity check the resultant offset */
	if (real < stream->bound_start) {
		errno = EINVAL;
		return -1;
	}

	/* Same-position seeks retain the view's observed EOF, like MimeKit. */
	if (real == stream->position)
		return real;
	
	if (stream->bound_end != -1 && real > stream->bound_end) {
		errno = EINVAL;
		return -1;
	}
	
	if (seek_file (fstream, real, SEEK_SET) == -1)
		return -1;

	if ((stream->bound_end != -1 && real < stream->bound_end) ||
	    (fstream->cached_eos && real < stream->position))
		fstream->cached_eos = FALSE;
	
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
	GMimeStreamFile *fstream = (GMimeStreamFile *) stream;
	gint64 bound_end;
	
	if (!check_open (stream))
		return -1;
	
	if (stream->bound_end != -1)
		return stream->bound_end - stream->bound_start;
	
	if (seek_file (fstream, 0, SEEK_END) == -1)
		return -1;
	
	errno = 0;
	bound_end = ftell (fstream->fp);
	if (bound_end == -1)
		return stdio_error ();
	
	if (seek_file (fstream, stream->position, SEEK_SET) == -1)
		return -1;
	
	return MAX (0, bound_end - stream->bound_start);
}

static GMimeStream *
stream_substream (GMimeStream *stream, gint64 start, gint64 end)
{
	GMimeStreamFile *fstream;

	if (!check_open (stream))
		return NULL;
	
	fstream = g_object_new (GMIME_TYPE_STREAM_FILE, NULL);
	g_mime_stream_construct ((GMimeStream *) fstream, start, end);
	fstream->fp = ((GMimeStreamFile *) stream)->fp;
	fstream->owner = FALSE;
	
	return (GMimeStream *) fstream;
}


/**
 * g_mime_stream_file_new:
 * @fp: a FILE pointer
 *
 * Creates a new #GMimeStreamFile object around @fp.
 *
 * Note: The created #GMimeStreamFile object will own the FILE pointer
 * passed in.
 *
 * Returns: a stream using @fp.
 **/
GMimeStream *
g_mime_stream_file_new (FILE *fp)
{
	gint64 start;
	
	g_return_val_if_fail (fp != NULL, NULL);
	
#ifdef G_OS_WIN32
	_setmode (_fileno (fp), O_BINARY);
#endif
	
	if ((start = ftell (fp)) == -1)
		start = 0;
	
	return g_mime_stream_file_new_with_bounds (fp, start, -1);
}


/**
 * g_mime_stream_file_new_with_bounds:
 * @fp: a FILE pointer
 * @start: start boundary
 * @end: end boundary
 *
 * Creates a new #GMimeStreamFile object around @fp with bounds @start
 * and @end.
 *
 * Note: The created #GMimeStreamFile object will own the FILE pointer
 * passed in.
 *
 * Returns: a stream using @fp with bounds @start and @end.
 **/
GMimeStream *
g_mime_stream_file_new_with_bounds (FILE *fp, gint64 start, gint64 end)
{
	GMimeStreamFile *fstream;
	
	g_return_val_if_fail (fp != NULL, NULL);
	
#ifdef G_OS_WIN32
	_setmode (_fileno (fp), O_BINARY);
#endif
	
	fstream = g_object_new (GMIME_TYPE_STREAM_FILE, NULL);
	g_mime_stream_construct ((GMimeStream *) fstream, start, end);
	fstream->owner = TRUE;
	fstream->fp = fp;
	
	return (GMimeStream *) fstream;
}


/**
 * g_mime_stream_file_open:
 * @path: the path to a file
 * @mode: as in fopen(3)
 * @err: a #GError
 *
 * Creates a new #GMimeStreamFile object for the specified @path.
 *
 * Returns: (transfer full): a stream using for reading and/or writing to the
 * specified file path or %NULL on error.
 **/
GMimeStream *
g_mime_stream_file_open (const char *path, const char *mode, GError **err)
{
	FILE *fp;
	
	g_return_val_if_fail (path != NULL, NULL);
	g_return_val_if_fail (mode != NULL, NULL);
	
	if (!(fp = fopen (path, mode))) {
		g_set_error (err, GMIME_ERROR, errno, "Failed to open `%s': %s", path, g_strerror (errno));
		return NULL;
	}
	
	return g_mime_stream_file_new (fp);
}


/**
 * g_mime_stream_file_get_owner:
 * @stream: a #GMimeStreamFile
 *
 * Gets whether or not @stream owns the backend FILE pointer.
 *
 * Returns: %TRUE if @stream owns the backend FILE pointer or %FALSE
 * otherwise.
 **/
gboolean
g_mime_stream_file_get_owner (GMimeStreamFile *stream)
{
	g_return_val_if_fail (GMIME_IS_STREAM_FILE (stream), FALSE);
	
	return stream->owner;
}


/**
 * g_mime_stream_file_set_owner:
 * @stream: a #GMimeStreamFile
 * @owner: %TRUE if this stream should own the FILE pointer or %FALSE otherwise
 *
 * Sets whether or not @stream owns the backend FILE pointer.
 *
 * Note: @owner should be %TRUE if the stream should fclose() the
 * backend FILE pointer when destroyed or %FALSE otherwise.
 **/
void
g_mime_stream_file_set_owner (GMimeStreamFile *stream, gboolean owner)
{
	g_return_if_fail (GMIME_IS_STREAM_FILE (stream));
	
	stream->owner = owner;
}
