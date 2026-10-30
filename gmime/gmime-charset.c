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

#if defined (WIN32) || defined (__CYGWIN__)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

#include "gmime-charset-map-private.h"
#include "gmime-table-private.h"
#include "gmime-charset.h"
#include "gmime-error.h"
#include "gmime-internal.h"
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
#define ICONV_SHIFT_JIS "shift-jis"
#define ICONV_10646 "iso-10646"
#endif /* USE_ICONV_DETECT */


/**
 * SECTION: gmime-charset
 * @title: gmime-charset
 * @short_description: Charset helper functions
 * @see_also:
 *
 * Charset utility functions.
 **/


/* a useful website on charset alaises:
 * http://www.li18nux.org/subgroups/sa/locnameguide/v1.1draft/CodesetAliasTable-V11.html */

static struct {
	const char *charset;
	const char *iconv_name;
	const char *mime_name;
} known_iconv_charsets[] = {
	/* charset name, iconv-friendly name (sometimes case sensitive), MIME name */
	{ "utf-8",           "UTF-8",      "utf-8" },
	{ "utf8",            "UTF-8",      "utf-8" },
	
	/* ANSI_X3.4-1968 is used on some systems and should be
	   treated the same as US-ASCII */
	{ "ANSI_X3.4-1968",  "US-ASCII",   "us-ascii" },
	{ "ascii",          "US-ASCII",   "us-ascii" },
	{ "iso-ir-6",       "US-ASCII",   "us-ascii" },
	{ "csascii",        "US-ASCII",   "us-ascii" },

	{ "latin1",         "ISO-8859-1", "iso-8859-1" },
	{ "latin2",         "ISO-8859-2", "iso-8859-2" },
	{ "latin3",         "ISO-8859-3", "iso-8859-3" },
	{ "latin4",         "ISO-8859-4", "iso-8859-4" },
	{ "latin5",         "ISO-8859-9", "iso-8859-9" },
	{ "latin9",         "ISO-8859-15", "iso-8859-15" },
	{ "ansi_x3.110-1983", "ISO-8859-1", "iso-8859-1" },
	
	/* 10646 is a special case, its usually UCS-2 big endian */
	/* This might need some checking but should be ok for
           solaris/linux */
	{ "iso-10646-1",     "UCS-2BE",    "ucs-2be" },
	{ "iso_10646-1",     "UCS-2BE",    "ucs-2be" },
	{ "iso10646-1",      "UCS-2BE",    "ucs-2be" },
	{ "iso-10646",       "UCS-2BE",    "ucs-2be" },
	{ "iso_10646",       "UCS-2BE",    "ucs-2be" },
	{ "iso10646",        "UCS-2BE",    "ucs-2be" },
	
	/* Korean charsets */
	/* Note: according to http://www.iana.org/assignments/character-sets,
	 * ks_c_5601-1987 should really map to ISO-2022-KR, but the EUC-KR
	 * mapping was given to me via a native Korean user, so I'm not sure
	 * if I should change this... perhaps they are compatible? */
	{ "ks_c_5601-1987",  "EUC-KR",     "euc-kr" },
	{ "5601",            "EUC-KR",     "euc-kr" },
	{ "ksc-5601",        "EUC-KR",     "euc-kr" },
	{ "ksc-5601-1987",   "EUC-KR",     "euc-kr" },
	{ "ksc-5601_1987",   "EUC-KR",     "euc-kr" },
	{ "ks_c_5861-1992",  "EUC-KR",     "euc-kr" },
	{ "euckr-0",         "EUC-KR",     "euc-kr" },
	
	/* Chinese charsets */
	{ "big5-0",          "BIG5",       "big5" },
	{ "big5.eten-0",     "BIG5",       "big5" },
	{ "big5hkscs-0",     "BIG5-HKSCS", "big5-hkscs" },
	{ "big5hkscs",       "BIG5-HKSCS", "big5-hkscs" },
	/* Note: GBK is a superset of gb2312 (see
	 * http://en.wikipedia.org/wiki/GBK for details), so 'upgrade'
	 * gb2312 to GBK so that we can completely convert GBK text
	 * that is incorrectly tagged as gb2312 to UTF-8. */
	{ "gb2312",          "GBK",        "gb2312" },
	{ "gb-2312",         "GBK",        "gb2312" },
	{ "gb2312-0",        "GBK",        "gb2312" },
	{ "gb2312-80",       "GBK",        "gb2312" },
	{ "gb2312.1980-0",   "GBK",        "gb2312" },
	/* euc-cn is an alias for gb2312 */
	{ "euc-cn",          "GBK",        "euc-cn" },
	{ "gb18030-0",       "gb18030",    "gb18030" },
	{ "gbk-0",           "GBK",        "euc-cn" },
	{ "gbk",             "GBK",        "euc-cn" },
	
	/* Japanese charsets */
	{ "eucjp-0",         "euc-jp",     "euc-jp" },
	{ "ujis-0",          "euc-jp",     "euc-jp" },
	{ "ujis",            "euc-jp",     "euc-jp" },
	{ "eucJP",           "eucJP",      "euc-jp" },
	{ NULL,              NULL,         NULL }
};

static const char *shiftjis_aliases[] = {
	"shift-jis", "shift_jis", "sjis", "shift_jis-2004", "shift_jisx0213",
	"jisx0208.1983-0", "jisx0212.1990-0", "pck", NULL
};

/* map CJKR charsets to their language code */
/* NOTE: only support charset names that will be returned by
 * g_mime_charset_iconv_name() so that we don't have to keep track of
 * all the aliases too. */
static struct {
	const char *charset;
	const char *lang;
} cjkr_lang_map[] = {
	{ "Big5",          "zh" },
	{ "BIG5HKSCS",     "zh" },
	{ "BIG5-HKSCS",    "zh" },
	{ "gb2312",        "zh" },
	{ "gb18030",       "zh" },
	{ "gbk",           "zh" },
	{ "euc-tw",        "zh" },
	{ "iso-2022-jp",   "ja" },
	{ ICONV_SHIFT_JIS, "ja" },
	{ "ujis",          "ja" },
	{ "eucJP",         "ja" },
	{ "euc-jp",        "ja" },
	{ "euc-kr",        "ko" },
	{ "koi8-r",        "ru" },
	{ "koi8-u",        "uk" }
};

static GHashTable *iconv_charsets = NULL;
static GHashTable *charset_codepages = NULL;
static GHashTable *codepage_cache = NULL;
static int next_codepage = -1;

static const struct {
	const char *name;
	int codepage;
} known_codepages[] = {
	{ "us-ascii", 20127 },
	{ "utf-8", 65001 },
	{ "utf-7", 65000 },
	{ "utf-16le", 1200 },
	{ "utf-16be", 1201 },
	{ "utf-32le", 12000 },
	{ "utf-32be", 12001 },
	{ "iso-8859-1", 28591 },
	{ "iso-8859-2", 28592 },
	{ "iso-8859-3", 28593 },
	{ "iso-8859-4", 28594 },
	{ "iso-8859-5", 28595 },
	{ "iso-8859-6", 28596 },
	{ "iso-8859-7", 28597 },
	{ "iso-8859-8", 28598 },
	{ "iso-8859-9", 28599 },
	{ "iso-8859-13", 28603 },
	{ "iso-8859-15", 28605 },
	{ "windows-1250", 1250 },
	{ "windows-1251", 1251 },
	{ "windows-1252", 1252 },
	{ "windows-1253", 1253 },
	{ "windows-1254", 1254 },
	{ "windows-1255", 1255 },
	{ "windows-1256", 1256 },
	{ "windows-1257", 1257 },
	{ "windows-1258", 1258 },
	{ "shift_jis", 932 },
	{ "euc-jp", 51932 },
	{ "euc-kr", 51949 },
	{ "gb2312", 936 },
	{ "gbk", 936 },
	{ "euc-cn", 936 },
	{ "big5", 950 },
	{ "gb18030", 54936 },
	{ "iso-2022-jp", 50220 },
	{ "iso-2022-kr", 50225 },
	{ "koi8-r", 20866 },
	{ "koi8-u", 21866 },
	{ "macintosh", 10000 }
};

static char *locale_charset = NULL;
static char *locale_lang = NULL;
static int initialized = 0;

#ifdef G_THREADS_ENABLED
static GMutex lock;
#define CHARSET_UNLOCK() g_mutex_unlock (&lock);
#define CHARSET_LOCK() g_mutex_lock (&lock);
#else
#define CHARSET_UNLOCK()
#define CHARSET_LOCK()
#endif /* G_THREADS_ENABLED */


/**
 * g_mime_charset_map_shutdown:
 *
 * Frees internal lookup tables created in g_mime_charset_map_init().
 **/
void
g_mime_charset_map_shutdown (void)
{
	if (--initialized)
		return;
	
#ifdef G_THREADS_ENABLED
	if (glib_check_version (2, 37, 4) == NULL) {
		/* The implementation of g_mutex_clear() prior
		 * to glib 2.37.4 did not properly reset the
		 * internal mutex pointer to NULL, so re-initializing
		 * GMime would not properly re-initialize the mutexes.
		 **/
		g_mutex_clear (&lock);
	}
#endif
	
	g_hash_table_destroy (codepage_cache);
	codepage_cache = NULL;
	g_hash_table_destroy (charset_codepages);
	charset_codepages = NULL;
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
		lang = g_strndup (locale, (size_t) (codeset - locale));
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
 * Initializes character set maps.
 *
 * Note: g_mime_init() calls this routine for you.
 **/
void
g_mime_charset_map_init (void)
{
	char *charset, *iconv_name;
	int i;
#ifndef WIN32
	char *locale;
#endif
	
	initialized = MAX (initialized, 0);
	
	if (initialized++)
		return;
	
#ifdef G_THREADS_ENABLED
	g_mutex_init (&lock);
#endif
	
	iconv_charsets = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, g_free);
	charset_codepages = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, NULL);
	codepage_cache = g_hash_table_new (g_direct_hash, g_direct_equal);
	next_codepage = -1;

	for (i = 0; i < G_N_ELEMENTS (known_codepages); i++)
		g_hash_table_insert (charset_codepages, g_strdup (known_codepages[i].name),
				     GINT_TO_POINTER (known_codepages[i].codepage));
	
	for (i = 0; known_iconv_charsets[i].charset != NULL; i++) {
		charset = g_ascii_strdown (known_iconv_charsets[i].charset, -1);
		iconv_name = g_strdup (known_iconv_charsets[i].iconv_name);
		g_hash_table_insert (iconv_charsets, charset, iconv_name);
	}
	
#ifndef WIN32
#ifdef HAVE_CODESET
	if ((locale_charset = nl_langinfo (CODESET)) && locale_charset[0]) {
#ifdef __CYGWIN__
		/* Apparently some versions of Cygwin, nl_langinfo(CODESET)
		 * always reports US-ASCII no matter what. */
		if (strcmp (locale_charset, "US-ASCII") != 0) {
			/* Guess this version of Cygwin is fixed. */
			locale_charset = g_ascii_strdown (locale_charset, -1);
		} else {
			/* Cannot rely on US-ASCII being accurate. */
			locale_charset = NULL;
		}
#else
		locale_charset = g_ascii_strdown (locale_charset, -1);
#endif
	} else
		locale_charset = NULL;
#endif
	
	/* Apparently setlocale() is not reliable either... use getenv() instead. */
	/*locale = setlocale (LC_ALL, NULL);*/
	
	if (!(locale = getenv ("LC_ALL")) || !locale[0])
		if (!(locale = getenv ("LC_CTYPE")) || !locale[0])
			locale = getenv ("LANG");
	
	if (!locale || !locale[0] || !strcmp (locale, "C") || !strcmp (locale, "POSIX")) {
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
				
				locale_charset = g_ascii_strdown (codeset, (size_t)(p - codeset));
			} else {
				/* charset unknown */
				locale_charset = NULL;
			}
		}
		
		locale_parse_lang (locale);
	}
#else /* WIN32 */
	locale_charset = g_strdup_printf ("cp%u", GetACP ());
#endif
}


/**
 * g_mime_locale_charset:
 *
 * Gets the user's locale charset (or iso-8859-1 by default).
 *
 * Returns: the user's locale charset (or iso-8859-1 by default).
 **/
const char *
g_mime_locale_charset (void)
{
	return locale_charset ? locale_charset : "iso-8859-1";
}


/**
 * g_mime_locale_language:
 *
 * Gets the user's locale language code (or %NULL by default).
 *
 * Returns: (nullable): the user's locale language code (or %NULL by default).
 **/
const char *
g_mime_locale_language (void)
{
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
 * Returns: (nullable): a language code that is specific to @charset,
 * or %NULL on fail.
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

static gboolean
is_shift_jis (const char *name)
{
	int i;
	
	for (i = 0; shiftjis_aliases[i] != NULL; i++) {
		if (!strcmp (name, shiftjis_aliases[i]))
			return TRUE;
	}
	
	return FALSE;
}

/**
 * g_mime_charset_iconv_name:
 * @charset: charset name
 *
 * Attempts to find an iconv-friendly charset name for @charset.
 * ISO numbers outside the unsigned-int range are left unnormalized.
 *
 * Returns: an iconv-friendly charset name for @charset.
 **/
const char *
g_mime_charset_iconv_name (const char *charset)
{
	char *name, *iconv_name, *buf;
	
	if (charset == NULL)
		return NULL;
	
	name = g_ascii_strdown (charset, -1);
	
	CHARSET_LOCK ();
	
	iconv_name = g_hash_table_lookup (iconv_charsets, name);
	if (iconv_name) {
		CHARSET_UNLOCK ();
		g_free (name);
		return iconv_name;
	}
	
	if (!strncmp (name, "iso", 3)) {
		guint64 iso, codepage;
		char *p;
		
		buf = name + 3;
		if (*buf == '-' || *buf == '_')
			buf++;
		
		iso = g_ascii_strtoull (buf, &p, 10);
		
		if (!g_ascii_isdigit (*buf) || iso > G_MAXUINT) {
			iconv_name = g_strdup (name);
		} else if (iso == 10646) {
			/* they all become ICONV_10646 */
			iconv_name = g_strdup (ICONV_10646);
		} else {
			buf = p;
			if (*buf == '-' || *buf == '_')
				buf++;
			
			codepage = g_ascii_strtoull (buf, &p, 10);
			
			if (p > buf) {
				/* codepage is numeric */
				if (!g_ascii_isdigit (*buf) || codepage > G_MAXUINT) {
					iconv_name = g_strdup (name);
				} else {
#ifdef __aix__
					if (codepage == 13)
						iconv_name = g_strdup ("IBM-921");
					else
#endif /* __aix__ */
						iconv_name = g_strdup_printf (ICONV_ISO_INT_FORMAT,
									      (guint) iso, (guint) codepage);
				}
			} else {
				/* codepage is a string - probably iso-2022-jp or something */
				iconv_name = g_strdup_printf (ICONV_ISO_STR_FORMAT, (guint) iso, p);
			}
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
	} else if (is_shift_jis (name)) {
		iconv_name = g_strdup (ICONV_SHIFT_JIS);
	} else {
		/* assume charset name is ok as is? */
		iconv_name = g_strdup (charset);
	}
	
	g_hash_table_insert (iconv_charsets, name, iconv_name);
	
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
 * Returns: a canonical charset name for @charset.
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
		
		if (iso >= G_N_ELEMENTS (iso_charsets))
			return charset;
		
		return iso_charsets[iso];
	} else if (!g_ascii_strncasecmp (charset, "CP125", 5)) {
		ptr = charset + 5;
		if (*ptr >= '0' && *ptr <= '9' && ptr[1] == '\0')
			return windows_charsets[*ptr - '0'];
	}
	
	return charset;
}

/**
 * g_mime_charset_try_get_bom_encoding:
 * @buffer: (array length=length) (element-type guint8) (nullable): bytes to inspect
 * @length: the number of available bytes
 * @charset: (out) (nullable) (transfer none): a static charset name, or %NULL
 *
 * Detects an initial UTF-16LE, UTF-16BE, or UTF-8 byte-order mark, returning
 * "utf-16le", "utf-16be", or "utf-8", respectively. At most three bytes are
 * inspected; incomplete marks do not match. @buffer may be %NULL only when
 * @length is zero.
 *
 * This follows MimeKit's BOM policy, not general Unicode encoding detection:
 * UTF-32 is not detected, and its FF FE prefix matches UTF-16LE. No bytes are
 * removed or decoded, and a detected mark remains part of the content.
 *
 * Returns: %TRUE if a supported mark was found, otherwise %FALSE.
 **/
gboolean
g_mime_charset_try_get_bom_encoding (const guint8 *buffer, gsize length, const char **charset)
{
	g_return_val_if_fail (charset != NULL, FALSE);

	*charset = NULL;
	g_return_val_if_fail (buffer != NULL || length == 0, FALSE);

	/* Match the two-byte UTF-16 marks first, just as MimeKit does. In
	 * particular, do not reinterpret FF FE 00 00 as a UTF-32 mark. */
	if (length >= 2 && buffer[0] == 0xff && buffer[1] == 0xfe)
		*charset = "utf-16le";
	else if (length >= 2 && buffer[0] == 0xfe && buffer[1] == 0xff)
		*charset = "utf-16be";
	else if (length >= 3 && buffer[0] == 0xef && buffer[1] == 0xbb && buffer[2] == 0xbf)
		*charset = "utf-8";

	return *charset != NULL;
}


/**
 * g_mime_charset_try_get_bom_encoding_from_stream:
 * @stream: a #GMimeStream
 * @charset: (out) (nullable) (transfer none): a static charset name, or %NULL
 * @error: return location for an error, or %NULL
 *
 * Reads up to three bytes at the stream's current position and applies
 * g_mime_charset_try_get_bom_encoding(). Exactly one read is attempted,
 * including on streams that return short reads. The stream is not rewound,
 * sought, or closed, and the inspected bytes remain consumed on either a
 * match or a nonmatch. A zero-byte read returns %FALSE without an error.
 *
 * A read failure returns %FALSE with %GMIME_ERROR using the stream's errno,
 * or %GMIME_ERROR_GENERAL if the stream did not set errno. No charset is
 * returned on failure.
 *
 * Returns: %TRUE if a supported mark was found, otherwise %FALSE.
 **/
gboolean
g_mime_charset_try_get_bom_encoding_from_stream (GMimeStream *stream, const char **charset, GError **error)
{
	guint8 buffer[3];
	ssize_t nread;
	int errsv;

	g_return_val_if_fail (charset != NULL, FALSE);

	*charset = NULL;
	g_return_val_if_fail (GMIME_IS_STREAM (stream), FALSE);
	g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

	/* A short read is not filled by further reads: detection must consume
	 * exactly what the single managed Stream.Read would have returned. */
	errno = 0;
	if ((nread = g_mime_stream_read (stream, (char *) buffer, sizeof (buffer))) == -1) {
		errsv = errno;
		g_set_error (error, GMIME_ERROR, errsv ? errsv : GMIME_ERROR_GENERAL,
			     "Failed to read a text byte-order mark: %s",
			     errsv ? g_strerror (errsv) : "unknown I/O error");
		return FALSE;
	}

	return g_mime_charset_try_get_bom_encoding (buffer, (gsize) nread, charset);
}


char *
_g_mime_charset_get_mime_name (const char *charset)
{
	const char *name;
	guint i;

	if (strstr (charset, "//"))
		return g_strdup (charset);

	for (i = 0; known_iconv_charsets[i].charset; i++) {
		if (!g_ascii_strcasecmp (charset, known_iconv_charsets[i].charset))
			return g_strdup (known_iconv_charsets[i].mime_name);
	}

	name = g_mime_charset_canon_name (charset);
	if (!g_ascii_strcasecmp (name, ICONV_SHIFT_JIS))
		return g_strdup ("shift_jis");

	if (g_str_has_prefix (name, "windows-cp"))
		return g_strconcat ("windows-", name + 10, NULL);

	return g_ascii_strdown (name, -1);
}

char *
_g_mime_charset_get_text_name (const char *charset, GError **error)
{
	const char *p;

	for (p = charset; *p; p++) {
		if ((guchar) *p >= 127 || !is_ttoken (*p))
			break;
	}

	if (!*charset || *p) {
		g_set_error_literal (error, G_CONVERT_ERROR, G_CONVERT_ERROR_NO_CONVERSION,
				     "Text charset name must be a nonempty MIME token");
		return NULL;
	}

	/* MimeKit's historical "unicode" alias means UTF-8, unlike .NET's
	 * Encoding.GetEncoding alias. Resolve it before native charset lookup. */
	if (!g_ascii_strcasecmp (charset, "unicode"))
		charset = "utf-8";
	/* Managed UTF-16/32 encodings use little-endian bytes, not BOM sniffing. */
	else if (!g_ascii_strcasecmp (charset, "utf16") || !g_ascii_strcasecmp (charset, "csunicode"))
		charset = "utf-16";
	else if (!g_ascii_strcasecmp (charset, "utf32"))
		charset = "utf-32";

	return _g_mime_charset_get_mime_name (charset);
}

const char *
_g_mime_charset_get_text_converter (const char *name)
{
	if (!g_ascii_strcasecmp (name, "utf-16"))
		return "UTF-16LE";

	if (!g_ascii_strcasecmp (name, "utf-32"))
		return "UTF-32LE";

	return g_mime_charset_iconv_name (name);
}

char *
_g_mime_charset_encode_text (const char *charset, const char *text, gsize *length, char **mime_name, GError **error)
{
	char *name, *bytes;

	if (!(name = _g_mime_charset_get_text_name (charset, error)))
		return NULL;

	/* Explicit-endian encoders omit the preamble, like Encoding.GetBytes. */
	bytes = _g_mime_iconv_convert (text, -1, _g_mime_charset_get_text_converter (name), "UTF-8", length, error);
	if (bytes)
		*mime_name = name;
	else
		g_free (name);

	return bytes;
}

iconv_t
_g_mime_charset_open_text_decoder (const char *charset, const char **encoding, GError **error)
{
	iconv_t decoder;
	const char *converter;
	char *name;
	int errsv;

	if (encoding)
		*encoding = NULL;
	if (!(name = _g_mime_charset_get_text_name (charset, error)))
		return (iconv_t) -1;

	converter = _g_mime_charset_get_text_converter (name);
	errno = 0;
	decoder = iconv_open ("UTF-8", converter);
	errsv = errno;
	g_free (name);

	if (decoder == (iconv_t) -1) {
		g_set_error (error, G_CONVERT_ERROR, errsv == EINVAL ? G_CONVERT_ERROR_NO_CONVERSION : G_CONVERT_ERROR_FAILED,
			     "Cannot decode text using charset '%s': %s",
			     charset, errsv ? g_strerror (errsv) : "unknown conversion error");
	} else if (encoding) {
		*encoding = converter;
	}

	return decoder;
}

/* Positive identities are codepages; negative identities keep unmapped native
 * charsets distinct. Both are valid only until the charset maps shut down. */
int
_g_mime_charset_get_codepage (const char *charset)
{
	gpointer value;
	char *name;

	g_return_val_if_fail (charset != NULL, 0);

	CHARSET_LOCK ();
	value = g_hash_table_lookup (codepage_cache, charset);
	CHARSET_UNLOCK ();
	if (value)
		return GPOINTER_TO_INT (value);

	charset = g_mime_charset_iconv_name (charset);
	CHARSET_LOCK ();
	value = g_hash_table_lookup (codepage_cache, charset);
	CHARSET_UNLOCK ();
	if (value)
		return GPOINTER_TO_INT (value);

	name = _g_mime_charset_get_mime_name (charset);
	CHARSET_LOCK ();
	if (!(value = g_hash_table_lookup (charset_codepages, name))) {
		g_assert (next_codepage > G_MININT);
		value = GINT_TO_POINTER (next_codepage--);
		g_hash_table_insert (charset_codepages, name, value);
		name = NULL;
	}

	g_hash_table_insert (codepage_cache, (gpointer) charset, value);
	CHARSET_UNLOCK ();
	g_free (name);

	return GPOINTER_TO_INT (value);
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
 * Returns: an iconv-friendly charset name for @charset.
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
 * Returns: the user's locale charset (or iso-8859-1 by default).
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
 * Returns: equivalent Windows charset.
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
 * @inbuf: input text buffer (must be in UTF-8)
 * @inlen: input buffer length
 *
 * Steps through the input buffer 1 unicode character (glyph) at a
 * time (ie, not necessarily 1 byte at a time). Bitwise 'and' our
 * @charset->mask with the mask for each glyph. This has the effect of
 * limiting what charsets our @charset->mask can match.
 **/
void
g_mime_charset_step (GMimeCharset *charset, const char *inbuf, size_t inlen)
{
	register const char *inptr = inbuf;
	const char *inend = inbuf + inlen;
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
 * Returns: (nullable): the charset name that best represents the charset
 * mask @charset or %NULL for us-ascii.
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
 * @inbuf: a UTF-8 text buffer
 * @inlen: input buffer length
 *
 * Computes the best charset to use to encode this text buffer.
 *
 * Returns: (nullable): the charset name best suited for the input text
 * or %NULL if it is ascii-safe.
 **/
const char *
g_mime_charset_best (const char *inbuf, size_t inlen)
{
	GMimeCharset charset;
	
	g_mime_charset_init (&charset);
	g_mime_charset_step (&charset, inbuf, inlen);
	
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
 * Returns: %TRUE if it is safe to encode @text into @charset or %FALSE
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
