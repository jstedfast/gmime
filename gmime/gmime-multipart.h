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


#ifndef __GMIME_MULTIPART_H__
#define __GMIME_MULTIPART_H__

#include <glib.h>

#include <gmime/gmime-encodings.h>
#include <gmime/gmime-object.h>

G_BEGIN_DECLS

#define GMIME_TYPE_MULTIPART            (g_mime_multipart_get_type ())
#define GMIME_MULTIPART(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), GMIME_TYPE_MULTIPART, GMimeMultipart))
#define GMIME_MULTIPART_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), GMIME_TYPE_MULTIPART, GMimeMultipartClass))
#define GMIME_IS_MULTIPART(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GMIME_TYPE_MULTIPART))
#define GMIME_IS_MULTIPART_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), GMIME_TYPE_MULTIPART))
#define GMIME_MULTIPART_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), GMIME_TYPE_MULTIPART, GMimeMultipartClass))

typedef struct _GMimeMultipart GMimeMultipart;
typedef struct _GMimeMultipartClass GMimeMultipartClass;

/**
 * GMimeMultipart:
 * @parent_object: parent #GMimeObject
 * @children: array of MIME sub-parts
 * @boundary: MIME boundary
 * @preface: multipart preface
 * @postface: multipart postface
 *
 * A base MIME multipart object.
 **/
struct _GMimeMultipart {
	GMimeObject parent_object;
	
	GPtrArray *children;
	char *boundary;
	char *preface;
	char *postface;
};

struct _GMimeMultipartClass {
	GMimeObjectClass parent_class;
	
	void (* clear) (GMimeMultipart *multipart);
	void (* add) (GMimeMultipart *multipart, GMimeObject *part);
	void (* insert) (GMimeMultipart *multipart, int index, GMimeObject *part);
	gboolean (* remove) (GMimeMultipart *multipart, GMimeObject *part);
	GMimeObject * (* remove_at) (GMimeMultipart *multipart, int index);
	GMimeObject * (* get_part) (GMimeMultipart *multipart, int index);
	
	gboolean (* contains) (GMimeMultipart *multipart, GMimeObject *part);
	int (* index_of) (GMimeMultipart *multipart, GMimeObject *part);
	
	int  (* get_count) (GMimeMultipart *multipart);
	
	void (* set_boundary) (GMimeMultipart *multipart, const char *boundary);
	const char * (* get_boundary) (GMimeMultipart *multipart);
};


GType g_mime_multipart_get_type (void);

GMimeMultipart *g_mime_multipart_new (void);

GMimeMultipart *g_mime_multipart_new_with_subtype (const char *subtype);

void g_mime_multipart_set_preface (GMimeMultipart *multipart, const char *preface);
const char *g_mime_multipart_get_preface (GMimeMultipart *multipart);

void g_mime_multipart_set_postface (GMimeMultipart *multipart, const char *postface);
const char *g_mime_multipart_get_postface (GMimeMultipart *multipart);

void g_mime_multipart_clear (GMimeMultipart *multipart);

void g_mime_multipart_add (GMimeMultipart *multipart, GMimeObject *part);
void g_mime_multipart_insert (GMimeMultipart *multipart, int index, GMimeObject *part);
gboolean g_mime_multipart_remove (GMimeMultipart *multipart, GMimeObject *part);
GMimeObject *g_mime_multipart_remove_at (GMimeMultipart *multipart, int index);
GMimeObject *g_mime_multipart_replace (GMimeMultipart *multipart, int index, GMimeObject *replacement);
GMimeObject *g_mime_multipart_get_part (GMimeMultipart *multipart, int index);

gboolean g_mime_multipart_contains (GMimeMultipart *multipart, GMimeObject *part);
int g_mime_multipart_index_of (GMimeMultipart *multipart, GMimeObject *part);

int g_mime_multipart_get_count (GMimeMultipart *multipart);

void g_mime_multipart_set_boundary (GMimeMultipart *multipart, const char *boundary);
const char *g_mime_multipart_get_boundary (GMimeMultipart *multipart);

/* convenience functions */

void g_mime_multipart_foreach (GMimeMultipart *multipart, GMimeObjectForeachFunc callback,
			       gpointer user_data);

GMimeObject *g_mime_multipart_get_subpart_from_content_id (GMimeMultipart *multipart,
							   const char *content_id);

G_END_DECLS

#endif /* __GMIME_MULTIPART_H__ */
