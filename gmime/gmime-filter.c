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

#include <string.h> /* for memcpy */

#include "gmime-filter.h"


/**
 * SECTION: gmime-filter
 * @title: GMimeFilter
 * @short_description: Abstract filter class
 * @see_also: #GMimeStreamFilter
 *
 * Stream filters are an efficient way of converting data from one
 * format to another.
 **/


struct _GMimeFilterPrivate {
	char *inbuf;
	size_t inlen;
};

#define PRE_HEAD (64)
#define BACK_HEAD (64)
#define _PRIVATE(o) (((GMimeFilter *)(o))->priv)

static void g_mime_filter_class_init (GMimeFilterClass *klass);
static void g_mime_filter_init (GMimeFilter *filter, GMimeFilterClass *klass);
static void g_mime_filter_finalize (GObject *object);

static GMimeFilter *filter_copy (GMimeFilter *filter);
static void filter_filter (GMimeFilter *filter, char *inbuf, size_t inlen, size_t prespace,
			   char **outbuf, size_t *outlen, size_t *outprespace);
static void filter_complete (GMimeFilter *filter, char *inbuf, size_t inlen, size_t prespace,
			     char **outbuf, size_t *outlen, size_t *outprespace);
static void filter_reset (GMimeFilter *filter);


static GObjectClass *parent_class = NULL;


GType
g_mime_filter_get_type (void)
{
	static GType type = 0;
	
	if (!type) {
		static const GTypeInfo info = {
			sizeof (GMimeFilterClass),
			NULL, /* base_class_init */
			NULL, /* base_class_finalize */
			(GClassInitFunc) g_mime_filter_class_init,
			NULL, /* class_finalize */
			NULL, /* class_data */
			sizeof (GMimeFilter),
			0,    /* n_preallocs */
			(GInstanceInitFunc) g_mime_filter_init,
		};
		
		type = g_type_register_static (G_TYPE_OBJECT, "GMimeFilter",
					       &info, G_TYPE_FLAG_ABSTRACT);
	}
	
	return type;
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
g_mime_filter_init (GMimeFilter *filter, GMimeFilterClass *klass)
{
	filter->priv = g_new0 (struct _GMimeFilterPrivate, 1);
	filter->outptr = NULL;
	filter->outreal = NULL;
	filter->outbuf = NULL;
	filter->outsize = 0;
	
	filter->backbuf = NULL;
	filter->backsize = 0;
	filter->backlen = 0;
}

static void
g_mime_filter_finalize (GObject *object)
{
	GMimeFilter *filter = (GMimeFilter *) object;
	
	g_free (filter->priv->inbuf);
	g_free (filter->priv);
	g_free (filter->outreal);
	g_free (filter->backbuf);
	
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
 * Copies @filter into a new GMimeFilter object.
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
	if (prespace < filter->backlen) {
		struct _GMimeFilterPrivate *p = _PRIVATE (filter);
		size_t newlen = inlen + prespace + filter->backlen;
		
		if (p->inlen < newlen) {
			/* NOTE: g_realloc copies data, we dont need that (slower) */
			g_free (p->inbuf);
			p->inbuf = g_malloc (newlen + PRE_HEAD);
			p->inlen = newlen + PRE_HEAD;
		}
		
		/* copy to end of structure */
		memcpy (p->inbuf + p->inlen - inlen, inbuf, inlen);
		inbuf = p->inbuf + p->inlen - inlen;
		prespace = p->inlen - inlen;
	}
	
	/* preload any backed up data */
	if (filter->backlen > 0) {
		memcpy (inbuf - filter->backlen, filter->backbuf, filter->backlen);
		inbuf -= filter->backlen;
		inlen += filter->backlen;
		prespace -= filter->backlen;
		filter->backlen = 0;
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
	g_return_if_fail (GMIME_IS_FILTER (filter));
	
	GMIME_FILTER_GET_CLASS (filter)->reset (filter);
	
	/* could free some buffers, if they are really big? */
	filter->backlen = 0;
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
	g_return_if_fail (GMIME_IS_FILTER (filter));
	
	if (filter->backsize < length) {
		/* g_realloc copies data, unnecessary overhead */
		g_free (filter->backbuf);
		filter->backbuf = g_malloc (length + BACK_HEAD);
		filter->backsize = length + BACK_HEAD;
	}
	
	filter->backlen = length;
	memcpy (filter->backbuf, data, length);
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
	g_return_if_fail (GMIME_IS_FILTER (filter));
	
	if (filter->outsize < size) {
		size_t offset = filter->outptr - filter->outreal;
		
		if (keep) {
			filter->outreal = g_realloc (filter->outreal, size + PRE_HEAD * 4);
		} else {
			g_free (filter->outreal);
			filter->outreal = g_malloc (size + PRE_HEAD * 4);
		}
		
		filter->outptr = filter->outreal + offset;
		filter->outbuf = filter->outreal + PRE_HEAD * 4;
		filter->outsize = size;
		
		/* this could be offset from the end of the structure, but 
		   this should be good enough */
		
		filter->outpre = PRE_HEAD * 4;
	}
}
