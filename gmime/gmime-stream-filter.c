/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*  GMime
 *  Copyright (C) 2000-2014 Jeffrey Stedfast
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

#include "gmime-stream-filter.h"


/**
 * SECTION: gmime-stream-filter
 * @title: GMimeStreamFilter
 * @short_description: A filtering stream
 * @see_also: #Filters
 *
 * A #GMimeStream meant for filtering data passing through it.
 *
 * This stream class is useful for converting data of one type to
 * another using #GMimeFilter objects.
 *
 * When data passes through a #GMimeStreamFilter, it will pass through
 * #GMimeFilter filters in the order they were added.
 **/


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
	
	int last_was_read:1;	/* was the last op read or write? */
	int flushed:1;          /* have the filters been flushed? */
};

static void g_mime_stream_filter_class_init (GMimeStreamFilterClass *klass);
static void g_mime_stream_filter_init (GMimeStreamFilter *stream, GMimeStreamFilterClass *klass);
static void g_mime_stream_filter_finalize (GObject *object);

static ssize_t stream_read (GMimeStream *stream, char *buf, size_t n);
static ssize_t stream_write (GMimeStream *stream, const char *buf, size_t n);
static int stream_flush (GMimeStream *stream);
static int stream_close (GMimeStream *stream);
static gboolean stream_eos (GMimeStream *stream);
static int stream_reset (GMimeStream *stream);
static gint64 stream_seek (GMimeStream *stream, gint64 offset, GMimeSeekWhence whence);
static gint64 stream_tell (GMimeStream *stream);
static gint64 stream_length (GMimeStream *stream);
static GMimeStream *stream_substream (GMimeStream *stream, gint64 start, gint64 end);


static GMimeStreamClass *parent_class = NULL;


GType
g_mime_stream_filter_get_type (void)
{
	static GType type = 0;
	
	if (!type) {
		static const GTypeInfo info = {
			sizeof (GMimeStreamFilterClass),
			NULL, /* base_class_init */
			NULL, /* base_class_finalize */
			(GClassInitFunc) g_mime_stream_filter_class_init,
			NULL, /* class_finalize */
			NULL, /* class_data */
			sizeof (GMimeStreamFilter),
			0,    /* n_preallocs */
			(GInstanceInitFunc) g_mime_stream_filter_init,
		};
		
		type = g_type_register_static (GMIME_TYPE_STREAM, "GMimeStreamFilter", &info, 0);
	}
	
	return type;
}


static void
g_mime_stream_filter_class_init (GMimeStreamFilterClass *klass)
{
	GMimeStreamClass *stream_class = GMIME_STREAM_CLASS (klass);
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	
	parent_class = g_type_class_ref (GMIME_TYPE_STREAM);
	
	object_class->finalize = g_mime_stream_filter_finalize;
	
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
g_mime_stream_filter_init (GMimeStreamFilter *stream, GMimeStreamFilterClass *klass)
{
	stream->source = NULL;
	stream->priv = g_new (struct _GMimeStreamFilterPrivate, 1);
	stream->priv->filters = NULL;
	stream->priv->filterid = 0;
	stream->priv->realbuffer = g_malloc (READ_SIZE + READ_PAD);
	stream->priv->buffer = stream->priv->realbuffer + READ_PAD;
	stream->priv->last_was_read = TRUE;
	stream->priv->filteredlen = 0;
	stream->priv->flushed = FALSE;
}

static void
g_mime_stream_filter_finalize (GObject *object)
{
	GMimeStreamFilter *filter = (GMimeStreamFilter *) object;
	struct _GMimeStreamFilterPrivate *p = filter->priv;
	struct _filter *fn, *f;
	
	f = p->filters;
	while (f) {
		fn = f->next;
		g_object_unref (f->filter);
		g_free (f);
		f = fn;
	}
	
	g_free (p->realbuffer);
	g_free (p);
	
	if (filter->source)
		g_object_unref (filter->source);
	
	G_OBJECT_CLASS (parent_class)->finalize (object);
}


static ssize_t
stream_read (GMimeStream *stream, char *buf, size_t n)
{
	GMimeStreamFilter *filter = (GMimeStreamFilter *) stream;
	struct _GMimeStreamFilterPrivate *priv = filter->priv;
	struct _filter *f;
	ssize_t nread;
	
	priv->last_was_read = TRUE;
	
	if (priv->filteredlen <= 0) {
		size_t presize = READ_PAD;
		
		nread = g_mime_stream_read (filter->source, priv->buffer, READ_SIZE);
		if (nread <= 0) {
			/* this is somewhat untested */
			if (g_mime_stream_eos (filter->source) && !priv->flushed) {
				priv->filtered = priv->buffer;
				priv->filteredlen = 0;
				f = priv->filters;
				
				while (f != NULL) {
					g_mime_filter_complete (f->filter, priv->filtered, priv->filteredlen,
								presize, &priv->filtered, &priv->filteredlen,
								&presize);
					f = f->next;
				}
				
				nread = priv->filteredlen;
				priv->flushed = TRUE;
			}
			
			if (nread <= 0)
				return nread;
		} else {
			priv->filtered = priv->buffer;
			priv->filteredlen = nread;
			priv->flushed = FALSE;
			f = priv->filters;
			
			while (f != NULL) {
				g_mime_filter_filter (f->filter, priv->filtered, priv->filteredlen, presize,
						      &priv->filtered, &priv->filteredlen, &presize);
				
				f = f->next;
			}
		}
	}
	
	nread = MIN (n, priv->filteredlen);
	memcpy (buf, priv->filtered, nread);
	priv->filteredlen -= nread;
	priv->filtered += nread;
	
	return nread;
}

static ssize_t
stream_write (GMimeStream *stream, const char *buf, size_t n)
{
	GMimeStreamFilter *filter = (GMimeStreamFilter *) stream;
	struct _GMimeStreamFilterPrivate *priv = filter->priv;
	char *buffer = (char *) buf;
	ssize_t nwritten = n;
	struct _filter *f;
	size_t presize;
	
	priv->last_was_read = FALSE;
	priv->flushed = FALSE;
	
	f = priv->filters;
	presize = 0;
	while (f != NULL) {
		g_mime_filter_filter (f->filter, buffer, n, presize, &buffer, &n, &presize);
		
		f = f->next;
	}
	
	if (g_mime_stream_write (filter->source, buffer, n) == -1)
		return -1;
	
	/* return original input len because that's what our caller expects */
	return nwritten;
}

static int
stream_flush (GMimeStream *stream)
{
	GMimeStreamFilter *filter = (GMimeStreamFilter *) stream;
	struct _GMimeStreamFilterPrivate *priv = filter->priv;
	size_t presize, len;
	struct _filter *f;
	char *buffer;
	
	if (priv->last_was_read) {
		/* no-op */
		return 0;
	}
	
	buffer = "";
	len = 0;
	presize = 0;
	f = priv->filters;
	
	while (f != NULL) {
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
	struct _GMimeStreamFilterPrivate *priv = filter->priv;
	
	if (!priv->last_was_read)
		stream_flush (stream);
	
	return g_mime_stream_close (filter->source);
}

static gboolean
stream_eos (GMimeStream *stream)
{
	GMimeStreamFilter *filter = (GMimeStreamFilter *) stream;
	struct _GMimeStreamFilterPrivate *priv = filter->priv;
	
	if (priv->filteredlen > 0)
		return FALSE;
	
	if (!priv->flushed)
		return FALSE;
	
	return g_mime_stream_eos (filter->source);
}

static int
stream_reset (GMimeStream *stream)
{
	GMimeStreamFilter *filter = (GMimeStreamFilter *) stream;
	struct _GMimeStreamFilterPrivate *priv = filter->priv;
	struct _filter *f;
	
	if (g_mime_stream_reset (filter->source) == -1)
		return -1;
	
	priv->filteredlen = 0;
	priv->flushed = FALSE;
	
	/* and reset filters */
	f = priv->filters;
	while (f != NULL) {
		g_mime_filter_reset (f->filter);
		f = f->next;
	}
	
	return 0;
}

static gint64
stream_seek (GMimeStream *stream, gint64 offset, GMimeSeekWhence whence)
{
	return -1;
}

static gint64
stream_tell (GMimeStream *stream)
{
	return -1;
}

static gint64
stream_length (GMimeStream *stream)
{
	return stream->bound_end - stream->bound_start;
}

static GMimeStream *
stream_substream (GMimeStream *stream, gint64 start, gint64 end)
{
	GMimeStreamFilter *filter = (GMimeStreamFilter *) stream;
	GMimeStreamFilter *sub;
	
	sub = g_object_newv (GMIME_TYPE_STREAM_FILTER, 0, NULL);
	sub->source = filter->source;
	g_object_ref (sub->source);
	
	if (filter->priv->filters) {
		struct _filter *f, *sn, *s = NULL;
		
		f = filter->priv->filters;
		while (f) {
			sn = g_new (struct _filter, 1);
			sn->filter = g_mime_filter_copy (f->filter);
			sn->id = f->id;
			
			if (s) {
				s->next = sn;
				s = sn;
			} else {
				s = sub->priv->filters = sn;
			}
			
			f = f->next;
		}
		
		s->next = NULL;
		
		sub->priv->filterid = filter->priv->filterid;
	}
	
	g_mime_stream_construct (GMIME_STREAM (filter), start, end);
	
	return GMIME_STREAM (sub);
}


/**
 * g_mime_stream_filter_new:
 * @stream: source stream
 *
 * Creates a new #GMimeStreamFilter object using @stream as the source
 * stream.
 *
 * Returns: a new filter stream with @stream as its source.
 **/
GMimeStream *
g_mime_stream_filter_new (GMimeStream *stream)
{
	GMimeStreamFilter *filter;
	
	g_return_val_if_fail (GMIME_IS_STREAM (stream), NULL);
	
	filter = g_object_newv (GMIME_TYPE_STREAM_FILTER, 0, NULL);
	filter->source = stream;
	g_object_ref (stream);
	
	g_mime_stream_construct (GMIME_STREAM (filter),
				 stream->bound_start,
				 stream->bound_end);
	
	return GMIME_STREAM (filter);
}


/**
 * g_mime_stream_filter_add:
 * @stream: a #GMimeStreamFilter
 * @filter: a #GMimeFilter
 *
 * Adds @filter to @stream. Filters are applied in the same order in
 * which they are added.
 *
 * Returns: an id for the filter.
 **/
int
g_mime_stream_filter_add (GMimeStreamFilter *stream, GMimeFilter *filter)
{
	struct _GMimeStreamFilterPrivate *priv;
	struct _filter *f, *fn;
	
	g_return_val_if_fail (GMIME_IS_STREAM_FILTER (stream), -1);
	g_return_val_if_fail (GMIME_IS_FILTER (filter), -1);
	
	g_object_ref (filter);
	
	priv = stream->priv;
	
	fn = g_new (struct _filter, 1);
	fn->next = NULL;
	fn->filter = filter;
	fn->id = priv->filterid++;
	
	f = (struct _filter *) &priv->filters;
	while (f->next)
		f = f->next;
	
	f->next = fn;
	fn->next = NULL;
	
	return fn->id;
}


/**
 * g_mime_stream_filter_remove:
 * @stream: a #GMimeStreamFilter
 * @id: filter id
 *
 * Removed a filter from the stream based on the id (as returned from
 * filter_add).
 **/
void
g_mime_stream_filter_remove (GMimeStreamFilter *stream, int id)
{
	struct _GMimeStreamFilterPrivate *priv;
	struct _filter *f, *fn;
	
	g_return_if_fail (GMIME_IS_STREAM_FILTER (stream));
	
	priv = stream->priv;
	
	if (id == -1)
		return;
	
	f = (struct _filter *) &priv->filters;
	while (f && f->next) {
		fn = f->next;
		if (fn->id == id) {
			f->next = fn->next;
			g_object_unref (fn->filter);
			g_free (fn);
		}
		f = f->next;
	}
}
