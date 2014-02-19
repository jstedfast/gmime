/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*  GMime
 *  Copyright (C) 2000-2014 Jeffrey Stedfast
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


#ifndef __GMIME_PART_ITER_H__
#define __GMIME_PART_ITER_H__

#include <gmime/gmime-object.h>

G_BEGIN_DECLS

/**
 * GMimePartIter:
 *
 * A MIME part iterator.
 **/
typedef struct _GMimePartIter GMimePartIter;

GMimePartIter *g_mime_part_iter_new (GMimeObject *toplevel);
void g_mime_part_iter_free (GMimePartIter *iter);

void g_mime_part_iter_reset (GMimePartIter *iter);

gboolean g_mime_part_iter_jump_to (GMimePartIter *iter, const char *path);

gboolean g_mime_part_iter_is_valid (GMimePartIter *iter);

gboolean g_mime_part_iter_next (GMimePartIter *iter);
gboolean g_mime_part_iter_prev (GMimePartIter *iter);

GMimeObject *g_mime_part_iter_get_toplevel (GMimePartIter *iter);
GMimeObject *g_mime_part_iter_get_current (GMimePartIter *iter);
GMimeObject *g_mime_part_iter_get_parent (GMimePartIter *iter);
char *g_mime_part_iter_get_path (GMimePartIter *iter);

gboolean g_mime_part_iter_replace (GMimePartIter *iter, GMimeObject *replacement);
gboolean g_mime_part_iter_remove (GMimePartIter *iter);

G_END_DECLS

#endif /* __GMIME_PART_ITER_H__ */
