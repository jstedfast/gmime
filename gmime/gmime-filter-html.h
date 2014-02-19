/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*  GMime
 *  Copyright (C) 2000-2014 Jeffrey Stedfast
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


#ifndef __GMIME_FILTER_HTML_H__
#define __GMIME_FILTER_HTML_H__

#include <gmime/gmime-filter.h>

G_BEGIN_DECLS

#define GMIME_TYPE_FILTER_HTML            (g_mime_filter_html_get_type ())
#define GMIME_FILTER_HTML(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), GMIME_TYPE_FILTER_HTML, GMimeFilterHTML))
#define GMIME_FILTER_HTML_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), GMIME_TYPE_FILTER_HTML, GMimeFilterHTMLClass))
#define GMIME_IS_FILTER_HTML(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GMIME_TYPE_FILTER_HTML))
#define GMIME_IS_FILTER_HTML_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), GMIME_TYPE_FILTER_HTML))
#define GMIME_FILTER_HTML_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), GMIME_TYPE_FILTER_HTML, GMimeFilterHTMLClass))

typedef struct _GMimeFilterHTML GMimeFilterHTML;
typedef struct _GMimeFilterHTMLClass GMimeFilterHTMLClass;


/**
 * GMIME_FILTER_HTML_PRE:
 *
 * Wrap stream in &lt;pre&gt; tags.
 **/
#define GMIME_FILTER_HTML_PRE               (1 << 0)


/**
 * GMIME_FILTER_HTML_CONVERT_NL:
 *
 * Convert new-lines ('\n') into &lt;br&gt; tags.
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
 * Wrap detected URLs in &lt;a href=...&gt; tags.
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
 * Cites text by prepending "&gt; " to each cited line.
 **/
#define GMIME_FILTER_HTML_CITE              (1 << 7)


/**
 * GMimeFilterHTML:
 * @parent_object: parent #GMimeFilter
 * @scanner: URL scanner state
 * @flags: flags specifying HTML conversion rules
 * @colour: cite colour
 * @column: current column
 * @pre_open: currently inside of a 'pre' tag.
 *
 * A filter for converting text/plain into text/html.
 **/
struct _GMimeFilterHTML {
	GMimeFilter parent_object;
	
	struct _UrlScanner *scanner;
	
	guint32 flags;
	guint32 colour;
	
	guint32 column       : 31;
	guint32 pre_open     : 1;
};

struct _GMimeFilterHTMLClass {
	GMimeFilterClass parent_class;
	
};


GType g_mime_filter_html_get_type (void);

GMimeFilter *g_mime_filter_html_new (guint32 flags, guint32 colour);

G_END_DECLS

#endif /* __GMIME_FILTER_HTML_H__ */
