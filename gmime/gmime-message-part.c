/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 *  Authors: Jeffrey Stedfast <fejj@ximian.com>
 *
 *  Copyright 2002 Ximian, Inc. (www.ximian.com)
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Street #330, Boston, MA 02111-1307, USA.
 *
 */


#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <string.h>

#include "gmime-message-part.h"

#define d(x) x

/* GObject class methods */
static void g_mime_message_part_class_init (GMimeMessagePartClass *klass);
static void g_mime_message_part_init (GMimeMessagePart *message_part, GMimeMessagePartClass *klass);
static void g_mime_message_part_finalize (GObject *object);

/* GMimeObject class methods */
static void message_part_init (GMimeObject *object);
static void message_part_add_header (GMimeObject *object, const char *header, const char *value);
static void message_part_set_header (GMimeObject *object, const char *header, const char *value);
static const char *message_part_get_header (GMimeObject *object, const char *header);
static void message_part_remove_header (GMimeObject *object, const char *header);
static void message_part_set_content_type (GMimeObject *object, GMimeContentType *content_type);
static char *message_part_get_headers (GMimeObject *object);
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
			16,   /* n_preallocs */
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
	
	object_class->init = message_part_init;
	object_class->add_header = message_part_add_header;
	object_class->set_header = message_part_set_header;
	object_class->get_header = message_part_get_header;
	object_class->remove_header = message_part_remove_header;
	object_class->set_content_type = message_part_set_content_type;
	object_class->get_headers = message_part_get_headers;
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
		g_mime_object_unref (GMIME_OBJECT (part->message));
	
	G_OBJECT_CLASS (parent_class)->finalize (object);
}


static void
message_part_init (GMimeObject *object)
{
	/* no-op */
	GMIME_OBJECT_CLASS (parent_class)->init (object);
}

static void
message_part_add_header (GMimeObject *object, const char *header, const char *value)
{
	/* Make sure that the header is a Content-* header, else it
           doesn't belong on a message part */
	
	if (!strncasecmp ("Content-", header, 8))
		GMIME_OBJECT_CLASS (parent_class)->add_header (object, header, value);
}

static void
message_part_set_header (GMimeObject *object, const char *header, const char *value)
{
	/* RFC 1864 states that you cannot set a Content-MD5 on a message part */
	if (!strcasecmp ("Content-MD5", header))
		return;
	
	/* Make sure that the header is a Content-* header, else it
           doesn't belong on a message part */
	
	if (!strncasecmp ("Content-", header, 8))
		GMIME_OBJECT_CLASS (parent_class)->set_header (object, header, value);
}

static const char *
message_part_get_header (GMimeObject *object, const char *header)
{
	/* Make sure that the header is a Content-* header, else it
           doesn't belong on a message part */
	
	if (!strncasecmp ("Content-", header, 8))
		return GMIME_OBJECT_CLASS (parent_class)->get_header (object, header);
	else
		return NULL;
}

static void
message_part_remove_header (GMimeObject *object, const char *header)
{
	/* Make sure that the header is a Content-* header, else it
           doesn't belong on a message part */
	
	if (!strncasecmp ("Content-", header, 8))
		GMIME_OBJECT_CLASS (parent_class)->remove_header (object, header);
}

static void
message_part_set_content_type (GMimeObject *object, GMimeContentType *content_type)
{
	/* nothing special here... */
	GMIME_OBJECT_CLASS (parent_class)->set_content_type (object, content_type);
}

static char *
message_part_get_headers (GMimeObject *object)
{
	return GMIME_OBJECT_CLASS (parent_class)->get_headers (object);
}

static ssize_t
message_part_write_to_stream (GMimeObject *object, GMimeStream *stream)
{
	GMimeMessagePart *part = (GMimeMessagePart *) object;
	ssize_t nwritten, total = 0;
	
	/* write the content headers */
	nwritten = g_mime_header_write_to_stream (object->headers, stream);
	if (nwritten == -1)
		return -1;
	
	total += nwritten;
	
	/* terminate the headers */
	nwritten = g_mime_stream_write (stream, "\n", 1);
	if (nwritten == -1)
		return -1;
	
	total += nwritten;
	
	/* write the message */
	if (part->message) {
		nwritten = g_mime_object_write_to_stream (GMIME_OBJECT (part->message), stream);
		if (nwritten == -1)
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
 * Returns an empty MIME message part object with a default
 * content-type of message/@subtype.
 **/
GMimeMessagePart *
g_mime_message_part_new (const char *subtype)
{
	GMimeMessagePart *part;
	GMimeContentType *type;
	
	part = g_object_new (GMIME_TYPE_MESSAGE_PART, NULL, NULL);
	
	type = g_mime_content_type_new ("message", subtype ? subtype : "rfc822");
	g_mime_object_set_content_type (GMIME_OBJECT (part), type);
	
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
 * Returns a MIME message part object with a default content-type of
 * message/@subtype containing @message.
 **/
GMimeMessagePart *
g_mime_message_part_new_with_message (const char *subtype, GMimeMessage *message)
{
	GMimeMessagePart *part;
	
	part = g_mime_message_part_new (subtype);
	part->message = message;
	g_mime_object_ref (GMIME_OBJECT (message));
	
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
		g_mime_object_ref (GMIME_OBJECT (message));
	
	if (part->message)
		g_mime_object_unref (GMIME_OBJECT (part->message));
	
	part->message = message;
}


/**
 * g_mime_message_part_get_message:
 * @part: message part
 *
 * Gets the message object on the message part object @part.
 *
 * Returns the message part contained within @part.
 **/
GMimeMessage *
g_mime_message_part_get_message (GMimeMessagePart *part)
{
	g_return_val_if_fail (GMIME_IS_MESSAGE_PART (part), NULL);
	
	return part->message;
}
