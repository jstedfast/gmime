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

#ifndef __GMIME_HTML_TOKEN_PRIVATE_H__
#define __GMIME_HTML_TOKEN_PRIVATE_H__

#include "gmime-html-token.h"

struct _GMimeHtmlAttribute {
	gatomicrefcount refs;
	char *name, *value;
	GMimeHtmlAttributeId id;
};

struct _GMimeHtmlToken {
	gatomicrefcount refs;
	GMimeHtmlTokenKind kind;
	char *name, *data;
	gsize length;
	GMimeHtmlTagId id;
	GPtrArray *attributes;
	gboolean is_end_tag, is_empty_element, encode_entities, is_data_state;
	gboolean bogus, bang, force_quirks;
	char *raw_tag_name, *public_identifier, *system_identifier, *public_keyword, *system_keyword;
	char public_quote, system_quote;
};

char *_g_mime_html_encode_len (const char *text, gsize length, char quote);

#endif
