/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 *  Authors: Jeffrey Stedfast <fejj@ximian.com>
 *
 *  Copyright 2001 Ximain, Inc. (www.ximian.com)
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


#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "gmime-disposition.h"
#include "strlib.h"


static int
param_equal (gconstpointer v, gconstpointer v2)
{
	return strcasecmp ((const char *) v, (const char *) v2) == 0;
}

static guint
param_hash (gconstpointer key)
{
	const char *p = key;
	guint h = tolower (*p);
	
	if (h)
		for (p += 1; *p != '\0'; p++)
			h = (h << 5) - h + tolower (*p);
	
	return h;
}


/**
 * g_mime_disposition_new:
 * @disposition: disposition header (and params)
 *
 * Returns a new disposition object
 **/
GMimeDisposition *
g_mime_disposition_new (const char *disposition)
{
	GMimeDisposition *new;
	GMimeParam *param;
	const char *inptr;
	char *value;
	
	new = g_new (GMimeDisposition, 1);
	if (!disposition) {
		new->disposition = g_strdup (GMIME_DISPOSITION_ATTACHMENT);
		new->params = NULL;
		new->param_hash = NULL;
		
		return new;
	}
	
	/* get content disposition part */
	for (inptr = disposition; *inptr && *inptr != ';'; inptr++); /* find ; or \0 */
	value = g_strndup (disposition, (int) (inptr - disposition));
	g_strstrip (value);
	
	new->disposition = value;
	
	/* parse the parameters, if any */
	if (*inptr++ == ';' && *inptr) {
		param = new->params = g_mime_param_new_from_string (inptr);
		new->param_hash = g_hash_table_new (param_hash, param_equal);
		while (param) {
			g_hash_table_insert (new->param_hash, param->name, param);
			param = param->next;
		}
	} else {
		new->params = NULL;
		new->param_hash = NULL;
	}
	
	return new;
}


/**
 * g_mime_disposition_destroy:
 * @disposition: disposition object
 *
 * Destroy the disposition object.
 **/
void
g_mime_disposition_destroy (GMimeDisposition *disposition)
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
 * g_mime_disposition_set:
 * @disposition: disposition object
 * @value: disposition value
 *
 * Sets the disposition to @value which may be one of
 * GMIME_DISPOSITION_ATTACHMENT or GMIME_DISPOSITION_INLINE or, by your
 * choice, any other string which would indicate how the MIME part
 * should be displayed by the MUA.
 **/
void
g_mime_disposition_set (GMimeDisposition *disposition, const char *value)
{
	g_return_if_fail (disposition != NULL);
	g_return_if_fail (value != NULL);
	
	g_free (disposition->disposition);
	disposition->disposition = g_strdup (value);
}


/**
 * g_mime_disposition_get:
 * @disposition: disposition object
 *
 * Returns the disposition string which is probably one of
 * GMIME_DISPOSITION_ATTACHMENT or GMIME_DISPOSITION_INLINE.
 **/
const char *
g_mime_disposition_get (GMimeDisposition *disposition)
{
	g_return_val_if_fail (disposition != NULL, NULL);
	
	return disposition->disposition;
}


/**
 * g_mime_disposition_add_parameter:
 * @disposition: disposition object
 * @attribute: parameter name
 * @value: parameter value
 *
 * Adds a new parameter of name @name and value @value to the
 * disposition.
 **/
void
g_mime_disposition_add_parameter (GMimeDisposition *disposition, const char *attribute, const char *value)
{
	GMimeParam *param = NULL;
	
	g_return_if_fail (disposition != NULL);
	g_return_if_fail (attribute != NULL);
	g_return_if_fail (value != NULL);
	
	if (disposition->params) {
		param = g_hash_table_lookup (disposition->param_hash, attribute);
		if (param) {
			g_free (param->value);
			param->value = g_strdup (value);
		}
	} else {
		/* hash table may not be initialized */
		if (!disposition->param_hash)
			disposition->param_hash = g_hash_table_new (param_hash, param_equal);
	}
	
	if (param == NULL) {
		param = g_mime_param_new (attribute, value);
		disposition->params = g_mime_param_append_param (disposition->params, param);
		g_hash_table_insert (disposition->param_hash, param->name, param);
	}
}


/**
 * g_mime_disposition_get_parameter:
 * @disposition: disposition object
 * @attribute: parameter name
 *
 * Returns the value of the parameter of name @attribute.
 **/
const char *
g_mime_disposition_get_parameter (GMimeDisposition *disposition, const char *attribute)
{
	GMimeParam *param;
	
	g_return_val_if_fail (disposition != NULL, NULL);
	g_return_val_if_fail (attribute != NULL, NULL);
	
	if (!disposition->param_hash)
		return NULL;
	
	param = g_hash_table_lookup (disposition->param_hash, attribute);
	
	if (param)
		return param->value;
	else
		return NULL;
}


/**
 * g_mime_disposition_header:
 * @disposition: disposition object
 * @fold: fold header if needed
 *
 * Returns a string containing the disposition header
 **/
char *
g_mime_disposition_header (GMimeDisposition *disposition, gboolean fold)
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
