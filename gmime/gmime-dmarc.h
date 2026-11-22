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

#ifndef GMIME_DMARC_H
#define GMIME_DMARC_H

#include <gio/gio.h>
#include <gmime/gmime-authentication-results.h>
#include <gmime/gmime-dmarc-record.h>
#include <gmime/gmime-message.h>

G_BEGIN_DECLS

#define GMIME_TYPE_DMARC_VERIFIER (g_mime_dmarc_verifier_get_type ())
G_DECLARE_FINAL_TYPE (GMimeDmarcVerifier, g_mime_dmarc_verifier, GMIME, DMARC_VERIFIER, GObject)

typedef enum {
	GMIME_DMARC_STATUS_NONE,
	GMIME_DMARC_STATUS_PASS,
	GMIME_DMARC_STATUS_FAIL,
	GMIME_DMARC_STATUS_TEMP_ERROR,
	GMIME_DMARC_STATUS_PERM_ERROR
} GMimeDmarcStatus;

typedef enum {
	GMIME_DMARC_ERRORS_NONE = 0,
	GMIME_DMARC_ERRORS_NO_FROM_HEADER = 1 << 0,
	GMIME_DMARC_ERRORS_MULTIPLE_FROM_HEADERS = 1 << 1,
	GMIME_DMARC_ERRORS_NO_AUTHOR_DOMAIN = 1 << 2,
	GMIME_DMARC_ERRORS_INVALID_AUTHOR_DOMAIN = 1 << 3,
	GMIME_DMARC_ERRORS_MULTIPLE_AUTHOR_DOMAINS = 1 << 4,
	GMIME_DMARC_ERRORS_MULTIPLE_RECORDS = 1 << 5,
	GMIME_DMARC_ERRORS_INVALID_POLICY_RECORD = 1 << 6,
	GMIME_DMARC_ERRORS_DNS_TEMPORARY_FAILURE = 1 << 7,
	GMIME_DMARC_ERRORS_DKIM_TEMPORARY_ERROR = 1 << 8,
	GMIME_DMARC_ERRORS_SPF_TEMPORARY_ERROR = 1 << 9
} GMimeDmarcErrors;

typedef enum {
	GMIME_DMARC_POLICY_DOMAIN_SOURCE_UNSPECIFIED,
	GMIME_DMARC_POLICY_DOMAIN_SOURCE_AUTHOR_DOMAIN,
	GMIME_DMARC_POLICY_DOMAIN_SOURCE_ORGANIZATIONAL_DOMAIN,
	GMIME_DMARC_POLICY_DOMAIN_SOURCE_PUBLIC_SUFFIX_DOMAIN
} GMimeDmarcPolicyDomainSource;

typedef enum {
	GMIME_DMARC_IDENTIFIER_NONE,
	GMIME_DMARC_IDENTIFIER_PASS,
	GMIME_DMARC_IDENTIFIER_FAIL,
	GMIME_DMARC_IDENTIFIER_TEMP_ERROR,
	GMIME_DMARC_IDENTIFIER_PERM_ERROR,
	GMIME_DMARC_IDENTIFIER_SOFT_FAIL,
	GMIME_DMARC_IDENTIFIER_NEUTRAL
} GMimeDmarcIdentifierStatus;

/**
 * GMimeDmarcIdentifier:
 * @status: Outcome from the receiver's DKIM or SPF verifier.
 * @domain: DKIM signing domain (d=) or SPF-checked MAIL FROM domain (including
 * the HELO fallback for a null reverse-path), not a whole mailbox.
 *
 * Trusted evidence for this message. Never construct passing identifiers from
 * untrusted Authentication-Results or Received-SPF headers.
 */
typedef struct {
	GMimeDmarcIdentifierStatus status;
	const char *domain;
} GMimeDmarcIdentifier;

typedef enum {
	GMIME_DMARC_DOMAIN_UNKNOWN,
	GMIME_DMARC_DOMAIN_EXISTS,
	GMIME_DMARC_DOMAIN_NONEXISTENT
} GMimeDmarcDomainExistence;

/* NONEXISTENT requires a DNS NXDOMAIN response, not an empty RRset. */
typedef GMimeDmarcDomainExistence (*GMimeDmarcExistenceFunc) (const char *domain, GCancellable *cancellable,
							     gpointer user_data, GError **error);

typedef struct _GMimeDmarcResult GMimeDmarcResult;

GMimeDmarcVerifier *g_mime_dmarc_verifier_new (GResolver *resolver);
guint g_mime_dmarc_verifier_get_max_author_domains (GMimeDmarcVerifier *verifier);
void g_mime_dmarc_verifier_set_max_author_domains (GMimeDmarcVerifier *verifier, guint limit);
guint g_mime_dmarc_verifier_get_max_dkim_signatures (GMimeDmarcVerifier *verifier);
void g_mime_dmarc_verifier_set_max_dkim_signatures (GMimeDmarcVerifier *verifier, guint limit);
void g_mime_dmarc_verifier_set_existence_func (GMimeDmarcVerifier *verifier, GMimeDmarcExistenceFunc func,
					      gpointer user_data, GDestroyNotify destroy);

GMimeDmarcResult *g_mime_dmarc_verifier_verify (GMimeDmarcVerifier *verifier, GMimeMessage *message,
						const GMimeDmarcIdentifier *spf, GCancellable *cancellable, GError **error);
GMimeDmarcResult *g_mime_dmarc_verifier_verify_with_results (GMimeDmarcVerifier *verifier, GMimeMessage *message,
							    const GMimeDmarcIdentifier *dkim, guint n_dkim,
							    const GMimeDmarcIdentifier *spf, GCancellable *cancellable, GError **error);

/* Result getters borrow their return values for the lifetime of the result. */
void g_mime_dmarc_result_free (GMimeDmarcResult *result);
GMimeDmarcStatus g_mime_dmarc_result_get_status (const GMimeDmarcResult *result);
GMimeDmarcErrors g_mime_dmarc_result_get_errors (const GMimeDmarcResult *result);
const char *g_mime_dmarc_result_get_author_domain (const GMimeDmarcResult *result);
const char *g_mime_dmarc_result_get_organizational_domain (const GMimeDmarcResult *result);
const char *g_mime_dmarc_result_get_policy_domain (const GMimeDmarcResult *result);
GMimeDmarcPolicyDomainSource g_mime_dmarc_result_get_policy_domain_source (const GMimeDmarcResult *result);
const GMimeDmarcRecord *g_mime_dmarc_result_get_record (const GMimeDmarcResult *result);
GMimeDmarcPolicy g_mime_dmarc_result_get_requested_policy (const GMimeDmarcResult *result);
GMimeDmarcPolicy g_mime_dmarc_result_get_policy (const GMimeDmarcResult *result);
gboolean g_mime_dmarc_result_get_testing (const GMimeDmarcResult *result);
gboolean g_mime_dmarc_result_get_dkim_aligned (const GMimeDmarcResult *result);
gboolean g_mime_dmarc_result_get_spf_aligned (const GMimeDmarcResult *result);
const GMimeDmarcIdentifier *g_mime_dmarc_result_get_spf_result (const GMimeDmarcResult *result);
const GMimeDmarcIdentifier *g_mime_dmarc_result_get_aligned_dkim_result (const GMimeDmarcResult *result);
guint g_mime_dmarc_result_get_dkim_result_count (const GMimeDmarcResult *result);
const GMimeDmarcIdentifier *g_mime_dmarc_result_get_dkim_result (const GMimeDmarcResult *result, guint index);
guint g_mime_dmarc_result_get_author_domain_result_count (const GMimeDmarcResult *result);
const GMimeDmarcResult *g_mime_dmarc_result_get_author_domain_result (const GMimeDmarcResult *result, guint index);
GMimeAuthenticationMethodResult *g_mime_dmarc_result_to_authentication_method_result (const GMimeDmarcResult *result);

G_END_DECLS

#endif /* GMIME_DMARC_H */
