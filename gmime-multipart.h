/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 *  Authors: Jeffrey Stedfast <fejj@ximian.com>
 *
 *  Copyright 2002 Ximian, Inc. (www.ximian.com)
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


#ifndef __GMIME_MULTIPART_H__
#define __GMIME_MULTIPART_H__

#ifdef __cplusplus
extern "C" {
#pragma }
#endif /* __cplusplus */

#include <glib.h>

#include "gmime-object.h"
#include "gmime-content-type.h"

#define GMIME_TYPE_MULTIPART            (g_mime_multipart_get_type ())
#define GMIME_MULTIPART(obj)            (GMIME_CHECK_CAST ((obj), GMIME_TYPE_MULTIPART, GMimeMultipart))
#define GMIME_MULTIPART_CLASS(klass)    (GMIME_CHECK_CLASS_CAST ((klass), GMIME_TYPE_MULTIPART, GMimeMultipartClass))
#define GMIME_IS_MULTIPART(obj)         (GMIME_CHECK_TYPE ((obj), GMIME_TYPE_MULTIPART))
#define GMIME_IS_MULTIPART_CLASS(klass) (GMIME_CHECK_CLASS_TYPE ((klass), GMIME_TYPE_MULTIPART))
#define GMIME_MULTIPART_GET_CLASS(obj)  (GMIME_CHECK_GET_CLASS ((obj), GMIME_TYPE_MULTIPART, GMimeMultipartClass))

typedef struct _GMimeMultipart GMimeMultipart;
typedef struct _GMimeMultipartClass GMimeMultipartClass;

struct _GMimeMultipart {
	GMimeObject parent_object;
	
	GMimeContentType *content_type;
	char *boundary;
	
	char *preface;
	char *postface;
	
	GList *parts;
};

struct _GMimeMultipartClass {
	GMimeObjectClass parent_class;
	
	void (*add_part) (GMimeMultipart *multipart, GMimeObject *part);
	void (*add_part_at) (GMimeMultipart *multipart, GMimeObject *part, int index);
	void (*remove_part) (GMimeMultipart *multipart, GMimeObject *part);
	
	GMimeObject * (*remove_part_at) (GMimeMultipart *multipart, int index);
	GMimeObject * (*get_part) (GMimeMultipart *multipart, int index);
	
	int  (*get_number) (GMimeMultipart *multipart);
	
	void (*set_boundary) (GMimeMultipart *multipart, const char *boundary);
	const char * (*get_boundary) (GMimeMultipart *multipart);
};


GType g_mime_multipart_get_type (void);

GMimeMultipart *g_mime_multipart_new (void);

void g_mime_multipart_add_part (GMimeMultipart *multipart, GMimeObject *part);
void g_mime_multipart_add_part_at (GMimeMultipart *multipart, GMimeObject *part, int index);
void g_mime_multipart_remove_part (GMimeMultipart *multipart, GMimeObject *part);

GMimeObject *g_mime_multipart_remove_part_at (GMimeMultipart *multipart, int index);
GMimeObject *g_mime_multipart_get_part (GMimeMultipart *multipart, int index);

int g_mime_multipart_get_number (GMimeMultipart *multipart);

void g_mime_multipart_set_boundary (GMimeMultipart *multipart, const char *boundary);
const char *g_mime_multipart_get_boundary (GMimeMultipart *multipart);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* __GMIME_MULTIPART_H__ */
