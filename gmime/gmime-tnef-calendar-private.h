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

#ifndef __GMIME_TNEF_CALENDAR_PRIVATE_H__
#define __GMIME_TNEF_CALENDAR_PRIVATE_H__

#include "gmime-tnef-calendar.h"
#include "gmime-tnef-private.h"

#define CAL_INVALID GMIME_TNEF_CONVERSION_LOSS_INVALID_CALENDAR_DATA
#define CAL_UNSUPPORTED GMIME_TNEF_CONVERSION_LOSS_UNSUPPORTED_CALENDAR_DATA
#define CAL_LIMIT GMIME_TNEF_CONVERSION_LOSS_CALENDAR_EXCEPTION_LIMIT_EXCEEDED
#define CAL_MAX_BYTES (64U * 1024 * 1024)
#define CAL_NO_TIME G_MININT64
#define CAL_MINUTES(m) ((gint64) (m) * 60 - G_GINT64_CONSTANT (11644473600))

typedef struct {
	GString *out;
	gsize limit;
	guint column;
	gboolean failed;
} CalWriter;

typedef struct {
	guint16 year, month, weekday, day, hour, minute, second, milliseconds;
} CalTransition;

typedef struct {
	char *id;
	int standard, daylight;
	CalTransition std_date, dst_date;
} CalZone;

typedef struct {
	guint32 start, end, original;
	guint16 flags;
	char *subject, *location;
	int delta, reminder, busy, all_day;
} CalException;

typedef struct {
	char *rule;
	GArray *deleted;  /* local timestamps, excluding modified instances */
	GArray *exceptions;
} CalRecur;

G_GNUC_INTERNAL void cal_begin (CalWriter *w, const char *name);
G_GNUC_INTERNAL void cal_param (CalWriter *w, const char *name, const char *value);
G_GNUC_INTERNAL void cal_value (CalWriter *w, const char *value, gboolean text);
G_GNUC_INTERNAL void cal_prop (CalWriter *w, const char *name, const char *value);
G_GNUC_INTERNAL void cal_text (CalWriter *w, const char *name, const char *value);
G_GNUC_INTERNAL void cal_categories (CalWriter *w, GMimeTnefProperty *p);
G_GNUC_INTERNAL char *cal_format_time (gint64 stamp, gboolean date, gboolean utc);
G_GNUC_INTERNAL CalZone *cal_zone_parse (GBytes *bytes, gboolean definition, const char *fallback);
G_GNUC_INTERNAL void cal_zone_free (CalZone *zone);
G_GNUC_INTERNAL gint64 cal_local (CalZone *zone, gint64 utc);
G_GNUC_INTERNAL gint64 cal_utc (CalZone *zone, gint64 local);
G_GNUC_INTERNAL void cal_write_zone (CalWriter *w, CalZone *zone);
G_GNUC_INTERNAL CalRecur *cal_recur_parse (GBytes *blob, const char *charset, CalZone *zone, gboolean date,
	guint max_exceptions, TnefCalendarLoss loss, gpointer data);
G_GNUC_INTERNAL void cal_recur_free (CalRecur *recur);

#endif
