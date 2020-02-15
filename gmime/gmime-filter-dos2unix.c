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

#include "gmime-filter-dos2unix.h"


/**
 * SECTION: gmime-filter-dos2unix
 * @title: GMimeFilterDos2Unix
 * @short_description: Convert line-endings from Windows/DOS (CRLF) to UNIX (LF).
 *
 * A #GMimeFilter for converting from DOS to UNIX line-endings.
 **/


static void g_mime_filter_dos2unix_class_init (GMimeFilterDos2UnixClass *klass);
static void g_mime_filter_dos2unix_init (GMimeFilterDos2Unix *filter, GMimeFilterDos2UnixClass *klass);

static GMimeFilter *filter_copy (GMimeFilter *filter);
static void filter_filter (GMimeFilter *filter, char *in, size_t len, size_t prespace,
			   char **out, size_t *outlen, size_t *outprespace);
static void filter_complete (GMimeFilter *filter, char *in, size_t len, size_t prespace,
			     char **out, size_t *outlen, size_t *outprespace);
static void filter_reset (GMimeFilter *filter);


static GMimeFilterClass *parent_class = NULL;


GType
g_mime_filter_dos2unix_get_type (void)
{
	static GType type = 0;
	
	if (!type) {
		static const GTypeInfo info = {
			sizeof (GMimeFilterDos2UnixClass),
			NULL, /* base_class_init */
			NULL, /* base_class_finalize */
			(GClassInitFunc) g_mime_filter_dos2unix_class_init,
			NULL, /* class_finalize */
			NULL, /* class_data */
			sizeof (GMimeFilterDos2Unix),
			0,    /* n_preallocs */
			(GInstanceInitFunc) g_mime_filter_dos2unix_init,
		};
		
		type = g_type_register_static (GMIME_TYPE_FILTER, "GMimeFilterDos2Unix", &info, 0);
	}
	
	return type;
}


static void
g_mime_filter_dos2unix_class_init (GMimeFilterDos2UnixClass *klass)
{
	GMimeFilterClass *filter_class = GMIME_FILTER_CLASS (klass);
	
	parent_class = g_type_class_ref (GMIME_TYPE_FILTER);
	
	filter_class->copy = filter_copy;
	filter_class->filter = filter_filter;
	filter_class->complete = filter_complete;
	filter_class->reset = filter_reset;
}

static void
g_mime_filter_dos2unix_init (GMimeFilterDos2Unix *filter, GMimeFilterDos2UnixClass *klass)
{
	filter->ensure_newline = FALSE;
	filter->pc = '\0';
}


static GMimeFilter *
filter_copy (GMimeFilter *filter)
{
	GMimeFilterDos2Unix *dos2unix = (GMimeFilterDos2Unix *) filter;
	
	return g_mime_filter_dos2unix_new (dos2unix->ensure_newline);
}

static void
convert (GMimeFilter *filter, char *inbuf, size_t inlen, size_t prespace,
	 char **outbuf, size_t *outlen, size_t *outprespace, gboolean flush)
{
	GMimeFilterDos2Unix *dos2unix = (GMimeFilterDos2Unix *) filter;
	register const char *inptr = inbuf;
	const char *inend = inbuf + inlen;
	size_t expected = inlen;
	char *outptr;
	
	if (flush && dos2unix->ensure_newline)
		expected++;
	
	if (dos2unix->pc == '\r')
		expected++;
	
	g_mime_filter_set_size (filter, expected, FALSE);
	
	outptr = filter->outbuf;
	while (inptr < inend) {
		if (*inptr == '\n') {
			*outptr++ = *inptr;
		} else {
			if (dos2unix->pc == '\r')
				*outptr++ = dos2unix->pc;
			
			if (*inptr != '\r')
				*outptr++ = *inptr;
		}
		
		dos2unix->pc = *inptr++;
	}
	
	if (flush && dos2unix->ensure_newline && dos2unix->pc != '\n')
		dos2unix->pc = *outptr++ = '\n';
	
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
	GMimeFilterDos2Unix *dos2unix = (GMimeFilterDos2Unix *) filter;
	
	dos2unix->pc = '\0';
}


/**
 * g_mime_filter_dos2unix_new:
 * @ensure_newline: %TRUE if the filter should ensure that the stream ends in a new line
 *
 * Creates a new #GMimeFilterDos2Unix filter.
 *
 * Returns: a new #GMimeFilterDos2Unix filter.
 **/
GMimeFilter *
g_mime_filter_dos2unix_new (gboolean ensure_newline)
{
	GMimeFilterDos2Unix *dos2unix;
	
	dos2unix = g_object_new (GMIME_TYPE_FILTER_DOS2UNIX, NULL);
	dos2unix->ensure_newline = ensure_newline;
	
	return (GMimeFilter *) dos2unix;
}
