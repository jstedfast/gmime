/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*  GMime
 *  Copyright (C) 2000-2017 Jeffrey Stedfast
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

static GMimeParserOptions *default_options = NULL;


/**
 * SECTION: gmime-parser-options
 * @title: GMimeParserOptions
 * @short_description: Parser options
 * @see_also:
 *
 * A #GMimeParserOptions is used to pass various options to #GMimeParser
 * and all of the various other parser functions in GMime.
 **/


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
	
	options->charsets = g_malloc (sizeof (char *) * 3);
	options->charsets[0] = g_strdup ("utf-8");
	options->charsets[1] = g_strdup ("iso-8859-1");
	options->charsets[2] = NULL;
	
	return options;
}


/**
 * _g_mime_parser_options_clone:
 * @options: a #GMimeParserOptions
 *
 * Clones a #GMimeParserOptions.
 *
 * Returns: a newly allocated #GMimeParserOptions.
 **/
GMimeParserOptions *
_g_mime_parser_options_clone (GMimeParserOptions *options)
{
	GMimeParserOptions *clone;
	guint i, n = 0;
	
	clone = g_slice_new (GMimeParserOptions);
	clone->addresses = options->addresses;
	clone->parameters = options->parameters;
	clone->rfc2047 = options->rfc2047;
	
	while (options->charsets[n])
		n++;
	
	clone->charsets = g_malloc (sizeof (char *) * (n + 1));
	for (i = 0; i < n; i++)
		clone->charsets[i] = g_strdup (options->charsets[i]);
	clone->charsets[i] = NULL;
	
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
 * g_mime_parser_options_get_address_parser_compliance_mode:
 * @options: a #GMimeParserOptions
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
g_mime_parser_options_get_address_parser_compliance_mode (GMimeParserOptions *options)
{
	g_return_val_if_fail (options != NULL, GMIME_RFC_COMPLIANCE_LOOSE);
	
	return options->addresses;
}


/**
 * g_mime_parser_options_set_address_parser_compliance_mode:
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
g_mime_parser_options_set_address_parser_compliance_mode (GMimeParserOptions *options, GMimeRfcComplianceMode mode)
{
	g_return_if_fail (options != NULL);
	
	options->addresses = mode;
}


/**
 * g_mime_parser_options_get_parameter_compliance_mode:
 * @options: a #GMimeParserOptions
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
	g_return_val_if_fail (options != NULL, GMIME_RFC_COMPLIANCE_LOOSE);
	
	return options->parameters;
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
 * @options: a #GMimeParserOptions
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
	g_return_val_if_fail (options != NULL, GMIME_RFC_COMPLIANCE_LOOSE);
	
	return options->rfc2047;
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
 * @options: a #GMimeParserOptions
 *
 * Gets the fallback charsets to try when decoding 8-bit headers.
 *
 * Returns: a %NULL-terminated list of charsets to try when decoding
 * 8-bit headers.
 **/
const char **
g_mime_parser_options_get_fallback_charsets (GMimeParserOptions *options)
{
	g_return_val_if_fail (options != NULL, (const char **) default_charsets);
	
	return (const char **) options->charsets;
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
