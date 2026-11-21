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

#include "gmime-rtf-compliance-private.h"

/**
 * SECTION: gmime-rtf-compliance
 * @title: RTF compliance diagnostics
 * @short_description: RTF recovery diagnostics and reporting limits
 *
 * A logger receives an issue valid for the duration of its callback. Description
 * and remarks strings are static. A zero per-violation limit means unlimited
 * reporting. Otherwise one truncation marker is emitted at the first suppressed
 * issue; the reporting budget restarts for each conversion.
 */

struct _GMimeRtfComplianceOptions {
	gatomicrefcount refs;
	guint maximum;
};

G_DEFINE_BOXED_TYPE (GMimeRtfComplianceOptions, g_mime_rtf_compliance_options, g_mime_rtf_compliance_options_ref,
		     g_mime_rtf_compliance_options_unref)

void
_g_mime_rtf_diagnostics_init (GMimeRtfDiagnostics *diag, const GMimeRtfComplianceOptions *options,
			      GMimeRtfComplianceLogger logger, gpointer user_data)
{
	*diag = (GMimeRtfDiagnostics) { .logger = logger, .user_data = user_data, .maximum = options ? options->maximum : 0 };
}

void
_g_mime_rtf_report (GMimeRtfDiagnostics *diag, GMimeRtfComplianceViolation violation,
		    guint64 offset, guint64 line, guint64 column, guint64 depth)
{
	GMimeRtfComplianceIssue issue;

	if (!diag || !diag->logger)
		return;

	if (diag->maximum && (guint) violation < G_N_ELEMENTS (diag->counts)) {
		if (diag->counts[violation] < diag->maximum) {
			diag->counts[violation]++;
		} else if (!diag->suppressed) {
			diag->suppressed = TRUE;
			violation = GMIME_RTF_COMPLIANCE_VIOLATION_TOO_MANY_COMPLIANCE_ISSUES;
		} else {
			return;
		}
	}

	issue.violation = violation;
	issue.offset = MIN (offset, G_MAXINT64);
	issue.line_number = MIN (line, G_MAXINT);
	issue.line_position = MIN (column, G_MAXINT);
	issue.depth = MIN (depth, G_MAXINT);
	issue.description = g_mime_rtf_compliance_violation_get_description (violation);
	issue.severity = g_mime_rtf_compliance_violation_get_severity (violation);
	issue.categories = g_mime_rtf_compliance_violation_get_categories (violation);

	diag->logger (&issue, diag->user_data);
}

/**
 * g_mime_rtf_compliance_options_new:
 *
 * Returns: (transfer full): new diagnostic options
 */
GMimeRtfComplianceOptions *
g_mime_rtf_compliance_options_new (void)
{
	GMimeRtfComplianceOptions *options = g_new0 (GMimeRtfComplianceOptions, 1);

	g_atomic_ref_count_init (&options->refs);

	return options;
}

/**
 * g_mime_rtf_compliance_options_ref:
 * @options: diagnostic options
 *
 * Returns: (transfer full): @options with an additional reference
 */
GMimeRtfComplianceOptions *
g_mime_rtf_compliance_options_ref (GMimeRtfComplianceOptions *options)
{
	g_atomic_ref_count_inc (&options->refs);

	return options;
}

/**
 * g_mime_rtf_compliance_options_unref:
 * @options: (transfer full) (nullable): diagnostic options to release
 *
 * Releases one reference to @options.
 */
void
g_mime_rtf_compliance_options_unref (GMimeRtfComplianceOptions *options)
{
	if (options && g_atomic_ref_count_dec (&options->refs))
		g_free (options);
}

/**
 * g_mime_rtf_compliance_options_clone:
 * @options: diagnostic options to copy
 *
 * Returns: (transfer full): an independent copy of @options
 */
GMimeRtfComplianceOptions *
g_mime_rtf_compliance_options_clone (const GMimeRtfComplianceOptions *options)
{
	GMimeRtfComplianceOptions *clone = g_mime_rtf_compliance_options_new ();

	clone->maximum = options->maximum;

	return clone;
}

/**
 * g_mime_rtf_compliance_options_get_max_issues_per_violation:
 * @options: diagnostic options
 *
 * Gets the reporting limit for each violation. The default is zero, meaning
 * unlimited reporting.
 *
 * Returns: the maximum number of reports per violation, or zero for no limit
 **/
guint
g_mime_rtf_compliance_options_get_max_issues_per_violation (const GMimeRtfComplianceOptions *options)
{
	return options->maximum;
}

/**
 * g_mime_rtf_compliance_options_set_max_issues_per_violation:
 * @options: diagnostic options
 * @maximum: the maximum number of reports per violation, or zero for no limit
 *
 * Sets the reporting limit. A tokenizer emits one
 * %GMIME_RTF_COMPLIANCE_VIOLATION_TOO_MANY_COMPLIANCE_ISSUES diagnostic when
 * reports are first suppressed. The limit is sampled when a conversion begins.
 **/
void
g_mime_rtf_compliance_options_set_max_issues_per_violation (GMimeRtfComplianceOptions *options, guint maximum)
{
	options->maximum = maximum;
}


/**
 * g_mime_rtf_compliance_violation_get_severity:
 * @violation: an RTF compliance violation
 *
 * Returns: the severity for @violation
 */
GMimeComplianceSeverity
g_mime_rtf_compliance_violation_get_severity (GMimeRtfComplianceViolation violation)
{
	switch (violation) {
	case GMIME_RTF_COMPLIANCE_VIOLATION_MISSING_RTF_HEADER:
	case GMIME_RTF_COMPLIANCE_VIOLATION_UNBALANCED_GROUP_END:
	case GMIME_RTF_COMPLIANCE_VIOLATION_KEYWORD_TOO_LONG:
	case GMIME_RTF_COMPLIANCE_VIOLATION_INVALID_PARAMETER:
	case GMIME_RTF_COMPLIANCE_VIOLATION_INVALID_HEX_ESCAPE:
	case GMIME_RTF_COMPLIANCE_VIOLATION_UNESCAPED_EIGHT_BIT_CHARACTER:
	case GMIME_RTF_COMPLIANCE_VIOLATION_UNPAIRED_SURROGATE:
	case GMIME_RTF_COMPLIANCE_VIOLATION_UNDEFINED_FONT:
	case GMIME_RTF_COMPLIANCE_VIOLATION_UNDEFINED_COLOR:
		return GMIME_COMPLIANCE_SEVERITY_MINOR;
	case GMIME_RTF_COMPLIANCE_VIOLATION_CONTENT_AFTER_DOCUMENT:
	case GMIME_RTF_COMPLIANCE_VIOLATION_INVALID_FONT_SIZE:
		return GMIME_COMPLIANCE_SEVERITY_MAJOR;
	case GMIME_RTF_COMPLIANCE_VIOLATION_TRUNCATED_STREAM:
	case GMIME_RTF_COMPLIANCE_VIOLATION_UNSUPPORTED_CODE_PAGE:
	case GMIME_RTF_COMPLIANCE_VIOLATION_FIELD_INSTRUCTION_TOO_LONG:
		return GMIME_COMPLIANCE_SEVERITY_MAJOR;
	case GMIME_RTF_COMPLIANCE_VIOLATION_FONT_TABLE_LIMIT_EXCEEDED:
	case GMIME_RTF_COMPLIANCE_VIOLATION_COLOR_TABLE_LIMIT_EXCEEDED:
	case GMIME_RTF_COMPLIANCE_VIOLATION_GROUP_DEPTH_LIMIT_EXCEEDED:
	case GMIME_RTF_COMPLIANCE_VIOLATION_TOO_MANY_COMPLIANCE_ISSUES:
		return GMIME_COMPLIANCE_SEVERITY_MAJOR;
	default:
		return GMIME_COMPLIANCE_SEVERITY_MINOR;
	}
}

/**
 * g_mime_rtf_compliance_violation_get_categories:
 * @violation: an RTF compliance violation
 *
 * Returns: the categories for @violation
 */
GMimeComplianceCategories
g_mime_rtf_compliance_violation_get_categories (GMimeRtfComplianceViolation violation)
{
	switch (violation) {
	case GMIME_RTF_COMPLIANCE_VIOLATION_MISSING_RTF_HEADER:
	case GMIME_RTF_COMPLIANCE_VIOLATION_UNBALANCED_GROUP_END:
	case GMIME_RTF_COMPLIANCE_VIOLATION_KEYWORD_TOO_LONG:
	case GMIME_RTF_COMPLIANCE_VIOLATION_INVALID_PARAMETER:
	case GMIME_RTF_COMPLIANCE_VIOLATION_UNESCAPED_EIGHT_BIT_CHARACTER:
	case GMIME_RTF_COMPLIANCE_VIOLATION_UNDEFINED_FONT:
	case GMIME_RTF_COMPLIANCE_VIOLATION_UNDEFINED_COLOR:
		return GMIME_COMPLIANCE_CATEGORY_INTEROPERABILITY;
	case GMIME_RTF_COMPLIANCE_VIOLATION_INVALID_HEX_ESCAPE:
	case GMIME_RTF_COMPLIANCE_VIOLATION_UNPAIRED_SURROGATE:
	case GMIME_RTF_COMPLIANCE_VIOLATION_UNSUPPORTED_CODE_PAGE:
		return GMIME_COMPLIANCE_CATEGORY_INTEROPERABILITY | GMIME_COMPLIANCE_CATEGORY_DATA_LOSS;
	case GMIME_RTF_COMPLIANCE_VIOLATION_CONTENT_AFTER_DOCUMENT:
	case GMIME_RTF_COMPLIANCE_VIOLATION_INVALID_FONT_SIZE:
		return GMIME_COMPLIANCE_CATEGORY_INTEROPERABILITY | GMIME_COMPLIANCE_CATEGORY_SECURITY;
	case GMIME_RTF_COMPLIANCE_VIOLATION_TRUNCATED_STREAM:
	case GMIME_RTF_COMPLIANCE_VIOLATION_FIELD_INSTRUCTION_TOO_LONG:
		return GMIME_COMPLIANCE_CATEGORY_DATA_LOSS;
	case GMIME_RTF_COMPLIANCE_VIOLATION_FONT_TABLE_LIMIT_EXCEEDED:
	case GMIME_RTF_COMPLIANCE_VIOLATION_COLOR_TABLE_LIMIT_EXCEEDED:
	case GMIME_RTF_COMPLIANCE_VIOLATION_GROUP_DEPTH_LIMIT_EXCEEDED:
		return GMIME_COMPLIANCE_CATEGORY_DATA_LOSS | GMIME_COMPLIANCE_CATEGORY_SECURITY;
	case GMIME_RTF_COMPLIANCE_VIOLATION_TOO_MANY_COMPLIANCE_ISSUES:
		return GMIME_COMPLIANCE_CATEGORY_SECURITY;
	default:
		return 0;
	}
}

/**
 * g_mime_rtf_compliance_violation_get_description:
 * @violation: an RTF compliance violation
 *
 * Returns: the description for @violation
 */
const char *
g_mime_rtf_compliance_violation_get_description (GMimeRtfComplianceViolation violation)
{
	switch (violation) {
	case GMIME_RTF_COMPLIANCE_VIOLATION_MISSING_RTF_HEADER:
		return "The document did not begin with the {\\rtf header.";
	case GMIME_RTF_COMPLIANCE_VIOLATION_UNBALANCED_GROUP_END:
		return "A group end did not match any group start.";
	case GMIME_RTF_COMPLIANCE_VIOLATION_CONTENT_AFTER_DOCUMENT:
		return "Content followed the end of the outermost group.";
	case GMIME_RTF_COMPLIANCE_VIOLATION_TRUNCATED_STREAM:
		return "The document ended before all of its groups were closed.";
	case GMIME_RTF_COMPLIANCE_VIOLATION_KEYWORD_TOO_LONG:
		return "A control word was longer than 32 letters.";
	case GMIME_RTF_COMPLIANCE_VIOLATION_INVALID_PARAMETER:
		return "A control word had a malformed or out-of-range numeric parameter.";
	case GMIME_RTF_COMPLIANCE_VIOLATION_INVALID_HEX_ESCAPE:
		return "A \\' escape was not followed by two hexadecimal digits.";
	case GMIME_RTF_COMPLIANCE_VIOLATION_UNESCAPED_EIGHT_BIT_CHARACTER:
		return "The document contained a non-ASCII character that was not escaped.";
	case GMIME_RTF_COMPLIANCE_VIOLATION_UNPAIRED_SURROGATE:
		return "A \\uN control word specified one half of a UTF-16 surrogate pair without the other.";
	case GMIME_RTF_COMPLIANCE_VIOLATION_UNSUPPORTED_CODE_PAGE:
		return "A code page specified by the document could not be resolved.";
	case GMIME_RTF_COMPLIANCE_VIOLATION_UNDEFINED_FONT:
		return "A \\fN control word referred to a font that was not defined in the font table.";
	case GMIME_RTF_COMPLIANCE_VIOLATION_UNDEFINED_COLOR:
		return "A \\cfN or \\cbN control word referred to a color that was not defined in the color table.";
	case GMIME_RTF_COMPLIANCE_VIOLATION_INVALID_FONT_SIZE:
		return "A \\fsN control word specified a font size that was not positive or was too large.";
	case GMIME_RTF_COMPLIANCE_VIOLATION_FIELD_INSTRUCTION_TOO_LONG:
		return "A field instruction was longer than the maximum length that is examined.";
	case GMIME_RTF_COMPLIANCE_VIOLATION_FONT_TABLE_LIMIT_EXCEEDED:
		return "The font table contained more entries than the configured limit.";
	case GMIME_RTF_COMPLIANCE_VIOLATION_COLOR_TABLE_LIMIT_EXCEEDED:
		return "The color table contained more entries than the configured limit.";
	case GMIME_RTF_COMPLIANCE_VIOLATION_GROUP_DEPTH_LIMIT_EXCEEDED:
		return "Groups that change the formatting state were nested more deeply than the configured limit.";
	case GMIME_RTF_COMPLIANCE_VIOLATION_TOO_MANY_COMPLIANCE_ISSUES:
		return "Too many compliance issues were detected and the remainder were suppressed.";
	default:
		return NULL;
	}
}

/**
 * g_mime_rtf_compliance_violation_get_remarks:
 * @violation: an RTF compliance violation
 *
 * Returns: the remarks for @violation
 */
const char *
g_mime_rtf_compliance_violation_get_remarks (GMimeRtfComplianceViolation violation)
{
	switch (violation) {
	case GMIME_RTF_COMPLIANCE_VIOLATION_MISSING_RTF_HEADER:
		return "The RTF specification requires that a document begin with an opening brace followed by the \\rtfN control word. The content is interpreted as RTF regardless, but other readers may reject the document or treat it as plain text.";
	case GMIME_RTF_COMPLIANCE_VIOLATION_UNBALANCED_GROUP_END:
		return "Every closing brace must close a group that was opened by a matching opening brace. The extra closing brace is ignored.";
	case GMIME_RTF_COMPLIANCE_VIOLATION_CONTENT_AFTER_DOCUMENT:
		return "The RTF specification defines a document as a single group. Content that follows the closing brace of that group is rendered, but readers disagree about whether to render it, so it may be shown to some recipients and not to others.";
	case GMIME_RTF_COMPLIANCE_VIOLATION_TRUNCATED_STREAM:
		return "The document ended within a group, or within the binary data of a \\binN control word. It was most likely truncated in transit or in storage, and whatever followed the point of truncation is lost.";
	case GMIME_RTF_COMPLIANCE_VIOLATION_KEYWORD_TOO_LONG:
		return "The RTF specification limits a control word to 32 letters. A longer control word cannot be valid, so it is treated as an unknown control word and ignored.";
	case GMIME_RTF_COMPLIANCE_VIOLATION_INVALID_PARAMETER:
		return "The RTF specification defines a control word parameter as an optional hyphen followed by digits, forming a signed 16-bit or 32-bit integer. A hyphen that is not followed by any digits is ignored, and a value that is out of range is clamped or replaced, which may not be what the writer intended.";
	case GMIME_RTF_COMPLIANCE_VIOLATION_INVALID_HEX_ESCAPE:
		return "The RTF specification requires that \\' be followed by exactly two hexadecimal digits. An escape without any hexadecimal digits is dropped, and one with a single hexadecimal digit is decoded using that digit alone, so the character is either lost or decoded incorrectly.";
	case GMIME_RTF_COMPLIANCE_VIOLATION_UNESCAPED_EIGHT_BIT_CHARACTER:
		return "RTF is a 7-bit format, so characters outside of the ASCII range are supposed to be written using \\'hh or \\uN escapes. Unescaped 8-bit characters are common in practice and are decoded using the code page of the current font, but they do not survive transports that are not 8-bit clean.";
	case GMIME_RTF_COMPLIANCE_VIOLATION_UNPAIRED_SURROGATE:
		return "The RTF specification writes characters outside of the Basic Multilingual Plane as a pair of \\uN control words holding the high and low surrogates. A surrogate without its other half does not represent a character, so it is replaced with U+FFFD REPLACEMENT CHARACTER.";
	case GMIME_RTF_COMPLIANCE_VIOLATION_UNSUPPORTED_CODE_PAGE:
		return "Text is decoded using the code page specified by \\ansicpgN, \\fcharsetN or \\cpgN. When that code page is not available, the text is decoded using a fallback encoding instead, which may decode it incorrectly.";
	case GMIME_RTF_COMPLIANCE_VIOLATION_UNDEFINED_FONT:
		return "The font determines the code page that text is decoded with. Text formatted with an undefined font is decoded using the document's default code page, which may not be what the writer intended.";
	case GMIME_RTF_COMPLIANCE_VIOLATION_UNDEFINED_COLOR:
		return "Text formatted with an undefined color is rendered using the automatic (default) color.";
	case GMIME_RTF_COMPLIANCE_VIOLATION_INVALID_FONT_SIZE:
		return "The RTF specification measures font sizes in half-points, and Microsoft Word limits them to 1638 points. A font size of zero is a well-known way to hide text from the reader of a message while leaving it visible to a content scanner, and readers disagree about how to render such text.";
	case GMIME_RTF_COMPLIANCE_VIOLATION_FIELD_INSTRUCTION_TOO_LONG:
		return "Field instructions are only examined for HYPERLINK targets, so only the first 2048 characters are kept. A longer instruction is truncated, so a hyperlink may be lost or its target may be truncated.";
	case GMIME_RTF_COMPLIANCE_VIOLATION_FONT_TABLE_LIMIT_EXCEEDED:
		return "This is not a defect in the document, but the font table entries beyond the limit were ignored, so text formatted with those fonts may have been decoded incorrectly.";
	case GMIME_RTF_COMPLIANCE_VIOLATION_COLOR_TABLE_LIMIT_EXCEEDED:
		return "This is not a defect in the document, but the color table entries beyond the limit were ignored, so text formatted with those colors was rendered using the automatic color.";
	case GMIME_RTF_COMPLIANCE_VIOLATION_GROUP_DEPTH_LIMIT_EXCEEDED:
		return "This is not a defect in the document, but the content of the groups beyond the limit was not processed.";
	case GMIME_RTF_COMPLIANCE_VIOLATION_TOO_MANY_COMPLIANCE_ISSUES:
		return "Some violation was detected more often than the configured per-violation limit, so the remainder of that violation, and of any other violation that exceeds the limit, were not reported.";
	default:
		return NULL;
	}
}
