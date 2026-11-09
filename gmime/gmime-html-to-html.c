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
/* Filtering and open-element tracking follow MimeKit HtmlToHtml. */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <string.h>

#include "gmime-html-to-html.h"
#include "gmime-html-tree-private.h"
#include "gmime-text-to-html.h"
#include "gmime-text-private.h"
#include "gmime-stream-mem.h"
#include "gmime-reader-simd-private.h"

/**
 * SECTION: gmime-html-to-html
 * @title: HTML rewriting
 * @short_description: HTML rewriting with callbacks and compliance reporting
 *
 * Rewrites HTML with optional comment filtering, fragment output, noscript
 * handling and tag callbacks. Compliance diagnostics describe lexical
 * and structural recovery without changing the output.
 *
 * This converter is not a sanitizer. In particular, raw HTML headers and
 * footers bypass filtering and callbacks.
 **/
struct _GMimeHtmlToHtml {
	GMimeTextConverter parent_object;
	gboolean comments, fragment;
	guint depth;
	GMimeHeaderFooterFormat header_format, footer_format;
	GMimeHtmlNoScriptHandling noscript;
	GMimeHtmlTagCallback callback;
	gpointer callback_data;
	GDestroyNotify callback_destroy;
	GMimeHtmlComplianceLogger logger;
	gpointer logger_data;
	GDestroyNotify logger_destroy;
	GMimeHtmlComplianceOptions *compliance;
};

G_DEFINE_TYPE (GMimeHtmlToHtml, g_mime_html_to_html, GMIME_TYPE_TEXT_CONVERTER)

typedef struct _OpenTag OpenTag;

struct _OpenTag {
	GMimeHtmlTagContext *context;
	OpenTag *next;
	gboolean suppressed;
	char *name;
};

typedef struct {
	GMimeHtmlToHtml *options;
	GMimeHtmlTokenizer *tokenizer;
	GMimeHtmlWriter *writer;
	GHashTable *open;
	guint suppressed;
	OpenTag *head;
	gboolean head_done, raw_text;
	guint head_template_depth;
	GError **error;
} HtmlState;

/* Maps a tag name to the innermost open element with that name. Each element
 * links to the next (outer) open element with the same name. HTML tag names
 * are ASCII case-insensitive. This avoids quadratic scans for unmatched tags. */
static OpenTag *
peek (HtmlState *state, const char *name)
{
	char *lower = g_ascii_strdown (name, -1);
	OpenTag *tag = g_hash_table_lookup (state->open, lower);

	g_free (lower);
	return tag;
}

static void
free_tag (OpenTag *tag)
{
	g_mime_html_tag_context_free (tag->context);
	g_free (tag->name);
	g_free (tag);
}

static OpenTag *
push (HtmlState *state, GMimeHtmlTagContext *context)
{
	OpenTag *tag = g_new0 (OpenTag, 1);

	tag->context = context;
	tag->name = g_ascii_strdown (g_mime_html_tag_context_get_tag_name (context), -1);
	tag->next = peek (state, tag->name);

	/* Snapshot suppression when opened, so later context changes cannot
	 * make the suppression count inconsistent. */
	tag->suppressed = g_mime_html_tag_context_get_suppress_inner_content (context);
	state->suppressed += tag->suppressed;
	g_hash_table_replace (state->open, tag->name, tag);

	return tag;
}

static OpenTag *
pop (HtmlState *state, const char *name)
{
	OpenTag *tag = peek (state, name);

	if (!tag)
		return NULL;

	if (tag->next)
		g_hash_table_replace (state->open, tag->next->name, tag->next);
	else
		g_hash_table_remove (state->open, tag->name);

	state->suppressed -= tag->suppressed;

	return tag;
}

static gboolean
callback (HtmlState *state, GMimeHtmlTagContext *context)
{
	gboolean ok =
		state->options->callback ?
			state->options->callback (context, state->writer, state->options->callback_data, state->error) :
			g_mime_html_tag_context_write_tag (context, state->writer, TRUE, state->error);

	if (!ok && state->error && !*state->error)
		g_set_error_literal (state->error, G_MARKUP_ERROR, G_MARKUP_ERROR_INVALID_CONTENT,
				     "The HTML tag callback failed.");
	return ok;
}

static gboolean
structure (GMimeHtmlTagId id)
{
	return id == GMIME_HTML_TAG_HTML || id == GMIME_HTML_TAG_HEAD || id == GMIME_HTML_TAG_BODY;
}

static gboolean
is_template (GMimeHtmlToken *tag)
{
	return !g_ascii_strcasecmp (g_mime_html_token_get_name (tag), "template");
}

/* Any other start tag implicitly closes <head>; browsers insert it into <body>. */
static gboolean
allowed_in_head (GMimeHtmlToken *tag)
{
	switch (g_mime_html_token_get_tag_id (tag)) {
	case GMIME_HTML_TAG_HTML:
	case GMIME_HTML_TAG_HEAD:
	case GMIME_HTML_TAG_BASE:
	case GMIME_HTML_TAG_BASE_FONT:
	case GMIME_HTML_TAG_BG_SOUND:
	case GMIME_HTML_TAG_LINK:
	case GMIME_HTML_TAG_META:
	case GMIME_HTML_TAG_TITLE:
	case GMIME_HTML_TAG_NO_SCRIPT:
	case GMIME_HTML_TAG_NO_FRAMES:
	case GMIME_HTML_TAG_STYLE:
	case GMIME_HTML_TAG_SCRIPT:
		return TRUE;
	default:
		return is_template (tag);
	}
}

static gboolean
white_space (const char *text)
{
	static const GMimeReaderByteSet nonspace = { { [0] = 4, [9] = 1, [10] = 1, [12] = 1, [13] = 1 }, TRUE };
	gsize length = strlen (text);

	return _g_mime_reader_find_byte ((const guint8 *) text, length, &nonspace) == length;
}

static void
end_head (HtmlState *state)
{
	if (state->head) {
		OpenTag *tag = pop (state, "head");

		if (tag)
			free_tag (tag);

		state->head = NULL;
	}

	state->head_done = TRUE;
}

static gboolean
process_end_tag (HtmlState *state, GMimeHtmlToken *token, gboolean implicit)
{
	GMimeHtmlTagId id = g_mime_html_token_get_tag_id (token);
	const char *name = g_mime_html_token_get_name (token);
	gboolean unwrap = state->options->noscript == GMIME_HTML_NO_SCRIPT_HANDLING_UNWRAP;
	gboolean popped = TRUE, ok = TRUE;
	OpenTag *tag;

	if (unwrap && id == GMIME_HTML_TAG_NO_SCRIPT)
		return TRUE;

	/* An ignored end tag leaves the browser's element open. Do not remove its
	 * context, or the eventual end tag could be written with the wrong flags. */
	if (!implicit &&
	    _g_mime_html_tokenizer_get_tag_disposition (state->tokenizer) == GMIME_HTML_TAG_DISPOSITION_IGNORED) {
		/* Removing noscript can make an otherwise ignored end tag take effect. */
		if (unwrap && _g_mime_html_tokenizer_get_is_noscript_open (state->tokenizer))
			return TRUE;

		tag = peek (state, name);
		popped = FALSE;
	} else {
		tag = pop (state, name);
	}

	if (tag) {
		GMimeHtmlTagContext *context = tag->context;

		if (!state->suppressed) {
			if (g_mime_html_tag_context_get_invoke_callback_for_end_tag (context)) {
				GMimeHtmlTagContext *end = g_mime_html_tag_context_new (token);

				g_mime_html_tag_context_set_invoke_callback_for_end_tag (end, TRUE);
				g_mime_html_tag_context_set_suppress_inner_content (end,
										    g_mime_html_tag_context_get_suppress_inner_content (
											    context));
				g_mime_html_tag_context_set_delete_end_tag (end,
									    g_mime_html_tag_context_get_delete_end_tag (
										    context));
				g_mime_html_tag_context_set_delete_tag (end, g_mime_html_tag_context_get_delete_tag (
										     context));
				ok = callback (state, end);
				g_mime_html_tag_context_free (end);
			} else if (!g_mime_html_tag_context_get_delete_end_tag (context)) {
				ok = g_mime_html_writer_write_end_tag (state->writer, name, state->error);
			}
		}

		if (popped) {
			if (state->head == tag)
				state->head = NULL;

			free_tag (tag);
		}
	} else if (!state->suppressed && !(state->options->fragment && structure (id))) {
		/* Self-closing HTML starts may have no context. Their explicit end tags
		 * are still necessary before closing the foreign elements beneath them. */
		GMimeHtmlTagContext *context = g_mime_html_tag_context_new (token);

		ok = callback (state, context);
		g_mime_html_tag_context_free (context);
	}

	return ok;
}

static gboolean
process_tag (HtmlState *state, GMimeHtmlToken *token)
{
	gsize count;
	const GMimeHtmlClosedElement *closed = _g_mime_html_tokenizer_get_closed_elements (state->tokenizer, &count);
	GMimeHtmlTagId id = g_mime_html_token_get_tag_id (token);
	gboolean end = g_mime_html_token_is_end_tag (token);
	gboolean empty = g_mime_html_token_is_empty_element (token);
	gboolean fragment = state->options->fragment;
	gssize last_foreign = (gssize) count - 1;
	GMimeHtmlTagContext *context;

	/* Explicitly close foreign elements, including HTML elements above them.
	 * Otherwise removing a breakout tag can change how output is interpreted.
	 * This also keeps conversion idempotent. */
	while (last_foreign >= 0 && !closed[last_foreign].is_foreign)
		last_foreign--;

	for (gssize i = 0; i <= last_foreign; i++) {
		GMimeHtmlToken *end_tag = g_mime_html_token_new_tag (closed[i].name, TRUE, FALSE, NULL, 0);
		gboolean ok = TRUE;

		for (guint n = 0; n < closed[i].count && ok; n++)
			ok = process_end_tag (state, end_tag, TRUE);

		g_mime_html_token_unref (end_tag);
		if (!ok)
			return FALSE;
	}

	_g_mime_html_tokenizer_clear_closed_elements (state->tokenizer);

	/* Ignored frameset tags must not become active after filtering the tokens
	 * that made the original document a frameset (or cleared frameset-ok). */
	if (_g_mime_html_tokenizer_get_in_frameset (state->tokenizer) && id != GMIME_HTML_TAG_FRAME_SET &&
	    id != GMIME_HTML_TAG_FRAME && id != GMIME_HTML_TAG_NO_FRAMES)
		return TRUE;

	if (id == GMIME_HTML_TAG_FRAME_SET && !end && !_g_mime_html_tokenizer_get_in_frameset (state->tokenizer))
		return TRUE;

	/* Unwrap noscript so output has the same interpretation with scripting on or off. */
	if (state->options->noscript == GMIME_HTML_NO_SCRIPT_HANDLING_UNWRAP && id == GMIME_HTML_TAG_NO_SCRIPT)
		return TRUE;

	if (!end) {
		if (fragment && !state->head_done) {
			if (!state->head) {
				if (!allowed_in_head (token))
					state->head_done = TRUE;
			} else if (is_template (token)) {
				state->head_template_depth++;
			} else if (!state->head_template_depth && !allowed_in_head (token)) {
				end_head (state);
			}
		}

		if (fragment && structure (id)) {
			if (id == GMIME_HTML_TAG_HEAD) {
				/* Browsers ignore self-closing syntax and subsequent head starts. */
				if (!state->head_done && !state->head) {
					context = g_mime_html_tag_context_new (token);
					g_mime_html_tag_context_set_suppress_inner_content (context, TRUE);
					g_mime_html_tag_context_set_delete_tag (context, TRUE);
					g_mime_html_tag_context_set_delete_end_tag (context, TRUE);
					state->head = push (state, context);
				}
			} else if (!empty) {
				context = g_mime_html_tag_context_new (token);
				g_mime_html_tag_context_set_delete_tag (context, TRUE);
				g_mime_html_tag_context_set_delete_end_tag (context, TRUE);
				push (state, context);
			}
		} else if (!empty ||
			   g_mime_html_tokenizer_get_state (state->tokenizer) != GMIME_HTML_TOKENIZER_STATE_DATA) {
			/* <style/> and <script/> still enter raw text: retain a context so
			 * suppression covers their content despite the self-closing flag. */
			context = g_mime_html_tag_context_new (token);

			if (!state->suppressed && !callback (state, context)) {
				g_mime_html_tag_context_free (context);
				return FALSE;
			}

			push (state, context);
		} else if (!state->suppressed) {
			gboolean ok;

			context = g_mime_html_tag_context_new (token);
			ok = callback (state, context);
			g_mime_html_tag_context_free (context);

			if (!ok)
				return FALSE;
		}

		state->raw_text = g_mime_html_tokenizer_get_state (state->tokenizer) != GMIME_HTML_TOKENIZER_STATE_DATA;
	} else {
		state->raw_text = FALSE;

		if (fragment && state->head) {
			if (state->head_template_depth) {
				if (is_template (token))
					state->head_template_depth--;
				else if (id == GMIME_HTML_TAG_HEAD)
					return TRUE;
			} else if (id == GMIME_HTML_TAG_HEAD) {
				gboolean ok = process_end_tag (state, token, FALSE);

				state->head = NULL;
				state->head_done = TRUE;

				return ok;
			} else if (id == GMIME_HTML_TAG_BODY || id == GMIME_HTML_TAG_HTML || id == GMIME_HTML_TAG_BR) {
				end_head (state);
			}
		}

		return process_end_tag (state, token, FALSE);
	}

	return TRUE;
}

static gboolean
write_header_footer (GMimeStream *output, const char *text, GMimeHeaderFooterFormat format, GError **error)
{
	GMimeTextConverter *converter;
	GMimeStream *input;
	gboolean ok;

	if (!text || !*text)
		return TRUE;

	if (format == GMIME_HEADER_FOOTER_FORMAT_HTML)
		return _g_mime_text_write (output, text, strlen (text), error);

	converter = g_mime_text_to_html_new ();
	g_mime_text_to_html_set_output_html_fragment (GMIME_TEXT_TO_HTML (converter), TRUE);

	input = g_mime_stream_mem_new_with_buffer (text, strlen (text));

	/* Use the UTF-8 vfunc; only the outer converter owns encoding and flushing. */
	ok = GMIME_TEXT_CONVERTER_GET_CLASS (converter)->convert (converter, input, output, error);

	g_object_unref (input);
	g_object_unref (converter);

	return ok;
}

static gboolean
convert_html_to_html (GMimeTextConverter *converter, GMimeStream *input, GMimeStream *output, GError **error)
{
	GMimeHtmlToHtml *options = GMIME_HTML_TO_HTML (converter);
	HtmlState state = { 0 };
	GMimeHtmlToken *token;
	GHashTableIter iter;
	gpointer value;
	gboolean ok = TRUE;

	if (!write_header_footer (output, g_mime_text_converter_get_header (converter), options->header_format, error))
		return FALSE;

	state.options = options;
	state.error = error;
	state.open = g_hash_table_new (g_str_hash, g_str_equal);
	state.tokenizer = g_mime_html_tokenizer_new (input);
	state.writer = g_mime_html_writer_new (output);

	g_mime_html_tokenizer_set_decode_character_references (state.tokenizer, FALSE);
	g_mime_html_tokenizer_set_scripting_enabled (state.tokenizer,
						     options->noscript ==
							     GMIME_HTML_NO_SCRIPT_HANDLING_SCRIPTING_ENABLED);
	g_mime_html_tokenizer_set_ignore_truncated_tags (state.tokenizer, TRUE);
	g_mime_html_tokenizer_set_max_element_depth (state.tokenizer, options->depth);
	g_mime_html_tokenizer_set_compliance_options (state.tokenizer, options->compliance);
	g_mime_html_tokenizer_set_compliance_logger (state.tokenizer, options->logger, options->logger_data, NULL);
	_g_mime_html_tokenizer_set_track_closed_elements (state.tokenizer, TRUE);

	while (ok) {
		ok = g_mime_html_tokenizer_read_next_token (state.tokenizer, &token, error);
		if (!ok || !token)
			break;

		GMimeHtmlTokenKind kind = g_mime_html_token_get_kind (token);

		if (kind == GMIME_HTML_TOKEN_TAG) {
			ok = process_tag (&state, token);
		} else if (kind == GMIME_HTML_TOKEN_COMMENT) {
			if (!options->comments && !state.suppressed)
				ok = g_mime_html_writer_write_token (state.writer, token, error);
		} else if (kind == GMIME_HTML_TOKEN_DOCTYPE) {
			if (!options->fragment && !state.suppressed)
				ok = g_mime_html_writer_write_token (state.writer, token, error);
		} else {
			if (options->fragment && !state.head_done && !state.raw_text && kind == GMIME_HTML_TOKEN_DATA &&
			    !state.head_template_depth && !white_space (g_mime_html_token_get_data (token)))
				end_head (&state);

			if (!state.suppressed)
				ok = g_mime_html_writer_write_token (state.writer, token, error);
		}

		g_mime_html_token_unref (token);
	}

	if (ok)
		ok = g_mime_html_writer_finish (state.writer, error);

	g_hash_table_iter_init (&iter, state.open);

	while (g_hash_table_iter_next (&iter, NULL, &value)) {
		OpenTag *tag = value;

		while (tag) {
			OpenTag *next = tag->next;

			free_tag (tag);
			tag = next;
		}
	}

	g_hash_table_destroy (state.open);
	g_object_unref (state.tokenizer);
	g_object_unref (state.writer);

	if (ok)
		ok = write_header_footer (output, g_mime_text_converter_get_footer (converter), options->footer_format,
					  error);
	return ok;
}

static void
finalize (GObject *object)
{
	GMimeHtmlToHtml *self = GMIME_HTML_TO_HTML (object);

	if (self->callback_destroy)
		self->callback_destroy (self->callback_data);

	if (self->logger_destroy)
		self->logger_destroy (self->logger_data);

	g_mime_html_compliance_options_unref (self->compliance);

	G_OBJECT_CLASS (g_mime_html_to_html_parent_class)->finalize (object);
}

static void
g_mime_html_to_html_class_init (GMimeHtmlToHtmlClass *klass)
{
	GMimeTextConverterClass *converter = GMIME_TEXT_CONVERTER_CLASS (klass);

	G_OBJECT_CLASS (klass)->finalize = finalize;

	converter->input_format = converter->output_format = GMIME_TEXT_FORMAT_HTML;
	converter->convert = convert_html_to_html;
}

static void
g_mime_html_to_html_init (GMimeHtmlToHtml *self)
{
	self->depth = 4096;
	self->compliance = g_mime_html_compliance_options_new ();
}

/**
 * g_mime_html_to_html_new:
 *
 * Creates an HTML rewriting converter. By default, comments and document
 * structure are retained, noscript elements are unwrapped, and headers and
 * footers are interpreted as plain text. This converter is not a sanitizer.
 *
 * Returns: (transfer full): a new #GMimeHtmlToHtml.
 **/
GMimeTextConverter *
g_mime_html_to_html_new (void)
{
	return g_object_new (GMIME_TYPE_HTML_TO_HTML, NULL);
}

/**
 * g_mime_html_to_html_get_filter_comments:
 * @converter: a #GMimeHtmlToHtml
 *
 * Gets whether HTML comments are omitted from the output.
 *
 * Returns: %TRUE if comments are filtered, or %FALSE if retained (the default).
 **/
gboolean
g_mime_html_to_html_get_filter_comments (GMimeHtmlToHtml *converter)
{
	g_return_val_if_fail (GMIME_IS_HTML_TO_HTML (converter), FALSE);

	return converter->comments;
}

/**
 * g_mime_html_to_html_set_filter_comments:
 * @converter: a #GMimeHtmlToHtml
 * @value: whether to omit HTML comments
 *
 * Enables or disables comment filtering. Removing comments does not make
 * the output safe for use as untrusted HTML.
 **/
void
g_mime_html_to_html_set_filter_comments (GMimeHtmlToHtml *converter, gboolean value)
{
	g_return_if_fail (GMIME_IS_HTML_TO_HTML (converter));

	converter->comments = !!value;
}

/**
 * g_mime_html_to_html_get_output_html_fragment:
 * @converter: a #GMimeHtmlToHtml
 *
 * Gets whether document structure is removed to produce a fragment.
 *
 * Returns: %TRUE for fragment output, or %FALSE to retain document structure
 *   (the default).
 **/
gboolean
g_mime_html_to_html_get_output_html_fragment (GMimeHtmlToHtml *converter)
{
	g_return_val_if_fail (GMIME_IS_HTML_TO_HTML (converter), FALSE);

	return converter->fragment;
}

/**
 * g_mime_html_to_html_set_output_html_fragment:
 * @converter: a #GMimeHtmlToHtml
 * @value: whether to produce an HTML fragment
 *
 * When enabled, omits the doctype, html and body tags, and the head element
 * with its contents. Configured headers and footers are still included.
 * When disabled, document structure from the input is retained rather
 * than synthesizing wrappers around an input fragment.
 **/
void
g_mime_html_to_html_set_output_html_fragment (GMimeHtmlToHtml *converter, gboolean value)
{
	g_return_if_fail (GMIME_IS_HTML_TO_HTML (converter));

	converter->fragment = !!value;
}

/**
 * g_mime_html_to_html_get_max_element_depth:
 * @converter: a #GMimeHtmlToHtml
 *
 * Gets the tokenizer's maximum number of distinct open-element stack entries.
 *
 * Returns: the element depth limit, initially 4096.
 **/
guint
g_mime_html_to_html_get_max_element_depth (GMimeHtmlToHtml *converter)
{
	g_return_val_if_fail (GMIME_IS_HTML_TO_HTML (converter), 0);

	return converter->depth;
}

/**
 * g_mime_html_to_html_set_max_element_depth:
 * @converter: a #GMimeHtmlToHtml
 * @value: the maximum element depth, from 1 to %G_MAXINT
 *
 * Limits the tokenizer's distinct open-element stack entries. After this
 * limit is exceeded, the remaining input is treated as literal text rather
 * than parsed as markup. A depth-limit diagnostic is reported when a
 * compliance logger is registered.
 **/
void
g_mime_html_to_html_set_max_element_depth (GMimeHtmlToHtml *converter, guint value)
{
	g_return_if_fail (GMIME_IS_HTML_TO_HTML (converter));
	g_return_if_fail (value > 0);

	converter->depth = value;
}

/**
 * g_mime_html_to_html_get_header_format:
 * @converter: a #GMimeHtmlToHtml
 *
 * Gets how the configured header is interpreted.
 *
 * Returns: the header format, initially %GMIME_HEADER_FOOTER_FORMAT_TEXT.
 **/
GMimeHeaderFooterFormat
g_mime_html_to_html_get_header_format (GMimeHtmlToHtml *converter)
{
	g_return_val_if_fail (GMIME_IS_HTML_TO_HTML (converter), 0);

	return converter->header_format;
}

/**
 * g_mime_html_to_html_set_header_format:
 * @converter: a #GMimeHtmlToHtml
 * @value: the format of the configured header
 *
 * Selects how the header set by g_mime_text_converter_set_header() is written.
 * Text is independently converted to HTML. HTML is copied verbatim without
 * filtering, sanitization or tag callbacks.
 **/
void
g_mime_html_to_html_set_header_format (GMimeHtmlToHtml *converter, GMimeHeaderFooterFormat value)
{
	g_return_if_fail (GMIME_IS_HTML_TO_HTML (converter));
	g_return_if_fail (value >= 0 && value <= GMIME_HEADER_FOOTER_FORMAT_HTML);

	converter->header_format = value;
}

/**
 * g_mime_html_to_html_get_footer_format:
 * @converter: a #GMimeHtmlToHtml
 *
 * Gets how the configured footer is interpreted.
 *
 * Returns: the footer format, initially %GMIME_HEADER_FOOTER_FORMAT_TEXT.
 **/
GMimeHeaderFooterFormat
g_mime_html_to_html_get_footer_format (GMimeHtmlToHtml *converter)
{
	g_return_val_if_fail (GMIME_IS_HTML_TO_HTML (converter), 0);

	return converter->footer_format;
}

/**
 * g_mime_html_to_html_set_footer_format:
 * @converter: a #GMimeHtmlToHtml
 * @value: the format of the configured footer
 *
 * Selects how the footer set by g_mime_text_converter_set_footer() is written.
 * Text is independently converted to HTML. HTML is copied verbatim without
 * filtering, sanitization or tag callbacks.
 **/
void
g_mime_html_to_html_set_footer_format (GMimeHtmlToHtml *converter, GMimeHeaderFooterFormat value)
{
	g_return_if_fail (GMIME_IS_HTML_TO_HTML (converter));
	g_return_if_fail (value >= 0 && value <= GMIME_HEADER_FOOTER_FORMAT_HTML);

	converter->footer_format = value;
}

/**
 * g_mime_html_to_html_get_no_script_handling:
 * @converter: a #GMimeHtmlToHtml
 *
 * Gets how noscript elements and their contents are parsed and written.
 *
 * Returns: the noscript mode, initially %GMIME_HTML_NO_SCRIPT_HANDLING_UNWRAP.
 **/
GMimeHtmlNoScriptHandling
g_mime_html_to_html_get_no_script_handling (GMimeHtmlToHtml *converter)
{
	g_return_val_if_fail (GMIME_IS_HTML_TO_HTML (converter), 0);

	return converter->noscript;
}

/**
 * g_mime_html_to_html_set_no_script_handling:
 * @converter: a #GMimeHtmlToHtml
 * @value: the noscript handling mode
 *
 * Selects noscript parsing and output. The default unwrap mode parses its
 * contents as HTML and removes the noscript tags independently of the
 * renderer's scripting setting. The other modes require the renderer's
 * scripting setting to match the selected mode. None of these modes makes
 * the converter a sanitizer.
 **/
void
g_mime_html_to_html_set_no_script_handling (GMimeHtmlToHtml *converter, GMimeHtmlNoScriptHandling value)
{
	g_return_if_fail (GMIME_IS_HTML_TO_HTML (converter));
	g_return_if_fail (value >= 0 && value <= GMIME_HTML_NO_SCRIPT_HANDLING_SCRIPTING_DISABLED);

	converter->noscript = value;
}

/**
 * g_mime_html_to_html_set_html_tag_callback:
 * @converter: a #GMimeHtmlToHtml
 * @callback: (nullable) (scope notified) (closure user_data) (destroy destroy): the HTML tag callback
 * @user_data: (nullable): data passed to the callback
 * @destroy: (nullable): destroy notifier for @user_data
 *
 * Sets the HTML tag callback, or restores default tag output if @callback
 * is %NULL. The callback must explicitly write any desired tag; end-tag
 * callbacks are opt-in through #GMimeHtmlTagContext. Headers and footers
 * do not invoke the callback.
 *
 * The previous destroy notifier is called when replacing the registration.
 * @destroy is called when this registration is replaced or the converter
 * is finalized. The callback cannot be changed during conversion.
 **/
void
g_mime_html_to_html_set_html_tag_callback (GMimeHtmlToHtml *converter, GMimeHtmlTagCallback callback,
					   gpointer user_data, GDestroyNotify destroy)
{
	g_return_if_fail (GMIME_IS_HTML_TO_HTML (converter));
	g_return_if_fail (!_g_mime_text_converter_is_active (GMIME_TEXT_CONVERTER (converter)));

	if (converter->callback_destroy)
		converter->callback_destroy (converter->callback_data);

	converter->callback = callback;
	converter->callback_data = user_data;
	converter->callback_destroy = destroy;
}

/**
 * g_mime_html_to_html_get_html_tag_callback: (skip)
 * @converter: a #GMimeHtmlToHtml
 * @user_data: (out) (optional) (nullable) (transfer none): the callback's borrowed user data
 *
 * Gets the currently registered HTML tag callback.
 *
 * Returns: (nullable): the HTML tag callback, or %NULL if none is registered.
 **/
GMimeHtmlTagCallback
g_mime_html_to_html_get_html_tag_callback (GMimeHtmlToHtml *converter, gpointer *user_data)
{
	g_return_val_if_fail (GMIME_IS_HTML_TO_HTML (converter), NULL);

	if (user_data)
		*user_data = converter->callback_data;

	return converter->callback;
}

/**
 * g_mime_html_to_html_set_compliance_logger:
 * @converter: a #GMimeHtmlToHtml
 * @logger: (nullable) (scope notified) (closure user_data) (destroy destroy): the compliance logger
 * @user_data: (nullable): data passed to the logger
 * @destroy: (nullable): destroy notifier for @user_data
 *
 * Sets the compliance logger, or clears it if @logger is %NULL. Diagnostics
 * are borrowed for the duration of the callback and do not change output.
 *
 * The previous destroy notifier is called when replacing the registration.
 * @destroy is called when this registration is replaced or the converter
 * is finalized. The logger cannot be changed during conversion.
 **/
void
g_mime_html_to_html_set_compliance_logger (GMimeHtmlToHtml *converter, GMimeHtmlComplianceLogger logger,
					   gpointer user_data, GDestroyNotify destroy)
{
	g_return_if_fail (GMIME_IS_HTML_TO_HTML (converter));
	g_return_if_fail (!_g_mime_text_converter_is_active (GMIME_TEXT_CONVERTER (converter)));

	if (converter->logger_destroy)
		converter->logger_destroy (converter->logger_data);

	converter->logger = logger;
	converter->logger_data = user_data;
	converter->logger_destroy = destroy;
}

/**
 * g_mime_html_to_html_get_compliance_logger: (skip)
 * @converter: a #GMimeHtmlToHtml
 * @user_data: (out) (optional) (nullable) (transfer none): the logger's borrowed user data
 *
 * Gets the currently registered HTML compliance logger.
 *
 * Returns: (nullable): the compliance logger, or %NULL if none is registered.
 **/
GMimeHtmlComplianceLogger
g_mime_html_to_html_get_compliance_logger (GMimeHtmlToHtml *converter, gpointer *user_data)
{
	g_return_val_if_fail (GMIME_IS_HTML_TO_HTML (converter), NULL);

	if (user_data)
		*user_data = converter->logger_data;

	return converter->logger;
}

/**
 * g_mime_html_to_html_get_compliance_options:
 * @converter: a #GMimeHtmlToHtml
 *
 * Gets the converter's HTML compliance options.
 *
 * Returns: (transfer none): the converter's compliance options.
 **/
GMimeHtmlComplianceOptions *
g_mime_html_to_html_get_compliance_options (GMimeHtmlToHtml *converter)
{
	g_return_val_if_fail (GMIME_IS_HTML_TO_HTML (converter), NULL);

	return converter->compliance;
}

/**
 * g_mime_html_to_html_set_compliance_options:
 * @converter: a #GMimeHtmlToHtml
 * @options: (transfer none): the HTML compliance options
 *
 * Sets the compliance options. The converter retains a reference to @options.
 **/
void
g_mime_html_to_html_set_compliance_options (GMimeHtmlToHtml *converter, GMimeHtmlComplianceOptions *options)
{
	g_return_if_fail (GMIME_IS_HTML_TO_HTML (converter));
	g_return_if_fail (options != NULL);

	g_mime_html_compliance_options_ref (options);
	g_mime_html_compliance_options_unref (converter->compliance);

	converter->compliance = options;
}
