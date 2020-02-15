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

#include <stdio.h>
#include <string.h>

#include "gmime-filter-strip.h"
#include "gmime-table-private.h"
#include "packed.h"


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
	filter->lwsp = packed_byte_array_new ();
}

static void
g_mime_filter_strip_finalize (GObject *object)
{
	GMimeFilterStrip *filter = (GMimeFilterStrip *) object;
	
	packed_byte_array_free (filter->lwsp);
	
	G_OBJECT_CLASS (parent_class)->finalize (object);
}


static GMimeFilter *
filter_copy (GMimeFilter *filter)
{
	return g_mime_filter_strip_new ();
}

static void
convert (GMimeFilter *filter, char *in, size_t len, size_t prespace,
	 char **out, size_t *outlen, size_t *outprespace, gboolean flush)
{
	GMimeFilterStrip *strip = (GMimeFilterStrip *) filter;
	PackedByteArray *lwsp = (PackedByteArray *) strip->lwsp;
	register char *inptr, *outptr;
	char *inend, *outbuf;
	
	if (len == 0) {
		if (flush)
			packed_byte_array_clear (lwsp);
		
		*outprespace = prespace;
		*outlen = len;
		*out = in;
		
		return;
	}
	
	g_mime_filter_set_size (filter, len + lwsp->len, FALSE);
	outptr = outbuf = filter->outbuf;
	inend = in + len;
	inptr = in;
	
	if (flush)
		packed_byte_array_clear (strip->lwsp);
	
	while (inptr < inend) {
		if (is_blank (*inptr)) {
			packed_byte_array_add (lwsp, *inptr);
		} else if (*inptr == '\r') {
			packed_byte_array_clear (lwsp);
			*outptr++ = *inptr;
		} else if (*inptr == '\n') {
			packed_byte_array_clear (lwsp);
			*outptr++ = *inptr;
		} else {
			if (lwsp->len > 0) {
				packed_byte_array_copy_to (lwsp, outptr);
				outptr += lwsp->len;
				packed_byte_array_clear (lwsp);
			}
			
			*outptr++ = *inptr;
		}
		
		inptr++;
	}
	
	if (flush)
		packed_byte_array_clear (lwsp);
	
	*outprespace = filter->outpre;
	*outlen = (outptr - filter->outbuf);
	*out = filter->outbuf;
}

static void
filter_filter (GMimeFilter *filter, char *in, size_t len, size_t prespace,
	       char **out, size_t *outlen, size_t *outprespace)
{
	convert (filter, in, len, prespace, out, outlen, outprespace, FALSE);
}

static void 
filter_complete (GMimeFilter *filter, char *in, size_t len, size_t prespace,
		 char **out, size_t *outlen, size_t *outprespace)
{
	convert (filter, in, len, prespace, out, outlen, outprespace, TRUE);
}

static void
filter_reset (GMimeFilter *filter)
{
	GMimeFilterStrip *strip = (GMimeFilterStrip *) filter;
	
	packed_byte_array_clear (strip->lwsp);
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
	return g_object_new (GMIME_TYPE_FILTER_STRIP, NULL);
}
