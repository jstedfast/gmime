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
#include <time.h>

#include <gmime/gmime.h>
#include <gmime/gmime-stream.h>
#include <gmime/gmime-stream-fs.h>
#include <gmime/gmime-stream-cat.h>
#include <gmime/gmime-stream-mem.h>

#include "testsuite.h"

/* Note: this test suite assumes StreamFs and StreamMem are correct */

extern int verbose;

#define d(x)
#define v(x) if (verbose > 3) x;

static int randfd;

typedef struct {
	TestStreamOneByte parent_object;
	size_t chunk_size, bytes_read, fail_after;
	guint read_calls, seek_calls, reset_calls;
	gboolean nonseekable, zero_read;
	int read_errno;
	size_t bytes_written, write_after;
	guint write_calls, flush_calls, length_calls, tell_calls;
	gboolean zero_write, fail_flush, no_length, no_tell;
	int write_errno;
	gint64 reported_length;
	guint fail_seek_call, fail_length_call;
} TestCatSource;

typedef TestStreamOneByteClass TestCatSourceClass;

G_DEFINE_TYPE (TestCatSource, test_cat_source, TEST_TYPE_STREAM_ONEBYTE)

static ssize_t
cat_source_read (GMimeStream *stream, char *buf, size_t len)
{
	TestCatSource *source = (TestCatSource *) stream;
	ssize_t n;

	source->read_calls++;
	if (source->zero_read) {
		source->zero_read = FALSE;
		return 0;
	}
	if (source->bytes_read == source->fail_after) {
		errno = source->read_errno;
		return -1;
	}

	len = MIN (len, source->chunk_size);
	len = MIN (len, source->fail_after - source->bytes_read);
	n = g_mime_stream_read (source->parent_object.source, buf, len);
	if (n > 0) {
		stream->position += n;
		source->bytes_read += n;
	}
	return n;
}

static gint64
cat_source_tell (GMimeStream *stream)
{
	TestCatSource *source = (TestCatSource *) stream;

	source->tell_calls++;
	if (source->no_tell) {
		errno = ESPIPE;
		return -1;
	}
	return g_mime_stream_tell (source->parent_object.source);
}

static gint64
cat_source_length (GMimeStream *stream)
{
	TestCatSource *source = (TestCatSource *) stream;

	source->length_calls++;
	if (source->length_calls == source->fail_length_call) {
		errno = EIO;
		return -1;
	}
	if (source->no_length) {
		errno = ESPIPE;
		return -1;
	}
	return source->reported_length >= 0 ? source->reported_length :
		g_mime_stream_length (source->parent_object.source);
}

static ssize_t
cat_source_write (GMimeStream *stream, const char *buf, size_t len)
{
	TestCatSource *source = (TestCatSource *) stream;
	ssize_t n;

	source->write_calls++;
	if (source->bytes_written == source->write_after) {
		if (source->zero_write)
			return 0;
		errno = source->write_errno;
		return -1;
	}
	len = MIN (len, source->chunk_size);
	len = MIN (len, source->write_after - source->bytes_written);
	n = g_mime_stream_write (source->parent_object.source, buf, len);
	if (n > 0) {
		stream->position += n;
		source->bytes_written += n;
	}
	return n;
}

static int
cat_source_flush (GMimeStream *stream)
{
	TestCatSource *source = (TestCatSource *) stream;

	source->flush_calls++;
	if (source->fail_flush) {
		errno = EIO;
		return -1;
	}
	return g_mime_stream_flush (source->parent_object.source);
}

static gint64
cat_source_seek (GMimeStream *stream, gint64 offset, GMimeSeekWhence whence)
{
	TestCatSource *source = (TestCatSource *) stream;
	gint64 position;

	source->seek_calls++;
	if (source->seek_calls == source->fail_seek_call) {
		errno = EIO;
		return -1;
	}
	if (source->nonseekable) {
		errno = ESPIPE;
		return -1;
	}

	position = g_mime_stream_seek (source->parent_object.source, offset, whence);
	if (position != -1)
		stream->position = position;
	return position;
}

static int
cat_source_reset (GMimeStream *stream)
{
	TestCatSource *source = (TestCatSource *) stream;

	source->reset_calls++;
	if (source->nonseekable) {
		errno = ESPIPE;
		return -1;
	}

	if (g_mime_stream_reset (source->parent_object.source) == -1)
		return -1;
	source->bytes_read = 0;
	source->bytes_written = 0;
	return 0;
}

static void
test_cat_source_class_init (TestCatSourceClass *klass)
{
	GMimeStreamClass *stream_class = GMIME_STREAM_CLASS (klass);

	stream_class->read = cat_source_read;
	stream_class->write = cat_source_write;
	stream_class->flush = cat_source_flush;
	stream_class->tell = cat_source_tell;
	stream_class->length = cat_source_length;
	stream_class->seek = cat_source_seek;
	stream_class->reset = cat_source_reset;
}

static void
test_cat_source_init (TestCatSource *source)
{
	source->chunk_size = 1;
	source->fail_after = G_MAXSIZE;
	source->write_after = G_MAXSIZE;
	source->write_errno = ENOSPC;
	source->reported_length = -1;
	source->read_errno = EIO;
}

static TestCatSource *
cat_source_new (const char *data, size_t len)
{
	TestCatSource *source = g_object_new (test_cat_source_get_type (), NULL);

	g_mime_stream_construct ((GMimeStream *) source, 0, -1);
	source->parent_object.source = g_mime_stream_mem_new_with_buffer (data, len);
	return source;
}

static void
test_cat_read_fill (void)
{
	const char expected[] = "a\0bcdef";
	size_t chunk, request, offset;
	TestCatSource *sources[5], *appended;
	guint calls[5], i;
	GMimeStream *cat;
	char buf[12];
	ssize_t n;

	testsuite_check ("Chained reads fill across nonseekable, prepositioned children");

	for (chunk = 1; chunk <= 4; chunk++) {
		for (request = 1; request <= 9; request++) {
			cat = g_mime_stream_cat_new ();
			sources[0] = cat_source_new ("", 0);
			sources[1] = cat_source_new ("XXa\0b", 5);
			sources[2] = cat_source_new ("", 0);
			sources[3] = cat_source_new ("YYYcdef", 7);
			sources[4] = cat_source_new ("", 0);
			g_assert_cmpint (g_mime_stream_seek (sources[1]->parent_object.source, 2, GMIME_STREAM_SEEK_SET), ==, 2);
			g_assert_cmpint (g_mime_stream_seek (sources[3]->parent_object.source, 3, GMIME_STREAM_SEEK_SET), ==, 3);
			for (i = 0; i < G_N_ELEMENTS (sources); i++) {
				sources[i]->chunk_size = chunk;
				sources[i]->nonseekable = TRUE;
				g_assert_cmpint (g_mime_stream_cat_add_source (GMIME_STREAM_CAT (cat), (GMimeStream *) sources[i]), ==, 0);
			}

			g_assert_cmpint (g_mime_stream_read (cat, buf, 0), ==, 0);
			for (i = 0; i < G_N_ELEMENTS (sources); i++)
				g_assert_cmpuint (sources[i]->read_calls, ==, 0);

			/* Compare every read with the corresponding slice of the master data. */
			offset = 0;
			while (offset < sizeof (expected) - 1) {
				memset (buf, 0x5a, sizeof (buf));
				n = g_mime_stream_read (cat, buf + 1, request);
				g_assert_cmpint (n, ==, MIN (request, sizeof (expected) - 1 - offset));
				g_assert_cmpmem (buf + 1, n, expected + offset, n);
				g_assert_cmpint (buf[0], ==, 0x5a);
				for (i = (guint) n + 1; i < sizeof (buf); i++)
					g_assert_cmpint (buf[i], ==, 0x5a);
				offset += n;
				g_assert_cmpint (g_mime_stream_tell (cat), ==, offset);
			}

			g_assert_cmpint (g_mime_stream_read (cat, buf, sizeof (buf)), ==, 0);
			g_assert_true (g_mime_stream_eos (cat));
			for (i = 0; i < G_N_ELEMENTS (sources); i++)
				calls[i] = sources[i]->read_calls;
			g_assert_cmpint (g_mime_stream_read (cat, buf, sizeof (buf)), ==, 0);
			g_assert_cmpint (g_mime_stream_read (cat, buf, sizeof (buf)), ==, 0);
			for (i = 0; i < G_N_ELEMENTS (sources); i++) {
				g_assert_cmpuint (sources[i]->read_calls, ==, calls[i]);
				g_assert_cmpuint (sources[i]->seek_calls, ==, 0);
				g_assert_cmpuint (sources[i]->reset_calls, ==, 0);
				g_object_unref (sources[i]);
			}

			appended = cat_source_new ("gh", 2);
			appended->nonseekable = TRUE;
			g_mime_stream_cat_add_source (GMIME_STREAM_CAT (cat), (GMimeStream *) appended);
			g_assert_false (g_mime_stream_eos (cat));
			g_assert_cmpint (g_mime_stream_read (cat, buf, sizeof (buf)), ==, 2);
			g_assert_cmpmem (buf, 2, "gh", 2);
			g_assert_cmpint (g_mime_stream_tell (cat), ==, 9);
			g_assert_cmpuint (appended->seek_calls, ==, 0);
			g_assert_cmpuint (appended->reset_calls, ==, 0);
			g_object_unref (appended);
			g_object_unref (cat);
		}
	}

	testsuite_check_passed ();
}

static void
test_cat_read_errors (void)
{
	const struct {
		size_t after;
		int error;
	} failures[] = {
		{ 0, EIO }, { 1, EIO }, { 2, EIO },
		{ 0, EINVAL }, { 1, EINVAL },
		{ 0, EAGAIN }, { 1, EAGAIN }, { 2, EAGAIN }
	};
	TestCatSource *first, *second, *third;
	GMimeStream *cat;
	size_t fail_after;
	char buf[8];
	guint i;

	testsuite_check ("Chained read errors preserve partial progress and the failing child");

	for (i = 0; i < G_N_ELEMENTS (failures); i++) {
		fail_after = failures[i].after;
		cat = g_mime_stream_cat_new ();
		first = cat_source_new ("ab", 2);
		second = cat_source_new ("cd", 2);
		third = cat_source_new ("ef", 2);
		first->nonseekable = second->nonseekable = third->nonseekable = TRUE;
		second->fail_after = fail_after;
		second->read_errno = failures[i].error;
		g_mime_stream_cat_add_source (GMIME_STREAM_CAT (cat), (GMimeStream *) first);
		g_mime_stream_cat_add_source (GMIME_STREAM_CAT (cat), (GMimeStream *) second);
		g_mime_stream_cat_add_source (GMIME_STREAM_CAT (cat), (GMimeStream *) third);

		memset (buf, 0x5a, sizeof (buf));
		errno = 0;
		g_assert_cmpint (g_mime_stream_read (cat, buf, 6), ==, 2 + fail_after);
		g_assert_cmpint (errno, ==, failures[i].error);
		g_assert_cmpmem (buf, 2 + fail_after, "abcdef", 2 + fail_after);
		g_assert_cmpint (buf[2 + fail_after], ==, 0x5a);
		g_assert_cmpint (g_mime_stream_tell (cat), ==, 2 + fail_after);
		g_assert_cmpuint (third->read_calls, ==, 0);
		g_assert_false (g_mime_stream_eos (cat));

		errno = 0;
		g_assert_cmpint (g_mime_stream_read (cat, buf, 6), ==, -1);
		g_assert_cmpint (errno, ==, failures[i].error);
		g_assert_cmpint (g_mime_stream_tell (cat), ==, 2 + fail_after);
		g_assert_cmpuint (third->read_calls, ==, 0);
		second->fail_after = G_MAXSIZE;
		g_assert_cmpint (g_mime_stream_read (cat, buf, 6), ==, 4 - fail_after);
		g_assert_cmpmem (buf, 4 - fail_after, "cdef" + fail_after, 4 - fail_after);
		g_assert_cmpint (g_mime_stream_tell (cat), ==, 6);
		g_assert_cmpuint (second->seek_calls, ==, 0);
		g_assert_cmpuint (second->reset_calls, ==, 0);

		g_object_unref (first);
		g_object_unref (second);
		g_object_unref (third);
		g_object_unref (cat);
	}

	testsuite_check_passed ();
}

static void
test_cat_read_zero (void)
{
	TestCatSource *first = cat_source_new ("ab", 2);
	TestCatSource *second = cat_source_new ("cd", 2);
	GMimeStream *cat = g_mime_stream_cat_new ();
	char buf[8];

	testsuite_check ("Transient zero reads do not skip children");
	first->zero_read = second->zero_read = TRUE;
	g_mime_stream_cat_add_source (GMIME_STREAM_CAT (cat), (GMimeStream *) first);
	g_mime_stream_cat_add_source (GMIME_STREAM_CAT (cat), (GMimeStream *) second);
	g_assert_cmpint (g_mime_stream_read (cat, buf, 4), ==, 0);
	g_assert_false (g_mime_stream_eos (cat));
	g_assert_cmpint (g_mime_stream_tell (cat), ==, 0);
	g_assert_cmpuint (second->read_calls, ==, 0);
	g_assert_cmpint (g_mime_stream_read (cat, buf, 4), ==, 2);
	g_assert_cmpmem (buf, 2, "ab", 2);
	g_assert_false (g_mime_stream_eos (cat));
	g_assert_cmpint (g_mime_stream_read (cat, buf, 4), ==, 2);
	g_assert_cmpmem (buf, 2, "cd", 2);
	g_assert_cmpint (g_mime_stream_tell (cat), ==, 4);
	g_assert_cmpint (g_mime_stream_read (cat, buf, 4), ==, 0);
	g_object_unref (first);
	g_object_unref (second);
	g_object_unref (cat);
	testsuite_check_passed ();
}

static void
test_cat_read_reposition (void)
{
	TestCatSource *first, *second;
	GMimeStream *cat;
	char buf[8];
	gint64 offset;

	testsuite_check ("Chained reads preserve native seek, reset and finite bounds");
	cat = g_mime_stream_cat_new ();
	first = cat_source_new ("ab", 2);
	second = cat_source_new ("cdef", 4);
	g_mime_stream_cat_add_source (GMIME_STREAM_CAT (cat), (GMimeStream *) first);
	g_mime_stream_cat_add_source (GMIME_STREAM_CAT (cat), (GMimeStream *) second);
	g_assert_cmpint (g_mime_stream_read (cat, buf, sizeof (buf)), ==, 6);
	for (offset = 0; offset < 6; offset++) {
		g_assert_cmpint (g_mime_stream_seek (cat, offset, GMIME_STREAM_SEEK_SET), ==, offset);
		g_assert_cmpint (g_mime_stream_read (cat, buf, sizeof (buf)), ==, 6 - offset);
		g_assert_cmpmem (buf, 6 - offset, "abcdef" + offset, 6 - offset);
	}
	g_assert_cmpint (g_mime_stream_reset (cat), ==, 0);
	g_mime_stream_set_bounds (cat, 0, 3);
	g_assert_cmpint (g_mime_stream_read (cat, buf, sizeof (buf)), ==, 3);
	g_assert_cmpmem (buf, 3, "abc", 3);
	g_assert_true (g_mime_stream_eos (cat));
	g_assert_cmpint (g_mime_stream_read (cat, buf, sizeof (buf)), ==, 0);
	g_assert_cmpint (g_mime_stream_tell (second->parent_object.source), ==, 1);
	g_object_unref (first);
	g_object_unref (second);
	g_object_unref (cat);

	cat = g_mime_stream_cat_new ();
	g_assert_cmpint (g_mime_stream_read (cat, buf, sizeof (buf)), ==, 0);
	first = cat_source_new ("", 0);
	second = cat_source_new ("", 0);
	g_mime_stream_cat_add_source (GMIME_STREAM_CAT (cat), (GMimeStream *) first);
	g_mime_stream_cat_add_source (GMIME_STREAM_CAT (cat), (GMimeStream *) second);
	g_assert_cmpint (g_mime_stream_read (cat, buf, sizeof (buf)), ==, 0);
	g_assert_true (g_mime_stream_eos (cat));
	g_mime_stream_write (first->parent_object.source, "a", 1);
	g_mime_stream_write (second->parent_object.source, "b", 1);
	g_assert_cmpint (g_mime_stream_reset (cat), ==, 0);
	g_assert_cmpuint (first->reset_calls, ==, 1);
	g_assert_cmpuint (second->reset_calls, ==, 1);
	g_assert_cmpint (g_mime_stream_read (cat, buf, sizeof (buf)), ==, 2);
	g_assert_cmpmem (buf, 2, "ab", 2);
	g_object_unref (first);
	g_object_unref (second);
	g_object_unref (cat);
	testsuite_check_passed ();
}

static void
test_cat_read_shared_source (void)
{
	TestCatSource *source = cat_source_new ("ab", 2);
	GMimeStream *cat = g_mime_stream_cat_new ();
	char buf[8];

	testsuite_check ("Duplicate child references share their source position");
	source->nonseekable = TRUE;
	g_mime_stream_cat_add_source (GMIME_STREAM_CAT (cat), (GMimeStream *) source);
	g_mime_stream_cat_add_source (GMIME_STREAM_CAT (cat), (GMimeStream *) source);
	g_assert_cmpint (g_mime_stream_read (cat, buf, sizeof (buf)), ==, 2);
	g_assert_cmpmem (buf, 2, "ab", 2);
	g_assert_cmpuint (source->read_calls, ==, 4);
	g_assert_cmpuint (source->seek_calls, ==, 0);
	g_assert_cmpuint (source->reset_calls, ==, 0);
	g_assert_cmpint (g_mime_stream_close (cat), ==, 0);
	g_assert_cmpint (g_mime_stream_reset ((GMimeStream *) source), ==, -1);
	g_assert_cmpint (errno, ==, ESPIPE);
	source->nonseekable = FALSE;
	g_assert_cmpint (g_mime_stream_reset ((GMimeStream *) source), ==, 0);
	g_assert_cmpint (g_mime_stream_read ((GMimeStream *) source, buf, 1), ==, 1);
	g_assert_cmpint (buf[0], ==, 'a');
	g_object_unref (cat);
	g_object_unref (source);
	testsuite_check_passed ();
}

static void
test_cat_read_filtered_pipe (void)
{
	GMimeStream *cat, *source, *filtered, *tail;
	const char *inputs[] = { "", "abc" };
	GMimeFilter *filter;
	char buf[16];
	int fds[2];
	guint i;
	size_t len;

	testsuite_check ("Chained reads drain filter completion from real nonseekable pipes");
	for (i = 0; i < G_N_ELEMENTS (inputs); i++) {
		len = strlen (inputs[i]);
		g_assert_cmpint (pipe (fds), ==, 0);
		if (len > 0)
			g_assert_cmpint (write (fds[1], inputs[i], len), ==, len);
		g_assert_cmpint (close (fds[1]), ==, 0);
		source = g_mime_stream_pipe_new (fds[0]);
		filtered = g_mime_stream_filter_new (source);
		filter = g_mime_filter_unix2dos_new (TRUE);
		g_mime_stream_filter_add (GMIME_STREAM_FILTER (filtered), filter);
		g_object_unref (filter);
		tail = g_mime_stream_mem_new_with_buffer ("Z", 1);
		cat = g_mime_stream_cat_new ();
		g_mime_stream_cat_add_source (GMIME_STREAM_CAT (cat), filtered);
		g_mime_stream_cat_add_source (GMIME_STREAM_CAT (cat), tail);
		g_object_unref (filtered);
		g_object_unref (tail);

		/* Complete the filtered child before moving on to the following source. */
		memset (buf, 0x5a, sizeof (buf));
		g_assert_cmpint (g_mime_stream_read (cat, buf, sizeof (buf)), ==, len + 3);
		g_assert_cmpmem (buf, len, inputs[i], len);
		g_assert_cmpmem (buf + len, 3, "\r\nZ", 3);
		g_assert_cmpint (buf[len + 3], ==, 0x5a);
		g_assert_true (g_mime_stream_eos (cat));
		g_assert_cmpint (g_mime_stream_read (cat, buf, sizeof (buf)), ==, 0);
		g_assert_cmpint (g_mime_stream_close (cat), ==, 0);
		g_assert_cmpint (g_mime_stream_read (source, buf, sizeof (buf)), ==, 0);
		g_object_unref (source);
		g_object_unref (cat);
	}
	testsuite_check_passed ();
}

static void
test_cat_read_empty_bounded_filter (void)
{
	GMimeStream *source = g_mime_stream_mem_new ();
	GMimeStream *filtered, *cat;
	GMimeFilter *filter;
	char buf[8];

	testsuite_check ("Empty bounded filters complete before concatenation EOF");
	g_mime_stream_set_bounds (source, 0, 0);
	filtered = g_mime_stream_filter_new (source);
	g_object_unref (source);
	filter = g_mime_filter_unix2dos_new (TRUE);
	g_mime_stream_filter_add (GMIME_STREAM_FILTER (filtered), filter);
	g_object_unref (filter);
	cat = g_mime_stream_cat_new ();
	g_mime_stream_cat_add_source (GMIME_STREAM_CAT (cat), filtered);
	g_object_unref (filtered);
	g_assert_cmpint (g_mime_stream_read (cat, buf, 1), ==, 1);
	g_assert_cmpint (buf[0], ==, '\r');
	g_assert_false (g_mime_stream_eos (cat));
	g_assert_cmpint (g_mime_stream_read (cat, buf, sizeof (buf)), ==, 1);
	g_assert_cmpint (buf[0], ==, '\n');
	g_assert_cmpint (g_mime_stream_read (cat, buf, sizeof (buf)), ==, 0);
	g_assert_true (g_mime_stream_eos (cat));
	g_object_unref (cat);
	testsuite_check_passed ();
}

static void
test_cat_read_overflow (void)
{
	TestCatSource *source = cat_source_new ("ab", 2);
	GMimeStream *cat = g_mime_stream_cat_new ();
	char buf[4];

	testsuite_check ("Chained reads preserve representable progress at the position limit");
	g_mime_stream_construct (cat, G_MAXINT64 - 1, -1);
	g_mime_stream_cat_add_source (GMIME_STREAM_CAT (cat), (GMimeStream *) source);
	errno = 0;
	g_assert_cmpint (g_mime_stream_read (cat, buf, sizeof (buf)), ==, 1);
	g_assert_cmpint (buf[0], ==, 'a');
	g_assert_cmpint (errno, ==, EOVERFLOW);
	g_assert_cmpint (g_mime_stream_tell (cat), ==, G_MAXINT64);
	g_assert_cmpuint (source->bytes_read, ==, 1);
	errno = 0;
	g_assert_cmpint (g_mime_stream_read (cat, buf, sizeof (buf)), ==, -1);
	g_assert_cmpint (errno, ==, EOVERFLOW);
	g_assert_cmpuint (source->bytes_read, ==, 1);
	g_object_unref (source);
	g_object_unref (cat);
	testsuite_check_passed ();
}

static void
test_cat_seek_boundaries (void)
{
	TestCatSource *sources[4];
	GMimeStream *cat = g_mime_stream_cat_new ();
	char buf[8];
	guint i, calls;

	testsuite_check ("Chained seeks physically position children and preserve boundary selection");
	sources[0] = cat_source_new ("ab", 2);
	sources[1] = cat_source_new ("", 0);
	sources[2] = cat_source_new ("cd", 2);
	sources[3] = cat_source_new ("ef", 2);
	for (i = 0; i < G_N_ELEMENTS (sources); i++)
		g_mime_stream_cat_add_source (GMIME_STREAM_CAT (cat), (GMimeStream *) sources[i]);
	g_assert_cmpint (g_mime_stream_length (cat), ==, 6);
	g_assert_cmpint (g_mime_stream_seek (cat, 2, GMIME_STREAM_SEEK_SET), ==, 2);
	g_assert_cmpint (g_mime_stream_tell (sources[0]->parent_object.source), ==, 2);
	g_assert_cmpuint (sources[1]->seek_calls, ==, 0);
	g_assert_cmpuint (sources[2]->seek_calls, ==, 0);
	g_assert_cmpint (g_mime_stream_flush (cat), ==, 0);
	g_assert_cmpuint (sources[0]->flush_calls, ==, 1);
	g_assert_cmpuint (sources[2]->flush_calls, ==, 0);
	g_assert_cmpint (g_mime_stream_write (cat, "X", 1), ==, 1);
	g_assert_cmpint (g_mime_stream_seek (cat, 0, GMIME_STREAM_SEEK_END), ==, 6);
	for (i = 0; i < G_N_ELEMENTS (sources); i++) {
		g_assert_cmpint (g_mime_stream_tell (sources[i]->parent_object.source), ==, i == 1 ? 0 : 2);
		g_assert_cmpuint (sources[i]->reset_calls, ==, 0);
	}
	g_assert_false (g_mime_stream_eos (cat));
	g_assert_cmpint (g_mime_stream_flush (cat), ==, 0);
	g_assert_cmpuint (sources[3]->flush_calls, ==, 1);
	g_assert_cmpint (g_mime_stream_seek (cat, 0, GMIME_STREAM_SEEK_SET), ==, 0);
	for (i = 0; i < G_N_ELEMENTS (sources); i++) {
		g_assert_cmpint (g_mime_stream_tell (sources[i]->parent_object.source), ==, 0);
		g_assert_cmpuint (sources[i]->reset_calls, ==, 0);
	}
	g_assert_cmpint (g_mime_stream_read (cat, buf, sizeof (buf)), ==, 6);
	g_assert_cmpmem (buf, 6, "abXdef", 6);
	g_assert_cmpint (g_mime_stream_read (cat, buf, sizeof (buf)), ==, 0);
	calls = sources[3]->seek_calls;
	g_assert_cmpint (g_mime_stream_seek (cat, 0, GMIME_STREAM_SEEK_END), ==, 6);
	g_assert_true (g_mime_stream_eos (cat));
	g_assert_cmpuint (sources[3]->seek_calls, ==, calls);
	g_assert_cmpint (g_mime_stream_seek (cat, -1, GMIME_STREAM_SEEK_END), ==, 5);
	g_assert_false (g_mime_stream_eos (cat));
	g_assert_cmpint (g_mime_stream_read (cat, buf, 1), ==, 1);
	g_assert_cmpint (buf[0], ==, 'f');
	for (i = 0; i < G_N_ELEMENTS (sources); i++)
		g_object_unref (sources[i]);
	g_object_unref (cat);
	testsuite_check_passed ();
}

static void
test_cat_seek_unvisited (void)
{
	TestCatSource *first = cat_source_new ("ab", 2);
	TestCatSource *tail = cat_source_new ("cd", 2);
	GMimeStream *cat = g_mime_stream_cat_new ();
	char buf[4];

	testsuite_check ("Chained seeks leave unvisited children and same-position EOF unchanged");
	g_mime_stream_seek (tail->parent_object.source, 1, GMIME_STREAM_SEEK_SET);
	g_mime_stream_cat_add_source (GMIME_STREAM_CAT (cat), (GMimeStream *) first);
	g_mime_stream_cat_add_source (GMIME_STREAM_CAT (cat), (GMimeStream *) tail);
	g_assert_cmpint (g_mime_stream_seek (cat, 1, GMIME_STREAM_SEEK_SET), ==, 1);
	g_assert_cmpint (g_mime_stream_seek (cat, -1, GMIME_STREAM_SEEK_CUR), ==, 0);
	g_assert_cmpuint (tail->seek_calls, ==, 0);
	g_assert_cmpuint (tail->reset_calls, ==, 0);
	g_assert_cmpint (g_mime_stream_tell (tail->parent_object.source), ==, 1);
	g_assert_cmpint (g_mime_stream_reset (cat), ==, 0);
	g_assert_cmpint (g_mime_stream_tell (tail->parent_object.source), ==, 0);
	g_assert_cmpuint (first->reset_calls, ==, 1);
	g_assert_cmpuint (tail->reset_calls, ==, 1);
	g_mime_stream_seek (tail->parent_object.source, 1, GMIME_STREAM_SEEK_SET);
	g_assert_cmpint (g_mime_stream_read (cat, buf, sizeof (buf)), ==, 3);
	g_assert_cmpmem (buf, 3, "abd", 3);
	g_object_unref (cat);
	g_object_unref (first);
	g_object_unref (tail);

	first = cat_source_new ("", 0);
	tail = cat_source_new ("", 0);
	cat = g_mime_stream_cat_new ();
	g_mime_stream_cat_add_source (GMIME_STREAM_CAT (cat), (GMimeStream *) first);
	g_mime_stream_cat_add_source (GMIME_STREAM_CAT (cat), (GMimeStream *) tail);
	g_assert_cmpint (g_mime_stream_seek (cat, 0, GMIME_STREAM_SEEK_END), ==, 0);
	g_assert_cmpint (g_mime_stream_read (cat, buf, 1), ==, 0);
	g_assert_cmpint (g_mime_stream_seek (cat, 0, GMIME_STREAM_SEEK_CUR), ==, 0);
	g_assert_true (g_mime_stream_eos (cat));
	g_assert_cmpint (g_mime_stream_write (cat, "A", 1), ==, 1);
	g_assert_cmpint (g_mime_stream_seek (cat, 0, GMIME_STREAM_SEEK_SET), ==, 0);
	g_assert_cmpint (g_mime_stream_read (cat, buf, 1), ==, 1);
	g_assert_cmpint (buf[0], ==, 'A');
	g_assert_cmpuint (first->seek_calls, ==, 0);
	g_assert_cmpuint (tail->seek_calls, ==, 1);
	g_object_unref (cat);
	g_object_unref (first);
	g_object_unref (tail);
	testsuite_check_passed ();
}

static void
test_cat_seek_errors (void)
{
	TestCatSource *first = cat_source_new ("ab", 2);
	TestCatSource *second = cat_source_new ("cd", 2);
	TestCatSource *tail = cat_source_new ("ef", 2);
	GMimeStream *cat = g_mime_stream_cat_new ();
	char buf[8];

	testsuite_check ("Chained seek failures expose errors and retain completed positioning");
	g_mime_stream_cat_add_source (GMIME_STREAM_CAT (cat), (GMimeStream *) first);
	g_mime_stream_cat_add_source (GMIME_STREAM_CAT (cat), (GMimeStream *) second);
	g_mime_stream_cat_add_source (GMIME_STREAM_CAT (cat), (GMimeStream *) tail);
	tail->no_length = TRUE;
	errno = 0;
	g_assert_cmpint (g_mime_stream_seek (cat, 3, GMIME_STREAM_SEEK_SET), ==, -1);
	g_assert_cmpint (errno, ==, ESPIPE);
	g_assert_cmpuint (first->seek_calls, ==, 0);
	tail->no_length = FALSE;
	first->no_tell = TRUE;
	errno = 0;
	g_assert_cmpint (g_mime_stream_seek (cat, 1, GMIME_STREAM_SEEK_SET), ==, -1);
	g_assert_cmpint (errno, ==, ESPIPE);
	first->no_tell = FALSE;
	second->fail_length_call = second->length_calls + 2;
	errno = 0;
	g_assert_cmpint (g_mime_stream_seek (cat, 3, GMIME_STREAM_SEEK_SET), ==, -1);
	g_assert_cmpint (errno, ==, EIO);
	g_assert_cmpint (g_mime_stream_tell (cat), ==, 2);
	g_assert_cmpint (g_mime_stream_tell (first->parent_object.source), ==, 2);
	g_assert_cmpuint (second->seek_calls, ==, 0);
	second->nonseekable = TRUE;
	errno = 0;
	g_assert_cmpint (g_mime_stream_seek (cat, 3, GMIME_STREAM_SEEK_SET), ==, -1);
	g_assert_cmpint (errno, ==, ESPIPE);
	g_assert_cmpint (g_mime_stream_tell (cat), ==, 2);
	second->nonseekable = FALSE;
	g_assert_cmpint (g_mime_stream_seek (cat, 3, GMIME_STREAM_SEEK_SET), ==, 3);
	first->fail_seek_call = first->seek_calls + 1;
	errno = 0;
	g_assert_cmpint (g_mime_stream_seek (cat, 1, GMIME_STREAM_SEEK_SET), ==, -1);
	g_assert_cmpint (errno, ==, EIO);
	g_assert_cmpint (g_mime_stream_tell (cat), ==, 0);
	g_assert_cmpint (g_mime_stream_tell (second->parent_object.source), ==, 1);
	g_assert_cmpint (g_mime_stream_reset (cat), ==, 0);
	g_assert_cmpint (g_mime_stream_seek (cat, 0, GMIME_STREAM_SEEK_END), ==, 6);
	tail->fail_seek_call = tail->seek_calls + 1;
	errno = 0;
	g_assert_cmpint (g_mime_stream_seek (cat, 1, GMIME_STREAM_SEEK_SET), ==, -1);
	g_assert_cmpint (errno, ==, EIO);
	g_assert_cmpint (g_mime_stream_tell (cat), ==, 1);
	g_assert_cmpint (g_mime_stream_tell (first->parent_object.source), ==, 1);
	g_assert_cmpint (g_mime_stream_tell (second->parent_object.source), ==, 0);
	g_assert_cmpint (g_mime_stream_tell (tail->parent_object.source), ==, 2);
	/* An explicit reset recovers all children after a partial backward seek. */
	g_assert_cmpint (g_mime_stream_reset (cat), ==, 0);
	g_assert_cmpint (g_mime_stream_read (cat, buf, sizeof (buf)), ==, 6);
	g_assert_cmpmem (buf, 6, "abcdef", 6);
	tail->fail_seek_call = tail->seek_calls + 1;
	errno = 0;
	g_assert_cmpint (g_mime_stream_seek (cat, 0, GMIME_STREAM_SEEK_SET), ==, -1);
	g_assert_cmpint (errno, ==, EIO);
	g_assert_cmpint (g_mime_stream_tell (cat), ==, 0);
	g_assert_cmpint (g_mime_stream_tell (tail->parent_object.source), ==, 2);
	/* Position zero alone does not mean that all children have been reset. */
	g_assert_cmpint (g_mime_stream_reset (cat), ==, 0);
	g_assert_cmpint (g_mime_stream_tell (tail->parent_object.source), ==, 0);
	g_assert_cmpint (g_mime_stream_read (cat, buf, sizeof (buf)), ==, 6);
	g_assert_cmpmem (buf, 6, "abcdef", 6);
	g_object_unref (cat);
	g_object_unref (first);
	g_object_unref (second);
	g_object_unref (tail);
	testsuite_check_passed ();
}

static void
test_cat_seek_limits (void)
{
	TestCatSource *first = cat_source_new ("ab", 2);
	TestCatSource *tail = cat_source_new ("cdef", 4);
	GMimeStream *cat = g_mime_stream_cat_new ();
	GMimeStream *head, *end;
	char buf[8];

	testsuite_check ("Chained lengths and seek offsets enforce native bounds without overflow");
	errno = 0;
	g_assert_cmpint (g_mime_stream_seek (cat, 0, GMIME_STREAM_SEEK_SET), ==, -1);
	g_assert_cmpint (errno, ==, EBADF);
	g_mime_stream_cat_add_source (GMIME_STREAM_CAT (cat), (GMimeStream *) first);
	g_mime_stream_cat_add_source (GMIME_STREAM_CAT (cat), (GMimeStream *) tail);
	errno = 0;
	g_assert_cmpint (g_mime_stream_seek (cat, 0, (GMimeSeekWhence) 99), ==, -1);
	g_assert_cmpint (errno, ==, EINVAL);
	errno = 0;
	g_assert_cmpint (g_mime_stream_seek (cat, -1, GMIME_STREAM_SEEK_SET), ==, -1);
	g_assert_cmpint (errno, ==, EINVAL);
	errno = 0;
	g_assert_cmpint (g_mime_stream_seek (cat, 7, GMIME_STREAM_SEEK_SET), ==, -1);
	g_assert_cmpint (errno, ==, EINVAL);
	first->reported_length = G_MAXINT64 - 4;
	g_assert_cmpint (g_mime_stream_length (cat), ==, G_MAXINT64);
	errno = 0;
	g_assert_cmpint (g_mime_stream_seek (cat, 1, GMIME_STREAM_SEEK_END), ==, -1);
	g_assert_cmpint (errno, ==, EOVERFLOW);
	errno = 0;
	g_assert_cmpint (g_mime_stream_seek (cat, G_MININT64, GMIME_STREAM_SEEK_END), ==, -1);
	g_assert_cmpint (errno, ==, EINVAL);
	first->reported_length = G_MAXINT64;
	errno = 0;
	g_assert_cmpint (g_mime_stream_length (cat), ==, -1);
	g_assert_cmpint (errno, ==, EOVERFLOW);
	errno = 0;
	g_assert_cmpint (g_mime_stream_seek (cat, 1, GMIME_STREAM_SEEK_SET), ==, -1);
	g_assert_cmpint (errno, ==, EOVERFLOW);
	g_assert_cmpuint (first->seek_calls, ==, 0);
	g_mime_stream_construct (cat, 1, -1);
	first->reported_length = G_MAXINT64 - 4;
	errno = 0;
	g_assert_cmpint (g_mime_stream_seek (cat, 0, GMIME_STREAM_SEEK_END), ==, -1);
	g_assert_cmpint (errno, ==, EOVERFLOW);
	errno = 0;
	g_assert_cmpint (g_mime_stream_seek (cat, G_MAXINT64, GMIME_STREAM_SEEK_CUR), ==, -1);
	g_assert_cmpint (errno, ==, EOVERFLOW);
	first->reported_length = -1;
	g_mime_stream_construct (cat, 3, 7);
	g_assert_cmpint (g_mime_stream_length (cat), ==, 4);
	errno = 0;
	g_assert_cmpint (g_mime_stream_seek (cat, 2, GMIME_STREAM_SEEK_SET), ==, -1);
	g_assert_cmpint (errno, ==, EINVAL);
	g_assert_cmpint (g_mime_stream_seek (cat, 0, GMIME_STREAM_SEEK_END), ==, 7);
	g_assert_cmpint (g_mime_stream_tell (first->parent_object.source), ==, 2);
	g_assert_cmpint (g_mime_stream_tell (tail->parent_object.source), ==, 2);
	errno = 0;
	g_assert_cmpint (g_mime_stream_seek (cat, 1, GMIME_STREAM_SEEK_CUR), ==, -1);
	g_assert_cmpint (errno, ==, EINVAL);
	g_assert_cmpint (g_mime_stream_seek (cat, -2, GMIME_STREAM_SEEK_END), ==, 5);
	g_assert_cmpint (g_mime_stream_read (cat, buf, sizeof (buf)), ==, 2);
	g_assert_cmpmem (buf, 2, "cd", 2);
	g_object_unref (cat);
	g_object_unref (first);
	g_object_unref (tail);

	cat = g_mime_stream_cat_new ();
	head = g_mime_stream_mem_new_with_buffer ("XXabYY", 6);
	end = g_mime_stream_mem_new_with_buffer ("ZZcdWW", 6);
	g_mime_stream_set_bounds (head, 2, 4);
	g_mime_stream_set_bounds (end, 2, 4);
	g_mime_stream_cat_add_source (GMIME_STREAM_CAT (cat), head);
	g_mime_stream_cat_add_source (GMIME_STREAM_CAT (cat), end);
	g_assert_cmpint (g_mime_stream_seek (cat, 3, GMIME_STREAM_SEEK_SET), ==, 3);
	g_assert_cmpint (g_mime_stream_tell (head), ==, 4);
	g_assert_cmpint (g_mime_stream_tell (end), ==, 3);
	g_assert_cmpint (g_mime_stream_seek (cat, 1, GMIME_STREAM_SEEK_SET), ==, 1);
	g_assert_cmpint (g_mime_stream_read (cat, buf, sizeof (buf)), ==, 3);
	g_assert_cmpmem (buf, 3, "bcd", 3);
	g_object_unref (cat);
	g_object_unref (head);
	g_object_unref (end);
	testsuite_check_passed ();
}

static void
cat_source_assert_data (TestCatSource *source, const char *data, size_t len)
{
	GByteArray *bytes = g_mime_stream_mem_get_byte_array (GMIME_STREAM_MEM (source->parent_object.source));

	g_assert_cmpuint (bytes->len, ==, len);
	g_assert_cmpmem (bytes->data, bytes->len, data, len);
	g_assert_cmpuint (source->seek_calls, ==, 0);
	g_assert_cmpuint (source->reset_calls, ==, 0);
}

static void
test_cat_write_fill (void)
{
	const char input[] = "a\0bcdefgh";
	TestCatSource *sources[5];
	GMimeStream *cat;
	size_t chunk, request, offset, count;
	guint i;

	testsuite_check ("Chained writes fill existing extents and grow a nonseekable tail");
	for (chunk = 1; chunk <= 4; chunk++) {
		for (request = 1; request <= 9; request++) {
			cat = g_mime_stream_cat_new ();
			sources[0] = cat_source_new ("", 0);
			sources[1] = cat_source_new ("XX123", 5);
			sources[2] = cat_source_new ("", 0);
			sources[3] = cat_source_new ("YY456", 5);
			sources[4] = cat_source_new ("Q", 1);
			g_mime_stream_seek (sources[1]->parent_object.source, 2, GMIME_STREAM_SEEK_SET);
			g_mime_stream_seek (sources[3]->parent_object.source, 2, GMIME_STREAM_SEEK_SET);
			g_mime_stream_seek (sources[4]->parent_object.source, 1, GMIME_STREAM_SEEK_SET);
			for (i = 0; i < G_N_ELEMENTS (sources); i++) {
				sources[i]->chunk_size = chunk;
				sources[i]->nonseekable = TRUE;
				g_mime_stream_cat_add_source (GMIME_STREAM_CAT (cat), (GMimeStream *) sources[i]);
			}
			sources[4]->no_length = sources[4]->no_tell = TRUE;
			g_assert_cmpint (g_mime_stream_write (cat, input, 0), ==, 0);
			for (i = 0; i < G_N_ELEMENTS (sources); i++)
				g_assert_cmpuint (sources[i]->write_calls, ==, 0);
			for (offset = 0; offset < sizeof (input) - 1; offset += count) {
				count = MIN (request, sizeof (input) - 1 - offset);
				g_assert_cmpint (g_mime_stream_write (cat, input + offset, count), ==, count);
				g_assert_cmpint (g_mime_stream_tell (cat), ==, offset + count);
			}
			g_assert_cmpint (g_mime_stream_flush (cat), ==, 0);
			cat_source_assert_data (sources[0], "", 0);
			cat_source_assert_data (sources[1], "XXa\0b", 5);
			cat_source_assert_data (sources[2], "", 0);
			cat_source_assert_data (sources[3], "YYcde", 5);
			cat_source_assert_data (sources[4], "Qfgh", 4);
			g_assert_cmpuint (sources[4]->length_calls, ==, 0);
			g_assert_cmpuint (sources[4]->tell_calls, ==, 0);
			for (i = 0; i < G_N_ELEMENTS (sources); i++) {
				g_assert_cmpuint (sources[i]->flush_calls, ==, 1);
				g_object_unref (sources[i]);
			}
			g_object_unref (cat);
		}
	}
	testsuite_check_passed ();
}

static void
test_cat_write_errors (void)
{
	TestCatSource *first, *second, *tail;
	GMimeStream *cat;
	size_t accepted, fail_after;
	guint mode;
	int expected_errno;

	testsuite_check ("Chained write errors and zero progress retain the failing child");
	for (mode = 0; mode < 3; mode++) {
		expected_errno = mode == 2 ? EIO : mode == 1 ? EAGAIN : ENOSPC;
		for (fail_after = 0; fail_after < 2; fail_after++) {
			first = cat_source_new ("12", 2);
			second = cat_source_new ("34", 2);
			tail = cat_source_new ("", 0);
			second->write_after = fail_after;
			second->zero_write = mode == 2;
			second->write_errno = expected_errno;
			cat = g_mime_stream_cat_new ();
			g_mime_stream_cat_add_source (GMIME_STREAM_CAT (cat), (GMimeStream *) first);
			g_mime_stream_cat_add_source (GMIME_STREAM_CAT (cat), (GMimeStream *) second);
			g_mime_stream_cat_add_source (GMIME_STREAM_CAT (cat), (GMimeStream *) tail);
			accepted = 2 + fail_after;
			errno = 0;
			g_assert_cmpint (g_mime_stream_write (cat, "abcdef", 6), ==, accepted);
			g_assert_cmpint (errno, ==, expected_errno);
			g_assert_cmpint (g_mime_stream_tell (cat), ==, accepted);
			g_assert_cmpuint (tail->write_calls, ==, 0);
			g_assert_cmpuint (second->flush_calls, ==, 0);
			errno = 0;
			g_assert_cmpint (g_mime_stream_write (cat, "abcdef" + accepted, 6 - accepted), ==, -1);
			g_assert_cmpint (errno, ==, expected_errno);
			second->write_after = G_MAXSIZE;
			g_assert_cmpint (g_mime_stream_write (cat, "abcdef" + accepted, 6 - accepted), ==, 6 - accepted);
			g_assert_cmpint (g_mime_stream_tell (cat), ==, 6);
			cat_source_assert_data (first, "ab", 2);
			cat_source_assert_data (second, "cd", 2);
			cat_source_assert_data (tail, "ef", 2);
			g_assert_cmpuint (first->flush_calls, ==, 1);
			g_assert_cmpuint (second->flush_calls, ==, 1);
			g_object_unref (first);
			g_object_unref (second);
			g_object_unref (tail);
			g_object_unref (cat);
		}
	}

	tail = cat_source_new ("", 0);
	tail->no_length = tail->no_tell = TRUE;
	tail->write_after = 1;
	tail->write_errno = EAGAIN;
	cat = g_mime_stream_cat_new ();
	g_mime_stream_cat_add_source (GMIME_STREAM_CAT (cat), (GMimeStream *) tail);
	errno = 0;
	g_assert_cmpint (g_mime_stream_write (cat, "abc", 3), ==, 1);
	g_assert_cmpint (errno, ==, EAGAIN);
	errno = 0;
	g_assert_cmpint (g_mime_stream_write (cat, "bc", 2), ==, -1);
	g_assert_cmpint (errno, ==, EAGAIN);
	tail->write_after = G_MAXSIZE;
	g_assert_cmpint (g_mime_stream_write (cat, "bc", 2), ==, 2);
	cat_source_assert_data (tail, "abc", 3);
	g_assert_cmpuint (tail->length_calls, ==, 0);
	g_assert_cmpuint (tail->tell_calls, ==, 0);
	g_object_unref (cat);
	g_object_unref (tail);
	testsuite_check_passed ();
}

static void
test_cat_write_flush_retry (void)
{
	TestCatSource *first, *tail;
	GMimeStream *cat;
	guint start;

	testsuite_check ("Boundary flush failures are retryable without replaying bytes");
	for (start = 0; start <= 2; start += 2) {
		first = cat_source_new ("ab", 2);
		tail = cat_source_new ("", 0);
		g_mime_stream_seek (first->parent_object.source, start, GMIME_STREAM_SEEK_SET);
		first->fail_flush = TRUE;
		cat = g_mime_stream_cat_new ();
		g_mime_stream_cat_add_source (GMIME_STREAM_CAT (cat), (GMimeStream *) first);
		g_mime_stream_cat_add_source (GMIME_STREAM_CAT (cat), (GMimeStream *) tail);
		if (start == 0) {
			/* Filling a child exactly does not prematurely complete it. */
			g_assert_cmpint (g_mime_stream_write (cat, "ab", 2), ==, 2);
			g_assert_cmpuint (first->flush_calls, ==, 0);
		}
		errno = 0;
		g_assert_cmpint (g_mime_stream_write (cat, "cd", 2), ==, -1);
		g_assert_cmpint (errno, ==, EIO);
		g_assert_cmpuint (tail->write_calls, ==, 0);
		g_assert_cmpint (g_mime_stream_tell (cat), ==, 2 - start);
		/* Explicit flush uses the same current child and preserves its error. */
		errno = 0;
		g_assert_cmpint (g_mime_stream_flush (cat), ==, -1);
		g_assert_cmpint (errno, ==, EIO);
		first->fail_flush = FALSE;
		g_assert_cmpint (g_mime_stream_write (cat, "cd", 2), ==, 2);
		g_assert_cmpint (g_mime_stream_flush (cat), ==, 0);
		g_assert_cmpuint (first->flush_calls, ==, 3);
		g_assert_cmpuint (tail->flush_calls, ==, 1);
		cat_source_assert_data (first, "ab", 2);
		cat_source_assert_data (tail, "cd", 2);
		g_object_unref (first);
		g_object_unref (tail);
		g_object_unref (cat);
	}

	first = cat_source_new ("12", 2);
	tail = cat_source_new ("", 0);
	first->fail_flush = TRUE;
	cat = g_mime_stream_cat_new ();
	g_mime_stream_cat_add_source (GMIME_STREAM_CAT (cat), (GMimeStream *) first);
	g_mime_stream_cat_add_source (GMIME_STREAM_CAT (cat), (GMimeStream *) tail);
	errno = 0;
	g_assert_cmpint (g_mime_stream_write (cat, "abcd", 4), ==, 2);
	g_assert_cmpint (errno, ==, EIO);
	g_assert_cmpint (g_mime_stream_tell (cat), ==, 2);
	first->fail_flush = FALSE;
	g_assert_cmpint (g_mime_stream_write (cat, "cd", 2), ==, 2);
	cat_source_assert_data (first, "ab", 2);
	cat_source_assert_data (tail, "cd", 2);
	g_object_unref (first);
	g_object_unref (tail);
	g_object_unref (cat);
	testsuite_check_passed ();
}

static void
test_cat_write_bounds (void)
{
	TestCatSource *source = cat_source_new ("12", 2);
	GMimeStream *cat = g_mime_stream_cat_new ();
	GMimeStream *head, *tail = g_mime_stream_mem_new ();

	testsuite_check ("Chained writes enforce native bounds, metadata and overflow");
	g_assert_cmpint (g_mime_stream_write (cat, "", 0), ==, 0);
	errno = 0;
	g_assert_cmpint (g_mime_stream_write (cat, "a", 1), ==, -1);
	g_assert_cmpint (errno, ==, EBADF);
	g_assert_cmpint (g_mime_stream_flush (cat), ==, 0);
	g_mime_stream_cat_add_source (GMIME_STREAM_CAT (cat), (GMimeStream *) source);
	g_mime_stream_cat_add_source (GMIME_STREAM_CAT (cat), tail);
	source->no_length = TRUE;
	errno = 0;
	g_assert_cmpint (g_mime_stream_write (cat, "abc", 3), ==, -1);
	g_assert_cmpint (errno, ==, ESPIPE);
	source->no_length = FALSE;
	source->no_tell = TRUE;
	errno = 0;
	g_assert_cmpint (g_mime_stream_write (cat, "abc", 3), ==, -1);
	g_assert_cmpint (errno, ==, ESPIPE);
	g_assert_cmpuint (source->write_calls, ==, 0);
	source->no_tell = FALSE;
	g_mime_stream_set_bounds ((GMimeStream *) source, 1, -1);
	errno = 0;
	g_assert_cmpint (g_mime_stream_write (cat, "abc", 3), ==, -1);
	g_assert_cmpint (errno, ==, EINVAL);
	g_assert_cmpuint (source->flush_calls, ==, 0);
	g_object_unref (cat);
	g_object_unref (source);
	g_object_unref (tail);

	cat = g_mime_stream_cat_new ();
	head = g_mime_stream_mem_new_with_buffer ("XX12ZZ", 6);
	tail = g_mime_stream_mem_new ();
	g_mime_stream_set_bounds (head, 2, 4);
	g_mime_stream_cat_add_source (GMIME_STREAM_CAT (cat), head);
	g_mime_stream_cat_add_source (GMIME_STREAM_CAT (cat), tail);
	g_assert_cmpint (g_mime_stream_write (cat, "abcd", 4), ==, 4);
	g_assert_cmpmem (g_mime_stream_mem_get_byte_array (GMIME_STREAM_MEM (head))->data, 6, "XXabZZ", 6);
	g_assert_cmpmem (g_mime_stream_mem_get_byte_array (GMIME_STREAM_MEM (tail))->data, 2, "cd", 2);
	g_object_unref (cat);
	g_object_unref (head);
	g_object_unref (tail);

	cat = g_mime_stream_cat_new ();
	tail = g_mime_stream_mem_new ();
	g_mime_stream_cat_add_source (GMIME_STREAM_CAT (cat), tail);
	g_mime_stream_set_bounds (cat, 0, 2);
	g_assert_cmpint (g_mime_stream_write (cat, "abcd", 4), ==, 2);
	errno = 0;
	g_assert_cmpint (g_mime_stream_write (cat, "cd", 2), ==, -1);
	g_assert_cmpint (errno, ==, EINVAL);
	g_assert_cmpint (g_mime_stream_tell (tail), ==, 2);
	g_object_unref (cat);
	g_object_unref (tail);

	cat = g_mime_stream_cat_new ();
	tail = g_mime_stream_mem_new_with_buffer ("1234", 4);
	g_mime_stream_set_bounds (tail, 1, 3);
	g_mime_stream_cat_add_source (GMIME_STREAM_CAT (cat), tail);
	errno = 0;
	g_assert_cmpint (g_mime_stream_write (cat, "abcd", 4), ==, 2);
	g_assert_cmpint (errno, ==, EIO);
	g_assert_cmpmem (g_mime_stream_mem_get_byte_array (GMIME_STREAM_MEM (tail))->data, 4, "1ab4", 4);
	g_object_unref (cat);
	g_object_unref (tail);

	cat = g_mime_stream_cat_new ();
	source = cat_source_new ("", 0);
	g_mime_stream_construct (cat, G_MAXINT64 - 1, -1);
	g_mime_stream_cat_add_source (GMIME_STREAM_CAT (cat), (GMimeStream *) source);
	errno = 0;
	g_assert_cmpint (g_mime_stream_write (cat, "ab", 2), ==, 1);
	g_assert_cmpint (errno, ==, EOVERFLOW);
	g_assert_cmpint (g_mime_stream_tell (cat), ==, G_MAXINT64);
	errno = 0;
	g_assert_cmpint (g_mime_stream_write (cat, "b", 1), ==, -1);
	g_assert_cmpint (errno, ==, EOVERFLOW);
	cat_source_assert_data (source, "a", 1);
	g_object_unref (cat);
	g_object_unref (source);
	testsuite_check_passed ();
}

static void
test_cat_write_shared_source (void)
{
	TestCatSource *first = cat_source_new ("ab", 2);
	TestCatSource *tail = cat_source_new ("cd", 2);
	GMimeStream *cat = g_mime_stream_cat_new ();
	GMimeStream *appended;
	char buf[8];

	testsuite_check ("Chained writes share current positions with reads and duplicate children");
	first->nonseekable = tail->nonseekable = TRUE;
	g_mime_stream_cat_add_source (GMIME_STREAM_CAT (cat), (GMimeStream *) first);
	g_mime_stream_cat_add_source (GMIME_STREAM_CAT (cat), (GMimeStream *) tail);
	g_assert_cmpint (g_mime_stream_read (cat, buf, 2), ==, 2);
	g_assert_cmpint (g_mime_stream_write (cat, "XY", 2), ==, 2);
	g_assert_cmpint (g_mime_stream_read (cat, buf, sizeof (buf)), ==, 0);
	first->fail_flush = tail->fail_flush = TRUE;
	g_assert_cmpint (g_mime_stream_flush (cat), ==, 0);
	g_assert_cmpuint (first->flush_calls, ==, 1);
	g_assert_cmpuint (tail->flush_calls, ==, 0);
	first->fail_flush = tail->fail_flush = FALSE;
	g_assert_cmpint (g_mime_stream_write (cat, "Z", 1), ==, 1);
	g_assert_cmpint (g_mime_stream_tell (cat), ==, 5);
	g_assert_true (g_mime_stream_eos (cat));
	g_assert_cmpint (g_mime_stream_read (cat, buf, sizeof (buf)), ==, 0);
	g_assert_cmpint (g_mime_stream_flush (cat), ==, 0);
	g_assert_cmpuint (tail->flush_calls, ==, 1);
	/* Appending clears cached read EOF without losing the selected write tail. */
	appended = g_mime_stream_mem_new_with_buffer ("!", 1);
	g_mime_stream_cat_add_source (GMIME_STREAM_CAT (cat), appended);
	g_object_unref (appended);
	g_assert_false (g_mime_stream_eos (cat));
	g_assert_cmpint (g_mime_stream_read (cat, buf, sizeof (buf)), ==, 1);
	g_assert_cmpint (buf[0], ==, '!');
	cat_source_assert_data (first, "ab", 2);
	cat_source_assert_data (tail, "XYZ", 3);
	g_object_unref (cat);
	g_object_unref (first);
	g_object_unref (tail);

	cat = g_mime_stream_cat_new ();
	first = cat_source_new ("12", 2);
	g_mime_stream_cat_add_source (GMIME_STREAM_CAT (cat), (GMimeStream *) first);
	g_mime_stream_cat_add_source (GMIME_STREAM_CAT (cat), (GMimeStream *) first);
	g_assert_cmpint (g_mime_stream_write (cat, "abcd", 4), ==, 4);
	g_assert_cmpint (g_mime_stream_tell (cat), ==, 4);
	g_assert_cmpint (g_mime_stream_flush (cat), ==, 0);
	g_assert_cmpuint (first->flush_calls, ==, 2);
	cat_source_assert_data (first, "abcd", 4);
	g_object_unref (cat);
	g_object_unref (first);
	testsuite_check_passed ();
}

static void
test_cat_write_reposition (void)
{
	TestCatSource *first = cat_source_new ("123", 3);
	TestCatSource *tail = cat_source_new ("456", 3);
	GMimeStream *cat = g_mime_stream_cat_new ();
	guint calls;
	char buf[8];

	testsuite_check ("Chained write positions and cached EOF survive seek and reset");
	g_mime_stream_cat_add_source (GMIME_STREAM_CAT (cat), (GMimeStream *) first);
	g_mime_stream_cat_add_source (GMIME_STREAM_CAT (cat), (GMimeStream *) tail);
	g_assert_cmpint (g_mime_stream_write (cat, "abcdef", 6), ==, 6);
	g_assert_cmpint (g_mime_stream_seek (cat, 1, GMIME_STREAM_SEEK_SET), ==, 1);
	g_assert_cmpint (g_mime_stream_write (cat, "XYZ", 3), ==, 3);
	g_assert_cmpint (g_mime_stream_tell (cat), ==, 4);
	g_assert_cmpint (g_mime_stream_reset (cat), ==, 0);
	g_assert_cmpint (g_mime_stream_write (cat, "ghiJKL", 6), ==, 6);
	g_assert_cmpint (g_mime_stream_read (cat, buf, sizeof (buf)), ==, 0);
	g_assert_cmpint (g_mime_stream_write (cat, "M", 1), ==, 1);
	calls = tail->read_calls;
	g_assert_cmpint (g_mime_stream_read (cat, buf, sizeof (buf)), ==, 0);
	g_assert_cmpuint (tail->read_calls, ==, calls);
	g_assert_cmpint (g_mime_stream_seek (cat, 1, GMIME_STREAM_SEEK_SET), ==, 1);
	g_assert_false (g_mime_stream_eos (cat));
	g_assert_cmpint (g_mime_stream_read (cat, buf, 4), ==, 4);
	g_assert_cmpmem (buf, 4, "hiJK", 4);
	g_assert_cmpint (g_mime_stream_reset (cat), ==, 0);
	g_assert_cmpint (g_mime_stream_read (cat, buf, sizeof (buf)), ==, 7);
	g_assert_cmpmem (buf, 7, "ghiJKLM", 7);
	g_object_unref (cat);
	g_object_unref (first);
	g_object_unref (tail);
	testsuite_check_passed ();
}

static void
test_cat_write_filtered_pipe (void)
{
	TestCatSource *first = cat_source_new ("12", 2);
	GMimeStream *cat = g_mime_stream_cat_new ();
	GMimeStream *output, *tail;
	GMimeFilter *filter;
	int fds[2];
	char buf[4];

	testsuite_check ("Chained writes support pipe and filtered tails without repositioning");
	g_assert_cmpint (pipe (fds), ==, 0);
	tail = g_mime_stream_pipe_new (fds[1]);
	g_mime_stream_cat_add_source (GMIME_STREAM_CAT (cat), (GMimeStream *) first);
	g_mime_stream_cat_add_source (GMIME_STREAM_CAT (cat), tail);
	g_object_unref (tail);
	g_assert_cmpint (g_mime_stream_write (cat, "abc", 3), ==, 3);
	g_assert_cmpint (g_mime_stream_flush (cat), ==, 0);
	g_object_unref (cat);
	g_assert_cmpint (read (fds[0], buf, sizeof (buf)), ==, 1);
	g_assert_cmpint (buf[0], ==, 'c');
	g_assert_cmpint (read (fds[0], buf, sizeof (buf)), ==, 0);
	close (fds[0]);
	cat_source_assert_data (first, "ab", 2);
	g_object_unref (first);

	first = cat_source_new ("12", 2);
	cat = g_mime_stream_cat_new ();
	output = g_mime_stream_mem_new ();
	tail = g_mime_stream_filter_new (output);
	filter = g_mime_filter_unix2dos_new (TRUE);
	g_mime_stream_filter_add (GMIME_STREAM_FILTER (tail), filter);
	g_object_unref (filter);
	g_mime_stream_cat_add_source (GMIME_STREAM_CAT (cat), (GMimeStream *) first);
	g_mime_stream_cat_add_source (GMIME_STREAM_CAT (cat), tail);
	g_object_unref (tail);
	g_assert_cmpint (g_mime_stream_write (cat, "abc", 3), ==, 3);
	g_assert_cmpint (g_mime_stream_flush (cat), ==, 0);
	g_assert_cmpint (g_mime_stream_flush (cat), ==, 0);
	g_assert_cmpuint (first->flush_calls, ==, 1);
	g_assert_cmpmem (g_mime_stream_mem_get_byte_array (GMIME_STREAM_MEM (output))->data, 3, "c\r\n", 3);
	g_object_unref (cat);
	g_object_unref (output);
	g_object_unref (first);
	testsuite_check_passed ();
}

static unsigned char
randc (void)
{
	unsigned char c;
	ssize_t n;
	
	do {
		if ((n = read (randfd, &c, 1)) > 0)
		    break;
	} while (n == -1 && errno == EINTR);
	
	return c;
}

static float
randf (void)
{
	unsigned int value = 0;
	size_t nread = 0;
	ssize_t n;
	
	do {
		if ((n = read (randfd, ((char *) &value) + nread, sizeof (value) - nread)) > 0) {
			if ((nread += n) == sizeof (value))
				break;
		}
	} while (n == -1 && errno == EINTR);
	
	return (value * 1.0) / UINT_MAX;
}


static GMimeStream *
random_whole_stream (const char *datadir, char **filename)
{
	size_t nwritten, buflen, total = 0, size;
	GMimeStream *stream;
	char buf[4096];
	ssize_t n;
	int fd;
	
	/* read between 4k and 14k bytes */
	size = 4096 + (size_t) (10240.0 * randf ());
	v(fprintf (stdout, "Generating %zu bytes of random data... ", size));
	v(fflush (stdout));
	
	g_mkdir_with_parents (datadir, 0755);
	
	g_snprintf (buf, sizeof (buf), "%s%cstream.%u", datadir, G_DIR_SEPARATOR, getpid ());
	if ((fd = open (buf, O_CREAT | O_TRUNC | O_RDWR, 0666)) == -1) {
		fprintf (stderr, "Error: Cannot create `%s': %s\n", buf, g_strerror (errno));
		exit (EXIT_FAILURE);
	}
	
	*filename = g_strdup (buf);
	
	stream = g_mime_stream_fs_new (fd);
	
	while (total < size) {
		buflen = size - total > sizeof (buf) ? sizeof (buf) : size - total;
		
		nwritten = 0;
		do {
			if ((n = read (randfd, buf + nwritten, buflen - nwritten)) > 0)
				nwritten += n;
		} while (nwritten < buflen);
		
		nwritten = 0;
		do {
			if ((n = g_mime_stream_write (stream, buf + nwritten, buflen - nwritten)) <= 0)
				break;
			nwritten += n;
			total += n;
		} while (nwritten < buflen);
		
		if (nwritten < buflen)
			break;
	}
	
	g_mime_stream_flush (stream);
	g_mime_stream_reset (stream);
	
	v(fputs ("done\n", stdout));
	
	return stream;
}

struct _StreamPart {
	struct _StreamPart *next;
	gint64 pstart, pend;  /* start/end offsets of the part stream */
	gint64 wstart, wend;  /* corresponding start/end offsets of the whole stream */
	char *filename;
};


static int
check_streams_match (GMimeStream *orig, GMimeStream *dup, const char *filename, int check_overflow)
{
	char buf[4096], dbuf[4096], errstr[1024];
	gint64 len, totalsize, totalread = 0;
	size_t nread, size;
	ssize_t n;
	
	v(fprintf (stdout, "Matching original stream (%" G_GINT64_FORMAT " -> %" G_GINT64_FORMAT ") with %s (%" G_GINT64_FORMAT ", %" G_GINT64_FORMAT ")... ",
		   orig->position, orig->bound_end, filename, dup->position, dup->bound_end));
	
	if (orig->bound_end != -1) {
		totalsize = orig->bound_end - orig->position;
	} else if ((len = g_mime_stream_length (orig)) == -1) {
		g_strlcpy (errstr, "Error: Unable to get length of original stream\n", sizeof (errstr));
		goto fail;
	} else if (len < (orig->position - orig->bound_start)) {
		g_strlcpy (errstr, "Error: Overflow on original stream?\n", sizeof (errstr));
		goto fail;
	} else {
		totalsize = len - (orig->position - orig->bound_start);
	}
	
	while (totalread < totalsize) {
		if ((n = g_mime_stream_read (orig, buf, sizeof (buf))) <= 0)
			break;
		
		size = n;
		nread = 0;
		totalread += n;
		
		d(fprintf (stderr, "read %zu bytes from original stream\n", size));
		
		do {
			if ((n = g_mime_stream_read (dup, dbuf + nread, size - nread)) <= 0) {
				d(fprintf (stderr, "dup read() returned %zd, EOF\n", n));
				break;
			}
			d(fprintf (stderr, "read %zd bytes from dup stream\n", n));
			nread += n;
		} while (nread < size);
		
		if (nread < size) {
			snprintf (errstr, sizeof (errstr) - 1, "Error: `%s' appears to be truncated, short %zu+ bytes\n", filename, size - nread);
			goto fail;
		}
		
		if (memcmp (buf, dbuf, size) != 0) {
			snprintf (errstr, sizeof (errstr) - 1, "Error: `%s': content does not match\n", filename);
			goto fail;
		} else {
			d(fprintf (stderr, "%zu bytes identical\n", size));
		}
	}
	
	if (totalread < totalsize) {
		g_strlcpy (errstr, "Error: expected more data from original stream\n", sizeof (errstr));
		goto fail;
	}
	
	if (check_overflow && (n = g_mime_stream_read (dup, buf, sizeof (buf))) > 0) {
		snprintf (errstr, sizeof (errstr) - 1, "Error: `%s' appears to contain extra content\n", filename);
		goto fail;
	}
	
	v(fputs ("passed\n", stdout));
	
	return 0;
	
 fail:
	
	v(fputs ("failed\n", stdout));
	v(fputs (errstr, stderr));
	
	return -1;
}

static void
test_cat_write (GMimeStream *whole, struct _StreamPart *parts, int bounded)
{
	struct _StreamPart *part = parts;
	GMimeStream *stream, *sub, *cat;
	Exception *ex;
	int fd;
	
	cat = g_mime_stream_cat_new ();
	
	while (part != NULL) {
		d(fprintf (stderr, "adding %s start=%lld, end=%lld...\n",
			   part->filename, part->pstart, part->pend));
		
		if ((fd = open (part->filename, O_CREAT | O_TRUNC | O_WRONLY, 0666)) == -1) {
			ex = exception_new ("could not create `%s': %s", part->filename, g_strerror (errno));
			throw (ex);
		}
		
		stream = g_mime_stream_fs_new_with_bounds (fd, part->pstart, part->pend);
		g_mime_stream_cat_add_source ((GMimeStreamCat *) cat, stream);
		g_object_unref (stream);
		
		part = part->next;
	}
	
	g_mime_stream_reset (whole);
	if (g_mime_stream_write_to_stream (whole, (GMimeStream *) cat) == -1) {
		ex = exception_new ("%s", g_strerror (errno));
		g_object_unref (cat);
		throw (ex);
	}
	
	g_object_unref (cat);
	
	/* now lets check that the content matches */
	d(fprintf (stderr, "checking all part streams have correct data...\n"));
	part = parts;
	while (part != NULL) {
		d(fprintf (stderr, "checking substream %s\n", part->filename));
		if ((fd = open (part->filename, O_RDONLY, 0)) == -1) {
			ex = exception_new ("could not open `%s': %s", part->filename, g_strerror (errno));
			throw (ex);
		}
		
		if (!(sub = g_mime_stream_substream (whole, part->wstart, part->wend))) {
			ex = exception_new ("could not substream original stream");
			close (fd);
			throw (ex);
		}
		
		if (!(stream = g_mime_stream_fs_new_with_bounds (fd, part->pstart, -1))) {
			ex = exception_new ("could not instantiate stream for `%s'", part->filename);
			close (fd);
			throw (ex);
		}
		
		d(fprintf (stderr, "checking substream %s matches...\n", part->filename));
		if (check_streams_match (sub, stream, part->filename, TRUE) == -1) {
			ex = exception_new ("streams did not match");
			g_object_unref (stream);
			g_object_unref (sub);
			throw (ex);
		}
		
		g_object_unref (stream);
		g_object_unref (sub);
		
		part = part->next;
	}
}

static void
test_cat_read (GMimeStream *whole, struct _StreamPart *parts, int bounded)
{
	struct _StreamPart *part = parts;
	GMimeStream *stream, *cat;
	Exception *ex;
	int fd;
	
	cat = g_mime_stream_cat_new ();
	
	while (part != NULL) {
		d(fprintf (stderr, "adding %s start=%lld, end=%lld...\n",
			   part->filename, part->pstart, part->pend));
		
		if ((fd = open (part->filename, O_RDONLY, 0)) == -1) {
			ex = exception_new ("could not open `%s': %s", part->filename, g_strerror (errno));
			g_object_unref (cat);
			throw (ex);
		}
		
		stream = g_mime_stream_fs_new_with_bounds (fd, part->pstart, bounded ? part->pend : -1);
		g_mime_stream_cat_add_source ((GMimeStreamCat *) cat, stream);
		g_object_unref (stream);
		
		part = part->next;
	}
	
	g_mime_stream_reset (whole);
	if (check_streams_match (whole, cat, "stream.part*", TRUE) == -1) {
		ex = exception_new ("streams do not match");
		g_object_unref (cat);
		throw (ex);
	}
	g_object_unref (cat);
}

static void
test_cat_seek (GMimeStream *whole, struct _StreamPart *parts, int bounded)
{
	struct _StreamPart *part = parts;
	GMimeStream *stream, *cat;
	gint64 offset, len;
	Exception *ex;
	int fd;
	
	if (whole->bound_end != -1) {
		len = whole->bound_end - whole->bound_start;
	} else if ((len = g_mime_stream_length (whole)) == -1) {
		ex = exception_new ("unable to get original stream length");
		throw (ex);
	}
	
	cat = g_mime_stream_cat_new ();
	
	while (part != NULL) {
		d(fprintf (stderr, "adding %s start=%lld, end=%lld...\n",
			   part->filename, part->pstart, part->pend));
		
		if ((fd = open (part->filename, O_RDONLY, 0)) == -1) {
			ex = exception_new ("could not open `%s': %s", part->filename, g_strerror (errno));
			g_object_unref (cat);
			throw (ex);
		}
		
		stream = g_mime_stream_fs_new_with_bounds (fd, part->pstart, bounded ? part->pend : -1);
		g_mime_stream_cat_add_source ((GMimeStreamCat *) cat, stream);
		g_object_unref (stream);
		
		part = part->next;
	}
	
	/* calculate a random seek offset to compare at */
	offset = (gint64) (len * randf ());
	
	if (g_mime_stream_seek (whole, offset, GMIME_STREAM_SEEK_SET) == -1) {
		ex = exception_new ("could not seek to %lld in original stream: %s",
				    (long long) offset, g_strerror (errno));
		g_object_unref (cat);
		throw (ex);
	}
	
	if (g_mime_stream_seek (cat, offset, GMIME_STREAM_SEEK_SET) == -1) {
		ex = exception_new ("could not seek to %lld: %s",
				    (long long) offset, g_strerror (errno));
		g_object_unref (cat);
		throw (ex);
	}
	
	if (check_streams_match (whole, cat, "stream.part*", TRUE) == -1) {
		ex = exception_new ("streams did not match");
		g_object_unref (cat);
		throw (ex);
	}
	g_object_unref (cat);
}

static void
test_cat_substream (GMimeStream *whole, struct _StreamPart *parts, int bounded)
{
	GMimeStream *stream, *cat, *sub1, *sub2;
	struct _StreamPart *part = parts;
	gint64 start, end, len;
	Exception *ex;
	int fd;
	
	if (whole->bound_end != -1) {
		len = whole->bound_end - whole->bound_start;
	} else if ((len = g_mime_stream_length (whole)) == -1) {
		ex = exception_new ("unable to get original stream length");
		throw (ex);
	}
	
	cat = g_mime_stream_cat_new ();
	
	while (part != NULL) {
		d(fprintf (stderr, "adding %s start=%lld, end=%lld...\n",
			   part->filename, part->pstart, part->pend));
		
		if ((fd = open (part->filename, O_RDONLY, 0)) == -1) {
			ex = exception_new ("could not open `%s': %s", part->filename, g_strerror (errno));
			g_object_unref (cat);
			throw (ex);
		}
		
		stream = g_mime_stream_fs_new_with_bounds (fd, part->pstart, bounded ? part->pend : -1);
		g_mime_stream_cat_add_source ((GMimeStreamCat *) cat, stream);
		g_object_unref (stream);
		
		part = part->next;
	}
	
	/* calculate a random start/end offsets */
	start = (gint64) (len * randf ());
	if (randc () % 2)
		end = start + (gint64) ((len - start) * randf ());
	else
		end = -1;
	
	if (!(sub1 = g_mime_stream_substream (whole, start, end))) {
		ex = exception_new ("could not substream the original stream: %s",
				    g_strerror (errno));
		g_object_unref (cat);
		throw (ex);
	}
	
	if (!(sub2 = g_mime_stream_substream (cat, start, end))) {
		ex = exception_new ("%s", g_strerror (errno));
		g_object_unref (sub1);
		g_object_unref (cat);
		throw (ex);
	}
	
	g_object_unref (cat);
	
	if (check_streams_match (sub1, sub2, "stream.part*", TRUE) == -1) {
		ex = exception_new ("streams did not match");
		g_object_unref (sub1);
		g_object_unref (sub2);
		throw (ex);
	}
	
	g_object_unref (sub1);
	g_object_unref (sub2);
}


typedef void (* checkFunc) (GMimeStream *stream, struct _StreamPart *parts, int bounded);

struct {
	const char *what;
	checkFunc check;
	int bounded;
} checks[] = {
	{ "GMimeStreamCat::write()",            test_cat_write,     FALSE },
	{ "GMimeStreamCat::read(bound)",        test_cat_read,      TRUE  },
	{ "GMimeStreamCat::read(unbound)",      test_cat_read,      FALSE },
	{ "GMimeStreamCat::seek(bound)",        test_cat_seek,      TRUE  },
	{ "GMimeStreamCat::seek(unbound)",      test_cat_seek,      FALSE },
	{ "GMimeStreamCat::substream(bound)",   test_cat_substream, TRUE  },
	{ "GMimeStreamCat::substream(unbound)", test_cat_substream, FALSE },
};

int main (int argc, char **argv)
{
	const char *datadir = "data/cat";
	struct _StreamPart *list, *tail, *n;
	gboolean failed = FALSE;
	gint64 wholelen, left;
	GMimeStream *whole;
	guint32 part = 0;
	gint64 start = 0;
	char *filename;
	struct stat st;
	gint64 len;
	int fd, i;
	
	g_mime_init ();
	
	testsuite_init (argc, argv);

	testsuite_start ("ChainedStream read port");
	test_cat_read_fill ();
	test_cat_read_errors ();
	test_cat_read_zero ();
	test_cat_read_reposition ();
	test_cat_read_shared_source ();
	test_cat_read_filtered_pipe ();
	test_cat_read_empty_bounded_filter ();
	test_cat_read_overflow ();
	testsuite_end ();

	testsuite_start ("ChainedStream write port");
	test_cat_write_fill ();
	test_cat_write_errors ();
	test_cat_write_flush_retry ();
	test_cat_write_bounds ();
	test_cat_write_shared_source ();
	test_cat_write_reposition ();
	test_cat_write_filtered_pipe ();
	testsuite_end ();

	testsuite_start ("ChainedStream seek port");
	test_cat_seek_boundaries ();
	test_cat_seek_unvisited ();
	test_cat_seek_errors ();
	test_cat_seek_limits ();
	testsuite_end ();
	
	if ((randfd = open ("/dev/urandom", O_RDONLY)) == -1)
		return EXIT_FAILURE;
	
	for (i = 1; i < argc; i++) {
		if (argv[i][0] != '-') {
			datadir = argv[i];
			break;
		}
	}
	
	if (i < argc) {
		if (stat (datadir, &st) == -1) {
			if (errno == ENOENT) {
				g_mkdir_with_parents (datadir, 0755);
				if (stat (datadir, &st) == -1) {
					close (randfd);
					
					return EXIT_FAILURE;
				}
			} else {
				close (randfd);
				
				return EXIT_FAILURE;
			}
		}
		
		if (S_ISREG (st.st_mode)) {
			/* test a particular input file */
			if ((fd = open (argv[i], O_RDONLY, 0)) == -1) {
				close (randfd);
				
				return EXIT_FAILURE;
			}
			
			filename = g_strdup (argv[i]);
			whole = g_mime_stream_fs_new (fd);
		} else if (S_ISDIR (st.st_mode)) {
			/* use path as test suite data dir */
			whole = random_whole_stream (argv[i], &filename);
		} else {
			close (randfd);
			
			return EXIT_FAILURE;
		}
	} else {
		whole = random_whole_stream (datadir, &filename);
	}
	
	if ((wholelen = g_mime_stream_length (whole)) == -1) {
		fprintf (stderr, "Error: length of test stream unknown\n");
		g_object_unref (whole);
		close (randfd);
		
		return EXIT_FAILURE;
	} else if (wholelen == 64) {
		fprintf (stderr, "Error: length of test stream is unsuitable for testing\n");
		g_object_unref (whole);
		close (randfd);
		
		return EXIT_FAILURE;
	}
	
	list = NULL;
	tail = (struct _StreamPart *) &list;
	
	left = wholelen;
	while (left > 0) {
		len = 1 + (gint64) (left * randf ());
		n = g_new (struct _StreamPart, 1);
		n->filename = g_strdup_printf ("%s.%u", filename, part++);
		n->pstart = (gint64) 0; /* FIXME: we could make this a random offset */
		n->pend = n->pstart + len;
		n->wend = start + len;
		n->wstart = start;
		tail->next = n;
		tail = n;
		
		start += len;
		left -= len;
	}
	
	tail->next = NULL;
	
	testsuite_start ("GMimeStreamCat");
	
	for (i = 0; i < G_N_ELEMENTS (checks) && !failed; i++) {
		testsuite_check ("%s", checks[i].what);
		try {
			checks[i].check (whole, list, checks[i].bounded);
			testsuite_check_passed ();
		} catch (ex) {
			testsuite_check_failed ("%s failed: %s", checks[i].what,
						ex->message);
			failed = TRUE;
		} finally;
	}
	
	testsuite_end ();
	
	while (list != NULL) {
		n = list->next;
		if (!failed)
			unlink (list->filename);
		g_free (list->filename);
		g_free (list);
		list = n;
	}
	
	g_object_unref (whole);
	
	if (!failed)
		unlink (filename);
	
	g_free (filename);
	close (randfd);
	
	g_mime_shutdown ();
	
	return testsuite_exit ();
}
