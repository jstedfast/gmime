/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*  GMime
 *  Copyright (C) 2000-2014 Jeffrey Stedfast
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

#include <stdio.h>
#include <string.h>
#include <errno.h>

#include "gmime-iconv-utils.h"
#include "gmime-charset.h"

#ifdef ENABLE_WARNINGS
#define w(x) x
#else
#define w(x)
#endif /* ENABLE_WARNINGS */


/**
 * SECTION: gmime-iconv-utils
 * @title: gmime-iconv-utils
 * @short_description: High-level routines for converting text from one charset to another
 * @see_also:
 *
 * Charset conversion utility functions.
 **/


#ifdef G_THREADS_ENABLED
extern void _g_mime_iconv_utils_unlock (void);
extern void _g_mime_iconv_utils_lock (void);
#define UNLOCK() _g_mime_iconv_utils_unlock ()
#define LOCK()   _g_mime_iconv_utils_lock ()
#else
#define UNLOCK()
#define LOCK()
#endif /* G_THREADS_ENABLED */

static iconv_t utf8_to_locale = (iconv_t) -1;
static iconv_t locale_to_utf8 = (iconv_t) -1;


void
g_mime_iconv_utils_init (void)
{
	const char *utf8, *locale;
	
	utf8 = g_mime_charset_iconv_name ("UTF-8");
	
	if (!(locale = g_mime_locale_charset ()))
		locale = "US-ASCII";
	
	if ((locale = g_mime_charset_iconv_name (locale))) {
		utf8_to_locale = iconv_open (locale, utf8);
		locale_to_utf8 = iconv_open (utf8, locale);
	}
}

void
g_mime_iconv_utils_shutdown (void)
{
	if (utf8_to_locale != (iconv_t) -1) {
		iconv_close (utf8_to_locale);
		utf8_to_locale = (iconv_t) -1;
	}
	
	if (locale_to_utf8 != (iconv_t) -1) {
		iconv_close (locale_to_utf8);
		locale_to_utf8 = (iconv_t) -1;
	}
}


/**
 * g_mime_iconv_strndup: (skip)
 * @cd: conversion descriptor
 * @str: string in source charset
 * @n: number of bytes to convert
 *
 * Allocates a new string buffer containing the first @n bytes of @str
 * converted to the destination charset as described by the conversion
 * descriptor @cd.
 *
 * Returns: a new string buffer containing the first @n bytes of
 * @str converted to the destination charset as described by the
 * conversion descriptor @cd.
 **/
char *
g_mime_iconv_strndup (iconv_t cd, const char *str, size_t n)
{
	size_t inleft, outleft, converted = 0;
	char *out, *outbuf;
	const char *inbuf;
	size_t outlen;
	int errnosav;
	
	if (cd == (iconv_t) -1)
		return g_strndup (str, n);
	
	outlen = n * 2 + 16;
	out = g_malloc (outlen + 4);
	
	inbuf = str;
	inleft = n;
	
	do {
		errno = 0;
		outbuf = out + converted;
		outleft = outlen - converted;
		
		converted = iconv (cd, (char **) &inbuf, &inleft, &outbuf, &outleft);
		if (converted != (size_t) -1 || errno == EINVAL) {
			/*
			 * EINVAL  An  incomplete  multibyte sequence has been encoun­
			 *         tered in the input.
			 *
			 * We'll just have to ignore it...
			 */
			break;
		}
		
		if (errno != E2BIG) {
			errnosav = errno;
			
			w(g_warning ("g_mime_iconv_strndup: %s at byte %lu",
				     strerror (errno), n - inleft));
			
			g_free (out);
			
			/* reset the cd */
			iconv (cd, NULL, NULL, NULL, NULL);
			
			errno = errnosav;
			
			return NULL;
		}
		
		/*
		 * E2BIG   There is not sufficient room at *outbuf.
		 *
		 * We just need to grow our outbuffer and try again.
		 */
		
		converted = outbuf - out;
		outlen += inleft * 2 + 16;
		out = g_realloc (out, outlen + 4);
		outbuf = out + converted;
	} while (TRUE);
	
	/* flush the iconv conversion */
	while (iconv (cd, NULL, NULL, &outbuf, &outleft) == (size_t) -1) {
		if (errno != E2BIG)
			break;
		
		outlen += 16;
		converted = outbuf - out;
		out = g_realloc (out, outlen + 4);
		outleft = outlen - converted;
		outbuf = out + converted;
	}
	
	/* Note: not all charsets can be nul-terminated with a single
           nul byte. UCS2, for example, needs 2 nul bytes and UCS4
           needs 4. I hope that 4 nul bytes is enough to terminate all
           multibyte charsets? */
	
	/* nul-terminate the string */
	memset (outbuf, 0, 4);
	
	/* reset the cd */
	iconv (cd, NULL, NULL, NULL, NULL);
	
	return out;
}


/**
 * g_mime_iconv_strdup: (skip)
 * @cd: conversion descriptor
 * @str: string in source charset
 *
 * Allocates a new string buffer containing @str converted to the
 * destination charset described in @cd.
 *
 * Returns: a new string buffer containing the original string
 * converted to the new charset.
 **/
char *
g_mime_iconv_strdup (iconv_t cd, const char *str)
{
	return g_mime_iconv_strndup (cd, str, strlen (str));
}


/**
 * g_mime_iconv_locale_to_utf8:
 * @str: string in locale charset
 *
 * Allocates a new string buffer containing @str in UTF-8.
 *
 * Returns: a new string buffer containing @str converted to UTF-8.
 **/
char *
g_mime_iconv_locale_to_utf8 (const char *str)
{
	char *buf;
	
	LOCK ();
	buf = g_mime_iconv_strdup (locale_to_utf8, str);
	UNLOCK ();
	
	return buf;
}


/**
 * g_mime_iconv_locale_to_utf8_length:
 * @str: string in locale charset
 * @n: number of bytes to convert
 *
 * Allocates a new string buffer containing the first @n bytes of
 * @str converted to UTF-8.
 *
 * Returns: a new string buffer containing the first @n bytes of
 * @str converted to UTF-8.
 **/
char *
g_mime_iconv_locale_to_utf8_length (const char *str, size_t n)
{
	char *buf;
	
	LOCK ();
	buf = g_mime_iconv_strndup (locale_to_utf8, str, n);
	UNLOCK ();
	
	return buf;
}


/**
 * g_mime_iconv_utf8_to_locale:
 * @str: string in UTF-8 charset
 *
 * Allocates a new string buffer containing @str converted to the
 * user's locale charset.
 *
 * Returns: a new string buffer containing @str converted to the
 * user's locale charset.
 **/
char *
g_mime_iconv_utf8_to_locale (const char *str)
{
	char *buf;
	
	LOCK ();
	buf = g_mime_iconv_strdup (utf8_to_locale, str);
	UNLOCK ();
	
	return buf;
}


/**
 * g_mime_iconv_utf8_to_locale_length:
 * @str: string in UTF-8 charset
 * @n: number of bytes to convert
 *
 * Allocates a new string buffer containing the first @n bytes of
 * @str converted to the user's locale charset.
 *
 * Returns: a new string buffer containing the first @n bytes of
 * @str converted to the user's locale charset.
 **/
char *
g_mime_iconv_utf8_to_locale_length (const char *str, size_t n)
{
	char *buf;
	
	LOCK ();
	buf = g_mime_iconv_strndup (utf8_to_locale, str, n);
	UNLOCK ();
	
	return buf;
}
