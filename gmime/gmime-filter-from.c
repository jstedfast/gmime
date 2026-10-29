/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*  GMime
 *  Copyright (C) 2000-2026 Jeffrey Stedfast
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

#include "gmime-filter-from.h"


#include "gmime-object-private.h"

/**
 * SECTION: gmime-filter-from
 * @title: GMimeFilterFrom
 * @short_description: Escape MBox From_ lines
 * @see_also: #GMimeFilter
 *
 * Escapes exact, case-sensitive "From " markers at the beginning of input
 * or immediately after LF. Escape mode inserts ">", while armor mode
 * replaces only the leading F with "=46". Bare CR is not a line boundary,
 * and existing ">From " lines are left unchanged.
 *
 * Incomplete marker prefixes are buffered across calls. Only exact prefixes
 * are held back, and completion emits them unchanged if the marker remains
 * incomplete. Reset discards pending input and restores beginning-of-line
 * state; copying retains the mode but creates a fresh filter.
 *
 * Armoring is not a complete quoted-printable encoder. Apply it to
 * quoted-printable content and label the containing part accordingly.
 **/


static void g_mime_filter_from_class_init (GMimeFilterFromClass *klass);
static void g_mime_filter_from_init (GMimeFilterFrom *filter);
static void g_mime_filter_from_finalize (GObject *object);

static GMimeFilter *filter_copy (GMimeFilter *filter);
static void filter_filter (GMimeFilter *filter, char *in, size_t len, 
			   size_t prespace, char **out, 
			   size_t *outlen, size_t *outprespace);
static void filter_complete (GMimeFilter *filter, char *in, size_t len, 
			     size_t prespace, char **out, 
			     size_t *outlen, size_t *outprespace);
static void filter_reset (GMimeFilter *filter);


static GMimeFilterClass *parent_class = NULL;



G_DEFINE_TYPE_WITH_PRIVATE (GMimeFilterFrom, g_mime_filter_from, GMIME_TYPE_FILTER)

GMimeFilterFromPrivate *
_g_mime_filter_from_get_private (GMimeFilterFrom *object)
{
	return g_mime_filter_from_get_instance_private (object);
}


static void
g_mime_filter_from_class_init (GMimeFilterFromClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	GMimeFilterClass *filter_class = GMIME_FILTER_CLASS (klass);
	
	parent_class = g_type_class_ref (GMIME_TYPE_FILTER);
	
	object_class->finalize = g_mime_filter_from_finalize;
	
	filter_class->copy = filter_copy;
	filter_class->filter = filter_filter;
	filter_class->complete = filter_complete;
	filter_class->reset = filter_reset;
}

static void
g_mime_filter_from_init (GMimeFilterFrom *filter)
{
	GMimeFilterFromPrivate *priv = _g_mime_filter_from_get_private (filter);

	priv->midline = FALSE;
}

static void
g_mime_filter_from_finalize (GObject *object)
{
	G_OBJECT_CLASS (parent_class)->finalize (object);
}


static GMimeFilter *
filter_copy (GMimeFilter *filter)
{
	GMimeFilterFromPrivate *priv = _g_mime_filter_from_get_private ((GMimeFilterFrom *) filter);

	return g_mime_filter_from_new (priv->mode);
}

static void
convert (GMimeFilter *filter, char *in, size_t len, size_t prespace,
	 char **out, size_t *outlen, size_t *outprespace, gboolean flush)
{
	GMimeFilterFromPrivate *priv = _g_mime_filter_from_get_private ((GMimeFilterFrom *) filter);
	const char *inptr = in, *inend = in + len, *newline, *src;
	size_t consumed = len, left, offset, growth, need;
	GArray *offsets = NULL;
	char *outptr;
	guint i;

	if (priv->midline) {
		/* The last pass ended mid-line; first find the end of that line. */
		newline = memchr (inptr, '\n', len);
		if (newline) {
			inptr = newline + 1;
			priv->midline = FALSE;
		} else {
			/* No LF in this block: all of it still belongs to that line. */
			inptr = inend;
		}
	}

	while (inptr < inend) {
		/* Inside this loop we are always at the beginning of a line. */
		left = (size_t) (inend - inptr);
		newline = memchr (inptr, '\n', left);
		if (left >= 5 && memcmp (inptr, "From ", 5) == 0) {
			/* Keep match offsets on the heap, not one stack node per line. */
			if (!offsets)
				offsets = g_array_new (FALSE, FALSE, sizeof (size_t));
			offset = (size_t) (inptr - in);
			g_array_append_val (offsets, offset);
		}

		if (!newline) {
			if (left < 5 && !flush && memcmp (inptr, "From ", left) == 0) {
				/* This line might start with "From ", but we ran out of data.
				 * Exclude saved bytes from this pass's consumed input. */
				g_mime_filter_backup (filter, inptr, left);
				consumed = (size_t) (inptr - in);
				break;
			}

			priv->midline = TRUE;
			break;
		}

		inptr = newline + 1;
	}

	if (offsets) {
		GMimeFilterPrivate *filter_priv;

		growth = priv->mode == GMIME_FILTER_FROM_MODE_ARMOR ? 2 : 1;

		if (G_UNLIKELY (offsets->len > (G_MAXSIZE - consumed) / growth))
			g_error ("GMimeFilterFrom output buffer size overflow");

		need = consumed + (size_t) offsets->len * growth;
		g_mime_filter_set_size (filter, need, FALSE);

		src = in;
		filter_priv = _g_mime_filter_get_private (filter);

		outptr = filter_priv->outbuf;

		for (i = 0; i < offsets->len; i++) {
			inptr = in + g_array_index (offsets, size_t, i);
			memcpy (outptr, src, (size_t) (inptr - src));
			outptr += inptr - src;

			if (priv->mode == GMIME_FILTER_FROM_MODE_ARMOR) {
				/* Encode only the F using quoted-printable. */
				*outptr++ = '=';
				*outptr++ = '4';
				*outptr++ = '6';
				src = inptr + 1;
			} else {
				*outptr++ = '>';
				src = inptr;
			}
		}

		/* Copy the remaining input, excluding bytes saved for the next pass. */
		left = consumed - (size_t) (src - in);
		memcpy (outptr, src, left);
		*out = filter_priv->outbuf;
		*outlen = need;
		*outprespace = filter_priv->outpre;
		g_array_unref (offsets);
	} else {
		*out = in;
		*outlen = consumed;
		*outprespace = prespace;
	}
}

static void
filter_filter (GMimeFilter *filter, char *in, size_t len, size_t prespace,
	       char **out, size_t *outlen, size_t *outprespace)
{
	convert (filter, in, len, prespace, out, outlen, outprespace, FALSE);
}

static void 
filter_complete (GMimeFilter *filter, char *in, size_t len, size_t prespace,
		 char **out, size_t *outlen, size_t *outprespace)
{
	convert (filter, in, len, prespace, out, outlen, outprespace, TRUE);
}

static void
filter_reset (GMimeFilter *filter)
{
	GMimeFilterFromPrivate *priv = _g_mime_filter_from_get_private ((GMimeFilterFrom *) filter);

	priv->midline = FALSE;
}


/**
 * g_mime_filter_from_new:
 * @mode: filter mode
 *
 * Creates a new GMimeFilterFrom filter. If @mode is
 * #GMIME_FILTER_FROM_MODE_ARMOR, the from-filter will encode from
 * lines by encoding only the leading F as "=46", resulting in "=46rom ".
 * Using the #GMIME_FILTER_FROM_MODE_DEFAULT or
 * #GMIME_FILTER_FROM_MODE_ESCAPE mode (they are the same), from lines
 * will be escaped to ">From ".
 *
 * Only exact markers at the beginning of input or after LF are escaped.
 * Partial prefixes span input buffers and are emitted unchanged on completion
 * if they do not become a complete marker.
 *
 * Note: ARMOR is not a complete quoted-printable encoder. Apply it after
 * quoted-printable encoding, and set the containing part's transfer encoding
 * to #GMIME_CONTENT_ENCODING_QUOTEDPRINTABLE. Do not re-encode the inserted
 * "=46" escape.
 *
 * Returns: (transfer full): a new from filter with mode @mode.
 **/
GMimeFilter *
g_mime_filter_from_new (GMimeFilterFromMode mode)
{
	GMimeFilterFromPrivate *priv;
	GMimeFilterFrom *from;
	
	from = g_object_new (GMIME_TYPE_FILTER_FROM, NULL);
	priv = _g_mime_filter_from_get_private (from);

	priv->midline = FALSE;
	switch (mode) {
	case GMIME_FILTER_FROM_MODE_ARMOR:
		priv->mode = mode;
		break;
	case GMIME_FILTER_FROM_MODE_ESCAPE:
	default:
		priv->mode = GMIME_FILTER_FROM_MODE_ESCAPE;
		break;
	}
	
	return (GMimeFilter *) from;
}
