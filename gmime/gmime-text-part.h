/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*  GMime
 *  Copyright (C) 2000-2017 Jeffrey Stedfast
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


#ifndef __GMIME_TEXT_PART_H__
#define __GMIME_TEXT_PART_H__

#include <gmime/gmime-part.h>

G_BEGIN_DECLS

#define GMIME_TYPE_TEXT_PART            (g_mime_text_part_get_type ())
#define GMIME_TEXT_PART(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), GMIME_TYPE_TEXT_PART, GMimeTextPart))
#define GMIME_TEXT_PART_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), GMIME_TYPE_TEXT_PART, GMimeTextPartClass))
#define GMIME_IS_TEXT_PART(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GMIME_TYPE_TEXT_PART))
#define GMIME_IS_TEXT_PART_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), GMIME_TYPE_TEXT_PART))
#define GMIME_TEXT_PART_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), GMIME_TYPE_TEXT_PART, GMimeTextPartClass))

typedef struct _GMimeTextPart GMimeTextPart;
typedef struct _GMimeTextPartClass GMimeTextPartClass;

/**
 * GMimeTextPart:
 * @parent_object: parent #GMimePart
 *
 * A text MIME part object.
 **/
struct _GMimeTextPart {
	GMimePart parent_object;
	
};

struct _GMimeTextPartClass {
	GMimePartClass parent_class;
	
};


GType g_mime_text_part_get_type (void);

/* constructors */
GMimeTextPart *g_mime_text_part_new (void);
GMimeTextPart *g_mime_text_part_new_with_subtype (const char *subtype);

void g_mime_text_part_set_charset (GMimeTextPart *mime_part, const char *charset);
const char *g_mime_text_part_get_charset (GMimeTextPart *mime_part);

void g_mime_text_part_set_text (GMimeTextPart *mime_part, const char *text);
char *g_mime_text_part_get_text (GMimeTextPart *mime_part);

G_END_DECLS

#endif /* __GMIME_TEXT_PART_H__ */
