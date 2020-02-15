/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*  GMime
 *  Copyright (C) 2000-2020 Jeffrey Stedfast
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
#include "gmime-stream-mem.h"
#include "gmime-stream-filter.h"
#include "gmime-filter-charset.h"
#include "gmime-charset.h"

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
	g_mime_object_set_content_type ((GMimeObject *) mime_part, content_type);
	g_object_unref (content_type);
	
	return mime_part;
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
	
	content_type = g_mime_object_get_content_type ((GMimeObject *) mime_part);
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
	
	content_type = g_mime_object_get_content_type ((GMimeObject *) mime_part);
	
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
	GMimeDataWrapper *content;
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
	
	content_type = g_mime_object_get_content_type ((GMimeObject *) mime_part);
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
	
	content = g_mime_data_wrapper_new_with_stream (stream, GMIME_CONTENT_ENCODING_DEFAULT);
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
	GMimeDataWrapper *content;
	GMimeFilter *filter;
	const char *charset;
	GByteArray *buf;
	
	g_return_val_if_fail (GMIME_IS_TEXT_PART (mime_part), NULL);
	
	if (!(content = g_mime_part_get_content ((GMimePart *) mime_part)))
		return NULL;
	
	content_type = g_mime_object_get_content_type ((GMimeObject *) mime_part);
	stream = g_mime_stream_mem_new ();
	
	if ((charset = g_mime_content_type_get_parameter (content_type, "charset")) != NULL &&
	    (filter = g_mime_filter_charset_new (charset, "utf-8")) != NULL) {
		filtered = g_mime_stream_filter_new (stream);
		g_mime_stream_filter_add ((GMimeStreamFilter *) filtered, filter);
		g_object_unref (filter);
		
		g_mime_data_wrapper_write_to_stream (content, filtered);
		g_mime_stream_flush (filtered);
		g_object_unref (filtered);
	} else {
		g_mime_data_wrapper_write_to_stream (content, stream);
	}
	
	g_mime_stream_write (stream, "", 1);
	
	buf = g_mime_stream_mem_get_byte_array ((GMimeStreamMem *) stream);
	g_mime_stream_mem_set_owner ((GMimeStreamMem *) stream, FALSE);
	g_object_unref (stream);
	
	return (char *) g_byte_array_free (buf, FALSE);
}
