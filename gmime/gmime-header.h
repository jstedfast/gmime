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


#ifndef __GMIME_HEADER_H__
#define __GMIME_HEADER_H__

#include <glib.h>
#include <gmime/gmime-format-options.h>
#include <gmime/gmime-parser-options.h>
#include <gmime/gmime-stream.h>

G_BEGIN_DECLS

#define GMIME_TYPE_HEADER                  (g_mime_header_get_type ())
#define GMIME_HEADER(obj)                  (G_TYPE_CHECK_INSTANCE_CAST ((obj), GMIME_TYPE_HEADER, GMimeHeader))
#define GMIME_HEADER_CLASS(klass)          (G_TYPE_CHECK_CLASS_CAST ((klass), GMIME_TYPE_HEADER, GMimeHeaderClass))
#define GMIME_IS_HEADER(obj)               (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GMIME_TYPE_HEADER))
#define GMIME_IS_HEADER_CLASS(klass)       (G_TYPE_CHECK_CLASS_TYPE ((klass), GMIME_TYPE_HEADER))
#define GMIME_HEADER_GET_CLASS(obj)        (G_TYPE_INSTANCE_GET_CLASS ((obj), GMIME_TYPE_HEADER, GMimeHeaderClass))

#define GMIME_TYPE_HEADER_LIST             (g_mime_header_list_get_type ())
#define GMIME_HEADER_LIST(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), GMIME_TYPE_HEADER_LIST, GMimeHeaderList))
#define GMIME_HEADER_LIST_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), GMIME_TYPE_HEADER_LIST, GMimeHeaderListClass))
#define GMIME_IS_HEADER_LIST(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GMIME_TYPE_HEADER_LIST))
#define GMIME_IS_HEADER_LIST_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), GMIME_TYPE_HEADER_LIST))
#define GMIME_HEADER_LIST_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), GMIME_TYPE_HEADER_LIST, GMimeHeaderListClass))

typedef struct _GMimeHeader GMimeHeader;
typedef struct _GMimeHeaderClass GMimeHeaderClass;

typedef struct _GMimeHeaderList GMimeHeaderList;
typedef struct _GMimeHeaderListClass GMimeHeaderListClass;


/**
 * GMimeHeaderRawValueFormatter:
 * @header: a #GMimeHeader
 * @options: a #GMimeFormatOptions
 * @value: an unencoded header value
 * @charset: a charset
 *
 * Function callback for encoding and formatting a header value.
 *
 * Returns: the encoded and formatted raw header value.
 **/
typedef char * (* GMimeHeaderRawValueFormatter) (GMimeHeader *header, GMimeFormatOptions *options,
						 const char *value, const char *charset);

char *g_mime_header_format_content_disposition (GMimeHeader *header, GMimeFormatOptions *options, const char *value, const char *charset);
char *g_mime_header_format_content_type (GMimeHeader *header, GMimeFormatOptions *options, const char *value, const char *charset);
char *g_mime_header_format_disposition_notification_options (GMimeHeader *header, GMimeFormatOptions *options, const char *value, const char *charset);
char *g_mime_header_format_dkim_signature (GMimeHeader *header, GMimeFormatOptions *options, const char *value, const char *charset);
char *g_mime_header_format_mailing_list_command (GMimeHeader *header, GMimeFormatOptions *options, const char *value, const char *charset);
char *g_mime_header_format_message_id (GMimeHeader *header, GMimeFormatOptions *options, const char *value, const char *charset);
char *g_mime_header_format_references (GMimeHeader *header, GMimeFormatOptions *options, const char *value, const char *charset);
char *g_mime_header_format_addrlist (GMimeHeader *header, GMimeFormatOptions *options, const char *value, const char *charset);
char *g_mime_header_format_received (GMimeHeader *header, GMimeFormatOptions *options, const char *value, const char *charset);
char *g_mime_header_format_newsgroups (GMimeHeader *header, GMimeFormatOptions *options, const char *value, const char *charset);
char *g_mime_header_format_default (GMimeHeader *header, GMimeFormatOptions *options, const char *value, const char *charset);


/**
 * GMimeHeader:
 * @name: the name of the header
 * @value: the unfolded value of the header
 *
 * A message or mime-part header.
 **/
struct _GMimeHeader {
	/* <private> */
	GObject parent_object;
	
	char *name, *value;
	
	/* <private> */
	GMimeHeaderRawValueFormatter formatter;
	GMimeParserOptions *options;
	gboolean reformat;
	gpointer changed;
	char *raw_value;
	char *raw_name;
	char *charset;
	gint64 offset;
	gboolean owns_options;
	gboolean explicit_raw_value;
	GBytes *raw_field_bytes;
	GBytes *raw_value_bytes;
	gboolean invalid;
};

struct _GMimeHeaderClass {
	GObjectClass parent_class;
	
};


GType g_mime_header_get_type (void);

GMimeHeader *g_mime_header_new (const char *name, const char *value, const char *charset);
GMimeHeader *g_mime_header_new_full (const char *name, const char *value, const char *charset, GError **error);
GMimeHeader *g_mime_header_clone (GMimeHeader *header);

const char *g_mime_header_get_name (GMimeHeader *header);
const char *g_mime_header_get_raw_name (GMimeHeader *header);

const char *g_mime_header_get_value (GMimeHeader *header);
char *g_mime_header_get_value_with_charset (GMimeHeader *header, const char *charset, GError **error);
void g_mime_header_set_value (GMimeHeader *header, GMimeFormatOptions *options, const char *value, const char *charset);
gboolean g_mime_header_set_value_full (GMimeHeader *header, GMimeFormatOptions *options, const char *value,
				     const char *charset, GError **error);

const char *g_mime_header_get_raw_value (GMimeHeader *header);
void g_mime_header_set_raw_value (GMimeHeader *header, const char *raw_value);

gint64 g_mime_header_get_offset (GMimeHeader *header);

ssize_t g_mime_header_write_to_stream (GMimeHeader *header, GMimeFormatOptions *options, GMimeStream *stream);
char *g_mime_header_to_string (GMimeHeader *header);


/**
 * GMimeHeaderList:
 *
 * A list of message or mime-part headers.
 **/
struct _GMimeHeaderList {
	GObject parent_object;
	
	/* < private > */
	GMimeParserOptions *options;
	gpointer changed;
	GHashTable *hash;
	GPtrArray *array;
	gboolean has_body_separator;
};

struct _GMimeHeaderListClass {
	GObjectClass parent_class;
	
};


GType g_mime_header_list_get_type (void);

GMimeHeaderList *g_mime_header_list_new (GMimeParserOptions *options);
GMimeHeaderList *g_mime_header_list_load (GMimeParserOptions *options, GMimeStream *stream, GError **error);
GMimeHeaderList *g_mime_header_list_load_file (GMimeParserOptions *options, const char *filename, GError **error);

void g_mime_header_list_clear (GMimeHeaderList *headers);
int g_mime_header_list_get_count (GMimeHeaderList *headers);
gboolean g_mime_header_list_contains (GMimeHeaderList *headers, const char *name);
gboolean g_mime_header_list_contains_header (GMimeHeaderList *headers, GMimeHeader *header);
int g_mime_header_list_index_of (GMimeHeaderList *headers, const char *name);
int g_mime_header_list_index_of_header (GMimeHeaderList *headers, GMimeHeader *header);
int g_mime_header_list_last_index_of (GMimeHeaderList *headers, const char *name);
int g_mime_header_list_add (GMimeHeaderList *headers, GMimeHeader *header);
void g_mime_header_list_insert_header (GMimeHeaderList *headers, int index, GMimeHeader *header);
void g_mime_header_list_insert (GMimeHeaderList *headers, int index, const char *name, const char *value, const char *charset);
gboolean g_mime_header_list_insert_full (GMimeHeaderList *headers, int index, const char *name, const char *value,
				       const char *charset, GError **error);
void g_mime_header_list_prepend (GMimeHeaderList *headers, const char *name, const char *value, const char *charset);
gboolean g_mime_header_list_prepend_full (GMimeHeaderList *headers, const char *name, const char *value,
					const char *charset, GError **error);
void g_mime_header_list_append (GMimeHeaderList *headers, const char *name, const char *value, const char *charset);
gboolean g_mime_header_list_append_full (GMimeHeaderList *headers, const char *name, const char *value,
				       const char *charset, GError **error);
void g_mime_header_list_set (GMimeHeaderList *headers, const char *name, const char *value, const char *charset);
const char *g_mime_header_list_get_value (GMimeHeaderList *headers, const char *name);
gboolean g_mime_header_list_set_value_full (GMimeHeaderList *headers, const char *name, const char *value,
					  const char *charset, GError **error);
void g_mime_header_list_replace (GMimeHeaderList *headers, GMimeHeader *header);
gboolean g_mime_header_list_replace_value_full (GMimeHeaderList *headers, const char *name, const char *value,
					      const char *charset, GError **error);
GMimeHeader *g_mime_header_list_get_header (GMimeHeaderList *headers, const char *name);
GMimeHeader *g_mime_header_list_get_header_at (GMimeHeaderList *headers, int index);
void g_mime_header_list_set_header_at (GMimeHeaderList *headers, int index, GMimeHeader *header);
gboolean g_mime_header_list_remove (GMimeHeaderList *headers, const char *name);
gboolean g_mime_header_list_remove_header (GMimeHeaderList *headers, GMimeHeader *header);
void g_mime_header_list_remove_all (GMimeHeaderList *headers, const char *name);
void g_mime_header_list_remove_at (GMimeHeaderList *headers, int index);

ssize_t g_mime_header_list_write_to_stream (GMimeHeaderList *headers, GMimeFormatOptions *options, GMimeStream *stream);
char *g_mime_header_list_to_string (GMimeHeaderList *headers, GMimeFormatOptions *options);

G_END_DECLS

#endif /* __GMIME_HEADER_H__ */
