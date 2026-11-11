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

#include <glib.h>
#include <string.h>

#include "gmime-table-private.h"
#include "gmime-parse-utils.h"
#include "gmime-common.h"
#include "gmime-error.h"


#ifdef ENABLE_WARNINGS
#define w(x) x
#else
#define w(x)
#endif /* ENABLE_WARNINGS */

#define d(x)

gboolean
_g_mime_parse_error (GError **error, const char *text, const char *inptr, const char *reason)
{
	g_set_error (error, GMIME_ERROR, GMIME_ERROR_PARSE_ERROR,
		     "%s at byte offset %" G_GSIZE_FORMAT ".", reason, (gsize) (inptr - text));
	return FALSE;
}

gboolean
_g_mime_parse_validate_text (const char *str, gssize *length, GError **error)
{
	const char *nul;

	if (*length < 0)
		*length = strlen (str);

	if (*length > G_MAXINT) {
		g_set_error_literal (error, GMIME_ERROR, GMIME_ERROR_PARSE_ERROR,
				     "Input text exceeds the supported length.");
		return FALSE;
	}

	if ((nul = memchr (str, '\0', *length))) {
		_g_mime_parse_error (error, str, nul, "Embedded NUL in input text");
		return FALSE;
	}

	return TRUE;
}

gboolean
_g_mime_parse_int32 (const char **in, const char *end, gint32 *value)
{
	const char *p = *in;
	gint32 number = 0;
	int digit;

	if (p == end || *p < '0' || *p > '9')
		return FALSE;

	do {
		digit = *p++ - '0';
		if (number > (G_MAXINT32 - digit) / 10)
			return FALSE;
		number = number * 10 + digit;
	} while (p < end && *p >= '0' && *p <= '9');

	*value = number;
	*in = p;
	return TRUE;
}

char *
_g_mime_parse_message_id_value (const char *value)
{
	const char *inptr = value, *start, *end;
	gboolean angle_addr, quoted = FALSE;
	GString *text;
	char *msgid;

	if (!g_utf8_validate (value, -1, NULL))
		return NULL;

	while (is_lwsp (*inptr))
		inptr++;
	
	start = inptr;
	end = inptr;
	
	while (*inptr) {
		if (!is_lwsp (*inptr++))
			end = inptr;
	}

	if (start == end)
		return NULL;

	angle_addr = *start == '<';
	text = g_string_sized_new (end - start);

	for (inptr = start; inptr < end; inptr++) {
		if (*inptr == '\r' || *inptr == '\n') {
			if (*inptr == '\r' && inptr + 1 < end && inptr[1] == '\n')
				inptr++;

			if (inptr + 1 == end || (inptr[1] != ' ' && inptr[1] != '\t'))
				goto error;
		} else {
			g_string_append_c (text, *inptr);
		}
	}

	if (!angle_addr) {
		for (inptr = text->str; *inptr; inptr++) {
			if (quoted) {
				if (*inptr == '\\' && inptr[1])
					inptr++;
				else if (*inptr == '"')
					quoted = FALSE;
			} else if (*inptr == '"') {
				quoted = TRUE;
			} else if (*inptr == ' ' || *inptr == '\t') {
				goto error;
			}
		}

		g_string_prepend_c (text, '<');
		g_string_append_c (text, '>');
	}

	inptr = text->str;
	msgid = decode_msgid (&inptr, text->str + text->len);

	if (msgid && (!skip_cfws (&inptr, text->str + text->len) || *inptr))
		g_clear_pointer (&msgid, g_free);

	g_string_free (text, TRUE);

	return msgid;

 error:
	g_string_free (text, TRUE);
	return NULL;
}

gboolean
_g_mime_parse_cfws (const char *text, const char **in, const char *end, GError **error)
{
	skip_lwsp (in, end);

	while (*in < end && **in == '(') {
		if (!skip_comment (in, end))
			return _g_mime_parse_error (error, text, *in, "Incomplete comment");

		skip_lwsp (in, end);
	}

	return TRUE;
}


/**
 * g_mime_parse_content_type:
 * @in: address of input text string
 * @end: exclusive end of the input
 * @type: (nullable): address of the 'type' output string, or %NULL to skip allocation
 * @subtype: (nullable): address of the 'subtype' output string, or %NULL to skip allocation
 *
 * Decodes the simple Content-Type type/subtype tokens and updates @in
 * to point to the first char after the end of the subtype.
 *
 * Returns: %TRUE if the string was successfully parsed or %FALSE
 * otherwise.
 **/
gboolean
g_mime_parse_content_type (const char **in, const char *end, char **type, char **subtype)
{
	register const char *inptr;
	const char *start = *in;
	const char *type_start;
	const char *type_end;

	if (type)
		*type = NULL;

	if (subtype)
		*subtype = NULL;
	
	skip_cfws (&start, end);
	type_start = start;
	inptr = start;
	
	/* decode the type */
	while (inptr < end && is_ttoken (*inptr))
		inptr++;
	
	type_end = inptr;
	
	start = inptr;
	skip_cfws (&start, end);
	
	/* check for type/subtype delimeter */
	if (start == end || *start != '/')
		return FALSE;
	start++;
	
	skip_cfws (&start, end);
	inptr = start;
	
	/* decode the subtype */
	while (inptr < end && is_ttoken (*inptr))
		inptr++;
	
	/* check that the subtype exists */
	if (inptr == start)
		return FALSE;
	
	if (type)
		*type = g_strndup (type_start, (size_t) (type_end - type_start));

	if (subtype)
		*subtype = g_strndup (start, (size_t) (inptr - start));
	
	/* update the input string pointer */
	*in = inptr;
	
	return TRUE;
}


/**
 * g_mime_skip_comment:
 * @in: address of input string
 * @end: exclusive end of the input
 *
 * Skips a comment.
 *
 * Returns: %TRUE on success or %FALSE otherwise.
 **/
gboolean
g_mime_skip_comment (const char **in, const char *end)
{
	register const char *inptr = *in;
	gsize depth = 1;

	if (inptr == end || *inptr != '(')
		return FALSE;
	
	/* skip over the '(' */
	inptr++;
	while (inptr < end && depth) {
		if (*inptr == '\\' && inptr + 1 < end)
			inptr++;
		else if (*inptr == '(')
			depth++;
		else if (*inptr == ')')
			depth--;
		
		inptr++;
	}
	
	*in = inptr;
	
	return depth == 0;
}


/**
 * g_mime_skip_lwsp:
 * @in: address of input string
 * @end: exclusive end of the input
 *
 * Skips whitespace.
 *
 * Returns: %TRUE if any input was skipped or %FALSE otherwise.
 **/
gboolean
g_mime_skip_lwsp (const char **in, const char *end)
{
	register const char *inptr = *in;
	const char *start = inptr;
	
	while (inptr < end && is_lwsp (*inptr))
		inptr++;
	
	*in = inptr;
	
	return inptr > start;
}


/**
 * g_mime_skip_cfws:
 * @in: address of input string
 * @end: exclusive end of the input
 *
 * Skips comments and whitespace.
 *
 * Returns: %TRUE on success or %FALSE on error.
 **/
gboolean
g_mime_skip_cfws (const char **in, const char *end)
{
	const char *inptr = *in;
	
	skip_lwsp (&inptr, end);
	
	while (inptr < end && *inptr == '(') {
		if (!skip_comment (&inptr, end))
			return FALSE;
		
		skip_lwsp (&inptr, end);
	}
	
	*in = inptr;
	
	return TRUE;
}


/**
 * g_mime_skip_quoted:
 * @in: address of input string
 * @end: exclusive end of the input
 *
 * Skips a quoted string.
 *
 * Returns: %TRUE on success or %FALSE on error.
 **/
gboolean
g_mime_skip_quoted (const char **in, const char *end)
{
	register const char *inptr = *in;
	gboolean escaped = FALSE;

	if (inptr == end || *inptr != '"')
		return FALSE;
	
	/* skip over leading '"' */
	inptr++;
	
	while (inptr < end) {
		if (*inptr == '\\') {
			escaped = !escaped;
		} else if (!escaped) {
			if (*inptr == '"')
				break;
		} else {
			escaped = FALSE;
		}
		
		inptr++;
	}
	
	if (inptr == end) {
		*in = inptr;
		
		return FALSE;
	}
	
	/* skip over the closing '"' */
	inptr++;
	
	*in = inptr;
	
	return TRUE;
}


/**
 * g_mime_skip_atom:
 * @in: address of input string
 * @end: exclusive end of the input
 *
 * Skips an atom.
 *
 * Returns: %TRUE if any input was skipped or %FALSE otherwise.
 **/
gboolean
g_mime_skip_atom (const char **in, const char *end)
{
	register const char *inptr = *in;
	const char *start = inptr;
	
	while (inptr < end && is_atom (*inptr))
		inptr++;
	
	*in = inptr;
	
	return inptr > start;
}


/**
 * g_mime_skip_word:
 * @in: address of input string
 * @end: exclusive end of the input
 *
 * Skips a word token.
 *
 * Returns: %TRUE on success or %FALSE otherwise.
 **/
gboolean
g_mime_skip_word (const char **in, const char *end)
{
	if (*in == end)
		return FALSE;

	if (**in == '"')
		return skip_quoted (in, end);
	
	if (is_atom (**in))
		return skip_atom (in, end);
	
	return FALSE;
}


static gboolean
dotatom_parse (GString *str, const char **in, const char *end, const char *sentinels, gboolean replace_nuls)
{
	const char *atom, *comment;
	const char *inptr = *in;
	GString *domain = g_string_new (NULL);

	do {
		if (inptr == end || (!is_atom (*inptr) && !(replace_nuls && *inptr == '\0')))
			goto error;

		do {
			atom = inptr;
			while (inptr < end && is_atom (*inptr))
				inptr++;

			if (!g_utf8_validate (atom, inptr - atom, NULL))
				goto error;

			g_string_append_len (domain, atom, inptr - atom);

			if (inptr == end || !replace_nuls || *inptr != '\0')
				break;

			g_string_append (domain, "\357\277\275");
			inptr++;
		} while (inptr < end && (is_atom (*inptr) || *inptr == '\0'));

		comment = inptr;
		if (!skip_cfws (&inptr, end))
			goto error;

		if (inptr == end || *inptr != '.') {
			inptr = comment;
			break;
		}

		/* skip over the '.' */
		inptr++;

		if (!skip_cfws (&inptr, end))
			goto error;

		/* allow domains to end with a '.', but strip it off */
		if (inptr == end || (*inptr && strchr (sentinels, *inptr)))
			break;

		g_string_append_c (domain, '.');
	} while (TRUE);

	g_string_append_len (str, domain->str, domain->len);
	g_string_free (domain, TRUE);

	*in = inptr;

	return TRUE;

 error:
	g_string_free (domain, TRUE);

	*in = inptr;

	return FALSE;
}

static gboolean
domain_literal_parse (GString *str, const char **in, const char *end, gboolean replace_nuls)
{
	const char *inptr = *in;

	g_string_append_c (str, '[');
	inptr++;

	skip_lwsp (&inptr, end);

	do {
		const char *start = inptr;

		while (inptr < end && (guchar) *inptr >= 33 && (guchar) *inptr < 127 && is_dtext (*inptr))
			inptr++;

		g_string_append_len (str, start, (size_t) (inptr - start));

		skip_lwsp (&inptr, end);

		if (inptr == end)
			goto error;

		if (replace_nuls && *inptr == '\0') {
			g_string_append (str, "\357\277\275");
			inptr++;
			continue;
		}

		if (*inptr == ']')
			break;

		if (!is_ascii (*inptr) || is_ctrl (*inptr) || !is_dtext (*inptr))
			goto error;
	} while (TRUE);

	g_string_append_c (str, ']');
	*in = inptr + 1;

	return TRUE;

 error:
	*in = inptr;

	return FALSE;
}

gboolean
_g_mime_parse_domain (GString *domain, const char **in, const char *end, const char *sentinels, gboolean replace_nuls)
{
	if (*in == end)
		return FALSE;

	if (**in == '[')
		return domain_literal_parse (domain, in, end, replace_nuls);

	return dotatom_parse (domain, in, end, sentinels, replace_nuls);
}

char *
g_mime_decode_msgid (const char **in, const char *end)
{
	const char *inptr = *in, *start, *p;
	gboolean angle_addr, square_brackets;
	GString *msgid = NULL;

	if (!skip_cfws (&inptr, end) || inptr == end)
		goto error;

	angle_addr = *inptr == '<';
	if (angle_addr)
		inptr++;

	skip_lwsp (&inptr, end);
	if (inptr == end)
		goto error;

	msgid = g_string_new (NULL);
	/* Exchange sometimes wraps the msg-id inside square brackets as well as angles.
	 * https://github.com/jstedfast/MimeKit/issues/912 */
	square_brackets = *inptr == '[';

	/* Use a deliberately loose local-part grammar to accept real-world msg-ids.
	 * https://github.com/jstedfast/MimeKit/issues/472 */
	do {
		start = inptr;
		if (*inptr == '"') {
			if (!skip_quoted (&inptr, end))
				goto error;
		} else {
			while (inptr < end && *inptr != '.' && *inptr != '@' && *inptr != '>' && !is_lwsp (*inptr))
				inptr++;
		}

		p = start;
		while (p < inptr && !g_utf8_validate (p, inptr - p, &p) && *p == '\0')
			p++;
		if (p < inptr)
			goto error;

		/* Unfold quoted strings without discarding their literal spaces or tabs. */
		for (p = start; p < inptr; p++) {
			if (*p == '\0')
				g_string_append (msgid, "\357\277\275");
			else if (*start != '"' || (*p != '\r' && *p != '\n'))
				g_string_append_c (msgid, *p);
		}

		skip_lwsp (&inptr, end);
		if (inptr == end) {
			if (angle_addr)
				goto error;
			break;
		}

		if (*inptr == '@' || *inptr == '>')
			break;

		if (*inptr == '.') {
			g_string_append_c (msgid, '.');
			inptr++;
			skip_lwsp (&inptr, end);
		}

		if (inptr == end)
			goto error;
	} while (TRUE);

	if (inptr < end && *inptr == '@') {
		g_string_append_c (msgid, '@');
		/* Recover <local-part@@domain> by collapsing repeated separators.
		 * https://github.com/jstedfast/MimeKit/issues/962 */
		do {
			inptr++;
		} while (inptr < end && *inptr == '@');

		if (!skip_cfws (&inptr, end))
			goto error;

		/* Also accept <local-part@> for compatibility with existing messages.
		 * https://github.com/jstedfast/MimeKit/issues/102 */
		if (inptr < end && *inptr != '>') {
			do {
				if (!_g_mime_parse_domain (msgid, &inptr, end, ">@", TRUE))
					goto error;

				if (inptr == end || *inptr != '@')
					break;

				/* Preserve broken <local-part@domain1@domain2> identifiers.
				 * https://github.com/jstedfast/MailKit/issues/138 */
				g_string_append_c (msgid, '@');
				inptr++;
			} while (inptr < end);

			if (!skip_cfws (&inptr, end))
				goto error;
		}
	}

	if (square_brackets && inptr < end && *inptr == ']') {
		g_string_append_c (msgid, ']');
		inptr++;
	}

	if (angle_addr && (inptr == end || *inptr != '>'))
		goto error;

	if (inptr < end && *inptr == '>')
		inptr++;

	if (msgid->len == 0)
		goto error;

	*in = inptr;

	return g_string_free (msgid, FALSE);

 error:
	w(g_warning ("Invalid msg-id: %.*s", (int) MIN (end - *in, G_MAXINT), *in));
	if (msgid)
		g_string_free (msgid, TRUE);
	*in = inptr;

	return NULL;
}

char *
_g_mime_parse_reference (const char **in, const char *end)
{
	char *msgid;

	while (*in < end) {
		if (!skip_cfws (in, end) || *in == end)
			break;
		if (**in == '<') {
			if ((msgid = decode_msgid (in, end)))
				return msgid;
		} else if (!skip_word (in, end) && *in < end) {
			(*in)++;
		}
	}

	return NULL;
}
