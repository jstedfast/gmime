/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*  GMime
 *  Copyright (C) 2000-2007 Jeffrey Stedfast
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
 *  Foundation, 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */


#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <string.h>

#include "gmime-filter-basic.h"
#include "gmime-utils.h"

static void g_mime_filter_basic_class_init (GMimeFilterBasicClass *klass);
static void g_mime_filter_basic_init (GMimeFilterBasic *filter, GMimeFilterBasicClass *klass);
static void g_mime_filter_basic_finalize (GObject *object);

static GMimeFilter *filter_copy (GMimeFilter *filter);
static void filter_filter (GMimeFilter *filter, char *in, size_t len, size_t prespace,
			   char **out, size_t *outlen, size_t *outprespace);
static void filter_complete (GMimeFilter *filter, char *in, size_t len, size_t prespace,
			     char **out, size_t *outlen, size_t *outprespace);
static void filter_reset (GMimeFilter *filter);


static GMimeFilterClass *parent_class = NULL;


GType
g_mime_filter_basic_get_type (void)
{
	static GType type = 0;
	
	if (!type) {
		static const GTypeInfo info = {
			sizeof (GMimeFilterBasicClass),
			NULL, /* base_class_init */
			NULL, /* base_class_finalize */
			(GClassInitFunc) g_mime_filter_basic_class_init,
			NULL, /* class_finalize */
			NULL, /* class_data */
			sizeof (GMimeFilterBasic),
			0,    /* n_preallocs */
			(GInstanceInitFunc) g_mime_filter_basic_init,
		};
		
		type = g_type_register_static (GMIME_TYPE_FILTER, "GMimeFilterBasic", &info, 0);
	}
	
	return type;
}


static void
g_mime_filter_basic_class_init (GMimeFilterBasicClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	GMimeFilterClass *filter_class = GMIME_FILTER_CLASS (klass);
	
	parent_class = g_type_class_ref (GMIME_TYPE_FILTER);
	
	object_class->finalize = g_mime_filter_basic_finalize;
	
	filter_class->copy = filter_copy;
	filter_class->filter = filter_filter;
	filter_class->complete = filter_complete;
	filter_class->reset = filter_reset;
}

static void
g_mime_filter_basic_init (GMimeFilterBasic *filter, GMimeFilterBasicClass *klass)
{
	filter->type = 0;
	filter->state = 0;
	filter->save = 0;
}

static void
g_mime_filter_basic_finalize (GObject *object)
{
	G_OBJECT_CLASS (parent_class)->finalize (object);
}


static GMimeFilter *
filter_copy (GMimeFilter *filter)
{
	GMimeFilterBasic *basic = (GMimeFilterBasic *) filter;
	
	return g_mime_filter_basic_new_type (basic->type);
}

/* here we do all of the basic mime filtering */
static void
filter_filter (GMimeFilter *filter, char *in, size_t len, size_t prespace,
	       char **out, size_t *outlen, size_t *outprespace)
{
	GMimeFilterBasic *basic = (GMimeFilterBasic *) filter;
	const unsigned char *inbuf;
	unsigned char *outbuf;
	size_t newlen = 0;
	
	switch (basic->type) {
	case GMIME_FILTER_BASIC_BASE64_ENC:
		/* wont go to more than 2x size (overly conservative) */
		g_mime_filter_set_size (filter, len * 2 + 6, FALSE);
		outbuf = (unsigned char *) filter->outbuf;
		inbuf = (const unsigned char *) in;
		newlen = g_mime_utils_base64_encode_step (inbuf, len, outbuf, &basic->state, &basic->save);
		g_assert (newlen <= len * 2 + 6);
		break;
	case GMIME_FILTER_BASIC_QP_ENC:
		/* *4 is overly conservative, but will do */
		g_mime_filter_set_size (filter, len * 4 + 4, FALSE);
		outbuf = (unsigned char *) filter->outbuf;
		inbuf = (const unsigned char *) in;
		newlen = g_mime_utils_quoted_encode_step (inbuf, len, outbuf, &basic->state, &basic->save);
		g_assert (newlen <= len * 4 + 4);
		break;
	case GMIME_FILTER_BASIC_UU_ENC:
		/* won't go to more than 2 * (x + 2) + 62 */
		g_mime_filter_set_size (filter, (len + 2) * 2 + 62, FALSE);
		outbuf = (unsigned char *) filter->outbuf;
		inbuf = (const unsigned char *) in;
		newlen = g_mime_utils_uuencode_step (inbuf, len, outbuf, basic->uubuf, &basic->state,
						     &basic->save);
		g_assert (newlen <= (len + 2) * 2 + 62);
		break;
	case GMIME_FILTER_BASIC_BASE64_DEC:
		/* output can't possibly exceed the input size */
		g_mime_filter_set_size (filter, len + 3, FALSE);
		outbuf = (unsigned char *) filter->outbuf;
		inbuf = (const unsigned char *) in;
		newlen = g_mime_utils_base64_decode_step (inbuf, len, outbuf, &basic->state, &basic->save);
		g_assert (newlen <= len + 3);
		break;
	case GMIME_FILTER_BASIC_QP_DEC:
		/* output can't possibly exceed the input size */
		g_mime_filter_set_size (filter, len + 2, FALSE);
		outbuf = (unsigned char *) filter->outbuf;
		inbuf = (const unsigned char *) in;
		newlen = g_mime_utils_quoted_decode_step (inbuf, len, outbuf, &basic->state, &basic->save);
		g_assert (newlen <= len + 2);
		break;
	case GMIME_FILTER_BASIC_UU_DEC:
		if (!(basic->state & GMIME_UUDECODE_STATE_BEGIN)) {
			register char *inptr, *inend;
			size_t left;
			
			inptr = in;
			inend = inptr + len;
			
			while (inptr < inend) {
				left = inend - inptr;
				if (left < 6) {
					if (!strncmp (inptr, "begin ", left))
						g_mime_filter_backup (filter, inptr, left);
					break;
				} else if (!strncmp (inptr, "begin ", 6)) {
					for (in = inptr; inptr < inend && *inptr != '\n'; inptr++);
					if (inptr < inend) {
						inptr++;
						basic->state |= GMIME_UUDECODE_STATE_BEGIN;
						/* we can start uudecoding... */
						in = inptr;
						len = inend - in;
					} else {
						g_mime_filter_backup (filter, in, left);
					}
					break;
				}
				
				/* go to the next line */
				while (inptr < inend && *inptr != '\n')
					inptr++;
				
				if (inptr < inend)
					inptr++;
			}
		}
		
		if ((basic->state & GMIME_UUDECODE_STATE_BEGIN) && !(basic->state & GMIME_UUDECODE_STATE_END)) {
			/* "begin <mode> <filename>\n" has been found, so we can now start decoding */
			g_mime_filter_set_size (filter, len + 3, FALSE);
			outbuf = (unsigned char *) filter->outbuf;
			inbuf = (const unsigned char *) in;
			newlen = g_mime_utils_uudecode_step (inbuf, len, outbuf, &basic->state, &basic->save);
			g_assert (newlen <= len + 3);
		} else {
			newlen = 0;
		}
		break;
	}
	
	*out = filter->outbuf;
	*outlen = newlen;
	*outprespace = filter->outpre;
}

static void
filter_complete (GMimeFilter *filter, char *in, size_t len, size_t prespace,
		 char **out, size_t *outlen, size_t *outprespace)
{
	GMimeFilterBasic *basic = (GMimeFilterBasic *) filter;
	const unsigned char *inbuf;
	unsigned char *outbuf;
	size_t newlen = 0;
	
	switch (basic->type) {
	case GMIME_FILTER_BASIC_BASE64_ENC:
		/* wont go to more than 2x size (overly conservative) */
		g_mime_filter_set_size (filter, len * 2 + 6, FALSE);
		outbuf = (unsigned char *) filter->outbuf;
		inbuf = (const unsigned char *) in;
		newlen = g_mime_utils_base64_encode_close (inbuf, len, outbuf, &basic->state, &basic->save);
		g_assert (newlen <= len * 2 + 6);
		break;
	case GMIME_FILTER_BASIC_QP_ENC:
		/* len * 4 is definetly more than needed ... */
		g_mime_filter_set_size (filter, len * 4 + 4, FALSE);
		outbuf = (unsigned char *) filter->outbuf;
		inbuf = (const unsigned char *) in;
		newlen = g_mime_utils_quoted_encode_close (inbuf, len, outbuf, &basic->state, &basic->save);
		g_assert (newlen <= len * 4 + 4);
		break;
	case GMIME_FILTER_BASIC_UU_ENC:
		/* won't go to more than 2 * (x + 2) + 62 */
		g_mime_filter_set_size (filter, (len + 2) * 2 + 62, FALSE);
		outbuf = (unsigned char *) filter->outbuf;
		inbuf = (const unsigned char *) in;
		newlen = g_mime_utils_uuencode_close (inbuf, len, outbuf, basic->uubuf, &basic->state,
						      &basic->save);
		g_assert (newlen <= (len + 2) * 2 + 62);
		break;
	case GMIME_FILTER_BASIC_BASE64_DEC:
		/* output can't possibly exceed the input size */
 		g_mime_filter_set_size (filter, len, FALSE);
		outbuf = (unsigned char *) filter->outbuf;
		inbuf = (const unsigned char *) in;
		newlen = g_mime_utils_base64_decode_step (inbuf, len, outbuf, &basic->state, &basic->save);
		g_assert (newlen <= len);
		break;
	case GMIME_FILTER_BASIC_QP_DEC:
		/* output can't possibly exceed the input size */
		g_mime_filter_set_size (filter, len + 2, FALSE);
		outbuf = (unsigned char *) filter->outbuf;
		inbuf = (const unsigned char *) in;
		newlen = g_mime_utils_quoted_decode_step (inbuf, len, outbuf, &basic->state, &basic->save);
		g_assert (newlen <= len + 2);
		break;
	case GMIME_FILTER_BASIC_UU_DEC:
		if ((basic->state & GMIME_UUDECODE_STATE_BEGIN) && !(basic->state & GMIME_UUDECODE_STATE_END)) {
			/* "begin <mode> <filename>\n" has been found, so we can now start decoding */
			g_mime_filter_set_size (filter, len + 3, FALSE);
			outbuf = (unsigned char *) filter->outbuf;
			inbuf = (const unsigned char *) in;
			newlen = g_mime_utils_uudecode_step (inbuf, len, outbuf, &basic->state, &basic->save);
			g_assert (newlen <= len + 3);
		} else {
			newlen = 0;
		}
		break;
	}
	
	*out = filter->outbuf;
	*outlen = newlen;
	*outprespace = filter->outpre;
}

/* should this 'flush' outstanding state/data bytes? */
static void
filter_reset (GMimeFilter *filter)
{
	GMimeFilterBasic *basic = (GMimeFilterBasic *) filter;
	
	switch (basic->type) {
	case GMIME_FILTER_BASIC_QP_ENC:
		basic->state = -1;
		break;
	default:
		basic->state = 0;
	}
	basic->save = 0;
}


/**
 * g_mime_filter_basic_new_type:
 * @type: filter type
 *
 * Creates a new filter of type @type.
 *
 * Returns a new basic filter of type @type.
 **/
GMimeFilter *
g_mime_filter_basic_new_type (GMimeFilterBasicType type)
{
	GMimeFilterBasic *new;
	
	new = g_object_new (GMIME_TYPE_FILTER_BASIC, NULL, NULL);
	
	new->type = type;
	
	filter_reset ((GMimeFilter *) new);
	
	return (GMimeFilter *) new;
}
