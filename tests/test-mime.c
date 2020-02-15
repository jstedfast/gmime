/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*  GMime
 *  Copyright (C) 2000-2020 Jeffrey Stedfast
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
	  "Local recipients: phil, joe, alex, bob;" },
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
	  "Charles, likes illegal commas <charles@superpimp.org>" },
	{ "<charles@broken.host.com.>", NULL,
	  "charles@broken.host.com",
	  "charles@broken.host.com" },
	{ "fpons@mandrakesoft.com (=?iso-8859-1?q?Fran=E7ois?= Pons likes _'s and 	's too)", "iso-8859-1",
	  "Fran\xc3\xa7ois Pons likes _'s and 	's too <fpons@mandrakesoft.com>",
	  "=?iso-8859-1?q?Fran=E7ois?= Pons likes _'s and 	's too <fpons@mandrakesoft.com>" },
	{ "T\x81\xf5ivo Leedj\x81\xe4rv <leedjarv@interest.ee>", NULL,
	  "T\xc2\x81\xc3\xb5ivo Leedj\xc2\x81\xc3\xa4rv <leedjarv@interest.ee>",
	  "=?iso-8859-1?b?VIH1aXZvIExlZWRqgeRydg==?= <leedjarv@interest.ee>" },
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
	{ "undisclosed-recipients", NULL,
	  "undisclosed-recipients",
	  "undisclosed-recipients" },
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
	  "user@domain.com",
	  "user@domain.com" },
	{ "<@route1,,@route2,,,@route3:user@domain.com>", NULL,
	  "user@domain.com",
	  "user@domain.com" },
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
	  "=?UTF-8?b?eyNEw6jDqcKjw6XDvSBNJMOhw60uw6fDuG19?= <user@domain.com>" },
	/* quoted-printable payload split between multiple encoded-word tokens */
	{ "=?utf-8?Q?{#D=C3=A8=C3=A9=C2=?= =?utf-8?Q?A3=C3=A5=C3=BD_M$=C3=A1=C?= =?utf-8?Q?3=AD.=C3=A7=C3=B8m}?= <user@domain.com>", "utf-8",
	  "\"{#Dèé£åý M$áí.çøm}\" <user@domain.com>",
	  "=?UTF-8?b?eyNEw6jDqcKjw6XDvSBNJMOhw60uw6fDuG19?= <user@domain.com>" },
	/* base64 payload split between multiple encoded-word tokens */
	{ "=?iso-8859-1?b?ey?= =?iso-8859-1?b?NE6Omj5f0gTSTh7S7n+G1AI30=?= <user@domain.com>", "iso-8859-1",
	  "\"{#Dèé£åý M$áí.çøm@#}\" <user@domain.com>",
	  "=?iso-8859-1?b?eyNE6Omj5f0gTSTh7S7n+G1AI30=?= <user@domain.com>" },
};

static void
test_addrspec (GMimeParserOptions *options, gboolean test_broken)
{
	GMimeFormatOptions *format = g_mime_format_options_get_default ();
	InternetAddressList *addrlist;
	InternetAddress *address;
	const char *charset;
	char *str;
	guint i;
	
	for (i = 0; i < G_N_ELEMENTS (addrspec); i++) {
		addrlist = NULL;
		str = NULL;
		
		testsuite_check ("addrspec[%u]", i);
		try {
			if (!(addrlist = internet_address_list_parse (options, addrspec[i].input)))
				throw (exception_new ("could not parse: %s", addrspec[i].input));
			
			if (!(address = internet_address_list_get_address (addrlist, 0)))
				throw (exception_new ("could not get first address: %s", addrspec[i].input));
			
			charset = internet_address_get_charset (address);
			if (addrspec[i].charset != NULL) {
				if (charset == NULL)
					throw (exception_new ("expected '%s' but got NULL charset: %s", addrspec[i].charset, addrspec[i].input));
				if (g_ascii_strcasecmp (addrspec[i].charset, charset) != 0)
					throw (exception_new ("expected '%s' but got '%s' charset: %s", addrspec[i].charset, charset, addrspec[i].input));
			} else if (charset != NULL) {
				throw (exception_new ("expected NULL charset but address has a charset of '%s': %s", charset, addrspec[i].input));
			}
			
			str = internet_address_list_to_string (addrlist, format, FALSE);
			if (strcmp (addrspec[i].display, str) != 0)
				throw (exception_new ("display strings do not match.\ninput: %s\nexpected: %s\nactual: %s", addrspec[i].input, addrspec[i].display, str));
			g_free (str);
			
			str = internet_address_list_to_string (addrlist, format, TRUE);
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
				if (!(addrlist = internet_address_list_parse (options, broken_addrspec[i].input)))
					throw (exception_new ("could not parse: %s", broken_addrspec[i].input));

				if (!(address = internet_address_list_get_address (addrlist, 0)))
					throw (exception_new ("could not get first address: %s", broken_addrspec[i].input));
				
				charset = internet_address_get_charset (address);
				if (broken_addrspec[i].charset != NULL) {
					if (charset == NULL)
						throw (exception_new ("expected '%s' but got NULL charset: %s", broken_addrspec[i].charset, broken_addrspec[i].input));
					if (g_ascii_strcasecmp (broken_addrspec[i].charset, charset) != 0)
						throw (exception_new ("expected '%s' but got '%s' charset: %s", broken_addrspec[i].charset, charset, broken_addrspec[i].input));
				} else if (charset != NULL) {
					throw (exception_new ("expected NULL charset but address has a charset of '%s': %s", charset, broken_addrspec[i].input));
				}
				
				str = internet_address_list_to_string (addrlist, format, FALSE);
				if (strcmp (broken_addrspec[i].display, str) != 0)
					throw (exception_new ("display strings do not match.\ninput: %s\nexpected: %s\nactual: %s", broken_addrspec[i].input, broken_addrspec[i].display, str));
				g_free (str);
				
				str = internet_address_list_to_string (addrlist, format, TRUE);
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
	GDateTime *date;
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
				throw (exception_new ("time_t's do not match: %ld vs %ld", time, dates[i].date));
			
			if (tz_offset != dates[i].tzone)
				throw (exception_new ("timezones do not match"));
			
			buf = g_mime_utils_header_format_date (date);
			if (strcmp (dates[i].out, buf) != 0) {
				newex = exception_new ("date strings do not match: %s vs %s", buf, dates[i].out);
				g_free (buf);
				throw (newex);
			}
			
			g_free (buf);
			
			testsuite_check_passed ();
		} catch (ex) {
			testsuite_check_failed ("Date: '%s': %s", dates[i].in, ex->message);
		} finally;
	}
}

static struct {
	const char *input;
	const char *decoded;
	const char *encoded;
} rfc2047_text[] = {
#if 0
	{ "=?iso-8859-1?Q?blah?=:\t=?iso-8859-1?Q?I_am_broken?=",
	  "blah:\tI am broken",
	  "blah:\tI am broken" },
#endif
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
	  "Re: !!! =?iso-8859-2?b?TmFi7XrtbSBzY2Fub3bhbu0gbmVnYXRpdvks?= =?iso-8859-2?q?_p=F8edloh_do_A4=2C_=E8=2Fb_lasertov=FD?= tisk a =?iso-8859-2?q?=E8=2Fb_inkoutov=FD?= tisk do A2!!!" },
	/*"Re: =?iso-8859-2?q?!!!_Nab=EDz=EDm_scanov=E1n=ED_negativ=F9=2C_?= =?iso-8859-2?q?p=F8edloh_do_A4=2C_=E8=2Fb_lasertov=FD_?= =?iso-8859-2?q?tisk_a_=E8=2Fb_inkoutov=FD?= tisk do A2!!!" },*/
	{ "Re: =?iso-8859-2?q?!!!_Nab=EDz=EDm_scanov=E1n=ED_negativ=F9=2C_?= =?iso-8859-2?q?p=F8edloh_do_A4=2C_=E8=2Fb_lasertov=FD_?= =?iso-8859-2?q?tisk_a_=E8=2Fb_inkoutov=FD?= tisk do A2!!!",
	  "Re: !!! Nab\xc3\xadz\xc3\xadm scanov\xc3\xa1n\xc3\xad negativ\xc5\xaf, p\xc5\x99" "edloh do A4, \xc4\x8d/b lasertov\xc3\xbd tisk a \xc4\x8d/b inkoutov\xc3\xbd tisk do A2!!!",
	  "Re: !!! =?iso-8859-2?b?TmFi7XrtbSBzY2Fub3bhbu0gbmVnYXRpdvks?= =?iso-8859-2?q?_p=F8edloh_do_A4=2C_=E8=2Fb_lasertov=FD?= tisk a =?iso-8859-2?q?=E8=2Fb_inkoutov=FD?= tisk do A2!!!" },
	/*"Re: =?iso-8859-2?q?!!!_Nab=EDz=EDm_scanov=E1n=ED_negativ=F9=2C_?= =?iso-8859-2?q?p=F8edloh_do_A4=2C_=E8=2Fb_lasertov=FD_?= =?iso-8859-2?q?tisk_a_=E8=2Fb_inkoutov=FD?= tisk do A2!!!" },*/
	{ "OT - ich =?iso-8859-1?b?d2Vp3yw=?= trotzdem",
	  "OT - ich wei\xc3\x9f, trotzdem",
	  "OT - ich =?iso-8859-1?b?d2Vp3yw=?= trotzdem" },
	{ "=?iso-8859-5?b?tra2tra2tra2tra2tra2tra2tra2tra2tra2tra2tra2tra2tra2tra2tra2tra2tra2tra2tra2tra2tra2tra2tra2tra2tra2tra2trY=?=",
	  "\xd0\x96\xd0\x96\xd0\x96\xd0\x96\xd0\x96\xd0\x96\xd0\x96\xd0\x96\xd0\x96\xd0\x96\xd0\x96\xd0\x96\xd0\x96\xd0\x96\xd0\x96\xd0\x96\xd0\x96\xd0\x96\xd0\x96\xd0\x96\xd0\x96\xd0\x96\xd0\x96\xd0\x96\xd0\x96\xd0\x96\xd0\x96\xd0\x96\xd0\x96\xd0\x96\xd0\x96\xd0\x96\xd0\x96\xd0\x96\xd0\x96\xd0\x96\xd0\x96\xd0\x96\xd0\x96\xd0\x96\xd0\x96\xd0\x96\xd0\x96\xd0\x96\xd0\x96\xd0\x96\xd0\x96\xd0\x96\xd0\x96\xd0\x96\xd0\x96\xd0\x96\xd0\x96\xd0\x96\xd0\x96\xd0\x96\xd0\x96\xd0\x96\xd0\x96\xd0\x96\xd0\x96\xd0\x96\xd0\x96\xd0\x96\xd0\x96\xd0\x96\xd0\x96\xd0\x96\xd0\x96\xd0\x96\xd0\x96\xd0\x96\xd0\x96\xd0\x96\xd0\x96\xd0\x96\xd0\x96\xd0\x96\xd0\x96\xd0\x96",
	  "=?iso-8859-5?b?tra2tra2tra2tra2tra2tra2tra2tra2tra2tra2tra2tra2tra2?= =?iso-8859-5?b?tra2tra2tra2tra2tra2tra2tra2tra2tra2tra2tra2tra2tra2?= =?iso-8859-5?b?trY=?=" },
	{ "=?iso-8859-1?q?Jobbans=F6kan?= - duktig =?iso-8859-1?q?researcher=2Fomv=E4rldsbevakare=2Fomv=E4rldsan?= =?us-ascii?q?alytiker?=",
	  "Jobbansökan - duktig researcher/omvärldsbevakare/omvärldsanalytiker",
	  "=?iso-8859-1?q?Jobbans=F6kan?= - duktig =?iso-8859-1?q?researcher=2Fomv=E4rldsbevakare=2Fomv=E4rldsana?= =?us-ascii?q?lytiker?=" },
};

static struct {
	const char *input;
	const char *decoded;
	const char *encoded;
} broken_rfc2047_text[] = {
	{ "=?iso-8859-1?q?Jobbans=F6kan?= - duktig =?iso-8859-1?q?researcher=2Fomv=E4rldsbevakare=2Fomv=E4rldsan?=alytiker",
	  "Jobbansökan - duktig researcher/omvärldsbevakare/omvärldsanalytiker",
	  "=?iso-8859-1?q?Jobbans=F6kan?= - duktig =?iso-8859-1?q?researcher=2Fomv=E4rldsbevakare=2Fomv=E4rldsana?= =?us-ascii?q?lytiker?=" },
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

#if 0
static struct {
	const char *input;
	const char *decoded;
	const char *encoded;
} rfc2047_phrase[] = {
	/* FIXME: add some phrase tests */
};
#endif

static void
test_rfc2047 (GMimeParserOptions *options, gboolean test_broken)
{
	GMimeFormatOptions *format = g_mime_format_options_get_default ();
	char *enc, *dec;
	guint i;
	
	for (i = 0; i < G_N_ELEMENTS (rfc2047_text); i++) {
		dec = enc = NULL;
		testsuite_check ("rfc2047_text[%u]", i);
		try {
			dec = g_mime_utils_header_decode_text (options, rfc2047_text[i].input);
			if (strcmp (rfc2047_text[i].decoded, dec) != 0)
				throw (exception_new ("decoded text does not match: %s", dec));
			
			enc = g_mime_utils_header_encode_text (format, dec, NULL);
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
			
			enc = g_mime_utils_header_encode_text (format, dec, NULL);
			if (strcmp (broken_rfc2047_text[i].encoded, enc) != 0)
				throw (exception_new ("encoded text does not match: %s", enc));
			
			testsuite_check_passed ();
		} catch (ex) {
			testsuite_check_failed ("broken_rfc2047_text[%u]: %s", i, ex->message);
		} finally;
		
		g_free (dec);
		g_free (enc);
	}
	
#if 0
	for (i = 0; i < G_N_ELEMENTS (rfc2047_phrase); i++) {
		dec = enc = NULL;
		testsuite_check ("rfc2047_phrase[%u]", i);
		try {
			dec = g_mime_utils_header_decode_phrase (rfc2047_phrase[i].input);
			if (strcmp (rfc2047_phrase[i].decoded, dec) != 0)
				throw (exception_new ("decoded phrase does not match"));
			
			enc = g_mime_utils_header_encode_phrase (format, dec, NULL);
			if (strcmp (rfc2047_phrase[i].encoded, enc) != 0)
				throw (exception_new ("encoded phrase does not match"));
			
			testsuite_check_passed ();
		} catch (ex) {
			testsuite_check_failed ("rfc2047_phrase[%u]: %s", i, ex->message);
		} finally;
		
		g_free (dec);
		g_free (enc);
	}
#endif
}

static struct {
	const char *input;
	const char *folded;
} header_folding[] = {
	/* Note: This should fold w/o needing to encode or break apart words because they are designed to
	 * just *barely* fit within 78 characters. */
	{ "Subject: 012345678901234567890123456789012345678901234567890123456789012345678 01234567890123456789012345678901234567890123456789012345678901234567890123456 01234567890123456789012345678901234567890123456789012345678901234567890123456 0123456789",
	  "Subject: 012345678901234567890123456789012345678901234567890123456789012345678\n 01234567890123456789012345678901234567890123456789012345678901234567890123456\n 01234567890123456789012345678901234567890123456789012345678901234567890123456\n 0123456789\n" },
	/* Note: This should require folding for each word in order to fit within the 78 character limit */
	{ "Subject: 012345678901234567890123456789012345678901234567890123456789012345678 012345678901234567890123456789012345678901234567890123456789012345678901234567 012345678901234567890123456789012345678901234567890123456789012345678901234567 0123456789",
	  "Subject: 012345678901234567890123456789012345678901234567890123456789012345678\n"
	  " =?us-ascii?Q?01234567890123456789012345678901234567890123456789012345678901?=\n"
	  " =?us-ascii?Q?2345678901234567?=\n"
	  " =?us-ascii?Q?_01234567890123456789012345678901234567890123456789012345678901?=\n" // FIXME: this should be 1 char shorter
	  " =?us-ascii?Q?2345678901234567?= 0123456789\n" },
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
	  "filename*1*=that%20should%20force%20gmime%20to%20rfc2184%20encode%20it%20-;\n\t"
	  "filename*2*=%20yay!.html\n",
	  GMIME_PARAM_ENCODING_METHOD_RFC2231 },
	{ "{#Dèé£åý M$áí.çøm}.doc",
	  "iso-8859-1",
	  "Content-Disposition: attachment;\n\t"
	  "filename*=iso-8859-1'en'{#D%E8%E9%A3%E5%FD%20M$%E1%ED.%E7%F8m}.doc\n",
	  GMIME_PARAM_ENCODING_METHOD_RFC2231 },
	
	/* Note: technically these aren't rfc2184-encoded... but they need to be parsed... */
	{ "{#Dèé£åý M$áí.çøm}.doc",
	  "iso-8859-1",
	  "Content-Disposition: attachment;\n\t"
	  "filename=\"=?iso-8859-1?b?eyNE6Omj5f0gTSTh7S7n+G19LmRvYw==?=\"\n",
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
	GMimeReferences *refs, *copy;
	guint i;
	int j;
	
	for (i = 0; i < G_N_ELEMENTS (references); i++) {
		testsuite_check ("references[%u]", i);
		try {
			if (!(refs = g_mime_references_parse (options, references[i].input)))
				throw (exception_new ("failed to parse references"));
			
			if (g_mime_references_length (refs) != references[i].count)
				throw (exception_new ("number of references does not match"));
			
			for (j = 0; j < references[i].count; j++) {
				const char *msgid = g_mime_references_get_message_id (refs, j);
				
				if (strcmp (references[i].ids[j], msgid) != 0)
					throw (exception_new ("message ids do not match for ids[%d]", j));
			}
			
			copy = g_mime_references_copy (refs);
			if (g_mime_references_length (copy) != g_mime_references_length (refs))
				throw (exception_new ("number of copied references does not match"));
			
			for (j = 0; j < references[i].count; j++) {
				const char *msgid = g_mime_references_get_message_id (copy, j);
				
				if (strcmp (references[i].ids[j], msgid) != 0)
					throw (exception_new ("copied message ids do not match for ids[%d]", j));
			}
			
			g_mime_references_clear (copy);
			
			if (g_mime_references_length (copy) != 0)
				throw (exception_new ("references were not cleared"));
			
			g_mime_references_free (copy);
			g_mime_references_free (refs);
			
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
	testsuite_end ();
	
	testsuite_start ("addr-spec parser (loose)");
	g_mime_parser_options_set_rfc2047_compliance_mode (options, GMIME_RFC_COMPLIANCE_LOOSE);
	test_addrspec (options, TRUE);
	testsuite_end ();
	
	testsuite_start ("date parser");
	test_date_parser ();
	testsuite_end ();
	
	testsuite_start ("rfc2047 encoding/decoding (strict)");
	g_mime_parser_options_set_rfc2047_compliance_mode (options, GMIME_RFC_COMPLIANCE_STRICT);
	test_rfc2047 (options, FALSE);
	testsuite_end ();
	
	testsuite_start ("rfc2047 encoding/decoding (loose)");
	g_mime_parser_options_set_rfc2047_compliance_mode (options, GMIME_RFC_COMPLIANCE_LOOSE);
	test_rfc2047 (options, TRUE);
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
