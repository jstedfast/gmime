/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*  GMime
 *  Copyright (C) 2000-2022 Jeffrey Stedfast
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


#ifndef __GMIME_PARSER_H__
#define __GMIME_PARSER_H__

#include <glib.h>
#include <glib-object.h>
#include <errno.h>

#include <gmime/gmime-entity.h>
#include <gmime/gmime-message.h>
#include <gmime/gmime-content-type.h>
#include <gmime/gmime-header.h>
#include <gmime/gmime-parser-options.h>
#include <gmime/gmime-stream.h>

G_BEGIN_DECLS

#define GMIME_TYPE_PARSER            (g_mime_parser_get_type ())



/**
 * GMimeParser:
 *
 * A MIME parser context.
 **/
G_DECLARE_FINAL_TYPE (GMimeParser, g_mime_parser, GMIME, PARSER, GObject)


/**
 * GMimeParserHeaderRegexFunc:
 * @parser: The #GMimeParser object.
 * @header: The header field matched.
 * @value: The header field value.
 * @offset: The header field offset.
 * @user_data: The user-supplied callback data.
 *
 * Function signature for the callback to
 * g_mime_parser_set_header_regex().
 **/
typedef void (* GMimeParserHeaderRegexFunc) (GMimeParser *parser, const char *header,
					     const char *value, gint64 offset,
					     gpointer user_data);


GMimeParser *g_mime_parser_new (void);
GMimeParser *g_mime_parser_new_with_stream (GMimeStream *stream);
GMimeParser *g_mime_parser_new_with_stream_and_storage (GMimeStream *stream, GMimeFormat format,
						      GMimeContentStorage storage, GError **error);

void g_mime_parser_init_with_stream (GMimeParser *parser, GMimeStream *stream);
gboolean g_mime_parser_init_with_stream_and_storage (GMimeParser *parser, GMimeStream *stream,
						    GMimeFormat format, GMimeContentStorage storage, GError **error);

gboolean g_mime_parser_get_persist_stream (GMimeParser *parser);
void g_mime_parser_set_persist_stream (GMimeParser *parser, gboolean persist);

GMimeFormat g_mime_parser_get_format (GMimeParser *parser);
void g_mime_parser_set_format (GMimeParser *parser, GMimeFormat format);

void g_mime_parser_set_header_regex (GMimeParser *parser, const char *regex,
				     GMimeParserHeaderRegexFunc header_cb,
				     gpointer user_data);

GMimeHeaderList *g_mime_parser_construct_headers (GMimeParser *parser, GMimeParserOptions *options, GError **error);
GMimeEntity *g_mime_parser_construct_part (GMimeParser *parser, GMimeParserOptions *options, GError **error);
GMimeMessage *g_mime_parser_construct_message (GMimeParser *parser, GMimeParserOptions *options, GError **error);

gint64 g_mime_parser_tell (GMimeParser *parser);

gboolean g_mime_parser_eos (GMimeParser *parser);

char *g_mime_parser_get_mbox_marker (GMimeParser *parser);
gint64 g_mime_parser_get_mbox_marker_offset (GMimeParser *parser);

gint64 g_mime_parser_get_headers_begin (GMimeParser *parser);
gint64 g_mime_parser_get_headers_end (GMimeParser *parser);

G_END_DECLS

#endif /* __GMIME_PARSER_H__ */
