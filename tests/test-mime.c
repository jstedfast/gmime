/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*  GMime
 *  Copyright (C) 2000-2022 Jeffrey Stedfast
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
#include <ctype.h>

#include <gmime/gmime.h>

#include "testsuite.h"

extern int verbose;

#define v(x) if (verbose > 3) x

#if 0
static void
uputs (const char *str, FILE *out)
{
	register const unsigned char *s = (const unsigned char *) str;
	char buf[4096], *d = buf;
	
	while (*s != '\0') {
		if (!isascii ((int) *s)) {
			d += sprintf (d, "\\x%.2x", *s);
			if (isxdigit ((int) s[1])) {
				*d++ = '"';
				*d++ = ' ';
				*d++ = '"';
			}
		} else
			*d++ = (char) *s;
		s++;
	}
	
	*d = '\0';
	
	fputs (buf, out);
}
#endif

static struct {
	const char *input;
	const char *charset;
	const char *display;
	const char *encoded;
} addrspec[] = {
	{ "fejj@helixcode.com", NULL,
	  "fejj@helixcode.com",
	  "fejj@helixcode.com" },
	{ "Jeffrey Stedfast <fejj@helixcode.com>", NULL,
	  "Jeffrey Stedfast <fejj@helixcode.com>",
	  "Jeffrey Stedfast <fejj@helixcode.com>" },
	{ "Jeffrey \"fejj\" Stedfast <fejj@helixcode.com>", NULL,
	  "Jeffrey fejj Stedfast <fejj@helixcode.com>",
	  "Jeffrey fejj Stedfast <fejj@helixcode.com>" },
	{ "\"Jeffrey \\\"fejj\\\" Stedfast\" <fejj@helixcode.com>", NULL,
	  "Jeffrey \"fejj\" Stedfast <fejj@helixcode.com>",
	  "\"Jeffrey \\\"fejj\\\" Stedfast\" <fejj@helixcode.com>" },
	{ "\"Stedfast, Jeffrey\" <fejj@helixcode.com>", NULL,
	  "\"Stedfast, Jeffrey\" <fejj@helixcode.com>",
	  "\"Stedfast, Jeffrey\" <fejj@helixcode.com>" },
	{ "fejj@helixcode.com (Jeffrey Stedfast)", NULL,
	  "Jeffrey Stedfast <fejj@helixcode.com>",
	  "Jeffrey Stedfast <fejj@helixcode.com>" },
	{ "Jeff <fejj(recursive (comment) block)@helixcode.(and a comment here)com>", NULL,
	  "Jeff <fejj@helixcode.com>",
	  "Jeff <fejj@helixcode.com>" },
	{ "=?iso-8859-1?q?Kristoffer_Br=E5nemyr?= <ztion@swipenet.se>", "iso-8859-1",
	  "Kristoffer Br\xc3\xa5nemyr <ztion@swipenet.se>",
	  "Kristoffer =?iso-8859-1?q?Br=E5nemyr?= <ztion@swipenet.se>" },
	{ "fpons@mandrakesoft.com (=?iso-8859-1?q?Fran=E7ois?= Pons)", "iso-8859-1",
	  "Fran\xc3\xa7ois Pons <fpons@mandrakesoft.com>",
	  "=?iso-8859-1?q?Fran=E7ois?= Pons <fpons@mandrakesoft.com>" },
	{ "GNOME Hackers: miguel@gnome.org (Miguel de Icaza), Havoc Pennington <hp@redhat.com>;, fejj@helixcode.com", NULL,
	  "GNOME Hackers: Miguel de Icaza <miguel@gnome.org>, Havoc Pennington <hp@redhat.com>;, fejj@helixcode.com",
	  "GNOME Hackers: Miguel de Icaza <miguel@gnome.org>, Havoc Pennington <hp@redhat.com>;, fejj@helixcode.com" },
	{ "Local recipients: phil, joe, alex, bob", NULL,
	  "Local recipients: phil, joe, alex, bob;",
	  "Local recipients: <phil>, <joe>, <alex>, <bob>;" },
	{ "\":sysmail\"@  Some-Group. Some-Org,\n Muhammed.(I am  the greatest) Ali @(the)Vegas.WBA", NULL,
	  "\":sysmail\"@Some-Group.Some-Org, Muhammed.Ali@Vegas.WBA",
	  "\":sysmail\"@Some-Group.Some-Org, Muhammed.Ali@Vegas.WBA" },
	{ "Charles S. Kerr <charles@foo.com>", NULL,
	  "\"Charles S. Kerr\" <charles@foo.com>",
	  "\"Charles S. Kerr\" <charles@foo.com>" },
	{ "Charles \"Likes, to, put, commas, in, quoted, strings\" Kerr <charles@foo.com>", NULL,
	  "\"Charles Likes, to, put, commas, in, quoted, strings Kerr\" <charles@foo.com>",
	  "\"Charles Likes, to, put, commas, in, quoted, strings Kerr\" <charles@foo.com>" },
	{ "Charles Kerr, Pan Programmer <charles@superpimp.org>", NULL,
	  "\"Charles Kerr, Pan Programmer\" <charles@superpimp.org>",
	  "\"Charles Kerr, Pan Programmer\" <charles@superpimp.org>" },
	{ "Charles Kerr <charles@[127.0.0.1]>", NULL,
	  "Charles Kerr <charles@[127.0.0.1]>",
	  "Charles Kerr <charles@[127.0.0.1]>" },
	{ "Charles <charles@[127..0.1]>", NULL,
	  "Charles <charles@[127..0.1]>",
	  "Charles <charles@[127..0.1]>" },
	{ "Charles,, likes illegal commas <charles@superpimp.org>", NULL,
	  "Charles, likes illegal commas <charles@superpimp.org>",
	  "<Charles>, likes illegal commas <charles@superpimp.org>" },
	{ "<charles@broken.host.com.>", NULL,
	  "charles@broken.host.com",
	  "charles@broken.host.com" },
	{ "fpons@mandrakesoft.com (=?iso-8859-1?q?Fran=E7ois?= Pons likes _'s and 	's too)", "iso-8859-1",
	  "Fran\xc3\xa7ois Pons likes _'s and  's too <fpons@mandrakesoft.com>",
	  "=?iso-8859-1?q?Fran=E7ois?= Pons likes _'s and  's too <fpons@mandrakesoft.com>" },
	{ "T\x81\xf5ivo Leedj\x81\xe4rv <leedjarv@interest.ee>", NULL,
	  "T\xc2\x81\xc3\xb5ivo Leedj\xc2\x81\xc3\xa4rv <leedjarv@interest.ee>",
	  "=?utf-8?b?VMKBw7Vpdm8gTGVlZGrCgcOkcnY=?= <leedjarv@interest.ee>" },
	{ "fbosi@mokabyte.it;, rspazzoli@mokabyte.it", NULL,
	  "fbosi@mokabyte.it, rspazzoli@mokabyte.it",
	  "fbosi@mokabyte.it, rspazzoli@mokabyte.it" },
	{ "\"Miles (Star Trekkin) O'Brian\" <mobrian@starfleet.org>", NULL,
	  "\"Miles (Star Trekkin) O'Brian\" <mobrian@starfleet.org>",
	  "\"Miles (Star Trekkin) O'Brian\" <mobrian@starfleet.org>" },
	{ "undisclosed-recipients: ;", NULL,
	  "undisclosed-recipients: ;",
	  "undisclosed-recipients: ;" },
	{ "undisclosed-recipients:;", NULL,
	  "undisclosed-recipients: ;",
	  "undisclosed-recipients: ;" },
	{ "undisclosed-recipients:", NULL,
	  "undisclosed-recipients: ;",
	  "undisclosed-recipients: ;" },
	{ "\"\":;", NULL,
	  ": ;",
	  "\"\": ;" },
	{ "\"\": a@example.com;", NULL,
	  ": a@example.com;",
	  "\"\": a@example.com;" },
	{ "Outer: \"\": a@example.com;;", NULL,
	  "Outer: : a@example.com;;",
	  "Outer: \"\": a@example.com;;" },
	{ "\"\": \"\": a@example.com;;", NULL,
	  ": : a@example.com;;",
	  "\"\": \"\": a@example.com;;" },
	{ "\"\": \"\": \"\": a@example.com;;;", NULL,
	  ": : : a@example.com;;;",
	  "\"\": \"\": \"\": a@example.com;;;" },
	{ "undisclosed-recipients", NULL,
	  "undisclosed-recipients",
	  "<undisclosed-recipients>" },
	/* The following test case is to check that we properly handle
	 * mailbox addresses that do not have any lwsp between the
	 * name component and the addr-spec. See Evolution bug
	 * #347520 */
	{ "Canonical Patch Queue Manager<pqm@pqm.ubuntu.com>", NULL,
	  "Canonical Patch Queue Manager <pqm@pqm.ubuntu.com>",
	  "Canonical Patch Queue Manager <pqm@pqm.ubuntu.com>" },
	/* Some examples pulled from rfc5322 */
	{ "Pete(A nice \\) chap) <pete(his account)@silly.test(his host)>", NULL,
	  "Pete <pete@silly.test>",
	  "Pete <pete@silly.test>" },
	{ "A Group(Some people):Chris Jones <c@(Chris's host.)public.example>, joe@example.org, John <jdoe@one.test> (my dear friend); (the end of the group)", NULL,
	  "A Group: Chris Jones <c@public.example>, joe@example.org, John <jdoe@one.test>;",
	  "A Group: Chris Jones <c@public.example>, joe@example.org, John <jdoe@one.test>;" },
	/* odd number of double quotes in rfc-2047 encoded display-name */
	{ "=?UTF-8?Q?foo=20=22=20bar?= <test@mail.net>", "utf-8",
	  "\"foo \\\" bar\" <test@mail.net>",
	  "\"foo \\\" bar\" <test@mail.net>"},
	/* The following tests cases are meant to test forgivingness
	 * of the parser when it encounters unquoted specials in the
	 * name component */
	{ "Warren Worthington, Jr. <warren@worthington.com>", NULL,
	  "\"Warren Worthington, Jr.\" <warren@worthington.com>",
	  "\"Warren Worthington, Jr.\" <warren@worthington.com>" },
	{ "dot.com <dot.com>", NULL,
	  "\"dot.com\" <dot.com>",
	  "\"dot.com\" <dot.com>" },
	{ "=?UTF-8?Q?agatest123_\"test\"?= <agatest123@o2.pl>", "utf-8",
	  "agatest123 test <agatest123@o2.pl>",
	  "agatest123 test <agatest123@o2.pl>" },
	{ "\"=?ISO-8859-2?Q?TEST?=\" <p@p.org>", "iso-8859-2",
	  "TEST <p@p.org>",
	  "TEST <p@p.org>" },
	{ "sdfasf@wp.pl,c tert@wp.pl,sffdg.rtre@op.pl", NULL,
	  "sdfasf@wp.pl, sffdg.rtre@op.pl",
	  "sdfasf@wp.pl, sffdg.rtre@op.pl" },
	
	/* obsolete routing address syntax tests */
	{ "<@route:user@domain.com>", NULL,
	  "<@route:user@domain.com>",
	  "<@route:user@domain.com>" },
	{ "<@route1,,@route2,,,@route3:user@domain.com>", NULL,
	  "<@route1,@route2,@route3:user@domain.com>",
	  "<@route1,@route2,@route3:user@domain.com>" },
};

static struct {
	const char *input;
	const char *charset;
	const char *display;
	const char *encoded;
} broken_addrspec[] = {
	{ "\"Biznes=?ISO-8859-2?Q?_?=INTERIA.PL\"=?ISO-8859-2?Q?_?=<biuletyny@firma.interia.pl>", "iso-8859-2",
	  "\"Biznes INTERIA.PL\" <biuletyny@firma.interia.pl>",
	  "\"Biznes INTERIA.PL\" <biuletyny@firma.interia.pl>", },
	/* UTF-8 sequence split between multiple encoded-word tokens */
	{ "=?utf-8?Q?{#D=C3=A8=C3=A9=C2=A3=C3=A5=C3=BD_M$=C3=A1=C3?= =?utf-8?Q?=AD.=C3=A7=C3=B8m}?= <user@domain.com>", "utf-8",
	  "\"{#Dèé£åý M$áí.çøm}\" <user@domain.com>",
	  "=?utf-8?b?eyNEw6jDqcKjw6XDvSBNJMOhw60uw6fDuG19?= <user@domain.com>" },
	/* quoted-printable payload split between multiple encoded-word tokens */
	{ "=?utf-8?Q?{#D=C3=A8=C3=A9=C2=?= =?utf-8?Q?A3=C3=A5=C3=BD_M$=C3=A1=C?= =?utf-8?Q?3=AD.=C3=A7=C3=B8m}?= <user@domain.com>", "utf-8",
	  "\"{#Dèé£åý M$áí.çøm}\" <user@domain.com>",
	  "=?utf-8?b?eyNEw6jDqcKjw6XDvSBNJMOhw60uw6fDuG19?= <user@domain.com>" },
	/* base64 payload split between multiple encoded-word tokens */
	{ "=?iso-8859-1?b?ey?= =?iso-8859-1?b?NE6Omj5f0gTSTh7S7n+G1AI30=?= <user@domain.com>", "iso-8859-1",
	  "\"{#Dèé£åý M$áí.çøm@#}\" <user@domain.com>",
	  "=?iso-8859-1?b?eyNE6Omj5f0gTSTh7S7n+G1AI30=?= <user@domain.com>" },
};

static void
test_addrspec (GMimeParserOptions *options, gboolean test_broken)
{
	GMimeFormatOptions *format = g_mime_format_options_get_default ();
	GMimeInternetAddressList *addrlist;
	GMimeInternetAddress *address;
	const char *charset;
	char *str;
	guint i;
	
	for (i = 0; i < G_N_ELEMENTS (addrspec); i++) {
		addrlist = NULL;
		str = NULL;
		
		testsuite_check ("addrspec[%u]", i);
		try {
			if (!(addrlist = g_mime_internet_address_list_parse (options, addrspec[i].input)))
				throw (exception_new ("could not parse: %s", addrspec[i].input));
			
			if (!(address = g_mime_internet_address_list_get_address (addrlist, 0)))
				throw (exception_new ("could not get first address: %s", addrspec[i].input));
			
			charset = g_mime_internet_address_get_charset (address);
			if (addrspec[i].charset != NULL) {
				if (charset == NULL)
					throw (exception_new ("expected '%s' but got NULL charset: %s", addrspec[i].charset, addrspec[i].input));
				if (g_ascii_strcasecmp (addrspec[i].charset, charset) != 0)
					throw (exception_new ("expected '%s' but got '%s' charset: %s", addrspec[i].charset, charset, addrspec[i].input));
			} else if (charset != NULL) {
				throw (exception_new ("expected NULL charset but address has a charset of '%s': %s", charset, addrspec[i].input));
			}
			
			str = g_mime_internet_address_list_to_string (addrlist, format, FALSE);
			if (strcmp (addrspec[i].display, str) != 0)
				throw (exception_new ("display strings do not match.\ninput: %s\nexpected: %s\nactual: %s", addrspec[i].input, addrspec[i].display, str));
			g_free (str);
			
			str = g_mime_internet_address_list_to_string (addrlist, format, TRUE);
			if (strcmp (addrspec[i].encoded, str) != 0)
				throw (exception_new ("encoded strings do not match.\nexpected: %s\nactual: %s", addrspec[i].encoded, str));
			
			testsuite_check_passed ();
		} catch (ex) {
			testsuite_check_failed ("addrspec[%u]: %s", i, ex->message);
		} finally;
		
		g_free (str);
		if (addrlist)
			g_object_unref (addrlist);
	}
	
	if (test_broken) {
		for (i = 0; i < G_N_ELEMENTS (broken_addrspec); i++) {
			addrlist = NULL;
			str = NULL;
			
			testsuite_check ("broken_addrspec[%u]", i);
			try {
				if (!(addrlist = g_mime_internet_address_list_parse (options, broken_addrspec[i].input)))
					throw (exception_new ("could not parse: %s", broken_addrspec[i].input));

				if (!(address = g_mime_internet_address_list_get_address (addrlist, 0)))
					throw (exception_new ("could not get first address: %s", broken_addrspec[i].input));
				
				charset = g_mime_internet_address_get_charset (address);
				if (broken_addrspec[i].charset != NULL) {
					if (charset == NULL)
						throw (exception_new ("expected '%s' but got NULL charset: %s", broken_addrspec[i].charset, broken_addrspec[i].input));
					if (g_ascii_strcasecmp (broken_addrspec[i].charset, charset) != 0)
						throw (exception_new ("expected '%s' but got '%s' charset: %s", broken_addrspec[i].charset, charset, broken_addrspec[i].input));
				} else if (charset != NULL) {
					throw (exception_new ("expected NULL charset but address has a charset of '%s': %s", charset, broken_addrspec[i].input));
				}
				
				str = g_mime_internet_address_list_to_string (addrlist, format, FALSE);
				if (strcmp (broken_addrspec[i].display, str) != 0)
					throw (exception_new ("display strings do not match.\ninput: %s\nexpected: %s\nactual: %s", broken_addrspec[i].input, broken_addrspec[i].display, str));
				g_free (str);
				
				str = g_mime_internet_address_list_to_string (addrlist, format, TRUE);
				if (strcmp (broken_addrspec[i].encoded, str) != 0)
					throw (exception_new ("encoded strings do not match.\nexpected: %s\nactual: %s", broken_addrspec[i].encoded, str));
				
				testsuite_check_passed ();
			} catch (ex) {
				testsuite_check_failed ("broken_addrspec[%u]: %s", i, ex->message);
			} finally;
			
			g_free (str);
			if (addrlist)
				g_object_unref (addrlist);
		}
	}
}


static void
test_address_name_whitespace (GMimeParserOptions *options)
{
	static const struct {
		const char *input, *name;
	} cases[] = {
		{ "First\tLast <user@example.com>", "First Last" },
		{ "\"First\tLast\" <user@example.com>", "First Last" },
		{ "\"First\t\t Last\" <user@example.com>", "First   Last" },
		{ "\"First\\\tLast\" <user@example.com>", "First Last" },
		{ "\"First\r\n\tLast\" <user@example.com>", "First Last" },
		{ "\"\\\"First\\\"\tLast\" <user@example.com>", "\"First\" Last" },
		{ "\"First\\\\\tLast\" <user@example.com>", "First\\ Last" },
		{ "\"First\tLast\": user@example.com;", "First Last" },
		{ "First\tLast:;", "First Last" },
		{ "user@example.com (First\tLast)", "First Last" },
		{ "user (First\tLast)", "First Last" },
		{ "user (First Last)", "First Last" },
		{ "user.name (First\tLast)", "First Last" },
		{ "\"user name\" (First\tLast)", "First Last" },
		{ "user (part).name (First\tLast)", "First Last" },
		{ "user@example.com (First\t\t Last)", "First   Last" },
		{ "user@example.com (First\r\n\tLast)", "First\r\n Last" },
		{ "user@example.com (\"First\"\tLast)", "\"First\" Last" },
		{ "user (\"First\"\tLast)", "\"First\" Last" },
		{ "user@example.com (First\\\tLast)", "First\\ Last" },
		{ "user (First\\\tLast)", "First\\ Last" },
		{ "\"\xc3\xa9\tLast\" <user@example.com>", "\xc3\xa9 Last" },
		{ "user@example.com (\xc3\xa9\tLast)", "\xc3\xa9 Last" },
		{ "=?utf-8?q?First=09Last?= <user@example.com>", "First\tLast" },
		{ "\"=?utf-8?q?First=09Last?=\" <user@example.com>", "First\tLast" },
		{ "=?utf-8?q?First=09Last?=: user@example.com;", "First\tLast" },
		{ "user@example.com (=?utf-8?q?First=09Last?=)", "First\tLast" },
		{ "user (=?utf-8?q?First=09Last?=)", "First\tLast" },
		{ "\"First\t=?utf-8?q?Middle=09Last?=\" <user@example.com>", "First Middle\tLast" },
		{ "=?utf-8?q?First?=\t=?utf-8?q?Last?= <user@example.com>", "FirstLast" },
		{ "\"  John Smith  \" <user@example.com>", "John Smith" },
		{ "\" =?utf-8?q?John_Smith?= \" <user@example.com>", "John Smith" },
		{ "user@example.com (  John Smith  )", "John Smith" },
		{ "\"\302\240John Smith\342\200\203\" <user@example.com>", "\302\240John Smith\342\200\203" },
		{ "user@example.com (\302\240John Smith\342\200\203)", "\302\240John Smith\342\200\203" },
		{ "\"=?utf-8?q?=C2=85John=09Smith=C2=A0?=\" <user@example.com>", "\302\205John\tSmith\302\240" },
		{ "\"\342\200\203Group\342\200\203\": user@example.com;", "\342\200\203Group\342\200\203" },
		{ "\"\302\240\": user@example.com;", "\302\240" }
	};
	GMimeParserOptions *parser = g_mime_parser_options_clone (options);
	GMimeInternetAddressList *addresses;
	GMimeInternetAddress *address;
	char *unquoted;
	guint i;

	g_mime_parser_options_set_address_compliance_mode (parser,
		g_mime_parser_options_get_rfc2047_compliance_mode (options));
	for (i = 0; i < G_N_ELEMENTS (cases); i++) {
		addresses = g_mime_internet_address_list_parse (parser, cases[i].input);
		testsuite_check ("parsed address name whitespace[%u]", i);
		try {
			if (!addresses || g_mime_internet_address_list_length (addresses) != 1)
				throw (exception_new ("could not parse address whitespace fixture"));
			address = g_mime_internet_address_list_get_address (addresses, 0);
			if (g_strcmp0 (g_mime_internet_address_get_name (address), cases[i].name))
				throw (exception_new ("unexpected parsed display name: %s",
						      g_mime_internet_address_get_name (address)));
			testsuite_check_passed ();
		} catch (ex) {
			testsuite_check_failed ("%s", ex->message);
		} finally;
		g_clear_object (&addresses);
	}

	unquoted = g_strdup ("\"First\tLast\"");
	g_mime_utils_unquote_string (unquoted);
	testsuite_check ("general string unquoting retains tabs");
	if (strcmp (unquoted, "First\tLast"))
		testsuite_check_failed ("address normalization changed general string unquoting");
	else
		testsuite_check_passed ();
	g_free (unquoted);
	g_mime_parser_options_free (parser);
}

static struct {
	const char *in;
	const char *out;
	time_t date;
	int tzone;
} dates[] = {
	{ "Mon, 17 Jan 1994 11:14:55 -0500",
	  "Mon, 17 Jan 1994 11:14:55 -0500",
	  758823295, -500 },
	{ "Mon, 17 Jan 01 11:14:55 -0500",
	  "Wed, 17 Jan 2001 11:14:55 -0500",
	  979748095, -500 },
	{ "Tue, 30 Mar 2004 13:01:38 +0000",
	  "Tue, 30 Mar 2004 13:01:38 +0000",
	  1080651698, 0 },
	{ "Sat Mar 24 21:23:03 EDT 2007",
	  "Sat, 24 Mar 2007 21:23:03 -0400",
	  1174785783, -400 },
	{ "Sat, 24 Mar 2007 21:23:03 EDT",
	  "Sat, 24 Mar 2007 21:23:03 -0400",
	  1174785783, -400 },
	{ "Sat, 24 Mar 2007 21:23:03 GMT",
	  "Sat, 24 Mar 2007 21:23:03 +0000",
	  1174771383, 0 },
	{ "17-6-2008 17:10:08",
	  "Tue, 17 Jun 2008 17:10:08 +0000",
	  1213722608, 0 },
	{ "Sat, 28 Oct 2017 19:41:29 -0001",
	  "Sat, 28 Oct 2017 19:41:29 -0001",
	  1509219749, -1 },
	{ "nonsense",
	  "Thu, 01 Jan 1970 00:00:00 +0000",
	  0, 0 }
};

static void
test_date_parser (void)
{
	GDateTime *date = NULL;
	Exception *newex;
	int tz_offset;
	GTimeSpan tz;
	time_t time;
	char *buf;
	int sign;
	guint i;
	
	for (i = 0; i < G_N_ELEMENTS (dates); i++) {
		testsuite_check ("Date: '%s'", dates[i].in);
		try {
			if (!(date = g_mime_utils_header_decode_date (dates[i].in))) {
				if (dates[i].date != 0)
					throw (exception_new ("failed to parse date: %s", dates[i].in));
				testsuite_check_passed ();
				continue;
			}
			
			time = (time_t) g_date_time_to_unix (date);
			tz = g_date_time_get_utc_offset (date);
			
			sign = tz < 0 ? -1 : 1;
			tz *= sign;
			
			tz_offset = 100 * (tz / G_TIME_SPAN_HOUR);
			tz_offset += (tz % G_TIME_SPAN_HOUR) / G_TIME_SPAN_MINUTE;
			tz_offset *= sign;
			
			if (time != dates[i].date)
				throw (exception_new ("time_t's do not match: actual: %ld; expected: %ld", time, dates[i].date));
			
			if (tz_offset != dates[i].tzone)
				throw (exception_new ("timezones do not match"));
			
			buf = g_mime_utils_header_format_date (date);
			if (strcmp (dates[i].out, buf) != 0) {
				newex = exception_new ("date strings do not match: actual: %s; expected: %s", buf, dates[i].out);
				g_free (buf);
				throw (newex);
			}
			
			g_free (buf);
			
			testsuite_check_passed ();
		} catch (ex) {
			testsuite_check_failed ("Date: '%s': %s", dates[i].in, ex->message);
		} finally;
		g_clear_pointer (&date, g_date_time_unref);
	}
}

static void
test_date_formatting (void)
{
	GMimeMessage *message = g_mime_message_new (FALSE);
	GDateTime *date = g_date_time_new_utc (2025, 2, 28, 9, 0, 0);
	GTimeZone *zone;
	const char *stored;
	char *formatted;
	guint i;

	g_mime_message_set_date (message, date);
	g_date_time_unref (date);
	stored = g_mime_entity_get_header (GMIME_ENTITY (message), "Date");
	for (i = 0; i < 2; i++) {
		zone = g_time_zone_new_offset (i ? -37 : 37);
		date = i ? g_date_time_new (zone, 9999, 12, 31, 23, 59, 59) :
			g_date_time_new (zone, 1, 1, 1, 0, 0, 0);
		g_time_zone_unref (zone);
		testsuite_check ("Out-of-range UTC formatting at year %d", g_date_time_get_year (date));
		g_test_expect_message ("gmime", G_LOG_LEVEL_CRITICAL, "*utc != NULL*");
		formatted = g_mime_utils_header_format_date (date);
		g_test_assert_expected_messages ();
		g_test_expect_message ("gmime", G_LOG_LEVEL_CRITICAL, "*utc != NULL*");
		g_mime_message_set_date (message, date);
		g_test_assert_expected_messages ();
		if (!formatted && g_mime_entity_get_header (GMIME_ENTITY (message), "Date") == stored &&
		    g_date_time_get_year (g_mime_message_get_date (message)) == 2025)
			testsuite_check_passed ();
		else
			testsuite_check_failed ("unrepresentable dates must not replace the existing Date header");
		g_free (formatted);
		g_date_time_unref (date);
	}
	g_object_unref (message);
}

static void
test_date_recovery (void)
{
	static const struct {
		const char *input, *expected;
	} cases[] = {
		{ "Sun, 08 Dec 91 09:11:00 +0000", "Sun, 08 Dec 1991 09:11:00 +0000" },
		{ "8 Dec 1991 09:11 (Sunday)", "Sun, 08 Dec 1991 09:11:00 +0000" },
		{ "Tue, 9 Jun 92 03:45:24 JST", "Tue, 09 Jun 1992 03:45:24 +0900" },
		{ "Fri, 28 Feb 2025 09:00:00 UTC", "Fri, 28 Feb 2025 09:00:00 +0000" },
		{ "Fri, 28 Feb 2025 09:00:00 KST", "Fri, 28 Feb 2025 09:00:00 +0900" },
		{ "FRI, 30 NOV 2012 02:09:10 +0100", "Fri, 30 Nov 2012 02:09:10 +0100" },
		{ "Tue, 21 April 15 14:44:51 GMT", "Tue, 21 Apr 2015 14:44:51 +0000" },
		{ "Wed, 6 Aug 2014 01:53:48 -2200", "Wed, 06 Aug 2014 01:53:48 +0000" },
		{ "Tue, 12 Jun 2012 19:22:28 0200", "Tue, 12 Jun 2012 19:22:28 +0200" },
		{ "Fri, 8 May 2015", "Fri, 08 May 2015 00:00:00 +0000" },
		{ "Fri, 8 May 2015 12", "Fri, 08 May 2015 00:00:00 +0000" },
		{ "Fri, 8 May 2015 12:05", "Fri, 08 May 2015 12:05:00 +0000" },
		{ "Fri, 8 May 2015 12:05:01 400", "Fri, 08 May 2015 12:05:01 +0400" },
		{ "Sat, 9 May 2015 24:00:00 -0400", "Sat, 09 May 2015 00:00:00 -0400" },
		{ "2015 May 9 25:99:78 -0400", "Sat, 09 May 2015 00:00:00 -0400" },
		{ "Sat, Sep 10 77 12:59:19 0400", "Sat, 10 Sep 1977 12:59:19 +0400" },
		{ "Mon, 28 Feb 2025 09:00:00 -0500", "Fri, 28 Feb 2025 09:00:00 -0500" },
		{ "Fri, 28 Feb 2025 12:15:00 AM -0500", "Fri, 28 Feb 2025 00:15:00 -0500" },
		{ "Fri, 28 Feb 2025 11:30:00 PM -0500", "Fri, 28 Feb 2025 23:30:00 -0500" },
		{ "Feb 28 2025 12:15:00 AM -0500", "Fri, 28 Feb 2025 00:15:00 -0500" },
		{ "Feb 28 2025 11:30:00 PM -0500", "Fri, 28 Feb 2025 23:30:00 -0500" },
		{ "31 Dec 2024 10:15:00 AM", "Tue, 31 Dec 2024 10:15:00 +0000" },
		{ "31 Dec 2024 10:15:00 PM", "Tue, 31 Dec 2024 22:15:00 +0000" },
		{ "Fri, 28 Feb 2025 23:59:60 -0500", "Fri, 28 Feb 2025 23:59:59 -0500" },
		{ "Fri, 28 Feb 2025 22:59:60 -0500", "Fri, 28 Feb 2025 00:00:00 -0500" },
		{ "Fri, 28 Feb 2025 23:59:61 -0500", "Fri, 28 Feb 2025 00:00:00 -0500" },
		{ "Fri, 28 Feb 2025 23:60:59 -0500", "Fri, 28 Feb 2025 00:00:00 -0500" },
		{ "Fri, 28 Feb 2025 999999999999999:00:00 -0500", "Fri, 28 Feb 2025 00:00:00 -0500" },
		{ "Wed, 25 Mar 1992 00:00:00 -1192", "Wed, 25 Mar 1992 00:00:00 +0000" },
		{ "Wed, 25 Mar 1992 00:00:00 +0060", "Wed, 25 Mar 1992 00:00:00 +0000" },
		{ "Wed, 25 Mar 1992 00:00:00 +1399", "Wed, 25 Mar 1992 00:00:00 +0000" },
		{ "Wed, 25 Mar 1992 00:00:00 +0059", "Wed, 25 Mar 1992 00:00:00 +0059" },
		{ "Wed, 25 Mar 1992 00:00:00 -1159", "Wed, 25 Mar 1992 00:00:00 -1159" },
		{ "Fri, 28 Feb 2025 09:00:00 +1400", "Fri, 28 Feb 2025 09:00:00 +1400" },
		{ "Fri, 28 Feb 2025 09:00:00 -1200", "Fri, 28 Feb 2025 09:00:00 -1200" },
		{ "Fri, 28 Feb 2025 09:00:00 +1401", "Fri, 28 Feb 2025 09:00:00 +0000" },
		{ "Fri, 28 Feb 2025 09:00:00 -1201", "Fri, 28 Feb 2025 09:00:00 +0000" },
		{ "Fri, 28 Feb 2025 09:00:00 -1234567890123456789", "Fri, 28 Feb 2025 09:00:00 +0000" },
		{ "Fri, 28 Feb 2025 09:00:00 1234567890123456789", "Fri, 28 Feb 2025 09:00:00 +0000" },
		{ "Fri, 28 Feb 2025 09:00:00 XYZ", "Fri, 28 Feb 2025 09:00:00 +0000" },
		{ "(nested (comment)) 28 (day) Feb (month) 2025 11:30 PM -0330", "Fri, 28 Feb 2025 23:30:00 -0330" },
		{ "28 Feb 2025 11:30 (escaped\\) comment) PM -0330", "Fri, 28 Feb 2025 23:30:00 -0330" },
		{ "28 Feb 2025 (unfinished", "Fri, 28 Feb 2025 00:00:00 +0000" },
		{ "2025-02-28", "Fri, 28 Feb 2025 00:00:00 +0000" },
		{ "28-02-2025", "Fri, 28 Feb 2025 00:00:00 +0000" },
		{ "Fri, 28 Feb 2025 :09:00 -0500", "Fri, 28 Feb 2025 00:00:00 -0500" },
		{ "Fri, 28 Feb 2025 09::00 -0500", "Fri, 28 Feb 2025 00:00:00 -0500" },
		{ "Fri, 28 Feb 2025 09:00: -0500", "Fri, 28 Feb 2025 00:00:00 -0500" },
		{ "Thu, 01 Jan 1970 00:00:00 +0000", "Thu, 01 Jan 1970 00:00:00 +0000" },
		{ "31 Dec 9999 23:59:59 +1400", "Fri, 31 Dec 9999 23:59:59 +1400" },
		{ "31 Dec 9999 23:59:59 -0001", NULL },
		{ "Fri, 31 Feb 2025 09:00:00 -0500", NULL },
		{ "Feb 31 2025 09:00:00 -0500", NULL },
		{ "999999999999 Jan 2025 09:00:00 -0500", NULL },
		{ "Fri, 28 Feb 10000 09:00:00 +0000", NULL },
		{ "Fri, 28 Feb 1968 09:00:00 +0000", NULL },
		{ "(unfinished", NULL },
		{ "", NULL },
		{ " \t\r\n", NULL },
		{ "Sunday is the day of our Lord", NULL },
		{ "Sat, 9 May flies bit my arms", NULL }
	};
	GDateTime *date;
	char *actual;
	guint i;

	for (i = 0; i < G_N_ELEMENTS (cases); i++) {
		testsuite_check ("DateUtils recovery: '%s'", cases[i].input);
		date = g_mime_utils_header_decode_date (cases[i].input);
		actual = date ? g_mime_utils_header_format_date (date) : NULL;
		if (!g_strcmp0 (actual, cases[i].expected))
			testsuite_check_passed ();
		else
			testsuite_check_failed ("expected [%s], got [%s]", cases[i].expected, actual);
		g_free (actual);
		if (date)
			g_date_time_unref (date);
	}
	g_test_expect_message ("gmime", G_LOG_LEVEL_CRITICAL, "*str != NULL*");
	date = g_mime_utils_header_decode_date (NULL);
	g_test_assert_expected_messages ();
	g_assert_null (date);
}

static struct {
	const char *input;
	const char *decoded;
	const char *encoded;
} rfc2047_text[] = {
	{ "=?iso-8859-1?q?abc=5Cdef=E9?=",
	  "abc\\def\xc3\xa9",
	  "=?iso-8859-1?q?abc=5Cdef=E9?=" },
#if 0
	{ "=?iso-8859-1?Q?blah?=:\t=?iso-8859-1?Q?I_am_broken?=",
	  "blah:\tI am broken",
	  "blah:\tI am broken" },
#endif
	{ "=?UTF-8?B?SGVsbG8=?= =?UTF-8?B?IHdvcmxk?=",
	  "Hello world",
	  "Hello world" },
	{ "=?iso-8859-1?Q?Copy_of_Rapport_fra_Norges_R=E5fisklag=2Edoc?=",
	  "Copy of Rapport fra Norges R\xc3\xa5" "fisklag.doc",
	  "Copy of Rapport fra Norges =?iso-8859-1?q?R=E5fisklag=2Edoc?=" },
	{ "=?iso-8859-1?Q?Copy_of_Rapport_fra_Norges_R=E5fisklag.doc?=",
	  "Copy of Rapport fra Norges R\xc3\xa5" "fisklag.doc",
	  "Copy of Rapport fra Norges =?iso-8859-1?q?R=E5fisklag=2Edoc?=" },
	{ "=?iso-8859-1?B?dGVzdOb45S50eHQ=?=",
	  "test\xc3\xa6\xc3\xb8\xc3\xa5.txt",
	  "=?iso-8859-1?b?dGVzdOb45S50eHQ=?=" },
	{ "Re: !!! =?windows-1250?Q?Nab=EDz=EDm_scanov=E1n=ED_negativ=F9?= =?windows-1250?Q?=2C_p=F8edloh_do_A4=2C_=E8/b_lasertov=FD_ti?= =?windows-1250?Q?sk_a_=E8/b_inkoutov=FD_tisk_do_A2!!!?=",
	  "Re: !!! Nab\xc3\xadz\xc3\xadm scanov\xc3\xa1n\xc3\xad negativ\xc5\xaf, p\xc5\x99" "edloh do A4, \xc4\x8d/b lasertov\xc3\xbd tisk a \xc4\x8d/b inkoutov\xc3\xbd tisk do A2!!!",
	  "Re: !!! =?iso-8859-2?q?Nab=EDz=EDm_scanov=E1n=ED_negativ=F9=2C_p=F8edloh_do_A4=2C?= =?iso-8859-2?q?_=E8=2Fb_lasertov=FD_tisk_a_=E8=2Fb_inkoutov=FD?= tisk do A2!!!" },
	/*"Re: =?iso-8859-2?q?!!!_Nab=EDz=EDm_scanov=E1n=ED_negativ=F9=2C_?= =?iso-8859-2?q?p=F8edloh_do_A4=2C_=E8=2Fb_lasertov=FD_?= =?iso-8859-2?q?tisk_a_=E8=2Fb_inkoutov=FD?= tisk do A2!!!" },*/
	{ "Re: =?iso-8859-2?q?!!!_Nab=EDz=EDm_scanov=E1n=ED_negativ=F9=2C_?= =?iso-8859-2?q?p=F8edloh_do_A4=2C_=E8=2Fb_lasertov=FD_?= =?iso-8859-2?q?tisk_a_=E8=2Fb_inkoutov=FD?= tisk do A2!!!",
	  "Re: !!! Nab\xc3\xadz\xc3\xadm scanov\xc3\xa1n\xc3\xad negativ\xc5\xaf, p\xc5\x99" "edloh do A4, \xc4\x8d/b lasertov\xc3\xbd tisk a \xc4\x8d/b inkoutov\xc3\xbd tisk do A2!!!",
	  "Re: !!! =?iso-8859-2?q?Nab=EDz=EDm_scanov=E1n=ED_negativ=F9=2C_p=F8edloh_do_A4=2C?= =?iso-8859-2?q?_=E8=2Fb_lasertov=FD_tisk_a_=E8=2Fb_inkoutov=FD?= tisk do A2!!!" },
	/*"Re: =?iso-8859-2?q?!!!_Nab=EDz=EDm_scanov=E1n=ED_negativ=F9=2C_?= =?iso-8859-2?q?p=F8edloh_do_A4=2C_=E8=2Fb_lasertov=FD_?= =?iso-8859-2?q?tisk_a_=E8=2Fb_inkoutov=FD?= tisk do A2!!!" },*/
	{ "OT - ich =?iso-8859-1?b?d2Vp3yw=?= trotzdem",
	  "OT - ich wei\xc3\x9f, trotzdem",
	  "OT - ich =?iso-8859-1?b?d2Vp3yw=?= trotzdem" },
	{ "=?iso-8859-5?b?tra2tra2tra2tra2tra2tra2tra2tra2tra2tra2tra2tra2tra2tra2tra2tra2tra2tra2tra2tra2tra2tra2tra2tra2tra2tra2trY=?=",
	  "\xd0\x96\xd0\x96\xd0\x96\xd0\x96\xd0\x96\xd0\x96\xd0\x96\xd0\x96\xd0\x96\xd0\x96\xd0\x96\xd0\x96\xd0\x96\xd0\x96\xd0\x96\xd0\x96\xd0\x96\xd0\x96\xd0\x96\xd0\x96\xd0\x96\xd0\x96\xd0\x96\xd0\x96\xd0\x96\xd0\x96\xd0\x96\xd0\x96\xd0\x96\xd0\x96\xd0\x96\xd0\x96\xd0\x96\xd0\x96\xd0\x96\xd0\x96\xd0\x96\xd0\x96\xd0\x96\xd0\x96\xd0\x96\xd0\x96\xd0\x96\xd0\x96\xd0\x96\xd0\x96\xd0\x96\xd0\x96\xd0\x96\xd0\x96\xd0\x96\xd0\x96\xd0\x96\xd0\x96\xd0\x96\xd0\x96\xd0\x96\xd0\x96\xd0\x96\xd0\x96\xd0\x96\xd0\x96\xd0\x96\xd0\x96\xd0\x96\xd0\x96\xd0\x96\xd0\x96\xd0\x96\xd0\x96\xd0\x96\xd0\x96\xd0\x96\xd0\x96\xd0\x96\xd0\x96\xd0\x96\xd0\x96\xd0\x96\xd0\x96",
	  "=?iso-8859-5?b?tra2tra2tra2tra2tra2tra2tra2tra2tra2tra2tra2tra2tra2tra2?= =?iso-8859-5?b?tra2tra2tra2tra2tra2tra2tra2tra2tra2tra2tra2tra2trY=?=" },
	{ "=?iso-8859-1?q?Jobbans=F6kan?= - duktig =?iso-8859-1?q?researcher=2Fomv=E4rldsbevakare=2Fomv=E4rldsan?= =?us-ascii?q?alytiker?=",
	  "Jobbansökan - duktig researcher/omvärldsbevakare/omvärldsanalytiker",
	  "=?iso-8859-1?q?Jobbans=F6kan_-_duktig?= =?iso-8859-1?q?_researcher=2Fomv=E4rldsbevakare=2Fomv=E4rldsanalytiker?=" },
	{ "=?UTF-8?B?0J/RgNC40LLQtdGCINC40Lcg0KDQvtGB0YHQuNC4?= =?UTF-8?B?IChIZWxsbyBmcm9tIFJ1c3NpYSk=?=",
	  "Привет из России (Hello from Russia)",
	  "=?iso-8859-5?b?v+DY0tXiINjXIMDe4eHY2A==?= (Hello from Russia)" },
	{ "=?UTF-8?B?16nXnNeV150g15HXoteR16jXmdeqIChIZWxsbyBpbiBIZWJyZXcp?=",
	  "שלום בעברית (Hello in Hebrew)",
	  "=?iso-8859-8?b?+ezl7SDh8uH46fo=?= (Hello in Hebrew)" },
{ "=?UTF-8?B?2YXYsdit2KjYpyDYqNin2YTZhNi62Kkg2KfZhNi52LHYqNmK2KkgKEhlbGxvIGluIEFyYWJpYyk=?=",
	  "مرحبا باللغة العربية (Hello in Arabic)",
	  "=?iso-8859-6?b?5dHNyMcgyMfk5NrJIMfk2dHI6sk=?= (Hello in Arabic)" },
};

static struct {
	const char *input;
	const char *decoded;
	const char *encoded;
} broken_rfc2047_text[] = {
	{ "=?iso-8859-1?q?Jobbans=F6kan?= - duktig =?iso-8859-1?q?researcher=2Fomv=E4rldsbevakare=2Fomv=E4rldsan?=alytiker",
	  "Jobbansökan - duktig researcher/omvärldsbevakare/omvärldsanalytiker",
	  "=?iso-8859-1?q?Jobbans=F6kan_-_duktig?= =?iso-8859-1?q?_researcher=2Fomv=E4rldsbevakare=2Fomv=E4rldsanalytiker?=" },
	{ "Copy of Rapport fra Norges R=?iso-8859-1?Q?=E5?=fisklag.doc",
	  "Copy of Rapport fra Norges R\xc3\xa5" "fisklag.doc",
	  "Copy of Rapport fra Norges =?iso-8859-1?q?R=E5fisklag=2Edoc?=" },
	{ "Copy of Rapport fra Norges =?iso-8859-1?Q?R=E5?=fisklag.doc",
	  "Copy of Rapport fra Norges R\xc3\xa5" "fisklag.doc",
	  "Copy of Rapport fra Norges =?iso-8859-1?q?R=E5fisklag=2Edoc?=" },
	{ "=?iso-8859-1?q?Copy of Rapport fra Norges R=E5fisklag=2Edoc?=",
	  "Copy of Rapport fra Norges R\xc3\xa5" "fisklag.doc",
	  "Copy of Rapport fra Norges =?iso-8859-1?q?R=E5fisklag=2Edoc?=" },
	{ "=?utf-8?q?OT_-_ich_?==?iso-8859-1?b?d2Vp3yw=?= trotzdem",
	  "OT - ich wei\xc3\x9f, trotzdem",
	  "OT - ich =?iso-8859-1?b?d2Vp3yw=?= trotzdem" },
};

static struct {
	const char *input;
	const char *decoded;
	const char *encoded;
} rfc2047_phrase[] = {
	{ "=?iso-8859-1?q?abc=3Ddef=E9?=",
	  "abc=def\xc3\xa9",
	  "=?iso-8859-1?q?abc=3Ddef=E9?=" },
	{ "=?iso-8859-1?q?abc=5Fdef=E9?=",
	  "abc_def\xc3\xa9",
	  "=?iso-8859-1?q?abc=5Fdef=E9?=" },
	{ "=?iso-8859-1?q?abc!*+-/=E9?=",
	  "abc!*+-/\xc3\xa9",
	  "=?iso-8859-1?q?abc!*+-/=E9?=" },
};

static const struct {
	const char *input;
	const char *decoded;
	gboolean loose_only;
} rfc2047_decoding[] = {
	{ "=?UTF-8?Q?" "?=", "", FALSE },
	{ "=?UTF-8?B?" "?=", "", FALSE },
	{ "=?UTF-8?Q?a_b=5Fc?=", "a b_c", FALSE },
	{ "=?UTF-8?Q?abc=?=", "abc", FALSE },
	{ "=?UTF-8?Q?abc=4?=", "abc", FALSE },
	{ "=?UTF-8?Q?=4Z?=", "=4Z", FALSE },
	{ "=?UTF-8?Q?=A_?=", "=A_", FALSE },
	{ "=?UTF-8?Q?=__?=", "=_ ", FALSE },
	{ "=?UTF-8?Q?==41?=", "=A", FALSE },
	{ "=?UTF-8?Q?=4=41?=", "=4=41", FALSE },
	{ "=?UTF-8?Q?=?= =?UTF-8?Q?_?=", "=_", FALSE },
	{ "=?UTF-8?Q?=?= =?UTF-8?Q?4?= =?UTF-8?Q?1?=", "A", FALSE },
	{ "=?UTF-8?Q?=?= =?UTF-8?Q?" "?= =?UTF-8?Q?41?=", "A", FALSE },
	{ "=?UTF-8?Q?=C3?= =?UTF-8?Q?=A9?=", "\xc3\xa9", FALSE },
	{ "=?UTF-8?Q?=C?= =?UTF-8?Q?3=A?= =?UTF-8?Q?9?=", "\xc3\xa9", FALSE },
	{ "=?utf-8?q?=4?= =?UTF-8?Q?1?=", "A", FALSE },
	{ "=?UTF-8?Q?=4?= =?UTF-8?B?MQ==?= =?UTF-8?Q?1?=", "11", FALSE },
	{ "=?UTF-8?Q?=4?= =?US-ASCII?Q?1?= =?UTF-8?Q?1?=", "11", FALSE },
	{ "=?UTF-8?Q?=4?= raw =?UTF-8?Q?1?=", " raw 1", FALSE },
	{ "=?UTF-8?B?QQ==Qg==Qw==?=", "ABC", FALSE },
	{ "=?UTF-8?B?Q!Q==Qg==?=", "AB", FALSE },
	{ "=?UTF-8?B?Q===QQ==?=", "A", FALSE },
	{ "=?UTF-8?B?Q?= =?UTF-8?B?Q?= =?UTF-8?B?=?= =?UTF-8?B?=?=", "A", FALSE },
	{ "=?UTF-8?B?QQ=?= =?UTF-8?B?=Qg==?=", "AB", FALSE },
	{ "=?UTF-8?B?w6?= =?UTF-8?B?k=?=", "\xc3\xa9", FALSE },
	{ "=?UTF-8?B?Q?= =?UTF-8?B?" "?= =?UTF-8?B?Q==?=", "A", FALSE },
	{ "=?UTF-8?B?Q?= =?UTF-8?Q?abc?= =?UTF-8?B?Q==?=", "abc", FALSE },
	{ "=?UTF-8?B?Q?= =?US-ASCII?B?Q==?= =?UTF-8?B?Q==?=", "", FALSE },
	{ "=?UTF-8?B?Q?= raw =?UTF-8?B?Q==?=", " raw ", FALSE },
	{ "=?UTF-8?Q?a=\r\nb?=", "ab", TRUE },
	{ "=?UTF-8?Q?a=\nb?=", "ab", TRUE },
	{ "=?UTF-8?Q?a=\r_b?=", "a=\r_b", TRUE },
	{ "=?UTF-8?Q?a=?= =?UTF-8?Q?\r?= =?UTF-8?Q?\nb?=", "ab", TRUE },
};

static void
check_rfc2047_decoded (GMimeParserOptions *options, const char *input, const char *expected)
{
	guint phrase;

	for (phrase = 0; phrase < 2; phrase++) {
		char *decoded = NULL;

		testsuite_check ("rfc2047 %s decoding: %s", phrase ? "phrase" : "text", input);
		try {
			if (phrase)
				decoded = g_mime_utils_header_decode_phrase (options, input);
			else
				decoded = g_mime_utils_header_decode_text (options, input);
			if (strcmp (decoded, expected) != 0)
				throw (exception_new ("decoded value does not match: actual=\"%s\", expected=\"%s\"",
						      decoded, expected));
			testsuite_check_passed ();
		} catch (ex) {
			testsuite_check_failed ("rfc2047 decoding: %s", ex->message);
		} finally;
		g_free (decoded);
	}
}

static void
test_rfc2047_decoding (GMimeParserOptions *options)
{
	static const struct {
		const char *payload;
		const char *decoded;
		char encoding;
	} splits[] = {
		{ "alpha_=E2=82=AC_==41_=4=41_=A__=_omega=4",
		  "alpha \xe2\x82\xac =A =4=41 =A_ =_omega", 'Q' },
		{ "YWJjZGVmZ2hpamtsbW5vcHFyc3R1dnd4eXowMTIzNDU2Nzg5",
		  "abcdefghijklmnopqrstuvwxyz0123456789", 'B' },
	};
	GMimeRfcComplianceMode mode = g_mime_parser_options_get_rfc2047_compliance_mode (options);
	guint i, split, j, n;

	for (i = 0; i < G_N_ELEMENTS (rfc2047_decoding); i++) {
		if (!rfc2047_decoding[i].loose_only || mode == GMIME_RFC_COMPLIANCE_LOOSE)
			check_rfc2047_decoded (options, rfc2047_decoding[i].input, rfc2047_decoding[i].decoded);
	}

	for (i = 0; i < G_N_ELEMENTS (splits); i++) {
		for (split = 0; split <= strlen (splits[i].payload); split++) {
			char *input = g_strdup_printf ("=?UTF-8?%c?%.*s?= =?UTF-8?%c?%s?=",
				splits[i].encoding, (int) split, splits[i].payload,
				splits[i].encoding, splits[i].payload + split);

			check_rfc2047_decoded (options, input, splits[i].decoded);
			g_free (input);
		}
	}

	for (i = 0; i < 2; i++) {
		const char *payload = i ? "=41" : "QQ==";
		GString *input = g_string_new (NULL);
		char expected[129];

		memset (expected, 'A', 128);
		expected[128] = '\0';
		for (n = 0; n < 128; n++) {
			for (j = 0; payload[j]; j++) {
				if (input->len)
					g_string_append_c (input, ' ');
				g_string_append_printf (input, "=?UTF-8?%c?%c?=", i ? 'Q' : 'B', payload[j]);
			}
		}
		check_rfc2047_decoded (options, input->str, expected);
		g_string_free (input, TRUE);
	}
}

static char *
encode_legacy_header_fixture (GMimeFormatOptions *options, const char *decoded, const char *expected, gboolean phrase)
{
	const char *start = strstr (expected, "=?");
	char *charset, *encoded;

	/* Preserve legacy payload fixtures by explicitly requesting their charset. */
	charset = start ? g_strndup (start + 2, strchr (start + 2, '?') - (start + 2)) : NULL;
	encoded = phrase ? g_mime_utils_header_encode_phrase (options, decoded, charset) :
		g_mime_utils_header_encode_text (options, decoded, charset);
	g_free (charset);

	return encoded;
}

static void
test_rfc2047 (GMimeParserOptions *options, gboolean test_broken)
{
	GMimeFormatOptions *format = g_mime_format_options_new ();
	char *enc, *dec;
	guint i;

	g_mime_format_options_set_allow_mixed_charsets (format, TRUE);
	
	for (i = 0; i < G_N_ELEMENTS (rfc2047_text); i++) {
		dec = enc = NULL;
		testsuite_check ("rfc2047_text[%u]", i);
		try {
			dec = g_mime_utils_header_decode_text (options, rfc2047_text[i].input);
			if (strcmp (rfc2047_text[i].decoded, dec) != 0)
				throw (exception_new ("decoded text does not match: actual=\"%s\", expected=\"%s\"", dec, rfc2047_text[i].decoded));
			
			enc = encode_legacy_header_fixture (format, dec, rfc2047_text[i].encoded, FALSE);
			if (strcmp (rfc2047_text[i].encoded, enc) != 0)
				throw (exception_new ("encoded text does not match: actual=\"%s\", expected=\"%s\"", enc, rfc2047_text[i].encoded));

			//dec2 = g_mime_utils_header_decode_text (options, enc);
			//if (strcmp (rfc2047_text[i].decoded, dec2) != 0)
			//	throw (exception_new ("decoded2 text does not match: %s", dec));
			
			testsuite_check_passed ();
		} catch (ex) {
			testsuite_check_failed ("rfc2047_text[%u]: %s", i, ex->message);
		} finally;
		
		g_free (dec);
		g_free (enc);
	}
	
	for (i = 0; test_broken && i < G_N_ELEMENTS (broken_rfc2047_text); i++) {
		dec = enc = NULL;
		testsuite_check ("broken_rfc2047_text[%u]", i);
		try {
			dec = g_mime_utils_header_decode_text (options, broken_rfc2047_text[i].input);
			if (strcmp (broken_rfc2047_text[i].decoded, dec) != 0)
				throw (exception_new ("decoded text does not match: %s", dec));
			
			enc = encode_legacy_header_fixture (format, dec, broken_rfc2047_text[i].encoded, FALSE);
			if (strcmp (broken_rfc2047_text[i].encoded, enc) != 0)
				throw (exception_new ("encoded text does not match: %s", enc));
			
			testsuite_check_passed ();
		} catch (ex) {
			testsuite_check_failed ("broken_rfc2047_text[%u]: %s", i, ex->message);
		} finally;
		
		g_free (dec);
		g_free (enc);
	}
	
	for (i = 0; i < G_N_ELEMENTS (rfc2047_phrase); i++) {
		dec = enc = NULL;
		testsuite_check ("rfc2047_phrase[%u]", i);
		try {
			dec = g_mime_utils_header_decode_phrase (options, rfc2047_phrase[i].input);
			if (strcmp (rfc2047_phrase[i].decoded, dec) != 0)
				throw (exception_new ("decoded phrase does not match"));
			
			enc = encode_legacy_header_fixture (format, dec, rfc2047_phrase[i].encoded, TRUE);
			if (strcmp (rfc2047_phrase[i].encoded, enc) != 0)
				throw (exception_new ("encoded phrase does not match"));
			
			testsuite_check_passed ();
		} catch (ex) {
			testsuite_check_failed ("rfc2047_phrase[%u]: %s", i, ex->message);
		} finally;
		
		g_free (dec);
		g_free (enc);
	}

	g_mime_format_options_free (format);
	test_rfc2047_decoding (options);
}

static void
test_rfc2047_quoted_words (GMimeParserOptions *options)
{
	static const guint widths[] = { 60, 78, 998 };
	static const guint counts[] = { 8, 12, 40, 180 };
	static const char *prefixes[] = { "(team)", "name,", "\"quoted words\"", "slash\\", "Team name," };
	static const char *separators[] = { " ", "\t", " \t " };
	GMimeFormatOptions *format = g_mime_format_options_new ();
	GMimeInternetAddress *mailbox;
	GMimeInternetAddressList *parsed;
	GString *input = g_string_new (NULL);
	char *expected, *encoded, *text, *address, *wire, *normalized;
	guint i, j, k, n, c;

	for (i = 0; i < G_N_ELEMENTS (widths); i++) {
		g_mime_format_options_set_max_line_length (format, widths[i]);
		for (j = 0; j < G_N_ELEMENTS (prefixes); j++) {
			for (k = 0; k < G_N_ELEMENTS (separators); k++) {
				for (n = 0; n < G_N_ELEMENTS (counts); n++) {
					g_string_assign (input, prefixes[j]);
					for (c = 0; c < counts[n]; c++) {
						g_string_append (input, separators[k]);
						g_string_append (input, c % 3 == 1 ? "word," : "alpha");
					}
					expected = g_mime_utils_quote_string (input->str);
					encoded = g_mime_utils_header_encode_phrase (format, input->str, "utf-8");
					text = g_mime_utils_header_encode_text (format, input->str, "utf-8");
					mailbox = g_mime_mailbox_address_new (input->str, "user@example.com", NULL);
					wire = g_mime_internet_address_to_string (mailbox, format, TRUE);
					address = g_strconcat (expected, " <user@example.com>", NULL);
					normalized = g_strdelimit (g_strdup (input->str), "\t", ' ');
					parsed = NULL;
					testsuite_check ("rfc2047 quoted phrase[%u,%u,%u,%u]", widths[i], j, k, counts[n]);
					try {
						if (strcmp (encoded, expected))
							throw (exception_new ("phrase was not kept in one quoted string: %s", encoded));
						if (strcmp (text, input->str))
							throw (exception_new ("quoted phrase merging changed unstructured text: %s", text));
						if (strcmp (wire, address))
							throw (exception_new ("mailbox formatting split the quoted name: %s", wire));
						parsed = g_mime_internet_address_list_parse (options, wire);
						if (!parsed || g_mime_internet_address_list_length (parsed) != 1 ||
						    g_strcmp0 (g_mime_internet_address_get_name (
							    g_mime_internet_address_list_get_address (parsed, 0)), normalized))
							throw (exception_new ("quoted mailbox name did not round-trip"));
						testsuite_check_passed ();
					} catch (ex) {
						testsuite_check_failed ("%s", ex->message);
					} finally;
					g_clear_object (&parsed);
					g_object_unref (mailbox);
					g_free (address);
					g_free (normalized);
					g_free (wire);
					g_free (text);
					g_free (encoded);
					g_free (expected);
				}
			}
		}
	}

	g_string_assign (input, "(team)");
	for (c = 0; c < 20; c++)
		g_string_append (input, " alpha");
	expected = g_mime_utils_quote_string (input->str);
	for (i = 0; i < 3; i++) {
		if (i == 0) {
			text = g_strconcat (input->str, " \xe6\x97\xa5\xe6\x9c\xac\xe8\xaa\x9e tail", NULL);
			address = g_strconcat (expected, " =?utf-8?b?5pel5pys6Kqe?= tail", NULL);
		} else if (i == 1) {
			text = g_strconcat ("\xe6\x97\xa5\xe6\x9c\xac\xe8\xaa\x9e ", input->str, NULL);
			address = g_strconcat ("=?utf-8?b?5pel5pys6Kqe?= ", expected, NULL);
		} else {
			text = g_strconcat ("\xe6\x97\xa5\xe6\x9c\xac\xe8\xaa\x9e Team ", input->str, NULL);
			address = g_strconcat ("=?utf-8?b?5pel5pys6KqeIFRlYW0=?= ", expected, NULL);
		}
		encoded = g_mime_utils_header_encode_phrase (format, text, "utf-8");
		testsuite_check ("quoted phrase encoded-word boundary[%u]", i);
		try {
			if (strcmp (encoded, address))
				throw (exception_new ("quoted phrase merging crossed an encoded-word boundary: %s", encoded));
			testsuite_check_passed ();
		} catch (ex) {
			testsuite_check_failed ("%s", ex->message);
		} finally;
		g_free (encoded);
		g_free (address);
		g_free (text);
	}
	g_free (expected);
	g_string_free (input, TRUE);
	g_mime_format_options_free (format);
}

static void
test_rfc2047_literal_words (GMimeParserOptions *options)
{
	static const struct {
		const char *input, *encoded;
	} cases[] = {
		{ "=?", "=?us-ascii?q?=3D=3F?=" },
		{ "=?=", "=?us-ascii?q?=3D=3F=3D?=" },
		{ "=?" "?=", "=?us-ascii?q?=3D=3F=3F=3D?=" },
		{ "=?utf-8?q?hello?=", "=?us-ascii?q?=3D=3Futf-8=3Fq=3Fhello=3F=3D?=" },
		{ "=?utf-8?b?SGVsbG8=?=", "=?us-ascii?q?=3D=3Futf-8=3Fb=3FSGVsbG8=3D=3F=3D?=" },
		{ "abc=?x?q?hi?=xyz", "=?us-ascii?q?abc=3D=3Fx=3Fq=3Fhi=3F=3Dxyz?=" },
		{ "=?utf-8?Q?caf=C3=A9?=", "=?us-ascii?q?=3D=3Futf-8=3FQ=3Fcaf=3DC3=3DA9=3F=3D?=" },
		{ "=?x?q?=00?=", "=?us-ascii?q?=3D=3Fx=3Fq=3F=3D00=3F=3D?=" },
		{ "plain=word?", "plain=word?" },
		{ "?=word", "?=word" },
		{ "=word?", "=word?" }
	};
	static const char *values[] = {
		"before =?utf-8?q?hello?= after",
		"=?utf-8?q?one?= =?utf-8?q?two?=",
		"=?utf-8?q?one?==?utf-8?q?two?=",
		"=?utf-8?q?one?=\t  =?utf-8?q?two?=",
		"(=?utf-8?q?hello?=)",
		"\"=?utf-8?q?hello?=\"",
		"\xc3\xa9=?utf-8?q?hello?=",
		"\xe4\xb8\xad=?utf-8?q?hello?=",
		"\xf0\x9f\x98\x80=?utf-8?q?hello?=",
		"=?utf-8?q?hello?=\xc3\xa9",
		"=?utf-8?q?hello?=\xe4\xb8\xad",
		"=?utf-8?q?hello?=\xf0\x9f\x98\x80",
		"\"=?utf-8?q?hello?=\" (team)"
	};
	static const guint lengths[] = { 0, 1, 37, 38, 39, 40, 63, 64, 65, 76, 77, 78, 79, 156, 998 };
	GMimeFormatOptions *format = g_mime_format_options_new ();
	GMimeInternetAddressList *addresses;
	char *encoded, *decoded, *prefix, *input, *address;
	guint i;
	int phrase;

	for (phrase = 0; phrase < 2; phrase++) {
		for (i = 0; i < G_N_ELEMENTS (cases); i++) {
			decoded = NULL;
			encoded = phrase ? g_mime_utils_header_encode_phrase (format, cases[i].input, "us-ascii") :
				g_mime_utils_header_encode_text (format, cases[i].input, "us-ascii");
			testsuite_check ("rfc2047 literal %s word %u", phrase ? "phrase" : "text", i);
			try {
				if (strcmp (encoded, cases[i].encoded))
					throw (exception_new ("literal word encoding differs: expected=\"%s\", actual=\"%s\"",
							      cases[i].encoded, encoded));
				decoded = phrase ? g_mime_utils_header_decode_phrase (options, encoded) :
					g_mime_utils_header_decode_text (options, encoded);
				if (strcmp (decoded, cases[i].input))
					throw (exception_new ("literal word changed on decoding: %s", decoded));
				testsuite_check_passed ();
			} catch (ex) {
				testsuite_check_failed ("%s", ex->message);
			} finally;
			g_free (decoded);
			g_free (encoded);
		}

		for (i = 0; i < G_N_ELEMENTS (values) + G_N_ELEMENTS (lengths); i++) {
			if (i < G_N_ELEMENTS (values)) {
				input = g_strdup (values[i]);
			} else {
				prefix = g_strnfill (lengths[i - G_N_ELEMENTS (values)], 'a');
				input = g_strconcat (prefix, "=?utf-8?q?hello?=tail", NULL);
				g_free (prefix);
			}
			decoded = NULL;
			addresses = NULL;
			address = NULL;
			encoded = phrase ? g_mime_utils_header_encode_phrase (format, input, "utf-8") :
				g_mime_utils_header_encode_text (format, input, "utf-8");
			testsuite_check ("rfc2047 literal %s round-trip %u", phrase ? "phrase" : "text", i);
			try {
				if (phrase) {
					address = g_strconcat (encoded, " <user@example.com>", NULL);
					addresses = g_mime_internet_address_list_parse (options, address);
					if (!addresses || g_mime_internet_address_list_length (addresses) != 1)
						throw (exception_new ("encoded literal phrase could not be parsed as an address"));
					decoded = g_strdup (g_mime_internet_address_get_name (
						g_mime_internet_address_list_get_address (addresses, 0)));
				} else {
					decoded = g_mime_utils_header_decode_text (options, encoded);
				}
				if (g_strcmp0 (decoded, input))
					throw (exception_new ("literal word changed on decoding: expected=\"%s\", actual=\"%s\", encoded=\"%s\"",
							      input, decoded, encoded));
				testsuite_check_passed ();
			} catch (ex) {
				testsuite_check_failed ("%s", ex->message);
			} finally;
			g_free (decoded);
			g_free (encoded);
			g_free (input);
			g_free (address);
			g_clear_object (&addresses);
		}
	}
	g_mime_format_options_free (format);
}

static void
test_rfc2047_word_charsets (GMimeParserOptions *options)
{
	static const struct {
		const char *charset;
		const char *input;
		const char *encoded;
	} cases[] = {
		{ "utf-16", "A\xe4\xb8\xad", "=?utf-16?q?A=00-N?=" },
		{ "utf-16le", "A\xe4\xb8\xad", "=?utf-16?q?A=00-N?=" },
		{ "utf-16be", "A\xe4\xb8\xad", "=?utf-16be?q?=00AN-?=" },
		{ "utf-32", "A\xe4\xb8\xad", "=?utf-32?q?A=00=00=00-N=00=00?=" },
		{ "utf-32le", "A\xe4\xb8\xad", "=?utf-32?q?A=00=00=00-N=00=00?=" },
		{ "utf-32be", "A\xe4\xb8\xad", "=?utf-32be?q?=00=00=00A=00=00N-?=" },
		{ "utf-16le", "\xc3\xa9", "=?utf-16?b?6QA=?=" },
		{ "utf-16be", "\xc3\xa9", "=?utf-16be?b?AOk=?=" },
		{ "utf-32le", "\xc3\xa9", "=?utf-32?b?6QAAAA==?=" },
		{ "utf-16le", "\xf0\x9f\x98\x80", "=?utf-16?b?PdgA3g==?=" },
		{ "utf-16be", "\xf0\x9f\x98\x80", "=?utf-16be?b?2D3eAA==?=" },
		{ "utf-32le", "\xf0\x9f\x98\x80", "=?utf-32?b?APYBAA==?=" },
		{ "utf-32be", "\xf0\x9f\x98\x80", "=?utf-32be?b?AAH2AA==?=" },
		{ "iso-8859-1", "\xc3\xa9", "=?iso-8859-1?b?6Q==?=" },
		{ "windows-1251", "\xd0\x96", "=?windows-1251?b?xg==?=" },
		{ "iso-2022-jp", "\xe6\x97\xa5\xe6\x9c\xac\xe8\xaa\x9e", "=?iso-2022-jp?b?GyRCRnxLXDhsGyhC?=" },
		{ "iso-8859-1", "A\xe4\xb8\xad", "=?utf-8?b?QeS4rQ==?=" },
		{ "us-ascii", "\xc3\xa9", "=?utf-8?b?w6k=?=" },
		{ "windows-1251", "\xc3\xa9", "=?utf-8?b?w6k=?=" },
		{ "x-gmime-unknown", "\xc3\xa9", "=?utf-8?b?w6k=?=" },
		{ "us-ascii//TRANSLIT", "\xc3\xa9", "=?utf-8?b?w6k=?=" },
		{ "us-ascii//IGNORE", "A\xe4\xb8\xad", "=?utf-8?b?QeS4rQ==?=" }
	};
	static const char *chunk_charsets[] = {
		"utf-16", "utf-16be", "utf-32", "utf-32be",
		"iso-2022-jp", "us-ascii", "windows-1251"
	};
	GMimeFormatOptions *format = g_mime_format_options_new ();
	char *encoded, *decoded, **words;
	GString *input, *combined;
	guint i, j;
	int phrase;

	for (phrase = 0; phrase < 2; phrase++) {
		for (i = 0; i < G_N_ELEMENTS (cases); i++) {
			encoded = decoded = NULL;
			testsuite_check ("rfc2047 %s charset %s case %u", phrase ? "phrase" : "text", cases[i].charset, i);
			try {
				encoded = phrase ? g_mime_utils_header_encode_phrase (format, cases[i].input, cases[i].charset) :
					g_mime_utils_header_encode_text (format, cases[i].input, cases[i].charset);
				if (strcmp (encoded, cases[i].encoded))
					throw (exception_new ("encoded word does not match: actual=\"%s\", expected=\"%s\"",
							      encoded, cases[i].encoded));

				decoded = phrase ? g_mime_utils_header_decode_phrase (options, encoded) :
					g_mime_utils_header_decode_text (options, encoded);
				if (strcmp (decoded, cases[i].input))
					throw (exception_new ("encoded word did not round-trip: %s", decoded));

				testsuite_check_passed ();
			} catch (ex) {
				testsuite_check_failed ("%s", ex->message);
			} finally;
			g_free (decoded);
			g_free (encoded);
		}

		for (i = 16; i <= 18; i++) {
			input = g_string_new (NULL);
			for (j = 0; j < 25 - i; j++)
				g_string_append_c (input, 'a');
			for (j = 0; j < i; j++)
				g_string_append (input, "\xc3\xa9");

			encoded = decoded = NULL;
			words = NULL;
			testsuite_check ("rfc2047 %s encoding threshold %u%%", phrase ? "phrase" : "text", i);
			try {
				encoded = phrase ? g_mime_utils_header_encode_phrase (format, input->str, "utf-32") :
					g_mime_utils_header_encode_text (format, input->str, "utf-32");
				if (phrase && !g_str_has_prefix (encoded, i < 17 ? "=?utf-32?q?" : "=?utf-32?b?"))
					throw (exception_new ("incorrect encoding at %u%%: %s", i, encoded));

				if (!phrase) {
					/* Text chunks select Q/B independently, not from the unsplit word. */
					words = g_strsplit (encoded, " ", -1);
					for (j = 0; words[j]; j++) {
						gsize length, high = 0, k;
						char *bytes;

						decoded = g_mime_utils_header_decode_text (options, words[j]);
						bytes = g_convert (decoded, -1, "UTF-32LE", "UTF-8", NULL, &length, NULL);
						g_assert_nonnull (bytes);
						for (k = 0; k < length; k++)
							high += (guint8) bytes[k] > 127;
						g_free (bytes);
						g_clear_pointer (&decoded, g_free);
						if (strlen (words[j]) > 75 || !g_str_has_prefix (words[j],
							    (double) high < length * 0.17 ? "=?utf-32?q?" : "=?utf-32?b?"))
							throw (exception_new ("incorrect chunk encoding: %s", words[j]));
					}
					g_clear_pointer (&words, g_strfreev);
				}

				decoded = phrase ? g_mime_utils_header_decode_phrase (options, encoded) :
					g_mime_utils_header_decode_text (options, encoded);
				if (strcmp (decoded, input->str))
					throw (exception_new ("threshold word did not round-trip: %s", decoded));

				testsuite_check_passed ();
			} catch (ex) {
				testsuite_check_failed ("%s", ex->message);
			} finally;
			g_free (decoded);
			g_free (encoded);
			g_clear_pointer (&words, g_strfreev);
			g_string_free (input, TRUE);
		}

		for (i = 0; i < G_N_ELEMENTS (chunk_charsets); i++) {
			input = g_string_new (NULL);
			for (j = 0; j < 35; j++)
				g_string_append (input, "\xe6\x97\xa5\xe6\x9c\xac\xe8\xaa\x9e");

			encoded = decoded = NULL;
			words = NULL;
			combined = g_string_new (NULL);
			testsuite_check ("rfc2047 %s independently converted chunks %s",
					 phrase ? "phrase" : "text", chunk_charsets[i]);
			try {
				encoded = phrase ? g_mime_utils_header_encode_phrase (format, input->str, chunk_charsets[i]) :
					g_mime_utils_header_encode_text (format, input->str, chunk_charsets[i]);
				if (!strstr (encoded, phrase ? "?=\t=?" : "?= =?"))
					throw (exception_new ("expected multiple encoded words: %s", encoded));

				words = g_strsplit_set (encoded, " \t", -1);
				for (j = 0; words[j]; j++) {
					decoded = phrase ? g_mime_utils_header_decode_phrase (options, words[j]) :
						g_mime_utils_header_decode_text (options, words[j]);
					g_string_append (combined, decoded);
					g_clear_pointer (&decoded, g_free);
				}
				if (strcmp (combined->str, input->str))
					throw (exception_new ("individually decoded chunks did not round-trip: %s", combined->str));

				testsuite_check_passed ();
			} catch (ex) {
				testsuite_check_failed ("%s", ex->message);
			} finally;
			g_free (decoded);
			g_free (encoded);
			g_strfreev (words);
			g_string_free (combined, TRUE);
			g_string_free (input, TRUE);
		}
	}

	g_mime_format_options_free (format);
}

static struct {
	const char *input;
	const char *folded;
} header_folding[] = {
	/* Words reaching width minus one are encoded even if they could fit on a folded line. */
	{ "Subject: 012345678901234567890123456789012345678901234567890123456789012345678 01234567890123456789012345678901234567890123456789012345678901234567890123456 01234567890123456789012345678901234567890123456789012345678901234567890123456 0123456789",
	  "Subject: 012345678901234567890123456789012345678901234567890123456789012345678\n"
	  " =?utf-8?q?0123456789012345678901234567890123456789012345678901234567890?=\n"
	  " =?utf-8?q?1234567890123456?=\n"
	  " =?utf-8?q?_0123456789012345678901234567890123456789012345678901234567890?=\n"
	  " =?utf-8?q?1234567890123456?= 0123456789\n" },
	/* Note: This should require folding for each word in order to fit within the 78 character limit */
	{ "Subject: 012345678901234567890123456789012345678901234567890123456789012345678 012345678901234567890123456789012345678901234567890123456789012345678901234567 012345678901234567890123456789012345678901234567890123456789012345678901234567 0123456789",
	  "Subject: 012345678901234567890123456789012345678901234567890123456789012345678\n"
	  " =?utf-8?q?0123456789012345678901234567890123456789012345678901234567890?=\n"
	  " =?utf-8?q?12345678901234567?=\n"
	  " =?utf-8?q?_0123456789012345678901234567890123456789012345678901234567890?=\n"
	  " =?utf-8?q?12345678901234567?= 0123456789\n" },
};

static void
test_header_folding (GMimeParserOptions *options)
{
	GMimeFormatOptions *format = g_mime_format_options_get_default ();
	GMimeHeaderList *list;
	guint i;
	
	list = g_mime_header_list_new (NULL);
	
	for (i = 0; i < G_N_ELEMENTS (header_folding); i++) {
		char *folded = NULL;
		
		testsuite_check ("header_folding[%u]", i);
		try {
			const char *colon = strchr (header_folding[i].input, ':');
			char *name = g_strndup (header_folding[i].input, (colon - header_folding[i].input));
			const char *value = colon + 1;
			
			while (*value == ' ')
				value++;
			
			g_mime_header_list_append (list, name, value, NULL);
			g_free (name);
			
			folded = g_mime_header_list_to_string (list, format);
			if (strcmp (header_folding[i].folded, folded) != 0)
				throw (exception_new ("folded text does not match: -->\n%s<-- vs -->\n%s<--", header_folding[i].folded, folded));
			
			testsuite_check_passed ();
		} catch (ex) {
			testsuite_check_failed ("header_folding[%u]: %s", i, ex->message);
		} finally;
		
		g_mime_header_list_clear (list);
		g_free (folded);
	}
	
	g_object_unref (list);
}

static struct {
	const char *input;
	const char *charset;
	const char *encoded;
	GMimeParamEncodingMethod method;
} rfc2184[] = {
	{ "this is a really really long filename that should force gmime to rfc2184 encode it - yay!.html",
	  "us-ascii",
	  "Content-Disposition: attachment;\n\t"
	  "filename*0*=us-ascii'en'this%20is%20a%20really%20really%20long%20filename%20;\n\t"
	  "filename*1=\"that should force gmime to rfc2184 encode it - yay!.html\"\n",
	  GMIME_PARAM_ENCODING_METHOD_RFC2231 },
	{ "{#Dèé£åý M$áí.çøm}.doc",
	  "iso-8859-1",
	  "Content-Disposition: attachment;\n\t"
	  "filename*=iso-8859-1'en'{#D%E8%E9%A3%E5%FD%20M$%E1%ED.%E7%F8m}.doc\n",
	  GMIME_PARAM_ENCODING_METHOD_RFC2231 },
	
	/* Note: technically these aren't rfc2184-encoded... but they need to be parsed... */
	{ "{#Dèé£åý M$áí.çøm}.doc",
	  "iso-8859-1",
	  "Content-Disposition: attachment; "
	  "filename=\"=?iso-8859-1?b?eyNE6Omj5f0g?=\n\t"
	  "=?iso-8859-1?b?TSTh7S7n+G19LmRvYw==?=\"\n",
	  GMIME_PARAM_ENCODING_METHOD_RFC2047 },
};

static void
test_rfc2184 (GMimeParserOptions *options)
{
	GMimeFormatOptions *format = g_mime_format_options_get_default ();
	GMimeParamEncodingMethod method;
	GMimeParamList *params;
	GMimeParam *param;
	const char *value;
	GString *str;
	int count;
	size_t n;
	guint i;
	
	for (i = 0; i < G_N_ELEMENTS (rfc2184); i++) {
		params = g_mime_param_list_new ();
		g_mime_param_list_set_parameter (params, "filename", rfc2184[i].input);
		param = g_mime_param_list_get_parameter (params, "filename");
		g_mime_param_set_encoding_method (param, rfc2184[i].method);
		g_mime_param_set_lang (param, "en");
		
		str = g_string_new ("Content-Disposition: attachment");
		n = str->len;
		
		g_mime_param_list_encode (params, format, TRUE, str);
		g_object_unref (params);
		params = NULL;
		
		testsuite_check ("rfc2184[%u]", i);
		try {
			if (strcmp (rfc2184[i].encoded, str->str) != 0)
				throw (exception_new ("encoded param list does not match: \n%s\nvs\n%s",
						      rfc2184[i].encoded, str->str));
			
			if (!(params = g_mime_param_list_parse (options, str->str + n + 2)))
				throw (exception_new ("could not parse encoded param list"));
			
			if ((count = g_mime_param_list_length (params)) != 1)
				throw (exception_new ("expected only 1 param, but parsed %d", count));
			
			if (!(param = g_mime_param_list_get_parameter (params, "filename")))
				throw (exception_new ("failed to get filename param"));
			
			value = g_mime_param_get_value (param);
			if (strcmp (rfc2184[i].input, value) != 0)
				throw (exception_new ("parsed param value does not match: %s", value));
			
			if (!(value = g_mime_param_get_charset (param)))
				throw (exception_new ("parsed charset is NULL"));
			
			if (strcmp (rfc2184[i].charset, value) != 0)
				throw (exception_new ("parsed charset does not match: %s", value));
			
			if (rfc2184[i].method != GMIME_PARAM_ENCODING_METHOD_RFC2047) {
				if (!(value = g_mime_param_get_lang (param)))
					throw (exception_new ("parsed lang is NULL"));
				
				if (strcmp (value, "en") != 0)
					throw (exception_new ("parsed lang does not match: %s", value));
			}
			
			method = g_mime_param_get_encoding_method (param);
			if (method != rfc2184[i].method)
				throw (exception_new ("parsed encoding method does not match: %d", method));
			
			testsuite_check_passed ();
		} catch (ex) {
			testsuite_check_failed ("rfc2184[%u]: %s", i, ex->message);
		} finally;
		
		if (params != NULL)
			g_object_unref (params);
		
		g_string_free (str, TRUE);
	}
}


static struct {
	const char *input;
	const char *unquoted;
	const char *quoted;
} qstrings[] = {
	{ "this is a \\\"quoted\\\" string",
	  "this is a \"quoted\" string",
	  "this is a \"quoted\" string" },
	{ "\\\"this\\\" and \\\"that\\\"",
	  "\"this\" and \"that\"",
	  "\"this\" and \"that\"" },
	{ "\"Dr. A. Cula\"",
	  "Dr. A. Cula",
	  "\"Dr. A. Cula\"" },
};

static void
test_qstring (void)
{
	char *str;
	guint i;
	
	for (i = 0; i < G_N_ELEMENTS (qstrings); i++) {
		str = NULL;
		
		testsuite_check ("qstrings[%u]", i);
		try {
			str = g_strdup (qstrings[i].input);
			g_mime_utils_unquote_string (str);
			if (strcmp (qstrings[i].unquoted, str) != 0)
				throw (exception_new ("unquoted string does not match"));
			
			g_free (str);
			str = g_mime_utils_quote_string (qstrings[i].unquoted);
			if (strcmp (qstrings[i].quoted, str) != 0)
				throw (exception_new ("quoted string does not match"));
			
			testsuite_check_passed ();
		} catch (ex) {
			testsuite_check_failed ("qstrings[%u]: %s", i, ex->message);
		} finally;
		
		g_free (str);
	}
}


static struct {
	const char *input;
	int count;
	const char *ids[5];
} references[] = {
	{ "<3ohapq$h3b@gandalf.rutgers.edu> <3notqh$b52@ns2.ny.ubs.com> <3npoh0$2oo@news.blkbox.com> <3nqp09$r7t@ns2.ny.ubs.com>",
	  4,
	  { "3ohapq$h3b@gandalf.rutgers.edu", "3notqh$b52@ns2.ny.ubs.com", "3npoh0$2oo@news.blkbox.com", "3nqp09$r7t@ns2.ny.ubs.com", NULL }
	},
	{ "<3lmtu0$dv1@secnews.netscape.com> <3lpjth$g97@secnews.netscape.com> <3lrbuf$gvp@secnews.netscape.com> <3lst13$iur@secnews.netscape.com>",
	  4,
	  { "3lmtu0$dv1@secnews.netscape.com", "3lpjth$g97@secnews.netscape.com", "3lrbuf$gvp@secnews.netscape.com", "3lst13$iur@secnews.netscape.com", NULL }
	},
};


static void
test_references (GMimeParserOptions *options)
{
	GMimeMessageIdList *refs, *copy;
	guint i;
	int j;
	
	for (i = 0; i < G_N_ELEMENTS (references); i++) {
		testsuite_check ("references[%u]", i);
		try {
			if (!(refs = g_mime_message_id_list_parse (options, references[i].input)))
				throw (exception_new ("failed to parse references"));
			
			if (g_mime_message_id_list_length (refs) != references[i].count)
				throw (exception_new ("number of references does not match"));
			
			for (j = 0; j < references[i].count; j++) {
				const char *msgid = g_mime_message_id_list_get_message_id (refs, j);
				
				if (strcmp (references[i].ids[j], msgid) != 0)
					throw (exception_new ("message ids do not match for ids[%d]", j));
			}
			
			copy = g_mime_message_id_list_clone (refs);
			if (g_mime_message_id_list_length (copy) != g_mime_message_id_list_length (refs))
				throw (exception_new ("number of copied references does not match"));
			
			for (j = 0; j < references[i].count; j++) {
				const char *msgid = g_mime_message_id_list_get_message_id (copy, j);
				
				if (strcmp (references[i].ids[j], msgid) != 0)
					throw (exception_new ("copied message ids do not match for ids[%d]", j));
			}
			
			g_mime_message_id_list_clear (copy);
			
			if (g_mime_message_id_list_length (copy) != 0)
				throw (exception_new ("references were not cleared"));
			
			g_object_unref (copy);
			g_object_unref (refs);
			
			testsuite_check_passed ();
		} catch (ex) {
			testsuite_check_failed ("references[%u]: %s", i, ex->message);
		} finally;
	}
}

int main (int argc, char **argv)
{
	GMimeParserOptions *options = g_mime_parser_options_new ();
	
	g_mime_init ();
	
	testsuite_init (argc, argv);
	
	testsuite_start ("addr-spec parser (strict)");
	g_mime_parser_options_set_rfc2047_compliance_mode (options, GMIME_RFC_COMPLIANCE_STRICT);
	test_addrspec (options, FALSE);
	test_address_name_whitespace (options);
	testsuite_end ();
	
	testsuite_start ("addr-spec parser (loose)");
	g_mime_parser_options_set_rfc2047_compliance_mode (options, GMIME_RFC_COMPLIANCE_LOOSE);
	test_addrspec (options, TRUE);
	test_address_name_whitespace (options);
	testsuite_end ();
	
	testsuite_start ("date parser");
	test_date_parser ();
	test_date_recovery ();
	test_date_formatting ();
	testsuite_end ();
	
	testsuite_start ("rfc2047 encoding/decoding (strict)");
	g_mime_parser_options_set_rfc2047_compliance_mode (options, GMIME_RFC_COMPLIANCE_STRICT);
	test_rfc2047 (options, FALSE);
	test_rfc2047_quoted_words (options);
	test_rfc2047_literal_words (options);
	test_rfc2047_word_charsets (options);
	testsuite_end ();
	
	testsuite_start ("rfc2047 encoding/decoding (loose)");
	g_mime_parser_options_set_rfc2047_compliance_mode (options, GMIME_RFC_COMPLIANCE_LOOSE);
	test_rfc2047 (options, TRUE);
	test_rfc2047_quoted_words (options);
	test_rfc2047_literal_words (options);
	test_rfc2047_word_charsets (options);
	testsuite_end ();
	
	testsuite_start ("rfc2184 encoding/decoding");
	test_rfc2184 (options);
	testsuite_end ();
	
	testsuite_start ("quoted-strings");
	test_qstring ();
	testsuite_end ();
	
	testsuite_start ("header folding");
	test_header_folding (options);
	testsuite_end ();
	
	testsuite_start ("references");
	test_references (options);
	testsuite_end ();
	
	g_mime_parser_options_free (options);
	
	g_mime_shutdown ();
	
	return testsuite_exit ();
}
