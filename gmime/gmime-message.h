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
#include "gmime-stream.h"

#define	GMIME_RECIPIENT_TYPE_TO  "To"
#define	GMIME_RECIPIENT_TYPE_CC  "Cc"
#define	GMIME_RECIPIENT_TYPE_BCC "Bcc"

struct _GMimeMessageHeader {
	char *from;
	char *reply_to;
	
	GHashTable *recipients;
	
	char *subject;
	
	time_t date;
	int gmt_offset;     /* GMT offset */
	
	char *message_id;
	
	GMimeHeader *headers;
};

typedef struct _GMimeMessageHeader GMimeMessageHeader;

struct _GMimeMessage {
	GMimeObject parent_object;
	
	GMimeMessageHeader *header;
	
	GMimePart *mime_part;
};

typedef struct _GMimeMessage GMimeMessage;

#define GMIME_MESSAGE_TYPE       g_str_hash ("GMimeMessage")
#define GMIME_IS_MESSAGE(object) (object && ((GMimeObject *) object)->type == GMIME_MESSAGE_TYPE)
#define GMIME_MESSAGE(object)    ((GMimeMessage *) object)

GMimeMessage *g_mime_message_new (gboolean pretty_headers);

void g_mime_message_set_sender (GMimeMessage *message, const char *sender);
const char *g_mime_message_get_sender (GMimeMessage *message);

void g_mime_message_set_reply_to (GMimeMessage *message, const char *reply_to);
const char *g_mime_message_get_reply_to (GMimeMessage *message);

void g_mime_message_add_recipient (GMimeMessage *message, char *type, const char *name, const char *address);
void g_mime_message_add_recipients_from_string (GMimeMessage *message, char *type, const char *string);
GList *g_mime_message_get_recipients (GMimeMessage *message, const char *type);

void g_mime_message_set_subject (GMimeMessage *message, const char *subject);
const char *g_mime_message_get_subject (GMimeMessage *message);

void g_mime_message_set_date (GMimeMessage *message, time_t date, int gmt_offset);
void g_mime_message_get_date (GMimeMessage *message, time_t *date, int *gmt_offset);
char *g_mime_message_get_date_string (GMimeMessage *message);

void g_mime_message_set_message_id (GMimeMessage *message, const char *id);
const char *g_mime_message_get_message_id (GMimeMessage *message);

void g_mime_message_add_header (GMimeMessage *message, const char *header, const char *value);
void g_mime_message_set_header (GMimeMessage *message, const char *header, const char *value);
const char *g_mime_message_get_header (GMimeMessage *message, const char *header);

void g_mime_message_set_mime_part (GMimeMessage *message, GMimePart *mime_part);

/* utility functions */
void g_mime_message_write_to_stream (GMimeMessage *message, GMimeStream *stream);
char *g_mime_message_to_string (GMimeMessage *message);

char *g_mime_message_get_body (const GMimeMessage *message, gboolean want_plain, gboolean *is_html);
char *g_mime_message_get_headers (GMimeMessage *message);

void g_mime_message_foreach_part (GMimeMessage *message, GMimePartFunc callback, gpointer data);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* __GMIME_MESSAGE_H__ */
