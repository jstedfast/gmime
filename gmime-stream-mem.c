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

#include <string.h>
#include "gmime-stream-mem.h"

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

static GMimeStream stream_template = {
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
	GMimeStreamMem *mem = (GMimeStreamMem *) stream;
	
	if (mem->owner && mem->buffer)
		g_byte_array_free (mem->buffer, TRUE);
	
	g_free (mem);
}

static ssize_t
stream_read (GMimeStream *stream, char *buf, size_t len)
{
	GMimeStreamMem *mem = (GMimeStreamMem *) stream;
	off_t bound_end;
	ssize_t n;
	
	g_return_val_if_fail (mem->buffer != NULL, -1);
	
	bound_end = stream->bound_end != -1 ? stream->bound_end : mem->buffer->len;
	
	n = MIN (bound_end - stream->position, len);
	if (n > 0) {
		memcpy (buf, mem->buffer->data + stream->position, n);
		stream->position += n;
	}
	
	return n;
}

static ssize_t
stream_write (GMimeStream *stream, char *buf, size_t len)
{
	GMimeStreamMem *mem = (GMimeStreamMem *) stream;
	off_t bound_end;
	ssize_t n;
	
	g_return_val_if_fail (mem->buffer != NULL, -1);
	
	if (stream->bound_end == -1 && stream->position + len > mem->buffer->len)
		g_byte_array_set_size (mem->buffer, stream->position + len);
	
	bound_end = stream->bound_end != -1 ? stream->bound_end : mem->buffer->len;
	
	n = MIN (bound_end - stream->position, len);
	if (n > 0) {
		memcpy (mem->buffer->data + stream->position, buf, n);
		stream->position += n;
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
	
	bound_end = stream->bound_end != -1 ? stream->bound_end : mem->buffer->len;
	
	return mem->buffer ? stream->position >= bound_end : TRUE;
}

static int
stream_reset (GMimeStream *stream)
{
	GMimeStreamMem *mem = (GMimeStreamMem *) stream;
	
	g_return_val_if_fail (mem->buffer != NULL, -1);
	
	stream->position = stream->bound_start;
	
	return 0;
}

static off_t
stream_seek (GMimeStream *stream, off_t offset, GMimeSeekWhence whence)
{
	GMimeStreamMem *mem = (GMimeStreamMem *) stream;
	off_t bound_end;
	
	g_return_val_if_fail (mem->buffer != NULL, -1);
	
	bound_end = stream->bound_end != -1 ? stream->bound_end : mem->buffer->len;
	
	switch (whence) {
	case GMIME_STREAM_SEEK_SET:
		stream->position = MIN (offset + stream->bound_start, bound_end);
		break;
	case GMIME_STREAM_SEEK_END:
		stream->position = MAX (offset + bound_end, 0);
		break;
	case GMIME_STREAM_SEEK_CUR:
		stream->position += offset;
		if (stream->position < stream->bound_start)
			stream->position = stream->bound_start;
		else if (stream->position > bound_end)
			stream->position = bound_end;
	}
	
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
	
	bound_end = stream->bound_end != -1 ? stream->bound_end : mem->buffer->len;
	
	return bound_end - stream->bound_start;
}

static GMimeStream *
stream_substream (GMimeStream *stream, off_t start, off_t end)
{
	GMimeStreamMem *mem;
	
	mem = g_new (GMimeStreamMem, 1);
	mem->owner = FALSE;
	mem->buffer = GMIME_STREAM_MEM (stream)->buffer;
	
	g_mime_stream_construct (GMIME_STREAM (mem), &stream_template, GMIME_STREAM_MEM_TYPE, start, end);
	
	return GMIME_STREAM (mem);
}


/**
 * g_mime_stream_mem_new:
 *
 * Returns a new memory stream.
 **/
GMimeStream *
g_mime_stream_mem_new (void)
{
	GMimeStreamMem *mem;
	
	mem = g_new (GMimeStreamMem, 1);
	mem->owner = TRUE;
	mem->buffer = g_byte_array_new ();
	
	g_mime_stream_construct (GMIME_STREAM (mem), &stream_template, GMIME_STREAM_MEM_TYPE, 0, -1);
	
	return GMIME_STREAM (mem);
}


/**
 * g_mime_stream_mem_new_with_byte_array:
 * @array:
 *
 * Returns a new memory stream using @array.
 **/
GMimeStream *
g_mime_stream_mem_new_with_byte_array (GByteArray *array)
{
	GMimeStreamMem *mem;
	
	mem = g_new (GMimeStreamMem, 1);
	mem->owner = TRUE;
	mem->buffer = array;
	
	g_mime_stream_construct (GMIME_STREAM (mem), &stream_template, GMIME_STREAM_MEM_TYPE, 0, -1);
	
	return GMIME_STREAM (mem);
}


/**
 * g_mime_stream_mem_new_with_buffer:
 * @buffer:
 * @len:
 *
 * Returns a new memory stream initialized with @buffer.
 **/
GMimeStream *
g_mime_stream_mem_new_with_buffer (const char *buffer, size_t len)
{
	GMimeStreamMem *mem;
	
	mem = g_new (GMimeStreamMem, 1);
	mem->owner = TRUE;
	mem->buffer = g_byte_array_new ();
	
	g_byte_array_append (mem->buffer, buffer, len);
	
	g_mime_stream_construct (GMIME_STREAM (mem), &stream_template, GMIME_STREAM_MEM_TYPE, 0, -1);
	
	return GMIME_STREAM (mem);
}


/**
 * g_mime_stream_mem_set_byte_array:
 * @mem: 
 * @array: 
 *
 * Sets the byte array on the memory stream. Note: The memory stream
 * is not responsible for freeing the byte array.
 **/
void
g_mime_stream_mem_set_byte_array (GMimeStreamMem *mem, GByteArray *array)
{
	GMimeStream *stream;
	
	g_return_if_fail (mem != NULL);
	g_return_if_fail (array != NULL);
	
	if (mem->buffer)
		g_byte_array_free (mem->buffer, TRUE);
	
	mem->buffer = array;
	mem->owner = FALSE;
	
	stream = GMIME_STREAM (mem);
	
	stream->position = 0;
	stream->bound_start = 0;
	stream->bound_end = -1;
}
