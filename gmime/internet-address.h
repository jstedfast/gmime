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


#ifndef __INTERNET_ADDRESS_H__
#define __INTERNET_ADDRESS_H__

#include <glib.h>

G_BEGIN_DECLS




/**
 * InternetAddressType:
 * @INTERNET_ADDRESS_NONE: No type.
 * @INTERNET_ADDRESS_MAILBOX: A typical internet address type.
 * @INTERNET_ADDRESS_GROUP: An rfc2822 group type address.
 *
 * The type of #InternetAddress.
 **/
typedef enum {
	INTERNET_ADDRESS_NONE,
	INTERNET_ADDRESS_MAILBOX,
	INTERNET_ADDRESS_GROUP
} InternetAddressType;

typedef struct _InternetAddressList InternetAddressList;
typedef struct _InternetAddress InternetAddress;


/**
 * InternetAddress:
 * @type: The type of internet address.
 * @refcount: The reference count.
 * @name: The name component of the internet address.
 *
 * A structure representing an rfc2822 address.
 **/
struct _InternetAddress {
	InternetAddressType type;
	unsigned int refcount;
	char *name;
	union {
		char *addr;
		InternetAddressList *members;
	} value;
};


InternetAddress *internet_address_new (void);
InternetAddress *internet_address_new_mailbox (const char *name, const char *addr);
InternetAddress *internet_address_new_group (const char *name);

void internet_address_ref (InternetAddress *ia);
void internet_address_unref (InternetAddress *ia);

void internet_address_set_name (InternetAddress *ia, const char *name);
void internet_address_set_addr (InternetAddress *ia, const char *addr);
void internet_address_set_group (InternetAddress *ia, InternetAddressList *group);
void internet_address_add_member (InternetAddress *ia, InternetAddress *member);

InternetAddressType internet_address_get_type (const InternetAddress *ia);
const char *internet_address_get_name (const InternetAddress *ia);
const char *internet_address_get_addr (const InternetAddress *ia);
InternetAddressList *internet_address_get_members (const InternetAddress *ia);

char *internet_address_to_string (const InternetAddress *ia, gboolean encode);


/**
 * InternetAddressList:
 * @refcount: The reference count.
 * @array: The array of #InternetAddress objects.
 *
 * A collection of #InternetAddress objects.
 **/
struct _InternetAddressList {
	unsigned int refcount;
	GPtrArray *array;
};


InternetAddressList *internet_address_list_new (void);

void internet_address_list_ref (InternetAddressList *list);
void internet_address_list_unref (InternetAddressList *list);

int internet_address_list_length (const InternetAddressList *list);

void internet_address_list_clear (InternetAddressList *list);

int internet_address_list_add (InternetAddressList *list, InternetAddress *ia);
void internet_address_list_concat (InternetAddressList *list, InternetAddressList *concat);
void internet_address_list_insert (InternetAddressList *list, int index, InternetAddress *ia);
gboolean internet_address_list_remove (InternetAddressList *list, InternetAddress *ia);
gboolean internet_address_list_remove_at (InternetAddressList *list, int index);

gboolean internet_address_list_contains (const InternetAddressList *list, const InternetAddress *ia);
int internet_address_list_index_of (const InternetAddressList *list, const InternetAddress *ia);

InternetAddress *internet_address_list_get_address (const InternetAddressList *list, int index);
void internet_address_list_set_address (InternetAddressList *list, int index, InternetAddress *ia);

InternetAddressList *internet_address_list_parse_string (const char *str);

char *internet_address_list_to_string (const InternetAddressList *list, gboolean encode);

void internet_address_list_writer (const InternetAddressList *list, GString *str);

G_END_DECLS

#endif /* __INTERNET_ADDRESS_H__ */
