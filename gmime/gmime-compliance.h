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

#ifndef __GMIME_COMPLIANCE_H__
#define __GMIME_COMPLIANCE_H__

#include <glib-object.h>

G_BEGIN_DECLS

/**
 * GMimeComplianceContext:
 * @GMIME_COMPLIANCE_CONTEXT_TRANSPORT: validate mail being transported
 * @GMIME_COMPLIANCE_CONTEXT_STORAGE: validate stored mail
 *
 * Context used to determine a finding's severity, not its detection.
 **/
typedef enum {
	GMIME_COMPLIANCE_CONTEXT_TRANSPORT,
	GMIME_COMPLIANCE_CONTEXT_STORAGE
} GMimeComplianceContext;

/**
 * GMimeComplianceSeverity:
 * @GMIME_COMPLIANCE_SEVERITY_MINOR: a minor conformance defect
 * @GMIME_COMPLIANCE_SEVERITY_MAJOR: a significant interoperability defect
 * @GMIME_COMPLIANCE_SEVERITY_CRITICAL: a potentially dangerous defect
 *
 * The severity of a diagnostic in its reporting context.
 **/
typedef enum {
	GMIME_COMPLIANCE_SEVERITY_MINOR,
	GMIME_COMPLIANCE_SEVERITY_MAJOR,
	GMIME_COMPLIANCE_SEVERITY_CRITICAL
} GMimeComplianceSeverity;

/**
 * GMimeComplianceCategories:
 * @GMIME_COMPLIANCE_CATEGORY_NONE: no category
 * @GMIME_COMPLIANCE_CATEGORY_INTEROPERABILITY: interoperability concerns
 * @GMIME_COMPLIANCE_CATEGORY_DATA_LOSS: potential loss or alteration of data
 * @GMIME_COMPLIANCE_CATEGORY_SECURITY: security concerns
 *
 * Flags describing the possible consequences of a violation.
 **/
typedef enum {
	GMIME_COMPLIANCE_CATEGORY_NONE = 0,
	GMIME_COMPLIANCE_CATEGORY_INTEROPERABILITY = 1 << 0,
	GMIME_COMPLIANCE_CATEGORY_DATA_LOSS = 1 << 1,
	GMIME_COMPLIANCE_CATEGORY_SECURITY = 1 << 2
} GMimeComplianceCategories;

/**
 * GMimeComplianceValidators:
 * @GMIME_COMPLIANCE_VALIDATOR_NONE: disable specialized validators
 * @GMIME_COMPLIANCE_VALIDATOR_BASE64: validate base64 wire content
 * @GMIME_COMPLIANCE_VALIDATOR_QUOTED_PRINTABLE: validate quoted-printable content
 * @GMIME_COMPLIANCE_VALIDATOR_UUENCODE: validate uuencoded content
 * @GMIME_COMPLIANCE_VALIDATOR_ADDRESS: validate address header grammar
 * @GMIME_COMPLIANCE_VALIDATOR_PARAMETERS: validate malformed or repeated parameters and missing RFC 2231 continuation segments in Content-Type and Content-Disposition
 * @GMIME_COMPLIANCE_VALIDATOR_ALL: enable all specialized validators
 *
 * General header, structure and content checks remain enabled whenever a
 * logger is installed. These flags select additional grammar validators.
 **/
typedef enum {
	GMIME_COMPLIANCE_VALIDATOR_NONE = 0,
	GMIME_COMPLIANCE_VALIDATOR_BASE64 = 1 << 0,
	GMIME_COMPLIANCE_VALIDATOR_QUOTED_PRINTABLE = 1 << 1,
	GMIME_COMPLIANCE_VALIDATOR_UUENCODE = 1 << 2,
	GMIME_COMPLIANCE_VALIDATOR_ADDRESS = 1 << 3,
	GMIME_COMPLIANCE_VALIDATOR_PARAMETERS = 1 << 4,
	GMIME_COMPLIANCE_VALIDATOR_ALL = 31
} GMimeComplianceValidators;

/**
 * GMimeCompliancePositionKind:
 * @GMIME_COMPLIANCE_POSITION_EXACT: exact offending byte or end position
 * @GMIME_COMPLIANCE_POSITION_LINE_START: beginning of the affected line
 * @GMIME_COMPLIANCE_POSITION_ELEMENT_START: beginning of the affected element
 *
 * Interpretation of a diagnostic's input position.
 **/
typedef enum {
	GMIME_COMPLIANCE_POSITION_EXACT,
	GMIME_COMPLIANCE_POSITION_LINE_START,
	GMIME_COMPLIANCE_POSITION_ELEMENT_START
} GMimeCompliancePositionKind;

/**
 * GMimeComplianceViolation:
 * @GMIME_COMPLIANCE_NONE: no violation; not a diagnostic identity
 * @GMIME_COMPLIANCE_BARE_CARRIAGE_RETURN_IN_HEADER: bare CR in a header
 * @GMIME_COMPLIANCE_BARE_LINEFEED_IN_HEADER: bare LF in a header
 * @GMIME_COMPLIANCE_BARE_LINEFEED_IN_BODY: bare LF in a body
 * @GMIME_COMPLIANCE_OVERSIZED_LINE: physical line exceeds 1000 bytes
 * @GMIME_COMPLIANCE_UNEXPECTED_8BIT_BYTES_IN_HEADER: invalid UTF-8 header bytes
 * @GMIME_COMPLIANCE_UNEXPECTED_8BIT_BYTES_IN_BODY: non-ASCII bytes in 7-bit content
 * @GMIME_COMPLIANCE_UNEXPECTED_NULL_BYTES_IN_HEADER: NUL in a header
 * @GMIME_COMPLIANCE_UNEXPECTED_NULL_BYTES_IN_BODY: NUL in nonbinary content
 * @GMIME_COMPLIANCE_INVALID_HEADER: invalid header syntax
 * @GMIME_COMPLIANCE_INCOMPLETE_HEADER: unterminated header at EOF
 * @GMIME_COMPLIANCE_BLANK_HEADER_CONTINUATION_LINE: whitespace-only continuation
 * @GMIME_COMPLIANCE_REPEATED_CONTENT_TYPE: duplicate Content-Type
 * @GMIME_COMPLIANCE_REPEATED_CONTENT_DISPOSITION: duplicate Content-Disposition
 * @GMIME_COMPLIANCE_REPEATED_CONTENT_TRANSFER_ENCODING: duplicate transfer encoding
 * @GMIME_COMPLIANCE_REPEATED_DATE: duplicate Date
 * @GMIME_COMPLIANCE_REPEATED_FROM: duplicate From
 * @GMIME_COMPLIANCE_REPEATED_SENDER: duplicate Sender
 * @GMIME_COMPLIANCE_REPEATED_REPLY_TO: duplicate Reply-To
 * @GMIME_COMPLIANCE_REPEATED_TO: duplicate To
 * @GMIME_COMPLIANCE_REPEATED_CC: duplicate Cc
 * @GMIME_COMPLIANCE_REPEATED_BCC: duplicate Bcc
 * @GMIME_COMPLIANCE_REPEATED_MESSAGE_ID: duplicate Message-Id
 * @GMIME_COMPLIANCE_REPEATED_IN_REPLY_TO: duplicate In-Reply-To
 * @GMIME_COMPLIANCE_REPEATED_REFERENCES: duplicate References
 * @GMIME_COMPLIANCE_REPEATED_SUBJECT: duplicate Subject
 * @GMIME_COMPLIANCE_REPEATED_RETURN_PATH: duplicate Return-Path
 * @GMIME_COMPLIANCE_REPEATED_RESENT_DATE: duplicate Resent-Date within a block
 * @GMIME_COMPLIANCE_REPEATED_RESENT_FROM: duplicate Resent-From within a block
 * @GMIME_COMPLIANCE_REPEATED_RESENT_SENDER: duplicate Resent-Sender within a block
 * @GMIME_COMPLIANCE_REPEATED_RESENT_TO: duplicate Resent-To within a block
 * @GMIME_COMPLIANCE_REPEATED_RESENT_CC: duplicate Resent-Cc within a block
 * @GMIME_COMPLIANCE_REPEATED_RESENT_BCC: duplicate Resent-Bcc within a block
 * @GMIME_COMPLIANCE_REPEATED_RESENT_MESSAGE_ID: duplicate Resent-Message-Id within a block
 * @GMIME_COMPLIANCE_MISSING_DATE: missing message Date
 * @GMIME_COMPLIANCE_MISSING_FROM: missing message From
 * @GMIME_COMPLIANCE_MISSING_MESSAGE_ID: missing message Message-Id
 * @GMIME_COMPLIANCE_MISSING_MIME_VERSION: MIME headers without MIME-Version
 * @GMIME_COMPLIANCE_INVALID_CONTENT_TYPE: malformed media type, including unexpected
 *   text after type/subtype and before any parameter separator
 * @GMIME_COMPLIANCE_INVALID_CONTENT_TRANSFER_ENCODING: unrecognized transfer encoding
 * @GMIME_COMPLIANCE_ILLEGAL_MESSAGE_RFC822_CONTENT_TRANSFER_ENCODING: encoded message container
 * @GMIME_COMPLIANCE_ILLEGAL_MULTIPART_CONTENT_TRANSFER_ENCODING: illegal multipart encoding
 * @GMIME_COMPLIANCE_MISSING_MULTIPART_BOUNDARY_PARAMETER: missing boundary parameter
 * @GMIME_COMPLIANCE_INVALID_MULTIPART_BOUNDARY_PARAMETER: nonconforming boundary value
 * @GMIME_COMPLIANCE_INVALID_PARAMETER: malformed parameter declaration or fragment,
 *   reported at its element start; encoded RFC 2231 value contents are not validated
 * @GMIME_COMPLIANCE_REPEATED_BOUNDARY_PARAMETER: competing boundary declarations or repeated continuation indices
 * @GMIME_COMPLIANCE_REPEATED_FILENAME_PARAMETER: repeated name/filename declarations, allowing one ordinary fallback
 * @GMIME_COMPLIANCE_REPEATED_PARAMETER: repeated declarations of another MIME parameter
 * @GMIME_COMPLIANCE_MISSING_PARAMETER_SEGMENT: missing RFC 2231 continuation segments, reported at the first continuation declaration
 * @GMIME_COMPLIANCE_EXCESSIVE_ANGLE_BRACKETS_IN_ADDRESS: excess address brackets
 * @GMIME_COMPLIANCE_UNBALANCED_ANGLE_BRACKETS_IN_ADDRESS: unmatched address brackets
 * @GMIME_COMPLIANCE_UNBALANCED_QUOTES_IN_ADDRESS: unmatched address quotes
 * @GMIME_COMPLIANCE_UNBALANCED_PARENTHESES_IN_ADDRESS: unmatched address comment
 * @GMIME_COMPLIANCE_UNQUOTED_DISPLAY_NAME: display name requires quoting
 * @GMIME_COMPLIANCE_ADDRESS_IN_DISPLAY_NAME: address embedded in a display name
 * @GMIME_COMPLIANCE_UNQUOTED_ADDRESS_IN_DISPLAY_NAME: unquoted address in a display name
 * @GMIME_COMPLIANCE_ADDRESS_IN_GROUP_DISPLAY_NAME: address in a group name
 * @GMIME_COMPLIANCE_UNQUOTED_ADDRESS_IN_GROUP_DISPLAY_NAME: unquoted address in a group name
 * @GMIME_COMPLIANCE_INVALID_LOCAL_PART: invalid mailbox local part
 * @GMIME_COMPLIANCE_MISSING_ADDRESS_SEPARATOR: missing address-list separator
 * @GMIME_COMPLIANCE_AMBIGUOUS_MAILBOX_BOUNDARY: ambiguous mailbox separation
 * @GMIME_COMPLIANCE_EXTRANEOUS_COMMA_IN_ADDRESS_LIST: extra address-list comma
 * @GMIME_COMPLIANCE_OBSOLETE_ROUTE_ADDRESS: obsolete source-route syntax
 * @GMIME_COMPLIANCE_ADDRESS_WITHOUT_DOMAIN: domainless address
 * @GMIME_COMPLIANCE_OBSOLETE_DOMAIN_SYNTAX: obsolete domain spelling
 * @GMIME_COMPLIANCE_INVALID_DOMAIN: invalid address domain
 * @GMIME_COMPLIANCE_TRAILING_DOT_IN_DOMAIN: trailing domain dot
 * @GMIME_COMPLIANCE_WHITESPACE_IN_DOMAIN_LITERAL: whitespace in a domain literal
 * @GMIME_COMPLIANCE_INVALID_CHARACTER_IN_DOMAIN_LITERAL: invalid domain-literal byte
 * @GMIME_COMPLIANCE_INVALID_8BIT_ADDRESS: invalid non-ASCII address bytes
 * @GMIME_COMPLIANCE_MISSING_GROUP_TERMINATOR: missing group semicolon
 * @GMIME_COMPLIANCE_NON_CONFORMANT_ADDRESS: other nonconforming address syntax
 * @GMIME_COMPLIANCE_NULL_BYTE_IN_ADDRESS: NUL in an address
 * @GMIME_COMPLIANCE_NULL_BYTE_IN_DISPLAY_NAME: NUL in a display name
 * @GMIME_COMPLIANCE_LINE_BREAK_IN_ADDRESS: illegal address line break
 * @GMIME_COMPLIANCE_CONTROL_CHARACTER_IN_ADDRESS: illegal address control byte
 * @GMIME_COMPLIANCE_ISO2022_SEQUENCE_IN_LOCAL_PART: ISO-2022 local-part escape
 * @GMIME_COMPLIANCE_EMPTY_GROUP_NAME: unnamed address group
 * @GMIME_COMPLIANCE_NESTED_ADDRESS_GROUP: group nested inside another group
 * @GMIME_COMPLIANCE_MISSING_BODY_SEPARATOR: missing header/body separator
 * @GMIME_COMPLIANCE_MISSING_MULTIPART_BOUNDARY: missing multipart closing delimiter
 * @GMIME_COMPLIANCE_MIME_DEPTH_LIMIT_EXCEEDED: container flattened at the depth limit
 * @GMIME_COMPLIANCE_INCOMPLETE_BASE64_QUANTUM: incomplete base64 quantum
 * @GMIME_COMPLIANCE_INVALID_BASE64_CHARACTER: invalid base64 byte
 * @GMIME_COMPLIANCE_INVALID_BASE64_PADDING: invalid base64 padding
 * @GMIME_COMPLIANCE_BASE64_CHARACTERS_AFTER_PADDING: base64 data after padding
 * @GMIME_COMPLIANCE_OBSOLETE_BASE64_COMMENT: obsolete base64 comment
 * @GMIME_COMPLIANCE_INVALID_QUOTED_PRINTABLE_ENCODING: malformed quoted-printable escape
 * @GMIME_COMPLIANCE_INVALID_QUOTED_PRINTABLE_SOFT_BREAK: malformed soft line break
 * @GMIME_COMPLIANCE_INVALID_UUENCODE_PRETEXT: invalid uuencode preamble
 * @GMIME_COMPLIANCE_INVALID_UUENCODE_FILE_MODE: invalid uuencode file permissions
 * @GMIME_COMPLIANCE_INVALID_UUENCODED_CONTENT: invalid uuencoded byte
 * @GMIME_COMPLIANCE_INVALID_UUENCODED_LINE_LENGTH: invalid encoded line length
 * @GMIME_COMPLIANCE_INCOMPLETE_UUENCODED_LINE: truncated encoded line
 * @GMIME_COMPLIANCE_INVALID_UUENCODED_LINE_EXTRA_DATA: extra encoded line data
 * @GMIME_COMPLIANCE_INVALID_UUENCODE_END_MARKER: malformed uuencode end marker
 * @GMIME_COMPLIANCE_INCOMPLETE_UUENCODED_CONTENT: missing uuencode terminator
 * @GMIME_COMPLIANCE_TOO_MANY_COMPLIANCE_ISSUES: reporting budget was exceeded
 *
 * Diagnostic identities matching MimeKit. Values may change before the 4.0 release.
 * Use the violation
 * metadata functions for names, descriptions, remarks and context-specific severity.
 **/
typedef enum {
	GMIME_COMPLIANCE_NONE = 0,
	GMIME_COMPLIANCE_BARE_CARRIAGE_RETURN_IN_HEADER = 1,
	GMIME_COMPLIANCE_BARE_LINEFEED_IN_HEADER = 2,
	GMIME_COMPLIANCE_BARE_LINEFEED_IN_BODY = 3,
	GMIME_COMPLIANCE_OVERSIZED_LINE = 4,
	GMIME_COMPLIANCE_UNEXPECTED_8BIT_BYTES_IN_HEADER = 5,
	GMIME_COMPLIANCE_UNEXPECTED_8BIT_BYTES_IN_BODY = 6,
	GMIME_COMPLIANCE_UNEXPECTED_NULL_BYTES_IN_HEADER = 7,
	GMIME_COMPLIANCE_UNEXPECTED_NULL_BYTES_IN_BODY = 8,
	GMIME_COMPLIANCE_INVALID_HEADER = 9,
	GMIME_COMPLIANCE_INCOMPLETE_HEADER = 10,
	GMIME_COMPLIANCE_BLANK_HEADER_CONTINUATION_LINE = 11,
	GMIME_COMPLIANCE_REPEATED_CONTENT_TYPE = 12,
	GMIME_COMPLIANCE_REPEATED_CONTENT_DISPOSITION = 13,
	GMIME_COMPLIANCE_REPEATED_CONTENT_TRANSFER_ENCODING = 14,
	GMIME_COMPLIANCE_REPEATED_DATE = 15,
	GMIME_COMPLIANCE_REPEATED_FROM = 16,
	GMIME_COMPLIANCE_REPEATED_SENDER = 17,
	GMIME_COMPLIANCE_REPEATED_REPLY_TO = 18,
	GMIME_COMPLIANCE_REPEATED_TO = 19,
	GMIME_COMPLIANCE_REPEATED_CC = 20,
	GMIME_COMPLIANCE_REPEATED_BCC = 21,
	GMIME_COMPLIANCE_REPEATED_MESSAGE_ID = 22,
	GMIME_COMPLIANCE_REPEATED_IN_REPLY_TO = 23,
	GMIME_COMPLIANCE_REPEATED_REFERENCES = 24,
	GMIME_COMPLIANCE_REPEATED_SUBJECT = 25,
	GMIME_COMPLIANCE_REPEATED_RETURN_PATH = 26,
	GMIME_COMPLIANCE_REPEATED_RESENT_DATE = 27,
	GMIME_COMPLIANCE_REPEATED_RESENT_FROM = 28,
	GMIME_COMPLIANCE_REPEATED_RESENT_SENDER = 29,
	GMIME_COMPLIANCE_REPEATED_RESENT_TO = 30,
	GMIME_COMPLIANCE_REPEATED_RESENT_CC = 31,
	GMIME_COMPLIANCE_REPEATED_RESENT_BCC = 32,
	GMIME_COMPLIANCE_REPEATED_RESENT_MESSAGE_ID = 33,
	GMIME_COMPLIANCE_MISSING_DATE = 34,
	GMIME_COMPLIANCE_MISSING_FROM = 35,
	GMIME_COMPLIANCE_MISSING_MESSAGE_ID = 36,
	GMIME_COMPLIANCE_MISSING_MIME_VERSION = 37,
	GMIME_COMPLIANCE_INVALID_CONTENT_TYPE = 38,
	GMIME_COMPLIANCE_INVALID_CONTENT_TRANSFER_ENCODING = 39,
	GMIME_COMPLIANCE_ILLEGAL_MESSAGE_RFC822_CONTENT_TRANSFER_ENCODING = 40,
	GMIME_COMPLIANCE_ILLEGAL_MULTIPART_CONTENT_TRANSFER_ENCODING = 41,
	GMIME_COMPLIANCE_MISSING_MULTIPART_BOUNDARY_PARAMETER = 42,
	GMIME_COMPLIANCE_INVALID_MULTIPART_BOUNDARY_PARAMETER = 43,
	GMIME_COMPLIANCE_INVALID_PARAMETER = 44,
	GMIME_COMPLIANCE_REPEATED_BOUNDARY_PARAMETER = 45,
	GMIME_COMPLIANCE_REPEATED_FILENAME_PARAMETER = 46,
	GMIME_COMPLIANCE_REPEATED_PARAMETER = 47,
	GMIME_COMPLIANCE_MISSING_PARAMETER_SEGMENT = 48,
	GMIME_COMPLIANCE_EXCESSIVE_ANGLE_BRACKETS_IN_ADDRESS = 49,
	GMIME_COMPLIANCE_UNBALANCED_ANGLE_BRACKETS_IN_ADDRESS = 50,
	GMIME_COMPLIANCE_UNBALANCED_QUOTES_IN_ADDRESS = 51,
	GMIME_COMPLIANCE_UNBALANCED_PARENTHESES_IN_ADDRESS = 52,
	GMIME_COMPLIANCE_UNQUOTED_DISPLAY_NAME = 53,
	GMIME_COMPLIANCE_ADDRESS_IN_DISPLAY_NAME = 54,
	GMIME_COMPLIANCE_UNQUOTED_ADDRESS_IN_DISPLAY_NAME = 55,
	GMIME_COMPLIANCE_ADDRESS_IN_GROUP_DISPLAY_NAME = 56,
	GMIME_COMPLIANCE_UNQUOTED_ADDRESS_IN_GROUP_DISPLAY_NAME = 57,
	GMIME_COMPLIANCE_INVALID_LOCAL_PART = 58,
	GMIME_COMPLIANCE_MISSING_ADDRESS_SEPARATOR = 59,
	GMIME_COMPLIANCE_AMBIGUOUS_MAILBOX_BOUNDARY = 60,
	GMIME_COMPLIANCE_EXTRANEOUS_COMMA_IN_ADDRESS_LIST = 61,
	GMIME_COMPLIANCE_OBSOLETE_ROUTE_ADDRESS = 62,
	GMIME_COMPLIANCE_ADDRESS_WITHOUT_DOMAIN = 63,
	GMIME_COMPLIANCE_OBSOLETE_DOMAIN_SYNTAX = 64,
	GMIME_COMPLIANCE_INVALID_DOMAIN = 65,
	GMIME_COMPLIANCE_TRAILING_DOT_IN_DOMAIN = 66,
	GMIME_COMPLIANCE_WHITESPACE_IN_DOMAIN_LITERAL = 67,
	GMIME_COMPLIANCE_INVALID_CHARACTER_IN_DOMAIN_LITERAL = 68,
	GMIME_COMPLIANCE_INVALID_8BIT_ADDRESS = 69,
	GMIME_COMPLIANCE_MISSING_GROUP_TERMINATOR = 70,
	GMIME_COMPLIANCE_NON_CONFORMANT_ADDRESS = 71,
	GMIME_COMPLIANCE_NULL_BYTE_IN_ADDRESS = 72,
	GMIME_COMPLIANCE_NULL_BYTE_IN_DISPLAY_NAME = 73,
	GMIME_COMPLIANCE_LINE_BREAK_IN_ADDRESS = 74,
	GMIME_COMPLIANCE_CONTROL_CHARACTER_IN_ADDRESS = 75,
	GMIME_COMPLIANCE_ISO2022_SEQUENCE_IN_LOCAL_PART = 76,
	GMIME_COMPLIANCE_EMPTY_GROUP_NAME = 77,
	GMIME_COMPLIANCE_NESTED_ADDRESS_GROUP = 78,
	GMIME_COMPLIANCE_MISSING_BODY_SEPARATOR = 79,
	GMIME_COMPLIANCE_MISSING_MULTIPART_BOUNDARY = 80,
	GMIME_COMPLIANCE_MIME_DEPTH_LIMIT_EXCEEDED = 81,
	GMIME_COMPLIANCE_INCOMPLETE_BASE64_QUANTUM = 82,
	GMIME_COMPLIANCE_INVALID_BASE64_CHARACTER = 83,
	GMIME_COMPLIANCE_INVALID_BASE64_PADDING = 84,
	GMIME_COMPLIANCE_BASE64_CHARACTERS_AFTER_PADDING = 85,
	GMIME_COMPLIANCE_OBSOLETE_BASE64_COMMENT = 86,
	GMIME_COMPLIANCE_INVALID_QUOTED_PRINTABLE_ENCODING = 87,
	GMIME_COMPLIANCE_INVALID_QUOTED_PRINTABLE_SOFT_BREAK = 88,
	GMIME_COMPLIANCE_INVALID_UUENCODE_PRETEXT = 89,
	GMIME_COMPLIANCE_INVALID_UUENCODE_FILE_MODE = 90,
	GMIME_COMPLIANCE_INVALID_UUENCODED_CONTENT = 91,
	GMIME_COMPLIANCE_INVALID_UUENCODED_LINE_LENGTH = 92,
	GMIME_COMPLIANCE_INCOMPLETE_UUENCODED_LINE = 93,
	GMIME_COMPLIANCE_INVALID_UUENCODED_LINE_EXTRA_DATA = 94,
	GMIME_COMPLIANCE_INVALID_UUENCODE_END_MARKER = 95,
	GMIME_COMPLIANCE_INCOMPLETE_UUENCODED_CONTENT = 96,
	GMIME_COMPLIANCE_TOO_MANY_COMPLIANCE_ISSUES = G_MAXINT
} GMimeComplianceViolation;

/**
 * GMimeComplianceIssue:
 * @context: reporting context
 * @violation: diagnostic identity
 * @stream_offset: absolute input byte offset
 * @line_number: 1-based physical line number
 * @column_number: 1-based byte column, or zero if unknown
 * @position_kind: interpretation of the input position
 *
 * A boxed diagnostic referring to the original wire bytes.
 **/
typedef struct {
	GMimeComplianceContext context;
	GMimeComplianceViolation violation;
	gint64 stream_offset;
	guint64 line_number;
	guint64 column_number;
	GMimeCompliancePositionKind position_kind;
} GMimeComplianceIssue;

/**
 * GMimeComplianceIssueFunc:
 * @issue: (transfer none): the diagnostic, borrowed during the callback
 * @user_data: application data
 *
 * Receives a compliance finding without changing how MIME data is parsed.
 **/
typedef void (*GMimeComplianceIssueFunc) (const GMimeComplianceIssue *issue, gpointer user_data);

/**
 * GMimeComplianceOptions:
 *
 * Reference-counted boxed diagnostic options. References and boxed copies
 * share settings; explicit clones and internal snapshots remain independent.
 * Reader operations snapshot these values; changing them never changes the
 * parsing policy.
 **/
typedef struct _GMimeComplianceOptions GMimeComplianceOptions;

#define GMIME_TYPE_COMPLIANCE_OPTIONS (g_mime_compliance_options_get_type ())
#define GMIME_TYPE_COMPLIANCE_ISSUE (g_mime_compliance_issue_get_type ())

GType g_mime_compliance_options_get_type (void);
GMimeComplianceOptions *g_mime_compliance_options_new (void);
GMimeComplianceOptions *g_mime_compliance_options_get_default (void);
GMimeComplianceOptions *g_mime_compliance_options_clone (GMimeComplianceOptions *options);
GMimeComplianceOptions *g_mime_compliance_options_ref (GMimeComplianceOptions *options);
void g_mime_compliance_options_unref (GMimeComplianceOptions *options);
GMimeComplianceContext g_mime_compliance_options_get_context (GMimeComplianceOptions *options);
void g_mime_compliance_options_set_context (GMimeComplianceOptions *options, GMimeComplianceContext context);
GMimeComplianceValidators g_mime_compliance_options_get_enabled_validators (GMimeComplianceOptions *options);
void g_mime_compliance_options_set_enabled_validators (GMimeComplianceOptions *options, GMimeComplianceValidators validators);
guint g_mime_compliance_options_get_max_issues_per_violation (GMimeComplianceOptions *options);
void g_mime_compliance_options_set_max_issues_per_violation (GMimeComplianceOptions *options, guint limit);

GType g_mime_compliance_issue_get_type (void);
GMimeComplianceIssue *g_mime_compliance_issue_new (GMimeComplianceContext context, GMimeComplianceViolation violation,
	gint64 stream_offset, guint64 line_number, guint64 column_number, GMimeCompliancePositionKind position_kind);
GMimeComplianceIssue *g_mime_compliance_issue_copy (const GMimeComplianceIssue *issue);
void g_mime_compliance_issue_free (GMimeComplianceIssue *issue);
gboolean g_mime_compliance_issue_equal (const GMimeComplianceIssue *a, const GMimeComplianceIssue *b);
char *g_mime_compliance_issue_to_string (const GMimeComplianceIssue *issue);
GMimeComplianceSeverity g_mime_compliance_violation_get_severity (GMimeComplianceViolation violation, GMimeComplianceContext context);
GMimeComplianceCategories g_mime_compliance_violation_get_categories (GMimeComplianceViolation violation);
const char *g_mime_compliance_violation_get_name (GMimeComplianceViolation violation);
const char *g_mime_compliance_violation_get_description (GMimeComplianceViolation violation);
const char *g_mime_compliance_violation_get_remarks (GMimeComplianceViolation violation);

G_END_DECLS

#endif /* __GMIME_COMPLIANCE_H__ */
