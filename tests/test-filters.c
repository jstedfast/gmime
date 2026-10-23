/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*  GMime
 *  Copyright (C) 2000-2022 Jeffrey Stedfast
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */


#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>

#include <gmime/gmime.h>

#include "testsuite.h"

extern int verbose;

#define d(x) 
#define v(x) if (verbose > 3) x;


static GByteArray *
read_all_bytes (const char *path, gboolean is_text)
{
	GMimeStream *filtered, *stream, *mem;
	GMimeFilter *filter;
	GByteArray *buffer;
	
	buffer = g_byte_array_new ();
	stream = g_mime_stream_fs_open (path, O_RDONLY, 0644, NULL);
	mem = g_mime_stream_mem_new_with_byte_array (buffer);
	g_mime_stream_mem_set_owner ((GMimeStreamMem *) mem, FALSE);
	
	if (is_text) {
		filtered = g_mime_stream_filter_new (mem);
		filter = g_mime_filter_dos2unix_new (FALSE);
		g_mime_stream_filter_add ((GMimeStreamFilter *) filtered, filter);
		g_object_unref (filter);
		g_object_unref (mem);
	} else {
		filtered = mem;
	}
	
	g_mime_stream_write_to_stream (stream, filtered);
	g_mime_stream_flush (filtered);
	g_object_unref (filtered);
	g_object_unref (stream);
	
	return buffer;
}

static void
pump_data_through_filter (GMimeFilter *filter, const char *path, GMimeStream *ostream, gboolean is_text, gboolean inc)
{
	GMimeStream *onebyte, *filtered, *stream;
	GMimeFilter *dos2unix;
	
	filtered = g_mime_stream_filter_new (ostream);
	
	if (is_text) {
		/* canonicalize text input */
		dos2unix = g_mime_filter_dos2unix_new (FALSE);
		g_mime_stream_filter_add ((GMimeStreamFilter *) filtered, dos2unix);
		g_object_unref (dos2unix);
	}
	
	g_mime_stream_filter_add ((GMimeStreamFilter *) filtered, filter);
	
	if (inc) {
		onebyte = test_stream_onebyte_new (filtered);
		g_object_unref (filtered);
	} else {
		onebyte = filtered;
	}
	
	stream = g_mime_stream_fs_open (path, O_RDONLY, 0644, NULL);
	g_mime_stream_write_to_stream (stream, onebyte);
	g_mime_stream_flush (onebyte);
	g_object_unref (onebyte);
	g_object_unref (stream);
}

static void
test_charset_conversion (const char *datadir, const char *base, const char *from, const char *to)
{
	const char *what = "GMimeFilterCharset";
	GByteArray *actual, *expected;
	GMimeStream *stream;
	GMimeFilter *filter;
	char *path, *name;
	
	testsuite_check ("%s (%s %s -> %s)", what, base, from, to);
	
	if (!(filter = g_mime_filter_charset_new (from, to))) {
		testsuite_check_failed ("%s failed: system does not support conversion from %s to %s", what, from, to);
		return;
	}
	
	actual = g_byte_array_new ();
	stream = g_mime_stream_mem_new_with_byte_array (actual);
	g_mime_stream_mem_set_owner ((GMimeStreamMem *) stream, FALSE);
	
	name = g_strdup_printf ("%s.%s.txt", base, from);
	path = g_build_filename (datadir, name, NULL);
	pump_data_through_filter (filter, path, stream, TRUE, TRUE);
	g_mime_filter_reset (filter);
	g_object_unref (stream);
	g_object_unref (filter);
	g_free (path);
	g_free (name);
	
	name = g_strdup_printf ("%s.%s.txt", base, to);
	path = g_build_filename (datadir, name, NULL);
	expected = read_all_bytes (path, TRUE);
	g_free (path);
	g_free (name);
	
	if (actual->len != expected->len) {
		testsuite_check_failed ("%s failed: stream lengths do not match: expected=%u; actual=%u",
					what, expected->len, actual->len);
		goto error;
	}
	
	if (memcmp (actual->data, expected->data, actual->len) != 0) {
		testsuite_check_failed ("%s failed: stream contents do not match", what);
		goto error;
	}
	
	testsuite_check_passed ();
	
error:
	
	g_byte_array_free (expected, TRUE);
	g_byte_array_free (actual, TRUE);
}

static void
test_enriched (const char *datadir, const char *input, const char *output)
{
	const char *what = "GMimeFilterEnriched";
	GByteArray *actual, *expected;
	GMimeStream *stream;
	GMimeFilter *filter;
	char *path;
	
	testsuite_check ("%s (%s)", what, input);
	
	actual = g_byte_array_new ();
	stream = g_mime_stream_mem_new_with_byte_array (actual);
	g_mime_stream_mem_set_owner ((GMimeStreamMem *) stream, FALSE);
	
	filter = g_mime_filter_enriched_new (0);
	
	path = g_build_filename (datadir, input, NULL);
	pump_data_through_filter (filter, path, stream, TRUE, TRUE);
	g_mime_filter_reset (filter);
	g_object_unref (stream);
	g_object_unref (filter);
	g_free (path);
	
	path = g_build_filename (datadir, output, NULL);
	expected = read_all_bytes (path, TRUE);
	g_free (path);
	
	if (actual->len != expected->len) {
		testsuite_check_failed ("%s failed: stream lengths do not match: expected=%u; actual=%u",
					what, expected->len, actual->len);
		printf ("enriched: -->%.*s<--\n", (int) actual->len, (char *) actual->data);
		goto error;
	}
	
	if (memcmp (actual->data, expected->data, actual->len) != 0) {
		testsuite_check_failed ("%s failed: stream contents do not match", what);
		goto error;
	}
	
	testsuite_check_passed ();
	
error:
	
	g_byte_array_free (expected, TRUE);
	g_byte_array_free (actual, TRUE);
}

static void
test_gzip (const char *datadir, const char *filename)
{
	char *name = g_strdup_printf ("%s.gz", filename);
	char *path = g_build_filename (datadir, filename, NULL);
	const char *what = "GMimeFilterGzip::zip";
	GByteArray *actual, *expected;
	GMimeStream *stream;
	GMimeFilter *filter;
	
	testsuite_check ("%s", what);
	
	actual = g_byte_array_new ();
	stream = g_mime_stream_mem_new_with_byte_array (actual);
	g_mime_stream_mem_set_owner ((GMimeStreamMem *) stream, FALSE);
	
	filter = g_mime_filter_gzip_new (GMIME_FILTER_GZIP_MODE_ZIP, 9);
	g_mime_filter_gzip_set_filename ((GMimeFilterGZip *) filter, filename);
	g_mime_filter_gzip_set_comment ((GMimeFilterGZip *) filter, "This is a comment.");
	
	pump_data_through_filter (filter, path, stream, TRUE, TRUE);
	g_mime_filter_reset (filter);
	g_object_unref (stream);
	g_object_unref (filter);
	g_free (path);
	
	path = g_build_filename (datadir, name, NULL);
	expected = read_all_bytes (path, FALSE);
	g_free (name);
	g_free (path);
	
	if (actual->len != 1233 && (actual->len != expected->len || memcmp (actual->data, expected->data, actual->len) != 0)) {
		if (actual->len != expected->len)
			testsuite_check_failed ("%s failed: streams are not the same length: %u", what, actual->len);
		else
			testsuite_check_failed ("%s failed: streams do not match", what);
		
		name = g_strdup_printf ("%s.1.gz", filename);
		path = g_build_filename (datadir, name, NULL);
		g_free (name);
		
		stream = g_mime_stream_fs_open (path, O_WRONLY | O_CREAT | O_TRUNC, 0644, NULL);
		g_free (path);
		
		g_mime_stream_write (stream, (const char *) actual->data, actual->len);
		g_mime_stream_flush (stream);
		g_object_unref (stream);
	} else {
		testsuite_check_passed ();
	}
	
	g_byte_array_free (expected, TRUE);
	g_byte_array_free (actual, TRUE);
}

static void
test_gunzip (const char *datadir, const char *filename)
{
	char *name = g_strdup_printf ("%s.gz", filename);
	char *path = g_build_filename (datadir, name, NULL);
	const char *what = "GMimeFilterGzip::unzip";
	GByteArray *actual, *expected;
	GMimeStream *stream;
	GMimeFilter *filter;
	const char *value;
	
	testsuite_check ("%s", what);
	
	actual = g_byte_array_new ();
	stream = g_mime_stream_mem_new_with_byte_array (actual);
	g_mime_stream_mem_set_owner ((GMimeStreamMem *) stream, FALSE);
	
	filter = g_mime_filter_gzip_new (GMIME_FILTER_GZIP_MODE_UNZIP, 9);
	
	pump_data_through_filter (filter, path, stream, FALSE, TRUE);
	g_object_unref (stream);
	g_free (path);
	g_free (name);
	
	path = g_build_filename (datadir, filename, NULL);
	expected = read_all_bytes (path, TRUE);
	g_free (path);
	
	if (actual->len != expected->len) {
		testsuite_check_failed ("%s failed: stream lengths do not match: expected=%u; actual=%u",
					what, expected->len, actual->len);
		goto error;
	}
	
	if (memcmp (actual->data, expected->data, actual->len) != 0) {
		testsuite_check_failed ("%s failed: stream contents do not match", what);
		goto error;
	}
	
	value = g_mime_filter_gzip_get_filename ((GMimeFilterGZip *) filter);
	if (!value || strcmp (value, filename) != 0) {
		testsuite_check_failed ("%s failed: filename does not match: %s", what, value);
		goto error;
	}
	
	value = g_mime_filter_gzip_get_comment ((GMimeFilterGZip *) filter);
	if (!value || strcmp (value, "This is a comment.") != 0) {
		testsuite_check_failed ("%s failed: comment does not match: %s", what, value);
		goto error;
	}
	
	testsuite_check_passed ();
	
error:
	
	g_byte_array_free (expected, TRUE);
	g_byte_array_free (actual, TRUE);
	
	g_mime_filter_reset (filter);
	g_object_unref (filter);
}

static void
test_html (const char *datadir, const char *input, const char *output, guint32 citation)
{
	guint32 flags = GMIME_FILTER_HTML_CONVERT_NL | GMIME_FILTER_HTML_CONVERT_SPACES |
		GMIME_FILTER_HTML_CONVERT_URLS | GMIME_FILTER_HTML_CONVERT_ADDRESSES;
	const char *what = "GMimeFilterHtml", *mode;
	GByteArray *expected, *actual;
	GMimeStream *stream;
	GMimeFilter *filter;
	char *path;
	
	switch (citation) {
	case GMIME_FILTER_HTML_BLOCKQUOTE_CITATION: mode = "blockquote"; break;
	case GMIME_FILTER_HTML_MARK_CITATION: mode = "mark"; break;
	case GMIME_FILTER_HTML_CITE: mode = "cite"; break;
	default: mode = "none"; break;
	}
	
	testsuite_check ("%s (%s %s)", what, input, mode);
	
	actual = g_byte_array_new ();
	stream = g_mime_stream_mem_new_with_byte_array (actual);
	g_mime_stream_mem_set_owner ((GMimeStreamMem *) stream, FALSE);
	
	filter = g_mime_filter_html_new (flags | citation, 0x008888);
	
	path = g_build_filename (datadir, input, NULL);
	pump_data_through_filter (filter, path, stream, TRUE, TRUE);
	g_mime_filter_reset (filter);
	g_object_unref (stream);
	g_object_unref (filter);
	g_free (path);
	
	path = g_build_filename (datadir, output, NULL);
	expected = read_all_bytes (path, TRUE);
	g_free (path);
	
	if (actual->len != expected->len) {
		testsuite_check_failed ("%s failed: stream lengths do not match: expected=%u; actual=%u",
					what, expected->len, actual->len);
		stream = g_mime_stream_fs_open (output, O_WRONLY | O_CREAT, 0644, NULL);
		g_mime_stream_write (stream, (const char *) actual->data, actual->len);
		g_mime_stream_flush (stream);
		g_object_unref (stream);
		goto error;
	}
	
	if (memcmp (actual->data, expected->data, actual->len) != 0) {
		testsuite_check_failed ("%s failed: stream contents do not match", what);
		goto error;
	}
	
	testsuite_check_passed ();
	
error:
	
	g_byte_array_free (expected, TRUE);
	g_byte_array_free (actual, TRUE);
}

static void
test_smtp_data (const char *datadir, const char *input, const char *output)
{
	const char *what = "GMimeFilterSmtpData";
	GByteArray *expected, *actual;
	GMimeStream *stream;
	GMimeFilter *filter;
	char *path;
	
	testsuite_check ("%s", what);
	
	actual = g_byte_array_new ();
	stream = g_mime_stream_mem_new_with_byte_array (actual);
	g_mime_stream_mem_set_owner ((GMimeStreamMem *) stream, FALSE);
	
	filter = g_mime_filter_smtp_data_new ();
	
	path = g_build_filename (datadir, input, NULL);
	pump_data_through_filter (filter, path, stream, TRUE, TRUE);
	g_mime_filter_reset (filter);
	g_object_unref (stream);
	g_object_unref (filter);
	g_free (path);
	
	path = g_build_filename (datadir, output, NULL);
	expected = read_all_bytes (path, TRUE);
	g_free (path);
	
	if (actual->len != expected->len) {
		testsuite_check_failed ("%s failed: stream lengths do not match: expected=%u; actual=%u",
					what, expected->len, actual->len);
		goto error;
	}
	
	if (memcmp (actual->data, expected->data, actual->len) != 0) {
		testsuite_check_failed ("%s failed: stream contents do not match", what);
		goto error;
	}
	
	testsuite_check_passed ();
	
error:
	
	g_byte_array_free (expected, TRUE);
	g_byte_array_free (actual, TRUE);
}

static void
test_windows (const char *datadir, const char *filename, const char *claimed, const char *expected)
{
	const char *what = "GMimeFilterWindows";
	GMimeStream *stream;
	GMimeFilter *filter;
	const char *actual;
	char *path;
	
	testsuite_check ("%s", what);
	
	filter = g_mime_filter_windows_new (claimed);
	
	stream = g_mime_stream_null_new ();
	path = g_build_filename (datadir, filename, NULL);
	pump_data_through_filter (filter, path, stream, TRUE, TRUE);
	g_object_unref (stream);
	g_free (path);
	
	actual = g_mime_filter_windows_real_charset ((GMimeFilterWindows *) filter);
	
	if (strcmp (expected, actual) != 0) {
		testsuite_check_failed ("%s failed: charsets do not match: expected=%s; actual=%s",
					what, expected, actual);
		goto error;
	}
	
	if (!g_mime_filter_windows_is_windows_charset ((GMimeFilterWindows *) filter)) {
		testsuite_check_failed ("%s failed: is_windows_charset returned FALSE", what);
		goto error;
	}
	
	testsuite_check_passed ();
	
error:
	
	g_mime_filter_reset (filter);
	g_object_unref (filter);
}

static gboolean
filter_chunk (GMimeFilter *filter, const guint8 *input, size_t length, gboolean complete, GByteArray *result)
{
	size_t outlen, outprespace;
	char *buffer, *output;
	gboolean passthrough;
	size_t i;

	buffer = g_malloc (length + 32);
	memset (buffer, 'Z', length + 32);
	if (length)
		memcpy (buffer + 16, input, length);
	if (complete)
		g_mime_filter_complete (filter, buffer + 16, length, 16, &output, &outlen, &outprespace);
	else
		g_mime_filter_filter (filter, buffer + 16, length, 16, &output, &outlen, &outprespace);
	if (outlen)
		g_byte_array_append (result, (const guint8 *) output, outlen);
	if (length)
		g_assert_cmpint (memcmp (buffer + 16, input, length), ==, 0);
	passthrough = output == buffer + 16;
	if (passthrough) {
		g_assert_cmpuint (outlen, ==, length);
		g_assert_cmpuint (outprespace, ==, 16);
	} else {
		g_assert_true (output == filter->outbuf);
		g_assert_cmpuint (outprespace, ==, filter->outpre);
	}
	for (i = 0; i < 16; i++) {
		g_assert_cmpint (buffer[i], ==, 'Z');
		g_assert_cmpint (buffer[length + 16 + i], ==, 'Z');
	}
	g_free (buffer);
	return passthrough;
}

static void
strip_chunk (GMimeFilter *filter, const guint8 *input, size_t length, gboolean complete, GByteArray *result)
{
	gboolean passthrough = filter_chunk (filter, input, length, complete, result);

	g_assert_cmpint (passthrough, ==, length == 0);
}

static GByteArray *
strip_expected (const guint8 *input, size_t length)
{
	GByteArray *expected = g_byte_array_new ();
	size_t i = 0, start;

	while (i < length) {
		start = i;
		if (input[i] == ' ' || input[i] == '\t') {
			do {
				i++;
			} while (i < length && (input[i] == ' ' || input[i] == '\t'));
			if (i < length && input[i] != '\r' && input[i] != '\n')
				g_byte_array_append (expected, input + start, i - start);
		} else {
			g_byte_array_append (expected, input + i++, 1);
		}
	}
	return expected;
}

static void
check_filter_result (GByteArray *actual, GByteArray *expected)
{
	g_assert_cmpuint (actual->len, ==, expected->len);
	if (expected->len)
		g_assert_cmpint (memcmp (actual->data, expected->data, expected->len), ==, 0);
}

static void
check_strip_splits (const guint8 *input, size_t length)
{
	GByteArray *expected = strip_expected (input, length);
	GByteArray *actual = g_byte_array_new ();
	GMimeFilter *filter = g_mime_filter_strip_new ();
	size_t split;
	guint mode;

	for (split = 0; split <= length; split++) {
		for (mode = 0; mode < 2; mode++) {
			g_mime_filter_reset (filter);
			g_byte_array_set_size (actual, 0);
			strip_chunk (filter, input, split, FALSE, actual);
			strip_chunk (filter, input, 0, FALSE, actual);
			strip_chunk (filter, input + split, length - split, mode == 0, actual);
			if (mode)
				strip_chunk (filter, input, 0, TRUE, actual);
			check_filter_result (actual, expected);
		}
	}
	g_object_unref (filter);
	g_byte_array_unref (expected);
	g_byte_array_unref (actual);
}

static void
test_strip_streaming (void)
{
	static const guint8 alphabet[] = { ' ', '\t', '\r', '\n', 'a', 0, 255 };
	static const guint8 special[] = " \t\v\f\302\240\342\200\203 \tb \r\n\t \377 \t";
	guint8 input[256];
	guint count = 1, code, value, i, length;

	testsuite_check ("GMimeFilterStrip streaming, final buffers and byte classification");
	for (length = 0; length <= 4; length++) {
		for (code = 0; code < count; code++) {
			value = code;
			for (i = 0; i < length; i++) {
				input[i] = alphabet[value % G_N_ELEMENTS (alphabet)];
				value /= G_N_ELEMENTS (alphabet);
			}
			check_strip_splits (input, length);
		}
		count *= G_N_ELEMENTS (alphabet);
	}
	for (i = 0; i < sizeof (input); i++)
		input[i] = i;
	check_strip_splits (input, sizeof (input));
	check_strip_splits (special, sizeof (special) - 1);
	testsuite_check_passed ();
}

static void
test_strip_long_runs (void)
{
	static const size_t sizes[] = { 1, 2, 3, 63, 64, 65, 254, 255, 256, 257, 4095, 4096 };
	GByteArray *input = g_byte_array_new ();
	GByteArray *actual = g_byte_array_new ();
	GByteArray *expected;
	GMimeFilter *filter = g_mime_filter_strip_new ();
	guint8 c;
	size_t i, j, n, offset;

	testsuite_check ("GMimeFilterStrip packed whitespace runs and output expansion");
	for (i = 0; i < 66000; i++) {
		c = ' ';
		g_byte_array_append (input, &c, 1);
	}
	for (i = 0; i < 66000; i++) {
		c = '\t';
		g_byte_array_append (input, &c, 1);
	}
	for (i = 0; i < 8193; i++) {
		c = i % 2 ? ' ' : '\t';
		g_byte_array_append (input, &c, 1);
	}
	g_byte_array_append (input, (const guint8 *) "text \t\r \t\nlast \t", 16);
	expected = strip_expected (input->data, input->len);
	for (i = 0; i < G_N_ELEMENTS (sizes); i++) {
		for (j = 0; j < 2; j++) {
			g_mime_filter_reset (filter);
			g_byte_array_set_size (actual, 0);
			offset = 0;
			while (offset < input->len) {
				n = MIN (sizes[i], input->len - offset);
				strip_chunk (filter, input->data + offset, n, j == 0 && offset + n == input->len, actual);
				offset += n;
			}
			if (j)
				strip_chunk (filter, input->data, 0, TRUE, actual);
			check_filter_result (actual, expected);
		}
	}
	g_object_unref (filter);
	g_byte_array_unref (expected);
	g_byte_array_unref (actual);
	g_byte_array_unref (input);
	testsuite_check_passed ();
}

static void
test_strip_lifecycle (void)
{
	GMimeFilter *filter = g_mime_filter_strip_new ();
	GMimeFilter *copy;
	GByteArray *actual = g_byte_array_new ();

	testsuite_check ("GMimeFilterStrip reset, copy and completion state");
	strip_chunk (filter, (const guint8 *) "a \t", 3, FALSE, actual);
	g_assert_cmpuint (actual->len, ==, 1);
	copy = g_mime_filter_copy (filter);
	g_assert_true (GMIME_IS_FILTER_STRIP (copy));
	g_byte_array_set_size (actual, 0);
	strip_chunk (copy, (const guint8 *) "b", 1, TRUE, actual);
	g_assert_cmpuint (actual->len, ==, 1);
	g_assert_cmpint (actual->data[0], ==, 'b');
	g_byte_array_set_size (actual, 0);
	strip_chunk (filter, (const guint8 *) "b \t", 3, TRUE, actual);
	g_assert_cmpuint (actual->len, ==, 3);
	g_assert_cmpint (memcmp (actual->data, " \tb", 3), ==, 0);
	g_byte_array_set_size (actual, 0);
	strip_chunk (filter, (const guint8 *) "c", 1, TRUE, actual);
	g_assert_cmpuint (actual->len, ==, 1);
	g_assert_cmpint (actual->data[0], ==, 'c');
	g_byte_array_set_size (actual, 0);
	strip_chunk (filter, (const guint8 *) " \t", 2, FALSE, actual);
	g_mime_filter_reset (filter);
	strip_chunk (filter, (const guint8 *) "d", 1, TRUE, actual);
	g_assert_cmpuint (actual->len, ==, 1);
	g_assert_cmpint (actual->data[0], ==, 'd');
	g_byte_array_set_size (actual, 0);
	strip_chunk (filter, (const guint8 *) " \t", 2, FALSE, actual);
	strip_chunk (filter, (const guint8 *) "", 0, TRUE, actual);
	strip_chunk (filter, (const guint8 *) "e", 1, TRUE, actual);
	g_assert_cmpuint (actual->len, ==, 1);
	g_assert_cmpint (actual->data[0], ==, 'e');
	g_byte_array_unref (actual);
	g_object_unref (copy);
	g_object_unref (filter);
	testsuite_check_passed ();
}

static void
test_strip_streams (void)
{
	static const char input[] = "foo \tbar\t\r\nbaz \t";
	static const char expected[] = "foo \tbar\r\nbaz";
	GMimeStream *memory, *filtered;
	GMimeFilter *filter;
	GByteArray *actual;
	char buffer[3];
	ssize_t n;
	size_t offset;

	testsuite_check ("GMimeFilterStrip filtered stream reads and writes");
	memory = g_mime_stream_mem_new ();
	filtered = g_mime_stream_filter_new (memory);
	filter = g_mime_filter_strip_new ();
	g_mime_stream_filter_add (GMIME_STREAM_FILTER (filtered), filter);
	g_object_unref (filter);
	for (offset = 0; offset < sizeof (input) - 1; offset++)
		g_assert_cmpint (g_mime_stream_write (filtered, input + offset, 1), ==, 1);
	g_assert_cmpint (g_mime_stream_flush (filtered), ==, 0);
	actual = g_mime_stream_mem_get_byte_array (GMIME_STREAM_MEM (memory));
	g_assert_cmpuint (actual->len, ==, sizeof (expected) - 1);
	g_assert_cmpint (memcmp (actual->data, expected, actual->len), ==, 0);
	g_object_unref (filtered);
	g_object_unref (memory);
	memory = g_mime_stream_mem_new_with_buffer (input, sizeof (input) - 1);
	filtered = g_mime_stream_filter_new (memory);
	filter = g_mime_filter_strip_new ();
	g_mime_stream_filter_add (GMIME_STREAM_FILTER (filtered), filter);
	g_object_unref (filter);
	actual = g_byte_array_new ();
	while ((n = g_mime_stream_read (filtered, buffer, sizeof (buffer))) > 0)
		g_byte_array_append (actual, (const guint8 *) buffer, n);
	g_assert_cmpint (n, ==, 0);
	g_assert_cmpuint (actual->len, ==, sizeof (expected) - 1);
	g_assert_cmpint (memcmp (actual->data, expected, actual->len), ==, 0);
	g_byte_array_unref (actual);
	g_object_unref (filtered);
	g_object_unref (memory);
	testsuite_check_passed ();
}

static GMimeFilter *
newline_filter_new (gboolean to_dos, gboolean ensure_newline)
{
	return to_dos ? g_mime_filter_unix2dos_new (ensure_newline) : g_mime_filter_dos2unix_new (ensure_newline);
}

static GByteArray *
newline_expected (const guint8 *input, size_t length, gboolean to_dos, gboolean ensure_newline)
{
	GByteArray *expected = g_byte_array_new ();
	guint8 c, cr = '\r', lf = '\n';
	size_t i;

	/* Whole-buffer lookahead is independent of the filters' streaming state. */
	for (i = 0; i < length; i++) {
		c = input[i];
		if (to_dos) {
			if (c == '\n' && (i == 0 || input[i - 1] != '\r'))
				g_byte_array_append (expected, &cr, 1);
		} else if (c == '\r') {
			if (i + 1 < length && input[i + 1] == '\n')
				continue;
			if (ensure_newline && i + 1 == length)
				c = '\n';
		}
		g_byte_array_append (expected, &c, 1);
	}
	if (ensure_newline && (!expected->len || expected->data[expected->len - 1] != '\n')) {
		if (to_dos && (!expected->len || expected->data[expected->len - 1] != '\r'))
			g_byte_array_append (expected, &cr, 1);
		g_byte_array_append (expected, &lf, 1);
	}
	return expected;
}

static void
check_newline_splits (const guint8 *input, size_t length)
{
	GByteArray *expected, *actual = g_byte_array_new ();
	GMimeFilter *filter;
	size_t split;
	guint to_dos, ensure, mode;

	for (to_dos = 0; to_dos < 2; to_dos++) {
		for (ensure = 0; ensure < 2; ensure++) {
			filter = newline_filter_new (to_dos, ensure);
			expected = newline_expected (input, length, to_dos, ensure);
			for (split = 0; split <= length; split++) {
				for (mode = 0; mode < 2; mode++) {
					g_mime_filter_reset (filter);
					g_byte_array_set_size (actual, 0);
					filter_chunk (filter, input, split, FALSE, actual);
					filter_chunk (filter, input, 0, FALSE, actual);
					filter_chunk (filter, input + split, length - split, mode == 0, actual);
					if (mode)
						filter_chunk (filter, input, 0, TRUE, actual);
					check_filter_result (actual, expected);
					filter_chunk (filter, input, 0, TRUE, actual);
					filter_chunk (filter, input, 0, TRUE, actual);
					check_filter_result (actual, expected);
				}
			}
			g_byte_array_unref (expected);
			g_object_unref (filter);
		}
	}
	g_byte_array_unref (actual);
}

static void
test_newline_streaming (void)
{
	static const guint8 alphabet[] = { '\r', '\n', 'a', 0, 255 };
	guint8 input[256];
	guint count = 1, code, value, length, i;

	testsuite_check ("Newline filters split CRLF, trailing CR and repeated completion");
	for (length = 0; length <= 4; length++) {
		for (code = 0; code < count; code++) {
			value = code;
			for (i = 0; i < length; i++) {
				input[i] = alphabet[value % G_N_ELEMENTS (alphabet)];
				value /= G_N_ELEMENTS (alphabet);
			}
			check_newline_splits (input, length);
		}
		count *= G_N_ELEMENTS (alphabet);
	}
	for (i = 0; i < sizeof (input); i++)
		input[i] = i;
	check_newline_splits (input, sizeof (input));
	testsuite_check_passed ();
}

static void
test_newline_long_runs (void)
{
	static const size_t sizes[] = { 1, 2, 3, 63, 64, 255, 256, 4095, 4096, 4097 };
	guint8 input[16385];
	GByteArray *actual = g_byte_array_new (), *expected;
	GMimeFilter *filter;
	size_t i, offset, n;
	guint to_dos, ensure, mode;

	testsuite_check ("Newline filters long CR/LF runs and output expansion");
	memset (input, '\n', 8193);
	memset (input + 8193, '\r', sizeof (input) - 8193);
	for (to_dos = 0; to_dos < 2; to_dos++) {
		for (ensure = 0; ensure < 2; ensure++) {
			filter = newline_filter_new (to_dos, ensure);
			expected = newline_expected (input, sizeof (input), to_dos, ensure);
			for (i = 0; i < G_N_ELEMENTS (sizes); i++) {
				for (mode = 0; mode < 2; mode++) {
					g_mime_filter_reset (filter);
					g_byte_array_set_size (actual, 0);
					for (offset = 0; offset < sizeof (input); offset += n) {
						n = MIN (sizes[i], sizeof (input) - offset);
						filter_chunk (filter, input + offset, n,
							      mode == 0 && offset + n == sizeof (input), actual);
					}
					if (mode)
						filter_chunk (filter, input, 0, TRUE, actual);
					filter_chunk (filter, input, 0, TRUE, actual);
					check_filter_result (actual, expected);
				}
			}
			g_byte_array_unref (expected);
			g_object_unref (filter);
		}
	}
	g_byte_array_unref (actual);
	testsuite_check_passed ();
}

static void
test_newline_lifecycle (void)
{
	GByteArray *actual = g_byte_array_new (), *expected;
	GMimeFilter *filter, *copy;
	guint to_dos, ensure;

	testsuite_check ("Newline filters fresh copies, reset and reuse after completion");
	for (to_dos = 0; to_dos < 2; to_dos++) {
		for (ensure = 0; ensure < 2; ensure++) {
			filter = newline_filter_new (to_dos, ensure);
			g_byte_array_set_size (actual, 0);
			filter_chunk (filter, (const guint8 *) "\r", 1, FALSE, actual);
			copy = g_mime_filter_copy (filter);
			g_assert_true (G_OBJECT_TYPE (copy) == G_OBJECT_TYPE (filter));
			g_byte_array_set_size (actual, 0);
			filter_chunk (copy, (const guint8 *) "\n", 1, TRUE, actual);
			expected = newline_expected ((const guint8 *) "\n", 1, to_dos, ensure);
			check_filter_result (actual, expected);
			g_byte_array_unref (expected);
			g_byte_array_set_size (actual, 0);
			filter_chunk (copy, (const guint8 *) "q", 1, TRUE, actual);
			expected = newline_expected ((const guint8 *) "q", 1, to_dos, ensure);
			check_filter_result (actual, expected);
			g_byte_array_unref (expected);
			g_object_unref (copy);
			g_byte_array_set_size (actual, 0);
			filter_chunk (filter, (const guint8 *) "\n", 1, TRUE, actual);
			g_assert_cmpuint (actual->len, ==, 1);
			g_assert_cmpint (actual->data[0], ==, '\n');
			filter_chunk (filter, (const guint8 *) "\r", 1, FALSE, actual);
			g_mime_filter_reset (filter);
			g_byte_array_set_size (actual, 0);
			filter_chunk (filter, (const guint8 *) "\n", 1, TRUE, actual);
			expected = newline_expected ((const guint8 *) "\n", 1, to_dos, ensure);
			check_filter_result (actual, expected);
			g_byte_array_unref (expected);
			g_mime_filter_reset (filter);
			g_byte_array_set_size (actual, 0);
			filter_chunk (filter, (const guint8 *) "\r", 1, TRUE, actual);
			expected = newline_expected ((const guint8 *) "\r", 1, to_dos, ensure);
			check_filter_result (actual, expected);
			g_byte_array_unref (expected);
			g_byte_array_set_size (actual, 0);
			filter_chunk (filter, (const guint8 *) "q", 1, TRUE, actual);
			expected = newline_expected ((const guint8 *) "q", 1, to_dos, ensure);
			check_filter_result (actual, expected);
			g_byte_array_unref (expected);
			g_object_unref (filter);
		}
	}
	g_byte_array_unref (actual);
	testsuite_check_passed ();
}

static void
test_newline_streams (void)
{
	static const char input[] = "a\r\nb\rc\n\r\r\n\0\377last\r";
	GMimeStream *memory, *filtered;
	GMimeFilter *filter;
	GByteArray *actual, *expected;
	char buffer[3];
	ssize_t n;
	size_t offset;
	guint to_dos, ensure;

	testsuite_check ("Newline filters binary stream reads, writes and repeated flush");
	for (to_dos = 0; to_dos < 2; to_dos++) {
		for (ensure = 0; ensure < 2; ensure++) {
			expected = newline_expected ((const guint8 *) input, sizeof (input) - 1, to_dos, ensure);
			memory = g_mime_stream_mem_new ();
			filtered = g_mime_stream_filter_new (memory);
			filter = newline_filter_new (to_dos, ensure);
			g_mime_stream_filter_add (GMIME_STREAM_FILTER (filtered), filter);
			g_object_unref (filter);
			for (offset = 0; offset < sizeof (input) - 1; offset++)
				g_assert_cmpint (g_mime_stream_write (filtered, input + offset, 1), ==, 1);
			g_assert_cmpint (g_mime_stream_flush (filtered), ==, 0);
			g_assert_cmpint (g_mime_stream_flush (filtered), ==, 0);
			actual = g_mime_stream_mem_get_byte_array (GMIME_STREAM_MEM (memory));
			check_filter_result (actual, expected);
			g_object_unref (filtered);
			g_object_unref (memory);
			memory = g_mime_stream_mem_new_with_buffer (input, sizeof (input) - 1);
			filtered = g_mime_stream_filter_new (memory);
			filter = newline_filter_new (to_dos, ensure);
			g_mime_stream_filter_add (GMIME_STREAM_FILTER (filtered), filter);
			g_object_unref (filter);
			actual = g_byte_array_new ();
			while ((n = g_mime_stream_read (filtered, buffer, sizeof (buffer))) > 0)
				g_byte_array_append (actual, (const guint8 *) buffer, n);
			g_assert_cmpint (n, ==, 0);
			check_filter_result (actual, expected);
			g_assert_cmpint (g_mime_stream_read (filtered, buffer, sizeof (buffer)), ==, 0);
			g_byte_array_unref (actual);
			g_byte_array_unref (expected);
			g_object_unref (filtered);
			g_object_unref (memory);
		}
	}
	testsuite_check_passed ();
}

int main (int argc, char **argv)
{
	const char *datadir = "data/filters";
	struct stat st;
	int i;
	
	g_mime_init ();
	
	testsuite_init (argc, argv);
	
	//verbose = 4;
	
	for (i = 1; i < argc; i++) {
		if (argv[i][0] != '-') {
			datadir = argv[i];
			break;
		}
	}
	
	if (i < argc && (stat (datadir, &st) == -1 || !S_ISDIR (st.st_mode)))
		return 0;
	
	testsuite_start ("GMimeFilter");

	test_strip_streaming ();
	test_strip_long_runs ();
	test_strip_lifecycle ();
	test_strip_streams ();
	test_newline_streaming ();
	test_newline_long_runs ();
	test_newline_lifecycle ();
	test_newline_streams ();
	
	//test_charset_conversion (datadir, "chinese", "utf-8", "big5"); // Note: utf-8 -> big5 drops characters
	test_charset_conversion (datadir, "cyrillic", "utf-8", "cp1251");
	test_charset_conversion (datadir, "cyrillic", "cp1251", "utf-8");
	test_charset_conversion (datadir, "cyrillic", "utf-8", "iso-8859-5");
	test_charset_conversion (datadir, "cyrillic", "iso-8859-5", "utf-8");
	test_charset_conversion (datadir, "cyrillic", "utf-8", "koi8-r");
	test_charset_conversion (datadir, "cyrillic", "koi8-r", "utf-8");
	test_charset_conversion (datadir, "japanese", "utf-8", "iso-2022-jp");
	test_charset_conversion (datadir, "japanese", "iso-2022-jp", "utf-8");
	test_charset_conversion (datadir, "japanese", "utf-8", "shift-jis");
	test_charset_conversion (datadir, "japanese", "shift-jis", "utf-8");
	
	test_enriched (datadir, "enriched.txt", "enriched.html");
	
	test_gzip (datadir, "lorem-ipsum.txt");
	test_gunzip (datadir, "lorem-ipsum.txt");
	
	test_html (datadir, "html-input.txt", "html-output.blockquote.html", GMIME_FILTER_HTML_BLOCKQUOTE_CITATION);
	test_html (datadir, "html-input.txt", "html-output.mark.html", GMIME_FILTER_HTML_MARK_CITATION);
	test_html (datadir, "html-input.txt", "html-output.cite.html", GMIME_FILTER_HTML_CITE);
	
	test_smtp_data (datadir, "smtp-input.txt", "smtp-output.txt");
	
	test_windows (datadir, "french-fable.cp1252.txt", "iso-8859-1", "windows-cp1252");
	
	testsuite_end ();
	
	g_mime_shutdown ();
	
	return testsuite_exit ();
}
