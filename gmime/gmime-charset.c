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


#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <locale.h>

#ifdef HAVE_ALLOCA_H
#include <alloca.h>
#endif

#include "gmime-charset.h"
#include "strlib.h"


struct {
	char *charset;
	char *iconv_name;
} known_iconv_charsets[] = {
#if 0
	/* charset name, iconv-friendly charset name */
	{ "iso-8859-1",      "iso-8859-1" },
	{ "iso8859-1",       "iso-8859-1" },
	/* the above mostly serves as an example for iso-style charsets,
	   but we have code that will populate the iso-*'s if/when they
	   show up in g_mime_charset_name() so I'm
	   not going to bother putting them all in here... */
	{ "windows-cp1251",  "cp1251"     },
	{ "windows-1251",    "cp1251"     },
	{ "cp1251",          "cp1251"     },
	/* the above mostly serves as an example for windows-style
	   charsets, but we have code that will parse and convert them
	   to their cp#### equivalents if/when they show up in
	   g_mime_charset_name() so I'm not going to bother
	   putting them all in here either... */
#endif
	/* charset name, iconv-friendly name (sometimes case sensitive) */
	{ "utf-8",           "UTF-8"      },

	/* 10646 is a special case, its usually UCS-2 big endian */
	/* This might need some checking but should be ok for solaris/linux */
	{ "iso-10646-1",     "UCS-2BE"    },
	{ "iso_10646-1",     "UCS-2BE"    },
	{ "iso10646-1",      "UCS-2BE"    },
	{ "iso-10646",       "UCS-2BE"    },
	{ "iso_10646",       "UCS-2BE"    },
	{ "iso10646",        "UCS-2BE"    },

	{ "ks_c_5601-1987",  "EUC-KR"     },

	/* FIXME: Japanese/Korean/Chinese stuff needs checking */
	{ "euckr-0",         "EUC-KR"     },
	{ "big5-0",          "BIG5"       },
	{ "big5.eten-0",     "BIG5"       },
	{ "big5hkscs-0",     "BIG5HKCS"   },
	{ "gb2312-0",        "gb2312"     },
	{ "gb2312.1980-0",   "gb2312"     },
	{ "gb18030-0",       "gb18030"    },
	{ "gbk-0",           "GBK"        },

	{ "eucjp-0",         "eucJP"      },
	{ "ujis-0",          "ujis"       },
	{ "jisx0208.1983-0", "SJIS"       },
	{ "jisx0212.1990-0", "SJIS"       },
	{ NULL,              NULL         }
};

static GHashTable *iconv_charsets = NULL;
static char *locale_charset = NULL;


static void
iconv_charset_free (gpointer key, gpointer value, gpointer user_data)
{
	g_free (key);
	g_free (value);
}

static void
g_mime_charset_shutdown (void)
{
	g_hash_table_foreach (iconv_charsets, iconv_charset_free, NULL);
	g_hash_table_destroy (iconv_charsets);
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
	char *charset, *iconv_name, *locale;
	int i;
	
	if (iconv_charsets)
		return;
	
	iconv_charsets = g_hash_table_new (g_str_hash, g_str_equal);
	
	for (i = 0; known_iconv_charsets[i].charset != NULL; i++) {
		charset = g_strdup (known_iconv_charsets[i].charset);
		iconv_name = g_strdup (known_iconv_charsets[i].iconv_name);
		g_strdown (charset);
		g_hash_table_insert (iconv_charsets, charset, iconv_name);
	}
	
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
		char *codeset, *p;
		
		codeset = strchr (locale, '.');
		if (codeset) {
			codeset++;
			
			/* ; is a hack for debian systems and / is a hack for Solaris systems */
			for (p = codeset; *p && !strchr ("@;/", *p); p++);
			locale_charset = g_strndup (codeset, (unsigned) (p - codeset));
			g_strdown (locale_charset);
		} else {
			/* charset unknown */
			locale_charset = NULL;
		}
	}
	
	g_atexit (g_mime_charset_shutdown);
}


/**
 * g_mime_charset_locale_name:
 *
 * Gets the user's locale charset (or iso-8859-1 by default).
 *
 * Returns the user's locale charset (or iso-8859-1 by default).
 **/
const char *
g_mime_charset_locale_name (void)
{
	return locale_charset ? locale_charset : "iso-8859-1";
}


/**
 * g_mime_charset_name:
 * @charset: charset name
 *
 * Attempts to find an iconv-friendly charset name for @charset.
 *
 * Returns an iconv-friendly charset name for @charset.
 **/
const char *
g_mime_charset_name (const char *charset)
{
	char *name, *iconv_name, *buf;
	
	if (charset == NULL)
		return NULL;
	
	if (!iconv_charsets)
		return charset;
	
	name = alloca (strlen (charset) + 1);
	strcpy (name, charset);
	g_strdown (name);
	
	iconv_name = g_hash_table_lookup (iconv_charsets, name);
	if (iconv_name)
		return iconv_name;
	
	if (!strncmp (name, "iso", 3)) {
		buf = name + 3;
		if (*buf == '-' || *buf == '_')
			buf++;
		
		iconv_name = g_strdup_printf ("ISO-%s", buf);
	} else if (!strncmp (name, "windows-", 8)) {
		buf = name + 8;
		if (!strncmp (buf, "cp", 2))
			buf += 2;
		
		iconv_name = g_strdup_printf ("CP%s", buf);
	} else if (!strncmp (name, "microsoft-", 10)) {
		buf = name + 10;
		if (!strncmp (buf, "cp", 2))
			buf += 2;
		
		iconv_name = g_strdup_printf ("CP%s", buf);
	} else {
		/* assume charset name is ok as is? */
		iconv_name = g_strdup (charset);
	}
	
	g_hash_table_insert (iconv_charsets, g_strdup (name), iconv_name);
	
	return iconv_name;
}
