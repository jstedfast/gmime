/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* Based on MimeKit's YEncOptions, YEncHeader, YEncFormat, YEncReader and YEncWriter.
 *
 * Copyright (c) 2013-2026 .NET Foundation and Contributors
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <errno.h>
#include <string.h>

#include "gmime-yenc.h"
#include "gmime-codecs.h"
#include "gmime-error.h"
#include "gmime-internal.h"

/**
 * SECTION: gmime-yenc
 * @title: yEnc blocks
 * @short_description: Streaming framed yEnc readers and writers
 *
 * These APIs handle single-part and multipart blocks, bounded input lines,
 * filenames, size validation and CRC-32 trailers. They do not assemble files,
 * interpret MIME or undo NNTP dot-stuffing. Filenames are untrusted metadata,
 * never filesystem paths. Instances are not thread-safe.
 **/

/**
 * GMimeYencOptions:
 *
 * Boxed options copied by readers and writers. Defaults are CRLF output,
 * ISO-8859-1 filenames, a 16384-byte line limit excluding the newline, and
 * a 1 GiB decoded-block limit. Readers accept CRLF and LF independently of
 * the output newline setting. Filename conversion never substitutes bytes.
 **/
struct _GMimeYencOptions {
	guint max_line_length;
	gint64 max_decoded_size;
	char *filename_charset;
	GMimeNewLineFormat newline_format;
};

/**
 * GMimeYencHeader:
 *
 * Immutable block metadata. Multipart ranges are one-based and inclusive;
 * the size is the entire file size, not the current part size. Metadata
 * alone does not imply that content has been validated.
 **/
struct _GMimeYencHeader {
	GObject parent_instance;
	char *filename;
	gint64 size, begin, end;
	guint line_length, part_number, total_parts;
};

/**
 * GMimeYencReader:
 *
 * Call g_mime_yenc_reader_read_next() followed by
 * g_mime_yenc_reader_decode_to() for each block. Decode to a #GMimeStreamNull
 * to discard a block while still validating it. Do not access the source
 * directly: the reader buffers beyond the current block.
 *
 * Output is provisional until decoding succeeds. A parsing or I/O failure
 * invalidates the reader. Stream references are retained, not explicitly
 * closed; non-seekable sources are supported.
 **/
struct _GMimeYencReader {
	GObject parent_instance;
	GMimeYencOptions *options;
	GMimeStream *stream;
	GMimeYencHeader *header;
	GByteArray *line, *output;
	guint8 input[4096];
	gsize input_index, input_length;
	gint64 decoded_size;
	guint32 checksum, expected_checksum, expected_part_checksum;
	gboolean pending, faulted, eof, validated, have_expected, have_expected_part;
};

/**
 * GMimeYencWriter:
 *
 * Writes complete blocks without buffering entire files. Each source must
 * contain exactly the declared block size at its current position; multipart
 * sources contain only the indicated range. A failed write may leave partial
 * output and invalidates the writer. Stream references are retained, not
 * explicitly closed. No MIME headers or NNTP framing are generated.
 **/
struct _GMimeYencWriter {
	GObject parent_instance;
	GMimeYencOptions *options;
	GMimeStream *stream;
	gboolean faulted;
};

G_DEFINE_BOXED_TYPE (GMimeYencOptions, g_mime_yenc_options, g_mime_yenc_options_clone, g_mime_yenc_options_free)
G_DEFINE_FINAL_TYPE (GMimeYencHeader, g_mime_yenc_header, G_TYPE_OBJECT)
G_DEFINE_FINAL_TYPE (GMimeYencReader, g_mime_yenc_reader, G_TYPE_OBJECT)
G_DEFINE_FINAL_TYPE (GMimeYencWriter, g_mime_yenc_writer, G_TYPE_OBJECT)

static gboolean
yenc_error (GError **error, const char *message)
{
	g_set_error_literal (error, GMIME_ERROR, GMIME_ERROR_PARSE_ERROR, message);
	return FALSE;
}

static gboolean
yenc_state_error (GError **error, const char *message)
{
	g_set_error_literal (error, GMIME_ERROR, GMIME_ERROR_INVALID_OPERATION, message);
	return FALSE;
}

static gboolean
yenc_io_error (GError **error, const char *operation)
{
	int saved_errno = errno ? errno : EIO;

	g_set_error (error, GMIME_ERROR, saved_errno, "Could not %s yEnc stream: %s", operation, g_strerror (saved_errno));
	return FALSE;
}

/**
 * g_mime_yenc_options_new:
 *
 * Returns: (transfer full): options with the default limits and encoding.
 **/
GMimeYencOptions *
g_mime_yenc_options_new (void)
{
	GMimeYencOptions *options = g_new (GMimeYencOptions, 1);

	options->max_line_length = 16384;
	options->max_decoded_size = G_GINT64_CONSTANT (1024) * 1024 * 1024;
	options->filename_charset = g_strdup ("iso-8859-1");
	options->newline_format = GMIME_NEWLINE_FORMAT_DOS;
	return options;
}

/**
 * g_mime_yenc_options_clone:
 * @options: (nullable): options, or %NULL for defaults
 *
 * Returns: (transfer full): an independent copy.
 **/
GMimeYencOptions *
g_mime_yenc_options_clone (GMimeYencOptions *options)
{
	GMimeYencOptions *copy;

	if (!options)
		return g_mime_yenc_options_new ();
	copy = g_new (GMimeYencOptions, 1);
	*copy = *options;
	copy->filename_charset = g_strdup (options->filename_charset);
	return copy;
}

/**
 * g_mime_yenc_options_free:
 * @options: options to free
 **/
void
g_mime_yenc_options_free (GMimeYencOptions *options)
{
	g_return_if_fail (options != NULL);

	g_free (options->filename_charset);
	g_free (options);
}

/**
 * g_mime_yenc_options_get_max_line_length:
 * @options: options
 *
 * Returns: the maximum control, payload or scanned-text line length.
 **/
guint
g_mime_yenc_options_get_max_line_length (GMimeYencOptions *options)
{
	g_return_val_if_fail (options != NULL, 0);
	return options->max_line_length;
}

/**
 * g_mime_yenc_options_set_max_line_length:
 * @options: options
 * @length: a byte count from 1 through G_MAXINT - 2, excluding the newline
 **/
void
g_mime_yenc_options_set_max_line_length (GMimeYencOptions *options, guint length)
{
	g_return_if_fail (options != NULL);
	g_return_if_fail (length >= 1 && length <= G_MAXINT - 2);
	options->max_line_length = length;
}

/**
 * g_mime_yenc_options_get_max_decoded_size:
 * @options: options
 *
 * Returns: the maximum decoded size of an individual block, not the entire multipart file.
 **/
gint64
g_mime_yenc_options_get_max_decoded_size (GMimeYencOptions *options)
{
	g_return_val_if_fail (options != NULL, 0);
	return options->max_decoded_size;
}

/**
 * g_mime_yenc_options_set_max_decoded_size:
 * @options: options
 * @size: a nonnegative block size limit; zero permits only empty blocks
 **/
void
g_mime_yenc_options_set_max_decoded_size (GMimeYencOptions *options, gint64 size)
{
	g_return_if_fail (options != NULL);
	g_return_if_fail (size >= 0);
	options->max_decoded_size = size;
}

/**
 * g_mime_yenc_options_get_filename_charset:
 * @options: options
 *
 * Returns: (transfer none): the charset used to convert filenames to and from UTF-8.
 **/
const char *
g_mime_yenc_options_get_filename_charset (GMimeYencOptions *options)
{
	g_return_val_if_fail (options != NULL, NULL);
	return options->filename_charset;
}

/**
 * g_mime_yenc_options_set_filename_charset:
 * @options: options
 * @charset: a supported charset name
 * @error: return location for an error
 *
 * The format does not declare a charset. The default ISO-8859-1 preserves
 * every filename byte. Conversion errors are reported without replacement.
 *
 * Returns: whether the charset was accepted, without changing it on failure.
 **/
gboolean
g_mime_yenc_options_set_filename_charset (GMimeYencOptions *options, const char *charset, GError **error)
{
	iconv_t to, from;
	char *copy;

	g_return_val_if_fail (options != NULL, FALSE);
	g_return_val_if_fail (charset != NULL, FALSE);
	g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

	if (!*charset || strstr (charset, "//")) {
		g_set_error_literal (error, GMIME_ERROR, GMIME_ERROR_NOT_SUPPORTED, "A yEnc filename charset must use strict conversion.");
		return FALSE;
	}

	to = iconv_open ("UTF-8", g_mime_charset_iconv_name (charset));
	from = iconv_open (g_mime_charset_iconv_name (charset), "UTF-8");

	if (to != (iconv_t) -1)
		g_mime_iconv_close (to);

	if (from != (iconv_t) -1)
		g_mime_iconv_close (from);

	if (to == (iconv_t) -1 || from == (iconv_t) -1) {
		g_set_error (error, GMIME_ERROR, GMIME_ERROR_NOT_SUPPORTED, "Unsupported yEnc filename charset: %s", charset);
		return FALSE;
	}

	copy = g_strdup (charset);
	g_free (options->filename_charset);
	options->filename_charset = copy;

	return TRUE;
}

/**
 * g_mime_yenc_options_get_newline_format:
 * @options: options
 *
 * Returns: the writer's newline format.
 **/
GMimeNewLineFormat
g_mime_yenc_options_get_newline_format (GMimeYencOptions *options)
{
	g_return_val_if_fail (options != NULL, GMIME_NEWLINE_FORMAT_DOS);
	return options->newline_format;
}

/**
 * g_mime_yenc_options_set_newline_format:
 * @options: options
 * @format: DOS (CRLF) or Unix (LF)
 **/
void
g_mime_yenc_options_set_newline_format (GMimeYencOptions *options, GMimeNewLineFormat format)
{
	g_return_if_fail (options != NULL);
	g_return_if_fail (format == GMIME_NEWLINE_FORMAT_DOS || format == GMIME_NEWLINE_FORMAT_UNIX);
	options->newline_format = format;
}

static void
g_mime_yenc_header_finalize (GObject *object)
{
	g_free (GMIME_YENC_HEADER (object)->filename);
	G_OBJECT_CLASS (g_mime_yenc_header_parent_class)->finalize (object);
}

static void
g_mime_yenc_header_class_init (GMimeYencHeaderClass *klass)
{
	G_OBJECT_CLASS (klass)->finalize = g_mime_yenc_header_finalize;
}

static void
g_mime_yenc_header_init (GMimeYencHeader *header)
{
}

/**
 * g_mime_yenc_header_new:
 * @filename: a nonempty UTF-8 filename without CR, LF or surrounding ASCII spaces
 * @size: the entire file size, nonnegative
 * @line_length: the nominal payload line length, normally 128; writers support 60 through 998
 * @part_number: the part number, or zero for a single-part block
 * @total_parts: the total part count, or zero if unspecified
 * @begin: the one-based inclusive range start, or zero for a single-part block
 * @end: the one-based inclusive range end, or zero for a single-part block
 * @error: return location for an error
 *
 * Multipart ranges must fall within the file size. A total part count, when
 * specified, must include the part number. Filenames are metadata, not paths.
 *
 * Returns: (transfer full) (nullable): immutable metadata, or %NULL if invalid.
 **/
GMimeYencHeader *
g_mime_yenc_header_new (const char *filename, gint64 size, guint line_length,
			guint part_number, guint total_parts, gint64 begin, gint64 end, GError **error)
{
	GMimeYencHeader *header;
	gsize length;

	g_return_val_if_fail (filename != NULL, NULL);
	g_return_val_if_fail (error == NULL || *error == NULL, NULL);

	length = strlen (filename);

	if (!length || !g_utf8_validate (filename, -1, NULL) || filename[0] == ' ' || filename[length - 1] == ' ' ||
	    strpbrk (filename, "\r\n") || size < 0 || line_length < 1 || line_length > G_MAXINT - 2 ||
	    part_number > G_MAXINT || total_parts > G_MAXINT || (!part_number && total_parts) ||
	    (total_parts && part_number > total_parts) ||
	    (!part_number ? begin != 0 || end != 0 : begin < 1 || begin > size || end < begin || end > size)) {
		yenc_error (error, "Invalid yEnc filename, size, line length or multipart range.");
		return NULL;
	}

	header = g_object_new (GMIME_TYPE_YENC_HEADER, NULL);
	header->filename = g_strdup (filename);
	header->size = size;
	header->line_length = line_length;
	header->part_number = part_number;
	header->total_parts = total_parts;
	header->begin = begin;
	header->end = end;

	return header;
}

/**
 * g_mime_yenc_header_get_filename:
 * @header: block metadata
 *
 * Returns: (transfer none): the untrusted UTF-8 filename.
 **/
const char *
g_mime_yenc_header_get_filename (GMimeYencHeader *header)
{
	g_return_val_if_fail (GMIME_IS_YENC_HEADER (header), NULL);
	return header->filename;
}

/**
 * g_mime_yenc_header_get_size:
 * @header: block metadata
 *
 * Returns: the entire file size.
 **/
gint64
g_mime_yenc_header_get_size (GMimeYencHeader *header)
{
	g_return_val_if_fail (GMIME_IS_YENC_HEADER (header), 0);
	return header->size;
}

/**
 * g_mime_yenc_header_get_line_length:
 * @header: block metadata
 *
 * Returns: the nominal payload line length, excluding the newline.
 **/
guint
g_mime_yenc_header_get_line_length (GMimeYencHeader *header)
{
	g_return_val_if_fail (GMIME_IS_YENC_HEADER (header), 0);
	return header->line_length;
}

/**
 * g_mime_yenc_header_get_part_number:
 * @header: block metadata
 *
 * Returns: the part number, or zero for single-part blocks.
 **/
guint
g_mime_yenc_header_get_part_number (GMimeYencHeader *header)
{
	g_return_val_if_fail (GMIME_IS_YENC_HEADER (header), 0);
	return header->part_number;
}

/**
 * g_mime_yenc_header_get_total_parts:
 * @header: block metadata
 *
 * Returns: the total part count, or zero if unspecified.
 **/
guint
g_mime_yenc_header_get_total_parts (GMimeYencHeader *header)
{
	g_return_val_if_fail (GMIME_IS_YENC_HEADER (header), 0);
	return header->total_parts;
}

/**
 * g_mime_yenc_header_get_begin:
 * @header: block metadata
 *
 * Returns: the one-based inclusive range start, or zero for single-part blocks.
 **/
gint64
g_mime_yenc_header_get_begin (GMimeYencHeader *header)
{
	g_return_val_if_fail (GMIME_IS_YENC_HEADER (header), 0);
	return header->begin;
}

/**
 * g_mime_yenc_header_get_end:
 * @header: block metadata
 *
 * Returns: the one-based inclusive range end, or zero for single-part blocks.
 **/
gint64
g_mime_yenc_header_get_end (GMimeYencHeader *header)
{
	g_return_val_if_fail (GMIME_IS_YENC_HEADER (header), 0);
	return header->end;
}

/**
 * g_mime_yenc_header_get_decoded_size:
 * @header: block metadata
 *
 * Returns: the decoded size of this block, derived from the multipart range when applicable.
 **/
gint64
g_mime_yenc_header_get_decoded_size (GMimeYencHeader *header)
{
	g_return_val_if_fail (GMIME_IS_YENC_HEADER (header), 0);
	return header->part_number ? header->end - header->begin + 1 : header->size;
}

static GHashTable *
yenc_parse_fields (GByteArray *line, gsize start, gboolean is_header, GError **error)
{
	GHashTable *fields = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, g_free);
	gsize end, equals, value_start, value_end;
	char *key, *value;
	gboolean filename;

	if (memchr (line->data, 0, line->len))
		goto invalid;

	while (start < line->len) {
		if (line->data[start] == ' ') {
			start++;
			continue;
		}

		for (end = start; end < line->len && line->data[end] != ' '; end++)
			;

		for (equals = start; equals < end && line->data[equals] != '='; equals++)
			;

		if (equals == start || equals == end)
			goto invalid;

		key = g_strndup ((char *) line->data + start, equals - start);
		filename = is_header && !strcmp (key, "name");
		value_start = equals + 1;
		value_end = filename ? line->len : end;

		if (filename) {
			while (value_start < value_end && line->data[value_start] == ' ')
				value_start++;

			while (value_end > value_start && line->data[value_end - 1] == ' ')
				value_end--;
		}

		if (value_start == value_end || g_hash_table_contains (fields, key)) {
			g_free (key);
			goto invalid;
		}

		value = g_strndup ((char *) line->data + value_start, value_end - value_start);
		g_hash_table_insert (fields, key, value);

		if (filename)
			break;

		start = end;
	}

	return fields;

invalid:
	yenc_error (error, "Invalid, empty or duplicate yEnc control field.");
	g_hash_table_unref (fields);

	return NULL;
}

static gboolean
yenc_number (GHashTable *fields, const char *name, gboolean required, gint64 maximum, gint64 *number, GError **error)
{
	const char *value = g_hash_table_lookup (fields, name);
	guint digit;

	*number = 0;

	if (!value && !required)
		return TRUE;

	if (value) {
		while (*value) {
			if (*value < '0' || *value > '9')
				break;

			digit = *value++ - '0';
			if (*number > (maximum - digit) / 10)
				goto invalid;

			*number = *number * 10 + digit;
		}

		if (!*value && *number <= maximum)
			return TRUE;
	}

invalid:
	g_set_error (error, GMIME_ERROR, GMIME_ERROR_PARSE_ERROR, "Missing or invalid yEnc %s field.", name);
	return FALSE;
}

static gboolean
yenc_checksum (GHashTable *fields, const char *name, gboolean *present, guint32 *checksum, GError **error)
{
	const char *value = g_hash_table_lookup (fields, name);
	guint i;
	int digit;

	*present = value != NULL;
	*checksum = 0;

	if (!value)
		return TRUE;

	if (strlen (value) == 8) {
		for (i = 0; i < 8; i++) {
			if ((digit = g_ascii_xdigit_value (value[i])) == -1)
				break;

			*checksum = (*checksum << 4) | digit;
		}

		if (i == 8)
			return TRUE;
	}

	g_set_error (error, GMIME_ERROR, GMIME_ERROR_PARSE_ERROR, "Invalid yEnc %s checksum.", name);
	return FALSE;
}

static void
g_mime_yenc_reader_finalize (GObject *object)
{
	GMimeYencReader *reader = GMIME_YENC_READER (object);

	g_clear_object (&reader->stream);
	g_clear_object (&reader->header);
	g_mime_yenc_options_free (reader->options);
	g_byte_array_unref (reader->line);
	g_byte_array_unref (reader->output);
	G_OBJECT_CLASS (g_mime_yenc_reader_parent_class)->finalize (object);
}

static void
g_mime_yenc_reader_class_init (GMimeYencReaderClass *klass)
{
	G_OBJECT_CLASS (klass)->finalize = g_mime_yenc_reader_finalize;
}

static void
g_mime_yenc_reader_init (GMimeYencReader *reader)
{
	reader->options = g_mime_yenc_options_new ();
	reader->line = g_byte_array_new ();
	reader->output = g_byte_array_new ();
}

/**
 * g_mime_yenc_reader_new:
 * @stream: the source stream, not necessarily seekable
 * @options: (nullable): options, or %NULL for defaults
 *
 * Takes an independent snapshot of the options and retains a stream reference.
 *
 * Returns: (transfer full): a streaming yEnc reader.
 **/
GMimeYencReader *
g_mime_yenc_reader_new (GMimeStream *stream, GMimeYencOptions *options)
{
	GMimeYencReader *reader;

	g_return_val_if_fail (GMIME_IS_STREAM (stream), NULL);
	reader = g_object_new (GMIME_TYPE_YENC_READER, NULL);
	reader->stream = g_object_ref (stream);

	if (options) {
		g_mime_yenc_options_free (reader->options);
		reader->options = g_mime_yenc_options_clone (options);
	}

	return reader;
}

static gboolean
yenc_marker (GMimeYencReader *reader, const char *marker)
{
	gsize length = strlen (marker);

	return reader->line->len >= length && !memcmp (reader->line->data, marker, length);
}

static int
yenc_read_line (GMimeYencReader *reader, GError **error)
{
	ssize_t n;
	guint8 c;

	g_byte_array_set_size (reader->line, 0);

	while (TRUE) {
		if (reader->input_index == reader->input_length) {
			if (reader->eof)
				break;

			if ((n = g_mime_stream_read (reader->stream, (char *) reader->input, sizeof reader->input)) < 0) {
				yenc_io_error (error, "read");
				return -1;
			}

			reader->input_length = n;
			reader->input_index = 0;
			reader->eof = n == 0;
			continue;
		}

		c = reader->input[reader->input_index++];

		if (c == '\n')
			break;

		if ((reader->line->len == reader->options->max_line_length && c != '\r') ||
		    reader->line->len > reader->options->max_line_length) {
			yenc_error (error, "The yEnc input exceeds the maximum line length.");
			return -1;
		}

		g_byte_array_append (reader->line, &c, 1);
	}

	if (reader->line->len && reader->line->data[reader->line->len - 1] == '\r')
		g_byte_array_set_size (reader->line, reader->line->len - 1);

	return !reader->eof || reader->line->len > 0;
}

static gboolean
yenc_set_header (GMimeYencReader *reader, GHashTable *fields, GHashTable *part, GError **error)
{
	const char *raw_name = g_hash_table_lookup (fields, "name");
	gint64 size, length, number, total, begin = 0, end = 0;
	gsize filename_length;
	char *filename;

	if (!raw_name)
		return yenc_error (error, "Missing yEnc filename.");

	if (!yenc_number (fields, "size", TRUE, G_MAXINT64, &size, error) ||
	    !yenc_number (fields, "line", TRUE, G_MAXINT, &length, error) ||
	    !yenc_number (fields, "part", FALSE, G_MAXINT, &number, error) ||
	    !yenc_number (fields, "total", FALSE, G_MAXINT, &total, error) ||
	    (part && (!yenc_number (part, "begin", TRUE, G_MAXINT64, &begin, error) ||
		      !yenc_number (part, "end", TRUE, G_MAXINT64, &end, error))))
		return FALSE;

	if (part && !number)
		return yenc_error (error, "Invalid yEnc part number.");

	if (!(filename = _g_mime_iconv_convert (raw_name, -1, "UTF-8", reader->options->filename_charset, &filename_length, error)))
		return FALSE;

	if (memchr (filename, 0, filename_length)) {
		g_free (filename);
		return yenc_error (error, "The decoded yEnc filename contains NUL.");
	}

	reader->header = g_mime_yenc_header_new (filename, size, length, number, total, begin, end, error);
	g_free (filename);

	if (!reader->header)
		return FALSE;

	if (g_mime_yenc_header_get_decoded_size (reader->header) > reader->options->max_decoded_size ||
	    length > reader->options->max_line_length)
		return yenc_error (error, "The yEnc header exceeds the configured limits.");

	reader->pending = TRUE;

	return TRUE;
}

/**
 * g_mime_yenc_reader_read_next:
 * @reader: a yEnc reader
 * @error: return location for an error
 *
 * Skips surrounding text, respecting the line limit even outside blocks.
 * Decode the current block before calling again. Clean EOF is not an error.
 *
 * Returns: %TRUE if a block was found; %FALSE at EOF or on error.
 **/
gboolean
g_mime_yenc_reader_read_next (GMimeYencReader *reader, GError **error)
{
	GHashTable *fields = NULL, *part = NULL;
	gboolean success = FALSE;
	int result;

	g_return_val_if_fail (GMIME_IS_YENC_READER (reader), FALSE);
	g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

	if (reader->faulted)
		return yenc_state_error (error, "The yEnc reader cannot be reused after a failed operation.");

	if (reader->pending)
		return yenc_state_error (error, "Decode the current yEnc block before reading another.");

	g_clear_object (&reader->header);
	reader->decoded_size = 0;
	reader->validated = reader->have_expected = reader->have_expected_part = FALSE;

	while ((result = yenc_read_line (reader, error)) > 0) {
		if (!yenc_marker (reader, "=ybegin "))
			continue;

		if (!(fields = yenc_parse_fields (reader->line, 8, TRUE, error)))
			goto done;

		if (g_hash_table_contains (fields, "part")) {
			if ((result = yenc_read_line (reader, error)) < 0)
				goto done;

			if (!result || !yenc_marker (reader, "=ypart ")) {
				yenc_error (error, "Missing yEnc part header.");
				goto done;
			}

			if (!(part = yenc_parse_fields (reader->line, 7, FALSE, error)))
				goto done;
		}

		success = yenc_set_header (reader, fields, part, error);
		goto done;
	}

	if (result == 0)
		return FALSE;

done:
	g_clear_pointer (&fields, g_hash_table_unref);
	g_clear_pointer (&part, g_hash_table_unref);
	reader->faulted = !success;

	return success;
}

static gboolean
yenc_validate_trailer (GMimeYencReader *reader, GMimeYencDecoder *decoder, GError **error)
{
	GHashTable *fields = yenc_parse_fields (reader->line, 6, FALSE, error);
	GMimeYencHeader *header = reader->header;
	gint64 size, part, total;
	guint32 actual = ~g_mime_yenc_decoder_get_checksum (decoder), expected, expected_part;
	gboolean have_expected, have_part, success = FALSE;
	gboolean whole_file = !header->part_number || (header->begin == 1 && header->end == header->size);

	if (!fields)
		return FALSE;

	if (!yenc_number (fields, "size", TRUE, G_MAXINT64, &size, error))
		goto done;

	if (size != reader->decoded_size || size != g_mime_yenc_header_get_decoded_size (header)) {
		yenc_error (error, "The yEnc decoded size does not match its header or trailer.");
		goto done;
	}

	if (header->part_number || g_hash_table_contains (fields, "part")) {
		if (!yenc_number (fields, "part", TRUE, G_MAXINT, &part, error))
			goto done;

		if (part != header->part_number) {
			yenc_error (error, "The yEnc trailer part number does not match its header.");
			goto done;
		}
	}

	if (g_hash_table_contains (fields, "total")) {
		if (!yenc_number (fields, "total", TRUE, G_MAXINT, &total, error))
			goto done;

		if (total != header->total_parts) {
			yenc_error (error, "The yEnc trailer part count does not match its header.");
			goto done;
		}
	}

	if (!yenc_checksum (fields, "crc32", &have_expected, &expected, error) ||
	    !yenc_checksum (fields, "pcrc32", &have_part, &expected_part, error))
		goto done;

	if ((have_part && expected_part != actual) || (whole_file && have_expected && expected != actual)) {
		yenc_error (error, "The yEnc checksum does not match the decoded content.");
		goto done;
	}

	reader->checksum = actual;
	reader->expected_checksum = expected;
	reader->expected_part_checksum = expected_part;
	reader->have_expected = have_expected;
	reader->have_expected_part = have_part;
	reader->validated = TRUE;
	reader->pending = FALSE;
	success = TRUE;

done:
	g_hash_table_unref (fields);

	return success;
}

static gboolean
yenc_decode_line (GMimeYencReader *reader, GMimeYencDecoder *decoder, GError **error)
{
	gsize i, count, length = reader->line->len;
	guint8 *data = reader->line->data;
	gboolean final_escape = FALSE;

	if (yenc_marker (reader, "=y"))
		return yenc_error (error, "Unexpected yEnc control line.");

	if (length > reader->header->line_length + 1)
		return yenc_error (error, "The yEnc payload exceeds its declared line length.");

	for (i = 0; i < length; i++) {
		if (data[i] == 0 || data[i] == '\r')
			return yenc_error (error, "Unescaped critical byte in yEnc payload.");

		if (data[i] == '=') {
			final_escape = i + 2 == length;

			if (++i == length || data[i] == 0 || data[i] == '\r')
				return yenc_error (error, "Invalid or incomplete yEnc escape.");
		}
	}

	if (length > reader->header->line_length && !final_escape)
		return yenc_error (error, "The yEnc payload exceeds its declared line length.");

	g_byte_array_append (reader->line, (const guint8 *) "\n", 1);
	g_byte_array_set_size (reader->output, length + 1);

	count = g_mime_decoder_decode (GMIME_DECODER (decoder), reader->line->data, length + 1,
				      reader->output->data, reader->output->len);

	if (count > (guint64) (g_mime_yenc_header_get_decoded_size (reader->header) - reader->decoded_size))
		return yenc_error (error, "The yEnc payload exceeds its declared size.");

	reader->decoded_size += count;
	g_byte_array_set_size (reader->output, count);

	return TRUE;
}

/**
 * g_mime_yenc_reader_decode_to:
 * @reader: a yEnc reader with a pending block
 * @destination: a stream distinct from the source
 * @error: return location for an error
 *
 * Leaves the destination open and unflushed. Discard its output on failure.
 * Missing checksums are accepted; present block checksums are verified.
 * Whole-file checksums of partial ranges are unverified metadata.
 *
 * Returns: whether the block was decoded and validated successfully.
 **/
gboolean
g_mime_yenc_reader_decode_to (GMimeYencReader *reader, GMimeStream *destination, GError **error)
{
	GMimeYencDecoder *decoder;
	gboolean success = FALSE;
	int result;

	g_return_val_if_fail (GMIME_IS_YENC_READER (reader), FALSE);
	g_return_val_if_fail (GMIME_IS_STREAM (destination), FALSE);
	g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

	if (destination == reader->stream)
		return yenc_state_error (error, "The yEnc destination must differ from its source.");

	if (reader->faulted)
		return yenc_state_error (error, "The yEnc reader cannot be reused after a failed operation.");

	if (!reader->pending)
		return yenc_state_error (error, "Read a yEnc header before decoding a block.");

	decoder = g_mime_yenc_decoder_new (TRUE);

	while ((result = yenc_read_line (reader, error)) > 0) {
		if (yenc_marker (reader, "=yend ")) {
			success = yenc_validate_trailer (reader, decoder, error);
			goto done;
		}

		if (!yenc_decode_line (reader, decoder, error))
			goto done;

		if (reader->output->len &&
		    _g_mime_stream_write_all (destination, (char *) reader->output->data, reader->output->len) < 0) {
			yenc_io_error (error, "write");
			goto done;
		}
	}

	if (result == 0)
		yenc_error (error, "Missing yEnc trailer.");

done:
	g_object_unref (decoder);
	reader->faulted = !success;

	return success;
}

/**
 * g_mime_yenc_reader_get_header:
 * @reader: a yEnc reader
 *
 * Returns: (transfer none) (nullable): the current header, or %NULL before reading or after clean EOF.
 **/
GMimeYencHeader *
g_mime_yenc_reader_get_header (GMimeYencReader *reader)
{
	g_return_val_if_fail (GMIME_IS_YENC_READER (reader), NULL);
	return reader->header;
}

/**
 * g_mime_yenc_reader_get_decoded_size:
 * @reader: a yEnc reader
 *
 * Returns: the decoded byte count, which does not imply successful validation.
 **/
gint64
g_mime_yenc_reader_get_decoded_size (GMimeYencReader *reader)
{
	g_return_val_if_fail (GMIME_IS_YENC_READER (reader), 0);
	return reader->decoded_size;
}

/**
 * g_mime_yenc_reader_get_checksum:
 * @reader: a yEnc reader
 * @checksum: (out): the computed finalized block CRC-32, or zero if unavailable
 *
 * Returns: whether decoding and validation completed successfully.
 **/
gboolean
g_mime_yenc_reader_get_checksum (GMimeYencReader *reader, guint32 *checksum)
{
	g_return_val_if_fail (GMIME_IS_YENC_READER (reader), FALSE);
	g_return_val_if_fail (checksum != NULL, FALSE);

	*checksum = reader->validated ? reader->checksum : 0;

	return reader->validated;
}

/**
 * g_mime_yenc_reader_get_expected_checksum:
 * @reader: a yEnc reader
 * @checksum: (out): the advertised whole-file CRC-32, or zero if unavailable
 *
 * For partial ranges this checksum is unverified metadata.
 *
 * Returns: whether a whole-file checksum was present after successful decoding.
 **/
gboolean
g_mime_yenc_reader_get_expected_checksum (GMimeYencReader *reader, guint32 *checksum)
{
	g_return_val_if_fail (GMIME_IS_YENC_READER (reader), FALSE);
	g_return_val_if_fail (checksum != NULL, FALSE);

	*checksum = reader->validated && reader->have_expected ? reader->expected_checksum : 0;

	return reader->validated && reader->have_expected;
}

/**
 * g_mime_yenc_reader_get_expected_part_checksum:
 * @reader: a yEnc reader
 * @checksum: (out): the advertised part CRC-32, or zero if unavailable
 *
 * Returns: whether a part checksum was present and verified after successful decoding.
 **/
gboolean
g_mime_yenc_reader_get_expected_part_checksum (GMimeYencReader *reader, guint32 *checksum)
{
	g_return_val_if_fail (GMIME_IS_YENC_READER (reader), FALSE);
	g_return_val_if_fail (checksum != NULL, FALSE);

	*checksum = reader->validated && reader->have_expected_part ? reader->expected_part_checksum : 0;

	return reader->validated && reader->have_expected_part;
}

static void
g_mime_yenc_writer_finalize (GObject *object)
{
	GMimeYencWriter *writer = GMIME_YENC_WRITER (object);

	g_clear_object (&writer->stream);
	g_mime_yenc_options_free (writer->options);

	G_OBJECT_CLASS (g_mime_yenc_writer_parent_class)->finalize (object);
}

static void
g_mime_yenc_writer_class_init (GMimeYencWriterClass *klass)
{
	G_OBJECT_CLASS (klass)->finalize = g_mime_yenc_writer_finalize;
}

static void
g_mime_yenc_writer_init (GMimeYencWriter *writer)
{
	writer->options = g_mime_yenc_options_new ();
}

/**
 * g_mime_yenc_writer_new:
 * @stream: the destination, not necessarily seekable
 * @options: (nullable): options, or %NULL for defaults
 *
 * Takes an independent snapshot of the options and retains a stream reference.
 *
 * Returns: (transfer full): a streaming yEnc writer.
 **/
GMimeYencWriter *
g_mime_yenc_writer_new (GMimeStream *stream, GMimeYencOptions *options)
{
	GMimeYencWriter *writer;

	g_return_val_if_fail (GMIME_IS_STREAM (stream), NULL);

	writer = g_object_new (GMIME_TYPE_YENC_WRITER, NULL);
	writer->stream = g_object_ref (stream);

	if (options) {
		g_mime_yenc_options_free (writer->options);
		writer->options = g_mime_yenc_options_clone (options);
	}

	return writer;
}

static GString *
yenc_trailer (GMimeYencHeader *header, guint32 checksum, const guint32 *file_checksum)
{
	GString *text = g_string_new (NULL);

	g_string_append_printf (text, "=yend size=%" G_GINT64_FORMAT, g_mime_yenc_header_get_decoded_size (header));

	if (header->part_number) {
		g_string_append_printf (text, " part=%u", header->part_number);

		if (header->total_parts)
			g_string_append_printf (text, " total=%u", header->total_parts);

		g_string_append_printf (text, " pcrc32=%08x", checksum);

		if (file_checksum)
			g_string_append_printf (text, " crc32=%08x", *file_checksum);
	} else {
		g_string_append_printf (text, " crc32=%08x", checksum);
	}

	return text;
}

static GString *
yenc_create_header (GMimeYencWriter *writer, GMimeYencHeader *header, const guint32 *file_checksum, GError **error)
{
	const char *newline = writer->options->newline_format == GMIME_NEWLINE_FORMAT_DOS ? "\r\n" : "\n";
	GString *text = NULL, *part = NULL, *trailer;
	gboolean success = FALSE;
	gsize name_length;
	char *name = NULL;

	if (header->line_length < 60 || header->line_length > 998 ||
	    header->line_length + 1 > writer->options->max_line_length ||
	    g_mime_yenc_header_get_decoded_size (header) > writer->options->max_decoded_size) {
		yenc_error (error, "The yEnc block size or line length is outside the writer's limits.");
		return NULL;
	}

	trailer = yenc_trailer (header, 0, file_checksum);

	if (trailer->len > writer->options->max_line_length) {
		g_string_free (trailer, TRUE);
		yenc_error (error, "The yEnc trailer exceeds the configured maximum line length.");
		return NULL;
	}

	g_string_free (trailer, TRUE);
	text = g_string_new ("=ybegin ");

	if (header->part_number) {
		g_string_append_printf (text, "part=%u ", header->part_number);

		if (header->total_parts)
			g_string_append_printf (text, "total=%u ", header->total_parts);
	}

	g_string_append_printf (text, "line=%u size=%" G_GINT64_FORMAT " name=", header->line_length, header->size);

	if (!(name = _g_mime_iconv_convert (header->filename, -1, writer->options->filename_charset, "UTF-8", &name_length, error)))
		goto done;

	if (name_length > writer->options->max_line_length ||
	    text->len > writer->options->max_line_length - name_length ||
	    memchr (name, 0, name_length) || memchr (name, '\r', name_length) || memchr (name, '\n', name_length)) {
		yenc_error (error, "The encoded yEnc filename is invalid or its header is too long.");
		goto done;
	}

	g_string_append_len (text, name, name_length);
	g_string_append (text, newline);

	if (header->part_number) {
		part = g_string_new (NULL);
		g_string_append_printf (part, "=ypart begin=%" G_GINT64_FORMAT " end=%" G_GINT64_FORMAT, header->begin, header->end);

		if (part->len > writer->options->max_line_length || text->len + part->len + strlen (newline) > G_MAXINT) {
			yenc_error (error, "The yEnc part header exceeds the configured limits.");
			goto done;
		}

		g_string_append_len (text, part->str, part->len);
		g_string_append (text, newline);
	}

	success = TRUE;

done:
	g_free (name);

	if (part)
		g_string_free (part, TRUE);

	if (!success) {
		g_string_free (text, TRUE);
		text = NULL;
	}

	return text;
}

static gboolean
yenc_write (GMimeStream *stream, const void *data, gsize length, GError **error)
{
	if (_g_mime_stream_write_all (stream, data, length) < 0)
		return yenc_io_error (error, "write");

	return TRUE;
}

/**
 * g_mime_yenc_writer_write:
 * @writer: a yEnc writer
 * @header: the block metadata
 * @source: exactly this block's decoded bytes, distinct from the destination
 * @file_checksum: (in) (nullable): an optional finalized whole-file CRC-32
 * @checksum: (out) (optional): the finalized block CRC-32, or zero on failure
 * @error: return location for an error
 *
 * Writes framing, payload and a trailer. Leaves the source open and does not
 * flush the destination. A partial range's supplied whole-file checksum is
 * emitted without verification. For a whole-file block it must match.
 * Validation before output does not invalidate the writer; size, checksum or
 * I/O failures once writing begins do invalidate it.
 *
 * Returns: whether the complete block was written successfully.
 **/
gboolean
g_mime_yenc_writer_write (GMimeYencWriter *writer, GMimeYencHeader *header, GMimeStream *source,
			  const guint32 *file_checksum, guint32 *checksum, GError **error)
{
	guint32 actual, expected_file_checksum;
	gsize capacity, count, length;
	guint8 input[4096], *output;
	GMimeYencEncoder *encoder;
	gboolean success = FALSE;
	GMimeEncoder *codec;
	gint64 remaining;
	GString *text;
	ssize_t n;

	g_return_val_if_fail (GMIME_IS_YENC_WRITER (writer), FALSE);
	g_return_val_if_fail (GMIME_IS_YENC_HEADER (header), FALSE);
	g_return_val_if_fail (GMIME_IS_STREAM (source), FALSE);
	g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

	if (file_checksum) {
		expected_file_checksum = *file_checksum;
		file_checksum = &expected_file_checksum;
	}

	if (checksum)
		*checksum = 0;

	if (writer->faulted)
		return yenc_state_error (error, "The yEnc writer cannot be reused after a failed write.");

	if (source == writer->stream)
		return yenc_state_error (error, "The yEnc source must differ from its destination.");

	if (!(text = yenc_create_header (writer, header, file_checksum, error)))
		return FALSE;

	writer->faulted = TRUE;
	encoder = g_mime_yenc_encoder_new_with_newline_format (writer->options->newline_format, header->line_length);
	codec = GMIME_ENCODER (encoder);
	capacity = g_mime_encoder_estimate_output_length (codec, sizeof input);
	output = g_malloc (capacity);
	remaining = g_mime_yenc_header_get_decoded_size (header);

	if (!yenc_write (writer->stream, text->str, text->len, error))
		goto done;

	while (TRUE) {
		count = remaining >= sizeof input ? sizeof input : (gsize) remaining + 1;

		if ((n = g_mime_stream_read (source, (char *) input, count)) < 0) {
			yenc_io_error (error, "read");
			goto done;
		}

		if (n == 0) {
			if (remaining != 0) {
				yenc_error (error, "The yEnc source is shorter than its declared size.");
				goto done;
			}

			break;
		}

		if (n > remaining) {
			yenc_error (error, "The yEnc source is longer than its declared size.");
			goto done;
		}

		remaining -= n;
		length = g_mime_encoder_encode (codec, input, n, output, capacity);

		if (!yenc_write (writer->stream, output, length, error))
			goto done;
	}

	length = g_mime_encoder_flush (codec, NULL, 0, output, capacity);

	if (!yenc_write (writer->stream, output, length, error))
		goto done;

	actual = ~g_mime_yenc_encoder_get_checksum (encoder);

	if (file_checksum && (!header->part_number || (header->begin == 1 && header->end == header->size)) &&
	    *file_checksum != actual) {
		yenc_error (error, "The supplied whole-file yEnc checksum does not match the source.");
		goto done;
	}

	g_string_free (text, TRUE);
	text = yenc_trailer (header, actual, file_checksum);
	g_string_append (text, writer->options->newline_format == GMIME_NEWLINE_FORMAT_DOS ? "\r\n" : "\n");

	if (!yenc_write (writer->stream, text->str, text->len, error))
		goto done;

	if (checksum)
		*checksum = actual;

	success = TRUE;

done:
	g_string_free (text, TRUE);
	g_free (output);
	g_object_unref (encoder);
	writer->faulted = !success;

	return success;
}
