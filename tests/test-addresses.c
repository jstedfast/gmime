/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*  GMime
 *  Copyright (C) 2026 Jeffrey Stedfast
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public License
 *  as published by the Free Software Foundation; either version 2.1
 *  of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library; if not, write to the Free
 *  Software Foundation, 51 Franklin Street, Fifth Floor, Boston, MA
 *  02110-1301, USA.
 */

#include <string.h>
#include <gmime/gmime.h>

#include "testsuite.h"

static void
check (gboolean condition, const char *description)
{
	testsuite_check ("%s", description);
	if (condition)
		testsuite_check_passed ();
	else
		testsuite_check_failed ("%s", description);
}

static void
check_string (char *actual, const char *expected, const char *description)
{
	testsuite_check ("%s", description);
	if (!g_strcmp0 (actual, expected))
		testsuite_check_passed ();
	else
		testsuite_check_failed ("expected [%s], got [%s]", expected, actual ? actual : "(null)");
	g_free (actual);
}

static void
test_domains (void)
{
	GMimeDomainList *list = g_mime_domain_list_new (), *clone, *parsed;
	GMimeFormatOptions *options = g_mime_format_options_new (), *format;
	GError *error = NULL;
	const char bounded[] = { '@', 'a', '.', 'b', '!' };
	const char nul[] = { '@', 'a', '\0', 'b' };

	g_mime_domain_list_add (list, "First.EXAMPLE");
	g_mime_domain_list_add (list, "");
	g_mime_domain_list_add (list, " \t\r\n");
	g_mime_domain_list_add (list, "m\303\274nich.example");
	g_mime_domain_list_add (list, "[IPv6:::1]");
	check (g_mime_domain_list_length (list) == 5, "domain collection preserves empty entries");
	check (g_mime_domain_list_contains (list, "First.EXAMPLE") &&
	       !g_mime_domain_list_contains (list, "first.example"), "domain lookup is case-sensitive");
	check_string (g_mime_domain_list_to_string (list, NULL, FALSE),
		      "@First.EXAMPLE,@m\303\274nich.example,@[IPv6:::1]", "display routes omit linear whitespace");
	check_string (g_mime_domain_list_to_string (list, NULL, TRUE),
		      "@First.EXAMPLE,@xn--mnich-kva.example,@[IPv6:::1]", "transport routes encode only domain names");
	g_mime_format_options_set_allow_international (options, TRUE);
	format = g_mime_format_options_clone (options);
	check (g_mime_format_options_get_allow_international (format), "international formatting option is cloned");
	check_string (g_mime_domain_list_to_string (list, format, TRUE),
		      "@First.EXAMPLE,@m\303\274nich.example,@[IPv6:::1]", "international routes preserve Unicode");
	g_mime_format_options_free (format);
	g_mime_format_options_free (options);
	clone = g_mime_domain_list_clone (list);
	g_mime_domain_list_set_domain (clone, 0, g_mime_domain_list_get_domain (clone, 0) + 6);
	check (!strcmp (g_mime_domain_list_get_domain (clone, 0), "EXAMPLE") &&
	       !strcmp (g_mime_domain_list_get_domain (list, 0), "First.EXAMPLE"), "domain clones and aliased setters are independent");
	g_mime_domain_list_insert (clone, 1, "inserted");
	check (g_mime_domain_list_remove (clone, "inserted"), "remove domains by value");
	check (!g_mime_domain_list_remove_at (clone, 50), "out-of-range domain removal");
	g_mime_domain_list_clear (clone);
	check_string (g_mime_domain_list_to_string (clone, NULL, TRUE), "", "empty source-route string");
	g_mime_domain_list_add (clone, "\342\200\213");
	check_string (g_mime_domain_list_to_string (clone, NULL, TRUE), "@\342\200\213",
		      "failed IDNA mappings cannot silently erase domains");
	g_object_unref (clone);
	g_object_unref (list);

	parsed = g_mime_domain_list_parse ("(route)\r\n @one.example,,, \t(next) @xn--mnich-kva.example\r\n", -1, &error);
	check (parsed != NULL && error == NULL, "parse source-route domains and empty comma entries");
	if (parsed) {
		check_string (g_mime_domain_list_to_string (parsed, NULL, FALSE),
			      "@one.example,@xn--mnich-kva.example", "parsed routes preserve IDN domain spelling");
		g_object_unref (parsed);
	}
	parsed = g_mime_domain_list_parse (bounded, 4, &error);
	check (parsed != NULL && error == NULL, "domain parser honors non-NUL-terminated input length");
	g_clear_object (&parsed);
	parsed = g_mime_domain_list_parse (nul, sizeof (nul), &error);
	check (parsed == NULL && error && error->code == GMIME_ERROR_PARSE_ERROR, "domain parser rejects embedded NUL");
	g_clear_error (&error);
	parsed = g_mime_domain_list_parse ("@one.example:", -1, &error);
	check (parsed == NULL && error != NULL, "domain parser rejects trailing mailbox colon");
	g_clear_error (&error);
	parsed = g_mime_domain_list_parse ("@\342\200\210", -1, &error);
	check (parsed && g_mime_domain_list_length (parsed) == 1 && error == NULL &&
	       !strcmp (g_mime_domain_list_get_domain (parsed, 0), "\342\200\210"),
	       "parsed routes retain Unicode whitespace-only domains");
	g_clear_object (&parsed);
}

static void
test_domain_whitespace (void)
{
	static const gunichar whitespace[] = {
		'\t', '\n', '\v', '\f', '\r', ' ', 0x85, 0xa0, 0x1680,
		0x2000, 0x2001, 0x2002, 0x2003, 0x2004, 0x2005, 0x2006,
		0x2007, 0x2008, 0x2009, 0x200a, 0x2028, 0x2029, 0x202f, 0x205f, 0x3000
	};
	GMimeFormatOptions *options = g_mime_format_options_new ();
	GMimeDomainList *list = g_mime_domain_list_new ();
	guint i, padded, format;

	g_mime_domain_list_add (list, "first.example");
	g_mime_domain_list_add (list, "");
	g_mime_domain_list_add (list, "last.example");
	for (i = 0; i < G_N_ELEMENTS (whitespace); i++) {
		GMimeInternetAddress *address;
		GMimeDomainList *parsed;
		GError *error = NULL;
		char buffer[7] = { 0 };
		char *text;
		gboolean lwsp = whitespace[i] == ' ' || whitespace[i] == '\t' ||
			whitespace[i] == '\r' || whitespace[i] == '\n';
		gboolean valid = !lwsp && whitespace[i] != '\v' && whitespace[i] != '\f';

		g_unichar_to_utf8 (whitespace[i], buffer);
		for (padded = 0; padded < 2; padded++) {
			char *domain = padded ? g_strconcat (" \t", buffer, "\r\n", NULL) : g_strdup (buffer);
			char *expected = lwsp ? g_strdup ("@first.example,@last.example") :
				g_strconcat ("@first.example,@", domain, ",@last.example", NULL);

			g_mime_domain_list_set_domain (list, 1, domain);
			for (format = 0; format < 3; format++) {
				char *actual;

				g_mime_format_options_set_allow_international (options, format == 2);
				actual = g_mime_domain_list_to_string (list, options, format != 0);
				testsuite_check ("Source-route whitespace U+%04X [%u/%u]", whitespace[i], padded, format);
				if (!strcmp (actual, expected) &&
				    !strcmp (g_mime_domain_list_get_domain (list, 1), domain))
					testsuite_check_passed ();
				else
					testsuite_check_failed ("unexpected source route or stored-domain mutation");
				g_free (actual);
			}
			g_free (expected);
			g_free (domain);
		}

		text = g_strconcat ("@", buffer, NULL);
		parsed = g_mime_domain_list_parse (text, -1, &error);
		testsuite_check ("Source-route parser whitespace U+%04X", whitespace[i]);
		if (valid ? parsed && error == NULL && g_mime_domain_list_length (parsed) == 1 &&
		    !strcmp (g_mime_domain_list_get_domain (parsed, 0), buffer) :
		    parsed == NULL && g_error_matches (error, GMIME_ERROR, GMIME_ERROR_PARSE_ERROR))
			testsuite_check_passed ();
		else
			testsuite_check_failed ("unexpected parsed domain or error");
		if (parsed) {
			check_string (g_mime_domain_list_to_string (parsed, NULL, FALSE), text,
				      "parsed Unicode-only domains survive display formatting");
			check_string (g_mime_domain_list_to_string (parsed, NULL, TRUE), text,
				      "parsed Unicode-only domains survive transport formatting");
		}
		g_clear_object (&parsed);
		g_clear_error (&error);
		g_free (text);

		text = g_strconcat ("<@first.example,@", buffer, ",@last.example:user@example.com>", NULL);
		address = g_mime_mailbox_address_parse (NULL, text, -1, &error);
		parsed = address ? g_mime_mailbox_address_get_route (GMIME_MAILBOX_ADDRESS (address)) : NULL;
		testsuite_check ("Mailbox source-route parser whitespace U+%04X", whitespace[i]);
		if (valid ? parsed && error == NULL && g_mime_domain_list_length (parsed) == 3 &&
		    !strcmp (g_mime_domain_list_get_domain (parsed, 1), buffer) :
		    address == NULL && g_error_matches (error, GMIME_ERROR, GMIME_ERROR_PARSE_ERROR))
			testsuite_check_passed ();
		else
			testsuite_check_failed ("unexpected mailbox route or error");
		if (address) {
			check_string (g_mime_internet_address_to_string (address, NULL, FALSE), text,
				      "mailbox display formatting retains Unicode-only route domains");
			check_string (g_mime_internet_address_to_string (address, NULL, TRUE), text,
				      "mailbox transport formatting retains Unicode-only route domains");
		}
		g_clear_object (&address);
		g_clear_error (&error);
		g_free (text);
	}
	g_mime_domain_list_set_domain (list, 1, " \tKeep.EXAMPLE\r\n");
	for (format = 0; format < 3; format++) {
		g_mime_format_options_set_allow_international (options, format == 2);
		check_string (g_mime_domain_list_to_string (list, options, format != 0),
			      "@first.example,@ \tKeep.EXAMPLE\r\n,@last.example",
			      "formatting does not trim nonblank domains");
	}
	g_object_unref (list);
	g_mime_format_options_free (options);
}

static void
test_mailboxes (void)
{
	static const char *invalid[] = {
		"not an address", "user@", "user@example.com garbage", "user@example.com,other@example.com",
		"Name <user@example.com>", "\"bad\nlocal\"@example.com", "a..b@example.com", "a.@example.com"
	};
	GMimeInternetAddress *ia;
	GMimeMailboxAddress *mailbox;
	GMimeFormatOptions *options = g_mime_format_options_new ();
	GError *error = NULL;
	const char *raw;
	guint i;

	ia = g_mime_mailbox_address_new (" \302\240J\303\266rg ", "\"local@part\" (local) @ xn--mnich-kva.example (domain)", &error);
	check (ia != NULL && error == NULL, "construct a fully consumed mailbox without IDN decoding");
	if (!ia)
		goto done;
	mailbox = GMIME_MAILBOX_ADDRESS (ia);
	check_string (g_mime_mailbox_address_get_local_part (mailbox), "\"local@part\"", "local part uses the parsed at-sign");
	check (!strcmp (g_mime_mailbox_address_get_raw_domain (mailbox), "xn--mnich-kva.example"), "stored mailbox domain");
	check (!g_mime_mailbox_address_is_international (mailbox), "an ASCII A-label is not international");
	check_string (g_mime_internet_address_to_string (ia, NULL, FALSE),
		      "\302\240J\303\266rg <\"local@part\"@xn--mnich-kva.example>", "display mailbox preserves Unicode name padding and A-labels");
	g_mime_format_options_set_allow_international (options, TRUE);
	check_string (g_mime_internet_address_to_string (ia, options, TRUE),
		      "\302\240J\303\266rg <\"local@part\"@xn--mnich-kva.example>", "international mailbox transport preserves Unicode name padding and A-labels");
	raw = g_mime_mailbox_address_get_addrspec (mailbox);
	for (i = 0; i < G_N_ELEMENTS (invalid); i++) {
		check (!g_mime_mailbox_address_set_addrspec (mailbox, invalid[i], &error) &&
		       error && error->code == GMIME_ERROR_PARSE_ERROR, "invalid mailbox mutation reports a parse error");
		g_clear_error (&error);
		check (raw == g_mime_mailbox_address_get_addrspec (mailbox), "failed mutation preserves the raw address");
		check_string (g_mime_mailbox_address_get_address (mailbox, TRUE),
			      "\"local@part\"@xn--mnich-kva.example", "failed mutation preserves explicit IDN encoding");
		check (g_mime_mailbox_address_new (NULL, invalid[i], &error) == NULL && error != NULL,
		       "invalid mailbox construction fails explicitly");
		g_clear_error (&error);
	}
	check (g_mime_mailbox_address_set_addrspec (mailbox, "", &error), "empty mailbox address is valid");
	g_mime_domain_list_add (g_mime_mailbox_address_get_route (mailbox), "m\303\274nich.example");
	check (!g_mime_mailbox_address_is_international (mailbox), "empty mailbox is not international even with a route");
	check (g_mime_mailbox_address_set_addrspec (mailbox, "local", &error) &&
	       g_mime_mailbox_address_is_international (mailbox), "international route affects a nonempty mailbox");
	check (!strcmp (g_mime_mailbox_address_get_raw_domain (mailbox), ""), "domainless mailbox has an empty domain");
	g_object_unref (ia);
	check_string (g_mime_mailbox_address_encode_addrspec ("a\\@b@xn--mnich-kva.example"),
		      "a%40b@xn--mnich-kva.example", "IDN helpers use looser escaped-at parsing");
	check_string (g_mime_mailbox_address_decode_addrspec ("a@xn--a"), "a@xn--a",
		      "invalid A-labels cannot introduce control characters");
	check_string (g_mime_mailbox_address_decode_addrspec ("a@EXAMPLE.xn--bcher-kva"),
		      "a@example.b\303\274cher", "decoded IDN domains normalize ASCII label case");
	check_string (g_mime_mailbox_address_decode_addrspec ("a@XN--BCHER-KVA.EXAMPLE"),
		      "a@b\303\274cher.example", "explicit IDN decoding recognizes uppercase A-labels");
	check_string (g_mime_mailbox_address_decode_addrspec ("a..b@xn--mnich-kva.example"),
		      "a..b@m\303\274nich.example", "IDN helpers use looser consecutive-dot parsing");
	check_string (g_mime_mailbox_address_decode_addrspec ("user@[xn--literal]"),
		      "user@[xn--literal]", "IDN decoding preserves address literals");
	check_string (g_mime_mailbox_address_encode_addrspec ("not an address"), "not an address",
		      "best-effort IDN helpers preserve unparseable input");
done:
	g_clear_error (&error);
	g_mime_format_options_free (options);
}

static void
test_idn_accessors (void)
{
	static const struct {
		const char *raw, *encoded, *decoded;
	} cases[] = {
		{ "", "", "" },
		{ "local", "local", "local" },
		{ "\"local@part\"@xn--mnich-kva.example",
		  "\"local@part\"@xn--mnich-kva.example", "\"local@part\"@m\303\274nich.example" },
		{ "user@m\303\274nich.example", "user@xn--mnich-kva.example", "user@m\303\274nich.example" },
		{ "user@EXAMPLE.xn--bcher-kva", "user@EXAMPLE.xn--bcher-kva", "user@example.b\303\274cher" },
		{ "user@XN--BCHER-KVA.EXAMPLE", "user@XN--BCHER-KVA.EXAMPLE", "user@b\303\274cher.example" },
		{ "user@xn--a", "user@xn--a", "user@xn--a" },
		{ "user@[xn--literal]", "user@[xn--literal]", "user@[xn--literal]" },
		{ "user@[IPv6:::1]", "user@[IPv6:::1]", "user@[IPv6:::1]" }
	};
	GMimeInternetAddress *address, *parsed;
	GMimeMailboxAddress *mailbox;
	GMimeFormatOptions *options = g_mime_format_options_new ();
	const char *at, *raw;
	guint i;

	g_mime_format_options_set_allow_international (options, TRUE);
	for (i = 0; i < G_N_ELEMENTS (cases); i++) {
		address = g_mime_mailbox_address_new (NULL, cases[i].raw, NULL);
		check (address != NULL, "construct mailbox preserving its IDN representation");
		if (!address)
			continue;
		mailbox = GMIME_MAILBOX_ADDRESS (address);
		at = strrchr (cases[i].raw, '@');
		check (!strcmp (g_mime_mailbox_address_get_addrspec (mailbox), cases[i].raw) &&
		       !strcmp (g_mime_mailbox_address_get_raw_domain (mailbox), at ? at + 1 : ""),
		       "raw mailbox getters preserve domain spelling");
		raw = g_mime_mailbox_address_get_addrspec (mailbox);
		check_string (g_mime_mailbox_address_get_address (mailbox, TRUE), cases[i].encoded, "explicit IDN address encoding");
		check_string (g_mime_mailbox_address_get_address (mailbox, FALSE), cases[i].decoded, "explicit IDN address decoding");
		at = strrchr (cases[i].encoded, '@');
		check_string (g_mime_mailbox_address_get_domain (mailbox, TRUE), at ? at + 1 : "", "explicit IDN domain encoding");
		at = strrchr (cases[i].decoded, '@');
		check_string (g_mime_mailbox_address_get_domain (mailbox, FALSE), at ? at + 1 : "", "explicit IDN domain decoding");
		check (raw == g_mime_mailbox_address_get_addrspec (mailbox) && !strcmp (raw, cases[i].raw),
		       "IDN accessors do not mutate the mailbox");
		if (at)
			check_string (g_mime_internet_address_to_string (address, NULL, TRUE), cases[i].encoded,
				      "transport formatting explicitly encodes the domain");
		if (*cases[i].raw) {
			parsed = g_mime_mailbox_address_parse (NULL, cases[i].raw, -1, NULL);
			check (parsed && !strcmp (g_mime_mailbox_address_get_addrspec (GMIME_MAILBOX_ADDRESS (parsed)), cases[i].raw),
			       "parsed mailboxes preserve IDN representation");
			g_clear_object (&parsed);
		}
		g_object_unref (address);
	}
	address = g_mime_mailbox_address_new (NULL, "a@xn--mnich-kva.example", NULL);
	parsed = g_mime_mailbox_address_new (NULL, "a@m\303\274nich.example", NULL);
	check (!g_mime_internet_address_equal (address, parsed), "mailbox equality distinguishes A-label and Unicode domains");
	check_string (g_mime_internet_address_to_string (address, options, TRUE), "a@xn--mnich-kva.example",
		      "international transport preserves A-labels");
	check_string (g_mime_internet_address_to_string (parsed, options, TRUE), "a@m\303\274nich.example",
		      "international transport preserves Unicode domains");
	mailbox = GMIME_MAILBOX_ADDRESS (address);
	check (g_mime_mailbox_address_set_addrspec (mailbox, "a@b\303\274cher.example", NULL),
	       "update the stored domain");
	check_string (g_mime_mailbox_address_get_address (mailbox, TRUE), "a@xn--bcher-kva.example",
		      "encoding reads the updated domain");
	check_string (g_mime_internet_address_to_string (address, NULL, TRUE), "a@xn--bcher-kva.example",
		      "transport formatting reads the updated domain");
	check_string (g_mime_mailbox_address_get_address (mailbox, FALSE), "a@b\303\274cher.example",
		      "decoding reads the updated domain");
	g_object_unref (address);
	g_object_unref (parsed);
	g_mime_format_options_free (options);
}

static void
test_values (void)
{
	GMimeInternetAddressList *list, *clone, *flat;
	GMimeInternetAddress *group, *child, *other;
	GMimeDomainList *route;

	list = g_mime_internet_address_list_parse_full (NULL,
		"Team: User <@one.example,@m\303\274nich.example:user@example.com>, Inner: Second <two@example.com>;;, Other <three@example.com>",
		-1, NULL);
	check (list != NULL, "parse nested groups with mailbox routes");
	if (!list)
		return;
	group = g_mime_internet_address_list_get_address (list, 0);
	g_mime_internet_address_set_charset (group, "iso-8859-1");
	clone = g_mime_internet_address_list_clone (list);
	check (g_mime_internet_address_list_equal (list, clone), "deep-cloned address lists compare equal");
	check (g_mime_internet_address_list_compare (list, clone) == 0, "deep-cloned address list ordering");
	other = g_mime_internet_address_list_get_address (clone, 0);
	check (group != other && !strcmp (other->charset, "iso-8859-1"), "clone preserves charset and independent group ownership");
	child = g_mime_internet_address_list_get_address (GMIME_GROUP_ADDRESS (other)->members, 0);
	route = g_mime_mailbox_address_get_route (GMIME_MAILBOX_ADDRESS (child));
	g_mime_domain_list_add (route, "new.example");
	check (g_mime_internet_address_list_equal (list, clone), "mailbox equality ignores routes");
	check (g_mime_domain_list_length (GMIME_MAILBOX_ADDRESS (
		g_mime_internet_address_list_get_address (GMIME_GROUP_ADDRESS (group)->members, 0))->route) == 2,
		"cloned routes are independent");
	check (g_mime_internet_address_list_contains (list, other), "list lookup uses recursive address equality");
	g_mime_mailbox_address_set_addrspec (GMIME_MAILBOX_ADDRESS (child), "changed@example.com", NULL);
	check (!g_mime_internet_address_list_equal (list, clone) &&
	       !g_mime_internet_address_list_contains (list, other), "member changes affect recursive equality and lookup");
	check (g_mime_internet_address_compare (group, other) == 0, "group ordering ignores members");
	flat = g_mime_internet_address_list_get_mailboxes (list);
	check (g_mime_internet_address_list_length (flat) == 3, "flattening descends into nested groups");
	check (g_mime_internet_address_list_get_address (flat, 0) ==
	       g_mime_internet_address_list_get_address (GMIME_GROUP_ADDRESS (group)->members, 0),
	       "flattening retains the original mailbox objects");
	g_object_unref (clone);
	g_object_unref (list);
	check (!strcmp (g_mime_mailbox_address_get_addrspec (GMIME_MAILBOX_ADDRESS (
		g_mime_internet_address_list_get_address (flat, 1))), "two@example.com"), "flattened list owns mailbox references");
	g_object_unref (flat);
}

static void
test_ordering (void)
{
	static const struct {
		const char *left, *right;
		int sign;
	} names[] = {
		{ NULL, "", -1 }, { "Alpha", "ALPHA", 1 }, { "\303\237", "ss", 1 },
		{ "\317\202", "\317\203", -1 }, { "\304\261", "I", 1 }, { "\305\277", "S", 1 },
		{ "\360\220\200\200", "\356\200\200", 1 },
		{ "\315\205", "\316\231", -1 }, { "\342\205\260", "\342\205\240", 1 },
		{ "\342\223\220", "\342\222\266", 1 }
	};
	static const struct {
		const char *left, *right;
		int sign;
	} addresses[] = {
		{ "a@EXAMPLE.COM", "a@example.com", 0 },
		{ "a@b\303\274cher.example", "a@XN--BCHER-KVA.EXAMPLE", 0 },
		{ "a@B\303\234CHER.EXAMPLE", "a@xn--bcher-kva.example", 0 },
		{ "A@example.com", "a@example.com", -1 },
		{ "\303\211@example.com", "\303\251@example.com", -1 },
		{ "a@example", "a@example.com", -1 },
		{ "b@a.example", "a@z.example", 1 },
		{ "a@example.com", "aa@example.com", -1 },
		{ "User", "user", -1 }, { "", "", 0 },
		{ "a@[IPv6:ABCD::1]", "a@[ipv6:abcd::1]", 0 }
	};
	GMimeInternetAddress *left, *right;
	GMimeInternetAddressList *list, *other;
	guint i;
	int result;

	for (i = 0; i < G_N_ELEMENTS (names); i++) {
		left = g_mime_mailbox_address_new (names[i].left, "a@example.com", NULL);
		right = g_mime_mailbox_address_new (names[i].right, "a@example.com", NULL);
		result = g_mime_internet_address_compare (left, right);
		check (((result > 0) - (result < 0)) == names[i].sign, "case-sensitive UTF-8 name ordering");
		g_object_unref (left);
		g_object_unref (right);
	}
	for (i = 0; i < G_N_ELEMENTS (addresses); i++) {
		left = g_mime_mailbox_address_new ("same", addresses[i].left, NULL);
		right = g_mime_mailbox_address_new ("same", addresses[i].right, NULL);
		g_assert_nonnull (left);
		g_assert_nonnull (right);
		result = g_mime_internet_address_compare (left, right);
		check (((result > 0) - (result < 0)) == addresses[i].sign,
		       "mailbox ordering preserves local-part case and canonicalizes IDN domains");
		result = g_mime_internet_address_compare (right, left);
		check (((result > 0) - (result < 0)) == -addresses[i].sign, "mailbox ordering is antisymmetric");
		check (g_mime_internet_address_compare (left, left) == 0, "mailbox ordering is reflexive");
		g_object_unref (left);
		g_object_unref (right);
	}
	left = g_mime_mailbox_address_new ("same", "a@EXAMPLE.COM", NULL);
	right = g_mime_mailbox_address_new ("same", "a@example.com", NULL);
	check (g_mime_internet_address_compare (left, right) == 0, "domain case does not affect mailbox ordering");
	check (!g_mime_internet_address_equal (left, right), "ordering equality is not value equality");
	g_object_unref (right);
	right = g_mime_group_address_new ("same");
	check (g_mime_internet_address_compare (left, right) < 0, "equal-name mailboxes sort before groups");
	g_object_unref (left);
	g_object_unref (right);
	list = g_mime_internet_address_list_parse_full (NULL, "a@example.com", -1, NULL);
	other = g_mime_internet_address_list_parse_full (NULL, "a@example.com, b@example.com", -1, NULL);
	check (g_mime_internet_address_list_compare (list, other) < 0, "list ordering uses length after equal prefixes");
	g_object_unref (list);
	g_object_unref (other);
}

static void
test_collections (void)
{
	GMimeInternetAddressList *list, *prepend, *members;
	GMimeInternetAddress *ia, *group, *inner;
	guint i, len;

	list = g_mime_internet_address_list_parse_full (NULL, "c@example.com, d@example.com, e@example.com", -1, NULL);
	prepend = g_mime_internet_address_list_parse_full (NULL, "a@example.com, b@example.com", -1, NULL);
	g_mime_internet_address_list_prepend (list, prepend);
	check_string (g_mime_internet_address_list_to_string (list, NULL, FALSE),
		      "a@example.com, b@example.com, c@example.com, d@example.com, e@example.com", "prepend moves only existing entries");
	g_mime_internet_address_list_append (list, list);
	check (g_mime_internet_address_list_length (list) == 10, "self-append uses the original source length");
	len = list->array->len;
	g_mime_internet_address_list_prepend (list, list);
	check (list->array->len == 2 * len, "self-prepend uses the original source length");
	for (i = 0; i < len; i++)
		check (list->array->pdata[i] == list->array->pdata[len + i], "self-copy preserves object identity and order");
	ia = g_mime_internet_address_clone (list->array->pdata[0]);
	check (g_mime_internet_address_list_remove (list, ia) && list->array->len == 19, "remove uses value equality");
	g_object_unref (ia);
	g_object_unref (list);
	g_object_unref (prepend);

	list = g_mime_internet_address_list_parse_full (NULL, "Outer: Inner: a@example.com;;", -1, NULL);
	group = g_mime_internet_address_list_get_address (list, 0);
	inner = g_mime_internet_address_list_get_address (GMIME_GROUP_ADDRESS (group)->members, 0);
	members = GMIME_GROUP_ADDRESS (inner)->members;
	g_mime_group_address_set_members (GMIME_GROUP_ADDRESS (group), members);
	ia = g_mime_internet_address_list_get_address (members, 0);
	g_mime_internet_address_list_set_address (list, 0, ia);
	check_string (g_mime_internet_address_list_to_string (list, NULL, FALSE), "a@example.com",
		      "replacement refs descendants before releasing their owners");
	g_object_unref (list);
}

static void
test_parsing (void)
{
	static const char *looser[] = {
		"a.@example.com", "a..b@example.com", "a...@example.com",
		"<a\\@b@example.com>", "<a\\@b..@example.com>"
	};
	static const struct {
		const char *text;
		gboolean strict, loose;
	} cases[] = {
		{ "a@example.com", TRUE, TRUE },
		{ "a@example.com, b@example.com", TRUE, TRUE },
		{ "local (Name) (extra), b@example.com", TRUE, TRUE },
		{ "a@example.com b@example.com", FALSE, TRUE },
		{ "Team:;", TRUE, TRUE },
		{ "Team: a@example.com", FALSE, TRUE },
		{ "Team: a@example.com; b@example.com", FALSE, TRUE },
		{ "a@example.com, @bad, b@example.com", FALSE, FALSE },
		{ "a@example.com (unterminated", FALSE, FALSE },
		{ "\"a\nb\"@example.com", FALSE, FALSE },
		{ "a@[bad\001]", FALSE, FALSE },
		{ "a@[\377]", FALSE, FALSE },
		{ "a@[\303\251]", FALSE, FALSE },
		{ "<@(comment)one:a@example.com>", FALSE, FALSE },
		{ "<a@example.com", FALSE, TRUE },
		{ "<<a@example.com>>", FALSE, TRUE },
		{ "", FALSE, FALSE }
	};
	GMimeParserOptions *options = g_mime_parser_options_new ();
	GMimeInternetAddressList *list;
	GMimeInternetAddress *address;
	GError *error = NULL;
	const char bounded[] = { 'a', '@', 'b', '!', '!' };
	const char nul[] = { 'a', '@', 'b', '\0', ',', 'x', '@', 'y' };
	guint i, strict, mode;

	for (strict = 0; strict < 2; strict++) {
		g_mime_parser_options_set_address_compliance_mode (options, strict ?
			GMIME_RFC_COMPLIANCE_STRICT : GMIME_RFC_COMPLIANCE_LOOSE);
		for (i = 0; i < G_N_ELEMENTS (cases); i++) {
			gboolean valid = strict ? cases[i].strict : cases[i].loose;

			list = g_mime_internet_address_list_parse_full (options, cases[i].text, -1, &error);
			testsuite_check ("checked address-list parsing[%u,%u]", strict, i);
			if ((list != NULL) == valid && (valid ? error == NULL : error != NULL))
				testsuite_check_passed ();
			else
				testsuite_check_failed ("input [%s], expected success=%d", cases[i].text, valid);
			g_clear_object (&list);
			g_clear_error (&error);
		}
	}
	address = g_mime_internet_address_parse (NULL, bounded, 3, &error);
	check (address != NULL && error == NULL, "single-address parsing honors exact nonterminated length");
	g_clear_object (&address);
	address = g_mime_mailbox_address_parse (NULL, "Team: a@example.com;", -1, &error);
	check (address == NULL && error != NULL, "mailbox parser rejects groups");
	g_clear_error (&error);
	address = g_mime_group_address_parse (NULL, "a@example.com", -1, &error);
	check (address == NULL && error != NULL, "group parser rejects mailboxes");
	g_clear_error (&error);
	address = g_mime_internet_address_parse (NULL, "a@example.com, b@example.com", -1, &error);
	check (address == NULL && error != NULL, "single-address parser rejects extra addresses");
	g_clear_error (&error);
	address = g_mime_internet_address_parse (NULL, nul, sizeof (nul), &error);
	check (address == NULL && error != NULL, "bounded address parsing rejects embedded NUL");
	g_clear_error (&error);
	g_mime_parser_options_set_max_address_group_depth (options, 1);
	address = g_mime_group_address_parse (options, "Outer: Inner: a@example.com;;", -1, &error);
	check (address == NULL && error != NULL, "checked parser enforces group depth");
	g_clear_error (&error);
	g_mime_parser_options_set_address_compliance_mode (options, GMIME_RFC_COMPLIANCE_LOOSER);
	address = g_mime_mailbox_address_parse (options, "<a\\@b..@example.com>", -1, &error);
	check (address != NULL && error == NULL, "looser compliance accepts escaped-at and trailing dots");
	if (address)
		check_string (g_strdup (g_mime_mailbox_address_get_addrspec (GMIME_MAILBOX_ADDRESS (address))),
			      "a%40b..@example.com", "looser local part normalization");
	g_clear_object (&address);
	for (i = 0; i < G_N_ELEMENTS (looser); i++) {
		for (mode = 0; mode < 3; mode++) {
			g_mime_parser_options_set_address_compliance_mode (options,
				mode == 0 ? GMIME_RFC_COMPLIANCE_STRICT :
				mode == 1 ? GMIME_RFC_COMPLIANCE_LOOSE : GMIME_RFC_COMPLIANCE_LOOSER);
			address = g_mime_mailbox_address_parse (options, looser[i], -1, &error);
			check ((address != NULL) == (mode == 2) && (error != NULL) == (mode != 2),
			       "looser local-part extensions are gated by compliance mode");
			g_clear_object (&address);
			g_clear_error (&error);
		}
	}
	list = g_mime_internet_address_list_parse (NULL, "a@example.com, @bad, b@example.com");
	check (list && list->array->len == 2, "legacy list parsing retains best-effort recovery");
	g_clear_object (&list);
	list = g_mime_internet_address_list_parse (NULL,
		"IN%\"list@example.com\" \"List recipients\", valid@example.com");
	check (list && list->array->len == 1, "header recovery skips malformed domainless address fragments");
	if (list)
		check_string (g_mime_internet_address_list_to_string (list, NULL, FALSE), "valid@example.com",
			      "header recovery resumes at the next comma");
	g_clear_object (&list);
	g_mime_parser_options_free (options);
}

static void
test_international_formatting (void)
{
	GMimeFormatOptions *options = g_mime_format_options_new ();
	GMimeInternetAddress *address = g_mime_mailbox_address_new ("A \"B\" C", "a@example.com", NULL);
	GMimeHeaderList *headers = g_mime_header_list_new (NULL);
	GMimeHeader *header;

	g_mime_format_options_set_allow_international (options, TRUE);
	g_mime_format_options_set_max_line_length (options, 61);
	check_string (g_mime_internet_address_to_string (address, options, TRUE),
		      "\"A \\\"B\\\" C\" <a@example.com>", "international phrases escape embedded quotes");
	g_mime_internet_address_set_name (address, "A\\B");
	check_string (g_mime_internet_address_to_string (address, options, TRUE),
		      "\"A\\\\B\" <a@example.com>", "international phrases escape backslashes");
	g_object_unref (address);
	address = g_mime_mailbox_address_new ("Jos\303\251", "a@example.com", NULL);
	check_string (g_mime_internet_address_to_string (address, NULL, TRUE),
		      "=?utf-8?b?Sm9zw6k=?= <a@example.com>", "default address name charset is UTF-8");
	g_mime_header_list_append (headers, "From", "", NULL);
	header = g_mime_header_list_get_header_at (headers, 0);
	g_mime_header_set_value (header, options,
		"Name <@one.example,@two.example:a@b\303\274cher.example>, Next <b@example.com>", NULL);
	check (!strcmp (testsuite_header_raw_text (header),
		" Name <@one.example,@two.example:a@b\303\274cher.example>, Next\n\t<b@example.com>\n"),
	       "international folding counts Unicode columns rather than UTF-8 bytes");
	g_object_unref (address);
	g_object_unref (headers);
	g_mime_format_options_free (options);
}

static void
test_notifications (void)
{
	const char *raw = " \tUser\r\n\t<@one.example:user@example.com>\r\n";
	GMimeMessage *message = g_mime_message_new (FALSE);
	GMimeInternetAddressList *list;
	GMimeInternetAddress *address;
	GMimeDomainList *route;
	GMimeHeader *header;
	GError *error = NULL;

	g_mime_entity_set_header (GMIME_ENTITY (message), "To", "", NULL);
	header = g_mime_header_list_get_header (g_mime_entity_get_header_list (GMIME_ENTITY (message)), "To");
	testsuite_header_set_raw_text (header, raw);
	list = g_mime_message_get_addresses (message, GMIME_ADDRESS_TYPE_TO);
	address = g_mime_internet_address_list_get_address (list, 0);
	route = g_mime_mailbox_address_get_route (GMIME_MAILBOX_ADDRESS (address));
	g_mime_domain_list_set_domain (route, 0, "one.example");
	g_mime_mailbox_address_set_addrspec (GMIME_MAILBOX_ADDRESS (address), "user@example.com", NULL);
	check (!strcmp (testsuite_header_raw_text (header), raw), "equivalent route and mailbox assignments preserve raw headers");
	check (!g_mime_mailbox_address_set_addrspec (GMIME_MAILBOX_ADDRESS (address), "invalid address", &error),
	       "invalid mailbox assignment reports failure");
	check (!strcmp (testsuite_header_raw_text (header), raw), "invalid mailbox assignment preserves raw headers");
	g_clear_error (&error);
	g_mime_domain_list_set_domain (route, 0, "two.example");
	check (!strcmp (testsuite_header_raw_text (header), " User <@two.example:user@example.com>\n"),
	       "route mutations update the owning header");
	g_mime_internet_address_list_add (list, address);
	g_mime_internet_address_list_remove_at (list, 0);
	g_mime_domain_list_add (route, "three.example");
	check (!strcmp (testsuite_header_raw_text (header), " User <@two.example,@three.example:user@example.com>\n"),
	       "removing one duplicate preserves remaining change subscriptions");
	check (!g_mime_message_add_mailbox (message, GMIME_ADDRESS_TYPE_TO, NULL, "invalid address", &error) &&
	       list->array->len == 1, "invalid message mailbox addition is atomic");
	g_clear_error (&error);
	g_object_unref (message);
}

static void
test_header_recovery (void)
{
	static const struct {
		const char *text;
		int loose, strict, members;
	} cases[] = {
		{ "a@example.com b@example.com", 2, 0, -1 },
		{ "Team: a@example.com; b@example.com", 2, 0, -1 },
		{ "a@example.com, (unterminated", 0, 0, -1 },
		{ "Team: a@example.com, (unterminated", 1, 1, 0 },
		{ "Team: a@example.com", 1, 1, 1 },
		{ "a@example.com, @bad, b@example.com", 2, 2, -1 },
		{ "seed@example.com\nTo: a@example.com, (unterminated", 1, 1, -1 },
		{ "a@example.com b@example.com\nTo: c@example.com", 3, 1, -1 }
	};
	const char *raw = " IN%\"list@example.com\" \"List recipients\"\n";
	GMimeParserOptions *options = g_mime_parser_options_new ();
	GMimeMessage *message = g_mime_message_new (FALSE);
	GMimeInternetAddressList *list;
	GMimeInternetAddress *address;
	GMimeHeader *header;
	GMimeStream *stream;
	GMimeParser *parser;
	GError *error = NULL;
	guint i, mode;
	char *text;

	g_mime_entity_append_header (GMIME_ENTITY (message), "To", "", NULL);
	header = g_mime_header_list_get_header (g_mime_entity_get_header_list (GMIME_ENTITY (message)), "To");
	testsuite_header_set_raw_text (header, raw);
	list = g_mime_message_get_to (message);
	check (g_mime_internet_address_list_length (list) == 0, "malformed header does not introduce partial recipients");
	check (!strcmp (testsuite_header_raw_text (header), raw), "address recovery preserves the original raw header");
	g_mime_entity_append_header (GMIME_ENTITY (message), "To", "List recipients <list@example.com>", NULL);
	check_string (g_mime_internet_address_list_to_string (list, NULL, FALSE), "List recipients <list@example.com>",
		      "valid duplicate address header survives malformed header recovery");
	g_object_unref (message);

	for (mode = 0; mode < 3; mode++) {
		g_mime_parser_options_set_address_compliance_mode (options, mode == 0 ?
			GMIME_RFC_COMPLIANCE_LOOSE : mode == 1 ? GMIME_RFC_COMPLIANCE_STRICT : GMIME_RFC_COMPLIANCE_LOOSER);
		for (i = 0; i < G_N_ELEMENTS (cases); i++) {
			text = g_strconcat ("To: ", cases[i].text, "\n\n", NULL);
			stream = g_mime_stream_mem_new_with_buffer (text, strlen (text));
			parser = g_mime_parser_new_with_stream (stream);
			message = g_mime_parser_construct_message (parser, options, &error);
			check (message != NULL && error == NULL, "malformed address header preserves message parsing");
			if (message) {
				list = g_mime_message_get_to (message);
				check (g_mime_internet_address_list_length (list) == (mode == 1 ? cases[i].strict : cases[i].loose),
				       "failed address header does not retain partial recipients");
				if (cases[i].members >= 0) {
					address = g_mime_internet_address_list_get_address (list, 0);
					check (GMIME_IS_GROUP_ADDRESS (address) &&
					       g_mime_internet_address_list_length (g_mime_group_address_get_members (
						       GMIME_GROUP_ADDRESS (address))) == cases[i].members,
					       "failed group member parsing discards partial members");
				}
				g_object_unref (message);
			}
			g_clear_error (&error);
			g_object_unref (parser);
			g_object_unref (stream);
			g_free (text);
		}
	}
	g_mime_parser_options_free (options);
}

int
main (int argc, char **argv)
{
	g_mime_init ();
	testsuite_init (argc, argv);
	testsuite_start ("address models and checked parsing");
	test_domains ();
	test_domain_whitespace ();
	test_mailboxes ();
	test_idn_accessors ();
	test_values ();
	test_ordering ();
	test_collections ();
	test_parsing ();
	test_international_formatting ();
	test_notifications ();
	test_header_recovery ();
	testsuite_end ();
	g_mime_shutdown ();

	return testsuite_exit ();
}
