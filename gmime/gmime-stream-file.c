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

#ifdef G_OS_WIN32
#include <fcntl.h>
#include <io.h>
#endif
#include <errno.h>

#include "gmime-stream-file.h"


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
}

static void
g_mime_stream_file_finalize (GObject *object)
{
	GMimeStreamFile *stream = (GMimeStreamFile *) object;
	
	if (stream->owner && stream->fp)
		fclose (stream->fp);
	
	G_OBJECT_CLASS (parent_class)->finalize (object);
}


static ssize_t
stream_read (GMimeStream *stream, char *buf, size_t len)
{
	GMimeStreamFile *fstream = (GMimeStreamFile *) stream;
	size_t nread;
	
	if (fstream->fp == NULL) {
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
	fseek (fstream->fp, (long) stream->position, SEEK_SET);
	
	if ((nread = fread (buf, 1, len, fstream->fp)) > 0)
		stream->position += nread;
	
	return (ssize_t) nread;
}

static ssize_t
stream_write (GMimeStream *stream, const char *buf, size_t len)
{
	GMimeStreamFile *fstream = (GMimeStreamFile *) stream;
	size_t nwritten;
	
	if (fstream->fp == NULL) {
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
	fseek (fstream->fp, (long) stream->position, SEEK_SET);

	if ((nwritten = fwrite (buf, 1, len, fstream->fp)) > 0)
		stream->position += nwritten;
	
	return (ssize_t) nwritten;
}

static int
stream_flush (GMimeStream *stream)
{
	GMimeStreamFile *fstream = (GMimeStreamFile *) stream;
	
	if (fstream->fp == NULL) {
		errno = EBADF;
		return -1;
	}
	
	return fflush (fstream->fp);
}

static int
stream_close (GMimeStream *stream)
{
	GMimeStreamFile *fstream = (GMimeStreamFile *) stream;
	int rv;
	
	if (fstream->fp == NULL)
		return 0;
	
	if ((rv = fclose (fstream->fp)) != 0)
		fstream->fp = NULL;
	
	return rv;
}

static gboolean
stream_eos (GMimeStream *stream)
{
	GMimeStreamFile *fstream = (GMimeStreamFile *) stream;
	
	if (fstream->fp == NULL)
		return TRUE;
	
	if (stream->bound_end == -1)
		return feof (fstream->fp) ? TRUE : FALSE;
	else
		return stream->position >= stream->bound_end;
}

static int
stream_reset (GMimeStream *stream)
{
	GMimeStreamFile *fstream = (GMimeStreamFile *) stream;
	
	if (fstream->fp == NULL) {
		errno = EBADF;
		return -1;
	}
	
	if (stream->position == stream->bound_start)
		return 0;
	
	if (fseek (fstream->fp, (long) stream->bound_start, SEEK_SET) == -1)
		return -1;
	
	return 0;
}

static gint64
stream_seek (GMimeStream *stream, gint64 offset, GMimeSeekWhence whence)
{
	GMimeStreamFile *fstream = (GMimeStreamFile *) stream;
	gint64 real = stream->position;
	FILE *fp = fstream->fp;
	
	if (fstream->fp == NULL) {
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
		if (offset > 0 || (stream->bound_end == -1 && !feof (fp))) {
			/* need to do an actual lseek() here because
			 * we either don't know the offset of the end
			 * of the stream and/or don't know if we can
			 * seek past the end */
			if (fseek (fp, (long) offset, SEEK_END) == -1 || (real = ftell (fp)) == -1)
				return -1;
		} else if (feof (fp) && stream->bound_end == -1) {
			/* seeking backwards from eos (which happens
			 * to be our current position) */
			real = stream->position + offset;
		} else {
			/* seeking backwards from a known position */
			real = stream->bound_end + offset;
		}
		break;
	}
	
	/* sanity check the resultant offset */
	if (real < stream->bound_start) {
		errno = EINVAL;
		return -1;
	}
	
	if (stream->bound_end != -1 && real > stream->bound_end) {
		errno = EINVAL;
		return -1;
	}
	
	if (fseek (fp, (long) real, SEEK_SET) == -1 || (real = ftell (fp)) == -1)
		return -1;
	
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
	GMimeStreamFile *fstream = (GMimeStreamFile *) stream;
	gint64 bound_end;
	
	if (fstream->fp == NULL) {
		errno = EBADF;
		return -1;
	}
	
	if (stream->bound_start != -1 && stream->bound_end != -1)
		return stream->bound_end - stream->bound_start;
	
	fseek (fstream->fp, (long) 0, SEEK_END);
	bound_end = ftell (fstream->fp);
	fseek (fstream->fp, (long) stream->position, SEEK_SET);
	
	if (bound_end < stream->bound_start) {
		errno = EINVAL;
		return -1;
	}
	
	return bound_end - stream->bound_start;
}

static GMimeStream *
stream_substream (GMimeStream *stream, gint64 start, gint64 end)
{
	GMimeStreamFile *fstream;
	
	fstream = g_object_newv (GMIME_TYPE_STREAM_FILE, 0, NULL);
	g_mime_stream_construct (GMIME_STREAM (fstream), start, end);
	fstream->owner = FALSE;
	fstream->fp = GMIME_STREAM_FILE (stream)->fp;
	
	return GMIME_STREAM (fstream);
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
	GMimeStreamFile *fstream;
	gint64 start;
	
	g_return_val_if_fail (fp != NULL, NULL);
	
#ifdef G_OS_WIN32
	_setmode (_fileno (fp), O_BINARY);
#endif
	
	if ((start = ftell (fp)) == -1)
		start = 0;
	
	fstream = g_object_newv (GMIME_TYPE_STREAM_FILE, 0, NULL);
	g_mime_stream_construct (GMIME_STREAM (fstream), start, -1);
	fstream->owner = TRUE;
	fstream->fp = fp;
	
	return GMIME_STREAM (fstream);
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
	
	fstream = g_object_newv (GMIME_TYPE_STREAM_FILE, 0, NULL);
	g_mime_stream_construct (GMIME_STREAM (fstream), start, end);
	fstream->owner = TRUE;
	fstream->fp = fp;
	
	return GMIME_STREAM (fstream);
}


/**
 * g_mime_stream_file_new_for_path:
 * @path: the path to a file
 * @mode: as in fopen(3)
 *
 * Creates a new #GMimeStreamFile object for the specified @path.
 *
 * Returns: a stream using for reading and/or writing to the specified
 * file path or %NULL on error.
 *
 * Since: 2.6.18
 **/
GMimeStream *
g_mime_stream_file_new_for_path (const char *path, const char *mode)
{
	FILE *fp;
	
	g_return_val_if_fail (path != NULL, NULL);
	g_return_val_if_fail (mode != NULL, NULL);
	
	if (!(fp = fopen (path, mode)))
		return NULL;
	
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
