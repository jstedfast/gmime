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

#include <stdio.h>

#include "gmime-filter-strip.h"
#include "strlib.h"

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
 * g_mime_filter_strip_new:
 *
 * Creates a new GMimeFilterStrip filter.
 *
 * Returns a new strip filter.
 **/
GMimeFilter *
g_mime_filter_strip_new ()
{
	GMimeFilterStrip *new;
	
	new = g_new (GMimeFilterStrip, 1);
	
	g_mime_filter_construct (GMIME_FILTER (new), &filter_template);
	
	return GMIME_FILTER (new);
}


static void
filter_destroy (GMimeFilter *filter)
{
	g_free (filter);
}

static GMimeFilter *
filter_copy (GMimeFilter *filter)
{
	return g_mime_filter_strip_new ();
}

static void
filter_filter (GMimeFilter *filter, char *in, size_t len, size_t prespace,
	       char **out, size_t *outlen, size_t *outprespace)
{
	register unsigned char *inptr, *last;
	unsigned char *inend, *start;
	char *outptr;
	
	g_mime_filter_set_size (filter, len, FALSE);
	
	last = inptr = (unsigned char *) in;
	inend = (unsigned char *) in + len;
	
	outptr = filter->outbuf;
	
	while (inptr < inend) {
		start = inptr;
		while (inptr < inend && *inptr != '\n') {
			if (*inptr != ' ' && *inptr != '\t')
				last = inptr + 1;
			inptr++;
		}
		
		memcpy (outptr, start, last - start);
		outptr += (last - start);
		if (inptr < inend) {
			/* write the newline */
			*outptr++ = (char) *inptr++;
			last = inptr;
		}
	}
	
	g_mime_filter_backup (filter, last, inptr - last);
	
	*out = filter->outbuf;
	*outlen = outptr - filter->outbuf;
	*outprespace = filter->outpre;
}

static void 
filter_complete (GMimeFilter *filter, char *in, size_t len, size_t prespace,
		 char **out, size_t *outlen, size_t *outprespace)
{
	if (len)
		filter_filter (filter, in, len, prespace, out, outlen, outprespace);
}

static void
filter_reset (GMimeFilter *filter)
{
	/* no-op */
}
