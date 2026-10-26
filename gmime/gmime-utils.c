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

#define _GNU_SOURCE

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
#ifdef HAVE_NETDB_H
#include <netdb.h>
#endif
#include <ctype.h>
#include <errno.h>

#ifdef LIBIDN
#include <idn2.h>
#undef G_GNUC_UNUSED
#undef G_GNUC_DEPRECATED
#endif

#include <glib.h>
#ifdef G_OS_WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#endif

#include "gmime-utils.h"
#include "gmime-codecs.h"
#include "gmime-rfc2047-private.h"
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

static const unsigned char gmime_datetok_table[256] = {
	111,111,111,111,111,111,111,111,111,111,111,111,111,111,111,111,
	111,111,111,111,111,111,111,111,111,111,111,111,111,111,111,111,
	111,111,111,111,111,111,111,111,111,111,111,175,111,175,111,111,
	 38, 38, 38, 38, 38, 38, 38, 38, 38, 38,119,111,111,111,111,111,
	111, 73, 75, 75, 73, 73, 73, 75, 73, 73, 75, 79, 75, 73, 73, 73,
	 75, 79, 73, 73, 73, 73, 75, 77, 79, 73, 79,111,111,111,111,111,
	111,105,107,107,105,105,105,107,105,105,107,111,107,105,105,105,
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

/* Timezone values defined in rfc822 */
static struct {
	const char *name;
	int offset;
} tz_offsets [] = {
	{ "UT",       0 },
	{ "UTC",      0 },
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
	{ "JST",    900 },
	{ "KST",    900 },
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
 * string represented by @date. Sub-minute timezone offsets are converted
 * to UTC; that conversion must remain within the supported calendar range.
 *
 * Returns: (nullable): a valid string representation of the date, or %NULL
 * if a required UTC conversion is out of range.
 **/
char *
g_mime_utils_header_format_date (GDateTime *date)
{
	int wday, year, month, day, hour, min, sec, tz_offset;
	GDateTime *utc = NULL;
	GTimeSpan tz;
	char sign;
	
	g_return_val_if_fail (date != NULL, NULL);

	tz = g_date_time_get_utc_offset (date);
	if (tz % G_TIME_SPAN_MINUTE == 0) {
		if (tz < 0) {
			sign = '-';
			tz *= -1;
		} else {
			sign = '+';
		}

		tz_offset = 100 * (tz / G_TIME_SPAN_HOUR);
		tz_offset += (tz % G_TIME_SPAN_HOUR) / G_TIME_SPAN_MINUTE;
	} else {
		// Note: RFC822 Date headers can only make use of timezones that can be represented by
		// +/-HHMM and cannot represent any timezone that makes use of seconds. Therefore, the
		// only solution is to convert the date being formatted into its UTC representation and
		// use "-0000" as the timezone (as opposed to the normal "+0000" used for actual UTC).
		date = utc = g_date_time_to_utc (date);
		g_return_val_if_fail (utc != NULL, NULL);
		tz_offset = 0;
		sign = '-';
	}

	wday = g_date_time_get_day_of_week (date);
	year = g_date_time_get_year (date);
	month = g_date_time_get_month (date);
	day = g_date_time_get_day_of_month (date);
	hour = g_date_time_get_hour (date);
	min = g_date_time_get_minute (date);
	sec = g_date_time_get_second (date);

	if (utc != NULL)
		g_date_time_unref (utc);
	
	return g_strdup_printf ("%s, %02d %s %04d %02d:%02d:%02d %c%04d",
				tm_days[wday % 7], day, tm_months[month - 1],
				year, hour, min, sec, sign, tz_offset);
}

/* This is where it gets ugly... */

typedef struct _date_token {
	struct _date_token *next;
	unsigned char mask;
	const char *start;
	size_t len;
} date_token;

#define date_token_mask(t)  ((t)->mask)
#define is_numeric(t)       ((date_token_mask (t) & DATE_TOKEN_NON_NUMERIC) == 0)
#define is_weekday(t)       ((date_token_mask (t) & DATE_TOKEN_NON_WEEKDAY) == 0)
#define is_month(t)         ((date_token_mask (t) & DATE_TOKEN_NON_MONTH) == 0)
#define is_time(t)          (((date_token_mask (t) & DATE_TOKEN_NON_TIME) == 0) && (date_token_mask (t) & DATE_TOKEN_HAS_COLON))
#define is_tzone_alpha(t)   ((date_token_mask (t) & DATE_TOKEN_NON_TIMEZONE_ALPHA) == 0)
#define is_tzone_numeric(t) (((date_token_mask (t) & DATE_TOKEN_NON_TIMEZONE_NUMERIC) == 0) && (date_token_mask (t) & DATE_TOKEN_HAS_SIGN))
#define is_tzone(t)         (is_tzone_alpha (t) || is_tzone_numeric (t))

#define date_token_free(tok) g_slice_free (date_token, tok)
#define date_token_new() g_slice_new (date_token)

static gboolean
date_skip_cfws (const char **in, const char *end)
{
	const char *p = *in;
	gsize depth;

	while (p < end) {
		if (is_lwsp (*p)) {
			p++;
		} else if (*p == '(') {
			depth = 1;
			p++;
			while (p < end && depth) {
				if (*p == '\\' && p + 1 < end)
					p++;
				else if (*p == '(')
					depth++;
				else if (*p == ')')
					depth--;
				p++;
			}
			if (depth)
				return FALSE;
		} else {
			break;
		}
	}

	*in = p;
	return TRUE;
}

static date_token *
datetok (const char *date, gsize length)
{
	date_token tokens, *token, *tail;
	const char *limit = date + length;
	const char *start, *end;
        unsigned char mask;
	
	tail = (date_token *) &tokens;
	tokens.next = NULL;
	
	start = date;
	while (start < limit) {
		if (!date_skip_cfws (&start, limit))
			break;
		
		if (start == limit)
			break;
		
		mask = gmime_datetok_table[(unsigned char) *start];
		
		/* find the end of this token */
		end = start + 1;
		while (end < limit && !(*end && strchr ("-/,\t\r\n ", *end)))
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
		
		if (end < limit)
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

	if (inlen == 0)
		return -1;
	
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
	
	if (mday <= 0 || mday > 31)
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
	const char *colon, *end = in + inlen;
	
	*hour = *min = *sec = 0;
	
	if (!(colon = memchr (in, ':', inlen)) || (*hour = decode_int (in, colon - in)) < 0 || *hour > 23)
		return FALSE;

	in = colon + 1;
	if (!(colon = memchr (in, ':', end - in)))
		colon = end;

	if ((*min = decode_int (in, colon - in)) < 0 || *min > 59)
		return FALSE;

	/* Accept hh:mm without a seconds component. */
	if (colon == end)
		return TRUE;

	in = colon + 1;
	if ((*sec = decode_int (in, end - in)) < 0 || *sec > 60)
		return FALSE;

	if (*sec == 60) {
		if (*hour != 23 || *min != 59)
			return FALSE;
		/* GDateTime cannot represent leap seconds; use the last second of the day. */
		*sec = 59;
	}
	
	return TRUE;
}

static gboolean
get_am_pm (date_token *token, int *hour)
{
	if (!is_tzone_alpha (token) || token->len != 2 || token->start[1] != 'M')
		return FALSE;

	if (token->start[0] == 'A') {
		if (*hour == 12)
			*hour = 0;
		return TRUE;
	}

	if (token->start[0] == 'P') {
		if (*hour < 12)
			*hour += 12;
		return TRUE;
	}

	return FALSE;
}

static gboolean
get_tzone (date_token *token, int *offset)
{
	int value;
	guint t;

	if (is_tzone_numeric (token)) {
		if (token->start[0] != '+' && token->start[0] != '-')
			return FALSE;

		if ((value = decode_int (token->start + 1, token->len - 1)) < 0)
			return FALSE;

		if (token->start[0] == '-')
			value = -value;
	} else if (is_tzone_alpha (token)) {
		if (token->len > 3)
			return FALSE;

		for (t = 0; t < G_N_ELEMENTS (tz_offsets); t++) {
			if (strlen (tz_offsets[t].name) == token->len &&
			    !memcmp (token->start, tz_offsets[t].name, token->len))
				break;
		}

		if (t == G_N_ELEMENTS (tz_offsets))
			return FALSE;

		value = tz_offsets[t].offset;
	} else if (is_numeric (token)) {
		if ((value = decode_int (token->start, token->len)) < 0)
			return FALSE;
	} else {
		return FALSE;
	}

	/* Reject invalid minutes before converting HHMM to seconds: -1192 must
	 * not normalize to the otherwise valid offset -12:32. */
	if (value < -1200 || value > 1400 || ABS (value % 100) >= 60)
		return FALSE;

	*offset = value;
	return TRUE;
}

static GDateTime *
new_parsed_date (int year, int month, int day, int hour, int min, int sec, int offset)
{
	GTimeZone *tz;
	GDateTime *date;

	if (year < 1 || year > 9999 || !g_date_valid_dmy (day, month, year))
		return NULL;

	tz = g_time_zone_new_offset ((offset / 100) * 3600 + (offset % 100) * 60);
	date = g_date_time_new (tz, year, month, day, hour, min, sec);
	g_time_zone_unref (tz);

	/* DateTimeOffset also bounds the UTC representation to year 9999. */
	if (date && g_date_time_to_unix (date) > G_GINT64_CONSTANT (253402300799)) {
		g_date_time_unref (date);
		return NULL;
	}

	return date;
}

static GDateTime *
parse_rfc822_date (date_token *tokens)
{
	int year, month, day, hour, min, sec, offset = 0, n;
	date_token *token;
	
	token = tokens;
	
	year = month = day = hour = min = sec = 0;
	
	if (is_weekday (token) && get_wday (token->start, token->len) != -1) {
		/* not all dates may have this... */
		token = token->next;
	}
	
	/* get the mday */
	if (!token || (n = get_mday (token->start, token->len)) == -1)
		return NULL;
	
	token = token->next;
	day = n;
	
	/* get the month */
	if (!token || !is_month (token) || (n = get_month (token->start, token->len)) == -1)
		return NULL;
	
	token = token->next;
	month = n;
	
	/* get the year */
	if (!token || (n = get_year (token->start, token->len)) == -1)
		return NULL;
	
	token = token->next;
	year = n;
	
	/* get the hour/min/sec */
	if (!token || !is_time (token) || !get_time (token->start, token->len, &hour, &min, &sec))
		return NULL;
	
	token = token->next;
	
	if (!token)
		return NULL;

	if (get_am_pm (token, &hour))
		token = token->next;

	if (token)
		get_tzone (token, &offset);

	return new_parsed_date (year, month, day, hour, min, sec, offset);
}

static GDateTime *
parse_broken_date (date_token *tokens)
{
	gboolean have_weekday = FALSE, have_time = FALSE, have_am_pm = FALSE, have_tzone = FALSE, numeric_month = FALSE;
	int year = 0, month = 0, day = 0, hour = 0, min = 0, sec = 0, offset = 0, n;
	date_token *token;

	for (token = tokens; token; token = token->next) {
		if (!have_weekday && is_weekday (token) && get_wday (token->start, token->len) != -1) {
			have_weekday = TRUE;
			continue;
		}

		if ((!month || numeric_month) && is_month (token) && (n = get_month (token->start, token->len)) != -1) {
			if (numeric_month) {
				numeric_month = FALSE;
				day = month;
			}
			month = n;
			continue;
		}

		if (!have_time && is_time (token) && get_time (token->start, token->len, &hour, &min, &sec)) {
			have_time = TRUE;
			continue;
		}

		if (have_time && !have_tzone && !have_am_pm && get_am_pm (token, &hour)) {
			have_am_pm = TRUE;
			continue;
		}

		/* Unsigned numeric tokens are ambiguous here; leave them for the date
		 * fields below, except a second four-digit token may be the timezone. */
		if (!have_tzone && is_tzone (token) && get_tzone (token, &offset)) {
			have_tzone = TRUE;
			continue;
		}

		if (is_numeric (token)) {
			if (token->len == 4) {
				if (!year) {
					if ((n = get_year (token->start, token->len)) != -1)
						year = n;
				} else if (!have_tzone) {
					have_tzone = get_tzone (token, &offset);
				}
				continue;
			}
			if (token->len > 2)
				continue;

			n = decode_int (token->start, token->len);
			if (!month && n > 0 && n <= 12) {
				numeric_month = TRUE;
				month = n;
			} else if (!day && n > 0 && n <= 31) {
				day = n;
			} else if (!year && n >= 69) {
				year = 1900 + n;
			}
		}
	}

	if (!have_time)
		hour = min = sec = 0;

	return new_parsed_date (year, month, day, hour, min, sec, offset);
}


/**
 * g_mime_utils_header_decode_date:
 * @str: input date string
 *
 * Parses an RFC 822 date string, recovering common non-standard formats.
 * Missing or invalid timezone information is treated as UTC. Invalid
 * time tokens can recover as midnight when a complete calendar date is
 * present.
 *
 * Returns: (nullable) (transfer full): the #GDateTime representation of the date
 * string specified by @str or %NULL on error.
 **/
GDateTime *
g_mime_utils_header_decode_date (const char *str)
{
	g_return_val_if_fail (str != NULL, NULL);

	return _g_mime_utils_header_decode_date (str, strlen (str));
}

GDateTime *
_g_mime_utils_header_decode_date (const char *str, gsize length)
{
	date_token *token, *tokens;
	GDateTime *date;

	g_return_val_if_fail (str != NULL, NULL);
	
	if (!(tokens = datetok (str, length)))
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
	if (idn2_to_ascii_8z (fqdn, &ascii, 0) == IDN2_OK) {
		g_string_append (msgid, ascii);
		idn2_free (ascii);
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
 * Decodes a msg-id, with or without angle brackets, recovering common
 * non-standard local-parts and domains. Malformed or incomplete tokens
 * return %NULL; text following the first complete token is ignored.
 *
 * Returns: (nullable) (transfer full): the addr-spec portion of the msg-id,
 * or %NULL on error.
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
		else if (!quoted && is_special (*inptr))
			return TRUE;
		
		if (*inptr)
			inptr++;
	}
	
	return quoted;
}

/**
 * g_mime_utils_quote_string:
 * @str: input string
 *
 * Quotes @string as needed according to the rules in rfc2822.
 * 
 * Returns: an allocated string containing the escaped and quoted (if
 * needed to be) input string. The decision to quote the string is
 * based on whether or not the input string contains any 'specials'
 * as defined by rfc2822.
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
	
	qstring = g_string_free (out, FALSE);
	
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
	for (inptr = text; inptr < inend; inptr++) {
		if (*inptr > (unsigned char) 127)
			return TRUE;
	}
	
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


static char *
decode_8bit (GMimeParserOptions *options, const char *text, size_t len, size_t *decoded_length)
{
	size_t outleft, outlen, min, ninval;
	const char *charsets[4];
	const char *best, *charset;
	iconv_t cd;
	char *out;
	int i, n = 0;
	
	charset = g_mime_parser_options_get_charset_encoding (options);
	/* UTF-8 is always the first choice when no invalid sequences are present. */
	charsets[n++] = "utf-8";
	if (g_ascii_strcasecmp (charset, "utf-8") && g_ascii_strcasecmp (charset, "iso-8859-1"))
		charsets[n++] = charset;
	charsets[n++] = "iso-8859-1";
	charsets[n] = NULL;
	best = charsets[0];
	min = len;
	
	outleft = (len * 2) + 16;
	out = g_malloc (outleft + 1);
	
	for (i = 0; charsets[i]; i++) {
		if ((cd = g_mime_iconv_open ("UTF-8", charsets[i])) == (iconv_t) -1)
			continue;
		
		outlen = charset_convert (cd, text, len, &out, &outleft, &ninval);
		
		g_mime_iconv_close (cd);
		
		if (ninval == 0) {
			if (decoded_length)
				*decoded_length = outlen;
			return g_realloc (out, outlen + 1);
		}
		
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
		
		if (decoded_length)
			*decoded_length = outbuf - out;
		*outbuf++ = '\0';
		
		return g_realloc (out, (size_t) (outbuf - out));
	}
	
	outlen = charset_convert (cd, text, len, &out, &outleft, &ninval);
	
	g_mime_iconv_close (cd);

	if (decoded_length)
		*decoded_length = outlen;
	return g_realloc (out, outlen + 1);
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
	g_return_val_if_fail (text != NULL, NULL);

	return decode_8bit (options, text, len, NULL);
}

char *
_g_mime_utils_decode_8bit_text (GMimeParserOptions *options, const char *text, gsize length)
{
	char *decoded, *value;
	size_t decoded_length;

	decoded = decode_8bit (options, text, length, &decoded_length);
	value = g_utf8_make_valid (decoded, decoded_length);
	g_free (decoded);

	return value;
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
	const char *word;
	size_t length;
	size_t word_length;
	int codepage;
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
	const char *inend;
	const char *inptr;
	char *buf;
	char encoding;
	size_t n;
	
	/* check that this could even be an encoded-word token
	 * a minimal encoded-word token would be "=?a?b??=" which is 8 characters long */
	if (len < 8 || strncmp (word, "=?", 2) != 0 || strncmp (word + len - 2, "?=", 2) != 0)
		return NULL;
	
	/* skip over '=?' */
	inptr = word + 2;
	inend = word + len - 2;
	charset = inptr;
	
	if (*charset == '?' || *charset == '*') {
		/* this would result in an empty charset */
		return NULL;
	}
	
	/* rfc2231 updates rfc2047 encoded words...
	 * The ABNF given in RFC 2047 for encoded-words is:
	 *   encoded-word := "=?" charset "?" encoding "?" encoded-text "?="
	 * This specification changes this ABNF to:
	 *   encoded-word := "=?" charset ["*" language] "?" encoding "?" encoded-text "?="
	 */
	while (inptr < inend && *inptr != '?' && *inptr != '*') {
		if (!is_ascii (*inptr) || (!is_atom (*inptr) && *inptr != '.'))
			return NULL;
		inptr++;
	}

	/* record the length of the charset name*/
	n = (size_t) (inptr - charset);
	
	/* if there is a language defined, skip over it */
	if (inptr < inend && *inptr == '*') {
		inptr++;

		while (inptr < inend && *inptr != '?') {
			if (!is_ascii (*inptr) || !is_atom (*inptr))
				return NULL;
			inptr++;
		}
	}

	/* check that we are at a '?' and that we have enough room for an
	   encoding following by a '?' (and the "?=" starting at inend) */
	if (inend - inptr < 3 || *inptr != '?' || inptr[2] != '?')
		return NULL;

	/* validate and canonicalize our encoding */
	switch (inptr[1]) {
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
	payload = inptr + 3;
	
	token = rfc2047_token_new (payload, inend - payload);
	token->word = word;
	token->word_length = len;
	buf = g_strndup (charset, n);
	token->charset = g_mime_charset_iconv_name (buf);
	token->codepage = _g_mime_charset_get_codepage (token->charset);
	g_free (buf);
	token->encoding = encoding;
	
	return token;
}

static gboolean
rfc2047_is_encoded_word_start (const char *inptr, const char *inend)
{
	if (inend - inptr < 2 || inptr[0] != '=' || inptr[1] != '?')
		return FALSE;

	inptr += 2;
	while (inptr < inend && *inptr != '?' && !is_lwsp (*inptr))
		inptr++;

	return inend - inptr >= 3 && inptr[0] == '?' &&
		inptr[1] != '\0' && strchr ("BbQq", inptr[1]) != NULL && inptr[2] == '?';
}

static void
rfc2047_warn_invalid_value (GMimeParserOptions *options, const char *text, size_t length, gint64 offset)
{
	char *item = g_strndup (text, length);

	_g_mime_parser_options_warn (options, offset, GMIME_WARN_INVALID_RFC2047_HEADER_VALUE, item);
	g_free (item);
}

static rfc2047_token *
tokenize_rfc2047_phrase (GMimeParserOptions *options, const char *in, size_t length, gint64 offset, gboolean preserve_lwsp)
{
	gboolean can_warn = g_mime_parser_options_get_warning_callback (options) != NULL;
	rfc2047_token list, *lwsp, *token, *tail;
	register const char *inptr = in;
	const char *inend = in + length;
	gboolean has_specials = FALSE;
	GMimeRfcComplianceMode mode;
	gboolean encoded = FALSE;
	const char *text, *word;
	gboolean ascii;
	size_t n;
	
	mode = g_mime_parser_options_get_rfc2047_compliance_mode (options);
	tail = (rfc2047_token *) &list;
	list.next = NULL;
	lwsp = NULL;
	
	while (inptr < inend) {
		text = inptr;
		while (inptr < inend && is_lwsp (*inptr))
			inptr++;
		
		if (inptr > text)
			lwsp = rfc2047_token_new (text, inptr - text);
		else
			lwsp = NULL;
		
		if (inptr == inend) {
			if (lwsp != NULL)
				tail->next = lwsp;
			break;
		}

		word = inptr;
		ascii = TRUE;
		if (is_ascii (*inptr) && is_atom (*inptr)) {
			if (G_LIKELY (mode == GMIME_RFC_COMPLIANCE_LOOSE)) {
				/* Make an extra effort to detect and
				 * separate encoded-word tokens that
				 * have been merged with other
				 * words. */
				
				has_specials = FALSE;
				
				if (inend - inptr >= 2 && !memcmp (inptr, "=?", 2)) {
					inptr += 2;
					
					/* skip past the charset (if one is even declared, sigh) */
					while (inptr < inend && *inptr != '?' && !is_lwsp (*inptr)) {
						ascii = ascii && is_ascii (*inptr);
						if (!is_atom (*inptr)) {
							has_specials = TRUE;
						}
						inptr++;
					}
					
					/* sanity check encoding type */
					if (inend - inptr < 4 || inptr[0] != '?' || !inptr[1] ||
					    !strchr ("BbQq", inptr[1]) || inptr[2] != '?') {
						has_specials = FALSE;
						inptr = word + 2;
						ascii = TRUE;
						goto non_rfc2047;
					}
					
					inptr += 3;
					
					/* Find the end of the encoded word, but stop at another encoded-word
					 * start since that means this one was never terminated. */
					while (inptr < inend && (inend - inptr < 2 || memcmp (inptr, "?=", 2))) {
						if (*inptr == '=' && rfc2047_is_encoded_word_start (inptr, inend))
							break;
						ascii = ascii && is_ascii (*inptr);
						if (!is_atom (*inptr)) {
							has_specials = TRUE;
						}
						inptr++;
					}
					
					if (inend - inptr < 2 || memcmp (inptr, "?=", 2)) {
						/* didn't find an end marker... */
						has_specials = FALSE;
						inptr = word + 2;
						ascii = TRUE;
						
						goto non_rfc2047;
					}
					
					inptr += 2;
				} else {
				non_rfc2047:
					/* stop if we encounter a possible rfc2047 encoded
					 * token even if it's inside another word, sigh. */
					while (inptr < inend && is_atom (*inptr) &&
					       (inend - inptr < 2 || memcmp (inptr, "=?", 2))) {
						ascii = ascii && is_ascii (*inptr);
						inptr++;
					}
				}
			} else {
				/* Encoded-word tokens are atoms. */
				while (inptr < inend && is_ascii (*inptr) && is_atom (*inptr)) {
					inptr++;
				}
			}
			
			n = (size_t) (inptr - word);
			if ((token = rfc2047_token_new_encoded_word (word, n))) {
				if (can_warn && has_specials)
					rfc2047_warn_invalid_value (options, in, length, offset);
				
				/* Decoding ignores whitespace between encoded words; folding preserves it. */
				if ((!encoded || preserve_lwsp) && lwsp != NULL) {
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
			/* Append the non-ASCII atom token. */
			while (inptr < inend && !is_lwsp (*inptr) && (!is_ascii (*inptr) || !is_atom (*inptr))) {
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
	
	return list.next;
}

static rfc2047_token *
tokenize_rfc2047_text (GMimeParserOptions *options, const char *in, size_t length, gint64 offset, gboolean preserve_lwsp)
{
	gboolean can_warn = g_mime_parser_options_get_warning_callback (options) != NULL;
	rfc2047_token list, *lwsp, *token, *tail;
	register const char *inptr = in;
	const char *inend = in + length;
	gboolean has_specials = FALSE;
	GMimeRfcComplianceMode mode;
	gboolean encoded = FALSE;
	const char *text, *word;
	gboolean ascii;
	size_t n;
	
	mode = g_mime_parser_options_get_rfc2047_compliance_mode (options);
	tail = (rfc2047_token *) &list;
	list.next = NULL;
	lwsp = NULL;
	
	while (inptr < inend) {
		text = inptr;
		while (inptr < inend && is_lwsp (*inptr))
			inptr++;
		
		if (inptr > text)
			lwsp = rfc2047_token_new (text, inptr - text);
		else
			lwsp = NULL;
		
		if (inptr < inend) {
			word = inptr;
			ascii = TRUE;
			
			if (G_LIKELY (mode == GMIME_RFC_COMPLIANCE_LOOSE)) {
				has_specials = FALSE;
				
				/* Make an extra effort to detect and separate encoded-word
				 * tokens that have been merged with other words. */
				if (inend - inptr >= 2 && !memcmp (inptr, "=?", 2)) {
					inptr += 2;
					
					/* skip past the charset (if one is even declared, sigh) */
					while (inptr < inend && *inptr != '?' && !is_lwsp (*inptr)) {
						ascii = ascii && is_ascii (*inptr);
						inptr++;
					}
					
					/* sanity check encoding type */
					if (inend - inptr < 4 || inptr[0] != '?' || !inptr[1] ||
					    !strchr ("BbQq", inptr[1]) || inptr[2] != '?') {
						has_specials = FALSE;
						inptr = word + 2;
						ascii = TRUE;
						goto non_rfc2047;
					}
					
					inptr += 3;
					
					/* Find the end of the encoded word, but stop at another encoded-word
					 * start since that means this one was never terminated. */
					while (inptr < inend && (inend - inptr < 2 || memcmp (inptr, "?=", 2))) {
						if (*inptr == '=' && rfc2047_is_encoded_word_start (inptr, inend))
							break;
						ascii = ascii && is_ascii (*inptr);
						if (is_lwsp (*inptr))
							has_specials = TRUE;
						inptr++;
					}
					
					if (inend - inptr < 2 || memcmp (inptr, "?=", 2)) {
						/* didn't find an end marker... */
						has_specials = FALSE;
						inptr = word + 2;
						ascii = TRUE;
						
						goto non_rfc2047;
					}
					
					inptr += 2;
				} else {
				non_rfc2047:
					/* stop if we encounter a possible rfc2047 encoded
					 * token even if it's inside another word, sigh. */
					while (inptr < inend && !is_lwsp (*inptr) &&
					       (inend - inptr < 2 || memcmp (inptr, "=?", 2))) {
						ascii = ascii && is_ascii (*inptr);
						inptr++;
					}
				}
			} else {
				/* Find the end of a run of text. */
				while (inptr < inend && !is_lwsp (*inptr)) {
					ascii = ascii && is_ascii (*inptr);
					inptr++;
				}
			}
			
			n = (size_t) (inptr - word);
			if ((token = rfc2047_token_new_encoded_word (word, n))) {
				if (can_warn && has_specials)
					rfc2047_warn_invalid_value (options, in, length, offset);
				
				/* Decoding ignores whitespace between encoded words; folding preserves it. */
				if ((!encoded || preserve_lwsp) && lwsp != NULL) {
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
	
	return list.next;
}

typedef struct {
	const char *charset;
	size_t count, order;
} rfc2047_charset_count;

static const char *
rfc2047_get_most_common_charset (rfc2047_token *tokens)
{
	rfc2047_charset_count *entry, *best = NULL;
	GHashTable *counts;
	const char *charset;

	counts = g_hash_table_new_full (g_direct_hash, g_direct_equal, NULL, g_free);

	for (; tokens; tokens = tokens->next) {
		if (!tokens->encoding)
			continue;

		if (!(entry = g_hash_table_lookup (counts, GINT_TO_POINTER (tokens->codepage)))) {
			entry = g_new0 (rfc2047_charset_count, 1);
			entry->charset = tokens->charset;
			entry->order = g_hash_table_size (counts);
			g_hash_table_insert (counts, GINT_TO_POINTER (tokens->codepage), entry);
		}

		entry->count++;
		if (!best || entry->count > best->count ||
		    (entry->count == best->count && entry->order < best->order))
			best = entry;
	}

	charset = best ? best->charset : NULL;
	g_hash_table_destroy (counts);

	return charset;
}

static char *
rfc2047_decode_tokens (GMimeParserOptions *options, rfc2047_token *tokens, size_t buflen,
		      const char **charset_out, gboolean checked)
{
	GMimeDecoder *base64 = NULL, *qp = NULL, *decoder;
	rfc2047_token *token, *next;
	size_t outcap = 76, capacity;
	size_t outlen, ninval, len;
	unsigned char *outbuf, *outptr;
	const char *charset, *nul;
	GString *decoded;
	char encoding;
	int codepage;
	iconv_t cd;
	char *str;
	
	decoded = g_string_sized_new (buflen + 1);
	outbuf = g_malloc (outcap);

	if (charset_out)
		*charset_out = rfc2047_get_most_common_charset (tokens);
	
	token = tokens;
	while (token != NULL) {
		next = token->next;
		
		if (token->encoding) {
			/* In order to work around broken mailers, we need to combine
			 * the raw decoded content of runs of identically encoded word
			 * tokens before converting into UTF-8. */
			encoding = token->encoding;
			charset = token->charset;
			codepage = token->codepage;
			len = token->length;

			if (encoding == 'B') {
				if (base64 == NULL)
					base64 = GMIME_DECODER (g_mime_base64_decoder_new ());
				decoder = base64;
			} else {
				if (qp == NULL)
					qp = GMIME_DECODER (g_mime_quoted_printable_decoder_new (TRUE));
				decoder = qp;
			}
			
			/* find the end of the run (and measure the buffer length we'll need) */
			while (next && next->encoding == encoding && next->codepage == codepage) {
				len += next->length;
				next = next->next;
			}
			
			/* A whole-run estimate also covers each token's buffered-state allowance. */
			capacity = g_mime_decoder_estimate_output_length (decoder, len);
			g_assert (capacity != G_MAXSIZE);
			if (capacity > outcap) {
				outbuf = g_realloc (outbuf, capacity);
				outcap = capacity;
			}
			
			/* base64 / quoted-printable decode each of the tokens... */
			outlen = 0;
			do {
				/* Note: by not resetting the decoder each loop, we effectively
				 * treat the payloads as one continuous block, thus allowing
				 * us to handle cases where a hex-encoded triplet of a
				 * quoted-printable encoded payload is split between 2 or more
				 * encoded-word tokens. */
				len = g_mime_decoder_decode (decoder, (const unsigned char *) token->text,
							    token->length, outbuf + outlen, outcap - outlen);
				token = token->next;
				outlen += len;
			} while (token != next);
			g_mime_decoder_reset (decoder);
			outptr = outbuf;
			
			/* convert the raw decoded text into UTF-8 */
			if (codepage == 65001) {
				/* slight optimization over going through iconv */
				str = (char *) outptr;
				len = outlen;
				
				while (!g_utf8_validate (str, len, (const char **) &str)) {
					len = outlen - (str - (char *) outptr);
					if (*str) {
						*str = '?';
					} else {
						/* Replace Unicode NULs after charset conversion, not as invalid UTF-8. */
						str++;
						len--;
					}
				}
				
				g_string_append_len (decoded, (char *) outptr, outlen);
			} else if ((checked && codepage < 0) ||
				   (cd = g_mime_iconv_open ("UTF-8", charset)) == (iconv_t) -1) {
				/* Unknown declarations use the fallback, not native locale aliases. */
				w(g_warning ("Cannot convert from %s to UTF-8, header display may "
					     "be corrupt: %s", charset[0] ? charset : "unspecified charset",
					     g_strerror (checked && codepage < 0 ? EINVAL : errno)));
				
				str = decode_8bit (options, (char *) outptr, outlen, &len);
				g_string_append_len (decoded, str, len);
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
			str = decode_8bit (options, token->text, token->length, &len);
			g_string_append_len (decoded, str, len);
			g_free (str);
		} else {
			g_string_append_len (decoded, token->text, token->length);
		}
		
		token = next;
	}
	
	g_clear_object (&base64);
	g_clear_object (&qp);
	g_free (outbuf);

	if ((nul = memchr (decoded->str, 0, decoded->len))) {
		const char *start = decoded->str, *end = start + decoded->len;
		GString *value = g_string_sized_new (decoded->len);

		do {
			g_string_append_len (value, start, nul - start);
			g_string_append (value, "\357\277\275");
			start = nul + 1;
		} while ((nul = memchr (start, 0, end - start)));

		g_string_append_len (value, start, end - start);
		g_string_free (decoded, TRUE);
		decoded = value;
	}
	return g_string_free (decoded, FALSE);
}


static char *
header_decode_text (GMimeParserOptions *options, const char *text, size_t length, const char **charset, gint64 offset,
		    gboolean checked)
{
	rfc2047_token *tokens;
	char *decoded;

	if (text == NULL) {
		if (charset)
			*charset = NULL;

		return g_strdup ("");
	}

	tokens = tokenize_rfc2047_text (options, text, length, offset, FALSE);
	decoded = rfc2047_decode_tokens (options, tokens, length, charset, checked);
	rfc2047_token_list_free (tokens);

	return decoded;
}


/**
 * _g_mime_utils_header_decode_text:
 * @text: header text to decode
 * @options: (nullable): a #GMimeParserOptions or %NULL
 * @length: the number of input bytes, excluding any terminating NUL
 * @charset: (optional): if non-%NULL, receives the most frequent encoded-word charset, or %NULL if none
 * @offset: header offset, only used for reporting a #GMimeParserWarning
 *
 * Decodes an rfc2047 encoded 'text' header, replacing literal and decoded
 * NUL characters with the Unicode replacement character U+FFFD.
 *
 * Returns: a newly allocated UTF-8 string representing the the decoded
 * header.
 **/
char *
_g_mime_utils_header_decode_text (GMimeParserOptions *options, const char *text, size_t length,
				  const char **charset, gint64 offset)
{
	return header_decode_text (options, text, length, charset, offset, FALSE);
}

char *
_g_mime_utils_header_decode_text_full (GMimeParserOptions *options, const char *text, size_t length,
				     gint64 offset)
{
	return header_decode_text (options, text, length, NULL, offset, TRUE);
}


/**
 * g_mime_utils_header_decode_text:
 * @text: header text to decode
 * @options: (nullable): a #GMimeParserOptions or %NULL
 *
 * Decodes an rfc2047 encoded 'text' header, replacing decoded NUL characters
 * with the Unicode replacement character U+FFFD.
 *
 * Returns: a newly allocated UTF-8 string representing the the decoded
 * header.
 **/
char *
g_mime_utils_header_decode_text (GMimeParserOptions *options, const char *text)
{
	return _g_mime_utils_header_decode_text (options, text, text ? strlen (text) : 0, NULL, -1);
}


/**
 * _g_mime_utils_header_decode_phrase:
 * @phrase: header to decode
 * @options: (nullable): a #GMimeParserOptions or %NULL
 * @length: the number of input bytes, excluding any terminating NUL
 * @charset: (optional): if non-%NULL, receives the most frequent encoded-word charset, or %NULL if none
 * @offset: header offset, only used for reporting a #GMimeParserWarning
 *
 * Decodes an rfc2047 encoded 'phrase' header, replacing literal and decoded
 * NUL characters with the Unicode replacement character U+FFFD.
 *
 * Returns: a newly allocated UTF-8 string representing the the decoded
 * header.
 **/
char *
_g_mime_utils_header_decode_phrase (GMimeParserOptions *options, const char *phrase, size_t length,
				    const char **charset, gint64 offset)
{
	rfc2047_token *tokens;
	char *decoded;
	
	if (phrase == NULL) {
		if (charset)
			*charset = NULL;
		
		return g_strdup ("");
	}
	
	tokens = tokenize_rfc2047_phrase (options, phrase, length, offset, FALSE);
	decoded = rfc2047_decode_tokens (options, tokens, length, charset, FALSE);
	rfc2047_token_list_free (tokens);
	
	return decoded;
}


/**
 * g_mime_utils_header_decode_phrase:
 * @phrase: header to decode
 * @options: (nullable): a #GMimeParserOptions or %NULL
 *
 * Decodes an rfc2047 encoded 'phrase' header, replacing decoded NUL characters
 * with the Unicode replacement character U+FFFD.
 *
 * Returns: a newly allocated UTF-8 string representing the the decoded
 * header.
 **/
char *
g_mime_utils_header_decode_phrase (GMimeParserOptions *options, const char *phrase)
{
	return _g_mime_utils_header_decode_phrase (options, phrase, phrase ? strlen (phrase) : 0, NULL, -1);
}


char *
_g_mime_utils_convert_from_utf8 (const char *text, gsize length, const char **charset, gsize *converted_length)
{
	const char *name, *utf8 = g_mime_charset_iconv_name ("UTF-8");
	char *converted, *roundtrip = NULL;
	gsize roundtrip_len;
	GError *error = NULL;

	*converted_length = length;
	if (!g_ascii_strcasecmp (*charset, "utf-8"))
		return g_strndup (text, length);

	/* A requested charset may not represent newly added characters. Check that
	 * conversion is lossless instead of silently replacing them; otherwise use UTF-8. */
	name = g_mime_charset_iconv_name (*charset);
	converted = g_convert (text, length, name, utf8, NULL, converted_length, &error);

	if (converted) {
		if (*converted_length <= G_MAXSSIZE) {
			roundtrip = g_convert (converted, *converted_length, utf8, name, NULL, &roundtrip_len, &error);
			if (roundtrip && roundtrip_len == length && !memcmp (roundtrip, text, length)) {
				g_free (roundtrip);
				return converted;
			}
		} else {
			g_warning ("Cannot check charset '%s': converted text exceeds G_MAXSSIZE", *charset);
		}
	}

	if (error && !g_error_matches (error, G_CONVERT_ERROR, G_CONVERT_ERROR_NO_CONVERSION) &&
	    !g_error_matches (error, G_CONVERT_ERROR, G_CONVERT_ERROR_ILLEGAL_SEQUENCE))
		g_warning ("Cannot encode charset '%s': %s", *charset, error->message);

	g_clear_error (&error);
	g_free (roundtrip);
	g_free (converted);

	*charset = "utf-8";
	*converted_length = length;

	return g_strndup (text, length);
}


static const char *
rfc2047_charset_name (const char *charset)
{
	if (!g_ascii_strcasecmp (charset, "utf-16le"))
		return "utf-16";
	if (!g_ascii_strcasecmp (charset, "utf-32le"))
		return "utf-32";
	return charset;
}

static void
rfc2047_encode_word (GString *string, const char *word, size_t len,
		     const char *charset, gushort safemask)
{
	const char *conversion_charset = charset;
	size_t enclen, pos, count = 0;
	GMimeRfc2047Encoder *encoder;
	unsigned char *encoded;
	char *converted;

	/* Generic UTF-16/32 use little-endian bytes without a BOM, like MimeKit. */
	if (!g_ascii_strcasecmp (charset, "utf-16"))
		conversion_charset = "utf-16le";
	else if (!g_ascii_strcasecmp (charset, "utf-32"))
		conversion_charset = "utf-32le";

	word = converted = _g_mime_utils_convert_from_utf8 (word, len, &conversion_charset, &len);
	charset = rfc2047_charset_name (conversion_charset);

	for (pos = 0; pos < len; pos++)
		if ((unsigned char) word[pos] > 127)
			count++;

	/* RFC 1468 recommends B encoding for ISO-2022-JP header text. */
	if (g_ascii_strncasecmp (charset, "iso-2022-jp", 11) == 0 || (double) count >= len * 0.17) {
		encoder = _g_mime_rfc2047_base64_encoder_new ();
	} else {
		encoder = _g_mime_rfc2047_quoted_printable_encoder_new (safemask & IS_PSAFE ?
			GMIME_QENCODE_MODE_PHRASE : GMIME_QENCODE_MODE_TEXT);
	}
	
	enclen = _g_mime_rfc2047_encoder_estimate_output_length (encoder, len);
	g_assert (enclen != G_MAXSIZE);
	encoded = g_malloc (enclen);
	pos = _g_mime_rfc2047_encoder_encode (encoder, (const guint8 *) word, len, encoded, enclen);

	g_string_append_printf (string, "=?%s?%c?", charset, _g_mime_rfc2047_encoder_get_encoding (encoder));
	g_string_append_len (string, (const char *) encoded, pos);
	g_string_append (string, "?=");
	
	g_object_unref (encoder);
	g_free (encoded);
	g_free (converted);
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
	gsize byte_count;
	gsize encode_count;
	gsize quoted_pairs;
} rfc822_word;

#define rfc822_word_free(word) g_slice_free (rfc822_word, word)
#define rfc822_word_new() g_slice_new0 (rfc822_word)

static gsize
rfc2047_character_byte_count (const char *charset, const char *start, const char *end, gunichar c)
{
	gsize length = end - start;
	GError *error = NULL;
	char *converted;

	if (!g_ascii_strcasecmp (charset, "utf-8"))
		return length;

	/* iconv extensions can discard input, so retain the lossless conversion path. */
	if (strstr (charset, "//")) {
		converted = _g_mime_utils_convert_from_utf8 (start, length, &charset, &length);
		g_free (converted);
		return length;
	}

	/* Size estimates allow replacement, but emitted words remain lossless. */
	converted = g_convert_with_fallback (start, length, g_mime_charset_iconv_name (charset),
					    g_mime_charset_iconv_name ("utf-8"),
					    c > 0xffff ? "??" : "?", NULL, &length, &error);
	if (!converted) {
		g_clear_error (&error);
		length = end - start;
		converted = _g_mime_utils_convert_from_utf8 (start, length, &charset, &length);
	}

	g_free (converted);

	return length;
}

static gsize
rfc2047_estimate_word_length (const char *charset, int encoding, gsize byte_count, gsize encode_count)
{
	gsize length;

	if (encoding == 0)
		charset = "us-ascii";
	else if (encoding == 1)
		charset = "iso-8859-1";

	length = strlen (rfc2047_charset_name (charset ? charset : "utf-8")) + 7;

	if (byte_count > (G_MAXSIZE - length) / 3)
		return G_MAXSIZE;

	if ((double) encode_count < byte_count * 0.17)
		return length + byte_count + encode_count * 2;

	return length + ((byte_count + 2) / 3) * 4;
}

/* okay, so 'unstructured text' fields don't actually contain 'word'
 * tokens, but we can group stuff similarly... */
static rfc822_word *
rfc2047_encode_get_rfc822_words (GMimeFormatOptions *options, const char *charset, const char *in, gboolean phrase)
{
	const char *conversion_charset = charset ? charset : "utf-8";
	guint maxline = g_mime_format_options_get_max_line_length (options);
	rfc822_word words, saved, *tail, *word, *first, *next;
	const char *inptr, *start;
	gboolean quoted = FALSE, escaped = FALSE;
	gsize comment_depth = 0;

	if (!g_ascii_strcasecmp (conversion_charset, "utf-16"))
		conversion_charset = "utf-16le";
	else if (!g_ascii_strcasecmp (conversion_charset, "utf-32"))
		conversion_charset = "utf-32le";
	
	tail = (rfc822_word *) &words;
	words.next = NULL;
	word = rfc822_word_new ();
	word->start = in;
	
	inptr = in;
	while (inptr && *inptr) {
		gsize length;
		gboolean closed = FALSE;
		gunichar c;
		
		start = inptr;
		c = g_utf8_get_char (inptr);
		if (!g_unichar_validate (c)) {
			w(g_warning ("Invalid UTF-8 sequence encountered"));
			inptr++;
			continue;
		}
		
		inptr = g_utf8_next_char (inptr);
		
		if (!quoted && comment_depth == 0 && c < 256 && is_blank (c)) {
			if (word->byte_count > 0) {
				word->end = start;
				tail->next = word;
				tail = word;
				word = rfc822_word_new ();
			}
			
			word->start = inptr;
		} else {
			saved = *word;

			if (c < 128) {
				if (is_ctrl (c)) {
					word->type = WORD_2047;
					word->encoding = MAX (word->encoding, 1);
					word->encode_count++;
				} else if (c == '?' && start > word->start && start[-1] == '=') {
					/* Literal encoded-word prefixes must not be interpreted on decoding. */
					word->type = WORD_2047;
					word->encode_count += 2;
				} else if (phrase && !is_atom (c)) {
					/* phrases can have qstring words */
					word->type = MAX (word->type, WORD_QSTRING);

					if (c == '\\') {
						word->quoted_pairs++;
						escaped = !escaped;
					} else if (c == '"') {
						if (!escaped) {
							quoted = !quoted;
							closed = !quoted;
						}
						word->quoted_pairs++;
						escaped = FALSE;
					} else if (!quoted) {
						if (c == '(') {
							comment_depth++;
						} else if (c == ')' && comment_depth > 0) {
							comment_depth--;
							closed = comment_depth == 0;
						}
					} else {
						escaped = FALSE;
					}
				}
				word->byte_count++;
			} else if (c < 256) {
				word->type = WORD_2047;
				word->encoding = MAX (word->encoding, 1);
				word->byte_count++;
				word->encode_count++;
			} else {
				length = rfc2047_character_byte_count (conversion_charset, start, inptr, c);
				word->type = WORD_2047;
				word->encoding = 2;
				word->byte_count += length;
				word->encode_count += length;
			}

			if (word->type == WORD_2047)
				length = rfc2047_estimate_word_length (charset, word->encoding, word->byte_count, word->encode_count);
			else if (word->type == WORD_QSTRING)
				length = word->byte_count + word->quoted_pairs + 2;
			else
				length = word->byte_count;

			if (length >= maxline - 1 && saved.byte_count > 0) {
				rfc822_word_t type;

				*word = saved;
				inptr = start;

				if (word->type == WORD_ATOM) {
					length = strlen ("us-ascii") + 7;
					/* Oversized charset labels must not rewind before the token. */
					if (word->byte_count > length) {
						inptr -= length;
						word->byte_count -= length;
					}
					word->type = WORD_2047;
				}

				word->end = inptr;
				type = word->type;
				tail->next = word;
				tail = word;
				word = rfc822_word_new ();
				word->start = inptr;
				word->type = type;
			} else if (closed) {
				word->end = inptr;
				tail->next = word;
				tail = word;
				word = rfc822_word_new ();
				word->start = inptr;
			}
		}
	}
	
	if (word->byte_count > 0) {
		word->end = inptr;
		tail->next = word;
	} else
		rfc822_word_free (word);

	/* Adjacent pieces can only be rejoined losslessly if all are encoded together. */
	for (first = words.next; first; first = next) {
		gboolean encoded = FALSE;

		for (word = first; ; word = word->next) {
			encoded |= word->type == WORD_2047;
			if (!word->next || word->end != word->next->start)
				break;
		}
		next = word->next;

		if (encoded)
			for (word = first; word != next; word = word->next)
				word->type = WORD_2047;
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

static gboolean
should_merge_words (guint maxline, const char *charset, rfc822_word *word, rfc822_word *next)
{
	gsize length = word->byte_count + (next->start - word->end) + next->byte_count;

	switch (word->type) {
	case WORD_ATOM:
		if (next->type == WORD_2047)
			return FALSE;
		
		return length < maxline - 1;
	case WORD_QSTRING:
		/* avoid merging with words that need to be rfc2047 encoded */
		if (next->type == WORD_2047)
			return FALSE;
		
		/* Quoted strings can contain atoms and other quoted strings without a size limit. */
		return TRUE;
	case WORD_2047:
		if (next->type == WORD_ATOM) {
			rfc822_word *lookahead = next->next;
			int natoms = 0;
			
			while (lookahead && lookahead->type == WORD_ATOM && natoms < 3) {
				lookahead = lookahead->next;
				natoms++;
			}
			
			/* Only merge atoms followed by a nearby quoted string or encoded word. */
			if (!lookahead || natoms == 3)
				return FALSE;
		}
		
		/* avoid merging with qstrings */
		if (next->type == WORD_QSTRING)
			return FALSE;
		
		length = rfc2047_estimate_word_length (charset, MAX (word->encoding, next->encoding),
						      length, word->encode_count + next->encode_count);
		return length < maxline - 1;
	default:
		return FALSE;
	}
}

static void
rfc822_word_merge (rfc822_word *word, rfc822_word *next)
{
	word->type = MAX (word->type, next->type);
	word->encoding = MAX (word->encoding, next->encoding);
	word->byte_count += (next->start - word->end) + next->byte_count;
	word->encode_count += next->encode_count;
	word->quoted_pairs += next->quoted_pairs;
	word->end = next->end;
	word->next = next->next;
	rfc822_word_free (next);
}

static void
rfc2047_encode_merge_rfc822_words (GMimeFormatOptions *options, const char *charset, rfc822_word *words)
{
	guint maxline = g_mime_format_options_get_max_line_length (options);
	rfc822_word *word, *next;
	gsize length;

	/* first pass: merge qstrings with adjacent qstrings and encwords with adjacent encwords */
	word = words;
	while (word && word->next) {
		next = word->next;
		
		if (word->type != WORD_ATOM && word->type == next->type) {
			length = word->byte_count + (next->start - word->end) + next->byte_count;

			if (word->type == WORD_2047)
				length = rfc2047_estimate_word_length (charset, MAX (word->encoding, next->encoding),
								      length, word->encode_count + next->encode_count);
			else
				length += word->quoted_pairs + next->quoted_pairs + 2;

			if (length < maxline - 1) {
				rfc822_word_merge (word, next);
				continue;
			}
		}
		
		word = next;
	}
	
	/* second pass: now merge atoms with the other words */
	word = words;
	while (word && word->next) {
		next = word->next;
		
		if (should_merge_words (maxline, charset, word, next)) {
			rfc822_word_merge (word, next);
			continue;
		}
		
		word = next;
	}
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
	gboolean mixed = g_mime_format_options_get_allow_mixed_charsets (options);
	rfc822_word *words, *word, *prev = NULL;
	const char *charset, *start;
	GString *out;
	char *outstr, *mime_charset;
	size_t len;

	mime_charset = _g_mime_charset_get_mime_name (user_charset ? user_charset : "utf-8");
	user_charset = mime_charset;
	
	if (!(words = rfc2047_encode_get_rfc822_words (options, user_charset, in, safemask & IS_PSAFE))) {
		g_free (mime_charset);
		return g_strdup (in);
	}
	
	if (words->next)
		rfc2047_encode_merge_rfc822_words (options, user_charset, words);
	
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
				g_string_append_c (out, safemask & IS_PSAFE ? '\t' : ' ');
			} else {
				len = word->end - word->start;
				start = word->start;
			}
			
			if (!mixed || word->encoding >= 2) {
				charset = user_charset ? user_charset : "utf-8";
			} else {
				charset = word->encoding == 0 ? "us-ascii" : "iso-8859-1";
			}

			rfc2047_encode_word (out, start, len, charset, safemask);
			break;
		}
		
		rfc822_word_free (prev);
		
		prev = word;
		word = word->next;
	}
	
	rfc822_word_free (prev);
	
	outstr = g_string_free (out, FALSE);
	g_free (mime_charset);
	
	return outstr;
}


/**
 * g_mime_utils_header_encode_phrase:
 * @options: (nullable): a #GMimeFormatOptions or %NULL
 * @phrase: unencoded UTF-8 phrase to encode
 * @charset: (nullable): the charset to use or %NULL for UTF-8 by default
 *
 * Encodes a 'phrase' header according to the rules in rfc2047.
 * Literal encoded-word prefixes are encoded to preserve their text rather
 * than being interpreted as pre-encoded content.
 * Phrase tokenization keeps quoted text and nested comments together.
 * Contiguous split pieces use encoded words consistently when any piece
 * needs encoding, preserving literal quotes, parentheses and whitespace.
 * Token splitting and word merging use charset-aware estimates and the configured
 * maximum line length without splitting Unicode scalars. Adjacent encoded words
 * are separated by tabs, with their original
 * whitespace included in the encoded payload.
 * Encoded words use the requested charset unless mixed charsets are enabled
 * on @options, allowing US-ASCII and ISO-8859-1 words. Other words use UTF-8
 * when no charset was requested.
 * Common charset aliases use lowercase MIME labels consistently for conversion
 * and size estimates.
 * Encoded words use UTF-8 if @charset is unavailable or cannot represent
 * their text without loss. UTF-16 and UTF-32 encoded words retain embedded
 * zero bytes; their little-endian forms use BOM-free bytes and the generic
 * charset label. ISO-2022-JP encoded words always use Base64.
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
 * @text: unencoded UTF-8 header text to encode
 * @charset: (nullable): the charset to use or %NULL for UTF-8 by default
 *
 * Encodes a 'text' header according to the rules in rfc2047.
 * Literal encoded-word prefixes are encoded to preserve their text rather
 * than being interpreted as pre-encoded content.
 * Token splitting and word merging use charset-aware estimates and the configured
 * maximum line length without splitting Unicode scalars. Adjacent encoded words
 * are separated by spaces, with their original
 * whitespace included in the encoded payload.
 * Encoded words use the requested charset unless mixed charsets are enabled
 * on @options, allowing US-ASCII and ISO-8859-1 words. Other words use UTF-8
 * when no charset was requested.
 * Common charset aliases use lowercase MIME labels consistently for conversion
 * and size estimates.
 * Encoded words use UTF-8 if @charset is unavailable or cannot represent
 * their text without loss. UTF-16 and UTF-32 encoded words retain embedded
 * zero bytes; their little-endian forms use BOM-free bytes and the generic
 * charset label. ISO-2022-JP encoded words always use Base64.
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
	size_t maxline = g_mime_format_options_get_max_line_length (options);
	const char *newline = g_mime_format_options_get_newline (options);
	rfc2047_token *token, *next;
	size_t lwsp, tab, len, n;
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
				
				if (token->text[n] == '\n') {
					g_string_append (output, newline);
					lwsp = tab = 0;
					len = 0;
				} else {
					g_string_append_c (output, token->text[n]);
					len++;
				}
			}
			
			if (len == 0 && token->next) {
				g_string_append_c (output, structured ? '\t' : ' ');
				len = 1;
			}
		} else if (token->encoding != 0) {
			if (len + token->word_length > maxline) {
				if (tab != 0) {
					/* tabs are the perfect breaking opportunity... */
					g_string_insert (output, tab, newline);
					len = (lwsp - tab) + 1;
				} else if (lwsp != 0) {
					/* break just before the last lwsp character */
					g_string_insert (output, lwsp, newline);
					len = 1;
				} else if (len > 1 && token != tokens) {
					/* force a line break... */
					g_string_append (output, newline);
					g_string_append_c (output, structured ? '\t' : ' ');
					len = 1;
				}
			}
			
			/* Note: if the encoded-word token is longer than the fold length, oh well...
			 * it probably just means that we are folding a header written by a user-agent
			 * with a different max line length than ours. */
			
			g_string_append_len (output, token->word, token->word_length);
			len += token->word_length;
			lwsp = 0;
			tab = 0;
		} else if (len + token->length > maxline) {
			if (tab != 0) {
				/* tabs are the perfect breaking opportunity... */
				g_string_insert (output, tab, newline);
				len = (lwsp - tab) + 1;
			} else if (lwsp != 0) {
				/* break just before the last lwsp character */
				g_string_insert (output, lwsp, newline);
				len = 1;
			} else if (len > 1 && token != tokens) {
				/* force a line break... */
				g_string_append (output, newline);
				g_string_append_c (output, structured ? '\t' : ' ');
				len = 1;
			}
			
			if (token->length >= maxline) {
				/* the token is longer than the allowable line length,
				 * so we'll have to break it apart... */
				for (n = 0; n < token->length;) {
					size_t count;

					if (len >= maxline) {
						g_string_append (output, newline);
						g_string_append_c (output, structured ? '\t' : ' ');
						len = 1;
					}

					count = MIN (token->length - n, maxline - len);
					g_string_append_len (output, token->text + n, count);
					len += count;
					n += count;
				}
			} else {
				g_string_append_len (output, token->text, token->length);
				len += token->length;
			}
			
			lwsp = 0;
			tab = 0;
		} else {
			g_string_append_len (output, token->text, token->length);
			len += token->length;
			lwsp = 0;
			tab = 0;
		}
		
		next = token->next;
		rfc2047_token_free (token);
		token = next;
	}
	
	if (output->str[output->len - 1] != '\n')
		g_string_append (output, newline);
	
	return g_string_free (output, FALSE);
}


/**
 * g_mime_utils_structured_header_fold:
 * @options: (nullable): a #GMimeParserOptions or %NULL
 * @format: (nullable): a #GMimeFormatOptions or %NULL
 * @header: header field and value string
 *
 * Folds a structured header according to the rules in rfc822.
 * Uses the configured maximum line length and newline format, keeping
 * encoded words intact and using tabs for inserted continuations.
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
	
	len = strlen (value);
	tokens = tokenize_rfc2047_phrase (options, value, len, -1, TRUE);
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
		return g_strdup (g_mime_format_options_get_newline (format));
	
	len = strlen (value);
	tokens = tokenize_rfc2047_phrase (options, value, len, -1, TRUE);
	
	return header_fold_tokens (format, field, value, len, tokens, TRUE, FALSE);
}


/**
 * g_mime_utils_unstructured_header_fold:
 * @options: (nullable): a #GMimeParserOptions or %NULL
 * @format: (nullable): a #GMimeFormatOptions or %NULL
 * @header: header field and value string
 *
 * Folds an unstructured header according to the rules in rfc822.
 * Uses the configured maximum line length and newline format. Encoded words
 * remain intact; oversized unencoded tokens may be split across several
 * lines, with spaces introducing continuations.
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
	
	len = strlen (value);
	tokens = tokenize_rfc2047_text (options, value, len, -1, TRUE);
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
		return g_strdup (g_mime_format_options_get_newline (format));
	
	len = strlen (value);
	tokens = tokenize_rfc2047_text (options, value, len, -1, TRUE);
	
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
 * g_mime_utils_header_unfold:
 * @value: raw header value
 *
 * Unfolds a raw header value according to the rules in rfc822.
 *
 * Returns: an allocated string containing the unfolded header value.
 **/
char *
g_mime_utils_header_unfold (const char *value)
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
