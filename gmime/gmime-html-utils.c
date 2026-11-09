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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <string.h>

#include "gmime-html-utils.h"
#include "gmime-html-token-private.h"
#include "gmime-html-entity-private.h"
#include "gmime-reader-simd-private.h"

/**
 * SECTION: gmime-html-utils
 * @title: HTML text utilities
 * @short_description: UTF-8 HTML text and attribute encoding and decoding
 *
 * Encoding escapes markup characters, drops disallowed controls and emits
 * non-ASCII characters as decimal numeric references. Attribute encoding
 * includes the enclosing quote characters. Decoding recognizes the complete
 * HTML named-reference table and numeric replacement rules.
 */

char *
_g_mime_html_encode_len (const char *text, gsize length, char quote)
{
	static const GMimeReaderByteSet safe_text = {
		{ 0xfc, 0xfc, 0xf8, 0xfc, 0xfc, 0xfc, 0xf8, 0xf8, 0xfc, 0xfd, 0xfd, 0xfc, 0xf5, 0xfd, 0xf4, 0x7c }, TRUE
	};
	static const GMimeReaderByteSet safe_double_quoted = {
		{ 0xfc, 0xfc, 0xf8, 0xfc, 0xfc, 0xfc, 0xf8, 0xfc, 0xfc, 0xfd, 0xfd, 0xfc, 0xf5, 0xfd, 0xf4, 0x7c }, TRUE
	};
	static const GMimeReaderByteSet safe_single_quoted = {
		{ 0xfc, 0xfc, 0xfc, 0xfc, 0xfc, 0xfc, 0xf8, 0xf8, 0xfc, 0xfd, 0xfd, 0xfc, 0xf5, 0xfd, 0xf4, 0x7c }, TRUE
	};
	const GMimeReaderByteSet *set = !quote ? &safe_text : quote == '"' ? &safe_double_quoted : &safe_single_quoted;
	GString *buffer = g_string_sized_new (length);
	const char *p = text, *end = text + length;
	gsize prefix = _g_mime_reader_find_byte ((const guint8 *) text, length, set);

	if (quote)
		g_string_append_c (buffer, quote);

	g_string_append_len (buffer, text, prefix);
	p += prefix;

	while (p < end) {
		gunichar c = g_utf8_get_char (p);

		p = g_utf8_next_char (p);

		switch (c) {
		case '\t':
		case '\r':
		case '\n':
		case '\f':
			g_string_append_c (buffer, c);
			break;
		case '&':
			g_string_append (buffer, "&amp;");
			break;
		case '<':
			g_string_append (buffer, "&lt;");
			break;
		case '>':
			g_string_append (buffer, "&gt;");
			break;
		case '"':
			g_string_append (buffer, !quote || quote == '"' ? "&quot;" : "\"");
			break;
		case '\'':
			g_string_append (buffer, !quote || quote == '\'' ? "&#39;" : "'");
			break;
		default:
			if (c < 32 || (c >= 127 && c < 160))
				break;

			if (c >= 160)
				g_string_append_printf (buffer, "&#%u;", c);
			else
				g_string_append_c (buffer, c);
		}
	}

	if (quote)
		g_string_append_c (buffer, quote);

	return g_string_free (buffer, FALSE);
}

/**
 * g_mime_html_encode:
 * @text: UTF-8 text
 *
 * Returns: (transfer full): newly allocated HTML-encoded text
 */
char *
g_mime_html_encode (const char *text)
{
	g_return_val_if_fail (text && g_utf8_validate (text, -1, NULL), NULL);

	return _g_mime_html_encode_len (text, strlen (text), 0);
}

/**
 * g_mime_html_attribute_encode:
 * @text: a UTF-8 attribute value
 *
 * Returns: (transfer full): newly allocated encoded text, including double quotes
 */
char *
g_mime_html_attribute_encode (const char *text)
{
	return g_mime_html_attribute_encode_quoted (text, '"');
}

/**
 * g_mime_html_attribute_encode_quoted:
 * @text: a UTF-8 attribute value
 * @quote: the single or double quote to use
 *
 * Returns: (transfer full): newly allocated encoded text, including quotes
 */
char *
g_mime_html_attribute_encode_quoted (const char *text, char quote)
{
	g_return_val_if_fail (text && g_utf8_validate (text, -1, NULL), NULL);
	g_return_val_if_fail (quote == '"' || quote == '\'', NULL);

	return _g_mime_html_encode_len (text, strlen (text), quote);
}

static char *
decode (const char *text, gboolean attribute)
{
	GString *buffer = g_string_new (NULL);
	HtmlEntity entity = { 0 };
	const char *p = text;

	while (*p) {
		gunichar c = g_utf8_get_char (p);

		if (c != '&') {
			g_string_append_unichar (buffer, c);
			p = g_utf8_next_char (p);
			continue;
		}

		_g_mime_html_entity_push (&entity, '&');
		p++;

		while (*p && _g_mime_html_entity_push (&entity, (c = g_utf8_get_char (p)))) {
			p = g_utf8_next_char (p);
			if (c == ';')
				break;
		}

		g_string_append (buffer, _g_mime_html_entity_value (&entity, attribute, g_utf8_get_char (p)));
		_g_mime_html_entity_reset (&entity);
	}

	return g_string_free (buffer, FALSE);
}

/**
 * g_mime_html_decode:
 * @text: UTF-8 text containing character references
 *
 * Returns: (transfer full): newly allocated decoded text
 */
char *
g_mime_html_decode (const char *text)
{
	g_return_val_if_fail (text && g_utf8_validate (text, -1, NULL), NULL);

	return decode (text, FALSE);
}

/**
 * g_mime_html_attribute_decode:
 * @text: a UTF-8 attribute value containing character references
 *
 * Returns: (transfer full): newly allocated text decoded using attribute rules
 */
char *
g_mime_html_attribute_decode (const char *text)
{
	g_return_val_if_fail (text && g_utf8_validate (text, -1, NULL), NULL);

	return decode (text, TRUE);
}
