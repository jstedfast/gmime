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

#include "gmime-html-tokenizer.h"
#include "gmime-html-token-private.h"
#include "gmime-html-tree-internal.h"
#include "gmime-html-entity-private.h"
#include "gmime-reader-simd-private.h"
#include "gmime-text-private.h"
#include "gmime-error.h"

/**
 * SECTION: gmime-html-tokenizer
 * @title: HTML tokenizer
 * @short_description: Streaming HTML lexical and structural state machine
 *
 * The tokenizer retains, but never closes, its input stream. The ordinary
 * constructor accepts UTF-8; the charset constructor can also detect Unicode
 * byte order marks. Invalid UTF-8 and native input errors are propagated.
 * Positions and compliance offsets count Unicode scalars, not UTF-8 bytes.
 *
 * read_next_token() returns %TRUE with a new token, %TRUE with a %NULL token at
 * end of input, or %FALSE on error. Tokens remain valid across subsequent reads.
 * The depth limit counts folded, distinct open-element stack entries; after
 * exceeding it, the remaining input is treated as literal text.
 */

struct _GMimeHtmlTokenizer {
	GObject parent;
	GMimeTextReader reader;
	GError *error;
	HtmlTree *tree;
	HtmlEntity entity;
	GString *data, *name;
	GHashTable *attribute_names;
	GMimeHtmlToken *tag, *doctype;
	GMimeHtmlAttribute *attribute;
	const char *activeTagName;
	GMimeHtmlTokenizerState state;
	gboolean decode, ignore_truncated, isEndTag, bang, nestedComment, reported_depth;
	gunichar quote, cdata[3];
	guint cdataIndex;
	gint64 position, markOffset;
	int lineNumber, linePosition, markLineNumber, markLinePosition;
	gsize max_input;
	GMimeHtmlComplianceOptions *options;
	GMimeHtmlComplianceLogger logger;
	gpointer logger_data;
	GDestroyNotify logger_destroy;
	guint cap, issues[GMIME_HTML_COMPLIANCE_VIOLATION_TOO_MANY_COMPLIANCE_ISSUES + 1];
	gboolean cap_reported;
};

G_DEFINE_FINAL_TYPE (GMimeHtmlTokenizer, g_mime_html_tokenizer, G_TYPE_OBJECT)

static void
log_issue (GMimeHtmlTokenizer *tokenizer, GMimeHtmlComplianceViolation violation, gint64 offset, int line, int column)
{
	GMimeHtmlComplianceIssue issue;
	guint cap;

	if (!tokenizer->logger)
		return;

	cap = g_mime_html_compliance_options_get_max_issues_per_violation (tokenizer->options);
	if (cap != tokenizer->cap) {
		memset (tokenizer->issues, 0, sizeof tokenizer->issues);
		tokenizer->cap = cap;
		tokenizer->cap_reported = FALSE;
	}

	if (cap && tokenizer->issues[violation] >= cap) {
		if (tokenizer->cap_reported)
			return;

		tokenizer->cap_reported = TRUE;
		violation = GMIME_HTML_COMPLIANCE_VIOLATION_TOO_MANY_COMPLIANCE_ISSUES;
	} else if (cap) {
		tokenizer->issues[violation]++;
	}

	issue.violation = violation;
	issue.offset = offset;
	issue.line_number = line;
	issue.line_position = column;
	issue.severity = g_mime_html_compliance_violation_get_severity (violation);
	issue.categories = g_mime_html_compliance_violation_get_categories (violation);
	issue.description = g_mime_html_compliance_violation_get_description (violation);

	tokenizer->logger (&issue, tokenizer->logger_data);
}

static void
log_issue_at_current_position (GMimeHtmlTokenizer *tokenizer, GMimeHtmlComplianceViolation violation)
{
	log_issue (tokenizer, violation, tokenizer->position, tokenizer->lineNumber, tokenizer->linePosition);
}

static void
log_issue_at_previous_character (GMimeHtmlTokenizer *tokenizer, GMimeHtmlComplianceViolation violation, gunichar c)
{
	if (c == '\n')
		log_issue_at_current_position (tokenizer, violation);
	else
		log_issue (tokenizer, violation, tokenizer->position - 1, tokenizer->lineNumber,
			   tokenizer->linePosition - 1);
}

static void
mark_previous_character (GMimeHtmlTokenizer *tokenizer)
{
	tokenizer->markOffset = tokenizer->position - 1;
	tokenizer->markLineNumber = tokenizer->lineNumber;
	tokenizer->markLinePosition = tokenizer->linePosition - 1;
}

static void
log_issue_at_mark (GMimeHtmlTokenizer *tokenizer, GMimeHtmlComplianceViolation violation)
{
	log_issue (tokenizer, violation, tokenizer->markOffset, tokenizer->markLineNumber, tokenizer->markLinePosition);
}

static void
log_null_character (GMimeHtmlTokenizer *tokenizer)
{
	log_issue_at_previous_character (tokenizer, GMIME_HTML_COMPLIANCE_VIOLATION_UNEXPECTED_NULL_CHARACTER, 0);
}

static void
log_end_of_file (GMimeHtmlTokenizer *tokenizer, GMimeHtmlComplianceViolation violation)
{
	if (!tokenizer->error)
		log_issue_at_current_position (tokenizer, violation);
}

static void
log_invalid_doc_type_keyword (GMimeHtmlTokenizer *tokenizer, gboolean invalid)
{
	if (invalid)
		log_issue_at_mark (tokenizer,
				   GMIME_HTML_COMPLIANCE_VIOLATION_INVALID_CHARACTER_SEQUENCE_AFTER_DOC_TYPE_NAME);
	else
		log_issue_at_current_position (tokenizer, GMIME_HTML_COMPLIANCE_VIOLATION_EOF_IN_DOC_TYPE);
}

static gboolean
is_alpha_numeric (gunichar c)
{
	return c < 128 && g_ascii_isalnum (c);
}

static gboolean
is_ascii_letter (gunichar c)
{
	return c < 128 && g_ascii_isalpha (c);
}

static gunichar
to_lower (gunichar c)
{
	return c >= 'A' && c <= 'Z' ? c + 32 : c;
}

static gboolean
is_html_white_space (gunichar c)
{
	return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\f';
}

static gboolean
try_peek (GMimeHtmlTokenizer *tokenizer, gunichar *c)
{
	*c = 0;

	if (tokenizer->error || !_g_mime_text_reader_read (&tokenizer->reader, &tokenizer->error))
		return FALSE;

	if (tokenizer->reader.position == tokenizer->reader.end)
		return FALSE;

	if (tokenizer->max_input && (guint64) tokenizer->position >= tokenizer->max_input) {
		g_set_error_literal (&tokenizer->error, GMIME_ERROR, GMIME_ERROR_PARSE_ERROR,
				     "HTML input length limit exceeded");
		return FALSE;
	}

	*c = g_utf8_get_char (tokenizer->reader.buffer + tokenizer->reader.position);

	return TRUE;
}

static void
consume_character (GMimeHtmlTokenizer *tokenizer, gunichar c)
{
	const char *p = tokenizer->reader.buffer + tokenizer->reader.position;

	tokenizer->reader.position += g_utf8_next_char (p) - p;
	tokenizer->position++;

	if (c == '\n') {
		if (tokenizer->lineNumber < G_MAXINT)
			tokenizer->lineNumber++;

		tokenizer->linePosition = 1;
	} else if (tokenizer->linePosition < G_MAXINT) {
		tokenizer->linePosition++;
	}
}

static gboolean
try_read (GMimeHtmlTokenizer *tokenizer, gunichar *c)
{
	if (!try_peek (tokenizer, c))
		return FALSE;

	consume_character (tokenizer, *c);
	return TRUE;
}

/* The span fast paths use GMime's runtime-dispatched bounded SIMD byte search.
 * UTF-8 is validated by the reader; ASCII delimiter matches cannot split a scalar. */
static gboolean
read_until (GMimeHtmlTokenizer *tokenizer, const GMimeReaderByteSet *set, GString *dest, gboolean raw, gunichar *c)
{
	while (try_peek (tokenizer, c)) {
		const char *p = tokenizer->reader.buffer + tokenizer->reader.position;
		gsize n = _g_mime_reader_find_byte ((const guint8 *) p,
						    tokenizer->reader.end - tokenizer->reader.position, set);

		if (n) {
			const char *end = p + n, *q = p;

			while (q < end) {
				gunichar v = g_utf8_get_char (q);

				if (tokenizer->max_input && (guint64) tokenizer->position >= tokenizer->max_input) {
					g_set_error_literal (&tokenizer->error, GMIME_ERROR, GMIME_ERROR_PARSE_ERROR,
							     "HTML input length limit exceeded");
					return FALSE;
				}

				consume_character (tokenizer, v);
				q = g_utf8_next_char (q);
			}

			g_string_append_len (dest, p, n);

			if (raw)
				g_string_append_len (tokenizer->data, p, n);
		}

		if (tokenizer->reader.position < tokenizer->reader.end)
			return try_read (tokenizer, c);
	}

	return FALSE;
}

static gboolean
try_read_data_until (GMimeHtmlTokenizer *tokenizer, const GMimeReaderByteSet *set, gunichar *c)
{
	return read_until (tokenizer, set, tokenizer->data, FALSE, c);
}

static gboolean
try_read_name_until (GMimeHtmlTokenizer *tokenizer, const GMimeReaderByteSet *set, gunichar *c)
{
	return read_until (tokenizer, set, tokenizer->name, TRUE, c);
}

static gboolean
name_is (GMimeHtmlTokenizer *tokenizer, const char *value)
{
	return !g_ascii_strcasecmp (tokenizer->name->str, value);
}

static void
process_character_data (GMimeHtmlTokenizer *tokenizer, gsize start)
{
	const char *p = tokenizer->data->str + start, *end = tokenizer->data->str + tokenizer->data->len;

	while (p < end && _g_mime_html_tree_tracks_characters (tokenizer->tree)) {
		gunichar c = g_utf8_get_char (p);

		p = g_utf8_next_char (p);

		if (!is_html_white_space (c))
			_g_mime_html_tree_character (tokenizer->tree, c);
	}
}

static void
process_character_string (GMimeHtmlTokenizer *tokenizer, const char *p)
{
	while (*p && _g_mime_html_tree_tracks_characters (tokenizer->tree)) {
		gunichar c = g_utf8_get_char (p);

		p = g_utf8_next_char (p);

		if (!is_html_white_space (c))
			_g_mime_html_tree_character (tokenizer->tree, c);
	}
}

static GMimeHtmlToken *
create_tag_token (GMimeHtmlTokenizer *tokenizer, const char *name, gboolean end)
{
	return g_mime_html_token_new_tag (name, end, FALSE, NULL, 0);
}

static GMimeHtmlToken *
create_doc_type_token (GMimeHtmlTokenizer *tokenizer, const char *name)
{
	GMimeHtmlToken *token = g_mime_html_token_new_doctype (NULL, NULL, NULL);

	g_free (token->raw_tag_name);
	token->raw_tag_name = g_strdup (name);

	return token;
}

static void
emit_tag_attribute (GMimeHtmlTokenizer *tokenizer)
{
	if (tokenizer->logger) {
		char *key = g_ascii_strdown (tokenizer->name->str, tokenizer->name->len);

		if (!tokenizer->tag->attributes->len)
			g_hash_table_remove_all (tokenizer->attribute_names);

		if (!g_hash_table_add (tokenizer->attribute_names, key))
			log_issue_at_mark (tokenizer, GMIME_HTML_COMPLIANCE_VIOLATION_DUPLICATE_ATTRIBUTE);
	}

	tokenizer->attribute = g_mime_html_attribute_new (tokenizer->name->str, NULL);
	g_ptr_array_add (tokenizer->tag->attributes, tokenizer->attribute);
	g_string_truncate (tokenizer->name, 0);
}

static GMimeHtmlToken *
emit_comment (GMimeHtmlTokenizer *tokenizer, const char *value, gsize length, gboolean bogus)
{
	GMimeHtmlToken *token = g_mime_html_token_new_comment ("", bogus);

	g_free (token->data);

	token->data = g_malloc (length + 1);
	memcpy (token->data, value, length);
	token->data[length] = 0;
	token->length = length;
	token->bang = tokenizer->bang;

	g_string_truncate (tokenizer->data, 0);
	g_string_truncate (tokenizer->name, 0);

	tokenizer->bang = FALSE;

	return token;
}

static GMimeHtmlToken *
emit_data_token (GMimeHtmlTokenizer *tokenizer, gboolean encode, gboolean truncated)
{
	GMimeHtmlToken *token;

	if (!tokenizer->data->len)
		return NULL;

	if (truncated && tokenizer->ignore_truncated) {
		g_string_truncate (tokenizer->data, 0);
		return NULL;
	}

	token = g_mime_html_token_new_data ("");
	g_free (token->data);
	token->data = g_memdup2 (tokenizer->data->str, tokenizer->data->len + 1);
	token->length = tokenizer->data->len;
	token->encode_entities = encode;

	g_string_truncate (tokenizer->data, 0);

	return token;
}

static GMimeHtmlToken *
emit_c_data_token (GMimeHtmlTokenizer *tokenizer)
{
	GMimeHtmlToken *token = emit_data_token (tokenizer, FALSE, FALSE);

	if (token)
		token->kind = GMIME_HTML_TOKEN_CDATA;

	return token;
}

static GMimeHtmlToken *
emit_script_data_token (GMimeHtmlTokenizer *tokenizer)
{
	GMimeHtmlToken *token = emit_data_token (tokenizer, FALSE, FALSE);

	if (token)
		token->kind = GMIME_HTML_TOKEN_SCRIPT_DATA;

	return token;
}

static GMimeHtmlToken *
emit_eof_in_tag (GMimeHtmlTokenizer *tokenizer)
{
	log_end_of_file (tokenizer, GMIME_HTML_COMPLIANCE_VIOLATION_EOF_IN_TAG);

	return emit_data_token (tokenizer, FALSE, TRUE);
}

static GMimeHtmlToken *
emit_doc_type (GMimeHtmlTokenizer *tokenizer)
{
	GMimeHtmlToken *token = tokenizer->doctype;

	tokenizer->doctype = NULL;
	g_string_truncate (tokenizer->data, 0);

	/* Partial PUBLIC/SYSTEM keywords and quotes must not leak into the next tag. */
	g_string_truncate (tokenizer->name, 0);
	tokenizer->quote = 0;

	return token;
}

static GMimeHtmlToken *
emit_tag_token (GMimeHtmlTokenizer *tokenizer)
{
	GMimeHtmlToken *token = tokenizer->tag;

	if (tokenizer->logger && token->is_end_tag) {
		if (token->attributes->len)
			log_issue_at_previous_character (tokenizer,
							 GMIME_HTML_COMPLIANCE_VIOLATION_END_TAG_WITH_ATTRIBUTES, '>');

		if (token->is_empty_element)
			log_issue_at_previous_character (tokenizer,
							 GMIME_HTML_COMPLIANCE_VIOLATION_END_TAG_WITH_TRAILING_SOLIDUS,
							 '>');
	}

	tokenizer->state = _g_mime_html_tree_tag (tokenizer->tree, token);

	if (tokenizer->state == GMIME_HTML_TOKENIZER_STATE_RAW_TEXT ||
	    tokenizer->state == GMIME_HTML_TOKENIZER_STATE_RC_DATA)
		tokenizer->activeTagName = g_mime_html_tag_id_to_name (token->id);

	if (tokenizer->tree->BrokeOutOfForeignContent)
		log_issue_at_previous_character (tokenizer, GMIME_HTML_COMPLIANCE_VIOLATION_FOREIGN_CONTENT_BREAKOUT,
						 '>');

	if (!token->is_end_tag) {
		if (tokenizer->tree->disposition == GMIME_HTML_TAG_DISPOSITION_IGNORED)
			log_issue_at_previous_character (tokenizer, GMIME_HTML_COMPLIANCE_VIOLATION_IGNORED_START_TAG,
							 '>');
		else if (tokenizer->tree->disposition == GMIME_HTML_TAG_DISPOSITION_ATTRIBUTES_MERGED)
			log_issue_at_previous_character (tokenizer,
							 GMIME_HTML_COMPLIANCE_VIOLATION_ATTRIBUTES_MERGED_INTO_EXISTING_ELEMENT,
							 '>');
	}

	/* Once structural tracking overflows, the remaining input is literal text. */
	if (tokenizer->tree->depth_exceeded && !tokenizer->reported_depth) {
		log_issue_at_previous_character (tokenizer,
						 GMIME_HTML_COMPLIANCE_VIOLATION_ELEMENT_DEPTH_LIMIT_EXCEEDED, '>');
		tokenizer->reported_depth = TRUE;
	}

	g_string_truncate (tokenizer->data, 0);
	tokenizer->tag = NULL;
	tokenizer->attribute = NULL;

	return token;
}

static GMimeHtmlToken *
read_c_data_section (GMimeHtmlTokenizer *tokenizer)
{
	gunichar c;

	while (try_read (tokenizer, &c)) {
		if (tokenizer->cdataIndex == 3) {
			g_string_append_unichar (tokenizer->data, tokenizer->cdata[0]);
			tokenizer->cdata[0] = tokenizer->cdata[1];
			tokenizer->cdata[1] = tokenizer->cdata[2];
			tokenizer->cdata[2] = c;
		} else {
			tokenizer->cdata[tokenizer->cdataIndex++] = c;
		}

		/* Check the first three characters too, so empty CDATA terminates. */
		if (tokenizer->cdataIndex == 3 && tokenizer->cdata[0] == ']' && tokenizer->cdata[1] == ']' &&
		    tokenizer->cdata[2] == '>') {
			tokenizer->state = GMIME_HTML_TOKENIZER_STATE_DATA;
			tokenizer->cdataIndex = 0;
			process_character_data (tokenizer, 0);
			return emit_c_data_token (tokenizer);
		}
	}

	log_end_of_file (tokenizer, GMIME_HTML_COMPLIANCE_VIOLATION_EOF_IN_C_DATA);

	tokenizer->state = GMIME_HTML_TOKENIZER_STATE_END_OF_FILE;

	for (guint i = 0; i < tokenizer->cdataIndex; i++)
		g_string_append_unichar (tokenizer->data, tokenizer->cdata[i]);

	tokenizer->cdataIndex = 0;
	process_character_data (tokenizer, 0);

	return emit_c_data_token (tokenizer);
}

static void
set_string (char **dest, const char *value)
{
	g_free (*dest);
	*dest = g_strdup (value);
}

#include "gmime-html-tokenizer-states-private.h"

static void
g_mime_html_tokenizer_finalize (GObject *object)
{
	GMimeHtmlTokenizer *tokenizer = GMIME_HTML_TOKENIZER (object);

	if (tokenizer->reader.stream)
		g_object_unref (tokenizer->reader.stream);

	g_clear_error (&tokenizer->error);
	_g_mime_html_tree_free (tokenizer->tree);
	g_string_free (tokenizer->data, TRUE);
	g_string_free (tokenizer->name, TRUE);
	g_hash_table_unref (tokenizer->attribute_names);
	g_mime_html_token_unref (tokenizer->tag);
	g_mime_html_token_unref (tokenizer->doctype);
	g_mime_html_compliance_options_unref (tokenizer->options);

	if (tokenizer->logger_destroy)
		tokenizer->logger_destroy (tokenizer->logger_data);

	G_OBJECT_CLASS (g_mime_html_tokenizer_parent_class)->finalize (object);
}

static void
g_mime_html_tokenizer_class_init (GMimeHtmlTokenizerClass *klass)
{
	GObjectClass *object = G_OBJECT_CLASS (klass);

	object->finalize = g_mime_html_tokenizer_finalize;
}

static void
g_mime_html_tokenizer_init (GMimeHtmlTokenizer *tokenizer)
{
	tokenizer->reader.allow_nul = TRUE;
	tokenizer->tree = _g_mime_html_tree_new (4096);
	tokenizer->data = g_string_sized_new (2048);
	tokenizer->name = g_string_sized_new (32);
	tokenizer->attribute_names = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, NULL);
	tokenizer->options = g_mime_html_compliance_options_new ();
	tokenizer->decode = TRUE;
	tokenizer->lineNumber = tokenizer->linePosition = 1;
	tokenizer->activeTagName = "";
}

/**
 * g_mime_html_tokenizer_new:
 * @stream: (transfer none): a stream containing UTF-8 input
 *
 * Creates a tokenizer which retains @stream without closing it.
 *
 * Returns: (transfer full): a new tokenizer
 */
GMimeHtmlTokenizer *
g_mime_html_tokenizer_new (GMimeStream *stream)
{
	GMimeHtmlTokenizer *tokenizer;

	g_return_val_if_fail (GMIME_IS_STREAM (stream), NULL);

	tokenizer = g_object_new (GMIME_TYPE_HTML_TOKENIZER, NULL);
	tokenizer->reader.stream = g_object_ref (stream);

	return tokenizer;
}

/**
 * g_mime_html_tokenizer_new_with_charset:
 * @stream: (transfer none): the input stream
 * @charset: the input character set
 * @detect_bom: whether to detect a byte-order mark
 * @buffer_size: the input buffer size
 * @error: (out) (optional): return location for an error
 *
 * Creates a tokenizer with character-set decoding. The tokenizer retains
 * @stream without closing it.
 *
 * Returns: (transfer full) (nullable): a new tokenizer, or %NULL on error
 */
GMimeHtmlTokenizer *
g_mime_html_tokenizer_new_with_charset (GMimeStream *stream, const char *charset, gboolean detect_bom,
					gsize buffer_size, GError **error)
{
	GMimeStream *decoded = _g_mime_text_stream_new_decoder (stream, charset, detect_bom, buffer_size, error);
	GMimeHtmlTokenizer *tokenizer;

	if (!decoded)
		return NULL;

	tokenizer = g_mime_html_tokenizer_new (decoded);
	g_object_unref (decoded);

	return tokenizer;
}

/**
 * g_mime_html_tokenizer_read_next_token:
 * @tokenizer: a #GMimeHtmlTokenizer
 * @token: (out) (transfer full) (nullable): the next token, or %NULL at end of input
 * @error: (out) (optional): return location for an error
 *
 * Reads the next token. A successful read at end of input sets @token to
 * %NULL. On failure, @token is also set to %NULL and @error is set.
 *
 * Returns: %TRUE on success, including end of input, or %FALSE on error
 */
gboolean
g_mime_html_tokenizer_read_next_token (GMimeHtmlTokenizer *tokenizer, GMimeHtmlToken **token, GError **error)
{
	g_return_val_if_fail (GMIME_IS_HTML_TOKENIZER (tokenizer), FALSE);
	g_return_val_if_fail (token != NULL, FALSE);

	*token = NULL;

	while (!tokenizer->error && tokenizer->state != GMIME_HTML_TOKENIZER_STATE_END_OF_FILE) {
		*token = read_state (tokenizer);
		if (*token)
			break;
	}

	if (tokenizer->error) {
		g_mime_html_token_unref (*token);
		*token = NULL;

		if (error)
			*error = g_error_copy (tokenizer->error);

		return FALSE;
	}

	return TRUE;
}

/**
 * g_mime_html_tokenizer_get_decode_character_references:
 * @tokenizer: a #GMimeHtmlTokenizer
 *
 * Gets whether character references in text are decoded. The default is %TRUE.
 * Attribute references are always decoded.
 *
 * Returns: %TRUE if text references are decoded or %FALSE otherwise
 **/
gboolean
g_mime_html_tokenizer_get_decode_character_references (GMimeHtmlTokenizer *tokenizer)
{
	g_return_val_if_fail (GMIME_IS_HTML_TOKENIZER (tokenizer), FALSE);

	return tokenizer->decode;
}

/**
 * g_mime_html_tokenizer_set_decode_character_references:
 * @tokenizer: a #GMimeHtmlTokenizer
 * @value: whether to decode character references in text
 *
 * Sets whether text references are decoded. Attribute references are always
 * decoded, regardless of this setting.
 **/
void
g_mime_html_tokenizer_set_decode_character_references (GMimeHtmlTokenizer *tokenizer, gboolean value)
{
	g_return_if_fail (GMIME_IS_HTML_TOKENIZER (tokenizer));

	tokenizer->decode = !!value;
}

/**
 * g_mime_html_tokenizer_get_namespace:
 * @tokenizer: a #GMimeHtmlTokenizer
 *
 * Gets the document namespace. The default is %GMIME_HTML_NAMESPACE_HTML;
 * an HTML element's namespace declaration can update it while parsing.
 *
 * Returns: the document namespace
 **/
GMimeHtmlNamespace
g_mime_html_tokenizer_get_namespace (GMimeHtmlTokenizer *tokenizer)
{
	g_return_val_if_fail (GMIME_IS_HTML_TOKENIZER (tokenizer), GMIME_HTML_NAMESPACE_HTML);

	return tokenizer->tree->ns;
}

/**
 * g_mime_html_tokenizer_set_namespace:
 * @tokenizer: a #GMimeHtmlTokenizer
 * @ns: the document namespace
 *
 * Sets the document namespace used by tree construction.
 **/
void
g_mime_html_tokenizer_set_namespace (GMimeHtmlTokenizer *tokenizer, GMimeHtmlNamespace ns)
{
	g_return_if_fail (GMIME_IS_HTML_TOKENIZER (tokenizer));
	g_return_if_fail (ns >= GMIME_HTML_NAMESPACE_HTML && ns <= GMIME_HTML_NAMESPACE_XML_NS);

	tokenizer->tree->ns = ns;
}

/**
 * g_mime_html_tokenizer_get_ignore_truncated_tags:
 * @tokenizer: a #GMimeHtmlTokenizer
 *
 * Gets whether incomplete tags at end of input are discarded. The default is
 * %FALSE, preserving truncated tag text.
 *
 * Returns: %TRUE if truncated tags are discarded or %FALSE otherwise
 **/
gboolean
g_mime_html_tokenizer_get_ignore_truncated_tags (GMimeHtmlTokenizer *tokenizer)
{
	g_return_val_if_fail (GMIME_IS_HTML_TOKENIZER (tokenizer), FALSE);

	return tokenizer->ignore_truncated;
}

/**
 * g_mime_html_tokenizer_set_ignore_truncated_tags:
 * @tokenizer: a #GMimeHtmlTokenizer
 * @value: whether to discard incomplete tags at end of input
 *
 * Sets how truncated tags are handled. This does not disable compliance
 * diagnostics for those tags.
 **/
void
g_mime_html_tokenizer_set_ignore_truncated_tags (GMimeHtmlTokenizer *tokenizer, gboolean value)
{
	g_return_if_fail (GMIME_IS_HTML_TOKENIZER (tokenizer));

	tokenizer->ignore_truncated = !!value;
}

/**
 * g_mime_html_tokenizer_get_max_element_depth:
 * @tokenizer: a #GMimeHtmlTokenizer
 *
 * Gets the maximum number of distinct open-element stack entries. Consecutive
 * identical elements can share an entry. The default is 4096.
 *
 * Returns: the maximum stack depth
 **/
guint
g_mime_html_tokenizer_get_max_element_depth (GMimeHtmlTokenizer *tokenizer)
{
	g_return_val_if_fail (GMIME_IS_HTML_TOKENIZER (tokenizer), 0);

	return tokenizer->tree->max_depth;
}

/**
 * g_mime_html_tokenizer_set_max_element_depth:
 * @tokenizer: a #GMimeHtmlTokenizer
 * @maximum: the maximum stack depth, from 1 through %G_MAXINT
 *
 * Sets the open-element stack limit. Exceeding it reports
 * %GMIME_HTML_COMPLIANCE_VIOLATION_ELEMENT_DEPTH_LIMIT_EXCEEDED when a logger is
 * installed and treats the remaining input as plain text rather than failing.
 **/
void
g_mime_html_tokenizer_set_max_element_depth (GMimeHtmlTokenizer *tokenizer, guint maximum)
{
	g_return_if_fail (GMIME_IS_HTML_TOKENIZER (tokenizer));
	g_return_if_fail (maximum > 0 && maximum <= G_MAXINT);

	tokenizer->tree->max_depth = maximum;
}

/**
 * g_mime_html_tokenizer_get_scripting_enabled:
 * @tokenizer: a #GMimeHtmlTokenizer
 *
 * Gets the scripting mode used to interpret noscript elements. The default is
 * %FALSE. The tokenizer never executes scripts.
 *
 * Returns: %TRUE if scripting-mode parsing is enabled or %FALSE otherwise
 **/
gboolean
g_mime_html_tokenizer_get_scripting_enabled (GMimeHtmlTokenizer *tokenizer)
{
	g_return_val_if_fail (GMIME_IS_HTML_TOKENIZER (tokenizer), FALSE);

	return tokenizer->tree->scripting;
}

/**
 * g_mime_html_tokenizer_set_scripting_enabled:
 * @tokenizer: a #GMimeHtmlTokenizer
 * @enabled: whether to use scripting-mode parsing
 *
 * Sets whether noscript content is treated as raw text. This should match the
 * scripting mode of the intended renderer; it does not execute any scripts.
 **/
void
g_mime_html_tokenizer_set_scripting_enabled (GMimeHtmlTokenizer *tokenizer, gboolean enabled)
{
	g_return_if_fail (GMIME_IS_HTML_TOKENIZER (tokenizer));

	tokenizer->tree->scripting = !!enabled;
}

/**
 * g_mime_html_tokenizer_get_position:
 * @tokenizer: a #GMimeHtmlTokenizer
 *
 * Gets the number of Unicode scalar values consumed, rather than a byte
 * offset in the input stream. Buffered input may extend beyond this position.
 *
 * Returns: the zero-based character position
 **/
gint64
g_mime_html_tokenizer_get_position (GMimeHtmlTokenizer *tokenizer)
{
	g_return_val_if_fail (GMIME_IS_HTML_TOKENIZER (tokenizer), -1);

	return tokenizer->position;
}

/**
 * g_mime_html_tokenizer_get_line_number:
 * @tokenizer: a #GMimeHtmlTokenizer
 *
 * Gets the line number of the next input character. LF advances the line
 * number; a standalone CR does not.
 *
 * Returns: the one-based line number
 **/
int
g_mime_html_tokenizer_get_line_number (GMimeHtmlTokenizer *tokenizer)
{
	g_return_val_if_fail (GMIME_IS_HTML_TOKENIZER (tokenizer), -1);

	return tokenizer->lineNumber;
}

/**
 * g_mime_html_tokenizer_get_line_position:
 * @tokenizer: a #GMimeHtmlTokenizer
 *
 * Gets the column of the next input character, measured in Unicode scalar
 * values rather than bytes.
 *
 * Returns: the one-based line position
 **/
int
g_mime_html_tokenizer_get_line_position (GMimeHtmlTokenizer *tokenizer)
{
	g_return_val_if_fail (GMIME_IS_HTML_TOKENIZER (tokenizer), -1);

	return tokenizer->linePosition;
}

/**
 * g_mime_html_tokenizer_get_state:
 * @tokenizer: a #GMimeHtmlTokenizer
 *
 * Gets the current lexical state. A new tokenizer starts in
 * %GMIME_HTML_TOKENIZER_STATE_DATA.
 *
 * Returns: the tokenizer state
 **/
GMimeHtmlTokenizerState
g_mime_html_tokenizer_get_state (GMimeHtmlTokenizer *tokenizer)
{
	g_return_val_if_fail (GMIME_IS_HTML_TOKENIZER (tokenizer), GMIME_HTML_TOKENIZER_STATE_END_OF_FILE);

	return tokenizer->state;
}

/**
 * g_mime_html_tokenizer_set_compliance_logger:
 * @tokenizer: a #GMimeHtmlTokenizer
 * @logger: (nullable) (scope notified) (closure user_data) (destroy destroy): the diagnostic callback
 * @user_data: (nullable): data passed to @logger
 * @destroy: (nullable): function to release @user_data
 *
 * Sets the diagnostic callback, or disables reporting when @logger is %NULL.
 * The previous callback's data is released when replaced. The new data is
 * released when the callback is replaced or the tokenizer is finalized.
 */
void
g_mime_html_tokenizer_set_compliance_logger (GMimeHtmlTokenizer *tokenizer, GMimeHtmlComplianceLogger logger,
					     gpointer user_data, GDestroyNotify destroy)
{
	g_return_if_fail (GMIME_IS_HTML_TOKENIZER (tokenizer));

	if (tokenizer->logger_destroy)
		tokenizer->logger_destroy (tokenizer->logger_data);

	tokenizer->logger = logger;
	tokenizer->logger_data = user_data;
	tokenizer->logger_destroy = destroy;
	memset (tokenizer->issues, 0, sizeof tokenizer->issues);
	tokenizer->cap_reported = FALSE;
}

/**
 * g_mime_html_tokenizer_get_compliance_options:
 * @tokenizer: a #GMimeHtmlTokenizer
 *
 * Returns: (transfer none): the options owned by @tokenizer
 */
GMimeHtmlComplianceOptions *
g_mime_html_tokenizer_get_compliance_options (GMimeHtmlTokenizer *tokenizer)
{
	g_return_val_if_fail (GMIME_IS_HTML_TOKENIZER (tokenizer), NULL);

	return tokenizer->options;
}

/**
 * g_mime_html_tokenizer_set_compliance_options:
 * @tokenizer: a #GMimeHtmlTokenizer
 * @options: (transfer none): the diagnostic options
 *
 * Retains a reference to @options.
 */
void
g_mime_html_tokenizer_set_compliance_options (GMimeHtmlTokenizer *tokenizer, GMimeHtmlComplianceOptions *options)
{
	g_return_if_fail (GMIME_IS_HTML_TOKENIZER (tokenizer));
	g_return_if_fail (options != NULL);

	g_mime_html_compliance_options_ref (options);
	g_mime_html_compliance_options_unref (tokenizer->options);

	tokenizer->options = options;
	memset (tokenizer->issues, 0, sizeof tokenizer->issues);
	tokenizer->cap_reported = FALSE;
}

void
_g_mime_html_tokenizer_set_track_closed_elements (GMimeHtmlTokenizer *tokenizer, gboolean enabled)
{
	tokenizer->tree->track_closed = enabled;
}

const GMimeHtmlClosedElement *
_g_mime_html_tokenizer_get_closed_elements (GMimeHtmlTokenizer *tokenizer, gsize *count)
{
	if (count)
		*count = tokenizer->tree->closed->len;

	return (const GMimeHtmlClosedElement *) tokenizer->tree->closed->data;
}

GMimeHtmlTagDisposition
_g_mime_html_tokenizer_get_tag_disposition (GMimeHtmlTokenizer *tokenizer)
{
	return tokenizer->tree->disposition;
}

gboolean
_g_mime_html_tokenizer_get_in_frameset (GMimeHtmlTokenizer *tokenizer)
{
	return tokenizer->tree->inFrameset;
}

gboolean
_g_mime_html_tokenizer_get_depth_exceeded (GMimeHtmlTokenizer *tokenizer)
{
	return tokenizer->tree->depth_exceeded;
}

gboolean
_g_mime_html_tokenizer_get_broke_out_of_foreign_content (GMimeHtmlTokenizer *tokenizer)
{
	return tokenizer->tree->BrokeOutOfForeignContent;
}

gboolean
_g_mime_html_tokenizer_get_is_noscript_open (GMimeHtmlTokenizer *tokenizer)
{
	return _g_mime_html_tree_noscript (tokenizer->tree);
}

void
_g_mime_html_tokenizer_clear_closed_elements (GMimeHtmlTokenizer *tokenizer)
{
	_g_mime_html_tree_clear_closed (tokenizer->tree);
}

void
_g_mime_html_tokenizer_set_max_input_length (GMimeHtmlTokenizer *tokenizer, gsize maximum)
{
	tokenizer->max_input = maximum;
}
