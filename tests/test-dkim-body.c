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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "gmime/gmime-dkim-private.h"
#include <string.h>

/* Expected hash inputs include the CRLF MimeKit appends after flushing
 * the filter if the last output was not a newline. */
static const struct {
	const char *input, *simple, *relaxed;
} vectors[] = {
	{ "", "\r\n", "" },
	{ "\r\n\n", "\r\n", "" },
	{ "\r\r", "\r\r\r\n", "\r\r\r\n" },
	{ "\r", "\r\r\n", "\r\r\n" },
	{ "\rX", "\rX\r\n", "\rX\r\n" },
	{ "abc\r", "abc\r\r\n", "abc\r\r\n" },
	{ "a\n\rX", "a\n\rX\r\n", "a\n\rX\r\n" },
	{ "\n\rX", "\r\n\rX\r\n", "\r\n\rX\r\n" },
	{ "a\r\n\r\n\rb", "a\r\n\r\n\rb\r\n", "a\r\n\r\n\rb\r\n" },
	{ "a\n\n\r\n\rb", "a\n\r\n\r\n\rb\r\n", "a\n\r\n\r\n\rb\r\n" },
	{ "a\rb\r\n", "a\rb\r\n", "a\rb\r\n" },
	{ "Hello\rWorld\n", "Hello\rWorld\n", "Hello\rWorld\n" },
	{ "\r\r\n", "\r\r\n", "\r\r\n" },
	{ "a\r\n\r\n\r\r", "a\r\n\r\n\r\r\r\n", "a\r\n\r\n\r\r\r\n" },
	{ "a\n\n \r\r", "a\n\r\n \r\r\r\n", "a\n\r\n \r\r\r\n" },
	{ "a \rb", "a \rb\r\n", "a \rb\r\n" },
	{ "a\t\r \r\n", "a\t\r \r\n", "a \r\r\n" },
	{ "a \r\n", "a \r\n", "a\r\n" },
	{ "Hello!\r\n\r\n\r\n", "Hello!\r\n", "Hello!\r\n" },
	{ "Hello!\r\n  \r\n\r\n", "Hello!\r\n  \r\n", "Hello!\r\n" },
	{ "text\t \r\n\t \r\ntext\t \r\n", "text\t \r\n\t \r\ntext\t \r\n", "text\r\n\r\ntext\r\n" },
	{ " \t a  \t b \t  c\n\n\n", " \t a  \t b \t  c\n", " a b c\n" },
	{ "\t ", "\t \r\n", "" }
};

static void
check_chunks (gboolean relaxed, const char *input, size_t length, size_t split, size_t chunk, const char *expected)
{
	GMimeStream *stream = _g_mime_dkim_body_stream_new (relaxed, G_MAXUINT64);
	GChecksum *sum = g_checksum_new (G_CHECKSUM_SHA256);
	guint8 actual[32], reference[32];
	gsize n = sizeof reference;
	GError *error = NULL;

	g_assert_cmpint (g_mime_stream_write (stream, input, split), ==, (ssize_t) split);
	while (split < length) {
		size_t count = MIN (length - split, chunk);
		g_assert_cmpint (g_mime_stream_write (stream, input + split, count), ==, (ssize_t) count);
		split += count;
	}

	g_assert_true (_g_mime_dkim_body_stream_finish (stream, FALSE, actual, &error));
	g_assert_no_error (error);
	g_checksum_update (sum, (const guchar *) expected, strlen (expected));
	g_checksum_get_digest (sum, reference, &n);
	g_assert_cmpmem (actual, sizeof actual, reference, n);

	g_checksum_free (sum);
	g_object_unref (stream);
}

static void
test_vectors (gconstpointer arg)
{
	gboolean relaxed = GPOINTER_TO_INT (arg);

	for (guint i = 0; i < G_N_ELEMENTS (vectors); i++) {
		const char *expected = relaxed ? vectors[i].relaxed : vectors[i].simple;
		const char *input = vectors[i].input;
		size_t length = strlen (input);

		g_test_message ("Vector %u", i);
		for (size_t split = 0; split <= length; split++)
			check_chunks (relaxed, input, length, split, length, expected);

		check_chunks (relaxed, input, length, 0, 1, expected);
	}
}

static void
test_long_lf_run (void)
{
	GString *input = g_string_new ("a\n");
	GString *expected = g_string_new ("a\n");

	for (guint i = 0; i < 8192; i++) {
		g_string_append_c (input, '\n');
		g_string_append (expected, "\r\n");
	}

	for (int relaxed = 0; relaxed <= 1; relaxed++)
		check_chunks (relaxed, input->str, input->len, 2, 137, "a\n");

	g_string_append_c (input, 'b');
	g_string_append (expected, "b\r\n");
	for (int relaxed = 0; relaxed <= 1; relaxed++)
		check_chunks (relaxed, input->str, input->len, 2, 137, expected->str);

	g_string_free (expected, TRUE);
	g_string_free (input, TRUE);
}

int
main (int argc, char **argv)
{
	g_test_init (&argc, &argv, NULL);
	g_test_add_data_func ("/dkim/body/simple", GINT_TO_POINTER (FALSE), test_vectors);
	g_test_add_data_func ("/dkim/body/relaxed", GINT_TO_POINTER (TRUE), test_vectors);
	g_test_add_func ("/dkim/body/long-lf-run", test_long_lf_run);
	return g_test_run ();
}
