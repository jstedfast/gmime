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

#include <string.h>

#include "gmime-message-part.h"

#define d(x)


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
static void g_mime_message_part_init (GMimeMessagePart *message_part, GMimeMessagePartClass *klass);
static void g_mime_message_part_finalize (GObject *object);

/* GMimeObject class methods */
static ssize_t message_part_write_to_stream (GMimeObject *object, GMimeStream *stream);


static GMimeObjectClass *parent_class = NULL;


GType
g_mime_message_part_get_type (void)
{
	static GType type = 0;
	
	if (!type) {
		static const GTypeInfo info = {
			sizeof (GMimeMessagePartClass),
			NULL, /* base_class_init */
			NULL, /* base_class_finalize */
			(GClassInitFunc) g_mime_message_part_class_init,
			NULL, /* class_finalize */
			NULL, /* class_data */
			sizeof (GMimeMessagePart),
			0,    /* n_preallocs */
			(GInstanceInitFunc) g_mime_message_part_init,
		};
		
		type = g_type_register_static (GMIME_TYPE_OBJECT, "GMimeMessagePart", &info, 0);
	}
	
	return type;
}


static void
g_mime_message_part_class_init (GMimeMessagePartClass *klass)
{
	GMimeObjectClass *object_class = GMIME_OBJECT_CLASS (klass);
	GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
	
	parent_class = g_type_class_ref (GMIME_TYPE_OBJECT);
	
	gobject_class->finalize = g_mime_message_part_finalize;
	
	object_class->write_to_stream = message_part_write_to_stream;
}

static void
g_mime_message_part_init (GMimeMessagePart *part, GMimeMessagePartClass *klass)
{
	part->message = NULL;
}

static void
g_mime_message_part_finalize (GObject *object)
{
	GMimeMessagePart *part = (GMimeMessagePart *) object;
	
	if (part->message)
		g_object_unref (part->message);
	
	G_OBJECT_CLASS (parent_class)->finalize (object);
}

static ssize_t
message_part_write_to_stream (GMimeObject *object, GMimeStream *stream)
{
	GMimeMessagePart *part = (GMimeMessagePart *) object;
	ssize_t nwritten, total = 0;
	
	/* write the content headers */
	if ((nwritten = g_mime_header_list_write_to_stream (object->headers, stream)) == -1)
		return -1;
	
	total += nwritten;
	
	/* terminate the headers */
	if ((nwritten = g_mime_stream_write (stream, "\n", 1)) == -1)
		return -1;
	
	total += nwritten;
	
	/* write the message */
	if (part->message) {
		if ((nwritten = g_mime_object_write_to_stream (GMIME_OBJECT (part->message), stream)) == -1)
			return -1;
		
		total += nwritten;
	}
	
	return total;
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
	
	part = g_object_newv (GMIME_TYPE_MESSAGE_PART, 0, NULL);
	
	content_type = g_mime_content_type_new ("message", subtype ? subtype : "rfc822");
	g_mime_object_set_content_type (GMIME_OBJECT (part), content_type);
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
	GMimeMessagePart *part;
	
	part = g_mime_message_part_new (subtype);
	part->message = message;
	g_object_ref (message);
	
	return part;
}


/**
 * g_mime_message_part_set_message:
 * @part: message part
 * @message: message
 *
 * Sets the @message object on the message part object @part.
 **/
void
g_mime_message_part_set_message (GMimeMessagePart *part, GMimeMessage *message)
{
	g_return_if_fail (GMIME_IS_MESSAGE_PART (part));
	
	if (message)
		g_object_ref (message);
	
	if (part->message)
		g_object_unref (part->message);
	
	part->message = message;
}


/**
 * g_mime_message_part_get_message:
 * @part: message part
 *
 * Gets the message object on the message part object @part.
 *
 * Returns: (transfer none): the message part contained within @part.
 **/
GMimeMessage *
g_mime_message_part_get_message (GMimeMessagePart *part)
{
	g_return_val_if_fail (GMIME_IS_MESSAGE_PART (part), NULL);
	
	return part->message;
}
