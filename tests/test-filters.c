/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*  GMime
 *  Copyright (C) 2000-2020 Jeffrey Stedfast
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
