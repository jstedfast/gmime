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


#ifndef __GMIME_MESSAGE_PART_H__
#define __GMIME_MESSAGE_PART_H__

#include <gmime/gmime-object.h>
#include <gmime/gmime-message.h>

G_BEGIN_DECLS

#define GMIME_TYPE_MESSAGE_PART            (g_mime_message_part_get_type ())
#define GMIME_MESSAGE_PART(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), GMIME_TYPE_MESSAGE_PART, GMimeMessagePart))
#define GMIME_MESSAGE_PART_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), GMIME_TYPE_MESSAGE_PART, GMimeMessagePartClass))
#define GMIME_IS_MESSAGE_PART(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GMIME_TYPE_MESSAGE_PART))
#define GMIME_IS_MESSAGE_PART_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), GMIME_TYPE_MESSAGE_PART))
#define GMIME_MESSAGE_PART_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), GMIME_TYPE_MESSAGE_PART, GMimeMessagePartClass))

typedef struct _GMimeMessagePart GMimeMessagePart;
typedef struct _GMimeMessagePartClass GMimeMessagePartClass;

/**
 * GMimeMessagePart:
 * @parent_object: parent #GMimeObject
 * @message: child #GMimeMessage
 *
 * A message/rfc822 or message/news MIME part.
 **/
struct _GMimeMessagePart {
	GMimeObject parent_object;
	
	GMimeMessage *message;
};

struct _GMimeMessagePartClass {
	GMimeObjectClass parent_class;
	
};


GType g_mime_message_part_get_type (void);

GMimeMessagePart *g_mime_message_part_new (const char *subtype);

GMimeMessagePart *g_mime_message_part_new_with_message (const char *subtype, GMimeMessage *message);

void g_mime_message_part_set_message (GMimeMessagePart *part, GMimeMessage *message);

GMimeMessage *g_mime_message_part_get_message (GMimeMessagePart *part);

G_END_DECLS

#endif /* __GMIME_MESSAGE_PART_H__ */
