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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <string.h>
#include <ctype.h>
#include <errno.h>

#ifdef HAVE_ALLOCA_H
#include <alloca.h>
#endif

#include "gmime-param.h"
#include "gmime-table-private.h"
#include "gmime-charset.h"
#include "gmime-utils.h"
#include "gmime-iconv.h"
#include "gmime-iconv-utils.h"

#include "strlib.h"

#define d(x)
#define w(x)

extern int gmime_interfaces_utf8;

static unsigned char tohex[16] = {
	'0', '1', '2', '3', '4', '5', '6', '7',
	'8', '9', 'A', 'B', 'C', 'D', 'E', 'F'
};


/**
 * g_mime_param_new: Create a new MIME Param object
 * @name: parameter name
 * @value: parameter value
 *
 * Creates a new GMimeParam node with name @name and value @value.
 *
 * Returns a new paramter structure.
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


#define HEXVAL(c) (isdigit (c) ? (c) - '0' : tolower (c) - 'a' + 10)

static size_t
hex_decode (const unsigned char *in, size_t len, unsigned char *out)
{
	register const unsigned char *inptr;
	register unsigned char *outptr;
	const unsigned char *inend;
	
	inptr = in;
	inend = in + len;
	
	outptr = out;
	
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
	
	return outptr - out;
}

/* an rfc2184 encoded string looks something like:
 * us-ascii'en'This%20is%20even%20more%20
 */
static char *
rfc2184_decode (const char *in, size_t len)
{
	const char *inptr = in;
	const char *inend = in + len;
	const char *charset = NULL;
	char *decoded = NULL;
	char *charenc;
	
	/* skips to the end of the charset / beginning of the locale */
	inptr = memchr (inptr, '\'', len);
	if (!inptr)
		return NULL;
	
	/* save the charset */
	len = inptr - in;
	charenc = alloca (len + 1);
	memcpy (charenc, in, len);
	charenc[len] = '\0';
	charset = charenc;
	
	/* skip to the end of the locale */
	inptr = memchr (inptr + 1, '\'', (unsigned int) (inend - inptr - 1));
	if (!inptr)
		return NULL;
	
	inptr++;
	if (inptr < inend) {
		len = inend - inptr;
		if (gmime_interfaces_utf8 && strcasecmp (charset, "UTF-8") != 0) {
			char *udecoded;
			iconv_t cd;
			
			decoded = alloca (len + 1);
			len = hex_decode (inptr, len, decoded);
			
			cd = g_mime_iconv_open ("UTF-8", charset);
			if (cd == (iconv_t) -1) {
				d(g_warning ("Cannot convert from %s to UTF-8, param display may "
					     "be corrupt: %s", charset, g_strerror (errno)));
				charset = g_mime_charset_locale_name ();
				cd = g_mime_iconv_open ("UTF-8", charset);
				if (cd == (iconv_t) -1)
					return NULL;
			}
			
			udecoded = g_mime_iconv_strndup (cd, decoded, len);
			g_mime_iconv_close (cd);
			
			if (!udecoded) {
				d(g_warning ("Failed to convert \"%.*s\" to UTF-8, display may be "
					     "corrupt: %s", len, decoded, g_strerror (errno)));
			}
			
			decoded = udecoded;
		} else {
			decoded = g_malloc (len + 1);
			hex_decode (inptr, len, decoded);
		}
	}
	
	return decoded;
}

static void
decode_lwsp (const char **in)
{
	const char *inptr = *in;
	
	while (*inptr && (*inptr == '(' || is_lwsp (*inptr))) {
		while (*inptr && is_lwsp (*inptr))
			inptr++;
		
		/* skip over any comments */
		if (*inptr == '(') {
			int depth = 1;
			
			inptr++;
			while (*inptr && depth) {
				if (*inptr == '\\' && *(inptr + 1))
					inptr++;
				else if (*inptr == '(')
					depth++;
				else if (*inptr == ')')
					depth--;
				
				inptr++;
			}
		}
	}
	
	*in = inptr;
}

static int
decode_int (const char **in)
{
	const char *inptr = *in;
	int n = 0;
	
	decode_lwsp (&inptr);
	
	while (isdigit ((int) *inptr)) {
		n = n * 10 + (*inptr - '0');
		inptr++;
	}
	
	*in = inptr;
	
	return n;
}

static char *
decode_quoted_string (const char **in)
{
	const char *start, *inptr = *in;
	char *out = NULL;
	
	decode_lwsp (&inptr);
	if (*inptr == '"') {
		start = inptr++;
		
		while (*inptr && *inptr != '"') {
			if (*inptr++ == '\\')
				inptr++;
		}
		
		if (*inptr == '"') {
			start++;
			out = g_strndup (start, (unsigned int) (inptr - start));
			inptr++;
		} else {
			/* string wasn't properly quoted */
			out = g_strndup (start, (unsigned int) (inptr - start));
		}
	}
	
	*in = inptr;
	
	return out;
}

static char *
decode_token (const char **in)
{
	const char *inptr = *in;
	const char *start;
	
	decode_lwsp (&inptr);
	
	start = inptr;
	while (is_ttoken (*inptr))
		inptr++;
	if (inptr > start) {
		*in = inptr;
		return g_strndup (start, (unsigned int) (inptr - start));
	} else {
		return NULL;
	}
}

static char *
decode_value (const char **in)
{
	const char *inptr = *in;
	
	decode_lwsp (&inptr);
	
	if (*inptr == '"') {
		return decode_quoted_string (in);
	} else if (is_ttoken (*inptr)) {
		return decode_token (in);
	}
	
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
	
	decode_lwsp (&inptr);
	
	start = inptr;
	while (is_ttoken (*inptr) && *inptr != '*')
		inptr++;
	if (inptr > start) {
		*in = inptr;
		return g_strndup (start, (unsigned int) (inptr - start));
	} else {
		return NULL;
	}
}

static gboolean
decode_rfc2184_param (const char **in, char **paramp, int *part, gboolean *value_is_encoded)
{
	gboolean is_rfc2184 = FALSE;
	const char *inptr = *in;
	char *param;
	
	*value_is_encoded = FALSE;
	*part = -1;
	
	param = decode_param_token (&inptr);
	
	decode_lwsp (&inptr);
	
	if (*inptr == '*') {
		is_rfc2184 = TRUE;
		inptr++;
		
		decode_lwsp (&inptr);
		if (*inptr == '=') {
			/* form := param*=value */
			if (value_is_encoded)
				*value_is_encoded = TRUE;
		} else {
			/* form := param*#=value or param*#*=value */
			*part = decode_int (&inptr);
			
			decode_lwsp (&inptr);
			if (*inptr == '*') {
				/* form := param*#*=value */
				if (value_is_encoded)
					*value_is_encoded = TRUE;
				inptr++;
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

static int
decode_param (const char **in, char **paramp, char **valuep, gboolean *is_rfc2184_param)
{
	gboolean is_rfc2184_encoded = FALSE;
	gboolean is_rfc2184 = FALSE;
	gboolean valid_utf8 = FALSE;
	const char *inptr = *in;
	char *param, *value = NULL;
	int rfc2184_part = -1;
	
	*is_rfc2184_param = FALSE;
	
	is_rfc2184 = decode_rfc2184_param (&inptr, &param, &rfc2184_part,
					   &is_rfc2184_encoded);
	
	if (*inptr == '=') {
		inptr++;
		value = decode_value (&inptr);
		
		if (is_rfc2184) {
			/* We have ourselves an rfc2184 parameter */
			if (rfc2184_part == -1) {
				/* rfc2184 allows the value to be broken into
				 * multiple parts - this isn't one of them so
				 * it is safe to decode it.
				 */
				char *val;
				
				val = rfc2184_decode (value, strlen (value));
				if (val) {
					valid_utf8 = TRUE;
					g_free (value);
					value = val;
				}
			} else {
				/* Since we are expecting to find the rest of
				 * this paramter value later, let our caller know.
				 */
				*is_rfc2184_param = TRUE;
			}
		} else if (value && !strncmp (value, "=?", 2)) {
			/* We have a broken param value that is rfc2047 encoded.
			 * Since both Outlook and Netscape/Mozilla do this, we
			 * should handle this case.
			 */
			char *val;
			
			val = g_mime_utils_8bit_header_decode (value);
			if (val) {
				valid_utf8 = TRUE;
				g_free (value);
				value = val;
			}
		} else {
			if (gmime_interfaces_utf8)
				valid_utf8 = !g_mime_utils_text_is_8bit (value, strlen (value));
		}
	}
	
	if (gmime_interfaces_utf8 && value && !valid_utf8) {
		/* A (broken) mailer has sent us an unencoded 8bit value.
		 * Attempt to save it by assuming it's in the user's
		 * locale and converting to UTF-8 */
		char *buf;
		
		buf = g_mime_iconv_locale_to_utf8 (value);
		if (buf) {
			g_free (value);
			value = buf;
		} else {
			d(g_warning ("Failed to convert %s param value (\"%s\") to UTF-8: %s",
				     param, value, g_strerror (errno)));
		}
	}
	
	if (param && value) {
		*paramp = param;
		*valuep = value;
		*in = inptr;
		return 0;
	} else {
		g_free (param);
		g_free (value);
		return 1;
	}
}

static GMimeParam *
decode_param_list (const char **in)
{
	const char *inptr = *in;
	GMimeParam *head = NULL, *tail = NULL;
	gboolean last_was_rfc2184 = FALSE;
	gboolean is_rfc2184 = FALSE;
	
	decode_lwsp (&inptr);
	
	do {
		GMimeParam *param = NULL;
		char *name, *value;
		
		/* invalid format? */
		if (decode_param (&inptr, &name, &value, &is_rfc2184) != 0) {
			if (*inptr == ';') {
				continue;
			}
			break;
		}
		
		if (is_rfc2184 && tail && !strcasecmp (name, tail->name)) {
			/* rfc2184 allows a parameter to be broken into multiple parts
			 * and it looks like we've found one. Append this value to the
			 * last value.
			 */
			GString *gvalue;
			
			gvalue = g_string_new (tail->value);
			g_string_append (gvalue, value);
			g_free (tail->value);
			g_free (value);
			g_free (name);
			
			tail->value = gvalue->str;
			g_string_free (gvalue, FALSE);
		} else {
			if (last_was_rfc2184) {
				/* We've finished gathering the values for the last param
				 * so it is now safe to decode it.
				 */
				char *val;
				
				val = rfc2184_decode (tail->value, strlen (tail->value));
				if (val) {
					g_free (tail->value);
					tail->value = val;
				}
			}
			
			param = g_new (GMimeParam, 1);
			param->next = NULL;
			param->name = name;
			param->value = value;
			
			if (head == NULL)
				head = param;
			if (tail)
				tail->next = param;
			tail = param;
		}
		
		last_was_rfc2184 = is_rfc2184;
		
		decode_lwsp (&inptr);
	} while (*inptr++ == ';');
	
	if (last_was_rfc2184) {
		/* We've finished gathering the values for the last param
		 * so it is now safe to decode it.
		 */
		char *val;
		
		val = rfc2184_decode (tail->value, strlen (tail->value));
		if (val) {
			g_free (tail->value);
			tail->value = val;
		}
	}
	
	*in = inptr;
	
	return head;
}


/**
 * g_mime_param_new_from_string: Create a new MIME Param object
 * @string: input string
 *
 * Creates a parameter list based on the input string.
 *
 * Returns a GMimeParam structure based on @string.
 **/
GMimeParam *
g_mime_param_new_from_string (const char *string)
{
	g_return_val_if_fail (string != NULL, NULL);
	
	return decode_param_list (&string);
}


/**
 * g_mime_param_destroy: Destroy the MIME Param
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
 * g_mime_param_append:
 * @params: param list
 * @name: new param name
 * @value: new param value
 *
 * Appends a new parameter with name @name and value @value to the
 * parameter list @params.
 *
 * Returns a param list with the new param of name @name and value
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
 * Returns a param list with the new param @param appended to the list
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
encode_param (const unsigned char *in, gboolean *encoded)
{
	register const unsigned char *inptr;
	unsigned char *outbuf = NULL;
	iconv_t cd = (iconv_t) -1;
	const char *charset = NULL;
	char *outstr;
	GString *out;
	
	*encoded = FALSE;
	
	for (inptr = in; *inptr && inptr - in < GMIME_FOLD_LEN; inptr++)
		if (*inptr > 127)
			break;
	
	if (*inptr == '\0')
		return g_strdup (in);
	
	if (*inptr > 127) {
		if (gmime_interfaces_utf8)
			charset = g_mime_charset_best (in, strlen (in));
		else
			charset = g_mime_charset_locale_name ();
	}
	
	if (!charset)
		charset = "iso-8859-1";
	
	if (gmime_interfaces_utf8) {
		if (strcasecmp (charset, "UTF-8") != 0)
			cd = g_mime_iconv_open (charset, "UTF-8");
		
		if (cd == (iconv_t) -1)
			charset = "UTF-8";
	}
	
	if (cd != (iconv_t) -1) {
		outbuf = g_mime_iconv_strdup (cd, in);
		g_mime_iconv_close (cd);
		inptr = outbuf;
	} else {
		inptr = in;
	}
	
	/* FIXME: set the 'language' as well, assuming we can get that info...? */
	out = g_string_new ("");
	g_string_sprintfa (out, "%s''", charset);
	
	while (inptr && *inptr) {
		unsigned char c = *inptr++;
		
		/* FIXME: make sure that '\'', '*', and ';' are also encoded */
		
		if (c > 127) {
			g_string_sprintfa (out, "%%%c%c", tohex[(c >> 4) & 0xf], tohex[c & 0xf]);
		} else if (is_lwsp (c) || !(gmime_special_table[c] & IS_ESAFE)) {
			g_string_sprintfa (out, "%%%c%c", tohex[(c >> 4) & 0xf], tohex[c & 0xf]);
		} else {
			g_string_append_c (out, c);
		}
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
g_string_append_len (GString *out, const char *in, size_t len)
{
	char *buf;
	
	buf = alloca (len + 1);
	strlcpy (buf, in, len + 1);
	
	g_string_append (out, buf);
}

static void
param_list_format (GString *out, GMimeParam *param, gboolean fold)
{
	int used = out->len;
	
	while (param) {
		gboolean encoded = FALSE;
		gboolean quote = FALSE;
		unsigned nlen, vlen;
		int here = out->len;
		char *value;
		
		if (!param->value) {
			param = param->next;
			continue;
		}
		
		value = encode_param (param->value, &encoded);
		if (!value) {
			w(g_warning ("appending parameter %s=%s violates rfc2184",
				     param->name, param->value));
			value = g_strdup (param->value);
		}
		
		if (!encoded) {
			char *ch;
			
			for (ch = value; *ch; ch++) {
				if (is_tspecial (*ch) || is_lwsp (*ch))
					break;
			}
			
			quote = ch && *ch;
		}
		
		nlen = strlen (param->name);
		vlen = strlen (value);
		
		if (used + nlen + vlen > GMIME_FOLD_LEN - 8) {
			if (fold)
				g_string_append (out, ";\n\t");
			else
				g_string_append (out, "; ");
			
			here = out->len;
			used = 0;
		} else
			out = g_string_append (out, "; ");
		
		if (nlen + vlen > GMIME_FOLD_LEN - 10) {
			/* we need to do special rfc2184 parameter wrapping */
			int maxlen = GMIME_FOLD_LEN - (nlen + 10);
			char *inptr, *inend;
			int i = 0;
			
			inptr = value;
			inend = value + vlen;
			
			while (inptr < inend) {
				char *ptr = inptr + MIN (inend - inptr, maxlen);
				
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
					used = 0;
				}
				
				g_string_sprintfa (out, "%s*%d%s=", param->name, i++, encoded ? "*" : "");
				if (encoded || !quote)
					g_string_append_len (out, inptr, (unsigned) (ptr - inptr));
				else
					g_string_append_len_quoted (out, inptr, (unsigned) (ptr - inptr));
				
				d(printf ("wrote: %s\n", out->str + here));
				
				used += (out->len - here);
				
				inptr = ptr;
			}
		} else {
			g_string_sprintfa (out, "%s%s=", param->name, encoded ? "*" : "");
			
			if (encoded || !quote)
				g_string_append_len (out, value, vlen);
			else
				g_string_append_len_quoted (out, value, vlen);
			
			used += (out->len - here);
		}
		
		g_free (value);
		
		param = param->next;
	}
}


/**
 * g_mime_param_write_to_string:
 * @param: MIME Param list
 * @fold: specifies whether or not to fold headers
 * @string: output string
 *
 * Assumes the output string contains only the Content-* header and
 * it's immediate value.
 *
 * Writes the params out to the string @string.
 **/
void
g_mime_param_write_to_string (GMimeParam *param, gboolean fold, GString *string)
{
	g_return_if_fail (string != NULL);
	
	param_list_format (string, param, fold);
}
