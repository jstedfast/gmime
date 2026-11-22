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

#ifndef GMIME_ARC_H
#define GMIME_ARC_H

#include <gmime/gmime-dkim.h>

G_BEGIN_DECLS

/**
 * GMimeArcChainStatus:
 * @GMIME_ARC_CHAIN_NONE: No ARC fields were present.
 * @GMIME_ARC_CHAIN_PASS: The latest AMS and every seal verified.
 * @GMIME_ARC_CHAIN_FAIL: The chain did not verify.
 *
 * A passing chain authenticates custody, not the sender, the truth of an
 * assessment, or the trustworthiness of a custodian.
 */
typedef enum {
	GMIME_ARC_CHAIN_NONE,
	GMIME_ARC_CHAIN_PASS,
	GMIME_ARC_CHAIN_FAIL
} GMimeArcChainStatus;

GMimeArcChainStatus g_mime_arc_verify (GMimeMessage *message, GResolver *resolver,
				       GCancellable *cancellable, GError **error);

gboolean g_mime_arc_seal (GMimeMessage *message, GMimeArcChainStatus received_status,
			 const char *authentication_results, const char *domain, const char *selector,
			 GMimeDkimAlgorithm algorithm, GBytes *private_key,
			 GMimeDkimCanonicalization header_canon, GMimeDkimCanonicalization body_canon,
			 const char * const *headers, GError **error);

G_END_DECLS

#endif
