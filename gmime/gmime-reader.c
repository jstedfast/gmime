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
#include <string.h>

#include "gmime-reader.h"
#include "gmime-reader-private.h"
#include "gmime-stream-block.h"
#include "gmime-encodings.h"
#include "gmime-error.h"
#include "gmime-common.h"
#include "gmime-internal.h"
#include "gmime-parse-utils.h"
#include "gmime-encoding-validator-private.h"
#include "gmime-address-validator-private.h"
#include "gmime-reader-simd-private.h"

#include "gmime-object-private.h"

/**
 * SECTION: gmime-reader
 * @title: GMimeReader
 * @short_description: Forward-only MIME wire reader
 *
 * #GMimeReader owns its input buffer and delivers borrowed wire data to
 * subclass callbacks. It does not use #GMimeParser or construct entities.
 * Header blocks, messages, explicit-stack MIME traversal and mbox/MMDF
 * mailboxes are supported. Optional compliance reporting never changes
 * tolerant parsing. #GMimeParser uses a reader-backed model adapter.
 * Async operations are not yet implemented.
 **/

#define READ_AHEAD_SIZE 128
#define BLOCK_SIZE 4096
#define PAD_SIZE 4

typedef enum {
	FRAME_BEGIN,
	FRAME_NEXT_CHILD,
	FRAME_AWAIT_CHILD,
	FRAME_END
} EntityFrameState;

typedef struct {
	GMimeContentType *content_type;
	GMimeReaderEntityType type;
	EntityFrameState state;
	char *boundary;
	gboolean boundary_active, message_active;
	gint64 begin, headers_end, content_begin;
	guint64 begin_line, content_line;
	gint64 message_begin, message_headers_end;
	guint64 message_line;
	GMimeContentEncoding encoding;
	gint64 type_offset;
	guint64 type_line;
} EntityFrame;

typedef struct {
	GMimeComplianceIssueFunc report;
	gpointer user_data;
	GDestroyNotify destroy;
	guint refs;
} ComplianceLogger;

struct _GMimeReaderPrivate {
	GMimeComplianceOptions *compliance_options;
	ComplianceLogger *logger, *active_logger;
	GMimeComplianceValidators validators;
	GMimeComplianceContext context;
	guint issue_counts[91], issue_limit;
	GMimeEncodingValidator *validator;

	GMimeParserOptions *options;
	GMimeStream *stream;
	GMimeFormat format;
	gint64 position;

	guint8 input[READ_AHEAD_SIZE + BLOCK_SIZE + PAD_SIZE];
	gsize input_index, input_end;

	guint64 line_number;
	gint64 line_begin, prev_line_begin;

	GByteArray *header;
	GError *error;

	guint8 previous[2];
	guint previous_count;

	GPtrArray *frames;
	EntityFrame *boundary;

	GMimeContentType *content_type;
	GMimeContentEncoding encoding;
	gint64 content_length, content_end;

	guint32 seen_message, seen_resent;
	gint64 header_begin, type_offset, encoding_offset;
	guint64 header_line, type_line, encoding_line, encoding_column;

	unsigned int truncated_issues : 1;
	unsigned int reported_depth : 1;
	unsigned int message_headers : 1;
	unsigned int seen_mime_version : 1;
	unsigned int header_limit_exceeded : 1;
	unsigned int stop_at_separator : 1;
	unsigned int mailbox_boundary : 1;
	unsigned int seen_encoding : 1;
	unsigned int final_boundary : 1;
	unsigned int boundary_eos : 1;
	unsigned int entity_read : 1;
	unsigned int toplevel : 1;
	unsigned int headers_complete : 1;
	unsigned int input_eos : 1;
	unsigned int complete_eos : 1;
	unsigned int reading : 1;
	unsigned int initial : 1;
};

typedef struct _GMimeReaderPrivate GMimeReaderPrivate;

G_DEFINE_TYPE_WITH_PRIVATE (GMimeReader, g_mime_reader, G_TYPE_OBJECT)

static GMimeReaderPrivate *
_g_mime_reader_get_private (GMimeReader *object)
{
	return g_mime_reader_get_instance_private (object);
}

static void
logger_unref (ComplianceLogger *logger)
{
	if (logger && !--logger->refs) {
		if (logger->destroy)
			logger->destroy (logger->user_data);

		g_free (logger);
	}
}

static void
begin_compliance (GMimeReader *reader)
{
	GMimeReaderPrivate *priv = _g_mime_reader_get_private (reader);

	/* A callback may replace the logger or options. Keep the current
	 * operation's snapshot, including callback data, alive until it ends. */
	priv->active_logger = priv->logger;
	if (priv->active_logger)
		priv->active_logger->refs++;

	priv->context = g_mime_compliance_options_get_context (priv->compliance_options);
	priv->validators = g_mime_compliance_options_get_enabled_validators (priv->compliance_options);
	priv->issue_limit = g_mime_compliance_options_get_max_issues_per_violation (priv->compliance_options);
	memset (priv->issue_counts, 0, sizeof (priv->issue_counts));
	priv->truncated_issues = FALSE;
	priv->reported_depth = FALSE;
}

static void
end_compliance (GMimeReader *reader)
{
	GMimeReaderPrivate *priv = _g_mime_reader_get_private (reader);
	ComplianceLogger *logger = priv->active_logger;

	priv->active_logger = NULL;
	g_clear_object (&priv->validator);
	logger_unref (logger);
}

static void
report_issue (const GMimeComplianceIssue *issue, gpointer user_data)
{
	GMimeReaderPrivate *priv = _g_mime_reader_get_private (GMIME_READER (user_data));
	GMimeComplianceIssue truncated;
	guint index = (guint) issue->violation;

	if (!priv->active_logger)
		return;

	if (priv->issue_limit && index < G_N_ELEMENTS (priv->issue_counts)) {
		if (priv->issue_counts[index] == priv->issue_limit) {
			/* Budgets are per violation so a flood of harmless findings
			 * cannot hide a different, serious defect later in a message. */
			if (priv->truncated_issues)
				return;

			priv->truncated_issues = TRUE;
			truncated = *issue;
			truncated.violation = GMIME_COMPLIANCE_TOO_MANY_COMPLIANCE_ISSUES;
			truncated.position_kind = GMIME_COMPLIANCE_POSITION_EXACT;
			issue = &truncated;
		} else {
			priv->issue_counts[index]++;
		}
	}

	priv->active_logger->report (issue, priv->active_logger->user_data);
}

static void
report_at (GMimeReader *reader, GMimeComplianceViolation violation, gint64 offset,
	   guint64 line, guint64 column, GMimeCompliancePositionKind kind)
{
	GMimeReaderPrivate *priv = _g_mime_reader_get_private (reader);
	GMimeComplianceIssue issue = { priv->context, violation, offset, line, column, kind };

	report_issue (&issue, reader);
}

static gboolean
headers_begin (GMimeReader *reader, gint64 begin_offset, guint64 begin_line_number, GError **error)
{
	return TRUE;
}

static gboolean
header_read (GMimeReader *reader, const guint8 *buffer, gsize length,
	     gsize field_name_length, gsize field_length, gboolean invalid,
	     gint64 begin_offset, guint64 begin_line_number, GError **error)
{
	return TRUE;
}

static gboolean
headers_end (GMimeReader *reader, gint64 begin_offset, guint64 begin_line_number,
	     gint64 end_offset, guint64 end_line_number, GError **error)
{
	return TRUE;
}

static gboolean
body_separator (GMimeReader *reader, gint64 begin_offset, guint64 line_number, gint64 end_offset, GError **error)
{
	return TRUE;
}

static void
g_mime_reader_finalize (GObject *object)
{
	GMimeReader *reader = GMIME_READER (object);
	GMimeReaderPrivate *priv = _g_mime_reader_get_private (reader);

	g_mime_parser_options_free (priv->options);
	g_mime_compliance_options_free (priv->compliance_options);
	logger_unref (priv->logger);
	g_byte_array_unref (priv->header);
	g_clear_object (&priv->stream);
	g_clear_error (&priv->error);
	g_clear_object (&priv->content_type);

	G_OBJECT_CLASS (g_mime_reader_parent_class)->finalize (object);
}

static void
g_mime_reader_class_init (GMimeReaderClass *klass)
{
	G_OBJECT_CLASS (klass)->finalize = g_mime_reader_finalize;
	klass->headers_begin = headers_begin;
	klass->header_read = header_read;
	klass->headers_end = headers_end;
	klass->body_separator = body_separator;
}

static void
g_mime_reader_init (GMimeReader *reader)
{
	GMimeReaderPrivate *priv = _g_mime_reader_get_private (reader);

	priv->options = g_mime_parser_options_clone (NULL);
	priv->compliance_options = g_mime_compliance_options_clone (NULL);
	priv->header = g_byte_array_sized_new (512);
	priv->input_index = READ_AHEAD_SIZE;
	priv->input_end = READ_AHEAD_SIZE;
	priv->line_number = 1;
	priv->initial = TRUE;
}

/**
 * g_mime_reader_new:
 * @options: (nullable): parser options, or %NULL for defaults
 * @stream: the stream to read
 *
 * Creates a reader at the current stream position. The reader owns a reference
 * to @stream and a copy of @options, but never resets or closes the stream.
 * As with g_mime_parser_options_clone(), warning callback data stays borrowed.
 *
 * Returns: (transfer full): a new #GMimeReader.
 **/
GMimeReader *
g_mime_reader_new (GMimeParserOptions *options, GMimeStream *stream)
{
	return g_mime_reader_new_with_format (options, stream, GMIME_FORMAT_MESSAGE);
}

/**
 * g_mime_reader_new_with_format:
 * @options: (nullable): parser options, or %NULL for defaults
 * @stream: the stream to read
 * @format: the input format
 *
 * Creates a reader for a single message or mailbox at the current stream
 * position. Ownership is the same as g_mime_reader_new().
 *
 * Returns: (transfer full): a new #GMimeReader.
 **/
GMimeReader *
g_mime_reader_new_with_format (GMimeParserOptions *options, GMimeStream *stream, GMimeFormat format)
{
	GMimeReader *reader;

	g_return_val_if_fail (GMIME_IS_STREAM (stream), NULL);
	g_return_val_if_fail (format >= GMIME_FORMAT_MESSAGE && format <= GMIME_FORMAT_MMDF, NULL);

	reader = g_object_new (GMIME_TYPE_READER, NULL);
	g_mime_reader_set_options (reader, options);
	g_mime_reader_set_stream_with_format (reader, stream, format);

	return reader;
}

/**
 * g_mime_reader_set_stream:
 * @reader: a #GMimeReader
 * @stream: the new stream to read
 *
 * Discards buffered input and any previous error, then starts at the current
 * position of @stream with line number 1. A stream without tell() support uses
 * offset 0 as its origin. Neither stream is reset or closed. Must not be
 * called during a read operation or from a reader callback.
 **/
void
g_mime_reader_set_stream (GMimeReader *reader, GMimeStream *stream)
{
	g_mime_reader_set_stream_with_format (reader, stream, GMIME_FORMAT_MESSAGE);
}

/**
 * g_mime_reader_set_stream_with_format:
 * @reader: a #GMimeReader
 * @stream: the new input stream
 * @format: the input format
 *
 * Assigns a stream and format, discarding buffered input and sticky errors
 * without seeking, resetting or closing either stream.
 **/
void
g_mime_reader_set_stream_with_format (GMimeReader *reader, GMimeStream *stream, GMimeFormat format)
{
	struct _GMimeReaderPrivate *priv;
	gint64 position;

	g_return_if_fail (GMIME_IS_READER (reader));
	g_return_if_fail (GMIME_IS_STREAM (stream));
	g_return_if_fail (format >= GMIME_FORMAT_MESSAGE && format <= GMIME_FORMAT_MMDF);

	priv = _g_mime_reader_get_private (reader);

	g_return_if_fail (!priv->reading);

	g_object_ref (stream);
	g_clear_object (&priv->stream);
	priv->stream = stream;
	position = g_mime_stream_tell (stream);
	priv->position = position >= 0 ? position : 0;
	priv->input_index = READ_AHEAD_SIZE;
	priv->input_end = READ_AHEAD_SIZE;
	priv->line_number = 1;
	priv->line_begin = priv->position;
	priv->prev_line_begin = priv->position;
	priv->previous_count = 0;
	priv->boundary = NULL;
	priv->boundary_eos = FALSE;
	priv->format = format;
	priv->mailbox_boundary = FALSE;
	priv->content_length = -1;
	priv->content_end = 0;
	priv->header_limit_exceeded = FALSE;
	g_clear_object (&priv->content_type);
	priv->input_eos = FALSE;
	priv->complete_eos = FALSE;
	priv->initial = TRUE;
	g_byte_array_set_size (priv->header, 0);
	g_clear_error (&priv->error);
}

/**
 * g_mime_reader_get_format:
 * @reader: a #GMimeReader
 *
 * Returns: the input format selected when assigning the stream.
 **/
GMimeFormat
g_mime_reader_get_format (GMimeReader *reader)
{
	GMimeReaderPrivate *priv;

	g_return_val_if_fail (GMIME_IS_READER (reader), GMIME_FORMAT_MESSAGE);

	priv = _g_mime_reader_get_private (reader);

	return priv->format;
}

/**
 * g_mime_reader_get_options:
 * @reader: a #GMimeReader
 *
 * Gets the reader's options. Do not modify them during a read operation.
 *
 * Returns: (transfer none): the reader's options.
 **/
GMimeParserOptions *
g_mime_reader_get_options (GMimeReader *reader)
{
	GMimeReaderPrivate *priv;

	g_return_val_if_fail (GMIME_IS_READER (reader), NULL);

	priv = _g_mime_reader_get_private (reader);

	return priv->options;
}

/**
 * g_mime_reader_set_options:
 * @reader: a #GMimeReader
 * @options: (nullable): parser options, or %NULL for defaults
 *
 * Replaces the reader's options with a copy of @options. Must not be called
 * during a read operation or from a reader callback.
 **/
void
g_mime_reader_set_options (GMimeReader *reader, GMimeParserOptions *options)
{
	GMimeReaderPrivate *priv;
	GMimeParserOptions *copy;

	g_return_if_fail (GMIME_IS_READER (reader));
	priv = _g_mime_reader_get_private (reader);

	g_return_if_fail (!priv->reading);

	copy = g_mime_parser_options_clone (options);
	g_mime_parser_options_free (priv->options);
	priv->options = copy;
}

/**
 * g_mime_reader_get_compliance_options:
 * @reader: a #GMimeReader
 *
 * Changes apply at the next read operation, not to an active operation.
 *
 * Returns: (transfer none): the reader's compliance settings.
 **/
GMimeComplianceOptions *
g_mime_reader_get_compliance_options (GMimeReader *reader)
{
	GMimeReaderPrivate *priv;

	g_return_val_if_fail (GMIME_IS_READER (reader), NULL);

	priv = _g_mime_reader_get_private (reader);

	return priv->compliance_options;
}

/**
 * g_mime_reader_set_compliance_options:
 * @reader: a #GMimeReader
 * @options: (nullable): settings to copy, or %NULL for defaults
 *
 * Changes apply to the next read operation. They do not enable reporting
 * unless a logger has also been assigned.
 **/
void
g_mime_reader_set_compliance_options (GMimeReader *reader, GMimeComplianceOptions *options)
{
	GMimeReaderPrivate *priv;
	GMimeComplianceOptions *copy;

	g_return_if_fail (GMIME_IS_READER (reader));

	copy = g_mime_compliance_options_clone (options);
	priv = _g_mime_reader_get_private (reader);

	g_mime_compliance_options_free (priv->compliance_options);
	priv->compliance_options = copy;
}

/**
 * g_mime_reader_set_compliance_logger:
 * @reader: a #GMimeReader
 * @report: (nullable) (scope notified) (closure user_data) (destroy destroy): diagnostic callback
 * @user_data: (nullable): application data
 * @destroy: (nullable): releases @user_data
 *
 * Enables optional compliance reporting, or disables it when @report is %NULL.
 * Without a logger the reader skips compliance validation altogether.
 * Replacements made from callbacks take effect at the next read operation.
 * The previous data is released only after its active operation finishes.
 **/
void
g_mime_reader_set_compliance_logger (GMimeReader *reader, GMimeComplianceIssueFunc report,
				    gpointer user_data, GDestroyNotify destroy)
{
	GMimeReaderPrivate *priv;
	ComplianceLogger *logger = NULL, *previous;

	g_return_if_fail (GMIME_IS_READER (reader));
	g_return_if_fail (report != NULL || (user_data == NULL && destroy == NULL));

	if (report) {
		logger = g_new (ComplianceLogger, 1);
		logger->report = report;
		logger->user_data = user_data;
		logger->destroy = destroy;
		logger->refs = 1;
	}

	priv = _g_mime_reader_get_private (reader);

	previous = priv->logger;
	priv->logger = logger;
	logger_unref (previous);
}

/**
 * g_mime_reader_tell:
 * @reader: a #GMimeReader
 *
 * Gets the logical position, excluding buffered bytes not yet consumed.
 *
 * Returns: the current input offset.
 **/
gint64
g_mime_reader_tell (GMimeReader *reader)
{
	GMimeReaderPrivate *priv;

	g_return_val_if_fail (GMIME_IS_READER (reader), -1);

	priv = _g_mime_reader_get_private (reader);

	return priv->position - (priv->input_end - priv->input_index);
}

/**
 * g_mime_reader_eos:
 * @reader: a #GMimeReader
 *
 * Gets whether the last successful operation reached the end of input.
 * Buffered body bytes or an unprobed stream end do not count as end of input.
 *
 * Returns: %TRUE if the reader has reached the end of input.
 **/
gboolean
g_mime_reader_eos (GMimeReader *reader)
{
	GMimeReaderPrivate *priv;

	g_return_val_if_fail (GMIME_IS_READER (reader), TRUE);

	priv = _g_mime_reader_get_private (reader);

	return priv->complete_eos;
}

static gsize
read_ahead (GMimeReader *reader, gsize atleast)
{
	GMimeReaderPrivate *priv = _g_mime_reader_get_private (reader);
	gsize left = priv->input_end - priv->input_index;
	gsize start = READ_AHEAD_SIZE;
	ssize_t nread;
	int err;

	if (left >= atleast || priv->input_eos)
		return left;

	if (left > 0) {
		/* Align the end of the remaining input with the read-ahead area,
		 * leaving as much room as possible for the next block. */
		if (priv->input_index >= start) {
			start -= MIN (READ_AHEAD_SIZE, left);
			memmove (priv->input + start, priv->input + priv->input_index, left);
			priv->input_index = start;
			start += left;
		} else if (priv->input_index > 0) {
			gsize shift = MIN (priv->input_index, priv->input_end - start);

			memmove (priv->input + priv->input_index - shift, priv->input + priv->input_index, left);
			priv->input_index -= shift;
			start = priv->input_index + left;
		} else {
			start = priv->input_end;
		}

		priv->input_end = start;
	} else {
		priv->input_index = start;
		priv->input_end = start;
	}

	if (start == sizeof (priv->input) - PAD_SIZE)
		return left;

	errno = 0;
	nread = g_mime_stream_read (priv->stream, (char *) priv->input + start,
				   sizeof (priv->input) - PAD_SIZE - start);
	err = errno;

	if (nread < 0) {
		g_set_error (&priv->error, GMIME_ERROR, err ? err : GMIME_ERROR_GENERAL,
			     "Failed to read MIME input at offset %" G_GINT64_FORMAT ": %s",
			     priv->position, err ? g_strerror (err) : "stream read failed");
		return 0;
	}

	if (nread > G_MAXINT64 - priv->position) {
		g_set_error_literal (&priv->error, GMIME_ERROR, GMIME_ERROR_PARSE_ERROR,
				     "MIME input exceeds the stream offset range.");
		return 0;
	}

	if (nread > 0) {
		priv->input_end += nread;
		priv->position += nread;
	} else {
		priv->input_eos = TRUE;
	}

	return priv->input_end - priv->input_index;
}

static gsize
read_ahead_at_least (GMimeReader *reader, gsize atleast)
{
	gsize available, previous;

	/* A short read does not mean EOF. Unlike read_ahead(), keep filling
	 * until there is enough input, no progress, or an actual end/error. */
	GMimeReaderPrivate *priv = _g_mime_reader_get_private (reader);

	do {
		previous = priv->input_end - priv->input_index;
		available = read_ahead (reader, atleast);
	} while (!priv->error && available < atleast && available > previous);

	return available;
}

static gboolean
callback_result (GMimeReader *reader, gboolean result)
{
	GMimeReaderPrivate *priv = _g_mime_reader_get_private (reader);

	if (!result && !priv->error)
		g_set_error_literal (&priv->error, GMIME_ERROR, GMIME_ERROR_GENERAL,
				     "MIME reader callback failed without an error.");

	return priv->error == NULL;
}

static gboolean
invalid_headers (GMimeReader *reader, gint64 offset)
{
	GMimeReaderPrivate *priv = _g_mime_reader_get_private (reader);

	g_set_error (&priv->error, GMIME_ERROR, GMIME_ERROR_PARSE_ERROR,
		     "Failed to parse headers at offset %" G_GINT64_FORMAT ".", offset);
	return FALSE;
}

static gboolean
append_header (GMimeReader *reader, const guint8 *buffer, gsize length, gint64 offset)
{
	GMimeReaderPrivate *priv = _g_mime_reader_get_private (reader);
	gsize limit = g_mime_parser_options_get_max_header_length (priv->options);

	/* Check before allocating, including folds and line terminators in the
	 * budget. A single unterminated header must not grow without bounds. */
	if (length > limit - priv->header->len) {
		priv->header_limit_exceeded = TRUE;
		g_set_error (&priv->error, GMIME_ERROR, GMIME_ERROR_PARSE_ERROR,
			     "Header at offset %" G_GINT64_FORMAT " exceeds the maximum header length of %"
			     G_GSIZE_FORMAT " bytes.", offset, limit);
		return FALSE;
	}

	g_byte_array_append (priv->header, buffer, length);
	return TRUE;
}

static gboolean
is_blank (guint8 c)
{
	return c == ' ' || c == '\t';
}

static void
consume_input (GMimeReader *reader, gsize length)
{
	GMimeReaderPrivate *priv = _g_mime_reader_get_private (reader);
	gint64 offset = g_mime_reader_tell (reader);
	gsize i;

	/* Keep line positions and the last two bytes outside the input buffer:
	 * refilling may move or discard the bytes before the logical cursor. */
	for (i = 0; i < length; i++) {
		guint8 c = priv->input[priv->input_index++];

		priv->previous[0] = priv->previous[1];
		priv->previous[1] = c;
		priv->previous_count = MIN (priv->previous_count + 1, 2);

		if (c == '\n') {
			if (priv->active_logger && offset + i + 1 - priv->line_begin > 1000)
				report_at (reader, GMIME_COMPLIANCE_OVERSIZED_LINE, priv->line_begin,
					   priv->line_number, 1, GMIME_COMPLIANCE_POSITION_LINE_START);

			priv->prev_line_begin = priv->line_begin;
			priv->line_begin = offset + i + 1;
			priv->line_number++;
		}
	}
}

static gboolean
header_is (const guint8 *header, gsize length, const char *name)
{
	return strlen (name) == length && g_ascii_strncasecmp ((const char *) header, name, length) == 0;
}

static gint
repeat_header_index (const guint8 *header, gsize length)
{
	static const char *names[] = {
		"Date", "From", "Sender", "Reply-To", "To", "Cc", "Bcc", "Message-Id",
		"In-Reply-To", "References", "Subject", "Return-Path",
		"Resent-Date", "Resent-From", "Resent-Sender", "Resent-To", "Resent-Cc",
		"Resent-Bcc", "Resent-Message-Id"
	};
	guint i;

	for (i = 0; i < G_N_ELEMENTS (names); i++) {
		if (header_is (header, length, names[i]))
			return (gint) i;
	}

	return -1;
}

static gboolean
address_header (const guint8 *header, gsize length)
{
	static const char *names[] = {
		"From", "Sender", "Reply-To", "To", "Cc", "Bcc", "Resent-From",
		"Resent-Sender", "Resent-Reply-To", "Resent-To", "Resent-Cc",
		"Resent-Bcc", "Disposition-Notification-To"
	};
	guint i;

	/* Return-Path is a path, not an address list. Its empty <> is legal. */
	for (i = 0; i < G_N_ELEMENTS (names); i++) {
		if (header_is (header, length, names[i]))
			return TRUE;
	}

	return FALSE;
}

static gboolean
valid_utf8_bytes (const guint8 *bytes, gsize length)
{
	while (length) {
		const guint8 *nul = memchr (bytes, 0, length);
		gsize count = nul ? (gsize) (nul - bytes) : length;

		/* Embedded NUL is valid UTF-8, even though g_utf8_validate()
		 * treats it as the end of a C string. It is diagnosed separately. */
		if (!g_utf8_validate ((const char *) bytes, count, NULL))
			return FALSE;

		if (!nul)
			break;

		bytes += count + 1;
		length -= count + 1;
	}

	return TRUE;
}

static void
check_header_semantics (GMimeReader *reader, gsize name_length, gsize field_length,
			gboolean invalid, gint64 begin, guint64 line)
{
	GMimeReaderPrivate *priv = _g_mime_reader_get_private (reader);
	const guint8 *data = priv->header->data;
	const guint8 *value = data + (invalid ? 0 : field_length + 1);
	gsize length = priv->header->len - (invalid ? 0 : field_length + 1);
	gboolean address = !invalid && address_header (data, name_length);
	gint index;

	if (!priv->active_logger)
		return;

	if (!address && !valid_utf8_bytes (value, length))
		report_at (reader, GMIME_COMPLIANCE_UNEXPECTED_8BIT_BYTES_IN_HEADER, begin, line, 1,
			   GMIME_COMPLIANCE_POSITION_ELEMENT_START);

	if (address && length && (priv->validators & GMIME_COMPLIANCE_VALIDATOR_ADDRESS))
		_g_mime_address_validate (value, length, report_issue, reader, priv->context,
					 begin + field_length + 1, line, field_length + 2);

	if (invalid || !priv->message_headers)
		return;

	index = repeat_header_index (data, name_length);

	if (index < 12)
		priv->seen_resent = 0;

	if (index >= 0) {
		guint32 *seen = index < 12 ? &priv->seen_message : &priv->seen_resent;
		guint32 bit = 1u << index;

		if (*seen & bit)
			report_at (reader, GMIME_COMPLIANCE_REPEATED_DATE + index, begin, line, 1,
				   GMIME_COMPLIANCE_POSITION_ELEMENT_START);

		*seen |= bit;
	}

	if (header_is (data, name_length, "MIME-Version"))
		priv->seen_mime_version = TRUE;
}

static void
check_missing_headers (GMimeReader *reader, gint64 offset, guint64 line, gboolean eof)
{
	GMimeReaderPrivate *priv = _g_mime_reader_get_private (reader);
	GMimeCompliancePositionKind kind = eof ? GMIME_COMPLIANCE_POSITION_EXACT : GMIME_COMPLIANCE_POSITION_LINE_START;
	guint64 column = eof ? (guint64) (offset - priv->line_begin) + 1 : 1;

	if (!priv->active_logger || !priv->message_headers || !priv->toplevel)
		return;

	if (!(priv->seen_message & 1u))
		report_at (reader, GMIME_COMPLIANCE_MISSING_DATE, offset, line, column, kind);

	if (!(priv->seen_message & 2u))
		report_at (reader, GMIME_COMPLIANCE_MISSING_FROM, offset, line, column, kind);

	if (!(priv->seen_message & (1u << 7)))
		report_at (reader, GMIME_COMPLIANCE_MISSING_MESSAGE_ID, offset, line, column, kind);

	if (!priv->seen_mime_version && (priv->content_type || priv->seen_encoding))
		report_at (reader, GMIME_COMPLIANCE_MISSING_MIME_VERSION, offset, line, column, kind);
}

static gboolean
boundary_whitespace (guint8 c)
{
	return c == ' ' || c == '\t' || c == '\r' || c == '\v' || c == '\f';
}

static gboolean
check_boundary (GMimeReader *reader, const guint8 *buffer, gsize length)
{
	GMimeReaderPrivate *priv = _g_mime_reader_get_private (reader);
	guint i;

	if (length < 2)
		return FALSE;

	while (length > 0 && boundary_whitespace (buffer[length - 1]))
		length--;

	for (i = priv->frames ? priv->frames->len : 0; i > 0; i--) {
		EntityFrame *frame = g_ptr_array_index (priv->frames, i - 1);
		gsize marker_length;

		if (!frame->boundary_active)
			continue;

		if (buffer[0] != '-' || buffer[1] != '-')
			break;

		marker_length = strlen (frame->boundary);
		if (length != marker_length + 2 && length != marker_length + 4)
			continue;

		if (memcmp (buffer + 2, frame->boundary, marker_length) != 0)
			continue;

		if (length == marker_length + 4 &&
		    (buffer[length - 1] != '-' || buffer[length - 2] != '-'))
			continue;

		/* The nearest matching boundary wins. An ancestor match is left
		 * unconsumed while truncated descendants unwind their frames. */
		priv->boundary = frame;
		priv->final_boundary = length == marker_length + 4;

		return TRUE;
	}

	/* Mailbox separators are outermost boundaries. Content-Length only
	 * suppresses premature From-lines; it never splits a MIME body at an
	 * arbitrary byte or suppresses an actual multipart delimiter. */
	if ((priv->format == GMIME_FORMAT_MBOX && length >= 5 &&
	     memcmp (buffer, "From ", 5) == 0 && g_mime_reader_tell (reader) >= priv->content_end) ||
	    (priv->format == GMIME_FORMAT_MMDF && length >= 4 &&
	     memcmp (buffer, "\1\1\1\1", 4) == 0)) {
		priv->mailbox_boundary = TRUE;
		return TRUE;
	}

	return FALSE;
}

static gboolean
boundary_in_headers (GMimeReader *reader, gboolean accept_eof)
{
	GMimeReaderPrivate *priv = _g_mime_reader_get_private (reader);
	const guint8 *start, *lf;
	gsize left;

	if (!priv->entity_read && priv->format == GMIME_FORMAT_MESSAGE)
		return FALSE;

	do {
		left = priv->input_end - priv->input_index;
		start = priv->input + priv->input_index;

		if (!left || (start[0] != '-' && start[0] != 'F' && start[0] != '\1'))
			return FALSE;

		if ((lf = memchr (start, '\n', left)) != NULL)
			return check_boundary (reader, start, lf - start);

		if (priv->input_eos)
			return accept_eof && check_boundary (reader, start, left);

		/* Match the reference's bounded lookahead, not an unbounded line
		 * allocation for a malformed header that resembles a delimiter. */
		if (left >= sizeof (priv->input) - PAD_SIZE)
			return FALSE;

		read_ahead (reader, left + 1);
	} while (!priv->error);

	return FALSE;
}

static void
update_header_state (GMimeReader *reader, gsize name_length, gsize field_length, gint64 begin, guint64 line)
{
	GMimeReaderPrivate *priv = _g_mime_reader_get_private (reader);
	const guint8 *header = priv->header->data;
	const char *value = (const char *) header + field_length + 1;
	gsize length = priv->header->len - field_length - 1;

	if (!priv->entity_read && !priv->active_logger)
		return;

	if (header_is (header, name_length, "Content-Type")) {
		char *text = g_strndup (value, length);

		if (priv->content_type) {
			report_at (reader, GMIME_COMPLIANCE_REPEATED_CONTENT_TYPE, begin, line, 1,
				   GMIME_COMPLIANCE_POSITION_ELEMENT_START);
			g_free (text);
			return;
		}

		priv->type_offset = begin;
		priv->type_line = line;

		/* MimeReader retains an intelligible media type on parse failure,
		 * but only installs parameters when the complete list parses. */
		priv->content_type = g_mime_content_type_parse_full (priv->options, value, length, NULL);

		if (!priv->content_type) {
			const char *cursor = text;
			char *media, *subtype;

			if (!g_mime_parse_content_type (&cursor, &media, &subtype)) {
				report_at (reader, GMIME_COMPLIANCE_INVALID_CONTENT_TYPE, begin, line, 1,
					   GMIME_COMPLIANCE_POSITION_ELEMENT_START);
			} else {
				g_free (media);
				g_free (subtype);
			}

			priv->content_type = g_mime_content_type_parse (priv->options, text);
			g_mime_param_list_clear (g_mime_content_type_get_parameters (priv->content_type));
		}

		g_free (text);
	} else if (header_is (header, name_length, "Content-Transfer-Encoding")) {
		gsize i = 0;

		if (priv->seen_encoding) {
			report_at (reader, GMIME_COMPLIANCE_REPEATED_CONTENT_TRANSFER_ENCODING, begin, line, 1,
				   GMIME_COMPLIANCE_POSITION_ELEMENT_START);
			return;
		}

		priv->encoding_offset = begin + field_length + 1;
		priv->encoding_line = line;
		priv->encoding_column = field_length + 2;

		while (i < length && (value[i] == ' ' || value[i] == '\t' || value[i] == '\r' || value[i] == '\n')) {
			if (value[i++] == '\n') {
				priv->encoding_line++;
				priv->encoding_column = 1;
			} else {
				priv->encoding_column++;
			}
		}

		if (i < length) {
			priv->encoding_offset += i;
		} else {
			priv->encoding_line = line;
			priv->encoding_column = field_length + 2;
		}

		priv->encoding = _g_mime_content_encoding_parse (value, length);

		if (priv->encoding == GMIME_CONTENT_ENCODING_UNSPECIFIED)
			report_at (reader, GMIME_COMPLIANCE_INVALID_CONTENT_TRANSFER_ENCODING,
				   priv->encoding_offset, priv->encoding_line, priv->encoding_column,
				   GMIME_COMPLIANCE_POSITION_EXACT);

		priv->seen_encoding = TRUE;
	} else if (priv->content_length < 0 && name_length == 14 &&
		   !g_ascii_strncasecmp ((const char *) header, "Content-Length", 14)) {
		gint64 parsed = 0;
		gsize i = 0;

		while (i < length && g_ascii_isspace (value[i]))
			i++;

		if (i == 0 || i == length || !g_ascii_isdigit (value[i]))
			return;

		do {
			parsed = parsed * 10 + value[i++] - '0';
			if (parsed > G_MAXINT32)
				return;
		} while (i < length && g_ascii_isdigit (value[i]));

		priv->content_length = parsed;
	}
}

static gboolean
detect_invalid_header (GMimeReader *reader, gboolean blanks, gboolean *invalid, gssize *invalid_index,
		       gsize *name_length, gsize *field_length)
{
	GMimeReaderPrivate *priv = _g_mime_reader_get_private (reader);
	const guint8 *start = priv->input + priv->input_index;
	const guint8 *end = priv->input + priv->input_end;
	const guint8 *inptr = start;

	*name_length = 0;
	*invalid_index = -1;

	while (inptr < end && *inptr != ':') {
		/* Blanks may precede the colon, but cannot occur within the name. */
		if (is_blank (*inptr)) {
			if (!blanks) {
				*name_length = inptr - start;
				*invalid_index = inptr - start;
				blanks = TRUE;
			}
		} else if (blanks || *inptr < 32 || *inptr >= 127) {
			*field_length = inptr - start;

			if (!blanks)
				*invalid_index = inptr - start;

			*invalid = TRUE;
			return TRUE;
		}

		inptr++;
	}

	*field_length = inptr - start;

	if (*name_length == 0)
		*name_length = *field_length;

	*invalid = FALSE;

	return inptr < end;
}

typedef struct {
	gboolean null_reported, cr_reported, pending_cr;
	gboolean first_line, blank_line;
	gint64 cr_offset;
	guint64 cr_line, cr_column;
} HeaderDetection;

static void
check_header_span (GMimeReader *reader, HeaderDetection *detection, const guint8 *data, gsize length)
{
	GMimeReaderPrivate *priv = _g_mime_reader_get_private (reader);
	gint64 begin = g_mime_reader_tell (reader);
	guint64 column = (guint64) (begin - priv->line_begin) + 1;
	gsize i;

	if (!priv->active_logger || !length)
		return;

	if (detection->pending_cr && data[0] != '\n' && !detection->cr_reported) {
		report_at (reader, GMIME_COMPLIANCE_BARE_CARRIAGE_RETURN_IN_HEADER,
			   detection->cr_offset, detection->cr_line, detection->cr_column, GMIME_COMPLIANCE_POSITION_EXACT);
		detection->cr_reported = TRUE;
	}

	if (!detection->null_reported && memchr (data, 0, length)) {
		report_at (reader, GMIME_COMPLIANCE_UNEXPECTED_NULL_BYTES_IN_HEADER,
			   priv->line_begin, priv->line_number, 1, GMIME_COMPLIANCE_POSITION_LINE_START);
		detection->null_reported = TRUE;
	}

	for (i = 0; i < length; i++) {
		guint8 c = data[i];

		if (!detection->cr_reported && c == '\r' && i + 1 < length && data[i + 1] != '\n') {
			report_at (reader, GMIME_COMPLIANCE_BARE_CARRIAGE_RETURN_IN_HEADER,
				   begin + i, priv->line_number, column + i, GMIME_COMPLIANCE_POSITION_EXACT);
			detection->cr_reported = TRUE;
		}

		if (c == '\n') {
			if (!(i ? data[i - 1] == '\r' : detection->pending_cr))
				report_at (reader, GMIME_COMPLIANCE_BARE_LINEFEED_IN_HEADER,
					   begin + i, priv->line_number, column + i, GMIME_COMPLIANCE_POSITION_EXACT);

			if (!detection->first_line && detection->blank_line)
				report_at (reader, GMIME_COMPLIANCE_BLANK_HEADER_CONTINUATION_LINE,
					   priv->line_begin, priv->line_number, 1, GMIME_COMPLIANCE_POSITION_LINE_START);

			detection->first_line = FALSE;
			detection->blank_line = TRUE;
		} else if (c != ' ' && c != '\t' && c != '\r') {
			detection->blank_line = FALSE;
		}
	}

	detection->pending_cr = data[length - 1] == '\r';

	if (detection->pending_cr) {
		detection->cr_offset = begin + length - 1;
		detection->cr_line = priv->line_number;
		detection->cr_column = column + length - 1;
	}
}

static gboolean
is_from_line (const guint8 *buffer, gsize length)
{
	if (length && *buffer == '>') {
		buffer++;
		length--;
	}

	return length >= 5 && memcmp (buffer, "From ", 5) == 0;
}

static gboolean
step_headers (GMimeReader *reader, gboolean message)
{
	GMimeReaderPrivate *priv = _g_mime_reader_get_private (reader);
	GMimeReaderClass *klass = GMIME_READER_GET_CLASS (reader);
	guint64 begin_line_number = priv->line_number;
	gint64 begin_offset = g_mime_reader_tell (reader);
	gboolean have_valid_headers = FALSE;
	gboolean have_headers = FALSE;
	gboolean eof = FALSE, invalid_initial_from = FALSE;
	gint64 last_offset = begin_offset;
	guint64 last_line = begin_line_number;

	priv->headers_complete = FALSE;
	priv->header_begin = begin_offset;
	priv->header_line = begin_line_number;
	priv->type_offset = -1;
	priv->encoding_offset = -1;
	priv->seen_message = priv->seen_resent = 0;
	priv->seen_mime_version = FALSE;
	priv->message_headers = message;
	priv->content_length = -1;

	if (priv->entity_read || priv->active_logger) {
		g_clear_object (&priv->content_type);
		priv->seen_encoding = FALSE;
		priv->encoding = GMIME_CONTENT_ENCODING_UNSPECIFIED;
	}

	if (!callback_result (reader, klass->headers_begin (reader, begin_offset, begin_line_number, &priv->error)))
		return FALSE;

	read_ahead (reader, priv->stop_at_separator ? 1 : READ_AHEAD_SIZE);

	if (priv->error)
		return FALSE;

	do {
		HeaderDetection detection = { FALSE, FALSE, FALSE, TRUE, FALSE, 0, 0, 0 };
		guint64 line_number = priv->line_number;
		gint64 offset = g_mime_reader_tell (reader);
		gsize left = priv->input_end - priv->input_index;
		gsize field_length, name_length;
		gsize field_prefix = 0;
		gssize invalid_index;
		gboolean invalid, midline = TRUE;
		gboolean name_blanks = FALSE;
		const guint8 *start;

		last_offset = offset;
		last_line = line_number;
		g_byte_array_set_size (priv->header, 0);

		if (left < 2 && (!priv->stop_at_separator || !left || priv->input[priv->input_index] == '\r'))
			left = read_ahead_at_least (reader, 2);

		if (priv->error)
			return FALSE;

		if (left == 0) {
			/* Empty input has a begin callback but no matching end callback. */
			if (!have_valid_headers && (!priv->entity_read || priv->toplevel))
				return TRUE;

			report_at (reader, GMIME_COMPLIANCE_MISSING_BODY_SEPARATOR, offset, line_number, 1,
				   GMIME_COMPLIANCE_POSITION_ELEMENT_START);
			break;
		}

		start = priv->input + priv->input_index;

		if (*start == '\n' || (*start == '\r' && (left == 1 || start[1] == '\n'))) {
			gsize count = *start == '\r' && left > 1 ? 2 : 1;

			if (*start == '\n')
				report_at (reader, GMIME_COMPLIANCE_BARE_LINEFEED_IN_HEADER, offset, line_number, 1,
					   GMIME_COMPLIANCE_POSITION_EXACT);

			consume_input (reader, count);

			/* A final bare CR is a separator in the header state. */
			if (count == 1 && *start == '\r') {
				priv->prev_line_begin = priv->line_begin;
				priv->line_begin = g_mime_reader_tell (reader);
				priv->line_number++;
			}

			if (!callback_result (reader, klass->body_separator (reader, offset, line_number,
									  g_mime_reader_tell (reader), &priv->error)))
				return FALSE;
			break;
		}

		while (!detect_invalid_header (reader, name_blanks, &invalid, &invalid_index, &name_length, &field_length)) {
			gsize atleast = priv->input_end - priv->input_index + 1;

			if (read_ahead (reader, atleast) < atleast) {
				/* A valid field name can exceed the fixed read-ahead
				 * buffer. Spill validated bytes into the header budget,
				 * retaining whether whitespace before the colon began. */
				if (!priv->error && !priv->input_eos &&
				    atleast > sizeof (priv->input) - PAD_SIZE) {
					gsize count = priv->input_end - priv->input_index;

					if (!append_header (reader, priv->input + priv->input_index, count, offset))
						return FALSE;

					name_blanks = is_blank (priv->input[priv->input_end - 1]);
					consume_input (reader, count);
					field_prefix += count;
					continue;
				}

				invalid = TRUE;
				invalid_index = -1;
				break;
			}
		}

		if (priv->error)
			return FALSE;

		if (invalid_index >= 0)
			invalid_index += field_prefix;

		start = priv->input + priv->input_index;
		left = priv->input_end - priv->input_index;

		if (invalid) {
			if (!field_prefix && boundary_in_headers (reader, FALSE)) {
				_g_mime_parser_options_warn (priv->options, offset,
							    have_headers ? GMIME_WARN_PART_WITHOUT_CONTENT :
							    GMIME_CRIT_PART_WITHOUT_HEADERS_OR_CONTENT, NULL);
				report_at (reader, GMIME_COMPLIANCE_MISSING_BODY_SEPARATOR, offset, line_number, 1,
					   GMIME_COMPLIANCE_POSITION_ELEMENT_START);
				break;
			}

			if (priv->error)
				return FALSE;

			start = priv->input + priv->input_index;
			left = priv->input_end - priv->input_index;

			/* Message headers tolerate leading (munged) From-lines, but
			 * header/entity reads reject them. They remain invalid wire
			 * headers, not mailbox delimiters in this input format. */
			if (!field_prefix && (*start == 'F' || *start == '>')) {
				gsize need = *start == '>' ? 6 : 5;

				if (left < need)
					left = read_ahead_at_least (reader, need);

				if (priv->error)
					return FALSE;

				if (priv->format == GMIME_FORMAT_MBOX && left >= 5 &&
				    memcmp (priv->input + priv->input_index, "From ", 5) == 0 &&
				    g_mime_reader_tell (reader) >= priv->content_end) {
					priv->mailbox_boundary = TRUE;
					report_at (reader, GMIME_COMPLIANCE_MISSING_BODY_SEPARATOR, offset, line_number, 1,
						   GMIME_COMPLIANCE_POSITION_ELEMENT_START);
					break;
				}

				if ((!priv->entity_read || priv->toplevel) && !have_valid_headers && left >= need) {
					if (!message || !is_from_line (priv->input + priv->input_index, left)) {
						invalid_initial_from = TRUE;
						report_at (reader, GMIME_COMPLIANCE_MISSING_BODY_SEPARATOR, offset, line_number, 1,
							   GMIME_COMPLIANCE_POSITION_ELEMENT_START);
						break;
					}
				}
			}

			report_at (reader, GMIME_COMPLIANCE_INVALID_HEADER,
				   invalid_index >= 0 ? offset + invalid_index : offset, line_number,
				   invalid_index >= 0 ? (guint64) invalid_index + 1 : 1,
				   invalid_index >= 0 ? GMIME_COMPLIANCE_POSITION_EXACT : GMIME_COMPLIANCE_POSITION_ELEMENT_START);

			if ((!priv->entity_read || priv->toplevel) && !have_headers && priv->input_eos && field_length >= left)
				return invalid_headers (reader, offset);
		} else {
			if (!append_header (reader, start, field_length + 1, offset))
				return FALSE;

			consume_input (reader, field_length + 1);
		}

		do {
			const guint8 *end = priv->input + priv->input_end;
			const guint8 *inptr;

			start = priv->input + priv->input_index;
			inptr = start;

			while (inptr < end && (midline || is_blank (*inptr))) {
				const guint8 *lf = memchr (inptr, '\n', end - inptr);
				gsize count = lf ? (gsize) (lf - inptr + 1) : (gsize) (end - inptr);

				if (!append_header (reader, inptr, count, offset))
					return FALSE;

				check_header_span (reader, &detection, inptr, count);
				consume_input (reader, count);

				if (!lf) {
					inptr = end;
					midline = TRUE;
					break;
				}

				inptr = lf + 1;
				midline = FALSE;
			}

			if (inptr < end)
				break;

			if (read_ahead (reader, 1) == 0) {
				if (priv->error)
					return FALSE;

				eof = TRUE;

				if (priv->active_logger) {
					gint64 end_offset = g_mime_reader_tell (reader);
					guint64 column = (guint64) (end_offset - priv->line_begin) + 1;

					if (midline) {
						if (detection.pending_cr && !detection.cr_reported)
							report_at (reader, GMIME_COMPLIANCE_BARE_CARRIAGE_RETURN_IN_HEADER,
								   detection.cr_offset, detection.cr_line, detection.cr_column,
								   GMIME_COMPLIANCE_POSITION_EXACT);

						if (!detection.first_line && detection.blank_line)
							report_at (reader, GMIME_COMPLIANCE_BLANK_HEADER_CONTINUATION_LINE,
								   priv->line_begin, priv->line_number, 1, GMIME_COMPLIANCE_POSITION_LINE_START);
					}

					report_at (reader, midline ? GMIME_COMPLIANCE_INCOMPLETE_HEADER :
						   GMIME_COMPLIANCE_MISSING_BODY_SEPARATOR, end_offset, priv->line_number,
						   column, GMIME_COMPLIANCE_POSITION_EXACT);
				}

				break;
			}
		} while (TRUE);

		if ((!priv->entity_read || priv->toplevel) && !have_valid_headers && invalid) {
			if (!message || !is_from_line (priv->header->data, priv->header->len))
				return invalid_headers (reader, offset);

			/* A stream containing only From-lines is not a message. */
			if (eof)
				return TRUE;
		}

		if (invalid) {
			check_header_semantics (reader, name_length, field_length, TRUE, offset, line_number);
			name_length = field_length = priv->header->len;
			_g_mime_parser_options_warn (priv->options, offset, GMIME_CRIT_INVALID_HEADER_NAME, NULL);
		} else {
			have_valid_headers = TRUE;
			if (field_prefix) {
				name_length = field_prefix + field_length;
				while (name_length && is_blank (priv->header->data[name_length - 1]))
					name_length--;
			}
			field_length += field_prefix;
			check_header_semantics (reader, name_length, field_length, FALSE, offset, line_number);
			update_header_state (reader, name_length, field_length, offset, line_number);
		}

		have_headers = TRUE;

		if (!callback_result (reader, klass->header_read (reader, priv->header->data, priv->header->len,
								 name_length, field_length, invalid, offset,
								 line_number, &priv->error)))
			return FALSE;
	} while (!eof);

	check_missing_headers (reader, eof ? g_mime_reader_tell (reader) : last_offset,
			       eof ? priv->line_number : last_line, eof);

	priv->headers_complete = TRUE;

	if (!callback_result (reader, klass->headers_end (reader, begin_offset, begin_line_number,
							g_mime_reader_tell (reader), priv->line_number, &priv->error)))
		return FALSE;

	/* MimeReader completes the empty header-block notification before
	 * rejecting a leading From-line in a header-only operation. */
	return !invalid_initial_from || invalid_headers (reader, g_mime_reader_tell (reader));
}

/**
 * g_mime_reader_read_headers:
 * @reader: a #GMimeReader with an input stream
 * @error: return location for a #GError, or %NULL
 *
 * Reads one header block at the current logical position. A terminating
 * blank line is consumed and reported before headers_end. Input may be read
 * ahead; use g_mime_reader_tell(), not the underlying stream position.
 * Folded headers, original bytes, offsets and 1-based line numbers are
 * preserved. Invalid trailing headers are delivered with the invalid flag set in the
 * header_read callback. Invalid initial headers and header length violations
 * fail with %GMIME_ERROR_PARSE_ERROR. I/O failures use errno, or
 * %GMIME_ERROR_GENERAL when the stream provides none.
 *
 * Empty input succeeds with only a headers_begin callback. A callback failure
 * aborts without further callbacks; earlier callbacks are not rolled back.
 * Errors are sticky until g_mime_reader_set_stream() is called.
 * This operation neither scans for mailbox markers nor skips a UTF-8 BOM.
 * Reentrant or concurrent operations on the same reader are not supported.
 *
 * Returns: %TRUE on success, or %FALSE on failure.
 **/
gboolean
g_mime_reader_read_headers (GMimeReader *reader, GError **error)
{
	struct _GMimeReaderPrivate *priv;
	gboolean result = FALSE;

	g_return_val_if_fail (GMIME_IS_READER (reader), FALSE);

	priv = _g_mime_reader_get_private (reader);

	g_return_val_if_fail (GMIME_IS_STREAM (priv->stream), FALSE);
	g_return_val_if_fail (!priv->reading, FALSE);
	g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

	/* Keep the reader and its borrowed callback buffers alive even if a
	 * callback releases the caller's last reference. */
	g_object_ref (reader);
	priv->reading = TRUE;
	priv->toplevel = TRUE;
	begin_compliance (reader);

	if (!priv->error) {
		priv->initial = FALSE;
		result = step_headers (reader, FALSE);

		if (result)
			priv->complete_eos = priv->input_eos && priv->input_index == priv->input_end;
	}

	if (priv->error)
		g_propagate_error (error, g_error_copy (priv->error));

	end_compliance (reader);
	priv->reading = FALSE;
	g_object_unref (reader);

	return result;
}

/* The public parser's header-loading contract must not probe for a body
 * byte merely to distinguish a final LF separator from EOF. */
gboolean
_g_mime_reader_read_headers_to_separator (GMimeReader *reader, GError **error)
{
	gboolean result;
	GMimeReaderPrivate *priv = _g_mime_reader_get_private (reader);

	g_return_val_if_fail (!priv->reading, FALSE);

	priv->stop_at_separator = TRUE;
	result = g_mime_reader_read_headers (reader, error);
	priv->stop_at_separator = FALSE;
	return result;
}

/* A callback may either return FALSE or set an error. Both abort traversal,
 * including callbacks whose implementation accidentally returns TRUE. */
#define READER_CALLBACK(reader, priv, name, ...) \
	(!(GMIME_READER_GET_CLASS (reader)->name) || \
	 callback_result ((reader), GMIME_READER_GET_CLASS (reader)->name ((reader), __VA_ARGS__, &(priv)->error)))

static gint64
entity_end_offset (GMimeReader *reader, gint64 begin)
{
	GMimeReaderPrivate *priv = _g_mime_reader_get_private (reader);
	gint64 end = g_mime_reader_tell (reader);

	if (!priv->boundary_eos && priv->previous_count && priv->previous[1] == '\n') {
		end--;
		if (priv->previous_count == 2 && priv->previous[0] == '\r')
			end--;
	}

	return MAX (end, begin);
}

static guint64
content_lines (GMimeReader *reader, gint64 begin, guint64 begin_line, gint64 end)
{
	GMimeReaderPrivate *priv = _g_mime_reader_get_private (reader);
	guint64 lines = priv->line_number - begin_line;

	if (priv->line_begin >= begin && end > priv->line_begin)
		lines++;

	if (!priv->boundary_eos && end == priv->prev_line_begin && priv->prev_line_begin >= begin && lines)
		lines--;

	return lines;
}

static gsize
boundary_read_ahead (GMimeReader *reader)
{
	GMimeReaderPrivate *priv = _g_mime_reader_get_private (reader);
	gsize length = READ_AHEAD_SIZE;
	guint i;

	for (i = 0; i < priv->frames->len; i++) {
		EntityFrame *frame = g_ptr_array_index (priv->frames, i);

		if (frame->boundary_active)
			length = MAX (length, strlen (frame->boundary) + 6);
	}

	return MIN (length, sizeof (priv->input) - PAD_SIZE);
}

static void
report_encoding_issue (const GMimeEncodingIssue *issue, gpointer user_data)
{
	static const GMimeComplianceViolation violations[] = {
		GMIME_COMPLIANCE_INVALID_BASE64_CHARACTER,
		GMIME_COMPLIANCE_OBSOLETE_BASE64_COMMENT,
		GMIME_COMPLIANCE_INVALID_BASE64_PADDING,
		GMIME_COMPLIANCE_BASE64_CHARACTERS_AFTER_PADDING,
		GMIME_COMPLIANCE_INCOMPLETE_BASE64_QUANTUM,
		GMIME_COMPLIANCE_INVALID_QUOTED_PRINTABLE_ENCODING,
		GMIME_COMPLIANCE_INVALID_QUOTED_PRINTABLE_SOFT_BREAK,
		GMIME_COMPLIANCE_INVALID_UUENCODE_PRETEXT,
		GMIME_COMPLIANCE_INVALID_UUENCODE_FILE_MODE,
		GMIME_COMPLIANCE_INCOMPLETE_UUENCODED_LINE,
		GMIME_COMPLIANCE_INVALID_UUENCODED_LINE_LENGTH,
		GMIME_COMPLIANCE_INVALID_UUENCODED_CONTENT,
		GMIME_COMPLIANCE_INVALID_UUENCODED_LINE_EXTRA_DATA,
		GMIME_COMPLIANCE_INVALID_UUENCODE_END_MARKER,
		GMIME_COMPLIANCE_INCOMPLETE_UUENCODED_CONTENT
	};

	g_assert ((guint) issue->violation < G_N_ELEMENTS (violations));

	report_at (user_data, violations[issue->violation], issue->stream_offset,
		   issue->line_number, issue->column_number, GMIME_COMPLIANCE_POSITION_EXACT);
}

static void
consume_scanned_lines (GMimeReader *reader, const GMimeReaderScan *scan)
{
	GMimeReaderPrivate *priv = _g_mime_reader_get_private (reader);
	gint64 begin = g_mime_reader_tell (reader);
	const guint8 *data = priv->input + priv->input_index;

	/* Update line state in bulk, not with another per-byte pass that would
	 * erase the benefit of vectorized scanning. The previous two bytes
	 * still describe the physical wire after the next buffer refill. */
	priv->prev_line_begin = scan->lines > 1 ? begin + scan->previous_line : priv->line_begin;
	priv->line_begin = begin + scan->length;
	priv->line_number += scan->lines;
	priv->previous[0] = scan->length > 1 ? data[scan->length - 2] : priv->previous[1];
	priv->previous[1] = data[scan->length - 1];
	priv->previous_count = MIN (2, priv->previous_count + scan->length);
	priv->input_index += scan->length;
}

static gboolean
is_partial_boundary (GMimeReader *reader, const guint8 *start, gsize length)
{
	GMimeReaderPrivate *priv = _g_mime_reader_get_private (reader);
	guint i;

	if (start[0] == '-') {
		for (i = priv->frames->len; i > 0; i--) {
			EntityFrame *frame = g_ptr_array_index (priv->frames, i - 1);

			if (frame->boundary_active)
				return length < 2 || start[1] == '-';
		}
	}

	if (priv->format == GMIME_FORMAT_MBOX)
		return start[0] == 'F' && memcmp (start, "From ", MIN (length, 5)) == 0 &&
			g_mime_reader_tell (reader) >= priv->content_end;

	return priv->format == GMIME_FORMAT_MMDF && start[0] == '\1';
}

static gboolean
deliver_content_batch (GMimeReader *reader, GMimeReaderContentType type, guint8 *output,
		       gsize *buffered, gsize *pending, gboolean trim, gint64 *delivered)
{
	GMimeReaderPrivate *priv = _g_mime_reader_get_private (reader);
	gsize count, withheld = 0;

	if (!*buffered)
		return TRUE;

	count = *pending + *buffered;

	/* The newline before a boundary belongs to the boundary, including a
	 * CRLF split between input batches. Keep it until the next scan. */
	if (trim && output[count - 1] == '\n')
		withheld = count > 1 && output[count - 2] == '\r' ? 2 : 1;
	else if (trim && output[count - 1] == '\r')
		withheld = 1;

	count -= withheld;

	if (count && !READER_CALLBACK (reader, priv, content_read, type, output, count))
		return FALSE;

	*delivered += count;
	memmove (output, output + count, withheld);
	*pending = withheld;
	*buffered = 0;

	return TRUE;
}

static gboolean
scan_content (GMimeReader *reader, GMimeReaderContentType type)
{
	GMimeReaderPrivate *priv = _g_mime_reader_get_private (reader);
	gint64 begin = g_mime_reader_tell (reader), delivered = 0;
	guint64 begin_line = priv->line_number;
	guint8 output[READ_AHEAD_SIZE + BLOCK_SIZE + 2];
	gsize npending = 0, buffered = 0;
	GMimeReaderNewLineFormat newline_format = GMIME_READER_NEWLINE_UNKNOWN;
	gboolean midline = FALSE, incomplete = FALSE, trim = type != GMIME_READER_CONTENT_PREAMBLE;
	gboolean refill = TRUE;
	EntityFrame *frame = g_ptr_array_index (priv->frames, priv->frames->len - 1);
	gboolean detect_8bit = frame->encoding != GMIME_CONTENT_ENCODING_BINARY &&
		frame->encoding != GMIME_CONTENT_ENCODING_8BIT;
	gboolean detect_null = frame->encoding != GMIME_CONTENT_ENCODING_BINARY;
	gsize validator_start = priv->input_index;

	if (!READER_CALLBACK (reader, priv, content_begin, type, begin, begin_line))
		return FALSE;

	if (priv->active_logger && type == GMIME_READER_CONTENT_PART) {
		if (frame->encoding == GMIME_CONTENT_ENCODING_BASE64 && (priv->validators & GMIME_COMPLIANCE_VALIDATOR_BASE64))
			priv->validator = _g_mime_base64_validator_new (report_encoding_issue, reader, begin, begin_line);
		else if (frame->encoding == GMIME_CONTENT_ENCODING_QUOTEDPRINTABLE && (priv->validators & GMIME_COMPLIANCE_VALIDATOR_QUOTED_PRINTABLE))
			priv->validator = _g_mime_quoted_printable_validator_new (report_encoding_issue, reader, begin, begin_line);
		else if (frame->encoding == GMIME_CONTENT_ENCODING_UUENCODE && (priv->validators & GMIME_COMPLIANCE_VALIDATOR_UUENCODE))
			priv->validator = _g_mime_uu_validator_new (report_encoding_issue, reader, begin, begin_line);
	}

	while (!priv->boundary && !priv->mailbox_boundary && !priv->boundary_eos) {
		gsize atleast = boundary_read_ahead (reader);
		GMimeReaderScan scan = { 0 };
		const guint8 *start, *lf;
		gsize left, length;

		if (incomplete)
			atleast = MAX (atleast, priv->input_end - priv->input_index + 1);

		/* Scan the buffered batch before refilling, as MimeReader does.
		 * Probing EOF between complete lines can incorrectly promote an
		 * oversized, incomplete delimiter from content to a boundary. */
		if (refill && priv->validator && priv->input_index > validator_start)
			_g_mime_encoding_validator_write (priv->validator, priv->input + validator_start,
							 priv->input_index - validator_start);

		/* Generic checks run over the scanned input batch before encoding
		 * validation, which in turn precedes its raw content callback. */
		if (refill && !deliver_content_batch (reader, type, output, &buffered, &npending, trim, &delivered))
			return FALSE;

		left = refill ? read_ahead (reader, atleast) : priv->input_end - priv->input_index;

		if (refill)
			validator_start = priv->input_index;

		refill = FALSE;

		if (priv->error)
			return FALSE;

		if (!left) {
			priv->boundary_eos = TRUE;
			break;
		}

		start = priv->input + priv->input_index;

		if (!priv->active_logger && !midline && start[0] != '-' &&
		    start[0] != (priv->format == GMIME_FORMAT_MBOX ? 'F' : priv->format == GMIME_FORMAT_MMDF ? '\1' : '-'))
			scan = _g_mime_reader_scan_lines (start, left,
				priv->format == GMIME_FORMAT_MBOX ? 'F' : priv->format == GMIME_FORMAT_MMDF ? '\1' : '-',
				priv->format == GMIME_FORMAT_MBOX ? 'r' : priv->format == GMIME_FORMAT_MMDF ? '\1' : '-');

		lf = memchr (start, '\n', left);

		if (scan.length) {
			lf = start + scan.length - 1;
			if (type == GMIME_READER_CONTENT_PART)
				newline_format |= scan.formats;
		}

		length = lf ? (gsize) (lf - start) : left;

		if (priv->active_logger) {
			gint64 offset = g_mime_reader_tell (reader);
			guint64 column = (guint64) (offset - priv->line_begin) + 1;
			gsize i;

			if (detect_8bit) {
				for (i = 0; i < length && start[i] < 128; i++)
					;

				if (i < length) {
					report_at (reader, GMIME_COMPLIANCE_UNEXPECTED_8BIT_BYTES_IN_BODY,
						   offset, priv->line_number, column, GMIME_COMPLIANCE_POSITION_EXACT);
					detect_8bit = FALSE;
				}
			}

			if (detect_null && memchr (start, 0, length)) {
				report_at (reader, GMIME_COMPLIANCE_UNEXPECTED_NULL_BYTES_IN_BODY,
					   offset, priv->line_number, column, GMIME_COMPLIANCE_POSITION_EXACT);
				detect_null = FALSE;
			}
		}

		if (!midline && (lf || priv->input_eos) && check_boundary (reader, start, length))
			break;

		if (!lf && !priv->input_eos && !midline && length < 1000 &&
		    is_partial_boundary (reader, start, length)) {
			/* Keep a possible delimiter intact until its end is known. A
			 * long incomplete line cannot be a valid SMTP delimiter. */
			incomplete = TRUE;
			refill = TRUE;
			continue;
		}

		incomplete = FALSE;

		if (lf) {
			/* A CR may have been consumed by the preceding short read.
			 * Count the terminator before trimming boundary-owned bytes,
			 * just as MimeReader.ScanContent() does. */
			gboolean cr = length ? start[length - 1] == '\r' :
					(midline && priv->previous_count && priv->previous[1] == '\r');

			if (!cr)
				report_at (reader, GMIME_COMPLIANCE_BARE_LINEFEED_IN_BODY,
					   g_mime_reader_tell (reader) + length, priv->line_number,
					   (guint64) (g_mime_reader_tell (reader) + length - priv->line_begin) + 1,
					   GMIME_COMPLIANCE_POSITION_EXACT);

			if (type == GMIME_READER_CONTENT_PART)
				newline_format |= cr ? GMIME_READER_NEWLINE_DOS : GMIME_READER_NEWLINE_UNIX;

			length++;
		} else if (priv->active_logger && priv->input_eos &&
			   g_mime_reader_tell (reader) + length - priv->line_begin > 1000) {
			report_at (reader, GMIME_COMPLIANCE_OVERSIZED_LINE, priv->line_begin,
				   priv->line_number, 1, GMIME_COMPLIANCE_POSITION_LINE_START);
		}

		midline = !lf && !priv->input_eos;

		memcpy (output + npending + buffered, start, length);
		buffered += length;

		if (scan.length)
			consume_scanned_lines (reader, &scan);
		else
			consume_input (reader, length);

		refill = priv->input_index == priv->input_end;
	}

	if (priv->validator && priv->input_index > validator_start)
		_g_mime_encoding_validator_write (priv->validator, priv->input + validator_start,
						 priv->input_index - validator_start);

	if (!deliver_content_batch (reader, type, output, &buffered, &npending, trim, &delivered))
		return FALSE;

	if (priv->validator) {
		_g_mime_encoding_validator_flush (priv->validator);
		g_clear_object (&priv->validator);
	}

	if (npending && priv->boundary_eos) {
		if (!READER_CALLBACK (reader, priv, content_read, type, output, npending))
			return FALSE;
		delivered += npending;
	}

	return READER_CALLBACK (reader, priv, content_end, type, begin, begin_line, begin + delivered,
				content_lines (reader, begin, begin_line, begin + delivered), newline_format);
}

static gboolean
skip_boundary (GMimeReader *reader, gboolean *terminated)
{
	GMimeReaderPrivate *priv = _g_mime_reader_get_private (reader);
	gint64 begin = g_mime_reader_tell (reader);
	guint64 line = priv->line_number;
	gboolean final = priv->final_boundary;

	if (!READER_CALLBACK (reader, priv, boundary_begin, final, begin, line))
		return FALSE;

	do {
		const guint8 *start = priv->input + priv->input_index;
		gsize left = priv->input_end - priv->input_index;
		const guint8 *lf = memchr (start, '\n', left);
		gsize length = lf ? (gsize) (lf - start) : left;
		gint64 offset = g_mime_reader_tell (reader);

		if (lf && !final) {
			if (length == 0 || start[length - 1] != '\r')
				report_at (reader, GMIME_COMPLIANCE_BARE_LINEFEED_IN_BODY, offset + length,
					   line, (guint64) (offset + length - priv->line_begin) + 1,
					   GMIME_COMPLIANCE_POSITION_EXACT);
			length++;
		} else if (lf && length && start[length - 1] == '\r') {
			length--;
		}

		if (length) {
			consume_input (reader, length);
			if (!READER_CALLBACK (reader, priv, boundary_read, final, start, length, offset, line))
				return FALSE;
		}

		if (terminated)
			*terminated = lf != NULL;

		if (lf || priv->input_eos)
			break;

		if (!read_ahead (reader, 1)) {
			if (priv->error)
				return FALSE;
			break;
		}
	} while (TRUE);

	priv->boundary = NULL;

	return READER_CALLBACK (reader, priv, boundary_end, final, begin, line, g_mime_reader_tell (reader));
}

static gboolean
is_message_type (GMimeContentType *type)
{
	static const char *subtypes[] = { "rfc822", "news", "global", "global-headers", "external-body", "rfc2822" };
	guint i;

	if (g_mime_content_type_is_type (type, "text", "rfc822-headers"))
		return TRUE;

	for (i = 0; i < G_N_ELEMENTS (subtypes); i++) {
		if (g_mime_content_type_is_type (type, "message", subtypes[i]))
			return TRUE;
	}

	return FALSE;
}

static void
entity_frame_free (gpointer data)
{
	EntityFrame *frame = data;

	g_object_unref (frame->content_type);
	g_free (frame->boundary);
	g_free (frame);
}

static EntityFrame *
push_entity (GMimeReader *reader, EntityFrame *parent, gint64 begin, guint64 line)
{
	GMimeReaderPrivate *priv = _g_mime_reader_get_private (reader);
	EntityFrame *frame = g_new0 (EntityFrame, 1);
	gboolean encoded = priv->encoding == GMIME_CONTENT_ENCODING_BASE64 ||
		priv->encoding == GMIME_CONTENT_ENCODING_QUOTEDPRINTABLE ||
		priv->encoding == GMIME_CONTENT_ENCODING_UUENCODE;
	int max_depth = g_mime_parser_options_get_max_mime_depth (priv->options);

	frame->content_type = g_steal_pointer (&priv->content_type);
	frame->encoding = priv->encoding;
	frame->type_offset = priv->type_offset;
	frame->type_line = priv->type_line;

	if (!frame->content_type) {
		if (parent && g_mime_content_type_is_type (parent->content_type, "multipart", "digest"))
			frame->content_type = g_mime_content_type_new ("message", "rfc822");
		else
			frame->content_type = g_mime_content_type_new ("text", "plain");
	}

	if (priv->active_logger) {
		if (g_mime_content_type_is_type (frame->content_type, "multipart", "*") &&
		    priv->seen_encoding && priv->encoding != GMIME_CONTENT_ENCODING_7BIT &&
		    priv->encoding != GMIME_CONTENT_ENCODING_8BIT)
			report_at (reader, GMIME_COMPLIANCE_ILLEGAL_MULTIPART_CONTENT_TRANSFER_ENCODING,
				   priv->encoding_offset, priv->encoding_line, priv->encoding_column,
				   GMIME_COMPLIANCE_POSITION_EXACT);
		else if (is_message_type (frame->content_type) && encoded)
			report_at (reader, GMIME_COMPLIANCE_ILLEGAL_MESSAGE_RFC822_CONTENT_TRANSFER_ENCODING,
				   priv->encoding_offset, priv->encoding_line, priv->encoding_column,
				   GMIME_COMPLIANCE_POSITION_EXACT);
	}

	if (!priv->reported_depth && priv->frames->len >= (guint) max_depth &&
	    (g_mime_content_type_is_type (frame->content_type, "multipart", "*") ||
	     (!encoded && is_message_type (frame->content_type)))) {
		priv->reported_depth = TRUE;
		_g_mime_parser_options_warn (priv->options, priv->header_begin, GMIME_CRIT_NESTING_OVERFLOW, NULL);
		report_at (reader, GMIME_COMPLIANCE_MIME_DEPTH_LIMIT_EXCEEDED,
			   priv->type_offset < 0 ? priv->header_begin : priv->type_offset,
			   priv->type_offset < 0 ? priv->header_line : priv->type_line, 1,
			   GMIME_COMPLIANCE_POSITION_ELEMENT_START);
	}

	if (priv->frames->len < (guint) max_depth) {
		if (g_mime_content_type_is_type (frame->content_type, "multipart", "*"))
			frame->type = GMIME_READER_ENTITY_MULTIPART;
		else if (!encoded && is_message_type (frame->content_type))
			frame->type = GMIME_READER_ENTITY_MESSAGE_PART;
	}

	frame->begin = begin;
	frame->begin_line = line;
	frame->headers_end = g_mime_reader_tell (reader);
	frame->content_begin = frame->headers_end;
	frame->content_line = priv->line_number;
	frame->boundary = g_strdup (g_mime_content_type_get_boundary (frame->content_type));
	g_ptr_array_add (priv->frames, frame);

	return frame;
}

static gboolean
begin_entity (GMimeReader *reader, EntityFrame *frame)
{
	GMimeReaderPrivate *priv = _g_mime_reader_get_private (reader);

	return READER_CALLBACK (reader, priv, entity_begin, frame->type, frame->content_type, frame->begin, frame->begin_line);
}

static gboolean
read_child_headers (GMimeReader *reader, EntityFrame *parent, gboolean message)
{
	GMimeReaderPrivate *priv = _g_mime_reader_get_private (reader);
	gint64 begin = g_mime_reader_tell (reader);
	guint64 line = priv->line_number;
	EntityFrame *child;

	priv->toplevel = FALSE;

	if (!step_headers (reader, message))
		return FALSE;

	if (message) {
		parent->message_begin = begin;
		parent->message_line = line;
		parent->message_headers_end = g_mime_reader_tell (reader);
		parent->message_active = TRUE;

		if (!READER_CALLBACK (reader, priv, message_begin, begin, line))
			return FALSE;
	}

	parent->state = FRAME_AWAIT_CHILD;
	child = push_entity (reader, message ? NULL : parent, begin, line);

	return begin_entity (reader, child);
}

static gboolean
step_entity (GMimeReader *reader, gboolean message)
{
	GMimeReaderPrivate *priv = _g_mime_reader_get_private (reader);
	gint64 begin = g_mime_reader_tell (reader);
	guint64 line = priv->line_number;
	EntityFrame *frame;

	priv->entity_read = TRUE;
	priv->toplevel = TRUE;
	priv->boundary = NULL;
	priv->mailbox_boundary = FALSE;
	priv->boundary_eos = FALSE;
	priv->frames = g_ptr_array_new_with_free_func (entity_frame_free);

	if (!step_headers (reader, message))
		return FALSE;

	if (!priv->headers_complete)
		return invalid_headers (reader, begin);

	if (message && !READER_CALLBACK (reader, priv, message_begin, begin, line))
		return FALSE;

	if (message) {
		priv->content_end = 0;

		if (priv->format == GMIME_FORMAT_MBOX && priv->content_length >= 0 &&
		    g_mime_parser_options_get_respect_content_length (priv->options)) {
			gint64 offset = g_mime_reader_tell (reader);

			if (priv->content_length > G_MAXINT64 - offset) {
				g_set_error_literal (&priv->error, GMIME_ERROR, GMIME_ERROR_PARSE_ERROR,
						     "Content-Length exceeds the stream offset range.");
				return FALSE;
			}

			priv->content_end = offset + priv->content_length;
		}
	}

	frame = push_entity (reader, NULL, begin, line);

	if (!begin_entity (reader, frame))
		return FALSE;

	/* Only the top frame runs. Parents wait without consuming input so
	 * that any ancestor delimiter can terminate arbitrarily deep,
	 * truncated descendants without recursion or a call-stack limit. */
	while (priv->frames->len) {
		gint64 end;
		guint64 lines;

		frame = g_ptr_array_index (priv->frames, priv->frames->len - 1);

		switch (frame->state) {
		case FRAME_BEGIN:
			if (frame->type == GMIME_READER_ENTITY_PART) {
				if (!scan_content (reader, GMIME_READER_CONTENT_PART))
					return FALSE;

				frame->state = FRAME_END;
			} else if (frame->type == GMIME_READER_ENTITY_MULTIPART) {
				if (priv->active_logger && (!frame->boundary || !_g_mime_is_valid_boundary (frame->boundary)))
					report_at (reader, frame->boundary ? GMIME_COMPLIANCE_INVALID_MULTIPART_BOUNDARY_PARAMETER :
						   GMIME_COMPLIANCE_MISSING_MULTIPART_BOUNDARY_PARAMETER,
						   frame->type_offset, frame->type_line, 1, GMIME_COMPLIANCE_POSITION_ELEMENT_START);
				frame->boundary_active = frame->boundary != NULL;

				if (!scan_content (reader, GMIME_READER_CONTENT_PREAMBLE))
					return FALSE;

				frame->state = priv->boundary == frame && !priv->final_boundary ? FRAME_NEXT_CHILD : FRAME_END;
			} else {
				gboolean have_boundary = FALSE, found;
				const guint8 *start, *lf;
				gsize length;
				guint i;

				/* A boundary immediately after message/rfc822 headers
				 * means the embedded message is absent, not a new child.
				 * Short reads must not leave the probe with only part
				 * of a marker. The preceding newline remains in previous[]
				 * for entity_end_offset(), even across buffer refills. */
				read_ahead_at_least (reader, boundary_read_ahead (reader));

				if (priv->error)
					return FALSE;

				for (i = 0; i < priv->frames->len; i++) {
					EntityFrame *ancestor = g_ptr_array_index (priv->frames, i);

					have_boundary |= ancestor->boundary_active;
				}

				/* Probe only the buffered first line, not an entire
				 * potentially very long mailbox envelope. */
				start = priv->input + priv->input_index;
				lf = memchr (start, '\n', priv->input_end - priv->input_index);
				length = lf ? (gsize) (lf - start) : priv->input_end - priv->input_index;
				found = check_boundary (reader, start, length);

				if (((have_boundary || priv->format != GMIME_FORMAT_MESSAGE) &&
				     priv->input_index == priv->input_end) || found) {
					if (found && priv->active_logger) {
						if (!length || start[length - 1] != '\r')
							report_at (reader, GMIME_COMPLIANCE_BARE_LINEFEED_IN_BODY,
								   g_mime_reader_tell (reader) + length, priv->line_number,
								   (guint64) (g_mime_reader_tell (reader) + length - priv->line_begin) + 1,
								   GMIME_COMPLIANCE_POSITION_EXACT);
					}

					priv->boundary_eos = !priv->boundary && !priv->mailbox_boundary;
					frame->state = FRAME_END;
				} else if (!read_child_headers (reader, frame, TRUE)) {
					return FALSE;
				}
			}

			continue;
		case FRAME_NEXT_CHILD: {
			gboolean terminated;

			if (!skip_boundary (reader, &terminated))
				return FALSE;

			/* A complete marker introduces a child even at EOF. Only an
			 * unterminated marker is trailing boundary data; read-ahead
			 * discovering EOF must not change the resulting MIME tree. */
			if (!terminated) {
				priv->boundary_eos = TRUE;
				frame->state = FRAME_END;
			} else if (!read_child_headers (reader, frame, FALSE)) {
				return FALSE;
			}

			continue;
		}
		case FRAME_END:
			if (frame->boundary_active) {
				if (priv->boundary == frame && priv->final_boundary) {
					if (!skip_boundary (reader, NULL))
						return FALSE;

					frame->boundary_active = FALSE;

					if (!scan_content (reader, GMIME_READER_CONTENT_EPILOGUE))
						return FALSE;
				} else {
					if (priv->boundary || priv->mailbox_boundary)
						_g_mime_parser_options_warn (priv->options, frame->type_offset,
									    GMIME_WARN_MALFORMED_MULTIPART,
									    g_mime_content_type_get_media_subtype (frame->content_type));
					else if (priv->boundary_eos)
						_g_mime_parser_options_warn (priv->options, -1, GMIME_WARN_TRUNCATED_MESSAGE, NULL);
					report_at (reader, GMIME_COMPLIANCE_MISSING_MULTIPART_BOUNDARY,
						   g_mime_reader_tell (reader), priv->line_number,
						   (guint64) (g_mime_reader_tell (reader) - priv->line_begin) + 1,
						   GMIME_COMPLIANCE_POSITION_EXACT);
				}

				frame->boundary_active = FALSE;
			}
			break;
		default:
			g_assert_not_reached ();
		}

		end = entity_end_offset (reader, frame->begin);
		lines = content_lines (reader, frame->content_begin, frame->content_line,
				       entity_end_offset (reader, frame->content_begin));

		if (!READER_CALLBACK (reader, priv, entity_end, frame->type, frame->content_type, frame->begin,
				      frame->begin_line, MIN (frame->headers_end, end), end, lines))
			return FALSE;

		if (message && priv->frames->len == 1 &&
		    !READER_CALLBACK (reader, priv, message_end, frame->begin, frame->begin_line,
				      MIN (frame->headers_end, end), end, lines))
			return FALSE;

		g_ptr_array_remove_index (priv->frames, priv->frames->len - 1);

		if (priv->frames->len) {
			frame = g_ptr_array_index (priv->frames, priv->frames->len - 1);

			if (frame->message_active) {
				if (!READER_CALLBACK (reader, priv, message_end, frame->message_begin, frame->message_line,
						      MIN (frame->message_headers_end, end), end, lines))
					return FALSE;

				frame->message_active = FALSE;
			}

			frame->state = priv->boundary == frame && !priv->final_boundary ? FRAME_NEXT_CHILD : FRAME_END;
		}
	}

	return TRUE;
}

static gboolean
step_mailbox_marker (GMimeReader *reader)
{
	GMimeReaderPrivate *priv = _g_mime_reader_get_private (reader);
	const char *prefix = priv->format == GMIME_FORMAT_MBOX ? "From " : "\1\1\1\1";
	gsize prefix_length = strlen (prefix);
	gboolean midline = FALSE;
	gint64 begin;
	guint64 line;

	/* Search only at physical line starts. Discard a long non-marker line
	 * incrementally, retaining no unbounded mailbox pretext. */
	for (;;) {
		gsize left = read_ahead_at_least (reader, prefix_length);
		const guint8 *start = priv->input + priv->input_index, *lf;

		if (priv->error)
			return FALSE;

		if (left < prefix_length)
			break;

		if (!midline && memcmp (start, prefix, prefix_length) == 0)
			goto found;

		lf = memchr (start, '\n', left);
		consume_input (reader, lf ? (gsize) (lf - start + 1) : left);
		midline = lf == NULL;
	}

	if (!priv->error)
		g_set_error_literal (&priv->error, GMIME_ERROR, GMIME_ERROR_PARSE_ERROR,
				     "Failed to find a mailbox separator.");

	return FALSE;

found:
	begin = g_mime_reader_tell (reader);
	line = priv->line_number;

	if (!READER_CALLBACK (reader, priv, marker_begin, begin, line))
		return FALSE;

	for (;;) {
		gsize left = read_ahead (reader, READ_AHEAD_SIZE), count;
		const guint8 *start = priv->input + priv->input_index, *lf;

		if (priv->error)
			return FALSE;

		if (!left)
			break;

		lf = memchr (start, '\n', left);
		count = lf ? (gsize) (lf - start) : left;

		if (count && start[count - 1] == '\r')
			count--;

		if (!READER_CALLBACK (reader, priv, marker_read, start, count))
			return FALSE;

		if (lf) {
			consume_input (reader, lf - start + 1);
			priv->mailbox_boundary = FALSE;
			return READER_CALLBACK (reader, priv, marker_end, begin, line, g_mime_reader_tell (reader));
		}

		/* Leave a split CRLF intact, but consume a final CR at actual EOF
		 * so a truncated separator cannot stall the next read. */
		consume_input (reader, priv->input_eos ? left : count);
	}

	g_set_error_literal (&priv->error, GMIME_ERROR, GMIME_ERROR_PARSE_ERROR,
			     "Incomplete mailbox separator.");

	return FALSE;
}

static gboolean
step_byte_order_mark (GMimeReader *reader)
{
	static const guint8 bom[] = { 0xef, 0xbb, 0xbf };
	GMimeReaderPrivate *priv = _g_mime_reader_get_private (reader);
	gsize index = 0;

	/* Only the initialized message path strips a UTF-8 BOM. Consume a
	 * partial prefix across short reads, but reject a mismatching prefix
	 * rather than treating its remaining bytes as an ordinary header. */
	do {
		if (!read_ahead (reader, READ_AHEAD_SIZE))
			break;

		while (priv->input_index < priv->input_end && index < sizeof (bom) &&
		       priv->input[priv->input_index] == bom[index]) {
			consume_input (reader, 1);
			index++;
		}

		if (index == 0 || index == sizeof (bom))
			return TRUE;
	} while (priv->input_index == priv->input_end && !priv->error);

	if (!priv->error)
		g_set_error_literal (&priv->error, GMIME_ERROR, GMIME_ERROR_PARSE_ERROR,
				     "End of MIME input or incomplete UTF-8 byte-order mark.");

	return FALSE;
}

static gboolean
read_entity_or_message (GMimeReader *reader, gboolean message, GError **error)
{
	GMimeReaderPrivate *priv;
	gboolean result = FALSE;

	g_return_val_if_fail (GMIME_IS_READER (reader), FALSE);
	priv = _g_mime_reader_get_private (reader);

	g_return_val_if_fail (GMIME_IS_STREAM (priv->stream), FALSE);
	g_return_val_if_fail (!priv->reading, FALSE);
	g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

	g_object_ref (reader);
	priv->reading = TRUE;
	begin_compliance (reader);

	if (!priv->error) {
		gboolean initial = priv->initial;

		priv->initial = FALSE;

		if (message && !initial && priv->input_eos && priv->input_index == priv->input_end) {
			g_set_error_literal (&priv->error, GMIME_ERROR, GMIME_ERROR_PARSE_ERROR, "End of MIME input.");
		} else if (!message || !initial || step_byte_order_mark (reader)) {
			if (!message || priv->format == GMIME_FORMAT_MESSAGE || step_mailbox_marker (reader))
				result = step_entity (reader, message);

			if (result && message && priv->format == GMIME_FORMAT_MMDF && priv->mailbox_boundary) {
				/* MMDF has a closing separator as well as an opening one.
				 * Consume the close now; the next operation seeks its own
				 * opening separator, never a synthetic empty message. */
				result = step_mailbox_marker (reader);

				if (result) {
					read_ahead (reader, 1);
					result = priv->error == NULL;
				}
			}

			if (result)
				priv->complete_eos = !message || (priv->input_eos && priv->input_index == priv->input_end);
		}
	}

	if (priv->error)
		g_propagate_error (error, g_error_copy (priv->error));

	priv->boundary = NULL;
	g_clear_pointer (&priv->frames, g_ptr_array_unref);
	g_clear_object (&priv->content_type);
	priv->entity_read = FALSE;
	end_compliance (reader);
	priv->reading = FALSE;
	g_object_unref (reader);

	return result;
}

/**
 * g_mime_reader_read_entity:
 * @reader: a #GMimeReader with an input stream
 * @error: return location for a #GError, or %NULL
 *
 * Reads one MIME entity through end of input, without constructing a MIME
 * object tree. Delivers headers before entity_begin, then leaf content or
 * multipart/embedded-message descendants, and finally entity_end.
 * Nested entities use an explicit heap stack. At the configured maximum MIME
 * depth containers become opaque leaves. Encoded message parts are also
 * opaque; content callbacks always receive original, undecoded bytes.
 *
 * Multipart delimiters are recognized only at line starts, including ancestor
 * delimiters that terminate truncated descendants. Preambles retain their
 * trailing newline; leaf content and epilogues omit the newline immediately
 * preceding a delimiter. Closing delimiters leave their newline in the
 * epilogue. Offsets and line counts describe these wire ranges, not decoded
 * data. Missing closing delimiters are tolerated as in MimeKit.
 *
 * There is no initial mailbox marker search or BOM stripping. In mailbox
 * formats a subsequent separator terminates the entity. Compliance checks
 * run only with a logger; inline OpenPGP detection and model construction
 * are not performed.
 * Empty input fails. Callback and I/O failure semantics, ownership and
 * restrictions are the same as g_mime_reader_read_headers().
 *
 * Returns: %TRUE on success, or %FALSE on failure.
 **/
gboolean
g_mime_reader_read_entity (GMimeReader *reader, GError **error)
{
	return read_entity_or_message (reader, FALSE, error);
}

/**
 * g_mime_reader_read_message:
 * @reader: a #GMimeReader with an input stream
 * @error: return location for a #GError, or %NULL
 *
 * Reads a top-level message through end of input or the next mailbox marker. Message headers are
 * delivered once, followed by message_begin, the root entity and its
 * descendants, and message_end. Embedded messages use the same callbacks.
 *
 * At the start of a newly assigned stream, an optional UTF-8 byte-order
 * mark is consumed without a callback. Offsets still include those bytes
 * and line numbers still start at 1. A truncated or mismatching BOM prefix,
 * empty input, or a subsequent message read at EOF fails with
 * %GMIME_ERROR_PARSE_ERROR. Header-only and entity reads do not strip a BOM.
 * Earlier operations on the same stream disable this initial BOM probe.
 *
 * The default format treats body From-lines as ordinary content. Mbox and
 * MMDF formats search for an opening marker and report its raw bytes through
 * marker callbacks. Mbox leaves the next marker buffered for another call;
 * MMDF consumes its closing separator. Content-Length can suppress premature
 * mbox markers when respect_content_length is enabled. Compliance budgets
 * reset for each message. Error propagation, stream ownership and callback
 * restrictions are the same as g_mime_reader_read_entity().
 *
 * Returns: %TRUE on success, or %FALSE on failure.
 **/
gboolean
g_mime_reader_read_message (GMimeReader *reader, GError **error)
{
	return read_entity_or_message (reader, TRUE, error);
}

GMimeStream *
_g_mime_reader_get_stream (GMimeReader *reader)
{
	GMimeReaderPrivate *priv;

	g_return_val_if_fail (GMIME_IS_READER (reader), NULL);

	priv = _g_mime_reader_get_private (reader);

	return priv->stream;
}

/* GMimeParser's format setter must retain unread bytes, not reassign the
 * stream at its physical read-ahead position. */
void
_g_mime_reader_set_format (GMimeReader *reader, GMimeFormat format)
{
	GMimeReaderPrivate *priv;

	g_return_if_fail (GMIME_IS_READER (reader));
	g_return_if_fail (format >= GMIME_FORMAT_MESSAGE && format <= GMIME_FORMAT_MMDF);
	priv = _g_mime_reader_get_private (reader);

	g_return_if_fail (!priv->reading);

	priv->format = format;
}

/* Public model construction returns NULL without an error at EOF, unlike
 * MimeReader's lower-level traversal API. Do not consume a BOM or a byte. */
gboolean
_g_mime_reader_has_input (GMimeReader *reader, GError **error)
{
	GMimeReaderPrivate *priv = _g_mime_reader_get_private (reader);
	gboolean more;

	g_return_val_if_fail (!priv->reading, FALSE);
	g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

	more = !priv->error && read_ahead (reader, 1) > 0;
	if (priv->error)
		g_propagate_error (error, g_error_copy (priv->error));
	else if (!more)
		priv->complete_eos = TRUE;

	return more;
}

gboolean
_g_mime_reader_header_limit_exceeded (GMimeReader *reader)
{
	GMimeReaderPrivate *priv = _g_mime_reader_get_private (reader);

	return priv->header_limit_exceeded;
}

gboolean
_g_mime_reader_input_eos (GMimeReader *reader)
{
	GMimeReaderPrivate *priv = _g_mime_reader_get_private (reader);

	return priv->input_index == priv->input_end &&
		(priv->input_eos || g_mime_stream_eos (priv->stream));
}

/* Reading a persistent part can move the underlying stream. Restore the
 * physical read-ahead position, not tell(): the unread bytes are still
 * buffered and seeking to the logical position would read them twice. */
gboolean
_g_mime_reader_restore_stream_position (GMimeReader *reader, GError **error)
{
	GMimeReaderPrivate *priv;
	gint64 position;
	int err;

	g_return_val_if_fail (GMIME_IS_READER (reader), FALSE);
	priv = _g_mime_reader_get_private (reader);

	g_return_val_if_fail (GMIME_IS_STREAM (priv->stream), FALSE);
	g_return_val_if_fail (!priv->reading, FALSE);
	g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

	if (!priv->error) {
		errno = 0;
		position = g_mime_stream_tell (priv->stream);

		if (position < 0 || (position != priv->position &&
		    g_mime_stream_seek (priv->stream, priv->position, GMIME_STREAM_SEEK_SET) != priv->position)) {
			err = errno;
			g_set_error (&priv->error, GMIME_ERROR, err ? err : GMIME_ERROR_GENERAL,
				     "Failed to restore MIME input position: %s",
				     err ? g_strerror (err) : "stream seek failed");
		}
	}

	if (priv->error) {
		g_propagate_error (error, g_error_copy (priv->error));
		return FALSE;
	}

	return TRUE;
}

/* MimeParser.ParseStatusGroup() skips blank lines between status records.
 * A lone CR is not a blank line: leave it for the following header read. */
gboolean
_g_mime_reader_skip_blank_lines (GMimeReader *reader, GError **error)
{
	GMimeReaderPrivate *priv;
	gboolean more = FALSE;

	g_return_val_if_fail (GMIME_IS_READER (reader), FALSE);

	priv = _g_mime_reader_get_private (reader);

	g_return_val_if_fail (GMIME_IS_STREAM (priv->stream), FALSE);
	g_return_val_if_fail (!priv->reading, FALSE);
	g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

	g_object_ref (reader);
	priv->reading = TRUE;

	if (!priv->error)
		read_ahead (reader, READ_AHEAD_SIZE);

	while (!priv->error) {
		gsize left = read_ahead_at_least (reader, 2);
		const guint8 *start = priv->input + priv->input_index;

		if (!left || priv->error)
			break;

		if (*start == '\n') {
			consume_input (reader, 1);
		} else if (left > 1 && start[0] == '\r' && start[1] == '\n') {
			consume_input (reader, 2);
		} else {
			more = TRUE;
			break;
		}
	}

	if (priv->error)
		g_propagate_error (error, g_error_copy (priv->error));
	else
		priv->complete_eos = !more;

	priv->reading = FALSE;
	g_object_unref (reader);

	return more;
}

/* Preserve already buffered bytes even for nonseekable input. The status
 * parser needs this Office365 recovery path as a rewound block stream,
 * not a seek on the original input or a contiguous whole-message buffer. */
GMimeStream *
_g_mime_reader_read_to_eos (GMimeReader *reader, GError **error)
{
	GMimeReaderPrivate *priv;
	GMimeStream *content;

	g_return_val_if_fail (GMIME_IS_READER (reader), NULL);

	priv = _g_mime_reader_get_private (reader);

	g_return_val_if_fail (GMIME_IS_STREAM (priv->stream), NULL);
	g_return_val_if_fail (!priv->reading, NULL);
	g_return_val_if_fail (error == NULL || *error == NULL, NULL);

	g_object_ref (reader);
	priv->reading = TRUE;
	content = g_mime_stream_block_new ();

	while (!priv->error) {
		gsize left = read_ahead (reader, 1);
		int err;

		if (!left || priv->error)
			break;

		errno = 0;
		if (_g_mime_stream_write_all (content, (const char *) priv->input + priv->input_index, left) < 0) {
			err = errno;
			g_set_error (&priv->error, GMIME_ERROR, err ? err : GMIME_ERROR_GENERAL,
				     "Failed to preserve MIME input: %s", err ? g_strerror (err) : "stream write failed");
			break;
		}

		/* Like ReadToEos(), this recovery path does not parse lines. */
		priv->input_index = priv->input_end;
	}

	if (!priv->error && g_mime_stream_reset (content) < 0) {
		int err = errno;

		g_set_error (&priv->error, GMIME_ERROR, err ? err : GMIME_ERROR_GENERAL,
			     "Failed to rewind preserved MIME input: %s", err ? g_strerror (err) : "stream reset failed");
	}

	if (priv->error) {
		g_propagate_error (error, g_error_copy (priv->error));
		g_clear_object (&content);
	} else {
		priv->complete_eos = TRUE;
	}

	priv->reading = FALSE;
	g_object_unref (reader);

	return content;
}
