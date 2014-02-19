/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*  GMime
 *  Copyright (C) 2000-2014 Jeffrey Stedfast
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
	content_type->param_hash = g_hash_table_new (g_mime_strcase_hash, g_mime_strcase_equal);
	content_type->priv = g_mime_event_new ((GObject *) content_type);
	content_type->params = NULL;
	content_type->subtype = NULL;
	content_type->type = NULL;
}

static void
g_mime_content_type_finalize (GObject *object)
{
	GMimeContentType *content_type = (GMimeContentType *) object;
	
	g_hash_table_destroy (content_type->param_hash);
	g_mime_param_destroy (content_type->params);
	g_mime_event_destroy (content_type->priv);
	g_free (content_type->subtype);
	g_free (content_type->type);
	
	G_OBJECT_CLASS (parent_class)->finalize (object);
}


/**
 * g_mime_content_type_new:
 * @type: MIME type (or %NULL for "text")
 * @subtype: MIME subtype (or %NULL for "plain")
 *
 * Creates a Content-Type object with type @type and subtype @subtype.
 *
 * Returns: a new #GMimeContentType object.
 **/
GMimeContentType *
g_mime_content_type_new (const char *type, const char *subtype)
{
	GMimeContentType *mime_type;
	
	mime_type = g_object_newv (GMIME_TYPE_CONTENT_TYPE, 0, NULL);
	
	if (type && *type && subtype && *subtype) {
		mime_type->type = g_strdup (type);
		mime_type->subtype = g_strdup (subtype);
	} else {
		if (type && *type) {
			mime_type->type = g_strdup (type);
			if (!g_ascii_strcasecmp (type, "text")) {
				mime_type->subtype = g_strdup ("plain");
			} else if (!g_ascii_strcasecmp (type, "multipart")) {
				mime_type->subtype = g_strdup ("mixed");
			} else {
				g_free (mime_type->type);
				mime_type->type = g_strdup ("application");
				mime_type->subtype = g_strdup ("octet-stream");
			}
		} else {
			mime_type->type = g_strdup ("application");
			mime_type->subtype = g_strdup ("octet-stream");
		}
		
		w(g_warning ("Invalid or incomplete type: %s%s%s: defaulting to %s/%s",
			     type ? type : "", subtype ? "/" : "", subtype ? subtype : "",
			     mime_type->type, mime_type->subtype));
	}
	
	return mime_type;
}


/**
 * g_mime_content_type_new_from_string:
 * @str: input string containing a content-type (and params)
 *
 * Constructs a new Content-Type object based on the input string.
 *
 * Returns: a new #GMimeContentType object based on the input string.
 **/
GMimeContentType *
g_mime_content_type_new_from_string (const char *str)
{
	GMimeContentType *mime_type;
	const char *inptr = str;
	char *type, *subtype;
	
	g_return_val_if_fail (str != NULL, NULL);
	
	if (!g_mime_parse_content_type (&inptr, &type, &subtype))
		return g_mime_content_type_new ("application", "octet-stream");
	
	mime_type = g_object_newv (GMIME_TYPE_CONTENT_TYPE, 0, NULL);
	mime_type->subtype = subtype;
	mime_type->type = type;
	
	/* skip past any remaining junk that shouldn't be here... */
	decode_lwsp (&inptr);
	while (*inptr && *inptr != ';')
		inptr++;
	
	if (*inptr++ == ';' && *inptr) {
		GMimeParam *param;
		
		param = mime_type->params = g_mime_param_new_from_string (inptr);
		while (param != NULL) {
			g_hash_table_insert (mime_type->param_hash, param->name, param);
			param = param->next;
		}
	}
	
	return mime_type;
}


/**
 * g_mime_content_type_to_string:
 * @mime_type: a #GMimeContentType object
 *
 * Allocates a string buffer containing the type and subtype defined
 * by the @mime_type.
 *
 * Returns: an allocated string containing the type and subtype of the
 * content-type in the format: type/subtype.
 **/
char *
g_mime_content_type_to_string (GMimeContentType *mime_type)
{
	char *string;
	
	g_return_val_if_fail (GMIME_IS_CONTENT_TYPE (mime_type), NULL);
	
	/* type and subtype should never be NULL, but check anyway */
	string = g_strdup_printf ("%s/%s", mime_type->type ? mime_type->type : "text",
				  mime_type->subtype ? mime_type->subtype : "plain");
	
	return string;
}


/**
 * g_mime_content_type_is_type:
 * @mime_type: a #GMimeContentType object
 * @type: MIME type to compare against
 * @subtype: MIME subtype to compare against
 *
 * Compares the given type and subtype with that of the given mime
 * type object.
 *
 * Returns: TRUE if the MIME types match or FALSE otherwise. You may
 * use "*" in place of @type and/or @subtype as a wilcard.
 **/
gboolean
g_mime_content_type_is_type (GMimeContentType *mime_type, const char *type, const char *subtype)
{
	g_return_val_if_fail (GMIME_IS_CONTENT_TYPE (mime_type), FALSE);
	g_return_val_if_fail (mime_type->type != NULL, FALSE);
	g_return_val_if_fail (mime_type->subtype != NULL, FALSE);
	g_return_val_if_fail (type != NULL, FALSE);
	g_return_val_if_fail (subtype != NULL, FALSE);
	
	if (!strcmp (type, "*") || !g_ascii_strcasecmp (mime_type->type, type)) {
		if (!strcmp (subtype, "*")) {
			/* special case */
			return TRUE;
		}
		
		if (!g_ascii_strcasecmp (mime_type->subtype, subtype))
			return TRUE;
	}
	
	return FALSE;
}


/**
 * g_mime_content_type_set_media_type:
 * @mime_type: a #GMimeContentType object
 * @type: media type
 *
 * Sets the Content-Type's media type.
 **/
void
g_mime_content_type_set_media_type (GMimeContentType *mime_type, const char *type)
{
	char *buf;
	
	g_return_if_fail (GMIME_IS_CONTENT_TYPE (mime_type));
	g_return_if_fail (type != NULL);
	
	buf = g_strdup (type);
	g_free (mime_type->type);
	mime_type->type = buf;
	
	g_mime_event_emit (mime_type->priv, NULL);
}


/**
 * g_mime_content_type_get_media_type:
 * @mime_type: a #GMimeContentType object
 *
 * Gets the Content-Type's media type.
 *
 * Returns: the Content-Type's media type.
 **/
const char *
g_mime_content_type_get_media_type (GMimeContentType *mime_type)
{
	g_return_val_if_fail (GMIME_IS_CONTENT_TYPE (mime_type), NULL);
	
	return mime_type->type;
}


/**
 * g_mime_content_type_set_media_subtype:
 * @mime_type: a #GMimeContentType object
 * @subtype: media subtype
 *
 * Sets the Content-Type's media subtype.
 **/
void
g_mime_content_type_set_media_subtype (GMimeContentType *mime_type, const char *subtype)
{
	char *buf;
	
	g_return_if_fail (GMIME_IS_CONTENT_TYPE (mime_type));
	g_return_if_fail (subtype != NULL);
	
	buf = g_strdup (subtype);
	g_free (mime_type->subtype);
	mime_type->subtype = buf;
	
	g_mime_event_emit (mime_type->priv, NULL);
}


/**
 * g_mime_content_type_get_media_subtype:
 * @mime_type: a #GMimeContentType object
 *
 * Gets the Content-Type's media sub-type.
 *
 * Returns: the Content-Type's media sub-type.
 **/
const char *
g_mime_content_type_get_media_subtype (GMimeContentType *mime_type)
{
	g_return_val_if_fail (GMIME_IS_CONTENT_TYPE (mime_type), NULL);
	
	return mime_type->subtype;
}


/**
 * g_mime_content_type_set_params:
 * @mime_type: a #GMimeContentType object
 * @params: a list of #GMimeParam objects
 *
 * Sets the Content-Type's parameter list.
 **/
void
g_mime_content_type_set_params (GMimeContentType *mime_type, GMimeParam *params)
{
	g_return_if_fail (GMIME_IS_CONTENT_TYPE (mime_type));
	
	/* clear the current list/hash */
	g_hash_table_remove_all (mime_type->param_hash);
	g_mime_param_destroy (mime_type->params);
	mime_type->params = params;
	
	while (params != NULL) {
		g_hash_table_insert (mime_type->param_hash, params->name, params);
		params = params->next;
	}
	
	g_mime_event_emit (mime_type->priv, NULL);
}


/**
 * g_mime_content_type_get_params:
 * @mime_type: a #GMimeContentType object
 *
 * Gets the Content-Type's parameter list.
 *
 * Returns: the Content-Type's parameter list.
 **/
const GMimeParam *
g_mime_content_type_get_params (GMimeContentType *mime_type)
{
	g_return_val_if_fail (GMIME_IS_CONTENT_TYPE (mime_type), NULL);
	
	return mime_type->params;
}


/**
 * g_mime_content_type_set_parameter:
 * @mime_type: MIME Content-Type
 * @name: parameter name (aka attribute)
 * @value: parameter value
 *
 * Sets a parameter on the Content-Type.
 *
 * Note: The @name should be in US-ASCII while the @value should be in
 * UTF-8.
 **/
void
g_mime_content_type_set_parameter (GMimeContentType *mime_type, const char *name, const char *value)
{
	GMimeParam *param = NULL;
	
	g_return_if_fail (GMIME_IS_CONTENT_TYPE (mime_type));
	g_return_if_fail (name != NULL);
	g_return_if_fail (value != NULL);
	
	if ((param = g_hash_table_lookup (mime_type->param_hash, name))) {
		g_free (param->value);
		param->value = g_strdup (value);
	} else {
		param = g_mime_param_new (name, value);
		mime_type->params = g_mime_param_append_param (mime_type->params, param);
		g_hash_table_insert (mime_type->param_hash, param->name, param);
	}
	
	g_mime_event_emit (mime_type->priv, NULL);
}


/**
 * g_mime_content_type_get_parameter:
 * @mime_type: a #GMimeContentType object
 * @name: parameter name (aka attribute)
 *
 * Gets the parameter value specified by @name if it's available.
 *
 * Returns: the value of the requested parameter or %NULL if the
 * parameter is not set. If the parameter is set, the returned string
 * will be in UTF-8.
 **/
const char *
g_mime_content_type_get_parameter (GMimeContentType *mime_type, const char *name)
{
	GMimeParam *param;
	
	g_return_val_if_fail (GMIME_IS_CONTENT_TYPE (mime_type), NULL);
	g_return_val_if_fail (name != NULL, NULL);
	
	if (!(param = g_hash_table_lookup (mime_type->param_hash, name)))
		return NULL;
	
	return param->value;
}
