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

#include <stdio.h>
#include <errno.h>
#include <sys/types.h>
#include <string.h>

#include "gmime-text-part.h"
#include "gmime-error.h"
#include "gmime-visitor.h"
#include "gmime-stream-mem.h"
#include "gmime-stream-filter.h"
#include "gmime-filter-charset.h"
#include "gmime-html-tokenizer.h"
#include "gmime-charset.h"
#include "gmime-common.h"
#include "gmime-internal.h"

#define d(x)

#define HTML_CHARSET_SCAN_LENGTH 1024


/**
 * SECTION: gmime-text-part
 * @title: GMimeTextPart
 * @short_description: textual MIME parts
 * @see_also:
 *
 * A #GMimeTextPart represents any text MIME part.
 **/

/* GObject class methods */
static void g_mime_text_part_class_init (GMimeTextPartClass *klass);
static void g_mime_text_part_init (GMimeTextPart *mime_part);
static void g_mime_text_part_finalize (GObject *object);


static GMimePartClass *parent_class = NULL;

static void
text_part_accept (GMimeEntity *object, GMimeVisitor *visitor)
{
	g_mime_visitor_visit_text_part (visitor, (GMimeTextPart *) object);
}


G_DEFINE_TYPE (GMimeTextPart, g_mime_text_part, GMIME_TYPE_PART)


static void
g_mime_text_part_class_init (GMimeTextPartClass *klass)
{
	GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
	
	parent_class = g_type_class_ref (GMIME_TYPE_PART);
	
	gobject_class->finalize = g_mime_text_part_finalize;
	GMIME_ENTITY_CLASS (klass)->accept = text_part_accept;
}

static void
g_mime_text_part_init (GMimeTextPart *mime_part)
{
}

static void
g_mime_text_part_finalize (GObject *object)
{
	G_OBJECT_CLASS (parent_class)->finalize (object);
}


/**
 * g_mime_text_part_new:
 *
 * Creates a new text MIME part object with a default content-type of
 * text/plain.
 *
 * Returns: an empty MIME Part object with a default content-type of
 * text/plain.
 **/
GMimeTextPart *
g_mime_text_part_new (void)
{
	return g_mime_text_part_new_with_subtype ("plain");
}


/**
 * g_mime_text_part_new_with_subtype:
 * @subtype: textual subtype string
 *
 * Creates a new text MIME part with a sepcified subtype.
 *
 * Returns: an empty text MIME part object with the specified subtype.
 **/
GMimeTextPart *
g_mime_text_part_new_with_subtype (const char *subtype)
{
	GMimeContentType *content_type;
	GMimeTextPart *mime_part;
	
	mime_part = g_object_new (GMIME_TYPE_TEXT_PART, NULL);
	
	content_type = g_mime_content_type_new ("text", subtype);
	g_mime_entity_set_content_type ((GMimeEntity *) mime_part, content_type);
	g_object_unref (content_type);
	
	return mime_part;
}


/**
 * g_mime_text_part_new_with_format:
 * @format: a #GMimeTextFormat
 *
 * Creates a text part for @format. Flowed text uses text/plain with a
 * format=flowed parameter. Both rich-text formats use text/rtf; this
 * constructor does not compress content.
 *
 * Returns: (transfer full): a new #GMimeTextPart.
 **/
GMimeTextPart *
g_mime_text_part_new_with_format (GMimeTextFormat format)
{
	GMimeTextPart *mime_part;
	const char *subtype;

	switch (format) {
	case GMIME_TEXT_FORMAT_PLAIN:
	case GMIME_TEXT_FORMAT_FLOWED:
		subtype = "plain";
		break;
	case GMIME_TEXT_FORMAT_HTML:
		subtype = "html";
		break;
	case GMIME_TEXT_FORMAT_ENRICHED:
		subtype = "enriched";
		break;
	case GMIME_TEXT_FORMAT_COMPRESSED_RICH_TEXT:
	case GMIME_TEXT_FORMAT_RICH_TEXT:
		subtype = "rtf";
		break;
	default:
		g_return_val_if_reached (NULL);
	}

	mime_part = g_mime_text_part_new_with_subtype (subtype);
	if (format == GMIME_TEXT_FORMAT_FLOWED)
		g_mime_entity_set_content_type_parameter ((GMimeEntity *) mime_part, "format", "flowed");

	return mime_part;
}


/**
 * g_mime_text_part_get_format:
 * @mime_part: a #GMimeTextPart
 *
 * Gets the format indicated by the Content-Type header. Unknown media
 * types and subtypes return %GMIME_TEXT_FORMAT_PLAIN. Enriched text
 * includes text/richtext; RTF includes application/rtf.
 *
 * Flowed text is text/plain with a format parameter equal to "flowed",
 * ignoring ASCII case and surrounding ASCII linear whitespace in the parameter.
 * Compressed rich text is not inferred from the Content-Type header.
 *
 * Returns: the text format.
 **/
GMimeTextFormat
g_mime_text_part_get_format (GMimeTextPart *mime_part)
{
	GMimeContentType *content_type;
	const char *subtype;

	g_return_val_if_fail (GMIME_IS_TEXT_PART (mime_part), GMIME_TEXT_FORMAT_PLAIN);

	content_type = g_mime_entity_get_content_type ((GMimeEntity *) mime_part);

	if (!g_ascii_strcasecmp (g_mime_content_type_get_media_type (content_type), "text")) {
		subtype = g_mime_content_type_get_media_subtype (content_type);

		if (!g_ascii_strcasecmp (subtype, "plain")) {
			if (g_mime_text_part_is_flowed (mime_part))
				return GMIME_TEXT_FORMAT_FLOWED;
		} else if (g_mime_text_part_is_html (mime_part)) {
			return GMIME_TEXT_FORMAT_HTML;
		} else if (g_mime_text_part_is_rich_text (mime_part)) {
			return GMIME_TEXT_FORMAT_RICH_TEXT;
		} else if (g_mime_text_part_is_enriched (mime_part)) {
			return GMIME_TEXT_FORMAT_ENRICHED;
		}
	} else if (g_mime_text_part_is_rich_text (mime_part)) {
		return GMIME_TEXT_FORMAT_RICH_TEXT;
	}

	return GMIME_TEXT_FORMAT_PLAIN;
}


/**
 * g_mime_text_part_is_plain:
 * @mime_part: a #GMimeTextPart
 *
 * Tests whether the Content-Type is text/plain, ignoring ASCII case.
 * This also matches flowed text.
 *
 * Returns: %TRUE for text/plain, otherwise %FALSE.
 **/
gboolean
g_mime_text_part_is_plain (GMimeTextPart *mime_part)
{
	GMimeContentType *content_type;

	g_return_val_if_fail (GMIME_IS_TEXT_PART (mime_part), FALSE);

	content_type = g_mime_entity_get_content_type ((GMimeEntity *) mime_part);

	return g_mime_content_type_is_type (content_type, "text", "plain");
}


/**
 * g_mime_text_part_is_flowed:
 * @mime_part: a #GMimeTextPart
 *
 * Tests for text/plain with a format parameter equal to "flowed",
 * ignoring ASCII case and surrounding ASCII linear whitespace.
 *
 * Returns: %TRUE for flowed text, otherwise %FALSE.
 **/
gboolean
g_mime_text_part_is_flowed (GMimeTextPart *mime_part)
{
	const char *format;

	g_return_val_if_fail (GMIME_IS_TEXT_PART (mime_part), FALSE);

	if (!g_mime_text_part_is_plain (mime_part))
		return FALSE;

	format = g_mime_entity_get_content_type_parameter ((GMimeEntity *) mime_part, "format");

	return _g_mime_strcase_equal_trimmed (format, "flowed");
}


/**
 * g_mime_text_part_is_html:
 * @mime_part: a #GMimeTextPart
 *
 * Tests whether the Content-Type is text/html, ignoring ASCII case.
 *
 * Returns: %TRUE for text/html, otherwise %FALSE.
 **/
gboolean
g_mime_text_part_is_html (GMimeTextPart *mime_part)
{
	GMimeContentType *content_type;

	g_return_val_if_fail (GMIME_IS_TEXT_PART (mime_part), FALSE);

	content_type = g_mime_entity_get_content_type ((GMimeEntity *) mime_part);

	return g_mime_content_type_is_type (content_type, "text", "html");
}


/**
 * g_mime_text_part_is_enriched:
 * @mime_part: a #GMimeTextPart
 *
 * Tests whether the Content-Type is text/enriched or text/richtext,
 * ignoring ASCII case.
 *
 * Returns: %TRUE for enriched text, otherwise %FALSE.
 **/
gboolean
g_mime_text_part_is_enriched (GMimeTextPart *mime_part)
{
	GMimeContentType *content_type;

	g_return_val_if_fail (GMIME_IS_TEXT_PART (mime_part), FALSE);

	content_type = g_mime_entity_get_content_type ((GMimeEntity *) mime_part);
	return g_mime_content_type_is_type (content_type, "text", "enriched") ||
		g_mime_content_type_is_type (content_type, "text", "richtext");
}


/**
 * g_mime_text_part_is_rich_text:
 * @mime_part: a #GMimeTextPart
 *
 * Tests whether the Content-Type is text/rtf or application/rtf,
 * ignoring ASCII case.
 *
 * Returns: %TRUE for RTF, otherwise %FALSE.
 **/
gboolean
g_mime_text_part_is_rich_text (GMimeTextPart *mime_part)
{
	GMimeContentType *content_type;

	g_return_val_if_fail (GMIME_IS_TEXT_PART (mime_part), FALSE);

	content_type = g_mime_entity_get_content_type ((GMimeEntity *) mime_part);

	return g_mime_content_type_is_type (content_type, "text", "rtf") ||
		g_mime_content_type_is_type (content_type, "application", "rtf");
}

gboolean
_g_mime_text_part_is_format (GMimeTextPart *part, GMimeTextFormat format)
{
	switch (format) {
	case GMIME_TEXT_FORMAT_PLAIN:
		return g_mime_text_part_is_plain (part);
	case GMIME_TEXT_FORMAT_FLOWED:
		return g_mime_text_part_is_flowed (part);
	case GMIME_TEXT_FORMAT_HTML:
		return g_mime_text_part_is_html (part);
	case GMIME_TEXT_FORMAT_ENRICHED:
		return g_mime_text_part_is_enriched (part);
	case GMIME_TEXT_FORMAT_RICH_TEXT:
		return g_mime_text_part_is_rich_text (part);
	default:
		return FALSE;
	}
}


/**
 * g_mime_text_part_set_charset:
 * @mime_part: a #GMimeTextPart
 * @charset: the name of the charset
 *
 * Sets the charset parameter on the Content-Type header to the specified value.
 **/
void
g_mime_text_part_set_charset (GMimeTextPart *mime_part, const char *charset)
{
	GMimeContentType *content_type;
	
	g_return_if_fail (GMIME_IS_TEXT_PART (mime_part));
	g_return_if_fail (charset != NULL);
	
	content_type = g_mime_entity_get_content_type ((GMimeEntity *) mime_part);
	g_mime_content_type_set_parameter (content_type, "charset", charset);
}


/**
 * g_mime_text_part_get_charset:
 * @mime_part: a #GMimeTextPart
 *
 * Gets the value of the charset parameter on the Content-Type header.
 *
 * Returns: the value of the charset parameter or %NULL if unavailable.
 **/
const char *
g_mime_text_part_get_charset (GMimeTextPart *mime_part)
{
	GMimeContentType *content_type;
	
	g_return_val_if_fail (GMIME_IS_TEXT_PART (mime_part), NULL);
	
	content_type = g_mime_entity_get_content_type ((GMimeEntity *) mime_part);
	
	return g_mime_content_type_get_parameter (content_type, "charset");
}


/**
 * g_mime_text_part_set_text:
 * @mime_part: a #GMimeTextPart
 * @text: the text in utf-8
 *
 * Sets the specified text as the content and updates the charset parameter on the Content-Type header.
 **/
void
g_mime_text_part_set_text (GMimeTextPart *mime_part, const char *text)
{
	GMimeContentType *content_type;
	GMimeStream *filtered, *stream;
	GMimeContentEncoding encoding;
	GMimeContent *content;
	GMimeFilter *filter;
	const char *charset;
	GMimeCharset mask;
	size_t len;
	
	g_return_if_fail (GMIME_IS_TEXT_PART (mime_part));
	g_return_if_fail (text != NULL);
	
	len = strlen (text);
	
	g_mime_charset_init (&mask);
	g_mime_charset_step (&mask, text, len);
	
	switch (mask.level) {
	case 1: charset = "iso-8859-1"; break;
	case 0: charset = "us-ascii"; break;
	default: charset = "utf-8"; break;
	}
	
	content_type = g_mime_entity_get_content_type ((GMimeEntity *) mime_part);
	g_mime_content_type_set_parameter (content_type, "charset", charset);
	
	stream = g_mime_stream_mem_new_with_buffer (text, len);
	
	if (mask.level == 1) {
		filtered = g_mime_stream_filter_new (stream);
		g_object_unref (stream);
		
		filter = g_mime_filter_charset_new ("utf-8", charset);
		g_mime_stream_filter_add ((GMimeStreamFilter *) filtered, filter);
		g_object_unref (filter);
		
		stream = filtered;
	}
	
	content = g_mime_content_new_with_stream (stream, GMIME_CONTENT_ENCODING_UNSPECIFIED);
	g_object_unref (stream);
	
	g_mime_part_set_content ((GMimePart *) mime_part, content);
	g_object_unref (content);
	
	encoding = g_mime_part_get_content_encoding ((GMimePart *) mime_part);
	
	/* if the user has already specified encoding the content with base64/qp/uu, don't change it */
	if (encoding > GMIME_CONTENT_ENCODING_BINARY)
		return;
	
	/* ...otherwise, set an appropriate Content-Transfer-Encoding based on the text provided... */
	if (mask.level > 0)
		g_mime_part_set_content_encoding ((GMimePart *) mime_part, GMIME_CONTENT_ENCODING_8BIT);
	else
		g_mime_part_set_content_encoding ((GMimePart *) mime_part, GMIME_CONTENT_ENCODING_7BIT);
}


/**
 * g_mime_text_part_set_text_with_charset:
 * @mime_part: a #GMimeTextPart
 * @charset: a nonempty MIME charset name
 * @text: the text in UTF-8
 * @error: return location for an error, or %NULL
 *
 * Encodes @text using @charset and replaces the content and Content-Type
 * charset parameter. Charset aliases are normalized to MIME names. Generic
 * UTF-16 and UTF-32 use little-endian bytes without a byte-order mark.
 * The historical MimeKit alias "unicode" selects UTF-8, not UTF-16.
 * No newline normalization or Content-Transfer-Encoding selection occurs.
 * An existing transfer encoding is preserved.
 *
 * Unlike the legacy g_mime_text_part_set_text(), this function uses exactly
 * the requested charset. Unsupported charsets, invalid UTF-8, and text that
 * cannot be represented return a %G_CONVERT_ERROR without changing the part.
 * Managed custom encoders and replacement fallbacks are not supported.
 * Converted content exceeding the #GMimeStreamMem limit reports %GMIME_ERROR
 * with EOVERFLOW.
 *
 * Returns: %TRUE on success, or %FALSE on failure.
 **/
gboolean
g_mime_text_part_set_text_with_charset (GMimeTextPart *mime_part, const char *charset,
				      const char *text, GError **error)
{
	GMimeStream *stream;
	GMimeContent *content;
	char *bytes, *name;
	gsize length;

	g_return_val_if_fail (GMIME_IS_TEXT_PART (mime_part), FALSE);
	g_return_val_if_fail (charset != NULL, FALSE);
	g_return_val_if_fail (text != NULL, FALSE);
	g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

	if (!(bytes = _g_mime_charset_encode_text (charset, text, &length, &name, error)))
		return FALSE;

	if (length > G_MAXUINT) {
		g_set_error_literal (error, GMIME_ERROR, EOVERFLOW, "Text content exceeds the native memory stream limit");
		g_free (bytes);
		g_free (name);
		return FALSE;
	}

	/* Prepare owned bytes before publishing either change: inputs may be
	 * borrowed from the part, and conversion failures must leave it intact. */
	stream = g_mime_stream_mem_new_with_buffer (bytes, length);
	g_free (bytes);

	content = g_mime_content_new_with_stream (stream, GMIME_CONTENT_ENCODING_UNSPECIFIED);
	g_object_unref (stream);

	g_mime_text_part_set_charset (mime_part, name);
	g_free (name);

	g_mime_part_set_content ((GMimePart *) mime_part, content);
	g_object_unref (content);

	return TRUE;
}


static GMimeStream *
text_part_decode_content (GMimeTextPart *mime_part, GError **error)
{
	GMimeContent *content;
	GMimeStream *stream;
	int errsv;

	if (!(content = g_mime_part_get_content ((GMimePart *) mime_part)))
		return g_mime_stream_mem_new ();

	if (!g_mime_content_get_stream (content)) {
		g_set_error_literal (error, GMIME_ERROR, GMIME_ERROR_INVALID_OPERATION,
				     "Cannot decode text content without a backing stream");
		return NULL;
	}

	g_object_ref (content);
	stream = g_mime_stream_mem_new ();
	errno = 0;

	if (g_mime_content_decode_to_stream (content, stream) == -1) {
		errsv = errno;
		g_object_unref (content);
		g_object_unref (stream);
		g_set_error (error, GMIME_ERROR, errsv ? errsv : GMIME_ERROR_GENERAL,
			     "Failed to decode text content: %s",
			     errsv ? g_strerror (errsv) : "unknown I/O error");
		return NULL;
	}

	g_object_unref (content);
	return stream;
}

static char *
text_part_decode_text (GByteArray *buffer, iconv_t decoder, const char *fallback_charset, GError **error)
{
	char *text, *output, *normalized;
	size_t outlen, prespace;
	GMimeFilter *filter;
	gsize length;

	/* The legacy charset filter silently skips malformed bytes. Explicit
	 * decoding is strict; automatic decoding can request replacement. */
	if (fallback_charset)
		text = _g_mime_iconv_decode_with_fallback (buffer->len ? (const char *) buffer->data : "", buffer->len,
							 decoder, fallback_charset, &length, error);
	else
		text = _g_mime_iconv_convert_with_iconv (buffer->len ? (const char *) buffer->data : "", buffer->len,
						       decoder, &length, error);

	if (!text)
		return NULL;

	if (memchr (text, '\0', length)) {
		g_set_error_literal (error, G_CONVERT_ERROR, G_CONVERT_ERROR_EMBEDDED_NUL,
				     "Decoded text contains an embedded NUL character");
		g_free (text);
		return NULL;
	}

	if (!g_utf8_validate_len (text, length, NULL)) {
		g_set_error_literal (error, G_CONVERT_ERROR, G_CONVERT_ERROR_ILLEGAL_SEQUENCE,
				     "Decoded text contains an invalid Unicode sequence");
		g_free (text);
		return NULL;
	}

	if (!length)
		return text;

	/* Newline conversion belongs after charset decoding: CR/LF byte values
	 * inside UTF-16/32 characters must never be treated as line endings. */
	filter = g_mime_format_options_create_newline_filter (NULL, FALSE);
	g_mime_filter_complete (filter, text, length, 0, &output, &outlen, &prespace);
	normalized = g_strndup (output, outlen);
	g_object_unref (filter);
	g_free (text);

	return normalized;
}

static GMimeStream *
text_part_open_content (GMimeContent *content, GError **error)
{
	GMimeStream *stream;
	int errsv;

	if (!g_mime_content_get_stream (content)) {
		g_set_error_literal (error, GMIME_ERROR, GMIME_ERROR_INVALID_OPERATION,
				     "Cannot detect text encoding without a backing stream");
		return NULL;
	}

	errno = 0;
	stream = g_mime_content_open (content);
	errsv = errno;

	if (!stream) {
		g_set_error (error, GMIME_ERROR, errsv ? errsv : GMIME_ERROR_GENERAL,
			     "Failed to open text content: %s",
			     errsv ? g_strerror (errsv) : "unknown I/O error");
		return NULL;
	}

	return stream;
}

static char *
text_part_get_meta_charset (GMimeHtmlToken *token, gboolean *replace, GError **error)
{
	gboolean seen_charset = FALSE, seen_content = FALSE, seen_pragma = FALSE;
	gboolean need_pragma = FALSE, got_pragma = FALSE;
	char *charset = NULL, *trimmed, *name;
	const GMimeHtmlAttribute *attribute;
	const char *value, *start, *end;
	GMimeContentType *type;
	int codepage;
	gsize i;

	for (i = 0; i < g_mime_html_token_get_n_attributes (token); i++) {
		attribute = g_mime_html_token_get_attribute (token, i);

		if (!(value = g_mime_html_attribute_get_value (attribute)))
			continue;

		switch (g_mime_html_attribute_get_id (attribute)) {
		case GMIME_HTML_ATTRIBUTE_HTTP_EQUIV:
			if (!seen_pragma) {
				got_pragma = !g_ascii_strcasecmp (value, "Content-Type");
				seen_pragma = TRUE;
			}
			break;
		case GMIME_HTML_ATTRIBUTE_CONTENT:
			if (!seen_content && !charset &&
			    (type = g_mime_content_type_parse_full (NULL, value, -1, NULL))) {
				value = g_mime_content_type_get_charset (type);

				if (value && *value) {
					charset = g_strdup (value);
					need_pragma = TRUE;
				}

				g_object_unref (type);
			}

			seen_content = TRUE;
			break;
		case GMIME_HTML_ATTRIBUTE_CHARSET:
			if (!seen_charset && *value) {
				g_free (charset);
				charset = g_strdup (value);
				need_pragma = FALSE;
			}

			seen_charset = TRUE;
			break;
		default:
			break;
		}
	}

	if (!charset || (need_pragma && !got_pragma)) {
		g_free (charset);
		return NULL;
	}

	start = charset;
	end = charset + strlen (charset);

	while (start < end && g_ascii_isspace (*start))
		start++;

	while (end > start) {
		if (!g_ascii_isspace (end[-1]))
			break;

		end--;
	}

	trimmed = g_strndup (start, end - start);
	g_free (charset);

	type = g_mime_content_type_new ("text", "html");
	g_mime_content_type_set_charset (type, !g_ascii_strcasecmp (trimmed, "x-user-defined") ?
					"windows-1252" : trimmed);
	g_free (trimmed);
	name = g_mime_content_type_get_charset_encoding (type, error);
	g_object_unref (type);

	if (name) {
		codepage = _g_mime_charset_get_codepage (_g_mime_charset_get_text_converter (name));
		*replace = codepage != 1200 && codepage != 1201;

		if (!*replace) {
			g_free (name);
			name = g_strdup ("utf-8");
		}
	}

	return name;
}

static char *
text_part_detect_html_charset (const char *bytes, gsize length, gboolean *replace, GError **error)
{
	enum { NONE, HTML, HEAD, STOP } state = NONE;
	GMimeHtmlTokenizer *tokenizer;
	GMimeHtmlToken *token;
	GMimeStream *stream;
	GError *local_error = NULL;
	char *charset = NULL;

	/* Limit processing to the first 1024 transfer-decoded bytes. Latin-1
	 * keeps the prescan independent of the charset it is trying to find. */
	stream = g_mime_stream_mem_new_with_buffer (length ? bytes : "", MIN (length, HTML_CHARSET_SCAN_LENGTH));
	tokenizer = g_mime_html_tokenizer_new_with_charset (stream, "iso-8859-1", FALSE,
							 HTML_CHARSET_SCAN_LENGTH, error);
	g_object_unref (stream);

	if (!tokenizer)
		return NULL;

	while (g_mime_html_tokenizer_read_next_token (tokenizer, &token, &local_error) && token) {
		if (g_mime_html_token_get_kind (token) == GMIME_HTML_TOKEN_TAG) {
			switch (g_mime_html_token_get_tag_id (token)) {
			case GMIME_HTML_TAG_HTML:
				if (state == NONE)
					state = g_mime_html_token_is_end_tag (token) ||
						g_mime_html_token_is_empty_element (token) ? STOP : HTML;
				break;
			case GMIME_HTML_TAG_HEAD:
				if (state == HTML)
					state = g_mime_html_token_is_end_tag (token) ||
						g_mime_html_token_is_empty_element (token) ? STOP : HEAD;
				break;
			case GMIME_HTML_TAG_META:
				if (state == HEAD && !g_mime_html_token_is_end_tag (token))
					charset = text_part_get_meta_charset (token, replace, &local_error);
				break;
			case GMIME_HTML_TAG_BODY:
				state = STOP;
				break;
			default:
				break;
			}
		}

		g_mime_html_token_unref (token);

		if (charset || local_error || state == STOP)
			break;
	}

	g_object_unref (tokenizer);

	if (local_error)
		g_propagate_error (error, local_error);

	return charset;
}

/**
 * g_mime_text_part_try_detect_encoding:
 * @mime_part: a #GMimeTextPart
 * @charset: (out) (nullable) (transfer full): the detected charset, or %NULL
 * @confidence: (out) (optional): confidence in the detected charset
 * @error: return location for an error, or %NULL
 *
 * Resolves the Content-Type charset, then looks for a UTF-8 or UTF-16 BOM in
 * transfer-decoded content. A supported declared charset takes precedence
 * over a BOM; either gives %GMIME_TEXT_ENCODING_CONFIDENCE_CERTAIN. Missing,
 * invalid and unsupported declarations are ignored. No content gives
 * "us-ascii" with %GMIME_TEXT_ENCODING_CONFIDENCE_IRRELEVANT, even if a charset
 * is declared. Empty content is not the same as absent content.
 *
 * HTML without a declaration or BOM is scanned for meta charset declarations
 * in its first 1024 transfer-decoded bytes. A supported meta charset gives
 * %GMIME_TEXT_ENCODING_CONFIDENCE_TENTATIVE. The scan requires opening html
 * and head tags and stops at a body tag. Both charset attributes and
 * http-equiv="Content-Type" declarations are recognized. Unsupported names
 * are skipped; x-user-defined maps to Windows-1252 and UTF-16 to UTF-8.
 * Otherwise returns %FALSE with %NULL @charset and
 * %GMIME_TEXT_ENCODING_CONFIDENCE_UNDEFINED. This does not guess UTF-8 or
 * Latin-1 or validate the entire content.
 *
 * A declared charset or absent content requires no I/O. Otherwise this opens
 * a decoded reader with g_mime_content_open() and makes one read of up to
 * three decoded bytes. The decoder may read ahead in the backing stream;
 * short BOM reads are not retried. The HTML scan opens a new decoded reader
 * and fills its bounded prefix across short reads. Readers are released
 * without closing or rewinding the backing stream. This path uses the backing
 * stream, not the content's decode-to-stream override. Source-backed content
 * must remain open and unchanged.
 *
 * Stream failures report %GMIME_ERROR; operational charset resolution
 * failures report %G_CONVERT_ERROR. Both outputs are cleared on failure.
 * Neither headers nor content bytes are modified. Free @charset with g_free().
 *
 * Returns: %TRUE if an encoding was detected, otherwise %FALSE.
 **/
gboolean
g_mime_text_part_try_detect_encoding (GMimeTextPart *mime_part, char **charset,
				    GMimeTextEncodingConfidence *confidence, GError **error)
{
	char buffer[HTML_CHARSET_SCAN_LENGTH];
	GError *local_error = NULL;
	gboolean found, replace;
	GMimeContent *content;
	GMimeStream *stream;
	const char *bom;
	gsize length = 0;
	ssize_t nread;
	int errsv;

	g_return_val_if_fail (charset != NULL, FALSE);

	*charset = NULL;

	if (confidence)
		*confidence = GMIME_TEXT_ENCODING_CONFIDENCE_UNDEFINED;

	g_return_val_if_fail (GMIME_IS_TEXT_PART (mime_part), FALSE);
	g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

	if (!(content = g_mime_part_get_content ((GMimePart *) mime_part))) {
		*charset = g_strdup ("us-ascii");

		if (confidence)
			*confidence = GMIME_TEXT_ENCODING_CONFIDENCE_IRRELEVANT;

		return TRUE;
	}

	*charset = g_mime_content_type_get_charset_encoding (
		g_mime_entity_get_content_type ((GMimeEntity *) mime_part), &local_error);

	if (local_error) {
		g_propagate_error (error, local_error);
		return FALSE;
	}

	if (*charset) {
		if (confidence)
			*confidence = GMIME_TEXT_ENCODING_CONFIDENCE_CERTAIN;

		return TRUE;
	}

	g_object_ref (content);

	if (!(stream = text_part_open_content (content, error))) {
		g_object_unref (content);
		return FALSE;
	}

	found = g_mime_charset_try_get_bom_encoding_from_stream (stream, &bom, &local_error);
	g_object_unref (stream);

	if (found) {
		*charset = g_strdup (bom);

		if (confidence)
			*confidence = GMIME_TEXT_ENCODING_CONFIDENCE_CERTAIN;
	} else if (!local_error && g_mime_text_part_is_html (mime_part) &&
		   (stream = text_part_open_content (content, &local_error))) {
		while (length < sizeof (buffer)) {
			errno = 0;
			nread = g_mime_stream_read (stream, buffer + length, sizeof (buffer) - length);

			if (nread == -1) {
				errsv = errno;
				g_set_error (&local_error, GMIME_ERROR, errsv ? errsv : GMIME_ERROR_GENERAL,
					     "Failed to read HTML text content: %s",
					     errsv ? g_strerror (errsv) : "unknown I/O error");
				break;
			}

			if (!nread)
				break;

			length += nread;
		}

		g_object_unref (stream);

		if (!local_error)
			*charset = text_part_detect_html_charset (buffer, length, &replace, &local_error);

		if (*charset) {
			found = TRUE;

			if (confidence)
				*confidence = GMIME_TEXT_ENCODING_CONFIDENCE_TENTATIVE;
		}
	}

	g_object_unref (content);

	if (local_error)
		g_propagate_error (error, local_error);

	return found;
}

/**
 * g_mime_text_part_get_text_with_charset:
 * @mime_part: a #GMimeTextPart
 * @charset: a nonempty MIME charset name
 * @error: return location for an error, or %NULL
 *
 * Decodes the content using @charset, overriding the Content-Type charset.
 * Normalizes newlines using the default #GMimeFormatOptions, without adding
 * a final newline. Bare carriage returns and explicit Unicode byte-order
 * marks are preserved. Generic UTF-16/32 aliases use little-endian bytes.
 * The historical MimeKit alias "unicode" selects UTF-8, not UTF-16.
 * Missing content returns an empty string after validating @charset.
 *
 * Unsupported charsets and malformed input report %G_CONVERT_ERROR instead
 * of replacing or dropping bytes. Embedded NUL characters cannot be returned
 * by this native string API and report %G_CONVERT_ERROR_EMBEDDED_NUL.
 * Missing backing streams report %GMIME_ERROR_INVALID_OPERATION; I/O failures
 * report %GMIME_ERROR with the stream's errno or %GMIME_ERROR_GENERAL.
 * No failure returns partial text.
 *
 * Headers and content are unchanged. Decoding honors content stream-copy
 * overrides and rewinds the source before copying and again on success;
 * an I/O failure leaves the source at its current position.
 *
 * Returns: (nullable) (transfer full): UTF-8 text, or %NULL on failure.
 **/
char *
g_mime_text_part_get_text_with_charset (GMimeTextPart *mime_part, const char *charset, GError **error)
{
	GMimeStream *stream;
	iconv_t decoder;
	char *text = NULL;

	g_return_val_if_fail (GMIME_IS_TEXT_PART (mime_part), NULL);
	g_return_val_if_fail (charset != NULL, NULL);
	g_return_val_if_fail (error == NULL || *error == NULL, NULL);

	/* Resolve the override even for absent content, before reading any
	 * bytes. A bad charset must not become an empty success or an I/O error. */
	if ((decoder = _g_mime_charset_open_text_decoder (charset, NULL, error)) == (iconv_t) -1)
		return NULL;

	if ((stream = text_part_decode_content (mime_part, error))) {
		text = text_part_decode_text (g_mime_stream_mem_get_byte_array ((GMimeStreamMem *) stream), decoder, NULL, error);
		g_object_unref (stream);
	}

	g_mime_iconv_close (decoder);

	return text;
}

/**
 * g_mime_text_part_get_text_with_encoding:
 * @mime_part: a #GMimeTextPart
 * @used_charset: (out) (optional) (nullable) (transfer full): the charset actually used
 * @error: return location for an error, or %NULL
 *
 * Gets checked, automatically decoded UTF-8 text and optionally reports its
 * source charset. Uses the same decoding policy as
 * g_mime_message_get_text_body_with_format(): supported declared charsets and
 * UTF-16 BOMs use replacement, while UTF-8 BOMs and the UTF-8 default use
 * strict decoding and retry malformed input as ISO-8859-1. Without a declared
 * charset or BOM, HTML meta charsets in the first 1024 decoded bytes are used
 * before the UTF-8 default. Meta charsets use replacement, except UTF-16
 * declarations remapped to strict UTF-8. Incomplete final characters are
 * handled rather than dropped.
 *
 * Reports the charset used by the successful conversion, including Latin-1
 * after fallback, not merely the detected or declared charset. Missing content
 * gives an allocated empty string and "us-ascii"; empty content is decoded
 * using the normal charset selection. @used_charset is %NULL on any error.
 *
 * Transfer-decoded bytes are read once, honoring content decode-to-stream
 * overrides. The complete buffered prefix is used for BOM detection, even
 * when the source produces short reads. Newlines follow the default
 * #GMimeFormatOptions. BOMs and bare CRs are retained; no final newline is
 * added. No flowed-text conversion or markup conversion is done.
 *
 * I/O and conversion failures never return partial text. Embedded decoded NULs
 * report %G_CONVERT_ERROR_EMBEDDED_NUL. Source-backed content must remain open
 * and unchanged; successful decoding rewinds its stream, while failures may
 * leave it advanced. Headers and content bytes remain unchanged.
 * Free both the returned text and @used_charset with g_free().
 *
 * Returns: (nullable) (transfer full): UTF-8 text, or %NULL on failure.
 **/
char *
g_mime_text_part_get_text_with_encoding (GMimeTextPart *mime_part, char **used_charset, GError **error)
{
	GMimeContentType *type;
	GError *local_error = NULL;
	const char *encoding, *converter;
	GMimeStream *stream;
	GByteArray *buffer;
	iconv_t decoder;
	gboolean replace;
	char *charset, *text = NULL;

	if (used_charset)
		*used_charset = NULL;

	g_return_val_if_fail (GMIME_IS_TEXT_PART (mime_part), NULL);
	g_return_val_if_fail (error == NULL || *error == NULL, NULL);

	if (!g_mime_part_get_content ((GMimePart *) mime_part)) {
		if (used_charset)
			*used_charset = g_strdup ("us-ascii");
		return g_strdup ("");
	}

	type = g_mime_entity_get_content_type ((GMimeEntity *) mime_part);
	charset = g_mime_content_type_get_charset_encoding (type, &local_error);

	if (local_error) {
		g_propagate_error (error, local_error);
		return NULL;
	}

	if (!(stream = text_part_decode_content (mime_part, error))) {
		g_free (charset);
		return NULL;
	}

	buffer = g_mime_stream_mem_get_byte_array ((GMimeStreamMem *) stream);
	encoding = charset;
	replace = charset != NULL;

	if (!encoding) {
		if (g_mime_charset_try_get_bom_encoding (buffer->data, buffer->len, &encoding)) {
			replace = strcmp (encoding, "utf-8") != 0;
		} else {
			if (g_mime_text_part_is_html (mime_part))
				charset = text_part_detect_html_charset ((const char *) buffer->data, buffer->len,
									&replace, &local_error);
			if (local_error) {
				g_object_unref (stream);
				g_propagate_error (error, local_error);
				return NULL;
			}
			encoding = charset ? charset : "utf-8";
		}
	}

	if ((decoder = _g_mime_charset_open_text_decoder (encoding, &converter, &local_error)) != (iconv_t) -1) {
		text = text_part_decode_text (buffer, decoder, replace ? converter : NULL, &local_error);
		g_mime_iconv_close (decoder);

		/* Only strict UTF-8 selection can trigger the Latin-1 retry. */
		if (!replace && (g_error_matches (local_error, G_CONVERT_ERROR, G_CONVERT_ERROR_ILLEGAL_SEQUENCE) ||
				 g_error_matches (local_error, G_CONVERT_ERROR, G_CONVERT_ERROR_PARTIAL_INPUT))) {
			g_clear_error (&local_error);
			encoding = "iso-8859-1";

			if ((decoder = _g_mime_charset_open_text_decoder (encoding, NULL, &local_error)) != (iconv_t) -1) {
				text = text_part_decode_text (buffer, decoder, NULL, &local_error);
				g_mime_iconv_close (decoder);
			}
		}
	}

	if (text && used_charset)
		*used_charset = g_strdup (encoding);

	g_object_unref (stream);
	g_free (charset);

	if (local_error)
		g_propagate_error (error, local_error);

	return text;
}


/**
 * g_mime_text_part_get_text:
 * @mime_part: a #GMimeTextPart
 *
 * Gets the text content of the @mime_part as a string.
 * Generic UTF-16 and UTF-32 charsets use little-endian bytes, matching
 * g_mime_text_part_set_text_with_charset().
 *
 * Returns: a newly allocated string containing the utf-8 encoded text content.
 **/
char *
g_mime_text_part_get_text (GMimeTextPart *mime_part)
{
	GMimeContentType *content_type;
	GMimeStream *filtered, *stream;
	GMimeContent *content;
	GMimeFilter *filter;
	const char *charset;
	GByteArray *buf;
	
	g_return_val_if_fail (GMIME_IS_TEXT_PART (mime_part), NULL);
	
	if (!(content = g_mime_part_get_content ((GMimePart *) mime_part)))
		return g_strdup ("");
	
	content_type = g_mime_entity_get_content_type ((GMimeEntity *) mime_part);
	stream = g_mime_stream_mem_new ();
	
	if ((charset = g_mime_content_type_get_parameter (content_type, "charset")) != NULL &&
	    (filter = g_mime_filter_charset_new (_g_mime_charset_get_text_converter (charset), "utf-8")) != NULL) {
		filtered = g_mime_stream_filter_new (stream);
		g_mime_stream_filter_add ((GMimeStreamFilter *) filtered, filter);
		g_object_unref (filter);
		
		g_mime_content_decode_to_stream (content, filtered);
		g_mime_stream_flush (filtered);
		g_object_unref (filtered);
	} else {
		g_mime_content_decode_to_stream (content, stream);
	}
	
	g_mime_stream_write (stream, "", 1);
	
	buf = g_mime_stream_mem_get_byte_array ((GMimeStreamMem *) stream);
	g_mime_stream_mem_set_owner ((GMimeStreamMem *) stream, FALSE);
	g_object_unref (stream);
	
	return (char *) g_byte_array_free (buf, FALSE);
}
