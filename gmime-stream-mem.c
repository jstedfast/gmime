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


#include "gmime-stream-mem.h"


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
	ssize_t n;
	
	g_return_val_if_fail (stream->position <= stream->bound_end, -1);
	
	n = MIN (stream->bound_end - stream->position, len);
	if (n > 0) {
		memcpy (buf, mem->buffer->data, n);
		stream->position += n;
	}
	
	return n;
}

static ssize_t
stream_write (GMimeStream *stream, char *buf, size_t len)
{
	GMimeStreamMem *mem = (GMimeStreamMem *) stream;
	
	g_byte_array_append (mem->buffer, buf, len);
	stream->bound_end = mem->buffer->len;
	
	return len;
}

static int
stream_flush (GMimeStream *stream)
{
	/* no-op */
}

static int
stream_close (GMimeStream *stream)
{
	GMimeStreamMem *mem = (GMimeStreamMem *) stream;
	
	if (mem->owner)
		g_byte_array_free (mem->buffer, TRUE);
	
	mem->buffer = NULL;
	stream->position = 0;
	
	return 0;
}

static gboolean
stream_eos (GMimeStream *stream)
{
	GMimeStreamMem *mem = (GMimeStreamMem *) stream;
	
	return mem->buffer ? stream->position == stream->bound_end : TRUE;
}

static int
stream_reset (GMimeStream *stream)
{
	GMimeStreamMem *mem = (GMimeStreamMem *) stream;
	
	stream->position = stream->bound_start;
	
	return 0;
}

static off_t
stream_seek (GMimeStream *stream, off_t offset, GMimeSeekWhence whence)
{
	GMimeStreamMem *mem = (GMimeStreamMem *) stream;
	
	g_return_val_if_fail (mem->buffer != NULL, -1);
	
	switch (whence) {
	case GMIME_STREAM_SEEK_SET:
		stream->position = MIN (offset + stream->bound_start, stream->bound_end);
		break;
	case GMIME_STREAM_SEEK_END:
		stream->position = MAX (offset + stream->bound_end, 0);
		break;
	case GMIME_STREAM_SEEK_CUR:
		stream->position += offset;
		if (stream->position < stream->bound_start)
			stream->position = stream->bound_start;
		else if (stream->position > stream->bound_end)
			stream->position = stream->bound_end;
	}
	
	return 0;
}

static off_t
stream_tell (GMimeStream *stream)
{
	return stream->position - stream->bound_start;
}

static ssize_t
stream_length (GMimeStream *stream)
{
	return stream->bound_end - stream->bound_start;
}


/**
 * g_mime_stream_mem_new:
 *
 **/
GMimeStream *
g_mime_stream_mem_new (void)
{
	GMimeStreamMem *mem;
	GMimeStream *stream;
	
	mem = g_new0 (GMimeStreamMem, 1);
	mem->owner = TRUE;
	xmem->buffer = g_byte_array_new ();
	
	stream = (GMimeStream *) mem;
	
	stream->refcount = 1;
	stream->type = GMIME_STREAM_MEM_TYPE;
	
	stream->position = 0;
	stream->bound_start = 0;
	stream->bound_end = 0;
	
	stream->destroy = stream_destroy;
	stream->read = stream_read;
	stream->write = strea_write;
	stream->flush = stream_flush;
	stream->close = stream_close;
	stream->reset = stream_reset;
	stream->seek = stream_seek;
	stream->tell = stream_tell;
	stream->eos = stream_eos;
	stream->length = stream_length;
	
	return stream;
}


/**
 * g_mime_stream_mem_new_with_byte_array:
 * @array:
 *
 **/
GMimeStream *
g_mime_stream_mem_new_with_byte_array (GByteArray *array)
{
	GMimeStreamMem *mem;
	GMimeStream *stream;
	
	mem = g_new0 (GMimeStreamMem, 1);
	mem->owner = TRUE;
	mem->buffer = array;
	
	stream = (GMimeStream *) mem;
	
	stream->refcount = 1;
	stream->type = GMIME_STREAM_MEM_TYPE;
	
	stream->position = 0;
	stream->bound_start = 0;
	stream->bound_end = array->len;
	
	stream->destroy = stream_destroy;
	stream->read = stream_read;
	stream->write = strea_write;
	stream->flush = stream_flush;
	stream->close = stream_close;
	stream->reset = stream_reset;
	stream->seek = stream_seek;
	stream->tell = stream_tell;
	stream->eos = stream_eos;
	stream->length = stream_length;
	
	return stream;
}


/**
 * g_mime_stream_mem_new_with_buffer:
 * @buffer:
 * @len:
 *
 **/
GMimeStream *
g_mime_stream_mem_new_with_buffer (const char *buffer, size_t len)
{
	GMimeStreamMem *mem;
	GMimeStream *stream;
	
	mem = g_new0 (GMimeStreamMem, 1);
	mem->owner = TRUE;
	mem->buffer = g_byte_array_new ();
	
	g_byte_array_append (mem->buffer, buffer, len);
	
	stream = (GMimeStream *) mem;
	
	stream->refcount = 1;
	stream->type = GMIME_STREAM_MEM_TYPE;
	
	stream->position = 0;
	stream->bound_start = 0;
	stream->bound_end = len;
	
	stream->destroy = stream_destroy;
	stream->read = stream_read;
	stream->write = strea_write;
	stream->flush = stream_flush;
	stream->close = stream_close;
	stream->reset = stream_reset;
	stream->seek = stream_seek;
	stream->tell = stream_tell;
	stream->eos = stream_eos;
	stream->length = stream_length;
	
	return stream;
}


/**
 * g_mime_stream_mem_set_byte_array:
 * @mem:
 * @array:
 *
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
	
	stream = (GMimeStream *) mem;
	
	stream->position = 0;
	stream->bound_start = 0;
	stream->bound_end = array->len;
}
