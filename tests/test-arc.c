/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*  GMime
 *  Copyright (C) Jeffrey Stedfast
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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <string.h>
#include <gmime/gmime.h>
#include <gmime/gmime-arc.h>
#include "testsuite.h"

/* RFC 8463 Appendix A's public test seed, wrapped as PKCS#8. */
static const char pem[] = "-----BEGIN PRIVATE KEY-----\n"
	"MC4CAQAwBQYDK2VwBCIEIJ1hsZ3v/VpguoRK9JLsLMREScVpezJpGXA7rAMcrn9g\n"
	"-----END PRIVATE KEY-----\n";
static const char *txt[] = { "v=DKIM1; k=ed25519; ", "p=11qYAYKxCrfVS/7TyWQHOg7hcvPapiMlrwIaaPcHURo=", NULL };
static const char *rsa_txt[] = {
	"v=DKIM1; k=rsa; p=MIGfMA0GCSqGSIb3DQEBAQUAA4GNADCBiQKBgQDkHlOQoBTzWRiGs5V6NpP3id",
	"Y6Wk08a5qhdR6wy5bdOKb2jLQiY/J16JYi0Qvx/byYzCNb3W91y3FutACDfzwQ/BC/e/8uBsCR+yz1Lx",
	"j+PL6lHvqMKrM3rG4hstT5QjvHO9PzoxZyVYLzBfO2EeC3Ip3G+2kryOTIKT+l/K4w3QIDAQAB", NULL
};

typedef struct {
	GResolver parent;
	guint lookups;
	gboolean revoke_first;
} ArcResolver;
typedef GResolverClass ArcResolverClass;
G_DEFINE_TYPE (ArcResolver, arc_resolver, G_TYPE_RESOLVER)

static GList *
lookup_records (GResolver *resolver, const char *name, GResolverRecordType type,
		GCancellable *cancellable, GError **error)
{
	ArcResolver *self = (ArcResolver *) resolver;
	(void) cancellable;

	self->lookups++;
	g_assert_cmpint (type, ==, G_RESOLVER_RECORD_TXT);
	if (!strcmp (name, "dummy._domainkey.example.org."))
		return g_list_append (NULL, g_variant_ref_sink (g_variant_new ("(^as)", rsa_txt)));
	if (self->revoke_first && g_str_has_prefix (name, "one.")) {
		g_set_error_literal (error, G_RESOLVER_ERROR, G_RESOLVER_ERROR_NOT_FOUND, "Key unavailable");
		return NULL;
	}
	g_assert_true (g_str_has_suffix (name, "._domainkey.mx.test."));
	return g_list_append (NULL, g_variant_ref_sink (g_variant_new ("(^as)", txt)));
}

static void
arc_resolver_class_init (ArcResolverClass *klass)
{
	klass->lookup_records = lookup_records;
}

static void
arc_resolver_init (ArcResolver *self)
{
	(void) self;
}

static GBytes *
bytes (const char *text)
{
	return g_bytes_new (text, strlen (text));
}

static void
expect_status (GResolver *resolver, GBytes *message, GMimeArcChainStatus expected)
{
	GError *error = NULL;
	GMimeMessage *parsed = testsuite_parse_message (g_bytes_get_data (message, NULL), g_bytes_get_size (message));
	GMimeArcChainStatus actual = g_mime_arc_verify (parsed, resolver, NULL, &error);
	g_object_unref (parsed);

	if (expected == GMIME_ARC_CHAIN_FAIL)
		g_assert_nonnull (error);
	else
		g_assert_no_error (error);
	g_assert_cmpint (actual, ==, expected);
	g_clear_error (&error);
}

/* Independent wire vector, signed with Python cryptography Ed25519 over
 * manually canonicalized octets. The current seal's b= is folded and is
 * not the last tag. Header order on the wire is not AS digest order. */
static const char oracle[] =
	"ARC-Authentication-Results: i=1; mx.test; none\r\n"
	"From: person@example.org\r\n"
	"ARC-Seal: i=1; cv=none; a=ed25519-sha256;\r\n b=\r\n"
	"\tZukpdJNR8sVBdudTLikUe5acldNCW9LOo/laYG2JWEcvo7qfZQVOmqcNcpFa+suNgiGgcHxpKuuYXvJRDsxJAA== ; d=mx.test; s=test\r\n"
	"Subject: hello\r\n\tthere \r\n"
	"ARC-Message-Signature: i=1; a=ed25519-sha256; c=relaxed/relaxed;\r\n"
	" d=mx.test; s=test; h=from:subject; bh=VyqV/unA8yADB4nkiDcHr/4SSC+7HqBLPqgmfIeokPs=;\r\n"
	" b=9L84cOWaW+S0VJVLEPinxzBrfjkLlwuGVnYHFvRcCybNQzKihIM5rxWJaw4f9v6XqkPXDIYct0ofy1/ahyAyBg==\r\n"
	"\r\nhello world  \r\n\r\n";

static void
test_structure (GResolver *resolver)
{
	static const char * const fields[] = {
		"ARC-Seal: i=0; cv=none",
		"ARC-Seal: i=51; cv=none",
		"ARC-Seal: i=001; cv=none",
		"ARC-Seal: i=-1; cv=none",
		"ARC-Seal: cv=none",
		"ARC-Seal: i=2; cv=pass",
		"ARC-Authentication-Results \t: i=1; mx.test; none",
		"ARC-Message-Signature: i=1; b=aa",
		"ARC-Seal: i=1; cv=none",
		NULL
	};
	ArcResolver *self = (ArcResolver *) resolver;
	GBytes *fixture = bytes (oracle);
	guint i, lookups;

	expect_status (resolver, fixture, GMIME_ARC_CHAIN_PASS);
	lookups = self->lookups;
	for (i = 0; fields[i]; i++) {
		char *text = g_strconcat (fields[i], "\r\n", oracle, NULL);
		GBytes *message = bytes (text);
		expect_status (resolver, message, GMIME_ARC_CHAIN_FAIL);
		g_bytes_unref (message);
		g_free (text);
	}
	{
		char *bad_cv = g_strdup (oracle);
		char *cv = strstr (bad_cv, "cv=none");
		GBytes *message = NULL;

		memcpy (cv, "cv=pass", 7);
		message = bytes (bad_cv);
		expect_status (resolver, message, GMIME_ARC_CHAIN_FAIL);
		g_bytes_unref (message);
		g_free (bad_cv);
	}
	g_assert_cmpuint (self->lookups, ==, lookups);
	g_bytes_unref (fixture);
}

static GBytes *
seal (GBytes *message, GMimeArcChainStatus status, const char *selector, const char *payload, GError **error)
{
	GBytes *key = bytes (pem);
	GMimeMessage *parsed = testsuite_parse_message (g_bytes_get_data (message, NULL), g_bytes_get_size (message));
	gboolean ok = g_mime_arc_seal (parsed, status, payload, "mx.test", selector,
				GMIME_DKIM_ALGORITHM_ED25519_SHA256, key,
				GMIME_DKIM_CANONICALIZATION_RELAXED, GMIME_DKIM_CANONICALIZATION_RELAXED,
				NULL, error);
	GBytes *sealed = ok ? testsuite_serialize_message (parsed) : NULL;
	if (!ok) {
		GBytes *unchanged = testsuite_serialize_message (parsed);
		g_assert_true (g_bytes_equal (message, unchanged));
		g_bytes_unref (unchanged);
	}
	g_object_unref (parsed);
	g_bytes_unref (key);
	return sealed;
}

/* Mutate one byte without assuming a NUL-terminated message. */
static GBytes *
mutate (GBytes *message, const char *needle)
{
	gsize length;
	const char *data = g_bytes_get_data (message, &length);
	char *copy = g_memdup2 (data, length);
	char *found = g_strstr_len (copy, length, needle);
	g_assert_nonnull (found);
	*found = *found == 'a' ? 'b' : 'a';
	return g_bytes_new_take (copy, length);
}

static void
test_chain (GResolver *resolver)
{
	static const char original[] = "From: user@mx.test\r\nSubject: opaque\r\n\tfold\r\n\r\nbody\0binary \t\r\n";
	ArcResolver *self = (ArcResolver *) resolver;
	GBytes *message = g_bytes_new_static (original, sizeof original - 1);
	GBytes *one = NULL, *changed = NULL, *two = NULL, *altered = NULL, *failed = NULL;
	GError *error = NULL;
	gconstpointer data;
	gsize length;
	guint lookups;

	expect_status (resolver, message, GMIME_ARC_CHAIN_NONE);
	one = seal (message, GMIME_ARC_CHAIN_NONE, "one", "first.test;\r\n\tspf=pass", &error);
	g_assert_no_error (error);
	g_assert_nonnull (one);
	data = g_bytes_get_data (one, &length);
	g_assert_cmpuint (length, >, sizeof original - 1);
	g_assert_cmpmem ((const char *) data + length - (sizeof original - 1), sizeof original - 1,
			 original, sizeof original - 1);
	expect_status (resolver, one, GMIME_ARC_CHAIN_PASS);
	changed = mutate (one, "body");
	expect_status (resolver, changed, GMIME_ARC_CHAIN_FAIL);
	/* The second hop received the original, then modified the body. */
	two = seal (changed, GMIME_ARC_CHAIN_PASS, "two", "second.test; arc=pass", &error);
	g_assert_no_error (error);
	lookups = self->lookups;
	expect_status (resolver, two, GMIME_ARC_CHAIN_PASS);
	g_assert_cmpuint (self->lookups - lookups, ==, 3); /* latest AMS, AS2, AS1 */
	self->revoke_first = TRUE;
	expect_status (resolver, two, GMIME_ARC_CHAIN_FAIL);
	self->revoke_first = FALSE;
	altered = mutate (two, "first.test");
	expect_status (resolver, altered, GMIME_ARC_CHAIN_FAIL);
	g_clear_pointer (&altered, g_bytes_unref);
	altered = mutate (two, "second.test");
	expect_status (resolver, altered, GMIME_ARC_CHAIN_FAIL);

	failed = seal (changed, GMIME_ARC_CHAIN_FAIL, "two", "second.test; arc=fail", &error);
	g_assert_no_error (error);
	g_assert_nonnull (failed);
	lookups = self->lookups;
	expect_status (resolver, failed, GMIME_ARC_CHAIN_FAIL);
	g_assert_cmpuint (self->lookups, ==, lookups);
	g_assert_null (seal (failed, GMIME_ARC_CHAIN_FAIL, "two", "second.test; none", &error));
	g_assert_nonnull (error);
	g_clear_error (&error);
	g_assert_null (seal (message, GMIME_ARC_CHAIN_NONE, "one", "mx.test; none\r\nInjected: test", &error));
	g_assert_nonnull (error);
	g_clear_error (&error);
	g_bytes_unref (message);
	g_bytes_unref (one);
	g_bytes_unref (changed);
	g_bytes_unref (two);
	g_bytes_unref (altered);
	g_bytes_unref (failed);
}

static void
test_instance_limit (GResolver *resolver)
{
	GBytes *message = bytes ("From: person@mx.test\r\n\r\nbody\r\n");
	GError *error = NULL;
	guint i;

	for (i = 1; i <= 50; i++) {
		GBytes *next = seal (message, i == 1 ? GMIME_ARC_CHAIN_NONE : GMIME_ARC_CHAIN_PASS,
				     "limit", "mx.test; none", &error);
		g_assert_no_error (error);
		g_bytes_unref (message);
		message = next;
	}
	expect_status (resolver, message, GMIME_ARC_CHAIN_PASS);
	g_assert_null (seal (message, GMIME_ARC_CHAIN_PASS, "limit", "mx.test; none", &error));
	g_assert_nonnull (error);
	g_clear_error (&error);
	g_bytes_unref (message);
}

static GBytes *
load_fixture (const char *directory, const char *name)
{
	char *path = g_build_filename (directory, name, NULL);
	GError *error = NULL;
	char *data = NULL;
	gsize length;

	g_file_get_contents (path, &data, &length, &error);
	g_assert_no_error (error);
	g_free (path);
	return g_bytes_new_take (data, length);
}

static void
test_rsa_signer (GResolver *resolver, const char *directory)
{
	static const char * const files[] = {
		"c_simple_simple.eml", "i0_base.eml",
		"message_body_eol_wsp.eml", "message_body_inl_wsp.eml", "message_body_end_lines.eml",
		"message_body_trail_crlf.eml", "headers_field_name_case.eml", "headers_field_unfold.eml",
		"headers_eol_wsp.eml", "headers_inl_wsp.eml", "headers_col_wsp.eml"
	};
	static const char * const hdrs[] = { "mime-version", "date", "from", "to", "subject", NULL };
	static const char aar[] = "i=1; lists.example.org; arc=none; spf=pass smtp.mfrom=jqd@d1.example; "
		"dkim=pass (1024-bit key) header.i=@d1.example; dmarc=pass";
	GBytes *key = load_fixture (directory, "dummy.pem");

	for (guint i = 0; i < G_N_ELEMENTS (files); i++) {
		GMimeDkimCanonicalization canon = i ? GMIME_DKIM_CANONICALIZATION_RELAXED : GMIME_DKIM_CANONICALIZATION_SIMPLE;
		GBytes *input = load_fixture (directory, files[i]);
		GMimeMessage *message = testsuite_parse_message (g_bytes_get_data (input, NULL), g_bytes_get_size (input));
		GMimeHeaderList *list = g_mime_entity_get_header_list (GMIME_ENTITY (message));
		GError *error = NULL;
		GMimeHeader *header;
		const char *value;
		GBytes *wire;

		testsuite_check ("RSA-SHA256 signer: %s", files[i]);
		g_mime_arc_seal (message, GMIME_ARC_CHAIN_NONE, aar + strlen ("i=1; "), "example.org", "dummy",
				 GMIME_DKIM_ALGORITHM_RSA_SHA256, key, canon, canon, hdrs, &error);
		g_assert_no_error (error);
		header = g_mime_header_list_get_header_at (list, 0);
		g_assert_cmpstr (g_mime_header_get_name (header), ==, "ARC-Seal");
		value = g_mime_header_get_value (header);
		g_assert_nonnull (strstr (value, "cv=none"));
		g_assert_nonnull (strstr (value, "a=rsa-sha256"));
		header = g_mime_header_list_get_header_at (list, 1);
		g_assert_cmpstr (g_mime_header_get_name (header), ==, "ARC-Message-Signature");
		value = g_mime_header_get_value (header);
		g_assert_nonnull (strstr (value, i ? "c=relaxed/relaxed" : "c=simple/simple"));
		g_assert_nonnull (strstr (value, "h=mime-version:date:from:to:subject"));
		g_assert_nonnull (strstr (value, i ? "bh=KWSe46TZKCcDbH4klJPo+tjk5LWJnVRlP5pvjXFZYLQ=" :
					 "bh=hhFbTjokraRYc/Af+8v4zyKm/9ApHGkBSLO129NtPbo="));
		header = g_mime_header_list_get_header_at (list, 2);
		g_assert_cmpstr (g_mime_header_get_name (header), ==, "ARC-Authentication-Results");
		g_assert_cmpstr (g_mime_header_get_value (header), ==, aar);
		wire = testsuite_serialize_message (message);
		expect_status (resolver, wire, GMIME_ARC_CHAIN_PASS);
		g_bytes_unref (wire);
		g_object_unref (message);
		g_bytes_unref (input);
		testsuite_check_passed ();
	}
	g_bytes_unref (key);
}

static void
test_rsa_verifier (GResolver *resolver, const char *directory)
{
	static const struct {
		const char *file;
		GMimeArcChainStatus expected;
	} vectors[] = {
		{ "old-body-changed.eml", GMIME_ARC_CHAIN_PASS },
		{ "old-seal-invalid.eml", GMIME_ARC_CHAIN_FAIL },
		{ "folded-tags.eml", GMIME_ARC_CHAIN_PASS },
		{ "ams-duplicate.eml", GMIME_ARC_CHAIN_FAIL },
		{ "ams-invalid-name.eml", GMIME_ARC_CHAIN_FAIL },
		{ "seal-forbidden-h.eml", GMIME_ARC_CHAIN_FAIL },
		{ "seal-duplicate.eml", GMIME_ARC_CHAIN_FAIL }
	};
	GBytes *one = load_fixture (directory, "signed-1.eml");
	GBytes *two = load_fixture (directory, "signed-2.eml");
	GBytes *draft = load_fixture (directory, "draft-cv_pass_i1_1.eml");
	const char * const changes[] = { "arc=pass", "arc=none", "Hey gang,", "Example 1" };

	expect_status (resolver, one, GMIME_ARC_CHAIN_PASS);
	expect_status (resolver, two, GMIME_ARC_CHAIN_PASS);
	for (guint i = 0; i < G_N_ELEMENTS (changes); i++) {
		GBytes *bad = mutate (two, changes[i]);
		expect_status (resolver, bad, GMIME_ARC_CHAIN_FAIL);
		g_bytes_unref (bad);
	}
	/* RFC 8617 requires leading instances and prohibits signing ARC fields in h=. */
	expect_status (resolver, draft, GMIME_ARC_CHAIN_FAIL);
	g_bytes_unref (draft);
	g_bytes_unref (two);
	g_bytes_unref (one);
	for (guint i = 0; i < G_N_ELEMENTS (vectors); i++) {
		GBytes *input = load_fixture (directory, vectors[i].file);
		expect_status (resolver, input, vectors[i].expected);
		g_bytes_unref (input);
	}
}

int
main (int argc, char **argv)
{
	GResolver *resolver = g_object_new (arc_resolver_get_type (), NULL);
	const char *directory = argc > 1 && argv[argc - 1][0] != '-' ? argv[argc - 1] : "data/arc";

	g_mime_init ();
	testsuite_init (argc, argv);
	testsuite_start ("ARC");
	test_rsa_signer (resolver, directory);
	testsuite_check ("Independent RSA simple/simple AMS, seals and historic set tampering");
	test_rsa_verifier (resolver, directory);
	testsuite_check_passed ();
	testsuite_check ("Independent AS/AMS vector, transported order, duplicate and malformed instances");
	test_structure (resolver);
	testsuite_check_passed ();
	testsuite_check ("Byte-preserving multi-hop seals, modified body, tampering, historical keys and terminal failure");
	test_chain (resolver);
	testsuite_check_passed ();
	testsuite_check ("50 complete sets verify; a 51st cannot be appended");
	test_instance_limit (resolver);
	testsuite_check_passed ();
	testsuite_end ();
	g_object_unref (resolver);
	g_mime_shutdown ();
	return testsuite_exit ();
}
