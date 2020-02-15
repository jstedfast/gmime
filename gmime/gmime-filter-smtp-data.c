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

#include "gmime-filter-smtp-data.h"


/**
 * SECTION: gmime-filter-smtp-data
 * @title: GMimeFilterSmtpData
 * @short_description: Byte-stuffs outgoing SMTP DATA.
 *
 * A #GMimeFilter for byte-stuffing outgoing SMTP DATA.
 **/


static void g_mime_filter_smtp_data_class_init (GMimeFilterSmtpDataClass *klass);
static void g_mime_filter_smtp_data_init (GMimeFilterSmtpData *filter, GMimeFilterSmtpDataClass *klass);

static GMimeFilter *filter_copy (GMimeFilter *filter);
static void filter_filter (GMimeFilter *filter, char *in, size_t len, size_t prespace,
			   char **out, size_t *outlen, size_t *outprespace);
static void filter_reset (GMimeFilter *filter);


static GMimeFilterClass *parent_class = NULL;


GType
g_mime_filter_smtp_data_get_type (void)
{
	static GType type = 0;
	
	if (!type) {
		static const GTypeInfo info = {
			sizeof (GMimeFilterSmtpDataClass),
			NULL, /* base_class_init */
			NULL, /* base_class_finalize */
			(GClassInitFunc) g_mime_filter_smtp_data_class_init,
			NULL, /* class_finalize */
			NULL, /* class_data */
			sizeof (GMimeFilterSmtpData),
			0,    /* n_preallocs */
			(GInstanceInitFunc) g_mime_filter_smtp_data_init,
		};
		
		type = g_type_register_static (GMIME_TYPE_FILTER, "GMimeFilterSmtpData", &info, 0);
	}
	
	return type;
}


static void
g_mime_filter_smtp_data_class_init (GMimeFilterSmtpDataClass *klass)
{
	GMimeFilterClass *filter_class = GMIME_FILTER_CLASS (klass);
	
	parent_class = g_type_class_ref (GMIME_TYPE_FILTER);
	
	filter_class->copy = filter_copy;
	filter_class->filter = filter_filter;
	filter_class->complete = filter_filter;
	filter_class->reset = filter_reset;
}

static void
g_mime_filter_smtp_data_init (GMimeFilterSmtpData *filter, GMimeFilterSmtpDataClass *klass)
{
	filter->bol = TRUE;
}


static GMimeFilter *
filter_copy (GMimeFilter *filter)
{
	return g_mime_filter_smtp_data_new ();
}

static void
filter_filter (GMimeFilter *filter, char *inbuf, size_t inlen, size_t prespace,
	       char **outbuf, size_t *outlen, size_t *outprespace)
{
	GMimeFilterSmtpData *smtp = (GMimeFilterSmtpData *) filter;
	register const char *inptr = inbuf;
	const char *inend = inbuf + inlen;
	gboolean escape = smtp->bol;
	int ndots = 0;
	char *outptr;
	
	while (inptr < inend) {
		if (*inptr == '.' && escape) {
			escape = FALSE;
			ndots++;
		} else {
			escape = *inptr == '\n';
		}
		
		inptr++;
	}
	
	g_mime_filter_set_size (filter, inlen + ndots, FALSE);
	
	outptr = filter->outbuf;
	inptr = inbuf;
	
	while (inptr < inend) {
		if (*inptr == '.' && smtp->bol) {
			smtp->bol = FALSE;
			*outptr++ = '.';
		} else {
			smtp->bol = *inptr == '\n';
		}
		
		*outptr++ = *inptr++;
	}
	
	*outlen = outptr - filter->outbuf;
	*outprespace = filter->outpre;
	*outbuf = filter->outbuf;
}

static void
filter_reset (GMimeFilter *filter)
{
	GMimeFilterSmtpData *smtp = (GMimeFilterSmtpData *) filter;
	
	smtp->bol = TRUE;
}


/**
 * g_mime_filter_smtp_data_new:
 *
 * Creates a new #GMimeFilterSmtpData filter.
 *
 * Returns: a new #GMimeFilterSmtpData filter.
 **/
GMimeFilter *
g_mime_filter_smtp_data_new (void)
{
	return g_object_new (GMIME_TYPE_FILTER_SMTP_DATA, NULL);
}
