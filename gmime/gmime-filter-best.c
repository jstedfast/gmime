/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 *  Authors: Jeffrey Stedfast <fejj@ximian.com>
 *
 *  Copyright 2002 Ximian, Inc. (www.ximian.com)
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

#include <string.h>

#include "gmime-filter-best.h"

static void filter_destroy (GMimeFilter *filter);
static GMimeFilter *filter_copy (GMimeFilter *filter);
static void filter_filter (GMimeFilter *filter, char *in, size_t len, 
			   size_t prespace, char **out, 
			   size_t *outlen, size_t *outprespace);
static void filter_complete (GMimeFilter *filter, char *in, size_t len, 
			     size_t prespace, char **out, 
			     size_t *outlen, size_t *outprespace);
static void filter_reset (GMimeFilter *filter);

static GMimeFilter filter_template = {
	NULL, NULL, NULL, NULL,
	0, 0, NULL, 0, 0,
	filter_destroy,
	filter_copy,
	filter_filter,
	filter_complete,
	filter_reset,
};


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
 * Note: In order for the #g_mime_filter_best_charset() function to
 * work, the stream being filtered MUST already be encoded in UTF-8.
 *
 * Returns a new best filter with flags @flags.
 **/
GMimeFilter *
g_mime_filter_best_new (unsigned int flags)
{
	GMimeFilterBest *new;
	
	new = g_new (GMimeFilterBest, 1);
	new->flags = flags;
	
	g_mime_filter_construct (GMIME_FILTER (new), &filter_template);
	
	filter_reset (GMIME_FILTER (new));
	new->frombuf[5] = '\0';
	
	return GMIME_FILTER (new);
}


/**
 * g_mime_filter_best_charset:
 * @best: best filter
 *
 * Calculates the best charset for encoding the stream filtered
 * through the @best filter.
 *
 * Returns a pointer to a string containing the name of the charset
 * best suited for the text filtered through @best.
 **/
const char *
g_mime_filter_best_charset (GMimeFilterBest *best)
{
	g_return_val_if_fail (best != NULL, NULL);
	
	if (!(best->flags & GMIME_FILTER_BEST_CHARSET))
		return NULL;
	
	return g_mime_charset_best_name (&best->charset);
}


/**
 * g_mime_filter_best_encoding:
 * @best: best filter
 * @required: encoding that all data must fit within
 *
 * Calculates the best Content-Transfer-Encoding for the stream
 * filtered through @best that fits within the @required encoding.
 *
 * Returns the best encoding for the stream filtered by @best.
 **/
GMimePartEncodingType
g_mime_filter_best_encoding (GMimeFilterBest *best, GMimeBestEncoding required)
{
	GMimePartEncodingType encoding = GMIME_PART_ENCODING_DEFAULT;
	
	g_return_val_if_fail (best != NULL, GMIME_PART_ENCODING_DEFAULT);
	
	if (!(best->flags & GMIME_FILTER_BEST_ENCODING))
		return GMIME_PART_ENCODING_DEFAULT;
	
	switch (required) {
	case GMIME_BEST_ENCODING_7BIT:
		if (best->count0 > 0) {
			encoding = GMIME_PART_ENCODING_BASE64;
		} else if (best->count8 > 0) {
			if (best->count8 >= (best->total * 17 / 100))
				encoding = GMIME_PART_ENCODING_BASE64;
			else
				encoding = GMIME_PART_ENCODING_QUOTEDPRINTABLE;
		} else if (best->maxline > 998) {
			encoding = GMIME_PART_ENCODING_QUOTEDPRINTABLE;
		}
		break;
	case GMIME_BEST_ENCODING_8BIT:
		if (best->count0 > 0) {
			encoding = GMIME_PART_ENCODING_BASE64;
		} else if (best->maxline > 998) {
			encoding = GMIME_PART_ENCODING_QUOTEDPRINTABLE;
		}
		break;
	case GMIME_BEST_ENCODING_BINARY:
		if (best->count0 + best->count8 > 0)
			encoding = GMIME_PART_ENCODING_BINARY;
		break;
	}
	
	if (encoding == GMIME_PART_ENCODING_DEFAULT && best->hadfrom)
		encoding = GMIME_PART_ENCODING_QUOTEDPRINTABLE;
	
	return encoding;
}
   

static void
filter_destroy (GMimeFilter *filter)
{
	g_free (filter);
}

static GMimeFilter *
filter_copy (GMimeFilter *filter)
{
	GMimeFilterBest *best = (GMimeFilterBest *) filter;
	
	return g_mime_filter_best_new (best->flags);
}

static void
filter_filter (GMimeFilter *filter, char *in, size_t len, size_t prespace,
	       char **out, size_t *outlen, size_t *outprespace)
{
	GMimeFilterBest *best = (GMimeFilterBest *) filter;
	register char *inptr, *inend;
	unsigned int left;
	
	if (best->flags & GMIME_FILTER_BEST_CHARSET)
		g_mime_charset_step (&best->charset, in, len);
	
	if (best->flags & GMIME_FILTER_BEST_ENCODING) {
		best->total += len;
		
		inptr = in;
		inend = inptr + len;
		
		while (inptr < inend) {
			register int c = -1;
			
			if (best->midline) {
				while (inptr < inend && (c = *inptr++) != '\n') {
					if (c == 0)
						best->count0++;
					else if (c > 127)
						best->count8++;
					
					if (best->fromlen > 0 && best->fromlen < 5)
						best->frombuf[best->fromlen++] = c;
					
					best->linelen++;
				}
			}
			
			/* check our from-save buffer for "From " */
			if (best->fromlen == 5 && !strcmp (best->frombuf, "From "))
				best->hadfrom = TRUE;
			
			best->fromlen = 0;
			
			if (c == '\n' || !best->midline) {
				/* we're at the beginning of a line */
				best->midline = FALSE;
				
				/* update the max line length */
				best->maxline = MAX (best->maxline, best->linelen);
				
				/* if we have not yet found a from-line, check for one */
				if (!best->hadfrom) {
					left = inend - inptr;
					if (left > 0) {
						best->midline = TRUE;
						if (left < 5) {
							if (!strncmp (inptr, "From ", left)) {
								memcpy (best->frombuf, inptr, left);
								best->frombuf[left] = '\0';
								best->fromlen = left;
								break;
							}
						} else {
							if (!strncmp (inptr, "From ", 5)) {
								best->hadfrom = TRUE;
								
								inptr += 5;
							}
						}
					}
				}
			}
		}
	}
	
	*out = in;
	*outlen = len;
	*outprespace = prespace;
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
	best->midline = FALSE;
	best->hadfrom = FALSE;
}
