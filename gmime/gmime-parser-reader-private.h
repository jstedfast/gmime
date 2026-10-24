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

#ifndef __GMIME_PARSER_READER_PRIVATE_H__
#define __GMIME_PARSER_READER_PRIVATE_H__

#include "gmime-header.h"
#include "gmime-entity.h"
#include "gmime-message.h"
#include "gmime-reader.h"

G_BEGIN_DECLS

/* Private model-building foundation. Do not mix this reader's buffered
 * operations with a legacy parser on the same input stream. Header lists,
 * MIME trees and sequential message/mailbox models are supported. Stream
 * replacement uses the inherited reader setters. */
GMimeReader *_g_mime_parser_reader_new (GMimeParserOptions *options, GMimeStream *stream, gboolean persist);
GMimeHeaderList *_g_mime_parser_reader_construct_headers (GMimeReader *reader, GError **error);
GMimeEntity *_g_mime_parser_reader_construct_part (GMimeReader *reader, GError **error);
GMimeMessage *_g_mime_parser_reader_construct_message (GMimeReader *reader, GError **error);
/* Borrowed opening envelope bytes, excluding CRLF, from the last successful
 * message operation. Cleared by the next construction operation, including
 * failures. MMDF closing separators do not replace the opening marker. */
GBytes *_g_mime_parser_reader_get_mbox_marker (GMimeReader *reader);
gint64 _g_mime_parser_reader_get_mbox_marker_offset (GMimeReader *reader);

G_END_DECLS

#endif /* __GMIME_PARSER_READER_PRIVATE_H__ */
