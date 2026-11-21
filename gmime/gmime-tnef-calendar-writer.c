/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*  GMime
 *  Copyright (C) Jeffrey Stedfast
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

#include "gmime-tnef-calendar-private.h"

static void
append (CalWriter *writer, const char *str, gsize n)
{
	if (writer->failed || n > writer->limit - writer->out->len) {
		writer->failed = TRUE;
		return;
	}

	g_string_append_len (writer->out, str, n);
}

/* RFC 5545 section 3.1: fold between UTF-8 code points, counting octets
 * rather than characters. The continuation space consumes one of the 75. */
static void
unit (CalWriter *writer, const char *str, guint n)
{
	if (writer->column + n > 75) {
		append (writer, "\r\n ", 3);
		writer->column = 1;
	}

	append (writer, str, n);
	writer->column += n;
}

enum { RAW, TEXT, PARAM };

static void
emit (CalWriter *writer, const char *str, int mode)
{
	while (*str && !writer->failed) {
		if ((guchar) *str >= 0x80) {
			gunichar c = g_utf8_get_char_validated (str, -1);

			if (c == (gunichar) -1 || c == (gunichar) -2) {
				unit (writer, "\xef\xbf\xbd", 3);
				str++;
			} else {
				guint n = g_utf8_skip[(guchar) *str];
				unit (writer, str, n);
				str += n;
			}

			continue;
		}

		if (mode == TEXT && (*str == '\r' || *str == '\n')) {
			if (*str == '\r' && str[1] == '\n')
				str++;

			unit (writer, "\\", 1);
			unit (writer, "n", 1);
		} else if (mode == PARAM && *str == '"') {
			/* Even a quoted parameter cannot contain DQUOTE under
			 * RFC 5545 section 3.2. Avoid requiring RFC 6868 decoding. */
			unit (writer, "'", 1);
		} else if ((*str >= 0x20 && *str != 0x7f) || *str == '\t') {
			if (mode == TEXT && (*str == ',' || *str == ';' || *str == '\\'))
				unit (writer, "\\", 1);

			unit (writer, str, 1);
		}

		str++;
	}
}

void
cal_begin (CalWriter *writer, const char *name)
{
	emit (writer, name, RAW);
}

void
cal_param (CalWriter *writer, const char *name, const char *value)
{
	gboolean quote;

	if (!value || writer->failed)
		return;

	quote = strpbrk (value, ":;,") != NULL;
	unit (writer, ";", 1);
	emit (writer, name, RAW);
	unit (writer, "=", 1);

	if (quote)
		unit (writer, "\"", 1);

	emit (writer, value, PARAM);

	if (quote)
		unit (writer, "\"", 1);
}

static void
finish (CalWriter *writer)
{
	append (writer, "\r\n", 2);
	writer->column = 0;
}

void
cal_value (CalWriter *writer, const char *value, gboolean text)
{
	unit (writer, ":", 1);
	emit (writer, value, text ? TEXT : RAW);
	finish (writer);
}

void
cal_prop (CalWriter *writer, const char *name, const char *value)
{
	if (value) {
		cal_begin (writer, name);
		cal_value (writer, value, FALSE);
	}
}

void
cal_text (CalWriter *writer, const char *name, const char *value)
{
	if (value) {
		cal_begin (writer, name);
		cal_value (writer, value, TRUE);
	}
}

void
cal_categories (CalWriter *writer, GMimeTnefProperty *p)
{
	gboolean started = FALSE;
	guint i;

	for (i = 0; p && !writer->failed && i < g_mime_tnef_property_get_count (p); i++) {
		const GValue *v = g_mime_tnef_property_get_value (p, i);
		const char *s = G_VALUE_HOLDS_STRING (v) ? g_value_get_string (v) : NULL;

		if (!s || !*s)
			continue;

		if (!started) {
			cal_begin (writer, "CATEGORIES:");
			started = TRUE;
		} else {
			unit (writer, ",", 1);
		}

		emit (writer, s, TEXT);
	}

	if (started)
		finish (writer);
}

char *
cal_format_time (gint64 stamp, gboolean date, gboolean utc)
{
	GDateTime *time = g_date_time_new_from_unix_utc (stamp);
	char *str;

	if (!time)
		return NULL;

	/* Don't use locale-dependent digits or strftime's platform-specific %Y. */
	str = g_strdup_printf ("%04d%02d%02dT%02d%02d%02d%s", g_date_time_get_year (time),
		g_date_time_get_month (time), g_date_time_get_day_of_month (time), g_date_time_get_hour (time),
		g_date_time_get_minute (time), g_date_time_get_second (time), utc ? "Z" : "");

	if (date)
		str[8] = '\0';

	g_date_time_unref (time);

	return str;
}
