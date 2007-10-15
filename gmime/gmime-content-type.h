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


#ifndef __GMIME_CONTENT_TYPE_H__
#define __GMIME_CONTENT_TYPE_H__

#include <glib.h>
#include <gmime/gmime-param.h>

G_BEGIN_DECLS

struct _GMimeContentType {
	char *type;
	char *subtype;
	
	GMimeParam *params;
	GHashTable *param_hash;
};

typedef struct _GMimeContentType GMimeContentType;

GMimeContentType *g_mime_content_type_new (const char *type, const char *subtype);
GMimeContentType *g_mime_content_type_new_from_string (const char *string);

void g_mime_content_type_destroy (GMimeContentType *mime_type);

char *g_mime_content_type_to_string (const GMimeContentType *mime_type);

gboolean g_mime_content_type_is_type (const GMimeContentType *mime_type, const char *type, const char *subtype);

void g_mime_content_type_set_parameter (GMimeContentType *mime_type, const char *attribute, const char *value);
const char *g_mime_content_type_get_parameter (const GMimeContentType *mime_type, const char *attribute);


G_END_DECLS

#endif /* __GMIME_PART_H__ */
