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

#include "gmime-message-disposition-notification.h"
#include "gmime-visitor.h"
#include "gmime-internal.h"

#include "gmime-object-private.h"

/**
 * SECTION: gmime-message-disposition-notification
 * @title: GMimeMessageDispositionNotification
 * @short_description: Message disposition notification parts
 * @see_also: #GMimePart, #GMimeMultipartReport, #GMimeHeaderList
 *
 * A machine-readable disposition notification with a lazily parsed header
 * list. Both message/disposition-notification and
 * message/global-disposition-notification use this class.
 **/

static void notification_finalize (GObject *object);
static void fields_changed (GMimeHeaderList *fields, gpointer args, GMimeMessageDispositionNotification *notification);


G_DEFINE_TYPE_WITH_PRIVATE (GMimeMessageDispositionNotification, g_mime_message_disposition_notification, GMIME_TYPE_PART)

GMimeMessageDispositionNotificationPrivate *
_g_mime_message_disposition_notification_get_private (GMimeMessageDispositionNotification *object)
{
	return g_mime_message_disposition_notification_get_instance_private (object);
}

static void
notification_accept (GMimeEntity *object, GMimeVisitor *visitor)
{
	g_mime_visitor_visit_message_disposition_notification (visitor, (GMimeMessageDispositionNotification *) object);
}

static void
g_mime_message_disposition_notification_class_init (GMimeMessageDispositionNotificationClass *klass)
{
	GMIME_ENTITY_CLASS (klass)->accept = notification_accept;
	G_OBJECT_CLASS (klass)->finalize = notification_finalize;
}

static void
g_mime_message_disposition_notification_init (GMimeMessageDispositionNotification *notification)
{
	GMimeMessageDispositionNotificationPrivate *priv = _g_mime_message_disposition_notification_get_private (notification);

	priv->fields = NULL;
}

static void
notification_finalize (GObject *object)
{
	GMimeMessageDispositionNotification *notification = GMIME_MESSAGE_DISPOSITION_NOTIFICATION (object);
	GMimeMessageDispositionNotificationPrivate *priv = _g_mime_message_disposition_notification_get_private (notification);

	if (priv->fields) {
		g_mime_event_remove (priv->fields->changed, (GMimeEventCallback) fields_changed, notification);
		g_object_unref (priv->fields);
	}

	G_OBJECT_CLASS (g_mime_message_disposition_notification_parent_class)->finalize (object);
}

static void
fields_changed (GMimeHeaderList *fields, gpointer args, GMimeMessageDispositionNotification *notification)
{
	/* Fields cannot be NULL while this handler is registered to their Changed event. */
	_g_mime_part_serialize_fields ((GMimePart *) notification, fields, "disposition notification");
}

/**
 * g_mime_message_disposition_notification_new:
 *
 * Creates an empty message/disposition-notification part. Notification
 * fields and an empty content stream are created on first field access.
 *
 * Returns: (transfer full): a new #GMimeMessageDispositionNotification.
 **/
GMimeMessageDispositionNotification *
g_mime_message_disposition_notification_new (void)
{
	GMimeMessageDispositionNotification *notification;
	GMimeContentType *content_type;

	notification = g_object_new (GMIME_TYPE_MESSAGE_DISPOSITION_NOTIFICATION, NULL);
	content_type = g_mime_content_type_new ("message", "disposition-notification");
	g_mime_entity_set_content_type ((GMimeEntity *) notification, content_type);
	g_object_unref (content_type);
	return notification;
}

/**
 * g_mime_message_disposition_notification_get_fields:
 * @notification: a #GMimeMessageDispositionNotification
 * @error: return location for an error, or %NULL
 *
 * Lazily loads the first header block of the decoded content using default
 * parser options. Existing content is not rewritten merely by reading fields.
 * With no content, an empty header list and content stream are created.
 *
 * List changes and contained-header changes replace the content using default
 * format options without changing the part's transfer-encoding header.
 * Once loaded, the same list is returned even if the inherited content is
 * replaced; subsequent list edits replace that content again.
 *
 * Rewind, read and parse errors are propagated without caching a partial
 * list. Content without a backing stream reports
 * %GMIME_ERROR_INVALID_OPERATION. Stream positioning follows
 * g_mime_content_open().
 *
 * Returns: (transfer none) (nullable): the notification fields, or %NULL on error.
 **/
GMimeHeaderList *
g_mime_message_disposition_notification_get_fields (GMimeMessageDispositionNotification *notification, GError **error)
{
	GMimeMessageDispositionNotificationPrivate *priv;
	GMimeHeaderList *fields;

	g_return_val_if_fail (GMIME_IS_MESSAGE_DISPOSITION_NOTIFICATION (notification), NULL);
	g_return_val_if_fail (error == NULL || *error == NULL, NULL);

	priv = _g_mime_message_disposition_notification_get_private (notification);

	if (priv->fields)
		return priv->fields;

	if (!(fields = _g_mime_part_load_fields ((GMimePart *) notification, "Disposition notification", error)))
		return NULL;

	priv->fields = fields;
	g_mime_event_add (fields->changed, (GMimeEventCallback) fields_changed, notification);

	return fields;
}
