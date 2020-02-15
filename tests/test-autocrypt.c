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
	GBytes *keydata = NULL;
	if (t == NULL)
		return NULL;
	if (!(ah = g_mime_autocrypt_header_new())) {
		fprintf (stderr, "failed to make a new header");
		return NULL;
	}
	if (t->keydatacount)
		if (!(keydata = g_bytes_new_take ((guint8*) g_strnfill (t->keydatacount, t->keybyte), t->keydatacount))) {
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
		g_bytes_unref (keydata);
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
	  .txt = "addr=test@example.org; keydata=CwsLCwsLCwsLCwsLCwsLCwsLCwsL"
	  " CwsLCwsLCwsLCwsLCwsLCwsLCwsLCwsLCwsLCwsLCwsLCwsLCwsLCwsLCwsLCwsLCwsLCwsL"
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

			str = g_mime_autocrypt_header_to_string (ah, FALSE);
			if (strcmp (test->txt, str)) {
				fprintf (stderr, "expected[%u]: \n%s\n\ngot:\n%s\n", i,
					 test->txt, str);
				throw (exception_new ("failed comparison"));
			}
			GMimeAutocryptHeader *ah2 = g_mime_autocrypt_header_new_from_string (str);
			gint cmp = g_mime_autocrypt_header_compare (ah, ah2);
			if (cmp) {
				char *x = g_mime_autocrypt_header_to_string (ah2, FALSE);
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


struct _ah_inject_test {
	const char *name;
	const struct _ah_gen_test *acheader;
	const struct _ah_gen_test **gossipheaders;
	const char **encrypt_to;
	const char *before;
	const char *after;
	const char *inner_after;
};

static const char * local_recipients[] =
	{
		"0x0D211DC5D9F4567271AC0582D8DECFBFC9346CD4",
		NULL,
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

const static struct _ah_inject_test inject_test_data[] = {
	{ .name = "simple",
	  .acheader = &alice_addr,
	  .before = "From: alice@example.org\r\n"
	  "To: bob@example.org\r\n"
	  "Subject: A lovely day\r\n"
	  "Message-Id: <lovely-day@example.net>\r\n"
	  "Date: Mon, 23 Oct 2017 11:54:14 -0400\r\n"
	  "Mime-Version: 1.0\r\n"
	  "Content-Type: text/plain\r\n"
	  "\r\n"
	  "Isn't it a lovely day?\r\n",
	  .after = "From: alice@example.org\n"
	  "To: bob@example.org\n"
	  "Subject: A lovely day\n"
	  "Message-Id: <lovely-day@example.net>\n"
	  "Date: Mon, 23 Oct 2017 11:54:14 -0400\n"
	  "Mime-Version: 1.0\n"
	  "Autocrypt: addr=alice@example.org; keydata=CwsLCwsLCwsLCwsLCwsLCwsLCwsL\n"
	  " CwsLCwsLCwsLCwsLCwsLCwsLCwsLCwsLCwsLCwsLCwsLCwsLCwsLCwsLCwsLCwsLCwsLCwsL\n"
	  " CwsLCwsLCwsLCwsLCwsLCwsLCwsLCwsLCwsL\n"
	  "Content-Type: text/plain\n"
	  "\n"
	  "Isn't it a lovely day?\n",
	},
	{ .name = "gossip injection",
	  .acheader = &alice_addr,
	  .gossipheaders = bob_and_carol,
	  .encrypt_to = local_recipients,
	  .before = "From: alice@example.org\r\n"
	  "To: bob@example.org\r\n"
	  "Subject: A lovely day\r\n"
	  "Message-Id: <lovely-day@example.net>\r\n"
	  "Date: Mon, 23 Oct 2017 11:54:14 -0400\r\n"
	  "Mime-Version: 1.0\r\n"
	  "Content-Type: text/plain\r\n"
	  "\r\n"
	  "Isn't it a lovely day?\r\n",
	  .inner_after = "Content-Type: text/plain\n"
	  "Autocrypt-Gossip: addr=bob@example.org; keydata=W1tbW1tbW1tbW1tbW1tbW1tbW1tb\n"
	  " W1tbW1tbW1tbW1tbW1tbW1tbW1tbW1tbW1tbW1tbW1tbW1tbW1tbW1tbW1tbW1tbW1tbW1tb\n"
	  " W1tbW1tbW1tbW1tbW1tbW1tbW1tbW1tb\n"
	  "Autocrypt-Gossip: addr=carol@example.org; keydata=WVlZWVlZWVlZWVlZWVlZWVlZWVlZ\n"
	  " WVlZWVlZWVlZWVlZWVlZWVlZWVlZWVlZWVlZWVlZWVlZWVlZWVlZWVlZWVlZWVlZWVlZWVlZ\n"
	  " WVlZWVlZWVlZWVlZWVlZWVlZWVlZWVlZWVlZWVlZWVlZ\n"
	  "\n"
	  "Isn't it a lovely day?\n",
	},
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
		
	{ .name = "actually encrypted, fullgossip",
	  .acheader = &alice_addr,
	  .gossipheaders = bob_and_carol,
	  .msg = "From: alice@example.org\r\n"
	  "To: bob@example.org, carol@example.org\r\n"
	  "Subject: A gossipy lovely day\r\n"
	  "Message-Id: <encrypted-full-gossip@example.net>\r\n"
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
	  "hQGMA4xh3ftdkAY8AQv+NU4HDHKzqSk309FOoGCNfTIM16+LrT3TY+pwdQ+BHZNh\r\n"
	  "v62TfRrG3PFd46tvH3zInIHjow7Usb3Au+nz1fF0HgIkOg7IEGXUle0OuPgQt38i\r\n"
	  "J2B+7EhksG86aaGmlsCq7Y8v9QnBH/UsX95xSHOTpIgWDamdGed2nnqW0fdOtapK\r\n"
	  "QyfOWkmti8vUnzvDPxiEMLr2VW5UWtyJQiu6BwyEpme15KkmO0TJUNJ71N8cWKfD\r\n"
	  "+jK2qlQzlgKHeSy3cWmu6ejhkTqPOghxsgb6lGHNu4+/vHufZkZCKBOYrPq/6pLr\r\n"
	  "zySDS6p8+LsDf5WwbR3u1TENxUz1YfNDmFi0FcVRPgdbx6NsUe0EQgTudqMRJ7q4\r\n"
	  "6uID8HLG3p/i3nX3QbuJJZD5qz62AEypnNnuV2FsrZiQNkL/77uuBYrpruhNM6LZ\r\n"
	  "PfKWNCC8dOw7ABcbMrATGnaDenoSr0mrQWR4S7UeNeJUyB3as4iaTkc9inOHeUvr\r\n"
	  "3tck7qz96YII5gZzeo/40sA0AegT+pidzQ0xAe9llNHznJU/vqA5lV0gYpr6jCOh\r\n"
	  "46qWO/r4GEmwgKGDyakrifTOlO9DBM5A57FuWdFsnBX5dSgBuQrfaMhwVkeYN7jE\r\n"
	  "kGP9B6WeE53tFZKihq7fAgGKg8wOHKSlEKM42nI2V2+0XOqHySHgZbuS8gnhjG9O\r\n"
	  "Nc90XqYNWZUMDaUsSGeOvJzrpAM29kk9Vy2TdbWd3IvWsDMDtQRQcQfruAGiJCf9\r\n"
	  "mGH0HIKmGfHqMnIQZp+H/HOmNpEHPkEIVj5JT0XzHz/QXzuitsuV1ApGIu/lV7Ht\r\n"
	  "gdJzmTbrijjrinZE4kPsqNJQcQbuSw==\r\n"
	  "=/f+w\r\n"
	  "-----END PGP MESSAGE-----\r\n"
	  "\r\n"
	  "--boundary--\r\n",
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




static void
test_ah_injection (void)
{
	unsigned int i;
	
	for (i = 0; i < G_N_ELEMENTS (inject_test_data); i++) {
		GMimeMultipartEncrypted *encrypted = NULL;
		GMimeMessage *before = NULL;
		GMimeStream *stream = NULL;
		GMimeParser *parser = NULL;
		GError *err = NULL;
		char *str = NULL;
		int r, ix;
		
		try {
			const struct _ah_inject_test *test = inject_test_data + i;
			testsuite_check ("Autocrypt injection[%u] (%s)", i, test->name);
			
			stream = g_mime_stream_mem_new_with_buffer (test->before, strlen (test->before));
			parser = g_mime_parser_new_with_stream (stream);
			g_object_unref (stream);
			
			before = g_mime_parser_construct_message (parser, NULL);
			g_object_unref (parser);
			
			if (test->acheader) {
				GMimeAutocryptHeader *ah = _gen_header (test->acheader);
				
			        g_mime_object_set_header (GMIME_OBJECT (before), "Autocrypt",
						         g_mime_autocrypt_header_to_string (ah, FALSE), NULL);
				g_object_unref (ah);
			}
			
			if (test->encrypt_to) {
				/* get_mime_part is "transfer none" so mainpart does not need to be cleaned up */
				GMimeObject *mainpart = g_mime_message_get_mime_part (before);
				
				if (!mainpart)
					throw (exception_new ("failed to find main part!\n"));
				
				if (test->gossipheaders) {
					GMimeAutocryptHeaderList *ahl = _gen_header_list (test->gossipheaders);
					
					for (ix = 0; ix < g_mime_autocrypt_header_list_get_count (ahl); ix++) {
						g_mime_object_append_header (mainpart, "Autocrypt-Gossip",
									     g_mime_autocrypt_header_to_string (g_mime_autocrypt_header_list_get_header_at (ahl, ix), TRUE), NULL);
					}
					
					g_object_unref (ahl);
				}
				
				GMimeCryptoContext *ctx = g_mime_gpg_context_new ();
				GPtrArray *recip = g_ptr_array_new ();
				
				for (r = 0; test->encrypt_to[r]; r++)
					g_ptr_array_add (recip, (gpointer) test->encrypt_to[r]);
				
				encrypted = g_mime_multipart_encrypted_encrypt (ctx, mainpart, FALSE, NULL,
										GMIME_ENCRYPT_ALWAYS_TRUST,
										recip, &err);
				g_ptr_array_free (recip, TRUE);
				g_object_unref (ctx);
				
				if (!encrypted)
					throw (exception_new ("failed to encrypt: %s", err->message));
				
				g_mime_message_set_mime_part (before, GMIME_OBJECT (encrypted));
				g_object_unref (encrypted);
			}
			
			if (test->after) {
				stream = g_mime_stream_mem_new ();
				g_mime_object_write_to_stream (GMIME_OBJECT (before), NULL, stream);
				
				GByteArray *bytes = g_mime_stream_mem_get_byte_array (GMIME_STREAM_MEM (stream));
				char *got = (char *) bytes->data;
				
				if (memcmp (got, test->after, strlen (test->after)) != 0) {
					fprintf (stderr, "Expected: %s\nGot: %s\n", test->after, got);
					g_object_unref (stream);
					
					throw (exception_new ("failed to match"));
				} else {
					g_object_unref (stream);
				}
			}
			
			if (test->inner_after) {
				if (!encrypted)
					throw (exception_new ("inner_after, but no encrypted part!\n"));
				
				GMimeObject *cleartext = g_mime_multipart_encrypted_decrypt (encrypted,
											     GMIME_DECRYPT_NONE,
											     NULL, NULL, &err);
				if (!cleartext)
					throw (exception_new ("decryption failed: %s!\n", err->message));
				
				stream = g_mime_stream_mem_new ();
				g_mime_object_write_to_stream (cleartext, NULL, stream);
				g_object_unref (cleartext);
				
				GByteArray *bytes = g_mime_stream_mem_get_byte_array (GMIME_STREAM_MEM (stream));
				char *got = (char*) bytes->data;
				
				if (memcmp (got, test->inner_after, strlen (test->inner_after)) != 0) {
					fprintf (stderr, "Expected: %s\nGot: %s\n", test->inner_after, got);
					g_object_unref (stream);
					
					throw (exception_new ("failed to match"));
				} else {
					g_object_unref (stream);
				}
			}
			testsuite_check_passed ();
		} catch (ex) {
			testsuite_check_failed ("autocrypt header injection failed: %s", ex->message);
		} finally;
		
		if (before)
			g_object_unref (before);
		if (err)
			g_error_free (err);
		g_free (str);
		str = NULL;
	}
}


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
			char *e = g_mime_autocrypt_header_to_string (ahe, FALSE);
			char *g = g_mime_autocrypt_header_to_string (ahg, FALSE);
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
						      g_mime_autocrypt_header_to_string (ah_got, FALSE)));
			if (ah_expected)
				if (g_mime_autocrypt_header_compare (ah_expected, ah_got))
					throw (exception_new ("Autocrypt header did not match"));

			/* check on gossip */
			if (test->gossipheaders)
				gossip_expected = _gen_header_list (test->gossipheaders);
			if (innerpart) {
				gossip_got = g_mime_message_get_autocrypt_gossip_headers_from_inner_part (message, NULL, innerpart);
			} else {
				GMimeObject *obj = g_mime_message_get_mime_part (message);
				if (GMIME_IS_MULTIPART_ENCRYPTED (obj)) {
#ifdef ENABLE_CRYPTO
					GError *err = NULL;
					gossip_got = g_mime_message_get_autocrypt_gossip_headers (message, NULL, GMIME_DECRYPT_NONE, NULL, &err);
					if (!gossip_got && err) {
						fprintf (stderr, "%s", test->msg);
						Exception *ex;
						ex = exception_new ("%s", err->message);
						g_error_free (err);
						throw (ex);
					}
#else
					/* pretend that we do not expect any gossip so that we don't fail the test
					   simply because we don't have crypto support. */
					gossip_expected = FALSE;
#endif
				}
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


#ifdef ENABLE_CRYPTO
static void
import_secret_key (void)
{
	/* generated key 0x0D211DC5D9F4567271AC0582D8DECFBFC9346CD4 with GnuPG via:
	 *
	 * export GNUPGHOME=$(mktemp -d)
	 * gpg --pinentry-mode loopback --passphrase '' --batch --quick-gen-key $(uuidgen)@autocrypt.org
	 * gpg --command-fd 3 --edit-key autocrypt.org expire  3<<<0
	 * gpg --command-fd 3 --edit-key autocrypt.org clean
	 * gpg --armor --export-secret-keys autocrypt.org | sed -e 's/^/\t\t"/' -e 's/$/\\n"/'
	 */
	const char secret_key[] =
		"-----BEGIN PGP PRIVATE KEY BLOCK-----\n"
		"\n"
		"lQVYBFoDAZIBDAC6ZV+A42SHxfH5W4e6QM2PlVwrF6cRDbxCg0SFUNgsNtQknqcB\n"
		"iwdujOnc0rgqW2BMek+mfZaACSA4l5rEiDfowQFV7aogkySDcH9/2gDJ+0b/j8z0\n"
		"q0cM3nDzYggsrHdgGE3KkIW2+tf8eaB5Mjxt1VekT4AkS/IvES6Qoo22G39XG51V\n"
		"HS4wproDgiwxpVX/L9m+IXDJmpD90UQlIJ1kpWX3Tt9eiX+vFeo3ohxyVy8ICn9n\n"
		"K5Ve42e93dfXfcLQg3EKOdxI/jop7J+IOOEyHmQXZ3QrVeiIRYsUpc+Clk6TM5Nw\n"
		"JpwtNy1DFUt+bCqx90X30L1IlboK2eNte8n+x6DyhopBpQwjHAvV4qX87X2NYETl\n"
		"1iua051J6chXFZcP1b/Re0CtiKmv26d1HOsPF2ocAf2Ssx7e7aeXP0vF2r/btY02\n"
		"Cndrj0gy6xMbQ+EymE4cs+1h/anesuYE38XCLJdTjzbnBuG3sCHZ7v83Xn0d7XTy\n"
		"mgZVaxbaYJlIGq8AEQEAAQAL/0xL9n3BloLlCZkyWCprIDlnv+R7uA0I/EiVhtSz\n"
		"NOlUQB4FOwMsr4wW7htPvcbIxHBJmJTjz1j1Y1UG6XkM8SW66xsLP5o54LZUtDvX\n"
		"Nn929abia9iyy1B/NOjK9eGjbvHMwPrrkXBG2WYlOwShBY9HxqohSKiS1b1iYRcf\n"
		"Era6JrO3P/15BlEvzfBltkVUEhF0usJS2eIL/NGIeUZhRUvPUB+dD12ZFsTKSacg\n"
		"GljLSxsVgPTwKCJBH1PenN0+Qm6FqUUJzhhwqHvU6Qf8qZIr3cKq9XCAyRlFCZR4\n"
		"42WXMMA9b0R4ZhE5l2iNN0B8lyhr1UpRrb8//8E80nDVPiWaT3c9vxACDNS24+Sc\n"
		"1KIXX1Owl7+V15kJKefL5Lh6nCfQFz5iVI1m/8W40wgRO6rak/WiJdfOFXlWdzGN\n"
		"1PVY6U7Rk96FygCEPkTCy3NgUhMikkfC/+jEpz75M2k7hYX6vft4u3zRBKufEWpx\n"
		"PFMbp4i3DckEyURbhDisQFDsoQYAxovBi/jl3LMJe5efkV0W/kUCZJl5AGgWzgET\n"
		"1EJo91dW6YCUoikIxByBeyzQ3c/uu1G6ly5e9eSiqXdixtQoOFDsITqCAcxrbquQ\n"
		"0RIrcyAVOMO2v5DqPBRLqxzg5pJfzerx8jyO3xOCrCNdWP25HFpYM3rvdrHXh3TO\n"
		"wGvqmLUPO+jKEiAspRM9U74RLmfIRc+6/V9OwnmaCBM4nCCS0FLo40E/KFne6kZn\n"
		"xnqlK2o92IYrD/SrEVI4yZQAhj5HBgDwVY1hQH5jmjdgH6CVS0Zxtk1iyhYpDnDi\n"
		"iZstDLuCk0k0G9u5vlbCS8+zBanPi07xwE5DRVLhtI4twHJb4Qh84gka/tOc12rW\n"
		"QyooBWRjyHhGt70x1yzh6vqAuQy629hR48ogbXI70xAuS4LrlW9nd0iu2UhNjYd5\n"
		"3ZPhoz1qxuwA0w3Xf47yjTs+v4dG932vVvFFL1QrxfJL/4X+FescJGwhzBQhROIB\n"
		"sSL6I+qLERlYAbVWruCvXzGVf9e17FkGAN5AWq9OcFsMpOmUmngSjydIpphKN/u3\n"
		"OuReO4M4f52HShxFPi0aA+IieJGEA0wNaDHsQG8G2W7anKwYETg0l9FpoHNijS94\n"
		"GUCCaIuDtRI8y26d4OnFxrL3A5Nnez/i4uzI5PKoiHw8n4CthEQB7Ucqsq1BbkEC\n"
		"wFQhTK9FhGqhnrEPxDJsEhyPoNPIa9lqqXi3rq24w+AXnSmAqByTrIWj/H6s4DJq\n"
		"c1qIc9F9i/aJIyq1+fTrBPMRgC82GFB/NdpYtDI0YmMzODg4ZS0wNTZiLTRkYTkt\n"
		"YmQxMi0wMmE4ZWRiZTIzYmJAYXV0b2NyeXB0Lm9yZ4kBzgQTAQoAOAIbAwULCQgH\n"
		"AgYVCgkICwIEFgIDAQIeAQIXgBYhBA0hHcXZ9FZycawFgtjez7/JNGzUBQJaAwG3\n"
		"AAoJENjez7/JNGzUxaAMAJMFg7xwU2fAI4kF21edZiT5gah1cbsSTmAQz5PMz2BL\n"
		"6iufDkdhBseMjMc4ZFCgfBRH/n0ZJPqSgKHieaxBLUlyQITuyrLVV0UslPMe5PLu\n"
		"x2FMMoxDQoIuPbb0yMDIs9kxiPViAgOQwRhsud1K1u0S3u/isix6SdYor2sEfbr/\n"
		"JReZ5LFyA2PZebKpYRSMBOASeneYhQ8q4AZZKUQgxMQSQTHP/0ABVg/80o6NDqGp\n"
		"Ll5pFGCQQlwmHMZhXZ5DWMqbqwEdB1LjvmNhAfl1Pw6Q8V3lPz1gjB+FbG/sj74G\n"
		"HgI+Yn0R57DxR0JWS68lHcCHXn+d6GkBrVLKSRc9QA8GAFjntxQM8y5fUETYJ9bs\n"
		"EFzrPOkxUM0tTgtN4gTspTMdGv89Cd3MPU9recRzcMlqAXH/R0P6Oz8+l91sI/kN\n"
		"K8rFbqiGTPZB0uUiB397YkFDnZNT5mvnxMSVP4QwtM5wb1PPqgxQI5oDUepa3r56\n"
		"g5vq6SecDkEFP0Cm0qbbsp0FWARaAwGSAQwAoa7hUcD18PsB4QbvzUGBL9uwcdeD\n"
		"2m9yEY/ZNQscAYlipAYrmKBhIEIy7DLgDncM+IQ47Gf0tcIFRxT0bQxgUEAHlgRN\n"
		"D6aPCoswnX+IsPy9M5ZHh4LldkMldmVgs/iAtJ8+esi6V39073FhL191coBxuBFB\n"
		"fMo0iW+HmMBnX1jhTffSRUntdQTRMEGYGsmPkcFBgL6UFLePP2bwNOs9v0gdgnEK\n"
		"9u7l+y5cLc0HnbN6sEKCjT+HWQBFeBS4Nhff+pcw6ToFm+2LrxUpgt+URePC0wnr\n"
		"WWmsCPEeNs4SreYvn6zglDoqhfBSg7f+8DXY2rL2M+KKFPIS11t+e2Irzy0Xj7Iv\n"
		"V5NvCv/DrC4oBFzreL0jP4u4+z2GoadpUiqwPMq99TLY3v87KD3Zds5+W+jEzs1r\n"
		"py+UEZdQI1n46oFrvzh97/ASkxmuhXu0A/As1T9nxY6V7+Y1SuGSpDnhrWSHq04O\n"
		"bsqZoFa8sIciY5tNAP+NpACKcMuQUkfEwNWjABEBAAEAC/sFbXFy5R9cb5ColSsH\n"
		"oONNT/qkV6+9bXBO1p1cAnt2Mb518x8TiI66Hn7HHw4WbjipPwcKKJM2ZsT2leV+\n"
		"o6O4De4zQwGzPMwgdnuzTcyw5EsVqD3Odr5tMePYDZ8pa3YmmhHm3UYkGcs7Hns8\n"
		"s9+lcFpg63NfVQpecrgCkLLnqHwnueH9IXYvL3I2RH1uqMWBBxPD1qHx8BeG8VWu\n"
		"8RapjSowbssHbw2ZWP0PLIrM6HF96T2osDFC85dhaJCmgDae7IO06K/akf1qNUMR\n"
		"kFsXBBnduADzNNxiZofeThejDurnBsOPKH/wxiKEV+vDeyXGghyqy7WuWhBFAjZX\n"
		"xNQD+Un9pTc4V6EIX4RZex7gfu7ylXIiDZr+yZO3L2OsbnvUSyGYOJnEQNvl/bPN\n"
		"OBS10U/BW1wgkJmDwOXkleH3lSVHC7p7A5nxlUrT0mvrOu04cl6Tc5/hO2+/N9Oy\n"
		"9z5TrVECgv7HGG5tA0jWS2LeXyvOtUZuUtcdMirQUq+W8gEGAMA7PuGIreU09Dl5\n"
		"9Agg09+rHvLHRjuDQtyv9M5tnzOS01FWDCMBNnqj4hwJCzwZ1/s6y/ng6K2KEWmi\n"
		"XHR07gFBBRyNiQ6sAtZj5Ve5xdG4dW/t49ZabO++eJrLOfU7RFNhbyFm6aZcebva\n"
		"adGhDbisG4exQIB+ggiNOxP2iSTs1jgVWfuAtu4p9SfXXyESRer4VDpUN/N5lwIO\n"
		"jpWCsPNT3N4fkUxNNjiHV3anpY7jEr0fNx+cGPswfIGr+NgUgwYA11FmOmGpQTZE\n"
		"HrhiTLtq1CmkmFzdh1j103qdNWn/6UWiXQoxxmTPlyiufM+Fdd18fbyuxoegO67q\n"
		"Q4sg3ZkE7PSv5bzAome6ZtBheNJdH1kfSIfzInlSDNN57G7vndkMosiBoyJHF7+Y\n"
		"mOLDFkvvfF501huHyQz9DHt2KuBYy0TkKtw2uCYrkIuahCEagx4VEWxtKihKzX4x\n"
		"DyhR7cqWyXdz5dmgRBg1KE8uHmKcRvvuLWiVZnSRGyiTD0U7bbBhBgC19HjukV37\n"
		"shzXIjuLVwMeuLdx2NS4PyHjwroFcORO4wzZCUIUqv1IgXb0kWra0lmxEnSCkngq\n"
		"YpYpUFDjXM+mKYvumuPZzAT/3P3+aKMq4QpCYKqVdLHCNWpFJpGzyH+wvvBK4YQe\n"
		"PSVKZi1yu4aRtg7JHjGVIdesP3PdE/EMvglWaFCJjdz2ehVH0f7JYCSMcxcwvoTE\n"
		"PQK2z66Y2xFjbk3s+rJrg1txsNLryreGJMWk0OO+uEbcM085rXSUUi7ekokBtgQY\n"
		"AQoAIBYhBA0hHcXZ9FZycawFgtjez7/JNGzUBQJaAwGSAhsMAAoJENjez7/JNGzU\n"
		"KFMMAJLojGZv1C5nj6UC1tOq993wUVtq09gevHCl9/wzFw/bf85TjGOqo2hC7jb7\n"
		"KrzyhJSb3rxMWs8kfbyfZdcLPI3qtq3S6WcdCPLzaJsa+YcnAnE7dvpXavjF9cHz\n"
		"EAIGkxhBGB4xZOOLQecx60tUDQE64AhoDsIsi+ofMZFJgePTBrlLhHxChZqb8S6d\n"
		"SYYvs2k2r1gdhansk2o93G8nYksCe0ukZ7tqSywtmgce/ruPDRz+PI1OpS7SNX7D\n"
		"05YotAuuJD6D5yQaxpaqD5FxXQzPcvUU20mscQwS9MtjgOfyy3EauAS5BS/peB2O\n"
		"Gvg8DPfF5P2+/Eez1lEPNLYcbjzFOAItEUYrgdgpB5vg2VCRkLznZdWFXhh2KmvU\n"
		"NOiQoDWThtanLPNFXe3vxr+g0lSgMkJaT2yo+TciqZOvPRUs+TzDwWRMzVQe77PZ\n"
		"aJ/L2xxnGXtLsZ2V9rf/4VaXeky2HYl5UmwM5kr/3jiN5MdNtVh2sfIwOvzLRKaF\n"
		"7MLMmw==\n"
		"=HQjA\n"
		"-----END PGP PRIVATE KEY BLOCK-----\n"
		;
	GMimeCryptoContext *ctx = g_mime_gpg_context_new ();
	GMimeStream *stream = g_mime_stream_mem_new_with_buffer (secret_key, sizeof(secret_key));
	GError *err = NULL;

	testsuite_check ("Importing secret key");
	
	g_mime_crypto_context_import_keys (ctx, stream, &err);
	g_object_unref (stream);
	g_object_unref (ctx);

	if (err != NULL) {
		Exception *ex;
		ex = exception_new ("%s", err->message);
		g_error_free (err);
		throw (ex);
	}
	testsuite_check_passed ();
}
#endif

int main (int argc, char **argv)
{
	char *gpg;
	
	g_mime_init ();
	
	testsuite_init (argc, argv);
	
	if (!(gpg = g_find_program_in_path ("gpg2")))
		if (!(gpg = g_find_program_in_path ("gpg")))
			return EXIT_FAILURE;
	
	if (testsuite_setup_gpghome (gpg) != 0)
		return EXIT_FAILURE;
	
	testsuite_start ("Autocrypt: generate headers");
	test_ah_generation ();
	testsuite_end ();
	
#ifdef ENABLE_CRYPTO
	testsuite_start ("Autocrypt: import OpenPGP secret key");
	import_secret_key ();
	testsuite_end ();
#endif
	
	testsuite_start ("Autocrypt: parse messages");
	test_ah_message_parse ();
	testsuite_end ();
	
#ifdef ENABLE_CRYPTO
	testsuite_start ("Autocrypt: inject headers");
	test_ah_injection ();
	testsuite_end ();
#endif
	
	g_mime_shutdown ();
	
#ifdef ENABLE_CRYPTO
	if (testsuite_destroy_gpghome () != 0)
		return EXIT_FAILURE;
#endif
	
	return testsuite_exit ();
}
