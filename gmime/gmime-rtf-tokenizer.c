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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <string.h>

#include "gmime-rtf-tokenizer-private.h"
#include "gmime-text-private.h"

struct _GMimeRtfTokenizer {
	GMimeTextReader reader;
	GMimeRtfDiagnostics *diag;
	guint64 offset, line, column, depth;
	guint binary;
	guint8 text[4096];
	gboolean ended;
};

GMimeRtfTokenizer *
_g_mime_rtf_tokenizer_new (GMimeStream *stream)
{
	GMimeRtfTokenizer *rtf = g_new0 (GMimeRtfTokenizer, 1);

	rtf->reader.stream = g_object_ref (stream);
	rtf->reader.allow_nul = TRUE;
	rtf->line = rtf->column = 1;

	return rtf;
}

void
_g_mime_rtf_tokenizer_free (GMimeRtfTokenizer *rtf)
{
	g_object_unref (rtf->reader.stream);
	g_free (rtf);
}

void
_g_mime_rtf_tokenizer_set_diagnostics (GMimeRtfTokenizer *rtf, GMimeRtfDiagnostics *diag)
{
	rtf->diag = diag;
}

/* The converter supplies UTF-8. Its default Latin-1 input charset maps each
 * wire octet to exactly one character, including within a binary run. */
static int
peek (GMimeRtfTokenizer *rtf, GError **error)
{
	GMimeTextReader *r = &rtf->reader;

	if (r->position == r->end) {
		if (!_g_mime_text_reader_read (r, error))
			return -2;

		if (r->position == r->end)
			return -1;
	}

	return g_utf8_get_char (r->buffer + r->position);
}

static void
consume (GMimeRtfTokenizer *rtf, int c)
{
	GMimeTextReader *reader = &rtf->reader;

	reader->position += g_utf8_skip[(guint8) reader->buffer[reader->position]];
	rtf->offset++;

	if (c == '\n') {
		rtf->line++;
		rtf->column = 1;
	} else {
		rtf->column++;
	}
}

static void
report (GMimeRtfTokenizer *rtf, GMimeRtfToken *token, GMimeRtfComplianceViolation v)
{
	_g_mime_rtf_report (rtf->diag, v, token->offset, token->line, token->column, token->depth);
}

static gboolean
alpha (int c)
{
	return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
}

static gboolean
control_word (GMimeRtfTokenizer *rtf, GMimeRtfToken *token, int c, GError **error)
{
	guint n = 0, digits = 0;
	gint64 parameter = 0;
	gboolean negative;

	token->kind = GMIME_RTF_CONTROL_WORD;

	do {
		if (n < sizeof (token->keyword) - 1)
			token->keyword[n++] = c;
		else
			token->keyword_truncated = TRUE;

		consume (rtf, c);
		c = peek (rtf, error);
	} while (alpha (c));

	if ((negative = c == '-')) {
		consume (rtf, c);
		c = peek (rtf, error);
	}

	while (c >= '0' && c <= '9') {
		if (digits < 10) {
			parameter = parameter * 10 + c - '0';
			digits++;
		} else {
			token->parameter_invalid = TRUE;
		}

		consume (rtf, c);
		c = peek (rtf, error);
	}

	if (c == -2)
		return FALSE;

	token->has_parameter = digits != 0;

	if (negative)
		parameter = -parameter;

	if (negative && !digits && c >= 0)
		token->parameter_invalid = TRUE;
	else if (c == ' ')
		consume (rtf, c);

	token->parameter = CLAMP (parameter, G_MININT32, G_MAXINT32);

	if (token->parameter != parameter)
		token->parameter_invalid = TRUE;

	if (token->keyword_truncated)
		report (rtf, token, GMIME_RTF_COMPLIANCE_VIOLATION_KEYWORD_TOO_LONG);

	if (token->parameter_invalid)
		report (rtf, token, GMIME_RTF_COMPLIANCE_VIOLATION_INVALID_PARAMETER);

	if (!strcmp (token->keyword, "bin") && token->parameter > 0)
		rtf->binary = token->parameter;

	return TRUE;
}

gboolean
_g_mime_rtf_tokenizer_next (GMimeRtfTokenizer *rtf, GMimeRtfToken *token, GError **error)
{
	int c;

	for (;;) {
		c = peek (rtf, error);

		if (c == -2)
			return FALSE;

		if (c >= 0 && (rtf->binary || c == '\r' || c == '\n')) {
			if (rtf->binary)
				rtf->binary--;
			consume (rtf, c);
			continue;
		}

		*token = (GMimeRtfToken) { .offset = rtf->offset, .line = rtf->line, .column = rtf->column,
				       .depth = rtf->depth };

		if (c == -1) {
			token->kind = GMIME_RTF_EOF;
			token->truncated = rtf->depth != 0 || rtf->binary != 0;

			if (!rtf->ended && token->truncated)
				report (rtf, token, GMIME_RTF_COMPLIANCE_VIOLATION_TRUNCATED_STREAM);

			rtf->ended = TRUE;

			return TRUE;
		}

		consume (rtf, c);

		switch (c) {
		case '{':
			rtf->depth++;
			token->kind = GMIME_RTF_GROUP_START;
			return TRUE;
		case '}':
			if (rtf->depth)
				rtf->depth--;

			token->depth = rtf->depth;
			token->kind = GMIME_RTF_GROUP_END;
			return TRUE;
		case '\\':
			c = peek (rtf, error);
			if (c == -2)
				return FALSE;

			if (c < 0)
				continue;

			if (alpha (c))
				return control_word (rtf, token, c, error);

			consume (rtf, c);

			if (c != '\'') {
				token->kind = GMIME_RTF_CONTROL_SYMBOL;
				token->value = c == '\r' ? '\n' : c <= 255 ? c : 0;
				return TRUE;
			}

			token->kind = GMIME_RTF_HEX_CHAR;

			for (int i = 0; i < 2; i++) {
				int digit;

				c = peek (rtf, error);

				if (c == -2)
					return FALSE;

				if (c < 0 || c > 127 || (digit = g_ascii_xdigit_value (c)) < 0) {
					report (rtf, token, GMIME_RTF_COMPLIANCE_VIOLATION_INVALID_HEX_ESCAPE);

					if (i == 1)
						return TRUE;

					break;
				}

				token->value = (token->value << 4) + digit;
				consume (rtf, c);

				if (i == 1)
					return TRUE;
			}
			break;
		default:
			token->kind = GMIME_RTF_TEXT;
			token->text = rtf->text;

			if (c > 255) {
				token->literal = c;
				token->length = g_unichar_to_utf8 (c, (char *) rtf->text);
				return TRUE;
			}

			for (;;) {
				rtf->text[token->length++] = c;

				if (token->length == sizeof (rtf->text) || rtf->reader.position == rtf->reader.end)
					break;

				c = peek (rtf, error);

				if (c > 255 || c == '{' || c == '}' || c == '\\' || c == '\r' || c == '\n')
					break;

				consume (rtf, c);
			}

			return TRUE;
		}
	}
}
