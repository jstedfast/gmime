/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 *  Authors: Jeffrey Stedfast <fejj@ximian.com>
 *
 *  Copyright 2001 Ximain, Inc. (www.ximian.com)
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

#include "gmime-filter-from.h"
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
 * g_mime_filter_from_new:
 *
 * Creates a new GMimeFilterFrom filter.
 *
 * Returns a new from filter.
 **/
GMimeFilter *
g_mime_filter_from_new ()
{
	GMimeFilterFrom *new;
	
	new = g_new (GMimeFilterFrom, 1);
	
	new->midline = FALSE;
	
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
	return g_mime_filter_from_new ();
}

struct fromnode {
	struct fromnode *next;
	char *pointer;
};

static void
filter_filter (GMimeFilter *filter, char *in, size_t len, size_t prespace,
	       char **out, size_t *outlen, size_t *outprespace)
{
	GMimeFilterFrom *from = (GMimeFilterFrom *) filter;
	struct fromnode *head = NULL, *tail = (struct fromnode *) &head, *node;
	register char *inptr, *inend;
	unsigned int left;
	int fromcount = 0;
	char *outptr;
	
	inptr = in;
	inend = inptr + len;
	
	while (inptr < inend) {
		register int c = -1;
		
		if (from->midline) {
			while (inptr < inend && (c = *inptr++) != '\n')
				;
		}
		
		if (c == '\n' || !from->midline) {
			left = inend - inptr;
			if (left > 0) {
				from->midline = TRUE;
				if (left < 5) {
					if (*inptr == 'F') {
						g_mime_filter_backup (filter, inptr, left);
						from->midline = FALSE;
						inend = inptr;
						break;
					}
				} else {
					if (!strncmp (inptr, "From ", 5)) {
						fromcount++;
						
						node = alloca (sizeof (struct fromnode));
						node->pointer = inptr;
						node->next = NULL;
						tail->next = node;
						tail = node;
						
						inptr += 5;
					}
				}
			} else {
				from->midline = FALSE;
			}
		}
	}
	
	if (fromcount > 0) {
		g_mime_filter_set_size (filter, len + fromcount, FALSE);
		
		node = head;
		inptr = in;
		outptr = filter->outbuf;
		while (node) {
			memcpy (outptr, inptr, (unsigned) (node->pointer - inptr));
			outptr += node->pointer - inptr;
			*outptr++ = '>';
			inptr = node->pointer;
			node = node->next;
		}
		
		memcpy (outptr, inptr, (unsigned) (inend - inptr));
		outptr += inend - inptr;
		*out = filter->outbuf;
		*outlen = outptr - filter->outbuf;
		*outprespace = filter->outbuf - filter->outreal;
	} else {
		*out = in;
		*outlen = inend - in;
		*outprespace = prespace;
	}
}

static void 
filter_complete (GMimeFilter *filter, char *in, size_t len, size_t prespace,
		 char **out, size_t *outlen, size_t *outprespace)
{
	filter_filter (filter, in, len, prespace, out, outlen, outprespace);
}

static void
filter_reset (GMimeFilter *filter)
{
	GMimeFilterFrom *from = (GMimeFilterFrom *) filter;
	
	from->midline = FALSE;
}
