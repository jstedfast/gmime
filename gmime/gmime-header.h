/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*  GMime
 *  Copyright (C) 2000-2007 Jeffrey Stedfast
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

typedef void (*GMimeHeaderForeachFunc) (const char *name, const char *value, gpointer user_data);
typedef ssize_t (*GMimeHeaderWriter) (GMimeStream *stream, const char *name, const char *value);

GMimeHeader *g_mime_header_new (void);

void g_mime_header_destroy (GMimeHeader *header);

void g_mime_header_set (GMimeHeader *header, const char *name, const char *value);

void g_mime_header_add (GMimeHeader *header, const char *name, const char *value);

void g_mime_header_prepend (GMimeHeader *header, const char *name, const char *value);

const char *g_mime_header_get (const GMimeHeader *header, const char *name);

void g_mime_header_remove (GMimeHeader *header, const char *name);

ssize_t g_mime_header_write_to_stream (const GMimeHeader *header, GMimeStream *stream);

char *g_mime_header_to_string (const GMimeHeader *header);

void g_mime_header_foreach (const GMimeHeader *header, GMimeHeaderForeachFunc func, gpointer user_data);

void g_mime_header_register_writer (GMimeHeader *header, const char *name, GMimeHeaderWriter writer);


/* for internal use only */
void g_mime_header_set_raw (GMimeHeader *header, const char *raw);
gboolean g_mime_header_has_raw (GMimeHeader *header);

G_END_DECLS

#endif /* __GMIME_HEADER_H__ */
