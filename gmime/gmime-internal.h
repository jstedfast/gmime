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


#ifndef __GMIME_INTERNAL_H__
#define __GMIME_INTERNAL_H__

#include <gmime/gmime-format-options.h>
#include <gmime/gmime-parser-options.h>
#include <gmime/gmime-parser.h>
#include <gmime/gmime-entity.h>
#include <gmime/gmime-events.h>
#include <gmime/gmime-utils.h>
#include <gmime/gmime-stream-filter.h>
#include <gmime/gmime-text-part.h>
#include <gmime/gmime-multipart.h>

G_BEGIN_DECLS

typedef enum {
	GMIME_HEADER_LIST_CHANGED_ACTION_ADDED,
	GMIME_HEADER_LIST_CHANGED_ACTION_INSERTED,
	GMIME_HEADER_LIST_CHANGED_ACTION_CHANGED,
	GMIME_HEADER_LIST_CHANGED_ACTION_REMOVED,
	GMIME_HEADER_LIST_CHANGED_ACTION_CLEARED
} GMimeHeaderListChangedAction;

typedef struct {
	GMimeHeaderListChangedAction action;
	GMimeHeader *header;
} GMimeHeaderListChangedEventArgs;

/* GMimeFormatOptions */
G_GNUC_INTERNAL void g_mime_format_options_init (void);
G_GNUC_INTERNAL void g_mime_format_options_shutdown (void);
G_GNUC_INTERNAL GMimeFormatOptions *_g_mime_format_options_clone (GMimeFormatOptions *options, gboolean hidden);
G_GNUC_INTERNAL gboolean _g_mime_format_options_get_ensure_newline (GMimeFormatOptions *options);
G_GNUC_INTERNAL void _g_mime_format_options_set_ensure_newline (GMimeFormatOptions *options, gboolean ensure);

/* GMimeParserOptions */
G_GNUC_INTERNAL void g_mime_parser_options_init (void);
G_GNUC_INTERNAL void g_mime_parser_options_shutdown (void);
G_GNUC_INTERNAL void _g_mime_parser_options_warn (GMimeParserOptions *options, gint64 offset, GMimeParserWarning errcode,
						  const gchar *item);

/* GMimeParser */
G_GNUC_INTERNAL GMimeStream *_g_mime_parser_read_to_eos (GMimeParser *parser, GError **error);

/* GMimeHeader */
G_GNUC_INTERNAL GMimeContentEncoding _g_mime_header_get_content_encoding (GMimeHeader *header);
G_GNUC_INTERNAL GMimeHeader *_g_mime_header_new_from_reader (GMimeParserOptions *options, const guint8 *buffer,
							  gsize length, gsize name_length, gsize field_length,
							  gboolean invalid, gint64 offset);
//G_GNUC_INTERNAL void _g_mime_header_set_raw_value (GMimeHeader *header, const char *raw_value);
G_GNUC_INTERNAL void _g_mime_header_set_offset (GMimeHeader *header, gint64 offset);

/* GMimeHeaderList */
G_GNUC_INTERNAL GMimeParserOptions *_g_mime_header_list_get_options (GMimeHeaderList *headers);
G_GNUC_INTERNAL void _g_mime_header_list_set_options (GMimeHeaderList *headers, GMimeParserOptions *options);
G_GNUC_INTERNAL void _g_mime_header_list_append (GMimeHeaderList *headers, const char *name, const char *raw_name,
						 const char *raw_value, gint64 offset);
G_GNUC_INTERNAL void _g_mime_header_list_set (GMimeHeaderList *headers, const char *name, const char *raw_value);
G_GNUC_INTERNAL ssize_t _g_mime_header_list_write_to_stream (GMimeHeaderList *headers, GMimeFormatOptions *options,
							  GMimeStream *stream);

/* GMimeContentEncoding */
G_GNUC_INTERNAL GMimeContentEncoding _g_mime_content_encoding_parse (const char *text, gsize length);

/* GMimeStream */
G_GNUC_INTERNAL ssize_t _g_mime_stream_write_all (GMimeStream *stream, const char *buffer, size_t len);

/* GMimeStreamFilter */
G_GNUC_INTERNAL ssize_t _g_mime_stream_filter_get_written (GMimeStreamFilter *stream);

/* GMimeEntity */
G_GNUC_INTERNAL gboolean _g_mime_is_content_uri (const char *uri, gboolean absolute);
G_GNUC_INTERNAL void _g_mime_entity_block_header_list_changed (GMimeEntity *object);
G_GNUC_INTERNAL void _g_mime_entity_unblock_header_list_changed (GMimeEntity *object);
G_GNUC_INTERNAL void _g_mime_entity_set_content_type (GMimeEntity *object, GMimeContentType *content_type);
G_GNUC_INTERNAL void _g_mime_entity_append_header (GMimeEntity *object, const char *name, const char *raw_name,
						   const char *raw_value, gint64 offset);

/* GMimeTextPart */
G_GNUC_INTERNAL gboolean _g_mime_text_part_is_format (GMimeTextPart *part, GMimeTextFormat format);

/* GMimeMultipart */
G_GNUC_INTERNAL void _g_mime_multipart_initialize_from_reader (GMimeMultipart *multipart);
G_GNUC_INTERNAL void _g_mime_multipart_add_with_boundary (GMimeMultipart *multipart, GMimeEntity *part, GBytes *boundary);
G_GNUC_INTERNAL void _g_mime_multipart_set_raw_text (GMimeMultipart *multipart, GBytes *bytes, gboolean epilogue);

/* GMimePart */
G_GNUC_INTERNAL GMimeHeaderList *_g_mime_part_load_fields (GMimePart *part, const char *name, GError **error);
G_GNUC_INTERNAL void _g_mime_part_serialize_fields (GMimePart *part, GMimeHeaderList *fields, const char *name);

/* GMimeContentType */
G_GNUC_INTERNAL GMimeContentType *_g_mime_content_type_parse (GMimeParserOptions *options, const char *str, gint64 offset);

/* GMimeParamList */
G_GNUC_INTERNAL GMimeParamList *_g_mime_param_list_clone (GMimeParamList *list);
G_GNUC_INTERNAL GMimeParamList *_g_mime_param_list_parse (GMimeParserOptions *options, const char *str, gint64 offset);
G_GNUC_INTERNAL GMimeParamList *_g_mime_param_list_parse_full (GMimeParserOptions *options, const char *text,
							     const char *in, GError **error);

/* GMimeContentDisposition */
G_GNUC_INTERNAL GMimeContentDisposition *_g_mime_content_disposition_parse (GMimeParserOptions *options, const char *str,
									    gint64 offset);

/* GMimeCharset */
G_GNUC_INTERNAL char *_g_mime_charset_get_mime_name (const char *charset);
G_GNUC_INTERNAL char *_g_mime_charset_encode_text (const char *charset, const char *text, gsize *length,
						 char **mime_name, GError **error);
G_GNUC_INTERNAL GIConv _g_mime_charset_open_text_decoder (const char *charset, GError **error);
G_GNUC_INTERNAL int _g_mime_charset_get_codepage (const char *charset);

/* utils */
G_GNUC_INTERNAL char *_g_mime_utils_convert_from_utf8 (const char *text, gsize length, const char **charset,
						     gsize *converted_length);
G_GNUC_INTERNAL char *_g_mime_utils_unstructured_header_fold (GMimeParserOptions *options, GMimeFormatOptions *format,
							      const char *field, const char *value);
G_GNUC_INTERNAL char *_g_mime_utils_structured_header_fold (GMimeParserOptions *options, GMimeFormatOptions *format,
							    const char *field, const char *value);
G_GNUC_INTERNAL char *_g_mime_utils_header_decode_text (GMimeParserOptions *options, const char *text, const char **charset,
							gint64 offset);
G_GNUC_INTERNAL char *_g_mime_utils_header_decode_phrase (GMimeParserOptions *options, const char *text, const char **charset,
							  gint64 offset);

/* GMimeInternetAddressList */
G_GNUC_INTERNAL GMimeInternetAddressList *_g_mime_internet_address_list_parse (GMimeParserOptions *options, const char *str, gint64 offset);
G_GNUC_INTERNAL void _g_mime_internet_address_list_append_parse (GMimeInternetAddressList *list, GMimeParserOptions *options, const char *str, gint64 offset);

G_END_DECLS

#endif /* __GMIME_INTERNAL_H__ */
