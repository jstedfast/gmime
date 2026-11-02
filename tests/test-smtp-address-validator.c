/* GMime SMTP validator tests.
 * Copyright (C) 2026 Jeffrey Stedfast
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <gmime/gmime.h>
#include <string.h>
#include "testsuite.h"
#include "smtp-address-vectors.h"

static void
check (const char *address, gssize length, guint options, int expected, gboolean utf8)
{
	GMimeSmtpUtf8AddressError utf8_error = -1;
	GMimeSmtpAddressError smtp_error = -1;
	gboolean valid;
	int error;

	if (utf8) {
		valid = g_mime_smtp_utf8_address_try_validate (address, length, options, &utf8_error);
		error = utf8_error;
		g_assert_cmpint (g_mime_smtp_utf8_address_try_validate (address, length, options, NULL), ==, valid);
	} else {
		valid = g_mime_smtp_address_try_validate (address, length, options, &smtp_error);
		error = smtp_error;
		g_assert_cmpint (g_mime_smtp_address_try_validate (address, length, options, NULL), ==, valid);
	}
	if (error != expected)
		g_error ("SMTP%s validation of %.*s: expected %d, got %d (options %u)",
			 utf8 ? "UTF8" : "", (int) (length < 0 ? strlen (address) : (gsize) length),
			 address, expected, error, options);
	g_assert_cmpint (valid, ==, expected == 0);
}

static void
test_vectors (void)
{
	gsize length;
	char *bounded;
	guint i;

	for (i = 0; i < G_N_ELEMENTS (smtp_vectors); i++) {
		length = strlen (smtp_vectors[i].address);
		check (smtp_vectors[i].address, -1, smtp_vectors[i].options, smtp_vectors[i].expected, smtp_vectors[i].utf8);
		bounded = g_memdup2 (smtp_vectors[i].address, MAX (length, 1));
		check (bounded, length, smtp_vectors[i].options, smtp_vectors[i].expected, smtp_vectors[i].utf8);
		g_free (bounded);
	}
}

static char *
repeat (const char *text, guint count)
{
	GString *str = g_string_new (NULL);
	guint i;

	for (i = 0; i < count; i++)
		g_string_append (str, text);
	return g_string_free (str, FALSE);
}

static char *
domain_of_length (guint length)
{
	GString *str = g_string_new (NULL);
	guint n;

	while (length) {
		n = MIN (length, 63);
		length -= n;
		while (n--)
			g_string_append_c (str, 'a');
		if (length) {
			g_string_append_c (str, '.');
			length--;
		}
	}
	return g_string_free (str, FALSE);
}

static void
check_parts (const char *local, const char *domain, guint options, int expected, gboolean utf8)
{
	char *address = g_strconcat (local, "@", domain, NULL);

	check (address, -1, options, expected, utf8);
	g_free (address);
}

static void
test_lengths (void)
{
	char *local, *domain, *address;
	guint utf8, limit, i, n;
	static const char *characters[] = { "a", "\303\270", "\344\276\213", "\360\240\200\200" };

	for (utf8 = 0; utf8 < 2; utf8++) {
		limit = utf8 ? 4 : 2;
		for (i = 0; i < 2; i++) {
			local = repeat ("a", 64 + i);
			check_parts (local, "example.com", limit, i ? (utf8 ? 2 : 3) : 0, utf8);
			check_parts (local, "example.com", 0, 0, utf8);
			g_free (local);
			domain = repeat ("a", 63 + i);
			check_parts ("u", domain, limit, i ? (utf8 ? 3 : 4) : 0, utf8);
			g_free (domain);
		}
		for (n = 252; n <= 254; n++) {
			domain = domain_of_length (n);
			check_parts ("u", domain, limit, n == 252 ? 0 : (n == 253 ? (utf8 ? 5 : 6) : (utf8 ? 4 : 5)), utf8);
			check_parts ("u", domain, 0, 0, utf8);
			g_free (domain);
		}
		local = repeat ("a", 65);
		check_parts (local, "exa..mple", limit, 1, utf8);
		domain = repeat ("a", 64);
		check_parts (local, domain, limit, utf8 ? 2 : 3, utf8);
		g_free (domain);
		g_free (local);
		domain = domain_of_length (254);
		address = g_strconcat (domain, ".aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa", NULL);
		check_parts ("u", address, limit, utf8 ? 3 : 4, utf8);
		g_free (address);
		g_free (domain);
		for (n = 243; n <= 244; n++) {
			local = repeat ("a", n);
			domain = g_strconcat ("[tag:", local, "]", NULL);
			check_parts ("user", domain, limit, n == 243 ? 0 : (utf8 ? 5 : 6), utf8);
			g_free (domain);
			g_free (local);
		}
	}
	for (i = 0; i < G_N_ELEMENTS (characters); i++) {
		char *padding, *exact;

		n = 64 / strlen (characters[i]);
		local = repeat (characters[i], n);
		padding = repeat ("a", 64 - strlen (local));
		exact = g_strconcat (local, padding, NULL);
		check_parts (exact, "example", 4, 0, TRUE);
		address = g_strconcat (exact, "a", NULL);
		check_parts (address, "example", 4, 2, TRUE);
		g_free (padding);
		g_free (exact);
		g_free (local);
		g_free (address);
	}
	/* Repeated U+00F8 encodes as xn--pda plus one byte per additional scalar. */
	domain = repeat ("\303\270", 57);
	check_parts ("u", domain, 4, 0, TRUE);
	g_free (domain);
	domain = repeat ("\303\270", 58);
	check_parts ("u", domain, 4, 3, TRUE);
	g_free (domain);
	domain = repeat ("\303\251.", 32);
	address = g_strconcat (domain, "com", NULL);
	check_parts ("u", address, 4, 4, TRUE);
	g_free (address);
	g_free (domain);
	domain = repeat ("a", 60);
	address = g_strconcat ("xn--", domain, NULL);
	check_parts ("u", address, 1, 6, TRUE);
	check_parts ("u", address, 5, 3, TRUE);
	g_free (address);
	g_free (domain);
	local = repeat ("\303\270", 65);
	check_parts (local, "example", 2, 2, FALSE);
	g_free (local);
	for (n = 28; n <= 29; n++) {
		local = repeat ("F|", n);
		address = g_strconcat ("\"\033$B", local, "\033(B\"", NULL);
		check_parts (address, "example.jp", 3, n == 28 ? 0 : 3, FALSE);
		g_free (address);
		g_free (local);
	}
}

static void
test_native_inputs (void)
{
	static const char *invalid[] = {
		"a\200@b", "a\300\257@b", "a\355\240\200@b", "a\364\220\200\200@b",
		"a\370\210\200\200\200@b", "a\377@b", "a@b\342\202"
	};
	char embedded[] = { 'a', '@', 'b', 0, 'x' };
	char suffix[] = "a@b BAD";
	guint i;

	for (i = 0; i < G_N_ELEMENTS (invalid); i++) {
		check (invalid[i], -1, 0, 1, FALSE);
		check (invalid[i], -1, 0, 1, TRUE);
	}
	check (embedded, sizeof (embedded), 0, 1, FALSE);
	check (embedded, sizeof (embedded), 0, 1, TRUE);
	check (embedded, -1, 0, 0, TRUE);
	check (suffix, 3, 0, 0, TRUE);
	check (suffix, 3, 0, 0, FALSE);
	check (suffix, 0, 0, 1, TRUE);
	check ("a@\303\270", 3, 0, 1, TRUE);
}

static void
test_mailboxes (void)
{
	GMimeMailboxAddress *mailbox = GMIME_MAILBOX_ADDRESS (g_mime_mailbox_address_new ("\342\200\256", "a@example.com", NULL));
	GMimeSmtpUtf8AddressError utf8_error;
	GMimeSmtpAddressError smtp_error;

	g_mime_domain_list_add (g_mime_mailbox_address_get_route (mailbox), "\303\270.example");
	g_assert_true (g_mime_smtp_address_try_validate_mailbox (mailbox, 0, &smtp_error));
	g_assert_cmpint (smtp_error, ==, GMIME_SMTP_ADDRESS_ERROR_NONE);
	g_assert_true (g_mime_smtp_utf8_address_try_validate_mailbox (mailbox, 0, &utf8_error));
	g_assert_true (g_mime_mailbox_address_set_addrspec (mailbox, "user@xn--gr-zia.example", NULL));
	g_assert_false (g_mime_smtp_utf8_address_try_validate_mailbox (mailbox, 0, &utf8_error));
	g_assert_cmpint (utf8_error, ==, GMIME_SMTP_UTF8_ADDRESS_ERROR_A_LABEL);
	g_assert_true (g_mime_smtp_utf8_address_try_validate_mailbox (mailbox, 1, NULL));
	g_assert_cmpstr (g_mime_mailbox_address_get_addrspec (mailbox), ==, "user@xn--gr-zia.example");
	g_assert_true (g_mime_mailbox_address_set_addrspec (mailbox, "\303\270@example.com", NULL));
	g_assert_false (g_mime_smtp_address_try_validate_mailbox (mailbox, 0, &smtp_error));
	g_assert_cmpint (smtp_error, ==, GMIME_SMTP_ADDRESS_ERROR_NON_ASCII);
	g_assert_true (g_mime_smtp_utf8_address_try_validate_mailbox (mailbox, 0, NULL));
	g_assert_true (g_mime_mailbox_address_set_addrspec (mailbox, "local", NULL));
	g_assert_false (g_mime_smtp_utf8_address_try_validate_mailbox (mailbox, 0, &utf8_error));
	g_assert_cmpint (utf8_error, ==, GMIME_SMTP_UTF8_ADDRESS_ERROR_SYNTAX);
	g_object_unref (mailbox);
}

static void
test_large_inputs (void)
{
	GString *str = g_string_new ("a");
	char *marks = repeat ("\314\201", 50000);
	guint i;

	g_string_append (str, marks);
	g_string_append (str, "@example");
	check (str->str, str->len, 0, 8, TRUE);
	g_string_assign (str, "\303\241");
	g_string_append (str, marks);
	g_string_append (str, "@example");
	check (str->str, str->len, 0, 0, TRUE);
	g_free (marks);
	g_string_truncate (str, 0);
	for (i = 0; i < 50000; i++)
		g_string_append (str, "\343\203\273");
	g_string_append (str, "@example");
	check (str->str, str->len, 0, 9, TRUE);
	g_string_assign (str, "u@");
	for (i = 0; i < 100000; i++)
		g_string_append (str, "\320\266");
	check (str->str, str->len, 4, 3, TRUE);
	g_string_truncate (str, 0);
	for (i = 0; i < 10000; i++)
		g_string_append (str, "\330\250\331\216\331\216\342\200\214\331\216\331\216");
	g_string_append (str, "\330\250@example");
	check (str->str, str->len, 0, 0, TRUE);
	g_string_free (str, TRUE);
}

static void
test_scalar_results (void)
{
	GChecksum *checksum = g_checksum_new (G_CHECKSUM_SHA256);
	GMimeSmtpUtf8AddressError utf8_error;
	GMimeSmtpAddressError smtp_error;
	guchar results[12];
	char address[24];
	gunichar cp;
	guint i;
	int n;

	for (cp = 0; cp <= 0x10ffff; cp++) {
		if (cp >= 0xd800 && cp <= 0xdfff)
			continue;
		n = g_unichar_to_utf8 (cp, address);
		memcpy (address + n, "@example.com", 12);
		for (i = 0; i < 4; i++) {
			g_mime_smtp_address_try_validate (address, n + 12, i, &smtp_error);
			results[i] = smtp_error;
		}
		for (i = 0; i < 8; i++) {
			g_mime_smtp_utf8_address_try_validate (address, n + 12, i, &utf8_error);
			results[4 + i] = utf8_error;
		}
		g_checksum_update (checksum, results, sizeof (results));
	}
	/* All 13,344,768 results from actual MimeKit bbccea7630ba9475d03a2a09a1c5046924d8bf3f. */
	g_assert_cmpstr (g_checksum_get_string (checksum), ==,
			"dcee586965967e3e418f80589b17c6a624aa5919ef875d6b5f38198944976896");
	g_checksum_free (checksum);
}

int
main (int argc, char **argv)
{
	g_mime_init ();
	testsuite_init (argc, argv);
	testsuite_start ("SMTP address validation");
	testsuite_check ("Imported SMTP and SMTPUTF8 vectors");
	test_vectors ();
	testsuite_check_passed ();
	testsuite_check ("Length boundaries and precedence");
	test_lengths ();
	testsuite_check_passed ();
	testsuite_check ("Bounded native input");
	test_native_inputs ();
	testsuite_check_passed ();
	testsuite_check ("Mailbox wrappers");
	test_mailboxes ();
	testsuite_check_passed ();
	testsuite_check ("Large Unicode inputs");
	test_large_inputs ();
	testsuite_check_passed ();
	testsuite_check ("Every Unicode scalar with all option combinations");
	test_scalar_results ();
	testsuite_check_passed ();
	testsuite_end ();
	g_mime_shutdown ();
	return testsuite_exit ();
}
