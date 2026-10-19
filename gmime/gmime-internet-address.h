/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*  GMime
 *  Copyright (C) 2000-2022 Jeffrey Stedfast
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


#ifndef __GMIME_INTERNET_ADDRESS_H__
#define __GMIME_INTERNET_ADDRESS_H__

#include <glib.h>
#include <glib-object.h>

#include <gmime/gmime-format-options.h>
#include <gmime/gmime-parser-options.h>
#include <gmime/gmime-domain-list.h>

G_BEGIN_DECLS

#define GMIME_TYPE_INTERNET_ADDRESS                  (g_mime_internet_address_get_type ())
#define GMIME_INTERNET_ADDRESS(obj)                  (G_TYPE_CHECK_INSTANCE_CAST ((obj), g_mime_internet_address_get_type (), GMimeInternetAddress))
#define GMIME_INTERNET_ADDRESS_CLASS(klass)          (G_TYPE_CHECK_CLASS_CAST ((klass), g_mime_internet_address_get_type (), GMimeInternetAddressClass))
#define GMIME_IS_INTERNET_ADDRESS(obj)               (G_TYPE_CHECK_INSTANCE_TYPE ((obj), g_mime_internet_address_get_type ()))
#define GMIME_IS_INTERNET_ADDRESS_CLASS(klass)       (G_TYPE_CHECK_CLASS_TYPE ((klass), g_mime_internet_address_get_type ()))
#define GMIME_INTERNET_ADDRESS_GET_CLASS(obj)        (G_TYPE_INSTANCE_GET_CLASS ((obj), g_mime_internet_address_get_type (), GMimeInternetAddressClass))

#define GMIME_TYPE_GROUP_ADDRESS            (g_mime_group_address_get_type ())
#define GMIME_GROUP_ADDRESS(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), GMIME_TYPE_GROUP_ADDRESS, GMimeGroupAddress))
#define GMIME_GROUP_ADDRESS_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), GMIME_TYPE_GROUP_ADDRESS, GMimeGroupAddressClass))
#define GMIME_IS_GROUP_ADDRESS(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GMIME_TYPE_GROUP_ADDRESS))
#define GMIME_IS_GROUP_ADDRESS_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), GMIME_TYPE_GROUP_ADDRESS))

#define GMIME_TYPE_MAILBOX_ADDRESS          (g_mime_mailbox_address_get_type ())
#define GMIME_MAILBOX_ADDRESS(obj)          (G_TYPE_CHECK_INSTANCE_CAST ((obj), GMIME_TYPE_MAILBOX_ADDRESS, GMimeMailboxAddress))
#define GMIME_MAILBOX_ADDRESS_CLASS(klass)  (G_TYPE_CHECK_CLASS_CAST ((klass), GMIME_TYPE_MAILBOX_ADDRESS, GMimeMailboxAddressClass))
#define GMIME_IS_MAILBOX_ADDRESS(obj)       (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GMIME_TYPE_MAILBOX_ADDRESS))
#define GMIME_IS_MAILBOX_ADDRESS_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), GMIME_TYPE_MAILBOX_ADDRESS))

#define GMIME_TYPE_INTERNET_ADDRESS_LIST             (g_mime_internet_address_list_get_type ())
#define GMIME_INTERNET_ADDRESS_LIST(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), GMIME_TYPE_INTERNET_ADDRESS_LIST, GMimeInternetAddressList))
#define GMIME_INTERNET_ADDRESS_LIST_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), GMIME_TYPE_INTERNET_ADDRESS_LIST, GMimeInternetAddressListClass))
#define GMIME_IS_INTERNET_ADDRESS_LIST(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GMIME_TYPE_INTERNET_ADDRESS_LIST))
#define GMIME_IS_INTERNET_ADDRESS_LIST_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), GMIME_TYPE_INTERNET_ADDRESS_LIST))


typedef struct _GMimeInternetAddress GMimeInternetAddress;
typedef struct _GMimeInternetAddressClass GMimeInternetAddressClass;

typedef struct _GMimeGroupAddress GMimeGroupAddress;
typedef struct _GMimeGroupAddressClass GMimeGroupAddressClass;

typedef struct _GMimeMailboxAddress GMimeMailboxAddress;
typedef struct _GMimeMailboxAddressClass GMimeMailboxAddressClass;

typedef struct _GMimeInternetAddressList GMimeInternetAddressList;
typedef struct _GMimeInternetAddressListClass GMimeInternetAddressListClass;


/**
 * GMimeInternetAddress:
 * @parent_object: parent #GObject
 * @charset: the charset used for encoding the name
 * @name: display name
 *
 * An RFC 2822 Address object.
 **/
struct _GMimeInternetAddress {
	GObject parent_object;

	char *charset;
	char *name;

	/* < private > */
	gpointer changed;
};

struct _GMimeInternetAddressClass {
	GObjectClass parent_class;

	/* public virtual methods */
	void (* to_string) (GMimeInternetAddress *ia, GMimeFormatOptions *options, guint32 flags,
			    size_t *linelen, GString *str);
	GMimeInternetAddress * (* clone) (GMimeInternetAddress *ia);
	gboolean (* equal) (GMimeInternetAddress *ia, GMimeInternetAddress *other);
};


GType g_mime_internet_address_get_type (void);

void g_mime_internet_address_set_name (GMimeInternetAddress *ia, const char *name);
const char *g_mime_internet_address_get_name (GMimeInternetAddress *ia);

void g_mime_internet_address_set_charset (GMimeInternetAddress *ia, const char *charset);
const char *g_mime_internet_address_get_charset (GMimeInternetAddress *ia);

char *g_mime_internet_address_to_string (GMimeInternetAddress *ia, GMimeFormatOptions *options, gboolean encode);
GMimeInternetAddress *g_mime_internet_address_clone (GMimeInternetAddress *ia);
gboolean g_mime_internet_address_equal (GMimeInternetAddress *ia, GMimeInternetAddress *other);
int g_mime_internet_address_compare (GMimeInternetAddress *ia, GMimeInternetAddress *other);
GMimeInternetAddress *g_mime_internet_address_parse (GMimeParserOptions *options, const char *str, gssize length, GError **error);


/**
 * GMimeMailboxAddress:
 * @parent_object: parent #GMimeInternetAddress
 * @addrspec: the address string
 * @at: the index of the '@' character
 * @route: the source-route domains
 *
 * An RFC 2822 Mailbox address.
 **/
struct _GMimeMailboxAddress {
	GMimeInternetAddress parent_object;

	char *addrspec;
	int at;
	GMimeDomainList *route;
};

struct _GMimeMailboxAddressClass {
	GMimeInternetAddressClass parent_class;

};


GType g_mime_mailbox_address_get_type (void);

GMimeInternetAddress *g_mime_mailbox_address_new (const char *name, const char *addr, GError **error);

gboolean g_mime_mailbox_address_set_addrspec (GMimeMailboxAddress *mailbox, const char *addr, GError **error);
const char *g_mime_mailbox_address_get_addrspec (GMimeMailboxAddress *mailbox);
char *g_mime_mailbox_address_get_address (GMimeMailboxAddress *mailbox, gboolean idn_encode);
char *g_mime_mailbox_address_get_local_part (GMimeMailboxAddress *mailbox);
const char *g_mime_mailbox_address_get_raw_domain (GMimeMailboxAddress *mailbox);
char *g_mime_mailbox_address_get_domain (GMimeMailboxAddress *mailbox, gboolean idn_encode);
gboolean g_mime_mailbox_address_is_international (GMimeMailboxAddress *mailbox);
GMimeDomainList *g_mime_mailbox_address_get_route (GMimeMailboxAddress *mailbox);
char *g_mime_mailbox_address_encode_addrspec (const char *addr);
char *g_mime_mailbox_address_decode_addrspec (const char *addr);
GMimeInternetAddress *g_mime_mailbox_address_parse (GMimeParserOptions *options, const char *str, gssize length, GError **error);

/**
 * GMimeGroupAddress:
 * @parent_object: parent #GMimeInternetAddress
 * @members: a #GMimeInternetAddressList of group members
 *
 * An RFC 2822 Group address.
 **/
struct _GMimeGroupAddress {
	GMimeInternetAddress parent_object;

	GMimeInternetAddressList *members;
};

struct _GMimeGroupAddressClass {
	GMimeInternetAddressClass parent_class;

};


GType g_mime_group_address_get_type (void);

GMimeInternetAddress *g_mime_group_address_new (const char *name);
GMimeInternetAddress *g_mime_group_address_parse (GMimeParserOptions *options, const char *str, gssize length, GError **error);

void g_mime_group_address_set_members (GMimeGroupAddress *group, GMimeInternetAddressList *members);
GMimeInternetAddressList *g_mime_group_address_get_members (GMimeGroupAddress *group);

int g_mime_group_address_add_member (GMimeGroupAddress *group, GMimeInternetAddress *member);


/**
 * GMimeInternetAddressList:
 * @parent_object: parent #GObject
 * @array: The array of #GMimeInternetAddress objects.
 *
 * A collection of #GMimeInternetAddress objects.
 **/
struct _GMimeInternetAddressList {
	GObject parent_object;

	GPtrArray *array;

	/* < private > */
	gpointer changed;
};

struct _GMimeInternetAddressListClass {
	GObjectClass parent_class;

};


GType g_mime_internet_address_list_get_type (void);

GMimeInternetAddressList *g_mime_internet_address_list_new (void);
GMimeInternetAddressList *g_mime_internet_address_list_clone (GMimeInternetAddressList *list);
gboolean g_mime_internet_address_list_equal (GMimeInternetAddressList *list, GMimeInternetAddressList *other);
int g_mime_internet_address_list_compare (GMimeInternetAddressList *list, GMimeInternetAddressList *other);
GMimeInternetAddressList *g_mime_internet_address_list_get_mailboxes (GMimeInternetAddressList *list);

int g_mime_internet_address_list_length (GMimeInternetAddressList *list);

void g_mime_internet_address_list_clear (GMimeInternetAddressList *list);

int g_mime_internet_address_list_add (GMimeInternetAddressList *list, GMimeInternetAddress *ia);
void g_mime_internet_address_list_prepend (GMimeInternetAddressList *list, GMimeInternetAddressList *prepend);
void g_mime_internet_address_list_append (GMimeInternetAddressList *list, GMimeInternetAddressList *append);
void g_mime_internet_address_list_insert (GMimeInternetAddressList *list, int index, GMimeInternetAddress *ia);
gboolean g_mime_internet_address_list_remove (GMimeInternetAddressList *list, GMimeInternetAddress *ia);
gboolean g_mime_internet_address_list_remove_at (GMimeInternetAddressList *list, int index);

gboolean g_mime_internet_address_list_contains (GMimeInternetAddressList *list, GMimeInternetAddress *ia);
int g_mime_internet_address_list_index_of (GMimeInternetAddressList *list, GMimeInternetAddress *ia);

GMimeInternetAddress *g_mime_internet_address_list_get_address (GMimeInternetAddressList *list, int index);
void g_mime_internet_address_list_set_address (GMimeInternetAddressList *list, int index, GMimeInternetAddress *ia);

char *g_mime_internet_address_list_to_string (GMimeInternetAddressList *list, GMimeFormatOptions *options, gboolean encode);
void g_mime_internet_address_list_encode (GMimeInternetAddressList *list, GMimeFormatOptions *options, GString *str);

GMimeInternetAddressList *g_mime_internet_address_list_parse (GMimeParserOptions *options, const char *str);
GMimeInternetAddressList *g_mime_internet_address_list_parse_full (GMimeParserOptions *options, const char *str,
								gssize length, GError **error);
void g_mime_internet_address_list_append_parse (GMimeInternetAddressList *list, GMimeParserOptions *options, const char *str);

G_END_DECLS

#endif /* __GMIME_INTERNET_ADDRESS_H__ */
