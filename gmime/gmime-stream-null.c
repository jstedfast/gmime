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


#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <string.h>
#include "gmime-stream-null.h"

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
	GMimeStreamNull *null = (GMimeStreamNull *) stream;
	
	g_free (null);
}

static ssize_t
stream_read (GMimeStream *stream, char *buf, size_t len)
{
	memset (buf, 0, len);
	
	stream->position += len;
	
	return len;
}

static ssize_t
stream_write (GMimeStream *stream, char *buf, size_t len)
{
	GMimeStreamNull *null = (GMimeStreamNull *) stream;
	
	null->written += len;
	stream->position += len;
	
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
	
	null->written = 0;
	
	return 0;
}

static off_t
stream_seek (GMimeStream *stream, off_t offset, GMimeSeekWhence whence)
{
	GMimeStreamNull *null = (GMimeStreamNull *) stream;
	off_t bound_end;
	
	bound_end = stream->bound_end != -1 ? stream->bound_end : null->written;
	
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
	return stream->position;
}

static ssize_t
stream_length (GMimeStream *stream)
{
	GMimeStreamNull *null = GMIME_STREAM_NULL (stream);
	off_t bound_end;
	
	bound_end = stream->bound_end != -1 ? stream->bound_end : null->written;
	
	return bound_end - stream->bound_start;
}

static GMimeStream *
stream_substream (GMimeStream *stream, off_t start, off_t end)
{
	GMimeStreamNull *null;
	
	null = g_new0 (GMimeStreamNull, 1);
	
	g_mime_stream_construct (GMIME_STREAM (null), &stream_template, GMIME_STREAM_NULL_TYPE, start, end);
	
	return GMIME_STREAM (null);
}


/**
 * g_mime_stream_null_new:
 *
 * Returns a new null stream (ie something similar to /dev/null).
 **/
GMimeStream *
g_mime_stream_null_new (void)
{
	GMimeStreamNull *null;
	
	null = g_new0 (GMimeStreamNull, 1);
	
	g_mime_stream_construct (GMIME_STREAM (null), &stream_template, GMIME_STREAM_NULL_TYPE, 0, -1);
	
	return GMIME_STREAM (null);
}
