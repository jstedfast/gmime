/* Adapted from MimeKit.Fuzzing.Targets.FiltersTarget and RtfCompressedTarget.
 * Copyright (c) 2013-2026 .NET Foundation and Contributors.
 * Distributed under the MIT license; see LICENSE. */

#include "fuzz.h"
#include <string.h>

static GByteArray *
filter_bytes (GMimeFilter *filter, const guint8 *data, gsize length, gsize chunk)
{
	GMimeStream *output = g_mime_stream_mem_new ();
	GMimeStream *stream = g_mime_stream_filter_new (output);
	GByteArray *result;

	g_mime_stream_filter_add (GMIME_STREAM_FILTER (stream), filter);
	for (gsize offset = 0; offset < length; ) {
		gsize size = MIN (chunk, length - offset);
		g_assert_cmpint (g_mime_stream_write (stream, (const char *) data + offset, size), ==, size);
		offset += size;
	}
	g_assert_cmpint (g_mime_stream_flush (stream), ==, 0);
	result = g_byte_array_ref (g_mime_stream_mem_get_byte_array (GMIME_STREAM_MEM (output)));
	g_object_unref (stream);
	g_object_unref (output);
	return result;
}

static GMimeFilter *
new_filter (guint kind)
{
	switch (kind) {
	case 0: case 1: return g_mime_filter_dos2unix_new (kind == 1);
	case 2: case 3: return g_mime_filter_unix2dos_new (kind == 3);
	case 4: case 5: return g_mime_filter_from_new (kind == 4 ? GMIME_FILTER_FROM_MODE_ESCAPE : GMIME_FILTER_FROM_MODE_ARMOR);
	case 6: return g_mime_filter_strip_new ();
	case 7: return g_mime_filter_best_new (GMIME_FILTER_BEST_ENCODING);
	default: return g_mime_filter_openpgp_new ();
	}
}

static GString *
reference_filter (guint kind, const guint8 *data, gsize length)
{
	GString *output = g_string_sized_new (length);
	for (gsize index = 0; index < length; index++) {
		guint8 value = data[index];

		if (kind < 2 && value == '\r' && index + 1 < length && data[index + 1] == '\n')
			continue;
		if ((kind == 2 || kind == 3) && value == '\n' && (!index || data[index - 1] != '\r'))
			g_string_append_c (output, '\r');
		if ((kind == 4 || kind == 5) && (!index || data[index - 1] == '\n') &&
		    length - index >= 5 && !memcmp (data + index, "From ", 5)) {
			if (kind == 4)
				g_string_append_c (output, '>');
			else {
				g_string_append (output, "=46");
				continue;
			}
		}
		if (kind == 6 && (value == '\r' || value == '\n')) {
			while (output->len && (output->str[output->len - 1] == ' ' || output->str[output->len - 1] == '\t'))
				g_string_truncate (output, output->len - 1);
		}
		g_string_append_c (output, value);
	}
	if (kind == 6) {
		while (output->len && (output->str[output->len - 1] == ' ' || output->str[output->len - 1] == '\t'))
			g_string_truncate (output, output->len - 1);
	} else if ((kind == 1 || kind == 3) && (!length || data[length - 1] != '\n')) {
		if (kind == 1 && length && data[length - 1] == '\r')
			g_string_truncate (output, output->len - 1);
		if (kind == 3 && (!length || data[length - 1] != '\r'))
			g_string_append_c (output, '\r');
		g_string_append_c (output, '\n');
	}
	return output;
}

void
fuzz_filters (const guint8 *data, gsize length)
{
	guint kind = length ? data[0] % 9 : 0;
	guint selector = fuzz_selector (data, length);
	GMimeFilter *whole = new_filter (kind), *split = new_filter (kind);
	if (length) {
		data++;
		length--;
	}
	GByteArray *expected = filter_bytes (whole, data, length, MAX (length, 1));
	GByteArray *actual = filter_bytes (split, data, length, 1 + selector % 79);

	fuzz_equal (expected->data, expected->len, actual->data, actual->len, "Filter chunking");
	if (kind < 7) {
		GString *reference = reference_filter (kind, data, length);
		fuzz_equal ((const guint8 *) reference->str, reference->len, actual->data, actual->len, "Filter vs reference");
		g_string_free (reference, TRUE);
	} else if (kind == 7) {
		fuzz_equal (data, length, actual->data, actual->len, "Best-encoding pass-through");
		g_assert_cmpint (g_mime_filter_best_encoding (GMIME_FILTER_BEST (whole), GMIME_ENCODING_CONSTRAINT_7BIT),
			     ==, g_mime_filter_best_encoding (GMIME_FILTER_BEST (split), GMIME_ENCODING_CONSTRAINT_7BIT));
	} else if (kind == 8) {
		GMimeFilterOpenPGP *first = GMIME_FILTER_OPENPGP (whole), *second = GMIME_FILTER_OPENPGP (split);
		g_assert_cmpint (g_mime_filter_openpgp_get_data_type (first), ==, g_mime_filter_openpgp_get_data_type (second));
		g_assert_cmpint (g_mime_filter_openpgp_get_begin_offset (first), ==, g_mime_filter_openpgp_get_begin_offset (second));
		g_assert_cmpint (g_mime_filter_openpgp_get_end_offset (first), ==, g_mime_filter_openpgp_get_end_offset (second));
	}
	g_byte_array_unref (actual);
	g_byte_array_unref (expected);
	g_object_unref (whole);
	g_object_unref (split);
}

static GBytes *
encode_rtf (const guint8 *data, gsize length, gboolean compress)
{
	GMimeStream *output = g_mime_stream_mem_new ();
	GMimeTnefWriter *writer = g_mime_tnef_writer_new (output, 1252, 0, NULL);
	GMimeTnefPropertyWriter *properties = g_mime_tnef_writer_open_property_writer (writer,
		GMIME_TNEF_ATTRIBUTE_TAG_MAPI_PROPERTIES, NULL);
	GMimeStream *encoded;
	GMimeTnefReader *reader;
	GMimeTnefPropertyReader *input;
	GBytes *result;

	g_assert_true (g_mime_tnef_property_writer_write_property_tag (properties, GMIME_TNEF_PROPERTY_TAG_RTF_COMPRESSED, NULL));
	encoded = g_mime_tnef_property_writer_open_rtf_compressed_stream (properties, compress, NULL);
	if (length)
		g_assert_cmpint (g_mime_stream_write (encoded, (const char *) data, length), ==, length);
	g_assert_cmpint (g_mime_stream_close (encoded), ==, 0);
	g_assert_true (g_mime_tnef_property_writer_close (properties, NULL));
	g_assert_true (g_mime_tnef_writer_close (writer, NULL));
	g_object_unref (encoded);
	g_object_unref (properties);
	g_object_unref (writer);

	g_assert_cmpint (g_mime_stream_reset (output), ==, 0);
	reader = g_mime_tnef_reader_new (output, NULL);
	do {
		g_assert_true (g_mime_tnef_reader_read (reader, NULL));
	} while (g_mime_tnef_reader_get_tag (reader) != GMIME_TNEF_ATTRIBUTE_TAG_MAPI_PROPERTIES);
	input = g_mime_tnef_reader_get_property_reader (reader, NULL);
	g_assert_true (g_mime_tnef_property_reader_read_next_property (input, NULL));
	result = g_mime_tnef_property_reader_read_value_as_bytes (input, NULL);
	g_assert_nonnull (result);
	g_object_unref (input);
	g_object_unref (reader);
	g_object_unref (output);
	return result;
}

void
fuzz_rtf_compressed (const guint8 *data, gsize length)
{
	guint selector = fuzz_selector (data, length);
	GMimeFilter *whole = g_mime_filter_rtf_compressed_new (), *split = g_mime_filter_rtf_compressed_new ();
	GByteArray *first = filter_bytes (whole, data, length, MAX (length, 1));
	GByteArray *second = filter_bytes (split, data, length, 1 + selector % 127);
	GMimeFilterRtfCompressed *decoder = GMIME_FILTER_RTF_COMPRESSED (split);

	fuzz_equal (first->data, first->len, second->data, second->len, "Compressed RTF split decoding");
	g_assert_cmpint (g_mime_filter_rtf_compressed_get_compression_mode (GMIME_FILTER_RTF_COMPRESSED (whole)), ==,
			 g_mime_filter_rtf_compressed_get_compression_mode (decoder));
	g_assert_cmpint (g_mime_filter_rtf_compressed_is_valid_crc32 (GMIME_FILTER_RTF_COMPRESSED (whole)), ==,
			 g_mime_filter_rtf_compressed_is_valid_crc32 (decoder));
	g_byte_array_unref (first);
	g_byte_array_unref (second);
	/* Keep encoder work bounded independently of the raw decoder input. */
	gsize payload_length = MIN (length, 16384);
	GBytes *encoded = encode_rtf (data, payload_length, selector & 1);
	gsize size;
	const guint8 *bytes = g_bytes_get_data (encoded, &size);
	g_mime_filter_reset (split);
	second = filter_bytes (split, bytes, size, 1 + selector % 127);
	fuzz_equal (data, payload_length, second->data, second->len, "Compressed RTF writer round trip");
	if (selector & 1)
		g_assert_true (g_mime_filter_rtf_compressed_is_valid_crc32 (decoder));
	g_byte_array_unref (second);
	g_bytes_unref (encoded);
	g_object_unref (whole);
	g_object_unref (split);
}
