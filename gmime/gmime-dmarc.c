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

#include "gmime-dmarc.h"
#include "gmime-dkim-private.h"

#include <string.h>

struct _GMimeDmarcVerifier {
	GObject parent;
	GResolver *resolver;
	guint max_author_domains;
	guint max_dkim_signatures;
	GMimeDmarcExistenceFunc existence_func;
	gpointer existence_data;
	GDestroyNotify destroy;
};

struct _GMimeDmarcResult {
	GMimeDmarcStatus status;
	GMimeDmarcErrors errors;
	char *author_domain;
	char *organizational_domain;
	char *policy_domain;
	GMimeDmarcPolicyDomainSource policy_domain_source;
	GMimeDmarcPolicy policy;
	GMimeDmarcPolicy requested_policy;
	GMimeDmarcRecord *record;
	GMimeDmarcIdentifier *spf;
	const GMimeDmarcIdentifier *aligned_dkim;
	gboolean spf_aligned;
	GPtrArray *dkim;
	GPtrArray *authors;
};

typedef struct {
	char *domain;
	GMimeDmarcRecord *record;
	GMimeDmarcErrors errors;
} PolicyQuery;

typedef struct {
	char *domain;
	const char *org;
	guint count;
	PolicyQuery *records[8];
	GMimeDmarcErrors errors;
} TreeWalk;

typedef struct {
	GMimeDmarcVerifier *verifier;
	GMimeMessage *message;
	const GMimeDmarcIdentifier *spf;
	GPtrArray *dkim;
	GHashTable *queries;
	GHashTable *walks;
	GCancellable *cancellable;
	GError *cancel_error;
} Verification;

G_DEFINE_TYPE (GMimeDmarcVerifier, g_mime_dmarc_verifier, G_TYPE_OBJECT)

static void
g_mime_dmarc_verifier_finalize (GObject *object)
{
	GMimeDmarcVerifier *verifier = GMIME_DMARC_VERIFIER (object);

	if (verifier->destroy)
		verifier->destroy (verifier->existence_data);
	g_clear_object (&verifier->resolver);
	G_OBJECT_CLASS (g_mime_dmarc_verifier_parent_class)->finalize (object);
}

static void
g_mime_dmarc_verifier_class_init (GMimeDmarcVerifierClass *klass)
{
	G_OBJECT_CLASS (klass)->finalize = g_mime_dmarc_verifier_finalize;
}

static void
g_mime_dmarc_verifier_init (GMimeDmarcVerifier *verifier)
{
	verifier->max_author_domains = 1;
	verifier->max_dkim_signatures = 10;
}

/**
 * g_mime_dmarc_verifier_new:
 * @resolver: (nullable): DNS resolver, or %NULL for the process default.
 *
 * Returns: (transfer full): a synchronous DMARC verifier. DNS caches are
 * scoped to verification of one message.
 */
GMimeDmarcVerifier *
g_mime_dmarc_verifier_new (GResolver *resolver)
{
	GMimeDmarcVerifier *verifier;

	g_return_val_if_fail (!resolver || G_IS_RESOLVER (resolver), NULL);
	verifier = g_object_new (GMIME_TYPE_DMARC_VERIFIER, NULL);
	verifier->resolver = resolver ? g_object_ref (resolver) : g_resolver_get_default ();
	return verifier;
}

guint
g_mime_dmarc_verifier_get_max_author_domains (GMimeDmarcVerifier *verifier)
{
	g_return_val_if_fail (GMIME_IS_DMARC_VERIFIER (verifier), 0);
	return verifier->max_author_domains;
}

void
g_mime_dmarc_verifier_set_max_author_domains (GMimeDmarcVerifier *verifier, guint limit)
{
	g_return_if_fail (GMIME_IS_DMARC_VERIFIER (verifier));
	g_return_if_fail (limit > 0);
	verifier->max_author_domains = limit;
}

guint
g_mime_dmarc_verifier_get_max_dkim_signatures (GMimeDmarcVerifier *verifier)
{
	g_return_val_if_fail (GMIME_IS_DMARC_VERIFIER (verifier), 0);
	return verifier->max_dkim_signatures;
}

void
g_mime_dmarc_verifier_set_max_dkim_signatures (GMimeDmarcVerifier *verifier, guint limit)
{
	g_return_if_fail (GMIME_IS_DMARC_VERIFIER (verifier));
	g_return_if_fail (limit > 0);
	verifier->max_dkim_signatures = limit;
}

/**
 * g_mime_dmarc_verifier_set_existence_func:
 * @verifier: a #GMimeDmarcVerifier.
 * @func: (nullable): optional DNS existence check for inherited np policies.
 * @user_data: callback closure.
 * @destroy: (nullable): closure destructor.
 *
 * The default TXT lookup can establish existence but cannot tell NXDOMAIN
 * from NODATA. Set a callback backed by an RCODE-aware resolver to apply np
 * when the Author Domain is proved not to exist.
 */
void
g_mime_dmarc_verifier_set_existence_func (GMimeDmarcVerifier *verifier, GMimeDmarcExistenceFunc func,
					 gpointer user_data, GDestroyNotify destroy)
{
	g_return_if_fail (GMIME_IS_DMARC_VERIFIER (verifier));
	if (verifier->destroy)
		verifier->destroy (verifier->existence_data);
	verifier->existence_func = func;
	verifier->existence_data = user_data;
	verifier->destroy = destroy;
}

static GMimeDmarcIdentifier *
identifier_copy (const GMimeDmarcIdentifier *id)
{
	GMimeDmarcIdentifier *copy;

	if (!id)
		return NULL;
	copy = g_new (GMimeDmarcIdentifier, 1);
	copy->status = id->status;
	copy->domain = g_strdup (id->domain);
	return copy;
}

static void
identifier_free (gpointer data)
{
	GMimeDmarcIdentifier *id = data;

	if (id) {
		g_free ((char *) id->domain);
		g_free (id);
	}
}

void
g_mime_dmarc_result_free (GMimeDmarcResult *result)
{
	if (!result)
		return;

	/* The primary result is also one of the per-author results. */
	if (result->authors) {
		for (guint i = 0; i < result->authors->len; i++) {
			GMimeDmarcResult *child = result->authors->pdata[i];
			if (child != result)
				g_mime_dmarc_result_free (child);
		}
		g_ptr_array_unref (result->authors);
	}
	g_clear_pointer (&result->dkim, g_ptr_array_unref);
	identifier_free (result->spf);
	g_mime_dmarc_record_free (result->record);
	g_free (result->author_domain);
	g_free (result->organizational_domain);
	g_free (result->policy_domain);
	g_free (result);
}

static char *
normalize_domain (const char *input)
{
	char *domain;
	gsize length;
	guint label = 0;

	if (!input || !*input || !g_utf8_validate (input, -1, NULL))
		return NULL;
	domain = g_str_is_ascii (input) ? g_ascii_strdown (input, -1) : g_hostname_to_ascii (input);
	if (!domain)
		return NULL;
	length = strlen (domain);
	if (length && domain[length - 1] == '.')
		domain[--length] = '\0';
	if (length > 253)
		goto invalid;
	for (char *p = domain; *p; p++) {
		if (*p == '.') {
			if (!label)
				goto invalid;
			label = 0;
		} else if (g_ascii_isalnum (*p) || *p == '-' || *p == '_') {
			if (++label > 63)
				goto invalid;
			*p = g_ascii_tolower (*p);
		} else {
			goto invalid;
		}
	}
	if (label)
		return domain;
invalid:
	g_free (domain);
	return NULL;
}

static guint
count_headers (GMimeMessage *message, GMimeHeaderId id)
{
	GMimeHeaderList *headers = g_mime_entity_get_header_list (GMIME_ENTITY (message));
	guint count = 0;

	for (int i = 0; i < g_mime_header_list_get_count (headers); i++) {
		if (g_mime_header_get_id (g_mime_header_list_get_header_at (headers, i)) == id)
			count++;
	}
	return count;
}

static GMimeDmarcErrors
get_author_domains (Verification *ctx, GPtrArray *domains)
{
	GMimeDmarcErrors errors = GMIME_DMARC_ERRORS_NONE;
	GMimeInternetAddressList *mailboxes;
	guint count = count_headers (ctx->message, GMIME_HEADER_FROM);

	if (!count)
		return GMIME_DMARC_ERRORS_NO_FROM_HEADER;
	if (count > 1)
		return GMIME_DMARC_ERRORS_MULTIPLE_FROM_HEADERS;
	mailboxes = g_mime_internet_address_list_get_mailboxes (g_mime_message_get_from (ctx->message));
	for (int i = 0; i < g_mime_internet_address_list_length (mailboxes); i++) {
		GMimeInternetAddress *address = g_mime_internet_address_list_get_address (mailboxes, i);
		const char *raw = g_mime_mailbox_address_get_raw_domain (GMIME_MAILBOX_ADDRESS (address));
		char *domain;

		if (!raw || !*raw)
			continue;
		if (!(domain = normalize_domain (raw))) {
			errors = GMIME_DMARC_ERRORS_INVALID_AUTHOR_DOMAIN;
			break;
		}
		if (g_ptr_array_find_with_equal_func (domains, domain, g_str_equal, NULL)) {
			g_free (domain);
			continue;
		}
		if (domains->len == ctx->verifier->max_author_domains) {
			errors = GMIME_DMARC_ERRORS_MULTIPLE_AUTHOR_DOMAINS;
			g_free (domain);
			break;
		}
		g_ptr_array_add (domains, domain);
	}
	g_object_unref (mailboxes);
	if (!domains->len && !errors)
		errors = GMIME_DMARC_ERRORS_NO_AUTHOR_DOMAIN;
	return errors;
}

static void
query_free (gpointer data)
{
	PolicyQuery *query = data;

	g_mime_dmarc_record_free (query->record);
	g_free (query->domain);
	g_free (query);
}

static void
walk_free (gpointer data)
{
	TreeWalk *walk = data;

	g_free (walk->domain);
	g_free (walk);
}

static gboolean
cancelled (Verification *ctx)
{
	return ctx->cancel_error || g_cancellable_set_error_if_cancelled (ctx->cancellable, &ctx->cancel_error);
}

static void
consume_error (Verification *ctx, GError *error)
{
	if (!ctx->cancel_error && g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
		ctx->cancel_error = error;
	else
		g_clear_error (&error);
}

static PolicyQuery *
query_policy (Verification *ctx, const char *domain)
{
	PolicyQuery *query = g_hash_table_lookup (ctx->queries, domain);
	GError *error = NULL;
	GList *records;
	char *name;

	if (query)
		return query;
	query = g_new0 (PolicyQuery, 1);
	query->domain = g_strdup (domain);
	g_hash_table_insert (ctx->queries, query->domain, query);
	if (strlen (domain) + sizeof ("_dmarc.") - 1 > 253)
		return query;

	/* Absolute names prevent resolver search suffixes changing the policy. */
	name = g_strconcat ("_dmarc.", domain, ".", NULL);
	records = g_resolver_lookup_records (ctx->verifier->resolver, name, G_RESOLVER_RECORD_TXT, ctx->cancellable, &error);
	g_free (name);
	if (error && !g_error_matches (error, G_RESOLVER_ERROR, G_RESOLVER_ERROR_NOT_FOUND))
		query->errors = GMIME_DMARC_ERRORS_DNS_TEMPORARY_FAILURE;
	consume_error (ctx, error);
	for (GList *node = records; node; node = node->next) {
		GMimeDmarcRecord *record;
		char **chunks, *text;

		g_variant_get (node->data, "(^as)", &chunks);
		text = g_strjoinv ("", chunks);
		record = g_mime_dmarc_record_parse (text, NULL);
		g_free (text);
		g_strfreev (chunks);
		if (!record)
			continue;
		if (query->record) {
			/* Discard all DMARC records at this name, then continue up the tree. */
			g_clear_pointer (&query->record, g_mime_dmarc_record_free);
			g_mime_dmarc_record_free (record);
			query->errors |= GMIME_DMARC_ERRORS_MULTIPLE_RECORDS;
			break;
		}
		query->record = record;
	}
	g_list_free_full (records, (GDestroyNotify) g_variant_unref);
	return query;
}

static TreeWalk *
tree_walk (Verification *ctx, const char *domain)
{
	TreeWalk *walk = g_hash_table_lookup (ctx->walks, domain);
	const char *target;
	guint labels = 1;

	if (walk)
		return walk;
	walk = g_new0 (TreeWalk, 1);
	walk->domain = g_strdup (domain);
	walk->org = target = walk->domain;
	g_hash_table_insert (ctx->walks, walk->domain, walk);
	for (const char *p = target; *p; p++)
		labels += *p == '.';

	/* RFC 9989 §4.10: query the initial name and at most its seven
	 * rightmost ancestors. A PSD boundary stops the walk early. */
	while (!cancelled (ctx)) {
		PolicyQuery *query = query_policy (ctx, target);
		walk->errors |= query->errors;
		if (query->errors & GMIME_DMARC_ERRORS_DNS_TEMPORARY_FAILURE) {
			walk->org = NULL;
			break;
		}
		if (query->record) {
			GMimeDmarcPublicSuffixDomain psd = g_mime_dmarc_record_get_public_suffix_domain (query->record);
			walk->records[walk->count++] = query;
			walk->org = target;
			if (psd == GMIME_DMARC_PUBLIC_SUFFIX_DOMAIN_YES && target != walk->domain) {
				/* Organizational Domain is one label below the PSD, even
				 * if that label wasn't queried during the bounded walk. */
				const char *p = target - 1;
				while (p > walk->domain && p[-1] != '.')
					p--;
				walk->org = p;
			}
			if (psd != GMIME_DMARC_PUBLIC_SUFFIX_DOMAIN_UNSPECIFIED)
				break;
		}
		if (labels == 1)
			break;
		do {
			target = strchr (target, '.') + 1;
		} while (--labels > 7);
	}
	return walk;
}

static PolicyQuery *
discover_policy (TreeWalk *walk, GMimeDmarcResult *result)
{
	PolicyQuery *psd = NULL;

	result->errors |= walk->errors;
	if (walk->count && !strcmp (walk->records[0]->domain, walk->domain)) {
		result->policy_domain_source = GMIME_DMARC_POLICY_DOMAIN_SOURCE_AUTHOR_DOMAIN;
		return walk->records[0];
	}
	if (!walk->org) {
		/* A policy at the Author Domain can be used without knowing the
		 * Organizational Domain. An inherited policy cannot. */
		result->status = GMIME_DMARC_STATUS_TEMP_ERROR;
		return NULL;
	}
	for (guint i = 0; i < walk->count; i++) {
		PolicyQuery *query = walk->records[i];
		if (!strcmp (query->domain, walk->org)) {
			result->policy_domain_source = GMIME_DMARC_POLICY_DOMAIN_SOURCE_ORGANIZATIONAL_DOMAIN;
			return query;
		}
		if (g_mime_dmarc_record_get_public_suffix_domain (query->record) == GMIME_DMARC_PUBLIC_SUFFIX_DOMAIN_YES)
			psd = query;
	}
	if (psd)
		result->policy_domain_source = GMIME_DMARC_POLICY_DOMAIN_SOURCE_PUBLIC_SUFFIX_DOMAIN;
	return psd;
}

static void
verify_dkim (Verification *ctx)
{
	guint count = count_headers (ctx->message, GMIME_HEADER_DKIM_SIGNATURE);

	ctx->dkim = g_ptr_array_new_with_free_func (identifier_free);
	for (guint i = 0; i < MIN (count, ctx->verifier->max_dkim_signatures) && !cancelled (ctx); i++) {
		GMimeDmarcIdentifier *id = g_new0 (GMimeDmarcIdentifier, 1);
		GError *error = NULL;
		char *domain = NULL;

		if (g_mime_dkim_verify_full (ctx->message, i, ctx->verifier->resolver, ctx->cancellable, &domain, NULL, &error))
			id->status = GMIME_DMARC_IDENTIFIER_PASS;
		else if (g_error_matches (error, G_RESOLVER_ERROR, G_RESOLVER_ERROR_NOT_FOUND) ||
			 g_error_matches (error, G_IO_ERROR, G_IO_ERROR_INVALID_ARGUMENT))
			id->status = GMIME_DMARC_IDENTIFIER_PERM_ERROR;
		else if (error && !g_error_matches (error, G_IO_ERROR, G_IO_ERROR_INVALID_DATA))
			id->status = GMIME_DMARC_IDENTIFIER_TEMP_ERROR;
		else
			id->status = GMIME_DMARC_IDENTIFIER_FAIL;
		id->domain = domain;
		g_ptr_array_add (ctx->dkim, id);
		consume_error (ctx, error);
	}
}

typedef enum {
	NOT_ALIGNED,
	ALIGNED,
	ALIGNMENT_UNKNOWN
} Alignment;

static Alignment
get_alignment (Verification *ctx, const GMimeDmarcResult *result, const char *domain, GMimeDmarcAlignmentMode mode)
{
	const char *org = result->organizational_domain;
	gsize length, suffix;
	TreeWalk *walk;

	if (!strcmp (domain, result->author_domain))
		return ALIGNED;
	if (mode == GMIME_DMARC_ALIGNMENT_MODE_STRICT)
		return NOT_ALIGNED;
	if (!org)
		return ALIGNMENT_UNKNOWN;
	if (!strcmp (domain, org))
		return ALIGNED;
	/* An identifier can share the author's Organizational Domain only if
	 * it lies below it. A child may still assert its own boundary. */
	length = strlen (domain);
	suffix = strlen (org);
	if (length <= suffix + 1 || domain[length - suffix - 1] != '.' || strcmp (domain + length - suffix, org))
		return NOT_ALIGNED;
	walk = tree_walk (ctx, domain);
	if (!walk->org)
		return ALIGNMENT_UNKNOWN;
	return !strcmp (walk->org, org) ? ALIGNED : NOT_ALIGNED;
}

static GMimeDmarcErrors
evaluate_identifier (Verification *ctx, GMimeDmarcResult *result, const GMimeDmarcIdentifier *id, gboolean dkim)
{
	GMimeDmarcAlignmentMode mode;
	Alignment alignment;
	char *domain;

	if (!id || (id->status != GMIME_DMARC_IDENTIFIER_PASS && id->status != GMIME_DMARC_IDENTIFIER_TEMP_ERROR))
		return GMIME_DMARC_ERRORS_NONE;
	if (!(domain = normalize_domain (id->domain)))
		return GMIME_DMARC_ERRORS_NONE;
	mode = dkim ? g_mime_dmarc_record_get_dkim_alignment (result->record) : g_mime_dmarc_record_get_spf_alignment (result->record);
	alignment = get_alignment (ctx, result, domain, mode);
	g_free (domain);
	if (alignment != NOT_ALIGNED && id->status == GMIME_DMARC_IDENTIFIER_TEMP_ERROR)
		return dkim ? GMIME_DMARC_ERRORS_DKIM_TEMPORARY_ERROR : GMIME_DMARC_ERRORS_SPF_TEMPORARY_ERROR;
	if (alignment == ALIGNMENT_UNKNOWN)
		return GMIME_DMARC_ERRORS_DNS_TEMPORARY_FAILURE;
	if (alignment == ALIGNED) {
		if (!dkim)
			result->spf_aligned = TRUE;
		else if (!result->aligned_dkim)
			result->aligned_dkim = id;
	}
	return GMIME_DMARC_ERRORS_NONE;
}

static GMimeDmarcDomainExistence
check_existence (Verification *ctx, const char *domain)
{
	GMimeDmarcVerifier *verifier = ctx->verifier;
	GMimeDmarcDomainExistence existence;
	GError *error = NULL;

	if (verifier->existence_func) {
		existence = verifier->existence_func (domain, ctx->cancellable, verifier->existence_data, &error);
	} else {
		char *name = g_strconcat (domain, ".", NULL);
		GList *records = g_resolver_lookup_records (verifier->resolver, name, G_RESOLVER_RECORD_TXT, ctx->cancellable, &error);
		/* NOT_FOUND does not distinguish an absent name from a name with
		 * no TXT records. Only a positive reply proves existence here. */
		existence = records ? GMIME_DMARC_DOMAIN_EXISTS : GMIME_DMARC_DOMAIN_UNKNOWN;
		g_list_free_full (records, (GDestroyNotify) g_variant_unref);
		g_free (name);
	}
	if (error)
		existence = GMIME_DMARC_DOMAIN_UNKNOWN;
	consume_error (ctx, error);
	return existence;
}

static GMimeDmarcResult *
evaluate_author (Verification *ctx, const char *domain)
{
	const GMimeDmarcRecordErrors invalid = GMIME_DMARC_RECORD_ERRORS_INVALID_POLICY |
		GMIME_DMARC_RECORD_ERRORS_INVALID_SUBDOMAIN_POLICY | GMIME_DMARC_RECORD_ERRORS_INVALID_NON_EXISTENT_SUBDOMAIN_POLICY;
	GMimeDmarcResult *result = g_new0 (GMimeDmarcResult, 1);
	TreeWalk *walk = tree_walk (ctx, domain);
	PolicyQuery *query = discover_policy (walk, result);
	GMimeDmarcErrors pending = GMIME_DMARC_ERRORS_NONE;
	GMimeDmarcPolicy policy, np = GMIME_DMARC_POLICY_UNSPECIFIED;

	result->author_domain = g_strdup (domain);
	result->spf = identifier_copy (ctx->spf);
	if (ctx->dkim)
		result->dkim = g_ptr_array_ref (ctx->dkim);
	if (!query || cancelled (ctx))
		return result;
	result->record = g_mime_dmarc_record_copy (query->record);
	result->organizational_domain = g_strdup (walk->org);
	result->policy_domain = g_strdup (query->domain);
	policy = g_mime_dmarc_record_get_policy (query->record);
	if (policy == GMIME_DMARC_POLICY_UNSPECIFIED || (g_mime_dmarc_record_get_errors (query->record) & invalid)) {
		/* Without a valid reporting destination an invalid policy record
		 * does not enable DMARC processing, not even with p=none. */
		result->errors |= GMIME_DMARC_ERRORS_INVALID_POLICY_RECORD;
		if (!g_mime_dmarc_record_get_aggregate_report_uris (query->record)->len)
			return result;
		policy = GMIME_DMARC_POLICY_NONE;
	} else if (result->policy_domain_source != GMIME_DMARC_POLICY_DOMAIN_SOURCE_AUTHOR_DOMAIN) {
		GMimeDmarcPolicy sp = g_mime_dmarc_record_get_subdomain_policy (query->record);
		if (sp != GMIME_DMARC_POLICY_UNSPECIFIED)
			policy = sp;
		np = g_mime_dmarc_record_get_non_existent_subdomain_policy (query->record);
	}

	/* No signature verification until an applicable policy is found.
	 * All subsequent Author Domains share the same verified identities. */
	if (!ctx->dkim) {
		verify_dkim (ctx);
		result->dkim = g_ptr_array_ref (ctx->dkim);
	}
	for (guint i = 0; i < MIN (ctx->dkim->len, ctx->verifier->max_dkim_signatures) && !cancelled (ctx); i++)
		pending |= evaluate_identifier (ctx, result, ctx->dkim->pdata[i], TRUE);
	if (!cancelled (ctx))
		pending |= evaluate_identifier (ctx, result, ctx->spf, FALSE);
	if (np != GMIME_DMARC_POLICY_UNSPECIFIED && np != policy && !cancelled (ctx)) {
		GMimeDmarcDomainExistence exists = check_existence (ctx, domain);
		if (exists == GMIME_DMARC_DOMAIN_NONEXISTENT)
			policy = np;
		else if (exists != GMIME_DMARC_DOMAIN_EXISTS)
			pending |= GMIME_DMARC_ERRORS_DNS_TEMPORARY_FAILURE;
	}
	/* Only inconclusive checks which could change the verdict defer a
	 * failure. A single aligned pass suffices regardless of other errors. */
	if (result->aligned_dkim || result->spf_aligned) {
		result->status = GMIME_DMARC_STATUS_PASS;
	} else if (pending) {
		result->status = GMIME_DMARC_STATUS_TEMP_ERROR;
		result->errors |= pending;
	} else {
		result->status = GMIME_DMARC_STATUS_FAIL;
	}
	result->requested_policy = policy;
	if (g_mime_dmarc_record_get_testing (query->record))
		policy = policy == GMIME_DMARC_POLICY_REJECT ? GMIME_DMARC_POLICY_QUARANTINE : GMIME_DMARC_POLICY_NONE;
	result->policy = policy;
	return result;
}

static int
severity (const GMimeDmarcResult *result)
{
	switch (result->status) {
	case GMIME_DMARC_STATUS_FAIL: return 10 + result->policy;
	case GMIME_DMARC_STATUS_TEMP_ERROR: return 3;
	case GMIME_DMARC_STATUS_PERM_ERROR: return 2;
	case GMIME_DMARC_STATUS_PASS: return 1;
	default: return 0;
	}
}

static GMimeDmarcResult *
verify (GMimeDmarcVerifier *verifier, GMimeMessage *message, const GMimeDmarcIdentifier *dkim,
	guint n_dkim, gboolean supplied, const GMimeDmarcIdentifier *spf, GCancellable *cancellable, GError **error)
{
	GMimeDmarcResult *result = NULL;
	Verification ctx = { 0 };
	GMimeDmarcErrors errors;
	GPtrArray *domains;

	g_return_val_if_fail (GMIME_IS_DMARC_VERIFIER (verifier), NULL);
	g_return_val_if_fail (GMIME_IS_MESSAGE (message), NULL);
	g_return_val_if_fail (!n_dkim || dkim, NULL);
	if (g_cancellable_set_error_if_cancelled (cancellable, error))
		return NULL;
	ctx.verifier = verifier;
	ctx.message = message;
	ctx.cancellable = cancellable;
	ctx.spf = spf;
	domains = g_ptr_array_new_with_free_func (g_free);
	errors = get_author_domains (&ctx, domains);
	if (errors) {
		result = g_new0 (GMimeDmarcResult, 1);
		result->spf = identifier_copy (spf);
		result->status = GMIME_DMARC_STATUS_PERM_ERROR;
		result->errors = errors;
	} else {
		GPtrArray *authors = domains->len > 1 ? g_ptr_array_new () : NULL;

		ctx.queries = g_hash_table_new_full (g_str_hash, g_str_equal, NULL, query_free);
		ctx.walks = g_hash_table_new_full (g_str_hash, g_str_equal, NULL, walk_free);
		if (supplied) {
			ctx.dkim = g_ptr_array_new_with_free_func (identifier_free);
			for (guint i = 0; i < n_dkim; i++)
				g_ptr_array_add (ctx.dkim, identifier_copy (dkim + i));
		}
		for (guint i = 0; i < domains->len; i++) {
			GMimeDmarcResult *author = evaluate_author (&ctx, domains->pdata[i]);
			if (authors)
				g_ptr_array_add (authors, author);
			if (!result || severity (author) > severity (result))
				result = author;
			if (cancelled (&ctx))
				break;
		}
		result->authors = authors;
		g_clear_pointer (&ctx.dkim, g_ptr_array_unref);
		g_hash_table_unref (ctx.walks);
		g_hash_table_unref (ctx.queries);
	}
	g_ptr_array_unref (domains);
	if (cancelled (&ctx)) {
		g_propagate_error (error, ctx.cancel_error);
		g_clear_pointer (&result, g_mime_dmarc_result_free);
	}
	return result;
}

/**
 * g_mime_dmarc_verifier_verify:
 * @verifier: a #GMimeDmarcVerifier.
 * @message: message whose From and DKIM-Signature fields are evaluated.
 * @spf: (nullable): trusted SPF result for the SMTP transaction.
 * @cancellable: (nullable): optional cancellation token.
 * @error: cancellation error.
 *
 * Verifies actual signatures only when there is an applicable DMARC policy.
 * Does not read Authentication-Results or Received-SPF fields.
 *
 * Returns: (transfer full): the result, or %NULL on cancellation.
 */
GMimeDmarcResult *
g_mime_dmarc_verifier_verify (GMimeDmarcVerifier *verifier, GMimeMessage *message,
			      const GMimeDmarcIdentifier *spf, GCancellable *cancellable, GError **error)
{
	return verify (verifier, message, NULL, 0, FALSE, spf, cancellable, error);
}

/**
 * g_mime_dmarc_verifier_verify_with_results:
 * @verifier: a #GMimeDmarcVerifier.
 * @message: message whose From field is evaluated.
 * @dkim: (array length=n_dkim) (nullable): trusted verification results for
 * this message, in signature order. Never pass results claimed by the sender.
 * @n_dkim: number of DKIM results; zero disables DKIM alignment.
 * @spf: (nullable): trusted SPF result for the SMTP transaction.
 * @cancellable: (nullable): optional cancellation token.
 * @error: cancellation error.
 *
 * Evaluates supplied evidence without verifying any signatures. A negative
 * or temporary DMARC verdict is returned as a result, not a #GError.
 *
 * Returns: (transfer full): the result, or %NULL on cancellation.
 */
GMimeDmarcResult *
g_mime_dmarc_verifier_verify_with_results (GMimeDmarcVerifier *verifier, GMimeMessage *message,
					   const GMimeDmarcIdentifier *dkim, guint n_dkim,
					   const GMimeDmarcIdentifier *spf, GCancellable *cancellable, GError **error)
{
	return verify (verifier, message, dkim, n_dkim, TRUE, spf, cancellable, error);
}

GMimeDmarcStatus
g_mime_dmarc_result_get_status (const GMimeDmarcResult *result)
{
	g_return_val_if_fail (result, GMIME_DMARC_STATUS_NONE);
	return result->status;
}

GMimeDmarcErrors
g_mime_dmarc_result_get_errors (const GMimeDmarcResult *result)
{
	g_return_val_if_fail (result, GMIME_DMARC_ERRORS_NONE);
	return result->errors;
}

const char *
g_mime_dmarc_result_get_author_domain (const GMimeDmarcResult *result)
{
	g_return_val_if_fail (result, NULL);
	return result->author_domain;
}

const char *
g_mime_dmarc_result_get_organizational_domain (const GMimeDmarcResult *result)
{
	g_return_val_if_fail (result, NULL);
	return result->organizational_domain;
}

const char *
g_mime_dmarc_result_get_policy_domain (const GMimeDmarcResult *result)
{
	g_return_val_if_fail (result, NULL);
	return result->policy_domain;
}

GMimeDmarcPolicyDomainSource
g_mime_dmarc_result_get_policy_domain_source (const GMimeDmarcResult *result)
{
	g_return_val_if_fail (result, GMIME_DMARC_POLICY_DOMAIN_SOURCE_UNSPECIFIED);
	return result->policy_domain_source;
}

const GMimeDmarcRecord *
g_mime_dmarc_result_get_record (const GMimeDmarcResult *result)
{
	g_return_val_if_fail (result, NULL);
	return result->record;
}

GMimeDmarcPolicy
g_mime_dmarc_result_get_requested_policy (const GMimeDmarcResult *result)
{
	g_return_val_if_fail (result, GMIME_DMARC_POLICY_UNSPECIFIED);
	return result->requested_policy;
}

GMimeDmarcPolicy
g_mime_dmarc_result_get_policy (const GMimeDmarcResult *result)
{
	g_return_val_if_fail (result, GMIME_DMARC_POLICY_UNSPECIFIED);
	return result->policy;
}

gboolean
g_mime_dmarc_result_get_testing (const GMimeDmarcResult *result)
{
	g_return_val_if_fail (result, FALSE);
	return result->record && g_mime_dmarc_record_get_testing (result->record);
}

gboolean
g_mime_dmarc_result_get_dkim_aligned (const GMimeDmarcResult *result)
{
	g_return_val_if_fail (result, FALSE);
	return result->aligned_dkim != NULL;
}

gboolean
g_mime_dmarc_result_get_spf_aligned (const GMimeDmarcResult *result)
{
	g_return_val_if_fail (result, FALSE);
	return result->spf_aligned;
}

const GMimeDmarcIdentifier *
g_mime_dmarc_result_get_spf_result (const GMimeDmarcResult *result)
{
	g_return_val_if_fail (result, NULL);
	return result->spf;
}

const GMimeDmarcIdentifier *
g_mime_dmarc_result_get_aligned_dkim_result (const GMimeDmarcResult *result)
{
	g_return_val_if_fail (result, NULL);
	return result->aligned_dkim;
}

guint
g_mime_dmarc_result_get_dkim_result_count (const GMimeDmarcResult *result)
{
	g_return_val_if_fail (result, 0);
	return result->dkim ? result->dkim->len : 0;
}

const GMimeDmarcIdentifier *
g_mime_dmarc_result_get_dkim_result (const GMimeDmarcResult *result, guint index)
{
	g_return_val_if_fail (result && result->dkim && index < result->dkim->len, NULL);
	return result->dkim->pdata[index];
}

guint
g_mime_dmarc_result_get_author_domain_result_count (const GMimeDmarcResult *result)
{
	g_return_val_if_fail (result, 0);
	return result->authors ? result->authors->len : 0;
}

const GMimeDmarcResult *
g_mime_dmarc_result_get_author_domain_result (const GMimeDmarcResult *result, guint index)
{
	g_return_val_if_fail (result && result->authors && index < result->authors->len, NULL);
	return result->authors->pdata[index];
}

static void
add_property (GMimeAuthenticationMethodResult *method, const char *type, const char *name, const char *value)
{
	GMimeAuthenticationMethodProperty *property = g_mime_authentication_method_property_new (type, name, value);

	g_mime_authentication_method_result_add_property (method, property);
	g_object_unref (property);
}

GMimeAuthenticationMethodResult *
g_mime_dmarc_result_to_authentication_method_result (const GMimeDmarcResult *result)
{
	static const char *const statuses[] = { "none", "pass", "fail", "temperror", "permerror" };
	GMimeAuthenticationMethodResult *method;

	g_return_val_if_fail (result, NULL);
	method = g_mime_authentication_method_result_new ("dmarc", statuses[result->status]);
	if (result->author_domain)
		add_property (method, "header", "from", result->author_domain);
	if (result->status == GMIME_DMARC_STATUS_FAIL) {
		const char *policy = result->policy == GMIME_DMARC_POLICY_REJECT ? "reject" :
			result->policy == GMIME_DMARC_POLICY_QUARANTINE ? "quarantine" : "none";
		add_property (method, "policy", "dmarc", policy);
	}
	return method;
}
