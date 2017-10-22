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
	try {
		if (!(ah = g_mime_autocrypt_header_new()))
			throw (exception_new ("failed to make a new header"));
		if (t->keydatacount)
			if (!(keydata = g_byte_array_new_take (g_strnfill (t->keydatacount, t->keybyte), t->keydatacount)))
				throw (exception_new ("failed to make a new keydata"));
		g_mime_autocrypt_header_set_address_from_string (ah, t->addr);
		g_mime_autocrypt_header_set_keydata (ah, keydata);
		if (t->timestamp) {
			GDateTime *ts = g_date_time_new_from_unix_utc (t->timestamp);
			g_mime_autocrypt_header_set_effective_date (ah, ts);
			g_date_time_unref (ts);
		}
	} catch (ex) {
		if (keydata)
			g_byte_array_unref (keydata);
		if (ah)
			g_object_unref (ah);
		throw (ex);
	} finally;
	return ah;
}
	
const static struct _ah_gen_test gen_test_data[] = {
	{ .addr = "test@example.org",
	  .keydatacount = 102,
	  .keybyte = '\013',
	  .txt = "addr=test@example.org; type=1; keydata=CwsLCwsLCwsLCwsLCwsL\r\n"
	  " CwsLCwsLCwsLCwsLCwsLCwsLCwsLCwsLCwsLCwsLCwsLCwsLCwsLCwsLCwsLCwsLCwsLCwsL\r\n"
	  " CwsLCwsLCwsLCwsLCwsLCwsLCwsLCwsLCwsLCwsLCwsL",
	}
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

int main (int argc, char **argv)
{
	g_mime_init ();
	
	testsuite_init (argc, argv);
	
	testsuite_start ("Autocrypt: generate headers");
	test_ah_generation ();
	testsuite_end ();
	
	g_mime_shutdown ();
	
	return testsuite_exit ();
}
