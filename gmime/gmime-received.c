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
#include "gmime-common.h"
#include "gmime-internal.h"
#include "gmime-parse-utils.h"
#include "gmime-error.h"
#include "gmime-table-private.h"
#include "gmime-utils.h"

/**
 * SECTION: gmime-received
 * @title: GMimeReceived
 * @short_description: Received header values
 *
 * Parses Received header values following MimeKit's clause recovery rules.
 * Clauses retain their order and keyword spelling; their parsed values are
 * not subjected to the stricter public ReceivedClause constructor checks.
 * Duplicate known clauses, incomplete comments and nonempty invalid dates
 * fail with %GMIME_ERROR_PARSE_ERROR.
 *
 * Named setters update or remove the first matching clause. Adding a missing
 * named clause sorts the entire collection by clause identifier; updating or
 * removing an existing clause does not sort. Unlike MimeKit's unstable
 * List.Sort(), equal identifiers retain their relative order, so adding a
 * different clause cannot change which duplicate is first. Direct collection
 * operations preserve order and permit duplicate and shared clause instances,
 * but not NULL entries.
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

/**
 * g_mime_received_new:
 *
 * Creates an empty value with no clauses or date.
 *
 * Returns: (transfer full): a new #GMimeReceived.
 **/
GMimeReceived *
g_mime_received_new (void)
{
	return g_object_new (GMIME_TYPE_RECEIVED, NULL);
}

static char *
format_ip_address (GInetAddress *address)
{
	guint i, end, count, zero_start = 0, zero_length = 0;
	const guint8 *bytes;
	gboolean embedded;
	GString *builder;
	guint16 words[8];

	if (g_inet_address_get_family (address) == G_SOCKET_FAMILY_IPV4)
		return g_inet_address_to_string (address);

	bytes = g_inet_address_to_bytes (address);
	for (i = 0; i < G_N_ELEMENTS (words); i++)
		words[i] = ((guint16) bytes[i * 2] << 8) | bytes[i * 2 + 1];

	/* Match IPAddress's compatible, mapped, SIIT and ISATAP embedded IPv4 spelling. */
	embedded = (words[0] == 0 && words[1] == 0 && words[2] == 0 && words[3] == 0 &&
		    words[6] != 0 && ((words[4] == 0 && (words[5] == 0 || words[5] == 0xffff)) ||
				     (words[4] == 0xffff && words[5] == 0))) ||
		   (words[4] == 0 && words[5] == 0x5efe);
	count = embedded ? 6 : 8;

	for (i = 0; i < count; i = end) {
		end = i + 1;

		if (words[i] != 0)
			continue;

		while (end < count && words[end] == 0)
			end++;

		if (end - i > 1 && end - i > zero_length) {
			zero_start = i;
			zero_length = end - i;
		}
	}

	builder = g_string_new (NULL);
	for (i = 0; i < count;) {
		if (zero_length && i == zero_start) {
			g_string_append (builder, "::");
			i += zero_length;
		} else {
			if (builder->len && builder->str[builder->len - 1] != ':')
				g_string_append_c (builder, ':');
			g_string_append_printf (builder, "%x", words[i++]);
		}
	}

	if (embedded) {
		if (builder->str[builder->len - 1] != ':')
			g_string_append_c (builder, ':');

		g_string_append_printf (builder, "%u.%u.%u.%u", bytes[12], bytes[13], bytes[14], bytes[15]);
	}

	return g_string_free (builder, FALSE);
}

/**
 * g_mime_received_new_with_hosts:
 * @from: the sending host's domain or address literal
 * @from_tcp_info: the sending host's IP address
 * @by: the receiving host's domain or address literal
 * @by_tcp_info: the receiving host's IP address
 * @date_time: (transfer none): the date and timezone offset to retain
 * @error: (nullable): return location for a #GError
 *
 * Creates validated From and By clauses with bracketed IP address comments.
 * Host strings and address text are copied; the addresses are not retained.
 * Invalid host values report %GMIME_ERROR_PARSE_ERROR.
 *
 * #GInetAddress does not carry an IPv6 scope ID. Scoped address comments can
 * instead be assigned with g_mime_received_set_from_tcp_info() and
 * g_mime_received_set_by_tcp_info().
 *
 * Returns: (transfer full) (nullable): a new value, or %NULL on error.
 **/
GMimeReceived *
g_mime_received_new_with_hosts (const char *from, GInetAddress *from_tcp_info,
			      const char *by, GInetAddress *by_tcp_info,
			      GDateTime *date_time, GError **error)
{
	GInetAddress *addresses[] = { from_tcp_info, by_tcp_info };
	const char *hosts[] = { from, by };
	GMimeReceivedClause *clause;
	GMimeReceived *received;
	char *address, *comment;
	guint i;

	g_return_val_if_fail (from != NULL, NULL);
	g_return_val_if_fail (G_IS_INET_ADDRESS (from_tcp_info), NULL);
	g_return_val_if_fail (by != NULL, NULL);
	g_return_val_if_fail (G_IS_INET_ADDRESS (by_tcp_info), NULL);
	g_return_val_if_fail (date_time != NULL, NULL);
	g_return_val_if_fail (error == NULL || *error == NULL, NULL);

	received = g_mime_received_new ();
	for (i = 0; i < G_N_ELEMENTS (hosts); i++) {
		address = format_ip_address (addresses[i]);
		comment = g_strdup_printf ("[%s]", address);
		clause = g_mime_received_clause_new_with_comment (keywords[i], hosts[i], comment, error);
		g_free (comment);
		g_free (address);

		if (!clause) {
			g_object_unref (received);
			return NULL;
		}

		g_ptr_array_add (received->clauses, clause);
	}

	g_mime_received_set_date_time (received, date_time);

	return received;
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
unfold (char *text)
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

static gint
compare_clauses (gconstpointer a, gconstpointer b)
{
	const GMimeReceivedClause *x = *(GMimeReceivedClause * const *) a;
	const GMimeReceivedClause *y = *(GMimeReceivedClause * const *) b;

	return (int) x->id - (int) y->id;
}

static int
find_clause (GMimeReceived *received, GMimeReceivedClauseId id)
{
	GMimeReceivedClause *clause;
	guint i;

	for (i = 0; i < received->clauses->len; i++) {
		clause = received->clauses->pdata[i];
		if (clause->id == id)
			return i;
	}

	return -1;
}

static const char *
get_value (GMimeReceived *received, GMimeReceivedClauseId id)
{
	GMimeReceivedClause *clause;
	int index;

	g_return_val_if_fail (GMIME_IS_RECEIVED (received), NULL);

	if ((index = find_clause (received, id)) == -1)
		return NULL;

	clause = received->clauses->pdata[index];
	return clause->value;
}

static gboolean
set_value (GMimeReceived *received, GMimeReceivedClauseId id, const char *value, GError **error)
{
	GMimeReceivedClause *clause;
	int index;

	g_return_val_if_fail (GMIME_IS_RECEIVED (received), FALSE);
	g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

	index = find_clause (received, id);
	if (!value) {
		if (index != -1)
			g_ptr_array_remove_index (received->clauses, index);
		return TRUE;
	}

	if (index != -1)
		return g_mime_received_clause_set_value (received->clauses->pdata[index], value, error);

	g_return_val_if_fail (received->clauses->len < G_MAXINT, FALSE);

	if (!(clause = g_mime_received_clause_new (keywords[id - 1], value, error)))
		return FALSE;

	g_ptr_array_add (received->clauses, clause);
	g_ptr_array_sort (received->clauses, compare_clauses);

	return TRUE;
}

static const char *
get_tcp_info (GMimeReceived *received, GMimeReceivedClauseId id)
{
	GMimeReceivedClause *clause;
	int index;

	g_return_val_if_fail (GMIME_IS_RECEIVED (received), NULL);

	if ((index = find_clause (received, id)) == -1)
		return NULL;

	clause = received->clauses->pdata[index];

	return clause->comments->len ? clause->comments->pdata[0] : NULL;
}

static void
set_tcp_info (GMimeReceived *received, GMimeReceivedClauseId id, const char *comment)
{
	GMimeReceivedClause *clause;
	GPtrArray *comments;
	char *copy;
	int index;

	g_return_if_fail (GMIME_IS_RECEIVED (received));
	g_return_if_fail (comment == NULL || g_utf8_validate (comment, -1, NULL));

	index = find_clause (received, id);

	g_return_if_fail (comment == NULL || index != -1 || received->clauses->len < G_MAXINT);

	copy = comment ? unfold (g_strdup (comment)) : NULL;
	if (index != -1) {
		clause = received->clauses->pdata[index];

		g_ptr_array_set_size (clause->comments, 0);

		if (copy)
			g_ptr_array_add (clause->comments, copy);
	} else if (copy) {
		/* Note: use the same .ctor as the parser so that the value is not validated. */
		comments = g_ptr_array_new_with_free_func (g_free);
		g_ptr_array_add (comments, copy);
		parsed_clause_new (received, id, g_strdup (keywords[id - 1]), g_strdup (""), comments);
		g_ptr_array_sort (received->clauses, compare_clauses);
	}
}

static char *
format_comment (const char *comment)
{
	GString *builder = g_string_new ("(");

	while (*comment) {
		if (*comment == '\\')
			g_string_append_c (builder, '\\');

		if (*comment == '\t')
			g_string_append_c (builder, ' ');
		else if (*comment != '\r' && *comment != '\n')
			g_string_append_c (builder, *comment);

		comment++;
	}

	g_string_append_c (builder, ')');
	return g_string_free (builder, FALSE);
}

static void
append_clause (GMimeFormatOptions *options, GString *builder, gsize *line_length, GMimeReceivedClause *clause)
{
	const char *newline = g_mime_format_options_get_newline (options);
	gsize maxline = g_mime_format_options_get_max_line_length (options);
	gsize keyword_length, value_length, comment_length;
	char *comment = NULL;
	guint i;

	if (clause->comments->len)
		comment = format_comment (clause->comments->pdata[0]);

	if (clause->id != GMIME_RECEIVED_CLAUSE_NONE) {
		comment_length = comment ? _g_mime_count_utf16_units (comment, comment + strlen (comment)) : 0;
		keyword_length = _g_mime_count_utf16_units (clause->keyword, clause->keyword + strlen (clause->keyword));
		value_length = _g_mime_count_utf16_units (clause->value, clause->value + strlen (clause->value));

		if (comment_length > 0 && keyword_length + 1 + value_length + 1 + comment_length < maxline) {
			/* keyword, value and (the first) comment can all fit on a single line */
			if (*line_length + 1 + keyword_length + 1 + value_length + 1 + comment_length > maxline) {
				/* fold the header value here so that we can put as many tokens on the same line as we can */
				g_string_append_printf (builder, "%s\t", newline);
				*line_length = 1;
			} else {
				/* they can all fit on the current line */
				g_string_append_c (builder, ' ');
				(*line_length)++;
			}
		} else if (keyword_length + 1 + value_length < maxline) {
			/* keyword and value can both fit on a single line */
			if (*line_length + 1 + keyword_length + 1 + value_length > maxline) {
				/* they are too long to fit on the current line, so wrap and put them on the next line */
				g_string_append_printf (builder, "%s\t", newline);
				*line_length = 1;
			} else {
				/* they can both fit on the current line */
				g_string_append_c (builder, ' ');
				(*line_length)++;
			}
		} else {
			/* we'll need to separate the keyword and value */
			if (*line_length + 1 + keyword_length > maxline) {
				/* keyword is too long to fit on the current line */
				g_string_append_printf (builder, "%s\t", newline);
				*line_length = 1;
			} else {
				/* keyword fit on the current line */
				g_string_append_c (builder, ' ');
				(*line_length)++;
			}
		}

		/* append the keyword */
		g_string_append (builder, clause->keyword);
		*line_length += keyword_length;

		if (*line_length + 1 + value_length > maxline) {
			g_string_append_printf (builder, "%s\t", newline);
			*line_length = 1;
		} else {
			g_string_append_c (builder, ' ');
			(*line_length)++;
		}

		/* append the value */
		g_string_append (builder, clause->value);
		*line_length += value_length;
	}

	for (i = 0; i < clause->comments->len; i++) {
		/* Note: ReceivedClause.Comments is a public list, so make sure that the comments cannot inject line breaks. */
		if (i > 0)
			comment = format_comment (clause->comments->pdata[i]);
		comment_length = _g_mime_count_utf16_units (comment, comment + strlen (comment));

		if (*line_length + 1 + comment_length > maxline) {
			g_string_append_printf (builder, "%s\t", newline);
			*line_length = 1;
		} else {
			g_string_append_c (builder, ' ');
			(*line_length)++;
		}

		g_string_append (builder, comment);
		*line_length += comment_length;
		g_free (comment);
	}
}

/**
 * g_mime_received_to_string:
 * @received: a #GMimeReceived
 * @options: (nullable): a #GMimeFormatOptions or %NULL for the defaults
 *
 * Serializes the value without its field name, including leading whitespace
 * and a terminating newline. Folding accounts for the "Received:" field name
 * and counts UTF-16 units like MimeKit. Individual tokens are not split and
 * may exceed the configured line-length target.
 *
 * Comments are unfolded and backslashes escaped. Clauses retain their order,
 * keyword spelling and values. Parsed dates retain their unfolded spelling
 * until date assignment invalidates the cache. Otherwise dates use
 * g_mime_utils_header_format_date(). No RFC 2047 or IDN encoding is applied.
 *
 * Returns: (transfer full) (nullable): the serialized value, or %NULL if a date
 * cannot be formatted because a required UTC conversion is out of range.
 **/
char *
g_mime_received_to_string (GMimeReceived *received, GMimeFormatOptions *options)
{
	const char *newline = g_mime_format_options_get_newline (options);
	gsize maxline = g_mime_format_options_get_max_line_length (options);
	gsize line_length = strlen ("Received:"), stamp_length;
	char *formatted = NULL;
	const char *stamp;
	GString *builder;
	guint i;

	g_return_val_if_fail (GMIME_IS_RECEIVED (received), NULL);

	builder = g_string_new (NULL);
	for (i = 0; i < received->clauses->len; i++)
		append_clause (options, builder, &line_length, received->clauses->pdata[i]);

	if (received->date_time) {
		if (line_length + 1 > maxline) {
			g_string_append_printf (builder, "%s\t", newline);
			line_length = 1;
		}

		g_string_append_c (builder, ';');
		line_length++;

		/* use the cached value if available (so when reformatting a parsed header, we keep it in tact) */
		if (!(stamp = received->date_time_str)) {
			stamp = formatted = g_mime_utils_header_format_date (received->date_time);
			if (!stamp) {
				g_string_free (builder, TRUE);
				return NULL;
			}
		}

		stamp_length = _g_mime_count_utf16_units (stamp, stamp + strlen (stamp));

		if (line_length + 1 + stamp_length > maxline)
			g_string_append_printf (builder, "%s\t", newline);
		else
			g_string_append_c (builder, ' ');

		g_string_append (builder, stamp);
		g_free (formatted);
	}

	g_string_append (builder, newline);
	return g_string_free (builder, FALSE);
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
			received->date_time_str = unfold (_g_mime_utils_decode_8bit_text (
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
 * g_mime_received_set_clause:
 * @received: a #GMimeReceived
 * @index: an existing clause index
 * @clause: (transfer none): the replacement clause
 *
 * Retains @clause and releases the old entry without validating or sorting.
 * Replacing an entry with itself is safe. Duplicate instances are allowed.
 **/
void
g_mime_received_set_clause (GMimeReceived *received, int index, GMimeReceivedClause *clause)
{
	GMimeReceivedClause *previous;

	g_return_if_fail (GMIME_IS_RECEIVED (received));
	g_return_if_fail (index >= 0 && (guint) index < received->clauses->len);
	g_return_if_fail (GMIME_IS_RECEIVED_CLAUSE (clause));

	previous = received->clauses->pdata[index];
	received->clauses->pdata[index] = g_object_ref (clause);
	g_object_unref (previous);
}

/**
 * g_mime_received_add_clause:
 * @received: a #GMimeReceived
 * @clause: (transfer none): the clause to append
 *
 * Retains @clause without validating or sorting. Duplicate instances are allowed.
 **/
void
g_mime_received_add_clause (GMimeReceived *received, GMimeReceivedClause *clause)
{
	g_return_if_fail (GMIME_IS_RECEIVED (received));
	g_return_if_fail (GMIME_IS_RECEIVED_CLAUSE (clause));
	g_return_if_fail (received->clauses->len < G_MAXINT);

	g_ptr_array_add (received->clauses, g_object_ref (clause));
}

/**
 * g_mime_received_insert_clause:
 * @received: a #GMimeReceived
 * @index: an index from zero through the clause count
 * @clause: (transfer none): the clause to insert
 *
 * Retains @clause at @index without validating or sorting.
 **/
void
g_mime_received_insert_clause (GMimeReceived *received, int index, GMimeReceivedClause *clause)
{
	g_return_if_fail (GMIME_IS_RECEIVED (received));
	g_return_if_fail (index >= 0 && (guint) index <= received->clauses->len);
	g_return_if_fail (GMIME_IS_RECEIVED_CLAUSE (clause));
	g_return_if_fail (received->clauses->len < G_MAXINT);

	g_ptr_array_insert (received->clauses, index, g_object_ref (clause));
}

/**
 * g_mime_received_index_of_clause:
 * @received: a #GMimeReceived
 * @clause: the clause instance to find
 *
 * Returns: the first matching instance index, or -1 if absent.
 **/
int
g_mime_received_index_of_clause (GMimeReceived *received, GMimeReceivedClause *clause)
{
	guint index;

	g_return_val_if_fail (GMIME_IS_RECEIVED (received), -1);
	g_return_val_if_fail (GMIME_IS_RECEIVED_CLAUSE (clause), -1);

	return g_ptr_array_find (received->clauses, clause, &index) ? (int) index : -1;
}

/**
 * g_mime_received_contains_clause:
 * @received: a #GMimeReceived
 * @clause: the clause instance to find
 *
 * Returns: whether the exact @clause instance belongs to @received.
 **/
gboolean
g_mime_received_contains_clause (GMimeReceived *received, GMimeReceivedClause *clause)
{
	return g_mime_received_index_of_clause (received, clause) != -1;
}

/**
 * g_mime_received_remove_clause:
 * @received: a #GMimeReceived
 * @clause: (transfer none): the clause instance to remove
 *
 * Removes and releases only the first occurrence, preserving remaining order.
 *
 * Returns: whether a clause was removed.
 **/
gboolean
g_mime_received_remove_clause (GMimeReceived *received, GMimeReceivedClause *clause)
{
	int index = g_mime_received_index_of_clause (received, clause);

	if (index == -1)
		return FALSE;

	g_ptr_array_remove_index (received->clauses, index);
	return TRUE;
}

/**
 * g_mime_received_remove_clause_at:
 * @received: a #GMimeReceived
 * @index: an existing clause index
 *
 * Removes and releases the indexed entry without reordering survivors.
 **/
void
g_mime_received_remove_clause_at (GMimeReceived *received, int index)
{
	g_return_if_fail (GMIME_IS_RECEIVED (received));
	g_return_if_fail (index >= 0 && (guint) index < received->clauses->len);

	g_ptr_array_remove_index (received->clauses, index);
}

/**
 * g_mime_received_clear_clauses:
 * @received: a #GMimeReceived
 *
 * Removes and releases every clause without changing the date.
 **/
void
g_mime_received_clear_clauses (GMimeReceived *received)
{
	g_return_if_fail (GMIME_IS_RECEIVED (received));

	g_ptr_array_set_size (received->clauses, 0);
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

/**
 * g_mime_received_set_date_time:
 * @received: a #GMimeReceived
 * @date_time: (nullable) (transfer none): the date and offset, or %NULL to remove
 *
 * Retains @date_time. Always clears the cached parsed date spelling, even
 * when assigning the existing date. Does not change clauses.
 **/
void
g_mime_received_set_date_time (GMimeReceived *received, GDateTime *date_time)
{
	g_return_if_fail (GMIME_IS_RECEIVED (received));

	if (date_time)
		g_date_time_ref (date_time);
	g_clear_pointer (&received->date_time, g_date_time_unref);
	received->date_time = date_time;
	g_clear_pointer (&received->date_time_str, g_free);
}

/**
 * g_mime_received_get_from:
 * @received: a #GMimeReceived
 *
 * Returns: (transfer none) (nullable): the first From value, or %NULL if absent.
 **/
const char *
g_mime_received_get_from (GMimeReceived *received)
{
	return get_value (received, GMIME_RECEIVED_CLAUSE_FROM);
}

/**
 * g_mime_received_set_from:
 * @received: a #GMimeReceived
 * @value: (nullable): a nonempty UTF-8 domain or address literal, or %NULL to remove
 * @error: (nullable): return location for a #GError
 *
 * Updates the first From clause without changing its keyword or comments.
 * A missing clause is created and the collection sorted by identifier.
 * Invalid values report %GMIME_ERROR_PARSE_ERROR without changing the model.
 *
 * Returns: %TRUE on success, or %FALSE on error.
 **/
gboolean
g_mime_received_set_from (GMimeReceived *received, const char *value, GError **error)
{
	return set_value (received, GMIME_RECEIVED_CLAUSE_FROM, value, error);
}

/**
 * g_mime_received_get_from_tcp_info:
 * @received: a #GMimeReceived
 *
 * Returns: (transfer none) (nullable): the first From comment, or %NULL if absent.
 **/
const char *
g_mime_received_get_from_tcp_info (GMimeReceived *received)
{
	return get_tcp_info (received, GMIME_RECEIVED_CLAUSE_FROM);
}

/**
 * g_mime_received_set_from_tcp_info:
 * @received: a #GMimeReceived
 * @comment: (nullable): a UTF-8 comment, or %NULL to clear comments
 *
 * Replaces all comments of the first From clause with an unfolded copy.
 * Tabs become spaces and CR/LF are removed; other whitespace is preserved.
 * If absent and @comment is non-NULL, creates an empty-valued From clause
 * and sorts the collection by identifier. NULL does not create a clause.
 **/
void
g_mime_received_set_from_tcp_info (GMimeReceived *received, const char *comment)
{
	set_tcp_info (received, GMIME_RECEIVED_CLAUSE_FROM, comment);
}

/**
 * g_mime_received_get_by:
 * @received: a #GMimeReceived
 *
 * Returns: (transfer none) (nullable): the first By value, or %NULL if absent.
 **/
const char *
g_mime_received_get_by (GMimeReceived *received)
{
	return get_value (received, GMIME_RECEIVED_CLAUSE_BY);
}

/**
 * g_mime_received_set_by:
 * @received: a #GMimeReceived
 * @value: (nullable): a nonempty UTF-8 domain or address literal, or %NULL to remove
 * @error: (nullable): return location for a #GError
 *
 * Updates or removes the first By clause, as g_mime_received_set_from()
 * does for From. Failed validation leaves the model unchanged.
 *
 * Returns: %TRUE on success, or %FALSE on error.
 **/
gboolean
g_mime_received_set_by (GMimeReceived *received, const char *value, GError **error)
{
	return set_value (received, GMIME_RECEIVED_CLAUSE_BY, value, error);
}

/**
 * g_mime_received_get_by_tcp_info:
 * @received: a #GMimeReceived
 *
 * Returns: (transfer none) (nullable): the first By comment, or %NULL if absent.
 **/
const char *
g_mime_received_get_by_tcp_info (GMimeReceived *received)
{
	return get_tcp_info (received, GMIME_RECEIVED_CLAUSE_BY);
}

/**
 * g_mime_received_set_by_tcp_info:
 * @received: a #GMimeReceived
 * @comment: (nullable): a UTF-8 comment, or %NULL to clear comments
 *
 * Replaces all comments of the first By clause, as
 * g_mime_received_set_from_tcp_info() does for From.
 **/
void
g_mime_received_set_by_tcp_info (GMimeReceived *received, const char *comment)
{
	set_tcp_info (received, GMIME_RECEIVED_CLAUSE_BY, comment);
}

/**
 * g_mime_received_get_via:
 * @received: a #GMimeReceived
 *
 * Returns: (transfer none) (nullable): the first Via value, or %NULL if absent.
 **/
const char *
g_mime_received_get_via (GMimeReceived *received)
{
	return get_value (received, GMIME_RECEIVED_CLAUSE_VIA);
}

/**
 * g_mime_received_set_via:
 * @received: a #GMimeReceived
 * @value: (nullable): a nonempty UTF-8 value, or %NULL to remove
 * @error: (nullable): return location for a #GError
 *
 * Updates or removes the first Via clause. New clauses sort the collection
 * by identifier. Values are not trimmed and must pass clause validation.
 * Failed validation leaves the model unchanged.
 *
 * Returns: %TRUE on success, or %FALSE on error.
 **/
gboolean
g_mime_received_set_via (GMimeReceived *received, const char *value, GError **error)
{
	return set_value (received, GMIME_RECEIVED_CLAUSE_VIA, value, error);
}

/**
 * g_mime_received_get_with:
 * @received: a #GMimeReceived
 *
 * Returns: (transfer none) (nullable): the first With value, or %NULL if absent.
 **/
const char *
g_mime_received_get_with (GMimeReceived *received)
{
	return get_value (received, GMIME_RECEIVED_CLAUSE_WITH);
}

/**
 * g_mime_received_set_with:
 * @received: a #GMimeReceived
 * @value: (nullable): a nonempty UTF-8 value, or %NULL to remove
 * @error: (nullable): return location for a #GError
 *
 * Updates or removes the first With clause using the same ordering and
 * validation rules as g_mime_received_set_via().
 *
 * Returns: %TRUE on success, or %FALSE on error.
 **/
gboolean
g_mime_received_set_with (GMimeReceived *received, const char *value, GError **error)
{
	return set_value (received, GMIME_RECEIVED_CLAUSE_WITH, value, error);
}

/**
 * g_mime_received_get_id:
 * @received: a #GMimeReceived
 *
 * Returns: (transfer none) (nullable): the first Id value, or %NULL if absent.
 **/
const char *
g_mime_received_get_id (GMimeReceived *received)
{
	return get_value (received, GMIME_RECEIVED_CLAUSE_ID);
}

/**
 * g_mime_received_set_id:
 * @received: a #GMimeReceived
 * @value: (nullable): a nonempty UTF-8 message identifier, or %NULL to remove
 * @error: (nullable): return location for a #GError
 *
 * Updates or removes the first Id clause. New clauses sort the collection.
 * Uses complete lenient message-id validation without normalizing the value.
 * Failed validation leaves the model unchanged.
 *
 * Returns: %TRUE on success, or %FALSE on error.
 **/
gboolean
g_mime_received_set_id (GMimeReceived *received, const char *value, GError **error)
{
	return set_value (received, GMIME_RECEIVED_CLAUSE_ID, value, error);
}

/**
 * g_mime_received_get_for:
 * @received: a #GMimeReceived
 *
 * Returns: (transfer none) (nullable): the first For value, or %NULL if absent.
 **/
const char *
g_mime_received_get_for (GMimeReceived *received)
{
	return get_value (received, GMIME_RECEIVED_CLAUSE_FOR);
}

/**
 * g_mime_received_set_for:
 * @received: a #GMimeReceived
 * @value: (nullable): a nonempty UTF-8 mailbox address, or %NULL to remove
 * @error: (nullable): return location for a #GError
 *
 * Updates or removes the first For clause. New clauses sort the collection.
 * Uses the existing mailbox parser without normalizing the stored value.
 * Failed validation leaves the model unchanged.
 *
 * Returns: %TRUE on success, or %FALSE on error.
 **/
gboolean
g_mime_received_set_for (GMimeReceived *received, const char *value, GError **error)
{
	return set_value (received, GMIME_RECEIVED_CLAUSE_FOR, value, error);
}
