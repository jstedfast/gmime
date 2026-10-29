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

#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>

#include "gmime-common.h"
#include "gmime-charset.h"
#include "gmime-content-type.h"
#include "gmime-parse-utils.h"
#include "gmime-table-private.h"
#include "gmime-internal.h"
#include "gmime-events.h"
#include "gmime-iconv.h"


#ifdef ENABLE_WARNINGS
#define w(x) x
#else
#define w(x)
#endif /* ENABLE_WARNINGS */

#define d(x)


#include "gmime-object-private.h"

/**
 * SECTION: gmime-content-type
 * @title: GMimeContentType
 * @short_description: Content-Type fields
 * @see_also:
 *
 * A #GMimeContentType represents the pre-parsed contents of a
 * Content-Type header field.
 **/


static void g_mime_content_type_class_init (GMimeContentTypeClass *klass);
static void g_mime_content_type_init (GMimeContentType *content_type);
static void g_mime_content_type_finalize (GObject *object);

static void param_list_changed (GMimeParamList *list, gpointer args, GMimeContentType *content_type);


static GObjectClass *parent_class = NULL;


G_DEFINE_FINAL_TYPE (GMimeContentType, g_mime_content_type, G_TYPE_OBJECT)


static void
g_mime_content_type_class_init (GMimeContentTypeClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	
	parent_class = g_type_class_ref (G_TYPE_OBJECT);
	
	object_class->finalize = g_mime_content_type_finalize;
}

static void
g_mime_content_type_init (GMimeContentType *content_type)
{
	content_type->changed = g_mime_event_new ((GObject *) content_type);
	content_type->params = g_mime_param_list_new ();
	content_type->subtype = NULL;
	content_type->type = NULL;
	
	g_mime_event_add (content_type->params->changed, (GMimeEventCallback) param_list_changed, content_type);
}

static void
g_mime_content_type_finalize (GObject *object)
{
	GMimeContentType *content_type = (GMimeContentType *) object;
	
	g_mime_event_remove (content_type->params->changed, (GMimeEventCallback) param_list_changed, content_type);
	g_mime_event_free (content_type->changed);
	g_object_unref (content_type->params);
	g_free (content_type->subtype);
	g_free (content_type->type);
	
	G_OBJECT_CLASS (parent_class)->finalize (object);
}


static void
param_list_changed (GMimeParamList *list, gpointer args, GMimeContentType *content_type)
{
	g_mime_event_emit (content_type->changed, NULL);
}


/**
 * g_mime_content_type_new:
 * @type: the MIME type or %NULL for the default value
 * @subtype: the MIME subtype or %NULL for the default value
 *
 * Creates a Content-Type object with type @type and subtype @subtype.
 *
 * Returns: a new #GMimeContentType object.
 **/
GMimeContentType *
g_mime_content_type_new (const char *type, const char *subtype)
{
	GMimeContentType *content_type;
	
	content_type = g_object_new (GMIME_TYPE_CONTENT_TYPE, NULL);
	
	if (type && *type && subtype && *subtype) {
		content_type->type = g_strdup (type);
		content_type->subtype = g_strdup (subtype);
	} else {
		if (type && *type) {
			content_type->type = g_strdup (type);
			if (!g_ascii_strcasecmp (type, "text")) {
				content_type->subtype = g_strdup ("plain");
			} else if (!g_ascii_strcasecmp (type, "multipart")) {
				content_type->subtype = g_strdup ("mixed");
			} else {
				g_free (content_type->type);
				content_type->type = g_strdup ("application");
				content_type->subtype = g_strdup ("octet-stream");
			}
		} else {
			content_type->type = g_strdup ("application");
			content_type->subtype = g_strdup ("octet-stream");
		}
		
		w(g_warning ("Invalid or incomplete type: %s%s%s: defaulting to %s/%s",
			     type ? type : "", subtype ? "/" : "", subtype ? subtype : "",
			     content_type->type, content_type->subtype));
	}
	
	return content_type;
}


/**
 * g_mime_content_type_parse:
 * @options: (nullable): a #GMimeParserOptions or %NULL
 * @str: input string containing a content-type (and params)
 *
 * Parses the input string using best-effort recovery, substituting a
 * default content type on invalid input and retaining partial parameters.
 * Use g_mime_content_type_parse_full() for all-or-nothing parsing.
 *
 * Returns: (transfer full): a new #GMimeContentType object.
 **/
GMimeContentType *
g_mime_content_type_parse (GMimeParserOptions *options, const char *str)
{
	return _g_mime_content_type_parse (options, str, -1);
}

/**
 * g_mime_content_type_parse_full:
 * @options: (nullable): parser options, or %NULL for the defaults
 * @str: Content-Type field value
 * @length: byte length, or -1 for a NUL-terminated string
 * @error: (nullable): return location for a #GError
 *
 * Parses the complete input without substituting a default content type
 * or ignoring malformed parameters. Syntax errors return %NULL with
 * GMIME_ERROR_PARSE_ERROR and a byte offset relative to @str.
 * Explicitly bounded input need not be NUL-terminated but cannot contain
 * embedded NUL bytes.
 *
 * Comments and whitespace are accepted between tokens. The media type is
 * an ASCII atom excluding '/', and the subtype is an ASCII MIME token
 * that may also contain '/'. Parameters use the compliance and duplicate
 * precedence rules of g_mime_param_list_parse_full().
 *
 * Returns: (nullable) (transfer full): the parsed content type, or %NULL on error.
 **/
GMimeContentType *
g_mime_content_type_parse_full (GMimeParserOptions *options, const char *str, gssize length, GError **error)
{
	GMimeContentType *content_type = NULL;
	const char *inptr, *type, *type_end, *subtype, *subtype_end;
	GMimeParamList *params;
	char *text;

	g_return_val_if_fail (str != NULL, NULL);
	g_return_val_if_fail (length >= -1, NULL);
	g_return_val_if_fail (error == NULL || *error == NULL, NULL);

	if (!(text = _g_mime_parse_text (str, length, error)))
		return NULL;

	inptr = text;
	if (!_g_mime_parse_cfws (text, &inptr, error))
		goto done;

	type = inptr;
	while ((guchar) *inptr < 128 && is_atom (*inptr) && *inptr != '/')
		inptr++;

	if (inptr == type) {
		_g_mime_parse_error (error, text, inptr, "Invalid media type");
		goto done;
	}

	type_end = inptr;
	if (!_g_mime_parse_cfws (text, &inptr, error))
		goto done;

	if (*inptr != '/') {
		_g_mime_parse_error (error, text, inptr, "Expected media type slash");
		goto done;
	}

	inptr++;
	if (!_g_mime_parse_cfws (text, &inptr, error))
		goto done;

	subtype = inptr;
	while ((guchar) *inptr < 127 && (is_ttoken (*inptr) || *inptr == '/'))
		inptr++;

	if (inptr == subtype) {
		_g_mime_parse_error (error, text, inptr, "Invalid media subtype");
		goto done;
	}

	subtype_end = inptr;
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

	content_type = g_object_new (GMIME_TYPE_CONTENT_TYPE, NULL);
	content_type->type = g_strndup (type, type_end - type);
	content_type->subtype = g_strndup (subtype, subtype_end - subtype);
	g_mime_event_add (params->changed, (GMimeEventCallback) param_list_changed, content_type);
	g_object_unref (content_type->params);
	content_type->params = params;

done:
	g_free (text);
	return content_type;
}

GMimeContentType *
_g_mime_content_type_parse (GMimeParserOptions *options, const char *str, gint64 offset)
{
	GMimeContentType *content_type;
	const char *inptr = str;
	GMimeParamList *params;
	char *type, *subtype;
	
	g_return_val_if_fail (str != NULL, NULL);
	
	if (!g_mime_parse_content_type (&inptr, &type, &subtype)) {
		_g_mime_parser_options_warn (options, offset, GMIME_WARN_INVALID_CONTENT_TYPE, str);
		return g_mime_content_type_new ("application", "octet-stream");
	}
	
	content_type = g_object_new (GMIME_TYPE_CONTENT_TYPE, NULL);
	content_type->subtype = subtype;
	content_type->type = type;
	
	/* skip past any remaining junk that shouldn't be here... */
	skip_cfws (&inptr);
	if (*inptr && *inptr != ';') {
		_g_mime_parser_options_warn (options, offset, GMIME_WARN_INVALID_CONTENT_TYPE, str);
		while (*inptr && *inptr != ';')
			inptr++;
	}
	
	if (*inptr++ == ';' && *inptr && (params = _g_mime_param_list_parse (options, inptr, offset))) {
		g_mime_event_add (params->changed, (GMimeEventCallback) param_list_changed, content_type);
		g_object_unref (content_type->params);
		content_type->params = params;
	}
	
	return content_type;
}


/**
 * g_mime_content_type_clone:
 * @content_type: a #GMimeContentType
 *
 * Copies the media type, subtype, and all parameters, including their
 * charset, language, encoding method, and quoting preference. Parameters
 * are copied independently in their original order, retaining spelling
 * and any duplicate names recovered by legacy parsing.
 *
 * Returns: (transfer full): an independent copy of @content_type.
 **/
GMimeContentType *
g_mime_content_type_clone (GMimeContentType *content_type)
{
	GMimeContentType *clone;

	g_return_val_if_fail (GMIME_IS_CONTENT_TYPE (content_type), NULL);

	clone = g_object_new (GMIME_TYPE_CONTENT_TYPE, NULL);
	clone->type = g_strdup (content_type->type);
	clone->subtype = g_strdup (content_type->subtype);
	g_object_unref (clone->params);
	clone->params = _g_mime_param_list_clone (content_type->params);
	g_mime_event_add (clone->params->changed, (GMimeEventCallback) param_list_changed, clone);

	return clone;
}

/**
 * g_mime_content_type_get_mime_type:
 * @content_type: a #GMimeContentType
 *
 * Allocates a string buffer containing the type and subtype defined
 * by the @content_type.
 *
 * Returns: an allocated string containing the type and subtype of the
 * content-type in the format: type/subtype.
 **/
char *
g_mime_content_type_get_mime_type (GMimeContentType *content_type)
{
	char *mime_type;
	
	g_return_val_if_fail (GMIME_IS_CONTENT_TYPE (content_type), NULL);
	
	/* type and subtype should never be NULL, but check anyway */
	mime_type = g_strdup_printf ("%s/%s", content_type->type ? content_type->type : "text",
				     content_type->subtype ? content_type->subtype : "plain");
	
	return mime_type;
}


/**
 * g_mime_content_type_to_string:
 * @content_type: a #GMimeContentType
 *
 * Formats a display string beginning with "Content-Type: ". Parameter
 * values are always quoted and escaped, without transport encoding,
 * folding, or an appended newline. This is not a transport-safe header;
 * use g_mime_content_type_encode() when serializing a MIME message.
 *
 * Returns: (transfer full): the unencoded display string.
 **/
char *
g_mime_content_type_to_string (GMimeContentType *content_type)
{
	char *params, *str;

	g_return_val_if_fail (GMIME_IS_CONTENT_TYPE (content_type), NULL);

	params = g_mime_param_list_to_string (content_type->params);
	str = g_strdup_printf ("Content-Type: %s/%s%s",
			       content_type->type ? content_type->type : "text",
			       content_type->subtype ? content_type->subtype : "plain", params);
	g_free (params);

	return str;
}

/**
 * g_mime_content_type_encode:
 * @content_type: a #GMimeContentType
 * @options: (nullable): a #GMimeFormatOptions or %NULL
 *
 * Encodes the Content-Type field value, including leading whitespace and
 * a terminating newline. The field name is not included.
 *
 * Returns: a new string containing the encoded header value.
 **/
char *
g_mime_content_type_encode (GMimeContentType *content_type, GMimeFormatOptions *options)
{
	char *raw_value;
	GString *str;
	guint len, n;
	
	g_return_val_if_fail (GMIME_IS_CONTENT_TYPE (content_type), NULL);
	
	/* we need to have this so wrapping is correct */
	str = g_string_new ("Content-Type:");
	n = str->len;
	
	g_string_append_c (str, ' ');
	g_string_append (str, content_type->type ? content_type->type : "text");
	g_string_append_c (str, '/');
	g_string_append (str, content_type->subtype ? content_type->subtype : "plain");
	
	g_mime_param_list_encode (content_type->params, options, TRUE, str);
	len = str->len - n;
	
	raw_value = g_string_free (str, FALSE);
	
	memmove (raw_value, raw_value + n, len + 1);
	
	return raw_value;
}


/**
 * g_mime_content_type_is_type:
 * @content_type: a #GMimeContentType
 * @type: MIME type to compare against
 * @subtype: MIME subtype to compare against
 *
 * Compares the given type and subtype with that of the given mime
 * type object.
 *
 * Returns: %TRUE if the MIME types match or %FALSE otherwise. You may
 * use "*" in place of @type and/or @subtype as a wilcard.
 **/
gboolean
g_mime_content_type_is_type (GMimeContentType *content_type, const char *type, const char *subtype)
{
	g_return_val_if_fail (GMIME_IS_CONTENT_TYPE (content_type), FALSE);
	g_return_val_if_fail (content_type->type != NULL, FALSE);
	g_return_val_if_fail (content_type->subtype != NULL, FALSE);
	g_return_val_if_fail (type != NULL, FALSE);
	g_return_val_if_fail (subtype != NULL, FALSE);
	
	if (!strcmp (type, "*") || !g_ascii_strcasecmp (content_type->type, type)) {
		if (!strcmp (subtype, "*")) {
			/* special case */
			return TRUE;
		}
		
		if (!g_ascii_strcasecmp (content_type->subtype, subtype))
			return TRUE;
	}
	
	return FALSE;
}


/**
 * g_mime_content_type_set_media_type:
 * @content_type: a #GMimeContentType
 * @type: media type
 *
 * Sets the Content-Type's media type.
 **/
void
g_mime_content_type_set_media_type (GMimeContentType *content_type, const char *type)
{
	char *buf;
	
	g_return_if_fail (GMIME_IS_CONTENT_TYPE (content_type));
	g_return_if_fail (type != NULL);
	
	if (!g_strcmp0 (content_type->type, type))
		return;

	buf = g_strdup (type);
	g_free (content_type->type);
	content_type->type = buf;
	
	g_mime_event_emit (content_type->changed, NULL);
}


/**
 * g_mime_content_type_get_media_type:
 * @content_type: a #GMimeContentType
 *
 * Gets the Content-Type's media type.
 *
 * Returns: the Content-Type's media type.
 **/
const char *
g_mime_content_type_get_media_type (GMimeContentType *content_type)
{
	g_return_val_if_fail (GMIME_IS_CONTENT_TYPE (content_type), NULL);
	
	return content_type->type;
}


/**
 * g_mime_content_type_set_media_subtype:
 * @content_type: a #GMimeContentType
 * @subtype: media subtype
 *
 * Sets the Content-Type's media subtype.
 **/
void
g_mime_content_type_set_media_subtype (GMimeContentType *content_type, const char *subtype)
{
	char *buf;
	
	g_return_if_fail (GMIME_IS_CONTENT_TYPE (content_type));
	g_return_if_fail (subtype != NULL);
	
	if (!g_strcmp0 (content_type->subtype, subtype))
		return;

	buf = g_strdup (subtype);
	g_free (content_type->subtype);
	content_type->subtype = buf;
	
	g_mime_event_emit (content_type->changed, NULL);
}


/**
 * g_mime_content_type_get_media_subtype:
 * @content_type: a #GMimeContentType
 *
 * Gets the Content-Type's media sub-type.
 *
 * Returns: the Content-Type's media sub-type.
 **/
const char *
g_mime_content_type_get_media_subtype (GMimeContentType *content_type)
{
	g_return_val_if_fail (GMIME_IS_CONTENT_TYPE (content_type), NULL);
	
	return content_type->subtype;
}


/**
 * g_mime_content_type_set_boundary:
 * @content_type: a #GMimeContentType
 * @boundary: (nullable): the UTF-8 boundary, or %NULL to remove the parameter
 *
 * Sets the boundary parameter. An empty string remains an explicit empty
 * value. Unlike g_mime_multipart_set_boundary(), %NULL removes the parameter
 * rather than generating a boundary.
 **/
void
g_mime_content_type_set_boundary (GMimeContentType *content_type, const char *boundary)
{
	g_return_if_fail (GMIME_IS_CONTENT_TYPE (content_type));

	if (boundary)
		g_mime_param_list_set_parameter (content_type->params, "boundary", boundary);
	else
		g_mime_param_list_remove (content_type->params, "boundary");
}

/**
 * g_mime_content_type_get_boundary:
 * @content_type: a #GMimeContentType
 *
 * Gets the boundary parameter without generating a value.
 *
 * Returns: (nullable): the borrowed boundary, or %NULL if absent.
 **/
const char *
g_mime_content_type_get_boundary (GMimeContentType *content_type)
{
	return g_mime_content_type_get_parameter (content_type, "boundary");
}

/**
 * g_mime_content_type_set_charset:
 * @content_type: a #GMimeContentType
 * @charset: (nullable): the UTF-8 charset name, or %NULL to remove the parameter
 *
 * Sets the charset parameter without resolving or normalizing its name.
 * An empty string remains an explicit empty value.
 **/
void
g_mime_content_type_set_charset (GMimeContentType *content_type, const char *charset)
{
	g_return_if_fail (GMIME_IS_CONTENT_TYPE (content_type));

	if (charset)
		g_mime_param_list_set_parameter (content_type->params, "charset", charset);
	else
		g_mime_param_list_remove (content_type->params, "charset");
}

/**
 * g_mime_content_type_get_charset:
 * @content_type: a #GMimeContentType
 *
 * Gets the charset parameter without resolving or normalizing its name.
 *
 * Returns: (nullable): the borrowed charset name, or %NULL if absent.
 **/
const char *
g_mime_content_type_get_charset (GMimeContentType *content_type)
{
	return g_mime_content_type_get_parameter (content_type, "charset");
}

static char *
resolve_charset_encoding (const char *charset, GError **error)
{
	iconv_t decoder;
	char *name;
	int errsv;

	if (!(name = _g_mime_charset_get_text_name (charset, error)))
		return NULL;

	/* Preserve native errno values to distinguish unsupported names
	 * from operational converter failures. */
	errno = 0;
	decoder = iconv_open ("UTF-8", g_mime_charset_iconv_name (name));

	if (decoder == (iconv_t) -1) {
		errsv = errno;
		g_set_error (error, G_CONVERT_ERROR, errsv == EINVAL ? G_CONVERT_ERROR_NO_CONVERSION : G_CONVERT_ERROR_FAILED,
			     "Cannot resolve charset '%s': %s", charset,
			     errsv ? g_strerror (errsv) : "unknown conversion error");
		g_free (name);

		return NULL;
	}

	iconv_close (decoder);

	return name;
}

/**
 * g_mime_content_type_set_charset_encoding:
 * @content_type: a #GMimeContentType
 * @charset: (nullable): a supported MIME charset name, or %NULL to remove it
 * @error: return location for an error, or %NULL
 *
 * Resolves @charset and assigns its canonical MIME name, following MimeKit's
 * ContentType.CharsetEncoding property. Unlike g_mime_content_type_set_charset(),
 * this validates the name before changing any parameters or their owning entity's headers.
 * Unsupported, empty, or non-token names report %G_CONVERT_ERROR.
 *
 * Generic UTF-16/32 names select little-endian encodings. MimeKit's historical
 * "unicode" alias selects UTF-8. Available encodings and aliases follow the
 * platform's iconv implementation; custom managed encodings are not supported.
 * Borrowed parameter values may safely be passed as @charset.
 *
 * Returns: %TRUE on success, or %FALSE on failure without changing the object.
 **/
gboolean
g_mime_content_type_set_charset_encoding (GMimeContentType *content_type, const char *charset, GError **error)
{
	char *name;

	g_return_val_if_fail (GMIME_IS_CONTENT_TYPE (content_type), FALSE);
	g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

	if (!charset) {
		g_mime_content_type_set_charset (content_type, NULL);
		return TRUE;
	}

	if (!(name = resolve_charset_encoding (charset, error)))
		return FALSE;

	g_mime_content_type_set_charset (content_type, name);
	g_free (name);

	return TRUE;
}

/**
 * g_mime_content_type_get_charset_encoding:
 * @content_type: a #GMimeContentType
 * @error: return location for an error, or %NULL
 *
 * Resolves the charset parameter to a supported canonical MIME charset name,
 * following MimeKit's ContentType.CharsetEncoding property. Missing, empty,
 * invalid, and unsupported names return %NULL without an error. Other converter
 * failures report %G_CONVERT_ERROR rather than masquerading as an unknown
 * charset. Resolution uses the same aliases as
 * g_mime_content_type_set_charset_encoding().
 *
 * This method does not change the parameter, its metadata, or raw headers.
 * The result is independent of later parameter changes.
 *
 * Returns: (nullable) (transfer full): the resolved charset name, or %NULL
 * if unavailable or on failure.
 **/
char *
g_mime_content_type_get_charset_encoding (GMimeContentType *content_type, GError **error)
{
	GError *local_error = NULL;
	const char *charset;
	char *name;

	g_return_val_if_fail (GMIME_IS_CONTENT_TYPE (content_type), NULL);
	g_return_val_if_fail (error == NULL || *error == NULL, NULL);

	if (!(charset = g_mime_content_type_get_charset (content_type)))
		return NULL;

	if (!(name = resolve_charset_encoding (charset, &local_error))) {
		if (g_error_matches (local_error, G_CONVERT_ERROR, G_CONVERT_ERROR_NO_CONVERSION))
			g_error_free (local_error);
		else
			g_propagate_error (error, local_error);
		return NULL;
	}

	return name;
}

/**
 * g_mime_content_type_set_format:
 * @content_type: a #GMimeContentType
 * @format: (nullable): the UTF-8 format, or %NULL to remove the parameter
 *
 * Sets the format parameter, typically "fixed" or "flowed" for text/plain.
 * Other values and explicit empty strings are preserved.
 **/
void
g_mime_content_type_set_format (GMimeContentType *content_type, const char *format)
{
	g_return_if_fail (GMIME_IS_CONTENT_TYPE (content_type));

	if (format)
		g_mime_param_list_set_parameter (content_type->params, "format", format);
	else
		g_mime_param_list_remove (content_type->params, "format");
}

/**
 * g_mime_content_type_get_format:
 * @content_type: a #GMimeContentType
 *
 * Gets the format parameter.
 *
 * Returns: (nullable): the borrowed format, or %NULL if absent.
 **/
const char *
g_mime_content_type_get_format (GMimeContentType *content_type)
{
	return g_mime_content_type_get_parameter (content_type, "format");
}

/**
 * g_mime_content_type_set_name:
 * @content_type: a #GMimeContentType
 * @name: (nullable): the UTF-8 display name, or %NULL to remove the parameter
 *
 * Sets the name parameter. Empty strings and whitespace are preserved.
 **/
void
g_mime_content_type_set_name (GMimeContentType *content_type, const char *name)
{
	g_return_if_fail (GMIME_IS_CONTENT_TYPE (content_type));

	if (name)
		g_mime_param_list_set_parameter (content_type->params, "name", name);
	else
		g_mime_param_list_remove (content_type->params, "name");
}

/**
 * g_mime_content_type_get_name:
 * @content_type: a #GMimeContentType
 *
 * Gets the name parameter.
 *
 * Returns: (nullable): the borrowed UTF-8 display name, or %NULL if absent.
 **/
const char *
g_mime_content_type_get_name (GMimeContentType *content_type)
{
	return g_mime_content_type_get_parameter (content_type, "name");
}

/**
 * g_mime_content_type_get_parameters:
 * @content_type: a #GMimeContentType
 *
 * Gets the Content-Type's parameter list.
 *
 * Returns: (transfer none): the Content-Type's parameter list.
 **/
GMimeParamList *
g_mime_content_type_get_parameters (GMimeContentType *content_type)
{
	g_return_val_if_fail (GMIME_IS_CONTENT_TYPE (content_type), NULL);
	
	return content_type->params;
}


/**
 * g_mime_content_type_set_parameter:
 * @content_type: a #GMimeContentType
 * @name: parameter name (aka attribute)
 * @value: parameter value
 *
 * Sets a parameter on the Content-Type.
 *
 * Note: The @name should be in US-ASCII while the @value should be in
 * UTF-8.
 **/
void
g_mime_content_type_set_parameter (GMimeContentType *content_type, const char *name, const char *value)
{
	g_return_if_fail (GMIME_IS_CONTENT_TYPE (content_type));
	
	g_mime_param_list_set_parameter (content_type->params, name, value);
}


/**
 * g_mime_content_type_get_parameter:
 * @content_type: a #GMimeContentType
 * @name: parameter name (aka attribute)
 *
 * Gets the parameter value specified by @name if it's available.
 *
 * Returns: the value of the requested parameter or %NULL if the
 * parameter is not set. If the parameter is set, the returned string
 * will be in UTF-8.
 **/
const char *
g_mime_content_type_get_parameter (GMimeContentType *content_type, const char *name)
{
	GMimeParam *param;
	
	g_return_val_if_fail (GMIME_IS_CONTENT_TYPE (content_type), NULL);
	
	if (!(param = g_mime_param_list_get_parameter (content_type->params, name)))
		return NULL;
	
	return param->value;
}
