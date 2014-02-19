/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*  GMime
 *  Copyright (C) 2000-2014 Jeffrey Stedfast
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


#ifndef __GMIME_PART_H__
#define __GMIME_PART_H__

#include <glib.h>
#include <stdio.h>

#include <gmime/gmime-object.h>
#include <gmime/gmime-encodings.h>
#include <gmime/gmime-filter-best.h>
#include <gmime/gmime-data-wrapper.h>

G_BEGIN_DECLS

#define GMIME_TYPE_PART            (g_mime_part_get_type ())
#define GMIME_PART(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), GMIME_TYPE_PART, GMimePart))
#define GMIME_PART_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), GMIME_TYPE_PART, GMimePartClass))
#define GMIME_IS_PART(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GMIME_TYPE_PART))
#define GMIME_IS_PART_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), GMIME_TYPE_PART))
#define GMIME_PART_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), GMIME_TYPE_PART, GMimePartClass))

typedef struct _GMimePart GMimePart;
typedef struct _GMimePartClass GMimePartClass;

/**
 * GMimePart:
 * @parent_object: parent #GMimeObject
 * @encoding: a #GMimeContentEncoding
 * @content_description: Content-Description string
 * @content_location: Content-Location string
 * @content_md5: Content-MD5 string
 * @content: a #GMimeDataWrapper representing the MIME part's content
 *
 * A leaf-node MIME part object.
 **/
struct _GMimePart {
	GMimeObject parent_object;
	
	GMimeContentEncoding encoding;
	char *content_description;
	char *content_location;
	char *content_md5;
	
	GMimeDataWrapper *content;
};

struct _GMimePartClass {
	GMimeObjectClass parent_class;
	
	void (* set_content_object) (GMimePart *mime_part, GMimeDataWrapper *content);
};


GType g_mime_part_get_type (void);

/* constructors */
GMimePart *g_mime_part_new (void);
GMimePart *g_mime_part_new_with_type (const char *type, const char *subtype);

/* accessor functions */
void g_mime_part_set_content_description (GMimePart *mime_part, const char *description);
const char *g_mime_part_get_content_description (GMimePart *mime_part);

void g_mime_part_set_content_id (GMimePart *mime_part, const char *content_id);
const char *g_mime_part_get_content_id (GMimePart *mime_part);

void g_mime_part_set_content_md5 (GMimePart *mime_part, const char *content_md5);
gboolean g_mime_part_verify_content_md5 (GMimePart *mime_part);
const char *g_mime_part_get_content_md5 (GMimePart *mime_part);

void g_mime_part_set_content_location (GMimePart *mime_part, const char *content_location);
const char *g_mime_part_get_content_location (GMimePart *mime_part);

void g_mime_part_set_content_encoding (GMimePart *mime_part, GMimeContentEncoding encoding);
GMimeContentEncoding g_mime_part_get_content_encoding (GMimePart *mime_part);

GMimeContentEncoding g_mime_part_get_best_content_encoding (GMimePart *mime_part, GMimeEncodingConstraint constraint);

void g_mime_part_set_filename (GMimePart *mime_part, const char *filename);
const char *g_mime_part_get_filename (GMimePart *mime_part);

void g_mime_part_set_content_object (GMimePart *mime_part, GMimeDataWrapper *content);
GMimeDataWrapper *g_mime_part_get_content_object (GMimePart *mime_part);

G_END_DECLS

#endif /* __GMIME_PART_H__ */
