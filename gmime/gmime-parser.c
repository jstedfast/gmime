/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*  GMime
 *  Copyright (C) 2000-2026 Jeffrey Stedfast
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

#include <fcntl.h>

#include "gmime-parser.h"
#include "gmime-error.h"
#include "gmime-stream-fs.h"
#include "gmime-parser-reader-private.h"
#include "gmime-reader-private.h"
#include "gmime-internal.h"

#include "gmime-object-private.h"

/**
 * SECTION: gmime-parser
 * @title: GMimeParser
 * @short_description: Message and MIME part parser
 *
 * A #GMimeParser builds headers, MIME entities and messages using a single
 * buffered #GMimeReader. Mbox and MMDF streams support successive messages.
 **/

struct _GMimeParserPrivate {
	GMimeReader *reader;
	GMimeFormat format;
	GRegex *regex;
	GMimeParserHeaderRegexFunc header_cb;
	gpointer user_data;
	gboolean persist_stream;
	gboolean reading;
};

G_DEFINE_FINAL_TYPE (GMimeParser, g_mime_parser, G_TYPE_OBJECT)

static void
g_mime_parser_finalize (GObject *object)
{
	GMimeParser *parser = (GMimeParser *) object;

	g_clear_object (&parser->priv->reader);
	g_clear_pointer (&parser->priv->regex, g_regex_unref);
	g_free (parser->priv);
	G_OBJECT_CLASS (g_mime_parser_parent_class)->finalize (object);
}

static void
g_mime_parser_class_init (GMimeParserClass *klass)
{
	G_OBJECT_CLASS (klass)->finalize = g_mime_parser_finalize;
}

static void
g_mime_parser_init (GMimeParser *parser)
{
	parser->priv = g_new0 (struct _GMimeParserPrivate, 1);
	parser->priv->format = GMIME_FORMAT_MESSAGE;
	parser->priv->persist_stream = TRUE;
}

static gboolean
parser_header_read (GMimeHeader *header, gboolean toplevel, gpointer user_data, GError **error)
{
	GMimeParser *parser = user_data;
	struct _GMimeParserPrivate *priv = parser->priv;
	gconstpointer field;
	gsize length;

	field = g_bytes_get_data (header->raw_name, &length);

	/* Embedded messages can retain an incomplete leading line as raw
	 * pre-header bytes rather than treating it as a malformed root header. */
	if (!*header->name || (toplevel && header->invalid && !memchr (field, '\n', length))) {
		g_set_error (error, GMIME_ERROR, GMIME_ERROR_PARSE_ERROR,
			     "Failed to parse header at offset %" G_GINT64_FORMAT ".", header->offset);
		return FALSE;
	}

	if (!header->invalid && priv->regex && g_regex_match (priv->regex, header->name, 0, NULL)) {
		char *value = _g_mime_header_dup_raw_value (header);

		priv->header_cb (parser, header->name, value, header->offset, priv->user_data);
		g_free (value);
	}

	return TRUE;
}

static void
parser_propagate_error (GMimeParser *parser, GError *failure, GError **error)
{
	if (!failure)
		return;

	if (_g_mime_reader_header_limit_exceeded (parser->priv->reader))
		failure->code = GMIME_ERROR_HEADER_TOO_LONG;

	g_propagate_error (error, failure);
}

static gboolean
parser_check_source (GMimeStream *stream, GError **error)
{
	gint64 position;
	int errsv;

	/* Native streams expose no CanSeek property. Probe without moving
	 * the input before parsing, rather than silently copying content. */
	errno = 0;
	position = g_mime_stream_tell (stream);
	if (position >= 0 && g_mime_stream_seek (stream, 0, GMIME_STREAM_SEEK_CUR) == position)
		return TRUE;

	errsv = errno;
	g_set_error (error, GMIME_ERROR, errsv ? errsv : GMIME_ERROR_NOT_SUPPORTED,
		     "Source content storage requires a seekable stream: %s",
		     errsv ? g_strerror (errsv) : "seeking is not supported");
	errno = errsv;
	return FALSE;
}

/**
 * g_mime_parser_new:
 *
 * Creates a new parser object.
 *
 * Returns: (transfer full): a new parser object.
 **/
GMimeParser *
g_mime_parser_new (void)
{
	return g_object_new (GMIME_TYPE_PARSER, NULL);
}

/**
 * g_mime_parser_new_with_stream:
 * @stream: raw message or part stream
 *
 * Creates a parser at the current position of @stream.
 *
 * Returns: (transfer full): a new parser object.
 **/
GMimeParser *
g_mime_parser_new_with_stream (GMimeStream *stream)
{
	GMimeParser *parser;

	g_return_val_if_fail (GMIME_IS_STREAM (stream), NULL);

	parser = g_mime_parser_new ();
	g_mime_parser_init_with_stream (parser, stream);

	return parser;
}

/**
 * g_mime_parser_new_with_stream_and_storage:
 * @stream: input at the start of the entity, message or mailbox
 * @format: the input format
 * @storage: storage for parsed MIME part content
 * @error: return location for a #GError, or %NULL
 *
 * Creates a parser at the current position of @stream with explicit format
 * and content storage. Memory storage copies content; source storage
 * references leaf content through substreams. Source storage requires
 * working tell, seek and substream operations. Seekability is checked with
 * a zero-offset current-position seek before reading. Unsupported seeking
 * reports an error rather than falling back to memory; substream errors
 * are reported when parsing content.
 *
 * The caller owns @stream in both modes. Neither the parser nor returned
 * models close a stream still owned by the caller. With source storage,
 * keep the backing input open and unchanged while parsed content is used;
 * content views retain it after the parser is released or reinitialized.
 *
 * Returns: (nullable) (transfer full): a new parser, or %NULL on failure.
 **/
GMimeParser *
g_mime_parser_new_with_stream_and_storage (GMimeStream *stream, GMimeFormat format,
					  GMimeContentStorage storage, GError **error)
{
	GMimeParser *parser = g_mime_parser_new ();

	if (!g_mime_parser_init_with_stream_and_storage (parser, stream, format, storage, error)) {
		int errsv = errno;

		g_object_unref (parser);
		errno = errsv;
		return NULL;
	}

	return parser;
}

/**
 * g_mime_parser_init_with_stream:
 * @parser: a #GMimeParser context
 * @stream: raw message or part stream
 *
 * Replaces the input at its current position, discarding buffered data,
 * metadata and any previous error. Neither stream is reset or closed.
 * Parser format, persistence and header-regex settings are retained.
 *
 * Do not read, write, seek or reset @stream while parsing. Persistent part
 * content may be read between operations: its physical source position is
 * restored before the next operation. Use g_mime_parser_tell() for logical
 * positions; the underlying stream can be ahead due to buffered input.
 * Must not be called from a parsing callback.
 **/
void
g_mime_parser_init_with_stream (GMimeParser *parser, GMimeStream *stream)
{
	GMimeReader *reader;

	g_return_if_fail (GMIME_IS_PARSER (parser));
	g_return_if_fail (GMIME_IS_STREAM (stream));
	g_return_if_fail (!parser->priv->reading);

	reader = _g_mime_parser_reader_new (NULL, stream, parser->priv->persist_stream);
	_g_mime_reader_set_format (reader, parser->priv->format);
	_g_mime_parser_reader_set_header_callback (reader, parser_header_read, parser);
	g_clear_object (&parser->priv->reader);
	parser->priv->reader = reader;
}

/**
 * g_mime_parser_init_with_stream_and_storage:
 * @parser: a #GMimeParser context
 * @stream: replacement input at its current position
 * @format: the input format
 * @storage: storage for parsed MIME part content
 * @error: return location for a #GError, or %NULL
 *
 * Replaces the input, format and storage, discarding buffered data, metadata
 * and any previous parsing error. Header-regex settings are retained.
 * Neither stream is reset or closed. Previously returned models retain
 * their content and are unaffected by stream replacement.
 *
 * Source storage has the capability and lifetime requirements described
 * in g_mime_parser_new_with_stream_and_storage(). Validation occurs before
 * replacing the input: failure leaves the parser's input, buffered data,
 * metadata, format and storage unchanged.
 *
 * Use g_mime_parser_tell() for logical positions. Do not manipulate @stream
 * while parsing; source-backed content may be read between operations.
 * Must not be called from a parsing callback.
 *
 * Returns: %TRUE on success, or %FALSE on failure.
 **/
gboolean
g_mime_parser_init_with_stream_and_storage (GMimeParser *parser, GMimeStream *stream,
					   GMimeFormat format, GMimeContentStorage storage, GError **error)
{
	g_return_val_if_fail (GMIME_IS_PARSER (parser), FALSE);
	g_return_val_if_fail (GMIME_IS_STREAM (stream), FALSE);
	g_return_val_if_fail (format >= GMIME_FORMAT_MESSAGE && format <= GMIME_FORMAT_MMDF, FALSE);
	g_return_val_if_fail (storage == GMIME_CONTENT_STORAGE_MEMORY || storage == GMIME_CONTENT_STORAGE_SOURCE, FALSE);
	g_return_val_if_fail (error == NULL || *error == NULL, FALSE);
	g_return_val_if_fail (!parser->priv->reading, FALSE);

	if (storage == GMIME_CONTENT_STORAGE_SOURCE && !parser_check_source (stream, error))
		return FALSE;

	parser->priv->format = format;
	parser->priv->persist_stream = storage == GMIME_CONTENT_STORAGE_SOURCE;
	g_mime_parser_init_with_stream (parser, stream);

	return TRUE;
}

/**
 * g_mime_parser_get_persist_stream:
 * @parser: a #GMimeParser context
 *
 * Gets whether content is retained on the seekable source stream rather
 * than copied into memory.
 *
 * Returns: %TRUE if persistence is enabled and the stream is seekable.
 **/
gboolean
g_mime_parser_get_persist_stream (GMimeParser *parser)
{
	g_return_val_if_fail (GMIME_IS_PARSER (parser), FALSE);

	return parser->priv->persist_stream && parser->priv->reader &&
		g_mime_stream_tell (_g_mime_reader_get_stream (parser->priv->reader)) >= 0;
}

/**
 * g_mime_parser_set_persist_stream:
 * @parser: a #GMimeParser context
 * @persist: whether to retain content on the source
 *
 * Changes persistence for subsequent construction operations without
 * discarding buffered input. Enabled by default, but ignored for
 * nonseekable streams. Previously returned content is unaffected.
 * Must not be called from a parsing callback.
 **/
void
g_mime_parser_set_persist_stream (GMimeParser *parser, gboolean persist)
{
	g_return_if_fail (GMIME_IS_PARSER (parser));
	g_return_if_fail (!parser->priv->reading);

	parser->priv->persist_stream = persist;
	if (parser->priv->reader)
		_g_mime_parser_reader_set_persist (parser->priv->reader, persist);
}

/**
 * g_mime_parser_get_format:
 * @parser: a #GMimeParser context
 *
 * Returns: the expected input format.
 **/
GMimeFormat
g_mime_parser_get_format (GMimeParser *parser)
{
	g_return_val_if_fail (GMIME_IS_PARSER (parser), GMIME_FORMAT_MESSAGE);

	return parser->priv->format;
}

/**
 * g_mime_parser_set_format:
 * @parser: a #GMimeParser context
 * @format: the expected input format
 *
 * Changes the input format without discarding buffered input.
 * Must not be called from a parsing callback.
 **/
void
g_mime_parser_set_format (GMimeParser *parser, GMimeFormat format)
{
	g_return_if_fail (GMIME_IS_PARSER (parser));
	g_return_if_fail (format >= GMIME_FORMAT_MESSAGE && format <= GMIME_FORMAT_MMDF);
	g_return_if_fail (!parser->priv->reading);

	parser->priv->format = format;
	if (parser->priv->reader)
		_g_mime_reader_set_format (parser->priv->reader, format);
}

/**
 * g_mime_parser_set_header_regex: (skip)
 * @parser: a #GMimeParser context
 * @regex: (nullable): regular expression, or %NULL to unregister
 * @header_cb: callback function
 * @user_data: user data
 *
 * Calls @header_cb for each valid header whose name matches @regex,
 * including headers of nested entities. Values retain their raw folding.
 **/
void
g_mime_parser_set_header_regex (GMimeParser *parser, const char *regex,
				GMimeParserHeaderRegexFunc header_cb, gpointer user_data)
{
	g_return_if_fail (GMIME_IS_PARSER (parser));

	g_clear_pointer (&parser->priv->regex, g_regex_unref);
	parser->priv->header_cb = header_cb;
	parser->priv->user_data = user_data;

	if (regex && header_cb)
		parser->priv->regex = g_regex_new (regex, G_REGEX_RAW | G_REGEX_EXTENDED | G_REGEX_CASELESS, 0, NULL);
}

/**
 * g_mime_parser_construct_headers:
 * @parser: a #GMimeParser context
 * @options: (nullable): parser options, or %NULL for defaults
 * @error: return location for a #GError, or %NULL
 *
 * Reads a header block directly at the current logical position, without
 * searching for a mailbox marker or stripping a byte-order mark. Retains
 * message and content headers, original field bytes and offsets. A blank
 * separator is consumed but is not included in the list.
 *
 * Empty input returns an empty list. Fatal syntax, header limits and I/O
 * errors discard partial results. Errors remain sticky until stream
 * replacement. Options are copied at the start of each operation.
 * Reentrant or concurrent operations are not supported.
 *
 * Returns: (nullable) (transfer full): parsed headers, or %NULL on failure.
 **/
GMimeHeaderList *
g_mime_parser_construct_headers (GMimeParser *parser, GMimeParserOptions *options, GError **error)
{
	GMimeHeaderList *headers;
	GError *failure = NULL;

	g_return_val_if_fail (GMIME_IS_PARSER (parser), NULL);
	g_return_val_if_fail (parser->priv->reader != NULL, NULL);
	g_return_val_if_fail (!parser->priv->reading, NULL);
	g_return_val_if_fail (error == NULL || *error == NULL, NULL);

	g_object_ref (parser);
	parser->priv->reading = TRUE;
	g_mime_reader_set_options (parser->priv->reader, options);
	headers = _g_mime_parser_reader_construct_headers (parser->priv->reader, TRUE, &failure);
	parser_propagate_error (parser, failure, error);

	if (headers) {
		int i, count = g_mime_header_list_get_count (headers);

		/* Standalone lists own their parser options; their headers can
		 * borrow that snapshot until transferred to another list. */
		for (i = 0; i < count; i++) {
			GMimeHeader *header = g_mime_header_list_get_header_at (headers, i);

			g_mime_parser_options_free (header->options);
			header->options = headers->options;
			header->owns_options = FALSE;
		}
	}

	parser->priv->reading = FALSE;
	g_object_unref (parser);
	return headers;
}

/**
 * g_mime_parser_construct_part:
 * @parser: a #GMimeParser context
 * @options: (nullable): parser options, or %NULL for defaults
 * @error: return location for a #GError, or %NULL
 *
 * Constructs a MIME entity directly at the current logical position,
 * without mailbox marker search or byte-order-mark stripping. All input
 * operations share the same reader and buffered bytes.
 *
 * Fatal syntax, header limits and I/O errors discard partial results and
 * remain sticky until stream replacement. Options are copied per operation.
 * Reentrant or concurrent operations are not supported.
 *
 * Returns: (nullable) (transfer full): an entity, or %NULL on failure or
 * end of input. End of input does not set @error.
 **/
GMimeEntity *
g_mime_parser_construct_part (GMimeParser *parser, GMimeParserOptions *options, GError **error)
{
	GMimeEntity *entity;
	GError *failure = NULL;

	g_return_val_if_fail (GMIME_IS_PARSER (parser), NULL);
	g_return_val_if_fail (parser->priv->reader != NULL, NULL);
	g_return_val_if_fail (!parser->priv->reading, NULL);
	g_return_val_if_fail (error == NULL || *error == NULL, NULL);

	g_object_ref (parser);
	parser->priv->reading = TRUE;
	g_mime_reader_set_options (parser->priv->reader, options);
	entity = _g_mime_parser_reader_construct_part (parser->priv->reader, &failure);
	parser_propagate_error (parser, failure, error);
	parser->priv->reading = FALSE;
	g_object_unref (parser);

	return entity;
}

/**
 * g_mime_parser_construct_message:
 * @parser: a #GMimeParser context
 * @options: (nullable): parser options, or %NULL for defaults
 * @error: return location for a #GError, or %NULL
 *
 * Constructs a message through EOF or the next mailbox delimiter. The
 * first message operation on a newly assigned stream strips an optional
 * UTF-8 byte-order mark. Mbox and MMDF inputs search for an opening marker.
 * Persistent content may be read between successive message operations.
 *
 * Fatal syntax, header limits and I/O errors discard partial results and
 * remain sticky until stream replacement. Options are copied per operation.
 * Reentrant or concurrent operations are not supported.
 *
 * Returns: (nullable) (transfer full): a message, or %NULL on failure or
 * end of input. End of input does not set @error.
 **/
GMimeMessage *
g_mime_parser_construct_message (GMimeParser *parser, GMimeParserOptions *options, GError **error)
{
	GMimeMessage *message;
	GError *failure = NULL;

	g_return_val_if_fail (GMIME_IS_PARSER (parser), NULL);
	g_return_val_if_fail (parser->priv->reader != NULL, NULL);
	g_return_val_if_fail (!parser->priv->reading, NULL);
	g_return_val_if_fail (error == NULL || *error == NULL, NULL);

	g_object_ref (parser);
	parser->priv->reading = TRUE;
	g_mime_reader_set_options (parser->priv->reader, options);
	message = _g_mime_parser_reader_construct_message (parser->priv->reader, &failure);
	parser_propagate_error (parser, failure, error);
	parser->priv->reading = FALSE;
	g_object_unref (parser);

	return message;
}

GMimeEntity *
_g_mime_parser_load (GMimeParserOptions *options, GMimeStream *stream, GMimeContentStorage storage,
		     gboolean message, GError **error)
{
	GMimeParser *parser;
	GMimeEntity *entity;
	GError *failure = NULL;
	int errsv;

	if (!(parser = g_mime_parser_new_with_stream_and_storage (stream, GMIME_FORMAT_MESSAGE, storage, error)))
		return NULL;

	errno = 0;
	entity = message ? (GMimeEntity *) g_mime_parser_construct_message (parser, options, &failure) :
		g_mime_parser_construct_part (parser, options, &failure);
	errsv = errno;

	g_object_unref (parser);

	if (!entity && !failure)
		g_set_error_literal (&failure, GMIME_ERROR, GMIME_ERROR_PARSE_ERROR, "End of stream.");

	if (failure)
		g_propagate_error (error, failure);

	errno = errsv;

	return entity;
}

GMimeEntity *
_g_mime_parser_load_from_file (GMimeParserOptions *options, const char *filename, gboolean message, GError **error)
{
	GMimeStream *stream;
	GMimeEntity *entity;
	GError *failure = NULL;
	int errsv;

	g_return_val_if_fail (filename != NULL, NULL);
	g_return_val_if_fail (error == NULL || *error == NULL, NULL);

	if (!(stream = g_mime_stream_fs_open (filename, O_RDONLY, 0, error)))
		return NULL;

	entity = _g_mime_parser_load (options, stream, GMIME_CONTENT_STORAGE_MEMORY, message, &failure);
	errsv = errno;

	/* Closing must not replace an earlier parsing or read failure. */
	errno = 0;

	if (g_mime_stream_close (stream) < 0 && !failure) {
		errsv = errno;
		g_set_error (&failure, GMIME_ERROR, errsv ? errsv : GMIME_ERROR_GENERAL,
			     "Failed to close `%s': %s", filename, errsv ? g_strerror (errsv) : "unknown I/O error");
		g_clear_object (&entity);
	}

	g_object_unref (stream);

	if (failure)
		g_propagate_error (error, failure);

	errno = errsv;

	return entity;
}

GMimeStream *
_g_mime_parser_read_to_eos (GMimeParser *parser, GError **error)
{
	GMimeStream *stream;

	g_return_val_if_fail (GMIME_IS_PARSER (parser), NULL);
	g_return_val_if_fail (parser->priv->reader != NULL, NULL);
	g_return_val_if_fail (!parser->priv->reading, NULL);
	g_return_val_if_fail (error == NULL || *error == NULL, NULL);

	g_object_ref (parser);
	parser->priv->reading = TRUE;

	/* The Office365 status-group recovery must drain buffered bytes too;
	 * a decoded, nonseekable source cannot be rewound to the logical offset. */
	stream = _g_mime_reader_read_to_eos (parser->priv->reader, error);
	parser->priv->reading = FALSE;
	g_object_unref (parser);

	return stream;
}

/**
 * g_mime_parser_tell:
 * @parser: a #GMimeParser context
 *
 * Gets the logical input position, excluding unread buffered bytes.
 * Nonseekable streams use offset 0 as their initial origin.
 *
 * Returns: the logical offset, or %-1 if no stream has been assigned.
 **/
gint64
g_mime_parser_tell (GMimeParser *parser)
{
	g_return_val_if_fail (GMIME_IS_PARSER (parser), -1);

	return parser->priv->reader ? g_mime_reader_tell (parser->priv->reader) : -1;
}

/**
 * g_mime_parser_eos:
 * @parser: a #GMimeParser context
 *
 * Returns: %TRUE if parsing has reached EOF, or no stream is assigned.
 **/
gboolean
g_mime_parser_eos (GMimeParser *parser)
{
	g_return_val_if_fail (GMIME_IS_PARSER (parser), TRUE);

	return !parser->priv->reader || _g_mime_reader_input_eos (parser->priv->reader);
}

/**
 * g_mime_parser_get_mbox_marker:
 * @parser: a #GMimeParser context
 *
 * Gets the opening From-line, excluding CRLF, of the last successfully
 * constructed mbox message. The next construction operation clears it.
 *
 * Returns: (nullable) (transfer full): the From-line, or %NULL if unavailable.
 **/
char *
g_mime_parser_get_mbox_marker (GMimeParser *parser)
{
	GBytes *marker;
	gconstpointer data;
	gsize length;

	g_return_val_if_fail (GMIME_IS_PARSER (parser), NULL);

	if (!parser->priv->reader || parser->priv->format != GMIME_FORMAT_MBOX ||
	    !(marker = _g_mime_parser_reader_get_mbox_marker (parser->priv->reader)))
		return NULL;

	data = g_bytes_get_data (marker, &length);

	return g_strndup (data, length);
}

/**
 * g_mime_parser_get_mbox_marker_offset:
 * @parser: a #GMimeParser context
 *
 * Returns: the last successfully constructed mbox message's opening
 * From-line offset, or %-1 if unavailable.
 **/
gint64
g_mime_parser_get_mbox_marker_offset (GMimeParser *parser)
{
	g_return_val_if_fail (GMIME_IS_PARSER (parser), -1);

	if (!parser->priv->reader || parser->priv->format != GMIME_FORMAT_MBOX)
		return -1;

	return _g_mime_parser_reader_get_mbox_marker_offset (parser->priv->reader);
}

/**
 * g_mime_parser_get_headers_begin:
 * @parser: a #GMimeParser context
 *
 * Returns: the most recent top-level header block's starting offset,
 * or %-1 if unavailable.
 **/
gint64
g_mime_parser_get_headers_begin (GMimeParser *parser)
{
	g_return_val_if_fail (GMIME_IS_PARSER (parser), -1);

	return parser->priv->reader ? _g_mime_parser_reader_get_headers_begin (parser->priv->reader) : -1;
}

/**
 * g_mime_parser_get_headers_end:
 * @parser: a #GMimeParser context
 *
 * Returns: the start of the most recent top-level header block's blank
 * separator, or %-1 if no separator was encountered.
 **/
gint64
g_mime_parser_get_headers_end (GMimeParser *parser)
{
	g_return_val_if_fail (GMIME_IS_PARSER (parser), -1);

	return parser->priv->reader ? _g_mime_parser_reader_get_headers_end (parser->priv->reader) : -1;
}
