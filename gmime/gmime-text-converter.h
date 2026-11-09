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

#ifndef __GMIME_TEXT_CONVERTER_H__
#define __GMIME_TEXT_CONVERTER_H__

#include <gmime/gmime-text-part.h>
#include <gmime/gmime-stream.h>

G_BEGIN_DECLS

#define GMIME_TYPE_TEXT_CONVERTER (g_mime_text_converter_get_type ())

/**
 * GMimeTextConverter:
 *
 * An abstract, synchronous text converter with UTF-8 string APIs and selectable
 * stream charsets.
 **/
G_DECLARE_DERIVABLE_TYPE (GMimeTextConverter, g_mime_text_converter, GMIME, TEXT_CONVERTER, GObject)

/**
 * GMimeTextConverterClass:
 * @parent_class: parent class
 * @input_format: the input text format
 * @output_format: the output text format
 * @convert: converts UTF-8 streams, including the configured
 *   header/footer, reporting invalid input and I/O failures through @error;
 *   does not flush or close streams
 *
 * Base class for text converters. Implementations must not retain stream
 * arguments after returning, and must leave the converter reusable on failure.
 **/
struct _GMimeTextConverterClass {
	GObjectClass parent_class;

	GMimeTextFormat input_format;
	GMimeTextFormat output_format;

	gboolean (*convert) (GMimeTextConverter *converter, GMimeStream *input, GMimeStream *output, GError **error);
};

GMimeTextFormat g_mime_text_converter_get_input_format (GMimeTextConverter *converter);
GMimeTextFormat g_mime_text_converter_get_output_format (GMimeTextConverter *converter);

const char *g_mime_text_converter_get_input_charset (GMimeTextConverter *converter);
gboolean g_mime_text_converter_set_input_charset (GMimeTextConverter *converter, const char *charset, GError **error);

const char *g_mime_text_converter_get_output_charset (GMimeTextConverter *converter);
gboolean g_mime_text_converter_set_output_charset (GMimeTextConverter *converter, const char *charset, GError **error);

gboolean g_mime_text_converter_get_detect_encoding_from_byte_order_mark (GMimeTextConverter *converter);
void g_mime_text_converter_set_detect_encoding_from_byte_order_mark (GMimeTextConverter *converter, gboolean detect);

gboolean g_mime_text_converter_get_write_bom (GMimeTextConverter *converter);
void g_mime_text_converter_set_write_bom (GMimeTextConverter *converter, gboolean write_bom);

gsize g_mime_text_converter_get_input_stream_buffer_size (GMimeTextConverter *converter);
void g_mime_text_converter_set_input_stream_buffer_size (GMimeTextConverter *converter, gsize size);

gsize g_mime_text_converter_get_output_stream_buffer_size (GMimeTextConverter *converter);
void g_mime_text_converter_set_output_stream_buffer_size (GMimeTextConverter *converter, gsize size);

const char *g_mime_text_converter_get_header (GMimeTextConverter *converter);
void g_mime_text_converter_set_header (GMimeTextConverter *converter, const char *header);

const char *g_mime_text_converter_get_footer (GMimeTextConverter *converter);
void g_mime_text_converter_set_footer (GMimeTextConverter *converter, const char *footer);

gboolean g_mime_text_converter_convert (GMimeTextConverter *converter, GMimeStream *input, GMimeStream *output,
					GError **error);
char *g_mime_text_converter_convert_to_string (GMimeTextConverter *converter, const char *text, gssize length,
					       GError **error);

#define GMIME_TYPE_TEXT_TO_TEXT (g_mime_text_to_text_get_type ())
#define GMIME_TYPE_TEXT_TO_FLOWED (g_mime_text_to_flowed_get_type ())
#define GMIME_TYPE_FLOWED_TO_TEXT (g_mime_flowed_to_text_get_type ())

/**
 * GMimeTextToText:
 *
 * Copies UTF-8 text without changing its newlines.
 **/
G_DECLARE_FINAL_TYPE (GMimeTextToText, g_mime_text_to_text, GMIME, TEXT_TO_TEXT, GMimeTextConverter)

/**
 * GMimeTextToFlowed:
 *
 * Converts plain text to format=flowed; delsp=yes.
 **/
G_DECLARE_FINAL_TYPE (GMimeTextToFlowed, g_mime_text_to_flowed, GMIME, TEXT_TO_FLOWED, GMimeTextConverter)

/**
 * GMimeFlowedToText:
 *
 * Unwraps format=flowed text.
 **/
G_DECLARE_FINAL_TYPE (GMimeFlowedToText, g_mime_flowed_to_text, GMIME, FLOWED_TO_TEXT, GMimeTextConverter)

GMimeTextConverter *g_mime_text_to_text_new (void);
GMimeTextConverter *g_mime_text_to_flowed_new (void);
GMimeTextConverter *g_mime_flowed_to_text_new (void);

gboolean g_mime_flowed_to_text_get_delete_space (GMimeFlowedToText *converter);
void g_mime_flowed_to_text_set_delete_space (GMimeFlowedToText *converter, gboolean delete_space);

G_END_DECLS

#endif /* __GMIME_TEXT_CONVERTER_H__ */
