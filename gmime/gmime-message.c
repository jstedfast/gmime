/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 *  Authors: Jeffrey Stedfast <fejj@ximian.com>
 *
 *  Copyright 2000-2002 Ximain, Inc. (www.ximian.com)
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
#include <ctype.h>
#include <locale.h>

#include "gmime-message.h"
#include "gmime-multipart.h"
#include "gmime-part.h"
#include "gmime-utils.h"
#include "gmime-stream-mem.h"


static void g_mime_message_class_init (GMimeMessageClass *klass);
static void g_mime_message_init (GMimeMessage *message, GMimeMessageClass *klass);
static void g_mime_message_finalize (GObject *object);

/* GMimeObject class methods */
static void message_init (GMimeObject *object);
static void message_add_header (GMimeObject *object, const char *header, const char *value);
static void message_set_header (GMimeObject *object, const char *header, const char *value);
static const char *message_get_header (GMimeObject *object, const char *header);
static void message_remove_header (GMimeObject *object, const char *header);
static char *message_get_headers (GMimeObject *object);
static ssize_t message_write_to_stream (GMimeObject *object, GMimeStream *stream);

static void message_set_sender (GMimeMessage *message, const char *sender);
static void message_set_reply_to (GMimeMessage *message, const char *reply_to);
static void message_add_recipients_from_string (GMimeMessage *message, char *type, const char *string);
static void message_set_subject (GMimeMessage *message, const char *subject);
static void message_set_message_id (GMimeMessage *message, const char *message_id);

static GMimeObjectClass *parent_class = NULL;


static char *rfc822_headers[] = {
	"Return-Path",
	"Received",
	"Date",
	"From",
	"Reply-To",
	"Subject",
	"Sender",
	"To",
	"Cc",
	NULL
};


GType
g_mime_message_get_type (void)
{
	static GType type = 0;
	
	if (!type) {
		static const GTypeInfo info = {
			sizeof (GMimeMessageClass),
			NULL, /* base_class_init */
			NULL, /* base_class_finalize */
			(GClassInitFunc) g_mime_message_class_init,
			NULL, /* class_finalize */
			NULL, /* class_data */
			sizeof (GMimeMessage),
			0,   /* n_preallocs */
			(GInstanceInitFunc) g_mime_message_init,
		};
		
		type = g_type_register_static (GMIME_TYPE_OBJECT, "GMimeMessage", &info, 0);
	}
	
	return type;
}


static void
g_mime_message_class_init (GMimeMessageClass *klass)
{
	GMimeObjectClass *object_class = GMIME_OBJECT_CLASS (klass);
	GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
	
	parent_class = g_type_class_ref (GMIME_TYPE_OBJECT);
	
	gobject_class->finalize = g_mime_message_finalize;
	
	object_class->init = message_init;
	object_class->add_header = message_add_header;
	object_class->set_header = message_set_header;
	object_class->get_header = message_get_header;
	object_class->remove_header = message_remove_header;
	object_class->get_headers = message_get_headers;
	object_class->write_to_stream = message_write_to_stream;
}

static void
g_mime_message_init (GMimeMessage *message, GMimeMessageClass *klass)
{
	message->from = NULL;
	message->reply_to = NULL;
	message->recipients = g_hash_table_new (g_str_hash, g_str_equal);
	message->subject = NULL;
	message->date = 0;
	message->gmt_offset = 0;
	message->message_id = NULL;
	message->mime_part = NULL;
}

static gboolean
recipients_destroy (gpointer key, gpointer value, gpointer user_data)
{
	InternetAddressList *recipients = value;
	
	internet_address_list_destroy (recipients);
	
	return TRUE;
}

static void
g_mime_message_finalize (GObject *object)
{
	GMimeMessage *message = (GMimeMessage *) object;
	
	g_free (message->from);
	g_free (message->reply_to);
	
	/* destroy all recipients */
	g_hash_table_foreach_remove (message->recipients, recipients_destroy, NULL);
	g_hash_table_destroy (message->recipients);
	
	g_free (message->subject);
	
	g_free (message->message_id);
	
	/* unref child mime part */
	if (message->mime_part)
		g_mime_object_unref (GMIME_OBJECT (message->mime_part));
	
	G_OBJECT_CLASS (parent_class)->finalize (object);
}


static void
message_init (GMimeObject *object)
{
	/* no-op */
	GMIME_OBJECT_CLASS (parent_class)->init (object);
}


enum {
	HEADER_FROM,
	HEADER_REPLY_TO,
	HEADER_TO,
	HEADER_CC,
	HEADER_BCC,
	HEADER_SUBJECT,
	HEADER_DATE,
	HEADER_MESSAGE_ID,
	HEADER_UNKNOWN
};

static char *headers[] = {
	"From",
	"Reply-To",
	"To",
	"Cc",
	"Bcc",
	"Subject",
	"Date",
	"Message-Id",
	NULL
};

static gboolean
process_header (GMimeObject *object, const char *header, const char *value)
{
	GMimeMessage *message = (GMimeMessage *) object;	
	int offset, i;
	time_t date;
	
	for (i = 0; headers[i]; i++) {
		if (!strcasecmp (headers[i], header))
			break;
	}
	
	switch (i) {
	case HEADER_FROM:
		message_set_sender (message, value);
		break;
	case HEADER_REPLY_TO:
		message_set_reply_to (message, value);
		break;
	case HEADER_TO:
		message_add_recipients_from_string (message, GMIME_RECIPIENT_TYPE_TO, value);
		break;
	case HEADER_CC:
		message_add_recipients_from_string (message, GMIME_RECIPIENT_TYPE_CC, value);
		break;
	case HEADER_BCC:
		message_add_recipients_from_string (message, GMIME_RECIPIENT_TYPE_BCC, value);
		break;
	case HEADER_SUBJECT:
		message_set_subject (message, value);
		break;
	case HEADER_DATE:
		date = g_mime_utils_header_decode_date (value, &offset);
		message->date = date;
		message->gmt_offset = offset;
		break;
	case HEADER_MESSAGE_ID:
		message_set_message_id (message, value);
		break;
	default:
		return FALSE;
		break;
	}
	
	return TRUE;
}

static void
message_add_header (GMimeObject *object, const char *header, const char *value)
{
	if (!strcasecmp ("MIME-Version", header))
		return;
	
	/* Make sure that the header is not a Content-* header, else it
           doesn't belong on a message */
	
	if (strncasecmp ("Content-", header, 8)) {
		if (process_header (object, header, value))
			g_mime_header_add (object->headers, header, value);
		else
			GMIME_OBJECT_CLASS (parent_class)->add_header (object, header, value);
	}
}

static void
message_set_header (GMimeObject *object, const char *header, const char *value)
{
	if (!strcasecmp ("MIME-Version", header))
		return;
	
	/* Make sure that the header is not a Content-* header, else it
           doesn't belong on a message */
	
	if (strncasecmp ("Content-", header, 8)) {
		if (process_header (object, header, value))
			g_mime_header_set (object->headers, header, value);
		else
			GMIME_OBJECT_CLASS (parent_class)->set_header (object, header, value);
	}
}

static const char *
message_get_header (GMimeObject *object, const char *header)
{
	/* Make sure that the header is not a Content-* header, else it
           doesn't belong on a message */
	
	if (!strcasecmp ("MIME-Version", header))
		return "1.0";
	
	if (strncasecmp ("Content-", header, 8))
		return GMIME_OBJECT_CLASS (parent_class)->get_header (object, header);
	else
		return NULL;
}

static void
message_remove_header (GMimeObject *object, const char *header)
{
	if (!strcasecmp ("MIME-Version", header))
		return;
	
	/* Make sure that the header is not a Content-* header, else it
           doesn't belong on a multipart */
	
	if (!strncasecmp ("Content-", header, 8))
		return GMIME_OBJECT_CLASS (parent_class)->remove_header (object, header);
}

static char *
message_get_headers (GMimeObject *object)
{
	/* FIXME: get mime part headers too? */
	return GMIME_OBJECT_CLASS (parent_class)->get_headers (object);
}

static ssize_t
message_write_to_stream (GMimeObject *object, GMimeStream *stream)
{
	GMimeMessage *message = (GMimeMessage *) object;
	ssize_t nwritten, total = 0;
	
	/* write the content headers */
	nwritten = g_mime_header_write_to_stream (object->headers, stream);
	if (nwritten == -1)
		return -1;
	
	total += nwritten;
	
	if (message->mime_part) {
		nwritten = g_mime_stream_write_string (stream, "MIME-Version: 1.0\n");
		if (nwritten == -1)
			return -1;
		
		total += nwritten;
		
		nwritten = g_mime_object_write_to_stream (message->mime_part, stream);
	} else {
		nwritten = g_mime_stream_write (stream, "\n", 1);
		if (nwritten == -1)
			return -1;
	}
	
	total += nwritten;
	
	return total;
}


/**
 * g_mime_message_new: Create a new MIME Message object
 * @pretty_headers: make pretty headers 
 *
 * If @pretty_headers is %TRUE, then the standard rfc822 headers are
 * initialized so as to put headers in a nice friendly order. This is
 * strictly a cosmetic thing, so if you are unsure, it is safe to say
 * no (%FALSE).
 *
 * Returns an empty MIME Message object.
 **/
GMimeMessage *
g_mime_message_new (gboolean pretty_headers)
{
	GMimeMessage *message;
	GMimeHeader *headers;
	int i;
	
	message = g_object_new (GMIME_TYPE_MESSAGE, NULL, NULL);
	
	if (pretty_headers) {
		/* Populate with the "standard" rfc822 headers so we can have a standard order */
		for (i = 0; rfc822_headers[i]; i++) 
			g_mime_object_set_header (GMIME_OBJECT (message), rfc822_headers[i], NULL);
	}
	
	return message;
}


static void
message_set_sender (GMimeMessage *message, const char *sender)
{
	if (message->from)
		g_free (message->from);
	
	message->from = g_strstrip (g_strdup (sender));
}


/**
 * g_mime_message_set_sender:
 * @message: MIME Message to change
 * @sender: The name and address of the sender
 *
 * Set the sender's name and address on the MIME Message.
 * (ex: "\"Joe Sixpack\" <joe@sixpack.org>")
 **/
void
g_mime_message_set_sender (GMimeMessage *message, const char *sender)
{
	g_return_if_fail (GMIME_IS_MESSAGE (message));
	
	message_set_sender (message, sender);
	g_mime_header_set (GMIME_OBJECT (message)->headers, "From", message->from);
}


/**
 * g_mime_message_get_sender:
 * @message: MIME Message
 *
 * Gets the email address of the sender from @message.
 *
 * Returns the sender's name and address of the MIME Message.
 **/
const char *
g_mime_message_get_sender (GMimeMessage *message)
{
	g_return_val_if_fail (GMIME_IS_MESSAGE (message), NULL);
	
	return message->from;
}


static void
message_set_reply_to (GMimeMessage *message, const char *reply_to)
{
	if (message->reply_to)
		g_free (message->reply_to);
	
	message->reply_to = g_strstrip (g_strdup (reply_to));
}


/**
 * g_mime_message_set_reply_to:
 * @message: MIME Message to change
 * @reply_to: The Reply-To address
 *
 * Set the sender's Reply-To address on the MIME Message.
 **/
void
g_mime_message_set_reply_to (GMimeMessage *message, const char *reply_to)
{
	g_return_if_fail (GMIME_IS_MESSAGE (message));
	
	message_set_reply_to (message, reply_to);
	g_mime_header_set (GMIME_OBJECT (message)->headers, "Reply-To", message->reply_to);
}


/**
 * g_mime_message_get_reply_to:
 * @message: MIME Message
 *
 * Gets the Reply-To address from @message.
 *
 * Returns the sender's Reply-To address from the MIME Message.
 **/
const char *
g_mime_message_get_reply_to (GMimeMessage *message)
{
	g_return_val_if_fail (GMIME_IS_MESSAGE (message), NULL);
	
	return message->reply_to;
}


static void
sync_recipient_header (GMimeMessage *message, const char *type)
{
	InternetAddressList *recipients;
	
	/* sync the specified recipient header */
	recipients = g_mime_message_get_recipients (message, type);
	if (recipients) {
		char *string;
		
		string = internet_address_list_to_string (recipients, TRUE);
		g_mime_header_set (GMIME_OBJECT (message)->headers, type, string);
		g_free (string);
	} else
		g_mime_header_set (GMIME_OBJECT (message)->headers, type, NULL);
}


/**
 * g_mime_message_add_recipient:
 * @message: MIME Message to change
 * @type: Recipient type
 * @name: The recipient's name
 * @address: The recipient's address
 *
 * Add a recipient of a chosen type to the MIME Message. Available
 * recipient types include: #GMIME_RECIPIENT_TYPE_TO,
 * #GMIME_RECIPIENT_TYPE_CC and #GMIME_RECIPIENT_TYPE_BCC.
 **/
void
g_mime_message_add_recipient (GMimeMessage *message, char *type, const char *name, const char *address)
{
	InternetAddressList *recipients;
	InternetAddress *ia;
	
	g_return_if_fail (GMIME_IS_MESSAGE (message));
	g_return_if_fail (type != NULL);
	g_return_if_fail (name != NULL);
	g_return_if_fail (address != NULL);
	
	ia = internet_address_new_name (name, address);
	
	recipients = g_hash_table_lookup (message->recipients, type);
	g_hash_table_remove (message->recipients, type);
	
	recipients = internet_address_list_append (recipients, ia);
	internet_address_unref (ia);
	
	g_hash_table_insert (message->recipients, type, recipients);
	sync_recipient_header (message, type);
}


static void
message_add_recipients_from_string (GMimeMessage *message, char *type, const char *string)
{
	InternetAddressList *recipients, *addrlist;
	
	recipients = g_hash_table_lookup (message->recipients, type);
	g_hash_table_remove (message->recipients, type);
	
	addrlist = internet_address_parse_string (string);
	if (addrlist) {
		recipients = internet_address_list_concat (recipients, addrlist);
		internet_address_list_destroy (addrlist);
	}
	
	g_hash_table_insert (message->recipients, type, recipients);
}


/**
 * g_mime_message_add_recipients_from_string:
 * @message: MIME Message
 * @type: Recipient type
 * @string: A string of recipient names and addresses.
 *
 * Add a list of recipients of a chosen type to the MIME
 * Message. Available recipient types include:
 * #GMIME_RECIPIENT_TYPE_TO, #GMIME_RECIPIENT_TYPE_CC and
 * #GMIME_RECIPIENT_TYPE_BCC. The string must be in the format
 * specified in rfc822.
 **/
void
g_mime_message_add_recipients_from_string (GMimeMessage *message, char *type, const char *string)
{
	g_return_if_fail (GMIME_IS_MESSAGE (message));
	g_return_if_fail (type != NULL);
	g_return_if_fail (string != NULL);
	
	message_add_recipients_from_string (message, type, string);
	
	sync_recipient_header (message, type);
}


/**
 * g_mime_message_get_recipients:
 * @message: MIME Message
 * @type: Recipient type
 *
 * Gets a list of recipients of type @type from @message. Available
 * recipient types include: #GMIME_RECIPIENT_TYPE_TO,
 * #GMIME_RECIPIENT_TYPE_CC and #GMIME_RECIPIENT_TYPE_BCC.
 *
 * Returns a list of recipients of a chosen type from the MIME
 * Message.
 **/
InternetAddressList *
g_mime_message_get_recipients (GMimeMessage *message, const char *type)
{
	g_return_val_if_fail (GMIME_IS_MESSAGE (message), NULL);
	g_return_val_if_fail (type != NULL, NULL);
	
	return g_hash_table_lookup (message->recipients, type);
}


static void
message_set_subject (GMimeMessage *message, const char *subject)
{
	if (message->subject)
		g_free (message->subject);
	
	message->subject = g_strstrip (g_strdup (subject));
}


/**
 * g_mime_message_set_subject:
 * @message: MIME Message
 * @subject: Subject string
 *
 * Set the Subject field on a MIME Message.
 **/
void
g_mime_message_set_subject (GMimeMessage *message, const char *subject)
{
	g_return_if_fail (GMIME_IS_MESSAGE (message));
	
	message_set_subject (message, subject);
	g_mime_header_set (GMIME_OBJECT (message)->headers, "Subject", message->subject);
}


/**
 * g_mime_message_get_subject:
 * @message: MIME Message
 *
 * Gets the message's subject.
 *
 * Returns the Subject field on a MIME Message.
 **/
const char *
g_mime_message_get_subject (GMimeMessage *message)
{
	g_return_val_if_fail (GMIME_IS_MESSAGE (message), NULL);
	
	return message->subject;
}


/**
 * g_mime_message_set_date:
 * @message: MIME Message
 * @date: Sent-date (ex: gotten from time (NULL))
 * @gmt_offset: GMT date offset (in +/- hours)
 * 
 * Sets the sent-date on a MIME Message.
 **/
void
g_mime_message_set_date (GMimeMessage *message, time_t date, int gmt_offset)
{
	char *date_str;
	
	g_return_if_fail (GMIME_IS_MESSAGE (message));
	
	message->date = date;
	message->gmt_offset = gmt_offset;
	
	date_str = g_mime_message_get_date_string (message);
	g_mime_header_set (GMIME_OBJECT (message)->headers, "Date", date_str);
	g_free (date_str);
}


/**
 * g_mime_message_get_date:
 * @message: MIME Message
 * @date: Sent-date
 * @gmt_offset: GMT date offset (in +/- hours)
 * 
 * Stores the date in time_t format in #date and the GMT offset in
 * @gmt_offset.
 **/
void
g_mime_message_get_date (GMimeMessage *message, time_t *date, int *gmt_offset)
{
	g_return_if_fail (GMIME_IS_MESSAGE (message));
	g_return_if_fail (date != NULL);
	
	*date = message->date;
	
	if (gmt_offset)
		*gmt_offset = message->gmt_offset;
}


/**
 * g_mime_message_get_date_string:
 * @message: MIME Message
 *
 * Gets the message's sent date in string format.
 * 
 * Returns the sent-date of the MIME Message in string format.
 **/
char *
g_mime_message_get_date_string (GMimeMessage *message)
{
	char *date_str;
	
	g_return_val_if_fail (GMIME_IS_MESSAGE (message), NULL);
	
	date_str = g_mime_utils_header_format_date (message->date,
						    message->gmt_offset);
	
	return date_str;
}


static void
message_set_message_id (GMimeMessage *message, const char *message_id)
{
	if (message->message_id)
		g_free (message->message_id);
	
	message->message_id = g_strstrip (g_strdup (message_id));
}


/**
 * g_mime_message_set_message_id: 
 * @message: MIME Message
 * @message_id: message-id
 *
 * Set the Message-Id on a message.
 **/
void
g_mime_message_set_message_id (GMimeMessage *message, const char *message_id)
{
	g_return_if_fail (GMIME_IS_MESSAGE (message));
	
	message_set_message_id (message, message_id);
	g_mime_header_set (GMIME_OBJECT (message)->headers,
			   "Message-Id", message->message_id);
}


/**
 * g_mime_message_get_message_id: 
 * @message: MIME Message
 *
 * Gets the Message-Id header of @message.
 *
 * Returns the Message-Id of a message.
 **/
const char *
g_mime_message_get_message_id (GMimeMessage *message)
{
	g_return_val_if_fail (GMIME_IS_MESSAGE (message), NULL);
	
	return message->message_id;
}


/**
 * g_mime_message_add_header: Add an arbitrary message header
 * @message: MIME Message
 * @header: rfc822 header field
 * @value: the contents of the header field
 *
 * Add an arbitrary message header to the MIME Message such as X-Mailer,
 * X-Priority, or In-Reply-To.
 **/
void
g_mime_message_add_header (GMimeMessage *message, const char *header, const char *value)
{
	g_return_if_fail (GMIME_IS_MESSAGE (message));
	g_return_if_fail (header != NULL);
	
	g_mime_object_add_header (GMIME_OBJECT (message), header, value);
}


/**
 * g_mime_message_set_header: Add an arbitrary message header
 * @message: MIME Message
 * @header: rfc822 header field
 * @value: the contents of the header field
 *
 * Set an arbitrary message header to the MIME Message such as X-Mailer,
 * X-Priority, or In-Reply-To.
 **/
void
g_mime_message_set_header (GMimeMessage *message, const char *header, const char *value)
{
	g_return_if_fail (GMIME_IS_MESSAGE (message));
	g_return_if_fail (header != NULL);
	
	g_mime_object_set_header (GMIME_OBJECT (message), header, value);
}


/**
 * g_mime_message_get_header:
 * @message: MIME Message
 * @header: rfc822 header field
 *
 * Gets the value of the requested header @header if it exists, or
 * %NULL otherwise.
 *
 * Returns the value of the requested header (or %NULL if it isn't set)
 **/
const char *
g_mime_message_get_header (GMimeMessage *message, const char *header)
{
	g_return_val_if_fail (GMIME_IS_MESSAGE (message), NULL);
	g_return_val_if_fail (header != NULL, NULL);
	
	return g_mime_object_get_header (GMIME_OBJECT (message), header);
}


/**
 * g_mime_message_set_mime_part:
 * @message: MIME Message
 * @mime_part: The root-level MIME Part
 *
 * Set the root-level MIME part of the message.
 **/
void
g_mime_message_set_mime_part (GMimeMessage *message, GMimeObject *mime_part)
{
	g_return_if_fail (GMIME_IS_MESSAGE (message));
	g_return_if_fail (GMIME_IS_PART (mime_part) || GMIME_IS_MULTIPART (mime_part));
	
	g_mime_object_ref (GMIME_OBJECT (mime_part));
	
	if (message->mime_part)
		g_mime_object_unref (GMIME_OBJECT (message->mime_part));
	
	message->mime_part = mime_part;
}


/**
 * g_mime_message_write_to_stream:
 * @message: MIME Message
 * @stream: output stream
 *
 * Write the contents of the MIME Message to @stream.
 *
 * Returns -1 on fail.
 **/
ssize_t
g_mime_message_write_to_stream (GMimeMessage *message, GMimeStream *stream)
{
	g_return_val_if_fail (GMIME_IS_MESSAGE (message), -1);
	g_return_val_if_fail (GMIME_IS_STREAM (stream), -1);
	
	return g_mime_object_write_to_stream (GMIME_OBJECT (message), stream);
}


/**
 * g_mime_message_to_string:
 * @message: MIME Message
 *
 * Allocates a string buffer containing the mime message @message.
 *
 * Returns an allocated string containing the MIME Message.
 **/
char *
g_mime_message_to_string (GMimeMessage *message)
{
	g_return_val_if_fail (GMIME_IS_MESSAGE (message), NULL);
	
	return g_mime_object_to_string (GMIME_OBJECT (message));
}


/* Brief explanation of how this function works it's magic:
 *
 * We cycle through the immediate subparts looking for text parts. If
 * the first text part we come accross is exactly what we want then we
 * return it, otherwise keep a reference to it for later use (if we
 * don't find the preferred part later as we continue to cycle through
 * the subparts then we default to the first text part found). If we
 * come to a multipart, we descend into it repeating the process. If
 * we find the 'body' in a sub-multipart, we don't necessarily return
 * that value for it is entirely possible that there could be text
 * parts defined after the sub-multipart. For example, we could have
 * the following MIME structure:
 *
 * multipart/alternative
 *   image/png
 *   multipart/related
 *     text/html
 *     image/png
 *     image/gif
 *     image/jpeg
 *   text/plain
 *   text/html
 *
 * While one can never be certain that the text/html part within the
 * multipart/related isn't the true 'body', it's genrally safe to
 * assume that in cases like this, the outer text part(s) are the
 * message body. Note that this is an assumption and is thus not
 * guarenteed to always be correct.
 **/
static char *
multipart_get_body (GMimeMultipart *multipart, gboolean want_plain, gboolean *is_html)
{
	GMimeObject *first = NULL;
	const char *content;
	char *body = NULL;
	GList *subpart;
	size_t len;
	
	subpart = multipart->subparts;
	while (subpart) {
		const GMimeContentType *type;
		GMimeObject *mime_part;
		
		mime_part = subpart->data;
		type = g_mime_object_get_content_type (mime_part);
		
		if (g_mime_content_type_is_type (type, "text", want_plain ? "plain" : "html")) {
			/* we got what we came for */
			*is_html = !want_plain;
			
			content = g_mime_part_get_content (GMIME_PART (mime_part), &len);
			g_free (body);
			body = g_strndup (content, len);
			break;
		} else if (g_mime_content_type_is_type (type, "text", "*") && !first) {
			/* remember what our first text part was */
			first = mime_part;
			g_free (body);
			body = NULL;
		} else if (g_mime_content_type_is_type (type, "multipart", "*") && !first && !body) {
			/* look in the multipart for the body */
			body = multipart_get_body (GMIME_MULTIPART (mime_part), want_plain, is_html);
			
			/* You are probably asking: "why don't we break here?"
			 * The answer is because the real message body could
			 * be a part after this multipart */
		}
		
		subpart = subpart->next;
	}
	
	if (!body && first) {
		/* we didn't get the type we wanted but still got the body */
		*is_html = want_plain;
		
		content = g_mime_part_get_content (GMIME_PART (first), &len);
		body = g_strndup (content, len);
	}
	
	return body;
}


/**
 * g_mime_message_get_body:
 * @message: MIME Message
 * @want_plain: request text/plain
 * @is_html: body returned is in html format
 *
 * Attempts to get the body of the message in the preferred format
 * specified by @want_plain.
 *
 * Returns the prefered form of the message body. Sets the value of
 * @is_html to %TRUE if the part returned is in HTML format, otherwise
 * %FALSE.
 *
 * Note: This function is NOT guarenteed to always work as it
 * makes some assumptions that are not necessarily true. It is
 * recommended that you traverse the MIME structure yourself.
 **/
char *
g_mime_message_get_body (const GMimeMessage *message, gboolean want_plain, gboolean *is_html)
{
	const GMimeContentType *type;
	const char *content;
	char *body = NULL;
	size_t len = 0;
	
	g_return_val_if_fail (GMIME_IS_MESSAGE (message), NULL);
	g_return_val_if_fail (is_html != NULL, NULL);
	
	type = g_mime_object_get_content_type (message->mime_part);
	if (g_mime_content_type_is_type (type, "text", "*")) {
		/* this *has* to be the message body */
		if (g_mime_content_type_is_type (type, "text", want_plain ? "plain" : "html"))
			*is_html = !want_plain;
		else
			*is_html = want_plain;
		
		content = g_mime_part_get_content (GMIME_PART (message->mime_part), &len);
		body = g_strndup (content, len);
	} else if (g_mime_content_type_is_type (type, "multipart", "*")) {
		/* lets see if we can find a body in the multipart */
		body = multipart_get_body (GMIME_MULTIPART (message->mime_part), want_plain, is_html);
	}
	
	return body;
}


/**
 * g_mime_message_get_headers:
 * @message: MIME Message
 *
 * Allocates a string buffer containing the raw message headers.
 *
 * Returns an allocated string containing the raw message headers.
 **/
char *
g_mime_message_get_headers (GMimeMessage *message)
{
	g_return_val_if_fail (GMIME_IS_MESSAGE (message), NULL);
	
	return g_mime_object_get_headers (GMIME_OBJECT (message));
}


/**
 * g_mime_message_foreach_part:
 * @message: MIME message
 * @callback: function to call on each of the mime parts contained by the mime message
 * @data: extra data to pass to the callback
 *
 * Calls @callback on each of the mime parts in the mime message.
 **/
void
g_mime_message_foreach_part (GMimeMessage *message, GMimePartFunc callback, gpointer data)
{
	g_return_if_fail (GMIME_IS_MESSAGE (message));
	g_return_if_fail (callback != NULL);
	
	if (GMIME_IS_MULTIPART (message->mime_part))
		g_mime_multipart_foreach (GMIME_MULTIPART (message->mime_part), callback, data);
}
