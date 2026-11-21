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
	GMimeTnefNameId *name = g_mime_tnef_name_id_new_with_id ("00062002-0000-0000-c000-000000000046", id);
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
	g_assert_nonnull (strstr (text, "DTSTART;TZID=\"Test:Zone\":19700115T100000\r\n"));
	g_assert_nonnull (strstr (text, "SUMMARY:Rescheduled\r\n"));
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
