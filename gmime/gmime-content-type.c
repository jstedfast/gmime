/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*  GMime
 *  Copyright (C) 2000-2007 Jeffrey Stedfast
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
#include "gmime-table-private.h"
#include "gmime-parse-utils.h"


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


/**
 * g_mime_content_type_new:
 * @type: MIME type (or NULL for "text")
 * @subtype: MIME subtype (or NULL for "plain")
 *
 * Creates a Content-Type object with type @type and subtype @subtype.
 *
 * Returns a new MIME Content-Type object.
 **/
GMimeContentType *
g_mime_content_type_new (const char *type, const char *subtype)
{
	GMimeContentType *mime_type;
	
	mime_type = g_new0 (GMimeContentType, 1);
	
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
 * @string: input string containing a content-type (and params)
 *
 * Constructs a new Content-Type object based on the input string.
 *
 * Returns a new MIME Content-Type based on the input string.
 **/
GMimeContentType *
g_mime_content_type_new_from_string (const char *string)
{
	GMimeContentType *mime_type;
	char *type = NULL, *subtype;
	const char *inptr;
	
	g_return_val_if_fail (string != NULL, NULL);
	
	inptr = string;
	
	/* get the type */
	type = (char *) inptr;
	while (*inptr && is_ttoken (*inptr))
		inptr++;
	
	type = g_strndup (type, (unsigned) (inptr - type));
	
	decode_lwsp (&inptr);
	
	/* get the subtype */
	if (*inptr == '/') {
		inptr++;
		
		decode_lwsp (&inptr);
		
		subtype = (char *) inptr;
		while (*inptr && is_ttoken (*inptr))
			inptr++;
		
		if (inptr > subtype)
			subtype = g_strndup (subtype, (size_t) (inptr - subtype));
		else
			subtype = NULL;
		
		decode_lwsp (&inptr);
	} else {
		subtype = NULL;
	}
	
	mime_type = g_mime_content_type_new (type, subtype);
	g_free (subtype);
	g_free (type);
	
	/* skip past any remaining junk that shouldn't be here... */
	while (*inptr && *inptr != ';')
		inptr++;
	
	if (*inptr++ == ';' && *inptr) {
		GMimeParam *p;
		
		mime_type->params = g_mime_param_new_from_string (inptr);
		if ((p = mime_type->params) != NULL) {
			mime_type->param_hash = g_hash_table_new (g_mime_strcase_hash, g_mime_strcase_equal);
			
			while (p != NULL) {
				g_hash_table_insert (mime_type->param_hash, p->name, p);
				p = p->next;
			}
		}
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
	
	g_mime_param_destroy (mime_type->params);
	
	g_free (mime_type);
}


/**
 * g_mime_content_type_to_string:
 * @mime_type: MIME Content-Type
 *
 * Allocates a string buffer containing the type and subtype defined
 * by the @mime_type.
 *
 * Returns an allocated string containing the type and subtype of the
 * content-type in the format: type/subtype.
 **/
char *
g_mime_content_type_to_string (const GMimeContentType *mime_type)
{
	char *string;
	
	g_return_val_if_fail (mime_type != NULL, NULL);
	
	/* type and subtype should never be NULL, but check anyway */
	string = g_strdup_printf ("%s/%s", mime_type->type ? mime_type->type : "text",
				  mime_type->subtype ? mime_type->subtype : "plain");
	
	return string;
}


/**
 * g_mime_content_type_is_type:
 * @mime_type: MIME Content-Type
 * @type: MIME type to compare against
 * @subtype: MIME subtype to compare against
 *
 * Compares the given type and subtype with that of the given mime
 * type object.
 *
 * Returns TRUE if the MIME types match or FALSE otherwise. You may
 * use "*" in place of @type and/or @subtype as a wilcard.
 **/
gboolean
g_mime_content_type_is_type (const GMimeContentType *mime_type, const char *type, const char *subtype)
{
	g_return_val_if_fail (mime_type != NULL, FALSE);
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
 * g_mime_content_type_set_parameter:
 * @mime_type: MIME Content-Type
 * @attribute: parameter name (aka attribute)
 * @value: parameter value
 *
 * Sets a parameter on the Content-Type.
 **/
void
g_mime_content_type_set_parameter (GMimeContentType *mime_type, const char *attribute, const char *value)
{
	GMimeParam *param = NULL;
	
	g_return_if_fail (mime_type != NULL);
	g_return_if_fail (attribute != NULL);
	g_return_if_fail (value != NULL);
	
	if (mime_type->params) {
		if ((param = g_hash_table_lookup (mime_type->param_hash, attribute))) {
			g_free (param->value);
			param->value = g_strdup (value);
			return;
		}
	} else if (!mime_type->param_hash) {
		/* hash table not initialized */
		mime_type->param_hash = g_hash_table_new (g_mime_strcase_hash, g_mime_strcase_equal);
	}
	
	if (param == NULL) {
		param = g_mime_param_new (attribute, value);
		mime_type->params = g_mime_param_append_param (mime_type->params, param);
		g_hash_table_insert (mime_type->param_hash, param->name, param);
	}
}


/**
 * g_mime_content_type_get_parameter:
 * @mime_type: MIME Content-Type
 * @attribute: parameter name (aka attribute)
 *
 * Gets the parameter value specified by @attribute if it's available.
 *
 * Returns a const pointer to the paramer value specified by
 * @attribute or %NULL on fail.
 **/
const char *
g_mime_content_type_get_parameter (const GMimeContentType *mime_type, const char *attribute)
{
	GMimeParam *param;
	
	g_return_val_if_fail (mime_type != NULL, NULL);
	g_return_val_if_fail (attribute != NULL, NULL);
	
	if (!mime_type->param_hash)
		return NULL;
	
	if (!(param = g_hash_table_lookup (mime_type->param_hash, attribute)))
		return NULL;
	
	return param->value;
}
