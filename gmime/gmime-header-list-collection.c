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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "gmime-header-list-collection.h"
#include "gmime-events.h"

#include "gmime-object-private.h"

/**
 * SECTION: gmime-header-list-collection
 * @title: GMimeHeaderListCollection
 * @short_description: Ordered collections of header groups
 * @see_also: #GMimeHeaderList
 *
 * A mutable collection of header lists, retaining each group by reference.
 * Groups may be shared by multiple collections or occur more than once
 * in the same collection. Membership uses object identity, not field values.
 **/

static void collection_finalize (GObject *object);
static void group_changed (gpointer sender, gpointer args, gpointer user_data);

G_DEFINE_FINAL_TYPE (GMimeHeaderListCollection, g_mime_header_list_collection, G_TYPE_OBJECT)

static void
g_mime_header_list_collection_class_init (GMimeHeaderListCollectionClass *klass)
{
	G_OBJECT_CLASS (klass)->finalize = collection_finalize;
}

static void
g_mime_header_list_collection_init (GMimeHeaderListCollection *collection)
{
	collection->array = g_ptr_array_new_with_free_func (g_object_unref);
	collection->changed = g_mime_event_new (collection);
}

static void
collection_finalize (GObject *object)
{
	GMimeHeaderListCollection *collection = GMIME_HEADER_LIST_COLLECTION (object);
	GMimeHeaderList *group;
	guint i;

	for (i = 0; i < collection->array->len; i++) {
		group = collection->array->pdata[i];
		g_mime_event_remove (group->changed, (GMimeEventCallback) group_changed, collection);
	}
	g_ptr_array_unref (collection->array);
	g_mime_event_free (collection->changed);
	G_OBJECT_CLASS (g_mime_header_list_collection_parent_class)->finalize (object);
}

static void
group_changed (gpointer sender, gpointer args, gpointer user_data)
{
	GMimeHeaderListCollection *collection = user_data;

	g_mime_event_emit (collection->changed, NULL);
}

/**
 * g_mime_header_list_collection_new:
 *
 * Returns: (transfer full): a new empty collection of header groups.
 **/
GMimeHeaderListCollection *
g_mime_header_list_collection_new (void)
{
	return g_object_new (GMIME_TYPE_HEADER_LIST_COLLECTION, NULL);
}

/**
 * g_mime_header_list_collection_get_count:
 * @collection: a #GMimeHeaderListCollection
 *
 * Returns: the number of header groups, including duplicate references.
 **/
int
g_mime_header_list_collection_get_count (GMimeHeaderListCollection *collection)
{
	g_return_val_if_fail (GMIME_IS_HEADER_LIST_COLLECTION (collection), 0);

	return collection->array->len;
}

/**
 * g_mime_header_list_collection_get_group:
 * @collection: a #GMimeHeaderListCollection
 * @index: an existing group index
 *
 * Returns: (transfer none): the group at @index.
 **/
GMimeHeaderList *
g_mime_header_list_collection_get_group (GMimeHeaderListCollection *collection, int index)
{
	g_return_val_if_fail (GMIME_IS_HEADER_LIST_COLLECTION (collection), NULL);
	g_return_val_if_fail (index >= 0 && (guint) index < collection->array->len, NULL);

	return collection->array->pdata[index];
}

/**
 * g_mime_header_list_collection_set_group:
 * @collection: a #GMimeHeaderListCollection
 * @index: an existing group index
 * @group: (transfer none): the replacement header group
 *
 * Retains @group and releases the previous group. Assigning the same group
 * is a no-op. Matching MimeKit, indexed replacement changes group-change
 * subscriptions but does not itself notify an owning entity; subsequent
 * edits to the replacement group do notify it.
 **/
void
g_mime_header_list_collection_set_group (GMimeHeaderListCollection *collection, int index, GMimeHeaderList *group)
{
	GMimeHeaderList *previous;

	g_return_if_fail (GMIME_IS_HEADER_LIST_COLLECTION (collection));
	g_return_if_fail (index >= 0 && (guint) index < collection->array->len);
	g_return_if_fail (GMIME_IS_HEADER_LIST (group));

	previous = collection->array->pdata[index];
	if (previous == group)
		return;

	g_object_ref (group);
	g_mime_event_remove (previous->changed, (GMimeEventCallback) group_changed, collection);
	g_mime_event_add (group->changed, (GMimeEventCallback) group_changed, collection);
	collection->array->pdata[index] = group;
	g_object_unref (previous);
}

/**
 * g_mime_header_list_collection_add:
 * @collection: a #GMimeHeaderListCollection
 * @group: (transfer none): the header group to append
 *
 * Retains @group and appends it without copying. Adding the same group
 * multiple times is allowed; its edits notify owners once per occurrence.
 **/
void
g_mime_header_list_collection_add (GMimeHeaderListCollection *collection, GMimeHeaderList *group)
{
	g_return_if_fail (GMIME_IS_HEADER_LIST_COLLECTION (collection));
	g_return_if_fail (GMIME_IS_HEADER_LIST (group));
	g_return_if_fail (collection->array->len < G_MAXINT);

	g_mime_event_add (group->changed, (GMimeEventCallback) group_changed, collection);
	g_ptr_array_add (collection->array, g_object_ref (group));
	g_mime_event_emit (collection->changed, NULL);
}

/**
 * g_mime_header_list_collection_clear:
 * @collection: a #GMimeHeaderListCollection
 *
 * Removes and releases all groups, disconnecting their change subscriptions.
 * Notifies an owning entity even when the collection is already empty.
 **/
void
g_mime_header_list_collection_clear (GMimeHeaderListCollection *collection)
{
	GMimeHeaderList *group;
	guint i;

	g_return_if_fail (GMIME_IS_HEADER_LIST_COLLECTION (collection));

	for (i = 0; i < collection->array->len; i++) {
		group = collection->array->pdata[i];
		g_mime_event_remove (group->changed, (GMimeEventCallback) group_changed, collection);
	}
	g_ptr_array_set_size (collection->array, 0);
	g_mime_event_emit (collection->changed, NULL);
}

/**
 * g_mime_header_list_collection_contains:
 * @collection: a #GMimeHeaderListCollection
 * @group: a header group
 *
 * Returns: whether the exact @group instance belongs to the collection.
 **/
gboolean
g_mime_header_list_collection_contains (GMimeHeaderListCollection *collection, GMimeHeaderList *group)
{
	g_return_val_if_fail (GMIME_IS_HEADER_LIST_COLLECTION (collection), FALSE);
	g_return_val_if_fail (GMIME_IS_HEADER_LIST (group), FALSE);

	return g_ptr_array_find (collection->array, group, NULL);
}

/**
 * g_mime_header_list_collection_remove:
 * @collection: a #GMimeHeaderListCollection
 * @group: (transfer none): the header group to remove
 *
 * Removes only the first occurrence of @group and notifies an owning entity.
 * Removing a group that is not present does not notify it.
 *
 * Returns: whether a group was removed.
 **/
gboolean
g_mime_header_list_collection_remove (GMimeHeaderListCollection *collection, GMimeHeaderList *group)
{
	guint index;

	g_return_val_if_fail (GMIME_IS_HEADER_LIST_COLLECTION (collection), FALSE);
	g_return_val_if_fail (GMIME_IS_HEADER_LIST (group), FALSE);

	if (!g_ptr_array_find (collection->array, group, &index))
		return FALSE;

	g_mime_event_remove (group->changed, (GMimeEventCallback) group_changed, collection);
	g_ptr_array_remove_index (collection->array, index);
	g_mime_event_emit (collection->changed, NULL);
	return TRUE;
}
