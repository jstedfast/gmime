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

#include "gmime-filter-checksum.h"


#include "gmime-object-private.h"

/**
 * SECTION: gmime-filter-checksum
 * @title: GMimeFilterChecksum
 * @short_description: Calculate a checksum
 * @see_also: #GMimeFilter
 *
 * Calculate a checksum for a stream.
 **/

static void g_mime_filter_checksum_class_init (GMimeFilterChecksumClass *klass);
static void g_mime_filter_checksum_init (GMimeFilterChecksum *filter);
static void g_mime_filter_checksum_finalize (GObject *object);

static GMimeFilter *filter_copy (GMimeFilter *filter);
static void filter_filter (GMimeFilter *filter, char *in, size_t len, size_t prespace,
			   char **out, size_t *outlen, size_t *outprespace);
static void filter_complete (GMimeFilter *filter, char *in, size_t len, size_t prespace,
			     char **out, size_t *outlen, size_t *outprespace);
static void filter_reset (GMimeFilter *filter);


static GMimeFilterClass *parent_class = NULL;



G_DEFINE_TYPE_WITH_PRIVATE (GMimeFilterChecksum, g_mime_filter_checksum, GMIME_TYPE_FILTER)

GMimeFilterChecksumPrivate *
_g_mime_filter_checksum_get_private (GMimeFilterChecksum *object)
{
	return g_mime_filter_checksum_get_instance_private (object);
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
g_mime_filter_checksum_init (GMimeFilterChecksum *filter)
{
	GMimeFilterChecksumPrivate *priv = _g_mime_filter_checksum_get_private (filter);

	priv->checksum = NULL;
}

static void
g_mime_filter_checksum_finalize (GObject *object)
{
	GMimeFilterChecksumPrivate *priv = _g_mime_filter_checksum_get_private ((GMimeFilterChecksum *) object);

	if (priv->checksum)
		g_checksum_free (priv->checksum);
	
	G_OBJECT_CLASS (parent_class)->finalize (object);
}


static GMimeFilter *
filter_copy (GMimeFilter *filter)
{
	GMimeFilterChecksumPrivate *priv = _g_mime_filter_checksum_get_private ((GMimeFilterChecksum *) filter);
	GMimeFilterChecksumPrivate *copy_priv;
	GChecksum *checksum = priv->checksum;
	GMimeFilterChecksum *copy;
	
	copy = g_object_new (GMIME_TYPE_FILTER_CHECKSUM, NULL);
	copy_priv = _g_mime_filter_checksum_get_private (copy);

	copy_priv->checksum = checksum ? g_checksum_copy (checksum) : NULL;
	
	return (GMimeFilter *) copy;
}


static void
filter_filter (GMimeFilter *filter, char *in, size_t len, size_t prespace,
	       char **out, size_t *outlen, size_t *outprespace)
{
	GMimeFilterChecksumPrivate *priv = _g_mime_filter_checksum_get_private ((GMimeFilterChecksum *) filter);
	GChecksum *checksum = priv->checksum;
	
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
	GMimeFilterChecksumPrivate *priv = _g_mime_filter_checksum_get_private (((GMimeFilterChecksum *) filter));
	GChecksum *checksum = priv->checksum;
	
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
	GMimeFilterChecksumPrivate *priv;
	GMimeFilterChecksum *checksum;
	
	checksum = g_object_new (GMIME_TYPE_FILTER_CHECKSUM, NULL);
	priv = _g_mime_filter_checksum_get_private (checksum);

	priv->checksum = g_checksum_new (type);
	
	return (GMimeFilter *) checksum;
}


/**
 * g_mime_filter_checksum_get_digest:
 * @checksum: checksum filter object
 * @digest: (array length=len): the digest buffer
 * @len: the length of the digest buffer
 *
 * Outputs the checksum digest into @digest.
 *
 * Returns: the number of bytes used of the @digest buffer.
 **/
size_t
g_mime_filter_checksum_get_digest (GMimeFilterChecksum *checksum, unsigned char *digest, size_t len)
{
	GMimeFilterChecksumPrivate *priv;

	g_return_val_if_fail (GMIME_IS_FILTER_CHECKSUM (checksum), 0);
	
	priv = _g_mime_filter_checksum_get_private (checksum);

	g_checksum_get_digest (priv->checksum, digest, &len);
	
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
	GMimeFilterChecksumPrivate *priv;

	g_return_val_if_fail (GMIME_IS_FILTER_CHECKSUM (checksum), NULL);

	priv = _g_mime_filter_checksum_get_private (checksum);

	return g_strdup (g_checksum_get_string (priv->checksum));
}
