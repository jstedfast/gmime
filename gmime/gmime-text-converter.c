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
#include <string.h>

#include "gmime-text-converter.h"
#include "gmime-error.h"
#include "gmime-iconv.h"
#include "gmime-internal.h"
#include "gmime-stream-mem.h"
#include "gmime-text-private.h"
#include "gmime-reader-simd-private.h"

/**
 * SECTION: gmime-text-converter
 * @title: Text converters
 * @short_description: Streaming plain and flowed text conversion
 *
 * String APIs accept and produce UTF-8. Stream APIs use the selected input
 * and output charsets, defaulting to UTF-8. Invalid or unrepresentable text is
 * an error, not silently replaced. BOM detection and emission are opt-in.
 *
 * Plain text copying preserves newlines. Flowed conversion recognizes CRLF,
 * LF and bare CR as input line endings and writes LF, independently of the
 * host platform. Wrapping counts Unicode scalar values rather than UTF-16
 * code units; it never splits a UTF-8 character.
 *
 * Converters may be reused after success or failure, but must not be used
 * concurrently or reentrantly. Conversion reads from the current input
 * position and leaves both streams open. On failure, stream output may be
 * partial. The string convenience API returns no partial result.
 **/

typedef struct {
	char *header;
	char *footer;
	char *input_charset;
	char *output_charset;
	gsize input_buffer_size, output_buffer_size;
	gboolean detect_bom, write_bom;
	gboolean active;
} GMimeTextConverterPrivate;

struct _GMimeTextToText {
	GMimeTextConverter parent_object;
};

struct _GMimeTextToFlowed {
	GMimeTextConverter parent_object;
};

struct _GMimeFlowedToText {
	GMimeTextConverter parent_object;
	gboolean delete_space;
};

G_DEFINE_ABSTRACT_TYPE_WITH_PRIVATE (GMimeTextConverter, g_mime_text_converter, G_TYPE_OBJECT)
G_DEFINE_FINAL_TYPE (GMimeTextToText, g_mime_text_to_text, GMIME_TYPE_TEXT_CONVERTER)
G_DEFINE_FINAL_TYPE (GMimeTextToFlowed, g_mime_text_to_flowed, GMIME_TYPE_TEXT_CONVERTER)
G_DEFINE_FINAL_TYPE (GMimeFlowedToText, g_mime_flowed_to_text, GMIME_TYPE_TEXT_CONVERTER)

static gboolean
text_io_error (GError **error, int code, const char *operation)
{
	g_set_error (error, GMIME_ERROR, code ? code : GMIME_ERROR_GENERAL, "Text conversion %s failed: %s", operation,
		     code ? g_strerror (code) : "stream did not report an error");
	return FALSE;
}

static void
text_converter_finalize (GObject *object)
{
	GMimeTextConverterPrivate *priv = g_mime_text_converter_get_instance_private (GMIME_TEXT_CONVERTER (object));

	g_free (priv->header);
	g_free (priv->footer);
	g_free (priv->input_charset);
	g_free (priv->output_charset);

	G_OBJECT_CLASS (g_mime_text_converter_parent_class)->finalize (object);
}

static void
g_mime_text_converter_class_init (GMimeTextConverterClass *klass)
{
	G_OBJECT_CLASS (klass)->finalize = text_converter_finalize;
}

static void
g_mime_text_converter_init (GMimeTextConverter *converter)
{
	GMimeTextConverterPrivate *priv = g_mime_text_converter_get_instance_private (converter);

	priv->input_charset = g_strdup ("utf-8");
	priv->output_charset = g_strdup ("utf-8");
	priv->input_buffer_size = priv->output_buffer_size = 4096;
}

gboolean
_g_mime_text_converter_is_active (GMimeTextConverter *converter)
{
	GMimeTextConverterPrivate *priv = g_mime_text_converter_get_instance_private (converter);

	return priv->active;
}

static gboolean
text_converter_set_charset (GMimeTextConverter *converter, const char *charset, gboolean output, GError **error)
{
	GMimeTextConverterPrivate *priv;
	const char *encoding;
	char *name, **slot;
	iconv_t cd;

	g_return_val_if_fail (GMIME_IS_TEXT_CONVERTER (converter), FALSE);
	g_return_val_if_fail (charset != NULL, FALSE);
	g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

	priv = g_mime_text_converter_get_instance_private (converter);

	if (priv->active) {
		g_set_error_literal (error, GMIME_ERROR, GMIME_ERROR_INVALID_OPERATION,
				     "Cannot change the charset during conversion");
		return FALSE;
	}

	if (!(name = _g_mime_charset_get_text_name (charset, error)))
		return FALSE;

	encoding = _g_mime_charset_get_text_converter (name);
	cd = g_mime_iconv_open (output ? encoding : "UTF-8", output ? "UTF-8" : encoding);

	if (cd == (iconv_t) -1) {
		g_set_error (error, G_CONVERT_ERROR, G_CONVERT_ERROR_NO_CONVERSION,
			     "Cannot convert text using charset '%s'", charset);
		g_free (name);
		return FALSE;
	}

	g_mime_iconv_close (cd);
	slot = output ? &priv->output_charset : &priv->input_charset;
	g_free (*slot);
	*slot = name;

	return TRUE;
}

/**
 * g_mime_text_converter_get_input_charset:
 * @converter: a text converter
 *
 * Returns: (transfer none): the stream input charset, initially "utf-8".
 **/
const char *
g_mime_text_converter_get_input_charset (GMimeTextConverter *converter)
{
	GMimeTextConverterPrivate *priv;

	g_return_val_if_fail (GMIME_IS_TEXT_CONVERTER (converter), NULL);

	priv = g_mime_text_converter_get_instance_private (converter);

	return priv->input_charset;
}

/**
 * g_mime_text_converter_set_input_charset:
 * @converter: a text converter
 * @charset: a MIME charset name
 * @error: return location for a #GError, or %NULL
 *
 * Selects strict stream decoding. Generic UTF-16/32 names select little-endian
 * input. Strings are already UTF-8 and are not affected by this setting.
 *
 * Returns: %TRUE on success; unsupported charsets leave the setting unchanged.
 **/
gboolean
g_mime_text_converter_set_input_charset (GMimeTextConverter *converter, const char *charset, GError **error)
{
	return text_converter_set_charset (converter, charset, FALSE, error);
}

/**
 * g_mime_text_converter_get_output_charset:
 * @converter: a text converter
 *
 * Returns: (transfer none): the stream output charset, initially "utf-8".
 **/
const char *
g_mime_text_converter_get_output_charset (GMimeTextConverter *converter)
{
	GMimeTextConverterPrivate *priv;

	g_return_val_if_fail (GMIME_IS_TEXT_CONVERTER (converter), NULL);

	priv = g_mime_text_converter_get_instance_private (converter);

	return priv->output_charset;
}

/**
 * g_mime_text_converter_set_output_charset:
 * @converter: a text converter
 * @charset: a MIME charset name
 * @error: return location for a #GError, or %NULL
 *
 * Selects strict stream encoding. Unrepresentable characters are errors.
 * Generic UTF-16/32 names select little-endian bytes. String results are
 * always UTF-8. Output has no BOM unless explicitly enabled.
 *
 * Returns: %TRUE on success; unsupported charsets leave the setting unchanged.
 **/
gboolean
g_mime_text_converter_set_output_charset (GMimeTextConverter *converter, const char *charset, GError **error)
{
	return text_converter_set_charset (converter, charset, TRUE, error);
}

/**
 * g_mime_text_converter_get_detect_encoding_from_byte_order_mark:
 * @converter: a text converter
 *
 * Returns: whether stream BOM detection is enabled, initially %FALSE.
 **/
gboolean
g_mime_text_converter_get_detect_encoding_from_byte_order_mark (GMimeTextConverter *converter)
{
	GMimeTextConverterPrivate *priv;

	g_return_val_if_fail (GMIME_IS_TEXT_CONVERTER (converter), FALSE);

	priv = g_mime_text_converter_get_instance_private (converter);

	return priv->detect_bom;
}

/**
 * g_mime_text_converter_set_detect_encoding_from_byte_order_mark:
 * @converter: a text converter
 * @detect: whether to detect the stream encoding from its BOM
 *
 * When enabled, consumes an initial UTF-8, UTF-16LE/BE or UTF-32LE/BE BOM
 * and overrides the input charset. Without a BOM the configured charset is
 * used. When disabled, BOMs are ordinary text. Does not affect string input.
 **/
void
g_mime_text_converter_set_detect_encoding_from_byte_order_mark (GMimeTextConverter *converter, gboolean detect)
{
	GMimeTextConverterPrivate *priv;

	g_return_if_fail (GMIME_IS_TEXT_CONVERTER (converter));

	priv = g_mime_text_converter_get_instance_private (converter);

	g_return_if_fail (!priv->active);

	priv->detect_bom = detect != FALSE;
}

/**
 * g_mime_text_converter_get_write_bom:
 * @converter: a text converter
 *
 * Returns: whether stream BOM emission is enabled, initially %FALSE.
 **/
gboolean
g_mime_text_converter_get_write_bom (GMimeTextConverter *converter)
{
	GMimeTextConverterPrivate *priv;

	g_return_val_if_fail (GMIME_IS_TEXT_CONVERTER (converter), FALSE);

	priv = g_mime_text_converter_get_instance_private (converter);

	return priv->write_bom;
}

/**
 * g_mime_text_converter_set_write_bom:
 * @converter: a text converter
 * @write_bom: whether to emit a Unicode byte-order mark
 *
 * Writes a BOM at the current output position for UTF-8, UTF-16 or UTF-32,
 * including for empty output. Other charsets have no BOM. String output is
 * unaffected. Defaults to %FALSE rather than .NET's encoding-specific preamble.
 **/
void
g_mime_text_converter_set_write_bom (GMimeTextConverter *converter, gboolean write_bom)
{
	GMimeTextConverterPrivate *priv;

	g_return_if_fail (GMIME_IS_TEXT_CONVERTER (converter));

	priv = g_mime_text_converter_get_instance_private (converter);

	g_return_if_fail (!priv->active);

	priv->write_bom = write_bom != FALSE;
}

/**
 * g_mime_text_converter_get_input_stream_buffer_size:
 * @converter: a text converter
 *
 * Returns: the stream decoding buffer size in bytes, initially 4096.
 **/
gsize
g_mime_text_converter_get_input_stream_buffer_size (GMimeTextConverter *converter)
{
	GMimeTextConverterPrivate *priv;

	g_return_val_if_fail (GMIME_IS_TEXT_CONVERTER (converter), 0);

	priv = g_mime_text_converter_get_instance_private (converter);

	return priv->input_buffer_size;
}

/**
 * g_mime_text_converter_set_input_stream_buffer_size:
 * @converter: a text converter
 * @size: a positive byte count, at most G_MAXSSIZE
 *
 * Sets the decoder read size. Additional space retains incomplete characters.
 **/
void
g_mime_text_converter_set_input_stream_buffer_size (GMimeTextConverter *converter, gsize size)
{
	GMimeTextConverterPrivate *priv;

	g_return_if_fail (GMIME_IS_TEXT_CONVERTER (converter));
	g_return_if_fail (size > 0 && size <= G_MAXSSIZE);

	priv = g_mime_text_converter_get_instance_private (converter);

	g_return_if_fail (!priv->active);

	priv->input_buffer_size = size;
}

/**
 * g_mime_text_converter_get_output_stream_buffer_size:
 * @converter: a text converter
 *
 * Returns: the stream encoding buffer size in bytes, initially 4096.
 **/
gsize
g_mime_text_converter_get_output_stream_buffer_size (GMimeTextConverter *converter)
{
	GMimeTextConverterPrivate *priv;

	g_return_val_if_fail (GMIME_IS_TEXT_CONVERTER (converter), 0);

	priv = g_mime_text_converter_get_instance_private (converter);

	return priv->output_buffer_size;
}

/**
 * g_mime_text_converter_set_output_stream_buffer_size:
 * @converter: a text converter
 * @size: a positive byte count, at most G_MAXSSIZE
 *
 * Sets the encoder buffer size. At least 32 bytes are reserved for characters
 * and shift sequences.
 **/
void
g_mime_text_converter_set_output_stream_buffer_size (GMimeTextConverter *converter, gsize size)
{
	GMimeTextConverterPrivate *priv;

	g_return_if_fail (GMIME_IS_TEXT_CONVERTER (converter));
	g_return_if_fail (size > 0 && size <= G_MAXSSIZE);

	priv = g_mime_text_converter_get_instance_private (converter);

	g_return_if_fail (!priv->active);

	priv->output_buffer_size = size;
}

/**
 * g_mime_text_converter_get_input_format:
 * @converter: a text converter
 *
 * Returns: the input format.
 **/
GMimeTextFormat
g_mime_text_converter_get_input_format (GMimeTextConverter *converter)
{
	g_return_val_if_fail (GMIME_IS_TEXT_CONVERTER (converter), GMIME_TEXT_FORMAT_PLAIN);

	return GMIME_TEXT_CONVERTER_GET_CLASS (converter)->input_format;
}

/**
 * g_mime_text_converter_get_output_format:
 * @converter: a text converter
 *
 * Returns: the output format.
 **/
GMimeTextFormat
g_mime_text_converter_get_output_format (GMimeTextConverter *converter)
{
	g_return_val_if_fail (GMIME_IS_TEXT_CONVERTER (converter), GMIME_TEXT_FORMAT_PLAIN);

	return GMIME_TEXT_CONVERTER_GET_CLASS (converter)->output_format;
}

/**
 * g_mime_text_converter_get_header:
 * @converter: a text converter
 *
 * Returns: (nullable): borrowed UTF-8 text prepended to the converted body.
 **/
const char *
g_mime_text_converter_get_header (GMimeTextConverter *converter)
{
	GMimeTextConverterPrivate *priv;

	g_return_val_if_fail (GMIME_IS_TEXT_CONVERTER (converter), NULL);

	priv = g_mime_text_converter_get_instance_private (converter);

	return priv->header;
}

/**
 * g_mime_text_converter_set_header:
 * @converter: a text converter
 * @header: (nullable): UTF-8 text, or %NULL
 *
 * Copies @header. Set before conversion; no newline is added automatically.
 **/
void
g_mime_text_converter_set_header (GMimeTextConverter *converter, const char *header)
{
	GMimeTextConverterPrivate *priv;
	char *copy;

	g_return_if_fail (GMIME_IS_TEXT_CONVERTER (converter));
	g_return_if_fail (header == NULL || g_utf8_validate (header, -1, NULL));

	priv = g_mime_text_converter_get_instance_private (converter);

	g_return_if_fail (!priv->active);

	copy = g_strdup (header);
	g_free (priv->header);
	priv->header = copy;
}

/**
 * g_mime_text_converter_get_footer:
 * @converter: a text converter
 *
 * Returns: (nullable): borrowed UTF-8 text appended to the converted body.
 **/
const char *
g_mime_text_converter_get_footer (GMimeTextConverter *converter)
{
	GMimeTextConverterPrivate *priv;

	g_return_val_if_fail (GMIME_IS_TEXT_CONVERTER (converter), NULL);

	priv = g_mime_text_converter_get_instance_private (converter);

	return priv->footer;
}

/**
 * g_mime_text_converter_set_footer:
 * @converter: a text converter
 * @footer: (nullable): UTF-8 text, or %NULL
 *
 * Copies @footer. Set before conversion; no newline is added automatically.
 **/
void
g_mime_text_converter_set_footer (GMimeTextConverter *converter, const char *footer)
{
	GMimeTextConverterPrivate *priv;
	char *copy;

	g_return_if_fail (GMIME_IS_TEXT_CONVERTER (converter));
	g_return_if_fail (footer == NULL || g_utf8_validate (footer, -1, NULL));

	priv = g_mime_text_converter_get_instance_private (converter);

	g_return_if_fail (!priv->active);

	copy = g_strdup (footer);
	g_free (priv->footer);
	priv->footer = copy;
}

static gboolean
text_converter_convert (GMimeTextConverter *converter, GMimeStream *input, GMimeStream *output, gboolean encoded,
			GError **error)
{
	GMimeTextConverterPrivate *priv;
	GMimeTextConverterClass *klass;
	GMimeStream *reader = NULL, *writer = NULL;
	gboolean success = FALSE;

	g_return_val_if_fail (GMIME_IS_TEXT_CONVERTER (converter), FALSE);
	g_return_val_if_fail (GMIME_IS_STREAM (input), FALSE);
	g_return_val_if_fail (GMIME_IS_STREAM (output), FALSE);
	g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

	priv = g_mime_text_converter_get_instance_private (converter);
	klass = GMIME_TEXT_CONVERTER_GET_CLASS (converter);

	if (priv->active || input == output || klass->convert == NULL) {
		g_set_error_literal (error, GMIME_ERROR, GMIME_ERROR_INVALID_OPERATION,
				     "Text conversion requires distinct streams and an idle, implemented converter");
		return FALSE;
	}

	g_object_ref (converter);
	g_object_ref (input);
	g_object_ref (output);
	priv->active = TRUE;

	if (encoded && (g_ascii_strcasecmp (priv->input_charset, "utf-8") || priv->detect_bom ||
			priv->input_buffer_size != 4096)) {
		reader = _g_mime_text_stream_new_decoder (input, priv->input_charset, priv->detect_bom,
							  priv->input_buffer_size, error);
		if (!reader)
			goto done;
	} else {
		reader = g_object_ref (input);
	}

	if (encoded && (g_ascii_strcasecmp (priv->output_charset, "utf-8") || priv->write_bom ||
			priv->output_buffer_size != 4096)) {
		writer = _g_mime_text_stream_new_encoder (output, priv->output_charset, priv->write_bom,
							  priv->output_buffer_size, error);
		if (!writer)
			goto done;
	} else {
		writer = g_object_ref (output);
	}

	if (!klass->convert (converter, reader, writer, error))
		goto done;

	errno = 0;

	if (g_mime_stream_flush (writer) < 0) {
		text_io_error (error, errno, "flush");
		goto done;
	}

	success = TRUE;

done:
	if (!success) {
		if (reader)
			_g_mime_text_stream_propagate_error (reader, error);

		if (writer)
			_g_mime_text_stream_propagate_error (writer, error);
	}

	g_clear_object (&writer);
	g_clear_object (&reader);
	priv->active = FALSE;
	g_object_unref (output);
	g_object_unref (input);
	g_object_unref (converter);

	return success;
}

/**
 * g_mime_text_converter_convert:
 * @converter: a text converter
 * @input: a readable stream in the configured input charset
 * @output: a writable stream in the configured output charset, distinct from @input
 * @error: return location for a #GError, or %NULL
 *
 * Converts from the current input position, prepending/appending the configured
 * header/footer. Flushes output on success, without closing or rewinding either
 * stream. Output can be partial on failure. Shared backing storage must not
 * overlap. Invalid or unrepresentable characters are reported in %G_CONVERT_ERROR;
 * I/O errors use %GMIME_ERROR with errno, or %GMIME_ERROR_GENERAL when unavailable.
 * Encoding defaults to UTF-8 with no BOM detection or emission.
 *
 * Returns: %TRUE on success.
 **/
gboolean
g_mime_text_converter_convert (GMimeTextConverter *converter, GMimeStream *input, GMimeStream *output, GError **error)
{
	return text_converter_convert (converter, input, output, TRUE, error);
}

/**
 * g_mime_text_converter_convert_to_string:
 * @converter: a text converter
 * @text: UTF-8 input
 * @length: input length in bytes, or -1 for a terminated string
 * @error: return location for a #GError, or %NULL
 *
 * Converts a bounded string. Input need not be NUL-terminated when @length is
 * nonnegative. Malformed UTF-8 is rejected. Input and output are always UTF-8,
 * independent of stream charset and BOM settings. Plain-text converters reject
 * embedded NUL; HTML converters apply their tokenizer's NUL handling.
 *
 * Returns: (transfer full) (nullable): converted UTF-8 text; free with g_free().
 **/
char *
g_mime_text_converter_convert_to_string (GMimeTextConverter *converter, const char *text, gssize length, GError **error)
{
	GMimeStream *input, *output;
	GByteArray *bytes;
	char *result = NULL;

	g_return_val_if_fail (GMIME_IS_TEXT_CONVERTER (converter), NULL);
	g_return_val_if_fail (text != NULL, NULL);
	g_return_val_if_fail (length >= -1, NULL);
	g_return_val_if_fail (error == NULL || *error == NULL, NULL);

	if (length == -1)
		length = strlen (text);

	input = g_mime_stream_mem_new_with_buffer (text, length);
	output = g_mime_stream_mem_new ();

	if (text_converter_convert (converter, input, output, FALSE, error)) {
		bytes = g_mime_stream_mem_get_byte_array (GMIME_STREAM_MEM (output));
		result = g_strndup (bytes->len ? (const char *) bytes->data : "", bytes->len);
	}

	g_object_unref (output);
	g_object_unref (input);

	return result;
}

static gboolean
text_to_text_convert (GMimeTextConverter *converter, GMimeStream *input, GMimeStream *output, GError **error)
{
	GMimeTextConverterPrivate *priv = g_mime_text_converter_get_instance_private (converter);
	GMimeTextReader reader = { .stream = input };

	if (priv->header && !_g_mime_text_write (output, priv->header, strlen (priv->header), error))
		return FALSE;

	for (;;) {
		if (!_g_mime_text_reader_read (&reader, error))
			return FALSE;

		if (reader.position == reader.end)
			break;

		if (!_g_mime_text_write (output, reader.buffer + reader.position, reader.end - reader.position, error))
			return FALSE;

		reader.position = reader.end;
	}

	return !priv->footer || _g_mime_text_write (output, priv->footer, strlen (priv->footer), error);
}

static void
g_mime_text_to_text_class_init (GMimeTextToTextClass *klass)
{
	GMimeTextConverterClass *converter_class = GMIME_TEXT_CONVERTER_CLASS (klass);

	converter_class->input_format = GMIME_TEXT_FORMAT_PLAIN;
	converter_class->output_format = GMIME_TEXT_FORMAT_PLAIN;
	converter_class->convert = text_to_text_convert;
}

static void
g_mime_text_to_text_init (GMimeTextToText *converter)
{
}

/**
 * g_mime_text_to_text_new:
 *
 * Returns: (transfer full): a plain text copier.
 **/
GMimeTextConverter *
g_mime_text_to_text_new (void)
{
	return g_object_new (GMIME_TYPE_TEXT_TO_TEXT, NULL);
}

static const char *
text_unquote (const char *line, const char *end, gsize *depth, gboolean plain)
{
	*depth = 0;

	while (line < end && *line == '>') {
		(*depth)++;
		line++;

		if (plain && line < end && *line == ' ')
			line++;
	}

	if (!plain && *depth > 0 && line < end && *line == ' ')
		line++;

	return line;
}

static void
append_quote (GString *text, gsize depth)
{
	gsize i;

	for (i = 0; i < depth; i++)
		g_string_append_c (text, '>');
}

static const char *
get_flowed_line (GString *flowed, const char *line, const char *end, gsize *remaining, gsize quote_depth)
{
	static const GMimeReaderByteSet spaces = { { [0] = 1 << 2 }, FALSE };
	gsize prefix_length, soft_line_length, hard_line_length, length;

	g_string_truncate (flowed, 0);
	append_quote (flowed, quote_depth);

	/* Space-stuff lines which start with a space, "From ", or ">" (rfc3676, section 4.4). Note: Since the
	 * quote markers are removed from the line, a '>' can only appear at the start of a soft-broken line. */
	if (quote_depth > 0 || (line < end && (*line == ' ' || *line == '>')) ||
	    (end - line >= 5 && !memcmp (line, "From ", 5)))
		g_string_append_c (flowed, ' ');

	prefix_length = length = flowed->len;

	/* Note: When the quote depth is very deep, the quote markers alone may leave little or no room for text within
	 * MaxLineLength (or even MaxHardLineLength) characters. Always leave at least as much room for text as the
	 * quote markers take up, otherwise every line would contain a single word and the size of the output would
	 * grow with the quote depth multiplied by the number of words. */
	soft_line_length = MAX ((gsize) 78, prefix_length * 2);
	hard_line_length = MAX ((gsize) 998, prefix_length * 2);

	if (*remaining <= soft_line_length - length) {
		g_string_append_len (flowed, line, end - line);
		*remaining = 0;
		return end;
	}

	do {
		gsize bytes = _g_mime_reader_find_byte ((const guint8 *) line, end - line, &spaces);
		const char *word_end = line + bytes;
		gsize soft_break = word_end == end ? 0 : 2; /* 2 = space + soft-break space */
		gsize word_length = g_utf8_strlen (line, bytes);
		gsize max_line_length;

		if (length + word_length + soft_break <= soft_line_length) {
			/* The entire word will fit on the remainder of the line. */
		} else if (length > prefix_length && prefix_length + word_length + soft_break <= soft_line_length) {
			/* Only part of the word will fit on the remainder of this line, but it will easily fit
			 * on its own line. Insert a soft-break so that we don't break apart this word. */
			break;
		} else if (length + word_length + soft_break <= hard_line_length) {
			/* The word will not fit within softLineLength characters even on a line of its own, so send the word as is and exceed MaxLineLength
			 * rather than inserting a soft-break into the middle of the word (rfc3676, section 4.2). */
		} else if (length > prefix_length && prefix_length + word_length + soft_break <= hard_line_length) {
			/* The word will fit within hardLineLength characters on its own line. */
			break;
		} else {
			/* The word is too long to fit within hardLineLength characters even on a line of its own,
			 * so there is no choice but to break the word apart. Note: length is always less than
			 * softLineLength - 1 here and hardLineLength is at least twice prefixLength, so wordLength > 0. */
			word_length = hard_line_length - (length + 1);
			word_end = g_utf8_offset_to_pointer (line, word_length);
			g_string_append_len (flowed, line, word_end - line);
			*remaining -= word_length;
			line = word_end;
			break;
		}

		g_string_append_len (flowed, line, word_end - line);
		line = word_end;
		length += word_length;
		*remaining -= word_length;
		max_line_length = length < soft_line_length ? soft_line_length : hard_line_length;

		while (length + 1 < max_line_length && line < end && *line == ' ') {
			g_string_append_c (flowed, *line++);
			(*remaining)--;
			length++;
		}
	} while (line < end && length + 1 < soft_line_length);

	if (line < end)
		g_string_append_c (flowed, ' ');

	return line;
}

static gboolean
text_to_flowed_convert (GMimeTextConverter *converter, GMimeStream *input, GMimeStream *output, GError **error)
{
	GMimeTextConverterPrivate *priv = g_mime_text_converter_get_instance_private (converter);
	GMimeTextReader reader = { .stream = input };
	GString *line = g_string_new (NULL);
	GString *flowed = g_string_sized_new (80);
	gboolean eof, success = FALSE;

	if (priv->header && !_g_mime_text_write (output, priv->header, strlen (priv->header), error))
		goto done;

	for (;;) {
		const char *text, *end;
		gsize depth, remaining;

		if (!_g_mime_text_reader_read_line (&reader, line, &eof, error))
			break;

		if (eof) {
			success = !priv->footer ||
				  _g_mime_text_write (output, priv->footer, strlen (priv->footer), error);
			break;
		}

		/* Trim spaces before user-inserted hard line breaks. */
		end = line->str + line->len;
		while (end > line->str && end[-1] == ' ')
			end--;

		text = text_unquote (line->str, end, &depth, TRUE);
		remaining = g_utf8_strlen (text, end - text);

		/* Ensure all lines (fixed and flowed) are 78 characters or fewer in
		 * length, counting any trailing space as well as a space added as
		 * stuffing, but not counting the newline, unless a word by itself
		 * exceeds 78 characters. */
		do {
			text = get_flowed_line (flowed, text, end, &remaining, depth);
			g_string_append_c (flowed, '\n');

			if (!_g_mime_text_write (output, flowed->str, flowed->len, error))
				goto done;
		} while (text < end);
	}

done:
	g_string_free (flowed, TRUE);
	g_string_free (line, TRUE);

	return success;
}

static void
g_mime_text_to_flowed_class_init (GMimeTextToFlowedClass *klass)
{
	GMimeTextConverterClass *converter_class = GMIME_TEXT_CONVERTER_CLASS (klass);

	converter_class->input_format = GMIME_TEXT_FORMAT_PLAIN;
	converter_class->output_format = GMIME_TEXT_FORMAT_FLOWED;
	converter_class->convert = text_to_flowed_convert;
}

static void
g_mime_text_to_flowed_init (GMimeTextToFlowed *converter)
{
}

/**
 * g_mime_text_to_flowed_new:
 *
 * Converts plain text to flowed text. Label the output MIME part
 * text/plain; format=flowed; delsp=yes. Does not change MIME headers.
 *
 * Returns: (transfer full): a plain-to-flowed converter.
 **/
GMimeTextConverter *
g_mime_text_to_flowed_new (void)
{
	return g_object_new (GMIME_TYPE_TEXT_TO_FLOWED, NULL);
}

static gboolean
flowed_to_text_convert (GMimeTextConverter *converter, GMimeStream *input, GMimeStream *output, GError **error)
{
	GMimeTextConverterPrivate *priv = g_mime_text_converter_get_instance_private (converter);
	gboolean delete_space = GMIME_FLOWED_TO_TEXT (converter)->delete_space;
	gboolean eof, success = FALSE, have_para = FALSE;
	gsize para_quote_depth = 0, prefix_length = 0;
	GMimeTextReader reader = { .stream = input };
	GString *line = g_string_new (NULL);
	GString *para = g_string_new (NULL);

	if (priv->header && !_g_mime_text_write (output, priv->header, strlen (priv->header), error))
		goto done;

	for (;;) {
		const char *text, *end;
		gsize depth;

		if (!_g_mime_text_reader_read_line (&reader, line, &eof, error))
			goto done;

		if (eof)
			break;

		end = line->str + line->len;
		text = text_unquote (line->str, end, &depth, FALSE);

		/* if there is a leading space, it was stuffed */
		if (depth == 0 && text < end && *text == ' ')
			text++;

		if (have_para && depth != para_quote_depth) {
			/* Note: according to rfc3676, when a folded line has a different quote
			 * depth than the previous line, then quote-depth rules win and we need
			 * to treat this as a new paragraph. */
			g_string_append_c (para, '\n');

			if (!_g_mime_text_write (output, para->str, para->len, error))
				goto done;

			g_string_truncate (para, 0);
			have_para = FALSE;
		}

		if (!have_para) {
			para_quote_depth = depth;

			if (depth > 0) {
				append_quote (para, depth);
				g_string_append_c (para, ' ');
			}

			prefix_length = para->len;
			have_para = TRUE;
		}

		g_string_append_len (para, text, end - text);

		if (text == end || end[-1] != ' ') {
			/* when a line does not end with a space, then the paragraph has ended */
			g_string_append_c (para, '\n');

			if (!_g_mime_text_write (output, para->str, para->len, error))
				goto done;

			g_string_truncate (para, 0);
			have_para = FALSE;
		} else if (delete_space) {
			/* Note: lines ending with a space mean that the next line is a continuation */
			g_string_truncate (para, para->len - 1);
		}
	}

	/* Flush any remaining paragraph text. */
	if (have_para && para->len > prefix_length && !_g_mime_text_write (output, para->str, para->len, error))
		goto done;

	success = !priv->footer || _g_mime_text_write (output, priv->footer, strlen (priv->footer), error);

done:
	g_string_free (para, TRUE);
	g_string_free (line, TRUE);

	return success;
}

static void
g_mime_flowed_to_text_class_init (GMimeFlowedToTextClass *klass)
{
	GMimeTextConverterClass *converter_class = GMIME_TEXT_CONVERTER_CLASS (klass);

	converter_class->input_format = GMIME_TEXT_FORMAT_FLOWED;
	converter_class->output_format = GMIME_TEXT_FORMAT_PLAIN;
	converter_class->convert = flowed_to_text_convert;
}

static void
g_mime_flowed_to_text_init (GMimeFlowedToText *converter)
{
}

/**
 * g_mime_flowed_to_text_new:
 *
 * Returns: (transfer full): a flowed-to-plain converter with delsp=no.
 **/
GMimeTextConverter *
g_mime_flowed_to_text_new (void)
{
	return g_object_new (GMIME_TYPE_FLOWED_TO_TEXT, NULL);
}

/**
 * g_mime_flowed_to_text_get_delete_space:
 * @converter: a flowed-to-plain converter
 *
 * Returns: whether trailing soft-break spaces are removed.
 **/
gboolean
g_mime_flowed_to_text_get_delete_space (GMimeFlowedToText *converter)
{
	g_return_val_if_fail (GMIME_IS_FLOWED_TO_TEXT (converter), FALSE);

	return converter->delete_space;
}

/**
 * g_mime_flowed_to_text_set_delete_space:
 * @converter: a flowed-to-plain converter
 * @delete_space: whether the input MIME part has delsp=yes
 *
 * Set before conversion. Defaults to %FALSE, corresponding to delsp=no.
 **/
void
g_mime_flowed_to_text_set_delete_space (GMimeFlowedToText *converter, gboolean delete_space)
{
	GMimeTextConverterPrivate *priv;

	g_return_if_fail (GMIME_IS_FLOWED_TO_TEXT (converter));

	priv = g_mime_text_converter_get_instance_private (GMIME_TEXT_CONVERTER (converter));

	g_return_if_fail (!priv->active);

	converter->delete_space = delete_space != FALSE;
}
