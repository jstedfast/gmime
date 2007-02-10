/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*  GMime
 *  Copyright (C) 2000-2007 Jeffrey Stedfast
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
 *  Foundation, 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */


#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef HAVE_SYS_PARAM_H
#include <sys/param.h> /* for MAXHOSTNAMELEN */
#else
#define MAXHOSTNAMELEN 64
#endif
#include <sys/types.h>
#include <unistd.h>
#ifdef HAVE_NETDB_H
#include <netdb.h>
#endif
#include <ctype.h>
#include <errno.h>

#include "gmime-utils.h"
#include "gmime-table-private.h"
#include "gmime-part.h"
#include "gmime-charset.h"
#include "gmime-iconv.h"
#include "gmime-iconv-utils.h"

#ifndef HAVE_ISBLANK
#define isblank(c) (c == ' ' || c == '\t')
#endif

#ifdef ENABLE_WARNINGS
#define w(x) x
#else
#define w(x)
#endif /* ENABLE_WARNINGS */

#define d(x)

#define GMIME_UUENCODE_CHAR(c) ((c) ? (c) + ' ' : '`')
#define	GMIME_UUDECODE_CHAR(c) (((c) - ' ') & 077)

#define GMIME_FOLD_PREENCODED  (GMIME_FOLD_LEN / 2)

/* date parser macros */
#define NUMERIC_CHARS          "1234567890"
#define WEEKDAY_CHARS          "SundayMondayTuesdayWednesdayThursdayFridaySaturday"
#define MONTH_CHARS            "JanuaryFebruaryMarchAprilMayJuneJulyAugustSeptemberOctoberNovemberDecember"
#define TIMEZONE_ALPHA_CHARS   "UTCGMTESTEDTCSTCDTMSTPSTPDTZAMNY()"
#define TIMEZONE_NUMERIC_CHARS "-+1234567890"
#define TIME_CHARS             "1234567890:"

#define DATE_TOKEN_NON_NUMERIC          (1 << 0)
#define DATE_TOKEN_NON_WEEKDAY          (1 << 1)
#define DATE_TOKEN_NON_MONTH            (1 << 2)
#define DATE_TOKEN_NON_TIME             (1 << 3)
#define DATE_TOKEN_HAS_COLON            (1 << 4)
#define DATE_TOKEN_NON_TIMEZONE_ALPHA   (1 << 5)
#define DATE_TOKEN_NON_TIMEZONE_NUMERIC (1 << 6)
#define DATE_TOKEN_HAS_SIGN             (1 << 7)


static char *base64_alphabet =
"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

static unsigned char tohex[16] = {
	'0', '1', '2', '3', '4', '5', '6', '7',
	'8', '9', 'A', 'B', 'C', 'D', 'E', 'F'
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

static unsigned char gmime_datetok_table[256] = {
	128,111,111,111,111,111,111,111,111,111,111,111,111,111,111,111,
	111,111,111,111,111,111,111,111,111,111,111,111,111,111,111,111,
	111,111,111,111,111,111,111,111, 79, 79,111,175,111,175,111,111,
	 38, 38, 38, 38, 38, 38, 38, 38, 38, 38,119,111,111,111,111,111,
	111, 75,111, 79, 75, 79,105, 79,111,111,107,111,111, 73, 75,107,
	 79,111,111, 73, 77, 79,111,109,111, 79, 79,111,111,111,111,111,
	111,105,107,107,109,105,111,107,105,105,111,111,107,107,105,105,
	107,111,105,105,105,105,107,111,111,105,111,111,111,111,111,111,
	111,111,111,111,111,111,111,111,111,111,111,111,111,111,111,111,
	111,111,111,111,111,111,111,111,111,111,111,111,111,111,111,111,
	111,111,111,111,111,111,111,111,111,111,111,111,111,111,111,111,
	111,111,111,111,111,111,111,111,111,111,111,111,111,111,111,111,
	111,111,111,111,111,111,111,111,111,111,111,111,111,111,111,111,
	111,111,111,111,111,111,111,111,111,111,111,111,111,111,111,111,
	111,111,111,111,111,111,111,111,111,111,111,111,111,111,111,111,
	111,111,111,111,111,111,111,111,111,111,111,111,111,111,111,111,
};

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
 * @date: time_t date representation
 * @tz_offset: Timezone offset
 *
 * Allocates a string buffer containing the rfc822 formatted date
 * string represented by @time and @offset.
 *
 * Returns a valid string representation of the date.
 **/
char *
g_mime_utils_header_format_date (time_t date, int tz_offset)
{
	struct tm tm;
	
	date += ((tz_offset / 100) * (60 * 60)) + (tz_offset % 100) * 60;
	
#ifdef HAVE_GMTIME_R
	gmtime_r (&date, &tm);
#else
	memcpy (&tm, gmtime (&date), sizeof (tm));
#endif
	
	return g_strdup_printf ("%s, %02d %s %04d %02d:%02d:%02d %+05d",
				tm_days[tm.tm_wday], tm.tm_mday,
				tm_months[tm.tm_mon],
				tm.tm_year + 1900,
				tm.tm_hour, tm.tm_min, tm.tm_sec,
				tz_offset);
}

/* This is where it gets ugly... */

struct _date_token {
	struct _date_token *next;
	unsigned char mask;
	const char *start;
	size_t len;
};

static struct _date_token *
datetok (const char *date)
{
	struct _date_token *tokens = NULL, *token, *tail = (struct _date_token *) &tokens;
	const char *start, *end;
        unsigned char mask;
	
	start = date;
	while (*start) {
		/* kill leading whitespace */
		while (*start == ' ' || *start == '\t')
			start++;
		
		if (*start == '\0')
			break;
		
		mask = gmime_datetok_table[(unsigned char) *start];
		
		/* find the end of this token */
		end = start + 1;
		while (*end && !strchr ("-/,\t\r\n ", *end))
			mask |= gmime_datetok_table[(unsigned char) *end++];
		
		if (end != start) {
			token = g_malloc (sizeof (struct _date_token));
			token->next = NULL;
			token->start = start;
			token->len = end - start;
			token->mask = mask;
			
			tail->next = token;
			tail = token;
		}
		
		if (*end)
			start = end + 1;
		else
			break;
	}
	
	return tokens;
}

static int
decode_int (const char *in, size_t inlen)
{
	register const char *inptr;
	int sign = 1, val = 0;
	const char *inend;
	
	inptr = in;
	inend = in + inlen;
	
	if (*inptr == '-') {
		sign = -1;
		inptr++;
	} else if (*inptr == '+')
		inptr++;
	
	for ( ; inptr < inend; inptr++) {
		if (!(*inptr >= '0' && *inptr <= '9'))
			return -1;
		else
			val = (val * 10) + (*inptr - '0');
	}
	
	val *= sign;
	
	return val;
}

#if 0
static int
get_days_in_month (int month, int year)
{
        switch (month) {
	case 1:
	case 3:
	case 5:
	case 7:
	case 8:
	case 10:
	case 12:
	        return 31;
	case 4:
	case 6:
	case 9:
	case 11:
	        return 30;
	case 2:
	        if (g_date_is_leap_year (year))
		        return 29;
		else
		        return 28;
	default:
	        return 0;
	}
}
#endif

static int
get_wday (const char *in, size_t inlen)
{
	int wday;
	
	g_return_val_if_fail (in != NULL, -1);
	
	if (inlen < 3)
		return -1;
	
	for (wday = 0; wday < 7; wday++) {
		if (!g_ascii_strncasecmp (in, tm_days[wday], 3))
			return wday;
	}
	
	return -1;  /* unknown week day */
}

static int
get_mday (const char *in, size_t inlen)
{
	int mday;
	
	g_return_val_if_fail (in != NULL, -1);
	
	mday = decode_int (in, inlen);
	
	if (mday < 0 || mday > 31)
		mday = -1;
	
	return mday;
}

static int
get_month (const char *in, size_t inlen)
{
	int i;
	
	g_return_val_if_fail (in != NULL, -1);
	
	if (inlen < 3)
		return -1;
	
	for (i = 0; i < 12; i++) {
		if (!g_ascii_strncasecmp (in, tm_months[i], 3))
			return i;
	}
	
	return -1;  /* unknown month */
}

static int
get_year (const char *in, size_t inlen)
{
	int year;
	
	g_return_val_if_fail (in != NULL, -1);
	
	if ((year = decode_int (in, inlen)) == -1)
		return -1;
	
	if (year < 100)
		year += (year < 70) ? 2000 : 1900;
	
	if (year < 1969)
		return -1;
	
	return year;
}

static gboolean
get_time (const char *in, size_t inlen, int *hour, int *min, int *sec)
{
	register const char *inptr;
	int *val, colons = 0;
	const char *inend;
	
	*hour = *min = *sec = 0;
	
	inend = in + inlen;
	val = hour;
	for (inptr = in; inptr < inend; inptr++) {
		if (*inptr == ':') {
			colons++;
			switch (colons) {
			case 1:
				val = min;
				break;
			case 2:
				val = sec;
				break;
			default:
				return FALSE;
			}
		} else if (!(*inptr >= '0' && *inptr <= '9'))
			return FALSE;
		else
			*val = (*val * 10) + (*inptr - '0');
	}
	
	return TRUE;
}

static int
get_tzone (struct _date_token **token)
{
	const char *inptr, *inend;
	size_t inlen;
	int i, t;
	
	for (i = 0; *token && i < 2; *token = (*token)->next, i++) {
		inptr = (*token)->start;
		inlen = (*token)->len;
		inend = inptr + inlen;
		
		if (*inptr == '+' || *inptr == '-') {
			return decode_int (inptr, inlen);
		} else {
			if (*inptr == '(') {
				inptr++;
				if (*(inend - 1) == ')')
					inlen -= 2;
				else
					inlen--;
			}
			
			for (t = 0; t < 15; t++) {
				size_t len = strlen (tz_offsets[t].name);
				
				if (len != inlen)
					continue;
				
				if (!strncmp (inptr, tz_offsets[t].name, len))
					return tz_offsets[t].offset;
			}
		}
	}
	
	return -1;
}

static time_t
mktime_utc (struct tm *tm)
{
	time_t tt;
	
	tm->tm_isdst = -1;
	tt = mktime (tm);
	
#if defined (HAVE_TM_GMTOFF)
	tt += tm->tm_gmtoff;
#elif defined (HAVE_TIMEZONE)
	if (tm->tm_isdst > 0) {
#if defined (HAVE_ALTZONE)
		tt -= altzone;
#else /* !defined (HAVE_ALTZONE) */
		tt -= (timezone - 3600);
#endif
	} else
		tt -= timezone;
#else
#error Neither HAVE_TIMEZONE nor HAVE_TM_GMTOFF defined. Rerun autoheader, autoconf, etc.
#endif
	
	return tt;
}

static time_t
parse_rfc822_date (struct _date_token *tokens, int *tzone)
{
	int hour, min, sec, offset, n;
	struct _date_token *token;
	struct tm tm;
	time_t t;
	
	g_return_val_if_fail (tokens != NULL, (time_t) 0);
	
	token = tokens;
	
	memset ((void *) &tm, 0, sizeof (struct tm));
	
	if ((n = get_wday (token->start, token->len)) != -1) {
		/* not all dates may have this... */
		tm.tm_wday = n;
		token = token->next;
	}
	
	/* get the mday */
	if (!token || (n = get_mday (token->start, token->len)) == -1)
		return (time_t) 0;
	
	tm.tm_mday = n;
	token = token->next;
	
	/* get the month */
	if (!token || (n = get_month (token->start, token->len)) == -1)
		return (time_t) 0;
	
	tm.tm_mon = n;
	token = token->next;
	
	/* get the year */
	if (!token || (n = get_year (token->start, token->len)) == -1)
		return (time_t) 0;
	
	tm.tm_year = n - 1900;
	token = token->next;
	
	/* get the hour/min/sec */
	if (!token || !get_time (token->start, token->len, &hour, &min, &sec))
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
	
	t = mktime_utc (&tm);
	
	/* t is now GMT of the time we want, but not offset by the timezone ... */
	
	/* this should convert the time to the GMT equiv time */
	t -= ((offset / 100) * 60 * 60) + (offset % 100) * 60;
	
	if (tzone)
		*tzone = offset;
	
	return t;
}


#define date_token_mask(t)  (((struct _date_token *) t)->mask)
#define is_numeric(t)       ((date_token_mask (t) & DATE_TOKEN_NON_NUMERIC) == 0)
#define is_weekday(t)       ((date_token_mask (t) & DATE_TOKEN_NON_WEEKDAY) == 0)
#define is_month(t)         ((date_token_mask (t) & DATE_TOKEN_NON_MONTH) == 0)
#define is_time(t)          (((date_token_mask (t) & DATE_TOKEN_NON_TIME) == 0) && (date_token_mask (t) & DATE_TOKEN_HAS_COLON))
#define is_tzone_alpha(t)   ((date_token_mask (t) & DATE_TOKEN_NON_TIMEZONE_ALPHA) == 0)
#define is_tzone_numeric(t) (((date_token_mask (t) & DATE_TOKEN_NON_TIMEZONE_NUMERIC) == 0) && (date_token_mask (t) & DATE_TOKEN_HAS_SIGN))
#define is_tzone(t)         (is_tzone_alpha (t) || is_tzone_numeric (t))

static time_t
parse_broken_date (struct _date_token *tokens, int *tzone)
{
	gboolean got_wday, got_month, got_tzone;
	int hour, min, sec, offset, n;
	struct _date_token *token;
	struct tm tm;
	time_t t;
	
	memset ((void *) &tm, 0, sizeof (struct tm));
	got_wday = got_month = got_tzone = FALSE;
	offset = 0;
	
	token = tokens;
	while (token) {
		if (is_weekday (token) && !got_wday) {
			if ((n = get_wday (token->start, token->len)) != -1) {
				d(printf ("weekday; "));
				got_wday = TRUE;
				tm.tm_wday = n;
				goto next;
			}
		}
		
		if (is_month (token) && !got_month) {
			if ((n = get_month (token->start, token->len)) != -1) {
				d(printf ("month; "));
				got_month = TRUE;
				tm.tm_mon = n;
				goto next;
			}
		}
		
		if (is_time (token) && !tm.tm_hour && !tm.tm_min && !tm.tm_sec) {
			if (get_time (token->start, token->len, &hour, &min, &sec)) {
				d(printf ("time; "));
				tm.tm_hour = hour;
				tm.tm_min = min;
				tm.tm_sec = sec;
				goto next;
			}
		}
		
		if (is_tzone (token) && !got_tzone) {
			struct _date_token *t = token;
			
			if ((n = get_tzone (&t)) != -1) {
				d(printf ("tzone; "));
				got_tzone = TRUE;
				offset = n;
				goto next;
			}
		}
		
		if (is_numeric (token)) {
			if (token->len == 4 && !tm.tm_year) {
				if ((n = get_year (token->start, token->len)) != -1) {
					d(printf ("year; "));
					tm.tm_year = n - 1900;
					goto next;
				}
			} else {
				/* Note: assumes MM-DD-YY ordering if '0 < MM < 12' holds true */
				if (!got_month && token->next && is_numeric (token->next)) {
					if ((n = decode_int (token->start, token->len)) > 12) {
						goto mday;
					} else if (n > 0) {
						d(printf ("mon; "));
						got_month = TRUE;
						tm.tm_mon = n - 1;
					}
					goto next;
				} else if (!tm.tm_mday && (n = get_mday (token->start, token->len)) != -1) {
				mday:
					d(printf ("mday; "));
					tm.tm_mday = n;
					goto next;
				} else if (!tm.tm_year) {
					if ((n = get_year (token->start, token->len)) != -1) {
						printf ("2-digit year; ");
						tm.tm_year = n - 1900;
					}
					goto next;
				}
			}
		}
		
		d(printf ("???; "));
		
	next:
		
		token = token->next;
	}
	
	d(printf ("\n"));
	
	t = mktime_utc (&tm);
	
	/* t is now GMT of the time we want, but not offset by the timezone ... */
	
	/* this should convert the time to the GMT equiv time */
	t -= ((offset / 100) * 60 * 60) + (offset % 100) * 60;
	
	if (tzone)
		*tzone = offset;
	
	return t;
}

#if 0
static void
gmime_datetok_table_init (void)
{
	int i;
	
	memset (gmime_datetok_table, 0, sizeof (gmime_datetok_table));
	
	for (i = 0; i < 256; i++) {
		if (!strchr (NUMERIC_CHARS, i))
			gmime_datetok_table[i] |= DATE_TOKEN_NON_NUMERIC;
		
		if (!strchr (WEEKDAY_CHARS, i))
			gmime_datetok_table[i] |= DATE_TOKEN_NON_WEEKDAY;
		
		if (!strchr (MONTH_CHARS, i))
			gmime_datetok_table[i] |= DATE_TOKEN_NON_MONTH;
		
		if (!strchr (TIME_CHARS, i))
			gmime_datetok_table[i] |= DATE_TOKEN_NON_TIME;
		
		if (!strchr (TIMEZONE_ALPHA_CHARS, i))
			gmime_datetok_table[i] |= DATE_TOKEN_NON_TIMEZONE_ALPHA;
		
		if (!strchr (TIMEZONE_NUMERIC_CHARS, i))
			gmime_datetok_table[i] |= DATE_TOKEN_NON_TIMEZONE_NUMERIC;
		
		if (((char) i) == ':')
			gmime_datetok_table[i] |= DATE_TOKEN_HAS_COLON;
		
		if (strchr ("+-", i))
			gmime_datetok_table[i] |= DATE_TOKEN_HAS_SIGN;
	}
	
	printf ("static unsigned char gmime_datetok_table[256] = {");
	for (i = 0; i < 256; i++) {
		if (i % 16 == 0)
			printf ("\n\t");
		printf ("%3d,", gmime_datetok_table[i]);
	}
	printf ("\n};\n");
}
#endif


/**
 * g_mime_utils_header_decode_date:
 * @in: input date string
 * @tz_offset: timezone offset
 *
 * Decodes the rfc822 date string and saves the GMT offset into
 * @saveoffset if non-NULL.
 *
 * Returns the time_t representation of the date string specified by
 * @in. If 'saveoffset' is non-NULL, the value of the timezone offset
 * will be stored.
 **/
time_t
g_mime_utils_header_decode_date (const char *in, int *tz_offset)
{
	struct _date_token *token, *tokens;
	time_t date;
	
	if (!(tokens = datetok (in))) {
		if (tz_offset)
			*tz_offset = 0;
		
		return (time_t) 0;
	}
	
	if (!(date = parse_rfc822_date (tokens, tz_offset)))
		date = parse_broken_date (tokens, tz_offset);
	
	/* cleanup */
	while (tokens) {
		token = tokens;
		tokens = tokens->next;
		g_free (token);
	}
	
	return date;
}


/**
 * g_mime_utils_generate_message_id:
 * @fqdn: Fully qualified domain name
 *
 * Generates a unique Message-Id.
 *
 * Returns a unique string in an addr-spec format suitable for use as
 * a Message-Id.
 **/
char *
g_mime_utils_generate_message_id (const char *fqdn)
{
#ifdef G_THREADS_ENABLED
	static GStaticMutex mutex = G_STATIC_MUTEX_INIT;
#define MUTEX_LOCK()   g_static_mutex_lock (&mutex)
#define MUTEX_UNLOCK() g_static_mutex_unlock (&mutex)
#else
#define MUTEX_LOCK()
#define MUTEX_UNLOCK()
#endif
	static unsigned int count = 0;
	char host[MAXHOSTNAMELEN + 1];
#ifdef HAVE_GETADDRINFO
	struct addrinfo hints, *res;
	char *name = NULL;
#endif
	char *msgid;
	
	if (!fqdn) {
#ifdef HAVE_GETHOSTNAME
		if (gethostname (host, sizeof (host)) == 0) {
#ifdef HAVE_GETADDRINFO
			memset (&hints, 0, sizeof (hints));
			hints.ai_flags = AI_CANONNAME;
			
			if (getaddrinfo (host, NULL, &hints, &res) == 0) {
				name = g_strdup (res->ai_canonname);
				freeaddrinfo (res);
			}
#endif /* HAVE_GETADDRINFO */
		} else {
			host[0] = '\0';
		}
#else
		host[0] = '\0';
#endif /* HAVE_GETHOSTNAME */
		
#ifdef HAVE_GETADDRINFO
		fqdn = name != NULL ? name : (*host ? host : "localhost.localdomain");
		g_free (name);
#else
		fqdn = *host ? host : "localhost.localdomain";
#endif
	}
	
	MUTEX_LOCK ();
	msgid = g_strdup_printf ("%ul.%ul.%ul@%s", (unsigned int) time (NULL), getpid (), count++, fqdn);
	MUTEX_UNLOCK ();
	
	return msgid;
}

/* external symbols from internet-address.c */
extern void decode_lwsp (const char **in);
extern char *decode_word (const char **in);
extern char *decode_addrspec (const char **in);


static char *
decode_msgid (const char **in)
{
	const char *inptr = *in;
	char *msgid = NULL;
	
	decode_lwsp (&inptr);
	if (*inptr != '<') {
		w(g_warning ("Invalid msg-id; missing '<': %s", *in));
	} else {
		inptr++;
	}
	
	decode_lwsp (&inptr);
	if ((msgid = decode_addrspec (&inptr))) {
		decode_lwsp (&inptr);
		if (*inptr != '>') {
			w(g_warning ("Invalid msg-id; missing '>': %s", *in));
		} else {
			inptr++;
		}
		
		*in = inptr;
	} else {
		w(g_warning ("Invalid msg-id; missing addr-spec: %s", *in));
		*in = inptr;
		while (*inptr && *inptr != '>')
			inptr++;
		
		msgid = g_strndup (*in, inptr - *in);
		*in = inptr;
	}
	
	return msgid;
}


/**
 * g_mime_utils_decode_message_id:
 * @message_id: string containing a message-id
 *
 * Decodes a msg-id as defined by rfc822.
 *
 * Returns the addr-spec portion of the msg-id.
 **/
char *
g_mime_utils_decode_message_id (const char *message_id)
{
	g_return_val_if_fail (message_id != NULL, NULL);
	
	return decode_msgid (&message_id);
}


/**
 * g_mime_references_decode:
 * @text: string containing a list of msg-ids
 *
 * Decodes a list of msg-ids as in the References and/or In-Reply-To
 * headers defined in rfc822.
 *
 * Returns a list of referenced msg-ids.
 **/
GMimeReferences *
g_mime_references_decode (const char *text)
{
	GMimeReferences *refs, *tail, *ref;
	const char *inptr = text;
	char *word, *msgid;
	
	g_return_val_if_fail (text != NULL, NULL);
	
	refs = NULL;
	tail = (GMimeReferences *) &refs;
	
	while (*inptr) {
		decode_lwsp (&inptr);
		if (*inptr == '<') {
			/* looks like a msg-id */
			if ((msgid = decode_msgid (&inptr))) {
				ref = g_new (GMimeReferences, 1);
				ref->next = NULL;
				ref->msgid = msgid;
				tail->next = ref;
				tail = ref;
			} else {
				w(g_warning ("Invalid References header: %s", inptr));
				break;
			}
		} else if (*inptr) {
			/* looks like part of a phrase */
			if ((word = decode_word (&inptr))) {
				g_free (word);
			} else {
				w(g_warning ("Invalid References header: %s", inptr));
				break;
			}
		}
	}
	
	return refs;
}


/**
 * g_mime_references_append:
 * @refs: the address of a GMimeReferences list
 * @msgid: a message-id string
 *
 * Appends a reference to msgid to the list of references.
 **/
void
g_mime_references_append (GMimeReferences **refs, const char *msgid)
{
	GMimeReferences *ref;
	
	g_return_if_fail (refs != NULL);
	g_return_if_fail (msgid != NULL);
	
	ref = (GMimeReferences *) refs;
	while (ref->next)
		ref = ref->next;
	
	ref->next = g_new (GMimeReferences, 1);
	ref->next->msgid = g_strdup (msgid);
	ref->next->next = NULL;
}


/**
 * g_mime_references_clear:
 * @refs: address of a GMimeReferences list
 *
 * Clears the GMimeReferences list and resets it to %NULL.
 **/
void
g_mime_references_clear (GMimeReferences **refs)
{
	GMimeReferences *ref, *next;
	
	g_return_if_fail (refs != NULL);
	
	ref = *refs;
	while (ref) {
		next = ref->next;
		g_free (ref->msgid);
		g_free (ref);
		ref = next;
	}
	
	*refs = NULL;
}


/**
 * g_mime_references_next:
 * @ref: a GMimeReferences list
 *
 * Advances to the next reference node in the GMimeReferences list.
 *
 * Returns the next reference node in the GMimeReferences list.
 **/
GMimeReferences *
g_mime_references_next (const GMimeReferences *ref)
{
	return ref ? ref->next : NULL;
}


static gboolean
is_rfc2047_token (const char *inptr, size_t len)
{
	if (len < 8 || strncmp (inptr, "=?", 2) != 0 || strncmp (inptr + len - 2, "?=", 2) != 0)
		return FALSE;
	
	inptr += 3;
	len -= 3;
	
	while (*inptr != '?' && len > 0) {
		inptr++;
		len--;
	}
	
	if (*inptr != '?' || len < 4)
		return FALSE;
	
	if (inptr[1] != 'q' && inptr[1] != 'Q' && inptr[1] != 'b' && inptr[1] != 'B')
		return FALSE;
	
	inptr += 2;
	len -= 2;
	
	if (*inptr != '?')
		return FALSE;
	
	return TRUE;
}

static char *
header_fold (const char *in, gboolean structured)
{
	gboolean last_was_lwsp = FALSE;
	register const char *inptr;
	size_t len, outlen, i;
	size_t fieldlen;
	GString *out;
	char *ret;
	
	inptr = in;
	len = strlen (in);
	if (len <= GMIME_FOLD_LEN + 1)
		return g_strdup (in);
	
	out = g_string_new ("");
	fieldlen = strcspn (inptr, ": \t\n");
	g_string_append_len (out, inptr, fieldlen);
	outlen = fieldlen;
	inptr += fieldlen;
	
	while (*inptr && *inptr != '\n') {
		len = strcspn (inptr, " \t\n");
		
		if (len > 1 && outlen + len > GMIME_FOLD_LEN) {
			if (outlen > 1 && out->len > fieldlen + 2) {
				if (last_was_lwsp) {
					if (structured)
						out->str[out->len - 1] = '\t';
					
					g_string_insert_c (out, out->len - 1, '\n');
				} else
					g_string_append (out, "\n\t");
				outlen = 1;
			}
			
			if (!structured && !is_rfc2047_token (inptr, len)) {
				/* check for very long words, just cut them up */
				while (outlen + len > GMIME_FOLD_LEN) {
					for (i = 0; i < GMIME_FOLD_LEN - outlen; i++)
						g_string_append_c (out, inptr[i]);
					inptr += GMIME_FOLD_LEN - outlen;
					len -= GMIME_FOLD_LEN - outlen;
					g_string_append (out, "\n\t");
					outlen = 1;
				}
			} else {
				g_string_append_len (out, inptr, len);
				outlen += len;
				inptr += len;
			}
			last_was_lwsp = FALSE;
		} else if (len > 0) {
			g_string_append_len (out, inptr, len);
			outlen += len;
			inptr += len;
			last_was_lwsp = FALSE;
		} else {
			last_was_lwsp = TRUE;
			if (*inptr == '\t') {
				/* tabs are a good place to fold, odds
				   are that this is where the previous
				   mailer folded it */
				g_string_append (out, "\n\t");
				outlen = 1;
				while (isblank (*inptr))
					inptr++;
			} else {
				g_string_append_c (out, *inptr++);
				outlen++;
			}
		}
	}
	
	if (*inptr == '\n' && out->str[out->len - 1] != '\n')
		g_string_append_c (out, '\n');
	
	ret = out->str;
	g_string_free (out, FALSE);
	
	return ret;
}


/**
 * g_mime_utils_structured_header_fold:
 * @in: input header string
 *
 * Folds a structured header according to the rules in rfc822.
 *
 * Returns an allocated string containing the folded header.
 **/
char *
g_mime_utils_structured_header_fold (const char *in)
{
	return header_fold (in, TRUE);
}


/**
 * g_mime_utils_unstructured_header_fold:
 * @in: input header string
 *
 * Folds an unstructured header according to the rules in rfc822.
 *
 * Returns an allocated string containing the folded header.
 **/
char *
g_mime_utils_unstructured_header_fold (const char *in)
{
	return header_fold (in, FALSE);
}


/**
 * g_mime_utils_header_fold:
 * @in: input header string
 *
 * Folds a structured header according to the rules in rfc822.
 *
 * Returns an allocated string containing the folded header.
 **/
char *
g_mime_utils_header_fold (const char *in)
{
	return header_fold (in, TRUE);
}


/**
 * g_mime_utils_header_printf:
 * @format: string format
 * @Varargs: arguments
 *
 * Allocates a buffer containing a formatted header specified by the
 * @Varargs.
 *
 * Returns an allocated string containing the folded header specified
 * by @format and the following arguments.
 **/
char *
g_mime_utils_header_printf (const char *format, ...)
{
	char *buf, *ret;
	va_list ap;
	
	va_start (ap, format);
	buf = g_strdup_vprintf (format, ap);
	va_end (ap);
	
	ret = header_fold (buf, TRUE);
	g_free (buf);
	
	return ret;
}

static gboolean
need_quotes (const char *string)
{
	gboolean quoted = FALSE;
	const char *inptr;
	
	inptr = string;
	
	while (*inptr) {
		if (*inptr == '\\')
			inptr++;
		else if (*inptr == '"')
			quoted = !quoted;
		else if (!quoted && (is_tspecial (*inptr) || *inptr == '.'))
			return TRUE;
		
		if (*inptr)
			inptr++;
	}
	
	return FALSE;
}

/**
 * g_mime_utils_quote_string:
 * @string: input string
 *
 * Quotes @string as needed according to the rules in rfc2045.
 * 
 * Returns an allocated string containing the escaped and quoted (if
 * needed to be) input string. The decision to quote the string is
 * based on whether or not the input string contains any 'tspecials'
 * as defined by rfc2045.
 **/
char *
g_mime_utils_quote_string (const char *string)
{
	gboolean quote;
	const char *c;
	char *qstring;
	GString *out;
	
	out = g_string_new ("");
	
	if ((quote = need_quotes (string)))
		g_string_append_c (out, '"');
	
	for (c = string; *c; c++) {
		if ((*c == '"' && quote) || *c == '\\')
			g_string_append_c (out, '\\');
		
		g_string_append_c (out, *c);
	}
	
	if (quote)
		g_string_append_c (out, '"');
	
	qstring = out->str;
	g_string_free (out, FALSE);
	
	return qstring;
}


/**
 * g_mime_utils_unquote_string: Unquote a string.
 * @string: string
 * 
 * Unquotes and unescapes a string.
 **/
void
g_mime_utils_unquote_string (char *string)
{
	/* if the string is quoted, unquote it */
	register char *inptr = string;
	int escaped = FALSE;
	int quoted = FALSE;
	
	if (!string)
		return;
	
	while (*inptr) {
		if (*inptr == '\\') {
			if (escaped)
				*string++ = *inptr++;
			else
				inptr++;
			escaped = !escaped;
		} else if (*inptr == '"') {
			if (escaped) {
				*string++ = *inptr++;
				escaped = FALSE;
			} else {
				quoted = !quoted;
				inptr++;
			}
		} else {
			*string++ = *inptr++;
			escaped = FALSE;
		}
	}
	
	*string = '\0';
}


/**
 * g_mime_utils_text_is_8bit:
 * @text: text to check for 8bit chars
 * @len: text length
 *
 * Determines if @text contains 8bit characters within the first @len
 * bytes.
 *
 * Returns %TRUE if the text contains 8bit characters or %FALSE
 * otherwise.
 **/
gboolean
g_mime_utils_text_is_8bit (const unsigned char *text, size_t len)
{
	register const unsigned char *inptr;
	const unsigned char *inend;
	
	g_return_val_if_fail (text != NULL, FALSE);
	
	inend = text + len;
	for (inptr = text; *inptr && inptr < inend; inptr++)
		if (*inptr > (unsigned char) 127)
			return TRUE;
	
	return FALSE;
}


/**
 * g_mime_utils_best_encoding:
 * @text: text to encode
 * @len: text length
 *
 * Determines the best content encoding for the first @len bytes of
 * @text.
 *
 * Returns a #GMimePartEncodingType that is determined to be the best
 * encoding type for the specified block of text. ("best" in this
 * particular case means best compression)
 **/
GMimePartEncodingType
g_mime_utils_best_encoding (const unsigned char *text, size_t len)
{
	const unsigned char *ch, *inend;
	size_t count = 0;
	
	inend = text + len;
	for (ch = text; ch < inend; ch++)
		if (*ch > (unsigned char) 127)
			count++;
	
	if ((float) count <= len * 0.17)
		return GMIME_PART_ENCODING_QUOTEDPRINTABLE;
	else
		return GMIME_PART_ENCODING_BASE64;
}

/* this decodes rfc2047's version of quoted-printable */
static ssize_t
quoted_decode (const unsigned char *in, size_t len, unsigned char *out)
{
	register const unsigned char *inptr;
	register unsigned char *outptr;
	const unsigned char *inend;
	unsigned char c, c1;
	
	inend = in + len;
	outptr = out;
	
	inptr = in;
	while (inptr < inend) {
		c = *inptr++;
		if (c == '=') {
			if (inend - inptr >= 2) {
				c = toupper (*inptr++);
				c1 = toupper (*inptr++);
				*outptr++ = (((c >= 'A' ? c - 'A' + 10 : c - '0') & 0x0f) << 4)
					| ((c1 >= 'A' ? c1 - 'A' + 10 : c1 - '0') & 0x0f);
			} else {
				/* data was truncated */
				return -1;
			}
		} else if (c == '_') {
			/* _'s are an rfc2047 shortcut for encoding spaces */
			*outptr++ = ' ';
		} else {
			*outptr++ = c;
		}
	}
	
	return (outptr - out);
}

#define is_rfc2047_encoded_word(atom, len) (len >= 7 && !strncmp (atom, "=?", 2) && !strncmp (atom + len - 2, "?=", 2))

static char *
rfc2047_decode_word (const char *in, size_t inlen)
{
	const unsigned char *instart = (const unsigned char *) in;
	const register unsigned char *inptr = instart + 2;
	const unsigned char *inend = instart + inlen - 2;
	const char *charset;
	char *charenc, *p;
	size_t len;
	iconv_t cd;
	char *buf;
	
	inptr = memchr (inptr, '?', inend - inptr);
	if (inptr && inptr[2] == '?') {
		unsigned char *decoded;
		guint32 save = 0;
		ssize_t declen;
		int state = 0;
		
		inptr++;
		
		switch (*inptr) {
		case 'B':
		case 'b':
			inptr += 2;
			decoded = g_alloca (inend - inptr);
			declen = g_mime_utils_base64_decode_step (inptr, inend - inptr, decoded, &state, &save);
			break;
		case 'Q':
		case 'q':
			inptr += 2;
			decoded = g_alloca (inend - inptr);
			declen = quoted_decode (inptr, inend - inptr, decoded);
			
			if (declen == -1) {
				d(fprintf (stderr, "encountered broken 'Q' encoding\n"));
				return NULL;
			}
			break;
		default:
			d(fprintf (stderr, "unknown encoding\n"));
			return NULL;
		}
		
		len = (inptr - 3) - (instart + 2);
		charenc = g_alloca (len + 1);
		memcpy (charenc, in + 2, len);
		charenc[len] = '\0';
		charset = charenc;
		
		/* rfc2231 updates rfc2047 encoded words...
		 * The ABNF given in RFC 2047 for encoded-words is:
		 *   encoded-word := "=?" charset "?" encoding "?" encoded-text "?="
		 * This specification changes this ABNF to:
		 *   encoded-word := "=?" charset ["*" language] "?" encoding "?" encoded-text "?="
		 */
		
		/* trim off the 'language' part if it's there... */
		if ((p = strchr (charset, '*')))
			*p = '\0';
		
		/* slight optimization */
		if (!g_ascii_strcasecmp (charset, "UTF-8"))
			return g_strndup ((char *) decoded, declen);
		
		cd = g_mime_iconv_open ("UTF-8", charset);
		if (cd == (iconv_t) -1) {
			w(g_warning ("Cannot convert from %s to UTF-8, header display may "
				     "be corrupt: %s", charset, g_strerror (errno)));
			charset = g_mime_locale_charset ();
			cd = g_mime_iconv_open ("UTF-8", charset);
			if (cd == (iconv_t) -1)
				return NULL;
		}
		
		buf = g_mime_iconv_strndup (cd, (char *) decoded, declen);
		g_mime_iconv_close (cd);
		
		if (!buf) {
			w(g_warning ("Failed to convert \"%.*s\" to UTF-8, display may be "
				     "corrupt: %s", declen, decoded, g_strerror (errno)));
		}
		
		return buf;
	}
	
	return NULL;
}


/**
 * g_mime_utils_header_decode_text:
 * @in: header to decode
 *
 * Decodes an rfc2047 encoded 'text' header.
 *
 * Returns the decoded header (which will be in UTF-8 if at all
 * possible).
 **/
char *
g_mime_utils_header_decode_text (const char *in)
{
	register const unsigned char *inptr;
	gboolean last_was_encoded = FALSE;
	gboolean last_was_space = FALSE;
	GString *out, *lwsp, *text;
	char *decoded;
	
	inptr = (const unsigned char *) in;
	out = g_string_sized_new (256);
	lwsp = g_string_sized_new (256);
	text = g_string_sized_new (256);
	
	while (inptr && *inptr) {
		unsigned char c = *inptr++;
		
		if (is_lwsp ((int) c) && !last_was_space) {
			/* we reached the end of an atom */
			gboolean was_encoded;
			char *dword = NULL;
			const char *word;
			
			if ((was_encoded = is_rfc2047_encoded_word (text->str, text->len)))
				word = dword = rfc2047_decode_word (text->str, text->len);
			else
				word = text->str;
			
			if (word) {
				if (!(last_was_encoded && was_encoded)) {
					/* rfc2047 states that you
                                           must ignore all whitespace
                                           between encoded words */
					g_string_append (out, lwsp->str);
				}
				
				g_string_append (out, word);
				g_free (dword);
			} else {
				was_encoded = FALSE;
				g_string_append (out, lwsp->str);
				g_string_append (out, text->str);
			}
			
			last_was_encoded = was_encoded;
			
			g_string_truncate (lwsp, 0);
			g_string_truncate (text, 0);
			
			if (is_lwsp (c)) {
				g_string_append_c (lwsp, c);
				last_was_space = TRUE;
			} else {
				/* This is mostly here for interoperability with broken
                                   mailers that might do something stupid like:
                                   =?iso-8859-1?Q?blah?=:\t=?iso-8859-1?Q?I_am_broken?= */
				g_string_append_c (out, c);
				last_was_encoded = FALSE;
				last_was_space = FALSE;
			}
			
			continue;
		}
		
		if (!is_lwsp (c)) {
			g_string_append_c (text, c);
			last_was_space = FALSE;
		} else {
			g_string_append_c (lwsp, c);
			last_was_space = TRUE;
		}
	}
	
	if (text->len || lwsp->len) {
		gboolean was_encoded;
		char *dword = NULL;
		const char *word;
		
		if ((was_encoded = is_rfc2047_encoded_word (text->str, text->len)))
			word = dword = rfc2047_decode_word (text->str, text->len);
		else
			word = text->str;
		
		if (word) {
			if (!(last_was_encoded && was_encoded)) {
				/* rfc2047 states that you
				   must ignore all whitespace
				   between encoded words */
				g_string_append (out, lwsp->str);
			}
			
			g_string_append (out, word);
			g_free (dword);
		} else {
			g_string_append (out, lwsp->str);
			g_string_append (out, text->str);
		}
	}
	
	g_string_free (lwsp, TRUE);
	g_string_free (text, TRUE);
	
	decoded = out->str;
	g_string_free (out, FALSE);
	
	return decoded;
}


/**
 * g_mime_utils_header_decode_phrase:
 * @in: header to decode
 *
 * Decodes an rfc2047 encoded 'phrase' header.
 *
 * Returns the decoded header (which will be in UTF-8 if at all
 * possible).
 **/
char *
g_mime_utils_header_decode_phrase (const char *in)
{
	register const unsigned char *inptr;
	gboolean last_was_encoded = FALSE;
	gboolean last_was_space = FALSE;
	GString *out, *lwsp, *atom;
	char *decoded;
	
	inptr = (const unsigned char *) in;
	out = g_string_sized_new (256);
	lwsp = g_string_sized_new (256);
	atom = g_string_sized_new (256);
	
	while (inptr && *inptr) {
		unsigned char c = *inptr++;
		
		if (!is_atom (c) && !last_was_space) {
			/* we reached the end of an atom */
			gboolean was_encoded;
			char *dword = NULL;
			const char *word;
			
			if ((was_encoded = is_rfc2047_encoded_word (atom->str, atom->len)))
				word = dword = rfc2047_decode_word (atom->str, atom->len);
			else
				word = atom->str;
			
			if (word) {
				if (!(last_was_encoded && was_encoded)) {
					/* rfc2047 states that you
                                           must ignore all whitespace
                                           between encoded words */
					g_string_append (out, lwsp->str);
				}
				
				g_string_append (out, word);
				g_free (dword);
			} else {
				was_encoded = FALSE;
				g_string_append (out, lwsp->str);
				g_string_append (out, atom->str);
			}
			
			last_was_encoded = was_encoded;
			
			g_string_truncate (lwsp, 0);
			g_string_truncate (atom, 0);
			
			if (is_lwsp (c)) {
				g_string_append_c (lwsp, c);
				last_was_space = TRUE;
			} else {
				/* This is mostly here for interoperability with broken
                                   mailers that might do something stupid like:
                                   =?iso-8859-1?Q?blah?=:\t=?iso-8859-1?Q?I_am_broken?= */
				g_string_append_c (out, c);
				last_was_encoded = FALSE;
				last_was_space = FALSE;
			}
			
			continue;
		}
		
		if (is_atom (c)) {
			g_string_append_c (atom, c);
			last_was_space = FALSE;
		} else {
			g_string_append_c (lwsp, c);
			last_was_space = TRUE;
		}
	}
	
	if (atom->len || lwsp->len) {
		gboolean was_encoded;
		char *dword = NULL;
		const char *word;
		
		if ((was_encoded = is_rfc2047_encoded_word (atom->str, atom->len)))
			word = dword = rfc2047_decode_word (atom->str, atom->len);
		else
			word = atom->str;
		
		if (word) {
			if (!(last_was_encoded && was_encoded)) {
				/* rfc2047 states that you
				   must ignore all whitespace
				   between encoded words */
				g_string_append (out, lwsp->str);
			}
			
			g_string_append (out, word);
			g_free (dword);
		} else {
			g_string_append (out, lwsp->str);
			g_string_append (out, atom->str);
		}
	}
	
	g_string_free (lwsp, TRUE);
	g_string_free (atom, TRUE);
	
	decoded = out->str;
	g_string_free (out, FALSE);
	
	return decoded;
}

/* rfc2047 version of quoted-printable */
static size_t
quoted_encode (const char *in, size_t len, unsigned char *out, gushort safemask)
{
	register const unsigned char *inptr = (const unsigned char *) in;
	const unsigned char *inend = inptr + len;
	register unsigned char *outptr = out;
	unsigned char c;
	
	while (inptr < inend) {
		c = *inptr++;
		if (c == ' ') {
			*outptr++ = '_';
		} else if (gmime_special_table[c] & safemask) {
			*outptr++ = c;
		} else {
			*outptr++ = '=';
			*outptr++ = tohex[(c >> 4) & 0xf];
			*outptr++ = tohex[c & 0xf];
		}
	}
	
	return (outptr - out);
}

static void
rfc2047_encode_word (GString *string, const char *word, size_t len,
		     const char *charset, gushort safemask)
{
	iconv_t cd = (iconv_t) -1;
	char *ptr, *uword = NULL;
	unsigned char *encoded;
	size_t enclen, pos;
	guint32 save = 0;
	int state = 0;
	char encoding;
	
	if (g_ascii_strcasecmp (charset, "UTF-8") != 0)
		cd = g_mime_iconv_open (charset, "UTF-8");
	
	if (cd != (iconv_t) -1) {
		uword = g_mime_iconv_strndup (cd, (char *) word, len);
		g_mime_iconv_close (cd);
	}
	
	if (uword) {
		len = strlen (uword);
		word = uword;
	} else {
		charset = "UTF-8";
	}
	
	switch (g_mime_utils_best_encoding ((const unsigned char *) word, len)) {
	case GMIME_PART_ENCODING_BASE64:
		enclen = BASE64_ENCODE_LEN (len);
		encoded = g_alloca (enclen + 1);
		
		encoding = 'b';
		
		pos = g_mime_utils_base64_encode_close ((const unsigned char *) word, len, encoded, &state, &save);
		encoded[pos] = '\0';
		
		/* remove \n chars as headers need to be wrapped differently */
		ptr = (char *) encoded;
		while ((ptr = memchr (ptr, '\n', strlen (ptr))))
			memmove (ptr, ptr + 1, strlen (ptr));
		
		break;
	case GMIME_PART_ENCODING_QUOTEDPRINTABLE:
		enclen = QP_ENCODE_LEN (len);
		encoded = g_alloca (enclen + 1);
		
		encoding = 'q';
		
		pos = quoted_encode (word, len, encoded, safemask);
		encoded[pos] = '\0';
		
		break;
	default:
		encoded = NULL;
		encoding = '\0';
		g_assert_not_reached ();
	}
	
	g_free (uword);
	
	g_string_append_printf (string, "=?%s?%c?%s?=", charset, encoding, encoded);
}


enum _rfc822_word_t {
	WORD_ATOM,
	WORD_QSTRING,
	WORD_2047
};

struct _rfc822_word {
	struct _rfc822_word *next;
	enum _rfc822_word_t type;
	const char *start, *end;
	int encoding;
};

static gboolean
word_types_compatable (enum _rfc822_word_t type1, enum _rfc822_word_t type2)
{
	switch (type1) {
	case WORD_ATOM:
		return type2 != WORD_ATOM;
	case WORD_QSTRING:
		return type2 != WORD_2047;
	case WORD_2047:
		return type2 == WORD_2047;
	default:
		return FALSE;
	}
}

/* okay, so 'text' fields don't actually contain 'word's, but we can group stuff similarly */
static struct _rfc822_word *
rfc2047_encode_get_rfc822_words (const char *in, gboolean phrase)
{
	struct _rfc822_word *words, *tail, *word;
	enum _rfc822_word_t type = WORD_ATOM;
	const char *inptr, *start, *last;
	int count = 0, encoding = 0;
	
	words = NULL;
	tail = (struct _rfc822_word *) &words;
	
	last = start = inptr = in;
	while (inptr && *inptr) {
		const char *newinptr;
		gunichar c;
		
		newinptr = g_utf8_next_char (inptr);
		c = g_utf8_get_char (inptr);
		if (newinptr == NULL || !g_unichar_validate (c)) {
			w(g_warning ("Invalid UTF-8 sequence encountered"));
			inptr++;
			continue;
		}
		
		inptr = newinptr;
		
		if (c < 256 && is_lwsp (c)) {
			if (count > 0) {
				word = g_new (struct _rfc822_word, 1);
				word->next = NULL;
				word->start = start;
				word->end = last;
				word->type = type;
				word->encoding = encoding;
				
				tail->next = word;
				tail = word;
				count = 0;
			}
			
			start = inptr;
			type = WORD_ATOM;
			encoding = 0;
		} else {
			count++;
			if (phrase && c < 128) {
				/* phrases can have qstring words */
				if (!is_atom (c))
					type = MAX (type, WORD_QSTRING);
			} else if (c > 127 && c < 256) {
				type = WORD_2047;
				encoding = MAX (encoding, 1);
			} else if (c >= 256) {
				type = WORD_2047;
				encoding = 2;
			}
			
			if (count >= GMIME_FOLD_PREENCODED) {
				word = g_new (struct _rfc822_word, 1);
				word->next = NULL;
				word->start = start;
				word->end = inptr;
				word->type = type;
				word->encoding = encoding;
				
				tail->next = word;
				tail = word;
				count = 0;
				
				start = inptr;
				type = WORD_ATOM;
				encoding = 0;
			}
		}
		
		last = inptr;
	}
	
	if (count > 0) {
		word = g_new (struct _rfc822_word, 1);
		word->next = NULL;
		word->start = start;
		word->end = last;
		word->type = type;
		word->encoding = encoding;
		
		tail->next = word;
		tail = word;
	}
	
	return words;
}

#define MERGED_WORD_LT_FOLDLEN(wlen, type) ((type) == WORD_2047 ? (wlen) < GMIME_FOLD_PREENCODED : (wlen) < (GMIME_FOLD_LEN - 8))

static gboolean
rfc2047_encode_merge_rfc822_words (struct _rfc822_word **wordsp)
{
	struct _rfc822_word *word, *next, *words = *wordsp;
	gboolean merged = FALSE;
	
	/* scan the list, checking for words of similar types that can be merged */
	word = words;
	while (word) {
		next = word->next;
		
		while (next) {
			/* merge nodes of the same type AND we are not creating too long a string */
			if (word_types_compatable (word->type, next->type)) {
				if (MERGED_WORD_LT_FOLDLEN (next->end - word->start, MAX (word->type, next->type))) {
					/* the resulting word type is the MAX of the 2 types */
					word->type = MAX (word->type, next->type);
					
					word->end = next->end;
					word->next = next->next;
					
					g_free (next);
					
					next = word->next;
					
					merged = TRUE;
				} else {
					/* if it is going to be too long, make sure we include the
					   separating whitespace */
					word->end = next->start;
					break;
				}
			} else {
				break;
			}
		}
		
		word = word->next;
	}
	
	*wordsp = words;
	
	return merged;
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

static char *
rfc2047_encode (const char *in, gushort safemask)
{
	struct _rfc822_word *words, *word, *prev = NULL;
	GString *out;
	char *outstr;
	
	if (!(words = rfc2047_encode_get_rfc822_words (in, safemask & IS_PSAFE)))
		return g_strdup (in);
	
	while (rfc2047_encode_merge_rfc822_words (&words))
		;
	
	out = g_string_new ("");
	
	/* output words now with spaces between them */
	word = words;
	while (word) {
		const char *start;
		size_t len;
		
		/* append correct number of spaces between words */
		if (prev && !(prev->type == WORD_2047 && word->type == WORD_2047)) {
			/* one or both of the words are not encoded so we write the spaces out untouched */
			len = word->start - prev->end;
			g_string_append_len (out, prev->end, len);
		}
		
		switch (word->type) {
		case WORD_ATOM:
			g_string_append_len (out, word->start, word->end - word->start);
			break;
		case WORD_QSTRING:
			g_assert (safemask & IS_PSAFE);
			g_string_append_len_quoted (out, word->start, word->end - word->start);
			break;
		case WORD_2047:
			if (prev && prev->type == WORD_2047) {
				/* include the whitespace chars between these 2 words in the
				   resulting rfc2047 encoded word. */
				len = word->end - prev->end;
				start = prev->end;
				
				/* encoded words need to be separated by linear whitespace */
				g_string_append_c (out, ' ');
			} else {
				len = word->end - word->start;
				start = word->start;
			}
			
			if (word->encoding == 1)
				rfc2047_encode_word (out, start, len, "iso-8859-1", safemask);
			else
				rfc2047_encode_word (out, start, len, g_mime_charset_best (start, len), safemask);
			break;
		}
		
		g_free (prev);
		prev = word;
		word = word->next;
	}
	
	g_free (prev);
	
	outstr = out->str;
	g_string_free (out, FALSE);
	
	return outstr;
}


/**
 * g_mime_utils_header_encode_phrase:
 * @in: header to encode
 *
 * Encodes a 'phrase' header according to the rules in rfc2047.
 *
 * Returns the encoded 'phrase'. Useful for encoding internet
 * addresses.
 **/
char *
g_mime_utils_header_encode_phrase (const char *in)
{
	if (in == NULL)
		return NULL;
	
	return rfc2047_encode (in, IS_PSAFE);
}


/**
 * g_mime_utils_header_encode_text:
 * @in: header to encode
 *
 * Encodes a 'text' header according to the rules in rfc2047.
 *
 * Returns the encoded header. Useful for encoding
 * headers like "Subject".
 **/
char *
g_mime_utils_header_encode_text (const char *in)
{
	if (in == NULL)
		return NULL;
	
	return rfc2047_encode (in, IS_ESAFE);
}


/**
 * g_mime_utils_8bit_header_decode:
 * @in: header to decode
 *
 * Decodes an rfc2047 encoded header.
 *
 * WARNING: This function is deprecated. Use
 * g_mime_utils_header_decode_text() instead.
 *
 * Returns the decoded header (which will be in UTF-8 if at all
 * possible).
 **/
char *
g_mime_utils_8bit_header_decode (const char *in)
{
	return g_mime_utils_header_decode_text (in);
}


/**
 * g_mime_utils_8bit_header_encode:
 * @in: header to encode
 *
 * Encodes a 'text' header according to the rules in rfc2047.
 *
 * WARNING: This function is deprecated. Use
 * g_mime_utils_header_encode_text() instead.
 *
 * Returns the encoded header. Useful for encoding
 * headers like "Subject".
 **/
char *
g_mime_utils_8bit_header_encode (const char *in)
{
	return g_mime_utils_header_encode_text (in);
}


/**
 * g_mime_utils_8bit_header_encode_phrase:
 * @in: header to encode
 *
 * Encodes a 'phrase' header according to the rules in rfc2047.
 *
 * WARNING: This function is deprecated. Use
 * g_mime_utils_header_encode_phrase() instead.
 *
 * Returns the encoded 'phrase'. Useful for encoding internet
 * addresses.
 **/
char *
g_mime_utils_8bit_header_encode_phrase (const char *in)
{
	return g_mime_utils_header_encode_phrase (in);
}


/**
 * g_mime_utils_base64_encode_close:
 * @in: input stream
 * @inlen: length of the input
 * @out: output string
 * @state: holds the number of bits that are stored in @save
 * @save: leftover bits that have not yet been encoded
 *
 * Base64 encodes the input stream to the output stream. Call this
 * when finished encoding data with #g_mime_utils_base64_encode_step
 * to flush off the last little bit.
 *
 * Returns the number of bytes encoded.
 **/
size_t
g_mime_utils_base64_encode_close (const unsigned char *in, size_t inlen, unsigned char *out, int *state, guint32 *save)
{
	unsigned char *outptr = out;
	int c1, c2;
	
	if (inlen > 0)
		outptr += g_mime_utils_base64_encode_step (in, inlen, outptr, state, save);
	
	c1 = ((unsigned char *)save)[1];
	c2 = ((unsigned char *)save)[2];
	
	switch (((unsigned char *)save)[0]) {
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
 * g_mime_utils_base64_encode_step:
 * @in: input stream
 * @inlen: length of the input
 * @out: output string
 * @state: holds the number of bits that are stored in @save
 * @save: leftover bits that have not yet been encoded
 *
 * Base64 encodes a chunk of data. Performs an 'encode step', only
 * encodes blocks of 3 characters to the output at a time, saves
 * left-over state in state and save (initialise to 0 on first
 * invocation).
 *
 * Returns the number of bytes encoded.
 **/
size_t
g_mime_utils_base64_encode_step (const unsigned char *in, size_t inlen, unsigned char *out, int *state, guint32 *save)
{
	const register unsigned char *inptr;
	register unsigned char *outptr;
	
	if (inlen <= 0)
		return 0;
	
	inptr = in;
	outptr = out;
	
	if (inlen + ((unsigned char *)save)[0] > 2) {
		const unsigned char *inend = in + inlen - 2;
		register int c1 = 0, c2 = 0, c3 = 0;
		register int already;
		
		already = *state;
		
		switch (((char *)save)[0]) {
		case 1:	c1 = ((unsigned char *)save)[1]; goto skip1;
		case 2:	c1 = ((unsigned char *)save)[1];
			c2 = ((unsigned char *)save)[2]; goto skip2;
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
			*outptr++ = base64_alphabet [((c2 & 0x0f) << 2) | (c3 >> 6)];
			*outptr++ = base64_alphabet [c3 & 0x3f];
			/* this is a bit ugly ... */
			if ((++already) >= 19) {
				*outptr++ = '\n';
				already = 0;
			}
		}
		
		((unsigned char *)save)[0] = 0;
		inlen = 2 - (inptr - inend);
		*state = already;
	}
	
	d(printf ("state = %d, inlen = %d\n", (int)((char *)save)[0], inlen));
	
	if (inlen > 0) {
		register char *saveout;
		
		/* points to the slot for the next char to save */
		saveout = & (((char *)save)[1]) + ((char *)save)[0];
		
		/* inlen can only be 0 1 or 2 */
		switch (inlen) {
		case 2:	*saveout++ = *inptr++;
		case 1:	*saveout++ = *inptr++;
		}
		((char *)save)[0] += inlen;
	}
	
	d(printf ("mode = %d\nc1 = %c\nc2 = %c\n",
		  (int)((char *)save)[0],
		  (int)((char *)save)[1],
		  (int)((char *)save)[2]));
	
	return (outptr - out);
}


/**
 * g_mime_utils_base64_decode_step:
 * @in: input stream
 * @inlen: max length of data to decode
 * @out: output stream
 * @state: holds the number of bits that are stored in @save
 * @save: leftover bits that have not yet been decoded
 *
 * Decodes a chunk of base64 encoded data.
 *
 * Returns the number of bytes decoded (which have been dumped in @out).
 **/
size_t
g_mime_utils_base64_decode_step (const unsigned char *in, size_t inlen, unsigned char *out, int *state, guint32 *save)
{
	const register unsigned char *inptr;
	register unsigned char *outptr;
	const unsigned char *inend;
	register guint32 saved;
	unsigned char c;
	int i;
	
	inend = in + inlen;
	outptr = out;
	
	/* convert 4 base64 bytes to 3 normal bytes */
	saved = *save;
	i = *state;
	inptr = in;
	while (inptr < inend) {
		c = gmime_base64_rank[*inptr++];
		if (c != 0xff) {
			saved = (saved << 6) | c;
			i++;
			if (i == 4) {
				*outptr++ = saved >> 16;
				*outptr++ = saved >> 8;
				*outptr++ = saved;
				i = 0;
			}
		}
	}
	
	*save = saved;
	*state = i;
	
	/* quick scan back for '=' on the end somewhere */
	/* fortunately we can drop 1 output char for each trailing = (upto 2) */
	i = 2;
	while (inptr > in && i) {
		inptr--;
		if (gmime_base64_rank[*inptr] != 0xff) {
			if (*inptr == '=' && outptr > out)
				outptr--;
			i--;
		}
	}
	
	/* if i != 0 then there is a truncation error! */
	return (outptr - out);
}


/**
 * g_mime_utils_uuencode_close:
 * @in: input stream
 * @inlen: input stream length
 * @out: output stream
 * @uubuf: temporary buffer of 60 bytes
 * @state: holds the number of bits that are stored in @save
 * @save: leftover bits that have not yet been encoded
 *
 * Uuencodes a chunk of data. Call this when finished encoding data
 * with #g_mime_utils_uuencode_step to flush off the last little bit.
 *
 * Returns the number of bytes encoded.
 **/
size_t
g_mime_utils_uuencode_close (const unsigned char *in, size_t inlen, unsigned char *out, unsigned char *uubuf, int *state, guint32 *save)
{
	register unsigned char *outptr, *bufptr;
	register guint32 saved;
	int uulen, uufill, i;
	
	outptr = out;
	
	if (inlen > 0)
		outptr += g_mime_utils_uuencode_step (in, inlen, out, uubuf, state, save);
	
	uufill = 0;
	
	saved = *save;
	i = *state & 0xff;
	uulen = (*state >> 8) & 0xff;
	
	bufptr = uubuf + ((uulen / 3) * 4);
	
	if (i > 0) {
		while (i < 3) {
			saved <<= 8 | 0;
			uufill++;
			i++;
		}
		
		if (i == 3) {
			/* convert 3 normal bytes into 4 uuencoded bytes */
			unsigned char b0, b1, b2;
			
			b0 = saved >> 16;
			b1 = saved >> 8 & 0xff;
			b2 = saved & 0xff;
			
			*bufptr++ = GMIME_UUENCODE_CHAR ((b0 >> 2) & 0x3f);
			*bufptr++ = GMIME_UUENCODE_CHAR (((b0 << 4) | ((b1 >> 4) & 0xf)) & 0x3f);
			*bufptr++ = GMIME_UUENCODE_CHAR (((b1 << 2) | ((b2 >> 6) & 0x3)) & 0x3f);
			*bufptr++ = GMIME_UUENCODE_CHAR (b2 & 0x3f);
			
			i = 0;
			saved = 0;
			uulen += 3;
		}
	}
	
	if (uulen > 0) {
		int cplen = ((uulen / 3) * 4);
		
		*outptr++ = GMIME_UUENCODE_CHAR ((uulen - uufill) & 0xff);
		memcpy (outptr, uubuf, cplen);
		outptr += cplen;
		*outptr++ = '\n';
		uulen = 0;
	}
	
	*outptr++ = GMIME_UUENCODE_CHAR (uulen & 0xff);
	*outptr++ = '\n';
	
	*save = 0;
	*state = 0;
	
	return (outptr - out);
}


/**
 * g_mime_utils_uuencode_step:
 * @in: input stream
 * @inlen: input stream length
 * @out: output stream
 * @uubuf: temporary buffer of 60 bytes
 * @state: holds the number of bits that are stored in @save
 * @save: leftover bits that have not yet been encoded
 *
 * Uuencodes a chunk of data. Performs an 'encode step', only encodes
 * blocks of 45 characters to the output at a time, saves left-over
 * state in @uubuf, @state and @save (initialize to 0 on first
 * invocation).
 *
 * Returns the number of bytes encoded.
 **/
size_t
g_mime_utils_uuencode_step (const unsigned char *in, size_t inlen, unsigned char *out, unsigned char *uubuf, int *state, guint32 *save)
{
	register unsigned char *outptr, *bufptr;
	const register unsigned char *inptr;
	const unsigned char *inend;
	register guint32 saved;
	int uulen, i;
	
	saved = *save;
	i = *state & 0xff;
	uulen = (*state >> 8) & 0xff;
	
	inptr = in;
	inend = in + inlen;
	
	outptr = out;
	
	bufptr = uubuf + ((uulen / 3) * 4);
	
	while (inptr < inend) {
		while (uulen < 45 && inptr < inend) {
			while (i < 3 && inptr < inend) {
				saved = (saved << 8) | *inptr++;
				i++;
			}
			
			if (i == 3) {
				/* convert 3 normal bytes into 4 uuencoded bytes */
				unsigned char b0, b1, b2;
				
				b0 = saved >> 16;
				b1 = saved >> 8 & 0xff;
				b2 = saved & 0xff;
				
				*bufptr++ = GMIME_UUENCODE_CHAR ((b0 >> 2) & 0x3f);
				*bufptr++ = GMIME_UUENCODE_CHAR (((b0 << 4) | ((b1 >> 4) & 0xf)) & 0x3f);
				*bufptr++ = GMIME_UUENCODE_CHAR (((b1 << 2) | ((b2 >> 6) & 0x3)) & 0x3f);
				*bufptr++ = GMIME_UUENCODE_CHAR (b2 & 0x3f);
				
				i = 0;
				saved = 0;
				uulen += 3;
			}
		}
		
		if (uulen >= 45) {
			*outptr++ = GMIME_UUENCODE_CHAR (uulen & 0xff);
			memcpy (outptr, uubuf, ((uulen / 3) * 4));
			outptr += ((uulen / 3) * 4);
			*outptr++ = '\n';
			uulen = 0;
			bufptr = uubuf;
		}
	}
	
	*save = saved;
	*state = ((uulen & 0xff) << 8) | (i & 0xff);
	
	return (outptr - out);
}


/**
 * g_mime_utils_uudecode_step:
 * @in: input stream
 * @inlen: max length of data to decode
 * @out: output stream
 * @state: holds the number of bits that are stored in @save
 * @save: leftover bits that have not yet been decoded
 *
 * Uudecodes a chunk of data. Performs a 'decode step' on a chunk of
 * uuencoded data. Assumes the "begin mode filename" line has
 * been stripped off.
 *
 * Returns the number of bytes decoded.
 **/
size_t
g_mime_utils_uudecode_step (const unsigned char *in, size_t inlen, unsigned char *out, int *state, guint32 *save)
{
	const register unsigned char *inptr;
	register unsigned char *outptr;
	const unsigned char *inend;
	unsigned char ch;
	register guint32 saved;
	gboolean last_was_eoln;
	int uulen, i;
	
	if (*state & GMIME_UUDECODE_STATE_END)
		return 0;
	
	saved = *save;
	i = *state & 0xff;
	uulen = (*state >> 8) & 0xff;
	if (uulen == 0)
		last_was_eoln = TRUE;
	else
		last_was_eoln = FALSE;
	
	inend = in + inlen;
	outptr = out;
	
	inptr = in;
	while (inptr < inend) {
		if (*inptr == '\n') {
			last_was_eoln = TRUE;
			
			inptr++;
			continue;
		} else if (!uulen || last_was_eoln) {
			/* first octet on a line is the uulen octet */
			uulen = gmime_uu_rank[*inptr];
			last_was_eoln = FALSE;
			if (uulen == 0) {
				*state |= GMIME_UUDECODE_STATE_END;
				break;
			}
			
			inptr++;
			continue;
		}
		
		ch = *inptr++;
		
		if (uulen > 0) {
			/* save the byte */
			saved = (saved << 8) | ch;
			i++;
			if (i == 4) {
				/* convert 4 uuencoded bytes to 3 normal bytes */
				unsigned char b0, b1, b2, b3;
				
				b0 = saved >> 24;
				b1 = saved >> 16 & 0xff;
				b2 = saved >> 8 & 0xff;
				b3 = saved & 0xff;
				
				if (uulen >= 3) {
					*outptr++ = gmime_uu_rank[b0] << 2 | gmime_uu_rank[b1] >> 4;
					*outptr++ = gmime_uu_rank[b1] << 4 | gmime_uu_rank[b2] >> 2;
				        *outptr++ = gmime_uu_rank[b2] << 6 | gmime_uu_rank[b3];
				} else {
					if (uulen >= 1) {
						*outptr++ = gmime_uu_rank[b0] << 2 | gmime_uu_rank[b1] >> 4;
					}
					if (uulen >= 2) {
						*outptr++ = gmime_uu_rank[b1] << 4 | gmime_uu_rank[b2] >> 2;
					}
				}
				
				i = 0;
				saved = 0;
				uulen -= 3;
			}
		} else {
			break;
		}
	}
	
	*save = saved;
	*state = (*state & GMIME_UUDECODE_STATE_MASK) | ((uulen & 0xff) << 8) | (i & 0xff);
	
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
 * Quoted-printable encodes a block of text. Call this when finished
 * encoding data with #g_mime_utils_quoted_encode_step to flush off
 * the last little bit.
 *
 * Returns the number of bytes encoded.
 **/
size_t
g_mime_utils_quoted_encode_close (const unsigned char *in, size_t inlen, unsigned char *out, int *state, guint32 *save)
{
	register unsigned char *outptr = out;
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
 * g_mime_utils_quoted_encode_step:
 * @in: input stream
 * @inlen: length of the input
 * @out: output string
 * @state: holds the number of bits that are stored in @save
 * @save: leftover bits that have not yet been encoded
 *
 * Quoted-printable encodes a block of text. Performs an 'encode
 * step', saves left-over state in state and save (initialise to -1 on
 * first invocation).
 *
 * Returns the number of bytes encoded.
 **/
size_t
g_mime_utils_quoted_encode_step (const unsigned char *in, size_t inlen, unsigned char *out, int *state, guint32 *save)
{
	const register unsigned char *inptr, *inend;
	register unsigned char *outptr;
	register guint32 sofar = *save;  /* keeps track of how many chars on a line */
	register int last = *state;  /* keeps track if last char to end was a space cr etc */
	unsigned char c;
	
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
				if (is_qpsafe (last)) {
					*outptr++ = last;
					sofar++;
				} else {
					*outptr++ = '=';
					*outptr++ = tohex[(last >> 4) & 0xf];
					*outptr++ = tohex[last & 0xf];
					sofar += 3;
				}
			}
			
			if (is_qpsafe (c)) {
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
 * g_mime_utils_quoted_decode_step:
 * @in: input stream
 * @inlen: max length of data to decode
 * @out: output stream
 * @state: holds the number of bits that are stored in @save
 * @save: leftover bits that have not yet been decoded
 *
 * Decodes a block of quoted-printable encoded data. Performs a
 * 'decode step' on a chunk of QP encoded data.
 *
 * Returns the number of bytes decoded.
 **/
size_t
g_mime_utils_quoted_decode_step (const unsigned char *in, size_t inlen, unsigned char *out, int *state, guint32 *save)
{
	/* FIXME: this does not strip trailing spaces from lines (as
	 * it should, rfc 2045, section 6.7) Should it also
	 * canonicalise the end of line to CR LF??
	 *
	 * Note: Trailing rubbish (at the end of input), like = or =x
	 * or =\r will be lost.
	 */
	const register unsigned char *inptr;
	register unsigned char *outptr;
	const unsigned char *inend;
	unsigned char c;
	guint32 isave;
	int istate;
	
	inend = in + inlen;
	outptr = out;
	
	d(printf ("quoted-printable, decoding text '%.*s'\n", inlen, in));
	
	istate = *state;
	isave = *save;
	inptr = in;
	while (inptr < inend) {
		switch (istate) {
		case 0:
			while (inptr < inend) {
				c = *inptr++;
				/* FIXME: use a specials table to avoid 3 comparisons for the common case */
				if (c == '=') { 
					istate = 1;
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
				istate = 0;
			} else {
				isave = c;
				istate = 2;
			}
			break;
		case 2:
			c = *inptr++;
			if (isxdigit (c) && isxdigit (isave)) {
				c = toupper ((int) c);
				isave = toupper ((int) isave);
				*outptr++ = (((isave >= 'A' ? isave - 'A' + 10 : isave - '0') & 0x0f) << 4)
					| ((c >= 'A' ? c - 'A' + 10 : c - '0') & 0x0f);
			} else if (c == '\n' && isave == '\r') {
				/* soft break ... canonical end of line */
			} else {
				/* just output the data */
				*outptr++ = '=';
				*outptr++ = isave;
				*outptr++ = c;
			}
			istate = 0;
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
			istate = 0;
			break;
#endif
		}
	}
	
	*state = istate;
	*save = isave;
	
	return (outptr - out);
}
