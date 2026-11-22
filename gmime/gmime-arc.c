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

/* RFC 8617 validation and sealing */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <string.h>

#include "gmime-arc.h"
#include "gmime-authentication-results.h"
#include "gmime-dkim-backend-private.h"
#include "gmime-dkim-private.h"
#include "gmime-error.h"
#include "gmime-parse-utils.h"

#define MAX_INSTANCE 50

typedef enum {
	ARC_AUTHENTICATION_RESULTS,
	ARC_MESSAGE_SIGNATURE,
	ARC_SEAL
} ArcField;

typedef struct {
	const char *raw[3];
	guint ams_index;
} ArcSet;

typedef struct {
	ArcSet sets[MAX_INSTANCE + 1];
	guint highest;
} ArcChain;

static gboolean
invalid (GError **error, const char *message)
{
	g_set_error_literal (error, GMIME_ERROR, GMIME_ERROR_PROTOCOL_ERROR, message);
	return FALSE;
}

/* All three ARC fields begin with the same instance production. */
static guint
read_instance (const char *value, const char **tail)
{
	const char *p = value, *start, *end = value + strlen (value);
	guint n;

	if (!_g_mime_parse_cfws (value, &p, end, NULL) || *p++ != 'i' ||
	    !_g_mime_parse_cfws (value, &p, end, NULL) || *p++ != '=' ||
	    !_g_mime_parse_cfws (value, &p, end, NULL))
		return 0;

	start = p;

	for (n = 0; g_ascii_isdigit (*p); p++) {
		if (p - start == 2)
			return 0;

		n = n * 10 + (*p - '0');
	}

	if (!n || n > MAX_INSTANCE || !_g_mime_parse_cfws (value, &p, end, NULL) || *p++ != ';')
		return 0;

	if (tail)
		*tail = p;

	return n;
}

static gboolean
valid_aar_payload (const char *value, GError **error)
{
	GMimeAuthenticationResults *ar;
	gboolean valid;
	int unused;

	if (!(ar = g_mime_authentication_results_parse (value, -1, error)))
		return FALSE;

	valid = g_mime_authentication_results_get_authentication_service_identifier (ar) &&
		!g_mime_authentication_results_get_instance (ar, &unused);
	g_object_unref (ar);

	if (!valid)
		return invalid (error, "Expected an Authentication-Results payload, without an instance.");

	return TRUE;
}

/* Accept pre-folded local assessments (e.g. from a header formatter), but
 * never let local input add another header or terminate the header block. */
static gboolean
valid_aar_wire (const char *aar, GError **error)
{
	const guchar *inptr = (const guchar *) aar;
	gsize column = 0;

	while (*inptr) {
		if (*inptr == '\r' && inptr[1] == '\n' && (inptr[2] == ' ' || inptr[2] == '\t')) {
			inptr += 2;
			column = 0;
			continue;
		}

		if (++column > 998 || *inptr == 127 || (*inptr < 32 && *inptr != '\t'))
			return invalid (error, "Invalid control byte, line break or overlong ARC-Authentication-Results line.");

		inptr++;
	}

	if (!g_utf8_validate (aar, -1, NULL))
		return invalid (error, "Invalid UTF-8 in ARC-Authentication-Results.");

	return TRUE;
}

static gboolean
collect (GPtrArray *fields_raw, ArcChain *chain, gboolean failed, GError **error)
{
	static const char * const names[] = { "ARC-Authentication-Results", "ARC-Message-Signature", "ARC-Seal" };
	guint j, field, instance;

	for (j = 0; j < fields_raw->len; j++) {
		const char *raw = fields_raw->pdata[j];
		const char *colon = strchr (raw, ':');
		const char *tail;
		gsize len = colon - raw;
		ArcSet *set;

		/* Match the DKIM reader's treatment of obsolete WSP before ':';
		 * otherwise a malformed duplicate could disappear from the chain. */
		while (len && (raw[len - 1] == ' ' || raw[len - 1] == '\t'))
			len--;

		for (field = ARC_AUTHENTICATION_RESULTS; field <= ARC_SEAL; field++) {
			if (len == strlen (names[field]) && !g_ascii_strncasecmp (raw, names[field], len))
				break;
		}

		if (field > ARC_SEAL)
			continue;

		if (!(instance = read_instance (colon + 1, &tail)))
			return invalid (error, "Invalid ARC instance (expected 1..50).");

		chain->highest = MAX (instance, chain->highest);
		set = &chain->sets[instance];

		if (set->raw[field]) {
			/* Duplicate seals would make the most recent cv ambiguous. */
			if (!failed || field == ARC_SEAL)
				return invalid (error, "Duplicate field in ARC set.");
		}

		set->raw[field] = raw;

		if (field == ARC_MESSAGE_SIGNATURE)
			set->ams_index = j;

		if (!failed && field == ARC_AUTHENTICATION_RESULTS && !valid_aar_payload (tail, error))
			return FALSE;

		if (!failed && field == ARC_MESSAGE_SIGNATURE) {
			/* Even a superseded Arc-Message-Signature must identify its set unambiguously. */
			GHashTable *tags = _g_mime_dkim_parse_tags (raw, error);

			if (!tags)
				return FALSE;

			g_hash_table_unref (tags);
		}
	}

	return TRUE;
}

static gboolean
complete_chain (const ArcChain *chain, GError **error)
{
	guint n;

	for (n = 1; n <= chain->highest; n++) {
		const ArcSet *set = &chain->sets[n];
		GHashTable *tags = NULL;
		const char *cv;

		if (!set->raw[ARC_AUTHENTICATION_RESULTS] || !set->raw[ARC_MESSAGE_SIGNATURE] || !set->raw[ARC_SEAL])
			return invalid (error, "Missing field or gap in ARC chain.");

		if (!(tags = _g_mime_dkim_parse_tags (set->raw[ARC_SEAL], error)))
			return FALSE;

		cv = g_hash_table_lookup (tags, "cv");

		/* The first custodian saw no chain. Every subsequent custodian
		 * must attest to having validated the chain at receipt. */
		gboolean valid = cv && !g_ascii_strcasecmp (cv, n == 1 ? "none" : "pass");
		g_hash_table_unref (tags);

		if (!valid)
			return invalid (error, "Invalid or failed ARC chain validation status.");
	}

	return TRUE;
}

static void
seal_digest (const ArcChain *chain, guint first, guint last, const char *empty_seal, guint8 digest[32])
{
	GChecksum *sum = g_checksum_new (G_CHECKSUM_SHA256);
	gsize length = 32;
	guint n;

	/* RFC 8617 5.1.1: instance order, not transport header order. All
	 * earlier b= values remain intact. Only the seal being checked has
	 * an empty b= and, like a DKIM-Signature, no terminating CRLF. */
	for (n = first; n <= last; n++) {
		const ArcSet *set = &chain->sets[n];

		_g_mime_dkim_hash_header (sum, set->raw[ARC_AUTHENTICATION_RESULTS], TRUE, TRUE);
		_g_mime_dkim_hash_header (sum, set->raw[ARC_MESSAGE_SIGNATURE], TRUE, TRUE);
		_g_mime_dkim_hash_header (sum, n == last ? empty_seal : set->raw[ARC_SEAL], TRUE, n != last);
	}

	g_checksum_get_digest (sum, digest, &length);
	g_checksum_free (sum);
}

static gboolean
verify_seal (const ArcChain *chain, guint n, GResolver *resolver, GCancellable *cancellable, GError **error)
{
	const char *a, *d, *s, *b, *t, *raw = chain->sets[n].raw[ARC_SEAL];
	GHashTable *tags = _g_mime_dkim_parse_tags (raw, error);
	GBytes *signature = NULL, *key = NULL;
	GError *verify_error = NULL;
	char *empty_seal = NULL;
	gboolean valid = FALSE;
	gboolean ed25519;
	guint8 digest[32];

	if (!tags)
		return FALSE;

	a = g_hash_table_lookup (tags, "a");
	d = g_hash_table_lookup (tags, "d");
	s = g_hash_table_lookup (tags, "s");
	b = g_hash_table_lookup (tags, "b");
	t = g_hash_table_lookup (tags, "t");

	ed25519 = a && !g_ascii_strcasecmp (a, "ed25519-sha256");

	if ((!ed25519 && (!a || g_ascii_strcasecmp (a, "rsa-sha256"))) ||
	    !d || !_g_mime_dkim_valid_domain (d, FALSE) || !s || !_g_mime_dkim_valid_domain (s, TRUE) ||
	    !b || !(signature = _g_mime_dkim_decode_base64 (b)) ||
	    (t && (!*t || strlen (t) > 12 || strspn (t, "0123456789") != strlen (t))) ||
	    g_hash_table_contains (tags, "h")) {
		invalid (error, "Invalid or unsupported ARC-Seal tag.");
		goto done;
	}

	if (!(empty_seal = _g_mime_dkim_strip_b (raw))) {
		invalid (error, "Invalid ARC-Seal signature field.");
		goto done;
	}

	seal_digest (chain, 1, n, empty_seal, digest);

	if (!(key = _g_mime_dkim_lookup_key (resolver, d, s, ed25519, cancellable, error)))
		goto done;

	valid = _g_mime_dkim_verify_digest (ed25519, key, digest, signature, &verify_error);

	if (!valid) {
		if (verify_error)
			g_propagate_error (error, verify_error);
		else
			invalid (error, "ARC-Seal signature mismatch.");
	}

done:
	g_clear_pointer (&key, g_bytes_unref);
	g_clear_pointer (&signature, g_bytes_unref);
	g_free (empty_seal);
	g_hash_table_unref (tags);

	return valid;
}

/**
 * g_mime_arc_verify:
 * @message: Received message, before local changes.
 * @resolver: (nullable): TXT resolver, or %NULL for the default resolver.
 * @cancellable: (nullable): Cancellation object.
 * @error: (nullable): Diagnostic on failure, including lookup and cancellation errors.
 *
 * Verifies the most recent ARC-Message-Signature and all ARC seals. No DNS requests are
 * needed for a message with no ARC sets. Do not treat PASS as a policy
 * decision. RFC 8617 §5.2.1 includes DNS errors in FAIL, but the diagnostic
 * lets a receiver distinguish a temporary lookup error and defer delivery.
 *
 * Returns: Chain validation status.
 */
GMimeArcChainStatus
g_mime_arc_verify (GMimeMessage *message, GResolver *resolver, GCancellable *cancellable, GError **error)
{
	GMimeArcChainStatus status = GMIME_ARC_CHAIN_FAIL;
	ArcChain chain = { 0 };
	GPtrArray *fields;
	guint n;

	g_return_val_if_fail (GMIME_IS_MESSAGE (message), GMIME_ARC_CHAIN_FAIL);

	if (g_cancellable_set_error_if_cancelled (cancellable, error) ||
	    !(fields = _g_mime_dkim_get_headers (message, error)))
		return status;

	if (!collect (fields, &chain, FALSE, error) || !complete_chain (&chain, error))
		goto done;

	if (!chain.highest) {
		status = GMIME_ARC_CHAIN_NONE;
		goto done;
	}

	/* An intermediary can legitimately rewrite a subject or body. Only the last
	 * Arc-Message-Signature authenticates the message in its current form. */
	if (!_g_mime_dkim_verify_header (message, fields, chain.sets[chain.highest].ams_index, TRUE,
					 resolver, cancellable, error))
		goto done;

	/* A valid last seal alone does not authenticate any earlier sealer. */
	for (n = chain.highest; n > 0; n--) {
		if (!verify_seal (&chain, n, resolver, cancellable, error))
			goto done;
	}

	status = GMIME_ARC_CHAIN_PASS;

done:
	g_ptr_array_unref (fields);

	return status;
}

/* Fold only inside b=; folds there disappear when verifying this seal. */
static void
append_signature (GString *out, GBytes *signature)
{
	char *base64 = g_base64_encode (g_bytes_get_data (signature, NULL), g_bytes_get_size (signature));
	gsize i, len = strlen (base64);

	for (i = 0; i < len; i += 64) {
		g_string_append (out, "\r\n\t");
		g_string_append_len (out, base64 + i, MIN (len - i, 64));
	}

	g_free (base64);
}

/**
 * g_mime_arc_seal:
 * @message: Outgoing message, after any local changes.
 * @received_status: Result of verifying the incoming message **before**
 *   local changes. Never take this from an untrusted message header.
 * @authentication_results: Local Authentication-Results payload without a
 *   header name, instance or final newline (e.g. "mx.example; spf=pass").
 *   Legal CRLF folds are preserved.
 * @domain: Signing domain.
 * @selector: Signing selector.
 * @algorithm: Signing algorithm.
 * @private_key: PEM private key.
 * @header_canon: ARC-Message-Signature header canonicalization.
 * @body_canon: ARC-Message-Signature body canonicalization.
 * @headers: (array zero-terminated=1) (nullable): ARC-Message-Signature header list or %NULL for defaults.
 * @error: (nullable): Error on failure.
 *
 * Prepends a complete ARC set in place. A FAIL seal covers only the new set. Refuses
 * to extend a terminal failed chain or one where the next instance cannot safely be
 * determined.
 *
 * Returns: %TRUE on success.
 */
gboolean
g_mime_arc_seal (GMimeMessage *message, GMimeArcChainStatus received_status,
		 const char *authentication_results, const char *domain, const char *selector,
		 GMimeDkimAlgorithm algorithm, GBytes *private_key,
		 GMimeDkimCanonicalization header_canon, GMimeDkimCanonicalization body_canon,
		 const char * const *headers, GError **error)
{
	gboolean ed25519 = algorithm == GMIME_DKIM_ALGORITHM_ED25519_SHA256;
	gboolean failed = received_status == GMIME_ARC_CHAIN_FAIL;
	char *aar = NULL, *ams = NULL, *seal = NULL;
	GMimeHeader *set[3] = { NULL, NULL, NULL };
	GBytes *signature = NULL;
	gboolean result = FALSE;
	ArcChain chain = { 0 };
	GPtrArray *fields;
	guint8 digest[32];
	GString *out;
	guint n;

	g_return_val_if_fail (GMIME_IS_MESSAGE (message) && authentication_results && domain && selector && private_key, FALSE);

	if (received_status < GMIME_ARC_CHAIN_NONE || received_status > GMIME_ARC_CHAIN_FAIL ||
	    (!ed25519 && algorithm != GMIME_DKIM_ALGORITHM_RSA_SHA256) ||
	    !_g_mime_dkim_valid_domain (domain, FALSE) || !_g_mime_dkim_valid_domain (selector, TRUE)) {
		invalid (error, "Invalid ARC sealing parameters.");
		return FALSE;
	}

	if (!(fields = _g_mime_dkim_get_headers (message, error)))
		return FALSE;

	if (!collect (fields, &chain, failed, error))
		goto done;

	if ((!chain.highest) != (received_status == GMIME_ARC_CHAIN_NONE) || chain.highest == MAX_INSTANCE) {
		invalid (error, "ARC status or next instance inconsistent with outgoing message.");
		goto done;
	}

	if (!failed && !complete_chain (&chain, error))
		goto done;

	if (failed && chain.sets[chain.highest].raw[ARC_SEAL]) {
		GHashTable *tags = _g_mime_dkim_parse_tags (chain.sets[chain.highest].raw[ARC_SEAL], error);
		const char *cv;

		if (!tags)
			goto done;

		cv = g_hash_table_lookup (tags, "cv");
		gboolean terminal = cv && !g_ascii_strcasecmp (cv, "fail");

		g_hash_table_unref (tags);

		if (terminal) {
			invalid (error, "Cannot extend a terminally failed ARC chain.");
			goto done;
		}
	}

	n = chain.highest + 1;
	aar = g_strdup_printf ("ARC-Authentication-Results: i=%u; %s", n, authentication_results);

	if (!valid_aar_wire (aar, error) || !valid_aar_payload (authentication_results, error))
		goto done;

	ams = _g_mime_dkim_sign_header (message, fields, n, domain, selector, algorithm, private_key,
					header_canon, body_canon, headers, error);

	if (!ams)
		goto done;

	seal = g_strdup_printf ("ARC-Seal: i=%u; cv=%s; a=%s;\r\n\td=%s; s=%s;\r\n\tt=%" G_GINT64_FORMAT "; b=",
				n, failed ? "fail" : (n == 1 ? "none" : "pass"),
				ed25519 ? "ed25519-sha256" : "rsa-sha256", domain, selector,
				g_get_real_time () / G_USEC_PER_SEC);
	chain.sets[n].raw[ARC_AUTHENTICATION_RESULTS] = aar;
	chain.sets[n].raw[ARC_MESSAGE_SIGNATURE] = ams;

	/* There need not be a deterministic set of earlier fields when a
	 * chain is malformed. RFC 8617 §5.1.2 binds only this hop on FAIL. */
	seal_digest (&chain, failed ? n : 1, n, seal, digest);

	if (!(signature = _g_mime_dkim_sign_digest (ed25519, private_key, digest, error)))
		goto done;

	out = g_string_new (seal);
	append_signature (out, signature);
	set[0] = _g_mime_dkim_create_header (out->str, error);
	g_string_free (out, TRUE);

	if (!set[0] || !(set[1] = _g_mime_dkim_create_header (ams, error)) ||
	    !(set[2] = _g_mime_dkim_create_header (aar, error)))
		goto done;

	for (int i = 2; i >= 0; i--)
		g_mime_header_list_insert_header (g_mime_entity_get_header_list (GMIME_ENTITY (message)), 0, set[i]);

	result = TRUE;

done:
	for (int i = 0; i < 3; i++)
		g_clear_object (&set[i]);

	g_free (seal);
	g_free (ams);
	g_free (aar);
	g_clear_pointer (&signature, g_bytes_unref);
	g_ptr_array_unref (fields);

	return result;
}
