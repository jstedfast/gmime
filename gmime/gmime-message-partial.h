/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*  GMime
 *  Copyright (C) 1999-2006 Jeffrey Stedfast
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
 */


#ifndef __GMIME_MESSAGE_PARTIAL_H__
#define __GMIME_MESSAGE_PARTIAL_H__

#include <glib.h>

#include <gmime/gmime-part.h>
#include <gmime/gmime-message.h>

G_BEGIN_DECLS

#define GMIME_TYPE_MESSAGE_PARTIAL            (g_mime_message_partial_get_type ())
#define GMIME_MESSAGE_PARTIAL(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), GMIME_TYPE_MESSAGE_PARTIAL, GMimeMessagePartial))
#define GMIME_MESSAGE_PARTIAL_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), GMIME_TYPE_MESSAGE_PARTIAL, GMimeMessagePartialClass))
#define GMIME_IS_MESSAGE_PARTIAL(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GMIME_TYPE_MESSAGE_PARTIAL))
#define GMIME_IS_MESSAGE_PARTIAL_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), GMIME_TYPE_MESSAGE_PARTIAL))
#define GMIME_MESSAGE_PARTIAL_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), GMIME_TYPE_MESSAGE_PARTIAL, GMimeMessagePartialClass))

typedef struct _GMimeMessagePartial GMimeMessagePartial;
typedef struct _GMimeMessagePartialClass GMimeMessagePartialClass;

struct _GMimeMessagePartial {
	GMimePart parent_object;
	
	char *id;
	int number;
	int total;
};

struct _GMimeMessagePartialClass {
	GMimePartClass parent_class;
	
};


GType g_mime_message_partial_get_type (void);

GMimeMessagePartial *g_mime_message_partial_new (const char *id, int number, int total);

const char *g_mime_message_partial_get_id (GMimeMessagePartial *partial);

int g_mime_message_partial_get_number (GMimeMessagePartial *partial);

int g_mime_message_partial_get_total (GMimeMessagePartial *partial);

GMimeMessage *g_mime_message_partial_reconstruct_message (GMimeMessagePartial **partials, size_t num);

GMimeMessage **g_mime_message_partial_split_message (GMimeMessage *message, size_t max_size, size_t *nparts);

G_END_DECLS

#endif /* __GMIME_MESSAGE_PARTIAL_H__ */
