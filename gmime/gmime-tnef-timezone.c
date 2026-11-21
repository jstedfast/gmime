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

#include <stdlib.h>

#include "gmime-tnef-calendar-private.h"

#define U16 _g_mime_tnef_get_uint16
#define U32 _g_mime_tnef_get_uint32

static CalTransition
read_transition (const guint8 *data)
{
	CalTransition transition = { U16 (data), U16 (data + 2), U16 (data + 4), U16 (data + 6),
			    U16 (data + 8), U16 (data + 10), U16 (data + 12), U16 (data + 14) };

	return transition;
}

static gboolean
valid_transition (CalTransition transition)
{
	return transition.month >= 1 && transition.month <= 12 && transition.hour < 24 && transition.minute < 60 && transition.second < 60 &&
		transition.day >= 1 && (transition.year ? transition.day <= 31 : (transition.day <= 5 && transition.weekday <= 6));
}

CalZone *
cal_zone_parse (GBytes *bytes, gboolean definition, const char *fallback)
{
	const guint8 *p, *std, *dst;
	int bias, sbias, dbias, i;
	char *id = NULL;
	CalZone *z;
	gsize n;

	if (!bytes)
		return NULL;

	p = g_bytes_get_data (bytes, &n);

	if (definition) {
		guint end, len, count;
		const guint8 *rule = NULL;

		if (n < 10 || p[0] != 2 || (end = 4U + U16 (p + 2)) < 10 || end > n)
			return NULL;

		/* cbHeader locates the first TZRule; cRules is the final header
		 * field, after the variable-length UTF-16 key name. */
		count = MIN ((n - end) / 66, U16 (p + end - 2));

		if (!count)
			return NULL;

		/* Prefer the rule marked TZRULE_FLAG_EFFECTIVE_TZREG
		 * ([MS-OXOCAL] 2.2.1.41.1), otherwise use the last complete rule. */
		for (i = 0; (guint) i < count; i++) {
			rule = p + end + i * 66;

			if (U16 (rule + 4) & 2)
				break;
		}

		len = U16 (p + 6) * 2U;

		if (len && len <= end - 10)
			id = g_convert ((const char *) p + 8, len, "UTF-8", "UTF-16LE", NULL, NULL, NULL);

		p = rule + 22;
		std = p + 12;
		dst = p + 28;
	} else {
		if (n < 48)
			return NULL;

		/* In the legacy TZReg format each SYSTEMTIME is preceded by
		 * a separate year field, unlike the transitions in a TZRule. */
		std = p + 14;
		dst = p + 32;
	}

	bias = (gint32) U32 (p);
	sbias = (gint32) U32 (p + 4);
	dbias = (gint32) U32 (p + 8);

	if (bias < -1440 || bias > 1440 || sbias < -1440 || sbias > 1440 || dbias < -1440 || dbias > 1440) {
		g_free (id);
		return NULL;
	}

	z = g_new0 (CalZone, 1);
	/* Windows adds the bias to local time to get UTC. iCalendar uses
	 * the opposite sign: local time = UTC + offset. */
	z->standard = -(bias + sbias);
	z->daylight = -(bias + dbias);
	z->std_date = read_transition (std);
	z->dst_date = read_transition (dst);

	if (!z->std_date.month || !z->dst_date.month) {
		/* A missing transition cannot be inferred from its partner. */
		CalTransition fixed = { 0 };
		z->std_date = z->dst_date = fixed;
		z->daylight = z->standard;
	} else if (!valid_transition (z->std_date) || !valid_transition (z->dst_date)) {
		g_free (id);
		g_free (z);
		return NULL;
	}

	if (!id || !*g_strstrip (id)) {
		g_free (id);
		id = g_strdup (fallback);
	}

	if (!id || !*g_strstrip (id)) {
		g_free (id);
		id = g_strdup_printf ("UTC%c%02d:%02d", z->standard < 0 ? '-' : '+',
				      abs (z->standard) / 60, abs (z->standard) % 60);
	}

	/* A TZID occurs both in a TEXT value and a parameter. Use an ID
	 * representable in both (not two different escaped spellings). */
	for (i = 0; id[i]; i++) {
		if ((guchar) id[i] < 32 || id[i] == '"' || id[i] == 127)
			id[i] = '_';
	}

	z->id = id;

	return z;
}

void
cal_zone_free (CalZone *zone)
{
	if (zone) {
		g_free (zone->id);
		g_free (zone);
	}
}

static gint64
transition (CalTransition transition, int year)
{
	int days = g_date_get_days_in_month (transition.month, year);
	int day = MIN (transition.day, days);
	GDateTime *date;
	gint64 stamp;

	/* With no absolute year, day is the weekday's ordinal within the
	 * month; five means its last occurrence, not necessarily its fifth. */
	if (!transition.year) {
		GDate first;

		g_date_clear (&first, 1);
		g_date_set_dmy (&first, 1, transition.month, year);

		day = 1 + (7 + transition.weekday - g_date_get_weekday (&first)) % 7 + 7 * (transition.day - 1);

		if (day > days)
			day -= 7;
	}

	date = g_date_time_new_utc (year, transition.month, day, transition.hour, transition.minute, transition.second);
	stamp = g_date_time_to_unix (date);
	g_date_time_unref (date);

	return stamp;
}

static gboolean
is_daylight (CalZone *zone, gint64 utc)
{
	gint64 start, end;
	GDateTime *date;
	int year;

	if (!zone->dst_date.month)
		return FALSE;

	date = g_date_time_new_from_unix_utc (utc + zone->standard * 60);

	if (!date)
		return FALSE;

	year = g_date_time_get_year (date);
	g_date_time_unref (date);

	/* Each transition is expressed in the local time *before* the change. */
	start = transition (zone->dst_date, year) - zone->standard * 60;
	end = transition (zone->std_date, year) - zone->daylight * 60;

	return start < end ? utc >= start && utc < end : utc >= start || utc < end;
}

gint64
cal_local (CalZone *zone, gint64 utc)
{
	return zone ? utc + (is_daylight (zone, utc) ? zone->daylight : zone->standard) * 60 : utc;
}

gint64
cal_utc (CalZone *zone, gint64 local)
{
	if (!zone)
		return local;

	return local - (is_daylight (zone, local - zone->daylight * 60) ? zone->daylight : zone->standard) * 60;
}

static void
observance (CalWriter *writer, const char *name, CalTransition date, int from, int to)
{
	static const char *days[] = { "SU", "MO", "TU", "WE", "TH", "FR", "SA" };
	char buf[96];

	cal_prop (writer, "BEGIN", name);

	if (date.month) {
		char *time = cal_format_time (transition (date, 1601), FALSE, FALSE);

		cal_prop (writer, "DTSTART", time);
		g_free (time);

		if (date.year)
			g_snprintf (buf, sizeof buf, "FREQ=YEARLY;BYMONTHDAY=%d;BYMONTH=%d", date.day, date.month);
		else
			g_snprintf (buf, sizeof buf, "FREQ=YEARLY;BYDAY=%d%s;BYMONTH=%d",
				    date.day == 5 ? -1 : date.day, days[date.weekday], date.month);

		cal_prop (writer, "RRULE", buf);
	} else {
		cal_prop (writer, "DTSTART", "16010101T000000");
	}

	g_snprintf (buf, sizeof buf, "%c%02d%02d", from < 0 ? '-' : '+', abs (from) / 60, abs (from) % 60);
	cal_prop (writer, "TZOFFSETFROM", buf);
	g_snprintf (buf, sizeof buf, "%c%02d%02d", to < 0 ? '-' : '+', abs (to) / 60, abs (to) % 60);
	cal_prop (writer, "TZOFFSETTO", buf);
	cal_prop (writer, "END", name);
}

void
cal_write_zone (CalWriter *writer, CalZone *zone)
{
	CalTransition fixed = { 0 };

	cal_prop (writer, "BEGIN", "VTIMEZONE");
	cal_text (writer, "TZID", zone->id);

	observance (writer, "STANDARD", zone->dst_date.month ? zone->std_date : fixed, zone->daylight, zone->standard);

	if (zone->dst_date.month)
		observance (writer, "DAYLIGHT", zone->dst_date, zone->standard, zone->daylight);

	cal_prop (writer, "END", "VTIMEZONE");
}
