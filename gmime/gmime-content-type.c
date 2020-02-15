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

#include <stdio.h>
#include <string.h>
#include <ctype.h>

#include "gmime-common.h"
#include "gmime-content-type.h"
#include "gmime-parse-utils.h"
#include "gmime-internal.h"
#include "gmime-events.h"


#ifdef ENABLE_WARNINGS
#define w(x) x
#else
#define w(x)
#endif /* ENABLE_WARNINGS */

#define d(x)


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
static void g_mime_content_type_init (GMimeContentType *content_type, GMimeContentTypeClass *klass);
static void g_mime_content_type_finalize (GObject *object);

static void param_list_changed (GMimeParamList *list, gpointer args, GMimeContentType *content_type);


static GObjectClass *parent_class = NULL;


GType
g_mime_content_type_get_type (void)
{
	static GType type = 0;
	
	if (!type) {
		static const GTypeInfo info = {
			sizeof (GMimeContentTypeClass),
			NULL, /* base_class_init */
			NULL, /* base_class_finalize */
			(GClassInitFunc) g_mime_content_type_class_init,
			NULL, /* class_finalize */
			NULL, /* class_data */
			sizeof (GMimeContentType),
			0,    /* n_preallocs */
			(GInstanceInitFunc) g_mime_content_type_init,
		};
		
		type = g_type_register_static (G_TYPE_OBJECT, "GMimeContentType", &info, 0);
	}
	
	return type;
}


static void
g_mime_content_type_class_init (GMimeContentTypeClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	
	parent_class = g_type_class_ref (G_TYPE_OBJECT);
	
	object_class->finalize = g_mime_content_type_finalize;
}

static void
g_mime_content_type_init (GMimeContentType *content_type, GMimeContentTypeClass *klass)
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
 * Parses the input string into a #GMimeContentType object.
 *
 * Returns: (transfer full): a new #GMimeContentType object.
 **/
GMimeContentType *
g_mime_content_type_parse (GMimeParserOptions *options, const char *str)
{
	return _g_mime_content_type_parse (options, str, -1);
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
	while (*inptr && *inptr != ';')
		inptr++;
	
	if (*inptr++ == ';' && *inptr && (params = _g_mime_param_list_parse (options, inptr, offset))) {
		g_mime_event_add (params->changed, (GMimeEventCallback) param_list_changed, content_type);
		g_object_unref (content_type->params);
		content_type->params = params;
	}
	
	return content_type;
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
 * g_mime_content_type_encode:
 * @content_type: a #GMimeContentType
 * @options: (nullable): a #GMimeFormatOptions or %NULL
 *
 * Encodes the Content-Disposition header.
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
