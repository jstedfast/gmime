/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*  GMime
 *  Copyright (C) 2000-2008 Jeffrey Stedfast
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


#ifndef __GMIME_MESSAGE_H__
#define __GMIME_MESSAGE_H__

#include <stdarg.h>
#include <time.h>

#include <gmime/gmime-object.h>
#include <gmime/gmime-header.h>
#include <gmime/gmime-stream.h>
#include <gmime/internet-address.h>

G_BEGIN_DECLS

#define GMIME_TYPE_MESSAGE            (g_mime_message_get_type ())
#define GMIME_MESSAGE(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), GMIME_TYPE_MESSAGE, GMimeMessage))
#define GMIME_MESSAGE_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), GMIME_TYPE_MESSAGE, GMimeMessageClass))
#define GMIME_IS_MESSAGE(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GMIME_TYPE_MESSAGE))
#define GMIME_IS_MESSAGE_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), GMIME_TYPE_MESSAGE))
#define GMIME_MESSAGE_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), GMIME_TYPE_MESSAGE, GMimeMessageClass))

typedef struct _GMimeMessage GMimeMessage;
typedef struct _GMimeMessageClass GMimeMessageClass;

struct _GMimeMessage {
	GMimeObject parent_object;
	
	char *from;
	char *reply_to;
	
	GHashTable *recipients;
	
	char *subject;
	
	time_t date;
	int gmt_offset;     /* GMT offset */
	
	char *message_id;
	
	GMimeObject *mime_part;
};

struct _GMimeMessageClass {
	GMimeObjectClass parent_class;
	
};


/**
 * GMIME_RECIPIENT_TYPE_TO:
 *
 * Recipients in the To: header.
 **/
#define	GMIME_RECIPIENT_TYPE_TO  "To"


/**
 * GMIME_RECIPIENT_TYPE_CC:
 *
 * Recipients in the Cc: header.
 **/
#define	GMIME_RECIPIENT_TYPE_CC  "Cc"


/**
 * GMIME_RECIPIENT_TYPE_BCC:
 *
 * Recipients in the Bcc: header.
 **/
#define	GMIME_RECIPIENT_TYPE_BCC "Bcc"



GType g_mime_message_get_type (void);

GMimeMessage *g_mime_message_new (gboolean pretty_headers);

void g_mime_message_set_sender (GMimeMessage *message, const char *sender);
const char *g_mime_message_get_sender (GMimeMessage *message);

void g_mime_message_set_reply_to (GMimeMessage *message, const char *reply_to);
const char *g_mime_message_get_reply_to (GMimeMessage *message);

void g_mime_message_add_recipient (GMimeMessage *message, char *type, const char *name, const char *address);
void g_mime_message_add_recipients_from_string (GMimeMessage *message, char *type, const char *str);
const InternetAddressList *g_mime_message_get_recipients (GMimeMessage *message, const char *type);
InternetAddressList *g_mime_message_get_all_recipients (GMimeMessage *message);

void g_mime_message_set_subject (GMimeMessage *message, const char *subject);
const char *g_mime_message_get_subject (GMimeMessage *message);

void g_mime_message_set_date (GMimeMessage *message, time_t date, int gmt_offset);
void g_mime_message_get_date (GMimeMessage *message, time_t *date, int *gmt_offset);
char *g_mime_message_get_date_string (GMimeMessage *message);

void g_mime_message_set_message_id (GMimeMessage *message, const char *message_id);
const char *g_mime_message_get_message_id (GMimeMessage *message);

void g_mime_message_add_header (GMimeMessage *message, const char *header, const char *value);
void g_mime_message_set_header (GMimeMessage *message, const char *header, const char *value);
const char *g_mime_message_get_header (GMimeMessage *message, const char *header);

GMimeObject *g_mime_message_get_mime_part (GMimeMessage *message);
void g_mime_message_set_mime_part (GMimeMessage *message, GMimeObject *mime_part);

void g_mime_message_foreach_part (GMimeMessage *message, GMimePartFunc callback, gpointer data);

G_END_DECLS

#endif /* __GMIME_MESSAGE_H__ */
