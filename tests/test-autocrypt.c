/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*  GMime
 *  Copyright (C) 2000-2017 Jeffrey Stedfast
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

#include <gmime/gmime.h>
#include <gmime/gmime-autocrypt.h>

#include <stdio.h>
#include <stdlib.h>

#include "testsuite.h"

extern int verbose;

#define d(x)
#define v(x) if (verbose > 3) x

struct _ah_gen_test {
	const char *addr;
	int keydatacount;
	char keybyte;
	gint64 timestamp;
	const char *txt;
};

static GMimeAutocryptHeader*
_gen_header (const struct _ah_gen_test *t)
{
	GMimeAutocryptHeader *ah = NULL;
	GByteArray *keydata = NULL;
	if (t == NULL)
		return NULL;
	if (!(ah = g_mime_autocrypt_header_new())) {
		fprintf (stderr, "failed to make a new header");
		return NULL;
	}
	if (t->keydatacount)
		if (!(keydata = g_byte_array_new_take ((guint8*) g_strnfill (t->keydatacount, t->keybyte), t->keydatacount))) {
			fprintf (stderr, "failed to make a new keydata");
			g_object_unref (ah);
			return NULL;
		}
	g_mime_autocrypt_header_set_address_from_string (ah, t->addr);
	g_mime_autocrypt_header_set_keydata (ah, keydata);
	if (t->timestamp) {
		GDateTime *ts = g_date_time_new_from_unix_utc (t->timestamp);
		g_mime_autocrypt_header_set_effective_date (ah, ts);
		g_date_time_unref (ts);
	}
	if (keydata)
		g_byte_array_unref (keydata);
	return ah;
}

/* generates a header list based on a series with an addr=NULL sentinel value */
static GMimeAutocryptHeaderList*
_gen_header_list (const struct _ah_gen_test **tests)
{
	GMimeAutocryptHeaderList *ret = g_mime_autocrypt_header_list_new ();
	for (; *tests; tests++) {
		GMimeAutocryptHeader *ah = _gen_header (*tests);
		if (!ah) {
			fprintf (stderr, "failed to generate header <%s>", (*tests)->addr);
			g_object_unref (ret);
			return NULL;
		}
		g_mime_autocrypt_header_list_add (ret, ah);
		g_object_unref (ah);
	}
	return ret;
}

const static struct _ah_gen_test gen_test_data[] = {
	{ .addr = "test@example.org",
	  .keydatacount = 102,
	  .keybyte = '\013',
	  .txt = "addr=test@example.org; keydata=CwsLCwsLCwsLCwsLCwsLCwsLCwsL\r\n"
	  " CwsLCwsLCwsLCwsLCwsLCwsLCwsLCwsLCwsLCwsLCwsLCwsLCwsLCwsLCwsLCwsLCwsLCwsL\r\n"
	  " CwsLCwsLCwsLCwsLCwsLCwsLCwsLCwsLCwsL",
	}
};

const static struct _ah_gen_test * no_addrs[] = {
	NULL, /* sentinel */
};


static void
test_ah_generation (void)
{
	GMimeAutocryptHeader *ah = NULL;
	char *str = NULL;
	unsigned int i;
	for (i = 0; i < G_N_ELEMENTS(gen_test_data); i++) {
		try {
			testsuite_check ("Autocrypt header[%u]", i);
			const struct _ah_gen_test *test = gen_test_data + i;
			ah = _gen_header (test);

			str = g_mime_autocrypt_header_get_string (ah);
			if (strcmp (test->txt, str)) {
				fprintf (stderr, "expected[%u]: \n%s\n\ngot:\n%s\n", i,
					 test->txt, str);
				throw (exception_new ("failed comparison"));
			}
			GMimeAutocryptHeader *ah2 = g_mime_autocrypt_header_new_from_string (str);
			gint cmp = g_mime_autocrypt_header_compare (ah, ah2);
			if (cmp) {
				char *x = g_mime_autocrypt_header_get_string (ah2);
				fprintf (stderr, "after-rebuild[%u] (%d) \nexpected: \n%s\n\ngot:\n%s\n", i,
					 cmp, test->txt, x);
				g_free(x);
				throw (exception_new ("header from string did not match"));
			}
			
			testsuite_check_passed ();
		} catch (ex) {
			testsuite_check_failed ("autocrypt header creation failed: %s", ex->message);
		} finally;
		if (ah)
			g_object_unref (ah);
		g_free (str);
		str = NULL;
	}
}


struct _ah_parse_test {
	const char *name;
	const struct _ah_gen_test *acheader;
	const struct _ah_gen_test **gossipheaders;
	const char *msg;
	const char *innerpart;
};

const static struct _ah_gen_test alice_addr =
	{ .addr = "alice@example.org",
	  .keydatacount = 102,
	  .timestamp = 1508774054,
	  .keybyte = '\013',
	};


const static struct _ah_gen_test alice_incomplete =
	{ .addr = "alice@example.org",
	  .timestamp = 1508774054,
	};


const static struct _ah_gen_test bob_addr =
	{ .addr = "bob@example.org",
	  .keydatacount = 99,
	  .timestamp = 1508774054,
	  .keybyte = '\133' };

const static struct _ah_gen_test carol_addr =
	{ .addr = "carol@example.org",
	  .keydatacount = 108,
	  .timestamp = 1508774054,
	  .keybyte = '\131' };

const static struct _ah_gen_test bob_incomplete =
	{ .addr = "bob@example.org",
	  .timestamp = 1508774054,
	};

const static struct _ah_gen_test *just_bob[] = {
	&bob_addr,
	NULL,
};

const static struct _ah_gen_test *bob_and_carol[] = {
	&bob_addr,
	&carol_addr,
	NULL,
};

const static struct _ah_parse_test parse_test_data[] = {
	{ .name = "simple",
	  .acheader = &alice_addr,
	  .gossipheaders = NULL,
	  .msg = "From: alice@example.org\r\n"
	  "To: bob@example.org\r\n"
	  "Subject: A lovely day\r\n"
	  "Message-Id: <lovely-day@example.net>\r\n"
	  "Date: Mon, 23 Oct 2017 11:54:14 -0400\r\n"
	  "Autocrypt: addr=alice@example.org; keydata=CwsLCwsLCwsLCwsLCwsLCwsLCwsL\r\n"
	  " CwsLCwsLCwsLCwsLCwsLCwsLCwsLCwsLCwsLCwsLCwsLCwsLCwsLCwsLCwsLCwsLCwsL\r\n"
	  " CwsLCwsLCwsLCwsLCwsLCwsLCwsLCwsLCwsLCwsL\r\n"
	  "Mime-Version: 1.0\r\n"
	  "Content-Type: text/plain\r\n"
	  "\r\n"
	  "Isn't it a lovely day?\r\n",
	},
	
	{ .name = "simple+onegossip",
	  .acheader = &alice_addr,
	  .gossipheaders = just_bob,
	  .msg = "From: alice@example.org\r\n"
	  "To: bob@example.org, carol@example.org\r\n"
	  "Subject: A gossipy lovely day\r\n"
	  "Message-Id: <simple-one-gossip@example.net>\r\n"
	  "Date: Mon, 23 Oct 2017 11:54:14 -0400\r\n"
	  "Autocrypt: addr=alice@example.org; keydata=CwsLCwsLCwsLCwsLCwsLCwsLCwsL\r\n"
	  " CwsLCwsLCwsLCwsLCwsLCwsLCwsLCwsLCwsLCwsLCwsLCwsLCwsLCwsLCwsLCwsLCwsL\r\n"
	  " CwsLCwsLCwsLCwsLCwsLCwsLCwsLCwsLCwsLCwsL\r\n"
	  "Mime-Version: 1.0\r\n"
	  "Content-Type: multipart/encrypted;\r\n"
	  " protocol=\"application/pgp-encrypted\";\r\n"
	  " boundary=\"boundary\"\r\n"
	  "\r\n"
	  "This is an OpenPGP/MIME encrypted message (RFC 4880 and 3156)\r\n"
	  "--boundary\r\n"
	  "Content-Type: application/pgp-encrypted\r\n"
	  "Content-Description: PGP/MIME version identification\r\n"
	  "\r\n"
	  "Version: 1\r\n"
	  "\r\n"
	  "--boundary\r\n"
	  "Content-Type: application/octet-stream; name=\"encrypted.asc\"\r\n"
	  "Content-Description: OpenPGP encrypted message\r\n"
	  "Content-Disposition: inline; filename=\"encrypted.asc\"\r\n"
	  "\r\n"
	  "-----BEGIN PGP MESSAGE-----\r\n"
	  "\r\n"
	  "NOTREALLYOPENPGPJUSTATEST\r\n"
	  "-----END PGP MESSAGE-----\r\n"
	  "\r\n"
	  "--boundary--\r\n",
	  .innerpart = "Content-Type: text/plain\r\n"
	  "Autocrypt-Gossip: addr=bob@example.org; keydata=W1tbW1tbW1tbW1tbW1tbW1tbW1tbW1tb\r\n"
	  " W1tbW1tbW1tbW1tbW1tbW1tbW1tbW1tbW1tbW1tbW1tbW1tbW1tbW1tb\r\n"
	  " W1tbW1tbW1tbW1tbW1tbW1tbW1tbW1tbW1tbW1tbW1tb\r\n"
	  "\r\n"
	  "Isn't a lovely day?  Now Carol can encrypt to Bob, hopefully.\r\n",
	},
	
	{ .name = "simple+nogossip",
	  .acheader = &alice_addr,
	  .gossipheaders = no_addrs,
	  .msg = "From: alice@example.org\r\n"
	  "To: bob@example.org, carol@example.org\r\n"
	  "Subject: A gossipy lovely day\r\n"
	  "Message-Id: <simple-no-gossip@example.net>\r\n"
	  "Date: Mon, 23 Oct 2017 11:54:14 -0400\r\n"
	  "Autocrypt: addr=alice@example.org; keydata=CwsLCwsLCwsLCwsLCwsLCwsLCwsL\r\n"
	  " CwsLCwsLCwsLCwsLCwsLCwsLCwsLCwsLCwsLCwsLCwsLCwsLCwsLCwsLCwsLCwsLCwsL\r\n"
	  " CwsLCwsLCwsLCwsLCwsLCwsLCwsLCwsLCwsLCwsL\r\n"
	  "Mime-Version: 1.0\r\n"
	  "Content-Type: multipart/encrypted;\r\n"
	  " protocol=\"application/pgp-encrypted\";\r\n"
	  " boundary=\"boundary\"\r\n"
	  "\r\n"
	  "This is an OpenPGP/MIME encrypted message (RFC 4880 and 3156)\r\n"
	  "--boundary\r\n"
	  "Content-Type: application/pgp-encrypted\r\n"
	  "Content-Description: PGP/MIME version identification\r\n"
	  "\r\n"
	  "Version: 1\r\n"
	  "\r\n"
	  "--boundary\r\n"
	  "Content-Type: application/octet-stream; name=\"encrypted.asc\"\r\n"
	  "Content-Description: OpenPGP encrypted message\r\n"
	  "Content-Disposition: inline; filename=\"encrypted.asc\"\r\n"
	  "\r\n"
	  "-----BEGIN PGP MESSAGE-----\r\n"
	  "\r\n"
	  "NOTREALLYOPENPGPJUSTATEST\r\n"
	  "-----END PGP MESSAGE-----\r\n"
	  "\r\n"
	  "--boundary--\r\n",
	  .innerpart = "Content-Type: text/plain\r\n"
	  "\r\n"
	  "Isn't a lovely day?  I sure hope Bob and Carol have each other's info\r\n"
	  "because otherwise they won't be able to Reply All.\r\n",
	},
	
	{ .name = "simple+fullgossip",
	  .acheader = &alice_addr,
	  .gossipheaders = bob_and_carol,
	  .msg = "From: alice@example.org\r\n"
	  "To: bob@example.org, carol@example.org\r\n"
	  "Subject: A gossipy lovely day\r\n"
	  "Message-Id: <simple-full-gossip@example.net>\r\n"
	  "Date: Mon, 23 Oct 2017 11:54:14 -0400\r\n"
	  "Autocrypt: addr=alice@example.org; keydata=CwsLCwsLCwsLCwsLCwsLCwsLCwsL\r\n"
	  " CwsLCwsLCwsLCwsLCwsLCwsLCwsLCwsLCwsLCwsLCwsLCwsLCwsLCwsLCwsLCwsLCwsL\r\n"
	  " CwsLCwsLCwsLCwsLCwsLCwsLCwsLCwsLCwsLCwsL\r\n"
	  "Mime-Version: 1.0\r\n"
	  "Content-Type: multipart/encrypted;\r\n"
	  " protocol=\"application/pgp-encrypted\";\r\n"
	  " boundary=\"boundary\"\r\n"
	  "\r\n"
	  "This is an OpenPGP/MIME encrypted message (RFC 4880 and 3156)\r\n"
	  "--boundary\r\n"
	  "Content-Type: application/pgp-encrypted\r\n"
	  "Content-Description: PGP/MIME version identification\r\n"
	  "\r\n"
	  "Version: 1\r\n"
	  "\r\n"
	  "--boundary\r\n"
	  "Content-Type: application/octet-stream; name=\"encrypted.asc\"\r\n"
	  "Content-Description: OpenPGP encrypted message\r\n"
	  "Content-Disposition: inline; filename=\"encrypted.asc\"\r\n"
	  "\r\n"
	  "-----BEGIN PGP MESSAGE-----\r\n"
	  "\r\n"
	  "NOTREALLYOPENPGPJUSTATEST\r\n"
	  "-----END PGP MESSAGE-----\r\n"
	  "\r\n"
	  "--boundary--\r\n",
	  .innerpart = "Content-Type: text/plain\r\n"
	  "Autocrypt-Gossip: addr=bob@example.org; keydata=W1tbW1tbW1tbW1tbW1tbW1tbW1tbW1tb\r\n"
	  " W1tbW1tbW1tbW1tbW1tbW1tbW1tbW1tbW1tbW1tbW1tbW1tbW1tbW1tb\r\n"
	  " W1tbW1tbW1tbW1tbW1tbW1tbW1tbW1tbW1tbW1tbW1tb\r\n"
	  "Autocrypt-Gossip: addr=carol@example.org; keydata=WVlZWVlZWVlZWVlZWVlZWVlZWVlZ\r\n"
	  " WVlZWVlZWVlZWVlZWVlZWVlZWVlZWVlZWVlZWVlZWVlZWVlZWVlZWVlZWVlZWVlZWVlZWVlZ\r\n"
	  " WVlZWVlZWVlZWVlZWVlZWVlZWVlZWVlZWVlZWVlZWVlZ\r\n"
	  "\r\n"
	  "Isn't a lovely day?  Now Carol and Bob can now both Reply All, hopefully.\r\n",
	},
		
	{ .name = "simple+excessgossip",
	  .acheader = &alice_addr,
	  .gossipheaders = just_bob,
	  .msg = "From: alice@example.org\r\n"
	  "To: bob@example.org\r\n"
	  "Subject: A gossipy lovely day\r\n"
	  "Message-Id: <simple-excess-gossip@example.net>\r\n"
	  "Date: Mon, 23 Oct 2017 11:54:14 -0400\r\n"
	  "Autocrypt: addr=alice@example.org; keydata=CwsLCwsLCwsLCwsLCwsLCwsLCwsL\r\n"
	  " CwsLCwsLCwsLCwsLCwsLCwsLCwsLCwsLCwsLCwsLCwsLCwsLCwsLCwsLCwsLCwsLCwsL\r\n"
	  " CwsLCwsLCwsLCwsLCwsLCwsLCwsLCwsLCwsLCwsL\r\n"
	  "Mime-Version: 1.0\r\n"
	  "Content-Type: multipart/encrypted;\r\n"
	  " protocol=\"application/pgp-encrypted\";\r\n"
	  " boundary=\"boundary\"\r\n"
	  "\r\n"
	  "This is an OpenPGP/MIME encrypted message (RFC 4880 and 3156)\r\n"
	  "--boundary\r\n"
	  "Content-Type: application/pgp-encrypted\r\n"
	  "Content-Description: PGP/MIME version identification\r\n"
	  "\r\n"
	  "Version: 1\r\n"
	  "\r\n"
	  "--boundary\r\n"
	  "Content-Type: application/octet-stream; name=\"encrypted.asc\"\r\n"
	  "Content-Description: OpenPGP encrypted message\r\n"
	  "Content-Disposition: inline; filename=\"encrypted.asc\"\r\n"
	  "\r\n"
	  "-----BEGIN PGP MESSAGE-----\r\n"
	  "\r\n"
	  "NOTREALLYOPENPGPJUSTATEST\r\n"
	  "-----END PGP MESSAGE-----\r\n"
	  "\r\n"
	  "--boundary--\r\n",
	  .innerpart = "Content-Type: text/plain\r\n"
	  "Autocrypt-Gossip: addr=bob@example.org; keydata=W1tbW1tbW1tbW1tbW1tbW1tbW1tbW1tb\r\n"
	  " W1tbW1tbW1tbW1tbW1tbW1tbW1tbW1tbW1tbW1tbW1tbW1tbW1tbW1tb\r\n"
	  " W1tbW1tbW1tbW1tbW1tbW1tbW1tbW1tbW1tbW1tbW1tb\r\n"
	  "Autocrypt-Gossip: addr=carol@example.org; keydata=WVlZWVlZWVlZWVlZWVlZWVlZWVlZ\r\n"
	  " WVlZWVlZWVlZWVlZWVlZWVlZWVlZWVlZWVlZWVlZWVlZWVlZWVlZWVlZWVlZWVlZWVlZWVlZ\r\n"
	  " WVlZWVlZWVlZWVlZWVlZWVlZWVlZWVlZWVlZWVlZWVlZ\r\n"
	  "\r\n"
	  "Recipients of this message should not accept carol's public key for gossip, since\r\n"
	  "the message was not addressed to her\r\n"
	},
	
	{ .name = "simple+badgossip",
	  .acheader = &alice_addr,
	  .gossipheaders = NULL,
	  .msg = "From: alice@example.org\r\n"
	  "To: bob@example.org, carol@example.org\r\n"
	  "Subject: A gossipy lovely day\r\n"
	  "Message-Id: <lovely-badgossip-day@example.net>\r\n"
	  "Date: Mon, 23 Oct 2017 11:54:14 -0400\r\n"
	  "Autocrypt: addr=alice@example.org; keydata=CwsLCwsLCwsLCwsLCwsLCwsLCwsL\r\n"
	  " CwsLCwsLCwsLCwsLCwsLCwsLCwsLCwsLCwsLCwsLCwsLCwsLCwsLCwsLCwsLCwsLCwsL\r\n"
	  " CwsLCwsLCwsLCwsLCwsLCwsLCwsLCwsLCwsLCwsL\r\n"
	  "Autocrypt-Gossip: addr=borb@example.org; keydata=W1tbW1tbW1tbW1tbW1tbW1tbW1tbW1tb\r\n"
	  " W1tbW1tbW1tbW1tbW1tbW1tbW1tbW1tbW1tbW1tbW1tbW1tbW1tbW1tb\r\n"
	  " W1tbW1tbW1tbW1tbW1tbW1tbW1tbW1tbW1tbW1tbW1tb\r\n"
	  "Autocrypt: addr=bob@example.org; keydata=W1tbW1tbW1tbW1tbW1tbW1tbW1tbW1tb\r\n"
	  " W1tbW1tbW1tbW1tbW1tbW1tbW1tbW1tbW1tbW1tbW1tbW1tbW1tbW1tb\r\n"
	  " W1tbW1tbW1tbW1tbW1tbW1tbW1tbW1tbW1tbW1tbW1tb\r\n"
	  "Mime-Version: 1.0\r\n"
	  "Content-Type: text/plain\r\n"
	  "\r\n"
	  "There are at least two headers here which will be ignored.\r\n",
	},
	
	{ .name = "duplicate",
	  .acheader = &alice_incomplete,
	  .gossipheaders = NULL,
	  .msg = "From: alice@example.org\r\n"
	  "To: bob@example.org\r\n"
	  "Subject: A lovely day\r\n"
	  "Message-Id: <duplicated-headers@example.net>\r\n"
	  "Date: Mon, 23 Oct 2017 11:54:14 -0400\r\n"
	  "Autocrypt: addr=alice@example.org; keydata=CwsLCwsLCwsLCwsLCwsLCwsLCwsL\r\n"
	  " CwsLCwsLCwsLCwsLCwsLCwsLCwsLCwsLCwsLCwsLCwsLCwsLCwsLCwsLCwsLCwsLCwsL\r\n"
	  " CwsLCwsLCwsLCwsLCwsLCwsLCwsLCwsLCwsLCwsL\r\n"
	  "Autocrypt: addr=alice@example.org; keydata=CwsLCwsLCwsLCwsLCwsLCwsLCwsLCwsL\r\n"
	  " CwsLCwsLCwsLCwsLCwsLCwsLCwsLCwsLCwsLCwsLCwsLCwsLCwsLCwsLCwsLCwsLCwsL\r\n"
	  " CwsLCwsLCwsLCwsLCwsLCwsLCwsLCwsLCwsLCwsL\r\n"
	  "Mime-Version: 1.0\r\n"
	  "Content-Type: text/plain\r\n"
	  "\r\n"
	  "Duplicate Autocrypt headers should cause none to match?\r\n",
	},

	{ .name = "unrecognized critical attribute",
	  .acheader = &alice_incomplete,
	  .gossipheaders = NULL,
	  .msg = "From: alice@example.org\r\n"
	  "To: bob@example.org\r\n"
	  "Subject: A lovely day\r\n"
	  "Message-Id: <unknown-critical-attribute@example.net>\r\n"
	  "Date: Mon, 23 Oct 2017 11:54:14 -0400\r\n"
	  "Autocrypt: addr=alice@example.org; emergency=true; keydata=CwsLCwsLCwsLCwsLCwsLCwsLCwsL\r\n"
	  " CwsLCwsLCwsLCwsLCwsLCwsLCwsLCwsLCwsLCwsLCwsLCwsLCwsLCwsLCwsLCwsLCwsL\r\n"
	  " CwsLCwsLCwsLCwsLCwsLCwsLCwsLCwsLCwsLCwsL\r\n"
	  "Mime-Version: 1.0\r\n"
	  "Content-Type: text/plain\r\n"
	  "\r\n"
	  "An unrecognized attribute that does not start with _ is critical and should not cause a match\r\n",
	},

	{ .name = "unrecognized critical attribute + simple",
	  .acheader = &alice_addr,
	  .gossipheaders = NULL,
	  .msg = "From: alice@example.org\r\n"
	  "To: bob@example.org\r\n"
	  "Subject: A lovely day\r\n"
	  "Message-Id: <unknown-critical+simple@example.net>\r\n"
	  "Date: Mon, 23 Oct 2017 11:54:14 -0400\r\n"
	  "Autocrypt: addr=alice@example.org; keydata=CwsLCwsLCwsLCwsLCwsLCwsLCwsL\r\n"
	  " CwsLCwsLCwsLCwsLCwsLCwsLCwsLCwsLCwsLCwsLCwsLCwsLCwsLCwsLCwsLCwsLCwsL\r\n"
	  " CwsLCwsLCwsLCwsLCwsLCwsLCwsLCwsLCwsLCwsL\r\n"
	  "Autocrypt: addr=alice@example.org; emergency=true; keydata=CwsLCwsLCwsLCwsLCwsLCwsLCwsL\r\n"
	  " CwsLCwsLCwsLCwsLCwsLCwsLCwsLCwsLCwsLCwsLCwsLCwsLCwsLCwsLCwsLCwsLCwsL\r\n"
	  " CwsLCwsLCwsLCwsLCwsLCwsLCwsLCwsLCwsLCwsL\r\n"
	  "Mime-Version: 1.0\r\n"
	  "Content-Type: text/plain\r\n"
	  "\r\n"
	  "Unknown Autocrypt critical attribute should cause nothing to match but should not block a classic type header\r\n",
	},
	
	{ .name = "unrecognized non-critical attribute",
	  .acheader = &alice_addr,
	  .gossipheaders = NULL,
	  .msg = "From: alice@example.org\r\n"
	  "To: bob@example.org\r\n"
	  "Subject: A lovely day\r\n"
	  "Message-Id: <unknown-critical-attribute@example.net>\r\n"
	  "Date: Mon, 23 Oct 2017 11:54:14 -0400\r\n"
	  "Autocrypt: addr=alice@example.org; _not_an_emergency=true; keydata=CwsLCwsLCwsLCwsLCwsLCwsLCwsL\r\n"
	  " CwsLCwsLCwsLCwsLCwsLCwsLCwsLCwsLCwsLCwsLCwsLCwsLCwsLCwsLCwsLCwsLCwsL\r\n"
	  " CwsLCwsLCwsLCwsLCwsLCwsLCwsLCwsLCwsLCwsL\r\n"
	  "Mime-Version: 1.0\r\n"
	  "Content-Type: text/plain\r\n"
	  "\r\n"
	  "An unrecognized attribute that does not start with _ is critical and should not cause a match\r\n",
	},

	{ .name = "no From: at all",
	  .acheader = NULL,
	  .gossipheaders = NULL,
	  .msg = "To: carol@example.org\r\n"
	  "Subject: A lovely day\r\n"
	  "Message-Id: <no-from@example.net>\r\n"
	  "Date: Mon, 23 Oct 2017 11:54:14 -0400\r\n"
	  "Autocrypt: addr=alice@example.org; keydata=CwsLCwsLCwsLCwsLCwsLCwsLCwsL\r\n"
	  " CwsLCwsLCwsLCwsLCwsLCwsLCwsLCwsLCwsLCwsLCwsLCwsLCwsLCwsLCwsLCwsLCwsL\r\n"
	  " CwsLCwsLCwsLCwsLCwsLCwsLCwsLCwsLCwsLCwsL\r\n"
	  "Mime-Version: 1.0\r\n"
	  "Content-Type: text/plain\r\n"
	  "\r\n"
	  "This message has no sender at all\r\n",
	},

	{ .name = "with Sender: header",
	  .acheader = &bob_incomplete,
	  .gossipheaders = NULL,
	  .msg = "From: bob@example.org\r\n"
	  "Sender: alice@example.org\r\n"
	  "To: carol@example.org\r\n"
	  "Subject: A lovely day\r\n"
	  "Message-Id: <with-sender-header@example.net>\r\n"
	  "Date: Mon, 23 Oct 2017 11:54:14 -0400\r\n"
	  "Autocrypt: addr=alice@example.org; keydata=CwsLCwsLCwsLCwsLCwsLCwsLCwsL\r\n"
	  " CwsLCwsLCwsLCwsLCwsLCwsLCwsLCwsLCwsLCwsLCwsLCwsLCwsLCwsLCwsLCwsLCwsL\r\n"
	  " CwsLCwsLCwsLCwsLCwsLCwsLCwsLCwsLCwsLCwsL\r\n"
	  "Mime-Version: 1.0\r\n"
	  "Content-Type: text/plain\r\n"
	  "\r\n"
	  "This message has an Autocrypt header that matches the Sender: attribute but not the From:\r\n",
	},

	{ .name = "no senders",
	  .acheader = NULL,
	  .gossipheaders = NULL,
	  .msg = "From: undisclosed sender\r\n"
	  "To: carol@example.org\r\n"
	  "Subject: A lovely day\r\n"
	  "Message-Id: <no-senders@example.net>\r\n"
	  "Date: Mon, 23 Oct 2017 11:54:14 -0400\r\n"
	  "Autocrypt: addr=alice@example.org; keydata=CwsLCwsLCwsLCwsLCwsLCwsLCwsL\r\n"
	  " CwsLCwsLCwsLCwsLCwsLCwsLCwsLCwsLCwsLCwsLCwsLCwsLCwsLCwsLCwsLCwsLCwsL\r\n"
	  " CwsLCwsLCwsLCwsLCwsLCwsLCwsLCwsLCwsLCwsL\r\n"
	  "Mime-Version: 1.0\r\n"
	  "Content-Type: text/plain\r\n"
	  "\r\n"
	  "This message has no sender at all\r\n",
	},

	{ .name = "two senders",
	  .acheader = NULL,
	  .gossipheaders = NULL,
	  .msg = "From: alice@example.org, bob@example.org\r\n"
	  "To: carol@example.org\r\n"
	  "Subject: A lovely day\r\n"
	  "Message-Id: <two-senders@example.net>\r\n"
	  "Date: Mon, 23 Oct 2017 11:54:14 -0400\r\n"
	  "Autocrypt: addr=alice@example.org; keydata=CwsLCwsLCwsLCwsLCwsLCwsLCwsL\r\n"
	  " CwsLCwsLCwsLCwsLCwsLCwsLCwsLCwsLCwsLCwsLCwsLCwsLCwsLCwsLCwsLCwsLCwsL\r\n"
	  " CwsLCwsLCwsLCwsLCwsLCwsLCwsLCwsLCwsLCwsL\r\n"
	  "Autocrypt: addr=bob@example.org; keydata=W1tbW1tbW1tbW1tbW1tbW1tbW1tbW1tb\r\n"
	  " W1tbW1tbW1tbW1tbW1tbW1tbW1tbW1tbW1tbW1tbW1tbW1tbW1tbW1tb\r\n"
	  " W1tbW1tbW1tbW1tbW1tbW1tbW1tbW1tbW1tbW1tbW1tb\r\n"
	  "Mime-Version: 1.0\r\n"
	  "Content-Type: text/plain\r\n"
	  "\r\n"
	  "When Alice and Bob are both present, we should not update Autocrypt state at all\r\n",
	},
};


/* returns a non-NULL error if they're not the same */
char *
_acheaderlists_compare (GMimeAutocryptHeaderList *expected, GMimeAutocryptHeaderList *got)
{
	if (g_mime_autocrypt_header_list_get_count (expected) != g_mime_autocrypt_header_list_get_count (got))
		return g_strdup_printf ("header counts: expected: %d, got: %d",
					g_mime_autocrypt_header_list_get_count (expected),
					g_mime_autocrypt_header_list_get_count (got));

	guint ai;
	for (ai = 0; ai < g_mime_autocrypt_header_list_get_count (expected); ai++) {
		GMimeAutocryptHeader *ahe = g_mime_autocrypt_header_list_get_header_at (expected, ai);
		GMimeAutocryptHeader *ahg = g_mime_autocrypt_header_list_get_header_for_address (got, g_mime_autocrypt_header_get_address (ahe));
		gint cmp = g_mime_autocrypt_header_compare (ahe, ahg);
		if (cmp) {
			char *e = g_mime_autocrypt_header_get_string (ahe);
			char *g = g_mime_autocrypt_header_get_string (ahg);
			char *ret = g_strdup_printf ("comparing <%s> got cmp = %d \nexpected: \n%s\n\ngot:\n%s\n",
						     internet_address_mailbox_get_idn_addr (g_mime_autocrypt_header_get_address (ahe)),
						     cmp, e, g);
			g_free(e);
			g_free(g);
			return ret;
		}
	}
	return NULL;
}

static void
test_ah_message_parse (void)
{
	guint i;
	for (i = 0; i < G_N_ELEMENTS (parse_test_data); i++) {
		GMimeAutocryptHeader *ah_expected = NULL;
		GMimeAutocryptHeader *ah_got = NULL;
		GMimeAutocryptHeaderList *gossip_expected = NULL;
		GMimeAutocryptHeaderList *gossip_got = NULL;
		GMimeMessage *message = NULL;
		GMimeObject *innerpart = NULL;
		const struct _ah_parse_test *test = parse_test_data + i;
		try {
			testsuite_check ("Autocrypt message[%u] (%s)", i, test->name);

			/* make GMimeMessage from test->msg */
			GMimeStream *stream = g_mime_stream_mem_new_with_buffer (test->msg, strlen(test->msg));
			GMimeParser *parser = g_mime_parser_new_with_stream (stream);
			message = g_mime_parser_construct_message (parser, NULL);
			g_object_unref (parser);
			g_object_unref (stream);
			/* make GMimeObject from test->innerpart */
			if (test->innerpart) {
				stream = g_mime_stream_mem_new_with_buffer (test->innerpart, strlen(test->innerpart));
				parser = g_mime_parser_new_with_stream (stream);
				innerpart = g_mime_parser_construct_part (parser, NULL);
				g_object_unref (parser);
				g_object_unref (stream);
			}
			

			/* check on autocrypt header */
			ah_expected = _gen_header (test->acheader);
			ah_got = g_mime_message_get_autocrypt_header (message, NULL);
			if (!ah_got && ah_expected)
				throw (exception_new ("failed to extract Autocrypt header from message!"));
			if (ah_got && !ah_expected)
				throw (exception_new ("extracted Autocrypt header when we shouldn't!\n%s\n",
						      g_mime_autocrypt_header_get_string (ah_got)));
			if (ah_expected)
				if (g_mime_autocrypt_header_compare (ah_expected, ah_got))
					throw (exception_new ("Autocrypt header did not match"));

			/* check on gossip */
			if (test->gossipheaders)
				gossip_expected = _gen_header_list (test->gossipheaders);
			if (innerpart) {
				gossip_got = g_mime_message_get_autocrypt_gossip_headers_from_inner_part (message, NULL, innerpart);
				if (!gossip_got)
					throw (exception_new ("failed to extract gossip headers from message!"));
			}
			if (!gossip_got && gossip_expected)
				throw (exception_new ("failed to extract Autocrypt gossip headers from message!"));
			if (gossip_got && !gossip_expected)
				throw (exception_new ("extracted Autocrypt gossip headers when we shouldn't!\n"));
			if (gossip_expected) {
				gchar *err = NULL;
				err = _acheaderlists_compare (gossip_expected, gossip_got);
				if (err)
					throw (exception_new ("gossip headers: %s", err));
			}
			testsuite_check_passed ();
		} catch (ex) {
			testsuite_check_failed ("autocrypt message parse[%u] (%s) failed: %s", i, test->name, ex->message);
		} finally;
		if (ah_expected)
			g_object_unref (ah_expected);
		if (ah_got)
			g_object_unref (ah_got);
		if (gossip_expected)
			g_object_unref (gossip_expected);
		if (gossip_got)
			g_object_unref (gossip_got);
	}
}


int main (int argc, char **argv)
{
	g_mime_init ();
	
	testsuite_init (argc, argv);
	
	testsuite_start ("Autocrypt: generate headers");
	test_ah_generation ();
	testsuite_end ();
	
	testsuite_start ("Autocrypt: parse messages");
	test_ah_message_parse ();
	testsuite_end ();
	
	g_mime_shutdown ();
	
	return testsuite_exit ();
}
