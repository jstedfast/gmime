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


#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <string.h>
#include <ctype.h>
#include <locale.h>

#ifdef HAVE_ALLOCA_H
#include <alloca.h>
#endif

#include "gmime-message.h"
#include "gmime-utils.h"
#include "gmime-stream-mem.h"

static void g_mime_message_destroy (GMimeObject *object);

static GMimeObject object_template = {
	0, 0, g_mime_message_destroy
};

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
	
	message = g_new0 (GMimeMessage, 1);
	g_mime_object_construct (GMIME_OBJECT (message),
				 &object_template,
				 GMIME_MESSAGE_TYPE);
	
	message->header = g_new0 (GMimeMessageHeader, 1);
	
	message->header->recipients = g_hash_table_new (g_str_hash, g_str_equal);
	
	message->header->headers = headers = g_mime_header_new ();
	
	if (pretty_headers) {
		/* Populate with the "standard" rfc822 headers so we can have a standard order */
		for (i = 0; rfc822_headers[i]; i++) 
			g_mime_header_set (headers, rfc822_headers[i], NULL);
	}
	
	return message;
}

static gboolean
recipients_destroy (gpointer key, gpointer value, gpointer user_data)
{
	InternetAddressList *recipients = value;
	
	internet_address_list_destroy (recipients);
	
	return TRUE;
}

static void
g_mime_message_destroy (GMimeObject *object)
{
	GMimeMessage *message = (GMimeMessage *) object;
	
	g_return_if_fail (GMIME_IS_MESSAGE (object));
	
	g_free (message->header->from);
	g_free (message->header->reply_to);
	
	/* destroy all recipients */
	g_hash_table_foreach_remove (message->header->recipients, recipients_destroy, NULL);
	g_hash_table_destroy (message->header->recipients);
	
	g_free (message->header->subject);
	
	g_free (message->header->message_id);
	
	g_mime_header_destroy (message->header->headers);
	
	g_free (message->header);
	
	/* unref child mime part */
	if (message->mime_part)
		g_mime_object_unref (GMIME_OBJECT (message->mime_part));
	
	g_free (message);
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
	GMimeMessageHeader *header;
	
	g_return_if_fail (GMIME_IS_MESSAGE (message));
	
	header = message->header;
	
	if (header->from)
		g_free (header->from);
	
	header->from = g_strstrip (g_strdup (sender));
	g_mime_header_set (header->headers, "From", header->from);
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
	
	return message->header->from;
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
	GMimeMessageHeader *header;
	
	g_return_if_fail (GMIME_IS_MESSAGE (message));
	
	header = message->header;
	
	if (header->reply_to)
		g_free (header->reply_to);
	
	header->reply_to = g_strstrip (g_strdup (reply_to));
	g_mime_header_set (header->headers, "Reply-To", header->reply_to);
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
	
	return message->header->reply_to;
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
		g_mime_header_set (message->header->headers, type, string);
		g_free (string);
	} else
		g_mime_header_set (message->header->headers, type, NULL);
}


/**
 * g_mime_message_add_recipient:
 * @message: MIME Message to change
 * @type: Recipient type
 * @name: The recipient's name
 * @address: The recipient's address
 *
 * Add a recipient of a chosen type to the MIME Message. Available
 * recipient types include: GMIME_RECIPIENT_TYPE_TO,
 * GMIME_RECIPIENT_TYPE_CC and GMIME_RECIPIENT_TYPE_BCC.
 **/
void
g_mime_message_add_recipient (GMimeMessage *message, char *type, const char *name, const char *address)
{
	InternetAddressList *recipients;
	InternetAddress *ia;
	
	ia = internet_address_new_name (name, address);
	
	recipients = g_hash_table_lookup (message->header->recipients, type);
	g_hash_table_remove (message->header->recipients, type);
	
	recipients = internet_address_list_append (recipients, ia);
	internet_address_unref (ia);
	
	g_hash_table_insert (message->header->recipients, type, recipients);
	sync_recipient_header (message, type);
}


/**
 * g_mime_message_add_recipients_from_string:
 * @message: MIME Message
 * @type: Recipient type
 * @string: A string of recipient names and addresses.
 *
 * Add a list of recipients of a chosen type to the MIME
 * Message. Available recipient types include:
 * GMIME_RECIPIENT_TYPE_TO, GMIME_RECIPIENT_TYPE_CC and
 * GMIME_RECIPIENT_TYPE_BCC. The string must be in the format
 * specified in rfc822.
 **/
void
g_mime_message_add_recipients_from_string (GMimeMessage *message, char *type, const char *string)
{
	InternetAddressList *recipients, *addrlist;
	
	g_return_if_fail (GMIME_IS_MESSAGE (message));
	g_return_if_fail (string != NULL);
	
	recipients = g_hash_table_lookup (message->header->recipients, type);
	g_hash_table_remove (message->header->recipients, type);
	
	addrlist = internet_address_parse_string (string);
	if (addrlist)
		recipients = internet_address_list_concat (recipients, addrlist);
	
	g_hash_table_insert (message->header->recipients, type, recipients);
	
	sync_recipient_header (message, type);
}


/**
 * g_mime_message_get_recipients:
 * @message: MIME Message
 * @type: Recipient type
 *
 * Gets a list of recipients of type @type from @message.
 *
 * Returns a list of recipients of a chosen type from the MIME
 * Message. Available recipient types include:
 * GMIME_RECIPIENT_TYPE_TO, GMIME_RECIPIENT_TYPE_CC and
 * GMIME_RECIPIENT_TYPE_BCC.
 **/
InternetAddressList *
g_mime_message_get_recipients (GMimeMessage *message, const char *type)
{
	g_return_val_if_fail (GMIME_IS_MESSAGE (message), NULL);
	
	return g_hash_table_lookup (message->header->recipients, type);
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
	GMimeMessageHeader *header;
	
	g_return_if_fail (GMIME_IS_MESSAGE (message));
	
	header = message->header;
	
	if (header->subject)
		g_free (header->subject);
	
	header->subject = g_strstrip (g_strdup (subject));
	g_mime_header_set (header->headers, "Subject", header->subject);
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
	
	return message->header->subject;
}


/**
 * g_mime_message_set_date:
 * @message: MIME Message
 * @date: Sent-date (ex: gotten from time (NULL);)
 * @gmt_offset: GMT date offset (in +/- hours)
 * 
 * Set the sent-date on a MIME Message.
 **/
void
g_mime_message_set_date (GMimeMessage *message, time_t date, int gmt_offset)
{
	char *date_str;
	
	g_return_if_fail (GMIME_IS_MESSAGE (message));
	
	message->header->date = date;
	message->header->gmt_offset = gmt_offset;
	
	date_str = g_mime_message_get_date_string (message);
	g_mime_header_set (message->header->headers, "Date", date_str);
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
	
	*date = message->header->date;
	*gmt_offset = message->header->gmt_offset;
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
	char *locale;
	
	g_return_val_if_fail (GMIME_IS_MESSAGE (message), NULL);
	
	locale = g_strdup (setlocale (LC_TIME, ""));
	setlocale (LC_TIME, "POSIX");
	
	date_str = g_mime_utils_header_format_date (message->header->date,
						    message->header->gmt_offset);
	
	if (locale != NULL)
		setlocale (LC_TIME, locale);
	g_free (locale);
	
	return date_str;
}


/**
 * g_mime_message_set_message_id: 
 * @message: MIME Message
 * @id: message-id
 *
 * Set the Message-Id on a message.
 **/
void
g_mime_message_set_message_id (GMimeMessage *message, const char *id)
{
	GMimeMessageHeader *header;
	
	g_return_if_fail (GMIME_IS_MESSAGE (message));
	
	header = message->header;
	
	if (header->message_id)
		g_free (header->message_id);
	
	header->message_id = g_strstrip (g_strdup (id));
	g_mime_header_set (header->headers, "Message-Id", header->message_id);
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
	
	return message->header->message_id;
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
	
	g_mime_header_add (message->header->headers, header, value);
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
	
	g_mime_header_set (message->header->headers, header, value);
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
	
	return g_mime_header_get (message->header->headers, header);
}


/**
 * g_mime_message_set_mime_part:
 * @message: MIME Message
 * @mime_part: The root-level MIME Part
 *
 * Set the root-level MIME part of the message.
 **/
void
g_mime_message_set_mime_part (GMimeMessage *message, GMimePart *mime_part)
{
	g_return_if_fail (GMIME_IS_MESSAGE (message));
	g_return_if_fail (GMIME_IS_PART (mime_part));
	
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
 * Write the contents of the MIME Message to @string.
 **/
void
g_mime_message_write_to_stream (GMimeMessage *message, GMimeStream *stream)
{
	g_return_if_fail (GMIME_IS_MESSAGE (message));
	g_return_if_fail (stream != NULL);
	
	g_mime_header_write_to_stream (message->header->headers, stream);
	
	if (message->mime_part) {
		g_mime_stream_write_string (stream, "MIME-Version: 1.0\n");
		g_mime_part_write_to_stream (message->mime_part, stream);
	} else
		g_mime_stream_write (stream, "\n", 1);
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
	GMimeStream *stream;
	GByteArray *array;
	char *str;
	
	g_return_val_if_fail (GMIME_IS_MESSAGE (message), NULL);
	
	array = g_byte_array_new ();
	stream = g_mime_stream_mem_new ();
	g_mime_stream_mem_set_byte_array (GMIME_STREAM_MEM (stream), array);
	g_mime_message_write_to_stream (message, stream);
	g_mime_stream_unref (stream);
	g_byte_array_append (array, "", 1);
	str = array->data;
	g_byte_array_free (array, FALSE);
	
	return str;
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
multipart_get_body (GMimePart *multipart, gboolean want_plain, gboolean *is_html)
{
	GMimePart *first = NULL;
	const char *content;
	char *body = NULL;
	GList *child;
	size_t len;
	
	child = multipart->children;
	while (child) {
		const GMimeContentType *type;
		GMimePart *mime_part;
		
		mime_part = child->data;
		type = g_mime_part_get_content_type (mime_part);
		
		if (g_mime_content_type_is_type (type, "text", want_plain ? "plain" : "html")) {
			/* we got what we came for */
			*is_html = !want_plain;
			
			content = g_mime_part_get_content (mime_part, &len);
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
			body = multipart_get_body (mime_part, want_plain, is_html);
			
			/* You are probably asking: "why don't we break here?"
			 * The answer is because the real message body could
			 * be a part after this multipart */
		}
		
		child = child->next;
	}
	
	if (!body && first) {
		/* we didn't get the type we wanted but still got the body */
		*is_html = want_plain;
		
		content = g_mime_part_get_content (first, &len);
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
 * @is_html to TRUE if the part returned is in HTML format, otherwise
 * FALSE.
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

	g_return_val_if_fail (message!=NULL, NULL);
	g_return_val_if_fail (is_html!=NULL, NULL);
	
	type = g_mime_part_get_content_type (message->mime_part);
	if (g_mime_content_type_is_type (type, "text", "*")) {
		/* this *has* to be the message body */
		if (g_mime_content_type_is_type (type, "text", want_plain ? "plain" : "html"))
			*is_html = !want_plain;
		else
			*is_html = want_plain;
		
		content = g_mime_part_get_content (message->mime_part, &len);
		body = g_strndup (content, len);
	} else if (g_mime_content_type_is_type (type, "multipart", "*")) {
		/* lets see if we can find a body in the multipart */
		body = multipart_get_body (message->mime_part, want_plain, is_html);
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
	
	return g_mime_header_to_string (message->header->headers);
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
	
	g_mime_part_foreach (message->mime_part, callback, data);
}
