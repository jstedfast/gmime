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

static GMimeStream template = {
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
	GMimeStreamFile *fstream = (GMimeStreamFile *) stream;
	
	if (fstream->owner && fstream->fp)
		fclose (fstream->fp);
	
	g_free (fstream);
}

static ssize_t
stream_read (GMimeStream *stream, char *buf, size_t len)
{
	GMimeStreamFile *fstream = (GMimeStreamFile *) stream;
	ssize_t nread;
	
	if (stream->bound_end == -1 && stream->position > stream->bound_end)
		return -1;
	
	if (stream->bound_end != -1)
		len = MIN (stream->bound_end - stream->position, len);
	
	/* make sure we are at the right position */
	fseek (fstream->fp, stream->position, SEEK_SET);
	
	nread = fread (buf, 1, len, fstream->fp);
	
	if (nread > 0)
		stream->position += nread;
	
	return nread;
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
		fstream->fp = NULL;
	
	return ret;
}

static gboolean
stream_eos (GMimeStream *stream)
{
	GMimeStreamFile *fstream = (GMimeStreamFile *) stream;
	
	g_return_val_if_fail (fstream->fp != NULL, TRUE);
	
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

static GMimeStream *
stream_substream (GMimeStream *stream, off_t start, off_t end)
{
	GMimeStreamFile *fstream;
	
	fstream = g_new0 (GMimeStreamFile, 1);
	fstream->owner = FALSE;
	fstream->fp = GMIME_STREAM_FILE (stream)->fp;
	
	g_mime_stream_construct (GMIME_STREAM (fstream), &template, GMIME_STREAM_FILE_TYPE, start, end);
	
	return GMIME_STREAM (fstream);
}


/**
 * g_mime_stream_file_new:
 * @fp: file pointer
 *
 * Returns a stream using @fp.
 **/
GMimeStream *
g_mime_stream_file_new (FILE *fp)
{
	GMimeStreamFile *fstream;
	
	fstream = g_new (GMimeStreamFile, 1);
	fstream->owner = TRUE;
	fstream->fp = fp;
	
	g_mime_stream_construct (GMIME_STREAM (fstream), &template, GMIME_STREAM_FILE_TYPE, ftell (fp), -1);
	
	return GMIME_STREAM (fstream);
}


/**
 * g_mime_stream_file_new_with_bounds:
 * @fp: file pointer
 * @start: start boundary
 * @end: end boundary
 *
 * Returns a stream using @fp with bounds @start and @end.
 **/
GMimeStream *
g_mime_stream_file_new_with_bounds (FILE *fp, off_t start, off_t end)
{
	GMimeStreamFile *fstream;
	
	fstream = g_new (GMimeStreamFile, 1);
	fstream->owner = TRUE;
	fstream->fp = fp;
	
	g_mime_stream_construct (GMIME_STREAM (fstream), &template, GMIME_STREAM_FILE_TYPE, start, end);
	
	return GMIME_STREAM (fstream);
}
