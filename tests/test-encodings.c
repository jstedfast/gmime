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
#ifdef G_OS_UNIX
#include <sys/mman.h>
#endif

#include "testsuite.h"

extern int verbose;

#define d(x)
#define v(x) if (verbose > 3) x

static struct _mapping {
	const char *name;
	const char *text;
	GMimeContentEncoding encoding;
} mappings[] = {
	{ "\"7bit\"", "7bit", GMIME_CONTENT_ENCODING_7BIT },
	{ "\"7-bit\"", "7-bit", GMIME_CONTENT_ENCODING_7BIT },
	{ "\"8bit\"", "8bit", GMIME_CONTENT_ENCODING_8BIT },
	{ "\"8-bit\"", "8-bit", GMIME_CONTENT_ENCODING_8BIT },
	{ "\"binary\"", "binary", GMIME_CONTENT_ENCODING_BINARY },
	{ "\"base64\"", "base64", GMIME_CONTENT_ENCODING_BASE64 },
	{ "\"quoted-printable\"", "quoted-printable", GMIME_CONTENT_ENCODING_QUOTEDPRINTABLE },
	{ "\"uuencode\"", "uuencode", GMIME_CONTENT_ENCODING_UUENCODE },
	{ "\"x-uuencode\"", "x-uuencode", GMIME_CONTENT_ENCODING_UUENCODE },
	{ "\"x-uue\"", "x-uue", GMIME_CONTENT_ENCODING_UUENCODE },
	{ "\"garbage\"", "garbage", GMIME_CONTENT_ENCODING_UNSPECIFIED },
	{ "\" 7bit \"", " 7bit ", GMIME_CONTENT_ENCODING_7BIT },
	{ "NULL", NULL, GMIME_CONTENT_ENCODING_UNSPECIFIED },
	{ "empty", "", GMIME_CONTENT_ENCODING_UNSPECIFIED },
	{ "semicolon suffix", "bAsE64;anything", GMIME_CONTENT_ENCODING_BASE64 },
	{ "whitespace suffix", "7bit garbage", GMIME_CONTENT_ENCODING_7BIT },
	{ "hyphenated semicolon suffix", "7-BiT;anything", GMIME_CONTENT_ENCODING_7BIT },
	{ "hyphenated Unicode whitespace", "\302\2058-bIt\343\200\200suffix", GMIME_CONTENT_ENCODING_8BIT },
	{ "longer hyphenated token", "7-bitx", GMIME_CONTENT_ENCODING_UNSPECIFIED },
	{ "hyphenated embedded comment", "8-bit(comment)", GMIME_CONTENT_ENCODING_UNSPECIFIED },
	{ "empty token", ";base64", GMIME_CONTENT_ENCODING_UNSPECIFIED },
	{ "leading comment", "(comment) base64", GMIME_CONTENT_ENCODING_UNSPECIFIED },
	{ "embedded comment", "base64(comment)", GMIME_CONTENT_ENCODING_UNSPECIFIED },
	{ "separated comment", "base64 (comment)", GMIME_CONTENT_ENCODING_BASE64 },
	{ "longer token", "base64x", GMIME_CONTENT_ENCODING_UNSPECIFIED },
	{ "shorter token", "base6", GMIME_CONTENT_ENCODING_UNSPECIFIED },
	{ "Unicode whitespace", "\302\205\342\200\257X-UUE\343\200\200suffix", GMIME_CONTENT_ENCODING_UUENCODE },
	{ "non-whitespace separator", "base64\342\200\213", GMIME_CONTENT_ENCODING_UNSPECIFIED },
	{ "truncated UTF-8 prefix", "\302", GMIME_CONTENT_ENCODING_UNSPECIFIED },
	{ "truncated UTF-8 name", "base64\342\200", GMIME_CONTENT_ENCODING_UNSPECIFIED },
	{ "invalid UTF-8 name", "base64\377", GMIME_CONTENT_ENCODING_UNSPECIFIED },
	{ "ignored invalid suffix", "base64;\377", GMIME_CONTENT_ENCODING_BASE64 }
};

static void
test_content_encoding_mappings (void)
{
	GMimeContentEncoding encoding;
	guint i;
	
	g_assert_cmpint (GMIME_CONTENT_ENCODING_UNSPECIFIED, ==, 0);
	g_assert_null (g_mime_content_encoding_to_string (GMIME_CONTENT_ENCODING_UNSPECIFIED));
	g_assert_cmpstr (g_mime_content_encoding_to_string (g_mime_content_encoding_from_string ("7-bit")), ==, "7bit");
	g_assert_cmpstr (g_mime_content_encoding_to_string (g_mime_content_encoding_from_string ("8-bit")), ==, "8bit");

	for (i = 0; i < G_N_ELEMENTS (mappings); i++) {
		testsuite_check ("%s", mappings[i].name);
		encoding = g_mime_content_encoding_from_string (mappings[i].text);
		if (encoding != mappings[i].encoding)
			testsuite_check_failed ("failed: expected: %s; was: %s",
						g_mime_content_encoding_to_string (mappings[i].encoding),
						g_mime_content_encoding_to_string (encoding));
		else
			testsuite_check_passed ();
	}
}

static void
test_content_encoding_bounds (void)
{
#ifdef G_OS_UNIX
	gsize page = sysconf (_SC_PAGESIZE), length, n;
	guint i;
	char *map, *input, *copy;
	GMimeContentEncoding expected;

	testsuite_check ("Content-Transfer-Encoding names at a read-only guard-page boundary");
	map = mmap (NULL, page * 3, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
	g_assert_true (map != MAP_FAILED);
	for (i = 0; i < G_N_ELEMENTS (mappings); i++) {
		if (!mappings[i].text)
			continue;
		length = strlen (mappings[i].text);
		for (n = 0; n <= length; n++) {
			copy = g_strndup (mappings[i].text, n);
			expected = g_mime_content_encoding_from_string (copy);
			input = map + 2 * page - n - 1;
			g_assert_cmpint (mprotect (map + page, page, PROT_READ | PROT_WRITE), ==, 0);
			memcpy (input, copy, n + 1);
			g_assert_cmpint (mprotect (map + page, page, PROT_READ), ==, 0);
			g_assert_cmpint (g_mime_content_encoding_from_string (input), ==, expected);
			g_free (copy);
		}
	}
	g_assert_cmpint (munmap (map, page * 3), ==, 0);
	testsuite_check_passed ();
#endif
}

static void
test_content_encoding_parser (void)
{
	static const struct {
		const char *name;
		GMimeContentEncoding encoding;
		const char *decoded;
	} cases[] = {
		{ "base64;", GMIME_CONTENT_ENCODING_BASE64, "Hello" },
		{ "\302\240BaSe64\343\200\200suffix", GMIME_CONTENT_ENCODING_BASE64, "Hello" },
		{ "base64 (comment)", GMIME_CONTENT_ENCODING_BASE64, "Hello" },
		{ "7-bit", GMIME_CONTENT_ENCODING_7BIT, "SGVsbG8=" },
		{ "8-bit", GMIME_CONTENT_ENCODING_8BIT, "SGVsbG8=" },
		{ "7-BiT;suffix", GMIME_CONTENT_ENCODING_7BIT, "SGVsbG8=" },
		{ "\302\2408-bIt\343\200\200suffix", GMIME_CONTENT_ENCODING_8BIT, "SGVsbG8=" }
	};
	GMimeStream *source, *decoded;
	GMimeParser *parser;
	GMimeEntity *entity;
	GMimeContent *content;
	GByteArray *bytes;
	GError *error = NULL;
	char *text;
	guint i;

	testsuite_check ("Parsed content uses encoding-name delimiters and legacy hyphenated aliases");
	for (i = 0; i < G_N_ELEMENTS (cases); i++) {
		text = g_strdup_printf ("Content-Type: text/plain\nContent-Transfer-Encoding: %s\n\nSGVsbG8=", cases[i].name);
		source = g_mime_stream_mem_new_with_buffer (text, strlen (text));
		parser = g_mime_parser_new_with_stream (source);
		entity = g_mime_parser_construct_part (parser, NULL, &error);
		g_assert_no_error (error);
		g_assert_true (GMIME_IS_PART (entity));
		g_assert_cmpint (g_mime_part_get_content_encoding (GMIME_PART (entity)), ==, cases[i].encoding);
		content = g_mime_part_get_content (GMIME_PART (entity));
		g_assert_cmpint (g_mime_content_get_encoding (content), ==, cases[i].encoding);
		decoded = g_mime_stream_mem_new ();
		g_assert_cmpint (g_mime_content_decode_to_stream (content, decoded), ==, strlen (cases[i].decoded));
		bytes = g_mime_stream_mem_get_byte_array (GMIME_STREAM_MEM (decoded));
		g_assert_cmpmem (bytes->data, bytes->len, cases[i].decoded, strlen (cases[i].decoded));
		g_object_unref (decoded);
		g_object_unref (entity);
		g_object_unref (parser);
		g_object_unref (source);
		g_free (text);
	}
	testsuite_check_passed ();
}

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
	GMimeDecoder *decoder;
	char output[4096];
	size_t len, n;
	guint i, j;
	
	decoder = GMIME_DECODER (g_mime_base64_decoder_new ());
	
	for (i = 0; i < G_N_ELEMENTS (base64_encoded_patterns); i++) {
		testsuite_check ("base64_encoded_patterns[%u]", i);
		
		expected = base64_decoded_patterns[i];
		input = base64_encoded_patterns[i];
		len = strlen (input);
		
		n = g_mime_decoder_decode (decoder, (const guint8 *) input, len, (guint8 *) output, sizeof (output));
		len = strlen (expected);
		
		if (n != len)
			testsuite_check_failed ("base64_encoded_patterns[%u] failed: decoded lengths did not match (expected: %zu, was: %zu)", i, len, n);
		else if (strncmp (expected, output, n) != 0) {
			output[n] = 0;
			testsuite_check_failed ("base64_encoded_patterns[%u] failed: decoded values did not match: %s", i, output);
		} else
			testsuite_check_passed ();
		
		g_mime_decoder_reset (decoder);
	}
	
	for (i = 0; i < G_N_ELEMENTS (base64_encoded_long_patterns); i++) {
		testsuite_check ("base64_encoded_long_patterns[%u]", i);
		
		input = base64_encoded_long_patterns[i];
		len = strlen (input);
		
		n = g_mime_decoder_decode (decoder, (const guint8 *) input, len, (guint8 *) output, sizeof (output));
		len = strlen (expected);
		
		for (j = 0; j < n; j++) {
			if (output[j] != (char) (j + i))
				break;
		}
		
		if (j < n)
			testsuite_check_failed ("base64_encoded_long_patterns[%u] failed: decoded values did not match at index %u", i, j);
		else
			testsuite_check_passed ();
		
		g_mime_decoder_reset (decoder);
	}
	g_object_unref (decoder);
}

static const char *qp_encoded_patterns[] = {
	"=e1=e2=E3=E4\r\n",
	"=e1=g2=E3=E4\r\n",
	"=e1=eg=E3=E4\r\n",
	"   =e1 =e2  =E3\t=E4  \t \t    \r\n",
	"Soft line=\r\n\tHard line\r\n",
	"width==\r\n340 height=3d200\r\n",
	
};

static const char *qp_decoded_patterns[] = {
	"\u00e1\u00e2\u00e3\u00e4\r\n",
	"\u00e1=g2\u00e3\u00e4\r\n",
	"\u00e1=eg\u00e3\u00e4\r\n",
	"   \u00e1 \u00e2  \u00e3\t\u00e4  \t \t    \r\n",
	"Soft line\tHard line\r\n",
	"width=340 height=200\r\n"
};

static void
test_quoted_printable_decode_patterns (void)
{
	const char *input;
	GMimeDecoder *decoder;
	char output[4096];
	char *expected;
	size_t len, n;
	iconv_t cd;
	guint i;
	
	decoder = GMIME_DECODER (g_mime_quoted_printable_decoder_new (FALSE));
	cd = g_mime_iconv_open ("iso-8859-1", "utf-8");
	
	for (i = 0; i < G_N_ELEMENTS (base64_encoded_patterns); i++) {
		testsuite_check ("qp_encoded_patterns[%u]", i);
		
		expected = g_mime_iconv_strdup (cd, qp_decoded_patterns[i]);
		input = qp_encoded_patterns[i];
		len = strlen (input);
		
		n = g_mime_decoder_decode (decoder, (const guint8 *) input, len, (guint8 *) output, sizeof (output));
		len = strlen (expected);
		
		if (n != len)
			testsuite_check_failed ("qp_encoded_patterns[%u] failed: decoded lengths did not match (expected: %zu, was: %zu, value: \"%s\")", i, len, n, expected);
		else if (strncmp (expected, output, n) != 0)
			testsuite_check_failed ("qp_encoded_patterns[%u] failed: decoded values did not match", i);
		else
			testsuite_check_passed ();
		
		g_mime_decoder_reset (decoder);
		g_free (expected);
	}
	
	g_mime_iconv_close (cd);
	g_object_unref (decoder);
}

static void
test_quoted_printable_encode_space_dos_linebreak (void)
{
	const char *input = "This line ends with a space \r\nbefore a line break.";
	const char *expected = "This line ends with a space=20\nbefore a line break.=\n";
	GMimeEncoder *encoder;
	char output[4096];
	size_t len, n;
	
	testsuite_check ("quoted-printable encode <SPACE><CR><LF>");
	
	encoder = GMIME_ENCODER (g_mime_quoted_printable_encoder_new ());
	
	n = g_mime_encoder_flush (encoder, (const guint8 *) input, strlen (input), (guint8 *) output, sizeof (output));
	g_object_unref (encoder);
	len = strlen (expected);
	
	if (n != len)
		testsuite_check_failed ("failed: encoded lengths did not match (expected: %zu, was: %zu)", len, n);
	else if (strncmp (expected, output, n) != 0)
		testsuite_check_failed ("failed: encoded values did not match");
	else
		testsuite_check_passed ();
}

static void
test_quoted_printable_encode_space_unix_linebreak (void)
{
	const char *input = "This line ends with a space \nbefore a line break.";
	const char *expected = "This line ends with a space=20\nbefore a line break.=\n";
	GMimeEncoder *encoder;
	char output[4096];
	size_t len, n;
	
	testsuite_check ("quoted-printable encode <SPACE><LF>");
	
	encoder = GMIME_ENCODER (g_mime_quoted_printable_encoder_new ());
	
	n = g_mime_encoder_flush (encoder, (const guint8 *) input, strlen (input), (guint8 *) output, sizeof (output));
	g_object_unref (encoder);
	len = strlen (expected);
	
	if (n != len)
		testsuite_check_failed ("failed: encoded lengths did not match (expected: %zu, was: %zu)", len, n);
	else if (strncmp (expected, output, n) != 0)
		testsuite_check_failed ("failed: encoded values did not match");
	else
		testsuite_check_passed ();
}

static void
test_quoted_printable_encode_ending_with_space (void)
{
	const char *input = "This line ends with a space ";
	const char *expected = "This line ends with a space=20=\n";
	GMimeEncoder *encoder;
	char output[4096];
	size_t len, n;
	
	testsuite_check ("quoted-printable encode encoding with a space");
	
	encoder = GMIME_ENCODER (g_mime_quoted_printable_encoder_new ());
	
	n = g_mime_encoder_flush (encoder, (const guint8 *) input, strlen (input), (guint8 *) output, sizeof (output));
	g_object_unref (encoder);
	len = strlen (expected);
	
	if (n != len)
		testsuite_check_failed ("failed: encoded lengths did not match (expected: %zu, was: %zu)", len, n);
	else if (strncmp (expected, output, n) != 0)
		testsuite_check_failed ("failed: encoded values did not match");
	else
		testsuite_check_passed ();
}

static void
test_quoted_printable_decode_invalid_soft_break (void)
{
	const char *input = "This is an invalid=\rsoft break.";
	const char *expected = "This is an invalid=\rsoft break.";
	GMimeDecoder *decoder;
	char output[4096];
	size_t len, n;
	
	testsuite_check ("quoted-printable decode invalid soft break");
	
	decoder = GMIME_DECODER (g_mime_quoted_printable_decoder_new (FALSE));
	
	n = g_mime_decoder_decode (decoder, (const guint8 *) input, strlen (input), (guint8 *) output, sizeof (output));
	g_object_unref (decoder);
	len = strlen (expected);
	
	if (n != len)
		testsuite_check_failed ("failed: encoded lengths did not match (expected: %zu, was: %zu)", len, n);
	else if (strncmp (expected, output, n) != 0)
		testsuite_check_failed ("failed: encoded values did not match");
	else
		testsuite_check_passed ();
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
	char *buf = g_alloca (size);
	ssize_t nread, nwritten, n;
	GMimeFilter *filter;
	GByteArray *actual;
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
	
	g_byte_array_free (actual, TRUE);
	
	return;
	
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
	char *buf = g_alloca (size);
	ssize_t nread, nwritten, n;
	GMimeFilter *filter;
	GByteArray *actual;
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
	
	g_byte_array_free (actual, TRUE);
	
	return;
	
error:
	path = g_strdup_printf ("%s.decode.%zu.txt", name, size);
	ostream = g_mime_stream_fs_open (path, O_CREAT | O_TRUNC | O_RDWR, 0644, NULL);
	g_mime_stream_write (ostream, (const char *) actual->data, actual->len);
	g_mime_stream_flush (ostream);
	g_object_unref (ostream);
	
	g_byte_array_free (actual, TRUE);
}

static void
check_yenc_filter_input (GMimeFilter *filter, const char *text, const char *expected)
{
	GByteArray *actual = g_byte_array_new ();
	char *input = g_strdup (text), *output;
	size_t input_length = strlen (text), length, prespace, split, i;

	for (split = 0; split <= input_length + 1; split++) {
		g_mime_filter_reset (filter);
		g_byte_array_set_size (actual, 0);
		if (split <= input_length) {
			g_mime_filter_filter (filter, input, split, 0, &output, &length, &prespace);
			g_byte_array_append (actual, (guint8 *) output, length);
			g_mime_filter_filter (filter, "", 0, 0, &output, &length, &prespace);
			g_assert_cmpuint (length, ==, 0);
			g_mime_filter_complete (filter, input + split, input_length - split, 0,
						&output, &length, &prespace);
		} else {
			for (i = 0; i < input_length; i++) {
				g_mime_filter_filter (filter, input + i, 1, 0, &output, &length, &prespace);
				g_byte_array_append (actual, (guint8 *) output, length);
			}
			g_mime_filter_complete (filter, "", 0, 0, &output, &length, &prespace);
		}
		g_byte_array_append (actual, (guint8 *) output, length);
		g_assert_cmpmem (actual->data, actual->len, expected, strlen (expected));
		g_mime_filter_complete (filter, "", 0, 0, &output, &length, &prespace);
		g_assert_cmpuint (length, ==, 0);
	}
	g_free (input);
	g_byte_array_unref (actual);
}

static void
test_yenc_filters (void)
{
	static const struct {
		gboolean payload_only;
		const char *input;
		const char *expected;
		guint32 checksum;
	} cases[] = {
		{ FALSE, "pretext\r\n=ybegin line=128 size=9 name=x\r\n"
			"[\\]^_`abc\r\n=yend size=9 crc32=cbf43926\r\nignored",
			"123456789", 0xcbf43926 },
		{ FALSE, "=ybegin part=1 line=128 size=9 name=x\n"
			"=ypart begin=1 end=9\n[\\]^_`abc\n=yend size=9\nignored",
			"123456789", 0xcbf43926 },
		{ FALSE, "not =ybegin line=128 size=9 name=x\n"
			"=ybeginx\n=ybegin line=128 size=9 name=x\n[\\]^_`abc\n",
			"123456789", 0xcbf43926 },
		{ TRUE, "[\\]^_`abc\r\n=yend size=9\nignored", "123456789", 0xcbf43926 },
		{ FALSE, "=ybegin line=128 size=0 name=x\n=yend size=0\nignored", "", 0 },
		{ TRUE, "", "", 0 }
	};
	GMimeYencEncoder *encoder;
	GMimeYencDecoder *decoder;
	GMimeFilter *filter;
	char *output;
	size_t length, prespace;
	guint i;

	for (i = 0; i < G_N_ELEMENTS (cases); i++) {
		testsuite_check ("yEnc filter framing and checksum (%u)", i);
		decoder = g_mime_yenc_decoder_new (cases[i].payload_only);
		filter = g_mime_filter_basic_new_with_decoder (GMIME_DECODER (decoder));
		check_yenc_filter_input (filter, cases[i].input, cases[i].expected);
		g_assert_cmphex (~g_mime_yenc_decoder_get_checksum (decoder), ==, cases[i].checksum);
		g_mime_filter_reset (filter);
		g_assert_cmphex (g_mime_yenc_decoder_get_checksum (decoder), ==, G_MAXUINT32);
		g_object_unref (filter);
		g_object_unref (decoder);
		testsuite_check_passed ();
	}

	testsuite_check ("yEnc filter escaping, completion, and checksum lifetime");
	encoder = g_mime_yenc_encoder_new ();
	filter = g_mime_filter_basic_new_with_encoder (GMIME_ENCODER (encoder));
	check_yenc_filter_input (filter, "\004", "=n\n");
	check_yenc_filter_input (filter, "123456789", "[\\]^_`abc\n");
	g_assert_cmphex (~g_mime_yenc_encoder_get_checksum (encoder), ==, 0xcbf43926);
	g_mime_filter_reset (filter);
	g_assert_cmphex (g_mime_yenc_encoder_get_checksum (encoder), ==, G_MAXUINT32);
	g_mime_filter_complete (filter, "1234", 4, 0, &output, &length, &prespace);
	g_assert_cmpmem (output, length, "[\\]^\n", 5);
	g_mime_filter_complete (filter, "56789", 5, 0, &output, &length, &prespace);
	g_assert_cmpmem (output, length, "_`abc\n", 6);
	g_assert_cmphex (~g_mime_yenc_encoder_get_checksum (encoder), ==, 0xcbf43926);
	g_object_unref (filter);
	g_object_unref (encoder);
	testsuite_check_passed ();
}

static void
test_codec_filter_copies (void)
{
	GMimeEncoder *encoder;
	GMimeDecoder *decoder;
	GMimeFilter *filter, *copy;
	guint8 scratch[128];
	char input[49], *output;
	size_t length, prespace;

	testsuite_check ("codec filter ownership, state, configuration, and reset copies");
	encoder = GMIME_ENCODER (g_mime_base64_encoder_new_with_max_line_length (60));
	g_mime_encoder_encode (encoder, (const guint8 *) "x", 1, scratch, sizeof (scratch));
	filter = g_mime_filter_basic_new_with_encoder (encoder);
	g_object_unref (encoder);
	copy = g_mime_filter_copy (filter);
	g_mime_filter_complete (filter, "yz", 2, 0, &output, &length, &prespace);
	g_assert_cmpmem (output, length, "eHl6\n", 5);
	g_mime_filter_complete (copy, "yz", 2, 0, &output, &length, &prespace);
	g_assert_cmpmem (output, length, "eXo=\n", 5);
	g_mime_filter_reset (filter);
	g_mime_filter_complete (filter, "yz", 2, 0, &output, &length, &prespace);
	g_assert_cmpmem (output, length, "eXo=\n", 5);
	memset (input, 'A', sizeof (input));
	g_mime_filter_reset (copy);
	g_mime_filter_complete (copy, input, sizeof (input), 0, &output, &length, &prespace);
	g_assert_true (memchr (output, '\n', length) == output + 60);
	g_object_unref (filter);
	g_object_unref (copy);

	decoder = GMIME_DECODER (g_mime_uu_decoder_new (TRUE));
	g_mime_decoder_decode (decoder, (const guint8 *) "#0", 2, scratch, sizeof (scratch));
	filter = g_mime_filter_basic_new_with_decoder (decoder);
	g_object_unref (decoder);
	copy = g_mime_filter_copy (filter);
	g_mime_filter_complete (filter, "V%T\n`\n", 6, 0, &output, &length, &prespace);
	g_assert_cmpmem (output, length, "Cat", 3);
	g_mime_filter_complete (copy, "#0V%T\n`\n", 8, 0, &output, &length, &prespace);
	g_assert_cmpmem (output, length, "Cat", 3);
	g_mime_filter_reset (filter);
	g_mime_filter_complete (filter, "#0V%T\n`\n", 8, 0, &output, &length, &prespace);
	g_assert_cmpmem (output, length, "Cat", 3);
	g_object_unref (filter);
	g_object_unref (copy);

	encoder = GMIME_ENCODER (g_mime_yenc_encoder_new_with_max_line_length (60));
	g_mime_encoder_encode (encoder, (const guint8 *) "1", 1, scratch, sizeof (scratch));
	filter = g_mime_filter_basic_new_with_encoder (encoder);
	copy = g_mime_filter_copy (filter);
	g_mime_filter_complete (filter, "23456789", 8, 0, &output, &length, &prespace);
	g_assert_cmpmem (output, length, "\\]^_`abc\n", 9);
	g_assert_cmphex (g_mime_yenc_encoder_get_checksum (GMIME_YENC_ENCODER (encoder)), ==, 0x340bc6d9);
	g_mime_filter_complete (copy, "123456789", 9, 0, &output, &length, &prespace);
	g_assert_cmpmem (output, length, "[\\]^_`abc\n", 10);
	g_mime_filter_reset (filter);
	g_assert_cmphex (g_mime_yenc_encoder_get_checksum (GMIME_YENC_ENCODER (encoder)), ==, G_MAXUINT32);
	g_object_unref (encoder);
	memset (scratch, 23, 61);
	g_mime_filter_reset (copy);
	g_mime_filter_complete (copy, (char *) scratch, 61, 0, &output, &length, &prespace);
	g_assert_cmpuint (length, ==, 63);
	g_assert_true (memchr (output, '\n', length) == output + 60);
	g_object_unref (filter);
	g_object_unref (copy);

	decoder = GMIME_DECODER (g_mime_yenc_decoder_new (TRUE));
	g_mime_decoder_decode (decoder, (const guint8 *) "=", 1, scratch, sizeof (scratch));
	filter = g_mime_filter_basic_new_with_decoder (decoder);
	g_object_unref (decoder);
	copy = g_mime_filter_copy (filter);
	g_mime_filter_complete (filter, "nklm\n=yend\nklm\n", 15, 0, &output, &length, &prespace);
	g_assert_cmpmem (output, length, "\004ABC", 4);
	g_mime_filter_complete (copy, "=nklm\n=yend\nklm\n", 16, 0, &output, &length, &prespace);
	g_assert_cmpmem (output, length, "\004ABC", 4);
	g_mime_filter_reset (filter);
	g_mime_filter_complete (filter, "klm\n", 4, 0, &output, &length, &prespace);
	g_assert_cmpmem (output, length, "ABC", 3);
	g_object_unref (filter);
	g_object_unref (copy);
	testsuite_check_passed ();
}

int main (int argc, char **argv)
{
	const char *datadir = "data/encodings";
	GByteArray *photo, *b64, *uu;
	GByteArray *wikipedia, *qp;
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
	
	path = g_build_filename (datadir, "wikipedia.txt", NULL);
	wikipedia = read_all_bytes (path, TRUE);
	g_free (path);
	
	path = g_build_filename (datadir, "wikipedia.qp", NULL);
	qp = read_all_bytes (path, TRUE);
	g_free (path);
	
	testsuite_start ("Content-Transfer-Encoding");
	test_content_encoding_mappings ();
	test_content_encoding_bounds ();
	test_content_encoding_parser ();
	test_codec_filter_copies ();
	test_yenc_filters ();
	testsuite_end ();
	
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
	test_decoder (GMIME_CONTENT_ENCODING_UUENCODE, uu, photo, 16);
	test_decoder (GMIME_CONTENT_ENCODING_UUENCODE, uu, photo, 1);
	testsuite_end ();
	
	testsuite_start ("quoted-printable");
	test_quoted_printable_decode_patterns ();
	test_quoted_printable_encode_space_dos_linebreak ();
	test_quoted_printable_encode_space_unix_linebreak ();
	test_quoted_printable_encode_ending_with_space ();
	test_quoted_printable_decode_invalid_soft_break ();
	test_encoder (GMIME_CONTENT_ENCODING_QUOTEDPRINTABLE, wikipedia, qp, 4096);
	test_encoder (GMIME_CONTENT_ENCODING_QUOTEDPRINTABLE, wikipedia, qp, 1024);
	test_encoder (GMIME_CONTENT_ENCODING_QUOTEDPRINTABLE, wikipedia, qp, 16);
	test_encoder (GMIME_CONTENT_ENCODING_QUOTEDPRINTABLE, wikipedia, qp, 1);
	test_decoder (GMIME_CONTENT_ENCODING_QUOTEDPRINTABLE, qp, wikipedia, 4096);
	test_decoder (GMIME_CONTENT_ENCODING_QUOTEDPRINTABLE, qp, wikipedia, 1024);
	test_decoder (GMIME_CONTENT_ENCODING_QUOTEDPRINTABLE, qp, wikipedia, 16);
	test_decoder (GMIME_CONTENT_ENCODING_QUOTEDPRINTABLE, qp, wikipedia, 1);
	testsuite_end ();
	
	g_byte_array_free (wikipedia, TRUE);
	g_byte_array_free (photo, TRUE);
	g_byte_array_free (b64, TRUE);
	g_byte_array_free (uu, TRUE);
	g_byte_array_free (qp, TRUE);
	
	g_mime_shutdown ();
	
	return testsuite_exit ();
}
