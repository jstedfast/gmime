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

#include <string.h>

#include "gmime-filter-best.h"


#include "gmime-object-private.h"

/**
 * SECTION: gmime-filter-best
 * @title: GMimeFilterBest
 * @short_description: Determine the best charset/encoding to use for a stream
 * @see_also: #GMimeFilter
 *
 * A #GMimeFilter which is meant to determine the best charset and/or
 * transfer encoding suitable for the stream which is filtered through
 * it.
 *
 * Encoding detection follows MimeKit's BestEncodingFilter. It preserves
 * input bytes and tracks line lengths across buffers, excluding the CRLF
 * terminator. The default maximum line length is 78; use
 * g_mime_filter_best_encoding_with_max_line_length() to select another limit.
 **/


static void g_mime_filter_best_class_init (GMimeFilterBestClass *klass);
static void g_mime_filter_best_init (GMimeFilterBest *filter);
static void g_mime_filter_best_finalize (GObject *object);

static GMimeFilter *filter_copy (GMimeFilter *filter);
static void filter_filter (GMimeFilter *filter, char *in, size_t len, size_t prespace,
			   char **out, size_t *outlen, size_t *outprespace);
static void filter_complete (GMimeFilter *filter, char *in, size_t len, size_t prespace,
			     char **out, size_t *outlen, size_t *outprespace);
static void filter_reset (GMimeFilter *filter);


static GMimeFilterClass *parent_class = NULL;



G_DEFINE_TYPE_WITH_PRIVATE (GMimeFilterBest, g_mime_filter_best, GMIME_TYPE_FILTER)

GMimeFilterBestPrivate *
_g_mime_filter_best_get_private (GMimeFilterBest *object)
{
	return g_mime_filter_best_get_instance_private (object);
}


static void
g_mime_filter_best_class_init (GMimeFilterBestClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	GMimeFilterClass *filter_class = GMIME_FILTER_CLASS (klass);
	
	parent_class = g_type_class_ref (GMIME_TYPE_FILTER);
	
	object_class->finalize = g_mime_filter_best_finalize;
	
	filter_class->copy = filter_copy;
	filter_class->filter = filter_filter;
	filter_class->complete = filter_complete;
	filter_class->reset = filter_reset;
}

static void
g_mime_filter_best_init (GMimeFilterBest *filter)
{
	GMimeFilterBestPrivate *priv = _g_mime_filter_best_get_private (filter);

	priv->frombuf[5] = '\0';
}

static void
g_mime_filter_best_finalize (GObject *object)
{
	G_OBJECT_CLASS (parent_class)->finalize (object);
}


static GMimeFilter *
filter_copy (GMimeFilter *filter)
{
	GMimeFilterBestPrivate *priv = _g_mime_filter_best_get_private ((GMimeFilterBest *) filter);

	return g_mime_filter_best_new (priv->flags);
}

static void
filter_filter (GMimeFilter *filter, char *inbuf, size_t inlen, size_t prespace,
	       char **outbuf, size_t *outlen, size_t *outprespace)
{
	GMimeFilterBestPrivate *priv = _g_mime_filter_best_get_private ((GMimeFilterBest *) filter);
	size_t i;

	if (priv->flags & GMIME_FILTER_BEST_CHARSET)
		g_mime_charset_step (&priv->charset, inbuf, inlen);
	
	if (priv->flags & GMIME_FILTER_BEST_ENCODING) {
		if (G_UNLIKELY (inlen > G_MAXUINT64 - priv->total))
			g_error ("GMimeFilterBest byte count overflow");

		priv->total += inlen;

		for (i = 0; i < inlen; i++) {
			unsigned char c = (unsigned char) inbuf[i];

			if (c != '\n') {
				if (c == 0)
					priv->count0++;
				else if (c > 127)
					priv->count8++;

				if (!priv->hadfrom && priv->fromlen < 5)
					priv->frombuf[priv->fromlen++] = c;

				priv->linelen++;
			} else {
				if (priv->previous == '\r')
					priv->linelen--;

				priv->maxline = MAX (priv->maxline, priv->linelen);
				priv->linelen = 0;

				/* Check our from-save buffer for "From ". */
				if (!priv->hadfrom && priv->fromlen == 5 && !memcmp (priv->frombuf, "From ", 5))
					priv->hadfrom = TRUE;

				priv->fromlen = 0;
			}

			priv->previous = c;
		}
	}
	
	*outprespace = prespace;
	*outlen = inlen;
	*outbuf = inbuf;
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
	GMimeFilterBestPrivate *priv = _g_mime_filter_best_get_private ((GMimeFilterBest *) filter);

	g_mime_charset_init (&priv->charset);
	priv->count0 = 0;
	priv->count8 = 0;
	priv->total = 0;
	priv->maxline = 0;
	priv->linelen = 0;
	priv->fromlen = 0;
	priv->hadfrom = FALSE;
	priv->previous = 0;
}


/**
 * g_mime_filter_best_new:
 * @flags: filter flags
 *
 * Creates a new GMimeFilterBest filter. @flags are used to determine
 * which information to keep statistics of. If the
 * #GMIME_FILTER_BEST_CHARSET bit is set, the filter will be able to
 * compute the best charset for encoding the stream of data
 * filtered. If the #GMIME_FILTER_BEST_ENCODING bit is set, the filter
 * will be able to compute the best Content-Transfer-Encoding for use
 * with the stream being filtered.
 *
 * Note: In order for the g_mime_filter_best_charset() function to
 * work, the stream being filtered MUST already be encoded in UTF-8.
 *
 * Returns: a new best filter with flags @flags.
 **/
GMimeFilter *
g_mime_filter_best_new (GMimeFilterBestFlags flags)
{
	GMimeFilterBestPrivate *priv;
	GMimeFilterBest *best;
	
	best = g_object_new (GMIME_TYPE_FILTER_BEST, NULL);
	priv = _g_mime_filter_best_get_private (best);

	priv->flags = flags;
	filter_reset ((GMimeFilter *) best);
	
	return (GMimeFilter *) best;
}


/**
 * g_mime_filter_best_charset:
 * @best: a #GMimeFilterBest
 *
 * Calculates the best charset for encoding the content filtered
 * through the @best filter.
 *
 * Returns: the name of the charset most suitable for encoding
 * the content that has been passed through the filter or %NULL
 * if the filter was not configured to detect this.
 **/
const char *
g_mime_filter_best_charset (GMimeFilterBest *best)
{
	GMimeFilterBestPrivate *priv;
	const char *charset;
	
	g_return_val_if_fail (GMIME_IS_FILTER_BEST (best), NULL);
	
	priv = _g_mime_filter_best_get_private (best);

	if (!(priv->flags & GMIME_FILTER_BEST_CHARSET))
		return NULL;
	
	charset = g_mime_charset_best_name (&priv->charset);
	
	return charset ? charset : "us-ascii";
}


/**
 * g_mime_filter_best_encoding:
 * @best: a #GMimeFilterBest
 * @constraint: a #GMimeEncodingConstraint
 *
 * Calculates the most efficient Content-Transfer-Encoding for the
 * content filtered through @best that fits within the encoding
 * @constraint, using a maximum line length of 78 excluding CRLF.
 *
 * This is equivalent to calling
 * g_mime_filter_best_encoding_with_max_line_length() with a limit of 78.
 *
 * Returns: the best encoding for the content filtered by @best.
 **/
GMimeContentEncoding
g_mime_filter_best_encoding (GMimeFilterBest *best, GMimeEncodingConstraint constraint)
{
	return g_mime_filter_best_encoding_with_max_line_length (best, constraint, 78);
}

/**
 * g_mime_filter_best_encoding_with_max_line_length:
 * @best: a #GMimeFilterBest
 * @constraint: a #GMimeEncodingConstraint
 * @max_line_length: maximum line length, excluding CRLF, from 60 through 998
 *
 * Selects a transfer encoding using MimeKit's best-encoding rules. NUL bytes,
 * high-bit byte density, long lines, and newline-terminated lines beginning
 * with "From " affect the choice. The binary constraint permits binary data
 * but still selects base64 or quoted-printable for long or From-lines.
 *
 * The current unterminated line is included without requiring completion.
 * Calling this function does not change the filter state. ASCII content
 * requiring no encoding returns %GMIME_CONTENT_ENCODING_7BIT. If encoding
 * detection is disabled, returns %GMIME_CONTENT_ENCODING_UNSPECIFIED.
 *
 * Returns: the best encoding for the content filtered by @best.
 **/
GMimeContentEncoding
g_mime_filter_best_encoding_with_max_line_length (GMimeFilterBest *best, GMimeEncodingConstraint constraint,
					       guint max_line_length)
{
	GMimeFilterBestPrivate *priv;
	guint64 longest, threshold;

	g_return_val_if_fail (GMIME_IS_FILTER_BEST (best), GMIME_CONTENT_ENCODING_UNSPECIFIED);
	g_return_val_if_fail (constraint >= GMIME_ENCODING_CONSTRAINT_7BIT &&
			      constraint <= GMIME_ENCODING_CONSTRAINT_BINARY, GMIME_CONTENT_ENCODING_UNSPECIFIED);
	g_return_val_if_fail (max_line_length >= 60 && max_line_length <= 998, GMIME_CONTENT_ENCODING_UNSPECIFIED);

	priv = _g_mime_filter_best_get_private (best);

	if (!(priv->flags & GMIME_FILTER_BEST_ENCODING))
		return GMIME_CONTENT_ENCODING_UNSPECIFIED;

	/* The final line may not be terminated by a newline. */
	longest = MAX (priv->maxline, priv->linelen);
	/* Calculate floor(total * 17 / 100) without overflowing the counters. */
	threshold = (priv->total / 100) * 17 + ((priv->total % 100) * 17) / 100;

	switch (constraint) {
	case GMIME_ENCODING_CONSTRAINT_7BIT:
		if (priv->count0 > 0)
			return GMIME_CONTENT_ENCODING_BASE64;

		if (priv->count8 > 0) {
			if (priv->count8 >= threshold)
				return GMIME_CONTENT_ENCODING_BASE64;

			return GMIME_CONTENT_ENCODING_QUOTEDPRINTABLE;
		}

		if (priv->hadfrom || longest > max_line_length)
			return GMIME_CONTENT_ENCODING_QUOTEDPRINTABLE;

		break;
	case GMIME_ENCODING_CONSTRAINT_8BIT:
		if (priv->count0 > 0)
			return GMIME_CONTENT_ENCODING_BASE64;

		if (priv->hadfrom || longest > max_line_length)
			return GMIME_CONTENT_ENCODING_QUOTEDPRINTABLE;

		if (priv->count8 > 0)
			return GMIME_CONTENT_ENCODING_8BIT;

		break;
	case GMIME_ENCODING_CONSTRAINT_BINARY:
		if (priv->hadfrom || longest > max_line_length) {
			if (priv->count0 > 0 || priv->count8 > threshold)
				return GMIME_CONTENT_ENCODING_BASE64;

			return GMIME_CONTENT_ENCODING_QUOTEDPRINTABLE;
		}

		if (priv->count0 > 0)
			return GMIME_CONTENT_ENCODING_BINARY;

		if (priv->count8 > 0)
			return GMIME_CONTENT_ENCODING_8BIT;

		break;
	}

	return GMIME_CONTENT_ENCODING_7BIT;
}
