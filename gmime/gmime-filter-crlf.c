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

#include "gmime-filter-crlf.h"


/**
 * SECTION: gmime-filter-crlf
 * @title: GMimeFilterCRLF
 * @short_description: Convert line-endings from LF to CRLF or vise versa
 *
 * A #GMimeFilter for converting between DOS and UNIX line-endings.
 **/


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
	
	return g_mime_filter_crlf_new (crlf->encode, crlf->dots);
}

static void
filter_filter (GMimeFilter *filter, char *inbuf, size_t inlen, size_t prespace,
	       char **outbuf, size_t *outlen, size_t *outprespace)
{
	GMimeFilterCRLF *crlf = (GMimeFilterCRLF *) filter;
	register const char *inptr = inbuf;
	const char *inend = inbuf + inlen;
	char *outptr;
	
	if (crlf->encode) {
		g_mime_filter_set_size (filter, 3 * inlen, FALSE);
		
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
				if (crlf->dots && *inptr == '.' && crlf->saw_lf)
					*outptr++ = '.';
				
				crlf->saw_cr = FALSE;
				crlf->saw_lf = FALSE;
			}
			
			*outptr++ = *inptr++;
		}
	} else {
		g_mime_filter_set_size (filter, inlen + 1, FALSE);
		
		outptr = filter->outbuf;
		while (inptr < inend) {
			if (*inptr == '\r') {
				crlf->saw_dot = FALSE;
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
				
				if (!(crlf->dots && crlf->saw_dot && *inptr == '.'))
					*outptr++ = *inptr;
			}
			
			if (crlf->dots && *inptr == '.') {
				if (crlf->saw_lf) {
					crlf->saw_dot = TRUE;
				} else if (crlf->saw_dot) {
					crlf->saw_dot = FALSE;
				}
			}
			
			crlf->saw_lf = FALSE;
			
			inptr++;
		}
	}
	
	*outlen = outptr - filter->outbuf;
	*outprespace = filter->outpre;
	*outbuf = filter->outbuf;
}

static void 
filter_complete (GMimeFilter *filter, char *inbuf, size_t inlen, size_t prespace,
		 char **outbuf, size_t *outlen, size_t *outprespace)
{
	if (inbuf && inlen)
		filter_filter (filter, inbuf, inlen, prespace, outbuf, outlen, outprespace);
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
 * @encode: %TRUE if the filter should encode or %FALSE otherwise
 * @dots: encode/decode dots (as for SMTP)
 *
 * Creates a new #GMimeFilterCRLF filter.
 *
 * If @encode is %TRUE, then lone line-feeds ('\n') will be 'encoded'
 * into the canonical CRLF end-of-line sequence ("\r\n") otherwise
 * CRLF sequences will be 'decoded' into the UNIX line-ending form
 * ('\n').
 *
 * The @dots parameter tells the filter whether or not it should
 * encode or decode lines beginning with a dot ('.'). If both @encode
 * and @dots are %TRUE, then a '.' at the beginning of a line will be
 * 'encoded' into "..". If @encode is %FALSE, then ".." at the
 * beginning of a line will be decoded into a single '.'.
 *
 * Returns: a new #GMimeFilterCRLF filter.
 **/
GMimeFilter *
g_mime_filter_crlf_new (gboolean encode, gboolean dots)
{
	GMimeFilterCRLF *new;
	
	new = g_object_newv (GMIME_TYPE_FILTER_CRLF, 0, NULL);
	new->encode = encode;
	new->dots = dots;
	
	return (GMimeFilter *) new;
}
