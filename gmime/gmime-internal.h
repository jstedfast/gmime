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


#ifndef __GMIME_INTERNAL_H__
#define __GMIME_INTERNAL_H__

#include <gmime/gmime-format-options.h>
#include <gmime/gmime-parser-options.h>
#include <gmime/gmime-object.h>
#include <gmime/gmime-events.h>
#include <gmime/gmime-utils.h>

G_BEGIN_DECLS

typedef enum {
	GMIME_HEADER_LIST_CHANGED_ACTION_ADDED,
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

/* GMimeParserOptions */
G_GNUC_INTERNAL void g_mime_parser_options_init (void);
G_GNUC_INTERNAL void g_mime_parser_options_shutdown (void);
G_GNUC_INTERNAL void _g_mime_parser_options_warn (GMimeParserOptions *options, gint64 offset, GMimeParserWarning errcode,
						  const gchar *item);

/* GMimeHeader */
//G_GNUC_INTERNAL void _g_mime_header_set_raw_value (GMimeHeader *header, const char *raw_value);
G_GNUC_INTERNAL void _g_mime_header_set_offset (GMimeHeader *header, gint64 offset);

/* GMimeHeaderList */
G_GNUC_INTERNAL GMimeParserOptions *_g_mime_header_list_get_options (GMimeHeaderList *headers);
G_GNUC_INTERNAL void _g_mime_header_list_set_options (GMimeHeaderList *headers, GMimeParserOptions *options);
G_GNUC_INTERNAL void _g_mime_header_list_append (GMimeHeaderList *headers, const char *name, const char *raw_name,
						 const char *raw_value, gint64 offset);
G_GNUC_INTERNAL void _g_mime_header_list_set (GMimeHeaderList *headers, const char *name, const char *raw_value);

/* GMimeObject */
G_GNUC_INTERNAL void _g_mime_object_block_header_list_changed (GMimeObject *object);
G_GNUC_INTERNAL void _g_mime_object_unblock_header_list_changed (GMimeObject *object);
G_GNUC_INTERNAL void _g_mime_object_set_content_type (GMimeObject *object, GMimeContentType *content_type);
G_GNUC_INTERNAL void _g_mime_object_append_header (GMimeObject *object, const char *name, const char *raw_name,
						   const char *raw_value, gint64 offset);

/* GMimeContentType */
G_GNUC_INTERNAL GMimeContentType *_g_mime_content_type_parse (GMimeParserOptions *options, const char *str, gint64 offset);

/* GMimeParamList */
G_GNUC_INTERNAL GMimeParamList *_g_mime_param_list_parse (GMimeParserOptions *options, const char *str, gint64 offset);

/* GMimeContentDisposition */
G_GNUC_INTERNAL GMimeContentDisposition *_g_mime_content_disposition_parse (GMimeParserOptions *options, const char *str,
									    gint64 offset);

/* utils */
G_GNUC_INTERNAL char *_g_mime_utils_unstructured_header_fold (GMimeParserOptions *options, GMimeFormatOptions *format,
							      const char *field, const char *value);
G_GNUC_INTERNAL char *_g_mime_utils_structured_header_fold (GMimeParserOptions *options, GMimeFormatOptions *format,
							    const char *field, const char *value);
G_GNUC_INTERNAL char *_g_mime_utils_header_decode_text (GMimeParserOptions *options, const char *text, const char **charset,
							gint64 offset);
G_GNUC_INTERNAL char *_g_mime_utils_header_decode_phrase (GMimeParserOptions *options, const char *text, const char **charset,
							  gint64 offset);

/* InternetAddressList */
G_GNUC_INTERNAL InternetAddressList *_internet_address_list_parse (GMimeParserOptions *options, const char *str, gint64 offset);

G_END_DECLS

#endif /* __GMIME_INTERNAL_H__ */
