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

#include "gmime-param.h"
#include <config.h>
#include <string.h>
#include <ctype.h>


/**
 * g_mime_param_new: Create a new MIME Param object
 * @name: parameter name
 * @value: parameter value
 *
 * Creates an new paramter structure.
 **/
GMimeParam *
g_mime_param_new (const gchar *name, const gchar *value)
{
	GMimeParam *param;
	
	param = g_new (GMimeParam, 1);
	
	param->name = g_strdup (name);
	param->value = g_strdup (value);
	
	return param;
}


/**
 * g_mime_param_new_from_string: Create a new MIME Param object
 * @string: string to parse into a GMimeParam structure
 *
 * Parse the string into a GMimeParam structure.
 **/
GMimeParam *
g_mime_param_new_from_string (const gchar *string)
{
	GMimeParam *param;
	gchar *name, *value;
	gchar *ptr, *eptr;
	
	for (eptr = (gchar *) string; *eptr && *eptr != '='; eptr++);
	name = g_strndup (string, (gint) (eptr - string));
	g_strstrip (name);
	
	/* skip any whitespace */
	for (ptr = eptr + 1; *ptr && isspace (*ptr); ptr++);
	
	if (*ptr == '"') {
		/* value is in quotes */
		value = ptr + 1;
		for (eptr = value; *eptr && *eptr != '"'; eptr++);
		value = g_strndup (value, (gint) (eptr - value));
		g_strstrip (value);
	} else {
		/* value is not in quotes */
		value = g_strdup (ptr);
		g_strstrip (value);
	}
	
	param = g_mime_param_new (name, value);
	g_free (name);
	g_free (value);
	
	return param;
}

/**
 * g_mime_param_destroy: Destroy the MIME Param
 * @param: Mime param to destroy
 *
 * Releases all memory used by this mime param back to the Operating
 * System.
 **/
void
g_mime_param_destroy (GMimeParam *param)
{
	g_return_if_fail (param != NULL);
	
	g_free (param->name);
	g_free (param->value);
	g_free (param);
}


/**
 * g_mime_param_to_string: Write the MIME Param to a string
 * @param: MIME Param
 *
 * Returns an allocated string containing the MIME Param in the form:
 * name="value"
 **/
gchar *
g_mime_param_to_string (GMimeParam *param)
{
	g_return_val_if_fail (param != NULL, NULL);
	
	return g_strdup_printf ("%s=\"%s\"", param->name, param->value);
}
