/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 *  Authors: Jeffrey Stedfast <fejj@helixcode.com>
 *
 *  Copyright 2000 Helix Code, Inc. (www.helixcode.com)
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
#endif /* __cplusplus }*/

#include <glib.h>
#include <stdio.h>
#include "gmime-param.h"
#include "gmime-content-type.h"

typedef enum {
        GMIME_PART_ENCODING_DEFAULT,
        GMIME_PART_ENCODING_7BIT,
        GMIME_PART_ENCODING_8BIT,
        GMIME_PART_ENCODING_BASE64,
        GMIME_PART_ENCODING_QUOTEDPRINTABLE,
        GMIME_PART_NUM_ENCODINGS
} GMimePartEncodingType;

typedef struct _GMimePartDispositionParam GMimePartDispositionParam;

struct _GMimePartDisposition {
	gchar *disposition;
	GList *params;     /* of type GMimeParam */
	GHashTable *param_hash;
};

typedef struct _GMimePartDisposition GMimePartDisposition;

struct _GMimePart {
	GMimeContentType *mime_type;
	GMimePartEncodingType encoding;
	GMimePartDisposition *disposition;
	gchar *description;
	gchar *content_id;
	gchar *content_md5;
	gchar *content_location;
	
	GByteArray *content;
	
	GList *children;   /* of type GMimePart */
};

typedef struct _GMimePart GMimePart;

typedef void (*GMimePartFunc) (GMimePart *part, gpointer data);

/* constructors / destructors */

GMimePart *g_mime_part_new (void);
GMimePart *g_mime_part_new_with_type (const gchar *type, const gchar *subtype);
void g_mime_part_destroy (GMimePart *mime_part);

/* accessor functions */

void g_mime_part_set_content_description (GMimePart *mime_part, const gchar *description);
const gchar *g_mime_part_get_content_description (const GMimePart *mime_part);

void g_mime_part_set_content_id (GMimePart *mime_part, const gchar *content_id);
const gchar *g_mime_part_get_content_id (GMimePart *mime_part);

void g_mime_part_set_content_md5 (GMimePart *mime_part, const gchar *content_md5);
const gchar *g_mime_part_get_content_md5 (GMimePart *mime_part);

void g_mime_part_set_content_location (GMimePart *mime_part, const gchar *content_location);
const gchar *g_mime_part_get_content_location (GMimePart *mime_part);

void g_mime_part_set_content_type (GMimePart *mime_part, GMimeContentType *mime_type);
const GMimeContentType *g_mime_part_get_content_type (GMimePart *mime_part);

void g_mime_part_set_encoding (GMimePart *mime_part, GMimePartEncodingType encoding);
GMimePartEncodingType g_mime_part_get_encoding (GMimePart *mime_part);
const gchar *g_mime_part_encoding_to_string (GMimePartEncodingType encoding);
GMimePartEncodingType g_mime_part_encoding_from_string (const gchar *encoding);

void g_mime_part_set_content_disposition (GMimePart *mime_part, const gchar *disposition);
const gchar *g_mime_part_get_content_disposition (GMimePart *mime_part);

void g_mime_part_add_content_disposition_parameter (GMimePart *mime_part, const gchar *name, const gchar *value);
const gchar *g_mime_part_get_content_disposition_parameter (GMimePart *mime_part, const gchar *name);

void g_mime_part_set_filename (GMimePart *mime_part, const gchar *filename);
const gchar *g_mime_part_get_filename (GMimePart *mime_part);

void g_mime_part_set_boundary (GMimePart *mime_part, const gchar *boundary);
const gchar *g_mime_part_get_boundary (GMimePart *mime_part);

void g_mime_part_set_content (GMimePart *mime_part, const char *content, guint len);
void g_mime_part_set_pre_encoded_content (GMimePart *mime_part, const char *content,
					  guint len, GMimePartEncodingType encoding);
const gchar *g_mime_part_get_content (const GMimePart *mime_part, guint *len);

void g_mime_part_add_subpart (GMimePart *mime_part, GMimePart *subpart);
#define g_mime_part_add_child(mime_part, child) g_mime_part_add_subpart (mime_part, child)

/* utility functions */
gchar *g_mime_part_to_string (GMimePart *mime_part, gboolean toplevel);

void g_mime_part_foreach (GMimePart *mime_part, GMimePartFunc callback, gpointer data);

const GMimePart *g_mime_part_get_subpart_from_content_id (GMimePart *mime_part, const gchar *content_id);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* __GMIME_PART_H__ */
