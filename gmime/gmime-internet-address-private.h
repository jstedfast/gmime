/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*  GMime
 *  Copyright (C) 2026 Jeffrey Stedfast
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

#ifndef __GMIME_INTERNET_ADDRESS_PRIVATE_H__
#define __GMIME_INTERNET_ADDRESS_PRIVATE_H__

#include "gmime-internet-address.h"

G_BEGIN_DECLS

G_GNUC_INTERNAL size_t _g_mime_internet_address_text_length (const char *text, size_t length, GMimeFormatOptions *options);

enum {
	GMIME_INTERNET_ADDRESS_ENCODE = 1 << 0,
	GMIME_INTERNET_ADDRESS_FOLD   = 1 << 1,
	GMIME_INTERNET_ADDRESS_FIRST  = 1 << 2,
};

G_GNUC_INTERNAL gboolean _g_mime_internet_address_set_name (GMimeInternetAddress *ia, const char *name);
G_GNUC_INTERNAL char *_g_mime_internet_address_encode_domain (const char *domain);
G_GNUC_INTERNAL char *_g_mime_internet_address_decode_domain (const char *domain);
G_GNUC_INTERNAL GMimeDomainList *_g_mime_domain_list_parse (const char **in);
G_GNUC_INTERNAL char *_g_mime_internet_address_copy_text (const char *str, gssize length, GError **error);
G_GNUC_INTERNAL GMimeInternetAddress *_g_mime_internet_address_parse (GMimeParserOptions *options, const char *str,
								    gssize length, GType type, GError **error);
G_GNUC_INTERNAL char *_g_mime_internet_address_encode_name (GMimeFormatOptions *options, const char *raw,
							  gboolean rfc2047_encode, const char *charset);
G_GNUC_INTERNAL void _g_mime_internet_address_linewrap (GString *str, const char *newline);
G_GNUC_INTERNAL void _g_mime_internet_address_append_name (GString *str, size_t *linelen, const char *name,
							 GMimeFormatOptions *options, guint32 flags);
G_GNUC_INTERNAL void _g_mime_internet_address_list_to_string (const GMimeInternetAddressList *list,
							    GMimeFormatOptions *options, guint32 flags,
							    size_t *linelen, GString *str);
G_GNUC_INTERNAL GMimeInternetAddress *_g_mime_mailbox_address_new (const char *name, const char *addr, int at,
								 GMimeDomainList *route);
G_GNUC_INTERNAL char *_g_mime_mailbox_address_get_hashable_addrspec (GMimeMailboxAddress *mailbox);
G_GNUC_INTERNAL gboolean _g_mime_internet_address_parse_addrspec (const char **in, const char *sentinels,
								GMimeRfcComplianceMode mode, char **addrspec, int *at);

G_END_DECLS

#endif /* __GMIME_INTERNET_ADDRESS_PRIVATE_H__ */
