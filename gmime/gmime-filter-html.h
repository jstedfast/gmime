/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 *  Authors: Jeffrey Stedfast <fejj@ximian.com>
 *
 *  Copyright 2001 Ximian, Inc. (www.ximian.com)
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


#ifndef __G_MIME_FILTER_HTML_H__
#define __G_MIME_FILTER_HTML_H__

#ifdef __cplusplus
extern "C" {
#pragma }
#endif /* __cplusplus */

#include "gmime-filter.h"


/**
 * GMIME_FILTER_HTML_PRE:
 *
 * Wrap stream in <pre> tags.
 **/
#define GMIME_FILTER_HTML_PRE               (1 << 0)


/**
 * GMIME_FILTER_HTML_CONVERT_NL:
 *
 * Convert new-lines ('\n') into <br> tags.
 **/
#define GMIME_FILTER_HTML_CONVERT_NL        (1 << 1)


/**
 * GMIME_FILTER_HTML_CONVERT_SPACES:
 *
 * Preserve whitespace by converting spaces into their appropriate
 * html entities.
 **/
#define GMIME_FILTER_HTML_CONVERT_SPACES    (1 << 2)


/**
 * GMIME_FILTER_HTML_CONVERT_URLS:
 *
 * Wrap detected URLs in <a href=...> tags.
 **/
#define GMIME_FILTER_HTML_CONVERT_URLS      (1 << 3)


/**
 * GMIME_FILTER_HTML_MARK_CITATION:
 *
 * Change the colour of citation text.
 **/
#define GMIME_FILTER_HTML_MARK_CITATION     (1 << 4)


/**
 * GMIME_FILTER_HTML_CONVERT_ADDRESSES:
 *
 * Wrap email addresses in "mailto:" href tags.
 **/
#define GMIME_FILTER_HTML_CONVERT_ADDRESSES (1 << 5)


/**
 * GMIME_FILTER_HTML_ESCAPE_8BIT:
 *
 * Converts 8bit characters to '?'.
 **/
#define GMIME_FILTER_HTML_ESCAPE_8BIT       (1 << 6)


/**
 * GMIME_FILTER_HTML_CITE:
 *
 * Cites text by prepending "> " to each cited line.
 **/
#define GMIME_FILTER_HTML_CITE              (1 << 7)


typedef struct _GMimeFilterHTML {
	GMimeFilter parent;
	
	guint32 flags;
	guint32 colour;
	
	guint32 column       : 29;
	guint32 pre_open     : 1;
	guint32 saw_citation : 1;
	guint32 coloured     : 1;
} GMimeFilterHTML;

GMimeFilter *g_mime_filter_html_new (guint32 flags, guint32 colour);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* __GMIME_FILTER_HTML_H__ */
