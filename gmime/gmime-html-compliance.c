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

#include "gmime-html-compliance.h"

/**
 * SECTION: gmime-html-compliance
 * @title: HTML compliance diagnostics
 * @short_description: HTML recovery diagnostics and reporting limits
 *
 * A logger receives an issue valid for the duration of its callback. Description
 * and remarks strings are static. A zero per-violation limit means unlimited
 * reporting. Otherwise one truncation marker is emitted at the first suppressed
 * issue; changing the logger, options or limit starts a new reporting budget.
 */

struct _GMimeHtmlComplianceOptions {
	gatomicrefcount refs;
	guint maximum;
};

G_DEFINE_BOXED_TYPE (GMimeHtmlComplianceOptions, g_mime_html_compliance_options, g_mime_html_compliance_options_ref,
		     g_mime_html_compliance_options_unref)

/**
 * g_mime_html_compliance_options_new:
 *
 * Returns: (transfer full): new diagnostic options
 */
GMimeHtmlComplianceOptions *
g_mime_html_compliance_options_new (void)
{
	GMimeHtmlComplianceOptions *options = g_new0 (GMimeHtmlComplianceOptions, 1);

	g_atomic_ref_count_init (&options->refs);

	return options;
}

/**
 * g_mime_html_compliance_options_ref:
 * @options: diagnostic options
 *
 * Returns: (transfer full): @options with an additional reference
 */
GMimeHtmlComplianceOptions *
g_mime_html_compliance_options_ref (GMimeHtmlComplianceOptions *options)
{
	g_atomic_ref_count_inc (&options->refs);

	return options;
}

/**
 * g_mime_html_compliance_options_unref:
 * @options: (transfer full) (nullable): diagnostic options to release
 *
 * Releases one reference to @options.
 */
void
g_mime_html_compliance_options_unref (GMimeHtmlComplianceOptions *options)
{
	if (options && g_atomic_ref_count_dec (&options->refs))
		g_free (options);
}

/**
 * g_mime_html_compliance_options_clone:
 * @options: diagnostic options to copy
 *
 * Returns: (transfer full): an independent copy of @options
 */
GMimeHtmlComplianceOptions *
g_mime_html_compliance_options_clone (const GMimeHtmlComplianceOptions *options)
{
	GMimeHtmlComplianceOptions *clone = g_mime_html_compliance_options_new ();

	clone->maximum = options->maximum;

	return clone;
}

/**
 * g_mime_html_compliance_options_get_max_issues_per_violation:
 * @options: diagnostic options
 *
 * Gets the reporting limit for each violation. The default is zero, meaning
 * unlimited reporting.
 *
 * Returns: the maximum number of reports per violation, or zero for no limit
 **/
guint
g_mime_html_compliance_options_get_max_issues_per_violation (const GMimeHtmlComplianceOptions *options)
{
	return options->maximum;
}

/**
 * g_mime_html_compliance_options_set_max_issues_per_violation:
 * @options: diagnostic options
 * @maximum: the maximum number of reports per violation, or zero for no limit
 *
 * Sets the reporting limit. A tokenizer emits one
 * %GMIME_HTML_COMPLIANCE_VIOLATION_TOO_MANY_COMPLIANCE_ISSUES diagnostic when
 * reports are first suppressed. Changing the limit resets its reporting budget.
 **/
void
g_mime_html_compliance_options_set_max_issues_per_violation (GMimeHtmlComplianceOptions *options, guint maximum)
{
	options->maximum = maximum;
}

/**
 * g_mime_html_compliance_violation_get_severity:
 * @violation: a compliance violation
 *
 * Gets the default severity of @violation.
 *
 * Returns: the diagnostic severity
 **/
GMimeComplianceSeverity
g_mime_html_compliance_violation_get_severity (GMimeHtmlComplianceViolation violation)
{
	switch (violation) {
	// Note: Deviations that browsers recover from in a well-defined way and that other HTML
	// processors are unlikely to interpret differently in a way that matters.
	case GMIME_HTML_COMPLIANCE_VIOLATION_EOF_BEFORE_TAG_NAME:
	case GMIME_HTML_COMPLIANCE_VIOLATION_INVALID_FIRST_CHARACTER_OF_TAG_NAME:
	case GMIME_HTML_COMPLIANCE_VIOLATION_UNEXPECTED_QUESTION_MARK_INSTEAD_OF_TAG_NAME:
	case GMIME_HTML_COMPLIANCE_VIOLATION_MISSING_END_TAG_NAME:
	case GMIME_HTML_COMPLIANCE_VIOLATION_END_TAG_WITH_ATTRIBUTES:
	case GMIME_HTML_COMPLIANCE_VIOLATION_END_TAG_WITH_TRAILING_SOLIDUS:
	case GMIME_HTML_COMPLIANCE_VIOLATION_MISSING_ATTRIBUTE_VALUE:
	case GMIME_HTML_COMPLIANCE_VIOLATION_ABRUPT_DOC_TYPE_PUBLIC_IDENTIFIER:
	case GMIME_HTML_COMPLIANCE_VIOLATION_ABRUPT_DOC_TYPE_SYSTEM_IDENTIFIER:
	case GMIME_HTML_COMPLIANCE_VIOLATION_EOF_IN_DOC_TYPE:
	case GMIME_HTML_COMPLIANCE_VIOLATION_INVALID_CHARACTER_SEQUENCE_AFTER_DOC_TYPE_NAME:
	case GMIME_HTML_COMPLIANCE_VIOLATION_MISSING_DOC_TYPE_NAME:
	case GMIME_HTML_COMPLIANCE_VIOLATION_MISSING_DOC_TYPE_PUBLIC_IDENTIFIER:
	case GMIME_HTML_COMPLIANCE_VIOLATION_MISSING_DOC_TYPE_SYSTEM_IDENTIFIER:
	case GMIME_HTML_COMPLIANCE_VIOLATION_MISSING_QUOTE_BEFORE_DOC_TYPE_PUBLIC_IDENTIFIER:
	case GMIME_HTML_COMPLIANCE_VIOLATION_MISSING_QUOTE_BEFORE_DOC_TYPE_SYSTEM_IDENTIFIER:
	case GMIME_HTML_COMPLIANCE_VIOLATION_MISSING_WHITESPACE_AFTER_DOC_TYPE_PUBLIC_KEYWORD:
	case GMIME_HTML_COMPLIANCE_VIOLATION_MISSING_WHITESPACE_AFTER_DOC_TYPE_SYSTEM_KEYWORD:
	case GMIME_HTML_COMPLIANCE_VIOLATION_MISSING_WHITESPACE_BEFORE_DOC_TYPE_NAME:
	case GMIME_HTML_COMPLIANCE_VIOLATION_MISSING_WHITESPACE_BETWEEN_DOC_TYPE_PUBLIC_AND_SYSTEM_IDENTIFIERS:
	case GMIME_HTML_COMPLIANCE_VIOLATION_UNEXPECTED_CHARACTER_AFTER_DOC_TYPE_SYSTEM_IDENTIFIER:
	case GMIME_HTML_COMPLIANCE_VIOLATION_ABSENCE_OF_DIGITS_IN_NUMERIC_CHARACTER_REFERENCE:
	case GMIME_HTML_COMPLIANCE_VIOLATION_CONTROL_CHARACTER_REFERENCE:
	case GMIME_HTML_COMPLIANCE_VIOLATION_MISSING_SEMICOLON_AFTER_CHARACTER_REFERENCE:
	case GMIME_HTML_COMPLIANCE_VIOLATION_NONCHARACTER_CHARACTER_REFERENCE:
	case GMIME_HTML_COMPLIANCE_VIOLATION_UNKNOWN_NAMED_CHARACTER_REFERENCE:
	case GMIME_HTML_COMPLIANCE_VIOLATION_CHARACTER_REFERENCE_OUTSIDE_UNICODE_RANGE:
	case GMIME_HTML_COMPLIANCE_VIOLATION_NULL_CHARACTER_REFERENCE:
	case GMIME_HTML_COMPLIANCE_VIOLATION_SURROGATE_CHARACTER_REFERENCE:
		return GMIME_COMPLIANCE_SEVERITY_MINOR;

	// Note: Constructs that different HTML processors are known to interpret differently, so that
	// what a filter or scanner examined may not be what the recipient sees, or content that was lost.
	case GMIME_HTML_COMPLIANCE_VIOLATION_EOF_IN_TAG:
	case GMIME_HTML_COMPLIANCE_VIOLATION_ELEMENT_DEPTH_LIMIT_EXCEEDED:
	case GMIME_HTML_COMPLIANCE_VIOLATION_UNEXPECTED_SOLIDUS_IN_TAG:
	case GMIME_HTML_COMPLIANCE_VIOLATION_DUPLICATE_ATTRIBUTE:
	case GMIME_HTML_COMPLIANCE_VIOLATION_MISSING_WHITESPACE_BETWEEN_ATTRIBUTES:
	case GMIME_HTML_COMPLIANCE_VIOLATION_UNEXPECTED_CHARACTER_IN_ATTRIBUTE_NAME:
	case GMIME_HTML_COMPLIANCE_VIOLATION_UNEXPECTED_CHARACTER_IN_UNQUOTED_ATTRIBUTE_VALUE:
	case GMIME_HTML_COMPLIANCE_VIOLATION_UNEXPECTED_EQUALS_SIGN_BEFORE_ATTRIBUTE_NAME:
	case GMIME_HTML_COMPLIANCE_VIOLATION_ABRUPT_CLOSING_OF_EMPTY_COMMENT:
	case GMIME_HTML_COMPLIANCE_VIOLATION_EOF_IN_COMMENT:
	case GMIME_HTML_COMPLIANCE_VIOLATION_INCORRECTLY_CLOSED_COMMENT:
	case GMIME_HTML_COMPLIANCE_VIOLATION_INCORRECTLY_OPENED_COMMENT:
	case GMIME_HTML_COMPLIANCE_VIOLATION_NESTED_COMMENT:
	case GMIME_HTML_COMPLIANCE_VIOLATION_C_DATA_IN_HTML_CONTENT:
	case GMIME_HTML_COMPLIANCE_VIOLATION_EOF_IN_SCRIPT_HTML_COMMENT_LIKE_TEXT:
	case GMIME_HTML_COMPLIANCE_VIOLATION_UNEXPECTED_NULL_CHARACTER:
	case GMIME_HTML_COMPLIANCE_VIOLATION_EOF_IN_C_DATA:
	case GMIME_HTML_COMPLIANCE_VIOLATION_IGNORED_START_TAG:
	case GMIME_HTML_COMPLIANCE_VIOLATION_FOREIGN_CONTENT_BREAKOUT:
	case GMIME_HTML_COMPLIANCE_VIOLATION_ATTRIBUTES_MERGED_INTO_EXISTING_ELEMENT:
		return GMIME_COMPLIANCE_SEVERITY_MAJOR;

	// Note: Not a defect in the document, but a consumer that only looks at Major and above needs
	// to know that the report is incomplete.
	case GMIME_HTML_COMPLIANCE_VIOLATION_TOO_MANY_COMPLIANCE_ISSUES:
		return GMIME_COMPLIANCE_SEVERITY_MAJOR;

	default:
		return GMIME_COMPLIANCE_SEVERITY_MINOR;
	}
}

/**
 * g_mime_html_compliance_violation_get_categories:
 * @violation: a compliance violation
 *
 * Gets the diagnostic categories associated with @violation.
 *
 * Returns: the diagnostic category flags
 **/
GMimeComplianceCategories
g_mime_html_compliance_violation_get_categories (GMimeHtmlComplianceViolation violation)
{
	const GMimeComplianceCategories Interop = GMIME_COMPLIANCE_CATEGORY_INTEROPERABILITY;
	const GMimeComplianceCategories DataLoss = GMIME_COMPLIANCE_CATEGORY_DATA_LOSS;
	const GMimeComplianceCategories Security = GMIME_COMPLIANCE_CATEGORY_SECURITY;

	switch (violation) {
	case GMIME_HTML_COMPLIANCE_VIOLATION_EOF_BEFORE_TAG_NAME:
	case GMIME_HTML_COMPLIANCE_VIOLATION_INVALID_FIRST_CHARACTER_OF_TAG_NAME:
	case GMIME_HTML_COMPLIANCE_VIOLATION_UNEXPECTED_QUESTION_MARK_INSTEAD_OF_TAG_NAME:
	case GMIME_HTML_COMPLIANCE_VIOLATION_MISSING_END_TAG_NAME:
	case GMIME_HTML_COMPLIANCE_VIOLATION_END_TAG_WITH_ATTRIBUTES:
	case GMIME_HTML_COMPLIANCE_VIOLATION_END_TAG_WITH_TRAILING_SOLIDUS:
	case GMIME_HTML_COMPLIANCE_VIOLATION_MISSING_ATTRIBUTE_VALUE:
	case GMIME_HTML_COMPLIANCE_VIOLATION_ABRUPT_DOC_TYPE_PUBLIC_IDENTIFIER:
	case GMIME_HTML_COMPLIANCE_VIOLATION_ABRUPT_DOC_TYPE_SYSTEM_IDENTIFIER:
	case GMIME_HTML_COMPLIANCE_VIOLATION_EOF_IN_DOC_TYPE:
	case GMIME_HTML_COMPLIANCE_VIOLATION_INVALID_CHARACTER_SEQUENCE_AFTER_DOC_TYPE_NAME:
	case GMIME_HTML_COMPLIANCE_VIOLATION_MISSING_DOC_TYPE_NAME:
	case GMIME_HTML_COMPLIANCE_VIOLATION_MISSING_DOC_TYPE_PUBLIC_IDENTIFIER:
	case GMIME_HTML_COMPLIANCE_VIOLATION_MISSING_DOC_TYPE_SYSTEM_IDENTIFIER:
	case GMIME_HTML_COMPLIANCE_VIOLATION_MISSING_QUOTE_BEFORE_DOC_TYPE_PUBLIC_IDENTIFIER:
	case GMIME_HTML_COMPLIANCE_VIOLATION_MISSING_QUOTE_BEFORE_DOC_TYPE_SYSTEM_IDENTIFIER:
	case GMIME_HTML_COMPLIANCE_VIOLATION_MISSING_WHITESPACE_AFTER_DOC_TYPE_PUBLIC_KEYWORD:
	case GMIME_HTML_COMPLIANCE_VIOLATION_MISSING_WHITESPACE_AFTER_DOC_TYPE_SYSTEM_KEYWORD:
	case GMIME_HTML_COMPLIANCE_VIOLATION_MISSING_WHITESPACE_BEFORE_DOC_TYPE_NAME:
	case GMIME_HTML_COMPLIANCE_VIOLATION_MISSING_WHITESPACE_BETWEEN_DOC_TYPE_PUBLIC_AND_SYSTEM_IDENTIFIERS:
	case GMIME_HTML_COMPLIANCE_VIOLATION_UNEXPECTED_CHARACTER_AFTER_DOC_TYPE_SYSTEM_IDENTIFIER:
	case GMIME_HTML_COMPLIANCE_VIOLATION_ABSENCE_OF_DIGITS_IN_NUMERIC_CHARACTER_REFERENCE:
	case GMIME_HTML_COMPLIANCE_VIOLATION_CONTROL_CHARACTER_REFERENCE:
	case GMIME_HTML_COMPLIANCE_VIOLATION_MISSING_SEMICOLON_AFTER_CHARACTER_REFERENCE:
	case GMIME_HTML_COMPLIANCE_VIOLATION_NONCHARACTER_CHARACTER_REFERENCE:
	case GMIME_HTML_COMPLIANCE_VIOLATION_UNKNOWN_NAMED_CHARACTER_REFERENCE:
		return Interop;

	// Note: Content that was lost or went unprocessed is content that went unscanned.
	case GMIME_HTML_COMPLIANCE_VIOLATION_EOF_IN_TAG:
	case GMIME_HTML_COMPLIANCE_VIOLATION_ELEMENT_DEPTH_LIMIT_EXCEEDED:
		return DataLoss | Security;

	// Note: Different HTML processors may disagree about the structure of the document, so what a
	// filter or scanner examines may not be what the recipient is shown.
	case GMIME_HTML_COMPLIANCE_VIOLATION_UNEXPECTED_SOLIDUS_IN_TAG:
	case GMIME_HTML_COMPLIANCE_VIOLATION_DUPLICATE_ATTRIBUTE:
	case GMIME_HTML_COMPLIANCE_VIOLATION_MISSING_WHITESPACE_BETWEEN_ATTRIBUTES:
	case GMIME_HTML_COMPLIANCE_VIOLATION_UNEXPECTED_CHARACTER_IN_ATTRIBUTE_NAME:
	case GMIME_HTML_COMPLIANCE_VIOLATION_UNEXPECTED_CHARACTER_IN_UNQUOTED_ATTRIBUTE_VALUE:
	case GMIME_HTML_COMPLIANCE_VIOLATION_UNEXPECTED_EQUALS_SIGN_BEFORE_ATTRIBUTE_NAME:
	case GMIME_HTML_COMPLIANCE_VIOLATION_ABRUPT_CLOSING_OF_EMPTY_COMMENT:
	case GMIME_HTML_COMPLIANCE_VIOLATION_EOF_IN_COMMENT:
	case GMIME_HTML_COMPLIANCE_VIOLATION_INCORRECTLY_CLOSED_COMMENT:
	case GMIME_HTML_COMPLIANCE_VIOLATION_INCORRECTLY_OPENED_COMMENT:
	case GMIME_HTML_COMPLIANCE_VIOLATION_NESTED_COMMENT:
	case GMIME_HTML_COMPLIANCE_VIOLATION_C_DATA_IN_HTML_CONTENT:
	case GMIME_HTML_COMPLIANCE_VIOLATION_EOF_IN_SCRIPT_HTML_COMMENT_LIKE_TEXT:
	case GMIME_HTML_COMPLIANCE_VIOLATION_UNEXPECTED_NULL_CHARACTER:
	case GMIME_HTML_COMPLIANCE_VIOLATION_IGNORED_START_TAG:
	case GMIME_HTML_COMPLIANCE_VIOLATION_FOREIGN_CONTENT_BREAKOUT:
	case GMIME_HTML_COMPLIANCE_VIOLATION_ATTRIBUTES_MERGED_INTO_EXISTING_ELEMENT:
		return Interop | Security;

	// Note: A character is decoded as U+FFFD REPLACEMENT CHARACTER.
	case GMIME_HTML_COMPLIANCE_VIOLATION_CHARACTER_REFERENCE_OUTSIDE_UNICODE_RANGE:
	case GMIME_HTML_COMPLIANCE_VIOLATION_NULL_CHARACTER_REFERENCE:
	case GMIME_HTML_COMPLIANCE_VIOLATION_SURROGATE_CHARACTER_REFERENCE:
		return Interop | DataLoss;

	// Note: Not likely to be interpreted differently, but content has been lost.
	case GMIME_HTML_COMPLIANCE_VIOLATION_EOF_IN_C_DATA:
		return DataLoss;

	// Note: A suppressed report is a blind spot, and hiding a violation from whatever is
	// inspecting the document is exactly what an attacker would want out of a flood of cheap ones.
	case GMIME_HTML_COMPLIANCE_VIOLATION_TOO_MANY_COMPLIANCE_ISSUES:
		return Security;

	default:
		return GMIME_COMPLIANCE_CATEGORY_NONE;
	}
}

/**
 * g_mime_html_compliance_violation_get_description:
 * @violation: a compliance violation
 *
 * Returns: (transfer none): a static description of @violation
 */
const char *
g_mime_html_compliance_violation_get_description (GMimeHtmlComplianceViolation violation)
{
	switch (violation) {
	case GMIME_HTML_COMPLIANCE_VIOLATION_EOF_BEFORE_TAG_NAME:
		return "The input ended immediately after a < or </.";
	case GMIME_HTML_COMPLIANCE_VIOLATION_INVALID_FIRST_CHARACTER_OF_TAG_NAME:
		return "A < or </ was followed by a character that cannot start a tag name.";
	case GMIME_HTML_COMPLIANCE_VIOLATION_UNEXPECTED_QUESTION_MARK_INSTEAD_OF_TAG_NAME:
		return "A < was followed by a question mark.";
	case GMIME_HTML_COMPLIANCE_VIOLATION_MISSING_END_TAG_NAME:
		return "An end tag had no name (</>).";
	case GMIME_HTML_COMPLIANCE_VIOLATION_EOF_IN_TAG:
		return "The input ended inside of a tag.";
	case GMIME_HTML_COMPLIANCE_VIOLATION_UNEXPECTED_SOLIDUS_IN_TAG:
		return "A / inside of a tag was not immediately followed by >.";
	case GMIME_HTML_COMPLIANCE_VIOLATION_END_TAG_WITH_ATTRIBUTES:
		return "An end tag had attributes.";
	case GMIME_HTML_COMPLIANCE_VIOLATION_END_TAG_WITH_TRAILING_SOLIDUS:
		return "An end tag had a trailing /.";
	case GMIME_HTML_COMPLIANCE_VIOLATION_DUPLICATE_ATTRIBUTE:
		return "A tag had more than one attribute with the same name.";
	case GMIME_HTML_COMPLIANCE_VIOLATION_MISSING_ATTRIBUTE_VALUE:
		return "An attribute name and = were not followed by a value.";
	case GMIME_HTML_COMPLIANCE_VIOLATION_MISSING_WHITESPACE_BETWEEN_ATTRIBUTES:
		return "A quoted attribute value was not followed by whitespace.";
	case GMIME_HTML_COMPLIANCE_VIOLATION_UNEXPECTED_CHARACTER_IN_ATTRIBUTE_NAME:
		return "An attribute name contained a \", ', or < character.";
	case GMIME_HTML_COMPLIANCE_VIOLATION_UNEXPECTED_CHARACTER_IN_UNQUOTED_ATTRIBUTE_VALUE:
		return "An unquoted attribute value contained a \", ', <, =, or ` character.";
	case GMIME_HTML_COMPLIANCE_VIOLATION_UNEXPECTED_EQUALS_SIGN_BEFORE_ATTRIBUTE_NAME:
		return "An attribute name began with =.";
	case GMIME_HTML_COMPLIANCE_VIOLATION_ABRUPT_CLOSING_OF_EMPTY_COMMENT:
		return "A comment was closed by <!--> or <!--->.";
	case GMIME_HTML_COMPLIANCE_VIOLATION_EOF_IN_COMMENT:
		return "The input ended inside of a comment.";
	case GMIME_HTML_COMPLIANCE_VIOLATION_INCORRECTLY_CLOSED_COMMENT:
		return "A comment was closed by --!> instead of -->.";
	case GMIME_HTML_COMPLIANCE_VIOLATION_INCORRECTLY_OPENED_COMMENT:
		return "A <! was not followed by --, DOCTYPE or [CDATA[.";
	case GMIME_HTML_COMPLIANCE_VIOLATION_NESTED_COMMENT:
		return "A comment contained <!--.";
	case GMIME_HTML_COMPLIANCE_VIOLATION_ABRUPT_DOC_TYPE_PUBLIC_IDENTIFIER:
		return "A DOCTYPE public identifier was terminated by > before its closing quote.";
	case GMIME_HTML_COMPLIANCE_VIOLATION_ABRUPT_DOC_TYPE_SYSTEM_IDENTIFIER:
		return "A DOCTYPE system identifier was terminated by > before its closing quote.";
	case GMIME_HTML_COMPLIANCE_VIOLATION_EOF_IN_DOC_TYPE:
		return "The input ended inside of a DOCTYPE.";
	case GMIME_HTML_COMPLIANCE_VIOLATION_INVALID_CHARACTER_SEQUENCE_AFTER_DOC_TYPE_NAME:
		return "A DOCTYPE name was followed by something other than PUBLIC, SYSTEM or >.";
	case GMIME_HTML_COMPLIANCE_VIOLATION_MISSING_DOC_TYPE_NAME:
		return "A DOCTYPE had no name.";
	case GMIME_HTML_COMPLIANCE_VIOLATION_MISSING_DOC_TYPE_PUBLIC_IDENTIFIER:
		return "The PUBLIC keyword of a DOCTYPE was not followed by a public identifier.";
	case GMIME_HTML_COMPLIANCE_VIOLATION_MISSING_DOC_TYPE_SYSTEM_IDENTIFIER:
		return "The SYSTEM keyword of a DOCTYPE was not followed by a system identifier.";
	case GMIME_HTML_COMPLIANCE_VIOLATION_MISSING_QUOTE_BEFORE_DOC_TYPE_PUBLIC_IDENTIFIER:
		return "A DOCTYPE public identifier was not quoted.";
	case GMIME_HTML_COMPLIANCE_VIOLATION_MISSING_QUOTE_BEFORE_DOC_TYPE_SYSTEM_IDENTIFIER:
		return "A DOCTYPE system identifier was not quoted.";
	case GMIME_HTML_COMPLIANCE_VIOLATION_MISSING_WHITESPACE_AFTER_DOC_TYPE_PUBLIC_KEYWORD:
		return "The PUBLIC keyword of a DOCTYPE was not followed by whitespace.";
	case GMIME_HTML_COMPLIANCE_VIOLATION_MISSING_WHITESPACE_AFTER_DOC_TYPE_SYSTEM_KEYWORD:
		return "The SYSTEM keyword of a DOCTYPE was not followed by whitespace.";
	case GMIME_HTML_COMPLIANCE_VIOLATION_MISSING_WHITESPACE_BEFORE_DOC_TYPE_NAME:
		return "The DOCTYPE keyword was not followed by whitespace.";
	case GMIME_HTML_COMPLIANCE_VIOLATION_MISSING_WHITESPACE_BETWEEN_DOC_TYPE_PUBLIC_AND_SYSTEM_IDENTIFIERS:
		return "The public and system identifiers of a DOCTYPE were not separated by whitespace.";
	case GMIME_HTML_COMPLIANCE_VIOLATION_UNEXPECTED_CHARACTER_AFTER_DOC_TYPE_SYSTEM_IDENTIFIER:
		return "A DOCTYPE system identifier was followed by something other than whitespace or >.";
	case GMIME_HTML_COMPLIANCE_VIOLATION_ABSENCE_OF_DIGITS_IN_NUMERIC_CHARACTER_REFERENCE:
		return "A numeric character reference (&# or &#x) had no digits.";
	case GMIME_HTML_COMPLIANCE_VIOLATION_CHARACTER_REFERENCE_OUTSIDE_UNICODE_RANGE:
		return "A numeric character reference referred to a code point beyond U+10FFFF.";
	case GMIME_HTML_COMPLIANCE_VIOLATION_CONTROL_CHARACTER_REFERENCE:
		return "A numeric character reference referred to a control character.";
	case GMIME_HTML_COMPLIANCE_VIOLATION_MISSING_SEMICOLON_AFTER_CHARACTER_REFERENCE:
		return "A character reference was not terminated by a semicolon.";
	case GMIME_HTML_COMPLIANCE_VIOLATION_NONCHARACTER_CHARACTER_REFERENCE:
		return "A numeric character reference referred to a Unicode noncharacter.";
	case GMIME_HTML_COMPLIANCE_VIOLATION_NULL_CHARACTER_REFERENCE:
		return "A numeric character reference referred to U+0000 NULL.";
	case GMIME_HTML_COMPLIANCE_VIOLATION_SURROGATE_CHARACTER_REFERENCE:
		return "A numeric character reference referred to a UTF-16 surrogate code point.";
	case GMIME_HTML_COMPLIANCE_VIOLATION_UNKNOWN_NAMED_CHARACTER_REFERENCE:
		return "A named character reference terminated by a semicolon did not match any known name.";
	case GMIME_HTML_COMPLIANCE_VIOLATION_C_DATA_IN_HTML_CONTENT:
		return "A CDATA section appeared outside of SVG or MathML content.";
	case GMIME_HTML_COMPLIANCE_VIOLATION_EOF_IN_C_DATA:
		return "The input ended inside of a CDATA section.";
	case GMIME_HTML_COMPLIANCE_VIOLATION_EOF_IN_SCRIPT_HTML_COMMENT_LIKE_TEXT:
		return "The input ended inside of an HTML comment-like construct within a script element.";
	case GMIME_HTML_COMPLIANCE_VIOLATION_UNEXPECTED_NULL_CHARACTER:
		return "The input contained a U+0000 NULL character.";
	case GMIME_HTML_COMPLIANCE_VIOLATION_ELEMENT_DEPTH_LIMIT_EXCEEDED:
		return "Elements were nested more deeply than the configured limit.";
	case GMIME_HTML_COMPLIANCE_VIOLATION_IGNORED_START_TAG:
		return "A start tag was ignored.";
	case GMIME_HTML_COMPLIANCE_VIOLATION_FOREIGN_CONTENT_BREAKOUT:
		return "A tag closed SVG or MathML elements by breaking out of foreign content.";
	case GMIME_HTML_COMPLIANCE_VIOLATION_ATTRIBUTES_MERGED_INTO_EXISTING_ELEMENT:
		return "The attributes of a start tag were added to an existing element.";
	case GMIME_HTML_COMPLIANCE_VIOLATION_TOO_MANY_COMPLIANCE_ISSUES:
		return "Too many compliance issues were detected and the remainder were suppressed.";
	default:
		return "Unknown violation";
	}
}

/**
 * g_mime_html_compliance_violation_get_remarks:
 * @violation: a compliance violation
 *
 * Returns: (transfer none): static remarks about @violation
 */
const char *
g_mime_html_compliance_violation_get_remarks (GMimeHtmlComplianceViolation violation)
{
	switch (violation) {
	case GMIME_HTML_COMPLIANCE_VIOLATION_EOF_BEFORE_TAG_NAME:
		return "A < or </ at the very end of the input cannot start a tag, so it is treated as text.";
	case GMIME_HTML_COMPLIANCE_VIOLATION_INVALID_FIRST_CHARACTER_OF_TAG_NAME:
		return "A tag name must begin with an ASCII letter. After <, the < is treated as text; after </, the content up to the next > is treated as a bogus comment, which browsers do not render.";
	case GMIME_HTML_COMPLIANCE_VIOLATION_UNEXPECTED_QUESTION_MARK_INSTEAD_OF_TAG_NAME:
		return "Markup such as an XML processing instruction (<?xml ...?>) is not valid in HTML. The content up to the next > is treated as a bogus comment, which browsers do not render.";
	case GMIME_HTML_COMPLIANCE_VIOLATION_MISSING_END_TAG_NAME:
		return "The </> is ignored entirely; it neither closes an element nor is rendered as text.";
	case GMIME_HTML_COMPLIANCE_VIOLATION_EOF_IN_TAG:
		return "The input ended before the tag was closed by a >. The incomplete tag is dropped by browsers, so whatever followed the point of truncation is lost. Depending on the value of HtmlTokenizer.IgnoreTruncatedTags, the tokenizer either drops it as well or returns it as text.";
	case GMIME_HTML_COMPLIANCE_VIOLATION_UNEXPECTED_SOLIDUS_IN_TAG:
		return "A / within a tag is only meaningful immediately before the closing >. Elsewhere it is ignored and treated as whitespace between attributes, which makes it a common way to separate attributes from a tag name (e.g. <img/src=...>) without using whitespace.";
	case GMIME_HTML_COMPLIANCE_VIOLATION_END_TAG_WITH_ATTRIBUTES:
		return "End tags cannot have attributes, so browsers ignore them.";
	case GMIME_HTML_COMPLIANCE_VIOLATION_END_TAG_WITH_TRAILING_SOLIDUS:
		return "End tags cannot be self-closing, so browsers ignore the /.";
	case GMIME_HTML_COMPLIANCE_VIOLATION_DUPLICATE_ATTRIBUTE:
		return "Browsers keep the first occurrence of an attribute and ignore the rest, but other HTML processors may keep the last one instead. A filter or scanner that examines a different occurrence than the browser renders can be bypassed.";
	case GMIME_HTML_COMPLIANCE_VIOLATION_MISSING_ATTRIBUTE_VALUE:
		return "An attribute such as <a href=> is treated as having an empty value.";
	case GMIME_HTML_COMPLIANCE_VIOLATION_MISSING_WHITESPACE_BETWEEN_ATTRIBUTES:
		return "Attributes must be separated by whitespace. Browsers treat the next character as the start of a new attribute regardless, but other HTML processors may not, so the attributes they see may differ from the attributes that the browser renders.";
	case GMIME_HTML_COMPLIANCE_VIOLATION_UNEXPECTED_CHARACTER_IN_ATTRIBUTE_NAME:
		return "These characters are not valid within an attribute name, but browsers treat them as part of the name. They usually indicate broken quoting, and other HTML processors may split the tag into attributes differently than the browser does.";
	case GMIME_HTML_COMPLIANCE_VIOLATION_UNEXPECTED_CHARACTER_IN_UNQUOTED_ATTRIBUTE_VALUE:
		return "These characters are not valid within an unquoted attribute value, but browsers treat them as part of the value. They usually indicate broken quoting, and some older browsers treat ` as a quote character, so different HTML processors may disagree about where the value ends.";
	case GMIME_HTML_COMPLIANCE_VIOLATION_UNEXPECTED_EQUALS_SIGN_BEFORE_ATTRIBUTE_NAME:
		return "Browsers treat the = as the first character of the attribute name, but other HTML processors may treat it as the separator between a name and a value.";
	case GMIME_HTML_COMPLIANCE_VIOLATION_ABRUPT_CLOSING_OF_EMPTY_COMMENT:
		return "Browsers treat <!--> and <!---> as complete, empty comments, but other HTML processors may treat them as the start of a comment that continues up to the next -->, hiding the content in between from them but not from the browser.";
	case GMIME_HTML_COMPLIANCE_VIOLATION_EOF_IN_COMMENT:
		return "Browsers treat everything from the start of an unterminated comment to the end of the input as part of the comment, so none of it is rendered. Other HTML processors may render it instead.";
	case GMIME_HTML_COMPLIANCE_VIOLATION_INCORRECTLY_CLOSED_COMMENT:
		return "Browsers accept --!> as the end of a comment, but other HTML processors may not, and continue the comment up to the next -->, hiding the content in between from them but not from the browser.";
	case GMIME_HTML_COMPLIANCE_VIOLATION_INCORRECTLY_OPENED_COMMENT:
		return "Browsers treat the content up to the next > as a bogus comment, which they do not render. Other HTML processors may handle it differently. This is often the result of a conditional comment such as <![if !mso]>.";
	case GMIME_HTML_COMPLIANCE_VIOLATION_NESTED_COMMENT:
		return "Comments do not nest; the first --> closes the comment regardless of how many <!-- precede it. A processor that believes comments nest will consider content to be commented out that a browser renders.";
	case GMIME_HTML_COMPLIANCE_VIOLATION_ABRUPT_DOC_TYPE_PUBLIC_IDENTIFIER:
		return "The DOCTYPE ends at the >, the public identifier is truncated, and the document is rendered in quirks mode.";
	case GMIME_HTML_COMPLIANCE_VIOLATION_ABRUPT_DOC_TYPE_SYSTEM_IDENTIFIER:
		return "The DOCTYPE ends at the >, the system identifier is truncated, and the document is rendered in quirks mode.";
	case GMIME_HTML_COMPLIANCE_VIOLATION_EOF_IN_DOC_TYPE:
		return "The DOCTYPE is closed at the end of the input and the document is rendered in quirks mode.";
	case GMIME_HTML_COMPLIANCE_VIOLATION_INVALID_CHARACTER_SEQUENCE_AFTER_DOC_TYPE_NAME:
		return "The remainder of the DOCTYPE is ignored and the document is rendered in quirks mode.";
	case GMIME_HTML_COMPLIANCE_VIOLATION_MISSING_DOC_TYPE_NAME:
		return "A DOCTYPE without a name causes the document to be rendered in quirks mode.";
	case GMIME_HTML_COMPLIANCE_VIOLATION_MISSING_DOC_TYPE_PUBLIC_IDENTIFIER:
		return "The DOCTYPE is closed and the document is rendered in quirks mode.";
	case GMIME_HTML_COMPLIANCE_VIOLATION_MISSING_DOC_TYPE_SYSTEM_IDENTIFIER:
		return "The DOCTYPE is closed and the document is rendered in quirks mode.";
	case GMIME_HTML_COMPLIANCE_VIOLATION_MISSING_QUOTE_BEFORE_DOC_TYPE_PUBLIC_IDENTIFIER:
		return "The remainder of the DOCTYPE is ignored and the document is rendered in quirks mode.";
	case GMIME_HTML_COMPLIANCE_VIOLATION_MISSING_QUOTE_BEFORE_DOC_TYPE_SYSTEM_IDENTIFIER:
		return "The remainder of the DOCTYPE is ignored and the document is rendered in quirks mode.";
	case GMIME_HTML_COMPLIANCE_VIOLATION_MISSING_WHITESPACE_AFTER_DOC_TYPE_PUBLIC_KEYWORD:
		return "Browsers parse the public identifier regardless.";
	case GMIME_HTML_COMPLIANCE_VIOLATION_MISSING_WHITESPACE_AFTER_DOC_TYPE_SYSTEM_KEYWORD:
		return "Browsers parse the system identifier regardless.";
	case GMIME_HTML_COMPLIANCE_VIOLATION_MISSING_WHITESPACE_BEFORE_DOC_TYPE_NAME:
		return "Browsers parse the DOCTYPE name regardless.";
	case GMIME_HTML_COMPLIANCE_VIOLATION_MISSING_WHITESPACE_BETWEEN_DOC_TYPE_PUBLIC_AND_SYSTEM_IDENTIFIERS:
		return "Browsers parse the system identifier regardless.";
	case GMIME_HTML_COMPLIANCE_VIOLATION_UNEXPECTED_CHARACTER_AFTER_DOC_TYPE_SYSTEM_IDENTIFIER:
		return "The remainder of the DOCTYPE is ignored. Unlike most other DOCTYPE errors, this does not cause the document to be rendered in quirks mode.";
	case GMIME_HTML_COMPLIANCE_VIOLATION_ABSENCE_OF_DIGITS_IN_NUMERIC_CHARACTER_REFERENCE:
		return "The reference is not decoded and is rendered as text.";
	case GMIME_HTML_COMPLIANCE_VIOLATION_CHARACTER_REFERENCE_OUTSIDE_UNICODE_RANGE:
		return "There is no such character, so the reference is decoded as U+FFFD REPLACEMENT CHARACTER.";
	case GMIME_HTML_COMPLIANCE_VIOLATION_CONTROL_CHARACTER_REFERENCE:
		return "Control characters other than ASCII whitespace are not allowed in HTML. For historical reasons, references to the C1 controls (U+0080 through U+009F) are decoded as the windows-1252 characters with the same values, and other control characters are decoded as-is.";
	case GMIME_HTML_COMPLIANCE_VIOLATION_MISSING_SEMICOLON_AFTER_CHARACTER_REFERENCE:
		return "Browsers decode numeric references and a legacy set of named references (such as &amp and &copy) regardless, except within attribute values when a named reference is followed by = or an ASCII letter or digit (e.g. &copy=1 in a URL). Other HTML processors may decode them differently.";
	case GMIME_HTML_COMPLIANCE_VIOLATION_NONCHARACTER_CHARACTER_REFERENCE:
		return "Noncharacters are permanently reserved for internal use and are not allowed in HTML. Browsers decode the reference as-is regardless.";
	case GMIME_HTML_COMPLIANCE_VIOLATION_NULL_CHARACTER_REFERENCE:
		return "NULL characters are not allowed in HTML, so the reference is decoded as U+FFFD REPLACEMENT CHARACTER.";
	case GMIME_HTML_COMPLIANCE_VIOLATION_SURROGATE_CHARACTER_REFERENCE:
		return "Surrogate code points are not characters, so the reference is decoded as U+FFFD REPLACEMENT CHARACTER.";
	case GMIME_HTML_COMPLIANCE_VIOLATION_UNKNOWN_NAMED_CHARACTER_REFERENCE:
		return "The reference is not decoded and is rendered as text.";
	case GMIME_HTML_COMPLIANCE_VIOLATION_C_DATA_IN_HTML_CONTENT:
		return "CDATA sections are only allowed within SVG and MathML content. Elsewhere, browsers treat <![CDATA[ as the start of a bogus comment that ends at the next > rather than at ]]>, so other HTML processors may disagree about where it ends and what content it hides.";
	case GMIME_HTML_COMPLIANCE_VIOLATION_EOF_IN_C_DATA:
		return "The CDATA section is closed at the end of the input. The input was most likely truncated, and whatever followed the point of truncation is lost.";
	case GMIME_HTML_COMPLIANCE_VIOLATION_EOF_IN_SCRIPT_HTML_COMMENT_LIKE_TEXT:
		return "Within a <script> element, text such as <!-- <script> changes how the end of the script is detected, so the script may continue to the end of the input, consuming content that a different processor would render as markup.";
	case GMIME_HTML_COMPLIANCE_VIOLATION_UNEXPECTED_NULL_CHARACTER:
		return "NULL characters are not allowed in HTML. Depending on where they appear, browsers either ignore them or replace them with U+FFFD REPLACEMENT CHARACTER, while other HTML processors may truncate the content at the NULL.";
	case GMIME_HTML_COMPLIANCE_VIOLATION_ELEMENT_DEPTH_LIMIT_EXCEEDED:
		return "This is not a defect in the document, but the remainder of the input was treated as text rather than markup.";
	case GMIME_HTML_COMPLIANCE_VIOLATION_IGNORED_START_TAG:
		return "Browsers do not insert an element for some start tags that appear where they are not allowed, e.g. a second <head>, a <frameset> after the body content has started, a nested <form>, a table structure start tag such as <tr> outside of a table, or any start tag other than <html>, <frameset>, <frame> or <noframes> in a frameset document. Since the ignored start tag does not change how the content that follows it gets tokenized, other HTML processors that do honor it may disagree about the structure of the document.";
	case GMIME_HTML_COMPLIANCE_VIOLATION_FOREIGN_CONTENT_BREAKOUT:
		return "Within SVG or MathML content (other than at an integration point), start tags such as <p>, <b>, <img> or <font color>, as well as the </p> and </br> end tags, implicitly close all of the open SVG and MathML elements and get processed as HTML. Since this changes whether the content that follows gets tokenized as HTML or as foreign content (e.g. how <style> or <![CDATA[ are handled), it is a common technique for smuggling markup past HTML sanitizers.";
	case GMIME_HTML_COMPLIANCE_VIOLATION_ATTRIBUTES_MERGED_INTO_EXISTING_ELEMENT:
		return "An <html> or <body> start tag that appears after the element has already been inserted (explicitly or implicitly) does not insert another element. Instead, browsers add any of its attributes that the existing element does not already have to that element. This allows attributes such as event handlers to be attached to the document's <body> element from anywhere in the document, which HTML processors that discard the duplicate start tag will not expect.";
	case GMIME_HTML_COMPLIANCE_VIOLATION_TOO_MANY_COMPLIANCE_ISSUES:
		return "Some violation was detected more often than the configured per-violation limit, so the remainder of that violation, and of any other violation that exceeds the limit, were not reported.";
	default:
		return "Unknown violation";
	}
}
