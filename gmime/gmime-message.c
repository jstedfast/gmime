/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*  GMime
 *  Copyright (C) 2000-2022 Jeffrey Stedfast
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


#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <string.h>
#include <ctype.h>
#include <locale.h>

#include "gmime-message.h"
#include "gmime-visitor.h"
#include "gmime-message-part.h"
#include "gmime-multipart.h"
#include "gmime-multipart-signed.h"
#include "gmime-multipart-encrypted.h"
#include "gmime-part.h"
#include "gmime-utils.h"
#include "gmime-common.h"
#include "gmime-stream-mem.h"
#include "gmime-stream-filter.h"
#include "gmime-parse-utils.h"
#include "gmime-internet-address-private.h"
#include "gmime-internal.h"
#include "gmime-events.h"


/**
 * SECTION: gmime-message
 * @title: GMimeMessage
 * @short_description: Messages
 * @see_also:
 *
 * A #GMimeMessage represents an rfc822 message.
 **/

static void g_mime_message_class_init (GMimeMessageClass *klass);
static void g_mime_message_init (GMimeMessage *message, GMimeMessageClass *klass);
static void g_mime_message_finalize (GObject *object);

/* GMimeEntity class methods */
static void message_header_added    (GMimeEntity *object, GMimeHeader *header);
static void message_header_changed  (GMimeEntity *object, GMimeHeader *header);
static void message_header_removed  (GMimeEntity *object, GMimeHeader *header);
static void message_headers_cleared (GMimeEntity *object);

static char *message_get_headers (GMimeEntity *object, GMimeFormatOptions *options);
static ssize_t message_write_to_stream (GMimeEntity *object, GMimeFormatOptions *options,
					gboolean content_only, GMimeStream *stream);
static void message_encode (GMimeEntity *object, GMimeEncodingConstraint constraint);
static gboolean message_prepare (GMimeEntity *object, GMimeEncodingConstraint constraint,
				 guint max_line_length, GError **error);

static void sender_changed (GMimeInternetAddressList *list, gpointer args, GMimeMessage *message);
static void from_changed (GMimeInternetAddressList *list, gpointer args, GMimeMessage *message);
static void reply_to_changed (GMimeInternetAddressList *list, gpointer args, GMimeMessage *message);
static void to_list_changed (GMimeInternetAddressList *list, gpointer args, GMimeMessage *message);
static void cc_list_changed (GMimeInternetAddressList *list, gpointer args, GMimeMessage *message);
static void bcc_list_changed (GMimeInternetAddressList *list, gpointer args, GMimeMessage *message);
static void resent_from_changed (GMimeInternetAddressList *list, gpointer args, GMimeMessage *message);
static void resent_reply_to_changed (GMimeInternetAddressList *list, gpointer args, GMimeMessage *message);
static void resent_to_changed (GMimeInternetAddressList *list, gpointer args, GMimeMessage *message);
static void resent_cc_changed (GMimeInternetAddressList *list, gpointer args, GMimeMessage *message);
static void resent_bcc_changed (GMimeInternetAddressList *list, gpointer args, GMimeMessage *message);
static void resent_sender_changed (GMimeInternetAddressList *list, gpointer args, GMimeMessage *message);
static void references_changed (GMimeMessageIdList *list, gpointer args, GMimeMessage *message);

static GMimeEntityClass *parent_class = NULL;

static void
message_accept (GMimeEntity *object, GMimeVisitor *visitor)
{
	g_mime_visitor_visit_message (visitor, (GMimeMessage *) object);
}

static struct {
	const char *name;
	GMimeEventCallback changed_cb;
} address_types[] = {
	{ "Sender",          (GMimeEventCallback) sender_changed          },
	{ "From",            (GMimeEventCallback) from_changed            },
	{ "Reply-To",        (GMimeEventCallback) reply_to_changed        },
	{ "To",              (GMimeEventCallback) to_list_changed         },
	{ "Cc",              (GMimeEventCallback) cc_list_changed         },
	{ "Bcc",             (GMimeEventCallback) bcc_list_changed        },
	{ "Resent-From",     (GMimeEventCallback) resent_from_changed     },
	{ "Resent-Reply-To", (GMimeEventCallback) resent_reply_to_changed },
	{ "Resent-To",       (GMimeEventCallback) resent_to_changed       },
	{ "Resent-Cc",       (GMimeEventCallback) resent_cc_changed       },
	{ "Resent-Bcc",      (GMimeEventCallback) resent_bcc_changed      },
	{ "Resent-Sender",   (GMimeEventCallback) resent_sender_changed   },
};

#define N_ADDRESS_TYPES G_N_ELEMENTS (address_types)

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
		
		type = g_type_register_static (GMIME_TYPE_ENTITY, "GMimeMessage", &info, 0);
	}
	
	return type;
}


static void
g_mime_message_class_init (GMimeMessageClass *klass)
{
	GMimeEntityClass *object_class = GMIME_ENTITY_CLASS (klass);
	GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
	
	parent_class = g_type_class_ref (GMIME_TYPE_ENTITY);
	
	gobject_class->finalize = g_mime_message_finalize;
	
	object_class->header_added = message_header_added;
	object_class->header_changed = message_header_changed;
	object_class->header_removed = message_header_removed;
	object_class->headers_cleared = message_headers_cleared;
	object_class->get_headers = message_get_headers;
	object_class->write_to_stream = message_write_to_stream;
	object_class->encode = message_encode;
	object_class->accept = message_accept;
	object_class->prepare_with_max_line_length = message_prepare;
}

static void
connect_changed_event (GMimeMessage *message, GMimeAddressType type)
{
	GMimeInternetAddressList *addrlist;
	
	addrlist = message->addrlists[type];
	
	g_mime_event_add (addrlist->changed, address_types[type].changed_cb, message);
}

static void
disconnect_changed_event (GMimeMessage *message, GMimeAddressType type)
{
	GMimeInternetAddressList *addrlist;
	
	addrlist = message->addrlists[type];
	
	g_mime_event_remove (addrlist->changed, address_types[type].changed_cb, message);
}

static void
block_changed_event (GMimeMessage *message, GMimeAddressType type)
{
	GMimeInternetAddressList *addrlist;
	
	addrlist = message->addrlists[type];
	
	g_mime_event_block (addrlist->changed, address_types[type].changed_cb, message);
}

static void
unblock_changed_event (GMimeMessage *message, GMimeAddressType type)
{
	GMimeInternetAddressList *addrlist;
	
	addrlist = message->addrlists[type];
	
	g_mime_event_unblock (addrlist->changed, address_types[type].changed_cb, message);
}

static void
g_mime_message_init (GMimeMessage *message, GMimeMessageClass *klass)
{
	guint i;
	
	message->addrlists = g_new (GMimeInternetAddressList *, N_ADDRESS_TYPES);
	((GMimeEntity *) message)->ensure_newline = TRUE;
	message->message_id = NULL;
	message->in_reply_to = NULL;
	message->resent_message_id = NULL;
	message->mime_part = NULL;
	message->subject = NULL;
	message->date = NULL;
	message->resent_date = NULL;
	message->importance = GMIME_MESSAGE_IMPORTANCE_NORMAL;
	message->priority = GMIME_MESSAGE_PRIORITY_NORMAL;
	message->x_priority = GMIME_X_MESSAGE_PRIORITY_NORMAL;
	message->references = g_mime_message_id_list_new ();
	g_mime_event_add (message->references->changed, (GMimeEventCallback) references_changed, message);
	
	/* initialize recipient lists */
	for (i = 0; i < N_ADDRESS_TYPES; i++) {
		message->addrlists[i] = g_mime_internet_address_list_new ();
		connect_changed_event (message, i);
	}
}

static void
g_mime_message_finalize (GObject *object)
{
	GMimeMessage *message = (GMimeMessage *) object;
	guint i;
	
	/* disconnect changed handlers */
	for (i = 0; i < N_ADDRESS_TYPES; i++) {
		disconnect_changed_event (message, i);
		g_object_unref (message->addrlists[i]);
	}
	
	g_free (message->addrlists);
	g_free (message->message_id);
	g_free (message->in_reply_to);
	g_free (message->resent_message_id);
	g_free (message->subject);
	g_free (message->marker);
	g_clear_pointer (&message->raw_marker, g_bytes_unref);
	g_mime_event_remove (message->references->changed, (GMimeEventCallback) references_changed, message);
	g_object_unref (message->references);
	
	if (message->date)
		g_date_time_unref (message->date);
	g_clear_pointer (&message->resent_date, g_date_time_unref);
	
	/* unref child mime part */
	if (message->mime_part)
		g_object_unref (message->mime_part);
	
	G_OBJECT_CLASS (parent_class)->finalize (object);
}


enum {
	HEADER_SENDER,
	HEADER_FROM,
	HEADER_REPLY_TO,
	HEADER_TO,
	HEADER_CC,
	HEADER_BCC,
	HEADER_RESENT_FROM,
	HEADER_RESENT_REPLY_TO,
	HEADER_RESENT_TO,
	HEADER_RESENT_CC,
	HEADER_RESENT_BCC,
	HEADER_RESENT_SENDER,
	HEADER_SUBJECT,
	HEADER_DATE,
	HEADER_RESENT_DATE,
	HEADER_MESSAGE_ID,
	HEADER_IN_REPLY_TO,
	HEADER_RESENT_MESSAGE_ID,
	HEADER_REFERENCES,
	HEADER_IMPORTANCE,
	HEADER_PRIORITY,
	HEADER_X_PRIORITY,
	HEADER_MIME_VERSION,
	HEADER_UNKNOWN
};

static const char *message_headers[] = {
	"Sender",
	"From",
	"Reply-To",
	"To",
	"Cc",
	"Bcc",
	"Resent-From",
	"Resent-Reply-To",
	"Resent-To",
	"Resent-Cc",
	"Resent-Bcc",
	"Resent-Sender",
	"Subject",
	"Date",
	"Resent-Date",
	"Message-Id",
	"In-Reply-To",
	"Resent-Message-Id",
	"References",
	"Importance",
	"Priority",
	"X-Priority",
	"MIME-Version"
};

static void
message_update_priority (GMimeMessage *message, guint type)
{
	GMimeHeaderList *headers = ((GMimeEntity *) message)->headers;
	GMimeHeader *header = g_mime_header_list_get_header (headers, message_headers[type]);
	const char *value = NULL;
	gint32 number;

	if (header) {
		value = type == HEADER_X_PRIORITY ? g_mime_header_get_raw_value (header) :
			g_mime_header_get_value (header);
	}

	switch (type) {
	case HEADER_IMPORTANCE:
		if (_g_mime_strcase_equal_trimmed (value, "high"))
			message->importance = GMIME_MESSAGE_IMPORTANCE_HIGH;
		else if (_g_mime_strcase_equal_trimmed (value, "low"))
			message->importance = GMIME_MESSAGE_IMPORTANCE_LOW;
		else
			message->importance = GMIME_MESSAGE_IMPORTANCE_NORMAL;
		break;
	case HEADER_PRIORITY:
		if (_g_mime_strcase_equal_trimmed (value, "urgent"))
			message->priority = GMIME_MESSAGE_PRIORITY_URGENT;
		else if (_g_mime_strcase_equal_trimmed (value, "non-urgent"))
			message->priority = GMIME_MESSAGE_PRIORITY_NON_URGENT;
		else
			message->priority = GMIME_MESSAGE_PRIORITY_NORMAL;
		break;
	case HEADER_X_PRIORITY:
		message->x_priority = GMIME_X_MESSAGE_PRIORITY_NORMAL;
		if (value) {
			skip_lwsp (&value);
			if (_g_mime_parse_int32 (&value, &number))
				message->x_priority = (GMimeXMessagePriority) CLAMP (number, 1, 5);
		}
		break;
	}
}

static void
message_update_date (GMimeMessage *message, const char *name, GDateTime **date)
{
	GMimeHeaderList *headers = ((GMimeEntity *) message)->headers;
	GMimeHeader *header = g_mime_header_list_get_header (headers, name);
	const char *value;

	g_clear_pointer (date, g_date_time_unref);
	if (header && (value = g_mime_header_get_raw_value (header)))
		*date = g_mime_utils_header_decode_date (value);
}

static void
message_update_references (GMimeMessage *message)
{
	GMimeHeaderList *headers = ((GMimeEntity *) message)->headers;
	GMimeMessageIdList *list = message->references;
	GMimeHeader *header;
	const char *value;
	char *id;

	g_mime_event_block (list->changed, (GMimeEventCallback) references_changed, message);
	g_mime_message_id_list_clear (list);
	header = g_mime_header_list_get_header (headers, "References");
	if (header && (value = g_mime_header_get_raw_value (header))) {
		while ((id = _g_mime_parse_reference (&value)))
			g_ptr_array_add (list->array, id);
	}
	g_mime_event_unblock (list->changed, (GMimeEventCallback) references_changed, message);
}

static void
message_update_reply_id (GMimeMessage *message, const char *name, char **id)
{
	GMimeHeaderList *headers = ((GMimeEntity *) message)->headers;
	const char *value;
	GMimeHeader *header;

	g_clear_pointer (id, g_free);
	header = g_mime_header_list_get_header (headers, name);
	if (header && (value = g_mime_header_get_raw_value (header))) {
		if (!g_ascii_strcasecmp (name, "In-Reply-To"))
			*id = _g_mime_parse_reference (&value);
		else
			*id = g_mime_utils_decode_message_id (value);
	}
}

static void
message_add_addresses (GMimeMessage *message, GMimeParserOptions *options, GMimeHeader *header, GMimeAddressType type)
{
	GMimeInternetAddressList *addrlist;
	const char *value;
	
	block_changed_event (message, type);
	
	addrlist = message->addrlists[type];
	
	if ((value = g_mime_header_get_raw_value (header)))
		_g_mime_internet_address_list_append_parse (addrlist, options, value, header->offset);
	
	unblock_changed_event (message, type);
}

static void
message_update_addresses (GMimeMessage *message, GMimeParserOptions *options, GMimeAddressType type)
{
	GMimeHeaderList *headers = ((GMimeEntity *) message)->headers;
	GMimeInternetAddressList *addrlist;
	const char *name, *value;
	GMimeHeader *header;
	int count, i;
	
	block_changed_event (message, type);
	
	addrlist = message->addrlists[type];
	
	g_mime_internet_address_list_clear (addrlist);
	
	count = g_mime_header_list_get_count (headers);
	for (i = 0; i < count; i++) {
		header = g_mime_header_list_get_header_at (headers, i);
		name = g_mime_header_get_name (header);
		
		if (g_ascii_strcasecmp (address_types[type].name, name) != 0)
			continue;
		
		if ((value = g_mime_header_get_raw_value (header)))
			_g_mime_internet_address_list_append_parse (addrlist, options, value, header->offset);
	}
	
	unblock_changed_event (message, type);
}

static gboolean
header_was_appended (GMimeEntity *object, GMimeHeaderListChangedAction action, GMimeHeader *header)
{
	// Note: This is a hack until we can have a message_header_inserted() API.
	if (action == GMIME_HEADER_LIST_CHANGED_ACTION_ADDED) {
		int count = g_mime_header_list_get_count (object->headers);
		GMimeHeader *last = g_mime_header_list_get_header_at (object->headers, count - 1);
		/* An earlier insertion can share the last header's identity. */
		return header == last && g_mime_header_list_index_of_header (object->headers, header) == count - 1;
	}

	return FALSE;
}

static void
process_header (GMimeEntity *object, GMimeHeaderListChangedAction action, GMimeHeader *header)
{
	GMimeParserOptions *options = _g_mime_header_list_get_options (object->headers);
	GMimeMessage *message = (GMimeMessage *) object;
	const char *name, *value;
	guint i;
	
	name = g_mime_header_get_name (header);
	
	for (i = 0; i < G_N_ELEMENTS (message_headers); i++) {
		if (!g_ascii_strcasecmp (message_headers[i], name))
			break;
	}
	
	switch (i) {
	case HEADER_SENDER:
		if (header_was_appended (object, action, header))
			message_add_addresses (message, options, header, GMIME_ADDRESS_TYPE_SENDER);
		else
			message_update_addresses (message, options, GMIME_ADDRESS_TYPE_SENDER);
		break;
	case HEADER_FROM:
		if (header_was_appended (object, action, header))
			message_add_addresses (message, options, header, GMIME_ADDRESS_TYPE_FROM);
		else
			message_update_addresses (message, options, GMIME_ADDRESS_TYPE_FROM);
		break;
	case HEADER_REPLY_TO:
		if (header_was_appended (object, action, header))
			message_add_addresses (message, options, header, GMIME_ADDRESS_TYPE_REPLY_TO);
		else
			message_update_addresses (message, options, GMIME_ADDRESS_TYPE_REPLY_TO);
		break;
	case HEADER_TO:
		if (header_was_appended (object, action, header))
			message_add_addresses (message, options, header, GMIME_ADDRESS_TYPE_TO);
		else
			message_update_addresses (message, options, GMIME_ADDRESS_TYPE_TO);
		break;
	case HEADER_CC:
		if (header_was_appended (object, action, header))
			message_add_addresses (message, options, header, GMIME_ADDRESS_TYPE_CC);
		else
			message_update_addresses (message, options, GMIME_ADDRESS_TYPE_CC);
		break;
	case HEADER_BCC:
		if (header_was_appended (object, action, header))
			message_add_addresses (message, options, header, GMIME_ADDRESS_TYPE_BCC);
		else
			message_update_addresses (message, options, GMIME_ADDRESS_TYPE_BCC);
		break;
	case HEADER_RESENT_FROM:
		if (header_was_appended (object, action, header))
			message_add_addresses (message, options, header, GMIME_ADDRESS_TYPE_RESENT_FROM);
		else
			message_update_addresses (message, options, GMIME_ADDRESS_TYPE_RESENT_FROM);
		break;
	case HEADER_RESENT_REPLY_TO:
		if (header_was_appended (object, action, header))
			message_add_addresses (message, options, header, GMIME_ADDRESS_TYPE_RESENT_REPLY_TO);
		else
			message_update_addresses (message, options, GMIME_ADDRESS_TYPE_RESENT_REPLY_TO);
		break;
	case HEADER_RESENT_TO:
		if (header_was_appended (object, action, header))
			message_add_addresses (message, options, header, GMIME_ADDRESS_TYPE_RESENT_TO);
		else
			message_update_addresses (message, options, GMIME_ADDRESS_TYPE_RESENT_TO);
		break;
	case HEADER_RESENT_CC:
		if (header_was_appended (object, action, header))
			message_add_addresses (message, options, header, GMIME_ADDRESS_TYPE_RESENT_CC);
		else
			message_update_addresses (message, options, GMIME_ADDRESS_TYPE_RESENT_CC);
		break;
	case HEADER_RESENT_BCC:
		if (header_was_appended (object, action, header))
			message_add_addresses (message, options, header, GMIME_ADDRESS_TYPE_RESENT_BCC);
		else
			message_update_addresses (message, options, GMIME_ADDRESS_TYPE_RESENT_BCC);
		break;
	case HEADER_RESENT_SENDER:
		if (header_was_appended (object, action, header))
			message_add_addresses (message, options, header, GMIME_ADDRESS_TYPE_RESENT_SENDER);
		else
			message_update_addresses (message, options, GMIME_ADDRESS_TYPE_RESENT_SENDER);
		break;
	case HEADER_SUBJECT:
		g_free (message->subject);
		
		if ((value = g_mime_header_get_value (header)))
			message->subject = g_strdup (value);
		else
			message->subject = NULL;
		break;
	case HEADER_DATE:
		message_update_date (message, name, &message->date);
		break;
	case HEADER_RESENT_DATE:
		message_update_date (message, name, &message->resent_date);
		break;
	case HEADER_MESSAGE_ID:
		g_free (message->message_id);
		
		if ((value = g_mime_header_get_value (header)))
			message->message_id = g_mime_utils_decode_message_id (value);
		else
			message->message_id = NULL;
		break;
	case HEADER_IN_REPLY_TO:
		message_update_reply_id (message, name, &message->in_reply_to);
		break;
	case HEADER_RESENT_MESSAGE_ID:
		message_update_reply_id (message, name, &message->resent_message_id);
		break;
	case HEADER_REFERENCES:
		message_update_references (message);
		break;
	case HEADER_IMPORTANCE:
	case HEADER_PRIORITY:
	case HEADER_X_PRIORITY:
		message_update_priority (message, i);
		break;
	}
}

static void
message_header_added (GMimeEntity *object, GMimeHeader *header)
{
	process_header (object, GMIME_HEADER_LIST_CHANGED_ACTION_ADDED, header);
	
	GMIME_ENTITY_CLASS (parent_class)->header_added (object, header);
}

static void
message_header_changed (GMimeEntity *object, GMimeHeader *header)
{
	process_header (object, GMIME_HEADER_LIST_CHANGED_ACTION_CHANGED, header);
	
	GMIME_ENTITY_CLASS (parent_class)->header_changed (object, header);
}

static void
message_header_removed (GMimeEntity *object, GMimeHeader *header)
{
	GMimeParserOptions *options = _g_mime_header_list_get_options (object->headers);
	GMimeMessage *message = (GMimeMessage *) object;
	const char *name;
	guint i;
	
	name = g_mime_header_get_name (header);
	
	for (i = 0; i < G_N_ELEMENTS (message_headers); i++) {
		if (!g_ascii_strcasecmp (message_headers[i], name))
			break;
	}
	
	switch (i) {
	case HEADER_SENDER:
		message_update_addresses (message, options, GMIME_ADDRESS_TYPE_SENDER);
		break;
	case HEADER_FROM:
		message_update_addresses (message, options, GMIME_ADDRESS_TYPE_FROM);
		break;
	case HEADER_REPLY_TO:
		message_update_addresses (message, options, GMIME_ADDRESS_TYPE_REPLY_TO);
		break;
	case HEADER_TO:
		message_update_addresses (message, options, GMIME_ADDRESS_TYPE_TO);
		break;
	case HEADER_CC:
		message_update_addresses (message, options, GMIME_ADDRESS_TYPE_CC);
		break;
	case HEADER_BCC:
		message_update_addresses (message, options, GMIME_ADDRESS_TYPE_BCC);
		break;
	case HEADER_RESENT_FROM:
		message_update_addresses (message, options, GMIME_ADDRESS_TYPE_RESENT_FROM);
		break;
	case HEADER_RESENT_REPLY_TO:
		message_update_addresses (message, options, GMIME_ADDRESS_TYPE_RESENT_REPLY_TO);
		break;
	case HEADER_RESENT_TO:
		message_update_addresses (message, options, GMIME_ADDRESS_TYPE_RESENT_TO);
		break;
	case HEADER_RESENT_CC:
		message_update_addresses (message, options, GMIME_ADDRESS_TYPE_RESENT_CC);
		break;
	case HEADER_RESENT_BCC:
		message_update_addresses (message, options, GMIME_ADDRESS_TYPE_RESENT_BCC);
		break;
	case HEADER_RESENT_SENDER:
		message_update_addresses (message, options, GMIME_ADDRESS_TYPE_RESENT_SENDER);
		break;
	case HEADER_SUBJECT:
		g_free (message->subject);
		message->subject = NULL;
		break;
	case HEADER_DATE:
		message_update_date (message, name, &message->date);
		break;
	case HEADER_RESENT_DATE:
		message_update_date (message, name, &message->resent_date);
		break;
	case HEADER_MESSAGE_ID:
		g_free (message->message_id);
		message->message_id = NULL;
		break;
	case HEADER_IN_REPLY_TO:
		message_update_reply_id (message, name, &message->in_reply_to);
		break;
	case HEADER_RESENT_MESSAGE_ID:
		message_update_reply_id (message, name, &message->resent_message_id);
		break;
	case HEADER_REFERENCES:
		message_update_references (message);
		break;
	case HEADER_IMPORTANCE:
	case HEADER_PRIORITY:
	case HEADER_X_PRIORITY:
		message_update_priority (message, i);
		break;
	}
	
	GMIME_ENTITY_CLASS (parent_class)->header_removed (object, header);
}

static void
message_headers_cleared (GMimeEntity *object)
{
	GMimeMessage *message = (GMimeMessage *) object;
	guint i;
	
	for (i = 0; i < N_ADDRESS_TYPES; i++) {
		block_changed_event (message, i);
		g_mime_internet_address_list_clear (message->addrlists[i]);
		unblock_changed_event (message, i);
	}
	
	g_free (message->message_id);
	message->message_id = NULL;
	g_clear_pointer (&message->in_reply_to, g_free);
	g_clear_pointer (&message->resent_message_id, g_free);
	message_update_references (message);
	message->importance = GMIME_MESSAGE_IMPORTANCE_NORMAL;
	message->priority = GMIME_MESSAGE_PRIORITY_NORMAL;
	message->x_priority = GMIME_X_MESSAGE_PRIORITY_NORMAL;
	g_free (message->subject);
	message->subject = NULL;
	
	if (message->date) {
		g_date_time_unref (message->date);
		message->date = NULL;
	}
	g_clear_pointer (&message->resent_date, g_date_time_unref);
	
	GMIME_ENTITY_CLASS (parent_class)->headers_cleared (object);
}


/**
 * g_mime_message_load:
 * @options: (nullable): parser options, or %NULL for defaults
 * @stream: the input stream at the start of the message
 * @error: return location for a #GError, or %NULL
 *
 * Loads a message, copying its content into memory. An optional leading
 * UTF-8 byte-order mark is consumed. The input need not support seeking
 * and is neither reset nor closed. The returned message remains usable
 * after @stream is closed, modified or released.
 *
 * Empty input is a parse error. This loads a single MIME message, not an
 * mbox or MMDF mailbox; use #GMimeParser for mailbox input.
 *
 * Returns: (nullable) (transfer full): the message, or %NULL on failure.
 **/
GMimeMessage *
g_mime_message_load (GMimeParserOptions *options, GMimeStream *stream, GError **error)
{
	return g_mime_message_load_with_storage (options, stream, GMIME_CONTENT_STORAGE_MEMORY, error);
}


/**
 * g_mime_message_load_with_storage:
 * @options: (nullable): parser options, or %NULL for defaults
 * @stream: input positioned at the start of the message
 * @storage: storage for parsed MIME part content
 * @error: return location for a #GError, or %NULL
 *
 * Loads a message using the requested content storage. Memory storage
 * behaves like g_mime_message_load(). Source storage references leaf
 * content through substreams and requires working tell, seek and substream
 * operations. Seekability is checked with a zero-offset seek from the
 * current position before reading. Unsupported operations and I/O failures
 * report %GMIME_ERROR rather than falling back to memory.
 *
 * The caller owns @stream in both modes. With source storage, keep it open
 * and unchanged while parsed content is in use. Content views retain it;
 * releasing the message does not close a stream still owned by the caller.
 * An optional leading UTF-8 byte-order mark is consumed. Empty input is a
 * parse error. The stream is neither reset nor closed.
 *
 * Returns: (nullable) (transfer full): the message, or %NULL on failure.
 **/
GMimeMessage *
g_mime_message_load_with_storage (GMimeParserOptions *options, GMimeStream *stream,
				 GMimeContentStorage storage, GError **error)
{
	return (GMimeMessage *) _g_mime_parser_load (options, stream, storage, TRUE, error);
}


/**
 * g_mime_message_load_from_file:
 * @options: (nullable): parser options, or %NULL for defaults
 * @filename: (type filename): the input file path
 * @error: return location for a #GError, or %NULL
 *
 * Loads a message from @filename using g_mime_message_load(). The file
 * is opened for reading and closed before returning, on success or
 * failure. Content is copied into memory and does not retain the file.
 * Open, read, parse and close failures return %NULL with @error set.
 *
 * Returns: (nullable) (transfer full): the message, or %NULL on failure.
 **/
GMimeMessage *
g_mime_message_load_from_file (GMimeParserOptions *options, const char *filename, GError **error)
{
	return (GMimeMessage *) _g_mime_parser_load_from_file (options, filename, TRUE, error);
}


/* Write the message headers (merged with the headers of the body, if any). */
ssize_t
_g_mime_message_write_headers (GMimeEntity *object, GMimeFormatOptions *options, GMimeStream *stream)
{
	GMimeMessage *message = (GMimeMessage *) object;
	GMimeEntity *mime_part = message->mime_part;
	
	if (mime_part != NULL) {
		int body_count = g_mime_header_list_get_count (mime_part->headers);
		int count = g_mime_header_list_get_count (object->headers);
		GMimeHeader *header, *body_header;
		ssize_t total;
		gint64 body_offset, offset;
		GMimeStream *filtered;
		GMimeFilter *filter;
		int body_index = 0;
		int index = 0;
		
		filtered = g_mime_stream_filter_new (stream);
		filter = g_mime_format_options_create_newline_filter (options, FALSE);
		g_mime_stream_filter_add ((GMimeStreamFilter *) filtered, filter);
		g_object_unref (filter);
		
		/* write all the prepended message headers first */
		while (index < count) {
			header = g_mime_header_list_get_header_at (object->headers, index);
			if (g_mime_header_get_offset (header) >= 0)
				break;

			if (!g_mime_format_options_is_hidden_header (options, header->name) &&
			    g_mime_header_write_to_stream (header, options, filtered) == -1) {
				g_object_unref (filtered);
				return -1;
			}

			index++;
		}

		/* now merge the message and body headers as they appeared in the raw message */
		while (index < count && body_index < body_count) {
			body_header = g_mime_header_list_get_header_at (mime_part->headers, body_index);
			if ((body_offset = g_mime_header_get_offset (body_header)) < 0)
				break;
			
			header = g_mime_header_list_get_header_at (object->headers, index);
			offset = g_mime_header_get_offset (header);
			
			if (offset >= 0 && offset < body_offset) {
				if (!g_mime_format_options_is_hidden_header (options, header->name)) {
					if (g_mime_header_write_to_stream (header, options, filtered) == -1) {
						g_object_unref (filtered);
						return -1;
					}
				}
				
				index++;
			} else {
				if (!g_mime_format_options_is_hidden_header (options, body_header->name)) {
					if (g_mime_header_write_to_stream (body_header, options, filtered) == -1) {
						g_object_unref (filtered);
						return -1;
					}
				}
				
				body_index++;
			}
		}
		
		while (index < count) {
			header = g_mime_header_list_get_header_at (object->headers, index);
			
			if (!g_mime_format_options_is_hidden_header (options, header->name)) {
				if (g_mime_header_write_to_stream (header, options, filtered) == -1) {
					g_object_unref (filtered);
					return -1;
				}
			}
			
			index++;
		}
		
		while (body_index < body_count) {
			header = g_mime_header_list_get_header_at (mime_part->headers, body_index);
			
			if (!g_mime_format_options_is_hidden_header (options, header->name)) {
				if (g_mime_header_write_to_stream (header, options, filtered) == -1) {
					g_object_unref (filtered);
					return -1;
				}
			}
			
			body_index++;
		}
		
		if (g_mime_stream_flush (filtered) == -1) {
			int errsv = errno;

			g_object_unref (filtered);
			errno = errsv;
			return -1;
		}

		total = _g_mime_stream_filter_get_written ((GMimeStreamFilter *) filtered);
		g_object_unref (filtered);
		
		return total;
	}
	
	return _g_mime_header_list_write_to_stream (object->headers, options, stream);
}


/**
 * g_mime_message_write_headers_to_stream:
 * @message: a #GMimeMessage
 * @options: (nullable): formatting options, or %NULL for defaults
 * @stream: the output stream
 *
 * Writes the message headers, merged with the body's headers when present,
 * and the blank line separating headers from content. A parsed message
 * without a separator retains that state, as in full-message output.
 *
 * Unlike g_mime_entity_get_headers(), this includes the header/body
 * separator when present. It does not read or serialize the body, invoke
 * its virtual writer, or apply body newline completion. The caller's
 * output stream remains open.
 *
 * Returns: the number of bytes written, or %-1 on failure.
 **/
ssize_t
g_mime_message_write_headers_to_stream (GMimeMessage *message, GMimeFormatOptions *options, GMimeStream *stream)
{
	GMimeEntity *object = (GMimeEntity *) message;
	GMimeHeaderList *headers;
	const char *newline;
	ssize_t total, nwritten;
	size_t length;

	g_return_val_if_fail (GMIME_IS_MESSAGE (message), -1);
	g_return_val_if_fail (GMIME_IS_STREAM (stream), -1);

	if ((total = _g_mime_message_write_headers (object, options, stream)) < 0)
		return -1;

	headers = message->mime_part ? message->mime_part->headers : object->headers;

	if (headers->has_body_separator) {
		newline = g_mime_format_options_get_newline (options);
		length = strlen (newline);

		if (length > (size_t) (G_MAXSSIZE - total)) {
			errno = EOVERFLOW;
			return -1;
		}

		if ((nwritten = _g_mime_stream_write_all (stream, newline, length)) < 0)
			return -1;

		total += nwritten;
	}

	return total;
}


static char *
message_get_headers (GMimeEntity *object, GMimeFormatOptions *options)
{
	GMimeStream *stream;
	GByteArray *ba;
	char *str;
	
	ba = g_byte_array_new ();
	stream = g_mime_stream_mem_new ();
	g_mime_stream_mem_set_byte_array ((GMimeStreamMem *) stream, ba);
	if (_g_mime_message_write_headers (object, options, stream) == -1) {
		int errsv = errno;

		g_object_unref (stream);
		g_byte_array_free (ba, TRUE);
		errno = errsv;
		return NULL;
	}
	g_object_unref (stream);
	g_byte_array_append (ba, (unsigned char *) "", 1);
	str = (char *) ba->data;
	g_byte_array_free (ba, FALSE);
	
	return str;
}

static ssize_t
message_write_to_stream (GMimeEntity *object, GMimeFormatOptions *options, gboolean content_only, GMimeStream *stream)
{
	return _g_mime_writer_write (object, options, content_only, stream);
}

static void
message_encode (GMimeEntity *object, GMimeEncodingConstraint constraint)
{
	GMimeMessage *message = (GMimeMessage *) object;
	
	if (message->mime_part != NULL)
		g_mime_entity_encode (message->mime_part, constraint);
}



static gboolean
message_has_mime_version (GMimeMessage *message)
{
	GMimeHeader *header;
	const char *inptr;
	gint32 value;
	guint count = 0;

	header = g_mime_header_list_get_header (GMIME_ENTITY (message)->headers, "MIME-Version");
	if (!header)
		return FALSE;

	/* MimeKit parses the first raw header as two to four Int32 components,
	 * allowing comments and whitespace without normalizing a valid value. */
	inptr = g_mime_header_get_raw_value (header);
	do {
		if (!skip_cfws (&inptr) || !_g_mime_parse_int32 (&inptr, &value) || ++count > 4)
			return FALSE;
		if (!skip_cfws (&inptr))
			return FALSE;
		if (!*inptr)
			break;
		if (*inptr++ != '.')
			return FALSE;
	} while (*inptr);

	return count >= 2;
}


static gboolean
message_prepare (GMimeEntity *object, GMimeEncodingConstraint constraint, guint max_line_length, GError **error)
{
	GMimeMessage *message = (GMimeMessage *) object;

	if (!message->mime_part)
		return TRUE;

	/* Publish MIME-Version before preparing the body, even if preparation
	 * later fails. Existing valid versions and duplicates remain untouched. */
	if (g_mime_header_list_get_count (message->mime_part->headers) > 0 && !message_has_mime_version (message))
		g_mime_header_list_set (object->headers, "MIME-Version", "1.0", NULL);

	return g_mime_entity_prepare_with_max_line_length (message->mime_part, constraint, max_line_length, error);
}


/**
 * g_mime_message_new:
 * @pretty_headers: make pretty headers 
 *
 * If @pretty_headers is %TRUE, then the standard rfc822 headers are
 * initialized so as to put headers in a nice friendly order. This is
 * strictly a cosmetic thing, so if you are unsure, it is safe to say
 * no (%FALSE).
 *
 * Returns: an empty #GMimeMessage object.
 **/
GMimeMessage *
g_mime_message_new (gboolean pretty_headers)
{
	GMimeHeaderList *headers;
	GMimeMessage *message;
	guint i;
	
	message = g_object_new (GMIME_TYPE_MESSAGE, NULL);
	
	if (pretty_headers) {
		/* Populate with the "standard" rfc822 headers so we can have a standard order */
		headers = ((GMimeEntity *) message)->headers;
		
		_g_mime_entity_block_header_list_changed ((GMimeEntity *) message);
		for (i = 0; i < G_N_ELEMENTS (rfc822_headers); i++) 
			g_mime_header_list_set (headers, rfc822_headers[i], NULL, NULL);
		_g_mime_entity_unblock_header_list_changed ((GMimeEntity *) message);
	}
	
	return message;
}


/**
 * g_mime_message_get_sender:
 * @message: A #GMimeMessage
 *
 * Gets the persistent list of addresses from all Sender headers,
 * preserving mailboxes and groups. Missing headers yield an empty list.
 * Header changes refresh the list in place; list and nested address edits
 * rewrite the Sender header. Clearing the list removes all Sender headers.
 *
 * This is the same list returned by g_mime_message_get_addresses() with
 * %GMIME_ADDRESS_TYPE_SENDER. This intentionally differs from MimeKit's
 * single-mailbox Sender property to support group syntax.
 *
 * Returns: (transfer none): the Sender address list.
 **/
GMimeInternetAddressList *
g_mime_message_get_sender (GMimeMessage *message)
{
	g_return_val_if_fail (GMIME_IS_MESSAGE (message), NULL);

	return message->addrlists[GMIME_ADDRESS_TYPE_SENDER];
}


/**
 * g_mime_message_get_from:
 * @message: A #GMimeMessage
 *
 * Gets the parsed list of addresses in the From header.
 *
 * Returns: (transfer none): the parsed list of addresses in the From header.
 **/
GMimeInternetAddressList *
g_mime_message_get_from (GMimeMessage *message)
{
	g_return_val_if_fail (GMIME_IS_MESSAGE (message), NULL);
	
	return message->addrlists[GMIME_ADDRESS_TYPE_FROM];
}


/**
 * g_mime_message_get_reply_to:
 * @message: A #GMimeMessage
 *
 * Gets the parsed list of addresses in the Reply-To header.
 *
 * Returns: (transfer none): the parsed list of addresses in the Reply-To header.
 **/
GMimeInternetAddressList *
g_mime_message_get_reply_to (GMimeMessage *message)
{
	g_return_val_if_fail (GMIME_IS_MESSAGE (message), NULL);
	
	return message->addrlists[GMIME_ADDRESS_TYPE_REPLY_TO];
}


/**
 * g_mime_message_get_to:
 * @message: A #GMimeMessage
 *
 * Gets combined list of parsed addresses in the To header(s).
 *
 * Returns: (transfer none): the parsed list of addresses in the To header(s).
 **/
GMimeInternetAddressList *
g_mime_message_get_to (GMimeMessage *message)
{
	g_return_val_if_fail (GMIME_IS_MESSAGE (message), NULL);
	
	return message->addrlists[GMIME_ADDRESS_TYPE_TO];
}


/**
 * g_mime_message_get_cc:
 * @message: A #GMimeMessage
 *
 * Gets combined list of parsed addresses in the Cc header(s).
 *
 * Returns: (transfer none): the parsed list of addresses in the Cc header(s).
 **/
GMimeInternetAddressList *
g_mime_message_get_cc (GMimeMessage *message)
{
	g_return_val_if_fail (GMIME_IS_MESSAGE (message), NULL);
	
	return message->addrlists[GMIME_ADDRESS_TYPE_CC];
}


/**
 * g_mime_message_get_bcc:
 * @message: A #GMimeMessage
 *
 * Gets combined list of parsed addresses in the Bcc header(s).
 *
 * Returns: (transfer none): the parsed list of addresses in the Bcc header(s).
 **/
GMimeInternetAddressList *
g_mime_message_get_bcc (GMimeMessage *message)
{
	g_return_val_if_fail (GMIME_IS_MESSAGE (message), NULL);
	
	return message->addrlists[GMIME_ADDRESS_TYPE_BCC];
}


/**
 * g_mime_message_get_resent_from:
 * @message: a #GMimeMessage
 *
 * Gets the persistent, mutable list combining all Resent-From headers in
 * header order. Raw-header edits refresh this same list without rewriting
 * the headers. List and nested address changes replace matching headers;
 * emptying the list removes them all. A retained list is safe to use after
 * the message is finalized and no longer updates the message.
 *
 * Returns: (transfer none): the resent authors.
 **/
GMimeInternetAddressList *
g_mime_message_get_resent_from (GMimeMessage *message)
{
	g_return_val_if_fail (GMIME_IS_MESSAGE (message), NULL);

	return message->addrlists[GMIME_ADDRESS_TYPE_RESENT_FROM];
}

/**
 * g_mime_message_get_resent_reply_to:
 * @message: a #GMimeMessage
 *
 * Gets the persistent, mutable list combining all Resent-Reply-To headers.
 * Changes to the list update the header, removing it when empty.
 *
 * Returns: (transfer none): the resent reply addresses.
 **/
GMimeInternetAddressList *
g_mime_message_get_resent_reply_to (GMimeMessage *message)
{
	g_return_val_if_fail (GMIME_IS_MESSAGE (message), NULL);

	return message->addrlists[GMIME_ADDRESS_TYPE_RESENT_REPLY_TO];
}

/**
 * g_mime_message_get_resent_to:
 * @message: a #GMimeMessage
 *
 * Gets the persistent, mutable list combining all Resent-To headers.
 * Changes to the list update the header, removing it when empty.
 *
 * Returns: (transfer none): the resent primary recipients.
 **/
GMimeInternetAddressList *
g_mime_message_get_resent_to (GMimeMessage *message)
{
	g_return_val_if_fail (GMIME_IS_MESSAGE (message), NULL);

	return message->addrlists[GMIME_ADDRESS_TYPE_RESENT_TO];
}

/**
 * g_mime_message_get_resent_cc:
 * @message: a #GMimeMessage
 *
 * Gets the persistent, mutable list combining all Resent-Cc headers.
 * Changes to the list update the header, removing it when empty.
 *
 * Returns: (transfer none): the resent secondary recipients.
 **/
GMimeInternetAddressList *
g_mime_message_get_resent_cc (GMimeMessage *message)
{
	g_return_val_if_fail (GMIME_IS_MESSAGE (message), NULL);

	return message->addrlists[GMIME_ADDRESS_TYPE_RESENT_CC];
}

/**
 * g_mime_message_get_resent_bcc:
 * @message: a #GMimeMessage
 *
 * Gets the persistent, mutable list combining all Resent-Bcc headers.
 * Changes to the list update the header, removing it when empty.
 *
 * Returns: (transfer none): the resent blind-copy recipients.
 **/
GMimeInternetAddressList *
g_mime_message_get_resent_bcc (GMimeMessage *message)
{
	g_return_val_if_fail (GMIME_IS_MESSAGE (message), NULL);

	return message->addrlists[GMIME_ADDRESS_TYPE_RESENT_BCC];
}

/**
 * g_mime_message_get_resent_sender:
 * @message: a #GMimeMessage
 *
 * Gets the persistent list of addresses from all Resent-Sender headers,
 * preserving mailboxes and groups. Missing headers yield an empty list.
 * Header changes refresh the list in place; list and nested address edits
 * rewrite the Resent-Sender header. Clearing the list removes all matching
 * headers.
 *
 * This is the same list returned by g_mime_message_get_addresses() with
 * %GMIME_ADDRESS_TYPE_RESENT_SENDER. This intentionally differs from MimeKit's
 * single-mailbox ResentSender property to support group syntax.
 *
 * Returns: (transfer none): the Resent-Sender address list.
 **/
GMimeInternetAddressList *
g_mime_message_get_resent_sender (GMimeMessage *message)
{
	g_return_val_if_fail (GMIME_IS_MESSAGE (message), NULL);

	return message->addrlists[GMIME_ADDRESS_TYPE_RESENT_SENDER];
}

static void
references_changed (GMimeMessageIdList *list, gpointer args, GMimeMessage *message)
{
	GMimeEntity *object = (GMimeEntity *) message;
	const char *newline = g_mime_format_options_get_newline (NULL);
	size_t maxline = g_mime_format_options_get_max_line_length (NULL);
	size_t line = strlen ("References:");
	const char *id;
	GString *str;
	size_t length;
	guint i;

	_g_mime_entity_block_header_list_changed (object);
	if (list->array->len) {
		str = g_string_new (NULL);
		for (i = 0; i < list->array->len; i++) {
			id = list->array->pdata[i];
			length = _g_mime_count_utf16_units (id, id + strlen (id));
			if (i > 0 && line + length + 2 >= maxline) {
				g_string_append (str, newline);
				g_string_append_c (str, '\t');
				line = 1;
			} else {
				g_string_append_c (str, ' ');
				line++;
			}
			/* Match MimeKit's running column count, excluding the brackets. */
			line += length;
			g_string_append_c (str, '<');
			g_string_append (str, id);
			g_string_append_c (str, '>');
		}
		g_string_append (str, newline);
		_g_mime_header_list_set (object->headers, "References", str->str);
		g_string_free (str, TRUE);
	} else {
		g_mime_header_list_remove_all (object->headers, "References");
	}
	_g_mime_entity_unblock_header_list_changed (object);
}

static void
sync_g_mime_internet_address_list (GMimeInternetAddressList *list, GMimeMessage *message, const char *name)
{
	GMimeEntity *object = (GMimeEntity *) message;
	GString *str;
	guint n;
	
	if (g_mime_internet_address_list_length (list) == 0) {
		_g_mime_entity_block_header_list_changed (object);
		g_mime_header_list_remove_all (object->headers, name);
		_g_mime_entity_unblock_header_list_changed (object);
		return;
	}

	str = g_string_new (name);
	g_string_append_c (str, ':');
	n = str->len;
	
	g_string_append_c (str, ' ');
	g_mime_internet_address_list_encode (list, NULL, str);
	
	_g_mime_entity_block_header_list_changed (object);
	_g_mime_header_list_set (object->headers, name, str->str + n);
	_g_mime_entity_unblock_header_list_changed (object);
	
	g_string_free (str, TRUE);
}

static void
sync_address_header (GMimeMessage *message, GMimeAddressType type)
{
	GMimeInternetAddressList *list = message->addrlists[type];
	const char *name = address_types[type].name;
	
	sync_g_mime_internet_address_list (list, message, name);
}

static void
sender_changed (GMimeInternetAddressList *list, gpointer args, GMimeMessage *message)
{
	sync_address_header (message, GMIME_ADDRESS_TYPE_SENDER);
}

static void
from_changed (GMimeInternetAddressList *list, gpointer args, GMimeMessage *message)
{
	sync_address_header (message, GMIME_ADDRESS_TYPE_FROM);
}

static void
reply_to_changed (GMimeInternetAddressList *list, gpointer args, GMimeMessage *message)
{
	sync_address_header (message, GMIME_ADDRESS_TYPE_REPLY_TO);
}

static void
to_list_changed (GMimeInternetAddressList *list, gpointer args, GMimeMessage *message)
{
	sync_address_header (message, GMIME_ADDRESS_TYPE_TO);
}

static void
cc_list_changed (GMimeInternetAddressList *list, gpointer args, GMimeMessage *message)
{
	sync_address_header (message, GMIME_ADDRESS_TYPE_CC);
}

static void
bcc_list_changed (GMimeInternetAddressList *list, gpointer args, GMimeMessage *message)
{
	sync_address_header (message, GMIME_ADDRESS_TYPE_BCC);
}

static void
resent_from_changed (GMimeInternetAddressList *list, gpointer args, GMimeMessage *message)
{
	sync_address_header (message, GMIME_ADDRESS_TYPE_RESENT_FROM);
}

static void
resent_reply_to_changed (GMimeInternetAddressList *list, gpointer args, GMimeMessage *message)
{
	sync_address_header (message, GMIME_ADDRESS_TYPE_RESENT_REPLY_TO);
}

static void
resent_to_changed (GMimeInternetAddressList *list, gpointer args, GMimeMessage *message)
{
	sync_address_header (message, GMIME_ADDRESS_TYPE_RESENT_TO);
}

static void
resent_cc_changed (GMimeInternetAddressList *list, gpointer args, GMimeMessage *message)
{
	sync_address_header (message, GMIME_ADDRESS_TYPE_RESENT_CC);
}

static void
resent_bcc_changed (GMimeInternetAddressList *list, gpointer args, GMimeMessage *message)
{
	sync_address_header (message, GMIME_ADDRESS_TYPE_RESENT_BCC);
}

static void
resent_sender_changed (GMimeInternetAddressList *list, gpointer args, GMimeMessage *message)
{
	sync_address_header (message, GMIME_ADDRESS_TYPE_RESENT_SENDER);
}


/**
 * g_mime_message_add_mailbox:
 * @message: A #GMimeMessage
 * @type: A #GMimeAddressType
 * @name: (nullable): The name of the mailbox (or %NULL)
 * @addr: The address of the mailbox
 * @error: (nullable): return location for a #GError
 *
 * Add a mailbox of a chosen type to the MIME message.
 *
 * Note: The @name (and @addr) strings should be in UTF-8.
 *
 * Returns: whether the mailbox was valid and added.
 **/
gboolean
g_mime_message_add_mailbox (GMimeMessage *message, GMimeAddressType type, const char *name, const char *addr, GError **error)
{
	GMimeInternetAddressList *addrlist;
	GMimeInternetAddress *ia;
	
	g_return_val_if_fail (GMIME_IS_MESSAGE (message), FALSE);
	g_return_val_if_fail (type >= 0 && type < N_ADDRESS_TYPES, FALSE);
	g_return_val_if_fail (addr != NULL, FALSE);
	
	addrlist = message->addrlists[type];

	if (!(ia = g_mime_mailbox_address_new (name, addr, error)))
		return FALSE;

	g_mime_internet_address_list_add (addrlist, ia);
	g_object_unref (ia);

	return TRUE;
}


/**
 * g_mime_message_get_addresses:
 * @message: A #GMimeMessage
 * @type: A #GMimeAddressType
 *
 * Gets a list of addresses of the specified @type from the @message.
 *
 * Returns: (transfer none): a list of addresses of the specified
 * @type from the @message.
 **/
GMimeInternetAddressList *
g_mime_message_get_addresses (GMimeMessage *message, GMimeAddressType type)
{
	g_return_val_if_fail (GMIME_IS_MESSAGE (message), NULL);
	g_return_val_if_fail (type < N_ADDRESS_TYPES, NULL);
	
	return message->addrlists[type];
}


/**
 * g_mime_message_get_all_recipients:
 * @message: A #GMimeMessage
 *
 * Combines the ordinary To, Cc, and Bcc lists, preserving groups and
 * duplicates. This legacy accessor does not select resent recipients;
 * use g_mime_message_get_recipients() for MimeKit-style recipient selection.
 *
 * Returns: (transfer full): a newly allocated #GMimeInternetAddressList
 * containing all recipients of the message or %NULL if no recipients
 * are set.
 **/
GMimeInternetAddressList *
g_mime_message_get_all_recipients (GMimeMessage *message)
{
	GMimeInternetAddressList *recipients, *list = NULL;
	guint i;
	
	g_return_val_if_fail (GMIME_IS_MESSAGE (message), NULL);
	
	for (i = GMIME_ADDRESS_TYPE_TO; i <= GMIME_ADDRESS_TYPE_BCC; i++) {
		recipients = message->addrlists[i];
		
		if (g_mime_internet_address_list_length (recipients) == 0)
			continue;
		
		if (list == NULL)
			list = g_mime_internet_address_list_new ();
		
		g_mime_internet_address_list_append (list, recipients);
	}
	
	return list;
}


/**
 * g_mime_message_get_recipients:
 * @message: a #GMimeMessage
 * @only_unique: whether to include only the first mailbox for each address
 *
 * Gets a mailbox-only snapshot in To, Cc, then Bcc order, flattening groups
 * depth-first. If the Resent-Sender or Resent-From address list is nonempty,
 * only Resent-To, Resent-Cc, and Resent-Bcc are used, even when
 * they contain no mailboxes. Merely having resent recipient headers does
 * not select them.
 *
 * With @only_unique, local parts remain case-sensitive and domains are
 * IDN-encoded and converted to ASCII lowercase, as in address ordering.
 * Domainless addresses are compared as stored. Display names and source
 * routes do not affect deduplication. The first original mailbox instance
 * wins.
 *
 * The result owns references to original mailboxes, not clones. Its
 * membership is independent of the message, but edits to shared mailboxes
 * are visible through the original address lists.
 *
 * Returns: (transfer full): a newly allocated list containing only mailboxes,
 * which is empty when there are no selected recipients.
 **/
GMimeInternetAddressList *
g_mime_message_get_recipients (GMimeMessage *message, gboolean only_unique)
{
	GMimeInternetAddressList *result, *mailboxes;
	GMimeInternetAddress *mailbox;
	GHashTable *unique = NULL;
	guint first, last, type, i;

	g_return_val_if_fail (GMIME_IS_MESSAGE (message), NULL);

	if (g_mime_internet_address_list_length (message->addrlists[GMIME_ADDRESS_TYPE_RESENT_SENDER]) > 0 ||
	    g_mime_internet_address_list_length (message->addrlists[GMIME_ADDRESS_TYPE_RESENT_FROM]) > 0) {
		first = GMIME_ADDRESS_TYPE_RESENT_TO;
		last = GMIME_ADDRESS_TYPE_RESENT_BCC;
	} else {
		first = GMIME_ADDRESS_TYPE_TO;
		last = GMIME_ADDRESS_TYPE_BCC;
	}

	if (only_unique)
		unique = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, NULL);

	result = g_mime_internet_address_list_new ();

	for (type = first; type <= last; type++) {
		/* FIXME: this could probably be done via some sort of foreach() instead of allocating an array */
		mailboxes = g_mime_internet_address_list_get_mailboxes (message->addrlists[type]);

		for (i = 0; i < mailboxes->array->len; i++) {
			mailbox = mailboxes->array->pdata[i];

			if (unique) {
				char *addr = _g_mime_mailbox_address_get_hashable_addrspec (GMIME_MAILBOX_ADDRESS (mailbox));

				if (!g_hash_table_add (unique, addr)) {
					continue;
				}
			}

			g_mime_internet_address_list_add (result, mailbox);
		}

		g_object_unref (mailboxes);
	}

	if (unique)
		g_hash_table_unref (unique);

	return result;
}

/**
 * g_mime_message_set_importance:
 * @message: a #GMimeMessage
 * @importance: the message importance
 *
 * Sets the Importance header to low, normal, or high. Assigning the
 * current importance leaves the raw headers unchanged. Invalid enum
 * values are programming errors and leave the message unchanged.
 **/
void
g_mime_message_set_importance (GMimeMessage *message, GMimeMessageImportance importance)
{
	static const char *values[] = { "low", "normal", "high" };

	g_return_if_fail (GMIME_IS_MESSAGE (message));
	g_return_if_fail (importance >= GMIME_MESSAGE_IMPORTANCE_LOW && importance <= GMIME_MESSAGE_IMPORTANCE_HIGH);

	if (message->importance != importance)
		g_mime_entity_set_header ((GMimeEntity *) message, "Importance", values[importance], NULL);
}

/**
 * g_mime_message_get_importance:
 * @message: a #GMimeMessage
 *
 * Returns: the first Importance header's value, or
 * %GMIME_MESSAGE_IMPORTANCE_NORMAL if absent or unrecognized.
 **/
GMimeMessageImportance
g_mime_message_get_importance (GMimeMessage *message)
{
	g_return_val_if_fail (GMIME_IS_MESSAGE (message), GMIME_MESSAGE_IMPORTANCE_NORMAL);

	return message->importance;
}

/**
 * g_mime_message_set_priority:
 * @message: a #GMimeMessage
 * @priority: the message priority
 *
 * Sets the Priority header to non-urgent, normal, or urgent. Assigning
 * the current priority preserves raw headers. Invalid enum values are
 * programming errors and leave the message unchanged.
 **/
void
g_mime_message_set_priority (GMimeMessage *message, GMimeMessagePriority priority)
{
	static const char *values[] = { "non-urgent", "normal", "urgent" };

	g_return_if_fail (GMIME_IS_MESSAGE (message));
	g_return_if_fail (priority >= GMIME_MESSAGE_PRIORITY_NON_URGENT && priority <= GMIME_MESSAGE_PRIORITY_URGENT);

	if (message->priority != priority)
		g_mime_entity_set_header ((GMimeEntity *) message, "Priority", values[priority], NULL);
}

/**
 * g_mime_message_get_priority:
 * @message: a #GMimeMessage
 *
 * Returns: the first Priority header's value, or
 * %GMIME_MESSAGE_PRIORITY_NORMAL if absent or unrecognized.
 **/
GMimeMessagePriority
g_mime_message_get_priority (GMimeMessage *message)
{
	g_return_val_if_fail (GMIME_IS_MESSAGE (message), GMIME_MESSAGE_PRIORITY_NORMAL);

	return message->priority;
}

/**
 * g_mime_message_set_x_priority:
 * @message: a #GMimeMessage
 * @priority: the message's numeric priority
 *
 * Sets the X-Priority header to its numeric value and descriptive label.
 * Assigning the current priority preserves raw headers. Invalid enum
 * values are programming errors and leave the message unchanged.
 **/
void
g_mime_message_set_x_priority (GMimeMessage *message, GMimeXMessagePriority priority)
{
	static const char *values[] = { "1 (Highest)", "2 (High)", "3 (Normal)", "4 (Low)", "5 (Lowest)" };

	g_return_if_fail (GMIME_IS_MESSAGE (message));
	g_return_if_fail (priority >= GMIME_X_MESSAGE_PRIORITY_HIGHEST && priority <= GMIME_X_MESSAGE_PRIORITY_LOWEST);

	if (message->x_priority != priority)
		g_mime_entity_set_header ((GMimeEntity *) message, "X-Priority", values[priority - 1], NULL);
}

/**
 * g_mime_message_get_x_priority:
 * @message: a #GMimeMessage
 *
 * Parses an unsigned decimal prefix from the first raw X-Priority header
 * after ASCII folding whitespace. Values are clamped to 1 through 5;
 * missing digits and Int32 overflow yield normal priority.
 *
 * Returns: the numeric priority, or %GMIME_X_MESSAGE_PRIORITY_NORMAL
 * if the header is absent or invalid.
 **/
GMimeXMessagePriority
g_mime_message_get_x_priority (GMimeMessage *message)
{
	g_return_val_if_fail (GMIME_IS_MESSAGE (message), GMIME_X_MESSAGE_PRIORITY_NORMAL);

	return message->x_priority;
}

/**
 * g_mime_message_set_subject:
 * @message: A #GMimeMessage
 * @subject: Subject string
 * @charset: (nullable): The charset to use for encoding the subject or %NULL to use the default
 *
 * Set the subject of a @message.
 *
 * Note: The @subject string should be in UTF-8.
 **/
void
g_mime_message_set_subject (GMimeMessage *message, const char *subject, const char *charset)
{
	g_return_if_fail (GMIME_IS_MESSAGE (message));
	g_return_if_fail (subject != NULL);
	
	g_mime_entity_set_header ((GMimeEntity *) message, "Subject", subject, charset);
}


/**
 * g_mime_message_get_subject:
 * @message: A #GMimeMessage
 *
 * Gets the subject of the @message.
 *
 * Returns: (nullable): the subject of the @message in a form suitable for
 * display or %NULL if no subject is set. If not %NULL, the returned string
 * will be in UTF-8.
 **/
const char *
g_mime_message_get_subject (GMimeMessage *message)
{
	g_return_val_if_fail (GMIME_IS_MESSAGE (message), NULL);
	
	return message->subject;
}


static void
message_set_date (GMimeMessage *message, const char *name, GDateTime **cache, GDateTime *date)
{
	char *str;

	g_return_if_fail (date != NULL);

	if (*cache && g_date_time_equal (*cache, date))
		return;

	str = g_mime_utils_header_format_date (date);
	if (!str)
		return;

	g_date_time_ref (date);
	g_mime_entity_set_header ((GMimeEntity *) message, name, str, NULL);
	g_clear_pointer (cache, g_date_time_unref);
	*cache = date;
	g_free (str);
}

/**
 * g_mime_message_set_date:
 * @message: A #GMimeMessage
 * @date: a date to be used in the Date header
 * 
 * Sets the Date header on a MIME Message. The message retains @date,
 * including its timezone and microseconds; the header represents whole
 * seconds. Assigning the same instant preserves the current cached value,
 * raw header, and any duplicates. A %NULL date is a programming error.
 **/
void
g_mime_message_set_date (GMimeMessage *message, GDateTime *date)
{
	g_return_if_fail (GMIME_IS_MESSAGE (message));

	message_set_date (message, "Date", &message->date, date);
}


/**
 * g_mime_message_get_date:
 * @message: A #GMimeMessage
 * 
 * Gets the cached date and time value. Raw header edits parse the first
 * Date header without decoding encoded words or falling back to duplicates.
 *
 * Returns: (nullable) (transfer none): a #GDateTime on success or %NULL if
 * the date could not be parsed.
 **/
GDateTime *
g_mime_message_get_date (GMimeMessage *message)
{
	g_return_val_if_fail (GMIME_IS_MESSAGE (message), NULL);
	
	return message->date;
}

/**
 * g_mime_message_set_resent_date:
 * @message: a #GMimeMessage
 * @date: a date to be used in the Resent-Date header
 *
 * Sets the Resent-Date header independently of Date. The message retains
 * @date, including its timezone and microseconds; the header represents
 * whole seconds. Assigning the same instant preserves the current cached
 * value, raw header, and any duplicates. Changed values remove duplicate
 * Resent-Date headers. A %NULL date is a programming error.
 **/
void
g_mime_message_set_resent_date (GMimeMessage *message, GDateTime *date)
{
	g_return_if_fail (GMIME_IS_MESSAGE (message));

	message_set_date (message, "Resent-Date", &message->resent_date, date);
}

/**
 * g_mime_message_get_resent_date:
 * @message: a #GMimeMessage
 *
 * Gets the cached resent date and time. Raw header edits parse the first
 * Resent-Date header without decoding encoded words or falling back to
 * duplicates.
 *
 * Returns: (nullable) (transfer none): the resent date and time, or %NULL
 * if the header is absent or invalid.
 **/
GDateTime *
g_mime_message_get_resent_date (GMimeMessage *message)
{
	g_return_val_if_fail (GMIME_IS_MESSAGE (message), NULL);

	return message->resent_date;
}


/**
 * g_mime_message_set_message_id: 
 * @message: A #GMimeMessage
 * @message_id: a UTF-8 message-id, with or without angle brackets
 *
 * Sets the Message-Id after normalizing a complete message-id. Invalid
 * values, including empty IDs, extra tokens, and non-folding line breaks,
 * are programming errors and leave the message unchanged. Assigning the
 * current ID preserves the original raw header.
 **/
void
g_mime_message_set_message_id (GMimeMessage *message, const char *message_id)
{
	char *id, *msgid;
	
	g_return_if_fail (GMIME_IS_MESSAGE (message));
	g_return_if_fail (message_id != NULL);

	if (!g_strcmp0 (message->message_id, message_id))
		return;

	id = _g_mime_parse_message_id_value (message_id);
	g_return_if_fail (id != NULL);
	
	msgid = g_strdup_printf ("<%s>", id);
	g_free (id);
	g_mime_entity_set_header ((GMimeEntity *) message, "Message-Id", msgid, NULL);
	g_free (msgid);
}


/**
 * g_mime_message_get_message_id: 
 * @message: A #GMimeMessage
 *
 * Gets the Message-Id header of @message.
 *
 * Returns: (nullable): the Message-Id of a message, or %NULL if not specified.
 **/
const char *
g_mime_message_get_message_id (GMimeMessage *message)
{
	g_return_val_if_fail (GMIME_IS_MESSAGE (message), NULL);
	
	return message->message_id;
}

/**
 * g_mime_message_set_in_reply_to:
 * @message: a #GMimeMessage
 * @message_id: (nullable): a UTF-8 message-id, with or without angle brackets
 *
 * Sets the In-Reply-To header after normalizing a complete message-id.
 * Invalid values are programming errors and leave the message unchanged.
 * Assigning the current ID preserves the raw header. A %NULL value removes
 * all In-Reply-To headers, including malformed duplicates.
 **/
void
g_mime_message_set_in_reply_to (GMimeMessage *message, const char *message_id)
{
	char *id, *value;

	g_return_if_fail (GMIME_IS_MESSAGE (message));

	if (!message_id) {
		g_mime_header_list_remove_all (((GMimeEntity *) message)->headers, "In-Reply-To");
		return;
	}
	if (!g_strcmp0 (message->in_reply_to, message_id))
		return;

	id = _g_mime_parse_message_id_value (message_id);
	g_return_if_fail (id != NULL);
	value = g_strdup_printf ("<%s>", id);
	g_free (id);
	g_mime_entity_set_header ((GMimeEntity *) message, "In-Reply-To", value, NULL);
	g_free (value);
}

/**
 * g_mime_message_get_in_reply_to:
 * @message: a #GMimeMessage
 *
 * Gets the first recovered message-id from the first In-Reply-To header,
 * skipping malformed IDs and unrelated text without rewriting the header.
 *
 * Returns: (nullable): the message-id being replied to, or %NULL if absent
 * or no valid ID could be recovered.
 **/
const char *
g_mime_message_get_in_reply_to (GMimeMessage *message)
{
	g_return_val_if_fail (GMIME_IS_MESSAGE (message), NULL);

	return message->in_reply_to;
}

/**
 * g_mime_message_set_resent_message_id:
 * @message: a #GMimeMessage
 * @message_id: a UTF-8 message-id, with or without angle brackets
 *
 * Sets the Resent-Message-Id header after normalizing a complete message-id.
 * Invalid values, including %NULL, are programming errors and leave the
 * message unchanged. Assigning the current ID preserves the raw header.
 **/
void
g_mime_message_set_resent_message_id (GMimeMessage *message, const char *message_id)
{
	char *id, *value;

	g_return_if_fail (GMIME_IS_MESSAGE (message));
	g_return_if_fail (message_id != NULL);

	if (!g_strcmp0 (message->resent_message_id, message_id))
		return;

	id = _g_mime_parse_message_id_value (message_id);
	g_return_if_fail (id != NULL);
	value = g_strdup_printf ("<%s>", id);
	g_free (id);
	g_mime_entity_set_header ((GMimeEntity *) message, "Resent-Message-Id", value, NULL);
	g_free (value);
}

/**
 * g_mime_message_get_resent_message_id:
 * @message: a #GMimeMessage
 *
 * Gets the parsed ID from the first Resent-Message-Id header.
 *
 * Returns: (nullable): the resent message-id, or %NULL if absent or malformed.
 **/
const char *
g_mime_message_get_resent_message_id (GMimeMessage *message)
{
	g_return_val_if_fail (GMIME_IS_MESSAGE (message), NULL);

	return message->resent_message_id;
}

/**
 * g_mime_message_get_references:
 * @message: a #GMimeMessage
 *
 * Gets the persistent, mutable References list. Editing it updates the
 * References header, removing all matching headers when the list becomes
 * empty. Raw-header edits refresh this same list from the first matching
 * header without rewriting it. The caller may retain its own reference;
 * the list stops updating the message after the message is finalized.
 *
 * Returns: (transfer none): the message's References list.
 **/
GMimeMessageIdList *
g_mime_message_get_references (GMimeMessage *message)
{
	g_return_val_if_fail (GMIME_IS_MESSAGE (message), NULL);

	return message->references;
}


/**
 * g_mime_message_get_mime_part:
 * @message: A #GMimeMessage
 *
 * Gets the toplevel MIME part contained within @message.
 *
 * Returns: (nullable) (transfer none): the toplevel MIME part of @message, or
 * %NULL if none is present.
 **/
GMimeEntity *
g_mime_message_get_mime_part (GMimeMessage *message)
{
	g_return_val_if_fail (GMIME_IS_MESSAGE (message), NULL);
	
	if (message->mime_part == NULL)
		return NULL;
	
	return message->mime_part;
}


/**
 * g_mime_message_set_mime_part:
 * @message: A #GMimeMessage
 * @mime_part: The root-level MIME Part
 *
 * Set the root-level MIME part of the message.
 **/
void
g_mime_message_set_mime_part (GMimeMessage *message, GMimeEntity *mime_part)
{
	g_return_if_fail (GMIME_IS_ENTITY (mime_part));
	g_return_if_fail (GMIME_IS_MESSAGE (message));
	
	if (message->mime_part == mime_part)
		return;
	
	if (message->mime_part)
		g_object_unref (message->mime_part);
	
	if (mime_part) {
		GMimeHeaderList *headers = ((GMimeEntity *) message)->headers;
		GMimeHeader *header;
		int i;
		
		if (!g_mime_header_list_contains (headers, "MIME-Version"))
			g_mime_header_list_append (headers, "MIME-Version", "1.0", NULL);
		
		for (i = 0; i < g_mime_header_list_get_count (mime_part->headers); i++) {
			header = g_mime_header_list_get_header_at (mime_part->headers, i);
			_g_mime_header_set_offset (header, -1);
		}
		
		g_object_ref (mime_part);
	}
	
	message->mime_part = mime_part;
}


/**
 * g_mime_message_foreach:
 * @message: A #GMimeMessage
 * @callback: (scope call): function to call on each of the mime parts
 *   contained by the mime message
 * @user_data: user-supplied callback data
 *
 * Recursively calls @callback on each of the mime parts in the mime message.
 **/
void
g_mime_message_foreach (GMimeMessage *message, GMimeEntityForeachFunc callback, gpointer user_data)
{
	g_return_if_fail (GMIME_IS_MESSAGE (message));
	g_return_if_fail (callback != NULL);
	
	callback ((GMimeEntity *) message, message->mime_part, user_data);
	
	/* Attached messages are single body parts; do not descend into message/rfc822 content. */
	if (GMIME_IS_MULTIPART (message->mime_part))
		g_mime_multipart_foreach ((GMimeMultipart *) message->mime_part, callback, user_data);
}

static void
collect_body_part (GMimeEntity *parent, GMimeEntity *part, gpointer user_data)
{
	if (part && !GMIME_IS_MULTIPART (part))
		g_ptr_array_add (user_data, g_object_ref (part));
}

static void
collect_attachment (GMimeEntity *parent, GMimeEntity *part, gpointer user_data)
{
	if (part && g_mime_entity_is_attachment (part))
		collect_body_part (parent, part, user_data);
}

/**
 * g_mime_message_get_body_parts:
 * @message: a #GMimeMessage
 *
 * Gets all non-multipart body entities in depth-first order, including
 * inline and attachment parts. Message parts are included as leaves;
 * their embedded message bodies are not traversed.
 *
 * Unlike MimeKit's lazy BodyParts enumeration, this returns a membership
 * snapshot. The entities are retained original instances, not clones.
 * Changes to their properties remain visible through the message, but
 * changing the array does not change the MIME tree. Releasing the message
 * or removing parts from its tree does not invalidate the snapshot.
 *
 * Returns: (transfer container) (element-type GMimeEntity): a newly allocated
 * array owning its entity references, empty when there are no body parts.
 * Elements are borrowed from the array; retain them separately to outlive
 * it. Free the array with g_ptr_array_unref().
 **/
GPtrArray *
g_mime_message_get_body_parts (GMimeMessage *message)
{
	GPtrArray *parts;

	g_return_val_if_fail (GMIME_IS_MESSAGE (message), NULL);

	parts = g_ptr_array_new_with_free_func (g_object_unref);
	g_mime_message_foreach (message, collect_body_part, parts);

	return parts;
}

/**
 * g_mime_message_get_attachments:
 * @message: a #GMimeMessage
 *
 * Gets the entities from g_mime_message_get_body_parts() whose
 * Content-Disposition is "attachment". Multipart containers themselves
 * are excluded even when marked as attachments, but their children are
 * visited. Inline entities with filenames are not attachments. Attached
 * messages are included without traversing their embedded bodies.
 *
 * The result has the same depth-first ordering, snapshot membership, and
 * shared entity ownership as g_mime_message_get_body_parts(). Attachment
 * status is evaluated at the time of this call, not tracked by the array.
 *
 * Returns: (transfer container) (element-type GMimeEntity): a newly allocated
 * array owning its entity references, empty when there are no attachments.
 * Elements are borrowed from the array; retain them separately to outlive
 * it. Free the array with g_ptr_array_unref().
 **/
GPtrArray *
g_mime_message_get_attachments (GMimeMessage *message)
{
	GPtrArray *parts;

	g_return_val_if_fail (GMIME_IS_MESSAGE (message), NULL);

	parts = g_ptr_array_new_with_free_func (g_object_unref);
	g_mime_message_foreach (message, collect_attachment, parts);

	return parts;
}

/**
 * g_mime_message_try_get_entity:
 * @message: a #GMimeMessage
 * @path_specifier: a dot-separated MIME path
 * @entity: (out) (transfer none) (nullable): the entity found, or %NULL
 *
 * Looks up an entity directly, including inside attached messages. Multipart
 * children are numbered from 1. A non-multipart message body is part 1;
 * a multipart message body is part 0, and its children omit that 0.
 * For example, "3.0" selects the multipart body of an attached message in
 * part 3, while "3.1" selects its first child.
 *
 * Components contain only ASCII decimal digits and must fit in a signed
 * 32-bit integer. Leading zeros are accepted. A 0 component can only select
 * a multipart message body and must be the final component. Empty,
 * malformed, overflowing, or nonexistent paths return %FALSE and clear
 * @entity. The returned entity is borrowed from the message tree.
 *
 * Lookup is iterative and follows only the specified path, not siblings.
 * The existing #GMimePartIter path conventions are unchanged.
 *
 * Returns: %TRUE if the entity exists, or %FALSE otherwise.
 **/
gboolean
g_mime_message_try_get_entity (GMimeMessage *message, const char *path_specifier, GMimeEntity **entity)
{
	GMimeEntity *current;
	gboolean message_body = TRUE;
	const char *inptr = path_specifier;
	int index, digit;

	g_return_val_if_fail (GMIME_IS_MESSAGE (message), FALSE);
	g_return_val_if_fail (path_specifier != NULL, FALSE);
	g_return_val_if_fail (entity != NULL, FALSE);

	*entity = NULL;
	if (!(current = message->mime_part))
		return FALSE;

	while (TRUE) {
		index = 0;
		if (!g_ascii_isdigit (*inptr))
			return FALSE;
		do {
			digit = *inptr++ - '0';
			if (index > (G_MAXINT32 - digit) / 10)
				return FALSE;
			index = index * 10 + digit;
		} while (g_ascii_isdigit (*inptr));
		if (*inptr && *inptr != '.')
			return FALSE;

		if (!message_body && GMIME_IS_MESSAGE_PART (current)) {
			GMimeMessage *embedded = g_mime_message_part_get_message (GMIME_MESSAGE_PART (current));

			if (!embedded || !(current = embedded->mime_part))
				return FALSE;
			message_body = TRUE;
		}

		if (GMIME_IS_MULTIPART (current)) {
			if (index == 0) {
				if (!message_body || *inptr)
					return FALSE;
			} else {
				if (index > g_mime_multipart_get_count (GMIME_MULTIPART (current)))
					return FALSE;
				current = g_mime_multipart_get_part (GMIME_MULTIPART (current), index - 1);
				message_body = FALSE;
			}
		} else {
			if (!message_body || index != 1)
				return FALSE;
			message_body = FALSE;
		}

		if (!*inptr) {
			*entity = current;
			return TRUE;
		}
		inptr++;
	}
}

static gboolean
part_is_textual (GMimeEntity *mime_part)
{
	GMimeContentType *type;
	
	type = g_mime_entity_get_content_type (mime_part);
	
	return g_mime_content_type_is_type (type, "text", "*");
}

static GMimeEntity *
multipart_guess_body (GMimeMultipart *multipart)
{
	GMimeContentType *type;
	GMimeEntity *mime_part;
	int count, i;
	
	if (GMIME_IS_MULTIPART_ENCRYPTED (multipart)) {
		/* nothing more we can do */
		return (GMimeEntity *) multipart;
	}
	
	type = g_mime_entity_get_content_type ((GMimeEntity *) multipart);
	if (g_mime_content_type_is_type (type, "multipart", "alternative")) {
		/* very likely that this is the body - leave it up to
		 * our caller to decide which format of the body it
		 * wants to use. */
		return (GMimeEntity *) multipart;
	}
	
	count = g_mime_multipart_get_count (multipart);
	
	if (count >= 1 && GMIME_IS_MULTIPART_SIGNED (multipart)) {
		/* if the body is in here, it has to be the first part */
		count = 1;
	}
	
	for (i = 0; i < count; i++) {
		mime_part = g_mime_multipart_get_part (multipart, i);
		
		if (GMIME_IS_MULTIPART (mime_part)) {
			if ((mime_part = multipart_guess_body ((GMimeMultipart *) mime_part)))
				return mime_part;
		} else if (GMIME_IS_PART (mime_part)) {
			if (part_is_textual (mime_part))
				return mime_part;
		}
	}
	
	return NULL;
}


/**
 * g_mime_message_get_body:
 * @message: A #GMimeMessage
 *
 * Attempts to identify the MIME part containing the body of the
 * message.
 *
 * Returns: (nullable) (transfer none): a #GMimeEntity containing the textual
 * content that appears to be the main body of the message, or %NULL if no
 * body part has been set.
 *
 * Note: This function is NOT guaranteed to always work as it
 * makes some assumptions that are not necessarily true. It is
 * recommended that you traverse the MIME structure yourself.
 **/
GMimeEntity *
g_mime_message_get_body (GMimeMessage *message)
{
	GMimeEntity *mime_part;
	
	g_return_val_if_fail (GMIME_IS_MESSAGE (message), NULL);
	
	if (!(mime_part = message->mime_part))
		return NULL;
	
	if (GMIME_IS_MULTIPART (mime_part))
		return multipart_guess_body ((GMimeMultipart *) mime_part);
	else if (GMIME_IS_PART (mime_part) && part_is_textual (mime_part))
		return mime_part;
	
	return NULL;
}


/**
 * g_mime_message_get_autocrypt_header:
 * @message: a #GMimeMessage object.
 * @now: (nullable): a #GDateTime object, or %NULL
 *
 * Creates a new #GMimeAutocryptHeader based on the relevant Autocrypt
 * header associated with the sender of an e-mail message.
 *
 * If the message has no sender in the From: field, or has more than
 * one sender, then this function will return %NULL.  Autocrypt should
 * ignore the message entirely.
 *
 * If there is one sender, but no single Autocrypt header is found
 * that matches that e-mail address, a #GMimeAutocryptHeader will be
 * returned for the sender, but it will be incomplete (see
 * #g_mime_autocrypt_header_is_complete).
 *
 * Note that the following types of Autocrypt headers will not be
 * returned by this function:
 *
 *  - headers that do not match an address in "From:"
 *  - unparseable headers
 *  - headers with unknown critical attributes
 *  - duplicate valid headers for the sender's address
 *
 * The returned Autocrypt header will have its effective_date set to
 * the earliest of either:
 *
 * - the Date: header of the message or 
 * - @now (or the current time, if @now is %NULL)
 *
 * Returns: (nullable) (transfer full): a new #GMimeAutocryptHeader object,
 * or %NULL if the message should be ignored for purposes of
 * Autocrypt.
 **/
GMimeAutocryptHeader *
g_mime_message_get_autocrypt_header (GMimeMessage *message, GDateTime *now)
{
	g_return_val_if_fail (GMIME_IS_MESSAGE (message), NULL);

	GMimeAutocryptHeaderList *retlist = NULL;
	GMimeAutocryptHeader *ret = NULL;
	GDateTime *newnow = NULL;
	GDateTime *effective_date = NULL;
	if (now == NULL)
		now = newnow = g_date_time_new_now_utc ();
	effective_date = now;
	if (message->date && g_date_time_compare (message->date, now) < 0)
		effective_date = message->date;
	retlist = g_mime_entity_get_autocrypt_headers (GMIME_ENTITY (message),
						       effective_date,
						       "autocrypt",
						       message->addrlists[GMIME_ADDRESS_TYPE_FROM],
						       TRUE);
	if (newnow)
		g_date_time_unref (newnow);
	if (retlist) {
		if (g_mime_autocrypt_header_list_get_count (retlist) == 1) {
			ret = g_mime_autocrypt_header_list_get_header_at (retlist, 0);
			g_object_ref (ret);
		}
		g_object_unref (retlist);
	}
	return ret;
}


/**
 * g_mime_message_get_autocrypt_gossip_headers_from_inner_part:
 * @message: a #GMimeMessage object.
 * @now: (nullable): a #GDateTime object, or %NULL
 * @inner_part: a #GMimeEntity which is the cleartext part of the inner message
 *
 * Creates a new #GMimeAutocryptHeaderList of relevant headers of the
 * given type based on the recipient(s) of an e-mail message.
 * 
 * You must pass the decrypted inner part of the message to this
 * function, since Autocrypt-Gossip headers are only stored within the
 * encrypted layer.
 *
 * If you don't already have the decrypted inner part available to
 * you, you probably want to use
 * #g_mime_message_get_autocrypt_gossip_headers instead.
 *
 * Each header in the returned list will:
 *
 *  - have a valid address
 *  - be of the type requested
 *  - be complete
 *
 * If no Autocrypt header is found for a recipient, no
 * #GMimeAutocryptHeader will be in the list associated with that e-mail address.
 *
 * Note that the following types of Autocrypt headers will not be
 * returned by this function:
 *
 *  - headers of an unrequested type
 *  - headers that do not match an address in "From:"
 *  - unparseable headers
 *  - headers with unknown critical attributes
 *  - duplicate valid headers for a given address
 * 
 * On error (e.g. if this version of GMime cannot handle the requested
 * Autocrypt type, or if a parameter is missing or malformed), returns
 * %NULL
 *
 * The returned Autocrypt headers will have their effective_date set
 * to the earliest of either:
 *
 * - the Date: header of the message or 
 * - @now (or the current time, if @now is %NULL)
 * 
 * Returns: (nullable) (transfer full): a new #GMimeAutocryptHeaderList object,
 * or %NULL on error.
 **/
GMimeAutocryptHeaderList *
g_mime_message_get_autocrypt_gossip_headers_from_inner_part (GMimeMessage *message, GDateTime *now, GMimeEntity *inner_part)
{
	g_return_val_if_fail (GMIME_IS_MESSAGE (message), NULL);
	g_return_val_if_fail (GMIME_IS_ENTITY (inner_part), NULL);
	GMimeInternetAddressList *addresses = g_mime_message_get_all_recipients (message);
	GDateTime *newnow = NULL;
	GDateTime *effective_date = NULL;
	GMimeAutocryptHeaderList *ret = NULL;
	if (now == NULL)
		now = newnow = g_date_time_new_now_utc ();
	effective_date = now;
	if (message->date && g_date_time_compare (message->date, now) < 0)
		effective_date = message->date;
	ret = g_mime_entity_get_autocrypt_headers (inner_part,
						   effective_date,
						   "autocrypt-gossip",
						   addresses, FALSE);
	g_object_unref (addresses);
	if (newnow)
		g_date_time_unref (newnow);
	return ret;
}


/**
 * g_mime_message_get_autocrypt_gossip_headers:
 * @message: a #GMimeMessage object, which is expected to be encrypted.
 * @now: (nullable): a #GDateTime object, or %NULL
 * @flags: a #GMimeDecryptFlags, to be used during decryption
 * @session_key: (nullable): session key to use or %NULL
 * @err: a #GError (can be %NULL)
 *
 * Creates a new #GMimeAutocryptHeaderList of relevant headers of the
 * given type based on the recipient(s) of an e-mail message.
 * 
 * Returns the same object as
 * #g_mime_message_get_autocrypt_gossip_headers_with_inner_part , but
 * handles decryption and cleanup automatically.
 *
 * @flags and @session_key are passed through to
 * #g_mime_multipart_encrypted_decrypt, as needed.
 *
 * If the message is not actually an encrypted message, returns %NULL:
 * it should be ignored for purposes of evaluating gossip.
 *
 * If decryption fails, returns %NULL.  In this case, an exception
 * will be set on @err to provide information about the decryption
 * failure.
 *
 * Returns: (nullable) (transfer full): a new #GMimeAutocryptHeaderList object,
 * or %NULL on error.
 **/
GMimeAutocryptHeaderList *g_mime_message_get_autocrypt_gossip_headers (GMimeMessage *message,
								       GDateTime *now,
								       GMimeDecryptFlags flags,
								       const char *session_key,
								       GError **err)
{
	g_return_val_if_fail (GMIME_IS_MESSAGE (message), NULL);
	GMimeAutocryptHeaderList *ret = NULL;
	GMimeEntity *top_level = NULL;
	GMimeEntity *inner_part = NULL;

	top_level = g_mime_message_get_mime_part (message);
	if (!GMIME_IS_MULTIPART_ENCRYPTED (top_level))
		return NULL;

	inner_part = g_mime_multipart_encrypted_decrypt (GMIME_MULTIPART_ENCRYPTED (top_level),
							 flags,
							 session_key,
							 NULL, /* we do not care about decryptresult */
							 err);
	if (inner_part) {
		ret = g_mime_message_get_autocrypt_gossip_headers_from_inner_part (message, now, inner_part);
		g_object_unref (inner_part);
	}

	return ret;
}
