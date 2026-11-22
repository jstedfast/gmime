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

/* Adapted from MimeKit.Fuzzing's FuzzDnsZone and verifier targets.
 * Copyright (c) 2013-2026 .NET Foundation and Contributors (see LICENSE). */
#include "fuzz.h"
#include "gmime/gmime-dkim-private.h"
#include "gmime/gmime-dkim-backend-private.h"
#include <string.h>

typedef struct {
	GResolver parent;
	GHashTable *records, *outcomes;
	GString *queries;
	char *spf;
} Zone;

typedef GResolverClass ZoneClass;
G_DEFINE_TYPE (Zone, fuzz_zone, G_TYPE_RESOLVER)

static char *
zone_name (const char *name)
{
	gsize n = strlen (name);
	return g_ascii_strdown (name, n && name[n - 1] == '.' ? n - 1 : n);
}

static GList *
lookup (GResolver *resolver, const char *name, GResolverRecordType type, GCancellable *cancel, GError **error)
{
	Zone *zone = (Zone *) resolver;
	char *key = zone_name (name);
	const char *outcome = g_hash_table_lookup (zone->outcomes, key);
	GPtrArray *records = g_hash_table_lookup (zone->records, key);
	GList *answers = NULL;

	g_free (key);
	FUZZ_CHECK (type == G_RESOLVER_RECORD_TXT, "Expected a TXT query");
	g_string_append_printf (zone->queries, "%s\n", name);
	if (g_strcmp0 (outcome, "!empty") == 0 || g_strcmp0 (outcome, "!null") == 0)
		return NULL;
	if (g_strcmp0 (outcome, "!tempfail") == 0 || g_strcmp0 (outcome, "!throw") == 0) {
		g_set_error_literal (error, G_RESOLVER_ERROR, G_RESOLVER_ERROR_TEMPORARY_FAILURE, "Temporary failure");
		return NULL;
	}
	if (!records || g_strcmp0 (outcome, "!nxdomain") == 0) {
		g_set_error_literal (error, G_RESOLVER_ERROR, G_RESOLVER_ERROR_NOT_FOUND, "Name not found");
		return NULL;
	}
	for (guint i = 0; i < records->len; i++) {
		const char *chunks[] = { records->pdata[i], NULL };
		answers = g_list_prepend (answers, g_variant_ref_sink (g_variant_new ("(^as)", chunks)));
	}
	return g_list_reverse (answers);
}

static void
zone_finalize (GObject *object)
{
	Zone *zone = (Zone *) object;
	g_hash_table_unref (zone->records);
	g_hash_table_unref (zone->outcomes);
	g_string_free (zone->queries, TRUE);
	g_free (zone->spf);
	G_OBJECT_CLASS (fuzz_zone_parent_class)->finalize (object);
}

static void
fuzz_zone_class_init (ZoneClass *klass)
{
	klass->lookup_records = lookup;
	G_OBJECT_CLASS (klass)->finalize = zone_finalize;
}

static void
fuzz_zone_init (Zone *zone)
{
	zone->queries = g_string_new (NULL);
	zone->records = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, (GDestroyNotify) g_ptr_array_unref);
	zone->outcomes = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, g_free);
}

static void
add_record (Zone *zone, const char *name, const char *value)
{
	char *key = zone_name (name);
	GPtrArray *records = g_hash_table_lookup (zone->records, key);

	if (*value == '!' && value[1]) {
		g_hash_table_replace (zone->outcomes, key, g_strdup (value));
		return;
	}
	if (!records) {
		records = g_ptr_array_new_with_free_func (g_free);
		g_hash_table_insert (zone->records, key, records);
	} else {
		g_free (key);
	}
	if (records->len < 16)
		g_ptr_array_add (records, g_strdup (value));
}

/* One TXT record per line, then a %% line, then the message. Without
 * the delimiter, the entire input is a message and the zone is empty. */
static gsize
parse_zone (Zone *zone, const guint8 *data, gsize length)
{
	for (gsize pos = 0; pos < length;) {
		const guint8 *lf = memchr (data + pos, '\n', length - pos);
		gsize end = lf ? (gsize) (lf - data) : length;
		gsize next = end + (lf != NULL);

		if (end > pos && data[end - 1] == '\r')
			end--;
		if (end - pos == 2 && !memcmp (data + pos, "%%", 2)) {
			char *text = g_utf8_make_valid ((const char *) data, pos);
			char **lines = g_strsplit (text, "\n", -1);
			for (guint i = 0; lines[i]; i++) {
				char *line = lines[i], *value;
				gsize n = strlen (line);
				if (n && line[n - 1] == '\r')
					line[n - 1] = '\0';
				if (!*line || *line == '#')
					continue;
				value = line + strcspn (line, " \t");
				if (*value)
					*value++ = '\0';
				if (!strcmp (line, "@spf")) {
					g_free (zone->spf);
					zone->spf = g_strdup (value);
				} else {
					add_record (zone, line, value);
				}
			}
			g_strfreev (lines);
			g_free (text);
			return next;
		}
		pos = next;
	}
	return 0;
}

static void
record_result (GString *trace, int result, gboolean failed, GError *error)
{
	FUZZ_CHECK (failed == (error != NULL), "Verifier result disagrees with error");
	g_string_append_printf (trace, "%d:%u:%d\n", result, error ? error->domain : 0, error ? error->code : 0);
	g_clear_error (&error);
}

static GString *
verify_message (Zone *zone, const guint8 *data, gsize length, GMimeContentStorage storage, gsize chunk)
{
	GMimeStream *stream = fuzz_stream_new (data, length, chunk);
	GError *error = NULL;
	GMimeMessage *message = g_mime_message_load_with_storage (NULL, stream, storage, &error);
	GString *trace = g_string_new (NULL);
	guint index = 0;

	g_object_unref (stream);
	g_string_truncate (zone->queries, 0);
	if (message) {
		GMimeHeaderList *list = g_mime_entity_get_header_list (GMIME_ENTITY (message));
		for (int i = 0; i < g_mime_header_list_get_count (list) && index < 16; i++) {
			GMimeHeader *header = g_mime_header_list_get_header_at (list, i);
			if (g_mime_header_get_id (header) == GMIME_HEADER_DKIM_SIGNATURE) {
				gboolean ok = g_mime_dkim_verify (message, index++, G_RESOLVER (zone), NULL, &error);
				record_result (trace, ok, !ok, error);
				error = NULL;
			}
		}
		GMimeArcChainStatus arc = g_mime_arc_verify (message, G_RESOLVER (zone), NULL, &error);
		record_result (trace, arc, arc == GMIME_ARC_CHAIN_FAIL, error);
		g_object_unref (message);
	} else {
		record_result (trace, -1, TRUE, error);
	}
	g_string_append (trace, zone->queries->str);
	return trace;
}

void
fuzz_dkim_arc_verifier (const guint8 *data, gsize length)
{
	Zone *zone = g_object_new (fuzz_zone_get_type (), NULL);
	gsize pos = parse_zone (zone, data, length);
	gint64 start = g_get_real_time () / G_USEC_PER_SEC;
	GString *a = verify_message (zone, data + pos, length - pos, GMIME_CONTENT_STORAGE_MEMORY, 4096);
	GString *b = verify_message (zone, data + pos, length - pos, GMIME_CONTENT_STORAGE_SOURCE, 17);

	/* A signature can expire between the two runs. */
	if (start == g_get_real_time () / G_USEC_PER_SEC)
		FUZZ_CHECK (!strcmp (a->str, b->str), "Verification results or DNS queries differ by storage");
	g_string_free (a, TRUE);
	g_string_free (b, TRUE);
	g_object_unref (zone);
}

static GMimeDmarcDomainExistence
domain_exists (const char *name, GCancellable *cancel, gpointer data, GError **error)
{
	Zone *zone = data;
	char *key = zone_name (name);
	const char *outcome = g_hash_table_lookup (zone->outcomes, key);
	gboolean present = g_hash_table_contains (zone->records, key);

	g_free (key);
	if (!g_strcmp0 (outcome, "!empty") || (!outcome && present))
		return GMIME_DMARC_DOMAIN_EXISTS;
	if (!outcome || !strcmp (outcome, "!nxdomain"))
		return GMIME_DMARC_DOMAIN_NONEXISTENT;
	return GMIME_DMARC_DOMAIN_UNKNOWN;
}

static void
check_dmarc (const GMimeDmarcResult *result, const GMimeDmarcIdentifier *spf)
{
	GMimeDmarcStatus status = g_mime_dmarc_result_get_status (result);
	gboolean aligned = g_mime_dmarc_result_get_dkim_aligned (result) || g_mime_dmarc_result_get_spf_aligned (result);
	const GMimeDmarcIdentifier *dkim = g_mime_dmarc_result_get_aligned_dkim_result (result);
	GMimeAuthenticationResults *ar = g_mime_authentication_results_new ("mx.example");
	GMimeAuthenticationMethodResult *method = g_mime_dmarc_result_to_authentication_method_result (result);
	GMimeAuthenticationResults *parsed;
	char *text, *formatted;

	if (status == GMIME_DMARC_STATUS_PASS)
		FUZZ_CHECK (aligned && g_mime_dmarc_result_get_record (result), "DMARC pass without aligned authentication and policy");
	if (status == GMIME_DMARC_STATUS_FAIL)
		FUZZ_CHECK (!aligned && g_mime_dmarc_result_get_policy (result) != GMIME_DMARC_POLICY_UNSPECIFIED,
			    "Inconsistent DMARC failure");
	FUZZ_CHECK (g_mime_dmarc_result_get_dkim_aligned (result) == (dkim != NULL), "Missing or unexpected aligned DKIM result");
	if (dkim)
		FUZZ_CHECK (dkim->status == GMIME_DMARC_IDENTIFIER_PASS, "Aligned DKIM result did not pass");
	if (g_mime_dmarc_result_get_spf_aligned (result))
		FUZZ_CHECK (spf && spf->status == GMIME_DMARC_IDENTIFIER_PASS, "Aligned SPF result did not pass");

	g_mime_authentication_results_add_result (ar, method);
	text = g_mime_authentication_results_to_string (ar);
	parsed = g_mime_authentication_results_parse (text, -1, NULL);
	FUZZ_CHECK (parsed != NULL, "DMARC Authentication-Results did not parse");
	formatted = g_mime_authentication_results_to_string (parsed);
	FUZZ_CHECK (!strcmp (text, formatted), "DMARC Authentication-Results did not round-trip");
	g_free (formatted);
	g_free (text);
	g_object_unref (parsed);
	g_object_unref (method);
	g_object_unref (ar);
}

void
fuzz_dmarc_verifier (const guint8 *data, gsize length)
{
	static const char *const statuses[] = { "none", "pass", "fail", "temperror", "permerror", "softfail", "neutral" };
	Zone *zone = g_object_new (fuzz_zone_get_type (), NULL);
	gsize pos = parse_zone (zone, data, length);
	GMimeStream *stream = fuzz_stream_new (data + pos, length - pos, 37);
	GMimeMessage *message = g_mime_message_load (NULL, stream, NULL);
	GMimeDmarcVerifier *verifier;
	GMimeDmarcResult *direct, *supplied;
	GMimeDmarcIdentifier input, *spf = NULL;
	GMimeDmarcIdentifier ids[8];
	char *domains[8] = { NULL };
	guint count = 0;
	gint64 start = g_get_real_time () / G_USEC_PER_SEC;

	g_object_unref (stream);
	if (!message) {
		g_object_unref (zone);
		return;
	}
	if (zone->spf) {
		char *domain = zone->spf + strcspn (zone->spf, " \t");
		if (*domain) {
			*domain++ = '\0';
			for (guint i = 0; i < G_N_ELEMENTS (statuses); i++) {
				if (!g_ascii_strcasecmp (zone->spf, statuses[i])) {
					input.status = (GMimeDmarcIdentifierStatus) i;
					input.domain = g_strstrip (domain);
					spf = &input;
					break;
				}
			}
		}
	}
	verifier = g_mime_dmarc_verifier_new (G_RESOLVER (zone));
	g_mime_dmarc_verifier_set_existence_func (verifier, domain_exists, zone, NULL);
	g_mime_dmarc_verifier_set_max_author_domains (verifier, (fuzz_selector (data, length) & 1) ? 4 : 1);
	g_mime_dmarc_verifier_set_max_dkim_signatures (verifier, G_N_ELEMENTS (ids));
	direct = g_mime_dmarc_verifier_verify (verifier, message, spf, NULL, NULL);
	FUZZ_CHECK (direct != NULL, "No DMARC decision");
	check_dmarc (direct, spf);
	for (guint i = 0; i < g_mime_dmarc_result_get_author_domain_result_count (direct); i++)
		check_dmarc (g_mime_dmarc_result_get_author_domain_result (direct, i), spf);

	GMimeHeaderList *headers = g_mime_entity_get_header_list (GMIME_ENTITY (message));
	for (int i = 0; i < g_mime_header_list_get_count (headers) && count < G_N_ELEMENTS (ids); i++) {
		GMimeHeader *field = g_mime_header_list_get_header_at (headers, i);
		GError *error = NULL;
		if (g_mime_header_get_id (field) != GMIME_HEADER_DKIM_SIGNATURE)
			continue;
		gboolean ok = g_mime_dkim_verify_full (message, count, G_RESOLVER (zone), NULL, &domains[count], NULL, &error);
		ids[count].status = ok ? GMIME_DMARC_IDENTIFIER_PASS :
			g_error_matches (error, G_RESOLVER_ERROR, G_RESOLVER_ERROR_TEMPORARY_FAILURE) ?
			GMIME_DMARC_IDENTIFIER_TEMP_ERROR : GMIME_DMARC_IDENTIFIER_FAIL;
		ids[count].domain = domains[count];
		count++;
		g_clear_error (&error);
	}
	supplied = g_mime_dmarc_verifier_verify_with_results (verifier, message, ids, count, spf, NULL, NULL);
	FUZZ_CHECK (supplied != NULL, "No DMARC decision with precomputed DKIM results");
	check_dmarc (supplied, spf);
	if (start == g_get_real_time () / G_USEC_PER_SEC)
		FUZZ_CHECK (g_mime_dmarc_result_get_status (direct) == g_mime_dmarc_result_get_status (supplied) &&
			    g_mime_dmarc_result_get_errors (direct) == g_mime_dmarc_result_get_errors (supplied) &&
			    g_mime_dmarc_result_get_policy (direct) == g_mime_dmarc_result_get_policy (supplied) &&
			    g_mime_dmarc_result_get_dkim_aligned (direct) == g_mime_dmarc_result_get_dkim_aligned (supplied) &&
			    g_mime_dmarc_result_get_spf_aligned (direct) == g_mime_dmarc_result_get_spf_aligned (supplied),
			    "DMARC decision differs with precomputed DKIM results");
	for (guint i = 0; i < count; i++)
		g_free (domains[i]);
	g_mime_dmarc_result_free (supplied);
	g_mime_dmarc_result_free (direct);
	g_object_unref (verifier);
	g_object_unref (message);
	g_object_unref (zone);
}

void
fuzz_dns_records (const guint8 *data, gsize length)
{
	Zone *zone = g_object_new (fuzz_zone_get_type (), NULL);
	char *text = g_utf8_make_valid ((const char *) data, length);
	char *raw = g_strconcat ("DKIM-Signature: ", text, NULL);
	GHashTable *tags = _g_mime_dkim_parse_tags (raw, NULL);
	char **lines = g_strsplit (text, "\n", 17);
	guint accepted = 0;
	GError *error = NULL;
	GMimeDmarcRecord *record = g_mime_dmarc_record_parse (text, &error);

	FUZZ_CHECK ((record != NULL) == (error == NULL), "DMARC parse result disagrees with error");
	if (record) {
		GMimeDmarcRecord *again = g_mime_dmarc_record_parse (text, NULL);
		const GPtrArray *lists[] = { g_mime_dmarc_record_get_aggregate_report_uris (record),
					     g_mime_dmarc_record_get_failure_report_uris (record) };
		FUZZ_CHECK (again && g_mime_dmarc_record_get_errors (record) == g_mime_dmarc_record_get_errors (again) &&
			    g_mime_dmarc_record_get_policy (record) == g_mime_dmarc_record_get_policy (again),
			    "Inconsistent DMARC policy parse");
		for (guint i = 0; i < G_N_ELEMENTS (lists); i++) {
			for (guint j = 0; j < lists[i]->len; j++) {
				const char *uri = lists[i]->pdata[j];
				FUZZ_CHECK (uri && *uri, "Empty DMARC report URI");
			}
		}
		g_mime_dmarc_record_free (again);
		g_mime_dmarc_record_free (record);
	}
	g_clear_error (&error);

	if (tags) {
		char *stripped = _g_mime_dkim_strip_b (raw);
		if (g_hash_table_contains (tags, "b")) {
			GHashTable *again = stripped ? _g_mime_dkim_parse_tags (stripped, NULL) : NULL;
			FUZZ_CHECK (again && !g_strcmp0 (g_hash_table_lookup (again, "b"), ""), "Could not empty b= tag");
			g_hash_table_unref (again);
		}
		g_free (stripped);
		g_hash_table_unref (tags);
	}
	for (guint i = 0; i < 16 && lines[i]; i++) {
		char *name = g_strchomp (lines[i]);
		if (_g_mime_dkim_valid_domain (name, FALSE))
			FUZZ_CHECK (_g_mime_dkim_valid_domain (name, TRUE), "Valid domain rejected as selector");
	}
	add_record (zone, "s._domainkey.example.org", text);
	for (int ed = 0; ed <= 1; ed++) {
		GError *error = NULL;
		GBytes *key = _g_mime_dkim_lookup_key (G_RESOLVER (zone), "example.org", "s", ed, NULL, &error);
		FUZZ_CHECK (!!key == !error, "Key lookup result disagrees with error");
		if (key) {
			FUZZ_CHECK (!ed || g_bytes_get_size (key) == 32, "Invalid Ed25519 key size");
			accepted++;
			g_bytes_unref (key);
		}
		g_clear_error (&error);
	}
	FUZZ_CHECK (accepted <= 1, "Record accepted for incompatible algorithms");
	g_strfreev (lines);
	g_free (raw);
	g_free (text);
	g_object_unref (zone);
}
