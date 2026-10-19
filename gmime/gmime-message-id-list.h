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


#ifndef __GMIME_MESSAGE_ID_LIST_H__
#define __GMIME_MESSAGE_ID_LIST_H__

#include <glib-object.h>
#include <gmime/gmime-parser-options.h>

G_BEGIN_DECLS

#define GMIME_TYPE_MESSAGE_ID_LIST (g_mime_message_id_list_get_type ())
#define GMIME_MESSAGE_ID_LIST(obj) (G_TYPE_CHECK_INSTANCE_CAST ((obj), GMIME_TYPE_MESSAGE_ID_LIST, GMimeMessageIdList))
#define GMIME_IS_MESSAGE_ID_LIST(obj) (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GMIME_TYPE_MESSAGE_ID_LIST))

typedef struct _GMimeMessageIdList GMimeMessageIdList;
typedef struct _GMimeMessageIdListClass GMimeMessageIdListClass;

/**
 * GMimeMessageIdList:
 * @parent_object: parent #GObject
 *
 * An ordered, mutable collection of message identifiers.
 * Changes propagate to the owning message's References header.
 **/
struct _GMimeMessageIdList {
	GObject parent_object;

	/* <private> */
	GPtrArray *array;
	gpointer changed;
};

struct _GMimeMessageIdListClass {
	GObjectClass parent_class;
};

GType g_mime_message_id_list_get_type (void);
GMimeMessageIdList *g_mime_message_id_list_new (void);
GMimeMessageIdList *g_mime_message_id_list_clone (GMimeMessageIdList *list);
GMimeMessageIdList *g_mime_message_id_list_parse (GMimeParserOptions *options, const char *text);
int g_mime_message_id_list_length (GMimeMessageIdList *list);
void g_mime_message_id_list_clear (GMimeMessageIdList *list);
int g_mime_message_id_list_add (GMimeMessageIdList *list, const char *message_id);
void g_mime_message_id_list_add_range (GMimeMessageIdList *list, const char * const *message_ids, guint length);
void g_mime_message_id_list_insert (GMimeMessageIdList *list, int index, const char *message_id);
gboolean g_mime_message_id_list_remove (GMimeMessageIdList *list, const char *message_id);
gboolean g_mime_message_id_list_remove_at (GMimeMessageIdList *list, int index);
gboolean g_mime_message_id_list_contains (GMimeMessageIdList *list, const char *message_id);
int g_mime_message_id_list_index_of (GMimeMessageIdList *list, const char *message_id);
const char *g_mime_message_id_list_get_message_id (GMimeMessageIdList *list, int index);
void g_mime_message_id_list_set_message_id (GMimeMessageIdList *list, int index, const char *message_id);
char *g_mime_message_id_list_to_string (GMimeMessageIdList *list);

G_END_DECLS

#endif /* __GMIME_MESSAGE_ID_LIST_H__ */
