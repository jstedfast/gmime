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

#define GMIME_TYPE_GROUP_ADDRESS            (g_mime_group_address_get_type ())

#define GMIME_TYPE_MAILBOX_ADDRESS          (g_mime_mailbox_address_get_type ())

#define GMIME_TYPE_INTERNET_ADDRESS_LIST             (g_mime_internet_address_list_get_type ())





typedef struct _GMimeInternetAddressList GMimeInternetAddressList;


/**
 * GMimeInternetAddress:
 *
 * An RFC 2822 Address object.
 **/
G_DECLARE_DERIVABLE_TYPE (GMimeInternetAddress, g_mime_internet_address, GMIME, INTERNET_ADDRESS, GObject)

struct _GMimeInternetAddressClass {
	GObjectClass parent_class;

	/* public virtual methods */
	void (* to_string) (GMimeInternetAddress *ia, GMimeFormatOptions *options, guint32 flags,
			    size_t *linelen, GString *str);
	GMimeInternetAddress * (* clone) (GMimeInternetAddress *ia);
	gboolean (* equal) (GMimeInternetAddress *ia, GMimeInternetAddress *other);

	/*< private >*/
	gpointer reserved[8];
};


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
 *
 * An RFC 2822 Mailbox address.
 **/
G_DECLARE_DERIVABLE_TYPE (GMimeMailboxAddress, g_mime_mailbox_address, GMIME, MAILBOX_ADDRESS, GMimeInternetAddress)

struct _GMimeMailboxAddressClass {
	GMimeInternetAddressClass parent_class;

	/*< private >*/
	gpointer reserved[8];
};


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
 *
 * An RFC 2822 Group address.
 **/
G_DECLARE_DERIVABLE_TYPE (GMimeGroupAddress, g_mime_group_address, GMIME, GROUP_ADDRESS, GMimeInternetAddress)

struct _GMimeGroupAddressClass {
	GMimeInternetAddressClass parent_class;

	/*< private >*/
	gpointer reserved[8];
};


GMimeInternetAddress *g_mime_group_address_new (const char *name);
GMimeInternetAddress *g_mime_group_address_parse (GMimeParserOptions *options, const char *str, gssize length, GError **error);

void g_mime_group_address_set_members (GMimeGroupAddress *group, GMimeInternetAddressList *members);
GMimeInternetAddressList *g_mime_group_address_get_members (GMimeGroupAddress *group);

int g_mime_group_address_add_member (GMimeGroupAddress *group, GMimeInternetAddress *member);


/**
 * GMimeInternetAddressList:
 *
 * A collection of #GMimeInternetAddress objects.
 **/
G_DECLARE_FINAL_TYPE (GMimeInternetAddressList, g_mime_internet_address_list, GMIME, INTERNET_ADDRESS_LIST, GObject)


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
