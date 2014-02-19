/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*  GMime
 *  Copyright (C) 2000-2014 Jeffrey Stedfast
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
 *  Foundation, 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */


#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <gmime/gmime.h>

#include "testsuite.h"

/*#define ENABLE_ZENTIMER*/
#include "zentimer.h"

#ifdef TEST_CACHE
static char *charsets[] = {
	"iso-8859-1",
	"iso-8859-2",
	"iso-8859-4",
	"iso-8859-5",
	"iso-8859-7",
	"iso-8859-8",
	"iso-8859-9",
	"iso-8859-13",
	"iso-8859-15",
	"koi8-r",
	"koi8-u",
	"windows-1250",
	"windows-1251",
	"windows-1252",
	"windows-1253",
	"windows-1254",
	"windows-1255",
	"windows-1256",
	"windows-1257",
	"euc-kr",
	"euc-jp",
	"iso-2022-kr",
	"iso-2022-jp",
	"utf-8",
};

static void
test_cache (void)
{
	GSList *node, *next, *open_cds = NULL;
	iconv_t cd;
	int i;
	
	srand (time (NULL));
	
	for (i = 0; i < 5000; i++) {
		const char *from, *to;
		int which;
		
		which = rand () % G_N_ELEMENTS (charsets);
		from = charsets[which];
		which = rand () % G_N_ELEMENTS (charsets);
		to = charsets[which];
		
		cd = g_mime_iconv_open (from, to);
		if (cd == (iconv_t) -1) {
			g_warning ("%d: failed to open converter for %s to %s",
				   i, from, to);
			continue;
		}
		
		which = rand () % 3;
		if (!which) {
			g_mime_iconv_close (cd);
		} else {
			open_cds = g_slist_prepend (open_cds, cd);
		}
	}
	
	node = open_cds;
	while (node) {
		next = node->next;
		cd = node->data;
		g_mime_iconv_close (cd);
		g_slist_free_1 (node);
		node = next;
	}
}
#endif /* TEST_CACHE */


struct {
	const char *text;
	const char *charset;
} tests[] = {
	{ "ÆtraflÄ±", "utf-8" },                /* az */
	{ "Äîáàâè Óñëóãà", "windows-cp1251" },      /* bg */
	{ "Cònjuge", "iso-8859-1" },                /* ca */
	{ "Avanceret søgning", "iso-8859-1" },      /* da */
	{ "Löschen", "iso-8859-1" },                /* de */
	{ "some text", "iso-8859-1" },              /* en */
	{ "päivää", "iso-8859-15" },                /* fi */
	{ "Modifié", "iso-8859-1" },                /* fr */
	{ "Tidéal", "iso-8859-1" },                 /* ga */
	{ "Fábrica", "iso-8859-1" },                /* gl */
	{ "Szem-Bélyhívó-A ", "iso-8859-2" },            /* hu */
	{ "Non c'é corrispondenza", "iso-8859-1" }, /* it */
	{ "$(B>e5i8!:w(B", "euc-jp" },                   /* ja */
	{ "$(C0m1^(B $(C0K;v(B", "euc-kr" },                  /* ko */
	{ "Iðsami paieðka", "iso-8859-13" },        /* lt */
	{ "Paplaðinâtâ Meklçðana", "iso-8859-13" }, /* lv */
	{ "Kopiëren", "iso-8859-15" },              /* nl */
	{ "Øydelagd Søk", "iso-8859-1" },           /* nn */
	{ "Avansert søk", "iso-8859-1" },           /* no */
	{ "-B¬ród³a-A ksi-B±¿ki-A adresowej", "iso-8859-2" }, /* pl */
	{ "C-Bãutare-A avansat-Bã-A ", "iso-8859-2" },      /* ro */
	{ "-LÀÐáèØàÕÝÝëÙ-A -LßÞØáÚ-A ", "koi8-r" },         /* ru */
	{ "PokroÄilÃ© hÄ¾adanie", "utf-8" },    /* sk */
	{ "Ga Å¾elite", "utf-8" },                  /* sl */
	{ "den ändå?", "iso-8859-1" },              /* sv */
	{ "Geli-Mþmiþ-A Arama", "iso-8859-9" },         /* tr */
	{ "õÄÏÓËÏÎÁÌÅÎÉÊ ÐÏÛÕË", "koi8-u" },        /* uk */
	
#if 0
	/* this is expected to fail */
	{ "é’ÉšŽå°‹æ(I>(B", "utf-8" },         /* zh_TW */
#endif
};


static void
test_utils (void)
{
	char *utf8, *native;
	iconv_t cd;
	int i;
	
	testsuite_start ("charset conversion utils");
	
	for (i = 0; i < G_N_ELEMENTS (tests); i++) {
		testsuite_check ("test #%d: %s to UTF-8", i, tests[i].charset);
		
		try {
			utf8 = native = NULL;
			
			if ((cd = g_mime_iconv_open ("UTF-8", tests[i].charset)) == (iconv_t) -1) {
				throw (exception_new ("could not open conversion for %s to UTF-8",
						      tests[i].charset));
			}
			
			utf8 = g_mime_iconv_strdup (cd, tests[i].text);
			g_mime_iconv_close (cd);
			
			if (utf8 == NULL) {
				throw (exception_new ("could not convert \"%s\" from %s to UTF-8",
						      tests[i].text, tests[i].charset));
			}
			
			if ((cd = g_mime_iconv_open (tests[i].charset, "UTF-8")) == (iconv_t) -1) {
				g_free (utf8);
				
				throw (exception_new ("could not open conversion for UTF-8 back to %s",
						      tests[i].charset));
			}
			
			native = g_mime_iconv_strdup (cd, utf8);
			g_mime_iconv_close (cd);
			g_free (utf8);
			
			if (native == NULL) {
				throw (exception_new ("could not convert \"%s\" from UTF-8 back to %s",
						      tests[i].text, tests[i].charset));
			} else if (strcmp (tests[i].text, native) != 0) {
				g_free (native);
				
				throw (exception_new ("strings did not match after conversion"));
			}
			
			testsuite_check_passed ();
			
			g_free (native);
		} catch (ex) {
			testsuite_check_failed ("test #%d failed: %s", i, ex->message);
		} finally;
	}
	
	testsuite_end ();
}

int main (int argc, char **argv)
{
	g_mime_iconv_init ();
	
	testsuite_init (argc, argv);
	
#ifdef TEST_CACHE
	ZenTimerStart (NULL);
	test_cache ();
	ZenTimerStop (NULL);
	ZenTimerReport (NULL, "test_cache()");
#endif
	
	test_utils ();
	
	g_mime_iconv_shutdown ();
	
	return testsuite_exit ();
}
