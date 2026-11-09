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

#include "gmime-html-writer.h"
#include "gmime-html-utils.h"
#include "gmime-text-private.h"
#include "gmime-error.h"

/**
 * SECTION: gmime-html-writer
 * @title: HTML writer
 * @short_description: Stateful UTF-8 HTML serialization
 *
 * Writes escaped text and attributes using MimeKit's HTML output rules.
 * Start tags remain pending until another tag, text, finish, or flush call.
 * The stream is retained but never closed. Failures can leave partial output;
 * a failed writer should be discarded. Raw markup is not sanitized.
 **/

struct _GMimeHtmlWriter {
	GObject parent_object;
	GMimeStream *stream;
	GMimeHtmlWriterState state;
	gboolean empty;
};

G_DEFINE_FINAL_TYPE (GMimeHtmlWriter, g_mime_html_writer, G_TYPE_OBJECT)

static void
writer_finalize (GObject *object)
{
	g_object_unref (GMIME_HTML_WRITER (object)->stream);
	G_OBJECT_CLASS (g_mime_html_writer_parent_class)->finalize (object);
}

static void
g_mime_html_writer_class_init (GMimeHtmlWriterClass *klass)
{
	G_OBJECT_CLASS (klass)->finalize = writer_finalize;
}

static void
g_mime_html_writer_init (GMimeHtmlWriter *writer)
{
}

/**
 * g_mime_html_writer_new:
 * @stream: (transfer none): a stream for UTF-8 output
 *
 * Creates a writer which retains @stream without closing it. The caller must
 * finish or flush pending markup before releasing the writer.
 *
 * Returns: (transfer full): a new HTML writer
 **/
GMimeHtmlWriter *
g_mime_html_writer_new (GMimeStream *stream)
{
	GMimeHtmlWriter *writer;

	g_return_val_if_fail (GMIME_IS_STREAM (stream), NULL);
	writer = g_object_new (GMIME_TYPE_HTML_WRITER, NULL);
	writer->stream = g_object_ref (stream);

	return writer;
}

/**
 * g_mime_html_writer_get_state:
 * @writer: a #GMimeHtmlWriter
 *
 * Gets the current serialization state.
 *
 * Returns: the writer state
 **/
GMimeHtmlWriterState
g_mime_html_writer_get_state (GMimeHtmlWriter *writer)
{
	g_return_val_if_fail (GMIME_IS_HTML_WRITER (writer), GMIME_HTML_WRITER_STATE_DEFAULT);

	return writer->state;
}

static gboolean
invalid (GError **error, const char *message)
{
	g_set_error_literal (error, GMIME_ERROR, GMIME_ERROR_INVALID_OPERATION, message);
	return FALSE;
}

static gboolean
validate_text (const char *text, gssize *length, GError **error)
{
	if (!text || *length < -1)
		return invalid (error, "Invalid HTML text argument");

	if (*length == -1)
		*length = strlen (text);

	if (!g_utf8_validate (text, *length, NULL))
		return invalid (error, "HTML text must be NUL-free UTF-8");

	return TRUE;
}

static gboolean
validate_name (const char *name, gboolean attribute, GError **error)
{
	gssize length = -1;

	if (!validate_text (name, &length, error))
		return FALSE;

	if (!length || strpbrk (name, attribute ? "\t\r\n\f /=>\"'<" : "\t\r\n\f />"))
		return invalid (error, attribute ? "Invalid HTML attribute name" : "Invalid HTML tag name");

	return TRUE;
}

static gboolean
write_string (GMimeHtmlWriter *writer, const char *text, GError **error)
{
	return _g_mime_text_write (writer->stream, text, strlen (text), error);
}

/**
 * g_mime_html_writer_finish:
 * @writer: a #GMimeHtmlWriter
 * @error: (out) (optional): return location for an error
 *
 * Completes any pending start or empty-element tag without flushing or closing
 * the underlying stream. Does not write matching end tags.
 *
 * Returns: %TRUE on success or %FALSE on error
 **/
gboolean
g_mime_html_writer_finish (GMimeHtmlWriter *writer, GError **error)
{
	g_return_val_if_fail (GMIME_IS_HTML_WRITER (writer), FALSE);

	if (writer->state == GMIME_HTML_WRITER_STATE_DEFAULT)
		return TRUE;

	if (!write_string (writer, writer->empty ? "/>" : ">", error))
		return FALSE;

	writer->state = GMIME_HTML_WRITER_STATE_DEFAULT;
	writer->empty = FALSE;

	return TRUE;
}

static gboolean
write_tag (GMimeHtmlWriter *writer, const char *name, gboolean end, gboolean empty, GError **error)
{
	g_return_val_if_fail (GMIME_IS_HTML_WRITER (writer), FALSE);

	if (!validate_name (name, FALSE, error) || !g_mime_html_writer_finish (writer, error))
		return FALSE;

	if (!write_string (writer, end ? "</" : "<", error) || !write_string (writer, name, error))
		return FALSE;

	if (end)
		return write_string (writer, ">", error);

	writer->state = GMIME_HTML_WRITER_STATE_TAG;
	writer->empty = empty;

	return TRUE;
}

/**
 * g_mime_html_writer_write_start_tag:
 * @writer: a #GMimeHtmlWriter
 * @name: the UTF-8 tag name
 * @error: (out) (optional): return location for an error
 *
 * Completes any pending tag and begins a start tag. Attributes can be written
 * until the next tag, text, finish or flush operation.
 *
 * Returns: %TRUE on success or %FALSE on invalid input or a write error
 **/
gboolean
g_mime_html_writer_write_start_tag (GMimeHtmlWriter *writer, const char *name, GError **error)
{
	return write_tag (writer, name, FALSE, FALSE, error);
}

/**
 * g_mime_html_writer_write_end_tag:
 * @writer: a #GMimeHtmlWriter
 * @name: the UTF-8 tag name
 * @error: (out) (optional): return location for an error
 *
 * Completes any pending tag and writes an end tag. The writer does not check
 * whether a matching start tag was written.
 *
 * Returns: %TRUE on success or %FALSE on invalid input or a write error
 **/
gboolean
g_mime_html_writer_write_end_tag (GMimeHtmlWriter *writer, const char *name, GError **error)
{
	return write_tag (writer, name, TRUE, FALSE, error);
}

/**
 * g_mime_html_writer_write_empty_element_tag:
 * @writer: a #GMimeHtmlWriter
 * @name: the UTF-8 tag name
 * @error: (out) (optional): return location for an error
 *
 * Completes any pending tag and begins an empty-element tag. Attributes can
 * be written before the tag is completed with a trailing slash.
 *
 * Returns: %TRUE on success or %FALSE on invalid input or a write error
 **/
gboolean
g_mime_html_writer_write_empty_element_tag (GMimeHtmlWriter *writer, const char *name, GError **error)
{
	return write_tag (writer, name, FALSE, TRUE, error);
}

static gboolean
encode_attribute_name (GMimeHtmlWriter *writer, const char *name, GError **error)
{
	if (writer->state == GMIME_HTML_WRITER_STATE_DEFAULT)
		return invalid (error, "Cannot write attributes in the Default state");

	/* The tokenizer can produce names beginning with '='. Give a preceding
	 * valueless attribute an explicit value to prevent reparsing ambiguity. */
	if (writer->state == GMIME_HTML_WRITER_STATE_ATTRIBUTE && name[0] == '=' &&
	    !write_string (writer, "=\"\"", error))
		return FALSE;

	if (!write_string (writer, " ", error) || !write_string (writer, name, error))
		return FALSE;

	writer->state = GMIME_HTML_WRITER_STATE_ATTRIBUTE;

	return TRUE;
}

/**
 * g_mime_html_writer_write_attribute_name:
 * @writer: a #GMimeHtmlWriter
 * @name: the UTF-8 attribute name
 * @error: (out) (optional): return location for an error
 *
 * Writes an attribute name in a pending start or empty-element tag. An
 * attribute value is optional; writing another name leaves this one valueless.
 *
 * Returns: %TRUE on success or %FALSE on invalid input, an invalid writer
 * state or a write error
 **/
gboolean
g_mime_html_writer_write_attribute_name (GMimeHtmlWriter *writer, const char *name, GError **error)
{
	g_return_val_if_fail (GMIME_IS_HTML_WRITER (writer), FALSE);

	return validate_name (name, TRUE, error) && encode_attribute_name (writer, name, error);
}

/**
 * g_mime_html_writer_write_attribute_value:
 * @writer: a #GMimeHtmlWriter
 * @value: the NUL-terminated UTF-8 attribute value
 * @error: (out) (optional): return location for an error
 *
 * Escapes and quotes @value for the most recently written attribute name.
 * The writer must be in the %GMIME_HTML_WRITER_STATE_ATTRIBUTE state.
 *
 * Returns: %TRUE on success or %FALSE on invalid input, an invalid writer
 * state or a write error
 **/
gboolean
g_mime_html_writer_write_attribute_value (GMimeHtmlWriter *writer, const char *value, GError **error)
{
	gssize length = -1;
	char *encoded;
	gboolean result;

	g_return_val_if_fail (GMIME_IS_HTML_WRITER (writer), FALSE);

	if (!validate_text (value, &length, error))
		return FALSE;

	if (writer->state != GMIME_HTML_WRITER_STATE_ATTRIBUTE)
		return invalid (error, "Attribute values can only be written in the Attribute state");

	encoded = g_mime_html_attribute_encode (value);
	result = write_string (writer, "=", error) && write_string (writer, encoded, error);
	g_free (encoded);

	if (result)
		writer->state = GMIME_HTML_WRITER_STATE_TAG;

	return result;
}

/**
 * g_mime_html_writer_write_attribute:
 * @writer: a #GMimeHtmlWriter
 * @name: the UTF-8 attribute name
 * @value: (nullable): the NUL-terminated UTF-8 value, or %NULL for a valueless attribute
 * @error: (out) (optional): return location for an error
 *
 * Writes an attribute in a pending start or empty-element tag, escaping and
 * quoting its value when provided.
 *
 * Returns: %TRUE on success or %FALSE on invalid input, an invalid writer
 * state or a write error
 **/
gboolean
g_mime_html_writer_write_attribute (GMimeHtmlWriter *writer, const char *name, const char *value, GError **error)
{
	gssize length = -1;

	if (value && !validate_text (value, &length, error))
		return FALSE;

	return g_mime_html_writer_write_attribute_name (writer, name, error) &&
	       (!value || g_mime_html_writer_write_attribute_value (writer, value, error));
}

/**
 * g_mime_html_writer_write_html_attribute:
 * @writer: a #GMimeHtmlWriter
 * @attribute: (transfer none): an HTML attribute
 * @error: (out) (optional): return location for an error
 *
 * Writes @attribute in a pending start or empty-element tag. Unlike
 * g_mime_html_writer_write_attribute(), this accepts tokenizer-produced names
 * without revalidating them. The value is escaped and quoted when present.
 *
 * Returns: %TRUE on success or %FALSE on an invalid value, an invalid writer
 * state or a write error
 **/
gboolean
g_mime_html_writer_write_html_attribute (GMimeHtmlWriter *writer, const GMimeHtmlAttribute *attribute, GError **error)
{
	const char *value;

	g_return_val_if_fail (GMIME_IS_HTML_WRITER (writer) && attribute, FALSE);

	value = g_mime_html_attribute_get_value (attribute);

	return encode_attribute_name (writer, g_mime_html_attribute_get_name (attribute), error) &&
	       (!value || g_mime_html_writer_write_attribute_value (writer, value, error));
}

/**
 * g_mime_html_writer_write_text:
 * @writer: a #GMimeHtmlWriter
 * @text: NUL-free UTF-8 text
 * @length: the byte length of @text, or -1 for a NUL-terminated string
 * @error: (out) (optional): return location for an error
 *
 * Completes any pending tag and writes escaped text using g_mime_html_encode().
 *
 * Returns: %TRUE on success or %FALSE on invalid input or a write error
 **/
gboolean
g_mime_html_writer_write_text (GMimeHtmlWriter *writer, const char *text, gssize length, GError **error)
{
	char *copy, *encoded;
	gboolean result;

	g_return_val_if_fail (GMIME_IS_HTML_WRITER (writer), FALSE);

	if (!validate_text (text, &length, error) || !g_mime_html_writer_finish (writer, error))
		return FALSE;

	copy = g_strndup (text, length);
	encoded = g_mime_html_encode (copy);
	result = write_string (writer, encoded, error);
	g_free (encoded);
	g_free (copy);

	return result;
}

/**
 * g_mime_html_writer_write_markup_text:
 * @writer: a #GMimeHtmlWriter
 * @text: NUL-free UTF-8 markup
 * @length: the byte length of @text, or -1 for a NUL-terminated string
 * @error: (out) (optional): return location for an error
 *
 * Completes any pending tag and writes @text verbatim. The markup is not
 * escaped, parsed or sanitized.
 *
 * Returns: %TRUE on success or %FALSE on invalid input or a write error
 **/
gboolean
g_mime_html_writer_write_markup_text (GMimeHtmlWriter *writer, const char *text, gssize length, GError **error)
{
	g_return_val_if_fail (GMIME_IS_HTML_WRITER (writer), FALSE);

	return validate_text (text, &length, error) && g_mime_html_writer_finish (writer, error) &&
	       _g_mime_text_write (writer->stream, text, length, error);
}

/**
 * g_mime_html_writer_write_token:
 * @writer: a #GMimeHtmlWriter
 * @token: (transfer none): the token to serialize
 * @error: (out) (optional): return location for an error
 *
 * Completes any pending tag and serializes @token, preserving embedded NUL
 * bytes in token data. The token is not sanitized and remains owned by the
 * caller. A serialized tag is complete and cannot receive further attributes.
 *
 * Returns: %TRUE on success or %FALSE on a write error
 **/
gboolean
g_mime_html_writer_write_token (GMimeHtmlWriter *writer, GMimeHtmlToken *token, GError **error)
{
	g_return_val_if_fail (GMIME_IS_HTML_WRITER (writer), FALSE);
	g_return_val_if_fail (token != NULL, FALSE);

	return g_mime_html_writer_finish (writer, error) && g_mime_html_token_write_to (token, writer->stream, error);
}

/**
 * g_mime_html_writer_flush:
 * @writer: a #GMimeHtmlWriter
 * @error: (out) (optional): return location for an error
 *
 * Completes any pending tag and flushes the underlying stream without closing
 * it. Does not write matching end tags.
 *
 * Returns: %TRUE on success or %FALSE on a write or flush error
 **/
gboolean
g_mime_html_writer_flush (GMimeHtmlWriter *writer, GError **error)
{
	if (!g_mime_html_writer_finish (writer, error))
		return FALSE;

	errno = 0;
	if (g_mime_stream_flush (writer->stream) == -1) {
		g_set_error (error, GMIME_ERROR, errno ? errno : EIO, "HTML writer flush failed: %s",
			     g_strerror (errno ? errno : EIO));
		return FALSE;
	}

	return TRUE;
}
