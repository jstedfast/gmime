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


#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "gmime-events.h"
#include "list.h"

typedef struct _EventListener {
	struct _EventListener *next;
	struct _EventListener *prev;
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
	listener->prev = NULL;
	listener->next = NULL;
	listener->blocked = 0;
	
	return listener;
}

static void
event_listener_free (EventListener *listener)
{
	g_slice_free (EventListener, listener);
}


struct _GMimeEvent {
	gpointer owner;
	List list;
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
	list_init (&event->list);
	event->owner = owner;
	
	return event;
}


/**
 * g_mime_event_destroy:
 * @event: a #GMimeEvent
 *
 * Destroys an event context.
 **/
void
g_mime_event_destroy (GMimeEvent *event)
{
	EventListener *node, *next;
	
	node = (EventListener *) event->list.head;
	while (node->next) {
		next = node->next;
		event_listener_free (node);
		node = next;
	}
	
	g_slice_free (GMimeEvent, event);
}


static EventListener *
g_mime_event_find_listener (GMimeEvent *event, GMimeEventCallback callback, gpointer user_data)
{
	EventListener *node;
	
	node = (EventListener *) event->list.head;
	while (node->next) {
		if (node->callback == callback && node->user_data == user_data)
			return node;
		node = node->next;
	}
	
	return NULL;
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
	
	if ((listener = g_mime_event_find_listener (event, callback, user_data)))
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
	
	if ((listener = g_mime_event_find_listener (event, callback, user_data)))
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
	list_append (&event->list, (ListNode *) listener);
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
	
	if ((listener = g_mime_event_find_listener (event, callback, user_data))) {
		list_unlink ((ListNode *) listener);
		event_listener_free (listener);
	}
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
	EventListener *node;
	
	node = (EventListener *) event->list.head;
	while (node->next) {
		if (node->blocked <= 0)
			node->callback (event->owner, args, node->user_data);
		node = node->next;
	}
}
