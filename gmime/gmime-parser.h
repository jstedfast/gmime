/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 *  Authors: Jeffrey Stedfast <fejj@ximian.com>
 *
 *  Copyright 2002 Ximain, Inc. (www.ximian.com)
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Street #330, Boston, MA 02111-1307, USA.
 *
 */


#ifndef __GMIME_PARSER_H__
#define __GMIME_PARSER_H__

#ifdef __cplusplus
extern "C" {
#pragma }
#endif /* __cplusplus */

#include <glib.h>
#include <glib-object.h>
#include <errno.h>

#include "gmime-object.h"
#include "gmime-message.h"
#include "gmime-content-type.h"
#include "gmime-stream.h"

#define GMIME_TYPE_PARSER            (g_mime_parser_get_type ())
#define GMIME_PARSER(obj)            (GMIME_CHECK_CAST ((obj), GMIME_TYPE_PARSER, GMimeParser))
#define GMIME_PARSER_CLASS(klass)    (GMIME_CHECK_CLASS_CAST ((klass), GMIME_TYPE_PARSER, GMimeParserClass))
#define GMIME_IS_PARSER(obj)         (GMIME_CHECK_TYPE ((obj), GMIME_TYPE_PARSER))
#define GMIME_IS_PARSER_CLASS(klass) (GMIME_CHECK_CLASS_TYPE ((klass), GMIME_TYPE_PARSER))
#define GMIME_PARSER_GET_CLASS(obj)  (GMIME_CHECK_GET_CLASS ((obj), GMIME_TYPE_PARSER, GMimeParserClass))

typedef struct _GMimeParser GMimeParser;
typedef struct _GMimeParserClass GMimeParserClass;

struct _GMimeParser {
	GObject parent_object;
	
	struct _GMimeParserPrivate *priv;
};

struct _GMimeParserClass {
	GObjectClass parent_class;
	
};


GType g_mime_parser_get_type (void);

GMimeParser *g_mime_parser_new (void);

void g_mime_parser_init_with_stream (GMimeParser *parser, GMimeStream *stream);


GMimeObject *g_mime_parser_construct_part (GMimeParser *parser);

GMimeMessage *g_mime_parser_construct_message (GMimeParser *parser);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* __GMIME_PARSER_H__ */

