/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 *  Authors: Jeffrey Stedfast <fejj@ximian.com>
 *
 *  Copyright 2001 Ximian, Inc. (www.ximian.com)
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

#ifdef HAVE_ALLOCA_H
#include <alloca.h>
#endif

#include <stdio.h>

#include "gmime-filter-chomp.h"
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
 * g_mime_filter_chomp_new:
 *
 * Creates a new GMimeFilterChomp filter.
 *
 * Returns a new chomp filter.
 **/
GMimeFilter *
g_mime_filter_chomp_new ()
{
	GMimeFilterChomp *new;
	
	new = g_new (GMimeFilterChomp, 1);
	
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
	return g_mime_filter_chomp_new ();
}

static void
filter_filter (GMimeFilter *filter, char *in, size_t len, size_t prespace,
	       char **out, size_t *outlen, size_t *outprespace)
{
	register unsigned char *inptr, *s;
	const unsigned char *inend;
	register char *outptr;
	
	g_mime_filter_set_size (filter, len + prespace, FALSE);
	
	inend = in + len;
	inptr = in;
	
	outptr = filter->outbuf;
	
	while (inptr < inend) {
		s = inptr;
		while (inptr < inend && isspace ((int) *inptr))
			inptr++;
		
		if (inptr < inend) {
			while (s < inptr)
				*outptr++ = (char) *s++;
			
			while (inptr < inend && !isspace ((int) *inptr))
				*outptr++ = (char) *inptr++;
		} else {
#if 0
			if ((inend - s) >= 2 && *s == '\r' && *(s + 1) == '\n')
				*outptr++ = *s++;
			if (*s == '\n')
				*outptr++ = *s++;
#endif		
			if (s < inend)
				g_mime_filter_backup (filter, s, inend - s);
			break;
		}
	}
	
	*out = filter->outbuf;
	*outlen = outptr - filter->outbuf;
	*outprespace = filter->outpre;
}

static void 
filter_complete (GMimeFilter *filter, char *in, size_t len, size_t prespace,
		 char **out, size_t *outlen, size_t *outprespace)
{
	unsigned char *inptr;
	char *outptr;
	
	if (len)
		filter_filter (filter, in, len, prespace, out, outlen, outprespace);
	
	if (filter->backlen) {
		inptr = (unsigned char *) filter->backbuf;
		outptr = filter->outbuf + *outlen;
		
		/* in the case of a canonical eoln */
		if (*inptr == '\r' && *(inptr + 1) == '\n') {
			*outptr++ = (char) *inptr++;
			(*outlen)++;
		}
		
		if (*inptr == '\n') {
			*outptr++ = (char) *inptr++;
			(*outlen)++;
		}
		
		/* to protect against further complete calls */
		g_mime_filter_backup (filter, "", 0);
	}
}

static void
filter_reset (GMimeFilter *filter)
{
	/* no-op */
}
