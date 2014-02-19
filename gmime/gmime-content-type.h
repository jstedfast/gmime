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


#ifndef __GMIME_CONTENT_TYPE_H__
#define __GMIME_CONTENT_TYPE_H__

#include <glib.h>
#include <glib-object.h>

#include <gmime/gmime-param.h>

G_BEGIN_DECLS

#define GMIME_TYPE_CONTENT_TYPE              (g_mime_content_type_get_type ())
#define GMIME_CONTENT_TYPE(obj)              (G_TYPE_CHECK_INSTANCE_CAST ((obj), g_mime_content_type_get_type (), GMimeContentType))
#define GMIME_CONTENT_TYPE_CLASS(klass)      (G_TYPE_CHECK_CLASS_CAST ((klass), g_mime_content_type_get_type (), GMimeContentTypeClass))
#define GMIME_IS_CONTENT_TYPE(obj)           (G_TYPE_CHECK_INSTANCE_TYPE ((obj), g_mime_content_type_get_type ()))
#define GMIME_IS_CONTENT_TYPE_CLASS(klass)   (G_TYPE_CHECK_CLASS_TYPE ((klass), g_mime_content_type_get_type ()))
#define GMIME_CONTENT_TYPE_GET_CLASS(obj)    (G_TYPE_INSTANCE_GET_CLASS ((obj), g_mime_content_type_get_type (), GMimeContentTypeClass))

typedef struct _GMimeContentType GMimeContentType;
typedef struct _GMimeContentTypeClass GMimeContentTypeClass;

/**
 * GMimeContentType:
 * @parent_object: parent #GObject
 * @param_hash: parameter hash keyed by param name
 * @params: a #GMimeParam list
 * @type: media type
 * @subtype: media subtype
 *
 * A data structure representing a Content-Type.
 **/
struct _GMimeContentType {
	GObject parent_object;
	
	GHashTable *param_hash;
	GMimeParam *params;
	
	gpointer priv;
	
	char *type;
	char *subtype;
};

struct _GMimeContentTypeClass {
	GObjectClass parent_class;
	
};

GType g_mime_content_type_get_type (void);


GMimeContentType *g_mime_content_type_new (const char *type, const char *subtype);
GMimeContentType *g_mime_content_type_new_from_string (const char *str);

char *g_mime_content_type_to_string (GMimeContentType *mime_type);

gboolean g_mime_content_type_is_type (GMimeContentType *mime_type, const char *type, const char *subtype);

void g_mime_content_type_set_media_type (GMimeContentType *mime_type, const char *type);
const char *g_mime_content_type_get_media_type (GMimeContentType *mime_type);

void g_mime_content_type_set_media_subtype (GMimeContentType *mime_type, const char *subtype);
const char *g_mime_content_type_get_media_subtype (GMimeContentType *mime_type);

void g_mime_content_type_set_params (GMimeContentType *mime_type, GMimeParam *params);
const GMimeParam *g_mime_content_type_get_params (GMimeContentType *mime_type);

void g_mime_content_type_set_parameter (GMimeContentType *mime_type, const char *attribute, const char *value);
const char *g_mime_content_type_get_parameter (GMimeContentType *mime_type, const char *attribute);

G_END_DECLS

#endif /* __GMIME_CONTENT_TYPE_H__ */
