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


#include "gmime-stream-filter.h"

#define READ_PAD (64)		/* bytes padded before buffer */
#define READ_SIZE (4096)

#define _PRIVATE(o) (((GMimeStreamFilter *)(o))->priv)

struct _filter {
	struct _filter *next;
	GMimeFilter *filter;
	int id;
};

struct _GMimeStreamFilterPrivate {
	struct _filter *filters;
	int filterid;		/* next filter id */
	
	char *realbuffer;	/* buffer - READ_PAD */
	char *buffer;		/* READ_SIZE bytes */
	
	char *filtered;		/* the filtered data */
	size_t filteredlen;
	
	int last_was_read;	/* was the last op read or write? */
};

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
	GMimeStreamFilter *filter = (GMimeStreamFilter *) stream;
	struct _GMimeStreamFilterPrivate *p = _PRIVATE (filter);
	struct _filter *fn, *f;
	
	f = p->filters;
	while (f) {
		fn = f->next;
		g_mime_filter_destroy (f->filter);
		g_free (f);
		f = fn;
	}
	
	g_free (p->realbuffer);
	g_free (p);
	
	g_mime_stream_unref (filter->source);
	
	g_free (filter);
}

static ssize_t
stream_read (GMimeStream *stream, char *buf, size_t len)
{
	GMimeStreamFilter *filter = (GMimeStreamFilter *) stream;
	struct _CamelStreamFilterPrivate *p = _PRIVATE (filter);
	struct _filter *f;
	ssize_t size;
	
	p->last_was_read = TRUE;
	
	if (p->filteredlen <= 0) {
		int presize = READ_SIZE;
		
		size = g_mime_stream_read (filter->source, p->buffer, READ_SIZE);
		if (size <= 0) {
			/* this is somewhat untested */
			if (g_mime_stream_eos (filter->source)) {
				f = p->filters;
				p->filtered = p->buffer;
				p->filteredlen = 0;
				while (f) {
					g_mime_filter_complete (f->filter, p->filtered, p->filteredlen,
								presize, &p->filtered, &p->filteredlen,
								&presize);
					f = f->next;
				}
				size = p->filteredlen;
			}
			if (size <= 0)
				return size;
		} else {
			f = p->filters;
			p->filtered = p->buffer;
			p->filteredlen = size;
			
			while (f) {
				g_mime_filter_filter (f->filter, p->filtered, p->filteredlen, presize,
						      &p->filtered, &p->filteredlen, &presize);
				
				f = f->next;
			}
		}
	}
	
	size = MIN (len, p->filteredlen);
	memcpy (buffer, p->filtered, size);
	p->filteredlen -= size;
	p->filtered += size;
	
	return size;
}

static ssize_t
stream_write (GMimeStream *stream, char *buf, size_t len)
{
	GMimeStreamFilter *filter = (GMimeStreamFilter *) stream;
	struct _GMimeStreamFilterPrivate *p = _PRIVATE (filter);
	struct _filter *f;
	char *buffer;
	int presize;
	size_t n;
	
	p->last_was_read = FALSE;
	
	buffer = buf;
	n = len;
	
	f = p->filters;
	presize = 0;
	while (f) {
		g_mime_mime_filter_filter (f->filter, buffer, n, presize, &buffer, &n, &presize);
		
		f = f->next;
	}
	
	if (gmime_stream_write (filter->source, buffer, n) != n)
		return -1;
	
	/* return 'len' because that's what our caller expects */
	return len;
}

static int
stream_flush (GMimeStream *stream)
{
	GMimeStreamFilter *filter = (GMimeStreamFilter *) stream;
	struct _GMimeStreamFilterPrivate *p = _PRIVATE (filter);
	struct _filter *f;
	int len, presize;
	char *buffer;
	
	if (p->last_was_read) {
		/* no-op */
		return 0;
	}
	
	buffer = "";
	len = 0;
	presize = 0;
	f = p->filters;
	
	while (f) {
		g_mime_filter_complete (f->filter, buffer, len, presize, &buffer, &len, &presize);
		
		f = f->next;
	}
	
	if (len > 0 && g_mime_stream_write (filter->source, buffer, len) == -1)
		return -1;
	
	return g_mime_stream_flush (filter->source);
}

static int
stream_close (GMimeStream *stream)
{
	GMimeStreamFilter *filter = (GMimeStreamFilter *) stream;
	struct _GMimeStreamFilterPrivate *p = _PRIVATE (filter);
	
	if (!p->last_was_read) {
		stream_flush (stream);
	}
	
	return g_mime_stream_close (filter->source);
}

static gboolean
stream_eos (GMimeStream *stream)
{
	GMimeStreamFilter *filter = (GMimeStreamFilter *) stream;
	struct _GMimeStreamFilterPrivate *p = _PRIVATE (filter);
	
	if (p->filteredlen > 0)
		return FALSE;
	
	return g_mime_stream_eos (filter->source);
}

static int
stream_reset (GMimeStream *stream)
{
	GMimeStreamFilter *filter = (GMimeStreamFilter *) stream;
	struct _GMimeStreamFilterPrivate *p = _PRIVATE (filter);
	struct _filter *f;
	
	p->filteredlen = 0;
	
	/* and reset filters */
	f = p->filters;
	while (f) {
		g_mime_filter_reset (f->filter);
		f = f->next;
	}
	
	return g_mime_stream_reset (filter->source);
}

static off_t
stream_seek (GMimeStream *stream, off_t offset, GMimeSeekWhence whence)
{
	GMimeStreamFilter *filter = (GMimeStreamFilter *) stream;
	
	return -1;
}

static off_t
stream_tell (GMimeStream *stream)
{
	return -1;
}

static ssize_t
stream_length (GMimeStream *stream)
{
	return stream->bound_end - stream->bound_start;
}

static GMimeStream *
stream_substream (GMimeStream *stream, off_t start, off_t end)
{
	GMimeStreamFilter *filter;
	struct _filters *f, *fn;
	
	filter = g_new (GMimeStreamFilter, 1);
	filter->source = GMIME_STREAM_FILTER (stream)->source;
	g_mime_stream_ref (filter->source);
	
	filter->priv = g_new (struct _GMimeStreamFilterPrivate, 1);
	filter->priv->filters = NULL;
	filter->priv->filterid = 0;
	filter->priv->realbuffer = g_malloc (READ_SIZE + READ_PAD);
	filter->priv->buffer = filter->priv->realbuffer + READ_PAD;
	filter->priv->last_was_read = TRUE;
	filter->priv->filteredlen = 0;
	
	/* FIXME: copy the filters... */
	
	g_mime_stream_construct (GMIME_STREAM (filter), &template,
				 GMIME_STREAM_FILTER_TYPE,
				 filter->source->bound_start,
				 filter->source->bound_end);
	
	return GMIME_STREAM (filter);
}


GMimeStreamFilter *
g_mime_stream_filter_new_with_stream (GMimeStream *stream)
{
	GMimeStreamFilter *filter;
	
	g_return_val_if_fail (stream != NULL, NULL);
	
	filter = g_new (GMimeStreamFilter, 1);
	filter->source = stream;
	g_mime_stream_ref (stream);
	
	filter->priv = g_new (struct _GMimeStreamFilterPrivate, 1);
	filter->priv->filters = NULL;
	filter->priv->filterid = 0;
	filter->priv->realbuffer = g_malloc (READ_SIZE + READ_PAD);
	filter->priv->buffer = filter->priv->realbuffer + READ_PAD;
	filter->priv->last_was_read = TRUE;
	filter->priv->filteredlen = 0;
	
	g_mime_stream_construct (GMIME_STREAM (filter), &template,
				 GMIME_STREAM_FILTER_TYPE,
				 stream->bound_start,
				 stream->bound_end);
	
	return GMIME_STREAM (filter);
}


int
g_mime_stream_filter_add (GMimeStreamFilter *fstream, GMimeFilter *filter)
{
	struct _GMimeStreamFilterPrivate *p = _PRIVATE (fstream);
	struct _filter *f, *fn;
	
	g_return_val_if_fail (filter != NULL, -1);
	
	fn = g_new (struct _filter, 1);
	fn->next = NULL;
	fn->filter = filter;
	fn->id = p->filterid++;
	
	f = (struct _filter *) &p->filters;
	while (f->next)
		f = f->next;
	
	f->next = fn;
	fn->next = NULL;
	
	return fn->id;
}


void
g_mime_stream_filter_remove (GMimeStreamFilter *fstream, int id)
{
	struct _GMimeStreamFilterPrivate *p = _PRIVATE (fstream);
	struct _filter *f, *fn;
	
	g_return_val_if_fail (filter != NULL, -1);
	
	if (id == -1)
		return;
	
	f = (struct _filter *) &p->filters;
	while (f && f->next) {
		fn = f->next;
		if (fn->id == id) {
			f->next = fn->next;
			g_mime_filter_destroy (fn->filter);
			g_free (fn);
		}
		f = f->next;
	}
}
