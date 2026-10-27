/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*  GMime
 *  Copyright (C) 2000-2026 Jeffrey Stedfast
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

#include <string.h>

#include "gmime-message-id-list.h"
#include "gmime-parse-utils.h"
#include "gmime-events.h"

/**
 * SECTION: gmime-message-id-list
 * @title: GMimeMessageIdList
 * @short_description: Mutable message identifier lists
 *
 * An ordered collection of message identifiers. Mutations validate and
 * normalize complete IDs; parsing recovers IDs from malformed header text.
 **/

static void g_mime_message_id_list_finalize (GObject *object);

G_DEFINE_TYPE (GMimeMessageIdList, g_mime_message_id_list, G_TYPE_OBJECT)

static void
g_mime_message_id_list_class_init (GMimeMessageIdListClass *klass)
{
	G_OBJECT_CLASS (klass)->finalize = g_mime_message_id_list_finalize;
}

static void
g_mime_message_id_list_init (GMimeMessageIdList *list)
{
	list->array = g_ptr_array_new_with_free_func (g_free);
	list->changed = g_mime_event_new (list);
}

static void
g_mime_message_id_list_finalize (GObject *object)
{
	GMimeMessageIdList *list = GMIME_MESSAGE_ID_LIST (object);

	g_mime_event_free (list->changed);
	g_ptr_array_unref (list->array);
	G_OBJECT_CLASS (g_mime_message_id_list_parent_class)->finalize (object);
}

/**
 * g_mime_message_id_list_new:
 *
 * Returns: (transfer full): a new empty message identifier list.
 **/
GMimeMessageIdList *
g_mime_message_id_list_new (void)
{
	return g_object_new (GMIME_TYPE_MESSAGE_ID_LIST, NULL);
}

/**
 * g_mime_message_id_list_clone:
 * @list: a #GMimeMessageIdList
 *
 * Returns: (transfer full): an independent copy, preserving parsed IDs
 * without revalidating or renormalizing them.
 **/
GMimeMessageIdList *
g_mime_message_id_list_clone (GMimeMessageIdList *list)
{
	GMimeMessageIdList *clone;
	guint i;

	g_return_val_if_fail (GMIME_IS_MESSAGE_ID_LIST (list), NULL);

	clone = g_mime_message_id_list_new ();
	for (i = 0; i < list->array->len; i++)
		g_ptr_array_add (clone->array, g_strdup (list->array->pdata[i]));

	return clone;
}

/**
 * g_mime_message_id_list_parse:
 * @options: (nullable): a #GMimeParserOptions or %NULL
 * @text: a NUL-terminated raw References or In-Reply-To header value
 *
 * Recovers bracketed message identifiers, skipping unrelated text and
 * malformed IDs. Parsing does not apply property-assignment validation.
 *
 * Returns: (transfer full): a new list of recovered identifiers.
 **/
GMimeMessageIdList *
g_mime_message_id_list_parse (GMimeParserOptions *options, const char *text)
{
	GMimeMessageIdList *list;
	char *id;

	g_return_val_if_fail (text != NULL, NULL);

	list = g_mime_message_id_list_new ();
	while ((id = _g_mime_parse_reference (&text)))
		g_ptr_array_add (list->array, id);

	return list;
}

/**
 * g_mime_message_id_list_length:
 * @list: a #GMimeMessageIdList
 *
 * Returns: the number of message identifiers.
 **/
int
g_mime_message_id_list_length (GMimeMessageIdList *list)
{
	g_return_val_if_fail (GMIME_IS_MESSAGE_ID_LIST (list), 0);

	return list->array->len;
}

/**
 * g_mime_message_id_list_clear:
 * @list: a #GMimeMessageIdList
 *
 * Removes all identifiers and notifies the owner even when already empty.
 **/
void
g_mime_message_id_list_clear (GMimeMessageIdList *list)
{
	g_return_if_fail (GMIME_IS_MESSAGE_ID_LIST (list));

	g_ptr_array_set_size (list->array, 0);
	g_mime_event_emit (list->changed, NULL);
}

/**
 * g_mime_message_id_list_add:
 * @list: a #GMimeMessageIdList
 * @message_id: a UTF-8 message-id, with or without angle brackets
 *
 * Appends a normalized identifier. Invalid IDs are programming errors and
 * leave the list unchanged.
 *
 * Returns: the added identifier's index, or -1 for invalid arguments.
 **/
int
g_mime_message_id_list_add (GMimeMessageIdList *list, const char *message_id)
{
	char *id;
	int index;

	g_return_val_if_fail (GMIME_IS_MESSAGE_ID_LIST (list), -1);
	g_return_val_if_fail (message_id != NULL, -1);

	id = _g_mime_parse_message_id_value (message_id);
	g_return_val_if_fail (id != NULL, -1);

	index = list->array->len;
	g_ptr_array_add (list->array, id);
	g_mime_event_emit (list->changed, NULL);

	return index;
}

/**
 * g_mime_message_id_list_add_range:
 * @list: a #GMimeMessageIdList
 * @message_ids: (array length=length) (element-type utf8) (nullable): identifiers to append
 * @length: the number of identifiers
 *
 * Validates every identifier before appending any of them. Invalid entries
 * are programming errors and leave the list unchanged. Input strings may
 * alias the list. A successful call notifies the owner once, even for an
 * empty range.
 **/
void
g_mime_message_id_list_add_range (GMimeMessageIdList *list, const char * const *message_ids, guint length)
{
	GPtrArray *ids;
	char *id;
	guint i;

	g_return_if_fail (GMIME_IS_MESSAGE_ID_LIST (list));
	g_return_if_fail (message_ids != NULL || length == 0);
	g_return_if_fail (length <= G_MAXINT && list->array->len <= G_MAXINT - length);

	ids = g_ptr_array_new_with_free_func (g_free);

	for (i = 0; i < length; i++) {
		id = message_ids[i] ? _g_mime_parse_message_id_value (message_ids[i]) : NULL;

		if (!id) {
			g_ptr_array_unref (ids);
			g_return_if_fail (id != NULL);
			return;
		}

		g_ptr_array_add (ids, id);
	}

	g_ptr_array_extend_and_steal (list->array, ids);
	g_mime_event_emit (list->changed, NULL);
}

/**
 * g_mime_message_id_list_insert:
 * @list: a #GMimeMessageIdList
 * @index: an index between zero and the list length, inclusive
 * @message_id: a UTF-8 message-id, with or without angle brackets
 *
 * Inserts a validated, normalized identifier.
 **/
void
g_mime_message_id_list_insert (GMimeMessageIdList *list, int index, const char *message_id)
{
	char *id;

	g_return_if_fail (GMIME_IS_MESSAGE_ID_LIST (list));
	g_return_if_fail (index >= 0 && (guint) index <= list->array->len);
	g_return_if_fail (message_id != NULL);

	id = _g_mime_parse_message_id_value (message_id);
	g_return_if_fail (id != NULL);

	g_ptr_array_insert (list->array, index, id);
	g_mime_event_emit (list->changed, NULL);
}

/**
 * g_mime_message_id_list_index_of:
 * @list: a #GMimeMessageIdList
 * @message_id: the exact identifier to find
 *
 * Does not normalize the search value.
 *
 * Returns: the first case-sensitive match, or -1.
 **/
int
g_mime_message_id_list_index_of (GMimeMessageIdList *list, const char *message_id)
{
	guint i;

	g_return_val_if_fail (GMIME_IS_MESSAGE_ID_LIST (list), -1);
	g_return_val_if_fail (message_id != NULL, -1);

	for (i = 0; i < list->array->len; i++) {
		if (!strcmp (list->array->pdata[i], message_id))
			return i;
	}

	return -1;
}

/**
 * g_mime_message_id_list_contains:
 * @list: a #GMimeMessageIdList
 * @message_id: the exact identifier to find
 *
 * Returns: whether the list contains a case-sensitive match.
 **/
gboolean
g_mime_message_id_list_contains (GMimeMessageIdList *list, const char *message_id)
{
	return g_mime_message_id_list_index_of (list, message_id) != -1;
}

/**
 * g_mime_message_id_list_remove_at:
 * @list: a #GMimeMessageIdList
 * @index: an existing index
 *
 * Returns: whether an identifier was removed, or %FALSE for invalid arguments.
 **/
gboolean
g_mime_message_id_list_remove_at (GMimeMessageIdList *list, int index)
{
	g_return_val_if_fail (GMIME_IS_MESSAGE_ID_LIST (list), FALSE);
	g_return_val_if_fail (index >= 0 && (guint) index < list->array->len, FALSE);

	g_ptr_array_remove_index (list->array, index);
	g_mime_event_emit (list->changed, NULL);

	return TRUE;
}

/**
 * g_mime_message_id_list_remove:
 * @list: a #GMimeMessageIdList
 * @message_id: the exact identifier to remove
 *
 * Returns: whether the first case-sensitive match was removed.
 **/
gboolean
g_mime_message_id_list_remove (GMimeMessageIdList *list, const char *message_id)
{
	int index = g_mime_message_id_list_index_of (list, message_id);

	return index != -1 && g_mime_message_id_list_remove_at (list, index);
}

/**
 * g_mime_message_id_list_get_message_id:
 * @list: a #GMimeMessageIdList
 * @index: an existing index
 *
 * Returns: (nullable) (transfer none): the identifier, or %NULL for invalid arguments.
 **/
const char *
g_mime_message_id_list_get_message_id (GMimeMessageIdList *list, int index)
{
	g_return_val_if_fail (GMIME_IS_MESSAGE_ID_LIST (list), NULL);
	g_return_val_if_fail (index >= 0 && (guint) index < list->array->len, NULL);

	return list->array->pdata[index];
}

/**
 * g_mime_message_id_list_set_message_id:
 * @list: a #GMimeMessageIdList
 * @index: an existing index
 * @message_id: a UTF-8 message-id, with or without angle brackets
 *
 * Replaces an identifier after validation. Assigning the same exact or
 * normalized ID preserves the current entry and does not notify the owner.
 **/
void
g_mime_message_id_list_set_message_id (GMimeMessageIdList *list, int index, const char *message_id)
{
	char *id;

	g_return_if_fail (GMIME_IS_MESSAGE_ID_LIST (list));
	g_return_if_fail (index >= 0 && (guint) index < list->array->len);
	g_return_if_fail (message_id != NULL);

	if (!strcmp (list->array->pdata[index], message_id))
		return;

	id = _g_mime_parse_message_id_value (message_id);
	g_return_if_fail (id != NULL);

	if (!strcmp (list->array->pdata[index], id)) {
		g_free (id);
		return;
	}

	g_free (list->array->pdata[index]);
	list->array->pdata[index] = id;
	g_mime_event_emit (list->changed, NULL);
}

/**
 * g_mime_message_id_list_to_string:
 * @list: a #GMimeMessageIdList
 *
 * Surrounds each identifier with angle brackets and separates them with
 * one space, without folding or a trailing newline.
 *
 * Returns: (transfer full): the formatted list.
 **/
char *
g_mime_message_id_list_to_string (GMimeMessageIdList *list)
{
	GString *str;
	guint i;

	g_return_val_if_fail (GMIME_IS_MESSAGE_ID_LIST (list), NULL);

	str = g_string_new (NULL);
	for (i = 0; i < list->array->len; i++) {
		if (i)
			g_string_append_c (str, ' ');
		g_string_append_c (str, '<');
		g_string_append (str, list->array->pdata[i]);
		g_string_append_c (str, '>');
	}

	return g_string_free (str, FALSE);
}
