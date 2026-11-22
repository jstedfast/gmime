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

#include "gmime/gmime-dkim-backend-private.h"

typedef struct {
	GResolver parent;
	int scenario;
} TestResolver;
typedef GResolverClass TestResolverClass;
G_DEFINE_TYPE (TestResolver, test_resolver, G_TYPE_RESOLVER)

static GList *
lookup (GResolver *resolver, const char *name, GResolverRecordType type, GCancellable *cancellable, GError **error)
{
	static const char *const good[] = { "v=DKIM1; p=YW", "Jj; t=s", NULL };
	static const char *const empty[] = { "v=DKIM1; p=", NULL };
	static const char *const junk[] = { "unrelated text", NULL };
	GList *records = NULL;
	int scenario = ((TestResolver *) resolver)->scenario;

	g_assert_cmpstr (name, ==, "sel._domainkey.example.org.");
	g_assert_cmpint (type, ==, G_RESOLVER_RECORD_TXT);
	if (scenario == 3) {
		g_set_error_literal (error, G_RESOLVER_ERROR, G_RESOLVER_ERROR_TEMPORARY_FAILURE, "retry");
		return NULL;
	}
	records = g_list_prepend (records, g_variant_ref_sink (g_variant_new ("(^as)", junk)));
	if (scenario != 4)
		records = g_list_prepend (records, g_variant_ref_sink (g_variant_new ("(^as)", scenario == 2 ? empty : good)));
	if (scenario == 1)
		records = g_list_prepend (records, g_variant_ref_sink (g_variant_new ("(^as)", good)));
	return records;
}

static void test_resolver_init (TestResolver *resolver) { }
static void test_resolver_class_init (TestResolverClass *klass) { klass->lookup_records = lookup; }

static void
test_lookup (void)
{
	TestResolver *resolver = g_object_new (test_resolver_get_type (), NULL);
	GError *error = NULL;
	for (int scenario = 0; scenario < 6; scenario++) {
		GBytes *key;
		resolver->scenario = scenario;
		key = _g_mime_dkim_lookup_key_full (G_RESOLVER (resolver), "example.org", "sel", FALSE, scenario == 5, NULL, &error);
		if (!scenario) {
			gsize len;
			const void *bytes;
			g_assert_no_error (error);
			bytes = g_bytes_get_data (key, &len);
			g_assert_cmpmem (bytes, len, "abc", 3);
			g_bytes_unref (key);
		} else {
			g_assert_null (key);
			if (scenario == 3)
				g_assert_error (error, G_RESOLVER_ERROR, G_RESOLVER_ERROR_TEMPORARY_FAILURE);
			else if (scenario == 4)
				g_assert_error (error, G_RESOLVER_ERROR, G_RESOLVER_ERROR_NOT_FOUND);
			else
				g_assert_error (error, G_IO_ERROR, G_IO_ERROR_INVALID_DATA);
			g_clear_error (&error);
		}
	}
	g_object_unref (resolver);
}

int
main (int argc, char **argv)
{
	g_test_init (&argc, &argv, NULL);
	g_test_add_func ("/dkim/dns/lookup", test_lookup);
	return g_test_run ();
}
