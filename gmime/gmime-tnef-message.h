/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*  GMime
 *  Copyright (C) Jeffrey Stedfast
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

#ifndef __GMIME_TNEF_MESSAGE_H__
#define __GMIME_TNEF_MESSAGE_H__

#include <gmime/gmime-tnef-reader.h>

G_BEGIN_DECLS

/**
 * GMimeTnefMessageBodyFormat:
 * @GMIME_TNEF_MESSAGE_BODY_FORMAT_TEXT: Plain text.
 * @GMIME_TNEF_MESSAGE_BODY_FORMAT_HTML: HTML markup.
 * @GMIME_TNEF_MESSAGE_BODY_FORMAT_COMPRESSED_RTF: MS-OXRTFCP encoded RTF.
 *
 * The body representations kept separately by a #GMimeTnefMessage.
 **/
typedef enum {
	GMIME_TNEF_MESSAGE_BODY_FORMAT_TEXT,
	GMIME_TNEF_MESSAGE_BODY_FORMAT_HTML,
	GMIME_TNEF_MESSAGE_BODY_FORMAT_COMPRESSED_RTF
} GMimeTnefMessageBodyFormat;

#define GMIME_TYPE_TNEF_MESSAGE (g_mime_tnef_message_get_type ())
#define GMIME_TYPE_TNEF_RECIPIENT (g_mime_tnef_recipient_get_type ())
#define GMIME_TYPE_TNEF_MESSAGE_BODY (g_mime_tnef_message_body_get_type ())
#define GMIME_TYPE_TNEF_ATTACHMENT (g_mime_tnef_attachment_get_type ())

/**
 * GMimeTnefMessage:
 *
 * A snapshot of message properties, recipient rows, alternative bodies and
 * attachments. Member getters borrow from the message; take a reference to
 * retain a member independently.
 **/
G_DECLARE_FINAL_TYPE (GMimeTnefMessage, g_mime_tnef_message, GMIME, TNEF_MESSAGE, GObject)
/**
 * GMimeTnefRecipient:
 *
 * An immutable recipient row with UTF-8 views of its address metadata.
 **/
G_DECLARE_FINAL_TYPE (GMimeTnefRecipient, g_mime_tnef_recipient, GMIME, TNEF_RECIPIENT, GObject)
/**
 * GMimeTnefMessageBody:
 *
 * A retained body representation. Raw and decoded streams have independent
 * lifetimes and do not seek the original input.
 **/
G_DECLARE_FINAL_TYPE (GMimeTnefMessageBody, g_mime_tnef_message_body, GMIME, TNEF_MESSAGE_BODY, GObject)
/**
 * GMimeTnefAttachment:
 *
 * Attachment properties and optional payload. Embedded messages are parsed
 * only when requested, not while the enclosing message is loaded.
 **/
G_DECLARE_FINAL_TYPE (GMimeTnefAttachment, g_mime_tnef_attachment, GMIME, TNEF_ATTACHMENT, GObject)

GMimeTnefMessage *g_mime_tnef_message_load (GMimeStream *stream, const GMimeTnefOptions *options, GError **error);
GMimeTnefMessage *g_mime_tnef_message_load_from_reader (GMimeTnefReader *reader, GError **error);

int g_mime_tnef_message_get_codepage (GMimeTnefMessage *message);
guint16 g_mime_tnef_message_get_legacy_key (GMimeTnefMessage *message);
GMimeTnefPropertySet *g_mime_tnef_message_get_properties (GMimeTnefMessage *message);
const char *g_mime_tnef_message_get_subject (GMimeTnefMessage *message);
const char *g_mime_tnef_message_get_message_class (GMimeTnefMessage *message);
const char *g_mime_tnef_message_get_internet_message_id (GMimeTnefMessage *message);
guint g_mime_tnef_message_get_recipient_count (GMimeTnefMessage *message);
GMimeTnefRecipient *g_mime_tnef_message_get_recipient (GMimeTnefMessage *message, guint index);
guint g_mime_tnef_message_get_attachment_count (GMimeTnefMessage *message);
GMimeTnefAttachment *g_mime_tnef_message_get_attachment (GMimeTnefMessage *message, guint index);
GMimeTnefMessageBody *g_mime_tnef_message_get_text_body (GMimeTnefMessage *message);
GMimeTnefMessageBody *g_mime_tnef_message_get_html_body (GMimeTnefMessage *message);
GMimeTnefMessageBody *g_mime_tnef_message_get_rtf_body (GMimeTnefMessage *message);

GMimeTnefPropertySet *g_mime_tnef_recipient_get_properties (GMimeTnefRecipient *recipient);
GMimeTnefRecipientType g_mime_tnef_recipient_get_recipient_type (GMimeTnefRecipient *recipient);
const char *g_mime_tnef_recipient_get_display_name (GMimeTnefRecipient *recipient);
const char *g_mime_tnef_recipient_get_email_address (GMimeTnefRecipient *recipient);
const char *g_mime_tnef_recipient_get_address_type (GMimeTnefRecipient *recipient);

GMimeTnefMessageBodyFormat g_mime_tnef_message_body_get_format (GMimeTnefMessageBody *body);
GMimeTnefPropertyTag g_mime_tnef_message_body_get_tag (GMimeTnefMessageBody *body);
const char *g_mime_tnef_message_body_get_charset (GMimeTnefMessageBody *body);
gint64 g_mime_tnef_message_body_get_length (GMimeTnefMessageBody *body);
GMimeStream *g_mime_tnef_message_body_open_read (GMimeTnefMessageBody *body);
GMimeStream *g_mime_tnef_message_body_open_decoded_read (GMimeTnefMessageBody *body);
char *g_mime_tnef_message_body_get_text (GMimeTnefMessageBody *body, GError **error);

GMimeTnefPropertySet *g_mime_tnef_attachment_get_properties (GMimeTnefAttachment *attachment);
GMimeTnefAttachMethod g_mime_tnef_attachment_get_method (GMimeTnefAttachment *attachment);
GMimeTnefAttachFlags g_mime_tnef_attachment_get_flags (GMimeTnefAttachment *attachment);
const char *g_mime_tnef_attachment_get_file_name (GMimeTnefAttachment *attachment);
const char *g_mime_tnef_attachment_get_mime_type (GMimeTnefAttachment *attachment);
const char *g_mime_tnef_attachment_get_content_id (GMimeTnefAttachment *attachment);
gboolean g_mime_tnef_attachment_has_content (GMimeTnefAttachment *attachment);
gboolean g_mime_tnef_attachment_is_embedded_message (GMimeTnefAttachment *attachment);
gint64 g_mime_tnef_attachment_get_length (GMimeTnefAttachment *attachment);
GMimeStream *g_mime_tnef_attachment_open_read (GMimeTnefAttachment *attachment);
GMimeTnefMessage *g_mime_tnef_attachment_load_embedded_message (GMimeTnefAttachment *attachment, GError **error);

G_END_DECLS

#endif
