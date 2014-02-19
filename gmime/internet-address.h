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


#ifndef __INTERNET_ADDRESS_H__
#define __INTERNET_ADDRESS_H__

#include <glib.h>
#include <glib-object.h>

G_BEGIN_DECLS

#define INTERNET_ADDRESS_TYPE                  (internet_address_get_type ())
#define INTERNET_ADDRESS(obj)                  (G_TYPE_CHECK_INSTANCE_CAST ((obj), internet_address_get_type (), InternetAddress))
#define INTERNET_ADDRESS_CLASS(klass)          (G_TYPE_CHECK_CLASS_CAST ((klass), internet_address_get_type (), InternetAddressClass))
#define IS_INTERNET_ADDRESS(obj)               (G_TYPE_CHECK_INSTANCE_TYPE ((obj), internet_address_get_type ()))
#define IS_INTERNET_ADDRESS_CLASS(klass)       (G_TYPE_CHECK_CLASS_TYPE ((klass), internet_address_get_type ()))
#define INTERNET_ADDRESS_GET_CLASS(obj)        (G_TYPE_INSTANCE_GET_CLASS ((obj), internet_address_get_type (), InternetAddressClass))

#define INTERNET_ADDRESS_TYPE_GROUP            (internet_address_group_get_type ())
#define INTERNET_ADDRESS_GROUP(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), INTERNET_ADDRESS_TYPE_GROUP, InternetAddressGroup))
#define INTERNET_ADDRESS_GROUP_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), INTERNET_ADDRESS_TYPE_GROUP, InternetAddressGroupClass))
#define INTERNET_ADDRESS_IS_GROUP(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), INTERNET_ADDRESS_TYPE_GROUP))
#define INTERNET_ADDRESS_IS_GROUP_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), INTERNET_ADDRESS_TYPE_GROUP))

#define INTERNET_ADDRESS_TYPE_MAILBOX          (internet_address_mailbox_get_type ())
#define INTERNET_ADDRESS_MAILBOX(obj)          (G_TYPE_CHECK_INSTANCE_CAST ((obj), INTERNET_ADDRESS_TYPE_MAILBOX, InternetAddressMailbox))
#define INTERNET_ADDRESS_MAILBOX_CLASS(klass)  (G_TYPE_CHECK_CLASS_CAST ((klass), INTERNET_ADDRESS_TYPE_MAILBOX, InternetAddressMailboxClass))
#define INTERNET_ADDRESS_IS_MAILBOX(obj)       (G_TYPE_CHECK_INSTANCE_TYPE ((obj), INTERNET_ADDRESS_TYPE_MAILBOX))
#define INTERNET_ADDRESS_IS_MAILBOX_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), INTERNET_ADDRESS_TYPE_MAILBOX))

#define INTERNET_ADDRESS_LIST_TYPE             (internet_address_list_get_type ())
#define INTERNET_ADDRESS_LIST(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), INTERNET_ADDRESS_LIST_TYPE, InternetAddressList))
#define INTERNET_ADDRESS_LIST_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), INTERNET_ADDRESS_LIST_TYPE, InternetAddressListClass))
#define IS_INTERNET_ADDRESS_LIST(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), INTERNET_ADDRESS_LIST_TYPE))
#define IS_INTERNET_ADDRESS_LIST_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), INTERNET_ADDRESS_LIST_TYPE))


typedef struct _InternetAddress InternetAddress;
typedef struct _InternetAddressClass InternetAddressClass;

typedef struct _InternetAddressGroup InternetAddressGroup;
typedef struct _InternetAddressGroupClass InternetAddressGroupClass;

typedef struct _InternetAddressMailbox InternetAddressMailbox;
typedef struct _InternetAddressMailboxClass InternetAddressMailboxClass;

typedef struct _InternetAddressList InternetAddressList;
typedef struct _InternetAddressListClass InternetAddressListClass;


/**
 * InternetAddress:
 * @parent_object: parent #GObject
 * @priv: private data
 * @name: display name
 *
 * An RFC 2822 Address object.
 **/
struct _InternetAddress {
	GObject parent_object;
	gpointer priv;
	
	char *name;
};

struct _InternetAddressClass {
	GObjectClass parent_class;
	
	/* public virtual methods */
	void (* to_string) (InternetAddress *ia, guint32 flags, size_t *linelen, GString *out);
};


GType internet_address_get_type (void);

void internet_address_set_name (InternetAddress *ia, const char *name);
const char *internet_address_get_name (InternetAddress *ia);

char *internet_address_to_string (InternetAddress *ia, gboolean encode);


/**
 * InternetAddressMailbox:
 * @parent_object: parent #InternetAddress
 * @addr: address string
 *
 * An RFC 2822 Mailbox address.
 **/
struct _InternetAddressMailbox {
	InternetAddress parent_object;
	
	char *addr;
};

struct _InternetAddressMailboxClass {
	InternetAddressClass parent_class;
	
};


GType internet_address_mailbox_get_type (void);

InternetAddress *internet_address_mailbox_new (const char *name, const char *addr);

void internet_address_mailbox_set_addr (InternetAddressMailbox *mailbox, const char *addr);
const char *internet_address_mailbox_get_addr (InternetAddressMailbox *mailbox);


/**
 * InternetAddressGroup:
 * @parent_object: parent #InternetAddress
 * @members: a #InternetAddressList of group members
 *
 * An RFC 2822 Group address.
 **/
struct _InternetAddressGroup {
	InternetAddress parent_object;
	
	InternetAddressList *members;
};

struct _InternetAddressGroupClass {
	InternetAddressClass parent_class;
	
};


GType internet_address_group_get_type (void);

InternetAddress *internet_address_group_new (const char *name);

void internet_address_group_set_members (InternetAddressGroup *group, InternetAddressList *members);
InternetAddressList *internet_address_group_get_members (InternetAddressGroup *group);

int internet_address_group_add_member (InternetAddressGroup *group, InternetAddress *member);


/**
 * InternetAddressList:
 * @parent_object: parent #GObject
 * @priv: private data
 * @array: The array of #InternetAddress objects.
 *
 * A collection of #InternetAddress objects.
 **/
struct _InternetAddressList {
	GObject parent_object;
	gpointer priv;
	
	GPtrArray *array;
};

struct _InternetAddressListClass {
	GObjectClass parent_class;
	
};


GType internet_address_list_get_type (void);

InternetAddressList *internet_address_list_new (void);

int internet_address_list_length (InternetAddressList *list);

void internet_address_list_clear (InternetAddressList *list);

int internet_address_list_add (InternetAddressList *list, InternetAddress *ia);
void internet_address_list_prepend (InternetAddressList *list, InternetAddressList *prepend);
void internet_address_list_append (InternetAddressList *list, InternetAddressList *append);
void internet_address_list_insert (InternetAddressList *list, int index, InternetAddress *ia);
gboolean internet_address_list_remove (InternetAddressList *list, InternetAddress *ia);
gboolean internet_address_list_remove_at (InternetAddressList *list, int index);

gboolean internet_address_list_contains (InternetAddressList *list, InternetAddress *ia);
int internet_address_list_index_of (InternetAddressList *list, InternetAddress *ia);

InternetAddress *internet_address_list_get_address (InternetAddressList *list, int index);
void internet_address_list_set_address (InternetAddressList *list, int index, InternetAddress *ia);

char *internet_address_list_to_string (InternetAddressList *list, gboolean encode);

InternetAddressList *internet_address_list_parse_string (const char *str);

void internet_address_list_writer (InternetAddressList *list, GString *str);

G_END_DECLS

#endif /* __INTERNET_ADDRESS_H__ */
