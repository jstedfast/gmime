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

#ifndef __GMIME_ITERATOR_H__
#define __GMIME_ITERATOR_H__

#include <gmime/gmime-message.h>

G_BEGIN_DECLS

#define GMIME_TYPE_ITERATOR (g_mime_iterator_get_type ())


/**
 * GMimeIterator:
 *
 * A depth-first iterator over a message body or an entity subtree.
 **/
G_DECLARE_FINAL_TYPE (GMimeIterator, g_mime_iterator, GMIME, ITERATOR, GObject)


GMimeIterator *g_mime_iterator_new (GMimeMessage *message);
GMimeIterator *g_mime_iterator_new_for_entity (GMimeEntity *entity);
GMimeMessage *g_mime_iterator_get_message (GMimeIterator *iter);
gboolean g_mime_iterator_is_valid (GMimeIterator *iter);
GMimeEntity *g_mime_iterator_get_current (GMimeIterator *iter);
GMimeEntity *g_mime_iterator_get_parent (GMimeIterator *iter);
char *g_mime_iterator_get_path_specifier (GMimeIterator *iter);
int g_mime_iterator_get_depth (GMimeIterator *iter);
int g_mime_iterator_get_index (GMimeIterator *iter);
GMimeMessage *g_mime_iterator_get_current_message (GMimeIterator *iter);
GMimeEntity *g_mime_iterator_get_ancestor (GMimeIterator *iter, int generation);
gboolean g_mime_iterator_move_next (GMimeIterator *iter);
void g_mime_iterator_skip_children (GMimeIterator *iter);
gboolean g_mime_iterator_move_to (GMimeIterator *iter, const char *path_specifier, GError **error);
void g_mime_iterator_reset (GMimeIterator *iter);

G_END_DECLS

#endif /* __GMIME_ITERATOR_H__ */
