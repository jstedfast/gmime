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
_gen_header_list (const struct _ah_gen_test *tests)
{
	GMimeAutocryptHeaderList *ret = g_mime_autocrypt_header_list_new ();
	for (; tests->addr; tests++) {
		GMimeAutocryptHeader *ah = _gen_header (tests);
		if (!ah) {
			fprintf (stderr, "failed to generate header <%s>", tests->addr);
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

const static struct _ah_gen_test no_addrs[] = {
	{ .addr = NULL }, /* sentinel */
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
	const struct _ah_gen_test *acheaders;
	const struct _ah_gen_test *gossipheaders;
	const char *msg;
};

const static struct _ah_gen_test alice_addr[] = {
	{ .addr = "alice@example.org",
	  .keydatacount = 102,
	  .timestamp = 1508774054,
	  .keybyte = '\013' },
	{ .addr = NULL }, /* sentinel */
};

const static struct _ah_gen_test alice_incomplete[] = {
	{ .addr = "alice@example.org",
	  .timestamp = 1508774054,
	},
	{ .addr = NULL }, /* sentinel */
};

const static struct _ah_gen_test bob_addr[] = {
	{ .addr = "bob@example.org",
	  .keydatacount = 99,
	  .timestamp = 1508774054,
	  .keybyte = '\133' },
	{ .addr = NULL }, /* sentinel */
};

const static struct _ah_parse_test parse_test_data[] = {
	{ .name = "simple",
	  .acheaders = alice_addr,
	  .gossipheaders = no_addrs,
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
	
	{ .name = "simple+gossip",
	  .acheaders = alice_addr,
	  .gossipheaders = bob_addr,
	  .msg = "From: alice@example.org\r\n"
	  "To: bob@example.org, carol@example.org\r\n"
	  "Subject: A gossipy lovely day\r\n"
	  "Message-Id: <lovely-gossip-day@example.net>\r\n"
	  "Date: Mon, 23 Oct 2017 11:54:14 -0400\r\n"
	  "Autocrypt: addr=alice@example.org; keydata=CwsLCwsLCwsLCwsLCwsLCwsLCwsL\r\n"
	  " CwsLCwsLCwsLCwsLCwsLCwsLCwsLCwsLCwsLCwsLCwsLCwsLCwsLCwsLCwsLCwsLCwsL\r\n"
	  " CwsLCwsLCwsLCwsLCwsLCwsLCwsLCwsLCwsLCwsL\r\n"
	  "Autocrypt-Gossip: addr=bob@example.org; keydata=W1tbW1tbW1tbW1tbW1tbW1tbW1tbW1tb\r\n"
	  " W1tbW1tbW1tbW1tbW1tbW1tbW1tbW1tbW1tbW1tbW1tbW1tbW1tbW1tb\r\n"
	  " W1tbW1tbW1tbW1tbW1tbW1tbW1tbW1tbW1tbW1tbW1tb\r\n"
	  "Mime-Version: 1.0\r\n"
	  "Content-Type: text/plain\r\n"
	  "\r\n"
	  "Isn't a lovely day?  Now Carol can encrypt to Bob, hopefully.\r\n",
	},
	
	{ .name = "simple+badgossip",
	  .acheaders = alice_addr,
	  .gossipheaders = no_addrs,
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
	  .acheaders = alice_incomplete,
	  .gossipheaders = no_addrs,
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
	  .acheaders = alice_incomplete,
	  .gossipheaders = no_addrs,
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
	  .acheaders = alice_addr,
	  .gossipheaders = no_addrs,
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
	  .acheaders = alice_addr,
	  .gossipheaders = no_addrs,
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
		GMimeAutocryptHeaderList *ahl_expected = NULL;
		GMimeAutocryptHeaderList *ahl_got = NULL;
		GMimeAutocryptHeaderList *gossip_expected = NULL;
		GMimeAutocryptHeaderList *gossip_got = NULL;
		GMimeMessage *message = NULL;
		const struct _ah_parse_test *test = parse_test_data + i;
		try {
			testsuite_check ("Autocrypt message[%u] (%s)", i, test->name);

			ahl_expected = _gen_header_list (test->acheaders);
			gossip_expected = _gen_header_list (test->gossipheaders);

			/* make GMimeMessage from test->msg */
			GMimeStream *stream = g_mime_stream_mem_new_with_buffer (test->msg, strlen(test->msg));
			GMimeParser *parser = g_mime_parser_new_with_stream (stream);
			message = g_mime_parser_construct_message (parser, NULL);
			g_object_unref (parser);
			g_object_unref (stream);

			ahl_got = g_mime_message_get_autocrypt_headers (message, NULL);
			if (!ahl_got)
				throw (exception_new ("failed to extract headers from message!"));
			gossip_got = g_mime_message_get_autocrypt_gossip_headers (message, NULL);
			if (!gossip_got)
				throw (exception_new ("failed to extract gossip headers from message!"));
			gchar *err = NULL;
			err = _acheaderlists_compare (ahl_expected, ahl_got);
			if (err)
				throw (exception_new ("sender headers: %s", err));
			err = _acheaderlists_compare (gossip_expected, gossip_got);
			if (err)
				throw (exception_new ("gossip headers: %s", err));
			testsuite_check_passed ();
		} catch (ex) {
			testsuite_check_failed ("autocrypt message parse[%u] (%s) failed: %s", i, test->name, ex->message);
		} finally;
		if (ahl_expected)
			g_object_unref (ahl_expected);
		if (ahl_got)
			g_object_unref (ahl_got);
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
