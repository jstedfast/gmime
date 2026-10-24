/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*  GMime
 *  Copyright (C) 2000-2026 Jeffrey Stedfast and Michael Zucchi
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

#include "gmime-common.h"
#include "gmime-encodings.h"


/**
 * SECTION: gmime-encodings
 * @title: gmime-encodings
 * @short_description: Content transfer encoding names
 * @see_also:
 *
 * Utility functions to convert MIME Content-Transfer-Encoding names.
 **/


static gboolean
is (const char *str, size_t length, const char *value)
{
	return length == strlen (value) && g_ascii_strncasecmp (str, value, length) == 0;
}


/**
 * g_mime_content_encoding_from_string:
 * @str: (nullable): a UTF-8 Content-Transfer-Encoding value
 *
 * Gets the appropriate #GMimeContentEncoding enumeration value based
 * on the input string. Leading Unicode whitespace is skipped; the first
 * whitespace or semicolon terminates the case-insensitive encoding name.
 * Any suffix after that delimiter is ignored, matching MimeKit's tolerance
 * for semicolons added by Google Docs. Comments are not skipped.
 *
 * Recognizes 7bit, 8bit, binary, base64, quoted-printable, uuencode,
 * x-uuencode and x-uue. The legacy 7-bit and 8-bit aliases remain supported
 * as a GMime compatibility extension to MimeKit.
 * %NULL, an empty name, or malformed UTF-8 within the name returns
 * #GMIME_CONTENT_ENCODING_UNSPECIFIED.
 *
 * Returns: the #GMimeContentEncoding specified by @str or
 * #GMIME_CONTENT_ENCODING_UNSPECIFIED on error.
 **/
GMimeContentEncoding
g_mime_content_encoding_from_string (const char *str)
{
	const char *start;
	gunichar c;
	size_t length;

	if (!str)
		return GMIME_CONTENT_ENCODING_UNSPECIFIED;

	/* MimeKit uses Char.IsWhiteSpace, not MIME linear whitespace. Validate
	 * each code point before advancing so truncated UTF-8 stays in bounds. */
	while (*str) {
		c = g_utf8_get_char_validated (str, -1);
		if (c == (gunichar) -1 || c == (gunichar) -2)
			return GMIME_CONTENT_ENCODING_UNSPECIFIED;
		if (!_g_mime_is_unicode_whitespace (c))
			break;
		str = g_utf8_next_char (str);
	}

	start = str;
	/* Only the first token matters: delimiters allow arbitrary trailing
	 * text, but an embedded comment or extra name byte rejects the token. */
	while (*str && *str != ';') {
		c = g_utf8_get_char_validated (str, -1);
		if (c == (gunichar) -1 || c == (gunichar) -2)
			return GMIME_CONTENT_ENCODING_UNSPECIFIED;
		if (_g_mime_is_unicode_whitespace (c))
			break;
		str = g_utf8_next_char (str);
	}
	length = str - start;

	/* Retain GMime's historical hyphenated aliases even though MimeKit
	 * only recognizes the canonical 7bit and 8bit spellings. */
	if (is (start, length, "7bit") || is (start, length, "7-bit"))
		return GMIME_CONTENT_ENCODING_7BIT;
	else if (is (start, length, "8bit") || is (start, length, "8-bit"))
		return GMIME_CONTENT_ENCODING_8BIT;
	else if (is (start, length, "binary"))
		return GMIME_CONTENT_ENCODING_BINARY;
	else if (is (start, length, "base64"))
		return GMIME_CONTENT_ENCODING_BASE64;
	else if (is (start, length, "quoted-printable"))
		return GMIME_CONTENT_ENCODING_QUOTEDPRINTABLE;
	else if (is (start, length, "uuencode"))
		return GMIME_CONTENT_ENCODING_UUENCODE;
	else if (is (start, length, "x-uuencode"))
		return GMIME_CONTENT_ENCODING_UUENCODE;
	else if (is (start, length, "x-uue"))
		return GMIME_CONTENT_ENCODING_UUENCODE;
	else
		return GMIME_CONTENT_ENCODING_UNSPECIFIED;
}


/**
 * g_mime_content_encoding_to_string:
 * @encoding: a #GMimeContentEncoding
 *
 * Gets the string value of the content encoding.
 *
 * Returns: the encoding type as a string or %NULL on error. Available
 * values for the encoding are: #GMIME_CONTENT_ENCODING_UNSPECIFIED,
 * #GMIME_CONTENT_ENCODING_7BIT, #GMIME_CONTENT_ENCODING_8BIT,
 * #GMIME_CONTENT_ENCODING_BINARY, #GMIME_CONTENT_ENCODING_BASE64,
 * #GMIME_CONTENT_ENCODING_QUOTEDPRINTABLE and
 * #GMIME_CONTENT_ENCODING_UUENCODE.
 **/
const char *
g_mime_content_encoding_to_string (GMimeContentEncoding encoding)
{
	switch (encoding) {
	case GMIME_CONTENT_ENCODING_7BIT:
		return "7bit";
	case GMIME_CONTENT_ENCODING_8BIT:
		return "8bit";
	case GMIME_CONTENT_ENCODING_BINARY:
		return "binary";
	case GMIME_CONTENT_ENCODING_BASE64:
		return "base64";
	case GMIME_CONTENT_ENCODING_QUOTEDPRINTABLE:
		return "quoted-printable";
	case GMIME_CONTENT_ENCODING_UUENCODE:
		return "x-uuencode";
	default:
		/* I guess this is a good default... */
		return NULL;
	}
}
