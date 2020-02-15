/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*  GMime
 *  Copyright (C) 2000-2020 Jeffrey Stedfast
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

static EventListener *
event_listener_new (GMimeEventCallback callback, gpointer user_data)
{
	EventListener *listener;
	
	listener = g_slice_new (EventListener);
	listener->user_data = user_data;
	listener->callback = callback;
	listener->blocked = 0;
	
	return listener;
}

static void
event_listener_free (EventListener *listener)
{
	g_slice_free (EventListener, listener);
}


struct _GMimeEvent {
	GPtrArray *array;
	gpointer owner;
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
	event->array = g_ptr_array_new ();
	event->owner = owner;
	
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
	guint i;
	
	for (i = 0; i < event->array->len; i++)
		event_listener_free (event->array->pdata[i]);
	g_ptr_array_free (event->array, TRUE);
	
	g_slice_free (GMimeEvent, event);
}


static int
g_mime_event_index_of (GMimeEvent *event, GMimeEventCallback callback, gpointer user_data)
{
	EventListener *listener;
	int i;
	
	for (i = 0; i < event->array->len; i++) {
		listener = (EventListener *) event->array->pdata[i];
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
	
	listener = (EventListener *) event->array->pdata[index];
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
	
	listener = (EventListener *) event->array->pdata[index];
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
	EventListener *listener;
	
	listener = event_listener_new (callback, user_data);
	g_ptr_array_add (event->array, listener);
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
	EventListener *listener;
	int index;
	
	if ((index = g_mime_event_index_of (event, callback, user_data)) == -1)
		return;
	
	listener = (EventListener *) event->array->pdata[index];
	g_ptr_array_remove_index (event->array, index);
	event_listener_free (listener);
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
	
	for (i = 0; i < event->array->len; i++) {
		listener = (EventListener *) event->array->pdata[i];
		if (listener->blocked <= 0)
			listener->callback (event->owner, args, listener->user_data);
	}
}
