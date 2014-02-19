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

#include <errno.h>

#include "gmime-filter-charset.h"
#include "gmime-charset.h"
#include "gmime-iconv.h"


/**
 * SECTION: gmime-filter-charset
 * @title: GMimeFilterCharset
 * @short_description: Charset-conversion filter
 * @see_also:
 *
 * A #GMimeFilter which is used for converting text from one charset
 * to another.
 **/


static void g_mime_filter_charset_class_init (GMimeFilterCharsetClass *klass);
static void g_mime_filter_charset_init (GMimeFilterCharset *filter, GMimeFilterCharsetClass *klass);
static void g_mime_filter_charset_finalize (GObject *object);

static GMimeFilter *filter_copy (GMimeFilter *filter);
static void filter_filter (GMimeFilter *filter, char *in, size_t len, size_t prespace,
			   char **out, size_t *outlen, size_t *outprespace);
static void filter_complete (GMimeFilter *filter, char *in, size_t len, size_t prespace,
			     char **out, size_t *outlen, size_t *outprespace);
static void filter_reset (GMimeFilter *filter);


static GMimeFilterClass *parent_class = NULL;


GType
g_mime_filter_charset_get_type (void)
{
	static GType type = 0;
	
	if (!type) {
		static const GTypeInfo info = {
			sizeof (GMimeFilterCharsetClass),
			NULL, /* base_class_init */
			NULL, /* base_class_finalize */
			(GClassInitFunc) g_mime_filter_charset_class_init,
			NULL, /* class_finalize */
			NULL, /* class_data */
			sizeof (GMimeFilterCharset),
			0,    /* n_preallocs */
			(GInstanceInitFunc) g_mime_filter_charset_init,
		};
		
		type = g_type_register_static (GMIME_TYPE_FILTER, "GMimeFilterCharset", &info, 0);
	}
	
	return type;
}


static void
g_mime_filter_charset_class_init (GMimeFilterCharsetClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	GMimeFilterClass *filter_class = GMIME_FILTER_CLASS (klass);
	
	parent_class = g_type_class_ref (GMIME_TYPE_FILTER);
	
	object_class->finalize = g_mime_filter_charset_finalize;
	
	filter_class->copy = filter_copy;
	filter_class->filter = filter_filter;
	filter_class->complete = filter_complete;
	filter_class->reset = filter_reset;
}

static void
g_mime_filter_charset_init (GMimeFilterCharset *filter, GMimeFilterCharsetClass *klass)
{
	filter->from_charset = NULL;
	filter->to_charset = NULL;
	filter->cd = (iconv_t) -1;
}

static void
g_mime_filter_charset_finalize (GObject *object)
{
	GMimeFilterCharset *filter = (GMimeFilterCharset *) object;
	
	g_free (filter->from_charset);
	g_free (filter->to_charset);
	if (filter->cd != (iconv_t) -1)
		g_mime_iconv_close (filter->cd);
	
	G_OBJECT_CLASS (parent_class)->finalize (object);
}


static GMimeFilter *
filter_copy (GMimeFilter *filter)
{
	GMimeFilterCharset *charset = (GMimeFilterCharset *) filter;
	
	return g_mime_filter_charset_new (charset->from_charset, charset->to_charset);
}

static void
filter_filter (GMimeFilter *filter, char *in, size_t len, size_t prespace,
	       char **out, size_t *outlen, size_t *outprespace)
{
	GMimeFilterCharset *charset = (GMimeFilterCharset *) filter;
	size_t inleft, outleft, converted = 0;
	char *inbuf;
	char *outbuf;
	
	if (charset->cd == (iconv_t) -1)
		goto noop;
	
	g_mime_filter_set_size (filter, len * 5 + 16, FALSE);
	outbuf = filter->outbuf;
	outleft = filter->outsize;
	
	inbuf = in;
	inleft = len;
	
	do {
		converted = iconv (charset->cd, (ICONV_CONST char **) &inbuf, &inleft, (ICONV_CONST char **) &outbuf, &outleft);
		if (converted == (size_t) -1) {
			if (errno == E2BIG || errno == EINVAL)
				break;
			
			/* Note: GnuWin32's libiconv 1.9 can also set errno to ERANGE
			 * which seems to mean that it encountered a character that
			 * does not fit the specified 'from' charset. We'll handle
			 * that the same way we handle EILSEQ. */
			if (errno == EILSEQ || errno == ERANGE) {
				/*
				 * EILSEQ An invalid multibyte sequence has been  encountered
				 *        in the input.
				 *
				 * What we do here is eat the invalid bytes in the sequence
				 * and continue.
				 */
				
				inbuf++;
				inleft--;
			} else {
				/* unknown error condition */
				goto noop;
			}
		}
	} while (inleft > 0);
	
	if (inleft > 0) {
		/* We've either got an E2BIG or EINVAL. Save the
                   remainder of the buffer as we'll process this next
                   time through */
		g_mime_filter_backup (filter, inbuf, inleft);
	}
	
	*out = filter->outbuf;
	*outlen = outbuf - filter->outbuf;
	*outprespace = filter->outpre;
	
	return;
	
 noop:
	
	*out = in;
	*outlen = len;
	*outprespace = prespace;
}

static void 
filter_complete (GMimeFilter *filter, char *in, size_t len, size_t prespace,
		 char **out, size_t *outlen, size_t *outprespace)
{
	GMimeFilterCharset *charset = (GMimeFilterCharset *) filter;
	size_t inleft, outleft, converted = 0;
	char *inbuf;
	char *outbuf;
	
	if (charset->cd == (iconv_t) -1)
		goto noop;
	
	g_mime_filter_set_size (filter, len * 5 + 16, FALSE);
	outbuf = filter->outbuf;
	outleft = filter->outsize;
	
	inbuf = in;
	inleft = len;
	
	if (inleft > 0) {
		do {
			converted = iconv (charset->cd, (ICONV_CONST char **) &inbuf, &inleft, (ICONV_CONST char **) &outbuf, &outleft);
			if (converted != (size_t) -1)
				continue;
			
			if (errno == E2BIG) {
				/*
				 * E2BIG   There is not sufficient room at *outbuf.
				 *
				 * We just need to grow our outbuffer and try again.
				 */
				
				converted = outbuf - filter->outbuf;
				g_mime_filter_set_size (filter, inleft * 5 + filter->outsize + 16, TRUE);
				outbuf = filter->outbuf + converted;
				outleft = filter->outsize - converted;
			} else if (errno == EILSEQ) {
				/*
				 * EILSEQ An invalid multibyte sequence has been  encountered
				 *        in the input.
				 *
				 * What we do here is eat the invalid bytes in the sequence
				 * and continue.
				 */
				
				inbuf++;
				inleft--;
			} else if (errno == EINVAL) {
				/*
				 * EINVAL  An  incomplete  multibyte sequence has been encoun­
				 *         tered in the input.
				 *
				 * We assume that this can only happen if we've run out of
				 * bytes for a multibyte sequence, if not we're in trouble.
				 */
				
				break;
			} else {
				goto noop;
			}
		} while (inleft > 0);
	}
	
	/* flush the iconv conversion */
	while (iconv (charset->cd, NULL, NULL, &outbuf, &outleft) == (size_t) -1) {
		if (errno != E2BIG)
			break;
		
		converted = outbuf - filter->outbuf;
		g_mime_filter_set_size (filter, filter->outsize + 16, TRUE);
		outbuf = filter->outbuf + converted;
		outleft = filter->outsize - converted;
	}
	
	*out = filter->outbuf;
	*outlen = outbuf - filter->outbuf;
	*outprespace = filter->outpre;
	
	return;
	
 noop:
	
	*out = in;
	*outlen = len;
	*outprespace = prespace;
}

static void
filter_reset (GMimeFilter *filter)
{
	GMimeFilterCharset *charset = (GMimeFilterCharset *) filter;
	
	if (charset->cd != (iconv_t) -1)
		iconv (charset->cd, NULL, NULL, NULL, NULL);
}


/**
 * g_mime_filter_charset_new:
 * @from_charset: charset to convert from
 * @to_charset: charset to convert to
 *
 * Creates a new #GMimeFilterCharset filter.
 *
 * Returns: a new charset filter or %NULL if the charset conversion is
 * not possible.
 **/
GMimeFilter *
g_mime_filter_charset_new (const char *from_charset, const char *to_charset)
{
	GMimeFilterCharset *new;
	iconv_t cd;
	
	cd = g_mime_iconv_open (to_charset, from_charset);
	if (cd == (iconv_t) -1)
		return NULL;
	
	new = g_object_newv (GMIME_TYPE_FILTER_CHARSET, 0, NULL);
	new->from_charset = g_strdup (from_charset);
	new->to_charset = g_strdup (to_charset);
	new->cd = cd;
	
	return (GMimeFilter *) new;
}
