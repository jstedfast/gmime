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


#include "gmime-filter-crlf.h"

static void filter_destroy (GMimeFilter *filter);
static GMimeFilter *filter_copy (GMimeFilter *filter);
static void filter_filter (GMimeFilter *filter, char *in, size_t len, 
			   size_t prespace, char **out, 
			   size_t *outlen, size_t *outprespace);
static void filter_complete (GMimeFilter *filter, char *in, size_t len, 
			     size_t prespace, char **out, 
			     size_t *outlen, size_t *outprespace);
static void filter_reset (GMimeFilter *filter);

static GMimeFilter template = {
	NULL, NULL, NULL, NULL,
	0, 0, NULL, 0, 0,
	filter_destroy,
	filter_copy,
	filter_filter,
	filter_complete,
	filter_reset,
};


/**
 * g_mime_filter_crlf_new_type:
 * @direction:
 * @mode:
 *
 * Returns a new crlf(/dot) filter.
 **/
GMimeFilter *
g_mime_filter_crlf_new (GMimeFilterCRLFDirection direction, GMimeFilterCRLFMode mode)
{
	GMimeFilterCRLF *new;
	
	new = g_new (GMimeFilterCRLF, 1);
	
	new->direction = direction;
	new->mode = mode;
	new->saw_cr = FALSE;
	new->saw_lf = FALSE;
	new->saw_dot = FALSE;
	
	g_mime_filter_construct (GMIME_FILTER (new), &template);
	
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
	GMimeFilterCRLF *crlf = (GMimeFilterCRLF *) filter;
	
	return g_mime_filter_crlf_new (crlf->direction, crlf->mode);
}

static void
filter_filter (GMimeFilter *filter, char *in, size_t len, size_t prespace,
	       char **out, size_t *outlen, size_t *outprespace)
{
	GMimeFilterCRLF *crlf = (GMimeFilterCRLF *) filter;
	gboolean do_dots;
	char *p, *q;
	
	do_dots = crlf->mode == GMIME_FILTER_CRLF_MODE_CRLF_DOTS;
	
	if (crlf->direction == GMIME_FILTER_CRLF_ENCODE) {
		g_mime_filter_set_size (filter, 3 * len, FALSE);
		
		p = in;
		q = filter->outbuf;
		while (p < in + len) {
			if (*p == '\n') {
				crlf->saw_lf = TRUE;
				*q++ = '\r';
			} else {
				if (do_dots && *p == '.' && crlf->saw_lf)
					*q++ = '.';
				
				crlf->saw_lf = FALSE;
			}
			
			*q++ = *p++;
		}
	} else {
		g_mime_filter_set_size (filter, len, FALSE);
		
		p = in;
		q = filter->outbuf;
		while (p < in + len) {
			if (*p == '\r') {
				crlf->saw_cr = TRUE;
			} else {
				if (crlf->saw_cr) {
					crlf->saw_cr = FALSE;
					
					if (*p == '\n') {
						crlf->saw_lf = TRUE;
						*q++ = *p++;
						continue;
					} else
						*q++ = '\r';
				}
				
				*q++ = *p;
			}
			
			if (do_dots && *p == '.') {
				if (crlf->saw_lf) {
					crlf->saw_dot = TRUE;
					crlf->saw_lf = FALSE;
					p++;
				} else if (crlf->saw_dot) {
					crlf->saw_dot = FALSE;
				}
			}
			
			crlf->saw_lf = FALSE;
			
			p++;
		}
	}
	
	*out = filter->outbuf;
	*outlen = q - filter->outbuf;
	*outprespace = filter->outpre;
}

static void 
filter_complete (GMimeFilter *filter, char *in, size_t len, size_t prespace,
		 char **out, size_t *outlen, size_t *outprespace)
{
	if (in && len)
		filter_filter (filter, in, len, prespace, out, outlen, outprespace);
}

static void
filter_reset (GMimeFilter *filter)
{
	GMimeFilterCRLF *crlf = (GMimeFilterCRLF *) filter;
	
	crlf->saw_cr = FALSE;
	crlf->saw_lf = TRUE;
	crlf->saw_dot = FALSE;
}
