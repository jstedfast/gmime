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


#ifndef __GMIME_CONTENT_H__
#define __GMIME_CONTENT_H__

#include <glib.h>
#include <glib-object.h>

#include <gmime/gmime-content-type.h>
#include <gmime/gmime-encodings.h>
#include <gmime/gmime-format-options.h>
#include <gmime/gmime-stream.h>
#include <gmime/gmime-utils.h>

G_BEGIN_DECLS

#define GMIME_TYPE_CONTENT            (g_mime_content_get_type ())
#define GMIME_CONTENT(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), GMIME_TYPE_CONTENT, GMimeContent))
#define GMIME_CONTENT_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), GMIME_TYPE_CONTENT, GMimeContentClass))
#define GMIME_IS_CONTENT(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GMIME_TYPE_CONTENT))
#define GMIME_IS_CONTENT_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), GMIME_TYPE_CONTENT))
#define GMIME_CONTENT_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), GMIME_TYPE_CONTENT, GMimeContentClass))

typedef struct _GMimeContent GMimeContent;
typedef struct _GMimeContentClass GMimeContentClass;


/**
 * GMimeContent:
 * @parent_object: parent #GObject
 * @encoding: the encoding of the content
 * @stream: content stream
 *
 * A wrapper for a stream which may be encoded.
 **/
struct _GMimeContent {
	GObject parent_object;

	GMimeContentEncoding encoding;
	GMimeStream *stream;

	/* <private> */
	GMimeNewLineFormat newline_format;
	gboolean has_newline_format;
};

struct _GMimeContentClass {
	GObjectClass parent_class;

	ssize_t (* write_to_stream) (GMimeContent *content, GMimeStream *stream);
	ssize_t (* decode_to_stream) (GMimeContent *content, GMimeStream *stream);
};


GType g_mime_content_get_type (void);

GMimeContent *g_mime_content_new (void);
GMimeContent *g_mime_content_new_with_stream (GMimeStream *stream, GMimeContentEncoding encoding);

void g_mime_content_set_stream (GMimeContent *content, GMimeStream *stream);
GMimeStream *g_mime_content_get_stream (GMimeContent *content);

void g_mime_content_set_encoding (GMimeContent *content, GMimeContentEncoding encoding);
GMimeContentEncoding g_mime_content_get_encoding (GMimeContent *content);

gboolean g_mime_content_get_newline_format (GMimeContent *content, GMimeNewLineFormat *format);
void g_mime_content_set_newline_format (GMimeContent *content, GMimeNewLineFormat format);
void g_mime_content_clear_newline_format (GMimeContent *content);

GMimeStream *g_mime_content_open (GMimeContent *content);

ssize_t g_mime_content_write_to_stream (GMimeContent *content, GMimeStream *stream);
ssize_t g_mime_content_decode_to_stream (GMimeContent *content, GMimeStream *stream);

G_END_DECLS

#endif /* __GMIME_CONTENT_H__ */
