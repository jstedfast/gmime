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

#include "gmime-html-token-private.h"
#include "gmime-html-utils.h"
#include "gmime-text-private.h"
#include "gmime-html-token-tables-private.h"

/**
 * SECTION: gmime-html-token
 * @title: HTML tokens and attributes
 * @short_description: Immutable, reference-counted HTML lexical tokens
 *
 * Tokens and attributes are boxed values whose references may outlive their
 * tokenizer. Strings and attributes returned by getters are borrowed. Data
 * tokens may contain NUL bytes; use g_mime_html_token_get_data_length() rather
 * than strlen(), and g_mime_html_token_write_to() for lossless serialization.
 */

G_DEFINE_BOXED_TYPE (GMimeHtmlToken, g_mime_html_token, g_mime_html_token_ref, g_mime_html_token_unref)
G_DEFINE_BOXED_TYPE (GMimeHtmlAttribute, g_mime_html_attribute, g_mime_html_attribute_ref, g_mime_html_attribute_unref)

static GHashTable *tag_ids, *attribute_ids;
static gsize names_initialized;

static guint
ascii_name_hash (gconstpointer value)
{
	const guchar *p = value;
	guint hash = 5381;

	while (*p)
		hash = (hash << 5) + hash + (guchar) g_ascii_tolower (*p++);

	return hash;
}

static gboolean
ascii_name_equal (gconstpointer a, gconstpointer b)
{
	return !g_ascii_strcasecmp (a, b);
}

static void
initialize_names (void)
{
	if (g_once_init_enter (&names_initialized)) {
		tag_ids = g_hash_table_new (ascii_name_hash, ascii_name_equal);
		attribute_ids = g_hash_table_new (ascii_name_hash, ascii_name_equal);

		for (guint i = 1; i < G_N_ELEMENTS (tag_names); i++)
			g_hash_table_insert (tag_ids, (gpointer) tag_names[i], GUINT_TO_POINTER (i));

		for (guint i = 1; i < G_N_ELEMENTS (attribute_names); i++)
			g_hash_table_insert (attribute_ids, (gpointer) attribute_names[i], GUINT_TO_POINTER (i));

		g_once_init_leave (&names_initialized, 1);
	}
}

/**
 * g_mime_html_token_ref:
 * @token: an HTML token
 *
 * Returns: (transfer full): @token with an additional reference
 */
GMimeHtmlToken *
g_mime_html_token_ref (GMimeHtmlToken *token)
{
	g_atomic_ref_count_inc (&token->refs);
	return token;
}

/**
 * g_mime_html_token_unref:
 * @token: (transfer full) (nullable): an HTML token to release
 *
 * Releases one reference to @token.
 */
void
g_mime_html_token_unref (GMimeHtmlToken *token)
{
	if (!token || !g_atomic_ref_count_dec (&token->refs))
		return;

	g_free (token->name);
	g_free (token->data);
	g_free (token->raw_tag_name);
	g_free (token->public_identifier);
	g_free (token->system_identifier);
	g_free (token->public_keyword);
	g_free (token->system_keyword);

	if (token->attributes)
		g_ptr_array_unref (token->attributes);

	g_free (token);
}

/**
 * g_mime_html_attribute_ref:
 * @attribute: an HTML attribute
 *
 * Returns: (transfer full): @attribute with an additional reference
 */
GMimeHtmlAttribute *
g_mime_html_attribute_ref (GMimeHtmlAttribute *attribute)
{
	g_atomic_ref_count_inc (&attribute->refs);

	return attribute;
}

/**
 * g_mime_html_attribute_unref:
 * @attribute: (transfer full) (nullable): an HTML attribute to release
 *
 * Releases one reference to @attribute.
 */
void
g_mime_html_attribute_unref (GMimeHtmlAttribute *attribute)
{
	if (!attribute || !g_atomic_ref_count_dec (&attribute->refs))
		return;

	g_free (attribute->name);
	g_free (attribute->value);
	g_free (attribute);
}

/**
 * g_mime_html_attribute_new:
 * @name: the attribute name
 * @value: (nullable): the attribute value, or %NULL for a valueless attribute
 *
 * Copies @name and @value.
 *
 * Returns: (transfer full): a new immutable attribute
 */
GMimeHtmlAttribute *
g_mime_html_attribute_new (const char *name, const char *value)
{
	GMimeHtmlAttribute *attribute;

	g_return_val_if_fail (name != NULL, NULL);

	initialize_names ();

	attribute = g_new0 (GMimeHtmlAttribute, 1);
	g_atomic_ref_count_init (&attribute->refs);
	attribute->name = g_strdup (name);
	attribute->value = g_strdup (value);
	attribute->id = GPOINTER_TO_UINT (g_hash_table_lookup (attribute_ids, name));

	return attribute;
}

/**
 * g_mime_html_attribute_get_name:
 * @attribute: an HTML attribute
 *
 * Returns: (transfer none): the name owned by @attribute
 */
const char *
g_mime_html_attribute_get_name (const GMimeHtmlAttribute *attribute)
{
	return attribute->name;
}

/**
 * g_mime_html_attribute_get_value:
 * @attribute: an HTML attribute
 *
 * Returns: (transfer none) (nullable): the value owned by @attribute,
 * or %NULL for a valueless attribute
 */
const char *
g_mime_html_attribute_get_value (const GMimeHtmlAttribute *attribute)
{
	return attribute->value;
}

/**
 * g_mime_html_attribute_get_id:
 * @attribute: an HTML attribute
 *
 * Gets the identifier determined by the attribute's case-insensitive name.
 *
 * Returns: the attribute identifier, or %GMIME_HTML_ATTRIBUTE_UNKNOWN
 **/
GMimeHtmlAttributeId
g_mime_html_attribute_get_id (const GMimeHtmlAttribute *attribute)
{
	return attribute->id;
}

static GMimeHtmlToken *
new_token (GMimeHtmlTokenKind kind, const char *data)
{
	GMimeHtmlToken *token = g_new0 (GMimeHtmlToken, 1);

	g_atomic_ref_count_init (&token->refs);
	token->kind = kind;
	token->data = g_strdup (data);
	token->length = data ? strlen (data) : 0;
	token->public_quote = token->system_quote = '"';

	return token;
}

/**
 * g_mime_html_token_new_tag:
 * @name: the tag name
 * @is_end_tag: whether this is a closing tag
 * @is_empty_element: whether this tag is self-closing
 * @attributes: (array length=n_attributes) (element-type GMimeHtmlAttribute) (transfer none) (nullable): the tag attributes
 * @n_attributes: the number of attributes
 *
 * Copies @name and retains a reference to each attribute. The caller retains
 * ownership of the array and its elements.
 *
 * Returns: (transfer full): a new immutable tag token
 */
GMimeHtmlToken *
g_mime_html_token_new_tag (const char *name, gboolean is_end_tag, gboolean is_empty_element,
			   GMimeHtmlAttribute *const *attributes, gsize n_attributes)
{
	GMimeHtmlToken *token;
	gsize i;

	g_return_val_if_fail (name != NULL, NULL);
	g_return_val_if_fail (attributes != NULL || n_attributes == 0, NULL);

	token = new_token (GMIME_HTML_TOKEN_TAG, NULL);
	token->name = g_strdup (name);
	token->id = g_mime_html_tag_id_from_name (name);
	token->is_end_tag = is_end_tag;
	token->is_empty_element = is_empty_element;
	token->attributes = g_ptr_array_new_with_free_func ((GDestroyNotify) g_mime_html_attribute_unref);

	for (i = 0; i < n_attributes; i++)
		g_ptr_array_add (token->attributes, g_mime_html_attribute_ref (attributes[i]));

	return token;
}

/**
 * g_mime_html_token_new_data:
 * @data: UTF-8 character data
 *
 * Returns: (transfer full): a new token containing a copy of @data
 */
GMimeHtmlToken *
g_mime_html_token_new_data (const char *data)
{
	g_return_val_if_fail (data != NULL, NULL);

	return new_token (GMIME_HTML_TOKEN_DATA, data);
}

/**
 * g_mime_html_token_new_comment:
 * @data: UTF-8 comment text
 * @bogus: whether to serialize as a bogus comment
 *
 * Returns: (transfer full): a new comment token containing a copy of @data
 */
GMimeHtmlToken *
g_mime_html_token_new_comment (const char *data, gboolean bogus)
{
	GMimeHtmlToken *token;

	g_return_val_if_fail (data != NULL, NULL);

	token = new_token (GMIME_HTML_TOKEN_COMMENT, data);
	token->bogus = bogus;

	return token;
}

/**
 * g_mime_html_token_new_cdata:
 * @data: UTF-8 CDATA text
 *
 * Returns: (transfer full): a new CDATA token containing a copy of @data
 */
GMimeHtmlToken *
g_mime_html_token_new_cdata (const char *data)
{
	g_return_val_if_fail (data != NULL, NULL);

	return new_token (GMIME_HTML_TOKEN_CDATA, data);
}

/**
 * g_mime_html_token_new_script_data:
 * @data: UTF-8 script text
 *
 * Returns: (transfer full): a new script-data token containing a copy of @data
 */
GMimeHtmlToken *
g_mime_html_token_new_script_data (const char *data)
{
	g_return_val_if_fail (data != NULL, NULL);

	return new_token (GMIME_HTML_TOKEN_SCRIPT_DATA, data);
}

static gboolean
valid_identifier (const char *id)
{
	return !id || (!strchr (id, '>') && !(strchr (id, '\'') && strchr (id, '"')));
}

/**
 * g_mime_html_token_new_doctype:
 * @name: (nullable): the document type name
 * @public_id: (nullable): the public identifier
 * @system_id: (nullable): the system identifier
 *
 * Copies the supplied name and identifiers.
 *
 * Returns: (transfer full): a new document-type token
 */
GMimeHtmlToken *
g_mime_html_token_new_doctype (const char *name, const char *public_id, const char *system_id)
{
	GMimeHtmlToken *token;

	g_return_val_if_fail (!name || (*name && !strpbrk (name, "\t\n\f\r >")), NULL);
	g_return_val_if_fail (valid_identifier (public_id) && valid_identifier (system_id), NULL);

	token = new_token (GMIME_HTML_TOKEN_DOCTYPE, NULL);
	token->name = g_strdup (name);
	token->raw_tag_name = g_strdup ("DOCTYPE");
	token->public_identifier = g_strdup (public_id);
	token->system_identifier = g_strdup (system_id);
	token->public_keyword = public_id ? g_strdup ("PUBLIC") : NULL;
	token->system_keyword = system_id && !public_id ? g_strdup ("SYSTEM") : NULL;

	return token;
}

/**
 * g_mime_html_token_get_kind:
 * @token: an HTML token
 *
 * Gets the kind of lexical token.
 *
 * Returns: the token kind
 **/
GMimeHtmlTokenKind
g_mime_html_token_get_kind (const GMimeHtmlToken *token)
{
	return token->kind;
}

/**
 * g_mime_html_token_get_data:
 * @token: an HTML token
 *
 * Character data can contain embedded NUL bytes. C callers should use
 * g_mime_html_token_get_data_length() rather than strlen(); use
 * g_mime_html_token_write_to() for lossless serialization from bindings.
 *
 * Returns: (transfer none) (nullable): the data owned by @token,
 * or %NULL for a tag or document-type token
 */
const char *
g_mime_html_token_get_data (const GMimeHtmlToken *token)
{
	return token->data;
}

/**
 * g_mime_html_token_get_data_length:
 * @token: an HTML token
 *
 * Gets the byte length of the token data, including any embedded NUL bytes
 * but excluding the terminating NUL.
 *
 * Returns: the data length, or zero when there is no data
 **/
gsize
g_mime_html_token_get_data_length (const GMimeHtmlToken *token)
{
	return token->length;
}

/**
 * g_mime_html_token_get_name:
 * @token: an HTML token
 *
 * Returns: (transfer none) (nullable): the name owned by @token, if present
 */
const char *
g_mime_html_token_get_name (const GMimeHtmlToken *token)
{
	return token->name;
}

/**
 * g_mime_html_token_get_tag_id:
 * @token: an HTML token
 *
 * Gets the tag identifier determined by its case-insensitive name.
 *
 * Returns: the tag identifier, or %GMIME_HTML_TAG_UNKNOWN for an unrecognized
 * tag or a token that is not a tag
 **/
GMimeHtmlTagId
g_mime_html_token_get_tag_id (const GMimeHtmlToken *token)
{
	return token->id;
}

/**
 * g_mime_html_token_is_end_tag:
 * @token: an HTML token
 *
 * Tests whether @token represents an end tag.
 *
 * Returns: %TRUE for an end tag or %FALSE otherwise
 **/
gboolean
g_mime_html_token_is_end_tag (const GMimeHtmlToken *token)
{
	return token->is_end_tag;
}

/**
 * g_mime_html_token_is_empty_element:
 * @token: an HTML token
 *
 * Gets the explicit self-closing flag. This does not test whether the tag name
 * denotes an HTML void element; use g_mime_html_tag_id_is_empty_element() for that.
 *
 * Returns: %TRUE if the tag is explicitly self-closing or %FALSE otherwise
 **/
gboolean
g_mime_html_token_is_empty_element (const GMimeHtmlToken *token)
{
	return token->is_empty_element;
}

/**
 * g_mime_html_token_get_encode_entities:
 * @token: an HTML token
 *
 * Tests whether character references in the data were decoded and must be
 * escaped when serialized.
 *
 * Returns: %TRUE if the data requires escaping or %FALSE otherwise
 **/
gboolean
g_mime_html_token_get_encode_entities (const GMimeHtmlToken *token)
{
	return token->encode_entities;
}

/**
 * g_mime_html_token_get_is_data_state:
 * @token: an HTML token
 *
 * Tests whether the data was produced in the tokenizer's ordinary data state
 * rather than a raw-text or other specialized state.
 *
 * Returns: %TRUE for ordinary data-state text or %FALSE otherwise
 **/
gboolean
g_mime_html_token_get_is_data_state (const GMimeHtmlToken *token)
{
	return token->is_data_state;
}

/**
 * g_mime_html_token_get_is_bogus_comment:
 * @token: an HTML token
 *
 * Tests whether the comment uses HTML's bogus-comment recovery syntax.
 *
 * Returns: %TRUE for a bogus comment or %FALSE otherwise
 **/
gboolean
g_mime_html_token_get_is_bogus_comment (const GMimeHtmlToken *token)
{
	return token->bogus;
}

/**
 * g_mime_html_token_get_force_quirks_mode:
 * @token: an HTML token
 *
 * Gets the force-quirks flag produced while parsing a DOCTYPE.
 *
 * Returns: %TRUE if the DOCTYPE forces quirks mode or %FALSE otherwise
 **/
gboolean
g_mime_html_token_get_force_quirks_mode (const GMimeHtmlToken *token)
{
	return token->force_quirks;
}

/**
 * g_mime_html_token_get_public_identifier:
 * @token: an HTML token
 *
 * Returns: (transfer none) (nullable): the public identifier owned by @token,
 * if present
 */
const char *
g_mime_html_token_get_public_identifier (const GMimeHtmlToken *token)
{
	return token->public_identifier;
}

/**
 * g_mime_html_token_get_system_identifier:
 * @token: an HTML token
 *
 * Returns: (transfer none) (nullable): the system identifier owned by @token,
 * if present
 */
const char *
g_mime_html_token_get_system_identifier (const GMimeHtmlToken *token)
{
	return token->system_identifier;
}

/**
 * g_mime_html_token_get_n_attributes:
 * @token: an HTML token
 *
 * Gets the number of attributes in the tag.
 *
 * Returns: the attribute count, or zero for a token without attributes
 **/
gsize
g_mime_html_token_get_n_attributes (const GMimeHtmlToken *token)
{
	return token->attributes ? token->attributes->len : 0;
}

/**
 * g_mime_html_token_get_attribute:
 * @token: an HTML token
 * @index: the zero-based attribute index
 *
 * Returns: (transfer none) (nullable): the attribute owned by @token,
 * or %NULL if @index is out of range
 */
const GMimeHtmlAttribute *
g_mime_html_token_get_attribute (const GMimeHtmlToken *token, gsize index)
{
	return token->attributes && index < token->attributes->len ? g_ptr_array_index (token->attributes, index) :
								     NULL;
}

/**
 * g_mime_html_token_lookup_attribute:
 * @token: an HTML token
 * @name: the attribute name
 *
 * Returns: (transfer none) (nullable): the first matching attribute owned by
 * @token, or %NULL if no attribute matches
 */
const GMimeHtmlAttribute *
g_mime_html_token_lookup_attribute (const GMimeHtmlToken *token, const char *name)
{
	gsize i;

	for (i = 0; i < g_mime_html_token_get_n_attributes (token); i++) {
		const GMimeHtmlAttribute *attribute = g_ptr_array_index (token->attributes, i);

		if (!g_ascii_strcasecmp (attribute->name, name))
			return attribute;
	}

	return NULL;
}

static void
write_identifier (GString *buffer, const char *id, char quote)
{
	/* Prefer the original delimiter, falling back to one absent from the identifier. */
	if (quote != '\'' || strchr (id, '\''))
		quote = strchr (id, '"') ? '\'' : '"';

	g_string_append_c (buffer, ' ');
	g_string_append_c (buffer, quote);
	g_string_append (buffer, id);
	g_string_append_c (buffer, quote);
}

static GString *
serialize (const GMimeHtmlToken *token)
{
	GString *buffer = g_string_new (NULL);
	char *encoded;
	gsize i;

	switch (token->kind) {
	case GMIME_HTML_TOKEN_TAG:
		g_string_append (buffer, token->is_end_tag ? "</" : "<");
		g_string_append (buffer, token->name);

		for (i = 0; i < token->attributes->len; i++) {
			const GMimeHtmlAttribute *attribute = g_ptr_array_index (token->attributes, i);

			/* Without this value, '=name' would become the preceding attribute's value. */
			if (i && !((GMimeHtmlAttribute *) g_ptr_array_index (token->attributes, i - 1))->value &&
			    attribute->name[0] == '=')
				g_string_append (buffer, "=\"\"");

			g_string_append_c (buffer, ' ');
			g_string_append (buffer, attribute->name);

			if (attribute->value) {
				encoded = g_mime_html_attribute_encode (attribute->value);
				g_string_append_c (buffer, '=');
				g_string_append (buffer, encoded);
				g_free (encoded);
			}
		}

		g_string_append (buffer, token->is_empty_element ? "/>" : ">");
		break;
	case GMIME_HTML_TOKEN_COMMENT:
		/* Bogus '</' comments must retain the slash so they cannot become real markup. */
		g_string_append (buffer, !token->bogus         ? "<!--" :
					 token->bang           ? "<!" :
					 token->data[0] == '?' ? "<" :
								 "</");
		g_string_append_len (buffer, token->data, token->length);
		g_string_append (buffer, token->bogus ? ">" : "-->");
		break;
	case GMIME_HTML_TOKEN_DOCTYPE:
		g_string_append (buffer, "<!");
		g_string_append (buffer, token->raw_tag_name);

		/* Without a name, a keyword would be parsed as the name. */
		if (token->name) {
			g_string_append_c (buffer, ' ');
			g_string_append (buffer, token->name);

			if (token->public_identifier) {
				g_string_append_c (buffer, ' ');
				g_string_append (buffer, token->public_keyword ? token->public_keyword : "PUBLIC");
				write_identifier (buffer, token->public_identifier, token->public_quote);

				if (token->system_identifier)
					write_identifier (buffer, token->system_identifier, token->system_quote);
			} else if (token->system_identifier) {
				g_string_append_c (buffer, ' ');
				g_string_append (buffer, token->system_keyword ? token->system_keyword : "SYSTEM");
				write_identifier (buffer, token->system_identifier, token->system_quote);
			}
		}

		g_string_append_c (buffer, '>');
		break;
	case GMIME_HTML_TOKEN_CDATA:
		g_string_append (buffer, "<![CDATA[");
		g_string_append_len (buffer, token->data, token->length);
		g_string_append (buffer, "]]>");
		break;
	default:
		if (token->encode_entities) {
			encoded = _g_mime_html_encode_len (token->data, token->length, 0);
			g_string_append (buffer, encoded);
			g_free (encoded);
		} else if (token->kind == GMIME_HTML_TOKEN_DATA && token->is_data_state && token->length &&
			   token->data[token->length - 1] == '<') {
			/* The tokenizer only ends a data-state token with a literal '<' when the following
			 * input begins markup treated as text. If that markup is dropped, writing '<'
			 * verbatim could combine with the next token to form a new tag, so encode it. */
			g_string_append_len (buffer, token->data, token->length - 1);
			g_string_append (buffer, "&lt;");
		} else {
			g_string_append_len (buffer, token->data, token->length);
		}
		break;
	}

	return buffer;
}

/**
 * g_mime_html_token_to_string:
 * @token: an HTML token
 *
 * Serializes @token. Use g_mime_html_token_write_to() to preserve embedded
 * NUL bytes when consuming the result from language bindings.
 *
 * Returns: (transfer full): newly allocated serialized HTML
 */
char *
g_mime_html_token_to_string (const GMimeHtmlToken *token)
{
	return g_string_free (serialize (token), FALSE);
}

/**
 * g_mime_html_token_write_to:
 * @token: an HTML token
 * @stream: (transfer none): the output stream
 * @error: (out) (optional): return location for an error
 *
 * Writes @token without closing @stream.
 *
 * Returns: %TRUE on success, or %FALSE on error
 */
gboolean
g_mime_html_token_write_to (const GMimeHtmlToken *token, GMimeStream *stream, GError **error)
{
	GString *buffer = serialize (token);
	gboolean result = _g_mime_text_write (stream, buffer->str, buffer->len, error);

	g_string_free (buffer, TRUE);

	return result;
}

/**
 * g_mime_html_tag_id_to_name:
 * @id: a tag identifier
 *
 * Returns: (transfer none): the static tag name
 */
const char *
g_mime_html_tag_id_to_name (GMimeHtmlTagId id)
{
	return (guint) id < G_N_ELEMENTS (tag_names) ? tag_names[id] : "Unknown";
}

/**
 * g_mime_html_tag_id_from_name:
 * @name: (nullable): a tag name
 *
 * Returns: the tag identifier, or %GMIME_HTML_TAG_UNKNOWN for an unknown name
 */
GMimeHtmlTagId
g_mime_html_tag_id_from_name (const char *name)
{
	if (!name || !*name)
		return GMIME_HTML_TAG_UNKNOWN;

	if (*name == '!')
		return GMIME_HTML_TAG_COMMENT;

	initialize_names ();

	return GPOINTER_TO_UINT (g_hash_table_lookup (tag_ids, name));
}

/**
 * g_mime_html_tag_id_is_empty_element:
 * @id: a tag identifier
 *
 * Tests whether @id denotes a known HTML void element.
 *
 * Returns: %TRUE for a void element or %FALSE otherwise
 **/
gboolean
g_mime_html_tag_id_is_empty_element (GMimeHtmlTagId id)
{
	switch (id) {
	case GMIME_HTML_TAG_AREA:
	case GMIME_HTML_TAG_BASE:
	case GMIME_HTML_TAG_BR:
	case GMIME_HTML_TAG_COL:
	case GMIME_HTML_TAG_COMMAND:
	case GMIME_HTML_TAG_EMBED:
	case GMIME_HTML_TAG_HR:
	case GMIME_HTML_TAG_IMAGE:
	case GMIME_HTML_TAG_INPUT:
	case GMIME_HTML_TAG_KEYGEN:
	case GMIME_HTML_TAG_LINK:
	case GMIME_HTML_TAG_META:
	case GMIME_HTML_TAG_PARAM:
	case GMIME_HTML_TAG_SOURCE:
	case GMIME_HTML_TAG_TRACK:
	case GMIME_HTML_TAG_WBR:
		return TRUE;
	default:
		return FALSE;
	}
}

/**
 * g_mime_html_tag_id_is_formatting_element:
 * @id: a tag identifier
 *
 * Tests whether @id denotes an HTML formatting element, including legacy
 * formatting tags.
 *
 * Returns: %TRUE for a formatting element or %FALSE otherwise
 **/
gboolean
g_mime_html_tag_id_is_formatting_element (GMimeHtmlTagId id)
{
	switch (id) {
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
		return TRUE;
	default:
		return FALSE;
	}
}

/**
 * g_mime_html_namespace_to_url:
 * @ns: an HTML namespace
 *
 * Returns: (transfer none) (nullable): the static namespace URL,
 * or %NULL for an invalid namespace
 */
const char *
g_mime_html_namespace_to_url (GMimeHtmlNamespace ns)
{
	return (guint) ns < G_N_ELEMENTS (namespace_urls) ? namespace_urls[ns] : NULL;
}

/**
 * g_mime_html_namespace_from_url:
 * @url: (nullable): a namespace URL
 *
 * Returns: the namespace, or %GMIME_HTML_NAMESPACE_HTML for an unknown URL
 */
GMimeHtmlNamespace
g_mime_html_namespace_from_url (const char *url)
{
	guint i;

	for (i = 0; url && i < G_N_ELEMENTS (namespace_urls); i++) {
		if (!g_ascii_strcasecmp (url, namespace_urls[i]))
			return i;
	}

	return GMIME_HTML_NAMESPACE_HTML;
}
