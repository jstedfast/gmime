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
 * GMimeTextFormat:
 * @GMIME_TEXT_FORMAT_PLAIN: Plain text.
 * @GMIME_TEXT_FORMAT_TEXT: An alias for plain text.
 * @GMIME_TEXT_FORMAT_FLOWED: Flowed text.
 * @GMIME_TEXT_FORMAT_HTML: HTML.
 * @GMIME_TEXT_FORMAT_ENRICHED: Enriched text.
 * @GMIME_TEXT_FORMAT_COMPRESSED_RICH_TEXT: Compressed rich text.
 * @GMIME_TEXT_FORMAT_RICH_TEXT: Rich text (RTF).
 *
 * Text formats.
 **/
typedef enum {
	GMIME_TEXT_FORMAT_PLAIN,
	GMIME_TEXT_FORMAT_TEXT = GMIME_TEXT_FORMAT_PLAIN,
	GMIME_TEXT_FORMAT_FLOWED,
	GMIME_TEXT_FORMAT_HTML,
	GMIME_TEXT_FORMAT_ENRICHED,
	GMIME_TEXT_FORMAT_COMPRESSED_RICH_TEXT,
	GMIME_TEXT_FORMAT_RICH_TEXT
} GMimeTextFormat;

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
GMimeTextPart *g_mime_text_part_new_with_format (GMimeTextFormat format);

GMimeTextFormat g_mime_text_part_get_format (GMimeTextPart *mime_part);
gboolean g_mime_text_part_is_plain (GMimeTextPart *mime_part);
gboolean g_mime_text_part_is_flowed (GMimeTextPart *mime_part);
gboolean g_mime_text_part_is_html (GMimeTextPart *mime_part);
gboolean g_mime_text_part_is_enriched (GMimeTextPart *mime_part);
gboolean g_mime_text_part_is_rich_text (GMimeTextPart *mime_part);

void g_mime_text_part_set_charset (GMimeTextPart *mime_part, const char *charset);
const char *g_mime_text_part_get_charset (GMimeTextPart *mime_part);

void g_mime_text_part_set_text (GMimeTextPart *mime_part, const char *text);
gboolean g_mime_text_part_set_text_with_charset (GMimeTextPart *mime_part, const char *charset,
					       const char *text, GError **error);
char *g_mime_text_part_get_text (GMimeTextPart *mime_part);
char *g_mime_text_part_get_text_with_charset (GMimeTextPart *mime_part, const char *charset, GError **error);

G_END_DECLS

#endif /* __GMIME_TEXT_PART_H__ */
