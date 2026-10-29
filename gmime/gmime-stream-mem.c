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
#include <errno.h>

#include "gmime-stream-mem.h"


#include "gmime-object-private.h"

/**
 * SECTION: gmime-stream-mem
 * @title: GMimeStreamMem
 * @short_description: A memory-backed stream
 * @see_also: #GMimeStream
 *
 * A simple #GMimeStream implementation that uses a memory buffer for
 * storage. Substreams retain the shared byte array independently of their
 * parent. Closing or replacing a parent's buffer does not invalidate them.
 * Logical positions and bounds remain independent.
 *
 * A finite length describes the declared range, even if the buffer is
 * shorter. Reads stop at the actual allocation; writes grow it safely.
 * Bounded writes retain native partial-write behavior at the end bound.
 **/


static void g_mime_stream_mem_class_init (GMimeStreamMemClass *klass);
static void g_mime_stream_mem_init (GMimeStreamMem *stream);
static void g_mime_stream_mem_finalize (GObject *object);

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



G_DEFINE_TYPE_WITH_PRIVATE (GMimeStreamMem, g_mime_stream_mem, GMIME_TYPE_STREAM)

GMimeStreamMemPrivate *
_g_mime_stream_mem_get_private (GMimeStreamMem *object)
{
	return g_mime_stream_mem_get_instance_private (object);
}


static void
g_mime_stream_mem_class_init (GMimeStreamMemClass *klass)
{
	GMimeStreamClass *stream_class = GMIME_STREAM_CLASS (klass);
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	
	parent_class = g_type_class_ref (GMIME_TYPE_STREAM);
	
	object_class->finalize = g_mime_stream_mem_finalize;
	
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
g_mime_stream_mem_init (GMimeStreamMem *stream)
{
	GMimeStreamMemPrivate *priv = _g_mime_stream_mem_get_private (stream);

	priv->owner = TRUE;
	priv->buffer = NULL;
	priv->shared_buffer = NULL;
}

static void
g_mime_stream_mem_finalize (GObject *object)
{
	GMimeStream *stream = (GMimeStream *) object;
	
	stream_close (stream);
	
	G_OBJECT_CLASS (parent_class)->finalize (object);
}


static ssize_t
stream_read (GMimeStream *stream, char *buf, size_t len)
{
	GMimeStreamPrivate *stream_priv;
	GMimeStreamMem *mem = (GMimeStreamMem *) stream;
	GMimeStreamMemPrivate *mem_priv = _g_mime_stream_mem_get_private (mem);
	gint64 bound_end;
	size_t n;

	if (mem_priv->buffer == NULL) {
		errno = EBADF;
		return -1;
	}
	
	stream_priv = _g_mime_stream_get_private (stream);

	if (stream_priv->bound_start < 0 || stream_priv->position < stream_priv->bound_start ||
	    stream_priv->bound_end < -1 ||
	    (stream_priv->bound_end != -1 && stream_priv->bound_end < stream_priv->bound_start)) {
		errno = EINVAL;
		return -1;
	}

	/* The declared range may extend past the current allocation, including
	 * after another view shrinks the shared byte array. Never read that tail. */
	bound_end = mem_priv->buffer->len;
	if (stream_priv->bound_end != -1)
		bound_end = MIN (bound_end, stream_priv->bound_end);

	if (stream_priv->position >= bound_end)
		return 0;

	n = (size_t) MIN ((gint64) MIN (len, (size_t) G_MAXSSIZE), bound_end - stream_priv->position);
	memcpy (buf, mem_priv->buffer->data + (gsize) stream_priv->position, n);
	stream_priv->position += n;

	return (ssize_t) n;
}

static void
grow_buffer (GByteArray *buffer, guint length)
{
	guint previous = buffer->len;

	if (length > previous) {
		g_byte_array_set_size (buffer, length);
		/* Seeking or writing a view beyond existing data must not expose
		 * uninitialized bytes in the gap. */
		memset (buffer->data + previous, 0, length - previous);
	}
}

static ssize_t
stream_write (GMimeStream *stream, const char *buf, size_t len)
{
	GMimeStreamPrivate *stream_priv;
	GMimeStreamMem *mem = (GMimeStreamMem *) stream;
	GMimeStreamMemPrivate *mem_priv = _g_mime_stream_mem_get_private (mem);
	gint64 end;

	if (mem_priv->buffer == NULL) {
		errno = EBADF;
		return -1;
	}
	
	stream_priv = _g_mime_stream_get_private (stream);

	if (stream_priv->bound_start < 0 || stream_priv->position < stream_priv->bound_start ||
	    stream_priv->bound_end < -1 ||
	    (stream_priv->bound_end != -1 && stream_priv->bound_end < stream_priv->bound_start)) {
		errno = EINVAL;
		return -1;
	}

	if (len > G_MAXSSIZE) {
		errno = EOVERFLOW;
		return -1;
	}

	if (stream_priv->bound_end != -1) {
		if (stream_priv->position > stream_priv->bound_end) {
			errno = EINVAL;
			return -1;
		}
		if (stream_priv->position == stream_priv->bound_end)
			return 0;
		len = (size_t) MIN ((gint64) len, stream_priv->bound_end - stream_priv->position);
	}

	/* Check the accepted count before narrowing it to GByteArray's guint. */
	if (stream_priv->position > G_MAXUINT || len > G_MAXUINT - (guint64) stream_priv->position) {
		errno = EOVERFLOW;
		return -1;
	}

	end = stream_priv->position + (gint64) len;
	grow_buffer (mem_priv->buffer, (guint) end);
	memcpy (mem_priv->buffer->data + (gsize) stream_priv->position, buf, len);
	stream_priv->position = end;

	return (ssize_t) len;
}

static int
stream_flush (GMimeStream *stream)
{
	GMimeStreamMem *mem = (GMimeStreamMem *) stream;
	GMimeStreamMemPrivate *priv = _g_mime_stream_mem_get_private (mem);

	if (priv->buffer == NULL) {
		errno = EBADF;
		return -1;
	}
	
	return 0;
}

static int
stream_close (GMimeStream *stream)
{
	GMimeStreamMem *mem = (GMimeStreamMem *) stream;
	
	/* A substream's retained reference is separate from the public ownership
	 * policy. Release it once, even if the caller also enables owner. */
	GMimeStreamMemPrivate *priv = _g_mime_stream_mem_get_private (mem);

	if (priv->owner && priv->buffer && priv->buffer != priv->shared_buffer)
		g_byte_array_unref (priv->buffer);

	g_clear_pointer (&priv->shared_buffer, g_byte_array_unref);
	priv->buffer = NULL;
	
	return 0;
}

static gboolean
stream_eos (GMimeStream *stream)
{
	GMimeStreamPrivate *stream_priv;
	GMimeStreamMem *mem = (GMimeStreamMem *) stream;
	GMimeStreamMemPrivate *mem_priv = _g_mime_stream_mem_get_private (mem);
	gint64 bound_end;

	if (mem_priv->buffer == NULL)
		return TRUE;
	
	bound_end = mem_priv->buffer->len;
	stream_priv = _g_mime_stream_get_private (stream);

	if (stream_priv->bound_end != -1)
		bound_end = MIN (bound_end, stream_priv->bound_end);
	
	return stream_priv->position >= bound_end;
}

static int
stream_reset (GMimeStream *stream)
{
	GMimeStreamMem *mem = (GMimeStreamMem *) stream;
	GMimeStreamMemPrivate *priv = _g_mime_stream_mem_get_private (mem);

	if (priv->buffer == NULL) {
		errno = EBADF;
		return -1;
	}
	
	return 0;
}

static gint64
stream_seek (GMimeStream *stream, gint64 offset, GMimeSeekWhence whence)
{
	GMimeStreamPrivate *stream_priv;
	GMimeStreamMem *mem = (GMimeStreamMem *) stream;
	GMimeStreamMemPrivate *mem_priv = _g_mime_stream_mem_get_private (mem);
	gint64 origin, bound_end, real;

	if (mem_priv->buffer == NULL) {
		errno = EBADF;
		return -1;
	}
	
	stream_priv = _g_mime_stream_get_private (stream);

	if (stream_priv->bound_start < 0 || stream_priv->bound_end < -1 ||
	    (stream_priv->bound_end != -1 && stream_priv->bound_end < stream_priv->bound_start)) {
		errno = EINVAL;
		return -1;
	}

	bound_end = stream_priv->bound_end != -1 ? stream_priv->bound_end : (gint64) mem_priv->buffer->len;
	
	switch (whence) {
	case GMIME_STREAM_SEEK_SET:
		origin = 0;
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

	if ((offset > 0 && origin > G_MAXINT64 - offset) ||
	    (offset < 0 && origin < G_MININT64 - offset)) {
		errno = EOVERFLOW;
		return -1;
	}

	real = origin + offset;

	if (real < stream_priv->bound_start) {
		errno = EINVAL;
		return -1;
	}
	
	if (stream_priv->bound_end != -1 && real > bound_end) {
		errno = EINVAL;
		return -1;
	}
	
	/* Preserve no-op seeks when another view has shortened the storage. */
	if (real == stream_priv->position)
		return real;

	if (stream_priv->bound_end == -1 && real > bound_end) {
		if (real > G_MAXUINT) {
			errno = EOVERFLOW;
			return -1;
		}

		grow_buffer (mem_priv->buffer, (guint) real);
	}
	
	stream_priv->position = real;
	
	return stream_priv->position;
}

static gint64
stream_tell (GMimeStream *stream)
{
	GMimeStreamMem *mem = (GMimeStreamMem *) stream;
	GMimeStreamMemPrivate *mem_priv = _g_mime_stream_mem_get_private (mem);
	GMimeStreamPrivate *stream_priv;

	if (mem_priv->buffer == NULL) {
		errno = EBADF;
		return -1;
	}
	
	stream_priv = _g_mime_stream_get_private (stream);

	return stream_priv->position;
}

static gint64
stream_length (GMimeStream *stream)
{
	GMimeStreamMem *mem = GMIME_STREAM_MEM (stream);
	GMimeStreamMemPrivate *mem_priv = _g_mime_stream_mem_get_private (mem);
	GMimeStreamPrivate *stream_priv;
	gint64 bound_end;

	if (mem_priv->buffer == NULL) {
		errno = EBADF;
		return -1;
	}

	stream_priv = _g_mime_stream_get_private (stream);

	if (stream_priv->bound_start < 0 || stream_priv->bound_end < -1 ||
	    (stream_priv->bound_end != -1 && stream_priv->bound_end < stream_priv->bound_start)) {
		errno = EINVAL;
		return -1;
	}

	bound_end = stream_priv->bound_end != -1 ? stream_priv->bound_end : (gint64) mem_priv->buffer->len;
	
	return MAX (0, bound_end - stream_priv->bound_start);
}

static GMimeStream *
stream_substream (GMimeStream *stream, gint64 start, gint64 end)
{
	GMimeStreamMem *parent = GMIME_STREAM_MEM (stream);
	GMimeStreamMemPrivate *parent_priv = _g_mime_stream_mem_get_private (parent);
	GMimeStreamMemPrivate *mem_priv;
	GMimeStreamMem *mem;

	if (!parent_priv->buffer) {
		errno = EBADF;
		return NULL;
	}

	mem = g_object_new (GMIME_TYPE_STREAM_MEM, NULL);
	g_mime_stream_construct ((GMimeStream *) mem, start, end);
	/* Retain the array, not just the parent object: close and replacement can
	 * release the parent's own reference while a view is still alive. */
	mem_priv = _g_mime_stream_mem_get_private (mem);

	mem_priv->buffer = g_byte_array_ref (parent_priv->buffer);
	mem_priv->shared_buffer = mem_priv->buffer;
	mem_priv->owner = FALSE;
	
	return (GMimeStream *) mem;
}


/**
 * g_mime_stream_mem_new:
 *
 * Creates a new #GMimeStreamMem object.
 *
 * Returns: a new memory stream.
 **/
GMimeStream *
g_mime_stream_mem_new (void)
{
	GMimeStreamMemPrivate *priv;
	GMimeStreamMem *mem;
	
	mem = g_object_new (GMIME_TYPE_STREAM_MEM, NULL);
	g_mime_stream_construct ((GMimeStream *) mem, 0, -1);
	priv = _g_mime_stream_mem_get_private (mem);

	priv->buffer = g_byte_array_new ();
	priv->owner = TRUE;
	
	return (GMimeStream *) mem;
}


/**
 * g_mime_stream_mem_new_with_byte_array:
 * @array: source data
 *
 * Creates a new #GMimeStreamMem with data @array.
 *
 * Returns: a new memory stream using @array.
 **/
GMimeStream *
g_mime_stream_mem_new_with_byte_array (GByteArray *array)
{
	GMimeStreamMemPrivate *priv;
	GMimeStreamMem *mem;

	g_return_val_if_fail (array != NULL, NULL);

	mem = g_object_new (GMIME_TYPE_STREAM_MEM, NULL);
	g_mime_stream_construct ((GMimeStream *) mem, 0, -1);
	priv = _g_mime_stream_mem_get_private (mem);

	priv->buffer = array;
	priv->owner = TRUE;
	
	return (GMimeStream *) mem;
}


/**
 * g_mime_stream_mem_new_with_buffer:
 * @buffer: (array length=len) (element-type guint8): stream data
 * @len: buffer length
 *
 * Creates a new #GMimeStreamMem object and initializes the stream
 * contents with the first @len bytes of @buffer.
 *
 * Returns: (nullable): a new memory stream initialized with @buffer,
 * or %NULL with errno set to EOVERFLOW if @len exceeds G_MAXUINT.
 **/
GMimeStream *
g_mime_stream_mem_new_with_buffer (const char *buffer, size_t len)
{
	GMimeStreamMemPrivate *priv;
	GMimeStreamMem *mem;

	g_return_val_if_fail (buffer != NULL, NULL);

	if (len > G_MAXUINT) {
		errno = EOVERFLOW;
		return NULL;
	}

	mem = (GMimeStreamMem *) g_mime_stream_mem_new ();
	priv = _g_mime_stream_mem_get_private (mem);

	g_byte_array_append (priv->buffer, (unsigned char *) buffer, len);
	
	return (GMimeStream *) mem;
}


/**
 * g_mime_stream_mem_get_byte_array:
 * @mem: a #GMimeStreamMem
 *
 * Gets the byte array from the memory stream.
 *
 * Returns: (transfer none): the byte array from the memory stream.
 **/
GByteArray *
g_mime_stream_mem_get_byte_array (GMimeStreamMem *mem)
{
	GMimeStreamMemPrivate *priv;

	g_return_val_if_fail (GMIME_IS_STREAM_MEM (mem), NULL);
	
	priv = _g_mime_stream_mem_get_private (mem);

	return priv->buffer;
}


/**
 * g_mime_stream_mem_set_byte_array:
 * @mem: a #GMimeStreamMem
 * @array: stream data
 *
 * Sets the byte array on the memory stream.
 * Existing substreams continue to use the previous array. Reassigning the
 * same array is safe and still resets the position and bounds.
 *
 * Note: The memory stream is not responsible for freeing the byte
 * array. Use g_mime_stream_mem_set_owner() to change this behavior.
 * A substream retains its internal storage reference on self-assignment.
 **/
void
g_mime_stream_mem_set_byte_array (GMimeStreamMem *mem, GByteArray *array)
{
	GMimeStream *stream = (GMimeStream *) mem;
	GMimeStreamMemPrivate *mem_priv;
	GMimeStreamPrivate *stream_priv;
	
	g_return_if_fail (GMIME_IS_STREAM_MEM (mem));
	g_return_if_fail (array != NULL);
	
	/* Self-assignment hands an owned array back to the caller without freeing
	 * it. A substream still keeps its independent storage reference. */
	mem_priv = _g_mime_stream_mem_get_private (mem);

	if (mem_priv->buffer != array)
		stream_close (stream);
	
	mem_priv->buffer = array;
	mem_priv->owner = FALSE;
	
	stream_priv = _g_mime_stream_get_private (stream);

	stream_priv->position = 0;
	stream_priv->bound_start = 0;
	stream_priv->bound_end = -1;
}


/**
 * g_mime_stream_mem_get_owner:
 * @mem: a #GMimeStreamMem
 *
 * Gets whether or not @mem owns the backend memory buffer.
 *
 * Returns: %TRUE if @mem owns the backend memory buffer or %FALSE
 * otherwise.
 **/
gboolean
g_mime_stream_mem_get_owner (GMimeStreamMem *mem)
{
	GMimeStreamMemPrivate *priv;

	g_return_val_if_fail (GMIME_IS_STREAM_MEM (mem), FALSE);
	
	priv = _g_mime_stream_mem_get_private (mem);

	return priv->owner;
}


/**
 * g_mime_stream_mem_set_owner:
 * @mem: a #GMimeStreamMem
 * @owner: %TRUE if this stream should own the #GByteArray or %FALSE otherwise
 *
 * Sets whether or not @mem owns the backend memory buffer.
 *
 * Note: @owner should be %TRUE if the stream should free the backend
 * memory buffer when closed or destroyed or %FALSE otherwise. Substreams
 * independently retain and release a reference to their shared array,
 * regardless of this setting; enabling ownership does not release it twice.
 **/
void
g_mime_stream_mem_set_owner (GMimeStreamMem *mem, gboolean owner)
{
	GMimeStreamMemPrivate *priv;

	g_return_if_fail (GMIME_IS_STREAM_MEM (mem));
	
	priv = _g_mime_stream_mem_get_private (mem);

	priv->owner = owner;
}
