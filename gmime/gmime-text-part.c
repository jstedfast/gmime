/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*  GMime
 *  Copyright (C) 2000-2022 Jeffrey Stedfast
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
#include "gmime-charset.h"
#include "gmime-common.h"
#include "gmime-internal.h"

#define d(x)


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
static void g_mime_text_part_init (GMimeTextPart *mime_part, GMimeTextPartClass *klass);
static void g_mime_text_part_finalize (GObject *object);


static GMimePartClass *parent_class = NULL;

static void
text_part_accept (GMimeEntity *object, GMimeVisitor *visitor)
{
	g_mime_visitor_visit_text_part (visitor, (GMimeTextPart *) object);
}


GType
g_mime_text_part_get_type (void)
{
	static GType type = 0;
	
	if (!type) {
		static const GTypeInfo info = {
			sizeof (GMimeTextPartClass),
			NULL, /* base_class_init */
			NULL, /* base_class_finalize */
			(GClassInitFunc) g_mime_text_part_class_init,
			NULL, /* class_finalize */
			NULL, /* class_data */
			sizeof (GMimeTextPart),
			0,    /* n_preallocs */
			(GInstanceInitFunc) g_mime_text_part_init,
		};
		
		type = g_type_register_static (GMIME_TYPE_PART, "GMimeTextPart", &info, 0);
	}
	
	return type;
}


static void
g_mime_text_part_class_init (GMimeTextPartClass *klass)
{
	GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
	
	parent_class = g_type_class_ref (GMIME_TYPE_PART);
	
	gobject_class->finalize = g_mime_text_part_finalize;
	GMIME_ENTITY_CLASS (klass)->accept = text_part_accept;
}

static void
g_mime_text_part_init (GMimeTextPart *mime_part, GMimeTextPartClass *klass)
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
 * ignoring ASCII case and surrounding Unicode whitespace in the parameter.
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
 * ignoring ASCII case and surrounding Unicode whitespace.
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

	bytes = _g_mime_charset_encode_text (charset, text, &length, &name, error);
	if (!bytes)
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
	GMimeContent *content;
	GMimeStream *stream;
	GMimeFilter *filter;
	GByteArray *buffer;
	GIConv decoder;
	char *text, *output, *normalized;
	size_t outlen, prespace;
	gsize length;
	int errsv;

	g_return_val_if_fail (GMIME_IS_TEXT_PART (mime_part), NULL);
	g_return_val_if_fail (charset != NULL, NULL);
	g_return_val_if_fail (error == NULL || *error == NULL, NULL);

	/* Resolve the override even for absent content, before reading any
	 * bytes. A bad charset must not become an empty success or an I/O error. */
	if ((decoder = _g_mime_charset_open_text_decoder (charset, error)) == (GIConv) -1)
		return NULL;
	if (!(content = g_mime_part_get_content ((GMimePart *) mime_part))) {
		g_iconv_close (decoder);
		return g_strdup ("");
	}
	if (!g_mime_content_get_stream (content)) {
		g_iconv_close (decoder);
		g_set_error_literal (error, GMIME_ERROR, GMIME_ERROR_INVALID_OPERATION,
				     "Cannot decode text content without a backing stream");
		return NULL;
	}

	stream = g_mime_stream_mem_new ();
	errno = 0;
	if (g_mime_content_decode_to_stream (content, stream) == -1) {
		errsv = errno;
		g_object_unref (stream);
		g_iconv_close (decoder);
		g_set_error (error, GMIME_ERROR, errsv ? errsv : GMIME_ERROR_GENERAL,
			     "Failed to decode text content: %s",
			     errsv ? g_strerror (errsv) : "unknown I/O error");
		return NULL;
	}

	/* The legacy charset filter skips malformed bytes. Use checked
	 * conversion here so a valid prefix cannot hide an invalid suffix. */
	buffer = g_mime_stream_mem_get_byte_array ((GMimeStreamMem *) stream);
	text = g_convert_with_iconv (buffer->len ? (const char *) buffer->data : "", buffer->len,
				     decoder, NULL, &length, error);
	g_iconv_close (decoder);
	g_object_unref (stream);
	if (!text)
		return NULL;
	if (memchr (text, '\0', length)) {
		g_set_error_literal (error, G_CONVERT_ERROR, G_CONVERT_ERROR_EMBEDDED_NUL,
				     "Decoded text contains an embedded NUL character");
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


/**
 * g_mime_text_part_get_text:
 * @mime_part: a #GMimeTextPart
 *
 * Gets the text content of the @mime_part as a string.
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
	    (filter = g_mime_filter_charset_new (charset, "utf-8")) != NULL) {
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
