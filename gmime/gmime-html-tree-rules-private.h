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

/* Rules ported from MimeKit/Text/HtmlTreeConstruction.cs. */
static gboolean
ascii_match (const char *a, const char *b)
{
	return a && !g_ascii_strcasecmp (a, b);
}

static GMimeHtmlTokenizerState get_tokenizer_state (HtmlTree *tree, GMimeHtmlToken *tag);
static void pop_above (HtmlTree *tree, int index);
static void pop_through (HtmlTree *tree, int index);
static void remove_entry (HtmlTree *tree, int index);
static void process_character_data (HtmlTree *tree, gunichar c);
static void insert_implied_body (HtmlTree *tree);
static gboolean in_scope (HtmlTree *tree, int index, int boundary);
static void generate_implied_end_tags (HtmlTree *tree);
static gboolean has_implied_end_tag (HtmlTree *tree, Entry entry);
static gboolean is_heading (HtmlTree *tree, GMimeHtmlTagId id);
static ElementFlags get_html_flags (HtmlTree *tree, GMimeHtmlTagId id, const char *name);
static ElementFlags get_foreign_flags (HtmlTree *tree, GMimeHtmlNamespace ns, GMimeHtmlToken *tag);
static gboolean is_foreign_content_breakout (HtmlTree *tree, GMimeHtmlToken *tag);
static gboolean inserts_body (HtmlTree *tree, GMimeHtmlToken *tag);
static gboolean clears_frameset_ok (HtmlTree *tree, GMimeHtmlToken *tag);
static gboolean is_void_element (HtmlTree *tree, GMimeHtmlTagId id, const char *name);
static GMimeHtmlTokenizerState process_start_tag (HtmlTree *tree, GMimeHtmlToken *tag);
static gboolean process_head_start_tag (HtmlTree *tree, GMimeHtmlToken *tag);
static void close_p_in_button_scope (HtmlTree *tree);
static void close_list_item (HtmlTree *tree, int item);
static int current_table (HtmlTree *tree);
static void push_html (HtmlTree *tree, GMimeHtmlToken *tag);
static void push_implied (HtmlTree *tree, GMimeHtmlTagId id);
static void process_html_start_tag (HtmlTree *tree, GMimeHtmlToken *tag, InsertionMode entryMode, gboolean htmlExisted);
static void process_form_start_tag (HtmlTree *tree, GMimeHtmlToken *tag);
static void process_table_structure_start_tag (HtmlTree *tree, GMimeHtmlToken *tag);
static void process_end_tag (HtmlTree *tree, GMimeHtmlToken *tag);
static void process_head_end_tag (HtmlTree *tree, GMimeHtmlToken *tag);
static void process_end_tag_internal (HtmlTree *tree, GMimeHtmlToken *tag);
static void process_form_end_tag (HtmlTree *tree);
static void process_formatting_end_tag (HtmlTree *tree, GMimeHtmlTagId id);
static void process_html_end_tag (HtmlTree *tree, GMimeHtmlToken *tag);

static GMimeHtmlTokenizerState
get_tokenizer_state (HtmlTree *tree, GMimeHtmlToken *tag)
{
	// The self-closing flag does not affect the content state of non-void HTML elements.
	switch (tag->id) {
	case GMIME_HTML_TAG_STYLE:
	case GMIME_HTML_TAG_XMP:
	case GMIME_HTML_TAG_I_FRAME:
	case GMIME_HTML_TAG_NO_EMBED:
	case GMIME_HTML_TAG_NO_FRAMES:
		return GMIME_HTML_TOKENIZER_STATE_RAW_TEXT;
	case GMIME_HTML_TAG_TITLE:
	case GMIME_HTML_TAG_TEXT_AREA:
		return GMIME_HTML_TOKENIZER_STATE_RC_DATA;
	case GMIME_HTML_TAG_PLAIN_TEXT:
		return GMIME_HTML_TOKENIZER_STATE_PLAIN_TEXT;
	case GMIME_HTML_TAG_SCRIPT:
		return GMIME_HTML_TOKENIZER_STATE_SCRIPT_DATA;
	case GMIME_HTML_TAG_NO_SCRIPT:
		return tree->scripting ? GMIME_HTML_TOKENIZER_STATE_RAW_TEXT : GMIME_HTML_TOKENIZER_STATE_DATA;
	case GMIME_HTML_TAG_HTML:
		if (!tag->is_empty_element) {
			for (guint i = tag->attributes->len; i > 0; i--) {
				GMimeHtmlAttribute *attr = g_ptr_array_index (tag->attributes, i - 1);

				if (attr->id == GMIME_HTML_ATTRIBUTE_XML_NS && attr->value != NULL) {
					tree->ns = g_mime_html_namespace_from_url (attr->value);
					break;
				}
			}
		}

		break;
	default:
		break;
	}

	return GMIME_HTML_TOKENIZER_STATE_DATA;
}

static void
pop_above (HtmlTree *tree, int index)
{
	stack_pop_above (tree, index);

	if (tree->formPointerIndex >= stack_count (tree))
		tree->formPointerIndex = -1;
}

static void
pop_through (HtmlTree *tree, int index)
{
	tree->poppedThrough = TRUE;
	stack_pop_through (tree, index);

	if (tree->formPointerIndex >= stack_count (tree))
		tree->formPointerIndex = -1;
}

static void
remove_entry (HtmlTree *tree, int index)
{
	tree->poppedThrough = TRUE;
	stack_remove (tree, index);
}

static void
process_character_data (HtmlTree *tree, gunichar c)
{
	tree->htmlCreated = TRUE;

	if (tree->inFrameset)
		return;

	// Note: In the "in template" insertion mode, character data is processed as if in the body without closing the head.
	if (!(stack_index (tree, GMIME_HTML_NAMESPACE_HTML, GMIME_HTML_TAG_UNKNOWN, "template") >= 0)) {
		if (tree->mode == InHeadNoScript)
			pop_through (tree, stack_index (tree, GMIME_HTML_NAMESPACE_HTML, GMIME_HTML_TAG_NO_SCRIPT, ""));

		tree->mode = AfterHead;
		insert_implied_body (tree);
	}

	if (c != '\0')
		tree->framesetOk = FALSE;
}

static void
insert_implied_body (HtmlTree *tree)
{
	if (!tree->bodyCreated) {
		tree->bodyCreated = TRUE;
		tree->framesetOk = TRUE;
	}
}

static gboolean
in_scope (HtmlTree *tree, int index, int boundary)
{
	return index >= 0 && index >= boundary;
}

static void
generate_implied_end_tags (HtmlTree *tree)
{
	int index = stack_count (tree) - 1;

	while (index >= 0 && stack_entry (tree, index)->Namespace == GMIME_HTML_NAMESPACE_HTML &&
	       has_implied_end_tag (tree, (*stack_entry (tree, index))))
		index--;

	pop_above (tree, index);
}

static gboolean
has_implied_end_tag (HtmlTree *tree, Entry entry)
{
	switch (entry.Id) {
	case GMIME_HTML_TAG_DD:
	case GMIME_HTML_TAG_DT:
	case GMIME_HTML_TAG_LI:
	case GMIME_HTML_TAG_OPT_GROUP:
	case GMIME_HTML_TAG_OPTION:
	case GMIME_HTML_TAG_P:
	case GMIME_HTML_TAG_RP:
	case GMIME_HTML_TAG_RT:
		return TRUE;
	case GMIME_HTML_TAG_UNKNOWN:
		return !g_ascii_strcasecmp (entry.Name, "rb") || !g_ascii_strcasecmp (entry.Name, "rtc");
	default:
		return FALSE;
	}
}

static gboolean
is_heading (HtmlTree *tree, GMimeHtmlTagId id)
{
	return id >= GMIME_HTML_TAG_H1 && id <= GMIME_HTML_TAG_H6;
}

static ElementFlags
get_html_flags (HtmlTree *tree, GMimeHtmlTagId id, const char *name)
{
	switch (id) {
	case GMIME_HTML_TAG_APPLET:
	case GMIME_HTML_TAG_CAPTION:
	case GMIME_HTML_TAG_MARQUEE:
	case GMIME_HTML_TAG_OBJECT:
	case GMIME_HTML_TAG_TD:
	case GMIME_HTML_TAG_TH:
		return F_Special | F_Scope | F_ListItemStop;
	case GMIME_HTML_TAG_HTML:
	case GMIME_HTML_TAG_TABLE:
		return F_Special | F_Scope | F_TableScope | F_ListItemStop;
	case GMIME_HTML_TAG_OL:
	case GMIME_HTML_TAG_UL:
		return F_Special | F_ListItemScope | F_ListItemStop;
	case GMIME_HTML_TAG_BUTTON:
		return F_Special | F_ButtonScope | F_ListItemStop;
	case GMIME_HTML_TAG_H1:
	case GMIME_HTML_TAG_H2:
	case GMIME_HTML_TAG_H3:
	case GMIME_HTML_TAG_H4:
	case GMIME_HTML_TAG_H5:
	case GMIME_HTML_TAG_H6:
		return F_Special | F_Heading | F_ListItemStop;
	case GMIME_HTML_TAG_ADDRESS:
	case GMIME_HTML_TAG_DIV:
	case GMIME_HTML_TAG_P:
		return F_Special;
	case GMIME_HTML_TAG_AREA:
	case GMIME_HTML_TAG_ARTICLE:
	case GMIME_HTML_TAG_ASIDE:
	case GMIME_HTML_TAG_BASE:
	case GMIME_HTML_TAG_BASE_FONT:
	case GMIME_HTML_TAG_BG_SOUND:
	case GMIME_HTML_TAG_BLOCK_QUOTE:
	case GMIME_HTML_TAG_BODY:
	case GMIME_HTML_TAG_BR:
	case GMIME_HTML_TAG_CENTER:
	case GMIME_HTML_TAG_COL:
	case GMIME_HTML_TAG_COL_GROUP:
	case GMIME_HTML_TAG_DD:
	case GMIME_HTML_TAG_DETAILS:
	case GMIME_HTML_TAG_DIR:
	case GMIME_HTML_TAG_DL:
	case GMIME_HTML_TAG_DT:
	case GMIME_HTML_TAG_EMBED:
	case GMIME_HTML_TAG_FIELD_SET:
	case GMIME_HTML_TAG_FIG_CAPTION:
	case GMIME_HTML_TAG_FIGURE:
	case GMIME_HTML_TAG_FOOTER:
	case GMIME_HTML_TAG_FORM:
	case GMIME_HTML_TAG_FRAME:
	case GMIME_HTML_TAG_FRAME_SET:
	case GMIME_HTML_TAG_HEAD:
	case GMIME_HTML_TAG_HEADER:
	case GMIME_HTML_TAG_HR:
	case GMIME_HTML_TAG_I_FRAME:
	case GMIME_HTML_TAG_IMAGE:
	case GMIME_HTML_TAG_INPUT:
	case GMIME_HTML_TAG_KEYGEN:
	case GMIME_HTML_TAG_LI:
	case GMIME_HTML_TAG_LINK:
	case GMIME_HTML_TAG_LISTING:
	case GMIME_HTML_TAG_MAIN:
	case GMIME_HTML_TAG_MENU:
	case GMIME_HTML_TAG_META:
	case GMIME_HTML_TAG_NAV:
	case GMIME_HTML_TAG_NO_EMBED:
	case GMIME_HTML_TAG_NO_FRAMES:
	case GMIME_HTML_TAG_NO_SCRIPT:
	case GMIME_HTML_TAG_PARAM:
	case GMIME_HTML_TAG_PLAIN_TEXT:
	case GMIME_HTML_TAG_PRE:
	case GMIME_HTML_TAG_SCRIPT:
	case GMIME_HTML_TAG_SECTION:
	case GMIME_HTML_TAG_SELECT:
	case GMIME_HTML_TAG_SOURCE:
	case GMIME_HTML_TAG_STYLE:
	case GMIME_HTML_TAG_SUMMARY:
	case GMIME_HTML_TAG_T_BODY:
	case GMIME_HTML_TAG_TEXT_AREA:
	case GMIME_HTML_TAG_TFOOT:
	case GMIME_HTML_TAG_T_HEAD:
	case GMIME_HTML_TAG_TITLE:
	case GMIME_HTML_TAG_TR:
	case GMIME_HTML_TAG_TRACK:
	case GMIME_HTML_TAG_WBR:
	case GMIME_HTML_TAG_XMP:
		return F_Special | F_ListItemStop;
	case GMIME_HTML_TAG_UNKNOWN:
		if (!g_ascii_strcasecmp (name, "template"))
			return F_Special | F_Scope | F_TableScope | F_ListItemStop;

		if (!g_ascii_strcasecmp (name, "hgroup") || !g_ascii_strcasecmp (name, "search"))
			return F_Special | F_ListItemStop;

		return F_None;
	default:
		return F_None;
	}
}

static ElementFlags
get_foreign_flags (HtmlTree *tree, GMimeHtmlNamespace ns, GMimeHtmlToken *tag)
{
	const ElementFlags IntegrationPoint = F_Special | F_Scope | F_ListItemStop;
	const char *name = tag->name;

	if (ns == GMIME_HTML_NAMESPACE_MATH_ML) {
		switch (strlen (name)) {
		case 2:
			if (!g_ascii_strcasecmp (name, "mi") || !g_ascii_strcasecmp (name, "mo") ||
			    !g_ascii_strcasecmp (name, "mn") || !g_ascii_strcasecmp (name, "ms"))
				return IntegrationPoint | F_MathMLTextIntegrationPoint;

			break;
		case 5:
			if (!g_ascii_strcasecmp (name, "mtext"))
				return IntegrationPoint | F_MathMLTextIntegrationPoint;

			break;
		case 14:
			if (!g_ascii_strcasecmp (name, "annotation-xml")) {
				for (guint i = 0; i < tag->attributes->len; i++) {
					GMimeHtmlAttribute *attr = g_ptr_array_index (tag->attributes, i);

					if (!g_ascii_strcasecmp (attr->name, "encoding")) {
						if (attr->value != NULL &&
						    (!g_ascii_strcasecmp (attr->value, "text/html") ||
						     !g_ascii_strcasecmp (attr->value, "application/xhtml+xml")))
							return IntegrationPoint | F_HtmlIntegrationPoint;

						break;
					}
				}

				return IntegrationPoint;
			}

			break;
		default:
			break;
		}
	} else if (ns == GMIME_HTML_NAMESPACE_SVG) {
		if (!g_ascii_strcasecmp (name, "foreignObject") || !g_ascii_strcasecmp (name, "desc") ||
		    !g_ascii_strcasecmp (name, "title"))
			return IntegrationPoint | F_HtmlIntegrationPoint;
	}

	return F_None;
}

static gboolean
is_foreign_content_breakout (HtmlTree *tree, GMimeHtmlToken *tag)
{
	switch (tag->id) {
	case GMIME_HTML_TAG_B:
	case GMIME_HTML_TAG_BIG:
	case GMIME_HTML_TAG_BLOCK_QUOTE:
	case GMIME_HTML_TAG_BODY:
	case GMIME_HTML_TAG_BR:
	case GMIME_HTML_TAG_CENTER:
	case GMIME_HTML_TAG_CODE:
	case GMIME_HTML_TAG_DD:
	case GMIME_HTML_TAG_DIV:
	case GMIME_HTML_TAG_DL:
	case GMIME_HTML_TAG_DT:
	case GMIME_HTML_TAG_EM:
	case GMIME_HTML_TAG_EMBED:
	case GMIME_HTML_TAG_H1:
	case GMIME_HTML_TAG_H2:
	case GMIME_HTML_TAG_H3:
	case GMIME_HTML_TAG_H4:
	case GMIME_HTML_TAG_H5:
	case GMIME_HTML_TAG_H6:
	case GMIME_HTML_TAG_HEAD:
	case GMIME_HTML_TAG_HR:
	case GMIME_HTML_TAG_I:
	case GMIME_HTML_TAG_IMAGE:
	case GMIME_HTML_TAG_LI:
	case GMIME_HTML_TAG_LISTING:
	case GMIME_HTML_TAG_MENU:
	case GMIME_HTML_TAG_META:
	case GMIME_HTML_TAG_NO_BR:
	case GMIME_HTML_TAG_OL:
	case GMIME_HTML_TAG_P:
	case GMIME_HTML_TAG_PRE:
	case GMIME_HTML_TAG_RUBY:
	case GMIME_HTML_TAG_S:
	case GMIME_HTML_TAG_SMALL:
	case GMIME_HTML_TAG_SPAN:
	case GMIME_HTML_TAG_STRONG:
	case GMIME_HTML_TAG_STRIKE:
	case GMIME_HTML_TAG_SUB:
	case GMIME_HTML_TAG_SUP:
	case GMIME_HTML_TAG_TABLE:
	case GMIME_HTML_TAG_TT:
	case GMIME_HTML_TAG_U:
	case GMIME_HTML_TAG_UL:
	case GMIME_HTML_TAG_VAR:
		return TRUE;
	case GMIME_HTML_TAG_FONT:
		for (guint i = 0; i < tag->attributes->len; i++) {
			switch (((GMimeHtmlAttribute *) g_ptr_array_index (tag->attributes, i))->id) {
			case GMIME_HTML_ATTRIBUTE_COLOR:
			case GMIME_HTML_ATTRIBUTE_FACE:
			case GMIME_HTML_ATTRIBUTE_SIZE:
				return TRUE;
			default:
				break;
			}
		}

		return FALSE;
	default:
		return FALSE;
	}
}

static gboolean
inserts_body (HtmlTree *tree, GMimeHtmlToken *tag)
{
	switch (tag->id) {
	case GMIME_HTML_TAG_HTML:
	case GMIME_HTML_TAG_HEAD:
	case GMIME_HTML_TAG_FRAME_SET:
	case GMIME_HTML_TAG_BASE:
	case GMIME_HTML_TAG_BASE_FONT:
	case GMIME_HTML_TAG_BG_SOUND:
	case GMIME_HTML_TAG_LINK:
	case GMIME_HTML_TAG_META:
	case GMIME_HTML_TAG_NO_FRAMES:
	case GMIME_HTML_TAG_SCRIPT:
	case GMIME_HTML_TAG_STYLE:
	case GMIME_HTML_TAG_TITLE:
		return FALSE;
	case GMIME_HTML_TAG_UNKNOWN:
		return !!g_ascii_strcasecmp (tag->name, "template");
	default:
		return TRUE;
	}
}

static gboolean
clears_frameset_ok (HtmlTree *tree, GMimeHtmlToken *tag)
{
	switch (tag->id) {
	case GMIME_HTML_TAG_PRE:
	case GMIME_HTML_TAG_LISTING:
	case GMIME_HTML_TAG_LI:
	case GMIME_HTML_TAG_DD:
	case GMIME_HTML_TAG_DT:
	case GMIME_HTML_TAG_BUTTON:
	case GMIME_HTML_TAG_APPLET:
	case GMIME_HTML_TAG_MARQUEE:
	case GMIME_HTML_TAG_OBJECT:
	case GMIME_HTML_TAG_TABLE:
	case GMIME_HTML_TAG_AREA:
	case GMIME_HTML_TAG_BR:
	case GMIME_HTML_TAG_EMBED:
	case GMIME_HTML_TAG_IMAGE:
	case GMIME_HTML_TAG_KEYGEN:
	case GMIME_HTML_TAG_WBR:
	case GMIME_HTML_TAG_HR:
	case GMIME_HTML_TAG_TEXT_AREA:
	case GMIME_HTML_TAG_XMP:
	case GMIME_HTML_TAG_I_FRAME:
	case GMIME_HTML_TAG_SELECT:
	case GMIME_HTML_TAG_BODY:
		return TRUE;
	case GMIME_HTML_TAG_INPUT:
		// Note: Only the first attribute with a given name counts, since browsers drop any duplicates.
		for (guint i = 0; i < tag->attributes->len; i++) {
			GMimeHtmlAttribute *attr = g_ptr_array_index (tag->attributes, i);

			if (attr->id == GMIME_HTML_ATTRIBUTE_TYPE)
				return !ascii_match (attr->value, "hidden");
		}

		return TRUE;
	case GMIME_HTML_TAG_UNKNOWN:
		// Note: The tree builder treats an <image> start tag as an <img> start tag. A <template> start tag also sets
		// the frameset-ok flag to "not ok" (even in the <head>).
		return !g_ascii_strcasecmp (tag->name, "image") || !g_ascii_strcasecmp (tag->name, "template");
	default:
		return FALSE;
	}
}

static gboolean
is_void_element (HtmlTree *tree, GMimeHtmlTagId id, const char *name)
{
	switch (id) {
	case GMIME_HTML_TAG_BASE_FONT:
	case GMIME_HTML_TAG_BG_SOUND:
	case GMIME_HTML_TAG_FRAME:
		return TRUE;
	case GMIME_HTML_TAG_UNKNOWN:
		return !g_ascii_strcasecmp (name, "image");
	default:
		return g_mime_html_tag_id_is_empty_element (id);
	}
}

static GMimeHtmlTokenizerState
process_start_tag (HtmlTree *tree, GMimeHtmlToken *tag)
{
	gboolean htmlExisted = tree->htmlCreated;
	InsertionMode entryMode = tree->mode;

	tree->StartTagAttributesMerged = FALSE;
	tree->BrokeOutOfForeignContent = FALSE;
	tree->StartTagIgnored = FALSE;
	tree->htmlCreated = TRUE;

	// Note: In the frameset insertion modes, every start tag other than <html>, <frameset>, <frame> and <noframes> is
	// ignored, so <noframes> is the only start tag that switches the tokenizer state.
	if (tree->inFrameset) {
		switch (tag->id) {
		case GMIME_HTML_TAG_HTML:
			tree->StartTagAttributesMerged = tag->attributes->len > 0;
			break;
		case GMIME_HTML_TAG_FRAME_SET:
			// Note: The self-closing flag is ignored for the <frameset> element.
			if (tree->framesetDepth > 0)
				tree->framesetDepth++;
			else
				tree->StartTagIgnored = TRUE;

			break;
		case GMIME_HTML_TAG_FRAME:
			tree->StartTagIgnored = tree->framesetDepth == 0;
			break;
		case GMIME_HTML_TAG_NO_FRAMES:
			break;
		default:
			tree->StartTagIgnored = TRUE;
			break;
		}

		return tag->id == GMIME_HTML_TAG_NO_FRAMES ? GMIME_HTML_TOKENIZER_STATE_RAW_TEXT :
							     GMIME_HTML_TOKENIZER_STATE_DATA;
	}

	if (tree->mode != AfterHead && !process_head_start_tag (tree, tag)) {
		tree->StartTagIgnored = TRUE;
		return get_tokenizer_state (tree, tag);
	}

	if (stack_count (tree) > 0) {
		Entry top = (*stack_entry (tree, stack_count (tree) - 1));

		if (top.Namespace != GMIME_HTML_NAMESPACE_HTML) {
			gboolean html;

			if ((top.Flags & F_HtmlIntegrationPoint) != 0) {
				html = TRUE;
			} else if ((top.Flags & F_MathMLTextIntegrationPoint) != 0) {
				html = !!g_ascii_strcasecmp (tag->name, "mglyph") &&
				       !!g_ascii_strcasecmp (tag->name, "malignmark");
			} else if (top.Namespace == GMIME_HTML_NAMESPACE_MATH_ML &&
				   !g_ascii_strcasecmp (top.Name, "annotation-xml")) {
				html = !g_ascii_strcasecmp (tag->name, "svg");
			} else {
				html = FALSE;
			}

			if (!html) {
				if (is_foreign_content_breakout (tree, tag)) {
					// Pop until the current node is a MathML text integration point, an HTML integration point,
					// or an element in the HTML namespace, then reprocess the token using the HTML rules.
					pop_above (tree, stack_top (tree)->PrevHtmlOrIntegrationPoint);
					tree->BrokeOutOfForeignContent = TRUE;
				} else {
					// Insert a foreign element for the token in the same namespace as the adjusted current node.
					GMimeHtmlNamespace ns = top.Namespace;

					if (!tag->is_empty_element)
						stack_push (tree, tag->name, GMIME_HTML_TAG_UNKNOWN, ns,
							    get_foreign_flags (tree, ns, tag));

					return GMIME_HTML_TOKENIZER_STATE_DATA;
				}
			}
		}
	}

	process_html_start_tag (tree, tag, entryMode, htmlExisted);

	return get_tokenizer_state (tree, tag);
}

static gboolean
process_head_start_tag (HtmlTree *tree, GMimeHtmlToken *tag)
{
	// Note: In the "in template" insertion mode, any start tag that does not belong in the head is processed as
	// if in the body without closing the head (and a <noscript> start tag does not belong in the head).
	if ((stack_index (tree, GMIME_HTML_NAMESPACE_HTML, GMIME_HTML_TAG_UNKNOWN, "template") >= 0))
		return TRUE;

	switch (tree->mode) {
	case BeforeHead:
		// Note: Any start tag other than <html> implies a <head> start tag (if it isn't one) and gets reprocessed
		// using the "in head" insertion mode.
		if (tag->id == GMIME_HTML_TAG_HTML)
			return TRUE;

		tree->mode = InHead;
		break;
	case InHeadNoScript:
		switch (tag->id) {
		case GMIME_HTML_TAG_HTML:
		case GMIME_HTML_TAG_BASE_FONT:
		case GMIME_HTML_TAG_BG_SOUND:
		case GMIME_HTML_TAG_LINK:
		case GMIME_HTML_TAG_META:
		case GMIME_HTML_TAG_NO_FRAMES:
		case GMIME_HTML_TAG_STYLE:
			return TRUE;
		case GMIME_HTML_TAG_HEAD:
		case GMIME_HTML_TAG_NO_SCRIPT:
			return FALSE;
		default:
			break;
		}

		// Any other start tag closes the <noscript> element and gets reprocessed using the "in head" insertion mode.
		pop_through (tree, stack_index (tree, GMIME_HTML_NAMESPACE_HTML, GMIME_HTML_TAG_NO_SCRIPT, ""));
		tree->mode = InHead;
		break;
	default:
		break;
	}

	switch (tag->id) {
	case GMIME_HTML_TAG_HTML:
	case GMIME_HTML_TAG_HEAD:
	case GMIME_HTML_TAG_BASE:
	case GMIME_HTML_TAG_BASE_FONT:
	case GMIME_HTML_TAG_BG_SOUND:
	case GMIME_HTML_TAG_LINK:
	case GMIME_HTML_TAG_META:
	case GMIME_HTML_TAG_TITLE:
	case GMIME_HTML_TAG_NO_FRAMES:
	case GMIME_HTML_TAG_STYLE:
	case GMIME_HTML_TAG_SCRIPT:
		return TRUE;
	case GMIME_HTML_TAG_NO_SCRIPT:
		// Note: When scripting is enabled, the content of the <noscript> element is raw text.
		if (!tree->scripting)
			tree->mode = InHeadNoScript;

		return TRUE;
	case GMIME_HTML_TAG_UNKNOWN:
		if (!g_ascii_strcasecmp (tag->name, "template"))
			return TRUE;

		break;
	default:
		break;
	}

	// Any other start tag closes the <head> element.
	tree->mode = AfterHead;

	return TRUE;
}

static void
close_p_in_button_scope (HtmlTree *tree)
{
	int p = stack_index (tree, GMIME_HTML_NAMESPACE_HTML, GMIME_HTML_TAG_P, "");

	if (in_scope (tree, p, stack_top (tree)->PrevButtonScope))
		pop_through (tree, p);
}

static void
close_list_item (HtmlTree *tree, int item)
{
	// Walk the stack from the current node: if an item is found before a special element other than
	// address, div or p, close it.
	if (item >= 0 && item >= stack_top (tree)->PrevListItemStop)
		pop_through (tree, item);
}

static int
current_table (HtmlTree *tree)
{
	int table = stack_index (tree, GMIME_HTML_NAMESPACE_HTML, GMIME_HTML_TAG_TABLE, "");

	return table >= 0 && table == stack_top (tree)->PrevTableScope ? table : -1;
}

static void
push_html (HtmlTree *tree, GMimeHtmlToken *tag)
{
	stack_push (tree, tag->name, tag->id, GMIME_HTML_NAMESPACE_HTML, get_html_flags (tree, tag->id, tag->name));
}

static void
push_implied (HtmlTree *tree, GMimeHtmlTagId id)
{
	stack_push (tree, g_mime_html_tag_id_to_name (id), id, GMIME_HTML_NAMESPACE_HTML,
		    get_html_flags (tree, id, ""));
}

static void
process_html_start_tag (HtmlTree *tree, GMimeHtmlToken *tag, InsertionMode entryMode, gboolean htmlExisted)
{
	gboolean hadBody = tree->bodyCreated;
	GMimeHtmlTagId id = tag->id;
	int index, table;

	// Note: In the "after head" insertion mode, any start tag that does not belong in the head (other than <body> and
	// <frameset>) inserts the <body> element.
	if (!tree->bodyCreated && tree->mode == AfterHead && inserts_body (tree, tag) &&
	    !(stack_index (tree, GMIME_HTML_NAMESPACE_HTML, GMIME_HTML_TAG_UNKNOWN, "template") >= 0)) {
		if (id == GMIME_HTML_TAG_BODY)
			tree->bodyCreated = TRUE;
		else
			insert_implied_body (tree);
	}

	if (tree->framesetOk && clears_frameset_ok (tree, tag))
		tree->framesetOk = FALSE;

	switch (id) {
	case GMIME_HTML_TAG_HTML:
		// Note: The <html> element is never pushed more than once, so its start tag has no effect on the stack. A
		// subsequent <html> start tag is ignored if there is a <template> element on the stack; otherwise, its
		// attributes get added to the existing <html> element.
		if ((stack_index (tree, GMIME_HTML_NAMESPACE_HTML, GMIME_HTML_TAG_UNKNOWN, "template") >= 0))
			tree->StartTagIgnored = TRUE;
		else
			tree->StartTagAttributesMerged = htmlExisted && tag->attributes->len > 0;

		return;
	case GMIME_HTML_TAG_HEAD:
		// Note: The <head> element is never pushed more than once either, so any <head> start tag that appears after
		// the "before head" insertion mode gets ignored.
		tree->StartTagIgnored = entryMode != BeforeHead;
		return;
	case GMIME_HTML_TAG_BODY:
		// Note: The <body> element is never pushed more than once either, but a <body> start tag sets the frameset-ok
		// flag to "not ok" (see ClearsFramesetOk). A subsequent <body> start tag is ignored if there is a <template>
		// element on the stack; otherwise, its attributes get added to the existing <body> element.
		if ((stack_index (tree, GMIME_HTML_NAMESPACE_HTML, GMIME_HTML_TAG_UNKNOWN, "template") >= 0))
			tree->StartTagIgnored = TRUE;
		else
			tree->StartTagAttributesMerged = hadBody && tag->attributes->len > 0;

		return;
	case GMIME_HTML_TAG_FRAME:
		// Note: Outside of a <frameset>, the <frame> start tag is ignored.
		tree->StartTagIgnored = TRUE;
		return;
	case GMIME_HTML_TAG_FRAME_SET:
		// Note: Before the <body> has been inserted, the <frameset> element gets inserted no matter what the frameset-ok
		// flag is (e.g. after a <template> in the <head>, which sets it to "not ok"). Otherwise, unless the frameset-ok flag
		// is "not ok", the <frameset> element replaces the <body> (and everything in it). The <frameset> start tag is also
		// ignored if there is a <template> element on the stack.
		if ((tree->framesetOk || !tree->bodyCreated) &&
		    !(stack_index (tree, GMIME_HTML_NAMESPACE_HTML, GMIME_HTML_TAG_UNKNOWN, "template") >= 0)) {
			pop_above (tree, -1);
			tree->inFrameset = TRUE;
			tree->framesetDepth = 1;
		} else {
			tree->StartTagIgnored = TRUE;
		}

		return;
	case GMIME_HTML_TAG_ADDRESS:
	case GMIME_HTML_TAG_ARTICLE:
	case GMIME_HTML_TAG_ASIDE:
	case GMIME_HTML_TAG_BLOCK_QUOTE:
	case GMIME_HTML_TAG_CENTER:
	case GMIME_HTML_TAG_DETAILS:
	case GMIME_HTML_TAG_DIALOG:
	case GMIME_HTML_TAG_DIR:
	case GMIME_HTML_TAG_DIV:
	case GMIME_HTML_TAG_DL:
	case GMIME_HTML_TAG_FIELD_SET:
	case GMIME_HTML_TAG_FIG_CAPTION:
	case GMIME_HTML_TAG_FIGURE:
	case GMIME_HTML_TAG_FOOTER:
	case GMIME_HTML_TAG_HEADER:
	case GMIME_HTML_TAG_MAIN:
	case GMIME_HTML_TAG_MENU:
	case GMIME_HTML_TAG_NAV:
	case GMIME_HTML_TAG_OL:
	case GMIME_HTML_TAG_P:
	case GMIME_HTML_TAG_SECTION:
	case GMIME_HTML_TAG_SUMMARY:
	case GMIME_HTML_TAG_UL:
	case GMIME_HTML_TAG_PRE:
	case GMIME_HTML_TAG_LISTING:
	case GMIME_HTML_TAG_PLAIN_TEXT:
	case GMIME_HTML_TAG_XMP:
		close_p_in_button_scope (tree);
		push_html (tree, tag);
		return;
	case GMIME_HTML_TAG_FORM:
		process_form_start_tag (tree, tag);
		return;
	case GMIME_HTML_TAG_H1:
	case GMIME_HTML_TAG_H2:
	case GMIME_HTML_TAG_H3:
	case GMIME_HTML_TAG_H4:
	case GMIME_HTML_TAG_H5:
	case GMIME_HTML_TAG_H6:
		close_p_in_button_scope (tree);

		if (stack_count (tree) > 0 &&
		    stack_entry (tree, stack_count (tree) - 1)->Namespace == GMIME_HTML_NAMESPACE_HTML &&
		    is_heading (tree, stack_entry (tree, stack_count (tree) - 1)->Id))
			pop_through (tree, (stack_count (tree) - 1));

		push_html (tree, tag);
		return;
	case GMIME_HTML_TAG_HR:
		close_p_in_button_scope (tree);
		return;
	case GMIME_HTML_TAG_LI:
		close_list_item (tree, stack_index (tree, GMIME_HTML_NAMESPACE_HTML, GMIME_HTML_TAG_LI, ""));
		close_p_in_button_scope (tree);
		push_html (tree, tag);
		return;
	case GMIME_HTML_TAG_DD:
	case GMIME_HTML_TAG_DT:
		close_list_item (tree, MAX (stack_index (tree, GMIME_HTML_NAMESPACE_HTML, GMIME_HTML_TAG_DD, ""),
					    stack_index (tree, GMIME_HTML_NAMESPACE_HTML, GMIME_HTML_TAG_DT, "")));
		close_p_in_button_scope (tree);
		push_html (tree, tag);
		return;
	case GMIME_HTML_TAG_BUTTON:
		index = stack_index (tree, GMIME_HTML_NAMESPACE_HTML, GMIME_HTML_TAG_BUTTON, "");

		if (in_scope (tree, index, stack_top (tree)->PrevScope))
			pop_through (tree, index);

		push_html (tree, tag);
		return;
	case GMIME_HTML_TAG_A:
		// Approximates the adoption agency algorithm being run for a nested <a>: when there is no
		// special element above the open <a>, the open <a> gets popped.
		index = stack_index (tree, GMIME_HTML_NAMESPACE_HTML, GMIME_HTML_TAG_A, "");

		if (index >= 0 && stack_top (tree)->PrevSpecial < index)
			pop_through (tree, index);

		push_html (tree, tag);
		return;
	case GMIME_HTML_TAG_OPTION:
	case GMIME_HTML_TAG_OPT_GROUP:
		if (stack_is_top_html (tree, GMIME_HTML_TAG_OPTION))
			pop_through (tree, (stack_count (tree) - 1));

		push_html (tree, tag);
		return;
	case GMIME_HTML_TAG_TABLE:
		table = current_table (tree);

		if (table >= 0 &&
		    MAX (MAX (stack_index (tree, GMIME_HTML_NAMESPACE_HTML, GMIME_HTML_TAG_TD, ""),
			      stack_index (tree, GMIME_HTML_NAMESPACE_HTML, GMIME_HTML_TAG_TH, "")),
			 stack_index (tree, GMIME_HTML_NAMESPACE_HTML, GMIME_HTML_TAG_CAPTION, "")) < table) {
			// In the "in table", "in table body" or "in row" insertion modes, a <table> start tag closes the current table.
			pop_through (tree, table);
		} else {
			close_p_in_button_scope (tree);
		}

		push_html (tree, tag);
		return;
	case GMIME_HTML_TAG_CAPTION:
	case GMIME_HTML_TAG_COL_GROUP:
	case GMIME_HTML_TAG_COL:
	case GMIME_HTML_TAG_T_BODY:
	case GMIME_HTML_TAG_T_HEAD:
	case GMIME_HTML_TAG_TFOOT:
	case GMIME_HTML_TAG_TR:
	case GMIME_HTML_TAG_TD:
	case GMIME_HTML_TAG_TH:
		process_table_structure_start_tag (tree, tag);
		return;
	case GMIME_HTML_TAG_UNKNOWN:
		if (!g_ascii_strcasecmp (tag->name, "svg")) {
			if (!tag->is_empty_element)
				stack_push (tree, tag->name, GMIME_HTML_TAG_UNKNOWN, GMIME_HTML_NAMESPACE_SVG, F_None);

			return;
		}

		if (!g_ascii_strcasecmp (tag->name, "math")) {
			if (!tag->is_empty_element)
				stack_push (tree, tag->name, GMIME_HTML_TAG_UNKNOWN, GMIME_HTML_NAMESPACE_MATH_ML,
					    F_None);

			return;
		}

		if (!g_ascii_strcasecmp (tag->name, "hgroup") || !g_ascii_strcasecmp (tag->name, "search")) {
			close_p_in_button_scope (tree);
			push_html (tree, tag);
			return;
		}

		break;
	default:
		break;
	}

	// Note: The self-closing flag is ignored for HTML elements that are not void elements.
	if (!is_void_element (tree, id, tag->name))
		push_html (tree, tag);
}

static void
process_form_start_tag (HtmlTree *tree, GMimeHtmlToken *tag)
{
	gboolean templateOpen =
		(stack_index (tree, GMIME_HTML_NAMESPACE_HTML, GMIME_HTML_TAG_UNKNOWN, "template") >= 0);

	// Note: Unless there is a <template> element on the stack, a <form> start tag is ignored if the form element
	// pointer is set (even if the <form> element that it points to has already been closed).
	if (tree->formPointerSet && !templateOpen) {
		tree->StartTagIgnored = TRUE;
		return;
	}

	// Note: In the "in table", "in table body" and "in row" insertion modes, the <form> element is popped as soon as
	// it gets inserted (or the start tag is ignored if there is a <template> element on the stack).
	if (stack_count (tree) > 0 &&
	    stack_entry (tree, stack_count (tree) - 1)->Namespace == GMIME_HTML_NAMESPACE_HTML) {
		switch (stack_entry (tree, stack_count (tree) - 1)->Id) {
		case GMIME_HTML_TAG_TABLE:
		case GMIME_HTML_TAG_T_BODY:
		case GMIME_HTML_TAG_TFOOT:
		case GMIME_HTML_TAG_T_HEAD:
		case GMIME_HTML_TAG_TR:
			if (!templateOpen)
				tree->formPointerSet = TRUE;
			else
				tree->StartTagIgnored = TRUE;

			return;
		default:
			break;
		}
	}

	close_p_in_button_scope (tree);
	push_html (tree, tag);

	if (!templateOpen) {
		tree->formPointerSet = TRUE;
		tree->formPointerIndex = stack_is_top_html (tree, GMIME_HTML_TAG_FORM) ? stack_count (tree) - 1 : -1;
	}
}

static void
process_table_structure_start_tag (HtmlTree *tree, GMimeHtmlToken *tag)
{
	int table = current_table (tree);
	int index;

	// Table structure start tags outside of a table are ignored.
	if (table < 0) {
		tree->StartTagIgnored = stack_top (tree)->PrevTableScope < 0;
		return;
	}

	switch (tag->id) {
	case GMIME_HTML_TAG_CAPTION:
	case GMIME_HTML_TAG_COL_GROUP:
		pop_above (tree, table);
		push_html (tree, tag);
		break;
	case GMIME_HTML_TAG_COL:
		pop_above (tree, table);
		push_implied (tree, GMIME_HTML_TAG_COL_GROUP);
		break;
	case GMIME_HTML_TAG_T_BODY:
	case GMIME_HTML_TAG_T_HEAD:
	case GMIME_HTML_TAG_TFOOT:
		pop_above (tree, table);
		push_html (tree, tag);
		break;
	case GMIME_HTML_TAG_TR:
		index = MAX (MAX (stack_index (tree, GMIME_HTML_NAMESPACE_HTML, GMIME_HTML_TAG_T_BODY, ""),
				  stack_index (tree, GMIME_HTML_NAMESPACE_HTML, GMIME_HTML_TAG_T_HEAD, "")),
			     stack_index (tree, GMIME_HTML_NAMESPACE_HTML, GMIME_HTML_TAG_TFOOT, ""));

		if (index > table) {
			pop_above (tree, index);
		} else {
			pop_above (tree, table);
			push_implied (tree, GMIME_HTML_TAG_T_BODY);
		}

		push_html (tree, tag);
		break;
	default: // td, th
		index = stack_index (tree, GMIME_HTML_NAMESPACE_HTML, GMIME_HTML_TAG_TR, "");

		if (index > table) {
			pop_above (tree, index);
		} else {
			index = MAX (MAX (stack_index (tree, GMIME_HTML_NAMESPACE_HTML, GMIME_HTML_TAG_T_BODY, ""),
					  stack_index (tree, GMIME_HTML_NAMESPACE_HTML, GMIME_HTML_TAG_T_HEAD, "")),
				     stack_index (tree, GMIME_HTML_NAMESPACE_HTML, GMIME_HTML_TAG_TFOOT, ""));

			if (index > table) {
				pop_above (tree, index);
			} else {
				pop_above (tree, table);
				push_implied (tree, GMIME_HTML_TAG_T_BODY);
			}

			push_implied (tree, GMIME_HTML_TAG_TR);
		}

		push_html (tree, tag);
		break;
	}
}

static void
process_end_tag (HtmlTree *tree, GMimeHtmlToken *tag)
{
	tree->BrokeOutOfForeignContent = FALSE;
	tree->poppedThrough = FALSE;

	// Note: In the "before html" insertion mode, only </head>, </body>, </html> and </br> create the <html> element.
	if (tag->id == GMIME_HTML_TAG_HEAD || tag->id == GMIME_HTML_TAG_BODY || tag->id == GMIME_HTML_TAG_HTML ||
	    tag->id == GMIME_HTML_TAG_BR)
		tree->htmlCreated = TRUE;

	// Note: In the frameset insertion modes, end tags never affect anything that matters to the tokenizer.
	if (tree->inFrameset) {
		if (tag->id == GMIME_HTML_TAG_FRAME_SET && tree->framesetDepth > 0)
			tree->framesetDepth--;

		tree->EndTagIgnored = FALSE;
		return;
	}

	if (tree->mode != AfterHead)
		process_head_end_tag (tree, tag);

	// Note: In the "after head" insertion mode, </body>, </html> and </br> insert the <body> element.
	if (!tree->bodyCreated && tree->mode == AfterHead &&
	    (tag->id == GMIME_HTML_TAG_BODY || tag->id == GMIME_HTML_TAG_HTML || tag->id == GMIME_HTML_TAG_BR) &&
	    !(stack_index (tree, GMIME_HTML_NAMESPACE_HTML, GMIME_HTML_TAG_UNKNOWN, "template") >= 0))
		insert_implied_body (tree);

	// Note: In the body, </br> is treated as a <br> start tag.
	if (tag->id == GMIME_HTML_TAG_BR)
		tree->framesetOk = FALSE;

	process_end_tag_internal (tree, tag);

	// Note: </html>, </head>, </body> and </br> never close an element, so they are not considered to have been ignored.
	tree->EndTagIgnored = !tree->poppedThrough && tag->id != GMIME_HTML_TAG_HTML &&
			      tag->id != GMIME_HTML_TAG_HEAD && tag->id != GMIME_HTML_TAG_BODY &&
			      tag->id != GMIME_HTML_TAG_BR &&
			      (stack_index (tree, GMIME_HTML_NAMESPACE_HTML, tag->id, tag->name) >= 0 ||
			       stack_index (tree, GMIME_HTML_NAMESPACE_SVG, GMIME_HTML_TAG_UNKNOWN, tag->name) >= 0);
}

static void
process_head_end_tag (HtmlTree *tree, GMimeHtmlToken *tag)
{
	if ((stack_index (tree, GMIME_HTML_NAMESPACE_HTML, GMIME_HTML_TAG_UNKNOWN, "template") >= 0))
		return;

	switch (tag->id) {
	case GMIME_HTML_TAG_NO_SCRIPT:
		// Note: The </noscript> end tag itself pops the <noscript> element.
		if (tree->mode == InHeadNoScript)
			tree->mode = InHead;

		break;
	case GMIME_HTML_TAG_BR:
		if (tree->mode == InHeadNoScript)
			pop_through (tree, stack_index (tree, GMIME_HTML_NAMESPACE_HTML, GMIME_HTML_TAG_NO_SCRIPT, ""));

		tree->mode = AfterHead;
		break;
	case GMIME_HTML_TAG_HEAD:
	case GMIME_HTML_TAG_BODY:
	case GMIME_HTML_TAG_HTML:
		// Note: These end tags are ignored in the "in head noscript" insertion mode.
		if (tree->mode != InHeadNoScript)
			tree->mode = AfterHead;

		break;
	default:
		break;
	}
}

static void
process_end_tag_internal (HtmlTree *tree, GMimeHtmlToken *tag)
{
	if (stack_count (tree) > 0 &&
	    stack_entry (tree, stack_count (tree) - 1)->Namespace != GMIME_HTML_NAMESPACE_HTML) {
		if (tag->id == GMIME_HTML_TAG_BR || tag->id == GMIME_HTML_TAG_P) {
			// </br> and </p> break out of foreign content.
			int n = stack_count (tree);

			pop_above (tree, stack_top (tree)->PrevHtmlOrIntegrationPoint);
			tree->BrokeOutOfForeignContent = stack_count (tree) < n;
		} else {
			// Walk down the stack looking for a foreign element with a matching name, stopping at the first
			// element in the HTML namespace. If a match is found, pop through it; otherwise, process the
			// token using the rules for HTML content.
			int index = stack_index (tree, GMIME_HTML_NAMESPACE_SVG, GMIME_HTML_TAG_UNKNOWN, tag->name);

			if (index > stack_top (tree)->PrevHtml) {
				pop_through (tree, index);
				return;
			}
		}
	}

	process_html_end_tag (tree, tag);
}

static void
process_form_end_tag (HtmlTree *tree)
{
	int index;

	if ((stack_index (tree, GMIME_HTML_NAMESPACE_HTML, GMIME_HTML_TAG_UNKNOWN, "template") >= 0)) {
		index = stack_index (tree, GMIME_HTML_NAMESPACE_HTML, GMIME_HTML_TAG_FORM, "");

		if (in_scope (tree, index, stack_top (tree)->PrevScope))
			pop_through (tree, index);

		return;
	}

	// Note: Unless there is a <template> element on the stack, the </form> end tag unsets the form element pointer and
	// removes the <form> element that it pointed to (if it is in scope) from the stack *without* popping the elements
	// above it. E.g. in "<form><svg><style></form>", the SVG elements remain open.
	index = tree->formPointerSet ? tree->formPointerIndex : -1;
	tree->formPointerSet = FALSE;
	tree->formPointerIndex = -1;

	if (!in_scope (tree, index, stack_top (tree)->PrevScope))
		return;

	generate_implied_end_tags (tree);

	if (index == stack_count (tree) - 1)
		pop_through (tree, index);
	else
		remove_entry (tree, index);
}

static void
process_formatting_end_tag (HtmlTree *tree, GMimeHtmlTagId id)
{
	int index = stack_index (tree, GMIME_HTML_NAMESPACE_HTML, id, "");

	if (!in_scope (tree, index, stack_top (tree)->PrevScope))
		return;

	Entry replacement[7];

	// Model the stack effects, not the DOM reparenting. As elsewhere in this controller, formatting
	// elements are looked up on the stack rather than reconstructed from an active formatting list.
	for (int outer = 0; outer < 8; outer++) {
		if (stack_top (tree)->PrevSpecial < index) {
			pop_through (tree, index);
			return;
		}

		int block = index + 1;

		while ((stack_entry (tree, block)->Flags & F_Special) == 0)
			block++;

		Entry formatting = (*stack_entry (tree, index));
		Entry furthestBlock = (*stack_entry (tree, block));
		int count = 0;

		// A folded entry can contain more than one physical node. Only the last formatting node
		// moves, and only the first special node is the furthest block.
		if (formatting.Count > 1) {
			replacement[count] = formatting;
			replacement[count++].Count--;
		}

		int first = count;
		int remaining = 3;

		for (int i = block - 1; i > index && remaining > 0; i--) {
			Entry node = (*stack_entry (tree, i));
			int retained = MIN (remaining, node.Count);

			remaining -= retained;

			if (node.Namespace == GMIME_HTML_NAMESPACE_HTML &&
			    g_mime_html_tag_id_is_formatting_element (node.Id)) {
				node.Count = retained;
				replacement[count++] = node;
			}
		}

		// The inner loop visits nodes backwards, but their stack order does not change.
		for (int a = first, b = count - 1; a < b; a++, b--) {
			Entry swap = replacement[a];

			replacement[a] = replacement[b];
			replacement[b] = swap;
		}

		replacement[count] = furthestBlock;
		replacement[count++].Count = 1;
		replacement[count] = formatting;
		replacement[count++].Count = 1;

		if (furthestBlock.Count > 1) {
			replacement[count] = furthestBlock;
			replacement[count++].Count--;
		}

		// Non-formatting inner nodes and formatting nodes beyond the first three are removed
		// together, so a long inner loop does not repeatedly rebuild the same suffix.
		stack_replace (tree, index, block - index + 1, replacement, count);
		tree->poppedThrough = TRUE;

		if (tree->formPointerIndex >= index)
			tree->formPointerIndex = stack_index (tree, GMIME_HTML_NAMESPACE_HTML, GMIME_HTML_TAG_FORM, "");

		if (tree->depth_exceeded)
			return;

		index = stack_index (tree, GMIME_HTML_NAMESPACE_HTML, id, "");
	}
}

static void
process_html_end_tag (HtmlTree *tree, GMimeHtmlToken *tag)
{
	int index;

	switch (tag->id) {
	case GMIME_HTML_TAG_HTML:
	case GMIME_HTML_TAG_HEAD:
	case GMIME_HTML_TAG_BODY:
	case GMIME_HTML_TAG_BR:
		return;
	case GMIME_HTML_TAG_P:
		index = stack_index (tree, GMIME_HTML_NAMESPACE_HTML, GMIME_HTML_TAG_P, "");

		if (in_scope (tree, index, stack_top (tree)->PrevButtonScope))
			pop_through (tree, index);

		return;
	case GMIME_HTML_TAG_LI:
		index = stack_index (tree, GMIME_HTML_NAMESPACE_HTML, GMIME_HTML_TAG_LI, "");

		if (in_scope (tree, index, stack_top (tree)->PrevListItemScope))
			pop_through (tree, index);

		return;
	case GMIME_HTML_TAG_H1:
	case GMIME_HTML_TAG_H2:
	case GMIME_HTML_TAG_H3:
	case GMIME_HTML_TAG_H4:
	case GMIME_HTML_TAG_H5:
	case GMIME_HTML_TAG_H6:
		// Any heading end tag closes the topmost heading element.
		index = stack_top (tree)->PrevHeading;

		if (in_scope (tree, index, stack_top (tree)->PrevScope))
			pop_through (tree, index);

		return;
	case GMIME_HTML_TAG_ADDRESS:
	case GMIME_HTML_TAG_ARTICLE:
	case GMIME_HTML_TAG_ASIDE:
	case GMIME_HTML_TAG_BLOCK_QUOTE:
	case GMIME_HTML_TAG_BUTTON:
	case GMIME_HTML_TAG_CENTER:
	case GMIME_HTML_TAG_DETAILS:
	case GMIME_HTML_TAG_DIALOG:
	case GMIME_HTML_TAG_DIR:
	case GMIME_HTML_TAG_DIV:
	case GMIME_HTML_TAG_DL:
	case GMIME_HTML_TAG_FIELD_SET:
	case GMIME_HTML_TAG_FIG_CAPTION:
	case GMIME_HTML_TAG_FIGURE:
	case GMIME_HTML_TAG_FOOTER:
	case GMIME_HTML_TAG_HEADER:
	case GMIME_HTML_TAG_LISTING:
	case GMIME_HTML_TAG_MAIN:
	case GMIME_HTML_TAG_MENU:
	case GMIME_HTML_TAG_NAV:
	case GMIME_HTML_TAG_OL:
	case GMIME_HTML_TAG_PRE:
	case GMIME_HTML_TAG_SECTION:
	case GMIME_HTML_TAG_SUMMARY:
	case GMIME_HTML_TAG_UL:
	case GMIME_HTML_TAG_APPLET:
	case GMIME_HTML_TAG_MARQUEE:
	case GMIME_HTML_TAG_OBJECT:
	case GMIME_HTML_TAG_DD:
	case GMIME_HTML_TAG_DT:
		index = stack_index (tree, GMIME_HTML_NAMESPACE_HTML, tag->id, "");

		if (in_scope (tree, index, stack_top (tree)->PrevScope))
			pop_through (tree, index);

		return;
	case GMIME_HTML_TAG_TABLE:
	case GMIME_HTML_TAG_T_BODY:
	case GMIME_HTML_TAG_TFOOT:
	case GMIME_HTML_TAG_T_HEAD:
	case GMIME_HTML_TAG_TR:
	case GMIME_HTML_TAG_TD:
	case GMIME_HTML_TAG_TH:
	case GMIME_HTML_TAG_CAPTION:
	case GMIME_HTML_TAG_COL_GROUP:
		index = stack_index (tree, GMIME_HTML_NAMESPACE_HTML, tag->id, "");

		if (in_scope (tree, index, stack_top (tree)->PrevTableScope))
			pop_through (tree, index);

		return;
	case GMIME_HTML_TAG_FORM:
		process_form_end_tag (tree);
		return;
	case GMIME_HTML_TAG_A:
	case GMIME_HTML_TAG_B:
	case GMIME_HTML_TAG_BIG:
	case GMIME_HTML_TAG_CODE:
	case GMIME_HTML_TAG_EM:
	case GMIME_HTML_TAG_FONT:
	case GMIME_HTML_TAG_I:
	case GMIME_HTML_TAG_NO_BR:
	case GMIME_HTML_TAG_S:
	case GMIME_HTML_TAG_SMALL:
	case GMIME_HTML_TAG_STRIKE:
	case GMIME_HTML_TAG_STRONG:
	case GMIME_HTML_TAG_TT:
	case GMIME_HTML_TAG_U:
		process_formatting_end_tag (tree, tag->id);
		return;
	case GMIME_HTML_TAG_UNKNOWN:
		if (!g_ascii_strcasecmp (tag->name, "hgroup") || !g_ascii_strcasecmp (tag->name, "search")) {
			index = stack_index (tree, GMIME_HTML_NAMESPACE_HTML, GMIME_HTML_TAG_UNKNOWN, tag->name);

			if (in_scope (tree, index, stack_top (tree)->PrevScope))
				pop_through (tree, index);

			return;
		}

		if (!g_ascii_strcasecmp (tag->name, "template")) {
			index = stack_index (tree, GMIME_HTML_NAMESPACE_HTML, GMIME_HTML_TAG_UNKNOWN, tag->name);

			if (index >= 0)
				pop_through (tree, index);

			return;
		}

		break;
	default:
		break;
	}

	// Any other end tag: walk down the stack looking for an HTML element with a matching name,
	// giving up if a special element is found first.
	index = stack_index (tree, GMIME_HTML_NAMESPACE_HTML, tag->id, tag->name);

	if (index >= 0 && index >= stack_top (tree)->PrevSpecial)
		pop_through (tree, index);
}
