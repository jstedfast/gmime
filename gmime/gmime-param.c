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

#include <string.h>
#include <limits.h>
#include <ctype.h>
#include <errno.h>

#include "gmime-param.h"
#include "gmime-common.h"
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


/**
 * g_mime_param_new:
 * @name: parameter name
 * @value: parameter value
 *
 * Creates a new #GMimeParam node with name @name and value @value.
 *
 * Returns: a new paramter structure.
 **/
GMimeParam *
g_mime_param_new (const char *name, const char *value)
{
	GMimeParam *param;
	
	param = g_new (GMimeParam, 1);
	
	param->next = NULL;
	param->name = g_strdup (name);
	param->value = g_strdup (value);
	
	return param;
}

#define INT_OVERFLOW(x,d) (((x) > (INT_MAX / 10)) || ((x) == (INT_MAX / 10) && (d) > (INT_MAX % 10)))

static int
decode_int (const char **in)
{
	const unsigned char *inptr;
	int digit, n = 0;
	
	decode_lwsp (in);
	
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
	
	decode_lwsp (&inptr);
	
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
decode_token (const char **in)
{
	const char *inptr = *in;
	const char *start;
	
	decode_lwsp (&inptr);
	
	start = inptr;
#ifdef STRICT_PARSER
	while (is_ttoken (*inptr))
		inptr++;
#else
	/* Broken mail clients like to make our lives difficult. Scan
	 * for a ';' instead of trusting that the client followed the
	 * specification. */
	while (*inptr && *inptr != ';')
		inptr++;
	
	/* Scan backwards over any trailing lwsp */
	while (inptr > start && is_lwsp (inptr[-1]))
		inptr--;
#endif
	
	if (inptr > start) {
		*in = inptr;
		return g_strndup (start, (size_t) (inptr - start));
	} else {
		return NULL;
	}
}

static char *
decode_value (const char **in)
{
	const char *inptr = *in;
	
	decode_lwsp (&inptr);
	*in = inptr;
	
	if (*inptr == '"') {
		return decode_quoted_string (in);
	} else if (is_ttoken (*inptr)) {
		return decode_token (in);
	}
	
#ifndef STRICT_PARSER
	return decode_token (in);
#else
	return NULL;
#endif
}

/* This function is basically the same as decode_token()
 * except that it will not accept *'s which have a special
 * meaning for rfc2184 params */
static char *
decode_param_token (const char **in)
{
	const char *inptr = *in;
	const char *start;
	
	decode_lwsp (&inptr);
	
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
decode_rfc2184_param (const char **in, char **paramp, int *part, gboolean *encoded)
{
	gboolean is_rfc2184 = FALSE;
	const char *inptr = *in;
	char *param;
	
	*encoded = FALSE;
	*part = -1;
	
	param = decode_param_token (&inptr);
	
	decode_lwsp (&inptr);
	
	if (*inptr == '*') {
		is_rfc2184 = TRUE;
		inptr++;
		
		decode_lwsp (&inptr);
		if (*inptr == '=') {
			/* form := param*=value */
			*encoded = TRUE;
		} else {
			/* form := param*#=value or param*#*=value */
			*part = decode_int (&inptr);
			
			decode_lwsp (&inptr);
			if (*inptr == '*') {
				/* form := param*#*=value */
				inptr++;
				*encoded = TRUE;
				decode_lwsp (&inptr);
			}
		}
	}
	
	if (paramp)
		*paramp = param;
	
	if (param)
		*in = inptr;
	
	return is_rfc2184;
}

static gboolean
decode_param (const char **in, char **paramp, char **valuep, int *id, gboolean *encoded)
{
	gboolean is_rfc2184 = FALSE;
	const char *inptr = *in;
	char *param, *value = NULL;
	char *val;
	
	is_rfc2184 = decode_rfc2184_param (&inptr, &param, id, encoded);
	
	if (*inptr == '=') {
		inptr++;
		value = decode_value (&inptr);
		
		if (!is_rfc2184 && value) {
			if (strstr (value, "=?") != NULL) {
				/* We (may) have a broken param value that is rfc2047
				 * encoded. Since both Outlook and Netscape/Mozilla do
				 * this, we should handle this case.
				 */
				
				if ((val = g_mime_utils_header_decode_text (value))) {
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
						     param, value, g_strerror (errno)));
				}
			}
		}
	}
	
	if (param && value) {
		*paramp = param;
		*valuep = value;
		*in = inptr;
		return TRUE;
	} else {
		g_free (param);
		g_free (value);
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
	
	rfc2184->param = g_new (GMimeParam, 1);
	rfc2184->param->next = NULL;
	rfc2184->param->name = name;
	rfc2184->param->value = NULL;
	
	return rfc2184;
}

static GMimeParam *
decode_param_list (const char *in)
{
	struct _rfc2184_param *rfc2184, *list, *t;
	GMimeParam *param, *params, *tail;
	struct _rfc2184_part *part;
	GHashTable *rfc2184_hash;
	const char *inptr = in;
	char *name, *value;
	gboolean encoded;
	GString *gvalue;
	guint i;
	int id;
	
	params = NULL;
	tail = (GMimeParam *) &params;
	
	list = NULL;
	t = (struct _rfc2184_param *) &list;
	rfc2184_hash = g_hash_table_new (g_mime_strcase_hash, g_mime_strcase_equal);
	
	decode_lwsp (&inptr);
	
	do {
		/* invalid format? */
		if (!decode_param (&inptr, &name, &value, &id, &encoded)) {
			decode_lwsp (&inptr);
			
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
				
				tail->next = param;
				tail = param;
			} else {
				rfc2184_param_add_part (rfc2184, value, id, encoded);
				g_free (name);
			}
		} else {
			param = g_new (GMimeParam, 1);
			param->next = NULL;
			param->name = name;
			
			if (encoded) {
				/* singleton encoded rfc2184 param value */
				param->value = rfc2184_decode (value);
				g_free (value);
			} else {
				/* normal parameter value */
				param->value = value;
			}
			
			tail->next = param;
			tail = param;
		}
		
		decode_lwsp (&inptr);
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
		g_string_free (gvalue, FALSE);
		
		g_free (rfc2184->lang);
		g_free (rfc2184);
		rfc2184 = t;
	}
	
	return params;
}


/**
 * g_mime_param_new_from_string:
 * @str: input string
 *
 * Creates a parameter list based on the input string.
 *
 * Returns: a #GMimeParam structure based on @string.
 **/
GMimeParam *
g_mime_param_new_from_string (const char *str)
{
	g_return_val_if_fail (str != NULL, NULL);
	
	return decode_param_list (str);
}


/**
 * g_mime_param_destroy:
 * @param: Mime param list to destroy
 *
 * Releases all memory used by this mime param back to the Operating
 * System.
 **/
void
g_mime_param_destroy (GMimeParam *param)
{
	GMimeParam *next;
	
	while (param) {
		next = param->next;
		g_free (param->name);
		g_free (param->value);
		g_free (param);
		param = next;
	}
}


/**
 * g_mime_param_next:
 * @param: a #GMimeParam node
 *
 * Gets the next #GMimeParam node in the list.
 *
 * Returns: the next #GMimeParam node in the list.
 **/
const GMimeParam *
g_mime_param_next (const GMimeParam *param)
{
	g_return_val_if_fail (param != NULL, NULL);
	
	return param->next;
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
g_mime_param_get_name (const GMimeParam *param)
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
g_mime_param_get_value (const GMimeParam *param)
{
	g_return_val_if_fail (param != NULL, NULL);
	
	return param->value;
}


/**
 * g_mime_param_append:
 * @params: param list
 * @name: new param name
 * @value: new param value
 *
 * Appends a new parameter with name @name and value @value to the
 * parameter list @params.
 *
 * Returns: a param list with the new param of name @name and value
 * @value appended to the list of params @params.
 **/
GMimeParam *
g_mime_param_append (GMimeParam *params, const char *name, const char *value)
{
	GMimeParam *param, *p;
	
	g_return_val_if_fail (name != NULL, params);
	g_return_val_if_fail (value != NULL, params);
	
	param = g_mime_param_new (name, value);
	if (params) {
		p = params;
		while (p->next)
			p = p->next;
		p->next = param;
	} else
		params = param;
	
	return params;
}


/**
 * g_mime_param_append_param:
 * @params: param list
 * @param: param to append
 *
 * Appends @param to the param list @params.
 *
 * Returns: a param list with the new param @param appended to the list
 * of params @params.
 **/
GMimeParam *
g_mime_param_append_param (GMimeParam *params, GMimeParam *param)
{
	GMimeParam *p;
	
	g_return_val_if_fail (param != NULL, params);
	
	if (params) {
		p = params;
		while (p->next)
			p = p->next;
		p->next = param;
	} else
		params = param;
	
	return params;
}

/* FIXME: I wrote this in a quick & dirty fasion - it may not be 100% correct */
static char *
encode_param (const char *in, gboolean *encoded)
{
	register const unsigned char *inptr = (const unsigned char *) in;
	const unsigned char *instart = inptr;
	iconv_t cd = (iconv_t) -1;
	const char *charset = NULL;
	char *outbuf = NULL;
	unsigned char c;
	char *outstr;
	GString *out;
	
	*encoded = FALSE;
	
	while (*inptr && ((inptr - instart) < GMIME_FOLD_LEN)) {
		if (*inptr > 127)
			break;
		inptr++;
	}
	
	if (*inptr == '\0')
		return g_strdup (in);
	
	if (*inptr > 127)
		charset = g_mime_charset_best (in, strlen (in));
	
	if (!charset)
		charset = "iso-8859-1";
	
	if (g_ascii_strcasecmp (charset, "UTF-8") != 0)
		cd = g_mime_iconv_open (charset, "UTF-8");
	
	if (cd != (iconv_t) -1) {
		outbuf = g_mime_iconv_strdup (cd, in);
		g_mime_iconv_close (cd);
		if (outbuf == NULL) {
			charset = "UTF-8";
			inptr = instart;
		} else {
			inptr = (const unsigned char *) outbuf;
		}
	} else {
		charset = "UTF-8";
		inptr = instart;
	}
	
	/* FIXME: set the 'language' as well, assuming we can get that info...? */
	out = g_string_new ("");
	g_string_append_printf (out, "%s''", charset);
	
	while ((c = *inptr++)) {
		if (!is_attrchar (c))
			g_string_append_printf (out, "%%%c%c", tohex[(c >> 4) & 0xf], tohex[c & 0xf]);
		else
			g_string_append_c (out, c);
	}
	
	g_free (outbuf);
	
	outstr = out->str;
	g_string_free (out, FALSE);
	*encoded = TRUE;
	
	return outstr;
}

static void
g_string_append_len_quoted (GString *out, const char *in, size_t len)
{
	register const char *inptr;
	const char *inend;
	
	g_string_append_c (out, '"');
	
	inptr = in;
	inend = in + len;
	
	while (inptr < inend) {
		if ((*inptr == '"') || *inptr == '\\')
			g_string_append_c (out, '\\');
		
		g_string_append_c (out, *inptr);
		
		inptr++;
	}
	
	g_string_append_c (out, '"');
}

static void
param_list_format (GString *out, const GMimeParam *param, gboolean fold)
{
	int used = out->len;
	
	while (param) {
		gboolean encoded = FALSE;
		int here = out->len;
		size_t nlen, vlen;
		int quote = 0;
		char *value;
		
		if (!param->value) {
			param = param->next;
			continue;
		}
		
		if (!(value = encode_param (param->value, &encoded))) {
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
			g_string_append (out, ";\n\t");
			here = out->len;
			used = 1;
		} else {
			g_string_append (out, "; ");
			here = out->len;
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
						g_string_append (out, ";\n\t");
					else
						g_string_append (out, "; ");
					
					here = out->len;
					used = 1;
				}
				
				g_string_append_printf (out, "%s*%d%s=", param->name,
							i++, encoded ? "*" : "");
				
				if (encoded || !quote)
					g_string_append_len (out, inptr, (size_t) (ptr - inptr));
				else
					g_string_append_len_quoted (out, inptr, (size_t) (ptr - inptr));
				
				used += (out->len - here);
				
				inptr = ptr;
			}
		} else {
			g_string_append_printf (out, "%s%s=", param->name, encoded ? "*" : "");
			
			if (encoded || !quote)
				g_string_append_len (out, value, vlen);
			else
				g_string_append_len_quoted (out, value, vlen);
			
			used += (out->len - here);
		}
		
		g_free (value);
		
		param = param->next;
	}
	
	if (fold)
		g_string_append_c (out, '\n');
}


/**
 * g_mime_param_write_to_string:
 * @param: MIME Param list
 * @fold: specifies whether or not to fold headers
 * @str: output string
 *
 * Assumes the output string contains only the Content-* header and
 * it's immediate value.
 *
 * Writes the params out to the string @string.
 **/
void
g_mime_param_write_to_string (const GMimeParam *param, gboolean fold, GString *str)
{
	g_return_if_fail (str != NULL);
	
	param_list_format (str, param, fold);
}
