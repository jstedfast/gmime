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

#include <ctype.h>
#include <string.h>

#include "gmime-common.h"
#include "gmime-disposition.h"
#include "gmime-utils.h"
#include "gmime-parse-utils.h"
#include "gmime-table-private.h"
#include "gmime-internal.h"
#include "gmime-events.h"


#include "gmime-object-private.h"

/**
 * SECTION: gmime-disposition
 * @title: GMimeContentDisposition
 * @short_description: Content-Disposition fields
 * @see_also:
 *
 * A #GMimeContentDisposition represents the pre-parsed contents of a
 * Content-Disposition header field.
 **/


static void g_mime_content_disposition_class_init (GMimeContentDispositionClass *klass);
static void g_mime_content_disposition_init (GMimeContentDisposition *disposition);
static void g_mime_content_disposition_finalize (GObject *object);

static void param_list_changed (GMimeParamList *list, gpointer args, GMimeContentDisposition *disposition);


static GObjectClass *parent_class = NULL;


G_DEFINE_FINAL_TYPE (GMimeContentDisposition, g_mime_content_disposition, G_TYPE_OBJECT)


static void
g_mime_content_disposition_class_init (GMimeContentDispositionClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	
	parent_class = g_type_class_ref (G_TYPE_OBJECT);
	
	object_class->finalize = g_mime_content_disposition_finalize;
}

static void
g_mime_content_disposition_init (GMimeContentDisposition *disposition)
{
	disposition->changed = g_mime_event_new ((GObject *) disposition);
	disposition->params = g_mime_param_list_new ();
	disposition->disposition = NULL;
	
	g_mime_event_add (disposition->params->changed, (GMimeEventCallback) param_list_changed, disposition);
}

static void
g_mime_content_disposition_finalize (GObject *object)
{
	GMimeContentDisposition *disposition = (GMimeContentDisposition *) object;
	
	g_mime_event_remove (disposition->params->changed, (GMimeEventCallback) param_list_changed, disposition);
	g_mime_event_free (disposition->changed);
	g_object_unref (disposition->params);
	g_free (disposition->disposition);
	
	G_OBJECT_CLASS (parent_class)->finalize (object);
}


static void
param_list_changed (GMimeParamList *list, gpointer args, GMimeContentDisposition *disposition)
{
	g_mime_event_emit (disposition->changed, NULL);
}


/**
 * g_mime_content_disposition_new:
 *
 * Creates a new #GMimeContentDisposition object.
 *
 * Returns: a new #GMimeContentDisposition object.
 **/
GMimeContentDisposition *
g_mime_content_disposition_new (void)
{
	GMimeContentDisposition *disposition;
	
	disposition = g_object_new (GMIME_TYPE_CONTENT_DISPOSITION, NULL);
	disposition->disposition = g_strdup (GMIME_DISPOSITION_ATTACHMENT);
	
	return disposition;
}


/**
 * g_mime_content_disposition_parse:
 * @options: (nullable): a #GMimeParserOptions or %NULL
 * @str: Content-Disposition field value
 *
 * Parses the input string using best-effort recovery, accepting arbitrary
 * disposition text and retaining partial parameters. Use
 * g_mime_content_disposition_parse_full() for all-or-nothing parsing.
 *
 * Returns: (transfer full): a new #GMimeContentDisposition object.
 **/
GMimeContentDisposition *
g_mime_content_disposition_parse (GMimeParserOptions *options, const char *str)
{
	return _g_mime_content_disposition_parse (options, str, -1);
}

/**
 * g_mime_content_disposition_parse_full:
 * @options: (nullable): parser options, or %NULL for the defaults
 * @str: Content-Disposition field value
 * @length: byte length, or -1 for a NUL-terminated string
 * @error: (nullable): return location for a #GError
 *
 * Parses the complete input without recovering missing or quoted disposition
 * tokens or ignoring malformed parameters. Syntax errors return %NULL with
 * GMIME_ERROR_PARSE_ERROR and a byte offset relative to @str.
 * Explicitly bounded input need not be NUL-terminated but cannot contain
 * embedded NUL bytes.
 *
 * Comments and whitespace are accepted around the disposition atom.
 * Each non-ASCII atom byte becomes '?' following ASCII decoding.
 * Parameters use the compliance and duplicate precedence rules of
 * g_mime_param_list_parse_full().
 *
 * Returns: (nullable) (transfer full): the parsed disposition, or %NULL on error.
 **/
GMimeContentDisposition *
g_mime_content_disposition_parse_full (GMimeParserOptions *options, const char *str, gssize length, GError **error)
{
	GMimeContentDisposition *disposition = NULL;
	const char *inptr, *start, *end;
	GMimeParamList *params;
	char *text, *p;

	g_return_val_if_fail (str != NULL, NULL);
	g_return_val_if_fail (length >= -1, NULL);
	g_return_val_if_fail (error == NULL || *error == NULL, NULL);

	if (!(text = _g_mime_parse_text (str, length, error)))
		return NULL;

	inptr = text;
	if (!_g_mime_parse_cfws (text, &inptr, error))
		goto done;

	start = inptr;
	while (is_atom (*inptr))
		inptr++;

	if (inptr == start) {
		_g_mime_parse_error (error, text, inptr, "Invalid disposition atom");
		goto done;
	}

	end = inptr;
	if (!_g_mime_parse_cfws (text, &inptr, error))
		goto done;

	if (*inptr && *inptr != ';') {
		_g_mime_parse_error (error, text, inptr, "Expected parameter list separator");
		goto done;
	}

	if (*inptr)
		inptr++;

	if (!(params = _g_mime_param_list_parse_full (options, text, inptr, error)))
		goto done;

	disposition = g_object_new (GMIME_TYPE_CONTENT_DISPOSITION, NULL);
	disposition->disposition = g_strndup (start, end - start);
	for (p = disposition->disposition; *p; p++) {
		if ((guchar) *p >= 128)
			*p = '?';
	}

	g_mime_event_add (params->changed, (GMimeEventCallback) param_list_changed, disposition);
	g_object_unref (disposition->params);
	disposition->params = params;

done:
	g_free (text);
	return disposition;
}

GMimeContentDisposition *
_g_mime_content_disposition_parse (GMimeParserOptions *options, const char *str, gint64 offset)
{
	GMimeContentDisposition *disposition;
	const char *inptr = str;
	GMimeParamList *params;
	char *value;
	
	if (str == NULL)
		return g_mime_content_disposition_new ();
	
	disposition = g_object_new (GMIME_TYPE_CONTENT_DISPOSITION, NULL);
	
	/* get content disposition part */
	
	/* find ; or \0 */
	while (*inptr && *inptr != ';')
		inptr++;
	
	value = g_strndup (str, (size_t) (inptr - str));
	disposition->disposition = g_strstrip (value);
	
	/* parse the parameters, if any */
	if (*inptr++ == ';' && *inptr && (params = _g_mime_param_list_parse (options, inptr, offset))) {
		g_mime_event_add (params->changed, (GMimeEventCallback) param_list_changed, disposition);
		g_object_unref (disposition->params);
		disposition->params = params;
	}
	
	return disposition;
}


/**
 * g_mime_content_disposition_clone:
 * @disposition: a #GMimeContentDisposition
 *
 * Copies the disposition and all parameters, including their charset,
 * language, encoding method, and quoting preference. Parameters are copied
 * independently in their original order, retaining spelling and any duplicate
 * names recovered by legacy parsing. Legacy disposition text is preserved.
 *
 * Returns: (transfer full): an independent copy of @disposition.
 **/
GMimeContentDisposition *
g_mime_content_disposition_clone (GMimeContentDisposition *disposition)
{
	GMimeContentDisposition *clone;

	g_return_val_if_fail (GMIME_IS_CONTENT_DISPOSITION (disposition), NULL);

	clone = g_object_new (GMIME_TYPE_CONTENT_DISPOSITION, NULL);
	clone->disposition = g_strdup (disposition->disposition);
	g_object_unref (clone->params);
	clone->params = _g_mime_param_list_clone (disposition->params);
	g_mime_event_add (clone->params->changed, (GMimeEventCallback) param_list_changed, clone);

	return clone;
}

/**
 * g_mime_content_disposition_set_disposition:
 * @disposition: a #GMimeContentDisposition object
 * @value: disposition value
 *
 * Sets the disposition to @value which may be one of
 * #GMIME_DISPOSITION_ATTACHMENT or #GMIME_DISPOSITION_INLINE or, by
 * your choice, any other string which would indicate how the MIME
 * part should be displayed by the MUA.
 **/
void
g_mime_content_disposition_set_disposition (GMimeContentDisposition *disposition, const char *value)
{
	char *buf;
	
	g_return_if_fail (GMIME_IS_CONTENT_DISPOSITION (disposition));
	g_return_if_fail (value != NULL);
	
	if (!g_strcmp0 (disposition->disposition, value))
		return;

	buf = g_strdup (value);
	g_free (disposition->disposition);
	disposition->disposition = buf;
	
	g_mime_event_emit (disposition->changed, NULL);
}


/**
 * g_mime_content_disposition_get_disposition:
 * @disposition: a #GMimeContentDisposition object
 *
 * Gets the disposition or %NULL on fail.
 *
 * Returns: the disposition string which is probably one of
 * #GMIME_DISPOSITION_ATTACHMENT or #GMIME_DISPOSITION_INLINE.
 **/
const char *
g_mime_content_disposition_get_disposition (GMimeContentDisposition *disposition)
{
	g_return_val_if_fail (GMIME_IS_CONTENT_DISPOSITION (disposition), NULL);
	
	return disposition->disposition;
}


/**
 * g_mime_content_disposition_set_filename:
 * @disposition: a #GMimeContentDisposition
 * @filename: (nullable): the UTF-8 filename, or %NULL to remove the parameter
 *
 * Sets the filename parameter without trimming or otherwise normalizing it.
 * An empty string remains an explicit empty value. This does not also set
 * the name parameter on an owning entity's Content-Type.
 **/
void
g_mime_content_disposition_set_filename (GMimeContentDisposition *disposition, const char *filename)
{
	g_return_if_fail (GMIME_IS_CONTENT_DISPOSITION (disposition));

	if (filename)
		g_mime_param_list_set_parameter (disposition->params, "filename", filename);
	else
		g_mime_param_list_remove (disposition->params, "filename");
}

/**
 * g_mime_content_disposition_get_filename:
 * @disposition: a #GMimeContentDisposition
 *
 * Gets the filename parameter without falling back to an owning entity's
 * Content-Type name parameter.
 *
 * Returns: (nullable): the borrowed UTF-8 filename, or %NULL if absent.
 **/
const char *
g_mime_content_disposition_get_filename (GMimeContentDisposition *disposition)
{
	return g_mime_content_disposition_get_parameter (disposition, "filename");
}

static GDateTime *
disposition_get_date (GMimeContentDisposition *disposition, const char *name)
{
	const char *value = g_mime_content_disposition_get_parameter (disposition, name);

	return value ? g_mime_utils_header_decode_date (value) : NULL;
}

static void
disposition_set_date (GMimeContentDisposition *disposition, const char *name, GDateTime *date)
{
	char *value;

	g_return_if_fail (GMIME_IS_CONTENT_DISPOSITION (disposition));

	if (date) {
		value = g_mime_utils_header_format_date (date);
		if (!value)
			return;
		g_mime_param_list_set_parameter (disposition->params, name, value);
		g_free (value);
	} else {
		g_mime_param_list_remove (disposition->params, name);
	}
}

/**
 * g_mime_content_disposition_set_creation_date:
 * @disposition: a #GMimeContentDisposition
 * @date: (nullable): the creation date, or %NULL to remove the parameter
 *
 * Sets the creation-date parameter using g_mime_utils_header_format_date().
 * The date is formatted to whole seconds; no reference to @date is retained.
 * Existing parameter metadata is preserved and changes notify owning entities.
 **/
void
g_mime_content_disposition_set_creation_date (GMimeContentDisposition *disposition, GDateTime *date)
{
	disposition_set_date (disposition, "creation-date", date);
}

/**
 * g_mime_content_disposition_get_creation_date:
 * @disposition: a #GMimeContentDisposition
 *
 * Decodes the creation-date parameter without modifying its stored text.
 *
 * Returns: (transfer full) (nullable): a newly allocated date, or %NULL if the
 * parameter is absent, empty, or cannot be parsed. Free with g_date_time_unref().
 **/
GDateTime *
g_mime_content_disposition_get_creation_date (GMimeContentDisposition *disposition)
{
	return disposition_get_date (disposition, "creation-date");
}

/**
 * g_mime_content_disposition_set_modification_date:
 * @disposition: a #GMimeContentDisposition
 * @date: (nullable): the modification date, or %NULL to remove the parameter
 *
 * Sets the modification-date parameter using g_mime_utils_header_format_date().
 * The date is formatted to whole seconds; no reference to @date is retained.
 * Existing parameter metadata is preserved and changes notify owning entities.
 **/
void
g_mime_content_disposition_set_modification_date (GMimeContentDisposition *disposition, GDateTime *date)
{
	disposition_set_date (disposition, "modification-date", date);
}

/**
 * g_mime_content_disposition_get_modification_date:
 * @disposition: a #GMimeContentDisposition
 *
 * Decodes the modification-date parameter without modifying its stored text.
 *
 * Returns: (transfer full) (nullable): a newly allocated date, or %NULL if the
 * parameter is absent, empty, or cannot be parsed. Free with g_date_time_unref().
 **/
GDateTime *
g_mime_content_disposition_get_modification_date (GMimeContentDisposition *disposition)
{
	return disposition_get_date (disposition, "modification-date");
}

/**
 * g_mime_content_disposition_set_read_date:
 * @disposition: a #GMimeContentDisposition
 * @date: (nullable): the last-read date, or %NULL to remove the parameter
 *
 * Sets the read-date parameter using g_mime_utils_header_format_date().
 * The date is formatted to whole seconds; no reference to @date is retained.
 * Existing parameter metadata is preserved and changes notify owning entities.
 **/
void
g_mime_content_disposition_set_read_date (GMimeContentDisposition *disposition, GDateTime *date)
{
	disposition_set_date (disposition, "read-date", date);
}

/**
 * g_mime_content_disposition_get_read_date:
 * @disposition: a #GMimeContentDisposition
 *
 * Decodes the read-date parameter without modifying its stored text.
 *
 * Returns: (transfer full) (nullable): a newly allocated date, or %NULL if the
 * parameter is absent, empty, or cannot be parsed. Free with g_date_time_unref().
 **/
GDateTime *
g_mime_content_disposition_get_read_date (GMimeContentDisposition *disposition)
{
	return disposition_get_date (disposition, "read-date");
}

/**
 * g_mime_content_disposition_set_size:
 * @disposition: a #GMimeContentDisposition
 * @size: the size in bytes, or -1 to remove the parameter
 *
 * Sets the size parameter to a nonnegative decimal integer. Values below -1
 * are invalid. Existing parameter metadata is preserved and changes notify
 * owning entities.
 **/
void
g_mime_content_disposition_set_size (GMimeContentDisposition *disposition, gint64 size)
{
	char value[32];

	g_return_if_fail (GMIME_IS_CONTENT_DISPOSITION (disposition));
	g_return_if_fail (size >= -1);

	if (size == -1) {
		g_mime_param_list_remove (disposition->params, "size");
	} else {
		g_snprintf (value, sizeof (value), "%" G_GINT64_FORMAT, size);
		g_mime_param_list_set_parameter (disposition->params, "size", value);
	}
}

/**
 * g_mime_content_disposition_get_size:
 * @disposition: a #GMimeContentDisposition
 *
 * Gets the size parameter without modifying its stored text. Only ASCII
 * decimal digits are accepted; signs, whitespace, and values greater than
 * %G_MAXINT64 are invalid. Leading zeros are allowed.
 *
 * Returns: the nonnegative size in bytes, or -1 if the parameter is absent,
 * empty, or invalid.
 **/
gint64
g_mime_content_disposition_get_size (GMimeContentDisposition *disposition)
{
	const char *value;
	gint64 size = 0;
	int digit;

	g_return_val_if_fail (GMIME_IS_CONTENT_DISPOSITION (disposition), -1);

	value = g_mime_content_disposition_get_parameter (disposition, "size");
	if (!value || !*value)
		return -1;

	for (; *value; value++) {
		if (!g_ascii_isdigit (*value))
			return -1;
		digit = *value - '0';
		if (size > (G_MAXINT64 - digit) / 10)
			return -1;
		size = size * 10 + digit;
	}

	return size;
}

/**
 * g_mime_content_disposition_get_parameters:
 * @disposition: a #GMimeContentDisposition object
 *
 * Gets the Content-Disposition parameter list.
 *
 * Returns: (transfer none): the Content-Disposition's parameter list.
 **/
GMimeParamList *
g_mime_content_disposition_get_parameters (GMimeContentDisposition *disposition)
{
	g_return_val_if_fail (GMIME_IS_CONTENT_DISPOSITION (disposition), NULL);
	
	return disposition->params;
}


/**
 * g_mime_content_disposition_set_parameter:
 * @disposition: a #GMimeContentDisposition object
 * @name: parameter name
 * @value: parameter value
 *
 * Sets a parameter on the Content-Disposition.
 *
 * Note: The @name should be in US-ASCII while the @value should be in
 * UTF-8.
 **/
void
g_mime_content_disposition_set_parameter (GMimeContentDisposition *disposition, const char *name, const char *value)
{
	g_return_if_fail (GMIME_IS_CONTENT_DISPOSITION (disposition));
	
	g_mime_param_list_set_parameter (disposition->params, name, value);
}


/**
 * g_mime_content_disposition_get_parameter:
 * @disposition: a #GMimeContentDisposition object
 * @name: parameter name
 *
 * Gets the parameter value specified by @name if it's available.
 *
 * Returns: the value of the requested parameter or %NULL if the
 * parameter is not set. If the parameter is set, the returned string
 * will be in UTF-8.
 **/
const char *
g_mime_content_disposition_get_parameter (GMimeContentDisposition *disposition, const char *name)
{
	GMimeParam *param;
	
	g_return_val_if_fail (GMIME_IS_CONTENT_DISPOSITION (disposition), NULL);
	
	if (!(param = g_mime_param_list_get_parameter (disposition->params, name)))
		return NULL;
	
	return param->value;
}


/**
 * g_mime_content_disposition_is_attachment:
 * @disposition: a #GMimeContentDisposition object
 *
 * Determines if a Content-Disposition has a value of "attachment".
 *
 * Returns: %TRUE if the value matches "attachment", otherwise %FALSE.
 **/
gboolean
g_mime_content_disposition_is_attachment (GMimeContentDisposition *disposition)
{
	g_return_val_if_fail (GMIME_IS_CONTENT_DISPOSITION (disposition), FALSE);
	
	return !g_ascii_strcasecmp (disposition->disposition, "attachment");
}


/**
 * g_mime_content_disposition_set_is_attachment:
 * @disposition: a #GMimeContentDisposition
 * @attachment: %TRUE for attachment, or %FALSE for inline
 *
 * Sets the disposition to the lowercase "attachment" or "inline" token.
 * Existing parameters are retained. Changes notify owning entities, while
 * assigning an identical token leaves raw headers untouched.
 **/
void
g_mime_content_disposition_set_is_attachment (GMimeContentDisposition *disposition, gboolean attachment)
{
	g_mime_content_disposition_set_disposition (disposition, attachment ?
						 GMIME_DISPOSITION_ATTACHMENT : GMIME_DISPOSITION_INLINE);
}

/**
 * g_mime_content_disposition_to_string:
 * @disposition: a #GMimeContentDisposition
 *
 * Formats a display string beginning with "Content-Disposition: ".
 * Parameter values are always quoted and escaped, without transport
 * encoding, folding, or an appended newline. This is not a transport-safe
 * header; use g_mime_content_disposition_encode() when serializing a MIME
 * message.
 *
 * Returns: (transfer full): the unencoded display string.
 **/
char *
g_mime_content_disposition_to_string (GMimeContentDisposition *disposition)
{
	char *params, *str;

	g_return_val_if_fail (GMIME_IS_CONTENT_DISPOSITION (disposition), NULL);

	params = g_mime_param_list_to_string (disposition->params);
	str = g_strdup_printf ("Content-Disposition: %s%s",
			       disposition->disposition ? disposition->disposition : "", params);
	g_free (params);

	return str;
}

/**
 * g_mime_content_disposition_encode:
 * @disposition: a #GMimeContentDisposition object
 * @options: (nullable): a #GMimeFormatOptions or %NULL
 *
 * Encodes the Content-Disposition field value, including leading whitespace
 * and a terminating newline. The field name is not included.
 *
 * Returns: a new string containing the encoded header value.
 **/
char *
g_mime_content_disposition_encode (GMimeContentDisposition *disposition, GMimeFormatOptions *options)
{
	char *raw_value;
	GString *str;
	guint len, n;
	
	g_return_val_if_fail (GMIME_IS_CONTENT_DISPOSITION (disposition), NULL);
	
	/* we need to have this so wrapping is correct */
	str = g_string_new ("Content-Disposition:");
	n = str->len;
	
	g_string_append_c (str, ' ');
	g_string_append (str, disposition->disposition);
	g_mime_param_list_encode (disposition->params, options, TRUE, str);
	len = str->len - n;
	
	raw_value = g_string_free (str, FALSE);
	
	memmove (raw_value, raw_value + n, len + 1);
	
	return raw_value;
}
