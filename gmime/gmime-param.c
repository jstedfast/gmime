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

#include <string.h>
#include <limits.h>
#include <ctype.h>
#include <errno.h>

#include "gmime-param.h"
#include "gmime-common.h"
#include "gmime-error.h"
#include "gmime-events.h"
#include "gmime-internal.h"
#include "gmime-table-private.h"
#include "gmime-parse-utils.h"
#include "gmime-charset.h"
#include "gmime-utils.h"
#include "gmime-iconv.h"
#include "gmime-codec-private.h"
#include "gmime-rfc2047-private.h"


#ifdef ENABLE_WARNINGS
#define w(x) x
#else
#define w(x)
#endif /* ENABLE_WARNINGS */

#define d(x)

/**
 * SECTION: gmime-param
 * @title: GMimeParamList
 * @short_description: Content-Type and Content-Disposition parameters
 * @see_also: #GMimeContentType
 *
 * A #GMimeParam is a parameter name/value pair as found on MIME
 * header fields such as Content-Type and Content-Disposition.
 **/


static void g_mime_param_class_init (GMimeParamClass *klass);
static void g_mime_param_init (GMimeParam *cert, GMimeParamClass *klass);
static void g_mime_param_finalize (GObject *object);
static void g_string_append_len_quoted (GString *str, const char *text, size_t len);

static GObjectClass *parent_class = NULL;


GType
g_mime_param_get_type (void)
{
	static GType type = 0;
	
	if (!type) {
		static const GTypeInfo info = {
			sizeof (GMimeParamClass),
			NULL, /* base_class_init */
			NULL, /* base_class_finalize */
			(GClassInitFunc) g_mime_param_class_init,
			NULL, /* class_finalize */
			NULL, /* class_data */
			sizeof (GMimeParam),
			0,    /* n_preallocs */
			(GInstanceInitFunc) g_mime_param_init,
		};
		
		type = g_type_register_static (G_TYPE_OBJECT, "GMimeParam", &info, 0);
	}
	
	return type;
}

static void
g_mime_param_class_init (GMimeParamClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	
	parent_class = g_type_class_ref (G_TYPE_OBJECT);
	
	object_class->finalize = g_mime_param_finalize;
}

static void
g_mime_param_init (GMimeParam *param, GMimeParamClass *klass)
{
	param->method = GMIME_PARAM_ENCODING_METHOD_DEFAULT;
	param->always_quote = FALSE;
	param->changed = g_mime_event_new (param);
	param->charset = NULL;
	param->value = NULL;
	param->name = NULL;
	param->lang = NULL;
}

static void
g_mime_param_finalize (GObject *object)
{
	GMimeParam *param = (GMimeParam *) object;
	
	g_mime_event_free (param->changed);
	g_free (param->charset);
	g_free (param->value);
	g_free (param->name);
	g_free (param->lang);
	
	G_OBJECT_CLASS (parent_class)->finalize (object);
}


static GMimeParam *
param_new (void)
{
	return g_object_new (GMIME_TYPE_PARAM, NULL);
}

static gboolean
param_name_is_valid (const char *name)
{
	const unsigned char *p = (const unsigned char *) name;

	if (!p || !*p)
		return FALSE;

	while (*p) {
		if (*p > 127 || !is_attrchar (*p))
			return FALSE;
		p++;
	}

	return TRUE;
}

/**
 * g_mime_param_new:
 * @name: a nonempty ASCII parameter name containing only attribute characters
 * @value: a UTF-8 parameter value, possibly empty
 *
 * Creates a parameter with the default encoding method and charset.
 * Invalid names and invalid UTF-8 values are programming errors.
 *
 * Returns: (transfer full): a new #GMimeParam.
 **/
GMimeParam *
g_mime_param_new (const char *name, const char *value)
{
	GMimeParam *param;

	g_return_val_if_fail (param_name_is_valid (name), NULL);
	g_return_val_if_fail (value != NULL && g_utf8_validate (value, -1, NULL), NULL);

	param = param_new ();
	param->name = g_strdup (name);
	param->value = g_strdup (value);

	return param;
}

/**
 * g_mime_param_clone:
 * @param: a #GMimeParam
 *
 * Copies the name, value, preferred charset, language, encoding method,
 * and quoting preference. Changes to the clone do not affect @param or
 * its owning lists.
 *
 * Returns: (transfer full): an independent copy of @param.
 **/
GMimeParam *
g_mime_param_clone (GMimeParam *param)
{
	GMimeParam *clone;

	g_return_val_if_fail (GMIME_IS_PARAM (param), NULL);

	clone = param_new ();
	clone->name = g_strdup (param->name);
	clone->value = g_strdup (param->value);
	clone->charset = g_strdup (param->charset);
	clone->lang = g_strdup (param->lang);
	clone->method = param->method;
	clone->always_quote = param->always_quote;

	return clone;
}

static void
param_append (GString *str, GMimeParam *param)
{
	g_string_append (str, param->name);
	g_string_append_c (str, '=');
	g_string_append_len_quoted (str, param->value, strlen (param->value));
}

/**
 * g_mime_param_to_string:
 * @param: a #GMimeParam
 *
 * Formats the parameter as name="value", always quoting the value and
 * escaping quotes and backslashes. No transport encoding or folding is
 * performed.
 *
 * Returns: (transfer full): the display string.
 **/
char *
g_mime_param_to_string (GMimeParam *param)
{
	GString *str;

	g_return_val_if_fail (GMIME_IS_PARAM (param), NULL);

	str = g_string_new ("");
	param_append (str, param);

	return g_string_free (str, FALSE);
}


/**
 * g_mime_param_get_name:
 * @param: a #GMimeParam
 *
 * Gets the name of the parameter.
 *
 * Returns: the name of the parameter.
 **/
const char *
g_mime_param_get_name (GMimeParam *param)
{
	g_return_val_if_fail (GMIME_IS_PARAM (param), NULL);
	
	return param->name;
}


/**
 * g_mime_param_get_value:
 * @param: a #GMimeParam
 *
 * Gets the value of the parameter.
 *
 * Returns: the value of the parameter.
 **/
const char *
g_mime_param_get_value (GMimeParam *param)
{
	g_return_val_if_fail (GMIME_IS_PARAM (param), NULL);
	
	return param->value;
}


/**
 * g_mime_param_set_value:
 * @param: a #GMimeParam
 * @value: the new parameter value
 *
 * Sets the parameter value to @value.
 **/
void
g_mime_param_set_value (GMimeParam *param, const char *value)
{
	char *copy;

	g_return_if_fail (GMIME_IS_PARAM (param));
	g_return_if_fail (value != NULL && g_utf8_validate (value, -1, NULL));
	
	if (!g_strcmp0 (param->value, value))
		return;

	copy = g_strdup (value);
	g_free (param->value);
	param->value = copy;
	
	g_mime_event_emit (param->changed, NULL);
}


/**
 * g_mime_param_get_charset:
 * @param: a #GMimeParam
 *
 * Gets the configured or parsed parameter charset. RFC 2231 and RFC 2047
 * serialization choose ASCII or Latin-1 for values representable in those
 * charsets, otherwise use this charset or UTF-8 if it cannot encode the value.
 *
 * Returns: (nullable): the configured or parsed charset, or %NULL for the default.
 **/
const char *
g_mime_param_get_charset (GMimeParam *param)
{
	g_return_val_if_fail (GMIME_IS_PARAM (param), NULL);
	
	return param->charset;
}


/**
 * g_mime_param_set_charset:
 * @param: a #GMimeParam
 * @charset: the charset or %NULL to use the default
 *
 * Sets the preferred parameter charset. RFC 2231 and RFC 2047 serialization
 * use this charset for values outside Latin-1 if it preserves the complete value,
 * otherwise falls back to UTF-8. %NULL selects UTF-8 for those values.
 **/
void
g_mime_param_set_charset (GMimeParam *param, const char *charset)
{
	char *copy;

	g_return_if_fail (GMIME_IS_PARAM (param));
	
	if (!g_strcmp0 (param->charset, charset))
		return;

	copy = g_strdup (charset);
	g_free (param->charset);
	param->charset = copy;
	
	g_mime_event_emit (param->changed, NULL);
}


/**
 * g_mime_param_get_lang:
 * @param: a #GMimeParam
 *
 * Gets the language specifier used for encoding the parameter.
 *
 * Returns: the language specifier used for encoding the parameter.
 **/
const char *
g_mime_param_get_lang (GMimeParam *param)
{
	g_return_val_if_fail (GMIME_IS_PARAM (param), NULL);
	
	return param->lang;
}


/**
 * g_mime_param_set_lang:
 * @param: a #GMimeParam
 * @lang: the language specifier
 *
 * Sets the parameter language specifier used for encoding the value.
 **/
void
g_mime_param_set_lang (GMimeParam *param, const char *lang)
{
	char *copy;

	g_return_if_fail (GMIME_IS_PARAM (param));
	
	if (!g_strcmp0 (param->lang, lang))
		return;

	copy = g_strdup (lang);
	g_free (param->lang);
	param->lang = copy;
	
	g_mime_event_emit (param->changed, NULL);
}


/**
 * g_mime_param_get_encoding_method:
 * @param: a #GMimeParam
 *
 * Gets the encoding method used for encoding the parameter.
 *
 * Returns: the encoding method used for encoding the parameter.
 **/
GMimeParamEncodingMethod
g_mime_param_get_encoding_method (GMimeParam *param)
{
	g_return_val_if_fail (GMIME_IS_PARAM (param), GMIME_PARAM_ENCODING_METHOD_DEFAULT);
	
	return param->method;
}


/**
 * g_mime_param_set_encoding_method:
 * @param: a #GMimeParam
 * @method: a #GMimeParamEncodingMethod
 *
 * Sets the encoding method used for encoding the value.
 **/
void
g_mime_param_set_encoding_method (GMimeParam *param, GMimeParamEncodingMethod method)
{
	g_return_if_fail (GMIME_IS_PARAM (param));
	g_return_if_fail (method == GMIME_PARAM_ENCODING_METHOD_DEFAULT ||
			  method == GMIME_PARAM_ENCODING_METHOD_RFC2231 || method == GMIME_PARAM_ENCODING_METHOD_RFC2047);
	
	if (param->method == method)
		return;

	param->method = method;
	
	g_mime_event_emit (param->changed, NULL);
}


/**
 * g_mime_param_get_always_quote:
 * @param: a #GMimeParam
 *
 * Gets whether an otherwise unquoted parameter value should be quoted.
 * The default is %FALSE.
 *
 * Returns: %TRUE if quoting is requested for this parameter.
 **/
gboolean
g_mime_param_get_always_quote (GMimeParam *param)
{
	g_return_val_if_fail (GMIME_IS_PARAM (param), FALSE);

	return param->always_quote;
}


/**
 * g_mime_param_set_always_quote:
 * @param: a #GMimeParam
 * @always_quote: whether to quote otherwise unquoted parameter values
 *
 * Requests quoting for this parameter in addition to the format option.
 * %FALSE does not override the format option or suppress required quoting.
 * RFC 2231 encoded values are not quoted.
 **/
void
g_mime_param_set_always_quote (GMimeParam *param, gboolean always_quote)
{
	g_return_if_fail (GMIME_IS_PARAM (param));

	always_quote = !!always_quote;
	if (param->always_quote == always_quote)
		return;

	param->always_quote = always_quote;
	g_mime_event_emit (param->changed, NULL);
}


static void
param_changed (GMimeParam *param, gpointer args, GMimeParamList *list)
{
	g_mime_event_emit (list->changed, NULL);
}


static void g_mime_param_list_class_init (GMimeParamListClass *klass);
static void g_mime_param_list_init (GMimeParamList *list, GMimeParamListClass *klass);
static void g_mime_param_list_finalize (GObject *object);


static GObjectClass *list_parent_class = NULL;


GType
g_mime_param_list_get_type (void)
{
	static GType type = 0;
	
	if (!type) {
		static const GTypeInfo info = {
			sizeof (GMimeParamListClass),
			NULL, /* base_class_init */
			NULL, /* base_class_finalize */
			(GClassInitFunc) g_mime_param_list_class_init,
			NULL, /* class_finalize */
			NULL, /* class_data */
			sizeof (GMimeParamList),
			0,    /* n_preallocs */
			(GInstanceInitFunc) g_mime_param_list_init,
		};
		
		type = g_type_register_static (G_TYPE_OBJECT, "GMimeParamList", &info, 0);
	}
	
	return type;
}


static void
g_mime_param_list_class_init (GMimeParamListClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	
	list_parent_class = g_type_class_ref (G_TYPE_OBJECT);
	
	object_class->finalize = g_mime_param_list_finalize;
}

static void
g_mime_param_list_init (GMimeParamList *list, GMimeParamListClass *klass)
{
	list->changed = g_mime_event_new (list);
	list->array = g_ptr_array_new ();
}

static void
g_mime_param_list_finalize (GObject *object)
{
	GMimeParamList *list = (GMimeParamList *) object;
	GMimeParam *param;
	guint i;
	
	for (i = 0; i < list->array->len; i++) {
		param = (GMimeParam *) list->array->pdata[i];
		g_mime_event_remove (param->changed, (GMimeEventCallback) param_changed, list);
		g_object_unref (param);
	}
	
	g_ptr_array_free (list->array, TRUE);
	g_mime_event_free (list->changed);
	
	G_OBJECT_CLASS (list_parent_class)->finalize (object);
}


/**
 * g_mime_param_list_new:
 *
 * Creates a new Content-Type or Content-Disposition parameter list.
 *
 * Returns: a new #GMimeParamList.
 **/
GMimeParamList *
g_mime_param_list_new (void)
{
	return g_object_new (GMIME_TYPE_PARAM_LIST, NULL);
}


/**
 * g_mime_param_list_length:
 * @list: a #GMimeParamList
 *
 * Gets the length of the list.
 *
 * Returns: the number of #GMimeParam items in the list.
 **/
int
g_mime_param_list_length (GMimeParamList *list)
{
	g_return_val_if_fail (GMIME_IS_PARAM_LIST (list), -1);
	
	return list->array->len;
}


/**
 * g_mime_param_list_clear:
 * @list: a #GMimeParamList
 *
 * Clears the list of parameters.
 **/
void
g_mime_param_list_clear (GMimeParamList *list)
{
	GMimeParam *param;
	guint i;
	
	g_return_if_fail (GMIME_IS_PARAM_LIST (list));
	
	for (i = 0; i < list->array->len; i++) {
		param = (GMimeParam *) list->array->pdata[i];
		g_mime_event_remove (param->changed, (GMimeEventCallback) param_changed, list);
		g_object_unref (param);
	}
	
	g_ptr_array_set_size (list->array, 0);
	
	g_mime_event_emit (list->changed, NULL);
}


static void
param_list_add (GMimeParamList *list, GMimeParam *param)
{
	g_mime_event_add (param->changed, (GMimeEventCallback) param_changed, list);
	g_ptr_array_add (list->array, param);
}

GMimeParamList *
_g_mime_param_list_clone (GMimeParamList *list)
{
	GMimeParamList *clone = g_mime_param_list_new ();
	guint i;

	for (i = 0; i < list->array->len; i++)
		param_list_add (clone, g_mime_param_clone (list->array->pdata[i]));

	return clone;
}

/**
 * g_mime_param_list_index_of:
 * @list: a #GMimeParamList
 * @name: the parameter name
 *
 * Returns: the first case-insensitive name match, or -1 if absent.
 **/
int
g_mime_param_list_index_of (GMimeParamList *list, const char *name)
{
	guint i;

	g_return_val_if_fail (GMIME_IS_PARAM_LIST (list), -1);
	g_return_val_if_fail (name != NULL, -1);

	for (i = 0; i < list->array->len; i++) {
		GMimeParam *param = list->array->pdata[i];

		if (!g_ascii_strcasecmp (param->name, name))
			return i;
	}

	return -1;
}

/**
 * g_mime_param_list_index_of_parameter:
 * @list: a #GMimeParamList
 * @param: a #GMimeParam
 *
 * Returns: the first index of this exact object, or -1 if absent.
 **/
int
g_mime_param_list_index_of_parameter (GMimeParamList *list, GMimeParam *param)
{
	guint i;

	g_return_val_if_fail (GMIME_IS_PARAM_LIST (list), -1);
	g_return_val_if_fail (GMIME_IS_PARAM (param), -1);

	for (i = 0; i < list->array->len; i++) {
		if (list->array->pdata[i] == param)
			return i;
	}

	return -1;
}

/**
 * g_mime_param_list_contains:
 * @list: a #GMimeParamList
 * @name: the parameter name
 *
 * Returns: whether a parameter with this case-insensitive name exists.
 **/
gboolean
g_mime_param_list_contains (GMimeParamList *list, const char *name)
{
	return g_mime_param_list_index_of (list, name) != -1;
}

/**
 * g_mime_param_list_contains_parameter:
 * @list: a #GMimeParamList
 * @param: a #GMimeParam
 *
 * Returns: whether this exact object is in the list.
 **/
gboolean
g_mime_param_list_contains_parameter (GMimeParamList *list, GMimeParam *param)
{
	return g_mime_param_list_index_of_parameter (list, param) != -1;
}

/**
 * g_mime_param_list_add:
 * @list: a #GMimeParamList
 * @param: (transfer none): a #GMimeParam
 *
 * Appends @param, retaining a reference and observing its changes. A parameter
 * with the same case-insensitive name must not already exist.
 *
 * Returns: the added index, or -1 on a programming error.
 **/
int
g_mime_param_list_add (GMimeParamList *list, GMimeParam *param)
{
	int index;

	g_return_val_if_fail (GMIME_IS_PARAM_LIST (list), -1);
	g_return_val_if_fail (GMIME_IS_PARAM (param), -1);
	g_return_val_if_fail (list->array->len < G_MAXINT, -1);
	g_return_val_if_fail (g_mime_param_list_index_of (list, param->name) == -1, -1);

	index = list->array->len;
	param_list_add (list, g_object_ref (param));
	g_mime_event_emit (list->changed, NULL);

	return index;
}

/**
 * g_mime_param_list_insert:
 * @list: a #GMimeParamList
 * @index: an index from zero through the list length, inclusive
 * @param: (transfer none): a #GMimeParam
 *
 * Inserts @param, retaining a reference and observing its changes. A parameter
 * with the same case-insensitive name must not already exist.
 **/
void
g_mime_param_list_insert (GMimeParamList *list, int index, GMimeParam *param)
{
	g_return_if_fail (GMIME_IS_PARAM_LIST (list));
	g_return_if_fail (GMIME_IS_PARAM (param));
	g_return_if_fail (index >= 0 && (guint) index <= list->array->len);
	g_return_if_fail (list->array->len < G_MAXINT);
	g_return_if_fail (g_mime_param_list_index_of (list, param->name) == -1);

	g_object_ref (param);
	g_mime_event_add (param->changed, (GMimeEventCallback) param_changed, list);
	g_ptr_array_insert (list->array, index, param);
	g_mime_event_emit (list->changed, NULL);
}

/**
 * g_mime_param_list_set_parameter_at:
 * @list: a #GMimeParamList
 * @index: an existing index
 * @param: (transfer none): the replacement #GMimeParam
 *
 * Replaces an entry, retaining a reference to @param. A different name must
 * not duplicate another entry's case-insensitive name. Assigning the same
 * object does not emit a change.
 **/
void
g_mime_param_list_set_parameter_at (GMimeParamList *list, int index, GMimeParam *param)
{
	GMimeParam *old;

	g_return_if_fail (GMIME_IS_PARAM_LIST (list));
	g_return_if_fail (GMIME_IS_PARAM (param));
	g_return_if_fail (index >= 0 && (guint) index < list->array->len);

	old = list->array->pdata[index];
	if (old == param)
		return;

	g_return_if_fail (!g_ascii_strcasecmp (old->name, param->name) ||
			  g_mime_param_list_index_of (list, param->name) == -1);

	g_object_ref (param);
	g_mime_event_remove (old->changed, (GMimeEventCallback) param_changed, list);
	g_mime_event_add (param->changed, (GMimeEventCallback) param_changed, list);
	list->array->pdata[index] = param;
	g_object_unref (old);
	g_mime_event_emit (list->changed, NULL);
}


/**
 * g_mime_param_list_set_parameter:
 * @list: a #GMimeParamList
 * @name: The name of the parameter
 * @value: The parameter value
 *
 * Sets the specified parameter to @value.
 **/
void
g_mime_param_list_set_parameter (GMimeParamList *list, const char *name, const char *value)
{
	GMimeParam *param;
	guint i;
	
	g_return_if_fail (GMIME_IS_PARAM_LIST (list));
	g_return_if_fail (param_name_is_valid (name));
	g_return_if_fail (value != NULL && g_utf8_validate (value, -1, NULL));
	
	for (i = 0; i < list->array->len; i++) {
		param = list->array->pdata[i];
		
		if (!g_ascii_strcasecmp (param->name, name)) {
			g_mime_param_set_value (param, value);
			return;
		}
	}
	
	g_return_if_fail (list->array->len < G_MAXINT);
	param = g_mime_param_new (name, value);
	
	param_list_add (list, param);
	
	g_mime_event_emit (list->changed, NULL);
}


/**
 * g_mime_param_list_get_parameter:
 * @list: list: a #GMimeParamList
 * @name: the name of the parameter
 *
 * Gets the #GMimeParam with the given @name.
 *
 * Returns: (transfer none): the requested #GMimeParam.
 **/
GMimeParam *
g_mime_param_list_get_parameter (GMimeParamList *list, const char *name)
{
	int index = g_mime_param_list_index_of (list, name);

	return index != -1 ? list->array->pdata[index] : NULL;
}


/**
 * g_mime_param_list_get_parameter_at:
 * @list: a #GMimeParamList
 * @index: the index of the requested parameter
 *
 * Gets the #GMimeParam at the specified @index.
 *
 * Returns: (transfer none): the #GMimeParam at the specified index.
 **/
GMimeParam *
g_mime_param_list_get_parameter_at (GMimeParamList *list, int index)
{
	g_return_val_if_fail (GMIME_IS_PARAM_LIST (list), NULL);
	g_return_val_if_fail (index >= 0, NULL);
	
	if ((guint) index >= list->array->len)
		return NULL;
	
	return list->array->pdata[index];
}


/**
 * g_mime_param_list_remove:
 * @list: a #GMimeParamList
 * @name: the name of the parameter
 *
 * Removes a parameter from the #GMimeParamList.
 *
 * Returns: %TRUE if the specified parameter was removed or %FALSE otherwise.
 **/
gboolean
g_mime_param_list_remove (GMimeParamList *list, const char *name)
{
	int index = g_mime_param_list_index_of (list, name);

	return index != -1 && g_mime_param_list_remove_at (list, index);
}

/**
 * g_mime_param_list_remove_parameter:
 * @list: a #GMimeParamList
 * @param: a #GMimeParam
 *
 * Removes only this exact object, not another parameter with the same name.
 *
 * Returns: whether @param was removed.
 **/
gboolean
g_mime_param_list_remove_parameter (GMimeParamList *list, GMimeParam *param)
{
	int index = g_mime_param_list_index_of_parameter (list, param);

	return index != -1 && g_mime_param_list_remove_at (list, index);
}


/**
 * g_mime_param_list_remove_at:
 * @list: a #GMimeParamList
 * @index: index of the param to remove
 *
 * Removes a #GMimeParam from the #GMimeParamList at the specified index.
 *
 * Returns: %TRUE if a #GMimeParam was removed or %FALSE otherwise.
 **/
gboolean
g_mime_param_list_remove_at (GMimeParamList *list, int index)
{
	GMimeParam *param;
	
	g_return_val_if_fail (GMIME_IS_PARAM_LIST (list), FALSE);
	g_return_val_if_fail (index >= 0, FALSE);
	
	if ((guint) index >= list->array->len)
		return FALSE;
	
	param = list->array->pdata[index];
	g_mime_event_remove (param->changed, (GMimeEventCallback) param_changed, list);
	g_ptr_array_remove_index (list->array, index);
	g_object_unref (param);
	g_mime_event_emit (list->changed, NULL);
	
	return TRUE;
}

/**
 * g_mime_param_list_to_string:
 * @list: a #GMimeParamList
 *
 * Formats parameters in order, each prefixed by "; " and with its value
 * quoted and escaped. No transport encoding or folding is performed.
 *
 * Returns: (transfer full): the display string, or an empty string for an
 * empty list.
 **/
char *
g_mime_param_list_to_string (GMimeParamList *list)
{
	GString *str;
	guint i;

	g_return_val_if_fail (GMIME_IS_PARAM_LIST (list), NULL);

	str = g_string_new ("");
	for (i = 0; i < list->array->len; i++) {
		g_string_append (str, "; ");
		param_append (str, list->array->pdata[i]);
	}

	return g_string_free (str, FALSE);
}


static char *
encode_param_value (GMimeParam *param, const char **charset, gsize *length)
{
	const char *inptr;
	GMimeCharset best;

	*length = strlen (param->value);

	g_mime_charset_init (&best);
	g_mime_charset_step (&best, param->value, *length);

	if (best.level == 0) {
		for (inptr = param->value; *inptr; inptr++) {
			if (g_ascii_iscntrl (*inptr)) {
				best.level = 1;
				break;
			}
		}
	}

	if (best.level == 0)
		*charset = "us-ascii";
	else if (best.level == 1)
		*charset = "iso-8859-1";
	else
		*charset = param->charset ? param->charset : "utf-8";

	/* Select a lossless charset for the entire value before splitting it. RFC 2231
	 * declares only one charset, and RFC 2047 parameter metadata retains only one. */
	return _g_mime_utils_convert_from_utf8 (param->value, *length, charset, length);
}

static size_t
param_quote_overhead (GMimeParam *param, GMimeFormatOptions *options)
{
	const char *inptr;
	size_t quote = 0;

	if (!*param->value || param->always_quote || g_mime_format_options_get_always_quote_param_values (options))
		quote = 2;

	for (inptr = param->value; *inptr; inptr++) {
		if (!is_attrchar (*inptr) || is_lwsp (*inptr))
			quote = MAX (quote, 2);

		if (*inptr == '"' || *inptr == '\\')
			quote++;
	}

	return quote;
}

static gboolean
param_value_needs_encoding (const char *value, GMimeFormatOptions *options)
{
	const unsigned char *inptr = (const unsigned char *) value;
	gboolean international = g_mime_format_options_get_allow_international (options);

	/* Literal encoded-word prefixes must also be encoded, or parsing a quoted
	 * parameter would decode text that was meant to be preserved literally. */
	for ( ; *inptr; inptr++) {
		if ((*inptr > 127 && !international) || g_ascii_iscntrl (*inptr) ||
		    (*inptr == '=' && inptr[1] == '?'))
			return TRUE;
	}

	return FALSE;
}

static GMimeParamEncodingMethod
param_encoding_method (GMimeParam *param, GMimeFormatOptions *options)
{
	GMimeParamEncodingMethod requested = param->method;
	size_t length = strlen (param->value);

	/* An empty value cannot be a token: quote it, regardless of the name's length. */
	if (!length)
		return GMIME_PARAM_ENCODING_METHOD_DEFAULT;

	if (requested == GMIME_PARAM_ENCODING_METHOD_DEFAULT)
		requested = g_mime_format_options_get_param_encoding_method (options);

	if (strlen (param->name) + 1 + length + param_quote_overhead (param, options) >=
	    g_mime_format_options_get_max_line_length (options))
		return requested;

	if (param_value_needs_encoding (param->value, options))
		return requested;

	return GMIME_PARAM_ENCODING_METHOD_DEFAULT;
}

static char *
convert_param_chunk (const char *value, gsize length, GIConv cd, gsize *written, GError **error)
{
	if (g_iconv (cd, NULL, NULL, NULL, NULL) == (gsize) -1) {
		g_set_error (error, G_CONVERT_ERROR, G_CONVERT_ERROR_FAILED, "Cannot reset converter: %s", g_strerror (errno));
		return NULL;
	}

	return g_convert_with_iconv (value, length, cd, NULL, written, error);
}

static gboolean
encode_rfc2047_param (GMimeParam *param, GMimeFormatOptions *options, gboolean fold,
		     gboolean has_next, GString *str, size_t *used)
{
	const char *charset, *conversion_charset, *start = param->value, *end, *next, *newline;
	size_t maxline = g_mime_format_options_get_max_line_length (options);
	size_t line = *used, limit, chars;
	char *converted, *mime_charset;
	gboolean success = FALSE;
	GError *error = NULL;
	GString *out, *word;
	gsize length;
	GIConv cd;

	/* Validate a lossless charset for the entire value before splitting it.
	 * Discard these bytes and their length: each chunk is converted independently. */
	converted = encode_param_value (param, &charset, &length);
	g_free (converted);

	charset = mime_charset = _g_mime_charset_get_mime_name (charset);
	conversion_charset = charset;

	if (!g_ascii_strcasecmp (charset, "utf-16"))
		conversion_charset = "utf-16le";
	else if (!g_ascii_strcasecmp (charset, "utf-32"))
		conversion_charset = "utf-32le";

	cd = g_iconv_open (g_mime_charset_iconv_name (conversion_charset), g_mime_charset_iconv_name ("UTF-8"));

	if (cd == (GIConv) -1) {
		g_warning ("Cannot open parameter charset '%s': %s", charset, g_strerror (errno));
		g_free (mime_charset);
		return FALSE;
	}

	newline = g_mime_format_options_get_newline (options);
	out = g_string_new (";");
	word = g_string_new (NULL);
	line++;

	chars = _g_mime_count_utf16_units (start, start + strlen (start));

	/* Account for: <SPACE> + <NAME> + "=\"=?<CHARSET>?b?<10 chars>?=\"" */
	if (fold && line + strlen (param->name) + strlen (charset) + 10 + MIN (chars, 10) >= maxline) {
		g_string_append_printf (out, "%s\t", newline);
		line = 1;
	} else {
		g_string_append_c (out, ' ');
		line++;
	}

	g_string_append_printf (out, "%s=\"", param->name);
	line += strlen (param->name) + 2;

	while (*start) {
		gsize byte_count = 0, encode_count = 0;

		limit = maxline - 2;

		if (fold)
			limit = line < maxline - 1 ? maxline - line - 1 : 0;

		end = start;

		while (*end) {
			gunichar c = g_utf8_get_char (end);

			next = g_utf8_next_char (end);
			if (c < 127) {
				if (is_ctrl (c) || c == '"' || c == '\\')
					encode_count++;
				byte_count++;
			} else if (c < 256) {
				/* iso-8859-1 */
				encode_count++;
				byte_count++;
			} else {
				if (!(converted = convert_param_chunk (end, next - end, cd, &length, &error)))
					goto done;

				g_free (converted);
				byte_count += length;
				encode_count += length;
			}

			if (limit <= 1 || _g_mime_utils_rfc2047_estimate_word_length (charset, byte_count, encode_count) >= limit - 1)
				break;

			end = next;
		}

		/* Empty first words match the estimate, but oversized prefixes must still make progress. */
		if (end == start && (!fold || line + strlen (charset) + 8 > maxline))
			end = g_utf8_next_char (start);

		while (TRUE) {
			size_t suffix = 1 + (!*end && has_next);

			g_string_truncate (word, 0);
			_g_mime_utils_rfc2047_encode_word (word, start, end - start, charset, FALSE);

			if (fold && line + word->len + suffix > maxline && end == g_utf8_next_char (start) &&
			    line + strlen (charset) + 8 <= maxline) {
				end = start;
				continue;
			}

			if ((word->len <= 75 && (!fold || line + word->len + suffix <= maxline)) ||
			    end <= g_utf8_next_char (start))
				break;

			/* Retain the native encoded-word and physical-line bounds when estimates underestimate output. */
			end = g_utf8_prev_char (end);
		}

		g_string_append_len (out, word->str, word->len);
		line += word->len;
		start = end;

		if (*start) {
			if (fold) {
				g_string_append_printf (out, "%s\t", newline);
				line = 1;
			} else {
				g_string_append_c (out, ' ');
				line++;
			}
		}
	}

	g_string_append_c (out, '"');
	g_string_append_len (str, out->str, out->len);
	*used = line + 1;
	success = TRUE;

done:
	if (error)
		g_warning ("Cannot encode parameter charset '%s': %s", charset, error->message);

	g_clear_error (&error);
	g_free (mime_charset);
	g_string_free (word, TRUE);
	g_string_free (out, TRUE);
	g_iconv_close (cd);

	return success;
}

static void
g_string_append_len_quoted (GString *str, const char *text, size_t len)
{
	register const char *inptr = text;
	const char *inend = text + len;
	
	g_string_append_c (str, '"');
	
	while (inptr < inend) {
		if ((*inptr == '"') || *inptr == '\\')
			g_string_append_c (str, '\\');
		
		g_string_append_c (str, *inptr);
		inptr++;
	}
	
	g_string_append_c (str, '"');
}

static char *
encode_rfc2231_chunk (const char *value, gsize length, GIConv cd, gsize bom_length, const char *declaration,
		     gboolean international, gboolean *encoded, gsize *byte_count, GError **error)
{
	gboolean quote = FALSE;
	gsize count, i, capacity;
	char *converted;
	const char *bytes;
	guint8 *rfc2231;
	GString *out;

	if (!(converted = convert_param_chunk (value, length, cd, &count, error)))
		return NULL;

	g_assert (count >= bom_length);
	bytes = converted + bom_length;
	count -= bom_length;
	*byte_count = count;

	/* Raw continuations are decoded independently of the declared charset. */
	*encoded = declaration || count != length || memcmp (bytes, value, length) != 0;

	for (i = 0; i < count; i++) {
		unsigned char c = (unsigned char) bytes[i];

		if ((c > 127 && !international) || g_ascii_iscntrl (c))
			*encoded = TRUE;
		else if (!is_attrchar (c))
			quote = TRUE;
	}

	if (*encoded) {
		out = g_string_new (declaration);
		capacity = _g_mime_rfc2231_encoder_estimate_output_length (count);

		g_assert (capacity != G_MAXSIZE);

		rfc2231 = g_malloc (capacity);
		count = _g_mime_rfc2231_encode ((const guint8 *) bytes, count, rfc2231, capacity);
		g_string_append_len (out, (const char *) rfc2231, count);
		g_free (rfc2231);
	} else if (quote) {
		out = g_string_new (NULL);
		g_string_append_len_quoted (out, bytes, count);
	} else {
		out = g_string_new_len (bytes, count);
	}

	g_free (converted);

	return g_string_free (out, FALSE);
}

static size_t
shrink_param_chunk (size_t length, size_t minimum, gsize count, gsize excess)
{
	gsize ratio = count / length, remainder = count % length;
	size_t shrink;

	/* Match Math.Round's nearest-even estimate of bytes per UTF-16 character. */
	if (remainder > length - remainder || (remainder == length - remainder && (ratio & 1)))
		ratio++;

	shrink = ratio > 1 ? MAX (excess / ratio, 1) : 1;

	return shrink >= length - minimum ? minimum : length - shrink;
}

static gboolean
encode_rfc2231_param (GMimeParam *param, GMimeFormatOptions *options, gboolean fold, GString *str, size_t *used)
{
	size_t line = *used, nlen = strlen (param->name), max_length, limit, length, remaining, units, minimum;
	gboolean international = g_mime_format_options_get_allow_international (options);
	size_t maxline = g_mime_format_options_get_max_line_length (options);
	const char *charset, *start = param->value, *end, *inend, *newline;
	char *converted, *mime_charset, *declaration = NULL, *value = NULL;
	gboolean success = FALSE, encoded = FALSE;
	guint index = 0, digits, n;
	gsize count, bom_length = 0;
	GError *error = NULL;
	GString *out;
	GIConv cd;

	if (international && !param_value_needs_encoding (param->value, options) && !(param->lang && *param->lang)) {
		/* Unencoded international continuations must contain UTF-8, not the preferred legacy charset. */
		charset = "utf-8";
	} else {
		converted = encode_param_value (param, &charset, &count);
		g_free (converted);
	}

	mime_charset = _g_mime_charset_get_mime_name (charset);

	if (!g_ascii_strcasecmp (mime_charset, "utf-16") || !g_ascii_strcasecmp (mime_charset, "utf-32")) {
		char *explicit_endian = g_strconcat (mime_charset, "le", NULL);

		g_free (mime_charset);
		mime_charset = explicit_endian;
	}

	if (!g_ascii_strncasecmp (mime_charset, "utf-", 4)) {
		char *lowercase = g_ascii_strdown (mime_charset, -1);

		g_free (mime_charset);
		mime_charset = lowercase;
	}

	charset = mime_charset;
	cd = g_iconv_open (g_mime_charset_iconv_name (charset), g_mime_charset_iconv_name ("UTF-8"));

	if (cd == (GIConv) -1) {
		g_warning ("Cannot open parameter charset '%s': %s", charset, g_strerror (errno));
		g_free (mime_charset);
		return FALSE;
	}

	newline = g_mime_format_options_get_newline (options);
	out = g_string_new (NULL);

	/* Use explicit endianness instead of emitting converter-generated BOMs. */
	converted = convert_param_chunk ("a", 1, cd, &count, &error);

	if (converted) {
		if (count >= 4 && (!memcmp (converted, "\377\376\000\000", 4) ||
				   !memcmp (converted, "\000\000\376\377", 4)))
			bom_length = 4;
		else if (count >= 2 && (!memcmp (converted, "\377\376", 2) || !memcmp (converted, "\376\377", 2)))
			bom_length = 2;

		if (bom_length == 4)
			charset = (unsigned char) converted[0] == 0xff ? "utf-32le" : "utf-32be";
		else if (bom_length == 2)
			charset = (unsigned char) converted[0] == 0xff ? "utf-16le" : "utf-16be";

		g_free (converted);
	} else if (g_error_matches (error, G_CONVERT_ERROR, G_CONVERT_ERROR_ILLEGAL_SEQUENCE)) {
		/* Encodings unable to represent ASCII cannot emit a Unicode BOM. */
		g_clear_error (&error);
	} else {
		goto done;
	}

	if (param_value_needs_encoding (param->value, options) || (param->lang && *param->lang))
		declaration = g_strdup_printf ("%s'%s'", charset, param->lang ? param->lang : "");

	max_length = nlen < maxline - 6 ? MAX (maxline - (nlen + 6), 3) : 3;
	inend = start + strlen (start);
	remaining = _g_mime_count_utf16_units (start, inend);

	do {
		gsize declaration_length = declaration ? strlen (declaration) : 0;

		digits = 1;

		for (n = index; n >= 10; n /= 10)
			digits++;

		limit = max_length;
		units = 0;
		end = start;

		/* A long parameter name and charset may leave no room for data in the first chunk. */
		if (declaration && declaration_length >= limit) {
			value = g_strdup (declaration);
			encoded = TRUE;
		} else {
			/* Reduce the max allowed length to account for the charset declaration. */
			limit -= declaration_length;
			minimum = g_utf8_get_char (start) >= 0x10000 ? 2 : 1;
			units = MAX (MIN (limit, remaining), minimum);

			while (TRUE) {
				gsize encoded_length, overhead;

				end = _g_mime_get_utf16_chunk_end (start, inend, &units);
				if (!(value = encode_rfc2231_chunk (start, end - start, cd, bom_length,
					declaration, international, &encoded, &count, &error)))
					goto done;

				if (count > limit && units > minimum) {
					units = shrink_param_chunk (units, minimum, count, count - limit);
					g_clear_pointer (&value, g_free);
					continue;
				}

				encoded_length = strlen (value) - declaration_length;
				if (encoded && units > minimum && encoded_length > 3 && encoded_length > limit) {
					gssize excess = 0;
					gsize i;

					for (i = encoded_length; i > limit;) {
						i--;
						excess += encoded && value[declaration_length + i] == '%' ? -1 : 1;
					}

					units = shrink_param_chunk (units, minimum, count, (gsize) MAX (excess, 1));
					g_clear_pointer (&value, g_free);
					continue;
				}

				overhead = nlen + (encoded ? 2 : 1) + 2;

				if (index || *end)
					overhead += digits + 1;

				if (fold && units > minimum && nlen + digits + 5 < maxline &&
				    (overhead >= maxline || strlen (value) > maxline - overhead)) {
					/* Reserve the next semicolon and account for growing continuation indices. */
					units--;
					g_clear_pointer (&value, g_free);
					continue;
				}

				break;
			}
		}

		/* Only the first continuation declares the charset and language. */
		g_clear_pointer (&declaration, g_free);

		g_string_append_c (out, ';');
		line++;
		length = nlen + (encoded ? 2 : 1) + strlen (value);

		if (index || *end)
			length += digits + 1;

		if (fold && (index || *end || line + 1 + length >= maxline)) {
			g_string_append_printf (out, "%s\t", newline);
			line = 1;
		} else {
			g_string_append_c (out, ' ');
			line++;
		}

		g_string_append (out, param->name);
		if (index || *end)
			g_string_append_printf (out, "*%u", index);

		if (encoded)
			g_string_append_c (out, '*');

		g_string_append_c (out, '=');
		g_string_append (out, value);
		line += length;

		g_clear_pointer (&value, g_free);
		start = end;
		remaining -= units;
		index++;
	} while (*start);

	g_string_append_len (str, out->str, out->len);
	*used = line;
	success = TRUE;

done:
	if (error)
		g_warning ("Cannot encode parameter charset '%s': %s", charset, error->message);

	g_clear_error (&error);
	g_free (value);
	g_free (declaration);
	g_free (mime_charset);
	g_string_free (out, TRUE);
	g_iconv_close (cd);

	return success;
}


/**
 * g_mime_param_list_encode:
 * @list: a #GMimeParamList
 * @options: (nullable): a #GMimeFormatOptions or %NULL
 * @fold: %TRUE if the parameter list should be folded; otherwise, %FALSE
 * @str: (out): the output string buffer
 *
 * Encodes the parameter list into @str, folding lines if required.
 * Parameter values must be valid UTF-8. When international formatting is
 * enabled, ordinary Unicode values are quoted without transport encoding
 * and long RFC 2231 values may use unencoded UTF-8 continuations. Controls
 * and values resembling encoded words still require transport encoding.
 **/
void
g_mime_param_list_encode (GMimeParamList *list, GMimeFormatOptions *options, gboolean fold, GString *str)
{
	size_t used, maxline;
	const char *newline;
	guint count, i;
	
	g_return_if_fail (GMIME_IS_PARAM_LIST (list));
	g_return_if_fail (str != NULL);
	
	newline = g_mime_format_options_get_newline (options);
	maxline = g_mime_format_options_get_max_line_length (options);
	count = list->array->len;
	used = str->len;
	
	for (i = 0; i < count; i++) {
		GMimeParamEncodingMethod method;
		size_t nlen, vlen, length, quote = 0;
		gboolean fits;
		GMimeParam *param;
		char *value;
		
		param = (GMimeParam *) list->array->pdata[i];
		
		if (!param->value)
			continue;

		g_return_if_fail (g_utf8_validate (param->value, -1, NULL));
		
		method = param_encoding_method (param, options);
		if (method == GMIME_PARAM_ENCODING_METHOD_RFC2047) {
			if (!encode_rfc2047_param (param, options, fold, i + 1 < count, str, &used))
				return;
			continue;
		}

		if (method == GMIME_PARAM_ENCODING_METHOD_RFC2231) {
			if (!encode_rfc2231_param (param, options, fold, str, &used))
				return;
			continue;
		}

		value = g_strdup (param->value);
		quote = param_quote_overhead (param, options);
		vlen = strlen (value);
		
		nlen = strlen (param->name);
		
		g_string_append_c (str, ';');
		used++;

		length = used + nlen + vlen + quote + 2;
		fits = length < maxline;

		if (length == maxline && i + 1 == count) {
			const char *line = strrchr (str->str, '\n');

			line = line ? line + 1 : str->str;
			fits = _g_mime_count_utf16_units (line, str->str + str->len) +
				nlen + _g_mime_count_utf16_units (value, value + vlen) + quote + 2 < maxline;
		}

		if (fold && !fits) {
			g_string_append (str, newline);
			g_string_append_c (str, '\t');
			used = 1;
		} else {
			g_string_append_c (str, ' ');
			used++;
		}
		
		g_string_append_printf (str, "%s=", param->name);

		if (quote)
			g_string_append_len_quoted (str, value, vlen);
		else
			g_string_append_len (str, value, vlen);

		used += nlen + 1 + vlen + quote;
		
		g_free (value);
	}
	
	if (fold)
		g_string_append (str, newline);
}


#define INT_OVERFLOW(x,d) (((x) > (INT_MAX / 10)) || ((x) == (INT_MAX / 10) && (d) > (INT_MAX % 10)))

static int
decode_int (const char **in)
{
	const unsigned char *inptr;
	int digit, n = 0;
	
	skip_cfws (in);
	
	inptr = (const unsigned char *) *in;
	while (isdigit ((int) *inptr)) {
		digit = (*inptr - '0');
		if (INT_OVERFLOW (n, digit)) {
			while (isdigit ((int) *inptr))
				inptr++;
			break;
		}
		
		n = (n * 10) + digit;
		
		inptr++;
	}
	
	*in = (const char *) inptr;
	
	return n;
}

static char *
decode_quoted_string (const char **in, gboolean unquote)
{
	const char *start, *inptr = *in;
	char *outptr, *out = NULL;
	gboolean unescape = FALSE;
	
	skip_cfws (&inptr);
	
	if (*inptr != '"') {
		*in = inptr;
		return NULL;
	}
	
	start = inptr++;
	
	while (*inptr && *inptr != '"') {
		if (*inptr++ == '\\' && *inptr) {
			unescape = TRUE;
			inptr++;
		}
	}
	
	if (!unquote) {
		if (*inptr == '"')
			inptr++;

		*in = inptr;

		return g_strndup (start, (size_t) (inptr - start));
	}

	if (*inptr == '"') {
		start++;
		out = g_strndup (start, (size_t) (inptr - start));
		inptr++;
	} else {
		/* string wasn't properly quoted */
		out = g_strndup (start, (size_t) (inptr - start));
	}
	
	*in = inptr;
	
	if (unescape) {
		inptr = outptr = out;
		while (*inptr) {
			if (*inptr == '\\')
				inptr++;

			if (*inptr)
				*outptr++ = *inptr++;
		}
		
		*outptr = '\0';
	}
	
	return out;
}

static char *
decode_token (GMimeRfcComplianceMode mode, const char **in)
{
	const char *inptr = *in;
	const char *start;
	
	skip_cfws (&inptr);
	
	start = inptr;
	if (mode != GMIME_RFC_COMPLIANCE_LOOSE) {
		while (is_ttoken (*inptr))
			inptr++;
	} else {
		/* Broken mail clients like to make our lives difficult. Scan
		 * for a ';' instead of trusting that the client followed the
		 * specification. */
		while (*inptr && *inptr != ';')
			inptr++;
		
		/* Scan backwards over any trailing lwsp */
		while (inptr > start && is_lwsp (inptr[-1]))
			inptr--;
	}
	
	if (inptr > start) {
		*in = inptr;

		return g_strndup (start, (size_t) (inptr - start));
	} else {
		return NULL;
	}
}

static char *
decode_value (GMimeRfcComplianceMode mode, const char **in, gboolean unquote)
{
	const char *inptr = *in;
	
	skip_cfws (&inptr);
	*in = inptr;
	
	if (*inptr == '"') {
		return decode_quoted_string (in, unquote);
	} else if (is_ttoken (*inptr)) {
		return decode_token (mode, in);
	}
	
	if (mode == GMIME_RFC_COMPLIANCE_LOOSE)
		return decode_token (mode, in);
	
	return NULL;
}

/* This function is basically the same as decode_token()
 * except that it will not accept *'s which have a special
 * meaning for rfc2184 params */
static char *
decode_param_token (const char **in)
{
	const char *inptr = *in;
	const char *start;
	
	skip_cfws (&inptr);
	
	start = inptr;
	while (is_ttoken (*inptr) && *inptr != '*')
		inptr++;

	if (inptr > start) {
		*in = inptr;

		return g_strndup (start, (size_t) (inptr - start));
	} else {
		return NULL;
	}
}

static gboolean
decode_rfc2184_param (const char **in, char **namep, int *part, gboolean *encoded)
{
	gboolean is_rfc2184 = FALSE;
	const char *inptr = *in;
	char *name;
	
	*encoded = FALSE;
	*part = -1;
	
	name = decode_param_token (&inptr);
	
	skip_cfws (&inptr);
	
	if (*inptr == '*') {
		is_rfc2184 = TRUE;
		inptr++;
		
		skip_cfws (&inptr);
		if (*inptr == '=') {
			/* form := param*=value */
			*encoded = TRUE;
		} else {
			/* form := param*#=value or param*#*=value */
			*part = decode_int (&inptr);
			
			skip_cfws (&inptr);
			if (*inptr == '*') {
				/* form := param*#*=value */
				inptr++;
				*encoded = TRUE;
				skip_cfws (&inptr);
			}
		}
	}
	
	*namep = name;
	
	if (name)
		*in = inptr;
	
	return is_rfc2184;
}

static char *
decode_param_text (GMimeParserOptions *options, char *value, const char **charset,
		   GMimeParamEncodingMethod *method, gint64 offset, gboolean checked)
{
	char *decoded;

	if (strstr (value, "=?") != NULL) {
		decoded = _g_mime_utils_header_decode_text (options, value, strlen (value), charset, offset);
		g_free (value);
		value = decoded;

		if (!checked || *charset)
			*method = GMIME_PARAM_ENCODING_METHOD_RFC2047;

		if (checked && *charset && !g_ascii_strcasecmp (*charset, "UTF-8"))
			*charset = NULL;
	}

	if (!g_utf8_validate (value, -1, NULL)) {
		decoded = g_mime_utils_decode_8bit (options, value, strlen (value));
		g_free (value);
		value = decoded;
	}

	return value;
}

static gboolean
decode_param (GMimeParserOptions *options, const char **in, char **namep, char **valuep, int *id,
	      const char **rfc2047_charset, gboolean *encoded, GMimeParamEncodingMethod *method, gint64 offset)
{
	GMimeRfcComplianceMode mode = g_mime_parser_options_get_parameter_compliance_mode (options);
	gboolean is_rfc2184 = FALSE;
	const char *inptr = *in;
	char *name, *value = NULL;
	
	*method = GMIME_PARAM_ENCODING_METHOD_DEFAULT;
	*rfc2047_charset = NULL;
	
	if ((is_rfc2184 = decode_rfc2184_param (&inptr, &name, id, encoded)))
		*method = GMIME_PARAM_ENCODING_METHOD_RFC2231;
	
	if (*inptr == '=') {
		inptr++;
		value = decode_value (mode, &inptr, *id == -1 || *encoded);
		
		if (!is_rfc2184 && value)
			value = decode_param_text (options, value, rfc2047_charset, method, offset, FALSE);
	}
	
	if (name && value) {
		*valuep = value;
		*namep = name;
		*in = inptr;
		return TRUE;
	}
	
	_g_mime_parser_options_warn (options, offset, GMIME_WARN_INVALID_PARAMETER, name);
	g_free (value);
	g_free (name);
	
	return FALSE;
}

static void
param_unquote_value (char *value)
{
	char *inptr, *outptr;

	g_mime_utils_unquote_string (value);
	inptr = outptr = value;

	while (*inptr) {
		if (*inptr != '\r' && *inptr != '\n')
			*outptr++ = *inptr;

		inptr++;
	}

	*outptr = '\0';
}

static gboolean
decode_param_full (GMimeParserOptions *options, const char *text, const char **in,
		   char **namep, char **valuep, int *id, const char **charset,
		   gboolean *encoded, GMimeParamEncodingMethod *method, GError **error)
{
	GMimeRfcComplianceMode mode = g_mime_parser_options_get_parameter_compliance_mode (options);
	const char *inptr = *in, *start = inptr, *name_end, *value_start;
	char *value, *outptr;
	gboolean quoted;
	int number = 0, digit;
	gsize length;

	*method = GMIME_PARAM_ENCODING_METHOD_DEFAULT;
	*charset = NULL;
	*encoded = FALSE;
	*id = -1;

	while ((guchar) *inptr < 128 && is_attrchar (*inptr))
		inptr++;

	if (inptr == start)
		return _g_mime_parse_error (error, text, inptr, "Invalid parameter name");

	name_end = inptr;
	if (!_g_mime_parse_cfws (text, &inptr, error))
		return FALSE;

	if (*inptr == '*') {
		*method = GMIME_PARAM_ENCODING_METHOD_RFC2231;
		inptr++;

		if (!_g_mime_parse_cfws (text, &inptr, error))
			return FALSE;

		if (g_ascii_isdigit (*inptr)) {
			while (g_ascii_isdigit (*inptr)) {
				digit = *inptr - '0';

				if (INT_OVERFLOW (number, digit))
					return _g_mime_parse_error (error, text, inptr, "Parameter continuation index overflow");

				number = number * 10 + digit;
				inptr++;
			}

			*id = number;
			if (!_g_mime_parse_cfws (text, &inptr, error))
				return FALSE;

			if (*inptr == '*') {
				*encoded = TRUE;
				inptr++;

				if (!_g_mime_parse_cfws (text, &inptr, error))
					return FALSE;
			}
		} else {
			*encoded = TRUE;
		}
	}

	if (*inptr != '=')
		return _g_mime_parse_error (error, text, inptr, "Expected parameter equals sign");

	inptr++;
	if (!_g_mime_parse_cfws (text, &inptr, error))
		return FALSE;

	if (!*inptr)
		return _g_mime_parse_error (error, text, inptr, "Incomplete parameter value");

	value_start = inptr;
	quoted = *inptr == '"';

	if (quoted) {
		if (!skip_quoted (&inptr))
			return _g_mime_parse_error (error, text, inptr, "Incomplete quoted parameter value");
	} else if (mode == GMIME_RFC_COMPLIANCE_STRICT) {
		while ((guchar) *inptr < 127 && is_ttoken (*inptr))
			inptr++;
	} else {
		/* Google Docs and other mailers send unquoted filenames with spaces,
		 * sometimes folded across lines. Recover through the next semicolon. */
		while (*inptr && *inptr != ';')
			inptr++;
	}

	length = inptr - value_start;
	value = g_strndup (value_start, length);

	if (!quoted) {
		const char *p = value;

		outptr = value;
		while (*p) {
			if (*p != '\r' && *p != '\n')
				*outptr++ = *p;
			p++;
		}

		/* Broken mailers may leave whitespace after an unquoted filename. */
		while (outptr > value && is_lwsp (outptr[-1]))
			outptr--;

		*outptr = '\0';
	} else if (*encoded) {
		/* Some mailers quote RFC 2231 encoded values even though the syntax forbids it. */
		memmove (value, value + 1, length - 2);
		value[length - 2] = '\0';
	} else if (*id == -1) {
		param_unquote_value (value);
	}

	if (*id == -1 && !*encoded) {
		if (quoted || length > 0)
			*method = GMIME_PARAM_ENCODING_METHOD_RFC2047;
		value = decode_param_text (options, value, charset, method, -1, TRUE);
	}

	*namep = g_strndup (start, name_end - start);
	*valuep = value;
	*in = inptr;

	return TRUE;
}


struct _rfc2184_part {
	char *value;
	int id;
	gboolean encoded;
};

struct _rfc2184_param {
	struct _rfc2184_param *next;
	const char *charset;
	GMimeParam *param;
	GPtrArray *parts;
	char *lang;
};

static void
rfc2184_part_free (gpointer data)
{
	struct _rfc2184_part *part = data;

	g_free (part->value);
	g_free (part);
}

static void
rfc2184_param_free (struct _rfc2184_param *param)
{
	g_ptr_array_free (param->parts, TRUE);
	g_free (param->lang);
	g_free (param);
}

static int
rfc2184_sort_cb (const void *v0, const void *v1)
{
	const struct _rfc2184_part *p0 = *((struct _rfc2184_part **) v0);
	const struct _rfc2184_part *p1 = *((struct _rfc2184_part **) v1);
	
	return p0->id - p1->id;
}

static char *
rfc2231_decode (GMimeDecoder *rfc2231, const char *value, gsize *length)
{
	gsize capacity, input_length = strlen (value);
	char *decoded;

	capacity = g_mime_decoder_estimate_output_length (rfc2231, input_length);
	g_assert (capacity < G_MAXSIZE);
	decoded = g_malloc (capacity + 1);
	*length = g_mime_decoder_decode (rfc2231, (const guint8 *) value, input_length,
				       (guint8 *) decoded, capacity);
	decoded[*length] = '\0';

	return decoded;
}

static const char *
rfc2184_param_charset (const char **in, char **langp)
{
	const char *inptr = *in;
	const char *start = *in;
	const char *lang;
	char *charset;
	size_t len;
	
	while (*inptr && *inptr != '\'')
		inptr++;
	
	if (*inptr != '\'' || inptr == start) {
		*langp = NULL;
		return NULL;
	}
	
	len = (size_t) (inptr - start);
	charset = g_alloca (len + 1);
	memcpy (charset, start, len);
	charset[len] = '\0';
	
	lang = ++inptr;
	while (*inptr && *inptr != '\'')
		inptr++;
	
	if (*inptr != '\'') {
		*langp = NULL;
		return NULL;
	}

	*langp = inptr > lang ? g_strndup (lang, (size_t) (inptr - lang)) : NULL;
	inptr++;
	
	*in = inptr;
	
	return g_mime_charset_canon_name (charset);
}

static size_t
charset_invalid_length (const char *charset, const unsigned char *in, size_t len, int error)
{
	size_t n, width;

	if (!g_ascii_strncasecmp (charset, "UTF-16", 6) || !g_ascii_strncasecmp (charset, "UCS-2", 5))
		return MIN (len, 2);

	if (!g_ascii_strncasecmp (charset, "UTF-32", 6) || !g_ascii_strncasecmp (charset, "UCS-4", 5))
		return MIN (len, 4);

	if (error == EINVAL)
		return len;

	if (g_ascii_strcasecmp (charset, "UTF-8") && g_ascii_strcasecmp (charset, "UTF8"))
		return 1;

	if (in[0] < 0xc2 || in[0] > 0xf4 || len < 2)
		return 1;

	width = in[0] < 0xe0 ? 2 : in[0] < 0xf0 ? 3 : 4;
	if (in[1] < 0x80 || in[1] > 0xbf ||
	    (in[0] == 0xe0 && in[1] < 0xa0) || (in[0] == 0xed && in[1] >= 0xa0) ||
	    (in[0] == 0xf0 && in[1] < 0x90) || (in[0] == 0xf4 && in[1] >= 0x90))
		return 1;

	/* Replace a maximal valid UTF-8 prefix once, not once per byte. */
	for (n = 2; n < MIN (width, len) && in[n] >= 0x80 && in[n] <= 0xbf; n++)
		;

	return n;
}

static void
charset_convert (const char **charset, const char *in, size_t inlen, GString *out)
{
	char buffer[256], *outptr;
	const char *inptr = in, *name;
	size_t inleft = inlen, outleft, n, status;
	iconv_t cd;
	int code;

	if (*charset == NULL)
		*charset = "iso-8859-1";

	name = g_mime_charset_iconv_name (*charset);
	cd = iconv_open (g_mime_charset_iconv_name ("UTF-8"), name);

	if (cd == (iconv_t) -1) {
		code = errno;
		if (code != EINVAL)
			g_warning ("Cannot decode parameter charset '%s': %s", *charset, g_strerror (code));

		*charset = "iso-8859-1";

		for (n = 0; n < inlen; n++)
			g_string_append_unichar (out, (unsigned char) in[n]);

		return;
	}

	while (inleft > 0) {
		outptr = buffer;
		outleft = sizeof (buffer);

		status = iconv (cd, (char **) &inptr, &inleft, &outptr, &outleft);
		code = errno;

		g_string_append_len (out, buffer, outptr - buffer);

		if (status != (size_t) -1 || code == E2BIG)
			continue;

		if (code != EILSEQ && code != EINVAL)
			g_warning ("Cannot convert parameter charset '%s': %s", *charset, g_strerror (code));

		if (inleft == 0)
			break;

		n = charset_invalid_length (name, (const unsigned char *) inptr, inleft, code);
		g_string_append_c (out, '?');
		inptr += n;
		inleft -= n;
	}

	do {
		outptr = buffer;
		outleft = sizeof (buffer);

		status = iconv (cd, NULL, NULL, &outptr, &outleft);
		code = errno;

		g_string_append_len (out, buffer, outptr - buffer);
	} while (status == (size_t) -1 && code == E2BIG);

	if (status == (size_t) -1)
		g_warning ("Cannot flush parameter charset '%s': %s", *charset, g_strerror (code));

	g_mime_iconv_close (cd);
}

static char *
rfc2184_decode (const char *value, char **charsetp, char **langp)
{
	GMimeDecoder *rfc2231 = _g_mime_rfc2231_decoder_new ();
	const char *inptr = value, *charset;
	GString *out = g_string_new ("");
	char *decoded;
	size_t len;

	charset = rfc2184_param_charset (&inptr, langp);
	decoded = rfc2231_decode (rfc2231, inptr, &len);
	g_object_unref (rfc2231);
	charset_convert (&charset, decoded, len, out);
	*charsetp = g_strdup (charset);
	g_free (decoded);

	return g_string_free (out, FALSE);
}

static char *
rfc2184_decode_raw (GMimeParserOptions *options, const char *value)
{
	char *decoded = g_mime_utils_decode_8bit (options, value, strlen (value));

	/* Unencoded continuation values are sometimes quoted in real-world messages. */
	if (*value == '"')
		param_unquote_value (decoded);

	return decoded;
}

static void
rfc2184_param_add_part (struct _rfc2184_param *rfc2184, char *value, int id, gboolean encoded)
{
	struct _rfc2184_part *part;
	
	part = g_new (struct _rfc2184_part, 1);
	g_ptr_array_add (rfc2184->parts, part);
	part->id = id;
	part->encoded = encoded;
	part->value = value;
}

static struct _rfc2184_param *
rfc2184_param_new (char *name, char *value, int id, gboolean encoded)
{
	struct _rfc2184_param *rfc2184;
	
	rfc2184 = g_new (struct _rfc2184_param, 1);
	rfc2184->parts = g_ptr_array_new_with_free_func (rfc2184_part_free);
	rfc2184->next = NULL;
	
	rfc2184->charset = NULL;
	rfc2184->lang = NULL;
	rfc2184_param_add_part (rfc2184, value, id, encoded);
	
	rfc2184->param = param_new ();
	rfc2184->param->method = GMIME_PARAM_ENCODING_METHOD_RFC2231;
	rfc2184->param->name = name;
	
	return rfc2184;
}

static GMimeParamList *
decode_param_list (GMimeParserOptions *options, const char *text, const char *in,
		   gint64 offset, gboolean checked, GError **error)
{
	gboolean can_warn = g_mime_parser_options_get_warning_callback (options) != NULL;
	struct _rfc2184_param *rfc2184, *list, *t;
	gboolean have_charset, failed = FALSE;
	char *name, *value, *charset, *lang;
	GMimeParamEncodingMethod method;
	const char *rfc2047_charset;
	struct _rfc2184_part *part;
	GHashTable *rfc2184_hash;
	const char *inptr = in;
	GMimeParamList *params;
	GMimeParam *param;
	GMimeDecoder *rfc2231;
	char *decoded;
	gsize length;
	gboolean encoded;
	GString *buf, *octets;
	guint i;
	int id;
	
	params = g_mime_param_list_new ();
	
	list = NULL;
	t = (struct _rfc2184_param *) &list;
	rfc2184_hash = g_hash_table_new (g_mime_strcase_hash, g_mime_strcase_equal);
	
	while (TRUE) {
		if (checked) {
			if (!_g_mime_parse_cfws (text, &inptr, error)) {
				failed = TRUE;
				break;
			}
		} else {
			skip_cfws (&inptr);
		}
		
		if (*inptr == '\0')
			break;

		if (checked && *inptr == ';') {
			inptr++;
			continue;
		}
		
		if (checked) {
			if (!decode_param_full (options, text, &inptr, &name, &value, &id,
						&rfc2047_charset, &encoded, &method, error)) {
				failed = TRUE;
				break;
			}
		} else if (!decode_param (options, &inptr, &name, &value, &id, &rfc2047_charset, &encoded, &method, offset)) {
			skip_cfws (&inptr);
			
			if (*inptr == ';') {
				inptr++;
				continue;
			}
			
			break;
		}
		
		if (id != -1) {
			/* we have a multipart rfc2184 param */
			if (!(rfc2184 = g_hash_table_lookup (rfc2184_hash, name))) {
				rfc2184 = rfc2184_param_new (name, value, id, encoded);
				param = rfc2184->param;
				t->next = rfc2184;
				t = rfc2184;
				
				g_hash_table_insert (rfc2184_hash, param->name, rfc2184);
				param_list_add (params, param);
			} else {
				rfc2184_param_add_part (rfc2184, value, id, encoded);
				g_free (name);
			}
		} else {
			param = param_new ();
			param->name = name;
			
			if (encoded) {
				/* singleton encoded rfc2184 param value */
				param->value = rfc2184_decode (value, &charset, &lang);
				param->charset = charset;
				param->method = method;
				param->lang = lang;
				g_free (value);
			} else {
				/* normal parameter value */
				param->charset = rfc2047_charset ? g_strdup (rfc2047_charset) : NULL;
				param->method = method;
				param->value = value;
			}
			
			param_list_add (params, param);
		}
		
		if (checked) {
			if (!_g_mime_parse_cfws (text, &inptr, error)) {
				failed = TRUE;
				break;
			}

			if (!*inptr)
				break;

			if (*inptr == ';') {
				inptr++;
			} else if (g_mime_parser_options_get_parameter_compliance_mode (options) == GMIME_RFC_COMPLIANCE_STRICT) {
				_g_mime_parse_error (error, text, inptr, "Invalid parameter list separator");
				failed = TRUE;
				break;
			}
		} else {
			skip_cfws (&inptr);
			if (*inptr++ != ';')
				break;
		}
	}
	
	g_hash_table_destroy (rfc2184_hash);

	if (failed) {
		while (list) {
			t = list->next;
			rfc2184_param_free (list);
			list = t;
		}

		g_object_unref (params);
		return NULL;
	}
	
	rfc2184 = list;
	while (rfc2184 != NULL) {
		t = rfc2184->next;
		
		param = rfc2184->param;
		buf = g_string_new ("");
		octets = g_string_new ("");
		rfc2231 = _g_mime_rfc2231_decoder_new ();
		have_charset = FALSE;
		
		g_ptr_array_sort (rfc2184->parts, rfc2184_sort_cb);

		for (i = 0; i < rfc2184->parts->len; i++) {
			part = rfc2184->parts->pdata[i];

			if (part->encoded) {
				inptr = part->value;

				if (!have_charset) {
					rfc2184->charset = rfc2184_param_charset (&inptr, &rfc2184->lang);
					have_charset = TRUE;
				}

				decoded = rfc2231_decode (rfc2231, inptr, &length);
				g_string_append_len (octets, decoded, length);
				g_free (decoded);

				if (i + 1 == rfc2184->parts->len ||
				    !((struct _rfc2184_part *) rfc2184->parts->pdata[i + 1])->encoded) {
					charset_convert (&rfc2184->charset, octets->str, octets->len, buf);
					g_string_truncate (octets, 0);
				}
			} else {
				g_mime_decoder_reset (rfc2231);
				decoded = rfc2184_decode_raw (options, part->value);
				g_string_append (buf, decoded);
				g_free (decoded);
			}
		}
		
		g_object_unref (rfc2231);
		g_string_free (octets, TRUE);
		
		param->value = g_string_free (buf, FALSE);
		param->charset = rfc2184->charset ? g_strdup (rfc2184->charset) : NULL;
		param->lang = g_steal_pointer (&rfc2184->lang);
		
		rfc2184_param_free (rfc2184);
		rfc2184 = t;
	}

	if (can_warn) {
		GMimeParam *p;
		guint j;
		
		for (i = 0; i < params->array->len; i++) {
			param = params->array->pdata[i];
		
			for (j = i + 1; j < params->array->len; j++) {
				p = params->array->pdata[j];
				
				if (g_ascii_strcasecmp (param->name, p->name) != 0)
					continue;
				
				if (strcmp (param->value, p->value) != 0)
					_g_mime_parser_options_warn (options, offset, GMIME_CRIT_CONFLICTING_PARAMETER, param->name);
				else
					_g_mime_parser_options_warn (options, offset, GMIME_WARN_DUPLICATED_PARAMETER, param->name);
				
				break;
			}
		}
	}

	if (checked) {
		GMimeParamList *unique = g_mime_param_list_new ();
		GHashTable *names = g_hash_table_new (g_mime_strcase_hash, g_mime_strcase_equal);
		GMimeParam *previous;

		for (i = 0; i < params->array->len; i++) {
			param = params->array->pdata[i];

			if (!(previous = g_hash_table_lookup (names, param->name))) {
				param_list_add (unique, g_object_ref (param));
				g_hash_table_insert (names, param->name, param);
			} else if (param->method == GMIME_PARAM_ENCODING_METHOD_RFC2231) {
				/* Prefer the extended value over its ASCII fallback, as recommended
				 * for HTTP parameters by RFC 6266 when MIME gives no guidance. */
				g_mime_param_set_value (previous, param->value);
				g_mime_param_set_charset (previous, param->charset);
				g_mime_param_set_lang (previous, param->lang);
				g_mime_param_set_encoding_method (previous, param->method);
			}
		}

		g_hash_table_destroy (names);
		g_object_unref (params);
		params = unique;
	}
	
	return params;
}


/**
 * g_mime_param_list_parse:
 * @options: a #GMimeParserOptions or %NULL
 * @str: a string to parse
 *
 * Parses the input string into a parameter list using best-effort recovery.
 * Duplicate names are retained and malformed suffixes may be ignored. Use
 * g_mime_param_list_parse_full() for all-or-nothing parsing.
 *
 * Returns: (transfer full): a new #GMimeParamList.
 **/
GMimeParamList *
g_mime_param_list_parse (GMimeParserOptions *options, const char *str)
{
	return _g_mime_param_list_parse (options, str, -1);
}

/**
 * g_mime_param_list_parse_full:
 * @options: (nullable): parser options, or %NULL for the defaults
 * @str: parameter-list text
 * @length: byte length, or -1 for a NUL-terminated string
 * @error: (nullable): return location for a #GError
 *
 * Parses the complete input, returning %NULL with GMIME_ERROR_PARSE_ERROR
 * and a byte offset on malformed input. Explicitly bounded input need not
 * be NUL-terminated but cannot contain embedded NUL bytes. Empty lists and
 * empty semicolon-delimited entries are accepted.
 *
 * Strict parameter compliance requires semicolon separators. Loose modes
 * also accept missing separators after quoted values and unquoted values
 * containing spaces. Unterminated comments and quoted values are errors in
 * every mode.
 *
 * Names are matched case-insensitively. The first ordinary value wins, but
 * an RFC 2231 value overrides it. Repeated standalone RFC 2231 values use
 * the last value; numbered continuations are combined in numeric order.
 * The first occurrence determines the parameter's position and name spelling.
 *
 * Returns: (nullable) (transfer full): the parsed list, or %NULL on error.
 **/
GMimeParamList *
g_mime_param_list_parse_full (GMimeParserOptions *options, const char *str, gssize length, GError **error)
{
	GMimeParamList *params;
	char *copy;

	g_return_val_if_fail (str != NULL, NULL);
	g_return_val_if_fail (length >= -1, NULL);
	g_return_val_if_fail (error == NULL || *error == NULL, NULL);

	if (!(copy = _g_mime_parse_text (str, length, error)))
		return NULL;

	params = _g_mime_param_list_parse_full (options, copy, copy, error);
	g_free (copy);

	return params;
}

GMimeParamList *
_g_mime_param_list_parse_full (GMimeParserOptions *options, const char *text, const char *in, GError **error)
{
	return decode_param_list (options, text, in, -1, TRUE, error);
}

GMimeParamList *
_g_mime_param_list_parse (GMimeParserOptions *options, const char *str, gint64 offset)
{
	g_return_val_if_fail (str != NULL, NULL);
	
	return decode_param_list (options, str, str, offset, FALSE, NULL);
}
