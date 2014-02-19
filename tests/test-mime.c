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
	const char *display;
	const char *encoded;
} addrspec[] = {
	{ "fejj@helixcode.com",
	  "fejj@helixcode.com",
	  "fejj@helixcode.com" },
	{ "this is\n\ta folded name <folded@name.com>",
	  "this is a folded name <folded@name.com>",
	  "this is a folded name <folded@name.com>" },
	{ "Jeffrey Stedfast <fejj@helixcode.com>",
	  "Jeffrey Stedfast <fejj@helixcode.com>",
	  "Jeffrey Stedfast <fejj@helixcode.com>" },
	{ "Jeffrey \"fejj\" Stedfast <fejj@helixcode.com>",
	  "Jeffrey fejj Stedfast <fejj@helixcode.com>",
	  "Jeffrey fejj Stedfast <fejj@helixcode.com>" },
	{ "\"Jeffrey \\\"fejj\\\" Stedfast\" <fejj@helixcode.com>",
	  "Jeffrey \"fejj\" Stedfast <fejj@helixcode.com>",
	  "\"Jeffrey \\\"fejj\\\" Stedfast\" <fejj@helixcode.com>" },
	{ "\"Stedfast, Jeffrey\" <fejj@helixcode.com>",
	  "\"Stedfast, Jeffrey\" <fejj@helixcode.com>",
	  "\"Stedfast, Jeffrey\" <fejj@helixcode.com>" },
	{ "fejj@helixcode.com (Jeffrey Stedfast)",
	  "Jeffrey Stedfast <fejj@helixcode.com>",
	  "Jeffrey Stedfast <fejj@helixcode.com>" },
	{ "Jeff <fejj(recursive (comment) block)@helixcode.(and a comment here)com>",
	  "Jeff <fejj@helixcode.com>",
	  "Jeff <fejj@helixcode.com>" },
	{ "=?iso-8859-1?q?Kristoffer_Br=E5nemyr?= <ztion@swipenet.se>",
	  "Kristoffer Br\xc3\xa5nemyr <ztion@swipenet.se>",
	  "Kristoffer =?iso-8859-1?q?Br=E5nemyr?= <ztion@swipenet.se>" },
	{ "fpons@mandrakesoft.com (=?iso-8859-1?q?Fran=E7ois?= Pons)",
	  "Fran\xc3\xa7ois Pons <fpons@mandrakesoft.com>",
	  "=?iso-8859-1?q?Fran=E7ois?= Pons <fpons@mandrakesoft.com>" },
	{ "GNOME Hackers: miguel@gnome.org (Miguel de Icaza), Havoc Pennington <hp@redhat.com>;, fejj@helixcode.com",
	  "GNOME Hackers: Miguel de Icaza <miguel@gnome.org>, Havoc Pennington <hp@redhat.com>;, fejj@helixcode.com",
	  "GNOME Hackers: Miguel de Icaza <miguel@gnome.org>, Havoc Pennington <hp@redhat.com>;, fejj@helixcode.com" },
	{ "Local recipients: phil, joe, alex, bob",
	  "Local recipients: phil, joe, alex, bob;",
	  "Local recipients: phil, joe, alex, bob;" },
	{ "\":sysmail\"@  Some-Group. Some-Org,\n Muhammed.(I am  the greatest) Ali @(the)Vegas.WBA",
	  "\":sysmail\"@Some-Group.Some-Org, Muhammed.Ali@Vegas.WBA",
	  "\":sysmail\"@Some-Group.Some-Org, Muhammed.Ali@Vegas.WBA" },
	{ "Charles S. Kerr <charles@foo.com>",
	  "\"Charles S. Kerr\" <charles@foo.com>",
	  "\"Charles S. Kerr\" <charles@foo.com>" },
	{ "Charles \"Likes, to, put, commas, in, quoted, strings\" Kerr <charles@foo.com>",
	  "\"Charles Likes, to, put, commas, in, quoted, strings Kerr\" <charles@foo.com>",
	  "\"Charles Likes, to, put, commas, in, quoted, strings Kerr\" <charles@foo.com>" },
	{ "Charles Kerr, Pan Programmer <charles@superpimp.org>",
	  "\"Charles Kerr, Pan Programmer\" <charles@superpimp.org>",
	  "\"Charles Kerr, Pan Programmer\" <charles@superpimp.org>" },
	{ "Charles Kerr <charles@[127.0.0.1]>",
	  "Charles Kerr <charles@[127.0.0.1]>",
	  "Charles Kerr <charles@[127.0.0.1]>" },
	{ "Charles <charles@[127..0.1]>",
	  "Charles <charles@[127.0.1]>",
	  "Charles <charles@[127.0.1]>" },
	{ "Charles,, likes illegal commas <charles@superpimp.org>",
	  "Charles, likes illegal commas <charles@superpimp.org>",
	  "Charles, likes illegal commas <charles@superpimp.org>" },
	{ "<charles@>",
	  "charles",
	  "charles" },
	{ "<charles@broken.host.com.> (Charles Kerr)",
	  "Charles Kerr <charles@broken.host.com>",
	  "Charles Kerr <charles@broken.host.com>" },
	{ "fpons@mandrakesoft.com (=?iso-8859-1?q?Fran=E7ois?= Pons likes _'s and 	's too)",
	  "\"Fran\xc3\xa7ois Pons likes _'s and 	's too\" <fpons@mandrakesoft.com>",
	  "=?iso-8859-1?q?Fran=E7ois?= Pons likes _'s and 	's too <fpons@mandrakesoft.com>" },
	{ "T\x81\xf5ivo Leedj\x81\xe4rv <leedjarv@interest.ee>",
	  "T\xc2\x81\xc3\xb5ivo Leedj\xc2\x81\xc3\xa4rv <leedjarv@interest.ee>",
	  "=?iso-8859-1?b?VIH1aXZvIExlZWRqgeRydg==?= <leedjarv@interest.ee>" },
	{ "fbosi@mokabyte.it;, rspazzoli@mokabyte.it",
	  "fbosi@mokabyte.it, rspazzoli@mokabyte.it",
	  "fbosi@mokabyte.it, rspazzoli@mokabyte.it" },
	{ "\"Miles (Star Trekkin) O'Brian\" <mobrian@starfleet.org>",
	  "\"Miles (Star Trekkin) O'Brian\" <mobrian@starfleet.org>",
	  "\"Miles (Star Trekkin) O'Brian\" <mobrian@starfleet.org>" },
	{ "undisclosed-recipients: ;",
	  "undisclosed-recipients: ;",
	  "undisclosed-recipients: ;" },
	{ "undisclosed-recipients:;",
	  "undisclosed-recipients: ;",
	  "undisclosed-recipients: ;" },
	{ "undisclosed-recipients:",
	  "undisclosed-recipients: ;",
	  "undisclosed-recipients: ;" },
	{ "undisclosed-recipients",
	  "undisclosed-recipients",
	  "undisclosed-recipients" },
	/* The following 2 addr-specs are invalid according to the
	 * spec, but apparently some japanese cellphones use them?
	 * See Evolution bug #547969 */
	{ "some...dots@hocus.pocus.net",
	  "some...dots@hocus.pocus.net",
	  "some...dots@hocus.pocus.net" },
	{ "some.dots..@hocus.pocus.net",
	  "some.dots..@hocus.pocus.net",
	  "some.dots..@hocus.pocus.net" },
	/* The following test case is to check that we properly handle
	 * mailbox addresses that do not have any lwsp between the
	 * name component and the addr-spec. See Evolution bug
	 * #347520 */
	{ "Canonical Patch Queue Manager<pqm@pqm.ubuntu.com>",
	  "Canonical Patch Queue Manager <pqm@pqm.ubuntu.com>",
	  "Canonical Patch Queue Manager <pqm@pqm.ubuntu.com>" },
	/* Some examples pulled from rfc5322 */
	{ "Pete(A nice \\) chap) <pete(his account)@silly.test(his host)>",
	  "Pete <pete@silly.test>",
	  "Pete <pete@silly.test>" },
	{ "A Group(Some people):Chris Jones <c@(Chris's host.)public.example>, joe@example.org, John <jdoe@one.test> (my dear friend); (the end of the group)",
	  "A Group: Chris Jones <c@public.example>, joe@example.org, John <jdoe@one.test>;",
	  "A Group: Chris Jones <c@public.example>, joe@example.org, John <jdoe@one.test>;" },
	/* The following tests cases are meant to test forgivingness
	 * of the parser when it encounters unquoted specials in the
	 * name component */
	{ "Warren Worthington, Jr. <warren@worthington.com>",
	  "\"Warren Worthington, Jr.\" <warren@worthington.com>",
	  "\"Warren Worthington, Jr.\" <warren@worthington.com>" },
	{ "dot.com <dot.com>",
	  "\"dot.com\" <dot.com>",
	  "\"dot.com\" <dot.com>" },
	{ "=?UTF-8?Q?agatest123_\"test\"?= <agatest123@o2.pl>",
	  "agatest123 test <agatest123@o2.pl>",
	  "agatest123 test <agatest123@o2.pl>" },
	{ "\"=?ISO-8859-2?Q?TEST?=\" <p@p.org>",
	  "TEST <p@p.org>",
	  "TEST <p@p.org>" },
	{ "sdfasf@wp.pl,c tert@wp.pl,sffdg.rtre@op.pl",
	  "sdfasf@wp.pl, c, sffdg.rtre@op.pl",
	  "sdfasf@wp.pl, c, sffdg.rtre@op.pl" },
	
	/* obsolete routing address syntax tests */
	{ "<@route:user@domain.com>",
	  "user@domain.com",
	  "user@domain.com" },
	{ "<@route1,,@route2,,,@route3:user@domain.com>",
	  "user@domain.com",
	  "user@domain.com" },
};

static struct {
	const char *input;
	const char *display;
	const char *encoded;
} broken_addrspec[] = {
	{ "\"Biznes=?ISO-8859-2?Q?_?=INTERIA.PL\"=?ISO-8859-2?Q?_?=<biuletyny@firma.interia.pl>",
	  "\"Biznes INTERIA.PL \" <biuletyny@firma.interia.pl>",
	  "\"Biznes INTERIA.PL\" <biuletyny@firma.interia.pl>", },
	/* UTF-8 sequence split between multiple encoded-word tokens */
	{ "=?utf-8?Q?{#D=C3=A8=C3=A9=C2=A3=C3=A5=C3=BD_M$=C3=A1=C3?= =?utf-8?Q?=AD.=C3=A7=C3=B8m@#}?= <user@domain.com>",
	  "\"{#Dèé£åý M$áí.çøm@#}\" <user@domain.com>",
	  "=?iso-8859-1?b?eyNE6Omj5f0gTSTh7S7n+G1AI30=?= <user@domain.com>" },
	/* quoted-printable payload split between multiple encoded-word tokens */
	{ "=?utf-8?Q?{#D=C3=A8=C3=A9=C2=?= =?utf-8?Q?A3=C3=A5=C3=BD_M$=C3=A1=C?= =?utf-8?Q?3=AD.=C3=A7=C3=B8m@#}?= <user@domain.com>",
	  "\"{#Dèé£åý M$áí.çøm@#}\" <user@domain.com>",
	  "=?iso-8859-1?b?eyNE6Omj5f0gTSTh7S7n+G1AI30=?= <user@domain.com>" },
	/* base64 payload split between multiple encoded-word tokens */
	{ "=?iso-8859-1?b?ey?= =?iso-8859-1?b?NE6Omj5f0gTSTh7S7n+G1AI30=?= <user@domain.com>",
	  "\"{#Dèé£åý M$áí.çøm@#}\" <user@domain.com>",
	  "=?iso-8859-1?b?eyNE6Omj5f0gTSTh7S7n+G1AI30=?= <user@domain.com>" },
};

static void
test_addrspec (gboolean test_broken)
{
	InternetAddressList *addrlist;
	char *str;
	guint i;
	
	for (i = 0; i < G_N_ELEMENTS (addrspec); i++) {
		addrlist = NULL;
		str = NULL;
		
		testsuite_check ("addrspec[%u]", i);
		try {
			if (!(addrlist = internet_address_list_parse_string (addrspec[i].input)))
				throw (exception_new ("could not parse addr-spec"));
			
			str = internet_address_list_to_string (addrlist, FALSE);
			if (strcmp (addrspec[i].display, str) != 0)
				throw (exception_new ("display addr-spec %s does not match: %s", addrspec[i].display, str));
			g_free (str);
			
			str = internet_address_list_to_string (addrlist, TRUE);
			if (strcmp (addrspec[i].encoded, str) != 0)
				throw (exception_new ("encoded addr-spec %s does not match: %s", addrspec[i].encoded, str));
			
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
				if (!(addrlist = internet_address_list_parse_string (broken_addrspec[i].input)))
					throw (exception_new ("could not parse addr-spec"));
				
				str = internet_address_list_to_string (addrlist, FALSE);
				if (strcmp (broken_addrspec[i].display, str) != 0)
					throw (exception_new ("display addr-spec %s does not match: %s", broken_addrspec[i].display, str));
				g_free (str);
				
				str = internet_address_list_to_string (addrlist, TRUE);
				if (strcmp (broken_addrspec[i].encoded, str) != 0)
					throw (exception_new ("encoded addr-spec %s does not match: %s", broken_addrspec[i].encoded, str));
				
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
};

static void
test_date_parser (void)
{
	time_t date;
	int tzone;
	char *buf;
	guint i;
	
	for (i = 0; i < G_N_ELEMENTS (dates); i++) {
		buf = NULL;
		testsuite_check ("Date: '%s'", dates[i].in);
		try {
			date = g_mime_utils_header_decode_date (dates[i].in, &tzone);
			
			if (date != dates[i].date)
				throw (exception_new ("time_t's do not match"));
			
			if (tzone != dates[i].tzone)
				throw (exception_new ("timezones do not match"));
			
			buf = g_mime_utils_header_format_date (date, tzone);
			if (strcmp (dates[i].out, buf) != 0)
				throw (exception_new ("date strings do not match"));
			
			testsuite_check_passed ();
		} catch (ex) {
			testsuite_check_failed ("Date: '%s': %s", dates[i].in,
						ex->message);
		} finally;
		
		g_free (buf);
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
	  "=?iso-8859-5?b?tra2tra2tra2tra2tra2tra2tra2tra2tra2tra2tra2tra2trY=?= =?iso-8859-5?b?tra2tra2tra2tra2tra2tra2tra2tra2tra2tra2tra2tra2trY=?= =?iso-8859-5?b?tra2tg==?=" },
	{ "=?iso-8859-1?q?Jobbans=F6kan?= - duktig =?iso-8859-1?q?researcher=2Fomv=E4rldsbevakare=2Fomv=E4rldsan?= =?us-ascii?q?alytiker?=",
	  "Jobbansökan - duktig researcher/omvärldsbevakare/omvärldsanalytiker",
	  "=?iso-8859-1?q?Jobbans=F6kan?= - duktig =?iso-8859-1?q?researcher=2Fomv=E4rldsbevakare=2Fomv=E4rldsan?= =?us-ascii?q?alytiker?=" },
};

static struct {
	const char *input;
	const char *decoded;
	const char *encoded;
} broken_rfc2047_text[] = {
	{ "=?iso-8859-1?q?Jobbans=F6kan?= - duktig =?iso-8859-1?q?researcher=2Fomv=E4rldsbevakare=2Fomv=E4rldsan?=alytiker",
	  "Jobbansökan - duktig researcher/omvärldsbevakare/omvärldsanalytiker",
	  "=?iso-8859-1?q?Jobbans=F6kan?= - duktig =?iso-8859-1?q?researcher=2Fomv=E4rldsbevakare=2Fomv=E4rldsan?= =?us-ascii?q?alytiker?=" },
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
test_rfc2047 (gboolean test_broken)
{
	char *enc, *dec;
	guint i;
	
	for (i = 0; i < G_N_ELEMENTS (rfc2047_text); i++) {
		dec = enc = NULL;
		testsuite_check ("rfc2047_text[%u]", i);
		try {
			dec = g_mime_utils_header_decode_text (rfc2047_text[i].input);
			if (strcmp (rfc2047_text[i].decoded, dec) != 0)
				throw (exception_new ("decoded text does not match: %s", dec));
			
			enc = g_mime_utils_header_encode_text (dec);
			if (strcmp (rfc2047_text[i].encoded, enc) != 0)
				throw (exception_new ("encoded text does not match: %s", enc));
			
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
			dec = g_mime_utils_header_decode_text (broken_rfc2047_text[i].input);
			if (strcmp (broken_rfc2047_text[i].decoded, dec) != 0)
				throw (exception_new ("decoded text does not match: %s", dec));
			
			enc = g_mime_utils_header_encode_text (dec);
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
			
			enc = g_mime_utils_header_encode_phrase (dec);
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
	{ "Subject: qqqq wwwwwww [eee 1234]=?UTF-8?Q?=20=D0=95=D0=BC=D0=BC=D0=BC=D0=BC=D0=BC=D0=BC=D0=BC=D0=BC=D0=BC=D0=BC=D0=BC=20=D0=BC=D0=BC=D0=BC=D0=BC=D0=BC=D0=BC=D0=BC=D0=BC=D0=BC=D0=BC=D0=BC=D0=BC=D0=BC=D0=BC=20=D0=BC=D0=BC=D0=BC=D0=BC=D0=BC=D0=BC=D0=BC=D0=BC=D0=BC=D0=BC=D0=BC=D0=BC=D0=BC=D0=BC=20=D0=BC=D0=BC=D0=BC=D0=BC=D0=BC=D0=BC=D0=BC=D0=BC=D0=BC=D0=BC=D0=BC=D0=BC=D0=BC=D0=BC?=",
	  "Subject: qqqq wwwwwww [eee 1234]\n =?UTF-8?Q?=20=D0=95=D0=BC=D0=BC=D0=BC=D0=BC=D0=BC=D0=BC=D0=BC=D0=BC=D0=BC=D0=BC=D0=BC=20=D0=BC=D0=BC=D0=BC=D0=BC=D0=BC=D0=BC=D0=BC=D0=BC=D0=BC=D0=BC=D0=BC=D0=BC=D0=BC=D0=BC=20=D0=BC=D0=BC=D0=BC=D0=BC=D0=BC=D0=BC=D0=BC=D0=BC=D0=BC=D0=BC=D0=BC=D0=BC=D0=BC=D0=BC=20=D0=BC=D0=BC=D0=BC=D0=BC=D0=BC=D0=BC=D0=BC=D0=BC=D0=BC=D0=BC=D0=BC=D0=BC=D0=BC=D0=BC?=\n" },
};

static void
test_header_folding (void)
{
	char *folded;
	guint i;
	
	for (i = 0; i < G_N_ELEMENTS (header_folding); i++) {
	        folded = NULL;
		testsuite_check ("header_folding[%u]", i);
		try {
			folded = g_mime_utils_unstructured_header_fold (header_folding[i].input);
			if (strcmp (header_folding[i].folded, folded) != 0)
				throw (exception_new ("folded text does not match: -->%s<-- vs -->%s<--", header_folding[i].folded, folded));
			
			testsuite_check_passed ();
		} catch (ex) {
			testsuite_check_failed ("header_folding[%u]: %s", i, ex->message);
		} finally;
		
		g_free (folded);
	}
}

static struct {
	const char *input;
	const char *encoded;
} rfc2184[] = {
	{ "this is a really really long filename that should force gmime to rfc2184 encode it - yay!.html",
	  "Content-Disposition: attachment;\n\t"
	  "filename*0*=iso-8859-1''this%20is%20a%20really%20really%20long%20filename;\n\t"
	  "filename*1*=%20that%20should%20force%20gmime%20to%20rfc2184%20encode%20it;\n\t"
	  "filename*2*=%20-%20yay!.html\n" },
};

static void
test_rfc2184 (void)
{
	GMimeParam param, *params;
	GString *str;
	size_t n;
	guint i;
	
	param.next = NULL;
	param.name = "filename";
	
	str = g_string_new ("Content-Disposition: attachment");
	n = str->len;
	
	for (i = 0; i < G_N_ELEMENTS (rfc2184); i++) {
		params = NULL;
		
		param.value = (char *) rfc2184[i].input;
		
		testsuite_check ("rfc2184[%u]", i);
		try {
			g_mime_param_write_to_string (&param, TRUE, str);
			if (strcmp (rfc2184[i].encoded, str->str) != 0)
				throw (exception_new ("encoded param does not match"));
			
			if (!(params = g_mime_param_new_from_string (str->str + n + 2)))
				throw (exception_new ("could not parse encoded param list"));
			
			if (params->next != NULL)
				throw (exception_new ("parsed more than a single param?"));
			
			if (strcmp (rfc2184[i].input, params->value) != 0)
				throw (exception_new ("parsed param value does not match"));
			
			testsuite_check_passed ();
		} catch (ex) {
			testsuite_check_failed ("rfc2184[%u]: %s", i, ex->message);
		} finally;
		
		if (params != NULL)
			g_mime_param_destroy (params);
		
		g_string_truncate (str, n);
	}
	
	g_string_free (str, TRUE);
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


int main (int argc, char **argv)
{
	g_mime_init (0);
	
	testsuite_init (argc, argv);
	
	testsuite_start ("addr-spec parser");
	test_addrspec (FALSE);
	testsuite_end ();
	
	testsuite_start ("date parser");
	test_date_parser ();
	testsuite_end ();
	
	testsuite_start ("rfc2047 encoding/decoding");
	test_rfc2047 (FALSE);
	testsuite_end ();
	
	testsuite_start ("rfc2184 encoding/decoding");
	test_rfc2184 ();
	testsuite_end ();
	
	testsuite_start ("quoted-strings");
	test_qstring ();
	testsuite_end ();
	
	g_mime_shutdown ();
	
	g_mime_init (GMIME_ENABLE_RFC2047_WORKAROUNDS);
	testsuite_start ("broken rfc2047 encoding/decoding");
	test_header_folding ();
	test_addrspec (TRUE);
	test_rfc2047 (TRUE);
	testsuite_end ();
	g_mime_shutdown ();
	
	return testsuite_exit ();
}
