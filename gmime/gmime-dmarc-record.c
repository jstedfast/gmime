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

#include "gmime-dmarc-record.h"

#include <gio/gio.h>
#include <string.h>

struct _GMimeDmarcRecord {
	GMimeDmarcPolicy policy, subdomain_policy, non_existent_subdomain_policy;
	GMimeDmarcPublicSuffixDomain public_suffix_domain;
	gboolean testing;
	GMimeDmarcAlignmentMode dkim_alignment, spf_alignment;
	GPtrArray *aggregate_report_uris, *failure_report_uris;
	GMimeDmarcFailureReportingOptions failure_reporting_options;
	GMimeDmarcRecordErrors errors;
};

static gboolean
is_wsp (char c)
{
	return c == ' ' || c == '\t' || c == '\r' || c == '\n';
}

static char *
trim (char *str)
{
	char *end;

	while (is_wsp (*str))
		str++;

	end = str + strlen (str);

	while (end > str && is_wsp (end[-1]))
		end--;

	*end = '\0';

	return str;
}

/* Preserve empty items, even after a trailing separator. */
static char *
take_item (char **cursor, char separator)
{
	char *start = *cursor, *next = strchr (start, separator);

	if (next)
		*next++ = '\0';

	*cursor = next;

	return trim (start);
}

static GMimeDmarcPolicy
parse_policy (const char *value)
{
	if (!g_ascii_strcasecmp (value, "none"))
		return GMIME_DMARC_POLICY_NONE;

	if (!g_ascii_strcasecmp (value, "reject"))
		return GMIME_DMARC_POLICY_REJECT;

	if (!g_ascii_strcasecmp (value, "quarantine"))
		return GMIME_DMARC_POLICY_QUARANTINE;

	return GMIME_DMARC_POLICY_UNSPECIFIED;
}

static gboolean
valid_mailto_authority (const char *str)
{
	const char *end = str + strcspn (str, "/?#"), *at, *host;
	const char *port = NULL, *inptr;

	/* System.Uri treats the first mailto path component as userinfo@host[:port].
	 * GUri treats the entire component as an opaque path instead. */
	if (str == end)
		return TRUE;

	at = memchr (str, '@', end - str);
	host = at ? at + 1 : str;

	if (host == end)
		return FALSE;

	if (*host == '[') {
		const char *bracket = memchr (host + 1, ']', end - host - 1);
		char *address;
		gboolean valid;

		if (!bracket || (bracket + 1 != end && bracket[1] != ':'))
			return FALSE;

		address = g_strndup (host + 1, bracket - host - 1);
		valid = g_hostname_is_ip_address (address) && strchr (address, ':') != NULL;
		g_free (address);

		if (!valid)
			return FALSE;

		if (bracket + 1 < end)
			port = bracket + 1;
	} else {
		for (inptr = host; inptr < end; inptr++) {
			if (*inptr == ':') {
				port = inptr;
				break;
			}

			if (*inptr == '.') {
				if (inptr == host || inptr[-1] == '.')
					return FALSE;
			} else if (!g_ascii_isalnum (*inptr) && *inptr != '-' && *inptr != '_') {
				return FALSE;
			}
		}

		if (inptr == host)
			return FALSE;
	}

	if (port) {
		guint n = 0;

		for (inptr = port + 1; inptr < end; inptr++) {
			if (!g_ascii_isdigit (*inptr))
				return FALSE;

			n = n * 10 + (*inptr - '0');

			if (n > 65535)
				return FALSE;
		}
	}

	return TRUE;
}

static gboolean
valid_uri (const char *s)
{
	const char *p, *scheme;
	GUri *uri;
	gboolean valid = TRUE;

	/* Require an explicit RFC 3986 scheme, not an absolute file system path. */
	if (!g_ascii_isalpha (*s))
		return FALSE;

	for (p = s; *p; p++) {
		if (!g_ascii_isgraph (*p) || *p == ',')
			return FALSE;
	}

	if (!(uri = g_uri_parse (s, G_URI_FLAGS_PARSE_RELAXED | G_URI_FLAGS_ENCODED, NULL)))
		return FALSE;

	scheme = g_uri_get_scheme (uri);

	if (!g_ascii_strcasecmp (scheme, "mailto")) {
		valid = valid_mailto_authority (s + strlen ("mailto:"));
	} else if (!g_ascii_strcasecmp (scheme, "http") || !g_ascii_strcasecmp (scheme, "https") ||
	    !g_ascii_strcasecmp (scheme, "ftp")) {
		const char *host = g_uri_get_host (uri);
		valid = host && *host;
	}

	g_uri_unref (uri);

	return valid;
}

static gboolean
parse_uris (char *value, GPtrArray *uris)
{
	gboolean valid = TRUE;

	while (value) {
		char *uri = take_item (&value, ',');
		char *bang = strchr (uri, '!');

		/* Literal exclamation points must be encoded, so '!' introduces
		 * the obsolete report size limit. Ignore it. */
		if (bang)
			*bang = '\0';

		if (valid_uri (uri))
			g_ptr_array_add (uris, g_strdup (uri));
		else
			valid = FALSE;
	}

	return valid;
}

static gboolean
parse_options (char *value, GMimeDmarcFailureReportingOptions *result)
{
	GMimeDmarcFailureReportingOptions options = GMIME_DMARC_FAILURE_REPORTING_OPTIONS_NONE;
	const guint exclusive = GMIME_DMARC_FAILURE_REPORTING_OPTIONS_ALL_FAIL | GMIME_DMARC_FAILURE_REPORTING_OPTIONS_ANY_FAIL;

	while (value) {
		GMimeDmarcFailureReportingOptions option;
		char *item = take_item (&value, ':');

		if (!*item || item[1])
			return FALSE;

		switch (g_ascii_tolower (*item)) {
		case '0': option = GMIME_DMARC_FAILURE_REPORTING_OPTIONS_ALL_FAIL; break;
		case '1': option = GMIME_DMARC_FAILURE_REPORTING_OPTIONS_ANY_FAIL; break;
		case 'd': option = GMIME_DMARC_FAILURE_REPORTING_OPTIONS_DKIM; break;
		case 's': option = GMIME_DMARC_FAILURE_REPORTING_OPTIONS_SPF; break;
		default: return FALSE;
		}

		/* Each value may appear at most once. */
		if (options & option)
			return FALSE;

		options |= option;
	}

	/* "0" and "1" are mutually exclusive. */
	if ((options & exclusive) == exclusive)
		return FALSE;

	*result = options;

	return TRUE;
}

enum {
	TAG_V, TAG_P, TAG_SP, TAG_NP, TAG_PSD, TAG_T, TAG_ADKIM, TAG_ASPF, TAG_RUA, TAG_RUF, TAG_FO
};

static void
parse_tag (GMimeDmarcRecord *record, char *tag, guint *seen, char **fo)
{
	static const char *const names[] = { "v", "p", "sp", "np", "psd", "t", "adkim", "aspf", "rua", "ruf", "fo" };
	char *end = tag, *value;
	guint i;

	/* Tolerate empty tags (e.g. ";;"). */
	if (!*tag)
		return;

	while (g_ascii_isalpha (*end))
		end++;

	value = end;

	while (is_wsp (*value))
		value++;

	if (end == tag || *value != '=') {
		record->errors |= GMIME_DMARC_RECORD_ERRORS_MALFORMED_TAG;
		return;
	}

	value++;
	*end = '\0';

	while (is_wsp (*value))
		value++;

	for (i = 0; i < G_N_ELEMENTS (names); i++) {
		if (!g_ascii_strcasecmp (names[i], tag))
			break;
	}

	if (i == G_N_ELEMENTS (names))
		return; /* Unknown tags MUST be ignored. */

	if (*seen & (1u << i)) {
		record->errors |= GMIME_DMARC_RECORD_ERRORS_DUPLICATE_TAG;
		return;
	}

	*seen |= 1u << i;

	switch (i) {
	case TAG_P:
		record->policy = parse_policy (value);
		if (record->policy == GMIME_DMARC_POLICY_UNSPECIFIED)
			record->errors |= GMIME_DMARC_RECORD_ERRORS_INVALID_POLICY;
		break;
	case TAG_SP:
		record->subdomain_policy = parse_policy (value);
		if (record->subdomain_policy == GMIME_DMARC_POLICY_UNSPECIFIED)
			record->errors |= GMIME_DMARC_RECORD_ERRORS_INVALID_SUBDOMAIN_POLICY;
		break;
	case TAG_NP:
		record->non_existent_subdomain_policy = parse_policy (value);
		if (record->non_existent_subdomain_policy == GMIME_DMARC_POLICY_UNSPECIFIED)
			record->errors |= GMIME_DMARC_RECORD_ERRORS_INVALID_NON_EXISTENT_SUBDOMAIN_POLICY;
		break;
	case TAG_PSD:
		if (!g_ascii_strcasecmp (value, "y"))
			record->public_suffix_domain = GMIME_DMARC_PUBLIC_SUFFIX_DOMAIN_YES;
		else if (!g_ascii_strcasecmp (value, "n"))
			record->public_suffix_domain = GMIME_DMARC_PUBLIC_SUFFIX_DOMAIN_NO;
		else if (g_ascii_strcasecmp (value, "u"))
			record->errors |= GMIME_DMARC_RECORD_ERRORS_INVALID_PUBLIC_SUFFIX_DOMAIN;
		break;
	case TAG_T:
		if (!g_ascii_strcasecmp (value, "y"))
			record->testing = TRUE;
		else if (g_ascii_strcasecmp (value, "n"))
			record->errors |= GMIME_DMARC_RECORD_ERRORS_INVALID_TESTING;
		break;
	case TAG_ADKIM:
		if (!g_ascii_strcasecmp (value, "s"))
			record->dkim_alignment = GMIME_DMARC_ALIGNMENT_MODE_STRICT;
		else if (g_ascii_strcasecmp (value, "r"))
			record->errors |= GMIME_DMARC_RECORD_ERRORS_INVALID_DKIM_ALIGNMENT;
		break;
	case TAG_ASPF:
		if (!g_ascii_strcasecmp (value, "s"))
			record->spf_alignment = GMIME_DMARC_ALIGNMENT_MODE_STRICT;
		else if (g_ascii_strcasecmp (value, "r"))
			record->errors |= GMIME_DMARC_RECORD_ERRORS_INVALID_SPF_ALIGNMENT;
		break;
	case TAG_RUA:
		if (!parse_uris (value, record->aggregate_report_uris))
			record->errors |= GMIME_DMARC_RECORD_ERRORS_INVALID_AGGREGATE_REPORT_URI;
		break;
	case TAG_RUF:
		if (!parse_uris (value, record->failure_report_uris))
			record->errors |= GMIME_DMARC_RECORD_ERRORS_INVALID_FAILURE_REPORT_URI;
		break;
	case TAG_FO:
		/* Parse fo once we know whether there is also a ruf tag. */
		*fo = value;
		break;
	}
}

/**
 * g_mime_dmarc_record_parse:
 * @text: TXT record with its character-strings joined without a separator.
 * @error: (optional): return location for a fatal parse error.
 *
 * Parses an RFC 9989 policy record. Only a missing or invalid initial
 * version tag prevents recognition. Unknown tags are ignored. Other
 * errors leave default values in effect and can be retrieved with
 * g_mime_dmarc_record_get_errors().
 *
 * Returns: (transfer full) (nullable): the record, or %NULL on error.
 */
GMimeDmarcRecord *
g_mime_dmarc_record_parse (const char *text, GError **error)
{
	char *storage, *cursor, *fo = NULL;
	GMimeDmarcRecord *record;
	guint seen = 1u << TAG_V;
	const char *inptr = text;

	if (!text) {
		g_set_error_literal (error, G_IO_ERROR, G_IO_ERROR_INVALID_ARGUMENT, "Missing DMARC record text");
		return NULL;
	}

	/* Be lenient about whitespace. "v" is case-insensitive, "DMARC1" is not. */
	while (is_wsp (*inptr))
		inptr++;

	if (*inptr != 'v' && *inptr != 'V')
		goto invalid;

	inptr++;

	while (is_wsp (*inptr))
		inptr++;

	if (*inptr != '=')
		goto invalid;

	inptr++;

	while (is_wsp (*inptr))
		inptr++;

	if (!g_str_has_prefix (inptr, "DMARC1"))
		goto invalid;

	inptr += 6;

	while (is_wsp (*inptr))
		inptr++;

	if (*inptr && *inptr != ';')
		goto invalid;

	record = g_new0 (GMimeDmarcRecord, 1);
	record->failure_reporting_options = GMIME_DMARC_FAILURE_REPORTING_OPTIONS_ALL_FAIL;
	record->aggregate_report_uris = g_ptr_array_new_with_free_func (g_free);
	record->failure_report_uris = g_ptr_array_new_with_free_func (g_free);
	cursor = storage = *inptr ? g_strdup (inptr + 1) : NULL;

	while (cursor)
		parse_tag (record, take_item (&cursor, ';'), &seen, &fo);

	if (fo && (seen & (1u << TAG_RUF)) && !parse_options (fo, &record->failure_reporting_options))
		record->errors |= GMIME_DMARC_RECORD_ERRORS_INVALID_FAILURE_REPORTING_OPTIONS;

	g_free (storage);

	return record;

invalid:
	g_set_error (error, G_IO_ERROR, G_IO_ERROR_INVALID_DATA,
		     "Expected initial v=DMARC1 tag (at byte %" G_GSIZE_FORMAT ")", (gsize) (inptr - text));
	return NULL;
}

/**
 * g_mime_dmarc_record_free:
 * @record: (nullable): a parsed record.
 *
 * Frees the record and releases its URI lists.
 */
void
g_mime_dmarc_record_free (GMimeDmarcRecord *record)
{
	if (record) {
		g_ptr_array_unref (record->aggregate_report_uris);
		g_ptr_array_unref (record->failure_report_uris);
		g_free (record);
	}
}

/**
 * g_mime_dmarc_record_copy:
 * @record: a parsed record.
 *
 * Returns: (transfer full): an independently owned copy of the immutable record.
 */
GMimeDmarcRecord *
g_mime_dmarc_record_copy (const GMimeDmarcRecord *record)
{
	GMimeDmarcRecord *copy;

	g_return_val_if_fail (record != NULL, NULL);

	copy = g_new (GMimeDmarcRecord, 1);
	*copy = *record;

	g_ptr_array_ref (copy->aggregate_report_uris);
	g_ptr_array_ref (copy->failure_report_uris);

	return copy;
}

/**
 * g_mime_dmarc_record_get_policy:
 * @record: a parsed record.
 *
 * Returns: the p policy or %GMIME_DMARC_POLICY_UNSPECIFIED if absent or invalid.
 * Check %GMIME_DMARC_RECORD_ERRORS_INVALID_POLICY to distinguish these cases.
 */
GMimeDmarcPolicy
g_mime_dmarc_record_get_policy (const GMimeDmarcRecord *record)
{
	g_return_val_if_fail (record != NULL, GMIME_DMARC_POLICY_UNSPECIFIED);

	return record->policy;
}

/**
 * g_mime_dmarc_record_get_subdomain_policy:
 * @record: a parsed record.
 *
 * Returns: the sp policy or %GMIME_DMARC_POLICY_UNSPECIFIED, before fallback to p.
 */
GMimeDmarcPolicy
g_mime_dmarc_record_get_subdomain_policy (const GMimeDmarcRecord *record)
{
	g_return_val_if_fail (record != NULL, GMIME_DMARC_POLICY_UNSPECIFIED);

	return record->subdomain_policy;
}

/**
 * g_mime_dmarc_record_get_non_existent_subdomain_policy:
 * @record: a parsed record.
 *
 * Returns: the np policy or %GMIME_DMARC_POLICY_UNSPECIFIED, before fallback to sp or p.
 */
GMimeDmarcPolicy
g_mime_dmarc_record_get_non_existent_subdomain_policy (const GMimeDmarcRecord *record)
{
	g_return_val_if_fail (record != NULL, GMIME_DMARC_POLICY_UNSPECIFIED);

	return record->non_existent_subdomain_policy;
}

/**
 * g_mime_dmarc_record_get_public_suffix_domain:
 * @record: a parsed record.
 *
 * Returns: the psd tag, defaulting to %GMIME_DMARC_PUBLIC_SUFFIX_DOMAIN_UNSPECIFIED.
 */
GMimeDmarcPublicSuffixDomain
g_mime_dmarc_record_get_public_suffix_domain (const GMimeDmarcRecord *record)
{
	g_return_val_if_fail (record != NULL, GMIME_DMARC_PUBLIC_SUFFIX_DOMAIN_UNSPECIFIED);

	return record->public_suffix_domain;
}

/**
 * g_mime_dmarc_record_get_testing:
 * @record: a parsed record.
 *
 * Returns: %TRUE if t=y requests testing (apply policy one level below p).
 */
gboolean
g_mime_dmarc_record_get_testing (const GMimeDmarcRecord *record)
{
	g_return_val_if_fail (record != NULL, FALSE);

	return record->testing;
}

/**
 * g_mime_dmarc_record_get_dkim_alignment:
 * @record: a parsed record.
 *
 * Returns: the adkim alignment mode, relaxed by default.
 */
GMimeDmarcAlignmentMode
g_mime_dmarc_record_get_dkim_alignment (const GMimeDmarcRecord *record)
{
	g_return_val_if_fail (record != NULL, GMIME_DMARC_ALIGNMENT_MODE_RELAXED);

	return record->dkim_alignment;
}

/**
 * g_mime_dmarc_record_get_spf_alignment:
 * @record: a parsed record.
 *
 * Returns: the aspf alignment mode, relaxed by default.
 */
GMimeDmarcAlignmentMode
g_mime_dmarc_record_get_spf_alignment (const GMimeDmarcRecord *record)
{
	g_return_val_if_fail (record != NULL, GMIME_DMARC_ALIGNMENT_MODE_RELAXED);

	return record->spf_alignment;
}

/**
 * g_mime_dmarc_record_get_aggregate_report_uris:
 * @record: a parsed record.
 *
 * Returns: (transfer none) (element-type utf8): the valid rua URIs in record order
 * with obsolete size suffixes removed. Do not modify the array or its strings.
 */
const GPtrArray *
g_mime_dmarc_record_get_aggregate_report_uris (const GMimeDmarcRecord *record)
{
	g_return_val_if_fail (record != NULL, NULL);

	return record->aggregate_report_uris;
}

/**
 * g_mime_dmarc_record_get_failure_report_uris:
 * @record: a parsed record.
 *
 * Returns: (transfer none) (element-type utf8): the valid ruf URIs in record order
 * with obsolete size suffixes removed. Do not modify the array or its strings.
 */
const GPtrArray *
g_mime_dmarc_record_get_failure_report_uris (const GMimeDmarcRecord *record)
{
	g_return_val_if_fail (record != NULL, NULL);

	return record->failure_report_uris;
}

/**
 * g_mime_dmarc_record_get_failure_reporting_options:
 * @record: a parsed record.
 *
 * Returns: the fo flags, defaulting to %GMIME_DMARC_FAILURE_REPORTING_OPTIONS_ALL_FAIL.
 * Without a ruf tag, fo is ignored.
 */
GMimeDmarcFailureReportingOptions
g_mime_dmarc_record_get_failure_reporting_options (const GMimeDmarcRecord *record)
{
	g_return_val_if_fail (record != NULL, GMIME_DMARC_FAILURE_REPORTING_OPTIONS_ALL_FAIL);

	return record->failure_reporting_options;
}

/**
 * g_mime_dmarc_record_get_errors:
 * @record: a parsed record.
 *
 * Returns: the recoverable errors encountered after the initial version tag.
 */
GMimeDmarcRecordErrors
g_mime_dmarc_record_get_errors (const GMimeDmarcRecord *record)
{
	g_return_val_if_fail (record != NULL, GMIME_DMARC_RECORD_ERRORS_NONE);

	return record->errors;
}
