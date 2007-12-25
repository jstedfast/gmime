/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*  GMime
 *  Copyright (C) 2000-2007 Jeffrey Stedfast
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
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <locale.h>
#include <errno.h>

#ifdef HAVE_CODESET
#include <langinfo.h>
#endif

#include "gmime-charset-map-private.h"
#include "gmime-table-private.h"
#include "gmime-charset.h"
#include "gmime-iconv.h"

#ifdef HAVE_ICONV_DETECT_H
#include "iconv-detect.h"
#else /* use old-style detection */
#if defined (__aix__) || defined (__irix__) || defined (__sun__)
#define ICONV_ISO_INT_FORMAT "ISO%u-%u"
/* this one is for charsets like ISO-2022-JP, for which at least
   Solaris wants a - after the ISO */
#define ICONV_ISO_STR_FORMAT "ISO-%u-%s"
#elif defined (__hpux__)
#define ICONV_ISO_INT_FORMAT "iso%u%u"
#define ICONV_ISO_STR_FORMAT "iso%u%s"
#else
#define ICONV_ISO_INT_FORMAT "iso-%u-%u"
#define ICONV_ISO_STR_FORMAT "iso-%u-%s"
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
	
	/* Korean charsets */
	/* Note: according to http://www.iana.org/assignments/character-sets,
	 * ks_c_5601-1987 should really map to ISO-2022-KR, but the EUC-KR
	 * mapping was given to me via a native Korean user, so I'm not sure
	 * if I should change this... perhaps they are compatable? */
	{ "ks_c_5601-1987",  "EUC-KR"     },
	{ "5601",            "EUC-KR"     },
	{ "ksc-5601",        "EUC-KR"     },
	{ "ksc-5601-1987",   "EUC-KR"     },
	{ "ksc-5601_1987",   "EUC-KR"     },
	{ "ks_c_5861-1992",  "EUC-KR"     },
	{ "euckr-0",         "EUC-KR"     },
	
	/* Chinese charsets */
	{ "big5-0",          "BIG5"       },
	{ "big5.eten-0",     "BIG5"       },
	{ "big5hkscs-0",     "BIG5HKSCS"  },
	/* Note: GBK is a superset of gb2312, see
	 * http://en.wikipedia.org/wiki/GBK for details */
	{ "gb2312",          "GBK"        },
	{ "gb-2312",         "GBK"        },
	{ "gb2312-0",        "GBK"        },
	{ "gb2312-80",       "GBK"        },
	{ "gb2312.1980-0",   "GBK"        },
	/* euc-cn is an alias for gb2312 */
	{ "euc-cn",          "GBK"        },
	{ "gb18030-0",       "gb18030"    },
	{ "gbk-0",           "GBK"        },
	
	/* Japanese charsets */
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
	{ "BIG5HKSCS",   "zh" },
	{ "gb2312",      "zh" },
	{ "gb18030",     "zh" },
	{ "gbk",         "zh" },
	{ "euc-tw",      "zh" },
	{ "iso-2022-jp", "ja" },
	{ "Shift-JIS",   "ja" },
	{ "sjis",        "ja" },
	{ "ujis",        "ja" },
	{ "eucJP",       "ja" },
	{ "euc-jp",      "ja" },
	{ "euc-kr",      "ko" },
	{ "koi8-r",      "ru" },
	{ "koi8-u",      "uk" }
};

static GHashTable *iconv_charsets = NULL;
static char **user_charsets = NULL;
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


/**
 * g_mime_charset_map_shutdown:
 *
 * Frees internal lookup tables created in g_mime_charset_map_init().
 **/
void
g_mime_charset_map_shutdown (void)
{
	if (!iconv_charsets)
		return;
	
	g_hash_table_foreach (iconv_charsets, iconv_charset_free, NULL);
	g_hash_table_destroy (iconv_charsets);
	iconv_charsets = NULL;
	
	g_free (locale_charset);
	locale_charset = NULL;
	
	g_free (locale_lang);
	locale_lang = NULL;
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
			lang[0] = g_ascii_tolower (lang[0]);
			lang[1] = g_ascii_tolower (lang[1]);
			
			/* validate the country code */
			if (strlen (lang + 3) > 2) {
				/* invalid country code */
				lang[2] = '\0';
			} else {
				lang[2] = '-';
				lang[3] = g_ascii_toupper (lang[3]);
				lang[4] = g_ascii_toupper (lang[4]);
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
 * g_mime_locale_charset(). Only really needs to be called for non-
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
		charset = g_ascii_strdown (known_iconv_charsets[i].charset, -1);
		iconv_name = g_strdup (known_iconv_charsets[i].iconv_name);
		g_hash_table_insert (iconv_charsets, charset, iconv_name);
	}
	
#ifdef HAVE_CODESET
	if ((locale_charset = nl_langinfo (CODESET)) && locale_charset[0])
		locale_charset = g_ascii_strdown (locale_charset, -1);
	else
		locale_charset = NULL;
#endif
	
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
		
		if (!locale_charset) {
			codeset = strchr (locale, '.');
			if (codeset) {
				codeset++;
				
				/* ; is a hack for debian systems and / is a hack for Solaris systems */
				p = codeset;
				while (*p && !strchr ("@;/", *p))
					p++;
				
				locale_charset = g_ascii_strdown (codeset, p - codeset);
			} else {
				/* charset unknown */
				locale_charset = NULL;
			}
		}
		
		locale_parse_lang (locale);
	}
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
 * Returns a language code that is specific to @charset, or %NULL on
 * fail.
 **/
const char *
g_mime_charset_language (const char *charset)
{
	guint i;
	
	if (!charset)
		return NULL;
	
	for (i = 0; i < G_N_ELEMENTS (cjkr_lang_map); i++) {
		if (!g_ascii_strcasecmp (cjkr_lang_map[i].charset, charset))
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
					iconv_name = g_strdup_printf (ICONV_ISO_INT_FORMAT,
								      iso, codepage);
			} else {
				/* codepage is a string - probably iso-2022-jp or something */
				iconv_name = g_strdup_printf (ICONV_ISO_STR_FORMAT,
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

static const char *windows_charsets[] = {
	"windows-cp1250",
	"windows-cp1251",
	"windows-cp1252",
	"windows-cp1253",
	"windows-cp1254",
	"windows-cp1255",
	"windows-cp1256",
	"windows-cp1257",
	"windows-cp1258",
	"windows-cp1259"
};


/**
 * g_mime_charset_canon_name:
 * @charset: charset name
 *
 * Attempts to find a canonical charset name for @charset.
 *
 * Note: Will normally return the same value as
 * g_mime_charset_iconv_name() unless the system iconv does not use
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
	guint iso;
	
	if (!charset)
		return NULL;
	
	charset = g_mime_charset_iconv_name (charset);
	if (g_ascii_strncasecmp (charset, "iso", 3) == 0) {
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
		
		if (iso > G_N_ELEMENTS (iso_charsets))
			return charset;
		
		return iso_charsets[iso];
	} else if (!strncmp (charset, "CP125", 5)) {
		ptr = charset + 5;
		if (*ptr >= '0' && *ptr <= '9')
			return windows_charsets[*ptr - '0'];
	}
	
	return charset;
}


/**
 * g_mime_charset_name:
 * @charset: charset name
 *
 * Attempts to find an iconv-friendly charset name for @charset.
 *
 * Note: This function is deprecated. Use g_mime_charset_iconv_name()
 * instead.
 *
 * Returns an iconv-friendly charset name for @charset.
 **/
const char *
g_mime_charset_name (const char *charset)
{
	return g_mime_charset_iconv_name (charset);
}


/**
 * g_mime_charset_locale_name:
 *
 * Gets the user's locale charset (or iso-8859-1 by default).
 *
 * Note: This function is deprecated. Use g_mime_locale_charset()
 * instead.
 *
 * Returns the user's locale charset (or iso-8859-1 by default).
 **/
const char *
g_mime_charset_locale_name (void)
{
	return g_mime_locale_charset ();
}


/**
 * g_mime_charset_iso_to_windows:
 * @isocharset: ISO-8859-# charset
 *
 * Maps the ISO-8859-# charset to the equivalent Windows-CP125#
 * charset.
 *
 * Returns equivalent Windows charset.
 **/
const char *
g_mime_charset_iso_to_windows (const char *isocharset)
{
	/* According to http://czyborra.com/charsets/codepages.html,
	 * the charset mapping is as follows:
	 *
	 * us-ascii    maps to windows-cp1252
	 * iso-8859-1  maps to windows-cp1252
	 * iso-8859-2  maps to windows-cp1250
	 * iso-8859-3  maps to windows-cp????
	 * iso-8859-4  maps to windows-cp????
	 * iso-8859-5  maps to windows-cp1251
	 * iso-8859-6  maps to windows-cp1256
	 * iso-8859-7  maps to windows-cp1253
	 * iso-8859-8  maps to windows-cp1255
	 * iso-8859-9  maps to windows-cp1254
	 * iso-8859-10 maps to windows-cp????
	 * iso-8859-11 maps to windows-cp????
	 * iso-8859-12 maps to windows-cp????
	 * iso-8859-13 maps to windows-cp1257
	 *
	 * Assumptions:
	 *  - I'm going to assume that since iso-8859-4 and
	 *    iso-8859-13 are Baltic that it also maps to
	 *    windows-cp1257.
	 */
	
	isocharset = g_mime_charset_canon_name (isocharset);
	
	if (!g_ascii_strcasecmp (isocharset, "iso-8859-1") || !g_ascii_strcasecmp (isocharset, "us-ascii"))
		return "windows-cp1252";
	else if (!g_ascii_strcasecmp (isocharset, "iso-8859-2"))
		return "windows-cp1250";
	else if (!g_ascii_strcasecmp (isocharset, "iso-8859-4"))
		return "windows-cp1257";
	else if (!g_ascii_strcasecmp (isocharset, "iso-8859-5"))
		return "windows-cp1251";
	else if (!g_ascii_strcasecmp (isocharset, "iso-8859-6"))
		return "windows-cp1256";
	else if (!g_ascii_strcasecmp (isocharset, "iso-8859-7"))
		return "windows-cp1253";
	else if (!g_ascii_strcasecmp (isocharset, "iso-8859-8"))
		return "windows-cp1255";
	else if (!g_ascii_strcasecmp (isocharset, "iso-8859-9"))
		return "windows-cp1254";
	else if (!g_ascii_strcasecmp (isocharset, "iso-8859-13"))
		return "windows-cp1257";
	
	return isocharset;
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
 * @charset: charset structure
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
				level = 2;
		} else {
			mask = 0;
			level = 2;
		}
	}
	
	charset->mask = mask;
	charset->level = level;
}

static const char *
charset_best_mask (unsigned int mask)
{
	const char *lang;
	guint i;
	
	for (i = 0; i < G_N_ELEMENTS (charinfo); i++) {
		if (charinfo[i].bit & mask) {
			lang = g_mime_charset_language (charinfo[i].name);
			
			if (!lang || (locale_lang && !strncmp (locale_lang, lang, 2)))
				return charinfo[i].name;
		}
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


/**
 * g_mime_charset_can_encode:
 * @mask: a #GMimeCharset mask
 * @charset: a charset
 * @text: utf-8 text to check
 * @len: length of @text
 *
 * Check to see if the UTF-8 @text will fit safely within @charset.
 *
 * Returns %TRUE if it is safe to encode @text into @charset or %FALSE
 * otherwise.
 **/
gboolean
g_mime_charset_can_encode (GMimeCharset *mask, const char *charset, const char *text, size_t len)
{
	const unsigned char *inptr = (const unsigned char *) text;
	const unsigned char *inend = inptr + len;
	size_t inleft, outleft, rc;
	const char *inbuf = text;
	char out[256], *outbuf;
	const char *iconv_name;
	iconv_t cd;
	guint i;
	
	if (len == 0)
		return TRUE;
	
	if (mask->level == 0 && (!charset || !g_ascii_strcasecmp (charset, "us-ascii"))) {
		/* simple US-ASCII case - is this scan even necessary? */
		while (inptr < inend && is_ascii (*inptr))
			inptr++;
		
		if (inptr == inend)
			return TRUE;
		
		return FALSE;
	}
	
	if (!g_ascii_strcasecmp (charset, "utf-8")) {
		/* we can encode anything in utf-8 */
		return TRUE;
	}
	
	charset = g_mime_charset_iconv_name (charset);
	
	if (mask->level == 1)
		return !g_ascii_strcasecmp (charset, "iso-8859-1");
	
	/* check if this is a charset that we have precalculated masking for */
	for (i = 0; i < G_N_ELEMENTS (charinfo); i++) {
		iconv_name = g_mime_charset_iconv_name (charinfo[i].name);
		if (charset == iconv_name)
			break;
	}
	
	if (i < G_N_ELEMENTS (charinfo)) {
		/* indeed we do... */
		return (charinfo[i].bit & mask->mask);
	}
	
	/* down to the nitty gritty slow and painful way... */
	if ((cd = g_mime_iconv_open (charset, "UTF-8")) == (iconv_t) -1)
		return FALSE;
	
	inleft = len;
	
	do {
		outleft = sizeof (out);
		outbuf = out;
		errno = 0;
		
		rc = iconv (cd, (char **) &inbuf, &inleft, &outbuf, &outleft);
		if (rc == (size_t) -1 && errno != E2BIG)
			break;
	} while (inleft > 0);
	
	if (inleft == 0) {
		outleft = sizeof (out);
		outbuf = out;
		errno = 0;
		
		rc = iconv (cd, NULL, NULL, &outbuf, &outleft);
	}
	
	g_mime_iconv_close (cd);
	
	return rc != (size_t) -1;
}


/**
 * g_mime_set_user_charsets:
 * @charsets: an array of user-preferred charsets
 *
 * Set a list of charsets for GMime to use as a hint for encoding and
 * decoding headers. The charset list should be in order of preference
 * (e.g. most preferred first, least preferred last).
 **/
void
g_mime_set_user_charsets (const char **charsets)
{
	int i, n = 0;
	
	if (user_charsets)
		g_strfreev (user_charsets);
	
	if (charsets == NULL) {
		user_charsets = NULL;
		return;
	}
	
	while (charsets[n] != NULL)
		n++;
	
	if (n == 0) {
		user_charsets = NULL;
		return;
	}
	
	user_charsets = g_malloc (sizeof (char *) * (n + 1));
	for (i = 0; i <= n; i++)
		user_charsets[i] = (char *) charsets[i];
}


/**
 * g_mime_user_charsets:
 *
 * Get the list of user-preferred charsets set with
 * g_mime_set_user_charsets().
 *
 * Returns an array of user-set charsets or %NULL if none set.
 **/
const char **
g_mime_user_charsets (void)
{
	return (const char **) user_charsets;
}



#ifdef BUILD_CHARSET_MAP

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
	{ "iso-8859-6",   0, 0 },  /* Arabic */
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
	g_assert (bytes <= 4);
	
	for (i = 0; i < 128; i++)
		in[i] = i + 128;
	
	for (j = 0; tables[j].name && !tables[j].multibyte; j++) {
		cd = iconv_open (UCS, tables[j].name);
		inbuf = in;
		inleft = sizeof (in);
		outbuf = (char *) out;
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
	for (k = 0; k < bytes; k++)
		printf ("\tunsigned char *bits%d;\n", k);
	
	printf ("} charmap[256] = {\n\t");
	for (i = 0; i < 256; i++) {
		/* first, do we need this block? */
		printf ("{ ");
		for (k = 0; k < bytes; k++) {
			for (j = 0; j < 256; j++) {
				if ((encoding_map[i * 256 + j] & (0xff << (k * 8))) != 0)
					break;
			}
			
			if (j < 256)
				printf ("m%02x%x, ", i, k);
			else
				printf ("0, ");
		}
		
		printf ("}, ");
		if (((i + 1) & 7) == 0 && i < 255)
			printf ("\n\t");
	}
	printf ("\n};\n\n");
	
	printf ("struct {\n\tconst char *name;\n\tunsigned int bit;\n} charinfo[] = {\n");
	for (j = 0; tables[j].name; j++)
		printf ("\t{ \"%s\", 0x%08x },\n", tables[j].name, tables[j].bit);
	printf ("};\n\n");
	
	printf ("#define charset_mask(x) \\\n");
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
