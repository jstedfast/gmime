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

#include <string.h>
#include <ctype.h>
#include <errno.h>

#include "internet-address.h"
#include "gmime-table-private.h"
#include "gmime-utils.h"
#include "gmime-iconv-utils.h"


#define w(x) x


extern int gmime_interfaces_utf8;


/**
 * internet_address_new:
 *
 * Creates a new InternetAddress object
 * 
 * Returns a new Internet Address object.
 **/
InternetAddress *
internet_address_new ()
{
	InternetAddress *ia;
	
	ia = g_new (InternetAddress, 1);
	ia->type = INTERNET_ADDRESS_NONE;
	ia->refcount = 1;
	ia->name = NULL;
	ia->value.addr = NULL;
	
	return ia;
}


/**
 * internet_address_destroy:
 * @ia: InternetAddress object to destroy
 * 
 * Destroy the InternetAddress object pointed to by @ia.
 **/
static void
internet_address_destroy (InternetAddress *ia)
{
	if (ia) {
		g_free (ia->name);
		
		if (ia->type == INTERNET_ADDRESS_GROUP) {
			internet_address_list_destroy (ia->value.members);
		} else {
			g_free (ia->value.addr);
		}
		
		g_free (ia);
	}
}


/**
 * internet_address_ref:
 * @ia: address
 *
 * Ref's the internet address.
 **/
void
internet_address_ref (InternetAddress *ia)
{
	ia->refcount++;
}


/**
 * internet_address_unref:
 * @ia: address
 *
 * Unref's the internet address.
 **/
void
internet_address_unref (InternetAddress *ia)
{
	if (ia->refcount <= 1) {
		internet_address_destroy (ia);
	} else {
		ia->refcount--;
	}
}


/**
 * internet_address_new_name:
 * @name: person's name
 * @addr: person's address
 *
 * Creates a new InternetAddress object with name @name and address
 * @addr.
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
 * internet_address_new_group:
 * @name: group name
 *
 * Creates a new InternetAddress object with group name @name.
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
 * Set the internet address's address.
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
internet_address_set_group (InternetAddress *ia, InternetAddressList *group)
{
	/*InternetAddressList *members, *next; */
	
	g_return_if_fail (ia != NULL);
	g_return_if_fail (ia->type != INTERNET_ADDRESS_NAME);
	
	ia->type = INTERNET_ADDRESS_GROUP;
	internet_address_list_destroy (ia->value.members);
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
	g_return_if_fail (member != NULL);
	
	ia->type = INTERNET_ADDRESS_GROUP;
	ia->value.members = internet_address_list_append (ia->value.members, member);
}


/**
 * internet_address_list_prepend:
 * @list: a list of internet addresses
 * @ia: internet address to prepend
 *
 * Prepends the internet address to the list of internet addresses
 * pointed to by @list.
 *
 * Returns the resultant list.
 **/
InternetAddressList *
internet_address_list_prepend (InternetAddressList *list, InternetAddress *ia)
{
	InternetAddressList *node;
	
	g_return_val_if_fail (ia!=NULL, NULL);
	
	internet_address_ref (ia);
	node = g_new (InternetAddressList, 1);
	node->next = list;
	node->address = ia;
	
	return node;
}


/**
 * internet_address_list_append:
 * @list: a list of internet addresses
 * @ia: internet address to append
 *
 * Appends the internet address to the list of internet addresses
 * pointed to by @list.
 *
 * Returns the resultant list.
 **/
InternetAddressList *
internet_address_list_append (InternetAddressList *list, InternetAddress *ia)
{
	InternetAddressList *node, *n;
	
	g_return_val_if_fail (ia!=NULL, NULL);
	
	internet_address_ref (ia);
	node = g_new (InternetAddressList, 1);
	node->next = NULL;
	node->address = ia;
	
	if (list == NULL)
		return node;
	
	n = list;
	while (n->next)
		n = n->next;
	
	n->next = node;
	
	return list;
}


/**
 * internet_address_list_concat:
 * @a: first list
 * @b: second list
 *
 * Concatenates a copy of list @b onto the end of list @a.
 *
 * Returns the resulting list.
 **/
InternetAddressList *
internet_address_list_concat (InternetAddressList *a, InternetAddressList *b)
{
	InternetAddressList *node, *tail, *n;
	
	if (b == NULL)
		return a;
	
	/* find the end of list a */
	if (a != NULL) {
		tail = a;
		while (tail->next)
			tail = tail->next;
	} else {
		tail = (InternetAddressList *) &a;
	}
	
	/* concat a copy of list b to list a */
	node = b;
	while (node) {
		internet_address_ref (node->address);
		n = g_new (InternetAddressList, 1);
		n->next = NULL;
		n->address = node->address;
		tail->next = n;
		tail = n;
		
		node = node->next;
	}
	
	return a;
}


/**
 * internet_address_list_length:
 * @list: list of internet addresses
 *
 * Calculates the length of the list of addresses.
 *
 * Returns the number of internet addresses in @list.
 **/
int
internet_address_list_length (InternetAddressList *list)
{
	InternetAddressList *node;
	int len = 0;
	
	node = list;
	while (node) {
		node = node->next;
		len++;
	}
	
	return len;
}


/**
 * internet_address_list_destroy:
 * @list: address list
 *
 * Destroys the list of internet addresses.
 **/
void
internet_address_list_destroy (InternetAddressList *list)
{
	InternetAddressList *node, *next;
	
	node = list;
	while (node) {
		next = node->next;
		internet_address_unref (node->address);
		g_free (node);
		node = next;
	}
}


static char *
encoded_name (const char *raw, gboolean rfc2047_encode)
{
	char *name;
	
	g_return_val_if_fail (raw != NULL, NULL);
	
	if (rfc2047_encode && g_mime_utils_text_is_8bit (raw, strlen (raw))) {
		name = g_mime_utils_8bit_header_encode_phrase (raw);
	} else {
		name = g_mime_utils_quote_string (raw);
	}
	
	return name;
}

static void
internet_address_list_to_string_internal (InternetAddressList *list, gboolean encode, GString *string)
{
	while (list) {
		char *addr;
		
		addr = internet_address_to_string (list->address, encode);
		if (addr) {
			g_string_append (string, addr);
			g_free (addr);
			if (list->next)
				g_string_append (string, ", ");
		}
		
		list = list->next;
	}
}


/**
 * internet_address_to_string:
 * @ia: Internet Address object
 * @encode: TRUE if the address should be rfc2047 encoded
 *
 * Allocates a string containing the contents of the InternetAddress
 * object.
 * 
 * Returns the InternetAddress object as an allocated string in rfc822
 * format.
 **/
char *
internet_address_to_string (InternetAddress *ia, gboolean encode)
{
	char *str = NULL;
	
	if (ia->type == INTERNET_ADDRESS_NAME) {
		if (ia->name) {
			char *name;
			
			name = encoded_name (ia->name, encode);
			str = g_strdup_printf ("%s <%s>", name, ia->value.addr);
			g_free (name);
		} else {
			str = g_strdup (ia->value.addr);
		}
	} else if (ia->type == INTERNET_ADDRESS_GROUP) {
		InternetAddressList *members;
		GString *string;
		
		string = g_string_new (ia->name);
		g_string_append (string, ": ");
		
		members = ia->value.members;
		internet_address_list_to_string_internal (members, encode, string);
		g_string_append (string, ";");
		
		str = string->str;
		g_string_free (string, FALSE);
	}
	
	return str;
}


/**
 * internet_address_list_to_string:
 * @list: list of internet addresses
 * @encode: TRUE if the address should be rfc2047 encoded
 *
 * Allocates a string buffer containing the rfc822 formatted addresses
 * in @list.
 *
 * Returns a string containing the list of addresses in rfc822 format.
 **/
char *
internet_address_list_to_string (InternetAddressList *list, gboolean encode)
{
	GString *string;
	char *str;
	
	string = g_string_new ("");
	internet_address_list_to_string_internal (list, encode, string);
	str = string->str;
	g_string_free (string, FALSE);
	
	return str;
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
			w(g_warning ("Malformed domain-literal, unexpected char '%c': %s",
				     *inptr, *in));
			
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
				if (domain->str[domain->len - 1] == '.')
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
		gboolean retried = FALSE;
		
		/* this mailbox has a name part, so get the name */
		name = g_string_new ("");
		while (pre) {
			retried = FALSE;
			g_string_append (name, pre);
			g_free (pre);
		retry:
			pre = decode_word (&inptr);
			if (pre)
				g_string_append_c (name, ' ');
		}
		
		decode_lwsp (&inptr);
		if (*inptr == '<') {
			inptr++;
			bracket = TRUE;
			pre = decode_word (&inptr);
		} else if (!retried && *inptr) {
			w(g_warning ("Unexpected char '%c' in address: %s: attempting recovery.",
				     *inptr, *in));
			/* chew up this bad char and then attempt 1 more pass at parsing */
			g_string_append_c (name, *inptr++);
			retried = TRUE;
			goto retry;
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
		w(g_warning ("No local part for email address: %s", *in));
		if (name)
			g_string_free (name, TRUE);
		g_string_free (addr, TRUE);
		*in = inptr;
		
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
		
		inptr++;
		domain = decode_domain (&inptr);
		if (domain) {
			g_string_append_c (addr, '@');
			g_string_append (addr, domain);
			g_free (domain);
		}
	} else {
		w(g_warning ("No domain in email address: %s", *in));
	}
	
	if (bracket) {
		decode_lwsp (&inptr);
		if (*inptr == '>')
			inptr++;
		else
			w(g_warning ("Missing closing '>' bracket for email address: %s", *in));
	}
	
	if (!name || !name->len) {
		/* look for a trailing comment to use as the name? */
		char *comment;
		
		if (name) {
			g_string_free (name, TRUE);
			name = NULL;
		}
		
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
	
	if (addr->len) {
		if (gmime_interfaces_utf8 && name && g_mime_utils_text_is_8bit (name->str, name->len)) {
			/* A (broken) mailer has sent us an unencoded 8bit value.
			 * Attempt to save it by assuming it's in the user's
			 * locale and converting to UTF-8 */
			char *buf;
			
			buf = g_mime_iconv_locale_to_utf8 (name->str);
			if (buf) {
				g_string_truncate (name, 0);
				g_string_append (name, buf);
				g_free (buf);
			} else {
				(g_warning ("Failed to convert \"%s\" to UTF-8: %s",
					    name->str, g_strerror (errno)));
			}
		}
		
		mailbox = internet_address_new_name (name ? name->str : NULL, addr->str);
	}
	
	g_string_free (addr, TRUE);
	if (name)
		g_string_free (name, TRUE);
	
	return mailbox;
}

static InternetAddress *
decode_address (const char **in)
{
	InternetAddress *addr = NULL;
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
		InternetAddressList *group = NULL, *tail;
		
		tail = (InternetAddressList *) &group;
		
		inptr++;
		addr = internet_address_new_group (name->str);
		
		decode_lwsp (&inptr);
		while (*inptr && *inptr != ';') {
			InternetAddress *member;
			
			member = decode_mailbox (&inptr);
			if (member) {
				tail->next = g_new (InternetAddressList, 1);
				tail = tail->next;
				tail->next = NULL;
				tail->address = member;
			}
			
			decode_lwsp (&inptr);
			while (*inptr == ',') {
				inptr++;
				decode_lwsp (&inptr);
				member = decode_mailbox (&inptr);
				if (member) {
					tail->next = g_new (InternetAddressList, 1);
					tail = tail->next;
					tail->next = NULL;
					tail->address = member;
				}
				
				decode_lwsp (&inptr);
			}
		}
		
		if (*inptr == ';')
			inptr++;
		else
			w(g_warning ("Invalid group spec, missing closing ';': %.*s",
				     inptr - start, start));
		
		internet_address_set_group (addr, group);
		
		*in = inptr;
	} else {
		/* this is a mailbox */
		addr = decode_mailbox (in);
	}
	
	g_string_free (name, TRUE);
	
	return addr;
}


/**
 * internet_address_parse_string:
 * @string: a string containing internet addresses
 *
 * Construct a list of internet addresses from the given string.
 *
 * Returns a linked list of internet addresses. *Must* be free'd by
 * the caller.
 **/
InternetAddressList *
internet_address_parse_string (const char *string)
{
	InternetAddressList *node, *tail, *addrlist = NULL;
	const char *inptr;
	
	inptr = string;
	
	tail = (InternetAddressList *) &addrlist;
	
	while (inptr && *inptr) {
		InternetAddress *addr;
		const char *start;
		
		start = inptr;
		
		addr = decode_address (&inptr);
		
		if (addr) {
			node = g_new (InternetAddressList, 1);
			node->next = NULL;
			node->address = addr;
			tail->next = node;
			tail = node;
		} else {
			w(g_warning ("Invalid or incomplete address: %.*s",
				     inptr - start, start));
		}
		
		decode_lwsp (&inptr);
		if (*inptr == ',')
			inptr++;
		else if (*inptr) {
			w(g_warning ("Parse error at '%s': expected ','", inptr));
			/* try skipping to the next address */
			inptr = strchr (inptr, ',');
			if (inptr)
				inptr++;
		}
	}
	
	return addrlist;
}
