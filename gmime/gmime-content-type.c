/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 *  Authors: Jeffrey Stedfast <fejj@helixcode.com>
 *
 *  Copyright 2000 Helix Code, Inc. (www.helixcode.com)
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Street #330, Boston, MA 02111-1307, USA.
 *
 */

#include "gmime-content-type.h"
#include <config.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>

#define d(x)


/**
 * g_mime_content_type_new: Create a new MIME Content-Type
 * @type: MIME type (or NULL for "text")
 * @subtype: MIME subtype (or NULL for "plain")
 *
 * Returns a new MIME Content-Type.
 **/
GMimeContentType *
g_mime_content_type_new (const gchar *type, const gchar *subtype)
{
	GMimeContentType *mime_type;
	
	mime_type = g_new0 (GMimeContentType, 1);
	
	if (type && *type && subtype && *subtype) {
		mime_type->type = g_strdup (type);
		mime_type->subtype = g_strdup (subtype);
	} else {
		if (type && *type) {
			mime_type->type = g_strdup (type);
			if (!g_strcasecmp (type, "text")) {
				mime_type->subtype = g_strdup ("plain");
			} else if (!g_strcasecmp (type, "multipart")) {
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
		
		g_warning ("Invalid or incomplete type: %s%s%s: defaulting to %s/%s",
			   type ? type : "", subtype ? "/" : "", subtype ? subtype : "",
			   mime_type->type, mime_type->subtype);
	}
	
	return mime_type;
}


/**
 * g_mime_content_type_new_from_string: Create a new MIME Content-Type
 * @string: input string containing a content-type (and params)
 *
 * Returns a new MIME Content-Type based on the input string.
 **/
GMimeContentType *
g_mime_content_type_new_from_string (const gchar *string)
{
	GMimeContentType *mime_type;
	guchar *type = NULL, *subtype = NULL;
	guchar *eptr;
	
	g_return_val_if_fail (string != NULL, NULL);
	
	/* get the type */
	type = (gchar *) string;
	for (eptr = type; *eptr && *eptr != '/' && *eptr != ';'; eptr++);
	type = g_strndup (type, (gint) (eptr - type));
	g_strstrip (type);
	
	/* get the subtype */
	if (*eptr != ';') {
		subtype = eptr + 1;
		for (eptr = subtype; *eptr && *eptr != ';'; eptr++);
		subtype = g_strndup (subtype, (gint) (eptr - subtype));
		g_strstrip (subtype);
	}
	
	mime_type = g_mime_content_type_new (type, subtype);
	g_free (type);
	g_free (subtype);
	
	while (*eptr == ';') {
		/* looks like we've got some parameters */
		guchar *name, *value;
		
		while (*eptr && (*eptr == ';' || isspace (*eptr)))
			eptr++;
		
		/* get the param name - skip past all whitespace */
		for (name = eptr; *name && isspace (*name); name++);
		
		for (eptr = name; *eptr && *eptr != '='; eptr++);
		name = g_strndup (name, (gint) (eptr - name));
		
		g_strstrip (name);
		
		/* change the param name to lowercase */
		g_strdown (name);
		
		/* skip any whitespace */
		for (value = eptr + 1; *value && isspace (*value); value++);
		
		if (*value == '"') {
			/* value is in quotes */
			value++;
			for (eptr = value; *eptr && *eptr != '"'; eptr++);
			value = g_strndup (value, (gint) (eptr - value));
			g_strstrip (value);
			
			for ( ; *eptr && *eptr != ';'; eptr++);
		} else {			
			/* value is not in quotes */
			for (eptr = value; *eptr && *eptr != ';'; eptr++);
			value = g_strndup (value, (gint) (eptr - value));
			g_strstrip (value);
		}
		
		g_mime_content_type_add_parameter (mime_type, name, value);
		g_free (name);
		g_free (value);
	}
	
	return mime_type;
}


/**
 * g_mime_content_type_destroy: Destroy a MIME Content-Type object
 * @mime_type: MIME Content-Type object to destroy
 *
 * Destroys the given MIME Content-Type object.
 **/
void
g_mime_content_type_destroy (GMimeContentType *mime_type)
{
	g_return_if_fail (mime_type != NULL);
	
	g_free (mime_type->type);
	g_free (mime_type->subtype);
	
	if (mime_type->param_hash)
		g_hash_table_destroy (mime_type->param_hash);
	
	if (mime_type->params) {
		GList *parameter;
		
		parameter = mime_type->params;
		while (parameter) {
			GMimeParam *param = parameter->data;
			
			g_free (param->name);
			g_free (param->value);
			g_free (param);
			
			parameter = parameter->next;
		}
		
		g_list_free (mime_type->params);
	}
	
	g_free (mime_type);
}


/**
 * g_mime_content_type_to_string: Write the Content-Type to a string
 * @mime_type: MIME Content-Type
 *
 * Returns an allocated string containing the type and subtype of the
 * content-type in the format: type/subtype.
 **/
gchar *
g_mime_content_type_to_string (const GMimeContentType *mime_type)
{
	gchar *string;
	
	g_return_val_if_fail (mime_type != NULL, NULL);
	
	/* type and subtype should never be NULL, but check anyway */
	string = g_strdup_printf ("%s/%s", mime_type->type ? mime_type->type : "text",
				  mime_type->subtype ? mime_type->subtype : "plain");
	
	return string;
}


/**
 * g_mime_content_type_is_type: Compare MIME types
 * @mime_type: MIME Content-Type
 * @type: MIME type to compare against
 * @subtype: MIME subtype to compare against
 *
 * Returns TRUE if the MIME types match or FALSE otherwise. You may
 * use "*" in place of #type and/or #subtype as a wilcard.
 **/
gboolean
g_mime_content_type_is_type (const GMimeContentType *mime_type, const char *type, const char *subtype)
{
	g_return_val_if_fail (mime_type != NULL, FALSE);
	g_return_val_if_fail (mime_type->type != NULL, FALSE);
	g_return_val_if_fail (mime_type->subtype != NULL, FALSE);
	g_return_val_if_fail (type != NULL, FALSE);
	g_return_val_if_fail (subtype != NULL, FALSE);
	
	if (!g_strcasecmp (mime_type->type, type)) {
		if (!strcmp (subtype, "*")) {
			/* special case */
			return TRUE;
		} else {
			if (!g_strcasecmp (mime_type->subtype, subtype))
				return TRUE;
			else
				return FALSE;
		}
	}
	
	return FALSE;
}


/**
 * g_mime_content_type_add_parameter: Add a parameter to the MIME Content-Type
 * @mime_type: MIME Content-Type
 * @attribute: parameter name (aka attribute)
 * @value: parameter value
 *
 * Adds a parameter to the Content-Type.
 **/
void
g_mime_content_type_add_parameter (GMimeContentType *mime_type, const gchar *attribute, const gchar *value)
{
	GMimeParam *param;
	
	g_return_if_fail (mime_type != NULL);
	
	if (mime_type->params) {
		param = g_hash_table_lookup (mime_type->param_hash, attribute);
		if (param) {
			/* destroy previously defined param */
			g_hash_table_remove (mime_type->param_hash, attribute);
			mime_type->params = g_list_remove (mime_type->params, param);
			g_mime_param_destroy (param);
		}
	} else {
		/* hash table may not be initialized */
		if (!mime_type->param_hash)
			mime_type->param_hash = g_hash_table_new (g_str_hash, g_str_equal);
	}
	
	param = g_mime_param_new (attribute, value);
	mime_type->params = g_list_append (mime_type->params, param);
	g_hash_table_insert (mime_type->param_hash, param->name, param);
}


/**
 * g_mime_content_type_get_parameter: Get a parameter of the MIME Content-Type
 * @mime_type: MIME Content-Type
 * @attribute: parameter name (aka attribute)
 *
 * Returns a const pointer to the paramer value specified by #attribute.
 **/
const gchar *
g_mime_content_type_get_parameter (const GMimeContentType *mime_type, const gchar *attribute)
{
	GMimeParam *param;
	
	g_return_val_if_fail (mime_type != NULL, NULL);
	
	if (!mime_type->param_hash)
		return NULL;
	
	param = g_hash_table_lookup (mime_type->param_hash, attribute);
	
	if (param)
		return param->value;
	else
		return NULL;
}
