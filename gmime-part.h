/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 *  Authors: Jeffrey Stedfast <fejj@ximian.com>
 *
 *  Copyright 2000-2002 Ximain, Inc. (www.ximian.com)
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Street #330, Boston, MA 02111-1307, USA.
 *
 */


#ifndef __GMIME_PART_H__
#define __GMIME_PART_H__

#ifdef __cplusplus
extern "C" {
#pragma }
#endif /* __cplusplus */

#include <glib.h>
#include <stdio.h>

#include "gmime-object.h"
#include "gmime-param.h"
#include "gmime-disposition.h"
#include "gmime-data-wrapper.h"

#define GMIME_TYPE_PART            (g_mime_part_get_type ())
#define GMIME_PART(obj)            (GMIME_CHECK_CAST ((obj), GMIME_TYPE_PART, GMimePart))
#define GMIME_PART_CLASS(klass)    (GMIME_CHECK_CLASS_CAST ((klass), GMIME_TYPE_PART, GMimePartClass))
#define GMIME_IS_PART(obj)         (GMIME_CHECK_TYPE ((obj), GMIME_TYPE_PART))
#define GMIME_IS_PART_CLASS(klass) (GMIME_CHECK_CLASS_TYPE ((klass), GMIME_TYPE_PART))
#define GMIME_PART_GET_CLASS(obj)  (GMIME_CHECK_GET_CLASS ((obj), GMIME_TYPE_PART, GMimePartClass))

typedef struct _GMimePart GMimePart;
typedef struct _GMimePartClass GMimePartClass;

struct _GMimePart {
	GMimeObject parent_object;
	
	GMimePartEncodingType encoding;
	GMimeDisposition *disposition;
	char *content_description;
	char *content_location;
	char *content_md5;
	
	GMimeDataWrapper *content;
};

struct _GMimePartClass {
	GMimeObjectClass parent_class;
	
};


GType g_mime_part_get_type (void);

/* constructors */
GMimePart *g_mime_part_new (void);
GMimePart *g_mime_part_new_with_type (const char *type, const char *subtype);

/* accessor functions */
void g_mime_part_set_content_header (GMimePart *mime_part, const char *header, const char *value);
const char *g_mime_part_get_content_header (GMimePart *mime_part, const char *header);

void g_mime_part_set_content_description (GMimePart *mime_part, const char *description);
const char *g_mime_part_get_content_description (const GMimePart *mime_part);

void g_mime_part_set_content_id (GMimePart *mime_part, const char *content_id);
const char *g_mime_part_get_content_id (GMimePart *mime_part);

void g_mime_part_set_content_md5 (GMimePart *mime_part, const char *content_md5);
gboolean g_mime_part_verify_content_md5 (GMimePart *mime_part);
const char *g_mime_part_get_content_md5 (GMimePart *mime_part);

void g_mime_part_set_content_location (GMimePart *mime_part, const char *content_location);
const char *g_mime_part_get_content_location (GMimePart *mime_part);

void g_mime_part_set_content_type (GMimePart *mime_part, GMimeContentType *mime_type);
const GMimeContentType *g_mime_part_get_content_type (GMimePart *mime_part);

void g_mime_part_set_encoding (GMimePart *mime_part, GMimePartEncodingType encoding);
GMimePartEncodingType g_mime_part_get_encoding (GMimePart *mime_part);
const char *g_mime_part_encoding_to_string (GMimePartEncodingType encoding);
GMimePartEncodingType g_mime_part_encoding_from_string (const char *encoding);

void g_mime_part_set_content_disposition_object (GMimePart *mime_part, GMimeDisposition *disposition);
void g_mime_part_set_content_disposition (GMimePart *mime_part, const char *disposition);
const char *g_mime_part_get_content_disposition (GMimePart *mime_part);

void g_mime_part_add_content_disposition_parameter (GMimePart *mime_part, const char *attribute,
						    const char *value);
const char *g_mime_part_get_content_disposition_parameter (GMimePart *mime_part, const char *attribute);

void g_mime_part_set_filename (GMimePart *mime_part, const char *filename);
const char *g_mime_part_get_filename (const GMimePart *mime_part);

void g_mime_part_set_content_byte_array (GMimePart *mime_part, GByteArray *content);
void g_mime_part_set_content (GMimePart *mime_part, const char *content, size_t len);
void g_mime_part_set_pre_encoded_content (GMimePart *mime_part, const char *content,
					  size_t len, GMimePartEncodingType encoding);

/*void g_mime_part_set_content_stream (GMimePart *mime_part, GMimeStream *content);*/

void g_mime_part_set_content_object (GMimePart *mime_part, GMimeDataWrapper *content);
const GMimeDataWrapper *g_mime_part_get_content_object (const GMimePart *mime_part);
const char *g_mime_part_get_content (const GMimePart *mime_part, size_t *len);

/* utility functions */
ssize_t g_mime_part_write_to_stream (GMimePart *mime_part, GMimeStream *stream);
char *g_mime_part_to_string (GMimePart *mime_part);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* __GMIME_PART_H__ */
