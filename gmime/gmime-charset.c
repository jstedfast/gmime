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

#ifdef HAVE_CODESET
#include <langinfo.h>
#endif

#include "gmime-charset-map-private.h"
#include "gmime-charset.h"

#ifdef HAVE_ICONV_DETECT_H
#include "iconv-detect.h"
#else /* use old-style detection */
#if defined (__aix__) || defined (__irix__) || defined (__sun__)
#define ICONV_ISO_D_FORMAT "ISO%d-%d"
/* this one is for charsets like ISO-2022-JP, for which at least
   Solaris wants a - after the ISO */
#define ICONV_ISO_S_FORMAT "ISO-%d-%s"
#elif defined (__hpux__)
#define ICONV_ISO_D_FORMAT "iso%d%d"
#define ICONV_ISO_S_FORMAT "iso%d%s"
#else
#define ICONV_ISO_D_FORMAT "iso-%d-%d"
#define ICONV_ISO_S_FORMAT "iso-%d-%s"
#endif /* __aix__, __irix__, __sun__ */
#define ICONV_10646 "iso-10646"
#endif /* USE_ICONV_DETECT */


/* a useful website on charset alaises:
 * http://www.li18nux.org/subgroups/sa/locnameguide/v1.1draft/CodesetAliasTable-V11.html */

struct {
	char *charset;
	char *iconv_name;
} known_iconv_charsets[] = {
	/* charset name, iconv-friendly name (sometimes case sensitive) */
	{ "utf-8",           "UTF-8"      },
	{ "utf8",            "UTF-8"      },
	
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
	{ "5601",            "EUC-KR"     },
	{ "ksc-5601",        "EUC-KR"     },
	{ "ksc-5601-1987",   "EUC-KR"     },
	{ "ksc-5601_1987",   "EUC-KR"     },
	
	/* FIXME: Japanese/Korean/Chinese stuff needs checking */
	{ "euckr-0",         "EUC-KR"     },
	{ "5601",            "EUC-KR"     },
	{ "big5-0",          "BIG5"       },
	{ "big5.eten-0",     "BIG5"       },
	{ "big5hkscs-0",     "BIG5HKCS"   },
	{ "gb2312-0",        "gb2312"     },
	{ "gb2312.1980-0",   "gb2312"     },
	{ "euc-cn",          "gb2312"     },
	{ "gb18030-0",       "gb18030"    },
	{ "gbk-0",           "GBK"        },
	
	{ "eucjp-0",         "eucJP"  	  },  /* should this map to "EUC-JP" instead? */
	{ "ujis-0",          "ujis"  	  },  /* we might want to map this to EUC-JP */
	{ "jisx0208.1983-0", "SJIS"       },
	{ "jisx0212.1990-0", "SJIS"       },
	{ "pck",	     "SJIS"       },
	{ NULL,              NULL         }
};

/* map CJKR charsets to their language code */
/* NOTE: only support charset names that will be returned by
 * g_mime_charset_iconv_name() so that we don't have to keep track of
 * all the aliases too. */
static struct {
	char *charset;
	char *lang;
} cjkr_lang_map[] = {
	{ "Big5",        "zh" },
	{ "BIG5HKCS",    "zh" },
	{ "gb2312",      "zh" },
	{ "gb18030",     "zh" },
	{ "gbk",         "zh" },
	{ "euc-tw",      "zh" },
	{ "iso-2022-jp", "ja" },
	{ "sjis",        "ja" },
	{ "ujis",        "ja" },
	{ "eucJP",       "ja" },
	{ "euc-jp",      "ja" },
	{ "euc-kr",      "ko" },
	{ "koi8-r",      "ru" },
	{ "koi8-u",      "uk" }
};

#define NUM_CJKR_LANGS (sizeof (cjkr_lang_map) / sizeof (cjkr_lang_map[0]))


static GHashTable *iconv_charsets = NULL;
static char *locale_charset = NULL;
static char *locale_lang = NULL;

#ifdef G_THREADS_ENABLED
static GStaticMutex charset_lock = G_STATIC_MUTEX_INIT;
#define CHARSET_LOCK()   g_static_mutex_lock (&charset_lock);
#define CHARSET_UNLOCK() g_static_mutex_unlock (&charset_lock);
#else
#define CHARSET_LOCK()
#define CHARSET_UNLOCK()
#endif /* G_THREADS_ENABLED */


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
	g_free (locale_lang);
}


static void
locale_parse_lang (const char *locale)
{
	char *codeset, *lang;
	
	if ((codeset = strchr (locale, '.')))
		lang = g_strndup (locale, codeset - locale);
	else
		lang = g_strdup (locale);
	
	/* validate the language */
	if (strlen (lang) >= 2) {
		if (lang[2] == '-' || lang[2] == '_') {
			/* canonicalise the lang */
			g_ascii_strdown (lang, 2);
			
			/* validate the country code */
			if (strlen (lang + 3) > 2) {
				/* invalid country code */
				lang[2] = '\0';
			} else {
				lang[2] = '-';
				g_ascii_strup (lang + 3, 2);
			}
		} else if (lang[2] != '\0') {
			/* invalid language */
			g_free (lang);
			lang = NULL;
		}
		
		locale_lang = lang;
	} else {
		/* invalid language */
		locale_lang = NULL;
		g_free (lang);
	}
}


/**
 * g_mime_charset_map_init:
 *
 * Initializes the locale charset variable for later calls to
 * #g_mime_locale_charset(). Only really needs to be called for non-
 * iso-8859-1 locales.
 **/
void
g_mime_charset_map_init (void)
{
	char *charset, *iconv_name, *locale;
	int i;
	
	if (iconv_charsets)
		return;
	
	iconv_charsets = g_hash_table_new (g_str_hash, g_str_equal);
	
	for (i = 0; known_iconv_charsets[i].charset != NULL; i++) {
		charset = g_strdup (known_iconv_charsets[i].charset);
		iconv_name = g_strdup (known_iconv_charsets[i].iconv_name);
		g_ascii_strdown (charset, -1);
		g_hash_table_insert (iconv_charsets, charset, iconv_name);
	}
	
#ifdef HAVE_CODESET
	locale_charset = g_strdup (nl_langinfo (CODESET));
	g_ascii_strdown (locale_charset, -1);
#else
	locale = setlocale (LC_ALL, NULL);
	
	if (!locale || !strcmp (locale, "C") || !strcmp (locale, "POSIX")) {
		/* The locale "C"  or  "POSIX"  is  a  portable  locale;  its
		 * LC_CTYPE  part  corresponds  to  the 7-bit ASCII character
		 * set.  */
		
		locale_charset = NULL;
		locale_lang = NULL;
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
			g_ascii_strdown (locale_charset, -1);
		} else {
			/* charset unknown */
			locale_charset = NULL;
		}
		
		locale_parse_lang (locale);
	}
#endif
	
	g_atexit (g_mime_charset_shutdown);
}


/**
 * g_mime_locale_charset:
 *
 * Gets the user's locale charset (or iso-8859-1 by default).
 *
 * Returns the user's locale charset (or iso-8859-1 by default).
 **/
const char *
g_mime_locale_charset (void)
{
	CHARSET_LOCK ();
	if (!iconv_charsets)
		g_mime_charset_map_init ();
	CHARSET_UNLOCK ();
	
	return locale_charset ? locale_charset : "iso-8859-1";
}


/**
 * g_mime_locale_language:
 *
 * Gets the user's locale language code (or %NULL by default).
 *
 * Returns the user's locale language code (or %NULL by default).
 **/
const char *
g_mime_locale_language (void)
{
	CHARSET_LOCK ();
	if (!iconv_charsets)
		g_mime_charset_map_init ();
	CHARSET_UNLOCK ();
	
	return locale_lang;
}


/**
 * g_mime_charset_language:
 * @charset: charset name
 *
 * Attempts to find a specific language code that is specific to
 * @charset. Currently only handles CJK and Russian/Ukranian
 * charset->lang mapping. Everything else will return %NULL.
 *
 * Returns a language code that is specific to @charset, or NULL on
 * fail.
 **/
const char *
g_mime_charset_language (const char *charset)
{
	int i;
	
	if (!charset)
		return NULL;
	
	for (i = 0; i < NUM_CJKR_LANGS; i++) {
		if (!strcasecmp (cjkr_lang_map[i].charset, charset))
			return cjkr_lang_map[i].lang;
	}
	
	return NULL;
}


static const char *
strdown (char *str)
{
	register char *s = str;
	
	while (*s) {
		if (*s >= 'A' && *s <= 'Z')
			*s += 0x20;
		s++;
	}
	
	return str;
}

/**
 * g_mime_charset_iconv_name:
 * @charset: charset name
 *
 * Attempts to find an iconv-friendly charset name for @charset.
 *
 * Returns an iconv-friendly charset name for @charset.
 **/
const char *
g_mime_charset_iconv_name (const char *charset)
{
	char *name, *iconv_name, *buf;
	
	if (charset == NULL)
		return NULL;
	
	name = g_alloca (strlen (charset) + 1);
	strcpy (name, charset);
	strdown (name);
	
	CHARSET_LOCK ();
	if (!iconv_charsets)
		g_mime_charset_map_init ();
	
	iconv_name = g_hash_table_lookup (iconv_charsets, name);
	if (iconv_name) {
		CHARSET_UNLOCK ();
		return iconv_name;
	}
	
	if (!strncmp (name, "iso", 3)) {
		int iso, codepage;
		char *p;
		
		buf = name + 3;
		if (*buf == '-' || *buf == '_')
			buf++;
		
		iso = strtoul (buf, &p, 10);
		
		if (iso == 10646) {
			/* they all become ICONV_10646 */
			iconv_name = g_strdup (ICONV_10646);
		} else if (p > buf) {
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
					iconv_name = g_strdup_printf (ICONV_ISO_D_FORMAT,
								      iso, codepage);
			} else {
				/* codepage is a string - probably iso-2022-jp or something */
				iconv_name = g_strdup_printf (ICONV_ISO_S_FORMAT,
							      iso, p);
			}
		} else {
			/* p == buf, which probably means we've
			   encountered an invalid iso charset name */
			iconv_name = g_strdup (name);
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
	
	CHARSET_UNLOCK ();
	
	return iconv_name;
}


static const char *iso_charsets[] = {
	"us-ascii",
	"iso-8859-1",
	"iso-8859-2",
	"iso-8859-3",
	"iso-8859-4",
	"iso-8859-5",
	"iso-8859-6",
	"iso-8859-7",
	"iso-8859-8",
	"iso-8859-9",
	"iso-8859-10",
	"iso-8859-11",
	"iso-8859-12",
	"iso-8859-13",
	"iso-8859-14",
	"iso-8859-15",
	"iso-8859-16"
};

#define NUM_ISO_CHARSETS (sizeof (iso_charsets) / sizeof (iso_charsets[0]))


/**
 * g_mime_charset_canon_name:
 * @charset: charset name
 *
 * Attempts to find a canonical charset name for @charset.
 *
 * Note: Will normally return the same value as
 * #g_mime_charset_iconv_name() unless the system iconv does not use
 * the canonical ISO charset names (such as using ISO8859-1 rather
 * than the canonical form ISO-8859-1).
 *
 * Returns a canonical charset name for @charset.
 **/
const char *
g_mime_charset_canon_name (const char *charset)
{
	const char *ptr;
	char *endptr;
	int iso;
	
	if (!charset)
		return NULL;
	
	charset = g_mime_charset_iconv_name (charset);
	if (strncasecmp (charset, "iso", 3) != 0)
		return charset;
	
	ptr = charset + 3;
	if (*ptr == '-' || *ptr == '_')
		ptr++;
	
	if (strncmp (ptr, "8859", 4) != 0)
		return charset;
	
	ptr += 4;
	if (*ptr == '-' || *ptr == '_')
		ptr++;
	
	iso = strtoul (ptr, &endptr, 10);
	if (endptr == ptr || *endptr != '\0')
		return charset;
	
	if (iso >= NUM_ISO_CHARSETS)
		return charset;
	
	return iso_charsets[iso];
}


/**
 * g_mime_charset_init:
 * @charset: charset mask
 *
 * Initializes a charset mask structure.
 **/
void
g_mime_charset_init (GMimeCharset *charset)
{
	charset->mask = (unsigned int) ~0;
	charset->level = 0;
}


/**
 * g_mime_charset_step:
 * @charset:
 * @in: input text buffer (must be in UTF-8)
 * @len: input buffer length
 *
 * Steps through the input buffer 1 unicode character (glyph) at a
 * time (ie, not necessarily 1 byte at a time). Bitwise 'and' our
 * @charset->mask with the mask for each glyph. This has the effect of
 * limiting what charsets our @charset->mask can match.
 **/
void
g_mime_charset_step (GMimeCharset *charset, const char *in, size_t len)
{
	register const char *inptr = in;
	const char *inend = in + len;
	register unsigned int mask;
	register int level;
	
	mask = charset->mask;
	level = charset->level;
	
	while (inptr < inend) {
		const char *newinptr;
		gunichar c;
		
		newinptr = g_utf8_next_char (inptr);
		c = g_utf8_get_char (inptr);
		if (newinptr == NULL || !g_unichar_validate (c)) {
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


/**
 * g_mime_charset_best_name:
 * @charset: charset mask
 *
 * Gets the best charset name based on the charset mask @charset.
 *
 * Returns a pointer to a string containing the best charset name that
 * can represent the charset mask @charset.
 **/
const char *
g_mime_charset_best_name (GMimeCharset *charset)
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
	GMimeCharset charset;
	
	g_mime_charset_init (&charset);
	g_mime_charset_step (&charset, in, inlen);
	
	return g_mime_charset_best_name (&charset);
}


#ifdef BUILD_CHARSET_MAP

#include <sys/stat.h>
#include <errno.h>
#include <iconv.h>

static struct {
	char *name;        /* charset name */
	int multibyte;     /* charset type */
	unsigned int bit;  /* assigned bit */
} tables[] = {
	/* These are the 8bit character sets (other than iso-8859-1,
	 * which is special-cased) which are supported by both other
	 * mailers and the GNOME environment. Note that the order
	 * they're listed in is the order they'll be tried in, so put
	 * the more-popular ones first.
	 */
	{ "iso-8859-2",   0, 0 },  /* Central/Eastern European */
	{ "iso-8859-4",   0, 0 },  /* Baltic */
	{ "koi8-r",       0, 0 },  /* Russian */
	{ "koi8-u",       0, 0 },  /* Ukranian */
	{ "iso-8859-5",   0, 0 },  /* Least-popular Russian encoding */
	{ "iso-8859-7",   0, 0 },  /* Greek */
	{ "iso-8859-8",   0, 0 },  /* Hebrew; Visual */
	{ "iso-8859-9",   0, 0 },  /* Turkish */
	{ "iso-8859-13",  0, 0 },  /* Baltic again */
	{ "iso-8859-15",  0, 0 },  /* New-and-improved iso-8859-1, but most
				    * programs that support this support UTF8
				    */
	{ "windows-1251", 0, 0 },  /* Russian */
	
	/* These are the multibyte character sets which are commonly
	 * supported by other mail clients. Note: order for multibyte
	 * charsets does not affect priority unlike the 8bit charsets
	 * listed above.
	 */
	{ "iso-2022-jp",  1, 0 },  /* Japanese designed for use over the Net */
	{ "Shift-JIS",    1, 0 },  /* Japanese as used by Windows and MacOS systems */
	{ "euc-jp",       1, 0 },  /* Japanese traditionally used on Unix systems */
	{ "euc-kr",       1, 0 },  /* Korean */
	{ "iso-2022-kr",  1, 0 },  /* Korean (less popular than euc-kr) */
	{ "gb2312",       1, 0 },  /* Simplified Chinese */
	{ "Big5",         1, 0 },  /* Traditional Chinese */
	{ "euc-tw",       1, 0 },
	{ NULL,           0, 0 }
};

unsigned int encoding_map[256 * 256];

#if G_BYTE_ORDER == G_BIG_ENDIAN
#define UCS "UCS-4BE"
#else
#define UCS "UCS-4LE"
#endif

int main (int argc, char **argv)
{
	GHashTable *table_hash;
	size_t inleft, outleft;
	char *inbuf, *outbuf;
	guint32 out[128], c;
	unsigned int bit = 0x01;
	char in[128];
	int i, j, k;
	int bytes;
	iconv_t cd;
	
	/* dont count the terminator */
	bytes = ((sizeof (tables) / sizeof (tables[0])) + 7 - 1) / 8;
	
	for (i = 0; i < 128; i++)
		in[i] = i + 128;
	
	for (j = 0; tables[j].name && !tables[j].multibyte; j++) {
		cd = iconv_open (UCS, tables[j].name);
		inbuf = in;
		outbuf = (char *)(out);
		inleft = sizeof (in);
		outleft = sizeof (out);
		while (iconv (cd, &inbuf, &inleft, &outbuf, &outleft) == -1) {
			if (errno == EILSEQ) {
				inbuf++;
				inleft--;
			} else {
				g_warning ("iconv (%s->UCS4, ..., %d, ..., %d): %s",
					   tables[j].name, inleft, outleft,
					   g_strerror (errno));
				exit (1);
			}
		}
		iconv_close (cd);
		
		for (i = 0; i < 128 - outleft / 4; i++) {
			encoding_map[i] |= bit;
			encoding_map[out[i]] |= bit;
		}
		
		tables[j].bit = bit;
		bit <<= 1;
	}
	
	/* Mutibyte tables */
	for ( ; tables[j].name && tables[j].multibyte; j++) {
		cd = iconv_open (tables[j].name, UCS);
		if (cd == (iconv_t) -1)
			continue;
		
		for (c = 128, i = 0; c < 65535 && i < 65535; c++) {
			inbuf = (char *) &c;
			inleft = sizeof (c);
			outbuf = in;
			outleft = sizeof (in);
			
			if (iconv (cd, &inbuf, &inleft, &outbuf, &outleft) != (size_t) -1) {
				/* this is a legal character in charset table[j].name */
				iconv (cd, NULL, NULL, &outbuf, &outleft);
				encoding_map[i++] |= bit;
				encoding_map[c] |= bit;
			} else {
				/* reset the iconv descriptor */
				iconv (cd, NULL, NULL, NULL, NULL);
			}
		}
		
		iconv_close (cd);
		
		tables[j].bit = bit;
		bit <<= 1;
	}
	
	printf ("/* This file is automatically generated: DO NOT EDIT */\n\n");
	
	/* FIXME: we can condense better than what my quick hack does,
	   but it'd be more work and I'm not sure if it's worth it or
	   not. Currently I'm just making it so that tables that
	   contain all of the same values will only ever be
	   one-of-a-kind by making duplicates into macro aliases for
	   the original */
	
	table_hash = g_hash_table_new (g_int_hash, g_int_equal);
	
	for (i = 0; i < 256; i++) {
		/* first, do we need this block? */
		for (k = 0; k < bytes; k++) {
			int first = encoding_map[i * 256] & (0xff << (k * 8));
			int same = TRUE;
			int dump = FALSE;
			
			for (j = 0; j < 256; j++) {
				same = same && (encoding_map[i * 256 + j] & (0xff << (k * 8))) == first;
				if ((encoding_map[i * 256 + j] & (0xff << (k * 8))) != 0)
					dump = TRUE;
			}
			
			if (dump) {
				if (same) {
					/* this table is aliasable */
					char *table_name;
					
					if ((table_name = g_hash_table_lookup (table_hash, &first))) {
						/* we've already written out a table with the exact same
						   values so we can just alias it with a macro. */
						printf ("#define m%02x%x %s\n\n", i, k, table_name);
						continue;
					} else {
						table_name = g_strdup_printf ("m%02x%x", i, k);
						g_hash_table_insert (table_hash, &first, table_name);
					}
				}
				
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
