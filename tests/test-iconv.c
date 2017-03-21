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


struct {
	const char *text;
	const char *charset;
} tests[] = {
	{ "\xc6\x8ftrafl\xc4\xb1", "utf-8" },                                                      /* az */
	{ "\xc4\xee\xe1\xe0\xe2\xe8 \xd3\xf1\xeb\xf3\xe3\xe0", "windows-cp1251" },                 /* bg */
	{ "C\xf2njuge", "iso-8859-1" },                                                            /* ca */
	{ "Avanceret s\xf8gning", "iso-8859-1" },                                                  /* da */
	{ "L\xf6schen", "iso-8859-1" },                                                            /* de */
	{ "some text", "iso-8859-1" },                                                             /* en */
	{ "p\xe4iv\xe4\xe4", "iso-8859-15" },                                                      /* fi */
	{ "Modifi\xe9", "iso-8859-1" },                                                            /* fr */
	{ "Tid\xe9""al", "iso-8859-1" },                                                           /* ga */
	{ "F\xe1""brica", "iso-8859-1" },                                                          /* gl */
	{ "Szem\x1b-B\xe9lyh\xedv\xf3\x1b-A ", "iso-8859-2" },                                     /* hu */
	{ "Non c'\xe9 corrispondenza", "iso-8859-1" },                                             /* it */
	{ "\x1b$(B>e5i8!:w\x1b(B", "euc-jp" },                                                     /* ja */
	{ "\x1b$(C0m1^\x1b(B \x1b$(C0K;v\x1b(B", "euc-kr" },                                       /* ko */
	{ "I\xf0sami paie\xf0ka", "iso-8859-13" },                                                 /* lt */
	{ "Papla\xf0in\xe2t\xe2 Mekl\xe7\xf0""ana", "iso-8859-13" },                               /* lv */
	{ "Kopi\xebren", "iso-8859-15" },                                                          /* nl */
	{ "\xd8ydelagd S\xf8k", "iso-8859-1" },                                                    /* nn */
	{ "Avansert s\xf8k", "iso-8859-1" },                                                       /* no */
	{ "\x1b-B\xacr\xf3""d\xb3""a\x1b-A ksi\x1b-B\xb1\xbfki\x1b-A adresowej", "iso-8859-2" },   /* pl */
	{ "C\x1b-B\xe3utare\x1b-A avansat\x1b-B\xe3\x1b-A ", "iso-8859-2" },                       /* ro */
	{ "\x1b-L\xc0\xd0\xe1\xe8\xd8\xe0\xd5\xdd\xdd\xeb\xd9\x1b-A \x1b-L\xdf\xde\xd8\xe1\xda\x1b-A ", "koi8-r" }, /* ru */
	{ "Pokro\xc4\x8dil\xc3\xa9 h\xc4\xbe""adanie", "utf-8" },                                  /* sk */
	{ "Ga \xc5\xbe""elite", "utf-8" },                                                         /* sl */
	{ "den \xe4nd\xe5?", "iso-8859-1" },                                                       /* sv */
	{ "Geli\x1b-M\xfemi\xfe\x1b-A Arama", "iso-8859-9" },                                      /* tr */
	{ "\xf5\xc4\xcf\xd3\xcb\xcf\xce\xc1\xcc\xc5\xce\xc9\xca \xd0\xcf\xdb\xd5\xcb", "koi8-u" }, /* uk */
	
#if 0
	/* this is expected to fail */
	{ "\xe9\x92\xc9\x9a\x8e\xe5\xb0\x8b\xe6\x1b(I>\x1b(B", "utf-8" },                          /* zh_TW */
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
	g_mime_init ();
	
	testsuite_init (argc, argv);
	
	test_utils ();
	
	g_mime_shutdown ();
	
	return testsuite_exit ();
}
