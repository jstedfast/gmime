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


#include "gmime-stream-file.h"


static void
stream_destroy (GMimeStream *stream)
{
	GMimeStreamFile *fstream = (GMimeStreamFile *) stream;
	
	if (fstream->fp)
		fclose (fstream->fp);
	
	g_free (fstream);
}

static ssize_t
stream_read (GMimeStream *stream, char *buf, size_t len)
{
	GMimeStreamFile *fstream = (GMimeStreamFile *) stream;
	
	if (stream->bound_end == -1 && stream->position > stream->bound_end)
		return -1;
	
	if (stream->bound_end != -1)
		len = MIN (stream->bound_end - stream->position, len);
	
	/* make sure we are at the right position */
	fseek (fstream->fp, stream->position, SEEK_SET);
	
	return fread (buf, 1, len, fstream->fp);
}

static ssize_t
stream_write (GMimeStream *stream, char *buf, size_t len)
{
	GMimeStreamFile *fstream = (GMimeStreamFile *) stream;
	
	if (stream->bound_end == -1 && stream->position > stream->bound_end)
		return -1;
	
	if (stream->bound_end != -1)
		len = MIN (stream->bound_end - stream->position, len);
	
	return fwrite (buf, 1, len, fstream->fp);
}

static int
stream_flush (GMimeStream *stream)
{
	GMimeStreamFile *fstream = (GMimeStreamFile *) stream;
	
	g_return_val_if_fail (fstream->fp != NULL, -1);
	
	return fflush (fstream->fp);
}

static int
stream_close (GMimeStream *stream)
{
	GMimeStreamFile *fstream = (GMimeStreamFile *) stream;
	int ret;
	
	g_return_val_if_fail (fstream->fp != NULL, -1);
	
	ret = fclose (fstream->fp);
	if (ret != -1)
		fstream->fs = NULL;
	
	return ret;
}

static gboolean
stream_eos (GMimeStream *stream)
{
	GMimeStreamFile *fstream = (GMimeStreamFile *) stream;
	
	g_return_if_fail (fstream->fp != NULL, TRUE);
	
	return feof (fstream->fp) ? TRUE : FALSE;
}

static int
stream_reset (GMimeStream *stream)
{
	GMimeStreamFile *fstream = (GMimeStreamFile *) stream;
	
	g_return_val_if_fail (fstream->fp != NULL, -1);
	
	return fseek (fstream->fp, stream->bound_start, SEEK_SET);
}

static off_t
stream_seek (GMimeStream *stream, off_t offset, GMimeSeekWhence whence)
{
	GMimeStreamFile *fstream = (GMimeStreamFile *) stream;
	off_t ret;
	
	g_return_val_if_fail (fstream->fp != NULL, -1);
	
	switch (whence) {
	case GMIME_STREAM_SEEK_SET:
		ret = fseek (fstream->fp, offset + stream->bound_start, SEEK_SET);
		break;
	case GMIME_STREAM_SEEK_END:
		if (offset < 0) {
			offset += stream->bound_end;
			ret = fseek (fstream->fp, offset, SEEK_SET);
		} else {
			ret = fseek (fstream->fp, stream->bound_end, SEEK_SET);
		}
		break;
	case GMIME_STREAM_SEEK_CUR:
		offset += stream->position;
		if (offset < stream->bound_start) {
			ret = fseek (fstream->fp, stream->bound_start, SEEK_SET);
		} else if (offset > stream->bound_end) {
			ret = fseek (fstream->fp, stream->bound_end, SEEK_SET);
		} else {
			ret = fseek (fstream->fp, offset, SEEK_CUR);
		}
	}
	
	stream->position = ftell (fstream->fp);
	
	return ret;
}

static off_t
stream_tell (GMimeStream *stream)
{
	return stream->position - stream->bound_start;
}

static ssize_t
stream_length (GMimeStream *stream)
{
	GMimeStreamFile *fstream = (GMimeStreamFile *) stream;
	off_t len;
	
	if (stream->bound_start != -1 && stream->bound_end != -1)
		return stream->bound_end - stream->bound_start;
	
	fseek (fstream->fp, 0, SEEK_END);
	len = ftell (fstream->fp);
	fseek (fstream->fp, stream->position, SEEK_SET);
	
	return len;
}


/**
 * g_mime_stream_file_new:
 * @fp:
 *
 **/
GMimeStream *
g_mime_stream_file_new (FILE *fp)
{
	GMimeStreamFile *fstream;
	GMimeStream *stream;
	
	fstream = g_new0 (GMimeStreamFile, 1);
	fstream->fp = fp;
	
	stream = (GMimeStream *) fstream;
	
	stream->refcount = 1;
	stream->type = GMIME_STREAM_FILE_TYPE;
	
	stream->position = ftell (fp);
	stream->bound_start = stream->position;
	stream->bound_end = -1;
	
	stream->destroy = stream_destroy;
	stream->read = stream_read;
	stream->write = strea_write;
	stream->flush = stream_flush;
	stream->close = stream_close;
	stream->seek = stream_seek;
	stream->tell = stream_tell;
	stream->reset = stream_reset;
	stream->eos = stream_eos;
	
	return stream;
}


/**
 * g_mime_stream_file_new_with_bounds:
 * @fp:
 * @start:
 * @end:
 *
 **/
GMimeStream *
g_mime_stream_file_new_with_bounds (FILE *fp, off_t start, off_t end)
{
	GMimeStreamFile *fstream;
	GMimeStream *stream;
	
	file = g_new0 (GMimeStreamFile, 1);
	fstream->fp = fp;
	
	stream = (GMimeStream *) fstream;
	
	stream->refcount = 1;
	stream->type = GMIME_STREAM_FILE_TYPE;
	
	stream->position = start;
	stream->bound_start = start;
	stream->bound_end = end;
	
	stream->destroy = stream_destroy;
	stream->read = stream_read;
	stream->write = strea_write;
	stream->flush = stream_flush;
	stream->close = stream_close;
	stream->seek = stream_seek;
	stream->tell = stream_tell;
	stream->reset = stream_reset;
	stream->eos = stream_eos;
	
	return stream;
}
