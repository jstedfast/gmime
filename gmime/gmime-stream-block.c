/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*  GMime
 *  Copyright (C) 2026 Jeffrey Stedfast
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
#include <string.h>

#include "gmime-stream-block.h"
#include "gmime-error.h"

/**
 * SECTION: gmime-stream-block
 * @title: GMimeStreamBlock
 * @short_description: Non-contiguous memory streams
 * @see_also: #GMimeStreamMem
 *
 * Ports MimeKit's MemoryBlockStream using 2048-byte blocks. Growing the
 * stream does not copy previously stored bytes. Seeking cannot extend the
 * stream; use g_mime_stream_block_set_length() to grow it before seeking.
 * New blocks are zero-initialized rather than rented from a managed pool.
 *
 * Native bounds and substreams use absolute coordinates, including SEEK_SET.
 * Substreams share storage and length but have independent positions and
 * bounds. Closing one view does not close another. Reset rewinds without
 * discarding content. Nonempty I/O, position, length, reset and flush fail
 * with EBADF after close; native zero-length I/O remains a no-op.
 *
 * The backing length is limited to G_MAXINT * 2048 bytes. As in MimeKit,
 * writes ending at that limit are rejected, although setting length accepts
 * it. Allocation follows GLib's normal allocation policy. The optional
 * contiguous copy is limited to G_MAXUINT bytes.
 **/

#define BLOCK_SIZE 2048
#define MAX_CAPACITY ((gint64) G_MAXINT * BLOCK_SIZE)

struct _GMimeStreamBlockPrivate {
	grefcount refs;
	GPtrArray *blocks;
	gint64 length;
};

static void stream_finalize (GObject *object);
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

G_DEFINE_TYPE (GMimeStreamBlock, g_mime_stream_block, GMIME_TYPE_STREAM)

static void
g_mime_stream_block_class_init (GMimeStreamBlockClass *klass)
{
	GMimeStreamClass *stream_class = GMIME_STREAM_CLASS (klass);

	G_OBJECT_CLASS (klass)->finalize = stream_finalize;
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
g_mime_stream_block_init (GMimeStreamBlock *stream)
{
	stream->priv = g_new0 (struct _GMimeStreamBlockPrivate, 1);
	g_ref_count_init (&stream->priv->refs);
	stream->priv->blocks = g_ptr_array_new_with_free_func (g_free);
	g_mime_stream_construct (GMIME_STREAM (stream), 0, -1);
}

static gboolean
check_open (GMimeStream *stream)
{
	if (!GMIME_STREAM_BLOCK (stream)->priv) {
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

static gint64
get_end (GMimeStream *stream)
{
	gint64 end = GMIME_STREAM_BLOCK (stream)->priv->length;

	return stream->bound_end == -1 ? end : MIN (end, stream->bound_end);
}

static void
ensure_capacity (struct _GMimeStreamBlockPrivate *priv, gint64 length)
{
	while ((gint64) priv->blocks->len * BLOCK_SIZE < length)
		g_ptr_array_add (priv->blocks, g_malloc0 (BLOCK_SIZE));
}

static void
copy_bytes (struct _GMimeStreamBlockPrivate *priv, gint64 position, char *buf, size_t len)
{
	guint block = (guint) (position / BLOCK_SIZE);
	guint offset = (guint) (position % BLOCK_SIZE);
	size_t n;

	while (len > 0) {
		n = MIN (BLOCK_SIZE - offset, len);
		memcpy (buf, (char *) priv->blocks->pdata[block++] + offset, n);
		offset = 0;
		buf += n;
		len -= n;
	}
}

static void
stream_finalize (GObject *object)
{
	stream_close (GMIME_STREAM (object));
	G_OBJECT_CLASS (g_mime_stream_block_parent_class)->finalize (object);
}

static ssize_t
stream_read (GMimeStream *stream, char *buf, size_t len)
{
	gint64 end;
	size_t n;

	if (!check_open (stream))
		return -1;

	end = get_end (stream);
	if (stream->position >= end)
		return 0;

	n = (size_t) MIN ((gint64) MIN (len, (size_t) G_MAXSSIZE), end - stream->position);
	copy_bytes (GMIME_STREAM_BLOCK (stream)->priv, stream->position, buf, n);
	stream->position += n;

	return (ssize_t) n;
}

static ssize_t
stream_write (GMimeStream *stream, const char *buf, size_t len)
{
	struct _GMimeStreamBlockPrivate *priv = GMIME_STREAM_BLOCK (stream)->priv;
	guint block, offset;
	size_t n, written = 0;

	if (!check_open (stream))
		return -1;

	/* Validate before allocating or changing content, including the source's
	 * exclusive write limit even though SetLength accepts MaxCapacity. */
	if (len > G_MAXSSIZE || stream->position >= MAX_CAPACITY ||
	    len >= (guint64) (MAX_CAPACITY - stream->position)) {
		errno = EOVERFLOW;
		return -1;
	}

	if (stream->bound_end != -1) {
		if (stream->position >= stream->bound_end) {
			errno = EINVAL;
			return -1;
		}

		len = (size_t) MIN ((gint64) len, stream->bound_end - stream->position);
	}

	ensure_capacity (priv, stream->position + len);
	block = (guint) (stream->position / BLOCK_SIZE);
	offset = (guint) (stream->position % BLOCK_SIZE);
	while (written < len) {
		n = MIN (BLOCK_SIZE - offset, len - written);
		memcpy ((char *) priv->blocks->pdata[block++] + offset, buf + written, n);
		offset = 0;
		written += n;
	}

	stream->position += written;
	priv->length = MAX (priv->length, stream->position);

	return (ssize_t) written;
}

static int
stream_flush (GMimeStream *stream)
{
	/* Memory blocks have no buffered output to flush. */
	return check_open (stream) ? 0 : -1;
}

static int
stream_close (GMimeStream *stream)
{
	GMimeStreamBlock *block = GMIME_STREAM_BLOCK (stream);

	if (block->priv) {
		if (g_ref_count_dec (&block->priv->refs)) {
			g_ptr_array_unref (block->priv->blocks);
			g_free (block->priv);
		}

		block->priv = NULL;
	}

	return 0;
}

static gboolean
stream_eos (GMimeStream *stream)
{
	return !GMIME_STREAM_BLOCK (stream)->priv || stream->position >= get_end (stream);
}

static int
stream_reset (GMimeStream *stream)
{
	return check_open (stream) ? 0 : -1;
}

static gint64
stream_seek (GMimeStream *stream, gint64 offset, GMimeSeekWhence whence)
{
	gint64 origin, real;

	if (!check_open (stream))
		return -1;

	switch (whence) {
	case GMIME_STREAM_SEEK_SET: origin = 0; break;
	case GMIME_STREAM_SEEK_CUR: origin = stream->position; break;
	case GMIME_STREAM_SEEK_END: origin = MAX (stream->bound_start, get_end (stream)); break;
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
	if (real < stream->bound_start || real > MAX_CAPACITY) {
		errno = EINVAL;
		return -1;
	}

	/* Short-cut seeking to the current position before checking the extent:
	 * another view may have shortened the shared backing store. */
	if (real == stream->position)
		return real;

	if (real > MAX (stream->bound_start, get_end (stream))) {
		errno = EINVAL;
		return -1;
	}

	stream->position = real;

	return real;
}

static gint64
stream_tell (GMimeStream *stream)
{
	return check_open (stream) ? stream->position : -1;
}

static gint64
stream_length (GMimeStream *stream)
{
	return check_open (stream) ? MAX (0, get_end (stream) - stream->bound_start) : -1;
}

static GMimeStream *
stream_substream (GMimeStream *stream, gint64 start, gint64 end)
{
	GMimeStreamBlock *sub;

	if (!check_open (stream))
		return NULL;

	if (start < stream->bound_start || start > MAX_CAPACITY || end < -1 ||
	    (end != -1 && (end < start || end > MAX_CAPACITY)) ||
	    (stream->bound_end != -1 &&
	     (start > stream->bound_end || (end != -1 && end > stream->bound_end)))) {
		errno = EINVAL;
		return NULL;
	}

	sub = GMIME_STREAM_BLOCK (g_mime_stream_block_new ());
	stream_close (GMIME_STREAM (sub));
	sub->priv = GMIME_STREAM_BLOCK (stream)->priv;
	g_ref_count_inc (&sub->priv->refs);
	if (end == -1 && stream->bound_end != -1)
		end = stream->bound_end;
	g_mime_stream_construct (GMIME_STREAM (sub), start, end);

	return GMIME_STREAM (sub);
}

/**
 * g_mime_stream_block_new:
 *
 * Creates an empty memory stream backed by 2048-byte blocks.
 *
 * Returns: (transfer full): a new #GMimeStreamBlock.
 **/
GMimeStream *
g_mime_stream_block_new (void)
{
	return g_object_new (GMIME_TYPE_STREAM_BLOCK, NULL);
}

/**
 * g_mime_stream_block_set_length:
 * @stream: a #GMimeStreamBlock
 * @length: new length, relative to the stream's start bound
 *
 * Resizes the shared backing store to the start bound plus @length.
 * Growing exposes zero-initialized bytes. Shrinking releases unneeded blocks
 * and clears the truncated tail before it can be exposed again. The caller's
 * position is clamped to the new end; other views retain their positions.
 * A finite end bound is updated to the new end.
 *
 * Returns: %0 on success or %-1 on error, setting errno to EBADF for a closed
 * stream, EINVAL for negative length or EOVERFLOW for an excessive extent.
 **/
int
g_mime_stream_block_set_length (GMimeStreamBlock *stream, gint64 length)
{
	struct _GMimeStreamBlockPrivate *priv;
	GMimeStream *base;
	gint64 end, capacity;
	guint block, offset, count;

	g_return_val_if_fail (GMIME_IS_STREAM_BLOCK (stream), -1);

	base = GMIME_STREAM (stream);
	if (!check_open (base))
		return -1;

	if (length < 0) {
		errno = EINVAL;
		return -1;
	}

	if (base->bound_start > MAX_CAPACITY || length > MAX_CAPACITY - base->bound_start) {
		errno = EOVERFLOW;
		return -1;
	}

	priv = stream->priv;
	end = base->bound_start + length;
	capacity = (gint64) priv->blocks->len * BLOCK_SIZE;
	if (end > capacity) {
		ensure_capacity (priv, end);
	} else if (end < priv->length) {
		/* Shed unused blocks, retaining the boundary block so its discarded
		 * bytes can be cleared before a later extension reuses them. */
		while (capacity - end > BLOCK_SIZE) {
			g_ptr_array_remove_index (priv->blocks, priv->blocks->len - 1);
			capacity -= BLOCK_SIZE;
		}

		count = (guint) (MIN (priv->length, capacity) - end);
		block = (guint) (end / BLOCK_SIZE);
		offset = (guint) (end % BLOCK_SIZE);
		memset ((char *) priv->blocks->pdata[block] + offset, 0, count);
	}

	priv->length = end;
	base->position = MIN (base->position, end);
	if (base->bound_end != -1)
		base->bound_end = end;

	return 0;
}

/**
 * g_mime_stream_block_to_byte_array:
 * @stream: a #GMimeStreamBlock
 * @error: return location for a #GError, or %NULL
 *
 * Copies the bytes within the stream's bounds into an independent contiguous
 * array without changing position. The caller owns the returned copy.
 *
 * Returns: (transfer full) (nullable): the copied bytes, or %NULL on error,
 * setting errno and @error. Errors use %GMIME_ERROR with errno codes,
 * including EBADF after close and EOVERFLOW above G_MAXUINT bytes.
 **/
GByteArray *
g_mime_stream_block_to_byte_array (GMimeStreamBlock *stream, GError **error)
{
	GByteArray *array;
	gint64 length;

	g_return_val_if_fail (GMIME_IS_STREAM_BLOCK (stream), NULL);
	g_return_val_if_fail (error == NULL || *error == NULL, NULL);

	length = stream_length (GMIME_STREAM (stream));
	if (length == -1 || (guint64) length > G_MAXUINT) {
		int errsv = length == -1 ? errno : EOVERFLOW;

		/* Bindings marshal a NULL byte array like an empty array, so report
		 * failures through GError as well as the native errno contract. */
		g_set_error (error, GMIME_ERROR, errsv, "Cannot copy memory block stream: %s", g_strerror (errsv));
		errno = errsv;
		return NULL;
	}

	array = g_byte_array_sized_new ((guint) length);
	g_byte_array_set_size (array, (guint) length);
	copy_bytes (stream->priv, GMIME_STREAM (stream)->bound_start, (char *) array->data, array->len);

	return array;
}
