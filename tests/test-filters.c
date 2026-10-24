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
	guint8 preload[16];
	gboolean passthrough;
	size_t backedup = filter->backlen;
	size_t i;

	g_assert_cmpuint (backedup, <=, sizeof (preload));
	if (backedup)
		memcpy (preload, filter->backbuf, backedup);
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
	passthrough = output == buffer + 16 - backedup;
	if (passthrough) {
		g_assert_cmpuint (outlen, <=, length + backedup);
		g_assert_cmpuint (outprespace, ==, 16 - backedup);
	} else {
		g_assert_true (output == filter->outbuf);
		g_assert_cmpuint (outprespace, ==, filter->outpre);
	}
	for (i = 0; i < 16; i++) {
		if (i < 16 - backedup)
			g_assert_cmpint (buffer[i], ==, 'Z');
		g_assert_cmpint (buffer[length + 16 + i], ==, 'Z');
	}
	/* The filter framework may preload saved input into the caller's prespace. */
	if (backedup)
		g_assert_cmpint (memcmp (buffer + 16 - backedup, preload, backedup), ==, 0);
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

static GByteArray *
from_expected (const guint8 *input, size_t length, GMimeFilterFromMode mode)
{
	GByteArray *expected = g_byte_array_new ();
	size_t i;

	for (i = 0; i < length; i++) {
		if ((i == 0 || input[i - 1] == '\n') && length - i >= 5 &&
		    memcmp (input + i, "From ", 5) == 0) {
			if (mode == GMIME_FILTER_FROM_MODE_ARMOR) {
				g_byte_array_append (expected, (const guint8 *) "=46", 3);
				continue;
			}
			g_byte_array_append (expected, (const guint8 *) ">", 1);
		}
		g_byte_array_append (expected, input + i, 1);
	}
	return expected;
}

static void
check_from_splits (const guint8 *input, size_t length)
{
	GByteArray *actual = g_byte_array_new (), *expected;
	GMimeFilter *filter;
	size_t split;
	guint mode, final;

	for (mode = 0; mode < 2; mode++) {
		filter = g_mime_filter_from_new (mode);
		expected = from_expected (input, length, mode);
		for (split = 0; split <= length; split++) {
			for (final = 0; final < 2; final++) {
				g_mime_filter_reset (filter);
				g_byte_array_set_size (actual, 0);
				filter_chunk (filter, input, split, FALSE, actual);
				filter_chunk (filter, input, 0, FALSE, actual);
				filter_chunk (filter, input + split, length - split, final, actual);
				if (!final)
					filter_chunk (filter, input, 0, TRUE, actual);
				check_filter_result (actual, expected);
				filter_chunk (filter, input, 0, TRUE, actual);
				check_filter_result (actual, expected);
				g_assert_cmpuint (filter->backlen, ==, 0);
			}
		}
		g_byte_array_unref (expected);
		g_object_unref (filter);
	}
	g_byte_array_unref (actual);
}

static void
test_from_streaming (void)
{
	static const guint8 alphabet[] = { 'F', 'r', 'o', 'm', ' ', '\n', 'x' };
	static const char *cases[] = {
		"From ", "From\n", "From\t", "From\r", "From \nFro",
		"From From \nFrom \nFr", "xFrom \nFrom ", "\rFrom \nFrom ",
		">From \n>>From ", "from \nfRom \nFROM ", "From \rFrom \r\nFrom "
	};
	static const guint8 binary[] = "From \0From \nF\0\nFrom \377\nFro";
	guint8 input[256];
	guint count = 1, code, value, length, i;

	testsuite_check ("From filters partial prefixes, final buffers and byte classification");
	for (length = 0; length <= 4; length++) {
		for (code = 0; code < count; code++) {
			value = code;
			for (i = 0; i < length; i++) {
				input[i] = alphabet[value % G_N_ELEMENTS (alphabet)];
				value /= G_N_ELEMENTS (alphabet);
			}
			check_from_splits (input, length);
		}
		count *= G_N_ELEMENTS (alphabet);
	}
	for (i = 0; i < G_N_ELEMENTS (cases); i++)
		check_from_splits ((const guint8 *) cases[i], strlen (cases[i]));
	for (i = 0; i < sizeof (input); i++)
		input[i] = i;
	check_from_splits (input, sizeof (input));
	check_from_splits (binary, sizeof (binary) - 1);
	testsuite_check_passed ();
}

static void
test_from_lifecycle (void)
{
	GByteArray *actual = g_byte_array_new (), *expected;
	GMimeFilter *filter, *copy;
	guint mode;

	testsuite_check ("From filters immediate mismatches, fresh copies, reset and reuse");
	for (mode = 0; mode < 2; mode++) {
		filter = g_mime_filter_from_new (mode);
		g_byte_array_set_size (actual, 0);
		g_assert_true (filter_chunk (filter, (const guint8 *) "Fx", 2, FALSE, actual));
		g_assert_cmpuint (actual->len, ==, 2);
		g_assert_cmpint (memcmp (actual->data, "Fx", 2), ==, 0);
		g_assert_cmpuint (filter->backlen, ==, 0);
		g_mime_filter_reset (filter);
		g_byte_array_set_size (actual, 0);
		filter_chunk (filter, (const guint8 *) "Fr", 2, FALSE, actual);
		g_assert_cmpuint (actual->len, ==, 0);
		g_assert_cmpuint (filter->backlen, ==, 2);
		copy = g_mime_filter_copy (filter);
		g_assert_true (GMIME_IS_FILTER_FROM (copy));
		filter_chunk (copy, (const guint8 *) "From ", 5, TRUE, actual);
		expected = from_expected ((const guint8 *) "From ", 5, mode);
		check_filter_result (actual, expected);
		g_byte_array_set_size (actual, 0);
		filter_chunk (filter, (const guint8 *) "om ", 3, TRUE, actual);
		check_filter_result (actual, expected);
		g_byte_array_unref (expected);
		g_object_unref (copy);
		g_byte_array_set_size (actual, 0);
		filter_chunk (filter, (const guint8 *) "From ", 5, TRUE, actual);
		g_assert_cmpuint (actual->len, ==, 5);
		g_assert_cmpint (memcmp (actual->data, "From ", 5), ==, 0);
		copy = g_mime_filter_copy (filter);
		g_byte_array_set_size (actual, 0);
		filter_chunk (copy, (const guint8 *) "From ", 5, TRUE, actual);
		expected = from_expected ((const guint8 *) "From ", 5, mode);
		check_filter_result (actual, expected);
		g_byte_array_unref (expected);
		g_object_unref (copy);
		g_mime_filter_reset (filter);
		g_byte_array_set_size (actual, 0);
		filter_chunk (filter, (const guint8 *) "Fr", 2, FALSE, actual);
		g_mime_filter_reset (filter);
		filter_chunk (filter, (const guint8 *) "om ", 3, TRUE, actual);
		g_assert_cmpuint (actual->len, ==, 3);
		g_assert_cmpint (memcmp (actual->data, "om ", 3), ==, 0);
		g_object_unref (filter);
	}
	filter = g_mime_filter_from_new ((GMimeFilterFromMode) -1);
	g_assert_cmpint (GMIME_FILTER_FROM (filter)->mode, ==, GMIME_FILTER_FROM_MODE_ESCAPE);
	g_byte_array_set_size (actual, 0);
	filter_chunk (filter, (const guint8 *) "From ", 5, TRUE, actual);
	expected = from_expected ((const guint8 *) "From ", 5, GMIME_FILTER_FROM_MODE_ESCAPE);
	check_filter_result (actual, expected);
	g_byte_array_unref (expected);
	g_object_unref (filter);
	g_byte_array_unref (actual);
	testsuite_check_passed ();
}

static void
test_from_prespace (void)
{
	static const char *suffixes[] = { "om ", "x", "" };
	GByteArray *actual = g_byte_array_new (), *expected;
	GMimeFilter *filter;
	size_t i, length, outlen, outprespace;
	char buffer[32], *output, combined[8];
	guint mode, suffix;

	testsuite_check ("From filters backed-up input without caller prespace");
	for (mode = 0; mode < 2; mode++) {
		filter = g_mime_filter_from_new (mode);
		for (suffix = 0; suffix < G_N_ELEMENTS (suffixes); suffix++) {
			g_mime_filter_reset (filter);
			g_byte_array_set_size (actual, 0);
			filter_chunk (filter, (const guint8 *) "Fr", 2, FALSE, actual);
			length = strlen (suffixes[suffix]);
			memset (buffer, 'Z', sizeof (buffer));
			memcpy (buffer + 4, suffixes[suffix], length);
			g_mime_filter_complete (filter, buffer + 4, length, 0, &output, &outlen, &outprespace);
			g_assert_true (output != buffer + 4);
			if (outlen)
				g_byte_array_append (actual, (const guint8 *) output, outlen);
			memcpy (combined, "Fr", 2);
			memcpy (combined + 2, suffixes[suffix], length);
			expected = from_expected ((const guint8 *) combined, length + 2, mode);
			check_filter_result (actual, expected);
			for (i = 0; i < sizeof (buffer); i++) {
				if (i < 4 || i >= length + 4)
					g_assert_cmpint (buffer[i], ==, 'Z');
			}
			if (length)
				g_assert_cmpint (memcmp (buffer + 4, suffixes[suffix], length), ==, 0);
			g_byte_array_unref (expected);
		}
		g_object_unref (filter);
	}
	g_byte_array_unref (actual);
	testsuite_check_passed ();
}

static void
test_from_many_lines (void)
{
	const guint count = 1024 * 1024;
	guint8 *input = g_malloc ((size_t) count * 6 + 3);
	GByteArray *actual = g_byte_array_new (), *expected;
	GMimeFilter *filter;
	const char *escaped;
	guint i, mode;
	size_t width;

	testsuite_check ("From filters a million markers and a buffered final prefix");
	for (i = 0; i < count; i++)
		memcpy (input + (size_t) i * 6, "From \n", 6);
	memcpy (input + (size_t) count * 6, "Fro", 3);
	for (mode = 0; mode < 2; mode++) {
		escaped = mode == GMIME_FILTER_FROM_MODE_ARMOR ? "=46rom \n" : ">From \n";
		width = strlen (escaped);
		expected = g_byte_array_sized_new (count * width + 3);
		for (i = 0; i < count; i++)
			g_byte_array_append (expected, (const guint8 *) escaped, width);
		filter = g_mime_filter_from_new (mode);
		g_byte_array_set_size (actual, 0);
		filter_chunk (filter, input, (size_t) count * 6 + 3, FALSE, actual);
		check_filter_result (actual, expected);
		g_assert_cmpuint (filter->backlen, ==, 3);
		filter_chunk (filter, input, 0, TRUE, actual);
		g_byte_array_append (expected, (const guint8 *) "Fro", 3);
		check_filter_result (actual, expected);
		g_byte_array_unref (expected);
		g_object_unref (filter);
	}
	g_byte_array_unref (actual);
	g_free (input);
	testsuite_check_passed ();
}

static void
test_from_quoted_printable (void)
{
	static const char input[] = "From alpha\nFrom =beta\001\377\nFrom final";
	GMimeStream *memory, *filtered;
	GMimeFilter *filter;
	GByteArray *encoded, *actual, *expected;

	testsuite_check ("From armor after quoted-printable encoding decodes without double encoding");
	memory = g_mime_stream_mem_new ();
	filtered = g_mime_stream_filter_new (memory);
	filter = g_mime_filter_basic_new (GMIME_CONTENT_ENCODING_QUOTEDPRINTABLE, TRUE);
	g_mime_stream_filter_add (GMIME_STREAM_FILTER (filtered), filter);
	g_object_unref (filter);
	filter = g_mime_filter_from_new (GMIME_FILTER_FROM_MODE_ARMOR);
	g_mime_stream_filter_add (GMIME_STREAM_FILTER (filtered), filter);
	g_object_unref (filter);
	g_assert_cmpint (g_mime_stream_write (filtered, input, sizeof (input) - 1), ==, sizeof (input) - 1);
	g_assert_cmpint (g_mime_stream_flush (filtered), ==, 0);
	encoded = g_mime_stream_mem_get_byte_array (GMIME_STREAM_MEM (memory));
	g_assert_cmpuint (encoded->len, >=, 8);
	g_assert_cmpint (memcmp (encoded->data, "=46rom ", 7), ==, 0);
	actual = g_byte_array_new ();
	filter = g_mime_filter_basic_new (GMIME_CONTENT_ENCODING_QUOTEDPRINTABLE, FALSE);
	filter_chunk (filter, encoded->data, encoded->len, TRUE, actual);
	expected = g_byte_array_new ();
	g_byte_array_append (expected, (const guint8 *) input, sizeof (input) - 1);
	check_filter_result (actual, expected);
	g_byte_array_unref (expected);
	g_byte_array_unref (actual);
	g_object_unref (filter);
	g_object_unref (filtered);
	g_object_unref (memory);
	testsuite_check_passed ();
}

static void
test_from_streams (void)
{
	static const char input[] = "From a\nFrom b\r\nxFrom \rFrom \n>From \n\0From \nFro";
	GMimeStream *memory, *filtered;
	GMimeFilter *filter;
	GByteArray *actual, *expected;
	char buffer[3];
	size_t offset;
	ssize_t n;
	guint mode;

	testsuite_check ("From filters binary stream reads, writes and final partial markers");
	for (mode = 0; mode < 2; mode++) {
		expected = from_expected ((const guint8 *) input, sizeof (input) - 1, mode);
		memory = g_mime_stream_mem_new ();
		filtered = g_mime_stream_filter_new (memory);
		filter = g_mime_filter_from_new (mode);
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
		filter = g_mime_filter_from_new (mode);
		g_mime_stream_filter_add (GMIME_STREAM_FILTER (filtered), filter);
		g_object_unref (filter);
		actual = g_byte_array_new ();
		while ((n = g_mime_stream_read (filtered, buffer, sizeof (buffer))) > 0)
			g_byte_array_append (actual, (const guint8 *) buffer, n);
		g_assert_cmpint (n, ==, 0);
		check_filter_result (actual, expected);
		g_byte_array_unref (actual);
		g_byte_array_unref (expected);
		g_object_unref (filtered);
		g_object_unref (memory);
	}
	testsuite_check_passed ();
}

static GByteArray *
anonymize_expected (const guint8 *input, size_t length)
{
	GByteArray *expected = g_byte_array_sized_new (length);
	guint8 c;
	size_t i;

	for (i = 0; i < length; i++) {
		c = input[i];
		if (c != ' ' && c != '\t' && c != '\r' && c != '\n')
			c = 'x';
		g_byte_array_append (expected, &c, 1);
	}

	return expected;
}

static void
test_anonymize_bytes (void)
{
	static const size_t prespaces[] = { 0, 1, 16 };
	GMimeFilter *filter;
	GByteArray *expected;
	char input[288], saved[288], *output;
	size_t offset, length, outlen, outprespace, i;
	guint complete, p;

	testsuite_check ("Anonymization filter byte classification, ranges and independent output");
	g_assert_cmpuint (g_type_from_name ("GMimeFilterAnonymize"), ==, GMIME_TYPE_FILTER_ANONYMIZE);
	filter = g_mime_filter_anonymize_new ();
	g_assert_true (GMIME_IS_FILTER_ANONYMIZE (filter));
	g_assert_true (g_type_is_a (G_OBJECT_TYPE (filter), GMIME_TYPE_FILTER));
	memset (input, 'Z', sizeof (input));
	for (i = 0; i < 256; i++)
		input[16 + i] = (guint8) i;
	memcpy (saved, input, sizeof (input));
	expected = anonymize_expected ((const guint8 *) input + 16, 256);

	for (complete = 0; complete < 2; complete++) {
		for (p = 0; p < G_N_ELEMENTS (prespaces); p++) {
			for (offset = 0; offset <= 256; offset++) {
				for (length = 0; length <= 256 - offset; length++) {
					output = NULL;
					outlen = outprespace = G_MAXSIZE;
					if (complete)
						g_mime_filter_complete (filter, input + 16 + offset, length, prespaces[p],
									&output, &outlen, &outprespace);
					else
						g_mime_filter_filter (filter, input + 16 + offset, length, prespaces[p],
								      &output, &outlen, &outprespace);
					g_assert_true (output == filter->outbuf);
					g_assert_nonnull (output);
					g_assert_cmpuint (outlen, ==, length);
					g_assert_cmpuint (outprespace, ==, filter->outpre);
					g_assert_cmpmem (output, outlen, expected->data + offset, length);
				}
			}
		}
	}
	g_assert_cmpmem (input, sizeof (input), saved, sizeof (saved));
	g_assert_null (filter->backbuf);
	g_assert_cmpuint (filter->backlen, ==, 0);
	g_byte_array_unref (expected);
	g_object_unref (filter);
	testsuite_check_passed ();
}

static void
test_anonymize_lifecycle (void)
{
	static const guint8 whitespace[] = " \t\r\n";
	GMimeFilter *filter = g_mime_filter_anonymize_new (), *copy;
	GByteArray *actual = g_byte_array_new (), *expected;
	guint8 input[256], *large;
	size_t i, split;

	testsuite_check ("Anonymization filter binary splits, completion, copy, reset and output growth");
	for (i = 0; i < sizeof (input); i++)
		input[i] = i;
	expected = anonymize_expected (input, sizeof (input));
	for (split = 0; split <= sizeof (input); split++) {
		g_mime_filter_reset (filter);
		g_byte_array_set_size (actual, 0);
		g_assert_false (filter_chunk (filter, input, split, FALSE, actual));
		g_assert_cmpuint (actual->len, ==, split);
		g_assert_false (filter_chunk (filter, input, 0, FALSE, actual));
		g_assert_false (filter_chunk (filter, input + split, sizeof (input) - split, TRUE, actual));
		check_filter_result (actual, expected);
		g_assert_false (filter_chunk (filter, input, 0, TRUE, actual));
		g_assert_false (filter_chunk (filter, input, 0, TRUE, actual));
		check_filter_result (actual, expected);
	}
	copy = g_mime_filter_copy (filter);
	g_assert_true (GMIME_IS_FILTER_ANONYMIZE (copy));
	g_assert_true (copy != filter);
	g_assert_null (copy->outbuf);
	g_byte_array_set_size (actual, 0);
	g_assert_false (filter_chunk (copy, whitespace, sizeof (whitespace) - 1, TRUE, actual));
	g_assert_cmpmem (actual->data, actual->len, whitespace, sizeof (whitespace) - 1);
	g_assert_true (copy->outbuf != filter->outbuf);
	g_object_unref (filter);
	g_byte_array_set_size (actual, 0);
	g_assert_false (filter_chunk (copy, input, sizeof (input), FALSE, actual));
	check_filter_result (actual, expected);
	g_mime_filter_reset (copy);
	g_byte_array_set_size (actual, 0);
	g_assert_false (filter_chunk (copy, input, sizeof (input), TRUE, actual));
	check_filter_result (actual, expected);
	g_byte_array_unref (expected);
	large = g_malloc (65537);
	for (i = 0; i < 65537; i++)
		large[i] = (guint8) i;
	expected = anonymize_expected (large, 65537);
	g_byte_array_set_size (actual, 0);
	g_assert_false (filter_chunk (copy, large, 65537, FALSE, actual));
	check_filter_result (actual, expected);
	g_free (large);
	g_byte_array_unref (expected);
	g_byte_array_set_size (actual, 0);
	g_assert_false (filter_chunk (copy, whitespace, sizeof (whitespace) - 1, TRUE, actual));
	g_assert_cmpmem (actual->data, actual->len, whitespace, sizeof (whitespace) - 1);
	g_object_unref (copy);
	g_byte_array_unref (actual);
	testsuite_check_passed ();
}

static void
test_anonymize_streams (void)
{
	GMimeStream *memory, *filtered;
	GMimeFilter *filter;
	GByteArray *actual, *expected;
	char input[256], buffer[5];
	size_t offset;
	ssize_t n;
	guint mode, pass;

	testsuite_check ("Anonymization filter streamed bytewise writes and split reads");
	for (offset = 0; offset < sizeof (input); offset++)
		input[offset] = (guint8) offset;
	expected = anonymize_expected ((const guint8 *) input, sizeof (input));
	for (mode = 0; mode < 2; mode++) {
		memory = mode ? g_mime_stream_mem_new_with_buffer (input, sizeof (input)) :
			g_mime_stream_mem_new ();
		filtered = g_mime_stream_filter_new (memory);
		filter = g_mime_filter_anonymize_new ();
		g_mime_stream_filter_add (GMIME_STREAM_FILTER (filtered), filter);
		g_object_unref (filter);
		filter = g_mime_filter_pass_through_new ();
		g_mime_stream_filter_add (GMIME_STREAM_FILTER (filtered), filter);
		g_object_unref (filter);
		if (mode == 0) {
			for (offset = 0; offset < sizeof (input); offset++)
				g_assert_cmpint (g_mime_stream_write (filtered, input + offset, 1), ==, 1);
			g_assert_cmpint (g_mime_stream_flush (filtered), ==, 0);
			g_assert_cmpint (g_mime_stream_flush (filtered), ==, 0);
			actual = g_mime_stream_mem_get_byte_array (GMIME_STREAM_MEM (memory));
			check_filter_result (actual, expected);
		} else {
			for (pass = 0; pass < 2; pass++) {
				if (pass)
					g_assert_cmpint (g_mime_stream_reset (filtered), ==, 0);
				actual = g_byte_array_new ();
				memset (buffer, 'Z', sizeof (buffer));
				while ((n = g_mime_stream_read (filtered, buffer + 1, 3)) > 0) {
					g_assert_cmpint (buffer[0], ==, 'Z');
					g_assert_cmpint (buffer[4], ==, 'Z');
					g_byte_array_append (actual, (const guint8 *) buffer + 1, n);
				}
				g_assert_cmpint (n, ==, 0);
				check_filter_result (actual, expected);
				g_byte_array_unref (actual);
			}
		}
		g_object_unref (filtered);
		g_object_unref (memory);
	}
	g_byte_array_unref (expected);
	testsuite_check_passed ();
}

static void
test_pass_through_identity (void)
{
	static const size_t prespaces[] = { 0, 1, 16 };
	GMimeFilter *filter;
	char input[288], saved[288], *output;
	size_t offset, length, outlen, outprespace, i;
	guint complete, p;

	testsuite_check ("Pass-through filter input pointer, length and prespace identity");
	g_assert_cmpuint (g_type_from_name ("GMimeFilterPassThrough"), ==, GMIME_TYPE_FILTER_PASS_THROUGH);
	filter = g_mime_filter_pass_through_new ();
	g_assert_true (GMIME_IS_FILTER_PASS_THROUGH (filter));
	g_assert_true (g_type_is_a (G_OBJECT_TYPE (filter), GMIME_TYPE_FILTER));
	memset (input, 'Z', sizeof (input));
	for (i = 0; i < 256; i++)
		input[16 + i] = (guint8) i;
	memcpy (saved, input, sizeof (input));

	for (complete = 0; complete < 2; complete++) {
		for (p = 0; p < G_N_ELEMENTS (prespaces); p++) {
			for (offset = 0; offset <= 256; offset++) {
				for (length = 0; length <= 256 - offset; length++) {
					output = NULL;
					outlen = outprespace = G_MAXSIZE;
					if (complete)
						g_mime_filter_complete (filter, input + 16 + offset, length, prespaces[p],
									&output, &outlen, &outprespace);
					else
						g_mime_filter_filter (filter, input + 16 + offset, length, prespaces[p],
								      &output, &outlen, &outprespace);
					g_assert_true (output == input + 16 + offset);
					g_assert_cmpuint (outlen, ==, length);
					g_assert_cmpuint (outprespace, ==, prespaces[p]);
				}
			}
		}
	}
	g_assert_cmpint (memcmp (input, saved, sizeof (input)), ==, 0);
	g_assert_null (filter->outbuf);
	g_assert_null (filter->outreal);
	g_assert_cmpuint (filter->outsize, ==, 0);
	g_assert_null (filter->backbuf);
	g_assert_cmpuint (filter->backlen, ==, 0);
	g_object_unref (filter);
	testsuite_check_passed ();
}

static void
test_pass_through_lifecycle (void)
{
	GMimeFilter *filter = g_mime_filter_pass_through_new (), *copy;
	GByteArray *actual = g_byte_array_new (), *expected = g_byte_array_new ();
	guint8 input[256];
	size_t i, split;

	testsuite_check ("Pass-through filter binary splits, completion, reset and independent copies");
	for (i = 0; i < sizeof (input); i++)
		input[i] = i;
	g_byte_array_append (expected, input, sizeof (input));
	for (split = 0; split <= sizeof (input); split++) {
		g_mime_filter_reset (filter);
		g_byte_array_set_size (actual, 0);
		g_assert_true (filter_chunk (filter, input, split, FALSE, actual));
		g_assert_cmpuint (actual->len, ==, split);
		g_assert_true (filter_chunk (filter, input, 0, FALSE, actual));
		g_assert_true (filter_chunk (filter, input + split, sizeof (input) - split, TRUE, actual));
		check_filter_result (actual, expected);
		g_assert_true (filter_chunk (filter, input, 0, TRUE, actual));
		g_assert_true (filter_chunk (filter, input, 0, TRUE, actual));
		check_filter_result (actual, expected);
	}
	copy = g_mime_filter_copy (filter);
	g_assert_true (GMIME_IS_FILTER_PASS_THROUGH (copy));
	g_assert_true (copy != filter);
	g_object_unref (filter);
	g_mime_filter_reset (copy);
	g_byte_array_set_size (actual, 0);
	g_assert_true (filter_chunk (copy, input, sizeof (input), TRUE, actual));
	check_filter_result (actual, expected);
	g_byte_array_set_size (actual, 0);
	g_assert_true (filter_chunk (copy, input, sizeof (input), FALSE, actual));
	check_filter_result (actual, expected);
	g_assert_null (copy->outbuf);
	g_assert_null (copy->backbuf);
	g_object_unref (copy);
	g_byte_array_unref (expected);
	g_byte_array_unref (actual);
	testsuite_check_passed ();
}

static void
add_pass_through_chain (GMimeStream *stream, gboolean transform)
{
	GMimeFilter *filter;
	guint i;

	for (i = 0; i < 3; i++) {
		filter = g_mime_filter_pass_through_new ();
		g_mime_stream_filter_add (GMIME_STREAM_FILTER (stream), filter);
		g_object_unref (filter);
		if (transform && i < 2) {
			filter = i == 0 ? g_mime_filter_from_new (GMIME_FILTER_FROM_MODE_ESCAPE) :
				g_mime_filter_strip_new ();
			g_mime_stream_filter_add (GMIME_STREAM_FILTER (stream), filter);
			g_object_unref (filter);
		}
	}
}

static void
test_pass_through_streams (void)
{
	static const char input[] = "From a \t\n\0\377From \rFrom \nFrom \t\r\nFro";
	GMimeStream *memory, *filtered;
	GByteArray *actual, *expected, *escaped;
	char buffer[3];
	size_t offset;
	ssize_t n;
	guint transform, pass;

	testsuite_check ("Pass-through filter stream reads, writes, reset and buffering filter chains");
	for (transform = 0; transform < 2; transform++) {
		if (transform) {
			escaped = from_expected ((const guint8 *) input, sizeof (input) - 1, GMIME_FILTER_FROM_MODE_ESCAPE);
			expected = strip_expected (escaped->data, escaped->len);
			g_byte_array_unref (escaped);
		} else {
			expected = g_byte_array_new ();
			g_byte_array_append (expected, (const guint8 *) input, sizeof (input) - 1);
		}
		memory = g_mime_stream_mem_new ();
		filtered = g_mime_stream_filter_new (memory);
		add_pass_through_chain (filtered, transform);
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
		add_pass_through_chain (filtered, transform);
		for (pass = 0; pass < 2; pass++) {
			if (pass)
				g_assert_cmpint (g_mime_stream_reset (filtered), ==, 0);
			actual = g_byte_array_new ();
			while ((n = g_mime_stream_read (filtered, buffer, sizeof (buffer))) > 0)
				g_byte_array_append (actual, (const guint8 *) buffer, n);
			g_assert_cmpint (n, ==, 0);
			check_filter_result (actual, expected);
			g_byte_array_unref (actual);
		}
		g_byte_array_unref (expected);
		g_object_unref (filtered);
		g_object_unref (memory);
	}
	testsuite_check_passed ();
}

static GMimeFilter *
new_configured_encoding_filter (guint kind, GMimeNewLineFormat newline_format, guint max_line_length)
{
	GMimeEncoder *encoder;
	GMimeFilter *filter;

	switch (kind) {
	case 0: encoder = GMIME_ENCODER (g_mime_base64_encoder_new_with_newline_format (newline_format, max_line_length)); break;
	case 1: encoder = GMIME_ENCODER (g_mime_quoted_printable_encoder_new_with_newline_format (newline_format, max_line_length)); break;
	case 2: encoder = GMIME_ENCODER (g_mime_uu_encoder_new_with_newline_format (newline_format)); break;
	default: g_assert_not_reached ();
	}
	filter = g_mime_filter_basic_new_with_encoder (encoder);
	g_object_unref (encoder);
	return filter;
}

static void
test_encoding_newline_lifecycle (void)
{
	guint8 input[129];
	GMimeFilter *filter, *copy;
	GByteArray *expected = g_byte_array_new (), *actual = g_byte_array_new ();
	GByteArray *copied = g_byte_array_new ();
	guint kind, format, i;
	size_t split;

	testsuite_check ("Configured encoder filters preserve newlines and line limits through copy, reset and flush");
	for (i = 0; i < sizeof (input); i++)
		input[i] = i * 31;
	for (kind = 0; kind < 3; kind++) {
		for (format = 0; format < 2; format++) {
			filter = new_configured_encoding_filter (kind, format, 60);
			g_byte_array_set_size (expected, 0);
			filter_chunk (filter, input, sizeof (input), TRUE, expected);
			for (split = 0; split <= sizeof (input); split++) {
				g_byte_array_set_size (actual, 0);
				g_byte_array_set_size (copied, 0);
				filter_chunk (filter, input, split, FALSE, actual);
				copy = g_mime_filter_copy (filter);
				g_assert_true (GMIME_FILTER_BASIC (copy)->encoder != GMIME_FILTER_BASIC (filter)->encoder);
				filter_chunk (filter, input + split, sizeof (input) - split, TRUE, actual);
				check_filter_result (actual, expected);
				filter_chunk (copy, input, sizeof (input), TRUE, copied);
				check_filter_result (copied, expected);
				g_object_unref (copy);

				g_byte_array_set_size (actual, 0);
				filter_chunk (filter, input, split, FALSE, actual);
				g_mime_filter_reset (filter);
				g_byte_array_set_size (actual, 0);
				filter_chunk (filter, input, sizeof (input), TRUE, actual);
				check_filter_result (actual, expected);
			}
			g_object_unref (filter);
		}
	}
	g_byte_array_unref (copied);
	g_byte_array_unref (actual);
	g_byte_array_unref (expected);
	testsuite_check_passed ();
}

static void
test_encoding_newline_streams (void)
{
	static const GMimeContentEncoding encodings[] = {
		GMIME_CONTENT_ENCODING_BASE64, GMIME_CONTENT_ENCODING_QUOTEDPRINTABLE, GMIME_CONTENT_ENCODING_UUENCODE
	};
	guint8 input[1025];
	GMimeStream *memory, *filtered;
	GMimeFilter *filter;
	GByteArray *expected = g_byte_array_new (), *actual;
	char buffer[3];
	guint kind, format, i, pass;
	ssize_t n;

	testsuite_check ("Factory-created encoder filter stream reads, writes and reset preserve LF/CRLF output");
	for (i = 0; i < sizeof (input); i++)
		input[i] = i * 31;
	for (kind = 0; kind < 3; kind++) {
		for (format = 0; format < 2; format++) {
			filter = g_mime_filter_basic_create_encoder_with_newline_format (encodings[kind], format, 60);
			g_byte_array_set_size (expected, 0);
			filter_chunk (filter, input, sizeof (input), TRUE, expected);
			memory = g_mime_stream_mem_new ();
			filtered = g_mime_stream_filter_new (memory);
			g_mime_stream_filter_add (GMIME_STREAM_FILTER (filtered), filter);
			for (i = 0; i < sizeof (input); i++)
				g_assert_cmpint (g_mime_stream_write (filtered, (const char *) input + i, 1), ==, 1);
			g_assert_cmpint (g_mime_stream_flush (filtered), ==, 0);
			actual = g_mime_stream_mem_get_byte_array (GMIME_STREAM_MEM (memory));
			check_filter_result (actual, expected);
			g_object_unref (filtered);
			g_object_unref (memory);

			memory = g_mime_stream_mem_new_with_buffer ((const char *) input, sizeof (input));
			filtered = g_mime_stream_filter_new (memory);
			g_mime_filter_reset (filter);
			g_mime_stream_filter_add (GMIME_STREAM_FILTER (filtered), filter);
			g_object_unref (filter);
			for (pass = 0; pass < 2; pass++) {
				if (pass)
					g_assert_cmpint (g_mime_stream_reset (filtered), ==, 0);
				actual = g_byte_array_new ();
				while ((n = g_mime_stream_read (filtered, buffer, sizeof (buffer))) > 0)
					g_byte_array_append (actual, (const guint8 *) buffer, n);
				g_assert_cmpint (n, ==, 0);
				check_filter_result (actual, expected);
				g_byte_array_unref (actual);
			}
			g_object_unref (filtered);
			g_object_unref (memory);
		}
	}
	g_byte_array_unref (expected);
	testsuite_check_passed ();
}

static void
check_factory_selection (GMimeFilter *filter, GMimeContentEncoding encoding, gboolean encode)
{
	GMimeFilterBasic *basic;

	g_assert_nonnull (filter);
	if (encoding < GMIME_CONTENT_ENCODING_BASE64 || encoding > GMIME_CONTENT_ENCODING_UUENCODE) {
		g_assert_true (GMIME_IS_FILTER_PASS_THROUGH (filter));
		g_assert_null (filter->outbuf);
		return;
	}
	g_assert_true (GMIME_IS_FILTER_BASIC (filter));
	basic = GMIME_FILTER_BASIC (filter);
	if (encode) {
		g_assert_nonnull (basic->encoder);
		g_assert_null (basic->decoder);
		g_assert_cmpint (g_mime_encoder_get_encoding (basic->encoder), ==, encoding);
	} else {
		g_assert_nonnull (basic->decoder);
		g_assert_null (basic->encoder);
		g_assert_cmpint (g_mime_decoder_get_encoding (basic->decoder), ==, encoding);
	}
}

static void
check_factory_vector (GMimeFilter *filter, GMimeContentEncoding encoding, gboolean encode)
{
	const char *input = "Man", *text = "Man";
	GByteArray *expected = g_byte_array_new (), *actual = g_byte_array_new ();
	GByteArray *copied = g_byte_array_new ();
	GMimeFilter *copy;
	gboolean identity = encoding < GMIME_CONTENT_ENCODING_BASE64 || encoding > GMIME_CONTENT_ENCODING_UUENCODE;
	size_t split, length;

	check_factory_selection (filter, encoding, encode);
	switch (encoding) {
	case GMIME_CONTENT_ENCODING_BASE64: text = "TWFu\r\n"; break;
	case GMIME_CONTENT_ENCODING_QUOTEDPRINTABLE: text = "Man=\r\n"; break;
	case GMIME_CONTENT_ENCODING_UUENCODE: text = "#36%N\r\n`\r\n"; break;
	default: break;
	}
	if (!encode) {
		input = encoding == GMIME_CONTENT_ENCODING_UUENCODE ? "begin 644 x\r\n#36%N\r\n`\r\nend\r\n" : text;
		text = "Man";
	}
	length = strlen (input);
	g_byte_array_append (expected, (const guint8 *) text, strlen (text));
	for (split = 0; split <= length; split++) {
		g_mime_filter_reset (filter);
		g_byte_array_set_size (actual, 0);
		g_byte_array_set_size (copied, 0);
		g_assert_cmpint (filter_chunk (filter, (const guint8 *) input, split, FALSE, actual), ==, identity);
		copy = g_mime_filter_copy (filter);
		check_factory_selection (copy, encoding, encode);
		g_assert_cmpint (filter_chunk (filter, (const guint8 *) input + split, length - split, TRUE, actual), ==, identity);
		check_filter_result (actual, expected);
		filter_chunk (copy, (const guint8 *) input, length, TRUE, copied);
		check_filter_result (copied, expected);
		g_object_unref (copy);
	}
	g_byte_array_unref (copied);
	g_byte_array_unref (actual);
	g_byte_array_unref (expected);
	g_object_unref (filter);
}

static void
test_factory_selection (void)
{
	static const GMimeContentEncoding encodings[] = {
		(GMimeContentEncoding) -1, GMIME_CONTENT_ENCODING_UNSPECIFIED,
		GMIME_CONTENT_ENCODING_7BIT, GMIME_CONTENT_ENCODING_8BIT, GMIME_CONTENT_ENCODING_BINARY,
		GMIME_CONTENT_ENCODING_BASE64, GMIME_CONTENT_ENCODING_QUOTEDPRINTABLE,
		GMIME_CONTENT_ENCODING_UUENCODE, (GMimeContentEncoding) 7, (GMimeContentEncoding) G_MAXINT
	};
	static const struct {
		const char *name;
		GMimeContentEncoding encoding;
	} names[] = {
		{ "base64", GMIME_CONTENT_ENCODING_BASE64 },
		{ "\tBaSe64;ignored", GMIME_CONTENT_ENCODING_BASE64 },
		{ "base64 junk", GMIME_CONTENT_ENCODING_BASE64 },
		{ "quoted-printable", GMIME_CONTENT_ENCODING_QUOTEDPRINTABLE },
		{ " QUOTED-PRINTABLE;\t", GMIME_CONTENT_ENCODING_QUOTEDPRINTABLE },
		{ "x-uuencode", GMIME_CONTENT_ENCODING_UUENCODE },
		{ "UuEnCoDe;foo", GMIME_CONTENT_ENCODING_UUENCODE },
		{ "X-UUE ", GMIME_CONTENT_ENCODING_UUENCODE },
		{ "", GMIME_CONTENT_ENCODING_UNSPECIFIED },
		{ " \t\r\n", GMIME_CONTENT_ENCODING_UNSPECIFIED },
		{ ";base64", GMIME_CONTENT_ENCODING_UNSPECIFIED },
		{ "base64extra", GMIME_CONTENT_ENCODING_UNSPECIFIED },
		{ "base64(comment)", GMIME_CONTENT_ENCODING_UNSPECIFIED },
		{ "(comment) base64", GMIME_CONTENT_ENCODING_UNSPECIFIED },
		{ "x-invalid", GMIME_CONTENT_ENCODING_UNSPECIFIED },
		{ "7bit", GMIME_CONTENT_ENCODING_7BIT },
		{ "8bit", GMIME_CONTENT_ENCODING_8BIT },
		{ "binary", GMIME_CONTENT_ENCODING_BINARY }
	};
	guint i;

	testsuite_check ("Encoding filter factories select codecs, aliases, fresh copies and zero-copy identities");
	for (i = 0; i < G_N_ELEMENTS (encodings); i++) {
		check_factory_vector (g_mime_filter_basic_create_encoder (encodings[i]), encodings[i], TRUE);
		check_factory_vector (g_mime_filter_basic_create_decoder (encodings[i]), encodings[i], FALSE);
	}
	for (i = 0; i < G_N_ELEMENTS (names); i++) {
		check_factory_vector (g_mime_filter_basic_create_encoder_by_name (names[i].name), names[i].encoding, TRUE);
		check_factory_vector (g_mime_filter_basic_create_decoder_by_name (names[i].name), names[i].encoding, FALSE);
	}
	testsuite_check_passed ();
}

static void
test_factory_names (void)
{
	static const gunichar whitespace[] = {
		9, 10, 11, 12, 13, 32, 0x85, 0xa0, 0x1680,
		0x2000, 0x2001, 0x2002, 0x2003, 0x2004, 0x2005, 0x2006,
		0x2007, 0x2008, 0x2009, 0x200a, 0x2028, 0x2029, 0x202f, 0x205f, 0x3000
	};
	char space[7], *name;
	guint i;

	testsuite_check ("Encoding name factories recognize Unicode whitespace and semicolon delimiters");
	for (i = 0; i < G_N_ELEMENTS (whitespace); i++) {
		space[g_unichar_to_utf8 (whitespace[i], space)] = '\0';
		name = g_strdup_printf ("%sBaSe64%signored", space, space);
		check_factory_vector (g_mime_filter_basic_create_encoder_by_name (name), GMIME_CONTENT_ENCODING_BASE64, TRUE);
		check_factory_vector (g_mime_filter_basic_create_decoder_by_name (name), GMIME_CONTENT_ENCODING_BASE64, FALSE);
		g_free (name);
	}
	testsuite_check_passed ();
}

static void
test_factory_newlines (void)
{
	static const GMimeContentEncoding encodings[] = {
		GMIME_CONTENT_ENCODING_BASE64, GMIME_CONTENT_ENCODING_QUOTEDPRINTABLE, GMIME_CONTENT_ENCODING_UUENCODE
	};
	static const char *names[] = { "base64;", "quoted-printable ", "x-uue;" };
	static const guint limits[] = { 60, 61, 63, 75, 76, 78, 998 };
	guint8 input[1025];
	GMimeFilter *direct, *filter;
	GByteArray *expected = g_byte_array_new (), *actual = g_byte_array_new ();
	guint kind, format, i, j, by_name;

	testsuite_check ("Encoding factories preserve requested line endings and limits without changing legacy constructors");
	for (i = 0; i < sizeof (input); i++)
		input[i] = i * 31;
	for (kind = 0; kind < 3; kind++) {
		for (format = 0; format < 2; format++) {
			for (i = 0; i < G_N_ELEMENTS (limits); i++) {
				direct = new_configured_encoding_filter (kind, format, limits[i]);
				g_byte_array_set_size (expected, 0);
				filter_chunk (direct, input, sizeof (input), TRUE, expected);
				g_object_unref (direct);
				for (by_name = 0; by_name < 2; by_name++) {
					filter = by_name ?
						g_mime_filter_basic_create_encoder_by_name_with_newline_format (names[kind], format, limits[i]) :
						g_mime_filter_basic_create_encoder_with_newline_format (encodings[kind], format, limits[i]);
					g_byte_array_set_size (actual, 0);
					for (j = 0; j < sizeof (input); j++)
						filter_chunk (filter, input + j, 1, FALSE, actual);
					filter_chunk (filter, input, 0, TRUE, actual);
					check_filter_result (actual, expected);
					g_object_unref (filter);
				}
			}
		}
		direct = new_configured_encoding_filter (kind, GMIME_NEWLINE_FORMAT_UNIX, 76);
		g_byte_array_set_size (expected, 0);
		filter_chunk (direct, input, sizeof (input), TRUE, expected);
		g_object_unref (direct);
		filter = g_mime_filter_basic_new (encodings[kind], TRUE);
		g_byte_array_set_size (actual, 0);
		filter_chunk (filter, input, sizeof (input), TRUE, actual);
		check_filter_result (actual, expected);
		g_object_unref (filter);
	}
	for (i = GMIME_CONTENT_ENCODING_UNSPECIFIED; i <= GMIME_CONTENT_ENCODING_BINARY; i++) {
		for (j = 0; j < 2; j++) {
			filter = g_mime_filter_basic_new (i, j);
			g_assert_true (GMIME_IS_FILTER_BASIC (filter));
			g_byte_array_set_size (actual, 0);
			g_assert_false (filter_chunk (filter, input, sizeof (input), TRUE, actual));
			g_assert_cmpmem (actual->data, actual->len, input, sizeof (input));
			g_object_unref (filter);
		}
	}
	g_byte_array_unref (actual);
	g_byte_array_unref (expected);
	testsuite_check_passed ();
}

static void
test_factory_preconditions (void)
{
	static const guint lengths[] = { 0, 59, 999, G_MAXUINT };
	static const char *names[] = { "base64;", "quoted-printable " };
	static const GMimeContentEncoding encodings[] = {
		GMIME_CONTENT_ENCODING_BASE64, GMIME_CONTENT_ENCODING_QUOTEDPRINTABLE
	};
	GMimeFilter *filter;
	guint kind, i, format;

	testsuite_check ("Filter factory argument validation follows the selected codec and preserves pass-through semantics");
	g_test_expect_message ("gmime", G_LOG_LEVEL_CRITICAL, "*name != NULL*");
	g_assert_null (g_mime_filter_basic_create_encoder_by_name (NULL));
	g_test_assert_expected_messages ();
	g_test_expect_message ("gmime", G_LOG_LEVEL_CRITICAL, "*name != NULL*");
	g_assert_null (g_mime_filter_basic_create_decoder_by_name (NULL));
	g_test_assert_expected_messages ();
	g_test_expect_message ("gmime", G_LOG_LEVEL_CRITICAL, "*g_utf8_validate*");
	g_assert_null (g_mime_filter_basic_create_encoder_by_name ("\xff"));
	g_test_assert_expected_messages ();
	g_test_expect_message ("gmime", G_LOG_LEVEL_CRITICAL, "*g_utf8_validate*");
	g_assert_null (g_mime_filter_basic_create_decoder_by_name ("\xff"));
	g_test_assert_expected_messages ();
	for (kind = 0; kind < 2; kind++) {
		for (format = 0; format < 2; format++) {
			for (i = 0; i < G_N_ELEMENTS (lengths); i++) {
				g_test_expect_message ("gmime", G_LOG_LEVEL_CRITICAL, "*max_line_length >= 60*");
				g_assert_null (g_mime_filter_basic_create_encoder_with_newline_format (encodings[kind], format, lengths[i]));
				g_test_assert_expected_messages ();
				g_test_expect_message ("gmime", G_LOG_LEVEL_CRITICAL, "*max_line_length >= 60*");
				g_assert_null (g_mime_filter_basic_create_encoder_by_name_with_newline_format (names[kind], format, lengths[i]));
				g_test_assert_expected_messages ();
			}
		}
	}
	for (i = 0; i < G_N_ELEMENTS (lengths); i++) {
		filter = g_mime_filter_basic_create_encoder_with_newline_format (GMIME_CONTENT_ENCODING_UUENCODE, GMIME_NEWLINE_FORMAT_DOS, lengths[i]);
		check_factory_vector (filter, GMIME_CONTENT_ENCODING_UUENCODE, TRUE);
		filter = g_mime_filter_basic_create_encoder_by_name_with_newline_format ("unknown", GMIME_NEWLINE_FORMAT_DOS, lengths[i]);
		check_factory_vector (filter, GMIME_CONTENT_ENCODING_UNSPECIFIED, TRUE);
	}
	for (kind = 0; kind <= GMIME_CONTENT_ENCODING_UUENCODE; kind++) {
		g_test_expect_message ("gmime", G_LOG_LEVEL_CRITICAL, "*newline_format ==*");
		g_assert_null (g_mime_filter_basic_create_encoder_with_newline_format (kind, (GMimeNewLineFormat) -1, 0));
		g_test_assert_expected_messages ();
	}
	g_test_expect_message ("gmime", G_LOG_LEVEL_CRITICAL, "*newline_format ==*");
	g_assert_null (g_mime_filter_basic_create_encoder_by_name_with_newline_format ("unknown", (GMimeNewLineFormat) 2, 0));
	g_test_assert_expected_messages ();
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
	test_from_streaming ();
	test_from_lifecycle ();
	test_from_prespace ();
	test_from_many_lines ();
	test_from_quoted_printable ();
	test_from_streams ();
	test_pass_through_identity ();
	test_anonymize_bytes ();
	test_anonymize_lifecycle ();
	test_anonymize_streams ();
	test_pass_through_lifecycle ();
	test_pass_through_streams ();
	test_encoding_newline_lifecycle ();
	test_encoding_newline_streams ();
	test_factory_selection ();
	test_factory_names ();
	test_factory_newlines ();
	test_factory_preconditions ();
	
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
