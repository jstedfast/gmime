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

#ifndef __GMIME_TNEF_CONVERSION_H__
#define __GMIME_TNEF_CONVERSION_H__

#include <gmime/gmime-message.h>
#include <gmime/gmime-tnef-message.h>

G_BEGIN_DECLS

/**
 * GMimeTnefConversionLossKind:
 * @GMIME_TNEF_CONVERSION_LOSS_INVALID_MIME_SKELETON: The original MIME structure could not be restored.
 * @GMIME_TNEF_CONVERSION_LOSS_UNPARSABLE_RECIPIENT: No Internet mailbox could be recovered.
 * @GMIME_TNEF_CONVERSION_LOSS_INVALID_MESSAGE_ID: An invalid message or content identifier was omitted.
 * @GMIME_TNEF_CONVERSION_LOSS_UNSUPPORTED_ATTACH_METHOD: An unsupported attachment method was encountered.
 * @GMIME_TNEF_CONVERSION_LOSS_ATTACHMENT_WITHOUT_CONTENT: An attachment lacked data.
 * @GMIME_TNEF_CONVERSION_LOSS_INVALID_HEADER: An invalid Internet header was omitted.
 * @GMIME_TNEF_CONVERSION_LOSS_INVALID_EMBEDDED_MESSAGE: An embedded message could not be decoded.
 * @GMIME_TNEF_CONVERSION_LOSS_INVALID_RTF_BODY: An unusable rich-text body was dropped.
 * @GMIME_TNEF_CONVERSION_LOSS_RTF_CHECKSUM_MISMATCH: The rich-text body may be corrupt.
 * @GMIME_TNEF_CONVERSION_LOSS_INVALID_CALENDAR_DATA: Invalid appointment data was encountered.
 * @GMIME_TNEF_CONVERSION_LOSS_UNSUPPORTED_CALENDAR_DATA: Unsupported appointment data was encountered.
 * @GMIME_TNEF_CONVERSION_LOSS_CALENDAR_EXCEPTION_LIMIT_EXCEEDED: Calendar exception limits were reached.
 * @GMIME_TNEF_CONVERSION_LOSS_OLE_OBJECT_NOT_RENDERED: Original OLE bytes were retained instead of an image.
 *
 * Identifies a recoverable difference between the source message and its MIME representation.
 **/
typedef enum {
	GMIME_TNEF_CONVERSION_LOSS_INVALID_MIME_SKELETON,
	GMIME_TNEF_CONVERSION_LOSS_UNPARSABLE_RECIPIENT,
	GMIME_TNEF_CONVERSION_LOSS_INVALID_MESSAGE_ID,
	GMIME_TNEF_CONVERSION_LOSS_UNSUPPORTED_ATTACH_METHOD,
	GMIME_TNEF_CONVERSION_LOSS_ATTACHMENT_WITHOUT_CONTENT,
	GMIME_TNEF_CONVERSION_LOSS_INVALID_HEADER,
	GMIME_TNEF_CONVERSION_LOSS_INVALID_EMBEDDED_MESSAGE,
	GMIME_TNEF_CONVERSION_LOSS_INVALID_RTF_BODY,
	GMIME_TNEF_CONVERSION_LOSS_RTF_CHECKSUM_MISMATCH,
	GMIME_TNEF_CONVERSION_LOSS_INVALID_CALENDAR_DATA,
	GMIME_TNEF_CONVERSION_LOSS_UNSUPPORTED_CALENDAR_DATA,
	GMIME_TNEF_CONVERSION_LOSS_CALENDAR_EXCEPTION_LIMIT_EXCEEDED,
	GMIME_TNEF_CONVERSION_LOSS_OLE_OBJECT_NOT_RENDERED
} GMimeTnefConversionLossKind;

/**
 * GMimeTnefConversionLoss:
 * @kind: machine-readable reason
 * @description: human-readable detail, borrowed from the result
 *
 * One nonfatal loss encountered during conversion.
 **/
typedef struct {
	GMimeTnefConversionLossKind kind;
	const char *description;
} GMimeTnefConversionLoss;

/**
 * GMimeTnefOleObjectConverter:
 * @attachment: (transfer none): OLE attachment
 * @user_data: (closure) (nullable): caller data
 *
 * Returns: (transfer full) (nullable): PNG, JPEG, GIF, BMP or WebP stream,
 * read from its current position; %NULL leaves the compound file intact.
 **/
typedef GMimeStream *(*GMimeTnefOleObjectConverter) (GMimeTnefAttachment *attachment, gpointer user_data);

/**
 * GMimeTnefAttachmentPlaceholderCallback:
 * @attachment: (transfer none): attachment positioned in the RTF body
 * @entity: (transfer none) (nullable): converted MIME attachment, if any
 * @user_data: (closure) (nullable): caller data
 *
 * Returns: (transfer full) (nullable): replacement plain text. The converter
 * escapes this value when writing HTML.
 **/
typedef char *(*GMimeTnefAttachmentPlaceholderCallback) (GMimeTnefAttachment *attachment, GMimeEntity *entity,
							 gpointer user_data);

/**
 * GMimeTnefConversionOptions:
 *
 * Reference-counted MIME conversion settings. By default calendar generation
 * is enabled, nested TNEF is preserved and neither callback is installed.
 **/
typedef struct _GMimeTnefConversionOptions GMimeTnefConversionOptions;
/**
 * GMimeTnefConversionResult:
 *
 * Owns an independent MIME message and all recoverable conversion losses.
 **/
typedef struct _GMimeTnefConversionResult GMimeTnefConversionResult;

#define GMIME_TYPE_TNEF_CONVERSION_OPTIONS (g_mime_tnef_conversion_options_get_type ())
#define GMIME_TYPE_TNEF_CONVERSION_RESULT (g_mime_tnef_conversion_result_get_type ())

GType g_mime_tnef_conversion_options_get_type (void);
GMimeTnefConversionOptions *g_mime_tnef_conversion_options_new (void);
GMimeTnefConversionOptions *g_mime_tnef_conversion_options_ref (GMimeTnefConversionOptions *options);
void g_mime_tnef_conversion_options_unref (GMimeTnefConversionOptions *options);
GMimeTnefConversionOptions *g_mime_tnef_conversion_options_clone (const GMimeTnefConversionOptions *options);
gboolean g_mime_tnef_conversion_options_get_convert_embedded_messages (const GMimeTnefConversionOptions *options);
void g_mime_tnef_conversion_options_set_convert_embedded_messages (GMimeTnefConversionOptions *options, gboolean enable);
gboolean g_mime_tnef_conversion_options_get_generate_calendar (const GMimeTnefConversionOptions *options);
void g_mime_tnef_conversion_options_set_generate_calendar (GMimeTnefConversionOptions *options, gboolean enable);
guint g_mime_tnef_conversion_options_get_max_calendar_exceptions (const GMimeTnefConversionOptions *options);
void g_mime_tnef_conversion_options_set_max_calendar_exceptions (GMimeTnefConversionOptions *options, guint maximum);
guint64 g_mime_tnef_conversion_options_get_max_calendar_exceptions_size (const GMimeTnefConversionOptions *options);
void g_mime_tnef_conversion_options_set_max_calendar_exceptions_size (GMimeTnefConversionOptions *options, guint64 maximum);
void g_mime_tnef_conversion_options_set_ole_object_converter (GMimeTnefConversionOptions *options,
	GMimeTnefOleObjectConverter callback, gpointer user_data, GDestroyNotify destroy);
GMimeTnefOleObjectConverter g_mime_tnef_conversion_options_get_ole_object_converter (
	const GMimeTnefConversionOptions *options, gpointer *user_data);
void g_mime_tnef_conversion_options_set_attachment_placeholder_callback (GMimeTnefConversionOptions *options,
	GMimeTnefAttachmentPlaceholderCallback callback, gpointer user_data, GDestroyNotify destroy);
GMimeTnefAttachmentPlaceholderCallback g_mime_tnef_conversion_options_get_attachment_placeholder_callback (
	const GMimeTnefConversionOptions *options, gpointer *user_data);

GMimeTnefConversionResult *g_mime_tnef_message_convert_to_mime (GMimeTnefMessage *message,
	const GMimeTnefConversionOptions *options, GError **error);
GType g_mime_tnef_conversion_result_get_type (void);
GMimeTnefConversionResult *g_mime_tnef_conversion_result_ref (GMimeTnefConversionResult *result);
void g_mime_tnef_conversion_result_unref (GMimeTnefConversionResult *result);
GMimeMessage *g_mime_tnef_conversion_result_get_message (GMimeTnefConversionResult *result);
guint g_mime_tnef_conversion_result_get_loss_count (const GMimeTnefConversionResult *result);
const GMimeTnefConversionLoss *g_mime_tnef_conversion_result_get_loss (const GMimeTnefConversionResult *result, guint index);

G_END_DECLS

#endif /* __GMIME_TNEF_CONVERSION_H__ */
