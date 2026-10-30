/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*  GMime
 *  Copyright (C) 2000-2022 Jeffrey Stedfast
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


#ifndef __GMIME_PARSER_OPTIONS_H__
#define __GMIME_PARSER_OPTIONS_H__

#include <glib.h>
#include <glib-object.h>

G_BEGIN_DECLS

/**
 * GMimeFormat:
 * @GMIME_FORMAT_MESSAGE: The stream contains a single message.
 * @GMIME_FORMAT_MBOX: The stream is in the UNIX mbox format.
 * @GMIME_FORMAT_MMDF: The stream is in the MMDF format.
 *
 * The input format used by a reader or parser.
 **/
typedef enum {
	GMIME_FORMAT_MESSAGE,
	GMIME_FORMAT_MBOX,
	GMIME_FORMAT_MMDF
} GMimeFormat;

#define GMIME_TYPE_PARSER_OPTIONS (g_mime_parser_options_get_type ())

/**
 * GMimeRfcComplianceMode:
 * @GMIME_RFC_COMPLIANCE_LOOSE: Attempt to be much more liberal accepting broken and/or invalid formatting.
 * @GMIME_RFC_COMPLIANCE_STRICT: Do not attempt to be overly liberal in accepting broken and/or invalid formatting.
 * @GMIME_RFC_COMPLIANCE_LOOSER: Also allow consecutive or trailing local-part dots and escaped '@' characters in addresses.
 *
 * An RFC compliance mode.
 **/
typedef enum {
	GMIME_RFC_COMPLIANCE_LOOSE,
	GMIME_RFC_COMPLIANCE_STRICT,
	GMIME_RFC_COMPLIANCE_LOOSER
} GMimeRfcComplianceMode;

/**
 * GMimeParserWarning:
 * @GMIME_WARN_DUPLICATED_HEADER: Repeated exactly the same header which should exist only once.
 * @GMIME_WARN_DUPLICATED_PARAMETER: Repeated exactly the same header parameter.
 * @GMIME_WARN_UNENCODED_8BIT_HEADER: A header contains unencoded 8-bit characters.
 * @GMIME_WARN_INVALID_CONTENT_TYPE: Invalid content type, assuming `application/octet-stream`.
 * @GMIME_WARN_INVALID_RFC2047_HEADER_VALUE: Invalid RFC 2047 encoded header value.
 * @GMIME_WARN_INVALID_PARAMETER: Invalid header parameter.
 * @GMIME_WARN_MALFORMED_MULTIPART: No child parts detected within a multipart.
 * @GMIME_WARN_TRUNCATED_MESSAGE: The message is truncated.
 * @GMIME_WARN_MALFORMED_MESSAGE: The message is malformed.
 * @GMIME_WARN_INVALID_ADDRESS_LIST: Invalid address list.
 * @GMIME_CRIT_INVALID_HEADER_NAME: Invalid header name, the parser may skip the message or parts of it.
 * @GMIME_CRIT_CONFLICTING_HEADER: Conflicting header.
 * @GMIME_CRIT_CONFLICTING_PARAMETER: Conflicting header parameter.
 * @GMIME_CRIT_MULTIPART_WITHOUT_BOUNDARY: A multipart lacks the required boundary parameter.
 * @GMIME_CRIT_NESTING_OVERFLOW: The maximum MIME nesting level has been exceeded. This is very likely to be an attempt to exploit the MIME parser.
 * @GMIME_WARN_PART_WITHOUT_CONTENT: A MIME part's headers were terminated by a boundary marker.
 * @GMIME_CRIT_PART_WITHOUT_HEADERS_OR_CONTENT: A MIME part was encountered without any headers -or- content. This is very likely to be an attempt to exploit the MIME parser.
 *
 * Issues the #GMimeParser detects. Note that the `GMIME_CRIT_*` issues indicate that some parts of the #GMimeParser input may
 * be ignored or will be interpreted differently by other software products.
 *
 * New applications should prefer the #GMimeComplianceViolation reporting APIs.
 * Register a logger with g_mime_parser_set_compliance_logger() or
 * g_mime_reader_set_compliance_logger() to receive structured
 * #GMimeComplianceIssue diagnostics with more detailed input positions.
 *
 * The two reporting mechanisms have different coverage. Parser warning
 * callbacks remain supported for diagnostics not currently provided by the
 * compliance APIs, including parameter-level warnings. They are not deprecated
 * and may be used alongside compliance reporting; installing a compliance
 * logger does not replace or disable a parser warning callback.
 **/
typedef enum {
	GMIME_WARN_DUPLICATED_HEADER = 1U,
	GMIME_WARN_DUPLICATED_PARAMETER,
	GMIME_WARN_UNENCODED_8BIT_HEADER,
	GMIME_WARN_INVALID_CONTENT_TYPE,
	GMIME_WARN_INVALID_RFC2047_HEADER_VALUE,
	GMIME_WARN_MALFORMED_MULTIPART,
	GMIME_WARN_TRUNCATED_MESSAGE,
	GMIME_WARN_MALFORMED_MESSAGE,
	GMIME_CRIT_INVALID_HEADER_NAME,
	GMIME_CRIT_CONFLICTING_HEADER,
	GMIME_CRIT_CONFLICTING_PARAMETER,
	GMIME_CRIT_MULTIPART_WITHOUT_BOUNDARY,
	GMIME_WARN_INVALID_PARAMETER,
	GMIME_WARN_INVALID_ADDRESS_LIST,
	GMIME_CRIT_NESTING_OVERFLOW,
	GMIME_WARN_PART_WITHOUT_CONTENT,
	GMIME_CRIT_PART_WITHOUT_HEADERS_OR_CONTENT,
} GMimeParserWarning;

/**
 * GMimeParserOptions:
 *
 * A reference-counted set of parser options used by #GMimeParser and various
 * other parsing functions. Boxed copies share settings; use
 * g_mime_parser_options_clone() for an independent copy.
 **/
typedef struct _GMimeParserOptions GMimeParserOptions;

/**
 * GMimeParserWarningFunc:
 * @offset: parser offset where the issue has been detected, or -1 if it is unknown
 * @errcode: a #GMimeParserWarning
 * @item: a NUL-terminated string containing the value causing the issue, may be %NULL
 * @user_data: User-supplied callback data.
 *
 * The function signature for a callback to g_mime_parser_options_set_warning_callback().
 *
 * Prefer #GMimeComplianceIssueFunc for new compliance reporting code. See
 * #GMimeParserWarning for guidance on when this callback is still useful.
 **/
typedef void (*GMimeParserWarningFunc) (gint64 offset, GMimeParserWarning errcode, const gchar *item, gpointer user_data);


GType g_mime_parser_options_get_type (void);

GMimeParserOptions *g_mime_parser_options_get_default (void);

GMimeParserOptions *g_mime_parser_options_new (void);
GMimeParserOptions *g_mime_parser_options_ref (GMimeParserOptions *options);
void g_mime_parser_options_unref (GMimeParserOptions *options);

GMimeParserOptions *g_mime_parser_options_clone (GMimeParserOptions *options);

GMimeRfcComplianceMode g_mime_parser_options_get_address_compliance_mode (GMimeParserOptions *options);
void g_mime_parser_options_set_address_compliance_mode (GMimeParserOptions *options, GMimeRfcComplianceMode mode);

gboolean g_mime_parser_options_get_allow_addresses_without_domain (GMimeParserOptions *options);
void g_mime_parser_options_set_allow_addresses_without_domain (GMimeParserOptions *options, gboolean allow);

gboolean g_mime_parser_options_get_allow_unquoted_commas_in_addresses (GMimeParserOptions *options);
void g_mime_parser_options_set_allow_unquoted_commas_in_addresses (GMimeParserOptions *options, gboolean allow);

gboolean g_mime_parser_options_get_respect_content_length (GMimeParserOptions *options);
void g_mime_parser_options_set_respect_content_length (GMimeParserOptions *options, gboolean respect);

gboolean g_mime_parser_options_get_detect_openpgp (GMimeParserOptions *options);
void g_mime_parser_options_set_detect_openpgp (GMimeParserOptions *options, gboolean detect);

int g_mime_parser_options_get_max_address_group_depth (GMimeParserOptions *options);
void g_mime_parser_options_set_max_address_group_depth (GMimeParserOptions *options, int max_depth);

int g_mime_parser_options_get_max_mime_depth (GMimeParserOptions *options);
void g_mime_parser_options_set_max_mime_depth (GMimeParserOptions *options, int max_depth);

int g_mime_parser_options_get_max_header_length (GMimeParserOptions *options);
void g_mime_parser_options_set_max_header_length (GMimeParserOptions *options, int max_length);

GMimeRfcComplianceMode g_mime_parser_options_get_parameter_compliance_mode (GMimeParserOptions *options);
void g_mime_parser_options_set_parameter_compliance_mode (GMimeParserOptions *options, GMimeRfcComplianceMode mode);

GMimeRfcComplianceMode g_mime_parser_options_get_rfc2047_compliance_mode (GMimeParserOptions *options);
void g_mime_parser_options_set_rfc2047_compliance_mode (GMimeParserOptions *options, GMimeRfcComplianceMode mode);

const char *g_mime_parser_options_get_charset_encoding (GMimeParserOptions *options);
gboolean g_mime_parser_options_set_charset_encoding (GMimeParserOptions *options, const char *charset, GError **error);

GMimeParserWarningFunc g_mime_parser_options_get_warning_callback (GMimeParserOptions *options);
void g_mime_parser_options_set_warning_callback (GMimeParserOptions *options, GMimeParserWarningFunc warning_cb, gpointer user_data);
void g_mime_parser_options_set_warning_callback_full (GMimeParserOptions *options, GMimeParserWarningFunc warning_cb, gpointer user_data, GDestroyNotify notify);

G_END_DECLS

#endif /* __GMIME_PARSER_OPTIONS_H__ */
