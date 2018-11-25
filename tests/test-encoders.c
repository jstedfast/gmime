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
#define v(x) if (verbose > 3) x

static const char *base64_encoded_patterns[] = {
	"VGhpcyBpcyB0aGUgcGxhaW4gdGV4dCBtZXNzYWdlIQ==",
	"VGhpcyBpcyBhIHRleHQgd2hpY2ggaGFzIHRvIGJlIHBhZGRlZCBvbmNlLi4=",
	"VGhpcyBpcyBhIHRleHQgd2hpY2ggaGFzIHRvIGJlIHBhZGRlZCB0d2ljZQ==",
	"VGhpcyBpcyBhIHRleHQgd2hpY2ggd2lsbCBub3QgYmUgcGFkZGVk",
	" &% VGhp\r\ncyBp\r\ncyB0aGUgcGxhaW4g  \tdGV4dCBtZ?!XNzY*WdlIQ==",	
};

static const char *base64_decoded_patterns[] = {
	"This is the plain text message!",
	"This is a text which has to be padded once..",
	"This is a text which has to be padded twice",
	"This is a text which will not be padded",
	"This is the plain text message!"
};

static const char *base64_encoded_long_patterns[] = {
	"AAECAwQFBgcICQoLDA0ODxAREhMUFRYXGBkaGxwdHh8gISIjJCU" \
	"mJygpKissLS4vMDEyMzQ1Njc4OTo7PD0+P0BBQkNERUZHSElKS0" \
	"xNTk9QUVJTVFVWV1hZWltcXV5fYGFiY2RlZmdoaWprbG1ub3Bxc" \
	"nN0dXZ3eHl6e3x9fn+AgYKDhIWGh4iJiouMjY6PkJGSk5SVlpeY" \
	"mZqbnJ2en6ChoqOkpaanqKmqq6ytrq+wsbKztLW2t7i5uru8vb6" \
	"/wMHCw8TFxsfIycrLzM3Oz9DR0tPU1dbX2Nna29zd3t/g4eLj5O" \
	"Xm5+jp6uvs7e7v8PHy8/T19vf4+fr7/P3+/w==",
	
	"AQIDBAUGBwgJCgsMDQ4PEBESExQVFhcYGRobHB0eHyAhIiMkJSY" \
	"nKCkqKywtLi8wMTIzNDU2Nzg5Ojs8PT4/QEFCQ0RFRkdISUpLTE" \
	"1OT1BRUlNUVVZXWFlaW1xdXl9gYWJjZGVmZ2hpamtsbW5vcHFyc" \
	"3R1dnd4eXp7fH1+f4CBgoOEhYaHiImKi4yNjo+QkZKTlJWWl5iZ" \
	"mpucnZ6foKGio6SlpqeoqaqrrK2ur7CxsrO0tba3uLm6u7y9vr/" \
	"AwcLDxMXGx8jJysvMzc7P0NHS09TV1tfY2drb3N3e3+Dh4uPk5e" \
	"bn6Onq6+zt7u/w8fLz9PX29/j5+vv8/f7/AA==",
	
	"AgMEBQYHCAkKCwwNDg8QERITFBUWFxgZGhscHR4fICEiIyQlJic" \
	"oKSorLC0uLzAxMjM0NTY3ODk6Ozw9Pj9AQUJDREVGR0hJSktMTU" \
	"5PUFFSU1RVVldYWVpbXF1eX2BhYmNkZWZnaGlqa2xtbm9wcXJzd" \
	"HV2d3h5ent8fX5/gIGCg4SFhoeIiYqLjI2Oj5CRkpOUlZaXmJma" \
	"m5ydnp+goaKjpKWmp6ipqqusra6vsLGys7S1tre4ubq7vL2+v8D" \
	"BwsPExcbHyMnKy8zNzs/Q0dLT1NXW19jZ2tvc3d7f4OHi4+Tl5u" \
	"fo6err7O3u7/Dx8vP09fb3+Pn6+/z9/v8AAQ=="
};

static void
test_base64_decode_patterns (void)
{
	const char *input, *expected;
	GMimeEncoding decoder;
	char output[4096];
	size_t len, n;
	guint i, j;
	
	g_mime_encoding_init_decode (&decoder, GMIME_CONTENT_ENCODING_BASE64);
	
	for (i = 0; i < G_N_ELEMENTS (base64_encoded_patterns); i++) {
		testsuite_check ("base64_encoded_patterns[%u]", i);
		
		expected = base64_decoded_patterns[i];
		input = base64_encoded_patterns[i];
		len = strlen (input);
		
		n = g_mime_encoding_step (&decoder, input, len, output);
		len = strlen (expected);
		
		if (n != len)
			testsuite_check_failed ("base64_encoded_patterns[%u] failed: decoded lengths did not match (expected: %zu, was: %zu)", i, len, n);
		else if (strncmp (expected, output, n) != 0)
			testsuite_check_failed ("base64_encoded_patterns[%u] failed: decoded values did not match", i);
		else
			testsuite_check_passed ();
		
		g_mime_encoding_reset (&decoder);
	}
	
	for (i = 0; i < G_N_ELEMENTS (base64_encoded_long_patterns); i++) {
		testsuite_check ("base64_encoded_long_patterns[%u]", i);
		
		input = base64_encoded_long_patterns[i];
		len = strlen (input);
		
		n = g_mime_encoding_step (&decoder, input, len, output);
		len = strlen (expected);
		
		for (j = 0; j < n; j++) {
			if (output[j] != (char) (j + i))
				break;
		}
		
		if (j < n)
			testsuite_check_failed ("base64_encoded_long_patterns[%u] failed: decoded values did not match at index %u", i, j);
		else
			testsuite_check_passed ();
		
		g_mime_encoding_reset (&decoder);
	}
}

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
test_encoder (GMimeContentEncoding encoding, GByteArray *photo, GByteArray *expected, size_t size)
{
	const char *name = g_mime_content_encoding_to_string (encoding);
	GMimeStream *istream, *ostream, *filtered;
	ssize_t nread, nwritten, n;
	GMimeFilter *filter;
	GByteArray *actual;
	char buf[size];
	char *path;
	
	testsuite_check ("%s encoding; buffer-size=%zu", name, size);
	
	/* create the input stream */
	istream = g_mime_stream_mem_new_with_byte_array (photo);
	g_mime_stream_mem_set_owner ((GMimeStreamMem *) istream, FALSE);
	
	/* create the output stream */
	actual = g_byte_array_new ();
	ostream = g_mime_stream_mem_new_with_byte_array (actual);
	g_mime_stream_mem_set_owner ((GMimeStreamMem *) ostream, FALSE);
	
	if (encoding == GMIME_CONTENT_ENCODING_UUENCODE)
		g_mime_stream_write_string (ostream, "begin 644 photo.jpg\n");
	
	/* create the filtered stream that will do all of the encoding for us */
	filtered = g_mime_stream_filter_new (ostream);
	filter = g_mime_filter_basic_new (encoding, TRUE);
	g_mime_stream_filter_add ((GMimeStreamFilter *) filtered, filter);
	g_object_unref (filter);
	
	while ((nread = g_mime_stream_read (istream, buf, size)) > 0) {
		nwritten = 0;
		
		do {
			if ((n = g_mime_stream_write (filtered, buf + nwritten, nread - nwritten)) > 0)
				nwritten += n;
		} while (nwritten < nread);
	}
	
	g_mime_stream_flush (filtered);
	g_object_unref (filtered);
	g_object_unref (istream);
	
	if (encoding == GMIME_CONTENT_ENCODING_UUENCODE)
		g_mime_stream_write_string (ostream, "end\n");
	
	g_object_unref (ostream);
	
	/* now let's compare the expected vs actual output */
	if (actual->len != expected->len) {
		testsuite_check_failed ("%s encoding failed: encoded content does not match: expected=%u; actual=%u",
					name, expected->len, actual->len);
		goto error;
	}
	
	if (memcmp (actual->data, expected->data, actual->len) != 0) {
		testsuite_check_failed ("%s encoding failed: encoded content does not match", name);
		goto error;
	}
	
	testsuite_check_passed ();
	
error:
	path = g_strdup_printf ("%s.encode.%zu.txt", name, size);
	ostream = g_mime_stream_fs_open (path, O_CREAT | O_TRUNC | O_RDWR, 0644, NULL);
	g_mime_stream_write (ostream, (const char *) actual->data, actual->len);
	g_mime_stream_flush (ostream);
	g_object_unref (ostream);
	
	g_byte_array_free (actual, TRUE);
}

static void
test_decoder (GMimeContentEncoding encoding, GByteArray *encoded, GByteArray *expected, size_t size)
{
	const char *name = g_mime_content_encoding_to_string (encoding);
	GMimeStream *istream, *ostream, *filtered;
	ssize_t nread, nwritten, n;
	GMimeFilter *filter;
	GByteArray *actual;
	char buf[size];
	char *path;
	
	testsuite_check ("%s decoding; buffer-size=%zu", name, size);
	
	/* create the input stream */
	istream = g_mime_stream_mem_new_with_byte_array (encoded);
	g_mime_stream_mem_set_owner ((GMimeStreamMem *) istream, FALSE);
	
	/* create the output stream */
	actual = g_byte_array_new ();
	ostream = g_mime_stream_mem_new_with_byte_array (actual);
	g_mime_stream_mem_set_owner ((GMimeStreamMem *) ostream, FALSE);
	
	/* create the filtered stream that will do all of the decoding for us */
	filtered = g_mime_stream_filter_new (ostream);
	filter = g_mime_filter_basic_new (encoding, FALSE);
	g_mime_stream_filter_add ((GMimeStreamFilter *) filtered, filter);
	g_object_unref (filter);
	
	while ((nread = g_mime_stream_read (istream, buf, size)) > 0) {
		nwritten = 0;
		
		do {
			if ((n = g_mime_stream_write (filtered, buf + nwritten, nread - nwritten)) > 0)
				nwritten += n;
		} while (nwritten < nread);
	}
	
	g_mime_stream_flush (filtered);
	g_object_unref (filtered);
	g_object_unref (istream);
	g_object_unref (ostream);
	
	/* now let's compare the expected vs actual output */
	if (actual->len != expected->len) {
		testsuite_check_failed ("%s decoding failed: encoded content does not match: expected=%u; actual=%u",
					name, expected->len, actual->len);
		goto error;
	}
	
	if (memcmp (actual->data, expected->data, actual->len) != 0) {
		testsuite_check_failed ("%s decoding failed: encoded content does not match", name);
		goto error;
	}
	
	testsuite_check_passed ();
	
error:
	path = g_strdup_printf ("%s.decode.%zu.txt", name, size);
	ostream = g_mime_stream_fs_open (path, O_CREAT | O_TRUNC | O_RDWR, 0644, NULL);
	g_mime_stream_write (ostream, (const char *) actual->data, actual->len);
	g_mime_stream_flush (ostream);
	g_object_unref (ostream);
	
	g_byte_array_free (actual, TRUE);
}

int main (int argc, char **argv)
{
	const char *datadir = "data/encodings";
	GByteArray *photo, *b64, *uu;
	struct stat st;
	char *path;
	int i;
	
	g_mime_init ();
	
	testsuite_init (argc, argv);
	
	for (i = 1; i < argc; i++) {
		if (argv[i][0] != '-') {
			datadir = argv[i];
			break;
		}
	}
	
	if (i < argc && (stat (datadir, &st) == -1 || !S_ISDIR (st.st_mode)))
		return 0;
	
	path = g_build_filename (datadir, "photo.jpg", NULL);
	photo = read_all_bytes (path, FALSE);
	g_free (path);
	
	path = g_build_filename (datadir, "photo.b64", NULL);
	b64 = read_all_bytes (path, TRUE);
	g_free (path);
	
	path = g_build_filename (datadir, "photo.uu", NULL);
	uu = read_all_bytes (path, TRUE);
	g_free (path);
	
	testsuite_start ("base64");
	test_base64_decode_patterns ();
	test_encoder (GMIME_CONTENT_ENCODING_BASE64, photo, b64, 4096);
	test_encoder (GMIME_CONTENT_ENCODING_BASE64, photo, b64, 1024);
	test_encoder (GMIME_CONTENT_ENCODING_BASE64, photo, b64, 16);
	test_encoder (GMIME_CONTENT_ENCODING_BASE64, photo, b64, 1);
	test_decoder (GMIME_CONTENT_ENCODING_BASE64, b64, photo, 4096);
	test_decoder (GMIME_CONTENT_ENCODING_BASE64, b64, photo, 1024);
	test_decoder (GMIME_CONTENT_ENCODING_BASE64, b64, photo, 16);
	test_decoder (GMIME_CONTENT_ENCODING_BASE64, b64, photo, 1);
	testsuite_end ();
	
	testsuite_start ("uuencode");
	test_encoder (GMIME_CONTENT_ENCODING_UUENCODE, photo, uu, 4096);
	test_encoder (GMIME_CONTENT_ENCODING_UUENCODE, photo, uu, 1024);
	test_encoder (GMIME_CONTENT_ENCODING_UUENCODE, photo, uu, 16);
	test_encoder (GMIME_CONTENT_ENCODING_UUENCODE, photo, uu, 1);
	test_decoder (GMIME_CONTENT_ENCODING_UUENCODE, uu, photo, 4096);
	test_decoder (GMIME_CONTENT_ENCODING_UUENCODE, uu, photo, 1024);
	//test_decoder (GMIME_CONTENT_ENCODING_UUENCODE, uu, photo, 16);
	//test_decoder (GMIME_CONTENT_ENCODING_UUENCODE, uu, photo, 1);
	testsuite_end ();
	
	g_mime_shutdown ();
	
	return testsuite_exit ();
}
