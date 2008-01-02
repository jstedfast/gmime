/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*  GMime
 *  Copyright (C) 2000-2008 Jeffrey Stedfast
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

#include "gmime-stream-mem.h"


/**
 * SECTION: gmime-stream-mem
 * @title: GMimeStreamMem
 * @short_description: A memory-backed stream
 * @see_also: #GMimeStream
 *
 * A simple #GMimeStream implementation that uses a memory buffer for
 * storage.
 **/


static void g_mime_stream_mem_class_init (GMimeStreamMemClass *klass);
static void g_mime_stream_mem_init (GMimeStreamMem *stream, GMimeStreamMemClass *klass);
static void g_mime_stream_mem_finalize (GObject *object);

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
g_mime_stream_mem_get_type (void)
{
	static GType type = 0;
	
	if (!type) {
		static const GTypeInfo info = {
			sizeof (GMimeStreamMemClass),
			NULL, /* base_class_init */
			NULL, /* base_class_finalize */
			(GClassInitFunc) g_mime_stream_mem_class_init,
			NULL, /* class_finalize */
			NULL, /* class_data */
			sizeof (GMimeStreamMem),
			0,    /* n_preallocs */
			(GInstanceInitFunc) g_mime_stream_mem_init,
		};
		
		type = g_type_register_static (GMIME_TYPE_STREAM, "GMimeStreamMem", &info, 0);
	}
	
	return type;
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
g_mime_stream_mem_init (GMimeStreamMem *stream, GMimeStreamMemClass *klass)
{
	stream->owner = TRUE;
	stream->buffer = NULL;
}

static void
g_mime_stream_mem_finalize (GObject *object)
{
	GMimeStreamMem *stream = (GMimeStreamMem *) object;
	
	if (stream->owner && stream->buffer)
		g_byte_array_free (stream->buffer, TRUE);
	
	G_OBJECT_CLASS (parent_class)->finalize (object);
}


static ssize_t
stream_read (GMimeStream *stream, char *buf, size_t len)
{
	GMimeStreamMem *mem = (GMimeStreamMem *) stream;
	off_t bound_end;
	ssize_t n;
	
	g_return_val_if_fail (mem->buffer != NULL, -1);
	
	bound_end = stream->bound_end != -1 ? stream->bound_end : (off_t) mem->buffer->len;
	
	n = MIN (bound_end - stream->position, (off_t) len);
	if (n > 0) {
		memcpy (buf, mem->buffer->data + stream->position, n);
		stream->position += n;
	} else if (n < 0) {
		/* set errno?? */
		n = -1;
	}
	
	return n;
}

static ssize_t
stream_write (GMimeStream *stream, const char *buf, size_t len)
{
	GMimeStreamMem *mem = (GMimeStreamMem *) stream;
	off_t bound_end;
	ssize_t n;
	
	g_return_val_if_fail (mem->buffer != NULL, -1);
	
	if (stream->bound_end == -1 && stream->position + len > mem->buffer->len) {
		g_byte_array_set_size (mem->buffer, stream->position + len);
		bound_end = mem->buffer->len;
	} else
		bound_end = stream->bound_end;
	
	n = MIN (bound_end - stream->position, (off_t) len);
	if (n > 0) {
		memcpy (mem->buffer->data + stream->position, buf, n);
		stream->position += n;
	} else if (n < 0) {
		/* FIXME: set errno?? */
		n = -1;
	}
	
	return n;
}

static int
stream_flush (GMimeStream *stream)
{
	/* no-op */
	return 0;
}

static int
stream_close (GMimeStream *stream)
{
	GMimeStreamMem *mem = (GMimeStreamMem *) stream;
	
	if (mem->owner && mem->buffer)
		g_byte_array_free (mem->buffer, TRUE);
	
	mem->buffer = NULL;
	stream->position = 0;
	
	return 0;
}

static gboolean
stream_eos (GMimeStream *stream)
{
	GMimeStreamMem *mem = (GMimeStreamMem *) stream;
	off_t bound_end;
	
	g_return_val_if_fail (mem->buffer != NULL, TRUE);
	
	bound_end = stream->bound_end != -1 ? stream->bound_end : (off_t) mem->buffer->len;
	
	return stream->position >= bound_end;
}

static int
stream_reset (GMimeStream *stream)
{
	GMimeStreamMem *mem = (GMimeStreamMem *) stream;
	
	return mem->buffer ? 0 : -1;
}

static off_t
stream_seek (GMimeStream *stream, off_t offset, GMimeSeekWhence whence)
{
	GMimeStreamMem *mem = (GMimeStreamMem *) stream;
	off_t bound_end, real = stream->position;
	
	g_return_val_if_fail (mem->buffer != NULL, -1);
	
	bound_end = stream->bound_end != -1 ? stream->bound_end : (off_t) mem->buffer->len;
	
	switch (whence) {
	case GMIME_STREAM_SEEK_SET:
		real = offset;
		break;
	case GMIME_STREAM_SEEK_END:
		real = offset + bound_end;
		break;
	case GMIME_STREAM_SEEK_CUR:
		real = stream->position + offset;
		break;
	}
	
	if (real < stream->bound_start)
		real = stream->bound_start;
	else if (real > bound_end)
		real = bound_end;
	
	stream->position = real;
	
	return stream->position;
}

static off_t
stream_tell (GMimeStream *stream)
{
	GMimeStreamMem *mem = (GMimeStreamMem *) stream;
	
	g_return_val_if_fail (mem->buffer != NULL, -1);
	
	return stream->position;
}

static ssize_t
stream_length (GMimeStream *stream)
{
	GMimeStreamMem *mem = GMIME_STREAM_MEM (stream);
	off_t bound_end;
	
	g_return_val_if_fail (mem->buffer != NULL, -1);
	
	bound_end = stream->bound_end != -1 ? stream->bound_end : (off_t) mem->buffer->len;
	
	return bound_end - stream->bound_start;
}

static GMimeStream *
stream_substream (GMimeStream *stream, off_t start, off_t end)
{
	GMimeStreamMem *mem;
	
	mem = g_object_new (GMIME_TYPE_STREAM_MEM, NULL);
	mem->owner = FALSE;
	mem->buffer = GMIME_STREAM_MEM (stream)->buffer;
	
	g_mime_stream_construct (GMIME_STREAM (mem), start, end);
	
	return GMIME_STREAM (mem);
}


/**
 * g_mime_stream_mem_new:
 *
 * Creates a new GMimeStreamMem object.
 *
 * Returns a new memory stream.
 **/
GMimeStream *
g_mime_stream_mem_new (void)
{
	GMimeStreamMem *mem;
	
	mem = g_object_new (GMIME_TYPE_STREAM_MEM, NULL);
	mem->buffer = g_byte_array_new ();
	mem->owner = TRUE;
	
	g_mime_stream_construct (GMIME_STREAM (mem), 0, -1);
	
	return GMIME_STREAM (mem);
}


/**
 * g_mime_stream_mem_new_with_byte_array:
 * @array: source data
 *
 * Creates a new GMimeStreamMem with data @array.
 *
 * Returns a new memory stream using @array.
 **/
GMimeStream *
g_mime_stream_mem_new_with_byte_array (GByteArray *array)
{
	GMimeStreamMem *mem;
	
	mem = g_object_new (GMIME_TYPE_STREAM_MEM, NULL);
	mem->owner = TRUE;
	mem->buffer = array;
	
	g_mime_stream_construct (GMIME_STREAM (mem), 0, -1);
	
	return GMIME_STREAM (mem);
}


/**
 * g_mime_stream_mem_new_with_buffer:
 * @buffer: stream data
 * @len: data length
 *
 * Creates a new GMimeStreamMem object and initializes the stream
 * contents with the first @len bytes of @buffer.
 *
 * Returns a new memory stream initialized with @buffer.
 **/
GMimeStream *
g_mime_stream_mem_new_with_buffer (const char *buffer, size_t len)
{
	GMimeStreamMem *mem;
	
	mem = g_object_new (GMIME_TYPE_STREAM_MEM, NULL);
	mem->owner = TRUE;
	mem->buffer = g_byte_array_new ();
	
	g_byte_array_append (mem->buffer, (unsigned char *) buffer, len);
	
	g_mime_stream_construct (GMIME_STREAM (mem), 0, -1);
	
	return GMIME_STREAM (mem);
}


/**
 * g_mime_stream_mem_get_byte_array:
 * @mem: memory stream
 *
 * Gets the byte array from the memory stream.
 *
 * Returns the byte array from the memory stream.
 **/
GByteArray *
g_mime_stream_mem_get_byte_array (GMimeStreamMem *mem)
{
	g_return_val_if_fail (GMIME_IS_STREAM_MEM (mem), NULL);
	
	return mem->buffer;
}


/**
 * g_mime_stream_mem_set_byte_array:
 * @mem: memory stream
 * @array: stream data
 *
 * Sets the byte array on the memory stream.
 *
 * Note: The memory stream is not responsible for freeing the byte
 * array.
 **/
void
g_mime_stream_mem_set_byte_array (GMimeStreamMem *mem, GByteArray *array)
{
	GMimeStream *stream;
	
	g_return_if_fail (GMIME_IS_STREAM_MEM (mem));
	g_return_if_fail (array != NULL);
	
	if (mem->owner && mem->buffer)
		g_byte_array_free (mem->buffer, TRUE);
	
	mem->buffer = array;
	mem->owner = FALSE;
	
	stream = GMIME_STREAM (mem);
	
	stream->position = 0;
	stream->bound_start = 0;
	stream->bound_end = -1;
}


/**
 * g_mime_stream_mem_get_owner:
 * @mem: memory stream
 *
 * Gets whether or not @mem owns the backend memory buffer.
 *
 * Returns %TRUE if @mem owns the backend memory buffer or %FALSE
 * otherwise.
 **/
gboolean
g_mime_stream_mem_get_owner (GMimeStreamMem *mem)
{
	g_return_val_if_fail (GMIME_IS_STREAM_MEM (mem), FALSE);
	
	return mem->owner;
}


/**
 * g_mime_stream_mem_set_owner:
 * @mem: memory stream
 * @owner: owner
 *
 * Sets whether or not @mem owns the backend memory buffer.
 *
 * Note: @owner should be %TRUE if the stream should free the backend
 * memory buffer when destroyed or %FALSE otherwise.
 **/
void
g_mime_stream_mem_set_owner (GMimeStreamMem *mem, gboolean owner)
{
	g_return_if_fail (GMIME_IS_STREAM_MEM (mem));
	
	mem->owner = owner;
}
