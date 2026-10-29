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
#include <fcntl.h>
#include <string.h>
#include <sys/types.h>
#ifdef HAVE_MMAP
#include <sys/mman.h>
#endif
#include <unistd.h>
#include <glib/gstdio.h>
#include <gmime/gmime.h>

#include "testsuite.h"

static GMimeStream *
new_fs_stream (const char *data, size_t length, char **path)
{
	GError *error = NULL;
	GMimeStream *stream;
	int fd;

	fd = g_file_open_tmp ("gmime-substreams-XXXXXX", path, &error);
	g_assert_no_error (error);
	g_assert_cmpint (fd, >=, 0);
	stream = g_mime_stream_fs_new (fd);
	g_assert_cmpint (g_mime_stream_write (stream, data, length), ==, length);
	g_assert_cmpint (g_mime_stream_reset (stream), ==, 0);

	return stream;
}

static void
free_temporary_stream (GMimeStream *stream, char *path)
{
	g_object_unref (stream);
	g_assert_cmpint (g_unlink (path), ==, 0);
	g_free (path);
}

static GMimeStream *
new_file_stream (const char *data, size_t length, char **path)
{
	GError *error = NULL;
	GMimeStream *stream;
	FILE *fp;
	int fd;

	fd = g_file_open_tmp ("gmime-file-substreams-XXXXXX", path, &error);
	g_assert_no_error (error);
	g_assert_cmpint (fd, >=, 0);
	fp = fdopen (fd, "w+b");
	g_assert_nonnull (fp);
	stream = g_mime_stream_file_new (fp);
	g_assert_cmpint (g_mime_stream_write (stream, data, length), ==, length);
	g_assert_cmpint (g_mime_stream_reset (stream), ==, 0);

	return stream;
}

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
		GMimeStreamPrivate *priv;

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
		priv = _g_mime_stream_get_private (child);

		g_assert_cmpint (priv->bound_start, ==, 3);
		g_assert_cmpint (priv->bound_end, ==, 8);
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
test_filter_substream_bounds (void)
{
	const gint64 bounds[][2] = { { 4, 10 }, { 5, -1 }, { 14, 14 } };
	GMimeStream *source, *parent, *child;
	GMimeFilter *filter;
	gint64 start, end, expected_end;
	guint i, j;

	testsuite_check ("Filter substreams initialize child bounds and position without changing the parent");
	for (i = 0; i < 2; i++) {
		source = g_mime_stream_mem_new_with_buffer ("0123456789abcdef", 16);
		g_mime_stream_set_bounds (source, 2, 14);
		g_assert_cmpint (g_mime_stream_seek (source, 7, GMIME_STREAM_SEEK_SET), ==, 7);
		parent = g_mime_stream_filter_new (source);
		g_mime_stream_set_position (parent, 9);

		if (i) {
			filter = g_mime_filter_dos2unix_new (FALSE);
			g_mime_stream_filter_add (GMIME_STREAM_FILTER (parent), filter);
			g_object_unref (filter);
		}

		for (j = 0; j < G_N_ELEMENTS (bounds); j++) {
			child = g_mime_stream_substream (parent, bounds[j][0], bounds[j][1]);
			g_assert_nonnull (child);
			g_assert_true (GMIME_IS_STREAM_FILTER (child));
			expected_end = bounds[j][1] == -1 ? 14 : bounds[j][1];
			g_mime_stream_get_bounds (child, &start, &end);
			g_assert_cmpint (start, ==, bounds[j][0]);
			g_assert_cmpint (end, ==, expected_end);
			g_assert_cmpint (g_mime_stream_get_position (child), ==, bounds[j][0]);
			g_assert_cmpint (g_mime_stream_length (child), ==, expected_end - bounds[j][0]);
			g_mime_stream_get_bounds (parent, &start, &end);
			g_assert_cmpint (start, ==, 2);
			g_assert_cmpint (end, ==, 14);
			g_assert_cmpint (g_mime_stream_get_position (parent), ==, 9);
			g_assert_cmpint (g_mime_stream_length (parent), ==, 12);
			g_assert_cmpint (g_mime_stream_tell (source), ==, 7);
			g_object_unref (child);
		}

		g_object_unref (parent);
		g_object_unref (source);
	}
	testsuite_check_passed ();
}

static void
test_mem_substream_reads (void)
{
	GMimeStreamMemPrivate *priv;
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
	priv = _g_mime_stream_mem_get_private (GMIME_STREAM_MEM (parent));

	g_byte_array_set_size (priv->buffer, 1);
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
	GMimeStreamMemPrivate *priv = _g_mime_stream_mem_get_private (GMIME_STREAM_MEM (parent));
	GByteArray *array = priv->buffer;
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
	GMimeStreamMemPrivate *parent_priv;
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
		GMimeStreamMemPrivate *priv;

		errno = 0;
		g_assert_cmpint (g_mime_stream_seek (child, invalid[i][0], (GMimeSeekWhence) invalid[i][1]), ==, -1);
		g_assert_cmpint (errno, ==, i == 3 || i == 4 ? EOVERFLOW : EINVAL);
		g_assert_cmpint (g_mime_stream_tell (child), ==, 2);
		priv = _g_mime_stream_mem_get_private (GMIME_STREAM_MEM (parent));

		g_assert_cmpuint (priv->buffer->len, ==, 10);
	}
	g_assert_cmpint (g_mime_stream_seek (child, 0, GMIME_STREAM_SEEK_END), ==, 8);
	g_assert_cmpint (g_mime_stream_seek (child, -2, GMIME_STREAM_SEEK_CUR), ==, 6);
	g_assert_cmpint (g_mime_stream_seek (child, 3, GMIME_STREAM_SEEK_SET), ==, 3);
	g_assert_cmpint (g_mime_stream_tell (parent), ==, 0);
	parent_priv = _g_mime_stream_mem_get_private (GMIME_STREAM_MEM (parent));

	g_byte_array_set_size (parent_priv->buffer, 0);
	g_assert_cmpint (g_mime_stream_seek (tail, 0, GMIME_STREAM_SEEK_CUR), ==, 3);
	g_assert_cmpuint (parent_priv->buffer->len, ==, 0);
	g_assert_cmpint (g_mime_stream_seek (child, 7, GMIME_STREAM_SEEK_SET), ==, 7);
	g_assert_cmpuint (parent_priv->buffer->len, ==, 0);
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
	GMimeStreamMemPrivate *priv;
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
	priv = _g_mime_stream_mem_get_private (GMIME_STREAM_MEM (parent));

	g_assert_cmpuint (priv->buffer->len, ==, 3);
	child = g_mime_stream_substream (parent, G_MAXUINT, -1);
	errno = 0;
	g_assert_cmpint (g_mime_stream_write (child, "x", 1), ==, -1);
	g_assert_cmpint (errno, ==, EOVERFLOW);
	g_assert_cmpuint (priv->buffer->len, ==, 3);
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

static void
test_fs_substream_reads (void)
{
	GMimeStreamFsPrivate *parent_priv;
	GMimeStreamPrivate *nested_priv;
	GMimeStream *parent, *child, *sibling, *nested, *empty, *tail;
	char *path, buffer[16];
	gint64 physical;
	int fd;

	testsuite_check ("Descriptor views stop at finite EOF without disturbing other positions");
	parent = new_fs_stream ("0123456789", 10, &path);
	parent_priv = _g_mime_stream_fs_get_private (GMIME_STREAM_FS (parent));

	fd = parent_priv->fd;
	g_assert_cmpint (g_mime_stream_seek (parent, 7, GMIME_STREAM_SEEK_SET), ==, 7);
	child = g_mime_stream_substream (parent, 2, 6);
	sibling = g_mime_stream_substream (parent, 3, -1);
	nested = g_mime_stream_substream (child, 3, -1);
	nested_priv = _g_mime_stream_get_private (nested);

	g_assert_cmpint (nested_priv->bound_end, ==, 6);
	errno = 0;
	g_assert_null (g_mime_stream_substream (child, 1, 4));
	g_assert_cmpint (errno, ==, EINVAL);
	memset (buffer, 0x5a, sizeof (buffer));
	g_assert_cmpint (g_mime_stream_read (child, buffer + 1, 14), ==, 4);
	g_assert_cmpmem (buffer + 1, 4, "2345", 4);
	g_assert_cmpint (buffer[0], ==, 0x5a);
	g_assert_cmpint (buffer[5], ==, 0x5a);
	g_assert_true (g_mime_stream_eos (child));
	physical = lseek (fd, 0, SEEK_CUR);
	g_assert_cmpint (g_mime_stream_read (child, buffer + 1, 14), ==, 0);
	g_assert_cmpint (g_mime_stream_read (child, buffer + 1, 14), ==, 0);
	g_assert_cmpint (lseek (fd, 0, SEEK_CUR), ==, physical);
	g_assert_cmpint (g_mime_stream_read (sibling, buffer, 2), ==, 2);
	g_assert_cmpmem (buffer, 2, "34", 2);
	g_assert_cmpint (g_mime_stream_read (nested, buffer, 8), ==, 3);
	g_assert_cmpmem (buffer, 3, "345", 3);
	g_assert_cmpint (g_mime_stream_tell (parent), ==, 7);
	g_assert_cmpint (g_mime_stream_tell (sibling), ==, 5);
	g_assert_cmpint (g_mime_stream_read (parent, buffer, 1), ==, 1);
	g_assert_cmpmem (buffer, 1, "7", 1);
	empty = g_mime_stream_substream (child, 6, -1);
	physical = lseek (fd, 0, SEEK_CUR);
	g_assert_cmpint (g_mime_stream_length (empty), ==, 0);
	g_assert_cmpint (g_mime_stream_read (empty, buffer, sizeof (buffer)), ==, 0);
	g_assert_cmpint (lseek (fd, 0, SEEK_CUR), ==, physical);

	tail = g_mime_stream_substream (parent, 8, 20);
	memset (buffer, 0x5a, sizeof (buffer));
	g_assert_cmpint (g_mime_stream_read (tail, buffer + 1, 14), ==, 2);
	g_assert_cmpmem (buffer + 1, 2, "89", 2);
	g_assert_cmpint (buffer[3], ==, 0x5a);
	g_assert_cmpint (g_mime_stream_read (tail, buffer + 1, 14), ==, 0);
	g_assert_true (g_mime_stream_eos (tail));
	g_assert_cmpint (g_mime_stream_length (tail), ==, 12);
	g_object_unref (tail);
	tail = g_mime_stream_substream (parent, 15, -1);
	g_assert_cmpint (g_mime_stream_length (tail), ==, 0);
	g_assert_cmpint (g_mime_stream_read (tail, buffer, 1), ==, 0);
	g_assert_cmpint (g_mime_stream_tell (tail), ==, 15);
	g_assert_cmpint (g_mime_stream_length (parent), ==, 10);
	g_object_unref (tail);
	g_object_unref (empty);
	g_object_unref (nested);
	g_object_unref (sibling);
	g_object_unref (child);
	free_temporary_stream (parent, path);
	testsuite_check_passed ();
}

static void
test_fs_substream_writes (void)
{
	GMimeStream *parent, *child;
	char *path, buffer[16];

	testsuite_check ("Descriptor views retain partial writes, sparse gaps and native seek behavior");
	parent = new_fs_stream ("abc", 3, &path);
	child = g_mime_stream_substream (parent, 5, 9);
	g_assert_cmpint (g_mime_stream_write (child, "AB", 2), ==, 2);
	g_assert_cmpint (g_mime_stream_length (parent), ==, 7);
	g_assert_cmpint (g_mime_stream_tell (parent), ==, 0);
	g_assert_cmpint (g_mime_stream_write (child, "XYZ", 3), ==, 2);
	errno = 0;
	g_assert_cmpint (g_mime_stream_write (child, "!", 1), ==, -1);
	g_assert_cmpint (errno, ==, EINVAL);
	g_assert_cmpint (g_mime_stream_tell (child), ==, 9);
	g_assert_cmpint (g_mime_stream_read (child, buffer, 1), ==, 0);
	g_assert_cmpint (g_mime_stream_reset (child), ==, 0);
	g_assert_cmpint (g_mime_stream_read (child, buffer, sizeof (buffer)), ==, 4);
	g_assert_cmpmem (buffer, 4, "ABXY", 4);
	g_assert_cmpint (g_mime_stream_seek (parent, 15, GMIME_STREAM_SEEK_SET), ==, 15);
	g_assert_cmpint (g_mime_stream_length (parent), ==, 9);
	g_assert_cmpint (g_mime_stream_write (parent, "Z", 1), ==, 1);
	g_assert_cmpint (g_mime_stream_reset (parent), ==, 0);
	g_assert_cmpint (g_mime_stream_read (parent, buffer, sizeof (buffer)), ==, 16);
	g_assert_cmpmem (buffer, 16, "abc\0\0ABXY\0\0\0\0\0\0Z", 16);
	g_assert_cmpint (g_mime_stream_tell (child), ==, 9);
	g_object_unref (child);
	free_temporary_stream (parent, path);
	testsuite_check_passed ();
}

static void
test_fs_substream_seeks (void)
{
	const gint64 invalid[][2] = {
		{ 0, 77 }, { 1, GMIME_STREAM_SEEK_SET }, { 9, GMIME_STREAM_SEEK_SET },
		{ G_MAXINT64, GMIME_STREAM_SEEK_CUR }, { G_MAXINT64, GMIME_STREAM_SEEK_END },
		{ G_MININT64, GMIME_STREAM_SEEK_CUR }
	};
	GMimeStream *parent, *child, *wide;
	char *path, buffer[4];
	guint i;

	testsuite_check ("Descriptor seeks check arithmetic and preserve native END and EOF rules");
	parent = new_fs_stream ("0123456789abcdef", 16, &path);
	child = g_mime_stream_substream (parent, 2, 8);
	for (i = 0; i < G_N_ELEMENTS (invalid); i++) {
		errno = 0;
		g_assert_cmpint (g_mime_stream_seek (child, invalid[i][0], (GMimeSeekWhence) invalid[i][1]), ==, -1);
		g_assert_cmpint (errno, ==, i == 3 || i == 4 ? EOVERFLOW : EINVAL);
		g_assert_cmpint (g_mime_stream_tell (child), ==, 2);
	}
	g_assert_cmpint (g_mime_stream_seek (child, 0, GMIME_STREAM_SEEK_END), ==, 8);
	g_assert_cmpint (g_mime_stream_seek (child, -2, GMIME_STREAM_SEEK_CUR), ==, 6);
	g_assert_cmpint (g_mime_stream_seek (child, 3, GMIME_STREAM_SEEK_SET), ==, 3);
	wide = g_mime_stream_substream (parent, 5, 25);
	/* Positive END offsets consult physical EOF; zero and negative finite
	 * offsets use the declared view end, as in the existing native API. */
	g_assert_cmpint (g_mime_stream_seek (wide, 1, GMIME_STREAM_SEEK_END), ==, 17);
	g_assert_cmpint (g_mime_stream_seek (wide, 0, GMIME_STREAM_SEEK_END), ==, 25);
	g_assert_cmpint (g_mime_stream_seek (wide, -1, GMIME_STREAM_SEEK_END), ==, 24);
	g_assert_cmpint (g_mime_stream_read (wide, buffer, 1), ==, 0);
	g_assert_true (g_mime_stream_eos (wide));
	g_assert_cmpint (g_mime_stream_seek (wide, 0, GMIME_STREAM_SEEK_CUR), ==, 24);
	g_assert_true (g_mime_stream_eos (wide));
	g_assert_cmpint (g_mime_stream_seek (wide, -1, GMIME_STREAM_SEEK_CUR), ==, 23);
	g_assert_false (g_mime_stream_eos (wide));
	g_assert_cmpint (g_mime_stream_reset (wide), ==, 0);
	g_assert_cmpint (g_mime_stream_tell (wide), ==, 5);
	g_assert_cmpint (g_mime_stream_length (parent), ==, 16);
	g_assert_cmpint (g_mime_stream_tell (parent), ==, 0);
	g_object_unref (wide);
	g_object_unref (child);
	free_temporary_stream (parent, path);
	testsuite_check_passed ();
}

static void
test_fs_substream_eof_growth (void)
{
	GMimeStream *parent, *tail, *writer;
	char *path, buffer[32];

	testsuite_check ("Descriptor views retry reads after EOF and retain their own cached seek origin");
	parent = new_fs_stream ("0123456789abcdef", 16, &path);
	tail = g_mime_stream_substream (parent, 5, -1);
	writer = g_mime_stream_substream (parent, 16, -1);
	g_assert_cmpint (g_mime_stream_read (tail, buffer, sizeof (buffer)), ==, 11);
	g_assert_false (g_mime_stream_eos (tail));
	g_assert_cmpint (g_mime_stream_read (tail, buffer, 0), ==, 0);
	g_assert_false (g_mime_stream_eos (tail));
	g_assert_cmpint (g_mime_stream_read (tail, buffer, 1), ==, 0);
	g_assert_true (g_mime_stream_eos (tail));
	g_assert_false (g_mime_stream_eos (parent));
	g_assert_cmpint (g_mime_stream_write (writer, "ABCDEF", 6), ==, 6);
	g_assert_cmpint (g_mime_stream_length (tail), ==, 17);
	g_assert_cmpint (g_mime_stream_seek (tail, 0, GMIME_STREAM_SEEK_END), ==, 16);
	g_assert_true (g_mime_stream_eos (tail));
	g_assert_cmpint (g_mime_stream_seek (tail, -2, GMIME_STREAM_SEEK_END), ==, 14);
	g_assert_false (g_mime_stream_eos (tail));
	g_assert_cmpint (g_mime_stream_seek (tail, -2, GMIME_STREAM_SEEK_END), ==, 20);
	g_assert_cmpint (g_mime_stream_seek (tail, 0, GMIME_STREAM_SEEK_END), ==, 22);
	g_assert_cmpint (g_mime_stream_read (tail, buffer, 1), ==, 0);
	g_assert_cmpint (g_mime_stream_write (writer, "WXYZ", 4), ==, 4);
	g_assert_cmpint (g_mime_stream_read (tail, buffer, 2), ==, 2);
	g_assert_cmpmem (buffer, 2, "WX", 2);
	g_assert_true (g_mime_stream_eos (tail));
	g_assert_cmpint (g_mime_stream_read (tail, buffer, 2), ==, 2);
	g_assert_cmpmem (buffer, 2, "YZ", 2);
	g_assert_cmpint (g_mime_stream_reset (tail), ==, 0);
	g_assert_false (g_mime_stream_eos (tail));
	g_assert_cmpint (g_mime_stream_tell (tail), ==, 5);
	g_assert_cmpint (g_mime_stream_tell (parent), ==, 0);
	g_object_unref (writer);
	g_object_unref (tail);
	free_temporary_stream (parent, path);
	testsuite_check_passed ();
}

static void
test_fs_substream_errors (void)
{
	GMimeStreamFsPrivate *priv;
	GMimeStream *parent, *child;
	char *path, buffer[8];
	gint64 physical;
	int fd;

	testsuite_check ("Descriptor count, capacity and closed errors leave state and ownership intact");
	parent = new_fs_stream ("abc", 3, &path);
	priv = _g_mime_stream_fs_get_private (GMIME_STREAM_FS (parent));

	fd = priv->fd;
	physical = lseek (fd, 0, SEEK_CUR);
	errno = 0;
	g_assert_cmpint (g_mime_stream_read (parent, buffer, (size_t) G_MAXSSIZE + 1), ==, -1);
	g_assert_cmpint (errno, ==, EOVERFLOW);
	errno = 0;
	g_assert_cmpint (g_mime_stream_write (parent, "x", (size_t) G_MAXSSIZE + 1), ==, -1);
	g_assert_cmpint (errno, ==, EOVERFLOW);
#ifdef G_OS_WIN32
	errno = 0;
	g_assert_cmpint (g_mime_stream_read (parent, buffer, (size_t) G_MAXINT + 1), ==, -1);
	g_assert_cmpint (errno, ==, EOVERFLOW);
	errno = 0;
	g_assert_cmpint (g_mime_stream_write (parent, "x", (size_t) G_MAXINT + 1), ==, -1);
	g_assert_cmpint (errno, ==, EOVERFLOW);
#endif
	g_assert_cmpint (lseek (fd, 0, SEEK_CUR), ==, physical);
	child = g_mime_stream_substream (parent, G_MAXINT64 - 1, -1);
	errno = 0;
	g_assert_cmpint (g_mime_stream_read (child, buffer, 2), ==, -1);
	g_assert_cmpint (errno, ==, EOVERFLOW);
	errno = 0;
	g_assert_cmpint (g_mime_stream_write (child, "xx", 2), ==, -1);
	g_assert_cmpint (errno, ==, EOVERFLOW);
	errno = 0;
	g_assert_cmpint (g_mime_stream_seek (child, 2, GMIME_STREAM_SEEK_CUR), ==, -1);
	g_assert_cmpint (errno, ==, EOVERFLOW);
	g_assert_cmpint (g_mime_stream_tell (child), ==, G_MAXINT64 - 1);
	g_assert_cmpint (lseek (fd, 0, SEEK_CUR), ==, physical);
	g_assert_false (g_mime_stream_fs_get_owner (GMIME_STREAM_FS (child)));
	g_assert_cmpint (g_mime_stream_close (child), ==, 0);
	g_object_unref (child);
	if (sizeof (off_t) < sizeof (gint64)) {
		errno = 0;
		g_assert_cmpint (g_mime_stream_seek (parent, (gint64) G_MAXINT32 + 1, GMIME_STREAM_SEEK_SET), ==, -1);
		g_assert_cmpint (errno, ==, EOVERFLOW);
		g_assert_cmpint (lseek (fd, 0, SEEK_CUR), ==, physical);
	}
	g_assert_cmpint (g_mime_stream_write (parent, "Z", 1), ==, 1);
	g_assert_cmpint (g_mime_stream_length (parent), ==, 3);
	g_mime_stream_set_bounds (parent, G_MININT64, -1);
	errno = 0;
	g_assert_cmpint (g_mime_stream_length (parent), ==, -1);
	g_assert_cmpint (errno, ==, EINVAL);
	g_mime_stream_set_bounds (parent, 0, -1);
	g_mime_stream_fs_set_owner (GMIME_STREAM_FS (parent), FALSE);
	g_assert_cmpint (g_mime_stream_close (parent), ==, 0);
	/* Detaching a borrowed descriptor must not close the caller's handle. */
	g_assert_cmpint (lseek (fd, 0, SEEK_SET), ==, 0);
	g_assert_cmpint (read (fd, buffer, 3), ==, 3);
	g_assert_cmpmem (buffer, 3, "Zbc", 3);
	g_assert_cmpint (close (fd), ==, 0);
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
	g_assert_cmpint (g_mime_stream_flush (parent), ==, -1);
	g_assert_cmpint (errno, ==, EBADF);
	errno = 0;
	g_assert_cmpint (g_mime_stream_reset (parent), ==, -1);
	g_assert_cmpint (errno, ==, EBADF);
	g_assert_cmpint (g_mime_stream_read (parent, buffer, 0), ==, 0);
	g_assert_cmpint (g_mime_stream_write (parent, "x", 0), ==, 0);
	free_temporary_stream (parent, path);
	testsuite_check_passed ();
}

static void
test_file_substream_reads (void)
{
	GMimeStreamFilePrivate *parent_priv;
	GMimeStreamPrivate *nested_priv;
	GMimeStream *parent, *child, *nested, *tail, *empty;
	char *path, buffer[16];
	FILE *fp;
	long physical;

	testsuite_check ("Buffered views clip reads and retain EOF independently of shared stdio flags");
	parent = new_file_stream ("0123456789", 10, &path);
	parent_priv = _g_mime_stream_file_get_private (GMIME_STREAM_FILE (parent));

	fp = parent_priv->fp;
	child = g_mime_stream_substream (parent, 2, 6);
	nested = g_mime_stream_substream (child, 3, -1);
	nested_priv = _g_mime_stream_get_private (nested);

	g_assert_cmpint (nested_priv->bound_end, ==, 6);
	errno = 0;
	g_assert_null (g_mime_stream_substream (child, 1, 4));
	g_assert_cmpint (errno, ==, EINVAL);
	memset (buffer, 0x5a, sizeof (buffer));
	g_assert_cmpint (g_mime_stream_read (child, buffer + 1, 14), ==, 4);
	g_assert_cmpmem (buffer + 1, 4, "2345", 4);
	g_assert_cmpint (buffer[0], ==, 0x5a);
	g_assert_cmpint (buffer[5], ==, 0x5a);
	physical = ftell (fp);
	g_assert_cmpint (g_mime_stream_read (child, buffer, sizeof (buffer)), ==, 0);
	g_assert_cmpint (g_mime_stream_read (child, buffer, sizeof (buffer)), ==, 0);
	g_assert_cmpint (ftell (fp), ==, physical);
	g_assert_cmpint (g_mime_stream_read (nested, buffer, sizeof (buffer)), ==, 3);
	g_assert_cmpmem (buffer, 3, "345", 3);
	g_assert_cmpint (g_mime_stream_tell (parent), ==, 0);
	empty = g_mime_stream_substream (child, 6, -1);
	physical = ftell (fp);
	g_assert_cmpint (g_mime_stream_read (empty, buffer, sizeof (buffer)), ==, 0);
	g_assert_cmpint (ftell (fp), ==, physical);

	tail = g_mime_stream_substream (parent, 8, 20);
	memset (buffer, 0x5a, sizeof (buffer));
	g_assert_cmpint (g_mime_stream_read (tail, buffer + 1, 14), ==, 2);
	g_assert_cmpmem (buffer + 1, 2, "89", 2);
	g_assert_cmpint (buffer[3], ==, 0x5a);
	/* fread may already set feof on a short read, but this view observes
	 * EOF only after a zero result, just as MimeKit's wrapper does. */
	g_assert_true (feof (fp) != 0);
	g_assert_false (g_mime_stream_eos (tail));
	g_assert_false (g_mime_stream_eos (parent));
	g_assert_cmpint (g_mime_stream_read (tail, buffer, 1), ==, 0);
	g_assert_true (g_mime_stream_eos (tail));
	g_assert_cmpint (g_mime_stream_length (tail), ==, 12);
	g_assert_cmpint (g_mime_stream_length (parent), ==, 10);
	g_assert_false (feof (fp) != 0);
	g_assert_true (g_mime_stream_eos (tail));
	g_object_unref (tail);
	tail = g_mime_stream_substream (parent, 15, -1);
	g_assert_cmpint (g_mime_stream_length (tail), ==, 0);
	g_assert_cmpint (g_mime_stream_read (tail, buffer, 1), ==, 0);
	g_assert_true (g_mime_stream_eos (tail));
	g_assert_cmpint (g_mime_stream_reset (tail), ==, 0);
	g_assert_false (g_mime_stream_eos (tail));
	g_assert_cmpint (g_mime_stream_tell (tail), ==, 15);
	g_object_unref (tail);
	g_object_unref (empty);
	g_object_unref (nested);
	g_object_unref (child);
	free_temporary_stream (parent, path);
	testsuite_check_passed ();
}

static void
test_file_substream_writes_seeks (void)
{
	const gint64 invalid[][2] = {
		{ 0, 77 }, { 4, GMIME_STREAM_SEEK_SET }, { 10, GMIME_STREAM_SEEK_SET },
		{ G_MAXINT64, GMIME_STREAM_SEEK_CUR }, { G_MAXINT64, GMIME_STREAM_SEEK_END },
		{ G_MININT64, GMIME_STREAM_SEEK_CUR }
	};
	GMimeStream *parent, *child, *wide;
	char *path, buffer[16];
	guint i;

	testsuite_check ("Buffered views preserve partial writes, sparse gaps and checked native seeks");
	parent = new_file_stream ("abc", 3, &path);
	child = g_mime_stream_substream (parent, 5, 9);
	g_assert_cmpint (g_mime_stream_write (child, "AB", 2), ==, 2);
	g_assert_cmpint (g_mime_stream_write (child, "XYZ", 3), ==, 2);
	errno = 0;
	g_assert_cmpint (g_mime_stream_write (child, "!", 1), ==, -1);
	g_assert_cmpint (errno, ==, EINVAL);
	g_assert_cmpint (g_mime_stream_read (child, buffer, 1), ==, 0);
	g_assert_cmpint (g_mime_stream_seek (parent, 15, GMIME_STREAM_SEEK_SET), ==, 15);
	g_assert_cmpint (g_mime_stream_length (parent), ==, 9);
	g_assert_cmpint (g_mime_stream_write (parent, "Z", 1), ==, 1);
	g_assert_cmpint (g_mime_stream_reset (parent), ==, 0);
	g_assert_cmpint (g_mime_stream_read (parent, buffer, sizeof (buffer)), ==, 16);
	g_assert_cmpmem (buffer, 16, "abc\0\0ABXY\0\0\0\0\0\0Z", 16);
	g_assert_cmpint (g_mime_stream_tell (child), ==, 9);
	for (i = 0; i < G_N_ELEMENTS (invalid); i++) {
		errno = 0;
		g_assert_cmpint (g_mime_stream_seek (child, invalid[i][0], (GMimeSeekWhence) invalid[i][1]), ==, -1);
		g_assert_cmpint (errno, ==, i == 3 || i == 4 ? EOVERFLOW : EINVAL);
		g_assert_cmpint (g_mime_stream_tell (child), ==, 9);
	}
	g_assert_cmpint (g_mime_stream_seek (child, 0, GMIME_STREAM_SEEK_END), ==, 9);
	g_assert_cmpint (g_mime_stream_seek (child, -2, GMIME_STREAM_SEEK_CUR), ==, 7);
	wide = g_mime_stream_substream (parent, 5, 25);
	g_assert_cmpint (g_mime_stream_seek (wide, 1, GMIME_STREAM_SEEK_END), ==, 17);
	g_assert_cmpint (g_mime_stream_seek (wide, 0, GMIME_STREAM_SEEK_END), ==, 25);
	g_assert_cmpint (g_mime_stream_seek (wide, -1, GMIME_STREAM_SEEK_END), ==, 24);
	g_assert_cmpint (g_mime_stream_read (wide, buffer, 1), ==, 0);
	g_assert_true (g_mime_stream_eos (wide));
	g_assert_cmpint (g_mime_stream_seek (wide, 0, GMIME_STREAM_SEEK_CUR), ==, 24);
	g_assert_true (g_mime_stream_eos (wide));
	g_assert_cmpint (g_mime_stream_seek (wide, -1, GMIME_STREAM_SEEK_CUR), ==, 23);
	g_assert_false (g_mime_stream_eos (wide));
	g_assert_cmpint (g_mime_stream_tell (parent), ==, 16);
	g_object_unref (wide);
	g_object_unref (child);
	free_temporary_stream (parent, path);
	testsuite_check_passed ();
}

static void
test_file_substream_eof_growth (void)
{
	GMimeStream *parent, *tail, *writer;
	char *path, buffer[32];

	testsuite_check ("Buffered views retry EOF after growth without sharing cached end origins");
	parent = new_file_stream ("0123456789abcdef", 16, &path);
	tail = g_mime_stream_substream (parent, 5, -1);
	writer = g_mime_stream_substream (parent, 16, -1);
	g_assert_cmpint (g_mime_stream_read (tail, buffer, sizeof (buffer)), ==, 11);
	g_assert_false (g_mime_stream_eos (tail));
	g_assert_cmpint (g_mime_stream_read (tail, buffer, 0), ==, 0);
	g_assert_false (g_mime_stream_eos (tail));
	g_assert_cmpint (g_mime_stream_read (tail, buffer, 1), ==, 0);
	g_assert_true (g_mime_stream_eos (tail));
	g_assert_cmpint (g_mime_stream_write (writer, "ABCDEF", 6), ==, 6);
	g_assert_cmpint (g_mime_stream_length (tail), ==, 17);
	g_assert_cmpint (g_mime_stream_seek (tail, 0, GMIME_STREAM_SEEK_END), ==, 16);
	g_assert_true (g_mime_stream_eos (tail));
	g_assert_cmpint (g_mime_stream_seek (tail, -2, GMIME_STREAM_SEEK_END), ==, 14);
	g_assert_false (g_mime_stream_eos (tail));
	g_assert_cmpint (g_mime_stream_seek (tail, -2, GMIME_STREAM_SEEK_END), ==, 20);
	g_assert_cmpint (g_mime_stream_seek (tail, 0, GMIME_STREAM_SEEK_END), ==, 22);
	g_assert_cmpint (g_mime_stream_read (tail, buffer, 1), ==, 0);
	g_assert_cmpint (g_mime_stream_write (writer, "WXYZ", 4), ==, 4);
	g_assert_cmpint (g_mime_stream_read (tail, buffer, 2), ==, 2);
	g_assert_cmpmem (buffer, 2, "WX", 2);
	g_assert_true (g_mime_stream_eos (tail));
	g_assert_cmpint (g_mime_stream_read (tail, buffer, 2), ==, 2);
	g_assert_cmpmem (buffer, 2, "YZ", 2);
	g_assert_cmpint (g_mime_stream_reset (tail), ==, 0);
	g_assert_false (g_mime_stream_eos (tail));
	g_assert_false (g_mime_stream_eos (parent));
	g_assert_cmpint (g_mime_stream_tell (parent), ==, 0);
	g_object_unref (writer);
	g_object_unref (tail);
	free_temporary_stream (parent, path);
	testsuite_check_passed ();
}

static void
test_file_substream_errors (void)
{
	GMimeStreamFilePrivate *parent_priv;
	GMimeStreamFilePrivate *child_priv;
	GMimeStream *parent, *child, *sibling;
	char *path, buffer[8];
	FILE *fp;
	long physical;

	testsuite_check ("Buffered count, stdio and closed errors preserve positions and borrowed ownership");
	parent = new_file_stream ("abc", 3, &path);
	parent_priv = _g_mime_stream_file_get_private (GMIME_STREAM_FILE (parent));

	fp = parent_priv->fp;
	physical = ftell (fp);
	errno = 0;
	g_assert_cmpint (g_mime_stream_read (parent, buffer, (size_t) G_MAXSSIZE + 1), ==, -1);
	g_assert_cmpint (errno, ==, EOVERFLOW);
	errno = 0;
	g_assert_cmpint (g_mime_stream_write (parent, "x", (size_t) G_MAXSSIZE + 1), ==, -1);
	g_assert_cmpint (errno, ==, EOVERFLOW);
	child = g_mime_stream_substream (parent, G_MAXINT64 - 1, -1);
	errno = 0;
	g_assert_cmpint (g_mime_stream_read (child, buffer, 2), ==, -1);
	g_assert_cmpint (errno, ==, EOVERFLOW);
	errno = 0;
	g_assert_cmpint (g_mime_stream_write (child, "xx", 2), ==, -1);
	g_assert_cmpint (errno, ==, EOVERFLOW);
	g_assert_cmpint (ftell (fp), ==, physical);
	g_assert_cmpint (g_mime_stream_close (child), ==, 0);
	g_object_unref (child);
	if (sizeof (long) < sizeof (gint64)) {
		errno = 0;
		g_assert_cmpint (g_mime_stream_seek (parent, (gint64) G_MAXINT32 + 1, GMIME_STREAM_SEEK_SET), ==, -1);
		g_assert_cmpint (errno, ==, EOVERFLOW);
		g_assert_cmpint (ftell (fp), ==, physical);
	}
	fp = g_fopen (path, "rb");
	g_assert_nonnull (fp);
	child = g_mime_stream_file_new (fp);
	errno = 0;
	g_assert_cmpint (g_mime_stream_write (child, "x", 1), ==, -1);
	g_assert_cmpint (errno, !=, 0);
	g_assert_true (ferror (fp) != 0);
	g_assert_false (g_mime_stream_eos (child));
	sibling = g_mime_stream_substream (child, 0, -1);
	g_assert_cmpint (g_mime_stream_read (sibling, buffer, 3), ==, 3);
	g_assert_cmpmem (buffer, 3, "abc", 3);
	g_assert_cmpint (g_mime_stream_read (sibling, buffer, 1), ==, 0);
	g_assert_true (g_mime_stream_eos (sibling));
	g_assert_false (g_mime_stream_eos (child));
	g_object_unref (sibling);
	g_object_unref (child);
	fp = g_fopen (path, "wb");
	g_assert_nonnull (fp);
	child = g_mime_stream_file_new (fp);
	errno = 0;
	g_assert_cmpint (g_mime_stream_read (child, buffer, 1), ==, -1);
	g_assert_cmpint (errno, !=, 0);
	g_assert_false (g_mime_stream_eos (child));
	g_object_unref (child);
#ifdef __linux__
	/* A buffered write can succeed before fclose reports its flush error.
	 * The released FILE must not be closed again by the finalizer. */
	fp = g_fopen ("/dev/full", "wb");
	g_assert_nonnull (fp);
	child = g_mime_stream_file_new (fp);
	g_assert_cmpint (g_mime_stream_write (child, "x", 1), ==, 1);
	errno = 0;
	g_assert_cmpint (g_mime_stream_close (child), ==, -1);
	g_assert_cmpint (errno, ==, ENOSPC);
	child_priv = _g_mime_stream_file_get_private (GMIME_STREAM_FILE (child));

	g_assert_null (child_priv->fp);
	g_assert_cmpint (g_mime_stream_close (child), ==, 0);
	g_object_unref (child);
#endif
	g_mime_stream_set_bounds (parent, G_MININT64, -1);
	errno = 0;
	g_assert_cmpint (g_mime_stream_length (parent), ==, -1);
	g_assert_cmpint (errno, ==, EINVAL);
	g_mime_stream_set_bounds (parent, 0, -1);
	fp = parent_priv->fp;
	g_mime_stream_file_set_owner (GMIME_STREAM_FILE (parent), FALSE);
	g_assert_cmpint (g_mime_stream_close (parent), ==, 0);
	g_assert_cmpint (fseek (fp, 0, SEEK_SET), ==, 0);
	g_assert_cmpint (fclose (fp), ==, 0);
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
	free_temporary_stream (parent, path);
	testsuite_check_passed ();
}

#ifdef HAVE_MMAP
static GMimeStream *
new_mmap_stream (const char *data, size_t length, char **path)
{
	GMimeStreamFsPrivate *priv;
	GMimeStream *source, *stream;
	int fd;

	source = new_fs_stream (data, length, path);
	priv = _g_mime_stream_fs_get_private (GMIME_STREAM_FS (source));

	fd = priv->fd;
	g_mime_stream_fs_set_owner (GMIME_STREAM_FS (source), FALSE);
	g_object_unref (source);
	stream = g_mime_stream_mmap_new (fd, PROT_READ | PROT_WRITE, MAP_SHARED);
	g_assert_nonnull (stream);

	return stream;
}

static void
test_mmap_substream_reads (void)
{
	GMimeStreamPrivate *nested_priv;
	GMimeStreamMmapPrivate *parent_priv;
	GMimeStream *parent, *child, *nested, *tail, *empty;
	char *path, buffer[16];

	testsuite_check ("Mapped views clip reads to both declared bounds and actual storage");
	parent = new_mmap_stream ("0123456789", 10, &path);
	child = g_mime_stream_substream (parent, 2, 6);
	nested = g_mime_stream_substream (child, 3, -1);
	nested_priv = _g_mime_stream_get_private (nested);

	g_assert_cmpint (nested_priv->bound_end, ==, 6);
	errno = 0;
	g_assert_null (g_mime_stream_substream (child, 1, 4));
	g_assert_cmpint (errno, ==, EINVAL);
	memset (buffer, 0x5a, sizeof (buffer));
	g_assert_cmpint (g_mime_stream_read (child, buffer + 1, 14), ==, 4);
	g_assert_cmpmem (buffer + 1, 4, "2345", 4);
	g_assert_cmpint (buffer[0], ==, 0x5a);
	g_assert_cmpint (buffer[5], ==, 0x5a);
	g_assert_cmpint (g_mime_stream_read (child, buffer, 1), ==, 0);
	g_assert_cmpint (g_mime_stream_read (child, buffer, 1), ==, 0);
	g_assert_cmpint (g_mime_stream_read (nested, buffer, sizeof (buffer)), ==, 3);
	g_assert_cmpmem (buffer, 3, "345", 3);
	empty = g_mime_stream_substream (child, 6, -1);
	g_assert_cmpint (g_mime_stream_read (empty, buffer, 1), ==, 0);
	g_assert_cmpint (g_mime_stream_length (empty), ==, 0);
	tail = g_mime_stream_substream (parent, 8, G_MAXINT64);
	memset (buffer, 0x5a, sizeof (buffer));
	g_assert_cmpint (g_mime_stream_read (tail, buffer + 1, 14), ==, 2);
	g_assert_cmpmem (buffer + 1, 2, "89", 2);
	g_assert_cmpint (buffer[0], ==, 0x5a);
	g_assert_cmpint (buffer[3], ==, 0x5a);
	g_assert_false (g_mime_stream_eos (tail));
	g_assert_cmpint (g_mime_stream_read (tail, buffer, 1), ==, 0);
	g_assert_true (g_mime_stream_eos (tail));
	g_assert_false (g_mime_stream_eos (parent));
	g_assert_cmpint (g_mime_stream_length (tail), ==, G_MAXINT64 - 8);
	g_object_unref (tail);
	tail = g_mime_stream_substream (parent, G_MAXINT64, -1);
	g_assert_cmpint (g_mime_stream_length (tail), ==, 0);
	g_assert_cmpint (g_mime_stream_read (tail, buffer, 1), ==, 0);
	g_assert_true (g_mime_stream_eos (tail));
	g_assert_cmpint (g_mime_stream_reset (tail), ==, 0);
	g_assert_false (g_mime_stream_eos (tail));
	g_assert_cmpint (g_mime_stream_tell (tail), ==, G_MAXINT64);
	g_assert_cmpint (g_mime_stream_tell (parent), ==, 0);
	parent_priv = _g_mime_stream_mmap_get_private (GMIME_STREAM_MMAP (parent));

	g_assert_cmpint (lseek (parent_priv->fd, 0, SEEK_CUR), ==, 0);
	g_object_unref (tail);
	g_object_unref (empty);
	g_object_unref (nested);
	g_object_unref (child);
	free_temporary_stream (parent, path);
	testsuite_check_passed ();
}

static void
test_mmap_substream_writes_seeks (void)
{
	GMimeStreamMmapPrivate *priv;
	GMimeStream *parent, *child, *tail;
	char *path, buffer[16];

	testsuite_check ("Mapped views preserve partial fixed-size writes and checked native seeks");
	parent = new_mmap_stream ("0123456789", 10, &path);
	child = g_mime_stream_substream (parent, 2, 6);
	g_assert_cmpint (g_mime_stream_write (child, "ABCDEFGH", 8), ==, 4);
	errno = 0;
	g_assert_cmpint (g_mime_stream_write (child, "!", 1), ==, -1);
	g_assert_cmpint (errno, ==, EINVAL);
	tail = g_mime_stream_substream (parent, 8, 100);
	g_assert_cmpint (g_mime_stream_write (tail, "XYZ", 3), ==, 2);
	g_assert_cmpint (g_mime_stream_write (tail, "!", 1), ==, 0);
	g_assert_cmpint (g_mime_stream_read (tail, buffer, 1), ==, 0);
	g_assert_true (g_mime_stream_eos (tail));
	g_assert_cmpint (g_mime_stream_seek (tail, 10, GMIME_STREAM_SEEK_SET), ==, 10);
	g_assert_true (g_mime_stream_eos (tail));
	g_assert_cmpint (g_mime_stream_reset (tail), ==, 0);
	g_assert_false (g_mime_stream_eos (tail));
	g_assert_cmpint (g_mime_stream_tell (tail), ==, 8);
	g_assert_cmpint (g_mime_stream_read (tail, buffer, 2), ==, 2);
	g_assert_cmpmem (buffer, 2, "XY", 2);
	g_assert_cmpint (g_mime_stream_length (parent), ==, 10);
	g_assert_cmpint (g_mime_stream_read (parent, buffer, sizeof (buffer)), ==, 10);
	g_assert_cmpmem (buffer, 10, "01ABCD67XY", 10);
	g_assert_cmpint (g_mime_stream_flush (tail), ==, 0);
	g_assert_cmpint (g_mime_stream_seek (child, 0, GMIME_STREAM_SEEK_END), ==, 6);
	g_assert_cmpint (g_mime_stream_seek (child, -1, GMIME_STREAM_SEEK_END), ==, 5);
	errno = 0;
	g_assert_cmpint (g_mime_stream_seek (child, 1, GMIME_STREAM_SEEK_END), ==, -1);
	g_assert_cmpint (errno, ==, EINVAL);
	errno = 0;
	g_assert_cmpint (g_mime_stream_seek (child, G_MAXINT64, GMIME_STREAM_SEEK_CUR), ==, -1);
	g_assert_cmpint (errno, ==, EOVERFLOW);
	errno = 0;
	g_assert_cmpint (g_mime_stream_seek (child, G_MAXINT64, GMIME_STREAM_SEEK_END), ==, -1);
	g_assert_cmpint (errno, ==, EOVERFLOW);
	errno = 0;
	g_assert_cmpint (g_mime_stream_seek (child, 0, (GMimeSeekWhence) 77), ==, -1);
	g_assert_cmpint (errno, ==, EINVAL);
	g_assert_cmpint (g_mime_stream_tell (child), ==, 5);
	g_object_unref (tail);
	tail = g_mime_stream_substream (parent, 2, -1);
	g_assert_cmpint (g_mime_stream_seek (tail, 0, GMIME_STREAM_SEEK_END), ==, 10);
	g_assert_cmpint (g_mime_stream_seek (tail, G_MAXINT64, GMIME_STREAM_SEEK_SET), ==, G_MAXINT64);
	g_assert_cmpint (g_mime_stream_read (tail, buffer, 1), ==, 0);
	g_assert_cmpint (g_mime_stream_write (tail, "!", 1), ==, 0);
	errno = 0;
	g_assert_cmpint (g_mime_stream_seek (tail, 1, GMIME_STREAM_SEEK_CUR), ==, -1);
	g_assert_cmpint (errno, ==, EOVERFLOW);
	/* Unlike growable file streams, mapped END always uses its fixed extent,
	 * even after EOF was observed at a position beyond that extent. */
	g_assert_cmpint (g_mime_stream_seek (tail, -1, GMIME_STREAM_SEEK_END), ==, 9);
	g_assert_false (g_mime_stream_eos (tail));
	g_assert_cmpint (g_mime_stream_seek (tail, G_MININT64, GMIME_STREAM_SEEK_END), ==, 2);
	errno = 0;
	g_assert_cmpint (g_mime_stream_seek (tail, 1, GMIME_STREAM_SEEK_END), ==, -1);
	g_assert_cmpint (errno, ==, EINVAL);
	g_assert_cmpint (g_mime_stream_seek (tail, 3, GMIME_STREAM_SEEK_CUR), ==, 5);
	g_assert_cmpint (g_mime_stream_tell (parent), ==, 10);
	priv = _g_mime_stream_mmap_get_private (GMIME_STREAM_MMAP (parent));

	g_assert_cmpint (lseek (priv->fd, 0, SEEK_END), ==, 10);
	g_assert_cmpint (write (priv->fd, "Z", 1), ==, 1);
	g_assert_cmpint (g_mime_stream_length (parent), ==, 10);
	g_assert_cmpint (g_mime_stream_read (parent, buffer, 1), ==, 0);
	g_assert_cmpint (g_mime_stream_seek (tail, 0, GMIME_STREAM_SEEK_END), ==, 10);
	g_object_unref (tail);
	g_object_unref (child);
	free_temporary_stream (parent, path);
	testsuite_check_passed ();
}

static void
test_mmap_substream_protections (void)
{
	GMimeStreamMmapPrivate *stream_mmap_priv;
	GMimeStreamMmapPrivate *mapped_priv;
	GMimeStreamMmapPrivate *stream_mmap_priv2;
	GMimeStreamPrivate *stream_priv;
	GMimeStreamFsPrivate *parent_priv;
	const gint64 invalid[][2] = { { -1, 8 }, { 0, -2 }, { 8, 2 } };
	GMimeStream *parent, *mapped, *child;
	char *path, buffer[16];
	guint i;
	int fd;

	testsuite_check ("Mapped constructors respect physical extent, protections and descriptor ownership");
	parent = new_mmap_stream ("0123456789", 10, &path);
	stream_mmap_priv = _g_mime_stream_mmap_get_private (GMIME_STREAM_MMAP (parent));

	fd = stream_mmap_priv->fd;
	for (i = 0; i < G_N_ELEMENTS (invalid); i++) {
		errno = 0;
		g_assert_null (g_mime_stream_mmap_new_with_bounds (fd, PROT_READ, MAP_PRIVATE,
								 invalid[i][0], invalid[i][1]));
		g_assert_cmpint (errno, ==, EINVAL);
		g_assert_cmpint (fcntl (fd, F_GETFD), >=, 0);
	}
	mapped = g_mime_stream_mmap_new_with_bounds (dup (fd), PROT_READ, MAP_PRIVATE, 8, 8192);
	g_assert_nonnull (mapped);
	mapped_priv = _g_mime_stream_mmap_get_private (GMIME_STREAM_MMAP (mapped));

	g_assert_cmpuint (mapped_priv->maplen, ==, 10);
	g_assert_cmpint (g_mime_stream_length (mapped), ==, 8184);
	child = g_mime_stream_substream (mapped, 8, -1);
	errno = 0;
	g_assert_cmpint (g_mime_stream_write (child, "!", 1), ==, -1);
	g_assert_cmpint (errno, ==, EACCES);
	g_assert_cmpint (g_mime_stream_tell (child), ==, 8);
	g_assert_cmpint (g_mime_stream_read (child, buffer, sizeof (buffer)), ==, 2);
	g_assert_cmpint (g_mime_stream_read (child, buffer, 1), ==, 0);
	g_object_unref (mapped);
	g_assert_cmpint (g_mime_stream_reset (child), ==, 0);
	g_assert_cmpint (g_mime_stream_read (child, buffer, sizeof (buffer)), ==, 2);
	g_object_unref (child);
	g_assert_cmpint (lseek (fd, 8, SEEK_SET), ==, 8);
	mapped = g_mime_stream_mmap_new (dup (fd), PROT_READ, MAP_PRIVATE);
	g_assert_nonnull (mapped);
	stream_priv = _g_mime_stream_get_private (mapped);

	g_assert_cmpint (stream_priv->bound_start, ==, 8);
	g_assert_cmpint (g_mime_stream_length (mapped), ==, 2);
	g_assert_cmpint (g_mime_stream_seek (mapped, 0, GMIME_STREAM_SEEK_END), ==, 10);
	g_object_unref (mapped);
	g_assert_cmpint (lseek (fd, 0, SEEK_SET), ==, 0);
	mapped = g_mime_stream_mmap_new_with_bounds (dup (fd), PROT_READ, MAP_PRIVATE, 0, 4);
	g_assert_nonnull (mapped);
	stream_mmap_priv2 = _g_mime_stream_mmap_get_private (GMIME_STREAM_MMAP (mapped));

	g_assert_cmpuint (stream_mmap_priv2->maplen, ==, 4);
	g_assert_cmpint (g_mime_stream_read (mapped, buffer, sizeof (buffer)), ==, 4);
	g_assert_cmpmem (buffer, 4, "0123", 4);
	g_object_unref (mapped);
	mapped = g_mime_stream_mmap_new (dup (fd), PROT_WRITE, MAP_SHARED);
	g_assert_nonnull (mapped);
	child = g_mime_stream_substream (mapped, 0, 2);
	errno = 0;
	g_assert_cmpint (g_mime_stream_read (child, buffer, 1), ==, -1);
	g_assert_cmpint (errno, ==, EACCES);
	g_assert_false (g_mime_stream_eos (child));
	g_assert_cmpint (g_mime_stream_write (child, "AB", 2), ==, 2);
	g_assert_cmpint (g_mime_stream_read (parent, buffer, 2), ==, 2);
	g_assert_cmpmem (buffer, 2, "AB", 2);
	g_object_unref (child);
	g_object_unref (mapped);
	mapped = g_mime_stream_mmap_new (dup (fd), PROT_NONE, MAP_PRIVATE);
	g_assert_nonnull (mapped);
	errno = 0;
	g_assert_cmpint (g_mime_stream_read (mapped, buffer, 1), ==, -1);
	g_assert_cmpint (errno, ==, EACCES);
	errno = 0;
	g_assert_cmpint (g_mime_stream_write (mapped, "!", 1), ==, -1);
	g_assert_cmpint (errno, ==, EACCES);
	g_assert_cmpint (g_mime_stream_read (mapped, buffer, 0), ==, 0);
	g_assert_cmpint (g_mime_stream_write (mapped, "!", 0), ==, 0);
	g_object_unref (mapped);
	free_temporary_stream (parent, path);
	parent = new_fs_stream ("", 0, &path);
	parent_priv = _g_mime_stream_fs_get_private (GMIME_STREAM_FS (parent));

	fd = parent_priv->fd;
	errno = 0;
	g_assert_null (g_mime_stream_mmap_new (fd, PROT_READ, MAP_PRIVATE));
	g_assert_cmpint (errno, ==, EINVAL);
	g_assert_cmpint (fcntl (fd, F_GETFD), >=, 0);
	free_temporary_stream (parent, path);
	testsuite_check_passed ();
}

static void
test_mmap_substream_errors (void)
{
	GMimeStream *parent, *child;
	char *path, buffer[2];

	testsuite_check ("Mapped views reject excessive counts and closed or malformed operations");
	parent = new_mmap_stream ("0123456789", 10, &path);
	child = g_mime_stream_substream (parent, 2, 6);
	errno = 0;
	g_assert_cmpint (g_mime_stream_read (child, buffer, (size_t) G_MAXSSIZE + 1), ==, -1);
	g_assert_cmpint (errno, ==, EOVERFLOW);
	errno = 0;
	g_assert_cmpint (g_mime_stream_write (child, "!", (size_t) G_MAXSSIZE + 1), ==, -1);
	g_assert_cmpint (errno, ==, EOVERFLOW);
	g_assert_cmpint (g_mime_stream_tell (child), ==, 2);
	g_mime_stream_set_bounds (child, 6, 2);
	errno = 0;
	g_assert_cmpint (g_mime_stream_read (child, buffer, 1), ==, -1);
	g_assert_cmpint (errno, ==, EINVAL);
	g_mime_stream_set_bounds (child, 2, 6);
	g_assert_cmpint (g_mime_stream_close (child), ==, 0);
	g_assert_cmpint (g_mime_stream_read (parent, buffer, 1), ==, 1);
	g_assert_cmpint (buffer[0], ==, '0');
	errno = 0;
	g_assert_cmpint (g_mime_stream_read (child, buffer, 1), ==, -1);
	g_assert_cmpint (errno, ==, EBADF);
	errno = 0;
	g_assert_cmpint (g_mime_stream_write (child, "!", 1), ==, -1);
	g_assert_cmpint (errno, ==, EBADF);
	errno = 0;
	g_assert_cmpint (g_mime_stream_seek (child, 2, GMIME_STREAM_SEEK_SET), ==, -1);
	g_assert_cmpint (errno, ==, EBADF);
	errno = 0;
	g_assert_cmpint (g_mime_stream_tell (child), ==, -1);
	g_assert_cmpint (errno, ==, EBADF);
	errno = 0;
	g_assert_cmpint (g_mime_stream_length (child), ==, -1);
	g_assert_cmpint (errno, ==, EBADF);
	errno = 0;
	g_assert_cmpint (g_mime_stream_reset (child), ==, -1);
	g_assert_cmpint (errno, ==, EBADF);
	errno = 0;
	g_assert_cmpint (g_mime_stream_flush (child), ==, -1);
	g_assert_cmpint (errno, ==, EBADF);
	errno = 0;
	g_assert_null (g_mime_stream_substream (child, 2, 4));
	g_assert_cmpint (errno, ==, EBADF);
	g_assert_true (g_mime_stream_eos (child));
	g_assert_cmpint (g_mime_stream_read (child, buffer, 0), ==, 0);
	g_assert_cmpint (g_mime_stream_write (child, "!", 0), ==, 0);
	g_assert_cmpint (g_mime_stream_close (child), ==, 0);
	g_object_unref (child);
	free_temporary_stream (parent, path);
	testsuite_check_passed ();
}
#endif

int
main (int argc, char **argv)
{
	int errors;

	g_mime_init ();
	testsuite_init (argc, argv);
	testsuite_start ("Native substreams");
	test_substream_ranges ();
	test_filter_substream_bounds ();
	test_mem_substream_reads ();
	test_mem_substream_writes ();
	test_mem_substream_seeks ();
	test_mem_substream_lifetime ();
	test_mem_substream_errors ();
	test_fs_substream_reads ();
	test_fs_substream_writes ();
	test_fs_substream_seeks ();
	test_fs_substream_eof_growth ();
	test_fs_substream_errors ();
	test_file_substream_reads ();
	test_file_substream_writes_seeks ();
	test_file_substream_eof_growth ();
	test_file_substream_errors ();
#ifdef HAVE_MMAP
	test_mmap_substream_reads ();
	test_mmap_substream_writes_seeks ();
	test_mmap_substream_protections ();
	test_mmap_substream_errors ();
#endif
	testsuite_end ();
	errors = testsuite_exit ();
	g_mime_shutdown ();

	return errors;
}
