/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*  GMime
 *  Copyright (C) 2026 Jeffrey Stedfast
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

#include "gmime-compliance.h"
#include "gmime-compliance-data-private.h"

/**
 * SECTION: gmime-compliance
 * @title: MIME compliance
 * @short_description: Optional diagnostic reporting for MIME readers
 *
 * Compliance findings describe the original wire bytes without changing
 * parsing or decoding. Transport and storage contexts affect severity only.
 * Reader operations snapshot their options and reset per-violation budgets.
 **/

struct _GMimeComplianceOptions {
	GMimeComplianceContext context;
	GMimeComplianceValidators validators;
	guint limit;
};

G_DEFINE_BOXED_TYPE (GMimeComplianceOptions, g_mime_compliance_options,
		    g_mime_compliance_options_clone, g_mime_compliance_options_free)
G_DEFINE_BOXED_TYPE (GMimeComplianceIssue, g_mime_compliance_issue,
		    g_mime_compliance_issue_copy, g_mime_compliance_issue_free)

static gboolean
valid_violation (GMimeComplianceViolation violation)
{
	return (violation > GMIME_COMPLIANCE_NONE && violation <= GMIME_COMPLIANCE_INCOMPLETE_UUENCODED_CONTENT) ||
		violation == GMIME_COMPLIANCE_TOO_MANY_COMPLIANCE_ISSUES;
}

static guint
metadata_index (GMimeComplianceViolation violation)
{
	return violation == GMIME_COMPLIANCE_TOO_MANY_COMPLIANCE_ISSUES ? 91 : (guint) violation;
}

/**
 * g_mime_compliance_options_new:
 *
 * Enables all validators in transport context, without a reporting limit.
 *
 * Returns: (transfer full): new compliance options.
 **/
GMimeComplianceOptions *
g_mime_compliance_options_new (void)
{
	GMimeComplianceOptions *options = g_new0 (GMimeComplianceOptions, 1);

	options->validators = GMIME_COMPLIANCE_VALIDATOR_ALL;
	return options;
}

/**
 * g_mime_compliance_options_get_default:
 *
 * Gets application-wide defaults. Configure these once at startup, before
 * creating readers; concurrent mutation is not supported.
 *
 * Returns: (transfer none): the shared default options; do not free.
 **/
GMimeComplianceOptions *
g_mime_compliance_options_get_default (void)
{
	static GMimeComplianceOptions *defaults;
	static gsize initialized;

	if (g_once_init_enter (&initialized)) {
		defaults = g_mime_compliance_options_new ();
		g_once_init_leave (&initialized, 1);
	}

	return defaults;
}

/**
 * g_mime_compliance_options_clone:
 * @options: (nullable): options to copy, or %NULL for defaults
 *
 * Returns: (transfer full): an independent copy.
 **/
GMimeComplianceOptions *
g_mime_compliance_options_clone (GMimeComplianceOptions *options)
{
	GMimeComplianceOptions *copy = g_mime_compliance_options_new ();

	*copy = *(options ? options : g_mime_compliance_options_get_default ());
	return copy;
}

/**
 * g_mime_compliance_options_free:
 * @options: (nullable): options to free
 *
 * Releases the options.
 **/
void
g_mime_compliance_options_free (GMimeComplianceOptions *options)
{
	g_free (options);
}

/**
 * g_mime_compliance_options_get_context:
 * @options: compliance options
 *
 * Returns: the context used to classify diagnostic severity.
 **/
GMimeComplianceContext
g_mime_compliance_options_get_context (GMimeComplianceOptions *options)
{
	g_return_val_if_fail (options != NULL, GMIME_COMPLIANCE_CONTEXT_TRANSPORT);

	return options->context;
}

/**
 * g_mime_compliance_options_set_context:
 * @options: compliance options
 * @context: transport or storage context
 *
 * Sets severity context without affecting which defects are detected.
 **/
void
g_mime_compliance_options_set_context (GMimeComplianceOptions *options, GMimeComplianceContext context)
{
	g_return_if_fail (options != NULL);
	g_return_if_fail (context == GMIME_COMPLIANCE_CONTEXT_TRANSPORT || context == GMIME_COMPLIANCE_CONTEXT_STORAGE);

	options->context = context;
}

/**
 * g_mime_compliance_options_get_enabled_validators:
 * @options: compliance options
 *
 * Returns: flags selecting additional address and encoding validators.
 **/
GMimeComplianceValidators
g_mime_compliance_options_get_enabled_validators (GMimeComplianceOptions *options)
{
	g_return_val_if_fail (options != NULL, GMIME_COMPLIANCE_VALIDATOR_NONE);

	return options->validators;
}

/**
 * g_mime_compliance_options_set_enabled_validators:
 * @options: compliance options
 * @validators: specialized validators to enable
 *
 * Selects specialized validators. General wire and structure checks remain
 * enabled when a reader has a compliance logger.
 **/
void
g_mime_compliance_options_set_enabled_validators (GMimeComplianceOptions *options, GMimeComplianceValidators validators)
{
	g_return_if_fail (options != NULL);
	g_return_if_fail ((validators & ~GMIME_COMPLIANCE_VALIDATOR_ALL) == 0);

	options->validators = validators;
}

/**
 * g_mime_compliance_options_get_max_issues_per_violation:
 * @options: compliance options
 *
 * Returns: the per-violation budget, or zero for unlimited reporting.
 **/
guint
g_mime_compliance_options_get_max_issues_per_violation (GMimeComplianceOptions *options)
{
	g_return_val_if_fail (options != NULL, 0);

	return options->limit;
}

/**
 * g_mime_compliance_options_set_max_issues_per_violation:
 * @options: compliance options
 * @limit: maximum occurrences of each violation, or zero for unlimited
 *
 * Limits reporting independently for each violation in a reader operation.
 * The first suppressed finding produces one TOO_MANY_COMPLIANCE_ISSUES
 * marker; other violations still have their own budgets.
 **/
void
g_mime_compliance_options_set_max_issues_per_violation (GMimeComplianceOptions *options, guint limit)
{
	g_return_if_fail (options != NULL);

	options->limit = limit;
}

/**
 * g_mime_compliance_issue_new:
 * @context: transport or storage context
 * @violation: a diagnostic identity, excluding NONE
 * @stream_offset: absolute input offset
 * @line_number: 1-based line number
 * @column_number: 1-based column, or 0 if unknown
 * @position_kind: what the reported position identifies
 *
 * Returns: (transfer full): a new diagnostic.
 **/
GMimeComplianceIssue *
g_mime_compliance_issue_new (GMimeComplianceContext context, GMimeComplianceViolation violation,
			   gint64 stream_offset, guint64 line_number, guint64 column_number,
			   GMimeCompliancePositionKind position_kind)
{
	GMimeComplianceIssue *issue;

	g_return_val_if_fail (context == GMIME_COMPLIANCE_CONTEXT_TRANSPORT || context == GMIME_COMPLIANCE_CONTEXT_STORAGE, NULL);
	g_return_val_if_fail (valid_violation (violation), NULL);
	g_return_val_if_fail (position_kind >= GMIME_COMPLIANCE_POSITION_EXACT &&
			     position_kind <= GMIME_COMPLIANCE_POSITION_ELEMENT_START, NULL);

	issue = g_new (GMimeComplianceIssue, 1);
	issue->context = context;
	issue->violation = violation;
	issue->stream_offset = stream_offset;
	issue->line_number = line_number;
	issue->column_number = column_number;
	issue->position_kind = position_kind;
	return issue;
}

/**
 * g_mime_compliance_issue_copy:
 * @issue: the diagnostic to copy
 *
 * Returns: (transfer full): an independent diagnostic.
 **/
GMimeComplianceIssue *
g_mime_compliance_issue_copy (const GMimeComplianceIssue *issue)
{
	GMimeComplianceIssue *copy;

	g_return_val_if_fail (issue != NULL, NULL);

	copy = g_new (GMimeComplianceIssue, 1);
	*copy = *issue;
	return copy;
}

/**
 * g_mime_compliance_issue_free:
 * @issue: (nullable): diagnostic to release
 *
 * Releases a diagnostic.
 **/
void
g_mime_compliance_issue_free (GMimeComplianceIssue *issue)
{
	g_free (issue);
}

/**
 * g_mime_compliance_issue_equal:
 * @a: the first diagnostic
 * @b: the second diagnostic
 *
 * Returns: %TRUE if context, violation and every position field are equal.
 **/
gboolean
g_mime_compliance_issue_equal (const GMimeComplianceIssue *a, const GMimeComplianceIssue *b)
{
	g_return_val_if_fail (a != NULL && b != NULL, FALSE);

	return a->context == b->context && a->violation == b->violation &&
		a->stream_offset == b->stream_offset && a->line_number == b->line_number &&
		a->column_number == b->column_number && a->position_kind == b->position_kind;
}

/**
 * g_mime_compliance_issue_to_string:
 * @issue: the diagnostic
 *
 * Returns: (transfer full): its identity and input position.
 **/
char *
g_mime_compliance_issue_to_string (const GMimeComplianceIssue *issue)
{
	const char *name;

	g_return_val_if_fail (issue != NULL && valid_violation (issue->violation), NULL);

	name = compliance_metadata[metadata_index (issue->violation)].name;
	if (issue->column_number) {
		return g_strdup_printf ("%s at line %" G_GUINT64_FORMAT ", column %" G_GUINT64_FORMAT
					" (offset %" G_GINT64_FORMAT ")", name, issue->line_number,
					issue->column_number, issue->stream_offset);
	}

	return g_strdup_printf ("%s at line %" G_GUINT64_FORMAT " (offset %" G_GINT64_FORMAT ")",
				name, issue->line_number, issue->stream_offset);
}

/**
 * g_mime_compliance_violation_get_severity:
 * @violation: a diagnostic identity, excluding NONE
 * @context: transport or storage context
 *
 * Returns: severity of this violation in @context.
 **/
GMimeComplianceSeverity
g_mime_compliance_violation_get_severity (GMimeComplianceViolation violation, GMimeComplianceContext context)
{
	guint index;

	g_return_val_if_fail (valid_violation (violation), GMIME_COMPLIANCE_SEVERITY_MINOR);
	g_return_val_if_fail (context == GMIME_COMPLIANCE_CONTEXT_TRANSPORT || context == GMIME_COMPLIANCE_CONTEXT_STORAGE,
			     GMIME_COMPLIANCE_SEVERITY_MINOR);

	index = metadata_index (violation);

	return context == GMIME_COMPLIANCE_CONTEXT_STORAGE ?
		compliance_metadata[index].storage : compliance_metadata[index].transport;
}

/**
 * g_mime_compliance_violation_get_categories:
 * @violation: a diagnostic identity, excluding NONE
 *
 * Returns: flags describing the possible consequences.
 **/
GMimeComplianceCategories
g_mime_compliance_violation_get_categories (GMimeComplianceViolation violation)
{
	g_return_val_if_fail (valid_violation (violation), GMIME_COMPLIANCE_CATEGORY_NONE);

	return compliance_metadata[metadata_index (violation)].categories;
}

/**
 * g_mime_compliance_violation_get_description:
 * @violation: a diagnostic identity, excluding NONE
 *
 * Returns: (transfer none): the static English description.
 **/
const char *
g_mime_compliance_violation_get_description (GMimeComplianceViolation violation)
{
	g_return_val_if_fail (valid_violation (violation), NULL);

	return compliance_metadata[metadata_index (violation)].description;
}

/**
 * g_mime_compliance_violation_get_remarks:
 * @violation: a diagnostic identity, excluding NONE
 *
 * Returns: (transfer none): static English explanatory remarks.
 **/
const char *
g_mime_compliance_violation_get_remarks (GMimeComplianceViolation violation)
{
	g_return_val_if_fail (valid_violation (violation), NULL);

	return compliance_metadata[metadata_index (violation)].remarks;
}
