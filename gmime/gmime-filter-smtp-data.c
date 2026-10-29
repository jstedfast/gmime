/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*  GMime
 *  Copyright (C) 2000-2022 Jeffrey Stedfast
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


#include "gmime-object-private.h"

/**
 * SECTION: gmime-filter-smtp-data
 * @title: GMimeFilterSmtpData
 * @short_description: Byte-stuffs outgoing SMTP DATA.
 *
 * A #GMimeFilter for byte-stuffing outgoing SMTP DATA.
 **/


static void g_mime_filter_smtp_data_class_init (GMimeFilterSmtpDataClass *klass);
static void g_mime_filter_smtp_data_init (GMimeFilterSmtpData *filter);

static GMimeFilter *filter_copy (GMimeFilter *filter);
static void filter_filter (GMimeFilter *filter, char *in, size_t len, size_t prespace,
			   char **out, size_t *outlen, size_t *outprespace);
static void filter_reset (GMimeFilter *filter);


static GMimeFilterClass *parent_class = NULL;



G_DEFINE_TYPE_WITH_PRIVATE (GMimeFilterSmtpData, g_mime_filter_smtp_data, GMIME_TYPE_FILTER)

GMimeFilterSmtpDataPrivate *
_g_mime_filter_smtp_data_get_private (GMimeFilterSmtpData *object)
{
	return g_mime_filter_smtp_data_get_instance_private (object);
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
g_mime_filter_smtp_data_init (GMimeFilterSmtpData *filter)
{
	GMimeFilterSmtpDataPrivate *priv = _g_mime_filter_smtp_data_get_private (filter);

	priv->bol = TRUE;
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
	GMimeFilterSmtpDataPrivate *smtp = _g_mime_filter_smtp_data_get_private ((GMimeFilterSmtpData *) filter);
	GMimeFilterPrivate *priv = _g_mime_filter_get_private (filter);
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

	outptr = priv->outbuf;
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
	
	*outlen = outptr - priv->outbuf;
	*outprespace = priv->outpre;
	*outbuf = priv->outbuf;
}

static void
filter_reset (GMimeFilter *filter)
{
	GMimeFilterSmtpDataPrivate *priv = _g_mime_filter_smtp_data_get_private ((GMimeFilterSmtpData *) filter);

	priv->bol = TRUE;
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
