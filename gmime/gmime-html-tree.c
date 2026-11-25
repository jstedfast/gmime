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

/* An active entry identifies one physical instance in a folded stack entry.
 * A negative index means it must be reconstructed; UNKNOWN denotes a marker. */
typedef struct {
	GMimeHtmlTagId id;
	int index, instance;
	GBytes *key;
} FormattingSlot;

static void
clear_slot (gpointer data)
{
	FormattingSlot *slot = data;
	g_clear_pointer (&slot->key, g_bytes_unref);
}

void
_g_mime_html_tree_set_formatting_capacity (HtmlTree *tree, guint capacity)
{
	/* Opt in before parsing, independently of max_depth. */
	g_return_if_fail (!tree->htmlCreated && !stack_count (tree));
	g_return_if_fail (capacity <= G_MAXINT);

	g_clear_pointer (&tree->formatting, g_array_unref);
	tree->formatting_capacity = capacity;
	tree->formatting_peak = 0;
	tree->formatting_stop = HTML_FORMATTING_STOP_NONE;

	if (capacity) {
		tree->formatting = g_array_new (FALSE, FALSE, sizeof (FormattingSlot));
		g_array_set_clear_func (tree->formatting, clear_slot);
	}
}

gboolean
_g_mime_html_tree_formatting_exceeded (HtmlTree *tree)
{
	return tree->formatting_stop != HTML_FORMATTING_STOP_NONE;
}

static inline FormattingSlot *
formatting_slot (HtmlTree *tree, int index)
{
	return &g_array_index (tree->formatting, FormattingSlot, index);
}

static gboolean
formatting_pending (HtmlTree *tree)
{
	return tree->formatting && tree->formatting->len &&
		formatting_slot (tree, tree->formatting->len - 1)->index < 0;
}

/* Bound the canonical key in UTF-16 code units, as in MimeKit. Don't retain
 * source tokens or unbounded strings. NULL means exact comparison is unavailable. */
static int
key_component (gunichar2 *out, int room, const char *s, gboolean fold)
{
	int n = 0;

	while (s && *s) {
		gunichar c = g_utf8_get_char (s);

		if (c == '\r' || n >= room)
			return -1;

		if (c > 0xffff) {
			if (n + 1 >= room)
				return -1;

			c -= 0x10000;
			out[n++] = 0xd800 | (c >> 10);
			out[n++] = 0xdc00 | (c & 0x3ff);
		} else {
			out[n++] = fold && c >= 'A' && c <= 'Z' ? c + 32 : c;
		}

		s = g_utf8_next_char (s);
	}

	return n;
}

static GBytes *
formatting_key (GPtrArray *attributes)
{
	gunichar2 key[128], pair[128];
	int length = 0;

	if (attributes && attributes->len > 4)
		return NULL;

	for (guint i = 0; attributes && i < attributes->len; i++) {
		const GMimeHtmlAttribute *attr = attributes->pdata[i];
		int pos = 0, order = 1, n, v, size;

		if ((n = key_component (pair + 1, 126, attr->name, TRUE)) < 0 ||
		    (v = key_component (pair + 2 + n, 126 - n, attr->value, FALSE)) < 0)
			return NULL;

		pair[0] = n;
		pair[n + 1] = v;

		while (pos < length) {
			int other = key[pos];

			order = memcmp (pair + 1, key + pos + 1, MIN (n, other) * sizeof *key);

			if (!order)
				order = n - other;

			if (order <= 0)
				break;

			pos += 2 + other + key[pos + 1 + other];
		}

		if (!order)
			continue; /* The first duplicate wins. */

		size = 2 + n + v;

		if (size > 128 - length)
			return NULL;

		memmove (key + pos + size, key + pos, (length - pos) * sizeof *key);
		memcpy (key + pos, pair, size * sizeof *key);
		length += size;
	}

	return g_bytes_new (key, length * sizeof *key);
}

static void
formatting_push (HtmlTree *tree, GMimeHtmlTagId id, GPtrArray *attributes)
{
	gboolean uncertain = FALSE;
	FormattingSlot slot;
	int matches = 0;

	if (tree->depth_exceeded || tree->formatting_stop)
		return;

	slot.key = formatting_key (attributes);

	if (id != GMIME_HTML_TAG_UNKNOWN) {
		for (int i = (int) tree->formatting->len - 1; i >= 0; i--) {
			FormattingSlot *other = formatting_slot (tree, i);

			if (other->id == GMIME_HTML_TAG_UNKNOWN)
				break;

			if (other->id != id)
				continue;

			if (other->key && slot.key) {
				if (!g_bytes_equal (other->key, slot.key))
					continue;
			} else {
				/* Unknown keys have attributes, so can't match the empty key. */
				if ((other->key && !g_bytes_get_size (other->key)) ||
				    (slot.key && !g_bytes_get_size (slot.key)))
					continue;

				uncertain = TRUE;
			}

			if (++matches == 3) {
				if (uncertain) {
					tree->formatting_stop = HTML_FORMATTING_STOP_ATTRIBUTE_EQUIVALENCE;
					clear_slot (&slot);
					return;
				}

				g_array_remove_index (tree->formatting, i);
				break;
			}
		}
	}

	if (tree->formatting->len == tree->formatting_capacity) {
		tree->formatting_stop = HTML_FORMATTING_STOP_CAPACITY;
		clear_slot (&slot);
		return;
	}

	slot.id = id;
	slot.index = stack_count (tree) - 1;
	slot.instance = stack_top (tree)->Count;

	g_array_append_val (tree->formatting, slot);

	tree->formatting_peak = MAX (tree->formatting_peak, tree->formatting->len);
}

static void
formatting_popped (HtmlTree *tree)
{
	for (guint i = 0; tree->formatting && i < tree->formatting->len; i++) {
		FormattingSlot *slot = formatting_slot (tree, i);

		if (slot->index < 0 ||
		    (slot->index < stack_count (tree) && slot->instance <= stack_entry (tree, slot->index)->Count))
			continue;

		if (slot->id == GMIME_HTML_TAG_UNKNOWN) {
			g_array_set_size (tree->formatting, i);
			break;
		}

		slot->index = -1;
	}
}

static void
formatting_reconstruct (HtmlTree *tree)
{
	int start;

	if (!formatting_pending (tree))
		return;

	start = tree->formatting->len;

	while (start && formatting_slot (tree, start - 1)->index < 0)
		start--;

	for (guint i = start; i < tree->formatting->len; i++) {
		FormattingSlot *slot = formatting_slot (tree, i);

		stack_push (tree, g_mime_html_tag_id_to_name (slot->id), slot->id, GMIME_HTML_NAMESPACE_HTML, F_None);

		if (tree->depth_exceeded)
			break;

		slot->index = stack_count (tree) - 1;
		slot->instance = stack_top (tree)->Count;
	}
}

static gboolean
reconstructs_on_characters (HtmlTree *tree)
{
	return formatting_pending (tree) && stack_top (tree)->PrevHtmlOrIntegrationPoint == stack_count (tree) - 1;
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
	return !tree->inFrameset &&
		(tree->mode != AfterHead || tree->framesetOk || !tree->bodyCreated || reconstructs_on_characters (tree));
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
	g_clear_pointer (&tree->formatting, g_array_unref);
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
		state = GMIME_HTML_TOKENIZER_STATE_DATA;
	} else {
		state = process_start_tag (tree, tag);
		tree->disposition = tree->StartTagIgnored          ? GMIME_HTML_TAG_DISPOSITION_IGNORED :
				    tree->StartTagAttributesMerged ? GMIME_HTML_TAG_DISPOSITION_ATTRIBUTES_MERGED :
								     GMIME_HTML_TAG_DISPOSITION_NORMAL;
	}

	return tree->depth_exceeded || tree->formatting_stop ? GMIME_HTML_TOKENIZER_STATE_PLAIN_TEXT : state;
}

void
_g_mime_html_tree_character (HtmlTree *tree, gunichar c)
{
	if (c && reconstructs_on_characters (tree)) {
		formatting_reconstruct (tree);

		if (tree->depth_exceeded)
			return;
	}

	if (c != ' ' && c != '\t' && c != '\r' && c != '\n' && c != '\f')
		process_character_data (tree, c);
}
