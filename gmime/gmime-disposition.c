/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*  GMime
 *  Copyright (C) 2000-2008 Jeffrey Stedfast
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


/**
 * SECTION: gmime-disposition
 * @title: GMimeContentDisposition
 * @short_description: Content-Disposition fields
 * @see_also:
 *
 * A #GMimeContentDisposition represents the pre-parsed contents of a
 * Content-Disposition header field.
 **/


struct _GMimeObject;
void _g_mime_object_content_disposition_changed (struct _GMimeObject *object);


/**
 * g_mime_content_disposition_new:
 *
 * Creates a new #GMimeContentDisposition object.
 *
 * Returns a new #GMimeContentDisposition object.
 **/
GMimeContentDisposition *
g_mime_content_disposition_new (void)
{
	GMimeContentDisposition *disposition;
	
	disposition = g_new (GMimeContentDisposition, 1);
	disposition->disposition = g_strdup (GMIME_DISPOSITION_ATTACHMENT);
	disposition->parent_object = NULL;
	disposition->param_hash = NULL;
	disposition->params = NULL;
	
	return disposition;
}


/**
 * g_mime_content_disposition_new_from_string:
 * @str: Content-Disposition field value or %NULL
 *
 * Creates a new #GMimeContentDisposition object.
 *
 * Returns a new #GMimeContentDisposition object.
 **/
GMimeContentDisposition *
g_mime_content_disposition_new_from_string (const char *str)
{
	GMimeContentDisposition *disposition;
	const char *inptr = str;
	GMimeParam *param;
	char *value;
	
	if (str == NULL)
		return g_mime_content_disposition_new ();
	
	disposition = g_new (GMimeContentDisposition, 1);
	disposition->parent_object = NULL;
	
	/* get content disposition part */
	
	/* find ; or \0 */
	while (*inptr && *inptr != ';')
		inptr++;
	
	value = g_strndup (str, (size_t) (inptr - str));
	disposition->disposition = g_strstrip (value);
	
	/* parse the parameters, if any */
	if (*inptr++ == ';' && *inptr) {
		param = disposition->params = g_mime_param_new_from_string (inptr);
		disposition->param_hash = g_hash_table_new (g_mime_strcase_hash, g_mime_strcase_equal);
		while (param) {
			g_hash_table_insert (disposition->param_hash, param->name, param);
			param = param->next;
		}
	} else {
		disposition->param_hash = NULL;
		disposition->params = NULL;
	}
	
	return disposition;
}


/**
 * g_mime_content_disposition_destroy:
 * @disposition: a #GMimeContentDisposition object
 *
 * Destroy the #GMimeContentDisposition object.
 **/
void
g_mime_content_disposition_destroy (GMimeContentDisposition *disposition)
{
	if (disposition) {
		g_free (disposition->disposition);
		if (disposition->params)
			g_mime_param_destroy (disposition->params);
		if (disposition->param_hash)
			g_hash_table_destroy (disposition->param_hash);
		g_free (disposition);
	}
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
	g_return_if_fail (disposition != NULL);
	g_return_if_fail (value != NULL);
	
	g_free (disposition->disposition);
	disposition->disposition = g_strdup (value);
	
	if (disposition->parent_object)
		_g_mime_object_content_disposition_changed (disposition->parent_object);
}


/**
 * g_mime_content_disposition_get_disposition:
 * @disposition: a #GMimeContentDisposition object
 *
 * Gets the disposition or %NULL on fail.
 *
 * Returns the disposition string which is probably one of
 * #GMIME_DISPOSITION_ATTACHMENT or #GMIME_DISPOSITION_INLINE.
 **/
const char *
g_mime_content_disposition_get_disposition (const GMimeContentDisposition *disposition)
{
	g_return_val_if_fail (disposition != NULL, NULL);
	
	return disposition->disposition;
}


/**
 * g_mime_content_disposition_get_params:
 * @disposition: a #GMimeContentDisposition object
 *
 * Gets the Content-Disposition parameter list.
 **/
const GMimeParam *
g_mime_content_disposition_get_params (const GMimeContentDisposition *disposition)
{
	g_return_val_if_fail (disposition != NULL, NULL);
	
	return disposition->params;
}


/**
 * g_mime_content_disposition_set_parameter:
 * @disposition: a #GMimeContentDisposition object
 * @attribute: parameter name
 * @value: parameter value
 *
 * Sets a parameter on the Content-Disposition.
 **/
void
g_mime_content_disposition_set_parameter (GMimeContentDisposition *disposition, const char *attribute, const char *value)
{
	GMimeParam *param = NULL;
	
	g_return_if_fail (disposition != NULL);
	g_return_if_fail (attribute != NULL);
	g_return_if_fail (value != NULL);
	
	if (disposition->params) {
		if ((param = g_hash_table_lookup (disposition->param_hash, attribute))) {
			g_free (param->value);
			param->value = g_strdup (value);
			goto changed;
		}
	} else if (!disposition->param_hash) {
		/* hash table may not be initialized */
		disposition->param_hash = g_hash_table_new (g_mime_strcase_hash, g_mime_strcase_equal);
	}
	
	param = g_mime_param_new (attribute, value);
	disposition->params = g_mime_param_append_param (disposition->params, param);
	g_hash_table_insert (disposition->param_hash, param->name, param);
	
 changed:
	
	if (disposition->parent_object)
		_g_mime_object_content_disposition_changed (disposition->parent_object);
}


/**
 * g_mime_content_disposition_get_parameter:
 * @disposition: a #GMimeContentDisposition object
 * @attribute: parameter name
 *
 * Gets the value of the parameter @attribute, or %NULL on fail.
 *
 * Returns the value of the parameter of name @attribute.
 **/
const char *
g_mime_content_disposition_get_parameter (const GMimeContentDisposition *disposition, const char *attribute)
{
	GMimeParam *param;
	
	g_return_val_if_fail (disposition != NULL, NULL);
	g_return_val_if_fail (attribute != NULL, NULL);
	
	if (!disposition->param_hash)
		return NULL;
	
	if (!(param = g_hash_table_lookup (disposition->param_hash, attribute)))
		return NULL;
	
	return param->value;
}


/**
 * g_mime_content_disposition_to_string:
 * @disposition: a #GMimeContentDisposition object
 * @fold: fold header if needed
 *
 * Allocates a string buffer containing the Content-Disposition header
 * represented by the disposition object @disposition.
 *
 * Returns a string containing the disposition header
 **/
char *
g_mime_content_disposition_to_string (const GMimeContentDisposition *disposition, gboolean fold)
{
	GString *string;
	char *header, *buf;
	
	g_return_val_if_fail (disposition != NULL, NULL);
	
	/* we need to have this so wrapping is correct */
	string = g_string_new ("Content-Disposition: ");
	
	g_string_append (string, disposition->disposition);
	g_mime_param_write_to_string (disposition->params, fold, string);
	
	header = string->str;
	g_string_free (string, FALSE);
	
	buf = header + strlen ("Content-Disposition: ");
	memmove (header, buf, strlen (buf) + 1);
	
	return header;
}
