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


#ifndef __GMIME_HEADER_H__
#define __GMIME_HEADER_H__

#include <glib.h>
#include <gmime/gmime-stream.h>

G_BEGIN_DECLS

typedef struct _GMimeHeader GMimeHeader;


/**
 * GMimeHeaderForeachFunc:
 * @name: The field name.
 * @value: The field value.
 * @user_data: The user-supplied callback data.
 *
 * Function signature for the callback to g_mime_header_list_foreach().
 **/
typedef void (* GMimeHeaderForeachFunc) (const char *name, const char *value, gpointer user_data);


/**
 * GMimeHeaderList:
 *
 * A message or mime-part header.
 **/
typedef struct _GMimeHeaderList GMimeHeaderList;


/**
 * GMimeHeaderIter:
 * @hdrlist: a #GMimeHeaderList
 * @cursor: a #GMimeHeader
 * @version: the version of @hdrlist when initialized
 *
 * A message or mime-part header iterator. All members should be
 * considered private.
 **/
typedef struct _GMimeHeaderIter GMimeHeaderIter;

struct _GMimeHeaderIter {
	GMimeHeaderList *hdrlist;
	GMimeHeader *cursor;
	guint32 version;
};


GMimeHeaderIter *g_mime_header_iter_new (void);
void g_mime_header_iter_free (GMimeHeaderIter *iter);

GMimeHeaderIter *g_mime_header_iter_copy (GMimeHeaderIter *iter);
void g_mime_header_iter_copy_to (GMimeHeaderIter *src, GMimeHeaderIter *dest);

gboolean g_mime_header_iter_equal (GMimeHeaderIter *iter1, GMimeHeaderIter *iter2);

gboolean g_mime_header_iter_is_valid (GMimeHeaderIter *iter);

gboolean g_mime_header_iter_first (GMimeHeaderIter *iter);
gboolean g_mime_header_iter_last (GMimeHeaderIter *iter);

gboolean g_mime_header_iter_next (GMimeHeaderIter *iter);
gboolean g_mime_header_iter_prev (GMimeHeaderIter *iter);

/*gint64 g_mime_header_iter_get_offset (GMimeHeaderIter *iter);*/
const char *g_mime_header_iter_get_name (GMimeHeaderIter *iter);
gboolean g_mime_header_iter_set_value (GMimeHeaderIter *iter, const char *value);
const char *g_mime_header_iter_get_value (GMimeHeaderIter *iter);

gboolean g_mime_header_iter_remove (GMimeHeaderIter *iter);


/**
 * GMimeHeaderWriter:
 * @stream: The output stream.
 * @name: The field name.
 * @value: The field value.
 *
 * Function signature for the callback to
 * g_mime_header_list_register_writer().
 *
 * Returns: the number of bytes written or %-1 on error.
 **/
typedef ssize_t (* GMimeHeaderWriter) (GMimeStream *stream, const char *name, const char *value);


GMimeHeaderList *g_mime_header_list_new (void);

void g_mime_header_list_destroy (GMimeHeaderList *headers);

void g_mime_header_list_set_stream (GMimeHeaderList *headers, GMimeStream *stream);
GMimeStream *g_mime_header_list_get_stream (GMimeHeaderList *headers);

void g_mime_header_list_clear (GMimeHeaderList *headers);
gboolean g_mime_header_list_contains (const GMimeHeaderList *headers, const char *name);
void g_mime_header_list_prepend (GMimeHeaderList *headers, const char *name, const char *value);
void g_mime_header_list_append (GMimeHeaderList *headers, const char *name, const char *value);
void g_mime_header_list_set (GMimeHeaderList *headers, const char *name, const char *value);
const char *g_mime_header_list_get (const GMimeHeaderList *headers, const char *name);
gboolean g_mime_header_list_remove (GMimeHeaderList *headers, const char *name);

gboolean g_mime_header_list_get_iter (GMimeHeaderList *headers, GMimeHeaderIter *iter);

void g_mime_header_list_foreach (const GMimeHeaderList *headers, GMimeHeaderForeachFunc func, gpointer user_data);

void g_mime_header_list_register_writer (GMimeHeaderList *headers, const char *name, GMimeHeaderWriter writer);
ssize_t g_mime_header_list_write_to_stream (const GMimeHeaderList *headers, GMimeStream *stream);
char *g_mime_header_list_to_string (const GMimeHeaderList *headers);

G_END_DECLS

#endif /* __GMIME_HEADER_H__ */
