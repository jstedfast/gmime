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

/* Direct translation of all upstream lexical states. */
static const GMimeReaderByteSet DataSpecials = { { 1, 0, 0, 0, 0, 0, 4, 0, 0, 0, 0, 0, 8, 0, 0, 0 }, FALSE };
static const GMimeReaderByteSet LoggedDataSpecials = { { 1, 0, 0, 0, 0, 0, 4, 0, 0, 0, 0, 0, 8, 0, 0, 0 }, FALSE };
static const GMimeReaderByteSet RcDataSpecials = { { 1, 0, 0, 0, 0, 0, 4, 0, 0, 0, 0, 0, 8, 0, 0, 0 }, FALSE };
static const GMimeReaderByteSet RawTextSpecials = { { 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 8, 0, 0, 0 }, FALSE };
static const GMimeReaderByteSet PlainTextSpecials = { { 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }, FALSE };
static const GMimeReaderByteSet AttributeValueDoubleQuotedSpecials = {
	{ 1, 0, 4, 0, 0, 0, 4, 0, 0, 0, 0, 0, 0, 0, 0, 0 }, FALSE
};

static const GMimeReaderByteSet AttributeValueSingleQuotedSpecials = {
	{ 1, 0, 0, 0, 0, 0, 4, 4, 0, 0, 0, 0, 0, 0, 0, 0 }, FALSE
};

static const GMimeReaderByteSet BogusCommentSpecials = { { 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 8, 0 }, FALSE };
static const GMimeReaderByteSet CommentSpecials = { { 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 4, 0, 0 }, FALSE };
static GMimeHtmlToken *read_character_reference (GMimeHtmlTokenizer *tokenizer, GMimeHtmlTokenizerState next);
static int validate_character_reference (GMimeHtmlTokenizer *tokenizer, gboolean inAttribute, gunichar next);
static void read_ambiguous_ampersand (GMimeHtmlTokenizer *tokenizer, gboolean inAttribute, int length);
static GMimeHtmlToken *read_generic_raw_text_less_than (GMimeHtmlTokenizer *tokenizer, GMimeHtmlTokenizerState rawText,
							GMimeHtmlTokenizerState rawTextEndTagOpen);
static GMimeHtmlToken *read_generic_raw_text_end_tag_open (GMimeHtmlTokenizer *tokenizer, gboolean decoded,
							   GMimeHtmlTokenizerState rawText,
							   GMimeHtmlTokenizerState rawTextEndTagName);
static GMimeHtmlToken *read_generic_raw_text_end_tag_name (GMimeHtmlTokenizer *tokenizer, gboolean decoded,
							   GMimeHtmlTokenizerState rawText);
static GMimeHtmlToken *read_data (GMimeHtmlTokenizer *tokenizer);
static GMimeHtmlToken *read_character_reference_in_data (GMimeHtmlTokenizer *tokenizer);
static GMimeHtmlToken *read_rc_data (GMimeHtmlTokenizer *tokenizer);
static GMimeHtmlToken *read_character_reference_in_rc_data (GMimeHtmlTokenizer *tokenizer);
static GMimeHtmlToken *read_raw_text (GMimeHtmlTokenizer *tokenizer);
static GMimeHtmlToken *read_script_data (GMimeHtmlTokenizer *tokenizer);
static GMimeHtmlToken *read_plain_text (GMimeHtmlTokenizer *tokenizer);
static GMimeHtmlToken *read_tag_open (GMimeHtmlTokenizer *tokenizer);
static GMimeHtmlToken *read_end_tag_open (GMimeHtmlTokenizer *tokenizer);
static GMimeHtmlToken *read_tag_name (GMimeHtmlTokenizer *tokenizer);
static GMimeHtmlToken *read_rc_data_less_than (GMimeHtmlTokenizer *tokenizer);
static GMimeHtmlToken *read_rc_data_end_tag_open (GMimeHtmlTokenizer *tokenizer);
static GMimeHtmlToken *read_rc_data_end_tag_name (GMimeHtmlTokenizer *tokenizer);
static GMimeHtmlToken *read_raw_text_less_than (GMimeHtmlTokenizer *tokenizer);
static GMimeHtmlToken *read_raw_text_end_tag_open (GMimeHtmlTokenizer *tokenizer);
static GMimeHtmlToken *read_raw_text_end_tag_name (GMimeHtmlTokenizer *tokenizer);
static GMimeHtmlToken *read_script_data_less_than (GMimeHtmlTokenizer *tokenizer);
static GMimeHtmlToken *read_script_data_end_tag_open (GMimeHtmlTokenizer *tokenizer);
static GMimeHtmlToken *read_script_data_end_tag_name (GMimeHtmlTokenizer *tokenizer);
static GMimeHtmlToken *read_script_data_escape_start (GMimeHtmlTokenizer *tokenizer);
static GMimeHtmlToken *read_script_data_escape_start_dash (GMimeHtmlTokenizer *tokenizer);
static GMimeHtmlToken *read_script_data_escaped (GMimeHtmlTokenizer *tokenizer);
static GMimeHtmlToken *read_script_data_escaped_dash (GMimeHtmlTokenizer *tokenizer);
static GMimeHtmlToken *read_script_data_escaped_dash_dash (GMimeHtmlTokenizer *tokenizer);
static GMimeHtmlToken *read_script_data_escaped_less_than (GMimeHtmlTokenizer *tokenizer);
static GMimeHtmlToken *read_script_data_escaped_end_tag_open (GMimeHtmlTokenizer *tokenizer);
static GMimeHtmlToken *read_script_data_escaped_end_tag_name (GMimeHtmlTokenizer *tokenizer);
static GMimeHtmlToken *read_script_data_double_escape_start (GMimeHtmlTokenizer *tokenizer);
static GMimeHtmlToken *read_script_data_double_escaped (GMimeHtmlTokenizer *tokenizer);
static GMimeHtmlToken *read_script_data_double_escaped_dash (GMimeHtmlTokenizer *tokenizer);
static GMimeHtmlToken *read_script_data_double_escaped_dash_dash (GMimeHtmlTokenizer *tokenizer);
static GMimeHtmlToken *read_script_data_double_escaped_less_than (GMimeHtmlTokenizer *tokenizer);
static GMimeHtmlToken *read_script_data_double_escape_end (GMimeHtmlTokenizer *tokenizer);
static GMimeHtmlToken *read_before_attribute_name (GMimeHtmlTokenizer *tokenizer);
static GMimeHtmlToken *read_attribute_name (GMimeHtmlTokenizer *tokenizer);
static GMimeHtmlToken *read_after_attribute_name (GMimeHtmlTokenizer *tokenizer);
static GMimeHtmlToken *read_before_attribute_value (GMimeHtmlTokenizer *tokenizer);
static GMimeHtmlToken *read_attribute_value_double_quoted (GMimeHtmlTokenizer *tokenizer);
static GMimeHtmlToken *read_attribute_value_single_quoted (GMimeHtmlTokenizer *tokenizer);
static GMimeHtmlToken *read_attribute_value_unquoted (GMimeHtmlTokenizer *tokenizer);
static GMimeHtmlToken *read_character_reference_in_attribute_value (GMimeHtmlTokenizer *tokenizer);
static GMimeHtmlToken *read_after_attribute_value_quoted (GMimeHtmlTokenizer *tokenizer);
static GMimeHtmlToken *read_self_closing_start_tag (GMimeHtmlTokenizer *tokenizer);
static GMimeHtmlToken *read_bogus_comment (GMimeHtmlTokenizer *tokenizer);
static GMimeHtmlToken *read_markup_declaration_open (GMimeHtmlTokenizer *tokenizer);
static GMimeHtmlToken *read_comment_start (GMimeHtmlTokenizer *tokenizer);
static GMimeHtmlToken *read_comment_start_dash (GMimeHtmlTokenizer *tokenizer);
static GMimeHtmlToken *read_comment (GMimeHtmlTokenizer *tokenizer);
static GMimeHtmlToken *read_comment_end_dash (GMimeHtmlTokenizer *tokenizer);
static GMimeHtmlToken *read_comment_end (GMimeHtmlTokenizer *tokenizer);
static GMimeHtmlToken *read_comment_end_bang (GMimeHtmlTokenizer *tokenizer);
static GMimeHtmlToken *read_doc_type (GMimeHtmlTokenizer *tokenizer);
static GMimeHtmlToken *read_before_doc_type_name (GMimeHtmlTokenizer *tokenizer);
static GMimeHtmlToken *read_doc_type_name (GMimeHtmlTokenizer *tokenizer);
static GMimeHtmlToken *read_after_doc_type_name (GMimeHtmlTokenizer *tokenizer);
static GMimeHtmlToken *read_after_doc_type_public_keyword (GMimeHtmlTokenizer *tokenizer);
static GMimeHtmlToken *read_before_doc_type_public_identifier (GMimeHtmlTokenizer *tokenizer);
static GMimeHtmlToken *read_doc_type_public_identifier_quoted (GMimeHtmlTokenizer *tokenizer);
static GMimeHtmlToken *read_after_doc_type_public_identifier (GMimeHtmlTokenizer *tokenizer);
static GMimeHtmlToken *read_between_doc_type_public_and_system_identifiers (GMimeHtmlTokenizer *tokenizer);
static GMimeHtmlToken *read_after_doc_type_system_keyword (GMimeHtmlTokenizer *tokenizer);
static GMimeHtmlToken *read_before_doc_type_system_identifier (GMimeHtmlTokenizer *tokenizer);
static GMimeHtmlToken *read_doc_type_system_identifier_quoted (GMimeHtmlTokenizer *tokenizer);
static GMimeHtmlToken *read_after_doc_type_system_identifier (GMimeHtmlTokenizer *tokenizer);
static GMimeHtmlToken *read_bogus_doc_type (GMimeHtmlTokenizer *tokenizer);

static GMimeHtmlToken *
read_character_reference (GMimeHtmlTokenizer *tokenizer, GMimeHtmlTokenizerState next)
{
	gunichar c;

	if (!try_peek (tokenizer, &c)) {
		tokenizer->state = GMIME_HTML_TOKENIZER_STATE_END_OF_FILE;
		g_string_append_unichar (tokenizer->data, '&');

		return emit_data_token (tokenizer, tokenizer->decode, FALSE);
	}

	switch (c) {
	case '\t':
	case '\r':
	case '\n':
	case '\f':
	case ' ':
	case '<':
	case '&':
		// no character is consumed, emit '&'
		tokenizer->state = next;
		g_string_append_unichar (tokenizer->data, '&');

		if (next == GMIME_HTML_TOKENIZER_STATE_DATA && _g_mime_html_tree_tracks_characters (tokenizer->tree))
			_g_mime_html_tree_character (tokenizer->tree, '&');

		return NULL;
	}

	// Note: The '&' has already been consumed.
	if (tokenizer->logger != NULL)
		mark_previous_character (tokenizer);

	_g_mime_html_entity_push (&tokenizer->entity, '&');

	if (!tokenizer->decode)
		g_string_append_unichar (tokenizer->data, '&');

	while (_g_mime_html_entity_push (&tokenizer->entity, c)) {
		consume_character (tokenizer, c);

		if (!tokenizer->decode)
			g_string_append_unichar (tokenizer->data, c);

		if (c == ';')
			break;

		if (!try_peek (tokenizer, &c)) {
			tokenizer->state = GMIME_HTML_TOKENIZER_STATE_END_OF_FILE;
			if (tokenizer->logger != NULL)
				validate_character_reference (tokenizer, FALSE, '\0');

			if (tokenizer->decode)
				g_string_append (tokenizer->data,
						 _g_mime_html_entity_value (&tokenizer->entity, FALSE, 0));

			_g_mime_html_entity_reset (&tokenizer->entity);

			return emit_data_token (tokenizer, tokenizer->decode, FALSE);
		}
	}

	tokenizer->state = next;

	int unknown = tokenizer->logger != NULL ? validate_character_reference (tokenizer, FALSE, c) : -1;

	if (tokenizer->decode ||
	    (next == GMIME_HTML_TOKENIZER_STATE_DATA && _g_mime_html_tree_tracks_characters (tokenizer->tree))) {
		const char *value = _g_mime_html_entity_value (&tokenizer->entity, FALSE, 0);

		if (next == GMIME_HTML_TOKENIZER_STATE_DATA && _g_mime_html_tree_tracks_characters (tokenizer->tree))
			process_character_string (tokenizer, value);

		if (tokenizer->decode)
			g_string_append (tokenizer->data, value);
	}

	_g_mime_html_entity_reset (&tokenizer->entity);

	if (unknown != -1)
		read_ambiguous_ampersand (tokenizer, FALSE, unknown);

	return NULL;
}

static int
validate_character_reference (GMimeHtmlTokenizer *tokenizer, gboolean inAttribute, gunichar next)
{
	GMimeHtmlComplianceViolation first, second;

	_g_mime_html_entity_violations (&tokenizer->entity, inAttribute, next, &first, &second);

	if (first != GMIME_HTML_COMPLIANCE_VIOLATION_NONE)
		log_issue_at_mark (tokenizer, first);

	if (second != GMIME_HTML_COMPLIANCE_VIOLATION_NONE)
		log_issue_at_mark (tokenizer, second);

	return _g_mime_html_entity_unknown_length (&tokenizer->entity);
}

static void
read_ambiguous_ampersand (GMimeHtmlTokenizer *tokenizer, gboolean inAttribute, int length)
{
	gunichar c;

	while (try_peek (tokenizer, &c) && is_alpha_numeric (c)) {
		consume_character (tokenizer, c);

		if (inAttribute)
			g_string_append_unichar (tokenizer->name, c);

		g_string_append_unichar (tokenizer->data, c);
		length++;
	}

	if (length > 0 && try_peek (tokenizer, &c) && c == ';')
		log_issue_at_mark (tokenizer, GMIME_HTML_COMPLIANCE_VIOLATION_UNKNOWN_NAMED_CHARACTER_REFERENCE);
}

static GMimeHtmlToken *
read_generic_raw_text_less_than (GMimeHtmlTokenizer *tokenizer, GMimeHtmlTokenizerState rawText,
				 GMimeHtmlTokenizerState rawTextEndTagOpen)
{
	gunichar c;

	g_string_append_unichar (tokenizer->data, '<');

	if (try_peek (tokenizer, &c) && c == '/') {
		tokenizer->state = rawTextEndTagOpen;
		consume_character (tokenizer, c);
		g_string_append_unichar (tokenizer->data, '/');
		g_string_truncate (tokenizer->name, 0);
	} else {
		tokenizer->state = rawText;
	}

	return NULL;
}

static GMimeHtmlToken *
read_generic_raw_text_end_tag_open (GMimeHtmlTokenizer *tokenizer, gboolean decoded, GMimeHtmlTokenizerState rawText,
				    GMimeHtmlTokenizerState rawTextEndTagName)
{
	gunichar c;

	if (!try_peek (tokenizer, &c)) {
		tokenizer->state = GMIME_HTML_TOKENIZER_STATE_END_OF_FILE;
		return emit_data_token (tokenizer, decoded, FALSE);
	}

	if (is_ascii_letter (c)) {
		tokenizer->state = rawTextEndTagName;
		consume_character (tokenizer, c);
		g_string_append_unichar (tokenizer->name, c);
		g_string_append_unichar (tokenizer->data, c);
	} else {
		tokenizer->state = rawText;
	}

	return NULL;
}

static GMimeHtmlToken *
read_generic_raw_text_end_tag_name (GMimeHtmlTokenizer *tokenizer, gboolean decoded, GMimeHtmlTokenizerState rawText)
{
	gunichar c;

	GMimeHtmlTokenizerState current = tokenizer->state;

	do {
		if (!try_peek (tokenizer, &c)) {
			tokenizer->state = GMIME_HTML_TOKENIZER_STATE_END_OF_FILE;
			g_string_truncate (tokenizer->name, 0);

			return emit_data_token (tokenizer, decoded, FALSE);
		}

		switch (c) {
		case '\t':
		case '\r':
		case '\n':
		case '\f':
		case ' ':
			if (name_is (tokenizer, tokenizer->activeTagName)) {
				tokenizer->state = GMIME_HTML_TOKENIZER_STATE_BEFORE_ATTRIBUTE_NAME;
				break;
			}

			goto default_case;
		case '/':
			if (name_is (tokenizer, tokenizer->activeTagName)) {
				tokenizer->state = GMIME_HTML_TOKENIZER_STATE_SELF_CLOSING_START_TAG;
				break;
			}

			goto default_case;
		case '>':
			if (name_is (tokenizer, tokenizer->activeTagName)) {
				consume_character (tokenizer, c);
				tokenizer->tag = create_tag_token (tokenizer, tokenizer->name->str, TRUE);
				g_string_truncate (tokenizer->name, 0);
				return emit_tag_token (tokenizer);
			}

			goto default_case;
		default:
		default_case:
			if (!is_ascii_letter (c)) {
				// Note: The current input character is not consumed; it gets reconsumed in the RCDATA/RAWTEXT state
				// so that, for example, the second '<' in "</styl</style>" can still start the real end tag.
				tokenizer->state = rawText;
				g_string_truncate (tokenizer->name, 0);
				return NULL;
			}

			g_string_append_unichar (tokenizer->name, c);
			break;
		}

		// Note: we save the data in case we hit a parse error and have to emit a data token
		consume_character (tokenizer, c);
		g_string_append_unichar (tokenizer->data, c);
	} while (tokenizer->state == current);

	tokenizer->tag = create_tag_token (tokenizer, tokenizer->name->str, TRUE);
	g_string_truncate (tokenizer->name, 0);

	return NULL;
}

static GMimeHtmlToken *
read_data (GMimeHtmlTokenizer *tokenizer)
{
	gunichar c, next;

	const GMimeReaderByteSet *specials = tokenizer->logger != NULL ? &LoggedDataSpecials : &DataSpecials;

	do {
		gsize startIndex = tokenizer->data->len;

		if (!try_read_data_until (tokenizer, specials, &c)) {
			tokenizer->state = GMIME_HTML_TOKENIZER_STATE_END_OF_FILE;
			break;
		}

		if (_g_mime_html_tree_tracks_characters (tokenizer->tree))
			process_character_data (tokenizer, startIndex);

		switch (c) {
		case '&':
			// Note: Until the tree builder no longer needs to be told about character data, character references need to
			// be decoded (even if the decoded values don't get emitted) in order to tell whether they are whitespace.
			if (tokenizer->decode || tokenizer->logger != NULL ||
			    _g_mime_html_tree_tracks_characters (tokenizer->tree)) {
				tokenizer->state = GMIME_HTML_TOKENIZER_STATE_CHARACTER_REFERENCE_IN_DATA;
				return NULL;
			}

			goto default_case;
		case '<':
			// Note: If the next character cannot begin a tag, a comment or a markup declaration, then the tag open
			// state would just emit the '<' as character data and reconsume the next character in the data state,
			// so avoid emitting a separate data token for each '<' (e.g. "<<<<<<<<").
			if (try_peek (tokenizer, &next) && next != '!' && next != '/' && next != '?' &&
			    !is_ascii_letter (next)) {
				if (tokenizer->logger != NULL) {
					log_issue_at_current_position (tokenizer,
								       GMIME_HTML_COMPLIANCE_VIOLATION_INVALID_FIRST_CHARACTER_OF_TAG_NAME);
				}

				goto default_case;
			}

			tokenizer->state = GMIME_HTML_TOKENIZER_STATE_TAG_OPEN;
			break;
		case '\0':
			// unexpected-null-character parse error, but emit it anyway (only reached when logging)
			log_issue_at_previous_character (tokenizer,
							 GMIME_HTML_COMPLIANCE_VIOLATION_UNEXPECTED_NULL_CHARACTER, c);
			goto default_case;
		default:
		default_case:
			// Note: The special characters are never whitespace.
			if (_g_mime_html_tree_tracks_characters (tokenizer->tree))
				_g_mime_html_tree_character (tokenizer->tree, c);

			g_string_append_unichar (tokenizer->data, c);
			break;
		}
	} while (tokenizer->state == GMIME_HTML_TOKENIZER_STATE_DATA);

	GMimeHtmlToken *token = emit_data_token (tokenizer, tokenizer->decode, FALSE);

	if (token != NULL)
		token->is_data_state = TRUE;

	return token;
}

static GMimeHtmlToken *
read_character_reference_in_data (GMimeHtmlTokenizer *tokenizer)
{
	return read_character_reference (tokenizer, GMIME_HTML_TOKENIZER_STATE_DATA);
}

static GMimeHtmlToken *
read_rc_data (GMimeHtmlTokenizer *tokenizer)
{
	gunichar c;

	do {
		if (!try_read_data_until (tokenizer, &RcDataSpecials, &c)) {
			tokenizer->state = GMIME_HTML_TOKENIZER_STATE_END_OF_FILE;
			break;
		}

		switch (c) {
		case '&':
			if (tokenizer->decode || tokenizer->logger != NULL) {
				tokenizer->state = GMIME_HTML_TOKENIZER_STATE_CHARACTER_REFERENCE_IN_RC_DATA;
				return NULL;
			}

			goto default_case;
		case '<':
			tokenizer->state = GMIME_HTML_TOKENIZER_STATE_RC_DATA_LESS_THAN;
			return emit_data_token (tokenizer, tokenizer->decode, FALSE);
		case '\0':
			log_null_character (tokenizer);
			g_string_append_unichar (tokenizer->data, 0xfffd);
			break;
		default:
		default_case:
			g_string_append_unichar (tokenizer->data, c);
			break;
		}
	} while (tokenizer->state == GMIME_HTML_TOKENIZER_STATE_RC_DATA);

	return emit_data_token (tokenizer, tokenizer->decode, FALSE);
}

static GMimeHtmlToken *
read_character_reference_in_rc_data (GMimeHtmlTokenizer *tokenizer)
{
	return read_character_reference (tokenizer, GMIME_HTML_TOKENIZER_STATE_RC_DATA);
}

static GMimeHtmlToken *
read_raw_text (GMimeHtmlTokenizer *tokenizer)
{
	gunichar c;

	do {
		if (!try_read_data_until (tokenizer, &RawTextSpecials, &c)) {
			tokenizer->state = GMIME_HTML_TOKENIZER_STATE_END_OF_FILE;
			break;
		}

		switch (c) {
		case '<':
			tokenizer->state = GMIME_HTML_TOKENIZER_STATE_RAW_TEXT_LESS_THAN;
			return emit_data_token (tokenizer, FALSE, FALSE);
		case '\0':
			log_null_character (tokenizer);
			g_string_append_unichar (tokenizer->data, 0xfffd);
			break;
		default:
			g_string_append_unichar (tokenizer->data, c);
			break;
		}
	} while (tokenizer->state == GMIME_HTML_TOKENIZER_STATE_RAW_TEXT);

	return emit_data_token (tokenizer, FALSE, FALSE);
}

static GMimeHtmlToken *
read_script_data (GMimeHtmlTokenizer *tokenizer)
{
	gunichar c;

	do {
		if (!try_read_data_until (tokenizer, &RawTextSpecials, &c)) {
			tokenizer->state = GMIME_HTML_TOKENIZER_STATE_END_OF_FILE;
			break;
		}

		switch (c) {
		case '<':
			tokenizer->state = GMIME_HTML_TOKENIZER_STATE_SCRIPT_DATA_LESS_THAN;
			break;
		case '\0':
			log_null_character (tokenizer);
			g_string_append_unichar (tokenizer->data, 0xfffd);
			break;
		default:
			g_string_append_unichar (tokenizer->data, c);
			break;
		}
	} while (tokenizer->state == GMIME_HTML_TOKENIZER_STATE_SCRIPT_DATA);

	return emit_script_data_token (tokenizer);
}

static GMimeHtmlToken *
read_plain_text (GMimeHtmlTokenizer *tokenizer)
{
	gunichar discard;

	while (try_read_data_until (tokenizer, &PlainTextSpecials, &discard)) {
		log_null_character (tokenizer);
		g_string_append_unichar (tokenizer->data, 0xfffd);
	}

	tokenizer->state = GMIME_HTML_TOKENIZER_STATE_END_OF_FILE;

	// Note: If the depth limit was exceeded, then the remaining input is not the content of a <plaintext> element and
	// so needs to be encoded when written as HTML.
	return emit_data_token (tokenizer, tokenizer->tree->depth_exceeded, FALSE);
}

static GMimeHtmlToken *
read_tag_open (GMimeHtmlTokenizer *tokenizer)
{
	gunichar c;

	if (!try_peek (tokenizer, &c)) {
		// eof-before-tag-name parse error: emit '<' as character data
		if (tokenizer->logger != NULL) {
			log_issue_at_current_position (tokenizer, GMIME_HTML_COMPLIANCE_VIOLATION_EOF_BEFORE_TAG_NAME);
		}

		tokenizer->state = GMIME_HTML_TOKENIZER_STATE_END_OF_FILE;
		g_string_append_unichar (tokenizer->data, '<');
		return emit_data_token (tokenizer, TRUE, FALSE);
	}

	// Note: we save the data in case we hit a parse error and have to emit a data token
	g_string_append_unichar (tokenizer->data, '<');

	switch (c) {
	case '!':
		tokenizer->state = GMIME_HTML_TOKENIZER_STATE_MARKUP_DECLARATION_OPEN;
		consume_character (tokenizer, c);
		g_string_append_unichar (tokenizer->data, c);
		break;
	case '?':
		if (tokenizer->logger != NULL) {
			log_issue_at_current_position (tokenizer,
						       GMIME_HTML_COMPLIANCE_VIOLATION_UNEXPECTED_QUESTION_MARK_INSTEAD_OF_TAG_NAME);
		}

		tokenizer->state = GMIME_HTML_TOKENIZER_STATE_BOGUS_COMMENT;
		consume_character (tokenizer, c);
		g_string_truncate (tokenizer->data, 1);
		tokenizer->data->str[0] = c;
		break;
	case '/':
		tokenizer->state = GMIME_HTML_TOKENIZER_STATE_END_TAG_OPEN;
		consume_character (tokenizer, c);
		g_string_append_unichar (tokenizer->data, c);
		break;
	default:
		if (is_ascii_letter (c)) {
			tokenizer->state = GMIME_HTML_TOKENIZER_STATE_TAG_NAME;
			consume_character (tokenizer, c);
			g_string_append_unichar (tokenizer->data, c);
			tokenizer->isEndTag = FALSE;
			g_string_append_unichar (tokenizer->name, c);
		} else {
			// invalid-first-character-of-tag-name parse error: emit the '<' as character data and
			// reconsume the current input character in the data state (e.g. the second '<' in "<<img>"
			// must still be able to start a tag).
			if (tokenizer->logger != NULL) {
				log_issue_at_current_position (tokenizer,
							       GMIME_HTML_COMPLIANCE_VIOLATION_INVALID_FIRST_CHARACTER_OF_TAG_NAME);
			}

			tokenizer->state = GMIME_HTML_TOKENIZER_STATE_DATA;
		}

		break;
	}

	return NULL;
}

static GMimeHtmlToken *
read_end_tag_open (GMimeHtmlTokenizer *tokenizer)
{
	gunichar c;

	if (!try_read (tokenizer, &c)) {
		// eof-before-tag-name parse error: emit "</" as character data
		if (tokenizer->logger != NULL) {
			log_issue_at_current_position (tokenizer, GMIME_HTML_COMPLIANCE_VIOLATION_EOF_BEFORE_TAG_NAME);
		}

		tokenizer->state = GMIME_HTML_TOKENIZER_STATE_END_OF_FILE;
		return emit_data_token (tokenizer, TRUE, FALSE);
	}

	// Note: we save the data in case we hit a parse error and have to emit a data token
	g_string_append_unichar (tokenizer->data, c);

	switch (c) {
	case '>': // missing-end-tag-name parse error
		if (tokenizer->logger != NULL) {
			log_issue_at_previous_character (tokenizer,
							 GMIME_HTML_COMPLIANCE_VIOLATION_MISSING_END_TAG_NAME, c);
		}

		// Note: per the HTML5 spec, "</>" is simply dropped (nothing is emitted).
		tokenizer->state = GMIME_HTML_TOKENIZER_STATE_DATA;
		g_string_truncate (tokenizer->data, 0);
		break;
	default:
		if (is_ascii_letter (c)) {
			tokenizer->state = GMIME_HTML_TOKENIZER_STATE_TAG_NAME;
			tokenizer->isEndTag = TRUE;
			g_string_append_unichar (tokenizer->name, c);
		} else {
			// invalid-first-character-of-tag-name parse error
			if (tokenizer->logger != NULL) {
				log_issue_at_previous_character (tokenizer,
								 GMIME_HTML_COMPLIANCE_VIOLATION_INVALID_FIRST_CHARACTER_OF_TAG_NAME,
								 c);
			}

			tokenizer->state = GMIME_HTML_TOKENIZER_STATE_BOGUS_COMMENT;
			g_string_truncate (tokenizer->data, 0);
			g_string_append_unichar (tokenizer->data, c == '\0' ? 0xfffd : c);
		}

		break;
	}

	return NULL;
}

static GMimeHtmlToken *
read_tag_name (GMimeHtmlTokenizer *tokenizer)
{
	gunichar c;

	do {
		if (!try_read (tokenizer, &c)) {
			tokenizer->state = GMIME_HTML_TOKENIZER_STATE_END_OF_FILE;
			g_string_truncate (tokenizer->name, 0);

			return emit_eof_in_tag (tokenizer);
		}

		// Note: we save the data in case we hit a parse error and have to emit a data token
		g_string_append_unichar (tokenizer->data, c);

		switch (c) {
		case '\t':
		case '\r':
		case '\n':
		case '\f':
		case ' ':
			tokenizer->state = GMIME_HTML_TOKENIZER_STATE_BEFORE_ATTRIBUTE_NAME;
			break;
		case '/':
			tokenizer->state = GMIME_HTML_TOKENIZER_STATE_SELF_CLOSING_START_TAG;
			break;
		case '>':
			tokenizer->tag = create_tag_token (tokenizer, tokenizer->name->str, tokenizer->isEndTag);
			g_string_truncate (tokenizer->data, 0);
			g_string_truncate (tokenizer->name, 0);

			return emit_tag_token (tokenizer);
		case '\0':
			log_null_character (tokenizer);
			g_string_append_unichar (tokenizer->name, 0xfffd);
			break;
		default:
			g_string_append_unichar (tokenizer->name, c);
			break;
		}
	} while (tokenizer->state == GMIME_HTML_TOKENIZER_STATE_TAG_NAME);

	tokenizer->tag = create_tag_token (tokenizer, tokenizer->name->str, tokenizer->isEndTag);
	g_string_truncate (tokenizer->name, 0);

	return NULL;
}

static GMimeHtmlToken *
read_rc_data_less_than (GMimeHtmlTokenizer *tokenizer)
{
	return read_generic_raw_text_less_than (tokenizer, GMIME_HTML_TOKENIZER_STATE_RC_DATA,
						GMIME_HTML_TOKENIZER_STATE_RC_DATA_END_TAG_OPEN);
}

static GMimeHtmlToken *
read_rc_data_end_tag_open (GMimeHtmlTokenizer *tokenizer)
{
	return read_generic_raw_text_end_tag_open (tokenizer, tokenizer->decode, GMIME_HTML_TOKENIZER_STATE_RC_DATA,
						   GMIME_HTML_TOKENIZER_STATE_RC_DATA_END_TAG_NAME);
}

static GMimeHtmlToken *
read_rc_data_end_tag_name (GMimeHtmlTokenizer *tokenizer)
{
	return read_generic_raw_text_end_tag_name (tokenizer, tokenizer->decode, GMIME_HTML_TOKENIZER_STATE_RC_DATA);
}

static GMimeHtmlToken *
read_raw_text_less_than (GMimeHtmlTokenizer *tokenizer)
{
	return read_generic_raw_text_less_than (tokenizer, GMIME_HTML_TOKENIZER_STATE_RAW_TEXT,
						GMIME_HTML_TOKENIZER_STATE_RAW_TEXT_END_TAG_OPEN);
}

static GMimeHtmlToken *
read_raw_text_end_tag_open (GMimeHtmlTokenizer *tokenizer)
{
	return read_generic_raw_text_end_tag_open (tokenizer, FALSE, GMIME_HTML_TOKENIZER_STATE_RAW_TEXT,
						   GMIME_HTML_TOKENIZER_STATE_RAW_TEXT_END_TAG_NAME);
}

static GMimeHtmlToken *
read_raw_text_end_tag_name (GMimeHtmlTokenizer *tokenizer)
{
	return read_generic_raw_text_end_tag_name (tokenizer, FALSE, GMIME_HTML_TOKENIZER_STATE_RAW_TEXT);
}

static GMimeHtmlToken *
read_script_data_less_than (GMimeHtmlTokenizer *tokenizer)
{
	gunichar c;

	g_string_append_unichar (tokenizer->data, '<');

	if (try_peek (tokenizer, &c) && c == '/') {
		tokenizer->state = GMIME_HTML_TOKENIZER_STATE_SCRIPT_DATA_END_TAG_OPEN;
		consume_character (tokenizer, c);
		g_string_append_unichar (tokenizer->data, '/');
		g_string_truncate (tokenizer->name, 0);
	} else if (c == '!') {
		tokenizer->state = GMIME_HTML_TOKENIZER_STATE_SCRIPT_DATA_ESCAPE_START;
		consume_character (tokenizer, c);
		g_string_append_unichar (tokenizer->data, '!');
	} else {
		tokenizer->state = GMIME_HTML_TOKENIZER_STATE_SCRIPT_DATA;
	}

	return NULL;
}

static GMimeHtmlToken *
read_script_data_end_tag_open (GMimeHtmlTokenizer *tokenizer)
{
	gunichar c;

	if (!try_peek (tokenizer, &c)) {
		tokenizer->state = GMIME_HTML_TOKENIZER_STATE_END_OF_FILE;
		return emit_script_data_token (tokenizer);
	}

	if (c == 'S' || c == 's') {
		tokenizer->state = GMIME_HTML_TOKENIZER_STATE_SCRIPT_DATA_END_TAG_NAME;
		consume_character (tokenizer, c);
		g_string_append_unichar (tokenizer->name, 's');
		g_string_append_unichar (tokenizer->data, c);
	} else {
		tokenizer->state = GMIME_HTML_TOKENIZER_STATE_SCRIPT_DATA;
	}

	return NULL;
}

static GMimeHtmlToken *
read_script_data_end_tag_name (GMimeHtmlTokenizer *tokenizer)
{
	gunichar c;

	do {
		if (!try_peek (tokenizer, &c)) {
			tokenizer->state = GMIME_HTML_TOKENIZER_STATE_END_OF_FILE;
			g_string_truncate (tokenizer->name, 0);

			return emit_script_data_token (tokenizer);
		}

		switch (c) {
		case '\t':
		case '\r':
		case '\n':
		case '\f':
		case ' ':
			if (name_is (tokenizer, "script")) {
				tokenizer->state = GMIME_HTML_TOKENIZER_STATE_BEFORE_ATTRIBUTE_NAME;
				break;
			}

			goto default_case;
		case '/':
			if (name_is (tokenizer, "script")) {
				tokenizer->state = GMIME_HTML_TOKENIZER_STATE_SELF_CLOSING_START_TAG;
				break;
			}

			goto default_case;
		case '>':
			if (name_is (tokenizer, "script")) {
				consume_character (tokenizer, c);
				tokenizer->tag = create_tag_token (tokenizer, tokenizer->name->str, TRUE);
				g_string_truncate (tokenizer->name, 0);
				return emit_tag_token (tokenizer);
			}

			goto default_case;
		default:
		default_case:
			if (!is_ascii_letter (c)) {
				// Note: The current input character is reconsumed in the script data state.
				tokenizer->state = GMIME_HTML_TOKENIZER_STATE_SCRIPT_DATA;
				g_string_truncate (tokenizer->name, 0);
				return NULL;
			}

			g_string_append_unichar (tokenizer->name, c);
			break;
		}

		// Note: we save the data in case we hit a parse error and have to emit a data token
		consume_character (tokenizer, c);
		g_string_append_unichar (tokenizer->data, c);
	} while (tokenizer->state == GMIME_HTML_TOKENIZER_STATE_SCRIPT_DATA_END_TAG_NAME);

	tokenizer->tag = create_tag_token (tokenizer, tokenizer->name->str, TRUE);
	g_string_truncate (tokenizer->name, 0);

	return NULL;
}

static GMimeHtmlToken *
read_script_data_escape_start (GMimeHtmlTokenizer *tokenizer)
{
	gunichar c;

	if (try_peek (tokenizer, &c) && c == '-') {
		tokenizer->state = GMIME_HTML_TOKENIZER_STATE_SCRIPT_DATA_ESCAPE_START_DASH;
		consume_character (tokenizer, c);
		g_string_append_unichar (tokenizer->data, '-');
	} else {
		tokenizer->state = GMIME_HTML_TOKENIZER_STATE_SCRIPT_DATA;
	}

	return NULL;
}

static GMimeHtmlToken *
read_script_data_escape_start_dash (GMimeHtmlTokenizer *tokenizer)
{
	gunichar c;

	if (try_peek (tokenizer, &c) && c == '-') {
		tokenizer->state = GMIME_HTML_TOKENIZER_STATE_SCRIPT_DATA_ESCAPED_DASH_DASH;
		consume_character (tokenizer, c);
		g_string_append_unichar (tokenizer->data, '-');
	} else {
		tokenizer->state = GMIME_HTML_TOKENIZER_STATE_SCRIPT_DATA;
	}

	return NULL;
}

static GMimeHtmlToken *
read_script_data_escaped (GMimeHtmlTokenizer *tokenizer)
{
	gunichar c;

	GMimeHtmlToken *token = NULL;

	do {
		if (!try_read (tokenizer, &c)) {
			log_end_of_file (tokenizer,
					 GMIME_HTML_COMPLIANCE_VIOLATION_EOF_IN_SCRIPT_HTML_COMMENT_LIKE_TEXT);
			tokenizer->state = GMIME_HTML_TOKENIZER_STATE_END_OF_FILE;
			return emit_script_data_token (tokenizer);
		}

		switch (c) {
		case '-':
			tokenizer->state = GMIME_HTML_TOKENIZER_STATE_SCRIPT_DATA_ESCAPED_DASH;
			g_string_append_unichar (tokenizer->data, '-');
			break;
		case '<':
			tokenizer->state = GMIME_HTML_TOKENIZER_STATE_SCRIPT_DATA_ESCAPED_LESS_THAN;
			token = emit_script_data_token (tokenizer);
			g_string_append_unichar (tokenizer->data, '<');
			break;
		default:
			if (c == '\0')
				log_null_character (tokenizer);

			g_string_append_unichar (tokenizer->data, c == '\0' ? 0xfffd : c);
			break;
		}
	} while (tokenizer->state == GMIME_HTML_TOKENIZER_STATE_SCRIPT_DATA_ESCAPED);

	return token;
}

static GMimeHtmlToken *
read_script_data_escaped_dash (GMimeHtmlTokenizer *tokenizer)
{
	gunichar c;

	if (!try_read (tokenizer, &c)) {
		log_end_of_file (tokenizer, GMIME_HTML_COMPLIANCE_VIOLATION_EOF_IN_SCRIPT_HTML_COMMENT_LIKE_TEXT);
		tokenizer->state = GMIME_HTML_TOKENIZER_STATE_END_OF_FILE;
		return emit_script_data_token (tokenizer);
	}

	GMimeHtmlToken *token = NULL;

	switch (c) {
	case '-':
		tokenizer->state = GMIME_HTML_TOKENIZER_STATE_SCRIPT_DATA_ESCAPED_DASH_DASH;
		g_string_append_unichar (tokenizer->data, '-');
		break;
	case '<':
		tokenizer->state = GMIME_HTML_TOKENIZER_STATE_SCRIPT_DATA_ESCAPED_LESS_THAN;
		token = emit_script_data_token (tokenizer);
		g_string_append_unichar (tokenizer->data, '<');
		break;
	default:
		tokenizer->state = GMIME_HTML_TOKENIZER_STATE_SCRIPT_DATA_ESCAPED;
		if (c == '\0')
			log_null_character (tokenizer);

		g_string_append_unichar (tokenizer->data, c == '\0' ? 0xfffd : c);
		break;
	}

	return token;
}

static GMimeHtmlToken *
read_script_data_escaped_dash_dash (GMimeHtmlTokenizer *tokenizer)
{
	gunichar c;

	GMimeHtmlToken *token = NULL;

	do {
		if (!try_read (tokenizer, &c)) {
			log_end_of_file (tokenizer,
					 GMIME_HTML_COMPLIANCE_VIOLATION_EOF_IN_SCRIPT_HTML_COMMENT_LIKE_TEXT);
			tokenizer->state = GMIME_HTML_TOKENIZER_STATE_END_OF_FILE;
			return emit_script_data_token (tokenizer);
		}

		switch (c) {
		case '-':
			g_string_append_unichar (tokenizer->data, '-');
			break;
		case '<':
			tokenizer->state = GMIME_HTML_TOKENIZER_STATE_SCRIPT_DATA_ESCAPED_LESS_THAN;
			token = emit_script_data_token (tokenizer);
			g_string_append_unichar (tokenizer->data, '<');
			break;
		case '>':
			tokenizer->state = GMIME_HTML_TOKENIZER_STATE_SCRIPT_DATA;
			g_string_append_unichar (tokenizer->data, '>');
			break;
		default:
			tokenizer->state = GMIME_HTML_TOKENIZER_STATE_SCRIPT_DATA_ESCAPED;
			if (c == '\0')
				log_null_character (tokenizer);

			g_string_append_unichar (tokenizer->data, c == '\0' ? 0xfffd : c);
			break;
		}
	} while (tokenizer->state == GMIME_HTML_TOKENIZER_STATE_SCRIPT_DATA_ESCAPED_DASH_DASH);

	return token;
}

static GMimeHtmlToken *
read_script_data_escaped_less_than (GMimeHtmlTokenizer *tokenizer)
{
	gunichar c;

	if (!try_peek (tokenizer, &c)) {
		tokenizer->state = GMIME_HTML_TOKENIZER_STATE_SCRIPT_DATA_ESCAPED;
		return NULL;
	}

	if (c == '/') {
		tokenizer->state = GMIME_HTML_TOKENIZER_STATE_SCRIPT_DATA_ESCAPED_END_TAG_OPEN;
		consume_character (tokenizer, c);
		g_string_append_unichar (tokenizer->data, c);
		g_string_truncate (tokenizer->name, 0);
	} else if (is_ascii_letter (c)) {
		tokenizer->state = GMIME_HTML_TOKENIZER_STATE_SCRIPT_DATA_DOUBLE_ESCAPE_START;
		consume_character (tokenizer, c);
		g_string_append_unichar (tokenizer->data, c);
		g_string_truncate (tokenizer->name, 0);
		g_string_append_unichar (tokenizer->name, c);
	} else {
		tokenizer->state = GMIME_HTML_TOKENIZER_STATE_SCRIPT_DATA_ESCAPED;
	}

	return NULL;
}

static GMimeHtmlToken *
read_script_data_escaped_end_tag_open (GMimeHtmlTokenizer *tokenizer)
{
	gunichar c;

	if (!try_peek (tokenizer, &c)) {
		log_end_of_file (tokenizer, GMIME_HTML_COMPLIANCE_VIOLATION_EOF_IN_SCRIPT_HTML_COMMENT_LIKE_TEXT);
		tokenizer->state = GMIME_HTML_TOKENIZER_STATE_END_OF_FILE;
		return emit_script_data_token (tokenizer);
	}

	if (is_ascii_letter (c)) {
		tokenizer->state = GMIME_HTML_TOKENIZER_STATE_SCRIPT_DATA_ESCAPED_END_TAG_NAME;
		consume_character (tokenizer, c);
		g_string_append_unichar (tokenizer->data, c);
		g_string_truncate (tokenizer->name, 0);
		g_string_append_unichar (tokenizer->name, c);
	} else {
		tokenizer->state = GMIME_HTML_TOKENIZER_STATE_SCRIPT_DATA_ESCAPED;
	}

	return NULL;
}

static GMimeHtmlToken *
read_script_data_escaped_end_tag_name (GMimeHtmlTokenizer *tokenizer)
{
	gunichar c;

	do {
		if (!try_peek (tokenizer, &c)) {
			log_end_of_file (tokenizer,
					 GMIME_HTML_COMPLIANCE_VIOLATION_EOF_IN_SCRIPT_HTML_COMMENT_LIKE_TEXT);
			tokenizer->state = GMIME_HTML_TOKENIZER_STATE_END_OF_FILE;
			g_string_truncate (tokenizer->name, 0);

			return emit_script_data_token (tokenizer);
		}

		switch (c) {
		case '\t':
		case '\r':
		case '\n':
		case '\f':
		case ' ':
			if (name_is (tokenizer, "script")) {
				tokenizer->state = GMIME_HTML_TOKENIZER_STATE_BEFORE_ATTRIBUTE_NAME;
				break;
			}

			goto default_case;
		case '/':
			if (name_is (tokenizer, "script")) {
				tokenizer->state = GMIME_HTML_TOKENIZER_STATE_SELF_CLOSING_START_TAG;
				break;
			}

			goto default_case;
		case '>':
			if (name_is (tokenizer, "script")) {
				consume_character (tokenizer, c);
				tokenizer->tag = create_tag_token (tokenizer, tokenizer->name->str, TRUE);
				g_string_truncate (tokenizer->name, 0);
				return emit_tag_token (tokenizer);
			}

			goto default_case;
		default:
		default_case:
			if (!is_ascii_letter (c)) {
				// Note: The current input character is reconsumed in the script data escaped state.
				tokenizer->state = GMIME_HTML_TOKENIZER_STATE_SCRIPT_DATA_ESCAPED;
				g_string_truncate (tokenizer->name, 0);
				return NULL;
			}

			g_string_append_unichar (tokenizer->name, c);
			break;
		}

		// Note: we save the data in case we hit a parse error and have to emit a data token
		consume_character (tokenizer, c);
		g_string_append_unichar (tokenizer->data, c);
	} while (tokenizer->state == GMIME_HTML_TOKENIZER_STATE_SCRIPT_DATA_ESCAPED_END_TAG_NAME);

	tokenizer->tag = create_tag_token (tokenizer, tokenizer->name->str, TRUE);
	g_string_truncate (tokenizer->name, 0);

	return NULL;
}

static GMimeHtmlToken *
read_script_data_double_escape_start (GMimeHtmlTokenizer *tokenizer)
{
	gunichar c;

	do {
		if (!try_peek (tokenizer, &c)) {
			log_end_of_file (tokenizer,
					 GMIME_HTML_COMPLIANCE_VIOLATION_EOF_IN_SCRIPT_HTML_COMMENT_LIKE_TEXT);
			tokenizer->state = GMIME_HTML_TOKENIZER_STATE_END_OF_FILE;
			g_string_truncate (tokenizer->name, 0);

			return emit_script_data_token (tokenizer);
		}

		switch (c) {
		case '\t':
		case '\r':
		case '\n':
		case '\f':
		case ' ':
		case '/':
		case '>':
			if (name_is (tokenizer, "script"))
				tokenizer->state = GMIME_HTML_TOKENIZER_STATE_SCRIPT_DATA_DOUBLE_ESCAPED;
			else
				tokenizer->state = GMIME_HTML_TOKENIZER_STATE_SCRIPT_DATA_ESCAPED;

			g_string_truncate (tokenizer->name, 0);
			break;
		default:
			if (!is_ascii_letter (c)) {
				// Note: The current input character is reconsumed in the script data escaped state.
				tokenizer->state = GMIME_HTML_TOKENIZER_STATE_SCRIPT_DATA_ESCAPED;
				g_string_truncate (tokenizer->name, 0);
				return NULL;
			}

			g_string_append_unichar (tokenizer->name, c);
			break;
		}

		consume_character (tokenizer, c);
		g_string_append_unichar (tokenizer->data, c);
	} while (tokenizer->state == GMIME_HTML_TOKENIZER_STATE_SCRIPT_DATA_DOUBLE_ESCAPE_START);

	return NULL;
}

static GMimeHtmlToken *
read_script_data_double_escaped (GMimeHtmlTokenizer *tokenizer)
{
	gunichar c;

	do {
		if (!try_read (tokenizer, &c)) {
			log_end_of_file (tokenizer,
					 GMIME_HTML_COMPLIANCE_VIOLATION_EOF_IN_SCRIPT_HTML_COMMENT_LIKE_TEXT);
			tokenizer->state = GMIME_HTML_TOKENIZER_STATE_END_OF_FILE;
			return emit_script_data_token (tokenizer);
		}

		switch (c) {
		case '-':
			tokenizer->state = GMIME_HTML_TOKENIZER_STATE_SCRIPT_DATA_DOUBLE_ESCAPED_DASH;
			g_string_append_unichar (tokenizer->data, '-');
			break;
		case '<':
			tokenizer->state = GMIME_HTML_TOKENIZER_STATE_SCRIPT_DATA_DOUBLE_ESCAPED_LESS_THAN;
			g_string_append_unichar (tokenizer->data, '<');
			break;
		default:
			if (c == '\0')
				log_null_character (tokenizer);

			g_string_append_unichar (tokenizer->data, c == '\0' ? 0xfffd : c);
			break;
		}
	} while (tokenizer->state == GMIME_HTML_TOKENIZER_STATE_SCRIPT_DATA_DOUBLE_ESCAPED);

	return NULL;
}

static GMimeHtmlToken *
read_script_data_double_escaped_dash (GMimeHtmlTokenizer *tokenizer)
{
	gunichar c;

	if (!try_read (tokenizer, &c)) {
		log_end_of_file (tokenizer, GMIME_HTML_COMPLIANCE_VIOLATION_EOF_IN_SCRIPT_HTML_COMMENT_LIKE_TEXT);
		tokenizer->state = GMIME_HTML_TOKENIZER_STATE_END_OF_FILE;
		return emit_script_data_token (tokenizer);
	}

	switch (c) {
	case '-':
		tokenizer->state = GMIME_HTML_TOKENIZER_STATE_SCRIPT_DATA_DOUBLE_ESCAPED_DASH_DASH;
		g_string_append_unichar (tokenizer->data, '-');
		break;
	case '<':
		tokenizer->state = GMIME_HTML_TOKENIZER_STATE_SCRIPT_DATA_DOUBLE_ESCAPED_LESS_THAN;
		g_string_append_unichar (tokenizer->data, '<');
		break;
	default:
		tokenizer->state = GMIME_HTML_TOKENIZER_STATE_SCRIPT_DATA_DOUBLE_ESCAPED;
		if (c == '\0')
			log_null_character (tokenizer);

		g_string_append_unichar (tokenizer->data, c == '\0' ? 0xfffd : c);
		break;
	}

	return NULL;
}

static GMimeHtmlToken *
read_script_data_double_escaped_dash_dash (GMimeHtmlTokenizer *tokenizer)
{
	gunichar c;

	do {
		if (!try_read (tokenizer, &c)) {
			log_end_of_file (tokenizer,
					 GMIME_HTML_COMPLIANCE_VIOLATION_EOF_IN_SCRIPT_HTML_COMMENT_LIKE_TEXT);
			tokenizer->state = GMIME_HTML_TOKENIZER_STATE_END_OF_FILE;
			return emit_script_data_token (tokenizer);
		}

		switch (c) {
		case '-':
			g_string_append_unichar (tokenizer->data, '-');
			break;
		case '<':
			tokenizer->state = GMIME_HTML_TOKENIZER_STATE_SCRIPT_DATA_DOUBLE_ESCAPED_LESS_THAN;
			g_string_append_unichar (tokenizer->data, '<');
			break;
		case '>':
			tokenizer->state = GMIME_HTML_TOKENIZER_STATE_SCRIPT_DATA;
			g_string_append_unichar (tokenizer->data, '>');
			break;
		default:
			tokenizer->state = GMIME_HTML_TOKENIZER_STATE_SCRIPT_DATA_DOUBLE_ESCAPED;
			if (c == '\0')
				log_null_character (tokenizer);

			g_string_append_unichar (tokenizer->data, c == '\0' ? 0xfffd : c);
			break;
		}
	} while (tokenizer->state == GMIME_HTML_TOKENIZER_STATE_SCRIPT_DATA_DOUBLE_ESCAPED_DASH_DASH);

	return NULL;
}

static GMimeHtmlToken *
read_script_data_double_escaped_less_than (GMimeHtmlTokenizer *tokenizer)
{
	gunichar c;

	if (try_peek (tokenizer, &c) && c == '/') {
		tokenizer->state = GMIME_HTML_TOKENIZER_STATE_SCRIPT_DATA_DOUBLE_ESCAPE_END;
		consume_character (tokenizer, c);
		g_string_append_unichar (tokenizer->data, '/');
		g_string_truncate (tokenizer->name, 0);
	} else {
		tokenizer->state = GMIME_HTML_TOKENIZER_STATE_SCRIPT_DATA_DOUBLE_ESCAPED;
	}

	return NULL;
}

static GMimeHtmlToken *
read_script_data_double_escape_end (GMimeHtmlTokenizer *tokenizer)
{
	gunichar c;

	do {
		try_peek (tokenizer, &c);

		switch (c) {
		case '\t':
		case '\r':
		case '\n':
		case '\f':
		case ' ':
		case '/':
		case '>':
			if (name_is (tokenizer, "script"))
				tokenizer->state = GMIME_HTML_TOKENIZER_STATE_SCRIPT_DATA_ESCAPED;
			else
				tokenizer->state = GMIME_HTML_TOKENIZER_STATE_SCRIPT_DATA_DOUBLE_ESCAPED;

			consume_character (tokenizer, c);
			g_string_append_unichar (tokenizer->data, c);
			g_string_truncate (tokenizer->name, 0);
			break;
		default:
			if (!is_ascii_letter (c)) {
				// Note: EOF also hits this case.
				tokenizer->state = GMIME_HTML_TOKENIZER_STATE_SCRIPT_DATA_DOUBLE_ESCAPED;
				g_string_truncate (tokenizer->name, 0);
			} else {
				consume_character (tokenizer, c);
				g_string_append_unichar (tokenizer->name, c);
				g_string_append_unichar (tokenizer->data, c);
			}

			break;
		}
	} while (tokenizer->state == GMIME_HTML_TOKENIZER_STATE_SCRIPT_DATA_DOUBLE_ESCAPE_END);

	return NULL;
}

static GMimeHtmlToken *
read_before_attribute_name (GMimeHtmlTokenizer *tokenizer)
{
	gunichar c;

	do {
		if (!try_read (tokenizer, &c)) {
			tokenizer->state = GMIME_HTML_TOKENIZER_STATE_END_OF_FILE;
			g_clear_pointer (&tokenizer->tag, g_mime_html_token_unref);

			return emit_eof_in_tag (tokenizer);
		}

		// Note: we save the data in case we hit a parse error and have to emit a data token
		g_string_append_unichar (tokenizer->data, c);

		switch (c) {
		case '\t':
		case '\r':
		case '\n':
		case '\f':
		case ' ':
			break;
		case '/':
			tokenizer->state = GMIME_HTML_TOKENIZER_STATE_SELF_CLOSING_START_TAG;
			return NULL;
		case '>':
			return emit_tag_token (tokenizer);
		case '=':
			if (tokenizer->logger != NULL) {
				log_issue_at_previous_character (tokenizer,
								 GMIME_HTML_COMPLIANCE_VIOLATION_UNEXPECTED_EQUALS_SIGN_BEFORE_ATTRIBUTE_NAME,
								 c);
			}

			goto default_case;
		case '"':
		case '\'':
		case '<':
			if (tokenizer->logger != NULL) {
				log_issue_at_previous_character (tokenizer,
								 GMIME_HTML_COMPLIANCE_VIOLATION_UNEXPECTED_CHARACTER_IN_ATTRIBUTE_NAME,
								 c);
			}

			goto default_case;
		case '\0':
			log_null_character (tokenizer);
			goto default_case;
		default:
		default_case:
			if (tokenizer->logger != NULL)
				mark_previous_character (tokenizer);

			tokenizer->state = GMIME_HTML_TOKENIZER_STATE_ATTRIBUTE_NAME;
			g_string_append_unichar (tokenizer->name, c == '\0' ? 0xfffd : c);
			return NULL;
		}
	} while (TRUE);
}

static GMimeHtmlToken *
read_attribute_name (GMimeHtmlTokenizer *tokenizer)
{
	gunichar c;

	do {
		if (!try_read (tokenizer, &c)) {
			tokenizer->state = GMIME_HTML_TOKENIZER_STATE_END_OF_FILE;
			g_string_truncate (tokenizer->name, 0);
			g_clear_pointer (&tokenizer->tag, g_mime_html_token_unref);

			return emit_eof_in_tag (tokenizer);
		}

		// Note: we save the data in case we hit a parse error and have to emit a data token
		g_string_append_unichar (tokenizer->data, c);

		switch (c) {
		case '\t':
		case '\r':
		case '\n':
		case '\f':
		case ' ':
			tokenizer->state = GMIME_HTML_TOKENIZER_STATE_AFTER_ATTRIBUTE_NAME;
			break;
		case '/':
			tokenizer->state = GMIME_HTML_TOKENIZER_STATE_SELF_CLOSING_START_TAG;
			break;
		case '=':
			tokenizer->state = GMIME_HTML_TOKENIZER_STATE_BEFORE_ATTRIBUTE_VALUE;
			break;
		case '>':
			emit_tag_attribute (tokenizer);

			return emit_tag_token (tokenizer);
		case '"':
		case '\'':
		case '<':
			if (tokenizer->logger != NULL) {
				log_issue_at_previous_character (tokenizer,
								 GMIME_HTML_COMPLIANCE_VIOLATION_UNEXPECTED_CHARACTER_IN_ATTRIBUTE_NAME,
								 c);
			}

			goto default_case;
		case '\0':
			log_null_character (tokenizer);
			goto default_case;
		default:
		default_case:
			g_string_append_unichar (tokenizer->name, c == '\0' ? 0xfffd : c);
			break;
		}
	} while (tokenizer->state == GMIME_HTML_TOKENIZER_STATE_ATTRIBUTE_NAME);

	emit_tag_attribute (tokenizer);

	return NULL;
}

static GMimeHtmlToken *
read_after_attribute_name (GMimeHtmlTokenizer *tokenizer)
{
	gunichar c;

	do {
		if (!try_read (tokenizer, &c)) {
			tokenizer->state = GMIME_HTML_TOKENIZER_STATE_END_OF_FILE;
			g_clear_pointer (&tokenizer->tag, g_mime_html_token_unref);

			return emit_eof_in_tag (tokenizer);
		}

		// Note: we save the data in case we hit a parse error and have to emit a data token
		g_string_append_unichar (tokenizer->data, c);

		switch (c) {
		case '\t':
		case '\r':
		case '\n':
		case '\f':
		case ' ':
			break;
		case '/':
			tokenizer->state = GMIME_HTML_TOKENIZER_STATE_SELF_CLOSING_START_TAG;
			return NULL;
		case '=':
			tokenizer->state = GMIME_HTML_TOKENIZER_STATE_BEFORE_ATTRIBUTE_VALUE;
			return NULL;
		case '>':
			return emit_tag_token (tokenizer);
		case '"':
		case '\'':
		case '<':
			if (tokenizer->logger != NULL) {
				log_issue_at_previous_character (tokenizer,
								 GMIME_HTML_COMPLIANCE_VIOLATION_UNEXPECTED_CHARACTER_IN_ATTRIBUTE_NAME,
								 c);
			}

			goto default_case;
		case '\0':
			log_null_character (tokenizer);
			goto default_case;
		default:
		default_case:
			if (tokenizer->logger != NULL)
				mark_previous_character (tokenizer);

			tokenizer->state = GMIME_HTML_TOKENIZER_STATE_ATTRIBUTE_NAME;
			g_string_append_unichar (tokenizer->name, c == '\0' ? 0xfffd : c);
			return NULL;
		}
	} while (TRUE);
}

static GMimeHtmlToken *
read_before_attribute_value (GMimeHtmlTokenizer *tokenizer)
{
	gunichar c;

	do {
		if (!try_read (tokenizer, &c)) {
			tokenizer->state = GMIME_HTML_TOKENIZER_STATE_END_OF_FILE;
			g_clear_pointer (&tokenizer->tag, g_mime_html_token_unref);

			return emit_eof_in_tag (tokenizer);
		}

		// Note: we save the data in case we hit a parse error and have to emit a data token
		g_string_append_unichar (tokenizer->data, c);

		switch (c) {
		case '\t':
		case '\r':
		case '\n':
		case '\f':
		case ' ':
			break;
		case '"':
			tokenizer->state = GMIME_HTML_TOKENIZER_STATE_ATTRIBUTE_VALUE_DOUBLE_QUOTED;
			tokenizer->quote = c;
			return NULL;
		case '\'':
			tokenizer->state = GMIME_HTML_TOKENIZER_STATE_ATTRIBUTE_VALUE_SINGLE_QUOTED;
			tokenizer->quote = c;
			return NULL;
		case '&':
			// Note: This is the start of an unquoted attribute value. Make sure that a stale quote character
			// (e.g. from an abruptly terminated DOCTYPE identifier) does not cause the character reference
			// state to resume in the quoted attribute value state.
			tokenizer->state = GMIME_HTML_TOKENIZER_STATE_CHARACTER_REFERENCE_IN_ATTRIBUTE_VALUE;
			tokenizer->quote = '\0';
			return NULL;
		case '>':
			if (tokenizer->logger != NULL) {
				log_issue_at_previous_character (tokenizer,
								 GMIME_HTML_COMPLIANCE_VIOLATION_MISSING_ATTRIBUTE_VALUE,
								 c);
			}

			return emit_tag_token (tokenizer);
		case '<':
		case '=':
		case '`':
			if (tokenizer->logger != NULL) {
				log_issue_at_previous_character (tokenizer,
								 GMIME_HTML_COMPLIANCE_VIOLATION_UNEXPECTED_CHARACTER_IN_UNQUOTED_ATTRIBUTE_VALUE,
								 c);
			}

			goto default_case;
		case '\0':
			log_null_character (tokenizer);
			goto default_case;
		default:
		default_case:
			tokenizer->state = GMIME_HTML_TOKENIZER_STATE_ATTRIBUTE_VALUE_UNQUOTED;
			tokenizer->quote = '\0';
			g_string_append_unichar (tokenizer->name, c == '\0' ? 0xfffd : c);
			return NULL;
		}
	} while (TRUE);
}

static GMimeHtmlToken *
read_attribute_value_double_quoted (GMimeHtmlTokenizer *tokenizer)
{
	gunichar c;

	do {
		if (!try_read_name_until (tokenizer, &AttributeValueDoubleQuotedSpecials, &c)) {
			tokenizer->state = GMIME_HTML_TOKENIZER_STATE_END_OF_FILE;
			g_string_truncate (tokenizer->name, 0);

			return emit_eof_in_tag (tokenizer);
		}

		// Note: we save the data in case we hit a parse error and have to emit a data token
		g_string_append_unichar (tokenizer->data, c);

		switch (c) {
		case '&':
			tokenizer->state = GMIME_HTML_TOKENIZER_STATE_CHARACTER_REFERENCE_IN_ATTRIBUTE_VALUE;
			return NULL;
		case '"':
			tokenizer->state = GMIME_HTML_TOKENIZER_STATE_AFTER_ATTRIBUTE_VALUE_QUOTED;
			tokenizer->quote = '\0';
			break;
		default: // '\0'
			log_null_character (tokenizer);
			g_string_append_unichar (tokenizer->name, 0xfffd);
			break;
		}
	} while (tokenizer->state == GMIME_HTML_TOKENIZER_STATE_ATTRIBUTE_VALUE_DOUBLE_QUOTED);

	set_string (&tokenizer->attribute->value, tokenizer->name->str);
	g_string_truncate (tokenizer->name, 0);

	return NULL;
}

static GMimeHtmlToken *
read_attribute_value_single_quoted (GMimeHtmlTokenizer *tokenizer)
{
	gunichar c;

	do {
		if (!try_read_name_until (tokenizer, &AttributeValueSingleQuotedSpecials, &c)) {
			tokenizer->state = GMIME_HTML_TOKENIZER_STATE_END_OF_FILE;
			g_string_truncate (tokenizer->name, 0);

			return emit_eof_in_tag (tokenizer);
		}

		// Note: we save the data in case we hit a parse error and have to emit a data token
		g_string_append_unichar (tokenizer->data, c);

		switch (c) {
		case '&':
			tokenizer->state = GMIME_HTML_TOKENIZER_STATE_CHARACTER_REFERENCE_IN_ATTRIBUTE_VALUE;
			return NULL;
		case '\'':
			tokenizer->state = GMIME_HTML_TOKENIZER_STATE_AFTER_ATTRIBUTE_VALUE_QUOTED;
			tokenizer->quote = '\0';
			break;
		default: // '\0'
			log_null_character (tokenizer);
			g_string_append_unichar (tokenizer->name, 0xfffd);
			break;
		}
	} while (tokenizer->state == GMIME_HTML_TOKENIZER_STATE_ATTRIBUTE_VALUE_SINGLE_QUOTED);

	set_string (&tokenizer->attribute->value, tokenizer->name->str);
	g_string_truncate (tokenizer->name, 0);

	return NULL;
}

static GMimeHtmlToken *
read_attribute_value_unquoted (GMimeHtmlTokenizer *tokenizer)
{
	gunichar c;

	do {
		if (!try_read (tokenizer, &c)) {
			tokenizer->state = GMIME_HTML_TOKENIZER_STATE_END_OF_FILE;
			g_string_truncate (tokenizer->name, 0);

			return emit_eof_in_tag (tokenizer);
		}

		// Note: we save the data in case we hit a parse error and have to emit a data token
		g_string_append_unichar (tokenizer->data, c);

		switch (c) {
		case '\t':
		case '\r':
		case '\n':
		case '\f':
		case ' ':
			tokenizer->state = GMIME_HTML_TOKENIZER_STATE_BEFORE_ATTRIBUTE_NAME;
			break;
		case '&':
			tokenizer->state = GMIME_HTML_TOKENIZER_STATE_CHARACTER_REFERENCE_IN_ATTRIBUTE_VALUE;
			return NULL;
		case '>':
			set_string (&tokenizer->attribute->value, tokenizer->name->str);
			g_string_truncate (tokenizer->name, 0);

			return emit_tag_token (tokenizer);
		case '"':
		case '\'':
		case '<':
		case '=':
		case '`':
			if (tokenizer->logger != NULL) {
				log_issue_at_previous_character (tokenizer,
								 GMIME_HTML_COMPLIANCE_VIOLATION_UNEXPECTED_CHARACTER_IN_UNQUOTED_ATTRIBUTE_VALUE,
								 c);
			}

			goto default_case;
		case '\0':
			log_null_character (tokenizer);
			goto default_case;
		default:
		default_case:
			g_string_append_unichar (tokenizer->name, c == '\0' ? 0xfffd : c);
			break;
		}
	} while (tokenizer->state == GMIME_HTML_TOKENIZER_STATE_ATTRIBUTE_VALUE_UNQUOTED);

	set_string (&tokenizer->attribute->value, tokenizer->name->str);
	g_string_truncate (tokenizer->name, 0);

	return NULL;
}

static GMimeHtmlToken *
read_character_reference_in_attribute_value (GMimeHtmlTokenizer *tokenizer)
{
	gunichar c;

	gunichar additionalAllowedCharacter = tokenizer->quote == '\0' ? '>' : tokenizer->quote;

	if (!try_peek (tokenizer, &c)) {
		tokenizer->state = GMIME_HTML_TOKENIZER_STATE_END_OF_FILE;
		g_string_truncate (tokenizer->name, 0);

		return emit_eof_in_tag (tokenizer);
	}

	switch (c) {
	case '\t':
	case '\r':
	case '\n':
	case '\f':
	case ' ':
	case '<':
	case '&':
		// no character is consumed, emit '&'
		g_string_append_unichar (tokenizer->name, '&');
		break;
	default:
		if (c == additionalAllowedCharacter) {
			// this is not a character reference, nothing is consumed
			g_string_append_unichar (tokenizer->name, '&');
			break;
		}

		// Note: The '&' has already been consumed.
		if (tokenizer->logger != NULL)
			mark_previous_character (tokenizer);

		_g_mime_html_entity_push (&tokenizer->entity, '&');

		// Note: 'data' already contains the '&' and accumulates the raw text in case the tag is truncated.
		while (_g_mime_html_entity_push (&tokenizer->entity, c)) {
			consume_character (tokenizer, c);
			g_string_append_unichar (tokenizer->data, c);

			if (c == ';')
				break;

			if (!try_peek (tokenizer, &c)) {
				tokenizer->state = GMIME_HTML_TOKENIZER_STATE_END_OF_FILE;
				if (tokenizer->logger != NULL)
					validate_character_reference (tokenizer, TRUE, '\0');

				_g_mime_html_entity_reset (&tokenizer->entity);

				return emit_eof_in_tag (tokenizer);
			}
		}

		int unknown = tokenizer->logger != NULL ? validate_character_reference (tokenizer, TRUE, c) : -1;

		g_string_append (tokenizer->name, _g_mime_html_entity_value (&tokenizer->entity, TRUE, c));
		_g_mime_html_entity_reset (&tokenizer->entity);

		if (unknown != -1)
			read_ambiguous_ampersand (tokenizer, TRUE, unknown);

		break;
	}

	switch (tokenizer->quote) {
	case '"':
		tokenizer->state = GMIME_HTML_TOKENIZER_STATE_ATTRIBUTE_VALUE_DOUBLE_QUOTED;
		break;
	case '\'':
		tokenizer->state = GMIME_HTML_TOKENIZER_STATE_ATTRIBUTE_VALUE_SINGLE_QUOTED;
		break;
	default:
		tokenizer->state = GMIME_HTML_TOKENIZER_STATE_ATTRIBUTE_VALUE_UNQUOTED;
		break;
	}

	return NULL;
}

static GMimeHtmlToken *
read_after_attribute_value_quoted (GMimeHtmlTokenizer *tokenizer)
{
	gunichar c;

	GMimeHtmlToken *token = NULL;

	if (!try_peek (tokenizer, &c)) {
		tokenizer->state = GMIME_HTML_TOKENIZER_STATE_END_OF_FILE;
		return emit_eof_in_tag (tokenizer);
	}

	switch (c) {
	case '\t':
	case '\r':
	case '\n':
	case '\f':
	case ' ':
		tokenizer->state = GMIME_HTML_TOKENIZER_STATE_BEFORE_ATTRIBUTE_NAME;
		consume_character (tokenizer, c);
		g_string_append_unichar (tokenizer->data, c);
		break;
	case '/':
		tokenizer->state = GMIME_HTML_TOKENIZER_STATE_SELF_CLOSING_START_TAG;
		consume_character (tokenizer, c);
		g_string_append_unichar (tokenizer->data, c);
		break;
	case '>':
		consume_character (tokenizer, c);
		token = emit_tag_token (tokenizer);
		break;
	default:
		if (tokenizer->logger != NULL) {
			log_issue_at_current_position (tokenizer,
						       GMIME_HTML_COMPLIANCE_VIOLATION_MISSING_WHITESPACE_BETWEEN_ATTRIBUTES);
		}

		tokenizer->state = GMIME_HTML_TOKENIZER_STATE_BEFORE_ATTRIBUTE_NAME;
		break;
	}

	return token;
}

static GMimeHtmlToken *
read_self_closing_start_tag (GMimeHtmlTokenizer *tokenizer)
{
	gunichar c;

	if (!try_peek (tokenizer, &c)) {
		tokenizer->state = GMIME_HTML_TOKENIZER_STATE_END_OF_FILE;
		return emit_eof_in_tag (tokenizer);
	}

	if (c == '>') {
		consume_character (tokenizer, c);
		tokenizer->tag->is_empty_element = TRUE;

		return emit_tag_token (tokenizer);
	}

	// unexpected-solidus-in-tag parse error: reconsume the current input character in the before attribute
	// name state (e.g. "<img/onerror=...>" has an "onerror" attribute, not a "nerror" attribute).
	if (tokenizer->logger != NULL) {
		log_issue_at_previous_character (tokenizer, GMIME_HTML_COMPLIANCE_VIOLATION_UNEXPECTED_SOLIDUS_IN_TAG,
						 '/');
	}

	tokenizer->state = GMIME_HTML_TOKENIZER_STATE_BEFORE_ATTRIBUTE_NAME;

	return NULL;
}

static GMimeHtmlToken *
read_bogus_comment (GMimeHtmlTokenizer *tokenizer)
{
	gunichar c;

	do {
		if (!try_read_data_until (tokenizer, &BogusCommentSpecials, &c)) {
			tokenizer->state = GMIME_HTML_TOKENIZER_STATE_END_OF_FILE;
			break;
		}

		if (c == '>')
			break;

		if (c == '\0')
			log_null_character (tokenizer);

		g_string_append_unichar (tokenizer->data, c == '\0' ? 0xfffd : c);
	} while (TRUE);

	tokenizer->state = GMIME_HTML_TOKENIZER_STATE_DATA;

	return emit_comment (tokenizer, tokenizer->data->str, tokenizer->data->len, TRUE);
}

static GMimeHtmlToken *
read_markup_declaration_open (GMimeHtmlTokenizer *tokenizer)
{
	gint64 startOffset = tokenizer->position;
	int startLineNumber = tokenizer->lineNumber;
	int startLinePosition = tokenizer->linePosition;
	gboolean cdataInHtml = FALSE;
	int count = 0;
	gunichar c = '\0';

	while (count < 2) {
		if (!try_peek (tokenizer, &c))
			break;

		if (c != '-')
			break;

		// Note: we save the data in case we hit a parse error and have to emit a data token
		consume_character (tokenizer, c);
		g_string_append_unichar (tokenizer->data, c);
		count++;
	}

	if (count == 2) {
		// "<!--"
		tokenizer->state = GMIME_HTML_TOKENIZER_STATE_COMMENT_START;
		g_string_truncate (tokenizer->name, 0);
		return NULL;
	}

	if (count == 0) {
		// Check for "<!DOCTYPE " or "<![CDATA["
		//
		// Note: Only the characters that match the keyword are consumed. If a character does not match, it is not
		// consumed so that it gets reconsumed in the bogus comment state. This is important because the mismatched
		// character might be the '>' that terminates the bogus comment (e.g. "<!DOC>").
		if (c == 'D' || c == 'd') {
			// Note: we save the data in case we hit a parse error and have to emit a data token
			consume_character (tokenizer, c);
			g_string_append_unichar (tokenizer->data, c);
			g_string_append_unichar (tokenizer->name, c);
			count = 1;

			while (count < 7) {
				if (!try_peek (tokenizer, &c))
					break;

				if (to_lower (c) != (guchar) "doctype"[count])
					break;

				// Note: we save the data in case we hit a parse error and have to emit a data token
				consume_character (tokenizer, c);
				g_string_append_unichar (tokenizer->data, c);
				g_string_append_unichar (tokenizer->name, c);
				count++;
			}

			if (count == 7) {
				tokenizer->doctype = create_doc_type_token (tokenizer, tokenizer->name->str);
				tokenizer->state = GMIME_HTML_TOKENIZER_STATE_DOC_TYPE;
				g_string_truncate (tokenizer->name, 0);
				return NULL;
			}

			g_string_truncate (tokenizer->name, 0);
		} else if (c == '[') {
			// Note: we save the data in case we hit a parse error and have to emit a data token
			consume_character (tokenizer, c);
			g_string_append_unichar (tokenizer->data, c);
			count = 1;

			while (count < 7) {
				if (!try_peek (tokenizer, &c))
					break;

				if (c != (guchar) "[CDATA["[count])
					break;

				// Note: we save the data in case we hit a parse error and have to emit a data token
				consume_character (tokenizer, c);
				g_string_append_unichar (tokenizer->data, c);
				count++;
			}

			// Note: CDATA sections are only allowed in SVG and MathML content. Elsewhere, "<![CDATA[" starts a bogus comment.
			if (count == 7 && _g_mime_html_tree_foreign (tokenizer->tree)) {
				tokenizer->state = GMIME_HTML_TOKENIZER_STATE_C_DATA_SECTION;
				g_string_truncate (tokenizer->data, 0);
				return NULL;
			}

			cdataInHtml = count == 7;
		}
	}

	if (tokenizer->logger != NULL) {
		log_issue (tokenizer,
			   cdataInHtml ? GMIME_HTML_COMPLIANCE_VIOLATION_C_DATA_IN_HTML_CONTENT :
					 GMIME_HTML_COMPLIANCE_VIOLATION_INCORRECTLY_OPENED_COMMENT,
			   startOffset, startLineNumber, startLinePosition);
	}

	tokenizer->state = GMIME_HTML_TOKENIZER_STATE_BOGUS_COMMENT;

	// trim the leading "<!"
	for (gsize i = 0; i < tokenizer->data->len - 2; i++)
		tokenizer->data->str[i] = tokenizer->data->str[i + 2];

	g_string_truncate (tokenizer->data, tokenizer->data->len - 2);
	tokenizer->bang = TRUE;

	return NULL;
}

static GMimeHtmlToken *
read_comment_start (GMimeHtmlTokenizer *tokenizer)
{
	gunichar c;

	if (!try_read (tokenizer, &c)) {
		log_end_of_file (tokenizer, GMIME_HTML_COMPLIANCE_VIOLATION_EOF_IN_COMMENT);
		tokenizer->state = GMIME_HTML_TOKENIZER_STATE_DATA;

		return emit_comment (tokenizer, "", 0, FALSE);
	}

	g_string_append_unichar (tokenizer->data, c);

	switch (c) {
	case '-':
		tokenizer->state = GMIME_HTML_TOKENIZER_STATE_COMMENT_START_DASH;
		break;
	case '>':
		if (tokenizer->logger != NULL) {
			log_issue_at_previous_character (tokenizer,
							 GMIME_HTML_COMPLIANCE_VIOLATION_ABRUPT_CLOSING_OF_EMPTY_COMMENT,
							 c);
		}

		tokenizer->state = GMIME_HTML_TOKENIZER_STATE_DATA;
		return emit_comment (tokenizer, "", 0, FALSE);
	default:
		tokenizer->state = GMIME_HTML_TOKENIZER_STATE_COMMENT;
		if (c == '\0')
			log_null_character (tokenizer);

		g_string_append_unichar (tokenizer->name, c == '\0' ? 0xfffd : c);
		break;
	}

	return NULL;
}

static GMimeHtmlToken *
read_comment_start_dash (GMimeHtmlTokenizer *tokenizer)
{
	gunichar c;

	if (!try_read (tokenizer, &c)) {
		log_end_of_file (tokenizer, GMIME_HTML_COMPLIANCE_VIOLATION_EOF_IN_COMMENT);
		tokenizer->state = GMIME_HTML_TOKENIZER_STATE_DATA;
		return emit_comment (tokenizer, tokenizer->name->str, tokenizer->name->len, FALSE);
	}

	g_string_append_unichar (tokenizer->data, c);

	switch (c) {
	case '-':
		tokenizer->state = GMIME_HTML_TOKENIZER_STATE_COMMENT_END;
		break;
	case '>':
		if (tokenizer->logger != NULL) {
			log_issue_at_previous_character (tokenizer,
							 GMIME_HTML_COMPLIANCE_VIOLATION_ABRUPT_CLOSING_OF_EMPTY_COMMENT,
							 c);
		}

		tokenizer->state = GMIME_HTML_TOKENIZER_STATE_DATA;
		return emit_comment (tokenizer, tokenizer->name->str, tokenizer->name->len, FALSE);
	default:
		tokenizer->state = GMIME_HTML_TOKENIZER_STATE_COMMENT;
		g_string_append_unichar (tokenizer->name, '-');
		if (c == '\0')
			log_null_character (tokenizer);

		g_string_append_unichar (tokenizer->name, c == '\0' ? 0xfffd : c);
		break;
	}

	return NULL;
}

static GMimeHtmlToken *
read_comment (GMimeHtmlTokenizer *tokenizer)
{
	gunichar c;

	do {
		if (!try_read_name_until (tokenizer, &CommentSpecials, &c)) {
			tokenizer->state = GMIME_HTML_TOKENIZER_STATE_END_OF_FILE;
			log_end_of_file (tokenizer, GMIME_HTML_COMPLIANCE_VIOLATION_EOF_IN_COMMENT);
			return emit_comment (tokenizer, tokenizer->name->str, tokenizer->name->len, FALSE);
		}

		// Note: we save the data in case we hit a parse error and have to emit a data token
		g_string_append_unichar (tokenizer->data, c);

		switch (c) {
		case '-':
			tokenizer->state = GMIME_HTML_TOKENIZER_STATE_COMMENT_END_DASH;
			return NULL;
		default:
			if (c == '\0')
				log_null_character (tokenizer);

			g_string_append_unichar (tokenizer->name, c == '\0' ? 0xfffd : c);
			break;
		}
	} while (TRUE);
}

static GMimeHtmlToken *
read_comment_end_dash (GMimeHtmlTokenizer *tokenizer)
{
	gunichar c;

	if (!try_read (tokenizer, &c)) {
		log_end_of_file (tokenizer, GMIME_HTML_COMPLIANCE_VIOLATION_EOF_IN_COMMENT);
		tokenizer->state = GMIME_HTML_TOKENIZER_STATE_DATA;
		return emit_comment (tokenizer, tokenizer->name->str, tokenizer->name->len, FALSE);
	}

	g_string_append_unichar (tokenizer->data, c);

	switch (c) {
	case '-':
		// Note: "<!--" within a comment is a nested-comment parse error unless it is immediately followed by '>'.
		if (tokenizer->logger != NULL && tokenizer->name->len >= 2 &&
		    tokenizer->name->str[tokenizer->name->len - 2] == '<' &&
		    tokenizer->name->str[tokenizer->name->len - 1] == '!')
			tokenizer->nestedComment = TRUE;
		tokenizer->state = GMIME_HTML_TOKENIZER_STATE_COMMENT_END;
		break;
	default:
		tokenizer->state = GMIME_HTML_TOKENIZER_STATE_COMMENT;
		g_string_append_unichar (tokenizer->name, '-');
		if (c == '\0')
			log_null_character (tokenizer);

		g_string_append_unichar (tokenizer->name, c == '\0' ? 0xfffd : c);
		break;
	}

	return NULL;
}

static GMimeHtmlToken *
read_comment_end (GMimeHtmlTokenizer *tokenizer)
{
	gunichar c;

	gboolean nested = tokenizer->nestedComment;

	tokenizer->nestedComment = FALSE;

	do {
		if (!try_read (tokenizer, &c)) {
			log_end_of_file (tokenizer, GMIME_HTML_COMPLIANCE_VIOLATION_EOF_IN_COMMENT);
			tokenizer->state = GMIME_HTML_TOKENIZER_STATE_END_OF_FILE;
			return emit_comment (tokenizer, tokenizer->name->str, tokenizer->name->len, FALSE);
		}

		// Note: we save the data in case we hit a parse error and have to emit a data token
		g_string_append_unichar (tokenizer->data, c);

		if (nested) {
			if (c != '>')
				log_issue_at_previous_character (tokenizer,
								 GMIME_HTML_COMPLIANCE_VIOLATION_NESTED_COMMENT, c);

			nested = FALSE;
		}

		switch (c) {
		case '>':
			tokenizer->state = GMIME_HTML_TOKENIZER_STATE_DATA;
			return emit_comment (tokenizer, tokenizer->name->str, tokenizer->name->len, FALSE);
		case '!':
			tokenizer->state = GMIME_HTML_TOKENIZER_STATE_COMMENT_END_BANG;
			return NULL;
		case '-':
			g_string_append_unichar (tokenizer->name, '-');
			break;
		default:
			tokenizer->state = GMIME_HTML_TOKENIZER_STATE_COMMENT;
			g_string_append (tokenizer->name, "--");
			if (c == '\0')
				log_null_character (tokenizer);

			g_string_append_unichar (tokenizer->name, c == '\0' ? 0xfffd : c);
			return NULL;
		}
	} while (TRUE);
}

static GMimeHtmlToken *
read_comment_end_bang (GMimeHtmlTokenizer *tokenizer)
{
	gunichar c;

	if (!try_read (tokenizer, &c)) {
		log_end_of_file (tokenizer, GMIME_HTML_COMPLIANCE_VIOLATION_EOF_IN_COMMENT);
		tokenizer->state = GMIME_HTML_TOKENIZER_STATE_END_OF_FILE;
		return emit_comment (tokenizer, tokenizer->name->str, tokenizer->name->len, FALSE);
	}

	g_string_append_unichar (tokenizer->data, c);

	switch (c) {
	case '-':
		tokenizer->state = GMIME_HTML_TOKENIZER_STATE_COMMENT_END_DASH;
		g_string_append (tokenizer->name, "--!");
		break;
	case '>':
		if (tokenizer->logger != NULL) {
			log_issue_at_previous_character (tokenizer,
							 GMIME_HTML_COMPLIANCE_VIOLATION_INCORRECTLY_CLOSED_COMMENT, c);
		}

		tokenizer->state = GMIME_HTML_TOKENIZER_STATE_DATA;
		return emit_comment (tokenizer, tokenizer->name->str, tokenizer->name->len, FALSE);
	default:
		tokenizer->state = GMIME_HTML_TOKENIZER_STATE_COMMENT;
		g_string_append (tokenizer->name, "--!");
		if (c == '\0')
			log_null_character (tokenizer);

		g_string_append_unichar (tokenizer->name, c == '\0' ? 0xfffd : c);
		break;
	}

	return NULL;
}

static GMimeHtmlToken *
read_doc_type (GMimeHtmlTokenizer *tokenizer)
{
	gunichar c;

	if (!try_peek (tokenizer, &c)) {
		log_end_of_file (tokenizer, GMIME_HTML_COMPLIANCE_VIOLATION_EOF_IN_DOC_TYPE);
		tokenizer->state = GMIME_HTML_TOKENIZER_STATE_END_OF_FILE;
		tokenizer->doctype->force_quirks = TRUE;
		g_string_truncate (tokenizer->name, 0);

		return emit_doc_type (tokenizer);
	}

	tokenizer->state = GMIME_HTML_TOKENIZER_STATE_BEFORE_DOC_TYPE_NAME;

	switch (c) {
	case '\t':
	case '\r':
	case '\n':
	case '\f':
	case ' ':
		consume_character (tokenizer, c);
		g_string_append_unichar (tokenizer->data, c);
		break;
	case '>':
		break;
	default:
		if (tokenizer->logger != NULL) {
			log_issue_at_current_position (tokenizer,
						       GMIME_HTML_COMPLIANCE_VIOLATION_MISSING_WHITESPACE_BEFORE_DOC_TYPE_NAME);
		}

		break;
	}

	return NULL;
}

static GMimeHtmlToken *
read_before_doc_type_name (GMimeHtmlTokenizer *tokenizer)
{
	gunichar c;

	do {
		if (!try_read (tokenizer, &c)) {
			log_end_of_file (tokenizer, GMIME_HTML_COMPLIANCE_VIOLATION_EOF_IN_DOC_TYPE);
			tokenizer->state = GMIME_HTML_TOKENIZER_STATE_END_OF_FILE;
			tokenizer->doctype->force_quirks = TRUE;
			return emit_doc_type (tokenizer);
		}

		// Note: we save the data in case we hit a parse error and have to emit a data token
		g_string_append_unichar (tokenizer->data, c);

		switch (c) {
		case '\t':
		case '\r':
		case '\n':
		case '\f':
		case ' ':
			break;
		case '>':
			if (tokenizer->logger != NULL) {
				log_issue_at_previous_character (tokenizer,
								 GMIME_HTML_COMPLIANCE_VIOLATION_MISSING_DOC_TYPE_NAME,
								 c);
			}

			tokenizer->state = GMIME_HTML_TOKENIZER_STATE_DATA;
			tokenizer->doctype->force_quirks = TRUE;
			return emit_doc_type (tokenizer);
		default:
			tokenizer->state = GMIME_HTML_TOKENIZER_STATE_DOC_TYPE_NAME;
			if (c == '\0')
				log_null_character (tokenizer);

			g_string_append_unichar (tokenizer->name, c == '\0' ? 0xfffd : c);
			return NULL;
		}
	} while (TRUE);
}

static GMimeHtmlToken *
read_doc_type_name (GMimeHtmlTokenizer *tokenizer)
{
	gunichar c;

	do {
		if (!try_read (tokenizer, &c)) {
			log_end_of_file (tokenizer, GMIME_HTML_COMPLIANCE_VIOLATION_EOF_IN_DOC_TYPE);
			tokenizer->state = GMIME_HTML_TOKENIZER_STATE_END_OF_FILE;
			set_string (&tokenizer->doctype->name, tokenizer->name->str);
			tokenizer->doctype->force_quirks = TRUE;
			g_string_truncate (tokenizer->name, 0);

			return emit_doc_type (tokenizer);
		}

		// Note: we save the data in case we hit a parse error and have to emit a data token
		g_string_append_unichar (tokenizer->data, c);

		switch (c) {
		case '\t':
		case '\r':
		case '\n':
		case '\f':
		case ' ':
			tokenizer->state = GMIME_HTML_TOKENIZER_STATE_AFTER_DOC_TYPE_NAME;
			break;
		case '>':
			tokenizer->state = GMIME_HTML_TOKENIZER_STATE_DATA;
			set_string (&tokenizer->doctype->name, tokenizer->name->str);
			g_string_truncate (tokenizer->name, 0);

			return emit_doc_type (tokenizer);
		case '\0':
			log_null_character (tokenizer);
			g_string_append_unichar (tokenizer->name, 0xfffd);
			break;
		default:
			g_string_append_unichar (tokenizer->name, c);
			break;
		}
	} while (tokenizer->state == GMIME_HTML_TOKENIZER_STATE_DOC_TYPE_NAME);

	set_string (&tokenizer->doctype->name, tokenizer->name->str);
	g_string_truncate (tokenizer->name, 0);

	return NULL;
}

static GMimeHtmlToken *
read_after_doc_type_name (GMimeHtmlTokenizer *tokenizer)
{
	gunichar c;

	do {
		if (!try_read (tokenizer, &c)) {
			if (tokenizer->logger != NULL) {
				log_invalid_doc_type_keyword (tokenizer, tokenizer->name->len > 0);
			}

			tokenizer->state = GMIME_HTML_TOKENIZER_STATE_END_OF_FILE;
			tokenizer->doctype->force_quirks = TRUE;
			return emit_doc_type (tokenizer);
		}

		// Note: we save the data in case we hit a parse error and have to emit a data token
		g_string_append_unichar (tokenizer->data, c);

		switch (c) {
		case '\t':
		case '\r':
		case '\n':
		case '\f':
		case ' ':
			if (tokenizer->name->len > 0) {
				// parse error: not "PUBLIC" or "SYSTEM"
				if (tokenizer->logger != NULL) {
					log_invalid_doc_type_keyword (tokenizer, TRUE);
				}

				tokenizer->state = GMIME_HTML_TOKENIZER_STATE_BOGUS_DOC_TYPE;
				tokenizer->doctype->force_quirks = TRUE;
				g_string_truncate (tokenizer->name, 0);
				return NULL;
			}

			break;
		case '>':
			if (tokenizer->name->len > 0) {
				// parse error: not "PUBLIC" or "SYSTEM"
				if (tokenizer->logger != NULL) {
					log_invalid_doc_type_keyword (tokenizer, TRUE);
				}

				tokenizer->doctype->force_quirks = TRUE;
				g_string_truncate (tokenizer->name, 0);
			}

			tokenizer->state = GMIME_HTML_TOKENIZER_STATE_DATA;
			return emit_doc_type (tokenizer);
		default:
			if (tokenizer->name->len == 0 && tokenizer->logger != NULL)
				mark_previous_character (tokenizer);

			g_string_append_unichar (tokenizer->name, c);
			if (tokenizer->name->len < 6)
				break;

			if (name_is (tokenizer, "public")) {
				tokenizer->state = GMIME_HTML_TOKENIZER_STATE_AFTER_DOC_TYPE_PUBLIC_KEYWORD;
				set_string (&tokenizer->doctype->public_keyword, tokenizer->name->str);
			} else if (name_is (tokenizer, "system")) {
				tokenizer->state = GMIME_HTML_TOKENIZER_STATE_AFTER_DOC_TYPE_SYSTEM_KEYWORD;
				set_string (&tokenizer->doctype->system_keyword, tokenizer->name->str);
			} else {
				if (tokenizer->logger != NULL) {
					log_invalid_doc_type_keyword (tokenizer, TRUE);
				}

				tokenizer->state = GMIME_HTML_TOKENIZER_STATE_BOGUS_DOC_TYPE;
				tokenizer->doctype->force_quirks = TRUE;
			}

			g_string_truncate (tokenizer->name, 0);
			return NULL;
		}
	} while (TRUE);
}

static GMimeHtmlToken *
read_after_doc_type_public_keyword (GMimeHtmlTokenizer *tokenizer)
{
	gunichar c;

	if (!try_read (tokenizer, &c)) {
		log_end_of_file (tokenizer, GMIME_HTML_COMPLIANCE_VIOLATION_EOF_IN_DOC_TYPE);
		tokenizer->state = GMIME_HTML_TOKENIZER_STATE_END_OF_FILE;
		tokenizer->doctype->force_quirks = TRUE;
		return emit_doc_type (tokenizer);
	}

	// Note: we save the data in case we hit a parse error and have to emit a data token
	g_string_append_unichar (tokenizer->data, c);

	switch (c) {
	case '\t':
	case '\r':
	case '\n':
	case '\f':
	case ' ':
		tokenizer->state = GMIME_HTML_TOKENIZER_STATE_BEFORE_DOC_TYPE_PUBLIC_IDENTIFIER;
		break;
	case '"':
	case '\'':
		if (tokenizer->logger != NULL) {
			log_issue_at_previous_character (tokenizer,
							 GMIME_HTML_COMPLIANCE_VIOLATION_MISSING_WHITESPACE_AFTER_DOC_TYPE_PUBLIC_KEYWORD,
							 c);
		}

		tokenizer->state = GMIME_HTML_TOKENIZER_STATE_DOC_TYPE_PUBLIC_IDENTIFIER_QUOTED;
		set_string (&tokenizer->doctype->public_identifier, "");
		tokenizer->doctype->public_quote = c;
		tokenizer->quote = c;
		break;
	case '>':
		if (tokenizer->logger != NULL) {
			log_issue_at_previous_character (tokenizer,
							 GMIME_HTML_COMPLIANCE_VIOLATION_MISSING_DOC_TYPE_PUBLIC_IDENTIFIER,
							 c);
		}

		tokenizer->state = GMIME_HTML_TOKENIZER_STATE_DATA;
		tokenizer->doctype->force_quirks = TRUE;
		return emit_doc_type (tokenizer);
	default:
		if (tokenizer->logger != NULL) {
			log_issue_at_previous_character (tokenizer,
							 GMIME_HTML_COMPLIANCE_VIOLATION_MISSING_QUOTE_BEFORE_DOC_TYPE_PUBLIC_IDENTIFIER,
							 c);
		}

		tokenizer->state = GMIME_HTML_TOKENIZER_STATE_BOGUS_DOC_TYPE;
		tokenizer->doctype->force_quirks = TRUE;
		break;
	}

	return NULL;
}

static GMimeHtmlToken *
read_before_doc_type_public_identifier (GMimeHtmlTokenizer *tokenizer)
{
	gunichar c;

	do {
		if (!try_read (tokenizer, &c)) {
			log_end_of_file (tokenizer, GMIME_HTML_COMPLIANCE_VIOLATION_EOF_IN_DOC_TYPE);
			tokenizer->state = GMIME_HTML_TOKENIZER_STATE_END_OF_FILE;
			tokenizer->doctype->force_quirks = TRUE;
			return emit_doc_type (tokenizer);
		}

		// Note: we save the data in case we hit a parse error and have to emit a data token
		g_string_append_unichar (tokenizer->data, c);

		switch (c) {
		case '\t':
		case '\r':
		case '\n':
		case '\f':
		case ' ':
			break;
		case '"':
		case '\'':
			tokenizer->state = GMIME_HTML_TOKENIZER_STATE_DOC_TYPE_PUBLIC_IDENTIFIER_QUOTED;
			set_string (&tokenizer->doctype->public_identifier, "");
			tokenizer->doctype->public_quote = c;
			tokenizer->quote = c;
			return NULL;
		case '>':
			if (tokenizer->logger != NULL) {
				log_issue_at_previous_character (tokenizer,
								 GMIME_HTML_COMPLIANCE_VIOLATION_MISSING_DOC_TYPE_PUBLIC_IDENTIFIER,
								 c);
			}

			tokenizer->state = GMIME_HTML_TOKENIZER_STATE_DATA;
			tokenizer->doctype->force_quirks = TRUE;
			return emit_doc_type (tokenizer);
		default:
			if (tokenizer->logger != NULL) {
				log_issue_at_previous_character (tokenizer,
								 GMIME_HTML_COMPLIANCE_VIOLATION_MISSING_QUOTE_BEFORE_DOC_TYPE_PUBLIC_IDENTIFIER,
								 c);
			}

			tokenizer->state = GMIME_HTML_TOKENIZER_STATE_BOGUS_DOC_TYPE;
			tokenizer->doctype->force_quirks = TRUE;
			return NULL;
		}
	} while (TRUE);
}

static GMimeHtmlToken *
read_doc_type_public_identifier_quoted (GMimeHtmlTokenizer *tokenizer)
{
	gunichar c;

	do {
		if (!try_read (tokenizer, &c)) {
			log_end_of_file (tokenizer, GMIME_HTML_COMPLIANCE_VIOLATION_EOF_IN_DOC_TYPE);
			tokenizer->state = GMIME_HTML_TOKENIZER_STATE_END_OF_FILE;
			set_string (&tokenizer->doctype->public_identifier, tokenizer->name->str);
			tokenizer->doctype->force_quirks = TRUE;
			g_string_truncate (tokenizer->name, 0);

			return emit_doc_type (tokenizer);
		}

		// Note: we save the data in case we hit a parse error and have to emit a data token
		g_string_append_unichar (tokenizer->data, c);

		switch (c) {
		case '\0':
			log_null_character (tokenizer);
			g_string_append_unichar (tokenizer->name, 0xfffd);
			break;
		case '>':
			if (tokenizer->logger != NULL) {
				log_issue_at_previous_character (tokenizer,
								 GMIME_HTML_COMPLIANCE_VIOLATION_ABRUPT_DOC_TYPE_PUBLIC_IDENTIFIER,
								 c);
			}

			tokenizer->state = GMIME_HTML_TOKENIZER_STATE_DATA;
			set_string (&tokenizer->doctype->public_identifier, tokenizer->name->str);
			tokenizer->doctype->force_quirks = TRUE;
			g_string_truncate (tokenizer->name, 0);

			return emit_doc_type (tokenizer);
		default:
			if (c == tokenizer->quote) {
				tokenizer->state = GMIME_HTML_TOKENIZER_STATE_AFTER_DOC_TYPE_PUBLIC_IDENTIFIER;
				tokenizer->quote = '\0';
				break;
			}

			g_string_append_unichar (tokenizer->name, c);
			break;
		}
	} while (tokenizer->state == GMIME_HTML_TOKENIZER_STATE_DOC_TYPE_PUBLIC_IDENTIFIER_QUOTED);

	set_string (&tokenizer->doctype->public_identifier, tokenizer->name->str);
	g_string_truncate (tokenizer->name, 0);

	return NULL;
}

static GMimeHtmlToken *
read_after_doc_type_public_identifier (GMimeHtmlTokenizer *tokenizer)
{
	gunichar c;

	if (!try_read (tokenizer, &c)) {
		log_end_of_file (tokenizer, GMIME_HTML_COMPLIANCE_VIOLATION_EOF_IN_DOC_TYPE);
		tokenizer->state = GMIME_HTML_TOKENIZER_STATE_END_OF_FILE;
		tokenizer->doctype->force_quirks = TRUE;
		return emit_doc_type (tokenizer);
	}

	// Note: we save the data in case we hit a parse error and have to emit a data token
	g_string_append_unichar (tokenizer->data, c);

	switch (c) {
	case '\t':
	case '\r':
	case '\n':
	case '\f':
	case ' ':
		tokenizer->state = GMIME_HTML_TOKENIZER_STATE_BETWEEN_DOC_TYPE_PUBLIC_AND_SYSTEM_IDENTIFIERS;
		break;
	case '>':
		tokenizer->state = GMIME_HTML_TOKENIZER_STATE_DATA;
		return emit_doc_type (tokenizer);
	case '"':
	case '\'':
		if (tokenizer->logger != NULL) {
			log_issue_at_previous_character (tokenizer,
							 GMIME_HTML_COMPLIANCE_VIOLATION_MISSING_WHITESPACE_BETWEEN_DOC_TYPE_PUBLIC_AND_SYSTEM_IDENTIFIERS,
							 c);
		}

		tokenizer->state = GMIME_HTML_TOKENIZER_STATE_DOC_TYPE_SYSTEM_IDENTIFIER_QUOTED;
		set_string (&tokenizer->doctype->system_identifier, "");
		tokenizer->doctype->system_quote = c;
		tokenizer->quote = c;
		break;
	default:
		if (tokenizer->logger != NULL) {
			log_issue_at_previous_character (tokenizer,
							 GMIME_HTML_COMPLIANCE_VIOLATION_MISSING_QUOTE_BEFORE_DOC_TYPE_SYSTEM_IDENTIFIER,
							 c);
		}

		tokenizer->state = GMIME_HTML_TOKENIZER_STATE_BOGUS_DOC_TYPE;
		tokenizer->doctype->force_quirks = TRUE;
		break;
	}

	return NULL;
}

static GMimeHtmlToken *
read_between_doc_type_public_and_system_identifiers (GMimeHtmlTokenizer *tokenizer)
{
	gunichar c;

	do {
		if (!try_read (tokenizer, &c)) {
			log_end_of_file (tokenizer, GMIME_HTML_COMPLIANCE_VIOLATION_EOF_IN_DOC_TYPE);
			tokenizer->state = GMIME_HTML_TOKENIZER_STATE_END_OF_FILE;
			tokenizer->doctype->force_quirks = TRUE;
			return emit_doc_type (tokenizer);
		}

		// Note: we save the data in case we hit a parse error and have to emit a data token
		g_string_append_unichar (tokenizer->data, c);

		switch (c) {
		case '\t':
		case '\r':
		case '\n':
		case '\f':
		case ' ':
			break;
		case '>':
			tokenizer->state = GMIME_HTML_TOKENIZER_STATE_DATA;
			return emit_doc_type (tokenizer);
		case '"':
		case '\'':
			tokenizer->state = GMIME_HTML_TOKENIZER_STATE_DOC_TYPE_SYSTEM_IDENTIFIER_QUOTED;
			set_string (&tokenizer->doctype->system_identifier, "");
			tokenizer->doctype->system_quote = c;
			tokenizer->quote = c;
			return NULL;
		default:
			if (tokenizer->logger != NULL) {
				log_issue_at_previous_character (tokenizer,
								 GMIME_HTML_COMPLIANCE_VIOLATION_MISSING_QUOTE_BEFORE_DOC_TYPE_SYSTEM_IDENTIFIER,
								 c);
			}

			tokenizer->state = GMIME_HTML_TOKENIZER_STATE_BOGUS_DOC_TYPE;
			tokenizer->doctype->force_quirks = TRUE;
			return NULL;
		}
	} while (TRUE);
}

static GMimeHtmlToken *
read_after_doc_type_system_keyword (GMimeHtmlTokenizer *tokenizer)
{
	gunichar c;

	if (!try_read (tokenizer, &c)) {
		log_end_of_file (tokenizer, GMIME_HTML_COMPLIANCE_VIOLATION_EOF_IN_DOC_TYPE);
		tokenizer->state = GMIME_HTML_TOKENIZER_STATE_END_OF_FILE;
		tokenizer->doctype->force_quirks = TRUE;
		return emit_doc_type (tokenizer);
	}

	// Note: we save the data in case we hit a parse error and have to emit a data token
	g_string_append_unichar (tokenizer->data, c);

	switch (c) {
	case '\t':
	case '\r':
	case '\n':
	case '\f':
	case ' ':
		tokenizer->state = GMIME_HTML_TOKENIZER_STATE_BEFORE_DOC_TYPE_SYSTEM_IDENTIFIER;
		break;
	case '"':
	case '\'':
		if (tokenizer->logger != NULL) {
			log_issue_at_previous_character (tokenizer,
							 GMIME_HTML_COMPLIANCE_VIOLATION_MISSING_WHITESPACE_AFTER_DOC_TYPE_SYSTEM_KEYWORD,
							 c);
		}

		tokenizer->state = GMIME_HTML_TOKENIZER_STATE_DOC_TYPE_SYSTEM_IDENTIFIER_QUOTED;
		set_string (&tokenizer->doctype->system_identifier, "");
		tokenizer->doctype->system_quote = c;
		tokenizer->quote = c;
		break;
	case '>':
		if (tokenizer->logger != NULL) {
			log_issue_at_previous_character (tokenizer,
							 GMIME_HTML_COMPLIANCE_VIOLATION_MISSING_DOC_TYPE_SYSTEM_IDENTIFIER,
							 c);
		}

		tokenizer->state = GMIME_HTML_TOKENIZER_STATE_DATA;
		tokenizer->doctype->force_quirks = TRUE;
		return emit_doc_type (tokenizer);
	default:
		if (tokenizer->logger != NULL) {
			log_issue_at_previous_character (tokenizer,
							 GMIME_HTML_COMPLIANCE_VIOLATION_MISSING_QUOTE_BEFORE_DOC_TYPE_SYSTEM_IDENTIFIER,
							 c);
		}

		tokenizer->state = GMIME_HTML_TOKENIZER_STATE_BOGUS_DOC_TYPE;
		tokenizer->doctype->force_quirks = TRUE;
		break;
	}

	return NULL;
}

static GMimeHtmlToken *
read_before_doc_type_system_identifier (GMimeHtmlTokenizer *tokenizer)
{
	gunichar c;

	do {
		if (!try_read (tokenizer, &c)) {
			log_end_of_file (tokenizer, GMIME_HTML_COMPLIANCE_VIOLATION_EOF_IN_DOC_TYPE);
			tokenizer->state = GMIME_HTML_TOKENIZER_STATE_END_OF_FILE;
			tokenizer->doctype->force_quirks = TRUE;
			return emit_doc_type (tokenizer);
		}

		// Note: we save the data in case we hit a parse error and have to emit a data token
		g_string_append_unichar (tokenizer->data, c);

		switch (c) {
		case '\t':
		case '\r':
		case '\n':
		case '\f':
		case ' ':
			break;
		case '"':
		case '\'':
			tokenizer->state = GMIME_HTML_TOKENIZER_STATE_DOC_TYPE_SYSTEM_IDENTIFIER_QUOTED;
			set_string (&tokenizer->doctype->system_identifier, "");
			tokenizer->doctype->system_quote = c;
			tokenizer->quote = c;
			return NULL;
		case '>':
			if (tokenizer->logger != NULL) {
				log_issue_at_previous_character (tokenizer,
								 GMIME_HTML_COMPLIANCE_VIOLATION_MISSING_DOC_TYPE_SYSTEM_IDENTIFIER,
								 c);
			}

			tokenizer->state = GMIME_HTML_TOKENIZER_STATE_DATA;
			tokenizer->doctype->force_quirks = TRUE;
			return emit_doc_type (tokenizer);
		default:
			if (tokenizer->logger != NULL) {
				log_issue_at_previous_character (tokenizer,
								 GMIME_HTML_COMPLIANCE_VIOLATION_MISSING_QUOTE_BEFORE_DOC_TYPE_SYSTEM_IDENTIFIER,
								 c);
			}

			tokenizer->state = GMIME_HTML_TOKENIZER_STATE_BOGUS_DOC_TYPE;
			tokenizer->doctype->force_quirks = TRUE;
			return NULL;
		}
	} while (TRUE);
}

static GMimeHtmlToken *
read_doc_type_system_identifier_quoted (GMimeHtmlTokenizer *tokenizer)
{
	gunichar c;

	do {
		if (!try_read (tokenizer, &c)) {
			log_end_of_file (tokenizer, GMIME_HTML_COMPLIANCE_VIOLATION_EOF_IN_DOC_TYPE);
			tokenizer->state = GMIME_HTML_TOKENIZER_STATE_END_OF_FILE;
			set_string (&tokenizer->doctype->system_identifier, tokenizer->name->str);
			tokenizer->doctype->force_quirks = TRUE;
			g_string_truncate (tokenizer->name, 0);

			return emit_doc_type (tokenizer);
		}

		// Note: we save the data in case we hit a parse error and have to emit a data token
		g_string_append_unichar (tokenizer->data, c);

		switch (c) {
		case '\0':
			log_null_character (tokenizer);
			g_string_append_unichar (tokenizer->name, 0xfffd);
			break;
		case '>':
			if (tokenizer->logger != NULL) {
				log_issue_at_previous_character (tokenizer,
								 GMIME_HTML_COMPLIANCE_VIOLATION_ABRUPT_DOC_TYPE_SYSTEM_IDENTIFIER,
								 c);
			}

			tokenizer->state = GMIME_HTML_TOKENIZER_STATE_DATA;
			set_string (&tokenizer->doctype->system_identifier, tokenizer->name->str);
			tokenizer->doctype->force_quirks = TRUE;
			g_string_truncate (tokenizer->name, 0);

			return emit_doc_type (tokenizer);
		default:
			if (c == tokenizer->quote) {
				tokenizer->state = GMIME_HTML_TOKENIZER_STATE_AFTER_DOC_TYPE_SYSTEM_IDENTIFIER;
				tokenizer->quote = '\0';
				break;
			}

			g_string_append_unichar (tokenizer->name, c);
			break;
		}
	} while (tokenizer->state == GMIME_HTML_TOKENIZER_STATE_DOC_TYPE_SYSTEM_IDENTIFIER_QUOTED);

	set_string (&tokenizer->doctype->system_identifier, tokenizer->name->str);
	g_string_truncate (tokenizer->name, 0);

	return NULL;
}

static GMimeHtmlToken *
read_after_doc_type_system_identifier (GMimeHtmlTokenizer *tokenizer)
{
	gunichar c;

	do {
		if (!try_read (tokenizer, &c)) {
			log_end_of_file (tokenizer, GMIME_HTML_COMPLIANCE_VIOLATION_EOF_IN_DOC_TYPE);
			tokenizer->state = GMIME_HTML_TOKENIZER_STATE_END_OF_FILE;
			tokenizer->doctype->force_quirks = TRUE;
			return emit_doc_type (tokenizer);
		}

		// Note: we save the data in case we hit a parse error and have to emit a data token
		g_string_append_unichar (tokenizer->data, c);

		switch (c) {
		case '\t':
		case '\r':
		case '\n':
		case '\f':
		case ' ':
			break;
		case '>':
			tokenizer->state = GMIME_HTML_TOKENIZER_STATE_DATA;
			return emit_doc_type (tokenizer);
		default:
			if (tokenizer->logger != NULL) {
				log_issue_at_previous_character (tokenizer,
								 GMIME_HTML_COMPLIANCE_VIOLATION_UNEXPECTED_CHARACTER_AFTER_DOC_TYPE_SYSTEM_IDENTIFIER,
								 c);
			}

			tokenizer->state = GMIME_HTML_TOKENIZER_STATE_BOGUS_DOC_TYPE;
			return NULL;
		}
	} while (TRUE);
}

static GMimeHtmlToken *
read_bogus_doc_type (GMimeHtmlTokenizer *tokenizer)
{
	gunichar c;

	do {
		if (!try_read (tokenizer, &c)) {
			// Note: unlike most DOCTYPE states, EOF in the bogus DOCTYPE state does not set force-quirks.
			tokenizer->state = GMIME_HTML_TOKENIZER_STATE_END_OF_FILE;
			return emit_doc_type (tokenizer);
		}

		// Note: we save the data in case we hit a parse error and have to emit a data token
		g_string_append_unichar (tokenizer->data, c);

		if (c == '>') {
			tokenizer->state = GMIME_HTML_TOKENIZER_STATE_DATA;
			return emit_doc_type (tokenizer);
		}

		if (c == '\0')
			log_null_character (tokenizer);
	} while (TRUE);
}

static GMimeHtmlToken *
read_state (GMimeHtmlTokenizer *tokenizer)
{
	switch (tokenizer->state) {
	case GMIME_HTML_TOKENIZER_STATE_DATA:
		return read_data (tokenizer);
	case GMIME_HTML_TOKENIZER_STATE_CHARACTER_REFERENCE_IN_DATA:
		return read_character_reference_in_data (tokenizer);
	case GMIME_HTML_TOKENIZER_STATE_RC_DATA:
		return read_rc_data (tokenizer);
	case GMIME_HTML_TOKENIZER_STATE_CHARACTER_REFERENCE_IN_RC_DATA:
		return read_character_reference_in_rc_data (tokenizer);
	case GMIME_HTML_TOKENIZER_STATE_RAW_TEXT:
		return read_raw_text (tokenizer);
	case GMIME_HTML_TOKENIZER_STATE_SCRIPT_DATA:
		return read_script_data (tokenizer);
	case GMIME_HTML_TOKENIZER_STATE_PLAIN_TEXT:
		return read_plain_text (tokenizer);
	case GMIME_HTML_TOKENIZER_STATE_TAG_OPEN:
		return read_tag_open (tokenizer);
	case GMIME_HTML_TOKENIZER_STATE_END_TAG_OPEN:
		return read_end_tag_open (tokenizer);
	case GMIME_HTML_TOKENIZER_STATE_TAG_NAME:
		return read_tag_name (tokenizer);
	case GMIME_HTML_TOKENIZER_STATE_RC_DATA_LESS_THAN:
		return read_rc_data_less_than (tokenizer);
	case GMIME_HTML_TOKENIZER_STATE_RC_DATA_END_TAG_OPEN:
		return read_rc_data_end_tag_open (tokenizer);
	case GMIME_HTML_TOKENIZER_STATE_RC_DATA_END_TAG_NAME:
		return read_rc_data_end_tag_name (tokenizer);
	case GMIME_HTML_TOKENIZER_STATE_RAW_TEXT_LESS_THAN:
		return read_raw_text_less_than (tokenizer);
	case GMIME_HTML_TOKENIZER_STATE_RAW_TEXT_END_TAG_OPEN:
		return read_raw_text_end_tag_open (tokenizer);
	case GMIME_HTML_TOKENIZER_STATE_RAW_TEXT_END_TAG_NAME:
		return read_raw_text_end_tag_name (tokenizer);
	case GMIME_HTML_TOKENIZER_STATE_SCRIPT_DATA_LESS_THAN:
		return read_script_data_less_than (tokenizer);
	case GMIME_HTML_TOKENIZER_STATE_SCRIPT_DATA_END_TAG_OPEN:
		return read_script_data_end_tag_open (tokenizer);
	case GMIME_HTML_TOKENIZER_STATE_SCRIPT_DATA_END_TAG_NAME:
		return read_script_data_end_tag_name (tokenizer);
	case GMIME_HTML_TOKENIZER_STATE_SCRIPT_DATA_ESCAPE_START:
		return read_script_data_escape_start (tokenizer);
	case GMIME_HTML_TOKENIZER_STATE_SCRIPT_DATA_ESCAPE_START_DASH:
		return read_script_data_escape_start_dash (tokenizer);
	case GMIME_HTML_TOKENIZER_STATE_SCRIPT_DATA_ESCAPED:
		return read_script_data_escaped (tokenizer);
	case GMIME_HTML_TOKENIZER_STATE_SCRIPT_DATA_ESCAPED_DASH:
		return read_script_data_escaped_dash (tokenizer);
	case GMIME_HTML_TOKENIZER_STATE_SCRIPT_DATA_ESCAPED_DASH_DASH:
		return read_script_data_escaped_dash_dash (tokenizer);
	case GMIME_HTML_TOKENIZER_STATE_SCRIPT_DATA_ESCAPED_LESS_THAN:
		return read_script_data_escaped_less_than (tokenizer);
	case GMIME_HTML_TOKENIZER_STATE_SCRIPT_DATA_ESCAPED_END_TAG_OPEN:
		return read_script_data_escaped_end_tag_open (tokenizer);
	case GMIME_HTML_TOKENIZER_STATE_SCRIPT_DATA_ESCAPED_END_TAG_NAME:
		return read_script_data_escaped_end_tag_name (tokenizer);
	case GMIME_HTML_TOKENIZER_STATE_SCRIPT_DATA_DOUBLE_ESCAPE_START:
		return read_script_data_double_escape_start (tokenizer);
	case GMIME_HTML_TOKENIZER_STATE_SCRIPT_DATA_DOUBLE_ESCAPED:
		return read_script_data_double_escaped (tokenizer);
	case GMIME_HTML_TOKENIZER_STATE_SCRIPT_DATA_DOUBLE_ESCAPED_DASH:
		return read_script_data_double_escaped_dash (tokenizer);
	case GMIME_HTML_TOKENIZER_STATE_SCRIPT_DATA_DOUBLE_ESCAPED_DASH_DASH:
		return read_script_data_double_escaped_dash_dash (tokenizer);
	case GMIME_HTML_TOKENIZER_STATE_SCRIPT_DATA_DOUBLE_ESCAPED_LESS_THAN:
		return read_script_data_double_escaped_less_than (tokenizer);
	case GMIME_HTML_TOKENIZER_STATE_SCRIPT_DATA_DOUBLE_ESCAPE_END:
		return read_script_data_double_escape_end (tokenizer);
	case GMIME_HTML_TOKENIZER_STATE_BEFORE_ATTRIBUTE_NAME:
		return read_before_attribute_name (tokenizer);
	case GMIME_HTML_TOKENIZER_STATE_ATTRIBUTE_NAME:
		return read_attribute_name (tokenizer);
	case GMIME_HTML_TOKENIZER_STATE_AFTER_ATTRIBUTE_NAME:
		return read_after_attribute_name (tokenizer);
	case GMIME_HTML_TOKENIZER_STATE_BEFORE_ATTRIBUTE_VALUE:
		return read_before_attribute_value (tokenizer);
	case GMIME_HTML_TOKENIZER_STATE_ATTRIBUTE_VALUE_DOUBLE_QUOTED:
		return read_attribute_value_double_quoted (tokenizer);
	case GMIME_HTML_TOKENIZER_STATE_ATTRIBUTE_VALUE_SINGLE_QUOTED:
		return read_attribute_value_single_quoted (tokenizer);
	case GMIME_HTML_TOKENIZER_STATE_ATTRIBUTE_VALUE_UNQUOTED:
		return read_attribute_value_unquoted (tokenizer);
	case GMIME_HTML_TOKENIZER_STATE_CHARACTER_REFERENCE_IN_ATTRIBUTE_VALUE:
		return read_character_reference_in_attribute_value (tokenizer);
	case GMIME_HTML_TOKENIZER_STATE_AFTER_ATTRIBUTE_VALUE_QUOTED:
		return read_after_attribute_value_quoted (tokenizer);
	case GMIME_HTML_TOKENIZER_STATE_SELF_CLOSING_START_TAG:
		return read_self_closing_start_tag (tokenizer);
	case GMIME_HTML_TOKENIZER_STATE_BOGUS_COMMENT:
		return read_bogus_comment (tokenizer);
	case GMIME_HTML_TOKENIZER_STATE_MARKUP_DECLARATION_OPEN:
		return read_markup_declaration_open (tokenizer);
	case GMIME_HTML_TOKENIZER_STATE_COMMENT_START:
		return read_comment_start (tokenizer);
	case GMIME_HTML_TOKENIZER_STATE_COMMENT_START_DASH:
		return read_comment_start_dash (tokenizer);
	case GMIME_HTML_TOKENIZER_STATE_COMMENT:
		return read_comment (tokenizer);
	case GMIME_HTML_TOKENIZER_STATE_COMMENT_END_DASH:
		return read_comment_end_dash (tokenizer);
	case GMIME_HTML_TOKENIZER_STATE_COMMENT_END:
		return read_comment_end (tokenizer);
	case GMIME_HTML_TOKENIZER_STATE_COMMENT_END_BANG:
		return read_comment_end_bang (tokenizer);
	case GMIME_HTML_TOKENIZER_STATE_DOC_TYPE:
		return read_doc_type (tokenizer);
	case GMIME_HTML_TOKENIZER_STATE_BEFORE_DOC_TYPE_NAME:
		return read_before_doc_type_name (tokenizer);
	case GMIME_HTML_TOKENIZER_STATE_DOC_TYPE_NAME:
		return read_doc_type_name (tokenizer);
	case GMIME_HTML_TOKENIZER_STATE_AFTER_DOC_TYPE_NAME:
		return read_after_doc_type_name (tokenizer);
	case GMIME_HTML_TOKENIZER_STATE_AFTER_DOC_TYPE_PUBLIC_KEYWORD:
		return read_after_doc_type_public_keyword (tokenizer);
	case GMIME_HTML_TOKENIZER_STATE_BEFORE_DOC_TYPE_PUBLIC_IDENTIFIER:
		return read_before_doc_type_public_identifier (tokenizer);
	case GMIME_HTML_TOKENIZER_STATE_DOC_TYPE_PUBLIC_IDENTIFIER_QUOTED:
		return read_doc_type_public_identifier_quoted (tokenizer);
	case GMIME_HTML_TOKENIZER_STATE_AFTER_DOC_TYPE_PUBLIC_IDENTIFIER:
		return read_after_doc_type_public_identifier (tokenizer);
	case GMIME_HTML_TOKENIZER_STATE_BETWEEN_DOC_TYPE_PUBLIC_AND_SYSTEM_IDENTIFIERS:
		return read_between_doc_type_public_and_system_identifiers (tokenizer);
	case GMIME_HTML_TOKENIZER_STATE_AFTER_DOC_TYPE_SYSTEM_KEYWORD:
		return read_after_doc_type_system_keyword (tokenizer);
	case GMIME_HTML_TOKENIZER_STATE_BEFORE_DOC_TYPE_SYSTEM_IDENTIFIER:
		return read_before_doc_type_system_identifier (tokenizer);
	case GMIME_HTML_TOKENIZER_STATE_DOC_TYPE_SYSTEM_IDENTIFIER_QUOTED:
		return read_doc_type_system_identifier_quoted (tokenizer);
	case GMIME_HTML_TOKENIZER_STATE_AFTER_DOC_TYPE_SYSTEM_IDENTIFIER:
		return read_after_doc_type_system_identifier (tokenizer);
	case GMIME_HTML_TOKENIZER_STATE_BOGUS_DOC_TYPE:
		return read_bogus_doc_type (tokenizer);
	case GMIME_HTML_TOKENIZER_STATE_C_DATA_SECTION:
		return read_c_data_section (tokenizer);
	default:
		return NULL;
	}
}
