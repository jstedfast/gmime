/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 *  Authors: Jeffrey Stedfast <fejj@ximian.com>
 *
 *  Copyright 2002-2004 Ximian, Inc. (www.ximian.com)
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


#ifndef __GMIME_MESSAGE_PART_H__
#define __GMIME_MESSAGE_PART_H__

#ifdef __cplusplus
extern "C" {
#pragma }
#endif /* __cplusplus */

#include <gmime/gmime-object.h>
#include <gmime/gmime-message.h>

#define GMIME_TYPE_MESSAGE_PART            (g_mime_message_part_get_type ())
#define GMIME_MESSAGE_PART(obj)            (GMIME_CHECK_CAST ((obj), GMIME_TYPE_MESSAGE_PART, GMimeMessagePart))
#define GMIME_MESSAGE_PART_CLASS(klass)    (GMIME_CHECK_CLASS_CAST ((klass), GMIME_TYPE_MESSAGE_PART, GMimeMessagePartClass))
#define GMIME_IS_MESSAGE_PART(obj)         (GMIME_CHECK_TYPE ((obj), GMIME_TYPE_MESSAGE_PART))
#define GMIME_IS_MESSAGE_PART_CLASS(klass) (GMIME_CHECK_CLASS_TYPE ((klass), GMIME_TYPE_MESSAGE_PART))
#define GMIME_MESSAGE_PART_GET_CLASS(obj)  (GMIME_CHECK_GET_CLASS ((obj), GMIME_TYPE_MESSAGE_PART, GMimeMessagePartClass))

typedef struct _GMimeMessagePart GMimeMessagePart;
typedef struct _GMimeMessagePartClass GMimeMessagePartClass;

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

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* __GMIME_MESSAGE_PART_H__ */
