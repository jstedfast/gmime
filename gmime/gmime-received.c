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

#include "gmime-received.h"
#include "gmime-internal.h"
#include "gmime-parse-utils.h"
#include "gmime-error.h"
#include "gmime-table-private.h"

/**
 * SECTION: gmime-received
 * @title: GMimeReceived
 * @short_description: Parsed Received headers
 *
 * Parses Received header values following MimeKit's clause recovery rules.
 * Clauses retain their order and keyword spelling; their parsed values are
 * not subjected to the stricter public ReceivedClause constructor checks.
 * Duplicate known clauses, incomplete comments and nonempty invalid dates
 * fail with %GMIME_ERROR_PARSE_ERROR.
 **/

G_DEFINE_TYPE (GMimeReceived, g_mime_received, G_TYPE_OBJECT)

static const char *keywords[] = { "from", "by", "via", "with", "id", "for" };

static void
received_finalize (GObject *object)
{
	GMimeReceived *received = GMIME_RECEIVED (object);

	g_ptr_array_unref (received->clauses);
	g_clear_pointer (&received->date_time, g_date_time_unref);
	g_free (received->date_time_str);

	G_OBJECT_CLASS (g_mime_received_parent_class)->finalize (object);
}

static void
g_mime_received_class_init (GMimeReceivedClass *klass)
{
	G_OBJECT_CLASS (klass)->finalize = received_finalize;
}

static void
g_mime_received_init (GMimeReceived *received)
{
	received->clauses = g_ptr_array_new_with_free_func (g_object_unref);
}

static GMimeReceivedClause *
parsed_clause_new (GMimeReceived *received, GMimeReceivedClauseId id, char *keyword,
		   char *value, GPtrArray *comments)
{
	GMimeReceivedClause *clause = g_object_new (GMIME_TYPE_RECEIVED_CLAUSE, NULL);

	/* Note: This is the only .ctor used by the parser and therefore should not validate the value. */
	clause->id = id;
	clause->keyword = keyword;
	clause->value = value;
	g_ptr_array_unref (clause->comments);
	clause->comments = comments;
	g_ptr_array_add (received->clauses, clause);

	return clause;
}

static void
skip_whitespace (const char *text, gsize *index, gsize length)
{
	while (*index < length && is_lwsp (text[*index]))
		(*index)++;
}

static gsize
token_length (const char *text, gsize index, gsize length)
{
	gsize start = index;

	while (index < length && !(text[index] && strchr ("\t\r\n (;", text[index])))
		index++;

	return index - start;
}

static GMimeReceivedClauseId
keyword_id (const char *keyword)
{
	guint i;

	for (i = 0; i < G_N_ELEMENTS (keywords); i++) {
		if (!g_ascii_strcasecmp (keyword, keywords[i]))
			return (GMimeReceivedClauseId) (i + 1);
	}

	return GMIME_RECEIVED_CLAUSE_UNKNOWN;
}

static char *
parse_keyword (GMimeParserOptions *options, const char *text, gsize *index, gsize length)
{
	gsize count = token_length (text, *index, length);
	char *keyword = _g_mime_utils_decode_8bit_text (options, text + *index, count);

	*index += count;
	return keyword;
}

static char *
parse_value (GMimeParserOptions *options, const char *text, gsize *index, gsize length)
{
	gsize count = token_length (text, *index, length);
	char *value;
	guint i;

	for (i = 0; i < G_N_ELEMENTS (keywords); i++) {
		if (count == strlen (keywords[i]) && !g_ascii_strncasecmp (text + *index, keywords[i], count))
			return NULL;
	}

	value = _g_mime_utils_decode_8bit_text (options, text + *index, count);
	*index += count;

	return value;
}

static char *
parse_comment (GMimeParserOptions *options, const char *text, gsize *index, gsize length, GError **error)
{
	GString *builder = g_string_sized_new (64);
	gsize start = *index, depth = 1;
	gboolean escaped = FALSE;
	char *comment;
	char c;

	/* skip over the '(' */
	(*index)++;

	while (*index < length) {
		c = text[*index];

		if (escaped) {
			if (!is_lwsp (c))
				g_string_append_c (builder, c);
			else if (c != '\r' && c != '\n')
				g_string_append_c (builder, ' ');

			escaped = FALSE;
		} else if (c == '\\') {
			/* escape sequence */
			escaped = TRUE;
		} else if (c == '(') {
			g_string_append_c (builder, c);
			depth++;
		} else if (c == ')') {
			if (--depth == 0)
				break;
			g_string_append_c (builder, ')');
		} else {
			if (!is_lwsp (c))
				g_string_append_c (builder, c);
			else if (c != '\r' && c != '\n')
				g_string_append_c (builder, ' ');
		}

		(*index)++;
	}

	if (*index == length) {
		_g_mime_parse_error (error, text, text + start, "Incomplete comment token");
		g_string_free (builder, TRUE);
		return NULL;
	}

	comment = _g_mime_utils_decode_8bit_text (options, builder->str, builder->len);
	g_string_free (builder, TRUE);

	/* skip over ')' */
	(*index)++;

	return comment;
}

static gboolean
collect_comments (GMimeParserOptions *options, const char *text, gsize *index, gsize length,
		  GPtrArray *comments, GError **error)
{
	char *comment;

	while (*index < length) {
		skip_whitespace (text, index, length);

		if (*index == length || text[*index] != '(')
			break;

		if (!(comment = parse_comment (options, text, index, length, error)))
			return FALSE;

		g_ptr_array_add (comments, comment);
	}

	return TRUE;
}

static char *
unfold_date (char *text)
{
	char *in = text, *out = text;

	while (*in) {
		if (*in == '\t')
			*out++ = ' ';
		else if (*in != '\r' && *in != '\n')
			*out++ = *in;
		in++;
	}

	*out = '\0';

	return text;
}

/**
 * g_mime_received_parse:
 * @options: (nullable): a #GMimeParserOptions or %NULL for the defaults
 * @text: (array length=length) (element-type guint8): the raw Received header value
 * @length: the input byte length, or -1 for a NUL-terminated string
 * @error: (nullable): return location for a #GError
 *
 * Parses a header value, not its field name. Empty input, missing clause
 * values and missing dates are accepted. Parsed clauses bypass constructor
 * validation, retain keyword spelling and collapse whitespace between value
 * tokens to one space. Leading comments form a %GMIME_RECEIVED_CLAUSE_NONE
 * clause. Known keywords may occur only once; repeated extensions are allowed.
 *
 * Raw text uses the parser's fallback charset, without RFC 2047 decoding.
 * Embedded NUL characters are represented as U+FFFD in returned strings.
 * Input is bounded by @length and is not retained.
 *
 * Returns: (transfer full) (nullable): a parsed value, or %NULL on error.
 **/
GMimeReceived *
g_mime_received_parse (GMimeParserOptions *options, const char *text, gssize length, GError **error)
{
	gsize index = 0, keyword_index, date_end;
	char *keyword, *value, *token;
	GMimeReceivedClauseId id;
	GMimeReceived *received;
	GPtrArray *comments;
	guint mask = 0, bit;
	GString *builder;

	g_return_val_if_fail (text != NULL, NULL);
	g_return_val_if_fail (length >= -1, NULL);
	g_return_val_if_fail (error == NULL || *error == NULL, NULL);

	if (length == -1)
		length = strlen (text);

	if (length > G_MAXINT) {
		g_set_error_literal (error, GMIME_ERROR, GMIME_ERROR_PARSE_ERROR,
				     "Input text exceeds the supported length.");
		return NULL;
	}

	received = g_object_new (GMIME_TYPE_RECEIVED, NULL);

	/* skip over any leading whitespace, collecting any comments along the way */
	comments = g_ptr_array_new_with_free_func (g_free);

	if (!collect_comments (options, text, &index, length, comments, error)) {
		g_ptr_array_unref (comments);
		goto fail;
	}

	if (comments->len)
		parsed_clause_new (received, GMIME_RECEIVED_CLAUSE_NONE, g_strdup (""), g_strdup (""), comments);
	else
		g_ptr_array_unref (comments);

	/* continue parsing clauses until we reach a ';' or the end of the header value */
	while (index < (gsize) length && text[index] != ';') {
		keyword_index = index;

		/* parse the keyword token */
		keyword = parse_keyword (options, text, &index, length);

		/* update our parser state and check for duplicate clauses */
		id = keyword_id (keyword);
		if (id != GMIME_RECEIVED_CLAUSE_UNKNOWN) {
			bit = 1 << id;

			if (mask & bit) {
				g_set_error (error, GMIME_ERROR, GMIME_ERROR_PARSE_ERROR,
					     "Duplicate '%s' clause at byte offset %" G_GSIZE_FORMAT ".",
					     keywords[id - 1], keyword_index);
				g_free (keyword);
				goto fail;
			}

			mask |= bit;
		}

		/* skip over any whitespace, collecting any comments along the way... */
		comments = g_ptr_array_new_with_free_func (g_free);
		if (!collect_comments (options, text, &index, length, comments, error)) {
			g_ptr_array_unref (comments);
			g_free (keyword);
			goto fail;
		}

		/* check if we've reached the end of the header -or- we've reached a ';' */
		if (index == (gsize) length || text[index] == ';') {
			parsed_clause_new (received, id, keyword, g_strdup (""), comments);
			break;
		}

		/* parse the value token */
		if (!(value = parse_value (options, text, &index, length))) {
			/* Note: If TryParseValue returns false, then it means we've encountered what looks like the next keyword... */
			parsed_clause_new (received, id, keyword, g_strdup (""), comments);
			continue;
		}

		/* Note: some servers like to use multi-string values (e.g. "with Microsoft SMTP Server" or "via Frontend Transport") */
		builder = NULL;
		do {
			/* skip over any whitespace after the value token */
			skip_whitespace (text, &index, length);

			/* break out of the loop if we've reached the end of the header or we've reached ';' or the start of a comment */
			if (index == (gsize) length || text[index] == ';' || text[index] == '(')
				break;

			/* check to make sure this next token isn't a keyword */
			if (!(token = parse_value (options, text, &index, length)))
				break;

			/* append the token to our value */
			if (!builder)
				builder = g_string_new (value);

			g_string_append_c (builder, ' ');
			g_string_append (builder, token);
			g_free (token);
		} while (TRUE);

		if (builder) {
			g_free (value);
			value = g_string_free (builder, FALSE);
		}

		/* parse any comments that exist after the value... */
		if (!collect_comments (options, text, &index, length, comments, error)) {
			g_ptr_array_unref (comments);
			g_free (keyword);
			g_free (value);
			goto fail;
		}

		parsed_clause_new (received, id, keyword, value, comments);
	}

	if (index < (gsize) length && text[index] == ';') {
		index++;

		skip_whitespace (text, &index, length);
		date_end = length;

		/* trim trailing whitespace */
		while (date_end > index && is_lwsp (text[date_end - 1]))
			date_end--;

		if (index < date_end) {
			received->date_time = _g_mime_utils_header_decode_date (text + index, date_end - index);
			if (!received->date_time) {
				_g_mime_parse_error (error, text, text + index, "Invalid date-time format");
				goto fail;
			}

			/* cache the formatting of the date */
			received->date_time_str = unfold_date (_g_mime_utils_decode_8bit_text (
				options, text + index, date_end - index));
		}
	}

	return received;

 fail:
	g_object_unref (received);
	return NULL;
}

/**
 * g_mime_received_get_clause_count:
 * @received: a #GMimeReceived
 *
 * Returns: the number of clauses, including any leading-comment clause.
 **/
int
g_mime_received_get_clause_count (GMimeReceived *received)
{
	g_return_val_if_fail (GMIME_IS_RECEIVED (received), 0);

	return received->clauses->len;
}

/**
 * g_mime_received_get_clause:
 * @received: a #GMimeReceived
 * @index: a zero-based clause index
 *
 * Returns: (transfer none): the clause owned by @received.
 **/
GMimeReceivedClause *
g_mime_received_get_clause (GMimeReceived *received, int index)
{
	g_return_val_if_fail (GMIME_IS_RECEIVED (received), NULL);
	g_return_val_if_fail (index >= 0 && (guint) index < received->clauses->len, NULL);

	return received->clauses->pdata[index];
}

/**
 * g_mime_received_get_date_time:
 * @received: a #GMimeReceived
 *
 * Returns: (transfer none) (nullable): the parsed date with its offset, or %NULL if absent.
 **/
GDateTime *
g_mime_received_get_date_time (GMimeReceived *received)
{
	g_return_val_if_fail (GMIME_IS_RECEIVED (received), NULL);

	return received->date_time;
}
