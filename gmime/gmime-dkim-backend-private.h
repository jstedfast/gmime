/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*  GMime
 *  Copyright (C) Jeffrey Stedfast
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

#ifndef GMIME_DKIM_BACKEND_PRIVATE_H
#define GMIME_DKIM_BACKEND_PRIVATE_H

#include <gio/gio.h>

G_GNUC_INTERNAL GBytes *_g_mime_dkim_sign_digest (gboolean ed25519, GBytes *pem,
						  const guint8 digest[32], GError **error);
G_GNUC_INTERNAL gboolean _g_mime_dkim_verify_digest (gboolean ed25519, GBytes *key,
						     const guint8 digest[32], GBytes *signature, GError **error);
G_GNUC_INTERNAL GBytes *_g_mime_dkim_lookup_key (GResolver *resolver, const char *domain, const char *selector,
						 gboolean ed25519, GCancellable *cancellable, GError **error);
G_GNUC_INTERNAL GBytes *_g_mime_dkim_lookup_key_full (GResolver *resolver, const char *domain, const char *selector,
						      gboolean ed25519, gboolean subdomain_identity,
						      GCancellable *cancellable, GError **error);

#endif
