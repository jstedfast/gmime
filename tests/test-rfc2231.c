/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*  GMime
 *  Copyright (C) 2026 Jeffrey Stedfast
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

#include <gmime/gmime-codec-private.h>
#include <string.h>
#ifdef G_OS_UNIX
#include <sys/mman.h>
#include <unistd.h>
#endif

static void
assert_padding (const guint8 *output, gsize written, gsize capacity)
{
	gsize i;

	g_assert_cmpuint (written, <=, capacity);
	for (i = written; i <= capacity; i++)
		g_assert_cmpuint (output[i], ==, 0xa5);
}

static void
test_octets (void)
{
	guint8 input[256], encoded[769], decoded[257];
	char expected[769], *p = expected;
	GMimeDecoder *rfc2231 = _g_mime_rfc2231_decoder_new ();
	gsize length, written;
	guint i;

	g_assert_cmpstr (G_OBJECT_TYPE_NAME (rfc2231), ==, "GMimeRfc2231Decoder");

	for (i = 0; i < 256; i++) {
		input[i] = i;
		if (g_ascii_isalnum (i) || (i && strchr ("!#$&+-.^_`{|}~", i)))
			*p++ = i;
		else
			p += g_snprintf (p, 4, "%%%02X", i);
	}

	memset (encoded, 0xa5, sizeof (encoded));
	length = _g_mime_rfc2231_encode (input, sizeof (input), encoded, sizeof (encoded) - 1);
	g_assert_cmpmem (encoded, length, expected, (p - expected));
	assert_padding (encoded, length, sizeof (encoded) - 1);
	g_assert_cmpint (g_mime_decoder_get_encoding (rfc2231), ==, GMIME_CONTENT_ENCODING_UNSPECIFIED);

	memset (decoded, 0xa5, sizeof (decoded));
	written = 0;
	for (i = 0; i < length; i++) {
		guint8 output[4];
		gsize capacity = g_mime_decoder_estimate_output_length (rfc2231, 1);
		gsize n;

		memset (output, 0xa5, sizeof (output));
		n = g_mime_decoder_decode (rfc2231, encoded + i, 1, output, capacity);
		assert_padding (output, n, capacity);
		memcpy (decoded + written, output, n);
		written += n;
	}
	g_assert_cmpmem (decoded, written, input, sizeof (input));
	assert_padding (decoded, written, sizeof (decoded) - 1);
	g_object_unref (rfc2231);
}

static void
test_recovery (void)
{
	static const struct {
		const char *input;
		const char *expected;
	} cases[] = {
		{ "", "" },
		{ "%", "" },
		{ "%4", "" },
		{ "%g", "" },
		{ "hello+world", "hello+world" },
		{ "%20%41%7a%7E", " Az~" },
		{ "a%b", "a" },
		{ "%GG%41", "%GGA" },
		{ "%G%41", "%G%41" },
		{ "%%41", "%%41" },
		{ "%%%41", "%%%41" },
		{ "abc%4z%41%", "abc%4zA" },
		{ "%\r\n%20", "%\r\n " }
	};
	GMimeDecoder *rfc2231, *clone;
	guint8 a[128], b[128], prefix[128];
	gsize length, split, n, m, offset, count, capacity, written, chunk;
	guint i;

	for (i = 0; i < G_N_ELEMENTS (cases); i++) {
		length = strlen (cases[i].input);
		for (chunk = 1; chunk <= MAX (length, 1); chunk++) {
			rfc2231 = _g_mime_rfc2231_decoder_new ();
			written = 0;
			for (offset = 0; offset < length; offset += count) {
				count = MIN (chunk, length - offset);
				capacity = g_mime_decoder_estimate_output_length (rfc2231, count);
				memset (a, 0xa5, sizeof (a));
				n = g_mime_decoder_decode (rfc2231, (const guint8 *) cases[i].input + offset,
							  count, a, capacity);
				assert_padding (a, n, capacity);
				memcpy (b + written, a, n);
				written += n;
			}
			capacity = g_mime_decoder_estimate_output_length (rfc2231, 0);
			g_assert_cmpuint (g_mime_decoder_decode (rfc2231, NULL, 0, a, capacity), ==, 0);
			g_assert_cmpmem (b, written, cases[i].expected, strlen (cases[i].expected));
			g_object_unref (rfc2231);
		}

		for (split = 0; split <= length; split++) {
			rfc2231 = _g_mime_rfc2231_decoder_new ();
			written = g_mime_decoder_decode (rfc2231, (const guint8 *) cases[i].input,
							split, prefix, sizeof (prefix));
			clone = g_mime_decoder_clone (rfc2231);
			capacity = g_mime_decoder_estimate_output_length (rfc2231, length - split);
			g_assert_cmpuint (capacity, ==, g_mime_decoder_estimate_output_length (clone, length - split));
			n = g_mime_decoder_decode (rfc2231, (const guint8 *) cases[i].input + split,
						  length - split, a, capacity);
			m = g_mime_decoder_decode (clone, (const guint8 *) cases[i].input + split,
						  length - split, b, capacity);
			g_assert_cmpmem (a, n, b, m);
			memcpy (prefix + written, a, n);
			g_assert_cmpmem (prefix, written + n, cases[i].expected, strlen (cases[i].expected));
			g_mime_decoder_reset (rfc2231);
			n = g_mime_decoder_decode (rfc2231, (const guint8 *) cases[i].input, length, a, sizeof (a));
			g_assert_cmpmem (a, n, cases[i].expected, strlen (cases[i].expected));
			g_object_unref (clone);
			g_object_unref (rfc2231);
		}
	}
}

static void
test_estimates_and_preconditions (void)
{
	GMimeDecoder *rfc2231 = _g_mime_rfc2231_decoder_new ();
	guint8 output[8];
	guint state;

	g_assert_cmpuint (_g_mime_rfc2231_encoder_estimate_output_length (G_MAXSIZE / 3), ==, (G_MAXSIZE / 3) * 3);
	g_assert_cmpuint (_g_mime_rfc2231_encoder_estimate_output_length (G_MAXSIZE / 3 + 1), ==, G_MAXSIZE);
	g_assert_cmpuint (_g_mime_rfc2231_encode (NULL, 0, NULL, 0), ==, 0);

	g_test_expect_message ("gmime-tests", G_LOG_LEVEL_CRITICAL, "*capacity >= estimate*");
	g_assert_cmpuint (_g_mime_rfc2231_encode ((const guint8 *) "%", 1, output, 2), ==, 0);
	g_test_assert_expected_messages ();
	g_test_expect_message ("gmime-tests", G_LOG_LEVEL_CRITICAL, "*estimate != G_MAXSIZE*");
	g_assert_cmpuint (_g_mime_rfc2231_encode (output, G_MAXSIZE, output, G_MAXSIZE), ==, 0);
	g_test_assert_expected_messages ();
	g_test_expect_message ("gmime-tests", G_LOG_LEVEL_CRITICAL, "*input != NULL*");
	g_assert_cmpuint (_g_mime_rfc2231_encode (NULL, 1, output, sizeof (output)), ==, 0);
	g_test_assert_expected_messages ();
	g_test_expect_message ("gmime-tests", G_LOG_LEVEL_CRITICAL, "*output != NULL*");
	g_assert_cmpuint (_g_mime_rfc2231_encode (output, 1, NULL, 3), ==, 0);
	g_test_assert_expected_messages ();

	for (state = 0; state < 3; state++) {
		g_mime_decoder_reset (rfc2231);
		g_mime_decoder_decode (rfc2231, (const guint8 *) "%G", state, output, sizeof (output));
		g_assert_cmpuint (g_mime_decoder_estimate_output_length (rfc2231, 0), ==, state);
		g_assert_cmpuint (g_mime_decoder_estimate_output_length (rfc2231, G_MAXSIZE - state), ==, G_MAXSIZE);
		g_assert_cmpuint (g_mime_decoder_estimate_output_length (rfc2231, G_MAXSIZE), ==, G_MAXSIZE);
		g_assert_cmpuint (g_mime_decoder_decode (rfc2231, NULL, 0, output, state), ==, 0);
		if (state) {
			g_test_expect_message ("gmime", G_LOG_LEVEL_CRITICAL, "*output_capacity >= estimate*");
			g_assert_cmpuint (g_mime_decoder_decode (rfc2231, NULL, 0, NULL, 0), ==, 0);
			g_test_assert_expected_messages ();
		} else {
			g_assert_cmpuint (g_mime_decoder_decode (rfc2231, NULL, 0, NULL, 0), ==, 0);
		}
		g_test_expect_message ("gmime", G_LOG_LEVEL_CRITICAL, "*output_capacity >= estimate*");
		g_assert_cmpuint (g_mime_decoder_decode (rfc2231, (const guint8 *) "z", 1, output, state), ==, 0);
		g_test_assert_expected_messages ();
		g_assert_cmpuint (g_mime_decoder_estimate_output_length (rfc2231, 0), ==, state);
		g_test_expect_message ("gmime", G_LOG_LEVEL_CRITICAL, "*estimate != G_MAXSIZE*");
		g_assert_cmpuint (g_mime_decoder_decode (rfc2231, output, G_MAXSIZE, output, G_MAXSIZE), ==, 0);
		g_test_assert_expected_messages ();
		g_assert_cmpuint (g_mime_decoder_estimate_output_length (rfc2231, 0), ==, state);
	}
	g_object_unref (rfc2231);
}

static void
test_overlap (void)
{
	static const guint8 input[] = "%41%20%%41%gZ%7E%";
	guint8 buffer[128], expected[128], scratch[4];
	GMimeDecoder *rfc2231 = _g_mime_rfc2231_decoder_new ();
	gsize n, m, length = sizeof (input) - 1, capacity;
	gint delta;
	guint state;

	for (delta = -8; delta <= 8; delta++) {
		memset (buffer, 0xa5, sizeof (buffer));
		memcpy (buffer + 32, input, length);
		capacity = _g_mime_rfc2231_encoder_estimate_output_length (length);
		n = _g_mime_rfc2231_encode (input, length, expected, capacity);
		m = _g_mime_rfc2231_encode (buffer + 32, length, buffer + 32 + delta, capacity);
		g_assert_cmpmem (buffer + 32 + delta, m, expected, n);

		for (state = 0; state < 3; state++) {
			g_mime_decoder_reset (rfc2231);
			g_mime_decoder_decode (rfc2231, (const guint8 *) "%G", state, scratch, sizeof (scratch));
			capacity = g_mime_decoder_estimate_output_length (rfc2231, length);
			n = g_mime_decoder_decode (rfc2231, input, length, expected, capacity);
			g_mime_decoder_reset (rfc2231);
			g_mime_decoder_decode (rfc2231, (const guint8 *) "%G", state, scratch, sizeof (scratch));
			memcpy (buffer + 32, input, length);
			m = g_mime_decoder_decode (rfc2231, buffer + 32, length, buffer + 32 + delta, capacity);
			g_assert_cmpmem (buffer + 32 + delta, m, expected, n);
		}
	}
	g_object_unref (rfc2231);
}

static void
test_guard_pages (void)
{
#ifdef G_OS_UNIX
	gsize page_size = sysconf (_SC_PAGESIZE), length, capacity;
	guint8 *input, *output, scratch[4];
	GMimeDecoder *rfc2231 = _g_mime_rfc2231_decoder_new ();
	guint state;

	input = mmap (NULL, page_size * 2, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
	output = mmap (NULL, page_size * 2, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
	g_assert_true (input != MAP_FAILED && output != MAP_FAILED);
	g_assert_cmpint (mprotect (input + page_size, page_size, PROT_NONE), ==, 0);
	g_assert_cmpint (mprotect (output + page_size, page_size, PROT_NONE), ==, 0);
	for (length = 0; length <= 129; length++) {
		memset (input + page_size - length, '%', length);
		capacity = _g_mime_rfc2231_encoder_estimate_output_length (length);
		g_assert_cmpuint (_g_mime_rfc2231_encode (input + page_size - length, length,
						     output + page_size - capacity, capacity), ==, capacity);
		for (state = 0; state < 3; state++) {
			g_mime_decoder_reset (rfc2231);
			g_mime_decoder_decode (rfc2231, (const guint8 *) "%G", state, scratch, sizeof (scratch));
			capacity = g_mime_decoder_estimate_output_length (rfc2231, length);
			g_assert_cmpuint (g_mime_decoder_decode (rfc2231, input + page_size - length, length,
								output + page_size - capacity, capacity), <=, capacity);
		}
	}
	g_assert_cmpint (munmap (input, page_size * 2), ==, 0);
	g_assert_cmpint (munmap (output, page_size * 2), ==, 0);
	g_object_unref (rfc2231);
#else
	g_test_skip ("Guard pages require mmap");
#endif
}

int
main (int argc, char **argv)
{
	g_test_init (&argc, &argv, NULL);
	g_test_add_func ("/rfc2231/octets", test_octets);
	g_test_add_func ("/rfc2231/recovery-clone-reset", test_recovery);
	g_test_add_func ("/rfc2231/estimates-preconditions", test_estimates_and_preconditions);
	g_test_add_func ("/rfc2231/overlap", test_overlap);
	g_test_add_func ("/rfc2231/guard-pages", test_guard_pages);
	return g_test_run ();
}
