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
#include "gmime-internet-address.h"

#define TAG(id) GMIME_TNEF_PROPERTY_TAG_##id
#define ABSENT G_MININT

/* Numeric LIDs belong to a property set, not to the stream-dependent
 * property identifier assigned by the sender. */
static const char appointment[] = "00062002-0000-0000-c000-000000000046";
static const char common[] = "00062008-0000-0000-c000-000000000046";
static const char meeting[] = "6ed8da90-450b-101b-98da-00aa003f1305";

static const GValue *
get_first (GMimeTnefProperty *prop)
{
	return prop && g_mime_tnef_property_get_count (prop) ? g_mime_tnef_property_get_value (prop, 0) : NULL;
}

static const GValue *
get_named (GMimeTnefPropertySet *props, guint id)
{
	GMimeTnefNameId *name = g_mime_tnef_name_id_new_with_id (id < 0x8000 ? meeting : id < 0x8500 ? appointment : common, id);
	const GValue *value = get_first (g_mime_tnef_property_set_lookup_name (props, name));

	g_mime_tnef_name_id_unref (name);

	return value;
}

#define get_value(props,id) get_first (g_mime_tnef_property_set_lookup ((props), TAG (id)))

static int
get_number (const GValue *value, int fallback)
{
	return value && G_VALUE_HOLDS_INT (value) ? g_value_get_int (value) : fallback;
}

static int
get_boolean (const GValue *value, int fallback)
{
	return value && G_VALUE_HOLDS_BOOLEAN (value) ? g_value_get_boolean (value) : fallback;
}

static const char *
get_string (const GValue *value)
{
	return value && G_VALUE_HOLDS_STRING (value) ? g_value_get_string (value) : NULL;
}

static GBytes *
get_bytes (const GValue *value)
{
	return value && G_VALUE_HOLDS (value, G_TYPE_BYTES) ? g_value_get_boxed (value) : NULL;
}

static gint64
get_stamp (const GValue *value)
{
	GDateTime *date = value && G_VALUE_HOLDS (value, G_TYPE_DATE_TIME) ? g_value_get_boxed (value) : NULL;

	return date ? g_date_time_to_unix (date) : CAL_NO_TIME;
}

static gboolean
class_is (const char *name, const char *prefix)
{
	gsize prefix_length = strlen (prefix);

	return name && !g_ascii_strncasecmp (name, prefix, prefix_length) && (!name[prefix_length] || name[prefix_length] == '.');
}

static const char *
get_method (const char *name, GMimeTnefPropertySet *props, const char **status)
{
	if (class_is (name, "IPM.Appointment")) return "PUBLISH";
	if (class_is (name, "IPM.Schedule.Meeting.Request")) return "REQUEST";
	if (class_is (name, "IPM.Schedule.Meeting.Canceled")) return "CANCEL";
	if (class_is (name, "IPM.Schedule.Meeting.Resp.Pos")) *status = "ACCEPTED";
	if (class_is (name, "IPM.Schedule.Meeting.Resp.Neg")) *status = "DECLINED";
	if (class_is (name, "IPM.Schedule.Meeting.Resp.Tent")) {
		*status = "TENTATIVE";
		/* A tentative response proposing a new time is a counterproposal. */
		if (get_boolean (get_named (props, 0x8257), FALSE)) return "COUNTER";
	}

	return *status ? "REPLY" : NULL;
}

static const char *
subject (GMimeTnefMessage *message, GMimeTnefPropertySet *props)
{
	const char *str = get_string (get_value (props, NORMALIZED_SUBJECT_W));

	return str ? str : g_mime_tnef_message_get_subject (message);
}

static char *
description (GMimeTnefMessage *message, TnefCalendarLoss loss, gpointer data)
{
	GMimeTnefMessageBody *body = g_mime_tnef_message_get_text_body (message);
	char *str = NULL;

	if (body) {
		if (g_mime_tnef_message_body_get_length (body) <= CAL_MAX_BYTES)
			str = g_mime_tnef_message_body_get_text (body, NULL);

		if (!str)
			loss (CAL_INVALID, "The calendar body could not be decoded within the size limit.", data);
		else
			g_strchomp (str);
	}

	return str;
}

static void
numeric_prop (CalWriter *writer, const char *name, int number)
{
	char buf[32];

	if (number != ABSENT) {
		g_snprintf (buf, sizeof buf, "%d", number);
		cal_prop (writer, name, buf);
	}
}

static void
time_prop (CalWriter *writer, const char *name, gint64 timestamp, CalZone *zone, gboolean date, gboolean local)
{
	char *str;

	if (timestamp == CAL_NO_TIME)
		return;

	/* An all-day date is midnight in the organizer's zone. Without the
	 * zone, use the nearest midnight rather than truncating the UTC date. */
	if (!local)
		timestamp = zone ? cal_local (zone, timestamp) : date ? timestamp + 43200 : timestamp;

	str = cal_format_time (timestamp, date, !zone);

	if (!str) {
		writer->failed = TRUE;
		return;
	}

	cal_begin (writer, name);

	if (date)
		cal_param (writer, "VALUE", "DATE");
	else if (zone)
		cal_param (writer, "TZID", zone->id);

	cal_value (writer, str, FALSE);
	g_free (str);
}

static char *
mailbox (const char *type, const char *email, const char *smtp, const GValue *key)
{
	gboolean is_smtp = !type || !*type || !g_ascii_strcasecmp (type, "SMTP");
	const char *candidate = is_smtp && email && *email ? email : smtp;
	GMimeInternetAddress *mailbox_address;
	char *result = NULL, *allocated = NULL;

	if ((!candidate || !*candidate) && is_smtp) {
		GBytes *key_bytes = get_bytes (key);

		if (key_bytes && g_bytes_get_size (key_bytes) <= 8192)
			allocated = g_strndup (g_bytes_get_data (key_bytes, NULL), g_bytes_get_size (key_bytes));

		candidate = allocated ? allocated : get_string (key);
		candidate = candidate && !g_ascii_strncasecmp (candidate, "SMTP:", 5) ? candidate + 5 : NULL;
	}

	if (candidate && *candidate && !strpbrk (candidate, "\r\n") && (mailbox_address = g_mime_mailbox_address_parse (NULL, candidate, -1, NULL))) {
		const char *addr = g_mime_mailbox_address_get_addrspec (GMIME_MAILBOX_ADDRESS (mailbox_address));

		if (addr && strchr (addr, '@')) {
			char *escaped = g_uri_escape_string (addr, "@!$'()*+-._~", FALSE);

			result = g_strconcat ("mailto:", escaped, NULL);
			g_free (escaped);
		}

		g_object_unref (mailbox_address);
	}

	g_free (allocated);

	return result;
}

static void
address (CalWriter *writer, gboolean organizer, char *uri, const char *cn, int role, int rsvp, const char *status)
{
	cal_begin (writer, organizer ? "ORGANIZER" : "ATTENDEE");

	if (role == GMIME_TNEF_RECIPIENT_TYPE_BCC)
		cal_param (writer, "CUTYPE", "RESOURCE");

	if (role >= 0)
		cal_param (writer, "ROLE", role == GMIME_TNEF_RECIPIENT_TYPE_BCC ? "NON-PARTICIPANT" :
			   role == GMIME_TNEF_RECIPIENT_TYPE_CC ? "OPT-PARTICIPANT" : "REQ-PARTICIPANT");

	cal_param (writer, "PARTSTAT", status);

	if (rsvp >= 0)
		cal_param (writer, "RSVP", rsvp ? "TRUE" : "FALSE");

	if (cn && *cn)
		cal_param (writer, "CN", cn);

	cal_value (writer, uri ? uri : "invalid:nomail", FALSE);
	g_free (uri);
}

static void
attendees (CalWriter *writer, GMimeTnefMessage *message, GMimeTnefPropertySet *props, const char *method, const char *reply)
{
	gboolean publish = !strcmp (method, "PUBLISH"), has_organizer = FALSE;
	int rsvp = get_boolean (get_value (props, RESPONSE_REQUESTED), -1);
	const char *cn;
	char *uri;
	guint i;

	if (publish && !(get_number (get_named (props, 0x8217), 0) & 1))
		return;

	for (i = 0; i < g_mime_tnef_message_get_recipient_count (message) && !writer->failed; i++) {
		GMimeTnefRecipient *recipient = g_mime_tnef_message_get_recipient (message, i);
		GMimeTnefPropertySet *recipient_props = g_mime_tnef_recipient_get_properties (recipient);
		int role = g_mime_tnef_recipient_get_recipient_type (recipient);
		int flags = get_number (get_value (recipient_props, RECIPIENT_FLAGS), 0);
		int track = get_number (get_value (recipient_props, RECIPIENT_TRACK_STATUS), 0);
		gboolean organizer = reply || (flags & 2) || role == GMIME_TNEF_RECIPIENT_TYPE_ORIGINATOR;
		const char *status = !publish ? "NEEDS-ACTION" : track == 2 ? "TENTATIVE" : track == 3 ?
			"ACCEPTED" : track == 4 ? "DECLINED" : NULL;

		if ((reply ? role != GMIME_TNEF_RECIPIENT_TYPE_TO : (flags & 32)) || (organizer && has_organizer))
			continue;

		uri = mailbox (g_mime_tnef_recipient_get_address_type (recipient), get_string (get_value (recipient_props, EMAIL_ADDRESS_W)),
			       get_string (get_value (recipient_props, SMTP_ADDRESS_W)), get_value (recipient_props, SEARCH_KEY));
		address (writer, organizer, uri, g_mime_tnef_recipient_get_display_name (recipient), organizer ? -1 : role,
			 organizer ? -1 : rsvp, organizer ? NULL : status);

		if (organizer)
			has_organizer = TRUE;

		if (reply)
			break;
	}

	if (has_organizer && !reply)
		return;

	uri = mailbox (get_string (get_value (props, SENT_REPRESENTING_ADDRTYPE_W)), get_string (get_value (props, SENT_REPRESENTING_EMAIL_ADDRESS_W)),
		       NULL, get_value (props, SENT_REPRESENTING_SEARCH_KEY));
	cn = get_string (get_value (props, SENT_REPRESENTING_NAME_W));

	if (!uri) {
		if (!cn)
			cn = get_string (get_value (props, SENDER_NAME_W));

		uri = mailbox (get_string (get_value (props, SENDER_ADDRTYPE_W)), get_string (get_value (props, SENDER_EMAIL_ADDRESS_W)),
			       get_string (get_value (props, SENDER_SMTP_ADDRESS_W)), get_value (props, SENDER_SEARCH_KEY));
	}

	if (uri || (cn && *cn))
		address (writer, !reply, uri, cn, -1, -1, reply);
}

static char *
create_uid (GMimeTnefPropertySet *props, const char *summary, gint64 start, gint64 end, TnefCalendarLoss loss, gpointer data)
{
	/* A GlobalObjectId with this payload signature wraps an existing
	 * iCalendar UID rather than supplying a new one ([MS-OXOCAL] 2.2.1.27). */
	static const guint8 prefix[12] = { 'v','C','a','l','-','U','i','d',1,0,0,0 };
	GBytes *global_object_id = get_bytes (get_named (props, 0x23));
	const char *sender;
	GChecksum *sum;
	char dates[128];
	guint8 hash[32];
	gsize len = sizeof (hash);

	if (!global_object_id || !g_bytes_get_size (global_object_id))
		global_object_id = get_bytes (get_named (props, 3));

	if (global_object_id && g_bytes_get_size (global_object_id) >= 40 && g_bytes_get_size (global_object_id) <= 1024 * 1024) {
		const guint8 *id = g_bytes_get_data (global_object_id, NULL);
		guint32 payload_length = _g_mime_tnef_get_uint32 (id + 36);
		gsize i, size = g_bytes_get_size (global_object_id);
		static const char hex[] = "0123456789ABCDEF";
		char *uid;

		if (payload_length > sizeof prefix && payload_length <= size - 40 && !memcmp (id + 40, prefix, sizeof prefix) && id[52])
			return g_strndup ((const char *) id + 52, payload_length - 12);

		uid = g_malloc (2 * size + 1);

		for (i = 0; i < size; i++) {
			guint octet = i >= 16 && i < 20 ? 0 : id[i];

			uid[2*i] = hex[octet >> 4];
			uid[2*i+1] = hex[octet & 15];
		}

		uid[2*size] = 0;

		return uid;
	} else if (global_object_id) {
		loss (CAL_INVALID, "An invalid or oversized GlobalObjectId was replaced with a deterministic UID.", data);
	}

	/* Match the reference's seed (decimal .NET ticks, invariant culture),
	 * GUID field order and version/variant bits. */
	sum = g_checksum_new (G_CHECKSUM_SHA256);

	if (summary)
		g_checksum_update (sum, (const guchar *) summary, strlen (summary));

	{
		const GValue *start_value = get_named (props, 0x820d);
		const GValue *end_value = get_named (props, 0x820e);
		const GValue *created = get_value (props, CREATION_TIME);
		const GValue *sources[] = { start_value, end_value ? end_value : start_value, created };
		gint64 times[] = { start, end, get_stamp (created) };
		guint i;

		for (i = 0; i < G_N_ELEMENTS (times); i++) {
			gint64 ticks;

			g_checksum_update (sum, (const guchar *) "\n", 1);

			if (times[i] == CAL_NO_TIME)
				continue;

			/* Calendar lines show whole seconds, but the UID seed includes
			 * the fractional part of each timestamp. */
			ticks = (times[i] + G_GINT64_CONSTANT (62135596800)) * 10000000;

			if (sources[i] && G_VALUE_HOLDS (sources[i], G_TYPE_DATE_TIME) && g_value_get_boxed (sources[i]))
				ticks += 10 * g_date_time_get_microsecond (g_value_get_boxed (sources[i]));

			g_snprintf (dates, sizeof dates, "%" G_GINT64_FORMAT, ticks);
			g_checksum_update (sum, (const guchar *) dates, strlen (dates));
		}
	}

	g_checksum_update (sum, (const guchar *) "\n", 1);
	sender = get_string (get_value (props, SENT_REPRESENTING_EMAIL_ADDRESS_W));

	if (!sender)
		sender = get_string (get_value (props, SENDER_EMAIL_ADDRESS_W));

	if (sender)
		g_checksum_update (sum, (const guchar *) sender, strlen (sender));

	g_checksum_get_digest (sum, hash, &len);
	g_checksum_free (sum);

	hash[7] = (hash[7] & 15) | 0x50;
	hash[8] = (hash[8] & 63) | 0x80;

	return g_strdup_printf ("%02X%02X%02X%02X-%02X%02X-%02X%02X-%02X%02X-%02X%02X%02X%02X%02X%02X",
		hash[3],hash[2],hash[1],hash[0],hash[5],hash[4],hash[7],hash[6],hash[8],hash[9],
		hash[10],hash[11],hash[12],hash[13],hash[14],hash[15]);
}

static const char *const busy_names[] = { "FREE", "TENTATIVE", "BUSY", "OOF", "WORKINGELSEWHERE" };

static void
shared_properties (CalWriter *writer, GMimeTnefMessage *message, GMimeTnefPropertySet *props, const char *method, const char *reply, const char *uid)
{
	static const char *const classes[] = { "PUBLIC", "X-PERSONAL", "PRIVATE", "CONFIDENTIAL" };
	GMimeTnefNameId *key = g_mime_tnef_name_id_new_with_name ("00020329-0000-0000-c000-000000000046", "Keywords");
	int sensitivity, importance, intended_status, disallow_counter;
	gint64 timestamp = get_stamp (get_named (props, reply ? 1 : 0x1a));

	attendees (writer, message, props, method, reply);
	cal_categories (writer, g_mime_tnef_property_set_lookup_name (props, key));
	g_mime_tnef_name_id_unref (key);

	sensitivity = get_number (get_value (props, SENSITIVITY), -1);

	if ((guint) sensitivity < G_N_ELEMENTS (classes))
		cal_prop (writer, "CLASS", classes[sensitivity]);
	else if (sensitivity == -1 && get_boolean (get_named (props, 0x8506), FALSE))
		cal_prop (writer, "CLASS", "PRIVATE");

	if (timestamp == CAL_NO_TIME)
		timestamp = get_stamp (get_value (props, CLIENT_SUBMIT_TIME));

	if (timestamp == CAL_NO_TIME)
		timestamp = get_stamp (get_value (props, LAST_MODIFICATION_TIME));

	if (timestamp == CAL_NO_TIME)
		timestamp = g_get_real_time () / G_USEC_PER_SEC;

	time_prop (writer, "DTSTAMP", timestamp, NULL, FALSE, FALSE);
	time_prop (writer, "CREATED", get_stamp (get_value (props, CREATION_TIME)), NULL, FALSE, FALSE);
	time_prop (writer, "LAST-MODIFIED", get_stamp (get_value (props, LAST_MODIFICATION_TIME)), NULL, FALSE, FALSE);

	importance = get_number (get_value (props, IMPORTANCE), ABSENT);

	if (importance >= 0 && importance <= 2)
		numeric_prop (writer, "PRIORITY", 9 - 4 * importance);

	numeric_prop (writer, "X-MICROSOFT-CDO-IMPORTANCE", importance);

	numeric_prop (writer, "SEQUENCE", get_number (get_named (props, 0x8201), 0));

	if (!strcmp (method, "CANCEL"))
		cal_prop (writer, "STATUS", "CANCELLED");

	cal_text (writer, "UID", uid);

	intended_status = get_number (get_named (props, 0x8224), -1);

	if (!strcmp (method, "REQUEST") && (guint) intended_status < G_N_ELEMENTS (busy_names))
		cal_prop (writer, "X-MICROSOFT-CDO-INTENDEDSTATUS", busy_names[intended_status]);

	numeric_prop (writer, "X-MICROSOFT-CDO-OWNERAPPTID", get_number (get_value (props, OWNER_APPT_ID), ABSENT));

	disallow_counter = get_boolean (get_named (props, 0x825a), -1);

	if (disallow_counter >= 0)
		cal_prop (writer, "X-MICROSOFT-DISALLOW-COUNTER", disallow_counter ? "TRUE" : "FALSE");
}

typedef struct {
	gint64 start, end, original;
	CalZone *start_zone, *end_zone, *original_zone;
	gboolean date, original_date, local_start, local_end, local_original;
	const char *subject, *location, *body;
	int busy, alarm;
} Event;

static int
delta (int minutes)
{
	/* PidLidReminderDelta's "not set" sentinel means use the default
	 * 15-minute reminder ([MS-OXOCAL] 2.2.1.3). */
	return minutes == 0x5ae980e1 ? 15 : MAX (0, minutes);
}

static void
event (CalWriter *writer, const GString *shared, const Event *component, gboolean reply, CalRecur *recurrence)
{
	guint i;

	cal_prop (writer, "BEGIN", "VEVENT");

	if (writer->failed)
		return;

	if (shared->len > writer->limit - writer->out->len) {
		writer->failed = TRUE;
		return;
	}

	g_string_append_len (writer->out, shared->str, shared->len);
	time_prop (writer, "DTSTART", component->start, component->start_zone, component->date, component->local_start);
	time_prop (writer, "DTEND", component->end, component->end_zone, component->date, component->local_end);
	time_prop (writer, "RECURRENCE-ID", component->original, component->original_zone, component->original_date, component->local_original);

	if (recurrence) {
		cal_prop (writer, "RRULE", recurrence->rule);

		for (i = 0; i < recurrence->deleted->len && !writer->failed; i++)
			time_prop (writer, "EXDATE", g_array_index (recurrence->deleted, gint64, i), component->start_zone, component->date, TRUE);
	}

	cal_text (writer, "SUMMARY", component->subject ? component->subject : "");

	if (component->location && *component->location)
		cal_text (writer, "LOCATION", component->location);

	if (component->body && *component->body)
		cal_text (writer, reply ? "COMMENT" : "DESCRIPTION", component->body);

	if (component->busy != ABSENT)
		cal_prop (writer, "TRANSP", component->busy == 0 ? "TRANSPARENT" : "OPAQUE");

	if ((guint) component->busy < G_N_ELEMENTS (busy_names))
		cal_prop (writer, "X-MICROSOFT-CDO-BUSYSTATUS", busy_names[component->busy]);

	if (component->alarm >= 0) {
		char trigger[32];

		g_snprintf (trigger, sizeof trigger, "%sPT%dM", component->alarm ? "-" : "", component->alarm);
		cal_prop (writer, "BEGIN", "VALARM");
		cal_prop (writer, "ACTION", "DISPLAY");
		cal_prop (writer, "DESCRIPTION", "Reminder");
		cal_prop (writer, "TRIGGER", trigger);
		cal_prop (writer, "END", "VALARM");
	}

	cal_prop (writer, "END", "VEVENT");
}

static CalZone *
recurrence_zone (GMimeTnefPropertySet *props)
{
	const char *desc = get_string (get_named (props, 0x8234));
	CalZone *zone = cal_zone_parse (get_bytes (get_named (props, 0x8260)), TRUE, desc);

	/* Prefer the recurrence-specific TZ definition; older senders supply
	 * PidLidTimeZoneStruct instead ([MS-OXCICAL] 2.1.3.1.1.19). */
	return zone ? zone : cal_zone_parse (get_bytes (get_named (props, 0x8233)), FALSE, desc);
}

/* Register by both rules and name. Distinct rules that have the same
 * display name must not alias each other in the resulting VCALENDAR. */
static CalZone *
register_zone (GPtrArray *zones, CalZone *zone)
{
	guint i, suffix = 1;
	char *base;

	if (!zone)
		return NULL;

	base = g_strdup (zone->id);

	for (i = 0; i < zones->len; i++) {
		CalZone *other = g_ptr_array_index (zones, i);

		if (strcmp (other->id, zone->id))
			continue;

		if (zone->standard == other->standard && zone->daylight == other->daylight &&
		    !memcmp (&zone->std_date, &other->std_date, sizeof zone->std_date) &&
		    !memcmp (&zone->dst_date, &other->dst_date, sizeof zone->dst_date)) {
			cal_zone_free (zone);
			g_free (base);
			return other;
		}

		g_free (zone->id);
		zone->id = g_strdup_printf ("%s (%u)", base, ++suffix);
		i = (guint) -1;
	}

	g_free (base);
	g_ptr_array_add (zones, zone);

	return zone;
}

static void
instance_id (Event *component, GMimeTnefPropertySet *props, GPtrArray *zones)
{
	guint year, hour, minute, second, time;
	GDateTime *original_date;
	GBytes *global_object_id;
	const guint8 *id;

	component->original_date = component->date;
	component->original = get_stamp (get_named (props, 0x8228));

	if (component->original == CAL_NO_TIME && !get_boolean (get_named (props, 10), FALSE))
		return;

	component->original_zone = register_zone (zones, recurrence_zone (props));

	if (!component->original_zone)
		component->original_zone = component->start_zone;

	if (component->original != CAL_NO_TIME)
		return;

	global_object_id = get_bytes (get_named (props, 3));

	if (!global_object_id || g_bytes_get_size (global_object_id) < 20)
		return;

	id = g_bytes_get_data (global_object_id, NULL);
	year = id[16] * 256u + id[17];

	if (year < 1601 || year > 9999 || !g_date_valid_dmy (id[19], id[18], year))
		return;

	time = get_number (get_named (props, 14), 0);
	hour = (time >> 12) & 31;
	minute = (time >> 6) & 63;
	second = time & 63;

	if (hour > 23 || minute > 59 || second > 59)
		hour = minute = second = 0;

	original_date = g_date_time_new_utc (year, id[18], id[19], hour, minute, second);
	component->original = g_date_time_to_unix (original_date);
	component->local_original = TRUE;
	g_date_time_unref (original_date);
}

typedef struct {
	GMimeTnefAttachment *attachment;
	GMimeTnefMessage *message;
	gboolean exact;
} Match;

static void
index_time (GHashTable *table, gint64 timestamp, Match *match)
{
	gint64 *key;

	if (g_hash_table_contains (table, &timestamp))
		return;

	key = g_new (gint64, 1);
	*key = timestamp;

	g_hash_table_insert (table, key, match);
}

static Match *
match_time (GHashTable *table, gint64 timestamp)
{
	gint64 earlier = timestamp - ((timestamp % 60 + 60) % 60), later = earlier + 60;
	Match *match = g_hash_table_lookup (table, &earlier);

	return match || timestamp == earlier ? match : g_hash_table_lookup (table, &later);
}

/* Index the requested instances first. Loading an exception attachment
 * must not recurse through its own calendar or scale as instances * files. */
static Match *
find_attachments (GMimeTnefMessage *message, CalRecur *recurrence, CalZone *zone, TnefCalendarLoss loss, gpointer data)
{
	GHashTable *original = g_hash_table_new_full (g_int64_hash, g_int64_equal, g_free, NULL);
	GHashTable *local = g_hash_table_new_full (g_int64_hash, g_int64_equal, g_free, NULL);
	Match *matches = g_new0 (Match, recurrence->exceptions->len);
	guint64 remaining = CAL_MAX_BYTES;
	guint i;

	for (i = 0; i < recurrence->exceptions->len; i++) {
		CalException *ex = &g_array_index (recurrence->exceptions, CalException, i);

		index_time (original, cal_utc (zone, CAL_MINUTES (ex->original)), &matches[i]);
		index_time (local, CAL_MINUTES (ex->start), &matches[i]);
	}

	for (i = 0; i < g_mime_tnef_message_get_attachment_count (message); i++) {
		GMimeTnefAttachment *attachment = g_mime_tnef_message_get_attachment (message, i);
		GMimeTnefPropertySet *attachment_props = g_mime_tnef_attachment_get_properties (attachment);
		GMimeTnefMessage *embedded;
		const GValue *original_time;
		gint64 time, length;
		Match *match;

		if (!(get_number (get_value (attachment_props, ATTACHMENT_FLAGS), 0) & 2) || !g_mime_tnef_attachment_is_embedded_message (attachment))
			continue;

		length = g_mime_tnef_attachment_get_length (attachment);
	
		if (length < 0 || (guint64) length > remaining) {
			loss (CAL_LIMIT, "Exception attachment processing exceeded the input budget; unmatched files were retained.", data);
			break;
		}
	
		remaining -= length;

		embedded = g_mime_tnef_attachment_load_embedded_message (attachment, NULL);
	
		if (!embedded) {
			loss (CAL_INVALID, "A calendar exception attachment could not be decoded and was retained.", data);
			continue;
		}

		/* PidLidExceptionReplaceTime identifies the original start in UTC;
		 * the attachment's fallback identifies the modified start in local time. */
		original_time = get_named (g_mime_tnef_message_get_properties (embedded), 0x8228);
		time = get_stamp (original_time ? original_time : get_value (attachment_props, EXCEPTION_START_TIME));
		match = time == CAL_NO_TIME ? NULL : match_time (original_time ? original : local, time);
	
		if (!match || (match->message && (match->exact || !original_time))) {
			g_object_unref (embedded);
			continue;
		}

		g_clear_object (&match->message);
		match->message = embedded;
		match->attachment = attachment;
		match->exact = original_time != NULL;
	}

	g_hash_table_unref (original);
	g_hash_table_unref (local);

	return matches;
}

static gboolean
overlay (Event *component, GMimeTnefMessage *message, char **body, TnefCalendarLoss loss, gpointer data)
{
	GMimeTnefPropertySet *props = g_mime_tnef_message_get_properties (message);
	const char *text = subject (message, props);
	int busy_status, reminder_enabled, all_day;
	gboolean used = FALSE;
	gint64 timestamp;
	
	if (text) {
		component->subject = text;
		used = TRUE;
	}

	text = get_string (get_named (props, 0x8208));

	if (text) {
		component->location = text;
		used = TRUE;
	}

	*body = description (message, loss, data);

	if (*body && **body) {
		component->body = *body;
		used = TRUE;
	}

	busy_status = get_number (get_named (props, 0x8205), ABSENT);

	if (busy_status != ABSENT) {
		component->busy = busy_status;
		used = TRUE;
	}

	reminder_enabled = get_boolean (get_named (props, 0x8503), -1);

	if (reminder_enabled >= 0) {
		component->alarm = reminder_enabled ? delta (get_number (get_named (props, 0x8501), 15)) : -1;
		used = TRUE;
	}

	all_day = get_boolean (get_named (props, 0x8215), component->date);
	timestamp = get_stamp (get_named (props, 0x820d));

	if (timestamp != CAL_NO_TIME) {
		component->start = timestamp;
		component->local_start = FALSE;
		component->date = all_day;
		used = TRUE;
	}

	timestamp = get_stamp (get_named (props, 0x820e));

	if (timestamp != CAL_NO_TIME) {
		component->end = timestamp;
		component->local_end = FALSE;
		component->date = all_day;
		used = TRUE;
	}

	/* A detached event cannot carry nested TNEF payloads. Leave the file
	 * in the MIME tree when exporting it would drop any such content. */
	return used && !g_mime_tnef_message_get_attachment_count (message) &&
		(*body || (!g_mime_tnef_message_get_html_body (message) && !g_mime_tnef_message_get_rtf_body (message))) &&
		(!g_mime_tnef_message_get_text_body (message) || *body);
}

static void
write_exceptions (CalWriter *writer, const GString *shared, GMimeTnefMessage *message, const Event *master, CalRecur *recurrence,
		  gboolean reply, guint64 budget, GPtrArray *used, TnefCalendarLoss loss, gpointer data)
{
	gsize start = writer->out->len, limit = writer->limit;
	Match *matches;
	guint i;

	if (!recurrence->exceptions->len || writer->failed)
		return;

	/* Reserve the VCALENDAR closing line even when the caller's budget
	 * exceeds the hard limit. Roll back the *entire* last event on failure. */
	if (limit - start < 15 || !budget) {
		loss (CAL_LIMIT, "No modified occurrences fit the calendar size budget.", data);
		return;
	}

	writer->limit = start + (gsize) MIN (budget, limit - start - 15);
	matches = find_attachments (message, recurrence, master->start_zone, loss, data);

	for (i = 0; i < recurrence->exceptions->len; i++) {
		CalException *ex = &g_array_index (recurrence->exceptions, CalException, i);
		gsize previous = writer->out->len;
		Event occurrence = *master;
		gboolean consumed = FALSE;
		char *body = NULL;

		occurrence.start = CAL_MINUTES (ex->start);
		occurrence.end = CAL_MINUTES (ex->end);
		occurrence.original = CAL_MINUTES (ex->original);
		occurrence.original_date = master->date;
		occurrence.original_zone = master->start_zone;
		occurrence.local_start = occurrence.local_end = occurrence.local_original = TRUE;

		if (ex->flags & 1)
			occurrence.subject = ex->subject;
		if (ex->flags & 16)
			occurrence.location = ex->location;
		if (ex->flags & 32)
			occurrence.busy = ex->busy;
		if (ex->flags & 128)
			occurrence.date = ex->all_day;
		if (ex->flags & 8)
			occurrence.alarm = ex->reminder ? (occurrence.alarm < 0 ? 15 : occurrence.alarm) : -1;
		if (occurrence.alarm >= 0 && (ex->flags & 4))
			occurrence.alarm = delta (ex->delta);

		if (matches[i].message)
			consumed = overlay (&occurrence, matches[i].message, &body, loss, data);

		event (writer, shared, &occurrence, reply, NULL);
		g_free (body);

		if (writer->failed) {
			writer->failed = FALSE;
			writer->column = 0;
			g_string_truncate (writer->out, previous);
			loss (CAL_LIMIT, "Modified occurrences exceeding the calendar exception size budget were omitted.", data);
			break;
		}

		if (consumed)
			g_ptr_array_add (used, matches[i].attachment);
	}

	for (i = 0; i < recurrence->exceptions->len; i++)
		g_clear_object (&matches[i].message);

	g_free (matches);
	writer->limit = limit;
}

GBytes *
_g_mime_tnef_calendar_export (GMimeTnefMessage *message, const char *fallback_charset, guint max_exceptions,
			      guint64 max_exceptions_size, const char **method, GHashTable *consumed,
			      TnefCalendarLoss loss, gpointer data)
{
	GMimeTnefPropertySet *props = g_mime_tnef_message_get_properties (message);
	const char *reply = NULL, *calendar_method = get_method (g_mime_tnef_message_get_message_class (message), props, &reply);
	CalWriter writer = { NULL, CAL_MAX_BYTES, 0, FALSE }, shared = { NULL, CAL_MAX_BYTES, 0, FALSE };
	GBytes *blob, *result = NULL;
	CalRecur *recurrence = NULL;
	GPtrArray *zones, *used;
	Event master = { 0 };
	char *uid, *body;
	guint i;

	if (method)
		*method = NULL;

	if (!calendar_method)
		return NULL;

	master.start = get_stamp (get_named (props, 0x820d));

	if (master.start == CAL_NO_TIME) {
		loss (CAL_INVALID, "The calendar item lacks a start time; no text/calendar part was generated.", data);
		return NULL;
	}

	master.end = get_stamp (get_named (props, 0x820e));

	if (master.end == CAL_NO_TIME) {
		int minutes = get_number (get_named (props, 0x8213), -1);

		if (minutes >= 0) master.end = master.start + (gint64) minutes * 60;
	}

	master.date = get_boolean (get_named (props, 0x8215), FALSE);
	master.original = CAL_NO_TIME;
	master.subject = subject (message, props);
	master.location = get_string (get_named (props, 0x8208));
	master.busy = get_number (get_named (props, 0x8205), ABSENT);
	master.alarm = get_boolean (get_named (props, 0x8503), FALSE) ? delta (get_number (get_named (props, 0x8501), 15)) : -1;
	master.body = body = description (message, loss, data);
	uid = create_uid (props, master.subject, master.start, master.end, loss, data);

	zones = g_ptr_array_new_with_free_func ((GDestroyNotify) cal_zone_free);
	used = g_ptr_array_new ();
	shared.out = g_string_new (NULL);
	writer.out = g_string_new (NULL);
	shared_properties (&shared, message, props, calendar_method, reply, uid);

	blob = get_bytes (get_named (props, 0x8216));

	if (blob && get_boolean (get_named (props, 0x8223), TRUE)) {
		CalZone *zone = recurrence_zone (props);

		recurrence = cal_recur_parse (blob, fallback_charset ? fallback_charset : "windows-1252", zone, master.date,
				     max_exceptions, loss, data);
		if (recurrence) {
			if (!zone) {
				loss (CAL_INVALID, "The recurring appointment lacks a valid time zone; UTC was assumed.", data);
				zone = g_new0 (CalZone, 1);
				zone->id = g_strdup ("UTC");
			}

			master.start_zone = master.end_zone = register_zone (zones, zone);
		} else {
			cal_zone_free (zone);
		}
	}

	if (!recurrence) {
		GBytes *start_zone = get_bytes (get_named (props, 0x825e)), *end_zone = get_bytes (get_named (props, 0x825f));

		master.start_zone = register_zone (zones, cal_zone_parse (start_zone, TRUE, NULL));
		master.end_zone = register_zone (zones, cal_zone_parse (end_zone, TRUE, NULL));

		if ((start_zone && !master.start_zone) || (end_zone && !master.end_zone))
			loss (CAL_INVALID, "An invalid display time zone was ignored.", data);

		if (!master.end_zone)
			master.end_zone = master.start_zone;

		if (!strcmp (calendar_method, "COUNTER")) {
			gint64 proposed = get_stamp (get_named (props, 0x8250)), end = get_stamp (get_named (props, 0x8251));

			if (proposed != CAL_NO_TIME) {
				time_prop (&shared, "X-MS-OLK-ORIGINALSTART", master.start, master.start_zone, master.date, FALSE);
				time_prop (&shared, "X-MS-OLK-ORIGINALEND", master.end, master.end_zone, master.date, FALSE);
				master.start = proposed;

				if (end != CAL_NO_TIME)
					master.end = end;
			}
		}

		instance_id (&master, props, zones);
	}

	cal_prop (&writer, "BEGIN", "VCALENDAR");
	cal_prop (&writer, "METHOD", calendar_method);
	cal_prop (&writer, "PRODID", "-//GMime//EN");
	cal_prop (&writer, "VERSION", "2.0");
	cal_prop (&writer, "X-MS-OLK-FORCEINSPECTOROPEN", "TRUE");

	/* An all-day series can have timed exception overrides. */
	if (!master.date || (recurrence && recurrence->exceptions->len)) {
		for (i = 0; i < zones->len; i++)
			cal_write_zone (&writer, g_ptr_array_index (zones, i));
	}

	event (&writer, shared.out, &master, reply != NULL, recurrence);

	if (recurrence && !shared.failed)
		write_exceptions (&writer, shared.out, message, &master, recurrence, reply != NULL, max_exceptions_size, used, loss, data);

	cal_prop (&writer, "END", "VCALENDAR");

	if (writer.failed || shared.failed) {
		loss (CAL_INVALID, "The calendar could not be represented within the output or date range limits.", data);
		g_string_free (writer.out, TRUE);
	} else {
		gsize len = writer.out->len;

		result = g_bytes_new_take (g_string_free (writer.out, FALSE), len);

		if (method)
			*method = calendar_method;

		for (i = 0; consumed && i < used->len; i++)
			g_hash_table_add (consumed, g_ptr_array_index (used, i));
	}

	cal_recur_free (recurrence);
	g_ptr_array_unref (used);
	g_ptr_array_unref (zones);
	g_string_free (shared.out, TRUE);
	g_free (body);
	g_free (uid);

	return result;
}
