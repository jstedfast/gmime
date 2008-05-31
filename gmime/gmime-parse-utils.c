/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 *  Copyright (C) 2000-2008 Jeffrey Stedfast
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

static char *
decode_quoted_string (const char **in)
{
	const char *inptr = *in;
	char *out = NULL;
	
	decode_lwsp (&inptr);
	if (*inptr == '"') {
		out = (char *) inptr;
		
		inptr++;
		while (*inptr && *inptr != '"') {
			if (*inptr == '\\')
				inptr++;
			
			if (*inptr)
				inptr++;
		}
		
		if (*inptr == '"')
			inptr++;
		
		out = g_strndup (out, inptr - out);
	}
	
	*in = inptr;
	
	return out;
}

static char *
decode_atom (const char **in)
{
	const char *inptr = *in, *start;
	
	decode_lwsp (&inptr);
	start = inptr;
	while (is_atom (*inptr))
		inptr++;
	*in = inptr;
	if (inptr > start)
		return g_strndup (start, inptr - start);
	else
		return NULL;
}


/**
 * g_mime_decode_word:
 * @in: address of input text string
 *
 * Extracts the next rfc822 'word' token.
 *
 * Returns: the next rfc822 'word' token or %NULL if non exist.
 **/
char *
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
 *
 * Extracts the next rfc822 'domain' token.
 *
 * Returns: the next rfc822 'domain' token or %NULL if non exist.
 **/
char *
g_mime_decode_domain (const char **in)
{
	const char *inptr, *save;
	GString *domain;
	char *dom, *atom;
	
	domain = g_string_new ("");
	
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
			
			g_string_append (domain, atom);
			g_free (atom);
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
	
	if (domain->len)
		dom = domain->str;
	else
		dom = NULL;
	
	g_string_free (domain, dom ? FALSE : TRUE);
	
	*in = inptr;
	
	return dom;
}
