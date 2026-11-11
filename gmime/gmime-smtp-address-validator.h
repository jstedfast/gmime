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

#ifndef __GMIME_SMTP_ADDRESS_VALIDATOR_H__
#define __GMIME_SMTP_ADDRESS_VALIDATOR_H__

#include <gmime/gmime-internet-address.h>

G_BEGIN_DECLS

/**
 * GMimeSmtpAddressValidationOptions:
 * @GMIME_SMTP_ADDRESS_VALIDATION_NONE: Validate syntax without length limits.
 * @GMIME_SMTP_ADDRESS_VALIDATION_ALLOW_ISO2022JP_LOCAL_PART: Permit ISO-2022-JP
 * escape sequences in a quoted local-part, ending in ASCII mode.
 * @GMIME_SMTP_ADDRESS_VALIDATION_ENFORCE_LENGTH_LIMITS: Enforce SMTP/DNS lengths.
 *
 * Optional SMTP validation policies.
 **/
typedef enum {
	GMIME_SMTP_ADDRESS_VALIDATION_NONE = 0,
	GMIME_SMTP_ADDRESS_VALIDATION_ALLOW_ISO2022JP_LOCAL_PART = 1 << 0,
	GMIME_SMTP_ADDRESS_VALIDATION_ENFORCE_LENGTH_LIMITS = 1 << 1
} GMimeSmtpAddressValidationOptions;

/**
 * GMimeSmtpUtf8AddressValidationOptions:
 * @GMIME_SMTP_UTF8_ADDRESS_VALIDATION_NONE: Validate the address as written.
 * @GMIME_SMTP_UTF8_ADDRESS_VALIDATION_DECODE_IDN: Decode canonical domain A-labels.
 * @GMIME_SMTP_UTF8_ADDRESS_VALIDATION_ASCII_IS_LATIN: Count ASCII letters as Latin
 * within the local-part and separately within each domain label.
 * @GMIME_SMTP_UTF8_ADDRESS_VALIDATION_ENFORCE_LENGTH_LIMITS: Enforce SMTP/DNS lengths.
 *
 * Optional SMTPUTF8 validation policies.
 **/
typedef enum {
	GMIME_SMTP_UTF8_ADDRESS_VALIDATION_NONE = 0,
	GMIME_SMTP_UTF8_ADDRESS_VALIDATION_DECODE_IDN = 1 << 0,
	GMIME_SMTP_UTF8_ADDRESS_VALIDATION_ASCII_IS_LATIN = 1 << 1,
	GMIME_SMTP_UTF8_ADDRESS_VALIDATION_ENFORCE_LENGTH_LIMITS = 1 << 2
} GMimeSmtpUtf8AddressValidationOptions;

/**
 * GMimeSmtpAddressError:
 * @GMIME_SMTP_ADDRESS_ERROR_NONE: Valid address.
 * @GMIME_SMTP_ADDRESS_ERROR_SYNTAX: Invalid mailbox syntax or invalid UTF-8.
 * @GMIME_SMTP_ADDRESS_ERROR_NON_ASCII: Syntactically valid but requires SMTPUTF8.
 * @GMIME_SMTP_ADDRESS_ERROR_LOCAL_PART_TOO_LONG: Local-part exceeds 64 octets.
 * @GMIME_SMTP_ADDRESS_ERROR_LABEL_TOO_LONG: Domain label exceeds 63 octets.
 * @GMIME_SMTP_ADDRESS_ERROR_DOMAIN_TOO_LONG: Domain exceeds 253 octets.
 * @GMIME_SMTP_ADDRESS_ERROR_ADDRESS_TOO_LONG: Address exceeds 254 octets.
 *
 * Validation results, in precedence order. Length errors require the length
 * option. These are results, not a GError domain.
 **/
typedef enum {
	GMIME_SMTP_ADDRESS_ERROR_NONE,
	GMIME_SMTP_ADDRESS_ERROR_SYNTAX,
	GMIME_SMTP_ADDRESS_ERROR_NON_ASCII,
	GMIME_SMTP_ADDRESS_ERROR_LOCAL_PART_TOO_LONG,
	GMIME_SMTP_ADDRESS_ERROR_LABEL_TOO_LONG,
	GMIME_SMTP_ADDRESS_ERROR_DOMAIN_TOO_LONG,
	GMIME_SMTP_ADDRESS_ERROR_ADDRESS_TOO_LONG
} GMimeSmtpAddressError;

/**
 * GMimeSmtpUtf8AddressError:
 * @GMIME_SMTP_UTF8_ADDRESS_ERROR_NONE: Valid address.
 * @GMIME_SMTP_UTF8_ADDRESS_ERROR_SYNTAX: Invalid mailbox syntax or invalid UTF-8.
 * @GMIME_SMTP_UTF8_ADDRESS_ERROR_LOCAL_PART_TOO_LONG: Local-part exceeds 64 UTF-8 octets.
 * @GMIME_SMTP_UTF8_ADDRESS_ERROR_LABEL_TOO_LONG: Domain label exceeds 63 A-label octets.
 * @GMIME_SMTP_UTF8_ADDRESS_ERROR_DOMAIN_TOO_LONG: Domain exceeds 253 A-label octets.
 * @GMIME_SMTP_UTF8_ADDRESS_ERROR_ADDRESS_TOO_LONG: Address exceeds 254 UTF-8 octets.
 * @GMIME_SMTP_UTF8_ADDRESS_ERROR_A_LABEL: Forbidden or invalid A-label.
 * @GMIME_SMTP_UTF8_ADDRESS_ERROR_DISALLOWED_CODE_POINT: Code point is not permitted.
 * @GMIME_SMTP_UTF8_ADDRESS_ERROR_NOT_NORMALIZED: Address is not NFC.
 * @GMIME_SMTP_UTF8_ADDRESS_ERROR_CONTEXT_RULE: A contextual rule is unsatisfied.
 * @GMIME_SMTP_UTF8_ADDRESS_ERROR_MIXED_SCRIPTS: Disallowed script mixture.
 *
 * Validation results, in precedence order. Length errors require the length
 * option. These are results, not a GError domain.
 **/
typedef enum {
	GMIME_SMTP_UTF8_ADDRESS_ERROR_NONE,
	GMIME_SMTP_UTF8_ADDRESS_ERROR_SYNTAX,
	GMIME_SMTP_UTF8_ADDRESS_ERROR_LOCAL_PART_TOO_LONG,
	GMIME_SMTP_UTF8_ADDRESS_ERROR_LABEL_TOO_LONG,
	GMIME_SMTP_UTF8_ADDRESS_ERROR_DOMAIN_TOO_LONG,
	GMIME_SMTP_UTF8_ADDRESS_ERROR_ADDRESS_TOO_LONG,
	GMIME_SMTP_UTF8_ADDRESS_ERROR_A_LABEL,
	GMIME_SMTP_UTF8_ADDRESS_ERROR_DISALLOWED_CODE_POINT,
	GMIME_SMTP_UTF8_ADDRESS_ERROR_NOT_NORMALIZED,
	GMIME_SMTP_UTF8_ADDRESS_ERROR_CONTEXT_RULE,
	GMIME_SMTP_UTF8_ADDRESS_ERROR_MIXED_SCRIPTS
} GMimeSmtpUtf8AddressError;

gboolean g_mime_smtp_address_try_validate (const char *address, gssize length,
					 GMimeSmtpAddressValidationOptions options, GMimeSmtpAddressError *error);
gboolean g_mime_smtp_address_try_validate_mailbox (GMimeMailboxAddress *mailbox,
						 GMimeSmtpAddressValidationOptions options, GMimeSmtpAddressError *error);
gboolean g_mime_smtp_utf8_address_try_validate (const char *address, gssize length,
					      GMimeSmtpUtf8AddressValidationOptions options, GMimeSmtpUtf8AddressError *error);
gboolean g_mime_smtp_utf8_address_try_validate_mailbox (GMimeMailboxAddress *mailbox,
						      GMimeSmtpUtf8AddressValidationOptions options, GMimeSmtpUtf8AddressError *error);

G_END_DECLS

#endif
