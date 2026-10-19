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
#include <errno.h>

#include "gmime-parser-options.h"
#include "gmime-error.h"
#include "gmime-iconv.h"


/**
 * SECTION: gmime-parser-options
 * @title: GMimeParserOptions
 * @short_description: Parser options
 * @see_also:
 *
 * A #GMimeParserOptions is used to pass various options to #GMimeParser
 * and all of the various other parser functions in GMime.
 **/


struct _GMimeParserOptions {
	GMimeRfcComplianceMode addresses;
	GMimeRfcComplianceMode parameters;
	GMimeRfcComplianceMode rfc2047;
	gboolean allow_no_domain;
	gboolean allow_unquoted_commas;
	gboolean respect_content_length;
	int max_address_group_depth;
	int max_mime_depth;
	int max_header_length;
	char *charset_encoding;
	GMimeParserWarningFunc warning_cb;
	gpointer warning_user_data;
	GDestroyNotify notify;
};

static GMimeParserOptions *default_options = NULL;

G_DEFINE_BOXED_TYPE (GMimeParserOptions, g_mime_parser_options, g_mime_parser_options_clone, g_mime_parser_options_free);

void
g_mime_parser_options_init (void)
{
	if (default_options == NULL)
		default_options = g_mime_parser_options_new ();
}

void
g_mime_parser_options_shutdown (void)
{
	if (default_options == NULL)
		return;
	
	if (default_options->notify)
		default_options->notify (default_options->warning_user_data);
	
	g_free (default_options->charset_encoding);
	g_slice_free (GMimeParserOptions, default_options);
	default_options = NULL;
}

void
_g_mime_parser_options_warn (GMimeParserOptions *options, gint64 offset, guint errcode, const gchar *item)
{
	GMimeParserWarningFunc warn;
	gpointer user_data;
	
	if (options == NULL) {
		user_data = default_options->warning_user_data;
		warn = default_options->warning_cb;
	} else {
		user_data = options->warning_user_data;
		warn = options->warning_cb;
	}
	
	if (warn != NULL)
		warn (offset, errcode, item, user_data);
}

/**
 * g_mime_parser_options_get_default:
 *
 * Gets the default parser options.
 *
 * Returns: the default parser options.
 **/
GMimeParserOptions *
g_mime_parser_options_get_default (void)
{
	return default_options;
}


/**
 * g_mime_parser_options_new:
 *
 * Creates a new set of #GMimeParserOptions.
 *
 * Returns: a newly allocated set of #GMimeParserOptions with the default values.
 **/
GMimeParserOptions *
g_mime_parser_options_new (void)
{
	GMimeParserOptions *options;
	
	options = g_slice_new (GMimeParserOptions);
	options->addresses = GMIME_RFC_COMPLIANCE_LOOSE;
	options->parameters = GMIME_RFC_COMPLIANCE_LOOSE;
	options->rfc2047 = GMIME_RFC_COMPLIANCE_LOOSE;
	options->allow_no_domain = TRUE;
	options->allow_unquoted_commas = TRUE;
	options->respect_content_length = FALSE;
	options->max_address_group_depth = 3;
	options->max_mime_depth = 1024;
	options->max_header_length = 16 * 1024 * 1024;
	
	options->charset_encoding = g_strdup ("utf-8");
	
	options->warning_cb = NULL;
	options->warning_user_data = NULL;
	options->notify = NULL;

	return options;
}


/**
 * g_mime_parser_options_clone:
 * @options: (nullable): a #GMimeParserOptions or %NULL
 *
 * Clones a #GMimeParserOptions.
 *
 * Returns: (transfer full): a newly allocated #GMimeParserOptions.
 **/
GMimeParserOptions *
g_mime_parser_options_clone (GMimeParserOptions *options)
{
	GMimeParserOptions *clone;
	
	if (options == NULL)
		options = default_options;
	
	clone = g_slice_new (GMimeParserOptions);
	clone->allow_no_domain = options->allow_no_domain;
	clone->allow_unquoted_commas = options->allow_unquoted_commas;
	clone->respect_content_length = options->respect_content_length;
	clone->max_address_group_depth = options->max_address_group_depth;
	clone->max_mime_depth = options->max_mime_depth;
	clone->max_header_length = options->max_header_length;
	clone->addresses = options->addresses;
	clone->parameters = options->parameters;
	clone->rfc2047 = options->rfc2047;
	
	clone->charset_encoding = g_strdup (options->charset_encoding);
	
	clone->warning_cb = options->warning_cb;
	clone->warning_user_data = options->warning_user_data;
	// We transfer the pointer not ownership. So we don't copy the notify
	clone->notify = NULL;

	return clone;
}


/**
 * g_mime_parser_options_free:
 * @options: a #GMimeParserOptions
 *
 * Frees a set of #GMimeParserOptions.
 **/
void
g_mime_parser_options_free (GMimeParserOptions *options)
{
	g_return_if_fail (options != NULL);
	
	if (options != default_options) {
		if (options->notify)
			options->notify (options->warning_user_data);
		
		g_free (options->charset_encoding);
		g_slice_free (GMimeParserOptions, options);
	}
}


/**
 * g_mime_parser_options_get_address_compliance_mode:
 * @options: (nullable): a #GMimeParserOptions or %NULL
 *
 * Gets the compliance mode that should be used when parsing rfc822 addresses.
 *
 * Note: Even in #GMIME_RFC_COMPLIANCE_STRICT mode, the address parser is fairly liberal in
 * what it accepts. Setting it to #GMIME_RFC_COMPLIANCE_LOOSE just makes it try harder to
 * deal with garbage input.
 * #GMIME_RFC_COMPLIANCE_LOOSER additionally accepts consecutive or trailing
 * dots and escaped at-signs in local parts.
 *
 * Returns: the compliance mode that is currently set.
 **/
GMimeRfcComplianceMode
g_mime_parser_options_get_address_compliance_mode (GMimeParserOptions *options)
{
	return options ? options->addresses : default_options->addresses;
}


/**
 * g_mime_parser_options_set_address_compliance_mode:
 * @options: a #GMimeParserOptions
 * @mode: a #GMimeRfcComplianceMode
 *
 * Sets the compliance mode that should be used when parsing rfc822 addresses.
 *
 * In general, you'll probably want this value to be #GMIME_RFC_COMPLIANCE_LOOSE
 * (the default) as it allows maximum interoperability with existing (broken) mail clients
 * and other mail software such as sloppily written perl scripts (aka spambots).
 *
 * Note: Even in #GMIME_RFC_COMPLIANCE_STRICT mode, the address parser is fairly liberal in
 * what it accepts. Setting it to #GMIME_RFC_COMPLIANCE_LOOSE just makes it try harder to
 * deal with garbage input.
 **/
void
g_mime_parser_options_set_address_compliance_mode (GMimeParserOptions *options, GMimeRfcComplianceMode mode)
{
	g_return_if_fail (options != NULL);
	g_return_if_fail (mode == GMIME_RFC_COMPLIANCE_STRICT || mode == GMIME_RFC_COMPLIANCE_LOOSE ||
			  mode == GMIME_RFC_COMPLIANCE_LOOSER);
	
	options->addresses = mode;
}


/**
 * g_mime_parser_options_get_allow_addresses_without_domain:
 * @options: (nullable): a #GMimeParserOptions or %NULL
 *
 * Gets whether or not the rfc822 address parser should allow addresses without a domain.
 *
 * The default is %TRUE, allowing local UNIX mailbox addresses.
 *
 * This option exists in order to allow parsing of mailbox addresses that do not have a
 * domain component. These types of addresses are rare and were typically only used when
 * sending mail to other users on the same UNIX system.
 *
 * Returns: %TRUE if the address parser should allow addresses without a domain.
 **/
gboolean
g_mime_parser_options_get_allow_addresses_without_domain (GMimeParserOptions *options)
{
	return options ? options->allow_no_domain : default_options->allow_no_domain;
}


/**
 * g_mime_parser_options_set_allow_addresses_without_domain:
 * @options: a #GMimeParserOptions
 * @allow: %TRUE if the parser should allow addresses without a domain or %FALSE otherwise
 *
 * Sets whether the rfc822 address parser should allow addresses without a domain.
 *
 * The default is %TRUE, allowing local UNIX mailbox addresses.
 *
 * This option exists in order to allow parsing of mailbox addresses that do not have a
 * domain component. These types of addresses are rare and were typically only used when
 * sending mail to other users on the same UNIX system.
 **/
void
g_mime_parser_options_set_allow_addresses_without_domain (GMimeParserOptions *options, gboolean allow)
{
	g_return_if_fail (options != NULL);
	
	options->allow_no_domain = allow;
}

/**
 * g_mime_parser_options_get_allow_unquoted_commas_in_addresses:
 * @options: (nullable): a #GMimeParserOptions or %NULL
 *
 * Gets whether unquoted commas are allowed in address display names.
 * The default is %TRUE, matching MimeKit's address recovery behavior.
 *
 * Returns: %TRUE if unquoted commas are allowed.
 **/
gboolean
g_mime_parser_options_get_allow_unquoted_commas_in_addresses (GMimeParserOptions *options)
{
	return options ? options->allow_unquoted_commas : default_options->allow_unquoted_commas;
}


/**
 * g_mime_parser_options_set_allow_unquoted_commas_in_addresses:
 * @options: a #GMimeParserOptions
 * @allow: whether to allow unquoted commas in address display names
 *
 * Sets whether unquoted commas are allowed in address display names.
 * A comma following a quoted word is still treated as an address separator.
 * This option is independent of whether domainless addresses are allowed.
 **/
void
g_mime_parser_options_set_allow_unquoted_commas_in_addresses (GMimeParserOptions *options, gboolean allow)
{
	g_return_if_fail (options != NULL);

	options->allow_unquoted_commas = allow;
}


/**
 * g_mime_parser_options_get_respect_content_length:
 * @options: (nullable): a #GMimeParserOptions or %NULL
 *
 * Gets whether Content-Length headers postpone recognition of mbox
 * From-lines until the declared body length has been consumed.
 * The default is %FALSE. Other input formats ignore this option.
 *
 * Returns: %TRUE if mbox parsing respects Content-Length.
 **/
gboolean
g_mime_parser_options_get_respect_content_length (GMimeParserOptions *options)
{
	return options ? options->respect_content_length : default_options->respect_content_length;
}


/**
 * g_mime_parser_options_set_respect_content_length:
 * @options: a #GMimeParserOptions
 * @respect: whether mbox parsing should respect Content-Length
 *
 * Enables Content-Length handling for mbox input. The first header with
 * a valid nonnegative 32-bit numeric prefix is used; invalid values are
 * ignored. The raw value must start with whitespace followed by decimal
 * digits. This does not truncate content at the declared length: the
 * parser resumes looking for a From-line there.
 **/
void
g_mime_parser_options_set_respect_content_length (GMimeParserOptions *options, gboolean respect)
{
	g_return_if_fail (options != NULL);

	options->respect_content_length = respect;
}


/**
 * g_mime_parser_options_get_max_address_group_depth:
 * @options: (nullable): a #GMimeParserOptions or %NULL
 *
 * Gets the maximum number of nested address groups. The default is 3.
 * A value of 0 prevents the parser from accepting group addresses.
 *
 * Returns: the maximum address group depth.
 **/
int
g_mime_parser_options_get_max_address_group_depth (GMimeParserOptions *options)
{
	return options ? options->max_address_group_depth : default_options->max_address_group_depth;
}


/**
 * g_mime_parser_options_set_max_address_group_depth:
 * @options: a #GMimeParserOptions
 * @max_depth: the maximum address group depth, greater than or equal to 0
 *
 * Sets the maximum number of nested address groups. Groups beyond this
 * limit are rejected and reported through the parser warning callback.
 **/
void
g_mime_parser_options_set_max_address_group_depth (GMimeParserOptions *options, int max_depth)
{
	g_return_if_fail (options != NULL);
	g_return_if_fail (max_depth >= 0);

	options->max_address_group_depth = max_depth;
}


/**
 * g_mime_parser_options_get_max_mime_depth:
 * @options: (nullable): a #GMimeParserOptions or %NULL
 *
 * Gets the maximum MIME nesting depth. The default is 1024.
 * A value of 0 treats all multipart and embedded-message entities as leaves.
 *
 * Returns: the maximum MIME nesting depth.
 **/
int
g_mime_parser_options_get_max_mime_depth (GMimeParserOptions *options)
{
	return options ? options->max_mime_depth : default_options->max_mime_depth;
}


/**
 * g_mime_parser_options_set_max_mime_depth:
 * @options: a #GMimeParserOptions
 * @max_depth: the maximum MIME nesting depth, greater than or equal to 0
 *
 * Sets the maximum MIME nesting depth. Containers at this depth are parsed
 * as #GMimePart objects, preserving their content without parsing it.
 * Exceeding the limit is reported through the parser warning callback.
 *
 * The parser currently uses recursion. Increasing the limit above its
 * default may exhaust the call stack.
 **/
void
g_mime_parser_options_set_max_mime_depth (GMimeParserOptions *options, int max_depth)
{
	g_return_if_fail (options != NULL);
	g_return_if_fail (max_depth >= 0);

	options->max_mime_depth = max_depth;
}

/**
 * g_mime_parser_options_get_max_header_length:
 * @options: (nullable): a #GMimeParserOptions or %NULL
 *
 * Gets the maximum raw length of a single header, in bytes.
 * The default is 16777216 (16 MiB).
 *
 * Returns: the maximum header length.
 **/
int
g_mime_parser_options_get_max_header_length (GMimeParserOptions *options)
{
	return options ? options->max_header_length : default_options->max_header_length;
}


/**
 * g_mime_parser_options_set_max_header_length:
 * @options: a #GMimeParserOptions
 * @max_length: the maximum raw header length, greater than 0
 *
 * Sets the maximum length of a single header. The length includes the field
 * name, colon, raw value, folding whitespace and terminating newline sequence.
 * #GMimeParser fails with %GMIME_ERROR_HEADER_TOO_LONG before buffering data
 * beyond this limit.
 **/
void
g_mime_parser_options_set_max_header_length (GMimeParserOptions *options, int max_length)
{
	g_return_if_fail (options != NULL);
	g_return_if_fail (max_length > 0);

	options->max_header_length = max_length;
}


/**
 * g_mime_parser_options_get_parameter_compliance_mode:
 * @options: (nullable): a #GMimeParserOptions or %NULL
 *
 * Gets the compliance mode that should be used when parsing Content-Type and
 * Content-Disposition parameters.
 *
 * Note: Even in #GMIME_RFC_COMPLIANCE_STRICT mode, the parameter parser is fairly liberal
 * in what it accepts. Setting it to #GMIME_RFC_COMPLIANCE_LOOSE just makes it try harder
 * to deal with garbage input.
 *
 * Returns: the compliance mode that is currently set.
 **/
GMimeRfcComplianceMode
g_mime_parser_options_get_parameter_compliance_mode (GMimeParserOptions *options)
{
	return options ? options->parameters : default_options->parameters;
}


/**
 * g_mime_parser_options_set_parameter_compliance_mode:
 * @options: a #GMimeParserOptions
 * @mode: a #GMimeRfcComplianceMode
 *
 * Sets the compliance mode that should be used when parsing Content-Type and
 * Content-Disposition parameters.
 *
 * In general, you'll probably want this value to be #GMIME_RFC_COMPLIANCE_LOOSE
 * (the default) as it allows maximum interoperability with existing (broken) mail clients
 * and other mail software such as sloppily written perl scripts (aka spambots).
 *
 * Note: Even in #GMIME_RFC_COMPLIANCE_STRICT mode, the parameter parser is fairly liberal
 * in what it accepts. Setting it to #GMIME_RFC_COMPLIANCE_LOOSE just makes it try harder
 * to deal with garbage input.
 **/
void
g_mime_parser_options_set_parameter_compliance_mode (GMimeParserOptions *options, GMimeRfcComplianceMode mode)
{
	g_return_if_fail (options != NULL);
	
	options->parameters = mode;
}


/**
 * g_mime_parser_options_get_rfc2047_compliance_mode:
 * @options: (nullable): a #GMimeParserOptions or %NULL
 *
 * Gets the compliance mode that should be used when parsing rfc2047 encoded words.
 *
 * Note: Even in #GMIME_RFC_COMPLIANCE_STRICT mode, the rfc2047 parser is fairly liberal
 * in what it accepts. Setting it to #GMIME_RFC_COMPLIANCE_LOOSE just makes it try harder
 * to deal with garbage input.
 *
 * Returns: the compliance mode that is currently set.
 **/
GMimeRfcComplianceMode
g_mime_parser_options_get_rfc2047_compliance_mode (GMimeParserOptions *options)
{
	return options ? options->rfc2047 : default_options->rfc2047;
}


/**
 * g_mime_parser_options_set_rfc2047_compliance_mode:
 * @options: a #GMimeParserOptions
 * @mode: a #GMimeRfcComplianceMode
 *
 * Sets the compliance mode that should be used when parsing rfc2047 encoded words.
 *
 * In general, you'll probably want this value to be #GMIME_RFC_COMPLIANCE_LOOSE
 * (the default) as it allows maximum interoperability with existing (broken) mail clients
 * and other mail software such as sloppily written perl scripts (aka spambots).
 *
 * Note: Even in #GMIME_RFC_COMPLIANCE_STRICT mode, the parameter parser is fairly liberal
 * in what it accepts. Setting it to #GMIME_RFC_COMPLIANCE_LOOSE just makes it try harder
 * to deal with garbage input.
 **/
void
g_mime_parser_options_set_rfc2047_compliance_mode (GMimeParserOptions *options, GMimeRfcComplianceMode mode)
{
	g_return_if_fail (options != NULL);
	
	options->rfc2047 = mode;
}


/**
 * g_mime_parser_options_get_charset_encoding:
 * @options: (nullable): a #GMimeParserOptions or %NULL
 *
 * Gets the fallback charset encoding used when decoding undeclared 8-bit
 * header text. The default is "utf-8". UTF-8 is always attempted first,
 * followed by this charset, and finally ISO-8859-1.
 *
 * Returns: (transfer none): the charset name, owned by @options or the defaults.
 **/
const char *
g_mime_parser_options_get_charset_encoding (GMimeParserOptions *options)
{
	return options ? options->charset_encoding : default_options->charset_encoding;
}


/**
 * g_mime_parser_options_set_charset_encoding:
 * @options: a #GMimeParserOptions
 * @charset: (nullable): charset encoding name, or %NULL to reset to UTF-8
 * @error: return location for a #GError, or %NULL
 *
 * Sets the fallback charset encoding for undeclared 8-bit header text,
 * following MimeKit's CharsetEncoding option. The name is copied and
 * must be supported by the platform's iconv implementation.
 *
 * UTF-8 is always attempted first and ISO-8859-1 last. An unsupported
 * or empty name fails without changing the option.
 *
 * Returns: %TRUE on success or %FALSE with @error set on failure.
 **/
gboolean
g_mime_parser_options_set_charset_encoding (GMimeParserOptions *options, const char *charset, GError **error)
{
	char *copy;
	iconv_t cd;
	int code;

	g_return_val_if_fail (options != NULL, FALSE);
	g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

	if (charset == NULL)
		charset = "utf-8";
	if (*charset == '\0') {
		g_set_error_literal (error, GMIME_ERROR, GMIME_ERROR_NOT_SUPPORTED,
				     "Charset encoding name must not be empty");
		return FALSE;
	}
	if ((cd = g_mime_iconv_open ("UTF-8", charset)) == (iconv_t) -1) {
		code = errno;
		g_set_error (error, GMIME_ERROR, code == EINVAL ? GMIME_ERROR_NOT_SUPPORTED : code,
			     "Cannot decode charset '%s': %s", charset, g_strerror (code));
		return FALSE;
	}
	g_mime_iconv_close (cd);

	copy = g_strdup (charset);
	g_free (options->charset_encoding);
	options->charset_encoding = copy;
	return TRUE;
}


/**
 * g_mime_parser_options_get_warning_callback: (skip)
 * @options: (nullable): a #GMimeParserOptions or %NULL
 *
 * Gets callback function which is called if the parser detects any issues.
 *
 * Returns: the currently registered warning callback function
 **/
GMimeParserWarningFunc
g_mime_parser_options_get_warning_callback (GMimeParserOptions *options)
{
	return options ? options->warning_cb : default_options->warning_cb;
}


/**
 * g_mime_parser_options_set_warning_callback:
 * @options: a #GMimeParserOptions
 * @warning_cb: a #GMimeParserWarningFunc or %NULL to clear the callback
 * @user_data: data passed to the warning callback function
 *
 * Registers the callback function being called if the parser detects any issues.
 **/
void
g_mime_parser_options_set_warning_callback (GMimeParserOptions *options, GMimeParserWarningFunc warning_cb, gpointer user_data)
{
	g_mime_parser_options_set_warning_callback_full (options, warning_cb, user_data, NULL);
}

/**
 * g_mime_parser_options_set_warning_callback_full:
 * @options: a #GMimeParserOptions
 * @warning_cb: a #GMimeParserWarningFunc or %NULL to clear the callback
 * @user_data: data passed to the warning callback function
 * @notify: callback function ran on destruction
 *
 * Registers the callback function being called if the parser detects any issues.
 **/
void
g_mime_parser_options_set_warning_callback_full (GMimeParserOptions *options, GMimeParserWarningFunc warning_cb, gpointer user_data, GDestroyNotify notify)
{
	g_return_if_fail (options != NULL);

	if (options->notify)
		options->notify (options->warning_user_data);
	
	options->warning_cb = warning_cb;
	options->warning_user_data = user_data;
	options->notify = notify;
}
