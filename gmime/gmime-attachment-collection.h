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

#ifndef __GMIME_ATTACHMENT_COLLECTION_H__
#define __GMIME_ATTACHMENT_COLLECTION_H__

#include <gmime/gmime-entity.h>
#include <gmime/gmime-stream.h>

G_BEGIN_DECLS

#define GMIME_TYPE_ATTACHMENT_COLLECTION (g_mime_attachment_collection_get_type ())


/**
 * GMimeAttachmentCollection:
 *
 * An ordered, mutable collection for constructing attachments or linked resources.
 **/
G_DECLARE_FINAL_TYPE (GMimeAttachmentCollection, g_mime_attachment_collection, GMIME, ATTACHMENT_COLLECTION, GObject)


GMimeAttachmentCollection *g_mime_attachment_collection_new (gboolean linked_resources);
int g_mime_attachment_collection_get_count (GMimeAttachmentCollection *collection);
GMimeEntity *g_mime_attachment_collection_get_attachment (GMimeAttachmentCollection *collection, int index);
void g_mime_attachment_collection_set_attachment (GMimeAttachmentCollection *collection, int index, GMimeEntity *attachment);
void g_mime_attachment_collection_add (GMimeAttachmentCollection *collection, GMimeEntity *attachment);
GMimeEntity *g_mime_attachment_collection_add_data (GMimeAttachmentCollection *collection, const char *file_name,
						 const guint8 *data, gsize length, GMimeContentType *content_type, GError **error);
GMimeEntity *g_mime_attachment_collection_add_stream (GMimeAttachmentCollection *collection, const char *file_name,
						   GMimeStream *stream, GMimeContentType *content_type, GError **error);
GMimeEntity *g_mime_attachment_collection_add_file (GMimeAttachmentCollection *collection, const char *file_name,
						 GMimeContentType *content_type, GError **error);
void g_mime_attachment_collection_clear (GMimeAttachmentCollection *collection);
gboolean g_mime_attachment_collection_contains (GMimeAttachmentCollection *collection, GMimeEntity *attachment);
int g_mime_attachment_collection_index_of (GMimeAttachmentCollection *collection, GMimeEntity *attachment);
void g_mime_attachment_collection_insert (GMimeAttachmentCollection *collection, int index, GMimeEntity *attachment);
gboolean g_mime_attachment_collection_remove (GMimeAttachmentCollection *collection, GMimeEntity *attachment);
void g_mime_attachment_collection_remove_at (GMimeAttachmentCollection *collection, int index);

G_END_DECLS

#endif /* __GMIME_ATTACHMENT_COLLECTION_H__ */
