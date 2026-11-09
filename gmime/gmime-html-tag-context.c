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

#include "gmime-html-tag-context.h"

/**
 * SECTION: gmime-html-tag-context
 * @title: HTML tag callbacks
 * @short_description: Borrowed tag metadata and conversion decisions
 *
 * A callback receives borrowed immutable tag metadata and mutable decisions.
 * It must explicitly write the desired tag; setting deletion flags does not
 * undo previously written output. End-tag callbacks are opt-in. Writing a tag
 * does not itself consult deletion or suppression flags.
 **/
struct _GMimeHtmlTagContext {
	GMimeHtmlToken *token;
	gboolean is_end_tag, delete_tag, delete_end_tag;
	gboolean suppress_inner_content, invoke_callback_for_end_tag;
};

/**
 * g_mime_html_tag_context_new: (skip)
 * @token: (transfer none): a tag token
 *
 * Creates a mutable context retaining a reference to @token. This allocation
 * API is C-only; language bindings receive borrowed contexts in callbacks.
 * Free the returned context with g_mime_html_tag_context_free().
 *
 * Returns: (transfer full): a newly allocated #GMimeHtmlTagContext.
 **/
GMimeHtmlTagContext *
g_mime_html_tag_context_new (GMimeHtmlToken *token)
{
	GMimeHtmlTagContext *context;

	g_return_val_if_fail (token && g_mime_html_token_get_kind (token) == GMIME_HTML_TOKEN_TAG, NULL);

	context = g_new0 (GMimeHtmlTagContext, 1);
	g_mime_html_tag_context_set_token (context, token);

	return context;
}

/**
 * g_mime_html_tag_context_set_token:
 * @context: an HTML tag context
 * @token: (transfer none): a tag token
 *
 * Replaces the context's token, retaining a reference to @token and releasing
 * the previous token. Updates the end-tag state from @token without resetting
 * the deletion, suppression or callback flags.
 **/
void
g_mime_html_tag_context_set_token (GMimeHtmlTagContext *context, GMimeHtmlToken *token)
{
	g_return_if_fail (context && token && g_mime_html_token_get_kind (token) == GMIME_HTML_TOKEN_TAG);

	g_mime_html_token_ref (token);

	if (context->token)
		g_mime_html_token_unref (context->token);

	context->token = token;
	context->is_end_tag = g_mime_html_token_is_end_tag (token);
}

/**
 * g_mime_html_tag_context_free: (skip)
 * @context: (nullable) (transfer full): a context allocated by
 *   g_mime_html_tag_context_new(), or %NULL
 *
 * Frees an owned context and releases its token reference. This API is C-only.
 * Do not free a borrowed context received in a #GMimeHtmlTagCallback.
 **/
void
g_mime_html_tag_context_free (GMimeHtmlTagContext *context)
{
	if (!context)
		return;

	g_mime_html_token_unref (context->token);
	g_free (context);
}

/**
 * g_mime_html_tag_context_get_tag_name:
 * @context: an HTML tag context
 *
 * Gets the current token's tag name.
 *
 * Returns: (transfer none): the tag name, valid until the token is replaced
 *   or the context is freed.
 **/
const char *
g_mime_html_tag_context_get_tag_name (GMimeHtmlTagContext *context)
{
	return g_mime_html_token_get_name (context->token);
}

/**
 * g_mime_html_tag_context_get_tag_id:
 * @context: an HTML tag context
 *
 * Gets the known tag identifier for the current token.
 *
 * Returns: the tag identifier, or %GMIME_HTML_TAG_UNKNOWN for an unknown name.
 **/
GMimeHtmlTagId
g_mime_html_tag_context_get_tag_id (GMimeHtmlTagContext *context)
{
	return g_mime_html_token_get_tag_id (context->token);
}

/**
 * g_mime_html_tag_context_get_n_attributes:
 * @context: an HTML tag context
 *
 * Gets the number of attributes on the current token.
 *
 * Returns: the attribute count.
 **/
guint
g_mime_html_tag_context_get_n_attributes (GMimeHtmlTagContext *context)
{
	return g_mime_html_token_get_n_attributes (context->token);
}

/**
 * g_mime_html_tag_context_get_attribute:
 * @context: an HTML tag context
 * @index: an attribute index, less than the attribute count
 *
 * Gets an attribute by its zero-based index. The attribute is owned by the
 * token; retain it with g_mime_html_attribute_ref() if it must outlive the
 * current token or callback.
 *
 * Returns: (transfer none): the requested attribute.
 **/
const GMimeHtmlAttribute *
g_mime_html_tag_context_get_attribute (GMimeHtmlTagContext *context, guint index)
{
	return g_mime_html_token_get_attribute (context->token, index);
}

/**
 * g_mime_html_tag_context_get_is_empty_element_tag:
 * @context: an HTML tag context
 *
 * Gets whether the token is self-closing or its tag is a known HTML void element.
 *
 * Returns: %TRUE if the tag represents an empty element.
 **/
gboolean
g_mime_html_tag_context_get_is_empty_element_tag (GMimeHtmlTagContext *context)
{
	return g_mime_html_token_is_empty_element (context->token) ||
	       g_mime_html_tag_id_is_empty_element (g_mime_html_token_get_tag_id (context->token));
}

/**
 * g_mime_html_tag_context_get_is_end_tag:
 * @context: an HTML tag context
 *
 * Gets whether the context currently represents an end tag.
 *
 * Returns: %TRUE for an end tag, or %FALSE for a start tag.
 **/
gboolean
g_mime_html_tag_context_get_is_end_tag (GMimeHtmlTagContext *context)
{
	g_return_val_if_fail (context, FALSE);

	return context->is_end_tag;
}

/**
 * g_mime_html_tag_context_set_is_end_tag:
 * @context: an HTML tag context
 * @value: whether the context represents an end tag
 *
 * Changes how g_mime_html_tag_context_write_tag() writes the tag without
 * modifying the underlying token.
 **/
void
g_mime_html_tag_context_set_is_end_tag (GMimeHtmlTagContext *context, gboolean value)
{
	g_return_if_fail (context);

	context->is_end_tag = !!value;
}

/**
 * g_mime_html_tag_context_get_delete_tag:
 * @context: an HTML tag context
 *
 * Gets whether the callback has marked the current tag as deleted.
 *
 * Returns: the deletion flag, initially %FALSE.
 **/
gboolean
g_mime_html_tag_context_get_delete_tag (GMimeHtmlTagContext *context)
{
	g_return_val_if_fail (context, FALSE);

	return context->delete_tag;
}

/**
 * g_mime_html_tag_context_set_delete_tag:
 * @context: an HTML tag context
 * @value: whether the current tag is marked as deleted
 *
 * Records the callback's decision to omit the tag. The callback must also
 * refrain from writing it: this flag neither undoes output nor prevents an
 * explicit call to g_mime_html_tag_context_write_tag(). Inner-content and
 * matching-end-tag suppression are controlled separately.
 **/
void
g_mime_html_tag_context_set_delete_tag (GMimeHtmlTagContext *context, gboolean value)
{
	g_return_if_fail (context);

	context->delete_tag = !!value;
}

/**
 * g_mime_html_tag_context_get_delete_end_tag:
 * @context: an HTML tag context
 *
 * Gets whether automatic output of the matching end tag is suppressed.
 *
 * Returns: the end-tag deletion flag, initially %FALSE.
 **/
gboolean
g_mime_html_tag_context_get_delete_end_tag (GMimeHtmlTagContext *context)
{
	g_return_val_if_fail (context, FALSE);

	return context->delete_end_tag;
}

/**
 * g_mime_html_tag_context_set_delete_end_tag:
 * @context: an HTML tag context
 * @value: whether to suppress automatic output of the matching end tag
 *
 * Records whether the converter should omit the matching end tag.
 * This flag does not prevent explicit writes by a tag callback.
 **/
void
g_mime_html_tag_context_set_delete_end_tag (GMimeHtmlTagContext *context, gboolean value)
{
	g_return_if_fail (context);

	context->delete_end_tag = !!value;
}

/**
 * g_mime_html_tag_context_get_suppress_inner_content:
 * @context: an HTML tag context
 *
 * Gets whether output of the element's inner content is suppressed.
 *
 * Returns: the inner-content suppression flag, initially %FALSE.
 **/
gboolean
g_mime_html_tag_context_get_suppress_inner_content (GMimeHtmlTagContext *context)
{
	g_return_val_if_fail (context, FALSE);

	return context->suppress_inner_content;
}

/**
 * g_mime_html_tag_context_set_suppress_inner_content:
 * @context: an HTML tag context
 * @value: whether to suppress the element's inner content
 *
 * Requests suppression of text and nested elements inside the current
 * element. Set this in the start-tag callback. The element's own start and
 * end tags are controlled separately.
 **/
void
g_mime_html_tag_context_set_suppress_inner_content (GMimeHtmlTagContext *context, gboolean value)
{
	g_return_if_fail (context);

	context->suppress_inner_content = !!value;
}

/**
 * g_mime_html_tag_context_get_invoke_callback_for_end_tag:
 * @context: an HTML tag context
 *
 * Gets whether a callback is requested for the matching end tag.
 *
 * Returns: the end-tag callback flag, initially %FALSE.
 **/
gboolean
g_mime_html_tag_context_get_invoke_callback_for_end_tag (GMimeHtmlTagContext *context)
{
	g_return_val_if_fail (context, FALSE);

	return context->invoke_callback_for_end_tag;
}

/**
 * g_mime_html_tag_context_set_invoke_callback_for_end_tag:
 * @context: an HTML tag context
 * @value: whether to request an end-tag callback
 *
 * Requests a callback for the matching end tag instead of the default
 * automatic write. Set this in the start-tag callback. The end-tag
 * callback is responsible for writing any desired closing tag.
 **/
void
g_mime_html_tag_context_set_invoke_callback_for_end_tag (GMimeHtmlTagContext *context, gboolean value)
{
	g_return_if_fail (context);

	context->invoke_callback_for_end_tag = !!value;
}

/**
 * g_mime_html_tag_context_write_tag:
 * @context: an HTML tag context
 * @writer: the HTML writer
 * @write_attributes: whether to write the current token's attributes
 * @error: return location for a #GError, or %NULL
 *
 * Writes the current tag using the context's end-tag state and token
 * metadata. Attributes are written only for start or empty-element tags.
 * This function does not consult the deletion or suppression flags, and
 * does not flush or close the writer.
 *
 * Returns: %TRUE on success, or %FALSE with @error set on failure.
 **/
gboolean
g_mime_html_tag_context_write_tag (GMimeHtmlTagContext *context, GMimeHtmlWriter *writer, gboolean write_attributes,
				   GError **error)
{
	const char *name;

	g_return_val_if_fail (context, FALSE);

	name = g_mime_html_tag_context_get_tag_name (context);

	if (context->is_end_tag)
		return g_mime_html_writer_write_end_tag (writer, name, error);

	if (g_mime_html_tag_context_get_is_empty_element_tag (context)) {
		if (!g_mime_html_writer_write_empty_element_tag (writer, name, error))
			return FALSE;
	} else if (!g_mime_html_writer_write_start_tag (writer, name, error)) {
		return FALSE;
	}

	if (write_attributes) {
		for (guint i = 0; i < g_mime_html_tag_context_get_n_attributes (context); i++) {
			const GMimeHtmlAttribute *attr = g_mime_html_tag_context_get_attribute (context, i);

			if (!g_mime_html_writer_write_html_attribute (writer, attr, error))
				return FALSE;
		}
	}

	return TRUE;
}
