/* SPDX-License-Identifier: LGPL-2.1-or-later
 * Test fixtures for MS-OXOCAL properties. Included by test-tnef-calendar.c. */

#define INVALID GMIME_TNEF_CONVERSION_LOSS_INVALID_CALENDAR_DATA
#define UNSUPPORTED GMIME_TNEF_CONVERSION_LOSS_UNSUPPORTED_CALENDAR_DATA
#define LIMIT GMIME_TNEF_CONVERSION_LOSS_CALENDAR_EXCEPTION_LIMIT_EXCEEDED
#define ANY_CALENDAR_LOSS (-2)
#define JULY_1_2024 222737760u /* minutes since 1601-01-01 */

typedef struct {
	GMimeStream *stream;
	GMimeTnefWriter *writer;
	GMimeTnefPropertyWriter *pw;
	GMimeTnefConversionOptions *opts;
	gsize wire_size;
	guint retained;
	gboolean calendar_root, plain_root;
} CalendarFixture;

static void
fixture_time (CalendarFixture *f, int id, int month, int day, int hour)
{
	GDateTime *d = g_date_time_new_utc (2024, month, day, hour, 0, 0);
	named (f->pw, id, GMIME_TNEF_PROPERTY_TYPE_SYS_TIME);
	g_assert_true (g_mime_tnef_property_writer_write_value_date_time (f->pw, d, NULL));
	g_date_time_unref (d);
}

static CalendarFixture
fixture_writer (void)
{
	CalendarFixture f = { 0 };
	f.stream = g_mime_stream_mem_new ();
	f.writer = g_mime_tnef_writer_new (f.stream, 1252, 0, NULL);
	f.opts = g_mime_tnef_conversion_options_new ();
	return f;
}

static void
fixture_message (CalendarFixture *f, const char *message_class, gboolean start)
{
	f->pw = g_mime_tnef_writer_open_property_writer (f->writer, GMIME_TNEF_ATTRIBUTE_TAG_MAPI_PROPERTIES, NULL);
	write_text_property (f->pw, GMIME_TNEF_PROPERTY_TAG_MESSAGE_CLASS_W, message_class);
	if (start)
		fixture_time (f, 0x820d, 7, 1, 14);
}

static CalendarFixture
fixture_new (const char *message_class, gboolean start)
{
	CalendarFixture f = fixture_writer ();
	fixture_message (&f, message_class, start);
	return f;
}

static void
fixture_blob (CalendarFixture *f, int id, GByteArray *blob)
{
	named (f->pw, id, GMIME_TNEF_PROPERTY_TYPE_BINARY);
	g_assert_true (g_mime_tnef_property_writer_write_value_bytes (f->pw, blob->data, blob->len, NULL));
}

static void
fixture_bool (CalendarFixture *f, int id, gboolean value)
{
	named (f->pw, id, GMIME_TNEF_PROPERTY_TYPE_BOOLEAN);
	g_assert_true (g_mime_tnef_property_writer_write_value_boolean (f->pw, value, NULL));
}

static void
fixture_int (CalendarFixture *f, int id, int value)
{
	named (f->pw, id, GMIME_TNEF_PROPERTY_TYPE_LONG);
	g_assert_true (g_mime_tnef_property_writer_write_value_int32 (f->pw, value, NULL));
}

static void
fixture_close_properties (CalendarFixture *f)
{
	if (f->pw) {
		g_assert_true (g_mime_tnef_property_writer_close (f->pw, NULL));
		g_clear_object (&f->pw);
	}
}

static void
tag_int (GMimeTnefPropertyWriter *pw, GMimeTnefPropertyTag tag, int value)
{
	g_assert_true (g_mime_tnef_property_writer_write_property_tag (pw, tag, NULL));
	g_assert_true (g_mime_tnef_property_writer_write_value_int32 (pw, value, NULL));
}

/* Finish a public writer -> loader -> MIME round trip. Return unfolded lines;
 * tests of wire folding use convert() instead. -1 expects no loss. */
static char *
fixture_finish (CalendarFixture *f, int loss)
{
	GMimeTnefConversionResult *result;
	GMimeTnefMessage *source;
	GMimePart *part;
	GMimeEntity *root;
	char *text = NULL;

	fixture_close_properties (f);
	g_assert_true (g_mime_tnef_writer_close (f->writer, NULL));
	g_object_unref (f->writer);
	g_assert_cmpint (g_mime_stream_reset (f->stream), ==, 0);
	source = g_mime_tnef_message_load (f->stream, NULL, NULL);
	g_assert_nonnull (source);
	result = g_mime_tnef_message_convert_to_mime (source, f->opts, NULL);
	g_assert_nonnull (result);
	if (loss == ANY_CALENDAR_LOSS) {
		for (guint i = 0; i < g_mime_tnef_conversion_result_get_loss_count (result); i++) {
			int kind = g_mime_tnef_conversion_result_get_loss (result, i)->kind;
			g_assert_true (kind == INVALID || kind == UNSUPPORTED || kind == LIMIT);
		}
	} else {
		g_assert_cmpuint (g_mime_tnef_conversion_result_get_loss_count (result), ==, loss < 0 ? 0 : 1);
	}
	if (loss >= 0)
		g_assert_cmpint (g_mime_tnef_conversion_result_get_loss (result, 0)->kind, ==, loss);
	root = g_mime_message_get_mime_part (g_mime_tnef_conversion_result_get_message (result));
	part = find_calendar (root, &f->retained);
	f->calendar_root = part && root == GMIME_ENTITY (part);
	f->plain_root = g_mime_content_type_is_type (g_mime_entity_get_content_type (root), "text", "plain");
	if (part) {
		GMimeStream *out = g_mime_stream_mem_new ();
		GByteArray *bytes = g_mime_stream_mem_get_byte_array (GMIME_STREAM_MEM (out));
		char *raw, **pieces;
		g_assert_cmpint (g_mime_content_write_to_stream (g_mime_part_get_content (part), out), >, 0);
		f->wire_size = bytes->len;
		raw = g_strndup ((char *) bytes->data, bytes->len);
		g_assert_true (g_str_has_suffix (raw, "END:VEVENT\r\nEND:VCALENDAR\r\n"));
		pieces = g_strsplit (raw, "\r\n ", -1);
		text = g_strjoinv ("", pieces);
		g_strfreev (pieces);
		g_free (raw);
		g_object_unref (out);
	}
	g_mime_tnef_conversion_result_unref (result);
	g_object_unref (source);
	g_mime_tnef_conversion_options_unref (f->opts);
	g_object_unref (f->stream);
	return text;
}

static void
assert_line (const char *ics, const char *line)
{
	char *needle = g_strconcat ("\r\n", line, "\r\n", NULL);
	g_assert_nonnull (ics);
	if (!strstr (ics, needle))
		g_error ("Missing line '%s' in:\n%s", line, ics);
	g_free (needle);
}

static GByteArray *
recurrence_blob (guint pattern, guint calendar, guint period, guint first, guint second, guint wkst, guint end_type)
{
	GByteArray *a = g_byte_array_new ();
	u16 (a, 0x3004); u16 (a, 0x3004); u16 (a, 0x200b); u16 (a, pattern); u16 (a, calendar);
	u32 (a, 0); u32 (a, period); u32 (a, 0);
	if (pattern)
		u32 (a, first);
	if (pattern == 3 || pattern == 11)
		u32 (a, second);
	u32 (a, end_type); u32 (a, 10); u32 (a, wkst);
	u32 (a, 0); u32 (a, 0); /* deleted, modified counts */
	u32 (a, JULY_1_2024); u32 (a, JULY_1_2024 + 30 * 1440);
	u32 (a, 0x3006); u32 (a, 0x3009); u32 (a, 600); u32 (a, 660);
	u16 (a, 0); u32 (a, 0); /* no exceptions or reserved data */
	return a;
}

static void
system_time (GByteArray *a, guint month, guint ordinal)
{
	u16 (a, 0); u16 (a, month); u16 (a, 0); u16 (a, ordinal);
	u16 (a, 2); u16 (a, 0); u16 (a, 0); u16 (a, 0);
}

static GByteArray *
eastern_zone (gboolean definition, int bias)
{
	const char name[] = "Eastern Standard Time";
	GByteArray *a = g_byte_array_new ();
	if (definition) {
		u16 (a, 0x0102); u16 (a, 6 + 2 * (sizeof name - 1)); u16 (a, 2); u16 (a, sizeof name - 1);
		for (guint i = 0; name[i]; i++)
			u16 (a, name[i]);
		u16 (a, 1); /* rule count */
		u16 (a, 0x0102); u16 (a, 0x003e); u16 (a, 2); u16 (a, 2007);
		for (guint i = 0; i < 7; i++)
			u16 (a, 0);
	}
	u32 (a, bias); u32 (a, 0); u32 (a, (guint32) -60);
	if (!definition)
		u16 (a, 0);
	system_time (a, 11, 1);
	if (!definition)
		u16 (a, 0);
	system_time (a, 3, 2);
	return a;
}

static void
add_recurrence (CalendarFixture *f, GByteArray *pattern)
{
	GByteArray *zone = eastern_zone (TRUE, 300);
	fixture_blob (f, 0x8216, pattern);
	fixture_blob (f, 0x8260, zone);
	g_byte_array_unref (zone);
}

static void
set_u32 (GByteArray *a, guint offset, guint32 value)
{
	value = GUINT32_TO_LE (value);
	g_assert_cmpuint (offset + 4, <=, a->len);
	memcpy (a->data + offset, &value, 4);
}

static guint
line_count (const char *ics, const char *prefix)
{
	guint count = 0;
	g_assert_nonnull (ics);
	while (*ics) {
		if (g_str_has_prefix (ics, prefix))
			count++;
		ics = strstr (ics, "\r\n");
		g_assert_nonnull (ics);
		ics += 2;
	}
	return count;
}

static GByteArray *
global_object_id (guint year, guint month, guint day, const char *vcal)
{
	static const guint8 signature[16] = { 4,0,0,0,0x82,0,0xe0,0,0x74,0xc5,0xb7,0x10,0x1a,0x82,0xe0,8 };
	guint8 date[] = { year >> 8, year & 255, month, day };
	GByteArray *a = g_byte_array_new ();
	g_byte_array_append (a, signature, sizeof signature);
	g_byte_array_append (a, date, sizeof date);
	for (guint8 b = 0x11; b <= 0x18; b++)
		g_byte_array_append (a, &b, 1);
	u32 (a, 0); u32 (a, 0);
	u32 (a, vcal ? 13 + strlen (vcal) : 0);
	if (vcal) {
		g_byte_array_append (a, (const guint8 *) "vCal-Uid\1\0\0\0", 12);
		g_byte_array_append (a, (const guint8 *) vcal, strlen (vcal) + 1);
	}
	return a;
}
