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

#include "gmime-tnef-calendar-private.h"

typedef struct {
	const guint8 *next;
	gsize left;
	gboolean bad;
} Cursor;

static const guint8 *
take (Cursor *cursor, guint64 n)
{
	const guint8 *next = cursor->next;

	if (cursor->bad || n > cursor->left) {
		cursor->bad = TRUE;
		return NULL;
	}

	cursor->next += (gsize) n;
	cursor->left -= (gsize) n;

	return next;
}

static guint32
u32 (Cursor *cursor)
{
	const guint8 *next = take (cursor, 4);

	return next ? _g_mime_tnef_get_uint32 (next) : 0;
}

static guint16
u16 (Cursor *cursor)
{
	const guint8 *next = take (cursor, 2);

	return next ? _g_mime_tnef_get_uint16 (next) : 0;
}

static void
read_string (Cursor *cursor, gboolean wide, const char *charset, char **dest)
{
	const guint8 *next;
	guint len;

	if (!wide)
		u16 (cursor); /* length including the terminating NUL */

	len = u16 (cursor) * (wide ? 2 : 1);
	next = take (cursor, len);

	if (next && dest) {
		GBytes *b = g_bytes_new_static (next, len);

		g_free (*dest);

		*dest = _g_mime_tnef_decode_string (charset, b, wide);
		g_bytes_unref (b);
	}
}

static void
clear_exception (gpointer data)
{
	CalException *ex = data;

	g_free (ex->subject);
	g_free (ex->location);
}

void
cal_recur_free (CalRecur *recur)
{
	if (recur) {
		g_free (recur->rule);
		g_array_unref (recur->exceptions);
		g_array_unref (recur->deleted);
		g_free (recur);
	}
}

static gboolean
gregorian (guint cal, gboolean hijri)
{
	return (!hijri || cal != 0) && (cal <= 5 || cal == 7 || (cal >= 9 && cal <= 12));
}

static const char * const days[] = { "SU", "MO", "TU", "WE", "TH", "FR", "SA" };

static void
byday (GString *str, guint mask)
{
	char sep = '=';
	guint i;
	
	g_string_append (str, ";BYDAY");

	for (i = 0; i < 7; i++) {
		if (mask & (1u << i)) {
			g_string_append_c (str, sep);
			g_string_append (str, days[i]);
			sep = ',';
		}
	}
}

/* Preserve the series even if its optional ExceptionInfo/ExtendedException
 * suffix is damaged. Track original dates of *all* modifications, including
 * those beyond the export budget: they must not be emitted as EXDATEs. */
static void
exceptions (Cursor *cursor, CalRecur *recur, guint version, const char *charset, guint max, GHashTable *modified,
	    TnefCalendarLoss loss, gpointer data)
{
	guint count = u16 (cursor), i;
	guint16 *flags;

	if (count * 14u > cursor->left) {
		cursor->bad = TRUE;
		return;
	}

	flags = g_new (guint16, count);

	/* ExceptionInfo ([MS-OXOCAL] 2.2.1.44.2). All three timestamps are
	 * local times in minutes since midnight on January 1, 1601. */
	for (i = 0; i < count && !cursor->bad; i++) {
		CalException ex = { 0 };
		gboolean keep = i < max;

		ex.start = u32 (cursor);
		ex.end = u32 (cursor);
		ex.original = u32 (cursor);
		ex.flags = flags[i] = u16 (cursor);

		if (ex.flags & ~0x3ffu) cursor->bad = TRUE;
		if (ex.flags & 1) read_string (cursor, FALSE, charset, keep ? &ex.subject : NULL);
		if (ex.flags & 2) u32 (cursor); /* MeetingType */
		if (ex.flags & 4) ex.delta = (gint32) u32 (cursor);
		if (ex.flags & 8) ex.reminder = u32 (cursor) != 0;
		if (ex.flags & 16) read_string (cursor, FALSE, charset, keep ? &ex.location : NULL);
		if (ex.flags & 32) ex.busy = (gint32) u32 (cursor);
		if (ex.flags & 64) u32 (cursor); /* Attachment */
		if (ex.flags & 128) ex.all_day = u32 (cursor) != 0;
		if (ex.flags & 256) u32 (cursor); /* AppointmentColor */

		g_hash_table_add (modified, GUINT_TO_POINTER (ex.original / 1440));

		if (keep)
			g_array_append_val (recur->exceptions, ex);
	}

	take (cursor, u32 (cursor)); /* ReservedBlock1 */

	/* Each ExceptionInfo has a corresponding ExtendedException
	 * ([MS-OXOCAL] 2.2.1.44.3). Its Unicode subject and location, when
	 * present, supersede the 8-bit strings read in the first pass. */
	for (i = 0; i < count && !cursor->bad; i++) {
		CalException *ex = i < max ? &g_array_index (recur->exceptions, CalException, i) : NULL;

		if (version >= 0x3009)
			take (cursor, u32 (cursor)); /* ChangeHighlight */

		take (cursor, u32 (cursor)); /* ReservedBlockEE1 */

		if (!(flags[i] & 17))
			continue;

		take (cursor, 12); /* StartDateTime, EndDateTime, OriginalStartDate */
		if (flags[i] & 1) read_string (cursor, TRUE, charset, ex ? &ex->subject : NULL);
		if (flags[i] & 16) read_string (cursor, TRUE, charset, ex ? &ex->location : NULL);

		take (cursor, u32 (cursor)); /* ReservedBlockEE2 */
	}

	g_free (flags);

	if (!cursor->bad && count > max)
		loss (CAL_LIMIT, "Modified occurrences exceeding the calendar exception count limit were omitted.", data);
}

CalRecur *
cal_recur_parse (GBytes *blob, const char *charset, CalZone *zone, gboolean date, guint max,
		 TnefCalendarLoss loss, gpointer data)
{
	guint32 period, mask = 0, monthday = 0, week = 0, until, count, wkst, ndel, start, end, offset, version, i;
	const guint8 *deleted;
	GHashTable *modified;
	guint16 pattern, cal;
	Cursor cursor = { 0 };
	GString *str = NULL;
	CalRecur *recur;

	cursor.next = g_bytes_get_data (blob, &cursor.left);

	/* PidLidAppointmentRecur begins with a RecurrencePattern
	 * ([MS-OXOCAL] 2.2.1.44.1). Its first field is ReaderVersion. */
	if (cursor.left > CAL_MAX_BYTES || u16 (&cursor) != 0x3004)
		goto invalid;

	take (&cursor, 4); /* WriterVersion, RecurFrequency */
	pattern = u16 (&cursor);
	cal = u16 (&cursor);
	u32 (&cursor); /* FirstDateTime */
	period = u32 (&cursor);
	u32 (&cursor); /* SlidingFlag */

	if (!gregorian (cal, pattern >= 10)) {
		loss (CAL_UNSUPPORTED, "A non-Gregorian recurrence was exported as a single occurrence.", data);
		return NULL;
	}

	/* PatternTypeSpecific has no fixed size: weekly patterns carry a
	 * weekday mask; monthly patterns carry a day or a mask and ordinal. */
	if (pattern >= 10 && pattern <= 12) pattern -= 8;
	if (pattern == 1 || pattern == 3) mask = u32 (&cursor);
	if (pattern == 2 || pattern == 4) monthday = u32 (&cursor);
	if (pattern == 3) week = u32 (&cursor);

	until = u32 (&cursor);
	count = u32 (&cursor);
	wkst = u32 (&cursor);
	ndel = u32 (&cursor);

	/* A compact array of dates expands into one content line per entry. */
	if (ndel > 65536)
		goto invalid;

	deleted = take (&cursor, ndel * 4u);
	take (&cursor, (guint64) u32 (&cursor) * 4); /* ModifiedInstanceDates */
	start = u32 (&cursor);
	end = u32 (&cursor);

	/* AppointmentRecurrencePattern fields following the base pattern
	 * ([MS-OXOCAL] 2.2.1.44.5). WriterVersion2 selects the exception layout. */
	u32 (&cursor); /* ReaderVersion2 */
	version = u32 (&cursor); /* WriterVersion2 */
	offset = u32 (&cursor); /* StartTimeOffset */
	u32 (&cursor); /* EndTimeOffset */

	if (cursor.bad || !period || offset >= 1440 || pattern > 4)
		goto invalid;

	str = g_string_new ("FREQ=");

	if (!pattern) {
		if (period % 1440)
			goto invalid;

		period /= 1440;
		g_string_append (str, "DAILY");
	} else if (pattern == 1) {
		if (!(mask & 127))
			goto invalid;

		g_string_append (str, "WEEKLY");
		byday (str, mask);
	} else {
		gboolean yearly = period % 12 == 0;

		g_string_append (str, yearly ? "YEARLY" : "MONTHLY");

		if (pattern == 3) {
			if (!(mask & 127) || week < 1 || week > 5)
				goto invalid;

			byday (str, mask);
		} else {
			if (pattern != 4 && (monthday < 1 || monthday > 31))
				goto invalid;

			g_string_append_printf (str, ";BYMONTHDAY=%d", pattern == 4 || monthday == 31 ? -1 : (int) monthday);
		}

		if (yearly) {
			GDateTime *d = g_date_time_new_from_unix_utc (CAL_MINUTES (start));

			if (!d)
				goto invalid;

			g_string_append_printf (str, ";BYMONTH=%d", g_date_time_get_month (d));
			g_date_time_unref (d);
			period /= 12;
		}

		if (pattern == 3)
			g_string_append_printf (str, ";BYSETPOS=%d", week == 5 ? -1 : (int) week);
	}

	if (period != 1)
		g_string_append_printf (str, ";INTERVAL=%u", period);

	if (until == 0x2022) {
		if (!count)
			goto invalid;

		g_string_append_printf (str, ";COUNT=%u", count);
	} else if (until == 0x2021) {
		gint64 local = CAL_MINUTES (end);
		char *formatted = cal_format_time (date ? local : cal_utc (zone, local + offset * 60), date, !date);

		if (!formatted)
			goto invalid;

		g_string_append_printf (str, ";UNTIL=%s", formatted);
		g_free (formatted);
	} else if (until != 0x2023 && until != 0xffffffffu) {
		goto invalid;
	}

	if (pattern == 1 && period > 1 && wkst < 7)
		g_string_append_printf (str, ";WKST=%s", days[wkst]);

	recur = g_new0 (CalRecur, 1);
	recur->rule = g_string_free (str, FALSE);
	recur->deleted = g_array_new (FALSE, FALSE, sizeof (gint64));
	recur->exceptions = g_array_new (FALSE, FALSE, sizeof (CalException));
	g_array_set_clear_func (recur->exceptions, clear_exception);
	modified = g_hash_table_new (g_direct_hash, g_direct_equal);
	exceptions (&cursor, recur, version, charset, max, modified, loss, data);

	if (cursor.bad) {
		g_array_set_size (recur->exceptions, 0);
		g_hash_table_remove_all (modified);
		loss (CAL_INVALID, "The recurrence exception data was malformed; the series was retained.", data);
	}

	/* DeletedInstanceDates also includes rescheduled occurrences. Only
	 * outright deletions become EXDATEs ([MS-OXCICAL] 2.1.3.1.1.20.11). */
	for (i = 0; i < ndel; i++) {
		guint32 day = _g_mime_tnef_get_uint32 (deleted + i * 4) / 1440;

		if (!g_hash_table_contains (modified, GUINT_TO_POINTER (day))) {
			gint64 stamp = CAL_MINUTES ((gint64) day * 1440 + offset);
			g_array_append_val (recur->deleted, stamp);
		}
	}

	g_hash_table_unref (modified);

	return recur;

invalid:

	if (str)
		g_string_free (str, TRUE);

	loss (CAL_INVALID, "Invalid or oversized recurrence pattern; exported as a single occurrence.", data);

	return NULL;
}
