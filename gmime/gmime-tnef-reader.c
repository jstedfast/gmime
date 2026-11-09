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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <errno.h>
#include <math.h>
#include <string.h>

#include "gmime-error.h"
#include "gmime-tnef-private.h"

/**
 * SECTION: gmime-tnef-reader
 * @title: TNEF readers
 * @short_description: Forward-only TNEF attributes and MAPI properties
 *
 * A #GMimeTnefReader iterates attributes without seeking. For MapiProperties,
 * Attachment and RecipientTable attributes, obtain a #GMimeTnefPropertyReader.
 * Reading a property positions its first value; advance values explicitly
 * only for subsequent values. Recipient tables require advancing rows first.
 *
 * Fixed-width scalar values may be reread. Variable-length values are claimed
 * once, whether decoded, materialized, streamed or opened as embedded TNEF.
 * Advancing the enclosing property or attribute invalidates its child streams
 * and embedded readers, even if those readers have buffered input.
 *
 * Malformed input is recovered where framing allows and reported through
 * #GMimeTnefComplianceLogger. A false iteration result with no #GError means
 * end-of-input or a malformed structure that cannot be continued; it does not
 * prove that input was valid. Checksums are verified on the next attribute
 * advance, so iterate to the end to validate the last attribute.
 *
 * Property snapshots use #GValue, UTF-8 strings, #GBytes and #GDateTime.
 * Currency uses exact signed 64-bit units of 1/10000. FILETIME's 100 ns
 * precision is reduced to microseconds. C strings end at their first NUL;
 * read raw bytes when embedded NULs must be preserved.
 **/

typedef enum {
	READER_INITIAL,
	READER_ATTRIBUTE,
	READER_DONE
} ReaderState;

typedef enum {
	VALUE_UNCLAIMED,
	VALUE_SCALAR,
	VALUE_RAW,
	VALUE_PROPERTIES
} ValueClaim;

struct _GMimeTnefReader {
	GObject parent_instance;
	GMimeStream *stream;
	GMimeTnefOptions *options;
	GMimeTnefComplianceOptions *compliance;
	GMimeTnefComplianceLogger logger;
	gpointer logger_data;
	GDestroyNotify logger_destroy;
	GMimeTnefReader *parent, *root;
	GMimeTnefPropertyReader *properties;
	guint64 parent_generation, generation, value_generation;
	guint64 total_data_bytes;
	guint issue_counts[23], issue_limit, depth;
	gboolean reported_limit, started, seen_attachment, has_oem_codepage;
	gboolean eof, truncated, skip_iid, value_skipped;
	int io_errno, codepage, scalar_length;
	GMimeTnefAttachMethod attach_method;
	char *charset;
	ReaderState state;
	ValueClaim claim;
	GMimeTnefAttributeLevel level;
	GMimeTnefAttributeTag tag;
	guint length;
	guint16 legacy_key, checksum;
	gint64 offset, base_offset, attribute_offset, value_end;
	guint8 input[4096], scalar[14];
	gsize input_index, input_end;
	GDateTime *date;
};

struct _GMimeTnefPropertyReader {
	GObject parent_instance;
	GMimeTnefReader *reader;
	guint64 generation;
	gboolean is_table, in_row, stopped, has_property, has_value, consumed;
	int row_count, row_index, property_count, property_index, value_count, value_index;
	GMimeTnefPropertyTag tag;
	GMimeTnefNameId *name;
	gint64 data_start, data_end, value_end;
	guint8 scratch[16], object_iid[16];
	GDateTime *date;
};

typedef struct {
	GMimeStream parent_instance;
	GMimeTnefReader *reader;
	guint64 generation;
	gint64 end_offset;
	GMimeTnefPropertyTag tag;
	gboolean closed;
} TnefReadStream;

typedef struct {
	GMimeStreamClass parent_class;
} TnefReadStreamClass;

static GType tnef_read_stream_get_type (void);
static gboolean reader_valid (GMimeTnefReader *reader, GError **error);
static gboolean property_valid (GMimeTnefPropertyReader *properties, GError **error);
static gboolean advance_value (GMimeTnefPropertyReader *properties, GError **error);
static gboolean skip_property (GMimeTnefPropertyReader *properties, GError **error);

G_DEFINE_TYPE (GMimeTnefReader, g_mime_tnef_reader, G_TYPE_OBJECT)
G_DEFINE_TYPE (GMimeTnefPropertyReader, g_mime_tnef_property_reader, G_TYPE_OBJECT)
G_DEFINE_TYPE (TnefReadStream, tnef_read_stream, GMIME_TYPE_STREAM)

static const guint8 iid_imessage[16] = { 0x07, 0x03, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00,
					 0xc0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x46 };

static void
log_issue (GMimeTnefReader *reader, GMimeTnefComplianceViolation violation, gint64 offset, GMimeTnefPropertyTag tag)
{
	GMimeTnefReader *root = reader->root;
	guint index = (guint) violation;
	GMimeTnefComplianceIssue issue;

	if (!root->logger)
		return;

	/* One counter per violation. Limits and counters are shared by embedded readers. */
	if (root->issue_limit && index < G_N_ELEMENTS (root->issue_counts)) {
		if (root->issue_counts[index] >= root->issue_limit) {
			/* The report becomes incomplete at the first suppressed issue. Emit the marker only once. */
			if (root->reported_limit)
				return;

			root->reported_limit = TRUE;
			violation = GMIME_TNEF_COMPLIANCE_VIOLATION_TOO_MANY_COMPLIANCE_ISSUES;
		} else {
			root->issue_counts[index]++;
		}
	}

	issue.violation = violation;
	issue.severity = g_mime_tnef_compliance_violation_get_severity (violation);
	issue.categories = g_mime_tnef_compliance_violation_get_categories (violation);
	issue.description = g_mime_tnef_compliance_violation_get_description (violation);
	issue.stream_offset = reader->base_offset + offset;
	issue.depth = reader->depth;
	issue.attribute_tag = reader->tag;
	issue.property_tag = tag;

	root->logger (&issue, root->logger_data);
}

static void
set_truncated (GMimeTnefReader *reader, GMimeTnefPropertyTag tag)
{
	if (!reader->truncated && !reader->io_errno) {
		reader->truncated = TRUE;
		log_issue (reader, GMIME_TNEF_COMPLIANCE_VIOLATION_TRUNCATED_STREAM, reader->offset, tag);
	}
}

static gboolean
reader_valid (GMimeTnefReader *reader, GError **error)
{
	if (reader->parent) {
		if (!reader_valid (reader->parent, error))
			return FALSE;

		if (reader->parent_generation != reader->parent->value_generation)
			return _g_mime_tnef_invalid_operation (error,
							       "The parent reader has advanced beyond this embedded message.");
	}

	if (reader->io_errno) {
		errno = reader->io_errno;
		return _g_mime_tnef_io_error (error);
	}

	return TRUE;
}

static gboolean
fill (GMimeTnefReader *reader, gsize count, GError **error)
{
	g_assert (count <= sizeof reader->input);

	if (!reader_valid (reader, error))
		return FALSE;

	if (reader->input_end - reader->input_index >= count)
		return TRUE;

	if (reader->input_index) {
		reader->input_end -= reader->input_index;
		memmove (reader->input, reader->input + reader->input_index, reader->input_end);
		reader->input_index = 0;
	}

	while (reader->input_end < count && !reader->eof) {
		ssize_t nread = g_mime_stream_read (reader->stream, (char *) reader->input + reader->input_end,
						    sizeof reader->input - reader->input_end);

		if (nread < 0) {
			reader->io_errno = errno ? errno : EIO;
			errno = reader->io_errno;
			return _g_mime_tnef_io_error (error);
		}

		if (!nread)
			reader->eof = TRUE;
		else
			reader->input_end += nread;
	}

	return reader->input_end >= count;
}

static void
take_bytes (GMimeTnefReader *reader, guint8 *buffer, gsize count)
{
	const guint8 *input = reader->input + reader->input_index;

	g_assert (count <= reader->input_end - reader->input_index);

	if (buffer)
		memcpy (buffer, input, count);

	if (reader->state == READER_ATTRIBUTE && reader->offset < reader->value_end) {
		gsize length = MIN ((gint64) count, reader->value_end - reader->offset);
		reader->checksum = g_mime_tnef_checksum_update (reader->checksum, input, length);
	}

	reader->input_index += count;
	reader->offset += count;
}

static gboolean
skip_bytes (GMimeTnefReader *reader, gint64 count, GMimeTnefPropertyTag tag, GError **error)
{
	while (count > 0) {
		gsize length;

		if (!fill (reader, 1, error)) {
			set_truncated (reader, tag);
			return FALSE;
		}

		length = MIN (count, (gint64) (reader->input_end - reader->input_index));
		take_bytes (reader, NULL, length);
		count -= length;
	}

	return TRUE;
}

static gboolean
read_exact (GMimeTnefReader *reader, guint8 *buffer, gsize length, GMimeTnefPropertyTag tag, GError **error)
{
	if (!fill (reader, length, error)) {
		set_truncated (reader, tag);
		return FALSE;
	}

	take_bytes (reader, buffer, length);

	return TRUE;
}

static GBytes *
read_value_bytes (GMimeTnefReader *reader, guint length, GMimeTnefPropertyTag tag, GError **error)
{
	GMimeTnefReader *root = reader->root;
	guint remaining = length;
	GByteArray *bytes;

	reader->value_skipped = FALSE;

	if (length > reader->options->max_property_value_length ||
	    length > root->options->max_total_data_bytes - root->total_data_bytes) {
		reader->value_skipped = TRUE;

		log_issue (reader, GMIME_TNEF_COMPLIANCE_VIOLATION_DATA_SIZE_LIMIT_EXCEEDED, reader->offset, tag);

		if (!skip_bytes (reader, length, tag, error) && reader->io_errno)
			return NULL;

		return g_bytes_new (NULL, 0);
	}

	root->total_data_bytes += length;

	/* A claimed length must not force a large up-front allocation before its data is known to exist. */
	bytes = g_byte_array_sized_new (MIN (length, 65536));

	while (remaining) {
		gsize count;

		if (!fill (reader, 1, error)) {
			root->total_data_bytes -= remaining;

			if (reader->io_errno) {
				g_byte_array_unref (bytes);
				return NULL;
			}

			set_truncated (reader, tag);
			break;
		}

		count = MIN (remaining, reader->input_end - reader->input_index);
		g_byte_array_append (bytes, reader->input + reader->input_index, count);
		take_bytes (reader, NULL, count);
		remaining -= count;
	}

	return g_byte_array_free_to_bytes (bytes);
}

static void
set_codepage (GMimeTnefReader *reader, int codepage, gboolean oem)
{
	char *charset;

	if (codepage == 0 || (!oem && (reader->has_oem_codepage || codepage <= 0)))
		return;

	/* UTF-16 and UTF-32 contain embedded zero bytes and cannot supply an InternetCodepage override. */
	if (!oem && (codepage == 1200 || codepage == 1201 || codepage == 12000 || codepage == 12001))
		return;

	charset = _g_mime_tnef_get_charset (codepage);

	if (!charset) {
		if (oem)
			log_issue (reader, GMIME_TNEF_COMPLIANCE_VIOLATION_INVALID_MESSAGE_CODEPAGE,
				   reader->attribute_offset, 0);
		return;
	}

	g_free (reader->charset);

	reader->charset = charset;
	reader->codepage = codepage;
	reader->has_oem_codepage |= oem;
}

static gboolean
claim_value (GMimeTnefReader *reader, ValueClaim claim, GError **error)
{
	if (!reader_valid (reader, error))
		return FALSE;

	if (reader->state != READER_ATTRIBUTE)
		return _g_mime_tnef_invalid_operation (error, "The reader is not positioned on an attribute.");

	if (reader->claim != VALUE_UNCLAIMED && (reader->claim != VALUE_SCALAR || claim != VALUE_SCALAR))
		return _g_mime_tnef_invalid_operation (error, "The attribute value has already been claimed.");

	reader->claim = claim;

	return TRUE;
}

static void
g_mime_tnef_reader_finalize (GObject *object)
{
	GMimeTnefReader *reader = GMIME_TNEF_READER (object);

	g_clear_object (&reader->properties);
	g_clear_object (&reader->stream);
	g_clear_object (&reader->parent);
	g_clear_pointer (&reader->date, g_date_time_unref);

	g_mime_tnef_options_unref (reader->options);
	g_mime_tnef_compliance_options_unref (reader->compliance);

	if (reader->logger_destroy)
		reader->logger_destroy (reader->logger_data);

	g_free (reader->charset);

	G_OBJECT_CLASS (g_mime_tnef_reader_parent_class)->finalize (object);
}

static void
g_mime_tnef_reader_class_init (GMimeTnefReaderClass *klass)
{
	G_OBJECT_CLASS (klass)->finalize = g_mime_tnef_reader_finalize;
}

static void
g_mime_tnef_reader_init (GMimeTnefReader *reader)
{
	reader->options = g_mime_tnef_options_new ();
	reader->compliance = g_mime_tnef_compliance_options_new ();
	reader->root = reader;
	reader->scalar_length = -1;
	reader->codepage = 1252;
	reader->charset = g_strdup ("WINDOWS-1252");
	reader->attach_method = GMIME_TNEF_ATTACH_METHOD_BY_VALUE;
}

/**
 * g_mime_tnef_reader_new:
 * @stream: the input stream
 * @options: (nullable): options, or %NULL for defaults
 *
 * Creates a forward-only reader without reading any input. The reader takes
 * a reference to @stream but does not close it. Stream offsets are relative
 * to its initial position, not to the underlying stream's origin.
 *
 * Returns: (transfer full): a new reader
 **/
GMimeTnefReader *
g_mime_tnef_reader_new (GMimeStream *stream, const GMimeTnefOptions *options)
{
	GMimeTnefReader *reader;
	char *charset;

	g_return_val_if_fail (GMIME_IS_STREAM (stream), NULL);

	reader = g_object_new (GMIME_TYPE_TNEF_READER, NULL);
	reader->stream = g_object_ref (stream);

	if (options) {
		g_mime_tnef_options_unref (reader->options);
		reader->options = g_mime_tnef_options_clone (options);

		if ((charset = _g_mime_tnef_get_charset (options->default_codepage))) {
			g_free (reader->charset);
			reader->charset = charset;
			reader->codepage = options->default_codepage ? options->default_codepage : 1252;
		}
	}

	return reader;
}

/**
 * g_mime_tnef_reader_set_compliance_logger:
 * @reader: a TNEF reader
 * @logger: (scope notified) (closure user_data) (destroy destroy) (nullable): diagnostic callback
 * @user_data: (nullable): callback data
 * @destroy: (nullable): callback-data destroy function
 *
 * Sets the callback before reading starts. Embedded readers inherit the
 * outer reader's callback and diagnostic budget.
 **/
void
g_mime_tnef_reader_set_compliance_logger (GMimeTnefReader *reader, GMimeTnefComplianceLogger logger, gpointer user_data,
					  GDestroyNotify destroy)
{
	g_return_if_fail (GMIME_IS_TNEF_READER (reader));
	g_return_if_fail (!reader->started && !reader->parent);

	if (reader->logger_destroy)
		reader->logger_destroy (reader->logger_data);

	reader->logger = logger;
	reader->logger_data = user_data;
	reader->logger_destroy = destroy;
}

/**
 * g_mime_tnef_reader_set_compliance_options:
 * @reader: a TNEF reader
 * @options: the reporting options
 *
 * Copies reporting options before reading starts. The cap is snapshotted
 * when reading starts and shared with all embedded readers.
 **/
void
g_mime_tnef_reader_set_compliance_options (GMimeTnefReader *reader, const GMimeTnefComplianceOptions *options)
{
	GMimeTnefComplianceOptions *copy;

	g_return_if_fail (GMIME_IS_TNEF_READER (reader));
	g_return_if_fail (!reader->started && !reader->parent);
	g_return_if_fail (options != NULL);

	copy = g_mime_tnef_compliance_options_clone (options);
	g_mime_tnef_compliance_options_unref (reader->compliance);
	reader->compliance = copy;
}

static gboolean
read_header (GMimeTnefReader *reader, GError **error)
{
	guint8 header[16];

	/* Embedded data begins with the 16-byte IID of the IMessage interface. */
	if (reader->skip_iid && !read_exact (reader, header, 16, 0, error))
		return FALSE;

	if (!read_exact (reader, header, 4, 0, error))
		return FALSE;

	if (_g_mime_tnef_get_uint32 (header) != 0x223e9f78) {
		/* With a wrong signature, nothing meaningful remains to read. */
		log_issue (reader, GMIME_TNEF_COMPLIANCE_VIOLATION_INVALID_SIGNATURE, reader->offset - 4, 0);
		return FALSE;
	}

	if (!read_exact (reader, header, 2, 0, error))
		return FALSE;

	reader->legacy_key = _g_mime_tnef_get_uint16 (header);

	return TRUE;
}

static gboolean
read_attribute_header (GMimeTnefReader *reader, GError **error)
{
	guint peek = 0, index;
	int expected_level;
	guint8 header[9];
	guint32 length;

	reader->attribute_offset = reader->offset;

	/* End-of-stream between attributes is the normal terminator. */
	if (!fill (reader, 1, error))
		return FALSE;

	if (!read_exact (reader, header, sizeof header, 0, error))
		return FALSE;

	reader->level = header[0];
	reader->tag = _g_mime_tnef_get_uint32 (header + 1);
	length = _g_mime_tnef_get_uint32 (header + 5);
	reader->state = READER_ATTRIBUTE;

	if (reader->level == GMIME_TNEF_ATTRIBUTE_LEVEL_ATTACHMENT) {
		reader->seen_attachment = TRUE;
	} else if (reader->level == GMIME_TNEF_ATTRIBUTE_LEVEL_MESSAGE) {
		/* Once attachment attributes begin, the stream cannot go back to message attributes. */
		if (reader->seen_attachment)
			log_issue (reader, GMIME_TNEF_COMPLIANCE_VIOLATION_MESSAGE_ATTRIBUTE_AFTER_ATTACHMENT,
				   reader->attribute_offset, 0);
	} else {
		log_issue (reader, GMIME_TNEF_COMPLIANCE_VIOLATION_INVALID_ATTRIBUTE_LEVEL, reader->attribute_offset,
			   0);
	}

	if (length > G_MAXINT32 || reader->offset > G_MAXINT64 - length) {
		log_issue (reader, GMIME_TNEF_COMPLIANCE_VIOLATION_INVALID_ATTRIBUTE_LENGTH, reader->attribute_offset,
			   0);
		return FALSE;
	}

	reader->length = length;
	reader->value_end = reader->offset + length;
	expected_level = _g_mime_tnef_get_attribute_level (reader->tag);

	if (reader->tag && !expected_level) {
		log_issue (reader, GMIME_TNEF_COMPLIANCE_VIOLATION_UNKNOWN_ATTRIBUTE, reader->attribute_offset, 0);
		return TRUE;
	}

	if (reader->tag && (reader->level == 1 || reader->level == 2) && expected_level != reader->level)
		log_issue (reader, GMIME_TNEF_COMPLIANCE_VIOLATION_ATTRIBUTE_LEVEL_MISMATCH, reader->attribute_offset, 0);

	switch (reader->tag) {
	case GMIME_TNEF_ATTRIBUTE_TAG_ATTACH_RENDER_DATA:
		reader->attach_method = GMIME_TNEF_ATTACH_METHOD_BY_VALUE;
		break;
	case GMIME_TNEF_ATTRIBUTE_TAG_MESSAGE_CLASS:
	case GMIME_TNEF_ATTRIBUTE_TAG_ORIGINAL_MESSAGE_CLASS:
		if (!length || length > 255)
			log_issue (reader, GMIME_TNEF_COMPLIANCE_VIOLATION_INVALID_MESSAGE_CLASS,
				   reader->attribute_offset, 0);
		else
			peek = length;
		break;
	case GMIME_TNEF_ATTRIBUTE_TAG_OEM_CODEPAGE:
	case GMIME_TNEF_ATTRIBUTE_TAG_TNEF_VERSION:
		if (length < 4)
			log_issue (reader, GMIME_TNEF_COMPLIANCE_VIOLATION_INVALID_ATTRIBUTE_VALUE,
				   reader->attribute_offset, 0);
		else
			peek = 4;
		break;
	default:
		break;
	}

	if (peek && fill (reader, peek, error)) {
		const guint8 *data = reader->input + reader->input_index;

		if (reader->tag == GMIME_TNEF_ATTRIBUTE_TAG_OEM_CODEPAGE) {
			/* A nonzero OEM codepage takes precedence over PidTagInternetCodepage. */
			set_codepage (reader, (gint32) _g_mime_tnef_get_uint32 (data), TRUE);
		} else if (reader->tag == GMIME_TNEF_ATTRIBUTE_TAG_TNEF_VERSION) {
			if (_g_mime_tnef_get_uint32 (data) != 0x00010000)
				log_issue (reader, GMIME_TNEF_COMPLIANCE_VIOLATION_UNSUPPORTED_VERSION,
					   reader->attribute_offset, 0);
		} else {
			for (index = 0; index < peek; index++) {
				/* A single nul-terminator is allowed at the end of the value. */
				if (!data[index] && index + 1 == peek)
					break;

				if (data[index] < 0x20 || data[index] > 0x7e) {
					log_issue (reader, GMIME_TNEF_COMPLIANCE_VIOLATION_INVALID_MESSAGE_CLASS,
						   reader->attribute_offset, 0);
					break;
				}
			}
		}
	}

	return !reader->io_errno;
}

/**
 * g_mime_tnef_reader_read:
 * @reader: a TNEF reader
 * @error: return location for an error
 *
 * Advances to the next attribute, skipping unread bytes and checking the
 * previous checksum. Existing child streams and property readers become
 * invalid. Malformed input is reported through the compliance logger.
 *
 * Returns: %TRUE for an attribute; %FALSE at end-of-input or on error
 **/
gboolean
g_mime_tnef_reader_read (GMimeTnefReader *reader, GError **error)
{
	guint8 checksum[2];
	gboolean success;

	g_return_val_if_fail (GMIME_IS_TNEF_READER (reader), FALSE);
	g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

	if (!reader_valid (reader, error))
		return FALSE;

	reader->generation++;
	reader->value_generation++;
	g_clear_object (&reader->properties);

	if (!reader->started) {
		reader->started = TRUE;
		reader->issue_limit = reader->compliance->max_issues_per_violation;
	}

	if (reader->state == READER_DONE)
		return FALSE;

	if (reader->state == READER_INITIAL) {
		success = read_header (reader, error);
	} else {
		success = !reader->truncated && skip_bytes (reader, reader->value_end - reader->offset, 0, error) &&
			  read_exact (reader, checksum, 2, 0, error);

		if (success && _g_mime_tnef_get_uint16 (checksum) != reader->checksum)
			log_issue (reader, GMIME_TNEF_COMPLIANCE_VIOLATION_ATTRIBUTE_CHECKSUM_MISMATCH,
				   reader->attribute_offset, 0);
	}

	reader->state = READER_INITIAL;
	reader->claim = VALUE_UNCLAIMED;
	reader->checksum = 0;
	reader->scalar_length = -1;
	reader->length = 0;
	reader->tag = 0;
	reader->level = 0;

	g_clear_pointer (&reader->date, g_date_time_unref);

	if (success)
		success = read_attribute_header (reader, error);

	if (!success) {
		reader->state = READER_DONE;
		reader->tag = 0;
		reader->level = 0;
		reader->length = 0;
	}

	return success;
}

static gboolean
ensure_scalar (GMimeTnefReader *reader, gboolean date, GError **error)
{
	GMimeTnefAttributeType type = reader->tag & 0xffff0000;
	guint width, count;
	gboolean filled;

	switch (type) {
	case GMIME_TNEF_ATTRIBUTE_TYPE_SHORT:
	case GMIME_TNEF_ATTRIBUTE_TYPE_WORD:
		width = 2;
		break;
	case GMIME_TNEF_ATTRIBUTE_TYPE_LONG:
	case GMIME_TNEF_ATTRIBUTE_TYPE_D_WORD:
		width = 4;
		break;
	case GMIME_TNEF_ATTRIBUTE_TYPE_DATE:
		if (date) {
			width = 14;
			break;
		}
		G_GNUC_FALLTHROUGH;
	default:
		return _g_mime_tnef_invalid_operation (error, "The attribute cannot be read as this scalar type.");
	}

	if (!claim_value (reader, VALUE_SCALAR, error))
		return FALSE;

	if (reader->scalar_length >= 0)
		return TRUE;

	count = MIN (reader->length, width);
	filled = fill (reader, count, error);

	if (reader->io_errno)
		return FALSE;

	if (!filled) {
		count = MIN (count, reader->input_end - reader->input_index);
		set_truncated (reader, 0);
	}

	memset (reader->scalar, 0, sizeof reader->scalar);
	take_bytes (reader, reader->scalar, count);
	reader->scalar_length = count;

	if (reader->length < width) {
		log_issue (reader, GMIME_TNEF_COMPLIANCE_VIOLATION_INVALID_ATTRIBUTE_VALUE, reader->attribute_offset,
			   0);
		memset (reader->scalar, 0, sizeof reader->scalar);
	} else if (count == 14 && type == GMIME_TNEF_ATTRIBUTE_TYPE_DATE) {
		/* The TNEF date structure is seven 16-bit values: year, month, day, hour, minute, second, weekday. */
		guint year = _g_mime_tnef_get_uint16 (reader->scalar);
		guint month = _g_mime_tnef_get_uint16 (reader->scalar + 2);
		guint day = _g_mime_tnef_get_uint16 (reader->scalar + 4);
		guint hour = _g_mime_tnef_get_uint16 (reader->scalar + 6);
		guint minute = _g_mime_tnef_get_uint16 (reader->scalar + 8);
		guint second = _g_mime_tnef_get_uint16 (reader->scalar + 10);

		if (year >= 1 && year <= 9999 && g_date_valid_dmy (day, month, year) && hour < 24 && minute < 60 &&
		    second < 60)
			reader->date = g_date_time_new_utc (year, month, day, hour, minute, second);

		if (!reader->date)
			log_issue (reader, GMIME_TNEF_COMPLIANCE_VIOLATION_INVALID_DATE, reader->attribute_offset, 0);
	}

	return TRUE;
}

/**
 * g_mime_tnef_reader_read_value_as_int32:
 * @reader: a TNEF reader
 * @value: (out): the integer value
 * @error: return location for an error
 *
 * Reads a short, word, long or dword attribute. Scalar values may be reread.
 * Malformed values are reported and returned as zero.
 *
 * Returns: whether the operation succeeded
 **/
gboolean
g_mime_tnef_reader_read_value_as_int32 (GMimeTnefReader *reader, gint32 *value, GError **error)
{
	GMimeTnefAttributeType type;

	g_return_val_if_fail (GMIME_IS_TNEF_READER (reader), FALSE);
	g_return_val_if_fail (value != NULL, FALSE);
	g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

	if (!ensure_scalar (reader, FALSE, error))
		return FALSE;

	type = reader->tag & 0xffff0000;

	if (type == GMIME_TNEF_ATTRIBUTE_TYPE_SHORT)
		*value = reader->scalar_length >= 2 ? (gint16) _g_mime_tnef_get_uint16 (reader->scalar) : 0;
	else if (type == GMIME_TNEF_ATTRIBUTE_TYPE_WORD)
		*value = reader->scalar_length >= 2 ? _g_mime_tnef_get_uint16 (reader->scalar) : 0;
	else
		*value = reader->scalar_length >= 4 ? (gint32) _g_mime_tnef_get_uint32 (reader->scalar) : 0;

	return TRUE;
}

/**
 * g_mime_tnef_reader_read_value_as_int16:
 * @reader: a TNEF reader
 * @value: (out): the integer value
 * @error: return location for an error
 *
 * Reads an integer attribute, retaining its low 16 bits.
 *
 * Returns: whether the operation succeeded
 **/
gboolean
g_mime_tnef_reader_read_value_as_int16 (GMimeTnefReader *reader, gint16 *value, GError **error)
{
	gint32 number;

	g_return_val_if_fail (value != NULL, FALSE);

	if (!g_mime_tnef_reader_read_value_as_int32 (reader, &number, error))
		return FALSE;

	*value = (gint16) number;

	return TRUE;
}

/**
 * g_mime_tnef_reader_read_value_as_date_time:
 * @reader: a TNEF reader
 * @error: return location for an error
 *
 * Reads a date attribute. Legacy dates have no timezone; their fields are
 * represented as UTC without a timezone conversion. Invalid dates are
 * reported and represented by 0001-01-01.
 *
 * Returns: (transfer full) (nullable): the date, or %NULL on error
 **/
GDateTime *
g_mime_tnef_reader_read_value_as_date_time (GMimeTnefReader *reader, GError **error)
{
	g_return_val_if_fail (GMIME_IS_TNEF_READER (reader), NULL);
	g_return_val_if_fail (error == NULL || *error == NULL, NULL);

	if ((reader->tag & 0xffff0000) != GMIME_TNEF_ATTRIBUTE_TYPE_DATE) {
		_g_mime_tnef_invalid_operation (error, "The attribute is not a date.");
		return NULL;
	}

	if (!ensure_scalar (reader, TRUE, error))
		return NULL;

	return reader->date ? g_date_time_ref (reader->date) : g_date_time_new_utc (1, 1, 1, 0, 0, 0);
}

/**
 * g_mime_tnef_reader_read_value_as_bytes:
 * @reader: a TNEF reader
 * @error: return location for an error
 *
 * Claims and materializes an attribute's raw bytes. Exceeding a configured
 * data limit reports a diagnostic, skips the value and returns empty bytes.
 *
 * Returns: (transfer full) (nullable): the bytes, or %NULL on error
 **/
GBytes *
g_mime_tnef_reader_read_value_as_bytes (GMimeTnefReader *reader, GError **error)
{
	g_return_val_if_fail (GMIME_IS_TNEF_READER (reader), NULL);
	g_return_val_if_fail (error == NULL || *error == NULL, NULL);

	if (!claim_value (reader, VALUE_RAW, error))
		return NULL;

	return read_value_bytes (reader, reader->value_end - reader->offset, 0, error);
}

/**
 * g_mime_tnef_reader_read_value_as_string:
 * @reader: a TNEF reader
 * @error: return location for an error
 *
 * Claims and decodes a string, text, or message-class attribute as UTF-8.
 * Trailing nul terminators are removed.
 *
 * Returns: (transfer full) (nullable): the string, or %NULL on error
 **/
char *
g_mime_tnef_reader_read_value_as_string (GMimeTnefReader *reader, GError **error)
{
	GMimeTnefAttributeType type;
	GBytes *bytes;
	char *text;

	g_return_val_if_fail (GMIME_IS_TNEF_READER (reader), NULL);
	g_return_val_if_fail (error == NULL || *error == NULL, NULL);

	type = reader->tag & 0xffff0000;

	if (type != GMIME_TNEF_ATTRIBUTE_TYPE_STRING && type != GMIME_TNEF_ATTRIBUTE_TYPE_TEXT &&
	    reader->tag != GMIME_TNEF_ATTRIBUTE_TAG_MESSAGE_CLASS &&
	    reader->tag != GMIME_TNEF_ATTRIBUTE_TAG_ORIGINAL_MESSAGE_CLASS) {
		_g_mime_tnef_invalid_operation (error, "The attribute is not a string.");
		return NULL;
	}

	if (!(bytes = g_mime_tnef_reader_read_value_as_bytes (reader, error)))
		return NULL;

	text = _g_mime_tnef_decode_string (reader->charset, bytes, FALSE);
	g_bytes_unref (bytes);

	return text;
}

static gboolean
read_stream_valid (TnefReadStream *stream)
{
	if (stream->closed || stream->reader->value_generation != stream->generation ||
	    !reader_valid (stream->reader, NULL)) {
		errno = stream->closed ? EBADF : EINVAL;
		return FALSE;
	}

	return TRUE;
}

static ssize_t
read_stream_read (GMimeStream *base, char *buffer, size_t length)
{
	TnefReadStream *stream = (TnefReadStream *) base;
	GMimeTnefReader *reader = stream->reader;
	gsize count;

	if (!read_stream_valid (stream))
		return -1;

	if (!length || reader->offset >= stream->end_offset)
		return 0;

	if (!fill (reader, 1, NULL)) {
		if (reader->io_errno) {
			errno = reader->io_errno;
			return -1;
		}

		set_truncated (reader, stream->tag);
		return 0;
	}

	count = MIN (length, reader->input_end - reader->input_index);
	count = MIN ((gint64) count, stream->end_offset - reader->offset);
	take_bytes (reader, (guint8 *) buffer, count);
	g_mime_stream_set_position (base, g_mime_stream_get_position (base) + count);

	return count;
}

static int
read_stream_close (GMimeStream *base)
{
	((TnefReadStream *) base)->closed = TRUE;
	return 0;
}

static int
read_stream_flush (GMimeStream *base)
{
	return read_stream_valid ((TnefReadStream *) base) ? 0 : -1;
}

static gboolean
read_stream_eos (GMimeStream *base)
{
	TnefReadStream *stream = (TnefReadStream *) base;

	if (!read_stream_valid (stream))
		return TRUE;

	return stream->reader->offset >= stream->end_offset ||
	       (stream->reader->eof && stream->reader->input_index == stream->reader->input_end);
}

static void
read_stream_finalize (GObject *object)
{
	g_clear_object (&((TnefReadStream *) object)->reader);

	G_OBJECT_CLASS (tnef_read_stream_parent_class)->finalize (object);
}

static ssize_t
read_stream_write (GMimeStream *stream, const char *buffer, size_t length)
{
	errno = EBADF;
	return -1;
}

static void
tnef_read_stream_class_init (TnefReadStreamClass *klass)
{
	GMimeStreamClass *stream_class = GMIME_STREAM_CLASS (klass);

	G_OBJECT_CLASS (klass)->finalize = read_stream_finalize;

	stream_class->read = read_stream_read;
	stream_class->write = read_stream_write;
	stream_class->reset = _g_mime_tnef_stream_no_reset;
	stream_class->seek = _g_mime_tnef_stream_no_seek;
	stream_class->substream = _g_mime_tnef_stream_no_substream;
	stream_class->close = read_stream_close;
	stream_class->flush = read_stream_flush;
	stream_class->eos = read_stream_eos;
}

static void
tnef_read_stream_init (TnefReadStream *stream)
{
	g_mime_stream_construct (GMIME_STREAM (stream), 0, -1);
}

static GMimeStream *
open_read_stream (GMimeTnefReader *reader, gint64 end, GMimeTnefPropertyTag tag)
{
	TnefReadStream *stream = g_object_new (tnef_read_stream_get_type (), NULL);

	stream->reader = g_object_ref (reader);
	stream->generation = reader->value_generation;
	stream->end_offset = end;
	stream->tag = tag;

	return GMIME_STREAM (stream);
}

/**
 * g_mime_tnef_reader_open_value_stream:
 * @reader: a TNEF reader
 * @error: return location for an error
 *
 * Claims the attribute and opens a forward-only, read-only stream. It remains
 * valid only until the reader advances. Closing it neither closes the input
 * nor consumes the remainder. Materialization limits do not apply.
 *
 * Returns: (transfer full) (nullable): the stream, or %NULL on error
 **/
GMimeStream *
g_mime_tnef_reader_open_value_stream (GMimeTnefReader *reader, GError **error)
{
	g_return_val_if_fail (GMIME_IS_TNEF_READER (reader), NULL);
	g_return_val_if_fail (error == NULL || *error == NULL, NULL);

	if (!claim_value (reader, VALUE_RAW, error))
		return NULL;

	return open_read_stream (reader, reader->value_end, 0);
}

static void
g_mime_tnef_property_reader_finalize (GObject *object)
{
	GMimeTnefPropertyReader *properties = GMIME_TNEF_PROPERTY_READER (object);

	if (properties->reader)
		g_object_remove_weak_pointer (G_OBJECT (properties->reader), (gpointer *) &properties->reader);

	g_clear_pointer (&properties->name, g_mime_tnef_name_id_unref);
	g_clear_pointer (&properties->date, g_date_time_unref);

	G_OBJECT_CLASS (g_mime_tnef_property_reader_parent_class)->finalize (object);
}

static void
g_mime_tnef_property_reader_class_init (GMimeTnefPropertyReaderClass *klass)
{
	G_OBJECT_CLASS (klass)->finalize = g_mime_tnef_property_reader_finalize;
}

static void
g_mime_tnef_property_reader_init (GMimeTnefPropertyReader *properties)
{
	properties->row_count = -1;
	properties->property_count = -1;
	properties->value_index = -1;
}

/**
 * g_mime_tnef_reader_get_property_reader:
 * @reader: a TNEF reader
 * @error: return location for an error
 *
 * Claims a MapiProperties, Attachment or RecipientTable attribute. Repeated
 * calls return references to the same property reader. The property reader
 * is invalidated when @reader advances or is destroyed.
 *
 * Returns: (transfer full) (nullable): the property reader, or %NULL on error
 **/
GMimeTnefPropertyReader *
g_mime_tnef_reader_get_property_reader (GMimeTnefReader *reader, GError **error)
{
	GMimeTnefPropertyReader *properties;

	g_return_val_if_fail (GMIME_IS_TNEF_READER (reader), NULL);
	g_return_val_if_fail (error == NULL || *error == NULL, NULL);

	if (!reader_valid (reader, error))
		return NULL;

	if (reader->claim == VALUE_PROPERTIES)
		return g_object_ref (reader->properties);

	if (reader->tag != GMIME_TNEF_ATTRIBUTE_TAG_MAPI_PROPERTIES &&
	    reader->tag != GMIME_TNEF_ATTRIBUTE_TAG_ATTACHMENT &&
	    reader->tag != GMIME_TNEF_ATTRIBUTE_TAG_RECIPIENT_TABLE) {
		_g_mime_tnef_invalid_operation (error, "The attribute does not contain MAPI properties.");
		return NULL;
	}

	if (!claim_value (reader, VALUE_PROPERTIES, error))
		return NULL;

	properties = g_object_new (GMIME_TYPE_TNEF_PROPERTY_READER, NULL);
	properties->reader = reader;
	g_object_add_weak_pointer (G_OBJECT (reader), (gpointer *) &properties->reader);
	properties->generation = reader->generation;
	properties->is_table = reader->tag == GMIME_TNEF_ATTRIBUTE_TAG_RECIPIENT_TABLE;
	reader->properties = properties;

	return g_object_ref (properties);
}

static gboolean
property_valid (GMimeTnefPropertyReader *properties, GError **error)
{
	if (!properties->reader || properties->generation != properties->reader->generation)
		return _g_mime_tnef_invalid_operation (error, "The attribute reader has advanced or was destroyed.");

	return reader_valid (properties->reader, error);
}

static void
stop_properties (GMimeTnefPropertyReader *properties)
{
	/* Once the structure is untrustworthy, the remaining properties cannot be located. */
	properties->stopped = TRUE;
	properties->has_property = FALSE;
	properties->has_value = FALSE;
	properties->in_row = FALSE;
}

static void
property_log (GMimeTnefPropertyReader *properties, GMimeTnefComplianceViolation violation, gint64 offset)
{
	log_issue (properties->reader, violation, offset, properties->tag);
}

static gboolean
property_read (GMimeTnefPropertyReader *properties, guint8 *buffer, guint count, GMimeTnefComplianceViolation violation,
	       GError **error)
{
	GMimeTnefReader *reader = properties->reader;

	if ((gint64) count > reader->value_end - reader->offset) {
		property_log (properties, violation, reader->offset);
		stop_properties (properties);
		return FALSE;
	}

	if (!read_exact (reader, buffer, count, properties->tag, error)) {
		stop_properties (properties);
		return FALSE;
	}

	return TRUE;
}

static int
read_count (GMimeTnefPropertyReader *properties, GMimeTnefComplianceViolation violation, GError **error)
{
	GMimeTnefReader *reader = properties->reader;
	gint64 offset = reader->offset;
	gint32 count, maximum;
	guint8 bytes[4];

	if (!property_read (properties, bytes, 4, violation, error))
		return 0;

	count = (gint32) _g_mime_tnef_get_uint32 (bytes);

	/* Every row starts with a 32-bit property count; every property starts with a 32-bit tag. */
	maximum = (reader->value_end - reader->offset) / 4;

	if (count < 0 || count > maximum) {
		property_log (properties, violation, offset);
		count = count < 0 ? 0 : maximum;
	}

	return count;
}

static gboolean
position_next_value (GMimeTnefPropertyReader *properties, GError **error)
{
	GMimeTnefReader *reader = properties->reader;
	GMimeTnefPropertyType type = g_mime_tnef_property_tag_get_value_type (properties->tag);
	int width = _g_mime_tnef_get_fixed_width (type);
	gint64 offset = reader->offset;
	guint8 bytes[4];
	gint32 length;

	reader->value_generation++;
	properties->value_index++;
	properties->consumed = FALSE;
	properties->has_value = FALSE;
	memset (properties->object_iid, 0, 16);
	g_clear_pointer (&properties->date, g_date_time_unref);

	if (width >= 0) {
		if (!property_read (properties, properties->scratch, width,
				    GMIME_TNEF_COMPLIANCE_VIOLATION_INVALID_PROPERTY_LENGTH, error))
			return FALSE;

		properties->data_start = offset;
		properties->data_end = properties->value_end = reader->offset;
		properties->has_value = TRUE;

		if (type == GMIME_TNEF_PROPERTY_TYPE_SYS_TIME || type == GMIME_TNEF_PROPERTY_TYPE_APP_TIME) {
			properties->date = _g_mime_tnef_decode_date (type, properties->scratch);

			if (!properties->date) {
				property_log (properties, GMIME_TNEF_COMPLIANCE_VIOLATION_INVALID_DATE, offset);
				properties->date = g_date_time_new_utc (1, 1, 1, 0, 0, 0);
			}
		} else if (type == GMIME_TNEF_PROPERTY_TYPE_LONG) {
			guint id = properties->tag >> 16;
			gint32 value = (gint32) _g_mime_tnef_get_uint32 (properties->scratch);

			if (id == GMIME_TNEF_PROPERTY_ID_ATTACH_METHOD)
				reader->attach_method = value;
			else if (id == GMIME_TNEF_PROPERTY_ID_INTERNET_CODEPAGE &&
				 !g_mime_tnef_property_tag_is_multi_valued (properties->tag) &&
				 reader->level == GMIME_TNEF_ATTRIBUTE_LEVEL_MESSAGE &&
				 reader->tag == GMIME_TNEF_ATTRIBUTE_TAG_MAPI_PROPERTIES)
				set_codepage (reader, value, FALSE);
		}

		return TRUE;
	}

	if (!property_read (properties, bytes, 4, GMIME_TNEF_COMPLIANCE_VIOLATION_INVALID_PROPERTY_LENGTH, error))
		return FALSE;

	length = (gint32) _g_mime_tnef_get_uint32 (bytes);

	if (length < 0 || length > reader->value_end - reader->offset) {
		property_log (properties, GMIME_TNEF_COMPLIANCE_VIOLATION_INVALID_PROPERTY_LENGTH, offset);
		stop_properties (properties);
		return FALSE;
	}

	properties->data_start = reader->offset;
	properties->data_end = reader->offset + length;
	properties->value_end = properties->data_end + ((4 - (length & 3)) & 3);

	if (properties->value_end > reader->value_end) {
		property_log (properties, GMIME_TNEF_COMPLIANCE_VIOLATION_INVALID_PROPERTY_LENGTH,
			      properties->data_end);
		properties->value_end = reader->value_end;
	}

	properties->has_value = TRUE;

	/* The object IID identifies embedded messages even when AttachMethod has not been read yet. */
	if (type == GMIME_TNEF_PROPERTY_TYPE_OBJECT && length >= 16 && fill (reader, 16, error))
		memcpy (properties->object_iid, reader->input + reader->input_index, 16);

	return !reader->io_errno;
}

static gboolean
advance_value (GMimeTnefPropertyReader *properties, GError **error)
{
	GMimeTnefReader *reader = properties->reader;

	if (!properties->has_property || properties->stopped)
		return FALSE;

	if (properties->has_value) {
		properties->has_value = FALSE;
		reader->value_generation++;

		if (!skip_bytes (reader, properties->value_end - reader->offset, properties->tag, error)) {
			stop_properties (properties);
			return FALSE;
		}
	}

	if (properties->value_index + 1 >= properties->value_count)
		return FALSE;

	return position_next_value (properties, error);
}

static gboolean
skip_property (GMimeTnefPropertyReader *properties, GError **error)
{
	while (advance_value (properties, error)) {
		/* Skip the remaining values of the current property. */
	}

	properties->has_property = FALSE;

	return !properties->stopped && !properties->reader->io_errno;
}

static gboolean
read_name (GMimeTnefPropertyReader *properties, GError **error)
{
	GMimeTnefReader *reader = properties->reader;
	guint32 kind, number;
	char *guid, *name;
	guint8 bytes[20];
	GBytes *data;
	guint padding;
	gint64 offset;

	if (!property_read (properties, bytes, 20, GMIME_TNEF_COMPLIANCE_VIOLATION_INVALID_PROPERTY_LENGTH, error))
		return FALSE;

	guid = _g_mime_tnef_guid_to_string (bytes);
	kind = _g_mime_tnef_get_uint32 (bytes + 16);

	if (kind != GMIME_TNEF_NAME_ID_KIND_ID && kind != GMIME_TNEF_NAME_ID_KIND_NAME) {
		property_log (properties, GMIME_TNEF_COMPLIANCE_VIOLATION_INVALID_NAMED_PROPERTY_KIND,
			      reader->offset - 20);
		properties->name = g_mime_tnef_name_id_new_with_id (guid, 0);
		g_free (guid);
		return TRUE;
	}

	offset = reader->offset;

	if (!property_read (properties, bytes, 4, GMIME_TNEF_COMPLIANCE_VIOLATION_INVALID_PROPERTY_LENGTH, error)) {
		g_free (guid);
		return FALSE;
	}

	number = _g_mime_tnef_get_uint32 (bytes);

	if (kind == GMIME_TNEF_NAME_ID_KIND_ID) {
		properties->name = g_mime_tnef_name_id_new_with_id (guid, (gint32) number);
		g_free (guid);
		return TRUE;
	}

	if (number > G_MAXINT32 || number > reader->value_end - reader->offset) {
		property_log (properties, GMIME_TNEF_COMPLIANCE_VIOLATION_INVALID_PROPERTY_LENGTH, offset);
		stop_properties (properties);
		g_free (guid);
		return FALSE;
	}

	data = read_value_bytes (reader, number, properties->tag, error);

	if (!data || reader->truncated) {
		g_clear_pointer (&data, g_bytes_unref);
		g_free (guid);
		stop_properties (properties);
		return FALSE;
	}

	name = _g_mime_tnef_decode_string (reader->charset, data, TRUE);
	properties->name = g_mime_tnef_name_id_new_with_name (guid, name);
	g_free (name);
	g_free (guid);
	g_bytes_unref (data);

	padding = (4 - (number & 3)) & 3;

	if (padding > reader->value_end - reader->offset) {
		property_log (properties, GMIME_TNEF_COMPLIANCE_VIOLATION_INVALID_PROPERTY_LENGTH, reader->offset);
		padding = reader->value_end - reader->offset;
	}

	if (!skip_bytes (reader, padding, properties->tag, error)) {
		stop_properties (properties);
		return FALSE;
	}

	return TRUE;
}

/**
 * g_mime_tnef_property_reader_read_next_property:
 * @properties: a property reader
 * @error: return location for an error
 *
 * Advances to the next property, skipping unread values and positioning the
 * first value automatically. Recipient tables require a current row.
 *
 * Returns: %TRUE for a property; %FALSE at the end of the list or on error
 **/
gboolean
g_mime_tnef_property_reader_read_next_property (GMimeTnefPropertyReader *properties, GError **error)
{
	GMimeTnefPropertyType type;
	GMimeTnefReader *reader;
	int width, maximum;
	guint8 bytes[4];
	gboolean multi;
	gint64 offset;
	gint32 count;

	g_return_val_if_fail (GMIME_IS_TNEF_PROPERTY_READER (properties), FALSE);
	g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

	if (!property_valid (properties, error) || !skip_property (properties, error))
		return FALSE;

	reader = properties->reader;

	if (!properties->is_table && properties->property_count < 0)
		properties->property_count =
			read_count (properties, GMIME_TNEF_COMPLIANCE_VIOLATION_INVALID_PROPERTY_COUNT, error);

	if (properties->stopped || (properties->is_table && !properties->in_row) ||
	    properties->property_index >= properties->property_count)
		return FALSE;

	properties->property_index++;
	properties->tag = 0;
	offset = reader->offset;

	if (!property_read (properties, bytes, 4, GMIME_TNEF_COMPLIANCE_VIOLATION_INVALID_PROPERTY_LENGTH, error))
		return FALSE;

	properties->tag = _g_mime_tnef_get_uint32 (bytes);
	properties->has_property = TRUE;
	properties->has_value = FALSE;
	properties->consumed = FALSE;
	properties->value_count = 0;
	properties->value_index = -1;

	g_clear_pointer (&properties->name, g_mime_tnef_name_id_unref);

	if (g_mime_tnef_property_tag_is_named (properties->tag) && !read_name (properties, error))
		return FALSE;

	type = g_mime_tnef_property_tag_get_value_type (properties->tag);
	width = _g_mime_tnef_get_fixed_width (type);

	if (width == -2) {
		/* An unknown type has an unknowable length: following properties cannot be located. */
		property_log (properties, GMIME_TNEF_COMPLIANCE_VIOLATION_UNSUPPORTED_PROPERTY_TYPE, offset);
		stop_properties (properties);
		return FALSE;
	}

	multi = g_mime_tnef_property_tag_is_multi_valued (properties->tag);

	if (multi || width == -1) {
		offset = reader->offset;

		if (!property_read (properties, bytes, 4, GMIME_TNEF_COMPLIANCE_VIOLATION_INVALID_PROPERTY_LENGTH,
				    error))
			return FALSE;

		count = (gint32) _g_mime_tnef_get_uint32 (bytes);
		maximum = (reader->value_end - reader->offset) / (width > 0 ? width : 4);

		if (width == 0) {
			/* Multi-valued Null occupies no bytes, so the input cannot bound its count. */
			if (count)
				property_log (properties, GMIME_TNEF_COMPLIANCE_VIOLATION_INVALID_VALUE_COUNT, offset);
			count = 0;
		} else {
			if ((!multi && count != 1) || (multi && (count < 0 || count > maximum)))
				property_log (properties, GMIME_TNEF_COMPLIANCE_VIOLATION_INVALID_VALUE_COUNT, offset);

			/* Honor extra single-valued entries so following properties remain aligned. */
			count = CLAMP (count, 0, maximum);
		}

		properties->value_count = count;
	} else {
		properties->value_count = 1;
	}

	return properties->value_count == 0 || position_next_value (properties, error);
}

/**
 * g_mime_tnef_property_reader_read_next_row:
 * @properties: a recipient-table reader
 * @error: return location for an error
 *
 * Advances to the next recipient row, skipping unread properties.
 *
 * Returns: %TRUE for a row; %FALSE at end-of-table or on error
 **/
gboolean
g_mime_tnef_property_reader_read_next_row (GMimeTnefPropertyReader *properties, GError **error)
{
	g_return_val_if_fail (GMIME_IS_TNEF_PROPERTY_READER (properties), FALSE);
	g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

	if (!property_valid (properties, error))
		return FALSE;

	if (!properties->is_table)
		return _g_mime_tnef_invalid_operation (error, "Only a RecipientTable attribute contains rows.");

	if (properties->stopped)
		return FALSE;

	while (properties->in_row && g_mime_tnef_property_reader_read_next_property (properties, error)) {
		/* Skip any unread properties of the current row. */
	}

	if (properties->stopped || properties->reader->io_errno)
		return FALSE;

	if (properties->row_count < 0)
		properties->row_count =
			read_count (properties, GMIME_TNEF_COMPLIANCE_VIOLATION_INVALID_ROW_COUNT, error);

	properties->in_row = FALSE;

	if (properties->stopped || properties->row_index >= properties->row_count)
		return FALSE;

	properties->row_index++;
	properties->property_index = 0;
	properties->property_count =
		read_count (properties, GMIME_TNEF_COMPLIANCE_VIOLATION_INVALID_PROPERTY_COUNT, error);

	if (properties->stopped)
		return FALSE;

	properties->in_row = TRUE;

	return TRUE;
}

/**
 * g_mime_tnef_property_reader_read_next_value:
 * @properties: a property reader
 * @error: return location for an error
 *
 * Skips the current value and positions the next one. A successful property
 * advance already positions the first value, so call this after processing it.
 *
 * Returns: %TRUE for a value; %FALSE at end-of-property or on error
 **/
gboolean
g_mime_tnef_property_reader_read_next_value (GMimeTnefPropertyReader *properties, GError **error)
{
	g_return_val_if_fail (GMIME_IS_TNEF_PROPERTY_READER (properties), FALSE);
	g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

	return property_valid (properties, error) && advance_value (properties, error);
}

static gboolean
check_value (GMimeTnefPropertyReader *properties, GError **error)
{
	if (!property_valid (properties, error))
		return FALSE;

	if (!properties->has_value)
		return _g_mime_tnef_invalid_operation (error, "The property reader is not positioned on a value.");

	return TRUE;
}

static gboolean
claim_variable (GMimeTnefPropertyReader *properties, GError **error)
{
	if (!check_value (properties, error))
		return FALSE;

	if (_g_mime_tnef_get_fixed_width (g_mime_tnef_property_tag_get_value_type (properties->tag)) != -1)
		return _g_mime_tnef_invalid_operation (error, "The value is not variable-length.");

	if (properties->consumed)
		return _g_mime_tnef_invalid_operation (error, "The value has already been claimed.");

	properties->consumed = TRUE;

	return TRUE;
}

/**
 * g_mime_tnef_property_reader_read_value_as_bytes:
 * @properties: a property reader
 * @error: return location for an error
 *
 * Reads a string, binary, object or GUID as raw bytes. Variable-length values
 * are claimed once and subject to materialization limits. Exceeding a limit
 * reports a diagnostic and returns empty bytes.
 *
 * Returns: (transfer full) (nullable): the bytes, or %NULL on error
 **/
GBytes *
g_mime_tnef_property_reader_read_value_as_bytes (GMimeTnefPropertyReader *properties, GError **error)
{
	g_return_val_if_fail (GMIME_IS_TNEF_PROPERTY_READER (properties), NULL);
	g_return_val_if_fail (error == NULL || *error == NULL, NULL);

	if (!check_value (properties, error))
		return NULL;

	if (g_mime_tnef_property_tag_get_value_type (properties->tag) == GMIME_TNEF_PROPERTY_TYPE_CLASS_ID)
		return g_bytes_new (properties->scratch, 16);

	if (!claim_variable (properties, error))
		return NULL;

	return read_value_bytes (properties->reader, properties->data_end - properties->data_start, properties->tag,
				 error);
}

/**
 * g_mime_tnef_property_reader_read_value_as_string:
 * @properties: a property reader
 * @error: return location for an error
 *
 * Claims and decodes a String8, Unicode or Binary value as UTF-8. Unicode
 * uses UTF-16LE; other types use the current codepage. Trailing nul
 * terminators are removed.
 *
 * Returns: (transfer full) (nullable): the string, or %NULL on error
 **/
char *
g_mime_tnef_property_reader_read_value_as_string (GMimeTnefPropertyReader *properties, GError **error)
{
	GMimeTnefPropertyType type;
	GBytes *bytes;
	char *text;

	g_return_val_if_fail (GMIME_IS_TNEF_PROPERTY_READER (properties), NULL);
	g_return_val_if_fail (error == NULL || *error == NULL, NULL);

	if (!check_value (properties, error))
		return NULL;

	type = g_mime_tnef_property_tag_get_value_type (properties->tag);

	if (type != GMIME_TNEF_PROPERTY_TYPE_STRING8 && type != GMIME_TNEF_PROPERTY_TYPE_UNICODE &&
	    type != GMIME_TNEF_PROPERTY_TYPE_BINARY) {
		_g_mime_tnef_invalid_operation (error, "The property value is not a string or binary value.");
		return NULL;
	}

	if (!(bytes = g_mime_tnef_property_reader_read_value_as_bytes (properties, error)))
		return NULL;

	text = _g_mime_tnef_decode_string (properties->reader->charset, bytes,
					   type == GMIME_TNEF_PROPERTY_TYPE_UNICODE);
	g_bytes_unref (bytes);

	return text;
}

/**
 * g_mime_tnef_property_reader_open_value_stream:
 * @properties: a property reader
 * @error: return location for an error
 *
 * Claims a variable-length value and opens a bounded, forward-only read
 * stream. It is invalidated by advancing either reader. Closing it does
 * not close the input. Materialization limits do not apply.
 *
 * Returns: (transfer full) (nullable): the stream, or %NULL on error
 **/
GMimeStream *
g_mime_tnef_property_reader_open_value_stream (GMimeTnefPropertyReader *properties, GError **error)
{
	g_return_val_if_fail (GMIME_IS_TNEF_PROPERTY_READER (properties), NULL);
	g_return_val_if_fail (error == NULL || *error == NULL, NULL);

	if (!claim_variable (properties, error))
		return NULL;

	return open_read_stream (properties->reader, properties->data_end, properties->tag);
}

/**
 * g_mime_tnef_property_reader_is_embedded_message:
 * @properties: a property reader
 *
 * Checks the object IID, falling back to AttachMethod for binary values
 * and objects without a usable IID.
 *
 * Returns: whether the current value contains an embedded message
 **/
gboolean
g_mime_tnef_property_reader_is_embedded_message (GMimeTnefPropertyReader *properties)
{
	static const guint8 empty[16];
	GMimeTnefPropertyType type;

	g_return_val_if_fail (GMIME_IS_TNEF_PROPERTY_READER (properties), FALSE);
	g_return_val_if_fail (property_valid (properties, NULL), FALSE);

	if (!properties->has_value)
		return FALSE;

	type = g_mime_tnef_property_tag_get_value_type (properties->tag);

	if (type == GMIME_TNEF_PROPERTY_TYPE_OBJECT && memcmp (properties->object_iid, empty, 16))
		return !memcmp (properties->object_iid, iid_imessage, 16);

	return properties->reader->attach_method == GMIME_TNEF_ATTACH_METHOD_EMBEDDED_MESSAGE &&
	       (type == GMIME_TNEF_PROPERTY_TYPE_OBJECT || type == GMIME_TNEF_PROPERTY_TYPE_BINARY);
}

/**
 * g_mime_tnef_property_reader_open_embedded_message:
 * @properties: a property reader
 * @error: return location for an error
 *
 * Claims an embedded message and creates its reader. Options, codepage,
 * materialization and diagnostic budgets are inherited. Offsets remain
 * relative to the outermost stream. Exceeding the nesting limit reports
 * a diagnostic and returns an empty reader.
 *
 * Returns: (transfer full) (nullable): the reader, or %NULL on error
 **/
GMimeTnefReader *
g_mime_tnef_property_reader_open_embedded_message (GMimeTnefPropertyReader *properties, GError **error)
{
	GMimeTnefReader *reader, *child;
	GMimeStream *stream;

	g_return_val_if_fail (GMIME_IS_TNEF_PROPERTY_READER (properties), NULL);
	g_return_val_if_fail (error == NULL || *error == NULL, NULL);

	if (!check_value (properties, error))
		return NULL;

	if (!g_mime_tnef_property_reader_is_embedded_message (properties)) {
		_g_mime_tnef_invalid_operation (error, "The value does not contain an embedded message.");
		return NULL;
	}

	if (!(stream = g_mime_tnef_property_reader_open_value_stream (properties, error)))
		return NULL;

	reader = properties->reader;
	child = g_mime_tnef_reader_new (stream, reader->options);
	g_object_unref (stream);

	child->parent = g_object_ref (reader);
	child->parent_generation = reader->value_generation;
	child->root = reader->root;
	child->depth = reader->depth + 1;
	child->base_offset = reader->base_offset + properties->data_start;
	child->skip_iid = TRUE;
	child->codepage = reader->codepage;
	g_free (child->charset);
	child->charset = g_strdup (reader->charset);

	if (child->depth > reader->options->max_nesting_depth) {
		property_log (properties, GMIME_TNEF_COMPLIANCE_VIOLATION_NESTING_TOO_DEEP, properties->data_start);
		child->state = READER_DONE;
	}

	return child;
}

static double
get_floating_value (GMimeTnefPropertyReader *properties)
{
	guint64 bits64;
	guint32 bits32;
	double number;
	float single;

	if (g_mime_tnef_property_tag_get_value_type (properties->tag) == GMIME_TNEF_PROPERTY_TYPE_R4) {
		bits32 = _g_mime_tnef_get_uint32 (properties->scratch);
		memcpy (&single, &bits32, sizeof single);
		return single;
	}

	bits64 = _g_mime_tnef_get_uint64 (properties->scratch);
	memcpy (&number, &bits64, sizeof number);

	return number;
}

static gboolean
get_integer_value (GMimeTnefPropertyReader *properties, gint64 *value, gboolean boolean, GError **error)
{
	GMimeTnefPropertyType type;
	double number;

	if (!check_value (properties, error))
		return FALSE;

	type = g_mime_tnef_property_tag_get_value_type (properties->tag);

	switch (type) {
	case GMIME_TNEF_PROPERTY_TYPE_BOOLEAN:
		*value = _g_mime_tnef_get_uint16 (properties->scratch);
		break;
	case GMIME_TNEF_PROPERTY_TYPE_I2:
		*value = (gint16) _g_mime_tnef_get_uint16 (properties->scratch);
		break;
	case GMIME_TNEF_PROPERTY_TYPE_LONG:
	case GMIME_TNEF_PROPERTY_TYPE_ERROR:
		*value = (gint32) _g_mime_tnef_get_uint32 (properties->scratch);
		break;
	case GMIME_TNEF_PROPERTY_TYPE_CURRENCY:
		*value = (gint64) _g_mime_tnef_get_uint64 (properties->scratch);
		if (!boolean)
			*value /= 10000;
		break;
	case GMIME_TNEF_PROPERTY_TYPE_I8:
		*value = (gint64) _g_mime_tnef_get_uint64 (properties->scratch);
		break;
	case GMIME_TNEF_PROPERTY_TYPE_R4:
	case GMIME_TNEF_PROPERTY_TYPE_DOUBLE:
		if (boolean)
			return _g_mime_tnef_invalid_operation (error,
							       "A floating-point value cannot be read as a boolean.");

		number = get_floating_value (properties);

		if (!isfinite (number) || number < -9223372036854775808.0 || number >= 9223372036854775808.0)
			return _g_mime_tnef_invalid_operation (error,
							       "The floating-point value is outside the integer range.");

		*value = (gint64) number;
		break;
	default:
		return _g_mime_tnef_invalid_operation (error, "The value is not numeric.");
	}

	return TRUE;
}

/**
 * g_mime_tnef_property_reader_read_value_as_double:
 * @properties: a property reader
 * @value: (out): the numeric value
 * @error: return location for an error
 *
 * Reads any numeric value as a double. Currency is scaled by 1/10000.
 * The fixed-width value may be reread.
 *
 * Returns: whether the operation succeeded
 **/
gboolean
g_mime_tnef_property_reader_read_value_as_double (GMimeTnefPropertyReader *properties, double *value, GError **error)
{
	GMimeTnefPropertyType type;
	gint64 integer;

	g_return_val_if_fail (GMIME_IS_TNEF_PROPERTY_READER (properties), FALSE);
	g_return_val_if_fail (value != NULL, FALSE);
	g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

	if (!check_value (properties, error))
		return FALSE;

	type = g_mime_tnef_property_tag_get_value_type (properties->tag);

	if (type == GMIME_TNEF_PROPERTY_TYPE_DOUBLE || type == GMIME_TNEF_PROPERTY_TYPE_R4) {
		*value = get_floating_value (properties);
	} else if (type == GMIME_TNEF_PROPERTY_TYPE_CURRENCY) {
		*value = (gint64) _g_mime_tnef_get_uint64 (properties->scratch) / 10000.0;
	} else {
		if (!get_integer_value (properties, &integer, FALSE, error))
			return FALSE;

		*value = (double) integer;
	}

	return TRUE;
}

/**
 * g_mime_tnef_property_reader_read_value_as_float:
 * @properties: a property reader
 * @value: (out): the numeric value
 * @error: return location for an error
 *
 * Reads any numeric value as a float, scaling currency by 1/10000.
 *
 * Returns: whether the operation succeeded
 **/
gboolean
g_mime_tnef_property_reader_read_value_as_float (GMimeTnefPropertyReader *properties, float *value, GError **error)
{
	double number;

	g_return_val_if_fail (value != NULL, FALSE);

	if (!g_mime_tnef_property_reader_read_value_as_double (properties, &number, error))
		return FALSE;

	*value = (float) number;

	return TRUE;
}

/**
 * g_mime_tnef_property_reader_read_value_as_date_time:
 * @properties: a property reader
 * @error: return location for an error
 *
 * Reads a SysTime (FILETIME) or AppTime (OLE Automation date) value.
 * Dates are represented in UTC. Invalid dates are reported and returned
 * as 0001-01-01. GLib preserves microseconds rather than FILETIME's 100 ns.
 *
 * Returns: (transfer full) (nullable): the date, or %NULL on error
 **/
GDateTime *
g_mime_tnef_property_reader_read_value_as_date_time (GMimeTnefPropertyReader *properties, GError **error)
{
	g_return_val_if_fail (GMIME_IS_TNEF_PROPERTY_READER (properties), NULL);
	g_return_val_if_fail (error == NULL || *error == NULL, NULL);

	if (!check_value (properties, error))
		return NULL;

	if (!properties->date) {
		_g_mime_tnef_invalid_operation (error, "The value is not a date.");
		return NULL;
	}

	return g_date_time_ref (properties->date);
}

/**
 * g_mime_tnef_property_reader_read_value_as_guid:
 * @properties: a property reader
 * @error: return location for an error
 *
 * Reads a ClassId value as a canonical GUID string.
 *
 * Returns: (transfer full) (nullable): the GUID, or %NULL on error
 **/
char *
g_mime_tnef_property_reader_read_value_as_guid (GMimeTnefPropertyReader *properties, GError **error)
{
	g_return_val_if_fail (GMIME_IS_TNEF_PROPERTY_READER (properties), NULL);
	g_return_val_if_fail (error == NULL || *error == NULL, NULL);

	if (!check_value (properties, error))
		return NULL;

	if (g_mime_tnef_property_tag_get_value_type (properties->tag) != GMIME_TNEF_PROPERTY_TYPE_CLASS_ID) {
		_g_mime_tnef_invalid_operation (error, "The value is not a GUID.");
		return NULL;
	}

	return _g_mime_tnef_guid_to_string (properties->scratch);
}

/**
 * g_mime_tnef_property_reader_read_value:
 * @properties: a property reader
 * @value: (out caller-allocates): an initially unset value
 * @error: return location for an error
 *
 * Reads a value using its native GLib representation: integer, boolean,
 * float, double, string, #GDateTime or #GBytes. GUIDs are strings; Null is
 * a null pointer. Currency is a signed 64-bit integer in units of 1/10000,
 * preserving its exact precision without requiring a decimal type.
 *
 * Returns: whether the operation succeeded
 **/
gboolean
g_mime_tnef_property_reader_read_value (GMimeTnefPropertyReader *properties, GValue *value, GError **error)
{
	GMimeTnefPropertyType type;
	GBytes *bytes;
	char *text;

	g_return_val_if_fail (GMIME_IS_TNEF_PROPERTY_READER (properties), FALSE);
	g_return_val_if_fail (value != NULL && G_VALUE_TYPE (value) == 0, FALSE);
	g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

	if (!check_value (properties, error))
		return FALSE;

	type = g_mime_tnef_property_tag_get_value_type (properties->tag);

	switch (type) {
	case GMIME_TNEF_PROPERTY_TYPE_NULL:
		g_value_init (value, G_TYPE_POINTER);
		g_value_set_pointer (value, NULL);
		break;
	case GMIME_TNEF_PROPERTY_TYPE_BOOLEAN:
		g_value_init (value, G_TYPE_BOOLEAN);
		g_value_set_boolean (value, _g_mime_tnef_get_uint16 (properties->scratch) != 0);
		break;
	case GMIME_TNEF_PROPERTY_TYPE_I2:
		g_value_init (value, G_TYPE_INT);
		g_value_set_int (value, (gint16) _g_mime_tnef_get_uint16 (properties->scratch));
		break;
	case GMIME_TNEF_PROPERTY_TYPE_LONG:
	case GMIME_TNEF_PROPERTY_TYPE_ERROR:
		g_value_init (value, G_TYPE_INT);
		g_value_set_int (value, (gint32) _g_mime_tnef_get_uint32 (properties->scratch));
		break;
	case GMIME_TNEF_PROPERTY_TYPE_I8:
	case GMIME_TNEF_PROPERTY_TYPE_CURRENCY:
		g_value_init (value, G_TYPE_INT64);
		g_value_set_int64 (value, (gint64) _g_mime_tnef_get_uint64 (properties->scratch));
		break;
	case GMIME_TNEF_PROPERTY_TYPE_R4:
		g_value_init (value, G_TYPE_FLOAT);
		g_value_set_float (value, (float) get_floating_value (properties));
		break;
	case GMIME_TNEF_PROPERTY_TYPE_DOUBLE:
		g_value_init (value, G_TYPE_DOUBLE);
		g_value_set_double (value, get_floating_value (properties));
		break;
	case GMIME_TNEF_PROPERTY_TYPE_SYS_TIME:
	case GMIME_TNEF_PROPERTY_TYPE_APP_TIME:
		g_value_init (value, G_TYPE_DATE_TIME);
		g_value_set_boxed (value, properties->date);
		break;
	case GMIME_TNEF_PROPERTY_TYPE_CLASS_ID:
		g_value_init (value, G_TYPE_STRING);
		g_value_take_string (value, _g_mime_tnef_guid_to_string (properties->scratch));
		break;
	case GMIME_TNEF_PROPERTY_TYPE_STRING8:
	case GMIME_TNEF_PROPERTY_TYPE_UNICODE:
		if (!(text = g_mime_tnef_property_reader_read_value_as_string (properties, error)))
			return FALSE;

		g_value_init (value, G_TYPE_STRING);
		g_value_take_string (value, text);
		break;
	default:
		if (!(bytes = g_mime_tnef_property_reader_read_value_as_bytes (properties, error)))
			return FALSE;

		g_value_init (value, G_TYPE_BYTES);
		g_value_take_boxed (value, bytes);
		break;
	}

	return TRUE;
}

/**
 * g_mime_tnef_property_reader_read_property:
 * @properties: a property reader positioned on a property
 * @error: return location for an error
 *
 * Materializes the current property's values into an immutable snapshot.
 * Call before consuming variable-length values or advancing past the first
 * value. If any value exceeds a materialization limit, the snapshot contains
 * no values. Multi-value arrays grow only as actual values are read.
 *
 * Returns: (transfer full) (nullable): the snapshot, or %NULL on error
 **/
GMimeTnefProperty *
g_mime_tnef_property_reader_read_property (GMimeTnefPropertyReader *properties, GError **error)
{
	GMimeTnefProperty *property;
	GValue value = G_VALUE_INIT;
	GError *fault = NULL;

	g_return_val_if_fail (GMIME_IS_TNEF_PROPERTY_READER (properties), NULL);
	g_return_val_if_fail (error == NULL || *error == NULL, NULL);

	if (!property_valid (properties, error))
		return NULL;

	if (!properties->has_property || properties->value_index > 0 || properties->consumed) {
		_g_mime_tnef_invalid_operation (error, "The reader must be positioned on an unread property.");
		return NULL;
	}

	property = _g_mime_tnef_property_new (properties->tag, properties->name);

	while (properties->has_value) {
		properties->reader->value_skipped = FALSE;

		if (!g_mime_tnef_property_reader_read_value (properties, &value, &fault))
			break;

		if (properties->reader->value_skipped) {
			g_value_unset (&value);
			g_array_set_size (property->values, 0);
			break;
		}

		g_array_append_val (property->values, value);
		memset (&value, 0, sizeof value);

		if (!g_mime_tnef_property_tag_is_multi_valued (properties->tag) ||
		    !g_mime_tnef_property_reader_read_next_value (properties, &fault))
			break;
	}

	if (fault) {
		g_mime_tnef_property_unref (property);
		g_propagate_error (error, fault);
		return NULL;
	}

	return property;
}

/**
 * g_mime_tnef_property_reader_read_property_set:
 * @properties: a property reader
 * @error: return location for an error
 *
 * Materializes the remaining properties after the current property, or
 * the whole list if not yet positioned. For tables, reads only the current
 * row. Wire order and duplicate properties are preserved.
 *
 * Returns: (transfer full) (nullable): the property set, or %NULL on error
 **/
GMimeTnefPropertySet *
g_mime_tnef_property_reader_read_property_set (GMimeTnefPropertyReader *properties, GError **error)
{
	GMimeTnefPropertySet *set;
	GMimeTnefProperty *property;
	GError *fault = NULL;

	g_return_val_if_fail (GMIME_IS_TNEF_PROPERTY_READER (properties), NULL);
	g_return_val_if_fail (error == NULL || *error == NULL, NULL);

	if (!property_valid (properties, error))
		return NULL;

	set = _g_mime_tnef_property_set_new ();

	while (g_mime_tnef_property_reader_read_next_property (properties, &fault)) {
		if (!(property = g_mime_tnef_property_reader_read_property (properties, &fault)))
			break;
		g_ptr_array_add (set->items, property);
	}

	if (fault) {
		g_mime_tnef_property_set_unref (set);
		g_propagate_error (error, fault);
		return NULL;
	}

	return set;
}

/**
 * g_mime_tnef_property_reader_read_rows_as_property_sets:
 * @properties: a property reader
 * @error: return location for an error
 *
 * Materializes the remaining recipient rows, skipping any unread portion
 * of the current row. Non-table attributes return an empty array.
 *
 * Returns: (transfer full) (element-type GMimeTnefPropertySet) (nullable):
 *   the rows, or %NULL on error
 **/
GPtrArray *
g_mime_tnef_property_reader_read_rows_as_property_sets (GMimeTnefPropertyReader *properties, GError **error)
{
	GPtrArray *rows;
	GMimeTnefPropertySet *set;
	GError *fault = NULL;

	g_return_val_if_fail (GMIME_IS_TNEF_PROPERTY_READER (properties), NULL);
	g_return_val_if_fail (error == NULL || *error == NULL, NULL);

	if (!property_valid (properties, error))
		return NULL;

	rows = g_ptr_array_new_with_free_func ((GDestroyNotify) g_mime_tnef_property_set_unref);

	if (!properties->is_table)
		return rows;

	while (g_mime_tnef_property_reader_read_next_row (properties, &fault)) {
		if (!(set = g_mime_tnef_property_reader_read_property_set (properties, &fault)))
			break;
		g_ptr_array_add (rows, set);
	}

	if (fault) {
		g_ptr_array_unref (rows);
		g_propagate_error (error, fault);
		return NULL;
	}

	return rows;
}

/* Explicit TNEF reader accessors. */

/**
 * g_mime_tnef_reader_get_options:
 * @reader: a TNEF reader
 *
 * Gets the reader's construction options.
 *
 * Returns: (transfer none): the reader's copied options
 **/
const GMimeTnefOptions *
g_mime_tnef_reader_get_options (GMimeTnefReader *reader)
{
	g_return_val_if_fail (GMIME_IS_TNEF_READER (reader), NULL);

	return reader->options;
}

/**
 * g_mime_tnef_reader_get_compliance_options:
 * @reader: a TNEF reader
 *
 * Gets reporting options. Configure them before reading starts.
 *
 * Returns: (transfer none): reporting options
 **/
GMimeTnefComplianceOptions *
g_mime_tnef_reader_get_compliance_options (GMimeTnefReader *reader)
{
	g_return_val_if_fail (GMIME_IS_TNEF_READER (reader), NULL);

	return reader->root->compliance;
}

/**
 * g_mime_tnef_reader_get_depth:
 * @reader: a TNEF reader
 *
 * Gets the nesting depth; outermost readers have depth zero.
 *
 * Returns: the nesting depth
 **/
guint
g_mime_tnef_reader_get_depth (GMimeTnefReader *reader)
{
	g_return_val_if_fail (GMIME_IS_TNEF_READER (reader), 0);

	return reader->depth;
}

/**
 * g_mime_tnef_reader_get_legacy_key:
 * @reader: a TNEF reader
 *
 * Gets the legacy key, available after the first read.
 *
 * Returns: the legacy key
 **/
guint16
g_mime_tnef_reader_get_legacy_key (GMimeTnefReader *reader)
{
	g_return_val_if_fail (GMIME_IS_TNEF_READER (reader), 0);

	return reader->legacy_key;
}

/**
 * g_mime_tnef_reader_get_codepage:
 * @reader: a TNEF reader
 *
 * Gets the codepage used for 8-bit strings.
 *
 * Returns: the current codepage
 **/
int
g_mime_tnef_reader_get_codepage (GMimeTnefReader *reader)
{
	g_return_val_if_fail (GMIME_IS_TNEF_READER (reader), 0);

	return reader->codepage;
}

/**
 * g_mime_tnef_reader_get_level:
 * @reader: a TNEF reader
 *
 * Gets the current attribute's level.
 *
 * Returns: the current level, or zero
 **/
GMimeTnefAttributeLevel
g_mime_tnef_reader_get_level (GMimeTnefReader *reader)
{
	g_return_val_if_fail (GMIME_IS_TNEF_READER (reader), 0);

	return reader->level;
}

/**
 * g_mime_tnef_reader_get_tag:
 * @reader: a TNEF reader
 *
 * Gets the current attribute's tag.
 *
 * Returns: the current tag
 **/
GMimeTnefAttributeTag
g_mime_tnef_reader_get_tag (GMimeTnefReader *reader)
{
	g_return_val_if_fail (GMIME_IS_TNEF_READER (reader), 0);

	return reader->tag;
}

/**
 * g_mime_tnef_reader_get_attribute_type:
 * @reader: a TNEF reader
 *
 * Gets the current attribute's type.
 *
 * Returns: the attribute type
 **/
GMimeTnefAttributeType
g_mime_tnef_reader_get_attribute_type (GMimeTnefReader *reader)
{
	g_return_val_if_fail (GMIME_IS_TNEF_READER (reader), 0);

	return (GMimeTnefAttributeType) (reader->tag & 0xffff0000);
}

/**
 * g_mime_tnef_reader_get_length:
 * @reader: a TNEF reader
 *
 * Gets the current attribute's declared value length.
 *
 * Returns: the value length in bytes
 **/
guint
g_mime_tnef_reader_get_length (GMimeTnefReader *reader)
{
	g_return_val_if_fail (GMIME_IS_TNEF_READER (reader), 0);

	return reader->length;
}

/**
 * g_mime_tnef_reader_get_stream_offset:
 * @reader: a TNEF reader
 *
 * Gets the current attribute header's offset within the outermost stream.
 *
 * Returns: the attribute header offset
 **/
gint64
g_mime_tnef_reader_get_stream_offset (GMimeTnefReader *reader)
{
	g_return_val_if_fail (GMIME_IS_TNEF_READER (reader), 0);

	return reader->base_offset + reader->attribute_offset;
}

/**
 * g_mime_tnef_property_reader_get_row_count:
 * @reader: a TNEF reader
 *
 * Gets the recipient table's row count.
 *
 * Returns: the row count, or -1 before it is read
 **/
int
g_mime_tnef_property_reader_get_row_count (GMimeTnefPropertyReader *reader)
{
	g_return_val_if_fail (GMIME_IS_TNEF_PROPERTY_READER (reader), -1);
	g_return_val_if_fail (property_valid (reader, NULL), -1);

	return reader->row_count;
}

/**
 * g_mime_tnef_property_reader_get_property_count:
 * @reader: a TNEF reader
 *
 * Gets the property count of the list or current recipient row.
 *
 * Returns: the count, or -1 before it is read
 **/
int
g_mime_tnef_property_reader_get_property_count (GMimeTnefPropertyReader *reader)
{
	g_return_val_if_fail (GMIME_IS_TNEF_PROPERTY_READER (reader), -1);
	g_return_val_if_fail (property_valid (reader, NULL), -1);

	return reader->property_count;
}

/**
 * g_mime_tnef_property_reader_get_tag:
 * @reader: a TNEF reader
 *
 * Gets the current property's tag.
 *
 * Returns: the current property tag
 **/
GMimeTnefPropertyTag
g_mime_tnef_property_reader_get_tag (GMimeTnefPropertyReader *reader)
{
	g_return_val_if_fail (GMIME_IS_TNEF_PROPERTY_READER (reader), 0);
	g_return_val_if_fail (property_valid (reader, NULL), 0);

	return reader->tag;
}

/**
 * g_mime_tnef_property_reader_get_property_type:
 * @reader: a TNEF reader
 *
 * Gets the property's value type without its multi-valued flag.
 *
 * Returns: the element type
 **/
GMimeTnefPropertyType
g_mime_tnef_property_reader_get_property_type (GMimeTnefPropertyReader *reader)
{
	g_return_val_if_fail (GMIME_IS_TNEF_PROPERTY_READER (reader), 0);
	g_return_val_if_fail (property_valid (reader, NULL), 0);

	return g_mime_tnef_property_tag_get_value_type (reader->tag);
}

/**
 * g_mime_tnef_property_reader_is_multi_valued:
 * @reader: a TNEF reader
 *
 * Checks the current property's multi-valued flag.
 *
 * Returns: whether the property is multi-valued
 **/
gboolean
g_mime_tnef_property_reader_is_multi_valued (GMimeTnefPropertyReader *reader)
{
	g_return_val_if_fail (GMIME_IS_TNEF_PROPERTY_READER (reader), FALSE);
	g_return_val_if_fail (property_valid (reader, NULL), FALSE);

	return g_mime_tnef_property_tag_is_multi_valued (reader->tag);
}

/**
 * g_mime_tnef_property_reader_get_name:
 * @reader: a TNEF reader
 *
 * Gets the current property's name, or NULL for an ordinary property.
 *
 * Returns: (transfer none) (nullable): the named-property identity
 **/
GMimeTnefNameId *
g_mime_tnef_property_reader_get_name (GMimeTnefPropertyReader *reader)
{
	g_return_val_if_fail (GMIME_IS_TNEF_PROPERTY_READER (reader), NULL);
	g_return_val_if_fail (property_valid (reader, NULL), NULL);

	return reader->name;
}

/**
 * g_mime_tnef_property_reader_get_value_count:
 * @reader: a TNEF reader
 *
 * Gets the current property's declared and bounded value count.
 *
 * Returns: the value count
 **/
int
g_mime_tnef_property_reader_get_value_count (GMimeTnefPropertyReader *reader)
{
	g_return_val_if_fail (GMIME_IS_TNEF_PROPERTY_READER (reader), 0);
	g_return_val_if_fail (property_valid (reader, NULL), 0);

	return reader->value_count;
}

/**
 * g_mime_tnef_property_reader_is_value_consumed:
 * @reader: a TNEF reader
 *
 * Checks whether the variable-length value has been claimed.
 *
 * Returns: whether the value has been claimed
 **/
gboolean
g_mime_tnef_property_reader_is_value_consumed (GMimeTnefPropertyReader *reader)
{
	g_return_val_if_fail (GMIME_IS_TNEF_PROPERTY_READER (reader), FALSE);
	g_return_val_if_fail (property_valid (reader, NULL), FALSE);

	return reader->consumed;
}

/**
 * g_mime_tnef_property_reader_read_value_as_boolean:
 * @properties: a property reader
 * @value: (out): the converted value
 * @error: return location for an error
 *
 * Reads any integer-based value as a boolean.
 *
 * Returns: whether the operation succeeded
 **/
gboolean
g_mime_tnef_property_reader_read_value_as_boolean (GMimeTnefPropertyReader *properties, gboolean *value, GError **error)
{
	gint64 number;

	g_return_val_if_fail (GMIME_IS_TNEF_PROPERTY_READER (properties), FALSE);
	g_return_val_if_fail (value != NULL, FALSE);
	g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

	if (!get_integer_value (properties, &number, TRUE, error))
		return FALSE;

	*value = number != 0;

	return TRUE;
}

/**
 * g_mime_tnef_property_reader_read_value_as_int16:
 * @properties: a property reader
 * @value: (out): the converted value
 * @error: return location for an error
 *
 * Reads a numeric value as an integer, truncating fractions and narrowing
 * integer values. Non-finite or out-of-range floating-point values fail.
 *
 * Returns: whether the operation succeeded
 **/
gboolean
g_mime_tnef_property_reader_read_value_as_int16 (GMimeTnefPropertyReader *properties, gint16 *value, GError **error)
{
	gint64 number;

	g_return_val_if_fail (GMIME_IS_TNEF_PROPERTY_READER (properties), FALSE);
	g_return_val_if_fail (value != NULL, FALSE);
	g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

	if (!get_integer_value (properties, &number, FALSE, error))
		return FALSE;

	*value = (gint16) number;

	return TRUE;
}

/**
 * g_mime_tnef_property_reader_read_value_as_int32:
 * @properties: a property reader
 * @value: (out): the converted value
 * @error: return location for an error
 *
 * Reads a numeric value as an integer, truncating fractions and narrowing
 * integer values. Non-finite or out-of-range floating-point values fail.
 *
 * Returns: whether the operation succeeded
 **/
gboolean
g_mime_tnef_property_reader_read_value_as_int32 (GMimeTnefPropertyReader *properties, gint32 *value, GError **error)
{
	gint64 number;

	g_return_val_if_fail (GMIME_IS_TNEF_PROPERTY_READER (properties), FALSE);
	g_return_val_if_fail (value != NULL, FALSE);
	g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

	if (!get_integer_value (properties, &number, FALSE, error))
		return FALSE;

	*value = (gint32) number;

	return TRUE;
}

/**
 * g_mime_tnef_property_reader_read_value_as_int64:
 * @properties: a property reader
 * @value: (out): the converted value
 * @error: return location for an error
 *
 * Reads a numeric value as an integer, truncating fractions and narrowing
 * integer values. Non-finite or out-of-range floating-point values fail.
 *
 * Returns: whether the operation succeeded
 **/
gboolean
g_mime_tnef_property_reader_read_value_as_int64 (GMimeTnefPropertyReader *properties, gint64 *value, GError **error)
{
	gint64 number;

	g_return_val_if_fail (GMIME_IS_TNEF_PROPERTY_READER (properties), FALSE);
	g_return_val_if_fail (value != NULL, FALSE);
	g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

	if (!get_integer_value (properties, &number, FALSE, error))
		return FALSE;

	*value = number;

	return TRUE;
}
