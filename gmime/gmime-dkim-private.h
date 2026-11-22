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

#ifndef GMIME_DKIM_PRIVATE_H
#define GMIME_DKIM_PRIVATE_H

#include "gmime-dkim.h"

G_GNUC_INTERNAL GPtrArray *_g_mime_dkim_get_headers (GMimeMessage *message, GError **error);
G_GNUC_INTERNAL GMimeHeader *_g_mime_dkim_create_header (const char *field, GError **error);
G_GNUC_INTERNAL GHashTable *_g_mime_dkim_parse_tags (const char *raw, GError **error);
G_GNUC_INTERNAL GBytes *_g_mime_dkim_decode_base64 (const char *text);
G_GNUC_INTERNAL gboolean _g_mime_dkim_valid_domain (const char *text, gboolean selector);
G_GNUC_INTERNAL char *_g_mime_dkim_strip_b (const char *raw);
G_GNUC_INTERNAL void _g_mime_dkim_hash_header (GChecksum *sum, const char *raw, gboolean relaxed, gboolean crlf);

/* Write body chunks, then finish the digest once. */
G_GNUC_INTERNAL GMimeStream *_g_mime_dkim_body_stream_new (gboolean relaxed, guint64 remaining);
G_GNUC_INTERNAL gboolean _g_mime_dkim_body_stream_finish (GMimeStream *stream, gboolean limited, guint8 digest[32], GError **error);

/* instance==0: DKIM; otherwise AMS. Returned header excludes final CRLF. */
G_GNUC_INTERNAL char *_g_mime_dkim_sign_header (GMimeMessage *message, GPtrArray *fields, guint instance, const char *domain,
			       const char *selector, GMimeDkimAlgorithm algorithm, GBytes *private_key,
			       GMimeDkimCanonicalization header_canon, GMimeDkimCanonicalization body_canon,
			       const char * const *headers, GError **error);

G_GNUC_INTERNAL gboolean _g_mime_dkim_verify_header (GMimeMessage *message, GPtrArray *fields, guint header_index, gboolean ams,
				   GResolver *resolver, GCancellable *cancellable, GError **error);

#endif
