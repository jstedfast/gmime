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
#endif

#include <errno.h>
#include <string.h>
#include <gmime/gmime.h>

#include "testsuite.h"

static void
test_substream_ranges (void)
{
	const gint64 invalid[][2] = {
		{ -1, 8 }, { 2, -2 }, { 4, 3 }, { 1, 5 }, { 2, 9 }, { 9, -1 }
	};
	GMimeStream *sources[3], *parent, *child, *empty;
	guint i, j;

	testsuite_check ("Substreams validate absolute ranges and inherit finite parent ends");
	sources[0] = g_mime_stream_mem_new_with_buffer ("0123456789", 10);
	sources[1] = g_mime_stream_block_new ();
	sources[2] = g_mime_stream_null_new ();
	g_assert_cmpint (g_mime_stream_write (sources[1], "0123456789", 10), ==, 10);
	g_assert_cmpint (g_mime_stream_reset (sources[1]), ==, 0);
	for (i = 0; i < G_N_ELEMENTS (sources); i++) {
		parent = g_mime_stream_substream (sources[i], 2, 8);
		g_assert_nonnull (parent);
		for (j = 0; j < G_N_ELEMENTS (invalid); j++) {
			errno = 0;
			g_assert_null (g_mime_stream_substream (parent, invalid[j][0], invalid[j][1]));
			g_assert_cmpint (errno, ==, EINVAL);
			g_assert_cmpint (g_mime_stream_tell (parent), ==, 2);
		}
		child = g_mime_stream_substream (parent, 3, -1);
		g_assert_nonnull (child);
		g_assert_cmpint (child->bound_start, ==, 3);
		g_assert_cmpint (child->bound_end, ==, 8);
		g_assert_cmpint (g_mime_stream_length (child), ==, 5);
		g_assert_cmpint (g_mime_stream_tell (sources[i]), ==, 0);
		empty = g_mime_stream_substream (child, 8, -1);
		g_assert_nonnull (empty);
		g_assert_cmpint (g_mime_stream_length (empty), ==, 0);
		g_object_unref (empty);
		g_object_unref (child);
		g_object_unref (parent);

		g_mime_stream_set_bounds (sources[i], -1, 8);
		errno = 0;
		g_assert_null (g_mime_stream_substream (sources[i], 0, 8));
		g_assert_cmpint (errno, ==, EINVAL);
		g_mime_stream_set_bounds (sources[i], 5, 4);
		errno = 0;
		g_assert_null (g_mime_stream_substream (sources[i], 5, -1));
		g_assert_cmpint (errno, ==, EINVAL);
		g_object_unref (sources[i]);
	}
	testsuite_check_passed ();
}

static void
test_mem_substream_reads (void)
{
	GMimeStream *parent, *child, *sibling, *tail;
	char buffer[16];

	testsuite_check ("Memory views clip reads to storage and retain independent positions");
	parent = g_mime_stream_mem_new_with_buffer ("0123456789", 10);
	g_assert_cmpint (g_mime_stream_seek (parent, 7, GMIME_STREAM_SEEK_SET), ==, 7);
	child = g_mime_stream_substream (parent, 2, 6);
	sibling = g_mime_stream_substream (parent, 3, -1);
	memset (buffer, 0x5a, sizeof (buffer));
	g_assert_cmpint (g_mime_stream_read (child, buffer + 1, 10), ==, 4);
	g_assert_cmpmem (buffer + 1, 4, "2345", 4);
	g_assert_cmpint (buffer[0], ==, 0x5a);
	g_assert_cmpint (buffer[5], ==, 0x5a);
	g_assert_cmpint (g_mime_stream_read (sibling, buffer, 2), ==, 2);
	g_assert_cmpmem (buffer, 2, "34", 2);
	g_assert_cmpint (g_mime_stream_read (parent, buffer, 1), ==, 1);
	g_assert_cmpmem (buffer, 1, "7", 1);
	g_assert_cmpint (g_mime_stream_tell (child), ==, 6);
	g_assert_cmpint (g_mime_stream_tell (sibling), ==, 5);

	tail = g_mime_stream_substream (parent, 8, 20);
	memset (buffer, 0x5a, sizeof (buffer));
	g_assert_cmpint (g_mime_stream_length (tail), ==, 12);
	g_assert_cmpint (g_mime_stream_read (tail, buffer + 1, 14), ==, 2);
	g_assert_cmpmem (buffer + 1, 2, "89", 2);
	g_assert_cmpint (buffer[0], ==, 0x5a);
	g_assert_cmpint (buffer[3], ==, 0x5a);
	g_assert_true (g_mime_stream_eos (tail));
	g_assert_cmpint (g_mime_stream_read (tail, buffer + 1, 14), ==, 0);

	/* External truncation changes storage, not the independent view bounds. */
	g_byte_array_set_size (GMIME_STREAM_MEM (parent)->buffer, 1);
	g_assert_cmpint (g_mime_stream_reset (tail), ==, 0);
	g_assert_cmpint (g_mime_stream_read (tail, buffer + 1, 14), ==, 0);
	g_assert_cmpint (g_mime_stream_length (tail), ==, 12);
	g_assert_cmpint (g_mime_stream_length (sibling), ==, 0);
	g_assert_true (g_mime_stream_eos (sibling));
	g_object_unref (tail);
	g_object_unref (sibling);
	g_object_unref (child);
	g_object_unref (parent);
	testsuite_check_passed ();
}

static void
test_mem_substream_writes (void)
{
	GMimeStream *parent = g_mime_stream_mem_new_with_buffer ("abc", 3);
	GMimeStream *child = g_mime_stream_substream (parent, 5, 9);
	GMimeStream *empty = g_mime_stream_substream (parent, 9, 9);
	GByteArray *array = GMIME_STREAM_MEM (parent)->buffer;
	char buffer[6];

	testsuite_check ("Memory views grow backing storage and retain native partial writes");
	g_assert_cmpint (g_mime_stream_write (empty, "x", 1), ==, 0);
	g_assert_cmpuint (array->len, ==, 3);
	g_object_unref (empty);
	g_assert_cmpint (g_mime_stream_write (child, "AB", 2), ==, 2);
	g_assert_cmpmem (array->data, array->len, "abc\0\0AB", 7);
	g_assert_cmpint (g_mime_stream_tell (parent), ==, 0);
	g_assert_cmpint (g_mime_stream_write (child, "XYZ", 3), ==, 2);
	g_assert_cmpmem (array->data, array->len, "abc\0\0ABXY", 9);
	g_assert_cmpint (g_mime_stream_write (child, "!", 1), ==, 0);
	g_assert_cmpint (g_mime_stream_tell (child), ==, 9);
	g_assert_cmpint (g_mime_stream_reset (child), ==, 0);
	g_assert_cmpint (g_mime_stream_read (child, buffer, sizeof (buffer)), ==, 4);
	g_assert_cmpmem (buffer, 4, "ABXY", 4);
	g_assert_cmpint (g_mime_stream_seek (parent, 15, GMIME_STREAM_SEEK_SET), ==, 15);
	g_assert_cmpuint (array->len, ==, 15);
	g_assert_cmpmem (array->data + 9, 6, "\0\0\0\0\0\0", 6);
	g_object_unref (child);
	g_object_unref (parent);
	testsuite_check_passed ();
}

static void
test_mem_substream_seeks (void)
{
	GMimeStream *parent = g_mime_stream_mem_new_with_buffer ("0123456789", 10);
	GMimeStream *child = g_mime_stream_substream (parent, 2, 8);
	GMimeStream *tail = g_mime_stream_substream (parent, 3, -1);
	const gint64 invalid[][2] = {
		{ 0, 77 }, { 1, GMIME_STREAM_SEEK_SET }, { 9, GMIME_STREAM_SEEK_SET },
		{ G_MAXINT64, GMIME_STREAM_SEEK_CUR }, { G_MAXINT64, GMIME_STREAM_SEEK_END },
		{ G_MININT64, GMIME_STREAM_SEEK_CUR }
	};
	guint i;
	char buffer[2];

	testsuite_check ("Memory view seeks check origins, bounds and arithmetic without mutation");
	for (i = 0; i < G_N_ELEMENTS (invalid); i++) {
		errno = 0;
		g_assert_cmpint (g_mime_stream_seek (child, invalid[i][0], (GMimeSeekWhence) invalid[i][1]), ==, -1);
		g_assert_cmpint (errno, ==, i == 3 || i == 4 ? EOVERFLOW : EINVAL);
		g_assert_cmpint (g_mime_stream_tell (child), ==, 2);
		g_assert_cmpuint (GMIME_STREAM_MEM (parent)->buffer->len, ==, 10);
	}
	g_assert_cmpint (g_mime_stream_seek (child, 0, GMIME_STREAM_SEEK_END), ==, 8);
	g_assert_cmpint (g_mime_stream_seek (child, -2, GMIME_STREAM_SEEK_CUR), ==, 6);
	g_assert_cmpint (g_mime_stream_seek (child, 3, GMIME_STREAM_SEEK_SET), ==, 3);
	g_assert_cmpint (g_mime_stream_tell (parent), ==, 0);
	g_byte_array_set_size (GMIME_STREAM_MEM (parent)->buffer, 0);
	g_assert_cmpint (g_mime_stream_seek (tail, 0, GMIME_STREAM_SEEK_CUR), ==, 3);
	g_assert_cmpuint (GMIME_STREAM_MEM (parent)->buffer->len, ==, 0);
	g_assert_cmpint (g_mime_stream_seek (child, 7, GMIME_STREAM_SEEK_SET), ==, 7);
	g_assert_cmpuint (GMIME_STREAM_MEM (parent)->buffer->len, ==, 0);
	g_assert_cmpint (g_mime_stream_read (child, buffer, 1), ==, 0);
	g_object_unref (tail);
	g_object_unref (child);
	g_object_unref (parent);
	testsuite_check_passed ();
}

static void
test_mem_substream_lifetime (void)
{
	GMimeStream *parent, *child, *nested;
	GByteArray *array;
	char buffer[16];

	testsuite_check ("Memory views survive parent close, replacement and ownership changes");
	parent = g_mime_stream_mem_new_with_buffer ("0123456789", 10);
	child = g_mime_stream_substream (parent, 2, 8);
	nested = g_mime_stream_substream (child, 3, 6);
	g_assert_false (g_mime_stream_mem_get_owner (GMIME_STREAM_MEM (child)));
	g_assert_cmpint (g_mime_stream_close (parent), ==, 0);
	g_object_unref (parent);
	g_assert_cmpint (g_mime_stream_read (child, buffer, 6), ==, 6);
	g_assert_cmpmem (buffer, 6, "234567", 6);
	g_mime_stream_mem_set_owner (GMIME_STREAM_MEM (child), TRUE);
	g_assert_cmpint (g_mime_stream_close (child), ==, 0);
	g_object_unref (child);
	g_assert_cmpint (g_mime_stream_read (nested, buffer, 3), ==, 3);
	g_assert_cmpmem (buffer, 3, "345", 3);
	g_object_unref (nested);

	parent = g_mime_stream_mem_new_with_buffer ("original", 8);
	child = g_mime_stream_substream (parent, 1, 5);
	nested = g_mime_stream_substream (child, 2, 4);
	array = g_byte_array_new ();
	g_byte_array_append (array, (const guint8 *) "new", 3);
	g_mime_stream_mem_set_byte_array (GMIME_STREAM_MEM (parent), array);
	g_assert_cmpint (g_mime_stream_read (child, buffer, 4), ==, 4);
	g_assert_cmpmem (buffer, 4, "rigi", 4);
	g_mime_stream_mem_set_byte_array (GMIME_STREAM_MEM (child), array);
	g_assert_cmpint (g_mime_stream_read (nested, buffer, 2), ==, 2);
	g_assert_cmpmem (buffer, 2, "ig", 2);
	g_object_unref (nested);
	g_assert_cmpint (g_mime_stream_write (parent, "NEW", 3), ==, 3);
	g_assert_cmpint (g_mime_stream_close (parent), ==, 0);
	g_assert_cmpmem (array->data, array->len, "NEW", 3);
	g_object_unref (child);
	g_byte_array_unref (array);
	g_object_unref (parent);

	array = g_byte_array_new ();
	g_byte_array_append (array, (const guint8 *) "borrowed", 8);
	parent = g_mime_stream_mem_new ();
	g_mime_stream_mem_set_byte_array (GMIME_STREAM_MEM (parent), array);
	child = g_mime_stream_substream (parent, 0, -1);
	g_byte_array_unref (array);
	g_assert_cmpint (g_mime_stream_close (parent), ==, 0);
	g_assert_cmpint (g_mime_stream_read (child, buffer, 8), ==, 8);
	g_assert_cmpmem (buffer, 8, "borrowed", 8);
	g_object_unref (child);
	g_object_unref (parent);

	/* Reassigning the same owned array transfers its original reference back
	 * to the caller. The view's additional reference remains independent. */
	parent = g_mime_stream_mem_new_with_buffer ("self", 4);
	array = g_mime_stream_mem_get_byte_array (GMIME_STREAM_MEM (parent));
	child = g_mime_stream_substream (parent, 1, -1);
	g_mime_stream_mem_set_byte_array (GMIME_STREAM_MEM (parent), array);
	g_assert_false (g_mime_stream_mem_get_owner (GMIME_STREAM_MEM (parent)));
	g_assert_cmpint (g_mime_stream_close (parent), ==, 0);
	g_byte_array_unref (array);
	array = g_mime_stream_mem_get_byte_array (GMIME_STREAM_MEM (child));
	g_mime_stream_mem_set_byte_array (GMIME_STREAM_MEM (child), array);
	g_mime_stream_mem_set_owner (GMIME_STREAM_MEM (child), TRUE);
	g_mime_stream_mem_set_owner (GMIME_STREAM_MEM (child), FALSE);
	g_assert_cmpint (g_mime_stream_read (child, buffer, 4), ==, 4);
	g_assert_cmpmem (buffer, 4, "self", 4);
	g_object_unref (child);
	g_object_unref (parent);
	testsuite_check_passed ();
}

static void
test_mem_substream_errors (void)
{
	GMimeStream *parent = g_mime_stream_mem_new_with_buffer ("abc", 3);
	GMimeStream *child;
	char buffer[8];

	testsuite_check ("Memory view count and capacity errors preserve storage and position");
	if (G_MAXSIZE > G_MAXUINT) {
		errno = 0;
		g_assert_null (g_mime_stream_mem_new_with_buffer ("x", (size_t) G_MAXUINT + 1));
		g_assert_cmpint (errno, ==, EOVERFLOW);
	}
	errno = 0;
	g_assert_cmpint (g_mime_stream_write (parent, "x", (size_t) G_MAXSSIZE + 1), ==, -1);
	g_assert_cmpint (errno, ==, EOVERFLOW);
	errno = 0;
	g_assert_cmpint (g_mime_stream_seek (parent, (gint64) G_MAXUINT + 1, GMIME_STREAM_SEEK_SET), ==, -1);
	g_assert_cmpint (errno, ==, EOVERFLOW);
	g_assert_cmpint (g_mime_stream_tell (parent), ==, 0);
	g_assert_cmpuint (GMIME_STREAM_MEM (parent)->buffer->len, ==, 3);
	child = g_mime_stream_substream (parent, G_MAXUINT, -1);
	errno = 0;
	g_assert_cmpint (g_mime_stream_write (child, "x", 1), ==, -1);
	g_assert_cmpint (errno, ==, EOVERFLOW);
	g_assert_cmpuint (GMIME_STREAM_MEM (parent)->buffer->len, ==, 3);
	g_object_unref (child);
	/* A huge requested read is safe when only a few allocated bytes remain. */
	g_assert_cmpint (g_mime_stream_read (parent, buffer, G_MAXSIZE), ==, 3);
	g_assert_cmpmem (buffer, 3, "abc", 3);
	g_mime_stream_set_bounds (parent, G_MININT64, -1);
	errno = 0;
	g_assert_cmpint (g_mime_stream_length (parent), ==, -1);
	g_assert_cmpint (errno, ==, EINVAL);
	errno = 0;
	g_assert_cmpint (g_mime_stream_read (parent, buffer, 1), ==, -1);
	g_assert_cmpint (errno, ==, EINVAL);
	errno = 0;
	g_assert_cmpint (g_mime_stream_write (parent, "x", 1), ==, -1);
	g_assert_cmpint (errno, ==, EINVAL);
	g_mime_stream_set_bounds (parent, 0, -1);
	g_assert_cmpint (g_mime_stream_close (parent), ==, 0);
	/* Invalid ranges are rejected even before a closed backend can dispatch. */
	errno = 0;
	g_assert_null (g_mime_stream_substream (parent, -1, -1));
	g_assert_cmpint (errno, ==, EINVAL);
	errno = 0;
	g_assert_null (g_mime_stream_substream (parent, 0, -1));
	g_assert_cmpint (errno, ==, EBADF);
	errno = 0;
	g_assert_cmpint (g_mime_stream_read (parent, buffer, 1), ==, -1);
	g_assert_cmpint (errno, ==, EBADF);
	errno = 0;
	g_assert_cmpint (g_mime_stream_write (parent, "x", 1), ==, -1);
	g_assert_cmpint (errno, ==, EBADF);
	errno = 0;
	g_assert_cmpint (g_mime_stream_seek (parent, 0, GMIME_STREAM_SEEK_SET), ==, -1);
	g_assert_cmpint (errno, ==, EBADF);
	errno = 0;
	g_assert_cmpint (g_mime_stream_tell (parent), ==, -1);
	g_assert_cmpint (errno, ==, EBADF);
	errno = 0;
	g_assert_cmpint (g_mime_stream_length (parent), ==, -1);
	g_assert_cmpint (errno, ==, EBADF);
	errno = 0;
	g_assert_cmpint (g_mime_stream_reset (parent), ==, -1);
	g_assert_cmpint (errno, ==, EBADF);
	errno = 0;
	g_assert_cmpint (g_mime_stream_flush (parent), ==, -1);
	g_assert_cmpint (errno, ==, EBADF);
	g_assert_cmpint (g_mime_stream_read (parent, buffer, 0), ==, 0);
	g_assert_cmpint (g_mime_stream_write (parent, "x", 0), ==, 0);
	g_object_unref (parent);
	testsuite_check_passed ();
}

int
main (int argc, char **argv)
{
	int errors;

	g_mime_init ();
	testsuite_init (argc, argv);
	testsuite_start ("Native substreams");
	test_substream_ranges ();
	test_mem_substream_reads ();
	test_mem_substream_writes ();
	test_mem_substream_seeks ();
	test_mem_substream_lifetime ();
	test_mem_substream_errors ();
	testsuite_end ();
	errors = testsuite_exit ();
	g_mime_shutdown ();

	return errors;
}
