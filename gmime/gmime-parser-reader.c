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

#include "gmime-parser-reader-private.h"
#include "gmime-reader-private.h"
#include "gmime-internal.h"
#include "gmime-error.h"
#include "gmime-part.h"
#include "gmime-text-part.h"
#include "gmime-message-part.h"
#include "gmime-message-partial.h"
#include "gmime-multipart.h"
#include "gmime-text-rfc822-headers.h"
#include "gmime-stream-block.h"
#include "gmime-filter-openpgp.h"
#include "gmime-utils.h"

typedef struct {
	GMimeReader parent_object;
	GMimeStream *stream;
	GMimeHeaderList *headers;
	GPtrArray *stack;
	GMimeStream *content;
	GByteArray *boundary;
	GByteArray *end_boundary;
	GByteArray *pending_marker;
	GBytes *marker;
	gint64 marker_offset;
	gint64 headers_begin;
	gint64 headers_end;
	gboolean (*header_callback) (GMimeHeader *, gboolean, gpointer, GError **);
	gpointer header_data;
	GMimeFilter *openpgp;
	gboolean persist;
	gboolean persistent;
	gboolean has_body_separator;
} GMimeParserReader;

typedef GMimeReaderClass GMimeParserReaderClass;

G_DEFINE_TYPE (GMimeParserReader, g_mime_parser_reader, GMIME_TYPE_READER)

static void
parser_reader_reset (GMimeParserReader *reader)
{
	g_clear_object (&reader->content);
	g_ptr_array_set_size (reader->stack, 0);
	g_clear_object (&reader->headers);
	g_clear_pointer (&reader->boundary, g_byte_array_unref);
	g_clear_pointer (&reader->end_boundary, g_byte_array_unref);
	g_clear_pointer (&reader->pending_marker, g_byte_array_unref);
	g_clear_object (&reader->openpgp);
}

static void
parser_reader_finalize (GObject *object)
{
	GMimeParserReader *reader = (GMimeParserReader *) object;

	parser_reader_reset (reader);
	g_ptr_array_unref (reader->stack);
	g_clear_object (&reader->stream);
	g_clear_pointer (&reader->marker, g_bytes_unref);
	G_OBJECT_CLASS (g_mime_parser_reader_parent_class)->finalize (object);
}

static gboolean
parser_reader_headers_begin (GMimeReader *base, gint64 offset, guint64 line, GError **error)
{
	GMimeParserReader *reader = (GMimeParserReader *) base;

	g_clear_object (&reader->headers);
	reader->headers = g_mime_header_list_new (g_mime_reader_get_options (base));
	reader->headers->has_body_separator = FALSE;
	reader->has_body_separator = FALSE;
	if (!reader->stack->len)
		reader->headers_begin = offset;

	return TRUE;
}

static gboolean
parser_reader_header_read (GMimeReader *base, const guint8 *buffer, gsize length,
			   gsize name_length, gsize field_length, gboolean invalid,
			   gint64 offset, guint64 line, GError **error)
{
	GMimeParserReader *reader = (GMimeParserReader *) base;
	GMimeHeader *header;
	int index;

	header = _g_mime_header_new_from_reader (g_mime_reader_get_options (base), buffer, length,
					       name_length, field_length, invalid, offset);
	index = g_mime_header_list_add (reader->headers, header);

	if (index < 0) {
		g_object_unref (header);
		g_set_error_literal (error, GMIME_ERROR, GMIME_ERROR_GENERAL, "Failed to add parsed MIME header.");
		return FALSE;
	}

	if (reader->header_callback &&
	    !reader->header_callback (header, reader->stack->len == 0, reader->header_data, error)) {
		g_object_unref (header);
		return FALSE;
	}

	if (!invalid) {
		if (g_mime_utils_text_is_8bit (buffer, length))
			_g_mime_parser_options_warn (g_mime_reader_get_options (base), offset,
						    GMIME_WARN_UNENCODED_8BIT_HEADER, header->name);
	}

	g_object_unref (header);

	return TRUE;
}

static gboolean
parser_reader_body_separator (GMimeReader *base, gint64 offset, guint64 line, gint64 end, GError **error)
{
	GMimeParserReader *reader = (GMimeParserReader *) base;

	reader->has_body_separator = TRUE;
	if (!reader->stack->len)
		reader->headers_end = offset;

	return TRUE;
}

static gboolean
parser_reader_headers_end (GMimeReader *base, gint64 begin, guint64 begin_line,
			   gint64 end, guint64 end_line, GError **error)
{
	GMimeParserReader *reader = (GMimeParserReader *) base;

	reader->headers->has_body_separator = reader->has_body_separator;
	return TRUE;
}

static void
parser_reader_check_repeated_header (GMimeParserOptions *options, GMimeEntity *entity, GMimeHeader *header)
{
	static const char *singletons[] = {
		"bcc", "cc", "date", "from", "in-reply-to", "message-id",
		"references", "reply-to", "sender", "subject", "to"
	};
	GMimeHeader *existing;
	guint i;

	if (header->invalid || !g_mime_parser_options_get_warning_callback (options))
		return;

	if (GMIME_IS_MESSAGE (entity)) {
		for (i = 0; i < G_N_ELEMENTS (singletons); i++) {
			if (!g_ascii_strcasecmp (header->name, singletons[i]))
				break;
		}
		if (i == G_N_ELEMENTS (singletons))
			return;
	}

	if ((existing = g_mime_header_list_get_header (entity->headers, header->name)))
		_g_mime_parser_options_warn (options, header->offset,
					    g_bytes_equal (existing->raw_value, header->raw_value) ?
					    GMIME_WARN_DUPLICATED_HEADER : GMIME_CRIT_CONFLICTING_HEADER,
					    header->name);
}

static void
parser_reader_copy_headers (GMimeParserReader *reader, GMimeEntity *entity, gboolean toplevel)
{
	gboolean message = GMIME_IS_MESSAGE (entity);
	int i, count;

	entity->ensure_newline = FALSE;
	_g_mime_header_list_set_options (entity->headers, g_mime_reader_get_options ((GMimeReader *) reader));
	g_mime_header_list_clear (entity->headers);

	/* MimeKit attaches Headers.Changed after copying the parsed headers.
	 * Do likewise: later duplicates must not replace the first parsed value.
	 * Message headers exclude Content-*; a message's root entity keeps only
	 * Content-* headers. Non-toplevel entities keep every header. */
	_g_mime_entity_block_header_list_changed (entity);
	count = g_mime_header_list_get_count (reader->headers);

	for (i = 0; i < count; i++) {
		GMimeHeader *header = g_mime_header_list_get_header_at (reader->headers, i);
		gboolean content = !g_ascii_strncasecmp (header->name, "Content-", 8);

		if ((message && content) || (!message && toplevel && !content))
			continue;

		parser_reader_check_repeated_header (entity->headers->options, entity, header);
		g_mime_header_list_add (entity->headers, header);
		if (header->owns_options)
			g_mime_parser_options_free (header->options);
		header->options = entity->headers->options;
		header->owns_options = FALSE;
	}

	_g_mime_entity_unblock_header_list_changed (entity);

	if (!message)
		entity->headers->has_body_separator = reader->has_body_separator;

	/* Initialize GMime's eager caches once per field. Address callbacks
	 * see the complete list, including repeated address headers, just as
	 * MimeKit's lazy properties do. Invalid fields are wire data only. */
	count = g_mime_header_list_get_count (entity->headers);

	for (i = 0; i < count; i++) {
		GMimeHeader *header = g_mime_header_list_get_header_at (entity->headers, i);

		if (!header->invalid && g_ascii_strcasecmp (header->name, "Content-Type") &&
		    g_mime_header_list_get_header (entity->headers, header->name) == header)
			GMIME_ENTITY_GET_CLASS (entity)->header_added (entity, header);
	}
}

static GMimeEntity *
parser_reader_peek (GMimeParserReader *reader)
{
	return reader->stack->len ? g_ptr_array_index (reader->stack, reader->stack->len - 1) : NULL;
}

static gboolean
parser_reader_entity_begin (GMimeReader *base, GMimeReaderEntityType type, GMimeContentType *content_type,
			    gint64 offset, guint64 line, GError **error)
{
	GMimeParserReader *reader = (GMimeParserReader *) base;
	GMimeParserOptions *options = g_mime_reader_get_options (base);
	GMimeContentType *copy;
	GMimeEntity *entity;
	GMimeEntity *parent = parser_reader_peek (reader);
	const char *media_type, *subtype;
	gboolean opaque = FALSE;

	media_type = g_mime_content_type_get_media_type (content_type);
	subtype = g_mime_content_type_get_media_subtype (content_type);
	entity = g_mime_entity_new_type (options, media_type, subtype);

	if (entity && G_OBJECT_TYPE (entity) == GMIME_TYPE_MESSAGE_PARTIAL) {
		GMimeHeader *header = g_mime_header_list_get_header (reader->headers, "Content-Transfer-Encoding");
		GMimeContentEncoding encoding = header ?
			_g_mime_header_get_content_encoding (header) :
			GMIME_CONTENT_ENCODING_UNSPECIFIED;

		opaque = encoding == GMIME_CONTENT_ENCODING_BASE64 ||
			encoding == GMIME_CONTENT_ENCODING_QUOTEDPRINTABLE ||
			encoding == GMIME_CONTENT_ENCODING_UUENCODE;
	}

	if (entity && type == GMIME_READER_ENTITY_PART && (!GMIME_IS_PART (entity) || opaque)) {
		if (!opaque && !((G_OBJECT_TYPE (entity) == GMIME_TYPE_MESSAGE_PART &&
				  (g_mime_content_type_is_type (content_type, "message", "rfc822") ||
				   g_mime_content_type_is_type (content_type, "message", "rfc2822") ||
				   g_mime_content_type_is_type (content_type, "message", "global") ||
				   g_mime_content_type_is_type (content_type, "message", "news") ||
				   g_mime_content_type_is_type (content_type, "message", "external-body"))) ||
				 (G_OBJECT_TYPE (entity) == GMIME_TYPE_TEXT_RFC822_HEADERS &&
				  (g_mime_content_type_is_type (content_type, "text", "rfc822-headers") ||
				   g_mime_content_type_is_type (content_type, "message", "global-headers"))) ||
				 (GMIME_IS_MULTIPART (entity) &&
				  g_mime_content_type_is_type (content_type, "multipart", "*")))) {
			g_set_error (error, GMIME_ERROR, GMIME_ERROR_NOT_SUPPORTED,
				     "Reader-backed construction of %s as a leaf is not supported.",
				     G_OBJECT_TYPE_NAME (entity));
			g_object_unref (entity);
			return FALSE;
		}

		g_object_unref (entity);

		/* RFC 2046 forbids encoding message/rfc822 and message/partial, but
		 * broken clients do it anyway. The reader selects an opaque leaf
		 * for encoded messages and header parts rather than parsing
		 * encoded bytes as MIME structure. The same opaque fallback
		 * applies when the reader reaches the MIME nesting limit. */
		entity = g_object_new (!g_ascii_strcasecmp (media_type, "text") ?
				       GMIME_TYPE_TEXT_PART : GMIME_TYPE_PART, NULL);
		_g_mime_header_list_set_options (entity->headers, options);
	}

	if (!entity) {
		g_set_error_literal (error, GMIME_ERROR, GMIME_ERROR_NOT_SUPPORTED,
				     "No MIME entity type is registered for the parsed content type.");
		return FALSE;
	}

	if (type == GMIME_READER_ENTITY_MESSAGE_PART && !GMIME_IS_MESSAGE_PART (entity)) {
		g_set_error (error, GMIME_ERROR, GMIME_ERROR_NOT_SUPPORTED,
			     "Registered %s cannot hold a parsed embedded message.", G_OBJECT_TYPE_NAME (entity));
		g_object_unref (entity);
		return FALSE;
	}

	if (type == GMIME_READER_ENTITY_MULTIPART && !GMIME_IS_MULTIPART (entity)) {
		g_set_error (error, GMIME_ERROR, GMIME_ERROR_NOT_SUPPORTED,
			     "Registered %s cannot hold parsed multipart children.", G_OBJECT_TYPE_NAME (entity));
		g_object_unref (entity);
		return FALSE;
	}

	parser_reader_copy_headers (reader, entity, parent && GMIME_IS_MESSAGE (parent));
	copy = g_mime_content_type_clone (content_type);
	_g_mime_entity_set_content_type (entity, copy);
	g_object_unref (copy);

	if (type == GMIME_READER_ENTITY_MULTIPART)
		_g_mime_multipart_initialize_from_reader ((GMimeMultipart *) entity);

	if (type == GMIME_READER_ENTITY_MULTIPART && !g_mime_content_type_get_boundary (content_type))
		_g_mime_parser_options_warn (options, offset, GMIME_CRIT_MULTIPART_WITHOUT_BOUNDARY, subtype);

	if (parent && GMIME_IS_MULTIPART (parent)) {
		GBytes *boundary = reader->boundary ?
			g_byte_array_free_to_bytes (g_steal_pointer (&reader->boundary)) : NULL;

		_g_mime_multipart_add_with_boundary ((GMimeMultipart *) parent, entity, boundary);
		g_clear_pointer (&boundary, g_bytes_unref);
	} else if (parent) {
		/* Parsed bodies retain their offsets and do not synthesize a
		 * MIME-Version header, unlike the public body assignment helper. */
		GMIME_MESSAGE (parent)->mime_part = g_object_ref (entity);
	}

	g_ptr_array_add (reader->stack, entity);

	return TRUE;
}

static gboolean
parser_reader_entity_end (GMimeReader *base, GMimeReaderEntityType type, GMimeContentType *content_type,
			  gint64 begin, guint64 begin_line, gint64 headers_end,
			  gint64 end, guint64 lines, GError **error)
{
	GMimeParserReader *reader = (GMimeParserReader *) base;

	if (type == GMIME_READER_ENTITY_MULTIPART) {
		GMimeMultipart *multipart = (GMimeMultipart *) parser_reader_peek (reader);

		/* Preserve an opening marker at EOF that was not followed by a
		 * child, but only if no closing delimiter was encountered. */
		if (!multipart->write_end_boundary && reader->boundary)
			multipart->raw_trailing_boundary = g_byte_array_free_to_bytes (
				g_steal_pointer (&reader->boundary));
		g_clear_pointer (&reader->boundary, g_byte_array_unref);
	}

	if (reader->stack->len > 1)
		g_ptr_array_remove_index (reader->stack, reader->stack->len - 1);

	return TRUE;
}

static gboolean
parser_reader_message_begin (GMimeReader *base, gint64 begin, guint64 line, GError **error)
{
	GMimeParserReader *reader = (GMimeParserReader *) base;
	GMimeEntity *parent = parser_reader_peek (reader);
	GMimeMessage *message = g_mime_message_new (FALSE);
	GByteArray *marker = NULL;

	/* MimeKit keeps leading invalid message headers as pre-header marker
	 * bytes, not headers. Embedded messages write these bytes unchanged,
	 * including NULs and their original newlines. */
	while (g_mime_header_list_get_count (reader->headers)) {
		GMimeHeader *header = g_mime_header_list_get_header_at (reader->headers, 0);
		gconstpointer data;
		gsize length;

		if (!header->invalid)
			break;

		if (!marker)
			marker = g_byte_array_new ();

		data = g_bytes_get_data (header->raw_name, &length);
		g_byte_array_append (marker, data, length);
		g_mime_header_list_remove_at (reader->headers, 0);
	}

	if (marker)
		message->raw_marker = g_byte_array_free_to_bytes (marker);

	parser_reader_copy_headers (reader, (GMimeEntity *) message, FALSE);

	if (parent)
		g_mime_message_part_set_message (GMIME_MESSAGE_PART (parent), message);

	g_ptr_array_add (reader->stack, message);

	return TRUE;
}

static gboolean
parser_reader_message_end (GMimeReader *base, gint64 begin, guint64 begin_line,
			   gint64 headers_end, gint64 end, guint64 lines, GError **error)
{
	GMimeParserReader *reader = (GMimeParserReader *) base;

	if (reader->stack->len > 1)
		g_ptr_array_remove_index (reader->stack, reader->stack->len - 1);

	return TRUE;
}

static gboolean
parser_reader_marker_begin (GMimeReader *base, gint64 begin, guint64 line, GError **error)
{
	GMimeParserReader *reader = (GMimeParserReader *) base;

	/* MimeKit keeps the mailbox envelope on the parser, separate from a
	 * message's invalid pre-header bytes. MMDF also reports its closing
	 * separator after message_end; retain the opening envelope instead. */
	if (!reader->stack->len) {
		reader->marker_offset = begin;
		reader->pending_marker = g_byte_array_new ();
	}

	return TRUE;
}

static gboolean
parser_reader_marker_read (GMimeReader *base, const guint8 *buffer, gsize length, GError **error)
{
	GMimeParserReader *reader = (GMimeParserReader *) base;

	if (!reader->stack->len)
		g_byte_array_append (reader->pending_marker, buffer, length);

	return TRUE;
}

static gboolean
parser_reader_boundary_begin (GMimeReader *base, gboolean final, gint64 begin, guint64 line, GError **error)
{
	GMimeParserReader *reader = (GMimeParserReader *) base;

	if (final) {
		g_clear_pointer (&reader->end_boundary, g_byte_array_unref);
		reader->end_boundary = g_byte_array_new ();
	}

	return TRUE;
}

static gboolean
parser_reader_boundary_read (GMimeReader *base, gboolean final, const guint8 *buffer, gsize length,
			     gint64 begin, guint64 line, GError **error)
{
	GMimeParserReader *reader = (GMimeParserReader *) base;

	if (final) {
		g_byte_array_append (reader->end_boundary, buffer, length);
	} else {
		/* A nonempty buffer means a double (or triple, etc.) boundary.
		 * Append each marker so the next child retains the whole run. */
		if (!reader->boundary)
			reader->boundary = g_byte_array_new ();
		g_byte_array_append (reader->boundary, buffer, length);
	}

	return TRUE;
}

static gboolean
parser_reader_boundary_end (GMimeReader *base, gboolean final, gint64 begin, guint64 line,
			    gint64 end, GError **error)
{
	GMimeParserReader *reader = (GMimeParserReader *) base;

	if (final) {
		GMimeMultipart *multipart = (GMimeMultipart *) parser_reader_peek (reader);

		g_clear_pointer (&multipart->raw_end_boundary, g_bytes_unref);
		multipart->raw_end_boundary = g_byte_array_free_to_bytes (g_steal_pointer (&reader->end_boundary));
		multipart->write_end_boundary = TRUE;
	}

	return TRUE;
}

static gboolean
parser_reader_content_begin (GMimeReader *base, GMimeReaderContentType type,
			     gint64 offset, guint64 line, GError **error)
{
	GMimeParserReader *reader = (GMimeParserReader *) base;

	reader->content = reader->persistent && type == GMIME_READER_CONTENT_PART ? NULL : g_mime_stream_block_new ();

	if (type == GMIME_READER_CONTENT_PART &&
	    g_mime_parser_options_get_detect_openpgp (g_mime_reader_get_options (base)))
		reader->openpgp = g_mime_filter_openpgp_new ();

	return TRUE;
}

static gboolean
parser_reader_io_error (GError **error, const char *operation)
{
	int err = errno;

	g_set_error (error, GMIME_ERROR, err ? err : GMIME_ERROR_GENERAL,
		     "Failed to %s MIME content: %s", operation, err ? g_strerror (err) : "stream operation failed");

	return FALSE;
}

static gboolean
parser_reader_content_read (GMimeReader *base, GMimeReaderContentType type,
			    const guint8 *buffer, gsize length, GError **error)
{
	GMimeParserReader *reader = (GMimeParserReader *) base;
	char *output;
	size_t output_length, prespace;

	if (reader->openpgp)
		g_mime_filter_filter (reader->openpgp, (char *) buffer, length, 0,
				     &output, &output_length, &prespace);

	errno = 0;
	if (reader->content && _g_mime_stream_write_all (reader->content, (const char *) buffer, length) < 0)
		return parser_reader_io_error (error, "buffer");

	return TRUE;
}

static void
parser_reader_finish_openpgp (GMimeParserReader *reader, GMimePart *part)
{
	GMimeContentEncoding encoding = g_mime_part_get_content_encoding (part);
	size_t length, prespace;
	GMimeOpenPGPData type;
	char *output;

	/* Detection remains opt-in and examines raw wire bytes. Encoded parts
	 * without recognizable armor keep their lazy decoded-content cache. */
	if (!reader->openpgp)
		return;

	g_mime_filter_complete (reader->openpgp, "\n", 1, 0, &output, &length, &prespace);
	type = g_mime_filter_openpgp_get_data_type ((GMimeFilterOpenPGP *) reader->openpgp);

	if (type != GMIME_OPENPGP_DATA_NONE ||
	    (encoding != GMIME_CONTENT_ENCODING_BASE64 && encoding != GMIME_CONTENT_ENCODING_UUENCODE))
		g_mime_part_set_openpgp_data (part, type);

	g_clear_object (&reader->openpgp);
}

static gboolean
parser_reader_content_end (GMimeReader *base, GMimeReaderContentType type,
			   gint64 begin, guint64 begin_line, gint64 end, guint64 lines,
			   GMimeReaderNewLineFormat newline, GError **error)
{
	GMimeParserReader *reader = (GMimeParserReader *) base;
	GMimeContent *content;
	GMimePart *part;

	if (type != GMIME_READER_CONTENT_PART) {
		GByteArray *buffer;
		GBytes *bytes;

		/* The reader only delivers [begin, end); retain the inexpensive
		 * length safeguard for multipart preambles and epilogues too. */
		errno = 0;
		if (g_mime_stream_block_set_length ((GMimeStreamBlock *) reader->content, end - begin) < 0)
			return parser_reader_io_error (error, "size multipart");

		buffer = g_mime_stream_block_to_byte_array ((GMimeStreamBlock *) reader->content, error);
		if (!buffer)
			return FALSE;

		bytes = g_byte_array_free_to_bytes (buffer);
		_g_mime_multipart_set_raw_text ((GMimeMultipart *) parser_reader_peek (reader), bytes,
					       type == GMIME_READER_CONTENT_EPILOGUE);
		g_bytes_unref (bytes);
		g_clear_object (&reader->content);

		return TRUE;
	}

	part = GMIME_PART (parser_reader_peek (reader));
	if (end <= begin && newline == GMIME_READER_NEWLINE_UNKNOWN) {
		/* Leave Content null when there is no body at all. The part ends
		 * with its header block, whose newline can double as the newline
		 * preceding the next multipart boundary marker. */
		g_clear_object (&reader->content);
		parser_reader_finish_openpgp (reader, part);
		return TRUE;
	}

	errno = 0;

	if (reader->persistent) {
		reader->content = g_mime_stream_substream (reader->stream, begin, end);

		if (!reader->content)
			return parser_reader_io_error (error, "reference persistent");
	} else {
		/* The reader only delivers bytes within [begin, end), so this
		 * normally does nothing. Keep the cheap safeguard so content
		 * always matches the reported offsets, then rewind for reading. */
		if (g_mime_stream_block_set_length (GMIME_STREAM_BLOCK (reader->content), end - begin) < 0 ||
		    g_mime_stream_seek (reader->content, 0, GMIME_STREAM_SEEK_SET) != 0)
			return parser_reader_io_error (error, "rewind buffered");
	}

	content = g_mime_content_new_with_stream (reader->content, g_mime_part_get_content_encoding (part));

	if (newline == GMIME_READER_NEWLINE_UNIX)
		g_mime_content_set_newline_format (content, GMIME_NEWLINE_FORMAT_UNIX);
	else if (newline == GMIME_READER_NEWLINE_DOS)
		g_mime_content_set_newline_format (content, GMIME_NEWLINE_FORMAT_DOS);
	else if (newline == GMIME_READER_NEWLINE_MIXED)
		g_mime_content_set_newline_format (content, GMIME_NEWLINE_FORMAT_MIXED);

	g_mime_part_set_content (part, content);
	parser_reader_finish_openpgp (reader, part);
	g_object_unref (content);
	g_clear_object (&reader->content);

	return TRUE;
}

static void
g_mime_parser_reader_class_init (GMimeParserReaderClass *klass)
{
	G_OBJECT_CLASS (klass)->finalize = parser_reader_finalize;
	klass->headers_begin = parser_reader_headers_begin;
	klass->header_read = parser_reader_header_read;
	klass->headers_end = parser_reader_headers_end;
	klass->body_separator = parser_reader_body_separator;
	klass->entity_begin = parser_reader_entity_begin;
	klass->entity_end = parser_reader_entity_end;
	klass->message_begin = parser_reader_message_begin;
	klass->message_end = parser_reader_message_end;
	klass->marker_begin = parser_reader_marker_begin;
	klass->marker_read = parser_reader_marker_read;
	klass->boundary_begin = parser_reader_boundary_begin;
	klass->boundary_read = parser_reader_boundary_read;
	klass->boundary_end = parser_reader_boundary_end;
	klass->content_begin = parser_reader_content_begin;
	klass->content_read = parser_reader_content_read;
	klass->content_end = parser_reader_content_end;
}

static void
g_mime_parser_reader_init (GMimeParserReader *reader)
{
	reader->stack = g_ptr_array_new_with_free_func (g_object_unref);
	reader->marker_offset = -1;
	reader->headers_begin = reader->headers_end = -1;
}

GMimeReader *
_g_mime_parser_reader_new (GMimeParserOptions *options, GMimeStream *stream, gboolean persist)
{
	GMimeParserReader *reader;

	g_return_val_if_fail (GMIME_IS_STREAM (stream), NULL);

	reader = g_object_new (g_mime_parser_reader_get_type (), NULL);
	reader->stream = g_object_ref (stream);
	reader->persist = persist;
	reader->persistent = persist && g_mime_stream_tell (stream) >= 0;
	g_mime_reader_set_options ((GMimeReader *) reader, options);
	g_mime_reader_set_stream ((GMimeReader *) reader, stream);

	return (GMimeReader *) reader;
}

void
_g_mime_parser_reader_set_persist (GMimeReader *base, gboolean persist)
{
	GMimeParserReader *reader = (GMimeParserReader *) base;

	reader->persist = persist;
	reader->persistent = persist && g_mime_stream_tell (reader->stream) >= 0;
}

void
_g_mime_parser_reader_set_header_callback (GMimeReader *base,
					  gboolean (*callback) (GMimeHeader *, gboolean, gpointer, GError **), gpointer user_data)
{
	GMimeParserReader *reader = (GMimeParserReader *) base;

	reader->header_callback = callback;
	reader->header_data = user_data;
}

gint64
_g_mime_parser_reader_get_headers_begin (GMimeReader *base)
{
	return ((GMimeParserReader *) base)->headers_begin;
}

gint64
_g_mime_parser_reader_get_headers_end (GMimeReader *base)
{
	return ((GMimeParserReader *) base)->headers_end;
}

static gboolean
parser_reader_initialize (GMimeReader *base, GError **error)
{
	GMimeParserReader *reader = (GMimeParserReader *) base;
	GMimeStream *stream = _g_mime_reader_get_stream (base);

	parser_reader_reset (reader);
	g_clear_pointer (&reader->marker, g_bytes_unref);
	reader->marker_offset = -1;
	reader->headers_begin = reader->headers_end = -1;

	if (stream != reader->stream) {
		g_set_object (&reader->stream, stream);
		reader->persistent = reader->persist && g_mime_stream_tell (stream) >= 0;
	}

	/* If a previously parsed part's content has been read, the source
	 * position may have moved and must be restored before parsing again. */
	return g_mime_stream_tell (stream) < 0 || _g_mime_reader_restore_stream_position (base, error);
}

GMimeHeaderList *
_g_mime_parser_reader_construct_headers (GMimeReader *base, gboolean stop_at_separator, GError **error)
{
	GMimeParserReader *reader = (GMimeParserReader *) base;

	g_return_val_if_fail (G_TYPE_CHECK_INSTANCE_TYPE (base, g_mime_parser_reader_get_type ()), NULL);
	g_return_val_if_fail (error == NULL || *error == NULL, NULL);

	if (!parser_reader_initialize (base, error) ||
	    !(stop_at_separator ? _g_mime_reader_read_headers_to_separator (base, error) :
	      g_mime_reader_read_headers (base, error))) {
		parser_reader_reset (reader);
		reader->headers_begin = reader->headers_end = -1;
		return NULL;
	}

	return g_steal_pointer (&reader->headers);
}

GMimeEntity *
_g_mime_parser_reader_construct_part (GMimeReader *base, GError **error)
{
	GMimeParserReader *reader = (GMimeParserReader *) base;
	GMimeEntity *entity;

	g_return_val_if_fail (G_TYPE_CHECK_INSTANCE_TYPE (base, g_mime_parser_reader_get_type ()), NULL);
	g_return_val_if_fail (error == NULL || *error == NULL, NULL);

	if (!parser_reader_initialize (base, error) || !_g_mime_reader_has_input (base, error) ||
	    !g_mime_reader_read_entity (base, error)) {
		parser_reader_reset (reader);
		return NULL;
	}

	entity = g_object_ref (g_ptr_array_index (reader->stack, 0));
	parser_reader_reset (reader);
	return entity;
}

GMimeMessage *
_g_mime_parser_reader_construct_message (GMimeReader *base, GError **error)
{
	GMimeParserReader *reader = (GMimeParserReader *) base;
	GMimeMessage *message;

	g_return_val_if_fail (G_TYPE_CHECK_INSTANCE_TYPE (base, g_mime_parser_reader_get_type ()), NULL);
	g_return_val_if_fail (error == NULL || *error == NULL, NULL);

	if (!parser_reader_initialize (base, error) || !_g_mime_reader_has_input (base, error) ||
	    !g_mime_reader_read_message (base, error)) {
		parser_reader_reset (reader);
		return NULL;
	}

	message = g_object_ref (g_ptr_array_index (reader->stack, 0));
	if (reader->pending_marker)
		reader->marker = g_byte_array_free_to_bytes (g_steal_pointer (&reader->pending_marker));
	parser_reader_reset (reader);

	return message;
}

GBytes *
_g_mime_parser_reader_get_mbox_marker (GMimeReader *base)
{
	g_return_val_if_fail (G_TYPE_CHECK_INSTANCE_TYPE (base, g_mime_parser_reader_get_type ()), NULL);

	return ((GMimeParserReader *) base)->marker;
}

gint64
_g_mime_parser_reader_get_mbox_marker_offset (GMimeReader *base)
{
	GMimeParserReader *reader;

	g_return_val_if_fail (G_TYPE_CHECK_INSTANCE_TYPE (base, g_mime_parser_reader_get_type ()), -1);

	reader = (GMimeParserReader *) base;
	return reader->marker ? reader->marker_offset : -1;
}
