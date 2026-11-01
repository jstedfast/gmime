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
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library; if not, write to the Free
 *  Software Foundation, 51 Franklin Street, Fifth Floor, Boston, MA
 *  02110-1301, USA.
 */

#include <config.h>

#include <string.h>

#include "gmime-encoded-word-validator-private.h"
#include "gmime-table-private.h"

void
_g_mime_encoded_word_validator_init (GMimeEncodedWordValidator *v, const guint8 *buffer, gsize length,
				    GMimeComplianceIssueFunc report, gpointer user_data, GMimeComplianceContext context,
				    gint64 stream_offset, guint64 line, guint64 column)
{
	g_return_if_fail (v != NULL);
	g_return_if_fail (buffer != NULL || length == 0);
	g_return_if_fail (length <= G_MAXSSIZE);

	*v = (GMimeEncodedWordValidator) {
		buffer, length, 0, 0, 0, report, user_data, context, stream_offset, line, column, FALSE, FALSE
	};
}

static gboolean
is_word_token (guint8 c)
{
	return c < 127 && is_ttoken (c) && c != '.';
}

static gboolean
read_word (GMimeEncodedWordValidator *v, GMimeEncodedWordContext location, gsize *end, gboolean *complete)
{
	gsize limit = v->index + MIN (v->length - v->index, 75);
	gsize at = v->index + 2, charset = at, encoding, payload;
	guint padding = 0, hex = 0;
	gboolean base64, valid;
	guint8 kind;

	*complete = FALSE;
	*end = at;

	while (at < limit && is_word_token (v->text[at]))
		at++;

	if (at == charset || v->text[charset] == '*' || at >= limit || v->text[at++] != '?')
		return FALSE;

	encoding = at;
	while (at < limit && is_word_token (v->text[at]))
		at++;

	if (at == encoding || at >= limit || v->text[at++] != '?')
		return FALSE;

	kind = v->text[encoding];
	base64 = kind == 'B' || kind == 'b';
	valid = at - encoding == 2 && (base64 || kind == 'Q' || kind == 'q');
	payload = at;

	for (; at < limit; at++) {
		guint8 c = v->text[at];

		if (c == '?' && at + 1 < limit && v->text[at + 1] == '=') {
			*end = at + 2;
			*complete = TRUE;

			if (at == payload || hex != 0 || (base64 && ((at - payload) % 4 != 0 || padding > 2)))
				valid = FALSE;

			return valid;
		}

		/* Leave a nested candidate for the next iteration. */
		if (c == '=' && at + 2 < limit && v->text[at + 1] == '?' && v->text[at + 2] != '=')
			return FALSE;

		if (c <= 32 || c >= 127 || c == '?')
			valid = FALSE;

		if (base64) {
			if (c == '=')
				padding++;
			else if (padding != 0 || !(g_ascii_isalnum (c) || c == '+' || c == '/'))
				valid = FALSE;
		} else if (hex > 0) {
			if (!g_ascii_isxdigit (c))
				valid = FALSE;
			hex--;
		} else if (c == '=') {
			hex = 2;
		} else if (location == GMIME_ENCODED_WORD_CONTEXT_PHRASE) {
			if (!is_psafe (c))
				valid = FALSE;
		} else if (location == GMIME_ENCODED_WORD_CONTEXT_COMMENT && (c == '(' || c == ')' || c == '\\')) {
			valid = FALSE;
		}
	}

	return FALSE;
}

static gboolean
is_boundary (GMimeEncodedWordValidator *v, gsize at, GMimeEncodedWordContext location)
{
	if (at >= v->length || is_lwsp (v->text[at]))
		return TRUE;

	return location == GMIME_ENCODED_WORD_CONTEXT_COMMENT && (v->text[at] == '(' || v->text[at] == ')');
}

static void
log_issue (GMimeEncodedWordValidator *v, GMimeComplianceViolation violation)
{
	while (v->scan_index < v->index) {
		const guint8 *newline = memchr (v->text + v->scan_index, '\n', v->index - v->scan_index);

		if (!newline) {
			v->column += v->index - v->scan_index;
			v->scan_index = v->index;
			break;
		}

		v->scan_index = newline - v->text + 1;
		v->line++;
		v->column = 1;
	}

	GMimeComplianceIssue issue = {
		v->context, violation, v->stream_offset + (gint64) v->index, v->line, v->column,
		GMIME_COMPLIANCE_POSITION_ELEMENT_START
	};

	v->report (&issue, v->user_data);
}

static gboolean
contains_word_marker (const guint8 *text, gsize length)
{
	gsize at = 0;

	while (at + 1 < length) {
		const guint8 *next = memchr (text + at, '=', length - at - 1);

		if (!next)
			return FALSE;
		if (next[1] == '?')
			return TRUE;
		at = next - text + 1;
	}

	return FALSE;
}

void
_g_mime_encoded_word_validator_validate (GMimeEncodedWordValidator *v, gsize end, GMimeEncodedWordContext location)
{
	g_return_if_fail (v != NULL);
	g_return_if_fail (end <= v->length);

	if (!v->report)
		return;

	if (v->index == 0 && end > 0 && !contains_word_marker (v->text, v->length)) {
		v->index = v->length;
		return;
	}

	while (v->index < end) {
		GMimeEncodedWordContext word_context = location;
		guint8 c = v->text[v->index];

		if (location == GMIME_ENCODED_WORD_CONTEXT_TEXT) {
			if (c != '=') {
				const guint8 *next = memchr (v->text + v->index, '=', end - v->index);

				if (!next) {
					v->index = end;
					return;
				}

				v->index = next - v->text;
				c = v->text[v->index];
			}
		} else {
			if (v->comment_depth > 0)
				word_context = GMIME_ENCODED_WORD_CONTEXT_COMMENT;
			else if (v->quoted || v->literal)
				word_context = GMIME_ENCODED_WORD_CONTEXT_FORBIDDEN;

			if (c == '\\' && (v->comment_depth > 0 || v->quoted || v->literal)) {
				v->index = MIN (v->index + 2, v->length);
				continue;
			}

			if (c == '(' && !v->quoted && !v->literal)
				v->comment_depth++;
			else if (c == ')' && v->comment_depth > 0)
				v->comment_depth--;
			else if (v->comment_depth == 0 && !v->literal && c == '"')
				v->quoted = !v->quoted;
			else if (v->comment_depth == 0 && !v->quoted && c == '[')
				v->literal = TRUE;
			else if (v->literal && c == ']')
				v->literal = FALSE;
		}

		if (c == '=' && v->index + 1 < v->length && v->text[v->index + 1] == '?') {
			gboolean complete, valid;
			gsize word_end;

			valid = read_word (v, word_context, &word_end, &complete);

			/* In addr-specs, incomplete lookalikes can be ordinary local-part text. */
			if (word_context != GMIME_ENCODED_WORD_CONTEXT_FORBIDDEN || complete) {
				if (word_context == GMIME_ENCODED_WORD_CONTEXT_FORBIDDEN)
					log_issue (v, GMIME_COMPLIANCE_INVALID_ENCODED_WORD_PLACEMENT);
				else if ((v->index > 0 && !is_boundary (v, v->index - 1, word_context)) ||
					 !is_boundary (v, word_end, word_context))
					valid = FALSE;

				if (!valid)
					log_issue (v, GMIME_COMPLIANCE_INVALID_ENCODED_WORD);
			}

			if (complete) {
				v->index = word_end;
				continue;
			}
		}

		v->index++;
	}
}
