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

#include <errno.h>
#include <string.h>

#include "gmime-enriched-to-html.h"
#include "gmime-text-private.h"
#include "gmime-error.h"

/**
 * SECTION: gmime-enriched-to-html
 * @title: Enriched text to HTML
 * @short_description: Bounded streaming text/enriched conversion
 *
 * Implements MimeKit's RFC 1896 command, parameter, and formatting model.
 * Unknown commands retain their nesting without emitting arbitrary markup.
 * Parameter values are validated before use in CSS or language attributes.
 * Input, parameter, and output limits count Unicode scalar values; output
 * limits include wrappers, escaped content, headers, and footers.
 * CRLF and CR are normalized to LF during parsing.
 **/

struct _GMimeEnrichedToHtml {
	GMimeTextConverter parent_object;
	gboolean fragment;
	GMimeHeaderFooterFormat header_format, footer_format;
	guint max_depth, max_parameter;
	gsize max_input, max_output;
};

G_DEFINE_FINAL_TYPE (GMimeEnrichedToHtml, g_mime_enriched_to_html, GMIME_TYPE_TEXT_CONVERTER)

/* Bound the complete output, including headers and footers, before forwarding
 * each write. The wrapper never closes or flushes its borrowed destination. */
typedef struct {
	GMimeStream parent_object;
	GMimeStream *destination;
	gsize remaining;
	gboolean exceeded;
} EnrichedLimitedStream;

typedef GMimeStreamClass EnrichedLimitedStreamClass;

G_DEFINE_FINAL_TYPE (EnrichedLimitedStream, enriched_limited_stream, GMIME_TYPE_STREAM)

static ssize_t
limited_write (GMimeStream *base, const char *text, size_t length)
{
	EnrichedLimitedStream *stream = (EnrichedLimitedStream *) base;
	gsize count = 0;
	ssize_t written;

	/* Count UTF-8 leading bytes, not UTF-16 code units. This also handles a
	 * short downstream write ending within a multibyte scalar correctly. */
	for (gsize i = 0; i < length; i++)
		count += (((guint8) text[i] & 0xc0) != 0x80);

	if (count > stream->remaining) {
		stream->exceeded = TRUE;
		errno = EFBIG;
		return -1;
	}

	written = g_mime_stream_write (stream->destination, text, length);
	if (written > 0) {
		for (ssize_t i = 0; i < written; i++)
			stream->remaining -= (((guint8) text[i] & 0xc0) != 0x80);
	}

	return written;
}

static int
limited_noop (GMimeStream *stream)
{
	(void) stream;
	return 0;
}

static void
enriched_limited_stream_class_init (EnrichedLimitedStreamClass *klass)
{
	klass->write = limited_write;
	klass->flush = limited_noop;
	klass->close = limited_noop;
}

static void
enriched_limited_stream_init (EnrichedLimitedStream *stream)
{
	g_mime_stream_construct (GMIME_STREAM (stream), 0, -1);
}

typedef struct {
	gboolean bold, italic, underline, nofill;
	gint64 size, left, right, first_line, excerpt;
	const char *font, *color, *language, *alignment;
} Formatting;

typedef struct _Command {
	char *name;
	guint index;
	Formatting previous;
	struct _Command *same_name;
} Command;

typedef struct {
	GMimeEnrichedToHtml *options;
	GMimeHtmlWriter *html;
	GMimeTextReader reader;
	GError **error;
	GPtrArray *commands, *strings;
	GHashTable *contexts;
	GString *parameter, *text, *css;
	Formatting state, last_line;
	Command *parameter_target;
	gsize input_length, newlines, parameter_length;
	guint parameter_depth, text_length;
	gint pending;
	gboolean paragraph_open, span_open, invalid_parameter, implicit_break, trailing_newline, failed;
} Enriched;

static gboolean
limit_error (Enriched *enriched, const char *limit)
{
	g_set_error (enriched->error, GMIME_ERROR, GMIME_ERROR_PARSE_ERROR,
		     "The enriched text %s limit has been exceeded", limit);
	enriched->failed = TRUE;
	return FALSE;
}

static gint
read_raw (Enriched *enriched)
{
	gunichar c;

	if (!_g_mime_text_reader_read (&enriched->reader, enriched->error)) {
		enriched->failed = TRUE;
		return -1;
	}

	if (enriched->reader.position == enriched->reader.end)
		return -1;

	if (enriched->input_length == enriched->options->max_input) {
		limit_error (enriched, "input length");
		return -1;
	}

	c = g_utf8_get_char (enriched->reader.buffer + enriched->reader.position);
	enriched->reader.position =
		g_utf8_next_char (enriched->reader.buffer + enriched->reader.position) - enriched->reader.buffer;
	enriched->input_length++;

	return c;
}

static gint
read_character (Enriched *enriched)
{
	gint c;

	if (enriched->pending != -1) {
		c = enriched->pending;
		enriched->pending = -1;
	} else {
		c = read_raw (enriched);
	}

	if (c != '\r')
		return c;

	c = read_raw (enriched);

	if (c != '\n')
		enriched->pending = c;

	return '\n';
}

static gboolean
is_block (const char *name)
{
	return !strcmp (name, "center") || !strcmp (name, "flushleft") || !strcmp (name, "flushright") ||
	       !strcmp (name, "flushboth") || !strcmp (name, "paraindent") || !strcmp (name, "nofill") ||
	       !strcmp (name, "excerpt");
}

static const char *
style (Enriched *enriched)
{
	if (enriched->css->len)
		g_string_truncate (enriched->css, enriched->css->len - 1);

	return enriched->css->str;
}

static void
append_indent (Enriched *enriched, const char *property, gint64 count)
{
	if (count)
		g_string_append_printf (enriched->css, "%s: %" G_GINT64_FORMAT "ch; ", property, count * 4);
}

static gboolean
open_paragraph (Enriched *enriched)
{
	if (enriched->paragraph_open)
		return TRUE;

	if (!g_mime_html_writer_write_start_tag (enriched->html, "div", enriched->error))
		return FALSE;

	g_string_truncate (enriched->css, 0);

	if (enriched->state.nofill)
		g_string_append (enriched->css, "white-space: pre; ");

	if (enriched->state.nofill && !g_strcmp0 (enriched->state.alignment, "justify"))
		g_string_append (enriched->css, "text-align: left; ");
	else if (enriched->state.alignment)
		g_string_append_printf (enriched->css, "text-align: %s; ", enriched->state.alignment);

	append_indent (enriched, "margin-left",
		       enriched->state.left + enriched->state.excerpt + MAX ((gint64) 0, -enriched->state.first_line));
	append_indent (enriched, "margin-right", enriched->state.right);
	append_indent (enriched, "text-indent", enriched->state.first_line);

	if (enriched->css->len) {
		if (!g_mime_html_writer_write_attribute (enriched->html, "style", style (enriched), enriched->error))
			return FALSE;
	}

	enriched->paragraph_open = TRUE;

	return TRUE;
}

static gboolean
open_span (Enriched *enriched)
{
	if (enriched->span_open)
		return TRUE;

	g_string_truncate (enriched->css, 0);

	if (enriched->state.bold)
		g_string_append (enriched->css, "font-weight: bold; ");

	if (enriched->state.italic)
		g_string_append (enriched->css, "font-style: italic; ");

	if (enriched->state.underline)
		g_string_append (enriched->css, "text-decoration: underline; ");

	if (enriched->state.font)
		g_string_append_printf (enriched->css, "font-family: %s; ", enriched->state.font);

	if (enriched->state.color)
		g_string_append_printf (enriched->css, "color: %s; ", enriched->state.color);

	if (enriched->state.size) {
		char number[G_ASCII_DTOSTR_BUF_SIZE], *end;
		double size = MAX (1.0 / 6, MIN (12, 1 + enriched->state.size / 6.0));

		g_ascii_formatd (number, sizeof number, "%.3f", size);
		end = number + strlen (number);
		while (end > number && end[-1] == '0')
			*--end = 0;

		if (end > number && end[-1] == '.')
			*--end = 0;

		g_string_append_printf (enriched->css, "font-size: %sem; ", number);
	}

	if (!enriched->css->len && !enriched->state.language)
		return TRUE;

	if (!g_mime_html_writer_write_start_tag (enriched->html, "span", enriched->error))
		return FALSE;

	if (enriched->css->len &&
	    !g_mime_html_writer_write_attribute (enriched->html, "style", style (enriched), enriched->error))
		return FALSE;

	if (enriched->state.language &&
	    !g_mime_html_writer_write_attribute (enriched->html, "lang", enriched->state.language, enriched->error))
		return FALSE;

	enriched->span_open = TRUE;

	return TRUE;
}

static gboolean
flush_text (Enriched *enriched)
{
	if (!enriched->text->len)
		return TRUE;

	if (!open_paragraph (enriched))
		return FALSE;

	if (!open_span (enriched))
		return FALSE;

	if (!g_mime_html_writer_write_text (enriched->html, enriched->text->str, enriched->text->len, enriched->error))
		return FALSE;

	g_string_truncate (enriched->text, 0);
	enriched->text_length = 0;

	return TRUE;
}

static gboolean
close_span (Enriched *enriched)
{
	if (!flush_text (enriched))
		return FALSE;

	if (enriched->span_open) {
		if (!g_mime_html_writer_write_end_tag (enriched->html, "span", enriched->error))
			return FALSE;

		enriched->span_open = FALSE;
	}

	return TRUE;
}

static gboolean
close_paragraph (Enriched *enriched)
{
	if (!close_span (enriched))
		return FALSE;

	if (enriched->paragraph_open) {
		if (!g_mime_html_writer_write_end_tag (enriched->html, "div", enriched->error))
			return FALSE;

		enriched->paragraph_open = FALSE;
	}

	return TRUE;
}

static gboolean
append_text (Enriched *enriched, gunichar c)
{
	enriched->implicit_break = enriched->trailing_newline = FALSE;

	g_string_append_unichar (enriched->text, c);

	/* A Unicode scalar is always appended whole; never split its UTF-8 bytes. */
	return ++enriched->text_length < 1024 || flush_text (enriched);
}

static gboolean
break_paragraph (Enriched *enriched)
{
	if (enriched->paragraph_open || enriched->text->len)
		enriched->implicit_break = TRUE;

	return close_paragraph (enriched);
}

static gboolean
flush_newlines (Enriched *enriched)
{
	if (!enriched->newlines)
		return TRUE;

	if (!enriched->state.nofill && enriched->newlines == 1) {
		if (!append_text (enriched, ' '))
			return FALSE;
	} else {
		gsize count = enriched->state.nofill ? enriched->newlines : enriched->newlines - 1;

		if (enriched->implicit_break && count)
			count--;

		enriched->implicit_break = FALSE;

		for (gsize i = 0; i < count; i++) {
			if (!flush_text (enriched))
				return FALSE;

			if (!enriched->paragraph_open) {
				if (!open_paragraph (enriched))
					return FALSE;

				if (!g_mime_html_writer_write_empty_element_tag (enriched->html, "br", enriched->error))
					return FALSE;
			}

			if (!close_paragraph (enriched))
				return FALSE;
		}

		enriched->trailing_newline = TRUE;
		enriched->last_line = enriched->state;
	}

	enriched->newlines = 0;

	return TRUE;
}

static gboolean
count_parameter (Enriched *enriched, gsize count)
{
	if (count > enriched->options->max_parameter - enriched->parameter_length)
		return limit_error (enriched, "parameter length");

	enriched->parameter_length += count;

	return TRUE;
}

static gboolean
character (Enriched *enriched, gunichar c)
{
	if (enriched->parameter_depth) {
		if (!count_parameter (enriched, 1))
			return FALSE;

		if (enriched->parameter_target && !enriched->invalid_parameter)
			g_string_append_unichar (enriched->parameter, c);

		return TRUE;
	}

	enriched->parameter_target = NULL;

	if (c == '\n') {
		enriched->newlines++;
	} else {
		if (!flush_newlines (enriched))
			return FALSE;

		if (!append_text (enriched, c))
			return FALSE;
	}

	return TRUE;
}

static gboolean
check_depth (Enriched *enriched, guint extra)
{
	if (extra >= enriched->options->max_depth || enriched->commands->len >= enriched->options->max_depth - extra)
		return limit_error (enriched, "command depth");

	return TRUE;
}

static gboolean
start_command (Enriched *enriched, const char *name)
{
	Command *command;

	if (!check_depth (enriched, 0))
		return FALSE;

	if (!strcmp (name, "param")) {
		enriched->parameter_depth = 1;
		enriched->parameter_length = 0;
		enriched->invalid_parameter = FALSE;
		g_string_truncate (enriched->parameter, 0);
		return TRUE;
	}

	enriched->parameter_target = NULL;
	if (!(is_block (name) ? break_paragraph (enriched) : close_span (enriched)))
		return FALSE;

	command = g_new (Command, 1);
	command->name = g_strdup (name);
	command->index = enriched->commands->len;
	command->previous = enriched->state;
	command->same_name = g_hash_table_lookup (enriched->contexts, name);

	g_ptr_array_add (enriched->commands, command);
	g_hash_table_insert (enriched->contexts, command->name, command);

	if (!strcmp (name, "bold"))
		enriched->state.bold = TRUE;
	else if (!strcmp (name, "italic"))
		enriched->state.italic = TRUE;
	else if (!strcmp (name, "underline"))
		enriched->state.underline = TRUE;
	else if (!strcmp (name, "fixed"))
		enriched->state.font = "monospace";
	else if (!strcmp (name, "bigger"))
		enriched->state.size++;
	else if (!strcmp (name, "smaller"))
		enriched->state.size--;
	else if (!strcmp (name, "nofill"))
		enriched->state.nofill = TRUE;
	else if (!strcmp (name, "center"))
		enriched->state.alignment = "center";
	else if (!strcmp (name, "flushleft"))
		enriched->state.alignment = "left";
	else if (!strcmp (name, "flushright"))
		enriched->state.alignment = "right";
	else if (!strcmp (name, "flushboth"))
		enriched->state.alignment = "justify";
	else if (!strcmp (name, "excerpt"))
		enriched->state.excerpt++;
	else if (!strcmp (name, "fontfamily") || !strcmp (name, "color") || !strcmp (name, "lang") ||
		 !strcmp (name, "paraindent"))
		enriched->parameter_target = command;

	return TRUE;
}

static void
command_free (gpointer data)
{
	Command *command = data;

	g_free (command->name);
	g_free (command);
}

static gboolean
close_commands (Enriched *enriched, guint index)
{
	while (enriched->commands->len > index) {
		Command *command = g_ptr_array_index (enriched->commands, enriched->commands->len - 1);

		if (!(is_block (command->name) ? break_paragraph (enriched) : close_span (enriched)))
			return FALSE;

		enriched->state = command->previous;

		if (command->same_name)
			g_hash_table_replace (enriched->contexts, command->same_name->name, command->same_name);
		else
			g_hash_table_remove (enriched->contexts, command->name);

		g_ptr_array_remove_index (enriched->commands, enriched->commands->len - 1);
	}

	return TRUE;
}

static const char *
keep_string (Enriched *enriched, char *text)
{
	g_ptr_array_add (enriched->strings, text);
	return text;
}

static char *
parse_color (const char *value)
{
	static const char *names[] = { "red", "blue", "green", "yellow", "cyan", "magenta", "black", "white" };
	guint rgb = 0;

	for (guint i = 0; i < G_N_ELEMENTS (names); i++) {
		if (!g_ascii_strcasecmp (value, names[i]))
			return g_strdup (names[i]);
	}

	if (strlen (value) != 14 || value[4] != ',' || value[9] != ',')
		return NULL;

	for (guint i = 0; i < 3; i++) {
		guint component = 0;

		for (guint j = 0; j < 4; j++) {
			gint digit = g_ascii_xdigit_value (value[i * 5 + j]);

			if (digit == -1)
				return NULL;

			component = (component << 4) | digit;
		}

		rgb = (rgb << 8) | ((component + 128) / 257);
	}

	return g_strdup_printf ("#%06x", rgb);
}

static gboolean
is_font (const char *value)
{
	if (!*value)
		return FALSE;

	for (const char *p = value; *p; p = g_utf8_next_char (p)) {
		gunichar c = g_utf8_get_char (p);

		if (!g_unichar_isalnum (c) && c != ' ' && c != '-' && c != '_' && c != '.')
			return FALSE;
	}

	return TRUE;
}

static gboolean
is_language (const char *value)
{
	guint length = 0;
	gboolean first = TRUE;

	for (const char *p = value; *p; p++) {
		if (*p == '-') {
			if (!length)
				return FALSE;

			first = FALSE;
			length = 0;
		} else {
			if (!g_ascii_isalpha (*p) && (first || !g_ascii_isdigit (*p)))
				return FALSE;

			if (++length > 8)
				return FALSE;
		}
	}

	return length > 0;
}

static char *
unicode_trim (const char *text)
{
	const char *start = text, *end = text + strlen (text), *p;

	while (*start && g_unichar_isspace (g_utf8_get_char (start)))
		start = g_utf8_next_char (start);

	while (end > start) {
		p = g_utf8_find_prev_char (start, end);

		if (!g_unichar_isspace (g_utf8_get_char (p)))
			break;

		end = p;
	}

	return g_strndup (start, end - start);
}

static void
apply_indent (Enriched *enriched, const char *value)
{
	gint64 left = 0, right = 0, inward = 0, outward = 0;
	char **items = g_strsplit (value, ",", -1);
	gboolean valid = TRUE;

	for (guint i = 0; items[i]; i++) {
		char *part = unicode_trim (items[i]);

		if (!g_ascii_strcasecmp (part, "left"))
			left++;
		else if (!g_ascii_strcasecmp (part, "right"))
			right++;
		else if (!g_ascii_strcasecmp (part, "in"))
			inward++;
		else if (!g_ascii_strcasecmp (part, "out"))
			outward++;
		else
			valid = FALSE;

		g_free (part);

		if (!valid)
			break;
	}

	g_strfreev (items);

	if (!valid || (inward && outward))
		return;

	enriched->state.left += left;
	enriched->state.right += right;
	enriched->state.first_line += inward - outward;
}

static void
apply_parameter (Enriched *enriched)
{
	char *value, *color;
	const char *name;

	if (!enriched->parameter_target || enriched->invalid_parameter)
		return;

	value = unicode_trim (enriched->parameter->str);
	name = enriched->parameter_target->name;

	if (!strcmp (name, "color")) {
		if ((color = parse_color (value)))
			enriched->state.color = keep_string (enriched, color);
	} else if (!strcmp (name, "fontfamily")) {
		if (is_font (value))
			enriched->state.font = keep_string (enriched, g_strdup_printf ("'%s'", value));
	} else if (!strcmp (name, "lang")) {
		if (is_language (value))
			enriched->state.language = keep_string (enriched, g_strdup (value));
	} else if (!strcmp (name, "paraindent")) {
		apply_indent (enriched, value);
	}

	g_free (value);
}

static gboolean
process_command (Enriched *enriched, const char *name, gboolean end_tag)
{
	if (enriched->parameter_depth) {
		if (!strcmp (name, "param") && end_tag) {
			if (!--enriched->parameter_depth) {
				apply_parameter (enriched);
				enriched->parameter_target = NULL;
				g_string_truncate (enriched->parameter, 0);
				return TRUE;
			}
		} else if (!strcmp (name, "param")) {
			if (!check_depth (enriched, enriched->parameter_depth))
				return FALSE;

			enriched->parameter_depth++;
		}

		enriched->invalid_parameter = TRUE;
		return count_parameter (enriched, strlen (name) + (end_tag ? 3 : 2));
	}

	if (!flush_newlines (enriched))
		return FALSE;

	if (end_tag) {
		Command *command = g_hash_table_lookup (enriched->contexts, name);

		enriched->parameter_target = NULL;
		if (command && !close_commands (enriched, command->index))
			return FALSE;
	} else {
		if (!start_command (enriched, name))
			return FALSE;
	}

	return TRUE;
}

static gboolean
read_command (Enriched *enriched)
{
	char name[61];
	guint length = 0;
	gint c = read_character (enriched);
	gboolean end_tag;

	if (enriched->failed)
		return FALSE;

	if (c == '<') {
		if (enriched->parameter_depth && !count_parameter (enriched, 1))
			return FALSE;

		return character (enriched, '<');
	}

	end_tag = c == '/';
	if (end_tag)
		c = read_character (enriched);

	while (c != -1 && length < 60 && c < 128 && (g_ascii_isalnum (c) || c == '-')) {
		name[length++] = c;
		c = read_character (enriched);
	}

	if (enriched->failed)
		return FALSE;

	if (length && c == '>') {
		name[length] = 0;
		for (guint i = 0; i < length; i++)
			name[i] = g_ascii_tolower (name[i]);

		return process_command (enriched, name, end_tag);
	}

	if (!character (enriched, '<'))
		return FALSE;

	if (end_tag && !character (enriched, '/'))
		return FALSE;

	for (guint i = 0; i < length; i++)
		if (!character (enriched, name[i]))
			return FALSE;

	return c == -1 || character (enriched, c);
}

static gboolean
enriched_body (Enriched *enriched)
{
	gint c;

	if (!g_mime_html_writer_write_start_tag (enriched->html, "div", enriched->error))
		return FALSE;

	if (!g_mime_html_writer_write_attribute (enriched->html, "style", "white-space: pre-wrap;", enriched->error))
		return FALSE;

	while ((c = read_character (enriched)) != -1) {
		if (enriched->failed)
			return FALSE;

		if (!(c == '<' ? read_command (enriched) : character (enriched, c)))
			return FALSE;
	}

	if (enriched->failed)
		return FALSE;

	if (!flush_newlines (enriched))
		return FALSE;

	if (!close_commands (enriched, 0))
		return FALSE;

	if (!close_paragraph (enriched))
		return FALSE;

	if (enriched->trailing_newline) {
		enriched->state = enriched->last_line;

		if (!open_paragraph (enriched))
			return FALSE;

		if (!g_mime_html_writer_write_empty_element_tag (enriched->html, "br", enriched->error))
			return FALSE;

		if (!close_paragraph (enriched))
			return FALSE;
	}

	if (!g_mime_html_writer_write_end_tag (enriched->html, "div", enriched->error))
		return FALSE;

	return g_mime_html_writer_finish (enriched->html, enriched->error);
}

static gboolean
enriched_convert (GMimeTextConverter *base, GMimeStream *input, GMimeStream *output, GError **error)
{
	GMimeEnrichedToHtml *self = GMIME_ENRICHED_TO_HTML (base);
	EnrichedLimitedStream *limited = g_object_new (enriched_limited_stream_get_type (), NULL);
	Enriched enriched = { 0 };
	gboolean result;

	limited->destination = output;
	limited->remaining = self->max_output;

	enriched.options = self;
	enriched.html = g_mime_html_writer_new (GMIME_STREAM (limited));
	enriched.reader.stream = input;
	enriched.error = error;
	enriched.pending = -1;
	enriched.commands = g_ptr_array_new_with_free_func (command_free);
	enriched.strings = g_ptr_array_new_with_free_func (g_free);
	enriched.contexts = g_hash_table_new (g_str_hash, g_str_equal);
	enriched.parameter = g_string_new (NULL);
	enriched.text = g_string_new (NULL);
	enriched.css = g_string_new (NULL);

	result =
		(self->fragment || g_mime_html_writer_write_markup_text (enriched.html, "<html><body>", -1, error)) &&
		_g_mime_html_output_header_footer (enriched.html, g_mime_text_converter_get_header (base),
						   self->header_format, error) &&
		enriched_body (&enriched) &&
		_g_mime_html_output_header_footer (enriched.html, g_mime_text_converter_get_footer (base),
						   self->footer_format, error) &&
		(self->fragment || g_mime_html_writer_write_markup_text (enriched.html, "</body></html>", -1, error)) &&
		g_mime_html_writer_finish (enriched.html, error);

	if (!result && limited->exceeded) {
		if (error)
			g_clear_error (error);

		limit_error (&enriched, "output length");
	} else if (!result) {
		/* Preserve a charset wrapper's original conversion error through the
		 * output-limit stream rather than reducing it to errno. */
		_g_mime_text_stream_propagate_error (output, error);
	}

	g_hash_table_unref (enriched.contexts);
	g_ptr_array_unref (enriched.commands);
	g_ptr_array_unref (enriched.strings);
	g_string_free (enriched.parameter, TRUE);
	g_string_free (enriched.text, TRUE);
	g_string_free (enriched.css, TRUE);
	g_object_unref (enriched.html);
	g_object_unref (limited);

	return result;
}

static void
g_mime_enriched_to_html_class_init (GMimeEnrichedToHtmlClass *klass)
{
	GMimeTextConverterClass *base = GMIME_TEXT_CONVERTER_CLASS (klass);

	base->input_format = GMIME_TEXT_FORMAT_ENRICHED;
	base->output_format = GMIME_TEXT_FORMAT_HTML;
	base->convert = enriched_convert;
}

static void
g_mime_enriched_to_html_init (GMimeEnrichedToHtml *self)
{
	self->max_depth = self->max_parameter = 4096;
	self->max_input = 16 * 1024 * 1024;
	self->max_output = 64 * 1024 * 1024;
}

/**
 * g_mime_enriched_to_html_new:
 *
 * Creates an RFC 1896 text/enriched to HTML converter. By default, the
 * output includes html and body wrappers, and headers and footers are
 * interpreted as plain text.
 *
 * Returns: (transfer full): a new #GMimeEnrichedToHtml.
 **/
GMimeTextConverter *
g_mime_enriched_to_html_new (void)
{
	return g_object_new (GMIME_TYPE_ENRICHED_TO_HTML, NULL);
}

/**
 * g_mime_enriched_to_html_get_output_html_fragment:
 * @converter: a #GMimeEnrichedToHtml
 *
 * Gets whether the generated html and body wrappers are omitted.
 *
 * Returns: %TRUE for fragment output, or %FALSE for document output (the default).
 **/
gboolean
g_mime_enriched_to_html_get_output_html_fragment (GMimeEnrichedToHtml *converter)
{
	g_return_val_if_fail (GMIME_IS_ENRICHED_TO_HTML (converter), 0);

	return converter->fragment;
}

/**
 * g_mime_enriched_to_html_set_output_html_fragment:
 * @converter: a #GMimeEnrichedToHtml
 * @value: whether to omit the generated html and body wrappers
 *
 * Selects fragment or document output. The div used to preserve whitespace,
 * as well as configured headers and footers, is included in either mode.
 * This setting cannot be changed during conversion.
 **/
void
g_mime_enriched_to_html_set_output_html_fragment (GMimeEnrichedToHtml *converter, gboolean value)
{
	g_return_if_fail (GMIME_IS_ENRICHED_TO_HTML (converter));
	g_return_if_fail (!_g_mime_text_converter_is_active (GMIME_TEXT_CONVERTER (converter)));

	converter->fragment = value;
}

/**
 * g_mime_enriched_to_html_get_header_format:
 * @converter: a #GMimeEnrichedToHtml
 *
 * Gets how the configured header is interpreted.
 *
 * Returns: the header format, initially %GMIME_HEADER_FOOTER_FORMAT_TEXT.
 **/
GMimeHeaderFooterFormat
g_mime_enriched_to_html_get_header_format (GMimeEnrichedToHtml *converter)
{
	g_return_val_if_fail (GMIME_IS_ENRICHED_TO_HTML (converter), 0);

	return converter->header_format;
}

/**
 * g_mime_enriched_to_html_set_header_format:
 * @converter: a #GMimeEnrichedToHtml
 * @value: the format of the configured header
 *
 * Selects how the header set by g_mime_text_converter_set_header() is written.
 * Text is converted as plain text, not enriched text. HTML is copied
 * verbatim and is not sanitized. This setting cannot be changed during
 * conversion.
 **/
void
g_mime_enriched_to_html_set_header_format (GMimeEnrichedToHtml *converter, GMimeHeaderFooterFormat value)
{
	g_return_if_fail (GMIME_IS_ENRICHED_TO_HTML (converter));
	g_return_if_fail (!_g_mime_text_converter_is_active (GMIME_TEXT_CONVERTER (converter)));
	g_return_if_fail (value <= GMIME_HEADER_FOOTER_FORMAT_HTML);

	converter->header_format = value;
}

/**
 * g_mime_enriched_to_html_get_footer_format:
 * @converter: a #GMimeEnrichedToHtml
 *
 * Gets how the configured footer is interpreted.
 *
 * Returns: the footer format, initially %GMIME_HEADER_FOOTER_FORMAT_TEXT.
 **/
GMimeHeaderFooterFormat
g_mime_enriched_to_html_get_footer_format (GMimeEnrichedToHtml *converter)
{
	g_return_val_if_fail (GMIME_IS_ENRICHED_TO_HTML (converter), 0);

	return converter->footer_format;
}

/**
 * g_mime_enriched_to_html_set_footer_format:
 * @converter: a #GMimeEnrichedToHtml
 * @value: the format of the configured footer
 *
 * Selects how the footer set by g_mime_text_converter_set_footer() is written.
 * Text is converted as plain text, not enriched text. HTML is copied
 * verbatim and is not sanitized. This setting cannot be changed during
 * conversion.
 **/
void
g_mime_enriched_to_html_set_footer_format (GMimeEnrichedToHtml *converter, GMimeHeaderFooterFormat value)
{
	g_return_if_fail (GMIME_IS_ENRICHED_TO_HTML (converter));
	g_return_if_fail (!_g_mime_text_converter_is_active (GMIME_TEXT_CONVERTER (converter)));
	g_return_if_fail (value <= GMIME_HEADER_FOOTER_FORMAT_HTML);

	converter->footer_format = value;
}

/**
 * g_mime_enriched_to_html_get_max_element_depth:
 * @converter: a #GMimeEnrichedToHtml
 *
 * Gets the maximum nesting depth of enriched commands, including unknown
 * commands and nested parameters.
 *
 * Returns: the nesting limit, initially 4096.
 **/
guint
g_mime_enriched_to_html_get_max_element_depth (GMimeEnrichedToHtml *converter)
{
	g_return_val_if_fail (GMIME_IS_ENRICHED_TO_HTML (converter), 0);

	return converter->max_depth;
}

/**
 * g_mime_enriched_to_html_set_max_element_depth:
 * @converter: a #GMimeEnrichedToHtml
 * @value: the maximum nesting depth, greater than zero
 *
 * Limits enriched command and parameter nesting. Exceeding the limit
 * causes conversion to fail with %GMIME_ERROR_PARSE_ERROR. This setting
 * cannot be changed during conversion.
 **/
void
g_mime_enriched_to_html_set_max_element_depth (GMimeEnrichedToHtml *converter, guint value)
{
	g_return_if_fail (GMIME_IS_ENRICHED_TO_HTML (converter));
	g_return_if_fail (!_g_mime_text_converter_is_active (GMIME_TEXT_CONVERTER (converter)));
	g_return_if_fail (value > 0);

	converter->max_depth = value;
}

/**
 * g_mime_enriched_to_html_get_max_parameter_length:
 * @converter: a #GMimeEnrichedToHtml
 *
 * Gets the maximum length of an enriched command parameter, measured in
 * Unicode scalar values rather than bytes or UTF-16 code units.
 *
 * Returns: the parameter length limit, initially 4096.
 **/
guint
g_mime_enriched_to_html_get_max_parameter_length (GMimeEnrichedToHtml *converter)
{
	g_return_val_if_fail (GMIME_IS_ENRICHED_TO_HTML (converter), 0);

	return converter->max_parameter;
}

/**
 * g_mime_enriched_to_html_set_max_parameter_length:
 * @converter: a #GMimeEnrichedToHtml
 * @value: the maximum parameter length in Unicode scalar values, greater than zero
 *
 * Limits the text in each enriched command parameter. Exceeding the limit
 * causes conversion to fail with %GMIME_ERROR_PARSE_ERROR. This setting
 * cannot be changed during conversion.
 **/
void
g_mime_enriched_to_html_set_max_parameter_length (GMimeEnrichedToHtml *converter, guint value)
{
	g_return_if_fail (GMIME_IS_ENRICHED_TO_HTML (converter));
	g_return_if_fail (!_g_mime_text_converter_is_active (GMIME_TEXT_CONVERTER (converter)));
	g_return_if_fail (value > 0);

	converter->max_parameter = value;
}

/**
 * g_mime_enriched_to_html_get_max_input_length:
 * @converter: a #GMimeEnrichedToHtml
 *
 * Gets the input length limit in Unicode scalar values, including enriched
 * markup but excluding the configured header and footer.
 *
 * Returns: the input length limit, initially 16 * 1024 * 1024.
 **/
gsize
g_mime_enriched_to_html_get_max_input_length (GMimeEnrichedToHtml *converter)
{
	g_return_val_if_fail (GMIME_IS_ENRICHED_TO_HTML (converter), 0);

	return converter->max_input;
}

/**
 * g_mime_enriched_to_html_set_max_input_length:
 * @converter: a #GMimeEnrichedToHtml
 * @value: the maximum input length in Unicode scalar values, greater than zero
 *
 * Limits decoded input, including enriched markup. Exceeding the limit
 * causes conversion to fail with %GMIME_ERROR_PARSE_ERROR. This setting
 * cannot be changed during conversion.
 **/
void
g_mime_enriched_to_html_set_max_input_length (GMimeEnrichedToHtml *converter, gsize value)
{
	g_return_if_fail (GMIME_IS_ENRICHED_TO_HTML (converter));
	g_return_if_fail (!_g_mime_text_converter_is_active (GMIME_TEXT_CONVERTER (converter)));
	g_return_if_fail (value > 0);

	converter->max_input = value;
}

/**
 * g_mime_enriched_to_html_get_max_output_length:
 * @converter: a #GMimeEnrichedToHtml
 *
 * Gets the output length limit in Unicode scalar values. Generated markup,
 * escaped text, wrappers, headers and footers all count toward this limit.
 *
 * Returns: the output length limit, initially 64 * 1024 * 1024.
 **/
gsize
g_mime_enriched_to_html_get_max_output_length (GMimeEnrichedToHtml *converter)
{
	g_return_val_if_fail (GMIME_IS_ENRICHED_TO_HTML (converter), 0);

	return converter->max_output;
}

/**
 * g_mime_enriched_to_html_set_max_output_length:
 * @converter: a #GMimeEnrichedToHtml
 * @value: the maximum output length in Unicode scalar values, greater than zero
 *
 * Limits the complete generated HTML before output charset encoding.
 * Exceeding the limit causes conversion to fail with
 * %GMIME_ERROR_PARSE_ERROR. This setting cannot be changed during conversion.
 **/
void
g_mime_enriched_to_html_set_max_output_length (GMimeEnrichedToHtml *converter, gsize value)
{
	g_return_if_fail (GMIME_IS_ENRICHED_TO_HTML (converter));
	g_return_if_fail (!_g_mime_text_converter_is_active (GMIME_TEXT_CONVERTER (converter)));
	g_return_if_fail (value > 0);

	converter->max_output = value;
}
