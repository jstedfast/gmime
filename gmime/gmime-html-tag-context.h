/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*  GMime
 *  Copyright (C) Jeffrey Stedfast
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

#ifndef __GMIME_HTML_TAG_CONTEXT_H__
#define __GMIME_HTML_TAG_CONTEXT_H__

#include <gmime/gmime-html-writer.h>

G_BEGIN_DECLS

/**
 * GMimeHeaderFooterFormat:
 * @GMIME_HEADER_FOOTER_FORMAT_TEXT: Convert the header or footer from plain text.
 * @GMIME_HEADER_FOOTER_FORMAT_HTML: Copy the header or footer as raw HTML.
 *
 * Specifies how HTML-producing converters interpret their configured
 * headers and footers. Raw HTML is not sanitized.
 **/
typedef enum {
	GMIME_HEADER_FOOTER_FORMAT_TEXT,
	GMIME_HEADER_FOOTER_FORMAT_HTML
} GMimeHeaderFooterFormat;

typedef struct _GMimeHtmlTagContext GMimeHtmlTagContext;

/**
 * GMimeHtmlTagCallback:
 * @context: (transfer none): borrowed tag metadata and mutable decisions
 * @writer: (transfer none): the borrowed output writer
 * @user_data: (nullable): user data supplied when registering the callback
 * @error: return location for a #GError, or %NULL
 *
 * Writes the desired tag explicitly. The context is borrowed for the
 * duration of the callback and must not be freed or retained. Its attributes
 * are borrowed; use g_mime_html_attribute_ref() to retain an attribute
 * beyond the callback. The writer is borrowed and must not be freed by
 * the callback.
 *
 * Returns: %TRUE on success, or %FALSE to abort conversion with @error set.
 **/
typedef gboolean (*GMimeHtmlTagCallback) (GMimeHtmlTagContext *context, GMimeHtmlWriter *writer, gpointer user_data,
					  GError **error);

GMimeHtmlTagContext *g_mime_html_tag_context_new (GMimeHtmlToken *token);
void g_mime_html_tag_context_free (GMimeHtmlTagContext *context);

void g_mime_html_tag_context_set_token (GMimeHtmlTagContext *context, GMimeHtmlToken *token);

const char *g_mime_html_tag_context_get_tag_name (GMimeHtmlTagContext *context);
GMimeHtmlTagId g_mime_html_tag_context_get_tag_id (GMimeHtmlTagContext *context);

guint g_mime_html_tag_context_get_n_attributes (GMimeHtmlTagContext *context);
const GMimeHtmlAttribute *g_mime_html_tag_context_get_attribute (GMimeHtmlTagContext *context, guint index);

gboolean g_mime_html_tag_context_get_is_empty_element_tag (GMimeHtmlTagContext *context);
gboolean g_mime_html_tag_context_get_is_end_tag (GMimeHtmlTagContext *context);
void g_mime_html_tag_context_set_is_end_tag (GMimeHtmlTagContext *context, gboolean value);

gboolean g_mime_html_tag_context_get_delete_tag (GMimeHtmlTagContext *context);
void g_mime_html_tag_context_set_delete_tag (GMimeHtmlTagContext *context, gboolean value);

gboolean g_mime_html_tag_context_get_delete_end_tag (GMimeHtmlTagContext *context);
void g_mime_html_tag_context_set_delete_end_tag (GMimeHtmlTagContext *context, gboolean value);

gboolean g_mime_html_tag_context_get_suppress_inner_content (GMimeHtmlTagContext *context);
void g_mime_html_tag_context_set_suppress_inner_content (GMimeHtmlTagContext *context, gboolean value);

gboolean g_mime_html_tag_context_get_invoke_callback_for_end_tag (GMimeHtmlTagContext *context);
void g_mime_html_tag_context_set_invoke_callback_for_end_tag (GMimeHtmlTagContext *context, gboolean value);

gboolean g_mime_html_tag_context_write_tag (GMimeHtmlTagContext *context, GMimeHtmlWriter *writer,
					    gboolean write_attributes, GError **error);

G_END_DECLS

#endif
