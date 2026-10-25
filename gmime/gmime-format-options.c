/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*  GMime
 *  Copyright (C) 2000-2022 Jeffrey Stedfast
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

#include "gmime-format-options.h"
#include "gmime-filter-dos2unix.h"
#include "gmime-filter-unix2dos.h"


/**
 * SECTION: gmime-format-options
 * @title: GMimeFormatOptions
 * @short_description: Format options
 * @see_also:
 *
 * A #GMimeFormatOptions is used by GMime to determine how to serialize various objects and headers.
 **/


struct _GMimeFormatOptions {
	GMimeParamEncodingMethod method;
	GMimeNewLineFormat newline;
	gboolean mixed_charsets;
	gboolean international;
	gboolean always_quote_param_values;
	gboolean ensure_newline;
	GPtrArray *hidden;
	guint maxline;
};

static GMimeFormatOptions *default_options = NULL;

G_DEFINE_BOXED_TYPE (GMimeFormatOptions, g_mime_format_options, g_mime_format_options_clone, g_mime_format_options_free);

void
g_mime_format_options_init (void)
{
	if (default_options == NULL)
		default_options = g_mime_format_options_new ();
}

void
g_mime_format_options_shutdown (void)
{
	guint i;
	
	if (default_options == NULL)
		return;
	
	for (i = 0; i < default_options->hidden->len; i++)
		g_free (default_options->hidden->pdata[i]);
	
	g_ptr_array_free (default_options->hidden, TRUE);
	g_slice_free (GMimeFormatOptions, default_options);
	default_options = NULL;
}


/**
 * g_mime_format_options_get_default:
 *
 * Gets the default format options.
 *
 * Returns: the default format options.
 **/
GMimeFormatOptions *
g_mime_format_options_get_default (void)
{
	return default_options;
}


/**
 * g_mime_format_options_new:
 *
 * Creates a new set of #GMimeFormatOptions.
 *
 * Returns: a newly allocated set of #GMimeFormatOptions with the default values.
 **/
GMimeFormatOptions *
g_mime_format_options_new (void)
{
	GMimeFormatOptions *options;
	
	options = g_slice_new (GMimeFormatOptions);
	options->method = GMIME_PARAM_ENCODING_METHOD_RFC2231;
	options->newline = GMIME_NEWLINE_FORMAT_UNIX;
	options->hidden = g_ptr_array_new ();
	options->mixed_charsets = FALSE;
	options->international = FALSE;
	options->always_quote_param_values = FALSE;
	options->ensure_newline = FALSE;
	options->maxline = 78;
	
	return options;
}


/**
 * _g_mime_format_options_clone:
 * @options: (nullable): a #GMimeFormatOptions or %NULL
 * @hidden: %TRUE if the hidden headers should also be cloned
 *
 * Clones a #GMimeFormatOptions.
 *
 * Returns: (transfer full): a newly allocated #GMimeFormatOptions.
 **/
GMimeFormatOptions *
_g_mime_format_options_clone (GMimeFormatOptions *options, gboolean hidden)
{
	GMimeFormatOptions *clone;
	guint i;
	
	if (options == NULL)
		options = default_options;
	
	clone = g_slice_new (GMimeFormatOptions);
	clone->method = options->method;
	clone->newline = options->newline;
	clone->mixed_charsets = options->mixed_charsets;
	clone->international = options->international;
	clone->always_quote_param_values = options->always_quote_param_values;
	clone->ensure_newline = options->ensure_newline;
	clone->maxline = options->maxline;
	
	clone->hidden = g_ptr_array_new ();
	
	if (hidden) {
		for (i = 0; i < options->hidden->len; i++)
			g_ptr_array_add (clone->hidden, g_strdup (options->hidden->pdata[i]));
	}
	
	return clone;
}

gboolean
_g_mime_format_options_get_ensure_newline (GMimeFormatOptions *options)
{
	return (options ? options : default_options)->ensure_newline;
}

void
_g_mime_format_options_set_ensure_newline (GMimeFormatOptions *options, gboolean ensure)
{
	options->ensure_newline = ensure;
}

/**
 * g_mime_format_options_clone:
 * @options: (nullable): a #GMimeFormatOptions or %NULL
 *
 * Clones a #GMimeFormatOptions.
 *
 * Returns: (transfer full): a newly allocated #GMimeFormatOptions.
 **/
GMimeFormatOptions *
g_mime_format_options_clone (GMimeFormatOptions *options)
{
	return _g_mime_format_options_clone (options, TRUE);
}


/**
 * g_mime_format_options_free:
 * @options: a #GMimeFormatOptions
 *
 * Frees a set of #GMimeFormatOptions.
 **/
void
g_mime_format_options_free (GMimeFormatOptions *options)
{
	guint i;
	
	g_return_if_fail (options != NULL);
	
	if (options != default_options) {
		for (i = 0; i < options->hidden->len; i++)
			g_free (options->hidden->pdata[i]);
		g_ptr_array_free (options->hidden, TRUE);
		
		g_slice_free (GMimeFormatOptions, options);
	}
}


/**
 * g_mime_format_options_get_max_line_length:
 * @options: (nullable): a #GMimeFormatOptions or %NULL for the defaults
 *
 * Gets the line-length target for parameter encoding and header folding.
 * The default is 78.
 *
 * Returns: the configured maximum line length.
 **/
guint
g_mime_format_options_get_max_line_length (GMimeFormatOptions *options)
{
	return options ? options->maxline : default_options->maxline;
}


/**
 * g_mime_format_options_set_max_line_length:
 * @options: a mutable #GMimeFormatOptions
 * @max_line_length: a line length between 60 and 998, inclusive
 *
 * Sets the line-length target for parameter encoding and header folding,
 * including addresses, general text, References, Received, and
 * Disposition-Notification-Options headers.
 * RFC 2047 parameter encoded-words remain limited to 75 characters. Oversized
 * parameter names, charset/language declarations, or individual characters
 * may require exceeding the target. Disabling parameter folding suppresses hard
 * line breaks, not parameter encoding or continuation selection.
 *
 * The global defaults cannot be changed with this setter; use
 * g_mime_format_options_new() or g_mime_format_options_clone().
 * References, Received, and Disposition-Notification-Options tokens are not
 * split; an oversized token or first token may exceed the target.
 * Address display names are folded between words,
 * keeping quoted strings intact. General text folding preserves encoded-word
 * tokens and can split oversized unencoded tokens over multiple lines.
 * RFC 2047 text and phrase generation still uses legacy word sizing.
 **/
void
g_mime_format_options_set_max_line_length (GMimeFormatOptions *options, guint max_line_length)
{
	g_return_if_fail (options != NULL);
	g_return_if_fail (options != default_options);
	g_return_if_fail (max_line_length >= 60 && max_line_length <= 998);

	options->maxline = max_line_length;
}


/**
 * g_mime_format_options_get_param_encoding_method:
 * @options: (nullable): a #GMimeFormatOptions or %NULL
 *
 * Gets the parameter encoding method to use for #GMimeParam parameters that do not
 * already have an encoding method specified.
 *
 * Returns: the encoding method that is currently set.
 **/
GMimeParamEncodingMethod
g_mime_format_options_get_param_encoding_method (GMimeFormatOptions *options)
{
	return options ? options->method : default_options->method;
}


/**
 * g_mime_format_options_set_param_encoding_method:
 * @options: a #GMimeFormatOptions
 * @method: a #GMimeParamEncodingMethod
 *
 * Sets the parameter encoding method to use when encoding parameters which
 * do not have an encoding method specified.
 *
 * Note: #GMIME_PARAM_ENCODING_METHOD_DEFAULT is not allowed.
 **/
void
g_mime_format_options_set_param_encoding_method (GMimeFormatOptions *options, GMimeParamEncodingMethod method)
{
	g_return_if_fail (options != NULL);
	g_return_if_fail (method == GMIME_PARAM_ENCODING_METHOD_RFC2231 || method == GMIME_PARAM_ENCODING_METHOD_RFC2047);
	
	options->method = method;
}


/**
 * g_mime_format_options_get_always_quote_param_values:
 * @options: (nullable): a #GMimeFormatOptions or %NULL for the defaults
 *
 * Gets whether otherwise unquoted parameter values should be quoted.
 * The default is %FALSE.
 *
 * Returns: %TRUE if quoting is requested for all parameters.
 **/
gboolean
g_mime_format_options_get_always_quote_param_values (GMimeFormatOptions *options)
{
	return options ? options->always_quote_param_values : default_options->always_quote_param_values;
}


/**
 * g_mime_format_options_set_always_quote_param_values:
 * @options: a mutable #GMimeFormatOptions
 * @always_quote: whether to quote otherwise unquoted parameter values
 *
 * Requests quoting for all parameters written using @options.
 * %FALSE does not suppress required quoting or a parameter's own request.
 * RFC 2231 encoded values are not quoted. The global defaults cannot be
 * changed with this setter; use g_mime_format_options_new() or
 * g_mime_format_options_clone() to obtain mutable options.
 **/
void
g_mime_format_options_set_always_quote_param_values (GMimeFormatOptions *options, gboolean always_quote)
{
	g_return_if_fail (options != NULL);
	g_return_if_fail (options != default_options);

	options->always_quote_param_values = !!always_quote;
}


/**
 * g_mime_format_options_get_newline_format:
 * @options: (nullable): a #GMimeFormatOptions or %NULL
 *
 * Gets the new-line format to use when writing out messages and headers.
 *
 * Returns: the new-line format that is currently set.
 **/
GMimeNewLineFormat
g_mime_format_options_get_newline_format (GMimeFormatOptions *options)
{
	return options ? options->newline : default_options->newline;
}


/**
 * g_mime_format_options_set_newline_format:
 * @options: a #GMimeFormatOptions
 * @newline: a #GMimeNewLineFormat
 *
 * Sets the new-line format that should be used when writing headers and messages.
 * Only %GMIME_NEWLINE_FORMAT_UNIX and %GMIME_NEWLINE_FORMAT_DOS are valid;
 * %GMIME_NEWLINE_FORMAT_MIXED describes input, not an output format.
 **/
void
g_mime_format_options_set_newline_format (GMimeFormatOptions *options, GMimeNewLineFormat newline)
{
	g_return_if_fail (options != NULL);
	g_return_if_fail (newline == GMIME_NEWLINE_FORMAT_UNIX || newline == GMIME_NEWLINE_FORMAT_DOS);
	
	options->newline = newline;
}


/**
 * g_mime_format_options_get_newline:
 * @options: (nullable): a #GMimeFormatOptions or %NULL
 *
 * Gets a string representing the currently set new-line format.
 *
 * Returns: a new-line character sequence.
 **/
const char *
g_mime_format_options_get_newline (GMimeFormatOptions *options)
{
	if (options == NULL)
		options = default_options;
	
	if (options->newline == GMIME_NEWLINE_FORMAT_DOS)
		return "\r\n";
	
	return "\n";
}


/**
 * g_mime_format_options_create_newline_filter:
 * @options: (nullable): a #GMimeFormatOptions or %NULL
 * @ensure_newline: %TRUE if the output must *always* end with a new line
 *
 * Creates a #GMimeFilter suitable for converting line-endings to the
 * currently set new-line format.
 *
 * Returns: (transfer full): a #GMimeFilter to convert to the specified new-line format.
 **/
GMimeFilter *
g_mime_format_options_create_newline_filter (GMimeFormatOptions *options, gboolean ensure_newline)
{
	if (options == NULL)
		options = default_options;
	
	if (options->newline == GMIME_NEWLINE_FORMAT_DOS)
		return g_mime_filter_unix2dos_new (ensure_newline);
	
	return g_mime_filter_dos2unix_new (ensure_newline);
}


/**
 * g_mime_format_options_get_allow_mixed_charsets:
 * @options: (nullable): a #GMimeFormatOptions or %NULL
 *
 * Gets whether individual RFC 2047 words may use different charsets.
 * The default is %FALSE.
 *
 * Returns: %TRUE if each header value is allowed to be encoded with mixed charsets or %FALSE otherwise.
 **/
gboolean
g_mime_format_options_get_allow_mixed_charsets (GMimeFormatOptions *options)
{
	if (options == NULL)
		options = default_options;
	
	return options->mixed_charsets;
}


/**
 * g_mime_format_options_set_allow_mixed_charsets:
 * @options: a #GMimeFormatOptions
 * @allow: %TRUE if each header value should be allowed to be encoded with mixed charsets
 *
 * Sets whether RFC 2047 words may use US-ASCII or ISO-8859-1 instead of
 * the requested charset. Disabled by default, so encoded words use the
 * requested charset, or UTF-8 when no charset was specified. Lossless
 * UTF-8 fallback remains possible for an unrepresentable word.
 *
 * When enabled, words needing other characters still use the requested
 * charset, or UTF-8 if none was specified. This option does not change
 * International UTF-8 output or explicitly assigned raw header values.
 **/
void
g_mime_format_options_set_allow_mixed_charsets (GMimeFormatOptions *options, gboolean allow)
{
	g_return_if_fail (options != NULL);
	
	options->mixed_charsets = allow;
}

/**
 * g_mime_format_options_get_allow_international:
 * @options: (nullable): a #GMimeFormatOptions or %NULL
 *
 * Gets whether or not international encoding is allowed.
 *
 * Returns: %TRUE if international encoding is allowed or %FALSE otherwise.
 **/
gboolean
g_mime_format_options_get_allow_international (GMimeFormatOptions *options)
{
	return options ? options->international : default_options->international;
}


/**
 * g_mime_format_options_set_allow_international:
 * @options: a #GMimeFormatOptions
 * @allow: %TRUE if international encoding is allowed
 *
 * Sets whether UTF-8 address names, domains, source routes, parameter values,
 * and ordinary text headers can be written without RFC 2047, IDN, or RFC 2231
 * encoding. Parameter values are quoted as needed; controls and encoded-word-like
 * parameter text still require encoding. Ordinary text headers use Unicode-safe
 * folding, counting UTF-16 units rather than UTF-8 bytes. Explicit raw header
 * values are not reformatted. Parsed text, address and content fields are
 * decoded and re-encoded without replacing their stored raw values. Mailing-list
 * command fields preserve URL boundaries and format comments as UTF-8. Parsed
 * trace, identifier, authentication and signature fields remain unchanged.
 **/
void
g_mime_format_options_set_allow_international (GMimeFormatOptions *options, gboolean allow)
{
	g_return_if_fail (options != NULL);
	
	options->international = allow;
}


/**
 * g_mime_format_options_is_hidden_header:
 * @options: (nullable): a #GMimeFormatOptions or %NULL
 * @header: the name of a header
 *
 * Gets whether or not the specified header should be hidden.
 *
 * Returns: %TRUE if the header should be hidden or %FALSE otherwise.
 **/
gboolean
g_mime_format_options_is_hidden_header (GMimeFormatOptions *options, const char *header)
{
	guint i;
	
	g_return_val_if_fail (header != NULL, FALSE);
	
	if (options == NULL)
		options = default_options;
	
	for (i = 0; i < options->hidden->len; i++) {
		if (!g_ascii_strcasecmp (options->hidden->pdata[i], header))
			return TRUE;
	}
	
	return FALSE;
}


/**
 * g_mime_format_options_add_hidden_header:
 * @options: a #GMimeFormatOptions
 * @header: a header name
 *
 * Adds the given header to the list of headers that should be hidden.
 **/
void
g_mime_format_options_add_hidden_header (GMimeFormatOptions *options, const char *header)
{
	g_return_if_fail (options != NULL);
	g_return_if_fail (header != NULL);
	
	g_ptr_array_add (options->hidden, g_strdup (header));
}


/**
 * g_mime_format_options_remove_hidden_header:
 * @options: a #GMimeFormatOptions
 * @header: a header name
 *
 * Removes the given header from the list of headers that should be hidden.
 **/
void
g_mime_format_options_remove_hidden_header (GMimeFormatOptions *options, const char *header)
{
	guint i;
	
	g_return_if_fail (options != NULL);
	g_return_if_fail (header != NULL);
	
	for (i = options->hidden->len; i > 0; i--) {
		if (!g_ascii_strcasecmp (options->hidden->pdata[i - 1], header)) {
			g_free (options->hidden->pdata[i - 1]);
			g_ptr_array_remove_index (options->hidden, i - 1);
		}
	}
}


/**
 * g_mime_format_options_clear_hidden_headers:
 * @options: a #GMimeFormatOptions
 *
 * Clears the list of headers that should be hidden.
 **/
void
g_mime_format_options_clear_hidden_headers (GMimeFormatOptions *options)
{
	guint i;
	
	g_return_if_fail (options != NULL);
	
	for (i = 0; i < options->hidden->len; i++)
		g_free (options->hidden->pdata[i]);
	
	g_ptr_array_set_size (options->hidden, 0);
}
