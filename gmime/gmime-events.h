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


#ifndef __GMIME_EVENTS_H__
#define __GMIME_EVENTS_H__

#include <glib.h>

G_BEGIN_DECLS

typedef void (* GMimeEventCallback) (gpointer sender, gpointer args, gpointer user_data);

typedef struct _GMimeEvent GMimeEvent;

G_GNUC_INTERNAL GMimeEvent *g_mime_event_new (gpointer owner);
G_GNUC_INTERNAL void g_mime_event_destroy (GMimeEvent *event);

G_GNUC_INTERNAL void g_mime_event_add (GMimeEvent *event, GMimeEventCallback callback, gpointer user_data);
G_GNUC_INTERNAL void g_mime_event_remove (GMimeEvent *event, GMimeEventCallback callback, gpointer user_data);

G_GNUC_INTERNAL void g_mime_event_block (GMimeEvent *event, GMimeEventCallback callback, gpointer user_data);
G_GNUC_INTERNAL void g_mime_event_unblock (GMimeEvent *event, GMimeEventCallback callback, gpointer user_data);

G_GNUC_INTERNAL void g_mime_event_emit (GMimeEvent *event, gpointer args);

G_END_DECLS

#endif /* __GMIME_EVENTS_H__ */
