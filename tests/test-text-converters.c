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

#include <errno.h>
#include <string.h>
#include <gmime/gmime.h>

typedef struct {
	GMimeStream parent;
	const char *data;
	gsize length, position, chunk;
	GByteArray *written;
	gssize read_limit, write_limit;
	int failure;
	gboolean zero_progress, fail_flush;
	guint closes;
} ChunkStream;

typedef GMimeStreamClass ChunkStreamClass;

G_DEFINE_TYPE (ChunkStream, chunk_stream, GMIME_TYPE_STREAM)

static ssize_t
chunk_read (GMimeStream *stream, char *buffer, size_t length)
{
	ChunkStream *chunk = (ChunkStream *) stream;

	if (chunk->read_limit >= 0 && chunk->position >= (gsize) chunk->read_limit) {
		errno = chunk->failure;
		return chunk->zero_progress ? 0 : -1;
	}

	length = MIN (length, MIN (chunk->chunk, chunk->length - chunk->position));
	if (chunk->read_limit >= 0)
		length = MIN (length, (gsize) chunk->read_limit - chunk->position);

	if (length)
		memcpy (buffer, chunk->data + chunk->position, length);

	chunk->position += length;
	return length;
}

static ssize_t
chunk_write (GMimeStream *stream, const char *buffer, size_t length)
{
	ChunkStream *chunk = (ChunkStream *) stream;

	if (chunk->write_limit >= 0 && chunk->written->len >= (gsize) chunk->write_limit) {
		errno = chunk->failure;
		return chunk->zero_progress ? 0 : -1;
	}

	length = MIN (length, chunk->chunk);
	if (chunk->write_limit >= 0)
		length = MIN (length, (gsize) chunk->write_limit - chunk->written->len);

	g_byte_array_append (chunk->written, (const guint8 *) buffer, length);
	return length;
}

static gboolean
chunk_eos (GMimeStream *stream)
{
	ChunkStream *chunk = (ChunkStream *) stream;

	return chunk->position == chunk->length;
}

static int
chunk_flush (GMimeStream *stream)
{
	ChunkStream *chunk = (ChunkStream *) stream;

	errno = chunk->failure;
	return chunk->fail_flush ? -1 : 0;
}

static int
chunk_close (GMimeStream *stream)
{
	((ChunkStream *) stream)->closes++;
	return 0;
}

static void
chunk_finalize (GObject *object)
{
	g_byte_array_unref (((ChunkStream *) object)->written);
	G_OBJECT_CLASS (chunk_stream_parent_class)->finalize (object);
}

static void
chunk_stream_class_init (ChunkStreamClass *klass)
{
	klass->read = chunk_read;
	klass->write = chunk_write;
	klass->eos = chunk_eos;
	klass->flush = chunk_flush;
	klass->close = chunk_close;
	G_OBJECT_CLASS (klass)->finalize = chunk_finalize;
}

static void
chunk_stream_init (ChunkStream *stream)
{
	g_mime_stream_construct (GMIME_STREAM (stream), 0, -1);
	stream->chunk = 1;
	stream->read_limit = stream->write_limit = -1;
	stream->failure = EIO;
	stream->written = g_byte_array_new ();
}

static void
check_conversion (GMimeTextConverter *converter, const char *text, const char *expected)
{
	static const gsize chunks[] = { 1, 2, 3, 15, 16, 17, 127, 128, 4095, 4096 };
	GError *error = NULL;
	char *result;
	guint i;

	result = g_mime_text_converter_convert_to_string (converter, text, -1, &error);
	g_assert_no_error (error);
	g_assert_cmpstr (result, ==, expected);
	g_free (result);

	for (i = 0; i < G_N_ELEMENTS (chunks); i++) {
		ChunkStream *input = g_object_new (chunk_stream_get_type (), NULL);
		ChunkStream *output = g_object_new (chunk_stream_get_type (), NULL);

		input->data = text;
		input->length = strlen (text);
		input->chunk = chunks[i];
		output->chunk = 3;
		g_assert_true (
			g_mime_text_converter_convert (converter, GMIME_STREAM (input), GMIME_STREAM (output), &error));
		g_assert_no_error (error);
		g_assert_cmpuint (input->position, ==, input->length);
		g_assert_cmpmem (output->written->data, output->written->len, expected, strlen (expected));
		g_assert_cmpuint (input->closes, ==, 0);
		g_assert_cmpuint (output->closes, ==, 0);
		g_object_unref (output);
		g_object_unref (input);
	}
}

static void
test_conversion (void)
{
	static const struct {
		const char *input, *flowed, *plain;
	} cases[] = { { "", "", "" },
		      { "\r\n\r\n", "\n\n", "\n\n" },
		      { "one\r\ntwo\rthree\nfour", "one\ntwo\nthree\nfour\n", "one\ntwo\nthree\nfour\n" },
		      { "From here\n leading\n> a\n> > b\n", " From here\n  leading\n> a\n>> b\n",
			"From here\n leading\n> a\n>> b\n" },
		      { "ends with spaces   ", "ends with spaces\n", "ends with spaces\n" },
		      { " \n>\n>>   \n", "\n> \n>> \n", "\n> \n>> \n" },
		      { "caf\xc3\xa9 \xf0\x9f\x98\x80\n", "caf\xc3\xa9 \xf0\x9f\x98\x80\n",
			"caf\xc3\xa9 \xf0\x9f\x98\x80\n" } };

	GMimeTextConverter *copy = g_mime_text_to_text_new ();
	GMimeTextConverter *flow = g_mime_text_to_flowed_new ();
	GMimeTextConverter *unflow = g_mime_flowed_to_text_new ();
	GError *error = NULL;
	const char bounded[] = { 'a', 'b', 'c' };
	char *result;
	guint i;

	for (i = 0; i < G_N_ELEMENTS (cases); i++) {
		check_conversion (copy, cases[i].input, cases[i].input);
		check_conversion (flow, cases[i].input, cases[i].flowed);
		check_conversion (unflow, cases[i].flowed, cases[i].plain);
	}

	g_mime_text_converter_set_header (copy, "prefix:");
	g_mime_text_converter_set_footer (copy, ":suffix");
	check_conversion (copy, "body", "prefix:body:suffix");
	check_conversion (copy, "", "prefix::suffix");
	g_mime_text_converter_set_header (flow, "prefix:");
	g_mime_text_converter_set_footer (flow, ":suffix");
	check_conversion (flow, "body", "prefix:body\n:suffix");
	g_mime_text_converter_set_header (unflow, "prefix:");
	g_mime_text_converter_set_footer (unflow, ":suffix");
	check_conversion (unflow, "body \ncontinued", "prefix:body continued\n:suffix");

	g_mime_text_converter_set_header (copy, NULL);
	g_mime_text_converter_set_footer (copy, NULL);
	result = g_mime_text_converter_convert_to_string (copy, bounded, sizeof (bounded), &error);
	g_assert_no_error (error);
	g_assert_cmpstr (result, ==, "abc");
	g_free (result);

	g_object_unref (copy);
	g_object_unref (flow);
	g_object_unref (unflow);
}

static void
test_unflow (void)
{
	GMimeTextConverter *converter = g_mime_flowed_to_text_new ();

	check_conversion (converter, "one \r\ntwo \r\nthree", "one two three\n");
	check_conversion (converter, "> one \n>> two\n", "> one \n>> two\n");
	check_conversion (converter, " one\n From here\n >not quoted\n", "one\nFrom here\n>not quoted\n");
	check_conversion (converter, "last ", "last ");

	g_mime_flowed_to_text_set_delete_space (GMIME_FLOWED_TO_TEXT (converter), TRUE);
	check_conversion (converter, "one \r\ntwo", "onetwo\n");
	check_conversion (converter, "one  \ntwo", "one two\n");
	check_conversion (converter, "> one \n>> two\n", "> one\n>> two\n");
	check_conversion (converter, "last ", "last");
	check_conversion (converter, ">  ", "");
	check_conversion (converter, " \n", "\n");
	g_object_unref (converter);
}

static void
test_wrapping (void)
{
	static const guint depths[] = { 0, 2, 38, 70, 75, 76, 77, 90, 200, 996, 997, 2000, 5000 };
	GMimeTextConverter *flow = g_mime_text_to_flowed_new ();
	GMimeTextConverter *unflow = g_mime_flowed_to_text_new ();
	GString *input = g_string_new (NULL);
	GString *expected = g_string_new (NULL);
	GError *error = NULL;
	guint i, j;
	char *flowed, *plain;

	g_mime_flowed_to_text_set_delete_space (GMIME_FLOWED_TO_TEXT (unflow), TRUE);
	for (i = 0; i < 75; i++)
		g_string_append_c (input, 'a');

	g_string_append (expected, input->str);
	g_string_append (input, " >not quoted\n");
	g_string_append (expected, "  \n >not quoted\n");
	check_conversion (flow, input->str, expected->str);
	check_conversion (unflow, expected->str, input->str);

	g_string_truncate (input, 0);
	g_string_truncate (expected, 0);
	for (i = 0; i < 2000; i++)
		g_string_append_c (input, 'x');

	g_string_append_c (input, '\n');
	g_string_append_len (expected, input->str, 997);
	g_string_append (expected, " \n");
	g_string_append_len (expected, input->str, 997);
	g_string_append (expected, " \nxxxxxx\n");
	check_conversion (flow, input->str, expected->str);
	check_conversion (unflow, expected->str, input->str);

	for (i = 0; i < G_N_ELEMENTS (depths); i++) {
		char **lines;
		gsize limit = MAX (998, (depths[i] + 1) * 2);

		g_string_truncate (input, 0);
		for (j = 0; j < depths[i]; j++)
			g_string_append_c (input, '>');

		if (depths[i])
			g_string_append_c (input, ' ');

		for (j = 0; j < 5000; j++)
			g_string_append (input, "a ");

		g_string_append (input, "end\n");
		flowed = g_mime_text_converter_convert_to_string (flow, input->str, input->len, &error);
		g_assert_no_error (error);
		g_assert_cmpuint (strlen (flowed), <=, input->len * 3);
		lines = g_strsplit (flowed, "\n", -1);
		for (j = 0; lines[j]; j++)
			g_assert_cmpuint (strlen (lines[j]), <=, limit);

		plain = g_mime_text_converter_convert_to_string (unflow, flowed, -1, &error);
		g_assert_no_error (error);
		g_assert_cmpstr (plain, ==, input->str);
		g_free (plain);
		g_strfreev (lines);
		g_free (flowed);
	}

	/* Multi-byte characters straddle reader buffers and hard wrapping limits. */
	g_string_truncate (input, 0);
	g_string_truncate (expected, 0);
	for (i = 0; i < 2100; i++) {
		g_string_append (input, "\xf0\x9f\x98\x80");
		g_string_append (expected, "\xf0\x9f\x98\x80");
		if (i == 996 || i == 1993)
			g_string_append (expected, " \n");
	}

	g_string_append_c (input, '\n');
	g_string_append_c (expected, '\n');
	check_conversion (flow, input->str, expected->str);
	check_conversion (unflow, expected->str, input->str);

	g_string_free (input, TRUE);
	g_string_free (expected, TRUE);
	g_object_unref (flow);
	g_object_unref (unflow);
}

static void
test_invalid_text (void)
{
	static const struct {
		const char *text;
		gsize length;
		int code;
	} cases[] = { { "a\0b", 3, G_CONVERT_ERROR_ILLEGAL_SEQUENCE },
		      { "a\xc0\xaf", 3, G_CONVERT_ERROR_ILLEGAL_SEQUENCE },
		      { "a\xed\xa0\x80", 4, G_CONVERT_ERROR_ILLEGAL_SEQUENCE },
		      { "a\xf4\x90\x80\x80", 5, G_CONVERT_ERROR_ILLEGAL_SEQUENCE },
		      { "a\xf0\x9f", 3, G_CONVERT_ERROR_PARTIAL_INPUT } };

	GMimeTextConverter *converters[] = { g_mime_text_to_text_new (), g_mime_text_to_flowed_new (),
					     g_mime_flowed_to_text_new () };
	GError *error = NULL;
	guint i, j;

	for (i = 0; i < G_N_ELEMENTS (converters); i++) {
		for (j = 0; j < G_N_ELEMENTS (cases); j++) {
			ChunkStream *input = g_object_new (chunk_stream_get_type (), NULL);
			ChunkStream *output = g_object_new (chunk_stream_get_type (), NULL);
			char *result = g_mime_text_converter_convert_to_string (converters[i], cases[j].text,
										cases[j].length, &error);

			g_assert_null (result);
			g_assert_error (error, G_CONVERT_ERROR, cases[j].code);
			g_clear_error (&error);
			input->data = cases[j].text;
			input->length = cases[j].length;
			g_assert_false (g_mime_text_converter_convert (converters[i], GMIME_STREAM (input),
								       GMIME_STREAM (output), &error));
			g_assert_error (error, G_CONVERT_ERROR, cases[j].code);
			g_clear_error (&error);
			g_object_unref (input);
			g_object_unref (output);
		}

		check_conversion (converters[i], "retry\n", "retry\n");
		g_object_unref (converters[i]);
	}
}

static void
check_io_failures (gboolean wrapped)
{
	GMimeTextConverter *converters[] = { g_mime_text_to_text_new (), g_mime_text_to_flowed_new (),
					     g_mime_flowed_to_text_new () };
	GError *error = NULL;
	guint i, mode;

	for (i = 0; i < G_N_ELEMENTS (converters); i++) {
		if (wrapped) {
			g_mime_text_converter_set_input_stream_buffer_size (converters[i], 1);
			g_mime_text_converter_set_output_stream_buffer_size (converters[i], 1);
		}

		for (mode = 0; mode < 9; mode++) {
			ChunkStream *input = g_object_new (chunk_stream_get_type (), NULL);
			ChunkStream *output = g_object_new (chunk_stream_get_type (), NULL);
			int code = mode == 2 || mode == 5 ? GMIME_ERROR_GENERAL : EIO;

			input->data = "xxbody\n";
			input->length = strlen (input->data);
			input->position = 2;
			if (mode < 3) {
				input->read_limit = 5;
				input->zero_progress = mode == 1;
				input->failure = mode == 2 ? 0 : EIO;
			} else if (mode < 6) {
				output->write_limit = 2;
				output->zero_progress = mode == 4;
				output->failure = mode == 5 ? 0 : EIO;
			} else if (mode == 6) {
				output->fail_flush = TRUE;
			} else {
				g_mime_text_converter_set_header (converters[i], mode == 7 ? "header:" : NULL);
				g_mime_text_converter_set_footer (converters[i], mode == 8 ? ":footer" : NULL);
				output->write_limit = mode == 7 ? 2 : 7;
			}

			g_test_message ("converter %u, I/O failure mode %u", i, mode);
			g_assert_false (g_mime_text_converter_convert (converters[i], GMIME_STREAM (input),
								       GMIME_STREAM (output), &error));
			g_assert_error (error, GMIME_ERROR, code);
			g_clear_error (&error);
			g_assert_cmpuint (input->closes, ==, 0);
			g_assert_cmpuint (output->closes, ==, 0);
			input->read_limit = output->write_limit = -1;
			output->fail_flush = FALSE;
			g_mime_text_converter_set_header (converters[i], NULL);
			g_mime_text_converter_set_footer (converters[i], NULL);
			input->position = 2;
			g_byte_array_set_size (output->written, 0);
			g_assert_true (g_mime_text_converter_convert (converters[i], GMIME_STREAM (input),
								      GMIME_STREAM (output), &error));
			g_assert_no_error (error);
			g_assert_cmpmem (output->written->data, output->written->len, "body\n", 5);
			g_assert_false (g_mime_text_converter_convert (converters[i], GMIME_STREAM (input),
								       GMIME_STREAM (input), &error));
			g_assert_error (error, GMIME_ERROR, GMIME_ERROR_INVALID_OPERATION);
			g_clear_error (&error);
			g_object_unref (input);
			g_object_unref (output);
		}

		g_object_unref (converters[i]);
	}

	/* Error reporting must not be required for a failed conversion to fail. */
	{
		GMimeTextConverter *converter = g_mime_text_to_text_new ();
		char *result = g_mime_text_converter_convert_to_string (converter, "\xc0", 1, NULL);

		g_assert_null (result);
		g_object_unref (converter);
	}
}

static void
test_io_failures (void)
{
	check_io_failures (FALSE);
	check_io_failures (TRUE);
}

static void
check_encoded_conversion (GMimeTextConverter *converter, const char *text, gsize length, const char *expected,
			  gsize expected_length)
{
	static const gsize chunks[] = { 1, 2, 3, 4096 };
	guint i;

	for (i = 0; i < G_N_ELEMENTS (chunks); i++) {
		ChunkStream *input = g_object_new (chunk_stream_get_type (), NULL);
		ChunkStream *output = g_object_new (chunk_stream_get_type (), NULL);
		GError *error = NULL;

		input->data = text;
		input->length = length;
		input->chunk = chunks[i];
		output->chunk = 2;
		g_assert_true (
			g_mime_text_converter_convert (converter, GMIME_STREAM (input), GMIME_STREAM (output), &error));
		g_assert_no_error (error);
		g_assert_cmpmem (output->written->data, output->written->len, expected, expected_length);
		g_assert_cmpuint (input->closes, ==, 0);
		g_assert_cmpuint (output->closes, ==, 0);
		g_object_unref (input);
		g_object_unref (output);
	}
}

static void
test_encodings (void)
{
	static const struct {
		const char *bytes;
		gsize length;
	} bom_cases[] = { { "\xef\xbb\xbf"
			    "a\xc3\xa9\n",
			    7 },
			  { "\xff\xfe"
			    "a\0\xe9\0\n\0",
			    8 },
			  { "\xfe\xff\0a\0\xe9\0\n", 8 },
			  { "\xff\xfe\0\0a\0\0\0\xe9\0\0\0\n\0\0\0", 16 },
			  { "\0\0\xfe\xff\0\0\0a\0\0\0\xe9\0\0\0\n", 16 } };

	GMimeTextConverter *converter = g_mime_text_to_text_new ();
	GError *error = NULL;
	char *result;
	guint i;

	g_mime_text_converter_set_detect_encoding_from_byte_order_mark (converter, TRUE);
	for (i = 0; i < G_N_ELEMENTS (bom_cases); i++)
		check_encoded_conversion (converter, bom_cases[i].bytes, bom_cases[i].length, "a\xc3\xa9\n", 4);

	check_encoded_conversion (converter, "a\xc3\xa9\n", 4, "a\xc3\xa9\n", 4);
	check_encoded_conversion (converter, "", 0, "", 0);

	g_mime_text_converter_set_detect_encoding_from_byte_order_mark (converter, FALSE);
	check_encoded_conversion (converter, bom_cases[0].bytes, bom_cases[0].length, bom_cases[0].bytes,
				  bom_cases[0].length);
	g_assert_true (g_mime_text_converter_set_input_charset (converter, "iso-8859-1", &error));
	g_assert_true (g_mime_text_converter_set_output_charset (converter, "utf-16", &error));
	g_assert_no_error (error);
	check_encoded_conversion (converter, "a\xe9\n", 3, "a\0\xe9\0\n\0", 6);
	g_mime_text_converter_set_write_bom (converter, TRUE);
	check_encoded_conversion (converter, "a\xe9\n", 3,
				  "\xff\xfe"
				  "a\0\xe9\0\n\0",
				  8);
	check_encoded_conversion (converter, "", 0, "\xff\xfe", 2);

	/* Byte-oriented settings never reinterpret string input or its result. */
	result = g_mime_text_converter_convert_to_string (converter, "a\xc3\xa9", -1, &error);
	g_assert_no_error (error);
	g_assert_cmpstr (result, ==, "a\xc3\xa9");
	g_free (result);
	g_assert_false (g_mime_text_converter_set_input_charset (converter, "not-a-charset", &error));
	g_assert_error (error, G_CONVERT_ERROR, G_CONVERT_ERROR_NO_CONVERSION);
	g_clear_error (&error);
	g_assert_cmpstr (g_mime_text_converter_get_input_charset (converter), ==, "iso-8859-1");

	g_mime_text_converter_set_write_bom (converter, FALSE);
	g_assert_true (g_mime_text_converter_set_input_charset (converter, "utf-8", &error));
	g_assert_true (g_mime_text_converter_set_output_charset (converter, "iso-2022-jp", &error));
	g_assert_no_error (error);
	check_encoded_conversion (converter, "\xe3\x81\x82", 3, "\x1b$B$\"\x1b(B", 8);
	g_assert_true (g_mime_text_converter_set_input_charset (converter, "iso-2022-jp", &error));
	g_assert_true (g_mime_text_converter_set_output_charset (converter, "utf-8", &error));
	check_encoded_conversion (converter, "\x1b$B$\"\x1b(B", 8, "\xe3\x81\x82", 3);

	{
		GString *utf8 = g_string_new (NULL);
		GString *encoded = g_string_new ("\x1b$B");

		for (i = 0; i < 3000; i++) {
			g_string_append (utf8, "\xe3\x81\x82");
			g_string_append (encoded, "$\"");
		}

		g_string_append (encoded, "\x1b(B");
		check_encoded_conversion (converter, encoded->str, encoded->len, utf8->str, utf8->len);
		g_assert_true (g_mime_text_converter_set_input_charset (converter, "utf-8", &error));
		g_assert_true (g_mime_text_converter_set_output_charset (converter, "iso-2022-jp", &error));
		check_encoded_conversion (converter, utf8->str, utf8->len, encoded->str, encoded->len);
		g_string_free (encoded, TRUE);
		g_string_free (utf8, TRUE);
	}

	for (i = 0; i < 2; i++) {
		GMimeStream *input, *output = g_mime_stream_mem_new ();
		int code = i ? G_CONVERT_ERROR_ILLEGAL_SEQUENCE : G_CONVERT_ERROR_PARTIAL_INPUT;

		g_assert_true (g_mime_text_converter_set_input_charset (converter, i ? "utf-8" : "utf-16le", &error));
		g_assert_true (g_mime_text_converter_set_output_charset (converter, i ? "us-ascii" : "utf-8", &error));
		input = g_mime_stream_mem_new_with_buffer (i ? "\xc3\xa9" : "a\0b", i ? 2 : 3);
		g_assert_false (g_mime_text_converter_convert (converter, input, output, &error));
		g_assert_error (error, G_CONVERT_ERROR, code);
		g_clear_error (&error);
		g_object_unref (input);
		g_object_unref (output);
	}

	g_object_unref (converter);
}

int
main (int argc, char **argv)
{
	int result;

	g_test_init (&argc, &argv, NULL);
	g_mime_init ();
	g_test_add_func ("/text/conversion", test_conversion);
	g_test_add_func ("/text/unflow", test_unflow);
	g_test_add_func ("/text/wrapping", test_wrapping);
	g_test_add_func ("/text/invalid", test_invalid_text);
	g_test_add_func ("/text/io-failures", test_io_failures);
	g_test_add_func ("/text/encodings", test_encodings);
	result = g_test_run ();
	g_mime_shutdown ();
	return result;
}
