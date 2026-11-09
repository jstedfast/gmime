/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*  GMime
 *  Copyright (C) Jeffrey Stedfast
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
#include <errno.h>

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

static void
test_utils_growth (void)
{
	char input[64], expected[sizeof (input) * 4 + 4] = { 0 }, *converted;
	GString *utf8, *iso2022;
	iconv_t cd;
	guint i;

	testsuite_check ("Conversion buffer growth preserves output and multibyte terminators");
	for (i = 0; i < sizeof (input); i++)
		expected[i * 4] = input[i] = 'A' + i % 26;
	cd = g_mime_iconv_open ("UTF-32LE", "UTF-8");
	g_assert_true (cd != (iconv_t) -1);
	converted = g_mime_iconv_strndup (cd, input, sizeof (input));
	g_assert_nonnull (converted);
	g_assert_cmpmem (converted, sizeof (expected), expected, sizeof (expected));
	g_free (converted);
	g_mime_iconv_close (cd);
	testsuite_check_passed ();

	testsuite_check ("Stateful conversion grows during flush and resets the descriptor for reuse");
	utf8 = g_string_new (NULL);
	iso2022 = g_string_new (NULL);
	/* Leave no output space for the final three-byte shift back to ASCII. */
	for (i = 0; i < 17; i++) {
		g_string_append (utf8, "\xe3\x81\x82" "A");
		g_string_append (iso2022, "\033$B$\"\033(BA");
	}
	g_string_append (utf8, "\xe3\x81\x82");
	g_string_append (iso2022, "\033$B$\"\033(B");
	cd = g_mime_iconv_open ("ISO-2022-JP", "UTF-8");
	g_assert_true (cd != (iconv_t) -1);
	for (i = 0; i < 2; i++) {
		converted = g_mime_iconv_strdup (cd, utf8->str);
		g_assert_nonnull (converted);
		g_assert_cmpstr (converted, ==, iso2022->str);
		g_free (converted);
	}
	g_mime_iconv_close (cd);
	g_string_free (iso2022, TRUE);
	g_string_free (utf8, TRUE);
	testsuite_check_passed ();
}

static void
test_utils_errors (void)
{
	const char binary[] = "a\0b";
	char *converted;
	iconv_t cd;

	testsuite_check ("Bounded conversion handles binary data, truncated input and invalid descriptors");
	converted = g_mime_iconv_strndup ((iconv_t) -1, "abc\xff", 3);
	g_assert_cmpstr (converted, ==, "abc");
	g_free (converted);
	cd = g_mime_iconv_open ("UTF-8", "UTF-8");
	g_assert_true (cd != (iconv_t) -1);
	converted = g_mime_iconv_strndup (cd, binary, sizeof (binary) - 1);
	g_assert_nonnull (converted);
	g_assert_cmpmem (converted, sizeof (binary), binary, sizeof (binary));
	g_free (converted);
	converted = g_mime_iconv_strdup (cd, "a\xc3");
	g_assert_cmpstr (converted, ==, "a");
	g_free (converted);
	g_mime_iconv_close (cd);
	testsuite_check_passed ();

	testsuite_check ("Conversion errors preserve errno and reset partially shifted descriptors");
	cd = g_mime_iconv_open ("ISO-2022-JP", "UTF-8");
	g_assert_true (cd != (iconv_t) -1);
#ifdef ENABLE_WARNINGS
	g_test_expect_message ("gmime", G_LOG_LEVEL_WARNING, "*g_mime_iconv_strndup:*");
#endif
	errno = 0;
	converted = g_mime_iconv_strdup (cd, "\xe3\x81\x82\xff");
	g_assert_null (converted);
	g_assert_cmpint (errno, ==, EILSEQ);
#ifdef ENABLE_WARNINGS
	g_test_assert_expected_messages ();
#endif
	converted = g_mime_iconv_strdup (cd, "A");
	g_assert_cmpstr (converted, ==, "A");
	g_free (converted);
	g_mime_iconv_close (cd);
	testsuite_check_passed ();
}

static void
test_utils_locale (void)
{
	char *converted;

	testsuite_check ("Locale conversion wrappers preserve ASCII and honor explicit byte lengths");
	converted = g_mime_iconv_locale_to_utf8 ("Subject: hello");
	g_assert_cmpstr (converted, ==, "Subject: hello");
	g_free (converted);
	converted = g_mime_iconv_utf8_to_locale ("Subject: hello");
	g_assert_cmpstr (converted, ==, "Subject: hello");
	g_free (converted);
	converted = g_mime_iconv_locale_to_utf8_length ("hello\xff", 5);
	g_assert_cmpstr (converted, ==, "hello");
	g_free (converted);
	converted = g_mime_iconv_utf8_to_locale_length ("hello\xff", 5);
	g_assert_cmpstr (converted, ==, "hello");
	g_free (converted);
	testsuite_check_passed ();
}

int main (int argc, char **argv)
{
	g_mime_init ();
	
	testsuite_init (argc, argv);
	
	test_utils ();
	testsuite_start ("charset conversion edge cases");
	test_utils_growth ();
	test_utils_errors ();
	test_utils_locale ();
	testsuite_end ();
	
	g_mime_shutdown ();
	
	return testsuite_exit ();
}
