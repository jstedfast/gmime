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

#ifdef ENABLE_CRYPTO
#include "gmime-gpgme.h"
#endif

#include "gmime-object-private.h"

/**
 * SECTION: gmime-message
 * @title: GMimeMessage
 * @short_description: Messages
 * @see_also:
 *
 * A #GMimeMessage represents an rfc822 message.
 **/

static void g_mime_message_class_init (GMimeMessageClass *klass);
static void g_mime_message_init (GMimeMessage *message);
static void g_mime_message_finalize (GObject *object);

/* GMimeEntity class methods */
static void message_header_added    (GMimeEntity *object, GMimeHeader *header);
static void message_header_changed  (GMimeEntity *object, GMimeHeader *header);
static void message_header_removed  (GMimeEntity *object, GMimeHeader *header);
static void message_headers_cleared (GMimeEntity *object);

static char *message_get_headers (GMimeEntity *object, GMimeFormatOptions *options);
static ssize_t message_write_to_stream (GMimeEntity *object, GMimeFormatOptions *options,
					gboolean content_only, GMimeStream *stream);
static gboolean message_prepare (GMimeEntity *object, GMimeEncodingConstraint constraint,
				 guint max_line_length, GError **error);

static void sender_changed (gpointer sender, gpointer args, gpointer user_data);
static void from_changed (gpointer sender, gpointer args, gpointer user_data);
static void reply_to_changed (gpointer sender, gpointer args, gpointer user_data);
static void to_list_changed (gpointer sender, gpointer args, gpointer user_data);
static void cc_list_changed (gpointer sender, gpointer args, gpointer user_data);
static void bcc_list_changed (gpointer sender, gpointer args, gpointer user_data);
static void resent_from_changed (gpointer sender, gpointer args, gpointer user_data);
static void resent_reply_to_changed (gpointer sender, gpointer args, gpointer user_data);
static void resent_to_changed (gpointer sender, gpointer args, gpointer user_data);
static void resent_cc_changed (gpointer sender, gpointer args, gpointer user_data);
static void resent_bcc_changed (gpointer sender, gpointer args, gpointer user_data);
static void resent_sender_changed (gpointer sender, gpointer args, gpointer user_data);
static void references_changed (gpointer sender, gpointer args, gpointer user_data);

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



G_DEFINE_TYPE_WITH_PRIVATE (GMimeMessage, g_mime_message, GMIME_TYPE_ENTITY)

GMimeMessagePrivate *
_g_mime_message_get_private (GMimeMessage *object)
{
	return g_mime_message_get_instance_private (object);
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
	object_class->accept = message_accept;
	object_class->prepare_with_max_line_length = message_prepare;
}

static void
connect_changed_event (GMimeMessage *message, GMimeAddressType type)
{
	GMimeMessagePrivate *priv = _g_mime_message_get_private (message);
	GMimeInternetAddressList *addrlist = priv->addrlists[type];
	
	g_mime_event_add (addrlist->changed, address_types[type].changed_cb, message);
}

static void
disconnect_changed_event (GMimeMessage *message, GMimeAddressType type)
{
	GMimeMessagePrivate *priv = _g_mime_message_get_private (message);
	GMimeInternetAddressList *addrlist = priv->addrlists[type];
	
	g_mime_event_remove (addrlist->changed, address_types[type].changed_cb, message);
}

static void
block_changed_event (GMimeMessage *message, GMimeAddressType type)
{
	GMimeMessagePrivate *priv = _g_mime_message_get_private (message);
	GMimeInternetAddressList *addrlist = priv->addrlists[type];
	
	g_mime_event_block (addrlist->changed, address_types[type].changed_cb, message);
}

static void
unblock_changed_event (GMimeMessage *message, GMimeAddressType type)
{
	GMimeMessagePrivate *priv = _g_mime_message_get_private (message);
	GMimeInternetAddressList *addrlist = priv->addrlists[type];
	
	g_mime_event_unblock (addrlist->changed, address_types[type].changed_cb, message);
}

static void
g_mime_message_init (GMimeMessage *message)
{
	GMimeMessagePrivate *priv = _g_mime_message_get_private (message);
	GMimeEntityPrivate *entity_priv;
	guint i;

	priv->addrlists = g_new (GMimeInternetAddressList *, N_ADDRESS_TYPES);
	entity_priv = _g_mime_entity_get_private (((GMimeEntity *) message));

	entity_priv->ensure_newline = TRUE;
	priv->message_id = NULL;
	priv->in_reply_to = NULL;
	priv->resent_message_id = NULL;
	priv->mime_part = NULL;
	priv->subject = NULL;
	priv->date = NULL;
	priv->resent_date = NULL;
	priv->importance = GMIME_MESSAGE_IMPORTANCE_NORMAL;
	priv->priority = GMIME_MESSAGE_PRIORITY_NORMAL;
	priv->x_priority = GMIME_X_MESSAGE_PRIORITY_NORMAL;
	priv->references = g_mime_message_id_list_new ();

	g_mime_event_add (priv->references->changed, (GMimeEventCallback) references_changed, message);
	
	/* initialize recipient lists */
	for (i = 0; i < N_ADDRESS_TYPES; i++) {
		priv->addrlists[i] = g_mime_internet_address_list_new ();
		connect_changed_event (message, i);
	}
}

static void
g_mime_message_finalize (GObject *object)
{
	GMimeMessagePrivate *priv = _g_mime_message_get_private ((GMimeMessage *) object);
	GMimeMessage *message = (GMimeMessage *) object;
	guint i;
	
	/* disconnect changed handlers */
	for (i = 0; i < N_ADDRESS_TYPES; i++) {
		disconnect_changed_event (message, i);
		g_object_unref (priv->addrlists[i]);
	}
	
	g_free (priv->addrlists);
	g_free (priv->message_id);
	g_free (priv->in_reply_to);
	g_free (priv->resent_message_id);
	g_free (priv->subject);
	g_free (priv->marker);
	g_clear_pointer (&priv->raw_marker, g_bytes_unref);
	g_mime_event_remove (priv->references->changed, (GMimeEventCallback) references_changed, message);
	g_object_unref (priv->references);
	
	if (priv->date)
		g_date_time_unref (priv->date);

	g_clear_pointer (&priv->resent_date, g_date_time_unref);
	
	/* unref child mime part */
	if (priv->mime_part)
		_g_mime_entity_unref (priv->mime_part);
	
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
	GMimeEntityPrivate *entity_priv = _g_mime_entity_get_private ((GMimeEntity *) message);
	GMimeMessagePrivate *priv = _g_mime_message_get_private (message);
	GMimeHeaderList *headers = entity_priv->headers;
	GMimeHeader *header = g_mime_header_list_get_header (headers, message_headers[type]);
	const char *value = NULL;
	char *raw = NULL;
	gint32 number;

	if (header) {
		value = type == HEADER_X_PRIORITY ? (raw = _g_mime_header_dup_raw_value (header)) :
			g_mime_header_get_value (header);
	}

	switch (type) {
	case HEADER_IMPORTANCE:
		if (_g_mime_strcase_equal_trimmed (value, "high"))
			priv->importance = GMIME_MESSAGE_IMPORTANCE_HIGH;
		else if (_g_mime_strcase_equal_trimmed (value, "low"))
			priv->importance = GMIME_MESSAGE_IMPORTANCE_LOW;
		else
			priv->importance = GMIME_MESSAGE_IMPORTANCE_NORMAL;
		break;
	case HEADER_PRIORITY:
		if (_g_mime_strcase_equal_trimmed (value, "urgent"))
			priv->priority = GMIME_MESSAGE_PRIORITY_URGENT;
		else if (_g_mime_strcase_equal_trimmed (value, "non-urgent"))
			priv->priority = GMIME_MESSAGE_PRIORITY_NON_URGENT;
		else
			priv->priority = GMIME_MESSAGE_PRIORITY_NORMAL;
		break;
	case HEADER_X_PRIORITY:
		priv->x_priority = GMIME_X_MESSAGE_PRIORITY_NORMAL;

		if (value) {
			skip_lwsp (&value);

			if (_g_mime_parse_int32 (&value, &number))
				priv->x_priority = (GMimeXMessagePriority) CLAMP (number, 1, 5);
		}
		break;
	}

	g_free (raw);
}

static void
message_update_date (GMimeMessage *message, const char *name, GDateTime **date)
{
	GMimeEntityPrivate *priv = _g_mime_entity_get_private ((GMimeEntity *) message);
	GMimeHeaderList *headers = priv->headers;
	GMimeHeader *header = g_mime_header_list_get_header (headers, name);
	char *value;

	g_clear_pointer (date, g_date_time_unref);

	if (header && (value = _g_mime_header_dup_raw_value (header))) {
		*date = g_mime_utils_header_decode_date (value);
		g_free (value);
	}
}

static void
message_update_references (GMimeMessage *message)
{
	GMimeEntityPrivate *entity_priv = _g_mime_entity_get_private ((GMimeEntity *) message);
	GMimeHeaderList *headers = entity_priv->headers;
	GMimeMessagePrivate *priv = _g_mime_message_get_private (message);
	GMimeMessageIdList *list = priv->references;
	GMimeHeader *header;
	const char *value;
	char *id, *raw;

	g_mime_event_block (list->changed, (GMimeEventCallback) references_changed, message);
	g_mime_message_id_list_clear (list);

	header = g_mime_header_list_get_header (headers, "References");

	if (header && (raw = _g_mime_header_dup_raw_value (header))) {
		value = raw;

		while ((id = _g_mime_parse_reference (&value)))
			g_ptr_array_add (list->array, id);

		g_free (raw);
	}

	g_mime_event_unblock (list->changed, (GMimeEventCallback) references_changed, message);
}

static void
message_update_subject (GMimeMessage *message)
{
	GMimeEntityPrivate *entity_priv = _g_mime_entity_get_private (GMIME_ENTITY (message));
	const char *value = g_mime_header_list_get_value (entity_priv->headers, "Subject");
	GMimeMessagePrivate *priv = _g_mime_message_get_private (message);

	g_free (priv->subject);
	priv->subject = g_strdup (value);
}

static void
message_update_reply_id (GMimeMessage *message, const char *name, char **id)
{
	GMimeEntityPrivate *priv = _g_mime_entity_get_private ((GMimeEntity *) message);
	GMimeHeaderList *headers = priv->headers;
	GMimeHeader *header;
	const char *value;
	char *raw;

	g_clear_pointer (id, g_free);
	header = g_mime_header_list_get_header (headers, name);

	if (header && (raw = _g_mime_header_dup_raw_value (header))) {
		value = raw;

		if (!g_ascii_strcasecmp (name, "In-Reply-To"))
			*id = _g_mime_parse_reference (&value);
		else
			*id = g_mime_utils_decode_message_id (value);

		g_free (raw);
	}
}

static void
message_add_addresses (GMimeMessage *message, GMimeParserOptions *options, GMimeHeader *header, GMimeAddressType type)
{
	GMimeMessagePrivate *priv = _g_mime_message_get_private (message);
	GMimeInternetAddressList *addrlist = priv->addrlists[type];
	char *value;
	
	block_changed_event (message, type);
	
	if ((value = _g_mime_header_dup_raw_value (header))) {
		_g_mime_internet_address_list_append_parse (addrlist, options, value, header->offset);
		g_free (value);
	}
	
	unblock_changed_event (message, type);
}

static void
message_update_addresses (GMimeMessage *message, GMimeParserOptions *options, GMimeAddressType type)
{
	GMimeEntityPrivate *entity_priv = _g_mime_entity_get_private ((GMimeEntity *) message);
	GMimeMessagePrivate *priv = _g_mime_message_get_private (message);
	GMimeInternetAddressList *addrlist = priv->addrlists[type];
	GMimeHeaderList *headers = entity_priv->headers;
	const char *name;
	char *value;
	GMimeHeader *header;
	int count, i;
	
	block_changed_event (message, type);

	g_mime_internet_address_list_clear (addrlist);
	
	count = g_mime_header_list_get_count (headers);

	for (i = 0; i < count; i++) {
		header = g_mime_header_list_get_header_at (headers, i);
		name = g_mime_header_get_name (header);
		
		if (g_ascii_strcasecmp (address_types[type].name, name) != 0)
			continue;
		
		if ((value = _g_mime_header_dup_raw_value (header))) {
			_g_mime_internet_address_list_append_parse (addrlist, options, value, header->offset);
			g_free (value);
		}
	}
	
	unblock_changed_event (message, type);
}

static gboolean
header_was_appended (GMimeEntity *entity, GMimeHeaderListChangedAction action, GMimeHeader *header)
{
	// Note: This is a hack until we can have a message_header_inserted() API.
	if (action == GMIME_HEADER_LIST_CHANGED_ACTION_ADDED) {
		GMimeEntityPrivate *priv = _g_mime_entity_get_private (entity);
		int count = g_mime_header_list_get_count (priv->headers);
		GMimeHeader *last = g_mime_header_list_get_header_at (priv->headers, count - 1);

		/* An earlier insertion can share the last header's identity. */
		return header == last && g_mime_header_list_index_of_header (priv->headers, header) == count - 1;
	}

	return FALSE;
}

static void
process_header (GMimeEntity *entity, GMimeHeaderListChangedAction action, GMimeHeader *header)
{
	GMimeMessage *message = (GMimeMessage *) entity;
	GMimeMessagePrivate *priv = _g_mime_message_get_private (message);
	GMimeEntityPrivate *entity_priv = _g_mime_entity_get_private (entity);
	GMimeParserOptions *options = _g_mime_header_list_get_options (entity_priv->headers);
	const char *name;
	guint i;
	
	name = g_mime_header_get_name (header);
	
	for (i = 0; i < G_N_ELEMENTS (message_headers); i++) {
		if (!g_ascii_strcasecmp (message_headers[i], name))
			break;
	}

	switch (i) {
	case HEADER_SENDER:
		if (header_was_appended (entity, action, header))
			message_add_addresses (message, options, header, GMIME_ADDRESS_TYPE_SENDER);
		else
			message_update_addresses (message, options, GMIME_ADDRESS_TYPE_SENDER);
		break;
	case HEADER_FROM:
		if (header_was_appended (entity, action, header))
			message_add_addresses (message, options, header, GMIME_ADDRESS_TYPE_FROM);
		else
			message_update_addresses (message, options, GMIME_ADDRESS_TYPE_FROM);
		break;
	case HEADER_REPLY_TO:
		if (header_was_appended (entity, action, header))
			message_add_addresses (message, options, header, GMIME_ADDRESS_TYPE_REPLY_TO);
		else
			message_update_addresses (message, options, GMIME_ADDRESS_TYPE_REPLY_TO);
		break;
	case HEADER_TO:
		if (header_was_appended (entity, action, header))
			message_add_addresses (message, options, header, GMIME_ADDRESS_TYPE_TO);
		else
			message_update_addresses (message, options, GMIME_ADDRESS_TYPE_TO);
		break;
	case HEADER_CC:
		if (header_was_appended (entity, action, header))
			message_add_addresses (message, options, header, GMIME_ADDRESS_TYPE_CC);
		else
			message_update_addresses (message, options, GMIME_ADDRESS_TYPE_CC);
		break;
	case HEADER_BCC:
		if (header_was_appended (entity, action, header))
			message_add_addresses (message, options, header, GMIME_ADDRESS_TYPE_BCC);
		else
			message_update_addresses (message, options, GMIME_ADDRESS_TYPE_BCC);
		break;
	case HEADER_RESENT_FROM:
		if (header_was_appended (entity, action, header))
			message_add_addresses (message, options, header, GMIME_ADDRESS_TYPE_RESENT_FROM);
		else
			message_update_addresses (message, options, GMIME_ADDRESS_TYPE_RESENT_FROM);
		break;
	case HEADER_RESENT_REPLY_TO:
		if (header_was_appended (entity, action, header))
			message_add_addresses (message, options, header, GMIME_ADDRESS_TYPE_RESENT_REPLY_TO);
		else
			message_update_addresses (message, options, GMIME_ADDRESS_TYPE_RESENT_REPLY_TO);
		break;
	case HEADER_RESENT_TO:
		if (header_was_appended (entity, action, header))
			message_add_addresses (message, options, header, GMIME_ADDRESS_TYPE_RESENT_TO);
		else
			message_update_addresses (message, options, GMIME_ADDRESS_TYPE_RESENT_TO);
		break;
	case HEADER_RESENT_CC:
		if (header_was_appended (entity, action, header))
			message_add_addresses (message, options, header, GMIME_ADDRESS_TYPE_RESENT_CC);
		else
			message_update_addresses (message, options, GMIME_ADDRESS_TYPE_RESENT_CC);
		break;
	case HEADER_RESENT_BCC:
		if (header_was_appended (entity, action, header))
			message_add_addresses (message, options, header, GMIME_ADDRESS_TYPE_RESENT_BCC);
		else
			message_update_addresses (message, options, GMIME_ADDRESS_TYPE_RESENT_BCC);
		break;
	case HEADER_RESENT_SENDER:
		if (header_was_appended (entity, action, header))
			message_add_addresses (message, options, header, GMIME_ADDRESS_TYPE_RESENT_SENDER);
		else
			message_update_addresses (message, options, GMIME_ADDRESS_TYPE_RESENT_SENDER);
		break;
	case HEADER_SUBJECT:
		message_update_subject (message);
		break;
	case HEADER_DATE:
		message_update_date (message, name, &priv->date);
		break;
	case HEADER_RESENT_DATE:
		message_update_date (message, name, &priv->resent_date);
		break;
	case HEADER_MESSAGE_ID:
		message_update_reply_id (message, name, &priv->message_id);
		break;
	case HEADER_IN_REPLY_TO:
		message_update_reply_id (message, name, &priv->in_reply_to);
		break;
	case HEADER_RESENT_MESSAGE_ID:
		message_update_reply_id (message, name, &priv->resent_message_id);
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
message_header_added (GMimeEntity *entity, GMimeHeader *header)
{
	process_header (entity, GMIME_HEADER_LIST_CHANGED_ACTION_ADDED, header);
	
	GMIME_ENTITY_CLASS (parent_class)->header_added (entity, header);
}

static void
message_header_changed (GMimeEntity *entity, GMimeHeader *header)
{
	process_header (entity, GMIME_HEADER_LIST_CHANGED_ACTION_CHANGED, header);
	
	GMIME_ENTITY_CLASS (parent_class)->header_changed (entity, header);
}

static void
message_header_removed (GMimeEntity *entity, GMimeHeader *header)
{
	GMimeMessage *message = (GMimeMessage *) entity;
	GMimeMessagePrivate *priv = _g_mime_message_get_private (message);
	GMimeEntityPrivate *entity_priv = _g_mime_entity_get_private (entity);
	GMimeParserOptions *options = _g_mime_header_list_get_options (entity_priv->headers);
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
		message_update_subject (message);
		break;
	case HEADER_DATE:
		message_update_date (message, name, &priv->date);
		break;
	case HEADER_RESENT_DATE:
		message_update_date (message, name, &priv->resent_date);
		break;
	case HEADER_MESSAGE_ID:
		message_update_reply_id (message, name, &priv->message_id);
		break;
	case HEADER_IN_REPLY_TO:
		message_update_reply_id (message, name, &priv->in_reply_to);
		break;
	case HEADER_RESENT_MESSAGE_ID:
		message_update_reply_id (message, name, &priv->resent_message_id);
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
	
	GMIME_ENTITY_CLASS (parent_class)->header_removed (entity, header);
}

static void
message_headers_cleared (GMimeEntity *entity)
{
	GMimeMessage *message = (GMimeMessage *) entity;
	GMimeMessagePrivate *priv = _g_mime_message_get_private (message);
	guint i;
	
	for (i = 0; i < N_ADDRESS_TYPES; i++) {
		block_changed_event (message, i);
		g_mime_internet_address_list_clear (priv->addrlists[i]);
		unblock_changed_event (message, i);
	}

	g_free (priv->message_id);
	priv->message_id = NULL;
	g_clear_pointer (&priv->in_reply_to, g_free);
	g_clear_pointer (&priv->resent_message_id, g_free);
	message_update_references (message);
	priv->importance = GMIME_MESSAGE_IMPORTANCE_NORMAL;
	priv->priority = GMIME_MESSAGE_PRIORITY_NORMAL;
	priv->x_priority = GMIME_X_MESSAGE_PRIORITY_NORMAL;
	g_free (priv->subject);
	priv->subject = NULL;

	if (priv->date) {
		g_date_time_unref (priv->date);
		priv->date = NULL;
	}

	g_clear_pointer (&priv->resent_date, g_date_time_unref);
	
	GMIME_ENTITY_CLASS (parent_class)->headers_cleared (entity);
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


GPtrArray *
_g_mime_message_merge_headers (GMimeMessage *message)
{
	GPtrArray *headers = g_ptr_array_new_with_free_func (g_object_unref);
	GMimeEntity *entity = (GMimeEntity *) message;
	GMimeEntityPrivate *entity_priv = _g_mime_entity_get_private (entity);
	GMimeMessagePrivate *priv = _g_mime_message_get_private (message);
	GMimeEntity *mime_part = priv->mime_part;
	GMimeEntityPrivate *mime_part_priv = NULL;
	GMimeHeader *header, *body_header;
	guint index = 0, body_index = 0;

	if (mime_part != NULL) {
		mime_part_priv = _g_mime_entity_get_private (mime_part);

		/* write all the prepended message headers first */
		while (index < entity_priv->headers->array->len) {
			header = entity_priv->headers->array->pdata[index];

			if (header->offset >= 0)
				break;

			g_ptr_array_add (headers, g_object_ref (header));
			index++;
		}

		/* now merge the message and body headers as they appeared in the raw message */
		while (index < entity_priv->headers->array->len && body_index < mime_part_priv->headers->array->len) {
			body_header = mime_part_priv->headers->array->pdata[body_index];

			if (body_header->offset < 0)
				break;

			header = entity_priv->headers->array->pdata[index];

			if (header->offset >= 0 && header->offset < body_header->offset) {
				g_ptr_array_add (headers, g_object_ref (header));
				index++;
			} else {
				g_ptr_array_add (headers, g_object_ref (body_header));
				body_index++;
			}
		}
	}

	while (index < entity_priv->headers->array->len)
		g_ptr_array_add (headers, g_object_ref (entity_priv->headers->array->pdata[index++]));

	if (mime_part_priv != NULL) {
		while (body_index < mime_part_priv->headers->array->len)
			g_ptr_array_add (headers, g_object_ref (mime_part_priv->headers->array->pdata[body_index++]));
	}

	return headers;
}


/* Write the message headers (merged with the headers of the body, if any). */
ssize_t
_g_mime_message_write_headers (GMimeEntity *entity, GMimeFormatOptions *options, GMimeStream *stream)
{
	GMimeMessage *message = (GMimeMessage *) entity;
	GMimeEntityPrivate *entity_priv = _g_mime_entity_get_private (entity);
	GMimeMessagePrivate *priv = _g_mime_message_get_private (message);
	GMimeStream *filtered;
	GMimeFilter *filter;
	GPtrArray *headers;
	ssize_t total = -1;
	int errsv;
	guint i;

	if (!priv->mime_part)
		return _g_mime_header_list_write_to_stream (entity_priv->headers, options, stream);

	headers = _g_mime_message_merge_headers (message);
	filtered = g_mime_stream_filter_new (stream);
	filter = g_mime_format_options_create_newline_filter (options, FALSE);
	g_mime_stream_filter_add ((GMimeStreamFilter *) filtered, filter);
	g_object_unref (filter);

	for (i = 0; i < headers->len; i++) {
		GMimeHeader *header = headers->pdata[i];

		if (!g_mime_format_options_is_hidden_header (options, header->name) &&
		    g_mime_header_write_to_stream (header, options, filtered) == -1)
			goto done;
	}

	if (g_mime_stream_flush (filtered) != -1)
		total = _g_mime_stream_filter_get_written ((GMimeStreamFilter *) filtered);

done:

	errsv = errno;
	g_object_unref (filtered);
	g_ptr_array_unref (headers);
	errno = errsv;

	return total;
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
	GMimeEntity *entity = (GMimeEntity *) message;
	GMimeEntityPrivate *entity_priv;
	GMimeMessagePrivate *priv;
	GMimeHeaderList *headers;
	ssize_t total, nwritten;
	const char *newline;
	size_t length;

	g_return_val_if_fail (GMIME_IS_MESSAGE (message), -1);
	g_return_val_if_fail (GMIME_IS_STREAM (stream), -1);

	if ((total = _g_mime_message_write_headers (entity, options, stream)) < 0)
		return -1;

	priv = _g_mime_message_get_private (message);
	entity_priv = _g_mime_entity_get_private (entity);

	if (priv->mime_part) {
		GMimeEntityPrivate *part_priv = _g_mime_entity_get_private (priv->mime_part);

		headers = part_priv->headers;
	} else {
		headers = entity_priv->headers;
	}

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

static gboolean
message_has_mime_version (GMimeMessage *message)
{
	GMimeEntityPrivate *priv = _g_mime_entity_get_private (GMIME_ENTITY (message));
	gboolean valid = FALSE;
	GMimeHeader *header;
	const char *inptr;
	guint count = 0;
	gint32 value;
	char *raw;

	if (!(header = g_mime_header_list_get_header (priv->headers, "MIME-Version")))
		return FALSE;

	/* MimeKit parses the first raw header as two to four Int32 components,
	 * allowing comments and whitespace without normalizing a valid value. */
	inptr = raw = _g_mime_header_dup_raw_value (header);

	if (!raw)
		return FALSE;

	do {
		if (!skip_cfws (&inptr) || !_g_mime_parse_int32 (&inptr, &value) || ++count > 4)
			goto done;

		if (!skip_cfws (&inptr))
			goto done;

		if (!*inptr)
			break;

		if (*inptr++ != '.')
			goto done;
	} while (*inptr);

	valid = count >= 2;

done:

	g_free (raw);
	return valid;
}


static gboolean
message_prepare (GMimeEntity *entity, GMimeEncodingConstraint constraint, guint max_line_length, GError **error)
{
	GMimeMessage *message = (GMimeMessage *) entity;
	GMimeMessagePrivate *priv = _g_mime_message_get_private (message);
	GMimeEntityPrivate *part_priv;
	GMimeEntityPrivate *object_priv;

	if (!priv->mime_part)
		return TRUE;

	part_priv = _g_mime_entity_get_private (priv->mime_part);

	/* Publish MIME-Version before preparing the body, even if preparation
	 * later fails. Existing valid versions and duplicates remain untouched. */
	object_priv = _g_mime_entity_get_private (entity);

	if (g_mime_header_list_get_count (part_priv->headers) > 0 && !message_has_mime_version (message))
		g_mime_header_list_set (object_priv->headers, "MIME-Version", "1.0", NULL);

	return g_mime_entity_prepare_with_max_line_length (priv->mime_part, constraint, max_line_length, error);
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
	GMimeMessage *message;
	guint i;
	
	message = g_object_new (GMIME_TYPE_MESSAGE, NULL);
	
	if (pretty_headers) {
		/* Populate with the "standard" rfc822 headers so we can have a standard order */
		GMimeEntityPrivate *priv = _g_mime_entity_get_private ((GMimeEntity *) message);
		GMimeHeaderList *headers = priv->headers;
		
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
	GMimeMessagePrivate *priv;

	g_return_val_if_fail (GMIME_IS_MESSAGE (message), NULL);

	priv = _g_mime_message_get_private (message);

	return priv->addrlists[GMIME_ADDRESS_TYPE_SENDER];
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
	GMimeMessagePrivate *priv;

	g_return_val_if_fail (GMIME_IS_MESSAGE (message), NULL);
	
	priv = _g_mime_message_get_private (message);

	return priv->addrlists[GMIME_ADDRESS_TYPE_FROM];
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
	GMimeMessagePrivate *priv;

	g_return_val_if_fail (GMIME_IS_MESSAGE (message), NULL);
	
	priv = _g_mime_message_get_private (message);

	return priv->addrlists[GMIME_ADDRESS_TYPE_REPLY_TO];
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
	GMimeMessagePrivate *priv;

	g_return_val_if_fail (GMIME_IS_MESSAGE (message), NULL);
	
	priv = _g_mime_message_get_private (message);

	return priv->addrlists[GMIME_ADDRESS_TYPE_TO];
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
	GMimeMessagePrivate *priv;

	g_return_val_if_fail (GMIME_IS_MESSAGE (message), NULL);
	
	priv = _g_mime_message_get_private (message);

	return priv->addrlists[GMIME_ADDRESS_TYPE_CC];
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
	GMimeMessagePrivate *priv;

	g_return_val_if_fail (GMIME_IS_MESSAGE (message), NULL);
	
	priv = _g_mime_message_get_private (message);

	return priv->addrlists[GMIME_ADDRESS_TYPE_BCC];
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
	GMimeMessagePrivate *priv;

	g_return_val_if_fail (GMIME_IS_MESSAGE (message), NULL);

	priv = _g_mime_message_get_private (message);

	return priv->addrlists[GMIME_ADDRESS_TYPE_RESENT_FROM];
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
	GMimeMessagePrivate *priv;

	g_return_val_if_fail (GMIME_IS_MESSAGE (message), NULL);

	priv = _g_mime_message_get_private (message);

	return priv->addrlists[GMIME_ADDRESS_TYPE_RESENT_REPLY_TO];
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
	GMimeMessagePrivate *priv;

	g_return_val_if_fail (GMIME_IS_MESSAGE (message), NULL);

	priv = _g_mime_message_get_private (message);

	return priv->addrlists[GMIME_ADDRESS_TYPE_RESENT_TO];
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
	GMimeMessagePrivate *priv;

	g_return_val_if_fail (GMIME_IS_MESSAGE (message), NULL);

	priv = _g_mime_message_get_private (message);

	return priv->addrlists[GMIME_ADDRESS_TYPE_RESENT_CC];
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
	GMimeMessagePrivate *priv;

	g_return_val_if_fail (GMIME_IS_MESSAGE (message), NULL);

	priv = _g_mime_message_get_private (message);

	return priv->addrlists[GMIME_ADDRESS_TYPE_RESENT_BCC];
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
	GMimeMessagePrivate *priv;

	g_return_val_if_fail (GMIME_IS_MESSAGE (message), NULL);

	priv = _g_mime_message_get_private (message);

	return priv->addrlists[GMIME_ADDRESS_TYPE_RESENT_SENDER];
}

static void
references_changed (gpointer sender, gpointer args, gpointer user_data)
{
	GMimeMessageIdList *list = sender;
	GMimeMessage *message = user_data;
	size_t maxline = g_mime_format_options_get_max_line_length (NULL);
	const char *newline = g_mime_format_options_get_newline (NULL);
	GMimeEntity *entity = (GMimeEntity *) message;
	GMimeEntityPrivate *priv = _g_mime_entity_get_private (entity);
	size_t line = strlen ("References:");
	const char *id;
	GString *str;
	size_t length;
	guint i;

	_g_mime_entity_block_header_list_changed (entity);

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
		_g_mime_header_list_set (priv->headers, "References", str->str);
		g_string_free (str, TRUE);
	} else {
		g_mime_header_list_remove_all (priv->headers, "References");
	}

	_g_mime_entity_unblock_header_list_changed (entity);
}

static void
sync_g_mime_internet_address_list (GMimeInternetAddressList *list, GMimeMessage *message, const char *name)
{
	GMimeEntity *entity = (GMimeEntity *) message;
	GMimeEntityPrivate *entity_priv = _g_mime_entity_get_private (entity);
	GString *str;
	guint n;
	
	if (g_mime_internet_address_list_length (list) == 0) {
		_g_mime_entity_block_header_list_changed (entity);
		g_mime_header_list_remove_all (entity_priv->headers, name);
		_g_mime_entity_unblock_header_list_changed (entity);
		return;
	}

	str = g_string_new (name);
	g_string_append_c (str, ':');
	n = str->len;
	
	g_string_append_c (str, ' ');
	g_mime_internet_address_list_encode (list, NULL, str);
	
	_g_mime_entity_block_header_list_changed (entity);
	_g_mime_header_list_set (entity_priv->headers, name, str->str + n);
	_g_mime_entity_unblock_header_list_changed (entity);
	
	g_string_free (str, TRUE);
}

static void
sync_address_header (GMimeMessage *message, GMimeAddressType type)
{
	GMimeMessagePrivate *priv = _g_mime_message_get_private (message);
	GMimeInternetAddressList *list = priv->addrlists[type];
	const char *name = address_types[type].name;
	
	sync_g_mime_internet_address_list (list, message, name);
}

static void
sender_changed (gpointer sender, gpointer args, gpointer user_data)
{
	sync_address_header (user_data, GMIME_ADDRESS_TYPE_SENDER);
}

static void
from_changed (gpointer sender, gpointer args, gpointer user_data)
{
	sync_address_header (user_data, GMIME_ADDRESS_TYPE_FROM);
}

static void
reply_to_changed (gpointer sender, gpointer args, gpointer user_data)
{
	sync_address_header (user_data, GMIME_ADDRESS_TYPE_REPLY_TO);
}

static void
to_list_changed (gpointer sender, gpointer args, gpointer user_data)
{
	sync_address_header (user_data, GMIME_ADDRESS_TYPE_TO);
}

static void
cc_list_changed (gpointer sender, gpointer args, gpointer user_data)
{
	sync_address_header (user_data, GMIME_ADDRESS_TYPE_CC);
}

static void
bcc_list_changed (gpointer sender, gpointer args, gpointer user_data)
{
	sync_address_header (user_data, GMIME_ADDRESS_TYPE_BCC);
}

static void
resent_from_changed (gpointer sender, gpointer args, gpointer user_data)
{
	sync_address_header (user_data, GMIME_ADDRESS_TYPE_RESENT_FROM);
}

static void
resent_reply_to_changed (gpointer sender, gpointer args, gpointer user_data)
{
	sync_address_header (user_data, GMIME_ADDRESS_TYPE_RESENT_REPLY_TO);
}

static void
resent_to_changed (gpointer sender, gpointer args, gpointer user_data)
{
	sync_address_header (user_data, GMIME_ADDRESS_TYPE_RESENT_TO);
}

static void
resent_cc_changed (gpointer sender, gpointer args, gpointer user_data)
{
	sync_address_header (user_data, GMIME_ADDRESS_TYPE_RESENT_CC);
}

static void
resent_bcc_changed (gpointer sender, gpointer args, gpointer user_data)
{
	sync_address_header (user_data, GMIME_ADDRESS_TYPE_RESENT_BCC);
}

static void
resent_sender_changed (gpointer sender, gpointer args, gpointer user_data)
{
	sync_address_header (user_data, GMIME_ADDRESS_TYPE_RESENT_SENDER);
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
	GMimeMessagePrivate *priv;
	GMimeInternetAddress *ia;
	
	g_return_val_if_fail (GMIME_IS_MESSAGE (message), FALSE);
	g_return_val_if_fail (type >= 0 && type < N_ADDRESS_TYPES, FALSE);
	g_return_val_if_fail (addr != NULL, FALSE);
	
	priv = _g_mime_message_get_private (message);
	addrlist = priv->addrlists[type];

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
	GMimeMessagePrivate *priv;

	g_return_val_if_fail (GMIME_IS_MESSAGE (message), NULL);
	g_return_val_if_fail (type < N_ADDRESS_TYPES, NULL);
	
	priv = _g_mime_message_get_private (message);

	return priv->addrlists[type];
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
	GMimeMessagePrivate *priv;
	guint i;
	
	g_return_val_if_fail (GMIME_IS_MESSAGE (message), NULL);

	priv = _g_mime_message_get_private (message);
	
	for (i = GMIME_ADDRESS_TYPE_TO; i <= GMIME_ADDRESS_TYPE_BCC; i++) {
		recipients = priv->addrlists[i];
		
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
	GMimeMessagePrivate *priv;
	GHashTable *unique = NULL;
	guint first, last, type, i;

	g_return_val_if_fail (GMIME_IS_MESSAGE (message), NULL);

	priv = _g_mime_message_get_private (message);

	if (g_mime_internet_address_list_length (priv->addrlists[GMIME_ADDRESS_TYPE_RESENT_SENDER]) > 0 ||
	    g_mime_internet_address_list_length (priv->addrlists[GMIME_ADDRESS_TYPE_RESENT_FROM]) > 0) {
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
		mailboxes = g_mime_internet_address_list_get_mailboxes (priv->addrlists[type]);

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

static gboolean
message_has_duplicate_header (GMimeMessage *message, const char *name)
{
	GMimeEntityPrivate *priv = _g_mime_entity_get_private (GMIME_ENTITY (message));
	GMimeHeaderList *headers = priv->headers;

	return g_mime_header_list_index_of (headers, name) != g_mime_header_list_last_index_of (headers, name);
}

static void
message_set_resent_header (GMimeMessage *message, const char *name, const char *value)
{
	GMimeEntityPrivate *priv = _g_mime_entity_get_private (GMIME_ENTITY (message));
	GMimeHeaderList *headers = priv->headers;
	GMimeHeader *header = g_mime_header_list_get_header (headers, name);

	if (header)
		g_mime_header_set_value (header, NULL, value, NULL);
	else
		g_mime_header_list_append (headers, name, value, NULL);
}

/**
 * g_mime_message_set_importance:
 * @message: a #GMimeMessage
 * @importance: the message importance
 *
 * Sets the Importance header to low, normal, or high. Assigning the
 * current importance preserves a single raw header; duplicates are replaced.
 * Invalid enum values are programming errors and leave the message unchanged.
 **/
void
g_mime_message_set_importance (GMimeMessage *message, GMimeMessageImportance importance)
{
	static const char *values[] = { "low", "normal", "high" };
	GMimeMessagePrivate *priv;

	g_return_if_fail (GMIME_IS_MESSAGE (message));
	g_return_if_fail (importance >= GMIME_MESSAGE_IMPORTANCE_LOW && importance <= GMIME_MESSAGE_IMPORTANCE_HIGH);

	priv = _g_mime_message_get_private (message);

	if (priv->importance != importance || message_has_duplicate_header (message, "Importance"))
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
	GMimeMessagePrivate *priv;

	g_return_val_if_fail (GMIME_IS_MESSAGE (message), GMIME_MESSAGE_IMPORTANCE_NORMAL);

	priv = _g_mime_message_get_private (message);

	return priv->importance;
}

/**
 * g_mime_message_set_priority:
 * @message: a #GMimeMessage
 * @priority: the message priority
 *
 * Sets the Priority header to non-urgent, normal, or urgent. Assigning
 * the current priority preserves a single raw header; duplicates are replaced.
 * Invalid enum values are programming errors and leave the message unchanged.
 **/
void
g_mime_message_set_priority (GMimeMessage *message, GMimeMessagePriority priority)
{
	static const char *values[] = { "non-urgent", "normal", "urgent" };
	GMimeMessagePrivate *priv;

	g_return_if_fail (GMIME_IS_MESSAGE (message));
	g_return_if_fail (priority >= GMIME_MESSAGE_PRIORITY_NON_URGENT && priority <= GMIME_MESSAGE_PRIORITY_URGENT);

	priv = _g_mime_message_get_private (message);

	if (priv->priority != priority || message_has_duplicate_header (message, "Priority"))
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
	GMimeMessagePrivate *priv;

	g_return_val_if_fail (GMIME_IS_MESSAGE (message), GMIME_MESSAGE_PRIORITY_NORMAL);

	priv = _g_mime_message_get_private (message);

	return priv->priority;
}

/**
 * g_mime_message_set_x_priority:
 * @message: a #GMimeMessage
 * @priority: the message's numeric priority
 *
 * Sets the X-Priority header to its numeric value and descriptive label.
 * Assigning the current priority preserves a single raw header; duplicates
 * are replaced. Invalid enum values are programming errors and leave the
 * message unchanged.
 **/
void
g_mime_message_set_x_priority (GMimeMessage *message, GMimeXMessagePriority priority)
{
	static const char *values[] = { "1 (Highest)", "2 (High)", "3 (Normal)", "4 (Low)", "5 (Lowest)" };
	GMimeMessagePrivate *priv;

	g_return_if_fail (GMIME_IS_MESSAGE (message));
	g_return_if_fail (priority >= GMIME_X_MESSAGE_PRIORITY_HIGHEST && priority <= GMIME_X_MESSAGE_PRIORITY_LOWEST);

	priv = _g_mime_message_get_private (message);

	if (priv->x_priority != priority || message_has_duplicate_header (message, "X-Priority"))
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
	GMimeMessagePrivate *priv;

	g_return_val_if_fail (GMIME_IS_MESSAGE (message), GMIME_X_MESSAGE_PRIORITY_NORMAL);

	priv = _g_mime_message_get_private (message);

	return priv->x_priority;
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
	GMimeMessagePrivate *priv;

	g_return_val_if_fail (GMIME_IS_MESSAGE (message), NULL);
	
	priv = _g_mime_message_get_private (message);

	return priv->subject;
}


static void
message_set_date (GMimeMessage *message, const char *name, GDateTime **cache, GDateTime *date, gboolean replace)
{
	char *str;

	g_return_if_fail (date != NULL);

	if (*cache && g_date_time_equal (*cache, date) && (!replace || !message_has_duplicate_header (message, name)))
		return;

	if (!(str = g_mime_utils_header_format_date (date)))
		return;

	g_date_time_ref (date);

	if (replace)
		g_mime_entity_set_header ((GMimeEntity *) message, name, str, NULL);
	else
		message_set_resent_header (message, name, str);

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
 * seconds. Assigning the same instant preserves the current cached value
 * and raw header unless duplicate Date headers need to be replaced.
 * A %NULL date is a programming error.
 **/
void
g_mime_message_set_date (GMimeMessage *message, GDateTime *date)
{
	GMimeMessagePrivate *priv;

	g_return_if_fail (GMIME_IS_MESSAGE (message));

	priv = _g_mime_message_get_private (message);

	message_set_date (message, "Date", &priv->date, date, TRUE);
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
	GMimeMessagePrivate *priv;

	g_return_val_if_fail (GMIME_IS_MESSAGE (message), NULL);
	
	priv = _g_mime_message_get_private (message);

	return priv->date;
}

/**
 * g_mime_message_set_resent_date:
 * @message: a #GMimeMessage
 * @date: a date to be used in the Resent-Date header
 *
 * Sets the Resent-Date header independently of Date. The message retains
 * @date, including its timezone and microseconds; the header represents
 * whole seconds. Assigning the same instant preserves the current cached
 * value, raw header, and any duplicates. Changed values update only the first
 * Resent-Date header, preserving older resent blocks. A %NULL date is a
 * programming error.
 **/
void
g_mime_message_set_resent_date (GMimeMessage *message, GDateTime *date)
{
	GMimeMessagePrivate *priv;

	g_return_if_fail (GMIME_IS_MESSAGE (message));

	priv = _g_mime_message_get_private (message);

	message_set_date (message, "Resent-Date", &priv->resent_date, date, FALSE);
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
	GMimeMessagePrivate *priv;

	g_return_val_if_fail (GMIME_IS_MESSAGE (message), NULL);

	priv = _g_mime_message_get_private (message);

	return priv->resent_date;
}


/**
 * g_mime_message_set_message_id: 
 * @message: A #GMimeMessage
 * @message_id: a UTF-8 message-id, with or without angle brackets
 *
 * Sets the Message-Id after normalizing a complete message-id. Invalid
 * values, including empty IDs, extra tokens, and non-folding line breaks,
 * are programming errors and leave the message unchanged. Assigning the
 * current ID preserves a single raw header; duplicates are replaced.
 **/
void
g_mime_message_set_message_id (GMimeMessage *message, const char *message_id)
{
	GMimeMessagePrivate *priv;
	char *id, *msgid;
	
	g_return_if_fail (GMIME_IS_MESSAGE (message));
	g_return_if_fail (message_id != NULL);

	priv = _g_mime_message_get_private (message);

	if (!g_strcmp0 (priv->message_id, message_id) && !message_has_duplicate_header (message, "Message-Id"))
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
	GMimeMessagePrivate *priv;

	g_return_val_if_fail (GMIME_IS_MESSAGE (message), NULL);
	
	priv = _g_mime_message_get_private (message);

	return priv->message_id;
}

/**
 * g_mime_message_set_in_reply_to:
 * @message: a #GMimeMessage
 * @message_id: (nullable): a UTF-8 message-id, with or without angle brackets
 *
 * Sets the In-Reply-To header after normalizing a complete message-id.
 * Invalid values are programming errors and leave the message unchanged.
 * Assigning the current ID preserves a single raw header; duplicates are
 * replaced. A %NULL value removes all In-Reply-To headers, including
 * malformed duplicates.
 **/
void
g_mime_message_set_in_reply_to (GMimeMessage *message, const char *message_id)
{
	GMimeMessagePrivate *priv;
	char *id, *value;

	g_return_if_fail (GMIME_IS_MESSAGE (message));

	if (!message_id) {
		GMimeEntityPrivate *entity_priv = _g_mime_entity_get_private ((GMimeEntity *) message);

		g_mime_header_list_remove_all (entity_priv->headers, "In-Reply-To");
		return;
	}

	priv = _g_mime_message_get_private (message);

	if (!g_strcmp0 (priv->in_reply_to, message_id) && !message_has_duplicate_header (message, "In-Reply-To"))
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
	GMimeMessagePrivate *priv;

	g_return_val_if_fail (GMIME_IS_MESSAGE (message), NULL);

	priv = _g_mime_message_get_private (message);

	return priv->in_reply_to;
}

/**
 * g_mime_message_set_resent_message_id:
 * @message: a #GMimeMessage
 * @message_id: a UTF-8 message-id, with or without angle brackets
 *
 * Sets the Resent-Message-Id header after normalizing a complete message-id.
 * Invalid values, including %NULL, are programming errors and leave the
 * message unchanged. Assigning the current ID preserves the raw header.
 * Changed values update only the first Resent-Message-Id header, preserving
 * older resent blocks.
 **/
void
g_mime_message_set_resent_message_id (GMimeMessage *message, const char *message_id)
{
	GMimeMessagePrivate *priv;
	char *id, *value;

	g_return_if_fail (GMIME_IS_MESSAGE (message));
	g_return_if_fail (message_id != NULL);

	priv = _g_mime_message_get_private (message);

	if (!g_strcmp0 (priv->resent_message_id, message_id))
		return;

	id = _g_mime_parse_message_id_value (message_id);
	g_return_if_fail (id != NULL);

	value = g_strdup_printf ("<%s>", id);
	g_free (id);

	message_set_resent_header (message, "Resent-Message-Id", value);
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
	GMimeMessagePrivate *priv;

	g_return_val_if_fail (GMIME_IS_MESSAGE (message), NULL);

	priv = _g_mime_message_get_private (message);

	return priv->resent_message_id;
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
	GMimeMessagePrivate *priv;

	g_return_val_if_fail (GMIME_IS_MESSAGE (message), NULL);

	priv = _g_mime_message_get_private (message);

	return priv->references;
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
	GMimeMessagePrivate *priv;

	g_return_val_if_fail (GMIME_IS_MESSAGE (message), NULL);
	
	priv = _g_mime_message_get_private (message);

	if (priv->mime_part == NULL)
		return NULL;
	
	return priv->mime_part;
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
	GMimeMessagePrivate *priv;

	g_return_if_fail (GMIME_IS_ENTITY (mime_part));
	g_return_if_fail (GMIME_IS_MESSAGE (message));
	
	priv = _g_mime_message_get_private (message);

	if (priv->mime_part == mime_part)
		return;
	
	if (priv->mime_part)
		g_object_unref (priv->mime_part);
	
	if (mime_part) {
		GMimeEntityPrivate *entity_priv = _g_mime_entity_get_private ((GMimeEntity *) message);
		GMimeEntityPrivate *mime_part_priv = _g_mime_entity_get_private (mime_part);
		GMimeHeaderList *headers = entity_priv->headers;
		GMimeHeader *header;
		int i;
		
		if (!g_mime_header_list_contains (headers, "MIME-Version"))
			g_mime_header_list_append (headers, "MIME-Version", "1.0", NULL);

		for (i = 0; i < g_mime_header_list_get_count (mime_part_priv->headers); i++) {
			header = g_mime_header_list_get_header_at (mime_part_priv->headers, i);
			_g_mime_header_set_offset (header, -1);
		}
		
		g_object_ref (mime_part);
	}
	
	priv->mime_part = mime_part;
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
	GMimeMessagePrivate *priv;

	g_return_if_fail (GMIME_IS_MESSAGE (message));
	g_return_if_fail (callback != NULL);
	
	priv = _g_mime_message_get_private (message);

	callback ((GMimeEntity *) message, priv->mime_part, user_data);
	
	/* Attached messages are single body parts; do not descend into message/rfc822 content. */
	if (GMIME_IS_MULTIPART (priv->mime_part))
		g_mime_multipart_foreach ((GMimeMultipart *) priv->mime_part, callback, user_data);
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
 * Path specifiers use the same numbering as #GMimeIterator.
 *
 * Returns: %TRUE if the entity exists, or %FALSE otherwise.
 **/
gboolean
g_mime_message_try_get_entity (GMimeMessage *message, const char *path_specifier, GMimeEntity **entity)
{
	const char *inptr = path_specifier;
	gboolean message_body = TRUE;
	GMimeMessagePrivate *priv;
	GMimeEntity *current;
	int index, digit;

	g_return_val_if_fail (GMIME_IS_MESSAGE (message), FALSE);
	g_return_val_if_fail (path_specifier != NULL, FALSE);
	g_return_val_if_fail (entity != NULL, FALSE);

	priv = _g_mime_message_get_private (message);
	*entity = NULL;

	if (!(current = priv->mime_part))
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
			GMimeMessagePrivate *embedded_priv;

			if (!embedded)
				return FALSE;

			embedded_priv = _g_mime_message_get_private (embedded);

			if (!(current = embedded_priv->mime_part))
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
	GMimeMessagePrivate *priv;
	GMimeEntity *mime_part;
	
	g_return_val_if_fail (GMIME_IS_MESSAGE (message), NULL);
	
	priv = _g_mime_message_get_private (message);

	if (!(mime_part = priv->mime_part))
		return NULL;
	
	if (GMIME_IS_MULTIPART (mime_part))
		return multipart_guess_body ((GMimeMultipart *) mime_part);
	
	if (GMIME_IS_PART (mime_part) && part_is_textual (mime_part))
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
	GMimeAutocryptHeaderList *retlist = NULL;
	GMimeAutocryptHeader *ret = NULL;
	GDateTime *effective_date = NULL;
	GMimeMessagePrivate *priv;
	GDateTime *newnow = NULL;

	g_return_val_if_fail (GMIME_IS_MESSAGE (message), NULL);

	priv = _g_mime_message_get_private (message);

	if (now == NULL)
		now = newnow = g_date_time_new_now_utc ();

	effective_date = now;

	if (priv->date && g_date_time_compare (priv->date, now) < 0)
		effective_date = priv->date;

	retlist = g_mime_entity_get_autocrypt_headers (GMIME_ENTITY (message),
						       effective_date,
						       "autocrypt",
						       priv->addrlists[GMIME_ADDRESS_TYPE_FROM],
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
	GMimeAutocryptHeaderList *ret = NULL;
	GMimeInternetAddressList *addresses;
	GDateTime *effective_date = NULL;
	GDateTime *newnow = NULL;
	GMimeMessagePrivate *priv;

	g_return_val_if_fail (GMIME_IS_MESSAGE (message), NULL);
	g_return_val_if_fail (GMIME_IS_ENTITY (inner_part), NULL);

	addresses = g_mime_message_get_all_recipients (message);
	priv = _g_mime_message_get_private (message);
	
	if (now == NULL)
		now = newnow = g_date_time_new_now_utc ();

	effective_date = now;

	if (priv->date && g_date_time_compare (priv->date, now) < 0)
		effective_date = priv->date;

	ret = g_mime_entity_get_autocrypt_headers (inner_part,
						   effective_date,
						   "autocrypt-gossip",
						   addresses, FALSE);

	g_object_unref (addresses);
	
	if (newnow)
		g_date_time_unref (newnow);
	
	return ret;
}


#ifdef ENABLE_CRYPTO

/**
 * g_mime_message_get_autocrypt_gossip_headers: (skip)
 * @message: a #GMimeMessage object, which is expected to be encrypted.
 * @now: (nullable): a #GDateTime object, or %NULL
 * @ctx: a borrowed GPGME OpenPGP context
 * @err: a #GError (can be %NULL)
 *
 * Creates a new #GMimeAutocryptHeaderList of relevant headers of the
 * given type based on the recipient(s) of an e-mail message.
 * 
 * Returns the same object as
 * g_mime_message_get_autocrypt_gossip_headers_from_inner_part(), but
 * handles decryption and cleanup automatically.
 *
 * Configure network and session-key policy on @ctx before calling.
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
								       gpgme_ctx_t ctx,
								       GError **err)
{
	GMimeAutocryptHeaderList *ret = NULL;
	GMimeEntity *top_level = NULL;
	GMimeEntity *inner_part = NULL;

	g_return_val_if_fail (GMIME_IS_MESSAGE (message), NULL);

	top_level = g_mime_message_get_mime_part (message);

	if (!GMIME_IS_MULTIPART_ENCRYPTED (top_level))
		return NULL;

	inner_part = g_mime_multipart_encrypted_decrypt (GMIME_MULTIPART_ENCRYPTED (top_level),
							 ctx, NULL, NULL,
							 err);

	if (inner_part) {
		ret = g_mime_message_get_autocrypt_gossip_headers_from_inner_part (message, now, inner_part);
		g_object_unref (inner_part);
	}

	return ret;
}

#endif /* ENABLE_CRYPTO */
