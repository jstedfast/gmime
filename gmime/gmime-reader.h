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

#ifndef __GMIME_READER_H__
#define __GMIME_READER_H__

#include <gmime/gmime-parser-options.h>
#include <gmime/gmime-content-type.h>
#include <gmime/gmime-stream.h>
#include <gmime/gmime-compliance.h>

G_BEGIN_DECLS

#define GMIME_TYPE_READER            (g_mime_reader_get_type ())


/**
 * GMimeReaderEntityType:
 * @GMIME_READER_ENTITY_PART: an opaque leaf, including depth-limited containers
 * @GMIME_READER_ENTITY_MULTIPART: a multipart container
 * @GMIME_READER_ENTITY_MESSAGE_PART: an unencoded embedded message container
 *
 * The wire structure selected by the reader, not a constructed object type.
 **/
typedef enum {
	GMIME_READER_ENTITY_PART,
	GMIME_READER_ENTITY_MULTIPART,
	GMIME_READER_ENTITY_MESSAGE_PART
} GMimeReaderEntityType;

/**
 * GMimeReaderContentType:
 * @GMIME_READER_CONTENT_PART: raw, transfer-encoded leaf content
 * @GMIME_READER_CONTENT_PREAMBLE: multipart preamble
 * @GMIME_READER_CONTENT_EPILOGUE: multipart epilogue
 *
 * The kind of data delivered by content callbacks.
 **/
typedef enum {
	GMIME_READER_CONTENT_PART,
	GMIME_READER_CONTENT_PREAMBLE,
	GMIME_READER_CONTENT_EPILOGUE
} GMimeReaderContentType;

/**
 * GMimeReaderNewLineFormat:
 * @GMIME_READER_NEWLINE_UNKNOWN: no LF or CRLF was scanned
 * @GMIME_READER_NEWLINE_UNIX: only bare LF was scanned
 * @GMIME_READER_NEWLINE_DOS: only CRLF was scanned
 * @GMIME_READER_NEWLINE_MIXED: both bare LF and CRLF were scanned
 *
 * Newline formats observed in raw leaf content. Unlike output formatting
 * options, a reader can report mixed input or input without any newline.
 **/
typedef enum {
	GMIME_READER_NEWLINE_UNKNOWN,
	GMIME_READER_NEWLINE_UNIX,
	GMIME_READER_NEWLINE_DOS,
	GMIME_READER_NEWLINE_MIXED
} GMimeReaderNewLineFormat;

/**
 * GMimeReader:
 *
 * A buffered, forward-only MIME reader. Subclasses consume wire data through
 * virtual callbacks without constructing a MIME object tree.
 **/
G_DECLARE_DERIVABLE_TYPE (GMimeReader, g_mime_reader, GMIME, READER, GObject)

/**
 * GMimeReaderClass:
 * @parent_class: parent class
 * @headers_begin: called at the beginning of a header block
 * @header_read: called with a complete raw header, including folded lines
 * @headers_end: called after the header block, including any body separator
 * @body_separator: called when the blank line separating headers and body is consumed
 * @entity_begin: called after entity headers, before its content or descendants
 * @entity_end: called after entity content and descendants
 * @content_begin: called before scanning leaf content, preamble or epilogue
 * @content_read: called with borrowed raw content chunks
 * @content_end: called after scanning content, with the leaf's newline format
 * @boundary_begin: called before consuming a multipart delimiter
 * @boundary_read: called with borrowed delimiter bytes
 * @boundary_end: called after consuming a multipart delimiter
 * @message_begin: called after a message's headers, before its root entity
 * @message_end: called after a message's root entity
 * @marker_begin: called before a mailbox separator
 * @marker_read: called with separator bytes, excluding CRLF or LF
 * @marker_end: called after consuming the separator and its newline
 *
 * Optional synchronous callbacks. Offsets are absolute for streams that
 * support tell(), otherwise relative to the stream position when
 * g_mime_reader_set_stream() was called.
 * Line numbers start at 1 there. Buffers are borrowed until the callback
 * returns; copy them if they are needed later. Returning %FALSE aborts the
 * operation; callbacks should set a #GError describing the failure.
 * Setting an error also aborts the operation even if the callback returns
 * %TRUE. After an error, no further callbacks are made, including matching
 * end callbacks.
 *
 * Callbacks may inspect the reader but must not read from it, change its
 * stream or parser options, or manipulate the underlying stream position.
 * Compliance options and the logger may be replaced from callbacks; the
 * active operation retains its original snapshot until it finishes.
 * Content types are borrowed and must not be modified during callbacks.
 * The entity callbacks coalesce MimeKit's separate part, multipart and
 * message-part callbacks; content callbacks similarly identify their kind.
 **/
struct _GMimeReaderClass {
	GObjectClass parent_class;

	/**
	 * GMimeReaderClass::headers_begin:
	 * @reader: a #GMimeReader
	 * @begin_offset: stream offset where the header block begins
	 * @begin_line_number: 1-based physical line number of the header block
	 * @error: return location for a #GError
	 *
	 * Called before reading the first header in a block, including blocks
	 * belonging to nested entities and embedded messages. Empty input can
	 * produce this callback without a matching headers_end callback.
	 *
	 * Returns: %TRUE to continue reading, or %FALSE on failure.
	 **/
	gboolean (* headers_begin) (GMimeReader *reader, gint64 begin_offset, guint64 begin_line_number, GError **error);

	/**
	 * GMimeReaderClass::header_read:
	 * @reader: a #GMimeReader
	 * @buffer: (array length=length) (element-type guint8): complete raw header bytes
	 * @length: number of bytes, including line terminators
	 * @field_name_length: field name length, excluding blanks before the colon
	 * @field_length: length up to, but not including, the colon
	 * @invalid: whether the bytes do not form a valid header field
	 * @begin_offset: stream offset of the first byte
	 * @begin_line_number: 1-based line number of the first byte
	 * @error: return location for a #GError
	 *
	 * For invalid headers, both field lengths equal @length and there is
	 * no separate value. Otherwise, the raw value begins at @field_length
	 * plus one. No NUL termination or UTF-8 validity is implied.
	 *
	 * Returns: %TRUE to continue reading, or %FALSE on failure.
	 **/
	gboolean (* header_read) (GMimeReader *reader, const guint8 *buffer, gsize length,
				 gsize field_name_length, gsize field_length, gboolean invalid,
				 gint64 begin_offset, guint64 begin_line_number, GError **error);

	/**
	 * GMimeReaderClass::headers_end:
	 * @reader: a #GMimeReader
	 * @begin_offset: stream offset where the header block began
	 * @begin_line_number: 1-based physical line number where the block began
	 * @end_offset: stream offset immediately after the consumed header block
	 * @end_line_number: 1-based physical line number at @end_offset
	 * @error: return location for a #GError
	 *
	 * Called after the last header and any body_separator callback. If a
	 * separator was found, @end_offset includes its bytes; otherwise the
	 * block can end at EOF or before an unconsumed boundary. An end
	 * notification does not guarantee that the read operation will succeed.
	 *
	 * Returns: %TRUE to continue, or %FALSE on failure.
	 **/
	gboolean (* headers_end) (GMimeReader *reader, gint64 begin_offset, guint64 begin_line_number,
				 gint64 end_offset, guint64 end_line_number, GError **error);

	/**
	 * GMimeReaderClass::body_separator:
	 * @reader: a #GMimeReader
	 * @begin_offset: stream offset of the blank line separating headers and body
	 * @line_number: 1-based physical line number of the separator
	 * @end_offset: stream offset immediately after the separator
	 * @error: return location for a #GError
	 *
	 * Called after consuming the separator, before headers_end. Its bytes
	 * are not delivered by header_read or content_read. No callback is made
	 * when the separator is missing. A final bare CR is also accepted as a
	 * separator.
	 *
	 * Returns: %TRUE to continue, or %FALSE on failure.
	 **/
	gboolean (* body_separator) (GMimeReader *reader, gint64 begin_offset, guint64 line_number,
				    gint64 end_offset, GError **error);

	/**
	 * GMimeReaderClass::entity_begin:
	 * @reader: a #GMimeReader
	 * @type: the entity's wire-structure classification
	 * @content_type: (transfer none): borrowed media type; must not be modified
	 * @begin_offset: stream offset where the entity's header block began
	 * @begin_line_number: 1-based physical line number where its headers began
	 * @error: return location for a #GError
	 *
	 * Called after headers_end and, for a message's root entity, after
	 * message_begin. Content and descendant callbacks follow. The reader
	 * supplies a default media type if the headers do not specify one.
	 *
	 * Returns: %TRUE to continue, or %FALSE on failure.
	 **/
	gboolean (* entity_begin) (GMimeReader *reader, GMimeReaderEntityType type, GMimeContentType *content_type,
				  gint64 begin_offset, guint64 begin_line_number, GError **error);

	/**
	 * GMimeReaderClass::entity_end:
	 * @reader: a #GMimeReader
	 * @type: the entity's wire-structure classification
	 * @content_type: (transfer none): borrowed media type; must not be modified
	 * @begin_offset: stream offset where the entity's header block began
	 * @begin_line_number: 1-based physical line number where its headers began
	 * @headers_end_offset: end of the header block, clamped to @end_offset
	 * @end_offset: exclusive stream offset of the entity's end
	 * @lines: number of physical body lines, including descendants
	 * @error: return location for a #GError
	 *
	 * Called after the entity's content and descendants, before any
	 * enclosing entity_end or matching message_end callback. At a boundary,
	 * @end_offset excludes the preceding LF or CRLF and is never less than
	 * @begin_offset. Header lines are not included in @lines. Truncated
	 * entities can end at an ancestor's boundary without consuming it.
	 *
	 * Returns: %TRUE to continue, or %FALSE on failure.
	 **/
	gboolean (* entity_end) (GMimeReader *reader, GMimeReaderEntityType type, GMimeContentType *content_type,
				gint64 begin_offset, guint64 begin_line_number, gint64 headers_end_offset,
				gint64 end_offset, guint64 lines, GError **error);

	/**
	 * GMimeReaderClass::content_begin:
	 * @reader: a #GMimeReader
	 * @type: the content kind
	 * @begin_offset: stream offset where content scanning begins
	 * @begin_line_number: 1-based physical line number where scanning begins
	 * @error: return location for a #GError
	 *
	 * Called before scanning leaf wire content, a multipart preamble or
	 * an epilogue. Empty content can produce content_begin and content_end
	 * without any content_read callbacks. Transfer encodings are not decoded.
	 *
	 * Returns: %TRUE to continue, or %FALSE on failure.
	 **/
	gboolean (* content_begin) (GMimeReader *reader, GMimeReaderContentType type,
				   gint64 begin_offset, guint64 begin_line_number, GError **error);

	/**
	 * GMimeReaderClass::content_read:
	 * @reader: a #GMimeReader
	 * @type: the content kind
	 * @buffer: (array length=length) (element-type guint8): borrowed wire bytes
	 * @length: number of bytes
	 * @error: return location for a #GError
	 *
	 * Chunk boundaries are unspecified. Transfer encodings are not decoded.
	 *
	 * Returns: %TRUE to continue, or %FALSE on failure.
	 **/
	gboolean (* content_read) (GMimeReader *reader, GMimeReaderContentType type,
				  const guint8 *buffer, gsize length, GError **error);

	/**
	 * GMimeReaderClass::content_end:
	 * @reader: a #GMimeReader
	 * @type: the content kind
	 * @begin_offset: stream offset where scanning began
	 * @begin_line_number: 1-based line number where scanning began
	 * @end_offset: stream offset after the delivered bytes
	 * @lines: number of content lines
	 * @newline_format: formats scanned in leaf content, or
	 *   %GMIME_READER_NEWLINE_UNKNOWN for preambles and epilogues
	 * @error: return location for a #GError
	 *
	 * The newline immediately preceding a delimiter is scanned even though
	 * it is excluded from delivered leaf bytes. Transfer encodings are not
	 * decoded, so this describes the wire format rather than decoded text.
	 *
	 * Returns: %TRUE to continue, or %FALSE on failure.
	 **/
	gboolean (* content_end) (GMimeReader *reader, GMimeReaderContentType type,
				 gint64 begin_offset, guint64 begin_line_number, gint64 end_offset,
				 guint64 lines, GMimeReaderNewLineFormat newline_format, GError **error);

	/**
	 * GMimeReaderClass::boundary_begin:
	 * @reader: a #GMimeReader
	 * @final: whether this is a closing multipart delimiter
	 * @begin_offset: stream offset of the delimiter's first hyphen
	 * @line_number: 1-based physical line number of the delimiter
	 * @error: return location for a #GError
	 *
	 * Called before consuming a MIME multipart delimiter. Opening
	 * delimiters precede the next child's headers; closing delimiters
	 * precede the multipart epilogue. Mailbox separators use marker
	 * callbacks instead.
	 *
	 * Returns: %TRUE to continue, or %FALSE on failure.
	 **/
	gboolean (* boundary_begin) (GMimeReader *reader, gboolean final, gint64 begin_offset,
				    guint64 line_number, GError **error);

	/**
	 * GMimeReaderClass::boundary_read:
	 * @reader: a #GMimeReader
	 * @final: whether this closes the multipart
	 * @buffer: (array length=length) (element-type guint8): borrowed delimiter bytes
	 * @length: number of bytes
	 * @begin_offset: offset of this chunk
	 * @line_number: line number of the delimiter
	 * @error: return location for a #GError
	 *
	 * Opening delimiters include their newline; closing delimiters leave
	 * it for the epilogue. The newline before a delimiter is not included.
	 *
	 * Returns: %TRUE to continue, or %FALSE on failure.
	 **/
	gboolean (* boundary_read) (GMimeReader *reader, gboolean final, const guint8 *buffer,
				   gsize length, gint64 begin_offset, guint64 line_number, GError **error);

	/**
	 * GMimeReaderClass::boundary_end:
	 * @reader: a #GMimeReader
	 * @final: whether this is a closing multipart delimiter
	 * @begin_offset: stream offset of the delimiter's first hyphen
	 * @line_number: 1-based physical line number of the delimiter
	 * @end_offset: stream offset immediately after the consumed delimiter bytes
	 * @error: return location for a #GError
	 *
	 * Called after the delimiter's boundary_read callbacks. Opening
	 * delimiters include their line terminator in @end_offset; closing
	 * delimiters leave it unread for the epilogue.
	 *
	 * Returns: %TRUE to continue, or %FALSE on failure.
	 **/
	gboolean (* boundary_end) (GMimeReader *reader, gboolean final, gint64 begin_offset,
				  guint64 line_number, gint64 end_offset, GError **error);

	/**
	 * GMimeReaderClass::message_begin:
	 * @reader: a #GMimeReader
	 * @begin_offset: stream offset where the message's header block began
	 * @begin_line_number: 1-based physical line number where its headers began
	 * @error: return location for a #GError
	 *
	 * Called after the message's headers_end callback and before its root
	 * entity_begin callback. Describes both top-level message reads and
	 * embedded messages encountered during entity traversal. A mailbox
	 * separator or initial UTF-8 BOM is not part of the header block.
	 *
	 * Returns: %TRUE to continue, or %FALSE on failure.
	 **/
	gboolean (* message_begin) (GMimeReader *reader, gint64 begin_offset, guint64 begin_line_number, GError **error);

	/**
	 * GMimeReaderClass::message_end:
	 * @reader: a #GMimeReader
	 * @begin_offset: stream offset where the message's header block began
	 * @begin_line_number: 1-based physical line number where its headers began
	 * @headers_end_offset: end of the header block, clamped to @end_offset
	 * @end_offset: exclusive stream offset of the root entity's end
	 * @lines: number of physical lines in the message body, including descendants
	 * @error: return location for a #GError
	 *
	 * Called after the root entity_end callback, for both top-level and
	 * embedded messages. End offsets and body line counts follow
	 * entity_end semantics and do not include message header lines.
	 *
	 * Returns: %TRUE to continue, or %FALSE on failure.
	 **/
	gboolean (* message_end) (GMimeReader *reader, gint64 begin_offset, guint64 begin_line_number,
				 gint64 headers_end_offset, gint64 end_offset, guint64 lines, GError **error);

	/**
	 * GMimeReaderClass::marker_begin:
	 * @reader: a #GMimeReader
	 * @begin_offset: stream offset of the mailbox separator's first byte
	 * @line_number: 1-based physical line number of the separator
	 * @error: return location for a #GError
	 *
	 * Called before delivering an mbox From-line or MMDF separator.
	 * Marker callbacks are used by mailbox message reads, not standalone
	 * header or entity reads. They precede the following header block;
	 * MMDF closing separators can also follow a message.
	 *
	 * Returns: %TRUE to continue, or %FALSE on failure.
	 **/
	gboolean (* marker_begin) (GMimeReader *reader, gint64 begin_offset, guint64 line_number, GError **error);

	/**
	 * GMimeReaderClass::marker_read:
	 * @reader: a #GMimeReader
	 * @buffer: (array length=length) (element-type guint8): borrowed mailbox separator bytes
	 * @length: number of bytes, excluding the line terminator
	 * @error: return location for a #GError
	 *
	 * Delivers mbox From-lines or MMDF separators without allocating a
	 * complete marker. Chunk boundaries are unspecified.
	 *
	 * Returns: %TRUE to continue, or %FALSE on failure.
	 **/
	gboolean (* marker_read) (GMimeReader *reader, const guint8 *buffer, gsize length, GError **error);

	/**
	 * GMimeReaderClass::marker_end:
	 * @reader: a #GMimeReader
	 * @begin_offset: stream offset of the mailbox separator's first byte
	 * @line_number: 1-based physical line number of the separator
	 * @end_offset: stream offset after the separator and its line terminator
	 * @error: return location for a #GError
	 *
	 * Called after all marker_read callbacks and after consuming the
	 * separator's LF or CRLF, which is not included in marker_read bytes.
	 * An unterminated separator fails without a marker_end callback.
	 *
	 * Returns: %TRUE to continue, or %FALSE on failure.
	 **/
	gboolean (* marker_end) (GMimeReader *reader, gint64 begin_offset, guint64 line_number,
				gint64 end_offset, GError **error);

	/*< private >*/
	gpointer reserved[8];
};


GMimeReader *g_mime_reader_new (GMimeParserOptions *options, GMimeStream *stream);
GMimeReader *g_mime_reader_new_with_format (GMimeParserOptions *options, GMimeStream *stream, GMimeFormat format);

void g_mime_reader_set_stream (GMimeReader *reader, GMimeStream *stream);
void g_mime_reader_set_stream_with_format (GMimeReader *reader, GMimeStream *stream, GMimeFormat format);
GMimeFormat g_mime_reader_get_format (GMimeReader *reader);
GMimeParserOptions *g_mime_reader_get_options (GMimeReader *reader);
void g_mime_reader_set_options (GMimeReader *reader, GMimeParserOptions *options);
GMimeComplianceOptions *g_mime_reader_get_compliance_options (GMimeReader *reader);
void g_mime_reader_set_compliance_options (GMimeReader *reader, GMimeComplianceOptions *options);
void g_mime_reader_set_compliance_logger (GMimeReader *reader, GMimeComplianceIssueFunc report,
					 gpointer user_data, GDestroyNotify destroy);

gint64 g_mime_reader_tell (GMimeReader *reader);
gboolean g_mime_reader_eos (GMimeReader *reader);

gboolean g_mime_reader_read_headers (GMimeReader *reader, GError **error);
gboolean g_mime_reader_read_entity (GMimeReader *reader, GError **error);
gboolean g_mime_reader_read_message (GMimeReader *reader, GError **error);

G_END_DECLS

#endif /* __GMIME_READER_H__ */
