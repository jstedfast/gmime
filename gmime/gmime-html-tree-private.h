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

#ifndef __GMIME_HTML_TREE_PRIVATE_H__
#define __GMIME_HTML_TREE_PRIVATE_H__

#include "gmime-html-tokenizer.h"

typedef enum {
	GMIME_HTML_TAG_DISPOSITION_NORMAL,
	GMIME_HTML_TAG_DISPOSITION_IGNORED,
	GMIME_HTML_TAG_DISPOSITION_ATTRIBUTES_MERGED
} GMimeHtmlTagDisposition;

typedef struct {
	const char *name;
	guint count;
	gboolean is_foreign;
} GMimeHtmlClosedElement;

void _g_mime_html_tokenizer_set_track_closed_elements (GMimeHtmlTokenizer *tokenizer, gboolean enabled);

const GMimeHtmlClosedElement *_g_mime_html_tokenizer_get_closed_elements (GMimeHtmlTokenizer *tokenizer, gsize *count);

GMimeHtmlTagDisposition _g_mime_html_tokenizer_get_tag_disposition (GMimeHtmlTokenizer *tokenizer);

gboolean _g_mime_html_tokenizer_get_in_frameset (GMimeHtmlTokenizer *tokenizer);

gboolean _g_mime_html_tokenizer_get_depth_exceeded (GMimeHtmlTokenizer *tokenizer);

gboolean _g_mime_html_tokenizer_get_broke_out_of_foreign_content (GMimeHtmlTokenizer *tokenizer);

gboolean _g_mime_html_tokenizer_get_is_noscript_open (GMimeHtmlTokenizer *tokenizer);

void _g_mime_html_tokenizer_clear_closed_elements (GMimeHtmlTokenizer *tokenizer);

void _g_mime_html_tokenizer_set_max_input_length (GMimeHtmlTokenizer *tokenizer, gsize maximum);

#endif
