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

#include "gmime-charset-map-private.h"
#include "gmime-charset.h"
#include "strlib.h"


#if defined (__aix__) || defined (__irix__) || defined (__sun__)
#define CANONICAL_ISO_D_FORMAT "ISO%d-%d"
/* this one is for charsets like ISO-2022-JP, for which at least
   Solaris wants a - after the ISO */
#define CANONICAL_ISO_S_FORMAT "ISO-%d-%s"
#elif defined (__hpux__)
#define CANONICAL_ISO_D_FORMAT "iso%d%d"
#define CANONICAL_ISO_S_FORMAT "iso%d%s"
#else
#define CANONICAL_ISO_D_FORMAT "iso-%d-%d"
#define CANONICAL_ISO_S_FORMAT "iso-%d-%s"
#endif /* __aix__, __irix__, __sun__ */


static struct {
	char *name;
	unsigned int bit;	/* assigned bit */
} tables[] = {
	/* These are the 8bit character sets (other than iso-8859-1,
	 * which is special-cased) which are supported by both other
	 * mailers and the GNOME environment. Note that the order
	 * they're listed in is the order they'll be tried in, so put
	 * the more-popular ones first.
	 */
	{ "iso-8859-2", 0 },	/* Central/Eastern European */
	{ "iso-8859-4", 0 },	/* Baltic */
	{ "koi8-r", 0 },	/* Russian */
	{ "koi8-u", 0 },	/* Ukranian */
	{ "iso-8859-5", 0 },	/* Least-popular Russian encoding */
	{ "iso-8859-7", 0 },	/* Greek */
	{ "iso-8859-8", 0 },    /* Hebrew; Visual */
	{ "iso-8859-9", 0 },	/* Turkish */
	{ "iso-8859-13", 0 },	/* Baltic again */
	{ "iso-8859-15", 0 },	/* New-and-improved iso-8859-1, but most
				 * programs that support this support UTF8
				 */
	{ "windows-1251", 0 },	/* Russian */
	{ 0, 0 }
};

unsigned int encoding_map[256 * 256];

#if G_BYTE_ORDER == G_BIG_ENDIAN
#define UCS "UCS-4BE"
#else
#define UCS "UCS-4LE"
#endif


struct {
	char *charset;
	char *iconv_name;
} known_iconv_charsets[] = {
	/* charset name, iconv-friendly name (sometimes case sensitive) */
	{ "utf-8",           "UTF-8"      },
	
	/* ANSI_X3.4-1968 is used on some systems and should be
           treated the same as US-ASCII */
	{ "ANSI_X3.4-1968",  NULL         },
	
	/* 10646 is a special case, its usually UCS-2 big endian */
	/* This might need some checking but should be ok for
           solaris/linux */
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
		int iso, codepage;
		char *p;
		
		buf = name + 3;
		if (*buf == '-' || *buf == '_')
			buf++;
		
		iso = strtoul (buf, &p, 10);
		
		g_assert (p > buf);
		
		buf = p;
		if (*buf == '-' || *buf == '_')
			buf++;
		
		codepage = strtoul (buf, &p, 10);
		
		if (p > buf) {
			/* codepage is numeric */
#ifdef __aix__
			if (codepage == 13)
				iconv_name = g_strdup ("IBM-921");
			else
#endif /* __aix__ */
				iconv_name = g_strdup_printf (CANONICAL_ISO_D_FORMAT,
							      iso, codepage);
		} else {
			/* codepage is a string? */
			iconv_name = g_strdup_printf (CANONICAL_ISO_S_FORMAT,
						      iso, p);
		}
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



/* unicode_* and unichar are stolen from glib2... */
typedef guint32 unichar;

static const char unicode_skip[256] = {
  1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
  1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
  1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
  1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
  1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
  1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
  2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,
  3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,4,4,4,4,4,4,4,4,5,5,5,5,6,6,1,1
};

#define UTF8_COMPUTE(ch, mask, len)					      \
  if (ch < 128) 							      \
    {									      \
      len = 1;								      \
      mask = 0x7f;							      \
    }									      \
  else if ((ch & 0xe0) == 0xc0) 					      \
    {									      \
      len = 2;								      \
      mask = 0x1f;							      \
    }									      \
  else if ((ch & 0xf0) == 0xe0) 					      \
    {									      \
      len = 3;								      \
      mask = 0x0f;							      \
    }									      \
  else if ((ch & 0xf8) == 0xf0) 	 				      \
    {									      \
      len = 4;								      \
      mask = 0x07;							      \
    }									      \
  else if ((ch & 0xfc) == 0xf8) 					      \
    {									      \
      len = 5;								      \
      mask = 0x03;							      \
    }									      \
  else if ((ch & 0xfe) == 0xfc) 					      \
    {									      \
      len = 6;								      \
      mask = 0x01;							      \
    }									      \
  else									      \
    len = -1;

#define UTF8_GET(result, chars, count, mask, len)			      \
  (result) = (chars)[0] & (mask);					      \
  for ((count) = 1; (count) < (len); ++(count))				      \
    {									      \
      if (((chars)[(count)] & 0xc0) != 0x80)				      \
	{								      \
	  (result) = -1;						      \
	  break;							      \
	}								      \
      (result) <<= 6;							      \
      (result) |= ((chars)[(count)] & 0x3f);				      \
    }

#define UNICODE_VALID(ch)                   \
    ((ch) < 0x110000 &&                     \
     ((ch) < 0xD800 || (ch) >= 0xE000) &&   \
     (ch) != 0xFFFE && (ch) != 0xFFFF)



#define unicode_next_char(p) (char *)((p) + unicode_skip[*(unsigned char *)(p)])

unichar
unicode_get_char (const char  *p)
{
	unsigned char c = (unsigned char) *p;
	int i, mask = 0, len;
	unichar result;
	
	UTF8_COMPUTE (c, mask, len);
	if (len == -1)
		return (unichar) -1;
	UTF8_GET (result, p, i, mask, len);
	
	return result;
}

gboolean
unichar_validate (unichar ch)
{
	return UNICODE_VALID (ch);
}



typedef struct _Charset {
	unsigned int mask;
	unsigned int level;
} Charset;

static void
charset_init (Charset *charset)
{
	charset->mask = ~0;
	charset->level = 0;
}

static void
charset_step (Charset *charset, const char *in, size_t len)
{
	register const char *inptr = in;
	const char *inend = in + len;
	register unsigned int mask;
	register int level;
	
	mask = charset->mask;
	level = charset->level;
	
	while (inptr < inend) {
		const char *newinptr;
		unichar c;
		
		newinptr = unicode_next_char (inptr);
		c = unicode_get_char (inptr);
		if (newinptr == NULL || !unichar_validate (c)) {
			inptr++;
			continue;
		}
		
		inptr = newinptr;
		if (c <= 0xffff) {
			mask &= charset_mask (c);
			
			if (c >= 128 && c < 256)
				level = MAX (level, 1);
			else if (c >= 256)
				level = MAX (level, 2);
		} else {
			mask = 0;
			level = MAX (level, 2);
		}
	}
	
	charset->mask = mask;
	charset->level = level;
}

static const char *
charset_best_mask (unsigned int mask)
{
	int i;
	
	for (i = 0; i < sizeof (charinfo) / sizeof (charinfo[0]); i++) {
		if (charinfo[i].bit & mask)
			return charinfo[i].name;
	}
	
	return "UTF-8";
}

static const char *
charset_best_name (Charset *charset)
{
	if (charset->level == 1)
		return "iso-8859-1";
	else if (charset->level == 2)
		return charset_best_mask (charset->mask);
	else
		return NULL;
}


/**
 * g_mime_charset_best:
 * @in: a UTF-8 text buffer
 * @inlen: length of @in
 *
 * Computes the best charset to use to encode this text buffer.
 *
 * Returns the charset name best suited for the input text or %NULL if
 * it is US-ASCII safe.
 **/
const char *
g_mime_charset_best (const char *in, size_t inlen)
{
	Charset charset;
	
	charset_init (&charset);
	charset_step (&charset, in, inlen);
	return charset_best_name (&charset);
}


#ifdef BUILD_CHARSET_MAP

#include <errno.h>
#include <iconv.h>

/* the following functions are copied from glib2... */


int main (int argc, char **argv)
{
	char *inptr, *outptr;
	size_t inlen, outlen;
	guint32 out[128];
	char in[128];
	int i, j, k;
	int max, min;
	int bit = 0x01;
	int bytes;
	iconv_t cd;
	
	/* dont count the terminator */
	bytes = ((sizeof (tables) / sizeof (tables[0])) + 7 - 1) / 8;
	
	for (i = 0; i < 128; i++)
		in[i] = i + 128;
	
	for (j = 0; tables[j].name; j++) {
		cd = iconv_open (UCS, tables[j].name);
		inptr = in;
		outptr = (char *)(out);
		inlen = sizeof (in);
		outlen = sizeof (out);
		while (iconv (cd, &inptr, &inlen, &outptr, &outlen) == -1) {
			if (errno == EILSEQ) {
				inptr++;
				inlen--;
			} else {
				g_warning ("%s\n", g_strerror (errno));
				exit (1);
			}
		}
		iconv_close (cd);
		
		for (i = 0; i < 128 - outlen / 4; i++) {
			encoding_map[i] |= bit;
			encoding_map[out[i]] |= bit;
		}
		
		tables[j].bit = bit;
		bit <<= 1;
	}
	
	printf ("/* This file is automatically generated: DO NOT EDIT */\n\n");
	
	for (i = 0; i < 256; i++) {
		/* first, do we need this block? */
		for (k = 0; k < bytes; k++) {
			for (j = 0; j < 256; j++) {
				if ((encoding_map[i * 256 + j] & (0xff << (k * 8))) != 0)
					break;
			}
			if (j < 256) {
				/* yes, dump it */
				printf ("static unsigned char m%02x%x[256] = {\n\t", i, k);
				for (j = 0; j < 256; j++) {
					printf ("0x%02x, ", (encoding_map[i * 256 + j] >> (k * 8)) & 0xff);
					if (((j + 1) & 7) == 0 && j < 255)
						printf ("\n\t");
				}
				printf ("\n};\n\n");
			}
		}
	}
	
	printf ("struct {\n");
	for (k = 0; k < bytes; k++) {
		printf ("\tunsigned char *bits%d;\n", k);
	}
	
	printf ("} charmap[256] = {\n\t");
	for (i = 0; i < 256; i++) {
		/* first, do we need this block? */
		printf ("{ ");
		for (k = 0; k < bytes; k++) {
			for (j = 0; j < 256; j++) {
				if ((encoding_map[i * 256 + j] & (0xff << (k * 8))) != 0)
					break;
			}
			
			if (j < 256) {
				printf ("m%02x%x, ", i, k);
			} else {
				printf ("0, ");
			}
		}
		
		printf ("}, ");
		if (((i + 1) & 7) == 0 && i < 255)
			printf ("\n\t");
	}
	printf ("\n};\n\n");
	
	printf ("struct {\n\tconst char *name;\n\tunsigned int bit;\n} charinfo[] = {\n");
	for (j = 0; tables[j].name; j++) {
		printf ("\t{ \"%s\", 0x%04x },\n", tables[j].name, tables[j].bit);
	}
	printf ("};\n\n");
	
	printf("#define charset_mask(x) \\\n");
	for (k = 0; k < bytes; k++) {
		if (k != 0)
			printf ("\t| ");
		else
			printf ("\t");
		
		printf ("(charmap[(x) >> 8].bits%d ? charmap[(x) >> 8].bits%d[(x) & 0xff] << %d : 0)",
			k, k, k * 8);
		
		if (k < bytes - 1)
			printf ("\t\\\n");
	}
	printf ("\n\n");
	
	return 0;
}
#endif /* BUILD_CHARSET_MAP */
