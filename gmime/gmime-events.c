/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*  GMime
 *  Copyright (C) 2000-2022 Jeffrey Stedfast
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

#include "gmime-events.h"

typedef struct _EventListener {
	GMimeEventCallback callback;
	gpointer user_data;
	int blocked;
} EventListener;

struct _GMimeEvent {
	EventListener first;
	GArray *extra;
	gpointer owner;
	guint count;
};


/**
 * g_mime_event_new:
 * @owner: a pointer to the object owning this event
 *
 * Creates a new #GMimeEvent context.
 *
 * Returns: a newly allocated #GMimeEvent context.
 **/
GMimeEvent *
g_mime_event_new (gpointer owner)
{
	GMimeEvent *event;
	
	event = g_slice_new (GMimeEvent);
	event->extra = NULL;
	event->owner = owner;
	event->count = 0;
	
	return event;
}


/**
 * g_mime_event_free:
 * @event: a #GMimeEvent
 *
 * Frees an event context.
 **/
void
g_mime_event_free (GMimeEvent *event)
{
	g_clear_pointer (&event->extra, g_array_unref);
	g_slice_free (GMimeEvent, event);
}

static EventListener *
event_listener_at (GMimeEvent *event, guint index)
{
	return index ? &g_array_index (event->extra, EventListener, index - 1) : &event->first;
}


static int
g_mime_event_index_of (GMimeEvent *event, GMimeEventCallback callback, gpointer user_data)
{
	EventListener *listener;
	int i;
	
	for (i = 0; i < event->count; i++) {
		listener = event_listener_at (event, i);
		if (listener->callback == callback && listener->user_data == user_data)
			return i;
	}
	
	return -1;
}


/**
 * g_mime_event_block:
 * @event: a #GMimeEvent
 * @callback: a #GMimeEventCallback
 * @user_data: user context data
 *
 * Blocks the specified callback from being called when @event is emitted.
 **/
void
g_mime_event_block (GMimeEvent *event, GMimeEventCallback callback, gpointer user_data)
{
	EventListener *listener;
	int index;
	
	if ((index = g_mime_event_index_of (event, callback, user_data)) == -1)
		return;
	
	listener = event_listener_at (event, index);
	listener->blocked++;
}


/**
 * g_mime_event_unblock:
 * @event: a #GMimeEvent
 * @callback: a #GMimeEventCallback
 * @user_data: user context data
 *
 * Unblocks the specified callback from being called when @event is
 * emitted.
 **/
void
g_mime_event_unblock (GMimeEvent *event, GMimeEventCallback callback, gpointer user_data)
{
	EventListener *listener;
	int index;
	
	if ((index = g_mime_event_index_of (event, callback, user_data)) == -1)
		return;
	
	listener = event_listener_at (event, index);
	listener->blocked--;
}


/**
 * g_mime_event_add:
 * @event: a #GMimeEvent
 * @callback: a #GMimeEventCallback
 * @user_data: user context data
 *
 * Adds a callback function that will get called with the specified
 * @user_data whenever @event is emitted.
 **/
void
g_mime_event_add (GMimeEvent *event, GMimeEventCallback callback, gpointer user_data)
{
	EventListener listener = { callback, user_data, 0 };

	if (event->count == 0) {
		event->first = listener;
	} else {
		if (!event->extra)
			event->extra = g_array_new (FALSE, FALSE, sizeof (EventListener));
		g_array_append_val (event->extra, listener);
	}

	event->count++;
}


/**
 * g_mime_event_remove:
 * @event: a #GMimeEvent
 * @callback: a #GMimeEventCallback
 * @user_data: user context data
 *
 * Removes the specified callback function from the list of callbacks
 * that will be called when the @event is emitted.
 **/
void
g_mime_event_remove (GMimeEvent *event, GMimeEventCallback callback, gpointer user_data)
{
	int index;
	
	if ((index = g_mime_event_index_of (event, callback, user_data)) == -1)
		return;
	
	if (index == 0) {
		if (event->count > 1) {
			event->first = g_array_index (event->extra, EventListener, 0);
			g_array_remove_index (event->extra, 0);
		}
	} else {
		g_array_remove_index (event->extra, index - 1);
	}

	event->count--;
}


/**
 * g_mime_event_emit:
 * @event: a #GMimeEvent
 * @args: an argument pointer
 *
 * Calls each callback registered with this @event with the specified
 * @args.
 **/
void
g_mime_event_emit (GMimeEvent *event, gpointer args)
{
	EventListener *listener;
	guint i;
	
	for (i = 0; i < event->count; i++) {
		listener = event_listener_at (event, i);
		if (listener->blocked <= 0)
			listener->callback (event->owner, args, listener->user_data);
	}
}
