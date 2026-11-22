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

#include "gmime/gmime-dmarc-record.h"
#include <gio/gio.h>

#define ERR(name) GMIME_DMARC_RECORD_ERRORS_##name
#define FO(name) GMIME_DMARC_FAILURE_REPORTING_OPTIONS_##name

static GMimeDmarcRecord *
parse (const char *text, GMimeDmarcRecordErrors expected)
{
	GError *error = NULL;
	GMimeDmarcRecord *record = g_mime_dmarc_record_parse (text, &error);

	g_assert_no_error (error);
	g_assert_nonnull (record);
	g_assert_cmpuint (g_mime_dmarc_record_get_errors (record), ==, expected);
	return record;
}

static void
assert_defaults (const GMimeDmarcRecord *record)
{
	g_assert_cmpint (g_mime_dmarc_record_get_policy (record), ==, GMIME_DMARC_POLICY_UNSPECIFIED);
	g_assert_cmpint (g_mime_dmarc_record_get_subdomain_policy (record), ==, GMIME_DMARC_POLICY_UNSPECIFIED);
	g_assert_cmpint (g_mime_dmarc_record_get_non_existent_subdomain_policy (record), ==, GMIME_DMARC_POLICY_UNSPECIFIED);
	g_assert_cmpint (g_mime_dmarc_record_get_public_suffix_domain (record), ==, GMIME_DMARC_PUBLIC_SUFFIX_DOMAIN_UNSPECIFIED);
	g_assert_false (g_mime_dmarc_record_get_testing (record));
	g_assert_cmpint (g_mime_dmarc_record_get_dkim_alignment (record), ==, GMIME_DMARC_ALIGNMENT_MODE_RELAXED);
	g_assert_cmpint (g_mime_dmarc_record_get_spf_alignment (record), ==, GMIME_DMARC_ALIGNMENT_MODE_RELAXED);
	g_assert_cmpuint (g_mime_dmarc_record_get_aggregate_report_uris (record)->len, ==, 0);
	g_assert_cmpuint (g_mime_dmarc_record_get_failure_report_uris (record)->len, ==, 0);
	g_assert_cmpuint (g_mime_dmarc_record_get_failure_reporting_options (record), ==, FO (ALL_FAIL));
}

static void
test_version (void)
{
	static const char *const minimal[] = {
		"v=DMARC1", "v=DMARC1;", "v=DMARC1 ;", "v = DMARC1", "V=DMARC1", "  v=DMARC1  ", "v=\tDMARC1\t;\t"
	};
	static const char *const invalid[] = {
		NULL, "", "   ", "p=none; v=DMARC1", "v=dmarc1; p=none", "v=DMARC2; p=none", "v=DMARC; p=none",
		"v=DMARC10; p=none", "v=DMARC1 p=none", "v DMARC1; p=none", "v=; p=none", "v", "version=DMARC1; p=none", "v=spf1 -all"
	};
	guint i;

	for (i = 0; i < G_N_ELEMENTS (minimal); i++) {
		GMimeDmarcRecord *record = parse (minimal[i], 0);
		assert_defaults (record);
		g_mime_dmarc_record_free (record);
	}
	for (i = 0; i < G_N_ELEMENTS (invalid); i++) {
		GError *error = NULL;
		int code = invalid[i] ? G_IO_ERROR_INVALID_DATA : G_IO_ERROR_INVALID_ARGUMENT;
		g_assert_null (g_mime_dmarc_record_parse (invalid[i], &error));
		g_assert_error (error, G_IO_ERROR, code);
		g_clear_error (&error);
	}
}

static void
test_full (void)
{
	GMimeDmarcRecord *record = parse ("v=DMARC1 ;\tP = reject\t; SP=quarantine; np=none; psd=n; t=y; ADKIM= s ; aspf=s; "
					  "RUA =  mailto:dmarc@example.com ,\tmailto:reports@example.net  ; ruf=mailto:failures@example.com; fo=1:d:s;", 0);
	GMimeDmarcRecord *copy = g_mime_dmarc_record_copy (record);
	const GPtrArray *rua, *ruf;

	g_mime_dmarc_record_free (record);
	g_assert_cmpint (g_mime_dmarc_record_get_policy (copy), ==, GMIME_DMARC_POLICY_REJECT);
	g_assert_cmpint (g_mime_dmarc_record_get_subdomain_policy (copy), ==, GMIME_DMARC_POLICY_QUARANTINE);
	g_assert_cmpint (g_mime_dmarc_record_get_non_existent_subdomain_policy (copy), ==, GMIME_DMARC_POLICY_NONE);
	g_assert_cmpint (g_mime_dmarc_record_get_public_suffix_domain (copy), ==, GMIME_DMARC_PUBLIC_SUFFIX_DOMAIN_NO);
	g_assert_true (g_mime_dmarc_record_get_testing (copy));
	g_assert_cmpint (g_mime_dmarc_record_get_dkim_alignment (copy), ==, GMIME_DMARC_ALIGNMENT_MODE_STRICT);
	g_assert_cmpint (g_mime_dmarc_record_get_spf_alignment (copy), ==, GMIME_DMARC_ALIGNMENT_MODE_STRICT);
	g_assert_cmpuint (g_mime_dmarc_record_get_failure_reporting_options (copy), ==, FO (ANY_FAIL) | FO (DKIM) | FO (SPF));
	rua = g_mime_dmarc_record_get_aggregate_report_uris (copy);
	ruf = g_mime_dmarc_record_get_failure_report_uris (copy);
	g_assert_cmpuint (rua->len, ==, 2);
	g_assert_cmpstr (rua->pdata[0], ==, "mailto:dmarc@example.com");
	g_assert_cmpstr (rua->pdata[1], ==, "mailto:reports@example.net");
	g_assert_cmpuint (ruf->len, ==, 1);
	g_assert_cmpstr (ruf->pdata[0], ==, "mailto:failures@example.com");
	g_mime_dmarc_record_free (copy);
}

static void
test_policies (void)
{
	static const struct { const char *value; GMimeDmarcPolicy policy; } cases[] = {
		{ "none", GMIME_DMARC_POLICY_NONE }, { "quarantine", GMIME_DMARC_POLICY_QUARANTINE },
		{ "reject", GMIME_DMARC_POLICY_REJECT }, { "REJECT", GMIME_DMARC_POLICY_REJECT }, { "Quarantine", GMIME_DMARC_POLICY_QUARANTINE },
		{ "", GMIME_DMARC_POLICY_UNSPECIFIED }, { "bogus", GMIME_DMARC_POLICY_UNSPECIFIED }, { "rejected", GMIME_DMARC_POLICY_UNSPECIFIED },
		{ "none reject", GMIME_DMARC_POLICY_UNSPECIFIED }
	};
	const guint errors = ERR (INVALID_POLICY) | ERR (INVALID_SUBDOMAIN_POLICY) | ERR (INVALID_NON_EXISTENT_SUBDOMAIN_POLICY);
	guint i;

	for (i = 0; i < G_N_ELEMENTS (cases); i++) {
		const char *v = cases[i].value;
		char *text = g_strdup_printf ("v=DMARC1; p=%s; sp=%s; np=%s", v, v, v);
		GMimeDmarcRecord *record = parse (text, cases[i].policy == GMIME_DMARC_POLICY_UNSPECIFIED ? errors : 0);
		g_assert_cmpint (g_mime_dmarc_record_get_policy (record), ==, cases[i].policy);
		g_assert_cmpint (g_mime_dmarc_record_get_subdomain_policy (record), ==, cases[i].policy);
		g_assert_cmpint (g_mime_dmarc_record_get_non_existent_subdomain_policy (record), ==, cases[i].policy);
		g_mime_dmarc_record_free (record);
		g_free (text);
	}
}

static void
test_simple_tags (void)
{
	static const struct {
		const char *tags;
		GMimeDmarcPublicSuffixDomain psd;
		gboolean testing;
		GMimeDmarcAlignmentMode alignment;
		guint errors;
	} cases[] = {
		{ "psd=y; t=y; adkim=s; aspf=s", GMIME_DMARC_PUBLIC_SUFFIX_DOMAIN_YES, TRUE, GMIME_DMARC_ALIGNMENT_MODE_STRICT, 0 },
		{ "psd=n; t=n; adkim=r; aspf=r", GMIME_DMARC_PUBLIC_SUFFIX_DOMAIN_NO, FALSE, GMIME_DMARC_ALIGNMENT_MODE_RELAXED, 0 },
		{ "psd=Y; t=Y; adkim=S; aspf=S", GMIME_DMARC_PUBLIC_SUFFIX_DOMAIN_YES, TRUE, GMIME_DMARC_ALIGNMENT_MODE_STRICT, 0 },
		{ "psd=u", GMIME_DMARC_PUBLIC_SUFFIX_DOMAIN_UNSPECIFIED, FALSE, GMIME_DMARC_ALIGNMENT_MODE_RELAXED, 0 },
		{ "psd=x; t=yes; adkim=strict; aspf=", GMIME_DMARC_PUBLIC_SUFFIX_DOMAIN_UNSPECIFIED, FALSE, GMIME_DMARC_ALIGNMENT_MODE_RELAXED,
		  ERR (INVALID_PUBLIC_SUFFIX_DOMAIN) | ERR (INVALID_TESTING) | ERR (INVALID_DKIM_ALIGNMENT) | ERR (INVALID_SPF_ALIGNMENT) }
	};
	guint i;

	for (i = 0; i < G_N_ELEMENTS (cases); i++) {
		char *text = g_strdup_printf ("v=DMARC1; %s; p=none", cases[i].tags);
		GMimeDmarcRecord *record = parse (text, cases[i].errors);
		g_assert_cmpint (g_mime_dmarc_record_get_public_suffix_domain (record), ==, cases[i].psd);
		g_assert_cmpint (g_mime_dmarc_record_get_testing (record), ==, cases[i].testing);
		g_assert_cmpint (g_mime_dmarc_record_get_dkim_alignment (record), ==, cases[i].alignment);
		g_assert_cmpint (g_mime_dmarc_record_get_spf_alignment (record), ==, cases[i].alignment);
		g_assert_cmpint (g_mime_dmarc_record_get_policy (record), ==, GMIME_DMARC_POLICY_NONE);
		g_mime_dmarc_record_free (record);
		g_free (text);
	}
}

static void
test_discarded_tags (void)
{
	static const char *const malformed[] = { "p", "=none", "p1=none", "p_x=none", "p x=none", "garbage" };
	GMimeDmarcRecord *record;
	guint i;

	record = parse ("v=DMARC1;; ; pct=50; rf=afrf; ri=86400; foo=bar; x=; p=reject;;", 0);
	g_assert_cmpint (g_mime_dmarc_record_get_policy (record), ==, GMIME_DMARC_POLICY_REJECT);
	g_mime_dmarc_record_free (record);
	for (i = 0; i < G_N_ELEMENTS (malformed); i++) {
		char *text = g_strdup_printf ("v=DMARC1; %s; sp=reject", malformed[i]);
		record = parse (text, ERR (MALFORMED_TAG));
		g_assert_cmpint (g_mime_dmarc_record_get_policy (record), ==, GMIME_DMARC_POLICY_UNSPECIFIED);
		g_assert_cmpint (g_mime_dmarc_record_get_subdomain_policy (record), ==, GMIME_DMARC_POLICY_REJECT);
		g_mime_dmarc_record_free (record);
		g_free (text);
	}
	record = parse ("v=DMARC1; p=reject; p=none; adkim=s; adkim=r; v=DMARC1", ERR (DUPLICATE_TAG));
	g_assert_cmpint (g_mime_dmarc_record_get_policy (record), ==, GMIME_DMARC_POLICY_REJECT);
	g_assert_cmpint (g_mime_dmarc_record_get_dkim_alignment (record), ==, GMIME_DMARC_ALIGNMENT_MODE_STRICT);
	g_mime_dmarc_record_free (record);

	/* The first occurrence wins, even when it is invalid. */
	record = parse ("v=DMARC1; p=bogus; p=reject", ERR (INVALID_POLICY) | ERR (DUPLICATE_TAG));
	assert_defaults (record);
	g_mime_dmarc_record_free (record);
}

static void
test_report_uris (void)
{
	static const struct { const char *value; guint count; } invalid[] = {
		{ "", 0 }, { ",", 0 }, { "dmarc@example.com", 0 }, { "/var/dmarc", 0 }, { "1mailto:a@example.com", 0 },
		{ "mailto:a@example.com,", 1 }, { "mailto:a@example.com,,mailto:b@example.com", 2 },
		{ "mailto:a@example.com, not a uri", 1 }, { "mailto:a b@example.com", 0 },
		{ "mailto:dmarc@ex\xc3\xa4mple.com, mailto:b@example.com", 1 }, { "!10m", 0 },
		{ "mailto:a@", 0 }, { "mailto:a@@b", 0 }, { "mailto:a@bad..domain", 0 },
		{ "mailto:a@example.com:80000", 0 }
	};
	GMimeDmarcRecord *record;
	const GPtrArray *rua, *ruf;
	guint i;

	record = parse ("v=DMARC1; rua=mailto:a@example.com!10m, https://example.com/dmarc!5 , mailto:b@example.com; ruf=mailto:c@example.com!1g", 0);
	rua = g_mime_dmarc_record_get_aggregate_report_uris (record);
	ruf = g_mime_dmarc_record_get_failure_report_uris (record);
	g_assert_cmpuint (rua->len, ==, 3);
	g_assert_cmpstr (rua->pdata[0], ==, "mailto:a@example.com");
	g_assert_cmpstr (rua->pdata[1], ==, "https://example.com/dmarc");
	g_assert_cmpstr (rua->pdata[2], ==, "mailto:b@example.com");
	g_assert_cmpuint (ruf->len, ==, 1);
	g_assert_cmpstr (ruf->pdata[0], ==, "mailto:c@example.com");
	g_mime_dmarc_record_free (record);

	record = parse ("v=DMARC1; rua=mailto:a%2Cb%21c@example.com", 0);
	rua = g_mime_dmarc_record_get_aggregate_report_uris (record);
	g_assert_cmpuint (rua->len, ==, 1);
	g_assert_cmpstr (rua->pdata[0], ==, "mailto:a%2Cb%21c@example.com");
	g_mime_dmarc_record_free (record);

	for (i = 0; i < G_N_ELEMENTS (invalid); i++) {
		char *text = g_strdup_printf ("v=DMARC1; p=none; rua=%s", invalid[i].value);
		record = parse (text, ERR (INVALID_AGGREGATE_REPORT_URI));
		g_assert_cmpuint (g_mime_dmarc_record_get_aggregate_report_uris (record)->len, ==, invalid[i].count);
		g_assert_cmpint (g_mime_dmarc_record_get_policy (record), ==, GMIME_DMARC_POLICY_NONE);
		g_mime_dmarc_record_free (record);
		g_free (text);
	}
	record = parse ("v=DMARC1; ruf=bogus, mailto:a@example.com", ERR (INVALID_FAILURE_REPORT_URI));
	ruf = g_mime_dmarc_record_get_failure_report_uris (record);
	g_assert_cmpuint (ruf->len, ==, 1);
	g_assert_cmpstr (ruf->pdata[0], ==, "mailto:a@example.com");
	g_mime_dmarc_record_free (record);

	/* The verifier, not the parser, substitutes p=none when there is a valid rua. */
	record = parse ("v=DMARC1; p=bogus; rua=mailto:a@example.com", ERR (INVALID_POLICY));
	g_assert_cmpint (g_mime_dmarc_record_get_policy (record), ==, GMIME_DMARC_POLICY_UNSPECIFIED);
	g_assert_cmpuint (g_mime_dmarc_record_get_aggregate_report_uris (record)->len, ==, 1);
	g_mime_dmarc_record_free (record);
}

static void
test_failure_options (void)
{
	static const struct { const char *value; guint flags; } cases[] = {
		{ "0", FO (ALL_FAIL) }, { "1", FO (ANY_FAIL) }, { "d", FO (DKIM) }, { "s", FO (SPF) },
		{ "D:S", FO (DKIM) | FO (SPF) }, { "0:d", FO (ALL_FAIL) | FO (DKIM) }, { "d:0:s", FO (ALL_FAIL) | FO (DKIM) | FO (SPF) },
		{ "s:d:1", FO (ANY_FAIL) | FO (DKIM) | FO (SPF) }, { "1 : d", FO (ANY_FAIL) | FO (DKIM) },
		{ "", 0 }, { "   ", 0 }, { "0:1", 0 }, { "1:d:0", 0 }, { "d:d", 0 }, { "0:0", 0 },
		{ "2", 0 }, { "ds", 0 }, { "d:", 0 }, { ":d", 0 }, { "d::s", 0 }, { "x", 0 }
	};
	guint i;
	GMimeDmarcRecord *record;

	for (i = 0; i < G_N_ELEMENTS (cases); i++) {
		char *text = g_strdup_printf ("v=DMARC1; fo=%s; ruf=mailto:a@example.com", cases[i].value);
		guint expected = cases[i].flags ? cases[i].flags : FO (ALL_FAIL);
		record = parse (text, cases[i].flags ? 0 : ERR (INVALID_FAILURE_REPORTING_OPTIONS));
		g_assert_cmpuint (g_mime_dmarc_record_get_failure_reporting_options (record), ==, expected);
		g_mime_dmarc_record_free (record);
		g_free (text);
	}

	record = parse ("v=DMARC1; p=none; fo=1", 0);
	g_assert_cmpuint (g_mime_dmarc_record_get_failure_reporting_options (record), ==, FO (ALL_FAIL));
	g_mime_dmarc_record_free (record);
	record = parse ("v=DMARC1; p=none; fo=0:1", 0);
	g_assert_cmpuint (g_mime_dmarc_record_get_failure_reporting_options (record), ==, FO (ALL_FAIL));
	g_mime_dmarc_record_free (record);
	record = parse ("v=DMARC1; fo=s; ruf=", ERR (INVALID_FAILURE_REPORT_URI));
	g_assert_cmpuint (g_mime_dmarc_record_get_failure_reporting_options (record), ==, FO (SPF));
	g_mime_dmarc_record_free (record);
}

static void
test_large_record (void)
{
	GString *text = g_string_new ("v=DMARC1; p=reject");
	GMimeDmarcRecord *record;
	guint i;

	for (i = 0; i < 10000; i++)
		g_string_append (text, "; x=y");
	g_string_append (text, "; rua=mailto:a@example.com");
	for (i = 0; i < 10000; i++)
		g_string_append (text, ",mailto:a@example.com");
	record = parse (text->str, 0);
	g_assert_cmpint (g_mime_dmarc_record_get_policy (record), ==, GMIME_DMARC_POLICY_REJECT);
	g_assert_cmpuint (g_mime_dmarc_record_get_aggregate_report_uris (record)->len, ==, 10001);
	g_mime_dmarc_record_free (record);
	g_string_free (text, TRUE);
}

int
main (int argc, char **argv)
{
	g_test_init (&argc, &argv, NULL);
	g_test_add_func ("/dmarc/record/version", test_version);
	g_test_add_func ("/dmarc/record/full", test_full);
	g_test_add_func ("/dmarc/record/policies", test_policies);
	g_test_add_func ("/dmarc/record/simple-tags", test_simple_tags);
	g_test_add_func ("/dmarc/record/discarded-tags", test_discarded_tags);
	g_test_add_func ("/dmarc/record/report-uris", test_report_uris);
	g_test_add_func ("/dmarc/record/failure-options", test_failure_options);
	g_test_add_func ("/dmarc/record/large", test_large_record);
	return g_test_run ();
}
