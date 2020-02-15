/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*  GMime
 *  Copyright (C) 2000-2020 Jeffrey Stedfast
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

#include "gmime-table-private.h"
#include "gmime-parse-utils.h"


#ifdef ENABLE_WARNINGS
#define w(x) x
#else
#define w(x)
#endif /* ENABLE_WARNINGS */

#define d(x)


/**
 * g_mime_parse_content_type:
 * @in: address of input text string
 * @type: address of the 'type' output string
 * @subtype: address of the 'subtype' output string
 *
 * Decodes the simple Content-Type type/subtype tokens and updates @in
 * to point to the first char after the end of the subtype.
 *
 * Returns: %TRUE if the string was successfully parsed or %FALSE
 * otherwise.
 **/
gboolean
g_mime_parse_content_type (const char **in, char **type, char **subtype)
{
	register const char *inptr;
	const char *start = *in;
	
	skip_cfws (&start);
	inptr = start;
	
	/* decode the type */
	while (*inptr && is_ttoken (*inptr))
		inptr++;
	
	*type = g_strndup (start, (size_t) (inptr - start));
	
	start = inptr;
	skip_cfws (&start);
	
	/* check for type/subtype delimeter */
	if (*start++ != '/') {
		g_free (*type);
		*subtype = NULL;
		*type = NULL;
		return FALSE;
	}
	
	skip_cfws (&start);
	inptr = start;
	
	/* decode the subtype */
	while (*inptr && is_ttoken (*inptr))
		inptr++;
	
	/* check that the subtype exists */
	if (inptr == start) {
		g_free (*type);
		*subtype = NULL;
		*type = NULL;
		return FALSE;
	}
	
	*subtype = g_strndup (start, (size_t) (inptr - start));
	
	/* update the input string pointer */
	*in = inptr;
	
	return TRUE;
}


/**
 * g_mime_skip_comment:
 * @in: address of input string
 *
 * Skips a comment.
 *
 * Returns: %TRUE on success or %FALSE otherwise.
 **/
gboolean
g_mime_skip_comment (const char **in)
{
	register const char *inptr = *in;
	int depth = 1;
	
	/* skip over the '(' */
	inptr++;
	while (*inptr && depth) {
		if (*inptr == '\\' && *(inptr + 1))
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
 *
 * Skips whitespace.
 *
 * Returns: %TRUE if any input was skipped or %FALSE otherwise.
 **/
gboolean
g_mime_skip_lwsp (const char **in)
{
	register const char *inptr = *in;
	const char *start = inptr;
	
	while (is_lwsp (*inptr))
		inptr++;
	
	*in = inptr;
	
	return inptr > start;
}


/**
 * g_mime_skip_cfws:
 * @in: address of input string
 *
 * Skips comments and whitespace.
 *
 * Returns: %TRUE on success or %FALSE on error.
 **/
gboolean
g_mime_skip_cfws (const char **in)
{
	const char *inptr = *in;
	
	skip_lwsp (&inptr);
	
	while (*inptr == '(') {
		if (!skip_comment (&inptr))
			return FALSE;
		
		skip_lwsp (&inptr);
	}
	
	*in = inptr;
	
	return TRUE;
}


/**
 * g_mime_skip_quoted:
 * @in: address of input string
 *
 * Skips a quoted string.
 *
 * Returns: %TRUE on success or %FALSE on error.
 **/
gboolean
g_mime_skip_quoted (const char **in)
{
	register const char *inptr = *in;
	gboolean escaped = FALSE;
	
	/* skip over leading '"' */
	inptr++;
	
	while (*inptr) {
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
	
	if (*inptr == '\0') {
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
 *
 * Skips an atom.
 *
 * Returns: %TRUE if any input was skipped or %FALSE otherwise.
 **/
gboolean
g_mime_skip_atom (const char **in)
{
	register const char *inptr = *in;
	const char *start = inptr;
	
	while (is_atom (*inptr))
		inptr++;
	
	*in = inptr;
	
	return inptr > start;
}


/**
 * g_mime_skip_word:
 * @in: address of input string
 *
 * Skips a word token.
 *
 * Returns: %TRUE on success or %FALSE otherwise.
 **/
gboolean
g_mime_skip_word (const char **in)
{
	if (**in == '"')
		return skip_quoted (in);
	
	if (is_atom (**in))
		return skip_atom (in);
	
	return FALSE;
}


static const char *
decode_quoted_string (const char **in)
{
	register const char *inptr = *in;
	const char *qstring = NULL;
	
	if (*inptr == '"') {
		qstring = inptr;
		
		inptr++;
		while (*inptr && *inptr != '"') {
			if (*inptr == '\\')
				inptr++;
			
			if (*inptr)
				inptr++;
		}
		
		if (*inptr == '"')
			inptr++;
		
		*in = inptr;
	}
	
	return qstring;
}

static const char *
decode_atom (const char **in)
{
	register const char *inptr = *in;
	const char *atom = NULL;
	
	if (!is_atom (*inptr))
		return NULL;
	
	atom = inptr++;
	while (is_atom (*inptr))
		inptr++;
	
	*in = inptr;
	
	return atom;
}


/**
 * g_mime_decode_word:
 * @in: address of input text string
 *
 * Extracts the next rfc822 'word' token.
 *
 * Returns: the next rfc822 'word' token or %NULL if non exist.
 **/
const char *
g_mime_decode_word (const char **in)
{
	const char *inptr = *in;
	
	skip_cfws (&inptr);
	if (*inptr == '"') {
		*in = inptr;
		return decode_quoted_string (in);
	} else {
		*in = inptr;
		return decode_atom (in);
	}
}

static gboolean
decode_subliteral (const char **in, GString *domain)
{
	const char *inptr = *in;
	gboolean got = FALSE;
	
	while (*inptr && *inptr != '.' && *inptr != ']') {
		if (is_dtext (*inptr)) {
			g_string_append_c (domain, *inptr);
			inptr++;
			got = TRUE;
		} else if (is_lwsp (*inptr)) {
			skip_lwsp (&inptr);
		} else {
			break;
		}
	}
	
	*in = inptr;
	
	return got;
}

static void
decode_domain_literal (const char **in, GString *domain)
{
	const char *inptr = *in;
	
	skip_cfws (&inptr);
	while (*inptr && *inptr != ']') {
		if (decode_subliteral (&inptr, domain) && *inptr == '.') {
			g_string_append_c (domain, *inptr);
			inptr++;
		} else if (*inptr != ']') {
			w(g_warning ("Malformed domain-literal, unexpected char '%c': %s",
				     *inptr, *in));
			
			/* try and skip to the next char ?? */
			if (*inptr)
				inptr++;
		}
	}
	
	*in = inptr;
}


/**
 * g_mime_decode_domain:
 * @in: address of input text string
 * @domain: a #GString to decode the domain into
 *
 * Extracts the next rfc822 'domain' token and appends it to @domain.
 *
 * Returns: %TRUE if an rfc822 'domain' token was decoded or %FALSE
 * otherwise.
 **/
gboolean
g_mime_decode_domain (const char **in, GString *domain)
{
	const char *inptr, *save, *atom;
	size_t initial = domain->len;
	
	inptr = *in;
	while (inptr && *inptr) {
		skip_cfws (&inptr);
		if (*inptr == '[') {
			/* domain literal */
			g_string_append_c (domain, '[');
			inptr++;
			
			decode_domain_literal (&inptr, domain);
			
			if (*inptr == ']') {
				g_string_append_c (domain, ']');
				inptr++;
			} else {
				w(g_warning ("Missing ']' in domain-literal: %s", *in));
			}
		} else {
			if (!(atom = decode_atom (&inptr))) {
				w(g_warning ("Unexpected char '%c' in domain: %s", *inptr, *in));
				/* remove the last '.' */
				if (domain->len && domain->str[domain->len - 1] == '.')
					g_string_truncate (domain, domain->len - 1);
				break;
			}
			
			g_string_append_len (domain, atom, (size_t) (inptr - atom));
		}
		
		save = inptr;
		skip_cfws (&inptr);
		if (*inptr != '.') {
			inptr = save;
			break;
		}
		
		g_string_append_c (domain, '.');
		inptr++;
	}
	
	*in = inptr;
	
	return domain->len > initial;
}


char *
g_mime_decode_addrspec (const char **in)
{
	const char *word, *inptr;
	GString *addrspec;
	char *str;
	
	skip_cfws (in);
	inptr = *in;
	
	if (!(word = decode_word (&inptr))) {
		w(g_warning ("No local-part in addr-spec: %s", *in));
		return NULL;
	}
	
	addrspec = g_string_new ("");
	g_string_append_len (addrspec, word, (size_t) (inptr - word));
	
	/* get the rest of the local-part */
	skip_cfws (&inptr);
	while (*inptr == '.') {
		g_string_append_c (addrspec, *inptr++);
		if ((word = decode_word (&inptr))) {
			g_string_append_len (addrspec, word, (size_t) (inptr - word));
			skip_cfws (&inptr);
		} else {
			w(g_warning ("Invalid local-part in addr-spec: %s", *in));
			goto exception;
		}
	}
	
	/* we should be at the '@' now... */
	if (*inptr++ != '@') {
		w(g_warning ("Invalid addr-spec; missing '@': %s", *in));
		goto exception;
	}
	
	g_string_append_c (addrspec, '@');
	if (!decode_domain (&inptr, addrspec)) {
		w(g_warning ("No domain in addr-spec: %s", *in));
		goto exception;
	}
	
	str = addrspec->str;
	g_string_free (addrspec, FALSE);
	
	*in = inptr;
	
	return str;
	
 exception:
	
	g_string_free (addrspec, TRUE);
	
	return NULL;
}

char *
g_mime_decode_msgid (const char **in)
{
	const char *inptr = *in;
	char *msgid = NULL;
	
	skip_cfws (&inptr);
	if (*inptr != '<') {
		w(g_warning ("Invalid msg-id; missing '<': %s", *in));
	} else {
		inptr++;
	}
	
	skip_cfws (&inptr);
	if ((msgid = decode_addrspec (&inptr))) {
		skip_cfws (&inptr);
		if (*inptr != '>') {
			w(g_warning ("Invalid msg-id; missing '>': %s", *in));
		} else {
			inptr++;
		}
		
		*in = inptr;
	} else {
		w(g_warning ("Invalid msg-id; missing addr-spec: %s", *in));
		*in = inptr;
		while (*inptr && *inptr != '>')
			inptr++;
		
		msgid = g_strndup (*in, (size_t) (inptr - *in));
		*in = inptr;
	}
	
	return msgid;
}
