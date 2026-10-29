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

#include <errno.h>

#include "gmime-stream-null.h"

#include "gmime-object-private.h"

/**
 * SECTION: gmime-stream-null
 * @title: GMimeStreamNull
 * @short_description: A null stream
 * @see_also: #GMimeStream
 *
 * A #GMimeStream which has no real backing storage at all. This
 * stream is useful for dry-runs and can also be useful for
 * determining statistics on source data which can be written to
 * streams but cannot be read as a stream itself (e.g. a #GMimeEntity
 * via g_mime_entity_write_to_stream()).
 *
 * Logical length tracks the highest written position, independently of
 * cumulative byte and newline statistics. Reading is not supported.
 **/


static void g_mime_stream_null_class_init (GMimeStreamNullClass *klass);
static void g_mime_stream_null_init (GMimeStreamNull *stream);
static void g_mime_stream_null_finalize (GObject *object);

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



G_DEFINE_TYPE_WITH_PRIVATE (GMimeStreamNull, g_mime_stream_null, GMIME_TYPE_STREAM)

GMimeStreamNullPrivate *
_g_mime_stream_null_get_private (GMimeStreamNull *object)
{
	return g_mime_stream_null_get_instance_private (object);
}


static void
g_mime_stream_null_class_init (GMimeStreamNullClass *klass)
{
	GMimeStreamClass *stream_class = GMIME_STREAM_CLASS (klass);
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	
	parent_class = g_type_class_ref (GMIME_TYPE_STREAM);
	
	object_class->finalize = g_mime_stream_null_finalize;
	
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
g_mime_stream_null_init (GMimeStreamNull *stream)
{
	GMimeStreamNullPrivate *priv = _g_mime_stream_null_get_private (stream);

	priv->written = 0;
	priv->newlines = 0;
	priv->count_newlines = FALSE;
	priv->measured_end = 0;
}

static void
g_mime_stream_null_finalize (GObject *object)
{
	G_OBJECT_CLASS (parent_class)->finalize (object);
}

/**
 * g_mime_stream_null_get_written:
 * @stream: a #GMimeStreamNull
 *
 * Returns: cumulative bytes written since the last reset.
 **/
size_t
g_mime_stream_null_get_written (GMimeStreamNull *stream)
{
	GMimeStreamNullPrivate *priv;

	g_return_val_if_fail (GMIME_IS_STREAM_NULL (stream), 0);

	priv = _g_mime_stream_null_get_private (stream);

	return priv->written;
}

/**
 * g_mime_stream_null_get_newlines:
 * @stream: a #GMimeStreamNull
 *
 * Returns: cumulative LF bytes counted since the last reset. Counting must
 * be enabled with g_mime_stream_null_set_count_newlines().
 **/
size_t
g_mime_stream_null_get_newlines (GMimeStreamNull *stream)
{
	GMimeStreamNullPrivate *priv;

	g_return_val_if_fail (GMIME_IS_STREAM_NULL (stream), 0);

	priv = _g_mime_stream_null_get_private (stream);

	return priv->newlines;
}


static ssize_t
stream_read (GMimeStream *stream, char *buf, size_t len)
{
	errno = EBADF;
	return -1;
}

static ssize_t
stream_write (GMimeStream *stream, const char *buf, size_t len)
{
	GMimeStreamNull *null = (GMimeStreamNull *) stream;
	GMimeStreamNullPrivate *priv = _g_mime_stream_null_get_private (null);
	GMimeStreamPrivate *stream_priv = _g_mime_stream_get_private (stream);
	size_t i, newlines = priv->newlines;

	if (stream_priv->bound_end != -1 && stream_priv->position >= stream_priv->bound_end) {
		errno = EINVAL;
		return -1;
	}

	if (len > G_MAXSSIZE) {
		errno = EOVERFLOW;
		return -1;
	}

	if (stream_priv->bound_end != -1)
		len = (size_t) MIN ((gint64) len, stream_priv->bound_end - stream_priv->position);

	if ((guint64) len > (guint64) (G_MAXINT64 - stream_priv->position) ||
	    len > G_MAXSIZE - priv->written) {
		errno = EOVERFLOW;
		return -1;
	}

	/* Statistics count accepted bytes, including overwrites. Check them
	 * before committing the position or logical length of the stream. */
	if (priv->count_newlines) {
		for (i = 0; i < len; i++) {
			if (buf[i] == '\n') {
				if (newlines == G_MAXSIZE) {
					errno = EOVERFLOW;
					return -1;
				}
				newlines++;
			}
		}
	}

	priv->newlines = newlines;
	priv->written += len;
	stream_priv->position += len;

	/* Seeking backwards and overwriting must not inflate the measured size. */
	priv->measured_end = MAX (priv->measured_end, stream_priv->position);

	return len;
}

static int
stream_flush (GMimeStream *stream)
{
	return 0;
}

static int
stream_close (GMimeStream *stream)
{
	return 0;
}

static gboolean
stream_eos (GMimeStream *stream)
{
	return TRUE;
}

static int
stream_reset (GMimeStream *stream)
{
	GMimeStreamNull *null = (GMimeStreamNull *) stream;
	GMimeStreamNullPrivate *priv = _g_mime_stream_null_get_private (null);
	GMimeStreamPrivate *stream_priv = _g_mime_stream_get_private (stream);

	priv->written = 0;
	priv->newlines = 0;
	priv->measured_end = stream_priv->bound_start;
	
	return 0;
}

static gint64
stream_seek (GMimeStream *stream, gint64 offset, GMimeSeekWhence whence)
{
	GMimeStreamNull *null = (GMimeStreamNull *) stream;
	GMimeStreamPrivate *stream_priv = _g_mime_stream_get_private (stream);
	GMimeStreamNullPrivate *priv = _g_mime_stream_null_get_private (null);
	gint64 bound_end, origin, real;

	bound_end = stream_priv->bound_end != -1 ? stream_priv->bound_end :
		MAX (priv->measured_end, stream_priv->bound_start);

	switch (whence) {
	case GMIME_STREAM_SEEK_SET:
		origin = stream_priv->bound_start;
		break;
	case GMIME_STREAM_SEEK_END:
		origin = bound_end;
		break;
	case GMIME_STREAM_SEEK_CUR:
		origin = stream_priv->position;
		break;
	default:
		errno = EINVAL;
		return -1;
	}

	/* Origins are nonnegative; negative offsets cannot underflow gint64. */
	if (offset > 0 && origin > G_MAXINT64 - offset) {
		errno = EOVERFLOW;
		return -1;
	}
	real = origin + offset;

	/* Sanity check the resultant offset rather than silently clamping it. */
	if (real < stream_priv->bound_start || real > bound_end) {
		errno = EINVAL;
		return -1;
	}

	return stream_priv->position = real;
}

static gint64
stream_tell (GMimeStream *stream)
{
	GMimeStreamPrivate *priv = _g_mime_stream_get_private (stream);

	return priv->position;
}

static gint64
stream_length (GMimeStream *stream)
{
	GMimeStreamNull *null = (GMimeStreamNull *) stream;
	GMimeStreamPrivate *stream_priv = _g_mime_stream_get_private (stream);
	GMimeStreamNullPrivate *priv = _g_mime_stream_null_get_private (null);
	gint64 bound_end;

	bound_end = stream_priv->bound_end != -1 ? stream_priv->bound_end :
		MAX (priv->measured_end, stream_priv->bound_start);
	
	return bound_end - stream_priv->bound_start;
}

static GMimeStream *
stream_substream (GMimeStream *stream, gint64 start, gint64 end)
{
	GMimeStreamNullPrivate *source_priv = _g_mime_stream_null_get_private (GMIME_STREAM_NULL (stream));
	GMimeStreamPrivate *stream_priv;
	GMimeStreamNullPrivate *priv;
	GMimeStream *null;
	
	null = g_object_new (GMIME_TYPE_STREAM_NULL, NULL);
	g_mime_stream_construct (null, start, end);
	priv = _g_mime_stream_null_get_private (GMIME_STREAM_NULL (null));

	stream_priv = _g_mime_stream_get_private (stream);

	priv->measured_end = stream_priv->bound_end != -1 ?
		stream_priv->bound_end : source_priv->measured_end;

	return null;
}


/**
 * g_mime_stream_null_new:
 *
 * Creates a new #GMimeStreamNull object.
 *
 * Returns: a new null stream (similar to /dev/null on Unix).
 **/
GMimeStream *
g_mime_stream_null_new (void)
{
	GMimeStream *null;
	
	null = g_object_new (GMIME_TYPE_STREAM_NULL, NULL);
	g_mime_stream_construct (null, 0, -1);
	
	return null;
}


/**
 * g_mime_stream_null_set_length:
 * @stream: a #GMimeStreamNull
 * @length: the desired measured length, in bytes
 *
 * Sets the logical length without writing bytes. The position is moved to
 * the new end only if it was beyond that end. Cumulative byte and newline
 * statistics are unchanged. For a bounded stream, the end bound is updated
 * to the new length relative to its start bound.
 *
 * Returns: %0 on success or %-1 on failure, with errno set to EINVAL for
 * a negative length or EOVERFLOW if the new end is not representable.
 **/
int
g_mime_stream_null_set_length (GMimeStreamNull *stream, gint64 length)
{
	GMimeStreamNullPrivate *stream_priv;
	GMimeStreamPrivate *base_priv;
	GMimeStream *base;
	gint64 end;

	g_return_val_if_fail (GMIME_IS_STREAM_NULL (stream), -1);

	base = (GMimeStream *) stream;
	if (length < 0) {
		errno = EINVAL;
		return -1;
	}

	base_priv = _g_mime_stream_get_private (base);

	if (length > G_MAXINT64 - base_priv->bound_start) {
		errno = EOVERFLOW;
		return -1;
	}

	end = base_priv->bound_start + length;
	base_priv->position = MIN (base_priv->position, end);
	stream_priv = _g_mime_stream_null_get_private (stream);

	stream_priv->measured_end = end;
	if (base_priv->bound_end != -1)
		base_priv->bound_end = end;

	return 0;
}


/**
 * g_mime_stream_null_set_count_newlines:
 * @stream: a #GMimeStreamNull
 * @count: %TRUE if newlines should be counted or %FALSE otherwise
 *
 * Sets whether or not the stream should keep track of the number of newlines
 * encountered.
 **/
void
g_mime_stream_null_set_count_newlines (GMimeStreamNull *stream, gboolean count)
{
	GMimeStreamNullPrivate *priv;

	g_return_if_fail (GMIME_IS_STREAM_NULL (stream));
	
	priv = _g_mime_stream_null_get_private (stream);

	priv->count_newlines = count;
}


/**
 * g_mime_stream_null_get_count_newlines:
 * @stream: a #GMimeStreamNull
 *
 * Gets whether or not the stream should keep track of the number of newlines
 * encountered.
 *
 * Returns: %TRUE if the stream should count the number of newlines or %FALSE otherwise.
 **/
gboolean
g_mime_stream_null_get_count_newlines (GMimeStreamNull *stream)
{
	GMimeStreamNullPrivate *priv;

	g_return_val_if_fail (GMIME_IS_STREAM_NULL (stream), FALSE);
	
	priv = _g_mime_stream_null_get_private (stream);

	return priv->count_newlines;
}
