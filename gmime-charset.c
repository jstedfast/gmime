/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 *  Authors: Jeffrey Stedfast <fejj@ximian.com>
 *
 *  Copyright 2001 Ximian, Inc. (www.ximian.com)
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


#include "gmime-charset.h"

#include <config.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <locale.h>

static char *locale_charset = NULL;


static void
g_mime_charset_shutdown (void)
{
	g_free (locale_charset);
}


/**
 * g_mime_charset_init:
 *
 * Initializes the locale charset variable for later calls to
 * gmime_charset_locale_name. Only really needs to be called for non-
 * iso-8859-1 locales.
 **/
void
g_mime_charset_init (void)
{
	char *locale;
	
	g_free (locale_charset);
	
	locale = setlocale (LC_ALL, NULL);
	
	if (!locale || !strcmp (locale, "C") || !strcmp (locale, "POSIX")) {
		/* The locale "C"  or  "POSIX"  is  a  portable  locale;  its
		 * LC_CTYPE  part  corresponds  to  the 7-bit ASCII character
		 * set.  */
		
		locale_charset = NULL;
	} else {
		/* A locale name is typically of  the  form  language[_terri-
		 * tory][.codeset][@modifier],  where  language is an ISO 639
		 * language code, territory is an ISO 3166 country code,  and
		 * codeset  is  a  character  set or encoding identifier like
		 * ISO-8859-1 or UTF-8.
		 */
		char *p;
		int len;
		
		p = strchr (locale, '@');
		len = p ? (p - locale) : strlen (locale);
		if ((p = strchr (locale, '.'))) {
			locale_charset = g_strndup (p + 1, len - (p - locale) + 1);
			g_strdown (locale_charset);
		}
	}
	
	g_atexit (g_mime_charset_shutdown);
}


/**
 * g_mime_charset_locale_name:
 *
 * Returns the user's locale charset (or iso-8859-1 by default).
 **/
const char *
g_mime_charset_locale_name (void)
{
	return locale_charset ? locale_charset : "iso-8859-1";
}
