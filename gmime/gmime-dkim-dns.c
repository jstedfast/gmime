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

#include "gmime-dkim-backend-private.h"

static gboolean
valid_labels (const char *name)
{
	const char *label = name;

	if (!name || !*name || strlen (name) > 253)
		return FALSE;

	for (const char *inptr = name; ; inptr++) {
		if (*inptr == '.' || !*inptr) {
			if (inptr == label || inptr - label > 63 || *label == '-' || inptr[-1] == '-')
				return FALSE;
	
			if (!*inptr)
				return TRUE;
	
			label = inptr + 1;
		} else if (!g_ascii_isalnum (*inptr) && *inptr != '-') {
			return FALSE;
		}
	}
}

static gboolean
contains_token (const char *list, const char *token)
{
	char **items = g_strsplit (list, ":", -1);
	gboolean found = FALSE;

	for (guint i = 0; items[i] && !found; i++)
		found = !strcmp (g_strstrip (items[i]), token);

	g_strfreev (items);

	return found;
}

/* GResolver has already decoded DNS character-strings. Join them within
 * one RR only: distinct RRs at the selector must never form a single key. */
static GHashTable *
key_tags (GVariant *rr)
{
	GHashTable *tags = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, g_free);
	GString *record = g_string_new (NULL);
	GVariantIter *chunks;
	const char *chunk;
	char **fields;

	g_variant_get (rr, "(as)", &chunks);

	while (g_variant_iter_next (chunks, "&s", &chunk))
		g_string_append (record, chunk);

	g_variant_iter_free (chunks);
	fields = g_strsplit (record->str, ";", -1);

	for (guint i = 0; fields[i]; i++) {
		char *field = g_strstrip (fields[i]);
		char *equal = strchr (field, '=');
		char *value;

		if (!*field && !fields[i + 1])
			break;

		if (!equal)
			goto invalid;

		*equal++ = '\0';
		g_strchomp (field);
		value = g_strstrip (equal);

		if (!g_ascii_isalpha (*field) || g_hash_table_contains (tags, field))
			goto invalid;

		for (char *p = field + 1; *p; p++) {
			if (!g_ascii_isalnum (*p) && *p != '_')
				goto invalid;
		}

		if (!strcmp (field, "v") && (i || strcmp (value, "DKIM1")))
			goto invalid;

		g_hash_table_insert (tags, g_strdup (field), g_strdup (value));
	}

	if (!g_hash_table_contains (tags, "p"))
		goto invalid;

	goto done;

invalid:
	g_clear_pointer (&tags, g_hash_table_unref);

done:
	g_strfreev (fields);
	g_string_free (record, TRUE);

	return tags;
}

static GBytes *
decode_key (GHashTable *tags, gboolean ed25519, gboolean subdomain_identity, GError **error)
{
	const char *type = g_hash_table_lookup (tags, "k");
	const char *hashes = g_hash_table_lookup (tags, "h");
	const char *services = g_hash_table_lookup (tags, "s");
	const char *flags = g_hash_table_lookup (tags, "t");
	const char *value = g_hash_table_lookup (tags, "p");
	GString *compact;
	guchar *decoded;
	gboolean valid;
	char *encoded;
	gsize length;

	if (strcmp (type ? type : "rsa", ed25519 ? "ed25519" : "rsa") ||
	    (hashes && !contains_token (hashes, "sha256")) ||
	    (services && !contains_token (services, "*") && !contains_token (services, "email")) ||
	    (subdomain_identity && flags && contains_token (flags, "s"))) {
		g_set_error_literal (error, G_IO_ERROR, G_IO_ERROR_INVALID_DATA, "DKIM key policy does not permit this signature");
		return NULL;
	}

	if (!*value) {
		g_set_error_literal (error, G_IO_ERROR, G_IO_ERROR_INVALID_DATA, "DKIM key has been revoked");
		return NULL;
	}

	compact = g_string_new (NULL);

	for (; *value; value++) {
		if (*value != ' ' && *value != '\t')
			g_string_append_c (compact, *value);
	}

	decoded = g_base64_decode (compact->str, &length);
	encoded = g_base64_encode (decoded, length);
	valid = length && !strcmp (compact->str, encoded) && (!ed25519 || length == 32);

	g_string_free (compact, TRUE);
	g_free (encoded);

	if (!valid) {
		g_free (decoded);
		g_set_error_literal (error, G_IO_ERROR, G_IO_ERROR_INVALID_DATA, "Invalid DKIM public key encoding");
		return NULL;
	}

	return g_bytes_new_take (decoded, length);
}

GBytes *
_g_mime_dkim_lookup_key_full (GResolver *resolver, const char *domain, const char *selector,
			      gboolean ed25519, gboolean subdomain_identity, GCancellable *cancellable, GError **error)
{
	GError *lookup_error = NULL;
	GHashTable *key = NULL;
	GBytes *result = NULL;
	GList *records;
	char *qname;

	if (!valid_labels (domain) || !valid_labels (selector) || strlen (domain) + strlen (selector) + 12 > 253) {
		g_set_error_literal (error, G_IO_ERROR, G_IO_ERROR_INVALID_ARGUMENT, "Invalid DKIM selector or signing domain");
		return NULL;
	}

	if (g_cancellable_set_error_if_cancelled (cancellable, error))
		return NULL;

	qname = g_strdup_printf ("%s._domainkey.%s.", selector, domain);
	resolver = resolver ? g_object_ref (resolver) : g_resolver_get_default ();
	records = g_resolver_lookup_records (resolver, qname, G_RESOLVER_RECORD_TXT, cancellable, &lookup_error);
	g_object_unref (resolver);
	g_free (qname);

	if (lookup_error) {
		g_propagate_error (error, lookup_error);
		return NULL;
	}

	for (GList *item = records; item; item = item->next) {
		GHashTable *candidate = key_tags (item->data);

		if (!candidate)
			continue;

		if (key) {
			g_hash_table_unref (candidate);
			g_set_error_literal (error, G_IO_ERROR, G_IO_ERROR_INVALID_DATA, "Ambiguous DKIM selector: multiple key records");
			goto done;
		}

		key = candidate;
	}

	if (key)
		result = decode_key (key, ed25519, subdomain_identity, error);
	else
		g_set_error_literal (error, G_RESOLVER_ERROR, G_RESOLVER_ERROR_NOT_FOUND, "No DKIM key at the requested selector");

done:
	g_clear_pointer (&key, g_hash_table_unref);
	g_list_free_full (records, (GDestroyNotify) g_variant_unref);

	return result;
}

GBytes *
_g_mime_dkim_lookup_key (GResolver *resolver, const char *domain, const char *selector, gboolean ed25519,
			 GCancellable *cancellable, GError **error)
{
	return _g_mime_dkim_lookup_key_full (resolver, domain, selector, ed25519, FALSE, cancellable, error);
}
