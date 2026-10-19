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

#ifndef __GMIME_DOMAIN_LIST_H__
#define __GMIME_DOMAIN_LIST_H__

#include <glib-object.h>
#include <gmime/gmime-format-options.h>

G_BEGIN_DECLS

#define GMIME_TYPE_DOMAIN_LIST (g_mime_domain_list_get_type ())
#define GMIME_DOMAIN_LIST(obj) (G_TYPE_CHECK_INSTANCE_CAST ((obj), GMIME_TYPE_DOMAIN_LIST, GMimeDomainList))
#define GMIME_IS_DOMAIN_LIST(obj) (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GMIME_TYPE_DOMAIN_LIST))

typedef struct _GMimeDomainList GMimeDomainList;
typedef struct _GMimeDomainListClass GMimeDomainListClass;

/**
 * GMimeDomainList:
 * @parent_object: parent #GObject
 *
 * An ordered collection of source-route domains.
 **/
struct _GMimeDomainList {
	GObject parent_object;

	/* < private > */
	GPtrArray *array;
	gpointer changed;
};

struct _GMimeDomainListClass {
	GObjectClass parent_class;
};

GType g_mime_domain_list_get_type (void);
GMimeDomainList *g_mime_domain_list_new (void);
GMimeDomainList *g_mime_domain_list_clone (GMimeDomainList *list);
int g_mime_domain_list_length (GMimeDomainList *list);
void g_mime_domain_list_clear (GMimeDomainList *list);
int g_mime_domain_list_add (GMimeDomainList *list, const char *domain);
void g_mime_domain_list_insert (GMimeDomainList *list, int index, const char *domain);
gboolean g_mime_domain_list_remove (GMimeDomainList *list, const char *domain);
gboolean g_mime_domain_list_remove_at (GMimeDomainList *list, int index);
gboolean g_mime_domain_list_contains (GMimeDomainList *list, const char *domain);
int g_mime_domain_list_index_of (GMimeDomainList *list, const char *domain);
const char *g_mime_domain_list_get_domain (GMimeDomainList *list, int index);
void g_mime_domain_list_set_domain (GMimeDomainList *list, int index, const char *domain);
char *g_mime_domain_list_to_string (GMimeDomainList *list, GMimeFormatOptions *options, gboolean encode);
GMimeDomainList *g_mime_domain_list_parse (const char *str, gssize length, GError **error);

G_END_DECLS

#endif /* __GMIME_DOMAIN_LIST_H__ */
