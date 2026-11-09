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

#ifndef __GMIME_HTML_WRITER_H__
#define __GMIME_HTML_WRITER_H__

#include <gmime/gmime-stream.h>
#include <gmime/gmime-html-token.h>

G_BEGIN_DECLS

/**
 * GMimeHtmlWriterState:
 * @GMIME_HTML_WRITER_STATE_DEFAULT: No tag is pending; attributes cannot be written.
 * @GMIME_HTML_WRITER_STATE_TAG: A start or empty-element tag is pending and an
 *   attribute name can be written.
 * @GMIME_HTML_WRITER_STATE_ATTRIBUTE: An attribute name has been written in a
 *   pending tag; its optional value can be written.
 *
 * The current serialization state of a #GMimeHtmlWriter. Writing an attribute
 * value returns the writer to the tag state. Finishing a pending tag returns
 * the writer to the default state.
 **/
typedef enum {
	GMIME_HTML_WRITER_STATE_DEFAULT,
	GMIME_HTML_WRITER_STATE_TAG,
	GMIME_HTML_WRITER_STATE_ATTRIBUTE
} GMimeHtmlWriterState;

#define GMIME_TYPE_HTML_WRITER (g_mime_html_writer_get_type ())

/**
 * GMimeHtmlWriter:
 *
 * A stateful writer for UTF-8 HTML markup, text and attributes.
 **/
G_DECLARE_FINAL_TYPE (GMimeHtmlWriter, g_mime_html_writer, GMIME, HTML_WRITER, GObject)

GMimeHtmlWriter *g_mime_html_writer_new (GMimeStream *stream);

GMimeHtmlWriterState g_mime_html_writer_get_state (GMimeHtmlWriter *writer);

gboolean g_mime_html_writer_write_start_tag (GMimeHtmlWriter *writer, const char *name, GError **error);
gboolean g_mime_html_writer_write_end_tag (GMimeHtmlWriter *writer, const char *name, GError **error);
gboolean g_mime_html_writer_write_empty_element_tag (GMimeHtmlWriter *writer, const char *name, GError **error);

gboolean g_mime_html_writer_write_attribute (GMimeHtmlWriter *writer, const char *name, const char *value,
					     GError **error);
gboolean g_mime_html_writer_write_html_attribute (GMimeHtmlWriter *writer, const GMimeHtmlAttribute *attribute,
						  GError **error);
gboolean g_mime_html_writer_write_attribute_name (GMimeHtmlWriter *writer, const char *name, GError **error);
gboolean g_mime_html_writer_write_attribute_value (GMimeHtmlWriter *writer, const char *value, GError **error);

gboolean g_mime_html_writer_write_text (GMimeHtmlWriter *writer, const char *text, gssize length, GError **error);
gboolean g_mime_html_writer_write_markup_text (GMimeHtmlWriter *writer, const char *text, gssize length,
					       GError **error);

gboolean g_mime_html_writer_write_token (GMimeHtmlWriter *writer, GMimeHtmlToken *token, GError **error);

gboolean g_mime_html_writer_finish (GMimeHtmlWriter *writer, GError **error);
gboolean g_mime_html_writer_flush (GMimeHtmlWriter *writer, GError **error);

G_END_DECLS

#endif
