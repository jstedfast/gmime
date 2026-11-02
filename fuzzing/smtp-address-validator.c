/* Inspired by MimeKit.Fuzzing.Targets.SmtpUtf8ValidatorTarget.
 * Copyright (c) 2013-2026 .NET Foundation and Contributors.
 * Copyright (c) 2026 Jeffrey Stedfast.
 * Distributed under the MIT license; see LICENSE. */

#include "fuzz.h"
#include <string.h>

static gboolean
is_length_error (GMimeSmtpUtf8AddressError error)
{
	return error >= GMIME_SMTP_UTF8_ADDRESS_ERROR_LOCAL_PART_TOO_LONG &&
		error <= GMIME_SMTP_UTF8_ADDRESS_ERROR_ADDRESS_TOO_LONG;
}

void
fuzz_smtp_address_validator (const guint8 *data, gsize length)
{
	GMimeSmtpUtf8AddressError errors[8], again, with;
	GMimeSmtpAddressError smtp[4], smtp_again;
	char *bounded = g_malloc (MAX (length, 1));
	char *terminated = g_malloc (length + 1);
	gboolean valid;
	guint i;

	if (length) {
		memcpy (bounded, data, length);
		memcpy (terminated, data, length);
	}
	terminated[length] = 0;
	for (i = 0; i < 8; i++) {
		valid = g_mime_smtp_utf8_address_try_validate (bounded, length, i, &errors[i]);
		FUZZ_CHECK (valid == (errors[i] == GMIME_SMTP_UTF8_ADDRESS_ERROR_NONE), "SMTPUTF8 result/error disagree");
		g_mime_smtp_utf8_address_try_validate (terminated, length, i, &again);
		FUZZ_CHECK (again == errors[i], "SMTPUTF8 reads outside input");
		FUZZ_CHECK (valid == g_mime_smtp_utf8_address_try_validate (bounded, length, i, NULL), "SMTPUTF8 optional error changes result");
		if (!memchr (terminated, 0, length)) {
			g_mime_smtp_utf8_address_try_validate (terminated, -1, i, &again);
			FUZZ_CHECK (again == errors[i], "SMTPUTF8 bounded/string disagreement");
		}
	}
	for (i = 0; i < 8; i++) {
		if (!(i & GMIME_SMTP_UTF8_ADDRESS_VALIDATION_ENFORCE_LENGTH_LIMITS)) {
			with = errors[i | GMIME_SMTP_UTF8_ADDRESS_VALIDATION_ENFORCE_LENGTH_LIMITS];
			FUZZ_CHECK (!is_length_error (errors[i]), "SMTPUTF8 unrequested length limit");
			FUZZ_CHECK (with == errors[i] ||
				    (errors[i] != GMIME_SMTP_UTF8_ADDRESS_ERROR_SYNTAX && is_length_error (with)),
				    "SMTPUTF8 length option changes an unrelated result");
		}
		if (!(i & GMIME_SMTP_UTF8_ADDRESS_VALIDATION_ASCII_IS_LATIN)) {
			with = errors[i | GMIME_SMTP_UTF8_ADDRESS_VALIDATION_ASCII_IS_LATIN];
			FUZZ_CHECK (with == errors[i] ||
				    (errors[i] == GMIME_SMTP_UTF8_ADDRESS_ERROR_NONE && with == GMIME_SMTP_UTF8_ADDRESS_ERROR_MIXED_SCRIPTS),
				    "ASCII-is-Latin changes an unrelated result");
		}
		if (!(i & GMIME_SMTP_UTF8_ADDRESS_VALIDATION_DECODE_IDN)) {
			with = errors[i | GMIME_SMTP_UTF8_ADDRESS_VALIDATION_DECODE_IDN];
			FUZZ_CHECK (errors[i] == GMIME_SMTP_UTF8_ADDRESS_ERROR_A_LABEL ?
				    (!is_length_error (with) && with != GMIME_SMTP_UTF8_ADDRESS_ERROR_SYNTAX) : with == errors[i],
				    "IDN decoding changes an unrelated result");
		}
	}
	for (i = 0; i < 4; i++) {
		valid = g_mime_smtp_address_try_validate (bounded, length, i, &smtp[i]);
		g_mime_smtp_address_try_validate (terminated, length, i, &smtp_again);
		FUZZ_CHECK (smtp[i] == smtp_again && valid == (smtp[i] == GMIME_SMTP_ADDRESS_ERROR_NONE),
			    "SMTP result is inconsistent");
		FUZZ_CHECK (valid == g_mime_smtp_address_try_validate (bounded, length, i, NULL), "SMTP optional error changes result");
	}
	if (errors[0] == GMIME_SMTP_UTF8_ADDRESS_ERROR_NONE)
		FUZZ_CHECK (smtp[0] == GMIME_SMTP_ADDRESS_ERROR_NONE || smtp[0] == GMIME_SMTP_ADDRESS_ERROR_NON_ASCII,
			    "SMTP and SMTPUTF8 syntax disagree");
	fuzz_equal (data, length, (const guint8 *) bounded, length, "SMTP input mutated");
	g_free (terminated);
	g_free (bounded);
}
