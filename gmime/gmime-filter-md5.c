/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 *  Authors: Jeffrey Stedfast <fejj@ximian.com>
 *
 *  Copyright 2002-2004 Ximian, Inc. (www.ximian.com)
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
 *  Foundation, Inc., 59 Temple Street #330, Boston, MA 02111-1307, USA.
 *
 */


#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "md5-utils.h"

#include "gmime-filter-md5.h"

struct _GMimeFilterMd5Private {
	MD5Context md5;
};

static void g_mime_filter_md5_class_init (GMimeFilterMd5Class *klass);
static void g_mime_filter_md5_init (GMimeFilterMd5 *filter, GMimeFilterMd5Class *klass);
static void g_mime_filter_md5_finalize (GObject *object);

static GMimeFilter *filter_copy (GMimeFilter *filter);
static void filter_filter (GMimeFilter *filter, char *in, size_t len, size_t prespace,
			   char **out, size_t *outlen, size_t *outprespace);
static void filter_complete (GMimeFilter *filter, char *in, size_t len, size_t prespace,
			     char **out, size_t *outlen, size_t *outprespace);
static void filter_reset (GMimeFilter *filter);


static GMimeFilterClass *parent_class = NULL;


GType
g_mime_filter_md5_get_type (void)
{
	static GType type = 0;
	
	if (!type) {
		static const GTypeInfo info = {
			sizeof (GMimeFilterMd5Class),
			NULL, /* base_class_init */
			NULL, /* base_class_finalize */
			(GClassInitFunc) g_mime_filter_md5_class_init,
			NULL, /* class_finalize */
			NULL, /* class_data */
			sizeof (GMimeFilterMd5),
			0,    /* n_preallocs */
			(GInstanceInitFunc) g_mime_filter_md5_init,
		};
		
		type = g_type_register_static (GMIME_TYPE_FILTER, "GMimeFilterMd5", &info, 0);
	}
	
	return type;
}


static void
g_mime_filter_md5_class_init (GMimeFilterMd5Class *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	GMimeFilterClass *filter_class = GMIME_FILTER_CLASS (klass);
	
	parent_class = g_type_class_ref (GMIME_TYPE_FILTER);
	
	object_class->finalize = g_mime_filter_md5_finalize;
	
	filter_class->copy = filter_copy;
	filter_class->filter = filter_filter;
	filter_class->complete = filter_complete;
	filter_class->reset = filter_reset;
}

static void
g_mime_filter_md5_init (GMimeFilterMd5 *filter, GMimeFilterMd5Class *klass)
{
	filter->priv = g_new (struct _GMimeFilterMd5Private, 1);
	md5_init (&filter->priv->md5);
}

static void
g_mime_filter_md5_finalize (GObject *object)
{
	GMimeFilterMd5 *filter = (GMimeFilterMd5 *) object;
	
	g_free (filter->priv);
	
	G_OBJECT_CLASS (parent_class)->finalize (object);
}


static GMimeFilter *
filter_copy (GMimeFilter *filter)
{
	return g_mime_filter_md5_new ();
}


static void
filter_filter (GMimeFilter *filter, char *in, size_t len, size_t prespace,
	       char **out, size_t *outlen, size_t *outprespace)
{
	GMimeFilterMd5 *md5 = (GMimeFilterMd5 *) filter;
	
	md5_update (&md5->priv->md5, in, len);
	
	*out = in;
	*outlen = len;
	*outprespace = prespace;
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
	GMimeFilterMd5 *md5 = (GMimeFilterMd5 *) filter;
	
	md5_init (&md5->priv->md5);
}


/**
 * g_mime_filter_md5_new:
 *
 * Creates a new Md5 filter.
 *
 * Returns a new Md5 filter.
 **/
GMimeFilter *
g_mime_filter_md5_new (void)
{
	return (GMimeFilter *) g_object_new (GMIME_TYPE_FILTER_MD5, NULL);
}


/**
 * g_mime_filter_md5_get_digest:
 * @md5: md5 filter object
 * @digest: output buffer of at least 16 bytes
 *
 * Outputs the md5 digest into @digest.
 **/
void
g_mime_filter_md5_get_digest (GMimeFilterMd5 *md5, unsigned char digest[16])
{
	g_return_if_fail (GMIME_IS_FILTER_MD5 (md5));
	
	md5_final (&md5->priv->md5, digest);
}
