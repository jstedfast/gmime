/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 *  Authors: Jeffrey Stedfast <fejj@helixcode.com>
 *
 *  Copyright 2000 Helix Code, Inc. (www.helixcode.com)
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

#ifndef __INTERNET_ADDRESS_H__
#define __INTERNET_ADDRESS_H__

#ifdef __cplusplus
extern "C" {
#pragma }
#endif /* __cplusplus }*/

#include <glib.h>

typedef enum {
	INTERNET_ADDRESS_NONE,
	INTERNET_ADDRESS_NAME,
	INTERNET_ADDRESS_GROUP
} InternetAddressType;

struct _InternetAddress {
	InternetAddressType type;
	char *name;
	union {
		char *addr;
		GList *members;
	} value;
};

typedef struct _InternetAddress InternetAddress;

InternetAddress *internet_address_new (void);
InternetAddress *internet_address_new_name (const char *name, const char *addr);
InternetAddress *internet_address_new_group (const char *name);

void internet_address_destroy (InternetAddress *ia);

void internet_address_set_name (InternetAddress *ia, const char *name);
void internet_address_set_addr (InternetAddress *ia, const char *addr);
void internet_address_set_group (InternetAddress *ia, GList *group);
void internet_address_add_member (InternetAddress *ia, InternetAddress *member);

GList *internet_address_parse_string (const char *string);

char *internet_address_to_string (InternetAddress *ia, gboolean encode);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* __INTERNET_ADDRESS_H__ */
