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

#include "gmime-stream-filter.h"
#include "gmime-internal.h"


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
 *
 * Flushing completes the write-side filters once until more input is
 * written. Unwritten completion output is retained if the source fails,
 * so flushing or closing again can retry it without completing the filters
 * twice. Closing reports these errors before closing an owned source.
 * Flush does not flush the source stream itself.
 *
 * Reading completes the filters once at end-of-stream and drains their
 * final output before reporting EOF. Reset discards buffered output and
 * filter state and starts a fresh conversion at the source's reset position.
 * Do not manipulate filters or the source while output is buffered.
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
	ssize_t written;
	
	int last_was_read:1;	/* was the last op read or write? */
	int flushed:1;		/* have the write-side filters been flushed? */
	int eos:1;		/* have the read-side filters reached EOF? */
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
	stream->owner = FALSE;
	stream->priv = g_new (struct _GMimeStreamFilterPrivate, 1);
	stream->priv->filters = NULL;
	stream->priv->filterid = 0;
	stream->priv->realbuffer = g_malloc (READ_SIZE + READ_PAD);
	stream->priv->buffer = stream->priv->realbuffer + READ_PAD;
	stream->priv->last_was_read = FALSE;
	stream->priv->filteredlen = 0;
	stream->priv->flushed = FALSE;
	stream->priv->eos = FALSE;
	stream->priv->written = 0;
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
	
	/* Filters may consume input without producing output (for example, a
	 * trailing CR), so keep reading until output or completed EOF. */
	while (priv->filteredlen == 0) {
		size_t presize = READ_PAD;
		
		if (priv->eos)
			return 0;

		nread = g_mime_stream_read (filter->source, priv->buffer, READ_SIZE);
		if (nread < 0)
			return -1;
		if (nread == 0) {
			if (g_mime_stream_eos (filter->source)) {
				/* At EOF, flush any data buffered by the filters once. */
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
				priv->eos = TRUE;
			}
			
			if (nread <= 0)
				return nread;
		} else {
			priv->filtered = priv->buffer;
			priv->filteredlen = nread;
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

static int
write_filtered (GMimeStreamFilter *filter, char **buffer, size_t *len)
{
	ssize_t nwritten;

	if (*len > (size_t) (G_MAXSSIZE - filter->priv->written)) {
		errno = EOVERFLOW;
		return -1;
	}

	/* Native writes can be short. Retain the unwritten suffix on failure,
	 * rather than replaying bytes already accepted by the source. */
	while (*len > 0) {
		if ((nwritten = g_mime_stream_write (filter->source, *buffer, *len)) == -1)
			return -1;
		if (nwritten == 0) {
			errno = EIO;
			return -1;
		}
		filter->priv->written += nwritten;
		*buffer += nwritten;
		*len -= nwritten;
	}

	return 0;
}

ssize_t
_g_mime_stream_filter_get_written (GMimeStreamFilter *stream)
{
	return stream->priv->written;
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
	
	/* Do not overwrite a filter's retained completion buffer with new input
	 * until a previous failed flush has finished writing it. */
	if (!priv->last_was_read && priv->flushed && priv->filteredlen > 0 && stream_flush (stream) == -1)
		return -1;

	priv->last_was_read = FALSE;
	priv->flushed = FALSE;
	
	f = priv->filters;
	presize = 0;
	while (f != NULL) {
		g_mime_filter_filter (f->filter, buffer, n, presize, &buffer, &n, &presize);
		
		f = f->next;
	}
	
	if (write_filtered (filter, &buffer, &n) == -1)
		return -1;
	
	/* return original input len because that's what our caller expects */
	return nwritten;
}

static int
stream_flush (GMimeStream *stream)
{
	GMimeStreamFilter *filter = (GMimeStreamFilter *) stream;
	struct _GMimeStreamFilterPrivate *priv = filter->priv;
	size_t presize;
	struct _filter *f;
	
	if (priv->last_was_read) {
		/* no-op */
		return 0;
	}
	
	if (!priv->flushed) {
		priv->filtered = "";
		priv->filteredlen = 0;
		presize = 0;
		f = priv->filters;

		while (f != NULL) {
			g_mime_filter_complete (f->filter, priv->filtered, priv->filteredlen, presize,
						&priv->filtered, &priv->filteredlen, &presize);
			f = f->next;
		}

		/* Completion changes filter state. Keep its output for retries
		 * instead of completing again after a source write failure. */
		priv->flushed = TRUE;
	}
	
	return write_filtered (filter, &priv->filtered, &priv->filteredlen);
}

static int
stream_close (GMimeStream *stream)
{
	GMimeStreamFilter *filter = (GMimeStreamFilter *) stream;
	struct _GMimeStreamFilterPrivate *priv = filter->priv;
	
	if (!priv->last_was_read && stream_flush (stream) == -1)
		return -1;
	
	if (filter->owner)
		return g_mime_stream_close (filter->source);
	
	return 0;
}

static gboolean
stream_eos (GMimeStream *stream)
{
	GMimeStreamFilter *filter = (GMimeStreamFilter *) stream;
	struct _GMimeStreamFilterPrivate *priv = filter->priv;
	
	if (priv->filteredlen > 0)
		return FALSE;
	
	return priv->eos != 0;
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
	priv->eos = FALSE;
	priv->last_was_read = FALSE;
	priv->written = 0;
	
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
	
	sub = g_object_new (GMIME_TYPE_STREAM_FILTER, NULL);
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
	
	g_mime_stream_construct ((GMimeStream *) filter, start, end);
	
	return (GMimeStream *) sub;
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
	
	filter = g_object_new (GMIME_TYPE_STREAM_FILTER, NULL);
	filter->source = stream;
	g_object_ref (stream);
	
	g_mime_stream_construct ((GMimeStream *) filter,
				 stream->bound_start,
				 stream->bound_end);
	
	return (GMimeStream *) filter;
}


/**
 * g_mime_stream_filter_get_source:
 * @stream: a #GMimeStreamFilter
 *
 * Gets the underlying source stream. Manipulating the source directly is
 * generally unsafe because filters retain state about previous input and
 * may have buffered output.
 *
 * Returns: (transfer none): the source stream.
 **/
GMimeStream *
g_mime_stream_filter_get_source (GMimeStreamFilter *stream)
{
	g_return_val_if_fail (GMIME_IS_STREAM_FILTER (stream), NULL);

	return stream->source;
}


/**
 * g_mime_stream_filter_add:
 * @stream: a #GMimeStreamFilter
 * @filter: a #GMimeFilter
 *
 * Adds @filter to @stream. Filters are applied in the same order in
 * which they are added. The same filter can be added more than once;
 * each occurrence retains its own reference.
 *
 * Returns: an id for the filter.
 **/
int
g_mime_stream_filter_add (GMimeStreamFilter *stream, GMimeFilter *filter)
{
	struct _GMimeStreamFilterPrivate *priv;
	struct _filter **link, *fn;
	
	g_return_val_if_fail (GMIME_IS_STREAM_FILTER (stream), -1);
	g_return_val_if_fail (GMIME_IS_FILTER (filter), -1);
	
	g_object_ref (filter);
	
	priv = stream->priv;
	
	fn = g_new (struct _filter, 1);
	fn->next = NULL;
	fn->filter = filter;
	fn->id = priv->filterid++;
	
	link = &priv->filters;
	while (*link)
		link = &(*link)->next;

	*link = fn;
	
	return fn->id;
}


/**
 * g_mime_stream_filter_contains:
 * @stream: a #GMimeStreamFilter
 * @filter: a #GMimeFilter
 *
 * Checks whether @stream contains @filter, comparing filters by identity.
 *
 * Returns: %TRUE if @filter is present or %FALSE otherwise.
 **/
gboolean
g_mime_stream_filter_contains (GMimeStreamFilter *stream, GMimeFilter *filter)
{
	struct _filter *f;

	g_return_val_if_fail (GMIME_IS_STREAM_FILTER (stream), FALSE);
	g_return_val_if_fail (GMIME_IS_FILTER (filter), FALSE);

	for (f = stream->priv->filters; f; f = f->next) {
		if (f->filter == filter)
			return TRUE;
	}

	return FALSE;
}

static void
remove_filter (struct _filter **link)
{
	struct _filter *f = *link;

	/* Unlink before releasing the reference so finalizers see the updated chain. */
	*link = f->next;
	g_object_unref (f->filter);
	g_free (f);
}

/**
 * g_mime_stream_filter_remove_filter:
 * @stream: a #GMimeStreamFilter
 * @filter: a #GMimeFilter
 *
 * Removes the first occurrence of @filter, comparing filters by identity.
 * Remaining filters keep their order and state. Removing a filter releases
 * only the reference belonging to that occurrence.
 *
 * Do not modify the chain while filtered output is buffered.
 *
 * Returns: %TRUE if an occurrence was removed or %FALSE if not found.
 **/
gboolean
g_mime_stream_filter_remove_filter (GMimeStreamFilter *stream, GMimeFilter *filter)
{
	struct _filter **link;

	g_return_val_if_fail (GMIME_IS_STREAM_FILTER (stream), FALSE);
	g_return_val_if_fail (GMIME_IS_FILTER (filter), FALSE);

	for (link = &stream->priv->filters; *link; link = &(*link)->next) {
		if ((*link)->filter == filter) {
			/* Duplicates are independent entries; remove only the first one. */
			remove_filter (link);
			return TRUE;
		}
	}

	return FALSE;
}


/**
 * g_mime_stream_filter_remove:
 * @stream: a #GMimeStreamFilter
 * @id: filter id
 *
 * Removes a filter from the stream based on the id returned from
 * g_mime_stream_filter_add(). Other occurrences of the same filter are
 * unaffected. An unknown id does nothing.
 *
 * Do not modify the chain while filtered output is buffered.
 **/
void
g_mime_stream_filter_remove (GMimeStreamFilter *stream, int id)
{
	struct _filter **link;
	
	g_return_if_fail (GMIME_IS_STREAM_FILTER (stream));
	
	if (id == -1)
		return;
	
	for (link = &stream->priv->filters; *link; link = &(*link)->next) {
		if ((*link)->id == id) {
			remove_filter (link);
			return;
		}
	}
}


/**
 * g_mime_stream_filter_set_owner:
 * @stream: a #GMimeStreamFilter
 * @owner: %TRUE if this stream should own the source stream or %FALSE otherwise
 *
 * Sets whether or not @stream owns the source stream..
 *
 * Note: @owner should be %TRUE if explicitly closing the stream should
 * close its source, or %FALSE to leave the source open. Finalization
 * does not complete the filters or explicitly close the source.
 **/
void
g_mime_stream_filter_set_owner (GMimeStreamFilter *stream, gboolean owner)
{
	g_return_if_fail (GMIME_IS_STREAM_FILTER (stream));
	
	stream->owner = owner;
}


/**
 * g_mime_stream_filter_get_owner:
 * @stream: a #GMimeStreamFilter
 *
 * Gets whether or not @stream owns the source stream.
 *
 * Returns: %TRUE if @stream owns the source stream or %FALSE
 * otherwise.
 **/
gboolean
g_mime_stream_filter_get_owner (GMimeStreamFilter *stream)
{
	g_return_val_if_fail (GMIME_IS_STREAM_FILTER (stream), FALSE);
	
	return stream->owner;
}
