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
#include <sys/types.h>
#include <string.h>

#include "gmime-text-part.h"
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
		return NULL;
	
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
