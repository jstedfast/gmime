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

#include <errno.h>
#include <gmime/gmime.h>
#include "gmime/gmime-object-private.h"
#include "testsuite.h"

typedef struct {
	GInputStream parent;
	int *calls;
	int error;
} CloseInput;

typedef GInputStreamClass CloseInputClass;

typedef struct {
	GOutputStream parent;
	int *calls;
	int error;
} CloseOutput;

typedef GOutputStreamClass CloseOutputClass;

G_DEFINE_TYPE (CloseInput, close_input, G_TYPE_INPUT_STREAM)
G_DEFINE_TYPE (CloseOutput, close_output, G_TYPE_OUTPUT_STREAM)

static gboolean
input_close (GInputStream *stream, GCancellable *cancellable, GError **error)
{
	CloseInput *input = (CloseInput *) stream;

	(*input->calls)++;
	if (input->error < 0)
		return TRUE;
	g_set_error_literal (error, G_IO_ERROR, input->error, "input close failure");
	return FALSE;
}

static gboolean
output_close (GOutputStream *stream, GCancellable *cancellable, GError **error)
{
	CloseOutput *output = (CloseOutput *) stream;

	(*output->calls)++;
	if (output->error < 0)
		return TRUE;
	g_set_error_literal (error, G_IO_ERROR, output->error, "output close failure");
	return FALSE;
}

static void
input_finalize (GObject *object)
{
	G_OBJECT_CLASS (close_input_parent_class)->finalize (object);
	errno = EINTR;
}

static void
output_finalize (GObject *object)
{
	G_OBJECT_CLASS (close_output_parent_class)->finalize (object);
	errno = EINTR;
}

static void
close_input_class_init (CloseInputClass *klass)
{
	klass->close_fn = input_close;
	G_OBJECT_CLASS (klass)->finalize = input_finalize;
}

static void
close_output_class_init (CloseOutputClass *klass)
{
	klass->close_fn = output_close;
	G_OBJECT_CLASS (klass)->finalize = output_finalize;
}

static void
close_input_init (CloseInput *input)
{
	input->error = -1;
}

static void
close_output_init (CloseOutput *output)
{
	output->error = -1;
}

static void
test_gio_close (gboolean finalize)
{
	static const struct {
		int input_error, output_error, expected;
	} cases[] = {
		{ -1, -1, 0 },
		{ G_IO_ERROR_PERMISSION_DENIED, -1, EACCES },
		{ -1, G_IO_ERROR_NO_SPACE, ENOSPC },
		{ G_IO_ERROR_PERMISSION_DENIED, G_IO_ERROR_NO_SPACE, EACCES },
		{ G_IO_ERROR_FAILED, G_IO_ERROR_FAILED, EIO }
	};
	guint i, owner;

	for (owner = 0; owner < 2; owner++) {
		for (i = 0; i < G_N_ELEMENTS (cases); i++) {
			GFile *file = g_file_new_for_path (".");
			GFile *weak_file = file;
			GMimeStream *stream = g_mime_stream_gio_new (file);
			GMimeStreamGIOPrivate *priv = _g_mime_stream_gio_get_private (GMIME_STREAM_GIO (stream));
			CloseInput *input = g_object_new (close_input_get_type (), NULL);
			CloseOutput *output = g_object_new (close_output_get_type (), NULL);
			int input_calls = 0, output_calls = 0;

			g_object_add_weak_pointer (G_OBJECT (file), (gpointer *) &weak_file);
			input->calls = &input_calls;
			input->error = cases[i].input_error;
			output->calls = &output_calls;
			output->error = cases[i].output_error;
			priv->istream = G_INPUT_STREAM (input);
			priv->ostream = G_OUTPUT_STREAM (output);
			g_mime_stream_gio_set_owner (GMIME_STREAM_GIO (stream), owner);
			g_assert_cmpint (g_mime_stream_gio_get_owner (GMIME_STREAM_GIO (stream)), ==, owner);

			if (!finalize) {
				errno = 0;
				g_assert_cmpint (g_mime_stream_close (stream), ==, cases[i].expected ? -1 : 0);
				if (cases[i].expected)
					g_assert_cmpint (errno, ==, cases[i].expected);
				g_assert_cmpint (input_calls, ==, 1);
				g_assert_cmpint (output_calls, ==, 1);
				g_assert_null (priv->istream);
				g_assert_null (priv->ostream);
				g_assert_null (priv->file);
				g_assert_cmpint (g_mime_stream_close (stream), ==, 0);
			}
			g_object_unref (stream);
			g_assert_cmpint (weak_file == NULL, ==, owner);
			g_assert_cmpint (input_calls, ==, 1);
			g_assert_cmpint (output_calls, ==, 1);
			if (!owner)
				g_object_unref (file);
			g_assert_null (weak_file);
		}
	}
}

static void
test_gio_lifetime (void)
{
	GFile *file = g_file_new_for_path (".");
	GFile *weak_file = file;
	GMimeStream *parent, *sub;

	g_object_add_weak_pointer (G_OBJECT (file), (gpointer *) &weak_file);
	parent = g_mime_stream_gio_new_with_bounds (file, 0, 10);
	sub = g_mime_stream_substream (parent, 1, 3);
	g_assert_nonnull (sub);
	g_assert_false (g_mime_stream_gio_get_owner (GMIME_STREAM_GIO (sub)));
	g_object_unref (parent);
	g_assert_nonnull (weak_file);
	g_assert_cmpint (g_mime_stream_close (sub), ==, 0);
	g_assert_nonnull (weak_file);
	g_object_unref (sub);
	g_assert_null (weak_file);
}

static void
test_wrappers (void)
{
	GMimeStream *source, *wrapper;
	GByteArray *bytes;
	guint owner, finalize, flush;

	for (finalize = 0; finalize < 2; finalize++) {
		for (flush = 0; flush < 2; flush++) {
			source = g_mime_stream_mem_new ();
			bytes = g_mime_stream_mem_get_byte_array (GMIME_STREAM_MEM (source));
			wrapper = g_mime_stream_buffer_new (source, GMIME_STREAM_BUFFER_BLOCK_WRITE);
			g_assert_cmpint (g_mime_stream_write (wrapper, "abc", 3), ==, 3);
			g_assert_cmpuint (bytes->len, ==, 0);
			if (flush)
				g_assert_cmpint (g_mime_stream_flush (wrapper), ==, 0);
			if (!finalize) {
				g_assert_cmpint (g_mime_stream_close (wrapper), ==, 0);
				g_assert_cmpint (g_mime_stream_close (wrapper), ==, 0);
			}
			g_object_unref (wrapper);
			g_assert_cmpuint (bytes->len, ==, flush ? 3 : 0);
			g_assert_cmpint (g_mime_stream_write (source, "x", 1), ==, 1);
			g_object_unref (source);
		}

		source = g_mime_stream_mem_new ();
		wrapper = g_mime_stream_cat_new ();
		g_assert_cmpint (g_mime_stream_cat_add_source (GMIME_STREAM_CAT (wrapper), source), ==, 0);
		g_assert_cmpint (g_mime_stream_cat_add_source (GMIME_STREAM_CAT (wrapper), source), ==, 0);
		if (!finalize) {
			g_assert_cmpint (g_mime_stream_close (wrapper), ==, 0);
			g_assert_cmpint (g_mime_stream_close (wrapper), ==, 0);
		}
		g_object_unref (wrapper);
		g_assert_cmpint (g_mime_stream_write (source, "x", 1), ==, 1);
		g_object_unref (source);

		for (owner = 0; owner < 2; owner++) {
			source = g_mime_stream_mem_new ();
			wrapper = g_mime_stream_filter_new (source);
			g_mime_stream_filter_set_owner (GMIME_STREAM_FILTER (wrapper), owner);
			g_assert_cmpint (g_mime_stream_write (wrapper, "abc", 3), ==, 3);
			if (!finalize) {
				g_assert_cmpint (g_mime_stream_close (wrapper), ==, 0);
				g_assert_cmpint (g_mime_stream_close (wrapper), ==, 0);
			}
			g_object_unref (wrapper);
			errno = 0;
			g_assert_cmpint (g_mime_stream_write (source, "x", 1), ==, owner && !finalize ? -1 : 1);
			if (owner && !finalize)
				g_assert_cmpint (errno, ==, EBADF);
			g_object_unref (source);
		}
	}
}

int
main (int argc, char **argv)
{
	g_mime_init ();
	testsuite_init (argc, argv);
	testsuite_start ("Stream close contracts");
	testsuite_check ("GIO closes both streams, preserves the first error and releases each reference once");
	test_gio_close (FALSE);
	testsuite_check_passed ();
	testsuite_check ("GIO finalization closes and releases resources even on failure");
	test_gio_close (TRUE);
	testsuite_check_passed ();
	testsuite_check ("GIO substreams retain their parent's lifetime");
	test_gio_lifetime ();
	testsuite_check_passed ();
	testsuite_check ("Buffer, concatenation and filter close/finalization ownership");
	test_wrappers ();
	testsuite_check_passed ();
	testsuite_end ();
	g_mime_shutdown ();
	return testsuite_exit ();
}
