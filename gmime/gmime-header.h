/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*  GMime
 *  Copyright (C) 2000-2017 Jeffrey Stedfast
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


#ifndef __GMIME_HEADER_H__
#define __GMIME_HEADER_H__

#include <glib.h>
#include <gmime/gmime-parser-options.h>
#include <gmime/gmime-stream.h>

G_BEGIN_DECLS


/**
 * GMimeHeaderWriter:
 * @options: The #GMimeParserOptions
 * @stream: The output stream.
 * @name: The field name.
 * @value: The field value.
 *
 * Function signature for the callback to
 * g_mime_header_list_register_writer().
 *
 * Returns: the number of bytes written or %-1 on error.
 **/
typedef ssize_t (* GMimeHeaderWriter) (GMimeParserOptions *options, GMimeStream *stream, const char *name, const char *value);


/**
 * GMimeHeader:
 *
 * A message/rfc822 header.
 **/
typedef struct _GMimeHeader GMimeHeader;

const char *g_mime_header_get_name (GMimeHeader *header);

const char *g_mime_header_get_value (GMimeHeader *header);
void g_mime_header_set_value (GMimeHeader *header, const char *value);

gint64 g_mime_header_get_offset (GMimeHeader *header);

ssize_t g_mime_header_write_to_stream (GMimeHeader *header, GMimeStream *stream);


/**
 * GMimeHeaderList:
 *
 * A list of message or mime-part headers.
 **/
typedef struct _GMimeHeaderList GMimeHeaderList;

GMimeHeaderList *g_mime_header_list_new (GMimeParserOptions *options);

void g_mime_header_list_destroy (GMimeHeaderList *headers);

void g_mime_header_list_clear (GMimeHeaderList *headers);
int g_mime_header_list_get_count (GMimeHeaderList *headers);
gboolean g_mime_header_list_contains (const GMimeHeaderList *headers, const char *name);
void g_mime_header_list_prepend (GMimeHeaderList *headers, const char *name, const char *value);
void g_mime_header_list_append (GMimeHeaderList *headers, const char *name, const char *value);
void g_mime_header_list_set (GMimeHeaderList *headers, const char *name, const char *value);
const char *g_mime_header_list_get (const GMimeHeaderList *headers, const char *name);
GMimeHeader *g_mime_header_list_get_header (GMimeHeaderList *headers, int index);
gboolean g_mime_header_list_remove (GMimeHeaderList *headers, const char *name);
void g_mime_header_list_remove_at (GMimeHeaderList *headers, int index);

void g_mime_header_list_register_writer (GMimeHeaderList *headers, const char *name, GMimeHeaderWriter writer);
ssize_t g_mime_header_list_write_to_stream (const GMimeHeaderList *headers, GMimeStream *stream);
char *g_mime_header_list_to_string (const GMimeHeaderList *headers);

G_END_DECLS

#endif /* __GMIME_HEADER_H__ */
