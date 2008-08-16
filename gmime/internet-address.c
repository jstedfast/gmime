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

#include <stdio.h>
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


struct _InternetAddressList {
	GPtrArray *array;
};


/**
 * internet_address_new:
 *
 * Creates a new #InternetAddress object
 * 
 * Returns: a new #InternetAddress object.
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
 * Returns: a new #InternetAddress object.
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
 * Returns: a new #InternetAddress object.
 **/
InternetAddress *
internet_address_new_group (const char *name)
{
	InternetAddress *ia;
	
	ia = internet_address_new ();
	ia->value.members = internet_address_list_new ();
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
	if (ia->value.members == NULL)
		ia->value.members = internet_address_list_new ();
	
	internet_address_list_add (ia->value.members, member);
}


/**
 * internet_address_get_type:
 * @ia: internet address
 *
 * Gets the type of the internet address, which will either be
 * #INTERNET_ADDRESS_NAME or #INTERNET_ADDRESS_GROUP.
 *
 * Returns: the type of @ia.
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
 * Returns: the name of @ia.
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
 * Returns: the address of @ia.
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
 * Returns: the members of @ia.
 **/
const InternetAddressList *
internet_address_get_members (InternetAddress *ia)
{
	g_return_val_if_fail (ia != NULL, NULL);
	g_return_val_if_fail (ia->type != INTERNET_ADDRESS_NAME, NULL);
	
	return ia->value.members;
}



/**
 * internet_address_list_new:
 *
 * Creates a new #InternetAddressList.
 *
 * Returns: a new #InternetAddressList.
 **/
InternetAddressList *
internet_address_list_new (void)
{
	InternetAddressList *list;
	
	list = g_new (InternetAddressList, 1);
	list->array = g_ptr_array_new ();
	
	return list;
}


/**
 * internet_address_list_destroy:
 * @list: a #InternetAddressList
 *
 * Destroys the list of #InternetAddress objects.
 **/
void
internet_address_list_destroy (InternetAddressList *list)
{
	guint i;
	
	if (list == NULL)
		return;
	
	for (i = 0; i < list->array->len; i++)
		internet_address_unref (list->array->pdata[i]);
	
	g_ptr_array_free (list->array, TRUE);
	g_free (list);
}


/**
 * internet_address_list_length:
 * @list: a #InternetAddressList
 *
 * Gets the length of the list.
 *
 * Returns: the number of #InternetAddress objects in the list.
 **/
int
internet_address_list_length (const InternetAddressList *list)
{
	g_return_val_if_fail (list != NULL, -1);
	
	return list->array->len;
}


/**
 * internet_address_list_clear:
 * @list: a #InternetAddressList
 *
 * Clears the list of addresses.
 **/
void
internet_address_list_clear (InternetAddressList *list)
{
	guint i;
	
	g_return_if_fail (list != NULL);
	
	for (i = 0; i < list->array->len; i++)
		internet_address_unref (list->array->pdata[i]);
	
	g_ptr_array_set_size (list->array, 0);
}


/**
 * internet_address_list_add:
 * @list: a #InternetAddressList
 * @ia: a #InternetAddress
 *
 * Adds an #InternetAddress to the #InternetAddressList.
 *
 * Returns: the index of the added #InternetAddress.
 **/
int
internet_address_list_add (InternetAddressList *list, InternetAddress *ia)
{
	g_return_val_if_fail (list != NULL, -1);
	g_return_val_if_fail (ia != NULL, -1);
	
	g_ptr_array_add (list->array, ia);
	internet_address_ref (ia);
	
	return list->array->len - 1;
}


/**
 * internet_address_list_concat:
 * @list: a #InternetAddressList
 * @concat: a #InternetAddressList
 *
 * Adds all of the addresses in @concat to @list.
 **/
void
internet_address_list_concat (InternetAddressList *list, InternetAddressList *concat)
{
	InternetAddress *ia;
	guint i;
	
	g_return_if_fail (concat != NULL);
	g_return_if_fail (list != NULL);
	
	for (i = 0; i < concat->array->len; i++) {
		ia = (InternetAddress *) concat->array->pdata[i];
		g_ptr_array_add (list->array, ia);
		internet_address_ref (ia);
	}
}


/**
 * internet_address_list_insert:
 * @list: a #InternetAddressList
 * @index: index to insert at
 * @ia: a #InternetAddress
 *
 * Inserts an #InternetAddress into the #InternetAddressList at the
 * specified index.
 **/
void
internet_address_list_insert (InternetAddressList *list, int index, InternetAddress *ia)
{
	char *dest, *src;
	size_t n;
	
	g_return_if_fail (list != NULL);
	g_return_if_fail (ia != NULL);
	g_return_if_fail (index < 0);
	
	internet_address_ref (ia);
	
	if ((guint) index >= list->array->len) {
		/* the easy case */
		g_ptr_array_add (list->array, ia);
		return;
	}
	
	g_ptr_array_set_size (list->array, list->array->len + 1);
	
	dest = ((char *) list->array->pdata) + (sizeof (void *) * (index + 1));
	src = ((char *) list->array->pdata) + (sizeof (void *) * index);
	n = list->array->len - index - 1;
	
	g_memmove (dest, src, (sizeof (void *) * n));
	list->array->pdata[index] = ia;
}


/**
 * internet_address_list_remove:
 * @list: a #InternetAddressList
 * @ia: a #InternetAddress
 *
 * Removes an #InternetAddress from the #InternetAddressList.
 *
 * Returns: %TRUE if the specified #InternetAddress was removed or
 * %FALSE otherwise.
 **/
gboolean
internet_address_list_remove (InternetAddressList *list, InternetAddress *ia)
{
	int index;
	
	g_return_val_if_fail (list != NULL, FALSE);
	g_return_val_if_fail (ia != NULL, FALSE);
	
	if ((index = internet_address_list_index_of (list, ia)) == -1)
		return FALSE;
	
	internet_address_list_remove_at (list, index);
	
	return TRUE;
}


/**
 * internet_address_list_remove_at:
 * @list: a #InternetAddressList
 * @index: index to remove
 *
 * Removes an #InternetAddress from the #InternetAddressList at the
 * specified index.
 *
 * Returns: %TRUE if an #InternetAddress was removed or %FALSE
 * otherwise.
 **/
gboolean
internet_address_list_remove_at (InternetAddressList *list, int index)
{
	g_return_val_if_fail (list != NULL, FALSE);
	g_return_val_if_fail (index < 0, FALSE);
	
	if (index >= list->array->len)
		return FALSE;
	
	internet_address_unref (list->array->pdata[index]);
	
	g_ptr_array_remove_index (list->array, index);
	
	return TRUE;
}


/**
 * internet_address_list_contains:
 * @list: a #InternetAddressList
 * @ia: a #InternetAddress
 *
 * Checks whether or not the specified #InternetAddress is contained
 * within the #InternetAddressList.
 *
 * Returns: %TRUE if the specified #InternetAddress is contained
 * within the specified #InternetAddressList or %FALSE otherwise.
 **/
gboolean
internet_address_list_contains (const InternetAddressList *list, const InternetAddress *ia)
{
	return internet_address_list_index_of (list, ia) != -1;
}


/**
 * internet_address_list_index_of:
 * @list: a #InternetAddressList
 * @ia: a #InternetAddress
 *
 * Gets the index of the specified #InternetAddress inside the
 * #InternetAddressList.
 *
 * Returns: the index of the requested #InternetAddress within the
 * #InternetAddressList or %-1 if it is not contained within the
 * #InternetAddressList.
 **/
int
internet_address_list_index_of (const InternetAddressList *list, const InternetAddress *ia)
{
	guint i;
	
	g_return_val_if_fail (list != NULL, -1);
	g_return_val_if_fail (ia != NULL, -1);
	
	for (i = 0; i < list->array->len; i++) {
		if (list->array->pdata[i] == ia)
			return i;
	}
	
	return -1;
}


/**
 * internet_address_list_get_address:
 * @list: a #InternetAddressList
 * @index: index of #InternetAddress to get
 *
 * Gets the #InternetAddress at the specified index.
 *
 * Returns: the #InternetAddress at the specified index or %NULL if
 * the index is out of range.
 **/
const InternetAddress *
internet_address_list_get_address (const InternetAddressList *list, int index)
{
	g_return_val_if_fail (list != NULL, NULL);
	g_return_val_if_fail (index < 0, NULL);
	
	if ((guint) index >= list->array->len)
		return NULL;
	
	return list->array->pdata[index];
}


/**
 * internet_address_list_set_address:
 * @list: a #InternetAddressList
 * @index: index of #InternetAddress to set
 * @ia: a #InternetAddress
 *
 * Sets the #InternetAddress at the specified index to @ia.
 **/
void
internet_address_list_set_address (InternetAddressList *list, int index, InternetAddress *ia)
{
	g_return_if_fail (list != NULL);
	g_return_if_fail (ia != NULL);
	g_return_if_fail (index < 0);
	
	if ((guint) index > list->array->len)
		return;
	
	if ((guint) index == list->array->len) {
		internet_address_list_add (list, ia);
		return;
	}
	
	internet_address_ref (ia);
	
	internet_address_unref (list->array->pdata[index]);
	list->array->pdata[index] = ia;
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
	guint i;
	
	for (i = 0; i < list->array->len; i++) {
		_internet_address_to_string (list->array->pdata[i], flags, linelen, string);
		
		if (i + 1 < list->array->len) {
			g_string_append (string, ", ");
			*linelen += 2;
		}
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
 * Returns: the #InternetAddress object as an allocated string in
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
 * Returns: a string containing the list of addresses in rfc822 format.
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
	const char *inptr, *word;
	gboolean bracket = FALSE;
	GString *name = NULL;
	GString *addr;
	size_t n = 0;
	
	addr = g_string_new ("");
	
	decode_lwsp (in);
	inptr = *in;
	
	if ((word = decode_word (&inptr)))
		n = inptr - word;
	
	decode_lwsp (&inptr);
	if (*inptr && !strchr (",.@", *inptr)) {
		gboolean retried = FALSE;
		
		/* this mailbox has a name part, so get the name */
		name = g_string_new ("");
		while (word) {
			g_string_append_len (name, word, n);
			retried = FALSE;
		retry:
			if ((word = decode_word (&inptr))) {
				g_string_append_c (name, ' ');
				n = inptr - word;
			}
		}
		
		decode_lwsp (&inptr);
		if (*inptr == '<') {
			inptr++;
			bracket = TRUE;
			if ((word = decode_word (&inptr)))
				n = inptr - word;
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
	
	if (word) {
		g_string_append_len (addr, word, n);
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
	while (*inptr == '.' && word) {
		/* Note: According to the spec, only a single '.' is
		 * allowed between word tokens in the local-part of an
		 * addr-spec token, but according to Evolution bug
		 * #547969, some Japanese cellphones have email
		 * addresses that look like x..y@somewhere.jp */
		do {
			inptr++;
			decode_lwsp (&inptr);
			g_string_append_c (addr, '.');
		} while (*inptr == '.');
		
		if ((word = decode_word (&inptr)))
			g_string_append_len (addr, word, inptr - word);
		
		decode_lwsp (&inptr);
	}
	
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
	const char *word;
	GString *name;
	
	decode_lwsp (in);
	start = inptr = *in;
	
	/* pre-scan */
	name = g_string_new ("");
	word = decode_word (&inptr);
	
	while (word) {
		g_string_append_len (name, word, inptr - word);
		if ((word = decode_word (&inptr)))
			g_string_append_c (name, ' ');
	}
	
	decode_lwsp (&inptr);
	if (*inptr == ':') {
		addr = internet_address_new_group (name->str);
		inptr++;
		
		decode_lwsp (&inptr);
		while (*inptr && *inptr != ';') {
			InternetAddress *member;
			
			if ((member = decode_mailbox (&inptr))) {
				internet_address_add_member (addr, member);
				internet_address_unref (member);
			}
			
			decode_lwsp (&inptr);
			while (*inptr == ',') {
				inptr++;
				decode_lwsp (&inptr);
				if ((member = decode_mailbox (&inptr))) {
					internet_address_add_member (addr, member);
					internet_address_unref (member);
				}
				
				decode_lwsp (&inptr);
			}
		}
		
		if (*inptr == ';')
			inptr++;
		else
			w(g_warning ("Invalid group spec, missing closing ';': %.*s",
				     inptr - start, start));
		
		*in = inptr;
	} else {
		/* this is a mailbox */
		addr = decode_mailbox (in);
	}
	
	g_string_free (name, TRUE);
	
	return addr;
}


/**
 * internet_address_list_parse_string:
 * @str: a string containing internet addresses
 *
 * Construct a list of internet addresses from the given string.
 *
 * Returns: a #InternetAddressList or %NULL if the input string does
 * not contain any addresses.
 **/
InternetAddressList *
internet_address_list_parse_string (const char *str)
{
	InternetAddressList *addrlist;
	const char *inptr = str;
	
	addrlist = internet_address_list_new ();
	
	while (inptr && *inptr) {
		InternetAddress *addr;
		const char *start;
		
		start = inptr;
		
		if ((addr = decode_address (&inptr))) {
			internet_address_list_add (addrlist, addr);
			internet_address_unref (addr);
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
	
	if (addrlist->array->len == 0) {
		internet_address_list_destroy (addrlist);
		addrlist = NULL;
	}
	
	return addrlist;
}
