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

#include "gmime-table-private.h"
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
is (const char *str, const char *value, size_t n)
{
	return g_ascii_strncasecmp (str, value, n) == 0 &&
		(str[n] == '\0' || is_lwsp (str[n]));
}


/**
 * g_mime_content_encoding_from_string:
 * @str: a string representing a Content-Transfer-Encoding value
 *
 * Gets the appropriate #GMimeContentEncoding enumeration value based
 * on the input string.
 *
 * Returns: the #GMimeContentEncoding specified by @str or
 * #GMIME_CONTENT_ENCODING_UNSPECIFIED on error.
 **/
GMimeContentEncoding
g_mime_content_encoding_from_string (const char *str)
{
	while (is_lwsp (*str))
		str++;

	if (is (str, "7bit", 4))
		return GMIME_CONTENT_ENCODING_7BIT;
	else if (is (str, "8bit", 4))
		return GMIME_CONTENT_ENCODING_8BIT;
	else if (is (str, "7-bit", 5))
		return GMIME_CONTENT_ENCODING_7BIT;
	else if (is (str, "8-bit", 5))
		return GMIME_CONTENT_ENCODING_8BIT;
	else if (is (str, "binary", 6))
		return GMIME_CONTENT_ENCODING_BINARY;
	else if (is (str, "base64", 6))
		return GMIME_CONTENT_ENCODING_BASE64;
	else if (is (str, "quoted-printable", 16))
		return GMIME_CONTENT_ENCODING_QUOTEDPRINTABLE;
	else if (is (str, "uuencode", 8))
		return GMIME_CONTENT_ENCODING_UUENCODE;
	else if (is (str, "x-uuencode", 10))
		return GMIME_CONTENT_ENCODING_UUENCODE;
	else if (is (str, "x-uue", 5))
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
