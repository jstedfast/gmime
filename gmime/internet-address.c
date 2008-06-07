/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*  GMime
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

#include <string.h>
#include <ctype.h>
#include <errno.h>

#include "internet-address.h"
#include "gmime-table-private.h"
#include "gmime-parse-utils.h"
#include "gmime-iconv-utils.h"
#include "gmime-utils.h"


#ifdef ENABLE_WARNINGS
#define w(x) x
#else
#define w(x)
#endif /* ENABLE_WARNINGS */

#define d(x)


/**
 * SECTION: internet-address
 * @title: InternetAddress
 * @short_description: Internet addresses
 * @see_also:
 *
 * An #InternetAddress represents what is commonly referred to as an
 * E-Mail address.
 **/


/**
 * internet_address_new:
 *
 * Creates a new #InternetAddress object
 * 
 * Returns a new #InternetAddress object.
 **/
InternetAddress *
internet_address_new (void)
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
 * @ia: internet address
 * 
 * Destroy the #InternetAddress object pointed to by @ia.
 **/
static void
internet_address_destroy (InternetAddress *ia)
{
	g_free (ia->name);
	
	if (ia->type == INTERNET_ADDRESS_GROUP) {
		internet_address_list_destroy (ia->value.members);
	} else {
		g_free (ia->value.addr);
	}
	
	g_free (ia);
}


/**
 * internet_address_ref:
 * @ia: internet address
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
 * @ia: internet address
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
 * Creates a new #InternetAddress object with name @name and address
 * @addr.
 * 
 * Returns a new #InternetAddress object.
 **/
InternetAddress *
internet_address_new_name (const char *name, const char *addr)
{
	InternetAddress *ia;
	
	g_return_val_if_fail (addr != NULL, NULL);
	
	ia = internet_address_new ();
	ia->type = INTERNET_ADDRESS_NAME;
	if (name) {
		ia->name = g_mime_utils_header_decode_phrase (name);
		g_mime_utils_unquote_string (ia->name);
	}
	ia->value.addr = g_strdup (addr);
	
	return ia;
}


/**
 * internet_address_new_group:
 * @name: group name
 *
 * Creates a new #InternetAddress object with group name @name.
 * 
 * Returns a new #InternetAddress object.
 **/
InternetAddress *
internet_address_new_group (const char *name)
{
	InternetAddress *ia;
	
	ia = internet_address_new ();
	ia->type = INTERNET_ADDRESS_GROUP;
	if (name) {
		ia->name = g_mime_utils_header_decode_phrase (name);
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
		ia->name = g_mime_utils_header_decode_phrase (name);
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
 * internet_address_get_type:
 * @ia: internet address
 *
 * Gets the type of the internet address, which will either be
 * #INTERNET_ADDRESS_NAME or #INTERNET_ADDRESS_GROUP.
 *
 * Returns the type of @ia.
 **/
InternetAddressType
internet_address_get_type (InternetAddress *ia)
{
	g_return_val_if_fail (ia != NULL, INTERNET_ADDRESS_NONE);
	
	return ia->type;
}

/**
 * internet_address_get_name:
 * @ia: internet address
 *
 * Gets the name component of the internet address. If the internet
 * address is a group, it will get the group name.
 *
 * Returns the name of @ia.
 **/
const char *
internet_address_get_name (InternetAddress *ia)
{
	g_return_val_if_fail (ia != NULL, NULL);
	
	return ia->name;
}


/**
 * internet_address_get_addr:
 * @ia: internet address
 *
 * Gets the addr-spec of the internet address.
 *
 * Returns the address of @ia.
 **/
const char *
internet_address_get_addr (InternetAddress *ia)
{
	g_return_val_if_fail (ia != NULL, NULL);
	g_return_val_if_fail (ia->type != INTERNET_ADDRESS_GROUP, NULL);
	
	return ia->value.addr;
}


/**
 * internet_address_get_members:
 * @ia: internet address
 *
 * Gets the #InternetAddressList containing the group members of an
 * rfc822 group address.
 *
 * Returns the members of @ia.
 **/
const InternetAddressList *
internet_address_get_members (InternetAddress *ia)
{
	g_return_val_if_fail (ia != NULL, NULL);
	g_return_val_if_fail (ia->type != INTERNET_ADDRESS_NAME, NULL);
	
	return ia->value.members;
}


/**
 * internet_address_list_prepend:
 * @list: a list of internet addresses
 * @ia: internet address to prepend
 *
 * Prepends the internet address @ia to the list of internet addresses
 * pointed to by @list.
 *
 * Returns the resultant list.
 **/
InternetAddressList *
internet_address_list_prepend (InternetAddressList *list, InternetAddress *ia)
{
	InternetAddressList *node;
	
	g_return_val_if_fail (ia != NULL, NULL);
	
	internet_address_ref (ia);
	node = g_new (InternetAddressList, 1);
	node->address = ia;
	node->next = list;
	
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
	
	g_return_val_if_fail (ia != NULL, NULL);
	
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
 * internet_address_list_next:
 * @list: list of internet addresses
 *
 * Advances to the next address node in the #InternetAddessList.
 *
 * Returns the next address node in the #InternetAddessList.
 **/
InternetAddressList *
internet_address_list_next (const InternetAddressList *list)
{
	return list ? list->next : NULL;
}


/**
 * internet_address_list_get_address:
 * @list: list of internet addresses
 *
 * Gets the #InternetAddress currently pointed to in @list.
 *
 * Returns the #InternetAddress currently pointed to in @list.
 **/
InternetAddress *
internet_address_list_get_address (const InternetAddressList *list)
{
	return list ? list->address : NULL;
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
internet_address_list_length (const InternetAddressList *list)
{
	const InternetAddressList *node;
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
	
	if (rfc2047_encode) {
		name = g_mime_utils_header_encode_phrase (raw);
	} else {
		name = g_mime_utils_quote_string (raw);
	}
	
	return name;
}

enum {
	INTERNET_ADDRESS_ENCODE = 1 << 0,
	INTERNET_ADDRESS_FOLD   = 1 << 1,
};

static void _internet_address_list_to_string (const InternetAddressList *list, guint32 flags, size_t *linelen, GString *string);

static void
linewrap (GString *string)
{
	if (string->len > 0 && string->str[string->len - 1] == ' ') {
		string->str[string->len - 1] = '\n';
		g_string_append_c (string, '\t');
	} else {
		g_string_append (string, "\n\t");
	}
}

static void
append_folded_name (GString *string, size_t *linelen, const char *name)
{
	const char *word, *lwsp;
	size_t len;
	
	word = name;
	
	while (*word) {
		lwsp = word;
		
		if (*word == '"') {
			/* quoted string, don't break these up */
			lwsp++;
			
			while (*lwsp && *lwsp != '"') {
				if (*lwsp == '\\')
					lwsp++;
				
				if (*lwsp)
					lwsp++;
			}
			
			if (*lwsp == '"')
				lwsp++;
		} else {
			/* normal word */
			while (*lwsp && !is_lwsp (*lwsp))
				lwsp++;
		}
		
		len = lwsp - word;
		if (*linelen > 1 && (*linelen + len) > GMIME_FOLD_LEN) {
			linewrap (string);
			*linelen = 1;
		}
		
		g_string_append_len (string, word, len);
		*linelen += len;
		
		word = lwsp;
		while (*word && is_lwsp (*word))
			word++;
		
		if (*word && is_lwsp (*lwsp)) {
			g_string_append_c (string, ' ');
			(*linelen)++;
		}
	}
}

static void
_internet_address_to_string (const InternetAddress *ia, guint32 flags, size_t *linelen, GString *string)
{
	gboolean encode = flags & INTERNET_ADDRESS_ENCODE;
	gboolean fold = flags & INTERNET_ADDRESS_FOLD;
	char *name;
	size_t len;
	
	if (ia->type == INTERNET_ADDRESS_NAME) {
		if (ia->name && *ia->name) {
			name = encoded_name (ia->name, encode);
			len = strlen (name);
			
			if (fold && (*linelen + len) > GMIME_FOLD_LEN) {
				if (len > GMIME_FOLD_LEN) {
					/* we need to break up the name */
					append_folded_name (string, linelen, name);
				} else {
					/* the name itself is short enough to fit on a single
					 * line, but only if we write it on a line by itself */
					if (*linelen > 1) {
						linewrap (string);
						*linelen = 1;
					}
					
					g_string_append_len (string, name, len);
					*linelen += len;
				}
			} else {
				/* we can safly fit the name on this line */
				g_string_append_len (string, name, len);
				*linelen += len;
			}
			
			g_free (name);
			
			len = strlen (ia->value.addr);
			
			if (fold && (*linelen + len + 3) >= GMIME_FOLD_LEN) {
				g_string_append_len (string, "\n\t<", 3);
				*linelen = 2;
			} else {
				g_string_append_len (string, " <", 2);
				*linelen += 2;
			}
			
			g_string_append_len (string, ia->value.addr, len);
			g_string_append_c (string, '>');
			*linelen += len + 1;
		} else {
			len = strlen (ia->value.addr);
			
			if (fold && (*linelen + len) > GMIME_FOLD_LEN) {
				linewrap (string);
				*linelen = 1;
			}
			
			g_string_append_len (string, ia->value.addr, len);
			*linelen += len;
		}
	} else if (ia->type == INTERNET_ADDRESS_GROUP) {
		InternetAddressList *members;
		
		name = encoded_name (ia->name, encode);
		len = strlen (name);
		
		if (fold && *linelen > 1 && (*linelen + len + 1) > GMIME_FOLD_LEN) {
			linewrap (string);
			*linelen = 1;
		}
		
		g_string_append_len (string, name, len);
		g_string_append_len (string, ": ", 2);
		*linelen += len + 2;
		g_free (name);
		
		members = ia->value.members;
		_internet_address_list_to_string (members, flags, linelen, string);
		g_string_append_c (string, ';');
		*linelen += 1;
	}
}

static void
_internet_address_list_to_string (const InternetAddressList *list, guint32 flags, size_t *linelen, GString *string)
{
	while (list) {
		_internet_address_to_string (list->address, flags, linelen, string);
		
		if (list->next) {
			g_string_append (string, ", ");
			*linelen += 2;
		}
		
		list = list->next;
	}
}


/**
 * internet_address_to_string:
 * @ia: Internet Address object
 * @encode: TRUE if the address should be rfc2047 encoded
 *
 * Allocates a string containing the contents of the #InternetAddress
 * object.
 * 
 * Returns the #InternetAddress object as an allocated string in
 * rfc822 format.
 **/
char *
internet_address_to_string (const InternetAddress *ia, gboolean encode)
{
	guint32 flags = encode ? INTERNET_ADDRESS_ENCODE : 0;
	size_t linelen = 0;
	GString *string;
	char *str;
	
	string = g_string_new ("");
	_internet_address_to_string (ia, flags, &linelen, string);
	str = string->str;
	
	g_string_free (string, FALSE);
	
	return str;
}


/**
 * internet_address_list_to_string:
 * @list: list of internet addresses
 * @encode: %TRUE if the address should be rfc2047 encoded
 *
 * Allocates a string buffer containing the rfc822 formatted addresses
 * in @list.
 *
 * Returns a string containing the list of addresses in rfc822 format.
 **/
char *
internet_address_list_to_string (const InternetAddressList *list, gboolean encode)
{
	guint32 flags = encode ? INTERNET_ADDRESS_ENCODE : 0;
	size_t linelen = 0;
	GString *string;
	char *str;
	
	string = g_string_new ("");
	_internet_address_list_to_string (list, flags, &linelen, string);
	str = string->str;
	
	g_string_free (string, FALSE);
	
	return str;
}


/**
 * internet_address_list_writer:
 * @list: list of internet addresses
 * @str: string to write to
 *
 * Writes the rfc2047-encoded rfc822 formatted addresses in @list to
 * @string, folding appropriately.
 **/
void
internet_address_list_writer (const InternetAddressList *list, GString *str)
{
	guint32 flags = INTERNET_ADDRESS_ENCODE | INTERNET_ADDRESS_FOLD;
	size_t linelen = str->len;
	
	_internet_address_list_to_string (list, flags, &linelen, str);
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
			if ((pre = decode_word (&inptr)))
				g_string_append_c (name, ' ');
		}
		
		decode_lwsp (&inptr);
		if (*inptr == '<') {
			inptr++;
			bracket = TRUE;
			pre = decode_word (&inptr);
		} else if (!retried && *inptr) {
			w(g_warning ("Unexpected char '%c' in mailbox: %s: attempting recovery.",
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
		
		/* comma will be eaten by our caller */
		if (*inptr && *inptr != ',')
			*in = inptr + 1;
		else
			*in = inptr;
		
		return NULL;
	}
	
	/* get the rest of the local-part */
	decode_lwsp (&inptr);
	while (*inptr == '.' && pre) {
		inptr++;
		g_free (pre);
		if ((pre = decode_word (&inptr))) {
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
		if ((domain = decode_domain (&inptr))) {
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
			if ((comment = memchr (comment, '(', inptr - comment))) {
				const char *cend;
				
				/* find the end of the comment */
				cend = inptr - 1;
				while (cend > comment && is_lwsp (*cend))
					cend--;
				
				if (*cend == ')')
					cend--;
				
				comment = g_strndup (comment + 1, cend - comment);
				g_strstrip (comment);
				
				name = g_string_new (comment);
				g_free (comment);
			}
		}
	}
	
	*in = inptr;
	
	if (addr->len) {
		if (name && !g_utf8_validate (name->str, -1, NULL)) {
			/* A (broken) mailer has sent us raw 8bit/multibyte text data... */
			char *utf8 = g_mime_utils_decode_8bit (name->str, name->len);
			
			g_string_truncate (name, 0);
			g_string_append (name, utf8);
			g_free (utf8);
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
		
		if ((pre = decode_word (&inptr)))
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
			
			if ((member = decode_mailbox (&inptr))) {
				tail->next = g_new (InternetAddressList, 1);
				tail = tail->next;
				tail->next = NULL;
				tail->address = member;
			}
			
			decode_lwsp (&inptr);
			while (*inptr == ',') {
				inptr++;
				decode_lwsp (&inptr);
				if ((member = decode_mailbox (&inptr))) {
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
			if ((inptr = strchr (inptr, ',')))
				inptr++;
		}
	}
	
	return addrlist;
}
