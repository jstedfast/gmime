/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 *  Authors: Jeffrey Stedfast <fejj@ximian.com>
 *
 *  Copyright 2002 Ximian, Inc. (www.ximian.com)
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Street #330, Boston, MA 02111-1307, USA.
 *
 */


#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <string.h>

#include "gtrie.h"
#include "url-scanner.h"

#include "gmime-table-private.h"


struct _GUrlScanner {
	GPtrArray *patterns;
	GHashTable *pattern_hash;
	GTrie *trie;
};


GUrlScanner *
g_url_scanner_new (void)
{
	GUrlScanner *scanner;
	
	scanner = g_new (GUrlScanner, 1);
	scanner->patterns = g_ptr_array_new ();
	scanner->pattern_hash = g_hash_table_new (g_str_hash, g_str_equal);
	scanner->trie = g_trie_new (TRUE);
	
	return scanner;
}


void
g_url_scanner_free (GUrlScanner *scanner)
{
	g_return_if_fail (scanner != NULL);
	
	g_ptr_array_free (scanner->patterns, TRUE);
	g_hash_table_destroy (scanner->pattern_hash);
	g_trie_free (scanner->trie);
	g_free (scanner);
}


void
g_url_scanner_add (GUrlScanner *scanner, urlpattern_t *pattern)
{
	g_return_if_fail (scanner != NULL);
	
	g_ptr_array_add (scanner->patterns, pattern);
	g_hash_table_insert (scanner->pattern_hash, pattern->pattern, pattern);
	g_trie_add (scanner->trie, pattern->pattern);
}


gboolean
g_url_scanner_scan (GUrlScanner *scanner, const char *in, size_t inlen, urlmatch_t *match)
{
	const char *pattern, *pos, *inend;
	urlpattern_t *pat;
	
	g_return_val_if_fail (scanner != NULL, FALSE);
	g_return_val_if_fail (in != NULL, FALSE);
	
	if (!(pos = g_trie_search (scanner->trie, in, inlen, &pattern)))
		return FALSE;
	
	pat = g_hash_table_lookup (scanner->pattern_hash, pattern);
	
	inend = in + inlen;
	if (!pat->start (in, pos, inend, match))
		return FALSE;
	
	if (!pat->end (in, pos, inend, match))
		return FALSE;
	
	match->pattern = pattern;
	match->prefix = pat->prefix;
	
	return TRUE;
}


gboolean
g_url_addrspec_start (const char *in, const char *pos, const char *inend, urlmatch_t *match)
{
	register const char *inptr = pos;
	
	g_assert (*inptr == '@');
	
	inptr--;
	
	while (inptr > in) {
		if (is_atom (*inptr))
			inptr--;
		else
			break;
		
		while (inptr > in && is_atom (*inptr))
			inptr--;
		
		if (inptr > in && *inptr == '.')
			inptr--;
	}
	
	if (!is_atom (*inptr))
		inptr++;
	
	if (inptr == pos)
		return FALSE;
	
	match->um_so = (inptr - in);
	
	return TRUE;
}

gboolean
g_url_addrspec_end (const char *in, const char *pos, const char *inend, urlmatch_t *match)
{
	const char *inptr = pos;
	int parts = 0, digits;
	
	g_assert (*inptr == '@');
	
	inptr++;
	
	if (*inptr == '[') {
		/* domain literal */
		do {
			inptr++;
			
			digits = 0;
			while (inptr < inend && is_digit (*inptr) && digits < 3) {
				inptr++;
				digits++;
			}
			
			parts++;
			
			if (*inptr != '.' && parts != 4)
				return FALSE;
		} while (parts < 4);
		
		if (*inptr == ']')
			inptr++;
		else
			return FALSE;
	} else {
		while (inptr < inend) {
			if (is_domain (*inptr))
				inptr++;
			else
				break;
			
			while (inptr < inend && is_domain (*inptr))
				inptr++;
			
			if (inptr < inend && *inptr == '.')
				inptr++;
		}
	}
	
	if (inptr == pos)
		return FALSE;
	
	match->um_eo = (inptr - in);
	
	return TRUE;
}

gboolean
g_url_file_start (const char *in, const char *pos, const char *inend, urlmatch_t *match)
{
	match->um_so = (pos - in);
	
	return TRUE;
}

gboolean
g_url_file_end (const char *in, const char *pos, const char *inend, urlmatch_t *match)
{
	register const char *inptr = pos;
	
	inptr += strlen (match->pattern);
	
	if (*inptr == '/')
		inptr++;
	
	while (inptr < inend && is_urlsafe (*inptr))
		inptr++;
	
	if (inptr == pos)
		return FALSE;
	
	match->um_eo = (inptr - in);
	
	return TRUE;
}

gboolean
g_url_web_start (const char *in, const char *pos, const char *inend, urlmatch_t *match)
{
	match->um_so = (pos - in);
	
	return TRUE;
}

gboolean
g_url_web_end (const char *in, const char *pos, const char *inend, urlmatch_t *match)
{
	register const char *inptr = pos;
	int parts = 0, digits, port;
	
	inptr += strlen (match->pattern);
	
	/* find the end of the domain */
	if (is_digit (*inptr)) {
		/* domain-literal */
		do {
			digits = 0;
			while (inptr < inend && is_digit (*inptr) && digits < 3) {
				inptr++;
				digits++;
			}
			
			parts++;
			
			if (*inptr != '.' && parts != 4)
				return FALSE;
			else if (*inptr == '.')
				inptr++;
			
		} while (parts < 4);
	} else if (is_domain (*inptr)) {
		do {
			while (inptr < inend && is_domain (*inptr))
				inptr++;
			
			if (inptr < inend && *inptr == '.')
				inptr++;
			else
				break;
			
		} while (inptr < inend);
	} else {
		return FALSE;
	}
	
	if (inptr < inend && *inptr == ':') {
		/* skip past the port */
		inptr++;
		port = 0;
		
		while (inptr < inend && is_digit (*inptr) && port < 65536)
			port = (port * 10) + (*inptr++ - '0');
	}
	
	if (inptr < inend && *inptr == '/') {
		/* skip past our url path */
		inptr++;
		
		while (inptr < inend && is_urlsafe (*inptr))
			inptr++;
	}
	
	match->um_eo = (inptr - in);
	
	return TRUE;
}
