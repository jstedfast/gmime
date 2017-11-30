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

#define _GNU_SOURCE

#include <glib.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef HAVE_SYS_PARAM_H
#include <sys/param.h>      /* for MAXHOSTNAMELEN */
#else
#define MAXHOSTNAMELEN 64
#endif
#ifdef HAVE_UTSNAME_DOMAINNAME
#include <sys/utsname.h>    /* for uname() */
#endif
#include <sys/types.h>
#include <unistd.h>         /* Unix header for getpid() */
#ifdef G_OS_WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#endif
#ifdef HAVE_NETDB_H
#include <netdb.h>
#endif
#include <ctype.h>
#include <errno.h>

#ifdef LIBIDN
#include <idna.h>
#endif

#include "gmime-utils.h"
#include "gmime-common.h"
#include "gmime-internal.h"
#include "gmime-table-private.h"
#include "gmime-parse-utils.h"
#include "gmime-part.h"
#include "gmime-charset.h"
#include "gmime-iconv.h"
#include "gmime-iconv-utils.h"

#ifdef ENABLE_WARNINGS
#define w(x) x
#else
#define w(x)
#endif /* ENABLE_WARNINGS */

#define d(x)


/**
 * SECTION: gmime-utils
 * @title: gmime-utils
 * @short_description: MIME utility functions
 * @see_also:
 *
 * Utility functions to parse, encode and decode various MIME tokens
 * and encodings.
 **/

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

static char base36[36] = {
	'0', '1', '2', '3', '4', '5', '6', '7',
	'8', '9', 'A', 'B', 'C', 'D', 'E', 'F',
	'G', 'H', 'I', 'J', 'K', 'L', 'M', 'N',
	'O', 'P', 'Q', 'R', 'S', 'T', 'U', 'V',
	'W', 'X', 'Y', 'Z'
};

static unsigned char tohex[16] = {
	'0', '1', '2', '3', '4', '5', '6', '7',
	'8', '9', 'A', 'B', 'C', 'D', 'E', 'F'
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

/* Timezone values defined in rfc5322 */
static struct {
	const char *name;
	int offset;
} tz_offsets [] = {
	{ "UT",       0 },
	{ "GMT",      0 },
	{ "EDT",   -400 },
	{ "EST",   -500 },
	{ "CDT",   -500 },
	{ "CST",   -600 },
	{ "MDT",   -600 },
	{ "MST",   -700 },
	{ "PDT",   -700 },
	{ "PST",   -800 },
	/* Note: rfc822 got the signs backwards for the military
	 * timezones so some sending clients may mistakenly use the
	 * wrong values. */
	{ "A",      100 },
	{ "B",      200 },
	{ "C",      300 },
	{ "D",      400 },
	{ "E",      500 },
	{ "F",      600 },
	{ "G",      700 },
	{ "H",      800 },
	{ "I",      900 },
	{ "K",     1000 },
	{ "L",     1100 },
	{ "M",     1200 },
	{ "N",     -100 },
	{ "O",     -200 },
	{ "P",     -300 },
	{ "Q",     -400 },
	{ "R",     -500 },
	{ "S",     -600 },
	{ "T",     -700 },
	{ "U",     -800 },
	{ "V",     -900 },
	{ "W",    -1000 },
	{ "X",    -1100 },
	{ "Y",    -1200 },
	{ "Z",        0 },
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
 * @date: a #GDateTime
 *
 * Allocates a string buffer containing the rfc822 formatted date
 * string represented by @date.
 *
 * Returns: a valid string representation of the date.
 **/
char *
g_mime_utils_header_format_date (GDateTime *date)
{
	int wday, year, month, day, hour, min, sec, tz_offset, sign;
	GTimeSpan tz;
	
	g_return_val_if_fail (date != NULL, NULL);
	
	wday = g_date_time_get_day_of_week (date);
	year = g_date_time_get_year (date);
	month = g_date_time_get_month (date);
	day = g_date_time_get_day_of_month (date);
	hour = g_date_time_get_hour (date);
	min = g_date_time_get_minute (date);
	sec = g_date_time_get_second (date);
	tz = g_date_time_get_utc_offset (date);
	
	sign = tz < 0 ? -1 : 1;
	tz *= sign;
	
	tz_offset = 100 * (tz / G_TIME_SPAN_HOUR);
	tz_offset += (tz % G_TIME_SPAN_HOUR) / G_TIME_SPAN_MINUTE;
	tz_offset *= sign;
	
	return g_strdup_printf ("%s, %02d %s %04d %02d:%02d:%02d %+05d",
				tm_days[wday % 7], day, tm_months[month - 1],
				year, hour, min, sec, tz_offset);
}

/* This is where it gets ugly... */

typedef struct _date_token {
	struct _date_token *next;
	unsigned char mask;
	const char *start;
	size_t len;
} date_token;

#define date_token_free(tok) g_slice_free (date_token, tok)
#define date_token_new() g_slice_new (date_token)

static date_token *
datetok (const char *date)
{
	date_token tokens, *token, *tail;
	const char *start, *end;
        unsigned char mask;
	
	tail = (date_token *) &tokens;
	tokens.next = NULL;
	
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
			token = date_token_new ();
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
	
	return tokens.next;
}

static int
decode_int (const char *in, size_t inlen)
{
	register const char *inptr = in;
	const char *inend = in + inlen;
	int val = 0;
	
	while (inptr < inend) {
		if (!(*inptr >= '0' && *inptr <= '9'))
			return -1;
		if (val > (INT_MAX - (*inptr - '0')) / 10)
			return -1;
		val = (val * 10) + (*inptr - '0');
		inptr++;
	}
	
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
			return i + 1;
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
	int *val, max, colons = 0;
	const char *inend;
	
	*hour = *min = *sec = 0;
	
	inend = in + inlen;
	val = hour;
	max = 23;
	for (inptr = in; inptr < inend; inptr++) {
		if (*inptr == ':') {
			colons++;
			switch (colons) {
			case 1:
				val = min;
				max = 59;
				break;
			case 2:
				val = sec;
				max = 60;
				break;
			default:
				return FALSE;
			}
		} else if (!(*inptr >= '0' && *inptr <= '9'))
			return FALSE;
		else {
			*val = (*val * 10) + (*inptr - '0');
			if (*val > max)
				return FALSE;
		}
	}
	
	return TRUE;
}

static GTimeZone *
get_tzone (date_token **token)
{
	const char *inptr, *inend;
	char tzone[8];
	size_t len, n;
	int value, i;
	guint t;
	
	for (i = 0; *token && i < 2; *token = (*token)->next, i++) {
		inptr = (*token)->start;
		len = (*token)->len;
		inend = inptr + len;
		
		if (len >= 6)
			continue;
		
		if (len == 5 && (*inptr == '+' || *inptr == '-')) {
			if ((value = decode_int (inptr + 1, len - 1)) == -1)
				return NULL;
			
			memcpy (tzone, inptr, len);
			tzone[len] = '\0';
			
			return g_time_zone_new (tzone);
		}
		
		if (*inptr == '(') {
			inptr++;
			if (*(inend - 1) == ')')
				len -= 2;
			else
				len--;
		}
		
		for (t = 0; t < G_N_ELEMENTS (tz_offsets); t++) {
			n = strlen (tz_offsets[t].name);
			
			if (n != len || strncmp (inptr, tz_offsets[t].name, n) != 0)
				continue;
			
			snprintf (tzone, 6, "%+05d", tz_offsets[t].offset);
			
			return g_time_zone_new (tzone);
		}
	}
	
	return NULL;
}

static GDateTime *
parse_rfc822_date (date_token *tokens)
{
	int year, month, day, hour, min, sec, n;
	GTimeZone *tz = NULL;
	date_token *token;
	GDateTime *date;
	
	token = tokens;
	
	year = month = day = hour = min = sec = 0;
	
	if ((n = get_wday (token->start, token->len)) != -1) {
		/* not all dates may have this... */
		token = token->next;
	}
	
	/* get the mday */
	if (!token || (n = get_mday (token->start, token->len)) == -1)
		return NULL;
	
	token = token->next;
	day = n;
	
	/* get the month */
	if (!token || (n = get_month (token->start, token->len)) == -1)
		return NULL;
	
	token = token->next;
	month = n;
	
	/* get the year */
	if (!token || (n = get_year (token->start, token->len)) == -1)
		return NULL;
	
	token = token->next;
	year = n;
	
	/* get the hour/min/sec */
	if (!token || !get_time (token->start, token->len, &hour, &min, &sec))
		return NULL;
	
	token = token->next;
	
	/* get the timezone */
	if (!token || !(tz = get_tzone (&token))) {
		/* I guess we assume tz is GMT? */
		tz = g_time_zone_new_utc ();
	}
	
	date = g_date_time_new (tz, year, month, day, hour, min, (gdouble) sec);
	g_time_zone_unref (tz);
	
	return date;
}


#define date_token_mask(t)  (((date_token *) t)->mask)
#define is_numeric(t)       ((date_token_mask (t) & DATE_TOKEN_NON_NUMERIC) == 0)
#define is_weekday(t)       ((date_token_mask (t) & DATE_TOKEN_NON_WEEKDAY) == 0)
#define is_month(t)         ((date_token_mask (t) & DATE_TOKEN_NON_MONTH) == 0)
#define is_time(t)          (((date_token_mask (t) & DATE_TOKEN_NON_TIME) == 0) && (date_token_mask (t) & DATE_TOKEN_HAS_COLON))
#define is_tzone_alpha(t)   ((date_token_mask (t) & DATE_TOKEN_NON_TIMEZONE_ALPHA) == 0)
#define is_tzone_numeric(t) (((date_token_mask (t) & DATE_TOKEN_NON_TIMEZONE_NUMERIC) == 0) && (date_token_mask (t) & DATE_TOKEN_HAS_SIGN))
#define is_tzone(t)         (is_tzone_alpha (t) || is_tzone_numeric (t))

#define YEAR    (1 << 0)
#define MONTH   (1 << 1)
#define DAY     (1 << 2)
#define WEEKDAY (1 << 3)
#define TIME    (1 << 4)
#define TZONE   (1 << 5)

static GDateTime *
parse_broken_date (date_token *tokens)
{
	int year, month, day, hour, min, sec, n;
	GTimeZone *tz = NULL;
	date_token *token;
	GDateTime *date;
	int mask;
	
	year = month = day = hour = min = sec = 0;
	mask = 0;
	
	token = tokens;
	while (token) {
		if (is_weekday (token) && !(mask & WEEKDAY)) {
			if ((n = get_wday (token->start, token->len)) != -1) {
				d(printf ("weekday; "));
				mask |= WEEKDAY;
				goto next;
			}
		}
		
		if (is_month (token) && !(mask & MONTH)) {
			if ((n = get_month (token->start, token->len)) != -1) {
				d(printf ("month; "));
				mask |= MONTH;
				month = n;
				goto next;
			}
		}
		
		if (is_time (token) && !(mask & TIME)) {
			if (get_time (token->start, token->len, &hour, &min, &sec)) {
				d(printf ("time; "));
				mask |= TIME;
				goto next;
			}
		}
		
		if (is_tzone (token) && !(mask & TZONE)) {
			date_token *t = token;
			
			if ((tz = get_tzone (&t))) {
				d(printf ("tzone; "));
				mask |= TZONE;
				goto next;
			}
		}
		
		if (is_numeric (token)) {
			if (token->len == 4 && !(mask & YEAR)) {
				if ((n = get_year (token->start, token->len)) != -1) {
					d(printf ("year; "));
					mask |= YEAR;
					year = n;
					goto next;
				}
			} else {
				/* Note: assumes MM-DD-YY ordering if '0 < MM < 12' holds true */
				if (!(mask & MONTH) && token->next && is_numeric (token->next)) {
					if ((n = decode_int (token->start, token->len)) > 12) {
						goto mday;
					} else if (n > 0) {
						d(printf ("mon; "));
						mask |= MONTH;
						month = n;
					}
					goto next;
				} else if (!(mask & DAY) && (n = get_mday (token->start, token->len)) != -1) {
				mday:
					d(printf ("mday; "));
					mask |= DAY;
					day = n;
					goto next;
				} else if (!(mask & YEAR)) {
					if ((n = get_year (token->start, token->len)) != -1) {
						d(printf ("2-digit year; "));
						mask |= YEAR;
						year = n;
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
	
	if (!(mask & (YEAR | MONTH | DAY | TIME))) {
		if (tz != NULL)
			g_time_zone_unref (tz);
		
		return NULL;
	}
	
	if (tz == NULL)
		tz = g_time_zone_new_utc ();
	
	date = g_date_time_new (tz, year, month, day, hour, min, (gdouble) sec);
	g_time_zone_unref (tz);
	
	return date;
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
 * @str: input date string
 *
 * Parses the rfc822 date string.
 *
 * Returns: (nullable) (transfer full): the #GDateTime representation of the date
 * string specified by @str or %NULL on error.
 **/
GDateTime *
g_mime_utils_header_decode_date (const char *str)
{
	date_token *token, *tokens;
	GDateTime *date;
	
	if (!(tokens = datetok (str)))
		return NULL;
	
	if (!(date = parse_rfc822_date (tokens)))
		date = parse_broken_date (tokens);
	
	/* cleanup */
	while (tokens) {
		token = tokens;
		tokens = tokens->next;
		date_token_free (token);
	}
	
	return date;
}


/**
 * g_mime_utils_generate_message_id:
 * @fqdn: Fully qualified domain name
 *
 * Generates a unique Message-Id.
 *
 * Returns: a unique string in an addr-spec format suitable for use as
 * a Message-Id.
 **/
char *
g_mime_utils_generate_message_id (const char *fqdn)
{
	const char *hostname = NULL;
	unsigned char block[8];
	unsigned long value;
	char *name = NULL;
	GString *msgid;
	char *ascii;
	int i;
	
	if (!fqdn) {
#ifdef HAVE_UTSNAME_DOMAINNAME
		struct utsname unam;
		
		uname (&unam);
		
		hostname = unam.nodename;
		
		if (unam.domainname[0])
			name = g_strdup_printf ("%s.%s", hostname, unam.domainname);
#else /* ! HAVE_UTSNAME_DOMAINNAME */
		char host[MAXHOSTNAMELEN + 1];
		
#ifdef HAVE_GETHOSTNAME
		host[MAXHOSTNAMELEN] = '\0';
		if (gethostname (host, MAXHOSTNAMELEN) == 0) {
#ifdef HAVE_GETDOMAINNAME
			size_t domainlen = MAXHOSTNAMELEN;
			char *domain;
			int rv;
			
			domain = g_malloc (domainlen);
			
			while ((rv = getdomainname (domain, domainlen)) == -1 && errno == EINVAL) {
				domainlen += MAXHOSTNAMELEN;
				domain = g_realloc (domain, domainlen);
			}
			
			if (rv == 0 && domain[0]) {
				if (host[0]) {
					name = g_strdup_printf ("%s.%s", host, domain);
					g_free (domain);
				} else {
					name = domain;
				}
			}
#endif /* HAVE_GETDOMAINNAME */
		} else {
			host[0] = '\0';
		}
#endif /* HAVE_GETHOSTNAME */
		hostname = host;
#endif /* HAVE_UTSNAME_DOMAINNAME */
		
#ifdef HAVE_GETADDRINFO
		if (!name && hostname[0]) {
			/* we weren't able to get a domain name */
			struct addrinfo hints, *res;
			
			memset (&hints, 0, sizeof (hints));
			hints.ai_flags = AI_CANONNAME;
			
			if (getaddrinfo (hostname, NULL, &hints, &res) == 0) {
				name = g_strdup (res->ai_canonname);
				freeaddrinfo (res);
			}
		}
#endif /* HAVE_GETADDRINFO */
		
		fqdn = name != NULL ? name : (hostname[0] ? hostname : "localhost.localdomain");
	}
	
	value = (unsigned long) time (NULL);
	g_mime_read_random_pool (block, 8);
	msgid = g_string_new ("");
	
	do {
		g_string_append_c (msgid, base36[(int) (value % 36)]);
		value /= 36;
	} while (value != 0);
	
	g_string_append_c (msgid, '.');
	
	value = 0;
	for (i = 0; i < 8; i++)
		value = (value << 8) | block[i];
	
	do {
		g_string_append_c (msgid, base36[(int) (value % 36)]);
		value /= 36;
	} while (value != 0);
	
	g_string_append_c (msgid, '@');
	
#ifdef LIBIDN
	if (idna_to_ascii_8z (fqdn, &ascii, 0) == IDNA_SUCCESS) {
		g_string_append (msgid, ascii);
		free (ascii);
	} else {
		g_string_append (msgid, fqdn);
	}
#else
	g_string_append (msgid, fqdn);
#endif
	
	g_free (name);
	
	return g_string_free (msgid, FALSE);
}


/**
 * g_mime_utils_decode_message_id:
 * @message_id: string containing a message-id
 *
 * Decodes a msg-id as defined by rfc822.
 *
 * Returns: the addr-spec portion of the msg-id.
 **/
char *
g_mime_utils_decode_message_id (const char *message_id)
{
	g_return_val_if_fail (message_id != NULL, NULL);
	
	return decode_msgid (&message_id);
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
 * @str: input string
 *
 * Quotes @string as needed according to the rules in rfc2045.
 * 
 * Returns: an allocated string containing the escaped and quoted (if
 * needed to be) input string. The decision to quote the string is
 * based on whether or not the input string contains any 'tspecials'
 * as defined by rfc2045.
 **/
char *
g_mime_utils_quote_string (const char *str)
{
	gboolean quote;
	const char *c;
	char *qstring;
	GString *out;
	
	out = g_string_new ("");
	
	if ((quote = need_quotes (str)))
		g_string_append_c (out, '"');
	
	for (c = str; *c; c++) {
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
 * g_mime_utils_unquote_string:
 * @str: input string
 * 
 * Unquotes and unescapes a string.
 **/
void
g_mime_utils_unquote_string (char *str)
{
	/* if the string is quoted, unquote it */
	register char *inptr = str;
	int escaped = FALSE;
	int quoted = FALSE;
	
	if (!str)
		return;
	
	while (*inptr) {
		if (*inptr == '\\') {
			if (escaped)
				*str++ = *inptr++;
			else
				inptr++;
			escaped = !escaped;
		} else if (*inptr == '"') {
			if (escaped) {
				*str++ = *inptr++;
				escaped = FALSE;
			} else {
				quoted = !quoted;
				inptr++;
			}
		} else {
			*str++ = *inptr++;
			escaped = FALSE;
		}
	}
	
	*str = '\0';
}


/**
 * g_mime_utils_text_is_8bit:
 * @text: (array length=len) (element-type guint8): text to check for 8bit chars
 * @len: text length
 *
 * Determines if @text contains 8bit characters within the first @len
 * bytes.
 *
 * Returns: %TRUE if the text contains 8bit characters or %FALSE
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
 * @text: (array length=len) (element-type guint8): text to encode
 * @len: text length
 *
 * Determines the best content encoding for the first @len bytes of
 * @text.
 *
 * Returns: a #GMimeContentEncoding that is determined to be the best
 * encoding type for the specified block of text. ("best" in this
 * particular case means smallest output size)
 **/
GMimeContentEncoding
g_mime_utils_best_encoding (const unsigned char *text, size_t len)
{
	const unsigned char *ch, *inend;
	size_t count = 0;
	
	inend = text + len;
	for (ch = text; ch < inend; ch++)
		if (*ch > (unsigned char) 127)
			count++;
	
	if ((float) count <= len * 0.17)
		return GMIME_CONTENT_ENCODING_QUOTEDPRINTABLE;
	else
		return GMIME_CONTENT_ENCODING_BASE64;
}


/**
 * charset_convert:
 * @cd: iconv converter
 * @inbuf: input text buffer to convert
 * @inleft: length of the input buffer
 * @outp: pointer to output buffer
 * @outlenp: pointer to output buffer length
 * @ninval: the number of invalid bytes in @inbuf
 *
 * Converts the input buffer from one charset to another using the
 * @cd. On completion, @outp will point to the output buffer
 * containing the converted text (nul-terminated), @outlenp will be
 * the size of the @outp buffer (note: not the strlen() of @outp) and
 * @ninval will contain the number of bytes which could not be
 * converted.
 *
 * Bytes which cannot be converted from @inbuf will appear as '?'
 * characters in the output buffer.
 *
 * If *@outp is non-NULL, then it is assumed that it points to a
 * pre-allocated buffer of length *@outlenp. This is done so that the
 * same output buffer can be reused multiple times.
 *
 * Returns: the string length of the output buffer.
 **/
static size_t
charset_convert (iconv_t cd, const char *inbuf, size_t inleft, char **outp, size_t *outlenp, size_t *ninval)
{
	size_t outlen, outleft, rc, n = 0;
	char *outbuf, *out;
	
	if (*outp == NULL) {
		outleft = outlen = (inleft * 2) + 16;
		outbuf = out = g_malloc (outlen + 1);
	} else {
		outleft = outlen = *outlenp;
		outbuf = out = *outp;
	}
	
	do {
		rc = iconv (cd, (char **) &inbuf, &inleft, &outbuf, &outleft);
		if (rc == (size_t) -1) {
			if (errno == EINVAL) {
				/* incomplete sequence at the end of the input buffer */
				n += inleft;
				break;
			}
			
#ifdef G_OS_WIN32
			/* seems that GnuWin32's libiconv 1.9 does not set errno in
			 * the E2BIG case, so we have to fake it */
			if (outleft <= inleft)
				errno = E2BIG;
#endif
			
			if (errno == E2BIG || outleft == 0) {
				/* need to grow the output buffer */
				outlen += (inleft * 2) + 16;
				rc = (size_t) (outbuf - out);
				out = g_realloc (out, outlen + 1);
				outleft = outlen - rc;
				outbuf = out + rc;
			}
			
			/* Note: GnuWin32's libiconv 1.9 can also set errno to ERANGE
			 * which seems to mean that it encountered a character that
			 * does not fit the specified 'from' charset. We'll handle
			 * that the same way we handle EILSEQ. */
			if (errno == EILSEQ || errno == ERANGE) {
				/* invalid or incomplete multibyte
				 * sequence in the input buffer */
				*outbuf++ = '?';
				outleft--;
				inleft--;
				inbuf++;
				n++;
			}
		}
	} while (inleft > 0);
	
	while (iconv (cd, NULL, NULL, &outbuf, &outleft) == (size_t) -1) {
		if (errno != E2BIG)
			break;
		
		outlen += 16;
		rc = (size_t) (outbuf - out);
		out = g_realloc (out, outlen + 1);
		outleft = outlen - rc;
		outbuf = out + rc;
	}
	
	*outbuf = '\0';
	
	*outlenp = outlen;
	*outp = out;
	*ninval = n;
	
	return (outbuf - out);
}


/**
 * g_mime_utils_decode_8bit:
 * @options: (nullable): a #GMimeParserOptions or %NULL
 * @text: (array length=len) (element-type guint8): input text in
 *   unknown 8bit/multibyte character set
 * @len: input text length
 *
 * Attempts to convert text in an unknown 8bit/multibyte charset into
 * UTF-8 by finding the charset which will convert the most bytes into
 * valid UTF-8 characters as possible. If no exact match can be found,
 * it will choose the best match and convert invalid byte sequences
 * into question-marks (?) in the returned string buffer.
 *
 * Returns: a UTF-8 string representation of @text.
 **/
char *
g_mime_utils_decode_8bit (GMimeParserOptions *options, const char *text, size_t len)
{
	size_t outleft, outlen, min, ninval;
	const char **charsets;
	const char *best;
	iconv_t cd;
	char *out;
	int i;
	
	g_return_val_if_fail (text != NULL, NULL);
	
	charsets = g_mime_parser_options_get_fallback_charsets (options);
	best = charsets[0];
	min = len;
	
	outleft = (len * 2) + 16;
	out = g_malloc (outleft + 1);
	
	for (i = 0; charsets[i]; i++) {
		if ((cd = g_mime_iconv_open ("UTF-8", charsets[i])) == (iconv_t) -1)
			continue;
		
		outlen = charset_convert (cd, text, len, &out, &outleft, &ninval);
		
		g_mime_iconv_close (cd);
		
		if (ninval == 0)
			return g_realloc (out, outlen + 1);
		
		if (ninval < min) {
			best = charsets[i];
			min = ninval;
		}
	}
	
	/* if we get here, then none of the charsets fit the 8bit text flawlessly...
	 * try to find the one that fit the best and use that to convert what we can,
	 * replacing any byte we can't convert with a '?' */
	
	if ((cd = g_mime_iconv_open ("UTF-8", best)) == (iconv_t) -1) {
		/* this shouldn't happen... but if we are here, then
		 * it did...  the only thing we can do at this point
		 * is replace the 8bit garbage and pray */
		register const char *inptr = text;
		const char *inend = inptr + len;
		char *outbuf = out;
		
		while (inptr < inend) {
			if (is_ascii (*inptr))
				*outbuf++ = *inptr;
			else
				*outbuf++ = '?';
			
			inptr++;
		}
		
		*outbuf++ = '\0';
		
		return g_realloc (out, (size_t) (outbuf - out));
	}
	
	outlen = charset_convert (cd, text, len, &out, &outleft, &ninval);
	
	g_mime_iconv_close (cd);
	
	return g_realloc (out, outlen + 1);
}


/* this decodes rfc2047's version of quoted-printable */
static size_t
quoted_decode (const unsigned char *in, size_t len, unsigned char *out, int *state, guint32 *save)
{
	register const unsigned char *inptr;
	register unsigned char *outptr;
	const unsigned char *inend;
	unsigned char c, c1;
	guint32 saved;
	int need;
	
	if (len == 0)
		return 0;
	
	inend = in + len;
	outptr = out;
	inptr = in;
	
	need = *state;
	saved = *save;
	
	if (need > 0) {
		if (isxdigit ((int) *inptr)) {
			if (need == 1) {
				c = g_ascii_toupper ((int) (saved & 0xff));
				c1 = g_ascii_toupper ((int) *inptr++);
				saved = 0;
				need = 0;
				
				goto decode;
			}
			
			saved = 0;
			need = 0;
			
			goto equals;
		}
		
		/* last encoded-word ended in a malformed quoted-printable sequence */
		*outptr++ = '=';
		
		if (need == 1)
			*outptr++ = (char) (saved & 0xff);
		
		saved = 0;
		need = 0;
	}
	
	while (inptr < inend) {
		c = *inptr++;
		if (c == '=') {
		equals:
			if (inend - inptr >= 2) {
				if (isxdigit ((int) inptr[0]) && isxdigit ((int) inptr[1])) {
					c = g_ascii_toupper (*inptr++);
					c1 = g_ascii_toupper (*inptr++);
				decode:
					*outptr++ = (((c >= 'A' ? c - 'A' + 10 : c - '0') & 0x0f) << 4)
						| ((c1 >= 'A' ? c1 - 'A' + 10 : c1 - '0') & 0x0f);
				} else {
					/* malformed quoted-printable sequence? */
					*outptr++ = '=';
				}
			} else {
				/* truncated payload, maybe it was split across encoded-words? */
				if (inptr < inend) {
					if (isxdigit ((int) *inptr)) {
						saved = *inptr;
						need = 1;
						break;
					} else {
						/* malformed quoted-printable sequence? */
						*outptr++ = '=';
					}
				} else {
					saved = 0;
					need = 2;
					break;
				}
			}
		} else if (c == '_') {
			/* _'s are an rfc2047 shortcut for encoding spaces */
			*outptr++ = ' ';
		} else {
			*outptr++ = c;
		}
	}
	
	*state = need;
	*save = saved;
	
	return (size_t) (outptr - out);
}


#if 0
static gboolean
is_rfc2047_token (const char *inptr, size_t len)
{
	if (len < 8 || strncmp (inptr, "=?", 2) != 0 || strncmp (inptr + len - 2, "?=", 2) != 0)
		return FALSE;
	
	inptr += 2;
	len -= 2;
	
	/* skip past the charset */
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
#endif


typedef struct _rfc2047_token {
	struct _rfc2047_token *next;
	const char *charset;
	const char *text;
	size_t length;
	char encoding;
	char is_8bit;
} rfc2047_token;

#define rfc2047_token_list_free(tokens) g_slice_free_chain (rfc2047_token, tokens, next)
#define rfc2047_token_free(token) g_slice_free (rfc2047_token, token)

static rfc2047_token *
rfc2047_token_new (const char *text, size_t len)
{
	rfc2047_token *token;
	
	token = g_slice_new0 (rfc2047_token);
	token->length = len;
	token->text = text;
	
	return token;
}

static rfc2047_token *
rfc2047_token_new_encoded_word (const char *word, size_t len)
{
	rfc2047_token *token;
	const char *payload;
	const char *charset;
	const char *inptr;
	char *buf, *lang;
	char encoding;
	size_t n;
	
	/* check that this could even be an encoded-word token */
	if (len < 7 || strncmp (word, "=?", 2) != 0 || strncmp (word + len - 2, "?=", 2) != 0)
		return NULL;
	
	/* skip over '=?' */
	inptr = word + 2;
	charset = inptr;
	
	if (*charset == '?' || *charset == '*') {
		/* this would result in an empty charset */
		return NULL;
	}
	
	/* skip to the end of the charset */
	if (!(inptr = memchr (inptr, '?', len - 2)) || inptr[2] != '?')
		return NULL;
	
	/* copy the charset into a buffer */
	n = (size_t) (inptr - charset);
	buf = g_alloca (n + 1);
	memcpy (buf, charset, n);
	buf[n] = '\0';
	charset = buf;
	
	/* rfc2231 updates rfc2047 encoded words...
	 * The ABNF given in RFC 2047 for encoded-words is:
	 *   encoded-word := "=?" charset "?" encoding "?" encoded-text "?="
	 * This specification changes this ABNF to:
	 *   encoded-word := "=?" charset ["*" language] "?" encoding "?" encoded-text "?="
	 */
	
	/* trim off the 'language' part if it's there... */
	if ((lang = strchr (charset, '*')))
		*lang = '\0';
	
	/* skip over the '?' */
	inptr++;
	
	/* make sure the first char after the encoding is another '?' */
	if (inptr[1] != '?')
		return NULL;
	
	switch (*inptr++) {
	case 'B': case 'b':
		encoding = 'B';
		break;
	case 'Q': case 'q':
		encoding = 'Q';
		break;
	default:
		return NULL;
	}
	
	/* the payload begins right after the '?' */
	payload = inptr + 1;
	
	/* find the end of the payload */
	inptr = word + len - 2;
	
	/* make sure that we don't have something like: =?iso-8859-1?Q?= */
	if (payload > inptr)
		return NULL;
	
	token = rfc2047_token_new (payload, inptr - payload);
	token->charset = g_mime_charset_iconv_name (charset);
	token->encoding = encoding;
	
	return token;
}

static rfc2047_token *
tokenize_rfc2047_phrase (GMimeParserOptions *options, const char *in, size_t *len)
{
	rfc2047_token list, *lwsp, *token, *tail;
	register const char *inptr = in;
	GMimeRfcComplianceMode mode;
	gboolean encoded = FALSE;
	const char *text, *word;
	gboolean ascii;
	size_t n;
	
	mode = g_mime_parser_options_get_rfc2047_compliance_mode (options);
	tail = (rfc2047_token *) &list;
	list.next = NULL;
	lwsp = NULL;
	
	while (*inptr != '\0') {
		text = inptr;
		while (is_lwsp (*inptr))
			inptr++;
		
		if (inptr > text)
			lwsp = rfc2047_token_new (text, inptr - text);
		else
			lwsp = NULL;
		
		word = inptr;
		ascii = TRUE;
		if (is_atom (*inptr)) {
			if (G_LIKELY (mode == GMIME_RFC_COMPLIANCE_LOOSE)) {
				/* Make an extra effort to detect and
				 * separate encoded-word tokens that
				 * have been merged with other
				 * words. */
				
				if (!strncmp (inptr, "=?", 2)) {
					inptr += 2;
					
					/* skip past the charset (if one is even declared, sigh) */
					while (*inptr && *inptr != '?') {
						ascii = ascii && is_ascii (*inptr);
						inptr++;
					}
					
					/* sanity check encoding type */
					if (inptr[0] != '?' || inptr[1] == '\0' || !strchr ("BbQq", inptr[1]) || inptr[2] != '?')
						goto non_rfc2047;
					
					inptr += 3;
					
					/* find the end of the rfc2047 encoded word token */
					while (*inptr && strncmp (inptr, "?=", 2) != 0) {
						ascii = ascii && is_ascii (*inptr);
						inptr++;
					}
					
					if (*inptr == '\0') {
						/* didn't find an end marker... */
						inptr = word + 2;
						ascii = TRUE;
						
						goto non_rfc2047;
					}
					
					inptr += 2;
				} else {
				non_rfc2047:
					/* stop if we encounter a possible rfc2047 encoded
					 * token even if it's inside another word, sigh. */
					while (is_atom (*inptr) && strncmp (inptr, "=?", 2) != 0)
						inptr++;
				}
			} else {
				while (is_atom (*inptr))
					inptr++;
			}
			
			n = (size_t) (inptr - word);
			if ((token = rfc2047_token_new_encoded_word (word, n))) {
				/* rfc2047 states that you must ignore all
				 * whitespace between encoded words */
				if (!encoded && lwsp != NULL) {
					tail->next = lwsp;
					tail = lwsp;
				} else if (lwsp != NULL) {
					rfc2047_token_free (lwsp);
				}
				
				tail->next = token;
				tail = token;
				
				encoded = TRUE;
			} else {
				/* append the lwsp and atom tokens */
				if (lwsp != NULL) {
					tail->next = lwsp;
					tail = lwsp;
				}
				
				token = rfc2047_token_new (word, n);
				token->is_8bit = ascii ? 0 : 1;
				tail->next = token;
				tail = token;
				
				encoded = FALSE;
			}
		} else {
			/* append the lwsp token */
			if (lwsp != NULL) {
				tail->next = lwsp;
				tail = lwsp;
			}
			
			ascii = TRUE;
			while (*inptr && !is_lwsp (*inptr) && !is_atom (*inptr)) {
				ascii = ascii && is_ascii (*inptr);
				inptr++;
			}
			
			n = (size_t) (inptr - word);
			token = rfc2047_token_new (word, n);
			token->is_8bit = ascii ? 0 : 1;
			
			tail->next = token;
			tail = token;
			
			encoded = FALSE;
		}
	}
	
	*len = (size_t) (inptr - in);
	
	return list.next;
}

static rfc2047_token *
tokenize_rfc2047_text (GMimeParserOptions *options, const char *in, size_t *len)
{
	rfc2047_token list, *lwsp, *token, *tail;
	register const char *inptr = in;
	GMimeRfcComplianceMode mode;
	gboolean encoded = FALSE;
	const char *text, *word;
	gboolean ascii;
	size_t n;
	
	mode = g_mime_parser_options_get_rfc2047_compliance_mode (options);
	tail = (rfc2047_token *) &list;
	list.next = NULL;
	lwsp = NULL;
	
	while (*inptr != '\0') {
		text = inptr;
		while (is_lwsp (*inptr))
			inptr++;
		
		if (inptr > text)
			lwsp = rfc2047_token_new (text, inptr - text);
		else
			lwsp = NULL;
		
		if (*inptr != '\0') {
			word = inptr;
			ascii = TRUE;
			
			if (G_LIKELY (mode == GMIME_RFC_COMPLIANCE_LOOSE)) {
				if (!strncmp (inptr, "=?", 2)) {
					inptr += 2;
					
					/* skip past the charset (if one is even declared, sigh) */
					while (*inptr && *inptr != '?') {
						ascii = ascii && is_ascii (*inptr);
						inptr++;
					}
					
					/* sanity check encoding type */
					if (inptr[0] != '?' || inptr[1] == '\0' || !strchr ("BbQq", inptr[1]) || inptr[2] != '?')
						goto non_rfc2047;
					
					inptr += 3;
					
					/* find the end of the rfc2047 encoded word token */
					while (*inptr && strncmp (inptr, "?=", 2) != 0) {
						ascii = ascii && is_ascii (*inptr);
						inptr++;
					}
					
					if (*inptr == '\0') {
						/* didn't find an end marker... */
						inptr = word + 2;
						ascii = TRUE;
						
						goto non_rfc2047;
					}
					
					inptr += 2;
				} else {
				non_rfc2047:
					/* stop if we encounter a possible rfc2047 encoded
					 * token even if it's inside another word, sigh. */
					while (*inptr && !is_lwsp (*inptr) &&
					       strncmp (inptr, "=?", 2) != 0) {
						ascii = ascii && is_ascii (*inptr);
						inptr++;
					}
				}
			} else {
				while (*inptr && !is_lwsp (*inptr)) {
					ascii = ascii && is_ascii (*inptr);
					inptr++;
				}
			}
			
			n = (size_t) (inptr - word);
			if ((token = rfc2047_token_new_encoded_word (word, n))) {
				/* rfc2047 states that you must ignore all
				 * whitespace between encoded words */
				if (!encoded && lwsp != NULL) {
					tail->next = lwsp;
					tail = lwsp;
				} else if (lwsp != NULL) {
					rfc2047_token_free (lwsp);
				}
				
				tail->next = token;
				tail = token;
				
				encoded = TRUE;
			} else {
				/* append the lwsp and atom tokens */
				if (lwsp != NULL) {
					tail->next = lwsp;
					tail = lwsp;
				}
				
				token = rfc2047_token_new (word, n);
				token->is_8bit = ascii ? 0 : 1;
				tail->next = token;
				tail = token;
				
				encoded = FALSE;
			}
		} else {
			if (lwsp != NULL) {
				/* appending trailing lwsp */
				tail->next = lwsp;
				tail = lwsp;
			}
			
			break;
		}
	}
	
	*len = (size_t) (inptr - in);
	
	return list.next;
}

static size_t
rfc2047_token_decode (rfc2047_token *token, unsigned char *outbuf, int *state, guint32 *save)
{
	const unsigned char *inbuf = (const unsigned char *) token->text;
	size_t len = token->length;
	
	if (token->encoding == 'B')
		return g_mime_encoding_base64_decode_step (inbuf, len, outbuf, state, save);
	else
		return quoted_decode (inbuf, len, outbuf, state, save);
}

static char *
rfc2047_decode_tokens (GMimeParserOptions *options, rfc2047_token *tokens, size_t buflen, const char **charset_out)
{
	rfc2047_token *token, *next;
	size_t outlen, ninval, len;
	unsigned char *outptr;
	const char *charset;
	GByteArray *outbuf;
	GString *decoded;
	char encoding;
	guint32 save;
	iconv_t cd;
	int state;
	char *str;
	
	decoded = g_string_sized_new (buflen + 1);
	outbuf = g_byte_array_sized_new (76);

	if (charset_out)
		*charset_out = NULL;
	
	token = tokens;
	while (token != NULL) {
		next = token->next;
		
		if (token->encoding) {
			/* In order to work around broken mailers, we need to combine
			 * the raw decoded content of runs of identically encoded word
			 * tokens before converting into UTF-8. */
			encoding = token->encoding;
			charset = token->charset;
			len = token->length;
			state = 0;
			save = 0;
			
			/* Note: if any token was encoded in UTF-8, return UTF-8 as the charset used;
			 * otherwise, use the first charset we encounter... */
			if (charset_out && (*charset_out == NULL || !g_ascii_strcasecmp (charset, "UTF-8")))
				*charset_out = charset;
			
			/* find the end of the run (and measure the buffer length we'll need) */
			while (next && next->encoding == encoding && !strcmp (next->charset, charset)) {
				len += next->length;
				next = next->next;
			}
			
			/* make sure our temporary output buffer is large enough... */
			if (len > outbuf->len)
				g_byte_array_set_size (outbuf, len);
			
			/* base64 / quoted-printable decode each of the tokens... */
			outptr = outbuf->data;
			outlen = 0;
			do {
				/* Note: by not resetting state/save each loop, we effectively
				 * treat the payloads as one continuous block, thus allowing
				 * us to handle cases where a hex-encoded triplet of a
				 * quoted-printable encoded payload is split between 2 or more
				 * encoded-word tokens. */
				len = rfc2047_token_decode (token, outptr, &state, &save);
				token = token->next;
				outptr += len;
				outlen += len;
			} while (token != next);
			outptr = outbuf->data;
			
			/* convert the raw decoded text into UTF-8 */
			if (!g_ascii_strcasecmp (charset, "UTF-8")) {
				/* slight optimization over going through iconv */
				str = (char *) outptr;
				len = outlen;
				
				while (!g_utf8_validate (str, len, (const char **) &str)) {
					len = outlen - (str - (char *) outptr);
					*str = '?';
				}
				
				g_string_append_len (decoded, (char *) outptr, outlen);
			} else if ((cd = g_mime_iconv_open ("UTF-8", charset)) == (iconv_t) -1) {
				w(g_warning ("Cannot convert from %s to UTF-8, header display may "
					     "be corrupt: %s", charset[0] ? charset : "unspecified charset",
					     g_strerror (errno)));
				
				str = g_mime_utils_decode_8bit (options, (char *) outptr, outlen);
				g_string_append (decoded, str);
				g_free (str);
			} else {
				str = g_malloc (outlen + 1);
				len = outlen;
				
				len = charset_convert (cd, (char *) outptr, outlen, &str, &len, &ninval);
				g_mime_iconv_close (cd);
				
				g_string_append_len (decoded, str, len);
				g_free (str);
				
#if w(!)0
				if (ninval > 0) {
					g_warning ("Failed to completely convert \"%.*s\" to UTF-8, display may be "
						   "corrupt: %s", outlen, (char *) outptr, g_strerror (errno));
				}
#endif
			}
		} else if (token->is_8bit) {
			/* *sigh* I hate broken mailers... */
			str = g_mime_utils_decode_8bit (options, token->text, token->length);
			g_string_append (decoded, str);
			g_free (str);
		} else {
			g_string_append_len (decoded, token->text, token->length);
		}
		
		token = next;
	}
	
	g_byte_array_free (outbuf, TRUE);
	
	return g_string_free (decoded, FALSE);
}


/**
 * _g_mime_utils_header_decode_text:
 * @text: header text to decode
 * @options: (nullable): a #GMimeParserOptions or %NULL
 * @charset: (optional): if non-%NULL, this will be set to the charset used in the rfc2047 encoded-word tokens
 *
 * Decodes an rfc2047 encoded 'text' header.
 *
 * Returns: a newly allocated UTF-8 string representing the the decoded
 * header.
 **/
char *
_g_mime_utils_header_decode_text (GMimeParserOptions *options, const char *text, const char **charset)
{
	rfc2047_token *tokens;
	char *decoded;
	size_t len;
	
	if (text == NULL) {
		if (charset)
			*charset = NULL;
		
		return g_strdup ("");
	}
	
	tokens = tokenize_rfc2047_text (options, text, &len);
	decoded = rfc2047_decode_tokens (options, tokens, len, charset);
	rfc2047_token_list_free (tokens);
	
	return decoded;
}


/**
 * g_mime_utils_header_decode_text:
 * @text: header text to decode
 * @options: (nullable): a #GMimeParserOptions or %NULL
 *
 * Decodes an rfc2047 encoded 'text' header.
 *
 * Returns: a newly allocated UTF-8 string representing the the decoded
 * header.
 **/
char *
g_mime_utils_header_decode_text (GMimeParserOptions *options, const char *text)
{
	return _g_mime_utils_header_decode_text (options, text, NULL);
}


/**
 * _g_mime_utils_header_decode_phrase:
 * @phrase: header to decode
 * @options: (nullable): a #GMimeParserOptions or %NULL
 * @charset: (optional): if non-%NULL, this will be set to the charset used in the rfc2047 encoded-word tokens
 *
 * Decodes an rfc2047 encoded 'phrase' header.
 *
 * Returns: a newly allocated UTF-8 string representing the the decoded
 * header.
 **/
char *
_g_mime_utils_header_decode_phrase (GMimeParserOptions *options, const char *phrase, const char **charset)
{
	rfc2047_token *tokens;
	char *decoded;
	size_t len;
	
	if (phrase == NULL) {
		if (charset)
			*charset = NULL;
		
		return g_strdup ("");
	}
	
	tokens = tokenize_rfc2047_phrase (options, phrase, &len);
	decoded = rfc2047_decode_tokens (options, tokens, len, charset);
	rfc2047_token_list_free (tokens);
	
	return decoded;
}


/**
 * g_mime_utils_header_decode_phrase:
 * @phrase: header to decode
 * @options: (nullable): a #GMimeParserOptions or %NULL
 *
 * Decodes an rfc2047 encoded 'phrase' header.
 *
 * Returns: a newly allocated UTF-8 string representing the the decoded
 * header.
 **/
char *
g_mime_utils_header_decode_phrase (GMimeParserOptions *options, const char *phrase)
{
	return _g_mime_utils_header_decode_phrase (options, phrase, NULL);
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
		} else if (c != '_' && gmime_special_table[c] & safemask) {
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
	register char *inptr, *outptr;
	iconv_t cd = (iconv_t) -1;
	unsigned char *encoded;
	size_t enclen, pos;
	char *uword = NULL;
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
	case GMIME_CONTENT_ENCODING_BASE64:
		enclen = GMIME_BASE64_ENCODE_LEN (len);
		encoded = g_alloca (enclen + 1);
		
		encoding = 'b';
		
		pos = g_mime_encoding_base64_encode_close ((const unsigned char *) word, len, encoded, &state, &save);
		encoded[pos] = '\0';
		
		/* remove \n chars as headers need to be wrapped differently */
		if (G_UNLIKELY ((inptr = strchr ((char *) encoded, '\n')))) {
			outptr = inptr++;
			while (G_LIKELY (*inptr)) {
				if (G_LIKELY (*inptr != '\n'))
					*outptr++ = *inptr;
				
				inptr++;
			}
			
			*outptr = '\0';
		}
		
		break;
	case GMIME_CONTENT_ENCODING_QUOTEDPRINTABLE:
		enclen = GMIME_QP_ENCODE_LEN (len);
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


typedef enum {
	WORD_ATOM,
	WORD_QSTRING,
	WORD_2047
} rfc822_word_t;

typedef struct _rfc822_word {
	struct _rfc822_word *next;
	const char *start, *end;
	rfc822_word_t type;
	int encoding;
} rfc822_word;

#define rfc822_word_free(word) g_slice_free (rfc822_word, word)
#define rfc822_word_new() g_slice_new (rfc822_word)

/* okay, so 'unstructured text' fields don't actually contain 'word'
 * tokens, but we can group stuff similarly... */
static rfc822_word *
rfc2047_encode_get_rfc822_words (const char *in, gboolean phrase)
{
	rfc822_word words, *tail, *word;
	rfc822_word_t type = WORD_ATOM;
	const char *inptr, *start, *last;
	int count = 0, encoding = 0;
	
	tail = (rfc822_word *) &words;
	words.next = NULL;
	
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
		
		if (c < 256 && is_blank (c)) {
			if (count > 0) {
				word = rfc822_word_new ();
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
			if (c < 128) {
				if (is_ctrl (c)) {
					type = WORD_2047;
					encoding = MAX (encoding, 1);
				} else if (phrase && !is_atom (c)) {
					/* phrases can have qstring words */
					type = MAX (type, WORD_QSTRING);
				}
			} else if (c < 256) {
				type = WORD_2047;
				encoding = MAX (encoding, 1);
			} else {
				type = WORD_2047;
				encoding = 2;
			}
			
			if (count >= (type == WORD_2047 ? GMIME_FOLD_PREENCODED : GMIME_FOLD_LEN)) {
				if (type != WORD_2047) {
					inptr -= strlen ("=?us-ascii?Q\?\?= ");
					type = WORD_2047;
				}
				
				word = rfc822_word_new ();
				word->next = NULL;
				word->start = start;
				word->end = inptr;
				word->type = type;
				word->encoding = encoding;
				
				tail->next = word;
				tail = word;
				count = 0;
				
				/* Note: don't reset 'type' as it
				 * needs to be preserved when breaking
				 * long words */
				start = inptr;
				encoding = 0;
			}
		}
		
		last = inptr;
	}
	
	if (count > 0) {
		word = rfc822_word_new ();
		word->next = NULL;
		word->start = start;
		word->end = last;
		word->type = type;
		word->encoding = encoding;
		
		tail->next = word;
		tail = word;
	}
	
#if d(!)0
	printf ("rfc822 word tokens:\n");
	word = words.next;
	while (word) {
		printf ("\t'%.*s'; type=%d, encoding=%d\n",
			word->end - word->start, word->start,
			word->type, word->encoding);
		
		word = word->next;
	}
#endif
	
	return words.next;
}

#define MERGED_WORD_LT_FOLDLEN(wlen, type) ((type) == WORD_2047 ? (wlen) < GMIME_FOLD_PREENCODED : (wlen) < (GMIME_FOLD_LEN - 8))

static gboolean
should_merge_words (rfc822_word *word, rfc822_word *next)
{
	switch (word->type) {
	case WORD_ATOM:
		if (next->type == WORD_2047)
			return FALSE;
		
		return (MERGED_WORD_LT_FOLDLEN (next->end - word->start, next->type));
	case WORD_QSTRING:
		/* avoid merging with words that need to be rfc2047 encoded */
		if (next->type == WORD_2047)
			return FALSE;
		
		return (MERGED_WORD_LT_FOLDLEN (next->end - word->start, WORD_QSTRING));
	case WORD_2047:
		if (next->type == WORD_ATOM) {
			/* whether we merge or not is dependent upon:
			 * 1. the number of atoms in a row after 'word'
			 * 2. if there is another encword after the string of atoms.
			 */
			int natoms = 0;
			
			while (next && next->type == WORD_ATOM) {
				next = next->next;
				natoms++;
			}
			
			/* if all the words after the encword are atoms, don't merge */
			if (!next || natoms > 3)
				return FALSE;
		}
		
		/* avoid merging with qstrings */
		if (next->type == WORD_QSTRING)
			return FALSE;
		
		return (MERGED_WORD_LT_FOLDLEN (next->end - word->start, WORD_2047));
	default:
		return FALSE;
	}
}

static void
rfc2047_encode_merge_rfc822_words (rfc822_word **wordsp)
{
	rfc822_word *word, *next, *words = *wordsp;
	
	/* first pass: merge qstrings with adjacent qstrings and encwords with adjacent encwords */
	word = words;
	while (word && word->next) {
		next = word->next;
		
		if (word->type != WORD_ATOM && word->type == next->type &&
		    MERGED_WORD_LT_FOLDLEN (next->end - word->start, word->type)) {
			/* merge the words */
			word->encoding = MAX (word->encoding, next->encoding);
			
			word->end = next->end;
			word->next = next->next;
			
			rfc822_word_free (next);
			
			next = word;
		}
		
		word = next;
	}
	
	/* second pass: now merge atoms with the other words */
	word = words;
	while (word && word->next) {
		next = word->next;
		
		if (should_merge_words (word, next)) {
			/* the resulting word type is the MAX of the 2 types */
			word->type = MAX (word->type, next->type);
			
			word->encoding = MAX (word->encoding, next->encoding);
			
			word->end = next->end;
			word->next = next->next;
			
			rfc822_word_free (next);
			
			continue;
		}
		
		word = next;
	}
	
	*wordsp = words;
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
		if (*inptr == '"' || *inptr == '\\')
			g_string_append_c (out, '\\');
		
		g_string_append_c (out, *inptr);
		
		inptr++;
	}
	
	g_string_append_c (out, '"');
}

static char *
rfc2047_encode (GMimeFormatOptions *options, const char *in, gushort safemask, const char *user_charset)
{
	rfc822_word *words, *word, *prev = NULL;
	const char *charset, *start;
	GMimeCharset mask;
	GString *out;
	char *outstr;
	size_t len;
	
	if (!(words = rfc2047_encode_get_rfc822_words (in, safemask & IS_PSAFE)))
		return g_strdup (in);
	
	rfc2047_encode_merge_rfc822_words (&words);
	
	out = g_string_new ("");
	
	/* output words now with spaces between them */
	word = words;
	while (word) {
		/* append correct number of spaces between words */
		if (prev && !(prev->type == WORD_2047 && word->type == WORD_2047)) {
			/* one or both of the words are not encoded so we write the spaces out untouched */
			len = word->start - prev->end;
			g_string_append_len (out, prev->end, len);
		}
		
		switch (word->type) {
		case WORD_ATOM:
			g_string_append_len (out, word->start, (size_t) (word->end - word->start));
			break;
		case WORD_QSTRING:
			g_assert (safemask & IS_PSAFE);
			g_string_append_len_quoted (out, word->start, (size_t) (word->end - word->start));
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
			
			switch (word->encoding) {
			case 0: /* us-ascii */
				rfc2047_encode_word (out, start, len, "us-ascii", safemask);
				break;
			case 1: /* iso-8859-1 */
				if (user_charset == NULL) {
					rfc2047_encode_word (out, start, len, "iso-8859-1", safemask);
					break;
				}
			default:
				charset = NULL;
				g_mime_charset_init (&mask);
				g_mime_charset_step (&mask, start, len);
				
				if (user_charset && g_mime_charset_can_encode (&mask, user_charset, start, len)) {
					charset = user_charset;
				} else {
					charset = g_mime_charset_best_name (&mask);
				}
				
				rfc2047_encode_word (out, start, len, charset, safemask);
				break;
			}
			
			break;
		}
		
		rfc822_word_free (prev);
		
		prev = word;
		word = word->next;
	}
	
	rfc822_word_free (prev);
	
	outstr = out->str;
	g_string_free (out, FALSE);
	
	return outstr;
}


/**
 * g_mime_utils_header_encode_phrase:
 * @options: (nullable): a #GMimeFormatOptions or %NULL
 * @phrase: phrase to encode
 * @charset: (nullable): the charset to use or %NULL to use the default
 *
 * Encodes a 'phrase' header according to the rules in rfc2047.
 *
 * Returns: the encoded 'phrase'. Useful for encoding internet
 * addresses.
 **/
char *
g_mime_utils_header_encode_phrase (GMimeFormatOptions *options, const char *phrase, const char *charset)
{
	if (phrase == NULL)
		return NULL;
	
	return rfc2047_encode (options, phrase, IS_PSAFE, charset);
}


/**
 * g_mime_utils_header_encode_text:
 * @options: (nullable): a #GMimeFormatOptions or %NULL
 * @text: text to encode
 * @charset: (nullable): the charset to use or %NULL to use the default
 *
 * Encodes a 'text' header according to the rules in rfc2047.
 *
 * Returns: the encoded header. Useful for encoding
 * headers like "Subject".
 **/
char *
g_mime_utils_header_encode_text (GMimeFormatOptions *options, const char *text, const char *charset)
{
	if (text == NULL)
		return NULL;
	
	return rfc2047_encode (options, text, IS_ESAFE, charset);
}


static char *
header_fold_tokens (GMimeFormatOptions *options, const char *field, const char *value,
		    size_t vlen, rfc2047_token *tokens, gboolean structured, gboolean include_field)
{
	rfc2047_token *token, *next;
	size_t lwsp, tab, len, n;
	gboolean encoded = FALSE;
	GString *output;
	
	len = strlen (field) + 2;
	
	if (include_field) {
		output = g_string_sized_new (len + vlen + 1);
		g_string_append (output, field);
		g_string_append (output, ": ");
	} else {
		output = g_string_sized_new (vlen + 2);
		g_string_append_c (output, ' ');
	}
	lwsp = 0;
	tab = 0;
	
	token = tokens;
	while (token != NULL) {
		if (is_lwsp (token->text[0])) {
			for (n = 0; n < token->length; n++) {
				if (token->text[n] == '\r')
					continue;
				
				lwsp = output->len;
				if (token->text[n] == '\t')
					tab = output->len;
				
				g_string_append_c (output, token->text[n]);
				if (token->text[n] == '\n') {
					lwsp = tab = 0;
					len = 0;
				} else {
					len++;
				}
			}
			
			if (len == 0 && token->next) {
				g_string_append_c (output, structured ? '\t' : ' ');
				len = 1;
			}
			
			encoded = FALSE;
		} else if (token->encoding != 0) {
			n = strlen (token->charset) + 7 + (encoded ? 1 : 0);
			
			if (len + token->length + n > GMIME_FOLD_LEN) {
				if (tab != 0) {
					/* tabs are the perfect breaking opportunity... */
					g_string_insert_c (output, tab, '\n');
					len = (lwsp - tab) + 1;
				} else if (lwsp != 0) {
					/* break just before the last lwsp character */
					g_string_insert_c (output, lwsp, '\n');
					len = 1;
				} else if (len > 1) {
					/* force a line break... */
					g_string_append (output, structured ? "\n\t" : "\n ");
					len = 1;
				}
			} else if (encoded) {
				/* the previous token was an encoded-word token, so make sure to add
				 * whitespace between the two tokens... */
				g_string_append_c (output, ' ');
			}
			
			/* Note: if the encoded-word token is longer than the fold length, oh well...
			 * it probably just means that we are folding a header written by a user-agent
			 * with a different max line length than ours. */
			
			g_string_append_printf (output, "=?%s?%c?", token->charset, token->encoding);
			g_string_append_len (output, token->text, token->length);
			g_string_append (output, "?=");
			len += token->length + n;
			encoded = TRUE;
			lwsp = 0;
			tab = 0;
		} else if (len + token->length > GMIME_FOLD_LEN) {
			if (tab != 0) {
				/* tabs are the perfect breaking opportunity... */
				g_string_insert_c (output, tab, '\n');
				len = (lwsp - tab) + 1;
			} else if (lwsp != 0) {
				/* break just before the last lwsp character */
				g_string_insert_c (output, lwsp, '\n');
				len = 1;
			} else if (len > 1) {
				/* force a line break... */
				g_string_append (output, structured ? "\n\t" : "\n ");
				len = 1;
			}
			
			if (token->length >= GMIME_FOLD_LEN) {
				/* the token is longer than the allowable line length,
				 * so we'll have to break it apart... */
				n = GMIME_FOLD_LEN - len;
				g_string_append_len (output, token->text, n);
				g_string_append (output, "\n\t");
				g_string_append_len (output, token->text + n, token->length - n);
				len = (token->length - n) + 1;
			} else {
				g_string_append_len (output, token->text, token->length);
				len += token->length;
			}
			
			encoded = FALSE;
			lwsp = 0;
			tab = 0;
		} else {
			g_string_append_len (output, token->text, token->length);
			len += token->length;
			encoded = FALSE;
			lwsp = 0;
			tab = 0;
		}
		
		next = token->next;
		rfc2047_token_free (token);
		token = next;
	}
	
	if (output->str[output->len - 1] != '\n')
		g_string_append_c (output, '\n');
	
	return g_string_free (output, FALSE);
}


/**
 * g_mime_utils_structured_header_fold:
 * @options: (nullable): a #GMimeParserOptions or %NULL
 * @format: (nullable): a #GMimeFormatOptions or %NULL
 * @header: header field and value string
 *
 * Folds a structured header according to the rules in rfc822.
 *
 * Returns: an allocated string containing the folded header.
 **/
char *
g_mime_utils_structured_header_fold (GMimeParserOptions *options, GMimeFormatOptions *format, const char *header)
{
	rfc2047_token *tokens;
	const char *value;
	char *folded;
	char *field;
	size_t len;
	
	if (header == NULL)
		return NULL;
	
	value = header;
	while (*value && *value != ':')
		value++;
	
	if (*value == '\0')
		return NULL;
	
	field = g_strndup (header, value - header);
	
	value++;
	while (*value && is_lwsp (*value))
		value++;
	
	tokens = tokenize_rfc2047_phrase (options, value, &len);
	folded = header_fold_tokens (format, field, value, len, tokens, TRUE, TRUE);
	g_free (field);
	
	return folded;
}


/**
 * _g_mime_utils_structured_header_fold:
 * @options: (nullable): a #GMimeParserOptions or %NULL
 * @format: (nullable): a #GMimeFormatOptions or %NULL
 * @field: header field
 * @value: header value
 *
 * Folds an structured header according to the rules in rfc822.
 *
 * Returns: an allocated string containing the folded header.
 **/
char *
_g_mime_utils_structured_header_fold (GMimeParserOptions *options, GMimeFormatOptions *format,
				      const char *field, const char *value)
{
	rfc2047_token *tokens;
	size_t len;
	
	if (field == NULL)
		return NULL;
	
	if (value == NULL)
		return g_strdup ("\n");
	
	tokens = tokenize_rfc2047_phrase (options, value, &len);
	
	return header_fold_tokens (format, field, value, len, tokens, TRUE, FALSE);
}


/**
 * g_mime_utils_unstructured_header_fold:
 * @options: (nullable): a #GMimeParserOptions or %NULL
 * @format: (nullable): a #GMimeFormatOptions or %NULL
 * @header: header field and value string
 *
 * Folds an unstructured header according to the rules in rfc822.
 *
 * Returns: an allocated string containing the folded header.
 **/
char *
g_mime_utils_unstructured_header_fold (GMimeParserOptions *options, GMimeFormatOptions *format, const char *header)
{
	rfc2047_token *tokens;
	const char *value;
	char *folded;
	char *field;
	size_t len;
	
	if (header == NULL)
		return NULL;
	
	value = header;
	while (*value && *value != ':')
		value++;
	
	if (*value == '\0')
		return NULL;
	
	field = g_strndup (header, value - header);
	
	value++;
	while (*value && is_lwsp (*value))
		value++;
	
	tokens = tokenize_rfc2047_text (options, value, &len);
	folded = header_fold_tokens (format, field, value, len, tokens, FALSE, TRUE);
	g_free (field);
	
	return folded;
}


/**
 * _g_mime_utils_unstructured_header_fold:
 * @options: (nullable): a #GMimeParserOptions or %NULL
 * @format: (nullable): a #GMimeFormatOptions or %NULL
 * @field: header field
 * @value: header value
 *
 * Folds an unstructured header according to the rules in rfc822.
 *
 * Returns: an allocated string containing the folded header.
 **/
char *
_g_mime_utils_unstructured_header_fold (GMimeParserOptions *options, GMimeFormatOptions *format, const char *field, const char *value)
{
	rfc2047_token *tokens;
	size_t len;
	
	if (field == NULL)
		return NULL;
	
	if (value == NULL)
		return g_strdup ("\n");
	
	tokens = tokenize_rfc2047_text (options, value, &len);
	
	return header_fold_tokens (format, field, value, len, tokens, FALSE, FALSE);
}


/**
 * g_mime_utils_header_printf:
 * @options: (nullable): a #GMimeParserOptions or %NULL
 * @format: (nullable): a #GMimeFormatOptions or %NULL
 * @text: text with printf-style formatters
 * @...: arguments
 *
 * Allocates a buffer containing a formatted header specified by the
 * @Varargs.
 *
 * Returns: an allocated string containing the folded header specified
 * by @text and the following arguments.
 **/
char *
g_mime_utils_header_printf (GMimeParserOptions *options, GMimeFormatOptions *format, const char *text, ...)
{
	char *buf, *ret;
	va_list ap;
	
	va_start (ap, text);
	buf = g_strdup_vprintf (text, ap);
	va_end (ap);
	
	ret = g_mime_utils_unstructured_header_fold (options, format, buf);
	g_free (buf);
	
	return ret;
}


/**
 * g_mime_utils_header_value_unfold:
 * @value: raw header value
 *
 * Unfolds a raw header value according to the rules in rfc822.
 *
 * Returns: an allocated string containing the unfolded header value.
 **/
char *
g_mime_utils_header_value_unfold (const char *value)
{
	register const char *inptr = value;
	const char *start, *inend;
	char *str, *outptr;

	while (is_lwsp (*inptr))
		inptr++;

	inend = start = inptr;
	while (*inptr) {
		if (!is_lwsp (*inptr++))
			inend = inptr;
	}

	outptr = str = g_malloc ((size_t) (inend - start) + 1);
	inptr = start;

	while (inptr < inend) {
		if (*inptr != '\r' && *inptr != '\n')
			*outptr++ = *inptr;
		inptr++;
	}

	*outptr = '\0';

	return str;
}
