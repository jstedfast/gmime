/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 *  Authors: Jeffrey Stedfast <fejj@helixcode.com>
 *
 *  Copyright 2000 Helix Code, Inc. (www.helixcode.com)
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

#ifndef __GMIME_CONTENT_TYPE_H__
#define __GMIME_CONTENT_TYPE_H__

#ifdef __cplusplus
extern "C" {
#pragma }
#endif /* __cplusplus }*/

#include <glib.h>
#include <gmime-param.h>

struct _GMimeContentType {
	gchar *type;
	gchar *subtype;
	
	GList *params;   /* of type GMimeParam */
	GHashTable *param_hash;
};

typedef struct _GMimeContentType GMimeContentType;

GMimeContentType *g_mime_content_type_new (const gchar *type, const gchar *subtype);
GMimeContentType *g_mime_content_type_new_from_string (const gchar *string);

void g_mime_content_type_destroy (GMimeContentType *mime_type);

gchar *g_mime_content_type_to_string (const GMimeContentType *mime_type);

gboolean g_mime_content_type_is_type (const GMimeContentType *mime_type, const char *type, const char *subtype);

void g_mime_content_type_add_parameter (GMimeContentType *mime_type, const gchar *attribute, const gchar *value);
const gchar *g_mime_content_type_get_parameter (const GMimeContentType *mime_type, const gchar *attribute);



#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* __GMIME_PART_H__ */
