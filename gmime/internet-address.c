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

#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>

#include "internet-address.h"
#include "gmime-table-private.h"
#include "gmime-parse-utils.h"
#include "gmime-iconv-utils.h"
#include "gmime-events.h"
#include "gmime-utils.h"
#include "list.h"


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
 * @see_also: #InternetAddressGroup, #InternetAddressMailbox
 *
 * An #InternetAddress is the base class for #InternetAddressGroup and
 * #InternetAddressMailbox.
 **/

/**
 * SECTION: internet-address-group
 * @title: InternetAddressGroup
 * @short_description: rfc822 'group' address
 * @see_also: #InternetAddress
 *
 * An #InternetAddressGroup represents an rfc822 'group' address.
 **/

/**
 * SECTION: internet-address-mailbox
 * @title: InternetAddressMailbox
 * @short_description: rfc822 'mailbox' address
 * @see_also: #InternetAddress
 *
 * An #InternetAddressMailbox represents what is a typical "email
 * address".
 **/

/**
 * SECTION: internet-address-list
 * @title: InternetAddressList
 * @short_description: A list of internet addresses
 * @see_also: #InternetAddress
 *
 * An #InternetAddressList is a collection of #InternetAddress
 * objects.
 **/


enum {
	INTERNET_ADDRESS_ENCODE = 1 << 0,
	INTERNET_ADDRESS_FOLD   = 1 << 1,
};


static void internet_address_class_init (InternetAddressClass *klass);
static void internet_address_init (InternetAddress *ia, InternetAddressClass *klass);
static void internet_address_finalize (GObject *object);


static GObjectClass *parent_class = NULL;


GType
internet_address_get_type (void)
{
	static GType type = 0;
	
	if (!type) {
		static const GTypeInfo info = {
			sizeof (InternetAddressClass),
			NULL, /* base_class_init */
			NULL, /* base_class_finalize */
			(GClassInitFunc) internet_address_class_init,
			NULL, /* class_finalize */
			NULL, /* class_data */
			sizeof (InternetAddress),
			0,    /* n_preallocs */
			(GInstanceInitFunc) internet_address_init,
		};
		
		type = g_type_register_static (G_TYPE_OBJECT, "InternetAddress",
					       &info, G_TYPE_FLAG_ABSTRACT);
	}
	
	return type;
}


static void
internet_address_class_init (InternetAddressClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	
	parent_class = g_type_class_ref (G_TYPE_OBJECT);
	
	object_class->finalize = internet_address_finalize;
	
	klass->to_string = NULL;
}

static void
internet_address_init (InternetAddress *ia, InternetAddressClass *klass)
{
	ia->priv = g_mime_event_new ((GObject *) ia);
	ia->name = NULL;
}

static void
internet_address_finalize (GObject *object)
{
	InternetAddress *ia = (InternetAddress *) object;
	
	g_mime_event_destroy (ia->priv);
	g_free (ia->name);
	
	G_OBJECT_CLASS (parent_class)->finalize (object);
}


static void
_internet_address_set_name (InternetAddress *ia, const char *name)
{
	char *buf;
	
	buf = g_strdup (name);
	g_free (ia->name);
	ia->name = buf;
}

/**
 * internet_address_set_name:
 * @ia: a #InternetAddress
 * @name: the display name for the address group or mailbox
 *
 * Set the display name of the #InternetAddress.
 *
 * Note: The @name string should be in UTF-8.
 **/
void
internet_address_set_name (InternetAddress *ia, const char *name)
{
	g_return_if_fail (IS_INTERNET_ADDRESS (ia));
	
	_internet_address_set_name (ia, name);
	
	g_mime_event_emit (ia->priv, NULL);
}


/**
 * internet_address_get_name:
 * @ia: a #InternetAddress
 *
 * Gets the display name of the #InternetAddress.
 *
 * Returns: the name of the mailbox or group in a form suitable for
 * display if available or %NULL otherwise. If the name is available,
 * the returned string will be in UTF-8.
 **/
const char *
internet_address_get_name (InternetAddress *ia)
{
	g_return_val_if_fail (IS_INTERNET_ADDRESS (ia), NULL);
	
	return ia->name;
}


/**
 * internet_address_to_string:
 * @ia: Internet Address object
 * @encode: %TRUE if the address should be rfc2047 encoded
 *
 * Allocates a string containing the contents of the #InternetAddress
 * object.
 * 
 * Returns: the #InternetAddress object as an allocated string in
 * rfc822 format.
 **/
char *
internet_address_to_string (InternetAddress *ia, gboolean encode)
{
	guint32 flags = encode ? INTERNET_ADDRESS_ENCODE : 0;
	size_t linelen = 0;
	GString *string;
	char *str;
	
	string = g_string_new ("");
	INTERNET_ADDRESS_GET_CLASS (ia)->to_string (ia, flags, &linelen, string);
	str = string->str;
	
	g_string_free (string, FALSE);
	
	return str;
}


static void internet_address_mailbox_class_init (InternetAddressMailboxClass *klass);
static void internet_address_mailbox_init (InternetAddressMailbox *mailbox, InternetAddressMailboxClass *klass);
static void internet_address_mailbox_finalize (GObject *object);

static void mailbox_to_string (InternetAddress *ia, guint32 flags, size_t *linelen, GString *out);


static GObjectClass *mailbox_parent_class = NULL;


GType
internet_address_mailbox_get_type (void)
{
	static GType type = 0;
	
	if (!type) {
		static const GTypeInfo info = {
			sizeof (InternetAddressMailboxClass),
			NULL, /* base_class_init */
			NULL, /* base_class_finalize */
			(GClassInitFunc) internet_address_mailbox_class_init,
			NULL, /* class_finalize */
			NULL, /* class_data */
			sizeof (InternetAddressMailbox),
			0,    /* n_preallocs */
			(GInstanceInitFunc) internet_address_mailbox_init,
		};
		
		type = g_type_register_static (INTERNET_ADDRESS_TYPE, "InternetAddressMailbox", &info, 0);
	}
	
	return type;
}


static void
internet_address_mailbox_class_init (InternetAddressMailboxClass *klass)
{
	InternetAddressClass *address_class = INTERNET_ADDRESS_CLASS (klass);
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	
	mailbox_parent_class = g_type_class_ref (INTERNET_ADDRESS_TYPE);
	
	object_class->finalize = internet_address_mailbox_finalize;
	
	address_class->to_string = mailbox_to_string;
}

static void
internet_address_mailbox_init (InternetAddressMailbox *mailbox, InternetAddressMailboxClass *klass)
{
	mailbox->addr = NULL;
}

static void
internet_address_mailbox_finalize (GObject *object)
{
	InternetAddressMailbox *mailbox = (InternetAddressMailbox *) object;
	
	g_free (mailbox->addr);
	
	G_OBJECT_CLASS (mailbox_parent_class)->finalize (object);
}


/**
 * internet_address_mailbox_new:
 * @name: person's name
 * @addr: person's address
 *
 * Creates a new #InternetAddress object with the specified @name and
 * @addr.
 * 
 * Returns: a new #InternetAddressMailbox object.
 *
 * Note: The @name string should be in UTF-8.
 **/
InternetAddress *
internet_address_mailbox_new (const char *name, const char *addr)
{
	InternetAddressMailbox *mailbox;
	
	g_return_val_if_fail (addr != NULL, NULL);
	
	mailbox = g_object_newv (INTERNET_ADDRESS_TYPE_MAILBOX, 0, NULL);
	mailbox->addr = g_strdup (addr);
	
	_internet_address_set_name ((InternetAddress *) mailbox, name);
	
	return (InternetAddress *) mailbox;
}


/**
 * internet_address_mailbox_set_addr:
 * @mailbox: a #InternetAddressMailbox
 * @addr: contact's email address
 *
 * Set the mailbox address.
 **/
void
internet_address_mailbox_set_addr (InternetAddressMailbox *mailbox, const char *addr)
{
	g_return_if_fail (INTERNET_ADDRESS_IS_MAILBOX (mailbox));
	
	if (mailbox->addr == addr)
		return;
	
	g_free (mailbox->addr);
	mailbox->addr = g_strdup (addr);
	
	g_mime_event_emit (((InternetAddress *) mailbox)->priv, NULL);
}


/**
 * internet_address_mailbox_get_addr:
 * @mailbox: a #InternetAddressMailbox
 *
 * Gets the addr-spec of the internet address mailbox.
 *
 * Returns: the address of the mailbox.
 **/
const char *
internet_address_mailbox_get_addr (InternetAddressMailbox *mailbox)
{
	g_return_val_if_fail (INTERNET_ADDRESS_IS_MAILBOX (mailbox), NULL);
	
	return mailbox->addr;
}


static void internet_address_group_class_init (InternetAddressGroupClass *klass);
static void internet_address_group_init (InternetAddressGroup *group, InternetAddressGroupClass *klass);
static void internet_address_group_finalize (GObject *object);

static void group_to_string (InternetAddress *ia, guint32 flags, size_t *linelen, GString *out);


static GObjectClass *group_parent_class = NULL;


GType
internet_address_group_get_type (void)
{
	static GType type = 0;
	
	if (!type) {
		static const GTypeInfo info = {
			sizeof (InternetAddressGroupClass),
			NULL, /* base_class_init */
			NULL, /* base_class_finalize */
			(GClassInitFunc) internet_address_group_class_init,
			NULL, /* class_finalize */
			NULL, /* class_data */
			sizeof (InternetAddressGroup),
			0,    /* n_preallocs */
			(GInstanceInitFunc) internet_address_group_init,
		};
		
		type = g_type_register_static (INTERNET_ADDRESS_TYPE, "InternetAddressGroup", &info, 0);
	}
	
	return type;
}


static void
internet_address_group_class_init (InternetAddressGroupClass *klass)
{
	InternetAddressClass *address_class = INTERNET_ADDRESS_CLASS (klass);
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	
	group_parent_class = g_type_class_ref (INTERNET_ADDRESS_TYPE);
	
	object_class->finalize = internet_address_group_finalize;
	
	address_class->to_string = group_to_string;
}

static void
members_changed (InternetAddressList *members, gpointer args, InternetAddress *group)
{
	g_mime_event_emit (((InternetAddress *) group)->priv, NULL);
}

static void
internet_address_group_init (InternetAddressGroup *group, InternetAddressGroupClass *klass)
{
	group->members = internet_address_list_new ();
	
	g_mime_event_add (group->members->priv, (GMimeEventCallback) members_changed, group);
}

static void
internet_address_group_finalize (GObject *object)
{
	InternetAddressGroup *group = (InternetAddressGroup *) object;
	
	g_mime_event_remove (group->members->priv, (GMimeEventCallback) members_changed, group);
	
	g_object_unref (group->members);
	
	G_OBJECT_CLASS (group_parent_class)->finalize (object);
}


/**
 * internet_address_group_new:
 * @name: group name
 *
 * Creates a new #InternetAddressGroup object with the specified
 * @name.
 * 
 * Returns: a new #InternetAddressGroup object.
 *
 * Note: The @name string should be in UTF-8.
 **/
InternetAddress *
internet_address_group_new (const char *name)
{
	InternetAddress *group;
	
	group = g_object_newv (INTERNET_ADDRESS_TYPE_GROUP, 0, NULL);
	_internet_address_set_name (group, name);
	
	return group;
}


/**
 * internet_address_group_set_members:
 * @group: a #InternetAddressGroup
 * @members: a #InternetAddressList
 *
 * Set the members of the internet address group.
 **/
void
internet_address_group_set_members (InternetAddressGroup *group, InternetAddressList *members)
{
	g_return_if_fail (INTERNET_ADDRESS_IS_GROUP (group));
	g_return_if_fail (IS_INTERNET_ADDRESS_LIST (members));
	
	if (group->members == members)
		return;
	
	if (group->members) {
		g_mime_event_remove (group->members->priv, (GMimeEventCallback) members_changed, group);
		g_object_unref (group->members);
	}
	
	if (members) {
		g_mime_event_add (members->priv, (GMimeEventCallback) members_changed, group);
		g_object_ref (members);
	}
	
	group->members = members;
	
	g_mime_event_emit (((InternetAddress *) group)->priv, NULL);
}


/**
 * internet_address_group_get_members:
 * @group: a #InternetAddressGroup
 *
 * Gets the #InternetAddressList containing the group members of an
 * rfc822 group address.
 *
 * Returns: (transfer none): a #InternetAddressList containing the
 * members of @group.
 **/
InternetAddressList *
internet_address_group_get_members (InternetAddressGroup *group)
{
	g_return_val_if_fail (INTERNET_ADDRESS_IS_GROUP (group), NULL);
	
	return group->members;
}


#define _internet_address_group_add_member(group,member) _internet_address_list_add (group->members, member)

/**
 * internet_address_group_add_member:
 * @group: a #InternetAddressGroup
 * @member: a #InternetAddress
 *
 * Add a contact to the internet address group.
 *
 * Returns: the index of the newly added member.
 **/
int
internet_address_group_add_member (InternetAddressGroup *group, InternetAddress *member)
{
	g_return_val_if_fail (INTERNET_ADDRESS_IS_GROUP (group), -1);
	g_return_val_if_fail (IS_INTERNET_ADDRESS (member), -1);
	
	return internet_address_list_add (group->members, member);
}


static void internet_address_list_class_init (InternetAddressListClass *klass);
static void internet_address_list_init (InternetAddressList *list, InternetAddressListClass *klass);
static void internet_address_list_finalize (GObject *object);


static GObjectClass *list_parent_class = NULL;


GType
internet_address_list_get_type (void)
{
	static GType type = 0;
	
	if (!type) {
		static const GTypeInfo info = {
			sizeof (InternetAddressListClass),
			NULL, /* base_class_init */
			NULL, /* base_class_finalize */
			(GClassInitFunc) internet_address_list_class_init,
			NULL, /* class_finalize */
			NULL, /* class_data */
			sizeof (InternetAddressList),
			0,    /* n_preallocs */
			(GInstanceInitFunc) internet_address_list_init,
		};
		
		type = g_type_register_static (G_TYPE_OBJECT, "InternetAddressList", &info, 0);
	}
	
	return type;
}


static void
internet_address_list_class_init (InternetAddressListClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	
	list_parent_class = g_type_class_ref (G_TYPE_OBJECT);
	
	object_class->finalize = internet_address_list_finalize;
}

static void
internet_address_list_init (InternetAddressList *list, InternetAddressListClass *klass)
{
	list->priv = g_mime_event_new ((GObject *) list);
	list->array = g_ptr_array_new ();
}

static void
address_changed (InternetAddress *ia, gpointer args, InternetAddressList *list)
{
	g_mime_event_emit (list->priv, NULL);
}

static void
internet_address_list_finalize (GObject *object)
{
	InternetAddressList *list = (InternetAddressList *) object;
	InternetAddress *ia;
	guint i;
	
	for (i = 0; i < list->array->len; i++) {
		ia = (InternetAddress *) list->array->pdata[i];
		g_mime_event_remove (ia->priv, (GMimeEventCallback) address_changed, list);
		g_object_unref (ia);
	}
	
	g_mime_event_destroy (list->priv);
	
	g_ptr_array_free (list->array, TRUE);
	
	G_OBJECT_CLASS (list_parent_class)->finalize (object);
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
	return g_object_newv (INTERNET_ADDRESS_LIST_TYPE, 0, NULL);
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
internet_address_list_length (InternetAddressList *list)
{
	g_return_val_if_fail (IS_INTERNET_ADDRESS_LIST (list), -1);
	
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
	InternetAddress *ia;
	guint i;
	
	g_return_if_fail (IS_INTERNET_ADDRESS_LIST (list));
	
	for (i = 0; i < list->array->len; i++) {
		ia = (InternetAddress *) list->array->pdata[i];
		g_mime_event_remove (ia->priv, (GMimeEventCallback) address_changed, list);
		g_object_unref (ia);
	}
	
	g_ptr_array_set_size (list->array, 0);
	
	g_mime_event_emit (list->priv, NULL);
}


static int
_internet_address_list_add (InternetAddressList *list, InternetAddress *ia)
{
	int index;
	
	g_mime_event_add (ia->priv, (GMimeEventCallback) address_changed, list);
	
	index = list->array->len;
	g_ptr_array_add (list->array, ia);
	
	return index;
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
	int index;
	
	g_return_val_if_fail (IS_INTERNET_ADDRESS_LIST (list), -1);
	g_return_val_if_fail (IS_INTERNET_ADDRESS (ia), -1);
	
	index = _internet_address_list_add (list, ia);
	g_object_ref (ia);
	
	g_mime_event_emit (list->priv, NULL);
	
	return index;
}


/**
 * internet_address_list_prepend:
 * @list: a #InternetAddressList
 * @prepend: a #InternetAddressList
 *
 * Inserts all of the addresses in @prepend to the beginning of @list.
 **/
void
internet_address_list_prepend (InternetAddressList *list, InternetAddressList *prepend)
{
	InternetAddress *ia;
	char *dest, *src;
	guint len, i;
	
	g_return_if_fail (IS_INTERNET_ADDRESS_LIST (prepend));
	g_return_if_fail (IS_INTERNET_ADDRESS_LIST (list));
	
	if (prepend->array->len == 0)
		return;
	
	len = prepend->array->len;
	g_ptr_array_set_size (list->array, list->array->len + len);
	
	src = ((char *) list->array->pdata);
	dest = src + (sizeof (void *) * len);
	
	g_memmove (dest, src, (sizeof (void *) * list->array->len));
	
	for (i = 0; i < prepend->array->len; i++) {
		ia = (InternetAddress *) prepend->array->pdata[i];
		g_mime_event_add (ia->priv, (GMimeEventCallback) address_changed, list);
		list->array->pdata[i] = ia;
		g_object_ref (ia);
	}
	
	g_mime_event_emit (list->priv, NULL);
}


/**
 * internet_address_list_append:
 * @list: a #InternetAddressList
 * @append: a #InternetAddressList
 *
 * Adds all of the addresses in @append to @list.
 **/
void
internet_address_list_append (InternetAddressList *list, InternetAddressList *append)
{
	InternetAddress *ia;
	guint len, i;
	
	g_return_if_fail (IS_INTERNET_ADDRESS_LIST (append));
	g_return_if_fail (IS_INTERNET_ADDRESS_LIST (list));
	
	len = list->array->len;
	g_ptr_array_set_size (list->array, len + append->array->len);
	
	for (i = 0; i < append->array->len; i++) {
		ia = (InternetAddress *) append->array->pdata[i];
		g_mime_event_add (ia->priv, (GMimeEventCallback) address_changed, list);
		list->array->pdata[len + i] = ia;
		g_object_ref (ia);
	}
	
	g_mime_event_emit (list->priv, NULL);
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
	
	g_return_if_fail (IS_INTERNET_ADDRESS_LIST (list));
	g_return_if_fail (IS_INTERNET_ADDRESS (ia));
	g_return_if_fail (index >= 0);
	
	g_mime_event_add (ia->priv, (GMimeEventCallback) address_changed, list);
	g_object_ref (ia);
	
	if ((guint) index < list->array->len) {
		g_ptr_array_set_size (list->array, list->array->len + 1);
		
		dest = ((char *) list->array->pdata) + (sizeof (void *) * (index + 1));
		src = ((char *) list->array->pdata) + (sizeof (void *) * index);
		n = list->array->len - index - 1;
		
		g_memmove (dest, src, (sizeof (void *) * n));
		list->array->pdata[index] = ia;
	} else {
		/* the easy case */
		g_ptr_array_add (list->array, ia);
	}
	
	g_mime_event_emit (list->priv, NULL);
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
	
	g_return_val_if_fail (IS_INTERNET_ADDRESS_LIST (list), FALSE);
	g_return_val_if_fail (IS_INTERNET_ADDRESS (ia), FALSE);
	
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
	InternetAddress *ia;
	
	g_return_val_if_fail (IS_INTERNET_ADDRESS_LIST (list), FALSE);
	g_return_val_if_fail (index >= 0, FALSE);
	
	if ((guint) index >= list->array->len)
		return FALSE;
	
	ia = list->array->pdata[index];
	g_mime_event_remove (ia->priv, (GMimeEventCallback) address_changed, list);
	g_object_unref (ia);
	
	g_ptr_array_remove_index (list->array, index);
	
	g_mime_event_emit (list->priv, NULL);
	
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
internet_address_list_contains (InternetAddressList *list, InternetAddress *ia)
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
internet_address_list_index_of (InternetAddressList *list, InternetAddress *ia)
{
	guint i;
	
	g_return_val_if_fail (IS_INTERNET_ADDRESS_LIST (list), -1);
	g_return_val_if_fail (IS_INTERNET_ADDRESS (ia), -1);
	
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
 * Returns: (transfer none): the #InternetAddress at the specified
 * index or %NULL if the index is out of range.
 **/
InternetAddress *
internet_address_list_get_address (InternetAddressList *list, int index)
{
	g_return_val_if_fail (IS_INTERNET_ADDRESS_LIST (list), NULL);
	g_return_val_if_fail (index >= 0, NULL);
	
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
	InternetAddress *old;
	
	g_return_if_fail (IS_INTERNET_ADDRESS_LIST (list));
	g_return_if_fail (IS_INTERNET_ADDRESS (ia));
	g_return_if_fail (index >= 0);
	
	if ((guint) index > list->array->len)
		return;
	
	if ((guint) index == list->array->len) {
		internet_address_list_add (list, ia);
		return;
	}
	
	if ((old = list->array->pdata[index]) == ia)
		return;
	
	g_mime_event_remove (old->priv, (GMimeEventCallback) address_changed, list);
	g_object_unref (old);
	
	g_mime_event_add (ia->priv, (GMimeEventCallback) address_changed, list);
	list->array->pdata[index] = ia;
	g_object_ref (ia);
	
	g_mime_event_emit (list->priv, NULL);
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
mailbox_to_string (InternetAddress *ia, guint32 flags, size_t *linelen, GString *string)
{
	InternetAddressMailbox *mailbox = (InternetAddressMailbox *) ia;
	gboolean encode = flags & INTERNET_ADDRESS_ENCODE;
	gboolean fold = flags & INTERNET_ADDRESS_FOLD;
	char *name;
	size_t len;
	
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
			/* we can safely fit the name on this line */
			g_string_append_len (string, name, len);
			*linelen += len;
		}
		
		g_free (name);
		
		len = strlen (mailbox->addr);
		
		if (fold && (*linelen + len + 3) >= GMIME_FOLD_LEN) {
			g_string_append_len (string, "\n\t<", 3);
			*linelen = 2;
		} else {
			g_string_append_len (string, " <", 2);
			*linelen += 2;
		}
		
		g_string_append_len (string, mailbox->addr, len);
		g_string_append_c (string, '>');
		*linelen += len + 1;
	} else {
		len = strlen (mailbox->addr);
		
		if (fold && (*linelen + len) > GMIME_FOLD_LEN) {
			linewrap (string);
			*linelen = 1;
		}
		
		g_string_append_len (string, mailbox->addr, len);
		*linelen += len;
	}
}

static void
_internet_address_list_to_string (const InternetAddressList *list, guint32 flags, size_t *linelen, GString *string)
{
	InternetAddress *ia;
	guint i;
	
	for (i = 0; i < list->array->len; i++) {
		ia = (InternetAddress *) list->array->pdata[i];
		
		INTERNET_ADDRESS_GET_CLASS (ia)->to_string (ia, flags, linelen, string);
		
		if (i + 1 < list->array->len) {
			g_string_append (string, ", ");
			*linelen += 2;
		}
	}
}

static void
group_to_string (InternetAddress *ia, guint32 flags, size_t *linelen, GString *string)
{
	InternetAddressGroup *group = (InternetAddressGroup *) ia;
	gboolean encode = flags & INTERNET_ADDRESS_ENCODE;
	gboolean fold = flags & INTERNET_ADDRESS_FOLD;
	char *name = NULL;
	size_t len = 0;
	
	if (ia->name != NULL) {
		name = encoded_name (ia->name, encode);
		len = strlen (name);
		
		if (fold && *linelen > 1 && (*linelen + len + 1) > GMIME_FOLD_LEN) {
			linewrap (string);
			*linelen = 1;
		}
		
		g_string_append_len (string, name, len);
	}
	
	g_string_append_len (string, ": ", 2);
	*linelen += len + 2;
	g_free (name);
	
	_internet_address_list_to_string (group->members, flags, linelen, string);
	g_string_append_c (string, ';');
	*linelen += 1;
}


/**
 * internet_address_list_to_string:
 * @list: list of internet addresses
 * @encode: %TRUE if the address should be rfc2047 encoded
 *
 * Allocates a string buffer containing the rfc822 formatted addresses
 * in @list.
 *
 * Returns: a string containing the list of addresses in rfc822 format
 * or %NULL if no addresses are contained in the list.
 **/
char *
internet_address_list_to_string (InternetAddressList *list, gboolean encode)
{
	guint32 flags = encode ? INTERNET_ADDRESS_ENCODE : 0;
	size_t linelen = 0;
	GString *string;
	char *str;
	
	g_return_val_if_fail (IS_INTERNET_ADDRESS_LIST (list), NULL);
	
	if (list->array->len == 0)
		return NULL;
	
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
 * @str, folding appropriately.
 **/
void
internet_address_list_writer (InternetAddressList *list, GString *str)
{
	guint32 flags = INTERNET_ADDRESS_ENCODE | INTERNET_ADDRESS_FOLD;
	size_t linelen = str->len;
	
	g_return_if_fail (IS_INTERNET_ADDRESS_LIST (list));
	g_return_if_fail (str != NULL);
	
	_internet_address_list_to_string (list, flags, &linelen, str);
}

static void
_internet_address_decode_name (InternetAddress *ia, GString *name)
{
	char *value, *buf = NULL;
	char *phrase;
	
	if (!g_utf8_validate (name->str, name->len, NULL)) {
		/* A (broken) mailer has sent us raw 8bit/multibyte text data... */
		buf = g_mime_utils_decode_8bit (name->str, name->len);
		phrase = buf;
	} else {
		phrase = name->str;
	}
	
	/* decode the phrase */
	g_mime_utils_unquote_string (phrase);
	value = g_mime_utils_header_decode_phrase (phrase);
	g_free (ia->name);
	ia->name = value;
	g_free (buf);
}

static InternetAddress *decode_address (const char **in);

static void
skip_lwsp (const char **in)
{
	register const char *inptr = *in;
	
	while (*inptr && is_lwsp (*inptr))
		inptr++;
	
	*in = inptr;
}

static InternetAddress *
decode_addrspec (const char **in)
{
	InternetAddress *mailbox = NULL;
	const char *start, *inptr, *word;
	gboolean got_local = FALSE;
	GString *addr;
	size_t len;
	
	addr = g_string_new ("");
	inptr = *in;
	
	decode_lwsp (&inptr);
	
	/* some spam bots set their addresses to stuff like: ).ORHH@em.ca */
	while (*inptr && !(*inptr == '"' || is_atom (*inptr)))
		inptr++;
	
	start = inptr;
	
	/* extract the first word of the local-part */
	if ((word = decode_word (&inptr))) {
		g_string_append_len (addr, word, (size_t) (inptr - word));
		decode_lwsp (&inptr);
		got_local = TRUE;
	}
	
	/* extract the rest of the local-part */
	while (word && *inptr == '.') {
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
			g_string_append_len (addr, word, (size_t) (inptr - word));
		
		decode_lwsp (&inptr);
	}
	
	if (*inptr == '@') {
		len = addr->len;
		
		g_string_append_c (addr, '@');
		inptr++;
		
		if (!decode_domain (&inptr, addr)) {
			/* drop the @domain and continue as if it weren't there */
			w(g_warning ("Missing domain in addr-spec: %.*s",
				     inptr - start, start));
			g_string_truncate (addr, len);
		}
	} else if (got_local) {
		w(g_warning ("Missing '@' and domain in addr-spec: %.*s",
			     inptr - start, start));
	}
	
	*in = inptr;
	
	if (!got_local) {
		w(g_warning ("Invalid addr-spec, missing local-part: %.*s",
			     inptr - start, start));
		g_string_free (addr, TRUE);
		return NULL;
	}
	
	mailbox = g_object_newv (INTERNET_ADDRESS_TYPE_MAILBOX, 0, NULL);
	((InternetAddressMailbox *) mailbox)->addr = addr->str;
	g_string_free (addr, FALSE);
	
	return mailbox;
}

static InternetAddress *
decode_group (const char **in)
{
	InternetAddressGroup *group;
	InternetAddress *addr;
	const char *inptr;
	
	inptr = *in;
	
	addr = internet_address_group_new (NULL);
	group = (InternetAddressGroup *) addr;
	
	decode_lwsp (&inptr);
	while (*inptr && *inptr != ';') {
		InternetAddress *member;
		
		if ((member = decode_address (&inptr)))
			_internet_address_group_add_member (group, member);
		
		decode_lwsp (&inptr);
		while (*inptr == ',') {
			inptr++;
			decode_lwsp (&inptr);
			if ((member = decode_address (&inptr)))
				_internet_address_group_add_member (group, member);
			
			decode_lwsp (&inptr);
		}
	}
	
	*in = inptr;
	
	return addr;
}

static gboolean
decode_route (const char **in)
{
	const char *start = *in;
	const char *inptr = *in;
	GString *route;
	
	route = g_string_new ("");
	
	do {
		inptr++;
		
		g_string_append_c (route, '@');
		if (!decode_domain (&inptr, route))
			goto error;
		
		decode_lwsp (&inptr);
		if (*inptr == ',') {
			g_string_append_c (route, ',');
			inptr++;
			decode_lwsp (&inptr);
			
			/* obs-domain-lists allow commas with nothing between them... */
			while (*inptr == ',') {
				inptr++;
				decode_lwsp (&inptr);
			}
		}
	} while (*inptr == '@');
	
	g_string_free (route, TRUE);
	decode_lwsp (&inptr);
	
	if (*inptr != ':') {
		w(g_warning ("Invalid route domain-list, missing ':': %.*s", inptr - start, start));
		goto error;
	}
	
	/* eat the ':' */
	*in = inptr + 1;
	
	return TRUE;
	
 error:
	
	while (*inptr && *inptr != ':' && *inptr != '>')
		inptr++;
	
	if (*inptr == ':')
		inptr++;
	
	*in = inptr;
	
	return FALSE;
}

static InternetAddress *
decode_address (const char **in)
{
	const char *inptr, *start, *word, *comment = NULL;
	InternetAddress *addr = NULL;
	gboolean has_lwsp = FALSE;
	gboolean is_word;
	GString *name;
	
	decode_lwsp (in);
	start = inptr = *in;
	
	name = g_string_new ("");
	
	/* Both groups and mailboxes can begin with a phrase (denoting
	 * the display name for the address). Collect all of the
	 * tokens that make up this name phrase.
	 */
	while (*inptr) {
		if ((word = decode_word (&inptr))) {
			g_string_append_len (name, word, (size_t) (inptr - word));
			
		check_lwsp:
			word = inptr;
			skip_lwsp (&inptr);
			
			/* is the next token a word token? */
			is_word = *inptr == '"' || is_atom (*inptr);
			
			if (inptr > word && is_word) {
				g_string_append_c (name, ' ');
				has_lwsp = TRUE;
			}
			
			if (is_word)
				continue;
		}
		
		/* specials    =  "(" / ")" / "<" / ">" / "@"  ; Must be in quoted-
		 *             /  "," / ";" / ":" / "\" / <">  ;  string, to use
		 *             /  "." / "[" / "]"              ;  within a word.
		 */
		if (*inptr == ':') {
			/* rfc2822 group */
			inptr++;
			addr = decode_group (&inptr);
			decode_lwsp (&inptr);
			if (*inptr != ';')
				w(g_warning ("Invalid group spec, missing closing ';': %.*s",
					     inptr - start, start));
			else
				inptr++;
			break;
		} else if (*inptr == '<') {
			/* rfc2822 angle-addr */
			inptr++;
			
			/* check for obsolete routing... */
			if (*inptr != '@' || decode_route (&inptr)) {
				/* rfc2822 addr-spec */
				addr = decode_addrspec (&inptr);
			}
			
			decode_lwsp (&inptr);
			if (*inptr != '>') {
				w(g_warning ("Invalid rfc2822 angle-addr, missing closing '>': %.*s",
					     inptr - start, start));
				
				while (*inptr && *inptr != '>' && *inptr != ',')
					inptr++;
				
				if (*inptr == '>')
					inptr++;
			} else {
				inptr++;
			}
			
			/* if comment is non-NULL, we can check for a comment containing a name */
			comment = inptr;
			break;
		} else if (*inptr == '(') {
			/* beginning of a comment, use decode_lwsp() to skip past it */
			decode_lwsp (&inptr);
		} else if (*inptr && strchr ("@,;", *inptr)) {
			if (name->len == 0) {
				if (*inptr == '@') {
					GString *domain;
					
					w(g_warning ("Unexpected address: %s: skipping.", start));
					
					/* skip over @domain? */
					inptr++;
					domain = g_string_new ("");
					decode_domain (&inptr, domain);
					g_string_free (domain, TRUE);
				} else {
					/* empty address */
				}
				break;
			} else if (has_lwsp) {
				/* assume this is just an unquoted special that we should
				   treat as part of the name */
				w(g_warning ("Unquoted '%c' in address name: %s: ignoring.", *inptr, start));
				g_string_append_c (name, *inptr);
				inptr++;
				
				goto check_lwsp;
			}
			
		addrspec:
			/* what we thought was a name was actually an addrspec? */
			g_string_truncate (name, 0);
			inptr = start;
			
			addr = decode_addrspec (&inptr);
			
			/* if comment is non-NULL, we can check for a comment containing a name */
			comment = inptr;
			break;
		} else if (*inptr == '.') {
			/* This would normally signify that we are
			 * decoding the local-part of an addr-spec,
			 * but sadly, it is common for broken mailers
			 * to forget to quote/encode .'s in the name
			 * phrase. */
			g_string_append_c (name, *inptr);
			inptr++;
			
			goto check_lwsp;
		} else if (*inptr) {
			/* Technically, these are all invalid tokens
			 * but in the interest of being liberal in
			 * what we accept, we'll ignore them. */
			w(g_warning ("Unexpected char '%c' in address: %s: ignoring.", *inptr, start));
			g_string_append_c (name, *inptr);
			inptr++;
			
			goto check_lwsp;
		} else {
			goto addrspec;
		}
	}
	
	/* Note: will also skip over any comments */
	decode_lwsp (&inptr);
	
	if (name->len == 0 && comment && inptr > comment) {
		/* missing a name, look for a trailing comment */
		if ((comment = memchr (comment, '(', inptr - comment))) {
			const char *cend;
			
			/* find the end of the comment */
			cend = inptr - 1;
			while (cend > comment && is_lwsp (*cend))
				cend--;
			
			if (*cend == ')')
				cend--;
			
			g_string_append_len (name, comment + 1, (size_t) (cend - comment));
		}
	}
	
	if (addr && name->len > 0)
		_internet_address_decode_name (addr, name);
	
	g_string_free (name, TRUE);
	
	*in = inptr;
	
	return addr;
}


/**
 * internet_address_list_parse_string:
 * @str: a string containing internet addresses
 *
 * Construct a list of internet addresses from the given string.
 *
 * Returns: (transfer full): a #InternetAddressList or %NULL if the
 * input string does not contain any addresses.
 **/
InternetAddressList *
internet_address_list_parse_string (const char *str)
{
	InternetAddressList *addrlist;
	const char *inptr = str;
	InternetAddress *addr;
	const char *start;
	
	addrlist = internet_address_list_new ();
	
	while (inptr && *inptr) {
		start = inptr;
		
		if ((addr = decode_address (&inptr))) {
			_internet_address_list_add (addrlist, addr);
		} else {
			w(g_warning ("Invalid or incomplete address: %.*s",
				     inptr - start, start));
		}
		
		decode_lwsp (&inptr);
		if (*inptr == ',') {
			inptr++;
			decode_lwsp (&inptr);
			
			/* obs-mbox-list and obs-addr-list allow for empty members (commas with nothing between them) */
			while (*inptr == ',') {
				inptr++;
				decode_lwsp (&inptr);
			}
		} else if (*inptr) {
			w(g_warning ("Parse error at '%s': expected ','", inptr));
			/* try skipping to the next address */
			if ((inptr = strchr (inptr, ',')))
				inptr++;
		}
	}
	
	if (addrlist->array->len == 0) {
		g_object_unref (addrlist);
		addrlist = NULL;
	}
	
	return addrlist;
}
