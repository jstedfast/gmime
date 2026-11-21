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

#ifndef __GMIME_RTF_COMPLIANCE_H__
#define __GMIME_RTF_COMPLIANCE_H__

#include <gmime/gmime-compliance.h>

G_BEGIN_DECLS

/**
 * GMimeRtfComplianceViolation:
 * @GMIME_RTF_COMPLIANCE_VIOLATION_NONE: No violation.
 * @GMIME_RTF_COMPLIANCE_VIOLATION_MISSING_RTF_HEADER: The document did not begin with the {\rtf header.
 * @GMIME_RTF_COMPLIANCE_VIOLATION_UNBALANCED_GROUP_END: A group end did not match any group start.
 * @GMIME_RTF_COMPLIANCE_VIOLATION_CONTENT_AFTER_DOCUMENT: Content followed the end of the outermost group.
 * @GMIME_RTF_COMPLIANCE_VIOLATION_TRUNCATED_STREAM: The document ended before all of its groups were closed.
 * @GMIME_RTF_COMPLIANCE_VIOLATION_KEYWORD_TOO_LONG: A control word was longer than 32 letters.
 * @GMIME_RTF_COMPLIANCE_VIOLATION_INVALID_PARAMETER: A control word had a malformed or out-of-range numeric parameter.
 * @GMIME_RTF_COMPLIANCE_VIOLATION_INVALID_HEX_ESCAPE: A \' escape was not followed by two hexadecimal digits.
 * @GMIME_RTF_COMPLIANCE_VIOLATION_UNESCAPED_EIGHT_BIT_CHARACTER: The document contained a non-ASCII character that was not escaped.
 * @GMIME_RTF_COMPLIANCE_VIOLATION_UNPAIRED_SURROGATE: A \uN control word specified one half of a UTF-16 surrogate pair without the other.
 * @GMIME_RTF_COMPLIANCE_VIOLATION_UNSUPPORTED_CODE_PAGE: A code page specified by the document could not be resolved.
 * @GMIME_RTF_COMPLIANCE_VIOLATION_UNDEFINED_FONT: A \fN control word referred to a font that was not defined in the font table.
 * @GMIME_RTF_COMPLIANCE_VIOLATION_UNDEFINED_COLOR: A \cfN or \cbN control word referred to a color that was not defined in the color table.
 * @GMIME_RTF_COMPLIANCE_VIOLATION_INVALID_FONT_SIZE: A \fsN control word specified a font size that was not positive or was too large.
 * @GMIME_RTF_COMPLIANCE_VIOLATION_FIELD_INSTRUCTION_TOO_LONG: A field instruction was longer than the maximum length that is examined.
 * @GMIME_RTF_COMPLIANCE_VIOLATION_FONT_TABLE_LIMIT_EXCEEDED: The font table contained more entries than the configured limit.
 * @GMIME_RTF_COMPLIANCE_VIOLATION_COLOR_TABLE_LIMIT_EXCEEDED: The color table contained more entries than the configured limit.
 * @GMIME_RTF_COMPLIANCE_VIOLATION_GROUP_DEPTH_LIMIT_EXCEEDED: Groups that change the formatting state were nested more deeply than the configured limit.
 * @GMIME_RTF_COMPLIANCE_VIOLATION_TOO_MANY_COMPLIANCE_ISSUES: Too many compliance issues were detected and the remainder were suppressed.
 *
 * RTF reader and interpreter recovery diagnostics.
 */
typedef enum {
	GMIME_RTF_COMPLIANCE_VIOLATION_NONE,
	GMIME_RTF_COMPLIANCE_VIOLATION_MISSING_RTF_HEADER,
	GMIME_RTF_COMPLIANCE_VIOLATION_UNBALANCED_GROUP_END,
	GMIME_RTF_COMPLIANCE_VIOLATION_CONTENT_AFTER_DOCUMENT,
	GMIME_RTF_COMPLIANCE_VIOLATION_TRUNCATED_STREAM,
	GMIME_RTF_COMPLIANCE_VIOLATION_KEYWORD_TOO_LONG,
	GMIME_RTF_COMPLIANCE_VIOLATION_INVALID_PARAMETER,
	GMIME_RTF_COMPLIANCE_VIOLATION_INVALID_HEX_ESCAPE,
	GMIME_RTF_COMPLIANCE_VIOLATION_UNESCAPED_EIGHT_BIT_CHARACTER,
	GMIME_RTF_COMPLIANCE_VIOLATION_UNPAIRED_SURROGATE,
	GMIME_RTF_COMPLIANCE_VIOLATION_UNSUPPORTED_CODE_PAGE,
	GMIME_RTF_COMPLIANCE_VIOLATION_UNDEFINED_FONT,
	GMIME_RTF_COMPLIANCE_VIOLATION_UNDEFINED_COLOR,
	GMIME_RTF_COMPLIANCE_VIOLATION_INVALID_FONT_SIZE,
	GMIME_RTF_COMPLIANCE_VIOLATION_FIELD_INSTRUCTION_TOO_LONG,
	GMIME_RTF_COMPLIANCE_VIOLATION_FONT_TABLE_LIMIT_EXCEEDED,
	GMIME_RTF_COMPLIANCE_VIOLATION_COLOR_TABLE_LIMIT_EXCEEDED,
	GMIME_RTF_COMPLIANCE_VIOLATION_GROUP_DEPTH_LIMIT_EXCEEDED,
	GMIME_RTF_COMPLIANCE_VIOLATION_TOO_MANY_COMPLIANCE_ISSUES = G_MAXINT,
} GMimeRtfComplianceViolation;

/**
 * GMimeRtfComplianceIssue:
 * @violation: the reported violation
 * @severity: diagnostic severity
 * @categories: diagnostic categories
 * @offset: zero-based input character offset (byte offset for default Latin-1 input)
 * @line_number: one-based line number
 * @line_position: one-based Unicode scalar column
 * @depth: group depth at the diagnostic
 * @description: (transfer none): a borrowed description
 *
 * A diagnostic borrowed by a compliance callback. The structure is only
 * valid for the duration of that callback.
 */
typedef struct {
	GMimeRtfComplianceViolation violation;
	GMimeComplianceSeverity severity;
	GMimeComplianceCategories categories;
	gint64 offset;
	int line_number, line_position, depth;
	const char *description;
} GMimeRtfComplianceIssue;

/**
 * GMimeRtfComplianceLogger:
 * @issue: (transfer none): a diagnostic valid only during this callback
 * @user_data: (closure) (nullable): callback data
 *
 * Receives an RTF compliance diagnostic.
 */
typedef void (*GMimeRtfComplianceLogger) (const GMimeRtfComplianceIssue *issue, gpointer user_data);

/**
 * GMimeRtfComplianceOptions:
 *
 * Reference-counted options controlling RTF diagnostic reporting limits.
 **/
typedef struct _GMimeRtfComplianceOptions GMimeRtfComplianceOptions;

#define GMIME_TYPE_RTF_COMPLIANCE_OPTIONS (g_mime_rtf_compliance_options_get_type ())

GType g_mime_rtf_compliance_options_get_type (void);

GMimeRtfComplianceOptions *g_mime_rtf_compliance_options_new (void);

GMimeRtfComplianceOptions *g_mime_rtf_compliance_options_ref (GMimeRtfComplianceOptions *options);

void g_mime_rtf_compliance_options_unref (GMimeRtfComplianceOptions *options);

GMimeRtfComplianceOptions *g_mime_rtf_compliance_options_clone (const GMimeRtfComplianceOptions *options);

guint g_mime_rtf_compliance_options_get_max_issues_per_violation (const GMimeRtfComplianceOptions *options);
void g_mime_rtf_compliance_options_set_max_issues_per_violation (GMimeRtfComplianceOptions *options, guint maximum);

const char *g_mime_rtf_compliance_violation_get_description (GMimeRtfComplianceViolation violation);

const char *g_mime_rtf_compliance_violation_get_remarks (GMimeRtfComplianceViolation violation);

GMimeComplianceSeverity g_mime_rtf_compliance_violation_get_severity (GMimeRtfComplianceViolation violation);
GMimeComplianceCategories g_mime_rtf_compliance_violation_get_categories (GMimeRtfComplianceViolation violation);

G_END_DECLS

#endif
