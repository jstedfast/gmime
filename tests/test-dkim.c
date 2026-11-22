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

#include "gmime/gmime-dkim.h"
#include "testsuite.h"
#include <string.h>

/* RFC 8463 Appendix A. Both published signatures are checked over the
 * complete wire message, not over a hash constructed by the test. */
static const char message[] =
	"DKIM-Signature: v=1; a=ed25519-sha256; c=relaxed/relaxed;\r\n"
	" d=football.example.com; i=@football.example.com;\r\n"
	" q=dns/txt; s=brisbane; t=1528637909; h=from : to :\r\n"
	" subject : date : message-id : from : subject : date;\r\n"
	" bh=2jUSOH9NhtVGCQWNr9BrIAPreKQjO6Sn7XIkfJVOzv8=;\r\n"
	" b=/gCrinpcQOoIfuHNQIbq4pgh9kyIK3AQUdt9OdqQehSwhEIug4D11Bus\r\n"
	" Fa3bT3FY5OsU7ZbnKELq+eXdp1Q1Dw==\r\n"
	"DKIM-Signature: v=1; a=rsa-sha256; c=relaxed/relaxed;\r\n"
	" d=football.example.com; i=@football.example.com;\r\n"
	" q=dns/txt; s=test; t=1528637909; h=from : to : subject :\r\n"
	" date : message-id : from : subject : date;\r\n"
	" bh=2jUSOH9NhtVGCQWNr9BrIAPreKQjO6Sn7XIkfJVOzv8=;\r\n"
	" b=F45dVWDfMbQDGHJFlXUNB2HKfbCeLRyhDXgFpEL8GwpsRe0IeIixNTe3\r\n"
	" DhCVlUrSjV4BwcVcOF6+FF3Zo9Rpo1tFOeS9mPYQTnGdaSGsgeefOsk2Jz\r\n"
	" dA+L10TeYt9BgDfQNZtKdN1WO//KgIqXP7OdEFE4LjFYNcUxZQ4FADY+8=\r\n"
	"From: Joe SixPack <joe@football.example.com>\r\n"
	"To: Suzie Q <suzie@shopping.example.net>\r\n"
	"Subject: Is dinner ready?\r\n"
	"Date: Fri, 11 Jul 2003 21:00:37 -0700 (PDT)\r\n"
	"Message-ID: <20030712040037.46341.5F8J@football.example.com>\r\n\r\n"
	"Hi.\r\n\r\nWe lost the game.  Are you hungry yet?\r\n\r\nJoe.\r\n\r\n";

static const char pem[] =
	"-----BEGIN PRIVATE KEY-----\n"
	"MC4CAQAwBQYDK2VwBCIEIJ1hsZ3v/VpguoRK9JLsLMREScVpezJpGXA7rAMcrn9g\n"
	"-----END PRIVATE KEY-----\n";

typedef struct {
	GResolver parent;
	gboolean strict;
	gboolean invalid_point, include_good_key;
	guint lookups;
} DkimTestResolver;
typedef GResolverClass DkimTestResolverClass;
G_DEFINE_TYPE (DkimTestResolver, dkim_test_resolver, G_TYPE_RESOLVER)

static GList *
lookup (GResolver *resolver, const char *name, GResolverRecordType type, GCancellable *cancel, GError **error)
{
	DkimTestResolver *test = (DkimTestResolver *) resolver;
	const char *txt[3] = { NULL, NULL, NULL };

	test->lookups++;
	g_assert_cmpint (type, ==, G_RESOLVER_RECORD_TXT);
	if (!strcmp (name, "20120113._domainkey.gmail.com.")) {
		/* Archived key from MimeKit's TestData/dkim/gmail.pub. */
		txt[0] = "v=DKIM1; k=rsa; p=MIIBIjANBgkqhkiG9w0BAQEFAAOCAQ8AMIIBCgKCAQEA1Kd87/UeJjenpabgbFwh+"
			"eBCsSTrqmwIYYvywlbhbqoo2DymndFkbjOVIPIldNs/m40KF+yzMn1skyoxcTUGCQs8g3FgD2A"
			"p3ZB5DekAo5wMmk4wimDO+U8QzI3SD07y2+07wlNWwIt8svnxgdxGkVbbhzY8i+RQ9DpSVpPbF"
			"7ykQxtKXkv/ahW3KjViiAH+ghvvIhkx4xYSIc9oSwVmAl5OctMEeWUwg8Istjqz8BZeTWbf41f"
			"bNhte7Y+YqZOwq1Sd0DbvYAD9NOZK9vlfuac0598HY+vtSBczUiKERHv1yRbcaQtZFh5wtiRrN"
			"04BLUTD21MycBX5jYchHjPY/wIDAQAB";
	} else if (!strcmp (name, "brisbane._domainkey.football.example.com.")) {
		GList *records = NULL;
		if (test->invalid_point) {
			txt[0] = "v=DKIM1; k=ed25519; p=11qYAYKxCrfVS/7TyWQHO7PvpcghaiMlrwIaaPcHURo=";
			records = g_list_prepend (NULL, g_variant_ref_sink (g_variant_new ("(^as)", txt)));
			if (!test->include_good_key)
				return records;
		}
		txt[0] = "v=DKIM1; k=ed25519; p=11qYAYKxCrfVS/7TyWQHOg7hcvPapiMlrwIaaPcHURo=";
		txt[1] = test->strict ? "; t=s" : NULL;
		return g_list_append (records, g_variant_ref_sink (g_variant_new ("(^as)", txt)));
	} else {
		g_assert_cmpstr (name, ==, "test._domainkey.football.example.com.");
		txt[0] = "v=DKIM1; k=rsa; p=MIGfMA0GCSqGSIb3DQEBAQUAA4GNADCBiQKBgQDkHlOQoBTzWR"
			"iGs5V6NpP3idY6Wk08a5qhdR6wy5bdOKb2jLQiY/J16JYi0Qvx/byYzCNb3W91y3FutAC"
			"DfzwQ/BC/e/8uBsCR+yz1Lxj+PL6lHvqMKrM3rG4hstT5QjvHO9PzoxZyVYLzBfO2EeC3"
			"Ip3G+2kryOTIKT+l/K4w3QIDAQAB";
	}
	return g_list_prepend (NULL, g_variant_ref_sink (g_variant_new ("(^as)", txt)));
}

static void dkim_test_resolver_init (DkimTestResolver *test) { }
static void dkim_test_resolver_class_init (DkimTestResolverClass *klass) { klass->lookup_records = lookup; }

static gboolean
verify (const char *text, guint index, GResolver *resolver)
{
	GMimeMessage *parsed = testsuite_parse_message (text, strlen (text));
	GError *error = NULL;
	gboolean ok = g_mime_dkim_verify (parsed, index, resolver, NULL, &error);
	if (!ok) {
		g_assert_nonnull (error);
		g_test_message ("%s", error->message);
	}
	g_clear_error (&error);
	g_object_unref (parsed);
	return ok;
}

static char *
replace (const char *text, const char *search, const char *with)
{
	const char *p = strstr (text, search);
	char *prefix = NULL;
	g_assert_nonnull (p);
	prefix = g_strndup (text, p - text);
	char *result = g_strconcat (prefix, with, p + strlen (search), NULL);
	g_free (prefix);
	return result;
}

static void
test_rfc (void)
{
	GResolver *resolver = g_object_new (dkim_test_resolver_get_type (), NULL);
	char *fold = replace (message, "Subject: Is dinner", "SuBJECT \t:\tIs\r\n\t dinner");
	char *body_wsp = replace (fold, "game.  Are", "game.\t Are");
	char *body_change = replace (message, "game.", "game!");
	char *header_change = replace (message, "dinner", "lunch");
	char *insertion = replace (message, "To: Suzie", "Subject: Inserted\r\nTo: Suzie");
	char *bad_ed = replace (message, "/gCr", "AgCr");

	for (guint i = 0; i < 2; i++) {
		g_assert_true (verify (message, i, resolver));
		g_assert_true (verify (body_wsp, i, resolver));
		g_assert_false (verify (body_change, i, resolver));
		g_assert_false (verify (header_change, i, resolver));
		g_assert_false (verify (insertion, i, resolver));
	}
	g_assert_false (verify (bad_ed, 0, resolver));
	g_assert_true (verify (bad_ed, 1, resolver));
	g_assert_false (verify (message, 2, resolver));
	g_free (bad_ed);
	g_free (insertion);
	g_free (header_change);
	g_free (body_change);
	g_free (body_wsp);
	g_free (fold);
	g_object_unref (resolver);
}

static void
test_prefix (void)
{
	/* Independently computed Ed25519 signature over simple/simple bytes.
	 * Folding inside b= disappears, but the semicolon and extension after
	 * it stay in the hash. l=3 ends in the middle of a body line. */
	static const char vector[] =
		"DKIM-Signature: v=1; a=ed25519-sha256; d=football.example.com;\r\n"
		" s=brisbane; h=from:x-slot:x-slot; l=3; i=me@sub.football.example.com;\r\n"
		" bh=tdQEXD9Gb6kf4sxqvnkjKhpXzfEE96JucW4KHieJ33g=; b= \r\n"
		" cNX0mKy2fL8RZDU1qfF2QPOnoPH8FWTjaywk5+MATZn1b7cRdI7D6vNdvzg+A1pHQJbXPJuf581iSWb2AVpyBg==\t; ext=test\r\n"
		"X-Slot: first\r\nFrom: A\r\nX-Slot: second\r\n\r\nABC some unsigned tail";
	GResolver *resolver = g_object_new (dkim_test_resolver_get_type (), NULL);
	char *prefix = replace (vector, "ABC", "XBC");
	char *tail = replace (vector, "some unsigned tail", "changed");
	char *ext = replace (vector, "ext=test", "ext=TEST");
	char *slot = replace (vector, "first", "First");
	g_assert_true (verify (vector, 0, resolver));
	g_assert_true (verify (tail, 0, resolver));
	g_assert_false (verify (prefix, 0, resolver));
	g_assert_false (verify (ext, 0, resolver));
	g_assert_false (verify (slot, 0, resolver));
	((DkimTestResolver *) resolver)->strict = TRUE;
	g_assert_false (verify (vector, 0, resolver));
	g_free (slot);
	g_free (ext);
	g_free (tail);
	g_free (prefix);
	g_object_unref (resolver);
}

static void
test_malformed (void)
{
	static const char * const changes[][2] = {
		{ "v=1", "v=2" }, { "v=1", "V=1" }, { "v=1", "v=1; v=1" },
		{ "v=1", "v=1;; x=5" }, { "v=1", "v=1; 0=abc" },
		{ "v=1", "v=1; tag=x; tag=y" }, { "v=1", "v=1; x=1528637909" },
		{ "v=1", "v=1; x=1528637910" }, { "v=1", "v=1; l=999" },
		{ "v=1", "v=1; l=-1" }, { "v=1", "v=1; l=18446744073709551616" },
		{ "i=@football.example.com", "i=@notfootball.example.com" },
		{ "d=football", "d=-x.football" }, { "s=brisbane", "s=." },
		{ "q=dns/txt", "q=other" }, { "ed25519-sha256", "rsa-sha1" },
		{ "h=from : to :\r\n subject : date : message-id : from : subject : date", "h=subject" },
		{ "From: Joe", "X-From: Joe" }, { "From: Joe", "From: Other\r\nFrom: Joe" },
		{ "b=/gCr", "b=?gCr" }, { "bh=2jUS", "bh=2 US" },
	};
	GResolver *resolver = g_object_new (dkim_test_resolver_get_type (), NULL);
	for (guint i = 0; i < G_N_ELEMENTS (changes); i++) {
		char *s = replace (message, changes[i][0], changes[i][1]);
		g_assert_false (verify (s, 0, resolver));
		g_free (s);
	}
	g_assert_cmpuint (((DkimTestResolver *) resolver)->lookups, ==, 0);
	g_object_unref (resolver);
}

static void
test_sign (void)
{
	static const char binary[] = "From: Foo <a@example.com>\r\nX-Test: a\r\n\tb\r\n\r\n"
		"\r\n \tdata\t \0\xff  \r\n \t\r\n";
	static const char * const headers[] = { "From", "X-Test", "X-Test", NULL };
	static const char * const no_from[] = { "To", NULL };
	GResolver *resolver = g_object_new (dkim_test_resolver_get_type (), NULL);
	GBytes *key = g_bytes_new_static (pem, sizeof pem - 1);
	GMimeMessage *input = testsuite_parse_message (binary, sizeof binary - 1);
	GError *error = NULL;
	for (int hc = 0; hc <= 1; hc++) {
		for (int bc = 0; bc <= 1; bc++) {
			GBytes *output;
			GMimeMessage *parsed;
			gsize len;
			const char *data;
			g_assert_true (g_mime_dkim_sign (input, "football.example.com", "brisbane",
				GMIME_DKIM_ALGORITHM_ED25519_SHA256, key, hc, bc, headers, &error));
			g_assert_no_error (error);
			output = testsuite_serialize_message (input);
			data = g_bytes_get_data (output, &len);
			g_assert_cmpuint (len, >, sizeof binary - 1);
			g_assert_cmpmem (data + len - sizeof binary + 1, sizeof binary - 1, binary, sizeof binary - 1);
			parsed = testsuite_parse_message (data, len);
			g_assert_true (g_mime_dkim_verify (parsed, 0, resolver, NULL, &error));
			g_assert_no_error (error);
			g_object_unref (parsed);
			g_bytes_unref (output);
		}
	}
	g_assert_false (g_mime_dkim_sign (input, "football.example.com", "brisbane",
		GMIME_DKIM_ALGORITHM_ED25519_SHA256, key, 0, 0, no_from, &error));
	g_assert_error (error, G_IO_ERROR, G_IO_ERROR_INVALID_DATA);
	g_clear_error (&error);
	g_object_unref (input);
	g_bytes_unref (key);
	g_object_unref (resolver);
}

static void
test_constructed (void)
{
	GMimeMessage *msg = g_mime_message_new (TRUE);
	GMimeTextPart *part = g_mime_text_part_new ();
	GResolver *resolver = g_object_new (dkim_test_resolver_get_type (), NULL);
	GBytes *key = g_bytes_new_static (pem, sizeof pem - 1);
	GError *error = NULL;
	GBytes *wire;
	GMimeMessage *parsed;

	g_assert_true (g_mime_message_add_mailbox (msg, GMIME_ADDRESS_TYPE_FROM, "Joe", "joe@example.com", &error));
	g_mime_text_part_set_text (part, "First line\nSecond line.\n");
	g_mime_message_set_mime_part (msg, GMIME_ENTITY (part));
	g_assert_true (g_mime_dkim_sign (msg, "football.example.com", "brisbane", GMIME_DKIM_ALGORITHM_ED25519_SHA256,
					key, GMIME_DKIM_CANONICALIZATION_SIMPLE, GMIME_DKIM_CANONICALIZATION_SIMPLE, NULL, &error));
	wire = testsuite_serialize_message (msg);
	parsed = testsuite_parse_message (g_bytes_get_data (wire, NULL), g_bytes_get_size (wire));
	g_assert_true (g_mime_dkim_verify (parsed, 0, resolver, NULL, &error));
	g_mime_entity_set_content_type_parameter (g_mime_message_get_mime_part (parsed), "charset", "iso-8859-1");
	g_assert_false (g_mime_dkim_verify (parsed, 0, resolver, NULL, &error));
	g_assert_nonnull (error);
	g_clear_error (&error);
	g_object_unref (parsed);
	g_bytes_unref (wire);
	g_bytes_unref (key);
	g_object_unref (resolver);
	g_object_unref (part);
	g_object_unref (msg);
}

static void
test_resolver_edges (void)
{
	DkimTestResolver *resolver = g_object_new (dkim_test_resolver_get_type (), NULL);
	GMimeMessage *parsed = testsuite_parse_message (message, strlen (message));
	GCancellable *cancel = g_cancellable_new ();
	GError *error = NULL;

	g_cancellable_cancel (cancel);
	g_assert_false (g_mime_dkim_verify (parsed, 0, G_RESOLVER (resolver), cancel, &error));
	g_assert_error (error, G_IO_ERROR, G_IO_ERROR_CANCELLED);
	g_clear_error (&error);
	g_assert_cmpuint (resolver->lookups, ==, 0);

	resolver->invalid_point = TRUE;
	g_assert_false (g_mime_dkim_verify (parsed, 0, G_RESOLVER (resolver), NULL, &error));
	g_assert_nonnull (error);
	g_clear_error (&error);
	/* GMime rejects multiple syntactically valid key records rather than
	 * selecting one by trial verification. */
	resolver->include_good_key = TRUE;
	g_assert_false (g_mime_dkim_verify (parsed, 0, G_RESOLVER (resolver), NULL, &error));
	g_assert_error (error, G_IO_ERROR, G_IO_ERROR_INVALID_DATA);
	g_clear_error (&error);
	g_object_unref (cancel);
	g_object_unref (parsed);
	g_object_unref (resolver);
}

static void
test_google_message (gconstpointer path)
{
	GResolver *resolver = g_object_new (dkim_test_resolver_get_type (), NULL);
	GError *error = NULL;
	GMimeMessage *parsed = g_mime_message_load_from_file (NULL, path, &error);
	g_assert_no_error (error);
	g_assert_nonnull (parsed);
	gboolean verified = g_mime_dkim_verify (parsed, 0, resolver, NULL, &error);
	g_assert_no_error (error);
	g_assert_true (verified);
	g_assert_cmpuint (((DkimTestResolver *) resolver)->lookups, ==, 1);
	g_object_unref (parsed);
	g_object_unref (resolver);
}

static void
test_many_headers (void)
{
	/* Independently signed over "from:a\r\n", then x-test:<number>\r\n
	 * in descending order. Missing and oversigned occurrences add no bytes. */
	static const struct { guint count; const char *sig; } vectors[] = {
		{ 0, "fqO70CnVMBYE7YEAeDNkA38EBymFjDCCRosBeJWmWGpv+QDlbnnJL5YjBejrSygJlwEAWhXsboPnS/DTVR/TCA==" },
		{ 1000, "XoRnkeotdIwi6nq69hAvNufauYaAobUjjTHP7rjmJOFzMXvtXnB3e4XhrxOvhxpBbNsM0UfazgiLYfH54ZAUDA==" },
		{ 2000, "UdmM4ryt8+4YDWK5daLuBTsxFjuC6Bnd8zaMKfDZ5x06nMvKM7G700gHnK/rhvYDCR0adHz7wIliRuapKoJjCA==" },
		{ 4000, "rbxu5c9VgafleyKvGCTCCbUzrT5P1LBr97xk8urqM++eBmdXaB2dbWVUAYkTpqOKdDQeGW3gT678yc3iY5KiCg==" },
		{ 8000, "L3/xeVssoS8Og/S+2l5v6Qdpz7wVeClPljOz2sku7D0/SHS7mVPms6gBlN/CzZsWv2fGKzmqwr6u5aeg6fn4BQ==" },
		{ 16000, "rYfYEqi5NVNO/+jc037RiGRUwU2ugb/LBL37ScAg+s/hFLLg23WXsMR7SIYZm2rQJEQM0INjva4SZm4JoOJDDw==" }
	};
	GResolver *resolver = g_object_new (dkim_test_resolver_get_type (), NULL);
	for (guint n = 0; n < G_N_ELEMENTS (vectors); n++) {
		GString *msg = g_string_new ("From: a\r\n");
		GString *sig = g_string_new (" v=1; a=ed25519-sha256; c=relaxed/relaxed; d=football.example.com;"
					     " s=brisbane; t=2147483648; x=999999999999; h=from");
		GMimeHeader *header = g_mime_header_new ("DKIM-Signature", "", NULL);
		GError *error = NULL;
		for (guint i = 0; i < vectors[n].count; i++) {
			g_string_append_printf (msg, "X-Test: %u\r\n", i);
			g_string_append (sig, ":x-test:x-missing");
		}
		if (vectors[n].count)
			g_string_append (sig, ":x-test");
		g_string_append_printf (sig, "; bh=47DEQpj8HBSa+/TImW+5JCeuQeRkm5NMpJWZG3hSuFU=; b=%s\r\n", vectors[n].sig);
		g_string_append (msg, "\r\n");
		GMimeMessage *parsed = testsuite_parse_message (msg->str, msg->len);
		GBytes *raw = g_string_free_to_bytes (sig);
		g_assert_true (g_mime_header_set_raw_value (header, raw, &error));
		g_mime_header_list_insert_header (g_mime_entity_get_header_list (GMIME_ENTITY (parsed)), 0, header);
		gboolean valid = g_mime_dkim_verify (parsed, 0, resolver, NULL, &error);
		g_assert_no_error (error);
		g_assert_true (valid);
		g_bytes_unref (raw);
		g_object_unref (header);
		g_object_unref (parsed);
		g_string_free (msg, TRUE);
	}
	g_object_unref (resolver);
}

static void
test_empty (void)
{
	/* RFC 6376 §3.4.3/4: an empty body hashes one CRLF under simple
	 * canonicalization, and zero bytes under relaxed canonicalization. */
	static const char *const hashes[] = {
		"bh=frcCV1k9oG9oKj3dpUqdJg1PxRT2RSN/XKdLCPjaYaY=",
		"bh=47DEQpj8HBSa+/TImW+5JCeuQeRkm5NMpJWZG3hSuFU="
	};
	static const char empty[] = "From: Joe\r\n";
	GBytes *key = g_bytes_new_static (pem, sizeof pem - 1);
	GMimeMessage *input = testsuite_parse_message (empty, sizeof empty - 1);
	GError *error = NULL;
	for (int bc = 0; bc <= 1; bc++) {
		const char *value;
		g_assert_true (g_mime_dkim_sign (input, "football.example.com", "brisbane",
			GMIME_DKIM_ALGORITHM_ED25519_SHA256, key, 0, bc, NULL, &error));
		g_assert_no_error (error);
		value = g_mime_entity_get_header (GMIME_ENTITY (input), "DKIM-Signature");
		g_assert_nonnull (strstr (value, hashes[bc]));
	}
	g_object_unref (input);
	g_bytes_unref (key);
}

int
main (int argc, char **argv)
{
	g_test_init (&argc, &argv, NULL);
	g_mime_init ();
	const char *data_dir = argc > 1 ? argv[argc - 1] : "data/dkim";
	char *parent = g_path_get_dirname (data_dir);
	static const char *const files[] = { "gmail.msg", "related.msg", "multipart-no-end-boundary.msg" };
	for (guint i = 0; i < G_N_ELEMENTS (files); i++) {
		char *name = g_strdup_printf ("/dkim/google/%s", files[i]);
		char *path = g_build_filename (parent, "roundtrip", "dkim", files[i], NULL);
		g_test_add_data_func_full (name, path, test_google_message, g_free);
		g_free (name);
	}
	g_free (parent);
	g_test_add_func ("/dkim/resolver-edges", test_resolver_edges);
	g_test_add_func ("/dkim/many-headers", test_many_headers);
	g_test_add_func ("/dkim/rfc8463", test_rfc);
	g_test_add_func ("/dkim/prefix", test_prefix);
	g_test_add_func ("/dkim/malformed", test_malformed);
	g_test_add_func ("/dkim/sign", test_sign);
	g_test_add_func ("/dkim/constructed", test_constructed);
	g_test_add_func ("/dkim/empty-body", test_empty);
	int result = g_test_run ();
	g_mime_shutdown ();
	return result;
}
