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
#include <errno.h>
#include <string.h>
#include <gmime/gmime.h>

static const char *data_directory;
static const char property_set[] = "00020329-0000-0000-c000-000000000046";

#define CHECK(expression) \
	G_STMT_START \
	{ \
		gboolean success = (expression); \
		g_assert_no_error (error); \
		g_assert_true (success); \
	} \
	G_STMT_END

typedef struct {
	GMimeStream parent_instance;
	GMimeStream *inner;
	gsize chunk;
	gint64 fail_after;
	gboolean zero_write, fail_seek;
} TestStream;

typedef GMimeStreamClass TestStreamClass;
G_DEFINE_TYPE (TestStream, test_stream, GMIME_TYPE_STREAM)

static ssize_t
test_read (GMimeStream *base, char *buffer, size_t length)
{
	TestStream *stream = (TestStream *) base;
	ssize_t count;
	gint64 position = g_mime_stream_get_position (base);

	if (position >= stream->fail_after) {
		errno = EIO;
		return -1;
	}
	length = MIN (length, MIN (stream->chunk, (gsize) (stream->fail_after - position)));
	count = g_mime_stream_read (stream->inner, buffer, length);
	if (count > 0)
		g_mime_stream_set_position (base, position + count);
	return count;
}

static ssize_t
test_write (GMimeStream *base, const char *buffer, size_t length)
{
	TestStream *stream = (TestStream *) base;
	ssize_t count;
	gint64 position = g_mime_stream_get_position (base);

	if (stream->zero_write)
		return 0;
	if (position >= stream->fail_after) {
		errno = EIO;
		return -1;
	}
	length = MIN (length, MIN (stream->chunk, (gsize) (stream->fail_after - position)));
	count = g_mime_stream_write (stream->inner, buffer, length);
	if (count > 0)
		g_mime_stream_set_position (base, position + count);
	return count;
}

static gint64
test_seek (GMimeStream *base, gint64 offset, GMimeSeekWhence whence)
{
	TestStream *stream = (TestStream *) base;
	gint64 position;

	if (stream->fail_seek) {
		errno = EIO;
		return -1;
	}
	position = g_mime_stream_seek (stream->inner, offset, whence);
	if (position >= 0)
		g_mime_stream_set_position (base, position);
	return position;
}

static gint64
test_length (GMimeStream *base)
{
	return g_mime_stream_length (((TestStream *) base)->inner);
}

static void
test_stream_finalize (GObject *object)
{
	g_clear_object (&((TestStream *) object)->inner);
	G_OBJECT_CLASS (test_stream_parent_class)->finalize (object);
}

static void
test_stream_class_init (TestStreamClass *klass)
{
	G_OBJECT_CLASS (klass)->finalize = test_stream_finalize;
	klass->read = test_read;
	klass->write = test_write;
	klass->seek = test_seek;
	klass->length = test_length;
}

static void
test_stream_init (TestStream *stream)
{
	stream->inner = g_mime_stream_mem_new ();
	stream->chunk = 3;
	stream->fail_after = G_MAXINT64;
	g_mime_stream_construct (GMIME_STREAM (stream), 0, -1);
}

static GMimeTnefPropertyReader *
find_properties (GMimeTnefReader *reader)
{
	GError *error = NULL;
	GMimeTnefPropertyReader *properties;

	while (g_mime_tnef_reader_read (reader, &error)) {
		if (g_mime_tnef_reader_get_tag (reader) != GMIME_TNEF_ATTRIBUTE_TAG_MAPI_PROPERTIES)
			continue;
		properties = g_mime_tnef_reader_get_property_reader (reader, &error);
		g_assert_no_error (error);
		g_assert_nonnull (properties);
		return properties;
	}
	g_assert_no_error (error);
	g_assert_not_reached ();
}

static GMimeTnefReader *
rewind_reader (GMimeStream *stream, GMimeTnefOptions *options)
{
	g_assert_cmpint (g_mime_stream_seek (stream, 0, GMIME_STREAM_SEEK_SET), ==, 0);
	return g_mime_tnef_reader_new (stream, options);
}

static void
write_tag (GMimeTnefPropertyWriter *writer, guint16 id, GMimeTnefPropertyType type)
{
	GError *error = NULL;
	CHECK (g_mime_tnef_property_writer_write_property_tag (writer, g_mime_tnef_property_tag_new (id, type),
							       &error));
}

static void
test_properties (void)
{
	TestStream *stream = g_object_new (test_stream_get_type (), NULL);
	GMimeTnefWriter *writer, *copy_writer;
	GMimeTnefPropertyWriter *properties, *copy_properties;
	GMimeTnefReader *reader;
	GMimeTnefPropertyReader *input;
	GMimeTnefPropertySet *set, *copied;
	GMimeTnefProperty *property;
	GMimeTnefNameId *name;
	GMimeStream *copy = g_mime_stream_mem_new ();
	GDateTime *date = g_date_time_new_utc (2026, 6, 12, 14, 25, 30.125);
	GError *error = NULL;
	guint i;
	GPtrArray *rows;
	const GValue *value;

	writer = g_mime_tnef_writer_new (GMIME_STREAM (stream), 1252, 0x1234, &error);
	g_assert_no_error (error);
	CHECK (g_mime_tnef_writer_write_attribute_string (writer, GMIME_TNEF_ATTRIBUTE_TAG_SUBJECT, "Caf\xc3\xa9",
							  &error));
	CHECK (g_mime_tnef_writer_write_attribute_date_time (writer, GMIME_TNEF_ATTRIBUTE_TAG_DATE_SENT, date, &error));
	properties = g_mime_tnef_writer_open_property_writer (writer, GMIME_TNEF_ATTRIBUTE_TAG_RECIPIENT_TABLE, &error);
	for (i = 0; i < 2; i++) {
		CHECK (g_mime_tnef_property_writer_begin_row (properties, &error));
		write_tag (properties, 0x3001, GMIME_TNEF_PROPERTY_TYPE_UNICODE);
		CHECK (g_mime_tnef_property_writer_write_value_string (properties, i ? "Second" : "First", &error));
	}
	CHECK (g_mime_tnef_property_writer_close (properties, &error));
	g_object_unref (properties);
	properties = g_mime_tnef_writer_open_property_writer (writer, GMIME_TNEF_ATTRIBUTE_TAG_MAPI_PROPERTIES, &error);
	write_tag (properties, 0x2000, GMIME_TNEF_PROPERTY_TYPE_I2);
	CHECK (g_mime_tnef_property_writer_write_value_int16 (properties, -1234, &error));
	write_tag (properties, 0x2001, GMIME_TNEF_PROPERTY_TYPE_LONG);
	CHECK (g_mime_tnef_property_writer_write_value_int32 (properties, G_MININT32, &error));
	write_tag (properties, 0x2002, GMIME_TNEF_PROPERTY_TYPE_I8);
	CHECK (g_mime_tnef_property_writer_write_value_int64 (properties, G_MININT64 + 1, &error));
	write_tag (properties, 0x2003, GMIME_TNEF_PROPERTY_TYPE_BOOLEAN);
	CHECK (g_mime_tnef_property_writer_write_value_boolean (properties, TRUE, &error));
	write_tag (properties, 0x2004, GMIME_TNEF_PROPERTY_TYPE_R4);
	CHECK (g_mime_tnef_property_writer_write_value_float (properties, 1.25f, &error));
	write_tag (properties, 0x2005, GMIME_TNEF_PROPERTY_TYPE_DOUBLE);
	CHECK (g_mime_tnef_property_writer_write_value_double (properties, -2.125, &error));
	write_tag (properties, 0x2006, GMIME_TNEF_PROPERTY_TYPE_CURRENCY);
	CHECK (g_mime_tnef_property_writer_write_value_currency (properties, G_MAXINT64, &error));
	write_tag (properties, 0x2007, GMIME_TNEF_PROPERTY_TYPE_SYS_TIME);
	CHECK (g_mime_tnef_property_writer_write_value_date_time (properties, date, &error));
	write_tag (properties, 0x2008, GMIME_TNEF_PROPERTY_TYPE_APP_TIME);
	CHECK (g_mime_tnef_property_writer_write_value_date_time (properties, date, &error));
	write_tag (properties, 0x2009, GMIME_TNEF_PROPERTY_TYPE_CLASS_ID);
	CHECK (g_mime_tnef_property_writer_write_value_guid (properties, property_set, &error));
	write_tag (properties, 0x200a, GMIME_TNEF_PROPERTY_TYPE_BINARY);
	CHECK (g_mime_tnef_property_writer_write_value_bytes (properties, (const guint8 *) "a\0b", 3, &error));
	write_tag (properties, 0x200b, GMIME_TNEF_PROPERTY_TYPE_NULL);
	write_tag (properties, 0x0037, GMIME_TNEF_PROPERTY_TYPE_STRING8);
	CHECK (g_mime_tnef_property_writer_write_value_string (properties, "\xe2\x82\xac 10", &error));
	write_tag (properties, 0x0037, GMIME_TNEF_PROPERTY_TYPE_UNICODE);
	CHECK (g_mime_tnef_property_writer_write_value_string (properties, "duplicate \xf0\x9f\x98\x80", &error));
	name = g_mime_tnef_name_id_new_with_name (property_set, "custom \xce\xa9");
	CHECK (g_mime_tnef_property_writer_write_named_property (properties, name,
								 GMIME_TNEF_PROPERTY_TYPE_UNICODE |
									 GMIME_TNEF_PROPERTY_TYPE_MULTI_VALUED,
								 &error));
	CHECK (g_mime_tnef_property_writer_write_value_string (properties, "one", &error));
	CHECK (g_mime_tnef_property_writer_write_value_string (properties, "two", &error));
	CHECK (g_mime_tnef_property_writer_write_named_property (properties, name, GMIME_TNEF_PROPERTY_TYPE_LONG,
								 &error));
	CHECK (g_mime_tnef_property_writer_write_value_int32 (properties, 42, &error));
	CHECK (g_mime_tnef_property_writer_close (properties, &error));
	CHECK (g_mime_tnef_writer_close (writer, &error));
	g_object_unref (properties);
	g_object_unref (writer);

	reader = rewind_reader (GMIME_STREAM (stream), NULL);
	while (g_mime_tnef_reader_read (reader, &error)) {
		if (g_mime_tnef_reader_get_tag (reader) == GMIME_TNEF_ATTRIBUTE_TAG_RECIPIENT_TABLE) {
			input = g_mime_tnef_reader_get_property_reader (reader, &error);
			rows = g_mime_tnef_property_reader_read_rows_as_property_sets (input, &error);
			g_assert_no_error (error);
			g_assert_cmpuint (rows->len, ==, 2);
			property = g_mime_tnef_property_set_get_property (g_ptr_array_index (rows, 1), 0);
			g_assert_cmpstr (g_value_get_string (g_mime_tnef_property_get_value (property, 0)), ==,
					 "Second");
			g_ptr_array_unref (rows);
			g_object_unref (input);
			break;
		}
	}
	g_assert_cmpuint (g_mime_tnef_reader_get_legacy_key (reader), ==, 0x1234);
	input = find_properties (reader);
	set = g_mime_tnef_property_reader_read_property_set (input, &error);
	g_assert_no_error (error);
	g_assert_cmpuint (g_mime_tnef_property_set_get_count (set), ==, 16);
	g_object_unref (input);
	g_object_unref (reader);
	property =
		g_mime_tnef_property_set_lookup (set, g_mime_tnef_property_tag_new (0x0037,
										    GMIME_TNEF_PROPERTY_TYPE_UNICODE));
	g_assert_cmpstr (g_value_get_string (g_mime_tnef_property_get_value (property, 0)), ==, "\xe2\x82\xac 10");
	property = g_mime_tnef_property_set_lookup_name (set, name);
	g_assert_cmpuint (g_mime_tnef_property_get_count (property), ==, 2);
	g_assert_cmpstr (g_value_get_string (g_mime_tnef_property_get_value (property, 1)), ==, "two");
	g_assert_cmpuint (g_mime_tnef_property_tag_get_id (g_mime_tnef_property_get_tag (property)), ==,
			  g_mime_tnef_property_tag_get_id (
				  g_mime_tnef_property_get_tag (g_mime_tnef_property_set_get_property (set, 15))));
	property = g_mime_tnef_property_set_get_property (set, 0);
	g_assert_cmpint (g_value_get_int (g_mime_tnef_property_get_value (property, 0)), ==, -1234);
	property = g_mime_tnef_property_set_get_property (set, 2);
	g_assert_cmpint (g_value_get_int64 (g_mime_tnef_property_get_value (property, 0)), ==, G_MININT64 + 1);
	property = g_mime_tnef_property_set_get_property (set, 6);
	g_assert_cmpint (g_value_get_int64 (g_mime_tnef_property_get_value (property, 0)), ==, G_MAXINT64);
	for (i = 7; i <= 8; i++) {
		property = g_mime_tnef_property_set_get_property (set, i);
		value = g_mime_tnef_property_get_value (property, 0);
		g_assert_cmpint (g_date_time_difference (g_value_get_boxed (value), date), ==, 0);
	}
	copy_writer = g_mime_tnef_writer_new (copy, 65001, 0, &error);
	copy_properties =
		g_mime_tnef_writer_open_property_writer (copy_writer, GMIME_TNEF_ATTRIBUTE_TAG_MAPI_PROPERTIES, &error);
	for (i = 0; i < g_mime_tnef_property_set_get_count (set); i++)
		CHECK (g_mime_tnef_property_writer_write_property (copy_properties,
								   g_mime_tnef_property_set_get_property (set, i),
								   &error));
	CHECK (g_mime_tnef_property_writer_close (copy_properties, &error));
	CHECK (g_mime_tnef_writer_close (copy_writer, &error));
	reader = rewind_reader (copy, NULL);
	input = find_properties (reader);
	copied = g_mime_tnef_property_reader_read_property_set (input, &error);
	g_assert_no_error (error);
	g_assert_cmpuint (g_mime_tnef_property_set_get_count (copied), ==, 16);
	property = g_mime_tnef_property_set_lookup_name (copied, name);
	g_assert_cmpstr (g_value_get_string (g_mime_tnef_property_get_value (property, 1)), ==, "two");
	g_mime_tnef_property_set_unref (copied);
	g_mime_tnef_property_set_unref (set);
	g_mime_tnef_name_id_unref (name);
	g_date_time_unref (date);
	g_object_unref (copy_properties);
	g_object_unref (copy_writer);
	g_object_unref (input);
	g_object_unref (reader);
	g_object_unref (copy);
	g_object_unref (stream);
}

static GBytes *
compress_rtf (const guint8 *data, gsize length, gboolean compress, guint chunk)
{
	GMimeStream *output = g_mime_stream_mem_new (), *rtf;
	GError *error = NULL;
	GMimeTnefWriter *writer = g_mime_tnef_writer_new (output, 1252, 0, &error);
	GMimeTnefPropertyWriter *properties =
		g_mime_tnef_writer_open_property_writer (writer, GMIME_TNEF_ATTRIBUTE_TAG_MAPI_PROPERTIES, &error);
	GMimeTnefReader *reader;
	GMimeTnefPropertyReader *input;
	GBytes *bytes;
	gsize offset, count;

	write_tag (properties, GMIME_TNEF_PROPERTY_ID_RTF_COMPRESSED, GMIME_TNEF_PROPERTY_TYPE_BINARY);
	rtf = g_mime_tnef_property_writer_open_rtf_compressed_stream (properties, compress, &error);
	g_assert_no_error (error);
	for (offset = 0; offset < length; offset += count) {
		count = MIN (chunk, length - offset);
		g_assert_cmpint (g_mime_stream_write (rtf, (const char *) data + offset, count), ==, count);
	}
	g_assert_cmpint (g_mime_stream_close (rtf), ==, 0);
	CHECK (g_mime_tnef_property_writer_close (properties, &error));
	CHECK (g_mime_tnef_writer_close (writer, &error));
	reader = rewind_reader (output, NULL);
	input = find_properties (reader);
	CHECK (g_mime_tnef_property_reader_read_next_property (input, &error));
	bytes = g_mime_tnef_property_reader_read_value_as_bytes (input, &error);
	g_assert_no_error (error);
	g_object_unref (input);
	g_object_unref (reader);
	g_object_unref (rtf);
	g_object_unref (properties);
	g_object_unref (writer);
	g_object_unref (output);
	return bytes;
}

static GBytes *
decompress_rtf (GBytes *bytes, guint chunk, gboolean valid)
{
	GMimeFilter *filter = g_mime_filter_rtf_compressed_new ();
	GByteArray *result = g_byte_array_new ();
	const char *data;
	char *output;
	gsize length, offset, count, output_length, prespace;

	data = g_bytes_get_data (bytes, &length);
	for (offset = 0; offset < length; offset += count) {
		count = MIN (chunk, length - offset);
		g_mime_filter_filter (filter, (char *) data + offset, count, 0, &output, &output_length, &prespace);
		g_byte_array_append (result, (guint8 *) output, output_length);
	}
	g_mime_filter_complete (filter, NULL, 0, 0, &output, &output_length, &prespace);
	g_byte_array_append (result, (guint8 *) output, output_length);
	g_assert_cmpint (g_mime_filter_rtf_compressed_is_valid_crc32 (GMIME_FILTER_RTF_COMPRESSED (filter)), ==, valid);
	g_mime_filter_reset (filter);
	g_assert_cmpint (g_mime_filter_rtf_compressed_get_compression_mode (GMIME_FILTER_RTF_COMPRESSED (filter)), ==,
			 GMIME_RTF_COMPRESSION_MODE_UNKNOWN);
	g_assert_false (g_mime_filter_rtf_compressed_is_valid_crc32 (GMIME_FILTER_RTF_COMPRESSED (filter)));
	g_object_unref (filter);
	return g_byte_array_free_to_bytes (result);
}

static void
test_rtf (void)
{
	static const char text[] = "{\\rtf1\\ansi\\ansicpg1252\\pard hello world}\r\n";
	static const guint8 expected[] = { 0x2d, 0,    0,    0,    0x2b, 0,    0,    0,    0x4c, 0x5a, 0x46, 0x75, 0xf1,
					   0xc5, 0xc7, 0xa7, 0x03, 0,    0x0a, 0,    0x72, 0x63, 0x70, 0x67, 0x31, 0x32,
					   0x35, 0x42, 0x32, 0x0a, 0xf3, 0x20, 0x68, 0x65, 0x6c, 0x09, 0,    0x20, 0x62,
					   0x77, 0x05, 0xb0, 0x6c, 0x64, 0x7d, 0x0a, 0x80, 0x0f, 0xa0 };
	static const guint chunks[] = { 1, 7, 4096, G_MAXUINT };
	GBytes *compressed, *decoded, *source, *damaged;
	GByteArray *large = g_byte_array_new ();
	guint i, j, mode;
	guint8 corrupt[sizeof expected + 3];

	/* [MS-OXRTFCP] 4.1 fixes the exact compressed representation, not just a round trip. */
	compressed = compress_rtf ((const guint8 *) text, sizeof text - 1, TRUE, 1);
	g_assert_cmpmem (g_bytes_get_data (compressed, NULL), g_bytes_get_size (compressed), expected, sizeof expected);
	g_bytes_unref (compressed);
	for (i = 0; i < 1000; i++)
		g_byte_array_append (large, (const guint8 *) text, sizeof text - 1);
	for (mode = 0; mode < 2; mode++) {
		for (i = 0; i < G_N_ELEMENTS (chunks); i++) {
			for (j = 0; j < 3; j++) {
				source = j == 0 ? g_bytes_new (NULL, 0) :
					 j == 1 ? g_bytes_new (text, sizeof text - 1) :
						  g_bytes_new (large->data, large->len);
				compressed = compress_rtf (g_bytes_get_data (source, NULL), g_bytes_get_size (source),
							   mode, chunks[i]);
				decoded = decompress_rtf (compressed, chunks[i], TRUE);
				g_assert_true (g_bytes_equal (decoded, source));
				g_bytes_unref (source);
				g_bytes_unref (compressed);
				g_bytes_unref (decoded);
			}
		}
	}
	memcpy (corrupt, expected, sizeof expected);
	memset (corrupt + sizeof expected, 0xff, 3);
	damaged = g_bytes_new (corrupt, sizeof corrupt);
	decoded = decompress_rtf (damaged, 1, TRUE);
	g_assert_cmpmem (g_bytes_get_data (decoded, NULL), g_bytes_get_size (decoded), text, sizeof text - 1);
	g_bytes_unref (decoded);
	g_bytes_unref (damaged);
	corrupt[12] ^= 1;
	damaged = g_bytes_new (corrupt, sizeof expected);
	decoded = decompress_rtf (damaged, 7, FALSE);
	g_bytes_unref (decoded);
	g_bytes_unref (damaged);
	corrupt[8] = 0;
	damaged = g_bytes_new (corrupt, sizeof expected);
	decoded = decompress_rtf (damaged, 1, FALSE);
	g_assert_cmpuint (g_bytes_get_size (decoded), ==, 0);
	g_bytes_unref (decoded);
	g_bytes_unref (damaged);
	for (i = 0; i < 16; i++) {
		damaged = g_bytes_new (expected, i);
		decoded = decompress_rtf (damaged, 1, FALSE);
		g_assert_cmpuint (g_bytes_get_size (decoded), ==, 0);
		g_bytes_unref (decoded);
		g_bytes_unref (damaged);
	}
	g_byte_array_unref (large);
}

static void
test_nested_streams (void)
{
	GMimeStream *output = g_mime_stream_mem_new (), *value;
	GMimeTnefWriter *writer, *embedded;
	GMimeTnefPropertyWriter *properties;
	GMimeTnefReader *reader, *nested;
	GMimeTnefPropertyReader *input;
	GError *error = NULL;
	char buffer[8], *text;

	writer = g_mime_tnef_writer_new (output, 1252, 0, &error);
	properties = g_mime_tnef_writer_open_property_writer (writer, GMIME_TNEF_ATTRIBUTE_TAG_MAPI_PROPERTIES, &error);
	write_tag (properties, 0x3701, GMIME_TNEF_PROPERTY_TYPE_OBJECT);
	embedded = g_mime_tnef_property_writer_open_embedded_message (properties, 65001, &error);
	CHECK (g_mime_tnef_writer_write_attribute_string (embedded, GMIME_TNEF_ATTRIBUTE_TAG_SUBJECT, "nested \xce\xa9",
							  &error));
	g_assert_false (g_mime_tnef_property_writer_close (properties, &error));
	g_assert_error (error, GMIME_ERROR, GMIME_ERROR_INVALID_OPERATION);
	g_clear_error (&error);
	CHECK (g_mime_tnef_writer_close (embedded, &error));
	g_object_unref (embedded);
	write_tag (properties, 0x2000, GMIME_TNEF_PROPERTY_TYPE_BINARY);
	value = g_mime_tnef_property_writer_open_value_stream (properties, &error);
	g_assert_cmpint (g_mime_stream_write (value, "binary", 6), ==, 6);
	g_assert_cmpint (g_mime_stream_seek (value, 0, GMIME_STREAM_SEEK_SET), ==, -1);
	g_assert_cmpint (errno, ==, ESPIPE);
	g_assert_cmpint (g_mime_stream_read (value, buffer, 1), ==, -1);
	g_assert_cmpint (errno, ==, EBADF);
	g_assert_cmpint (g_mime_stream_close (value), ==, 0);
	g_object_unref (value);
	CHECK (g_mime_tnef_property_writer_close (properties, &error));
	CHECK (g_mime_tnef_writer_close (writer, &error));
	g_object_unref (properties);
	g_object_unref (writer);

	reader = rewind_reader (output, NULL);
	input = find_properties (reader);
	CHECK (g_mime_tnef_property_reader_read_next_property (input, &error));
	g_assert_true (g_mime_tnef_property_reader_is_embedded_message (input));
	nested = g_mime_tnef_property_reader_open_embedded_message (input, &error);
	g_assert_no_error (error);
	g_assert_cmpuint (g_mime_tnef_reader_get_depth (nested), ==, 1);
	while (g_mime_tnef_reader_read (nested, &error)) {
		if (g_mime_tnef_reader_get_tag (nested) == GMIME_TNEF_ATTRIBUTE_TAG_SUBJECT) {
			text = g_mime_tnef_reader_read_value_as_string (nested, &error);
			g_assert_cmpstr (text, ==, "nested \xce\xa9");
			g_free (text);
			break;
		}
	}
	g_assert_no_error (error);
	CHECK (g_mime_tnef_property_reader_read_next_property (input, &error));
	g_assert_false (g_mime_tnef_reader_read (nested, &error));
	g_assert_error (error, GMIME_ERROR, GMIME_ERROR_INVALID_OPERATION);
	g_clear_error (&error);
	g_object_unref (nested);
	value = g_mime_tnef_property_reader_open_value_stream (input, &error);
	g_assert_cmpint (g_mime_stream_write (value, "x", 1), ==, -1);
	g_assert_cmpint (errno, ==, EBADF);
	g_assert_cmpint (g_mime_stream_read (value, buffer, 2), ==, 2);
	g_assert_cmpmem (buffer, 2, "bi", 2);
	g_assert_false (g_mime_tnef_reader_read (reader, &error));
	g_assert_no_error (error);
	g_assert_cmpint (g_mime_stream_read (value, buffer, 1), ==, -1);
	g_object_unref (value);
	g_object_unref (reader);
	g_assert_false (g_mime_tnef_property_reader_read_next_property (input, &error));
	g_assert_error (error, GMIME_ERROR, GMIME_ERROR_INVALID_OPERATION);
	g_clear_error (&error);
	g_object_unref (input);
	g_object_unref (output);
}

typedef struct {
	guint counts[32];
	guint suppressed;
	gint64 last_offset;
	guint last_depth;
} Diagnostics;

static void
log_issue (const GMimeTnefComplianceIssue *issue, gpointer user_data)
{
	Diagnostics *diagnostics = user_data;
	if (issue->violation == GMIME_TNEF_COMPLIANCE_VIOLATION_TOO_MANY_COMPLIANCE_ISSUES) {
		diagnostics->suppressed++;
	} else {
		g_assert_cmpuint (issue->violation, <, G_N_ELEMENTS (diagnostics->counts));
		diagnostics->counts[issue->violation]++;
	}
	diagnostics->last_offset = issue->stream_offset;
	diagnostics->last_depth = issue->depth;
}

static void
test_limits_and_damage (void)
{
	GMimeStream *output = g_mime_stream_mem_new (), *raw;
	GMimeTnefWriter *writer;
	GMimeTnefPropertyWriter *properties;
	GMimeTnefReader *reader;
	GMimeTnefPropertyReader *input;
	GMimeTnefPropertySet *set;
	GMimeTnefOptions *options = g_mime_tnef_options_new ();
	GMimeTnefComplianceOptions *compliance = g_mime_tnef_compliance_options_new ();
	GError *error = NULL;
	GByteArray *bytes;
	Diagnostics diagnostics = { 0 };
	guint i;
	guint8 original;

	writer = g_mime_tnef_writer_new (output, 1252, 0, &error);
	properties = g_mime_tnef_writer_open_property_writer (writer, GMIME_TNEF_ATTRIBUTE_TAG_MAPI_PROPERTIES, &error);
	for (i = 0; i < 3; i++) {
		write_tag (properties, 0x2000 + i, GMIME_TNEF_PROPERTY_TYPE_BINARY);
		CHECK (g_mime_tnef_property_writer_write_value_bytes (properties, (const guint8 *) "large", 5, &error));
	}
	CHECK (g_mime_tnef_property_writer_close (properties, &error));
	CHECK (g_mime_tnef_writer_close (writer, &error));
	g_object_unref (properties);
	g_object_unref (writer);
	g_mime_tnef_options_set_max_property_value_length (options, 4);
	g_mime_tnef_compliance_options_set_max_issues_per_violation (compliance, 1);
	reader = rewind_reader (output, options);
	g_mime_tnef_reader_set_compliance_logger (reader, log_issue, &diagnostics, NULL);
	g_mime_tnef_reader_set_compliance_options (reader, compliance);
	input = find_properties (reader);
	set = g_mime_tnef_property_reader_read_property_set (input, &error);
	g_assert_no_error (error);
	g_assert_cmpuint (g_mime_tnef_property_set_get_count (set), ==, 3);
	for (i = 0; i < 3; i++)
		g_assert_cmpuint (g_mime_tnef_property_get_count (g_mime_tnef_property_set_get_property (set, i)), ==,
				  0);
	g_assert_cmpuint (diagnostics.counts[GMIME_TNEF_COMPLIANCE_VIOLATION_DATA_SIZE_LIMIT_EXCEEDED], ==, 1);
	g_assert_cmpuint (diagnostics.suppressed, ==, 1);
	g_mime_tnef_property_set_unref (set);
	g_object_unref (input);
	g_object_unref (reader);
	g_mime_tnef_options_unref (options);
	g_mime_tnef_compliance_options_unref (compliance);

	bytes = g_mime_stream_mem_get_byte_array (GMIME_STREAM_MEM (output));
	original = bytes->data[bytes->len - 1];
	bytes->data[bytes->len - 1] ^= 1;
	memset (&diagnostics, 0, sizeof diagnostics);
	reader = rewind_reader (output, NULL);
	g_mime_tnef_reader_set_compliance_logger (reader, log_issue, &diagnostics, NULL);
	while (g_mime_tnef_reader_read (reader, &error))
		;
	g_assert_no_error (error);
	g_assert_cmpuint (diagnostics.counts[GMIME_TNEF_COMPLIANCE_VIOLATION_ATTRIBUTE_CHECKSUM_MISMATCH], ==, 1);
	g_object_unref (reader);
	bytes->data[bytes->len - 1] = original;
	for (i = 0; i < bytes->len; i++) {
		raw = g_mime_stream_mem_new_with_buffer ((const char *) bytes->data, i);
		reader = g_mime_tnef_reader_new (raw, NULL);
		while (g_mime_tnef_reader_read (reader, &error)) {
			if (g_mime_tnef_reader_get_tag (reader) == GMIME_TNEF_ATTRIBUTE_TAG_MAPI_PROPERTIES) {
				input = g_mime_tnef_reader_get_property_reader (reader, &error);
				if (input) {
					set = g_mime_tnef_property_reader_read_property_set (input, &error);
					if (set)
						g_mime_tnef_property_set_unref (set);
					g_object_unref (input);
				}
			}
		}
		g_assert_no_error (error);
		g_object_unref (reader);
		g_object_unref (raw);
	}
	g_object_unref (output);
}

static GMimeStream *
temporary_factory (gpointer user_data, GError **error)
{
	(void) error;
	return g_object_ref (user_data);
}

static void
test_reader_errors (void)
{
	TestStream *stream = g_object_new (test_stream_get_type (), NULL);
	GMimeTnefWriter *writer;
	GMimeTnefReader *reader;
	GMimeStream *value;
	GError *error = NULL;
	Diagnostics diagnostics = { 0 };
	char buffer[16];
	gint64 length;
	ssize_t count;
	guint pass;

	writer = g_mime_tnef_writer_new (GMIME_STREAM (stream), 1252, 0, &error);
	CHECK (g_mime_tnef_writer_write_attribute_string (writer, GMIME_TNEF_ATTRIBUTE_TAG_SUBJECT, "reader failure",
							  &error));
	CHECK (g_mime_tnef_writer_close (writer, &error));
	g_object_unref (writer);
	length = g_mime_stream_length (GMIME_STREAM (stream));
	for (pass = 0; pass < 3; pass++) {
		reader = rewind_reader (GMIME_STREAM (stream), NULL);
		stream->fail_after = pass ? length - 3 : 0;
		g_mime_tnef_reader_set_compliance_logger (reader, log_issue, &diagnostics, NULL);
		while (g_mime_tnef_reader_read (reader, &error)) {
			if (g_mime_tnef_reader_get_tag (reader) != GMIME_TNEF_ATTRIBUTE_TAG_SUBJECT)
				continue;
			if (pass == 1) {
				g_assert_null (g_mime_tnef_reader_read_value_as_string (reader, &error));
			} else {
				value = g_mime_tnef_reader_open_value_stream (reader, &error);
				g_assert_no_error (error);
				do {
					count = g_mime_stream_read (value, buffer, sizeof buffer);
				} while (count > 0);
				g_assert_cmpint (count, ==, -1);
				g_assert_cmpint (errno, ==, EIO);
				g_object_unref (value);
				g_assert_false (g_mime_tnef_reader_read (reader, &error));
			}
			break;
		}
		g_assert_error (error, GMIME_ERROR, EIO);
		g_clear_error (&error);
		stream->fail_after = G_MAXINT64;
		g_assert_false (g_mime_tnef_reader_read (reader, &error));
		g_assert_error (error, GMIME_ERROR, EIO);
		g_clear_error (&error);
		g_assert_cmpuint (diagnostics.counts[GMIME_TNEF_COMPLIANCE_VIOLATION_TRUNCATED_STREAM], ==, 0);
		g_object_unref (reader);
	}
	g_object_unref (stream);
}

static void
test_text_encodings (void)
{
	static const struct {
		int codepage;
		const char *input, *expected;
	} cases[] = { { 1252,
			"A\xe2\x82\xac\xe2\x98\x83"
			"B",
			"A\xe2\x82\xac?B" },
		      { 936, "\xe4\xb8\x82", "\xe4\xb8\x82" },
		      { 50220, "\xe6\x97\xa5\xe6\x9c\xac\xe2\x98\x83\xe8\xaa\x9e",
			"\xe6\x97\xa5\xe6\x9c\xac?\xe8\xaa\x9e" } };

	GMimeStream *output;
	GMimeTnefWriter *writer;
	GMimeTnefPropertyWriter *properties;
	GMimeTnefReader *reader;
	GMimeTnefPropertyReader *input;
	GError *error = NULL;
	char *text;
	guint i, count;

	for (i = 0; i < G_N_ELEMENTS (cases); i++) {
		output = g_mime_stream_mem_new ();
		writer = g_mime_tnef_writer_new (output, cases[i].codepage, 0, &error);
		g_assert_no_error (error);
		CHECK (g_mime_tnef_writer_write_attribute_string (writer, GMIME_TNEF_ATTRIBUTE_TAG_SUBJECT,
								  cases[i].input, &error));
		properties = g_mime_tnef_writer_open_property_writer (writer, GMIME_TNEF_ATTRIBUTE_TAG_MAPI_PROPERTIES,
								      &error);
		write_tag (properties, 0x2000, GMIME_TNEF_PROPERTY_TYPE_STRING8);
		CHECK (g_mime_tnef_property_writer_write_value_string (properties, cases[i].input, &error));
		write_tag (properties, 0x2001, GMIME_TNEF_PROPERTY_TYPE_UNICODE);
		CHECK (g_mime_tnef_property_writer_write_value_string (properties, cases[i].input, &error));
		CHECK (g_mime_tnef_property_writer_close (properties, &error));
		CHECK (g_mime_tnef_writer_close (writer, &error));
		g_object_unref (properties);
		g_object_unref (writer);
		reader = rewind_reader (output, NULL);
		count = 0;
		while (g_mime_tnef_reader_read (reader, &error)) {
			if (g_mime_tnef_reader_get_tag (reader) == GMIME_TNEF_ATTRIBUTE_TAG_SUBJECT) {
				text = g_mime_tnef_reader_read_value_as_string (reader, &error);
				g_assert_cmpstr (text, ==, cases[i].expected);
				g_free (text);
				count++;
			} else if (g_mime_tnef_reader_get_tag (reader) == GMIME_TNEF_ATTRIBUTE_TAG_MAPI_PROPERTIES) {
				input = g_mime_tnef_reader_get_property_reader (reader, &error);
				CHECK (g_mime_tnef_property_reader_read_next_property (input, &error));
				text = g_mime_tnef_property_reader_read_value_as_string (input, &error);
				g_assert_cmpstr (text, ==, cases[i].expected);
				g_free (text);
				CHECK (g_mime_tnef_property_reader_read_next_property (input, &error));
				text = g_mime_tnef_property_reader_read_value_as_string (input, &error);
				g_assert_cmpstr (text, ==, cases[i].input);
				g_free (text);
				count += 2;
				g_object_unref (input);
			}
		}
		g_assert_no_error (error);
		g_assert_cmpuint (count, ==, 3);
		g_object_unref (reader);
		g_object_unref (output);
	}
}

static void
test_depth_budget_and_counts (void)
{
	static const guint8 malformed[] = { 2, 0, 0, 0, 0x1e, 0, 0x37, 0, 2, 0, 0, 0, 2, 0,    0,  0, 'A', 0,
					    0, 0, 2, 0, 0,    0, 'B',  0, 0, 0, 3, 0, 0, 0x20, 42, 0, 0,   0 };
	GMimeStream *output = g_mime_stream_mem_new ();
	GMimeTnefWriter *writer, *embedded;
	GMimeTnefPropertyWriter *properties, *nested_properties;
	GMimeTnefReader *reader, *nested;
	GMimeTnefPropertyReader *input, *nested_input;
	GMimeTnefOptions *options = g_mime_tnef_options_new ();
	GMimeTnefPropertySet *set;
	GMimeTnefProperty *property;
	GBytes *bytes;
	GError *error = NULL;
	Diagnostics diagnostics = { 0 };
	guint pass;

	writer = g_mime_tnef_writer_new (output, 1252, 0, &error);
	properties = g_mime_tnef_writer_open_property_writer (writer, GMIME_TNEF_ATTRIBUTE_TAG_MAPI_PROPERTIES, &error);
	write_tag (properties, 0x2000, GMIME_TNEF_PROPERTY_TYPE_BINARY);
	CHECK (g_mime_tnef_property_writer_write_value_bytes (properties, (const guint8 *) "1234", 4, &error));
	write_tag (properties, 0x3701, GMIME_TNEF_PROPERTY_TYPE_OBJECT);
	embedded = g_mime_tnef_property_writer_open_embedded_message (properties, 0, &error);
	nested_properties =
		g_mime_tnef_writer_open_property_writer (embedded, GMIME_TNEF_ATTRIBUTE_TAG_MAPI_PROPERTIES, &error);
	write_tag (nested_properties, 0x2000, GMIME_TNEF_PROPERTY_TYPE_BINARY);
	CHECK (g_mime_tnef_property_writer_write_value_bytes (nested_properties, (const guint8 *) "abcd", 4, &error));
	CHECK (g_mime_tnef_property_writer_close (nested_properties, &error));
	CHECK (g_mime_tnef_writer_close (embedded, &error));
	CHECK (g_mime_tnef_property_writer_close (properties, &error));
	CHECK (g_mime_tnef_writer_close (writer, &error));
	g_object_unref (nested_properties);
	g_object_unref (embedded);
	g_object_unref (properties);
	g_object_unref (writer);
	g_mime_tnef_options_set_max_total_data_bytes (options, 6);
	for (pass = 0; pass < 2; pass++) {
		if (pass)
			g_mime_tnef_options_set_max_nesting_depth (options, 0);
		reader = rewind_reader (output, options);
		g_mime_tnef_reader_set_compliance_logger (reader, log_issue, &diagnostics, NULL);
		input = find_properties (reader);
		CHECK (g_mime_tnef_property_reader_read_next_property (input, &error));
		bytes = g_mime_tnef_property_reader_read_value_as_bytes (input, &error);
		g_assert_cmpuint (g_bytes_get_size (bytes), ==, 4);
		g_bytes_unref (bytes);
		CHECK (g_mime_tnef_property_reader_read_next_property (input, &error));
		nested = g_mime_tnef_property_reader_open_embedded_message (input, &error);
		g_assert_no_error (error);
		if (pass) {
			g_assert_false (g_mime_tnef_reader_read (nested, &error));
			g_assert_no_error (error);
			g_assert_cmpuint (diagnostics.counts[GMIME_TNEF_COMPLIANCE_VIOLATION_NESTING_TOO_DEEP], ==, 1);
		} else {
			nested_input = find_properties (nested);
			CHECK (g_mime_tnef_property_reader_read_next_property (nested_input, &error));
			bytes = g_mime_tnef_property_reader_read_value_as_bytes (nested_input, &error);
			g_assert_no_error (error);
			g_assert_cmpuint (g_bytes_get_size (bytes), ==, 0);
			g_assert_cmpuint (diagnostics.counts[GMIME_TNEF_COMPLIANCE_VIOLATION_DATA_SIZE_LIMIT_EXCEEDED],
					  ==, 1);
			g_assert_cmpuint (diagnostics.last_depth, ==, 1);
			g_assert_cmpint (diagnostics.last_offset, >, g_mime_tnef_reader_get_stream_offset (reader));
			g_bytes_unref (bytes);
			g_object_unref (nested_input);
		}
		g_object_unref (nested);
		g_object_unref (input);
		g_object_unref (reader);
	}
	g_mime_tnef_options_unref (options);
	g_object_unref (output);
	output = g_mime_stream_mem_new ();
	writer = g_mime_tnef_writer_new (output, 1252, 0, &error);
	CHECK (g_mime_tnef_writer_write_attribute_bytes (writer, GMIME_TNEF_ATTRIBUTE_TAG_MAPI_PROPERTIES, malformed,
							 sizeof malformed, &error));
	CHECK (g_mime_tnef_writer_close (writer, &error));
	reader = rewind_reader (output, NULL);
	g_mime_tnef_reader_set_compliance_logger (reader, log_issue, &diagnostics, NULL);
	input = find_properties (reader);
	set = g_mime_tnef_property_reader_read_property_set (input, &error);
	g_assert_no_error (error);
	g_assert_cmpuint (g_mime_tnef_property_set_get_count (set), ==, 2);
	property = g_mime_tnef_property_set_get_property (set, 0);
	g_assert_cmpstr (g_value_get_string (g_mime_tnef_property_get_value (property, 0)), ==, "A");
	property = g_mime_tnef_property_set_get_property (set, 1);
	g_assert_cmpint (g_value_get_int (g_mime_tnef_property_get_value (property, 0)), ==, 42);
	g_assert_cmpuint (diagnostics.counts[GMIME_TNEF_COMPLIANCE_VIOLATION_INVALID_VALUE_COUNT], ==, 1);
	g_mime_tnef_property_set_unref (set);
	g_object_unref (input);
	g_object_unref (reader);
	g_object_unref (writer);
	g_object_unref (output);
}

static void
test_writer_errors (void)
{
	TestStream *stream, *temporary;
	GMimeTnefWriter *writer;
	GMimeTnefPropertyWriter *properties;
	GMimeStream *child;
	GError *error = NULL;
	guint i;

	for (i = 0; i < 5; i++) {
		stream = g_object_new (test_stream_get_type (), NULL);
		writer = g_mime_tnef_writer_new (GMIME_STREAM (stream), 1252, 0, &error);
		if (i < 2) {
			stream->zero_write = i == 0;
			stream->fail_after = i == 1 ? 12 : G_MAXINT64;
			g_assert_false (g_mime_tnef_writer_close (writer, &error));
			g_assert_error (error, GMIME_ERROR, EIO);
			g_clear_error (&error);
		} else {
			properties = g_mime_tnef_writer_open_property_writer (writer,
									      GMIME_TNEF_ATTRIBUTE_TAG_MAPI_PROPERTIES,
									      &error);
			write_tag (properties, 0x2000, GMIME_TNEF_PROPERTY_TYPE_BINARY);
			if (i == 2) {
				child = g_mime_tnef_property_writer_open_value_stream (properties, &error);
				g_object_unref (child);
			} else if (i == 3) {
				g_assert_false (g_mime_tnef_property_writer_close (properties, &error));
				g_assert_error (error, GMIME_ERROR, GMIME_ERROR_INVALID_OPERATION);
				g_clear_error (&error);
			}
			g_object_unref (properties);
		}
		g_assert_false (g_mime_tnef_writer_flush (writer, &error));
		g_assert_nonnull (error);
		g_clear_error (&error);
		g_object_unref (writer);
		g_object_unref (stream);
	}
	stream = g_object_new (test_stream_get_type (), NULL);
	temporary = g_object_new (test_stream_get_type (), NULL);
	writer = g_mime_tnef_writer_new (GMIME_STREAM (stream), 1252, 0, &error);
	g_mime_tnef_writer_set_temporary_stream_factory (writer, temporary_factory, temporary, NULL);
	properties = g_mime_tnef_writer_open_property_writer (writer, GMIME_TNEF_ATTRIBUTE_TAG_MAPI_PROPERTIES, &error);
	g_assert_no_error (error);
	write_tag (properties, 0x2000, GMIME_TNEF_PROPERTY_TYPE_BINARY);
	child = g_mime_tnef_property_writer_open_value_stream (properties, &error);
	g_assert_cmpint (g_mime_stream_write (child, "x", 1), ==, 1);
	temporary->fail_seek = TRUE;
	g_assert_cmpint (g_mime_stream_close (child), ==, -1);
	g_assert_false (g_mime_tnef_property_writer_close (properties, &error));
	g_assert_error (error, GMIME_ERROR, EIO);
	g_clear_error (&error);
	g_object_unref (child);
	g_object_unref (properties);
	g_object_unref (writer);
	g_object_unref (temporary);
	g_object_unref (stream);
}

static void
test_fixture (gconstpointer user_data)
{
	char *path = g_build_filename (data_directory, user_data, NULL), *data;
	gsize length;
	GError *error = NULL;
	GMimeStream *stream;
	GMimeTnefReader *reader;
	GMimeTnefPropertyReader *properties;
	GMimeTnefPropertySet *set;
	GPtrArray *rows;
	guint attributes = 0, rtf_count = 0;
	GMimeTnefProperty *rtf;
	GBytes *decoded;
	char *expected, *expected_path;
	gsize expected_length;

	CHECK (g_file_get_contents (path, &data, &length, &error));
	stream = g_mime_stream_mem_new_with_buffer (data, length);
	reader = g_mime_tnef_reader_new (stream, NULL);
	while (g_mime_tnef_reader_read (reader, &error)) {
		GMimeTnefAttributeTag tag = g_mime_tnef_reader_get_tag (reader);
		attributes++;
		if (tag != GMIME_TNEF_ATTRIBUTE_TAG_MAPI_PROPERTIES && tag != GMIME_TNEF_ATTRIBUTE_TAG_ATTACHMENT &&
		    tag != GMIME_TNEF_ATTRIBUTE_TAG_RECIPIENT_TABLE)
			continue;
		properties = g_mime_tnef_reader_get_property_reader (reader, &error);
		g_assert_no_error (error);
		if (tag == GMIME_TNEF_ATTRIBUTE_TAG_RECIPIENT_TABLE) {
			rows = g_mime_tnef_property_reader_read_rows_as_property_sets (properties, &error);
			g_assert_no_error (error);
			g_ptr_array_unref (rows);
		} else {
			set = g_mime_tnef_property_reader_read_property_set (properties, &error);
			g_assert_no_error (error);
			rtf = g_mime_tnef_property_set_lookup (set, GMIME_TNEF_PROPERTY_TAG_RTF_COMPRESSED);
			if (rtf && !strcmp (user_data, "rtf.tnef")) {
				decoded = decompress_rtf (g_value_get_boxed (g_mime_tnef_property_get_value (rtf, 0)),
							  1, TRUE);
				expected_path = g_build_filename (data_directory, "rtf.expected.rtf", NULL);
				CHECK (g_file_get_contents (expected_path, &expected, &expected_length, &error));
				g_assert_cmpmem (g_bytes_get_data (decoded, NULL), g_bytes_get_size (decoded), expected,
						 expected_length);
				g_bytes_unref (decoded);
				g_free (expected);
				g_free (expected_path);
				rtf_count++;
			}
			g_mime_tnef_property_set_unref (set);
		}
		g_object_unref (properties);
	}
	g_assert_no_error (error);
	g_assert_cmpuint (attributes, >, 2);
	if (!strcmp (user_data, "rtf.tnef"))
		g_assert_cmpuint (rtf_count, >, 0);
	g_object_unref (reader);
	g_object_unref (stream);
	g_free (data);
	g_free (path);
}

int
main (int argc, char **argv)
{
	static const char *fixtures[] = { "body.tnef", "rtf.tnef", "multi-value-attribute.tnef",
					  "unicode-mapi-attr-name.tnef", "MAPI_OBJECT.tnef" };
	guint i;
	int result;

	g_test_init (&argc, &argv, NULL);
	data_directory = "data/tnef";
	for (i = 1; i < (guint) argc; i++) {
		if (argv[i][0] != '-')
			data_directory = argv[i];
	}
	g_mime_init ();
	g_test_add_func ("/tnef/properties", test_properties);
	g_test_add_func ("/tnef/rtf", test_rtf);
	g_test_add_func ("/tnef/nested-streams", test_nested_streams);
	g_test_add_func ("/tnef/limits-and-damage", test_limits_and_damage);
	g_test_add_func ("/tnef/reader-errors", test_reader_errors);
	g_test_add_func ("/tnef/text-encodings", test_text_encodings);
	g_test_add_func ("/tnef/depth-budget-and-counts", test_depth_budget_and_counts);
	g_test_add_func ("/tnef/writer-errors", test_writer_errors);
	for (i = 0; i < G_N_ELEMENTS (fixtures); i++) {
		char *name = g_strdup_printf ("/tnef/fixtures/%s", fixtures[i]);
		g_test_add_data_func (name, fixtures[i], test_fixture);
		g_free (name);
	}
	result = g_test_run ();
	g_mime_shutdown ();
	return result;
}
