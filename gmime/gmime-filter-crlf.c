/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 *  Authors: Jeffrey Stedfast <fejj@ximian.com>
 *
 *  Copyright 2001-2002 Ximian, Inc. (www.ximian.com)
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

#include "gmime-filter-crlf.h"

static void g_mime_filter_crlf_class_init (GMimeFilterCRLFClass *klass);
static void g_mime_filter_crlf_init (GMimeFilterCRLF *filter, GMimeFilterCRLFClass *klass);
static void g_mime_filter_crlf_finalize (GObject *object);

static GMimeFilter *filter_copy (GMimeFilter *filter);
static void filter_filter (GMimeFilter *filter, char *in, size_t len, size_t prespace,
			   char **out, size_t *outlen, size_t *outprespace);
static void filter_complete (GMimeFilter *filter, char *in, size_t len, size_t prespace,
			     char **out, size_t *outlen, size_t *outprespace);
static void filter_reset (GMimeFilter *filter);


static GMimeFilterClass *parent_class = NULL;


GType
g_mime_filter_crlf_get_type (void)
{
	static GType type = 0;
	
	if (!type) {
		static const GTypeInfo info = {
			sizeof (GMimeFilterCRLFClass),
			NULL, /* base_class_init */
			NULL, /* base_class_finalize */
			(GClassInitFunc) g_mime_filter_crlf_class_init,
			NULL, /* class_finalize */
			NULL, /* class_data */
			sizeof (GMimeFilterCRLF),
			0,    /* n_preallocs */
			(GInstanceInitFunc) g_mime_filter_crlf_init,
		};
		
		type = g_type_register_static (GMIME_TYPE_FILTER, "GMimeFilterCRLF", &info, 0);
	}
	
	return type;
}


static void
g_mime_filter_crlf_class_init (GMimeFilterCRLFClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	GMimeFilterClass *filter_class = GMIME_FILTER_CLASS (klass);
	
	parent_class = g_type_class_ref (GMIME_TYPE_FILTER);
	
	object_class->finalize = g_mime_filter_crlf_finalize;
	
	filter_class->copy = filter_copy;
	filter_class->filter = filter_filter;
	filter_class->complete = filter_complete;
	filter_class->reset = filter_reset;
}

static void
g_mime_filter_crlf_init (GMimeFilterCRLF *filter, GMimeFilterCRLFClass *klass)
{
	filter->saw_cr = FALSE;
	filter->saw_lf = FALSE;
	filter->saw_dot = FALSE;
}

static void
g_mime_filter_crlf_finalize (GObject *object)
{
	G_OBJECT_CLASS (parent_class)->finalize (object);
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
	register const char *inptr;
	const char *inend;
	gboolean do_dots;
	char *outptr;
	
	do_dots = crlf->mode == GMIME_FILTER_CRLF_MODE_CRLF_DOTS;
	
	inptr = in;
	inend = in + len;
	
	if (crlf->direction == GMIME_FILTER_CRLF_ENCODE) {
		g_mime_filter_set_size (filter, 3 * len, FALSE);
		
		outptr = filter->outbuf;
		while (inptr < inend) {
			if (*inptr == '\r') {
				crlf->saw_cr = TRUE;
			} else if (*inptr == '\n') {
				crlf->saw_lf = TRUE;
				if (!crlf->saw_cr)
					*outptr++ = '\r';
				crlf->saw_cr = FALSE;
			} else {
				if (do_dots && *inptr == '.' && crlf->saw_lf)
					*outptr++ = '.';
				
				crlf->saw_cr = FALSE;
				crlf->saw_lf = FALSE;
			}
			
			*outptr++ = *inptr++;
		}
	} else {
		g_mime_filter_set_size (filter, len, FALSE);
		
		outptr = filter->outbuf;
		while (inptr < inend) {
			if (*inptr == '\r') {
				crlf->saw_cr = TRUE;
			} else {
				if (crlf->saw_cr) {
					crlf->saw_cr = FALSE;
					
					if (*inptr == '\n') {
						crlf->saw_lf = TRUE;
						*outptr++ = *inptr++;
						continue;
					} else
						*outptr++ = '\r';
				}
				
				*outptr++ = *inptr;
			}
			
			if (do_dots && *inptr == '.') {
				if (crlf->saw_lf) {
					crlf->saw_dot = TRUE;
					crlf->saw_lf = FALSE;
					inptr++;
				} else if (crlf->saw_dot) {
					crlf->saw_dot = FALSE;
				}
			}
			
			crlf->saw_lf = FALSE;
			
			inptr++;
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


/**
 * g_mime_filter_crlf_new:
 * @direction: encode direction
 * @mode: crlf or crlf & dot mode
 *
 * Creates a new GMimeFilterCRLF filter.
 *
 * Returns a new crlf(/dot) filter.
 **/
GMimeFilter *
g_mime_filter_crlf_new (GMimeFilterCRLFDirection direction, GMimeFilterCRLFMode mode)
{
	GMimeFilterCRLF *new;
	
	new = g_object_new (GMIME_TYPE_FILTER_CRLF, NULL, NULL);
	new->direction = direction;
	new->mode = mode;
	
	return (GMimeFilter *) new;
}
