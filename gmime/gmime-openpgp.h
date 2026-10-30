/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* GMime
 * Copyright (C) 2026 Jeffrey Stedfast
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * as published by the Free Software Foundation; either version 2.1
 * of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * Lesser General Public License for more details.
 */

#ifndef __GMIME_OPENPGP_H__
#define __GMIME_OPENPGP_H__

#include <gio/gio.h>
#include <gmime/gmime-part.h>

G_BEGIN_DECLS

/**
 * GMimeOpenPGPBlockKind:
 * @GMIME_OPENPGP_BLOCK_MESSAGE: PGP MESSAGE armor, not necessarily encrypted.
 * @GMIME_OPENPGP_BLOCK_SIGNED_MESSAGE: Clear-signed content and its signature.
 * @GMIME_OPENPGP_BLOCK_PUBLIC_KEY: Public-key armor.
 * @GMIME_OPENPGP_BLOCK_PRIVATE_KEY: Private-key armor.
 * @GMIME_OPENPGP_BLOCK_SIGNATURE: Standalone signature armor.
 *
 * The observed armor type, not a cryptographic assertion.
 **/
typedef enum {
	GMIME_OPENPGP_BLOCK_MESSAGE,
	GMIME_OPENPGP_BLOCK_SIGNED_MESSAGE,
	GMIME_OPENPGP_BLOCK_PUBLIC_KEY,
	GMIME_OPENPGP_BLOCK_PRIVATE_KEY,
	GMIME_OPENPGP_BLOCK_SIGNATURE
} GMimeOpenPGPBlockKind;

/**
 * GMimeOpenPGPBlockStatus:
 * @GMIME_OPENPGP_BLOCK_COMPLETE: Matching markers were found.
 * @GMIME_OPENPGP_BLOCK_TRUNCATED: EOF before the expected marker.
 * @GMIME_OPENPGP_BLOCK_INTERRUPTED: Another block began before completion.
 * @GMIME_OPENPGP_BLOCK_MALFORMED: An unexpected end marker was found.
 *
 * Structural discovery status. COMPLETE does not validate armor, packets,
 * signatures, or trust.
 **/
typedef enum {
	GMIME_OPENPGP_BLOCK_COMPLETE,
	GMIME_OPENPGP_BLOCK_TRUNCATED,
	GMIME_OPENPGP_BLOCK_INTERRUPTED,
	GMIME_OPENPGP_BLOCK_MALFORMED
} GMimeOpenPGPBlockStatus;

#define GMIME_TYPE_OPENPGP_BLOCK (g_mime_openpgp_block_get_type ())
#define GMIME_TYPE_OPENPGP_DOCUMENT (g_mime_openpgp_document_get_type ())

/**
 * GMimeOpenPGPBlock:
 *
 * An immutable candidate block in a transfer-decoded snapshot.
 **/
G_DECLARE_FINAL_TYPE (GMimeOpenPGPBlock, g_mime_openpgp_block, GMIME, OPENPGP_BLOCK, GObject)

/**
 * GMimeOpenPGPDocument:
 *
 * An immutable decoded snapshot, including unprotected gaps. Implements
 * #GListModel with #GMimeOpenPGPBlock items in source order.
 **/
G_DECLARE_FINAL_TYPE (GMimeOpenPGPDocument, g_mime_openpgp_document, GMIME, OPENPGP_DOCUMENT, GObject)

GMimeOpenPGPDocument *g_mime_part_scan_openpgp (GMimePart *part, GError **error);

gint64 g_mime_openpgp_document_get_length (GMimeOpenPGPDocument *document);
guint g_mime_openpgp_document_get_count (GMimeOpenPGPDocument *document);
GMimeOpenPGPBlock *g_mime_openpgp_document_get_block (GMimeOpenPGPDocument *document, guint index);
GMimeStream *g_mime_openpgp_document_open_stream (GMimeOpenPGPDocument *document);

GMimeOpenPGPBlockKind g_mime_openpgp_block_get_kind (GMimeOpenPGPBlock *block);
GMimeOpenPGPBlockStatus g_mime_openpgp_block_get_status (GMimeOpenPGPBlock *block);
gint64 g_mime_openpgp_block_get_offset (GMimeOpenPGPBlock *block);
gint64 g_mime_openpgp_block_get_length (GMimeOpenPGPBlock *block);
GMimeStream *g_mime_openpgp_block_open_stream (GMimeOpenPGPBlock *block);

G_END_DECLS

#endif /* __GMIME_OPENPGP_H__ */
