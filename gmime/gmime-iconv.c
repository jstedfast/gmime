/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*  GMime
 *  Copyright (C) Jeffrey Stedfast
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

#include <glib.h>
#include <errno.h>
#include <string.h>

#include "gmime-charset.h"
#include "gmime-iconv.h"
#include "gmime-internal.h"


/**
 * SECTION: gmime-iconv
 * @title: gmime-iconv
 * @short_description: Low-level routines for converting text from one charset to another
 * @see_also:
 *
 * These functions are wrappers around the configured iconv(3) backend. The
 * purpose of this wrapper is to use the appropriate system charset alias for
 * the MIME charset names given as arguments. On macOS, GNU libiconv is required
 * instead of Apple's system implementation. Descriptors must only be used with
 * GMime's wrappers, not GLib's or the application's system iconv functions.
 **/


/**
 * g_mime_iconv_open: (skip)
 * @to: charset to convert to
 * @from: charset to convert from
 *
 * Allocates a coversion descriptor suitable for converting byte
 * sequences from charset @from to charset @to. The resulting
 * descriptor can be used with g_mime_iconv() any
 * number of times until closed using g_mime_iconv_close().
 *
 * See the manual page for iconv_open(3) for further details.
 *
 * Returns: (type gpointer): a new conversion descriptor for use with g_mime_iconv() on
 * success or (iconv_t) %-1 on fail as well as setting an appropriate
 * errno value.
 **/
iconv_t
g_mime_iconv_open (const char *to, const char *from)
{
	if (from == NULL || to == NULL) {
		errno = EINVAL;
		return (iconv_t) -1;
	}
	
	if (!g_ascii_strcasecmp (from, "x-unknown"))
		from = g_mime_locale_charset ();
	
	from = g_mime_charset_iconv_name (from);
	to = g_mime_charset_iconv_name (to);
	
	return iconv_open (to, from);
}


/**
 * g_mime_iconv_close: (skip)
 * @cd: (type gpointer): conversion descriptor from g_mime_iconv_open()
 *
 * Closes the iconv descriptor @cd.
 *
 * See the manual page for iconv_close(3) for further details.
 *
 * Returns: %0 on success or %-1 on fail as well as setting an
 * appropriate errno value.
 **/
int
g_mime_iconv_close (iconv_t cd)
{
	return iconv_close (cd);
}


size_t
g_mime_iconv (iconv_t cd, char **inbuf, size_t *inleft, char **outbuf, size_t *outleft)
{
	return iconv (cd, inbuf, inleft, outbuf, outleft);
}


static char *
iconv_convert (const char *text, gsize length, iconv_t cd, const char *charset, const char *replacement,
	       gsize *written, GError **error)
{
	size_t inleft = length, outleft, status;
	gsize replacement_length = replacement ? strlen (replacement) : 0, n;
	gsize capacity = MAX (length, 64), used = 0;
	char *buffer = NULL, *expanded, *out;
	char *in = (char *) text;
	const char *invalid;
	gboolean flushing = FALSE, recover;
	gboolean utf8 = charset && (!g_ascii_strcasecmp (charset, "UTF-8") || !g_ascii_strcasecmp (charset, "UTF8"));
	GConvertError code;
	int errsv;

	if (written)
		*written = 0;

	if (capacity > G_MAXSIZE - 4 || !(buffer = g_try_malloc (capacity + 4)))
		goto no_memory;

	while (TRUE) {
		out = buffer + used;
		outleft = capacity - used;

		if (utf8 && !flushing) {
			/* Some iconv implementations accept obsolete five/six-byte
			 * UTF-8. Replacement must instead follow Unicode boundaries. */
			g_utf8_validate_len (in, inleft, &invalid);
			n = invalid - in;
			if (n > outleft) {
				status = (size_t) -1;
				errsv = E2BIG;
			} else {
				memcpy (out, in, n);
				out += n;
				outleft -= n;
				in += n;
				inleft -= n;
				if (inleft && !*in) {
					g_set_error_literal (error, G_CONVERT_ERROR, G_CONVERT_ERROR_EMBEDDED_NUL,
							     "Decoded text contains an embedded NUL character");
					goto fail;
				}
				status = inleft ? (size_t) -1 : 0;
				errsv = inleft ? EILSEQ : 0;
			}
		} else {
			status = g_mime_iconv (cd, flushing ? NULL : &in, flushing ? NULL : &inleft, &out, &outleft);
			errsv = errno;
		}
		used = out - buffer;
		recover = status == (size_t) -1 && replacement && !flushing && inleft > 0 &&
			(errsv == EILSEQ || errsv == EINVAL);

		if (status != (size_t) -1) {
			if (status != 0) {
				g_set_error_literal (error, G_CONVERT_ERROR, G_CONVERT_ERROR_ILLEGAL_SEQUENCE,
						     "Conversion cannot represent all input characters");
				goto fail;
			}

			if (flushing)
				break;

			flushing = TRUE;
		} else if (errsv == E2BIG || (recover && outleft < replacement_length)) {
			if (capacity > (G_MAXSIZE - 4) / 2)
				goto no_memory;

			capacity *= 2;

			if (!(expanded = g_try_realloc (buffer, capacity + 4)))
				goto no_memory;

			buffer = expanded;
		} else if (recover) {
			n = _g_mime_charset_invalid_length (charset, (const unsigned char *) in, inleft, errsv);
			memcpy (buffer + used, replacement, replacement_length);
			used += replacement_length;
			in += n;
			inleft -= n;
			if (!inleft)
				flushing = TRUE;
		} else {
			code = errsv == EINVAL ? G_CONVERT_ERROR_PARTIAL_INPUT :
				errsv == EILSEQ ? G_CONVERT_ERROR_ILLEGAL_SEQUENCE : G_CONVERT_ERROR_FAILED;

			g_set_error (error, G_CONVERT_ERROR, code, "Cannot convert text: %s", g_strerror (errsv));
			goto fail;
		}
	}

	/* Allow callers to use UTF-16/32 output without counting its terminator. */
	memset (buffer + used, 0, 4);

	if (written)
		*written = used;

	return buffer;

no_memory:
	g_set_error_literal (error, G_CONVERT_ERROR, G_CONVERT_ERROR_NO_MEMORY,
			     "Cannot allocate charset conversion buffer");
fail:
	g_free (buffer);
	return NULL;
}

char *
_g_mime_iconv_convert_with_iconv (const char *text, gsize length, iconv_t cd, gsize *written, GError **error)
{
	return iconv_convert (text, length, cd, NULL, NULL, written, error);
}

char *
_g_mime_iconv_decode_with_fallback (const char *text, gsize length, iconv_t cd, const char *charset,
				   gsize *written, GError **error)
{
	const char *replacement = "?";

	if (!g_ascii_strcasecmp (charset, "UTF-8") || !g_ascii_strcasecmp (charset, "UTF8") ||
	    !g_ascii_strncasecmp (charset, "UTF-16", 6) || !g_ascii_strncasecmp (charset, "UTF-32", 6) ||
	    !g_ascii_strncasecmp (charset, "UCS-2", 5) || !g_ascii_strncasecmp (charset, "UCS-4", 5))
		replacement = "\357\277\275";

	return iconv_convert (text, length, cd, charset, replacement, written, error);
}


char *
_g_mime_iconv_convert (const char *text, gssize length, const char *to, const char *from,
		      gsize *written, GError **error)
{
	iconv_t cd;
	char *converted;
	int errsv;

	if (written)
		*written = 0;

	/* Strict APIs must not apply the legacy x-unknown -> locale fallback. */
	if ((cd = iconv_open (g_mime_charset_iconv_name (to), g_mime_charset_iconv_name (from))) == (iconv_t) -1) {
		errsv = errno;
		g_set_error (error, G_CONVERT_ERROR,
			     errsv == EINVAL ? G_CONVERT_ERROR_NO_CONVERSION : G_CONVERT_ERROR_FAILED,
			     "Cannot open converter from '%s' to '%s': %s", from, to, g_strerror (errsv));
		return NULL;
	}

	converted = _g_mime_iconv_convert_with_iconv (text, length < 0 ? strlen (text) : (gsize) length,
						    cd, written, error);
	g_mime_iconv_close (cd);

	return converted;
}
