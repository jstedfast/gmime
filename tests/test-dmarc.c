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

#include <config.h>
#include "testsuite.h"
#include "gmime/gmime-dmarc.h"
#include "gmime/gmime-dkim.h"

typedef struct {
	GResolver parent;
	GHashTable *zone;
	GString *queries;
} TestResolver;

typedef GResolverClass TestResolverClass;
G_DEFINE_TYPE (TestResolver, test_resolver, G_TYPE_RESOLVER)

static GList *
lookup_records (GResolver *resolver, const char *name, GResolverRecordType type, GCancellable *cancel, GError **error)
{
	TestResolver *test = (TestResolver *) resolver;
	const char *response = g_hash_table_lookup (test->zone, name);
	GList *records = NULL;
	char **rrs;

	g_assert_cmpint (type, ==, G_RESOLVER_RECORD_TXT);
	g_assert_true (g_str_has_suffix (name, "."));
	if (test->queries->len)
		g_string_append_c (test->queries, ' ');
	g_string_append (test->queries, name);
	if (!response || !strcmp (response, "SERVFAIL")) {
		g_set_error_literal (error, G_RESOLVER_ERROR, response ? G_RESOLVER_ERROR_TEMPORARY_FAILURE : G_RESOLVER_ERROR_NOT_FOUND, "DNS failure");
		return NULL;
	}
	if (!strcmp (response, "CANCEL")) {
		g_set_error_literal (error, G_IO_ERROR, G_IO_ERROR_CANCELLED, "Cancelled during DNS lookup");
		return NULL;
	}
	rrs = g_strsplit (response, "\n", -1);
	for (guint i = 0; rrs[i]; i++) {
		char **chunks = g_strsplit (rrs[i], "|", -1);
		records = g_list_prepend (records, g_variant_ref_sink (g_variant_new ("(^as)", chunks)));
		g_strfreev (chunks);
	}
	g_strfreev (rrs);
	return g_list_reverse (records);
}

static void
test_resolver_finalize (GObject *object)
{
	TestResolver *test = (TestResolver *) object;
	g_hash_table_unref (test->zone);
	g_string_free (test->queries, TRUE);
	G_OBJECT_CLASS (test_resolver_parent_class)->finalize (object);
}

static void
test_resolver_class_init (TestResolverClass *klass)
{
	G_OBJECT_CLASS (klass)->finalize = test_resolver_finalize;
	klass->lookup_records = lookup_records;
}

static void
test_resolver_init (TestResolver *test)
{
	test->zone = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, g_free);
	test->queries = g_string_new (NULL);
}

typedef struct {
	TestResolver *dns;
	GMimeDmarcVerifier *verifier;
	GMimeDmarcResult *result;
} Fixture;

static void
setup (Fixture *f, gconstpointer unused)
{
	f->dns = g_object_new (test_resolver_get_type (), NULL);
	f->verifier = g_mime_dmarc_verifier_new (G_RESOLVER (f->dns));
}

static void
teardown (Fixture *f, gconstpointer unused)
{
	g_mime_dmarc_result_free (f->result);
	g_object_unref (f->verifier);
	g_object_unref (f->dns);
}

static void
publish (Fixture *f, const char *name, const char *response)
{
	g_hash_table_replace (f->dns->zone, g_strdup (name), g_strdup (response));
}

static GMimeMessage *
message_new (const char *from)
{
	/* Sender-supplied results must never authenticate the message. */
	char *wire = g_strconcat ("Authentication-Results: mx.example; dkim=pass header.d=example.com; spf=pass smtp.mailfrom=example.com\r\n",
				  from ? "From: " : "", from ? from : "", from ? "\r\n" : "", "\r\nbody\r\n", NULL);
	GMimeMessage *message = testsuite_parse_message (wire, strlen (wire));
	g_free (wire);
	return message;
}

static void
verify (Fixture *f, const char *from, const GMimeDmarcIdentifier *dkim, guint n_dkim, const GMimeDmarcIdentifier *spf)
{
	GMimeMessage *message = message_new (from);
	GError *error = NULL;
	g_string_truncate (f->dns->queries, 0);
	g_mime_dmarc_result_free (f->result);
	f->result = g_mime_dmarc_verifier_verify_with_results (f->verifier, message, dkim, n_dkim, spf, NULL, &error);
	g_assert_no_error (error);
	g_assert_nonnull (f->result);
	g_object_unref (message);
}

static void
expect (Fixture *f, GMimeDmarcStatus status, GMimeDmarcErrors errors)
{
	g_assert_cmpint (g_mime_dmarc_result_get_status (f->result), ==, status);
	g_assert_cmpint (g_mime_dmarc_result_get_errors (f->result), ==, errors);
}

static void
test_authors (Fixture *f, gconstpointer unused)
{
	static const struct {
		const char *from;
		GMimeDmarcErrors error;
	} invalid[] = {
		{ NULL, GMIME_DMARC_ERRORS_NO_FROM_HEADER },
		{ "a@example.com\r\nFrom: b@example.net", GMIME_DMARC_ERRORS_MULTIPLE_FROM_HEADERS },
		{ "undisclosed-recipients:;", GMIME_DMARC_ERRORS_NO_AUTHOR_DOMAIN },
		{ "local", GMIME_DMARC_ERRORS_NO_AUTHOR_DOMAIN },
		{ "a@[127.0.0.1]", GMIME_DMARC_ERRORS_INVALID_AUTHOR_DOMAIN },
		{ "a@example.com, b@example.net", GMIME_DMARC_ERRORS_MULTIPLE_AUTHOR_DOMAINS }
	};
	GMimeDmarcIdentifier pass = { GMIME_DMARC_IDENTIFIER_PASS, "xn--bcher-kva.example" };

	for (guint i = 0; i < G_N_ELEMENTS (invalid); i++) {
		verify (f, invalid[i].from, NULL, 0, &pass);
		expect (f, GMIME_DMARC_STATUS_PERM_ERROR, invalid[i].error);
		g_assert_nonnull (g_mime_dmarc_result_get_spf_result (f->result));
		g_assert_null (g_mime_dmarc_result_get_author_domain (f->result));
		g_assert_cmpstr (f->dns->queries->str, ==, "");
	}
	publish (f, "_dmarc.xn--bcher-kva.example.", "v=DMARC1; p=reject");
	verify (f, "Group: a@b\303\274cher.example, b@XN--BCHER-KVA.example;", &pass, 1, NULL);
	expect (f, GMIME_DMARC_STATUS_PASS, GMIME_DMARC_ERRORS_NONE);
	g_assert_cmpstr (g_mime_dmarc_result_get_author_domain (f->result), ==, pass.domain);
	g_assert_cmpuint (g_mime_dmarc_result_get_author_domain_result_count (f->result), ==, 0);
}

static void
test_multi_author (Fixture *f, gconstpointer unused)
{
	GMimeDmarcIdentifier pass = { GMIME_DMARC_IDENTIFIER_PASS, "a.example" };
	const char *from = "a@a.example, b@b.example, c@c.example";

	publish (f, "_dmarc.a.example.", "v=DMARC1; p=reject");
	publish (f, "_dmarc.b.example.", "v=DMARC1; p=quarantine");
	publish (f, "_dmarc.c.example.", "v=DMARC1; p=reject");
	g_mime_dmarc_verifier_set_max_author_domains (f->verifier, 2);
	verify (f, from, &pass, 1, NULL);
	expect (f, GMIME_DMARC_STATUS_PERM_ERROR, GMIME_DMARC_ERRORS_MULTIPLE_AUTHOR_DOMAINS);
	g_mime_dmarc_verifier_set_max_author_domains (f->verifier, 3);
	verify (f, from, &pass, 1, NULL);
	expect (f, GMIME_DMARC_STATUS_FAIL, GMIME_DMARC_ERRORS_NONE);
	g_assert_cmpstr (g_mime_dmarc_result_get_author_domain (f->result), ==, "c.example");
	g_assert_cmpuint (g_mime_dmarc_result_get_author_domain_result_count (f->result), ==, 3);
	g_assert_cmpint (g_mime_dmarc_result_get_status (g_mime_dmarc_result_get_author_domain_result (f->result, 0)), ==, GMIME_DMARC_STATUS_PASS);
	g_assert_cmpint (g_mime_dmarc_result_get_policy (g_mime_dmarc_result_get_author_domain_result (f->result, 1)), ==, GMIME_DMARC_POLICY_QUARANTINE);
	g_assert_true (g_mime_dmarc_result_get_author_domain_result (f->result, 2) == f->result);
	g_assert_cmpstr (f->dns->queries->str, ==, "_dmarc.a.example. _dmarc.example. _dmarc.b.example. _dmarc.c.example.");
	publish (f, "_dmarc.b.example.", "SERVFAIL");
	verify (f, "a@a.example, b@b.example", &pass, 1, NULL);
	expect (f, GMIME_DMARC_STATUS_TEMP_ERROR, GMIME_DMARC_ERRORS_DNS_TEMPORARY_FAILURE);
}

static void
test_walk (Fixture *f, gconstpointer unused)
{
	GMimeDmarcIdentifier id = { GMIME_DMARC_IDENTIFIER_PASS, "foo.example" };
	char *label = g_strnfill (63, 'a'), *from;

	verify (f, "u@a.b.c.d.e.f.g.h.i.j", NULL, 0, NULL);
	expect (f, GMIME_DMARC_STATUS_NONE, GMIME_DMARC_ERRORS_NONE);
	g_assert_cmpstr (f->dns->queries->str, ==, "_dmarc.a.b.c.d.e.f.g.h.i.j. _dmarc.d.e.f.g.h.i.j. _dmarc.e.f.g.h.i.j. _dmarc.f.g.h.i.j. _dmarc.g.h.i.j. _dmarc.h.i.j. _dmarc.i.j. _dmarc.j.");
	from = g_strdup_printf ("u@%s.%s.%s.%s", label, label, label, label + 2);
	verify (f, from, NULL, 0, NULL);
	expect (f, GMIME_DMARC_STATUS_NONE, GMIME_DMARC_ERRORS_NONE);
	g_assert_null (strstr (f->dns->queries->str, from + 2));
	g_assert_nonnull (strstr (f->dns->queries->str, strchr (from, '.') + 1));
	g_free (from);
	g_free (label);

	publish (f, "_dmarc.example.", "v=DMARC1; p=reject; sp=quarantine; psd=y");
	verify (f, "u@mail.foo.example", &id, 1, NULL);
	expect (f, GMIME_DMARC_STATUS_PASS, GMIME_DMARC_ERRORS_NONE);
	g_assert_cmpstr (g_mime_dmarc_result_get_organizational_domain (f->result), ==, "foo.example");
	g_assert_cmpint (g_mime_dmarc_result_get_policy_domain_source (f->result), ==, GMIME_DMARC_POLICY_DOMAIN_SOURCE_PUBLIC_SUFFIX_DOMAIN);
	g_assert_cmpint (g_mime_dmarc_result_get_policy (f->result), ==, GMIME_DMARC_POLICY_QUARANTINE);
	id.domain = "bar.example";
	verify (f, "u@mail.foo.example", &id, 1, NULL);
	expect (f, GMIME_DMARC_STATUS_FAIL, GMIME_DMARC_ERRORS_NONE);
	verify (f, "u@example", NULL, 0, NULL);
	g_assert_cmpint (g_mime_dmarc_result_get_policy_domain_source (f->result), ==, GMIME_DMARC_POLICY_DOMAIN_SOURCE_AUTHOR_DOMAIN);
	publish (f, "_dmarc.foo.example.", "v=DMARC1; p=none");
	verify (f, "u@mail.foo.example", NULL, 0, NULL);
	g_assert_cmpint (g_mime_dmarc_result_get_policy_domain_source (f->result), ==, GMIME_DMARC_POLICY_DOMAIN_SOURCE_ORGANIZATIONAL_DOMAIN);
	g_assert_cmpint (g_mime_dmarc_result_get_policy (f->result), ==, GMIME_DMARC_POLICY_NONE);
	publish (f, "_dmarc.foo.example.", "v=DMARC1; p=reject; psd=n");
	verify (f, "u@mail.foo.example", NULL, 0, NULL);
	g_assert_cmpstr (f->dns->queries->str, ==, "_dmarc.mail.foo.example. _dmarc.foo.example.");
}

static void
test_alignment (Fixture *f, gconstpointer unused)
{
	static const GMimeDmarcIdentifier dkim[] = {
		{ GMIME_DMARC_IDENTIFIER_FAIL, "example.com" },
		{ GMIME_DMARC_IDENTIFIER_PASS, "unrelated.example" },
		{ GMIME_DMARC_IDENTIFIER_PASS, "mail.Example.COM." },
		{ GMIME_DMARC_IDENTIFIER_PASS, "example.com" }
	};
	GMimeDmarcIdentifier spf = { GMIME_DMARC_IDENTIFIER_PASS, "mail.example.com" };
	const char *from = "u@news.example.com";

	verify (f, from, dkim, 4, NULL);
	expect (f, GMIME_DMARC_STATUS_NONE, GMIME_DMARC_ERRORS_NONE);
	g_assert_cmpuint (g_mime_dmarc_result_get_dkim_result_count (f->result), ==, 4);
	g_assert_null (g_mime_dmarc_result_get_record (f->result));
	publish (f, "_dmarc.example.com.", "v=spf1 -all\nv=DMA|RC1; p=reject");
	verify (f, from, dkim, 4, &spf);
	expect (f, GMIME_DMARC_STATUS_PASS, GMIME_DMARC_ERRORS_NONE);
	g_assert_true (g_mime_dmarc_result_get_spf_aligned (f->result));
	g_assert_true (g_mime_dmarc_result_get_dkim_aligned (f->result));
	g_assert_true (g_mime_dmarc_result_get_aligned_dkim_result (f->result) == g_mime_dmarc_result_get_dkim_result (f->result, 2));
	g_assert_cmpstr (f->dns->queries->str, ==, "_dmarc.news.example.com. _dmarc.example.com. _dmarc.com. _dmarc.mail.example.com.");
	g_mime_dmarc_verifier_set_max_dkim_signatures (f->verifier, 2);
	spf.domain = "bad/domain";
	verify (f, from, dkim, 4, &spf);
	expect (f, GMIME_DMARC_STATUS_FAIL, GMIME_DMARC_ERRORS_NONE);
	g_assert_cmpuint (g_mime_dmarc_result_get_dkim_result_count (f->result), ==, 4);

	publish (f, "_dmarc.example.com.", "v=DMARC1; p=reject; adkim=s; aspf=s");
	spf.domain = "mail.example.com";
	verify (f, from, dkim + 2, 1, &spf);
	expect (f, GMIME_DMARC_STATUS_FAIL, GMIME_DMARC_ERRORS_NONE);
	g_assert_cmpstr (f->dns->queries->str, ==, "_dmarc.news.example.com. _dmarc.example.com. _dmarc.com.");
	spf.domain = "news.example.com";
	verify (f, from, NULL, 0, &spf);
	expect (f, GMIME_DMARC_STATUS_PASS, GMIME_DMARC_ERRORS_NONE);
	spf.status = GMIME_DMARC_IDENTIFIER_SOFT_FAIL;
	verify (f, from, NULL, 0, &spf);
	expect (f, GMIME_DMARC_STATUS_FAIL, GMIME_DMARC_ERRORS_NONE);
	publish (f, "_dmarc.example.com.", "v=DMARC1; p=reject");
	publish (f, "_dmarc.mail.example.com.", "v=DMARC1; p=none; psd=n");
	verify (f, from, dkim + 2, 1, NULL);
	expect (f, GMIME_DMARC_STATUS_FAIL, GMIME_DMARC_ERRORS_NONE);
}

static void
test_incomplete (Fixture *f, gconstpointer unused)
{
	GMimeDmarcIdentifier id = { GMIME_DMARC_IDENTIFIER_PASS, "example.com" };
	GMimeDmarcIdentifier spf = { GMIME_DMARC_IDENTIFIER_PASS, "mail.example.com" };

	publish (f, "_dmarc.example.com.", "SERVFAIL");
	verify (f, "u@mail.example.com", &id, 1, NULL);
	expect (f, GMIME_DMARC_STATUS_TEMP_ERROR, GMIME_DMARC_ERRORS_DNS_TEMPORARY_FAILURE);
	g_assert_null (g_mime_dmarc_result_get_record (f->result));
	publish (f, "_dmarc.mail.example.com.", "v=DMARC1; p=reject");
	verify (f, "u@mail.example.com", &id, 1, &spf);
	expect (f, GMIME_DMARC_STATUS_PASS, GMIME_DMARC_ERRORS_DNS_TEMPORARY_FAILURE);
	g_assert_null (g_mime_dmarc_result_get_organizational_domain (f->result));
	verify (f, "u@mail.example.com", &id, 1, NULL);
	expect (f, GMIME_DMARC_STATUS_TEMP_ERROR, GMIME_DMARC_ERRORS_DNS_TEMPORARY_FAILURE);
	publish (f, "_dmarc.mail.example.com.", "v=DMARC1; p=reject; adkim=s");
	verify (f, "u@mail.example.com", &id, 1, NULL);
	expect (f, GMIME_DMARC_STATUS_FAIL, GMIME_DMARC_ERRORS_DNS_TEMPORARY_FAILURE);

	publish (f, "_dmarc.example.com.", "v=DMARC1; p=reject");
	publish (f, "_dmarc.mail.example.com.", "SERVFAIL");
	verify (f, "u@example.com", NULL, 0, &spf);
	expect (f, GMIME_DMARC_STATUS_TEMP_ERROR, GMIME_DMARC_ERRORS_DNS_TEMPORARY_FAILURE);
	verify (f, "u@example.com", &id, 1, &spf);
	expect (f, GMIME_DMARC_STATUS_PASS, GMIME_DMARC_ERRORS_NONE);
	spf.status = id.status = GMIME_DMARC_IDENTIFIER_TEMP_ERROR;
	verify (f, "u@example.com", &id, 1, &spf);
	expect (f, GMIME_DMARC_STATUS_TEMP_ERROR, GMIME_DMARC_ERRORS_SPF_TEMPORARY_ERROR | GMIME_DMARC_ERRORS_DKIM_TEMPORARY_ERROR);
	id.domain = spf.domain = "unrelated.example";
	verify (f, "u@example.com", &id, 1, &spf);
	expect (f, GMIME_DMARC_STATUS_FAIL, GMIME_DMARC_ERRORS_NONE);
}

static void
test_policy (Fixture *f, gconstpointer unused)
{
	const char *from = "u@mail.example.com";
	GMimeAuthenticationMethodResult *method;
	GMimeAuthenticationMethodProperty *property;

	publish (f, "_dmarc.example.com.", "v=DMARC1; sp=reject");
	verify (f, from, NULL, 0, NULL);
	expect (f, GMIME_DMARC_STATUS_NONE, GMIME_DMARC_ERRORS_INVALID_POLICY_RECORD);
	g_assert_nonnull (g_mime_dmarc_result_get_record (f->result));
	g_assert_cmpint (g_mime_dmarc_result_get_policy (f->result), ==, GMIME_DMARC_POLICY_UNSPECIFIED);
	publish (f, "_dmarc.example.com.", "v=DMARC1; p=reject; sp=bogus; rua=mailto:a@example.com");
	verify (f, from, NULL, 0, NULL);
	expect (f, GMIME_DMARC_STATUS_FAIL, GMIME_DMARC_ERRORS_INVALID_POLICY_RECORD);
	g_assert_cmpint (g_mime_dmarc_result_get_policy (f->result), ==, GMIME_DMARC_POLICY_NONE);
	publish (f, "_dmarc.example.com.", "v=DMARC1; p=reject; sp=quarantine; t=y");
	publish (f, "_dmarc.mail.example.com.", "v=DMARC1; p=none\nv=DMARC1; p=reject");
	verify (f, from, NULL, 0, NULL);
	expect (f, GMIME_DMARC_STATUS_FAIL, GMIME_DMARC_ERRORS_MULTIPLE_RECORDS);
	g_assert_cmpstr (g_mime_dmarc_result_get_policy_domain (f->result), ==, "example.com");
	g_assert_true (g_mime_dmarc_result_get_testing (f->result));
	g_assert_cmpint (g_mime_dmarc_result_get_requested_policy (f->result), ==, GMIME_DMARC_POLICY_QUARANTINE);
	g_assert_cmpint (g_mime_dmarc_result_get_policy (f->result), ==, GMIME_DMARC_POLICY_NONE);
	verify (f, "u@example.com", NULL, 0, NULL);
	g_assert_cmpint (g_mime_dmarc_result_get_policy (f->result), ==, GMIME_DMARC_POLICY_QUARANTINE);
	method = g_mime_dmarc_result_to_authentication_method_result (f->result);
	g_assert_cmpstr (g_mime_authentication_method_result_get_method (method), ==, "dmarc");
	g_assert_cmpstr (g_mime_authentication_method_result_get_result (method), ==, "fail");
	g_assert_cmpint (g_mime_authentication_method_result_get_property_count (method), ==, 2);
	property = g_mime_authentication_method_result_get_property (method, 0);
	g_assert_cmpstr (g_mime_authentication_method_property_get_property_type (property), ==, "header");
	g_assert_cmpstr (g_mime_authentication_method_property_get_property (property), ==, "from");
	g_assert_cmpstr (g_mime_authentication_method_property_get_value (property), ==, "example.com");
	property = g_mime_authentication_method_result_get_property (method, 1);
	g_assert_cmpstr (g_mime_authentication_method_property_get_property_type (property), ==, "policy");
	g_assert_cmpstr (g_mime_authentication_method_property_get_property (property), ==, "dmarc");
	g_assert_cmpstr (g_mime_authentication_method_property_get_value (property), ==, "quarantine");
	g_object_unref (method);
}

static GMimeDmarcDomainExistence
nxdomain (const char *domain, GCancellable *cancellable, gpointer data, GError **error)
{
	g_assert_cmpstr (domain, ==, "missing.example.com");
	return GMIME_DMARC_DOMAIN_NONEXISTENT;
}

static void
test_np (Fixture *f, gconstpointer unused)
{
	GMimeDmarcIdentifier pass = { GMIME_DMARC_IDENTIFIER_PASS, "example.com" };

	publish (f, "_dmarc.example.com.", "v=DMARC1; p=none; np=reject");
	verify (f, "u@example.com", NULL, 0, NULL);
	expect (f, GMIME_DMARC_STATUS_FAIL, GMIME_DMARC_ERRORS_NONE);
	g_assert_cmpint (g_mime_dmarc_result_get_policy (f->result), ==, GMIME_DMARC_POLICY_NONE);
	verify (f, "u@missing.example.com", NULL, 0, NULL);
	expect (f, GMIME_DMARC_STATUS_TEMP_ERROR, GMIME_DMARC_ERRORS_DNS_TEMPORARY_FAILURE);
	g_assert_cmpstr (f->dns->queries->str, ==, "_dmarc.missing.example.com. _dmarc.example.com. _dmarc.com. missing.example.com.");
	verify (f, "u@missing.example.com", &pass, 1, NULL);
	expect (f, GMIME_DMARC_STATUS_PASS, GMIME_DMARC_ERRORS_NONE);
	publish (f, "exists.example.com.", "v=spf1 -all");
	verify (f, "u@exists.example.com", NULL, 0, NULL);
	expect (f, GMIME_DMARC_STATUS_FAIL, GMIME_DMARC_ERRORS_NONE);
	g_assert_cmpint (g_mime_dmarc_result_get_policy (f->result), ==, GMIME_DMARC_POLICY_NONE);
	g_mime_dmarc_verifier_set_existence_func (f->verifier, nxdomain, NULL, NULL);
	verify (f, "u@missing.example.com", NULL, 0, NULL);
	expect (f, GMIME_DMARC_STATUS_FAIL, GMIME_DMARC_ERRORS_NONE);
	g_assert_cmpint (g_mime_dmarc_result_get_policy (f->result), ==, GMIME_DMARC_POLICY_REJECT);
	publish (f, "_dmarc.example.com.", "v=DMARC1; p=none; sp=reject; np=reject");
	verify (f, "u@other.example.com", NULL, 0, NULL);
	expect (f, GMIME_DMARC_STATUS_FAIL, GMIME_DMARC_ERRORS_NONE);
	g_assert_cmpstr (f->dns->queries->str, ==, "_dmarc.other.example.com. _dmarc.example.com. _dmarc.com.");
}

static void
test_cancel (Fixture *f, gconstpointer unused)
{
	GMimeMessage *message = message_new ("u@example.com");
	GCancellable *cancel = g_cancellable_new ();
	GError *error = NULL;

	g_cancellable_cancel (cancel);
	g_assert_null (g_mime_dmarc_verifier_verify (f->verifier, message, NULL, cancel, &error));
	g_assert_error (error, G_IO_ERROR, G_IO_ERROR_CANCELLED);
	g_clear_error (&error);
	g_assert_cmpstr (f->dns->queries->str, ==, "");
	publish (f, "_dmarc.example.com.", "CANCEL");
	g_assert_null (g_mime_dmarc_verifier_verify_with_results (f->verifier, message, NULL, 0, NULL, NULL, &error));
	g_assert_error (error, G_IO_ERROR, G_IO_ERROR_CANCELLED);
	g_clear_error (&error);
	g_object_unref (cancel);
	g_object_unref (message);
}

static void
test_actual_dkim (Fixture *f, gconstpointer unused)
{
	GMimeMessage *message = message_new ("u@example.com");
	GError *error = NULL;

#if defined (HAVE_DKIM_GNUTLS) || defined (HAVE_DKIM_OPENSSL)
	GBytes *key;
	/* RFC 8463 Appendix A test key. */
	static const char test_key[] =
		"-----BEGIN PRIVATE KEY-----\n"
		"MC4CAQAwBQYDK2VwBCIEIJ1hsZ3v/VpguoRK9JLsLMREScVpezJpGXA7rAMcrn9g\n"
		"-----END PRIVATE KEY-----\n";
	key = g_bytes_new_static (test_key, sizeof test_key - 1);
	g_assert_true (g_mime_dkim_sign (message, "example.com", "key", GMIME_DKIM_ALGORITHM_ED25519_SHA256, key,
					GMIME_DKIM_CANONICALIZATION_RELAXED, GMIME_DKIM_CANONICALIZATION_RELAXED, NULL, &error));
	g_assert_no_error (error);
	g_bytes_unref (key);
#else
	g_mime_entity_set_header (GMIME_ENTITY (message), "DKIM-Signature", "v=1; d=example.com", NULL);
#endif
	f->result = g_mime_dmarc_verifier_verify (f->verifier, message, NULL, NULL, &error);
	g_assert_no_error (error);
	expect (f, GMIME_DMARC_STATUS_NONE, GMIME_DMARC_ERRORS_NONE);
	g_assert_cmpuint (g_mime_dmarc_result_get_dkim_result_count (f->result), ==, 0);
	g_assert_cmpstr (f->dns->queries->str, ==, "_dmarc.example.com. _dmarc.com.");
	g_mime_dmarc_result_free (f->result);
	publish (f, "_dmarc.example.com.", "v=DMARC1; p=reject");
#if defined (HAVE_DKIM_GNUTLS) || defined (HAVE_DKIM_OPENSSL)
	publish (f, "key._domainkey.example.com.", "SERVFAIL");
	f->result = g_mime_dmarc_verifier_verify (f->verifier, message, NULL, NULL, &error);
	g_assert_no_error (error);
	expect (f, GMIME_DMARC_STATUS_TEMP_ERROR, GMIME_DMARC_ERRORS_DKIM_TEMPORARY_ERROR);
	g_mime_dmarc_result_free (f->result);
	publish (f, "key._domainkey.example.com.", "v=DKIM1; k=ed25519; p=11qYAYKxCrfVS/7TyWQHOg7hcvPapiMlrwIaaPcHURo=");
#endif
	f->result = g_mime_dmarc_verifier_verify (f->verifier, message, NULL, NULL, &error);
	g_assert_no_error (error);
#if defined (HAVE_DKIM_GNUTLS) || defined (HAVE_DKIM_OPENSSL)
	expect (f, GMIME_DMARC_STATUS_PASS, GMIME_DMARC_ERRORS_NONE);
	/* Keep the same aligned domain but change the signed From field: d=
	 * alone is not evidence that the author authenticated the message. */
	g_mime_entity_set_header (GMIME_ENTITY (message), "From", "other@example.com", NULL);
	g_mime_dmarc_result_free (f->result);
	f->result = g_mime_dmarc_verifier_verify (f->verifier, message, NULL, NULL, &error);
	g_assert_no_error (error);
	expect (f, GMIME_DMARC_STATUS_FAIL, GMIME_DMARC_ERRORS_NONE);
#else
	expect (f, GMIME_DMARC_STATUS_FAIL, GMIME_DMARC_ERRORS_NONE);
#endif
	g_assert_cmpuint (g_mime_dmarc_result_get_dkim_result_count (f->result), ==, 1);
	g_assert_cmpstr (g_mime_dmarc_result_get_dkim_result (f->result, 0)->domain, ==, "example.com");
	g_object_unref (message);
}

int
main (int argc, char **argv)
{
	int status;

	g_test_init (&argc, &argv, NULL);
	g_mime_init ();
	g_test_add ("/dmarc/authors", Fixture, NULL, setup, test_authors, teardown);
	g_test_add ("/dmarc/multi-author", Fixture, NULL, setup, test_multi_author, teardown);
	g_test_add ("/dmarc/tree-walk", Fixture, NULL, setup, test_walk, teardown);
	g_test_add ("/dmarc/alignment", Fixture, NULL, setup, test_alignment, teardown);
	g_test_add ("/dmarc/incomplete", Fixture, NULL, setup, test_incomplete, teardown);
	g_test_add ("/dmarc/policy", Fixture, NULL, setup, test_policy, teardown);
	g_test_add ("/dmarc/np", Fixture, NULL, setup, test_np, teardown);
	g_test_add ("/dmarc/cancel", Fixture, NULL, setup, test_cancel, teardown);
	g_test_add ("/dmarc/actual-dkim", Fixture, NULL, setup, test_actual_dkim, teardown);
	status = g_test_run ();
	g_mime_shutdown ();
	return status;
}
