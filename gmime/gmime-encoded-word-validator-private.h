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
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library; if not, write to the Free
 *  Software Foundation, 51 Franklin Street, Fifth Floor, Boston, MA
 *  02110-1301, USA.
 */

#ifndef __GMIME_ENCODED_WORD_VALIDATOR_PRIVATE_H__
#define __GMIME_ENCODED_WORD_VALIDATOR_PRIVATE_H__

#include "gmime-compliance.h"

G_BEGIN_DECLS

typedef enum {
	GMIME_ENCODED_WORD_CONTEXT_TEXT,
	GMIME_ENCODED_WORD_CONTEXT_PHRASE,
	GMIME_ENCODED_WORD_CONTEXT_COMMENT,
	GMIME_ENCODED_WORD_CONTEXT_FORBIDDEN
} GMimeEncodedWordContext;

typedef struct {
	const guint8 *text;
	gsize length, index, scan_index, comment_depth;
	GMimeComplianceIssueFunc report;
	gpointer user_data;
	GMimeComplianceContext context;
	gint64 stream_offset;
	guint64 line, column;
	gboolean quoted, literal;
} GMimeEncodedWordValidator;

/* Borrows the buffer and callback data. Coordinates identify buffer[0].
 * The initial marker search spans the full buffer. Forward-only ranges may
 * inspect a candidate's next 75 bytes; a complete word can advance past a range
 * end. Coordinates are tracked only when reporting. No decoding or allocation. */
G_GNUC_INTERNAL void _g_mime_encoded_word_validator_init (GMimeEncodedWordValidator *validator,
	const guint8 *buffer, gsize length, GMimeComplianceIssueFunc report, gpointer user_data,
	GMimeComplianceContext context, gint64 stream_offset, guint64 line, guint64 column);
G_GNUC_INTERNAL void _g_mime_encoded_word_validator_validate (GMimeEncodedWordValidator *validator,
	gsize end, GMimeEncodedWordContext location);

G_END_DECLS

#endif /* __GMIME_ENCODED_WORD_VALIDATOR_PRIVATE_H__ */
