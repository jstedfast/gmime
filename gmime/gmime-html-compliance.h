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

#ifndef __GMIME_HTML_COMPLIANCE_H__
#define __GMIME_HTML_COMPLIANCE_H__

#include <gmime/gmime-compliance.h>

G_BEGIN_DECLS

/**
 * GMimeHtmlComplianceViolation:
 * @GMIME_HTML_COMPLIANCE_VIOLATION_NONE: No violation.
 * @GMIME_HTML_COMPLIANCE_VIOLATION_EOF_BEFORE_TAG_NAME: Input ended immediately after a tag opener.
 * @GMIME_HTML_COMPLIANCE_VIOLATION_INVALID_FIRST_CHARACTER_OF_TAG_NAME: A tag opener was followed by an invalid first name character.
 * @GMIME_HTML_COMPLIANCE_VIOLATION_UNEXPECTED_QUESTION_MARK_INSTEAD_OF_TAG_NAME: A tag opener was followed by a question mark.
 * @GMIME_HTML_COMPLIANCE_VIOLATION_MISSING_END_TAG_NAME: An end tag had no name.
 * @GMIME_HTML_COMPLIANCE_VIOLATION_EOF_IN_TAG: Input ended inside a tag.
 * @GMIME_HTML_COMPLIANCE_VIOLATION_UNEXPECTED_SOLIDUS_IN_TAG: A slash inside a tag was not followed by its closing greater-than sign.
 * @GMIME_HTML_COMPLIANCE_VIOLATION_END_TAG_WITH_ATTRIBUTES: An end tag had attributes.
 * @GMIME_HTML_COMPLIANCE_VIOLATION_END_TAG_WITH_TRAILING_SOLIDUS: An end tag had a trailing slash.
 * @GMIME_HTML_COMPLIANCE_VIOLATION_DUPLICATE_ATTRIBUTE: A tag repeated an attribute name.
 * @GMIME_HTML_COMPLIANCE_VIOLATION_MISSING_ATTRIBUTE_VALUE: An attribute name and equals sign were not followed by a value.
 * @GMIME_HTML_COMPLIANCE_VIOLATION_MISSING_WHITESPACE_BETWEEN_ATTRIBUTES: A quoted attribute value was not followed by whitespace.
 * @GMIME_HTML_COMPLIANCE_VIOLATION_UNEXPECTED_CHARACTER_IN_ATTRIBUTE_NAME: An attribute name contained a quote or less-than sign.
 * @GMIME_HTML_COMPLIANCE_VIOLATION_UNEXPECTED_CHARACTER_IN_UNQUOTED_ATTRIBUTE_VALUE: An unquoted attribute value contained a forbidden character.
 * @GMIME_HTML_COMPLIANCE_VIOLATION_UNEXPECTED_EQUALS_SIGN_BEFORE_ATTRIBUTE_NAME: An attribute name began with an equals sign.
 * @GMIME_HTML_COMPLIANCE_VIOLATION_ABRUPT_CLOSING_OF_EMPTY_COMMENT: An empty comment was closed prematurely.
 * @GMIME_HTML_COMPLIANCE_VIOLATION_EOF_IN_COMMENT: Input ended inside a comment.
 * @GMIME_HTML_COMPLIANCE_VIOLATION_INCORRECTLY_CLOSED_COMMENT: A comment closer contained an unexpected exclamation mark.
 * @GMIME_HTML_COMPLIANCE_VIOLATION_INCORRECTLY_OPENED_COMMENT: A markup-declaration opener was not followed by a recognized declaration.
 * @GMIME_HTML_COMPLIANCE_VIOLATION_NESTED_COMMENT: A comment contained another comment opener.
 * @GMIME_HTML_COMPLIANCE_VIOLATION_ABRUPT_DOC_TYPE_PUBLIC_IDENTIFIER: A greater-than sign terminated a DOCTYPE public identifier before its closing quote.
 * @GMIME_HTML_COMPLIANCE_VIOLATION_ABRUPT_DOC_TYPE_SYSTEM_IDENTIFIER: A greater-than sign terminated a DOCTYPE system identifier before its closing quote.
 * @GMIME_HTML_COMPLIANCE_VIOLATION_EOF_IN_DOC_TYPE: Input ended inside a DOCTYPE.
 * @GMIME_HTML_COMPLIANCE_VIOLATION_INVALID_CHARACTER_SEQUENCE_AFTER_DOC_TYPE_NAME: A DOCTYPE name was followed by an invalid keyword or character.
 * @GMIME_HTML_COMPLIANCE_VIOLATION_MISSING_DOC_TYPE_NAME: A DOCTYPE had no name.
 * @GMIME_HTML_COMPLIANCE_VIOLATION_MISSING_DOC_TYPE_PUBLIC_IDENTIFIER: A DOCTYPE PUBLIC keyword was not followed by a public identifier.
 * @GMIME_HTML_COMPLIANCE_VIOLATION_MISSING_DOC_TYPE_SYSTEM_IDENTIFIER: A DOCTYPE SYSTEM keyword was not followed by a system identifier.
 * @GMIME_HTML_COMPLIANCE_VIOLATION_MISSING_QUOTE_BEFORE_DOC_TYPE_PUBLIC_IDENTIFIER: A DOCTYPE public identifier was not quoted.
 * @GMIME_HTML_COMPLIANCE_VIOLATION_MISSING_QUOTE_BEFORE_DOC_TYPE_SYSTEM_IDENTIFIER: A DOCTYPE system identifier was not quoted.
 * @GMIME_HTML_COMPLIANCE_VIOLATION_MISSING_WHITESPACE_AFTER_DOC_TYPE_PUBLIC_KEYWORD: A DOCTYPE PUBLIC keyword was not followed by whitespace.
 * @GMIME_HTML_COMPLIANCE_VIOLATION_MISSING_WHITESPACE_AFTER_DOC_TYPE_SYSTEM_KEYWORD: A DOCTYPE SYSTEM keyword was not followed by whitespace.
 * @GMIME_HTML_COMPLIANCE_VIOLATION_MISSING_WHITESPACE_BEFORE_DOC_TYPE_NAME: The DOCTYPE keyword was not followed by whitespace.
 * @GMIME_HTML_COMPLIANCE_VIOLATION_MISSING_WHITESPACE_BETWEEN_DOC_TYPE_PUBLIC_AND_SYSTEM_IDENTIFIERS: DOCTYPE public and system identifiers were not separated by whitespace.
 * @GMIME_HTML_COMPLIANCE_VIOLATION_UNEXPECTED_CHARACTER_AFTER_DOC_TYPE_SYSTEM_IDENTIFIER: A DOCTYPE system identifier was followed by an invalid character.
 * @GMIME_HTML_COMPLIANCE_VIOLATION_ABSENCE_OF_DIGITS_IN_NUMERIC_CHARACTER_REFERENCE: A numeric character reference had no digits.
 * @GMIME_HTML_COMPLIANCE_VIOLATION_CHARACTER_REFERENCE_OUTSIDE_UNICODE_RANGE: A numeric character reference exceeded U+10FFFF.
 * @GMIME_HTML_COMPLIANCE_VIOLATION_CONTROL_CHARACTER_REFERENCE: A numeric character reference named a control character.
 * @GMIME_HTML_COMPLIANCE_VIOLATION_MISSING_SEMICOLON_AFTER_CHARACTER_REFERENCE: A character reference lacked its terminating semicolon.
 * @GMIME_HTML_COMPLIANCE_VIOLATION_NONCHARACTER_CHARACTER_REFERENCE: A numeric character reference named a Unicode noncharacter.
 * @GMIME_HTML_COMPLIANCE_VIOLATION_NULL_CHARACTER_REFERENCE: A numeric character reference named U+0000.
 * @GMIME_HTML_COMPLIANCE_VIOLATION_SURROGATE_CHARACTER_REFERENCE: A numeric character reference named a UTF-16 surrogate code point.
 * @GMIME_HTML_COMPLIANCE_VIOLATION_UNKNOWN_NAMED_CHARACTER_REFERENCE: A semicolon-terminated named character reference was unrecognized.
 * @GMIME_HTML_COMPLIANCE_VIOLATION_C_DATA_IN_HTML_CONTENT: A CDATA section appeared outside SVG or MathML content.
 * @GMIME_HTML_COMPLIANCE_VIOLATION_EOF_IN_C_DATA: Input ended inside a CDATA section.
 * @GMIME_HTML_COMPLIANCE_VIOLATION_EOF_IN_SCRIPT_HTML_COMMENT_LIKE_TEXT: Input ended inside comment-like text in a script element.
 * @GMIME_HTML_COMPLIANCE_VIOLATION_UNEXPECTED_NULL_CHARACTER: Input contained U+0000.
 * @GMIME_HTML_COMPLIANCE_VIOLATION_IGNORED_START_TAG: A start tag was ignored.
 * @GMIME_HTML_COMPLIANCE_VIOLATION_FOREIGN_CONTENT_BREAKOUT: A tag broke out of foreign content, closing SVG or MathML elements.
 * @GMIME_HTML_COMPLIANCE_VIOLATION_ATTRIBUTES_MERGED_INTO_EXISTING_ELEMENT: A start tag's attributes were added to an existing element.
 * @GMIME_HTML_COMPLIANCE_VIOLATION_ELEMENT_DEPTH_LIMIT_EXCEEDED: Element nesting exceeded the configured limit.
 * @GMIME_HTML_COMPLIANCE_VIOLATION_TOO_MANY_COMPLIANCE_ISSUES: The diagnostic cap suppressed further issues.
 *
 * HTML lexical and structural recovery diagnostics. Descriptions, remarks,
 * severities and categories are available through the violation accessors.
 */
typedef enum {
	GMIME_HTML_COMPLIANCE_VIOLATION_NONE,
	GMIME_HTML_COMPLIANCE_VIOLATION_EOF_BEFORE_TAG_NAME,
	GMIME_HTML_COMPLIANCE_VIOLATION_INVALID_FIRST_CHARACTER_OF_TAG_NAME,
	GMIME_HTML_COMPLIANCE_VIOLATION_UNEXPECTED_QUESTION_MARK_INSTEAD_OF_TAG_NAME,
	GMIME_HTML_COMPLIANCE_VIOLATION_MISSING_END_TAG_NAME,
	GMIME_HTML_COMPLIANCE_VIOLATION_EOF_IN_TAG,
	GMIME_HTML_COMPLIANCE_VIOLATION_UNEXPECTED_SOLIDUS_IN_TAG,
	GMIME_HTML_COMPLIANCE_VIOLATION_END_TAG_WITH_ATTRIBUTES,
	GMIME_HTML_COMPLIANCE_VIOLATION_END_TAG_WITH_TRAILING_SOLIDUS,
	GMIME_HTML_COMPLIANCE_VIOLATION_DUPLICATE_ATTRIBUTE,
	GMIME_HTML_COMPLIANCE_VIOLATION_MISSING_ATTRIBUTE_VALUE,
	GMIME_HTML_COMPLIANCE_VIOLATION_MISSING_WHITESPACE_BETWEEN_ATTRIBUTES,
	GMIME_HTML_COMPLIANCE_VIOLATION_UNEXPECTED_CHARACTER_IN_ATTRIBUTE_NAME,
	GMIME_HTML_COMPLIANCE_VIOLATION_UNEXPECTED_CHARACTER_IN_UNQUOTED_ATTRIBUTE_VALUE,
	GMIME_HTML_COMPLIANCE_VIOLATION_UNEXPECTED_EQUALS_SIGN_BEFORE_ATTRIBUTE_NAME,
	GMIME_HTML_COMPLIANCE_VIOLATION_ABRUPT_CLOSING_OF_EMPTY_COMMENT,
	GMIME_HTML_COMPLIANCE_VIOLATION_EOF_IN_COMMENT,
	GMIME_HTML_COMPLIANCE_VIOLATION_INCORRECTLY_CLOSED_COMMENT,
	GMIME_HTML_COMPLIANCE_VIOLATION_INCORRECTLY_OPENED_COMMENT,
	GMIME_HTML_COMPLIANCE_VIOLATION_NESTED_COMMENT,
	GMIME_HTML_COMPLIANCE_VIOLATION_ABRUPT_DOC_TYPE_PUBLIC_IDENTIFIER,
	GMIME_HTML_COMPLIANCE_VIOLATION_ABRUPT_DOC_TYPE_SYSTEM_IDENTIFIER,
	GMIME_HTML_COMPLIANCE_VIOLATION_EOF_IN_DOC_TYPE,
	GMIME_HTML_COMPLIANCE_VIOLATION_INVALID_CHARACTER_SEQUENCE_AFTER_DOC_TYPE_NAME,
	GMIME_HTML_COMPLIANCE_VIOLATION_MISSING_DOC_TYPE_NAME,
	GMIME_HTML_COMPLIANCE_VIOLATION_MISSING_DOC_TYPE_PUBLIC_IDENTIFIER,
	GMIME_HTML_COMPLIANCE_VIOLATION_MISSING_DOC_TYPE_SYSTEM_IDENTIFIER,
	GMIME_HTML_COMPLIANCE_VIOLATION_MISSING_QUOTE_BEFORE_DOC_TYPE_PUBLIC_IDENTIFIER,
	GMIME_HTML_COMPLIANCE_VIOLATION_MISSING_QUOTE_BEFORE_DOC_TYPE_SYSTEM_IDENTIFIER,
	GMIME_HTML_COMPLIANCE_VIOLATION_MISSING_WHITESPACE_AFTER_DOC_TYPE_PUBLIC_KEYWORD,
	GMIME_HTML_COMPLIANCE_VIOLATION_MISSING_WHITESPACE_AFTER_DOC_TYPE_SYSTEM_KEYWORD,
	GMIME_HTML_COMPLIANCE_VIOLATION_MISSING_WHITESPACE_BEFORE_DOC_TYPE_NAME,
	GMIME_HTML_COMPLIANCE_VIOLATION_MISSING_WHITESPACE_BETWEEN_DOC_TYPE_PUBLIC_AND_SYSTEM_IDENTIFIERS,
	GMIME_HTML_COMPLIANCE_VIOLATION_UNEXPECTED_CHARACTER_AFTER_DOC_TYPE_SYSTEM_IDENTIFIER,
	GMIME_HTML_COMPLIANCE_VIOLATION_ABSENCE_OF_DIGITS_IN_NUMERIC_CHARACTER_REFERENCE,
	GMIME_HTML_COMPLIANCE_VIOLATION_CHARACTER_REFERENCE_OUTSIDE_UNICODE_RANGE,
	GMIME_HTML_COMPLIANCE_VIOLATION_CONTROL_CHARACTER_REFERENCE,
	GMIME_HTML_COMPLIANCE_VIOLATION_MISSING_SEMICOLON_AFTER_CHARACTER_REFERENCE,
	GMIME_HTML_COMPLIANCE_VIOLATION_NONCHARACTER_CHARACTER_REFERENCE,
	GMIME_HTML_COMPLIANCE_VIOLATION_NULL_CHARACTER_REFERENCE,
	GMIME_HTML_COMPLIANCE_VIOLATION_SURROGATE_CHARACTER_REFERENCE,
	GMIME_HTML_COMPLIANCE_VIOLATION_UNKNOWN_NAMED_CHARACTER_REFERENCE,
	GMIME_HTML_COMPLIANCE_VIOLATION_C_DATA_IN_HTML_CONTENT,
	GMIME_HTML_COMPLIANCE_VIOLATION_EOF_IN_C_DATA,
	GMIME_HTML_COMPLIANCE_VIOLATION_EOF_IN_SCRIPT_HTML_COMMENT_LIKE_TEXT,
	GMIME_HTML_COMPLIANCE_VIOLATION_UNEXPECTED_NULL_CHARACTER,
	GMIME_HTML_COMPLIANCE_VIOLATION_IGNORED_START_TAG,
	GMIME_HTML_COMPLIANCE_VIOLATION_FOREIGN_CONTENT_BREAKOUT,
	GMIME_HTML_COMPLIANCE_VIOLATION_ATTRIBUTES_MERGED_INTO_EXISTING_ELEMENT,
	GMIME_HTML_COMPLIANCE_VIOLATION_ELEMENT_DEPTH_LIMIT_EXCEEDED,
	GMIME_HTML_COMPLIANCE_VIOLATION_TOO_MANY_COMPLIANCE_ISSUES
} GMimeHtmlComplianceViolation;

/**
 * GMimeHtmlComplianceIssue:
 * @violation: the reported violation
 * @severity: diagnostic severity
 * @categories: diagnostic categories
 * @offset: zero-based Unicode scalar offset
 * @line_number: one-based line number
 * @line_position: one-based Unicode scalar column
 * @description: (transfer none): a borrowed description
 *
 * A diagnostic borrowed by a compliance callback. The structure is only
 * valid for the duration of that callback.
 */
typedef struct {
	GMimeHtmlComplianceViolation violation;
	GMimeComplianceSeverity severity;
	GMimeComplianceCategories categories;
	gint64 offset;
	int line_number, line_position;
	const char *description;
} GMimeHtmlComplianceIssue;

/**
 * GMimeHtmlComplianceLogger:
 * @issue: (transfer none): a diagnostic valid only during this callback
 * @user_data: (closure) (nullable): callback data
 *
 * Receives an HTML compliance diagnostic.
 */
typedef void (*GMimeHtmlComplianceLogger) (const GMimeHtmlComplianceIssue *issue, gpointer user_data);

/**
 * GMimeHtmlComplianceOptions:
 *
 * Reference-counted options controlling HTML diagnostic reporting limits.
 **/
typedef struct _GMimeHtmlComplianceOptions GMimeHtmlComplianceOptions;

#define GMIME_TYPE_HTML_COMPLIANCE_OPTIONS (g_mime_html_compliance_options_get_type ())

GType g_mime_html_compliance_options_get_type (void);

GMimeHtmlComplianceOptions *g_mime_html_compliance_options_new (void);

GMimeHtmlComplianceOptions *g_mime_html_compliance_options_ref (GMimeHtmlComplianceOptions *options);

void g_mime_html_compliance_options_unref (GMimeHtmlComplianceOptions *options);

GMimeHtmlComplianceOptions *g_mime_html_compliance_options_clone (const GMimeHtmlComplianceOptions *options);

guint g_mime_html_compliance_options_get_max_issues_per_violation (const GMimeHtmlComplianceOptions *options);
void g_mime_html_compliance_options_set_max_issues_per_violation (GMimeHtmlComplianceOptions *options, guint maximum);

const char *g_mime_html_compliance_violation_get_description (GMimeHtmlComplianceViolation violation);

const char *g_mime_html_compliance_violation_get_remarks (GMimeHtmlComplianceViolation violation);

GMimeComplianceSeverity g_mime_html_compliance_violation_get_severity (GMimeHtmlComplianceViolation violation);
GMimeComplianceCategories g_mime_html_compliance_violation_get_categories (GMimeHtmlComplianceViolation violation);

G_END_DECLS

#endif
