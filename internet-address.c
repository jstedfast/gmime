/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 *  Authors: Jeffrey Stedfast <fejj@ximian.com>
 *
 *  Copyright 2001 Ximain, Inc. (www.ximian.com)
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

#include "internet-address.h"
#include "gmime-table-private.h"
#include "gmime-utils.h"

#include <string.h>
#include <ctype.h>


/**
 * internet_address_new: Create a new Internet Address object
 * 
 * Returns a new Internet Address object.
 **/
InternetAddress *
internet_address_new ()
{
	return g_new0 (InternetAddress, 1);
}


/**
 * internet_address_destroy: Destroy an Internet Address object
 * @ia: Internet Address object to destroy
 * 
 * Destroy the InternetAddress object pointed to by @ia.
 **/
void
internet_address_destroy (InternetAddress *ia)
{
	if (ia) {
		g_free (ia->name);
		
		if (ia->type == INTERNET_ADDRESS_GROUP) {
			GList *members;
			
			members = ia->value.members;
			while (members) {
				InternetAddress *member;
				
				member = members->data;
				internet_address_destroy (member);
				members = members->next;
			}
			
			g_list_free (ia->value.members);
		} else {
			g_free (ia->value.addr);
		}
		
		g_free (ia);
	}
}


/**
 * internet_address_new_name: Create a new Internet Address object
 * @name: person's name
 * @address: person's address
 * 
 * Returns a new Internet Address object.
 **/
InternetAddress *
internet_address_new_name (const char *name, const char *addr)
{
	InternetAddress *ia;
	
	g_return_val_if_fail (addr != NULL, NULL);
	
	ia = internet_address_new ();
	ia->type = INTERNET_ADDRESS_NAME;
	if (name) {
		ia->name = g_mime_utils_8bit_header_decode (name);
		g_mime_utils_unquote_string (ia->name);
	}
	ia->value.addr = g_strdup (addr);
	
	return ia;
}


/**
 * internet_address_new_group: Create a new Internet Address object
 * @name: group name
 * 
 * Returns a new Internet Address object.
 **/
InternetAddress *
internet_address_new_group (const char *name)
{
	InternetAddress *ia;
	
	ia = internet_address_new ();
	ia->type = INTERNET_ADDRESS_GROUP;
	if (name) {
		ia->name = g_mime_utils_8bit_header_decode (name);
		g_mime_utils_unquote_string (ia->name);
	}
	
	return ia;
}


/**
 * internet_address_set_name:
 * @ia: internet address
 * @name: group or contact's name
 *
 * Set the name of the internet address.
 **/
void
internet_address_set_name (InternetAddress *ia, const char *name)
{
	g_return_if_fail (ia != NULL);
	
	g_free (ia->name);
	if (name) {
		ia->name = g_mime_utils_8bit_header_decode (name);
		g_mime_utils_unquote_string (ia->name);
	} else
		ia->name = NULL;
}


/**
 * internet_address_set_addr:
 * @ia: internet address
 * @addr: contact's email address
 *
 * Set the intenet address's address.
 **/
void
internet_address_set_addr (InternetAddress *ia, const char *addr)
{
	g_return_if_fail (ia != NULL);
	g_return_if_fail (ia->type != INTERNET_ADDRESS_GROUP);
	
	ia->type = INTERNET_ADDRESS_NAME;
	g_free (ia->value.addr);
	ia->value.addr = g_strdup (addr);
}


/**
 * internet_address_set_group:
 * @ia: internet address
 * @group: a list of internet addresses
 *
 * Set the members of the internet address group.
 **/
void
internet_address_set_group (InternetAddress *ia, GList *group)
{
	GList *members;
	
	g_return_if_fail (ia != NULL);
	g_return_if_fail (ia->type != INTERNET_ADDRESS_NAME);
	
	ia->type = INTERNET_ADDRESS_GROUP;
	members = ia->value.members;
	while (members) {
		InternetAddress *member;

		member = members->data;
		internet_address_destroy (member);
		members = members->next;
	}
	
	g_list_free (ia->value.members);
	ia->value.members = group;
}


/**
 * internet_address_add_member:
 * @ia: internet address
 * @member: group member's internet address
 *
 * Add a contact to the internet address group.
 **/
void
internet_address_add_member (InternetAddress *ia, InternetAddress *member)
{
	g_return_if_fail (ia != NULL);
	g_return_if_fail (ia->type != INTERNET_ADDRESS_NAME);
	
	ia->type = INTERNET_ADDRESS_GROUP;
	ia->value.members = g_list_append (ia->value.members, member);
}

static gchar *
encoded_name (const gchar *raw, gboolean rfc2047_encode)
{
	gchar *name;
	
	g_return_val_if_fail (raw != NULL, NULL);
	
	if (rfc2047_encode && g_mime_utils_text_is_8bit (raw, strlen (raw))) {
		name = g_mime_utils_8bit_header_encode_phrase (raw);
	} else {
		name = g_mime_utils_quote_string (raw);
	}
	
	return name;
}


/**
 * internet_address_to_string: Write the InternetAddress object to a string
 * @ia: Internet Address object
 * @encode: TRUE if the address should be rfc2047 encoded
 * 
 * Returns the InternetAddress object as an allocated string in rfc822
 * format.
 **/
gchar *
internet_address_to_string (InternetAddress *ia, gboolean encode)
{
	char *string = NULL;
	
	if (ia->type == INTERNET_ADDRESS_NAME) {
		if (ia->name) {
			char *name;
			
			name = encoded_name (ia->name, encode);
			string = g_strdup_printf ("%s <%s>", name,
						  ia->value.addr);
			g_free (name);
		} else {
			string = g_strdup (ia->value.addr);
		}
	} else if (ia->type == INTERNET_ADDRESS_GROUP) {
		GList *members;
		GString *gstr;
		
		gstr = g_string_new (ia->name);
		g_string_append (gstr, ": ");
		
		members = ia->value.members;
		while (members) {
			InternetAddress *member;
			char *addr;
			
			member = members->data;
			members = members->next;
			
			addr = internet_address_to_string (member, encode);
			if (addr) {
				g_string_append (gstr, addr);
				g_free (addr);
				if (members)
					g_string_append (gstr, ", ");
			}
		}
		
		g_string_append (gstr, ";");
		
		string = gstr->str;
		g_string_free (gstr, FALSE);
	}
	
	return string;
}

static void
decode_lwsp (const char **in)
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
	GString *string = NULL;
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

#if 0
static char *
decode_quoted_string (const char **in)
{
	const char *inptr = *in;
	char *out = NULL, *outptr;
	int outlen;
	int c;
	
	decode_lwsp (&inptr);
	if (*inptr == '"') {
		const char *intmp;
		int skip = 0;
		
		/* first, calc length */
		inptr++;
		intmp = inptr + 1;
		while ((c = *intmp++) && c != '"') {
			if (c == '\\' && *intmp) {
				intmp++;
				skip++;
			}
		}
		
		outlen = intmp - inptr - skip;
		out = outptr = g_malloc (outlen + 1);
		
		while ((c = *inptr++) && c != '"') {
			if (c == '\\' && *inptr) {
				c = *inptr++;
			}
			*outptr++ = c;
		}
		*outptr = '\0';
	}
	
	*in = inptr;
	
	return out;
}
#endif

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

static char *
decode_word (const char **in)
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
			g_warning ("Malformed domain-literal, "
				   "unexpected char (%c): %s", *inptr, *in);
			
			/* try and skip to the next char ?? */
			inptr++;
		}
	}
	
	*in = inptr;
}

static char *
decode_domain (const char **in)
{
	const char *inptr, *save;
	GString *domain;
	char *dom, *atom;
	
	domain = g_string_new ("");
	
	inptr = *in;
	while (TRUE) {
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
				g_warning ("Missing ']' in domain-literal: %s", *in);
		} else {
			atom = decode_atom (&inptr);
			if (atom)
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
	
	dom = domain->str;
	g_string_free (domain, FALSE);
	
	*in = inptr;
	
	return dom;
}

static InternetAddress *
decode_mailbox (const char **in)
{
	InternetAddress *mailbox = NULL;
	const char *inptr;
	gboolean bracket = FALSE;
	GString *name = NULL;
	GString *addr;
	char *pre;
	
	addr = g_string_new ("");
	
	decode_lwsp (in);
	inptr = *in;
	
	pre = decode_word (&inptr);
	decode_lwsp (&inptr);
	if (*inptr && !strchr (",.@", *inptr)) {
		/* this mailbox has a name part, so get the name */
		name = g_string_new ("");
		while (pre) {
			g_string_append (name, pre);
			g_free (pre);
			pre = decode_word (&inptr);
			if (pre)
				g_string_append_c (name, ' ');
		}
		
		decode_lwsp (&inptr);
		if (*inptr == '<') {
			inptr++;
			bracket = TRUE;
			pre = decode_word (&inptr);
		} else {
			g_string_free (name, TRUE);
			g_string_free (addr, TRUE);
			*in = inptr;
			
			return NULL;
		}
	}
	
	if (pre) {
		g_string_append (addr, pre);
	} else {
		g_warning ("No local part for email address: %s", *in);
		if (name)
			g_string_free (name, TRUE);
		g_string_free (addr, TRUE);
		return NULL;
	}
	
	/* get the rest of the local-part */
	decode_lwsp (&inptr);
	while (*inptr == '.' && pre) {
		inptr++;
		g_free (pre);
		pre = decode_word (&inptr);
		if (pre) {
			g_string_append_c (addr, '.');
			g_string_append (addr, pre);
		}
		decode_lwsp (&inptr);
	}
	g_free (pre);
	
	/* we should be at the '@' now... */
	if (*inptr == '@') {
		char *domain;
		
		g_string_append_c (addr, '@');
		inptr++;
		
		domain = decode_domain (&inptr);
		g_string_append (addr, domain);
		g_free (domain);
	} else {
		g_warning ("No domain in email address: %s", *in);
	}
	
	if (bracket) {
		decode_lwsp (&inptr);
		if (*inptr == '>')
			inptr++;
		else
			g_warning ("Missing closing '>' bracket for email address: %s", *in);
	}
	
	if (!name) {
		/* look for a trailing comment to use as the name? */
		char *comment;
		
		comment = (char *) inptr;
		decode_lwsp (&inptr);
		if (inptr > comment) {
			comment = memchr (comment, '(', inptr - comment);
			if (comment) {
				const char *cend;
				
				/* find the end of the comment */
				for (cend = inptr - 1; cend > comment && is_lwsp (*cend); cend--);
				if (*cend == ')')
					cend--;
				comment = g_strndup (comment + 1, cend - comment);
				
				name = g_string_new (comment);
				g_free (comment);
			}
		}
	}
	
	*in = inptr;
	
	if (addr->len)
		mailbox = internet_address_new_name (name ? name->str : NULL, addr->str);
	
	g_string_free (addr, TRUE);
	if (name)
		g_string_free (name, TRUE);
	
	return mailbox;
}

static InternetAddress *
decode_address (const char **in)
{
	InternetAddress *addr = NULL, *member;
	const char *inptr, *start;
	GString *name;
	char *pre;
	
	decode_lwsp (in);
	start = inptr = *in;
	
	/* pre-scan */
	name = g_string_new ("");
	pre = decode_word (&inptr);
	while (pre) {
		g_string_append (name, pre);
		g_free (pre);
		
		pre = decode_word (&inptr);
		if (pre)
			g_string_append_c (name, ' ');
	}
	
	decode_lwsp (&inptr);
	if (*inptr == ':') {
		/* this is a group */
		inptr++;
		addr = internet_address_new_group (name->str);
		
		decode_lwsp (&inptr);
		while (*inptr && *inptr != ';') {
			InternetAddress *member;
			
			member = decode_mailbox (&inptr);
			if (member)
				internet_address_add_member (addr, member);
			
			decode_lwsp (&inptr);
			while (*inptr == ',') {
				inptr++;
				decode_lwsp (&inptr);
				member = decode_mailbox (&inptr);
				if (member)
					internet_address_add_member (addr, member);
				
				decode_lwsp (&inptr);
			}
		}
		
		if (*inptr == ';')
			inptr++;
		else
			g_warning ("Invalid group spec, missing closing ';': %.*s",
				   inptr - start, start);
		
		*in = inptr;
	} else {
		/* this is a mailbox */
		addr = decode_mailbox (in);
	}
	
	g_string_free (name, TRUE);
	
	return addr;
}


/**
 * internet_address_paarse_string:
 * @string: a string containing internet addresses
 *
 * Construct a list of internet addresses from the given string.
 *
 * Returns a linked list of internet addresses.
 **/
GList *
internet_address_parse_string (const char *string)
{
	GList *addrlist = NULL;
	const char *inptr;
	
	inptr = string;
	
	while (inptr && *inptr) {
		InternetAddress *addr;
		const char *start;
		
		start = inptr;
		
		addr = decode_address (&inptr);
		
		if (addr)
			addrlist = g_list_append (addrlist, addr);
		else
			g_warning ("Invalid or incomplete address: %.*s", inptr - start, start);
		
		decode_lwsp (&inptr);
		if (*inptr == ',')
			inptr++;
		else if (*inptr) {
			g_warning ("Parse error at '%s': expected ','", inptr);
			/* try skipping to the next address */
			inptr = strchr (inptr, ',');
			if (inptr)
				inptr++;
		}
	}
	
	return addrlist;
}
