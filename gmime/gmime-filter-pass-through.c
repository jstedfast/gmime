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

#include "gmime-filter-pass-through.h"

/**
 * SECTION: gmime-filter-pass-through
 * @title: GMimeFilterPassThrough
 * @short_description: Zero-copy pass-through filter
 * @see_also: #GMimeFilter, #GMimeFilterBasic
 *
 * Passes bytes through without transforming, copying or buffering them.
 * Filtering and completion return the supplied input pointer, length and
 * prespace unchanged, including for empty input. Output is borrowed from
 * the caller and has the same lifetime as the supplied input buffer.
 *
 * Unlike a #GMimeFilterBasic using a pass-through codec, this filter does
 * not allocate an output buffer. It has no conversion state to reset;
 * copying creates a new, independent filter of the same type.
 **/

G_DEFINE_TYPE (GMimeFilterPassThrough, g_mime_filter_pass_through, GMIME_TYPE_FILTER)

static GMimeFilter *
filter_copy (GMimeFilter *filter)
{
	return g_mime_filter_pass_through_new ();
}

static void
filter_filter (GMimeFilter *filter, char *inbuf, size_t inlen, size_t prespace,
	       char **outbuf, size_t *outlen, size_t *outprespace)
{
	/* Preserve the input's prespace too, so downstream filters can use it
	 * without forcing the stream to allocate and copy another buffer. */
	*outbuf = inbuf;
	*outlen = inlen;
	*outprespace = prespace;
}

static void
g_mime_filter_pass_through_class_init (GMimeFilterPassThroughClass *klass)
{
	GMimeFilterClass *filter_class = GMIME_FILTER_CLASS (klass);

	filter_class->copy = filter_copy;
	filter_class->filter = filter_filter;
	/* There is no pending state: completion must also pass its input through. */
	filter_class->complete = filter_filter;
}

static void
g_mime_filter_pass_through_init (GMimeFilterPassThrough *filter)
{
}

/**
 * g_mime_filter_pass_through_new:
 *
 * Creates a filter that passes data through without processing or copying
 * it. Completion emits the supplied input without adding bytes, and the
 * filter can be reused without resetting it.
 *
 * Returns: (transfer full): a new pass-through filter.
 **/
GMimeFilter *
g_mime_filter_pass_through_new (void)
{
	return g_object_new (GMIME_TYPE_FILTER_PASS_THROUGH, NULL);
}
