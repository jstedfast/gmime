/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*  GMime
 *  Copyright (C) 2000-2017 Jeffrey Stedfast
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
	
	if (actual->len != expected->len || memcmp (actual->data, expected->data, actual->len) != 0) {
		//if (actual->len != expected->len)
		//	testsuite_check_failed ("%s failed: streams are not the same length: %u", what, actual->len);
		//else
		//	testsuite_check_failed ("%s failed: streams do not match", what);
		
		guint min = MIN (actual->len, expected->len);
		guint i;
		
		for (i = 0; i < min; i++) {
			if (actual->data[i] != expected->data[i])
				break;
		}
		
		testsuite_check_failed ("%s failed: streams do not match at index %u", what, i);
		
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
	
	if (actual->len != expected->len || memcmp (actual->data, expected->data, actual->len) != 0) {
		testsuite_check_failed ("%s failed: streams do not match", what);
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

int main (int argc, char **argv)
{
	const char *datadir = "data/filters";
	struct stat st;
	int i;
	
	g_mime_init ();
	
	testsuite_init (argc, argv);
	
	verbose = 4;
	
	for (i = 1; i < argc; i++) {
		if (argv[i][0] != '-') {
			datadir = argv[i];
			break;
		}
	}
	
	if (i < argc && (stat (datadir, &st) == -1 || !S_ISDIR (st.st_mode)))
		return 0;
	
	testsuite_start ("Filter tests");
	
	test_gzip (datadir, "lorem-ipsum.txt");
	test_gunzip (datadir, "lorem-ipsum.txt");
	
	testsuite_end ();
	
	g_mime_shutdown ();
	
	return testsuite_exit ();
}
