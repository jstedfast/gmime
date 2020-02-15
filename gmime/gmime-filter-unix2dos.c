/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*  GMime
 *  Copyright (C) 2000-2020 Jeffrey Stedfast
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

#include "gmime-filter-unix2dos.h"


/**
 * SECTION: gmime-filter-unix2dos
 * @title: GMimeFilterUnix2Dos
 * @short_description: Convert line-endings from UNIX (LF) to Windows/DOS (CRLF).
 *
 * A #GMimeFilter for converting from UNIX to DOS line-endings.
 **/


static void g_mime_filter_unix2dos_class_init (GMimeFilterUnix2DosClass *klass);
static void g_mime_filter_unix2dos_init (GMimeFilterUnix2Dos *filter, GMimeFilterUnix2DosClass *klass);

static GMimeFilter *filter_copy (GMimeFilter *filter);
static void filter_filter (GMimeFilter *filter, char *in, size_t len, size_t prespace,
			   char **out, size_t *outlen, size_t *outprespace);
static void filter_complete (GMimeFilter *filter, char *in, size_t len, size_t prespace,
			     char **out, size_t *outlen, size_t *outprespace);
static void filter_reset (GMimeFilter *filter);


static GMimeFilterClass *parent_class = NULL;


GType
g_mime_filter_unix2dos_get_type (void)
{
	static GType type = 0;
	
	if (!type) {
		static const GTypeInfo info = {
			sizeof (GMimeFilterUnix2DosClass),
			NULL, /* base_class_init */
			NULL, /* base_class_finalize */
			(GClassInitFunc) g_mime_filter_unix2dos_class_init,
			NULL, /* class_finalize */
			NULL, /* class_data */
			sizeof (GMimeFilterUnix2Dos),
			0,    /* n_preallocs */
			(GInstanceInitFunc) g_mime_filter_unix2dos_init,
		};
		
		type = g_type_register_static (GMIME_TYPE_FILTER, "GMimeFilterUnix2Dos", &info, 0);
	}
	
	return type;
}


static void
g_mime_filter_unix2dos_class_init (GMimeFilterUnix2DosClass *klass)
{
	GMimeFilterClass *filter_class = GMIME_FILTER_CLASS (klass);
	
	parent_class = g_type_class_ref (GMIME_TYPE_FILTER);
	
	filter_class->copy = filter_copy;
	filter_class->filter = filter_filter;
	filter_class->complete = filter_complete;
	filter_class->reset = filter_reset;
}

static void
g_mime_filter_unix2dos_init (GMimeFilterUnix2Dos *filter, GMimeFilterUnix2DosClass *klass)
{
	filter->ensure_newline = FALSE;
	filter->pc = '\0';
}


static GMimeFilter *
filter_copy (GMimeFilter *filter)
{
	GMimeFilterUnix2Dos *unix2dos = (GMimeFilterUnix2Dos *) filter;
	
	return g_mime_filter_unix2dos_new (unix2dos->ensure_newline);
}

static void
convert (GMimeFilter *filter, char *inbuf, size_t inlen, size_t prespace,
	 char **outbuf, size_t *outlen, size_t *outprespace, gboolean flush)
{
	GMimeFilterUnix2Dos *unix2dos = (GMimeFilterUnix2Dos *) filter;
	register const char *inptr = inbuf;
	const char *inend = inbuf + inlen;
	size_t expected = inlen * 2;
	char *outptr;
	
	if (flush && unix2dos->ensure_newline)
		expected += 2;
	
	g_mime_filter_set_size (filter, expected, FALSE);
	
	outptr = filter->outbuf;
	while (inptr < inend) {
		if (*inptr == '\r') {
			*outptr++ = *inptr;
		} else if (*inptr == '\n') {
			if (unix2dos->pc != '\r')
				*outptr++ = '\r';
			*outptr++ = *inptr;
		} else {
			*outptr++ = *inptr;
		}
		
		unix2dos->pc = *inptr++;
	}
	
	if (flush && unix2dos->ensure_newline && unix2dos->pc != '\n') {
		if (unix2dos->pc != '\r')
			*outptr++ = '\r';
		*outptr++ = '\n';
	}
	
	*outlen = outptr - filter->outbuf;
	*outprespace = filter->outpre;
	*outbuf = filter->outbuf;
}

static void
filter_filter (GMimeFilter *filter, char *inbuf, size_t inlen, size_t prespace,
	       char **outbuf, size_t *outlen, size_t *outprespace)
{
	convert (filter, inbuf, inlen, prespace, outbuf, outlen, outprespace, FALSE);
}

static void 
filter_complete (GMimeFilter *filter, char *inbuf, size_t inlen, size_t prespace,
		 char **outbuf, size_t *outlen, size_t *outprespace)
{
	convert (filter, inbuf, inlen, prespace, outbuf, outlen, outprespace, TRUE);
}

static void
filter_reset (GMimeFilter *filter)
{
	GMimeFilterUnix2Dos *unix2dos = (GMimeFilterUnix2Dos *) filter;
	
	unix2dos->pc = '\0';
}


/**
 * g_mime_filter_unix2dos_new:
 * @ensure_newline: %TRUE if the filter should ensure that the stream ends in a new line
 *
 * Creates a new #GMimeFilterUnix2Dos filter.
 *
 * Returns: a new #GMimeFilterUnix2Dos filter.
 **/
GMimeFilter *
g_mime_filter_unix2dos_new (gboolean ensure_newline)
{
	GMimeFilterUnix2Dos *unix2dos;
	
	unix2dos = g_object_new (GMIME_TYPE_FILTER_UNIX2DOS, NULL);
	unix2dos->ensure_newline = ensure_newline;
	
	return (GMimeFilter *) unix2dos;
}
