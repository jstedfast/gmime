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

/* Regression tests for the MimeKit-shaped codec interfaces. */
#include <glib.h>
#include <gmime/gmime-codecs.h>
#include <stdio.h>
#include <string.h>
#ifdef G_OS_UNIX
#include <sys/mman.h>
#include <unistd.h>
#endif

#ifdef GMIME_DISABLE_SIMD
#define CODEC_LOG_DOMAIN "gmime-tests"
#else
#define CODEC_LOG_DOMAIN "gmime"
#endif

static GMimeEncoder *
new_encoder (guint kind)
{
	switch (kind) {
	case 0: return GMIME_ENCODER (g_mime_base64_encoder_new ());
	case 1: return GMIME_ENCODER (g_mime_quoted_printable_encoder_new ());
	case 2: return GMIME_ENCODER (g_mime_uu_encoder_new ());
	case 4: return GMIME_ENCODER (g_mime_yenc_encoder_new ());
	default: return GMIME_ENCODER (g_mime_pass_through_encoder_new (GMIME_CONTENT_ENCODING_BASE64));
	}
}

static GMimeDecoder *
new_decoder (guint kind)
{
	switch (kind) {
	case 0: return GMIME_DECODER (g_mime_base64_decoder_new ());
	case 1: return GMIME_DECODER (g_mime_quoted_printable_decoder_new (FALSE));
	case 2: return GMIME_DECODER (g_mime_uu_decoder_new (TRUE));
	case 4: return GMIME_DECODER (g_mime_yenc_decoder_new (TRUE));
	default: return GMIME_DECODER (g_mime_pass_through_decoder_new (GMIME_CONTENT_ENCODING_BASE64));
	}
}

static GMimeEncoder *
new_encoder_with_newline_format (guint kind, GMimeNewLineFormat newline_format, guint max_line_length)
{
	switch (kind) {
	case 0: return GMIME_ENCODER (g_mime_base64_encoder_new_with_newline_format (newline_format, max_line_length));
	case 1: return GMIME_ENCODER (g_mime_quoted_printable_encoder_new_with_newline_format (newline_format, max_line_length));
	case 2: return GMIME_ENCODER (g_mime_uu_encoder_new_with_newline_format (newline_format));
	case 3: return GMIME_ENCODER (g_mime_yenc_encoder_new_with_newline_format (newline_format, max_line_length));
	default: g_assert_not_reached ();
	}
}

static void
assert_padding (const guint8 *output, gsize written, gsize capacity)
{
	gsize i;
	g_assert_cmpuint (written, <=, capacity);
	for (i = written; i < capacity + 1; i++)
		g_assert_cmpuint (output[i], ==, 0xa5);
}

static GByteArray *
encode_chunks (GMimeEncoder *encoder, const guint8 *input, gsize length, gsize chunk)
{
	GByteArray *result = g_byte_array_new ();
	guint8 *output;
	gsize offset = 0, capacity, written, n;

	while (offset < length) {
		n = MIN (chunk, length - offset);
		capacity = g_mime_encoder_estimate_output_length (encoder, n);
		output = g_malloc (capacity + 1);
		memset (output, 0xa5, capacity + 1);
		written = g_mime_encoder_encode (encoder, input + offset, n, output, capacity);
		assert_padding (output, written, capacity);
		g_byte_array_append (result, output, written);
		g_free (output);
		offset += n;
	}
	capacity = g_mime_encoder_estimate_output_length (encoder, 0);
	output = g_malloc (capacity + 1);
	memset (output, 0xa5, capacity + 1);
	written = g_mime_encoder_flush (encoder, NULL, 0, output, capacity);
	assert_padding (output, written, capacity);
	g_byte_array_append (result, output, written);
	g_free (output);
	return result;
}

static GByteArray *
decode_chunks (GMimeDecoder *decoder, const guint8 *input, gsize length, gsize chunk)
{
	GByteArray *result = g_byte_array_new ();
	guint8 *output;
	gsize offset = 0, capacity, written, n;

	while (offset < length) {
		n = MIN (chunk, length - offset);
		capacity = g_mime_decoder_estimate_output_length (decoder, n);
		output = g_malloc (capacity + 1);
		memset (output, 0xa5, capacity + 1);
		written = g_mime_decoder_decode (decoder, input + offset, n, output, capacity);
		assert_padding (output, written, capacity);
		g_byte_array_append (result, output, written);
		g_free (output);
		offset += n;
	}
	capacity = g_mime_decoder_estimate_output_length (decoder, 0);
	output = g_malloc (capacity + 1);
	memset (output, 0xa5, capacity + 1);
	g_assert_cmpuint (g_mime_decoder_decode (decoder, NULL, 0, output, capacity), ==, 0);
	assert_padding (output, 0, capacity);
	g_free (output);
	return result;
}

static void
test_roundtrip (void)
{
	static const guint chunks[] = { 1, 2, 3, 4, 15, 16, 17, 31, 45, 57, 76, 129, 4096 };
	guint8 input[1025];
	GMimeEncoder *encoder;
	GMimeDecoder *decoder;
	GByteArray *encoded, *decoded, *expected;
	guint i, j, kind;

	for (i = 0; i < sizeof (input); i++)
		input[i] = i * 31;
	for (kind = 0; kind < 5; kind++) {
		encoder = new_encoder (kind);
		expected = encode_chunks (encoder, input, sizeof (input), sizeof (input));
		for (j = 0; j < G_N_ELEMENTS (chunks); j++) {
			encoded = encode_chunks (encoder, input, sizeof (input), chunks[j]);
			g_assert_cmpmem (encoded->data, encoded->len, expected->data, expected->len);
			decoder = new_decoder (kind);
			decoded = decode_chunks (decoder, encoded->data, encoded->len, chunks[j]);
			g_assert_cmpmem (decoded->data, decoded->len, input, sizeof (input));
			g_byte_array_unref (decoded);
			g_byte_array_unref (encoded);
			g_object_unref (decoder);
		}
		g_byte_array_unref (expected);
		g_object_unref (encoder);
	}
}

static void
test_clone_reset (void)
{
	static const GMimeContentEncoding encodings[] = {
		GMIME_CONTENT_ENCODING_BASE64,
		GMIME_CONTENT_ENCODING_QUOTEDPRINTABLE,
		GMIME_CONTENT_ENCODING_UUENCODE,
		GMIME_CONTENT_ENCODING_BASE64,
		GMIME_CONTENT_ENCODING_UNSPECIFIED
	};
	guint8 input[129], scratch[1024], a[1024], b[1024];
	GMimeEncoder *encoder, *enc_copy;
	GMimeDecoder *decoder, *dec_copy;
	GByteArray *encoded;
	gsize n, m, prefix;
	guint kind, i;

	for (i = 0; i < sizeof (input); i++)
		input[i] = i * 31;
	for (kind = 0; kind < 5; kind++) {
		encoder = new_encoder (kind);
		g_assert_cmpint (g_mime_encoder_get_encoding (encoder), ==, encodings[kind]);
		encoded = encode_chunks (encoder, input, sizeof (input), 13);
		for (prefix = 0; prefix <= sizeof (input); prefix++) {
			g_mime_encoder_encode (encoder, input, prefix, scratch, sizeof (scratch));
			enc_copy = g_mime_encoder_clone (encoder);
			g_assert_cmpint (g_mime_encoder_get_encoding (enc_copy), ==, encodings[kind]);
			g_assert_cmpuint (g_mime_encoder_estimate_output_length (encoder, 129 - prefix), ==,
					  g_mime_encoder_estimate_output_length (enc_copy, 129 - prefix));
			n = g_mime_encoder_flush (encoder, input + prefix, 129 - prefix, a, sizeof (a));
			m = g_mime_encoder_flush (enc_copy, input + prefix, 129 - prefix, b, sizeof (b));
			g_assert_cmpmem (a, n, b, m);
			g_mime_encoder_encode (encoder, input, prefix, scratch, sizeof (scratch));
			g_mime_encoder_reset (encoder);
			n = g_mime_encoder_flush (encoder, input, 129, a, sizeof (a));
			g_assert_cmpmem (a, n, encoded->data, encoded->len);
			g_object_unref (enc_copy);
		}
		decoder = new_decoder (kind);
		g_assert_cmpint (g_mime_decoder_get_encoding (decoder), ==, encodings[kind]);
		for (prefix = 0; prefix <= encoded->len; prefix++) {
			g_mime_decoder_reset (decoder);
			g_mime_decoder_decode (decoder, encoded->data, prefix, scratch, sizeof (scratch));
			dec_copy = g_mime_decoder_clone (decoder);
			g_assert_cmpint (g_mime_decoder_get_encoding (dec_copy), ==, encodings[kind]);
			g_assert_cmpuint (g_mime_decoder_estimate_output_length (decoder, encoded->len - prefix), ==,
					  g_mime_decoder_estimate_output_length (dec_copy, encoded->len - prefix));
			n = g_mime_decoder_decode (decoder, encoded->data + prefix, encoded->len - prefix, a, sizeof (a));
			m = g_mime_decoder_decode (dec_copy, encoded->data + prefix, encoded->len - prefix, b, sizeof (b));
			g_assert_cmpmem (a, n, b, m);
			g_mime_decoder_reset (decoder);
			n = g_mime_decoder_decode (decoder, encoded->data, encoded->len, a, sizeof (a));
			g_assert_cmpmem (a, n, input, sizeof (input));
			g_object_unref (dec_copy);
		}
		g_object_unref (decoder);
		g_object_unref (encoder);
		g_byte_array_unref (encoded);
	}
}

static void
test_preconditions (void)
{
	const guint8 input[] = "T=4M";
	guint8 scratch[256], a[256], b[256];
	GMimeEncoder *encoder, *enc_copy;
	GMimeDecoder *decoder, *dec_copy;
	gsize capacity, n, m;
	guint kind;

	for (kind = 0; kind < 5; kind++) {
		encoder = new_encoder (kind);
		g_mime_encoder_encode (encoder, input, 1, scratch, sizeof (scratch));
		enc_copy = g_mime_encoder_clone (encoder);
		capacity = g_mime_encoder_estimate_output_length (encoder, 3);
		g_test_expect_message (CODEC_LOG_DOMAIN, G_LOG_LEVEL_CRITICAL, "*output_capacity >= estimate*");
		g_assert_cmpuint (g_mime_encoder_flush (encoder, input + 1, 3, a, capacity - 1), ==, 0);
		g_test_assert_expected_messages ();
		g_test_expect_message (CODEC_LOG_DOMAIN, G_LOG_LEVEL_CRITICAL, "*input != NULL*");
		g_mime_encoder_encode (encoder, NULL, 1, a, sizeof (a));
		g_test_assert_expected_messages ();
		g_test_expect_message (CODEC_LOG_DOMAIN, G_LOG_LEVEL_CRITICAL, "*output != NULL*");
		g_mime_encoder_flush (encoder, input + 1, 3, NULL, sizeof (a));
		g_test_assert_expected_messages ();
		g_test_expect_message (CODEC_LOG_DOMAIN, G_LOG_LEVEL_CRITICAL, "*estimate != G_MAXSIZE*");
		g_mime_encoder_encode (encoder, input, G_MAXSIZE, a, sizeof (a));
		g_test_assert_expected_messages ();
		n = g_mime_encoder_flush (encoder, input + 1, 3, a, sizeof (a));
		m = g_mime_encoder_flush (enc_copy, input + 1, 3, b, sizeof (b));
		g_assert_cmpmem (a, n, b, m);
		if (kind == 4)
			g_assert_cmphex (g_mime_yenc_encoder_get_checksum (GMIME_YENC_ENCODER (encoder)), ==,
					 g_mime_yenc_encoder_get_checksum (GMIME_YENC_ENCODER (enc_copy)));
		if (kind != 3)
			g_assert_cmpuint (g_mime_encoder_estimate_output_length (encoder, G_MAXSIZE), ==, G_MAXSIZE);
		g_object_unref (enc_copy);
		g_object_unref (encoder);

		decoder = new_decoder (kind);
		g_mime_decoder_decode (decoder, input + (kind == 1), 1, scratch, sizeof (scratch));
		dec_copy = g_mime_decoder_clone (decoder);
		capacity = g_mime_decoder_estimate_output_length (decoder, 3);
		g_test_expect_message (CODEC_LOG_DOMAIN, G_LOG_LEVEL_CRITICAL, "*output_capacity >= estimate*");
		g_assert_cmpuint (g_mime_decoder_decode (decoder, input + 1, 3, a, capacity - 1), ==, 0);
		g_test_assert_expected_messages ();
		g_test_expect_message (CODEC_LOG_DOMAIN, G_LOG_LEVEL_CRITICAL, "*input != NULL*");
		g_mime_decoder_decode (decoder, NULL, 1, a, sizeof (a));
		g_test_assert_expected_messages ();
		g_test_expect_message (CODEC_LOG_DOMAIN, G_LOG_LEVEL_CRITICAL, "*output != NULL*");
		g_mime_decoder_decode (decoder, input + 1, 3, NULL, sizeof (a));
		g_test_assert_expected_messages ();
		n = g_mime_decoder_decode (decoder, input + 1, 3, a, sizeof (a));
		m = g_mime_decoder_decode (dec_copy, input + 1, 3, b, sizeof (b));
		g_assert_cmpmem (a, n, b, m);
		if (kind == 4)
			g_assert_cmphex (g_mime_yenc_decoder_get_checksum (GMIME_YENC_DECODER (decoder)), ==,
					 g_mime_yenc_decoder_get_checksum (GMIME_YENC_DECODER (dec_copy)));
		g_object_unref (dec_copy);
		g_object_unref (decoder);
	}
}

static void
test_empty_operations (void)
{
	GMimeEncoder *encoder;
	GMimeDecoder *decoder;
	guint8 output[128];
	gsize n;
	guint kind, i;

	for (kind = 0; kind < 5; kind++) {
		encoder = new_encoder (kind);
		decoder = new_decoder (kind);
		for (i = 0; i < 3; i++) {
			memset (output, 0xa5, sizeof (output));
			g_assert_cmpuint (g_mime_encoder_encode (encoder, NULL, 0, output, sizeof (output)), ==, 0);
			n = g_mime_encoder_flush (encoder, NULL, 0, output, sizeof (output));
			g_assert_cmpuint (n, ==, kind == 2 ? 2 : 0);
			if (kind == 2)
				g_assert_cmpmem (output, n, "`\n", 2);
			g_assert_cmpuint (output[n], ==, 0xa5);
			g_assert_cmpuint (g_mime_decoder_decode (decoder, NULL, 0, output, sizeof (output)), ==, 0);
		}
		if (kind == 3) {
			g_assert_cmpuint (g_mime_encoder_encode (encoder, NULL, 0, NULL, 0), ==, 0);
			g_assert_cmpuint (g_mime_encoder_flush (encoder, NULL, 0, NULL, 0), ==, 0);
			g_assert_cmpuint (g_mime_decoder_decode (decoder, NULL, 0, NULL, 0), ==, 0);
		}
		if (kind == 4)
			g_assert_cmpuint (g_mime_decoder_decode (decoder, NULL, 0, NULL, 0), ==, 0);
		g_object_unref (encoder);
		g_object_unref (decoder);
	}
}

static void
test_constructors (void)
{
	GMimeContentEncoding encoding;
	GMimeEncoder *encoder, *enc_copy;
	GMimeDecoder *decoder, *dec_copy;
	const guint8 input[] = "TWFu";
	guint8 output[16];
	guint length, i;

	for (i = 0; i < 2; i++) {
		length = i ? 999 : 59;
		g_test_expect_message (CODEC_LOG_DOMAIN, G_LOG_LEVEL_CRITICAL, "*max_line_length >= 60*");
		g_assert_null (g_mime_base64_encoder_new_with_max_line_length (length));
		g_test_assert_expected_messages ();
		g_test_expect_message (CODEC_LOG_DOMAIN, G_LOG_LEVEL_CRITICAL, "*max_line_length >= 60*");
		g_assert_null (g_mime_quoted_printable_encoder_new_with_max_line_length (length));
		g_test_assert_expected_messages ();
		g_test_expect_message (CODEC_LOG_DOMAIN, G_LOG_LEVEL_CRITICAL, "*max_line_length >= 60*");
		g_assert_null (g_mime_yenc_encoder_new_with_max_line_length (length));
		g_test_assert_expected_messages ();
	}
	for (encoding = GMIME_CONTENT_ENCODING_UNSPECIFIED; encoding <= GMIME_CONTENT_ENCODING_UUENCODE; encoding++) {
		encoder = GMIME_ENCODER (g_mime_pass_through_encoder_new (encoding));
		decoder = GMIME_DECODER (g_mime_pass_through_decoder_new (encoding));
		g_assert_cmpint (g_mime_encoder_get_encoding (encoder), ==, encoding);
		g_assert_cmpint (g_mime_decoder_get_encoding (decoder), ==, encoding);
		g_assert_cmpuint (g_mime_encoder_flush (encoder, input, 4, output, sizeof (output)), ==, 4);
		g_assert_cmpmem (output, 4, input, 4);
		g_assert_cmpuint (g_mime_decoder_decode (decoder, input, 4, output, sizeof (output)), ==, 4);
		g_assert_cmpmem (output, 4, input, 4);
		enc_copy = g_mime_encoder_clone (encoder);
		dec_copy = g_mime_decoder_clone (decoder);
		g_mime_encoder_reset (enc_copy);
		g_mime_decoder_reset (dec_copy);
		g_assert_cmpint (g_mime_encoder_get_encoding (enc_copy), ==, encoding);
		g_assert_cmpint (g_mime_decoder_get_encoding (dec_copy), ==, encoding);
		g_assert_cmpuint (g_mime_encoder_flush (enc_copy, input, 4, output, sizeof (output)), ==, 4);
		g_assert_cmpmem (output, 4, input, 4);
		g_assert_cmpuint (g_mime_decoder_decode (dec_copy, input, 4, output, sizeof (output)), ==, 4);
		g_assert_cmpmem (output, 4, input, 4);
		g_object_unref (enc_copy);
		g_object_unref (dec_copy);
		g_object_unref (encoder);
		g_object_unref (decoder);
	}
}

static void
test_decoder_mode_clones (void)
{
	static const struct {
		const char *input;
		const char *expected;
	} cases[] = {
		{ "_=41=5F=\r\n_", " A_ " },
		{ "begin 644 x\n#0V%T\n`\nend\n", "Cat" }
	};
	GMimeDecoder *decoder, *copy;
	GByteArray *decoded, *original;
	guint8 prefix_output[64];
	gsize prefix, length, written;
	guint i;

	for (i = 0; i < G_N_ELEMENTS (cases); i++) {
		decoder = i ? GMIME_DECODER (g_mime_uu_decoder_new (FALSE)) :
			GMIME_DECODER (g_mime_quoted_printable_decoder_new (TRUE));
		length = strlen (cases[i].input);
		for (prefix = 0; prefix <= length; prefix++) {
			g_mime_decoder_reset (decoder);
			written = g_mime_decoder_decode (decoder, (const guint8 *) cases[i].input, prefix,
							prefix_output, sizeof (prefix_output));
			g_assert_cmpuint (written, <=, strlen (cases[i].expected));
			copy = g_mime_decoder_clone (decoder);
			decoded = decode_chunks (copy, (const guint8 *) cases[i].input + prefix, length - prefix, 1);
			g_assert_cmpmem (prefix_output, written, cases[i].expected, written);
			g_assert_cmpmem (decoded->data, decoded->len, cases[i].expected + written,
					strlen (cases[i].expected) - written);
			g_mime_decoder_reset (copy);
			g_byte_array_unref (decoded);
			decoded = decode_chunks (copy, (const guint8 *) cases[i].input, length, 2);
			g_assert_cmpmem (decoded->data, decoded->len, cases[i].expected, strlen (cases[i].expected));
			original = decode_chunks (decoder, (const guint8 *) cases[i].input + prefix, length - prefix, 1);
			g_assert_cmpmem (original->data, original->len, cases[i].expected + written,
					strlen (cases[i].expected) - written);
			g_byte_array_unref (decoded);
			g_byte_array_unref (original);
			g_object_unref (copy);
		}
		g_object_unref (decoder);
	}
}

static void
check_decode (GMimeDecoder *decoder, const char *input, const char *expected)
{
	GByteArray *decoded;
	guint chunk;
	for (chunk = 1; chunk <= strlen (input) + 1; chunk++) {
		g_mime_decoder_reset (decoder);
		decoded = decode_chunks (decoder, (const guint8 *) input, strlen (input), chunk);
		g_assert_cmpmem (decoded->data, decoded->len, expected, strlen (expected));
		g_byte_array_unref (decoded);
	}
}

static void
test_recovery_modes (void)
{
	GMimeDecoder *decoder;
	GMimeEncoder *encoder;
	GByteArray *encoded;

	decoder = GMIME_DECODER (g_mime_base64_decoder_new ());
	check_decode (decoder, "T!W@F#u==TQ==T", "Man");
	check_decode (decoder, "TQ==TWE=TWFu", "MMaMan");
	check_decode (decoder, "====TWFu", "Man");
	check_decode (decoder, "T==uTWFu", "Man");
	g_object_unref (decoder);
	decoder = GMIME_DECODER (g_mime_quoted_printable_decoder_new (FALSE));
	check_decode (decoder, "_=41==\r\n=QZ=4X=\rX=", "_A==QZ=4X=\rX");
	g_object_unref (decoder);
	decoder = GMIME_DECODER (g_mime_quoted_printable_decoder_new (TRUE));
	check_decode (decoder, "_=5F=20_=Q_", " _  =Q ");
	g_object_unref (decoder);
	decoder = GMIME_DECODER (g_mime_uu_decoder_new (FALSE));
	check_decode (decoder, "not begin 644 x\nbegX\nbegin 644 x\n#0V%T\n`\nend\n#0V%T\n", "Cat");
	g_object_unref (decoder);
	encoder = GMIME_ENCODER (g_mime_quoted_printable_encoder_new ());
	encoded = encode_chunks (encoder, (const guint8 *) "a \r\nb\t\rc", 8, 1);
	g_assert_cmpmem (encoded->data, encoded->len, "a=20\nb\t=0Dc=\n", 13);
	g_byte_array_unref (encoded);
	g_object_unref (encoder);
}

static void
check_base64_mime_tail (const guint8 *input, gsize length)
{
	static const gsize chunks[] = { 31, 32, 33, 77, 78, 100, G_MAXSIZE };
	GMimeDecoder *decoder = new_decoder (0), *inplace_decoder = new_decoder (0);
	GByteArray *expected = decode_chunks (decoder, input, length, 1);
	guint8 output[512], inplace[512];
	gsize offset, total, count, capacity, written, n;
	guint i;

	for (i = 0; i < G_N_ELEMENTS (chunks); i++) {
		g_mime_decoder_reset (decoder);
		g_mime_decoder_reset (inplace_decoder);
		total = 0;
		for (offset = 0; offset < length; offset += count) {
			count = MIN (chunks[i], length - offset);
			capacity = g_mime_decoder_estimate_output_length (decoder, count);
			g_assert_cmpuint (MAX (count, capacity) + 1, <=, sizeof (output));
			memset (output, 0xa5, sizeof (output));
			memset (inplace, 0xa5, sizeof (inplace));
			memcpy (inplace, input + offset, count);
			written = g_mime_decoder_decode (decoder, input + offset, count, output, capacity);
			n = g_mime_decoder_decode (inplace_decoder, inplace, count, inplace, capacity);
			g_assert_cmpuint (total + written, <=, expected->len);
			g_assert_cmpmem (output, written, expected->data + total, written);
			g_assert_cmpmem (inplace, n, output, written);
			assert_padding (output, written, capacity);
			if (written < count)
				g_assert_cmpmem (inplace + written, count - written, input + offset + written, count - written);
			assert_padding (inplace, MAX (count, written), MAX (count, capacity));
			total += written;
		}
		g_assert_cmpuint (total, ==, expected->len);
	}

	g_byte_array_unref (expected);
	g_object_unref (inplace_decoder);
	g_object_unref (decoder);
}

static void
test_base64_mime_tails (void)
{
	guint8 input[228], encoded[512];
	GMimeEncoder *encoder;
	GByteArray *original;
	guint i, format, value, position, start;

	for (i = 0; i < sizeof (input); i++)
		input[i] = i * 31;
	for (format = 0; format < 2; format++) {
		encoder = new_encoder_with_newline_format (0, format, 76);
		original = encode_chunks (encoder, input, sizeof (input), sizeof (input));
		g_object_unref (encoder);
		g_byte_array_append (original, (const guint8 *) "TQ==TWE=TWFu", 12);
		for (value = 0; value < 256; value++) {
			for (position = 64; position < 77 + format; position++) {
				memcpy (encoded, original->data, original->len);
				encoded[position] = value;
				/* Also begin directly at a tail, without preceding vector blocks. */
				for (start = 0; start <= 64; start += 64)
					check_base64_mime_tail (encoded + start, original->len - start);
			}
		}
		g_byte_array_unref (original);
	}
}

static void
test_overlap (void)
{
	guint8 original[129], output[2048], scratch[512];
	GMimeEncoder *encoder;
	GMimeDecoder *decoder;
	GByteArray *expected;
	guint kind, i;
	gsize n;

	for (i = 0; i < sizeof (original); i++)
		original[i] = i * 31;
	for (kind = 0; kind < 5; kind++) {
		encoder = new_encoder (kind);
		expected = encode_chunks (encoder, original, sizeof (original), 129);
		memcpy (output + 1, original, sizeof (original));
		n = g_mime_encoder_flush (encoder, output + 1, 129, output, sizeof (output));
		g_assert_cmpmem (output, n, expected->data, expected->len);
		decoder = new_decoder (kind);
		/* Pending input can make in-place decoding expand the first quartet. */
		g_mime_decoder_decode (decoder, output, 1, scratch, sizeof (scratch));
		memmove (output, output + 1, expected->len - 1);
		n = g_mime_decoder_decode (decoder, output, expected->len - 1, output, sizeof (output));
		if (kind < 3)
			g_assert_cmpmem (output, n, original, sizeof (original));
		else
			g_assert_cmpmem (output, n, original + 1, sizeof (original) - 1);
		g_byte_array_unref (expected);
		g_object_unref (decoder);
		g_object_unref (encoder);
	}
}

static void
test_line_lengths (void)
{
	guint8 input[512], output[2048];
	GMimeEncoder *encoder, *copy;
	guint kind, length, i, column;
	gsize n;

	memset (input, 0xff, sizeof (input));
	for (kind = 0; kind < 3; kind++) {
		if (kind == 2) {
			for (i = 0; i < sizeof (input); i++)
				input[i] = i % 3 ? 23 : 214;
		}
		for (length = 60; length <= 998; length++) {
			encoder = kind == 0 ?
				GMIME_ENCODER (g_mime_base64_encoder_new_with_max_line_length (length)) :
				kind == 1 ? GMIME_ENCODER (g_mime_quoted_printable_encoder_new_with_max_line_length (length)) :
				GMIME_ENCODER (g_mime_yenc_encoder_new_with_max_line_length (length));
			copy = g_mime_encoder_clone (encoder);
			g_object_unref (encoder);
			n = g_mime_encoder_flush (copy, input, sizeof (input), output, sizeof (output));
			column = 0;
			for (i = 0; i < n; i++) {
				if (output[i] == '\n')
					column = 0;
				else
					g_assert_cmpuint (++column, <=, kind == 2 ? length + 1 : MIN (length, 76));
			}
			g_object_unref (copy);
		}
	}
}

static guint32
reference_crc32 (const guint8 *input, gsize length)
{
	guint32 crc = G_MAXUINT32;
	gsize i;
	guint bit;

	for (i = 0; i < length; i++) {
		crc ^= input[i];
		for (bit = 0; bit < 8; bit++)
			crc = (crc >> 1) ^ ((crc & 1) ? 0xedb88320u : 0);
	}
	return crc;
}

static GByteArray *
convert_encoded_newlines (GByteArray *input, GMimeNewLineFormat newline_format)
{
	GByteArray *result = g_byte_array_new ();
	guint i;

	for (i = 0; i < input->len; i++) {
		g_assert_cmpuint (input->data[i], !=, '\r');
		if (input->data[i] == '\n' && newline_format == GMIME_NEWLINE_FORMAT_DOS)
			g_byte_array_append (result, (const guint8 *) "\r", 1);
		g_byte_array_append (result, input->data + i, 1);
	}
	return result;
}

static void
test_newline_vectors (void)
{
	static const char *expected[2][3] = {
		{ "TWFu\n", "Man=\n", "#36%N\n`\n" },
		{ "TWFu\r\n", "Man=\r\n", "#36%N\r\n`\r\n" }
	};
	static const char *qp_input[] = { "\r\n\n\r", "x \r\nx\t\n", "\r\r\n", " \t" };
	static const char *qp_expected[2][4] = {
		{ "\n\n=0D=\n", "x=20\nx=09\n", "=0D\n", " =09=\n" },
		{ "\r\n\r\n=0D=\r\n", "x=20\r\nx=09\r\n", "=0D\r\n", " =09=\r\n" }
	};
	static const gsize estimates[2][3] = { { 78, 2, 64 }, { 80, 3, 66 } };
	GMimeEncoder *encoder;
	GByteArray *result;
	guint kind, format, i;

	for (format = 0; format < 2; format++) {
		for (kind = 0; kind < 3; kind++) {
			encoder = new_encoder_with_newline_format (kind, format, 76);
			g_assert_cmpuint (g_mime_encoder_estimate_output_length (encoder, 0), ==, estimates[format][kind]);
			g_assert_cmpuint (g_mime_encoder_estimate_output_length (encoder, G_MAXSIZE), ==, G_MAXSIZE);
			result = encode_chunks (encoder, (const guint8 *) "Man", 3, 1);
			g_assert_cmpmem (result->data, result->len, expected[format][kind], strlen (expected[format][kind]));
			g_byte_array_unref (result);
			g_object_unref (encoder);
		}
		encoder = new_encoder_with_newline_format (1, format, 76);
		for (i = 0; i < G_N_ELEMENTS (qp_input); i++) {
			result = encode_chunks (encoder, (const guint8 *) qp_input[i], strlen (qp_input[i]), 1);
			g_assert_cmpmem (result->data, result->len, qp_expected[format][i], strlen (qp_expected[format][i]));
			g_byte_array_unref (result);
		}
		g_object_unref (encoder);
	}
}

static void
test_newline_chunks (void)
{
	static const guint lengths[] = { 60, 61, 62, 63, 64, 73, 74, 75, 76, 77, 998 };
	static const guint chunks[] = { 1, 2, 3, 4, 15, 16, 17, 31, 44, 45, 46, 56, 57, 58, 76, 129, 4096 };
	guint8 input[2049];
	GMimeEncoder *encoder, *unix_encoder;
	GByteArray *unix_result, *expected, *result;
	guint kind, format, i, j, k, column, limit;

	for (i = 0; i < sizeof (input); i++)
		input[i] = i * 31;
	memcpy (input + 70, " \r\n\t\n\r\r\n", 8);
	for (kind = 0; kind < 3; kind++) {
		for (i = 0; i < G_N_ELEMENTS (lengths); i++) {
			unix_encoder = new_encoder_with_newline_format (kind, GMIME_NEWLINE_FORMAT_UNIX, lengths[i]);
			unix_result = encode_chunks (unix_encoder, input, sizeof (input), sizeof (input));
			g_object_unref (unix_encoder);
			if (lengths[i] == 76) {
				encoder = new_encoder (kind);
				result = encode_chunks (encoder, input, sizeof (input), sizeof (input));
				g_assert_cmpmem (result->data, result->len, unix_result->data, unix_result->len);
				g_byte_array_unref (result);
				g_object_unref (encoder);
			}
			for (format = 0; format < 2; format++) {
				expected = convert_encoded_newlines (unix_result, format);
				encoder = new_encoder_with_newline_format (kind, format, lengths[i]);
				limit = kind == 2 ? 61 : kind == 0 ? MIN (lengths[i], 76) / 4 * 4 : MIN (lengths[i], 76);
				for (j = 0; j < G_N_ELEMENTS (chunks); j++) {
					result = encode_chunks (encoder, input, sizeof (input), chunks[j]);
					g_assert_cmpmem (result->data, result->len, expected->data, expected->len);
					column = 0;
					for (k = 0; k < result->len; k++) {
						if (result->data[k] == '\r') {
							g_assert_cmpuint (format, ==, GMIME_NEWLINE_FORMAT_DOS);
							g_assert_cmpuint (k + 1, <, result->len);
							g_assert_cmpuint (result->data[k + 1], ==, '\n');
						} else if (result->data[k] == '\n') {
							g_assert_true (format == GMIME_NEWLINE_FORMAT_UNIX || (k && result->data[k - 1] == '\r'));
							column = 0;
						} else {
							g_assert_cmpuint (++column, <=, limit);
						}
					}
					g_byte_array_unref (result);
				}
				g_object_unref (encoder);
				g_byte_array_unref (expected);
			}
			g_byte_array_unref (unix_result);
		}
	}
}

static void
test_newline_clone_reset (void)
{
	guint8 input[129], scratch[1024], a[1024], b[1024];
	GMimeEncoder *encoder, *copy;
	GByteArray *expected;
	guint kind, format, limit, i;
	gsize prefix, n, m, capacity, written;

	for (i = 0; i < sizeof (input); i++)
		input[i] = i * 31;
	for (kind = 0; kind < 4; kind++) {
		for (format = 0; format < 2; format++) {
			for (limit = 60; limit <= 76; limit += 16) {
				encoder = new_encoder_with_newline_format (kind, format, limit);
				expected = encode_chunks (encoder, input, sizeof (input), 1);
				for (prefix = 0; prefix <= sizeof (input); prefix++) {
					n = g_mime_encoder_encode (encoder, input, prefix, scratch, sizeof (scratch));
					copy = g_mime_encoder_clone (encoder);
					capacity = g_mime_encoder_estimate_output_length (encoder, sizeof (input) - prefix);
					g_assert_cmpuint (capacity, ==, g_mime_encoder_estimate_output_length (copy, sizeof (input) - prefix));
					memset (a, 0xa5, sizeof (a));
					memset (b, 0xa5, sizeof (b));
					written = g_mime_encoder_flush (encoder, input + prefix, sizeof (input) - prefix, a, capacity);
					m = g_mime_encoder_flush (copy, input + prefix, sizeof (input) - prefix, b, capacity);
					assert_padding (a, written, capacity);
					assert_padding (b, m, capacity);
					g_assert_cmpmem (a, written, b, m);
					g_assert_cmpmem (scratch, n, expected->data, n);
					g_assert_cmpmem (a, written, expected->data + n, expected->len - n);
					g_mime_encoder_encode (copy, input, prefix, scratch, sizeof (scratch));
					g_mime_encoder_reset (copy);
					m = g_mime_encoder_flush (copy, input, sizeof (input), b, sizeof (b));
					g_assert_cmpmem (b, m, expected->data, expected->len);
					g_object_unref (copy);
				}
				g_byte_array_unref (expected);
				g_object_unref (encoder);
			}
		}
	}
}

static void
test_newline_preconditions (void)
{
	static const guint lengths[] = { 0, 59, 999, G_MAXUINT };
	static const GMimeNewLineFormat formats[] = { (GMimeNewLineFormat) -1, (GMimeNewLineFormat) 2 };
	guint kind, format, i;

	for (kind = 0; kind < 3; kind++) {
		for (i = 0; i < G_N_ELEMENTS (formats); i++) {
			g_test_expect_message (CODEC_LOG_DOMAIN, G_LOG_LEVEL_CRITICAL, "*newline_format ==*");
			g_assert_null (new_encoder_with_newline_format (kind, formats[i], 76));
			g_test_assert_expected_messages ();
		}
		if (kind == 2)
			continue;
		for (format = 0; format < 2; format++) {
			for (i = 0; i < G_N_ELEMENTS (lengths); i++) {
				g_test_expect_message (CODEC_LOG_DOMAIN, G_LOG_LEVEL_CRITICAL, "*max_line_length >= 60*");
				g_assert_null (new_encoder_with_newline_format (kind, format, lengths[i]));
				g_test_assert_expected_messages ();
			}
		}
	}
}

static void
test_yenc_checksum (void)
{
	const guint8 vector[] = "123456789";
	const guint8 special[] = { 214, 224, 227, 19, 4, 223, 23 };
	guint8 input[256], output[1024], scratch[1024], a[1024], b[1024];
	GMimeEncoder *encoder, *enc_copy;
	GMimeDecoder *decoder, *dec_copy;
	gsize prefix, n, m;
	guint i;

	encoder = GMIME_ENCODER (g_mime_yenc_encoder_new ());
	decoder = GMIME_DECODER (g_mime_yenc_decoder_new (TRUE));
	g_assert_cmpstr (G_OBJECT_TYPE_NAME (encoder), ==, "GMimeYencEncoder");
	g_assert_cmpstr (G_OBJECT_TYPE_NAME (decoder), ==, "GMimeYencDecoder");
	g_assert_cmpint (g_mime_encoder_get_encoding (encoder), ==, GMIME_CONTENT_ENCODING_UNSPECIFIED);
	g_assert_cmpint (g_mime_decoder_get_encoding (decoder), ==, GMIME_CONTENT_ENCODING_UNSPECIFIED);
	g_assert_cmphex (g_mime_yenc_encoder_get_checksum (GMIME_YENC_ENCODER (encoder)), ==, G_MAXUINT32);
	g_assert_cmphex (g_mime_yenc_decoder_get_checksum (GMIME_YENC_DECODER (decoder)), ==, G_MAXUINT32);
	n = g_mime_encoder_flush (encoder, vector, 9, output, sizeof (output));
	g_assert_cmphex (g_mime_yenc_encoder_get_checksum (GMIME_YENC_ENCODER (encoder)), ==, 0x340bc6d9);
	m = g_mime_decoder_decode (decoder, output, n, scratch, sizeof (scratch));
	g_assert_cmpmem (scratch, m, vector, 9);
	g_assert_cmphex (g_mime_yenc_decoder_get_checksum (GMIME_YENC_DECODER (decoder)), ==, 0x340bc6d9);
	g_assert_cmpuint (g_mime_encoder_flush (encoder, NULL, 0, output, sizeof (output)), ==, 0);
	g_assert_cmphex (g_mime_yenc_encoder_get_checksum (GMIME_YENC_ENCODER (encoder)), ==, 0x340bc6d9);
	memcpy (scratch, vector, 9);
	memcpy (scratch + 9, vector, 9);
	n = g_mime_encoder_flush (encoder, vector, 9, output, sizeof (output));
	g_assert_cmphex (g_mime_yenc_encoder_get_checksum (GMIME_YENC_ENCODER (encoder)), ==,
			 reference_crc32 (scratch, 18));
	m = g_mime_decoder_decode (decoder, output, n, a, sizeof (a));
	g_assert_cmpmem (a, m, vector, 9);
	g_assert_cmphex (g_mime_yenc_decoder_get_checksum (GMIME_YENC_DECODER (decoder)), ==,
			 reference_crc32 (scratch, 18));

	for (i = 0; i < sizeof (input); i++)
		input[i] = i;
	for (prefix = 0; prefix <= sizeof (input); prefix++) {
		g_mime_encoder_reset (encoder);
		g_assert_cmphex (g_mime_yenc_encoder_get_checksum (GMIME_YENC_ENCODER (encoder)), ==, G_MAXUINT32);
		n = g_mime_encoder_encode (encoder, input, prefix, output, sizeof (output));
		g_assert_cmphex (g_mime_yenc_encoder_get_checksum (GMIME_YENC_ENCODER (encoder)), ==,
				 reference_crc32 (input, prefix));
		enc_copy = g_mime_encoder_clone (encoder);
		g_assert_cmphex (g_mime_yenc_encoder_get_checksum (GMIME_YENC_ENCODER (enc_copy)), ==,
				 reference_crc32 (input, prefix));
		n += g_mime_encoder_flush (encoder, input + prefix, sizeof (input) - prefix,
					  output + n, sizeof (output) - n);
		g_mime_encoder_flush (enc_copy, input + prefix, sizeof (input) - prefix, a, sizeof (a));
		g_assert_cmphex (g_mime_yenc_encoder_get_checksum (GMIME_YENC_ENCODER (encoder)), ==,
				 reference_crc32 (input, sizeof (input)));
		g_assert_cmphex (g_mime_yenc_encoder_get_checksum (GMIME_YENC_ENCODER (enc_copy)), ==,
				 reference_crc32 (input, sizeof (input)));
		g_object_unref (enc_copy);

		g_mime_decoder_reset (decoder);
		g_assert_cmphex (g_mime_yenc_decoder_get_checksum (GMIME_YENC_DECODER (decoder)), ==, G_MAXUINT32);
		m = g_mime_decoder_decode (decoder, output, prefix, scratch, sizeof (scratch));
		g_assert_cmphex (g_mime_yenc_decoder_get_checksum (GMIME_YENC_DECODER (decoder)), ==,
				 reference_crc32 (scratch, m));
		dec_copy = g_mime_decoder_clone (decoder);
		m = g_mime_decoder_decode (decoder, output + prefix, n - prefix, a, sizeof (a));
		n = g_mime_decoder_decode (dec_copy, output + prefix, n - prefix, b, sizeof (b));
		g_assert_cmpmem (a, m, b, n);
		g_assert_cmphex (g_mime_yenc_decoder_get_checksum (GMIME_YENC_DECODER (decoder)), ==,
				 reference_crc32 (input, sizeof (input)));
		g_assert_cmphex (g_mime_yenc_decoder_get_checksum (GMIME_YENC_DECODER (dec_copy)), ==,
				 reference_crc32 (input, sizeof (input)));
		g_object_unref (dec_copy);
	}
	g_mime_encoder_reset (encoder);
	n = g_mime_encoder_flush (encoder, special, sizeof (special), output, sizeof (output));
	g_assert_cmpmem (output, n, "=@=J=M=}=n=IA\n", 14);
	g_object_unref (encoder);
	g_object_unref (decoder);
}

static void
test_yenc_markers (void)
{
	static const char *cases[] = {
		"not =ybegin x\n=ybeginX\n=ybegin line=128 size=3 name=x\r\nklm\r\n=yend size=3\nklm\n",
		"=ybegin x\n=ypart begin=1 end=3\r\nklm\r\n=yend\n",
		"=ybegin x\n=yxignored\n=ypxignored\n=ypaxignored\n=yparxignored\n=ypartxignored\nklm\n=yend\n",
		"=ybegin x\n=xklm\n=yend\n",
		"=ybegin x\n=ye",
		"=ybegin x\n=ypart x\n"
	};
	static const char *expected[] = { "ABC", "ABC", "ABC", "NABC", "", "" };
	GMimeDecoder *decoder = GMIME_DECODER (g_mime_yenc_decoder_new (FALSE)), *copy;
	guint8 scratch[512], a[512], b[512];
	gsize prefix, n, m, length;
	guint i;

	for (i = 0; i < G_N_ELEMENTS (cases); i++) {
		check_decode (decoder, cases[i], expected[i]);
		length = strlen (cases[i]);
		for (prefix = 0; prefix <= length; prefix++) {
			g_mime_decoder_reset (decoder);
			g_mime_decoder_decode (decoder, (const guint8 *) cases[i], prefix, scratch, sizeof (scratch));
			copy = g_mime_decoder_clone (decoder);
			n = g_mime_decoder_decode (decoder, (const guint8 *) cases[i] + prefix, length - prefix, a, sizeof (a));
			m = g_mime_decoder_decode (copy, (const guint8 *) cases[i] + prefix, length - prefix, b, sizeof (b));
			g_assert_cmpmem (a, n, b, m);
			g_assert_cmphex (g_mime_yenc_decoder_get_checksum (GMIME_YENC_DECODER (decoder)), ==,
					 g_mime_yenc_decoder_get_checksum (GMIME_YENC_DECODER (copy)));
			g_object_unref (copy);
		}
	}
	g_object_unref (decoder);
	decoder = GMIME_DECODER (g_mime_yenc_decoder_new (TRUE));
	check_decode (decoder, "=\rklm\n=\nklm\n=yend\nklm", "ABCABC");
	check_decode (decoder, "klm=yklm", "ABC\017ABC");
	g_object_unref (decoder);
}

static void
test_yenc_estimates (void)
{
	static const guint lengths[] = { 60, 61, 128, 253, 254, 255, 256, 998 };
	guint8 input[2996], prefix[998], *output;
	GMimeEncoder *encoder;
	gsize length, capacity, written;
	guint i, j;

	memset (input, 214, sizeof (input));
	memset (prefix, 23, sizeof (prefix));
	for (i = 0; i < G_N_ELEMENTS (lengths); i++) {
		encoder = GMIME_ENCODER (g_mime_yenc_encoder_new_with_max_line_length (lengths[i]));
		for (length = 0; length <= lengths[i] * 3 + 1; length++) {
			for (j = 0; j < 2; j++) {
				g_mime_encoder_reset (encoder);
				if (j) {
					capacity = g_mime_encoder_estimate_output_length (encoder, lengths[i] - 1);
					output = g_malloc (capacity);
					g_mime_encoder_encode (encoder, prefix, lengths[i] - 1, output, capacity);
					g_free (output);
				}
				capacity = g_mime_encoder_estimate_output_length (encoder, length);
				output = g_malloc (capacity + 1);
				memset (output, 0xa5, capacity + 1);
				written = g_mime_encoder_flush (encoder, input, length, output, capacity);
				assert_padding (output, written, capacity);
				g_free (output);
			}
		}
		g_assert_cmpuint (g_mime_encoder_estimate_output_length (encoder, G_MAXSIZE / 2), ==, G_MAXSIZE);
		g_object_unref (encoder);
	}
}

static void
test_guard_pages (void)
{
#ifdef G_OS_UNIX
	gsize page = sysconf (_SC_PAGESIZE), length, capacity, n;
	guint8 *inmap, *outmap, *input, *output;
	GMimeEncoder *encoder;
	GMimeDecoder *decoder;
	guint kind, layout, i;

	inmap = mmap (NULL, page * 3, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
	outmap = mmap (NULL, page * 3, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
	g_assert_true (inmap != MAP_FAILED && outmap != MAP_FAILED);
	g_assert_cmpint (mprotect (inmap + page, page, PROT_READ | PROT_WRITE), ==, 0);
	g_assert_cmpint (mprotect (outmap + page, page, PROT_READ | PROT_WRITE), ==, 0);
	for (kind = 0; kind < 5; kind++) {
		encoder = new_encoder (kind);
		decoder = new_decoder (kind);
		for (length = 0; length < 130; length++) {
			input = inmap + 2 * page - length;
			memset (input, 'A', length);
			capacity = g_mime_encoder_estimate_output_length (encoder, length);
			output = outmap + 2 * page - capacity;
			n = g_mime_encoder_flush (encoder, input, length, output, capacity);
			g_assert_cmpuint (n, <=, capacity);
			g_mime_decoder_reset (decoder);
			capacity = g_mime_decoder_estimate_output_length (decoder, length);
			output = outmap + 2 * page - capacity;
			n = g_mime_decoder_decode (decoder, input, length, output, capacity);
			g_assert_cmpuint (n, <=, capacity);
		}
		g_object_unref (encoder);
		g_object_unref (decoder);
	}
	decoder = new_decoder (0);
	for (layout = 0; layout < 4; layout++) {
		for (length = 0; length < 260; length++) {
			input = inmap + 2 * page - length;
			memset (input, 'A', length);
			for (i = layout < 2 ? 12 : 76; i < length; i += 77 + (layout & 1)) {
				input[i] = layout & 1 ? '\r' : '\n';
				if ((layout & 1) && i + 1 < length)
					input[i + 1] = '\n';
			}
			g_mime_decoder_reset (decoder);
			capacity = g_mime_decoder_estimate_output_length (decoder, length);
			output = outmap + 2 * page - capacity;
			n = g_mime_decoder_decode (decoder, input, length, output, capacity);
			g_assert_cmpuint (n, <=, capacity);
		}
	}
	g_object_unref (decoder);
	g_assert_cmpint (munmap (inmap, page * 3), ==, 0);
	g_assert_cmpint (munmap (outmap, page * 3), ==, 0);
#else
	g_test_skip ("Guard pages require mmap");
#endif
}

static int
benchmark_base64 (void)
{
	static const struct {
		const char *name, *separator;
		gsize line_length, length;
	} layouts[] = {
		{ "unwrapped", "", G_MAXSIZE, 65536 },
		{ "MIME CRLF", "\r\n", 76, 65536 },
		{ "MIME LF", "\n", 76, 65536 },
		{ "short lines", " \t\r\n", 7, 65536 },
		{ "garbage", "!? \t\r\n", 23, 65536 },
		{ "small", "", G_MAXSIZE, 48 }
	};
	const gsize chunk = 4096;
	GMimeDecoder *decoder = new_decoder (0);
	guint8 input[65536], output[3076];
	guint i, j, iteration, iterations;
	gsize offset, count, written, total, length;
	GString *encoded;
	char *base64;
	gint64 start;

	for (i = 0; i < sizeof (input); i++)
		input[i] = i * 31;
	for (i = 0; i < G_N_ELEMENTS (layouts); i++) {
		base64 = g_base64_encode (input, layouts[i].length);
		length = strlen (base64);
		encoded = g_string_new (NULL);
		for (offset = 0; offset < length; offset += count) {
			count = MIN (layouts[i].line_length, length - offset);
			g_string_append_len (encoded, base64 + offset, count);
			g_string_append (encoded, layouts[i].separator);
		}
		g_free (base64);
		g_mime_decoder_reset (decoder);
		total = 0;
		for (offset = 0; offset < encoded->len; offset += count) {
			count = MIN (chunk, encoded->len - offset);
			written = g_mime_decoder_decode (decoder, (guint8 *) encoded->str + offset,
							count, output, sizeof (output));
			g_assert_cmpuint (total + written, <=, layouts[i].length);
			g_assert_cmpmem (output, written, input + total, written);
			total += written;
		}
		g_assert_cmpuint (total, ==, layouts[i].length);
		iterations = layouts[i].length == 48 ? 1000000 : 1024;
		for (j = 0; j < 3; j++) {
			start = g_get_monotonic_time ();
			for (iteration = 0; iteration < iterations; iteration++) {
				g_mime_decoder_reset (decoder);
				total = 0;
				for (offset = 0; offset < encoded->len; offset += count) {
					count = MIN (chunk, encoded->len - offset);
					total += g_mime_decoder_decode (decoder, (guint8 *) encoded->str + offset,
								       count, output, sizeof (output));
				}
				g_assert_cmpuint (total, ==, layouts[i].length);
			}
			g_print ("base64 %-11s streaming: %.2f MiB/s\n", layouts[i].name,
				 (double) layouts[i].length * iterations * 1000000.0 /
				 (1024 * 1024 * (g_get_monotonic_time () - start)));
		}
		g_string_free (encoded, TRUE);
	}
	g_object_unref (decoder);
	return 0;
}

static int
benchmark (void)
{
	const gsize length = 1024 * 1024;
	guint8 *input = g_malloc (length), *encoded, *decoded;
	GMimeEncoder *encoder;
	GMimeDecoder *decoder;
	gint64 start;
	gsize capacity, n;
	guint kind, i, iteration;

	for (i = 0; i < length; i++)
		input[i] = i * 31;
	for (kind = 0; kind < 3; kind += 2) {
		encoder = new_encoder (kind);
		decoder = new_decoder (kind);
		capacity = g_mime_encoder_estimate_output_length (encoder, length);
		encoded = g_malloc (capacity);
		start = g_get_monotonic_time ();
		for (iteration = 0; iteration < 64; iteration++)
			n = g_mime_encoder_flush (encoder, input, length, encoded, capacity);
		g_print ("%s encode: %.2f MiB/s\n", kind ? "uu" : "base64",
			 64000000.0 / (g_get_monotonic_time () - start));
		capacity = g_mime_decoder_estimate_output_length (decoder, n);
		decoded = g_malloc (capacity);
		start = g_get_monotonic_time ();
		for (iteration = 0; iteration < 64; iteration++) {
			g_mime_decoder_reset (decoder);
			g_assert_cmpuint (g_mime_decoder_decode (decoder, encoded, n, decoded, capacity), ==, length);
		}
		g_print ("%s decode: %.2f MiB/s\n", kind ? "uu" : "base64",
			 64000000.0 / (g_get_monotonic_time () - start));
		g_assert_cmpmem (decoded, length, input, length);
		g_free (decoded);
		g_free (encoded);
		g_object_unref (decoder);
		g_object_unref (encoder);
	}
	g_free (input);
	return 0;
}

int
main (int argc, char **argv)
{
	if (argc > 1 && !strcmp (argv[1], "--benchmark-base64"))
		return benchmark_base64 ();
	if (argc > 1 && !strcmp (argv[1], "--benchmark"))
		return benchmark ();
	g_test_init (&argc, &argv, NULL);
	g_test_add_func ("/codecs/roundtrip", test_roundtrip);
	g_test_add_func ("/codecs/clone-reset", test_clone_reset);
	g_test_add_func ("/codecs/decoder-mode-clones", test_decoder_mode_clones);
	g_test_add_func ("/codecs/preconditions", test_preconditions);
	g_test_add_func ("/codecs/empty-operations", test_empty_operations);
	g_test_add_func ("/codecs/constructors", test_constructors);
	g_test_add_func ("/codecs/recovery-modes", test_recovery_modes);
	g_test_add_func ("/codecs/base64-mime-tails", test_base64_mime_tails);
	g_test_add_func ("/codecs/overlap", test_overlap);
	g_test_add_func ("/codecs/line-lengths", test_line_lengths);
	g_test_add_func ("/codecs/newline-vectors", test_newline_vectors);
	g_test_add_func ("/codecs/newline-chunks", test_newline_chunks);
	g_test_add_func ("/codecs/newline-clone-reset", test_newline_clone_reset);
	g_test_add_func ("/codecs/newline-preconditions", test_newline_preconditions);
	g_test_add_func ("/codecs/yenc-checksum", test_yenc_checksum);
	g_test_add_func ("/codecs/yenc-markers", test_yenc_markers);
	g_test_add_func ("/codecs/yenc-estimates", test_yenc_estimates);
	g_test_add_func ("/codecs/guard-pages", test_guard_pages);
	return g_test_run ();
}
