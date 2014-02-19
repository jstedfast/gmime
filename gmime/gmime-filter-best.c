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

#include "gmime-filter-best.h"


/**
 * SECTION: gmime-filter-best
 * @title: GMimeFilterBest
 * @short_description: Determine the best charset/encoding to use for a stream
 * @see_also: #GMimeFilter
 *
 * A #GMimeFilter which is meant to determine the best charset and/or
 * transfer encoding suitable for the stream which is filtered through
 * it.
 **/


static void g_mime_filter_best_class_init (GMimeFilterBestClass *klass);
static void g_mime_filter_best_init (GMimeFilterBest *filter, GMimeFilterBestClass *klass);
static void g_mime_filter_best_finalize (GObject *object);

static GMimeFilter *filter_copy (GMimeFilter *filter);
static void filter_filter (GMimeFilter *filter, char *in, size_t len, size_t prespace,
			   char **out, size_t *outlen, size_t *outprespace);
static void filter_complete (GMimeFilter *filter, char *in, size_t len, size_t prespace,
			     char **out, size_t *outlen, size_t *outprespace);
static void filter_reset (GMimeFilter *filter);


static GMimeFilterClass *parent_class = NULL;


GType
g_mime_filter_best_get_type (void)
{
	static GType type = 0;
	
	if (!type) {
		static const GTypeInfo info = {
			sizeof (GMimeFilterBestClass),
			NULL, /* base_class_init */
			NULL, /* base_class_finalize */
			(GClassInitFunc) g_mime_filter_best_class_init,
			NULL, /* class_finalize */
			NULL, /* class_data */
			sizeof (GMimeFilterBest),
			0,    /* n_preallocs */
			(GInstanceInitFunc) g_mime_filter_best_init,
		};
		
		type = g_type_register_static (GMIME_TYPE_FILTER, "GMimeFilterBest", &info, 0);
	}
	
	return type;
}


static void
g_mime_filter_best_class_init (GMimeFilterBestClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	GMimeFilterClass *filter_class = GMIME_FILTER_CLASS (klass);
	
	parent_class = g_type_class_ref (GMIME_TYPE_FILTER);
	
	object_class->finalize = g_mime_filter_best_finalize;
	
	filter_class->copy = filter_copy;
	filter_class->filter = filter_filter;
	filter_class->complete = filter_complete;
	filter_class->reset = filter_reset;
}

static void
g_mime_filter_best_init (GMimeFilterBest *filter, GMimeFilterBestClass *klass)
{
	filter->frombuf[5] = '\0';
}

static void
g_mime_filter_best_finalize (GObject *object)
{
	G_OBJECT_CLASS (parent_class)->finalize (object);
}


static GMimeFilter *
filter_copy (GMimeFilter *filter)
{
	GMimeFilterBest *best = (GMimeFilterBest *) filter;
	
	return g_mime_filter_best_new (best->flags);
}

static void
filter_filter (GMimeFilter *filter, char *inbuf, size_t inlen, size_t prespace,
	       char **outbuf, size_t *outlen, size_t *outprespace)
{
	GMimeFilterBest *best = (GMimeFilterBest *) filter;
	register unsigned char *inptr, *inend;
	register unsigned char c;
	size_t left;
	
	if (best->flags & GMIME_FILTER_BEST_CHARSET)
		g_mime_charset_step (&best->charset, inbuf, inlen);
	
	if (best->flags & GMIME_FILTER_BEST_ENCODING) {
		best->total += inlen;
		
		inptr = (unsigned char *) inbuf;
		inend = inptr + inlen;
		
		while (inptr < inend) {
			c = 0;
			
			if (best->midline) {
				while (inptr < inend && (c = *inptr++) != '\n') {
					if (c == 0)
						best->count0++;
					else if (c & 0x80)
						best->count8++;
					
					if (best->fromlen > 0 && best->fromlen < 5)
						best->frombuf[best->fromlen++] = c & 0xff;
					
					best->linelen++;
				}
				
				if (c == '\n') {
					best->maxline = MAX (best->maxline, best->linelen);
					best->startline = TRUE;
					best->midline = FALSE;
					best->linelen = 0;
				}
			}
			
			/* check our from-save buffer for "From " */
			if (best->fromlen == 5 && !strcmp ((char *) best->frombuf, "From "))
				best->hadfrom = TRUE;
			
			best->fromlen = 0;
			
			left = inend - inptr;
			
			/* if we have not yet found a from-line, check for one */
			if (best->startline && !best->hadfrom && left > 0) {
				if (left < 5) {
					if (!strncmp ((char *) inptr, "From ", left)) {
						memcpy (best->frombuf, inptr, left);
						best->frombuf[left] = '\0';
						best->fromlen = left;
						break;
					}
				} else {
					if (!strncmp ((char *) inptr, "From ", 5)) {
						best->hadfrom = TRUE;
						inptr += 5;
					}
				}
			}
			
			best->startline = FALSE;
			best->midline = TRUE;
		}
	}
	
	*outprespace = prespace;
	*outlen = inlen;
	*outbuf = inbuf;
}

static void 
filter_complete (GMimeFilter *filter, char *in, size_t len, size_t prespace,
		 char **out, size_t *outlen, size_t *outprespace)
{
	GMimeFilterBest *best = (GMimeFilterBest *) filter;
	
	filter_filter (filter, in, len, prespace, out, outlen, outprespace);
	
	best->maxline = MAX (best->maxline, best->linelen);
}

static void
filter_reset (GMimeFilter *filter)
{
	GMimeFilterBest *best = (GMimeFilterBest *) filter;
	
	g_mime_charset_init (&best->charset);
	best->count0 = 0;
	best->count8 = 0;
	best->total = 0;
	best->maxline = 0;
	best->linelen = 0;
	best->fromlen = 0;
	best->hadfrom = FALSE;
	best->startline = TRUE;
	best->midline = FALSE;
}


/**
 * g_mime_filter_best_new:
 * @flags: filter flags
 *
 * Creates a new GMimeFilterBest filter. @flags are used to determine
 * which information to keep statistics of. If the
 * #GMIME_FILTER_BEST_CHARSET bit is set, the filter will be able to
 * compute the best charset for encoding the stream of data
 * filtered. If the #GMIME_FILTER_BEST_ENCODING bit is set, the filter
 * will be able to compute the best Content-Transfer-Encoding for use
 * with the stream being filtered.
 *
 * Note: In order for the g_mime_filter_best_charset() function to
 * work, the stream being filtered MUST already be encoded in UTF-8.
 *
 * Returns: a new best filter with flags @flags.
 **/
GMimeFilter *
g_mime_filter_best_new (GMimeFilterBestFlags flags)
{
	GMimeFilterBest *new;
	
	new = g_object_newv (GMIME_TYPE_FILTER_BEST, 0, NULL);
	new->flags = flags;
	filter_reset ((GMimeFilter *) new);
	
	return (GMimeFilter *) new;
}


/**
 * g_mime_filter_best_charset:
 * @best: best filter
 *
 * Calculates the best charset for encoding the stream filtered
 * through the @best filter.
 *
 * Returns: a pointer to a string containing the name of the charset
 * best suited for the text filtered through @best.
 **/
const char *
g_mime_filter_best_charset (GMimeFilterBest *best)
{
	const char *charset;
	
	g_return_val_if_fail (GMIME_IS_FILTER_BEST (best), NULL);
	
	if (!(best->flags & GMIME_FILTER_BEST_CHARSET))
		return NULL;
	
	charset = g_mime_charset_best_name (&best->charset);
	
	return charset ? charset : "us-ascii";
}


/**
 * g_mime_filter_best_encoding:
 * @best: a #GMimeFilterBest
 * @constraint: a #GMimeEncodingConstraint
 *
 * Calculates the most efficient Content-Transfer-Encoding for the
 * stream filtered through @best that fits within the encoding
 * @constraint.
 *
 * Returns: the best encoding for the stream filtered by @best.
 **/
GMimeContentEncoding
g_mime_filter_best_encoding (GMimeFilterBest *best, GMimeEncodingConstraint constraint)
{
	GMimeContentEncoding encoding = GMIME_CONTENT_ENCODING_DEFAULT;
	
	g_return_val_if_fail (GMIME_IS_FILTER_BEST (best), GMIME_CONTENT_ENCODING_DEFAULT);
	
	if (!(best->flags & GMIME_FILTER_BEST_ENCODING))
		return GMIME_CONTENT_ENCODING_DEFAULT;
	
	switch (constraint) {
	case GMIME_ENCODING_CONSTRAINT_7BIT:
		if (best->count0 > 0) {
			encoding = GMIME_CONTENT_ENCODING_BASE64;
		} else if (best->count8 > 0) {
			if (best->count8 >= (unsigned int) (best->total * (17.0 / 100.0)))
				encoding = GMIME_CONTENT_ENCODING_BASE64;
			else
				encoding = GMIME_CONTENT_ENCODING_QUOTEDPRINTABLE;
		} else if (best->maxline > 998) {
			encoding = GMIME_CONTENT_ENCODING_QUOTEDPRINTABLE;
		}
		break;
	case GMIME_ENCODING_CONSTRAINT_8BIT:
		if (best->count0 > 0) {
			encoding = GMIME_CONTENT_ENCODING_BASE64;
		} else if (best->maxline > 998) {
			encoding = GMIME_CONTENT_ENCODING_QUOTEDPRINTABLE;
		}
		break;
	case GMIME_ENCODING_CONSTRAINT_BINARY:
		if (best->count0 + best->count8 > 0)
			encoding = GMIME_CONTENT_ENCODING_BINARY;
		break;
	}
	
	if (encoding == GMIME_CONTENT_ENCODING_DEFAULT && best->hadfrom)
		encoding = GMIME_CONTENT_ENCODING_QUOTEDPRINTABLE;
	
	return encoding;
}
