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

#include "gmime-filter-checksum.h"


/**
 * SECTION: gmime-filter-checksum
 * @title: GMimeFilterChecksum
 * @short_description: Calculate a checksum
 * @see_also: #GMimeFilter
 *
 * Calculate a checksum for a stream.
 **/

static void g_mime_filter_checksum_class_init (GMimeFilterChecksumClass *klass);
static void g_mime_filter_checksum_init (GMimeFilterChecksum *filter, GMimeFilterChecksumClass *klass);
static void g_mime_filter_checksum_finalize (GObject *object);

static GMimeFilter *filter_copy (GMimeFilter *filter);
static void filter_filter (GMimeFilter *filter, char *in, size_t len, size_t prespace,
			   char **out, size_t *outlen, size_t *outprespace);
static void filter_complete (GMimeFilter *filter, char *in, size_t len, size_t prespace,
			     char **out, size_t *outlen, size_t *outprespace);
static void filter_reset (GMimeFilter *filter);


static GMimeFilterClass *parent_class = NULL;


GType
g_mime_filter_checksum_get_type (void)
{
	static GType type = 0;
	
	if (!type) {
		static const GTypeInfo info = {
			sizeof (GMimeFilterChecksumClass),
			NULL, /* base_class_init */
			NULL, /* base_class_finalize */
			(GClassInitFunc) g_mime_filter_checksum_class_init,
			NULL, /* class_finalize */
			NULL, /* class_data */
			sizeof (GMimeFilterChecksum),
			0,    /* n_preallocs */
			(GInstanceInitFunc) g_mime_filter_checksum_init,
		};
		
		type = g_type_register_static (GMIME_TYPE_FILTER, "GMimeFilterChecksum", &info, 0);
	}
	
	return type;
}


static void
g_mime_filter_checksum_class_init (GMimeFilterChecksumClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	GMimeFilterClass *filter_class = GMIME_FILTER_CLASS (klass);
	
	parent_class = g_type_class_ref (GMIME_TYPE_FILTER);
	
	object_class->finalize = g_mime_filter_checksum_finalize;
	
	filter_class->copy = filter_copy;
	filter_class->filter = filter_filter;
	filter_class->complete = filter_complete;
	filter_class->reset = filter_reset;
}

static void
g_mime_filter_checksum_init (GMimeFilterChecksum *filter, GMimeFilterChecksumClass *klass)
{
	filter->checksum = NULL;
}

static void
g_mime_filter_checksum_finalize (GObject *object)
{
	GMimeFilterChecksum *filter = (GMimeFilterChecksum *) object;
	
	if (filter->checksum)
		g_checksum_free (filter->checksum);
	
	G_OBJECT_CLASS (parent_class)->finalize (object);
}


static GMimeFilter *
filter_copy (GMimeFilter *filter)
{
	GChecksum *checksum = ((GMimeFilterChecksum *) filter)->checksum;
	GMimeFilterChecksum *copy;
	
	copy = g_object_new (GMIME_TYPE_FILTER_CHECKSUM, NULL);
	copy->checksum = checksum ? g_checksum_copy (checksum) : NULL;
	
	return (GMimeFilter *) copy;
}


static void
filter_filter (GMimeFilter *filter, char *in, size_t len, size_t prespace,
	       char **out, size_t *outlen, size_t *outprespace)
{
	GChecksum *checksum = ((GMimeFilterChecksum *) filter)->checksum;
	
	g_checksum_update (checksum, (unsigned char *) in, len);
	
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
	GChecksum *checksum = ((GMimeFilterChecksum *) filter)->checksum;
	
	g_checksum_reset (checksum);
}


/**
 * g_mime_filter_checksum_new:
 * @type: the type of checksum
 *
 * Creates a new checksum filter.
 *
 * Returns: a new #GMimeFilterChecksum filter.
 **/
GMimeFilter *
g_mime_filter_checksum_new (GChecksumType type)
{
	GMimeFilterChecksum *checksum;
	
	checksum = g_object_new (GMIME_TYPE_FILTER_CHECKSUM, NULL);
	checksum->checksum = g_checksum_new (type);
	
	return (GMimeFilter *) checksum;
}


/**
 * g_mime_filter_checksum_get_digest:
 * @checksum: checksum filter object
 * @digest: the digest buffer
 * @len: the length of the digest buffer
 *
 * Outputs the checksum digest into @digest.
 *
 * Returns: the number of bytes used of the @digest buffer.
 **/
size_t
g_mime_filter_checksum_get_digest (GMimeFilterChecksum *checksum, unsigned char *digest, size_t len)
{
	g_return_val_if_fail (GMIME_IS_FILTER_CHECKSUM (checksum), 0);
	
	g_checksum_get_digest (checksum->checksum, digest, &len);
	
	return len;
}


/**
 * g_mime_filter_checksum_get_string:
 * @checksum: checksum filter object
 *
 * Outputs the checksum digest as a newly allocated hexadecimal string.
 *
 * Returns: the hexadecimal representation of the checksum. The returned string should be freed with g_free() when no longer needed.
 **/
gchar *
g_mime_filter_checksum_get_string (GMimeFilterChecksum *checksum)
{
	g_return_val_if_fail (GMIME_IS_FILTER_CHECKSUM (checksum), NULL);

	return g_strdup (g_checksum_get_string (checksum->checksum));
}
