/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 *  Authors: Jeffrey Stedfast <fejj@helixcode.com>
 *
 *  Copyright 2000 Helix Code, Inc. (www.helixcode.com)
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

#ifndef __GMIME_MESSAGE_H__
#define __GMIME_MESSAGE_H__

#ifdef __cplusplus
extern "C" {
#pragma }
#endif /* __cplusplus }*/

#include <glib.h>
#include <stdarg.h>
#include <time.h>
#include "gmime-part.h"
#include "gmime-header.h"

#define	GMIME_RECIPIENT_TYPE_TO  "To"
#define	GMIME_RECIPIENT_TYPE_CC  "Cc"
#define	GMIME_RECIPIENT_TYPE_BCC "Bcc"

struct _GMimeMessageHeader {
	gchar *from;
	gchar *reply_to;
	
	GHashTable *recipients;
	
	gchar *subject;
	
	time_t date;
	int gmt_offset;     /* GMT offset */
	
	gchar *message_id;
	
	GMimeHeader *headers;
};

typedef struct _GMimeMessageHeader GMimeMessageHeader;

struct _GMimeMessage {
	GMimeMessageHeader *header;
	
	GMimePart *mime_part;
};

typedef struct _GMimeMessage GMimeMessage;

GMimeMessage *g_mime_message_new (void);
void g_mime_message_destroy (GMimeMessage *message);

void g_mime_message_set_sender (GMimeMessage *message, const gchar *sender);
const gchar *g_mime_message_get_sender (GMimeMessage *message);

void g_mime_message_set_reply_to (GMimeMessage *message, const gchar *reply_to);
const gchar *g_mime_message_get_reply_to (GMimeMessage *message);

void g_mime_message_add_recipient (GMimeMessage *message, gchar *type, const gchar *name, const gchar *address);
void g_mime_message_add_recipients_from_string (GMimeMessage *message, gchar *type, const gchar *string);
GList *g_mime_message_get_recipients (GMimeMessage *message, const gchar *type);

void g_mime_message_set_subject (GMimeMessage *message, const gchar *subject);
const gchar *g_mime_message_get_subject (GMimeMessage *message);

void g_mime_message_set_date (GMimeMessage *message, time_t date, int gmt_offset);
void g_mime_message_get_date (GMimeMessage *message, time_t *date, int *gmt_offset);
gchar *g_mime_message_get_date_string (GMimeMessage *message);

void g_mime_message_set_message_id (GMimeMessage *message, const gchar *id);
const gchar *g_mime_message_get_message_id (GMimeMessage *message);

void g_mime_message_set_header (GMimeMessage *message, const gchar *field, const gchar *value);
const gchar *g_mime_message_get_header (GMimeMessage *message, const gchar *field);

void g_mime_message_set_mime_part (GMimeMessage *message, GMimePart *mime_part);

/* utility functions */
gchar *g_mime_message_to_string (GMimeMessage *message);

gchar *g_mime_message_get_body (const GMimeMessage *message, gboolean want_plain, gboolean *is_html);
gchar *g_mime_message_get_headers (GMimeMessage *message);

void g_mime_message_foreach_part (GMimeMessage *message, GMimePartFunc callback, gpointer data);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* __GMIME_MESSAGE_H__ */
