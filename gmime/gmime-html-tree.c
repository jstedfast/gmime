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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <string.h>

#include "gmime-html-tree-internal.h"

typedef enum {
	F_None = 0,
	F_Special = 1,
	F_Scope = 2,
	F_ListItemScope = 4,
	F_ButtonScope = 8,
	F_TableScope = 16,
	F_Heading = 32,
	F_ListItemStop = 64,
	F_MathMLTextIntegrationPoint = 128,
	F_HtmlIntegrationPoint = 256
} ElementFlags;

typedef enum {
	BeforeHead,
	InHead,
	InHeadNoScript,
	AfterHead
} InsertionMode;

typedef struct {
	char *Name;
	GMimeHtmlTagId Id;
	GMimeHtmlNamespace Namespace;
	ElementFlags Flags;
	int Count, PrevSameName;
	int PrevSpecial, PrevScope, PrevListItemScope, PrevButtonScope, PrevTableScope, PrevHeading, PrevListItemStop,
		PrevHtml, PrevHtmlOrIntegrationPoint;
} Entry;

static inline int
stack_count (HtmlTree *tree)
{
	return (int) tree->entries->len;
}

static inline Entry *
stack_entry (HtmlTree *tree, int index)
{
	return &g_array_index (tree->entries, Entry, index);
}

static inline const Entry *
stack_top (HtmlTree *tree)
{
	static const Entry empty = { .PrevSpecial = -1,
				     .PrevScope = -1,
				     .PrevListItemScope = -1,
				     .PrevButtonScope = -1,
				     .PrevTableScope = -1,
				     .PrevHeading = -1,
				     .PrevListItemStop = -1,
				     .PrevHtml = -1,
				     .PrevHtmlOrIntegrationPoint = -1 };

	if (!tree->entries->len)
		return &empty;

	return stack_entry (tree, stack_count (tree) - 1);
}

static int
stack_index (HtmlTree *tree, GMimeHtmlNamespace ns, GMimeHtmlTagId id, const char *name)
{
	gpointer value;
	char *key;

	if (ns == GMIME_HTML_NAMESPACE_HTML && id != GMIME_HTML_TAG_UNKNOWN)
		return tree->html_ids[id];

	key = g_ascii_strdown (name, -1);
	value = g_hash_table_lookup (ns == GMIME_HTML_NAMESPACE_HTML ? tree->html_names : tree->foreign_names, key);
	g_free (key);

	return GPOINTER_TO_INT (value) - 1;
}

static void
stack_set_index (HtmlTree *tree, GMimeHtmlNamespace ns, GMimeHtmlTagId id, const char *name, int index)
{
	GHashTable *table;
	char *key;

	if (ns == GMIME_HTML_NAMESPACE_HTML && id != GMIME_HTML_TAG_UNKNOWN) {
		tree->html_ids[id] = index;
		return;
	}

	table = ns == GMIME_HTML_NAMESPACE_HTML ? tree->html_names : tree->foreign_names;
	key = g_ascii_strdown (name, -1);

	if (index < 0) {
		g_hash_table_remove (table, key);
		g_free (key);
	} else {
		g_hash_table_replace (table, key, GINT_TO_POINTER (index + 1));
	}
}

static gboolean
stack_is_top_html (HtmlTree *tree, GMimeHtmlTagId id)
{
	return stack_count (tree) &&
	       stack_entry (tree, stack_count (tree) - 1)->Namespace == GMIME_HTML_NAMESPACE_HTML &&
	       stack_entry (tree, stack_count (tree) - 1)->Id == id;
}

static void
stack_push (HtmlTree *tree, const char *name, GMimeHtmlTagId id, GMimeHtmlNamespace ns, ElementFlags flags)
{
	Entry entry;
	int index = stack_count (tree);

	if (index) {
		Entry *top = stack_entry (tree, index - 1);

		/* Fold consecutive identical elements into a single entry. */
		if (top->Namespace == ns && top->Id == id && top->Flags == flags &&
		    (id != GMIME_HTML_TAG_UNKNOWN || !g_ascii_strcasecmp (top->Name, name))) {
			if (top->Count < G_MAXINT)
				top->Count++;

			return;
		}
	}

	if ((guint) index >= tree->max_depth) {
		tree->depth_exceeded = TRUE;
		return;
	}

	if (index)
		entry = (*stack_entry (tree, index - 1));
	else
		memset (&entry, 0xff, sizeof entry);

	entry.Name = g_ref_string_new (name);
	entry.Id = id;
	entry.Namespace = ns;
	entry.Flags = flags;
	entry.Count = 1;
	entry.PrevSameName = stack_index (tree, ns, id, name);

	if (flags & F_Special)
		entry.PrevSpecial = index;

	if (flags & F_Scope)
		entry.PrevScope = entry.PrevListItemScope = entry.PrevButtonScope = index;

	if (flags & F_ListItemScope)
		entry.PrevListItemScope = index;

	if (flags & F_ButtonScope)
		entry.PrevButtonScope = index;

	if (flags & F_TableScope)
		entry.PrevTableScope = index;

	if (flags & F_Heading)
		entry.PrevHeading = index;

	if (flags & F_ListItemStop)
		entry.PrevListItemStop = index;

	if (ns == GMIME_HTML_NAMESPACE_HTML)
		entry.PrevHtml = entry.PrevHtmlOrIntegrationPoint = index;
	else if (flags & (F_HtmlIntegrationPoint | F_MathMLTextIntegrationPoint))
		entry.PrevHtmlOrIntegrationPoint = index;

	g_array_append_val (tree->entries, entry);
	stack_set_index (tree, ns, id, name, index);
}

static void
stack_remove_top (HtmlTree *tree)
{
	Entry entry = (*stack_entry (tree, stack_count (tree) - 1));

	stack_set_index (tree, entry.Namespace, entry.Id, entry.Name, entry.PrevSameName);
	g_ref_string_release (entry.Name);
	g_array_set_size (tree->entries, tree->entries->len - 1);
}

static void
stack_pop_above (HtmlTree *tree, int index)
{
	while (stack_count (tree) - 1 > index) {
		Entry *entry = stack_entry (tree, stack_count (tree) - 1);

		if (tree->track_closed) {
			GMimeHtmlClosedElement closed = { g_ref_string_acquire (entry->Name), entry->Count,
							  entry->Namespace != GMIME_HTML_NAMESPACE_HTML };

			g_array_append_val (tree->closed, closed);
		}

		stack_remove_top (tree);
	}
}

static void
stack_pop_through (HtmlTree *tree, int index)
{
	if (index < 0 || index >= stack_count (tree))
		return;

	stack_pop_above (tree, index);

	if (stack_entry (tree, index)->Count > 1)
		stack_entry (tree, index)->Count--;
	else
		stack_remove_top (tree);
}

static void
stack_push_entry (HtmlTree *tree, const Entry *entry)
{
	if (tree->depth_exceeded)
		return;

	stack_push (tree, entry->Name, entry->Id, entry->Namespace, entry->Flags);

	if (!tree->depth_exceeded) {
		Entry *top = stack_entry (tree, stack_count (tree) - 1);

		top->Count = MIN ((gint64) top->Count + entry->Count - 1, G_MAXINT);
	}
}

static void
stack_replace (HtmlTree *tree, int index, int length, Entry *replacement, int n)
{
	GArray *saved = g_array_new (FALSE, FALSE, sizeof (Entry));
	int i;

	for (i = 0; i < n; i++) {
		Entry entry = replacement[i];

		entry.Name = g_ref_string_acquire (entry.Name);
		g_array_append_val (saved, entry);
	}

	for (i = index + length; i < stack_count (tree); i++) {
		Entry entry = (*stack_entry (tree, i));

		entry.Name = g_ref_string_acquire (entry.Name);
		g_array_append_val (saved, entry);
	}

	while (stack_count (tree) > index)
		stack_remove_top (tree);

	for (i = 0; i < (int) saved->len; i++) {
		Entry *entry = &g_array_index (saved, Entry, i);

		stack_push_entry (tree, entry);
		g_ref_string_release (entry->Name);
	}

	g_array_unref (saved);
}

static void
stack_remove (HtmlTree *tree, int index)
{
	if (stack_entry (tree, index)->Count > 1)
		stack_entry (tree, index)->Count--;
	else
		stack_replace (tree, index, 1, NULL, 0);
}

void
_g_mime_html_tree_clear_closed (HtmlTree *tree)
{
	guint i;

	for (i = 0; i < tree->closed->len; i++)
		g_ref_string_release ((char *) g_array_index (tree->closed, GMimeHtmlClosedElement, i).name);

	g_array_set_size (tree->closed, 0);
}

gboolean
_g_mime_html_tree_tracks_characters (HtmlTree *tree)
{
	return !tree->inFrameset && (tree->mode != AfterHead || tree->framesetOk || !tree->bodyCreated);
}

gboolean
_g_mime_html_tree_foreign (HtmlTree *tree)
{
	return stack_count (tree) && stack_entry (tree, stack_count (tree) - 1)->Namespace != GMIME_HTML_NAMESPACE_HTML;
}

gboolean
_g_mime_html_tree_noscript (HtmlTree *tree)
{
	return tree->html_ids[GMIME_HTML_TAG_NO_SCRIPT] >= 0;
}

#include "gmime-html-tree-rules-private.h"

HtmlTree *
_g_mime_html_tree_new (guint max)
{
	HtmlTree *tree = g_new0 (HtmlTree, 1);

	tree->entries = g_array_new (FALSE, FALSE, sizeof (Entry));
	tree->closed = g_array_new (FALSE, FALSE, sizeof (GMimeHtmlClosedElement));
	tree->html_names = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, NULL);
	tree->foreign_names = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, NULL);
	memset (tree->html_ids, 0xff, sizeof tree->html_ids);
	tree->max_depth = max;
	tree->framesetOk = TRUE;
	tree->formPointerIndex = -1;

	return tree;
}

void
_g_mime_html_tree_free (HtmlTree *tree)
{
	while (stack_count (tree))
		stack_remove_top (tree);

	_g_mime_html_tree_clear_closed (tree);
	g_array_unref (tree->entries);
	g_array_unref (tree->closed);
	g_hash_table_unref (tree->html_names);
	g_hash_table_unref (tree->foreign_names);
	g_free (tree);
}

GMimeHtmlTokenizerState
_g_mime_html_tree_tag (HtmlTree *tree, GMimeHtmlToken *tag)
{
	GMimeHtmlTokenizerState state;

	_g_mime_html_tree_clear_closed (tree);

	if (tag->is_end_tag) {
		process_end_tag (tree, tag);
		tree->disposition =
			tree->EndTagIgnored ? GMIME_HTML_TAG_DISPOSITION_IGNORED : GMIME_HTML_TAG_DISPOSITION_NORMAL;
		state = GMIME_HTML_TOKENIZER_STATE_DATA;
	} else {
		state = process_start_tag (tree, tag);
		tree->disposition = tree->StartTagIgnored          ? GMIME_HTML_TAG_DISPOSITION_IGNORED :
				    tree->StartTagAttributesMerged ? GMIME_HTML_TAG_DISPOSITION_ATTRIBUTES_MERGED :
								     GMIME_HTML_TAG_DISPOSITION_NORMAL;
	}

	return tree->depth_exceeded ? GMIME_HTML_TOKENIZER_STATE_PLAIN_TEXT : state;
}

void
_g_mime_html_tree_character (HtmlTree *tree, gunichar c)
{
	process_character_data (tree, c);
}
