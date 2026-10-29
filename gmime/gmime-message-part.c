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

#include <string.h>

#include "gmime-message-part.h"
#include "gmime-internal.h"
#include "gmime-visitor.h"

#define d(x)


#include "gmime-object-private.h"

/**
 * SECTION: gmime-message-part
 * @title: GMimeMessagePart
 * @short_description: Message parts
 * @see_also:
 *
 * A #GMimeMessagePart represents message/rfc822 and message/news MIME
 * parts.
 **/


/* GObject class methods */
static void g_mime_message_part_class_init (GMimeMessagePartClass *klass);
static void g_mime_message_part_init (GMimeMessagePart *message_part);
static void g_mime_message_part_finalize (GObject *object);

/* GMimeEntity class methods */
static ssize_t message_part_write_to_stream (GMimeEntity *object, GMimeFormatOptions *options,
					     gboolean content_only, GMimeStream *stream);
static gboolean message_part_prepare (GMimeEntity *object, GMimeEncodingConstraint constraint,
				      guint max_line_length, GError **error);


static GMimeEntityClass *parent_class = NULL;

static void
message_part_accept (GMimeEntity *object, GMimeVisitor *visitor)
{
	g_mime_visitor_visit_message_part (visitor, (GMimeMessagePart *) object);
}



G_DEFINE_TYPE_WITH_PRIVATE (GMimeMessagePart, g_mime_message_part, GMIME_TYPE_ENTITY)

GMimeMessagePartPrivate *
_g_mime_message_part_get_private (GMimeMessagePart *object)
{
	return g_mime_message_part_get_instance_private (object);
}


static void
g_mime_message_part_class_init (GMimeMessagePartClass *klass)
{
	GMimeEntityClass *object_class = GMIME_ENTITY_CLASS (klass);
	GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
	
	parent_class = g_type_class_ref (GMIME_TYPE_ENTITY);
	
	gobject_class->finalize = g_mime_message_part_finalize;
	
	object_class->write_to_stream = message_part_write_to_stream;
	object_class->accept = message_part_accept;
	object_class->prepare_with_max_line_length = message_part_prepare;
}

static void
g_mime_message_part_init (GMimeMessagePart *part)
{
	GMimeMessagePartPrivate *priv = _g_mime_message_part_get_private (part);

	priv->message = NULL;
}

static void
g_mime_message_part_finalize (GObject *object)
{
	GMimeMessagePart *part = (GMimeMessagePart *) object;
	GMimeMessagePartPrivate *priv = _g_mime_message_part_get_private (part);

	if (priv->message)
		g_object_unref (priv->message);
	
	G_OBJECT_CLASS (parent_class)->finalize (object);
}

static gboolean
message_part_prepare (GMimeEntity *object, GMimeEncodingConstraint constraint, guint max_line_length, GError **error)
{
	GMimeMessagePart *part = (GMimeMessagePart *) object;

	/* An absent encapsulated message is an empty container, not leaf data
	 * to analyze or an outer Content-Transfer-Encoding to rewrite. */
	GMimeMessagePartPrivate *priv = _g_mime_message_part_get_private (part);

	return !priv->message ||
		g_mime_entity_prepare_with_max_line_length (GMIME_ENTITY (priv->message), constraint, max_line_length, error);
}


static ssize_t
message_part_write_to_stream (GMimeEntity *object, GMimeFormatOptions *options, gboolean content_only, GMimeStream *stream)
{
	return _g_mime_writer_write (object, options, content_only, stream);
}


/**
 * g_mime_message_part_new:
 * @subtype: message subtype or %NULL for "rfc822"
 *
 * Creates a new MIME message part object with a default content-type
 * of message/@subtype.
 *
 * Returns: an empty MIME message part object with a default
 * content-type of message/@subtype.
 **/
GMimeMessagePart *
g_mime_message_part_new (const char *subtype)
{
	GMimeContentType *content_type;
	GMimeMessagePart *part;
	
	part = g_object_new (GMIME_TYPE_MESSAGE_PART, NULL);
	
	content_type = g_mime_content_type_new ("message", subtype ? subtype : "rfc822");
	g_mime_entity_set_content_type ((GMimeEntity *) part, content_type);
	g_object_unref (content_type);
	
	return part;
}


/**
 * g_mime_message_part_new_with_message:
 * @subtype: message subtype or %NULL for "rfc822"
 * @message: message
 *
 * Creates a new MIME message part object with a default content-type
 * of message/@subtype containing @message.
 *
 * Returns: a MIME message part object with a default content-type of
 * message/@subtype containing @message.
 **/
GMimeMessagePart *
g_mime_message_part_new_with_message (const char *subtype, GMimeMessage *message)
{
	GMimeMessagePartPrivate *priv;
	GMimeMessagePart *part;
	
	part = g_mime_message_part_new (subtype);
	priv = _g_mime_message_part_get_private (part);

	priv->message = message;
	g_object_ref (message);
	
	return part;
}


/**
 * g_mime_message_part_set_message:
 * @part: message part
 * @message: (nullable): message
 *
 * Sets the @message object on the message part object @part.
 **/
void
g_mime_message_part_set_message (GMimeMessagePart *part, GMimeMessage *message)
{
	GMimeMessagePartPrivate *priv;

	g_return_if_fail (GMIME_IS_MESSAGE_PART (part));
	
	if (message)
		g_object_ref (message);
	
	priv = _g_mime_message_part_get_private (part);

	if (priv->message)
		g_object_unref (priv->message);
	
	priv->message = message;
}


/**
 * g_mime_message_part_get_message:
 * @part: message part
 *
 * Gets the message object on the message part object @part.
 *
 * Returns: (transfer none) (nullable): the message contained within @part.
 **/
GMimeMessage *
g_mime_message_part_get_message (GMimeMessagePart *part)
{
	GMimeMessagePartPrivate *priv;

	g_return_val_if_fail (GMIME_IS_MESSAGE_PART (part), NULL);
	
	priv = _g_mime_message_part_get_private (part);

	return priv->message;
}
