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

#include <ctype.h>
#include <string.h>

#include "gmime-common.h"
#include "gmime-disposition.h"
#include "gmime-internal.h"
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

static void param_list_changed (GMimeParamList *list, gpointer args, GMimeContentDisposition *disposition);


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
	disposition->changed = g_mime_event_new ((GObject *) disposition);
	disposition->params = g_mime_param_list_new ();
	disposition->disposition = NULL;
	
	g_mime_event_add (disposition->params->changed, (GMimeEventCallback) param_list_changed, disposition);
}

static void
g_mime_content_disposition_finalize (GObject *object)
{
	GMimeContentDisposition *disposition = (GMimeContentDisposition *) object;
	
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
 * Parses the input string into a #GMimeContentDisposition object.
 *
 * Returns: (transfer full): a new #GMimeContentDisposition object.
 **/
GMimeContentDisposition *
g_mime_content_disposition_parse (GMimeParserOptions *options, const char *str)
{
	return _g_mime_content_disposition_parse (options, str, -1);
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
 * g_mime_content_disposition_encode:
 * @disposition: a #GMimeContentDisposition object
 * @options: (nullable): a #GMimeFormatOptions or %NULL
 *
 * Encodes the Content-Disposition header.
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
