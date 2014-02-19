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

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>

#include "gmime-stream-gio.h"


/**
 * SECTION: gmime-stream-gio
 * @title: GMimeStreamGIO
 * @short_description: A wrapper for GLib's GIO streams
 * @see_also: #GMimeStream
 *
 * A simple #GMimeStream implementation that sits on top of GLib's GIO
 * input and output streams.
 **/


static void g_mime_stream_gio_class_init (GMimeStreamGIOClass *klass);
static void g_mime_stream_gio_init (GMimeStreamGIO *stream, GMimeStreamGIOClass *klass);
static void g_mime_stream_gio_finalize (GObject *object);

static ssize_t stream_read (GMimeStream *stream, char *buf, size_t len);
static ssize_t stream_write (GMimeStream *stream, const char *buf, size_t len);
static int stream_flush (GMimeStream *stream);
static int stream_close (GMimeStream *stream);
static gboolean stream_eos (GMimeStream *stream);
static int stream_reset (GMimeStream *stream);
static gint64 stream_seek (GMimeStream *stream, gint64 ofgioet, GMimeSeekWhence whence);
static gint64 stream_tell (GMimeStream *stream);
static gint64 stream_length (GMimeStream *stream);
static GMimeStream *stream_substream (GMimeStream *stream, gint64 start, gint64 end);


static GMimeStreamClass *parent_class = NULL;


GType
g_mime_stream_gio_get_type (void)
{
	static GType type = 0;
	
	if (!type) {
		static const GTypeInfo info = {
			sizeof (GMimeStreamGIOClass),
			NULL, /* base_class_init */
			NULL, /* base_class_finalize */
			(GClassInitFunc) g_mime_stream_gio_class_init,
			NULL, /* class_finalize */
			NULL, /* class_data */
			sizeof (GMimeStreamGIO),
			0,    /* n_preallocs */
			(GInstanceInitFunc) g_mime_stream_gio_init,
		};
		
		type = g_type_register_static (GMIME_TYPE_STREAM, "GMimeStreamGIO", &info, 0);
	}
	
	return type;
}


static void
g_mime_stream_gio_class_init (GMimeStreamGIOClass *klass)
{
	GMimeStreamClass *stream_class = GMIME_STREAM_CLASS (klass);
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	
	parent_class = g_type_class_ref (GMIME_TYPE_STREAM);
	
	object_class->finalize = g_mime_stream_gio_finalize;
	
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
g_mime_stream_gio_init (GMimeStreamGIO *stream, GMimeStreamGIOClass *klass)
{
	stream->ostream = NULL;
	stream->istream = NULL;
	stream->file = NULL;
	
	stream->owner = TRUE;
	stream->eos = FALSE;
}

static void
g_mime_stream_gio_finalize (GObject *object)
{
	GMimeStreamGIO *gio = (GMimeStreamGIO *) object;
	
	if (gio->istream) {
		g_input_stream_close (gio->istream, NULL, NULL);
		g_object_unref (gio->istream);
	}
	
	if (gio->ostream) {
		g_output_stream_close (gio->ostream, NULL, NULL);
		g_object_unref (gio->ostream);
	}
	
	if (gio->owner && gio->file)
		g_object_unref (gio->file);
	
	G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
set_errno (GError *err)
{
	if (!err) {
		errno = 0;
		return;
	}
	
	switch (err->code) {
	case G_IO_ERROR_NOT_FOUND: errno = ENOENT; break;
	case G_IO_ERROR_EXISTS: errno = EEXIST; break;
	case G_IO_ERROR_IS_DIRECTORY: errno = EISDIR; break;
	case G_IO_ERROR_NOT_DIRECTORY: errno = ENOTDIR; break;
	case G_IO_ERROR_NOT_EMPTY: errno = ENOTEMPTY; break;
	case G_IO_ERROR_FILENAME_TOO_LONG: errno = ENAMETOOLONG; break;
	case G_IO_ERROR_TOO_MANY_LINKS: errno = EMLINK; break;
	case G_IO_ERROR_NO_SPACE: errno = ENOSPC; break; // or ENOMEM
	case G_IO_ERROR_INVALID_ARGUMENT: errno = EINVAL; break;
	case G_IO_ERROR_PERMISSION_DENIED: errno = EACCES; break; // or EPERM
#ifdef ENOTSUP
	case G_IO_ERROR_NOT_SUPPORTED: errno = ENOTSUP; break;
#endif
#ifdef ECANCELED
	case G_IO_ERROR_CANCELLED: errno = ECANCELED; break;
#endif
	case G_IO_ERROR_READ_ONLY: errno = EROFS; break;
#ifdef ETIMEDOUT
	case G_IO_ERROR_TIMED_OUT: errno = ETIMEDOUT; break;
#endif
	case G_IO_ERROR_BUSY: errno = EBUSY; break;
	case G_IO_ERROR_WOULD_BLOCK: errno = EAGAIN; break;
	case G_IO_ERROR_FAILED:
	default:
		errno = EIO;
		break;
	}
	
	g_error_free (err);
}

static ssize_t
stream_read (GMimeStream *stream, char *buf, size_t len)
{
	GMimeStreamGIO *gio = (GMimeStreamGIO *) stream;
	GError *err = NULL;
	ssize_t nread;
	
	if (gio->file == NULL) {
		errno = EBADF;
		return -1;
	}
	
	if (gio->istream == NULL) {
		/* try opening an input stream */
		if (!(gio->istream = (GInputStream *) g_file_read (gio->file, NULL, &err))) {
			set_errno (err);
			return -1;
		}
	}
	
	if (stream->bound_end != -1 && stream->position >= stream->bound_end) {
		errno = EINVAL;
		return -1;
	}
	
	if (stream->bound_end != -1)
		len = (size_t) MIN (stream->bound_end - stream->position, (gint64) len);
	
	/* make sure we are at the right position */
	if (G_IS_SEEKABLE (gio->istream)) {
		GSeekable *seekable = (GSeekable *) gio->istream;
		
		if (!g_seekable_seek (seekable, stream->position, G_SEEK_SET, NULL, &err)) {
			set_errno (err);
			return -1;
		}
	}
	
	if ((nread = g_input_stream_read (gio->istream, buf, len, NULL, &err)) < 0) {
		set_errno (err);
		return -1;
	}
	
	if (nread > 0)
		stream->position += nread;
	else if (nread == 0)
		gio->eos = TRUE;
	
	return nread;
}

static ssize_t
stream_write (GMimeStream *stream, const char *buf, size_t len)
{
	GMimeStreamGIO *gio = (GMimeStreamGIO *) stream;
	size_t nwritten = 0;
	GError *err = NULL;
	
	if (gio->file == NULL) {
		errno = EBADF;
		return -1;
	}
	
	if (gio->ostream == NULL) {
		/* try opening an output stream */
		if (!(gio->ostream = (GOutputStream *) g_file_append_to (gio->file, G_FILE_CREATE_NONE, NULL, &err))) {
			set_errno (err);
			return -1;
		}
	}
	
	if (stream->bound_end != -1 && stream->position >= stream->bound_end) {
		errno = EINVAL;
		return -1;
	}
	
	if (stream->bound_end != -1)
		len = (size_t) MIN (stream->bound_end - stream->position, (gint64) len);
	
	/* make sure we are at the right position */
	if (G_IS_SEEKABLE (gio->ostream)) {
		GSeekable *seekable = (GSeekable *) gio->ostream;
		
		if (!g_seekable_seek (seekable, stream->position, G_SEEK_SET, NULL, &err)) {
			set_errno (err);
			return -1;
		}
	}
	
	if (!g_output_stream_write_all (gio->ostream, buf, len, &nwritten, NULL, &err)) {
		set_errno (err);
		gio->eos = TRUE;
		
		if (nwritten == 0) {
			/* nothing was written, return error */
			return -1;
		}
		
		errno = 0;
	}
	
	if (nwritten > 0)
		stream->position += nwritten;
	
	return nwritten;
}

static int
stream_flush (GMimeStream *stream)
{
	GMimeStreamGIO *gio = (GMimeStreamGIO *) stream;
	GError *err = NULL;
	
	if (gio->file == NULL) {
		errno = EBADF;
		return -1;
	}
	
	if (gio->ostream && !g_output_stream_flush (gio->ostream, NULL, &err)) {
		set_errno (err);
		return -1;
	}
	
	return 0;
}

static int
stream_close (GMimeStream *stream)
{
	GMimeStreamGIO *gio = (GMimeStreamGIO *) stream;
	
	if (gio->istream) {
		g_input_stream_close (gio->istream, NULL, NULL);
		g_object_unref (gio->istream);
		gio->istream = NULL;
	}
	
	if (gio->ostream) {
		g_output_stream_close (gio->ostream, NULL, NULL);
		g_object_unref (gio->ostream);
		gio->ostream = NULL;
	}
	
	if (gio->owner && gio->file)
		g_object_unref (gio->file);
	
	gio->file = NULL;
	
	return 0;
}

static gboolean
stream_eos (GMimeStream *stream)
{
	GMimeStreamGIO *gio = (GMimeStreamGIO *) stream;
	
	if (gio->file == NULL)
		return TRUE;
	
	return gio->eos;
}

static int
stream_reset (GMimeStream *stream)
{
	GMimeStreamGIO *gio = (GMimeStreamGIO *) stream;
	GError *err = NULL;
	
	if (gio->file == NULL) {
		errno = EBADF;
		return -1;
	}
	
	if (stream->position == stream->bound_start) {
		gio->eos = FALSE;
		return 0;
	}
	
	if (gio->istream != NULL) {
		/* reset the input stream */
		if (!G_IS_SEEKABLE (gio->istream)) {
			errno = EINVAL;
			return -1;
		}
		
		if (!g_seekable_seek ((GSeekable *) gio->istream, stream->bound_start, G_SEEK_SET, NULL, &err)) {
			set_errno (err);
			return -1;
		}
	}
	
	if (gio->ostream != NULL) {
		/* reset the output stream */
		if (!G_IS_SEEKABLE (gio->ostream)) {
			errno = EINVAL;
			return -1;
		}
		
		if (!g_seekable_seek ((GSeekable *) gio->ostream, stream->bound_start, G_SEEK_SET, NULL, &err)) {
			set_errno (err);
			return -1;
		}
	}
	
	gio->eos = FALSE;
	
	return 0;
}

static gint64
gio_seekable_seek (GMimeStream *stream, GSeekable *seekable, gint64 offset, GMimeSeekWhence whence)
{
	GMimeStreamGIO *gio = (GMimeStreamGIO *) stream;
	gboolean need_seek = TRUE;
	GError *err = NULL;
	gint64 real;
	
	switch (whence) {
	case GMIME_STREAM_SEEK_SET:
		real = offset;
		break;
	case GMIME_STREAM_SEEK_CUR:
		real = stream->position + offset;
		break;
	case GMIME_STREAM_SEEK_END:
		if (offset > 0 || (stream->bound_end == -1 && !gio->eos)) {
			/* need to do an actual lseek() here because
			 * we either don't know the offset of the end
			 * of the stream and/or don't know if we can
			 * seek past the end */
			if (!g_seekable_seek (seekable, offset, G_SEEK_END, NULL, &err)) {
				set_errno (err);
				return -1;
			}
			
			need_seek = FALSE;
			real = offset;
		} else if (gio->eos && stream->bound_end == -1) {
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
	
	if (need_seek && !g_seekable_seek (seekable, real, G_SEEK_SET, NULL, &err)) {
		set_errno (err);
		return -1;
	}
	
	return real;
}

static gint64
stream_seek (GMimeStream *stream, gint64 offset, GMimeSeekWhence whence)
{
	GMimeStreamGIO *gio = (GMimeStreamGIO *) stream;
	gint64 real;
	
	if (gio->file == NULL) {
		errno = EBADF;
		return -1;
	}
	
	/* if either of our streams are unseekable, fail */
	if ((gio->istream != NULL && !G_IS_SEEKABLE (gio->istream)) ||
	    (gio->ostream != NULL && !G_IS_SEEKABLE (gio->ostream))) {
		errno = EINVAL;
		return -1;
	}
	
	if (gio->istream || gio->ostream) {
		gint64 outreal = -1;
		gint64 inreal = -1;
		
		if (gio->istream) {
			/* seek on our input stream */
			if ((inreal = gio_seekable_seek (stream, (GSeekable *) gio->istream, offset, whence)) == -1)
				return -1;
			
			if (gio->ostream == NULL)
				outreal = inreal;
		}
		
		if (gio->ostream) {
			/* seek on our output stream */
			if ((outreal = gio_seekable_seek (stream, (GSeekable *) gio->istream, offset, whence)) == -1)
				return -1;
			
			if (gio->istream == NULL)
				inreal = outreal;
		}
		
		if (outreal != inreal) {
		}
		
		real = outreal;
	} else {
		/* no streams yet opened... */
		switch (whence) {
		case GMIME_STREAM_SEEK_SET:
			real = offset;
			break;
		case GMIME_STREAM_SEEK_CUR:
			real = stream->position + offset;
			break;
		case GMIME_STREAM_SEEK_END:
			real = stream->bound_end + offset;
			break;
		default:
			g_assert_not_reached ();
			return -1;
		}
		
		/* check that we haven't seekend beyond bound_end */
		if (stream->bound_end != -1 && real > stream->bound_end) {
			errno = EINVAL;
			return -1;
		}
		
		/* check that we are within the starting bounds */
		if (real < stream->bound_start) {
			errno = EINVAL;
			return -1;
		}
	}
	
	/* reset eos if appropriate */
	if ((stream->bound_end != -1 && real < stream->bound_end) ||
	    (gio->eos && real < stream->position))
		gio->eos = FALSE;
	
	stream->position = real;
	
	return real;
}

static gint64
stream_tell (GMimeStream *stream)
{
	GMimeStreamGIO *gio = (GMimeStreamGIO *) stream;
	
	if (gio->file == NULL) {
		errno = EBADF;
		return -1;
	}
	
	return stream->position;
}

static gint64
gio_seekable_bound_end (GMimeStream *stream, GSeekable *seekable)
{
	GError *err = NULL;
	gint64 bound_end;
	
	if (!g_seekable_seek (seekable, 0, G_SEEK_END, NULL, &err)) {
		set_errno (err);
		return -1;
	}
	
	bound_end = g_seekable_tell (seekable);
	if (bound_end < stream->bound_start) {
		errno = EINVAL;
		return -1;
	}
	
	if (!g_seekable_seek (seekable, stream->position, G_SEEK_SET, NULL, &err)) {
		set_errno (err);
		return -1;
	}
	
	return bound_end;
}

static gint64
stream_length (GMimeStream *stream)
{
	GMimeStreamGIO *gio = (GMimeStreamGIO *) stream;
	gint64 bound_end;
	
	if (gio->file == NULL) {
		errno = EBADF;
		return -1;
	}
	
	if (stream->bound_end != -1)
		return stream->bound_end - stream->bound_start;
	
	if (gio->istream && G_IS_SEEKABLE (gio->istream)) {
		if ((bound_end = gio_seekable_bound_end (stream, (GSeekable *) gio->istream)) == -1)
			return -1;
	} else if (gio->ostream && G_IS_SEEKABLE (gio->ostream)) {
		if ((bound_end = gio_seekable_bound_end (stream, (GSeekable *) gio->ostream)) == -1)
			return -1;
	} else if (!gio->istream && !gio->ostream) {
		/* try opening an input stream to get the length */
		if (!(gio->istream = (GInputStream *) g_file_read (gio->file, NULL, NULL))) {
			errno = EINVAL;
			return -1;
		}
		
		if ((bound_end = gio_seekable_bound_end (stream, (GSeekable *) gio->istream)) == -1)
			return -1;
	} else {
		/* neither of our streams is seekable, can't get the length */
		errno = EINVAL;
		return -1;
	}
	
	return bound_end - stream->bound_start;
}

static GMimeStream *
stream_substream (GMimeStream *stream, gint64 start, gint64 end)
{
	GMimeStreamGIO *gio;
	
	gio = g_object_newv (GMIME_TYPE_STREAM_GIO, 0, NULL);
	g_mime_stream_construct (GMIME_STREAM (gio), start, end);
	gio->file = GMIME_STREAM_GIO (stream)->file;
	gio->owner = FALSE;
	gio->eos = FALSE;
	
	return (GMimeStream *) gio;
}


/**
 * g_mime_stream_gio_new:
 * @file: a #GFile
 *
 * Creates a new #GMimeStreamGIO wrapper around a #GFile object.
 *
 * Returns: (transfer full): a stream using @file.
 **/
GMimeStream *
g_mime_stream_gio_new (GFile *file)
{
	GMimeStreamGIO *gio;
	
	g_return_val_if_fail (G_IS_FILE (file), NULL);
	
	gio = g_object_newv (GMIME_TYPE_STREAM_GIO, 0, NULL);
	g_mime_stream_construct (GMIME_STREAM (gio), 0, -1);
	gio->file = file;
	gio->owner = TRUE;
	gio->eos = FALSE;
	
	return (GMimeStream *) gio;
}


/**
 * g_mime_stream_gio_new_with_bounds:
 * @file: a #GFile
 * @start: start boundary
 * @end: end boundary
 *
 * Creates a new #GMimeStreamGIO stream around a #GFile with bounds
 * @start and @end.
 *
 * Returns: (transfer full): a stream using @file with bounds @start
 * and @end.
 **/
GMimeStream *
g_mime_stream_gio_new_with_bounds (GFile *file, gint64 start, gint64 end)
{
	GMimeStreamGIO *gio;
	
	g_return_val_if_fail (G_IS_FILE (file), NULL);
	
	gio = g_object_newv (GMIME_TYPE_STREAM_GIO, 0, NULL);
	g_mime_stream_construct (GMIME_STREAM (gio), start, end);
	gio->file = file;
	gio->owner = TRUE;
	gio->eos = FALSE;
	
	return (GMimeStream *) gio;
}


/**
 * g_mime_stream_gio_get_owner:
 * @stream: a #GMimeStreamGIO stream
 *
 * Gets whether or not @stream owns the backend #GFile.
 *
 * Returns: %TRUE if @stream owns the backend #GFile or %FALSE
 * otherwise.
 **/
gboolean
g_mime_stream_gio_get_owner (GMimeStreamGIO *stream)
{
	g_return_val_if_fail (GMIME_IS_STREAM_GIO (stream), FALSE);
	
	return stream->owner;
}


/**
 * g_mime_stream_gio_set_owner:
 * @stream: a #GMimeStreamGIO stream
 * @owner: %TRUE if this stream should own the #GFile or %FALSE otherwise
 *
 * Sets whether or not @stream owns the backend GIO pointer.
 *
 * Note: @owner should be %TRUE if the stream should close() the
 * backend file descriptor when destroyed or %FALSE otherwise.
 **/
void
g_mime_stream_gio_set_owner (GMimeStreamGIO *stream, gboolean owner)
{
	g_return_if_fail (GMIME_IS_STREAM_GIO (stream));
	
	stream->owner = owner;
}
