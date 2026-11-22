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
				add_record (zone, line, value);
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

void
fuzz_dns_records (const guint8 *data, gsize length)
{
	Zone *zone = g_object_new (fuzz_zone_get_type (), NULL);
	char *text = g_utf8_make_valid ((const char *) data, length);
	char *raw = g_strconcat ("DKIM-Signature: ", text, NULL);
	GHashTable *tags = _g_mime_dkim_parse_tags (raw, NULL);
	char **lines = g_strsplit (text, "\n", 17);
	guint accepted = 0;

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
