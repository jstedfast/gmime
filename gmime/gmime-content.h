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


/**
 * GMimeContentStorage:
 * @GMIME_CONTENT_STORAGE_MEMORY: Copy MIME part content into memory.
 * @GMIME_CONTENT_STORAGE_SOURCE: Reference content in the seekable input.
 *
 * Storage for parsed MIME part content. The caller owns the input stream
 * in both modes. Memory storage allows it to be closed or modified after
 * parsing. Source storage requires it to remain open and unchanged while
 * the parsed content is in use.
 **/
typedef enum {
	GMIME_CONTENT_STORAGE_MEMORY,
	GMIME_CONTENT_STORAGE_SOURCE
} GMimeContentStorage;


/**
 * GMimeContent:
 *
 * A wrapper for a stream which may be encoded.
 **/
G_DECLARE_DERIVABLE_TYPE (GMimeContent, g_mime_content, GMIME, CONTENT, GObject)

struct _GMimeContentClass {
	GObjectClass parent_class;

	ssize_t (* write_to_stream) (GMimeContent *content, GMimeStream *stream);
	ssize_t (* decode_to_stream) (GMimeContent *content, GMimeStream *stream);

	/*< private >*/
	gpointer reserved[8];
};


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
