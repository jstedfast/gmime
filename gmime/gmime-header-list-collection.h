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

#ifndef __GMIME_HEADER_LIST_COLLECTION_H__
#define __GMIME_HEADER_LIST_COLLECTION_H__

#include <gmime/gmime-header.h>

G_BEGIN_DECLS

#define GMIME_TYPE_HEADER_LIST_COLLECTION (g_mime_header_list_collection_get_type ())
#define GMIME_HEADER_LIST_COLLECTION(obj) (G_TYPE_CHECK_INSTANCE_CAST ((obj), GMIME_TYPE_HEADER_LIST_COLLECTION, GMimeHeaderListCollection))
#define GMIME_IS_HEADER_LIST_COLLECTION(obj) (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GMIME_TYPE_HEADER_LIST_COLLECTION))

typedef struct _GMimeHeaderListCollection GMimeHeaderListCollection;
typedef struct _GMimeHeaderListCollectionClass GMimeHeaderListCollectionClass;

/**
 * GMimeHeaderListCollection:
 * @parent_object: parent #GObject
 *
 * An ordered collection of shared header-list groups.
 **/
struct _GMimeHeaderListCollection {
	GObject parent_object;

	/* <private> */
	GPtrArray *array;
	gpointer changed;
};

struct _GMimeHeaderListCollectionClass {
	GObjectClass parent_class;
};

GType g_mime_header_list_collection_get_type (void);
GMimeHeaderListCollection *g_mime_header_list_collection_new (void);
int g_mime_header_list_collection_get_count (GMimeHeaderListCollection *collection);
GMimeHeaderList *g_mime_header_list_collection_get_group (GMimeHeaderListCollection *collection, int index);
void g_mime_header_list_collection_set_group (GMimeHeaderListCollection *collection, int index, GMimeHeaderList *group);
void g_mime_header_list_collection_add (GMimeHeaderListCollection *collection, GMimeHeaderList *group);
void g_mime_header_list_collection_clear (GMimeHeaderListCollection *collection);
gboolean g_mime_header_list_collection_contains (GMimeHeaderListCollection *collection, GMimeHeaderList *group);
gboolean g_mime_header_list_collection_remove (GMimeHeaderListCollection *collection, GMimeHeaderList *group);

G_END_DECLS

#endif /* __GMIME_HEADER_LIST_COLLECTION_H__ */
