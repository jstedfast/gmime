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

#include <config.h>
#include <string.h>
#include <gmime/gmime.h>
#include <gmime/gmime-tnef-message.h>
#include <gmime/gmime-tnef-part.h>
#include <gmime/gmime-tnef-conversion.h>
#include <gmime/gmime-tnef-writer.h>

static void
u32 (GByteArray *a, guint32 n)
{
	n = GUINT32_TO_LE (n);
	g_byte_array_append (a, (guint8 *) &n, 4);
}

static void
u16 (GByteArray *a, guint16 n)
{
	n = GUINT16_TO_LE (n);
	g_byte_array_append (a, (guint8 *) &n, 2);
}

/* Thursday series at 09:00 local, one deleted and one moved instance. */
static GByteArray *
weekly (void)
{
	GByteArray *a = g_byte_array_new ();
	guint32 epoch = 194074560; /* 1970-01-01 in minutes since 1601 */
	u16 (a, 0x3004); u16 (a, 0x3004); u16 (a, 0x200b); u16 (a, 1); u16 (a, 0);
	u32 (a, 0); u32 (a, 1); u32 (a, 0); u32 (a, 1 << 4);
	u32 (a, 0x2022); u32 (a, 5); u32 (a, 1);
	u32 (a, 2); u32 (a, epoch + 7 * 1440); u32 (a, epoch + 14 * 1440);
	u32 (a, 1); u32 (a, epoch + 14 * 1440);
	u32 (a, epoch); u32 (a, epoch + 28 * 1440);
	u32 (a, 0x3006); u32 (a, 0x3009); u32 (a, 540); u32 (a, 600);
	u16 (a, 1); u32 (a, epoch + 14 * 1440 + 600); u32 (a, epoch + 14 * 1440 + 660);
	u32 (a, epoch + 14 * 1440 + 540); u16 (a, 0);
	u32 (a, 0); u32 (a, 0); u32 (a, 0);
	return a;
}

static void
named (GMimeTnefPropertyWriter *pw, int id, int type)
{
	const char *set = id < 0x8000 ? "6ed8da90-450b-101b-98da-00aa003f1305" :
		id < 0x8500 ? "00062002-0000-0000-c000-000000000046" : "00062008-0000-0000-c000-000000000046";
	GMimeTnefNameId *name = g_mime_tnef_name_id_new_with_id (set, id);
	g_assert_true (g_mime_tnef_property_writer_write_named_property (pw, name, type, NULL));
	g_mime_tnef_name_id_unref (name);
}

static GMimeTnefMessage *
appointment (GByteArray *pattern)
{
	GMimeStream *s = g_mime_stream_mem_new ();
	GMimeTnefWriter *w = g_mime_tnef_writer_new (s, 1252, 0, NULL);
	GMimeTnefPropertyWriter *pw = g_mime_tnef_writer_open_property_writer (w, GMIME_TNEF_ATTRIBUTE_TAG_MAPI_PROPERTIES, NULL);
	GMimeTnefMessage *m;
	GDateTime *d = g_date_time_new_utc (1970, 1, 1, 14, 0, 0);
	GString *text = g_string_new ("Location;1,2\\3\r\nX-INJECT:");
	guint i;
	for (i = 0; i < 80; i++) g_string_append (text, "\xe2\x98\x83");
	g_assert_true (g_mime_tnef_property_writer_write_property_tag (pw, GMIME_TNEF_PROPERTY_TAG_MESSAGE_CLASS_W, NULL));
	g_assert_true (g_mime_tnef_property_writer_write_value_string (pw, "IPM.Schedule.Meeting.Request", NULL));
	named (pw, 0x820d, GMIME_TNEF_PROPERTY_TYPE_SYS_TIME);
	g_assert_true (g_mime_tnef_property_writer_write_value_date_time (pw, d, NULL));
	named (pw, 0x8208, GMIME_TNEF_PROPERTY_TYPE_UNICODE);
	g_assert_true (g_mime_tnef_property_writer_write_value_string (pw, text->str, NULL));
	if (pattern) {
		guint8 zone[48] = { 0x2c, 1 }; /* UTC-05:00 */
		named (pw, 0x8216, GMIME_TNEF_PROPERTY_TYPE_BINARY);
		g_assert_true (g_mime_tnef_property_writer_write_value_bytes (pw, pattern->data, pattern->len, NULL));
		named (pw, 0x8233, GMIME_TNEF_PROPERTY_TYPE_BINARY);
		g_assert_true (g_mime_tnef_property_writer_write_value_bytes (pw, zone, sizeof zone, NULL));
		named (pw, 0x8234, GMIME_TNEF_PROPERTY_TYPE_UNICODE);
		g_assert_true (g_mime_tnef_property_writer_write_value_string (pw, "Test:Zone", NULL));
	}
	g_assert_true (g_mime_tnef_property_writer_close (pw, NULL));
	g_object_unref (pw);
	if (pattern) {
		/* Both are flagged exceptions. Only the one whose original start
		 * matches an actually exported occurrence can be suppressed. */
		for (i = 0; i < 2; i++) {
			static const guint8 render[14] = { 1, 0, 255, 255, 255, 255 };
			GMimeTnefWriter *embedded;
			GMimeTnefPropertyWriter *ep;
			GDateTime *original = g_date_time_new_utc (1970, 1, 15 + i, 14, 0, 0);
			g_assert_true (g_mime_tnef_writer_write_attribute_bytes (w, GMIME_TNEF_ATTRIBUTE_TAG_ATTACH_RENDER_DATA,
										render, sizeof render, NULL));
			pw = g_mime_tnef_writer_open_property_writer (w, GMIME_TNEF_ATTRIBUTE_TAG_ATTACHMENT, NULL);
			g_assert_true (g_mime_tnef_property_writer_write_property_tag (pw, GMIME_TNEF_PROPERTY_TAG_ATTACHMENT_FLAGS, NULL));
			g_assert_true (g_mime_tnef_property_writer_write_value_int32 (pw, 2, NULL));
			g_assert_true (g_mime_tnef_property_writer_write_property_tag (pw, GMIME_TNEF_PROPERTY_TAG_ATTACH_METHOD, NULL));
			g_assert_true (g_mime_tnef_property_writer_write_value_int32 (pw, 5, NULL));
			g_assert_true (g_mime_tnef_property_writer_write_property_tag (pw, GMIME_TNEF_PROPERTY_TAG_ATTACH_DATA_OBJ, NULL));
			embedded = g_mime_tnef_property_writer_open_embedded_message (pw, 1252, NULL);
			ep = g_mime_tnef_writer_open_property_writer (embedded, GMIME_TNEF_ATTRIBUTE_TAG_MAPI_PROPERTIES, NULL);
			named (ep, 0x8228, GMIME_TNEF_PROPERTY_TYPE_SYS_TIME);
			g_assert_true (g_mime_tnef_property_writer_write_value_date_time (ep, original, NULL));
			g_assert_true (g_mime_tnef_property_writer_write_property_tag (ep, GMIME_TNEF_PROPERTY_TAG_SUBJECT_W, NULL));
			g_assert_true (g_mime_tnef_property_writer_write_value_string (ep, i ? "Unmatched" : "Rescheduled", NULL));
			if (!i) {
				GDateTime *end = g_date_time_new_utc (1970, 1, 15, 17, 30, 0);
				GDateTime *start = g_date_time_new_utc (1970, 1, 15, 16, 0, 0);
				named (ep, 0x820d, GMIME_TNEF_PROPERTY_TYPE_SYS_TIME);
				g_assert_true (g_mime_tnef_property_writer_write_value_date_time (ep, start, NULL));
				named (ep, 0x820e, GMIME_TNEF_PROPERTY_TYPE_SYS_TIME);
				g_assert_true (g_mime_tnef_property_writer_write_value_date_time (ep, end, NULL));
				g_date_time_unref (start);
				g_date_time_unref (end);
				g_assert_true (g_mime_tnef_property_writer_write_property_tag (ep, GMIME_TNEF_PROPERTY_TAG_BODY_W, NULL));
				g_assert_true (g_mime_tnef_property_writer_write_value_string (ep, "Exception body", NULL));
				named (ep, 0x8208, GMIME_TNEF_PROPERTY_TYPE_UNICODE);
				g_assert_true (g_mime_tnef_property_writer_write_value_string (ep, "Room 9", NULL));
				named (ep, 0x8205, GMIME_TNEF_PROPERTY_TYPE_LONG);
				g_assert_true (g_mime_tnef_property_writer_write_value_int32 (ep, 3, NULL));
				named (ep, 0x8503, GMIME_TNEF_PROPERTY_TYPE_BOOLEAN);
				g_assert_true (g_mime_tnef_property_writer_write_value_boolean (ep, TRUE, NULL));
				named (ep, 0x8501, GMIME_TNEF_PROPERTY_TYPE_LONG);
				g_assert_true (g_mime_tnef_property_writer_write_value_int32 (ep, 20, NULL));
			}
			g_assert_true (g_mime_tnef_property_writer_close (ep, NULL));
			g_assert_true (g_mime_tnef_writer_close (embedded, NULL));
			g_assert_true (g_mime_tnef_property_writer_close (pw, NULL));
			g_object_unref (ep);
			g_object_unref (embedded);
			g_object_unref (pw);
			g_date_time_unref (original);
		}
	}
	g_assert_true (g_mime_tnef_writer_close (w, NULL));
	g_object_unref (w);
	g_string_free (text, TRUE);
	g_date_time_unref (d);
	g_assert_cmpint (g_mime_stream_reset (s), ==, 0);
	m = g_mime_tnef_message_load (s, NULL, NULL);
	g_assert_nonnull (m);
	g_object_unref (s);
	return m;
}

static GMimePart *
find_calendar (GMimeEntity *entity, guint *retained)
{
	int i;
	if (GMIME_IS_MULTIPART (entity)) {
		GMimeMultipart *multi = GMIME_MULTIPART (entity);
		GMimePart *calendar = NULL;
		for (i = 0; i < g_mime_multipart_get_count (multi); i++) {
			GMimePart *found = find_calendar (g_mime_multipart_get_part (multi, i), retained);
			if (found) calendar = found;
		}
		return calendar;
	} else if (GMIME_IS_TNEF_PART (entity)) {
		(*retained)++;
	} else if (GMIME_IS_PART (entity) && g_mime_content_type_is_type (g_mime_entity_get_content_type (entity), "text", "calendar")) {
		return GMIME_PART (entity);
	}
	return NULL;
}

static char *
convert (GByteArray *pattern, guint count, guint64 size, int expected_loss)
{
	GMimeTnefMessage *m = appointment (pattern);
	GMimeTnefConversionOptions *opts = g_mime_tnef_conversion_options_new ();
	GMimeTnefConversionResult *result;
	GMimePart *part;
	GMimeStream *stream = g_mime_stream_mem_new ();
	GByteArray *buf;
	gboolean found = FALSE;
	char *text;
	guint i, retained = 0;
	g_mime_tnef_conversion_options_set_max_calendar_exceptions (opts, count);
	g_mime_tnef_conversion_options_set_max_calendar_exceptions_size (opts, size);
	result = g_mime_tnef_message_convert_to_mime (m, opts, NULL);
	g_assert_nonnull (result);
	part = find_calendar (g_mime_message_get_mime_part (g_mime_tnef_conversion_result_get_message (result)), &retained);
	g_assert_nonnull (part);
	g_assert_cmpuint (retained, ==, pattern ? (expected_loss >= 0 ? 2 : 1) : 0);
	g_assert_cmpstr (g_mime_entity_get_content_type_parameter (GMIME_ENTITY (part), "method"), ==, "REQUEST");
	for (i = 0; i < g_mime_tnef_conversion_result_get_loss_count (result); i++) {
		const GMimeTnefConversionLoss *loss = g_mime_tnef_conversion_result_get_loss (result, i);
		if ((int) loss->kind == expected_loss) found = TRUE;
	}
	if (expected_loss >= 0) g_assert_true (found);
	g_assert_cmpint (g_mime_content_write_to_stream (g_mime_part_get_content (part), stream), >, 0);
	buf = g_mime_stream_mem_get_byte_array (GMIME_STREAM_MEM (stream));
	text = g_strndup ((const char *) buf->data, buf->len);
	g_object_unref (stream);
	g_mime_tnef_conversion_result_unref (result);
	g_mime_tnef_conversion_options_unref (opts);
	g_object_unref (m);
	return text;
}

static void
test_folding (void)
{
	char *text = convert (NULL, 10, 1024 * 1024, -1), **lines = g_strsplit (text, "\r\n", -1);
	guint i;
	g_assert_nonnull (strstr (text, "DTSTART:19700101T140000Z\r\n"));
	g_assert_nonnull (strstr (text, "LOCATION:Location\\;1\\,2\\\\3\\nX-INJECT:"));
	g_assert_null (strstr (text, "\r\nX-INJECT:"));
	g_assert_nonnull (strstr (text, "\r\n "));
	for (i = 0; lines[i]; i++) {
		g_assert_cmpuint (strlen (lines[i]), <=, 75);
		g_assert_true (g_utf8_validate (lines[i], -1, NULL));
	}
	g_strfreev (lines);
	g_free (text);
}

static void
test_recurrence (void)
{
	GByteArray *pattern = weekly ();
	char *text = convert (pattern, 1, 1024 * 1024, -1);
	g_assert_nonnull (strstr (text, "BEGIN:VTIMEZONE\r\nTZID:Test:Zone\r\n"));
	g_assert_nonnull (strstr (text, "DTSTART;TZID=\"Test:Zone\":19700101T090000\r\n"));
	g_assert_nonnull (strstr (text, "RRULE:FREQ=WEEKLY;BYDAY=TH;COUNT=5\r\n"));
	g_assert_nonnull (strstr (text, "EXDATE;TZID=\"Test:Zone\":19700108T090000\r\n"));
	g_assert_null (strstr (text, "EXDATE;TZID=\"Test:Zone\":19700115"));
	g_assert_nonnull (strstr (text, "RECURRENCE-ID;TZID=\"Test:Zone\":19700115T090000\r\n"));
	g_assert_nonnull (strstr (text, "DTSTART;TZID=\"Test:Zone\":19700115T110000\r\n"));
	g_assert_nonnull (strstr (text, "DTEND;TZID=\"Test:Zone\":19700115T123000\r\n"));
	g_assert_nonnull (strstr (text, "SUMMARY:Rescheduled\r\n"));
	g_assert_nonnull (strstr (text, "DESCRIPTION:Exception body\r\n"));
	g_assert_nonnull (strstr (text, "LOCATION:Room 9\r\n"));
	g_assert_nonnull (strstr (text, "X-MICROSOFT-CDO-BUSYSTATUS:OOF\r\n"));
	g_assert_nonnull (strstr (text, "TRIGGER:-PT20M\r\n"));
	g_assert_null (strstr (text, "SUMMARY:Unmatched"));
	g_free (text);
	text = convert (pattern, 0, 1024 * 1024, GMIME_TNEF_CONVERSION_LOSS_CALENDAR_EXCEPTION_LIMIT_EXCEEDED);
	g_assert_null (strstr (text, "RECURRENCE-ID"));
	g_free (text);
	text = convert (pattern, 1, 30, GMIME_TNEF_CONVERSION_LOSS_CALENDAR_EXCEPTION_LIMIT_EXCEEDED);
	g_assert_null (strstr (text, "RECURRENCE-ID"));
	g_assert_true (g_str_has_suffix (text, "END:VEVENT\r\nEND:VCALENDAR\r\n"));
	g_free (text);
	g_byte_array_set_size (pattern, pattern->len - 1);
	text = convert (pattern, 1, 1024, GMIME_TNEF_CONVERSION_LOSS_INVALID_CALENDAR_DATA);
	g_assert_nonnull (strstr (text, "RRULE:FREQ=WEEKLY"));
	g_assert_null (strstr (text, "RECURRENCE-ID"));
	g_free (text);
	g_byte_array_set_size (pattern, 12);
	text = convert (pattern, 1, 1024, GMIME_TNEF_CONVERSION_LOSS_INVALID_CALENDAR_DATA);
	g_assert_null (strstr (text, "RRULE:"));
	g_free (text);
	g_byte_array_unref (pattern);
}

static void
write_text_property (GMimeTnefPropertyWriter *writer, GMimeTnefPropertyTag tag, const char *text)
{
	g_assert_true (g_mime_tnef_property_writer_write_property_tag (writer, tag, NULL));
	g_assert_true (g_mime_tnef_property_writer_write_value_string (writer, text, NULL));
}

static void
write_appointment_time (GMimeTnefPropertyWriter *writer, int id, int day, int hour, int minute)
{
	GDateTime *time = g_date_time_new_utc (2024, 3, day, hour, minute, 0);

	named (writer, id, GMIME_TNEF_PROPERTY_TYPE_SYS_TIME);
	g_assert_true (g_mime_tnef_property_writer_write_value_date_time (writer, time, NULL));
	g_date_time_unref (time);
}

typedef struct {
	const char *message_class;
	const char *method;
	const char *attendee_status;
} MeetingCase;

static void
test_meeting (gconstpointer user_data)
{
	const MeetingCase *meeting = user_data;
	GMimeStream *stream = g_mime_stream_mem_new ();
	GMimeTnefWriter *writer = g_mime_tnef_writer_new (stream, 1252, 0, NULL);
	GMimeTnefPropertyWriter *properties;
	GMimeTnefMessage *source;
	GMimeTnefConversionResult *result;
	GMimePart *part;
	GMimeStream *output = g_mime_stream_mem_new ();
	GByteArray *bytes;
	char *calendar, *expected;
	guint retained = 0;
	gboolean counter = !strcmp (meeting->method, "COUNTER");

	properties = g_mime_tnef_writer_open_property_writer (writer, GMIME_TNEF_ATTRIBUTE_TAG_RECIPIENT_TABLE, NULL);
	g_assert_nonnull (properties);
	g_assert_true (g_mime_tnef_property_writer_begin_row (properties, NULL));
	g_assert_true (g_mime_tnef_property_writer_write_property_tag (properties, GMIME_TNEF_PROPERTY_TAG_RECIPIENT_TYPE, NULL));
	g_assert_true (g_mime_tnef_property_writer_write_value_int32 (properties, GMIME_TNEF_RECIPIENT_TYPE_TO, NULL));
	write_text_property (properties, GMIME_TNEF_PROPERTY_TAG_DISPLAY_NAME_W, "Alice");
	write_text_property (properties, GMIME_TNEF_PROPERTY_TAG_SMTP_ADDRESS_W, "alice@example.com");
	g_assert_true (g_mime_tnef_property_writer_close (properties, NULL));
	g_object_unref (properties);

	properties = g_mime_tnef_writer_open_property_writer (writer, GMIME_TNEF_ATTRIBUTE_TAG_MAPI_PROPERTIES, NULL);
	write_text_property (properties, GMIME_TNEF_PROPERTY_TAG_MESSAGE_CLASS_W, meeting->message_class);
	write_text_property (properties, GMIME_TNEF_PROPERTY_TAG_BODY_W, "See you there");
	write_text_property (properties, GMIME_TNEF_PROPERTY_TAG_SENT_REPRESENTING_NAME_W, "Bob");
	write_text_property (properties, GMIME_TNEF_PROPERTY_TAG_SENT_REPRESENTING_ADDRTYPE_W, "SMTP");
	write_text_property (properties, GMIME_TNEF_PROPERTY_TAG_SENT_REPRESENTING_EMAIL_ADDRESS_W, "bob@example.com");
	write_appointment_time (properties, 0x820d, 1, 15, 0);
	write_appointment_time (properties, 0x820e, 1, 16, 0);
	if (counter) {
		named (properties, 0x8257, GMIME_TNEF_PROPERTY_TYPE_BOOLEAN);
		g_assert_true (g_mime_tnef_property_writer_write_value_boolean (properties, TRUE, NULL));
		write_appointment_time (properties, 0x8250, 2, 15, 0);
		write_appointment_time (properties, 0x8251, 2, 16, 30);
	}
	g_assert_true (g_mime_tnef_property_writer_close (properties, NULL));
	g_object_unref (properties);

	g_assert_true (g_mime_tnef_writer_close (writer, NULL));
	g_object_unref (writer);

	g_assert_cmpint (g_mime_stream_reset (stream), ==, 0);
	source = g_mime_tnef_message_load (stream, NULL, NULL);
	g_assert_nonnull (source);
	result = g_mime_tnef_message_convert_to_mime (source, NULL, NULL);
	g_assert_nonnull (result);
	part = find_calendar (g_mime_message_get_mime_part (g_mime_tnef_conversion_result_get_message (result)), &retained);
	g_assert_nonnull (part);
	g_assert_cmpstr (g_mime_entity_get_content_type_parameter (GMIME_ENTITY (part), "method"), ==, meeting->method);
	g_assert_cmpint (g_mime_content_write_to_stream (g_mime_part_get_content (part), output), >, 0);
	bytes = g_mime_stream_mem_get_byte_array (GMIME_STREAM_MEM (output));
	calendar = g_strndup ((const char *) bytes->data, bytes->len);
	expected = g_strdup_printf ("\r\nMETHOD:%s\r\n", meeting->method);
	g_assert_nonnull (strstr (calendar, expected));
	g_free (expected);

	if (meeting->attendee_status) {
		expected = g_strdup_printf ("\r\nATTENDEE;PARTSTAT=%s;CN=Bob:mailto:bob@example.com\r\n", meeting->attendee_status);
		g_assert_nonnull (strstr (calendar, expected));
		g_free (expected);
		g_assert_nonnull (strstr (calendar, "\r\nORGANIZER;CN=Alice:mailto:alice@example.com\r\n"));
		g_assert_nonnull (strstr (calendar, "\r\nCOMMENT:See you there\r\n"));
		g_assert_null (strstr (calendar, "\r\nDESCRIPTION:See you there\r\n"));
	} else {
		g_assert_nonnull (strstr (calendar, "\r\nSTATUS:CANCELLED\r\n"));
	}
	if (counter) {
		g_assert_nonnull (strstr (calendar, "\r\nDTSTART:20240302T150000Z\r\n"));
		g_assert_nonnull (strstr (calendar, "\r\nDTEND:20240302T163000Z\r\n"));
		g_assert_nonnull (strstr (calendar, "\r\nX-MS-OLK-ORIGINALSTART:20240301T150000Z\r\n"));
		g_assert_nonnull (strstr (calendar, "\r\nX-MS-OLK-ORIGINALEND:20240301T160000Z\r\n"));
	}
	g_free (calendar);
	g_object_unref (output);
	g_mime_tnef_conversion_result_unref (result);
	g_object_unref (source);
	g_object_unref (stream);
}

#include "tnef-calendar-fixture.h"

typedef struct {
	const char *name;
	guint pattern, period, first, second, wkst, calendar;
	const char *rule;
} RuleCase;

static const RuleCase rules[] = {
	{ "daily", 0, 2880, 0, 0, 0, 0, "FREQ=DAILY;INTERVAL=2;COUNT=10" },
	{ "weekly", 1, 2, 2, 0, 1, 0, "FREQ=WEEKLY;BYDAY=MO;INTERVAL=2;COUNT=10;WKST=MO" },
	{ "weekly-invalid-wkst", 1, 2, 2, 0, 7, 0, "FREQ=WEEKLY;BYDAY=MO;INTERVAL=2;COUNT=10" },
	{ "monthly", 2, 1, 10, 0, 0, 0, "FREQ=MONTHLY;BYMONTHDAY=10;COUNT=10" },
	{ "yearly", 2, 12, 10, 0, 0, 0, "FREQ=YEARLY;BYMONTHDAY=10;BYMONTH=7;COUNT=10" },
	{ "biennial", 2, 24, 10, 0, 0, 0, "FREQ=YEARLY;BYMONTHDAY=10;BYMONTH=7;INTERVAL=2;COUNT=10" },
	{ "day-31", 2, 1, 31, 0, 0, 0, "FREQ=MONTHLY;BYMONTHDAY=-1;COUNT=10" },
	{ "month-end", 4, 1, 0, 0, 0, 0, "FREQ=MONTHLY;BYMONTHDAY=-1;COUNT=10" },
	{ "last-monday", 3, 1, 2, 5, 0, 0, "FREQ=MONTHLY;BYDAY=MO;BYSETPOS=-1;COUNT=10" },
	{ "second-tue-thu", 3, 2, 0x14, 2, 0, 0, "FREQ=MONTHLY;BYDAY=TU,TH;BYSETPOS=2;INTERVAL=2;COUNT=10" },
	{ "first-monday-july", 3, 12, 2, 1, 0, 0, "FREQ=YEARLY;BYDAY=MO;BYMONTH=7;BYSETPOS=1;COUNT=10" },
	{ "hj-month", 10, 1, 10, 0, 0, 1, "FREQ=MONTHLY;BYMONTHDAY=10;COUNT=10" },
	{ "hj-month-nth", 11, 1, 2, 5, 0, 9, "FREQ=MONTHLY;BYDAY=MO;BYSETPOS=-1;COUNT=10" },
	{ "hj-month-end", 12, 1, 0, 0, 0, 12, "FREQ=MONTHLY;BYMONTHDAY=-1;COUNT=10" },
	{ "hijri-default", 10, 1, 10, 0, 0, 0, NULL },
	{ "hijri-week", 1, 1, 2, 0, 0, 6, NULL },
	{ "lunar", 2, 1, 10, 0, 0, 14, NULL },
	{ "daily-zero", 0, 0, 0, 0, 0, 0, NULL },
	{ "daily-fraction", 0, 1000, 0, 0, 0, 0, NULL },
	{ "weekly-no-days", 1, 1, 128, 0, 0, 0, NULL },
	{ "weekly-zero", 1, 0, 2, 0, 0, 0, NULL },
	{ "monthly-zero", 2, 0, 10, 0, 0, 0, NULL },
	{ "monthly-day-0", 2, 1, 0, 0, 0, 0, NULL },
	{ "monthly-day-32", 2, 1, 32, 0, 0, 0, NULL },
	{ "month-nth-no-days", 3, 1, 0, 1, 0, 0, NULL },
	{ "month-nth-week-0", 3, 1, 2, 0, 0, 0, NULL },
	{ "month-nth-week-6", 3, 1, 2, 6, 0, 0, NULL }
};

static void
test_rule (gconstpointer data)
{
	const RuleCase *v = data;
	CalendarFixture f = fixture_new ("IPM.Appointment", TRUE);
	GByteArray *a = recurrence_blob (v->pattern, v->calendar, v->period, v->first, v->second, v->wkst, 0x2022);
	int loss = v->rule ? -1 : (v->calendar || v->pattern == 10 ? UNSUPPORTED : INVALID);
	char *ics;

	add_recurrence (&f, a);
	ics = fixture_finish (&f, loss);
	if (v->rule) {
		char *line = g_strconcat ("RRULE:", v->rule, NULL);
		assert_line (ics, line);
		g_free (line);
	} else {
		g_assert_null (strstr (ics, "\r\nRRULE:"));
		assert_line (ics, "DTSTART:20240701T140000Z");
	}
	g_free (ics);
	g_byte_array_unref (a);
}

static void
test_recurrence_ends (void)
{
	static const char *ends[] = { "RRULE:FREQ=DAILY;UNTIL=20240731T140000Z", "RRULE:FREQ=DAILY;UNTIL=20240731", "RRULE:FREQ=DAILY" };
	for (guint i = 0; i < G_N_ELEMENTS (ends); i++) {
		CalendarFixture f = fixture_new ("IPM.Appointment", TRUE);
		GByteArray *a = recurrence_blob (0, 0, 1440, 0, 0, 0, i == 2 ? 0x2023 : 0x2021);
		char *ics;
		add_recurrence (&f, a);
		fixture_bool (&f, 0x8215, i == 1);
		ics = fixture_finish (&f, -1);
		assert_line (ics, ends[i]);
		g_free (ics);
		g_byte_array_unref (a);
	}
}

static void
test_zone_struct (void)
{
	static const char *descriptions[] = { NULL, " ", "Eastern Time ", NULL };
	static const char *starts[] = {
		"DTSTART;TZID=\"UTC-05:00\":20240701T100000", "DTSTART;TZID=\"UTC-05:00\":20240701T100000",
		"DTSTART;TZID=Eastern Time:20240701T100000", "DTSTART;TZID=UTC:20240701T140000"
	};
	GByteArray *pattern = recurrence_blob (0, 0, 1440, 0, 0, 0, 0x2022);
	GByteArray *zone = eastern_zone (FALSE, 300);
	for (guint i = 0; i < G_N_ELEMENTS (descriptions); i++) {
		CalendarFixture f = fixture_new ("IPM.Appointment", TRUE);
		char *ics;
		if (descriptions[i]) {
			named (f.pw, 0x8234, GMIME_TNEF_PROPERTY_TYPE_UNICODE);
			g_assert_true (g_mime_tnef_property_writer_write_value_string (f.pw, descriptions[i], NULL));
		}
		if (i == 3)
			g_byte_array_set_size (zone, 47);
		fixture_blob (&f, 0x8233, zone);
		fixture_blob (&f, 0x8216, pattern);
		ics = fixture_finish (&f, i == 3 ? INVALID : -1);
		assert_line (ics, starts[i]);
		assert_line (ics, i == 3 ? "TZID:UTC" : i == 2 ? "TZID:Eastern Time" : "TZID:UTC-05:00");
		assert_line (ics, "RRULE:FREQ=DAILY;COUNT=10");
		g_free (ics);
	}
	g_byte_array_unref (pattern);
	g_byte_array_unref (zone);
}

static void
test_display_zones (void)
{
	GByteArray *zone = eastern_zone (TRUE, 300);
	CalendarFixture f = fixture_new ("IPM.Appointment", TRUE);
	char *ics;
	fixture_time (&f, 0x820e, 12, 2, 15);
	fixture_blob (&f, 0x825e, zone);
	fixture_time (&f, 0x8228, 7, 10, 14);
	ics = fixture_finish (&f, -1);
	assert_line (ics, "DTSTART;TZID=Eastern Standard Time:20240701T100000");
	assert_line (ics, "DTEND;TZID=Eastern Standard Time:20241202T100000");
	assert_line (ics, "RECURRENCE-ID;TZID=Eastern Standard Time:20240710T100000");
	assert_line (ics, "BEGIN:STANDARD\r\nDTSTART:16011104T020000\r\nRRULE:FREQ=YEARLY;BYDAY=1SU;BYMONTH=11\r\n"
		     "TZOFFSETFROM:-0400\r\nTZOFFSETTO:-0500\r\nEND:STANDARD");
	assert_line (ics, "BEGIN:DAYLIGHT\r\nDTSTART:16010311T020000\r\nRRULE:FREQ=YEARLY;BYDAY=2SU;BYMONTH=3\r\n"
		     "TZOFFSETFROM:-0500\r\nTZOFFSETTO:-0400\r\nEND:DAYLIGHT");
	g_free (ics);
	/* Bias, standard bias, daylight bias and each SYSTEMTIME field are
	 * independent rule inputs, even when the display name is identical. */
	for (int i = -1; i < 12; i++) {
		guint8 *rule = zone->data + zone->len - 66;
		guint pos = i < 0 ? 0 : i < 3 ? 22 + i * 4 : i < 11 ? 34 + (i - 3) * 2 : 52;
		const char *end = i < 0 ? "DTEND;TZID=Eastern Standard Time:" : "DTEND;TZID=Eastern Standard Time (2):";
		f = fixture_new ("IPM.Appointment", TRUE);
		fixture_time (&f, 0x820e, 7, 1, 16);
		fixture_blob (&f, 0x825e, zone);
		if (i >= 0)
			rule[pos] += i == 0 ? 60 : 1;
		fixture_blob (&f, 0x825f, zone);
		if (i >= 0)
			rule[pos] -= i == 0 ? 60 : 1;
		ics = fixture_finish (&f, -1);
		g_assert_cmpuint (line_count (ics, "BEGIN:VTIMEZONE"), ==, i < 0 ? 1 : 2);
		g_assert_cmpuint (line_count (ics, end), ==, 1);
		if (i == 0)
			assert_line (ics, "DTEND;TZID=Eastern Standard Time (2):20240701T110000");
		g_free (ics);
	}
	/* In fixed-offset zones, unused transition fields do not distinguish rules. */
	f = fixture_new ("IPM.Appointment", TRUE);
	fixture_time (&f, 0x820e, 7, 1, 16);
	zone->data[zone->len - 14] = 0; /* no daylight transition */
	fixture_blob (&f, 0x825e, zone);
	zone->data[zone->len - 30] = 0; /* no standard transition either */
	fixture_blob (&f, 0x825f, zone);
	ics = fixture_finish (&f, -1);
	g_assert_cmpuint (line_count (ics, "BEGIN:VTIMEZONE"), ==, 1);
	assert_line (ics, "DTEND;TZID=Eastern Standard Time:20240701T110000");
	g_assert_null (strstr (ics, "BEGIN:DAYLIGHT"));
	g_free (ics);
	g_byte_array_unref (zone);
}

static char *
read_uid (GByteArray *id, guint day, int loss)
{
	CalendarFixture f = fixture_new ("IPM.Appointment", FALSE);
	char *ics, *start, *uid;
	fixture_time (&f, 0x820d, 7, day, 14);
	if (id)
		fixture_blob (&f, 3, id);
	ics = fixture_finish (&f, loss);
	g_assert_cmpuint (line_count (ics, "UID:"), ==, 1);
	start = strstr (ics, "\r\nUID:") + 6;
	uid = g_strndup (start, strcspn (start, "\r\n"));
	g_assert_cmpstr (uid, !=, "");
	g_free (ics);
	return uid;
}

static void
test_uid (void)
{
	const char hex[] = "040000008200E00074C5B7101A82E0080000000011121314151617180000000000000000";
	GByteArray *id = global_object_id (2024, 7, 10, NULL);
	char *uid = read_uid (id, 1, -1), *other;
	char *expected = g_strconcat (hex, "00000000", NULL);
	g_assert_cmpstr (uid, ==, expected);
	g_free (uid); g_free (expected); g_byte_array_unref (id);
	id = global_object_id (0, 0, 0, "abc-123@example.com");
	uid = read_uid (id, 1, -1);
	g_assert_cmpstr (uid, ==, "abc-123@example.com");
	g_free (uid);
	{
		const guint32 sizes[] = { G_MAXUINT32, G_MAXINT32, 0x80000000u, 65536, 12, 13, 8 };
		for (guint i = 0; i < G_N_ELEMENTS (sizes); i++) {
			set_u32 (id, 36, sizes[i]);
			uid = read_uid (id, 1, -1);
			g_assert_true (sizes[i] == 13 ? !strcmp (uid, "a") : g_str_has_prefix (uid, hex));
			g_free (uid);
		}
	}
	g_byte_array_unref (id);
	id = global_object_id (2024, 7, 10, "");
	uid = read_uid (id, 1, -1);
	g_assert_true (g_str_has_prefix (uid, hex));
	g_free (uid);
	uid = read_uid (NULL, 1, -1);
	g_assert_true (g_uuid_string_is_valid (uid));
	other = read_uid (NULL, 1, -1);
	g_assert_cmpstr (uid, ==, other);
	g_free (other);
	other = read_uid (NULL, 2, -1);
	g_assert_cmpstr (uid, !=, other);
	g_free (other);
	const guint lengths[] = {39, 1, 0};
	for (guint i = 0; i < G_N_ELEMENTS (lengths); i++) {
		g_byte_array_set_size (id, lengths[i]);
		other = read_uid (id, 1, INVALID);
		g_assert_cmpstr (uid, ==, other);
		g_free (other);
	}
	g_free (uid); g_byte_array_unref (id);
}

static void
test_instance_id (void)
{
	const int time = (10 << 12) | (30 << 6) | 15;
	static const guint invalid_dates[][3] = { {0,7,10}, {1600,7,10}, {10000,7,10}, {2024,0,10},
		{2024,13,10}, {2024,7,0}, {2023,2,29} };
	const int times[] = { time, time, -1, time, 24 << 12, 60 << 6, 60 };
	const char *ids[] = { "RECURRENCE-ID;TZID=Eastern Standard Time:20240710T103015", "RECURRENCE-ID:20240710T103015Z",
		"RECURRENCE-ID:20240710T000000Z", "RECURRENCE-ID;VALUE=DATE:20240710" };
	GByteArray *zone = eastern_zone (TRUE, 300);
	for (guint i = 0; i <= G_N_ELEMENTS (times) + G_N_ELEMENTS (invalid_dates); i++) {
		CalendarFixture f = fixture_new ("IPM.Schedule.Meeting.Request", TRUE);
		const guint *date = i >= G_N_ELEMENTS (times) && i < G_N_ELEMENTS (times) + G_N_ELEMENTS (invalid_dates) ?
			invalid_dates[i - G_N_ELEMENTS (times)] : NULL;
		GByteArray *id = date ? global_object_id (date[0], date[1], date[2], NULL) : global_object_id (2024, 7, 10, NULL);
		char *ics;
		if (i == G_N_ELEMENTS (times) + G_N_ELEMENTS (invalid_dates))
			g_byte_array_set_size (id, 19);
		fixture_bool (&f, 10, TRUE);
		fixture_blob (&f, 3, id);
		if (i < G_N_ELEMENTS (times) && times[i] >= 0)
			fixture_int (&f, 14, times[i]);
		if (i == 0 || i == 3 || i >= G_N_ELEMENTS (times))
			fixture_blob (&f, 0x825e, zone);
		if (i == 3)
			fixture_bool (&f, 0x8215, TRUE);
		ics = fixture_finish (&f, id->len < 20 ? INVALID : -1);
		if (i < G_N_ELEMENTS (times))
			assert_line (ics, ids[i < 4 ? i : 2]);
		else
			g_assert_cmpuint (line_count (ics, "RECURRENCE-ID"), ==, 0);
		g_free (ics);
		g_byte_array_unref (id);
	}
	g_byte_array_unref (zone);
}

static void
test_calendar_sweeps (void)
{
	GByteArray *blobs[] = { weekly (), eastern_zone (TRUE, 300), eastern_zone (FALSE, 300) };
	GRand *random = g_rand_new_with_seed (20240701);
	for (guint i = 0; i < G_N_ELEMENTS (blobs); i++) {
		GByteArray *a = blobs[i];
		guint len = a->len;
		for (guint j = 0; j < len * 2 + (i ? 200 : 300); j++) {
			CalendarFixture f = fixture_new ("IPM.Appointment", TRUE);
			GByteArray *pattern, candidate = *a;
			guint8 *copy = NULL;
			char *ics;
			/* Every truncated prefix and every single-byte complement. */
			if (j < len)
				candidate.len = j;
			else {
				copy = g_memdup2 (a->data, len);
				candidate.data = copy;
				if (j < len * 2)
					copy[j - len] ^= 0xff;
				else {
					guint n = g_rand_int_range (random, 1, 5);
					for (guint k = 0; k < n; k++) {
						guint pos = g_rand_int_range (random, 0, len);
						copy[pos] ^= g_rand_int_range (random, 1, 256);
					}
				}
			}
			if (i == 0) {
				add_recurrence (&f, &candidate);
			} else {
				pattern = recurrence_blob (0, 0, 1440, 0, 0, 0, 0x2021);
				fixture_blob (&f, 0x8216, pattern);
				fixture_blob (&f, i == 1 ? 0x8260 : 0x8233, &candidate);
				g_byte_array_unref (pattern);
			}
			ics = fixture_finish (&f, ANY_CALENDAR_LOSS);
			g_assert_true (g_str_has_prefix (ics, "BEGIN:VCALENDAR\r\n"));
			g_assert_cmpuint (line_count (ics, "BEGIN:VEVENT"), ==, line_count (ics, "END:VEVENT"));
			g_assert_cmpuint (line_count (ics, "BEGIN:VTIMEZONE"), ==, line_count (ics, "END:VTIMEZONE"));
			g_free (ics);
			g_free (copy);
		}
		g_byte_array_unref (a);
	}
	g_rand_free (random);
}

static void
test_malformed_fields (void)
{
	/* Byte offsets in weekly(): deleted count, modified count, time offset,
	 * exception count, and extreme date fields. */
	static const struct { guint offset; guint32 value; gboolean series; } cases[] = {
		{38, G_MAXUINT32, FALSE}, {50, G_MAXUINT32, FALSE},
		{74, G_MAXUINT32, FALSE}, {74, G_MAXINT32, FALSE}, {74, 0x80000000u, FALSE},
		{82, 0xffff, TRUE}
	};
	for (guint i = 0; i < G_N_ELEMENTS (cases); i++) {
		CalendarFixture f = fixture_new ("IPM.Appointment", TRUE);
		GByteArray *a = weekly ();
		char *ics;
		set_u32 (a, cases[i].offset, cases[i].value);
		add_recurrence (&f, a);
		ics = fixture_finish (&f, INVALID);
		g_assert_cmpuint (line_count (ics, "BEGIN:VEVENT"), ==, 1);
		if (cases[i].series)
			assert_line (ics, "RRULE:FREQ=WEEKLY;BYDAY=TH;COUNT=5");
		else
			g_assert_cmpuint (line_count (ics, "RRULE:"), ==, 0);
		g_free (ics);
		g_byte_array_unref (a);
	}
}

static GByteArray *
many_exceptions (guint count)
{
	GByteArray *a = recurrence_blob (1, 0, 1, 0x7f, 0, 0, 0x2022);
	g_byte_array_set_size (a, 38); /* up to DeletedInstanceCount */
	set_u32 (a, 30, count + 10);
	for (guint list = 0; list < 2; list++) {
		u32 (a, count);
		for (guint i = 0; i < count; i++)
			u32 (a, JULY_1_2024 + i * 1440);
	}
	u32 (a, JULY_1_2024); u32 (a, JULY_1_2024 + (count + 10) * 1440);
	u32 (a, 0x3006); u32 (a, 0x3009); u32 (a, 600); u32 (a, 660);
	u16 (a, count);
	for (guint i = 0; i < count; i++) {
		guint start = JULY_1_2024 + i * 1440 + 600;
		u32 (a, start + 180); u32 (a, start + 240); u32 (a, start); u16 (a, 0);
	}
	u32 (a, 0);
	for (guint i = 0; i < count; i++) {
		u32 (a, 4); u32 (a, 0); u32 (a, 0);
	}
	u32 (a, 0);
	return a;
}

static void
test_exception_limits (void)
{
	GMimeTnefConversionOptions *opts = g_mime_tnef_conversion_options_new (), *clone;
	g_assert_cmpuint (g_mime_tnef_conversion_options_get_max_calendar_exceptions (opts), ==, 1024);
	g_assert_cmpuint (g_mime_tnef_conversion_options_get_max_calendar_exceptions_size (opts), ==, 16 * 1024 * 1024);
	g_mime_tnef_conversion_options_set_max_calendar_exceptions (opts, 0);
	g_mime_tnef_conversion_options_set_max_calendar_exceptions_size (opts, 0);
	clone = g_mime_tnef_conversion_options_clone (opts);
	g_assert_cmpuint (g_mime_tnef_conversion_options_get_max_calendar_exceptions (clone), ==, 0);
	g_assert_cmpuint (g_mime_tnef_conversion_options_get_max_calendar_exceptions_size (clone), ==, 0);
	g_mime_tnef_conversion_options_unref (clone);
	g_mime_tnef_conversion_options_unref (opts);

	for (guint i = 0; i < 2; i++) {
		CalendarFixture f = fixture_new ("IPM.Appointment", TRUE);
		GByteArray *a = many_exceptions (i ? 200 : 2000);
		guint max = g_mime_tnef_conversion_options_get_max_calendar_exceptions (f.opts);
		guint budget = 256 * 1024, events;
		char *ics;
		if (i) {
			char *body = g_strnfill (16 * 1024, 'x');
			write_text_property (f.pw, GMIME_TNEF_PROPERTY_TAG_BODY_W, body);
			g_mime_tnef_conversion_options_set_max_calendar_exceptions_size (f.opts, budget);
			g_free (body);
		}
		add_recurrence (&f, a);
		ics = fixture_finish (&f, LIMIT);
		events = line_count (ics, "BEGIN:VEVENT");
		g_assert_cmpuint (events, ==, line_count (ics, "END:VEVENT"));
		g_assert_cmpuint (events, ==, line_count (ics, "RECURRENCE-ID") + 1);
		g_assert_cmpuint (line_count (ics, "EXDATE"), ==, 0);
		if (i) {
			g_assert_cmpuint (events, >, 1);
			g_assert_cmpuint (events, <, 201);
			g_assert_cmpuint (f.wire_size, <, budget + 34 * 1024);
		} else {
			g_assert_cmpuint (events, ==, max + 1);
		}
		g_free (ics);
		g_byte_array_unref (a);
	}
}

static void
test_calendar_selection (void)
{
	const char *classes[] = { "IPM.Note", "IPM.AppointmentX", "IPM.Schedule.Meeting.Notification.Forward",
		"IPM.Schedule.Meeting.Request", "IPM.Schedule.Meeting.Request", "IPM.Appointment.Custom" };
	for (guint i = 0; i < G_N_ELEMENTS (classes); i++) {
		CalendarFixture f = fixture_new (classes[i], i != 4);
		char *ics;
		if (i != 5)
			write_text_property (f.pw, GMIME_TNEF_PROPERTY_TAG_BODY_W, "Hello");
		if (i == 3)
			g_mime_tnef_conversion_options_set_generate_calendar (f.opts, FALSE);
		ics = fixture_finish (&f, i == 4 ? INVALID : -1);
		if (i == 5) {
			assert_line (ics, "METHOD:PUBLISH");
			g_assert_true (f.calendar_root);
			g_assert_cmpuint (line_count (ics, "ATTENDEE"), ==, 0);
		} else {
			g_assert_null (ics);
			g_assert_true (f.plain_root);
		}
		g_free (ics);
	}
}

static void
test_event_properties (void)
{
	const int durations[] = { 90, 0, -1 };
	const char *classes[] = { "CLASS:PUBLIC", "CLASS:X-PERSONAL", "CLASS:PRIVATE", "CLASS:CONFIDENTIAL",
		NULL, "CLASS:PRIVATE", NULL, "CLASS:PUBLIC" };
	for (guint i = 0; i < G_N_ELEMENTS (classes); i++) {
		CalendarFixture f = fixture_new ("IPM.Appointment", TRUE);
		char *ics;
		if (i <= 4 || i == 7) {
			g_assert_true (g_mime_tnef_property_writer_write_property_tag (f.pw, GMIME_TNEF_PROPERTY_TAG_SENSITIVITY, NULL));
			g_assert_true (g_mime_tnef_property_writer_write_value_int32 (f.pw, i == 7 ? 0 : (int) i, NULL));
		}
		if (i >= 4)
			fixture_bool (&f, 0x8506, i != 6);
		if (i < G_N_ELEMENTS (durations))
			fixture_int (&f, 0x8213, durations[i]);
		if (i < 4) {
			tag_int (f.pw, GMIME_TNEF_PROPERTY_TAG_IMPORTANCE, i);
		}
		ics = fixture_finish (&f, -1);
		if (classes[i])
			assert_line (ics, classes[i]);
		else
			g_assert_cmpuint (line_count (ics, "CLASS:"), ==, 0);
		if (i < 2)
			assert_line (ics, i == 0 ? "DTEND:20240701T153000Z" : "DTEND:20240701T140000Z");
		else
			g_assert_cmpuint (line_count (ics, "DTEND:"), ==, 0);
		if (i < 3) {
			const char *priority[] = { "PRIORITY:9", "PRIORITY:5", "PRIORITY:1" };
			assert_line (ics, priority[i]);
		} else {
			g_assert_cmpuint (line_count (ics, "PRIORITY:"), ==, 0);
		}
		g_free (ics);
	}
}

static void
test_attendees (void)
{
	static const struct {
		const char *name, *smtp, *type, *email, *key;
		int role, flags, track;
	} rows[] = {
		{ "Alice", "alice@example.com", NULL, NULL, NULL, 1, 3, 0 },
		{ "Bob", "bob@example.com", NULL, NULL, NULL, 1, 1, 0 },
		{ "Carol", "carol@example.com", NULL, NULL, NULL, 2, 0, 2 },
		{ "Room 1", "room1@example.com", NULL, NULL, NULL, 3, 0, 3 },
		{ "Dave", "dave@example.com", NULL, NULL, NULL, 1, 0x21, 0 },
		{ "Eve", "eve@example.com", "EX", "/o=Example/cn=eve", NULL, 1, 0, 4 },
		{ "Frank", NULL, NULL, NULL, "SMTP:FRANK@EXAMPLE.COM", 1, 0, 0 },
		{ "Grace", NULL, "EX", NULL, "EX:/O=EXAMPLE", 1, 0, 0 }
	};
	for (guint publish = 0; publish < 2; publish++) {
		CalendarFixture f = fixture_writer ();
		char *ics;
		f.pw = g_mime_tnef_writer_open_property_writer (f.writer, GMIME_TNEF_ATTRIBUTE_TAG_RECIPIENT_TABLE, NULL);
		for (guint i = 0; i < G_N_ELEMENTS (rows); i++) {
			g_assert_true (g_mime_tnef_property_writer_begin_row (f.pw, NULL));
			tag_int (f.pw, GMIME_TNEF_PROPERTY_TAG_RECIPIENT_TYPE, rows[i].role);
			tag_int (f.pw, GMIME_TNEF_PROPERTY_TAG_RECIPIENT_FLAGS, rows[i].flags);
			tag_int (f.pw, GMIME_TNEF_PROPERTY_TAG_RECIPIENT_TRACK_STATUS, rows[i].track);
			write_text_property (f.pw, GMIME_TNEF_PROPERTY_TAG_DISPLAY_NAME_W, rows[i].name);
			if (rows[i].smtp)
				write_text_property (f.pw, GMIME_TNEF_PROPERTY_TAG_SMTP_ADDRESS_W, rows[i].smtp);
			if (rows[i].type)
				write_text_property (f.pw, GMIME_TNEF_PROPERTY_TAG_ADDRTYPE_W, rows[i].type);
			if (rows[i].email)
				write_text_property (f.pw, GMIME_TNEF_PROPERTY_TAG_EMAIL_ADDRESS_W, rows[i].email);
			if (rows[i].key) {
				g_assert_true (g_mime_tnef_property_writer_write_property_tag (f.pw, GMIME_TNEF_PROPERTY_TAG_SEARCH_KEY, NULL));
				g_assert_true (g_mime_tnef_property_writer_write_value_bytes (f.pw, (const guint8 *) rows[i].key,
						strlen (rows[i].key) + 1, NULL));
			}
		}
		fixture_close_properties (&f);
		fixture_message (&f, publish ? "IPM.Appointment" : "IPM.Schedule.Meeting.Request", TRUE);
		fixture_int (&f, 0x8217, 1);
		if (!publish) {
			write_text_property (f.pw, GMIME_TNEF_PROPERTY_TAG_SUBJECT_W, "Team sync");
			write_text_property (f.pw, GMIME_TNEF_PROPERTY_TAG_BODY_W, "Agenda;\r\nItem 1, Item 2\r\n");
			fixture_time (&f, 0x820e, 7, 1, 15);
			fixture_time (&f, 0x1a, 1, 2, 3);
			fixture_int (&f, 0x8201, 3);
			fixture_int (&f, 0x8205, 2);
			fixture_bool (&f, 0x8503, TRUE);
			fixture_int (&f, 0x8501, 10);
			g_assert_true (g_mime_tnef_property_writer_write_property_tag (f.pw, GMIME_TNEF_PROPERTY_TAG_RESPONSE_REQUESTED, NULL));
			g_assert_true (g_mime_tnef_property_writer_write_value_boolean (f.pw, TRUE, NULL));
		}
		ics = fixture_finish (&f, GMIME_TNEF_CONVERSION_LOSS_UNPARSABLE_RECIPIENT);
		assert_line (ics, "ORGANIZER;CN=Alice:mailto:alice@example.com");
		g_assert_cmpuint (line_count (ics, "ATTENDEE;"), ==, 6);
		g_assert_null (strstr (ics, "CN=Dave"));
		if (publish) {
			assert_line (ics, "ATTENDEE;ROLE=REQ-PARTICIPANT;CN=Bob:mailto:bob@example.com");
			assert_line (ics, "ATTENDEE;ROLE=OPT-PARTICIPANT;PARTSTAT=TENTATIVE;CN=Carol:mailto:carol@example.com");
			assert_line (ics, "ATTENDEE;CUTYPE=RESOURCE;ROLE=NON-PARTICIPANT;PARTSTAT=ACCEPTED;CN=Room 1:mailto:room1@example.com");
			assert_line (ics, "ATTENDEE;ROLE=REQ-PARTICIPANT;PARTSTAT=DECLINED;CN=Eve:mailto:eve@example.com");
		} else {
			assert_line (ics, "DTSTAMP:20240102T030000Z");
			assert_line (ics, "DTEND:20240701T150000Z");
			assert_line (ics, "SUMMARY:Team sync");
			assert_line (ics, "DESCRIPTION:Agenda\\;\\nItem 1\\, Item 2");
			assert_line (ics, "SEQUENCE:3");
			assert_line (ics, "TRANSP:OPAQUE");
			assert_line (ics, "X-MICROSOFT-CDO-BUSYSTATUS:BUSY");
			assert_line (ics, "BEGIN:VALARM\r\nACTION:DISPLAY\r\nDESCRIPTION:Reminder\r\nTRIGGER:-PT10M\r\nEND:VALARM");
			assert_line (ics, "ATTENDEE;ROLE=REQ-PARTICIPANT;PARTSTAT=NEEDS-ACTION;RSVP=TRUE;CN=Bob:mailto:bob@example.com");
			assert_line (ics, "ATTENDEE;ROLE=OPT-PARTICIPANT;PARTSTAT=NEEDS-ACTION;RSVP=TRUE;CN=Carol:mailto:carol@example.com");
			assert_line (ics, "ATTENDEE;CUTYPE=RESOURCE;ROLE=NON-PARTICIPANT;PARTSTAT=NEEDS-ACTION;RSVP=TRUE;CN=Room 1:mailto:room1@example.com");
			assert_line (ics, "ATTENDEE;ROLE=REQ-PARTICIPANT;PARTSTAT=NEEDS-ACTION;RSVP=TRUE;CN=Frank:mailto:FRANK@EXAMPLE.COM");
			assert_line (ics, "ATTENDEE;ROLE=REQ-PARTICIPANT;PARTSTAT=NEEDS-ACTION;RSVP=TRUE;CN=Grace:invalid:nomail");
		}
		g_free (ics);
	}
}

static void
test_extensions (void)
{
	for (guint request = 0; request < 2; request++) {
		CalendarFixture f = fixture_new (request ? "IPM.Schedule.Meeting.Request" : "IPM.Appointment", TRUE);
		GMimeTnefNameId *key = g_mime_tnef_name_id_new_with_name ("00020329-0000-0000-c000-000000000046", "Keywords");
		char *ics;
		g_assert_true (g_mime_tnef_property_writer_write_named_property (f.pw, key, GMIME_TNEF_PROPERTY_TYPE_MULTI_VALUED |
				GMIME_TNEF_PROPERTY_TYPE_UNICODE, NULL));
		g_mime_tnef_name_id_unref (key);
		if (request)
			g_assert_true (g_mime_tnef_property_writer_write_value_string (f.pw, "Work", NULL));
		g_assert_true (g_mime_tnef_property_writer_write_value_string (f.pw, "", NULL));
		if (request)
			g_assert_true (g_mime_tnef_property_writer_write_value_string (f.pw, "Urgent, important", NULL));
		tag_int (f.pw, GMIME_TNEF_PROPERTY_TAG_OWNER_APPT_ID, 42);
		fixture_int (&f, 0x8224, 0);
		fixture_bool (&f, 0x8503, TRUE);
		fixture_int (&f, 0x8501, 0x5ae980e1);
		ics = fixture_finish (&f, -1);
		assert_line (ics, "TRIGGER:-PT15M");
		assert_line (ics, "X-MICROSOFT-CDO-OWNERAPPTID:42");
		if (request) {
			assert_line (ics, "X-MICROSOFT-CDO-INTENDEDSTATUS:FREE");
			assert_line (ics, "CATEGORIES:Work,Urgent\\, important");
		} else {
			g_assert_cmpuint (line_count (ics, "X-MICROSOFT-CDO-INTENDEDSTATUS:"), ==, 0);
			g_assert_cmpuint (line_count (ics, "CATEGORIES:"), ==, 0);
		}
		g_free (ics);
	}
}

static void
test_reply_without_organizer (void)
{
	CalendarFixture f = fixture_writer ();
	char *ics;
	f.pw = g_mime_tnef_writer_open_property_writer (f.writer, GMIME_TNEF_ATTRIBUTE_TAG_RECIPIENT_TABLE, NULL);
	g_assert_true (g_mime_tnef_property_writer_begin_row (f.pw, NULL));
	tag_int (f.pw, GMIME_TNEF_PROPERTY_TAG_RECIPIENT_TYPE, GMIME_TNEF_RECIPIENT_TYPE_CC);
	write_text_property (f.pw, GMIME_TNEF_PROPERTY_TAG_DISPLAY_NAME_W, "Alice");
	write_text_property (f.pw, GMIME_TNEF_PROPERTY_TAG_SMTP_ADDRESS_W, "alice@example.com");
	fixture_close_properties (&f);
	fixture_message (&f, " IPM.Schedule.Meeting.Resp.Neg ", TRUE);
	write_text_property (f.pw, GMIME_TNEF_PROPERTY_TAG_SENT_REPRESENTING_NAME_W, "Bob\r\nX-EVIL:1;\"x\":");
	write_text_property (f.pw, GMIME_TNEF_PROPERTY_TAG_SUBJECT_W, "Sync\r\nEND:VEVENT\r\nBEGIN:VEVENT\rUID:evil\nX-EVIL:1\a\033");
	ics = fixture_finish (&f, GMIME_TNEF_CONVERSION_LOSS_UNPARSABLE_RECIPIENT);
	assert_line (ics, "METHOD:REPLY");
	g_assert_cmpuint (line_count (ics, "ORGANIZER"), ==, 0);
	g_assert_cmpuint (line_count (ics, "ATTENDEE;PARTSTAT=DECLINED;CN="), ==, 1);
	g_assert_nonnull (strstr (ics, ":invalid:nomail\r\n"));
	g_assert_cmpuint (line_count (ics, "BEGIN:VEVENT"), ==, 1);
	g_assert_cmpuint (line_count (ics, "END:VEVENT"), ==, 1);
	g_assert_cmpuint (line_count (ics, "UID:"), ==, 1);
	g_assert_cmpuint (line_count (ics, "X-EVIL"), ==, 0);
	g_assert_null (strpbrk (ics, "\a\033"));
	g_free (ics);
}

static void
test_exception_overrides (void)
{
	const guint days[] = { 2, 7, 9 };
	const guint flags[] = { 0x2c, 8, 8 };
	GByteArray *a = recurrence_blob (1, 0, 1, 0xa, 0, 0, 0x2022);
	CalendarFixture f = fixture_new ("IPM.Appointment", TRUE);
	char *ics, **components;
	g_byte_array_set_size (a, a->len - 6);
	u16 (a, 3);
	for (guint i = 0; i < 3; i++) {
		guint start = JULY_1_2024 + days[i] * 1440 + 600;
		u32 (a, start); u32 (a, start + 60); u32 (a, start); u16 (a, flags[i]);
		if (!i)
			u32 (a, 5);
		u32 (a, i != 1);
		if (!i)
			u32 (a, 0);
	}
	u32 (a, 0);
	for (guint i = 0; i < 3; i++) {
		u32 (a, 0); u32 (a, 0);
	}
	add_recurrence (&f, a);
	ics = fixture_finish (&f, -1);
	components = g_strsplit (ics, "BEGIN:VEVENT", -1);
	g_assert_cmpuint (g_strv_length (components), ==, 5);
	g_assert_null (strstr (components[1], "\r\nTRIGGER:"));
	assert_line (components[2], "RECURRENCE-ID;TZID=Eastern Standard Time:20240703T100000");
	assert_line (components[2], "TRIGGER:-PT5M");
	assert_line (components[2], "TRANSP:TRANSPARENT");
	assert_line (components[2], "X-MICROSOFT-CDO-BUSYSTATUS:FREE");
	assert_line (components[3], "RECURRENCE-ID;TZID=Eastern Standard Time:20240708T100000");
	g_assert_null (strstr (components[3], "\r\nTRIGGER:"));
	assert_line (components[4], "RECURRENCE-ID;TZID=Eastern Standard Time:20240710T100000");
	assert_line (components[4], "TRIGGER:-PT15M");
	g_strfreev (components); g_free (ics); g_byte_array_unref (a);
}

static void
test_extended_exception (void)
{
	GByteArray *a = weekly ();
	const char *subject = "Moved sync", *location = "Room 2";
	/* Replace the flags and suffix of weekly() with narrow and Unicode
	 * overrides. The narrow strings must not win over ExtendedException. */
	g_byte_array_set_size (a, a->len - 14);
	u16 (a, 0x11);
	u16 (a, 11); u16 (a, 10); g_byte_array_append (a, (const guint8 *) subject, 10);
	u16 (a, 7); u16 (a, 6); g_byte_array_append (a, (const guint8 *) "Room 1", 6);
	u32 (a, 0); u32 (a, 4); u32 (a, 0); u32 (a, 0);
	for (guint i = 0; i < 3; i++)
		u32 (a, 194074560 + 14 * 1440 + (i == 0 ? 600 : i == 1 ? 660 : 540));
	u16 (a, 10);
	for (guint i = 0; subject[i]; i++)
		u16 (a, subject[i] == 'y' ? 0xfd : subject[i]);
	u16 (a, 6);
	for (guint i = 0; location[i]; i++)
		u16 (a, location[i]);
	u32 (a, 0); u32 (a, 0);
	for (guint date = 0; date < 2; date++) {
		CalendarFixture f = fixture_new ("IPM.Appointment", FALSE);
		GDateTime *d = g_date_time_new_utc (1970, 1, 1, 14, 0, 0);
		char *ics, **events, *uid;
		named (f.pw, 0x820d, GMIME_TNEF_PROPERTY_TYPE_SYS_TIME);
		g_assert_true (g_mime_tnef_property_writer_write_value_date_time (f.pw, d, NULL));
		g_date_time_unref (d);
		fixture_bool (&f, 0x8215, date);
		add_recurrence (&f, a);
		ics = fixture_finish (&f, -1);
		events = g_strsplit (ics, "BEGIN:VEVENT", -1);
		g_assert_cmpuint (g_strv_length (events), ==, 3);
		assert_line (events[1], "RRULE:FREQ=WEEKLY;BYDAY=TH;COUNT=5");
		assert_line (events[1], date ? "DTSTART;VALUE=DATE:19700101" : "DTSTART;TZID=Eastern Standard Time:19700101T090000");
		assert_line (events[1], date ? "EXDATE;VALUE=DATE:19700108" : "EXDATE;TZID=Eastern Standard Time:19700108T090000");
		assert_line (events[2], date ? "RECURRENCE-ID;VALUE=DATE:19700115" : "RECURRENCE-ID;TZID=Eastern Standard Time:19700115T090000");
		assert_line (events[2], date ? "DTSTART;VALUE=DATE:19700115" : "DTSTART;TZID=Eastern Standard Time:19700115T100000");
		assert_line (events[2], "SUMMARY:Moved s\xc3\xbdnc");
		assert_line (events[2], "LOCATION:Room 2");
		g_assert_cmpuint (line_count (events[2], "RRULE:"), ==, 0);
		g_assert_cmpuint (line_count (events[2], "EXDATE"), ==, 0);
		uid = strstr (events[1], "\r\nUID:");
		g_assert_nonnull (uid);
		uid += 2;
		uid[strcspn (uid, "\r\n")] = 0;
		assert_line (events[2], uid);
		g_strfreev (events); g_free (ics);
	}
	g_byte_array_unref (a);
}

static void
test_attachment_fallback (void)
{
	static const guint8 render[14] = { 1, 0, 255, 255, 255, 255 };
	static const guint8 bad_object[] = { 7,3,2,0,0,0,0,0,0xc0,0,0,0,0,0,0,0x46,1,2,3,4,5,6 };
	/* Only the flagged attachment matching the moved start (in local
	 * time) should be consumed. A malformed exception must be retained. */
	CalendarFixture f = fixture_new ("IPM.Appointment", TRUE);
	GByteArray *a = many_exceptions (1);
	char *ics;
	add_recurrence (&f, a);
	fixture_close_properties (&f);
	for (guint i = 0; i < 3; i++) {
		GDateTime *d = g_date_time_new_utc (2024, 7, 1, 13, 0, 0);
		g_assert_true (g_mime_tnef_writer_write_attribute_bytes (f.writer, GMIME_TNEF_ATTRIBUTE_TAG_ATTACH_RENDER_DATA,
				render, sizeof render, NULL));
		f.pw = g_mime_tnef_writer_open_property_writer (f.writer, GMIME_TNEF_ATTRIBUTE_TAG_ATTACHMENT, NULL);
		tag_int (f.pw, GMIME_TNEF_PROPERTY_TAG_ATTACHMENT_FLAGS, i ? 2 : 0);
		tag_int (f.pw, GMIME_TNEF_PROPERTY_TAG_ATTACH_METHOD, 5);
		g_assert_true (g_mime_tnef_property_writer_write_property_tag (f.pw, GMIME_TNEF_PROPERTY_TAG_EXCEPTION_START_TIME, NULL));
		g_assert_true (g_mime_tnef_property_writer_write_value_date_time (f.pw, d, NULL));
		g_date_time_unref (d);
		g_assert_true (g_mime_tnef_property_writer_write_property_tag (f.pw, GMIME_TNEF_PROPERTY_TAG_ATTACH_DATA_OBJ, NULL));
		if (i == 2) {
			g_assert_true (g_mime_tnef_property_writer_write_value_bytes (f.pw, bad_object, sizeof bad_object, NULL));
		} else {
			GMimeTnefWriter *embedded = g_mime_tnef_property_writer_open_embedded_message (f.pw, 1252, NULL);
			GMimeTnefPropertyWriter *pw = g_mime_tnef_writer_open_property_writer (embedded, GMIME_TNEF_ATTRIBUTE_TAG_MAPI_PROPERTIES, NULL);
			write_text_property (pw, GMIME_TNEF_PROPERTY_TAG_BODY_W, i ? "Matched by start" : "Unflagged");
			g_assert_true (g_mime_tnef_property_writer_close (pw, NULL));
			g_assert_true (g_mime_tnef_writer_close (embedded, NULL));
			g_object_unref (pw); g_object_unref (embedded);
		}
		fixture_close_properties (&f);
	}
	ics = fixture_finish (&f, INVALID);
	g_assert_cmpuint (f.retained, ==, 2);
	g_assert_cmpuint (line_count (ics, "BEGIN:VEVENT"), ==, 2);
	assert_line (ics, "RECURRENCE-ID;TZID=Eastern Standard Time:20240701T100000");
	assert_line (ics, "DESCRIPTION:Matched by start");
	g_assert_null (strstr (ics, "Unflagged"));
	g_free (ics);
	g_byte_array_unref (a);
}

static void
test_date_bounds (void)
{
	static const guint date_fields[] = { 42, 54, 58, 62 };
	for (guint i = 0; i < G_N_ELEMENTS (date_fields); i++) {
		CalendarFixture f = fixture_new ("IPM.Appointment", TRUE);
		GByteArray *a = weekly ();
		char *ics;
		set_u32 (a, date_fields[i], G_MAXUINT32);
		add_recurrence (&f, a);
		ics = fixture_finish (&f, ANY_CALENDAR_LOSS);
		assert_line (ics, "RRULE:FREQ=WEEKLY;BYDAY=TH;COUNT=5");
		g_assert_cmpuint (line_count (ics, "BEGIN:VEVENT"), ==, line_count (ics, "END:VEVENT"));
		g_free (ics); g_byte_array_unref (a);
	}
	{
		CalendarFixture f = fixture_new ("IPM.Appointment", FALSE);
		char *ics;
		for (guint i = 0; i < 2; i++) {
			GDateTime *d = g_date_time_new_utc (9999, 12, 30 + i, 23, 59, 59);
			named (f.pw, 0x820d + i, GMIME_TNEF_PROPERTY_TYPE_SYS_TIME);
			g_assert_true (g_mime_tnef_property_writer_write_value_date_time (f.pw, d, NULL));
			g_date_time_unref (d);
		}
		ics = fixture_finish (&f, -1);
		assert_line (ics, "DTSTART:99991230T235959Z");
		assert_line (ics, "DTEND:99991231T235959Z");
		g_free (ics);
	}
}

static void
test_all_day (void)
{
	CalendarFixture f = fixture_new ("IPM.Appointment", FALSE);
	GByteArray *zone = eastern_zone (TRUE, 300);
	char *ics;
	fixture_time (&f, 0x820d, 7, 4, 4);
	fixture_time (&f, 0x820e, 7, 5, 4);
	fixture_bool (&f, 0x8215, TRUE);
	fixture_blob (&f, 0x825e, zone);
	ics = fixture_finish (&f, -1);
	assert_line (ics, "DTSTART;VALUE=DATE:20240704");
	assert_line (ics, "DTEND;VALUE=DATE:20240705");
	g_assert_cmpuint (line_count (ics, "BEGIN:VTIMEZONE"), ==, 0);
	g_free (ics); g_byte_array_unref (zone);
}

int
main (int argc, char **argv)
{
	static const MeetingCase meetings[] = {
		{ "IPM.Schedule.Meeting.Resp.Pos", "REPLY", "ACCEPTED" },
		{ "IPM.Schedule.Meeting.Resp.Tent", "REPLY", "TENTATIVE" },
		{ "IPM.Schedule.Meeting.Resp.Neg", "REPLY", "DECLINED" },
		{ "IPM.Schedule.Meeting.Resp.Tent", "COUNTER", "TENTATIVE" },
		{ "IPM.Schedule.Meeting.Canceled", "CANCEL", NULL }
	};
	int status;
	g_test_init (&argc, &argv, NULL);
	g_mime_init ();
	g_test_add_func ("/tnef/calendar/attachment-fallback", test_attachment_fallback);
	g_test_add_func ("/tnef/calendar/all-day", test_all_day);
	g_test_add_func ("/tnef/calendar/robustness/date-bounds", test_date_bounds);
	g_test_add_func ("/tnef/calendar/exception-overrides", test_exception_overrides);
	g_test_add_func ("/tnef/calendar/extended-exception", test_extended_exception);
	g_test_add_func ("/tnef/calendar/attendees", test_attendees);
	g_test_add_func ("/tnef/calendar/extensions", test_extensions);
	g_test_add_func ("/tnef/calendar/reply-without-organizer", test_reply_without_organizer);
	g_test_add_func ("/tnef/calendar/selection", test_calendar_selection);
	g_test_add_func ("/tnef/calendar/event-properties", test_event_properties);
	g_test_add_func ("/tnef/calendar/robustness/sweeps", test_calendar_sweeps);
	g_test_add_func ("/tnef/calendar/robustness/malformed-fields", test_malformed_fields);
	g_test_add_func ("/tnef/calendar/robustness/exception-limits", test_exception_limits);
	g_test_add_func ("/tnef/calendar/recurrence-ends", test_recurrence_ends);
	g_test_add_func ("/tnef/calendar/zone-struct", test_zone_struct);
	g_test_add_func ("/tnef/calendar/display-zones", test_display_zones);
	g_test_add_func ("/tnef/calendar/uid", test_uid);
	g_test_add_func ("/tnef/calendar/instance-id", test_instance_id);
	for (guint i = 0; i < G_N_ELEMENTS (rules); i++) {
		char *path = g_strconcat ("/tnef/calendar/rules/", rules[i].name, NULL);
		g_test_add_data_func (path, rules + i, test_rule);
		g_free (path);
	}
	for (guint index = 0; index < G_N_ELEMENTS (meetings); index++) {
		char *path = g_strdup_printf ("/tnef/calendar/meeting/%s/%s", meetings[index].method, meetings[index].message_class);
		g_test_add_data_func (path, meetings + index, test_meeting);
		g_free (path);
	}
	g_test_add_func ("/tnef/calendar/folding", test_folding);
	g_test_add_func ("/tnef/calendar/recurrence", test_recurrence);
	status = g_test_run ();
	g_mime_shutdown ();
	return status;
}
