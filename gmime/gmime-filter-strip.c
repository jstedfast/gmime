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

#include <stdio.h>
#include <string.h>

#include "gmime-filter-strip.h"


/**
 * SECTION: gmime-filter-strip
 * @title: GMimeFilterStrip
 * @short_description: Strip trailing whitespace from the end of lines
 * @see_also: #GMimeFilter
 *
 * A #GMimeFilter used for stripping trailing whitespace from the end
 * of lines.
 **/


static void g_mime_filter_strip_class_init (GMimeFilterStripClass *klass);
static void g_mime_filter_strip_init (GMimeFilterStrip *filter, GMimeFilterStripClass *klass);
static void g_mime_filter_strip_finalize (GObject *object);

static GMimeFilter *filter_copy (GMimeFilter *filter);
static void filter_filter (GMimeFilter *filter, char *in, size_t len, size_t prespace,
			   char **out, size_t *outlen, size_t *outprespace);
static void filter_complete (GMimeFilter *filter, char *in, size_t len, size_t prespace,
			     char **out, size_t *outlen, size_t *outprespace);
static void filter_reset (GMimeFilter *filter);


static GMimeFilterClass *parent_class = NULL;


GType
g_mime_filter_strip_get_type (void)
{
	static GType type = 0;
	
	if (!type) {
		static const GTypeInfo info = {
			sizeof (GMimeFilterStripClass),
			NULL, /* base_class_init */
			NULL, /* base_class_finalize */
			(GClassInitFunc) g_mime_filter_strip_class_init,
			NULL, /* class_finalize */
			NULL, /* class_data */
			sizeof (GMimeFilterStrip),
			0,    /* n_preallocs */
			(GInstanceInitFunc) g_mime_filter_strip_init,
		};
		
		type = g_type_register_static (GMIME_TYPE_FILTER, "GMimeFilterStrip", &info, 0);
	}
	
	return type;
}


static void
g_mime_filter_strip_class_init (GMimeFilterStripClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	GMimeFilterClass *filter_class = GMIME_FILTER_CLASS (klass);
	
	parent_class = g_type_class_ref (GMIME_TYPE_FILTER);
	
	object_class->finalize = g_mime_filter_strip_finalize;
	
	filter_class->copy = filter_copy;
	filter_class->filter = filter_filter;
	filter_class->complete = filter_complete;
	filter_class->reset = filter_reset;
}

static void
g_mime_filter_strip_init (GMimeFilterStrip *filter, GMimeFilterStripClass *klass)
{
	/* no-op */
}

static void
g_mime_filter_strip_finalize (GObject *object)
{
	G_OBJECT_CLASS (parent_class)->finalize (object);
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
	
	g_mime_filter_backup (filter, (char *) last, inptr - last);
	
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


/**
 * g_mime_filter_strip_new:
 *
 * Creates a new #GMimeFilterStrip filter which will strip trailing
 * whitespace from every line of input passed through the filter.
 *
 * Returns: a new strip filter.
 **/
GMimeFilter *
g_mime_filter_strip_new (void)
{
	return g_object_newv (GMIME_TYPE_FILTER_STRIP, 0, NULL);
}
