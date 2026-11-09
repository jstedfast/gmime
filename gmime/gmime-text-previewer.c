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
#include <errno.h>

#include "gmime-text-previewer.h"
#include "gmime-text-private.h"
#include "gmime-stream-mem.h"
#include "gmime-html-tokenizer.h"
#include "gmime-reader-simd-private.h"
#include "gmime-error.h"

/**
 * SECTION: gmime-text-previewer
 * @title: Text previews
 * @short_description: Bounded previews of plain text and HTML
 *
 * Previewers produce UTF-8 plain text with collapsed whitespace and a
 * configurable length limit measured in Unicode scalar values. The HTML
 * previewer requires an explicit body element; use #GMimeHtmlToText to
 * render arbitrary HTML fragments.
 **/

typedef struct {
	guint maximum;
} GMimeTextPreviewerPrivate;

struct _GMimePlainTextPreviewer {
	GMimeTextPreviewer parent_object;
};

struct _GMimeHtmlTextPreviewer {
	GMimeTextPreviewer parent_object;
};

G_DEFINE_ABSTRACT_TYPE_WITH_PRIVATE (GMimeTextPreviewer, g_mime_text_previewer, G_TYPE_OBJECT)
G_DEFINE_TYPE (GMimePlainTextPreviewer, g_mime_plain_text_previewer, GMIME_TYPE_TEXT_PREVIEWER)
G_DEFINE_TYPE (GMimeHtmlTextPreviewer, g_mime_html_text_previewer, GMIME_TYPE_TEXT_PREVIEWER)

typedef struct {
	gunichar text[1024];
	guint length, maximum;
	gboolean whitespace;
} Preview;

static gboolean
is_whitespace (gunichar c, gboolean zero_width)
{
	/* Match .NET Char.IsWhiteSpace, which includes NEL but not U+001C..U+001F. */
	return (c >= 9 && c <= 13) || c == 0x85 || c == ' ' || g_unichar_type (c) == G_UNICODE_SPACE_SEPARATOR ||
	       g_unichar_type (c) == G_UNICODE_LINE_SEPARATOR || g_unichar_type (c) == G_UNICODE_PARAGRAPH_SEPARATOR ||
	       (zero_width && c >= 0x200b && c <= 0x200d);
}

static void
ellipsis (Preview *preview, gboolean trim)
{
	if (trim && preview->whitespace && preview->length)
		preview->length--;

	/* A one-character preview filled by whitespace can become empty after
	 * trimming. Avoid the upstream negative index in that boundary case. */
	if (!preview->length)
		preview->text[preview->length++] = 0x2026;
	else
		preview->text[preview->length - 1] = 0x2026;

	preview->whitespace = FALSE;
}

static gboolean
append (Preview *preview, const char *text, gsize length, gboolean zero_width, gboolean trim)
{
	const char *end = text + length;

	/* The shared byte-set search skips ordinary ASCII spans using SIMD. */
	static const GMimeReaderByteSet special = {
		{ 0xfb, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xfe, 0xfe, 0xfe, 0xfe, 0xfe, 0xff, 0xff }, TRUE
	};

	while (text < end && preview->length < preview->maximum) {
		gsize run = _g_mime_reader_find_byte ((const guint8 *) text, end - text, &special);

		while (run && (guchar) *text < 0x80 && preview->length < preview->maximum) {
			preview->text[preview->length++] = (guchar) *text++;
			preview->whitespace = FALSE;
			run--;
		}

		if (text == end || preview->length == preview->maximum)
			break;

		gunichar c = g_utf8_get_char (text);

		text = g_utf8_next_char (text);
		if (is_whitespace (c, zero_width)) {
			if (!preview->whitespace) {
				preview->text[preview->length++] = ' ';
				preview->whitespace = TRUE;
			}
		} else {
			preview->text[preview->length++] = c;
			preview->whitespace = FALSE;
		}
	}

	if (text < end) {
		ellipsis (preview, trim);
		return TRUE;
	}

	return FALSE;
}

static char *
finish (Preview *preview)
{
	GString *text = g_string_sized_new (preview->length);

	if (preview->whitespace && preview->length)
		preview->length--;

	for (guint i = 0; i < preview->length; i++)
		g_string_append_unichar (text, preview->text[i]);

	return g_string_free (text, FALSE);
}

static char *
plain_preview (GMimeTextPreviewer *previewer, GMimeStream *input, gboolean string_input, GError **error)
{
	GMimeTextReader reader = { .stream = input };
	Preview preview = { .maximum = g_mime_text_previewer_get_maximum_preview_length (previewer),
			    .whitespace = TRUE };

	while (TRUE) {
		if (!_g_mime_text_reader_read (&reader, error))
			return NULL;

		if (reader.position == reader.end)
			break;

		/* Preserve the source's string/reader distinction: zero-width spaces
		 * are collapsed only by the plain string overload, not the stream one. */
		if (append (&preview, reader.buffer + reader.position, reader.end - reader.position, string_input,
			    string_input))
			break;

		reader.position = reader.end;
	}

	return finish (&preview);
}

typedef struct _PreviewTag PreviewTag;

struct _PreviewTag {
	GMimeHtmlTagId id;
	guint list_index;
	gboolean suppressed;

	/* Document-order links and the next outer element with the same tag ID. */
	PreviewTag *outer, *inner, *same_id;
	guint64 sequence;
};

typedef struct {
	GHashTable *ids;
	PreviewTag *inner;
	guint64 sequence;
} PreviewStack;

/* Every operation is O(1). A flat list would make unmatched end tags and
 * repeated list items quadratic in the number of unclosed elements. */
static PreviewTag *
preview_peek (PreviewStack *stack, GMimeHtmlTagId id)
{
	return g_hash_table_lookup (stack->ids, GINT_TO_POINTER (id));
}

static void
preview_push (PreviewStack *stack, GMimeHtmlTagId id, gboolean suppressed)
{
	PreviewTag *tag = g_new0 (PreviewTag, 1);

	tag->id = id;
	tag->suppressed = suppressed;
	tag->sequence = stack->sequence++;
	tag->same_id = preview_peek (stack, id);
	g_hash_table_insert (stack->ids, GINT_TO_POINTER (id), tag);
	tag->outer = stack->inner;

	if (stack->inner)
		stack->inner->inner = tag;

	stack->inner = tag;
}

static void
preview_pop (PreviewStack *stack, GMimeHtmlTagId id)
{
	PreviewTag *tag = preview_peek (stack, id);

	if (!tag)
		return;

	if (tag->same_id)
		g_hash_table_insert (stack->ids, GINT_TO_POINTER (id), tag->same_id);
	else
		g_hash_table_remove (stack->ids, GINT_TO_POINTER (id));

	if (tag->inner)
		tag->inner->outer = tag->outer;
	else
		stack->inner = tag->outer;

	if (tag->outer)
		tag->outer->inner = tag->inner;

	g_free (tag);
}

static void
preview_clear (PreviewStack *stack)
{
	PreviewTag *tag = stack->inner;

	while (tag) {
		PreviewTag *outer = tag->outer;

		g_free (tag);
		tag = outer;
	}

	stack->inner = NULL;
	g_hash_table_remove_all (stack->ids);
}

static gboolean
preview_suppresses (GMimeHtmlTagId id)
{
	switch (id) {
	case GMIME_HTML_TAG_OL:
	case GMIME_HTML_TAG_UL:
	case GMIME_HTML_TAG_SCRIPT:
	case GMIME_HTML_TAG_STYLE:
	case GMIME_HTML_TAG_TABLE:
	case GMIME_HTML_TAG_T_BODY:
	case GMIME_HTML_TAG_T_HEAD:
	case GMIME_HTML_TAG_TR:
		return TRUE;
	default:
		return FALSE;
	}
}

static gboolean
append_prefixed (Preview *preview, const char **prefix, const char *text)
{
	char *value = g_strconcat (*prefix, text ? text : "", NULL);
	gboolean full = append (preview, value, strlen (value), TRUE, TRUE);

	*prefix = "";
	g_free (value);

	return full;
}

static char *
html_preview (GMimeTextPreviewer *previewer, GMimeStream *input, gboolean string_input, GError **error)
{
	GMimeHtmlTokenizer *tokenizer = g_mime_html_tokenizer_new (input);
	Preview preview = { .maximum = g_mime_text_previewer_get_maximum_preview_length (previewer),
			    .whitespace = TRUE };
	gboolean body = FALSE, full = FALSE, ok = TRUE;
	PreviewStack stack = { 0 };
	const char *prefix = "";
	GMimeHtmlToken *token;

	stack.ids = g_hash_table_new (g_direct_hash, g_direct_equal);
	g_mime_html_tokenizer_set_ignore_truncated_tags (tokenizer, TRUE);
	g_mime_html_tokenizer_set_decode_character_references (tokenizer, TRUE);

	while (!full) {
		ok = g_mime_html_tokenizer_read_next_token (tokenizer, &token, error);
		if (!ok || !token)
			break;

		GMimeHtmlTokenKind kind = g_mime_html_token_get_kind (token);

		if (kind == GMIME_HTML_TOKEN_TAG) {
			GMimeHtmlTagId id = g_mime_html_token_get_tag_id (token);

			if (!g_mime_html_token_is_end_tag (token)) {
				if (body) {
					if (id == GMIME_HTML_TAG_IMAGE) {
						const GMimeHtmlAttribute *alt =
							g_mime_html_token_lookup_attribute (token, "alt");

						if (alt)
							full = append_prefixed (&preview, &prefix,
										g_mime_html_attribute_get_value (alt));
					} else if (id == GMIME_HTML_TAG_LI) {
						PreviewTag *ol = preview_peek (&stack, GMIME_HTML_TAG_OL);
						PreviewTag *ul = preview_peek (&stack, GMIME_HTML_TAG_UL);
						PreviewTag *list = !ol ? ul :
								   !ul ? ol :
								   ol->sequence > ul->sequence ? ol :
												 ul;

						if (list) {
							if (list->id == GMIME_HTML_TAG_OL) {
								char *marker =
									g_strdup_printf (" %u. ", ++list->list_index);

								full = append (&preview, marker, strlen (marker), TRUE,
									       TRUE);
								g_free (marker);
								prefix = "";
							} else {
								prefix = " ";
							}
						}
					} else if (id == GMIME_HTML_TAG_BR || id == GMIME_HTML_TAG_P) {
						prefix = " ";
					}

					if (!g_mime_html_token_is_empty_element (token) ||
					    g_mime_html_tokenizer_get_state (tokenizer) !=
						    GMIME_HTML_TOKENIZER_STATE_DATA)
						preview_push (&stack, id, preview_suppresses (id));
				} else if (id == GMIME_HTML_TAG_BODY && !g_mime_html_token_is_empty_element (token)) {
					body = TRUE;
				}
			} else if (id == GMIME_HTML_TAG_BODY) {
				preview_clear (&stack);
				body = FALSE;
			} else {
				preview_pop (&stack, id);
			}
		} else if (kind == GMIME_HTML_TOKEN_DATA && body && !(stack.inner && stack.inner->suppressed)) {
			if (strlen (g_mime_html_token_get_data (token)) != g_mime_html_token_get_data_length (token)) {
				g_set_error_literal (error, G_CONVERT_ERROR, G_CONVERT_ERROR_EMBEDDED_NUL,
						     "The HTML preview contains an embedded NUL.");
				ok = FALSE;
			} else {
				full = append_prefixed (&preview, &prefix, g_mime_html_token_get_data (token));
			}
		}

		g_mime_html_token_unref (token);

		if (!ok)
			break;
	}

	preview_clear (&stack);
	g_hash_table_destroy (stack.ids);
	g_object_unref (tokenizer);

	return ok ? finish (&preview) : NULL;
}

static void
g_mime_text_previewer_class_init (GMimeTextPreviewerClass *klass)
{
}

static void
g_mime_text_previewer_init (GMimeTextPreviewer *self)
{
	GMimeTextPreviewerPrivate *priv = g_mime_text_previewer_get_instance_private (self);

	priv->maximum = 230;
}

static void
g_mime_plain_text_previewer_init (GMimePlainTextPreviewer *self)
{
}

static void
g_mime_html_text_previewer_init (GMimeHtmlTextPreviewer *self)
{
}

static void
g_mime_plain_text_previewer_class_init (GMimePlainTextPreviewerClass *klass)
{
	GMimeTextPreviewerClass *previewer = GMIME_TEXT_PREVIEWER_CLASS (klass);

	previewer->input_format = GMIME_TEXT_FORMAT_PLAIN;
	previewer->preview = plain_preview;
}

static void
g_mime_html_text_previewer_class_init (GMimeHtmlTextPreviewerClass *klass)
{
	GMimeTextPreviewerClass *previewer = GMIME_TEXT_PREVIEWER_CLASS (klass);

	previewer->input_format = GMIME_TEXT_FORMAT_HTML;
	previewer->preview = html_preview;
}

/**
 * g_mime_plain_text_previewer_new:
 *
 * Creates a plain-text previewer that collapses whitespace and produces
 * previews of at most 230 Unicode scalar values by default.
 *
 * Returns: (transfer full): a new #GMimePlainTextPreviewer.
 **/
GMimeTextPreviewer *
g_mime_plain_text_previewer_new (void)
{
	return g_object_new (GMIME_TYPE_PLAIN_TEXT_PREVIEWER, NULL);
}

/**
 * g_mime_html_text_previewer_new:
 *
 * Creates an HTML previewer that extracts text from an explicit body
 * element, suppresses script/style and structural list/table text, and
 * prefixes ordered list items. The default preview length is 230 Unicode
 * scalar values.
 *
 * Input without a body element produces an empty preview. Use
 * #GMimeHtmlToText to render arbitrary HTML fragments.
 *
 * Returns: (transfer full): a new #GMimeHtmlTextPreviewer.
 **/
GMimeTextPreviewer *
g_mime_html_text_previewer_new (void)
{
	return g_object_new (GMIME_TYPE_HTML_TEXT_PREVIEWER, NULL);
}

/**
 * g_mime_text_previewer_get_input_format:
 * @previewer: a #GMimeTextPreviewer
 *
 * Gets the text format accepted by @previewer.
 *
 * Returns: the previewer's input format.
 **/
GMimeTextFormat
g_mime_text_previewer_get_input_format (GMimeTextPreviewer *previewer)
{
	g_return_val_if_fail (GMIME_IS_TEXT_PREVIEWER (previewer), GMIME_TEXT_FORMAT_PLAIN);

	return GMIME_TEXT_PREVIEWER_GET_CLASS (previewer)->input_format;
}

/**
 * g_mime_text_previewer_get_maximum_preview_length:
 * @previewer: a #GMimeTextPreviewer
 *
 * Gets the maximum preview length, measured in Unicode scalar values
 * rather than bytes or UTF-16 code units.
 *
 * Returns: the maximum preview length, initially 230.
 **/
guint
g_mime_text_previewer_get_maximum_preview_length (GMimeTextPreviewer *previewer)
{
	GMimeTextPreviewerPrivate *priv;

	g_return_val_if_fail (GMIME_IS_TEXT_PREVIEWER (previewer), 0);

	priv = g_mime_text_previewer_get_instance_private (previewer);

	return priv->maximum;
}

/**
 * g_mime_text_previewer_set_maximum_preview_length:
 * @previewer: a #GMimeTextPreviewer
 * @maximum: the maximum number of Unicode scalar values, from 1 to 1024
 *
 * Sets the preview length limit. When text is truncated, the last output
 * character is replaced by an ellipsis, which counts toward this limit.
 **/
void
g_mime_text_previewer_set_maximum_preview_length (GMimeTextPreviewer *previewer, guint maximum)
{
	GMimeTextPreviewerPrivate *priv;

	g_return_if_fail (GMIME_IS_TEXT_PREVIEWER (previewer));
	g_return_if_fail (maximum >= 1 && maximum <= 1024);

	priv = g_mime_text_previewer_get_instance_private (previewer);
	priv->maximum = maximum;
}

/**
 * g_mime_text_previewer_get_preview_text:
 * @previewer: a #GMimeTextPreviewer
 * @text: NUL-terminated UTF-8 input in the previewer's input format
 * @error: return location for a #GError, or %NULL
 *
 * Generates a plain-text preview with collapsed whitespace. Plain-text
 * string previews treat U+200B through U+200D as whitespace; plain-text
 * stream previews preserve those characters. The returned string must
 * be freed with g_free().
 *
 * Returns: (transfer full) (nullable): a UTF-8 preview, or %NULL with
 *   @error set on failure.
 **/
char *
g_mime_text_previewer_get_preview_text (GMimeTextPreviewer *previewer, const char *text, GError **error)
{
	GMimeStream *input;
	char *preview;

	g_return_val_if_fail (GMIME_IS_TEXT_PREVIEWER (previewer), NULL);
	g_return_val_if_fail (text != NULL, NULL);
	g_return_val_if_fail (!error || !*error, NULL);

	input = g_mime_stream_mem_new_with_buffer (text, strlen (text));
	preview = GMIME_TEXT_PREVIEWER_GET_CLASS (previewer)->preview (previewer, input, TRUE, error);
	g_object_unref (input);

	return preview;
}

/**
 * g_mime_text_previewer_get_preview_text_from_stream:
 * @previewer: a #GMimeTextPreviewer
 * @stream: the input stream, positioned at the text to preview
 * @charset: the input character set
 * @error: return location for a #GError, or %NULL
 *
 * Generates a UTF-8 plain-text preview from @stream using @charset for
 * decoding. Unrecognized charsets fall back to UTF-8; byte-order-mark
 * detection is not performed. Invalid input encountered while decoding
 * and stream I/O failures are reported through @error.
 *
 * Reading can stop once the preview is full, but buffering may consume
 * additional input. The caller's stream is not reset or closed. The
 * returned string must be freed with g_free().
 *
 * Returns: (transfer full) (nullable): a UTF-8 preview, or %NULL with
 *   @error set on failure.
 **/
char *
g_mime_text_previewer_get_preview_text_from_stream (GMimeTextPreviewer *previewer, GMimeStream *stream,
						    const char *charset, GError **error)
{
	GMimeStream *input;
	char *preview;
	GIConv cd;

	g_return_val_if_fail (GMIME_IS_TEXT_PREVIEWER (previewer), NULL);
	g_return_val_if_fail (GMIME_IS_STREAM (stream), NULL);
	g_return_val_if_fail (charset != NULL, NULL);
	g_return_val_if_fail (!error || !*error, NULL);

	cd = g_iconv_open ("UTF-8", charset);
	if (cd == (GIConv) -1)
		charset = "UTF-8";
	else
		g_iconv_close (cd);

	input = _g_mime_text_stream_new_decoder (stream, charset, FALSE, 4096, error);
	if (!input)
		return NULL;

	preview = GMIME_TEXT_PREVIEWER_GET_CLASS (previewer)->preview (previewer, input, FALSE, error);
	g_object_unref (input);

	return preview;
}

/**
 * g_mime_text_previewer_get_preview_text_from_part:
 * @part: the text MIME part to preview
 * @error: return location for a #GError, or %NULL
 *
 * Reads at most 16 KiB of transfer-decoded content and applies
 * #GMimeTextPart's charset, byte-order-mark and fallback decoding.
 * Format=flowed text is unwrapped using the part's delsp parameter.
 * HTML parts use an HTML previewer; other text parts use a plain-text
 * previewer. The default maximum preview length of 230 Unicode scalar
 * values is used.
 *
 * A part without content produces an empty string. The returned string
 * must be freed with g_free().
 *
 * Returns: (transfer full) (nullable): a UTF-8 preview, or %NULL with
 *   @error set on failure.
 **/
char *
g_mime_text_previewer_get_preview_text_from_part (GMimeTextPart *part, GError **error)
{
	GMimeTextPreviewer *previewer;
	GMimeTextPart *prefix;
	GMimeContent *content;
	GMimeStream *decoded, *memory;
	const char *charset;
	char buffer[16 * 1024];
	gsize length = 0;
	char *text, *preview;

	g_return_val_if_fail (GMIME_IS_TEXT_PART (part), NULL);
	g_return_val_if_fail (!error || !*error, NULL);

	if (!g_mime_part_get_content (GMIME_PART (part)))
		return g_strdup ("");

	decoded = g_mime_content_open (g_mime_part_get_content (GMIME_PART (part)));
	if (!decoded) {
		g_set_error_literal (error, GMIME_ERROR, GMIME_ERROR_INVALID_OPERATION,
				     "Cannot open the text part content.");
		return NULL;
	}

	while (length < sizeof (buffer)) {
		ssize_t nread;

		errno = 0;
		nread = g_mime_stream_read (decoded, buffer + length, sizeof (buffer) - length);

		if (nread < 0 || (!nread && !g_mime_stream_eos (decoded))) {
			int code = errno ? errno : EIO;

			g_set_error (error, GMIME_ERROR, code, "Cannot read the text part content: %s",
				     g_strerror (code));
			g_object_unref (decoded);
			return NULL;
		}

		if (!nread)
			break;

		length += nread;
	}

	g_object_unref (decoded);

	/* Reuse the text part's checked charset/BOM/fallback decoding, but only
	 * for the bounded transfer-decoded prefix used by MimeKit previewers. */
	prefix = g_mime_text_part_new ();
	charset = g_mime_text_part_get_charset (part);

	if (charset)
		g_mime_text_part_set_charset (prefix, charset);

	memory = g_mime_stream_mem_new_with_buffer (buffer, length);
	content = g_mime_content_new_with_stream (memory, GMIME_CONTENT_ENCODING_UNSPECIFIED);
	g_mime_part_set_content (GMIME_PART (prefix), content);
	text = g_mime_text_part_get_text_with_encoding (prefix, NULL, error);
	g_object_unref (content);
	g_object_unref (memory);
	g_object_unref (prefix);

	if (!text)
		return NULL;

	if (g_mime_text_part_is_flowed (part)) {
		GMimeTextConverter *flowed = g_mime_flowed_to_text_new ();
		const char *delsp = g_mime_entity_get_content_type_parameter (GMIME_ENTITY (part), "delsp");
		char *unwrapped;

		g_mime_flowed_to_text_set_delete_space (GMIME_FLOWED_TO_TEXT (flowed),
							delsp && !g_ascii_strcasecmp (delsp, "yes"));
		unwrapped = g_mime_text_converter_convert_to_string (flowed, text, -1, error);
		g_object_unref (flowed);
		g_free (text);
		text = unwrapped;

		if (!text)
			return NULL;
	}

	previewer = g_mime_text_part_is_html (part) ? g_mime_html_text_previewer_new () :
						      g_mime_plain_text_previewer_new ();
	preview = g_mime_text_previewer_get_preview_text (previewer, text, error);
	g_object_unref (previewer);
	g_free (text);

	return preview;
}
