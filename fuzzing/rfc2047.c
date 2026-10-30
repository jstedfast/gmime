/* Adapted from MimeKit.Fuzzing.Targets.Rfc2047Target.
 * Copyright (c) 2013-2026 .NET Foundation and Contributors.
 * Distributed under the MIT license; see LICENSE. */

#include "fuzz.h"
#include <string.h>

void
fuzz_rfc2047 (const guint8 *data, gsize length)
{
	static const GMimeRfcComplianceMode modes[] = { GMIME_RFC_COMPLIANCE_STRICT, GMIME_RFC_COMPLIANCE_LOOSE };
	GMimeParserOptions *options = fuzz_parser_options ();
	GMimeFormatOptions *format = g_mime_format_options_new ();
	char *input = g_malloc (length + 1), *original, *encoded, *decoded, *phrase, *start, *end;
	gboolean encodable = TRUE;
	gsize i;
	guint mode;

	/* Public header utilities take C strings: keep bytes after an embedded NUL
	 * reachable by mapping literal NULs to spaces, not truncating the input. */
	for (i = 0; i < length; i++)
		input[i] = data[i] ? data[i] : ' ';
	input[length] = '\0';
	for (mode = 0; mode < G_N_ELEMENTS (modes); mode++) {
		g_mime_parser_options_set_rfc2047_compliance_mode (options, modes[mode]);
		decoded = g_mime_utils_header_decode_text (options, input);
		phrase = g_mime_utils_header_decode_phrase (options, input);
		FUZZ_CHECK (decoded && phrase, "RFC 2047 decoding returned NULL");
		FUZZ_CHECK (g_utf8_validate (decoded, -1, NULL) && g_utf8_validate (phrase, -1, NULL),
			    "RFC 2047 decoding returned invalid UTF-8");
		FUZZ_CHECK ((gsize) g_utf8_strlen (decoded, -1) <= length * 3 + 16 &&
			    (gsize) g_utf8_strlen (phrase, -1) <= length * 3 + 16, "Unbounded RFC 2047 expansion");
		g_free (decoded);
		g_free (phrase);
	}
	original = g_utf8_make_valid (input, length);
	for (i = 0; original[i]; i++)
		if ((guint8) original[i] < 32 || original[i] == 127)
			encodable = FALSE;
	if (encodable) {
		g_mime_parser_options_set_rfc2047_compliance_mode (options, GMIME_RFC_COMPLIANCE_STRICT);
		encoded = g_mime_utils_header_encode_text (format, original, "utf-8");
		FUZZ_CHECK (encoded != NULL, "RFC 2047 text encoding returned NULL");
		decoded = g_mime_utils_header_decode_text (options, encoded);
		FUZZ_CHECK (decoded != NULL, "RFC 2047 round-trip decoding returned NULL");
		start = original;
		while (*start == ' ')
			start++;
		end = original + strlen (original);
		while (end > start && end[-1] == ' ')
			end--;
		/* GMime passes all-space input through when there are no words. */
		if (start == end) {
			start = original;
			end = original + strlen (original);
		}
		fuzz_equal ((const guint8 *) start, end - start, (const guint8 *) decoded, strlen (decoded),
			    "RFC 2047 text round trip");
		g_free (decoded);
		g_free (encoded);
	}
	encoded = g_mime_utils_header_encode_phrase (format, original, "utf-8");
	FUZZ_CHECK (encoded != NULL, "RFC 2047 phrase encoding returned NULL");
	g_free (encoded);
	g_free (original);
	g_free (input);
	g_mime_format_options_free (format);
	g_mime_parser_options_free (options);
}
