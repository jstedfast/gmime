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
#include <gio/gio.h>

#include "gmime-received-clause.h"
#include "gmime-internet-address.h"
#include "gmime-parse-utils.h"
#include "gmime-error.h"

/**
 * SECTION: gmime-received-clause
 * @title: GMimeReceivedClause
 * @short_description: Received header clauses
 *
 * Ports MimeKit's ReceivedClause model. Keywords retain their spelling,
 * while common clause identifiers are recognized ignoring ASCII case.
 * Values are validated without trimming, unfolding or normalization.
 * Failed assignments leave the previous value and comments unchanged.
 *
 * Comments are independent UTF-8 strings, without parentheses. Like
 * MimeKit's public comment list, they may be empty and may contain control
 * characters or line breaks. This model does not serialize them; a writer
 * must escape and fold comments rather than emitting them verbatim.
 **/

G_DEFINE_TYPE (GMimeReceivedClause, g_mime_received_clause, G_TYPE_OBJECT)

static void
received_clause_finalize (GObject *object)
{
	GMimeReceivedClause *clause = GMIME_RECEIVED_CLAUSE (object);

	g_free (clause->keyword);
	g_free (clause->value);
	g_ptr_array_unref (clause->comments);

	G_OBJECT_CLASS (g_mime_received_clause_parent_class)->finalize (object);
}

static void
g_mime_received_clause_class_init (GMimeReceivedClauseClass *klass)
{
	G_OBJECT_CLASS (klass)->finalize = received_clause_finalize;
}

static void
g_mime_received_clause_init (GMimeReceivedClause *clause)
{
	clause->id = GMIME_RECEIVED_CLAUSE_UNKNOWN;
	clause->comments = g_ptr_array_new_with_free_func (g_free);
}

static gboolean
invalid_clause (GError **error, const char *reason)
{
	g_set_error_literal (error, GMIME_ERROR, GMIME_ERROR_PARSE_ERROR, reason);
	return FALSE;
}

static gboolean
validate_keyword (const char *keyword, GMimeReceivedClauseId *id, GError **error)
{
	static const char *keywords[] = { "from", "by", "via", "with", "id", "for" };
	const char *p;
	guint i;

	if (!*keyword || *keyword == '-')
		return invalid_clause (error, "Invalid Received clause keyword.");

	for (p = keyword; *p; p++) {
		if (!g_ascii_isalpha (*p) && *p != '-')
			return invalid_clause (error, "Invalid Received clause keyword.");
	}

	*id = GMIME_RECEIVED_CLAUSE_UNKNOWN;
	for (i = 0; i < G_N_ELEMENTS (keywords); i++) {
		if (!g_ascii_strcasecmp (keyword, keywords[i])) {
			*id = (GMimeReceivedClauseId) (i + 1);
			break;
		}
	}

	return TRUE;
}

static gboolean
validate_ipv4 (const char *text)
{
	guint64 number;
	guint base, components = 0;
	int digit;
	const char *start;

	/* IPAddress.TryParse also accepts legacy one-to-four-part, octal and hex IPv4 forms. */
	do {
		base = 10;

		if (*text == '0') {
			base = 8;
			if (text[1] == 'x' || text[1] == 'X') {
				base = 16;
				text += 2;
			}
		}

		start = text;
		number = 0;

		while ((digit = g_ascii_xdigit_value (*text)) >= 0 && (guint) digit < base) {
			if (number > (G_MAXUINT32 - (guint) digit) / base)
				return FALSE;
			number = number * base + digit;
			text++;
		}

		if (text == start || (*text && *text != '.') || ++components > 4)
			return FALSE;

		if (!*text)
			return number <= (G_MAXUINT32 >> (8 * (components - 1)));

		if (number > 255)
			return FALSE;

		text++;
	} while (*text);

	return FALSE;
}

static gboolean
validate_ip_literal (const char *value)
{
	GInetAddress *address;
	char *text, *end, *scope;
	const char *port;
	gboolean valid;
	gsize length = strlen (value);
	guint base;
	int digit;

	if (length < 2 || value[length - 1] != ']')
		return FALSE;

	text = g_strndup (value + 1, length - 2);
	if (!strchr (text, ':')) {
		valid = validate_ipv4 (text);
		g_free (text);
		return valid;
	}

	if (*text == '[') {
		if (!(end = strchr (text, ']')))
			goto invalid;

		port = end + 1;
		if (*port) {
			if (*port++ != ':')
				goto invalid;

			base = 10;
			if (port[0] == '0' && (port[1] == 'x' || port[1] == 'X')) {
				base = 16;
				port += 2;
			}

			while (*port) {
				digit = g_ascii_xdigit_value (*port);
				if (digit < 0 || (guint) digit >= base)
					goto invalid;

				port++;
			}
		}

		*end = '\0';

		memmove (text, text + 1, end - text);
	}

	/* Scope names are accepted by IPAddress.TryParse even when not locally resolvable. */
	if (strpbrk (text, "/]"))
		goto invalid;

	if ((scope = strchr (text, '%')))
		*scope = '\0';

	address = g_inet_address_new_from_string (text);
	valid = address && g_inet_address_get_family (address) == G_SOCKET_FAMILY_IPV6;
	g_clear_object (&address);
	g_free (text);

	return valid;

 invalid:

	g_free (text);
	return FALSE;
}

static gboolean
validate_domain (const char *value)
{
	const unsigned char *p;
	gboolean dot = TRUE;

	if (*value == '[')
		return validate_ip_literal (value);

	for (p = (const unsigned char *) value; *p; p++) {
		if (*p <= 32 || *p == 127 || (*p > 90 && *p < 94))
			return FALSE;

		if (dot) {
			if (*p == '.' || *p == '-')
				return FALSE;

			dot = FALSE;
		} else if (*p == '.') {
			dot = TRUE;
		}
	}

	return TRUE;
}

static gboolean
validate_value (GMimeReceivedClauseId id, const char *value, GError **error)
{
	GMimeInternetAddress *mailbox;
	const unsigned char *p;
	const char *end;
	char *msgid;
	gboolean valid;

	if (!*value || !g_utf8_validate (value, -1, NULL))
		return invalid_clause (error, "Received clause value must be nonempty UTF-8.");

	switch (id) {
	case GMIME_RECEIVED_CLAUSE_FROM:
	case GMIME_RECEIVED_CLAUSE_BY:
		if (!validate_domain (value))
			return invalid_clause (error, "Invalid Received clause domain or address literal.");
		break;
	case GMIME_RECEIVED_CLAUSE_ID:
		/* Note: the value is serialized verbatim, so it must not contain line breaks or other control characters. */
		for (p = (const unsigned char *) value; *p; p++) {
			if (*p < 32 || *p == 127)
				return invalid_clause (error, "Invalid Received clause message identifier.");
		}

		end = value;
		msgid = decode_msgid (&end);
		valid = msgid && skip_cfws (&end) && !*end;
		g_free (msgid);

		if (!valid)
			return invalid_clause (error, "Invalid Received clause message identifier.");
		break;
	case GMIME_RECEIVED_CLAUSE_FOR:
		/* Note: validate the value by parsing as a mailbox address. */
		mailbox = g_mime_mailbox_address_parse (NULL, value, -1, error);

		if (!mailbox)
			return FALSE;

		g_object_unref (mailbox);
		break;
	default:
		for (p = (const unsigned char *) value; *p; p++) {
			if (*p < 32 || *p == 127 || *p == '(' || *p == ')' || *p == ';')
				return invalid_clause (error, "Received clause value contains illegal characters.");
		}

		break;
	}

	return TRUE;
}

/**
 * g_mime_received_clause_new:
 * @keyword: a nonempty ASCII keyword containing only letters and hyphens
 * @value: a nonempty UTF-8 clause value
 * @error: (nullable): return location for a #GError
 *
 * Creates a clause with no comments. The keyword must not start with a hyphen.
 * Common keywords are recognized ignoring case; extension keywords use
 * %GMIME_RECEIVED_CLAUSE_UNKNOWN. Invalid text reports %GMIME_ERROR_PARSE_ERROR.
 *
 * Returns: (transfer full) (nullable): a new clause, or %NULL on error.
 **/
GMimeReceivedClause *
g_mime_received_clause_new (const char *keyword, const char *value, GError **error)
{
	GMimeReceivedClause *clause;
	GMimeReceivedClauseId id;

	g_return_val_if_fail (keyword != NULL, NULL);
	g_return_val_if_fail (value != NULL, NULL);
	g_return_val_if_fail (error == NULL || *error == NULL, NULL);

	if (!validate_keyword (keyword, &id, error) || !validate_value (id, value, error))
		return NULL;

	clause = g_object_new (GMIME_TYPE_RECEIVED_CLAUSE, NULL);
	clause->keyword = g_strdup (keyword);
	clause->value = g_strdup (value);
	clause->id = id;

	return clause;
}

/**
 * g_mime_received_clause_new_with_comment:
 * @keyword: the clause keyword
 * @value: a nonempty UTF-8 clause value
 * @comment: a UTF-8 comment without enclosing parentheses
 * @error: (nullable): return location for a #GError
 *
 * Creates a validated clause with one comment, which may be empty.
 *
 * Returns: (transfer full) (nullable): a new clause, or %NULL on error.
 **/
GMimeReceivedClause *
g_mime_received_clause_new_with_comment (const char *keyword, const char *value, const char *comment, GError **error)
{
	GMimeReceivedClause *clause;

	g_return_val_if_fail (comment != NULL, NULL);
	g_return_val_if_fail (g_utf8_validate (comment, -1, NULL), NULL);

	if ((clause = g_mime_received_clause_new (keyword, value, error)))
		g_ptr_array_add (clause->comments, g_strdup (comment));

	return clause;
}

/**
 * g_mime_received_clause_get_id:
 * @clause: a #GMimeReceivedClause
 *
 * Returns: the immutable clause identifier.
 **/
GMimeReceivedClauseId
g_mime_received_clause_get_id (GMimeReceivedClause *clause)
{
	g_return_val_if_fail (GMIME_IS_RECEIVED_CLAUSE (clause), GMIME_RECEIVED_CLAUSE_UNKNOWN);

	return clause->id;
}

/**
 * g_mime_received_clause_get_keyword:
 * @clause: a #GMimeReceivedClause
 *
 * Returns: (transfer none): the immutable keyword with its original spelling.
 **/
const char *
g_mime_received_clause_get_keyword (GMimeReceivedClause *clause)
{
	g_return_val_if_fail (GMIME_IS_RECEIVED_CLAUSE (clause), NULL);

	return clause->keyword;
}

/**
 * g_mime_received_clause_get_value:
 * @clause: a #GMimeReceivedClause
 *
 * Returns: (transfer none): the value, borrowed until the next successful assignment.
 **/
const char *
g_mime_received_clause_get_value (GMimeReceivedClause *clause)
{
	g_return_val_if_fail (GMIME_IS_RECEIVED_CLAUSE (clause), NULL);

	return clause->value;
}

/**
 * g_mime_received_clause_set_value:
 * @clause: a #GMimeReceivedClause
 * @value: a nonempty UTF-8 clause value
 * @error: (nullable): return location for a #GError
 *
 * Validates and copies @value without normalization. Invalid text reports
 * %GMIME_ERROR_PARSE_ERROR and leaves the clause unchanged.
 *
 * Returns: %TRUE on success, or %FALSE on error.
 **/
gboolean
g_mime_received_clause_set_value (GMimeReceivedClause *clause, const char *value, GError **error)
{
	char *copy;

	g_return_val_if_fail (GMIME_IS_RECEIVED_CLAUSE (clause), FALSE);
	g_return_val_if_fail (value != NULL, FALSE);
	g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

	if (!validate_value (clause->id, value, error))
		return FALSE;

	copy = g_strdup (value);
	g_free (clause->value);
	clause->value = copy;

	return TRUE;
}

/**
 * g_mime_received_clause_get_comment_count:
 * @clause: a #GMimeReceivedClause
 *
 * Returns: the number of comments.
 **/
int
g_mime_received_clause_get_comment_count (GMimeReceivedClause *clause)
{
	g_return_val_if_fail (GMIME_IS_RECEIVED_CLAUSE (clause), 0);

	return clause->comments->len;
}

/**
 * g_mime_received_clause_get_comment:
 * @clause: a #GMimeReceivedClause
 * @index: a zero-based comment index
 *
 * Returns: (transfer none): the comment, borrowed until replaced or removed.
 **/
const char *
g_mime_received_clause_get_comment (GMimeReceivedClause *clause, int index)
{
	g_return_val_if_fail (GMIME_IS_RECEIVED_CLAUSE (clause), NULL);
	g_return_val_if_fail (index >= 0 && (guint) index < clause->comments->len, NULL);

	return clause->comments->pdata[index];
}

/**
 * g_mime_received_clause_index_of_comment:
 * @clause: a #GMimeReceivedClause
 * @comment: the comment to find
 *
 * Returns: the first exact, case-sensitive matching index, or -1 if absent.
 **/
int
g_mime_received_clause_index_of_comment (GMimeReceivedClause *clause, const char *comment)
{
	guint i;

	g_return_val_if_fail (GMIME_IS_RECEIVED_CLAUSE (clause), -1);
	g_return_val_if_fail (comment != NULL, -1);

	for (i = 0; i < clause->comments->len; i++) {
		if (!strcmp (clause->comments->pdata[i], comment))
			return i;
	}

	return -1;
}

/**
 * g_mime_received_clause_add_comment:
 * @clause: a #GMimeReceivedClause
 * @comment: a UTF-8 comment, which may be empty
 *
 * Appends an owned copy of @comment.
 **/
void
g_mime_received_clause_add_comment (GMimeReceivedClause *clause, const char *comment)
{
	g_return_if_fail (GMIME_IS_RECEIVED_CLAUSE (clause));
	g_return_if_fail (comment != NULL);
	g_return_if_fail (g_utf8_validate (comment, -1, NULL));

	g_ptr_array_add (clause->comments, g_strdup (comment));
}

/**
 * g_mime_received_clause_insert_comment:
 * @clause: a #GMimeReceivedClause
 * @index: an insertion index from zero through the comment count
 * @comment: a UTF-8 comment, which may be empty
 *
 * Inserts an owned copy without changing the order of existing comments.
 **/
void
g_mime_received_clause_insert_comment (GMimeReceivedClause *clause, int index, const char *comment)
{
	g_return_if_fail (GMIME_IS_RECEIVED_CLAUSE (clause));
	g_return_if_fail (index >= 0 && (guint) index <= clause->comments->len);
	g_return_if_fail (comment != NULL);
	g_return_if_fail (g_utf8_validate (comment, -1, NULL));

	g_ptr_array_insert (clause->comments, index, g_strdup (comment));
}

/**
 * g_mime_received_clause_set_comment:
 * @clause: a #GMimeReceivedClause
 * @index: a zero-based comment index
 * @comment: a UTF-8 comment, which may be empty
 *
 * Replaces one comment with an owned copy. @comment may alias any comment
 * or other string stored in @clause.
 **/
void
g_mime_received_clause_set_comment (GMimeReceivedClause *clause, int index, const char *comment)
{
	char *copy;

	g_return_if_fail (GMIME_IS_RECEIVED_CLAUSE (clause));
	g_return_if_fail (index >= 0 && (guint) index < clause->comments->len);
	g_return_if_fail (comment != NULL);
	g_return_if_fail (g_utf8_validate (comment, -1, NULL));

	copy = g_strdup (comment);
	g_free (clause->comments->pdata[index]);
	clause->comments->pdata[index] = copy;
}

/**
 * g_mime_received_clause_remove_comment:
 * @clause: a #GMimeReceivedClause
 * @comment: the comment to remove
 *
 * Removes only the first exact, case-sensitive match.
 *
 * Returns: %TRUE if a comment was removed, or %FALSE if absent.
 **/
gboolean
g_mime_received_clause_remove_comment (GMimeReceivedClause *clause, const char *comment)
{
	int index;

	g_return_val_if_fail (GMIME_IS_RECEIVED_CLAUSE (clause), FALSE);
	g_return_val_if_fail (comment != NULL, FALSE);

	index = g_mime_received_clause_index_of_comment (clause, comment);
	if (index < 0)
		return FALSE;

	g_ptr_array_remove_index (clause->comments, index);
	return TRUE;
}

/**
 * g_mime_received_clause_remove_comment_at:
 * @clause: a #GMimeReceivedClause
 * @index: a zero-based comment index
 *
 * Removes one comment, preserving the order of the remaining comments.
 **/
void
g_mime_received_clause_remove_comment_at (GMimeReceivedClause *clause, int index)
{
	g_return_if_fail (GMIME_IS_RECEIVED_CLAUSE (clause));
	g_return_if_fail (index >= 0 && (guint) index < clause->comments->len);

	g_ptr_array_remove_index (clause->comments, index);
}

/**
 * g_mime_received_clause_clear_comments:
 * @clause: a #GMimeReceivedClause
 *
 * Removes all comments without changing the keyword or value.
 **/
void
g_mime_received_clause_clear_comments (GMimeReceivedClause *clause)
{
	g_return_if_fail (GMIME_IS_RECEIVED_CLAUSE (clause));

	g_ptr_array_set_size (clause->comments, 0);
}
