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


#ifndef __GMIME_ENTITY_H__
#define __GMIME_ENTITY_H__

#include <glib.h>
#include <glib-object.h>

#include <gmime/gmime-format-options.h>
#include <gmime/gmime-parser-options.h>
#include <gmime/gmime-content-type.h>
#include <gmime/gmime-disposition.h>
#include <gmime/gmime-encodings.h>
#include <gmime/gmime-stream.h>
#include <gmime/gmime-header.h>
#include <gmime/gmime-autocrypt.h>

G_BEGIN_DECLS

#define GMIME_TYPE_ENTITY            (g_mime_entity_get_type ())
#define GMIME_ENTITY(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), GMIME_TYPE_ENTITY, GMimeEntity))
#define GMIME_ENTITY_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), GMIME_TYPE_ENTITY, GMimeEntityClass))
#define GMIME_IS_ENTITY(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GMIME_TYPE_ENTITY))
#define GMIME_IS_ENTITY_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), GMIME_TYPE_ENTITY))
#define GMIME_ENTITY_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), GMIME_TYPE_ENTITY, GMimeEntityClass))

typedef struct _GMimeEntity GMimeEntity;
typedef struct _GMimeEntityClass GMimeEntityClass;
typedef struct _GMimeVisitor GMimeVisitor;

/**
 * GMimeEntity:
 * @parent_object: parent #GObject
 * @disposition: a #GMimeContentDisposition
 * @content_type: a #GMimeContentType
 * @content_id: a Content-Id
 * @headers: a #GMimeHeaderList
 *
 * Base class for all MIME parts.
 **/
struct _GMimeEntity {
	GObject parent_object;

	GMimeContentDisposition *disposition;
	GMimeContentType *content_type;
	GMimeHeaderList *headers;
	char *content_id;

	/* < private > */
	gboolean ensure_newline;
	char *content_base;
	char *content_location;
	gboolean content_base_loaded;
	gboolean content_location_loaded;
};

struct _GMimeEntityClass {
	GObjectClass parent_class;

	void         (* header_added)    (GMimeEntity *object, GMimeHeader *header);
	void         (* header_changed)  (GMimeEntity *object, GMimeHeader *header);
	void         (* header_removed)  (GMimeEntity *object, GMimeHeader *header);
	void         (* headers_cleared) (GMimeEntity *object);

	void         (* set_content_type) (GMimeEntity *object, GMimeContentType *content_type);

	char *       (* get_headers)   (GMimeEntity *object, GMimeFormatOptions *options);

	ssize_t      (* write_to_stream) (GMimeEntity *object, GMimeFormatOptions *options,
					  gboolean content_only, GMimeStream *stream);

	void         (* encode) (GMimeEntity *object, GMimeEncodingConstraint constraint);

	void         (* accept) (GMimeEntity *object, GMimeVisitor *visitor);
};


/**
 * GMimeEntityForeachFunc:
 * @parent: parent #GMimeEntity
 * @part: a #GMimeEntity
 * @user_data: User-supplied callback data.
 *
 * The function signature for a callback to g_mime_message_foreach()
 * and g_mime_multipart_foreach().
 **/
typedef void (* GMimeEntityForeachFunc) (GMimeEntity *parent, GMimeEntity *part, gpointer user_data);


GType g_mime_entity_get_type (void);

void g_mime_entity_register_type (const char *type, const char *subtype, GType object_type);

GMimeEntity *g_mime_entity_new (GMimeParserOptions *options, GMimeContentType *content_type);
GMimeEntity *g_mime_entity_new_type (GMimeParserOptions *options, const char *type, const char *subtype);

void g_mime_entity_set_content_type (GMimeEntity *object, GMimeContentType *content_type);
GMimeContentType *g_mime_entity_get_content_type (GMimeEntity *object);
void g_mime_entity_set_content_type_parameter (GMimeEntity *object, const char *name, const char *value);
const char *g_mime_entity_get_content_type_parameter (GMimeEntity *object, const char *name);

void g_mime_entity_set_content_disposition (GMimeEntity *object, GMimeContentDisposition *disposition);
GMimeContentDisposition *g_mime_entity_get_content_disposition (GMimeEntity *object);

void g_mime_entity_set_disposition (GMimeEntity *object, const char *disposition);
const char *g_mime_entity_get_disposition (GMimeEntity *object);

gboolean g_mime_entity_is_attachment (GMimeEntity *object);
void g_mime_entity_set_is_attachment (GMimeEntity *object, gboolean attachment);

void g_mime_entity_set_content_disposition_parameter (GMimeEntity *object, const char *name, const char *value);
const char *g_mime_entity_get_content_disposition_parameter (GMimeEntity *object, const char *name);

void g_mime_entity_set_content_id (GMimeEntity *object, const char *content_id);
const char *g_mime_entity_get_content_id (GMimeEntity *object);

void g_mime_entity_set_content_base (GMimeEntity *object, const char *uri);
const char *g_mime_entity_get_content_base (GMimeEntity *object);

void g_mime_entity_set_content_location (GMimeEntity *object, const char *uri);
const char *g_mime_entity_get_content_location (GMimeEntity *object);

void g_mime_entity_prepend_header (GMimeEntity *object, const char *header, const char *value, const char *charset);
void g_mime_entity_append_header (GMimeEntity *object, const char *header, const char *value, const char *charset);
void g_mime_entity_set_header (GMimeEntity *object, const char *header, const char *value, const char *charset);
const char *g_mime_entity_get_header (GMimeEntity *object, const char *header);
gboolean g_mime_entity_remove_header (GMimeEntity *object, const char *header);

GMimeHeaderList *g_mime_entity_get_header_list (GMimeEntity *object);

char *g_mime_entity_get_headers (GMimeEntity *object, GMimeFormatOptions *options);

ssize_t g_mime_entity_write_to_stream (GMimeEntity *object, GMimeFormatOptions *options, GMimeStream *stream);
ssize_t g_mime_entity_write_content_to_stream (GMimeEntity *object, GMimeFormatOptions *options, GMimeStream *stream);
char *g_mime_entity_to_string (GMimeEntity *object, GMimeFormatOptions *options);

void g_mime_entity_encode (GMimeEntity *object, GMimeEncodingConstraint constraint);

void g_mime_entity_accept (GMimeEntity *object, GMimeVisitor *visitor);

/* Internal API */
G_GNUC_INTERNAL void g_mime_entity_type_registry_init (void);
G_GNUC_INTERNAL void g_mime_entity_type_registry_shutdown (void);
G_GNUC_INTERNAL GMimeAutocryptHeaderList *
g_mime_entity_get_autocrypt_headers(GMimeEntity *mime_part, GDateTime *effective_date,
				    const char *matchheader, GMimeInternetAddressList *addresses,
				    gboolean keep_incomplete);

G_END_DECLS

#endif /* __GMIME_ENTITY_H__ */
