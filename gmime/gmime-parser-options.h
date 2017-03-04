/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*  GMime
 *  Copyright (C) 2000-2017 Jeffrey Stedfast
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

G_BEGIN_DECLS

/**
 * GMimeRfcComplianceMode:
 * @GMIME_RFC_COMPLIANCE_LOOSE: Attempt to be much more liberal accepting broken and/or invalid formatting.
 * @GMIME_RFC_COMPLIANCE_STRICT: Do not attempt to be overly liberal in accepting broken and/or invalid formatting.
 *
 * An RFC compliance mode.
 **/
typedef enum {
	GMIME_RFC_COMPLIANCE_LOOSE,
	GMIME_RFC_COMPLIANCE_STRICT
} GMimeRfcComplianceMode;

/**
 * GMimeParserOptions:
 * @addresses: The compliance mode that should be used when parsing rfc822 addresses.
 * @parameters: The compliance mode that should be used when parsing Content-Type and Content-Disposition parameters.
 * @rfc2047: The compliance mode that should be used when decoding rfc2047 encoded words.
 * @charsets: The fallback charsets to try when decoding 8-bit headers.
 *
 * A set of parser options used by #GMimeParser and various other parsing functions.
 **/
typedef struct {
	GMimeRfcComplianceMode addresses;
	GMimeRfcComplianceMode parameters;
	GMimeRfcComplianceMode rfc2047;
	char **charsets;
} GMimeParserOptions;

GMimeParserOptions *g_mime_parser_options_get_default (void);

GMimeParserOptions *g_mime_parser_options_new (void);
void g_mime_parser_options_free (GMimeParserOptions *options);

GMimeRfcComplianceMode g_mime_parser_options_get_address_parser_compliance_mode (GMimeParserOptions *options);
void g_mime_parser_options_set_address_parser_compliance_mode (GMimeParserOptions *options, GMimeRfcComplianceMode mode);

GMimeRfcComplianceMode g_mime_parser_options_get_parameter_compliance_mode (GMimeParserOptions *options);
void g_mime_parser_options_set_parameter_compliance_mode (GMimeParserOptions *options, GMimeRfcComplianceMode mode);

GMimeRfcComplianceMode g_mime_parser_options_get_rfc2047_compliance_mode (GMimeParserOptions *options);
void g_mime_parser_options_set_rfc2047_compliance_mode (GMimeParserOptions *options, GMimeRfcComplianceMode mode);

const char **g_mime_parser_options_get_fallback_charsets (GMimeParserOptions *options);
void g_mime_parser_options_set_fallback_charsets (GMimeParserOptions *options, const char **charsets);

G_END_DECLS

#endif /* __GMIME_PARSER_OPTIONS_H__ */
