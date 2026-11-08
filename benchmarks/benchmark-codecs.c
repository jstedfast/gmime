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

#include "benchmark.h"
#include <gmime/gmime-encoding-validator-private.h>
#include <errno.h>
#include <string.h>

typedef enum { BASE64, QP, UU } Codec;
typedef enum { ENCODE, DECODE, STRIP, UNIX2DOS, DOS2UNIX, MBOX_FROM, ARMORED_FROM } FilterKind;

typedef struct {
	GBytes *input;
	GByteArray *array;
	Codec codec;
	FilterKind kind;
	guint width, chunk;
} FilterData;

static GMimeEncoder *
new_encoder (Codec codec, guint width)
{
	switch (codec) {
	case BASE64:
		return GMIME_ENCODER (g_mime_base64_encoder_new_with_newline_format (GMIME_NEWLINE_FORMAT_DOS, width));
	case QP:
		return GMIME_ENCODER (g_mime_quoted_printable_encoder_new_with_newline_format (GMIME_NEWLINE_FORMAT_DOS, width));
	case UU:
		return GMIME_ENCODER (g_mime_uu_encoder_new_with_newline_format (GMIME_NEWLINE_FORMAT_DOS));
	default:
		g_error ("Unknown codec");
	}
}

static GMimeDecoder *
new_decoder (Codec codec, gboolean payload_only)
{
	switch (codec) {
	case BASE64: return GMIME_DECODER (g_mime_base64_decoder_new ());
	case QP: return GMIME_DECODER (g_mime_quoted_printable_decoder_new (FALSE));
	case UU: return GMIME_DECODER (g_mime_uu_decoder_new (payload_only));
	default: g_error ("Unknown codec");
	}
}

static gsize
codec_transform (FilterData *test, GBytes **result)
{
	gsize length, capacity, written;
	const guint8 *input = g_bytes_get_data (test->input, &length);
	guint8 *output;

	if (test->kind == ENCODE) {
		GMimeEncoder *encoder = new_encoder (test->codec, test->width);
		capacity = g_mime_encoder_estimate_output_length (encoder, length);
		output = g_malloc (capacity);
		written = g_mime_encoder_flush (encoder, input, length, output, capacity);
		g_object_unref (encoder);
	} else {
		GMimeDecoder *decoder = new_decoder (test->codec, FALSE);
		capacity = g_mime_decoder_estimate_output_length (decoder, length);
		output = g_malloc (capacity);
		written = g_mime_decoder_decode (decoder, input, length, output, capacity);
		g_object_unref (decoder);
	}

	if (result)
		*result = g_bytes_new_take (output, written);
	else
		g_free (output);

	return written;
}

static guint64
run_codec (gpointer data)
{
	return codec_transform (data, NULL);
}

static GMimeFilter *
new_filter (FilterData *test)
{
	GMimeFilter *filter;

	switch (test->kind) {
	case ENCODE: {
		GMimeEncoder *encoder = new_encoder (test->codec, test->width);
		filter = g_mime_filter_basic_new_with_encoder (encoder);
		g_object_unref (encoder);
		return filter;
	}
	case DECODE: {
		GMimeDecoder *decoder = new_decoder (test->codec, FALSE);
		filter = g_mime_filter_basic_new_with_decoder (decoder);
		g_object_unref (decoder);
		return filter;
	}
	case STRIP: return g_mime_filter_strip_new ();
	case UNIX2DOS: return g_mime_filter_unix2dos_new (FALSE);
	case DOS2UNIX: return g_mime_filter_dos2unix_new (FALSE);
	case MBOX_FROM: return g_mime_filter_from_new (GMIME_FILTER_FROM_MODE_ESCAPE);
	case ARMORED_FROM: return g_mime_filter_from_new (GMIME_FILTER_FROM_MODE_ARMOR);
	default: g_error ("Unknown filter");
	}
}

static gsize
filter_transform (FilterData *test, GBytes **result)
{
	gboolean memory = result || test->kind == MBOX_FROM || test->kind == ARMORED_FROM;
	GMimeStream *output = memory ? g_mime_stream_mem_new () : g_mime_stream_null_new ();
	GMimeStream *filtered = g_mime_stream_filter_new (output);
	GMimeStream *input = g_mime_stream_mem_new_with_byte_array (test->array);
	GMimeFilter *filter = new_filter (test);
	char buffer[4096];
	gsize written;
	ssize_t n;

	g_mime_stream_mem_set_owner (GMIME_STREAM_MEM (input), FALSE);
	g_mime_stream_filter_add (GMIME_STREAM_FILTER (filtered), filter);

	while ((n = g_mime_stream_read (input, buffer, test->chunk)) > 0) {
		if (g_mime_stream_write (filtered, buffer, n) != n)
			g_error ("Filtered stream write failed: %s", g_strerror (errno));
	}

	if (n < 0 || g_mime_stream_flush (filtered) != 0)
		g_error ("Filtered stream read/flush failed: %s", g_strerror (errno));

	if (memory) {
		GByteArray *array = g_mime_stream_mem_get_byte_array (GMIME_STREAM_MEM (output));
		written = array->len;

		if (result)
			*result = g_bytes_new (array->data, array->len);
	} else {
		written = g_mime_stream_null_get_written (GMIME_STREAM_NULL (output));
	}

	g_object_unref (input);
	g_object_unref (filtered);
	g_object_unref (filter);
	g_object_unref (output);

	return written;
}

static guint64
run_filter (gpointer data)
{
	return filter_transform (data, NULL);
}

static GBytes *
load_fixture (const char *datadir, const char *subdir, const char *name, gboolean dos)
{
	char *directory = g_build_filename (datadir, subdir, NULL);
	GBytes *bytes = benchmark_load (directory, name);

	g_free (directory);

	if (dos) {
		GBytes *normalized = benchmark_newlines (bytes, TRUE);
		g_bytes_unref (bytes);
		return normalized;
	}

	return bytes;
}

static void
verify_encode (FilterData *test, GBytes *encoded, GBytes *original)
{
	GMimeDecoder *decoder = new_decoder (test->codec, TRUE);
	gsize size = g_bytes_get_size (encoded), capacity, written;
	GBytes *decoded;
	guint8 *output;

	capacity = g_mime_decoder_estimate_output_length (decoder, size);
	output = g_malloc (capacity);
	written = g_mime_decoder_decode (decoder, g_bytes_get_data (encoded, NULL), size, output, capacity);
	decoded = g_bytes_new_take (output, written);

	benchmark_equal (decoded, original, "encoding roundtrip");

	g_bytes_unref (decoded);
	g_object_unref (decoder);
}

static void
codec_benchmarks (const char *datadir, const char *testdatadir)
{
	static const char *names[] = { "Base64", "QuotedPrintable", "UU" };
	static const char *encoded_files[] = { "photo.b64", "wikipedia.qp", "photo.uu" };
	guint codec, encode, stream, width;

	for (codec = 0; codec < 3; codec++) {
		for (encode = 0; encode < 2; encode++) {
			for (stream = 0; stream < 2; stream++) {
				if (encode && codec != BASE64 && !stream)
					continue;

				for (width = encode && codec == BASE64 ? 60 : 76; width <= 76; width += 4) {
					char *name = encode && codec == BASE64 ?
						g_strdup_printf ("%s/%sEncode/Width%u", stream ? "Filter" : "Codec", names[codec], width) :
						g_strdup_printf ("%s/%s%s", stream ? "Filter" : "Codec", names[codec], encode ? "Encode" : "Decode");

					if (benchmark_selected (name)) {
						GBytes *original, *expected, *actual;
						FilterData test = { 0 };

						original = load_fixture (codec == QP ? datadir : testdatadir, codec == QP ? NULL : "encodings",
							codec == QP ? "wikipedia.txt" : "photo.jpg", FALSE);

							test.kind = encode ? ENCODE : DECODE;
						test.codec = codec;
						test.width = width;
						test.chunk = 4096;
						test.input = encode ? g_bytes_ref (original) :
							load_fixture (codec == QP ? datadir : testdatadir, codec == QP ? NULL : "encodings",
								      encoded_files[codec], codec != QP);
						test.array = g_bytes_unref_to_array (g_bytes_ref (test.input));

						codec_transform (&test, &expected);

						if (encode)
							verify_encode (&test, expected, original);
						else
							benchmark_equal (expected, original, name);

						filter_transform (&test, &actual);
						benchmark_equal (actual, expected, name);

						if ((stream ? run_filter (&test) : run_codec (&test)) != g_bytes_get_size (expected))
							g_error ("%s: incorrect output length", name);

						benchmark_run (name, g_bytes_get_size (test.input), stream ? run_filter : run_codec, &test);

						g_bytes_unref (original);
						g_bytes_unref (actual);
						g_bytes_unref (expected);
						g_byte_array_unref (test.array);
						g_bytes_unref (test.input);
					}

					g_free (name);
				}
			}
		}
	}
}

typedef struct {
	GBytes *input;
	Codec codec;
	guint chunk;
} ValidatorData;

static void
count_encoding_issue (const GMimeEncodingIssue *issue, gpointer data)
{
	guint64 *count = data;
	(*count)++;
}

static guint64
run_validator (gpointer data)
{
	ValidatorData *test = data;
	GMimeEncodingValidator *validator;
	gsize length, offset;
	const guint8 *input = g_bytes_get_data (test->input, &length);
	guint64 count = 0;

	switch (test->codec) {
	case BASE64: validator = _g_mime_base64_validator_new (count_encoding_issue, &count, 0, 1); break;
	case QP: validator = _g_mime_quoted_printable_validator_new (count_encoding_issue, &count, 0, 1); break;
	case UU: validator = _g_mime_uu_validator_new (count_encoding_issue, &count, 0, 1); break;
	default: g_error ("Unknown validator");
	}

	for (offset = 0; offset < length; offset += test->chunk)
		_g_mime_encoding_validator_write (validator, input + offset, MIN (test->chunk, length - offset));

	_g_mime_encoding_validator_flush (validator);
	g_object_unref (validator);

	return count;
}

static void
validate_and_run (const char *name, ValidatorData *test, gboolean malformed)
{
	guint64 expected;

	/* Byte-sized writes provide an untimed check of vector/chunk state transitions. */
	test->chunk = 1;
	expected = run_validator (test);
	test->chunk = 4096;

	if (run_validator (test) != expected || (malformed ? expected == 0 : expected != 0))
		g_error ("%s: incorrect compliance issue count", name);

	benchmark_run (name, g_bytes_get_size (test->input), run_validator, test);
}

static GBytes *
base64_layout (guint layout)
{
	static const guint widths[] = { G_MAXUINT, 76, 76, 7, 23, G_MAXUINT };
	static const char *separators[] = { "", "\r\n", "\n", " \t\r\n", "!? \t\r\n", "" };
	guint8 data[65536];
	guint32 random = 12345;
	GString *text = g_string_new (NULL);
	char *base64;
	gsize i, length, count;

	/* Deterministic native data; the layout matches upstream, not .NET Random's sequence. */
	for (i = 0; i < sizeof (data); i++) {
		random = random * 1664525u + 1013904223u;
		data[i] = random >> 24;
	}

	base64 = g_base64_encode (data, layout == 5 ? 48 : sizeof (data));
	length = strlen (base64);

	for (i = 0; i < length; i += count) {
		count = MIN (widths[layout], length - i);
		g_string_append_len (text, base64 + i, count);
		g_string_append (text, separators[layout]);
	}

	g_free (base64);

	return g_string_free_to_bytes (text);
}

static void
validator_benchmarks (const char *datadir, const char *testdatadir)
{
	static const char *layouts[] = { "Unwrapped", "MimeCrlf", "MimeLf", "ShortLines", "Garbage", "Small" };
	static const char *names[] = { "Base64", "QuotedPrintable", "UU", "UUInvalidContent", "UUExtraLineData", "UUSingleLongLine" };
	guint i;

	for (i = 0; i < G_N_ELEMENTS (layouts); i++) {
		char *name = g_strdup_printf ("Base64Validator/%s", layouts[i]);

		if (benchmark_selected (name)) {
			ValidatorData test = { base64_layout (i), BASE64, 4096 };
			validate_and_run (name, &test, i == 4);
			g_bytes_unref (test.input);
		}

		g_free (name);
	}

	for (i = 0; i < G_N_ELEMENTS (names); i++) {
		char *name = g_strdup_printf ("EncodingValidator/%s", names[i]);

		if (benchmark_selected (name)) {
			ValidatorData test = { NULL, i == 0 ? BASE64 : i == 1 ? QP : UU, 4096 };

			if (i == 1) {
				GBytes *text = load_fixture (datadir, NULL, "wikipedia.txt", FALSE);
				GString *repeated = g_string_new (NULL);
				FilterData encoder = { 0 };
				guint j;

				for (j = 0; j < 512; j++)
					g_string_append_len (repeated, g_bytes_get_data (text, NULL), g_bytes_get_size (text));

				encoder.input = g_string_free_to_bytes (repeated);
				encoder.kind = ENCODE;
				encoder.codec = QP;
				encoder.width = 76;

				codec_transform (&encoder, &test.input);

				g_bytes_unref (encoder.input);
				g_bytes_unref (text);
			} else {
				test.input = load_fixture (testdatadir, "encodings", i ? "photo.uu" : "photo.b64", TRUE);

				if (i >= 3) {
					gsize size = g_bytes_get_size (test.input);
					GString *text = g_string_new ("begin 644 photo.jpg\r\n");
					char *line = g_strnfill (i == 5 ? size : i == 4 ? 120 : 60, i == 4 ? 'B' : 'a');

					while (text->len < size)
						g_string_append_printf (text, "M%s\r\n", line);

					g_string_append (text, "`\r\nend\r\n");
					g_free (line);
					g_bytes_unref (test.input);

					test.input = g_string_free_to_bytes (text);
				}
			}

			validate_and_run (name, &test, i >= 3);
			g_bytes_unref (test.input);
		}

		g_free (name);
	}
}

static guint64
run_best_encoding (gpointer data)
{
	GBytes *bytes = data;
	gsize length, offset, count, outlen, prespace;
	const char *input = g_bytes_get_data (bytes, &length);
	GMimeFilter *filter = g_mime_filter_best_new (GMIME_FILTER_BEST_ENCODING);
	char buffer[4096], *output;
	guint64 result = 0;
	guint i;

	for (i = 0; i < 100; i++) {
		for (offset = 0; offset < length; offset += count) {
			count = MIN (sizeof (buffer), length - offset);
			memcpy (buffer, input + offset, count);
			g_mime_filter_filter (filter, buffer, count, 0, &output, &outlen, &prespace);
		}

		g_mime_filter_complete (filter, buffer, 0, 0, &output, &outlen, &prespace);
		result += g_mime_filter_best_encoding (GMIME_FILTER_BEST (filter), GMIME_ENCODING_CONSTRAINT_7BIT);
		g_mime_filter_reset (filter);
	}

	g_object_unref (filter);

	return result;
}

static GBytes *
strip_reference (GBytes *bytes)
{
	gsize length, i, start = 0;
	const char *input = g_bytes_get_data (bytes, &length);
	GString *text = g_string_new (NULL);

	for (i = 0; i <= length; i++) {
		if (i == length || input[i] == '\n') {
			gsize end = i;
			gboolean cr = end > start && input[end - 1] == '\r';

			if (cr)
				end--;

			while (end > start && (input[end - 1] == ' ' || input[end - 1] == '\t'))
				end--;

			g_string_append_len (text, input + start, end - start);

			if (cr)
				g_string_append_c (text, '\r');

			if (i != length)
				g_string_append_c (text, '\n');

			start = i + 1;
		}
	}

	return g_string_free_to_bytes (text);
}

static void
filter_benchmarks (const char *datadir, const char *testdatadir)
{
	static const char *names[] = {
		"TrailingWhitespace/LoremIpsum",
		"Unix2Dos/LoremIpsumDos", "Unix2Dos/LoremIpsumUnix",
		"Unix2Dos/PathologicalDos", "Unix2Dos/PathologicalUnix",
		"Dos2Unix/LoremIpsumDos", "Dos2Unix/LoremIpsumUnix",
		"Dos2Unix/PathologicalDos", "Dos2Unix/PathologicalUnix",
		"MboxFrom/FromRussiaWithLove", "ArmoredFrom/FromRussiaWithLove"
	};
	const char sentence[] = "From Russia with love is one of my favorite James Bond files.\n";
	guint i, j;

	for (i = 0; i < G_N_ELEMENTS (names); i++) {
		char *name = g_strdup_printf ("Filter/%s", names[i]);

		if (benchmark_selected (name)) {
			FilterData test = { 0 };
			GBytes *expected, *actual;

			test.kind = i == 0 ? STRIP : i < 5 ? UNIX2DOS : i < 9 ? DOS2UNIX :
				i == 9 ? MBOX_FROM : ARMORED_FROM;
			test.chunk = i < 9 ? 4096 : 1024;

			if (i >= 9) {
				GString *text = g_string_new (NULL), *escaped = g_string_new (NULL);

				for (j = 0; j < 1000; j++) {
					g_string_append (text, sentence);
					g_string_append (escaped, i == 9 ? ">From" : "=46rom");
					g_string_append (escaped, sentence + 4);
				}

				test.input = g_string_free_to_bytes (text);
				expected = g_string_free_to_bytes (escaped);
			} else {
				guint layout = i ? (i - 1) % 4 : 0;
				GBytes *text;

				if (i && layout >= 2) {
					char *lf = g_strnfill (1024, '\n');
					text = g_bytes_new_take (lf, 1024);
				} else {
					text = load_fixture (datadir, NULL, "lorem-ipsum.txt", FALSE);
				}

				/* Unlike upstream's duplicated Unix2Dos setup, the Unix case is LF-only. */
				test.input = i ? benchmark_newlines (text, layout % 2 == 0) : g_bytes_ref (text);
				expected = i ? benchmark_newlines (text, test.kind == UNIX2DOS) : strip_reference (text);

				g_bytes_unref (text);
			}

			test.array = g_bytes_unref_to_array (g_bytes_ref (test.input));
			filter_transform (&test, &actual);
			benchmark_equal (actual, expected, name);

			if (run_filter (&test) != g_bytes_get_size (expected))
				g_error ("%s: incorrect output length", name);

			benchmark_run (name, g_bytes_get_size (test.input), run_filter, &test);

			g_bytes_unref (actual);
			g_bytes_unref (expected);
			g_byte_array_unref (test.array);
			g_bytes_unref (test.input);
		}

		g_free (name);
	}

	for (i = 0; i < 2; i++) {
		const char *name = i ? "Filter/BestEncoding/GirlJpeg" : "Filter/BestEncoding/LoremIpsum";

		if (benchmark_selected (name)) {
			GBytes *input = load_fixture (i ? testdatadir : datadir, i ? "encodings" : NULL,
						     i ? "photo.jpg" : "lorem-ipsum.txt", FALSE);
			guint64 expected = 100 * (i ? GMIME_CONTENT_ENCODING_BASE64 : GMIME_CONTENT_ENCODING_7BIT);

			if (run_best_encoding (input) != expected)
				g_error ("%s: incorrect best encoding", name);

			benchmark_run (name, g_bytes_get_size (input) * 100, run_best_encoding, input);

			g_bytes_unref (input);
		}
	}
}

void
benchmark_codecs (const char *datadir, const char *testdatadir)
{
	codec_benchmarks (datadir, testdatadir);
	validator_benchmarks (datadir, testdatadir);
	filter_benchmarks (datadir, testdatadir);
}
