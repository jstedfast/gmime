/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*  GMime
 *  Copyright (C) 2000-2014 Jeffrey Stedfast
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

#include <gmime/gmime-object.h>
#include <gmime/gmime-message.h>
#include <gmime/gmime-content-type.h>
#include <gmime/gmime-stream.h>

G_BEGIN_DECLS

#define GMIME_TYPE_PARSER            (g_mime_parser_get_type ())
#define GMIME_PARSER(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), GMIME_TYPE_PARSER, GMimeParser))
#define GMIME_PARSER_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), GMIME_TYPE_PARSER, GMimeParserClass))
#define GMIME_IS_PARSER(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GMIME_TYPE_PARSER))
#define GMIME_IS_PARSER_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), GMIME_TYPE_PARSER))
#define GMIME_PARSER_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), GMIME_TYPE_PARSER, GMimeParserClass))

typedef struct _GMimeParser GMimeParser;
typedef struct _GMimeParserClass GMimeParserClass;


/**
 * GMimeParser:
 * @parent_object: parent #GObject
 * @priv: private parser state
 *
 * A MIME parser context.
 **/
struct _GMimeParser {
	GObject parent_object;
	
	struct _GMimeParserPrivate *priv;
};

struct _GMimeParserClass {
	GObjectClass parent_class;
	
};


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


GType g_mime_parser_get_type (void);

GMimeParser *g_mime_parser_new (void);
GMimeParser *g_mime_parser_new_with_stream (GMimeStream *stream);

void g_mime_parser_init_with_stream (GMimeParser *parser, GMimeStream *stream);

gboolean g_mime_parser_get_persist_stream (GMimeParser *parser);
void g_mime_parser_set_persist_stream (GMimeParser *parser, gboolean persist);

gboolean g_mime_parser_get_scan_from (GMimeParser *parser);
void g_mime_parser_set_scan_from (GMimeParser *parser, gboolean scan_from);

gboolean g_mime_parser_get_respect_content_length (GMimeParser *parser);
void g_mime_parser_set_respect_content_length (GMimeParser *parser, gboolean respect_content_length);

void g_mime_parser_set_header_regex (GMimeParser *parser, const char *regex,
				     GMimeParserHeaderRegexFunc header_cb,
				     gpointer user_data);

GMimeObject *g_mime_parser_construct_part (GMimeParser *parser);

GMimeMessage *g_mime_parser_construct_message (GMimeParser *parser);

gint64 g_mime_parser_tell (GMimeParser *parser);

gboolean g_mime_parser_eos (GMimeParser *parser);

char *g_mime_parser_get_from (GMimeParser *parser);

gint64 g_mime_parser_get_from_offset (GMimeParser *parser);
gint64 g_mime_parser_get_headers_begin (GMimeParser *parser);
gint64 g_mime_parser_get_headers_end (GMimeParser *parser);

G_END_DECLS

#endif /* __GMIME_PARSER_H__ */
