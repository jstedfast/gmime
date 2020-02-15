/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*  GMime
 *  Copyright (C) 2000-2020 Jeffrey Stedfast
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
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>

#ifdef LIBIDN
#include <idn2.h>
#undef G_GNUC_UNUSED
#undef G_GNUC_DEPRECATED
#endif

#include "internet-address.h"
#include "gmime-table-private.h"
#include "gmime-parse-utils.h"
#include "gmime-iconv-utils.h"
#include "gmime-internal.h"
#include "gmime-events.h"
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

static gboolean addrspec_parse (const char **in, const char *sentinels, char **addrspec, int *at);

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
	ia->changed = g_mime_event_new ((GObject *) ia);
	ia->charset = NULL;
	ia->name = NULL;
}

static void
internet_address_finalize (GObject *object)
{
	InternetAddress *ia = (InternetAddress *) object;
	
	g_mime_event_free (ia->changed);
	g_free (ia->charset);
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
	
	g_mime_event_emit (ia->changed, NULL);
}


/**
 * internet_address_get_name:
 * @ia: a #InternetAddress
 *
 * Gets the display name of the #InternetAddress.
 *
 * Returns: (nullable): the name of the mailbox or group in a form suitable
 * for display if available or %NULL otherwise. If the name is available,
 * the returned string will be in UTF-8.
 **/
const char *
internet_address_get_name (InternetAddress *ia)
{
	g_return_val_if_fail (IS_INTERNET_ADDRESS (ia), NULL);
	
	return ia->name;
}


/**
 * internet_address_set_charset:
 * @ia: a #InternetAddress
 * @charset: (nullable): the charset to use when encoding the name or %NULL to use the defaults
 *
 * Set the charset to use for encoding the name of the mailbox or group.
 **/
void
internet_address_set_charset (InternetAddress *ia, const char *charset)
{
	char *buf;
	
	g_return_if_fail (IS_INTERNET_ADDRESS (ia));
	
	buf = charset ? g_strdup (charset) : NULL;
	g_free (ia->charset);
	ia->charset = buf;
	
	g_mime_event_emit (ia->changed, NULL);
}


/**
 * internet_address_get_charset:
 * @ia: a #InternetAddress
 *
 * Gets the charset to be used when encoding the name of the mailbox or group.
 *
 * Returns: (nullable): the charset to be used when encoding the name of the
 * mailbox or group if available or %NULL otherwise.
 **/
const char *
internet_address_get_charset (InternetAddress *ia)
{
	g_return_val_if_fail (IS_INTERNET_ADDRESS (ia), NULL);
	
	return ia->charset;
}


/**
 * internet_address_to_string:
 * @ia: Internet Address object
 * @options: (nullable): a #GMimeFormatOptions or %NULL
 * @encode: %TRUE if the address should be rfc2047 encoded
 *
 * Allocates a string containing the contents of the #InternetAddress
 * object.
 * 
 * Returns: the #InternetAddress object as an allocated string in
 * rfc822 format.
 **/
char *
internet_address_to_string (InternetAddress *ia, GMimeFormatOptions *options, gboolean encode)
{
	guint32 flags = encode ? INTERNET_ADDRESS_ENCODE : 0;
	size_t linelen = 0;
	GString *string;
	char *str;
	
	string = g_string_new ("");
	INTERNET_ADDRESS_GET_CLASS (ia)->to_string (ia, options, flags, &linelen, string);
	str = string->str;
	
	g_string_free (string, FALSE);
	
	return str;
}


static void internet_address_mailbox_class_init (InternetAddressMailboxClass *klass);
static void internet_address_mailbox_init (InternetAddressMailbox *mailbox, InternetAddressMailboxClass *klass);
static void internet_address_mailbox_finalize (GObject *object);

static void mailbox_to_string (InternetAddress *ia, GMimeFormatOptions *options, guint32 flags,
			       size_t *linelen, GString *out);


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
	mailbox->idn_addr = NULL;
	mailbox->addr = NULL;
	mailbox->at = -1;
}

static void
internet_address_mailbox_finalize (GObject *object)
{
	InternetAddressMailbox *mailbox = (InternetAddressMailbox *) object;
	
	g_free (mailbox->idn_addr);
	g_free (mailbox->addr);
	
	G_OBJECT_CLASS (mailbox_parent_class)->finalize (object);
}

static InternetAddress *
_internet_address_mailbox_new (const char *name, const char *addr, int at)
{
	InternetAddressMailbox *mailbox;
	
	mailbox = g_object_new (INTERNET_ADDRESS_TYPE_MAILBOX, NULL);
	mailbox->addr = g_strdup (addr);
	mailbox->at = at;
	
	_internet_address_set_name ((InternetAddress *) mailbox, name);
	
	return (InternetAddress *) mailbox;
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
	const char *inptr = addr;
	
	g_return_val_if_fail (addr != NULL, NULL);
	
	mailbox = g_object_new (INTERNET_ADDRESS_TYPE_MAILBOX, NULL);

	if (!addrspec_parse (&inptr, "", &mailbox->addr, &mailbox->at))
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
	const char *inptr = addr;
	
	g_return_if_fail (INTERNET_ADDRESS_IS_MAILBOX (mailbox));
	
	if (mailbox->addr == addr)
		return;
	
	g_free (mailbox->idn_addr);
	mailbox->idn_addr = NULL;
	
	g_free (mailbox->addr);
	
	if (!addrspec_parse (&inptr, "", &mailbox->addr, &mailbox->at))
		mailbox->addr = g_strdup (addr);
	
	g_mime_event_emit (((InternetAddress *) mailbox)->changed, NULL);
}


/**
 * internet_address_mailbox_get_addr:
 * @mailbox: a #InternetAddressMailbox
 *
 * Gets the addr-spec of the internet address mailbox.
 *
 * Returns: the addr-spec string.
 **/
const char *
internet_address_mailbox_get_addr (InternetAddressMailbox *mailbox)
{
	g_return_val_if_fail (INTERNET_ADDRESS_IS_MAILBOX (mailbox), NULL);
	
	return mailbox->addr;
}


/**
 * internet_address_mailbox_get_idn_addr:
 * @mailbox: a #InternetAddressMailbox
 *
 * Gets the IDN ascii-encoded addr-spec.
 *
 * Returns: the encoded addr-spec string.
 **/
const char *
internet_address_mailbox_get_idn_addr (InternetAddressMailbox *mailbox)
{
	GString *encoded;
	char *ascii;
	
	g_return_val_if_fail (INTERNET_ADDRESS_IS_MAILBOX (mailbox), NULL);
	
#ifdef LIBIDN
	if (!mailbox->idn_addr && mailbox->at > 0) {
		const char *domain = mailbox->addr + mailbox->at + 1;
		
		encoded = g_string_new ("");
		g_string_append_len (encoded, mailbox->addr, mailbox->at + 1);
		if (idn2_to_ascii_8z (domain, &ascii, 0) == IDN2_OK) {
			if (!g_ascii_strcasecmp (domain, ascii))
				g_string_append (encoded, domain);
			else
				g_string_append (encoded, ascii);
			idn2_free (ascii);
		} else {
			g_string_append (encoded, domain);
		}
		
		mailbox->idn_addr = g_string_free (encoded, FALSE);
	}
	
	return mailbox->idn_addr ? mailbox->idn_addr : mailbox->addr;
#else
	return mailbox->addr;
#endif
}


static void internet_address_group_class_init (InternetAddressGroupClass *klass);
static void internet_address_group_init (InternetAddressGroup *group, InternetAddressGroupClass *klass);
static void internet_address_group_finalize (GObject *object);

static void group_to_string (InternetAddress *ia, GMimeFormatOptions *options, guint32 flags,
			     size_t *linelen, GString *out);


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
	g_mime_event_emit (((InternetAddress *) group)->changed, NULL);
}

static void
internet_address_group_init (InternetAddressGroup *group, InternetAddressGroupClass *klass)
{
	group->members = internet_address_list_new ();
	
	g_mime_event_add (group->members->changed, (GMimeEventCallback) members_changed, group);
}

static void
internet_address_group_finalize (GObject *object)
{
	InternetAddressGroup *group = (InternetAddressGroup *) object;
	
	g_mime_event_remove (group->members->changed, (GMimeEventCallback) members_changed, group);
	
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
	
	group = g_object_new (INTERNET_ADDRESS_TYPE_GROUP, NULL);
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
		g_mime_event_remove (group->members->changed, (GMimeEventCallback) members_changed, group);
		g_object_unref (group->members);
	}
	
	if (members) {
		g_mime_event_add (members->changed, (GMimeEventCallback) members_changed, group);
		g_object_ref (members);
	}
	
	group->members = members;
	
	g_mime_event_emit (((InternetAddress *) group)->changed, NULL);
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
	list->changed = g_mime_event_new ((GObject *) list);
	list->array = g_ptr_array_new ();
}

static void
address_changed (InternetAddress *ia, gpointer args, InternetAddressList *list)
{
	g_mime_event_emit (list->changed, NULL);
}

static void
internet_address_list_finalize (GObject *object)
{
	InternetAddressList *list = (InternetAddressList *) object;
	InternetAddress *ia;
	guint i;
	
	for (i = 0; i < list->array->len; i++) {
		ia = (InternetAddress *) list->array->pdata[i];
		g_mime_event_remove (ia->changed, (GMimeEventCallback) address_changed, list);
		g_object_unref (ia);
	}
	
	g_mime_event_free (list->changed);
	
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
	return g_object_new (INTERNET_ADDRESS_LIST_TYPE, NULL);
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
		g_mime_event_remove (ia->changed, (GMimeEventCallback) address_changed, list);
		g_object_unref (ia);
	}
	
	g_ptr_array_set_size (list->array, 0);
	
	g_mime_event_emit (list->changed, NULL);
}


static int
_internet_address_list_add (InternetAddressList *list, InternetAddress *ia)
{
	int index;
	
	g_mime_event_add (ia->changed, (GMimeEventCallback) address_changed, list);
	
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
	
	g_mime_event_emit (list->changed, NULL);
	
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
	
	memmove (dest, src, (sizeof (void *) * list->array->len));
	
	for (i = 0; i < prepend->array->len; i++) {
		ia = (InternetAddress *) prepend->array->pdata[i];
		g_mime_event_add (ia->changed, (GMimeEventCallback) address_changed, list);
		list->array->pdata[i] = ia;
		g_object_ref (ia);
	}
	
	g_mime_event_emit (list->changed, NULL);
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
		g_mime_event_add (ia->changed, (GMimeEventCallback) address_changed, list);
		list->array->pdata[len + i] = ia;
		g_object_ref (ia);
	}
	
	g_mime_event_emit (list->changed, NULL);
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
	
	g_mime_event_add (ia->changed, (GMimeEventCallback) address_changed, list);
	g_object_ref (ia);
	
	if ((guint) index < list->array->len) {
		g_ptr_array_set_size (list->array, list->array->len + 1);
		
		dest = ((char *) list->array->pdata) + (sizeof (void *) * (index + 1));
		src = ((char *) list->array->pdata) + (sizeof (void *) * index);
		n = list->array->len - index - 1;
		
		memmove (dest, src, (sizeof (void *) * n));
		list->array->pdata[index] = ia;
	} else {
		/* the easy case */
		g_ptr_array_add (list->array, ia);
	}
	
	g_mime_event_emit (list->changed, NULL);
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
	g_mime_event_remove (ia->changed, (GMimeEventCallback) address_changed, list);
	g_object_unref (ia);
	
	g_ptr_array_remove_index (list->array, index);
	
	g_mime_event_emit (list->changed, NULL);
	
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
	
	g_mime_event_remove (old->changed, (GMimeEventCallback) address_changed, list);
	g_object_unref (old);
	
	g_mime_event_add (ia->changed, (GMimeEventCallback) address_changed, list);
	list->array->pdata[index] = ia;
	g_object_ref (ia);
	
	g_mime_event_emit (list->changed, NULL);
}


static char *
encoded_name (GMimeFormatOptions *options, const char *raw, gboolean rfc2047_encode, const char *charset)
{
	char *name;
	
	g_return_val_if_fail (raw != NULL, NULL);
	
	if (rfc2047_encode) {
		name = g_mime_utils_header_encode_phrase (options, raw, charset);
	} else {
		name = g_mime_utils_quote_string (raw);
	}
	
	return name;
}

static void
linewrap (GString *str, const char *newline)
{
	if (str->len > 0 && str->str[str->len - 1] == ' ') {
		str->str[str->len - 1] = newline[0];
		
		if (newline[1])
			g_string_append_c (str, newline[1]);
	} else {
		g_string_append (str, newline);
	}
	
	g_string_append_c (str, '\t');
}

static void
append_folded_name (GString *str, size_t *linelen, const char *name, const char *newline)
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
			linewrap (str, newline);
			*linelen = 1;
		}
		
		g_string_append_len (str, word, len);
		*linelen += len;
		
		word = lwsp;
		while (*word && is_lwsp (*word))
			word++;
		
		if (*word && is_lwsp (*lwsp)) {
			g_string_append_c (str, ' ');
			(*linelen)++;
		}
	}
}

static void
mailbox_to_string (InternetAddress *ia, GMimeFormatOptions *options, guint32 flags, size_t *linelen, GString *str)
{
	InternetAddressMailbox *mailbox = (InternetAddressMailbox *) ia;
	gboolean encode = flags & INTERNET_ADDRESS_ENCODE;
	gboolean fold = flags & INTERNET_ADDRESS_FOLD;
	const char *newline, *addr;
	char *name;
	size_t len;
	
	newline = g_mime_format_options_get_newline (options);
	
	addr = internet_address_mailbox_get_idn_addr (mailbox);
	
	if (ia->name && *ia->name) {
		name = encoded_name (options, ia->name, encode, ia->charset);
		len = strlen (name);
		
		if (fold && (*linelen + len) > GMIME_FOLD_LEN) {
			if (len > GMIME_FOLD_LEN) {
				/* we need to break up the name */
				append_folded_name (str, linelen, name, newline);
			} else {
				/* the name itself is short enough to fit on a single
				 * line, but only if we write it on a line by itself */
				if (*linelen > 1) {
					linewrap (str, newline);
					*linelen = 1;
				}
				
				g_string_append_len (str, name, len);
				*linelen += len;
			}
		} else {
			/* we can safely fit the name on this line */
			g_string_append_len (str, name, len);
			*linelen += len;
		}
		
		g_free (name);
		
		len = strlen (addr);
		
		if (fold && (*linelen + len + 3) >= GMIME_FOLD_LEN) {
			g_string_append (str, newline);
			g_string_append_len (str, "\t<", 2);
			*linelen = 2;
		} else {
			g_string_append_len (str, " <", 2);
			*linelen += 2;
		}
		
		g_string_append_len (str, addr, len);
		g_string_append_c (str, '>');
		*linelen += len + 1;
	} else {
		len = strlen (addr);
		
		if (fold && (*linelen + len) > GMIME_FOLD_LEN) {
			linewrap (str, newline);
			*linelen = 1;
		}
		
		g_string_append_len (str, addr, len);
		*linelen += len;
	}
}

static void
_internet_address_list_to_string (const InternetAddressList *list, GMimeFormatOptions *options, guint32 flags,
				  size_t *linelen, GString *str)
{
	InternetAddress *ia;
	guint i;
	
	for (i = 0; i < list->array->len; i++) {
		ia = (InternetAddress *) list->array->pdata[i];
		
		INTERNET_ADDRESS_GET_CLASS (ia)->to_string (ia, options, flags, linelen, str);
		
		if (i + 1 < list->array->len) {
			g_string_append (str, ", ");
			*linelen += 2;
		}
	}
}

static void
group_to_string (InternetAddress *ia, GMimeFormatOptions *options, guint32 flags, size_t *linelen, GString *str)
{
	InternetAddressGroup *group = (InternetAddressGroup *) ia;
	gboolean encode = flags & INTERNET_ADDRESS_ENCODE;
	gboolean fold = flags & INTERNET_ADDRESS_FOLD;
	const char *newline;
	char *name = NULL;
	size_t len = 0;
	
	newline = g_mime_format_options_get_newline (options);
	
	if (ia->name != NULL) {
		name = encoded_name (options, ia->name, encode, ia->charset);
		len = strlen (name);
		
		if (fold && *linelen > 1 && (*linelen + len + 1) > GMIME_FOLD_LEN) {
			linewrap (str, newline);
			*linelen = 1;
		}
		
		g_string_append_len (str, name, len);
	}
	
	g_string_append_len (str, ": ", 2);
	*linelen += len + 2;
	g_free (name);
	
	_internet_address_list_to_string (group->members, options, flags, linelen, str);
	g_string_append_c (str, ';');
	*linelen += 1;
}


/**
 * internet_address_list_to_string:
 * @list: list of internet addresses
 * @options: (nullable): a #GMimeFormatOptions or %NULL
 * @encode: %TRUE if the address should be rfc2047 encoded
 *
 * Allocates a string buffer containing the rfc822 formatted addresses
 * in @list.
 *
 * Returns: (nullable): a string containing the list of addresses in rfc822
 * format or %NULL if no addresses are contained in the list.
 **/
char *
internet_address_list_to_string (InternetAddressList *list, GMimeFormatOptions *options, gboolean encode)
{
	guint32 flags = encode ? INTERNET_ADDRESS_ENCODE : 0;
	size_t linelen = 0;
	GString *str;
	
	g_return_val_if_fail (IS_INTERNET_ADDRESS_LIST (list), NULL);
	
	if (list->array->len == 0)
		return NULL;
	
	str = g_string_new ("");
	_internet_address_list_to_string (list, options, flags, &linelen, str);
	
	return g_string_free (str, FALSE);
}


/**
 * internet_address_list_encode:
 * @list: list of internet addresses
 * @options: (nullable): a #GMimeFormatOptions or %NULL
 * @str: string to write to
 *
 * Writes the rfc2047-encoded rfc822 formatted addresses in @list to
 * @str, folding appropriately.
 **/
void
internet_address_list_encode (InternetAddressList *list, GMimeFormatOptions *options, GString *str)
{
	guint32 flags = INTERNET_ADDRESS_ENCODE | INTERNET_ADDRESS_FOLD;
	const char *newline;
	size_t linelen;
	
	g_return_if_fail (IS_INTERNET_ADDRESS_LIST (list));
	g_return_if_fail (str != NULL);
	
	newline = g_mime_format_options_get_newline (options);
	linelen = str->len;
	
	_internet_address_list_to_string (list, options, flags, &linelen, str);
	
	g_string_append (str, newline);
}


static char *
decode_name (GMimeParserOptions *options, const char *name, size_t len, const char **charset, gint64 offset)
{
	char *value, *buf = NULL;
	
	if (!g_utf8_validate (name, len, NULL)) {
		/* A (broken) mailer has sent us raw 8bit/multibyte text data... */
		buf = g_mime_utils_decode_8bit (options, name, len);
	} else {
		buf = g_strndup (name, len);
	}
	
	/* decode the phrase */
	g_mime_utils_unquote_string (buf);
	value = _g_mime_utils_header_decode_phrase (options, buf, charset, offset);
	g_strstrip (value);
	g_free (buf);
	
	return value;
}


typedef enum {
	ALLOW_MAILBOX = 1 << 0,
	ALLOW_GROUP   = 1 << 1,
	ALLOW_ANY     = ALLOW_MAILBOX | ALLOW_GROUP
} AddressParserFlags;

static gboolean
decode_route (const char **in)
{
	const char *inptr = *in;
	GString *route;
	
	route = g_string_new ("");
	
	do {
		inptr++;
		
		g_string_append_c (route, '@');
		if (!decode_domain (&inptr, route)) {
			g_string_free (route, TRUE);
			goto error;
		}
		
		skip_cfws (&inptr);
		if (*inptr == ',') {
			g_string_append_c (route, ',');
			inptr++;
			skip_cfws (&inptr);
			
			/* obs-domain-lists allow commas with nothing between them... */
			while (*inptr == ',') {
				inptr++;
				skip_cfws (&inptr);
			}
		}
	} while (*inptr == '@');
	
	g_string_free (route, TRUE);
	skip_cfws (&inptr);
	
	if (*inptr != ':') {
		w(g_warning ("Invalid route domain-list, missing ':': %.*s", inptr - start, start));
		goto error;
	}
	
	*in = inptr;
	
	return TRUE;
	
 error:
	
	while (*inptr && *inptr != ':' && *inptr != '>')
		inptr++;
	
	*in = inptr;
	
	return FALSE;
}

static gboolean
localpart_parse (GString *localpart, const char **in)
{
	const char *inptr = *in;
	const char *word;
	
	do {
		word = inptr;
		
		if (!skip_word (&inptr))
			goto error;
		
		if (!g_utf8_validate (word, (size_t) (inptr - word), NULL))
			goto error;
		
		g_string_append_len (localpart, word, (size_t) (inptr - word));
		
		if (!skip_cfws (&inptr))
			goto error;
		
		if (*inptr != '.')
			break;
		
		g_string_append_c (localpart, *inptr++);
		
		if (!skip_cfws (&inptr))
			goto error;
		
		if (*inptr == '\0')
			goto error;
	} while (TRUE);
	
	*in = inptr;
	
	return TRUE;
	
 error:
	*in = inptr;
	
	return FALSE;
}

#define COMMA_GREATER_THAN_OR_SEMICOLON ",>;"

static gboolean
dotatom_parse (GString *str, const char **in, const char *sentinels)
{
	const char *atom, *comment;
	const char *inptr = *in;
	GString *domain = str;
	
	do {
		if (!is_atom (*inptr))
			goto error;
		
		atom = inptr;
		while (is_atom (*inptr))
			inptr++;
		
		if (!g_utf8_validate (atom, (size_t) (inptr - atom), NULL))
			goto error;
		
#if LIBIDN
		if (domain == str && !strncmp (atom, "xn--", 4)) {
			/* from here on out, we'll use a temp domain buffer so that
			 * we can decode it once we're done parsing it */
			domain = g_string_new ("");
		}
#endif
		
		g_string_append_len (domain, atom, (size_t) (inptr - atom));
		
		comment = inptr;
		if (!skip_cfws (&inptr))
			goto error;
		
		if (*inptr != '.') {
			inptr = comment;
			break;
		}
		
		/* skip over the '.' */
		inptr++;
		
		if (!skip_cfws (&inptr))
			goto error;
		
		/* allow domains to end with a '.', but strip it off */
		if (*inptr == '\0' || strchr (sentinels, *inptr))
			break;
		
		g_string_append_c (domain, '.');
	} while (TRUE);
	
#ifdef LIBIDN
	if (domain != str) {
		char *unicode;
		
		if (idn2_to_unicode_8z8z (domain->str, &unicode, 0) == IDN2_OK) {
			g_string_append (str, unicode);
			free (unicode);
		} else {
			g_string_append_len (str, domain->str, domain->len);
		}
		
		g_string_free (domain, TRUE);
	}
#endif
	
	*in = inptr;
	
	return TRUE;
	
 error:
#ifdef LIBIDN
	if (domain != str) {
		g_string_append_len (str, domain->str, domain->len);
		g_string_free (domain, TRUE);
	}
#endif
	
	*in = inptr;
	
	return FALSE;
}

static gboolean
domain_literal_parse (GString *str, const char **in)
{
	const char *inptr = *in;
	
	g_string_append_c (str, '[');
	inptr++;
	
	skip_lwsp (&inptr);
	
	do {
		while (*inptr && is_dtext (*inptr))
			g_string_append_c (str, *inptr++);
		
		skip_lwsp (&inptr);
		
		if (*inptr == '\0')
			goto error;
		
		if (*inptr == ']')
			break;
		
		if (!is_dtext (*inptr))
			goto error;
	} while (TRUE);
	
	g_string_append_c (str, ']');
	*in = inptr + 1;
	
	return TRUE;
	
 error:
	*in = inptr;
	
	return FALSE;
}

static gboolean
domain_parse (GString *str, const char **in, const char *sentinels)
{
	if (**in == '[')
		return domain_literal_parse (str, in);
	
	return dotatom_parse (str, in, sentinels);
}

static gboolean
addrspec_parse (const char **in, const char *sentinels, char **addrspec, int *at)
{
	const char *inptr = *in;
	GString *str;
	
	str = g_string_new ("");
	
	if (!localpart_parse (str, &inptr))
		goto error;
	
	if (*inptr == '\0' || strchr (sentinels, *inptr)) {
		*addrspec = g_string_free (str, FALSE);
		*in = inptr;
		*at = -1;
		return TRUE;
	}
	
	if (*inptr != '@')
		goto error;
	
	*at = str->len;
	g_string_append_c (str, *inptr++);
	
	if (*inptr == '\0')
		goto error;
	
	if (!skip_cfws (&inptr))
		goto error;
	
	if (*inptr == '\0')
		goto error;
	
	if (!domain_parse (str, &inptr, sentinels))
		goto error;
	
	*addrspec = g_string_free (str, FALSE);
	*in = inptr;
	
	return TRUE;
	
 error:
	g_string_free (str, TRUE);
	*addrspec = NULL;
	*in = inptr;
	*at = -1;
	
	return FALSE;
}

// TODO: rename to angleaddr_parse??
static gboolean
mailbox_parse (GMimeParserOptions *options, const char **in, const char *name, InternetAddress **address)
{
	GMimeRfcComplianceMode mode = g_mime_parser_options_get_address_compliance_mode (options);
	const char *inptr = *in;
	char *addrspec = NULL;
	int at;
	
	/* skip over the '<' */
	inptr++;
	
	/* Note: check for excessive angle brackets like the example described in section 7.1.2 of rfc7103... */
	if (*inptr == '<') {
		if (mode != GMIME_RFC_COMPLIANCE_LOOSE)
			goto error;
		
		do {
			inptr++;
		} while (*inptr == '<');
	}
	
	if (*inptr == '\0')
		goto error;
	
	if (!skip_cfws (&inptr))
		goto error;
	
	if (*inptr == '@') {
		if (!decode_route (&inptr))
			goto error;
		
		if (*inptr != ':')
			goto error;
		
		inptr++;
		
		if (!skip_cfws (&inptr))
			goto error;
	}
	
	// Note: The only syntactically correct sentinel token here is the '>', but alas... to deal with the first
	// example in section 7.1.5 of rfc7103, we need to at least handle ',' as a sentinel and might as well handle
	// ';' as well in case the mailbox is within a group address.
	//
	// Example: <third@example.net, fourth@example.net>
	if (!addrspec_parse (&inptr, COMMA_GREATER_THAN_OR_SEMICOLON, &addrspec, &at))
		goto error;
	
	if (!skip_cfws (&inptr))
		goto error;
	
	if (*inptr != '>') {
		if (mode != GMIME_RFC_COMPLIANCE_LOOSE)
			goto error;
	} else {
		/* skip over the '>' */
		inptr++;
		
		/* Note: check for excessive angle brackets like the example described in section 7.1.2 of rfc7103... */
		if (*inptr == '>') {
			if (mode != GMIME_RFC_COMPLIANCE_LOOSE)
				goto error;
			
			do {
				inptr++;
			} while (*inptr == '>');
		}
	}
	
	*address = _internet_address_mailbox_new (name, addrspec, at);
	g_free (addrspec);
	*in = inptr;
	
	return TRUE;
	
 error:
	g_free (addrspec);
	*address = NULL;
	*in = inptr;
	
	return FALSE;
}

static gboolean address_list_parse (InternetAddressList *list, GMimeParserOptions *options, const char **in, gboolean is_group, gint64 offset);

static gboolean
group_parse (InternetAddressGroup *group, GMimeParserOptions *options, const char **in, gint64 offset)
{
	const char *inptr = *in;
	
	/* skip over the ':' */
	inptr++;
	
	/* ignore superfluous colons (and whitespace) */
	while (*inptr == ':' || is_lwsp (*inptr))
		inptr++;
	
	if (*inptr != '\0') {
		address_list_parse (group->members, options, &inptr, TRUE, offset);
		
		if (*inptr != ';') {
			while (*inptr && *inptr != ';')
				inptr++;
		} else {
			inptr++;
		}
	}
	
	*in = inptr;
	
	return TRUE;
}

static gboolean
address_parse (GMimeParserOptions *options, AddressParserFlags flags, const char **in, const char **charset, InternetAddress **address, gint64 offset)
{
	GMimeRfcComplianceMode mode = g_mime_parser_options_get_address_compliance_mode (options);
	int min_words = g_mime_parser_options_get_allow_addresses_without_domain (options) ? 1 : 0;
	gboolean trim_leading_quote = FALSE;
	const char *inptr = *in;
	const char *start;
	size_t length;
	int words = 0;
	
	if (!skip_cfws (&inptr) || *inptr == '\0')
		goto error;
	
	/* keep track of the start & length of the phrase */
	start = inptr;
	length = 0;
	
	while (*inptr) {
		if (mode != GMIME_RFC_COMPLIANCE_LOOSE) {
			if (!skip_word (&inptr))
				break;
		} else if (*inptr == '"') {
			const char *qstring = inptr;
			
			if (!skip_quoted (&inptr)) {
				inptr = qstring + 1;
				
				skip_lwsp (&inptr);
				
				if (!skip_atom (&inptr))
					break;
				
				if (start == qstring)
					trim_leading_quote = TRUE;
			}
		} else {
			if (!skip_atom (&inptr))
				break;
		}
		
		length = (size_t) (inptr - start);
		
		do {
			if (!skip_cfws (&inptr))
				goto error;
			
			/* Note: some clients don't quote dots in the name */
			if (*inptr != '.')
				break;
			
			inptr++;
			
			length = (size_t) (inptr - start);
		} while (TRUE);
		
		words++;
		
		/* Note: some clients don't quote commas in the name */
		if (*inptr == ',' && words > min_words) {
			inptr++;
			
			length = (size_t) (inptr - start);
			
			if (!skip_cfws (&inptr))
				goto error;
		}
	}
	
	if (!skip_cfws (&inptr))
		goto error;
	
	// specials    =  "(" / ")" / "<" / ">" / "@"  ; Must be in quoted-
	//             /  "," / ";" / ":" / "\" / <">  ;  string, to use
	//             /  "." / "[" / "]"              ;  within a word.
	
	if (*inptr == '\0' || *inptr == ',' || *inptr == '>' || *inptr == ';') {
		/* we've completely gobbled up an addr-spec w/o a domain */
		char sentinel = *inptr != '\0' ? *inptr : ',';
		char sentinels[2] = { sentinel, 0 };
		char *name, *addrspec;
		int at;
		
		/* rewind back to the beginning of the local-part */
		inptr = start;
		
		if (!(flags & ALLOW_MAILBOX))
			goto error;
		
		if (!addrspec_parse (&inptr, sentinels, &addrspec, &at))
			goto error;
		
		skip_lwsp (&inptr);
		
		if (*inptr == '(') {
			const char *comment = inptr;
			
			if (!skip_comment (&inptr))
				goto error;
			
			comment++;
			
			name = decode_name (options, comment, (size_t) ((inptr - 1) - comment), charset, offset);
		} else {
			name = g_strdup ("");
		}
		
		if (*inptr == '>') {
			if (mode != GMIME_RFC_COMPLIANCE_LOOSE)
				goto error;
			
			inptr++;
		}
		
		*address = _internet_address_mailbox_new (name, addrspec, at);
		g_free (addrspec);
		g_free (name);
		*in = inptr;
		
		return TRUE;
	}
	
	if (*inptr == ':') {
		/* rfc2822 group address */
		InternetAddressGroup *group;
		const char *phrase = start;
		gboolean retval;
		char *name;
		
		if (!(flags & ALLOW_GROUP))
			goto error;
		
		if (trim_leading_quote) {
			phrase++;
			length--;
		}
		
		if (length > 0) {
			name = decode_name (options, phrase, length, charset, offset);
		} else {
			name = g_strdup ("");
		}
		
		group = (InternetAddressGroup *) internet_address_group_new (name);
		*address = (InternetAddress *) group;
		g_free (name);
		
		retval = group_parse (group, options, &inptr, offset);
		*in = inptr;
		
		return retval;
	}
	
	if (!(flags & ALLOW_MAILBOX))
		goto error;
	
	if (*inptr == '@') {
		/* we're either in the middle of an addr-spec token or we completely gobbled up
		 * an addr-spec w/o a domain */
		char *name, *addrspec;
		int at;
		
		/* rewind back to the beginning of the local-part */
		inptr = start;
		
		if (!addrspec_parse (&inptr, COMMA_GREATER_THAN_OR_SEMICOLON, &addrspec, &at))
			goto error;
		
		skip_lwsp (&inptr);
		
		if (*inptr == '(') {
			const char *comment = inptr;
			
			if (!skip_comment (&inptr))
				goto error;
			
			comment++;
			
			name = decode_name (options, comment, (size_t) ((inptr - 1) - comment), charset, offset);
		} else {
			name = g_strdup ("");
		}
		
		if (!skip_cfws (&inptr)) {
			g_free (addrspec);
			g_free (name);
			goto error;
		}
		
		if (*inptr == '\0') {
			*address = _internet_address_mailbox_new (name, addrspec, at);
			g_free (addrspec);
			g_free (name);
			*in = inptr;
			
			return TRUE;
		}
		
		if (*inptr == '<') {
			/* We have an address like "user@example.com <user@example.com>"; i.e. the name
			 * is an unquoted string with an '@'. */
			const char *end;
			
			if (mode != GMIME_RFC_COMPLIANCE_LOOSE)
				goto error;
			
			end = inptr;
			while (end > start && is_lwsp (*(end - 1)))
				end--;
			
			length = (size_t) (end - start);
			g_free (addrspec);
			g_free (name);
			
			/* fall through to the rfc822 angle-addr token case... */
		} else {
			/* Note: since there was no '<', there should not be a '>'... but we handle it
			 * anyway in order to deal with the second Unbalanced Angle Brackets example in
			 * section 7.1.3: second@example.org> */
			if (*inptr == '>') {
				if (mode != GMIME_RFC_COMPLIANCE_LOOSE)
					goto error;
				
				inptr++;
			}
			
			*address = _internet_address_mailbox_new (name, addrspec, at);
			g_free (addrspec);
			g_free (name);
			*in = inptr;
			
			return TRUE;
		}
	}
	
	if (*inptr == '<') {
		/* rfc2822 angle-addr token */
		const char *phrase = start;
		gboolean retval;
		char *name;
		
		if (trim_leading_quote) {
			phrase++;
			length--;
		}
		
		if (length > 0) {
			name = decode_name (options, phrase, length, charset, offset);
		} else {
			name = g_strdup ("");
		}
		
		retval = mailbox_parse (options, &inptr, name, address);
		g_free (name);
		*in = inptr;
		
		return retval;
	}
	
 error:
	if (g_mime_parser_options_get_warning_callback (options) != NULL)
		_g_mime_parser_options_warn (options, offset, GMIME_WARN_INVALID_ADDRESS_LIST, *in);
	
	*address = NULL;
	*in = inptr;
	
	return FALSE;
}

static gboolean
address_list_parse (InternetAddressList *list, GMimeParserOptions *options, const char **in, gboolean is_group, gint64 offset)
{
	gboolean can_warn = g_mime_parser_options_get_warning_callback (options) != NULL;
	InternetAddress *address;
	const char *charset;
	const char *inptr;
	
	if (!skip_cfws (in))
		return FALSE;
	
	inptr = *in;
	
	if (*inptr == '\0')
		return FALSE;
	
	while (*inptr) {
		gboolean separator_between_addrs = FALSE;

		if (is_group && *inptr ==  ';')
			break;
		
		charset = NULL;
		
		if (!address_parse (options, ALLOW_ANY, &inptr, &charset, &address, offset)) {
			/* skip this address... */
			while (*inptr && *inptr != ',' && (!is_group || *inptr != ';'))
				inptr++;
		} else {
			_internet_address_list_add (list, address);
			
			if (charset)
				address->charset = g_strdup (charset);

			if (INTERNET_ADDRESS_IS_GROUP(address))
				separator_between_addrs = TRUE;
		}
		
		/* Note: we loop here in case there are any null addresses between commas */
		do {
			if (!skip_cfws (&inptr)) {
				*in = inptr;
				
				return FALSE;
			}
			
			if (*inptr != ',')
				break;
			
			separator_between_addrs = TRUE;
			inptr++;
		} while (TRUE);
		
		if (can_warn && !(separator_between_addrs || (*inptr == '\0') || (is_group && *inptr == ';')))
			_g_mime_parser_options_warn (options, offset, GMIME_WARN_INVALID_ADDRESS_LIST, *in);
	}
	
	*in = inptr;
	
	return TRUE;
}


/**
 * internet_address_list_parse:
 * @options: (nullable): a #GMimeParserOptions or %NULL
 * @str: a string containing internet addresses
 *
 * Construct a list of internet addresses from the given string.
 *
 * Returns: (nullable) (transfer full): a #InternetAddressList or %NULL if the
 * input string does not contain any addresses.
 **/
InternetAddressList *
internet_address_list_parse (GMimeParserOptions *options, const char *str)
{
	return _internet_address_list_parse (options, str, -1);
}

InternetAddressList *
_internet_address_list_parse (GMimeParserOptions *options, const char *str, gint64 offset)
{
	InternetAddressList *list;
	const char *inptr = str;
	
	g_return_val_if_fail (str != NULL, NULL);
	
	list = internet_address_list_new ();
	if (!address_list_parse (list, options, &inptr, FALSE, offset) || list->array->len == 0) {
		g_object_unref (list);
		return NULL;
	}
	
	return list;
}
