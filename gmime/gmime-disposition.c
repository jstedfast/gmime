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

#include <ctype.h>
#include <string.h>

#include "gmime-common.h"
#include "gmime-disposition.h"
#include "gmime-events.h"


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
static void g_mime_content_disposition_init (GMimeContentDisposition *disposition, GMimeContentDispositionClass *klass);
static void g_mime_content_disposition_finalize (GObject *object);


static GObjectClass *parent_class = NULL;


GType
g_mime_content_disposition_get_type (void)
{
	static GType type = 0;
	
	if (!type) {
		static const GTypeInfo info = {
			sizeof (GMimeContentDispositionClass),
			NULL, /* base_class_init */
			NULL, /* base_class_finalize */
			(GClassInitFunc) g_mime_content_disposition_class_init,
			NULL, /* class_finalize */
			NULL, /* class_data */
			sizeof (GMimeContentDisposition),
			0,    /* n_preallocs */
			(GInstanceInitFunc) g_mime_content_disposition_init,
		};
		
		type = g_type_register_static (G_TYPE_OBJECT, "GMimeContentDisposition", &info, 0);
	}
	
	return type;
}


static void
g_mime_content_disposition_class_init (GMimeContentDispositionClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	
	parent_class = g_type_class_ref (G_TYPE_OBJECT);
	
	object_class->finalize = g_mime_content_disposition_finalize;
}

static void
g_mime_content_disposition_init (GMimeContentDisposition *disposition, GMimeContentDispositionClass *klass)
{
	disposition->param_hash = g_hash_table_new (g_mime_strcase_hash, g_mime_strcase_equal);
	disposition->priv = g_mime_event_new ((GObject *) disposition);
	disposition->disposition = NULL;
	disposition->params = NULL;
}

static void
g_mime_content_disposition_finalize (GObject *object)
{
	GMimeContentDisposition *disposition = (GMimeContentDisposition *) object;
	
	g_hash_table_destroy (disposition->param_hash);
	g_mime_param_destroy (disposition->params);
	g_mime_event_destroy (disposition->priv);
	g_free (disposition->disposition);
	
	G_OBJECT_CLASS (parent_class)->finalize (object);
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
	
	disposition = g_object_newv (GMIME_TYPE_CONTENT_DISPOSITION, 0, NULL);
	disposition->disposition = g_strdup (GMIME_DISPOSITION_ATTACHMENT);
	
	return disposition;
}


/**
 * g_mime_content_disposition_new_from_string:
 * @str: Content-Disposition field value or %NULL
 *
 * Creates a new #GMimeContentDisposition object.
 *
 * Returns: a new #GMimeContentDisposition object.
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
	
	disposition = g_object_newv (GMIME_TYPE_CONTENT_DISPOSITION, 0, NULL);
	
	/* get content disposition part */
	
	/* find ; or \0 */
	while (*inptr && *inptr != ';')
		inptr++;
	
	value = g_strndup (str, (size_t) (inptr - str));
	disposition->disposition = g_strstrip (value);
	
	/* parse the parameters, if any */
	if (*inptr++ == ';' && *inptr) {
		param = disposition->params = g_mime_param_new_from_string (inptr);
		
		while (param) {
			g_hash_table_insert (disposition->param_hash, param->name, param);
			param = param->next;
		}
	}
	
	return disposition;
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
	
	buf = g_strdup (value);
	g_free (disposition->disposition);
	disposition->disposition = buf;
	
	g_mime_event_emit (disposition->priv, NULL);
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
 * g_mime_content_disposition_set_params:
 * @disposition: a #GMimeContentDisposition object
 * @params: a list of #GMimeParam objects
 *
 * Sets the Content-Disposition's parameter list.
 **/
void
g_mime_content_disposition_set_params (GMimeContentDisposition *disposition, GMimeParam *params)
{
	g_return_if_fail (GMIME_IS_CONTENT_DISPOSITION (disposition));
	
	/* destroy the current list/hash */
	g_hash_table_remove_all (disposition->param_hash);
	g_mime_param_destroy (disposition->params);
	disposition->params = params;
	
	while (params != NULL) {
		g_hash_table_insert (disposition->param_hash, params->name, params);
		params = params->next;
	}
	
	g_mime_event_emit (disposition->priv, NULL);
}


/**
 * g_mime_content_disposition_get_params:
 * @disposition: a #GMimeContentDisposition object
 *
 * Gets the Content-Disposition parameter list.
 *
 * Returns: the list of #GMimeParam's set on @disposition.
 **/
const GMimeParam *
g_mime_content_disposition_get_params (GMimeContentDisposition *disposition)
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
	GMimeParam *param = NULL;
	
	g_return_if_fail (GMIME_IS_CONTENT_DISPOSITION (disposition));
	g_return_if_fail (name != NULL);
	g_return_if_fail (value != NULL);
	
	if ((param = g_hash_table_lookup (disposition->param_hash, name))) {
		g_free (param->value);
		param->value = g_strdup (value);
	} else {
		param = g_mime_param_new (name, value);
		disposition->params = g_mime_param_append_param (disposition->params, param);
		g_hash_table_insert (disposition->param_hash, param->name, param);
	}
	
	g_mime_event_emit (disposition->priv, NULL);
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
	g_return_val_if_fail (name != NULL, NULL);
	
	if (!(param = g_hash_table_lookup (disposition->param_hash, name)))
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
 * Returns: a string containing the disposition header
 **/
char *
g_mime_content_disposition_to_string (GMimeContentDisposition *disposition, gboolean fold)
{
	GString *string;
	char *header, *buf;
	
	g_return_val_if_fail (GMIME_IS_CONTENT_DISPOSITION (disposition), NULL);
	
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
