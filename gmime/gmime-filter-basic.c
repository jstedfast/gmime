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

#include "gmime-filter-basic.h"
#include "gmime-utils.h"


/**
 * SECTION: gmime-filter-basic
 * @title: GMimeFilterBasic
 * @short_description: Basic transfer encoding filter
 * @see_also: #GMimeFilter
 *
 * A #GMimeFilter which can encode or decode basic MIME encodings such
 * as Quoted-Printable, Base64 and UUEncode.
 **/


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
	GMimeEncoding *encoder = &basic->encoder;
	
	return g_mime_filter_basic_new (encoder->encoding, encoder->encode);
}

/* here we do all of the basic mime filtering */
static void
filter_filter (GMimeFilter *filter, char *inbuf, size_t inlen, size_t prespace,
	       char **outbuf, size_t *outlen, size_t *outprespace)
{
	GMimeFilterBasic *basic = (GMimeFilterBasic *) filter;
	GMimeEncoding *encoder = &basic->encoder;
	size_t nwritten = 0;
	size_t len;
	
	if (!encoder->encode && encoder->encoding == GMIME_CONTENT_ENCODING_UUENCODE) {
		if (!(encoder->state & GMIME_UUDECODE_STATE_BEGIN)) {
			register char *inptr, *inend;
			size_t left;
			
			inend = inbuf + inlen;
			inptr = inbuf;
			
			while (inptr < inend) {
				left = inend - inptr;
				if (left < 6) {
					if (!strncmp (inptr, "begin ", left))
						g_mime_filter_backup (filter, inptr, left);
					break;
				} else if (!strncmp (inptr, "begin ", 6)) {
					inbuf = inptr;
					while (inptr < inend && *inptr != '\n')
						inptr++;
					
					if (inptr < inend) {
						inptr++;
						encoder->state |= GMIME_UUDECODE_STATE_BEGIN;
						/* we can start uudecoding... */
						inlen = inend - inptr;
						inbuf = inptr;
					} else {
						g_mime_filter_backup (filter, inbuf, left);
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
		
		switch (encoder->state & GMIME_UUDECODE_STATE_MASK) {
		case GMIME_UUDECODE_STATE_BEGIN:
			/* "begin <mode> <filename>\n" has been found and not yet seen the end */
			break;
		default:
			/* either we haven't seen the begin-line or we've finished decoding */
			goto done;
		}
	}
	
	len = g_mime_encoding_outlen (encoder, inlen);
	g_mime_filter_set_size (filter, len, FALSE);
	nwritten = g_mime_encoding_step (encoder, inbuf, inlen, filter->outbuf);
	g_assert (nwritten <= len);
	
 done:
	*outprespace = filter->outpre;
	*outbuf = filter->outbuf;
	*outlen = nwritten;
}

static void
filter_complete (GMimeFilter *filter, char *inbuf, size_t inlen, size_t prespace,
		 char **outbuf, size_t *outlen, size_t *outprespace)
{
	GMimeFilterBasic *basic = (GMimeFilterBasic *) filter;
	GMimeEncoding *encoder = &basic->encoder;
	size_t nwritten = 0;
	size_t len;
	
	if (!encoder->encode && encoder->encoding == GMIME_CONTENT_ENCODING_UUENCODE) {
		switch (encoder->state & GMIME_UUDECODE_STATE_MASK) {
		case GMIME_UUDECODE_STATE_BEGIN:
			/* "begin <mode> <filename>\n" has been found and not yet seen the end */
			break;
		default:
			/* either we haven't seen the begin-line or we've finished decoding */
			goto done;
		}
	}
	
	len = g_mime_encoding_outlen (encoder, inlen);
	g_mime_filter_set_size (filter, len, FALSE);
	nwritten = g_mime_encoding_flush (encoder, inbuf, inlen, filter->outbuf);
	g_assert (nwritten <= len);
	
 done:
	
	*outprespace = filter->outpre;
	*outbuf = filter->outbuf;
	*outlen = nwritten;
}

/* should this 'flush' outstanding state/data bytes? */
static void
filter_reset (GMimeFilter *filter)
{
	GMimeFilterBasic *basic = (GMimeFilterBasic *) filter;
	
	g_mime_encoding_reset (&basic->encoder);
}


/**
 * g_mime_filter_basic_new:
 * @encoding: a #GMimeContentEncoding
 * @encode: %TRUE to encode or %FALSE to decode
 *
 * Creates a new basic filter for @encoding.
 *
 * Returns: a new basic encoder filter.
 **/
GMimeFilter *
g_mime_filter_basic_new (GMimeContentEncoding encoding, gboolean encode)
{
	GMimeFilterBasic *basic;
	
	basic = g_object_newv (GMIME_TYPE_FILTER_BASIC, 0, NULL);
	
	if (encode)
		g_mime_encoding_init_encode (&basic->encoder, encoding);
	else
		g_mime_encoding_init_decode (&basic->encoder, encoding);
	
	return (GMimeFilter *) basic;
}
