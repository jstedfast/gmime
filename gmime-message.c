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

#include "gmime-message.h"
#include "gmime-utils.h"
#include "internet-address.h"
#include <config.h>
#include <string.h>
#include <ctype.h>
#include <locale.h>

/**
 * g_mime_message_new: Create a new MIME Message object
 *
 * Returns an empty MIME Message object with no headers nor content
 * set by default.
 **/
GMimeMessage *
g_mime_message_new ()
{
	GMimeMessage *message;
	
	message = g_new0 (GMimeMessage, 1);
	
	message->header = g_new0 (GMimeMessageHeader, 1);
	
	message->header->recipients = g_hash_table_new (g_str_hash, g_str_equal);
	
	message->header->arbitrary_headers = g_ptr_array_new ();
	
	return message;
}

static gboolean
recipients_destroy (gpointer key, gpointer value, gpointer user_data)
{
	GList *recipients = value;
	
	if (recipients) {
		GList *recipient;
		
		recipient = recipients;
		while (recipient) {
			internet_address_destroy (recipient->data);
			recipient = recipient->next;
		}
		
		g_list_free (recipients);
	}
	
	return TRUE;
}


/**
 * g_mime_message_destroy: Destroy the MIME Message.
 * @message: MIME Message to destroy
 *
 * Releases all memory used by the MIME Message and it's child MIME
 * Parts back to the Operating System for reuse.
 **/
void
g_mime_message_destroy (GMimeMessage *message)
{
	int i;
	
	g_return_if_fail (message != NULL);
	
	g_free (message->header->from);
	g_free (message->header->reply_to);
	
	/* destroy all recipients */
	g_hash_table_foreach_remove (message->header->recipients, recipients_destroy, NULL);
	g_hash_table_destroy (message->header->recipients);
	
	g_free (message->header->subject);
	
	g_free (message->header->message_id);
	
	/* destroy arbitrary headers */
	for (i = 0; i < message->header->arbitrary_headers->len; i++) {
		GMimeHeader *header = message->header->arbitrary_headers->pdata[i];
		
		g_free (header->name);
		g_free (header->value);
		g_free (header);
	}
	g_ptr_array_free (message->header->arbitrary_headers, TRUE);
	
	g_free (message->header);
	
	/* destroy all child mime parts */
	if (message->mime_part)
		g_mime_part_destroy (message->mime_part);
	
	g_free (message);
}


/**
 * g_mime_message_set_sender: Set the name/address of the sender
 * @message: MIME Message to change
 * @sender: The name and address of the sender
 *
 * Set the sender's name and address on the MIME Message.
 * (ex: "\"Joe Sixpack\" <joe@sixpack.org>")
 **/
void
g_mime_message_set_sender (GMimeMessage *message, const gchar *sender)
{
	g_return_if_fail (message != NULL);
	
	if (message->header->from)
		g_free (message->header->from);
	
	message->header->from = g_strdup (sender);
}


/**
 * g_mime_message_get_sender: Get the name/address of the sender
 * @message: MIME Message
 *
 * Returns the sender's name and address of the MIME Message.
 **/
const gchar *
g_mime_message_get_sender (GMimeMessage *message)
{
	g_return_val_if_fail (message != NULL, NULL);
	
	return message->header->from;
}


/**
 * g_mime_message_set_reply_to: Set the Reply-To header of the MIME Message
 * @message: MIME Message to change
 * @reply_to: The Reply-To address
 *
 * Set the sender's Reply-To address on the MIME Message.
 **/
void
g_mime_message_set_reply_to (GMimeMessage *message, const gchar *reply_to)
{
	g_return_if_fail (message != NULL);
	
	if (message->header->reply_to)
		g_free (message->header->reply_to);
	
	message->header->reply_to = g_strdup (reply_to);
}


/**
 * g_mime_message_get_reply_to: Get the Reply-To address (if available)
 * @message: MIME Message
 *
 * Returns the sender's Reply-To address from the MIME Message.
 **/
const gchar *
g_mime_message_get_reply_to (GMimeMessage *message)
{
	g_return_val_if_fail (message != NULL, NULL);
	
	return message->header->reply_to;
}


/**
 * g_mime_message_add_recipient: Add a recipient
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
g_mime_message_add_recipient (GMimeMessage *message, gchar *type, const gchar *name, const gchar *address)
{
	InternetAddress *ia;
	GList *recipients;
	
	ia = internet_address_new (name, address);
	
	recipients = g_hash_table_lookup (message->header->recipients, type);
	g_hash_table_remove (message->header->recipients, type);
	
	recipients = g_list_append (recipients, ia);
	g_hash_table_insert (message->header->recipients, type, recipients);
}


/**
 * g_mime_message_add_recipients_from_string: Add a list of recipients
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
g_mime_message_add_recipients_from_string (GMimeMessage *message, gchar *type, const gchar *string)
{
	InternetAddress *ia;
	GList *recipients;
	gchar *ptr, *eptr, *recipient;
	
	g_return_if_fail (message != NULL);
	g_return_if_fail (string != NULL);
	
	recipients = g_hash_table_lookup (message->header->recipients, type);
	g_hash_table_remove (message->header->recipients, type);
	
	ptr = (gchar *) string;
	
	while (*ptr) {
		gboolean in_quotes = FALSE;
		
		/* skip through leading whitespace */
		for ( ; *ptr && isspace (*ptr); ptr++);
		
		if (*ptr == '"')
			in_quotes = TRUE;
		
		/* find the end of this address */
		eptr = ptr + 1;
		while (*eptr) {
			if (*eptr == '"' && *(eptr - 1) != '\\')
				in_quotes = !in_quotes;
			else if (*eptr == ',' && !in_quotes)
				break;
			
			eptr++;
		}
		
		recipient = g_strndup (ptr, (gint) (eptr - ptr));
		ia = internet_address_new_from_string (recipient);
		g_free (recipient);
		recipients = g_list_append (recipients, ia);
		
		if (*eptr)
			ptr = eptr + 1;
		else
			break;
	}
	
	g_hash_table_insert (message->header->recipients, type, recipients);
}


/**
 * g_mime_message_get_recipients: Get a list of recipients
 * @message: MIME Message
 * @type: Recipient type
 *
 * Returns a list of recipients of a chosen type from the MIME
 * Message. Available recipient types include:
 * GMIME_RECIPIENT_TYPE_TO, GMIME_RECIPIENT_TYPE_CC and
 * GMIME_RECIPIENT_TYPE_BCC.
 **/
GList *
g_mime_message_get_recipients (GMimeMessage *message, const gchar *type)
{
	g_return_val_if_fail (message != NULL, NULL);
	
	return g_hash_table_lookup (message->header->recipients, type);
}


/**
 * g_mime_message_set_subject: Set the message subject
 * @message: MIME Message
 * @subject: Subject string
 *
 * Set the Subject field on a MIME Message.
 **/
void
g_mime_message_set_subject (GMimeMessage *message, const gchar *subject)
{
	g_return_if_fail (message != NULL);
	
	if (message->header->subject)
		g_free (message->header->subject);
	
	message->header->subject = g_strdup (subject);
}


/**
 * g_mime_message_get_subject: Get the message subject
 * @message: MIME Message
 *
 * Returns the Subject field on a MIME Message.
 **/
const gchar *
g_mime_message_get_subject (GMimeMessage *message)
{
	g_return_val_if_fail (message != NULL, NULL);
	
	return message->header->subject;
}


/**
 * g_mime_message_set_date: Set the message sent-date
 * @message: MIME Message
 * @date: Sent-date (ex: gotten from time (NULL);)
 * @gmt_offset: GMT date offset (in +/- hours)
 * 
 * Set the sent-date on a MIME Message.
 **/
void
g_mime_message_set_date (GMimeMessage *message, time_t date, int gmt_offset)
{
	g_return_if_fail (message != NULL);
	
	message->header->date = date;
	message->header->gmt_offset = gmt_offset;
}


/**
 * g_mime_message_get_date: Get the message sent-date
 * @message: MIME Message
 * @date: Sent-date
 * @gmt_offset: GMT date offset (in +/- hours)
 * 
 * Stores the date in time_t format in #date and the GMT offset in
 * #gmt_offset.
 **/
void
g_mime_message_get_date (GMimeMessage *message, time_t *date, int *gmt_offset)
{
	g_return_if_fail (message != NULL);
	
	*date = message->header->date;
	*gmt_offset = message->header->gmt_offset;
}


/**
 * g_mime_message_get_date_string: Get the message sent-date
 * @message: MIME Message
 * 
 * Returns the sent-date of the MIME Message in string format.
 **/
gchar *
g_mime_message_get_date_string (GMimeMessage *message)
{
	gchar *date_str;
	gchar *locale;
	
	g_return_val_if_fail (message != NULL, NULL);
	
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
g_mime_message_set_message_id (GMimeMessage *message, const gchar *id)
{
	g_return_if_fail (message != NULL);
	
	if (message->header->message_id)
		g_free (message->header->message_id);
	message->header->message_id = g_strdup (id);
}


/**
 * g_mime_message_get_message_id: 
 * @message: MIME Message
 *
 * Returns the Message-Id of a message.
 **/
const gchar *
g_mime_message_get_message_id (GMimeMessage *message)
{
	g_return_if_fail (message != NULL);
	
	return message->header->message_id;
}


/**
 * g_mime_message_add_arbitrary_header: Add an arbitrary message header
 * @message: MIME Message
 * @field: rfc822 header field
 * @value: the contents of the header field
 *
 * Add an arbitrary message header to the MIME Message such as X-Mailer,
 * X-Priority, or In-Reply-To.
 **/
void
g_mime_message_add_arbitrary_header (GMimeMessage *message, const gchar *field, const gchar *value)
{
	GMimeHeader *header;
	
	g_return_if_fail (message != NULL);
	
	header = g_new (GMimeHeader, 1);
	header->name = g_strdup (field);
	header->value = g_strdup (value);
	
	g_ptr_array_add (message->header->arbitrary_headers, header);
}


/**
 * g_mime_message_set_mime_part: Set the root-level MIME Part of the message
 * @message: MIME Message
 * @mime_part: The root-level MIME Part
 *
 * Set the root-level MIME part of the message.
 **/
void
g_mime_message_set_mime_part (GMimeMessage *message, GMimePart *mime_part)
{
	g_return_if_fail (message != NULL);
	
	if (message->mime_part)
		g_mime_part_destroy (message->mime_part);
	
	message->mime_part = mime_part;
}


static gchar *
create_header (GMimeMessage *message)
{
	/* Make sure to not append "\n\n" to the end
	 * so we can add MIME headers later */
	GString *string;
	gchar *str, *buf, *date, *subject;
	GList *recipients;
	
	string = g_string_new ("");
	
	/* write out the arbitrary headers first (as they may contain
	 * "Received:" headers which really should come first) */
	if (message->header->arbitrary_headers->len) {
		gint i;
		
		for (i = 0; i < message->header->arbitrary_headers->len; i++) {
			const GMimeHeader *header;
			gchar *encoded_value;
			
			header = message->header->arbitrary_headers->pdata[i];
			encoded_value = g_mime_utils_8bit_header_encode (header->value);
			
			buf = g_mime_utils_header_printf ("%s: %s\n", header->name, encoded_value);
			g_free (encoded_value);
			
			g_string_append (string, buf);
			g_free (buf);
		}
	}
	
	/* create the standard headers */
	if (!message->header->date)
		g_mime_message_set_date (message, time (NULL), 0);
	date = g_mime_message_get_date_string (message);
	buf = g_mime_utils_header_printf ("Date: %s\n", date);
	g_string_append (string, buf);
	g_free (date);
	g_free (buf);
	
	buf = g_mime_utils_header_printf ("From: %s\n", message->header->from ? message->header->from : "");
	g_string_append (string, buf);
	g_free (buf);
	
	if (message->header->reply_to) {
		buf = g_mime_utils_header_printf ("Reply-To: %s\n", message->header->reply_to);
		g_string_append (string, buf);
		g_free (buf);
	}
	
	recipients = g_mime_message_get_recipients (message, GMIME_RECIPIENT_TYPE_TO);
	if (recipients) {
		GString *recip;
		
		recip = g_string_new ("To: ");
		while (TRUE) {
			InternetAddress *ia;
			gchar *address;
			
			ia = recipients->data;
			address = internet_address_to_string (ia, TRUE);
			g_string_append (recip, address);
			g_free (address);
			
			recipients = recipients->next;
			if (recipients)
				g_string_append (recip, ", ");
			else
				break;
		}
		g_string_append (recip, "\n");
		buf = g_mime_utils_header_fold (recip->str);
		g_string_free (recip, TRUE);
		
		g_string_append (string, buf);
		g_free (buf);
	}
	
	recipients = g_mime_message_get_recipients (message, GMIME_RECIPIENT_TYPE_CC);
	if (recipients) {
		GString *recip;
		
		recip = g_string_new ("Cc: ");
		while (TRUE) {
			InternetAddress *ia;
			gchar *address;
			
			ia = recipients->data;
			address = internet_address_to_string (ia, TRUE);
			g_string_append (recip, address);
			g_free (address);
			
			recipients = recipients->next;
			if (recipients)
				g_string_append (recip, ", ");
			else
				break;
		}
		g_string_append (recip, "\n");
		buf = g_mime_utils_header_fold (recip->str);
		g_string_free (recip, TRUE);
		
		g_string_append (string, buf);
		g_free (buf);
	}
	
	subject = g_mime_utils_8bit_header_encode (message->header->subject);
	buf = g_mime_utils_header_printf ("Subject: %s\n", subject ? subject : "");
	g_string_append (string, buf);
	g_free (subject);
	g_free (buf);
	
	if (message->header->message_id) {
		gchar *message_id;
		
		message_id = g_mime_utils_8bit_header_encode (message->header->message_id);
		buf = g_mime_utils_header_printf ("Message-Id: %s\n", message_id ? message_id : "");
		g_string_append (string, buf);
		g_free (message_id);
		g_free (buf);
	}
	
	str = string->str;
	g_string_free (string, FALSE);
	
	return str;
}


/**
 * g_mime_message_to_string: Write the MIME Message to a string
 * @message: MIME Message
 *
 * Returns an allocated string containing the MIME Message.
 **/
gchar *
g_mime_message_to_string (GMimeMessage *message)
{
	GString *string;
	gchar *str, *header, *body;
	
	g_return_val_if_fail (message != NULL, NULL);
	
	header = create_header (message);
	if (!header)
		return NULL;
	
	string = g_string_new (header);
	g_free (header);
	
	body = g_mime_part_to_string (message->mime_part, TRUE);
	if (body)
		g_string_append (string, body);
	else
		g_string_append (string, "\n");
	g_free (body);
	
	str = string->str;
	g_string_free (string, FALSE);
	
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
static gchar *
multipart_get_body (GMimePart *multipart, gboolean want_plain, gboolean *is_html)
{
	GMimePart *first = NULL;
	const gchar *content;
	gchar *body = NULL;
	GList *child;
	guint len;
	
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
 * g_mime_message_get_body: Return the body of the message
 * @message: MIME Message
 * @want_plain: request text/plain
 * @is_html: body returned is in html format
 *
 * Returns the prefered form of the message body. Sets the value of
 * #is_html to TRUE if the part returned is in HTML format, otherwise
 * FALSE.
 * Note: This function is NOT guarenteed to always work as it
 * makes some assumptions that are not necessarily true. It is
 * recommended that you traverse the MIME structure yourself.
 **/
gchar *
g_mime_message_get_body (const GMimeMessage *message, gboolean want_plain, gboolean *is_html)
{
	const GMimeContentType *type;
	const gchar *content;
	gchar *body = NULL;
	guint len = 0;
	
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
 * g_mime_message_get_headers: Return the raw message headers
 * @message: MIME Message
 *
 * Returns an allocated string containing the raw message headers.
 **/
gchar *
g_mime_message_get_headers (GMimeMessage *message)
{
	return create_header (message);
}


/**
 * g_mime_message_foreach_part:
 * @message: MIME message
 * @callback: function to call on each of the mime parts contained by the mime message
 * @data: extra data to pass to the callback
 *
 * Calls #callback on each of the mime parts in the mime message.
 **/
void
g_mime_message_foreach_part (GMimeMessage *message, GMimePartFunc callback, gpointer data)
{
	g_return_if_fail (message != NULL);
	g_return_if_fail (callback != NULL);
	
	g_mime_part_foreach (message->mime_part, callback, data);
}
