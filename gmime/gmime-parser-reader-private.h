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

/* Shared model builder for GMimeParser. All operations retain one reader's
 * buffered input. Stream replacement uses the inherited reader setters. */
GMimeReader *_g_mime_parser_reader_new (GMimeParserOptions *options, GMimeStream *stream, gboolean persist);
void _g_mime_parser_reader_set_persist (GMimeReader *reader, gboolean persist);
void _g_mime_parser_reader_set_header_callback (GMimeReader *reader,
					      gboolean (*callback) (GMimeHeader *, gboolean, gpointer, GError **), gpointer user_data);
gint64 _g_mime_parser_reader_get_headers_begin (GMimeReader *reader);
gint64 _g_mime_parser_reader_get_headers_end (GMimeReader *reader);
GMimeHeaderList *_g_mime_parser_reader_construct_headers (GMimeReader *reader, gboolean stop_at_separator, GError **error);
GMimeEntity *_g_mime_parser_reader_construct_part (GMimeReader *reader, GError **error);
GMimeMessage *_g_mime_parser_reader_construct_message (GMimeReader *reader, GError **error);
/* Borrowed opening envelope bytes, excluding CRLF, from the last successful
 * message operation. Cleared by the next construction operation, including
 * failures. MMDF closing separators do not replace the opening marker. */
GBytes *_g_mime_parser_reader_get_mbox_marker (GMimeReader *reader);
gint64 _g_mime_parser_reader_get_mbox_marker_offset (GMimeReader *reader);

G_END_DECLS

#endif /* __GMIME_PARSER_READER_PRIVATE_H__ */
