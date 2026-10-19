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

#include <gmime/gmime-rfc2047-private.h>
#include <string.h>
#ifdef G_OS_UNIX
#include <sys/mman.h>
#include <unistd.h>
#endif

static GMimeRfc2047Encoder *
new_encoder (guint kind)
{
	if (kind == 0)
		return _g_mime_rfc2047_base64_encoder_new ();

	return _g_mime_rfc2047_quoted_printable_encoder_new (kind == 1 ?
		GMIME_QENCODE_MODE_PHRASE : GMIME_QENCODE_MODE_TEXT);
}

static void
assert_encoded (GMimeRfc2047Encoder *encoder, const guint8 *input, gsize length, const char *expected)
{
	gsize capacity = _g_mime_rfc2047_encoder_estimate_output_length (encoder, length);
	guint8 *output = g_malloc (capacity + 1);
	gsize written, i;
	guint repeat;

	for (repeat = 0; repeat < 2; repeat++) {
		memset (output, 0xa5, capacity + 1);
		written = _g_mime_rfc2047_encoder_encode (encoder, input, length, output, capacity);
		g_assert_cmpmem (output, written, expected, strlen (expected));
		for (i = written; i <= capacity; i++)
			g_assert_cmpuint (output[i], ==, 0xa5);
	}
	g_free (output);
}

static void
test_base64 (void)
{
	static const char *expected[] = { "", "Zg==", "Zm8=", "Zm9v", "Zm9vYg==", "Zm9vYmE=", "Zm9vYmFy" };
	GMimeRfc2047Encoder *encoder = new_encoder (0);
	guint8 input[260];
	guint i, offset, length;
	char *encoded;

	g_assert_cmpint (_g_mime_rfc2047_encoder_get_encoding (encoder), ==, 'b');
	for (i = 0; i < G_N_ELEMENTS (expected); i++)
		assert_encoded (encoder, (const guint8 *) "foobar", i, expected[i]);

	for (offset = 0; offset < 4; offset++) {
		for (i = 0; i < 256; i++)
			input[offset + i] = i;
		for (length = 0; length <= 256; length++) {
			encoded = g_base64_encode (input + offset, length);
			g_assert_cmpuint (_g_mime_rfc2047_encoder_estimate_output_length (encoder, length),
					  ==, strlen (encoded));
			assert_encoded (encoder, input + offset, length, encoded);
			g_free (encoded);
		}
	}
	g_object_unref (encoder);
}

static void
test_quoted_printable (void)
{
	guint8 input[256];
	guint kind, i;

	for (i = 0; i < 256; i++)
		input[i] = i;

	for (kind = 1; kind < 3; kind++) {
		GMimeRfc2047Encoder *encoder = new_encoder (kind);
		char expected[769], *p = expected;
		gboolean safe;

		g_assert_cmpint (_g_mime_rfc2047_encoder_get_encoding (encoder), ==, 'q');
		g_assert_cmpuint (_g_mime_rfc2047_encoder_estimate_output_length (encoder, sizeof (input)), ==, 768);
		for (i = 0; i < 256; i++) {
			if (kind == 1)
				safe = g_ascii_isalnum (i) || (i && strchr ("!*+-/", i));
			else
				safe = i > 32 && i < 127 && !strchr ("()<>@,;:\"/[]?.=_\\", i);

			if (i == ' ')
				*p++ = '_';
			else if (safe)
				*p++ = i;
			else
				p += g_snprintf (p, 4, "=%02X", i);
		}
		*p = '\0';
		assert_encoded (encoder, input, sizeof (input), expected);
		assert_encoded (encoder, (const guint8 *) "a b_=?\\\r\n", 9, "a_b=5F=3D=3F=5C=0D=0A");
		g_object_unref (encoder);
	}
}

static void
test_preconditions (void)
{
	guint8 input[] = { 'a' }, output[4];
	guint kind;

	g_test_expect_message (G_LOG_DOMAIN, G_LOG_LEVEL_CRITICAL, "*assertion*mode*failed*");
	g_assert_null (_g_mime_rfc2047_quoted_printable_encoder_new ((GMimeQEncodeMode) -1));
	g_test_assert_expected_messages ();

	for (kind = 0; kind < 3; kind++) {
		GMimeRfc2047Encoder *encoder = new_encoder (kind);
		gsize limit = kind ? G_MAXSIZE / 3 : (G_MAXSIZE / 4) * 3;
		gsize capacity = _g_mime_rfc2047_encoder_estimate_output_length (encoder, 1);
		gsize length;
		guint i;

		g_assert_cmpuint (_g_mime_rfc2047_encoder_estimate_output_length (encoder, 0), ==, 0);
		g_assert_cmpuint (_g_mime_rfc2047_encoder_encode (encoder, NULL, 0, NULL, 0), ==, 0);
		g_assert_cmpuint (_g_mime_rfc2047_encoder_estimate_output_length (encoder, limit - 1),
				  ==, kind ? G_MAXSIZE - 3 : (G_MAXSIZE / 4) * 4);
		g_assert_cmpuint (_g_mime_rfc2047_encoder_estimate_output_length (encoder, limit + 1), ==, G_MAXSIZE);
		g_assert_cmpuint (_g_mime_rfc2047_encoder_estimate_output_length (encoder, G_MAXSIZE), ==, G_MAXSIZE);

		memset (output, 0xa5, sizeof (output));
		for (length = 0; length < capacity; length++) {
			g_test_expect_message (G_LOG_DOMAIN, G_LOG_LEVEL_CRITICAL, "*assertion*capacity >= estimate*failed*");
			g_assert_cmpuint (_g_mime_rfc2047_encoder_encode (encoder, input, 1, output, length), ==, 0);
			g_test_assert_expected_messages ();
		}
		g_test_expect_message (G_LOG_DOMAIN, G_LOG_LEVEL_CRITICAL, "*assertion*input != NULL*failed*");
		g_assert_cmpuint (_g_mime_rfc2047_encoder_encode (encoder, NULL, 1, output, sizeof (output)), ==, 0);
		g_test_assert_expected_messages ();
		g_test_expect_message (G_LOG_DOMAIN, G_LOG_LEVEL_CRITICAL, "*assertion*output != NULL*failed*");
		g_assert_cmpuint (_g_mime_rfc2047_encoder_encode (encoder, input, 1, NULL, capacity), ==, 0);
		g_test_assert_expected_messages ();
		g_test_expect_message (G_LOG_DOMAIN, G_LOG_LEVEL_CRITICAL, "*assertion*estimate != G_MAXSIZE*failed*");
		g_assert_cmpuint (_g_mime_rfc2047_encoder_encode (encoder, input, G_MAXSIZE, output, G_MAXSIZE), ==, 0);
		g_test_assert_expected_messages ();
		for (i = 0; i < sizeof (output); i++)
			g_assert_cmpuint (output[i], ==, 0xa5);
		assert_encoded (encoder, input, 1, kind ? "a" : "YQ==");
		g_object_unref (encoder);
	}
}

static void
test_overlap (void)
{
	static const int offsets[] = { -7, 0, 7 };
	guint8 input[49], expected[148], buffer[256], before[256];
	guint kind, i, j;

	for (i = 0; i < sizeof (input); i++)
		input[i] = i * 17;
	for (kind = 0; kind < 3; kind++) {
		GMimeRfc2047Encoder *encoder = new_encoder (kind);
		gsize capacity = _g_mime_rfc2047_encoder_estimate_output_length (encoder, sizeof (input));
		gsize written, length = _g_mime_rfc2047_encoder_encode (encoder, input, sizeof (input), expected, capacity);

		for (i = 0; i < G_N_ELEMENTS (offsets); i++) {
			guint8 *output = buffer + 32 + offsets[i];

			memset (buffer, 0xa5, sizeof (buffer));
			memcpy (buffer + 32, input, sizeof (input));
			memcpy (before, buffer, sizeof (buffer));
			written = _g_mime_rfc2047_encoder_encode (encoder, buffer + 32, sizeof (input), output, capacity);
			g_assert_cmpmem (output, written, expected, length);
			for (j = 0; j < sizeof (buffer); j++) {
				if (buffer + j < output || buffer + j >= output + written)
					g_assert_cmpuint (buffer[j], ==, before[j]);
			}
		}
		g_object_unref (encoder);
	}
}

static void
test_guard_pages (void)
{
#ifdef G_OS_UNIX
	gsize page = (gsize) sysconf (_SC_PAGESIZE);
	guint8 *input_map, *output_map, expected[192];
	guint kind, length, i;

	input_map = mmap (NULL, page * 2, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
	output_map = mmap (NULL, page * 2, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
	g_assert_true (input_map != MAP_FAILED && output_map != MAP_FAILED);
	g_assert_cmpint (mprotect (input_map + page, page, PROT_NONE), ==, 0);
	g_assert_cmpint (mprotect (output_map + page, page, PROT_NONE), ==, 0);

	for (kind = 0; kind < 3; kind++) {
		GMimeRfc2047Encoder *encoder = new_encoder (kind);

		for (length = 0; length <= 64; length++) {
			gsize capacity = _g_mime_rfc2047_encoder_estimate_output_length (encoder, length);
			guint8 *input = input_map + page - length;
			guint8 *output = output_map + page - capacity;
			gsize written, n;

			for (i = 0; i < length; i++)
				input[i] = i * 23;
			n = _g_mime_rfc2047_encoder_encode (encoder, input, length, expected, capacity);
			memset (output, 0xa5, capacity);
			written = _g_mime_rfc2047_encoder_encode (encoder, input, length, output, capacity);
			g_assert_cmpmem (output, written, expected, n);
			for (i = written; i < capacity; i++)
				g_assert_cmpuint (output[i], ==, 0xa5);
		}
		g_object_unref (encoder);
	}
	g_assert_cmpint (munmap (input_map, page * 2), ==, 0);
	g_assert_cmpint (munmap (output_map, page * 2), ==, 0);
#else
	g_test_skip ("Guard pages require mmap");
#endif
}

int
main (int argc, char **argv)
{
	g_test_init (&argc, &argv, NULL);
	g_test_add_func ("/rfc2047/base64", test_base64);
	g_test_add_func ("/rfc2047/quoted-printable", test_quoted_printable);
	g_test_add_func ("/rfc2047/preconditions", test_preconditions);
	g_test_add_func ("/rfc2047/overlap", test_overlap);
	g_test_add_func ("/rfc2047/guard-pages", test_guard_pages);
	return g_test_run ();
}
