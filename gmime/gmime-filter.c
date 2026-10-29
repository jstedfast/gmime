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

#include <string.h> /* for memcpy */

#include "gmime-filter.h"


#include "gmime-object-private.h"

/**
 * SECTION: gmime-filter
 * @title: GMimeFilter
 * @short_description: Abstract filter class
 * @see_also: #GMimeStreamFilter
 *
 * Stream filters are an efficient way of converting data from one
 * format to another.
 **/

#define PRE_HEAD (64)
#define BACK_HEAD (64)

static void g_mime_filter_class_init (GMimeFilterClass *klass);
static void g_mime_filter_init (GMimeFilter *filter);
static void g_mime_filter_finalize (GObject *object);

static GMimeFilter *filter_copy (GMimeFilter *filter);
static void filter_filter (GMimeFilter *filter, char *inbuf, size_t inlen, size_t prespace,
			   char **outbuf, size_t *outlen, size_t *outprespace);
static void filter_complete (GMimeFilter *filter, char *inbuf, size_t inlen, size_t prespace,
			     char **outbuf, size_t *outlen, size_t *outprespace);
static void filter_reset (GMimeFilter *filter);


static GObjectClass *parent_class = NULL;



G_DEFINE_ABSTRACT_TYPE_WITH_PRIVATE (GMimeFilter, g_mime_filter, G_TYPE_OBJECT)

GMimeFilterPrivate *
_g_mime_filter_get_private (GMimeFilter *object)
{
	return g_mime_filter_get_instance_private (object);
}


static void
g_mime_filter_class_init (GMimeFilterClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	
	parent_class = g_type_class_ref (G_TYPE_OBJECT);
	
	object_class->finalize = g_mime_filter_finalize;
	
	klass->copy = filter_copy;
	klass->filter = filter_filter;
	klass->complete = filter_complete;
	klass->reset = filter_reset;
}

static void
g_mime_filter_init (GMimeFilter *filter)
{
	GMimeFilterPrivate *priv = _g_mime_filter_get_private (filter);

	priv->inbuf = NULL;
	priv->inlen = 0;
	priv->outptr = NULL;
	priv->outreal = NULL;
	priv->outbuf = NULL;
	priv->outsize = 0;
	
	priv->backbuf = NULL;
	priv->backsize = 0;
	priv->backlen = 0;
}

static void
g_mime_filter_finalize (GObject *object)
{
	GMimeFilter *filter = (GMimeFilter *) object;
	GMimeFilterPrivate *priv = _g_mime_filter_get_private (filter);

	g_free (priv->inbuf);
	g_free (priv->outreal);
	g_free (priv->backbuf);
	
	G_OBJECT_CLASS (parent_class)->finalize (object);
}


static GMimeFilter *
filter_copy (GMimeFilter *filter)
{
	return NULL;
}


/**
 * g_mime_filter_copy:
 * @filter: filter
 *
 * Copies @filter into a new #GMimeFilter object.
 *
 * Returns: (transfer full): a duplicate of @filter.
 **/
GMimeFilter *
g_mime_filter_copy (GMimeFilter *filter)
{
	g_return_val_if_fail (GMIME_IS_FILTER (filter), NULL);
	
	return GMIME_FILTER_GET_CLASS (filter)->copy (filter);
}


static void
filter_run (GMimeFilter *filter, char *inbuf, size_t inlen, size_t prespace,
		char **outbuf, size_t *outlen, size_t *outprespace,
		void (*filterfunc) (GMimeFilter *filter,
		char *inbuf, size_t inlen, size_t prespace,
		char **outbuf, size_t *outlen, size_t *outprespace))
{
	/* here we take a performance hit, if the input buffer doesn't
	   have the pre-space required.  We make a buffer that does... */
	GMimeFilterPrivate *priv = _g_mime_filter_get_private (filter);

	if (prespace < priv->backlen) {
		size_t newlen = inlen + prespace + priv->backlen;
		
		if (priv->inlen < newlen) {
			/* NOTE: g_realloc copies data, we don't need that (slower) */
			g_free (priv->inbuf);
			priv->inbuf = g_malloc (newlen + PRE_HEAD);
			priv->inlen = newlen + PRE_HEAD;
		}
		
		/* copy to end of structure */
		memcpy (priv->inbuf + priv->inlen - inlen, inbuf, inlen);
		inbuf = priv->inbuf + priv->inlen - inlen;
		prespace = priv->inlen - inlen;
	}
	
	/* preload any backed up data */
	if (priv->backlen > 0) {
		memcpy (inbuf - priv->backlen, priv->backbuf, priv->backlen);
		inbuf -= priv->backlen;
		inlen += priv->backlen;
		prespace -= priv->backlen;
		priv->backlen = 0;
	}
	
	filterfunc (filter, inbuf, inlen, prespace, outbuf, outlen, outprespace);
}


static void
filter_filter (GMimeFilter *filter, char *inbuf, size_t inlen, size_t prespace,
	       char **outbuf, size_t *outlen, size_t *outprespace)
{
	/* no-op */
}


/**
 * g_mime_filter_filter:
 * @filter: filter
 * @inbuf: (array length=inlen) (element-type guint8): input buffer
 * @inlen: input buffer length
 * @prespace: prespace buffer length
 * @outbuf: (out) (array length=outlen) (element-type guint8) (transfer none):
 *   pointer to output buffer
 * @outlen: (out): pointer to output length
 * @outprespace: (out): pointer to output prespace buffer length
 *
 * Filters the input data and writes it to @out.
 **/
void
g_mime_filter_filter (GMimeFilter *filter, char *inbuf, size_t inlen, size_t prespace,
		      char **outbuf, size_t *outlen, size_t *outprespace)
{
	g_return_if_fail (GMIME_IS_FILTER (filter));
	
	filter_run (filter, inbuf, inlen, prespace, outbuf, outlen, outprespace,
		    GMIME_FILTER_GET_CLASS (filter)->filter);
}


static void
filter_complete (GMimeFilter *filter, char *inbuf, size_t inlen, size_t prespace,
		 char **outbuf, size_t *outlen, size_t *outprespace)
{
	/* no-op */
}


/**
 * g_mime_filter_complete:
 * @filter: filter
 * @inbuf: (array length=inlen) (element-type guint8): input buffer
 * @inlen: input buffer length
 * @prespace: prespace buffer length
 * @outbuf: (out) (array length=outlen) (element-type guint8) (transfer none):
 *   pointer to output buffer
 * @outlen: (out): pointer to output length
 * @outprespace: (out): pointer to output prespace buffer length
 *
 * Completes the filtering.
 **/
void
g_mime_filter_complete (GMimeFilter *filter,
			char *inbuf, size_t inlen, size_t prespace,
			char **outbuf, size_t *outlen, size_t *outprespace)
{
	g_return_if_fail (GMIME_IS_FILTER (filter));
	
	filter_run (filter, inbuf, inlen, prespace, outbuf, outlen, outprespace,
		    GMIME_FILTER_GET_CLASS (filter)->complete);
}


static void
filter_reset (GMimeFilter *filter)
{
	/* no-op */
}


/**
 * g_mime_filter_reset:
 * @filter: a #GMimeFilter object
 *
 * Resets the filter.
 **/
void
g_mime_filter_reset (GMimeFilter *filter)
{
	GMimeFilterPrivate *priv;

	g_return_if_fail (GMIME_IS_FILTER (filter));
	
	GMIME_FILTER_GET_CLASS (filter)->reset (filter);
	
	/* could free some buffers, if they are really big? */
	priv = _g_mime_filter_get_private (filter);

	priv->backlen = 0;
}


/**
 * g_mime_filter_backup:
 * @filter: filter
 * @data: (array length=length) (element-type guint8): data to backup
 * @length: length of @data
 *
 * Sets number of bytes backed up on the input, new calls replace
 * previous ones
 **/
void
g_mime_filter_backup (GMimeFilter *filter, const char *data, size_t length)
{
	GMimeFilterPrivate *priv;

	g_return_if_fail (GMIME_IS_FILTER (filter));
	
	priv = _g_mime_filter_get_private (filter);

	if (!priv->backbuf || priv->backsize < length) {
		/* g_realloc copies data, unnecessary overhead */
		g_free (priv->backbuf);
		priv->backbuf = g_malloc (length + BACK_HEAD);
		priv->backsize = length + BACK_HEAD;
	}
	
	priv->backlen = length;
	memcpy (priv->backbuf, data, length);
}


/**
 * g_mime_filter_set_size:
 * @filter: filter
 * @size: requested size for the output buffer
 * @keep: %TRUE if existing data in the output buffer should be kept
 *
 * Ensure this much size is available for filter output (if required)
 **/
void
g_mime_filter_set_size (GMimeFilter *filter, size_t size, gboolean keep)
{
	GMimeFilterPrivate *priv;

	g_return_if_fail (GMIME_IS_FILTER (filter));

	if (G_UNLIKELY (size > G_MAXSIZE - PRE_HEAD * 4))
		g_error ("GMimeFilter output buffer size overflow");
	
	priv = _g_mime_filter_get_private (filter);

	if (!priv->outreal || priv->outsize < size) {
		size_t offset = priv->outptr - priv->outreal;
		
		if (keep) {
			priv->outreal = g_realloc (priv->outreal, size + PRE_HEAD * 4);
		} else {
			g_free (priv->outreal);
			priv->outreal = g_malloc (size + PRE_HEAD * 4);
		}
		
		priv->outptr = priv->outreal + offset;
		priv->outbuf = priv->outreal + PRE_HEAD * 4;
		priv->outsize = size;
		
		/* this could be offset from the end of the structure, but 
		   this should be good enough */
		priv->outpre = PRE_HEAD * 4;
	}
}

/**
 * g_mime_filter_get_output_buffer:
 * @filter: a #GMimeFilter
 * @size: (out): writable capacity, excluding prespace
 * @prespace: (out) (optional): writable bytes preceding the returned buffer
 *
 * Gets the output buffer for use by a filter implementation. Allocate or
 * grow it with g_mime_filter_set_size(). The pointer is borrowed and may be
 * invalidated by resizing or destroying @filter. Subclasses should track
 * their own output cursor.
 *
 * Returns: (array length=size) (element-type guint8) (transfer none) (nullable):
 * the writable output buffer, or %NULL if it has not been allocated.
 **/
char *
g_mime_filter_get_output_buffer (GMimeFilter *filter, size_t *size, size_t *prespace)
{
	GMimeFilterPrivate *priv;

	g_return_val_if_fail (GMIME_IS_FILTER (filter), NULL);
	g_return_val_if_fail (size != NULL, NULL);

	priv = _g_mime_filter_get_private (filter);

	*size = priv->outsize;

	if (prespace)
		*prespace = priv->outpre;

	return priv->outbuf;
}
