/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 *  Authors: Jeffrey Stedfast <fejj@ximian.com>
 *
 *  Copyright 2002-2004 Ximian, Inc. (www.ximian.com)
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

#include "gmime-stream-cat.h"

static void g_mime_stream_cat_class_init (GMimeStreamCatClass *klass);
static void g_mime_stream_cat_init (GMimeStreamCat *stream, GMimeStreamCatClass *klass);
static void g_mime_stream_cat_finalize (GObject *object);

static ssize_t stream_read (GMimeStream *stream, char *buf, size_t len);
static ssize_t stream_write (GMimeStream *stream, const char *buf, size_t len);
static int stream_flush (GMimeStream *stream);
static int stream_close (GMimeStream *stream);
static gboolean stream_eos (GMimeStream *stream);
static int stream_reset (GMimeStream *stream);
static off_t stream_seek (GMimeStream *stream, off_t offset, GMimeSeekWhence whence);
static off_t stream_tell (GMimeStream *stream);
static ssize_t stream_length (GMimeStream *stream);
static GMimeStream *stream_substream (GMimeStream *stream, off_t start, off_t end);


static GMimeStreamClass *parent_class = NULL;


struct _cat_node {
	struct _cat_node *next;
	GMimeStream *stream;
	ssize_t length;
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
	struct _cat_node *p, *n;
	
	p = cat->sources;
	while (p) {
		n = p->next;
		g_object_unref (p->stream);
		g_free (p);
		p = n;
	}
	
	G_OBJECT_CLASS (parent_class)->finalize (object);
}


static ssize_t
stream_read (GMimeStream *stream, char *buf, size_t len)
{
	GMimeStreamCat *cat = (GMimeStreamCat *) stream;
	struct _cat_node *current;
	ssize_t n, nread = 0;
	
	/* check for end-of-stream */
	if (stream->bound_end != -1 && stream->position >= stream->bound_end)
		return -1;
	
	/* don't allow our caller to read past the end of the stream */
	if (stream->bound_end != -1)
		len = MIN (stream->bound_end - stream->position, (off_t) len);
	
	/* make sure our stream position is where it should be */
	if (stream_seek (stream, stream->position, GMIME_STREAM_SEEK_SET) == -1)
		return -1;
	
	if (!(current = cat->current))
		return -1;
	
	do {
		n = 0;
		while (!g_mime_stream_eos (current->stream) && nread < len) {
			n = g_mime_stream_read (current->stream, buf + nread, len - nread);
			if (n > 0)
				nread += n;
		}
		
		if (nread < len) {
			current = current->next;
			if (current) {
				g_mime_stream_reset (current->stream);
			} else {
				if (n == -1 && nread == 0)
					return -1;
				
				break;
			}
		}
	} while (nread < len);
	
	stream->position += nread;
	
	cat->current = current;
	
	return nread;
}
 
static ssize_t
stream_write (GMimeStream *stream, const char *buf, size_t len)
{
	GMimeStreamCat *cat = (GMimeStreamCat *) stream;
	struct _cat_node *current;
	ssize_t n, nwritten = 0;
	
	/* check for end-of-stream */
	if (stream->bound_end != -1 && stream->position >= stream->bound_end)
		return -1;
	
	/* don't allow our caller to write past the end of the stream */
	if (stream->bound_end != -1)
		len = MIN (stream->bound_end - stream->position, (off_t) len);
	
	/* make sure our stream position is where it should be */
	if (stream_seek (stream, stream->position, GMIME_STREAM_SEEK_SET) == -1)
		return -1;
	
	if (!(current = cat->current))
		return -1;
	
	do {
		n = -1;
		while (!g_mime_stream_eos (current->stream) && nwritten < len) {
			n = g_mime_stream_write (current->stream, buf + nwritten, len - nwritten);
			if (n > 0)
				nwritten += n;
		}
		
		if (nwritten < len) {
			current = current->next;
			if (current) {
				g_mime_stream_reset (current->stream);
			} else {
				if (n == -1 && nwritten == 0)
					return -1;
				
				break;
			}
		}
	} while (nwritten < len);
	
	stream->position += nwritten;
	
	cat->current = current;
	
	return nwritten;
}

static int
stream_flush (GMimeStream *stream)
{
	GMimeStreamCat *cat = (GMimeStreamCat *) stream;
	
	if (cat->current)
		return g_mime_stream_flush (cat->current->stream);
	else
		return 0;
}

static int
stream_close (GMimeStream *stream)
{
	GMimeStreamCat *cat = (GMimeStreamCat *) stream;
	struct _cat_node *p;
	
	cat->current = NULL;
	p = cat->sources;
	while (p) {
		if (p->stream) {
			if (g_mime_stream_close (p->stream) == -1)
				return -1;
		}
		p->stream = NULL;
		p = p->next;
	}
	
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
	int ret;
	
	if (stream->position == stream->bound_start)
		return 0;
	
	ret = stream_seek (stream, stream->bound_start, GMIME_STREAM_SEEK_SET);
	
	return ret == -1 ? -1 : 0;
}

static off_t
stream_seek (GMimeStream *stream, off_t offset, GMimeSeekWhence whence)
{
	GMimeStreamCat *cat = (GMimeStreamCat *) stream;
	struct _cat_node *current, *p;
	off_t real, ret;
	
	if (cat->sources == NULL)
		return -1;
	
	switch (whence) {
	case GMIME_STREAM_SEEK_SET:
	seek_set:
		/* we can't seek to a position less than bound_start */
		if (offset < stream->bound_start)
			return -1;
		
		real = offset;
		current = NULL;
		p = cat->sources;
		while (real > 0 && p) {
			real -= p->length;
			current = p;
			p = p->next;
		}
		
		if (p == NULL && real > 0) {
			/* bound_end < offset */
			return -1;
		}
		
		/* reset all the streams after this point */
		while (p) {
			/* FIXME: return -1 if a reset fails? */
			g_mime_stream_reset (p->stream);
			p = p->next;
		}
		
		if (!current)
			current = cat->sources;
		else
			real += current->length;
		
		real += current->stream->bound_start;
		ret = g_mime_stream_seek (current->stream, real, GMIME_STREAM_SEEK_SET);
		if (ret != -1) {
			stream->position = offset;
			cat->current = current;
			return stream->position;
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
		real = 0;
		p = cat->sources;
		while (p) {
			real += p->length;
			p = p->next;
		}
		
		/* calculate offset relative to the beginning of the stream */
		offset = real + offset;
		goto seek_set;
		break;
	default:
		g_assert_not_reached ();
		break;
	}
	
	return -1;
}

static off_t
stream_tell (GMimeStream *stream)
{
	return stream->position;
}

static ssize_t
stream_length (GMimeStream *stream)
{
	GMimeStreamCat *cat = GMIME_STREAM_CAT (stream);
	struct _cat_node *p;
	ssize_t len = 0;
	
	if (stream->bound_end != -1)
		return stream->bound_end - stream->bound_start;
	
	p = cat->sources;
	while (p) {
		len += p->length;
		p = p->next;
	}
	
	return len;
}

static GMimeStream *
stream_substream (GMimeStream *stream, off_t start, off_t end)
{
	GMimeStreamCat *cat;
	struct _cat_node *p;
	
	cat = g_object_new (GMIME_TYPE_STREAM_CAT, NULL, NULL);
	
	g_mime_stream_construct (GMIME_STREAM (cat), start, end);
	
	p = GMIME_STREAM_CAT (stream)->sources;
	while (p) {
		g_mime_stream_cat_add_source (cat, p->stream);
		p = p->next;
	}
	
	return GMIME_STREAM (cat);
}


/**
 * g_mime_stream_cat_new:
 *
 * Creates a new GMimeStreamCat object.
 *
 * Returns a new cat stream.
 **/
GMimeStream *
g_mime_stream_cat_new (void)
{
	GMimeStreamCat *cat;
	
	cat = g_object_new (GMIME_TYPE_STREAM_CAT, NULL, NULL);
	
	g_mime_stream_construct (GMIME_STREAM (cat), 0, -1);
	
	return GMIME_STREAM (cat);
}


/**
 * g_mime_stream_cat_add_source:
 * @cat: cat stream
 * @source: a source stream
 *
 * Adds the @source stream to the cat stream @cat.
 *
 * Returns 0 on success or -1 on fail.
 **/
int
g_mime_stream_cat_add_source (GMimeStreamCat *cat, GMimeStream *source)
{
	struct _cat_node *p, *node;
	ssize_t len;
	
	g_return_val_if_fail (GMIME_IS_STREAM_CAT (cat), -1);
	g_return_val_if_fail (GMIME_IS_STREAM (source), -1);
	
	len = g_mime_stream_length (source);
	if (len == -1)
		return -1;
	
	node = g_new (struct _cat_node, 1);
	node->next = NULL;
	node->stream = source;
	node->length = len;
	g_object_ref (source);
	
	p = cat->sources;
	while (p && p->next)
		p = p->next;
	
	if (!p)
		cat->sources = node;
	else
		p->next = node;
	
	if (!cat->current)
		cat->current = node;
	
	return 0;
}
