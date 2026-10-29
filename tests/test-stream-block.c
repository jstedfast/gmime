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

#ifdef HAVE_CONFIG_H
#include <config.h>
#include <gmime/gmime-object-private.h>
#endif

#include <errno.h>
#include <string.h>
#include <gmime/gmime.h>

#include "testsuite.h"

static void
test_block_read_write (void)
{
	GMimeStream *stream = g_mime_stream_block_new ();
	char data[9 * 1024], buffer[4098];
	GByteArray *copy;
	guint i, position = 0, n;

	testsuite_check ("Block reads and overwrites span 2048-byte boundaries");
	for (i = 0; i < sizeof (data); i++)
		data[i] = (char) (i * 71 + i / 2048);
	g_assert_true (g_mime_stream_eos (stream));
	g_assert_cmpint (g_mime_stream_length (stream), ==, 0);
	while (position < sizeof (data)) {
		n = MIN (sizeof (data) - position, 1 + position % 4096);
		g_assert_cmpint (g_mime_stream_write (stream, data + position, n), ==, n);
		position += n;
	}
	g_assert_cmpint (g_mime_stream_length (stream), ==, sizeof (data));
	g_assert_cmpint (g_mime_stream_reset (stream), ==, 0);
	position = 0;
	while (position < sizeof (data)) {
		memset (buffer, 0x5a, sizeof (buffer));
		n = MIN (sizeof (data) - position, 4096);
		g_assert_cmpint (g_mime_stream_read (stream, buffer + 1, 4096), ==, n);
		g_assert_cmpmem (buffer + 1, n, data + position, n);
		g_assert_cmpint (buffer[0], ==, 0x5a);
		g_assert_cmpint (buffer[n + 1], ==, 0x5a);
		position += n;
	}
	g_assert_cmpint (g_mime_stream_read (stream, buffer, 1), ==, 0);
	g_assert_true (g_mime_stream_eos (stream));
	g_assert_cmpint (g_mime_stream_seek (stream, 2047, GMIME_STREAM_SEEK_SET), ==, 2047);
	g_assert_cmpint (g_mime_stream_write (stream, "\0XYZ", 4), ==, 4);
	memcpy (data + 2047, "\0XYZ", 4);
	copy = g_mime_stream_block_to_byte_array (GMIME_STREAM_BLOCK (stream), NULL);
	g_assert_cmpmem (copy->data, copy->len, data, sizeof (data));
	g_assert_cmpint (g_mime_stream_tell (stream), ==, 2051);
	/* ToArray copies storage, so changing the copy must not change the stream. */
	copy->data[2047] = 'Q';
	g_assert_cmpint (g_mime_stream_seek (stream, 2047, GMIME_STREAM_SEEK_SET), ==, 2047);
	g_assert_cmpint (g_mime_stream_read (stream, buffer, 4), ==, 4);
	g_assert_cmpmem (buffer, 4, "\0XYZ", 4);
	g_byte_array_unref (copy);
	g_object_unref (stream);
	testsuite_check_passed ();
}

static void
test_block_resize (void)
{
	const gint64 sizes[] = { 0, 1, 2047, 2048, 2049, 4095, 4096, 4097, 10240 };
	GMimeStream *stream = g_mime_stream_block_new ();
	GMimeStreamBlock *block = GMIME_STREAM_BLOCK (stream);
	char data[12288];
	GByteArray *copy;
	guint i, j;

	testsuite_check ("Block resize clears truncated tails and zero-initializes growth");
	memset (data, 0x7e, sizeof (data));
	for (i = 0; i < G_N_ELEMENTS (sizes); i++) {
		g_assert_cmpint (g_mime_stream_block_set_length (block, 0), ==, 0);
		g_assert_cmpint (g_mime_stream_write (stream, data, sizeof (data)), ==, sizeof (data));
		g_assert_cmpint (g_mime_stream_block_set_length (block, sizes[i]), ==, 0);
		g_assert_cmpint (g_mime_stream_tell (stream), ==, sizes[i]);
		g_assert_cmpint (g_mime_stream_length (stream), ==, sizes[i]);
		g_assert_cmpint (g_mime_stream_block_set_length (block, sizeof (data)), ==, 0);
		g_assert_cmpint (g_mime_stream_tell (stream), ==, sizes[i]);
		copy = g_mime_stream_block_to_byte_array (block, NULL);
		g_assert_cmpuint (copy->len, ==, sizeof (data));
		for (j = 0; j < sizeof (data); j++)
			g_assert_cmpuint (copy->data[j], ==, j < sizes[i] ? 0x7e : 0);
		g_byte_array_unref (copy);
	}
	g_assert_cmpint (g_mime_stream_block_set_length (block, 0), ==, 0);
	g_assert_cmpint (g_mime_stream_block_set_length (block, 0), ==, 0);
	copy = g_mime_stream_block_to_byte_array (block, NULL);
	g_assert_cmpuint (copy->len, ==, 0);
	g_byte_array_unref (copy);
	errno = 0;
	g_assert_cmpint (g_mime_stream_block_set_length (block, -1), ==, -1);
	g_assert_cmpint (errno, ==, EINVAL);
	g_assert_cmpint (g_mime_stream_length (stream), ==, 0);
	g_assert_cmpint (g_mime_stream_block_set_length (block, 4097), ==, 0);
	g_assert_cmpint (g_mime_stream_seek (stream, 1, GMIME_STREAM_SEEK_SET), ==, 1);
	g_assert_cmpint (g_mime_stream_block_set_length (block, 2048), ==, 0);
	g_assert_cmpint (g_mime_stream_tell (stream), ==, 1);
	g_object_unref (stream);
	testsuite_check_passed ();
}

static void
test_block_seek (void)
{
	GMimeStream *stream = g_mime_stream_block_new ();
	guint32 random = 2048;
	gint64 position, offset;
	guint i;

	testsuite_check ("Block seeks validate all origins and preserve state on errors");
	g_assert_cmpint (g_mime_stream_block_set_length (GMIME_STREAM_BLOCK (stream), 9216), ==, 0);
	for (i = 0; i < 256; i++) {
		random = random * 1664525 + 1013904223;
		position = random % 9217;
		g_assert_cmpint (g_mime_stream_seek (stream, position, GMIME_STREAM_SEEK_SET), ==, position);
		offset = -(position / 2);
		position += offset;
		g_assert_cmpint (g_mime_stream_seek (stream, offset, GMIME_STREAM_SEEK_CUR), ==, position);
		offset = (9216 - position) / 2;
		position += offset;
		g_assert_cmpint (g_mime_stream_seek (stream, offset, GMIME_STREAM_SEEK_CUR), ==, position);
		g_assert_cmpint (g_mime_stream_seek (stream, position - 9216, GMIME_STREAM_SEEK_END), ==, position);
	}
	position = g_mime_stream_tell (stream);
	errno = 0;
	g_assert_cmpint (g_mime_stream_seek (stream, -1, GMIME_STREAM_SEEK_SET), ==, -1);
	g_assert_cmpint (errno, ==, EINVAL);
	g_assert_cmpint (g_mime_stream_seek (stream, 9217, GMIME_STREAM_SEEK_SET), ==, -1);
	g_assert_cmpint (g_mime_stream_seek (stream, -9217, GMIME_STREAM_SEEK_END), ==, -1);
	g_assert_cmpint (g_mime_stream_seek (stream, 1, GMIME_STREAM_SEEK_END), ==, -1);
	g_assert_cmpint (g_mime_stream_seek (stream, 0, (GMimeSeekWhence) 100), ==, -1);
	g_assert_cmpint (g_mime_stream_tell (stream), ==, position);
	errno = 0;
	g_assert_cmpint (g_mime_stream_seek (stream, G_MAXINT64, GMIME_STREAM_SEEK_CUR), ==, -1);
	g_assert_cmpint (errno, ==, EOVERFLOW);
	g_assert_cmpint (g_mime_stream_seek (stream, G_MININT64, GMIME_STREAM_SEEK_END), ==, -1);
	g_assert_cmpint (errno, ==, EINVAL);
	g_assert_cmpint (g_mime_stream_length (stream), ==, 9216);
	g_assert_cmpint (g_mime_stream_tell (stream), ==, position);
	g_object_unref (stream);
	testsuite_check_passed ();
}

static void
test_block_views (void)
{
	GMimeStreamPrivate *priv;
	GMimeStream *stream = g_mime_stream_block_new (), *sub, *nested;
	char data[8192], buffer[8];
	GByteArray *copy;

	testsuite_check ("Block views share storage, not positions or lifetime");
	memset (data, 'a', sizeof (data));
	g_assert_cmpint (g_mime_stream_write (stream, data, sizeof (data)), ==, sizeof (data));
	sub = g_mime_stream_substream (stream, 2047, 2051);
	g_assert_nonnull (sub);
	g_assert_cmpint (g_mime_stream_write (sub, "123456", 6), ==, 4);
	g_assert_cmpint (g_mime_stream_tell (sub), ==, 2051);
	g_assert_cmpint (g_mime_stream_tell (stream), ==, sizeof (data));
	errno = 0;
	g_assert_cmpint (g_mime_stream_write (sub, "x", 1), ==, -1);
	g_assert_cmpint (errno, ==, EINVAL);
	g_assert_cmpint (g_mime_stream_seek (sub, 0, GMIME_STREAM_SEEK_SET), ==, -1);
	g_assert_cmpint (g_mime_stream_seek (sub, 2047, GMIME_STREAM_SEEK_SET), ==, 2047);
	g_assert_cmpint (g_mime_stream_read (sub, buffer, sizeof (buffer)), ==, 4);
	g_assert_cmpmem (buffer, 4, "1234", 4);
	copy = g_mime_stream_block_to_byte_array (GMIME_STREAM_BLOCK (sub), NULL);
	g_assert_cmpmem (copy->data, copy->len, "1234", 4);
	g_byte_array_unref (copy);
	g_assert_cmpint (g_mime_stream_reset (sub), ==, 0);
	nested = g_mime_stream_substream (sub, 2048, -1);
	g_assert_nonnull (nested);
	priv = _g_mime_stream_get_private (nested);

	g_assert_cmpint (priv->bound_end, ==, 2051);
	g_assert_null (g_mime_stream_substream (sub, 2046, 2050));
	g_assert_null (g_mime_stream_substream (sub, 2048, 2052));
	g_assert_null (g_mime_stream_substream (sub, 2051, 2050));
	g_assert_null (g_mime_stream_substream (sub, 2048, -2));
	g_assert_cmpint (g_mime_stream_seek (nested, 2050, GMIME_STREAM_SEEK_SET), ==, 2050);
	/* A shortened backing store cannot leave a view reading freed blocks. */
	g_assert_cmpint (g_mime_stream_block_set_length (GMIME_STREAM_BLOCK (stream), 2048), ==, 0);
	g_assert_cmpint (g_mime_stream_length (sub), ==, 1);
	g_assert_cmpint (g_mime_stream_length (nested), ==, 0);
	g_assert_cmpint (g_mime_stream_read (nested, buffer, 1), ==, 0);
	g_assert_cmpint (g_mime_stream_tell (nested), ==, 2050);
	g_assert_cmpint (g_mime_stream_block_set_length (GMIME_STREAM_BLOCK (stream), 2047), ==, 0);
	g_assert_cmpint (g_mime_stream_length (sub), ==, 0);
	g_assert_cmpint (g_mime_stream_seek (nested, 0, GMIME_STREAM_SEEK_END), ==, 2048);
	g_assert_cmpint (g_mime_stream_seek (nested, 0, GMIME_STREAM_SEEK_CUR), ==, 2048);
	g_assert_cmpint (g_mime_stream_close (stream), ==, 0);
	g_object_unref (stream);
	g_assert_cmpint (g_mime_stream_block_set_length (GMIME_STREAM_BLOCK (nested), 4), ==, 0);
	g_assert_cmpint (priv->bound_end, ==, 2052);
	g_assert_cmpint (g_mime_stream_write (nested, "WXYZ", 4), ==, 4);
	g_assert_cmpint (g_mime_stream_read (sub, buffer, sizeof (buffer)), ==, 4);
	g_assert_cmpmem (buffer, 4, "\0WXY", 4);
	g_assert_cmpint (g_mime_stream_close (sub), ==, 0);
	g_object_unref (sub);
	g_assert_cmpint (g_mime_stream_reset (nested), ==, 0);
	g_assert_cmpint (g_mime_stream_read (nested, buffer, sizeof (buffer)), ==, 4);
	g_assert_cmpmem (buffer, 4, "WXYZ", 4);
	g_object_unref (nested);
	testsuite_check_passed ();
}

static void
test_block_errors (void)
{
	const gint64 maximum = (gint64) G_MAXINT * 2048;
	GMimeStream *stream = g_mime_stream_block_new ();
	GMimeStreamBlock *block = GMIME_STREAM_BLOCK (stream);
	char buffer[2] = { 'Q', 'Q' };
	GError *error = NULL;

	testsuite_check ("Block capacity and closed-stream failures preserve state");
	errno = 0;
	g_assert_cmpint (g_mime_stream_block_set_length (block, maximum + 1), ==, -1);
	g_assert_cmpint (errno, ==, EOVERFLOW);
	g_assert_cmpint (g_mime_stream_block_set_length (block, G_MAXINT64), ==, -1);
	g_assert_cmpint (g_mime_stream_write (stream, "x", (size_t) G_MAXSSIZE + 1), ==, -1);
	g_assert_cmpint (g_mime_stream_tell (stream), ==, 0);
	g_assert_cmpint (g_mime_stream_length (stream), ==, 0);
	g_mime_stream_set_bounds (stream, maximum - 1, -1);
	g_assert_cmpint (g_mime_stream_write (stream, "x", 1), ==, -1);
	g_assert_cmpint (g_mime_stream_write (stream, "xx", 2), ==, -1);
	g_assert_cmpint (g_mime_stream_tell (stream), ==, maximum - 1);
	g_assert_cmpint (g_mime_stream_block_set_length (block, 2), ==, -1);
	g_mime_stream_set_bounds (stream, G_MAXINT64, -1);
	g_assert_cmpint (g_mime_stream_write (stream, "x", 1), ==, -1);
	g_assert_cmpint (g_mime_stream_block_set_length (block, 1), ==, -1);
	g_mime_stream_set_bounds (stream, -1, -1);
	g_assert_cmpint (g_mime_stream_read (stream, buffer, 1), ==, -1);
	g_assert_cmpint (errno, ==, EINVAL);
	g_assert_null (g_mime_stream_block_to_byte_array (block, &error));
	g_assert_error (error, GMIME_ERROR, EINVAL);
	g_assert_cmpint (errno, ==, EINVAL);
	g_clear_error (&error);
	g_mime_stream_set_bounds (stream, 0, -1);
	g_assert_cmpint (g_mime_stream_reset (stream), ==, 0);
	g_assert_cmpint (g_mime_stream_close (stream), ==, 0);
	g_assert_cmpint (g_mime_stream_close (stream), ==, 0);
	g_assert_true (g_mime_stream_eos (stream));
	errno = 0;
	g_assert_cmpint (g_mime_stream_read (stream, buffer, 1), ==, -1);
	g_assert_cmpint (errno, ==, EBADF);
	g_assert_cmpmem (buffer, 2, "QQ", 2);
	g_assert_cmpint (g_mime_stream_write (stream, "x", 1), ==, -1);
	g_assert_cmpint (g_mime_stream_seek (stream, 0, GMIME_STREAM_SEEK_SET), ==, -1);
	g_assert_cmpint (g_mime_stream_tell (stream), ==, -1);
	g_assert_cmpint (g_mime_stream_length (stream), ==, -1);
	g_assert_cmpint (g_mime_stream_flush (stream), ==, -1);
	g_assert_cmpint (g_mime_stream_reset (stream), ==, -1);
	g_assert_cmpint (g_mime_stream_block_set_length (block, 0), ==, -1);
	g_assert_null (g_mime_stream_block_to_byte_array (block, &error));
	g_assert_error (error, GMIME_ERROR, EBADF);
	g_assert_cmpint (errno, ==, EBADF);
	g_clear_error (&error);
	g_assert_null (g_mime_stream_block_to_byte_array (block, NULL));
	g_assert_null (g_mime_stream_substream (stream, 0, -1));
	g_assert_cmpint (g_mime_stream_read (stream, buffer, 0), ==, 0);
	g_assert_cmpint (g_mime_stream_write (stream, "", 0), ==, 0);
	g_object_unref (stream);
	testsuite_check_passed ();
}

static void
test_block_integration (void)
{
	GMimeStream *stream = g_mime_stream_block_new (), *destination;
	GMimeStream *filtered = g_mime_stream_filter_new (stream);
	GMimeFilter *filter = g_mime_filter_basic_new (GMIME_CONTENT_ENCODING_BASE64, TRUE);
	GMimeContent *content;
	char data[8193];
	GByteArray *copy;
	guint i;

	testsuite_check ("Block storage supports filtered output and MIME content");
	for (i = 0; i < sizeof (data); i++)
		data[i] = (char) (i * 71);
	g_mime_stream_filter_add (GMIME_STREAM_FILTER (filtered), filter);
	g_assert_cmpint (g_mime_stream_write (filtered, data, sizeof (data)), ==, sizeof (data));
	g_assert_cmpint (g_mime_stream_flush (filtered), ==, 0);
	content = g_mime_content_new_with_stream (stream, GMIME_CONTENT_ENCODING_BASE64);
	destination = g_mime_stream_block_new ();
	g_assert_cmpint (g_mime_content_decode_to_stream (content, destination), ==, sizeof (data));
	copy = g_mime_stream_block_to_byte_array (GMIME_STREAM_BLOCK (destination), NULL);
	g_assert_cmpmem (copy->data, copy->len, data, sizeof (data));
	g_byte_array_unref (copy);
	g_object_unref (destination);
	g_object_unref (content);
	g_object_unref (filter);
	g_object_unref (filtered);
	g_object_unref (stream);
	testsuite_check_passed ();
}

int
main (int argc, char **argv)
{
	g_mime_init ();
	testsuite_init (argc, argv);
	testsuite_start ("MimeKit memory block streams");
	test_block_read_write ();
	test_block_resize ();
	test_block_seek ();
	test_block_views ();
	test_block_errors ();
	test_block_integration ();
	testsuite_end ();
	g_mime_shutdown ();
	return testsuite_exit ();
}
