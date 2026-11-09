/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*  GMime
 *  Copyright (C) 2026 Jeffrey Stedfast
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

#include <errno.h>
#include <math.h>
#include <string.h>

#include "gmime-error.h"
#include "gmime-stream-block.h"
#include "gmime-tnef-private.h"
#include "gmime-tnef-writer.h"

/**
 * SECTION: gmime-tnef-writer
 * @title: TNEF writers
 * @short_description: TNEF attributes, MAPI properties and embedded messages
 *
 * Writers generate the signature, version, codepage, lengths and checksums.
 * Message attributes precede attachments. MapiProperties ends the message
 * attributes; AttachRenderData starts each attachment and Attachment ends
 * its attributes. Do not write automatically generated header attributes.
 *
 * Property lists and streamed values use seekable temporary storage, allowing
 * the final output to be forward-only. A custom factory can use files instead
 * of the default block-backed memory. Embedded writers inherit the factory.
 *
 * Close value streams, embedded writers and property writers before continuing
 * their parent. Finally call g_mime_tnef_writer_close() and check its result.
 * Finalizers never complete output: abandoning an unfinished child poisons its
 * parent instead of silently publishing an incomplete attribute. The root
 * output stream remains open and owned by its caller.
 *
 * g_mime_tnef_property_writer_open_rtf_compressed_stream() accepts ordinary RTF
 * bytes and writes either LZFu or an uncompressed wrapper. Its explicit close
 * patches the RTF header and CRC before completing the surrounding value.
 * No RTF-to-text/HTML or high-level TNEF-to-MIME conversion is performed.
 **/

typedef enum {
	WRITER_MESSAGE,
	WRITER_MESSAGE_PROPERTIES,
	WRITER_ATTACHMENT,
	WRITER_ATTACHMENT_PROPERTIES
} WriterState;

typedef struct {
	gint ref_count;
	GMimeTnefTemporaryStreamFactory callback;
	gpointer user_data;
	GDestroyNotify destroy;
} TemporaryStreamFactory;

struct _GMimeTnefWriter {
	GObject parent_instance;
	GMimeStream *stream, *pending;
	GObject *child;
	TemporaryStreamFactory *factory;
	GHashTable *names;
	GError *fault;
	char *charset;
	int codepage;
	guint16 legacy_key;
	guint next_name_id;
	GMimeTnefAttributeTag pending_tag;
	GMimeTnefAttributeLevel pending_level;
	WriterState state;
	gboolean header_written, closed, close_output;
};

struct _GMimeTnefPropertyWriter {
	GObject parent_instance;
	GMimeTnefWriter *writer;
	GMimeStream *buffer;
	GObject *child;
	GError *fault;
	GMimeTnefPropertyTag tag;
	gboolean is_table, has_property, closed;
	gint64 count_offset, value_count_offset, value_length_offset;
	int row_count, property_count, value_count;
};

typedef struct {
	GMimeStream parent_instance;
	GMimeStream *output;
	GObject *owner;
	GMimeTnefStreamClosedFunc completed;
	gboolean closed;
	GError *fault;
} TnefWriteStream;

typedef struct {
	GMimeStreamClass parent_class;
} TnefWriteStreamClass;

static GType tnef_write_stream_get_type (void);
static gboolean property_check (GMimeTnefPropertyWriter *writer, GError **error);
static gboolean property_end_row (GMimeTnefPropertyWriter *writer, GError **error);
static gboolean property_value_completed (GObject *owner, GMimeStream *stream, const GError *fault, GError **error);

G_DEFINE_TYPE (GMimeTnefWriter, g_mime_tnef_writer, G_TYPE_OBJECT)
G_DEFINE_TYPE (GMimeTnefPropertyWriter, g_mime_tnef_property_writer, G_TYPE_OBJECT)
G_DEFINE_TYPE (TnefWriteStream, tnef_write_stream, GMIME_TYPE_STREAM)

static gboolean
propagate_fault (const GError *fault, GError **error)
{
	g_propagate_error (error, g_error_copy (fault));
	return FALSE;
}

static void
factory_unref (TemporaryStreamFactory *factory)
{
	if (factory && g_atomic_int_dec_and_test (&factory->ref_count)) {
		if (factory->destroy)
			factory->destroy (factory->user_data);
		g_free (factory);
	}
}

static void
g_mime_tnef_writer_finalize (GObject *object)
{
	GMimeTnefWriter *writer = GMIME_TNEF_WRITER (object);

	/* Completion must be explicit: finalizers cannot report failed output. */
	g_clear_object (&writer->pending);
	g_clear_object (&writer->stream);
	g_clear_error (&writer->fault);
	g_clear_pointer (&writer->names, g_hash_table_unref);
	factory_unref (writer->factory);
	g_free (writer->charset);

	G_OBJECT_CLASS (g_mime_tnef_writer_parent_class)->finalize (object);
}

static void
g_mime_tnef_writer_class_init (GMimeTnefWriterClass *klass)
{
	G_OBJECT_CLASS (klass)->finalize = g_mime_tnef_writer_finalize;
}

static void
g_mime_tnef_writer_init (GMimeTnefWriter *writer)
{
	writer->next_name_id = 0x8000;
	writer->names =
		g_hash_table_new_full ((GHashFunc) g_mime_tnef_name_id_hash, (GEqualFunc) g_mime_tnef_name_id_equal,
				       (GDestroyNotify) g_mime_tnef_name_id_unref, NULL);
}

/**
 * g_mime_tnef_writer_new:
 * @stream: the output stream
 * @codepage: the 8-bit string codepage, usually 1252
 * @legacy_key: the stream's legacy key, usually zero
 * @error: return location for an error
 *
 * Creates a writer without writing any output. The stream is referenced,
 * not closed. Unsupported codepages and UTF-16/UTF-32 are rejected.
 * Call g_mime_tnef_writer_close() to complete output before unref.
 *
 * Returns: (transfer full) (nullable): a writer, or %NULL on error
 **/
GMimeTnefWriter *
g_mime_tnef_writer_new (GMimeStream *stream, int codepage, guint16 legacy_key, GError **error)
{
	GMimeTnefWriter *writer;
	char *charset;

	g_return_val_if_fail (GMIME_IS_STREAM (stream), NULL);
	g_return_val_if_fail (error == NULL || *error == NULL, NULL);

	if (codepage <= 0 || codepage == 1200 || codepage == 1201 || codepage == 12000 || codepage == 12001 ||
	    !(charset = _g_mime_tnef_get_charset (codepage))) {
		g_set_error_literal (error, GMIME_ERROR, GMIME_ERROR_NOT_SUPPORTED,
				     "The codepage is unsupported or encodes characters using embedded zero bytes.");
		return NULL;
	}

	writer = g_object_new (GMIME_TYPE_TNEF_WRITER, NULL);
	writer->stream = g_object_ref (stream);
	writer->codepage = codepage;
	writer->legacy_key = legacy_key;
	writer->charset = charset;

	return writer;
}

static gboolean
writer_check (GMimeTnefWriter *writer, GError **error)
{
	if (writer->fault)
		return propagate_fault (writer->fault, error);

	if (writer->closed)
		return _g_mime_tnef_invalid_operation (error, "The TNEF writer is closed.");

	if (writer->child)
		return _g_mime_tnef_invalid_operation (error,
						       "Close the previous attribute's stream or property writer first.");

	return TRUE;
}

/**
 * g_mime_tnef_writer_set_temporary_stream_factory:
 * @writer: a TNEF writer
 * @factory: (scope notified) (closure user_data) (destroy destroy) (nullable): stream factory
 * @user_data: (nullable): callback data
 * @destroy: (nullable): callback-data destroy function
 *
 * Configures temporary attribute storage. %NULL restores block-backed
 * memory streams. Each embedded writer retains the factory current when
 * it is created, including ownership of the callback data.
 **/
void
g_mime_tnef_writer_set_temporary_stream_factory (GMimeTnefWriter *writer, GMimeTnefTemporaryStreamFactory factory,
						 gpointer user_data, GDestroyNotify destroy)
{
	TemporaryStreamFactory *replacement;

	g_return_if_fail (GMIME_IS_TNEF_WRITER (writer));
	g_return_if_fail (!writer->closed);

	replacement = g_new0 (TemporaryStreamFactory, 1);
	replacement->ref_count = 1;
	replacement->callback = factory;
	replacement->user_data = user_data;
	replacement->destroy = destroy;
	factory_unref (writer->factory);
	writer->factory = replacement;
}

static GMimeStream *
create_temporary_stream (GMimeTnefWriter *writer, GError **error)
{
	GMimeStream *stream;
	GMimeStreamClass *klass, *base_class;
	char byte;

	if (!writer->factory || !writer->factory->callback)
		return g_mime_stream_block_new ();

	stream = writer->factory->callback (writer->factory->user_data, error);

	if (!stream) {
		if (!error || !*error)
			g_set_error_literal (error, GMIME_ERROR, GMIME_ERROR_INVALID_OPERATION,
					     "The temporary stream factory returned NULL without an error.");
		return NULL;
	}

	if (!GMIME_IS_STREAM (stream)) {
		g_set_error_literal (error, GMIME_ERROR, GMIME_ERROR_INVALID_OPERATION,
				     "The factory did not return a GMimeStream.");
		g_object_unref (stream);
		return NULL;
	}

	klass = GMIME_STREAM_GET_CLASS (stream);
	base_class = g_type_class_peek (GMIME_TYPE_STREAM);

	if (stream == writer->stream || stream == writer->pending || klass->read == base_class->read ||
	    klass->write == base_class->write || g_mime_stream_tell (stream) != 0 ||
	    g_mime_stream_length (stream) != 0 || g_mime_stream_seek (stream, 0, GMIME_STREAM_SEEK_SET) != 0 ||
	    klass->write (stream, "", 0) != 0 || g_mime_stream_read (stream, &byte, 1) != 0 ||
	    g_mime_stream_seek (stream, 0, GMIME_STREAM_SEEK_SET) != 0) {
		g_set_error_literal (error, GMIME_ERROR, GMIME_ERROR_INVALID_OPERATION,
				     "The factory must return a new empty readable, writable and seekable stream at position zero.");
		g_object_unref (stream);
		return NULL;
	}

	return stream;
}

static gboolean
validate_attribute (GMimeTnefWriter *writer, GMimeTnefAttributeTag tag, WriterState *next,
		    GMimeTnefAttributeLevel *level, GError **error)
{
	int expected = _g_mime_tnef_get_attribute_level (tag);

	if (!expected || tag == GMIME_TNEF_ATTRIBUTE_TAG_TNEF_VERSION || tag == GMIME_TNEF_ATTRIBUTE_TAG_OEM_CODEPAGE)
		return _g_mime_tnef_invalid_operation (error, "The attribute is unknown or written automatically.");

	*level = expected;
	*next = writer->state;

	if (tag == GMIME_TNEF_ATTRIBUTE_TAG_MAPI_PROPERTIES) {
		if (writer->state != WRITER_MESSAGE)
			return _g_mime_tnef_invalid_operation (error,
							       "MapiProperties must be the last message attribute and may occur only once.");
		*next = WRITER_MESSAGE_PROPERTIES;
	} else if (tag == GMIME_TNEF_ATTRIBUTE_TAG_ATTACH_RENDER_DATA) {
		/* attAttachRenderData begins a new attachment. */
		*next = WRITER_ATTACHMENT;
	} else if (tag == GMIME_TNEF_ATTRIBUTE_TAG_ATTACHMENT) {
		if (writer->state != WRITER_ATTACHMENT)
			return _g_mime_tnef_invalid_operation (error,
							       "Attachment must follow AttachRenderData, once per attachment.");
		*next = WRITER_ATTACHMENT_PROPERTIES;
	} else if (expected == GMIME_TNEF_ATTRIBUTE_LEVEL_MESSAGE) {
		if (writer->state != WRITER_MESSAGE)
			return _g_mime_tnef_invalid_operation (error,
							       "Message attributes must precede MapiProperties and attachments.");
	} else if (writer->state != WRITER_ATTACHMENT) {
		return _g_mime_tnef_invalid_operation (error,
						       "Attachment attributes must follow AttachRenderData and precede Attachment.");
	}

	return TRUE;
}

static gboolean
write_output (GMimeTnefWriter *writer, const guint8 *buffer, gsize length, GError **error)
{
	if (!_g_mime_tnef_write_all (writer->stream, buffer, length, &writer->fault))
		return propagate_fault (writer->fault, error);

	return TRUE;
}

static gboolean
write_attribute_header (GMimeTnefWriter *writer, GMimeTnefAttributeLevel level, GMimeTnefAttributeTag tag,
			guint32 length, GError **error)
{
	guint8 bytes[9];

	bytes[0] = level;
	_g_mime_tnef_put_uint32 (bytes + 1, tag);
	_g_mime_tnef_put_uint32 (bytes + 5, length);

	return write_output (writer, bytes, 9, error);
}

static gboolean
write_attribute_value (GMimeTnefWriter *writer, GMimeTnefAttributeLevel level, GMimeTnefAttributeTag tag,
		       const guint8 *buffer, gsize length, GError **error)
{
	guint8 checksum[2];

	_g_mime_tnef_put_uint16 (checksum, g_mime_tnef_checksum_update (0, buffer, length));

	return write_attribute_header (writer, level, tag, length, error) &&
	       write_output (writer, buffer, length, error) && write_output (writer, checksum, 2, error);
}

static gboolean
write_header (GMimeTnefWriter *writer, GError **error)
{
	guint8 bytes[8] = { 0 };

	if (writer->header_written)
		return TRUE;

	/* [MS-OXTNEF] 2.1.3.1: TNEFStream = TNEFHeader TNEFVersion OEMCodePage MessageData *AttachData. */
	_g_mime_tnef_put_uint32 (bytes, 0x223e9f78);
	_g_mime_tnef_put_uint16 (bytes + 4, writer->legacy_key);

	if (!write_output (writer, bytes, 6, error))
		return FALSE;

	_g_mime_tnef_put_uint32 (bytes, 0x00010000);

	if (!write_attribute_value (writer, GMIME_TNEF_ATTRIBUTE_LEVEL_MESSAGE, GMIME_TNEF_ATTRIBUTE_TAG_TNEF_VERSION,
				    bytes, 4, error))
		return FALSE;

	_g_mime_tnef_put_uint32 (bytes, writer->codepage);

	/* The secondary codepage is unused and SHOULD be zero. */
	_g_mime_tnef_put_uint32 (bytes + 4, 0);
	if (!write_attribute_value (writer, GMIME_TNEF_ATTRIBUTE_LEVEL_MESSAGE, GMIME_TNEF_ATTRIBUTE_TAG_OEM_CODEPAGE,
				    bytes, 8, error))
		return FALSE;

	writer->header_written = TRUE;

	return TRUE;
}

static gboolean
drain_pending (GMimeTnefWriter *writer, GError **error)
{
	guint8 buffer[4096], checksum_bytes[2];
	gint64 length, remaining;
	guint16 checksum = 0;
	ssize_t count;

	if (!write_header (writer, error))
		return FALSE;

	if (!writer->pending)
		return TRUE;

	length = g_mime_stream_length (writer->pending);

	if (length < 0 || length > G_MAXINT32 || g_mime_stream_seek (writer->pending, 0, GMIME_STREAM_SEEK_SET) != 0) {
		if (length > G_MAXINT32)
			errno = EOVERFLOW;

		_g_mime_tnef_io_error (&writer->fault);

		return propagate_fault (writer->fault, error);
	}

	if (!write_attribute_header (writer, writer->pending_level, writer->pending_tag, length, error))
		return FALSE;

	remaining = length;

	while (remaining) {
		count = g_mime_stream_read (writer->pending, (char *) buffer, MIN (remaining, (gint64) sizeof buffer));

		if (count <= 0) {
			if (!count)
				errno = EIO;

			_g_mime_tnef_io_error (&writer->fault);

			return propagate_fault (writer->fault, error);
		}

		checksum = g_mime_tnef_checksum_update (checksum, buffer, count);

		if (!write_output (writer, buffer, count, error))
			return FALSE;

		remaining -= count;
	}

	_g_mime_tnef_put_uint16 (checksum_bytes, checksum);

	if (!write_output (writer, checksum_bytes, 2, error))
		return FALSE;

	if (g_mime_stream_close (writer->pending) == -1) {
		_g_mime_tnef_io_error (&writer->fault);
		return propagate_fault (writer->fault, error);
	}

	g_clear_object (&writer->pending);

	return TRUE;
}

/**
 * g_mime_tnef_writer_write_attribute_bytes:
 * @writer: a TNEF writer
 * @tag: the attribute tag
 * @buffer: (array length=length) (nullable): raw value bytes
 * @length: byte count, no greater than G_MAXINT32
 * @error: return location for an error
 *
 * Writes an attribute's raw value, generating its level, length and checksum.
 *
 * Returns: whether the operation succeeded
 **/
gboolean
g_mime_tnef_writer_write_attribute_bytes (GMimeTnefWriter *writer, GMimeTnefAttributeTag tag, const guint8 *buffer,
					  gsize length, GError **error)
{
	WriterState next;
	GMimeTnefAttributeLevel level;

	g_return_val_if_fail (GMIME_IS_TNEF_WRITER (writer), FALSE);
	g_return_val_if_fail (buffer != NULL || length == 0, FALSE);
	g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

	if (length > G_MAXINT32)
		return _g_mime_tnef_invalid_operation (error, "The attribute value exceeds the 32-bit length limit.");

	if (!writer_check (writer, error) || !validate_attribute (writer, tag, &next, &level, error) ||
	    !drain_pending (writer, error))
		return FALSE;

	writer->state = next;

	return write_attribute_value (writer, level, tag, buffer, length, error);
}

/**
 * g_mime_tnef_writer_write_attribute_int32:
 * @writer: a TNEF writer
 * @tag: the attribute tag
 * @value: the integer value
 * @error: return location for an error
 *
 * Writes a Short, Word, Long or DWord attribute, checking 16-bit ranges.
 *
 * Returns: whether the operation succeeded
 **/
gboolean
g_mime_tnef_writer_write_attribute_int32 (GMimeTnefWriter *writer, GMimeTnefAttributeTag tag, gint32 value,
					  GError **error)
{
	guint8 bytes[4];
	gsize length;

	g_return_val_if_fail (GMIME_IS_TNEF_WRITER (writer), FALSE);
	g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

	switch (tag & 0xffff0000) {
	case GMIME_TNEF_ATTRIBUTE_TYPE_SHORT:
		if (value < G_MININT16 || value > G_MAXINT16)
			return _g_mime_tnef_invalid_operation (error,
							       "The value does not fit a signed 16-bit attribute.");
		length = 2;
		break;
	case GMIME_TNEF_ATTRIBUTE_TYPE_WORD:
		if (tag == GMIME_TNEF_ATTRIBUTE_TAG_MESSAGE_CLASS ||
		    tag == GMIME_TNEF_ATTRIBUTE_TAG_ORIGINAL_MESSAGE_CLASS)
			return _g_mime_tnef_invalid_operation (error, "MessageClass is a string attribute.");

		if (value < 0 || value > G_MAXUINT16)
			return _g_mime_tnef_invalid_operation (error,
							       "The value does not fit an unsigned 16-bit attribute.");
		length = 2;
		break;
	case GMIME_TNEF_ATTRIBUTE_TYPE_LONG:
	case GMIME_TNEF_ATTRIBUTE_TYPE_D_WORD:
		length = 4;
		break;
	default:
		return _g_mime_tnef_invalid_operation (error, "The attribute is not an integer.");
	}

	_g_mime_tnef_put_uint32 (bytes, (guint32) value);

	return g_mime_tnef_writer_write_attribute_bytes (writer, tag, bytes, length, error);
}

/**
 * g_mime_tnef_writer_write_attribute_string:
 * @writer: a TNEF writer
 * @tag: the attribute tag
 * @value: a UTF-8 string
 * @error: return location for an error
 *
 * Writes a nul-terminated String, Text or message-class attribute using the
 * selected codepage. Message classes require 1 to 254 printable ASCII characters.
 *
 * Returns: whether the operation succeeded
 **/
gboolean
g_mime_tnef_writer_write_attribute_string (GMimeTnefWriter *writer, GMimeTnefAttributeTag tag, const char *value,
					   GError **error)
{
	GMimeTnefAttributeType type = tag & 0xffff0000;
	gboolean message_class, success;
	const guint8 *data;
	const char *cursor;
	GBytes *bytes;
	gsize length;

	g_return_val_if_fail (GMIME_IS_TNEF_WRITER (writer), FALSE);
	g_return_val_if_fail (value != NULL, FALSE);
	g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

	message_class =
		tag == GMIME_TNEF_ATTRIBUTE_TAG_MESSAGE_CLASS || tag == GMIME_TNEF_ATTRIBUTE_TAG_ORIGINAL_MESSAGE_CLASS;

	if (!message_class && type != GMIME_TNEF_ATTRIBUTE_TYPE_STRING && type != GMIME_TNEF_ATTRIBUTE_TYPE_TEXT)
		return _g_mime_tnef_invalid_operation (error, "The attribute is not a string.");

	if (message_class) {
		/* [MS-OXOMSG] restricts message classes to printable ASCII. */
		if (!*value || strlen (value) > 254)
			return _g_mime_tnef_invalid_operation (error,
							       "A message class must contain 1 to 254 printable ASCII characters.");

		for (cursor = value; *cursor; cursor++) {
			if ((guchar) *cursor < 0x20 || (guchar) *cursor > 0x7e)
				return _g_mime_tnef_invalid_operation (error,
								       "A message class must contain only printable ASCII.");
		}
	}

	if (!(bytes = _g_mime_tnef_encode_string (writer->charset, value, FALSE, error)))
		return FALSE;

	data = g_bytes_get_data (bytes, &length);
	success = g_mime_tnef_writer_write_attribute_bytes (writer, tag, data, length, error);
	g_bytes_unref (bytes);

	return success;
}

/**
 * g_mime_tnef_writer_write_attribute_date_time:
 * @writer: a TNEF writer
 * @tag: the date attribute tag
 * @value: a date and time
 * @error: return location for an error
 *
 * Writes the seven legacy date fields, without converting their timezone.
 *
 * Returns: whether the operation succeeded
 **/
gboolean
g_mime_tnef_writer_write_attribute_date_time (GMimeTnefWriter *writer, GMimeTnefAttributeTag tag, GDateTime *value,
					      GError **error)
{
	guint8 bytes[14];

	g_return_val_if_fail (GMIME_IS_TNEF_WRITER (writer), FALSE);
	g_return_val_if_fail (value != NULL, FALSE);
	g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

	if ((tag & 0xffff0000) != GMIME_TNEF_ATTRIBUTE_TYPE_DATE)
		return _g_mime_tnef_invalid_operation (error, "The attribute is not a date.");

	_g_mime_tnef_put_uint16 (bytes, g_date_time_get_year (value));
	_g_mime_tnef_put_uint16 (bytes + 2, g_date_time_get_month (value));
	_g_mime_tnef_put_uint16 (bytes + 4, g_date_time_get_day_of_month (value));
	_g_mime_tnef_put_uint16 (bytes + 6, g_date_time_get_hour (value));
	_g_mime_tnef_put_uint16 (bytes + 8, g_date_time_get_minute (value));
	_g_mime_tnef_put_uint16 (bytes + 10, g_date_time_get_second (value));
	_g_mime_tnef_put_uint16 (bytes + 12, g_date_time_get_day_of_week (value) % 7);

	return g_mime_tnef_writer_write_attribute_bytes (writer, tag, bytes, sizeof bytes, error);
}

/**
 * g_mime_tnef_writer_flush:
 * @writer: a TNEF writer
 * @error: return location for an error
 *
 * Writes the stream header and completed buffered attributes, then flushes
 * the underlying stream. All child writers and streams must be closed first.
 *
 * Returns: whether the operation succeeded
 **/
gboolean
g_mime_tnef_writer_flush (GMimeTnefWriter *writer, GError **error)
{
	g_return_val_if_fail (GMIME_IS_TNEF_WRITER (writer), FALSE);
	g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

	if (!writer_check (writer, error) || !drain_pending (writer, error))
		return FALSE;

	if (g_mime_stream_flush (writer->stream) == -1) {
		_g_mime_tnef_io_error (&writer->fault);
		return propagate_fault (writer->fault, error);
	}

	return TRUE;
}

/**
 * g_mime_tnef_writer_close:
 * @writer: a TNEF writer
 * @error: return location for an error
 *
 * Completes output and flushes it. Does not close the caller's output
 * stream. For an embedded writer, also completes the enclosing property
 * value. Repeated successful closes are harmless; errors remain latched.
 *
 * Returns: whether the writer completed successfully
 **/
gboolean
g_mime_tnef_writer_close (GMimeTnefWriter *writer, GError **error)
{
	g_return_val_if_fail (GMIME_IS_TNEF_WRITER (writer), FALSE);
	g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

	if (writer->fault)
		return propagate_fault (writer->fault, error);

	if (writer->closed)
		return TRUE;

	if (!g_mime_tnef_writer_flush (writer, error))
		return FALSE;

	if (writer->close_output && g_mime_stream_close (writer->stream) == -1) {
		_g_mime_tnef_io_error (&writer->fault);
		return propagate_fault (writer->fault, error);
	}

	writer->closed = TRUE;

	return TRUE;
}

static gboolean
finish_attribute (GMimeTnefWriter *writer, GObject *child, const GError *fault, GError **error)
{
	g_assert (writer->child == child);

	writer->child = NULL;

	if (fault) {
		if (!writer->fault)
			writer->fault = g_error_copy (fault);

		g_clear_object (&writer->pending);

		return propagate_fault (writer->fault, error);
	}

	return TRUE;
}

static gboolean
attribute_completed (GObject *owner, GMimeStream *stream, const GError *fault, GError **error)
{
	return finish_attribute (GMIME_TNEF_WRITER (owner), G_OBJECT (stream), fault, error);
}

static ssize_t
write_stream_write (GMimeStream *base, const char *buffer, size_t length)
{
	TnefWriteStream *stream = (TnefWriteStream *) base;
	gint64 position;

	if (stream->closed || stream->fault) {
		errno = stream->closed ? EBADF : EIO;
		return -1;
	}

	position = g_mime_stream_tell (stream->output);

	if (position < 0 || position > G_MAXINT32 || length > (gsize) (G_MAXINT32 - position)) {
		if (position >= 0)
			errno = EOVERFLOW;

		_g_mime_tnef_io_error (&stream->fault);

		return -1;
	}

	if (!_g_mime_tnef_write_all (stream->output, (const guint8 *) buffer, length, &stream->fault))
		return -1;

	g_mime_stream_set_position (base, g_mime_stream_get_position (base) + length);

	return length;
}

static int
write_stream_flush (GMimeStream *base)
{
	TnefWriteStream *stream = (TnefWriteStream *) base;

	if (stream->closed || stream->fault) {
		errno = stream->closed ? EBADF : EIO;
		return -1;
	}

	if (g_mime_stream_flush (stream->output) < 0) {
		_g_mime_tnef_io_error (&stream->fault);
		return -1;
	}

	return 0;
}

static int
write_stream_close (GMimeStream *base)
{
	TnefWriteStream *stream = (TnefWriteStream *) base;
	GError *completion_error = NULL;

	if (!stream->closed) {
		stream->closed = TRUE;

		if (!stream->completed (stream->owner, base, stream->fault, &completion_error)) {
			if (!stream->fault)
				stream->fault = g_steal_pointer (&completion_error);

			g_clear_error (&completion_error);
		}
	}

	if (stream->fault) {
		errno = stream->fault->code > 0 ? stream->fault->code : EINVAL;
		return -1;
	}

	return 0;
}

static void
write_stream_finalize (GObject *object)
{
	TnefWriteStream *stream = (TnefWriteStream *) object;

	if (!stream->closed) {
		if (!stream->fault)
			_g_mime_tnef_invalid_operation (&stream->fault,
							"A TNEF output stream was abandoned without being closed.");
		stream->completed (stream->owner, GMIME_STREAM (stream), stream->fault, NULL);
	}

	g_clear_object (&stream->output);
	g_clear_object (&stream->owner);
	g_clear_error (&stream->fault);

	G_OBJECT_CLASS (tnef_write_stream_parent_class)->finalize (object);
}

static void
tnef_write_stream_class_init (TnefWriteStreamClass *klass)
{
	GMimeStreamClass *stream_class = GMIME_STREAM_CLASS (klass);

	G_OBJECT_CLASS (klass)->finalize = write_stream_finalize;

	stream_class->read = _g_mime_tnef_stream_no_read;
	stream_class->reset = _g_mime_tnef_stream_no_reset;
	stream_class->seek = _g_mime_tnef_stream_no_seek;
	stream_class->substream = _g_mime_tnef_stream_no_substream;
	stream_class->write = write_stream_write;
	stream_class->flush = write_stream_flush;
	stream_class->close = write_stream_close;
}

static void
tnef_write_stream_init (TnefWriteStream *stream)
{
	g_mime_stream_construct (GMIME_STREAM (stream), 0, -1);
}

static GMimeStream *
open_write_stream (GMimeStream *buffer, GObject *owner, GMimeTnefStreamClosedFunc completed)
{
	TnefWriteStream *stream = g_object_new (tnef_write_stream_get_type (), NULL);

	stream->output = g_object_ref (buffer);
	stream->owner = g_object_ref (owner);
	stream->completed = completed;

	return GMIME_STREAM (stream);
}

static gboolean
begin_buffered_attribute (GMimeTnefWriter *writer, GMimeTnefAttributeTag tag, GError **error)
{
	GMimeTnefAttributeLevel level;
	GMimeStream *buffer;
	WriterState next;

	if (!writer_check (writer, error) || !validate_attribute (writer, tag, &next, &level, error))
		return FALSE;

	if (!(buffer = create_temporary_stream (writer, error)))
		return FALSE;

	if (!drain_pending (writer, error)) {
		g_object_unref (buffer);
		return FALSE;
	}

	writer->state = next;
	writer->pending_tag = tag;
	writer->pending_level = level;
	writer->pending = buffer;

	return TRUE;
}

/**
 * g_mime_tnef_writer_open_attribute_stream:
 * @writer: a TNEF writer
 * @tag: the attribute tag
 * @error: return location for an error
 *
 * Opens a write-only raw attribute stream backed by temporary storage.
 * Explicitly close it before the next writer operation. Closing queues the
 * completed attribute; the next operation writes its length and checksum.
 *
 * Returns: (transfer full) (nullable): the stream, or %NULL on error
 **/
GMimeStream *
g_mime_tnef_writer_open_attribute_stream (GMimeTnefWriter *writer, GMimeTnefAttributeTag tag, GError **error)
{
	GMimeStream *stream;

	g_return_val_if_fail (GMIME_IS_TNEF_WRITER (writer), NULL);
	g_return_val_if_fail (error == NULL || *error == NULL, NULL);

	if (!begin_buffered_attribute (writer, tag, error))
		return NULL;

	stream = open_write_stream (writer->pending, G_OBJECT (writer), attribute_completed);
	writer->child = G_OBJECT (stream);

	return stream;
}

static void
g_mime_tnef_property_writer_finalize (GObject *object)
{
	GMimeTnefPropertyWriter *writer = GMIME_TNEF_PROPERTY_WRITER (object);

	if (writer->writer && !writer->closed) {
		if (!writer->fault)
			_g_mime_tnef_invalid_operation (&writer->fault,
							"A property writer was abandoned without being closed.");
		finish_attribute (writer->writer, object, writer->fault, NULL);
	}

	g_clear_object (&writer->writer);
	g_clear_object (&writer->buffer);
	g_clear_error (&writer->fault);

	G_OBJECT_CLASS (g_mime_tnef_property_writer_parent_class)->finalize (object);
}

static void
g_mime_tnef_property_writer_class_init (GMimeTnefPropertyWriterClass *klass)
{
	G_OBJECT_CLASS (klass)->finalize = g_mime_tnef_property_writer_finalize;
}

static void
g_mime_tnef_property_writer_init (GMimeTnefPropertyWriter *writer)
{
	writer->count_offset = -1;
}

static gboolean
property_check (GMimeTnefPropertyWriter *writer, GError **error)
{
	if (writer->fault)
		return propagate_fault (writer->fault, error);

	if (writer->closed)
		return _g_mime_tnef_invalid_operation (error, "The property writer is closed.");

	if (writer->child)
		return _g_mime_tnef_invalid_operation (error,
						       "Close the previous value's stream or embedded writer first.");

	return TRUE;
}

static gboolean
property_write (GMimeTnefPropertyWriter *writer, const guint8 *buffer, gsize length, GError **error)
{
	gint64 position = g_mime_stream_tell (writer->buffer);

	if (position < 0 || position > G_MAXINT32 || length > (gsize) (G_MAXINT32 - position)) {
		if (position >= 0)
			errno = EOVERFLOW;

		_g_mime_tnef_io_error (&writer->fault);

		return propagate_fault (writer->fault, error);
	}

	if (!_g_mime_tnef_write_all (writer->buffer, buffer, length, &writer->fault))
		return propagate_fault (writer->fault, error);

	return TRUE;
}

static gboolean
property_write_int32 (GMimeTnefPropertyWriter *writer, guint32 value, GError **error)
{
	guint8 bytes[4];

	_g_mime_tnef_put_uint32 (bytes, value);

	return property_write (writer, bytes, 4, error);
}

static gboolean
patch_int32 (GMimeTnefPropertyWriter *writer, gint64 offset, guint32 value, GError **error)
{
	gint64 end = g_mime_stream_tell (writer->buffer);

	if (end < 0 || g_mime_stream_seek (writer->buffer, offset, GMIME_STREAM_SEEK_SET) != offset) {
		_g_mime_tnef_io_error (&writer->fault);
		return propagate_fault (writer->fault, error);
	}

	if (!property_write_int32 (writer, value, error))
		return FALSE;

	if (g_mime_stream_seek (writer->buffer, end, GMIME_STREAM_SEEK_SET) != end) {
		_g_mime_tnef_io_error (&writer->fault);
		return propagate_fault (writer->fault, error);
	}

	return TRUE;
}

static gboolean
write_padding (GMimeTnefPropertyWriter *writer, guint length, GError **error)
{
	static const guint8 padding[4];

	return property_write (writer, padding, (4 - (length & 3)) & 3, error);
}

/**
 * g_mime_tnef_writer_open_property_writer:
 * @writer: a TNEF writer
 * @tag: MapiProperties, Attachment or RecipientTable
 * @error: return location for an error
 *
 * Opens a property-list or recipient-table writer backed by temporary
 * storage. Explicitly close it before advancing the attribute writer.
 *
 * Returns: (transfer full) (nullable): the property writer, or %NULL on error
 **/
GMimeTnefPropertyWriter *
g_mime_tnef_writer_open_property_writer (GMimeTnefWriter *writer, GMimeTnefAttributeTag tag, GError **error)
{
	GMimeTnefPropertyWriter *properties;

	g_return_val_if_fail (GMIME_IS_TNEF_WRITER (writer), NULL);
	g_return_val_if_fail (error == NULL || *error == NULL, NULL);

	if (tag != GMIME_TNEF_ATTRIBUTE_TAG_MAPI_PROPERTIES && tag != GMIME_TNEF_ATTRIBUTE_TAG_ATTACHMENT &&
	    tag != GMIME_TNEF_ATTRIBUTE_TAG_RECIPIENT_TABLE) {
		_g_mime_tnef_invalid_operation (error, "The attribute does not contain MAPI properties.");
		return NULL;
	}

	if (!begin_buffered_attribute (writer, tag, error))
		return NULL;

	properties = g_object_new (GMIME_TYPE_TNEF_PROPERTY_WRITER, NULL);
	properties->writer = g_object_ref (writer);
	properties->buffer = g_object_ref (writer->pending);
	properties->is_table = tag == GMIME_TNEF_ATTRIBUTE_TAG_RECIPIENT_TABLE;
	properties->count_offset = properties->is_table ? -1 : 0;

	writer->child = G_OBJECT (properties);

	/* Both property lists and recipient tables start with a 32-bit count. */
	if (!property_write_int32 (properties, 0, error)) {
		g_object_unref (properties);
		return NULL;
	}

	return properties;
}

static gboolean
end_property (GMimeTnefPropertyWriter *writer, GError **error)
{
	if (!writer->has_property)
		return TRUE;

	if (g_mime_tnef_property_tag_is_multi_valued (writer->tag)) {
		if (!patch_int32 (writer, writer->value_count_offset, writer->value_count, error))
			return FALSE;
	} else if (g_mime_tnef_property_tag_get_value_type (writer->tag) != GMIME_TNEF_PROPERTY_TYPE_NULL &&
		   writer->value_count != 1) {
		return _g_mime_tnef_invalid_operation (error, "A single-valued property must have exactly one value.");
	}

	/* A Null property has no value. */
	writer->has_property = FALSE;

	return TRUE;
}

static gboolean
property_end_row (GMimeTnefPropertyWriter *writer, GError **error)
{
	if (!end_property (writer, error))
		return FALSE;

	return writer->count_offset < 0 || patch_int32 (writer, writer->count_offset, writer->property_count, error);
}

/**
 * g_mime_tnef_property_writer_begin_row:
 * @writer: a recipient-table writer
 * @error: return location for an error
 *
 * Completes the previous row and starts a new recipient row.
 *
 * Returns: whether the operation succeeded
 **/
gboolean
g_mime_tnef_property_writer_begin_row (GMimeTnefPropertyWriter *writer, GError **error)
{
	g_return_val_if_fail (GMIME_IS_TNEF_PROPERTY_WRITER (writer), FALSE);
	g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

	if (!property_check (writer, error))
		return FALSE;

	if (!writer->is_table)
		return _g_mime_tnef_invalid_operation (error, "Only a RecipientTable attribute contains rows.");

	if (writer->row_count == G_MAXINT32)
		return _g_mime_tnef_invalid_operation (error, "The row count exceeds the 32-bit limit.");

	if (!property_end_row (writer, error))
		return FALSE;

	writer->count_offset = g_mime_stream_tell (writer->buffer);
	writer->property_count = 0;

	if (!property_write_int32 (writer, 0, error))
		return FALSE;

	writer->row_count++;

	return TRUE;
}

static gboolean
validate_property_type (GMimeTnefPropertyType type, GError **error)
{
	GMimeTnefPropertyType value_type = (guint) type & ~GMIME_TNEF_PROPERTY_TYPE_MULTI_VALUED;

	if (_g_mime_tnef_get_fixed_width (value_type) == -2)
		return _g_mime_tnef_invalid_operation (error, "The property type is unsupported.");

	if (type != value_type &&
	    (value_type == GMIME_TNEF_PROPERTY_TYPE_NULL || value_type == GMIME_TNEF_PROPERTY_TYPE_ERROR ||
	     value_type == GMIME_TNEF_PROPERTY_TYPE_BOOLEAN || value_type == GMIME_TNEF_PROPERTY_TYPE_OBJECT))
		return _g_mime_tnef_invalid_operation (error, "This property type cannot be multi-valued.");

	return TRUE;
}

static gboolean
check_property (GMimeTnefPropertyWriter *writer, GError **error)
{
	if (!property_check (writer, error))
		return FALSE;

	if (writer->count_offset < 0)
		return _g_mime_tnef_invalid_operation (error, "Begin a recipient row before writing its properties.");

	if (writer->property_count == G_MAXINT32)
		return _g_mime_tnef_invalid_operation (error, "The property count exceeds the 32-bit limit.");

	return end_property (writer, error);
}

static gboolean
begin_property (GMimeTnefPropertyWriter *writer, GMimeTnefPropertyTag tag, GError **error)
{
	writer->has_property = TRUE;
	writer->tag = tag;
	writer->value_count = 0;
	writer->property_count++;

	if (g_mime_tnef_property_tag_is_multi_valued (tag)) {
		writer->value_count_offset = g_mime_stream_tell (writer->buffer);
		return property_write_int32 (writer, 0, error);
	}

	/* Single variable-length values are also preceded by a count. */
	if (_g_mime_tnef_get_fixed_width (g_mime_tnef_property_tag_get_value_type (tag)) == -1)
		return property_write_int32 (writer, 1, error);

	return TRUE;
}

/**
 * g_mime_tnef_property_writer_write_property_tag:
 * @writer: a property writer
 * @tag: a non-named property tag
 * @error: return location for an error
 *
 * Completes the preceding property and starts an ordinary property.
 *
 * Returns: whether the operation succeeded
 **/
gboolean
g_mime_tnef_property_writer_write_property_tag (GMimeTnefPropertyWriter *writer, GMimeTnefPropertyTag tag,
						GError **error)
{
	g_return_val_if_fail (GMIME_IS_TNEF_PROPERTY_WRITER (writer), FALSE);
	g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

	if (g_mime_tnef_property_tag_is_named (tag))
		return _g_mime_tnef_invalid_operation (error,
						       "Named properties require a name, not a preassigned identifier.");

	if (!validate_property_type (g_mime_tnef_property_tag_get_property_type (tag), error) ||
	    !check_property (writer, error))
		return FALSE;

	/* [MS-OXTNEF] stores the 16-bit property type followed by the 16-bit identifier. */
	return property_write_int32 (writer, tag, error) && begin_property (writer, tag, error);
}

/**
 * g_mime_tnef_property_writer_write_named_property:
 * @writer: a property writer
 * @name: the named-property identity
 * @type: the type, optionally multi-valued
 * @error: return location for an error
 *
 * Starts a named property. The outer attribute writer assigns consistent
 * identifiers in the range 0x8000 through 0xfffe within each TNEF stream.
 *
 * Returns: whether the operation succeeded
 **/
gboolean
g_mime_tnef_property_writer_write_named_property (GMimeTnefPropertyWriter *writer, GMimeTnefNameId *name, guint16 type,
						  GError **error)
{
	GMimeTnefPropertyTag tag;
	GMimeTnefWriter *parent;
	GBytes *bytes = NULL;
	const guint8 *data;
	gboolean success;
	guint8 guid[16];
	gsize length;
	guint id;

	g_return_val_if_fail (GMIME_IS_TNEF_PROPERTY_WRITER (writer), FALSE);
	g_return_val_if_fail (name != NULL, FALSE);
	g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

	if (!validate_property_type (type, error) || !check_property (writer, error))
		return FALSE;

	parent = writer->writer;
	id = GPOINTER_TO_UINT (g_hash_table_lookup (parent->names, name));

	if (!id) {
		if (parent->next_name_id > 0xfffe)
			return _g_mime_tnef_invalid_operation (error,
							       "The TNEF stream contains too many named properties.");
		id = parent->next_name_id++;
		g_hash_table_insert (parent->names, g_mime_tnef_name_id_ref (name), GUINT_TO_POINTER (id));
	}

	tag = g_mime_tnef_property_tag_new (id, type);
	_g_mime_tnef_guid_from_string (name->property_set, guid);

	if (!property_write_int32 (writer, tag, error) || !property_write (writer, guid, 16, error) ||
	    !property_write_int32 (writer, name->kind, error))
		return FALSE;

	if (name->kind == GMIME_TNEF_NAME_ID_KIND_ID) {
		if (!property_write_int32 (writer, (guint32) name->id, error))
			return FALSE;
	} else {
		/* Names are nul-terminated UTF-16LE, length-prefixed (including terminator), and padded to four bytes. */
		if (!(bytes = _g_mime_tnef_encode_string ("UTF-16LE", name->name, TRUE, error)))
			return FALSE;

		data = g_bytes_get_data (bytes, &length);

		if (length > G_MAXINT32) {
			g_bytes_unref (bytes);
			return _g_mime_tnef_invalid_operation (error,
							       "The property name exceeds the 32-bit length limit.");
		}

		success = property_write_int32 (writer, length, error) &&
			  property_write (writer, data, length, error) && write_padding (writer, length, error);
		g_bytes_unref (bytes);

		if (!success)
			return FALSE;
	}

	return begin_property (writer, tag, error);
}

static gboolean
check_value_type (GMimeTnefPropertyWriter *writer, GMimeTnefPropertyType first, GMimeTnefPropertyType second,
		  GError **error)
{
	GMimeTnefPropertyType type;

	if (!property_check (writer, error))
		return FALSE;

	if (!writer->has_property)
		return _g_mime_tnef_invalid_operation (error, "Start a property before writing its value.");

	type = g_mime_tnef_property_tag_get_value_type (writer->tag);

	if (type != first && type != second)
		return _g_mime_tnef_invalid_operation (error, "The value type does not match the property type.");

	if (!g_mime_tnef_property_tag_is_multi_valued (writer->tag) && writer->value_count)
		return _g_mime_tnef_invalid_operation (error, "The single-valued property already has a value.");

	if (writer->value_count == G_MAXINT32)
		return _g_mime_tnef_invalid_operation (error, "The value count exceeds the 32-bit limit.");

	return TRUE;
}

static gboolean
write_fixed_value (GMimeTnefPropertyWriter *writer, const guint8 *bytes, guint width, GError **error)
{
	if (!property_write (writer, bytes, width, error))
		return FALSE;

	writer->value_count++;

	return TRUE;
}

static gboolean
write_variable_value (GMimeTnefPropertyWriter *writer, const guint8 *buffer, gsize length, GError **error)
{
	if (length > G_MAXINT32)
		return _g_mime_tnef_invalid_operation (error, "The value exceeds the 32-bit length limit.");

	if (!property_write_int32 (writer, length, error) || !property_write (writer, buffer, length, error) ||
	    !write_padding (writer, length, error))
		return FALSE;

	writer->value_count++;

	return TRUE;
}

/**
 * g_mime_tnef_property_writer_write_value_string:
 * @writer: a property writer
 * @value: a UTF-8 string
 * @error: return location for an error
 *
 * Writes a nul-terminated String8 or Unicode value, with its count, length
 * and padding. String8 uses the writer's codepage; Unicode uses UTF-16LE.
 *
 * Returns: whether the operation succeeded
 **/
gboolean
g_mime_tnef_property_writer_write_value_string (GMimeTnefPropertyWriter *writer, const char *value, GError **error)
{
	gboolean success, unicode;
	const guint8 *data;
	GBytes *bytes;
	gsize length;

	g_return_val_if_fail (GMIME_IS_TNEF_PROPERTY_WRITER (writer), FALSE);
	g_return_val_if_fail (value != NULL, FALSE);
	g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

	if (!check_value_type (writer, GMIME_TNEF_PROPERTY_TYPE_STRING8, GMIME_TNEF_PROPERTY_TYPE_UNICODE, error))
		return FALSE;

	unicode = g_mime_tnef_property_tag_get_value_type (writer->tag) == GMIME_TNEF_PROPERTY_TYPE_UNICODE;

	if (!(bytes = _g_mime_tnef_encode_string (writer->writer->charset, value, unicode, error)))
		return FALSE;

	data = g_bytes_get_data (bytes, &length);
	success = write_variable_value (writer, data, length, error);
	g_bytes_unref (bytes);

	return success;
}

/**
 * g_mime_tnef_property_writer_write_value_bytes:
 * @writer: a property writer
 * @buffer: (array length=length) (nullable): the raw value
 * @length: byte count
 * @error: return location for an error
 *
 * Writes a Binary or Object value with its length and padding.
 *
 * Returns: whether the operation succeeded
 **/
gboolean
g_mime_tnef_property_writer_write_value_bytes (GMimeTnefPropertyWriter *writer, const guint8 *buffer, gsize length,
					       GError **error)
{
	g_return_val_if_fail (GMIME_IS_TNEF_PROPERTY_WRITER (writer), FALSE);
	g_return_val_if_fail (buffer != NULL || length == 0, FALSE);
	g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

	if (g_mime_tnef_property_tag_get_value_type (writer->tag) == GMIME_TNEF_PROPERTY_TYPE_OBJECT && length < 16)
		return _g_mime_tnef_invalid_operation (error,
						       "An Object value must include its 16-byte interface identifier.");

	return check_value_type (writer, GMIME_TNEF_PROPERTY_TYPE_BINARY, GMIME_TNEF_PROPERTY_TYPE_OBJECT, error) &&
	       write_variable_value (writer, buffer, length, error);
}

/**
 * g_mime_tnef_property_writer_write_value_guid:
 * @writer: a property writer
 * @value: a canonical GUID string
 * @error: return location for an error
 *
 * Writes a ClassId value using the MAPI GUID byte order.
 *
 * Returns: whether the operation succeeded
 **/
gboolean
g_mime_tnef_property_writer_write_value_guid (GMimeTnefPropertyWriter *writer, const char *value, GError **error)
{
	guint8 bytes[16];

	g_return_val_if_fail (GMIME_IS_TNEF_PROPERTY_WRITER (writer), FALSE);
	g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

	if (!_g_mime_tnef_guid_from_string (value, bytes))
		return _g_mime_tnef_invalid_operation (error, "The GUID string is invalid.");

	return check_value_type (writer, GMIME_TNEF_PROPERTY_TYPE_CLASS_ID, GMIME_TNEF_PROPERTY_TYPE_CLASS_ID, error) &&
	       write_fixed_value (writer, bytes, sizeof bytes, error);
}

/**
 * g_mime_tnef_property_writer_write_value_date_time:
 * @writer: a property writer
 * @value: a date and time
 * @error: return location for an error
 *
 * Writes SysTime as UTC FILETIME or AppTime as an OLE Automation date
 * using the supplied wall-clock fields.
 *
 * Returns: whether the operation succeeded
 **/
gboolean
g_mime_tnef_property_writer_write_value_date_time (GMimeTnefPropertyWriter *writer, GDateTime *value, GError **error)
{
	GDateTime *epoch, *wall;
	double days, whole;
	guint8 bytes[8];
	gint64 ticks;
	guint64 bits;

	g_return_val_if_fail (GMIME_IS_TNEF_PROPERTY_WRITER (writer), FALSE);
	g_return_val_if_fail (value != NULL, FALSE);
	g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

	if (!check_value_type (writer, GMIME_TNEF_PROPERTY_TYPE_SYS_TIME, GMIME_TNEF_PROPERTY_TYPE_APP_TIME, error))
		return FALSE;

	if (g_mime_tnef_property_tag_get_value_type (writer->tag) == GMIME_TNEF_PROPERTY_TYPE_SYS_TIME) {
		/* FILETIME counts 100-nanosecond intervals since January 1, 1601 UTC. */
		epoch = g_date_time_new_utc (1601, 1, 1, 0, 0, 0);
		ticks = g_date_time_difference (value, epoch);
		g_date_time_unref (epoch);

		if (ticks < 0)
			return _g_mime_tnef_invalid_operation (error,
							       "FILETIME cannot represent dates before January 1, 1601.");
		bits = (guint64) ticks * 10;
	} else {
		if (g_date_time_get_year (value) < 100)
			return _g_mime_tnef_invalid_operation (error,
							       "OLE Automation dates cannot represent years before 100.");

		wall = g_date_time_new_utc (g_date_time_get_year (value), g_date_time_get_month (value),
					    g_date_time_get_day_of_month (value), g_date_time_get_hour (value),
					    g_date_time_get_minute (value), g_date_time_get_seconds (value));
		epoch = g_date_time_new_utc (1899, 12, 30, 0, 0, 0);
		ticks = g_date_time_difference (wall, epoch);

		g_date_time_unref (wall);
		g_date_time_unref (epoch);

		days = (ticks / 1000) / 86400000.0;

		if (days < 0) {
			whole = floor (days);
			days = whole - (days - whole);
		}

		memcpy (&bits, &days, sizeof bits);
	}

	_g_mime_tnef_put_uint64 (bytes, bits);

	return write_fixed_value (writer, bytes, sizeof bytes, error);
}

static gboolean
begin_stream_value (GMimeTnefPropertyWriter *writer, GError **error)
{
	writer->value_length_offset = g_mime_stream_tell (writer->buffer);

	return property_write_int32 (writer, 0, error);
}

static gboolean
property_value_completed (GObject *owner, GMimeStream *stream, const GError *fault, GError **error)
{
	GMimeTnefPropertyWriter *writer = GMIME_TNEF_PROPERTY_WRITER (owner);
	gint64 length;

	g_assert (writer->child == G_OBJECT (stream));

	writer->child = NULL;

	if (fault) {
		if (!writer->fault)
			writer->fault = g_error_copy (fault);
		return propagate_fault (writer->fault, error);
	}

	length = g_mime_stream_tell (writer->buffer);

	if (length < writer->value_length_offset + 4 || length > G_MAXINT32) {
		errno = length < 0 ? EIO : EOVERFLOW;
		_g_mime_tnef_io_error (&writer->fault);
		return propagate_fault (writer->fault, error);
	}

	length -= writer->value_length_offset + 4;

	if (g_mime_tnef_property_tag_get_value_type (writer->tag) == GMIME_TNEF_PROPERTY_TYPE_OBJECT && length < 16) {
		_g_mime_tnef_invalid_operation (&writer->fault,
						"An Object value must include its 16-byte interface identifier.");
		return propagate_fault (writer->fault, error);
	}

	if (!patch_int32 (writer, writer->value_length_offset, length, error) || !write_padding (writer, length, error))
		return FALSE;

	writer->value_count++;

	return TRUE;
}

/**
 * g_mime_tnef_property_writer_open_value_stream:
 * @writer: a property writer
 * @error: return location for an error
 *
 * Opens a write-only Binary or Object value stream. Close it explicitly
 * to patch its length and add padding before writing another value.
 *
 * Returns: (transfer full) (nullable): the stream, or %NULL on error
 **/
GMimeStream *
g_mime_tnef_property_writer_open_value_stream (GMimeTnefPropertyWriter *writer, GError **error)
{
	GMimeStream *stream;

	g_return_val_if_fail (GMIME_IS_TNEF_PROPERTY_WRITER (writer), NULL);
	g_return_val_if_fail (error == NULL || *error == NULL, NULL);

	if (!check_value_type (writer, GMIME_TNEF_PROPERTY_TYPE_BINARY, GMIME_TNEF_PROPERTY_TYPE_OBJECT, error) ||
	    !begin_stream_value (writer, error))
		return NULL;

	stream = open_write_stream (writer->buffer, G_OBJECT (writer), property_value_completed);
	writer->child = G_OBJECT (stream);

	return stream;
}

/**
 * g_mime_tnef_property_writer_open_rtf_compressed_stream:
 * @writer: a property writer
 * @compress: whether to use LZFu compression rather than the uncompressed wrapper
 * @error: return location for an error
 *
 * Opens a write-only stream for RTF bytes in an RtfCompressed Binary property.
 * Explicit close writes the compressed-RTF header and CRC, then completes
 * the property value. Flushing does not finalize the compression stream.
 *
 * Returns: (transfer full) (nullable): the stream, or %NULL on error
 **/
GMimeStream *
g_mime_tnef_property_writer_open_rtf_compressed_stream (GMimeTnefPropertyWriter *writer, gboolean compress,
							GError **error)
{
	GMimeStream *stream;

	g_return_val_if_fail (GMIME_IS_TNEF_PROPERTY_WRITER (writer), NULL);
	g_return_val_if_fail (error == NULL || *error == NULL, NULL);

	if (!check_value_type (writer, GMIME_TNEF_PROPERTY_TYPE_BINARY, GMIME_TNEF_PROPERTY_TYPE_BINARY, error))
		return NULL;

	if (g_mime_tnef_property_tag_get_id (writer->tag) != GMIME_TNEF_PROPERTY_ID_RTF_COMPRESSED) {
		_g_mime_tnef_invalid_operation (error, "The current property is not RtfCompressed.");
		return NULL;
	}

	if (!begin_stream_value (writer, error))
		return NULL;

	stream = _g_mime_tnef_rtf_write_stream_new (writer->buffer, compress, G_OBJECT (writer),
						    property_value_completed, &writer->fault);

	if (!stream) {
		propagate_fault (writer->fault, error);
		return NULL;
	}

	writer->child = G_OBJECT (stream);

	return stream;
}

/**
 * g_mime_tnef_property_writer_open_embedded_message:
 * @writer: a property writer
 * @codepage: the embedded message's codepage, or zero to inherit
 * @error: return location for an error
 *
 * Opens an embedded TNEF writer for an Object property, prefixing its
 * IMessage IID. The temporary-stream factory is inherited. Close the
 * embedded writer explicitly before continuing the property list.
 *
 * Returns: (transfer full) (nullable): the embedded writer, or %NULL on error
 **/
GMimeTnefWriter *
g_mime_tnef_property_writer_open_embedded_message (GMimeTnefPropertyWriter *writer, int codepage, GError **error)
{
	static const guint8 iid[16] = { 7, 3, 2, 0, 0, 0, 0, 0, 0xc0, 0, 0, 0, 0, 0, 0, 0x46 };
	GMimeTnefWriter *embedded;
	GMimeStream *stream;

	g_return_val_if_fail (GMIME_IS_TNEF_PROPERTY_WRITER (writer), NULL);
	g_return_val_if_fail (error == NULL || *error == NULL, NULL);

	if (!check_value_type (writer, GMIME_TNEF_PROPERTY_TYPE_OBJECT, GMIME_TNEF_PROPERTY_TYPE_OBJECT, error))
		return NULL;

	/* Construction writes nothing: validate the codepage before starting the value. */
	embedded = g_mime_tnef_writer_new (writer->buffer, codepage ? codepage : writer->writer->codepage, 0, error);

	if (!embedded)
		return NULL;

	if (!(stream = g_mime_tnef_property_writer_open_value_stream (writer, error))) {
		g_object_unref (embedded);
		return NULL;
	}

	g_object_unref (embedded->stream);
	embedded->stream = stream;
	embedded->close_output = TRUE;
	embedded->factory = writer->writer->factory;

	if (embedded->factory)
		g_atomic_int_inc (&embedded->factory->ref_count);

	if (!_g_mime_tnef_write_all (stream, iid, sizeof iid, error)) {
		g_object_unref (embedded);
		return NULL;
	}

	return embedded;
}

/**
 * g_mime_tnef_property_writer_close:
 * @writer: a property writer
 * @error: return location for an error
 *
 * Completes the current property and row, patches counts, and queues the
 * attribute. An incomplete property discards the attribute and latches an
 * error in its parent. Close active value streams before calling this.
 *
 * Returns: whether the attribute completed successfully
 **/
gboolean
g_mime_tnef_property_writer_close (GMimeTnefPropertyWriter *writer, GError **error)
{
	GError *completion_error = NULL;
	gboolean success;

	g_return_val_if_fail (GMIME_IS_TNEF_PROPERTY_WRITER (writer), FALSE);
	g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

	if (writer->closed)
		return writer->fault ? propagate_fault (writer->fault, error) : TRUE;

	if (writer->child)
		return _g_mime_tnef_invalid_operation (error,
						       "Close the current value stream or embedded writer first.");

	if (!writer->fault) {
		success = property_end_row (writer, &completion_error);

		if (success && writer->is_table)
			success = patch_int32 (writer, 0, writer->row_count, &completion_error);

		if (!success && !writer->fault)
			writer->fault = g_steal_pointer (&completion_error);

		g_clear_error (&completion_error);
	}

	writer->closed = TRUE;

	return finish_attribute (writer->writer, G_OBJECT (writer), writer->fault, error);
}

/**
 * g_mime_tnef_property_writer_write_value:
 * @writer: a property writer
 * @value: a value in the representation returned by the property reader
 * @error: return location for an error
 *
 * Writes a native GLib value. Currency is an exact signed 64-bit integer
 * in units of 1/10000. GUIDs are strings. Null properties have no value
 * and do not require this operation.
 *
 * Returns: whether the operation succeeded
 **/
gboolean
g_mime_tnef_property_writer_write_value (GMimeTnefPropertyWriter *writer, const GValue *value, GError **error)
{
	GMimeTnefPropertyType type;
	const guint8 *data;
	GBytes *bytes;
	gsize length;
	int number;

	g_return_val_if_fail (GMIME_IS_TNEF_PROPERTY_WRITER (writer), FALSE);
	g_return_val_if_fail (value != NULL && G_IS_VALUE (value), FALSE);
	g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

	if (!property_check (writer, error))
		return FALSE;

	if (!writer->has_property)
		return _g_mime_tnef_invalid_operation (error, "Start a property before writing its value.");

	type = g_mime_tnef_property_tag_get_value_type (writer->tag);

	switch (type) {
	case GMIME_TNEF_PROPERTY_TYPE_I2:
		if (G_VALUE_HOLDS_INT (value)) {
			number = g_value_get_int (value);

			if (number >= G_MININT16 && number <= G_MAXINT16)
				return g_mime_tnef_property_writer_write_value_int16 (writer, number, error);
		}
		break;
	case GMIME_TNEF_PROPERTY_TYPE_LONG:
	case GMIME_TNEF_PROPERTY_TYPE_ERROR:
		if (G_VALUE_HOLDS_INT (value))
			return g_mime_tnef_property_writer_write_value_int32 (writer, g_value_get_int (value), error);
		break;
	case GMIME_TNEF_PROPERTY_TYPE_I8:
		if (G_VALUE_HOLDS_INT64 (value))
			return g_mime_tnef_property_writer_write_value_int64 (writer, g_value_get_int64 (value), error);
		break;
	case GMIME_TNEF_PROPERTY_TYPE_CURRENCY:
		if (G_VALUE_HOLDS_INT64 (value))
			return g_mime_tnef_property_writer_write_value_currency (writer, g_value_get_int64 (value),
										 error);
		break;
	case GMIME_TNEF_PROPERTY_TYPE_BOOLEAN:
		if (G_VALUE_HOLDS_BOOLEAN (value))
			return g_mime_tnef_property_writer_write_value_boolean (writer, g_value_get_boolean (value),
										error);
		break;
	case GMIME_TNEF_PROPERTY_TYPE_R4:
		if (G_VALUE_HOLDS_FLOAT (value))
			return g_mime_tnef_property_writer_write_value_float (writer, g_value_get_float (value), error);
		break;
	case GMIME_TNEF_PROPERTY_TYPE_DOUBLE:
		if (G_VALUE_HOLDS_DOUBLE (value))
			return g_mime_tnef_property_writer_write_value_double (writer, g_value_get_double (value),
									       error);
		break;
	case GMIME_TNEF_PROPERTY_TYPE_SYS_TIME:
	case GMIME_TNEF_PROPERTY_TYPE_APP_TIME:
		if (G_VALUE_HOLDS (value, G_TYPE_DATE_TIME) && g_value_get_boxed (value))
			return g_mime_tnef_property_writer_write_value_date_time (writer, g_value_get_boxed (value),
										  error);
		break;
	case GMIME_TNEF_PROPERTY_TYPE_CLASS_ID:
		if (G_VALUE_HOLDS_STRING (value))
			return g_mime_tnef_property_writer_write_value_guid (writer, g_value_get_string (value), error);
		break;
	case GMIME_TNEF_PROPERTY_TYPE_STRING8:
	case GMIME_TNEF_PROPERTY_TYPE_UNICODE:
		if (G_VALUE_HOLDS_STRING (value) && g_value_get_string (value))
			return g_mime_tnef_property_writer_write_value_string (writer, g_value_get_string (value),
									       error);
		break;
	case GMIME_TNEF_PROPERTY_TYPE_BINARY:
	case GMIME_TNEF_PROPERTY_TYPE_OBJECT:
		if (G_VALUE_HOLDS (value, G_TYPE_BYTES) && (bytes = g_value_get_boxed (value))) {
			data = g_bytes_get_data (bytes, &length);
			return g_mime_tnef_property_writer_write_value_bytes (writer, data, length, error);
		}
		break;
	default:
		break;
	}

	return _g_mime_tnef_invalid_operation (error,
					       "The value does not have the property's native GLib type or range.");
}

/**
 * g_mime_tnef_property_writer_write_property:
 * @writer: a property writer
 * @property: an immutable property snapshot
 * @error: return location for an error
 *
 * Writes a property and its values. Named identities are assigned fresh
 * stream-local identifiers. Strings use the destination's codepage.
 * Incomplete snapshots are rejected before writing a property tag.
 *
 * Returns: whether the operation succeeded
 **/
gboolean
g_mime_tnef_property_writer_write_property (GMimeTnefPropertyWriter *writer, const GMimeTnefProperty *property,
					    GError **error)
{
	GMimeTnefPropertyType type;
	const GValue *value;
	GDateTime *date;
	GBytes *bytes;
	guint i;

	g_return_val_if_fail (GMIME_IS_TNEF_PROPERTY_WRITER (writer), FALSE);
	g_return_val_if_fail (property != NULL, FALSE);
	g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

	if (!property_check (writer, error) ||
	    !validate_property_type (g_mime_tnef_property_tag_get_property_type (property->tag), error))
		return FALSE;

	type = g_mime_tnef_property_tag_get_value_type (property->tag);

	if (g_mime_tnef_property_tag_is_named (property->tag) && !property->name)
		return _g_mime_tnef_invalid_operation (error, "The named property has no identity.");

	if (type != GMIME_TNEF_PROPERTY_TYPE_NULL && !g_mime_tnef_property_tag_is_multi_valued (property->tag) &&
	    property->values->len != 1)
		return _g_mime_tnef_invalid_operation (error, "The snapshot does not contain a complete single value.");

	/* Validate values before writing anything so that an invalid snapshot is not partially written. */
	for (i = 0; i < property->values->len; i++) {
		value = &g_array_index (property->values, GValue, i);

		if (type == GMIME_TNEF_PROPERTY_TYPE_OBJECT) {
			bytes = g_value_get_boxed (value);

			if (g_bytes_get_size (bytes) < 16)
				return _g_mime_tnef_invalid_operation (error,
								       "An Object snapshot lacks its interface identifier.");
		} else if (type == GMIME_TNEF_PROPERTY_TYPE_SYS_TIME || type == GMIME_TNEF_PROPERTY_TYPE_APP_TIME) {
			date = g_value_get_boxed (value);

			if (g_date_time_get_year (date) < (type == GMIME_TNEF_PROPERTY_TYPE_SYS_TIME ? 1601 : 100))
				return _g_mime_tnef_invalid_operation (error,
								       "The snapshot's date is outside the property's wire range.");
		}
	}

	if (property->name) {
		if (!g_mime_tnef_property_writer_write_named_property (writer, property->name,
								       g_mime_tnef_property_tag_get_property_type (
									       property->tag),
								       error))
			return FALSE;
	} else if (!g_mime_tnef_property_writer_write_property_tag (writer, property->tag, error)) {
		return FALSE;
	}

	if (type == GMIME_TNEF_PROPERTY_TYPE_NULL)
		return TRUE;

	for (i = 0; i < property->values->len; i++) {
		if (!g_mime_tnef_property_writer_write_value (writer, &g_array_index (property->values, GValue, i),
							      error))
			return FALSE;
	}

	return TRUE;
}

/* Explicit TNEF writer value methods. */

/**
 * g_mime_tnef_property_writer_write_value_boolean:
 * @writer: a property writer
 * @value: the value
 * @error: return location for an error
 *
 * Writes a Boolean value padded to four bytes.
 *
 * Returns: whether the operation succeeded
 **/
gboolean
g_mime_tnef_property_writer_write_value_boolean (GMimeTnefPropertyWriter *writer, gboolean value, GError **error)
{
	guint8 bytes[4] = { 0 };

	g_return_val_if_fail (GMIME_IS_TNEF_PROPERTY_WRITER (writer), FALSE);
	g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

	if (!check_value_type (writer, GMIME_TNEF_PROPERTY_TYPE_BOOLEAN, GMIME_TNEF_PROPERTY_TYPE_BOOLEAN, error))
		return FALSE;

	_g_mime_tnef_put_uint32 (bytes, value ? 1 : 0);

	return write_fixed_value (writer, bytes, sizeof bytes, error);
}

/**
 * g_mime_tnef_property_writer_write_value_int16:
 * @writer: a property writer
 * @value: the value
 * @error: return location for an error
 *
 * Writes an I2 value padded to four bytes.
 *
 * Returns: whether the operation succeeded
 **/
gboolean
g_mime_tnef_property_writer_write_value_int16 (GMimeTnefPropertyWriter *writer, gint16 value, GError **error)
{
	guint8 bytes[4] = { 0 };

	g_return_val_if_fail (GMIME_IS_TNEF_PROPERTY_WRITER (writer), FALSE);
	g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

	if (!check_value_type (writer, GMIME_TNEF_PROPERTY_TYPE_I2, GMIME_TNEF_PROPERTY_TYPE_I2, error))
		return FALSE;

	_g_mime_tnef_put_uint16 (bytes, (guint16) value);

	return write_fixed_value (writer, bytes, sizeof bytes, error);
}

/**
 * g_mime_tnef_property_writer_write_value_int32:
 * @writer: a property writer
 * @value: the value
 * @error: return location for an error
 *
 * Writes a Long or Error value.
 *
 * Returns: whether the operation succeeded
 **/
gboolean
g_mime_tnef_property_writer_write_value_int32 (GMimeTnefPropertyWriter *writer, gint32 value, GError **error)
{
	guint8 bytes[4] = { 0 };

	g_return_val_if_fail (GMIME_IS_TNEF_PROPERTY_WRITER (writer), FALSE);
	g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

	if (!check_value_type (writer, GMIME_TNEF_PROPERTY_TYPE_LONG, GMIME_TNEF_PROPERTY_TYPE_ERROR, error))
		return FALSE;

	_g_mime_tnef_put_uint32 (bytes, (guint32) value);

	return write_fixed_value (writer, bytes, sizeof bytes, error);
}

/**
 * g_mime_tnef_property_writer_write_value_int64:
 * @writer: a property writer
 * @value: the value
 * @error: return location for an error
 *
 * Writes an I8 value.
 *
 * Returns: whether the operation succeeded
 **/
gboolean
g_mime_tnef_property_writer_write_value_int64 (GMimeTnefPropertyWriter *writer, gint64 value, GError **error)
{
	guint8 bytes[8] = { 0 };

	g_return_val_if_fail (GMIME_IS_TNEF_PROPERTY_WRITER (writer), FALSE);
	g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

	if (!check_value_type (writer, GMIME_TNEF_PROPERTY_TYPE_I8, GMIME_TNEF_PROPERTY_TYPE_I8, error))
		return FALSE;

	_g_mime_tnef_put_uint64 (bytes, (guint64) value);

	return write_fixed_value (writer, bytes, sizeof bytes, error);
}

/**
 * g_mime_tnef_property_writer_write_value_float:
 * @writer: a property writer
 * @value: the value
 * @error: return location for an error
 *
 * Writes an R4 value, preserving its IEEE-754 representation.
 *
 * Returns: whether the operation succeeded
 **/
gboolean
g_mime_tnef_property_writer_write_value_float (GMimeTnefPropertyWriter *writer, float value, GError **error)
{
	guint8 bytes[4] = { 0 };
	guint32 bits;

	g_return_val_if_fail (GMIME_IS_TNEF_PROPERTY_WRITER (writer), FALSE);
	g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

	if (!check_value_type (writer, GMIME_TNEF_PROPERTY_TYPE_R4, GMIME_TNEF_PROPERTY_TYPE_R4, error))
		return FALSE;

	memcpy (&bits, &value, sizeof bits);
	_g_mime_tnef_put_uint32 (bytes, bits);

	return write_fixed_value (writer, bytes, sizeof bytes, error);
}

/**
 * g_mime_tnef_property_writer_write_value_double:
 * @writer: a property writer
 * @value: the value
 * @error: return location for an error
 *
 * Writes a Double value, preserving its IEEE-754 representation.
 *
 * Returns: whether the operation succeeded
 **/
gboolean
g_mime_tnef_property_writer_write_value_double (GMimeTnefPropertyWriter *writer, double value, GError **error)
{
	guint8 bytes[8] = { 0 };
	guint64 bits;

	g_return_val_if_fail (GMIME_IS_TNEF_PROPERTY_WRITER (writer), FALSE);
	g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

	if (!check_value_type (writer, GMIME_TNEF_PROPERTY_TYPE_DOUBLE, GMIME_TNEF_PROPERTY_TYPE_DOUBLE, error))
		return FALSE;

	memcpy (&bits, &value, sizeof bits);
	_g_mime_tnef_put_uint64 (bytes, bits);

	return write_fixed_value (writer, bytes, sizeof bytes, error);
}

/**
 * g_mime_tnef_property_writer_write_value_currency:
 * @writer: a property writer
 * @scaled_value: the value in units of 1/10000
 * @error: return location for an error
 *
 * Writes an exact Currency value in units of 1/10000 of a monetary unit.
 *
 * Returns: whether the operation succeeded
 **/
gboolean
g_mime_tnef_property_writer_write_value_currency (GMimeTnefPropertyWriter *writer, gint64 scaled_value, GError **error)
{
	guint8 bytes[8] = { 0 };

	g_return_val_if_fail (GMIME_IS_TNEF_PROPERTY_WRITER (writer), FALSE);
	g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

	if (!check_value_type (writer, GMIME_TNEF_PROPERTY_TYPE_CURRENCY, GMIME_TNEF_PROPERTY_TYPE_CURRENCY, error))
		return FALSE;

	_g_mime_tnef_put_uint64 (bytes, (guint64) scaled_value);

	return write_fixed_value (writer, bytes, sizeof bytes, error);
}

/**
 * g_mime_tnef_writer_get_codepage:
 * @writer: a TNEF writer
 *
 * Gets the output codepage.
 *
 * Returns: the output codepage
 **/
int
g_mime_tnef_writer_get_codepage (GMimeTnefWriter *writer)
{
	g_return_val_if_fail (GMIME_IS_TNEF_WRITER (writer), 0);

	return writer->codepage;
}

/**
 * g_mime_tnef_writer_get_legacy_key:
 * @writer: a TNEF writer
 *
 * Gets the legacy key.
 *
 * Returns: the legacy key
 **/
guint16
g_mime_tnef_writer_get_legacy_key (GMimeTnefWriter *writer)
{
	g_return_val_if_fail (GMIME_IS_TNEF_WRITER (writer), 0);

	return writer->legacy_key;
}

/**
 * g_mime_tnef_property_writer_get_row_count:
 * @writer: a TNEF writer
 *
 * Gets the number of recipient rows started.
 *
 * Returns: the number of recipient rows started
 **/
int
g_mime_tnef_property_writer_get_row_count (GMimeTnefPropertyWriter *writer)
{
	g_return_val_if_fail (GMIME_IS_TNEF_PROPERTY_WRITER (writer), 0);

	return writer->row_count;
}

/**
 * g_mime_tnef_property_writer_get_property_count:
 * @writer: a TNEF writer
 *
 * Gets the number of properties in the list or current row.
 *
 * Returns: the number of properties in the list or current row
 **/
int
g_mime_tnef_property_writer_get_property_count (GMimeTnefPropertyWriter *writer)
{
	g_return_val_if_fail (GMIME_IS_TNEF_PROPERTY_WRITER (writer), 0);

	return writer->property_count;
}
