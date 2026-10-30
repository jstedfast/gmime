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


#ifndef __GMIME_MESSAGE_H__
#define __GMIME_MESSAGE_H__

#include <stdarg.h>
#include <time.h>

#include <gmime/gmime-internet-address.h>
#include <gmime/gmime-encodings.h>
#include <gmime/gmime-entity.h>
#include <gmime/gmime-header.h>
#include <gmime/gmime-message-id-list.h>
#include <gmime/gmime-stream.h>
#include <gmime/gmime-autocrypt.h>

G_BEGIN_DECLS

#define GMIME_TYPE_MESSAGE            (g_mime_message_get_type ())



/**
 * GMimeAddressType:
 * @GMIME_ADDRESS_TYPE_SENDER: Represents the addresses in the Sender header.
 * @GMIME_ADDRESS_TYPE_FROM: Represents the addresses in the From header.
 * @GMIME_ADDRESS_TYPE_REPLY_TO: Represents the addresses in the Reply-To header.
 * @GMIME_ADDRESS_TYPE_TO: Represents the recipients in the To header.
 * @GMIME_ADDRESS_TYPE_CC: Represents the recipients in the Cc header.
 * @GMIME_ADDRESS_TYPE_BCC: Represents the recipients in the Bcc header.
 * @GMIME_ADDRESS_TYPE_RESENT_FROM: Represents the authors in Resent-From headers.
 * @GMIME_ADDRESS_TYPE_RESENT_REPLY_TO: Represents the addresses in Resent-Reply-To headers.
 * @GMIME_ADDRESS_TYPE_RESENT_TO: Represents the recipients in Resent-To headers.
 * @GMIME_ADDRESS_TYPE_RESENT_CC: Represents the recipients in Resent-Cc headers.
 * @GMIME_ADDRESS_TYPE_RESENT_BCC: Represents the recipients in Resent-Bcc headers.
 * @GMIME_ADDRESS_TYPE_RESENT_SENDER: Represents the addresses in Resent-Sender headers.
 *
 * An address type.
 **/
typedef enum _GMimeAddressType {
	GMIME_ADDRESS_TYPE_SENDER,
	GMIME_ADDRESS_TYPE_FROM,
	GMIME_ADDRESS_TYPE_REPLY_TO,
	GMIME_ADDRESS_TYPE_TO,
	GMIME_ADDRESS_TYPE_CC,
	GMIME_ADDRESS_TYPE_BCC,
	GMIME_ADDRESS_TYPE_RESENT_FROM,
	GMIME_ADDRESS_TYPE_RESENT_REPLY_TO,
	GMIME_ADDRESS_TYPE_RESENT_TO,
	GMIME_ADDRESS_TYPE_RESENT_CC,
	GMIME_ADDRESS_TYPE_RESENT_BCC,
	GMIME_ADDRESS_TYPE_RESENT_SENDER,
} GMimeAddressType;

/**
 * GMimeMessageImportance:
 * @GMIME_MESSAGE_IMPORTANCE_LOW: Low importance.
 * @GMIME_MESSAGE_IMPORTANCE_NORMAL: Normal importance.
 * @GMIME_MESSAGE_IMPORTANCE_HIGH: High importance.
 *
 * Values of the Importance header.
 **/
typedef enum {
	GMIME_MESSAGE_IMPORTANCE_LOW,
	GMIME_MESSAGE_IMPORTANCE_NORMAL,
	GMIME_MESSAGE_IMPORTANCE_HIGH
} GMimeMessageImportance;

/**
 * GMimeMessagePriority:
 * @GMIME_MESSAGE_PRIORITY_NON_URGENT: Non-urgent priority.
 * @GMIME_MESSAGE_PRIORITY_NORMAL: Normal priority.
 * @GMIME_MESSAGE_PRIORITY_URGENT: Urgent priority.
 *
 * Values of the Priority header.
 **/
typedef enum {
	GMIME_MESSAGE_PRIORITY_NON_URGENT,
	GMIME_MESSAGE_PRIORITY_NORMAL,
	GMIME_MESSAGE_PRIORITY_URGENT
} GMimeMessagePriority;

/**
 * GMimeXMessagePriority:
 * @GMIME_X_MESSAGE_PRIORITY_HIGHEST: Highest priority.
 * @GMIME_X_MESSAGE_PRIORITY_HIGH: High priority.
 * @GMIME_X_MESSAGE_PRIORITY_NORMAL: Normal priority.
 * @GMIME_X_MESSAGE_PRIORITY_LOW: Low priority.
 * @GMIME_X_MESSAGE_PRIORITY_LOWEST: Lowest priority.
 *
 * Numeric values of the X-Priority header.
 **/
typedef enum {
	GMIME_X_MESSAGE_PRIORITY_HIGHEST = 1,
	GMIME_X_MESSAGE_PRIORITY_HIGH,
	GMIME_X_MESSAGE_PRIORITY_NORMAL,
	GMIME_X_MESSAGE_PRIORITY_LOW,
	GMIME_X_MESSAGE_PRIORITY_LOWEST
} GMimeXMessagePriority;


/**
 * GMimeMessage:
 *
 * A MIME Message object.
 **/
G_DECLARE_DERIVABLE_TYPE (GMimeMessage, g_mime_message, GMIME, MESSAGE, GMimeEntity)

struct _GMimeMessageClass {
	GMimeEntityClass parent_class;

	/*< private >*/
	gpointer reserved[8];
};


GMimeMessage *g_mime_message_new (gboolean pretty_headers);

GMimeMessage *g_mime_message_load (GMimeParserOptions *options, GMimeStream *stream, GError **error);
GMimeMessage *g_mime_message_load_with_storage (GMimeParserOptions *options, GMimeStream *stream,
					      GMimeContentStorage storage, GError **error);
GMimeMessage *g_mime_message_load_from_file (GMimeParserOptions *options, const char *filename, GError **error);

ssize_t g_mime_message_write_headers_to_stream (GMimeMessage *message, GMimeFormatOptions *options, GMimeStream *stream);

GMimeInternetAddressList *g_mime_message_get_from (GMimeMessage *message);
GMimeInternetAddressList *g_mime_message_get_sender (GMimeMessage *message);
GMimeInternetAddressList *g_mime_message_get_reply_to (GMimeMessage *message);
GMimeInternetAddressList *g_mime_message_get_to (GMimeMessage *message);
GMimeInternetAddressList *g_mime_message_get_cc (GMimeMessage *message);
GMimeInternetAddressList *g_mime_message_get_bcc (GMimeMessage *message);

GMimeInternetAddressList *g_mime_message_get_resent_from (GMimeMessage *message);
GMimeInternetAddressList *g_mime_message_get_resent_reply_to (GMimeMessage *message);
GMimeInternetAddressList *g_mime_message_get_resent_to (GMimeMessage *message);
GMimeInternetAddressList *g_mime_message_get_resent_cc (GMimeMessage *message);
GMimeInternetAddressList *g_mime_message_get_resent_bcc (GMimeMessage *message);

GMimeInternetAddressList *g_mime_message_get_resent_sender (GMimeMessage *message);

gboolean g_mime_message_add_mailbox (GMimeMessage *message, GMimeAddressType type, const char *name, const char *addr, GError **error);
GMimeInternetAddressList *g_mime_message_get_addresses (GMimeMessage *message, GMimeAddressType type);
GMimeInternetAddressList *g_mime_message_get_all_recipients (GMimeMessage *message);
GMimeInternetAddressList *g_mime_message_get_recipients (GMimeMessage *message, gboolean only_unique);

void g_mime_message_set_importance (GMimeMessage *message, GMimeMessageImportance importance);
GMimeMessageImportance g_mime_message_get_importance (GMimeMessage *message);
void g_mime_message_set_priority (GMimeMessage *message, GMimeMessagePriority priority);
GMimeMessagePriority g_mime_message_get_priority (GMimeMessage *message);
void g_mime_message_set_x_priority (GMimeMessage *message, GMimeXMessagePriority priority);
GMimeXMessagePriority g_mime_message_get_x_priority (GMimeMessage *message);

void g_mime_message_set_subject (GMimeMessage *message, const char *subject, const char *charset);
const char *g_mime_message_get_subject (GMimeMessage *message);

void g_mime_message_set_date (GMimeMessage *message, GDateTime *date);
GDateTime *g_mime_message_get_date (GMimeMessage *message);

void g_mime_message_set_resent_date (GMimeMessage *message, GDateTime *date);
GDateTime *g_mime_message_get_resent_date (GMimeMessage *message);

void g_mime_message_set_message_id (GMimeMessage *message, const char *message_id);
const char *g_mime_message_get_message_id (GMimeMessage *message);

void g_mime_message_set_in_reply_to (GMimeMessage *message, const char *message_id);
const char *g_mime_message_get_in_reply_to (GMimeMessage *message);

void g_mime_message_set_resent_message_id (GMimeMessage *message, const char *message_id);
const char *g_mime_message_get_resent_message_id (GMimeMessage *message);

GMimeMessageIdList *g_mime_message_get_references (GMimeMessage *message);

GMimeEntity *g_mime_message_get_mime_part (GMimeMessage *message);
void g_mime_message_set_mime_part (GMimeMessage *message, GMimeEntity *mime_part);

GPtrArray *g_mime_message_get_body_parts (GMimeMessage *message);
GPtrArray *g_mime_message_get_attachments (GMimeMessage *message);

gboolean g_mime_message_try_get_entity (GMimeMessage *message, const char *path_specifier, GMimeEntity **entity);

GMimeAutocryptHeader *g_mime_message_get_autocrypt_header (GMimeMessage *message, GDateTime *now);
GMimeAutocryptHeaderList *g_mime_message_get_autocrypt_gossip_headers_from_inner_part (GMimeMessage *message, GDateTime *now, GMimeEntity *inner_part);

/* convenience functions */

void g_mime_message_foreach (GMimeMessage *message, GMimeEntityForeachFunc callback,
			     gpointer user_data);

GMimeEntity *g_mime_message_get_body (GMimeMessage *message);

G_END_DECLS

#endif /* __GMIME_MESSAGE_H__ */
