/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*  GMime
 *  Copyright (C) 2026 Jeffrey Stedfast
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

#ifndef __GMIME_TEXT_PREVIEWER_H__
#define __GMIME_TEXT_PREVIEWER_H__

#include <gmime/gmime-text-part.h>
#include <gmime/gmime-stream.h>

G_BEGIN_DECLS

#define GMIME_TYPE_TEXT_PREVIEWER (g_mime_text_previewer_get_type ())

/**
 * GMimeTextPreviewer:
 *
 * An abstract generator of bounded, plain-text previews.
 **/
G_DECLARE_DERIVABLE_TYPE (GMimeTextPreviewer, g_mime_text_previewer, GMIME, TEXT_PREVIEWER, GObject)

/**
 * GMimeTextPreviewerClass:
 * @parent_class: parent class
 * @input_format: the text format accepted by the previewer
 * @preview: generates an allocated UTF-8 preview from a UTF-8 stream;
 *   the string_input argument distinguishes string and stream whitespace
 *   handling; reports failures through its error argument
 *
 * Base class for text previewers. Implementations must not retain or close
 * the input stream and must return %NULL on failure.
 **/
struct _GMimeTextPreviewerClass {
	GObjectClass parent_class;
	GMimeTextFormat input_format;

	char *(*preview) (GMimeTextPreviewer *previewer, GMimeStream *input, gboolean string_input, GError **error);
};

#define GMIME_TYPE_PLAIN_TEXT_PREVIEWER (g_mime_plain_text_previewer_get_type ())
#define GMIME_TYPE_HTML_TEXT_PREVIEWER (g_mime_html_text_previewer_get_type ())

/**
 * GMimePlainTextPreviewer:
 *
 * Generates plain-text previews with collapsed whitespace.
 **/
G_DECLARE_FINAL_TYPE (GMimePlainTextPreviewer, g_mime_plain_text_previewer, GMIME, PLAIN_TEXT_PREVIEWER,
		      GMimeTextPreviewer)

/**
 * GMimeHtmlTextPreviewer:
 *
 * Generates plain-text previews from explicit HTML body elements.
 **/
G_DECLARE_FINAL_TYPE (GMimeHtmlTextPreviewer, g_mime_html_text_previewer, GMIME, HTML_TEXT_PREVIEWER,
		      GMimeTextPreviewer)

GMimeTextPreviewer *g_mime_plain_text_previewer_new (void);
GMimeTextPreviewer *g_mime_html_text_previewer_new (void);

GMimeTextFormat g_mime_text_previewer_get_input_format (GMimeTextPreviewer *previewer);

guint g_mime_text_previewer_get_maximum_preview_length (GMimeTextPreviewer *previewer);
void g_mime_text_previewer_set_maximum_preview_length (GMimeTextPreviewer *previewer, guint maximum);

char *g_mime_text_previewer_get_preview_text (GMimeTextPreviewer *previewer, const char *text, GError **error);
char *g_mime_text_previewer_get_preview_text_from_stream (GMimeTextPreviewer *previewer, GMimeStream *stream,
							  const char *charset, GError **error);
char *g_mime_text_previewer_get_preview_text_from_part (GMimeTextPart *part, GError **error);

G_END_DECLS

#endif
