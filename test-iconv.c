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
#include <stdlib.h>
#include "gmime-iconv.h"
#include "gmime-iconv-utils.h"


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

static int num_charsets = sizeof (charsets) / sizeof (charsets[0]);


static void
test_cache (void)
{
	GSList *node, *open_cds = NULL;
	iconv_t cd;
	int i;
	
	for (i = 0; i < 500; i++) {
		const char *from, *to;
		int which;
		
		which = rand () % num_charsets;
		from = charsets[which];
		which = rand () % num_charsets;
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
		cd = node->data;
		g_mime_iconv_close (cd);
		node = node->next;
	}
	
	g_slist_free (open_cds);
}


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
	{ "é’ÉšŽå°‹æ(I>(B", "utf-8" },         /* zh_TW */
	{ NULL, NULL }
};


static void
test_utils (void)
{
	char *utf8, *native;
	iconv_t cd;
	int i;
	
	for (i = 0; tests[i].text; i++) {
		cd = g_mime_iconv_open ("UTF-8", tests[i].charset);
		if (cd != (iconv_t) -1) {
			utf8 = g_mime_iconv_strdup (cd, tests[i].text);
			g_mime_iconv_close (cd);
			if (utf8 == NULL) {
				g_warning ("tests[%d]: failed to convert \"%s\" to UTF-8 from %s",
					   i, tests[i].text, tests[i].charset);
				continue;
			}
			
			cd = g_mime_iconv_open (tests[i].charset, "UTF-8");
			if (cd != (iconv_t) -1) {
				native = g_mime_iconv_strdup (cd, utf8);
				g_mime_iconv_close (cd);
				if (!native) {
					g_warning ("tests[%d]: failed to convert \"%s\" to %s from UTF-8",
						   i, tests[i].text, tests[i].charset);
				} else if (strcmp (tests[i].text, native)) {
					g_warning ("tests[%d]: there seems to have been some lossage\n"
						   "in the conversion back to the native charset:\n"
						   "\"%s\" != \"%s\"", i, tests[i].text, native);
				}
				
				g_free (native);
			} else {
				g_warning ("tests[%d]: failed to open conversion descriptor for UTF-8 to %s",
					   i, tests[i].charset);
			}
			
			g_free (utf8);
		} else {
			g_warning ("tests[%d]: failed to open conversion descriptor for %s to UTF-8",
				   i, tests[i].charset);
		}
	}
}

int main (int argc, char **argv)
{
	g_mime_iconv_init ();
	
	/*test_cache ();*/
	
	test_utils ();
}
