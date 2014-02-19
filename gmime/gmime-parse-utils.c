/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*  GMime
 *  Copyright (C) 2000-2014 Jeffrey Stedfast
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
	
	decode_lwsp (&start);
	inptr = start;
	
	/* decode the type */
	while (*inptr && is_ttoken (*inptr))
		inptr++;
	
	*type = g_strndup (start, (size_t) (inptr - start));
	
	start = inptr;
	decode_lwsp (&start);
	
	/* check for type/subtype delimeter */
	if (*start++ != '/') {
		g_free (*type);
		*subtype = NULL;
		*type = NULL;
		return FALSE;
	}
	
	decode_lwsp (&start);
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
 * g_mime_decode_lwsp:
 * @in: address of input text string
 *
 * Skips past any LWSP or rfc822 comments in *@in and updates @in.
 **/
void
g_mime_decode_lwsp (const char **in)
{
	const char *inptr = *in;
	
	while (*inptr && (*inptr == '(' || is_lwsp (*inptr))) {
		while (*inptr && is_lwsp (*inptr))
			inptr++;
		
		/* skip over any comments */
		if (*inptr == '(') {
			int depth = 1;
			
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
		}
	}
	
	*in = inptr;
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
	
	decode_lwsp (&inptr);
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
		} else if (is_lwsp (*inptr))
			decode_lwsp (&inptr);
		else
			break;
	}
	
	*in = inptr;
	
	return got;
}

static void
decode_domain_literal (const char **in, GString *domain)
{
	const char *inptr = *in;
	
	decode_lwsp (&inptr);
	while (*inptr && *inptr != ']') {
		if (decode_subliteral (&inptr, domain) && *inptr == '.') {
			g_string_append_c (domain, *inptr);
			inptr++;
		} else if (*inptr != ']') {
			w(g_warning ("Malformed domain-literal, unexpected char '%c': %s",
				     *inptr, *in));
			
			/* try and skip to the next char ?? */
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
		decode_lwsp (&inptr);
		if (*inptr == '[') {
			/* domain literal */
			g_string_append_c (domain, '[');
			inptr++;
			
			decode_domain_literal (&inptr, domain);
			
			if (*inptr == ']') {
				g_string_append_c (domain, ']');
				inptr++;
			} else
				w(g_warning ("Missing ']' in domain-literal: %s", *in));
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
		decode_lwsp (&inptr);
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
