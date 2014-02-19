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


#ifndef __GMIME_OBJECT_H__
#define __GMIME_OBJECT_H__

#include <glib.h>
#include <glib-object.h>

#include <gmime/gmime-content-type.h>
#include <gmime/gmime-disposition.h>
#include <gmime/gmime-encodings.h>
#include <gmime/gmime-stream.h>
#include <gmime/gmime-header.h>

G_BEGIN_DECLS

#define GMIME_TYPE_OBJECT            (g_mime_object_get_type ())
#define GMIME_OBJECT(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), GMIME_TYPE_OBJECT, GMimeObject))
#define GMIME_OBJECT_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), GMIME_TYPE_OBJECT, GMimeObjectClass))
#define GMIME_IS_OBJECT(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GMIME_TYPE_OBJECT))
#define GMIME_IS_OBJECT_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), GMIME_TYPE_OBJECT))
#define GMIME_OBJECT_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), GMIME_TYPE_OBJECT, GMimeObjectClass))

typedef struct _GMimeObject GMimeObject;
typedef struct _GMimeObjectClass GMimeObjectClass;

/**
 * GMimeObject:
 * @parent_object: parent #GObject
 * @disposition: a #GMimeContentDisposition
 * @content_type: a #GMimeContentType
 * @content_id: a Content-Id
 * @headers: a #GMimeHeaderList
 *
 * Base class for all MIME parts.
 **/
struct _GMimeObject {
	GObject parent_object;
	
	GMimeContentDisposition *disposition;
	GMimeContentType *content_type;
	GMimeHeaderList *headers;
	
	char *content_id;
};

struct _GMimeObjectClass {
	GObjectClass parent_class;
	
	void         (* prepend_header) (GMimeObject *object, const char *header, const char *value);
	void         (* append_header)  (GMimeObject *object, const char *header, const char *value);
	void         (* set_header)     (GMimeObject *object, const char *header, const char *value);
	const char * (* get_header)     (GMimeObject *object, const char *header);
	gboolean     (* remove_header)  (GMimeObject *object, const char *header);
	
	void         (* set_content_type) (GMimeObject *object, GMimeContentType *content_type);
	
	char *       (* get_headers)   (GMimeObject *object);
	
	ssize_t      (* write_to_stream) (GMimeObject *object, GMimeStream *stream);
	
	void         (* encode) (GMimeObject *object, GMimeEncodingConstraint constraint);
};


/**
 * GMimeObjectForeachFunc:
 * @parent: parent #GMimeObject
 * @part: a #GMimeObject
 * @user_data: User-supplied callback data.
 *
 * The function signature for a callback to g_mime_message_foreach()
 * and g_mime_multipart_foreach().
 **/
typedef void (* GMimeObjectForeachFunc) (GMimeObject *parent, GMimeObject *part, gpointer user_data);


GType g_mime_object_get_type (void);

void g_mime_object_register_type (const char *type, const char *subtype, GType object_type);

GMimeObject *g_mime_object_new (GMimeContentType *content_type);
GMimeObject *g_mime_object_new_type (const char *type, const char *subtype);

void g_mime_object_set_content_type (GMimeObject *object, GMimeContentType *content_type);
GMimeContentType *g_mime_object_get_content_type (GMimeObject *object);
void g_mime_object_set_content_type_parameter (GMimeObject *object, const char *name, const char *value);
const char *g_mime_object_get_content_type_parameter (GMimeObject *object, const char *name);

void g_mime_object_set_content_disposition (GMimeObject *object, GMimeContentDisposition *disposition);
GMimeContentDisposition *g_mime_object_get_content_disposition (GMimeObject *object);

void g_mime_object_set_disposition (GMimeObject *object, const char *disposition);
const char *g_mime_object_get_disposition (GMimeObject *object);

void g_mime_object_set_content_disposition_parameter (GMimeObject *object, const char *attribute, const char *value);
const char *g_mime_object_get_content_disposition_parameter (GMimeObject *object, const char *attribute);

void g_mime_object_set_content_id (GMimeObject *object, const char *content_id);
const char *g_mime_object_get_content_id (GMimeObject *object);

void g_mime_object_prepend_header (GMimeObject *object, const char *header, const char *value);
void g_mime_object_append_header (GMimeObject *object, const char *header, const char *value);
void g_mime_object_set_header (GMimeObject *object, const char *header, const char *value);
const char *g_mime_object_get_header (GMimeObject *object, const char *header);
gboolean g_mime_object_remove_header (GMimeObject *object, const char *header);

GMimeHeaderList *g_mime_object_get_header_list (GMimeObject *object);

char *g_mime_object_get_headers (GMimeObject *object);

ssize_t g_mime_object_write_to_stream (GMimeObject *object, GMimeStream *stream);
char *g_mime_object_to_string (GMimeObject *object);

void g_mime_object_encode (GMimeObject *object, GMimeEncodingConstraint constraint);

/* Internal API */
G_GNUC_INTERNAL void g_mime_object_type_registry_init (void);
G_GNUC_INTERNAL void g_mime_object_type_registry_shutdown (void);

G_END_DECLS

#endif /* __GMIME_OBJECT_H__ */
