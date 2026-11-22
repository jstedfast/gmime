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

/**
 * SECTION: gmime-tnef-message
 * @title: GMimeTnefMessage
 * @section_id: gmime-gmime-tnef-message
 * @short_description: Access message bodies, recipients and attachments
 **/

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <errno.h>
#include <string.h>

#include "gmime-tnef-message-private.h"
#include "gmime-filter-rtf-compressed.h"
#include "gmime-internal.h"
#include "gmime-stream-filter.h"

G_DEFINE_TYPE (GMimeTnefMessage, g_mime_tnef_message, G_TYPE_OBJECT)
G_DEFINE_TYPE (GMimeTnefMessageBody, g_mime_tnef_message_body, G_TYPE_OBJECT)
G_DEFINE_TYPE (GMimeTnefAttachment, g_mime_tnef_attachment, G_TYPE_OBJECT)
G_DEFINE_TYPE (GMimeTnefRecipient, g_mime_tnef_recipient, G_TYPE_OBJECT)

static void
g_mime_tnef_message_finalize (GObject *object)
{
	GMimeTnefMessage *message = GMIME_TNEF_MESSAGE (object);
	guint i;

	g_mime_tnef_property_set_unref (message->properties);
	g_ptr_array_unref (message->attachments);
	g_ptr_array_unref (message->recipients);

	for (i = 0; i < G_N_ELEMENTS (message->bodies); i++)
		g_clear_object (&message->bodies[i]);

	g_free (message->message_class);
	G_OBJECT_CLASS (g_mime_tnef_message_parent_class)->finalize (object);
}

static void
g_mime_tnef_message_class_init (GMimeTnefMessageClass *klass)
{
	G_OBJECT_CLASS (klass)->finalize = g_mime_tnef_message_finalize;
}

static void
g_mime_tnef_message_init (GMimeTnefMessage *message)
{
	message->properties = _g_mime_tnef_property_set_new ();
	message->recipients = g_ptr_array_new_with_free_func (g_object_unref);
	message->attachments = g_ptr_array_new_with_free_func (g_object_unref);
}

static void
g_mime_tnef_recipient_finalize (GObject *object)
{
	GMimeTnefRecipient *recipient = GMIME_TNEF_RECIPIENT (object);

	g_clear_pointer (&recipient->properties, g_mime_tnef_property_set_unref);

	G_OBJECT_CLASS (g_mime_tnef_recipient_parent_class)->finalize (object);
}

static void
g_mime_tnef_recipient_class_init (GMimeTnefRecipientClass *klass)
{
	G_OBJECT_CLASS (klass)->finalize = g_mime_tnef_recipient_finalize;
}

static void
g_mime_tnef_recipient_init (GMimeTnefRecipient *recipient)
{
}

static void
g_mime_tnef_attachment_finalize (GObject *object)
{
	GMimeTnefAttachment *attachment = GMIME_TNEF_ATTACHMENT (object);

	g_mime_tnef_property_set_unref (attachment->properties);
	g_clear_pointer (&attachment->content, g_bytes_unref);
	g_clear_pointer (&attachment->embedded, _g_mime_tnef_replay_context_free);

	G_OBJECT_CLASS (g_mime_tnef_attachment_parent_class)->finalize (object);
}

static void
g_mime_tnef_attachment_class_init (GMimeTnefAttachmentClass *klass)
{
	G_OBJECT_CLASS (klass)->finalize = g_mime_tnef_attachment_finalize;
}

static void
g_mime_tnef_attachment_init (GMimeTnefAttachment *attachment)
{
	attachment->properties = _g_mime_tnef_property_set_new ();
}

static void
g_mime_tnef_message_body_finalize (GObject *object)
{
	GMimeTnefMessageBody *body = GMIME_TNEF_MESSAGE_BODY (object);

	g_clear_pointer (&body->content, g_bytes_unref);
	g_free (body->fallback);
	g_free (body->charset);

	G_OBJECT_CLASS (g_mime_tnef_message_body_parent_class)->finalize (object);
}

static void
g_mime_tnef_message_body_class_init (GMimeTnefMessageBodyClass *klass)
{
	G_OBJECT_CLASS (klass)->finalize = g_mime_tnef_message_body_finalize;
}

static void
g_mime_tnef_message_body_init (GMimeTnefMessageBody *body)
{
}

static GMimeTnefProperty *
lookup_scalar (GMimeTnefPropertySet *set, GMimeTnefPropertyTag tag)
{
	GMimeTnefProperty *prop = g_mime_tnef_property_set_lookup (set, tag);

	if (!prop || prop->values->len != 1 || g_mime_tnef_property_tag_is_multi_valued (prop->tag))
		return NULL;

	return prop;
}

const char *
_g_mime_tnef_set_string (GMimeTnefPropertySet *set, GMimeTnefPropertyTag tag)
{
	GMimeTnefProperty *prop = lookup_scalar (set, tag);
	const GValue *v;

	if (!prop)
		return NULL;

	v = &g_array_index (prop->values, GValue, 0);

	switch (g_mime_tnef_property_tag_get_value_type (prop->tag)) {
	case GMIME_TNEF_PROPERTY_TYPE_STRING8:
	case GMIME_TNEF_PROPERTY_TYPE_UNICODE:
		return g_value_get_string (v);
	case GMIME_TNEF_PROPERTY_TYPE_BINARY:
		/* Some producers put textual metadata in a binary property. Keep
		 * the native bytes intact and make the UTF-8 view stable as well. */
		if (!g_atomic_pointer_get (&prop->decoded_binary)) {
			char *charset = _g_mime_tnef_get_charset (prop->codepage);
			char *text = _g_mime_tnef_decode_string (charset, g_value_get_boxed (v), FALSE);

			g_free (charset);

			if (!g_atomic_pointer_compare_and_exchange (&prop->decoded_binary, NULL, text))
				g_free (text);
		}

		return g_atomic_pointer_get (&prop->decoded_binary);
	default:
		return NULL;
	}
}

int
_g_mime_tnef_set_int (GMimeTnefPropertySet *set, GMimeTnefPropertyTag tag, int fallback)
{
	GMimeTnefProperty *prop = lookup_scalar (set, tag);
	const GValue *v;
	double d;

	if (!prop)
		return fallback;

	v = &g_array_index (prop->values, GValue, 0);

	if (G_VALUE_HOLDS_INT (v))
		return g_value_get_int (v);

	if (G_VALUE_HOLDS_BOOLEAN (v))
		return g_value_get_boolean (v);

	if (G_VALUE_HOLDS_INT64 (v)) {
		gint64 n = g_value_get_int64 (v);
		return (int) (g_mime_tnef_property_tag_get_value_type (prop->tag) == GMIME_TNEF_PROPERTY_TYPE_CURRENCY ? n / 10000 : n);
	}

	if (G_VALUE_HOLDS_FLOAT (v) || G_VALUE_HOLDS_DOUBLE (v)) {
		d = G_VALUE_HOLDS_FLOAT (v) ? g_value_get_float (v) : g_value_get_double (v);

		if (d >= (double) G_MININT64 && d < -(double) G_MININT64)
			return (int) (gint64) d;
	}

	return fallback;
}

/* A read cursor holds the bytes, not the message. Multiple cursors (including
 * substreams) share immutable storage without sharing positions. */
typedef struct {
	GMimeStream parent_instance;
	GBytes *bytes;
} TnefContentStream;

typedef GMimeStreamClass TnefContentStreamClass;
static GType tnef_content_stream_get_type (void);
G_DEFINE_TYPE (TnefContentStream, tnef_content_stream, GMIME_TYPE_STREAM)

static GMimeStream *
content_stream_new (GBytes *bytes, gint64 start, gint64 end)
{
	TnefContentStream *stream = g_object_new (tnef_content_stream_get_type (), NULL);

	stream->bytes = bytes ? g_bytes_ref (bytes) : g_bytes_new (NULL, 0);
	g_mime_stream_construct (GMIME_STREAM (stream), start, end);

	return GMIME_STREAM (stream);
}

static gboolean
content_bounds (GMimeStream *stream, gint64 *start, gint64 *end)
{
	GBytes *bytes = ((TnefContentStream *) stream)->bytes;

	if (!bytes) {
		errno = EBADF;
		return FALSE;
	}

	g_mime_stream_get_bounds (stream, start, end);

	if (*end < 0 || (guint64) *end > g_bytes_get_size (bytes))
		*end = g_bytes_get_size (bytes);

	if (*start < 0 || *start > *end) {
		errno = EINVAL;
		return FALSE;
	}

	return TRUE;
}

static ssize_t
content_read (GMimeStream *stream, char *buffer, size_t count)
{
	gint64 start, end, pos = g_mime_stream_get_position (stream);

	if (!content_bounds (stream, &start, &end))
		return -1;

	if (pos < start || pos > end) {
		errno = EINVAL;
		return -1;
	}

	count = MIN (MIN (count, (gsize) G_MAXSSIZE), (gsize) (end - pos));

	if (count) {
		const guint8 *data = g_bytes_get_data (((TnefContentStream *) stream)->bytes, NULL);
		memcpy (buffer, data + pos, count);
		g_mime_stream_set_position (stream, pos + count);
	}

	return (ssize_t) count;
}

static ssize_t
content_write (GMimeStream *stream, const char *buffer, size_t count)
{
	errno = EBADF;
	return -1;
}

static gint64
content_seek (GMimeStream *stream, gint64 offset, GMimeSeekWhence whence)
{
	gint64 base, start, end;

	if (!content_bounds (stream, &start, &end))
		return -1;

	switch (whence) {
	case GMIME_STREAM_SEEK_SET: base = 0; break;
	case GMIME_STREAM_SEEK_CUR: base = g_mime_stream_get_position (stream); break;
	case GMIME_STREAM_SEEK_END: base = end; break;
	default: errno = EINVAL; return -1;
	}

	if (base < 0 || offset < start - base || offset > end - base) {
		errno = EINVAL;
		return -1;
	}

	g_mime_stream_set_position (stream, base + offset);

	return base + offset;
}

static GMimeStream *
content_substream (GMimeStream *stream, gint64 start, gint64 end)
{
	gint64 lower, upper;

	if (!content_bounds (stream, &lower, &upper))
		return NULL;

	if (end == -1)
		end = upper;

	if (start < lower || end > upper || start > end) {
		errno = EINVAL;
		return NULL;
	}

	return content_stream_new (((TnefContentStream *) stream)->bytes, start, end);
}

static int
content_close (GMimeStream *stream)
{
	g_clear_pointer (&((TnefContentStream *) stream)->bytes, g_bytes_unref);
	return 0;
}

static int
content_check_open (GMimeStream *stream)
{
	if (!((TnefContentStream *) stream)->bytes) {
		errno = EBADF;
		return -1;
	}

	return 0;
}

static gboolean
content_eos (GMimeStream *stream)
{
	gint64 start, end;

	return !content_bounds (stream, &start, &end) || g_mime_stream_get_position (stream) >= end;
}

static gint64
content_length (GMimeStream *stream)
{
	gint64 start, end;

	return content_bounds (stream, &start, &end) ? end - start : -1;
}

static void
content_finalize (GObject *object)
{
	content_close (GMIME_STREAM (object));

	G_OBJECT_CLASS (tnef_content_stream_parent_class)->finalize (object);
}

static void
tnef_content_stream_class_init (TnefContentStreamClass *klass)
{
	G_OBJECT_CLASS (klass)->finalize = content_finalize;

	klass->read = content_read;
	klass->write = content_write;
	klass->seek = content_seek;
	klass->reset = content_check_open;
	klass->flush = content_check_open;
	klass->length = content_length;
	klass->eos = content_eos;
	klass->close = content_close;
	klass->substream = content_substream;
}

static void
tnef_content_stream_init (TnefContentStream *stream)
{
}

/**
 * g_mime_tnef_attachment_open_read:
 * @attachment: an attachment
 *
 * Returns: (transfer full): an independent read-only stream (empty if no
 * content was materialized). Object interface prefixes are excluded.
 **/
GMimeStream *
g_mime_tnef_attachment_open_read (GMimeTnefAttachment *attachment)
{
	g_return_val_if_fail (GMIME_IS_TNEF_ATTACHMENT (attachment), NULL);

	return content_stream_new (attachment->content, 0, g_mime_tnef_attachment_get_length (attachment));
}

/**
 * g_mime_tnef_attachment_load_embedded_message:
 * @attachment: an embedded-message attachment
 * @error: return location for an error
 *
 * Parses the retained content afresh, sharing the enclosing reader's limits
 * and diagnostic callback. The enclosing stream need no longer exist.
 *
 * Returns: (transfer full) (nullable): the message, or %NULL on error
 **/
GMimeTnefMessage *
g_mime_tnef_attachment_load_embedded_message (GMimeTnefAttachment *attachment, GError **error)
{
	GMimeTnefMessage *message;
	GMimeTnefReader *reader;
	GMimeStream *stream;

	g_return_val_if_fail (GMIME_IS_TNEF_ATTACHMENT (attachment), NULL);
	g_return_val_if_fail (!error || !*error, NULL);

	if (!attachment->embedded) {
		_g_mime_tnef_invalid_operation (error, "The attachment is not an embedded message.");
		return NULL;
	}

	stream = g_mime_tnef_attachment_open_read (attachment);
	reader = _g_mime_tnef_replay_context_open (attachment->embedded, stream);
	message = g_mime_tnef_message_load_from_reader (reader, error);

	g_object_unref (reader);
	g_object_unref (stream);

	return message;
}

/**
 * g_mime_tnef_message_body_open_read:
 * @body: a message body
 *
 * Returns: (transfer full): an independent stream over the retained bytes
 **/
GMimeStream *
g_mime_tnef_message_body_open_read (GMimeTnefMessageBody *body)
{
	g_return_val_if_fail (GMIME_IS_TNEF_MESSAGE_BODY (body), NULL);

	return content_stream_new (body->content, 0, g_mime_tnef_message_body_get_length (body));
}

/**
 * g_mime_tnef_message_body_open_decoded_read:
 * @body: a message body
 *
 * Returns: (transfer full): a new read stream, decompressing RTF on demand.
 * No character-set conversion is performed.
 **/
GMimeStream *
g_mime_tnef_message_body_open_decoded_read (GMimeTnefMessageBody *body)
{
	GMimeStream *raw, *decoded;
	GMimeFilter *filter;

	g_return_val_if_fail (GMIME_IS_TNEF_MESSAGE_BODY (body), NULL);

	raw = g_mime_tnef_message_body_open_read (body);

	if (body->format != GMIME_TNEF_MESSAGE_BODY_FORMAT_COMPRESSED_RTF)
		return raw;

	decoded = g_mime_stream_filter_new (raw);
	filter = g_mime_filter_rtf_compressed_new ();
	g_mime_stream_filter_add (GMIME_STREAM_FILTER (decoded), filter);
	g_object_unref (filter);
	g_object_unref (raw);

	return decoded;
}

/**
 * g_mime_tnef_message_body_get_text:
 * @body: a message body
 * @error: return location for an error
 *
 * Returns: (transfer full) (nullable): newly allocated UTF-8 text. RTF is
 * decompressed, not rendered. Uses the message charset if none is declared.
 **/
char *
g_mime_tnef_message_body_get_text (GMimeTnefMessageBody *body, GError **error)
{
	const char *charset, *data;
	GBytes *bytes;
	gsize length;
	iconv_t cd;
	char *text;

	g_return_val_if_fail (GMIME_IS_TNEF_MESSAGE_BODY (body), NULL);
	g_return_val_if_fail (!error || !*error, NULL);

	if (body->format == GMIME_TNEF_MESSAGE_BODY_FORMAT_COMPRESSED_RTF) {
		GMimeStream *stream = g_mime_tnef_message_body_open_decoded_read (body);
		GByteArray *data = g_byte_array_new ();
		guint8 buffer[4096];
		ssize_t n;

		while ((n = g_mime_stream_read (stream, (char *) buffer, sizeof buffer)) > 0) {
			if ((gsize) n > G_MAXUINT - data->len) {
				errno = EOVERFLOW;
				n = -1;
				break;
			}

			g_byte_array_append (data, buffer, (guint) n);
		}

		if (n < 0)
			_g_mime_tnef_io_error (error);

		g_object_unref (stream);
		bytes = g_byte_array_free_to_bytes (data);

		if (n < 0) {
			g_bytes_unref (bytes);
			return NULL;
		}
	} else {
		bytes = g_bytes_ref (body->content);
	}

	charset = body->charset ? body->charset : body->fallback;
	data = g_bytes_get_data (bytes, &length);

	if (!length) {
		text = g_strdup ("");
	} else if ((cd = g_mime_iconv_open ("UTF-8", charset)) != (iconv_t) -1) {
		text = _g_mime_iconv_decode_with_fallback (data, length, cd, charset, NULL, error);
		g_mime_iconv_close (cd);

		/* Match StreamReader's treatment of the declared encoding's preamble
		 * without overriding the charset based on another encoding's BOM. */
		if (text && g_str_has_prefix (text, "\xef\xbb\xbf"))
			memmove (text, text + 3, strlen (text + 3) + 1);
	} else {
		g_set_error (error, G_CONVERT_ERROR, G_CONVERT_ERROR_NO_CONVERSION, "Cannot decode TNEF body charset '%s'.", charset);
		text = NULL;
	}

	g_bytes_unref (bytes);

	return text;
}

/**
 * g_mime_tnef_message_get_codepage:
 * @message: a loaded message
 *
 * Returns: the effective message codepage
 **/
int
g_mime_tnef_message_get_codepage (GMimeTnefMessage *message)
{
	g_return_val_if_fail (GMIME_IS_TNEF_MESSAGE (message), 0);

	return message->codepage;
}

/**
 * g_mime_tnef_message_get_legacy_key:
 * @message: a loaded message
 *
 * Returns: the legacy attachment key from the TNEF header
 **/
guint16
g_mime_tnef_message_get_legacy_key (GMimeTnefMessage *message)
{
	g_return_val_if_fail (GMIME_IS_TNEF_MESSAGE (message), 0);

	return message->legacy_key;
}

/**
 * g_mime_tnef_message_get_properties:
 * @message: a loaded message
 *
 * Returns: (transfer none): retained message properties, excluding streamed bodies
 **/
GMimeTnefPropertySet *
g_mime_tnef_message_get_properties (GMimeTnefMessage *message)
{
	g_return_val_if_fail (GMIME_IS_TNEF_MESSAGE (message), NULL);

	return message->properties;
}

/**
 * g_mime_tnef_message_get_subject:
 * @message: a loaded message
 *
 * Returns: (nullable): borrowed UTF-8 subject
 **/
const char *
g_mime_tnef_message_get_subject (GMimeTnefMessage *message)
{
	g_return_val_if_fail (GMIME_IS_TNEF_MESSAGE (message), NULL);

	return _g_mime_tnef_set_string (message->properties, GMIME_TNEF_PROPERTY_TAG_SUBJECT_W);
}

/**
 * g_mime_tnef_message_get_message_class:
 * @message: a loaded message
 *
 * Returns: (nullable): trimmed, borrowed message class, or %NULL if empty
 **/
const char *
g_mime_tnef_message_get_message_class (GMimeTnefMessage *message)
{
	g_return_val_if_fail (GMIME_IS_TNEF_MESSAGE (message), NULL);

	return message->message_class;
}

/**
 * g_mime_tnef_message_get_internet_message_id:
 * @message: a loaded message
 *
 * Returns: (nullable): borrowed Internet message identifier
 **/
const char *
g_mime_tnef_message_get_internet_message_id (GMimeTnefMessage *message)
{
	g_return_val_if_fail (GMIME_IS_TNEF_MESSAGE (message), NULL);

	return _g_mime_tnef_set_string (message->properties, GMIME_TNEF_PROPERTY_TAG_INTERNET_MESSAGE_ID_W);
}

/**
 * g_mime_tnef_message_get_recipient_count:
 * @message: a loaded message
 *
 * Returns: number of recipient rows
 **/
guint
g_mime_tnef_message_get_recipient_count (GMimeTnefMessage *message)
{
	g_return_val_if_fail (GMIME_IS_TNEF_MESSAGE (message), 0);

	return message->recipients->len;
}

/**
 * g_mime_tnef_message_get_recipient:
 * @message: a loaded message
 * @index: zero-based recipient index
 *
 * Returns: (transfer none) (nullable): recipient at @index, or %NULL if out of range
 **/
GMimeTnefRecipient *
g_mime_tnef_message_get_recipient (GMimeTnefMessage *message, guint index)
{
	g_return_val_if_fail (GMIME_IS_TNEF_MESSAGE (message), NULL);

	return index < message->recipients->len ? g_ptr_array_index (message->recipients, index) : NULL;
}

/**
 * g_mime_tnef_message_get_attachment_count:
 * @message: a loaded message
 *
 * Returns: number of materialized attachments
 **/
guint
g_mime_tnef_message_get_attachment_count (GMimeTnefMessage *message)
{
	g_return_val_if_fail (GMIME_IS_TNEF_MESSAGE (message), 0);

	return message->attachments->len;
}

/**
 * g_mime_tnef_message_get_attachment:
 * @message: a loaded message
 * @index: zero-based attachment index
 *
 * Returns: (transfer none) (nullable): attachment at @index, or %NULL if out of range
 **/
GMimeTnefAttachment *
g_mime_tnef_message_get_attachment (GMimeTnefMessage *message, guint index)
{
	g_return_val_if_fail (GMIME_IS_TNEF_MESSAGE (message), NULL);

	return index < message->attachments->len ? g_ptr_array_index (message->attachments, index) : NULL;
}

/**
 * g_mime_tnef_message_get_text_body:
 * @message: a loaded message
 *
 * Returns: (transfer none) (nullable): plain text representation
 **/
GMimeTnefMessageBody *
g_mime_tnef_message_get_text_body (GMimeTnefMessage *message)
{
	g_return_val_if_fail (GMIME_IS_TNEF_MESSAGE (message), NULL);

	return message->bodies[GMIME_TNEF_MESSAGE_BODY_FORMAT_TEXT];
}

/**
 * g_mime_tnef_message_get_html_body:
 * @message: a loaded message
 *
 * Returns: (transfer none) (nullable): HTML representation
 **/
GMimeTnefMessageBody *
g_mime_tnef_message_get_html_body (GMimeTnefMessage *message)
{
	g_return_val_if_fail (GMIME_IS_TNEF_MESSAGE (message), NULL);

	return message->bodies[GMIME_TNEF_MESSAGE_BODY_FORMAT_HTML];
}

/**
 * g_mime_tnef_message_get_rtf_body:
 * @message: a loaded message
 *
 * Returns: (transfer none) (nullable): compressed RTF representation
 **/
GMimeTnefMessageBody *
g_mime_tnef_message_get_rtf_body (GMimeTnefMessage *message)
{
	g_return_val_if_fail (GMIME_IS_TNEF_MESSAGE (message), NULL);

	return message->bodies[GMIME_TNEF_MESSAGE_BODY_FORMAT_COMPRESSED_RTF];
}

/**
 * g_mime_tnef_recipient_get_properties:
 * @recipient: a recipient row
 *
 * Returns: (transfer none): the complete recipient property set
 **/
GMimeTnefPropertySet *
g_mime_tnef_recipient_get_properties (GMimeTnefRecipient *recipient)
{
	g_return_val_if_fail (GMIME_IS_TNEF_RECIPIENT (recipient), NULL);

	return recipient->properties;
}

/**
 * g_mime_tnef_recipient_get_recipient_type:
 * @recipient: a recipient row
 *
 * Returns: recipient type, defaulting to To
 **/
GMimeTnefRecipientType
g_mime_tnef_recipient_get_recipient_type (GMimeTnefRecipient *recipient)
{
	g_return_val_if_fail (GMIME_IS_TNEF_RECIPIENT (recipient), GMIME_TNEF_RECIPIENT_TYPE_TO);

	/* The high bits can mark a resent recipient; they are not part of
	 * the recipient type ([MS-OXOMSG] 2.2.3.1). */
	return _g_mime_tnef_set_int (recipient->properties, GMIME_TNEF_PROPERTY_TAG_RECIPIENT_TYPE, GMIME_TNEF_RECIPIENT_TYPE_TO) & 0x0fffffff;
}

/**
 * g_mime_tnef_recipient_get_display_name:
 * @recipient: a recipient row
 *
 * Returns: (nullable): borrowed recipient name, falling back to transmittable and display name
 **/
const char *
g_mime_tnef_recipient_get_display_name (GMimeTnefRecipient *recipient)
{
	const char *name;

	g_return_val_if_fail (GMIME_IS_TNEF_RECIPIENT (recipient), NULL);

	name = _g_mime_tnef_set_string (recipient->properties, GMIME_TNEF_PROPERTY_TAG_RECIPIENT_DISPLAY_NAME_W);

	if (!name)
		name = _g_mime_tnef_set_string (recipient->properties, GMIME_TNEF_PROPERTY_TAG_TRANSMITABLE_DISPLAY_NAME_W);

	return name ? name : _g_mime_tnef_set_string (recipient->properties, GMIME_TNEF_PROPERTY_TAG_DISPLAY_NAME_W);
}

/**
 * g_mime_tnef_recipient_get_email_address:
 * @recipient: a recipient row
 *
 * Returns: (nullable): borrowed SMTP address or, if absent, the native email address
 **/
const char *
g_mime_tnef_recipient_get_email_address (GMimeTnefRecipient *recipient)
{
	const char *address;

	g_return_val_if_fail (GMIME_IS_TNEF_RECIPIENT (recipient), NULL);

	address = _g_mime_tnef_set_string (recipient->properties, GMIME_TNEF_PROPERTY_TAG_SMTP_ADDRESS_W);

	return address ? address : _g_mime_tnef_set_string (recipient->properties, GMIME_TNEF_PROPERTY_TAG_EMAIL_ADDRESS_W);
}

/**
 * g_mime_tnef_recipient_get_address_type:
 * @recipient: a recipient row
 *
 * Returns: (nullable): borrowed native address type
 **/
const char *
g_mime_tnef_recipient_get_address_type (GMimeTnefRecipient *recipient)
{
	g_return_val_if_fail (GMIME_IS_TNEF_RECIPIENT (recipient), NULL);

	return _g_mime_tnef_set_string (recipient->properties, GMIME_TNEF_PROPERTY_TAG_ADDRTYPE_W);
}

/**
 * g_mime_tnef_message_body_get_format:
 * @body: a message body
 *
 * Returns: the representation of @body
 **/
GMimeTnefMessageBodyFormat
g_mime_tnef_message_body_get_format (GMimeTnefMessageBody *body)
{
	g_return_val_if_fail (GMIME_IS_TNEF_MESSAGE_BODY (body), GMIME_TNEF_MESSAGE_BODY_FORMAT_TEXT);

	return body->format;
}

/**
 * g_mime_tnef_message_body_get_tag:
 * @body: a message body
 *
 * Returns: originating MAPI property tag (BodyA for a legacy body)
 **/
GMimeTnefPropertyTag
g_mime_tnef_message_body_get_tag (GMimeTnefMessageBody *body)
{
	g_return_val_if_fail (GMIME_IS_TNEF_MESSAGE_BODY (body), 0);

	return body->tag;
}

/**
 * g_mime_tnef_message_body_get_charset:
 * @body: a message body
 *
 * Returns: (nullable): borrowed character-set name, or %NULL for undeclared binary content
 **/
const char *
g_mime_tnef_message_body_get_charset (GMimeTnefMessageBody *body)
{
	g_return_val_if_fail (GMIME_IS_TNEF_MESSAGE_BODY (body), NULL);

	return body->charset;
}

/**
 * g_mime_tnef_message_body_get_length:
 * @body: a message body
 *
 * Returns: the byte length before decompression, excluding string terminators
 **/
gint64
g_mime_tnef_message_body_get_length (GMimeTnefMessageBody *body)
{
	g_return_val_if_fail (GMIME_IS_TNEF_MESSAGE_BODY (body), 0);

	return body->content ? g_bytes_get_size (body->content) : 0;
}

/**
 * g_mime_tnef_attachment_get_properties:
 * @attachment: a message attachment
 *
 * Returns: (transfer none): attachment properties other than the streamed data
 **/
GMimeTnefPropertySet *
g_mime_tnef_attachment_get_properties (GMimeTnefAttachment *attachment)
{
	g_return_val_if_fail (GMIME_IS_TNEF_ATTACHMENT (attachment), NULL);

	return attachment->properties;
}

/**
 * g_mime_tnef_attachment_get_method:
 * @attachment: a message attachment
 *
 * Returns: attachment method, inferred from the object IID if unspecified
 **/
GMimeTnefAttachMethod
g_mime_tnef_attachment_get_method (GMimeTnefAttachment *attachment)
{
	g_return_val_if_fail (GMIME_IS_TNEF_ATTACHMENT (attachment), GMIME_TNEF_ATTACH_METHOD_NONE);

	return _g_mime_tnef_set_int (attachment->properties, GMIME_TNEF_PROPERTY_TAG_ATTACH_METHOD,
				     attachment->embedded ? GMIME_TNEF_ATTACH_METHOD_EMBEDDED_MESSAGE : GMIME_TNEF_ATTACH_METHOD_BY_VALUE);
}

/**
 * g_mime_tnef_attachment_get_flags:
 * @attachment: a message attachment
 *
 * Returns: MAPI attachment display flags
 **/
GMimeTnefAttachFlags
g_mime_tnef_attachment_get_flags (GMimeTnefAttachment *attachment)
{
	g_return_val_if_fail (GMIME_IS_TNEF_ATTACHMENT (attachment), GMIME_TNEF_ATTACH_FLAGS_NONE);

	return _g_mime_tnef_set_int (attachment->properties, GMIME_TNEF_PROPERTY_TAG_ATTACH_FLAGS, 0);
}

/**
 * g_mime_tnef_attachment_get_file_name:
 * @attachment: a message attachment
 *
 * Returns: (nullable): borrowed long filename, falling back to the short filename
 **/
const char *
g_mime_tnef_attachment_get_file_name (GMimeTnefAttachment *attachment)
{
	const char *name;

	g_return_val_if_fail (GMIME_IS_TNEF_ATTACHMENT (attachment), NULL);

	name = _g_mime_tnef_set_string (attachment->properties, GMIME_TNEF_PROPERTY_TAG_ATTACH_LONG_FILENAME_W);

	return name ? name : _g_mime_tnef_set_string (attachment->properties, GMIME_TNEF_PROPERTY_TAG_ATTACH_FILENAME_W);
}

/**
 * g_mime_tnef_attachment_get_mime_type:
 * @attachment: a message attachment
 *
 * Returns: (nullable): borrowed MIME type supplied by the sender
 **/
const char *
g_mime_tnef_attachment_get_mime_type (GMimeTnefAttachment *attachment)
{
	g_return_val_if_fail (GMIME_IS_TNEF_ATTACHMENT (attachment), NULL);

	return _g_mime_tnef_set_string (attachment->properties, GMIME_TNEF_PROPERTY_TAG_ATTACH_MIME_TAG_W);
}

/**
 * g_mime_tnef_attachment_get_content_id:
 * @attachment: a message attachment
 *
 * Returns: (nullable): borrowed Content-ID
 **/
const char *
g_mime_tnef_attachment_get_content_id (GMimeTnefAttachment *attachment)
{
	g_return_val_if_fail (GMIME_IS_TNEF_ATTACHMENT (attachment), NULL);

	return _g_mime_tnef_set_string (attachment->properties, GMIME_TNEF_PROPERTY_TAG_ATTACH_CONTENT_ID_W);
}

/**
 * g_mime_tnef_attachment_has_content:
 * @attachment: a message attachment
 *
 * Returns: whether a payload was retained (which may be empty)
 **/
gboolean
g_mime_tnef_attachment_has_content (GMimeTnefAttachment *attachment)
{
	g_return_val_if_fail (GMIME_IS_TNEF_ATTACHMENT (attachment), FALSE);

	return attachment->content != NULL;
}

/**
 * g_mime_tnef_attachment_is_embedded_message:
 * @attachment: a message attachment
 *
 * Returns: whether this attachment can be replayed as a TNEF message
 **/
gboolean
g_mime_tnef_attachment_is_embedded_message (GMimeTnefAttachment *attachment)
{
	g_return_val_if_fail (GMIME_IS_TNEF_ATTACHMENT (attachment), FALSE);

	return attachment->embedded != NULL;
}

/**
 * g_mime_tnef_attachment_get_length:
 * @attachment: a message attachment
 *
 * Returns: payload length excluding the object's interface prefix
 **/
gint64
g_mime_tnef_attachment_get_length (GMimeTnefAttachment *attachment)
{
	g_return_val_if_fail (GMIME_IS_TNEF_ATTACHMENT (attachment), 0);

	return attachment->content ? g_bytes_get_size (attachment->content) : 0;
}
