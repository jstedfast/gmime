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

#ifndef __GMIME_HTML_TREE_INTERNAL_H__
#define __GMIME_HTML_TREE_INTERNAL_H__

#include "gmime-html-tree-private.h"
#include "gmime-html-token-private.h"

typedef struct _HtmlTree HtmlTree;

typedef enum {
	HTML_FORMATTING_STOP_NONE,
	HTML_FORMATTING_STOP_CAPACITY,
	HTML_FORMATTING_STOP_ATTRIBUTE_EQUIVALENCE,
	HTML_FORMATTING_STOP_COMPLEX_ADOPTION,
	HTML_FORMATTING_STOP_STACK_REMAPPING
} HtmlFormattingStopReason;

void _g_mime_html_tree_set_formatting_capacity (HtmlTree *tree, guint capacity);
gboolean _g_mime_html_tree_formatting_exceeded (HtmlTree *tree);
HtmlTree *_g_mime_html_tokenizer_get_tree (GMimeHtmlTokenizer *tokenizer);

HtmlTree *_g_mime_html_tree_new (guint maximum);
void _g_mime_html_tree_free (HtmlTree *tree);

GMimeHtmlTokenizerState _g_mime_html_tree_tag (HtmlTree *tree, GMimeHtmlToken *tag);

void _g_mime_html_tree_character (HtmlTree *tree, gunichar c);

gboolean _g_mime_html_tree_tracks_characters (HtmlTree *tree);

gboolean _g_mime_html_tree_foreign (HtmlTree *tree);

gboolean _g_mime_html_tree_noscript (HtmlTree *tree);

struct _HtmlTree {
	GArray *entries, *closed;
	GArray *formatting;
	guint formatting_capacity, formatting_peak;
	HtmlFormattingStopReason formatting_stop;
	GHashTable *html_names, *foreign_names;
	gint html_ids[GMIME_HTML_TAG_XMP + 1];
	guint max_depth;
	gboolean depth_exceeded, track_closed, scripting;
	GMimeHtmlNamespace ns;
	GMimeHtmlTagDisposition disposition;
	gboolean poppedThrough, framesetOk, inFrameset, htmlCreated, bodyCreated, formPointerSet;
	gboolean EndTagIgnored, StartTagIgnored, StartTagAttributesMerged, BrokeOutOfForeignContent;
	int mode, framesetDepth, formPointerIndex;
};

void _g_mime_html_tree_clear_closed (HtmlTree *tree);

#endif
