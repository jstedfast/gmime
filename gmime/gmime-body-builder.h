/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*  GMime
 *  Copyright (C) 2026 Jeffrey Stedfast
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

#ifndef __GMIME_BODY_BUILDER_H__
#define __GMIME_BODY_BUILDER_H__

#include <gmime/gmime-attachment-collection.h>

G_BEGIN_DECLS

#define GMIME_TYPE_BODY_BUILDER (g_mime_body_builder_get_type ())
#define GMIME_BODY_BUILDER(obj) (G_TYPE_CHECK_INSTANCE_CAST ((obj), GMIME_TYPE_BODY_BUILDER, GMimeBodyBuilder))
#define GMIME_IS_BODY_BUILDER(obj) (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GMIME_TYPE_BODY_BUILDER))

typedef struct _GMimeBodyBuilder GMimeBodyBuilder;
typedef struct _GMimeBodyBuilderClass GMimeBodyBuilderClass;

/**
 * GMimeBodyBuilder:
 * @parent_object: parent #GObject
 *
 * A helper for constructing common MIME message body structures.
 **/
struct _GMimeBodyBuilder {
	GObject parent_object;

	/* <private> */
	GMimeAttachmentCollection *attachments;
	GMimeAttachmentCollection *linked_resources;
	char *text_body;
	char *html_body;
	char *body_encoding;
};

struct _GMimeBodyBuilderClass {
	GObjectClass parent_class;
};

GType g_mime_body_builder_get_type (void);
GMimeBodyBuilder *g_mime_body_builder_new (void);
GMimeAttachmentCollection *g_mime_body_builder_get_attachments (GMimeBodyBuilder *builder);
GMimeAttachmentCollection *g_mime_body_builder_get_linked_resources (GMimeBodyBuilder *builder);
const char *g_mime_body_builder_get_text_body (GMimeBodyBuilder *builder);
void g_mime_body_builder_set_text_body (GMimeBodyBuilder *builder, const char *text);
const char *g_mime_body_builder_get_html_body (GMimeBodyBuilder *builder);
void g_mime_body_builder_set_html_body (GMimeBodyBuilder *builder, const char *html);
const char *g_mime_body_builder_get_body_encoding (GMimeBodyBuilder *builder);
gboolean g_mime_body_builder_set_body_encoding (GMimeBodyBuilder *builder, const char *charset, GError **error);
GMimeEntity *g_mime_body_builder_to_message_body (GMimeBodyBuilder *builder, GError **error);

G_END_DECLS

#endif /* __GMIME_BODY_BUILDER_H__ */
