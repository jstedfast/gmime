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

#include <errno.h>

#include "gmime-stream-cat.h"

#define d(x)


#include "gmime-object-private.h"

/**
 * SECTION: gmime-stream-cat
 * @title: GMimeStreamCat
 * @short_description: A concatenated stream
 * @see_also: #GMimeStream
 *
 * A #GMimeStream which chains together any number of other streams.
 *
 * Reads fill the requested buffer across child-stream boundaries without
 * seeking or resetting the children. Sources are read from their current
 * positions and are retained by reference. Explicit seek and reset operations
 * still require the affected children to support repositioning.
 **/


static void g_mime_stream_cat_class_init (GMimeStreamCatClass *klass);
static void g_mime_stream_cat_init (GMimeStreamCat *stream);
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


G_DEFINE_TYPE_WITH_PRIVATE (GMimeStreamCat, g_mime_stream_cat, GMIME_TYPE_STREAM)

GMimeStreamCatPrivate *
_g_mime_stream_cat_get_private (GMimeStreamCat *object)
{
	return g_mime_stream_cat_get_instance_private (object);
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
g_mime_stream_cat_init (GMimeStreamCat *stream)
{
	GMimeStreamCatPrivate *priv = _g_mime_stream_cat_get_private (stream);

	priv->sources = NULL;
	priv->current = NULL;
	priv->read_eof = FALSE;
}

static void
g_mime_stream_cat_finalize (GObject *object)
{
	GMimeStream *stream = (GMimeStream *) object;
	
	stream_close (stream);
	
	G_OBJECT_CLASS (parent_class)->finalize (object);
}

static ssize_t
stream_read (GMimeStream *stream, char *buf, size_t len)
{
	GMimeStreamCat *cat = (GMimeStreamCat *) stream;
	GMimeStreamCatPrivate *cat_priv = _g_mime_stream_cat_get_private (cat);
	GMimeStreamPrivate *stream_priv;
	struct _cat_node *current;
	size_t count, nread = 0;
	gint64 available;
	ssize_t n;
	int errsv;

	if (cat_priv->read_eof)
		return 0;
	
	/* check for end-of-stream */
	stream_priv = _g_mime_stream_get_private (stream);

	if (stream_priv->bound_end != -1 && stream_priv->position >= stream_priv->bound_end)
		return 0;
	
	/* The result must fit in ssize_t even when several children contribute. */
	len = MIN (len, (size_t) G_MAXSSIZE);

	/* don't allow our caller to read past the end of the stream */
	if (stream_priv->bound_end != -1)
		len = (size_t) MIN (stream_priv->bound_end - stream_priv->position, (gint64) len);
	
	current = cat_priv->current;
	while (nread < len && current != NULL) {
		available = MIN (G_MAXINT64 - stream_priv->position, G_MAXINT64 - current->position);
		count = (size_t) MIN ((guint64) (len - nread), (guint64) available);

		if (count == 0) {
			errno = EOVERFLOW;
			return nread > 0 ? (ssize_t) nread : -1;
		}

		/* Read before consulting EOS: filters may still have completion output. */
		n = g_mime_stream_read (current->stream, buf + nread, count);
		if (n < 0) {
			errsv = errno;
			/* Native bounded streams use EINVAL at EOF. Other errors must
			 * preserve accepted bytes and leave this child for retry. */
			if (errsv != EINVAL || !g_mime_stream_eos (current->stream)) {
				errno = errsv;
				return nread > 0 ? (ssize_t) nread : -1;
			}
		}

		if (n <= 0) {
			/* A transient zero read is not evidence that this child is finished. */
			if (n == 0 && !g_mime_stream_eos (current->stream))
				break;

			cat_priv->current = current = current->next;
			continue;
		}

		/* Account for progress before another read can fail. */
		current->position += n;
		stream_priv->position += n;
		nread += n;
	}
	
	/* Writes can select the tail again without reopening an exhausted reader. */
	if (nread == 0 && cat_priv->current == NULL)
		cat_priv->read_eof = TRUE;

	return nread;
}
 
static ssize_t
stream_write (GMimeStream *stream, const char *buf, size_t len)
{
	GMimeStreamPrivate *stream_priv = _g_mime_stream_get_private (stream);
	GMimeStreamCat *cat = (GMimeStreamCat *) stream;
	GMimeStreamCatPrivate *cat_priv;
	gint64 available, length, position;
	struct _cat_node *current;
	size_t count, nwritten = 0;
	ssize_t n;
	
	/* check for end-of-stream */
	if (stream_priv->bound_end != -1 && stream_priv->position >= stream_priv->bound_end) {
		errno = EINVAL;
		return -1;
	}
	
	len = MIN (len, (size_t) G_MAXSSIZE);

	/* don't allow our caller to write past the end of the stream */
	if (stream_priv->bound_end != -1)
		len = (size_t) MIN (stream_priv->bound_end - stream_priv->position, (gint64) len);
	
	cat_priv = _g_mime_stream_cat_get_private (cat);

	current = cat_priv->current;
	if (current == NULL) {
		/* After read-side EOF, writes resume at the final child's position. */
		current = cat_priv->sources;

		while (current != NULL && current->next != NULL)
			current = current->next;

		if (current == NULL) {
			errno = EBADF;
			return -1;
		}

		cat_priv->current = current;
	}

	while (nwritten < len) {
		available = MIN (G_MAXINT64 - stream_priv->position, G_MAXINT64 - current->position);
		count = (size_t) MIN ((guint64) (len - nwritten), (guint64) available);

		if (count == 0) {
			errno = EOVERFLOW;
			return nwritten > 0 ? (ssize_t) nwritten : -1;
		}

		/* Earlier children keep their existing lengths; the final child may grow.
		 * Do not require Length, Position or seeking from a write-only tail. */
		if (current->next != NULL) {
			GMimeStreamPrivate *current_priv = _g_mime_stream_get_private (current->stream);

			if ((length = g_mime_stream_length (current->stream)) < 0 ||
			    (position = g_mime_stream_tell (current->stream)) < 0)
				return nwritten > 0 ? (ssize_t) nwritten : -1;

			if (position < current_priv->bound_start ||
			    position - current_priv->bound_start > length) {
				errno = EINVAL;
				return nwritten > 0 ? (ssize_t) nwritten : -1;
			}

			available = length - (position - current_priv->bound_start);
			count = (size_t) MIN ((guint64) count, (guint64) available);

			if (count == 0) {
				/* Keep this child current until its flush succeeds, so a retry
				 * cannot skip completion output or replay accepted bytes. */
				if (g_mime_stream_flush (current->stream) == -1)
					return nwritten > 0 ? (ssize_t) nwritten : -1;
				cat_priv->current = current = current->next;
				continue;
			}
		}

		n = g_mime_stream_write (current->stream, buf + nwritten, count);

		if (n <= 0) {
			if (n == 0)
				errno = EIO;

			return nwritten > 0 ? (ssize_t) nwritten : -1;
		}

		/* Native writes may be short. Account for progress before retrying. */
		current->position += n;
		stream_priv->position += n;
		nwritten += n;
	}

	return nwritten;
}

static int
stream_flush (GMimeStream *stream)
{
	GMimeStreamCat *cat = (GMimeStreamCat *) stream;
	GMimeStreamCatPrivate *priv = _g_mime_stream_cat_get_private (cat);

	/* Earlier write destinations were flushed when advancing past them. */
	if (priv->current != NULL)
		return g_mime_stream_flush (priv->current->stream);

	return 0;
}

static int
stream_close (GMimeStream *stream)
{
	GMimeStreamCat *cat = (GMimeStreamCat *) stream;
	GMimeStreamCatPrivate *priv = _g_mime_stream_cat_get_private (cat);
	struct _cat_node *n, *nn;

	priv->current = NULL;
	n = priv->sources;

	while (n != NULL) {
		nn = n->next;
		g_object_unref (n->stream);
		g_free (n);
		n = nn;
	}
	
	priv->sources = NULL;
	
	return 0;
}

static gboolean
stream_eos (GMimeStream *stream)
{
	GMimeStreamCat *cat = (GMimeStreamCat *) stream;
	GMimeStreamCatPrivate *cat_priv = _g_mime_stream_cat_get_private (cat);
	GMimeStreamPrivate *stream_priv;

	if (cat_priv->read_eof || cat_priv->current == NULL)
		return TRUE;
	
	stream_priv = _g_mime_stream_get_private (stream);

	if (stream_priv->bound_end != -1 && stream_priv->position >= stream_priv->bound_end)
		return TRUE;
	
	return FALSE;
}

static int
stream_reset (GMimeStream *stream)
{
	GMimeStreamCat *cat = (GMimeStreamCat *) stream;
	GMimeStreamCatPrivate *priv = _g_mime_stream_cat_get_private (cat);
	struct _cat_node *n;
	
	/* A failed backward seek can leave later children displaced even when
	 * the concatenation is already positioned at its beginning. */
	n = priv->sources;

	while (n != NULL) {
		if (g_mime_stream_reset (n->stream) == -1)
			return -1;
		
		n->position = 0;
		n = n->next;
	}
	
	priv->current = priv->sources;
	priv->read_eof = FALSE;
	
	return 0;
}

static gint64
stream_seek (GMimeStream *stream, gint64 offset, GMimeSeekWhence whence)
{
	GMimeStreamCat *cat = (GMimeStreamCat *) stream;
	GMimeStreamCatPrivate *cat_priv = _g_mime_stream_cat_get_private (cat);
	struct _cat_node *current, *stop, *node;
	gint64 origin, real, length = -1, position, left, n;
	GMimeStreamPrivate *stream_priv;

	if (cat_priv->sources == NULL) {
		errno = EBADF;
		return -1;
	}
	
	stream_priv = _g_mime_stream_get_private (stream);

	switch (whence) {
	case GMIME_STREAM_SEEK_SET:
		origin = 0;
		break;
	case GMIME_STREAM_SEEK_CUR:
		origin = stream_priv->position;
		break;
	case GMIME_STREAM_SEEK_END:
		if ((length = stream_length (stream)) < 0)
			return -1;

		if (length > G_MAXINT64 - stream_priv->bound_start) {
			errno = EOVERFLOW;
			return -1;
		}

		origin = stream_priv->bound_start + length;
		break;
	default:
		errno = EINVAL;
		return -1;
	}

	/* Origins are nonnegative; only a positive offset can overflow the sum. */
	if (offset > 0 && origin > G_MAXINT64 - offset) {
		errno = EOVERFLOW;
		return -1;
	}

	real = origin + offset;

	if (real < stream_priv->bound_start) {
		errno = EINVAL;
		return -1;
	}

	/* Short-cut if we are seeking to our current position, including EOF. */
	if (real == stream_priv->position)
		return real;

	if (length < 0 && (length = stream_length (stream)) < 0)
		return -1;

	if (real - stream_priv->bound_start > length) {
		errno = EINVAL;
		return -1;
	}

	if (real > stream_priv->position) {
		current = cat_priv->current;

		while (current != NULL && stream_priv->position < real) {
			GMimeStreamPrivate *current_priv = _g_mime_stream_get_private (current->stream);

			if ((length = g_mime_stream_length (current->stream)) < 0 ||
			    (position = g_mime_stream_tell (current->stream)) < 0)
				return -1;

			if (position < current_priv->bound_start ||
			    position - current_priv->bound_start > length) {
				errno = EINVAL;
				return -1;
			}

			left = length - (position - current_priv->bound_start);
			n = MIN (left, real - stream_priv->position);

			if (n > G_MAXINT64 - position || n > G_MAXINT64 - current->position) {
				errno = EOVERFLOW;
				return -1;
			}

			/* Move every skipped child physically, retaining a full child
			 * as current when the requested offset lands exactly at its end. */
			if (g_mime_stream_seek (current->stream, n, GMIME_STREAM_SEEK_CUR) < 0)
				return -1;

			current->position += n;
			stream_priv->position += n;

			if (stream_priv->position < real)
				cat_priv->current = current = current->next;
		}

		cat_priv->read_eof = current == NULL;
	} else {
		/* Find the child which encompasses our seek offset. Only children
		 * between it and the old current child need to be repositioned. */
		stop = cat_priv->current != NULL ? cat_priv->current->next : NULL;
		current = cat_priv->sources;
		stream_priv->position = stream_priv->bound_start;

		while (current != stop) {
			if ((length = g_mime_stream_length (current->stream)) < 0)
				return -1;

			n = real - stream_priv->position;

			if (n < length) {
				GMimeStreamPrivate *current_priv = _g_mime_stream_get_private (current->stream);

				if (n > G_MAXINT64 - current_priv->bound_start) {
					errno = EOVERFLOW;
					return -1;
				}

				position = current_priv->bound_start + n;

				if (g_mime_stream_seek (current->stream, position, GMIME_STREAM_SEEK_SET) < 0)
					return -1;

				current->position = n;
				stream_priv->position = real;
				break;
			}

			current->position = length;
			stream_priv->position += length;
			current = current->next;
		}

		cat_priv->current = current;

		/* Seek rather than reset: repositioning must not reset child state. */
		node = current != stop ? current->next : stop;
		while (node != stop) {
			GMimeStreamPrivate *node_priv = _g_mime_stream_get_private (node->stream);

			if (g_mime_stream_seek (node->stream, node_priv->bound_start, GMIME_STREAM_SEEK_SET) < 0)
				return -1;

			node->position = 0;
			node = node->next;
		}

		cat_priv->read_eof = FALSE;
	}

	return stream_priv->position;
}

static gint64
stream_tell (GMimeStream *stream)
{
	GMimeStreamPrivate *priv = _g_mime_stream_get_private (stream);

	return priv->position;
}

static gint64
stream_length (GMimeStream *stream)
{
	GMimeStreamPrivate *stream_priv = _g_mime_stream_get_private (stream);
	GMimeStreamCat *cat = GMIME_STREAM_CAT (stream);
	GMimeStreamCatPrivate *cat_priv;
	gint64 len, total = 0;
	struct _cat_node *n;

	if (stream_priv->bound_end != -1)
		return stream_priv->bound_end - stream_priv->bound_start;
	
	cat_priv = _g_mime_stream_cat_get_private (cat);

	n = cat_priv->sources;
	while (n != NULL) {
		if ((len = g_mime_stream_length (n->stream)) < 0)
			return -1;

		if (len > G_MAXINT64 - total) {
			errno = EOVERFLOW;
			return -1;
		}

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
	GMimeStreamCatPrivate *priv = _g_mime_stream_cat_get_private (cat);
	struct _sub_node *streams, *tail, *s;
	gint64 offset = 0, subend = 0;
	GMimeStream *substream;
	struct _cat_node *n;
	gint64 len;
	
	d(fprintf (stderr, "GMimeStreamCat::substream (%p, %ld, %ld)\n", stream, start, end));
	
	/* find the first source stream that contains data we're interested in... */
	n = priv->sources;
	while (offset < start && n != NULL) {
		GMimeStreamPrivate *source_priv = _g_mime_stream_get_private (n->stream);

		if (source_priv->bound_end == -1) {
			if ((len = g_mime_stream_length (n->stream)) == -1)
				return NULL;
		} else {
			len = source_priv->bound_end - source_priv->bound_start;
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
	
	/* Little gimmick that allows us to set `streams` value to the address of a first node in linked list
	 * without any conditions in the loop body. `tail->next` would point to `streams` itself due to
	 * address of a structure being equal to an address of a first field. */
	streams = NULL;
	tail = (struct _sub_node *) &streams;
	
	do {
		GMimeStreamPrivate *source_priv = _g_mime_stream_get_private (n->stream);

		s = g_new (struct _sub_node, 1);
		s->next = NULL;
		s->stream = n->stream;
		tail->next = s;
		tail = s;
		
		s->start = source_priv->bound_start;

		if (n == priv->sources)
			s->start += start;
		else if (offset < start)
			s->start += (start - offset);
		
		d(fprintf (stderr, "added stream[%d] to our list\n", n->id));
		
		if (source_priv->bound_end == -1) {
			if ((len = g_mime_stream_length (n->stream)) == -1)
				goto error;
		} else {
			len = source_priv->bound_end - source_priv->bound_start;
		}
		
		d(fprintf (stderr, "stream[%d]: len = %ld, offset of beginning of stream is %ld\n",
			   n->id, len, offset));
		
		if (end != -1 && (end <= (offset + len))) {
			d(fprintf (stderr, "stream[%d]: requested end <= offset + len\n", n->id));
			s->end = source_priv->bound_start + (end - offset);
			d(fprintf (stderr, "stream[%d]: s->start = %ld, s->end = %ld; break\n",
				   n->id, s->start, s->end));
			subend += (end - offset);
			break;
		} else {
			s->end = source_priv->bound_start + len;
			d(fprintf (stderr, "stream[%d]: s->start = %ld, s->end = %ld\n",
				   n->id, s->start, s->end));
		}
		
		subend += (s->end - s->start);
		offset += len;
		
		n = n->next;
	} while (n != NULL);
	
	d(fprintf (stderr, "returning a substream containing multiple source streams\n"));

	cat = g_object_new (GMIME_TYPE_STREAM_CAT, NULL);
	/* Note: we could pass -1 as bound_end, it should Just
	 * Work(tm) but setting absolute bounds is kinda
	 * nice... */
	g_mime_stream_construct ((GMimeStream *) cat, 0, subend);
	
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
	
	stream = g_object_new (GMIME_TYPE_STREAM_CAT, NULL);
	g_mime_stream_construct (stream, 0, -1);
	
	return stream;
}


/**
 * g_mime_stream_cat_add_source:
 * @cat: a #GMimeStreamCat
 * @source: a source stream
 *
 * Adds a reference to @source to the @cat without seeking or resetting it.
 * Reading starts at the source's current position. Adding a source after
 * end-of-stream makes that source available to subsequent reads.
 *
 * Returns: %0 on success or %-1 on fail.
 **/
int
g_mime_stream_cat_add_source (GMimeStreamCat *cat, GMimeStream *source)
{
	GMimeStreamCatPrivate *priv;
	struct _cat_node *node, *n;
	
	g_return_val_if_fail (GMIME_IS_STREAM_CAT (cat), -1);
	g_return_val_if_fail (GMIME_IS_STREAM (source), -1);
	
	node = g_new (struct _cat_node, 1);
	node->next = NULL;
	node->stream = source;
	g_object_ref (source);
	node->position = 0;
	
	priv = _g_mime_stream_cat_get_private (cat);

	n = priv->sources;
	while (n && n->next)
		n = n->next;
	
	if (n == NULL) {
		priv->sources = node;
		node->id = 0;
	} else {
		node->id = n->id + 1;
		n->next = node;
	}
	
	if (!priv->current)
		priv->current = node;
	priv->read_eof = FALSE;
	
	return 0;
}
