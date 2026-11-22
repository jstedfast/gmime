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

#ifndef GMIME_DKIM_H
#define GMIME_DKIM_H

#include <gio/gio.h>
#include <gmime/gmime-message.h>

G_BEGIN_DECLS

/**
 * GMimeDkimAlgorithm:
 * @GMIME_DKIM_ALGORITHM_RSA_SHA256: RSA PKCS#1 v1.5 with SHA-256.
 * @GMIME_DKIM_ALGORITHM_ED25519_SHA256: Ed25519 over a SHA-256 digest.
 *
 * Signature algorithms supported by DKIM and ARC. RFC 8301 excludes SHA-1.
 */
typedef enum {
	GMIME_DKIM_ALGORITHM_RSA_SHA256,
	GMIME_DKIM_ALGORITHM_ED25519_SHA256
} GMimeDkimAlgorithm;

/**
 * GMimeDkimCanonicalization:
 * @GMIME_DKIM_CANONICALIZATION_SIMPLE: Preserve header bytes or body line content.
 * @GMIME_DKIM_CANONICALIZATION_RELAXED: Normalize whitespace as specified in RFC 6376.
 *
 * Header and body canonicalization are selected independently.
 */
typedef enum {
	GMIME_DKIM_CANONICALIZATION_SIMPLE,
	GMIME_DKIM_CANONICALIZATION_RELAXED
} GMimeDkimCanonicalization;

gboolean g_mime_dkim_sign (GMimeMessage *message, const char *domain, const char *selector,
			  GMimeDkimAlgorithm algorithm, GBytes *private_key,
			  GMimeDkimCanonicalization header_canon, GMimeDkimCanonicalization body_canon,
			  const char * const *headers, GError **error);

gboolean g_mime_dkim_verify (GMimeMessage *message, guint signature_index, GResolver *resolver,
			     GCancellable *cancellable, GError **error);
gboolean g_mime_dkim_verify_full (GMimeMessage *message, guint signature_index, GResolver *resolver,
				  GCancellable *cancellable, char **domain, char **selector, GError **error);

G_END_DECLS
#endif
