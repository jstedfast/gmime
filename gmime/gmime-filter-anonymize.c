/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*  GMime
 *  Copyright (C) 2026 Jeffrey Stedfast
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

#include "gmime-filter-anonymize.h"
#include "gmime-table-private.h"

#include "gmime-object-private.h"

/**
 * SECTION: gmime-filter-anonymize
 * @title: GMimeFilterAnonymize
 * @short_description: Byte-oriented content anonymization filter
 * @see_also: #GMimeFilter, #GMimeStreamFilter
 *
 * Replaces each byte except ASCII space, tab, CR and LF with lowercase
 * ASCII 'x'. This includes NUL, vertical tab, form feed and every high-bit
 * byte, without interpreting character encodings or normalizing newlines.
 * Output has the same length as input and uses filter-owned storage;
 * the caller's input and prespace are not modified.
 *
 * Completion processes its supplied bytes normally without adding a
 * terminator. The filter retains no conversion state, so it can be reused
 * after completion. Copying creates a fresh filter with independent storage.
 * This filter does not anonymize message headers or MIME structure on its
 * own; byte counts and whitespace patterns are retained.
 **/

G_DEFINE_TYPE (GMimeFilterAnonymize, g_mime_filter_anonymize, GMIME_TYPE_FILTER)

static GMimeFilter *
filter_copy (GMimeFilter *filter)
{
	return g_mime_filter_anonymize_new ();
}

static void
filter_filter (GMimeFilter *filter, char *inbuf, size_t inlen, size_t prespace,
	       char **outbuf, size_t *outlen, size_t *outprespace)
{
	GMimeFilterPrivate *priv;
	size_t i;

	/* Always use separate output storage, even for all-whitespace input:
	 * MimeKit's anonymizer does not overwrite the caller's buffer. */
	g_mime_filter_set_size (filter, inlen, FALSE);

	priv = _g_mime_filter_get_private (filter);

	for (i = 0; i < inlen; i++) {
		/* Native linear whitespace matches MimeKit's four-byte set;
		 * locale/Unicode whitespace would incorrectly preserve VT or FF. */
		priv->outbuf[i] = is_lwsp (inbuf[i]) ? inbuf[i] : 'x';
	}

	*outbuf = priv->outbuf;
	*outlen = inlen;
	*outprespace = priv->outpre;
}

static void
g_mime_filter_anonymize_class_init (GMimeFilterAnonymizeClass *klass)
{
	GMimeFilterClass *filter_class = GMIME_FILTER_CLASS (klass);

	filter_class->copy = filter_copy;
	filter_class->filter = filter_filter;
	/* Every byte is emitted immediately; completion has no pending suffix. */
	filter_class->complete = filter_filter;
}

static void
g_mime_filter_anonymize_init (GMimeFilterAnonymize *filter)
{
}

/**
 * g_mime_filter_anonymize_new:
 *
 * Creates a filter that preserves ASCII space, tab, CR and LF and replaces
 * every other byte with lowercase ASCII 'x'. Filtering and completion
 * produce equally sized, filter-owned output without modifying input.
 *
 * Returns: (transfer full): a new byte anonymization filter.
 **/
GMimeFilter *
g_mime_filter_anonymize_new (void)
{
	return g_object_new (GMIME_TYPE_FILTER_ANONYMIZE, NULL);
}
