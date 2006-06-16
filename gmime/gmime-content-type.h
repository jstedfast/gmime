/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*  GMime
 *  Copyright (C) 1999-2006 Jeffrey Stedfast
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
 */


#ifndef __GMIME_CONTENT_TYPE_H__
#define __GMIME_CONTENT_TYPE_H__

#include <glib.h>
#include <gmime/gmime-param.h>

#ifdef __cplusplus
extern "C" {
#pragma }
#endif /* __cplusplus */

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


#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* __GMIME_PART_H__ */
