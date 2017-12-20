/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*  GMime Autocrypt
 *  Copyright (C) 2017 Daniel Kahn Gillmor
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


#ifndef __GMIME_AUTOCRYPT_H__
#define __GMIME_AUTOCRYPT_H__

#include <glib.h>
#include <glib/gprintf.h>
#include <glib-object.h>
#include <gmime/internet-address.h>

G_BEGIN_DECLS

#define GMIME_TYPE_AUTOCRYPT_HEADER                  (g_mime_autocrypt_header_get_type ())
#define GMIME_AUTOCRYPT_HEADER(obj)                  (G_TYPE_CHECK_INSTANCE_CAST ((obj), GMIME_TYPE_AUTOCRYPT_HEADER, GMimeAutocryptHeader))
#define GMIME_AUTOCRYPT_HEADER_CLASS(klass)          (G_TYPE_CHECK_CLASS_CAST ((klass), GMIME_TYPE_AUTOCRYPT_HEADER, GMimeAutocryptHeaderClass))
#define GMIME_IS_AUTOCRYPT_HEADER(obj)               (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GMIME_TYPE_AUTOCRYPT_HEADER))
#define GMIME_IS_AUTOCRYPT_HEADER_CLASS(klass)       (G_TYPE_CHECK_CLASS_TYPE ((klass), GMIME_TYPE_AUTOCRYPT_HEADER))
#define GMIME_AUTOCRYPT_HEADER_GET_CLASS(obj)        (G_TYPE_INSTANCE_GET_CLASS ((obj), GMIME_TYPE_AUTOCRYPT_HEADER, GMimeAutocryptHeaderClass))


#define GMIME_TYPE_AUTOCRYPT_HEADER_LIST             (g_mime_autocrypt_header_list_get_type ())
#define GMIME_AUTOCRYPT_HEADER_LIST(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), GMIME_TYPE_AUTOCRYPT_HEADER_LIST, GMimeAutocryptHeaderList))
#define GMIME_AUTOCRYPT_HEADER_LIST_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), GMIME_TYPE_AUTOCRYPT_HEADER_LIST, GMimeAutocryptHeaderListClass))
#define GMIME_IS_AUTOCRYPT_HEADER_LIST(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GMIME_TYPE_AUTOCRYPT_HEADER_LIST))
#define GMIME_IS_AUTOCRYPT_HEADER_LIST_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), GMIME_TYPE_AUTOCRYPT_HEADER_LIST))
#define GMIME_AUTOCRYPT_HEADER_LIST_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), GMIME_TYPE_AUTOCRYPT_HEADER_LIST, GMimeAutocryptHeaderListClass))

typedef struct _GMimeAutocryptHeader GMimeAutocryptHeader;
typedef struct _GMimeAutocryptHeaderClass GMimeAutocryptHeaderClass;

typedef struct _GMimeAutocryptHeaderList GMimeAutocryptHeaderList;
typedef struct _GMimeAutocryptHeaderListClass GMimeAutocryptHeaderListClass;

/**
 * GMimeAutocryptPreferEncrypt:
 * @GMIME_AUTOCRYPT_PREFER_ENCRYPT_NONE: No preference stated.
 * @GMIME_AUTOCRYPT_PREFER_ENCRYPT_MUTUAL: Please encrypt, if you also have this preference
 *
 * A description of the user's preference for encrypted messaging.
 **/
typedef enum {
	GMIME_AUTOCRYPT_PREFER_ENCRYPT_NONE     = 0,
	GMIME_AUTOCRYPT_PREFER_ENCRYPT_MUTUAL   = 1
} GMimeAutocryptPreferEncrypt;

/**
 * GMimeAutocryptHeader:
 * @parent_object: parent #GObject
 * @address: the #InternetAddressMailbox associated with this Autocrypt header.
 * @prefer_encrypt: a #GMimeAutocryptPreferEncrypt value (defaults to @GMIME_AUTOCRYPT_PREFER_ENCRYPT_NONE).
 * @keydata: the raw binary form of the encoded key.
 * @effective_date: the date associated with the Autocrypt header in this message.
 *
 * An object containing Autocrypt information about a given e-mail
 * address, as derived from a message header.
 *
 * See https://autocrypt.org/ for details and motivation.
 **/
struct _GMimeAutocryptHeader {
	GObject parent_object;

	InternetAddressMailbox *address;
	GMimeAutocryptPreferEncrypt prefer_encrypt;
	GBytes *keydata;
	GDateTime *effective_date;
};

struct _GMimeAutocryptHeaderClass {
	GObjectClass parent_class;
};

GType g_mime_autocrypt_header_get_type (void);

GMimeAutocryptHeader *g_mime_autocrypt_header_new (void);
GMimeAutocryptHeader *g_mime_autocrypt_header_new_from_string (const char *string);

void g_mime_autocrypt_header_set_address (GMimeAutocryptHeader *ah, InternetAddressMailbox *address);
InternetAddressMailbox *g_mime_autocrypt_header_get_address (GMimeAutocryptHeader *ah);
void g_mime_autocrypt_header_set_address_from_string (GMimeAutocryptHeader *ah, const char *address);
const char *g_mime_autocrypt_header_get_address_as_string (GMimeAutocryptHeader *ah);

void g_mime_autocrypt_header_set_prefer_encrypt (GMimeAutocryptHeader *ah, GMimeAutocryptPreferEncrypt pref);
GMimeAutocryptPreferEncrypt g_mime_autocrypt_header_get_prefer_encrypt (GMimeAutocryptHeader *ah);

void g_mime_autocrypt_header_set_keydata (GMimeAutocryptHeader *ah, GBytes *data);
GBytes *g_mime_autocrypt_header_get_keydata (GMimeAutocryptHeader *ah);

void g_mime_autocrypt_header_set_effective_date (GMimeAutocryptHeader *ah, GDateTime *effective_date);
GDateTime *g_mime_autocrypt_header_get_effective_date (GMimeAutocryptHeader *ah);

char *g_mime_autocrypt_header_to_string (GMimeAutocryptHeader *ah, gboolean gossip);
gboolean g_mime_autocrypt_header_is_complete (GMimeAutocryptHeader *ah);

int g_mime_autocrypt_header_compare (GMimeAutocryptHeader *ah1, GMimeAutocryptHeader *ah2);
void g_mime_autocrypt_header_clone (GMimeAutocryptHeader *dst, GMimeAutocryptHeader *src);


/**
 * GMimeAutocryptHeaderList:
 *
 * A list of Autocrypt headers, typically extracted from a GMimeMessage.
 **/
struct _GMimeAutocryptHeaderList {
	GObject parent_object;
	
	/* < private > */
	GPtrArray *array;
};

struct _GMimeAutocryptHeaderListClass {
	GObjectClass parent_class;
	
};

GType g_mime_autocrypt_header_list_get_type (void);

GMimeAutocryptHeaderList *g_mime_autocrypt_header_list_new (void);
guint g_mime_autocrypt_header_list_add_missing_addresses (GMimeAutocryptHeaderList *list, InternetAddressList *addresses);
void g_mime_autocrypt_header_list_add (GMimeAutocryptHeaderList *list, GMimeAutocryptHeader *header);

guint g_mime_autocrypt_header_list_get_count (GMimeAutocryptHeaderList *list);
GMimeAutocryptHeader *g_mime_autocrypt_header_list_get_header_at (GMimeAutocryptHeaderList *list, guint index);
GMimeAutocryptHeader *g_mime_autocrypt_header_list_get_header_for_address (GMimeAutocryptHeaderList *list, InternetAddressMailbox *mailbox);
void g_mime_autocrypt_header_list_remove_incomplete (GMimeAutocryptHeaderList *list);


G_END_DECLS

#endif /* __GMIME_AUTOCRYPT_H__ */
