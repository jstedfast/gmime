/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 *  Authors: Michael Zucchi <notzed@helixcode.com>
 *           Jeffrey Stedfast <fejj@helixcode.com>
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

#include "gmime-utils.h"
#include "gmime-part.h"
#include <config.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#define d(x)

#define	GMIME_UUDECODE_CHAR(c) (((c) - ' ') & 077)

static char *base64_alphabet =
"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

static unsigned char tohex[16] = {
	'0', '1', '2', '3', '4', '5', '6', '7',
	'8', '9', 'A', 'B', 'C', 'D', 'E', 'F'
};

static unsigned short gmime_special_table[256] = {
	  5,  5,  5,  5,  5,  5,  5,  5,  5,231,  7,  5,  5, 39,  5,  5,
	  5,  5,  5,  5,  5,  5,  5,  5,  5,  5,  5,  5,  5,  5,  5,  5,
	242,448, 76,192,192,192,192,192, 76, 76,448,448, 76,448, 72,324,
	448,448,448,448,448,448,448,448,448,448, 76, 76, 76,  4, 76, 68,
	 76,448,448,448,448,448,448,448,448,448,448,448,448,448,448,448,
	448,448,448,448,448,448,448,448,448,448,448,108,236,108,192, 64,
	192,448,448,448,448,448,448,448,448,448,448,448,448,448,448,448,
	448,448,448,448,448,448,448,448,448,448,448,192,192,192,192,  5,
	  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
	  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
	  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
	  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
	  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
	  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
	  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
	  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
};

static unsigned char gmime_base64_rank[256] = {
	255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
	255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
	255,255,255,255,255,255,255,255,255,255,255, 62,255,255,255, 63,
	 52, 53, 54, 55, 56, 57, 58, 59, 60, 61,255,255,255,  0,255,255,
	255,  0,  1,  2,  3,  4,  5,  6,  7,  8,  9, 10, 11, 12, 13, 14,
	 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25,255,255,255,255,255,
	255, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40,
	 41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51,255,255,255,255,255,
	255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
	255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
	255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
	255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
	255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
	255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
	255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
	255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
};

static unsigned char gmime_uu_rank[256] = {
	 32, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47,
	 48, 49, 50, 51, 52, 53, 54, 55, 56, 57, 58, 59, 60, 61, 62, 63,
	  0,  1,  2,  3,  4,  5,  6,  7,  8,  9, 10, 11, 12, 13, 14, 15,
	 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31,
	 32, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47,
	 48, 49, 50, 51, 52, 53, 54, 55, 56, 57, 58, 59, 60, 61, 62, 63,
	  0,  1,  2,  3,  4,  5,  6,  7,  8,  9, 10, 11, 12, 13, 14, 15,
	 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31,
	 32, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47,
	 48, 49, 50, 51, 52, 53, 54, 55, 56, 57, 58, 59, 60, 61, 62, 63,
	  0,  1,  2,  3,  4,  5,  6,  7,  8,  9, 10, 11, 12, 13, 14, 15,
	 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31,
	 32, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47,
	 48, 49, 50, 51, 52, 53, 54, 55, 56, 57, 58, 59, 60, 61, 62, 63,
	  0,  1,  2,  3,  4,  5,  6,  7,  8,  9, 10, 11, 12, 13, 14, 15,
	 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31,
};


enum {
	IS_CTRL		= 1<<0,
	IS_LWSP		= 1<<1,
	IS_TSPECIAL	= 1<<2,
	IS_SPECIAL	= 1<<3,
	IS_SPACE	= 1<<4,
	IS_DSPECIAL	= 1<<5,
	IS_QPSAFE	= 1<<6,
	IS_ESAFE	= 1<<7,	/* encoded word safe */
	IS_PSAFE	= 1<<8,	/* encoded word in phrase safe */
};

#define is_ctrl(x) ((gmime_special_table[(unsigned char)(x)] & IS_CTRL) != 0)
#define is_lwsp(x) ((gmime_special_table[(unsigned char)(x)] & IS_LWSP) != 0)
#define is_tspecial(x) ((gmime_special_table[(unsigned char)(x)] & IS_TSPECIAL) != 0)
#define is_type(x, t) ((gmime_special_table[(unsigned char)(x)] & (t)) != 0)
#define is_ttoken(x) ((gmime_special_table[(unsigned char)(x)] & (IS_TSPECIAL|IS_LWSP|IS_CTRL)) == 0)
#define is_atom(x) ((gmime_special_table[(unsigned char)(x)] & (IS_SPECIAL|IS_SPACE|IS_CTRL)) == 0)
#define is_dtext(x) ((gmime_special_table[(unsigned char)(x)] & IS_DSPECIAL) == 0)
#define is_fieldname(x) ((gmime_special_table[(unsigned char)(x)] & (IS_CTRL|IS_SPACE)) == 0)
#define is_qpsafe(x) ((gmime_special_table[(unsigned char)(x)] & IS_QPSAFE) != 0)
#define is_especial(x) ((gmime_special_table[(unsigned char)(x)] & IS_ESPECIAL) != 0)
#define is_psafe(x) ((gmime_special_table[(unsigned char)(x)] & IS_PSAFE) != 0)

#ifndef HAVE_ISBLANK
#define isblank(c) ((c) == ' ' || (c) == '\t')
#endif /* HAVE_ISBLANK */

#define CHARS_LWSP " \t\n\r"               /* linear whitespace chars */
#define CHARS_TSPECIAL "()<>@,;:\\\"/[]?="
#define CHARS_SPECIAL "()<>@,;:\\\".[]"
#define CHARS_CSPECIAL "()\\\r"	           /* not in comments */
#define CHARS_DSPECIAL "[]\\\r \t"	   /* not in domains */
#define CHARS_ESPECIAL "()<>@,;:\"/[]?.="  /* encoded word specials (rfc2047 5.1) */
#define CHARS_PSPECIAL "!*+-/"             /* encoded phrase specials (rfc2047 5.3) */

#ifdef BUILD_TABLE
/* code to rebuild the gmime_special_table */
static void
header_remove_bits (gushort bit, guchar *vals)
{
	gint i;
	
	for (i = 0; vals[i]; i++)
		gmime_special_table[vals[i]] &= ~bit;
}

static void
header_init_bits (gushort bit, gushort bitcopy, gboolean remove, guchar *vals)
{
	gint i, len = strlen (vals);
	
	if (!remove) {
		for (i = 0; i < len; i++) {
			gmime_special_table[vals[i]] |= bit;
		}
		if (bitcopy) {
			for (i = 0; i < 256; i++) {
				if (gmime_special_table[i] & bitcopy)
					gmime_special_table[i] |= bit;
			}
		}
	} else {
		for (i = 0; i < 256; i++)
			gmime_special_table[i] |= bit;
		for (i = 0; i < len; i++) {
			gmime_special_table[vals[i]] &= ~bit;
		}
		if (bitcopy) {
			for (i = 0; i < 256; i++) {
				if (gmime_special_table[i] & bitcopy)
					gmime_special_table[i] &= ~bit;
			}
		}
	}
}

static void
header_decode_init (void)
{
	gint i;
	
	for (i = 0; i < 256; i++) {
		gmime_special_table[i] = 0;
		if (i < 32)
			gmime_special_table[i] |= IS_CTRL;
		if ((i >= 33 && i <= 60) || (i >= 62 && i <= 126) || i == 32 || i == 9)
			gmime_special_table[i] |= (IS_QPSAFE | IS_ESAFE);
		if ((i >= '0' && i <= '9') || (i >= 'a' && i <= 'z') || (i >= 'A' && i <= 'Z'))
			gmime_special_table[i] |= IS_PSAFE;
	}
	
	gmime_special_table[127] |= IS_CTRL;
	gmime_special_table[' '] |= IS_SPACE;
	header_init_bits (IS_LWSP, 0, FALSE, CHARS_LWSP);
	header_init_bits (IS_TSPECIAL, IS_CTRL, FALSE, CHARS_TSPECIAL);
	header_init_bits (IS_SPECIAL, 0, FALSE, CHARS_SPECIAL);
	header_init_bits (IS_DSPECIAL, 0, FALSE, CHARS_DSPECIAL);
	header_remove_bits (IS_ESAFE, CHARS_ESPECIAL);
	header_init_bits (IS_PSAFE, 0, FALSE, CHARS_PSPECIAL);
}
#endif /* BUILD_TABLE */

/* hrm, is there a library for this shit? */
static struct {
	char *name;
	int offset;
} tz_offsets [] = {
	{ "UT", 0 },
	{ "GMT", 0 },
	{ "EST", -500 },	/* these are all US timezones.  bloody yanks */
	{ "EDT", -400 },
	{ "CST", -600 },
	{ "CDT", -500 },
	{ "MST", -700 },
	{ "MDT", -600 },
	{ "PST", -800 },
	{ "PDT", -700 },
	{ "Z", 0 },
	{ "A", -100 },
	{ "M", -1200 },
	{ "N", 100 },
	{ "Y", 1200 },
};

static char *tm_months[] = {
	"Jan", "Feb", "Mar", "Apr", "May", "Jun",
	"Jul", "Aug", "Sep", "Oct", "Nov", "Dec"
};

static char *tm_days[] = {
	"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"
};


/**
 * g_mime_utils_header_format_date:
 * @time: time_t date representation
 * @offset: Timezone offset
 *
 * Returns a valid string representation of the date.
 **/
gchar *
g_mime_utils_header_format_date (time_t time, gint offset)
{
	struct tm tm;
	
	time += ((offset / 100) * (60 * 60)) + (offset % 100) * 60;
	
	memcpy (&tm, gmtime (&time), sizeof (tm));
	
	return g_strdup_printf ("%s, %02d %s %04d %02d:%02d:%02d %+05d",
				tm_days[tm.tm_wday], tm.tm_mday,
				tm_months[tm.tm_mon],
				tm.tm_year + 1900,
				tm.tm_hour, tm.tm_min, tm.tm_sec,
				offset);
}

/* This is where it gets ugly... */

static GList *
datetok (const gchar *date)
{
	GList *tokens = NULL;
	gchar *token, *start, *end;
	
	g_return_val_if_fail (date != NULL, NULL);
	
	start = (gchar *) date;
	while (*start) {
		/* kill leading whitespace */
		for ( ; *start && isspace (*start); start++);
		
		/* find the end of this token */
		for (end = start; *end && !isspace (*end); end++);
		
		token = g_strndup (start, (end - start));
		
		if (token && *token)
			tokens = g_list_append (tokens, token);
		else
			g_free (token);
		
		if (*end)
			start = end + 1;
		else
			break;
	}
	
	return tokens;
}

static gint
get_days_in_month (gint mon, gint year)
{
	switch (mon) {
	case 1: case 3: case 5: case 7: case 8: case 10: case 12:
		return 31;
	case 4: case 6: case 9: case 11:
		return 30;
	case 2:
		if ((year % 4 == 0 && year % 100 != 0) || year % 400 == 0)
			return 29;
		return 28;
	default:
		return 30;
	}
}

static gint
get_wday (gchar *str)
{
	gint i;
	
	g_return_val_if_fail (str != NULL, -1);
	
	for (i = 0; i < 7; i++)
		if (!g_strncasecmp (str, tm_days[i], 3))
			return i;
	
	return -1;  /* unknown week day */
}

static gint
get_mday (gchar *str)
{
	gchar *p;
	gint mday;
	
	g_return_val_if_fail (str != NULL, -1);
	
	for (p = str; *p; p++)
		if (!isdigit (*p))
			return -1;
	
	mday = atoi (str);
	
	if (mday < 0 || mday > 31)
		mday = -1;
	
	return mday;
}

static gint
get_month (gchar *str)
{
	gint i;
	
	g_return_val_if_fail (str != NULL, -1);
	
	for (i = 0; i < 12; i++)
		if (!g_strncasecmp (str, tm_months[i], 3))
			return i;
	
	return -1;  /* unknown month */
}

static gint
get_year (gchar *str)
{
	gint year;
	gchar *p;
	
	for (p = str; *p; p++)
		if (!isdigit (*p))
			return -1;
	
	year = atoi (str);
	
	if (year < 100)
		year += 1900;
	
	if (year < 1969)
		return -1;
	
	return year;
}

static gboolean
get_time (gchar *in, gint *hour, gint *min, gint *sec)
{
	gchar *p;
	gint colons = 0;
	gboolean digits = TRUE;
	
	for (p = in; *p && digits; p++) {
		if (*p == ':')
			colons++;
		else if (!isdigit (*p))
			digits = FALSE;
	}
	
	if (!digits || colons != 2)
		return FALSE;
	
	return sscanf (in, "%d:%d:%d", hour, min, sec) == 3;
}

static gint
get_tzone (GList **token)
{
	gint tz = -1;
	gint i;
	
	for (i = 0; *token && i < 2; *token = (*token)->next, i++) {
		gchar *str = (*token)->data;
		
		if (*str == '+' || *str == '-') {
			tz = atoi (str);
			return tz;
		} else {
			gint t;
			
			if (*str == '(')
				str++;
			
			for (t = 0; t < 15; t++)
				if (!strncmp (str, tz_offsets[t].name, strlen (tz_offsets[t].name))) {
					tz = tz_offsets[t].offset;
					return tz;
				}
		}
	}
	
	return -1;
}

static time_t
parse_rfc822_date (GList *tokens, int *tzone)
{
	GList *token;
	struct tm tm;
	time_t t;
	gint hour, min, sec, offset, n;
	
	g_return_val_if_fail (tokens != NULL, (time_t) 0);
	
	token = tokens;
	
	memset ((void *)&tm, 0, sizeof (struct tm));
	
	if ((n = get_wday (token->data)) != -1) {
		/* not all dates may have this... */
		tm.tm_wday = n;
		token = token->next;
	}
	
	/* get the mday */
	if (!token || (n = get_mday (token->data)) == -1)
		return (time_t) 0;
	
	tm.tm_mday = n;
	token = token->next;
	
	/* get the month */
	if (!token || (n = get_month (token->data)) == -1)
		return (time_t) 0;
	
	tm.tm_mon = n;
	token = token->next;
	
	/* get the year */
	if (!token || (n = get_year (token->data)) == -1)
		return (time_t) 0;
	
	tm.tm_year = n - 1900;
	token = token->next;
	
	/* get the hour/min/sec */
	if (!token || !get_time (token->data, &hour, &min, &sec))
		return (time_t) 0;
	
	tm.tm_hour = hour;
	tm.tm_min = min;
	tm.tm_sec = sec;
	token = token->next;
	
	/* get the timezone */
	if (!token || (n = get_tzone (&token)) == -1) {
		/* I guess we assume tz is GMT? */			
		offset = 0;
	} else {
		offset = n;
	}
	
	t = mktime (&tm);
#if defined(HAVE_TIMEZONE)
	t -= timezone;
#elif defined(HAVE_TM_GMTOFF)
	t += tm.tm_gmtoff;
#else
#error Neither HAVE_TIMEZONE nor HAVE_TM_GMTOFF defined. Rerun autoheader, autoconf, etc.
#endif
	
	/* t is now GMT of the time we want, but not offset by the timezone ... */
	
	/* this should convert the time to the GMT equiv time */
	t -= ((offset / 100) * 60 * 60) + (offset % 100) * 60;
	
	if (tzone)
		*tzone = offset;
	
	return t;
}

static time_t
parse_broken_date (GList *tokens, int *tzone)
{
	GList *token;
	struct tm tm;
	gint hour, min, sec, n;
	
	if (tzone)
		*tzone = 0;
	
	return (time_t) 0;
}


/**
 * g_mime_utils_header_decode_date: Decode a date string
 * @in: input date string
 * @saveoffset: 
 *
 * Returns the time_t representation of the date string specified by
 * #in. If 'saveoffset' is non-NULL, the value of the timezone offset
 * will be stored.
 **/
time_t
g_mime_utils_header_decode_date (const gchar *in, gint *saveoffset)
{
	GList *token, *tokens;
	time_t date;
	
	tokens = datetok (in);
	
	date = parse_rfc822_date (tokens, saveoffset);
	if (!date)
		date = parse_broken_date (tokens, saveoffset);
	
	/* cleanup */
	token = tokens;
	while (token) {
		g_free (token->data);
		token = token->next;
	}
	g_list_free (tokens);
	
	return date;
}


/**
 * g_mime_utils_quote_string: Quote a string.
 * @string: input string
 * 
 * Returns an allocated string containing the escaped and quoted (if
 * needed to be) input string. The decision to quote the string is
 * based on whether or not the input string contains any 'tspecials'
 * as defined by rfc2045.
 **/
gchar *
g_mime_utils_quote_string (const gchar *string)
{
	GString *out;
	gchar *qstring;
	guchar *c;
	gboolean quote = FALSE;
	
	out = g_string_new ("");
	
	for (c = (guchar *) string; *c; c++) {
		if (is_tspecial (*c))
			quote = TRUE;
		
		if (*c == '"' || *c == '\\')
			g_string_append_c (out, '\\');
		
		g_string_append_c (out, *c);
	}
	
	if (quote) {
		g_string_prepend_c (out, '"');
		g_string_append_c (out, '"');
	}
	
	qstring = out->str;
	g_string_free (out, FALSE);
	
	return qstring;
}


/**
 * g_mime_utils_unquote_string: Unquote a string.
 * @string:
 * 
 * Unquotes and unescapes a string.
 **/
void
g_mime_utils_unquote_string (gchar *string)
{
	/* if the string is quoted, unquote it */
	gchar *inptr, *inend;
	
	if (!string)
		return;
	
	inptr = string;
	inend = string + strlen (string);
	
	/* get rid of the wrapping quotes */
	if (*inptr == '"' && *(inend - 1) == '"') {
		inend--;
		*inend = '\0';
		if (*inptr)
			memmove (inptr, inptr + 1, inend - inptr);
	}
	
	/* un-escape the string */
	inend--;
	while (inptr < inend) {
		if (*inptr == '\\') {
			memmove (inptr, inptr + 1, inend - inptr);
			inend--;
		}
		
		inptr++;
	}
}


/**
 * g_mime_utils_text_is_8bit: Determine if a string contains 8bit chars
 * @text: text to check for 8bit chars
 *
 * Returns TRUE if the text contains 8bit characters or FALSE
 * otherwise.
 **/
gboolean
g_mime_utils_text_is_8bit (const guchar *text)
{
	guchar *c;
	
	for (c = (guchar *) text; *c; c++)
		if (*c > (guchar) 127)
			return TRUE;
	
	return FALSE;
}


/**
 * g_mime_utils_best_encoding: Determine the best encoding
 * @text: text to encode
 *
 * Returns a GMimePartEncodingType that is determined to be the best
 * encoding type for the specified block of text. ("best" in this
 * particular case means best compression)
 **/
GMimePartEncodingType
g_mime_utils_best_encoding (const guchar *text)
{
	guchar *ch;
	gint count = 0;
	gint total;
	
	for (ch = (guchar *) text; *ch; ch++)
		if (*ch > (guchar) 127)
			count++;
	
	total = (int) (ch - text);
	
	if ((float) count <= total * 0.17)
		return GMIME_PART_ENCODING_QUOTEDPRINTABLE;
	else
		return GMIME_PART_ENCODING_BASE64;
}

/* this decodes rfc2047's version of quoted-printable */
static gint
quoted_decode (const guchar *in, gint len, guchar *out)
{
	register const guchar *inptr;
	register guchar *outptr;
	const guchar *inend;
	guchar c, c1;
	gboolean err = FALSE;
	
	inend = in + len;
	outptr = out;
	
	inptr = in;
	while (inptr < inend) {
		c = *inptr++;
		if (c == '=') {
			if (inend - in >= 2) {
				c = toupper (*inptr++);
				c1 = toupper (*inptr++);
				*outptr++ = (((c >= 'A' ? c - 'A' + 10 : c - '0') & 0x0f) << 4)
					| ((c1 >= 'A' ? c1 - 'A' + 10 : c1 - '0') & 0x0f);
			} else {
				/* data was truncated */
				err = TRUE;
				break;
			}
		} else if (c == '_') {
			/* _'s are an rfc2047 shortcut for encoding spaces */
			*outptr++ = ' ';
		} else if (isblank (c) || strchr (CHARS_ESPECIAL, c)) {
			/* FIXME: this is an error! ignore for now ... */
			err = TRUE;
			break;
		} else {
			*outptr++ = c;
		}
	}
	
	if (!err) {
		return (outptr - out);
	}
	
	return -1;
}

static guchar *
decode_8bit_word (const guchar *word, gboolean *was_encoded)
{
	guchar *inptr, *inend;
	guint len;
	
	len = strlen (word);
	
	*was_encoded = TRUE;
	
	/* just make sure this is valid input */
	if (len < 7 || !(word[0] == '=' && word[1] == '?' && word[len - 2] == '?' && word[len - 1] == '=')) {
		d(fprintf (stderr, "invalid\n"));
		*was_encoded = FALSE;
		return g_strdup (word);
	}
	
	inptr = (guchar *) word + 2;
	inend = (guchar *) word + len - 2;
	
	inptr = memchr (inptr, '?', inend - inptr);
	if (inptr && inptr[2] == '?') {
		guchar *decoded;
		gint state = 0;
		gint save = 0;
		gint declen;
		
		d(fprintf (stderr, "encoding is '%c'\n", inptr[0]));
		
		inptr++;
		switch (*inptr) {
		case 'B':
		case 'b':
			inptr += 2;
			decoded = alloca (inend - inptr);
			declen = g_mime_utils_base64_decode_step (inptr, inend - inptr, decoded, &state, &save);
			return g_strndup (decoded, declen);
			break;
		case 'Q':
		case 'q':
			inptr += 2;
			decoded = alloca (inend - inptr);
			declen = quoted_decode (inptr, inend - inptr, decoded);
			
			if (declen == -1) {
				d(fprintf (stderr, "encountered broken 'Q' encoding\n"));
				return NULL;
			}
			
			return g_strndup (decoded, declen);
			break;
		default:
			d(fprintf (stderr, "unknown encoding\n"));
			return NULL;
		}
	}
	
	return NULL;
}


/**
 * g_mime_utils_8bit_header_decode: Decode an encoded header.
 * @in: header to decode
 *
 * Returns the mime encoded header as 8bit text.
 **/
gchar *
g_mime_utils_8bit_header_decode (const guchar *in)
{
	GString *out, *whtspc, *word;
	guchar *inptr;
	guchar *decoded;
	gboolean last_was_encoded = FALSE;
	gboolean last_was_space = FALSE;
	
	out = g_string_new ("");
	whtspc = g_string_new ("");
	word = g_string_new ("");
	inptr = (guchar *) in;
	
	while (inptr && *inptr) {
		guchar c = *inptr++;
		
		if (isspace (c) && !last_was_space) {
			/* we reached the end of a 'word' */
			gboolean was_encoded;
			guchar *dword;
			
			dword = decode_8bit_word (word->str, &was_encoded);
			if (dword) {
				if (!(last_was_encoded && was_encoded)) {
					/* rfc2047 states that you
                                           must ignore all whitespace
                                           between encoded words */
					g_string_append (out, whtspc->str);
				}
				
				g_string_append (out, dword);
				g_free (dword);
			} else {
				was_encoded = FALSE;
				g_string_append (out, whtspc->str);
				g_string_append (out, word->str);
			}
			
			last_was_encoded = was_encoded;
			
			g_string_free (whtspc, TRUE);
			whtspc = g_string_new ("");
			
			g_string_free (word, TRUE);
			word = g_string_new ("");
		}
		
		if (isspace (c)) {
			g_string_append_c (whtspc, c);
			last_was_space = TRUE;
		} else {
			g_string_append_c (word, c);
			last_was_space = FALSE;
		}
	}
	
	if (word->len || whtspc->len) {
		gboolean was_encoded;
		guchar *dword;
		
		dword = decode_8bit_word (word->str, &was_encoded);
		if (dword) {
			if (!(last_was_encoded && was_encoded)) {
				/* rfc2047 states that you
				   must ignore all whitespace
				   between encoded words */
				g_string_append (out, whtspc->str);
			}
			
			g_string_append (out, dword);
			g_free (dword);
		} else {
			g_string_append (out, whtspc->str);
			g_string_append (out, word->str);
		}
	}
	
	g_string_free (whtspc, TRUE);
	g_string_free (word, TRUE);
	
	decoded = out->str;
	g_string_free (out, FALSE);
	
	return decoded;
}

/* rfc2047 version of quoted-printable */
static gint
quoted_encode (const guchar *in, gint len, guchar *out, gushort safemask)
{
	register const guchar *inptr, *inend;
	guchar *outptr;
	guchar c;
	
	inptr = in;
	inend = in + len;
	outptr = out;
	
	while (inptr < inend) {
		c = *inptr++;
		if (gmime_special_table[c] & safemask && c != '\t') {
			if (c == ' ')
				c = '_';
			*outptr++ = c;
		} else {
			*outptr++ = '=';
			*outptr++ = tohex[(c >> 4) & 0xf];
			*outptr++ = tohex[c & 0xf];
		}
	}
	
	return (outptr - out);
}

static guchar *
encode_8bit_word (const guchar *word, gushort safemask, gboolean *this_was_encoded)
{
	guchar *encoded, *ptr;
	guint enclen, pos, len;
	gint state = 0;
	gint save = 0;
	gchar encoding;
	
	len = strlen (word);
	
	switch (g_mime_utils_best_encoding (word)) {
	case GMIME_PART_ENCODING_BASE64:
		/* this is a very precise calculation *cough* */
		enclen = (guint) (len * 4 / 3) + 4;
		encoded = alloca (enclen);
		
		encoding = 'b';
		
		pos = g_mime_utils_base64_encode_close (word, len, encoded, &state, &save);
		encoded[pos] = '\0';
		
		/* remove \n chars as headers need to be wrapped differently */
		ptr = encoded;
		while ((ptr = memchr (ptr, '\n', strlen (ptr))))
			memmove (ptr, ptr + 1, strlen (ptr));
		
		break;
	case GMIME_PART_ENCODING_QUOTEDPRINTABLE:
		/* this is a very precise calculation *cough* */
		enclen = (guint) (len * 3) + 4;
		encoded = alloca (enclen);
		
		encoding = 'q';
		
		pos = quoted_encode (word, len, encoded, safemask);
		encoded[pos] = '\0';
		
		break;
	default:
		if (this_was_encoded)
			*this_was_encoded = FALSE;
		
		return g_strdup (word);
	}
	
	if (this_was_encoded)
		*this_was_encoded = TRUE;
	
	return g_strdup_printf ("=?iso-8859-1?%c?%s?=", encoding, encoded);
}


/**
 * g_mime_utils_8bit_header_encode_phrase: Encode a string.
 * @in: header to encode
 *
 * Returns the header phrase as 1 encoded atom. Useful for encoding
 * internet addresses.
 **/
gchar *
g_mime_utils_8bit_header_encode_phrase (const guchar *in)
{
	return encode_8bit_word (in, IS_PSAFE, NULL);
}


/**
 * g_mime_utils_8bit_header_encode: Encode a string.
 * @in: header to encode
 *
 * Returns the header as several encoded atoms. Useful for encoding
 * headers like "Subject".
 **/
gchar *
g_mime_utils_8bit_header_encode (const guchar *in)
{
	GString *out, *word, *whtspc;
	guchar *inptr;
	guchar *encoded;
	gboolean is8bit = FALSE;
	gboolean last_was_encoded = FALSE;
	gboolean last_was_space = FALSE;
	
	out = g_string_new ("");
	word = g_string_new ("");
	whtspc = g_string_new ("");
	inptr = (guchar *) in;
	
	while (inptr && *inptr) {
		guchar c = *inptr++;
		
		if (isspace (c) && !last_was_space) {
			gboolean this_was_encoded = FALSE;
			guchar *eword;
			
			if (is8bit)
				eword = encode_8bit_word (word->str, IS_ESAFE, &this_was_encoded);
			else
				eword = g_strdup (word->str);
			
			/* append any whitespace */
			if (last_was_encoded && this_was_encoded) {
				/* we need to encode the whitespace */
				guchar *ewhtspc;
				gint len;
				
				ewhtspc = alloca (whtspc->len * 3 + 4);
				len = quoted_encode (whtspc->str, whtspc->len, ewhtspc, IS_ESAFE);
				ewhtspc[len] = '\0';
				
				g_string_append (out, " =?iso-8859-1?q?");
				g_string_append (out, ewhtspc);
				g_string_append (out, "?= ");
			} else {
				g_string_append (out, whtspc->str);
			}
			
			/* append the encoded word */
			g_string_append (out, eword);
			g_free (eword);
			
			g_string_free (whtspc, TRUE);
			whtspc = g_string_new ("");
			
			g_string_free (word, TRUE);
			word = g_string_new ("");
			
			last_was_encoded = this_was_encoded;
			is8bit = FALSE;
		}
		
		if (isspace (c)) {
			g_string_append_c (whtspc, c);
			last_was_space = TRUE;
		} else {
			if (c > 127)
				is8bit = TRUE;
			
			g_string_append_c (word, c);
			last_was_space = FALSE;
		}
	}
	
	if (word->len || whtspc->len) {
		gboolean this_was_encoded = FALSE;
		guchar *eword;
		
		if (is8bit)
			eword = encode_8bit_word (word->str, IS_ESAFE, &this_was_encoded);
		else
			eword = g_strdup (word->str);
		
		/* append any whitespace */
		if (last_was_encoded && this_was_encoded) {
			/* we need to encode the whitespace */
			guchar *ewhtspc;
			gint len;
			
			ewhtspc = alloca (whtspc->len * 3 + 4);
			len = quoted_encode (whtspc->str, whtspc->len, ewhtspc, IS_ESAFE);
			ewhtspc[len] = '\0';
			
			g_string_append (out, " =?iso-8859-1?q?");
			g_string_append (out, ewhtspc);
			g_string_append (out, "?= ");
		} else {
			g_string_append (out, whtspc->str);
		}
		
		/* append the encoded word */
		g_string_append (out, eword);
		g_free (eword);
	}
	
	g_string_free (whtspc, TRUE);
	g_string_free (word, TRUE);
	
	encoded = out->str;
	g_string_free (out, FALSE);
	
	return encoded;
}


/**
 * g_mime_utils_base64_encode_close:
 * @in: input stream
 * @inlen: length of the input
 * @out: output string
 * @state: holds the number of bits that are stored in @save
 * @save: leftover bits that have not yet been encoded
 *
 * Returns the number of bytes encoded. Call this when finished
 * encoding data with base64_encode_step to flush off the last little
 * bit.
 **/
gint
g_mime_utils_base64_encode_close (const guchar *in, gint inlen, guchar *out, gint *state, gint *save)
{
	guchar *outptr = out;
	gint c1, c2;
	
	if (inlen > 0)
		outptr += g_mime_utils_base64_encode_step (in, inlen, outptr, state, save);
	
	c1 = ((guchar *)save)[1];
	c2 = ((guchar *)save)[2];
	
	switch (((guchar *)save)[0]) {
	case 2:
		outptr[2] = base64_alphabet [(c2 & 0x0f) << 2];
		goto skip;
	case 1:
		outptr[2] = '=';
	skip:
		outptr[0] = base64_alphabet [c1 >> 2];
		outptr[1] = base64_alphabet [c2 >> 4 | ((c1 & 0x3) << 4)];
		outptr[3] = '=';
		outptr += 4;
		break;
	}
	*outptr++ = '\n';
	
	*save = 0;
	*state = 0;
	
	return (outptr - out);
}


/**
 * g_mime_utils_base64_encode_step: Base64 encode a chunk of data.
 * @in: input stream
 * @inlen: length of the input
 * @out: output string
 * @state: holds the number of bits that are stored in @save
 * @save: leftover bits that have not yet been encoded
 *
 * Returns the number of bytes encoded. Performs an 'encode step',
 * only encodes blocks of 3 characters to the output at a time, saves
 * left-over state in state and save (initialise to 0 on first
 * invocation).
 **/
gint
g_mime_utils_base64_encode_step (const guchar *in, gint inlen, guchar *out, gint *state, gint *save)
{
	const register guchar *inptr;
	register guchar *outptr;
	
	if (inlen <= 0)
		return 0;
	
	inptr = in;
	outptr = out;
	
	d(printf("we have %d chars, and %d saved chars\n", inlen, ((gchar *)save)[0]));
	
	if (inlen + ((guchar *)save)[0] > 2) {
		const guchar *inend = in + inlen - 2;
		register gint c1=0, c2=0, c3=0;
		register gint already;
		
		already = *state;
		
		switch (((gchar *)save)[0]) {
		case 1:	c1 = ((guchar *)save)[1]; goto skip1;
		case 2:	c1 = ((guchar *)save)[1];
			c2 = ((guchar *)save)[2]; goto skip2;
		}
		
		/* yes, we jump into the loop, no i'm not going to change it, its beautiful! */
		while (inptr < inend) {
			c1 = *inptr++;
		skip1:
			c2 = *inptr++;
		skip2:
			c3 = *inptr++;
			*outptr++ = base64_alphabet [c1 >> 2];
			*outptr++ = base64_alphabet [(c2 >> 4) | ((c1 & 0x3) << 4)];
			*outptr++ = base64_alphabet [((c2 &0x0f) << 2) | (c3 >> 6)];
			*outptr++ = base64_alphabet [c3 & 0x3f];
			/* this is a bit ugly ... */
			if ((++already) >= 19) {
				*outptr++ = '\n';
				already = 0;
			}
		}
		
		((guchar *)save)[0] = 0;
		inlen = 2 - (inptr - inend);
		*state = already;
	}
	
	d(printf ("state = %d, inlen = %d\n", (gint)((gchar *)save)[0], inlen));
	
	if (inlen > 0) {
		register gchar *saveout;
		
		/* points to the slot for the next char to save */
		saveout = & (((gchar *)save)[1]) + ((gchar *)save)[0];
		
		/* inlen can only be 0 1 or 2 */
		switch (inlen) {
		case 2:	*saveout++ = *inptr++;
		case 1:	*saveout++ = *inptr++;
		}
		((gchar *)save)[0] += inlen;
	}
	
	d(printf ("mode = %d\nc1 = %c\nc2 = %c\n",
		  (gint)((gchar *)save)[0],
		  (gint)((gchar *)save)[1],
		  (gint)((gchar *)save)[2]));
	
	return (outptr - out);
}

/**
 * g_mime_utils_base64_decode_step: decode a chunk of base64 encoded data
 * @in: input stream
 * @inlen: max length of data to decode
 * @out: output stream
 * @state: holds the number of bits that are stored in @save
 * @save: leftover bits that have not yet been decoded
 *
 * Returns the number of bytes decoded (which have been dumped in #out).
 **/
gint
g_mime_utils_base64_decode_step (const guchar *in, gint inlen, guchar *out, gint *state, guint *save)
{
	const register guchar *inptr;
	register guchar *outptr;
	const guchar *inend;
	guchar c;
	register guint v;
	int i;
	
	inend = in + inlen;
	outptr = out;
	
	/* convert 4 base64 bytes to 3 normal bytes */
	v = *save;
	i = *state;
	inptr = in;
	while (inptr < inend) {
		c = gmime_base64_rank[*inptr++];
		if (c != 0xff) {
			v = (v << 6) | c;
			i++;
			if (i == 4) {
				*outptr++ = v >> 16;
				*outptr++ = v >> 8;
				*outptr++ = v;
				i = 0;
			}
		}
	}
	
	*save = v;
	*state = i;
	
	/* quick scan back for '=' on the end somewhere */
	/* fortunately we can drop 1 output char for each trailing = (upto 2) */
	i = 2;
	while (inptr > in && i) {
		inptr--;
		if (gmime_base64_rank[*inptr] != 0xff) {
			if (*inptr == '=')
				outptr--;
			i--;
		}
	}
	
	/* if i != 0 then there is a truncation error! */
	return (outptr - out);
}


/**
 * g_mime_utils_uudecode_step: uudecode a chunk of data
 * @in: input stream
 * @inlen: max length of data to decode ( normally strlen(in) ??)
 * @out: output stream
 * @state: holds the number of bits that are stored in @save
 * @save: leftover bits that have not yet been decoded
 * @uulen: holds the value of the length-char which is used to calculate
 *         how many more chars need to be decoded for that 'line'
 *
 * Returns the number of bytes decoded. Performs a 'uudecode step' on
 * a chunk of uuencoded data. Assumes the "begin <mode> <file name>"
 * line has been stripped off.
 **/
gint
g_mime_utils_uudecode_step (const guchar *in, gint inlen, guchar *out, gint *state, guint32 *save, gchar *uulen)
{
	const register guchar *inptr;
	register guchar *outptr;
	const guchar *inend;
	guchar ch;
	register guint32 saved;
	gboolean last_was_eoln;
	int i;
	
	if (*uulen <= 0)
		last_was_eoln = TRUE;
	else
		last_was_eoln = FALSE;
	
	inend = in + inlen;
	outptr = out;
	saved = *save;
	i = *state;
	inptr = in;
	while (inptr < inend && *inptr) {
		if (*inptr == '\n' || last_was_eoln) {
			if (last_was_eoln) {
				*uulen = gmime_uu_rank[*inptr];
				last_was_eoln = FALSE;
			} else {
				last_was_eoln = TRUE;
			}
			
			inptr++;
			continue;
		}
		
		ch = *inptr++;
		
		if (*uulen > 0) {
			/* save the byte */
			saved = (saved << 8) | ch;
			i++;
			if (i == 4) {
				/* convert 4 uuencoded bytes to 3 normal bytes */
				guchar b0, b1, b2, b3;
				
				b0 = saved >> 24;
				b1 = saved >> 16 & 0xff;
				b2 = saved >> 8 & 0xff;
				b3 = saved & 0xff;
				
				if (*uulen >= 3) {
					*outptr++ = gmime_uu_rank[b0] << 2 | gmime_uu_rank[b1] >> 4;
					*outptr++ = gmime_uu_rank[b1] << 4 | gmime_uu_rank[b2] >> 2;
				        *outptr++ = gmime_uu_rank[b2] << 6 | gmime_uu_rank[b3];
				} else {
					if (*uulen >= 1) {
						*outptr++ = gmime_uu_rank[b0] << 2 | gmime_uu_rank[b1] >> 4;
					}
					if (*uulen >= 2) {
						*outptr++ = gmime_uu_rank[b1] << 4 | gmime_uu_rank[b2] >> 2;
					}
				}
				
				i = 0;
				saved = 0;
				*uulen -= 3;
			}
		} else {
			break;
		}
	}
	
	*save = saved;
	*state = i;
	
	return (outptr - out);
}


/**
 * g_mime_utils_quoted_encode_close:
 * @in: input stream
 * @inlen: length of the input
 * @out: output string
 * @state: holds the number of bits that are stored in @save
 * @save: leftover bits that have not yet been encoded
 *
 * Returns the number of bytes encoded. Call this when finished
 * encoding data with quoted_encode_step to flush off the last little
 * bit.
 **/
gint
g_mime_utils_quoted_encode_close (const guchar *in, gint inlen, guchar *out, gint *state, gint *save)
{
	register guchar *outptr = out;
	int last;

	if (inlen > 0)
		outptr += g_mime_utils_quoted_encode_step (in, inlen, outptr, state, save);
	
	last = *state;
	if (last != -1) {
		/* space/tab must be encoded if its the last character on
		   the line */
		if (is_qpsafe (last) && !isblank (last)) {
			*outptr++ = last;
		} else {
			*outptr++ = '=';
			*outptr++ = tohex[(last >> 4) & 0xf];
			*outptr++ = tohex[last & 0xf];
		}
	}
	
	*outptr++ = '\n';
	
	*save = 0;
	*state = -1;
	
	return (outptr - out);
}


/**
 * g_mime_utils_quoted_encode_step: QP encode a chunk of data.
 * @in: input stream
 * @inlen: length of the input
 * @out: output string
 * @state: holds the number of bits that are stored in @save
 * @save: leftover bits that have not yet been encoded
 *
 * Returns the number of bytes encoded. Performs an 'encode step',
 * saves left-over state in state and save (initialise to -1 on first
 * invocation).
 **/
gint
g_mime_utils_quoted_encode_step (const guchar *in, gint inlen, guchar *out, gint *state, gint *save)
{
	const register guchar *inptr, *inend;
	register guchar *outptr;
	guchar c;
	register int sofar = *save;  /* keeps track of how many chars on a line */
	register int last = *state;  /* keeps track if last char to end was a space cr etc */
	
	inptr = in;
	inend = in + inlen;
	outptr = out;
	while (inptr < inend) {
		c = *inptr++;
		if (c == '\r') {
			if (last != -1) {
				*outptr++ = '=';
				*outptr++ = tohex[(last >> 4) & 0xf];
				*outptr++ = tohex[last & 0xf];
				sofar += 3;
			}
			last = c;
		} else if (c == '\n') {
			if (last != -1 && last != '\r') {
				*outptr++ = '=';
				*outptr++ = tohex[(last >> 4) & 0xf];
				*outptr++ = tohex[last & 0xf];
			}
			*outptr++ = '\n';
			sofar = 0;
			last = -1;
		} else {
			if (last != -1) {
				if (is_qpsafe (last) || isblank ((char ) last)) {
					*outptr++ = last;
					sofar++;
				} else {
					*outptr++ = '=';
					*outptr++ = tohex[(last >> 4) & 0xf];
					*outptr++ = tohex[last & 0xf];
					sofar += 3;
				}
			}
			
			if (is_qpsafe (c) || isblank (c)) {
				if (sofar > 74) {
					*outptr++ = '=';
					*outptr++ = '\n';
					sofar = 0;
				}
				
				/* delay output of space char */
				if (isblank (c)) {
					last = c;
				} else {
					*outptr++ = c;
					sofar++;
					last = -1;
				}
			} else {
				if (sofar > 72) {
					*outptr++ = '=';
					*outptr++ = '\n';
					sofar = 3;
				} else
					sofar += 3;
				
				*outptr++ = '=';
				*outptr++ = tohex[(c >> 4) & 0xf];
				*outptr++ = tohex[c & 0xf];
				last = -1;
			}
		}
	}
	*save = sofar;
	*state = last;
	
	return (outptr - out);
}


/**
 * g_mime_utils_quoted_decode_step: decode a chunk of QP encoded data
 * @in: input stream
 * @inlen: max length of data to decode
 * @out: output stream
 * @savestate: holds the number of bits that are stored in @save
 * @saved: leftover bits that have not yet been decoded
 *
 * Returns the number of bytes decoded. Performs a 'decode step' on a
 * chunk of QP encoded data.
 **/
gint
g_mime_utils_quoted_decode_step (const guchar *in, gint inlen, guchar *out, gint *savestate, gint *saved)
{
	/* FIXME: this does not strip trailing spaces from lines (as
	 * it should, rfc 2045, section 6.7) Should it also
	 * canonicalise the end of line to CR LF??
	 *
	 * Note: Trailing rubbish (at the end of input), like = or =x
	 * or =\r will be lost.
	 */
	const register guchar *inptr;
	register guchar *outptr;
	const guchar *inend;
	guchar c;
	int state, save;
	
	inend = in + inlen;
	outptr = out;
	
	d(printf ("quoted-printable, decoding text '%.*s'\n", inlen, in));
	
	state = *savestate;
	save = *saved;
	inptr = in;
	while (inptr < inend) {
		switch (state) {
		case 0:
			while (inptr < inend) {
				c = *inptr++;
				/* FIXME: use a specials table to avoid 3 comparisons for the common case */
				if (c == '=') { 
					state = 1;
					break;
				}
#ifdef CANONICALISE_EOL
				/*else if (c=='\r') {
					state = 3;
				} else if (c=='\n') {
					*outptr++ = '\r';
					*outptr++ = c;
					} */
#endif
				else {
					*outptr++ = c;
				}
			}
			break;
		case 1:
			c = *inptr++;
			if (c == '\n') {
				/* soft break ... unix end of line */
				state = 0;
			} else {
				save = c;
				state = 2;
			}
			break;
		case 2:
			c = *inptr++;
			if (isxdigit (c) && isxdigit (save)) {
				c = toupper (c);
				save = toupper (save);
				*outptr++ = (((save >= 'A' ? save - 'A' + 10 : save - '0') & 0x0f) << 4)
					| ((c >= 'A' ? c - 'A' + 10 : c - '0') & 0x0f);
			} else if (c == '\n' && save == '\r') {
				/* soft break ... canonical end of line */
			} else {
				/* just output the data */
				*outptr++ = '=';
				*outptr++ = save;
				*outptr++ = c;
			}
			state = 0;
			break;
#ifdef CANONICALISE_EOL
		case 3:
			/* convert \n -> to \r\n, leaves \r\n alone */
			c = *inptr++;
			if (c == '\n') {
				*outptr++ = '\r';
				*outptr++ = c;
			} else {
				*outptr++ = '\r';
				*outptr++ = '\n';
				*outptr++ = c;
			}
			state = 0;
			break;
#endif
		}
	}
	
	*savestate = state;
	*saved = save;
	
	return (outptr - out);
}
