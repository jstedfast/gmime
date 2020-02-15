/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*  GMime
 *  Copyright (C) 2000-2020 Jeffrey Stedfast
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
	options->mixed_charsets = TRUE;
	options->international = FALSE;
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
	clone->maxline = options->newline;
	
	clone->hidden = g_ptr_array_new ();
	
	if (hidden) {
		for (i = 0; i < options->hidden->len; i++)
			g_ptr_array_add (clone->hidden, g_strdup (options->hidden->pdata[i]));
	}
	
	return clone;
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


#ifdef NOT_YET_IMPLEMENTED
/**
 * g_mime_format_options_get_allow_mixed_charsets:
 * @options: (nullable): a #GMimeFormatOptions or %NULL
 *
 * Gets whether or not headers are allowed to be encoded using mixed charsets.
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
 * Sets whether or not header values should be allowed to be encoded with mixed charsets.
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
 * Sets whether or not international encoding is allowed.
 **/
void
g_mime_format_options_set_allow_international (GMimeFormatOptions *options, gboolean allow)
{
	g_return_if_fail (options != NULL);
	
	options->international = allow;
}


/**
 * g_mime_format_options_get_max_line:
 * @options: (nullable): a #GMimeFormatOptions or %NULL
 *
 * Gets the max line length to use with encoders.
 *
 * Returns: the max line length to use with encoders.
 **/
guint
g_mime_format_options_get_max_line (GMimeFormatOptions *options)
{
	return options ? options->maxline : default_options->maxline;
}


/**
 * g_mime_format_options_set_max_line:
 * @options: a #GMimeFormatOptions
 * @maxline: the max line length
 *
 * Sets the max line length to use for encoders.
 **/
void
g_mime_format_options_set_max_line (GMimeFormatOptions *options, guint maxline)
{
	g_return_if_fail (options != NULL);
	
	options->maxline = maxline;
}
#endif

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
