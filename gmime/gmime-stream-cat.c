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

#include <errno.h>

#include "gmime-stream-cat.h"

#define d(x)


/**
 * SECTION: gmime-stream-cat
 * @title: GMimeStreamCat
 * @short_description: A concatenated stream
 * @see_also: #GMimeStream
 *
 * A #GMimeStream which chains together any number of other streams.
 **/


static void g_mime_stream_cat_class_init (GMimeStreamCatClass *klass);
static void g_mime_stream_cat_init (GMimeStreamCat *stream, GMimeStreamCatClass *klass);
static void g_mime_stream_cat_finalize (GObject *object);

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


struct _cat_node {
	struct _cat_node *next;
	GMimeStream *stream;
	gint64 position;
	int id; /* for debugging */
};

GType
g_mime_stream_cat_get_type (void)
{
	static GType type = 0;
	
	if (!type) {
		static const GTypeInfo info = {
			sizeof (GMimeStreamCatClass),
			NULL, /* base_class_init */
			NULL, /* base_class_finalize */
			(GClassInitFunc) g_mime_stream_cat_class_init,
			NULL, /* class_finalize */
			NULL, /* class_data */
			sizeof (GMimeStreamCat),
			0,    /* n_preallocs */
			(GInstanceInitFunc) g_mime_stream_cat_init,
		};
		
		type = g_type_register_static (GMIME_TYPE_STREAM, "GMimeStreamCat", &info, 0);
	}
	
	return type;
}


static void
g_mime_stream_cat_class_init (GMimeStreamCatClass *klass)
{
	GMimeStreamClass *stream_class = GMIME_STREAM_CLASS (klass);
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	
	parent_class = g_type_class_ref (GMIME_TYPE_STREAM);
	
	object_class->finalize = g_mime_stream_cat_finalize;
	
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
g_mime_stream_cat_init (GMimeStreamCat *stream, GMimeStreamCatClass *klass)
{
	stream->sources = NULL;
	stream->current = NULL;
}

static void
g_mime_stream_cat_finalize (GObject *object)
{
	GMimeStreamCat *cat = (GMimeStreamCat *) object;
	struct _cat_node *n, *nn;
	
	n = cat->sources;
	while (n != NULL) {
		nn = n->next;
		g_object_unref (n->stream);
		g_free (n);
		n = nn;
	}
	
	G_OBJECT_CLASS (parent_class)->finalize (object);
}

static ssize_t
stream_read (GMimeStream *stream, char *buf, size_t len)
{
	GMimeStreamCat *cat = (GMimeStreamCat *) stream;
	struct _cat_node *current;
	ssize_t nread = 0;
	gint64 offset;
	
	/* check for end-of-stream */
	if (stream->bound_end != -1 && stream->position >= stream->bound_end)
		return -1;
	
	/* don't allow our caller to read past the end of the stream */
	if (stream->bound_end != -1)
		len = (size_t) MIN (stream->bound_end - stream->position, (gint64) len);
	
	if (!(current = cat->current))
		return -1;
	
	/* make sure our stream position is where it should be */
	offset = current->stream->bound_start + current->position;
	if (g_mime_stream_seek (current->stream, offset, GMIME_STREAM_SEEK_SET) == -1)
		return -1;
	
	do {
		if ((nread = g_mime_stream_read (current->stream, buf, len)) <= 0) {
			cat->current = current = current->next;
			if (current != NULL) {
				if (g_mime_stream_reset (current->stream) == -1)
					return -1;
				current->position = 0;
			}
			nread = 0;
		} else if (nread > 0) {
			current->position += nread;
		}
	} while (nread == 0 && current != NULL);
	
	if (nread > 0)
		stream->position += nread;
	
	return nread;
}
 
static ssize_t
stream_write (GMimeStream *stream, const char *buf, size_t len)
{
	GMimeStreamCat *cat = (GMimeStreamCat *) stream;
	struct _cat_node *current;
	size_t nwritten = 0;
	ssize_t n = -1;
	gint64 offset;
	
	/* check for end-of-stream */
	if (stream->bound_end != -1 && stream->position >= stream->bound_end)
		return -1;
	
	/* don't allow our caller to write past the end of the stream */
	if (stream->bound_end != -1)
		len = (size_t) MIN (stream->bound_end - stream->position, (gint64) len);
	
	if (!(current = cat->current))
		return -1;
	
	/* make sure our stream position is where it should be */
	offset = current->stream->bound_start + current->position;
	if (g_mime_stream_seek (current->stream, offset, GMIME_STREAM_SEEK_SET) == -1)
		return -1;
	
	do {
		n = -1;
		while (!g_mime_stream_eos (current->stream) && nwritten < len) {
			if ((n = g_mime_stream_write (current->stream, buf + nwritten, len - nwritten)) <= 0)
				break;
			
			current->position += n;
			
			nwritten += n;
		}
		
		if (nwritten < len) {
			/* try spilling over into the next stream */
			current = current->next;
			if (current) {
				current->position = 0;
				if (g_mime_stream_reset (current->stream) == -1)
					break;
			} else {
				break;
			}
		}
	} while (nwritten < len);
	
	stream->position += nwritten;
	
	cat->current = current;
	
	if (n == -1 && nwritten == 0)
		return -1;
	
	return nwritten;
}

static int
stream_flush (GMimeStream *stream)
{
	GMimeStreamCat *cat = (GMimeStreamCat *) stream;
	struct _cat_node *node;
	int errnosav = 0;
	int rv = 0;
	
	/* flush all streams up to and including the current stream */
	
	node = cat->sources;
	while (node) {
		if (g_mime_stream_flush (node->stream) == -1) {
			if (errnosav == 0)
				errnosav = errno;
			rv = -1;
		}
		
		if (node == cat->current)
			break;
		
		node = node->next;
	}
	
	return rv;
}

static int
stream_close (GMimeStream *stream)
{
	GMimeStreamCat *cat = (GMimeStreamCat *) stream;
	struct _cat_node *n, *nn;
	
	cat->current = NULL;
	n = cat->sources;
	while (n != NULL) {
		nn = n->next;
		g_object_unref (n->stream);
		g_free (n);
		n = nn;
	}
	
	cat->sources = NULL;
	
	return 0;
}

static gboolean
stream_eos (GMimeStream *stream)
{
	GMimeStreamCat *cat = (GMimeStreamCat *) stream;
	
	if (cat->current == NULL)
		return TRUE;
	
	if (stream->bound_end != -1 && stream->position >= stream->bound_end)
		return TRUE;
	
	return FALSE;
}

static int
stream_reset (GMimeStream *stream)
{
	GMimeStreamCat *cat = (GMimeStreamCat *) stream;
	struct _cat_node *n;
	
	if (stream->position == stream->bound_start)
		return 0;
	
	n = cat->sources;
	while (n != NULL) {
		if (g_mime_stream_reset (n->stream) == -1)
			return -1;
		
		n->position = 0;
		n = n->next;
	}
	
	cat->current = cat->sources;
	
	return 0;
}

static gint64
stream_seek (GMimeStream *stream, gint64 offset, GMimeSeekWhence whence)
{
	GMimeStreamCat *cat = (GMimeStreamCat *) stream;
	struct _cat_node *current, *n;
	gint64 real, off, len;
	
	d(fprintf (stderr, "GMimeStreamCat::stream_seek (%p, %ld, %d)\n",
		   stream, offset, whence));
	
	if (cat->sources == NULL)
		return -1;
	
	switch (whence) {
	case GMIME_STREAM_SEEK_SET:
	seek_set:
		/* sanity check our seek - make sure we don't under/over-seek our bounds */
		if (offset < 0) {
			d(fprintf (stderr, "offset %ld < 0, fail\n", offset));
			return -1;
		}
		
		/* sanity check our seek */
		if (stream->bound_end != -1 && offset > stream->bound_end) {
			d(fprintf (stderr, "offset %ld > bound_end %ld, fail\n",
				   offset, stream->bound_end));
			return -1;
		}
		
		/* short-cut if we are seeking to our current position */
		if (offset == stream->position) {
			d(fprintf (stderr, "offset %ld == stream->position %ld, no need to seek\n",
				   offset, stream->position));
			return offset;
		}
		
		real = 0;
		n = cat->sources;
		current = cat->current;
		
		while (n != current) {
			if (real + n->position > offset)
				break;
			real += n->position;
			n = n->next;
		}
		
		if (n == NULL) {
			/* offset not within our grasp... */
			return -1;
		}
		
		if (n != current) {
			/* seeking to a previous stream (n->stream) */
			if ((offset - real) != n->position) {
				/* FIXME: could probably skip these seek checks... */
				off = n->stream->bound_start + (offset - real);
				if (g_mime_stream_seek (n->stream, off, GMIME_STREAM_SEEK_SET) == -1)
					return -1;
			}
			
			d(fprintf (stderr, "setting current stream to %i and updating cur->position to %ld\n",
				   n->id, offset - real));
			
			current = n;
			current->position = offset - real;
			
			break;
		} else {
			/* seeking to someplace in our current (or next) stream */
			d(fprintf (stderr, "seek offset %ld in current stream[%d] or after\n",
				   offset, current->id));
			if ((offset - real) == current->position) {
				/* exactly at our current position */
				d(fprintf (stderr, "seek offset at cur position of stream[%d]\n",
					   current->id));
				stream->position = offset;
				return offset;
			}
			
			if ((offset - real) < current->position) {
				/* in current stream, but before current position */
				d(fprintf (stderr, "seeking backwards in cur stream[%d]\n",
					   current->id));
				/* FIXME: again, could probably skip seek checks... */
				off = current->stream->bound_start + (offset - real);
				if (g_mime_stream_seek (current->stream, off, GMIME_STREAM_SEEK_SET) == -1)
					return -1;
				
				d(fprintf (stderr, "setting cur stream[%d] position to %ld\n",
					   current->id, offset - real));
				current->position = offset - real;
				
				break;
			}
			
			/* after our current position */
			d(fprintf (stderr, "after cur position in stream[%d] or in a later stream\n",
				   current->id));
			do {
				if (current->stream->bound_end != -1) {
					len = current->stream->bound_end - current->stream->bound_start;
				} else {
					if ((len = g_mime_stream_length (current->stream)) == -1)
						return -1;
				}
				
				d(fprintf (stderr, "real = %lld, stream[%d] len = %lld\n",
					   real, current->id, len));
				
				if ((real + len) > offset) {
					/* within the bounds of the current stream */
					d(fprintf (stderr, "offset within bounds of stream[%d]\n",
						   current->id));
					break;
				} else {
					d(fprintf (stderr, "not within bounds of stream[%d]\n",
						   current->id));
					current->position = len;
					real += len;
					
					current = current->next;
					if (current == NULL) {
						d(fprintf (stderr, "ran out of streams, failed\n"));
						return -1;
					}
					
					d(fprintf (stderr, "advanced to stream[%d]...\n", current->id));
					
					if (g_mime_stream_reset (current->stream) == -1)
						return -1;
					
					current->position = 0;
				}
			} while (1);
			
			/* FIXME: another seek check... probably can skip this */
			off = current->stream->bound_start + (offset - real);
			if (g_mime_stream_seek (current->stream, off, GMIME_STREAM_SEEK_SET) == -1)
				return -1;
			
			d(fprintf (stderr, "setting cur position of stream[%d] to %ld\n",
				   current->id, offset - real));
			current->position = offset - real;
		}
		
		break;
	case GMIME_STREAM_SEEK_CUR:
		if (offset == 0)
			return stream->position;
		
		/* calculate offset relative to the beginning of the stream */
		offset = stream->position + offset;
		goto seek_set;
		break;
	case GMIME_STREAM_SEEK_END:
		if (offset > 0)
			return -1;
		
		/* calculate the offset of the end of the stream */
		n = cat->sources;
		real = stream->bound_start;
		while (n != NULL) {
			if ((len = g_mime_stream_length (n->stream)) == -1)
				return -1;
			
			real += len;
			n = n->next;
		}
		
		/* calculate offset relative to the beginning of the stream */
		offset = real + offset;
		goto seek_set;
		break;
	default:
		g_assert_not_reached ();
		return -1;
	}
	
	d(fprintf (stderr, "setting stream->offset to %ld and current stream to %d\n",
		   offset, current->id));
	
	stream->position = offset;
	cat->current = current;
	
	/* reset all following streams */
	n = current->next;
	while (n != NULL) {
		if (g_mime_stream_reset (n->stream) == -1)
			return -1;
		n->position = 0;
		n = n->next;
	}
	
	return offset;
}

static gint64
stream_tell (GMimeStream *stream)
{
	return stream->position;
}

static gint64
stream_length (GMimeStream *stream)
{
	GMimeStreamCat *cat = GMIME_STREAM_CAT (stream);
	gint64 len, total = 0;
	struct _cat_node *n;
	
	if (stream->bound_end != -1)
		return stream->bound_end - stream->bound_start;
	
	n = cat->sources;
	while (n != NULL) {
		if ((len = g_mime_stream_length (n->stream)) == -1)
			return -1;
		
		total += len;
		n = n->next;
	}
	
	return total;
}

struct _sub_node {
	struct _sub_node *next;
	GMimeStream *stream;
	gint64 start, end;
};

static GMimeStream *
stream_substream (GMimeStream *stream, gint64 start, gint64 end)
{
	GMimeStreamCat *cat = (GMimeStreamCat *) stream;
	struct _sub_node *streams, *tail, *s;
	gint64 offset = 0, subend = 0;
	GMimeStream *substream;
	struct _cat_node *n;
	gint64 len;
	
	d(fprintf (stderr, "GMimeStreamCat::substream (%p, %ld, %ld)\n", stream, start, end));
	
	/* find the first source stream that contains data we're interested in... */
	n = cat->sources;
	while (offset < start && n != NULL) {
		if (n->stream->bound_end == -1) {
			if ((len = g_mime_stream_length (n->stream)) == -1)
				return NULL;
		} else {
			len = n->stream->bound_end - n->stream->bound_start;
		}
		
		if ((offset + len) > start)
			break;
		
		if (end != -1 && (offset + len) >= end)
			break;
		
		offset += len;
		
		n = n->next;
	}
	
	if (n == NULL)
		return NULL;
	
	d(fprintf (stderr, "stream[%d] is the first stream containing data we want\n", n->id));
	
	streams = NULL;
	tail = (struct _sub_node *) &streams;
	
	do {
		s = g_new (struct _sub_node, 1);
		s->next = NULL;
		s->stream = n->stream;
		tail->next = s;
		tail = s;
		
		s->start = n->stream->bound_start;
		if (n == cat->sources)
			s->start += start;
		else if (offset < start)
			s->start += (start - offset);
		
		d(fprintf (stderr, "added stream[%d] to our list\n", n->id));
		
		if (n->stream->bound_end == -1) {
			if ((len = g_mime_stream_length (n->stream)) == -1)
				goto error;
		} else {
			len = n->stream->bound_end - n->stream->bound_start;
		}
		
		d(fprintf (stderr, "stream[%d]: len = %ld, offset of beginning of stream is %ld\n",
			   n->id, len, offset));
		
		if (end != -1 && (end <= (offset + len))) {
			d(fprintf (stderr, "stream[%d]: requested end <= offset + len\n", n->id));
			s->end = n->stream->bound_start + (end - offset);
			d(fprintf (stderr, "stream[%d]: s->start = %ld, s->end = %ld; break\n",
				   n->id, s->start, s->end));
			subend += (end - offset);
			break;
		} else {
			s->end = n->stream->bound_start + len;
			d(fprintf (stderr, "stream[%d]: s->start = %ld, s->end = %ld\n",
				   n->id, s->start, s->end));
		}
		
		subend += (s->end - s->start);
		offset += len;
		
		n = n->next;
	} while (n != NULL);
	
	d(fprintf (stderr, "returning a substream containing multiple source streams\n"));
	cat = g_object_newv (GMIME_TYPE_STREAM_CAT, 0, NULL);
	/* Note: we could pass -1 as bound_end, it should Just
	 * Work(tm) but setting absolute bounds is kinda
	 * nice... */
	g_mime_stream_construct (GMIME_STREAM (cat), 0, subend);
	
	while (streams != NULL) {
		s = streams->next;
		substream = g_mime_stream_substream (streams->stream, streams->start, streams->end);
		g_mime_stream_cat_add_source (cat, substream);
		g_object_unref (substream);
		g_free (streams);
		streams = s;
	}
	
	substream = (GMimeStream *) cat;
	
	return substream;
	
 error:
	
	while (streams != NULL) {
		s = streams->next;
		g_free (streams);
		streams = s;
	}
	
	return NULL;
}


/**
 * g_mime_stream_cat_new:
 *
 * Creates a new #GMimeStreamCat object.
 *
 * Returns: a new #GMimeStreamCat stream.
 **/
GMimeStream *
g_mime_stream_cat_new (void)
{
	GMimeStream *stream;
	
	stream = g_object_newv (GMIME_TYPE_STREAM_CAT, 0, NULL);
	g_mime_stream_construct (stream, 0, -1);
	
	return stream;
}


/**
 * g_mime_stream_cat_add_source:
 * @cat: a #GMimeStreamCat
 * @source: a source stream
 *
 * Adds the @source stream to the @cat.
 *
 * Returns: %0 on success or %-1 on fail.
 **/
int
g_mime_stream_cat_add_source (GMimeStreamCat *cat, GMimeStream *source)
{
	struct _cat_node *node, *n;
	
	g_return_val_if_fail (GMIME_IS_STREAM_CAT (cat), -1);
	g_return_val_if_fail (GMIME_IS_STREAM (source), -1);
	
	node = g_new (struct _cat_node, 1);
	node->next = NULL;
	node->stream = source;
	g_object_ref (source);
	node->position = 0;
	
	n = cat->sources;
	while (n && n->next)
		n = n->next;
	
	if (n == NULL) {
		cat->sources = node;
		node->id = 0;
	} else {
		node->id = n->id + 1;
		n->next = node;
	}
	
	if (!cat->current)
		cat->current = node;
	
	return 0;
}
