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

#ifndef GMIME_DMARC_RECORD_H
#define GMIME_DMARC_RECORD_H

#include <glib.h>

G_BEGIN_DECLS

/**
 * GMimeDmarcPolicy:
 * @GMIME_DMARC_POLICY_UNSPECIFIED: Tag was absent or invalid.
 * @GMIME_DMARC_POLICY_NONE: No change to message disposition requested.
 * @GMIME_DMARC_POLICY_QUARANTINE: Treat a failing message as suspicious.
 * @GMIME_DMARC_POLICY_REJECT: Reject a failing message.
 *
 * A domain owner's assessment policy.
 */
typedef enum {
	GMIME_DMARC_POLICY_UNSPECIFIED,
	GMIME_DMARC_POLICY_NONE,
	GMIME_DMARC_POLICY_QUARANTINE,
	GMIME_DMARC_POLICY_REJECT
} GMimeDmarcPolicy;

/**
 * GMimeDmarcAlignmentMode:
 * @GMIME_DMARC_ALIGNMENT_MODE_RELAXED: Identifiers may share an organizational domain.
 * @GMIME_DMARC_ALIGNMENT_MODE_STRICT: Identifiers must match exactly.
 */
typedef enum {
	GMIME_DMARC_ALIGNMENT_MODE_RELAXED,
	GMIME_DMARC_ALIGNMENT_MODE_STRICT
} GMimeDmarcAlignmentMode;

/**
 * GMimeDmarcPublicSuffixDomain:
 * @GMIME_DMARC_PUBLIC_SUFFIX_DOMAIN_UNSPECIFIED: No assertion.
 * @GMIME_DMARC_PUBLIC_SUFFIX_DOMAIN_YES: The domain is a public suffix.
 * @GMIME_DMARC_PUBLIC_SUFFIX_DOMAIN_NO: The domain is not a public suffix.
 */
typedef enum {
	GMIME_DMARC_PUBLIC_SUFFIX_DOMAIN_UNSPECIFIED,
	GMIME_DMARC_PUBLIC_SUFFIX_DOMAIN_YES,
	GMIME_DMARC_PUBLIC_SUFFIX_DOMAIN_NO
} GMimeDmarcPublicSuffixDomain;

/**
 * GMimeDmarcFailureReportingOptions:
 * @GMIME_DMARC_FAILURE_REPORTING_OPTIONS_NONE: No options.
 * @GMIME_DMARC_FAILURE_REPORTING_OPTIONS_ALL_FAIL: All mechanisms must fail to produce an aligned pass.
 * @GMIME_DMARC_FAILURE_REPORTING_OPTIONS_ANY_FAIL: Any mechanism fails to produce an aligned pass.
 * @GMIME_DMARC_FAILURE_REPORTING_OPTIONS_DKIM: Request a DKIM failure report.
 * @GMIME_DMARC_FAILURE_REPORTING_OPTIONS_SPF: Request an SPF failure report.
 */
typedef enum {
	GMIME_DMARC_FAILURE_REPORTING_OPTIONS_NONE = 0,
	GMIME_DMARC_FAILURE_REPORTING_OPTIONS_ALL_FAIL = 1 << 0,
	GMIME_DMARC_FAILURE_REPORTING_OPTIONS_ANY_FAIL = 1 << 1,
	GMIME_DMARC_FAILURE_REPORTING_OPTIONS_DKIM = 1 << 2,
	GMIME_DMARC_FAILURE_REPORTING_OPTIONS_SPF = 1 << 3
} GMimeDmarcFailureReportingOptions;

/**
 * GMimeDmarcRecordErrors:
 * @GMIME_DMARC_RECORD_ERRORS_NONE: No discarded errors.
 * @GMIME_DMARC_RECORD_ERRORS_MALFORMED_TAG: Missing alphabetic name or equals sign.
 * @GMIME_DMARC_RECORD_ERRORS_DUPLICATE_TAG: A recognized tag occurred more than once.
 * @GMIME_DMARC_RECORD_ERRORS_INVALID_POLICY: Invalid p value.
 * @GMIME_DMARC_RECORD_ERRORS_INVALID_SUBDOMAIN_POLICY: Invalid sp value.
 * @GMIME_DMARC_RECORD_ERRORS_INVALID_NON_EXISTENT_SUBDOMAIN_POLICY: Invalid np value.
 * @GMIME_DMARC_RECORD_ERRORS_INVALID_PUBLIC_SUFFIX_DOMAIN: Invalid psd value.
 * @GMIME_DMARC_RECORD_ERRORS_INVALID_TESTING: Invalid t value.
 * @GMIME_DMARC_RECORD_ERRORS_INVALID_DKIM_ALIGNMENT: Invalid adkim value.
 * @GMIME_DMARC_RECORD_ERRORS_INVALID_SPF_ALIGNMENT: Invalid aspf value.
 * @GMIME_DMARC_RECORD_ERRORS_INVALID_AGGREGATE_REPORT_URI: At least one invalid rua URI.
 * @GMIME_DMARC_RECORD_ERRORS_INVALID_FAILURE_REPORT_URI: At least one invalid ruf URI.
 * @GMIME_DMARC_RECORD_ERRORS_INVALID_FAILURE_REPORTING_OPTIONS: Invalid fo options.
 *
 * Recoverable errors in an otherwise recognizable DMARC record.
 */
typedef enum {
	GMIME_DMARC_RECORD_ERRORS_NONE = 0,
	GMIME_DMARC_RECORD_ERRORS_MALFORMED_TAG = 1 << 0,
	GMIME_DMARC_RECORD_ERRORS_DUPLICATE_TAG = 1 << 1,
	GMIME_DMARC_RECORD_ERRORS_INVALID_POLICY = 1 << 2,
	GMIME_DMARC_RECORD_ERRORS_INVALID_SUBDOMAIN_POLICY = 1 << 3,
	GMIME_DMARC_RECORD_ERRORS_INVALID_NON_EXISTENT_SUBDOMAIN_POLICY = 1 << 4,
	GMIME_DMARC_RECORD_ERRORS_INVALID_PUBLIC_SUFFIX_DOMAIN = 1 << 5,
	GMIME_DMARC_RECORD_ERRORS_INVALID_TESTING = 1 << 6,
	GMIME_DMARC_RECORD_ERRORS_INVALID_DKIM_ALIGNMENT = 1 << 7,
	GMIME_DMARC_RECORD_ERRORS_INVALID_SPF_ALIGNMENT = 1 << 8,
	GMIME_DMARC_RECORD_ERRORS_INVALID_AGGREGATE_REPORT_URI = 1 << 9,
	GMIME_DMARC_RECORD_ERRORS_INVALID_FAILURE_REPORT_URI = 1 << 10,
	GMIME_DMARC_RECORD_ERRORS_INVALID_FAILURE_REPORTING_OPTIONS = 1 << 11
} GMimeDmarcRecordErrors;

/**
 * GMimeDmarcRecord:
 *
 * Immutable DMARC policy record published in DNS at _dmarc.&lt;domain&gt;.
 */
typedef struct _GMimeDmarcRecord GMimeDmarcRecord;

GMimeDmarcRecord *g_mime_dmarc_record_parse (const char *text, GError **error);
GMimeDmarcRecord *g_mime_dmarc_record_copy (const GMimeDmarcRecord *record);
void g_mime_dmarc_record_free (GMimeDmarcRecord *record);

GMimeDmarcPolicy g_mime_dmarc_record_get_policy (const GMimeDmarcRecord *record);
GMimeDmarcPolicy g_mime_dmarc_record_get_subdomain_policy (const GMimeDmarcRecord *record);
GMimeDmarcPolicy g_mime_dmarc_record_get_non_existent_subdomain_policy (const GMimeDmarcRecord *record);
GMimeDmarcPublicSuffixDomain g_mime_dmarc_record_get_public_suffix_domain (const GMimeDmarcRecord *record);
gboolean g_mime_dmarc_record_get_testing (const GMimeDmarcRecord *record);
GMimeDmarcAlignmentMode g_mime_dmarc_record_get_dkim_alignment (const GMimeDmarcRecord *record);
GMimeDmarcAlignmentMode g_mime_dmarc_record_get_spf_alignment (const GMimeDmarcRecord *record);
const GPtrArray *g_mime_dmarc_record_get_aggregate_report_uris (const GMimeDmarcRecord *record);
const GPtrArray *g_mime_dmarc_record_get_failure_report_uris (const GMimeDmarcRecord *record);
GMimeDmarcFailureReportingOptions g_mime_dmarc_record_get_failure_reporting_options (const GMimeDmarcRecord *record);
GMimeDmarcRecordErrors g_mime_dmarc_record_get_errors (const GMimeDmarcRecord *record);

G_END_DECLS

#endif /* GMIME_DMARC_RECORD_H */
