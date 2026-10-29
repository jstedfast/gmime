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
#include <gmime/gmime-content.h>
#include <gmime/gmime-content-type.h>
#include <gmime/gmime-disposition.h>
#include <gmime/gmime-encodings.h>
#include <gmime/gmime-stream.h>
#include <gmime/gmime-header.h>
#include <gmime/gmime-autocrypt.h>

G_BEGIN_DECLS

#define GMIME_TYPE_ENTITY            (g_mime_entity_get_type ())

typedef struct _GMimeVisitor GMimeVisitor;

/**
 * GMimeEntity:
 *
 * Base class for all MIME parts.
 **/
G_DECLARE_DERIVABLE_TYPE (GMimeEntity, g_mime_entity, GMIME, ENTITY, GObject)

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

	void         (* accept) (GMimeEntity *object, GMimeVisitor *visitor);

	gboolean     (* prepare_with_max_line_length) (GMimeEntity *object, GMimeEncodingConstraint constraint,
						      guint max_line_length, GError **error);

	/*< private >*/
	gpointer reserved[8];
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


void g_mime_entity_register_type (const char *type, const char *subtype, GType object_type);

GMimeEntity *g_mime_entity_new (GMimeParserOptions *options, GMimeContentType *content_type);
GMimeEntity *g_mime_entity_new_type (GMimeParserOptions *options, const char *type, const char *subtype);

GMimeEntity *g_mime_entity_load (GMimeParserOptions *options, GMimeStream *stream, GError **error);
GMimeEntity *g_mime_entity_load_with_storage (GMimeParserOptions *options, GMimeStream *stream,
					    GMimeContentStorage storage, GError **error);
GMimeEntity *g_mime_entity_load_from_file (GMimeParserOptions *options, const char *filename, GError **error);
GMimeEntity *g_mime_entity_load_with_content_type (GMimeParserOptions *options, GMimeContentType *content_type,
						 GMimeStream *content, GError **error);

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
gboolean g_mime_entity_write_to_file (GMimeEntity *object, GMimeFormatOptions *options,
				    const char *filename, GError **error);
gboolean g_mime_entity_write_content_to_file (GMimeEntity *object, GMimeFormatOptions *options,
					    const char *filename, GError **error);
char *g_mime_entity_to_string (GMimeEntity *object, GMimeFormatOptions *options);

gboolean g_mime_entity_prepare (GMimeEntity *object, GMimeEncodingConstraint constraint, GError **error);
gboolean g_mime_entity_prepare_with_max_line_length (GMimeEntity *object, GMimeEncodingConstraint constraint,
						   guint max_line_length, GError **error);

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
