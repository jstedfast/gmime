/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*  GMime
 *  Copyright (C) 2000-2007 Jeffrey Stedfast
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
 *  Foundation, 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
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
#include "gmime-table-private.h"
#include "gmime-parse-utils.h"


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

static void message_add_recipients_from_string (GMimeMessage *message, char *type, const char *string);

static ssize_t write_addrspec (GMimeStream *stream, const char *name, const char *value);
static ssize_t write_received (GMimeStream *stream, const char *name, const char *value);
static ssize_t write_subject (GMimeStream *stream, const char *name, const char *value);
static ssize_t write_msgid (GMimeStream *stream, const char *name, const char *value);


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
	
	g_mime_header_register_writer (((GMimeObject *) message)->headers, "Sender", write_addrspec);
	g_mime_header_register_writer (((GMimeObject *) message)->headers, "From", write_addrspec);
	g_mime_header_register_writer (((GMimeObject *) message)->headers, "To", write_addrspec);
	g_mime_header_register_writer (((GMimeObject *) message)->headers, "Cc", write_addrspec);
	g_mime_header_register_writer (((GMimeObject *) message)->headers, "Bcc", write_addrspec);
	
	g_mime_header_register_writer (((GMimeObject *) message)->headers, "Resent-Sender", write_addrspec);
	g_mime_header_register_writer (((GMimeObject *) message)->headers, "Resent-From", write_addrspec);
	g_mime_header_register_writer (((GMimeObject *) message)->headers, "Resent-To", write_addrspec);
	g_mime_header_register_writer (((GMimeObject *) message)->headers, "Resent-Cc", write_addrspec);
	g_mime_header_register_writer (((GMimeObject *) message)->headers, "Resent-Bcc", write_addrspec);
	
	g_mime_header_register_writer (((GMimeObject *) message)->headers, "Subject", write_subject);
	g_mime_header_register_writer (((GMimeObject *) message)->headers, "Received", write_received);
	g_mime_header_register_writer (((GMimeObject *) message)->headers, "Message-Id", write_msgid);
	g_mime_header_register_writer (((GMimeObject *) message)->headers, "References", write_addrspec);
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
		g_object_unref (message->mime_part);
	
	G_OBJECT_CLASS (parent_class)->finalize (object);
}


static void
message_init (GMimeObject *object)
{
	/* no-op */
	GMIME_OBJECT_CLASS (parent_class)->init (object);
}


typedef void (*token_skip_t) (const char **in);

struct _received_token {
	char *token;
	size_t len;
	token_skip_t skip;
};

static void skip_atom     (const char **in);
static void skip_domain   (const char **in);
static void skip_addr     (const char **in);
static void skip_msgid    (const char **in);

static struct _received_token received_tokens[] = {
	{ "from ", 5, skip_domain   },
	{ "by ",   3, skip_domain   },
	{ "via ",  4, skip_atom     },
	{ "with ", 5, skip_atom     },
	{ "id ",   3, skip_msgid    },
	{ "for ",  4, skip_addr     }
};

static void
skip_atom (const char **in)
{
	register const char *inptr;
	
	decode_lwsp (in);
	inptr = *in;
	while (is_atom (*inptr))
		inptr++;
	*in = inptr;
}

static void
skip_comment (const char **in)
{
	register const char *inptr = *in;
	int depth = 1;
	
	if (*inptr == '(')
		inptr++;
	
	while (*inptr && depth > 0) {
		if (*inptr == '(')
			depth++;
		else if (*inptr == ')')
			depth--;
		inptr++;
	}
	
	if (*inptr == ')')
		inptr++;
	
	*in = inptr;
}

static void
skip_quoted_string (const char **in)
{
	const char *inptr = *in;
	
	decode_lwsp (&inptr);
	if (*inptr == '"') {
		inptr++;
		while (*inptr && *inptr != '"') {
			if (*inptr == '\\')
				inptr++;
			
			if (*inptr)
				inptr++;
		}
		
		if (*inptr == '"')
			inptr++;
	}
	
	*in = inptr;
}

static void
skip_word (const char **in)
{
	decode_lwsp (in);
	if (**in == '"') {
		skip_quoted_string (in);
	} else {
		skip_atom (in);
	}
}

static void
skip_domain_subliteral (const char **in)
{
	const char *inptr = *in;
	
	while (*inptr && *inptr != '.' && *inptr != ']') {
		if (is_dtext (*inptr)) {
			inptr++;
		} else if (is_lwsp (*inptr)) {
			decode_lwsp (&inptr);
		} else {
			break;
		}
	}
	
	*in = inptr;
}

static void
skip_domain_literal (const char **in)
{
	const char *inptr = *in;
	
	decode_lwsp (&inptr);
	while (*inptr && *inptr != ']') {
		skip_domain_subliteral (&inptr);
		if (*inptr && *inptr != ']')
			inptr++;
	}
	
	*in = inptr;
}

static void
skip_domain (const char **in)
{
	const char *save, *inptr = *in;
	
	while (inptr && *inptr) {
		decode_lwsp (&inptr);
		if (*inptr == '[') {
			/* domain literal */
			inptr++;
			skip_domain_literal (&inptr);
			if (*inptr == ']')
				inptr++;
		} else {
			skip_atom (&inptr);
		}
		
		save = inptr;
		decode_lwsp (&inptr);
		if (*inptr != '.') {
			inptr = save;
			break;
		}
		
		inptr++;
	}
	
	*in = inptr;
}

static void
skip_addrspec (const char **in)
{
	const char *inptr = *in;
	
	decode_lwsp (&inptr);
	skip_word (&inptr);
	decode_lwsp (&inptr);
	
	while (*inptr == '.') {
		inptr++;
		skip_word (&inptr);
		decode_lwsp (&inptr);
	}
	
	if (*inptr == '@') {
		inptr++;
		skip_domain (&inptr);
	}
	
	*in = inptr;
}

static void
skip_addr (const char **in)
{
	const char *inptr = *in;
	
	decode_lwsp (&inptr);
	if (*inptr == '<') {
		inptr++;
		skip_addrspec (&inptr);
		if (*inptr == '>')
			inptr++;
	} else {
		skip_addrspec (&inptr);
	}
	
	*in = inptr;
}

static void
skip_msgid (const char **in)
{
	const char *inptr = *in;
	
	decode_lwsp (&inptr);
	if (*inptr == '<') {
		inptr++;
		skip_addrspec (&inptr);
		if (*inptr == '>')
			inptr++;
	} else {
		skip_atom (&inptr);
	}
	
	*in = inptr;
}


struct _received_part {
	struct _received_part *next;
	const char *start;
	size_t len;
};

static ssize_t
write_received (GMimeStream *stream, const char *name, const char *value)
{
	struct _received_part *parts, *part, *tail;
	const char *inptr, *lwsp = NULL;
	ssize_t nwritten;
	GString *str;
	size_t len;
	guint i;
	
	while (is_lwsp (*value))
		value++;
	
	if (*value == '\0')
		return 0;
	
	str = g_string_new (name);
	g_string_append_len (str, ": ", 2);
	len = 10;
	
	tail = parts = part = g_alloca (sizeof (struct _received_part));
	part->start = inptr = value;
	part->next = NULL;
	
	while (*inptr) {
		for (i = 0; i < G_N_ELEMENTS (received_tokens); i++) {
			if (!strncmp (inptr, received_tokens[i].token, received_tokens[i].len)) {
				if (inptr > part->start) {
					g_assert (lwsp != NULL);
					part->len = lwsp - part->start;
					
					part = g_alloca (sizeof (struct _received_part));
					part->start = inptr;
					part->next = NULL;
					
					tail->next = part;
					tail = part;
				}
				
				inptr += received_tokens[i].len;
				received_tokens[i].skip (&inptr);
				
				lwsp = inptr;
				while (is_lwsp (*inptr))
					inptr++;
				
				if (*inptr == ';') {
					inptr++;
					
					part->len = inptr - part->start;
					
					lwsp = inptr;
					while (is_lwsp (*inptr))
						inptr++;
					
					part = g_alloca (sizeof (struct _received_part));
					part->start = inptr;
					part->next = NULL;
					
					tail->next = part;
					tail = part;
				}
				
				break;
			}
		}
		
		if (i == G_N_ELEMENTS (received_tokens)) {
			while (*inptr && !is_lwsp (*inptr))
				inptr++;
			
			lwsp = inptr;
			while (is_lwsp (*inptr))
				inptr++;
		}
		
		if (*inptr == '(') {
			skip_comment (&inptr);
			
			lwsp = inptr;
			while (is_lwsp (*inptr))
				inptr++;
		}
	}
	
	part->len = lwsp - part->start;
	
	lwsp = NULL;
	part = parts;
	do {
		len += lwsp ? part->start - lwsp : 0;
		if (len + part->len > GMIME_FOLD_LEN && part != parts) {
			g_string_append (str, "\n\t");
			len = 1;
		} else if (lwsp) {
			g_string_append_len (str, lwsp, part->start - lwsp);
		}
		
		g_string_append_len (str, part->start, part->len);
		lwsp = part->start + part->len;
		len += part->len;
		
		part = part->next;
	} while (part != NULL);
	
	g_string_append_c (str, '\n');
	
	nwritten = g_mime_stream_write (stream, str->str, str->len);
	g_string_free (str, TRUE);
	
	return nwritten;
}

static ssize_t
write_subject (GMimeStream *stream, const char *name, const char *value)
{
	char *unfolded, *folded;
	ssize_t n;
	
	unfolded = g_strdup_printf ("%s: %s\n", name, value);
	folded = g_mime_utils_unstructured_header_fold (unfolded);
	g_free (unfolded);
	
	n = g_mime_stream_write_string (stream, folded);
	g_free (folded);
	
	return n;
}

static ssize_t
write_msgid (GMimeStream *stream, const char *name, const char *value)
{
	/* we don't want to wrap the Message-Id header - seems to
	   break a lot of clients (and servers) */
	return g_mime_stream_printf (stream, "%s: %s\n", name, value);
}

static ssize_t
write_addrspec (GMimeStream *stream, const char *name, const char *value)
{
	char *unfolded, *folded;
	ssize_t n;
	
	unfolded = g_strdup_printf ("%s: %s\n", name, value);
	folded = g_mime_utils_structured_header_fold (unfolded);
	g_free (unfolded);
	
	n = g_mime_stream_write_string (stream, folded);
	g_free (folded);
	
	return n;
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
	InternetAddressList *addrlist;
	int offset, i;
	time_t date;
	
	for (i = 0; headers[i]; i++) {
		if (!g_ascii_strcasecmp (headers[i], header))
			break;
	}
	
	switch (i) {
	case HEADER_FROM:
		g_free (message->from);
		addrlist = internet_address_parse_string (value);
		message->from = internet_address_list_to_string (addrlist, FALSE);
		internet_address_list_destroy (addrlist);
		break;
	case HEADER_REPLY_TO:
		g_free (message->reply_to);
		addrlist = internet_address_parse_string (value);
		message->reply_to = internet_address_list_to_string (addrlist, FALSE);
		internet_address_list_destroy (addrlist);
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
		g_free (message->subject);
		message->subject = g_mime_utils_header_decode_text (value);
		break;
	case HEADER_DATE:
		if (value) {
			date = g_mime_utils_header_decode_date (value, &offset);
			message->date = date;
			message->gmt_offset = offset;
		}
		break;
	case HEADER_MESSAGE_ID:
		g_free (message->message_id);
		message->message_id = g_mime_utils_decode_message_id (value);
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
	if (!g_ascii_strcasecmp ("MIME-Version", header))
		return;
	
	/* Make sure that the header is not a Content-* header, else it
           doesn't belong on a message */
	
	if (g_ascii_strncasecmp ("Content-", header, 8)) {
		if (process_header (object, header, value))
			g_mime_header_add (object->headers, header, value);
		else
			GMIME_OBJECT_CLASS (parent_class)->add_header (object, header, value);
	}
	
	if (((GMimeMessage *) object)->mime_part)
		g_mime_header_set_raw (((GMimeMessage *) object)->mime_part->headers, NULL);
}

static void
message_set_header (GMimeObject *object, const char *header, const char *value)
{
	if (!g_ascii_strcasecmp ("MIME-Version", header))
		return;
	
	/* Make sure that the header is not a Content-* header, else it
           doesn't belong on a message */
	
	if (g_ascii_strncasecmp ("Content-", header, 8)) {
		if (process_header (object, header, value))
			g_mime_header_set (object->headers, header, value);
		else
			GMIME_OBJECT_CLASS (parent_class)->set_header (object, header, value);
	}
	
	if (((GMimeMessage *) object)->mime_part)
		g_mime_header_set_raw (((GMimeMessage *) object)->mime_part->headers, NULL);
}

static const char *
message_get_header (GMimeObject *object, const char *header)
{
	/* Make sure that the header is not a Content-* header, else it
           doesn't belong on a message */
	
	if (!g_ascii_strcasecmp ("MIME-Version", header))
		return "1.0";
	
	if (g_ascii_strncasecmp ("Content-", header, 8) != 0)
		return GMIME_OBJECT_CLASS (parent_class)->get_header (object, header);
	else if (((GMimeMessage *) object)->mime_part)
		return g_mime_object_get_header (((GMimeMessage *) object)->mime_part, header);
	else
		return NULL;
}

static void
message_remove_header (GMimeObject *object, const char *header)
{
	GMimeMessage *message = (GMimeMessage *) object;
	InternetAddressList *addrlist;
	int i;
	
	if (!g_ascii_strcasecmp ("MIME-Version", header))
		return;
	
	/* Make sure that the header is not a Content-* header, else it
           doesn't belong on a message */
	
	if (!g_ascii_strncasecmp ("Content-", header, 8))
		return;
	
	for (i = 0; headers[i]; i++) {
		if (!g_ascii_strcasecmp (headers[i], header))
			break;
	}
	
	switch (i) {
	case HEADER_FROM:
		g_free (message->from);
		message->from = NULL;
		break;
	case HEADER_REPLY_TO:
		g_free (message->reply_to);
		message->reply_to = NULL;
		break;
	case HEADER_TO:
		addrlist = g_hash_table_lookup (message->recipients, GMIME_RECIPIENT_TYPE_TO);
		g_hash_table_remove (message->recipients, GMIME_RECIPIENT_TYPE_TO);
		internet_address_list_destroy (addrlist);
		break;
	case HEADER_CC:
		addrlist = g_hash_table_lookup (message->recipients, GMIME_RECIPIENT_TYPE_CC);
		g_hash_table_remove (message->recipients, GMIME_RECIPIENT_TYPE_CC);
		internet_address_list_destroy (addrlist);
		break;
	case HEADER_BCC:
		addrlist = g_hash_table_lookup (message->recipients, GMIME_RECIPIENT_TYPE_BCC);
		g_hash_table_remove (message->recipients, GMIME_RECIPIENT_TYPE_BCC);
		internet_address_list_destroy (addrlist);
		break;
	case HEADER_SUBJECT:
		g_free (message->subject);
		message->subject = NULL;
		break;
	case HEADER_DATE:
		message->date = 0;
		message->gmt_offset = 0;
		break;
	case HEADER_MESSAGE_ID:
		g_free (message->message_id);
		message->message_id = NULL;
		break;
	default:
		break;
	}
	
	GMIME_OBJECT_CLASS (parent_class)->remove_header (object, header);
	
	if (((GMimeMessage *) object)->mime_part)
		g_mime_header_set_raw (((GMimeMessage *) object)->mime_part->headers, NULL);
}


static char *
message_get_headers (GMimeObject *object)
{
	GMimeMessage *message = (GMimeMessage *) object;
	GMimeStream *stream;
	GByteArray *ba;
	char *str;
	
	ba = g_byte_array_new ();
	stream = g_mime_stream_mem_new ();
	g_mime_stream_mem_set_byte_array (GMIME_STREAM_MEM (stream), ba);
	
	if (message->mime_part && g_mime_header_has_raw (message->mime_part->headers)) {
		/* if the mime part has raw headers, then it contains the message headers as well */
		g_mime_header_write_to_stream (message->mime_part->headers, stream);
	} else {
		g_mime_header_write_to_stream (object->headers, stream);
		if (message->mime_part) {
			g_mime_stream_write_string (stream, "MIME-Version: 1.0\n");
			g_mime_header_write_to_stream (message->mime_part->headers, stream);
		}
	}
	
	g_object_unref (stream);
	g_byte_array_append (ba, (unsigned char *) "", 1);
	str = (char *) ba->data;
	g_byte_array_free (ba, FALSE);
	
	return str;
}

static ssize_t
message_write_to_stream (GMimeObject *object, GMimeStream *stream)
{
	GMimeMessage *message = (GMimeMessage *) object;
	ssize_t nwritten, total = 0;
	
	if (!(message->mime_part && g_mime_header_has_raw (message->mime_part->headers))) {
		/* write the content headers */
		if ((nwritten = g_mime_header_write_to_stream (object->headers, stream)) == -1)
			return -1;
		
		total += nwritten;
		
		if (message->mime_part) {
			if ((nwritten = g_mime_stream_write_string (stream, "MIME-Version: 1.0\n")) == -1)
				return -1;
			
			total += nwritten;
		}
	}
	
	if (message->mime_part) {
		nwritten = g_mime_object_write_to_stream (message->mime_part, stream);
	} else {
		if ((nwritten = g_mime_stream_write (stream, "\n", 1)) == -1)
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
	int i;
	
	message = g_object_new (GMIME_TYPE_MESSAGE, NULL, NULL);
	
	if (pretty_headers) {
		/* Populate with the "standard" rfc822 headers so we can have a standard order */
		for (i = 0; rfc822_headers[i]; i++) 
			g_mime_header_set (GMIME_OBJECT (message)->headers, rfc822_headers[i], NULL);
	}
	
	return message;
}


/**
 * g_mime_message_set_sender:
 * @message: MIME Message to change
 * @sender: The name and address of the sender
 *
 * Set the sender's name and address on the MIME Message.
 * (ex: "\"Joe Sixpack\" &lt;joe@sixpack.org&gt;")
 **/
void
g_mime_message_set_sender (GMimeMessage *message, const char *sender)
{
	InternetAddressList *addrlist;
	char *encoded;
	
	g_return_if_fail (GMIME_IS_MESSAGE (message));
	g_return_if_fail (sender != NULL);
	
	g_free (message->from);
	
	addrlist = internet_address_parse_string (sender);
	message->from = internet_address_list_to_string (addrlist, FALSE);
	encoded = internet_address_list_to_string (addrlist, TRUE);
	internet_address_list_destroy (addrlist);
	
	g_mime_header_set (GMIME_OBJECT (message)->headers, "From", encoded);
	g_free (encoded);
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
	g_return_if_fail (reply_to != NULL);
	
	g_free (message->reply_to);
	message->reply_to = g_strstrip (g_strdup (reply_to));
	
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
	const InternetAddressList *recipients;
	char *string;
	
	/* sync the specified recipient header */
	if ((recipients = g_mime_message_get_recipients (message, type))) {
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
	
	if ((addrlist = internet_address_parse_string (string))) {
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
const InternetAddressList *
g_mime_message_get_recipients (GMimeMessage *message, const char *type)
{
	g_return_val_if_fail (GMIME_IS_MESSAGE (message), NULL);
	g_return_val_if_fail (type != NULL, NULL);
	
	return g_hash_table_lookup (message->recipients, type);
}


static const char *recipient_types[] = {
	GMIME_RECIPIENT_TYPE_TO,
	GMIME_RECIPIENT_TYPE_CC,
	GMIME_RECIPIENT_TYPE_BCC
};

/**
 * g_mime_message_get_all_recipients:
 * @message: MIME Message
 *
 * Gets the complete list of recipients for @message.
 *
 * Returns a newly allocated #InternetAddressList containing all
 * recipients of the message.
 **/
InternetAddressList *
g_mime_message_get_all_recipients (GMimeMessage *message)
{
	InternetAddressList *list, *tail, *node, *recipients;
	guint i;
	
	g_return_val_if_fail (GMIME_IS_MESSAGE (message), NULL);
	
	list = NULL;
	tail = (InternetAddressList *) &list;
	
	for (i = 0; i < G_N_ELEMENTS (recipient_types); i++) {
		recipients = g_hash_table_lookup (message->recipients, recipient_types[i]);
		while (recipients != NULL) {
			internet_address_ref (recipients->address);
			node = g_new (InternetAddressList, 1);
			node->address = recipients->address;
			node->next = NULL;
			tail->next = node;
			tail = node;
			
			recipients = recipients->next;
		}
	}
	
	return list;
}


/**
 * g_mime_message_set_subject:
 * @message: MIME Message
 * @subject: Subject string
 *
 * Set the unencoded UTF-8 Subject field on a MIME Message.
 **/
void
g_mime_message_set_subject (GMimeMessage *message, const char *subject)
{
	char *encoded;
	
	g_return_if_fail (GMIME_IS_MESSAGE (message));
	g_return_if_fail (subject != NULL);
	
	g_free (message->subject);
	message->subject = g_strstrip (g_strdup (subject));
	
	encoded = g_mime_utils_header_encode_text (message->subject);
	g_mime_header_set (GMIME_OBJECT (message)->headers, "Subject", encoded);
	g_free (encoded);
}


/**
 * g_mime_message_get_subject:
 * @message: MIME Message
 *
 * Gets the message's subject.
 *
 * Returns the unencoded UTF-8 Subject field on a MIME Message.
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


/**
 * g_mime_message_set_message_id: 
 * @message: MIME Message
 * @message_id: message-id (addr-spec portion)
 *
 * Set the Message-Id on a message.
 **/
void
g_mime_message_set_message_id (GMimeMessage *message, const char *message_id)
{
	char *msgid;
	
	g_return_if_fail (GMIME_IS_MESSAGE (message));
	g_return_if_fail (message_id != NULL);
	
	g_free (message->message_id);
	message->message_id = g_strstrip (g_strdup (message_id));
	
	msgid = g_strdup_printf ("<%s>", message_id);
	g_mime_header_set (GMIME_OBJECT (message)->headers, "Message-Id", msgid);
	g_free (msgid);
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
	g_return_if_fail (value != NULL);
	
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
	g_return_if_fail (value != NULL);
	
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
 * g_mime_message_get_mime_part:
 * @message: MIME Message
 *
 * Gets the toplevel MIME part contained within @message.
 *
 * Returns the toplevel MIME part of @message.
 **/
GMimeObject *
g_mime_message_get_mime_part (GMimeMessage *message)
{
	g_return_val_if_fail (GMIME_IS_MESSAGE (message), NULL);
	
	if (message->mime_part == NULL)
		return NULL;
	
	g_object_ref (message->mime_part);
	
	return message->mime_part;
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
	
	g_object_ref (mime_part);
	g_mime_header_set_raw (mime_part->headers, NULL);
	
	if (message->mime_part)
		g_object_unref (message->mime_part);
	
	message->mime_part = mime_part;
}


/**
 * g_mime_message_write_to_stream:
 * @message: MIME Message
 * @stream: output stream
 *
 * Write the contents of the MIME Message to @stream.
 *
 * WARNING: This interface is deprecated. Use
 * g_mime_object_write_to_stream() instead.
 *
 * Returns %-1 on fail.
 **/
ssize_t
g_mime_message_write_to_stream (GMimeMessage *message, GMimeStream *stream)
{
	g_return_val_if_fail (GMIME_IS_MESSAGE (message), -1);
	g_return_val_if_fail (GMIME_IS_STREAM (stream), -1);
	
	return g_mime_object_write_to_stream ((GMimeObject *) message, stream);
}


/**
 * g_mime_message_to_string:
 * @message: MIME Message
 *
 * Allocates a string buffer containing the mime message @message.
 *
 * WARNING: This interface is deprecated. Use
 * g_mime_object_to_string() instead.
 *
 * Returns an allocated string containing the MIME Message.
 **/
char *
g_mime_message_to_string (GMimeMessage *message)
{
	g_return_val_if_fail (GMIME_IS_MESSAGE (message), NULL);
	
	return g_mime_object_to_string ((GMimeObject *) message);
}


/* The proper way to handle a multipart/alternative part is to return
 * the last part that we know how to render. For our purposes, we are
 * going to assume:
 *
 * If @want_plain is %FALSE then assume we can render text/plain and
 * text/html, thus the order of preference is text/html, then
 * text/plain and finally text/<any>.
 *
 * Otherwise, if @want_plain is %TRUE then we assume that we do not
 * know how to render text/html and so our order of preference becomes
 * text/plain and then text/<any>.
 **/
static GMimeObject *
handle_multipart_alternative (GMimeMultipart *multipart, gboolean want_plain, gboolean *is_html)
{
	GMimeObject *mime_part, *text_part = NULL;
	const GMimeContentType *type;
	GList *subpart;
	
	subpart = multipart->subparts;
	while (subpart) {
		mime_part = subpart->data;
		
		type = g_mime_object_get_content_type (mime_part);
		if (g_mime_content_type_is_type (type, "text", "*")) {
			if (!text_part || !g_ascii_strcasecmp (type->subtype, want_plain ? "plain" : "html")) {
				*is_html = !g_ascii_strcasecmp (type->subtype, "html");
				text_part = mime_part;
			}
		}
		
		subpart = subpart->next;
	}
	
	return text_part;
}

static GMimeObject *
handle_multipart_mixed (GMimeMultipart *multipart, gboolean want_plain, gboolean *is_html)
{
	GMimeObject *mime_part, *text_part = NULL;
	const GMimeContentType *type, *first_type = NULL;
	GList *subpart;
	
	subpart = multipart->subparts;
	while (subpart) {
		mime_part = subpart->data;
		
		type = g_mime_object_get_content_type (mime_part);
		if (GMIME_IS_MULTIPART (mime_part)) {
			multipart = GMIME_MULTIPART (mime_part);
			if (g_mime_content_type_is_type (type, "multipart", "alternative")) {
				mime_part = handle_multipart_alternative (multipart, want_plain, is_html);
				if (mime_part)
					return mime_part;
			} else {
				mime_part = handle_multipart_mixed (multipart, want_plain, is_html);
				if (mime_part && !text_part)
					text_part = mime_part;
			}
		} else if (g_mime_content_type_is_type (type, "text", "*")) {
			if (!g_ascii_strcasecmp (type->subtype, want_plain ? "plain" : "html")) {
				/* we got what we came for */
				*is_html = !g_ascii_strcasecmp (type->subtype, "html");
				return mime_part;
			}
			
			/* if we haven't yet found a text part or if it is a type we can
			 * understand and it is the first of that type, save it */
			if (!text_part || (!g_ascii_strcasecmp (type->subtype, "plain") && (first_type &&
					   g_ascii_strcasecmp (type->subtype, first_type->subtype) != 0))) {
				*is_html = !g_ascii_strcasecmp (type->subtype, "html");
				text_part = mime_part;
				first_type = type;
			}
		}
		
		subpart = subpart->next;
	}
	
	return text_part;
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
 * WARNING: This interface is deprecated.
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
	GMimeObject *mime_part = NULL;
	const GMimeContentType *type;
	GMimeMultipart *multipart;
	const char *content;
	char *body = NULL;
	size_t len = 0;
	
	g_return_val_if_fail (GMIME_IS_MESSAGE (message), NULL);
	g_return_val_if_fail (is_html != NULL, NULL);
	
	type = g_mime_object_get_content_type (message->mime_part);
	if (GMIME_IS_MULTIPART (message->mime_part)) {
		/* let's see if we can find a body in the multipart */
		multipart = GMIME_MULTIPART (message->mime_part);
		if (g_mime_content_type_is_type (type, "multipart", "alternative"))
			mime_part = handle_multipart_alternative (multipart, want_plain, is_html);
		else
			mime_part = handle_multipart_mixed (multipart, want_plain, is_html);
	} else if (g_mime_content_type_is_type (type, "text", "*")) {
		/* this *has* to be the message body */
		if (g_mime_content_type_is_type (type, "text", "html"))
			*is_html = TRUE;
		else
			*is_html = FALSE;
		
		mime_part = message->mime_part;
	}
	
	if (mime_part != NULL) {
		content = g_mime_part_get_content (GMIME_PART (mime_part), &len);
		body = g_strndup (content, len);
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
	else
		callback (message->mime_part, data);
}
