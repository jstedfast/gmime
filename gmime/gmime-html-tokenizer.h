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

#ifndef __GMIME_HTML_TOKENIZER_H__
#define __GMIME_HTML_TOKENIZER_H__

#include <gmime/gmime-html-token.h>
#include <gmime/gmime-html-compliance.h>

G_BEGIN_DECLS

#define GMIME_TYPE_HTML_TOKENIZER (g_mime_html_tokenizer_get_type ())

/**
 * GMimeHtmlTokenizer:
 *
 * A streaming HTML tokenizer with character-set decoding and optional
 * compliance diagnostics.
 **/
G_DECLARE_FINAL_TYPE (GMimeHtmlTokenizer, g_mime_html_tokenizer, GMIME, HTML_TOKENIZER, GObject)

GMimeHtmlTokenizer *g_mime_html_tokenizer_new (GMimeStream *stream);

GMimeHtmlTokenizer *g_mime_html_tokenizer_new_with_charset (GMimeStream *stream, const char *charset,
							    gboolean detect_bom, gsize buffer_size, GError **error);

GMimeHtmlTokenizerState g_mime_html_tokenizer_get_state (GMimeHtmlTokenizer *tokenizer);

gboolean g_mime_html_tokenizer_read_next_token (GMimeHtmlTokenizer *tokenizer, GMimeHtmlToken **token, GError **error);

gboolean g_mime_html_tokenizer_get_decode_character_references (GMimeHtmlTokenizer *tokenizer);
void g_mime_html_tokenizer_set_decode_character_references (GMimeHtmlTokenizer *tokenizer, gboolean value);

GMimeHtmlNamespace g_mime_html_tokenizer_get_namespace (GMimeHtmlTokenizer *tokenizer);
void g_mime_html_tokenizer_set_namespace (GMimeHtmlTokenizer *tokenizer, GMimeHtmlNamespace ns);

gboolean g_mime_html_tokenizer_get_ignore_truncated_tags (GMimeHtmlTokenizer *tokenizer);
void g_mime_html_tokenizer_set_ignore_truncated_tags (GMimeHtmlTokenizer *tokenizer, gboolean value);

guint g_mime_html_tokenizer_get_max_element_depth (GMimeHtmlTokenizer *tokenizer);
void g_mime_html_tokenizer_set_max_element_depth (GMimeHtmlTokenizer *tokenizer, guint maximum);

gboolean g_mime_html_tokenizer_get_scripting_enabled (GMimeHtmlTokenizer *tokenizer);
void g_mime_html_tokenizer_set_scripting_enabled (GMimeHtmlTokenizer *tokenizer, gboolean enabled);

gint64 g_mime_html_tokenizer_get_position (GMimeHtmlTokenizer *tokenizer);
int g_mime_html_tokenizer_get_line_number (GMimeHtmlTokenizer *tokenizer);
int g_mime_html_tokenizer_get_line_position (GMimeHtmlTokenizer *tokenizer);

void g_mime_html_tokenizer_set_compliance_logger (GMimeHtmlTokenizer *tokenizer, GMimeHtmlComplianceLogger logger,
						  gpointer user_data, GDestroyNotify destroy);

GMimeHtmlComplianceOptions *g_mime_html_tokenizer_get_compliance_options (GMimeHtmlTokenizer *tokenizer);

void g_mime_html_tokenizer_set_compliance_options (GMimeHtmlTokenizer *tokenizer, GMimeHtmlComplianceOptions *options);

G_END_DECLS

#endif
