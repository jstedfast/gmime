/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 *  Authors: Jeffrey Stedfast <fejj@ximian.com>
 *
 *  Copyright 2002 Ximain, Inc. (www.ximian.com)
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

#include <glib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>

#include "gmime-iconv-utils.h"
#include "gmime-charset.h"


static gboolean initialized = FALSE;

static iconv_t utf8_to_locale;
static iconv_t locale_to_utf8;


static void
iconv_utils_init (void)
{
	const char *utf8, *locale;
	
	g_mime_charset_init ();
	
	utf8 = g_mime_charset_name ("utf-8");
	locale = g_mime_charset_name (g_mime_charset_locale_name ());
	
	utf8_to_locale = iconv_open (locale, utf8);
	locale_to_utf8 = iconv_open (utf8, locale);
	
	initialized = TRUE;
}


/**
 * g_mime_iconv_strndup:
 * @cd: conversion descriptor
 * @string: string in source charset
 * @n: number of bytes to convert
 *
 * Allocates a new string buffer containing the first @n bytes of
 * @string converted to the destination charset as described by the
 * conversion descriptor @cd.
 *
 * Returns a new string buffer containing the first @n bytes of
 * @string converted to the destination charset as described by the
 * conversion descriptor @cd.
 **/
char *
g_mime_iconv_strndup (iconv_t cd, const char *string, size_t n)
{
	size_t inleft, outleft, converted = 0;
	char *out, *outbuf;
	const char *inbuf;
	size_t outlen;
	
	if (cd == (iconv_t) -1)
		return g_strndup (string, n);
	
	outlen = n * 2 + 16;
	out = g_malloc (outlen + 1);
	
	inbuf = string;
	inleft = n;
	
	do {
		outbuf = out + converted;
		outleft = outlen - converted;
		
		converted = iconv (cd, (char **) &inbuf, &inleft, &outbuf, &outleft);
		if (converted == (size_t) -1) {
			if (errno != E2BIG && errno != EINVAL)
				goto fail;
		}
		
		/*
		 * E2BIG   There is not sufficient room at *outbuf.
		 *
		 * We just need to grow our outbuffer and try again.
		 */
		
		converted = outlen - outleft;
		if (errno == E2BIG) {
			outlen += inleft * 2 + 16;
			out = g_realloc (out, outlen + 1);
			outbuf = out + converted;
		}
		
	} while (errno == E2BIG && inleft > 0);
	
	/*
	 * EINVAL  An  incomplete  multibyte sequence has been encoun­
	 *         tered in the input.
	 *
	 * We'll just have to ignore it...
	 */
	
	/* flush the iconv conversion */
	iconv (cd, NULL, NULL, &outbuf, &outleft);
	*outbuf = '\0';
	
	/* reset the cd */
	iconv (cd, NULL, NULL, NULL, NULL);
	
	return out;
	
 fail:
	
	g_warning ("g_mime_iconv_strndup: %s", g_strerror (errno));
	
	g_free (out);
	
	/* reset the cd */
	iconv (cd, NULL, NULL, NULL, NULL);
	
	return NULL;
}


/**
 * g_mime_iconv_strdup:
 * @cd: conversion descriptor
 * @string: string in source charset
 *
 * Allocates a new string buffer containing @string converted to
 * the destination charset described in @cd.
 *
 * Returns a new string buffer containing the original string
 * converted to the new charset.
 **/
char *
g_mime_iconv_strdup (iconv_t cd, const char *string)
{
	return g_mime_iconv_strndup (cd, string, strlen (string));
}


/**
 * g_mime_iconv_locale_to_utf8:
 * @string: string in locale charset
 *
 * Allocates a new string buffer containing @string in UTF-8.
 *
 * Returns a new string buffer containing @string converted to UTF-8.
 **/
char *
g_mime_iconv_locale_to_utf8 (const char *string)
{
	if (!initialized)
		iconv_utils_init ();
	
	return g_mime_iconv_strdup (locale_to_utf8, string);
}


/**
 * g_mime_iconv_locale_to_utf8_length:
 * @string: string in locale charset
 * @n: number of bytes to convert
 *
 * Allocates a new string buffer containing the first @n bytes of
 * @string converted to UTF-8.
 *
 * Returns a new string buffer containing the first @n bytes of
 * @string converted to UTF-8.
 **/
char *
g_mime_iconv_locale_to_utf8_length (const char *string, size_t n)
{
	if (!initialized)
		iconv_utils_init ();
	
	return g_mime_iconv_strndup (locale_to_utf8, string, n);
}


/**
 * g_mime_iconv_utf8_to_locale:
 * @string: string in UTF-8 charset
 *
 * Allocates a new string buffer containing @string converted to the
 * user's locale charset.
 *
 * Returns a new string buffer containing @string converted to the
 * user's locale charset.
 **/
char *
g_mime_iconv_utf8_to_locale (const char *string)
{
	if (!initialized)
		iconv_utils_init ();
	
	return g_mime_iconv_strdup (utf8_to_locale, string);
}


/**
 * g_mime_iconv_utf8_to_locale_length:
 * @string: string in UTF-8 charset
 * @n: number of bytes to convert
 *
 * Allocates a new string buffer containing the first @n bytes of
 * @string converted to the user's locale charset.
 *
 * Returns a new string buffer containing the first @n bytes of
 * @string converted to the user's locale charset.
 **/
char *
g_mime_iconv_utf8_to_locale_length (const char *string, size_t n)
{
	if (!initialized)
		iconv_utils_init ();
	
	return g_mime_iconv_strndup (utf8_to_locale, string, n);
}
