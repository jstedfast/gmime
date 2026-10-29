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

#include <errno.h>

#include "gmime-filter-charset.h"
#include "gmime-charset.h"
#include "gmime-iconv.h"


#include "gmime-object-private.h"

/**
 * SECTION: gmime-filter-charset
 * @title: GMimeFilterCharset
 * @short_description: Charset-conversion filter
 * @see_also:
 *
 * A #GMimeFilter which is used for converting text from one charset
 * to another using the native iconv backend. Complete input characters
 * are converted immediately, growing output storage as needed. Only an
 * incomplete input character is retained for the next call.
 *
 * Completion flushes the target charset's shift state. Reset discards
 * pending input and conversion state; copying creates a fresh converter
 * with the same charset names, not a snapshot of a partial character.
 *
 * Native conversion policy is retained: invalid or unrepresentable input
 * bytes are skipped, and an incomplete final character is discarded.
 * This differs from managed Encoding replacement fallbacks. Generic
 * UTF-16/UTF-32 byte-order marks also follow iconv rather than .NET policy.
 **/


static void g_mime_filter_charset_class_init (GMimeFilterCharsetClass *klass);
static void g_mime_filter_charset_init (GMimeFilterCharset *filter);
static void g_mime_filter_charset_finalize (GObject *object);

static GMimeFilter *filter_copy (GMimeFilter *filter);
static void filter_filter (GMimeFilter *filter, char *in, size_t len, size_t prespace,
			   char **out, size_t *outlen, size_t *outprespace);
static void filter_complete (GMimeFilter *filter, char *in, size_t len, size_t prespace,
			     char **out, size_t *outlen, size_t *outprespace);
static void filter_reset (GMimeFilter *filter);


static GMimeFilterClass *parent_class = NULL;



G_DEFINE_TYPE_WITH_PRIVATE (GMimeFilterCharset, g_mime_filter_charset, GMIME_TYPE_FILTER)

GMimeFilterCharsetPrivate *
_g_mime_filter_charset_get_private (GMimeFilterCharset *object)
{
	return g_mime_filter_charset_get_instance_private (object);
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
g_mime_filter_charset_init (GMimeFilterCharset *filter)
{
	GMimeFilterCharsetPrivate *priv = _g_mime_filter_charset_get_private (filter);

	priv->from_charset = NULL;
	priv->to_charset = NULL;
	priv->cd = (iconv_t) -1;
}

static void
g_mime_filter_charset_finalize (GObject *object)
{
	GMimeFilterCharsetPrivate *priv = _g_mime_filter_charset_get_private ((GMimeFilterCharset *) object);

	g_free (priv->from_charset);
	g_free (priv->to_charset);

	if (priv->cd != (iconv_t) -1)
		g_mime_iconv_close (priv->cd);
	
	G_OBJECT_CLASS (parent_class)->finalize (object);
}


static GMimeFilter *
filter_copy (GMimeFilter *filter)
{
	GMimeFilterCharsetPrivate *priv = _g_mime_filter_charset_get_private ((GMimeFilterCharset *) filter);

	return g_mime_filter_charset_new (priv->from_charset, priv->to_charset);
}

static void
grow_output (GMimeFilter *filter, char **outbuf, size_t *outleft)
{
	GMimeFilterPrivate *priv = _g_mime_filter_get_private (filter);
	size_t written = *outbuf - priv->outbuf;

	if (G_UNLIKELY (priv->outsize > (G_MAXSIZE - 16) / 2))
		g_error ("GMimeFilterCharset output buffer size overflow");

	/* Reallocation moves the buffer, so retain a byte offset, not iconv's
	 * old output pointer. Already converted bytes must survive growth. */
	g_mime_filter_set_size (filter, priv->outsize * 2 + 16, TRUE);

	*outbuf = priv->outbuf + written;
	*outleft = priv->outsize - written;
}

static void
convert (GMimeFilter *filter, char *in, size_t len, size_t prespace,
	 char **out, size_t *outlen, size_t *outprespace, gboolean complete)
{
	GMimeFilterCharsetPrivate *charset = _g_mime_filter_charset_get_private ((GMimeFilterCharset *) filter);
	GMimeFilterPrivate *priv = _g_mime_filter_get_private (filter);
	size_t inleft = len, outleft;
	char *inbuf = in, *outbuf;

	if (charset->cd == (iconv_t) -1) {
		*out = in;
		*outlen = len;
		*outprespace = prespace;
		return;
	}

	if (G_UNLIKELY (len > G_MAXSIZE - 16))
		g_error ("GMimeFilterCharset output buffer size overflow");

	/* The source length is only a starting estimate. Like MimeKit's
	 * decoder/encoder loop, drain complete characters in this call rather
	 * than saving valid input just because its output needs more space. */
	g_mime_filter_set_size (filter, len + 16, FALSE);

	outbuf = priv->outbuf;
	outleft = priv->outsize;

	while (inleft > 0) {
		if (iconv (charset->cd, &inbuf, &inleft, &outbuf, &outleft) != (size_t) -1)
			continue;

		if (errno == E2BIG) {
			grow_output (filter, &outbuf, &outleft);
		} else if (errno == EINVAL) {
			/* Only an incomplete input character needs a later call.
			 * Preserve the native policy of dropping it at completion. */
			if (!complete)
				g_mime_filter_backup (filter, inbuf, inleft);
			break;
		} else if (errno == EILSEQ || errno == ERANGE) {
			/* Retain native byte-skipping recovery. Old GnuWin32
			 * libiconv also reports invalid input as ERANGE. */
			inbuf++;
			inleft--;
		} else {
			/* The filter API cannot return conversion errors; do not
			 * silently substitute unconverted input after partial output. */
			g_error ("GMimeFilterCharset conversion failed: %s", g_strerror (errno));
		}
	}

	if (complete) {
		/* Stateful targets can emit a final shift sequence even with no
		 * input. Grow and retry it without losing the converted prefix. */
		while (iconv (charset->cd, NULL, NULL, &outbuf, &outleft) == (size_t) -1) {
			if (errno != E2BIG)
				g_error ("GMimeFilterCharset completion failed: %s", g_strerror (errno));
			grow_output (filter, &outbuf, &outleft);
		}
	}

	*out = priv->outbuf;
	*outlen = outbuf - priv->outbuf;
	*outprespace = priv->outpre;
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
	GMimeFilterCharsetPrivate *priv = _g_mime_filter_charset_get_private ((GMimeFilterCharset *) filter);

	if (priv->cd != (iconv_t) -1 && iconv (priv->cd, NULL, NULL, NULL, NULL) == (size_t) -1)
		g_error ("GMimeFilterCharset reset failed: %s", g_strerror (errno));
}


/**
 * g_mime_filter_charset_new:
 * @from_charset: charset to convert from
 * @to_charset: charset to convert to
 *
 * Creates a new #GMimeFilterCharset filter.
 *
 * Charset names are copied and remain available through
 * g_mime_filter_charset_get_from_charset() and
 * g_mime_filter_charset_get_to_charset().
 *
 * Returns: (nullable) (transfer full): a new charset filter or %NULL if
 * the charset conversion is not possible, with errno set.
 **/
GMimeFilter *
g_mime_filter_charset_new (const char *from_charset, const char *to_charset)
{
	GMimeFilterCharsetPrivate *priv;
	GMimeFilterCharset *charset;
	iconv_t cd;
	
	cd = g_mime_iconv_open (to_charset, from_charset);
	if (cd == (iconv_t) -1)
		return NULL;
	
	charset = g_object_new (GMIME_TYPE_FILTER_CHARSET, NULL);
	priv = _g_mime_filter_charset_get_private (charset);

	priv->from_charset = g_strdup (from_charset);
	priv->to_charset = g_strdup (to_charset);
	priv->cd = cd;
	
	return (GMimeFilter *) charset;
}

/**
 * g_mime_filter_charset_get_from_charset:
 * @filter: a #GMimeFilterCharset
 *
 * Gets the original source charset name supplied to the constructor.
 * The name is not replaced with an iconv alias or a canonical MIME name.
 *
 * Returns: (transfer none): the filter-owned source charset name.
 **/
const char *
g_mime_filter_charset_get_from_charset (GMimeFilterCharset *filter)
{
	GMimeFilterCharsetPrivate *priv;

	g_return_val_if_fail (GMIME_IS_FILTER_CHARSET (filter), NULL);

	priv = _g_mime_filter_charset_get_private (filter);

	return priv->from_charset;
}

/**
 * g_mime_filter_charset_get_to_charset:
 * @filter: a #GMimeFilterCharset
 *
 * Gets the original target charset name supplied to the constructor.
 * The name is not replaced with an iconv alias or a canonical MIME name.
 *
 * Returns: (transfer none): the filter-owned target charset name.
 **/
const char *
g_mime_filter_charset_get_to_charset (GMimeFilterCharset *filter)
{
	GMimeFilterCharsetPrivate *priv;

	g_return_val_if_fail (GMIME_IS_FILTER_CHARSET (filter), NULL);

	priv = _g_mime_filter_charset_get_private (filter);

	return priv->to_charset;
}
