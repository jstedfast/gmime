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

#include "gmime-parser-options.h"


static char *default_charsets[3] = { "utf-8", "iso-8859-1", NULL };


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
	char **charsets;
	GMimeParserWarningFunc warning_cb;
	gpointer warning_user_data;
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
	
	g_strfreev (default_options->charsets);
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
	options->allow_no_domain = FALSE;
	
	options->charsets = g_malloc (sizeof (char *) * 3);
	options->charsets[0] = g_strdup ("utf-8");
	options->charsets[1] = g_strdup ("iso-8859-1");
	options->charsets[2] = NULL;
	
	options->warning_cb = NULL;
	options->warning_user_data = NULL;

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
	guint i, n = 0;
	
	if (options == NULL)
		options = default_options;
	
	clone = g_slice_new (GMimeParserOptions);
	clone->allow_no_domain = options->allow_no_domain;
	clone->addresses = options->addresses;
	clone->parameters = options->parameters;
	clone->rfc2047 = options->rfc2047;
	
	while (options->charsets[n])
		n++;
	
	clone->charsets = g_malloc (sizeof (char *) * (n + 1));
	for (i = 0; i < n; i++)
		clone->charsets[i] = g_strdup (options->charsets[i]);
	clone->charsets[i] = NULL;
	
	clone->warning_cb = options->warning_cb;
	clone->warning_user_data = options->warning_user_data;

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
		g_strfreev (options->charsets);
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
	
	options->addresses = mode;
}


/**
 * g_mime_parser_options_get_allow_addresses_without_domain:
 * @options: (nullable): a #GMimeParserOptions or %NULL
 *
 * Gets whether or not the rfc822 address parser should allow addresses without a domain.
 *
 * In general, you'll probably want this value to be %FALSE (the default) as it allows 
 * maximum interoperability with existing (broken) mail clients and other mail software
 * such as sloppily written perl scripts (aka spambots) that do not properly quote the
 * name when it contains a comma.
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
 * In general, you'll probably want this value to be %FALSE (the default) as it allows 
 * maximum interoperability with existing (broken) mail clients and other mail software
 * such as sloppily written perl scripts (aka spambots) that do not properly quote the
 * name when it contains a comma.
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
 * g_mime_parser_options_get_fallback_charsets:
 * @options: (nullable): a #GMimeParserOptions or %NULL
 *
 * Gets the fallback charsets to try when decoding 8-bit headers.
 *
 * Returns: (transfer none): a %NULL-terminated list of charsets to try when
 * decoding 8-bit headers.
 **/
const char **
g_mime_parser_options_get_fallback_charsets (GMimeParserOptions *options)
{
	return (const char **) (options ? options->charsets : default_options->charsets);
}


/**
 * g_mime_parser_options_set_fallback_charsets:
 * @options: a #GMimeParserOptions
 * @charsets: a %NULL-terminated list of charsets or %NULL for the default list
 *
 * Sets the fallback charsets to try when decoding 8-bit headers.
 *
 * Note: It is recommended that the list of charsets start with utf-8
 * and end with iso-8859-1.
 **/
void
g_mime_parser_options_set_fallback_charsets (GMimeParserOptions *options, const char **charsets)
{
	guint i, n = 0;
	
	g_return_if_fail (options != NULL);
	
	g_strfreev (options->charsets);
	
	if (charsets == NULL || *charsets == NULL)
		charsets = (const char **) default_charsets;
	
	while (charsets[n] != NULL)
		n++;
	
	options->charsets = g_malloc (sizeof (char *) * (n + 1));
	for (i = 0; i < n; i++)
		options->charsets[i] = g_strdup (charsets[i]);
	options->charsets[n] = NULL;
}


/**
 * g_mime_parser_options_get_warning_callback:
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
	g_return_if_fail (options != NULL);
	
	options->warning_cb = warning_cb;
	options->warning_user_data = user_data;
}
