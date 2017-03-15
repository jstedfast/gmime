/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*  GMime
 *  Copyright (C) 2000-2017 Jeffrey Stedfast
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
#include "gmime-events.h"
#include "gmime-internal.h"
#include "gmime-table-private.h"
#include "gmime-parse-utils.h"
#include "gmime-iconv-utils.h"
#include "gmime-charset.h"
#include "gmime-utils.h"
#include "gmime-iconv.h"


#ifdef ENABLE_WARNINGS
#define w(x) x
#else
#define w(x)
#endif /* ENABLE_WARNINGS */

#define d(x)

/**
 * SECTION: gmime-param
 * @title: GMimeParam
 * @short_description: Content-Type and Content-Disposition parameters
 * @see_also: #GMimeContentType
 *
 * A #GMimeParam is a parameter name/value pair as found on MIME
 * header fields such as Content-Type and Content-Disposition.
 **/


static unsigned char tohex[16] = {
	'0', '1', '2', '3', '4', '5', '6', '7',
	'8', '9', 'A', 'B', 'C', 'D', 'E', 'F'
};


static GMimeParam *
g_mime_param_new_internal (void)
{
	GMimeParam *param;
	
	param = g_slice_new (GMimeParam);
	
	param->changed = g_mime_event_new (param);
	param->method = GMIME_PARAM_ENCODING_METHOD_DEFAULT;
	param->charset = NULL;
	param->value = NULL;
	param->name = NULL;
	param->lang = NULL;
	
	return param;
}


/**
 * g_mime_param_new:
 * @name: parameter name
 * @value: parameter value
 *
 * Creates a new #GMimeParam node with name @name and value @value.
 *
 * Returns: a new paramter structure.
 **/
static GMimeParam *
g_mime_param_new (const char *name, const char *value)
{
	GMimeParam *param;
	
	param = g_mime_param_new_internal ();
	param->value = g_strdup (value);
	param->name = g_strdup (name);
	
	return param;
}


/**
 * g_mime_param_free:
 * @param: a #GMimeParam
 *
 * Releases all memory used by this mime param back to the Operating
 * System.
 **/
static void
g_mime_param_free (GMimeParam *param)
{
	g_return_if_fail (param != NULL);
	
	g_mime_event_free (param->changed);
	g_free (param->charset);
	g_free (param->value);
	g_free (param->name);
	g_free (param->lang);
	
	g_slice_free (GMimeParam, param);
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
	g_return_val_if_fail (param != NULL, NULL);
	
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
	g_return_val_if_fail (param != NULL, NULL);
	
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
	g_return_if_fail (param != NULL);
	g_return_if_fail (value != NULL);
	
	g_free (param->value);
	param->value = g_strdup (value);
	
	g_mime_event_emit (param->changed, NULL);
}


/**
 * g_mime_param_get_charset:
 * @param: a #GMimeParam
 *
 * Gets the charset used for encoding the parameter.
 *
 * Returns: the charset used for encoding the parameter.
 **/
const char *
g_mime_param_get_charset (GMimeParam *param)
{
	g_return_val_if_fail (param != NULL, NULL);
	
	return param->charset;
}


/**
 * g_mime_param_set_charset:
 * @param: a #GMimeParam
 * @charset: the charset or %NULL to use the default
 *
 * Sets the parameter charset used for encoding the value.
 **/
void
g_mime_param_set_charset (GMimeParam *param, const char *charset)
{
	g_return_if_fail (param != NULL);
	
	g_free (param->charset);
	param->charset = charset ? g_strdup (charset) : NULL;
	
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
	g_return_val_if_fail (param != NULL, NULL);
	
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
	g_return_if_fail (param != NULL);
	
	g_free (param->lang);
	param->lang = lang ? g_strdup (lang) : NULL;
	
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
	g_return_val_if_fail (param != NULL, GMIME_PARAM_ENCODING_METHOD_DEFAULT);
	
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
	g_return_if_fail (param != NULL);
	
	param->method = method;
	
	g_mime_event_emit (param->changed, NULL);
}


static void
param_changed (GMimeParam *param, gpointer args, GMimeParamList *list)
{
	g_mime_event_emit (list->changed, NULL);
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
	GMimeParamList *list;
	
	list = g_slice_new (GMimeParamList);
	list->changed = g_mime_event_new (list);
	list->params = g_ptr_array_new ();
	
	return list;
}


/**
 * g_mime_param_list_free:
 * @list: a #GMimeParamList
 *
 * Frees the #GMimeParamList.
 **/
void
g_mime_param_list_free (GMimeParamList *list)
{
	guint i;
	
	g_return_if_fail (list != NULL);
	
	for (i = 0; i < list->params->len; i++)
		g_mime_param_free (list->params->pdata[i]);
	
	g_ptr_array_free (list->params, TRUE);
	g_mime_event_free (list->changed);
	
	g_slice_free (GMimeParamList, list);
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
	g_return_val_if_fail (list != NULL, -1);
	
	return list->params->len;
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
	guint i;
	
	g_return_if_fail (list != NULL);
	
	for (i = 0; i < list->params->len; i++)
		g_mime_param_free (list->params->pdata[i]);
	
	g_ptr_array_set_size (list->params, 0);
	
	g_mime_event_emit (list->changed, NULL);
}


/**
 * g_mime_param_list_add:
 * @list: a #GMimeParamList
 * @param: a #GMimeParam
 *
 * Adds a #GMimeParam to the #GMimeParamList.
 *
 * Returns: the index of the added #GMimeParam.
 **/
static void
g_mime_param_list_add (GMimeParamList *list, GMimeParam *param)
{
	g_mime_event_add (param->changed, (GMimeEventCallback) param_changed, list);
	g_ptr_array_add (list->params, param);
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
	
	g_return_if_fail (list != NULL);
	g_return_if_fail (name != NULL);
	g_return_if_fail (value != NULL);
	
	for (i = 0; i < list->params->len; i++) {
		param = list->params->pdata[i];
		
		if (!g_ascii_strcasecmp (param->name, name)) {
			g_mime_param_set_value (param, value);
			return;
		}
	}
	
	param = g_mime_param_new (name, value);
	g_mime_param_list_add (list, param);
	
	g_mime_event_emit (list->changed, NULL);
}


/**
 * g_mime_param_list_get_parameter:
 * @list: list: a #GMimeParamList
 * @name: the name of the parameter
 *
 * Gets the #GMimeParam with the given @name.
 *
 * Returns: the requested #GMimeParam.
 **/
GMimeParam *
g_mime_param_list_get_parameter (GMimeParamList *list, const char *name)
{
	GMimeParam *param;
	guint i;
	
	g_return_val_if_fail (list != NULL, NULL);
	g_return_val_if_fail (name != NULL, NULL);
	
	for (i = 0; i < list->params->len; i++) {
		param = list->params->pdata[i];
		
		if (!g_ascii_strcasecmp (param->name, name))
			return param;
	}
	
	return NULL;
}


/**
 * g_mime_param_list_get_parameter_at:
 * @list: a #GMimeParamList
 * @index: the index of the requested parameter
 *
 * Gets the #GMimeParam at the specified @index.
 *
 * Returns: the #GMimeParam at the specified index.
 **/
GMimeParam *
g_mime_param_list_get_parameter_at (GMimeParamList *list, int index)
{
	g_return_val_if_fail (list != NULL, NULL);
	g_return_val_if_fail (index >= 0, NULL);
	
	if ((guint) index >= list->params->len)
		return NULL;
	
	return list->params->pdata[index];
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
	GMimeParam *param;
	guint i;
	
	g_return_val_if_fail (list != NULL, FALSE);
	g_return_val_if_fail (name != NULL, FALSE);
	
	for (i = 0; i < list->params->len; i++) {
		param = list->params->pdata[i];
		
		if (!g_ascii_strcasecmp (param->name, name)) {
			g_ptr_array_remove_index (list->params, i);
			g_mime_param_free (param);
			return TRUE;
		}
	}
	
	return FALSE;
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
	
	g_return_val_if_fail (list != NULL, FALSE);
	g_return_val_if_fail (index >= 0, FALSE);
	
	if ((guint) index >= list->params->len)
		return FALSE;
	
	param = list->params->pdata[index];
	g_ptr_array_remove_index (list->params, index);
	g_mime_param_free (param);
	
	return TRUE;
}


/* FIXME: I wrote this in a quick & dirty fasion - it may not be 100% correct */
static char *
encode_param (GMimeParam *param, gboolean *encoded)
{
	register const unsigned char *inptr = (const unsigned char *) param->value;
	const unsigned char *start = inptr;
	const char *charset = NULL;
	iconv_t cd = (iconv_t) -1;
	char *outbuf = NULL;
	unsigned char c;
	char *outstr;
	GString *str;
	
	*encoded = FALSE;
	
	while (*inptr && ((inptr - start) < GMIME_FOLD_LEN)) {
		if (*inptr > 127)
			break;
		inptr++;
	}
	
	if (*inptr == '\0')
		return g_strdup (param->value);
	
	if (!param->charset) {
		if (*inptr > 127)
			charset = g_mime_charset_best (param->value, strlen (param->value));
		
		if (!charset)
			charset = "iso-8859-1";
	} else {
		charset = param->charset;
	}
	
	if (g_ascii_strcasecmp (charset, "UTF-8") != 0)
		cd = g_mime_iconv_open (charset, "UTF-8");
	
	if (cd != (iconv_t) -1) {
		outbuf = g_mime_iconv_strdup (cd, param->value);
		g_mime_iconv_close (cd);
		if (outbuf == NULL) {
			charset = "UTF-8";
			inptr = start;
		} else {
			inptr = (const unsigned char *) outbuf;
		}
	} else {
		charset = "UTF-8";
		inptr = start;
	}
	
	/* FIXME: use rfc2047 encoding if requested... */
	str = g_string_new (charset);
	g_string_append_c (str, '\'');
	if (param->lang)
		g_string_append (str, param->lang);
	g_string_append_c (str, '\'');
	
	while ((c = *inptr++)) {
		if (!is_attrchar (c))
			g_string_append_printf (str, "%%%c%c", tohex[(c >> 4) & 0xf], tohex[c & 0xf]);
		else
			g_string_append_c (str, c);
	}
	
	g_free (outbuf);
	
	outstr = g_string_free (str, FALSE);
	*encoded = TRUE;
	
	return outstr;
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


/**
 * g_mime_param_list_encode:
 * @list: a #GMimeParamList
 * @fold: %TRUE if the parameter list should be folded; otherwise, %FALSE
 * @str: the output string buffer
 *
 * Encodes the parameter list into @str, folding lines if required.
 **/
void
g_mime_param_list_encode (GMimeParamList *list, gboolean fold, GString *str)
{
	guint count, i;
	int used;
	
	g_return_if_fail (list != NULL);
	g_return_if_fail (str != NULL);
	
	count = list->params->len;
	used = str->len;
	
	for (i = 0; i < count; i++) {
		gboolean encoded = FALSE;
		int here = str->len;
		size_t nlen, vlen;
		GMimeParam *param;
		int quote = 0;
		char *value;
		
		param = (GMimeParam *) list->params->pdata[i];
		
		if (!param->value)
			continue;
		
		if (!(value = encode_param (param, &encoded))) {
			w(g_warning ("appending parameter %s=%s violates rfc2184",
				     param->name, param->value));
			value = g_strdup (param->value);
		}
		
		if (!encoded) {
			char *ch;
			
			for (ch = value; *ch; ch++) {
				if (!is_attrchar (*ch) || is_lwsp (*ch))
					quote++;
			}
		}
		
		nlen = strlen (param->name);
		vlen = strlen (value);
		
		if (fold && (used + nlen + vlen + quote > GMIME_FOLD_LEN - 2)) {
			g_string_append (str, ";\n\t");
			here = str->len;
			used = 1;
		} else {
			g_string_append (str, "; ");
			here = str->len;
			used += 2;
		}
		
		if (nlen + vlen + quote > GMIME_FOLD_LEN - 2) {
			/* we need to do special rfc2184 parameter wrapping */
			size_t maxlen = GMIME_FOLD_LEN - (nlen + 6);
			char *inptr, *inend;
			int i = 0;
			
			inptr = value;
			inend = value + vlen;
			
			while (inptr < inend) {
				char *ptr = inptr + MIN ((size_t) (inend - inptr), maxlen);
				
				if (encoded && ptr < inend) {
					/* be careful not to break an encoded char (ie %20) */
					char *q = ptr;
					int j = 2;
					
					for ( ; j > 0 && q > inptr && *q != '%'; j--, q--);
					if (*q == '%')
						ptr = q;
				}
				
				if (i != 0) {
					if (fold)
						g_string_append (str, ";\n\t");
					else
						g_string_append (str, "; ");
					
					here = str->len;
					used = 1;
				}
				
				g_string_append_printf (str, "%s*%d%s=", param->name,
							i++, encoded ? "*" : "");
				
				if (encoded || !quote)
					g_string_append_len (str, inptr, (size_t) (ptr - inptr));
				else
					g_string_append_len_quoted (str, inptr, (size_t) (ptr - inptr));
				
				used += (str->len - here);
				
				inptr = ptr;
			}
		} else {
			g_string_append_printf (str, "%s%s=", param->name, encoded ? "*" : "");
			
			if (encoded || !quote)
				g_string_append_len (str, value, vlen);
			else
				g_string_append_len_quoted (str, value, vlen);
			
			used += (str->len - here);
		}
		
		g_free (value);
	}
	
	if (fold)
		g_string_append_c (str, '\n');
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
decode_quoted_string (const char **in)
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
		if (*inptr++ == '\\') {
			unescape = TRUE;
			inptr++;
		}
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
			*outptr++ = *inptr++;
		}
		
		*outptr = '\0';
	}
	
	return out;
}

static char *
decode_token (GMimeParserOptions *options, const char **in)
{
	const char *inptr = *in;
	const char *start;
	
	skip_cfws (&inptr);
	
	start = inptr;
	if (options->parameters != GMIME_RFC_COMPLIANCE_LOOSE) {
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
decode_value (GMimeParserOptions *options, const char **in)
{
	const char *inptr = *in;
	
	skip_cfws (&inptr);
	*in = inptr;
	
	if (*inptr == '"') {
		return decode_quoted_string (in);
	} else if (is_ttoken (*inptr)) {
		return decode_token (options, in);
	}
	
	if (options->parameters == GMIME_RFC_COMPLIANCE_LOOSE)
		return decode_token (options, in);
	
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

static gboolean
decode_param (GMimeParserOptions *options, const char **in, char **namep, char **valuep, int *id, gboolean *encoded)
{
	gboolean is_rfc2184 = FALSE;
	const char *inptr = *in;
	char *name, *value = NULL;
	char *val;
	
	is_rfc2184 = decode_rfc2184_param (&inptr, &name, id, encoded);
	
	if (*inptr == '=') {
		inptr++;
		value = decode_value (options, &inptr);
		
		if (!is_rfc2184 && value) {
			if (strstr (value, "=?") != NULL) {
				/* We (may) have a broken param value that is rfc2047
				 * encoded. Since both Outlook and Netscape/Mozilla do
				 * this, we should handle this case.
				 */
				
				if ((val = g_mime_utils_header_decode_text (options, value))) {
					g_free (value);
					value = val;
				}
			}
			
			if (!g_utf8_validate (value, -1, NULL)) {
				/* A (broken) mailer has sent us an unencoded 8bit value.
				 * Attempt to save it by assuming it's in the user's
				 * locale and converting to UTF-8 */
				
				if ((val = g_mime_iconv_locale_to_utf8 (value))) {
					g_free (value);
					value = val;
				} else {
					d(g_warning ("Failed to convert %s param value (\"%s\") to UTF-8: %s",
						     name, value, g_strerror (errno)));
				}
			}
		}
	}
	
	if (name && value) {
		*valuep = value;
		*namep = name;
		*in = inptr;
		return TRUE;
	} else {
		g_free (value);
		g_free (name);
		return FALSE;
	}
}


struct _rfc2184_part {
	char *value;
	int id;
};

struct _rfc2184_param {
	struct _rfc2184_param *next;
	const char *charset;
	GMimeParam *param;
	GPtrArray *parts;
	char *lang;
};

static int
rfc2184_sort_cb (const void *v0, const void *v1)
{
	const struct _rfc2184_part *p0 = *((struct _rfc2184_part **) v0);
	const struct _rfc2184_part *p1 = *((struct _rfc2184_part **) v1);
	
	return p0->id - p1->id;
}

#define HEXVAL(c) (isdigit (c) ? (c) - '0' : tolower (c) - 'a' + 10)

static size_t
hex_decode (const char *in, size_t len, char *out)
{
	register const unsigned char *inptr = (const unsigned char *) in;
	register unsigned char *outptr = (unsigned char *) out;
	const unsigned char *inend = inptr + len;
	
	while (inptr < inend) {
		if (*inptr == '%') {
			if (isxdigit (inptr[1]) && isxdigit (inptr[2])) {
				*outptr++ = HEXVAL (inptr[1]) * 16 + HEXVAL (inptr[2]);
				inptr += 3;
			} else
				*outptr++ = *inptr++;
		} else
			*outptr++ = *inptr++;
	}
	
	*outptr = '\0';
	
	return ((char *) outptr) - out;
}

static const char *
rfc2184_param_charset (const char **in, char **langp)
{
	const char *lang, *inptr = *in;
	char *charset;
	size_t len;
	
	if (langp)
		*langp = NULL;
	
	while (*inptr && *inptr != '\'')
		inptr++;
	
	if (*inptr != '\'')
		return NULL;
	
	len = inptr - *in;
	charset = g_alloca (len + 1);
	memcpy (charset, *in, len);
	charset[len] = '\0';
	
	lang = ++inptr;
	while (*inptr && *inptr != '\'')
		inptr++;
	
	if (*inptr == '\'') {
		if (langp)
			*langp = g_strndup (lang, (size_t) (inptr - lang));
		
		inptr++;
	}
	
	*in = inptr;
	
	return g_mime_charset_canon_name (charset);
}

static char *
charset_convert (const char *charset, char *in, size_t inlen)
{
	gboolean locale = FALSE;
	char *result = NULL;
	iconv_t cd;
	
	if (!charset || !g_ascii_strcasecmp (charset, "UTF-8") || !g_ascii_strcasecmp (charset, "us-ascii")) {
		/* we shouldn't need any charset conversion here... */
		if (g_utf8_validate (in, inlen, NULL))
			return in;
		
		charset = g_mime_locale_charset ();
		locale = TRUE;
	}
	
	/* need charset conversion */
	cd = g_mime_iconv_open ("UTF-8", charset);
	if (cd == (iconv_t) -1 && !locale) {
		charset = g_mime_locale_charset ();
		cd = g_mime_iconv_open ("UTF-8", charset);
	}
	
	if (cd != (iconv_t) -1) {
		result = g_mime_iconv_strndup (cd, in, inlen);
		g_mime_iconv_close (cd);
	}
	
	if (result == NULL)
		result = in;
	else
		g_free (in);
	
	return result;
}

static char *
rfc2184_decode (const char *value)
{
	const char *inptr = value;
	const char *charset;
	char *decoded;
	size_t len;
	
	charset = rfc2184_param_charset (&inptr, NULL);
	
	len = strlen (inptr);
	decoded = g_alloca (len + 1);
	len = hex_decode (inptr, len, decoded);
	
	return charset_convert (charset, g_strdup (decoded), len);
}

static void
rfc2184_param_add_part (struct _rfc2184_param *rfc2184, char *value, int id, gboolean encoded)
{
	struct _rfc2184_part *part;
	size_t len;
	
	part = g_new (struct _rfc2184_part, 1);
	g_ptr_array_add (rfc2184->parts, part);
	part->id = id;
	
	if (encoded) {
		len = strlen (value);
		part->value = g_malloc (len + 1);
		hex_decode (value, len, part->value);
		g_free (value);
	} else {
		part->value = value;
	}
}

static struct _rfc2184_param *
rfc2184_param_new (char *name, char *value, int id, gboolean encoded)
{
	struct _rfc2184_param *rfc2184;
	const char *inptr = value;
	
	rfc2184 = g_new (struct _rfc2184_param, 1);
	rfc2184->parts = g_ptr_array_new ();
	rfc2184->next = NULL;
	
	if (encoded) {
		rfc2184->charset = rfc2184_param_charset (&inptr, &rfc2184->lang);
	} else {
		rfc2184->charset = NULL;
		rfc2184->lang = NULL;
	}
	
	if (inptr == value) {
		rfc2184_param_add_part (rfc2184, value, id, encoded);
	} else {
		rfc2184_param_add_part (rfc2184, g_strdup (inptr), id, encoded);
		g_free (value);
	}
	
	rfc2184->param = g_mime_param_new_internal ();
	rfc2184->param->name = name;
	
	return rfc2184;
}

static GMimeParamList *
decode_param_list (GMimeParserOptions *options, const char *in)
{
	struct _rfc2184_param *rfc2184, *list, *t;
	char *name, *value, *charset;
	struct _rfc2184_part *part;
	GHashTable *rfc2184_hash;
	const char *inptr = in;
	GMimeParamList *params;
	GMimeParam *param;
	gboolean encoded;
	GString *gvalue;
	guint i;
	int id;
	
	params = g_mime_param_list_new ();
	
	list = NULL;
	t = (struct _rfc2184_param *) &list;
	rfc2184_hash = g_hash_table_new (g_mime_strcase_hash, g_mime_strcase_equal);
	
	skip_cfws (&inptr);
	
	do {
		/* invalid format? */
		if (!decode_param (options, &inptr, &name, &value, &id, &encoded)) {
			skip_cfws (&inptr);
			
			if (*inptr == ';')
				continue;
			
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
				g_mime_param_list_add (params, param);
			} else {
				rfc2184_param_add_part (rfc2184, value, id, encoded);
				g_free (name);
			}
		} else {
			param = g_mime_param_new_internal ();
			param->name = name;
			
			if (encoded) {
				/* singleton encoded rfc2184 param value */
				param->value = rfc2184_decode (value);
				g_free (value);
			} else {
				/* normal parameter value */
				param->value = value;
			}
			
			g_mime_param_list_add (params, param);
		}
		
		skip_cfws (&inptr);
	} while (*inptr++ == ';');
	
	g_hash_table_destroy (rfc2184_hash);
	
	rfc2184 = list;
	while (rfc2184 != NULL) {
		t = rfc2184->next;
		
		param = rfc2184->param;
		gvalue = g_string_new ("");
		
		g_ptr_array_sort (rfc2184->parts, rfc2184_sort_cb);
		for (i = 0; i < rfc2184->parts->len; i++) {
			part = rfc2184->parts->pdata[i];
			g_string_append (gvalue, part->value);
			g_free (part->value);
			g_free (part);
		}
		
		g_ptr_array_free (rfc2184->parts, TRUE);
		
		param->value = charset_convert (rfc2184->charset, gvalue->str, gvalue->len);
		param->charset = rfc2184->charset ? g_strdup (rfc2184->charset) : NULL;
		param->lang = rfc2184->lang;
		
		g_string_free (gvalue, FALSE);
		g_free (rfc2184);
		rfc2184 = t;
	}
	
	return params;
}


/**
 * g_mime_param_list_parse:
 * @options: a #GMimeParserOptions
 * @str: a string to parse
 *
 * Parses the input string into a parameter list.
 *
 * Returns: a #GMimeParamList.
 **/
GMimeParamList *
g_mime_param_list_parse (GMimeParserOptions *options, const char *str)
{
	g_return_val_if_fail (str != NULL, NULL);
	
	return decode_param_list (options, str);
}
