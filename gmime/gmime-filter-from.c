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

#include <string.h>

#include "gmime-filter-from.h"


/**
 * SECTION: gmime-filter-from
 * @title: GMimeFilterFrom
 * @short_description: Escape MBox From_ lines
 * @see_also: #GMimeFilter
 *
 * A #GMimeFilter used for escaping MBox From_ lines using either the
 * traditional ">From " or quoted-printable encoding.
 **/


static void g_mime_filter_from_class_init (GMimeFilterFromClass *klass);
static void g_mime_filter_from_init (GMimeFilterFrom *filter, GMimeFilterFromClass *klass);
static void g_mime_filter_from_finalize (GObject *object);

static GMimeFilter *filter_copy (GMimeFilter *filter);
static void filter_filter (GMimeFilter *filter, char *in, size_t len, 
			   size_t prespace, char **out, 
			   size_t *outlen, size_t *outprespace);
static void filter_complete (GMimeFilter *filter, char *in, size_t len, 
			     size_t prespace, char **out, 
			     size_t *outlen, size_t *outprespace);
static void filter_reset (GMimeFilter *filter);


static GMimeFilterClass *parent_class = NULL;


GType
g_mime_filter_from_get_type (void)
{
	static GType type = 0;
	
	if (!type) {
		static const GTypeInfo info = {
			sizeof (GMimeFilterFromClass),
			NULL, /* base_class_init */
			NULL, /* base_class_finalize */
			(GClassInitFunc) g_mime_filter_from_class_init,
			NULL, /* class_finalize */
			NULL, /* class_data */
			sizeof (GMimeFilterFrom),
			0,    /* n_preallocs */
			(GInstanceInitFunc) g_mime_filter_from_init,
		};
		
		type = g_type_register_static (GMIME_TYPE_FILTER, "GMimeFilterFrom", &info, 0);
	}
	
	return type;
}


static void
g_mime_filter_from_class_init (GMimeFilterFromClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	GMimeFilterClass *filter_class = GMIME_FILTER_CLASS (klass);
	
	parent_class = g_type_class_ref (GMIME_TYPE_FILTER);
	
	object_class->finalize = g_mime_filter_from_finalize;
	
	filter_class->copy = filter_copy;
	filter_class->filter = filter_filter;
	filter_class->complete = filter_complete;
	filter_class->reset = filter_reset;
}

static void
g_mime_filter_from_init (GMimeFilterFrom *filter, GMimeFilterFromClass *klass)
{
	filter->midline = FALSE;
}

static void
g_mime_filter_from_finalize (GObject *object)
{
	G_OBJECT_CLASS (parent_class)->finalize (object);
}


static GMimeFilter *
filter_copy (GMimeFilter *filter)
{
	GMimeFilterFrom *from = (GMimeFilterFrom *) filter;
	
	return g_mime_filter_from_new (from->mode);
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
	int fromcount = 0;
	char *outptr;
	size_t left;
	
	inptr = in;
	inend = inptr + len;
	
	while (inptr < inend) {
		register int c = -1;
		
		if (from->midline) {
			while (inptr < inend && (c = *inptr++) != '\n')
				;
		}
		
		if (c == '\n' || !from->midline) {
			left = (size_t) (inend - inptr);
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
						
						node = g_alloca (sizeof (struct fromnode));
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
		if (from->mode == GMIME_FILTER_FROM_MODE_ARMOR)
			len += (fromcount * 2);
		else
			len += fromcount;
		
		g_mime_filter_set_size (filter, len, FALSE);
		
		node = head;
		inptr = in;
		outptr = filter->outbuf;
		while (node) {
			memcpy (outptr, inptr, (unsigned) (node->pointer - inptr));
			outptr += node->pointer - inptr;
			if (from->mode == GMIME_FILTER_FROM_MODE_ARMOR) {
				*outptr++ = '=';
				*outptr++ = '4';
				*outptr++ = '6';
				inptr = node->pointer + 1;
			} else {
				*outptr++ = '>';
				inptr = node->pointer;
			}
			
			node = node->next;
		}
		
		memcpy (outptr, inptr, (size_t) (inend - inptr));
		outptr += inend - inptr;
		*out = filter->outbuf;
		*outlen = (size_t) (outptr - filter->outbuf);
		*outprespace = filter->outbuf - filter->outreal;
	} else {
		*out = in;
		*outlen = (size_t) (inend - in);
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


/**
 * g_mime_filter_from_new:
 * @mode: filter mode
 *
 * Creates a new GMimeFilterFrom filter. If @mode is
 * #GMIME_FILTER_FROM_MODE_ARMOR, the from-filter will encode from
 * lines using the quoted-printable encoding resulting in "=46rom ".
 * Using the #GMIME_FILTER_FROM_MODE_DEFAULT or
 * #GMIME_FILTER_FROM_MODE_ESCAPE mode (they are the same), from lines
 * will be escaped to ">From ".
 *
 * Note: If you plan on using a from-filter in mode ARMOR, you should
 * remember to also use a #GMimeFilterBasic filter with an encoding of
 * #GMIME_CONTENT_ENCODING_QUOTEDPRINTABLE.
 *
 * Returns: a new from filter with mode @mode.
 **/
GMimeFilter *
g_mime_filter_from_new (GMimeFilterFromMode mode)
{
	GMimeFilterFrom *new;
	
	new = g_object_newv (GMIME_TYPE_FILTER_FROM, 0, NULL);
	new->midline = FALSE;
	switch (mode) {
	case GMIME_FILTER_FROM_MODE_ARMOR:
		new->mode = mode;
		break;
	case GMIME_FILTER_FROM_MODE_ESCAPE:
	default:
		new->mode = GMIME_FILTER_FROM_MODE_ESCAPE;
		break;
	}
	
	return (GMimeFilter *) new;
}
